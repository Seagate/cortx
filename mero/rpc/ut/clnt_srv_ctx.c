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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 09/28/2011
 */

#include "fop/fop.h"            /* m0_fop_alloc */
#include "net/lnet/lnet.h"      /* m0_net_lnet_xprt */
#include "rpc/rpclib.h"
#include "ut/cs_fop.h"          /* cs_ds2_req_fop_fopt */
#include "ut/cs_fop_xc.h"       /* cs_ds2_req_fop */
#include "ut/cs_service.h"      /* m0_cs_default_stypes */
#include "ut/misc.h"            /* M0_UT_PATH */

#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:*"

#define SERVER_DB_NAME        "rpc_ut_server.db"
#define SERVER_STOB_NAME      "rpc_ut_server.stob"
#define SERVER_ADDB_STOB_NAME "linuxstob:rpc_ut_server.addb_stob"
#define SERVER_LOG_NAME       "rpc_ut_server.log"
#define SERVER_ENDPOINT_ADDR  "0@lo:12345:34:1"
#define SERVER_ENDPOINT       "lnet:" SERVER_ENDPOINT_ADDR

enum {
	MAX_RPCS_IN_FLIGHT = 1,
	CONNECT_TIMEOUT    = 5,
	MAX_RETRIES        = 5,
};

static struct m0_net_xprt  *xprt = &m0_net_lnet_xprt;
static struct m0_net_domain client_net_dom;

#ifndef __KERNEL__
static struct m0_rpc_client_ctx cctx = {
	.rcx_net_dom               = &client_net_dom,
	.rcx_local_addr            = CLIENT_ENDPOINT_ADDR,
	.rcx_remote_addr           = SERVER_ENDPOINT_ADDR,
	.rcx_max_rpcs_in_flight    = MAX_RPCS_IN_FLIGHT,
	.rcx_recv_queue_min_length = M0_NET_TM_RECV_QUEUE_DEF_LEN,
	.rcx_fid                   = &g_process_fid,
};

static char *server_argv[] = {
	"rpclib_ut", "-T", "AD", "-D", SERVER_DB_NAME,
	"-S", SERVER_STOB_NAME, "-A", SERVER_ADDB_STOB_NAME,
	"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
	"-f", M0_UT_CONF_PROCESS,
	"-c", M0_UT_PATH("conf.xc")
};

static struct m0_rpc_server_ctx sctx;

/* 'inline' is used, to avoid compiler warning if the function is not used
   in file that includes this file.
 */
static inline void sctx_reset(void)
{
	sctx = (struct m0_rpc_server_ctx){
		.rsx_xprts            = &xprt,
		.rsx_xprts_nr         = 1,
		.rsx_argv             = server_argv,
		.rsx_argc             = ARRAY_SIZE(server_argv),
		.rsx_log_file_name    = SERVER_LOG_NAME,
	};
}

static inline void start_rpc_client_and_server(void)
{
	int rc;

	rc = m0_net_domain_init(&client_net_dom, xprt);
	M0_ASSERT(rc == 0);

	sctx_reset();
	rc = m0_rpc_server_start(&sctx);
	M0_ASSERT(rc == 0);

	rc = m0_rpc_client_start(&cctx);
	M0_ASSERT(rc == 0);
}

/* 'inline' is used, to avoid compiler warning if the function is not used
   in file that includes this file.
 */
static inline void stop_rpc_client_and_server(void)
{
	int rc;

	rc = m0_rpc_client_stop(&cctx);
	M0_ASSERT(rc == 0);
	m0_rpc_server_stop(&sctx);
	m0_net_domain_fini(&client_net_dom);
}

/* 'inline' is used, to avoid compiler warning if the function is not used
   in file that includes this file.
 */
static inline struct m0_fop *fop_alloc(struct m0_rpc_machine *machine)
{
	struct cs_ds2_req_fop *cs_ds2_fop;
	struct m0_fop         *fop;

	M0_PRE(machine != NULL);

	fop = m0_fop_alloc(&cs_ds2_req_fop_fopt, NULL, machine);
	M0_UT_ASSERT(fop != NULL);

	cs_ds2_fop = m0_fop_data(fop);
	cs_ds2_fop->csr_value = 0xaaf5;

	return fop;
}

#endif /* !__KERNEL__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
