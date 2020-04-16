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
 * Original author: Rohan Puri <Rohan_Puri@xyratex.com>
 *                  Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 03/17/2011
 */

#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/bitstring.h"
#include "lib/uuid.h"
#include "mero/magic.h"
#include "fop/fop.h"
#include "lib/arith.h"             /* M0_CNT_DEC */
#include "lib/finject.h"
#include "rpc/rpc_internal.h"

/**
   @addtogroup rpc_session

   @{

   This file defines functions related to m0_rpc_session.
 */

static void __session_fini(struct m0_rpc_session *session);
static void session_failed(struct m0_rpc_session *session, int32_t error);
static void session_idle_x_busy(struct m0_rpc_session *session);

/**
   Container of session_establish fop.

   Required only on sender to obtain pointer to session being established,
   when reply to session_establish is received.
 */
struct fop_session_establish_ctx {
	/** A fop instance of type m0_rpc_fop_session_establish_fopt */
	struct m0_fop          sec_fop;

	/** sender side session object */
	struct m0_rpc_session *sec_session;
};

static void session_establish_fop_release(struct m0_ref *ref);

static const struct m0_rpc_item_ops session_establish_item_ops = {
	.rio_replied = m0_rpc_session_establish_reply_received,
};

static const struct m0_rpc_item_ops session_terminate_item_ops = {
	.rio_replied = m0_rpc_session_terminate_reply_received,
};

M0_TL_DESCR_DEFINE(rpc_session, "rpc-sessions", M0_INTERNAL,
		   struct m0_rpc_session, s_link, s_magic, M0_RPC_SESSION_MAGIC,
		   M0_RPC_SESSION_HEAD_MAGIC);
M0_TL_DEFINE(rpc_session, M0_INTERNAL, struct m0_rpc_session);

static struct m0_sm_state_descr session_states[] = {
	[M0_RPC_SESSION_INITIALISED] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Initialised",
		.sd_allowed   = M0_BITS(M0_RPC_SESSION_ESTABLISHING,
					M0_RPC_SESSION_IDLE, /* only on rcvr */
					M0_RPC_SESSION_FINALISED,
					M0_RPC_SESSION_FAILED)
	},
	[M0_RPC_SESSION_ESTABLISHING] = {
		.sd_name      = "Establishing",
		.sd_allowed   = M0_BITS(M0_RPC_SESSION_IDLE,
					M0_RPC_SESSION_FAILED)
	},
	[M0_RPC_SESSION_IDLE] = {
		.sd_name      = "Idle",
		.sd_allowed   = M0_BITS(M0_RPC_SESSION_TERMINATING,
					M0_RPC_SESSION_TERMINATED,
					M0_RPC_SESSION_BUSY,
					M0_RPC_SESSION_FAILED)
	},
	[M0_RPC_SESSION_BUSY] = {
		.sd_name      = "Busy",
		.sd_allowed   = M0_BITS(M0_RPC_SESSION_IDLE)
	},
	[M0_RPC_SESSION_TERMINATING] = {
		.sd_name      = "Terminating",
		.sd_allowed   = M0_BITS(M0_RPC_SESSION_TERMINATED,
					M0_RPC_SESSION_FAILED)
	},
	[M0_RPC_SESSION_TERMINATED] = {
		.sd_name      = "Terminated",
		.sd_allowed   = M0_BITS(M0_RPC_SESSION_INITIALISED,
					M0_RPC_SESSION_FINALISED)
	},
	[M0_RPC_SESSION_FAILED] = {
		.sd_flags     = M0_SDF_FAILURE,
		.sd_name      = "Failed",
		.sd_allowed   = M0_BITS(M0_RPC_SESSION_INITIALISED,
					M0_RPC_SESSION_FINALISED)
	},
	[M0_RPC_SESSION_FINALISED] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Finalised",
	},
};

static const struct m0_sm_conf session_conf = {
	.scf_name      = "Session states",
	.scf_nr_states = ARRAY_SIZE(session_states),
	.scf_state     = session_states
};

