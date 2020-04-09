/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 10-Mar-2017
 */
#include <unistd.h>                     /* sleep */
#include "reqh/reqh_service_internal.h" /* reqh_service_ctx_sm_lock */
#include "rpc/rpclib.h"                 /* m0_rpc_server_start */
#include "ut/misc.h"                    /* M0_UT_PATH */
#include "ut/ut.h"

#define SERVER_LOG_FILE_NAME       "reqh_service_ctx.log"

static struct m0_net_xprt       *ut_xprts[] = { &m0_net_lnet_xprt };
static struct m0_rpc_server_ctx  ut_sctx;
static struct m0_net_domain      ut_client_net_dom;
static struct m0_net_xprt       *ut_xprt = &m0_net_lnet_xprt;
static struct m0_rpc_machine     ut_rmach;
static const char               *ut_ep_addr_remote = "0@lo:12345:34:999";
static struct m0_net_buffer_pool ut_buf_pool;
static struct m0_reqh            ut_reqh;

static char *sargs[] = {"m0d", "-T", "linux",
			"-D", "cs_sdb", "-S", "cs_stob",
			"-A", "linuxstob:cs_addb_stob",
			"-e", "lnet:0@lo:12345:34:1",
			"-H", "0@lo:12345:34:1",
			"-w", "10",
			"-c", M0_SRC_PATH("reqh/ut/service_ctx.xc")
};

static void reqh_service_ctx_ut_test_helper(char *ut_argv[], int ut_argc,
					   void (*ut_body)(void))
{
	ut_sctx = (struct m0_rpc_server_ctx) {
		.rsx_xprts            = ut_xprts,
		.rsx_xprts_nr         = ARRAY_SIZE(ut_xprts),
		.rsx_argv             = ut_argv,
		.rsx_argc             = ut_argc,
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
	};
	M0_UT_ASSERT(m0_rpc_server_start(&ut_sctx) == 0);
	ut_body();
	m0_rpc_server_stop(&ut_sctx);
}

static int reqh_service_ctx_ut__remote_rmach_init(void)
{
	enum { NR_TMS = 1 };
	int      rc;
	uint32_t bufs_nr;

	rc = m0_net_domain_init(&ut_client_net_dom, ut_xprt);
	M0_ASSERT(rc == 0);
	bufs_nr = m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, NR_TMS);
	rc = m0_rpc_net_buffer_pool_setup(&ut_client_net_dom, &ut_buf_pool,
					  bufs_nr, NR_TMS);
	M0_ASSERT(rc == 0);
	rc = M0_REQH_INIT(&ut_reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_db      = NULL,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &g_process_fid ) ?:
		m0_rpc_machine_init(&ut_rmach, &ut_client_net_dom,
				    ut_ep_addr_remote, &ut_reqh, &ut_buf_pool,
				    M0_BUFFER_ANY_COLOUR,
				    M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				    M0_NET_TM_RECV_QUEUE_DEF_LEN);
	return rc;
}

static void reqh_service_ctx_ut__remote_rmach_fini(void)
{
	m0_rpc_machine_fini(&ut_rmach);
	m0_reqh_fini(&ut_reqh);
	m0_rpc_net_buffer_pool_cleanup(&ut_buf_pool);
	m0_net_domain_fini(&ut_client_net_dom);
}

static struct m0_reqh_service_ctx *
reqh_service_ctx_ut__find(struct m0_fid *fid, int stype, const char *ep)
{
	struct m0_pools_common     *pc = &ut_sctx.rsx_mero_ctx.cc_pools_common;
	struct m0_reqh_service_ctx *ctx;

	ctx = m0_pools_common_service_ctx_find(pc, fid, stype);
	M0_UT_ASSERT(ctx != NULL);
	M0_UT_ASSERT(ep == NULL ||
		     m0_streq(m0_rpc_link_end_point(&ctx->sc_rlink), ep));
	return ctx;
}

static struct m0_reqh_service_ctx *reqh_service_ctx_ut__find_remote(void)
{
	struct m0_fid fid = M0_FID_TINIT('s', 1, 27);

	return reqh_service_ctx_ut__find(&fid, M0_CST_DS1, ut_ep_addr_remote);
}

static struct m0_semaphore g_sem;

static bool ut_dead_end__cb(struct m0_clink *clink)
{
	m0_semaphore_up(&g_sem);
	return true;
}

