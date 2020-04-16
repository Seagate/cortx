/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 05/08/2011
 */

#include <stdio.h>            /* printf */
#include <unistd.h>           /* pause */
#include <stdlib.h>           /* atoi */
#include <err.h>              /* warnx */
#include <signal.h>           /* sigaction */
#include <sys/time.h>
#include <sys/resource.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"              /* M0_SET0 */
#include "lib/user_space/trace.h"  /* m0_trace_set_buffer_size */

#include "mero/setup.h"
#include "mero/init.h"
#include "mero/version.h"
#include "module/instance.h"  /* m0 */
#include "net/lnet/lnet.h"
#include "reqh/reqh_service.h"
#include "mero/process_attr.h"
#include "ha/note.h"          /* M0_NC_ONLINE */

/**
   @addtogroup m0d
   @{
 */

/**
   Represents various network transports supported
   by a particular node in a cluster.
 */
static struct m0_net_xprt *cs_xprts[] = {
/*
 * TODO: Enable "sock" transport when it's fully integrated.
 *       There many places where "lnet" xprt object is hardcoded. They need to
 *       be fixed first. E.g. implement a similar mechanism that is used here in
 *       m0d - the transport is selected based on a string prefix in the
 *       endpoint address ("lnet:" or "sock:"), see cs_xprt_lookup().
 */
/* #ifdef ENABLE_LUSTRE */
	&m0_net_lnet_xprt /* , */
/* #endif */
	/* &m0_net_sock_xprt */
};

/* Signal handler result */
enum result_status
{
	/* Default value */
	M0_RESULT_STATUS_WORK    = 0,
	/* Stop work Mero instance */
	M0_RESULT_STATUS_STOP    = 1,
	/* Restart Mero instance */
	M0_RESULT_STATUS_RESTART = 2,
};

extern volatile sig_atomic_t gotsignal;
static bool regsignal = false;

/**
   Signal handler registered so that pause()
   returns in order to trigger proper cleanup.
 */
static void cs_term_sig_handler(int signum)
{
	gotsignal = signum == SIGUSR1 ? M0_RESULT_STATUS_RESTART :
					M0_RESULT_STATUS_STOP;
}

/**
   Registers signal handler to catch SIGTERM, SIGINT and
   SIGQUIT signals and pauses the Mero process.
   Registers signal handler to catch SIGUSR1 signals to
   restart the Mero process.
 */
static int cs_register_signal(void)
{
	struct sigaction        term_act;
	int rc;

	regsignal = false;
	gotsignal = M0_RESULT_STATUS_WORK;
	term_act.sa_handler = cs_term_sig_handler;
	sigemptyset(&term_act.sa_mask);
	term_act.sa_flags = 0;

	rc = sigaction(SIGTERM, &term_act, NULL) ?:
		sigaction(SIGINT,  &term_act, NULL) ?:
		sigaction(SIGQUIT, &term_act, NULL) ?:
		sigaction(SIGUSR1, &term_act, NULL);
	if (rc == 0)
		regsignal = true;
	return rc;
}

static int cs_wait_signal(void)
{
	M0_PRE(regsignal);
	printf("Press CTRL+C to quit.\n");
	fflush(stdout);
	do {
		pause();
	} while (!gotsignal);

	return gotsignal;
}

