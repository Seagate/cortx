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
 * Original author: Rohan Puri <rohan_puri@xyratex.com>
 * Original creation date: 04-Oct-2012
 */

#include "ut/ut.h"
#include "lib/finject.h"
#include "rpc/rpc.h"
#include "rpc/rpc_internal.h"
#include "net/net.h"
#include "rpc/rpc.h"
#include "net/buffer_pool.h"
#include "net/lnet/lnet.h"
#include "rpc/ut/clnt_srv_ctx.c"   /* sctx, cctx. NOTE: This is .c file */

static struct m0_rpc_machine     machine;
static uint32_t                  max_rpc_msg_size = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
static const char               *ep_addr = "0@lo:12345:34:2";
static struct m0_net_buffer_pool buf_pool;
static struct m0_reqh            reqh;
static uint32_t tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;

static int rpc_mc_ut_init(void)
{
	enum { NR_TMS = 1 };
	int      rc;
	uint32_t bufs_nr;

	rc = m0_net_domain_init(&client_net_dom, xprt);
	M0_ASSERT(rc == 0);

	bufs_nr = m0_rpc_bufs_nr(tm_recv_queue_min_len, NR_TMS);
	rc = m0_rpc_net_buffer_pool_setup(&client_net_dom, &buf_pool, bufs_nr,
					  NR_TMS);
	M0_ASSERT(rc == 0);
	/*
	 * Initialise a rudimentary reqh, sufficient for m0_rcp_machine_init()
	 * to go through.
	 */
	rc = M0_REQH_INIT(&reqh,
			  .rhia_dtm       = (void *)1,
			  .rhia_db        = NULL,
			  .rhia_mdstore   = (void *)1,
			  .rhia_fid       = &g_process_fid,
		);
	return rc;
}

static int rpc_mc_ut_fini(void)
{
	m0_reqh_fini(&reqh);
	m0_rpc_net_buffer_pool_cleanup(&buf_pool);
	m0_net_domain_fini(&client_net_dom);
	return 0;
}

static void rpc_mc_init_fini_test(void)
{
	int rc;

	/*
	 * Test - rpc_machine_init & rpc_machine_fini for success case
	 */

	rc = m0_rpc_machine_init(&machine, &client_net_dom, ep_addr,
				 &reqh, &buf_pool, M0_BUFFER_ANY_COLOUR,
				 max_rpc_msg_size, tm_recv_queue_min_len);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(machine.rm_stopping == false);
	m0_rpc_machine_fini(&machine);
}

