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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 18-Mar-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "conf/rconfc_internal.h"
#include "conf/helpers.h"          /* m0_confc_expired_cb */
#include "lib/finject.h"
#include "lib/misc.h"              /* M0_BITS */
#include "lib/memory.h"
#include "ut/ut.h"
#include "rpc/rpclib.h"
#include "rpc/ut/clnt_srv_ctx.c"   /* sctx, cctx. NOTE: This is .c file */
#include "rpc/ut/fops.h"
#include "rpc/rpc_internal.h"

static struct m0_rpc_machine *machine;
static const char            *remote_addr;

enum {
	TIMEOUT  = 4 /* second */,
};

static int ts_rcv_session_init(void)   /* ts_ for "test suite" */
{
	start_rpc_client_and_server();
	machine     = &cctx.rcx_rpc_machine;
	remote_addr =  cctx.rcx_remote_addr;
	return 0;
}

static int ts_rcv_session_fini(void)
{
	stop_rpc_client_and_server();
	return 0;
}

struct fp {
	const char *fn;
	const char *pt;
	int         erc; /* expected rc */
};

static bool enable_for_all_but_first_call(void *data)
{
	int *pcount = data;

	return ++*pcount > 1;
}

static void test_conn_establish(void)
{
	struct m0_net_end_point *ep;
	struct m0_rpc_conn       conn;
	int                      count;
	int                      rc;
	int                      i;
	struct fp fps1[] = {
		{"session_gen_fom_create",         "reply_fop_alloc_failed"},
		{"m0_rpc_fom_conn_establish_tick", "conn-alloc-failed"     },
	};
	struct fp fps2[] = {
		{"rpc_chan_get",        "fake_error"   },
		{"session_zero_attach", "out-of-memory"},
	};

	rc = m0_net_end_point_create(&ep, &machine->rm_tm, remote_addr);
	M0_UT_ASSERT(rc == 0);

	/* TEST: Connection established successfully */
	rc = m0_rpc_conn_create(&conn, NULL, ep, machine, MAX_RPCS_IN_FLIGHT,
				m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_conn_destroy(&conn, m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);

	/* TEST: Duplicate conn-establish requests are accepted but only
	         one of them gets executed and rest of them are ignored.
	 */
	m0_fi_enable_once("m0_rpc_fom_conn_establish_tick", "sleep-for-resend");
	m0_fi_enable("m0_rpc_fom_conn_establish_tick", "free-timer");
	rc = m0_rpc_conn_create(&conn, NULL, ep, machine, MAX_RPCS_IN_FLIGHT,
				M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_conn_destroy(&conn, m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < ARRAY_SIZE(fps1); ++i) {
		m0_fi_enable(fps1[i].fn, fps1[i].pt);
		rc = m0_rpc_conn_create(&conn, NULL, ep, machine,
					MAX_RPCS_IN_FLIGHT,
					m0_time_from_now(TIMEOUT, 0));
		m0_fi_disable(fps1[i].fn, fps1[i].pt);
		M0_UT_ASSERT(rc == -ETIMEDOUT);
	}
	for (i = 0; i < ARRAY_SIZE(fps2); ++i) {
		count = 0;
		m0_fi_enable_func(fps2[i].fn, fps2[i].pt,
				  enable_for_all_but_first_call, &count);
		rc = m0_rpc_conn_create(&conn, NULL, ep, machine,
					MAX_RPCS_IN_FLIGHT,
					m0_time_from_now(TIMEOUT, 0));
		m0_fi_disable(fps2[i].fn, fps2[i].pt);
		M0_UT_ASSERT(rc == -ETIMEDOUT);
	}
	m0_net_end_point_put(ep);
}

static void test_session_establish(void)
{
	struct m0_net_end_point *ep;
	struct m0_rpc_session    session;
	struct m0_rpc_conn       conn;
	int                      rc;
	int                      i;
	struct fp fps[] = {
		{"session_gen_fom_create", "reply_fop_alloc_failed",
								   -ETIMEDOUT},
		{"m0_rpc_fom_session_establish_tick",
		                           "session-alloc-failed", -ENOMEM},
	};
	rc = m0_net_end_point_create(&ep, &machine->rm_tm, remote_addr);
	M0_UT_ASSERT(rc == 0);

	/* TEST1: Connection established successfully */
	rc = m0_rpc_conn_create(&conn, NULL, ep, machine, MAX_RPCS_IN_FLIGHT,
				m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_session_create(&session, &conn,
				   m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_session_destroy(&session, m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < ARRAY_SIZE(fps); ++i) {
		m0_fi_enable(fps[i].fn, fps[i].pt);
		rc = m0_rpc_session_create(&session, &conn,
					   m0_time_from_now(TIMEOUT, 0));
		M0_UT_ASSERT(rc == fps[i].erc);
		m0_fi_disable(fps[i].fn, fps[i].pt);
	}

	rc = m0_rpc_conn_destroy(&conn, m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);

	m0_net_end_point_put(ep);
}

static void test_session_terminate(void)
{
	struct m0_net_end_point *ep;
	struct m0_rpc_session    session;
	struct m0_rpc_conn       conn;
	int                      rc;
	int                      i;
	struct fp fps[] = {
		{"session_gen_fom_create", "reply_fop_alloc_failed",
							-ETIMEDOUT},
	};
	rc = m0_net_end_point_create(&ep, &machine->rm_tm, remote_addr);
	M0_UT_ASSERT(rc == 0);

	/* TEST1: Connection established successfully */
	rc = m0_rpc_conn_create(&conn, NULL, ep, machine, MAX_RPCS_IN_FLIGHT,
				m0_time_from_now(TIMEOUT, 0));
	for (i = 0; i < ARRAY_SIZE(fps); ++i) {
		rc = m0_rpc_session_create(&session, &conn,
					   m0_time_from_now(TIMEOUT, 0));
		M0_UT_ASSERT(rc == 0);

		m0_fi_enable(fps[i].fn, fps[i].pt);
		rc = m0_rpc_session_destroy(&session,
					    m0_time_from_now(TIMEOUT, 0));
		M0_UT_ASSERT(rc == fps[i].erc);
		m0_fi_disable(fps[i].fn, fps[i].pt);
		/*
		  Q: How to handle following scenario:
		     - sender establishes one RPC connection with receiver
		     - sender creates one RPC session
		     - sender wants to terminate session:
		       -- sender sends SESSION_TERMINATE request
		       -- the request fails without any reply (consider
			  because fom alloc failed)
		     - sender side session moves to FAILED state, but
		       receiver side session is still active
		     - sender moves on to terminate conn
		     - sender sends CONN_TERMINATE fop
		     - the CONN_TERMINATE request gets accepted by receiver but
		       conn cannot be terminated because there is still active
		       session; reciever replies as -EBUSY
		  A. One possible way is, while processing CONN_TERMINATE
		     request, receiver simply aborts all sessions within it.
		     Temporarily implemented this behavior.
		 */
	}

	rc = m0_rpc_conn_destroy(&conn, m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);

	m0_net_end_point_put(ep);
}

static void test_conn_terminate(void)
{
	struct m0_net_end_point *ep;
	struct m0_rpc_conn       conn;
	int                      rc;
	int                      i;
	struct fp fps[] = {
		{"session_gen_fom_create", "reply_fop_alloc_failed",
							-ETIMEDOUT},
	};

	rc = m0_net_end_point_create(&ep, &machine->rm_tm, remote_addr);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < ARRAY_SIZE(fps); ++i) {
		rc = m0_rpc_conn_create(&conn, NULL, ep, machine,
					MAX_RPCS_IN_FLIGHT,
					m0_time_from_now(TIMEOUT, 0));
		M0_UT_ASSERT(rc == 0);
		m0_fi_enable(fps[i].fn, fps[i].pt);
		rc = m0_rpc_conn_destroy(&conn, m0_time_from_now(TIMEOUT, 0));
		M0_UT_ASSERT(rc == fps[i].erc);
		m0_fi_disable(fps[i].fn, fps[i].pt);
	}
	m0_net_end_point_put(ep);
}

M0_EXTERN struct m0_rm_incoming_ops m0_rconfc_ri_ops;

static void test_conn_ha_subscribe()
{
	struct m0_net_end_point *ep;
	struct m0_rpc_conn       conn;
	struct rlock_ctx        *rlx;
	struct m0_rconfc        *rconfc;
	struct m0_fid            profile = M0_FID_TINIT('p', 1, 0);
	struct m0_conf_obj      *svc_obj = NULL;
	int                      rc;

	*m0_reqh2profile(&cctx.rcx_reqh) = profile;
	rconfc = &cctx.rcx_reqh.rh_rconfc;
	rc = m0_rconfc_init(rconfc, &profile, m0_locality0_get()->lo_grp,
			    machine, m0_confc_expired_cb, m0_confc_ready_cb);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_LOG(M0_DEBUG, "rconfc_init %p", rconfc);
	rc = m0_net_end_point_create(&ep, &machine->rm_tm, remote_addr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_service_find(m0_reqh2confc(&cctx.rcx_reqh), M0_CST_IOS,
				   remote_addr, &svc_obj);
	M0_UT_ASSERT(rc == 0);
	M0_ASSERT(svc_obj != NULL);
	rc = m0_rpc_conn_create(&conn, &svc_obj->co_id, ep, machine,
			MAX_RPCS_IN_FLIGHT,
			m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(svc_obj->co_ha_chan.ch_waiters == 1);
	rlx = rconfc->rc_rlock_ctx;
	m0_rconfc_ri_ops.rio_conflict(&rlx->rlc_req);
	m0_sm_group_lock(rconfc->rc_sm.sm_grp);
	m0_sm_timedwait(&rconfc->rc_sm, M0_BITS(M0_RCS_IDLE, M0_RCS_FAILURE),
			M0_TIME_NEVER);
	m0_sm_group_unlock(rconfc->rc_sm.sm_grp);
	M0_UT_ASSERT(rconfc->rc_sm.sm_state == M0_RCS_IDLE);
	/*
	 * Check that conn is still subscribed after fetching new configuration
	 */
	rc = m0_confc_service_find(m0_reqh2confc(&cctx.rcx_reqh), M0_CST_IOS,
				   remote_addr, &svc_obj);
	M0_UT_ASSERT(rc == 0);
	M0_ASSERT(svc_obj != NULL);
	M0_UT_ASSERT(svc_obj->co_ha_chan.ch_waiters == 1);
	rc = m0_rpc_conn_destroy(&conn, m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(svc_obj->co_ha_chan.ch_waiters == 0);
	m0_net_end_point_put(ep);
	m0_rconfc_stop_sync(rconfc);
	m0_rconfc_fini(rconfc);
}

struct m0_ut_suite rpc_rcv_session_ut = {
	.ts_name = "rpc-rcv-session-ut",
	.ts_init = ts_rcv_session_init,
	.ts_fini = ts_rcv_session_fini,
	.ts_tests = {
		{ "conn-establish",    test_conn_establish   },
		{ "session-establish", test_session_establish},
		{ "session-terminate", test_session_terminate},
		{ "conn-terminate",    test_conn_terminate   },
		{ "conn-ha-subscribe", test_conn_ha_subscribe},
		{ NULL, NULL },
	}
};

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
