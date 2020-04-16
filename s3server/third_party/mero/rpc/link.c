/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro_podgornyi@xyratex.com>
 * Original creation date: 8-Aug-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/errno.h"
#include "lib/memory.h"               /* m0_free */
#include "lib/string.h"               /* m0_strdup */
#include "lib/trace.h"
#include "lib/chan.h"
#include "lib/time.h"
#include "lib/misc.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "net/net.h"                  /* m0_net_end_point */
#include "sm/sm.h"
#include "rpc/rpc_machine.h"          /* m0_rpc_machine, m0_rpc_machine_lock */
#include "rpc/rpc_opcodes.h"          /* M0_RPC_LINK_CONN_OPCODE */
#include "rpc/link.h"
#include "rpc/session_internal.h"     /* m0_rpc_session_fini_locked */
#include "rpc/conn_internal.h"        /* m0_rpc_conn_remove_session */

/**
 * @addtogroup rpc_link
 *
 * @{
 */

#define CONN_STATE(conn) ((conn)->c_sm.sm_state)
#define CONN_RC(conn)    ((conn)->c_sm.sm_rc)
#define CONN_CHAN(conn)  ((conn)->c_sm.sm_chan)

#define SESS_STATE(sess) ((sess)->s_sm.sm_state)
#define SESS_RC(sess)    ((sess)->s_sm.sm_rc)
#define SESS_CHAN(sess)  ((sess)->s_sm.sm_chan)

struct rpc_link_state_transition {
	/** Function which executes current phase */
	int       (*rlst_state_function)(struct m0_rpc_link *);
	int         rlst_next_phase;
	int         rlst_fail_phase;
	/** Description of phase */
	const char *rlst_st_desc;
};

typedef void (*rpc_link_cb_t)(struct m0_rpc_link*, m0_time_t, struct m0_clink*);

static int    rpc_link_conn_fom_tick(struct m0_fom *fom);
static void   rpc_link_conn_fom_fini(struct m0_fom *fom);
static int    rpc_link_disc_fom_tick(struct m0_fom *fom);
static void   rpc_link_disc_fom_fini(struct m0_fom *fom);
static size_t rpc_link_fom_locality(const struct m0_fom *fom);

static void rpc_link_conn_fom_wait_on(struct m0_fom *fom,
				      struct m0_rpc_link *rlink);

struct m0_fom_type rpc_link_conn_fom_type;
struct m0_fom_type rpc_link_disc_fom_type;

const struct m0_fom_ops rpc_link_conn_fom_ops = {
	.fo_fini          = rpc_link_conn_fom_fini,
	.fo_tick          = rpc_link_conn_fom_tick,
	.fo_home_locality = rpc_link_fom_locality
};

const struct m0_fom_ops rpc_link_disc_fom_ops = {
	.fo_fini          = rpc_link_disc_fom_fini,
	.fo_tick          = rpc_link_disc_fom_tick,
	.fo_home_locality = rpc_link_fom_locality
};

static const struct m0_fom_type_ops rpc_link_conn_fom_type_ops = {
	.fto_create = NULL,
};

static const struct m0_fom_type_ops rpc_link_disc_fom_type_ops = {
	.fto_create = NULL,
};

static size_t rpc_link_fom_locality(const struct m0_fom *fom)
{
	return 1;
}

/* Routines for connection */

static int rpc_link_conn_establish(struct m0_rpc_link *rlink)
{
	int rc;

	rc = m0_rpc_conn_establish(&rlink->rlk_conn, rlink->rlk_timeout);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Connection establish failed (rlink=%p)",
		       rlink);
	}
	return rc == 0 ? M0_FSO_AGAIN : rc;
}

static int rpc_link_sess_establish(struct m0_rpc_link *rlink)
{
	int rc;

	rc = m0_rpc_session_establish(&rlink->rlk_sess, rlink->rlk_timeout);
	if (rc != 0)
		M0_LOG(M0_ERROR, "Session establish failed (rlink=%p)", rlink);

	return rc == 0 ? M0_FSO_AGAIN : rc;
}

