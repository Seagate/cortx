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
 * Original author: Rohan Puri <Rohan_Puri@xyratex.com>,
 *                  Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 04/15/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/trace.h"
#include "lib/finject.h"
#include "stob/stob.h"
#include "net/net.h"
#include "rpc/rpc_internal.h"

/**
   @addtogroup rpc_session

   @{

   Definitions of foms that execute conn establish, conn terminate, session
   establish and session terminate fops.
 */

/**
   Common implementation of m0_fom::fo_ops::fo_fini() for conn establish,
   conn terminate, session establish and session terminate foms

   @see session_gen_fom_create
 */
static void session_gen_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
}

/**
   implementation of fop->f_type->ft_fom_type.ft_ops->fto_create for
   conn establish, conn terminate, session establish,
   session terminate fop types.
 */
static int session_gen_fom_create(struct m0_fop *fop, struct m0_fom **m,
				  struct m0_reqh *reqh)
{
	struct m0_rpc_connection_session_specific_fom *gen;
	const struct m0_fom_ops		              *fom_ops;
	struct m0_fom			              *fom;
	struct m0_fop_type		              *reply_fopt;
	struct m0_fop			              *reply_fop;
	int				               rc;

	M0_ENTRY("fop: %p", fop);

	M0_ALLOC_PTR(gen);
	if (gen == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}
	fom = &gen->ssf_fom_generic;

	if (fop->f_type == &m0_rpc_fop_conn_establish_fopt) {

		reply_fopt = &m0_rpc_fop_conn_establish_rep_fopt;
		fom_ops    = &m0_rpc_fom_conn_establish_ops;

	} else if (fop->f_type == &m0_rpc_fop_conn_terminate_fopt) {

		reply_fopt = &m0_rpc_fop_conn_terminate_rep_fopt;
		fom_ops    = &m0_rpc_fom_conn_terminate_ops;

	} else if (fop->f_type == &m0_rpc_fop_session_establish_fopt) {

		reply_fopt = &m0_rpc_fop_session_establish_rep_fopt;
		fom_ops    = &m0_rpc_fom_session_establish_ops;

	} else if (fop->f_type == &m0_rpc_fop_session_terminate_fopt) {

		reply_fopt = &m0_rpc_fop_session_terminate_rep_fopt;
		fom_ops    = &m0_rpc_fom_session_terminate_ops;

	} else {
		reply_fopt = NULL;
		fom_ops    = NULL;
	}

	if (reply_fopt == NULL || fom_ops == NULL) {
		rc = -EINVAL;
		M0_LOG(M0_ERROR, "unsupported fop type '%s'\n",
				 fop->f_type->ft_name);
		goto out;
	}