M0_INTERNAL void session_state_set(struct m0_rpc_session *session, int state)
{
	M0_PRE(session != NULL);

	M0_LOG(M0_INFO, "Session %p: %s -> %s", session,
		session_states[session->s_sm.sm_state].sd_name,
		session_states[state].sd_name);
	m0_sm_state_set(&session->s_sm, state);
}

M0_INTERNAL int session_state(const struct m0_rpc_session *session)
{
	return session->s_sm.sm_state;
}

M0_INTERNAL struct m0_rpc_machine *
session_machine(const struct m0_rpc_session *s)
{
	return s->s_conn->c_rpc_machine;
}

/**
   The routine is also called from session_foms.c, hence can't be static
 */
M0_INTERNAL bool m0_rpc_session_invariant(const struct m0_rpc_session *session)
{
	bool ok;

	ok = _0C(session != NULL) &&
	     _0C(session->s_conn != NULL) &&
	     _0C(ergo(rpc_session_tlink_is_in(session),
		      rpc_session_tlist_contains(&session->s_conn->c_sessions,
						 session))) &&
	     _0C(ergo(session->s_session_id != SESSION_ID_0,
		      session->s_conn->c_nr_sessions > 0));

	if (!ok)
		return false;

	switch (session_state(session)) {
	case M0_RPC_SESSION_INITIALISED:
	case M0_RPC_SESSION_ESTABLISHING:
		return _0C(session->s_session_id == SESSION_ID_INVALID) &&
		       _0C(m0_rpc_session_is_idle(session));

	case M0_RPC_SESSION_TERMINATED:
		return	_0C(session->s_session_id <= SESSION_ID_MAX) &&
			_0C(m0_rpc_session_is_idle(session));

	case M0_RPC_SESSION_IDLE:
	case M0_RPC_SESSION_TERMINATING:
		return _0C(m0_rpc_session_is_idle(session)) &&
		       _0C(session->s_session_id <= SESSION_ID_MAX);

	case M0_RPC_SESSION_BUSY:
		return _0C(!m0_rpc_session_is_idle(session)) &&
		       _0C(session->s_session_id <= SESSION_ID_MAX);

	case M0_RPC_SESSION_FAILED:
		return _0C(session->s_sm.sm_rc != 0);

	default:
		return _0C(false);
	}
	/* Should never reach here */
	M0_ASSERT(0);
}

M0_INTERNAL bool m0_rpc_session_is_idle(const struct m0_rpc_session *session)
{
	return session->s_hold_cnt == 0;
}

M0_INTERNAL int m0_rpc_session_init(struct m0_rpc_session *session,
				    struct m0_rpc_conn *conn)
{
	struct m0_rpc_machine *machine;
	int                    rc;

	M0_ENTRY("session: %p, conn: %p", session, conn);
	M0_PRE(session != NULL && conn != NULL);

	machine = conn->c_rpc_machine;
	M0_PRE(machine != NULL);
	m0_rpc_machine_lock(machine);
	rc = m0_rpc_session_init_locked(session, conn);
	m0_rpc_machine_unlock(machine);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_session_init);

M0_INTERNAL int m0_rpc_session_init_locked(struct m0_rpc_session *session,
					   struct m0_rpc_conn *conn)
{
	int rc;

	M0_ENTRY("session: %p, conn: %p", session, conn);
	M0_PRE(session != NULL && conn != NULL);
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));

	M0_SET0(session);

	session->s_session_id = SESSION_ID_INVALID;
	session->s_conn       = conn;
	session->s_xid        = 0;
	session->s_cancelled  = false;

	rpc_session_tlink_init(session);
	m0_sm_init(&session->s_sm, &session_conf,
		   M0_RPC_SESSION_INITIALISED,
		   &conn->c_rpc_machine->rm_sm_grp);
	m0_rpc_conn_add_session(conn, session);
	M0_ASSERT(m0_rpc_session_invariant(session));
	rc = m0_rpc_item_cache_init(&session->s_reply_cache,
				    &conn->c_rpc_machine->rm_sm_grp.s_lock) ?:
	     m0_rpc_item_cache_init(&session->s_req_cache,
				    &conn->c_rpc_machine->rm_sm_grp.s_lock);
	if (rc == 0)
		M0_LOG(M0_INFO, "Session %p INITIALISED", session);
	else
		M0_LOG(M0_ERROR, "Session %p initialisation failed: %d",
		       session, rc);
	m0_rpc_item_pending_cache_init(session);

	return M0_RC(rc);
}