static int rpc_link_sess_established(struct m0_rpc_link *rlink)
{
	/*
	 * Rpc_link considered to be connected after fom is fini'ed. This avoids
	 * race when rlink->rlk_fom is used by m0_rpc_link_disconnect_async().
	 * @see rpc_link_conn_fom_fini()
	 */
	return M0_FSO_WAIT;
}

/* Routines for disconnection */

static int rpc_link_disc_init(struct m0_rpc_link *rlink)
{
	return M0_FSO_AGAIN;
}

static int rpc_link_conn_terminate(struct m0_rpc_link *rlink)
{
	int rc;

	rc = m0_rpc_conn_terminate(&rlink->rlk_conn, rlink->rlk_timeout);
	if (rc == -ECANCELED)
		return M0_FSO_AGAIN; /* no need to fail in the case */
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Connection termination failed (rlink=%p)",
		       rlink);
	}
	return rc == 0 ? M0_FSO_AGAIN : rc;
}

static int rpc_link_conn_terminated(struct m0_rpc_link *rlink)
{
	m0_rpc_conn_ha_unsubscribe(&rlink->rlk_conn);
	return M0_FSO_WAIT;
}

static int rpc_link_sess_terminate(struct m0_rpc_link *rlink)
{
	int rc;

	M0_PRE(SESS_STATE(&rlink->rlk_sess) == M0_RPC_SESSION_IDLE);

	rc = m0_rpc_session_terminate(&rlink->rlk_sess, rlink->rlk_timeout);
	if (rc == -ECANCELED)
		return M0_FSO_AGAIN; /* continue normal way */
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Session termination failed (rlink=%p, rc=%d)",
		       rlink, rc);
	}
	return rc == 0 ? M0_FSO_AGAIN : rc;
}

static void rpc_link_sess_cleanup(struct m0_rpc_link *rlink)
{
	struct m0_rpc_session *sess = &rlink->rlk_sess;

	if (M0_IN(SESS_STATE(&rlink->rlk_sess), (M0_RPC_SESSION_INITIALISED,
						 M0_RPC_SESSION_FINALISED)))
		return;
	if (SESS_STATE(sess) != M0_RPC_SESSION_FAILED)
		m0_rpc_session_cancel(sess);
}

static int rpc_link_conn_failure(struct m0_rpc_link *rlink)
{
	M0_PRE(rlink->rlk_rc != 0);
	m0_rpc_conn_ha_unsubscribe(&rlink->rlk_conn);
	return M0_FSO_WAIT;
}

static int rpc_link_sess_failure(struct m0_rpc_link *rlink)
{
	M0_PRE(rlink->rlk_rc != 0);
	/*
	 * Terminate connection even if session termination failed.
	 * We don't rewrite rlink->rlk_rc, therefore, ignore return code.
	 * M0_RPC_CONN_FAILED is handled by foms in M0_RLS_CONN_TERMINATING
	 * phase.
	 */
	(void)rpc_link_conn_terminate(rlink);
	return M0_FSO_AGAIN;
}

static struct rpc_link_state_transition rpc_link_conn_states[] = {

	[M0_RLS_INIT] =
	{ &rpc_link_conn_establish, M0_RLS_CONN_CONNECTING,
	  M0_RLS_CONN_FAILURE, "Initialised" },

	[M0_RLS_CONN_CONNECTING] =
	{ &rpc_link_sess_establish, M0_RLS_SESS_ESTABLISHING,
	  M0_RLS_SESS_FAILURE, "Connection establish" },

	[M0_RLS_SESS_ESTABLISHING] =
	{ &rpc_link_sess_established, M0_RLS_FINI, 0, "Session establishing" },

	[M0_RLS_CONN_FAILURE] =
	{ &rpc_link_conn_failure, M0_RLS_FINI, 0, "Failure in connection" },

	[M0_RLS_SESS_FAILURE] =
	{ &rpc_link_sess_failure, M0_RLS_CONN_TERMINATING, 0,
	  "Failure in establishing session" },

	[M0_RLS_CONN_TERMINATING] =
	{ &rpc_link_conn_failure, M0_RLS_FINI, 0, "terminate connection" },

};

static struct rpc_link_state_transition rpc_link_disc_states[] = {