	reply_fop = m0_fop_reply_alloc(fop, reply_fopt);
	if (M0_FI_ENABLED("reply_fop_alloc_failed")) {
		m0_fop_put(reply_fop);
		reply_fop = NULL;
	}
	if (reply_fop == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	m0_fom_init(fom, &fop->f_type->ft_fom_type, fom_ops, fop, reply_fop,
		    reqh);
	*m = fom;
	rc = 0;

out:
	if (rc != 0) {
		m0_free(gen);
		*m = NULL;
	}
	return M0_RC(rc);
}

static int rpc_tick_ret(struct m0_rpc_session *session,
			struct m0_fom         *fom,
			int                    next_state)
{
	enum m0_fom_phase_outcome ret = M0_FSO_AGAIN;
	struct m0_rpc_machine *machine = session_machine(session);

	M0_ENTRY("session: %p, state: %d", session, session_state(session));
	M0_PRE(m0_rpc_machine_is_locked(machine));
	M0_PRE(m0_fom_phase(fom) == M0_RPC_CONN_SESS_TERMINATE_WAIT);

	if (M0_IN(session_state(session), (M0_RPC_SESSION_BUSY,
					   M0_RPC_SESSION_TERMINATING))) {
		ret = M0_FSO_WAIT;
		m0_fom_wait_on(fom, &session->s_sm.sm_chan, &fom->fo_cb);
	}

	m0_fom_phase_set(fom, next_state);

	M0_LEAVE("ret: %d", ret);
	return ret;
}

const struct m0_fom_ops m0_rpc_fom_conn_establish_ops = {
	.fo_fini          = session_gen_fom_fini,
	.fo_tick          = m0_rpc_fom_conn_establish_tick,
	.fo_home_locality = m0_rpc_session_default_home_locality
};

struct m0_fom_type_ops m0_rpc_fom_conn_establish_type_ops = {
	.fto_create = session_gen_fom_create
};

M0_INTERNAL size_t m0_rpc_session_default_home_locality(const struct m0_fom
							*fom)
{
	M0_PRE(fom != NULL);

	return m0_fop_opcode(fom->fo_fop);
}

M0_INTERNAL int m0_rpc_fom_conn_establish_tick(struct m0_fom *fom)
{
	struct m0_rpc_fop_conn_establish_rep *reply;
	struct m0_rpc_fop_conn_establish_ctx *ctx;
	struct m0_rpc_fop_conn_establish     *request;
	struct m0_rpc_item_header2           *header;
	struct m0_fop                        *fop;
	struct m0_fop                        *fop_rep;
	struct m0_rpc_item                   *item;
	struct m0_rpc_machine                *machine;
	struct m0_rpc_session                *session0;
	struct m0_rpc_conn                   *conn;
	struct m0_rpc_conn                   *est_conn;
	static struct m0_fom_timeout         *fom_timeout = NULL;
	int                                   rc;

	M0_ENTRY("fom: %p", fom);
	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_fop != NULL && fom->fo_rep_fop != NULL);

	if (M0_FI_ENABLED("sleep-for-resend")) {
		M0_ASSERT(fom_timeout == NULL);
		M0_ALLOC_PTR(fom_timeout);
		M0_ASSERT(fom_timeout != NULL);
		m0_fom_timeout_init(fom_timeout);
		rc = m0_fom_timeout_wait_on(fom_timeout, fom,
		  m0_time_from_now(M0_RPC_ITEM_RESEND_INTERVAL * 2, 0));
		M0_ASSERT(rc == 0);
		M0_LEAVE();
		return M0_FSO_WAIT;
	}
	if (M0_FI_ENABLED("free-timer") && fom_timeout != NULL &&
	    fom_timeout->to_cb.fc_fom == fom) {
		/* don't touch not our timer (from resend) */
		m0_fom_timeout_fini(fom_timeout);
		m0_free(fom_timeout);
		m0_fi_disable(__func__, "free-timer");
	}

	fop     = fom->fo_fop;
	request = m0_fop_data(fop);
	M0_ASSERT(request != NULL);

	fop_rep = fom->fo_rep_fop;
	reply   = m0_fop_data(fop_rep);
	M0_ASSERT(reply != NULL);

	item = &fop->f_item;
	header = &item->ri_header;
	/*
	 * On receiver side CONN_ESTABLISH fop is wrapped in
	 * m0_rpc_fop_conn_etablish_ctx object.
	 * See conn_establish_item_decode()
	 */
	ctx = container_of(fop, struct m0_rpc_fop_conn_establish_ctx, cec_fop);
	M0_ASSERT(ctx != NULL &&
		  ctx->cec_sender_ep != NULL);

	M0_ALLOC_PTR(conn);
	if (M0_FI_ENABLED("conn-alloc-failed"))
		m0_free0(&conn);
	if (conn == NULL) {
		goto ret;
		/* no reply if conn establish failed.
		   See [4] at end of this function. */
	}