M0_INTERNAL void m0_rpc_session_reset(struct m0_rpc_session *session)
{
	struct m0_rpc_machine *machine = session_machine(session);

	m0_rpc_machine_lock(machine);
	if (session_state(session) == M0_RPC_SESSION_INITIALISED) {
		m0_rpc_machine_unlock(machine);
		return;
	}
	session_state_set(session, M0_RPC_SESSION_INITIALISED);
	session->s_xid = 0;
	session->s_cancelled = false;
	session->s_session_id = SESSION_ID_INVALID;
	M0_POST(m0_rpc_session_invariant(session));
	m0_rpc_machine_unlock(machine);
}

/**
   Finalises session.
   Used by
    m0_rpc_session_init(), when initialisation fails.
    m0_rpc_session_fini() for cleanup
 */
static void __session_fini(struct m0_rpc_session *session)
{
	M0_ENTRY("session: %p", session);

	rpc_session_tlink_fini(session);

	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_session_fini(struct m0_rpc_session *session)
{
	struct m0_rpc_machine *machine;

	M0_ENTRY("session: %p", session);
	M0_PRE(session != NULL &&
	       session->s_conn != NULL &&
	       session_machine(session) != NULL);

	machine = session_machine(session);

	m0_rpc_machine_lock(machine);
	m0_rpc_session_fini_locked(session);
	m0_rpc_machine_unlock(machine);
	M0_LEAVE();
}
M0_EXPORTED(m0_rpc_session_fini);

M0_INTERNAL void m0_rpc_session_fini_locked(struct m0_rpc_session *session)
{
	M0_ENTRY("session %p", session);
	M0_ASSERT(m0_rpc_session_invariant(session));
	M0_PRE(M0_IN(session_state(session), (M0_RPC_SESSION_TERMINATED,
					      M0_RPC_SESSION_INITIALISED,
					      M0_RPC_SESSION_FAILED)));

	m0_rpc_item_cache_fini(&session->s_reply_cache);
	m0_rpc_item_cache_fini(&session->s_req_cache);
	m0_rpc_item_pending_cache_fini(session);
	if (rpc_session_tlink_is_in(session))
		m0_rpc_conn_remove_session(session);
	__session_fini(session);
	session->s_session_id = SESSION_ID_INVALID;
	session_state_set(session, M0_RPC_SESSION_FINALISED);
	m0_sm_fini(&session->s_sm);
	M0_LOG(M0_INFO, "Session %p FINALISED \n", session);
	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_session_timedwait(struct m0_rpc_session *session,
					 uint64_t states,
					 const m0_time_t abs_timeout)
{
	struct m0_rpc_machine *machine = session_machine(session);
	int                    rc;

	M0_ENTRY("session: %p, abs_timeout: "TIME_F, session,
		 TIME_P(abs_timeout));

	m0_rpc_machine_lock(machine);
	M0_ASSERT(m0_rpc_session_invariant(session));
	rc = m0_sm_timedwait(&session->s_sm, states, abs_timeout);
	M0_ASSERT(m0_rpc_session_invariant(session));
	m0_rpc_machine_unlock(machine);

	return M0_RC(rc ?: session->s_sm.sm_rc);
}
M0_EXPORTED(m0_rpc_session_timedwait);

M0_INTERNAL int m0_rpc_session_create(struct m0_rpc_session *session,
				      struct m0_rpc_conn *conn,
				      m0_time_t abs_timeout)
{
	int rc;

	M0_ENTRY("session: %p, conn: %p", session, conn);

	rc = m0_rpc_session_init(session, conn);
	if (rc == 0) {
		rc = m0_rpc_session_establish_sync(session, abs_timeout);
		if (rc != 0)
			m0_rpc_session_fini(session);
	}

	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_session_establish_sync(struct m0_rpc_session *session,
					      m0_time_t abs_timeout)
{
	int  rc;

	M0_ENTRY("session: %p", session);
	rc = m0_rpc_session_establish(session, abs_timeout);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_rpc_session_timedwait(session, M0_BITS(M0_RPC_SESSION_IDLE,
						       M0_RPC_SESSION_FAILED),
				      M0_TIME_NEVER);

	M0_ASSERT(M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
						 M0_RPC_SESSION_FAILED)));
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_session_establish_sync);

M0_INTERNAL int m0_rpc_session_establish(struct m0_rpc_session *session,
					 m0_time_t abs_timeout)
{
	struct m0_rpc_conn                  *conn;
	struct m0_fop                       *fop;
	struct m0_rpc_fop_session_establish *args;
	struct fop_session_establish_ctx    *ctx;
	struct m0_rpc_session               *session_0;
	struct m0_rpc_machine               *machine;
	int                                  rc;

	M0_ENTRY("session: %p", session);
	M0_PRE(session != NULL);

	if (M0_FI_ENABLED("fake_error"))
		return M0_RC(-EINVAL);

	machine = session_machine(session);

	M0_ALLOC_PTR(ctx);
	if (ctx == NULL) {
		rc = M0_ERR(-ENOMEM);
	} else {
		ctx->sec_session = session;
		m0_fop_init(&ctx->sec_fop,
			    &m0_rpc_fop_session_establish_fopt, NULL,
			    session_establish_fop_release);
		rc = m0_fop_data_alloc(&ctx->sec_fop);
		if (rc != 0) {
			m0_fop_put_lock(&ctx->sec_fop);
		}
	}

	m0_rpc_machine_lock(machine);

	M0_ASSERT(m0_rpc_session_invariant(session) &&
		  session_state(session) == M0_RPC_SESSION_INITIALISED);

	if (rc != 0) {
		session_failed(session, rc);
		m0_rpc_machine_unlock(machine);
		return M0_RC(rc);
	}

	conn = session->s_conn;

	M0_ASSERT(conn_state(conn) == M0_RPC_CONN_ACTIVE);

	fop  = &ctx->sec_fop;
	args = m0_fop_data(fop);
	M0_ASSERT(args != NULL);

	args->rse_sender_id = conn->c_sender_id;

	session_0 = m0_rpc_conn_session0(conn);
	rc = m0_rpc__fop_post(fop, session_0, &session_establish_item_ops,
			      abs_timeout);
	if (rc == 0) {
		session_state_set(session, M0_RPC_SESSION_ESTABLISHING);
	} else {
		session_failed(session, rc);
	}
	m0_fop_put(fop);

	M0_POST(ergo(rc != 0, session_state(session) == M0_RPC_SESSION_FAILED));
	M0_POST(m0_rpc_session_invariant(session));

	m0_rpc_machine_unlock(machine);

	/* see m0_rpc_session_establish_reply_received() */
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_session_establish);

/**
   Moves session to FAILED state and take it out of conn->c_sessions list.

   @pre m0_mutex_is_locked(&session->s_mutex)
   @pre M0_IN(session_state(session), (M0_RPC_SESSION_INITIALISED,
				       M0_RPC_SESSION_ESTABLISHING,
				       M0_RPC_SESSION_IDLE,
				       M0_RPC_SESSION_BUSY,
				       M0_RPC_SESSION_TERMINATING))
 */
static void session_failed(struct m0_rpc_session *session, int32_t error)
{
	M0_ASSERT(m0_rpc_session_invariant(session));
	M0_PRE(M0_IN(session_state(session), (M0_RPC_SESSION_INITIALISED,
					      M0_RPC_SESSION_ESTABLISHING,
					      M0_RPC_SESSION_IDLE,
					      M0_RPC_SESSION_BUSY,
					      M0_RPC_SESSION_TERMINATING)));
	m0_sm_fail(&session->s_sm, M0_RPC_SESSION_FAILED, error);

	M0_ASSERT(m0_rpc_session_invariant(session));
}

M0_INTERNAL void m0_rpc_session_establish_reply_received(struct m0_rpc_item
							 *item)
{
	struct m0_rpc_fop_session_establish_rep *reply = NULL;
	struct fop_session_establish_ctx        *ctx;
	struct m0_rpc_machine                   *machine;
	struct m0_rpc_session                   *session;
	struct m0_rpc_item                      *reply_item;
	struct m0_fop                           *fop;
	uint64_t                                 session_id;
	int32_t                                  rc;

	M0_ENTRY("item: %p", item);
	M0_PRE(item != NULL &&
	       item->ri_session != NULL &&
	       item->ri_session->s_session_id == SESSION_ID_0);

	fop = m0_rpc_item_to_fop(item);
	ctx = container_of(fop, struct fop_session_establish_ctx, sec_fop);
	session = ctx->sec_session;
	M0_ASSERT(session != NULL);

	machine = session_machine(session);
	M0_ASSERT(m0_rpc_machine_is_locked(machine));

	M0_ASSERT(m0_rpc_session_invariant(session));
	M0_ASSERT_INFO(session_state(session) == M0_RPC_SESSION_ESTABLISHING,
		       "Invalid session state: expected %s, got %s",
		       m0_rpc_session_state_to_str(M0_RPC_SESSION_ESTABLISHING),
		       m0_rpc_session_state_to_str(session_state(session)));

	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		reply_item = item->ri_reply;
		M0_ASSERT(reply_item != NULL &&
			  item->ri_session == reply_item->ri_session);
		reply = m0_fop_data(m0_rpc_item_to_fop(reply_item));
		rc = reply->rser_rc;
	}
	if (rc == 0) {
		M0_ASSERT(reply != NULL);
		session_id = reply->rser_session_id;
		if (session_id > SESSION_ID_MIN &&
		    session_id < SESSION_ID_MAX &&
		    reply->rser_sender_id != SENDER_ID_INVALID) {
			session->s_session_id = session_id;
			session_state_set(session, M0_RPC_SESSION_IDLE);
		} else {
			rc = M0_ERR(-EPROTO);
		}
	}
	if (rc != 0)
		session_failed(session, rc);

	M0_POST(m0_rpc_session_invariant(session));
	M0_POST(M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
					       M0_RPC_SESSION_FAILED)));
	M0_POST(m0_rpc_machine_is_locked(machine));
	M0_LEAVE();
}