	[M0_RLS_INIT] =
	{ &rpc_link_disc_init, M0_RLS_SESS_WAIT_IDLE, 0, "Initialised" },

	[M0_RLS_SESS_WAIT_IDLE] =
	{ &rpc_link_sess_terminate, M0_RLS_SESS_TERMINATING,
	  M0_RLS_SESS_FAILURE, "IDLE state wait" },

	[M0_RLS_SESS_TERMINATING] =
	{ &rpc_link_conn_terminate, M0_RLS_CONN_TERMINATING,
	  M0_RLS_CONN_FAILURE, "Session termination" },

	[M0_RLS_CONN_TERMINATING] =
	{ &rpc_link_conn_terminated, M0_RLS_FINI, 0, "Conn termination" },

	[M0_RLS_CONN_FAILURE] =
	{ &rpc_link_conn_failure, M0_RLS_FINI, 0, "Failure in disconnection" },

	[M0_RLS_SESS_FAILURE] =
	{ &rpc_link_sess_failure, M0_RLS_CONN_TERMINATING, 0,
	  "Failure in sess termination" },
};

static void rpc_link_conn_fom_wait_on(struct m0_fom *fom,
				      struct m0_rpc_link *rlink)
{
	M0_SET0(&rlink->rlk_fomcb);
	m0_fom_callback_init(&rlink->rlk_fomcb);
	m0_fom_wait_on(fom, &CONN_CHAN(&rlink->rlk_conn), &rlink->rlk_fomcb);
}

static void rpc_link_sess_fom_wait_on(struct m0_fom *fom,
				      struct m0_rpc_link *rlink)
{
	M0_SET0(&rlink->rlk_fomcb);
	m0_fom_callback_init(&rlink->rlk_fomcb);
	m0_fom_wait_on(fom, &SESS_CHAN(&rlink->rlk_sess), &rlink->rlk_fomcb);
}

static int rpc_link_conn_fom_tick(struct m0_fom *fom)
{
	int                      rc    = 0;
	int                      phase = m0_fom_phase(fom);
	bool                     armed = false;
	uint32_t                 state;
	struct m0_rpc_link      *rlink;
	struct m0_rpc_machine   *rpcmach;
	enum m0_rpc_link_states  fail_phase = M0_RLS_CONN_FAILURE;

	M0_ENTRY("fom=%p phase=%s", fom, m0_fom_phase_name(fom, phase));

	rlink   = container_of(fom, struct m0_rpc_link, rlk_fom);
	rpcmach = rlink->rlk_conn.c_rpc_machine;

	switch (phase) {
	case M0_RLS_CONN_CONNECTING:
		m0_rpc_machine_lock(rpcmach);
		state = CONN_STATE(&rlink->rlk_conn);
		M0_ASSERT(M0_IN(state, (M0_RPC_CONN_CONNECTING,
				M0_RPC_CONN_ACTIVE, M0_RPC_CONN_FAILED)));
		if (state == M0_RPC_CONN_FAILED) {
			fail_phase = M0_RLS_CONN_FAILURE;
			rc         = CONN_RC(&rlink->rlk_conn);
		}
		if (state == M0_RPC_CONN_CONNECTING) {
			rpc_link_conn_fom_wait_on(fom, rlink);
			armed = true;
			rc    = M0_FSO_WAIT;
		}
		m0_rpc_machine_unlock(rpcmach);
		break;
	case M0_RLS_SESS_ESTABLISHING:
		m0_rpc_machine_lock(rpcmach);
		state = SESS_STATE(&rlink->rlk_sess);
		/*
		 * There might chances of some subsystem send item
		 * as soon as session established and it becomes idle.
		 * So M0_RPC_SESSION_BUSY state also possible.
		 */
		M0_ASSERT(M0_IN(state, (M0_RPC_SESSION_ESTABLISHING,
					M0_RPC_SESSION_IDLE,
					M0_RPC_SESSION_BUSY,
					M0_RPC_SESSION_FAILED)));
		if (state == M0_RPC_SESSION_FAILED) {
			fail_phase = M0_RLS_SESS_FAILURE;
			rc         = SESS_RC(&rlink->rlk_sess);
		}
		if (state == M0_RPC_SESSION_ESTABLISHING) {
			rpc_link_sess_fom_wait_on(fom, rlink);
			armed = true;
			rc    = M0_FSO_WAIT;
		}
		m0_rpc_machine_unlock(rpcmach);
		break;
	case M0_RLS_SESS_FAILURE:
		/*
		 * We don't need machine lock here, it is done
		 * under machine lock down the stack in
		 * rpc_link_sess_cleanup().
		 */
		rpc_link_sess_cleanup(rlink);
		break;
	case M0_RLS_CONN_TERMINATING:
		m0_rpc_machine_lock(rpcmach);
		state = CONN_STATE(&rlink->rlk_conn);
		M0_ASSERT(M0_IN(state, (M0_RPC_CONN_TERMINATING,
				M0_RPC_CONN_TERMINATED,
				M0_RPC_CONN_FAILED)));
		/*
		 * No need to fail when state == M0_RPC_CONN_FAILED, because
		 * this is terminating path after session failure.
		 */
		if (state == M0_RPC_CONN_TERMINATING) {
			rpc_link_conn_fom_wait_on(fom, rlink);
			armed = true;
			rc    = M0_FSO_WAIT;
		}
		m0_rpc_machine_unlock(rpcmach);
		break;
	}

	if (rc == 0) {
		rc = (*rpc_link_conn_states[phase].rlst_state_function)(rlink);
		M0_ASSERT(rc != 0);
		if (rc > 0) {
			phase = rpc_link_conn_states[phase].rlst_next_phase;
			m0_fom_phase_set(fom, phase);
		} else
			fail_phase = rpc_link_conn_states[phase].rlst_fail_phase;
	}

	if (rc < 0) {
		if (armed)
			m0_fom_callback_cancel(&rlink->rlk_fomcb);
		rlink->rlk_rc = rlink->rlk_rc ?: rc;
		m0_fom_phase_set(fom, fail_phase);
		rc = M0_FSO_AGAIN;
	}
	return M0_RC(rc);
}

