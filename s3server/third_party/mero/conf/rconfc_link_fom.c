/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 29-Nov-2016
 */

#define M0_TRACE_SUBSYSTEM   M0_TRACE_SUBSYS_CONF

#include "lib/trace.h"

#include "lib/finject.h"          /* M0_FI_ENABLED */
#include "rpc/conn.h"             /* m0_rpc_conn_terminate */
#include "rpc/rpc.h"              /* m0_rpc__down_timeout */
#include "rpc/rpc_opcodes.h"
#include "rpc/session.h"          /* m0_rpc_session_terminate */
#include "conf/confc.h"
#include "conf/rconfc.h"
#include "conf/rconfc_internal.h"
#include "conf/rconfc_link_fom.h"

/*
 * rconfc_herd_link_fini() being located in conf/rconfc.c is exposed by this for
 * rconfc_link_fom_fini().
 */
M0_INTERNAL void rconfc_herd_link_fini(struct rconfc_link *lnk);

static struct m0_sm_state_descr rconfc_link_fom_state_descr[] = {
	[M0_RLF_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "M0_RLF_INIT",
		.sd_allowed = M0_BITS(M0_RLF_SESS_WAIT_IDLE, M0_RLF_FINI)
	},
	[M0_RLF_SESS_WAIT_IDLE] = {
		.sd_flags   = 0,
		.sd_name    = "M0_RLF_SESS_WAIT_IDLE",
		.sd_allowed = M0_BITS(M0_RLF_SESS_TERMINATING, M0_RLF_FINI)
	},
	[M0_RLF_SESS_TERMINATING] = {
		.sd_flags   = 0,
		.sd_name    = "M0_RLF_SESS_TERMINATING",
		.sd_allowed = M0_BITS(M0_RLF_CONN_TERMINATING, M0_RLF_FINI)
	},
	[M0_RLF_CONN_TERMINATING] = {
		.sd_flags   = 0,
		.sd_name    = "M0_RLF_CONN_TERMINATING",
		.sd_allowed = M0_BITS(M0_RLF_FINI)
	},
	[M0_RLF_FINI] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "M0_RLF_FINI",
		.sd_allowed = 0
	},
};

static const struct m0_sm_conf rconfc_link_fom_sm_conf = {
	.scf_name      = "rconfc_link FOM state machine",
	.scf_nr_states = ARRAY_SIZE(rconfc_link_fom_state_descr),
	.scf_state     = rconfc_link_fom_state_descr,
};

struct m0_fom_type rconfc_link_fom_type;

static int rconfc_link_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	M0_ENTRY();
	M0_IMPOSSIBLE("Expected to never be called");
	return M0_RC(0);
}

static void rconfc_link_fom_clink_cleanup_fini(struct rconfc_link *lnk)
{
	m0_clink_cleanup(&lnk->rl_fom_clink);
	m0_clink_fini(&lnk->rl_fom_clink);
	lnk->rl_fom_clink.cl_chan = NULL;
}

static void rconfc_link_fom_fini(struct m0_fom *fom)
{
	struct rconfc_link *lnk = M0_AMB(lnk, fom, rl_fom);

	M0_ENTRY("lnk=%p", lnk);
	m0_fom_fini(fom);
	m0_rconfc_lock(lnk->rl_rconfc);
	m0_mutex_lock(&lnk->rl_rconfc->rc_herd_lock);
	rconfc_herd_link_fini(lnk);
	/*
	 * Note: rl_state must be set to CONFC_DEAD only after link finalisation
	 * to not prevent the link from correct cleanup.
	 */
	lnk->rl_state = CONFC_DEAD;
	/*
	 * XXX: The callback is only for UT
	 */
	if (lnk->rl_on_state_cb != NULL)
		lnk->rl_on_state_cb(lnk);
	M0_SET0(&lnk->rl_fom);
	lnk->rl_fom_queued = false;
	m0_chan_broadcast(&lnk->rl_rconfc->rc_herd_chan);
	m0_mutex_unlock(&lnk->rl_rconfc->rc_herd_lock);
	m0_rconfc_unlock(lnk->rl_rconfc);
	M0_LEAVE();
}

static size_t rconfc_link_fom_locality(const struct m0_fom *fom)
{
	struct rconfc_link *lnk = M0_AMB(lnk, fom, rl_fom);

	return lnk->rl_confd_fid.f_key;
}

static bool rconfc_link__on_sess_idle(struct m0_clink *clink)
{
	struct rconfc_link    *lnk  = M0_AMB(lnk, clink, rl_fom_clink);
	struct m0_rpc_session *sess = m0_confc2sess(&lnk->rl_confc);

	if (!M0_IN(sess->s_sm.sm_state, (M0_RPC_SESSION_IDLE,
					 M0_RPC_SESSION_FAILED)))
		return true;
	m0_fom_wakeup(&lnk->rl_fom);
	return false;
}

static bool rconfc_link__on_sess_terminated(struct m0_clink *clink)
{
	struct rconfc_link    *lnk  = M0_AMB(lnk, clink, rl_fom_clink);
	struct m0_rpc_session *sess = m0_confc2sess(&lnk->rl_confc);

	if (!M0_IN(sess->s_sm.sm_state, (M0_RPC_SESSION_TERMINATED,
					 M0_RPC_SESSION_FAILED)))
		return true;
	m0_fom_wakeup(&lnk->rl_fom);
	return false;
}

static bool rconfc_link__on_conn_terminated(struct m0_clink *clink)
{
	struct rconfc_link *lnk  = M0_AMB(lnk, clink, rl_fom_clink);
	struct m0_rpc_conn *conn = m0_confc2conn(&lnk->rl_confc);

	if (!M0_IN(conn->c_sm.sm_state, (M0_RPC_CONN_TERMINATED,
					 M0_RPC_CONN_FAILED)))
		return true;
	m0_fom_wakeup(&lnk->rl_fom);
	return false;
}

