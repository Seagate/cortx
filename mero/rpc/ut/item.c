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
 * Original author: Amit Jambure <amit_jambure@xyratex.com>
 * Original creation date: 10/19/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#define M0_UT_TRACE 0

#include "ut/ut.h"
#include "lib/finject.h"
#include "lib/fs.h"                /* m0_file_read */
#include "lib/misc.h"              /* M0_BITS */
#include "lib/semaphore.h"
#include "lib/memory.h"
#include "rpc/rpclib.h"
#include "rpc/ut/clnt_srv_ctx.c"   /* sctx, cctx. NOTE: This is .c file */
#include "rpc/ut/fops.h"
#include "rpc/rpc_internal.h"

enum {
	TIMEOUT  = 4
};

static int _test(void);
static void _test_timeout(m0_time_t deadline, m0_time_t timeout, bool reset);
static void _test_resend(struct m0_fop *fop, bool post_sync);
static void _test_timer_start_failure(void);
static void _ha_notify(struct m0_rpc_conn *conn, uint8_t state);
static void _ha_do_not_notify(struct m0_rpc_conn *conn, uint8_t state);

static enum m0_ha_obj_state expected_state;
static struct m0_fid        expected_fid;
static struct m0_rpc_machine *machine;
static struct m0_rpc_session *session;
static struct m0_rpc_stats    saved;
static struct m0_rpc_stats    stats;
static struct m0_rpc_item    *item;
static struct m0_fop         *fop;
static int                    item_rc;
static struct m0_rpc_item_type test_item_cache_itype;
extern const struct m0_sm_conf outgoing_item_sm_conf;
extern const struct m0_sm_conf incoming_item_sm_conf;

#define IS_INCR_BY_N(p, n) _0C(saved.rs_ ## p + (n) == stats.rs_ ## p)
#define IS_INCR_BY_1(p) IS_INCR_BY_N(p, 1)

static int ts_item_init(void)   /* ts_ for "test suite" */
{
	test_item_cache_itype.rit_incoming_conf = incoming_item_sm_conf;
	test_item_cache_itype.rit_outgoing_conf = outgoing_item_sm_conf;
	m0_rpc_test_fops_init();
	start_rpc_client_and_server();
	session = &cctx.rcx_session;
	machine = cctx.rcx_session.s_conn->c_rpc_machine;

	return 0;
}

static int ts_item_fini(void)
{
	stop_rpc_client_and_server();
	m0_rpc_test_fops_fini();
	return 0;
}

static bool chk_state(const struct m0_rpc_item *item,
		      enum m0_rpc_item_state    state)
{
	return item->ri_sm.sm_state == state;
}

static void test_simple_transitions(void)
{
	int rc;

	/* TEST1: Simple request and reply sequence */
	M0_LOG(M0_DEBUG, "TEST:1:START");
	m0_rpc_machine_get_stats(machine, &saved, false /* clear stats? */);
	fop = fop_alloc(machine);
	item = &fop->f_item;
	rc = m0_rpc_post_sync(fop, session, &cs_ds_req_fop_rpc_item_ops,
			      0 /* deadline */);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_error == 0);
	M0_UT_ASSERT(item->ri_reply != NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_REPLIED) &&
		     chk_state(item->ri_reply, M0_RPC_ITEM_ACCEPTED));
	m0_rpc_machine_get_stats(machine, &stats, true);
	M0_UT_ASSERT(IS_INCR_BY_1(nr_sent_items) &&
		     IS_INCR_BY_1(nr_rcvd_items));
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:1:END");
}

void disable_packet_ready_set_reply_error(int arg)
{
	m0_nanosleep(m0_time(M0_RPC_ITEM_RESEND_INTERVAL * 2 + 1, 0), NULL);
	m0_fi_disable("packet_ready", "set_reply_error");
}

static void test_reply_item_error(void)
{
	int rc;
	struct m0_thread thread = {0};

	M0_LOG(M0_DEBUG, "TEST:1:START");
	m0_rpc_machine_get_stats(machine, &saved, false /* clear stats? */);
	fop = fop_alloc(machine);
	item = &fop->f_item;
	m0_fi_enable("packet_ready", "set_reply_error");
	rc = M0_THREAD_INIT(&thread, int, NULL,
			    &disable_packet_ready_set_reply_error,
			    0, "disable_fi");
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_post_sync(fop, session, NULL, 0 /* deadline */);

	/* Error happens on server side, and client will try to resend fop.
	 * (M0_RPC_ITEM_RESEND_INTERVAL * 2 + 1) seconds later, server sends
	 * back reply successfully. */
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_error == 0);
	M0_UT_ASSERT(item->ri_reply != NULL);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:1:END");
	m0_thread_join(&thread);
}

extern void (*m0_rpc__item_dropped)(struct m0_rpc_item *item);

static struct m0_semaphore wait;
static void test_dropped(struct m0_rpc_item *item)
{
	m0_semaphore_up(&wait);
}