static int rpc_link_disc_fom_tick(struct m0_fom *fom)
{
	int                      rc    = 0;
	int                      phase = m0_fom_phase(fom);
	bool                     armed = false;
	uint32_t                 state;
	struct m0_rpc_link      *rlink;
	struct m0_rpc_machine   *rpcmach;
	enum m0_rpc_link_states  fail_phase;

	M0_ENTRY("fom=%p phase=%s", fom, m0_fom_phase_name(fom, phase));

	rlink   = container_of(fom, struct m0_rpc_link, rlk_fom);
	rpcmach = rlink->rlk_conn.c_rpc_machine;

	m0_rpc_machine_lock(rpcmach);
	switch (phase) {
	case M0_RLS_SESS_WAIT_IDLE:
		state = SESS_STATE(&rlink->rlk_sess);
		M0_ASSERT(M0_IN(state, (M0_RPC_SESSION_IDLE,
				M0_RPC_SESSION_BUSY, M0_RPC_SESSION_FAILED)));
		if (state == M0_RPC_SESSION_FAILED) {
			fail_phase = M0_RLS_SESS_FAILURE;
			rc         = SESS_RC(&rlink->rlk_sess);
		}
		if (state == M0_RPC_SESSION_BUSY) {
			rpc_link_sess_fom_wait_on(fom, rlink);
			armed = true;
			rc    = M0_FSO_WAIT;
		}
		break;
	case M0_RLS_SESS_TERMINATING:
		state = SESS_STATE(&rlink->rlk_sess);
		M0_ASSERT(M0_IN(state, (M0_RPC_SESSION_TERMINATING,
				M0_RPC_SESSION_TERMINATED,
				M0_RPC_SESSION_FAILED)));
		if (state == M0_RPC_SESSION_FAILED) {
			fail_phase = M0_RLS_SESS_FAILURE;
			rc         = SESS_RC(&rlink->rlk_sess);
		}
		if (state == M0_RPC_SESSION_TERMINATING) {
			rpc_link_sess_fom_wait_on(fom, rlink);
			armed = true;
			rc    = M0_FSO_WAIT;
		}
		break;
	case M0_RLS_CONN_TERMINATING:
		state = CONN_STATE(&rlink->rlk_conn);
		M0_ASSERT(M0_IN(state, (M0_RPC_CONN_TERMINATING,
				M0_RPC_CONN_TERMINATED,
				M0_RPC_CONN_FAILED)));
		if (state == M0_RPC_CONN_FAILED) {
			fail_phase = M0_RLS_CONN_FAILURE;
			rc         = CONN_RC(&rlink->rlk_conn);
		}
		if (state == M0_RPC_CONN_TERMINATING) {
			rpc_link_conn_fom_wait_on(fom, rlink);
			armed = true;
			rc    = M0_FSO_WAIT;
		}
		break;
	}
	m0_rpc_machine_unlock(rpcmach);

	if (rc == 0) {
		rc = (*rpc_link_disc_states[phase].rlst_state_function)(rlink);
		M0_ASSERT(rc != 0);
		if (rc > 0) {
			phase = rpc_link_disc_states[phase].rlst_next_phase;
			m0_fom_phase_set(fom, phase);
		} else
			fail_phase = rpc_link_disc_states[phase].rlst_fail_phase;
	}

	if (rc < 0) {
		if (armed)
			m0_fom_callback_cancel(&rlink->rlk_fomcb);
		rlink->rlk_rc = rlink->rlk_rc ?: rc;
		m0_fom_phase_set(fom, fail_phase);
		rc = M0_FSO_AGAIN;
	}
	return M0_RC(rc);
}