static void session_establish_fop_release(struct m0_ref *ref)
{
	struct fop_session_establish_ctx *ctx;
	struct m0_fop                    *fop;

	fop = container_of(ref, struct m0_fop, f_ref);
	m0_fop_fini(fop);
	ctx = container_of(fop, struct fop_session_establish_ctx, sec_fop);
	m0_free(ctx);
}

int m0_rpc_session_destroy(struct m0_rpc_session *session,
			   m0_time_t abs_timeout)
{
	int rc;

	M0_ENTRY("session: %p", session);

	rc = m0_rpc_session_terminate_sync(session, abs_timeout);
	m0_rpc_session_fini(session);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_session_destroy);

M0_INTERNAL int m0_rpc_session_validate(struct m0_rpc_session *session)
{
	M0_ENTRY();
	M0_PRE(session != NULL);
	if (session->s_cancelled)
		return M0_ERR_INFO(-ECANCELED, "Cancelled session");
	if (!M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
					    M0_RPC_SESSION_BUSY)))
		return M0_ERR_INFO(-EINVAL, "Session state %s is not valid",
				   m0_rpc_session_state_to_str(
					   session_state(session)));
	if (session->s_conn == NULL)
		return M0_ERR_INFO(-ENOMEDIUM, "Session connection is NULL");
	if (session->s_conn->c_rpc_machine == NULL)
		return M0_ERR_INFO(-ENOMEDIUM,
				   "Session connection rpc machine is NULL");
	return M0_RC(0);
}