	machine = item->ri_rmachine;
	m0_rpc_machine_lock(machine);
	est_conn = m0_rpc_machine_find_conn(machine, item);
	if (est_conn != NULL) {
		/* This connection should aready be setup. */
		M0_ASSERT(m0_rpc_conn_invariant(est_conn));

		/* This is a duplicate request that was accepted
		   after original conn-establish request was accepted but
		   before the conn-establish operation completed.

		   It seems that server connect reply was dropped by client
		   due to the lack of free buffers. Let us resuse the existing
		   connection.
		 */
		M0_LOG(M0_INFO, "Duplicate conn-establish request %p", item);
		m0_free(conn);

		/*
		 * Reuse existing conn and proceed just like this is very
		 * first connection attempt.
		 */
		conn = est_conn;
		rc = 0;
	} else {
		rc = m0_rpc_rcv_conn_init(conn, ctx->cec_sender_ep, machine,
					  &header->osr_uuid);
		if (rc == 0) {
			conn->c_sender_id = m0_rpc_id_generate();
			conn_state_set(conn, M0_RPC_CONN_ACTIVE);
		}
	}
	if (rc == 0) {
		session0 = m0_rpc_conn_session0(conn);
		item->ri_session = session0;
		/* freed at m0_rpc_item_process_reply() */
		m0_rpc_session_hold_busy(session0);
	}
	m0_rpc_machine_unlock(machine);

	if (rc == 0) {
		reply->rcer_sender_id = conn->c_sender_id;
		reply->rcer_rc        = 0;
		M0_LOG(M0_INFO, "Conn established: conn [%p] id [%lu]\n", conn,
				(unsigned long)conn->c_sender_id);
		m0_rpc_reply_post(&fop->f_item, &fop_rep->f_item);
	} else {
		M0_ASSERT(conn != NULL);
		m0_free(conn);
		/* No reply is sent if conn establish failed. See [4] */
		M0_LOG(M0_ERROR, "Conn establish failed: rc [%d]\n", rc);
	}

ret:
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	M0_LEAVE();
	return M0_FSO_WAIT;
}

/*
 * FOM session create
 */

const struct m0_fom_ops m0_rpc_fom_session_establish_ops = {
	.fo_fini          = session_gen_fom_fini,
	.fo_tick          = m0_rpc_fom_session_establish_tick,
	.fo_home_locality = m0_rpc_session_default_home_locality
};

struct m0_fom_type_ops m0_rpc_fom_session_establish_type_ops = {
	.fto_create = session_gen_fom_create
};

M0_INTERNAL int m0_rpc_fom_session_establish_tick(struct m0_fom *fom)
{
	struct m0_rpc_fop_session_establish_rep *reply;
	struct m0_rpc_fop_session_establish     *request;
	struct m0_rpc_item                      *item;
	struct m0_fop                           *fop;
	struct m0_fop                           *fop_rep;
	struct m0_rpc_session                   *session;
	struct m0_rpc_conn                      *conn;
	struct m0_rpc_machine                   *machine;
	int                                      rc;

	M0_ENTRY("fom: %p", fom);
	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_fop != NULL && fom->fo_rep_fop != NULL);

	fop = fom->fo_fop;
	request = m0_fop_data(fop);
	M0_ASSERT(request != NULL);

	fop_rep = fom->fo_rep_fop;
	reply = m0_fop_data(fop_rep);
	M0_ASSERT(reply != NULL);

	item = &fop->f_item;
	M0_ASSERT(item->ri_session != NULL);
	conn = item2conn(item);
	M0_ASSERT(conn != NULL);
	machine = conn->c_rpc_machine;

	/*
	   Drop this session establish FOP.

	   Assume the following case:
	   1) time t1 < t2 < t3

	   2) t1: RPC item which corresponds to session establish FOP is
	      received.

	      t2: RPC item which corresponds to connection terminate FOP is
	      received. Connection terminate FOP is processed inside request
	      handler: m0_rpc_fom_conn_terminate_tick() is called.

	      t3: Session establish FOP is processed inside request hander,
	      m0_rpc_fom_session_establish_tick() is called.

	   3) NOTE: Listed above FOPs correspond to RPC items, which were
	   delivered onto RPC session0 of current RPC connection.

	   Analysis:

	   After sending connection termination FOP, sender side is no longer
	   waiting for connection establish FOP reply, so it's easy just to drop
	   this reply in this tick.

	   In other case, we can think to process (1-3) inside
	   m0_rpc_fom_conn_terminate_tick() so that we wait session to
	   establish, and only then terminate current connection. From the first
	   sight, this wait stage can be done by waiting session0 to become
	   IDLE, but there're two RPC items, pending on this session (terminate
	   connection item, establish session item), so session0 never becomes
	   IDLE here.
	 */
	m0_rpc_machine_lock(machine);
	if (M0_IN(conn_state(conn), (M0_RPC_CONN_TERMINATED,
				     M0_RPC_CONN_TERMINATING))) {
		m0_rpc_machine_unlock(machine);
		goto out2;
	}
	m0_rpc_machine_unlock(machine);

	M0_ALLOC_PTR(session);
	if (M0_FI_ENABLED("session-alloc-failed"))
		m0_free0(&session);
	if (session == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}
	m0_rpc_machine_lock(machine);
	rc = m0_rpc_session_init_locked(session, conn);
	if (rc == 0) {
		do {
			session->s_session_id = m0_rpc_id_generate();
		} while (session->s_session_id <= SESSION_ID_MIN ||
			 session->s_session_id >  SESSION_ID_MAX);
		session_state_set(session, M0_RPC_SESSION_IDLE);
		reply->rser_session_id = session->s_session_id;
	}
	m0_rpc_machine_unlock(machine);

