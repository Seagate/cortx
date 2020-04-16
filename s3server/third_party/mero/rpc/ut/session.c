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
 * Original author: Madhav Vemuri<madhav_vemuri@xyratex.com>
 * Original creation date: 10/09/2012
 */

#include "ut/ut.h"
#include "lib/mutex.h"
#include "lib/finject.h"
#include "fop/fop.h"
#include "rpc/rpc_internal.h"

enum {
	SENDER_ID  = 1001,
	SESSION_ID = 101,
};

static struct m0_rpc_machine machine;
static struct m0_rpc_conn    conn;
static struct m0_rpc_session session;
static struct m0_rpc_session session0;

/* This structure defination is copied from rpc/session.c. */
static struct fop_session_establish_ctx {
	/** A fop instance of type m0_rpc_fop_session_establish_fopt */
	struct m0_fop          sec_fop;
	/** sender side session object */
	struct m0_rpc_session *sec_session;
} est_ctx;

static struct m0_fop est_fop_rep;
static struct m0_fop term_fop;
static struct m0_fop term_fop_rep;

struct m0_rpc_fop_session_establish     est;
struct m0_rpc_fop_session_establish_rep est_reply;
struct m0_rpc_fop_session_terminate     term;
struct m0_rpc_fop_session_terminate_rep term_reply;

static void fop_set_session0(struct m0_fop *fop)
{
	fop->f_item.ri_session = m0_rpc_conn_session0(&conn);
}

static int session_ut_init(void)
{
	int rc;

	conn.c_rpc_machine = &machine;
	conn.c_sender_id   = SENDER_ID;
	rpc_session_tlist_init(&conn.c_sessions);
	rmach_watch_tlist_init(&machine.rm_watch);

	m0_sm_group_init(&machine.rm_sm_grp);
	rc = m0_rpc_session_init(&session0, &conn);
	M0_ASSERT(rc == 0);
	session0.s_session_id = SESSION_ID_0;

	est_ctx.sec_fop.f_item.ri_reply = &est_fop_rep.f_item;
	term_fop.f_item.ri_reply        = &term_fop_rep.f_item;

	fop_set_session0(&est_ctx.sec_fop);
	fop_set_session0(&est_fop_rep);
	fop_set_session0(&term_fop);
	fop_set_session0(&term_fop_rep);

	m0_fi_enable("m0_rpc__fop_post", "do_nothing");
	return 0;
}

static int session_ut_fini(void)
{
	session0.s_session_id = SESSION_ID_INVALID;
	m0_rpc_session_fini(&session0);
	rmach_watch_tlist_fini(&machine.rm_watch);
	rpc_session_tlist_fini(&conn.c_sessions);
	m0_sm_group_fini(&machine.rm_sm_grp);
	m0_fi_disable("m0_rpc__fop_post", "do_nothing");
	m0_fi_disable("m0_rpc__fop_post", "fake_error");
	m0_fi_disable("m0_alloc", "fail_allocation");
	return 0;
}

static void session_init(void)
{
	int rc;

	rc = m0_rpc_session_init(&session, &conn);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_INITIALISED);
}

static void session_init_fini_test(void)
{
	session_init();

	m0_rpc_session_fini(&session);
}

static void prepare_fake_est_reply(void)
{
	est_ctx.sec_session             = &session;
	est_ctx.sec_fop.f_data.fd_data  = &est;
	est_ctx.sec_fop.f_item.ri_error = 0;

	est_reply.rser_session_id = SESSION_ID; /* session_id_allocate() */
	est_reply.rser_sender_id  = SENDER_ID;  /* sender_id_allocate()  */
	est_reply.rser_rc         = 0;

	est_fop_rep.f_data.fd_data  = &est_reply;
}

static void session_init_and_establish(void)
{
	int rc;

	/* Session transition from INITIALISED => ESTABLISHING */
	session_init();

	conn.c_sm.sm_state = M0_RPC_CONN_ACTIVE;
	rc = m0_rpc_session_establish(&session, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_ESTABLISHING);

	prepare_fake_est_reply();
}

static void session_establish_reply(int err)
{
	/* Session transition from ESTABLISHING => IDLE | FAILED */
	est_ctx.sec_fop.f_item.ri_error = err;
	m0_rpc_machine_lock(&machine);
	m0_rpc_session_establish_reply_received(&est_ctx.sec_fop.f_item);
	m0_rpc_machine_unlock(&machine);
}

static void prepare_fake_term_reply(void)
{
	term_fop.f_item.ri_error = 0;
	term_fop.f_data.fd_data  = &term;
	term.rst_sender_id       = SENDER_ID;
	term.rst_session_id      = SESSION_ID;

	term_reply.rstr_session_id = SESSION_ID;
	term_reply.rstr_sender_id  = SENDER_ID;
	term_reply.rstr_rc         = 0;

	term_fop_rep.f_data.fd_data  = &term_reply;
}