M0_INTERNAL int m0_rpc_session_terminate_sync(struct m0_rpc_session *session,
					      m0_time_t abs_timeout)
{
	int rc;

	M0_ENTRY("session: %p", session);
	M0_PRE(M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
					      M0_RPC_SESSION_BUSY,
					      M0_RPC_SESSION_TERMINATING)));

	/* Wait for session to become IDLE */
	m0_rpc_session_timedwait(session, M0_BITS(M0_RPC_SESSION_IDLE),
				 M0_TIME_NEVER);

	rc = m0_rpc_session_terminate(session, abs_timeout);
	if (rc == 0) {
		M0_LOG(M0_DEBUG, "session: %p, wait for termination", session);
		rc = m0_rpc_session_timedwait(session,
					      M0_BITS(M0_RPC_SESSION_TERMINATED,
						      M0_RPC_SESSION_FAILED),
					      M0_TIME_NEVER);

		M0_ASSERT(M0_IN(session_state(session),
				(M0_RPC_SESSION_TERMINATED,
				 M0_RPC_SESSION_FAILED)));
	}
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_session_terminate_sync);

M0_INTERNAL int m0_rpc_session_terminate(struct m0_rpc_session *session,
					 m0_time_t abs_timeout)
{
	struct m0_fop                       *fop;
	struct m0_rpc_fop_session_terminate *args;
	struct m0_rpc_session               *session_0;
	struct m0_rpc_machine               *machine;
	struct m0_rpc_conn                  *conn;
	int                                  rc;

	M0_ENTRY("session: %p", session);
	M0_PRE(session != NULL && session->s_conn != NULL);

	conn    = session->s_conn;
	machine = conn->c_rpc_machine;

	m0_rpc_machine_lock(machine);

	M0_ASSERT(m0_rpc_session_invariant(session));
	M0_ASSERT(M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
						 M0_RPC_SESSION_TERMINATING)));

	if (session_state(session) == M0_RPC_SESSION_TERMINATING) {
		m0_rpc_machine_unlock(machine);
		return M0_RC(0);
	}

	if (!M0_FI_ENABLED("fail_allocation"))
		fop = m0_fop_alloc(&m0_rpc_fop_session_terminate_fopt,
				   NULL, machine);
	else
		fop = NULL;
	if (fop == NULL) {
		rc = M0_ERR(-ENOMEM);
		/* See [^1] about decision to move session to FAILED state */
		session_failed(session, rc);
		goto out_unlock;
	}

	args                 = m0_fop_data(fop);
	args->rst_sender_id  = conn->c_sender_id;
	args->rst_session_id = session->s_session_id;

	session_0 = m0_rpc_conn_session0(conn);

	/*
	 * m0_rpc_session_establish_reply_received() expects the session
	 * to be in M0_RPC_SESSION_TERMINATING state. Make sure it is so,
	 * even if item send below fails.
	 */
	session_state_set(session, M0_RPC_SESSION_TERMINATING);
	rc = m0_rpc__fop_post(fop, session_0, &session_terminate_item_ops,
			      abs_timeout);
	/*
	 * It is possible that ->rio_replied() was called
	 * and session is terminated already.
	 */
	if (rc != 0 && session_state(session) == M0_RPC_SESSION_TERMINATING)
		session_failed(session, rc);

	m0_fop_put(fop);