static void test_timeout(void)
{
	const struct m0_rpc_conn_ha_cfg *rchc_orig = session->s_conn->c_ha_cfg;
	struct m0_rpc_conn_ha_cfg  rchc_ut = *rchc_orig;
	int                        rc;

	/* Test2.1: Request item times out before reply reaches to sender.
		    Delayed reply is then dropped.
	 */
	rchc_ut.rchc_ops.cho_ha_notify = _ha_do_not_notify;
	m0_rpc_conn_ha_cfg_set(session->s_conn, &rchc_ut);
	M0_LOG(M0_DEBUG, "TEST:2.1:START");
	fop = fop_alloc(machine);
	item = &fop->f_item;
	item->ri_nr_sent_max = 1;
	m0_rpc_machine_get_stats(machine, &saved, false);
	m0_fi_enable_once("cs_req_fop_fom_tick", "inject_delay");
	m0_fi_enable_once("item_received", "drop_signal");
	m0_semaphore_init(&wait, 0);
	m0_rpc__item_dropped = &test_dropped;
	rc = m0_rpc_post_sync(fop, session, &cs_ds_req_fop_rpc_item_ops,
			      0 /* deadline */);
	M0_UT_ASSERT(rc == -ETIMEDOUT);
	M0_UT_ASSERT(item->ri_error == -ETIMEDOUT);
	M0_UT_ASSERT(item->ri_reply == NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED));
	m0_semaphore_down(&wait);
	m0_semaphore_fini(&wait);
	m0_rpc_machine_get_stats(machine, &stats, true);
	M0_UT_ASSERT(IS_INCR_BY_1(nr_dropped_items) &&
		     IS_INCR_BY_1(nr_timedout_items) &&
		     IS_INCR_BY_1(nr_failed_items));
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	m0_rpc__item_dropped = NULL;
	M0_LOG(M0_DEBUG, "TEST:2.1:END");

	/* Test [ENQUEUED] ---timeout----> [FAILED] */
	M0_LOG(M0_DEBUG, "TEST:2.2:START");
	_test_timeout(m0_time_from_now(1, 0),
		      m0_time(0, 100 * M0_TIME_ONE_MSEC), true);
	M0_LOG(M0_DEBUG, "TEST:2.2:END");

	/* Test [URGENT] ---timeout----> [FAILED] */
	m0_fi_enable("frm_balance", "do_nothing");
	M0_LOG(M0_DEBUG, "TEST:2.3:START");
	_test_timeout(m0_time_from_now(-1, 0),
		       m0_time(0, 100 * M0_TIME_ONE_MSEC), true);
	m0_fi_disable("frm_balance", "do_nothing");
	M0_LOG(M0_DEBUG, "TEST:2.3:END");

	/* Test: [SENDING] ---timeout----> [FAILED] */

	M0_LOG(M0_DEBUG, "TEST:2.4:START");
	/* Delay "sent" callback for 300 msec. */
	m0_fi_enable("buf_send_cb", "delay_callback");
	/* ASSUMPTION: Sender will not get "new item received" event until
		       the thread that has called buf_send_cb()
		       comes out of sleep and returns to net layer.
	 */
	_test_timeout(m0_time_from_now(-1, 0),
		      m0_time(0, 100 * M0_TIME_ONE_MSEC), true);
	/* wait until reply is processed */
	m0_nanosleep(m0_time(0, 500 * M0_TIME_ONE_MSEC), NULL);
	M0_LOG(M0_DEBUG, "TEST:2.4:END");
	m0_fi_disable("buf_send_cb", "delay_callback");
	/* restore HA ops */
	m0_rpc_conn_ha_cfg_set(session->s_conn, rchc_orig);
	conn_flag_unset(session->s_conn, RCF_TRANSIENT_SENT);
}

static void _test_timeout(m0_time_t deadline,
			   m0_time_t timeout,
			   bool reset)
{
	fop = fop_alloc(machine);
	item = &fop->f_item;
	m0_rpc_machine_get_stats(machine, &saved, false);
	item->ri_nr_sent_max = 1;
	item->ri_resend_interval = timeout;
	m0_rpc_post_sync(fop, session, NULL, deadline);
	M0_UT_ASSERT(item->ri_error == -ETIMEDOUT);
	M0_UT_ASSERT(item->ri_reply == NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED));
	m0_rpc_machine_get_stats(machine, &stats, reset);
	M0_UT_ASSERT(IS_INCR_BY_1(nr_timedout_items) &&
		     IS_INCR_BY_1(nr_failed_items));
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
}

static bool only_second_time(void *data)
{
	int *ip = data;

	++*ip;
	return *ip == 2;
}

static bool drop_twice(void *data)
{
	int *ip = data;

	++*ip;
	return *ip <= 2;
}

static void _ha_do_not_notify(struct m0_rpc_conn *conn, uint8_t state)
{
}

static void _ha_notify(struct m0_rpc_conn *conn, uint8_t state)
{
	struct m0_conf_obj *svc_obj;

	M0_UT_ENTER();
	/* make sure HA is to be called with expected parameters */
	M0_UT_ASSERT(expected_state == state);
	M0_UT_ASSERT(m0_fid_eq(&expected_fid, &conn->c_svc_fid));
	/*
	 * imitate reqh_service_ha_state_set() behavior while sending nothing to
	 * HA because there is no HA environment up and running to accept a note
	 */
	conn->c_rpc_machine->rm_stats.rs_nr_ha_noted_conns++;
	svc_obj = m0_rpc_conn2svc(conn);
	M0_UT_ASSERT(svc_obj != NULL);
	svc_obj->co_ha_state = state;
	/* toggle expected state */
	expected_state = expected_state == M0_NC_TRANSIENT ?
		M0_NC_ONLINE : M0_NC_TRANSIENT;
	M0_UT_RETURN();
}