out:
	reply->rser_sender_id  = request->rse_sender_id;
	reply->rser_rc         = rc;
	if (rc != 0) {
		reply->rser_session_id = SESSION_ID_INVALID;
		if (session != NULL)
			m0_free(session);
	}

	m0_rpc_reply_post(&fop->f_item, &fop_rep->f_item);
out2:
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	M0_LEAVE();
	return M0_FSO_WAIT;
}

/*
 * FOM session terminate
 */

const struct m0_fom_ops m0_rpc_fom_session_terminate_ops = {
	.fo_fini          = session_gen_fom_fini,
	.fo_tick          = m0_rpc_fom_session_terminate_tick,
	.fo_home_locality = m0_rpc_session_default_home_locality
};

struct m0_fom_type_ops m0_rpc_fom_session_terminate_type_ops = {
	.fto_create = session_gen_fom_create
};

M0_INTERNAL int m0_rpc_fom_session_terminate_tick(struct m0_fom *fom)
{
	struct m0_rpc_connection_session_specific_fom *gen;
	struct m0_rpc_fop_session_terminate_rep       *reply;
	struct m0_rpc_fop_session_terminate           *request;
	struct m0_rpc_item                            *item;
	struct m0_rpc_session                         *session;
	struct m0_rpc_machine                         *machine;
	struct m0_rpc_conn                            *conn;
	uint64_t                                       session_id;
	int                                            rc;

	M0_ENTRY("fom: %p, fom_phase: %d", fom, m0_fom_phase(fom));
	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_fop != NULL && fom->fo_rep_fop != NULL);

	gen = container_of(fom,
			       struct m0_rpc_connection_session_specific_fom,
			       ssf_fom_generic);
	M0_ASSERT(gen != NULL);

	request = m0_fop_data(fom->fo_fop);
	M0_ASSERT(request != NULL);

	reply = m0_fop_data(fom->fo_rep_fop);
	M0_ASSERT(reply != NULL);

	reply->rstr_sender_id = request->rst_sender_id;
	reply->rstr_session_id = session_id = request->rst_session_id;

	item = &fom->fo_fop->f_item;
	M0_ASSERT(item->ri_session != NULL);

	conn = item2conn(item);
	machine = conn->c_rpc_machine;

	m0_rpc_machine_lock(machine);
	M0_ASSERT(m0_rpc_conn_invariant(conn));

	/* The following switch is an asynchronous cycle and it
	 * does the following:
	 *
	 * wait until session_state(session) == M0_RPC_SESSION_IDLE
	 * finalize(session)
	 */
	rc = 0;
	session = NULL;
	switch(m0_fom_phase(fom)) {
	case M0_RPC_CONN_SESS_TERMINATE_INIT:
		gen->ssf_term_session = NULL;
		m0_fom_phase_set(fom, M0_RPC_CONN_SESS_TERMINATE_WAIT);
		m0_rpc_machine_unlock(machine);
		M0_LEAVE();
		return M0_FSO_AGAIN;

	case M0_RPC_CONN_SESS_TERMINATE_WAIT:
		if (conn_state(conn) != M0_RPC_CONN_ACTIVE) {
			rc = -EINVAL;
			break;
		}
		if (gen->ssf_term_session == NULL)
			gen->ssf_term_session = m0_rpc_session_search_and_pop(
							conn, session_id);
		session = (struct m0_rpc_session *)gen->ssf_term_session;
		if (session == NULL) {
			rc = -ENOENT;
			break;
		}
		rc = rpc_tick_ret(session, fom,
				  M0_RPC_CONN_SESS_TERMINATE_WAIT);
		if (rc == M0_FSO_WAIT) {
			M0_LOG(M0_DEBUG, "session: %p, hold_cnt: %d, "
			       "wait for session to become idle",
			       session, session->s_hold_cnt);
			m0_rpc_machine_unlock(machine);
			return rc;
		}
		M0_LOG(M0_DEBUG, "Actual session remove. session: %p, "
		       "hold_cnt: %d", session, session->s_hold_cnt);
		rc = m0_rpc_rcv_session_terminate(session);
		M0_ASSERT(ergo(rc != 0, session_state(session) ==
			       M0_RPC_SESSION_FAILED));
		M0_ASSERT(ergo(rc == 0, session_state(session) ==
			       M0_RPC_SESSION_TERMINATED));

		m0_rpc_session_fini_locked(session);
		m0_free(session);

	default:
		;
	}

	m0_rpc_machine_unlock(machine);

	reply->rstr_rc = rc;
	M0_LOG(M0_DEBUG, "Session terminate %s: session [%p] rc [%d]",
	       (rc == 0) ? "successful" : "failed", session, rc);

	/*
	 * Note: request is received on SESSION_0, which is different from
	 * current session being terminated. Reply will also go on SESSION_0.
	 */
	m0_fom_phase_set(fom, M0_RPC_CONN_SESS_TERMINATE_DONE);
	m0_rpc_reply_post(&fom->fo_fop->f_item, &fom->fo_rep_fop->f_item);

	M0_LEAVE();
	return M0_FSO_WAIT;
}