out_unlock:
	M0_ASSERT(m0_rpc_session_invariant(session));
	M0_POST(ergo(rc != 0, session_state(session) == M0_RPC_SESSION_FAILED));

	m0_rpc_machine_unlock(machine);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_session_terminate);
/*
 * m0_rpc_session_terminate
 * [^1]
 * There are two choices here:
 *
 * 1. leave session in TERMNATING state FOREVER.
 *    Then when to fini/cleanup session.
 *    This will not allow finialising of session, in turn conn,
 *    and rpc_machine can't be finalised.
 *
 * 2. Move session to FAILED state.
 *    For this session the receiver side state will still
 *    continue to exist. And receiver can send one-way
 *    items, that will be received on sender i.e. current node.
 *    Current code will drop such items. When/how to fini and
 *    cleanup receiver side state? XXX
 *
 * For now, later is chosen. This can be changed in future
 * to alternative 1, iff required.
 */


M0_INTERNAL void m0_rpc_session_terminate_reply_received(struct m0_rpc_item
							 *item)
{
	struct m0_rpc_fop_session_terminate_rep *reply;
	struct m0_rpc_fop_session_terminate     *args;
	struct m0_rpc_item                      *reply_item;
	struct m0_rpc_conn                      *conn;
	struct m0_rpc_session                   *session;
	struct m0_rpc_machine                   *machine;
	uint64_t                                 sender_id;
	uint64_t                                 session_id;
	int32_t                                  rc;

	M0_ENTRY("item: %p", item);
	M0_PRE(item != NULL &&
	       item->ri_session != NULL &&
	       item->ri_session->s_session_id == SESSION_ID_0);

	conn    = item2conn(item);
	machine = conn->c_rpc_machine;
	M0_ASSERT(m0_rpc_machine_is_locked(machine));
	M0_ASSERT(conn_state(conn) == M0_RPC_CONN_ACTIVE);

	args       = m0_fop_data(m0_rpc_item_to_fop(item));
	sender_id  = args->rst_sender_id;
	session_id = args->rst_session_id;
	M0_ASSERT(sender_id == conn->c_sender_id);
	session = m0_rpc_session_search(conn, session_id);
	M0_ASSERT(m0_rpc_session_invariant(session) &&
		  _0C(session_state(session) == M0_RPC_SESSION_TERMINATING));

	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		reply_item = item->ri_reply;
		M0_ASSERT(reply_item != NULL &&
			  item->ri_session == reply_item->ri_session);
		reply = m0_fop_data(m0_rpc_item_to_fop(reply_item));
		rc    = reply->rstr_rc;
	}
	if (rc == 0)
		session_state_set(session, M0_RPC_SESSION_TERMINATED);
	else
		session_failed(session, rc);

	M0_POST(m0_rpc_session_invariant(session));
	M0_POST(M0_IN(session_state(session), (M0_RPC_SESSION_TERMINATED,
					       M0_RPC_SESSION_FAILED)));
	M0_POST(m0_rpc_machine_is_locked(machine));
	M0_LEAVE();
}