static void test_resend(void)
{
	const struct m0_rpc_conn_ha_cfg *rchc_orig = session->s_conn->c_ha_cfg;
	struct m0_rpc_conn_ha_cfg  rchc_ut = *rchc_orig;
	struct m0_rpc_item *item;
	int                 rc;
	int                 cnt       = 0;
	struct m0_fid       sfid      = M0_FID_TINIT('s', 1, 25);
	struct m0_reqh     *reqh      = &sctx.rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	struct m0_rconfc   *cl_rconfc = &cctx.rcx_reqh.rh_rconfc;

	rc = m0_rconfc_init(cl_rconfc, m0_reqh2profile(reqh),
			    m0_locality0_get()->lo_grp, machine,
			    NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_file_read(M0_UT_PATH("conf.xc"), &cl_rconfc->rc_local_conf);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_start(cl_rconfc);
	rc = m0_rpc_conn_ha_subscribe(session->s_conn, &sfid);
	M0_UT_ASSERT(rc == 0);

	m0_rpc_machine_get_stats(machine, &saved, false);

	rchc_ut.rchc_ops.cho_ha_notify = _ha_notify;
	m0_rpc_conn_ha_cfg_set(session->s_conn, &rchc_ut);
	expected_state = M0_NC_TRANSIENT;
	expected_fid = sfid;

	/* Test: Request is dropped. */
	M0_LOG(M0_DEBUG, "TEST:3.1:START");
	m0_fi_enable_once("item_received_fi", "drop_item");
	_test_resend(NULL, true);
	M0_LOG(M0_DEBUG, "TEST:3.1:END");

	/* Test: Reply is dropped. */
	M0_LOG(M0_DEBUG, "TEST:3.2:START");
	m0_fi_enable_func("item_received_fi", "drop_item",
			  only_second_time, &cnt);
	_test_resend(NULL, true);
	m0_fi_disable("item_received_fi", "drop_item");
	M0_LOG(M0_DEBUG, "TEST:3.2:END");

	/* Test: ENQUEUED -> REPLIED transition.

	   Reply is delayed. On sender, request item is enqueued for
	   resending. But before formation could send the item,
	   reply is received.

	   nanosleep()s are inserted at specific points to create
	   this scenario:
	   - request is sent;
	   - the request is moved to WAITING_FOR_REPLY state;
	   - the item's timer is set to trigger after 1 sec;
	   - fault_point<"m0_rpc_reply_post", "delay_reply"> delays
	     sending reply by 1.2 sec;
	   - resend timer of request item triggers and calls
	     m0_rpc_item_send();
	   - fault_point<"m0_rpc_item_send", "advance_delay"> moves
	     deadline of request item 500ms in future, ergo the item
	     moves to ENQUEUED state when handed over to formation;
	   - receiver comes out of 1.2 sec sleep and sends reply.
	 */
	M0_LOG(M0_DEBUG, "TEST:3.3:START");
	cnt = 0;
	m0_fi_enable_func("m0_rpc_item_send", "advance_deadline",
			  only_second_time, &cnt);
	m0_fi_enable_once("m0_rpc_reply_post", "delay_reply");
	fop = fop_alloc(machine);
	item = &fop->f_item;
	_test_resend(fop, true);
	m0_fi_disable("m0_rpc_item_send", "advance_deadline");
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	M0_LOG(M0_DEBUG, "TEST:3.3:END");

	M0_LOG(M0_DEBUG, "TEST:3.4:START");
	/* CONTINUES TO USE fop/item FROM PREVIOUS TEST-CASE. */
	/* RPC call is complete i.e. item is in REPLIED state.
	   Explicitly resend the completed request; the way the item
	   will be resent during recovery.
	 */
	m0_rpc_machine_lock(item->ri_rmachine);
	M0_UT_ASSERT(item->ri_nr_sent == 2);
	item->ri_resend_interval = M0_TIME_NEVER;
	m0_rpc_item_get(item);
	m0_rpc_item_send(item);
	m0_rpc_machine_unlock(item->ri_rmachine);
	rc = m0_rpc_item_wait_for_reply(item, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_error == 0);
	M0_UT_ASSERT(item->ri_nr_sent == 3);
	M0_UT_ASSERT(item->ri_reply != NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_REPLIED));
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:3.4:END");

	/* Test: INITIALISED -> FAILED transition when m0_rpc_post()
		 fails to start item timer.
	 */
	M0_LOG(M0_DEBUG, "TEST:3.5.1:START");
	m0_fi_enable_once("m0_rpc_item_timer_start", "failed");
	_test_timer_start_failure();
	M0_LOG(M0_DEBUG, "TEST:3.5.1:END");

	/* Test: Move item from WAITING_FOR_REPLY to FAILED state if
		 item_sent() fails to start resend timer.
	 */
	M0_LOG(M0_DEBUG, "TEST:3.5.2:START");
	cnt = 0;
	m0_fi_enable_func("m0_rpc_item_timer_start", "failed",
			  only_second_time, &cnt);
	m0_fi_enable("item_received_fi", "drop_item");
	_test_timer_start_failure();
	m0_fi_disable("item_received_fi", "drop_item");
	m0_fi_disable("m0_rpc_item_timer_start", "failed");
	m0_rpc_machine_get_stats(machine, &stats, false);
	/*
	 * Check that HA was notified about the delay for a response from the
	 * service on the rpc item.
	 */
	M0_UT_ASSERT(saved.rs_nr_ha_timedout_items <
		     stats.rs_nr_ha_timedout_items);
	M0_UT_ASSERT(saved.rs_nr_ha_noted_conns <
		     stats.rs_nr_ha_noted_conns);
	M0_LOG(M0_DEBUG, "TEST:3.5.2:END");
	/* restore HA configuration */
	m0_rpc_conn_ha_cfg_set(session->s_conn, rchc_orig);
	/* clean up */
	m0_rpc_conn_ha_unsubscribe(session->s_conn);
	M0_UT_ASSERT(session->s_conn->c_ha_clink.cl_chan == NULL);
	m0_rconfc_stop_sync(cl_rconfc);
	m0_rconfc_fini(cl_rconfc);
	conn_flag_unset(session->s_conn, RCF_TRANSIENT_SENT);
}

static void _test_resend(struct m0_fop *fop, bool post_sync)
{
	bool fop_put_flag = false;
	int rc;

	if (fop == NULL) {
		fop = fop_alloc(machine);
		fop_put_flag = true;
	}
	item = &fop->f_item;
	if (post_sync) {
		rc = m0_rpc_post_sync(fop, session, NULL, 0 /* urgent */);
	}
	else {
		item->ri_session = session;
		item->ri_deadline = 0;
		rc = m0_rpc_post(item);
	}
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_error == 0);
	M0_UT_ASSERT(item->ri_nr_sent >= 1);
	if (post_sync) {
		M0_UT_ASSERT(item->ri_reply != NULL);
		M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_REPLIED));
	}
	if (fop_put_flag && post_sync) {
		M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
		m0_fop_put_lock(fop);
	}
}

static void _test_timer_start_failure(void)
{
	int rc;

	fop = fop_alloc(machine);
	item = &fop->f_item;
	rc = m0_rpc_post_sync(fop, session, NULL, 0 /* urgent */);
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(item->ri_error == -EINVAL);
	M0_UT_ASSERT(item->ri_reply == NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED));
	/* sleep until request reaches at server and is dropped */
	m0_nanosleep(m0_time(0, 5 * 1000 * 1000), NULL);
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
}

static void test_failure_before_sending(void)
{
	int rc;
	int i;
	struct /* anonymous */ {
		const char *func;
		const char *tag;
		int         rc;
	} fp[] = {
		{"m0_bufvec_alloc_aligned", "oom",        -ENOMEM},
		{"m0_net_buffer_register",  "fake_error", -EINVAL},
		{"m0_rpc_packet_encode",    "fake_error", -EFAULT},
		{"m0_net_buffer_add",       "fake_error", -EMSGSIZE},
	};

	/* TEST4: packet_ready() routine failed.
		  The item should move to FAILED state.
	 */
	for (i = 0; i < ARRAY_SIZE(fp); ++i) {
		M0_LOG(M0_DEBUG, "TEST:4.%d:START", i + 1);
		m0_fi_enable_once(fp[i].func, fp[i].tag);
		rc = _test();
		M0_UT_ASSERT(rc == fp[i].rc);
		M0_UT_ASSERT(item_rc == fp[i].rc);
		M0_LOG(M0_DEBUG, "TEST:4.%d:END", i + 1);
		m0_fop_put_lock(fop);
	}
	/* TEST5: Network layer reported buffer send failure.
		  The item should move to FAILED state.
		  NOTE: Buffer sending is successful, hence we need
		  to explicitly drop the item on receiver using
		  fault_point<"item_received_fi", "drop_item">.
	 */
	M0_LOG(M0_DEBUG, "TEST:5:START");
	m0_fi_enable("buf_send_cb", "fake_err");
	m0_fi_enable("item_received_fi", "drop_item");
	rc = _test();
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(item_rc == -EINVAL);
	m0_rpc_machine_get_stats(machine, &stats, false);
	m0_fi_disable("buf_send_cb", "fake_err");
	m0_fi_disable("item_received_fi", "drop_item");
	M0_LOG(M0_DEBUG, "TEST:5:END");
	m0_fop_put_lock(fop);
}