/*
 * FOM RPC connection terminate
 */
const struct m0_fom_ops m0_rpc_fom_conn_terminate_ops = {
	.fo_fini = session_gen_fom_fini,
	.fo_tick = m0_rpc_fom_conn_terminate_tick,
	.fo_home_locality = m0_rpc_session_default_home_locality
};

struct m0_fom_type_ops m0_rpc_fom_conn_terminate_type_ops = {
	.fto_create = session_gen_fom_create
};

static void conn_cleanup_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_rpc_conn *conn;

	conn = container_of(ast, struct m0_rpc_conn, c_ast);
	m0_rpc_conn_terminate_reply_sent(conn);
}

static void conn_terminate_reply_sent_cb(struct m0_rpc_item *item)
{
	struct m0_rpc_conn *conn;

	M0_ENTRY("item: %p", item);

	M0_PRE(item != NULL &&
	       item->ri_session != NULL &&
	       item->ri_session->s_session_id == SESSION_ID_0 &&
	       item2conn(item) != NULL);

	conn = item2conn(item);
	conn->c_ast.sa_cb = conn_cleanup_ast;
	m0_sm_ast_post(&conn->c_rpc_machine->rm_sm_grp, &conn->c_ast);
	M0_LEAVE();
}

static const struct m0_rpc_item_ops conn_terminate_reply_item_ops = {
	.rio_sent = conn_terminate_reply_sent_cb,
};