static void rpc_mc_fini_race_test(void)
{
	struct m0_rpc_conn    conn;
	struct m0_rpc_session session;
	int rc;

	rc = m0_rpc_machine_init(&machine, &client_net_dom, ep_addr,
				 &reqh, &buf_pool, M0_BUFFER_ANY_COLOUR,
				 max_rpc_msg_size, tm_recv_queue_min_len);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_connect(&conn, &session,
	                           &machine,
	                           machine.rm_tm.ntm_ep->nep_addr,
				   NULL, MAX_RPCS_IN_FLIGHT,
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_session_destroy(&session, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	/*
	 * The fault injection increases a time interval from the
	 * M0_RPC_CONN_TERMINATED state till M0_RPC_CONN_FINALISED. At this
	 * time, the connection isn't deleted yet from the the
	 * m0_rpc_machine::rm_incoming_conns and
	 * m0_rpc_machine_cleanup_incoming_connections()
	 * should work correctly in such case.
	 */
	m0_fi_enable("buf_send_cb", "delay_callback");
	rc = m0_rpc_conn_destroy(&conn, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	m0_rpc_machine_fini(&machine);
	m0_fi_disable("buf_send_cb", "delay_callback");
}

static void rpc_mc_init_fail_test(void)
{
	int rc;

	/*
	 * Test - rpc_machine_init for failure cases
	 *	Case 1 - m0_net_tm_init failed, should return -EINVAL
	 *	Case 2 - m0_net_tm_start failed, should return -ENETUNREACH
	 *	Case 3 - root_session_cob_create failed, should return -EINVAL
	 *	Case 4 - m0_root_session_cob_create failed, should ret -EINVAL
	 *		checks for db_tx_abort code path execution
	 */

	m0_fi_enable_once("m0_net_tm_init", "fake_error");
	rc = m0_rpc_machine_init(&machine, &client_net_dom, ep_addr,
				 &reqh, &buf_pool, M0_BUFFER_ANY_COLOUR,
				 max_rpc_msg_size, tm_recv_queue_min_len);
	M0_UT_ASSERT(rc == -EINVAL);

	m0_fi_enable_once("m0_net_tm_start", "fake_error");
	rc = m0_rpc_machine_init(&machine, &client_net_dom, ep_addr,
				 &reqh, &buf_pool, M0_BUFFER_ANY_COLOUR,
				 max_rpc_msg_size, tm_recv_queue_min_len);
	M0_UT_ASSERT(rc == -ENETUNREACH);
	/**
	  Root session cob as well as other mkfs related structres are now
	  created on behalf of serivice startup if -p option is specified.
	 */
}

#ifndef __KERNEL__

static bool conn_added_called;
static bool session_added_called;
static bool mach_terminated_called;

static void conn_added(struct m0_rpc_machine_watch *watch,
		       struct m0_rpc_conn *conn)
{
	M0_UT_ASSERT(conn->c_sm.sm_state == M0_RPC_CONN_INITIALISED);
	M0_UT_ASSERT(rpc_conn_tlink_is_in(conn));
	M0_UT_ASSERT(m0_rpc_machine_is_locked(watch->mw_mach));
	conn_added_called = true;
}

static void session_added(struct m0_rpc_machine_watch *watch,
			  struct m0_rpc_session *session)
{
	M0_UT_ASSERT(session->s_sm.sm_state == M0_RPC_SESSION_INITIALISED);
	M0_UT_ASSERT(rpc_session_tlink_is_in(session));
	M0_UT_ASSERT(m0_rpc_machine_is_locked(watch->mw_mach));
	session_added_called = true;
}

static void mach_terminated(struct m0_rpc_machine_watch *watch)
{
	M0_UT_ASSERT(!rmach_watch_tlink_is_in(watch));
	mach_terminated_called = true;
}

static void rpc_machine_watch_test(void)
{
	struct m0_rpc_machine_watch  watch;
	struct m0_rpc_machine       *rmach;
	int                          rc;

	sctx_reset();
	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);

	rmach = m0_rpc_server_ctx_get_rmachine(&sctx);
	M0_UT_ASSERT(rmach != NULL);

	watch.mw_mach          = rmach;
	watch.mw_conn_added    = conn_added;
	watch.mw_session_added = session_added;

	m0_rpc_machine_watch_attach(&watch);

	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(conn_added_called && session_added_called);

	m0_rpc_machine_watch_detach(&watch);
	/* It is safe to call detach if watch is already detached */
	m0_rpc_machine_watch_detach(&watch);

	/* If rpc machine is being terminated, while still having attached
	   watchers, then they are detached and mw_mach_terminated() callback
	   is called.
	 */
	watch.mw_mach_terminated = mach_terminated;
	m0_rpc_machine_watch_attach(&watch);
	rc = m0_rpc_client_stop(&cctx);
	M0_UT_ASSERT(rc == 0);
	m0_rpc_server_stop(&sctx);
	M0_UT_ASSERT(mach_terminated_called);
}
#endif /* __KERNEL__ */

struct m0_ut_suite rpc_mc_ut = {
	.ts_name = "rpc-machine-ut",
	.ts_init = rpc_mc_ut_init,
	.ts_fini = rpc_mc_ut_fini,
	.ts_tests = {
		{ "rpc_mc_init_fini", rpc_mc_init_fini_test },
		{ "rpc_mc_fini_race", rpc_mc_fini_race_test },
		{ "rpc_mc_init_fail", rpc_mc_init_fail_test },
#ifndef __KERNEL__
		{ "rpc_mc_watch",     rpc_machine_watch_test},
#endif
		{ NULL, NULL}
	}
};
M0_EXPORTED(rpc_mc_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