M0_INTERNAL int main(int argc, char **argv)
{
	static struct m0       instance;
	int                    trace_buf_size;
	int                    result;
	int                    rc;
	struct m0_mero         mero_ctx;
	struct rlimit          rlim = {10240, 10240};

	if (argc > 1 &&
	    (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
		m0_build_info_print();
		exit(EXIT_SUCCESS);
	}

	if (argc > 2 && strcmp(argv[1], "--trace-buf-size-mb") == 0) {
		trace_buf_size = atoi(argv[2]); /* in MiB */
		if (trace_buf_size > 0 &&
		    m0_trace_set_buffer_size((size_t)trace_buf_size *
					     1024 * 1024) == 0)
		{
			argv[2] = argv[0];
			argv += 2;
			argc -= 2;
		} else {
			if (trace_buf_size <= 0)
				fprintf(stderr, "mero: trace buffer size should"
					" be greater than zero (was %i)\n",
					trace_buf_size);
			exit(EXIT_FAILURE);
		}
	}

	rc = setrlimit(RLIMIT_NOFILE, &rlim);
	if (rc != 0) {
		warnx("\n Failed to setrlimit\n");
		goto out;
	}

	rc = cs_register_signal();
	if (rc != 0) {
		warnx("\n Failed to register signals\n");
		goto out;
	}

init_m0d:
	gotsignal = M0_RESULT_STATUS_WORK;

	errno = 0;
	M0_SET0(&instance);
	rc = m0_init(&instance);
	if (rc != 0) {
		warnx("\n Failed to initialise Mero \n");
		goto out;
	}

start_m0d:
	M0_SET0(&mero_ctx);
	rc = m0_cs_init(&mero_ctx, cs_xprts, ARRAY_SIZE(cs_xprts), stderr, false);
	if (rc != 0) {
		warnx("\n Failed to initialise Mero \n");
		goto cleanup2;
	}

	/*
	 * Prevent from automatic process fid generation by setting the context
	 * up with a dummy fid of non-process type.
	 */
	mero_ctx.cc_reqh_ctx.rc_fid = M0_FID_INIT(0, 1);
	/*
	 * Process FID specification is mandatory for m0d. Mero instance setup
	 * is going to stumble upon fid type precondition in m0_reqh_init()
	 * unless real process fid is present in argv.
	 */
	rc = m0_cs_setup_env(&mero_ctx, argc, argv);
	if (rc != 0)
		goto cleanup1;

#ifdef HAVE_SYSTEMD
	/*
	 * From the systemd's point of view, service can be considered as
	 * started when it can handle incoming connections, which is already
	 * true before m0_cs_start() is called. otherwise, if sd_notify() is
	 * called after m0_cs_start() it leads to a deadlock, because different
	 * m0d instances will wait for each other forever.
	 */
	rc = sd_notify(0, "READY=1");
	if (rc < 0)
		warnx("systemd READY notification failed, rc=%d\n", rc);
	else if (rc == 0)
		warnx("systemd notifications not allowed\n");
	else
		warnx("systemd READY notification successfull\n");
#endif
	rc = m0_cs_start(&mero_ctx);
	if (rc == 0) {
		/* For st/m0d-signal-test.sh */
		printf("Started\n");
		fflush(stdout);
		result = cs_wait_signal();
		if (gotsignal)
			warnx("got signal %d", gotsignal);
	}

	if (rc == 0 && result == M0_RESULT_STATUS_RESTART) {
		/*
		 * Note! A very common cause of failure restart is
		 * non-finalize (non-clean) any subsystem
		 */
		m0_cs_fini(&mero_ctx);
restart_signal:
		m0_quiesce();

		gotsignal = M0_RESULT_STATUS_WORK;

		rc = m0_cs_memory_limits_setup(&instance);
		if (rc != 0) {
			warnx("\n Failed to set process memory limits Mero \n");
			goto out;
		}

		rc = m0_resume(&instance);
		if (rc != 0) {
			warnx("\n Failed to reconfigure Mero \n");
			goto out;
		}

		/* Print to m0.log for ./sss/st system test*/
		printf("Restarting\n");
		fflush(stdout);

		goto start_m0d;
	}

	if (rc == 0) {
		/* Ignore cleanup labels signal handling for normal start. */
		gotsignal = M0_RESULT_STATUS_WORK;
	}

cleanup1:
	m0_cs_fini(&mero_ctx);
	if (gotsignal) {
		if (gotsignal == M0_RESULT_STATUS_RESTART)
			goto restart_signal;
		gotsignal = M0_RESULT_STATUS_WORK;
	}
cleanup2:
	m0_fini();
	if (gotsignal == M0_RESULT_STATUS_RESTART)
		goto init_m0d;
out:
	errno = rc < 0 ? -rc : rc;
	return errno;
}

/** @} endgroup m0d */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