static void rconfc_herd_link_fom_wait_on(struct rconfc_link *lnk,
					 int                 phase)
{
	struct m0_rpc_session *sess = m0_confc2sess(&lnk->rl_confc);
	struct m0_rpc_conn    *conn = m0_confc2conn(&lnk->rl_confc);
	struct m0_chan        *chan[] = {
		[M0_RLF_INIT]             = &sess->s_sm.sm_chan,
		[M0_RLF_SESS_WAIT_IDLE]   = &sess->s_sm.sm_chan,
		[M0_RLF_SESS_TERMINATING] = &conn->c_sm.sm_chan,
	};
	m0_chan_cb_t           cb[] = {
		[M0_RLF_INIT]             = rconfc_link__on_sess_idle,
		[M0_RLF_SESS_WAIT_IDLE]   = rconfc_link__on_sess_terminated,
		[M0_RLF_SESS_TERMINATING] = rconfc_link__on_conn_terminated,
	};

	m0_clink_init(&lnk->rl_fom_clink, cb[phase]);
	m0_clink_add_lock(chan[phase], &lnk->rl_fom_clink);
}

static int rconfc_link_fom_tick(struct m0_fom *fom)
{
	struct rconfc_link    *lnk   = M0_AMB(lnk, fom, rl_fom);
	struct m0_rpc_session *sess = m0_confc2sess(&lnk->rl_confc);
	struct m0_rpc_conn    *conn = m0_confc2conn(&lnk->rl_confc);
	int                    phase = m0_fom_phase(fom);
	int                    rc;

	M0_ENTRY();

	switch (phase) {
	case M0_RLF_INIT:
		/*
		 * In case confc is not online, it is safe to finalise the link
		 * right away.
		 */
		if (!m0_confc_is_inited(&lnk->rl_confc) ||
		    !m0_confc_is_online(&lnk->rl_confc)) {
			m0_fom_phase_set(fom, M0_RLF_FINI);
			return M0_FSO_WAIT;
		}
		m0_fom_phase_set(fom, M0_RLF_SESS_WAIT_IDLE);
		/*
		 * It turns out the confc needs to quiesce the session prior to
		 * going on with its termination.
		 */
		if (M0_IN(sess->s_sm.sm_state, (M0_RPC_SESSION_IDLE,
						M0_RPC_SESSION_FAILED)))
			return M0_FSO_AGAIN;
		rconfc_herd_link_fom_wait_on(lnk, phase);
		break;

	case M0_RLF_SESS_WAIT_IDLE:
		rconfc_herd_link_fom_wait_on(lnk, phase);
		m0_fom_phase_set(fom, M0_RLF_SESS_TERMINATING);
		/*
		 * The session may appear to be already failed due to external
		 * reasons. If so, go straight to disconnection phase.
		 */
		if (sess->s_sm.sm_state == M0_RPC_SESSION_FAILED) {
			M0_LOG(M0_DEBUG, "session=%p found failed", sess);
			return M0_FSO_AGAIN;
		}
		/* Break rpc session */
		if (M0_FI_ENABLED("sess_fail"))
			m0_fi_enable_once("m0_rpc_session_terminate",
					  "fail_allocation");
		rc = m0_rpc_session_terminate(sess, m0_rpc__down_timeout());
		if (rc != 0) {
			M0_LOG(M0_ERROR, "session=%p, rc=%d", sess, rc);
			M0_ASSERT(sess->s_sm.sm_state == M0_RPC_SESSION_FAILED);
		}
		break;

	case M0_RLF_SESS_TERMINATING:
		rconfc_link_fom_clink_cleanup_fini(lnk);
		m0_rpc_session_fini(sess);
		/* Break rpc connection */
		rconfc_herd_link_fom_wait_on(lnk, phase);
		m0_fom_phase_set(fom, M0_RLF_CONN_TERMINATING);
		if (M0_FI_ENABLED("conn_fail"))
			m0_fi_enable_once("m0_rpc_conn_terminate",
					  "fail_allocation");
		rc = m0_rpc_conn_terminate(conn, m0_rpc__down_timeout());
		if (rc != 0) {
			M0_LOG(M0_ERROR, "conn=%p, rc=%d", conn, rc);
			M0_ASSERT(conn->c_sm.sm_state == M0_RPC_CONN_FAILED);
		}
		break;

	case M0_RLF_CONN_TERMINATING:
		rconfc_link_fom_clink_cleanup_fini(lnk);
		m0_rpc_conn_fini(conn);
		/* Finalise FOM */
		m0_fom_phase_set(fom, M0_RLF_FINI);
		break;
	}

	return M0_FSO_WAIT;
}

const struct m0_fom_type_ops rconfc_link_fom_type_ops = {
	.fto_create = rconfc_link_fom_create
};

const struct m0_fom_ops rconfc_link_fom_ops = {
	.fo_fini          = rconfc_link_fom_fini,
	.fo_tick          = rconfc_link_fom_tick,
	.fo_home_locality = rconfc_link_fom_locality
};

M0_INTERNAL int m0_rconfc_mod_init(void)
{
	extern struct m0_reqh_service_type m0_rpc_service_type;

	m0_fom_type_init(&rconfc_link_fom_type, M0_RCONFC_HERD_LINK_OPCODE,
			 &rconfc_link_fom_type_ops, &m0_rpc_service_type,
			 &rconfc_link_fom_sm_conf);
	return 0;
}

M0_INTERNAL void m0_rconfc_mod_fini(void)
{
}

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