static int _test(void)
{
	int rc;

	/* Check SENDING -> FAILED transition */
	m0_rpc_machine_get_stats(machine, &saved, false);
	fop  = fop_alloc(machine);
	item = &fop->f_item;
	rc = m0_rpc_post_sync(fop, session, &cs_ds_req_fop_rpc_item_ops,
			      0 /* deadline */);
	M0_UT_ASSERT(item->ri_reply == NULL);
	item_rc = item->ri_error;
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED));
	m0_rpc_machine_get_stats(machine, &stats, false);
	M0_UT_ASSERT(IS_INCR_BY_1(nr_failed_items));
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	return rc;
}

static bool arrow_sent_cb_called = false;
static void arrow_sent_cb(struct m0_rpc_item *item)
{
	arrow_sent_cb_called = true;
}
static const struct m0_rpc_item_ops arrow_item_ops = {
	.rio_sent = arrow_sent_cb,
};

static bool fop_release_called;
static void fop_release(struct m0_ref *ref)
{
	fop_release_called = true;
	m0_fop_release(ref);
}

static void test_oneway_item(void)
{
	struct m0_rpc_item *item;
	struct m0_fop      *fop;
	bool                ok;
	int                 rc;

	/* Test 1: Confirm one-way items reach receiver */
	M0_LOG(M0_DEBUG, "TEST:6.1:START");
	fop = m0_fop_alloc(&m0_rpc_arrow_fopt, NULL, machine);
	M0_UT_ASSERT(fop != NULL);

	item              = &fop->f_item;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = 0;
	item->ri_ops      = &arrow_item_ops;
	M0_UT_ASSERT(!arrow_sent_cb_called);
	m0_rpc_oneway_item_post(&cctx.rcx_connection, item);
	rc = m0_rpc_item_timedwait(&fop->f_item,
				   M0_BITS(M0_RPC_ITEM_SENT,
					   M0_RPC_ITEM_FAILED),
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_ASSERT(item->ri_sm.sm_state == M0_RPC_ITEM_SENT);
	M0_UT_ASSERT(arrow_sent_cb_called);

	ok = m0_semaphore_timeddown(&arrow_hit, m0_time_from_now(5, 0));
	M0_UT_ASSERT(ok);

	ok = m0_semaphore_timeddown(&arrow_destroyed, m0_time_from_now(5, 0));
	M0_UT_ASSERT(ok);

	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:6.1:END");

	/* Test 2: Remaining queued oneway items are dropped during
		   m0_rpc_frm_fini()
	 */
	M0_LOG(M0_DEBUG, "TEST:6.2:START");
	M0_ALLOC_PTR(fop);
	M0_UT_ASSERT(fop != NULL);
	m0_fop_init(fop, &m0_rpc_arrow_fopt, NULL, fop_release);
	rc = m0_fop_data_alloc(fop);
	M0_UT_ASSERT(rc == 0);
	item              = &fop->f_item;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = m0_time_from_now(10, 0);
	item->ri_ops      = &arrow_item_ops;
	arrow_sent_cb_called = fop_release_called = false;
	m0_rpc_oneway_item_post(&cctx.rcx_connection, item);
	m0_fop_put_lock(fop);
	M0_UT_ASSERT(!arrow_sent_cb_called);
	M0_UT_ASSERT(!fop_release_called);
	m0_fi_enable("frm_fill_packet", "skip_oneway_items");
	/* stop client server to trigger m0_rpc_frm_fini() */
	stop_rpc_client_and_server();
	M0_UT_ASSERT(arrow_sent_cb_called); /* callback with FAILED items */
	M0_UT_ASSERT(fop_release_called);
	start_rpc_client_and_server();
	m0_fi_disable("frm_fill_packet", "skip_oneway_items");
	M0_LOG(M0_DEBUG, "TEST:6.2:END");
}

/*
static void rply_before_sentcb(void)
{
	@todo Simulate a case where:
		- Request item A is serialised in network buffer NB_A;
		- NB_A is submitted to net layer;
		- A is in SENDING state;
		- NB_A.sent() callback is not yet received;
		- And reply to A is received.
	     In this case reply processing of A should be postponed until
	     NB_A.sent() callback is invoked.

	     Tried to simulate this case, by introducing artificial delay in
	     buf_send_cb(). But because there is only one thread from lnet
	     transport that delivers buffer events, it also blocks delivery of
	     net_buf_receieved(A.reply) event.
}
*/

bool REINITIALISE_AFTER_CANCEL = true;
bool ALREADY_REPLIED           = true;

void fop_test(int expected_rc)
{
	int rc;

	fop = fop_alloc(machine);
	item              = &fop->f_item;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = m0_time_from_now(0, 0);
	rc = m0_rpc_post_sync(fop, session, NULL, 0 /* deadline */);
	if (expected_rc == 0) {
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(item->ri_error == 0);
		M0_UT_ASSERT(item->ri_reply != NULL);
		M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_REPLIED));
	} else {
		M0_UT_ASSERT(rc == -ECANCELED);
		M0_UT_ASSERT(rc == expected_rc);
		M0_UT_ASSERT(item->ri_error == expected_rc);
		M0_UT_ASSERT(item->ri_reply == NULL);
		M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED));
	}
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
}

static void check_cancel(bool already_replied, bool reinitialise)
{
	uint64_t xid = item->ri_header.osr_xid;
	int      rc;

	M0_UT_ASSERT(m0_rpc_item_is_request(item));

	if (reinitialise)
		m0_rpc_item_cancel_init(item);
	else {
		m0_rpc_item_cancel(item);
		if (already_replied) {
			/* Item already replied. Hence, not cancelled. */
			M0_UT_ASSERT(item->ri_error == 0);
			M0_UT_ASSERT(item->ri_reply != NULL);
			return;
		}
	}

	/* Verify that the item was indeed cancelled. */
	M0_UT_ASSERT(item->ri_reply == NULL);
	if (reinitialise) {
		M0_UT_ASSERT(item->ri_error == 0);
		M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_UNINITIALISED));

		/* Re-post the item that was re-initialised. */
		rc = m0_rpc_post_sync(fop, session, &cs_ds_req_fop_rpc_item_ops,
				      0 /* deadline */);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(item->ri_error == 0);
		M0_UT_ASSERT(item->ri_header.osr_xid != xid);
		M0_UT_ASSERT(item->ri_reply != NULL);
		M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_REPLIED) &&
			     chk_state(item->ri_reply, M0_RPC_ITEM_ACCEPTED));
	} else {
		M0_UT_ASSERT(item->ri_error == -ECANCELED);
		M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED));
	}
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
}