static void session_terminate(void)
{
	int rc;

	/* Session transition from IDLE => TERMINATING */
	rc = m0_rpc_session_terminate(&session, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_TERMINATING);

	rc = m0_rpc_session_terminate(&session, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_TERMINATING);

	prepare_fake_term_reply();
}

static void session_terminate_reply_and_fini(int err)
{
	/* Session transition from TERMINATING => TERMINATED | FAILED */
	term_fop.f_item.ri_error = err;
	m0_rpc_machine_lock(&machine);
	m0_rpc_session_terminate_reply_received(&term_fop.f_item);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(err == 0 ?
		     session_state(&session) == M0_RPC_SESSION_TERMINATED :
		     session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);
}

static void session_hold_release(void)
{
	/* Session transition from IDLE => BUSY => IDLE */
	m0_rpc_machine_lock(&machine);
	m0_rpc_session_hold_busy(&session);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_BUSY);
	m0_rpc_session_release(&session);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_IDLE);
	m0_rpc_machine_unlock(&machine);
}

static void session_check(void)
{
	/* Checks for session states transitions,
	   INITIALISED => ESTABLISHING => IDLE => BUSY => IDLE =>
	   TERMINATING => FINALISED.
	 */
	session_init_and_establish();
	session_establish_reply(0);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_IDLE);
	session_hold_release();
	session_terminate();
	session_terminate_reply_and_fini(0);
}

static void session_establish_fail_test(void)
{
	int rc;

	conn.c_sm.sm_state = M0_RPC_CONN_ACTIVE;

	/* Checks for Session state transition,
	   M0_RPC_SESSION_INITIALISED => M0_RPC_SESSION_FAILED
	 */
	session_init();

	m0_fi_enable_once("m0_rpc__fop_post", "fake_error");
	rc = m0_rpc_session_establish(&session, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);

	/* Allocation failure */
	session_init();

	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_rpc_session_establish(&session, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -ENOMEM);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);
}

static void session_establish_reply_fail_test(void)
{
	/* Checks for Session state transition,
	   M0_RPC_SESSION_ESTABLISHING => M0_RPC_SESSION_FAILED
	 */
	session_init_and_establish();

	session_establish_reply(-EINVAL);
	M0_UT_ASSERT(session.s_sm.sm_rc == -EINVAL);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);

	/* Due to invalid sender id. */
	session_init_and_establish();

	est_reply.rser_sender_id = SENDER_ID_INVALID;
	session_establish_reply(0);
	M0_UT_ASSERT(session.s_sm.sm_rc == -EPROTO);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);

	/* Due to error in establish reply fop. */
	session_init_and_establish();

	est_reply.rser_rc = -EINVAL;
	session_establish_reply(0);
	M0_UT_ASSERT(session.s_sm.sm_rc == -EINVAL);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);
}

static void session_terminate_fail_test(void)
{
	int rc;

	/* Checks for session M0_RPC_SESSION_IDLE => M0_RPC_SESSION_FAILED */
	session_init_and_establish();
	session_establish_reply(0);

	m0_fi_enable_once("m0_rpc_session_terminate", "fail_allocation");
	rc = m0_rpc_session_terminate(&session, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -ENOMEM);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);

	/* Due to m0_rpc__fop_post() failure. */
	session_init_and_establish();
	session_establish_reply(0);

	m0_fi_enable_once("m0_rpc__fop_post", "fake_error");
	rc = m0_rpc_session_terminate(&session, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(session_state(&session) == M0_RPC_SESSION_FAILED);

	m0_rpc_session_fini(&session);
}

static void session_terminate_reply_fail_test(void)
{
	/* Checks for M0_RPC_SESSION_TERMINATING => M0_RPC_SESSION_FAILED */
	session_init_and_establish();
	session_establish_reply(0);
	session_terminate();

	session_terminate_reply_and_fini(-EINVAL);
}

struct m0_ut_suite session_ut = {
	.ts_name = "rpc-session-ut",
	.ts_init = session_ut_init,
	.ts_fini = session_ut_fini,
	.ts_tests = {
		{ "session-init-fini", session_init_fini_test},
		{ "session-check", session_check},
		{ "session-establish-fail", session_establish_fail_test},
		{ "session-terminate-fail", session_terminate_fail_test},
		{ "session-establish-reply-fail", session_establish_reply_fail_test},
		{ "session-terminate_reply-fail", session_terminate_reply_fail_test},
		{ NULL, NULL}
	}
};
M0_EXPORTED(session_ut);