M0_INTERNAL m0_bcount_t
m0_rpc_session_get_max_item_size(const struct m0_rpc_session *session)
{
	return session->s_conn->c_rpc_machine->rm_min_recv_size -
		m0_rpc_packet_onwire_header_size() -
		m0_rpc_packet_onwire_footer_size();
}

M0_INTERNAL m0_bcount_t
m0_rpc_session_get_max_item_payload_size(const struct m0_rpc_session *session)
{
	return m0_rpc_session_get_max_item_size(session) -
	       m0_rpc_item_onwire_header_size -
	       m0_rpc_item_onwire_footer_size;
}

M0_INTERNAL void m0_rpc_session_hold_busy(struct m0_rpc_session *session)
{
	M0_LOG(M0_DEBUG, "session %p %d -> %d", session, session->s_hold_cnt,
	       session->s_hold_cnt + 1);

	++session->s_hold_cnt;
	session_idle_x_busy(session);
}

M0_INTERNAL void m0_rpc_session_release(struct m0_rpc_session *session)
{
	M0_PRE(session_state(session) == M0_RPC_SESSION_BUSY);
	M0_PRE(session->s_hold_cnt > 0);

	M0_LOG(M0_DEBUG, "session %p %d -> %d", session, session->s_hold_cnt,
	       session->s_hold_cnt - 1);

	--session->s_hold_cnt;
	session_idle_x_busy(session);
}

/** Perform (IDLE -> BUSY) or (BUSY -> IDLE) transition if required */
static void session_idle_x_busy(struct m0_rpc_session *session)
{
	int  state = session_state(session);
	bool idle  = m0_rpc_session_is_idle(session);

	M0_PRE(M0_IN(state, (M0_RPC_SESSION_IDLE, M0_RPC_SESSION_BUSY)));

	if (state == M0_RPC_SESSION_IDLE && !idle) {
		session_state_set(session, M0_RPC_SESSION_BUSY);
	} else if (state == M0_RPC_SESSION_BUSY && idle) {
		session_state_set(session, M0_RPC_SESSION_IDLE);
	}

	M0_ASSERT_EX(m0_rpc_session_invariant(session));
	M0_POST(M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
					       M0_RPC_SESSION_BUSY)));
}