static void rpc_link_fom_fini_common(struct m0_fom *fom, bool connected)
{
	struct m0_rpc_link *rlink;

	M0_ENTRY("fom=%p", fom);

	rlink = container_of(fom, struct m0_rpc_link, rlk_fom);
	m0_fom_fini(fom);
	rlink->rlk_connected = connected && (rlink->rlk_rc == 0);
	m0_chan_broadcast_lock(&rlink->rlk_wait);

	M0_LEAVE();
}

static void rpc_link_conn_fom_fini(struct m0_fom *fom)
{
	rpc_link_fom_fini_common(fom, true);
}

static void rpc_link_disc_fom_fini(struct m0_fom *fom)
{
	rpc_link_fom_fini_common(fom, false);
}

static struct m0_sm_state_descr rpc_link_conn_state_descr[] = {

	[M0_RLS_INIT] = {
		.sd_flags       = M0_SDF_INITIAL,
		.sd_name        = "Initialised",
		.sd_allowed     = M0_BITS(M0_RLS_CONN_FAILURE,
					  M0_RLS_CONN_CONNECTING)
	},
	[M0_RLS_CONN_FAILURE] = {
		.sd_flags       = 0,
		.sd_name        = "Conn failure",
		.sd_allowed     = M0_BITS(M0_RLS_FINI)
	},
	[M0_RLS_SESS_FAILURE] = {
		.sd_flags       = 0,
		.sd_name        = "Session failure",
		.sd_allowed     = M0_BITS(M0_RLS_CONN_TERMINATING)
	},
	[M0_RLS_CONN_TERMINATING] = {
		.sd_flags       = 0,
		.sd_name        = "Connection terminating",
		.sd_allowed     = M0_BITS(M0_RLS_FINI)
	},
	[M0_RLS_CONN_CONNECTING] = {
		.sd_flags       = 0,
		.sd_name        = "Connection establishing",
		.sd_allowed     = M0_BITS(M0_RLS_CONN_FAILURE,
					  M0_RLS_SESS_FAILURE,
					  M0_RLS_SESS_ESTABLISHING)
	},
	[M0_RLS_SESS_ESTABLISHING] = {
		.sd_flags       = 0,
		.sd_name        = "Session establishing",
		.sd_allowed     = M0_BITS(M0_RLS_SESS_FAILURE,
					  M0_RLS_FINI)
	},
	[M0_RLS_FINI] = {
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "Fini",
		.sd_allowed     = 0
	},
};

static const struct m0_sm_conf rpc_link_conn_sm_conf = {
	.scf_name      = "rpc_link connection state machine",
	.scf_nr_states = ARRAY_SIZE(rpc_link_conn_state_descr),
	.scf_state     = rpc_link_conn_state_descr
};