static void cancel_item_with_various_states(bool reinitialise)
{
	int rc;
	int sub_tc = reinitialise ? 1 : 2;

	M0_LOG(M0_DEBUG, "TEST:7:%d:START", sub_tc);

	/*
	 * Cancel item that is already replied.
	 * In this case, m0_rpc_item_cancel() is a no-op.
	 */
	M0_LOG(M0_DEBUG, "TEST:7:%d:1:START", sub_tc);
	fop = fop_alloc(machine);
	item = &fop->f_item;

	rc = m0_rpc_post_sync(fop, session, &cs_ds_req_fop_rpc_item_ops,
			      0 /* deadline */);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_error == 0);
	M0_UT_ASSERT(item->ri_reply != NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_REPLIED) &&
		     chk_state(item->ri_reply, M0_RPC_ITEM_ACCEPTED));
	m0_fi_enable_once("item_cancel_fi", "cancel_replied_item");
	check_cancel(ALREADY_REPLIED, reinitialise);
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:7:%d:1:END", sub_tc);

	/* Cancel item while in formation. */
	M0_LOG(M0_DEBUG, "TEST:7:%d:2:START", sub_tc);
	fop = fop_alloc(machine);
	item              = &fop->f_item;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = m0_time_from_now(50, 0);
	rc = m0_rpc_post(item);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_reply == NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_ENQUEUED));
	m0_fi_enable_once("item_cancel_fi", "cancel_enqueued_item");
	check_cancel(!ALREADY_REPLIED, reinitialise);
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:7:%d:2:END", sub_tc);

	/* Cancel while item is in SENDING state. */
	M0_LOG(M0_DEBUG, "TEST:7:%d:3:START", sub_tc);
	m0_fi_enable("buf_send_cb", "delay_callback");
	fop = fop_alloc(machine);
	item              = &fop->f_item;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = m0_time_from_now(0, 0);
	rc = m0_rpc_post(item);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_reply == NULL);
	rc = m0_rpc_item_timedwait(item, M0_BITS(M0_RPC_ITEM_SENDING),
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable_once("item_cancel_fi", "cancel_sending_item");
	check_cancel(!ALREADY_REPLIED, reinitialise);
	m0_fi_disable("buf_send_cb", "delay_callback");
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:7:%d:3:END", sub_tc);

	/*
	 * Cancel while waiting for reply.
	 * If reply is received for this request after cancelation, then it
	 * is dropped and is recorded using a record of the kind:
	 * item_received] 0x.. [REPLY/6] dropped
	 */
	M0_LOG(M0_DEBUG, "TEST:7:%d:4:START", sub_tc);
	m0_fi_enable_once("cs_req_fop_fom_tick", "inject_delay");
	fop = fop_alloc(machine);
	item              = &fop->f_item;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = m0_time_from_now(0, 0);
	rc = m0_rpc_post(item);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_item_timedwait(item, M0_BITS(M0_RPC_ITEM_WAITING_FOR_REPLY),
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(item->ri_reply == NULL);
	m0_fi_enable_once("item_cancel_fi", "cancel_waiting_for_reply_item");
	check_cancel(!ALREADY_REPLIED, reinitialise);
	M0_UT_ASSERT(m0_ref_read(&fop->f_ref) == 1);
	m0_fop_put_lock(fop);
	M0_LOG(M0_DEBUG, "TEST:7:%d:4:END", sub_tc);

	M0_LOG(M0_DEBUG, "TEST:7:%d:END", sub_tc);
}

static void test_cancel_item(void)
{
	/*
	 * Cancel item along with sending 'reinitialise = true' to
	 * m0_rpc_item_cancel().
	 */
	cancel_item_with_various_states(REINITIALISE_AFTER_CANCEL);

	/*
	 * Cancel item along with sending 'reinitialise = false' to
	 * m0_rpc_item_cancel().
	 */
	cancel_item_with_various_states(!REINITIALISE_AFTER_CANCEL);
}

uint32_t fop_dispatched_nr = 0;
/*
 * This is to help keep track of how many fops issued around session
 * cancelation have been taken to completion, may it be with success
 * or failure.
 */
static void session_ut_item_cb(struct m0_rpc_item *item)
{
	--fop_dispatched_nr;
}

static const struct m0_rpc_item_ops session_ut_item_ops = {
        .rio_replied = session_ut_item_cb,
};

static void test_cancel_session(void)
{
	uint32_t       fop_nr = 10;
	uint32_t       fop_cancelled_nr = 0;
	struct m0_fop *fop_arr[fop_nr];
	uint32_t       i;
	int            rc;

	/*
	 * Cancel rpc session while it may have items in various states like
	 * ENQUEUED, URGENT, SENDING, SENT or WAITING_FOR_REPLY. Replies are
	 * dropped for the items applicable.
	 */
	M0_LOG(M0_DEBUG, "TEST:8:1:START");
	m0_fi_enable("item_received_fi", "drop_item_reply");
	for (i = 0; i < fop_nr; ++i) {
		fop = fop_alloc(machine);
		item               = &fop->f_item;
		item->ri_session   = session;
		item->ri_prio      = M0_RPC_ITEM_PRIO_MID;
		item->ri_deadline  = m0_time_from_now(0, 0);
		fop->f_item.ri_ops = &session_ut_item_ops;
		rc = m0_rpc_post(item);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(item->ri_reply == NULL);
		fop_arr[i] = fop;
		++fop_dispatched_nr;
	}

	M0_LOG(M0_DEBUG, "TEST:8:1:2: cancel session");
	m0_rpc_session_cancel(session);
	M0_UT_ASSERT(fop_dispatched_nr == 0);

	for (i = 0; i < fop_nr; ++i) {
		item = &fop_arr[i]->f_item;
		M0_UT_ASSERT(m0_ref_read(&fop_arr[i]->f_ref) == 1);
		M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED) ||
			     chk_state(item, M0_RPC_ITEM_REPLIED));
		if (chk_state(item, M0_RPC_ITEM_FAILED)) {
			M0_UT_ASSERT(item->ri_error == -ECANCELED);
			++fop_cancelled_nr;
		}
		m0_fop_put_lock(fop_arr[i]);
	}
	M0_UT_ASSERT(fop_cancelled_nr > 0);

	/*
	 * Post a fop to verify that it gets cancelled while in the INITIALISED
	 * state, the session being cancelled.
	 */
	fop_test(-ECANCELED);
	m0_fi_disable("item_received_fi", "drop_item_reply");

	M0_LOG(M0_DEBUG, "TEST:8:1:2: restore session");
	/*
	 * In production scenario, session will be restored through service
	 * reconnect. It being UT, simply, destroying and recreating the
	 * session.
	 */
	M0_UT_ASSERT(session->s_cancelled == true);
	M0_UT_ASSERT(session->s_xid > 0);
	rc = m0_rpc_session_destroy(session, m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_session_create(session, cctx.rcx_session.s_conn,
				   m0_time_from_now(TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(session->s_cancelled == false);
	M0_UT_ASSERT(session->s_xid == 0);

	/*
	 * Post a fop successfully to verify that the session has been restored.
	 * This also ensures that 'the items received on the receiver which have
	 * been cancelled on the sender side' have been taken to completion.
	 * Otherwise, stop_rpc_client_and_server() may hang in a rare case.
	 */
	fop_test(0);

	M0_LOG(M0_DEBUG, "TEST:8:1:END");
}

enum {
	M0_RPC_ITEM_CACHE_ITEMS_NR_MAX = 0x40,
};

static uint64_t test_item_cache_item_get_xid = UINT64_MAX - 1;
static uint64_t test_item_cache_item_put_xid = UINT64_MAX - 1;

static void test_item_cache_item_get(struct m0_rpc_item *item)
{
	M0_UT_ASSERT(test_item_cache_item_get_xid == UINT64_MAX ||
		     item->ri_header.osr_xid ==
		     test_item_cache_item_get_xid);
	test_item_cache_item_get_xid = UINT64_MAX - 1;
}

static void test_item_cache_item_put(struct m0_rpc_item *item)
{
	M0_UT_ASSERT(test_item_cache_item_put_xid == UINT64_MAX ||
		     item->ri_header.osr_xid ==
		     test_item_cache_item_put_xid);
	test_item_cache_item_put_xid = UINT64_MAX - 1;
}

extern const struct m0_sm_conf outgoing_item_sm_conf;
extern const struct m0_sm_conf incoming_item_sm_conf;

static struct m0_rpc_item_type_ops test_item_cache_type_ops = {
	.rito_item_get = test_item_cache_item_get,
	.rito_item_put = test_item_cache_item_put,
};
static struct m0_rpc_item_type test_item_cache_itype = {
	.rit_ops           = &test_item_cache_type_ops,
};

/*
 * Add each nth item to the cache.
 * Lookup each item in cache.
 * Then remove each item from the cache.
 */
static void test_item_cache_add_nth(struct m0_rpc_item_cache *ic,
				    struct m0_mutex	     *lock,
				    struct m0_rpc_item	     *items,
				    int			      items_nr,
				    int			      n)
{
	struct m0_rpc_item *item;
	int		    added_nr;
	int		    test_nr;
	int		    i;

	M0_SET0(ic);
	m0_rpc_item_cache_init(ic, lock);
	added_nr = 0;
	for (i = 0; i < items_nr; ++i) {
		if ((i % n) == 0) {
			test_item_cache_item_get_xid = i;
			m0_rpc_item_cache_add(ic, &items[i], M0_TIME_NEVER);
			++added_nr;
		}
	}
	/* no-op */
	m0_rpc_item_cache_purge(ic);
	for (i = 0; i < items_nr; ++i) {
		/* do nothing */
		if ((i % n) == 0)
			m0_rpc_item_cache_add(ic, &items[i], M0_TIME_NEVER);
	}
	test_nr = 0;
	for (i = 0; i < items_nr; ++i) {
		item = m0_rpc_item_cache_lookup(ic, i);
		/* m0_rpc_item_cache_lookup() returns either NULL or item */
		M0_UT_ASSERT(item == NULL || item == &items[i]);
		M0_UT_ASSERT(equi(item != NULL, (i % n) == 0));
		test_nr += item != NULL;
	}
	M0_UT_ASSERT(test_nr == added_nr);
	for (i = 0; i < items_nr; ++i) {
		if ((i % n) == 0)
			test_item_cache_item_put_xid = i;
		m0_rpc_item_cache_del(ic, i);
	}
	/* cache is empty now */
	/* do nothing */
	m0_rpc_item_cache_clear(ic);
	for (i = 0; i < items_nr; ++i) {
		item = m0_rpc_item_cache_lookup(ic, i);
		M0_UT_ASSERT(item == NULL);
	}
	m0_rpc_item_cache_fini(ic);
}

static void test_item_cache(void)
{
	struct m0_rpc_item_cache  ic = {};
	struct m0_rpc_machine     rmach = {};
	struct m0_rpc_item       *items;
	struct m0_mutex           lock = {};
	int                       items_nr;
	int                       n;
	int                       i;

	M0_ALLOC_ARR(items, M0_RPC_ITEM_CACHE_ITEMS_NR_MAX);
	M0_UT_ASSERT(items != NULL);
	for (i = 0; i < M0_RPC_ITEM_CACHE_ITEMS_NR_MAX; ++i) {
		m0_rpc_item_init(&items[i], &test_item_cache_itype);
		items[i].ri_header.osr_xid = i;
		items[i].ri_rmachine = &rmach;
	}
	m0_mutex_init(&lock);
	m0_mutex_lock(&lock);
	/*
	 * This is needed because m0_rpc_item_put() checks rpc machine lock.
	 */
	m0_mutex_init(&rmach.rm_sm_grp.s_lock);
	m0_mutex_lock(&rmach.rm_sm_grp.s_lock);
	for (items_nr = 1;
	     items_nr < M0_RPC_ITEM_CACHE_ITEMS_NR_MAX; ++items_nr) {
		for (n = 1; n <= items_nr; ++n)
			test_item_cache_add_nth(&ic, &lock, items, items_nr, n);
	}
	m0_mutex_unlock(&rmach.rm_sm_grp.s_lock);
	m0_mutex_fini(&rmach.rm_sm_grp.s_lock);
	m0_mutex_unlock(&lock);
	m0_mutex_fini(&lock);
	for (i = 0; i < M0_RPC_ITEM_CACHE_ITEMS_NR_MAX; ++i)
		m0_rpc_item_fini(&items[i]);
	m0_free(items);
}

static struct m0_thread ha_thread = {0};
m0_chan_cb_t rpc_conn_original_ha_cb = NULL;

void __ha_accept_imitate(struct m0_fid *sfid)
{
	struct m0_reqh     *reqh  = &sctx.rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	struct m0_confc    *confc = m0_reqh2confc(reqh);
	struct m0_rconfc   *cl_rconfc = &cctx.rcx_reqh.rh_rconfc;
	struct m0_conf_obj *obj;

	M0_ENTRY("fid "FID_F, FID_P(sfid));
	m0_nanosleep(m0_time(0,
			     session->s_conn->c_ha_cfg->rchc_ha_interval / 2),
			     NULL);
	/* Update HA state of the service in client cache and server cache */
	obj = m0_conf_cache_lookup(&confc->cc_cache, sfid);
	M0_UT_ASSERT(obj != NULL);
	obj->co_ha_state = M0_NC_FAILED;
	m0_chan_broadcast_lock(&obj->co_ha_chan);
	obj = m0_conf_cache_lookup(&cl_rconfc->rc_confc.cc_cache, sfid);
	M0_UT_ASSERT(obj != NULL);
	obj->co_ha_state = M0_NC_FAILED;
	m0_chan_broadcast_lock(&obj->co_ha_chan);
	M0_UT_RETURN("broadcast done");
}

static void __ha_timer__dummy(struct m0_sm_timer *timer)
{
	struct m0_rpc_conn *conn;
	struct m0_conf_obj *obj;

	M0_UT_ENTER();
	conn = container_of(timer, struct m0_rpc_conn, c_ha_timer);
	M0_ASSERT(conn->c_magic == M0_RPC_CONN_MAGIC);
	obj = m0_rpc_conn2svc(conn);
	M0_LOG(M0_DEBUG, "obj = %p, fid "FID_F, obj, FID_P(&obj->co_id));
	M0_UT_ASSERT(obj->co_ha_state == M0_NC_FAILED);
	m0_semaphore_up(&wait);
	M0_UT_RETURN();
}

static bool __ha_service_event(struct m0_clink *link)
{
	bool rc;

	M0_UT_ENTER();
	rc = rpc_conn_original_ha_cb(link);
	M0_UT_LOG("rc = %d", rc);
	M0_UT_RETURN();
	return rc;
}

static void test_ha_cancel(void)
{
	const struct m0_rpc_conn_ha_cfg *rchc_orig = session->s_conn->c_ha_cfg;
	struct m0_rpc_conn_ha_cfg  rchc_ut = *rchc_orig;
	struct m0_reqh     *reqh  = &sctx.rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	struct m0_confc    *confc = &reqh->rh_rconfc.rc_confc;
	struct m0_fid       sfid  = M0_FID_TINIT('s', 1, 25);
	struct m0_rconfc   *cl_rconfc = &cctx.rcx_reqh.rh_rconfc;
	struct m0_conf_obj *obj;
	int                 rc;

	rc = m0_rconfc_init(cl_rconfc, m0_reqh2profile(reqh),
			    m0_locality0_get()->lo_grp, machine,
			    NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_file_read(M0_UT_PATH("conf.xc"), &cl_rconfc->rc_local_conf);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_start(cl_rconfc);

	/*
	 * Re-initiate rpc conn subscription to HA notes. This will replace
	 * original ha clink callback with the local one. We need to follow
	 * work flow, but take detours for the sake of test passage.
	 */
	rpc_conn_original_ha_cb = session->s_conn->c_ha_clink.cl_cb;
	m0_clink_fini(&session->s_conn->c_ha_clink);
	m0_clink_init(&session->s_conn->c_ha_clink, __ha_service_event);
	/*
	 * We are going to imitate service death notification before connection
	 * getting timed out. Need to intercept standard item's ha timer
	 * callback to prevent sending temporary failure status to HA as we have
	 * no real HA environment running
	 */
	rchc_ut.rchc_ops.cho_ha_timer_cb = __ha_timer__dummy;
	m0_rpc_conn_ha_cfg_set(session->s_conn, &rchc_ut);
	rc = m0_rpc_conn_ha_subscribe(session->s_conn, &sfid);
	M0_UT_ASSERT(rc == 0);
	/* imitate external HA note acceptance */
	rc = M0_THREAD_INIT(&ha_thread, struct m0_fid *, NULL,
			    &__ha_accept_imitate, &sfid, "death_note");
	M0_UT_ASSERT(rc == 0);

	/* send fop with delay enabled */
	fop = fop_alloc(machine);
	item = &fop->f_item;
	item->ri_nr_sent_max = 2;
	m0_rpc_machine_get_stats(machine, &saved, false);
	m0_fi_enable_once("cs_req_fop_fom_tick", "inject_delay");
	m0_semaphore_init(&wait, 0);
	M0_UT_LOG("posting item = %p", item);
	rc = m0_rpc_post_sync(fop, session, &cs_ds_req_fop_rpc_item_ops,
			      0 /* deadline */);
	M0_UT_LOG("done with posting");
	M0_UT_ASSERT(rc == -ECANCELED);
	M0_UT_ASSERT(item->ri_error == -ECANCELED);
	M0_UT_ASSERT(item->ri_reply == NULL);
	M0_UT_ASSERT(chk_state(item, M0_RPC_ITEM_FAILED));
	m0_semaphore_down(&wait);
	m0_semaphore_fini(&wait);
	m0_fop_put_lock(fop);
	/* restore HA ops */
	m0_rpc_conn_ha_cfg_set(session->s_conn, rchc_orig);
	/* recover connection */
	rc = m0_rpc_client_stop(&cctx);
	M0_UT_ASSERT(rc == -ECANCELED);
	m0_rconfc_stop_sync(cl_rconfc);
	m0_rconfc_fini(cl_rconfc);
	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);
	/* recover service object */
	obj = m0_conf_cache_lookup(&confc->cc_cache, &sfid);
	M0_UT_ASSERT(obj != NULL);
	obj->co_ha_state = M0_NC_ONLINE;
}

static void test_ha_notify()
{
	const struct m0_rpc_conn_ha_cfg *rchc_orig = session->s_conn->c_ha_cfg;
	struct m0_rpc_conn_ha_cfg  rchc_ut = *rchc_orig;
	struct m0_fid       sfid      = M0_FID_TINIT('s', 1, 25);
	struct m0_reqh     *reqh      = &sctx.rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	struct m0_rconfc   *cl_rconfc = &cctx.rcx_reqh.rh_rconfc;
	struct m0_fop      *fop2;
	int                 cnt       = 0;
	int                 rc;

	rc = m0_rconfc_init(cl_rconfc, m0_reqh2profile(reqh),
			    m0_locality0_get()->lo_grp, machine,
			    NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_file_read(M0_UT_PATH("conf.xc"), &cl_rconfc->rc_local_conf);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start(cl_rconfc);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_conn_ha_subscribe(session->s_conn, &sfid);
	M0_UT_ASSERT(rc == 0);

	rchc_ut.rchc_ops.cho_ha_notify = _ha_notify;
	m0_rpc_conn_ha_cfg_set(session->s_conn, &rchc_ut);
	expected_state = M0_NC_TRANSIENT;
	expected_fid = sfid;

	m0_rpc_machine_get_stats(machine, &saved, false);
	/* Test: one item resend, HA must be notified */
	M0_LOG(M0_DEBUG, "TEST:3.1:START");
	m0_fi_enable_once("item_received_fi", "drop_item");
	_test_resend(NULL, true);
	m0_rpc_machine_get_stats(machine, &stats, true);
	/*
	 * rs_nr_ha_noted_conns equals to 2 because the scenario is as follows:
	 * 1. item_resend() is triggered.
	 * 2. Notify HA about M0_NC_TRANSIENT state.
	 * 3. The reply is received after one resend.
	 * 4. Notify HA about M0_NC_ONLINE state.
	 */
	M0_UT_ASSERT(IS_INCR_BY_N(nr_ha_noted_conns, 2) &&
		     IS_INCR_BY_1(nr_resent_items));
	M0_LOG(M0_DEBUG, "TEST:3.1:END");

	m0_rpc_machine_get_stats(machine, &saved, false);
	/*
	 * Test: an item is resent twice, but HA must be notified only once
	 * about M0_NC_TRANSIENT state and M0_NC_ONLINE state when the reply is
	 * received.
	 */
	M0_LOG(M0_DEBUG, "TEST:3.2:START");
	m0_fi_enable_func("item_received_fi", "drop_item",
			  drop_twice, &cnt);
	_test_resend(NULL, true);
	m0_fi_disable("item_received_fi", "drop_item");
	m0_rpc_machine_get_stats(machine, &stats, true);
	M0_UT_ASSERT(IS_INCR_BY_N(nr_ha_noted_conns, 2) &&
		     IS_INCR_BY_N(nr_resend_attempts, 2));
	M0_LOG(M0_DEBUG, "TEST:3.2:END");

	m0_rpc_machine_get_stats(machine, &saved, false);
	/*
	 * Test: two items are resent, but HA must be notified only once about
	 * M0_NC_TRANSIENT state and M0_NC_ONLINE state when the reply is
	 * received.
	 */
	M0_LOG(M0_DEBUG, "TEST:3.3:START");
	cnt = 0;
	m0_fi_enable_func("item_received_fi", "drop_item",
			  drop_twice, &cnt);
	fop = fop_alloc(machine);
	fop2 = fop_alloc(machine);
	_test_resend(fop, false);
	_test_resend(fop2, false);
	rc = m0_rpc_item_wait_for_reply(&fop->f_item, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_item_wait_for_reply(&fop2->f_item, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	m0_fi_disable("item_received_fi", "drop_item");
	M0_UT_ASSERT(fop->f_item.ri_nr_sent == 2);
	M0_UT_ASSERT(fop2->f_item.ri_nr_sent == 2);
	m0_fop_put_lock(fop);
	m0_fop_put_lock(fop2);
	m0_rpc_machine_get_stats(machine, &stats, true);
	M0_UT_ASSERT(IS_INCR_BY_N(nr_ha_noted_conns, 2) &&
		     IS_INCR_BY_N(nr_resent_items, 2));
	M0_LOG(M0_DEBUG, "TEST:3.3:END");
	/*
	 * Test: item_timeout() occurs, notify HA about M0_NC_TRANSIENT state.
	 */
	m0_rpc_machine_get_stats(machine, &saved, false);
	_test_timeout(m0_time_from_now(1, 0),
		      m0_time(0, 100 * M0_TIME_ONE_MSEC), false);
	while (m0_sm_timer_is_armed(&session->s_conn->c_ha_timer))
		m0_nanosleep(M0_TIME_ONE_MSEC, NULL);
	m0_rpc_machine_get_stats(machine, &stats, false);
	M0_UT_ASSERT(IS_INCR_BY_1(nr_ha_noted_conns));
	_test_timeout(m0_time_from_now(1, 0),
		      m0_time(0, 100 * M0_TIME_ONE_MSEC), false);
	while (m0_sm_timer_is_armed(&session->s_conn->c_ha_timer))
		m0_nanosleep(M0_TIME_ONE_MSEC, NULL);
	m0_rpc_machine_get_stats(machine, &stats, true);
	M0_UT_ASSERT(saved.rs_nr_ha_noted_conns == stats.rs_nr_ha_noted_conns);
	/*
	 * Report about M0_NC_ONLINE state if a reply was received for another
	 * item after timeout happens.
	 */
	m0_rpc_machine_get_stats(machine, &saved, false);
	fop = fop_alloc(machine);
	rc = m0_rpc_post_sync(fop, session, NULL, 0 /* urgent */);
	M0_UT_ASSERT(rc == 0);
	m0_fop_put_lock(fop);
	m0_rpc_machine_get_stats(machine, &stats, false);
	M0_UT_ASSERT(IS_INCR_BY_1(nr_ha_noted_conns));
	/* restore HA ops */
	m0_rpc_conn_ha_cfg_set(session->s_conn, rchc_orig);
	/* clean up */
	m0_rpc_conn_ha_unsubscribe(session->s_conn);
	M0_UT_ASSERT(session->s_conn->c_ha_clink.cl_chan == NULL);
	m0_rconfc_stop_sync(cl_rconfc);
	m0_rconfc_fini(cl_rconfc);
}

struct m0_ut_suite item_ut = {
	.ts_name = "rpc-item-ut",
	.ts_init = ts_item_init,
	.ts_fini = ts_item_fini,
	.ts_tests = {
		{ "cache",		    test_item_cache		},
		{ "simple-transitions",     test_simple_transitions     },
		{ "reply-item-error",       test_reply_item_error       },
		{ "item-timeout",           test_timeout                },
		{ "item-resend",            test_resend                 },
		{ "failure-before-sending", test_failure_before_sending },
		{ "oneway-item",            test_oneway_item            },
		{ "cancel",                 test_cancel_item            },
		{ "ha-cancel",              test_ha_cancel              },
		{ "cancel-session",         test_cancel_session         },
		{ "ha-notify",              test_ha_notify              },
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
