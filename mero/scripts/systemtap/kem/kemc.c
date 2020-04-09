/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Malezhin <maxim.malezhin@seagate.com>
 * Original creation date: 5-Aug-2019
 */

/**
 * @addtogroup kem_client User-space application for KEM profiler
 *
 * @{
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>

#include "addb2/addb2.h"
#include "addb2/global.h"
#include "lib/uuid.h"
#include "lib/locality.h"
#include "module/instance.h"

#include "scripts/systemtap/kem/kem.h"
#include "scripts/systemtap/kem/kem_id.h"

enum {
	FILENAME_SIZE = 256,
	KEMD_FILENAME_SIZE = 256,
	ADDB2_STOB_NAME_SIZE = 256,
	ADDB2_STOB_SIZE = 4 * 1024 * 1024 * 1024ull,
};

const char *stob_location = "linuxstob:./_kemc";
const char *kemd_dev_path = "/dev/";

struct kemc_ctx {
	unsigned int             kc_dev_num;
	int                      kc_kemd_fd;
	char                     kc_kemd_filename[KEMD_FILENAME_SIZE];
	volatile sig_atomic_t    kc_done;
	bool                     kc_use_stdout;
	bool                     kc_use_addb2;
	char                     kc_stob_loc[ADDB2_STOB_NAME_SIZE];
	struct m0                kc_instance;
};

static struct kemc_ctx kemc_ctx;

static inline void kem_int_set(struct kemc_ctx *ctx, unsigned int v)
{
	ctx->kc_done = v;
}

static inline unsigned int kem_int_get(struct kemc_ctx *ctx)
{
	return ctx->kc_done;
}

static void kem_sigint(int signum)
{
	kem_int_set(&kemc_ctx, 1);
}

static void usage(void)
{
	fprintf(stderr, "m0kemc: missing CPU number\n"
		"Usage: sudo ./kemc [CPU]\n");
	exit(1);
}

void ke_pf_print(struct pf_event *event)
{
	printf("KE_PAGE_FAULT pid %5d tid %5d addr 0x%016lx wr %u "
	       "fault 0x%04x rdtsc_diff 0x%llx\n",
	       event->pfe_tgid, event->pfe_pid, event->pfe_address,
	       event->pfe_write_access, event->pfe_fault,
	       event->pfe_rdtsc_ret - event->pfe_rdtsc_call);
}

void ke_cs_print(struct cs_event *event)
{
	printf("KE_CTX_SWITCH prev_pid %5d prev_tid %5d next_pid %5d "
	       "next_tid %5d rdtsc 0x%llx\n",
	       event->cse_prev_tgid, event->cse_prev_pid,
	       event->cse_next_tgid, event->cse_next_pid,
	       event->cse_rdtsc);
}

void kem_print(struct ke_msg *msg)
{
	printf("%ld.%06ld ", msg->kem_timestamp.tv_sec,
	       msg->kem_timestamp.tv_usec);

	switch ((enum ke_type)msg->kem_data.ked_type) {
	case KE_PAGE_FAULT:
		ke_pf_print(&msg->kem_data.u.ked_pf);
		return;
	case KE_CONTEXT_SWITCH:
		ke_cs_print(&msg->kem_data.u.ked_cs);
		return;
	default:
		fprintf(stderr, "Unknown event ID: %u\n",
			msg->kem_data.ked_type);
	}
}

static int kem_addb2_init(struct kemc_ctx *ctx)
{
	int rc;
        struct m0_addb2_sys *gsys;
        struct m0_addb2_sys *sys;

	m0_instance_setup(&ctx->kc_instance);
	m0_node_uuid_string_set(NULL);
	rc = m0_module_init(&ctx->kc_instance.i_self, M0_LEVEL_INST_READY);
	if (rc < 0) {
		fprintf(stderr, "m0kemc: Unable to init m0 (%d).\n", rc);
		goto exit;
	}

	sys  = m0_fom_dom()->fd_addb2_sys;
	gsys = m0_addb2_global_get();

	sprintf(ctx->kc_stob_loc, "%s%u", stob_location,
		ctx->kc_dev_num);
	fprintf(stderr, "m0kemc: Trying to create ADDB2 stob:\t%s\n",
		ctx->kc_stob_loc);

	rc = m0_addb2_sys_stor_start(sys, ctx->kc_stob_loc, 13, true, true,
				     ADDB2_STOB_SIZE);
	if (rc != 0) {
		fprintf(stderr, "m0kemc: Unable to start ADDB2 stor.\n");
		goto module;
	}

	m0_addb2_sys_attach(gsys, sys);
	m0_addb2_sys_sm_start(sys);
	m0_addb2_sys_sm_start(gsys);

	M0_ADDB2_PUSH(M0_AVI_KEM_CPU, ctx->kc_dev_num);

	return 0;

 module:
	m0_module_fini(&ctx->kc_instance.i_self, M0_LEVEL_INST_READY);
 exit:
	return rc;
}

static void kem_addb2_fini(struct kemc_ctx *ctx)
{
	struct m0_addb2_sys *sys  = m0_fom_dom()->fd_addb2_sys;
	struct m0_addb2_sys *gsys = m0_addb2_global_get();

	m0_addb2_pop(M0_AVI_KEM_CPU);

	m0_addb2_sys_detach(gsys);
	m0_addb2_sys_sm_stop(gsys);
	m0_addb2_sys_sm_stop(sys);
	m0_addb2_sys_net_stop(sys);
	m0_addb2_sys_stor_stop(sys);
}


static void kem_addb2_log(struct ke_msg *msg)
{
	switch ((enum ke_type)msg->kem_data.ked_type) {
	case KE_PAGE_FAULT: {
		struct pf_event *event = &msg->kem_data.u.ked_pf;

		M0_ADDB2_ADD(M0_AVI_KEM_PAGE_FAULT,
			     event->pfe_tgid, event->pfe_pid,
			     event->pfe_address,
			     event->pfe_write_access, event->pfe_fault,
			     event->pfe_rdtsc_ret -
			     event->pfe_rdtsc_call);

		return;
	}
	case KE_CONTEXT_SWITCH: {
		struct cs_event *event = &msg->kem_data.u.ked_cs;

		M0_ADDB2_ADD(M0_AVI_KEM_CONTEXT_SWITCH,
			     event->cse_prev_tgid, event->cse_prev_pid,
			     event->cse_next_tgid, event->cse_next_pid,
			     event->cse_rdtsc);

		return;
	}
	default:
		fprintf(stderr, "Unknown event ID: %u\n",
			msg->kem_data.ked_type);
	}
}

static int kem_init(struct kemc_ctx *ctx, int dev_num,
		    void (*sighandler)(int),
		    bool use_stdout, bool use_addb2)
{
	struct sigaction sa;
	int              rc = 0;

	memset(ctx, 0, sizeof(*ctx));

	ctx->kc_use_stdout = use_stdout;
	ctx->kc_use_addb2 = use_addb2;

	if (!(ctx->kc_use_addb2 || ctx->kc_use_stdout)) {
		fprintf(stderr, "m0kemc: back end not specified\n");
		rc = -EINVAL;
		goto exit;
	}

	ctx->kc_dev_num = dev_num;
	sprintf(ctx->kc_kemd_filename, "%s%s%u", kemd_dev_path,
		KEMD_DEV_NAME, ctx->kc_dev_num);
	fprintf(stderr, "m0kemc: Trying to open:\t%s\n",
		ctx->kc_kemd_filename);

	ctx->kc_kemd_fd = open(ctx->kc_kemd_filename, O_RDONLY);
	if (ctx->kc_kemd_fd < 0) {
		fprintf(stderr, "m0kemc: Unable to open:%s %d\n",
			ctx->kc_kemd_filename, ctx->kc_kemd_fd);
		rc = -errno;
		goto exit;
	}

	kem_int_set(ctx, 0);

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sighandler;
	rc = sigaction(SIGINT, &sa, NULL);
	if (rc < 0) {
		fprintf(stderr, "m0kemc: Unable to set signal handler (%d).\n",
			rc);
		close(ctx->kc_kemd_fd);
		goto exit;
	}

	if (ctx->kc_use_addb2) {
		rc = kem_addb2_init(ctx);

		if (rc < 0) {
			fprintf(stderr, "m0kemc: Unable to init addb2 back end"
				" (%d)\n", rc);
			close(ctx->kc_kemd_fd);
			goto exit;
		}
	}
 exit:
	return rc;
}

static void kem_fini(struct kemc_ctx *ctx)
{
	if (ctx->kc_use_addb2) {
		kem_addb2_fini(ctx);
	}

	fflush(stdout);
	fflush(stderr);

	close(ctx->kc_kemd_fd);
}

static void kem_log(struct kemc_ctx *ctx, struct ke_msg *msg)
{
	if (ctx->kc_use_stdout)
		kem_print(msg);

	if (ctx->kc_use_addb2)
		kem_addb2_log(msg);
}

static int kem_read(struct kemc_ctx *ctx, struct ke_msg *msg)
{
	return read(ctx->kc_kemd_fd, msg, sizeof(*msg));
}

int main(int argc, char** argv)
{
	bool             use_stdout;
	bool             use_addb2;
	int              rc;
	int              dev_num;
	ssize_t          bytes_read;
	struct kemc_ctx *ctx;
	struct ke_msg    ke_msg;
	struct ke_msg   *ent;

	if (argc < 2)
		usage();

	/* TODO: Handling of options */
	use_stdout = false;
	use_addb2 = true;
	dev_num = atoi(argv[1]);

	ctx = &kemc_ctx;
	rc = kem_init(ctx, dev_num, &kem_sigint,
		      use_stdout, use_addb2);
	if (rc < 0)
		return rc;

	ent = &ke_msg;
	while(kem_int_get(ctx) == 0) {
		bytes_read = kem_read(ctx, ent);
		if ((bytes_read < 0 && errno == -EINTR) ||
		    !bytes_read) {
			continue;
		} else if (bytes_read < 0) {
			fprintf(stderr, "kem_read() returned -1, errno %d\n",
				errno);
			kem_int_set(ctx, 1);
			continue;
		}

		assert(bytes_read == sizeof(*ent));

		kem_log(ctx, ent);
	}
	fprintf(stderr, "Caught SIGINT, exit...\n");

	kem_fini(ctx);

	return 0;
}

/** @} end of kem_client group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
