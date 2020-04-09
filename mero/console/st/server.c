/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 08/03/2011
 */

#include <signal.h>
#include <unistd.h>             /* sleep */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"

#include "net/lnet/lnet.h"
#include "mero/init.h"          /* m0_init */
#include "lib/getopts.h"        /* M0_GETOPTS */
#include "module/instance.h"    /* m0 */
#include "ut/module.h"          /* m0_ut_module (XXX DELETEME) */
#include "rpc/rpclib.h"         /* m0_rpc_server_start */
#include "ut/cs_service.h"      /* m0_cs_default_stypes */
#include "ut/misc.h"            /* M0_UT_PATH */
#include "ut/ut.h"              /* m0_ut_init */

#include "console/console.h"
#include "console/console_fop.h"
#include "conf/ut/common.h"     /* SERVER_ENDPOINT_ADDR */

/**
   @addtogroup console
   @{
 */

#define ENDPOINT  "lnet:" SERVER_ENDPOINT_ADDR
#define NAME(ext) "console_st_srv" ext

static int signaled = 0;

static void sig_handler(int num)
{
	signaled = 1;
}

/** @brief Test server for m0console */
int main(int argc, char **argv)
{
	enum { CONSOLE_STR_LEN = 16 };
	static struct m0 instance;
	char     tm_len[CONSOLE_STR_LEN];
	char     rpc_size[CONSOLE_STR_LEN];
	int      result;
	uint32_t tm_recv_queue_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	uint32_t max_rpc_msg_size  = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	char    *server_argv[] = {
		argv[0], "-T", "AD", "-D", NAME(".db"),
		"-S", NAME(".stob"), "-A", "linuxstob:"NAME("-addb.stob"),
		"-w", "10", "-e", ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
		"-f", M0_UT_CONF_PROCESS,
		"-c", M0_UT_PATH("diter.xc"),
		"-q", tm_len, "-m", rpc_size
	};
	struct m0_net_xprt      *xprt = &m0_net_lnet_xprt;
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts            = &xprt,
		.rsx_xprts_nr         = 1,
		.rsx_argv             = server_argv,
		.rsx_argc             = ARRAY_SIZE(server_argv),
		.rsx_log_file_name    = NAME(".log")
	};

	M0_ENTRY();

	m0_instance_setup(&instance);
	(void)m0_ut_module_type.mt_create(&instance);

	/* We have to use m0_ut_init() here, because it initialises
	 * m0_cs_default_stypes. XXX FIXME */
	result = m0_ut_init(&instance);
	if (result != 0)
		return M0_RC(-result);

	m0_console_verbose = false;
	result = M0_GETOPTS("server", argc, argv,
			    M0_FLAGARG('v', "verbose", &m0_console_verbose),
			    M0_FORMATARG('q', "minimum TM receive queue length",
					 "%i", &tm_recv_queue_len),
			    M0_FORMATARG('m', "max rpc msg size", "%i",
					 &max_rpc_msg_size),);
	if (result != 0)
		return M0_RC(-result);

	sprintf(tm_len, "%d", tm_recv_queue_len);
	sprintf(rpc_size, "%d", max_rpc_msg_size);

	result = m0_console_fop_init();
	if (result != 0)
		goto ut_fini;

	result = m0_rpc_server_start(&sctx);
	if (result != 0)
		goto fop_fini;

	printf("Press CTRL+C to quit.\n");
	signal(SIGINT, sig_handler);
	while (!signaled)
		sleep(1);
	printf("\nExiting Server.\n");

	m0_rpc_server_stop(&sctx);
fop_fini:
	m0_console_fop_fini();
ut_fini:
	m0_ut_fini();
	return M0_RC(-result);
}

/** @} end of console group */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