static void ut_dead_end__connecting(void)
{
	struct m0_reqh_service_ctx *ctx = reqh_service_ctx_ut__find_remote();
	struct m0_clink             clink;
	/*
	 * Need to do nothing here but just wait. Connecting is to fail
	 * natural way due to "0@lo:12345:34:999" endpoint unavailable.
	 */
	m0_semaphore_init(&g_sem, 0);
	m0_clink_init(&clink, ut_dead_end__cb);
	m0_clink_add_lock(&ctx->sc_rlink.rlk_wait, &clink);
	m0_semaphore_down(&g_sem);
	m0_clink_del_lock(&clink);
	m0_semaphore_fini(&g_sem);
	m0_clink_fini(&clink);
}

/*
 * The test imitates situations when service context shuts down concurrently
 * with context connection.
 *
 * We are to start without "0@lo:12345:34:999" remote end and then see if we can
 * shutdown reqh smooth.
 */
static void test_dead_end_connect(void)
{
	reqh_service_ctx_ut_test_helper(sargs, ARRAY_SIZE(sargs),
					ut_dead_end__connecting);
}

static void reqh_service_ctx_ut__wait_online(struct m0_reqh_service_ctx *ctx)
{
	int state;
	/*
	 * Make sure the context gets online. As the context may be already
	 * connected to the moment, attaching clink may be a trap. So we are
	 * going to wait in a plain cycle.
	 */
	do {
		sleep(1);
		reqh_service_ctx_sm_lock(ctx);
		state = CTX_STATE(ctx);
		M0_UT_ASSERT(M0_IN(state, (M0_RSC_ONLINE, M0_RSC_CONNECTING)));
		reqh_service_ctx_sm_unlock(ctx);
	} while (state != M0_RSC_ONLINE);
}

static void reqh_service_ctx_ut__wait_all_online(void)
{
	struct m0_pools_common     *pc = &ut_sctx.rsx_mero_ctx.cc_pools_common;
	struct m0_reqh_service_ctx *ctx;

	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		reqh_service_ctx_ut__wait_online(ctx);
	} m0_tl_endfor;
}

/*
 * The case is about initiate disconnection when remote end is already dead, and
 * shut down pool contexts immediately while disconnecting.
 */
static void ut_dead_end__disconnecting(void)
{
	struct m0_reqh_service_ctx *ctx = reqh_service_ctx_ut__find_remote();

	reqh_service_ctx_ut__wait_all_online();
	reqh_service_ctx_ut__remote_rmach_fini();
	m0_reqh_service_disconnect(ctx);
}

/*
 * The case is about initiate disconnection when remote end is already dead, and
 * shut down pool contexts having corresponding context already offline.
 */
static void ut_dead_end__disconnected(void)
{
	struct m0_reqh_service_ctx *ctx = reqh_service_ctx_ut__find_remote();
	struct m0_clink             clink;

	reqh_service_ctx_ut__wait_all_online();
	m0_semaphore_init(&g_sem, 0);
	m0_clink_init(&clink, ut_dead_end__cb);
	m0_clink_add_lock(&ctx->sc_rlink.rlk_wait, &clink);
	reqh_service_ctx_ut__remote_rmach_fini();
	m0_reqh_service_disconnect(ctx);
	m0_semaphore_down(&g_sem);
	m0_clink_del_lock(&clink);
	m0_semaphore_fini(&g_sem);
	m0_clink_fini(&clink);
}

/*
 * The test imitates situations when service context shuts down concurrently
 * with disconnection.
 *
 * This time a separate rpc machine is launched to let "0@lo:12345:34:999"
 * remote end be connectable. The remote end will be stopped in the course of
 * the test, and then we see if we can shutdown reqh smooth.
 */
static void test_dead_end_disconnect(void)
{
	M0_UT_ASSERT(reqh_service_ctx_ut__remote_rmach_init() == 0);
	reqh_service_ctx_ut_test_helper(sargs, ARRAY_SIZE(sargs),
					ut_dead_end__disconnecting);

	M0_UT_ASSERT(reqh_service_ctx_ut__remote_rmach_init() == 0);
	reqh_service_ctx_ut_test_helper(sargs, ARRAY_SIZE(sargs),
					ut_dead_end__disconnected);
}

struct m0_ut_suite reqh_service_ctx_ut = {
	.ts_name = "reqh-service-ctx-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "dead-end-conn", test_dead_end_connect },
		{ "dead-end-disc", test_dead_end_disconnect },
		{ NULL, NULL },
	},
	.ts_owners = "Igor Vartanov",
};
M0_EXPORTED(reqh_service_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