M0_INTERNAL int m0_rpc_fom_conn_terminate_tick(struct m0_fom *fom)
{
	struct m0_rpc_connection_session_specific_fom *gen;
	struct m0_rpc_fop_conn_terminate_rep          *reply;
	struct m0_rpc_fop_conn_terminate              *request;
	struct m0_rpc_item                            *item;
	struct m0_fop                                 *fop;
	struct m0_fop                                 *fop_rep;
	struct m0_rpc_conn                            *conn;
	struct m0_rpc_machine                         *machine;
	struct m0_rpc_session                         *session;

	M0_ENTRY("fom: %p", fom);
	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_fop != NULL && fom->fo_rep_fop != NULL);

	gen = container_of(fom,
			       struct m0_rpc_connection_session_specific_fom,
			       ssf_fom_generic);
	M0_ASSERT(gen != NULL);

	fop     = fom->fo_fop;
	fop_rep = fom->fo_rep_fop;
	request = m0_fop_data(fop);
	reply   = m0_fop_data(fop_rep);

	item    = &fop->f_item;
	conn    = item2conn(item);
	machine = conn->c_rpc_machine;

	/* The following switch is an asynchronous cycle and it
	 * does the following:
	 *
	 * for (session : connection) {
	 *      wait until session_state(session) == M0_RPC_SESSION_IDLE
	 *      finalize(session)
	 * }
	 */
	switch(m0_fom_phase(fom)) {
	case M0_RPC_CONN_SESS_TERMINATE_INIT:
		gen->ssf_term_session = NULL;
		m0_fom_phase_set(fom, M0_RPC_CONN_SESS_TERMINATE_WAIT);
		return M0_FSO_AGAIN;

	case M0_RPC_CONN_SESS_TERMINATE_WAIT:
		m0_rpc_machine_lock(machine);

		if (gen->ssf_term_session == NULL) {
			session = m0_rpc_session_pop(conn);
			gen->ssf_term_session = session;
			if (session != NULL) {
				int rc;
				M0_LOG(M0_DEBUG, "Arming connection fom");
				rc = rpc_tick_ret(session, fom,
					  M0_RPC_CONN_SESS_TERMINATE_WAIT);
				m0_rpc_machine_unlock(machine);
				return rc;
			}
		} else {
			M0_LOG(M0_DEBUG, "Session is ready for termination");
			session = (struct m0_rpc_session *)gen->ssf_term_session;
			M0_ASSERT(session != NULL);
			M0_ASSERT(M0_IN(session_state(session),
					(M0_RPC_SESSION_IDLE,
					 M0_RPC_SESSION_BUSY)));
			if (session_state(session) == M0_RPC_SESSION_IDLE) {
				(void)m0_rpc_rcv_session_terminate(session);
				m0_rpc_session_fini_locked(session);
				m0_free(session);
				gen->ssf_term_session = NULL;
			} else {
				M0_LOG(M0_DEBUG, "Session [%p] busy.", session);
			}

			m0_rpc_machine_unlock(machine);
			M0_LEAVE();
			return M0_FSO_AGAIN;
		}

	default:
		;
	}

	reply->ctr_sender_id = request->ct_sender_id;
	switch (conn_state(conn)) {
	case M0_RPC_CONN_ACTIVE:
		M0_ASSERT(conn->c_nr_sessions <= 1);
		reply->ctr_rc = m0_rpc_rcv_conn_terminate(conn);
		break;
	case M0_RPC_CONN_TERMINATING:
		reply->ctr_rc = -EALREADY;
		break;
	case M0_RPC_CONN_TERMINATED:
		reply->ctr_rc = -EPERM;
		break;
	default:
		M0_IMPOSSIBLE("Invalid state");
	}
	m0_rpc_machine_unlock(machine);

	fop_rep->f_item.ri_ops = &conn_terminate_reply_item_ops;
	m0_rpc_reply_post(&fop->f_item, &fop_rep->f_item);

	/*
	 * In memory state of conn is not cleaned up, at this point.
	 * conn will be finalised and freed in the ->rio_sent()
	 * callback of &fop_rep->f_item item.
	 * see: conn_terminate_reply_sent_cb, conn_cleanup_ast()
	 */
	m0_fom_phase_set(fom, M0_RPC_CONN_SESS_TERMINATE_DONE);
	M0_LEAVE();
	return M0_FSO_WAIT;
}

/** @} End of rpc_session group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