static struct m0_sm_state_descr rpc_link_disc_state_descr[] = {

	[M0_RLS_INIT] = {
		.sd_flags       = M0_SDF_INITIAL,
		.sd_name        = "Initialised",
		.sd_allowed     = M0_BITS(M0_RLS_SESS_WAIT_IDLE)
	},
	[M0_RLS_CONN_FAILURE] = {
		.sd_flags       = 0,
		.sd_name        = "Conn failure",
		.sd_allowed     = M0_BITS(M0_RLS_FINI)
	},
	[M0_RLS_SESS_FAILURE] = {
		.sd_flags       = 0,
		.sd_name        = "Session failure",
		.sd_allowed     = M0_BITS(M0_RLS_CONN_TERMINATING)
	},
	[M0_RLS_SESS_WAIT_IDLE] = {
		.sd_flags       = 0,
		.sd_name        = "Waiting for session is idle",
		.sd_allowed     = M0_BITS(M0_RLS_SESS_FAILURE,
					  M0_RLS_SESS_TERMINATING)
	},
	[M0_RLS_SESS_TERMINATING] = {
		.sd_flags       = 0,
		.sd_name        = "Session termination",
		.sd_allowed     = M0_BITS(M0_RLS_SESS_FAILURE,
					  M0_RLS_CONN_FAILURE,
					  M0_RLS_CONN_TERMINATING)
	},
	[M0_RLS_CONN_TERMINATING] = {
		.sd_flags       = 0,
		.sd_name        = "Connection termination",
		.sd_allowed     = M0_BITS(M0_RLS_CONN_FAILURE,
					  M0_RLS_FINI)
	},
	[M0_RLS_FINI] = {
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "Fini",
		.sd_allowed     = 0
	},
};

static const struct m0_sm_conf rpc_link_disc_sm_conf = {
	.scf_name      = "rpc_link disconnection state machine",
	.scf_nr_states = ARRAY_SIZE(rpc_link_disc_state_descr),
	.scf_state     = rpc_link_disc_state_descr
};

extern struct m0_reqh_service_type m0_rpc_service_type;

M0_INTERNAL int m0_rpc_link_module_init(void)
{
	m0_fom_type_init(&rpc_link_conn_fom_type, M0_RPC_LINK_CONN_OPCODE,
			 &rpc_link_conn_fom_type_ops,
			 &m0_rpc_service_type, &rpc_link_conn_sm_conf);
	m0_fom_type_init(&rpc_link_disc_fom_type, M0_RPC_LINK_DISC_OPCODE,
			 &rpc_link_disc_fom_type_ops,
			 &m0_rpc_service_type, &rpc_link_disc_sm_conf);
	return 0;
}

M0_INTERNAL void m0_rpc_link_module_fini(void)
{
}