M0_INTERNAL int m0_rpc_rcv_session_terminate(struct m0_rpc_session *session)
{
	M0_ENTRY("session: %p", session);

	M0_PRE(session != NULL);
	M0_PRE(m0_rpc_machine_is_locked(session->s_conn->c_rpc_machine));
	M0_ASSERT(m0_rpc_session_invariant(session));
	M0_PRE(session_state(session) == M0_RPC_SESSION_IDLE);

	session_state_set(session, M0_RPC_SESSION_TERMINATED);
	M0_ASSERT(m0_rpc_session_invariant(session));
	return M0_RC(0);
}

M0_INTERNAL void m0_rpc_session_quiesce(struct m0_rpc_session *session)
{
	M0_ENTRY("session: %p", session);

	M0_PRE(session != NULL);
	M0_PRE(m0_rpc_machine_is_locked(session->s_conn->c_rpc_machine));
	M0_PRE(m0_rpc_session_invariant(session));
	M0_PRE(session_state(session) == M0_RPC_SESSION_ESTABLISHING);

	session_state_set(session, M0_RPC_SESSION_IDLE);
	M0_POST(m0_rpc_session_invariant(session));
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_session_cancel(struct m0_rpc_session *session)
{
	struct m0_rpc_item *item;

	M0_PRE(session->s_session_id != SESSION_ID_0);
	M0_PRE(M0_IN(session_state(session),
		     (M0_RPC_SESSION_BUSY, M0_RPC_SESSION_IDLE)));

	M0_ENTRY("session %p", session);
	m0_rpc_machine_lock(session->s_conn->c_rpc_machine);
	if (session->s_cancelled)
		goto leave_unlock;
	session->s_cancelled = true;
	m0_tl_for(pending_item, &session->s_pending_cache, item) {
		m0_rpc_item_get(item);
		m0_rpc_item_cancel_nolock(item);
		m0_rpc_item_put(item);
	} m0_tl_endfor;
leave_unlock:
	m0_rpc_machine_unlock(session->s_conn->c_rpc_machine);
	M0_POST(pending_item_tlist_is_empty(&session->s_pending_cache));
	M0_POST(session->s_sm.sm_state == M0_RPC_SESSION_IDLE);
	M0_LEAVE("session %p", session);
}

M0_INTERNAL bool m0_rpc_session_is_cancelled(struct m0_rpc_session *session)
{
	return session->s_cancelled;
}

M0_INTERNAL void m0_rpc_session_item_failed(struct m0_rpc_item *item)
{
	M0_PRE(item != NULL && item->ri_error != 0);
	M0_PRE(item->ri_sm.sm_state == M0_RPC_ITEM_FAILED);

	if (m0_rpc_item_is_request(item)) {
		M0_ASSERT(item->ri_error != 0);
		m0_rpc_item_replied_invoke(item);
	}
}

M0_INTERNAL const char *
m0_rpc_session_state_to_str(enum m0_rpc_session_state state)
{
	switch (state) {
#define S_CASE(x) case x: return #x
		S_CASE(M0_RPC_SESSION_INITIALISED);
		S_CASE(M0_RPC_SESSION_ESTABLISHING);
		S_CASE(M0_RPC_SESSION_IDLE);
		S_CASE(M0_RPC_SESSION_BUSY);
		S_CASE(M0_RPC_SESSION_FAILED);
		S_CASE(M0_RPC_SESSION_TERMINATING);
		S_CASE(M0_RPC_SESSION_TERMINATED);
		S_CASE(M0_RPC_SESSION_FINALISED);
#undef S_CASE
		default:
			M0_LOG(M0_ERROR, "Invalid state: %d", state);
			return NULL;
	}
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of session group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