M0_INTERNAL int m0_rpc_link_init(struct m0_rpc_link *rlink,
				 struct m0_rpc_machine *mach,
				 struct m0_fid *svc_fid,
				 const char *ep,
				 uint64_t max_rpcs_in_flight)
{
	struct m0_net_end_point *net_ep;
	int                      rc;

	M0_ENTRY("rlink=%p ep=%s", rlink, ep);

	rlink->rlk_connected = false;
	rlink->rlk_rc        = 0;

	rc = m0_net_end_point_create(&net_ep, &mach->rm_tm, ep);
	if (rc == 0) {
		rc = m0_rpc_conn_init(&rlink->rlk_conn, svc_fid, net_ep, mach,
				      max_rpcs_in_flight);
		m0_net_end_point_put(net_ep);
	}
	if (rc == 0) {
		rc = m0_rpc_session_init(&rlink->rlk_sess, &rlink->rlk_conn);
		if (rc != 0)
			m0_rpc_conn_fini(&rlink->rlk_conn);
	}
	if (rc == 0) {
		m0_mutex_init(&rlink->rlk_wait_mutex);
		m0_chan_init(&rlink->rlk_wait, &rlink->rlk_wait_mutex);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_rpc_link_fini(struct m0_rpc_link *rlink)
{
	M0_PRE(!rlink->rlk_connected);
	m0_chan_fini_lock(&rlink->rlk_wait);
	m0_mutex_fini(&rlink->rlk_wait_mutex);
	/*
	 * The link may be already discharged, as the peered service object
	 * might be announced dead. So need to double-check if the underlying
	 * session and connection really require finalising.
	 */
	if (session_state(&rlink->rlk_sess) != M0_RPC_SESSION_FINALISED)
		m0_rpc_session_fini(&rlink->rlk_sess);
	if (conn_state(&rlink->rlk_conn) != M0_RPC_CONN_FINALISED)
		m0_rpc_conn_fini(&rlink->rlk_conn);
}

M0_INTERNAL void m0_rpc_link_reset(struct m0_rpc_link *rlink)
{
	m0_rpc_session_reset(&rlink->rlk_sess);
	m0_rpc_conn_reset(&rlink->rlk_conn);
	rlink->rlk_rc = 0;
}

static void rpc_link_fom_queue(struct m0_rpc_link *rlink,
			       struct m0_clink *wait_clink,
			       const struct m0_fom_type *fom_type,
			       const struct m0_fom_ops *fom_ops)
{
	struct m0_rpc_machine *mach = rlink->rlk_conn.c_rpc_machine;

	M0_ENTRY("rlink=%p", rlink);
	M0_PRE(ergo(wait_clink != NULL, wait_clink->cl_is_oneshot));

	rlink->rlk_rc = 0;
	if (wait_clink != NULL)
		m0_clink_add_lock(&rlink->rlk_wait, wait_clink);
	M0_SET0(&rlink->rlk_fom);
	m0_fom_init(&rlink->rlk_fom, fom_type, fom_ops, NULL, NULL,
		    mach->rm_reqh);
	m0_fom_queue(&rlink->rlk_fom);

	M0_LEAVE();
}

static int rpc_link_call_sync(struct m0_rpc_link *rlink,
			      m0_time_t abs_timeout,
			      rpc_link_cb_t cb)
{
	struct m0_clink clink;

	M0_ENTRY("rlink=%p", rlink);

	m0_clink_init(&clink, NULL);
	clink.cl_is_oneshot = true;
	cb(rlink, abs_timeout, &clink);
	m0_chan_wait(&clink);
	m0_clink_fini(&clink);

	return M0_RC(rlink->rlk_rc);
}

M0_INTERNAL void m0_rpc_link_connect_async(struct m0_rpc_link *rlink,
					   m0_time_t abs_timeout,
					   struct m0_clink *wait_clink)
{
	M0_ENTRY("rlink=%p", rlink);
	M0_PRE(!rlink->rlk_connected);
	M0_PRE(rlink->rlk_rc == 0);
	rlink->rlk_timeout = abs_timeout;
	rpc_link_fom_queue(rlink, wait_clink, &rpc_link_conn_fom_type,
			   &rpc_link_conn_fom_ops);
	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_link_connect_sync(struct m0_rpc_link *rlink,
					 m0_time_t abs_timeout)
{
	return rpc_link_call_sync(rlink, abs_timeout,
				  &m0_rpc_link_connect_async);
}

M0_INTERNAL void m0_rpc_link_disconnect_async(struct m0_rpc_link *rlink,
					      m0_time_t abs_timeout,
					      struct m0_clink *wait_clink)
{
	M0_ENTRY("rlink=%p", rlink);
	M0_PRE(rlink->rlk_connected);
	M0_PRE(rlink->rlk_rc == 0);
	rlink->rlk_timeout = abs_timeout;
	rpc_link_fom_queue(rlink, wait_clink, &rpc_link_disc_fom_type,
			   &rpc_link_disc_fom_ops);
	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_link_disconnect_sync(struct m0_rpc_link *rlink,
					    m0_time_t abs_timeout)
{
	return rpc_link_call_sync(rlink, abs_timeout,
				  &m0_rpc_link_disconnect_async);
}

M0_INTERNAL bool m0_rpc_link_is_connected(const struct m0_rpc_link *rlink)
{
	return rlink->rlk_connected;
}

M0_INTERNAL const char *m0_rpc_link_end_point(const struct m0_rpc_link *rlink)
{
	return m0_rpc_conn_addr(&rlink->rlk_conn);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of rpc_link group */

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
