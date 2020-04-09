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
 * Original creation date: 08/24/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"    /* M0_BITS */
#include "lib/bitstring.h"
#include "lib/arith.h"
#include "lib/finject.h"
#include "lib/uuid.h"
#include "fop/fop.h"
#include "module/instance.h"  /* m0_get */
#include "reqh/reqh.h"        /* m0_reqh */
#include "rpc/rpc_internal.h"
#include "conf/helpers.h"     /* m0_conf_service_ep_is_known */
#include "conf/obj_ops.h"     /* m0_conf_obj_get, m0_conf_obj_put */
#include "conf/cache.h"       /* m0_conf_cache_lock, m0_conf_cache_unlock */
#include "ha/note.h"          /* M0_NC_FAILED */
#include "ha/msg.h"           /* M0_HA_MSG_EVENT_RPC */
#include "ha/ha.h"            /* m0_ha_send */

/**
   @addtogroup rpc_session

   @{

   This file implements functions related to m0_rpc_conn.
 */

M0_INTERNAL struct m0_rpc_chan *rpc_chan_get(struct m0_rpc_machine *machine,
					     struct m0_net_end_point *dest_ep,
					     uint64_t max_rpcs_in_flight);
M0_INTERNAL void rpc_chan_put(struct m0_rpc_chan *chan);

static bool rpc_conn__on_service_event_cb(struct m0_clink *clink);
static void rpc_conn_sessions_cleanup_fail(struct m0_rpc_conn *conn, bool fail);
static bool rpc_conn__on_cache_expired_cb(struct m0_clink *clink);
static bool rpc_conn__on_cache_ready_cb(struct m0_clink *clink);
static struct m0_confc *rpc_conn2confc(const struct m0_rpc_conn *conn);
static void rpc_conn_ha_timer_cb(struct m0_sm_timer *timer);
static void reqh_service_ha_state_set(struct m0_rpc_conn *conn, uint8_t state);

enum {
	/**
	 * Interval being elapsed after rpc item sending has to result in
	 * sending M0_NC_TRANSIENT state to HA.
	 *
	 * default value, in milliseconds
	 */
	RPC_HA_INTERVAL = 500 * M0_TIME_ONE_MSEC,
};

static struct m0_rpc_conn_ha_cfg rpc_conn_ha_cfg = {
	.rchc_ops = {
		.cho_ha_timer_cb = rpc_conn_ha_timer_cb,
		.cho_ha_notify   = reqh_service_ha_state_set,
	},
	.rchc_ha_interval = RPC_HA_INTERVAL
};

/**
   Attaches session 0 object to conn object.
 */
static int session_zero_attach(struct m0_rpc_conn *conn);

/**
   Detaches session 0 from conn
 */
static void session_zero_detach(struct m0_rpc_conn *conn);

static int __conn_init(struct m0_rpc_conn      *conn,
		       struct m0_net_end_point *ep,
		       struct m0_rpc_machine   *machine,
		       uint64_t			max_rpcs_in_flight);
/**
   Common code in m0_rpc_conn_fini() and init failed case in __conn_init()
 */
static void __conn_fini(struct m0_rpc_conn *conn);

static void conn_failed(struct m0_rpc_conn *conn, int32_t error);

static void deregister_all_item_sources(struct m0_rpc_conn *conn);

/*
 * This is sender side item_ops of conn_establish fop.
 * Receiver side conn_establish fop has different item_ops
 * rcv_conn_establish_item_ops defined in rpc/session_fops.c
 */
static const struct m0_rpc_item_ops conn_establish_item_ops = {
	.rio_replied = m0_rpc_conn_establish_reply_received,
};

static const struct m0_rpc_item_ops conn_terminate_item_ops = {
	.rio_replied = m0_rpc_conn_terminate_reply_received,
};

static struct m0_sm_state_descr conn_states[] = {
	[M0_RPC_CONN_INITIALISED] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Initialised",
		.sd_allowed   = M0_BITS(M0_RPC_CONN_CONNECTING,
					M0_RPC_CONN_ACTIVE, /* rcvr side only */
					M0_RPC_CONN_FINALISED,
					M0_RPC_CONN_FAILED)
	},
	[M0_RPC_CONN_CONNECTING] = {
		.sd_name      = "Connecting",
		.sd_allowed   = M0_BITS(M0_RPC_CONN_ACTIVE,
					M0_RPC_CONN_FAILED)
	},
	[M0_RPC_CONN_ACTIVE] = {
		.sd_name      = "Active",
		.sd_allowed   = M0_BITS(M0_RPC_CONN_TERMINATING,
					M0_RPC_CONN_TERMINATED, /* rcvr side */
					M0_RPC_CONN_FAILED)
	},
	[M0_RPC_CONN_TERMINATING] = {
		.sd_name      = "Terminating",
		.sd_allowed   = M0_BITS(M0_RPC_CONN_TERMINATED,
					M0_RPC_CONN_FAILED)
	},
	[M0_RPC_CONN_TERMINATED] = {
		.sd_name      = "Terminated",
		.sd_allowed   = M0_BITS(M0_RPC_CONN_INITIALISED,
					M0_RPC_CONN_FINALISED)
	},
	[M0_RPC_CONN_FAILED] = {
		.sd_flags     = M0_SDF_FAILURE,
		.sd_name      = "Failed",
		.sd_allowed   = M0_BITS(M0_RPC_CONN_INITIALISED,
					M0_RPC_CONN_FINALISED)
	},
	[M0_RPC_CONN_FINALISED] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Finalised",
	},
};

static const struct m0_sm_conf conn_conf = {
	.scf_name      = "Conn states",
	.scf_nr_states = ARRAY_SIZE(conn_states),
	.scf_state     = conn_states
};

M0_INTERNAL void conn_state_set(struct m0_rpc_conn *conn, int state)
{
	M0_PRE(conn != NULL);

	M0_LOG(M0_INFO, "%p[%s] %s -> %s", conn,
		m0_rpc_conn_is_snd(conn) ? "SENDER" : "RECEIVER",
		conn_states[conn->c_sm.sm_state].sd_name,
		conn_states[state].sd_name);
	m0_sm_state_set(&conn->c_sm, state);
}

/**
   Checks connection object invariant.

   Function is also called from session_foms.c, hence cannot be static.
 */
M0_INTERNAL bool m0_rpc_conn_invariant(const struct m0_rpc_conn *conn)
{
	struct m0_rpc_session *session0;
	struct m0_tl          *conn_list;
	int                    s0nr; /* Number of sessions with id == 0 */
	bool                   sender_end;
	bool                   recv_end;
	bool                   ok;

	if (conn == NULL || conn->c_rpc_machine == NULL)
		return false;

	session0   = NULL;
	sender_end = m0_rpc_conn_is_snd(conn);
	recv_end   = m0_rpc_conn_is_rcv(conn);
	conn_list  = sender_end ?
			&conn->c_rpc_machine->rm_outgoing_conns :
			&conn->c_rpc_machine->rm_incoming_conns;
	s0nr       = 0;

	/* conditions that should be true irrespective of conn state */
	ok = _0C(sender_end != recv_end) &&
		_0C(rpc_conn_tlist_contains(conn_list, conn)) &&
		_0C(M0_CHECK_EX(m0_tlist_invariant(&rpc_session_tl,
						   &conn->c_sessions))) &&
		_0C(rpc_session_tlist_length(&conn->c_sessions) ==
		    conn->c_nr_sessions) &&
		_0C(conn_state(conn) <= M0_RPC_CONN_TERMINATED) &&
	     /*
	      * Each connection has exactly one session with id SESSION_ID_0.
	      * From m0_rpc_conn_init() to m0_rpc_conn_fini(), this session0 is
	      * either in IDLE state or BUSY state.
	      */
	     m0_tl_forall(rpc_session, s, &conn->c_sessions,
		  _0C(ergo(s->s_session_id == SESSION_ID_0,
		       ++s0nr &&
		       (session0 = s) && /*'=' is intentional */
		       M0_IN(session_state(s), (M0_RPC_SESSION_IDLE,
						M0_RPC_SESSION_BUSY))))) &&
		_0C(session0 != NULL) &&
		_0C(s0nr == 1);
	if (!ok)
		return false;
	/*
	 * A connection not in ACTIVE or FAILED state has sessoins with only
	 * specific states except of session0.
	 */
	ok = M0_IN(conn_state(conn), (M0_RPC_CONN_ACTIVE,M0_RPC_CONN_FAILED)) ||
	     m0_tl_forall(rpc_session, s, &conn->c_sessions,
		ergo(s->s_session_id != SESSION_ID_0,
		     ergo(M0_IN(conn_state(conn), (M0_RPC_CONN_INITIALISED,
						   M0_RPC_CONN_CONNECTING)),
			  session_state(s) == M0_RPC_SESSION_INITIALISED) &&
		     ergo(M0_IN(conn_state(conn), (M0_RPC_CONN_TERMINATING,
						   M0_RPC_CONN_TERMINATED)),
			  M0_IN(session_state(s),(M0_RPC_SESSION_INITIALISED,
						  M0_RPC_SESSION_TERMINATED,
						  M0_RPC_SESSION_FAILED)))));
	if (!_0C(ok))
		return false;

	switch (conn_state(conn)) {
	case M0_RPC_CONN_INITIALISED:
		return  _0C(conn->c_sender_id == SENDER_ID_INVALID) &&
			_0C(conn->c_nr_sessions >= 1) &&
			_0C(session_state(session0) == M0_RPC_SESSION_IDLE);

	case M0_RPC_CONN_CONNECTING:
		return  _0C(conn->c_sender_id == SENDER_ID_INVALID) &&
			_0C(conn->c_nr_sessions >= 1);

	case M0_RPC_CONN_ACTIVE:
		return  _0C(conn->c_sender_id != SENDER_ID_INVALID) &&
			_0C(conn->c_nr_sessions >= 1);

	case M0_RPC_CONN_TERMINATING:
		return  _0C(conn->c_nr_sessions >= 1) &&
			_0C(conn->c_sender_id != SENDER_ID_INVALID);

	case M0_RPC_CONN_TERMINATED:
		return	_0C(conn->c_nr_sessions >= 1) &&
			_0C(conn->c_sender_id != SENDER_ID_INVALID) &&
			_0C(conn->c_sm.sm_rc == 0);

	case M0_RPC_CONN_FAILED:
		return _0C(conn->c_sm.sm_rc != 0);

	default:
		return false;
	}
	/* Should never reach here */
	M0_ASSERT(0);
}

/**
   Returns true iff @conn is sender end of rpc connection.
 */
M0_INTERNAL bool m0_rpc_conn_is_snd(const struct m0_rpc_conn *conn)
{
	return (conn->c_flags & RCF_SENDER_END) == RCF_SENDER_END;
}

/**
   Returns true iff @conn is receiver end of rpc connection.
 */
M0_INTERNAL bool m0_rpc_conn_is_rcv(const struct m0_rpc_conn *conn)
{
	return (conn->c_flags & RCF_RECV_END) == RCF_RECV_END;
}

M0_INTERNAL int m0_rpc_conn_init(struct m0_rpc_conn      *conn,
				 struct m0_fid           *svc_fid,
				 struct m0_net_end_point *ep,
				 struct m0_rpc_machine   *machine,
				 uint64_t                 max_rpcs_in_flight)
{
	int                   rc;
	struct m0_conf_cache *cc;

	M0_ENTRY("conn=%p, svc_fid=%p ep=%s", conn, svc_fid,
		 ep != NULL ? ep->nep_addr : NULL );
	M0_PRE(conn != NULL && machine != NULL && ep != NULL);

	M0_SET0(conn);

	cc = &m0_reqh2confc(machine->rm_reqh)->cc_cache;
	/*
	 * Lock ordering -
	 * 1. Conf cache lock
	 * 2. RPC machine lock
	 */
	if (svc_fid != NULL && m0_fid_is_set(svc_fid))
		m0_conf_cache_lock(cc);
	m0_rpc_machine_lock(machine);

	if (svc_fid != NULL)
		conn->c_svc_fid = *svc_fid;
	conn->c_flags = RCF_SENDER_END;
	m0_uuid_generate(&conn->c_uuid);

	rc = __conn_init(conn, ep, machine, max_rpcs_in_flight);
	if (rc == 0) {
		m0_sm_init(&conn->c_sm, &conn_conf, M0_RPC_CONN_INITIALISED,
			   &machine->rm_sm_grp);
		m0_rpc_machine_add_conn(machine, conn);
		M0_LOG(M0_INFO, "%p INITIALISED \n", conn);
	}

	M0_POST(ergo(rc == 0, m0_rpc_conn_invariant(conn) &&
			      conn_state(conn) == M0_RPC_CONN_INITIALISED &&
			      m0_rpc_conn_is_snd(conn)));

	m0_rpc_machine_unlock(machine);
	if (svc_fid != NULL && m0_fid_is_set(svc_fid))
		m0_conf_cache_unlock(cc);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_conn_init);

static struct m0_confc *rpc_conn2confc(const struct m0_rpc_conn *conn)
{

	return m0_reqh2confc(conn->c_rpc_machine->rm_reqh);
}

M0_INTERNAL struct m0_conf_obj *m0_rpc_conn2svc(const struct m0_rpc_conn *conn)
{
	struct m0_conf_obj *obj;

	if (!m0_fid_is_set(&conn->c_svc_fid))
		return NULL;

	M0_ASSERT(m0_conf_fid_type(&conn->c_svc_fid) == &M0_CONF_SERVICE_TYPE);
	obj = m0_conf_cache_lookup(&rpc_conn2confc(conn)->cc_cache,
				   &conn->c_svc_fid);
	if (obj == NULL)
		M0_LOG(M0_WARN, FID_F" not found in cache",
				FID_P(&conn->c_svc_fid));
	return obj;
}

static void __conn_ha_subscribe(struct m0_rpc_conn *conn)
{
	struct m0_conf_obj *svc_obj;

	M0_ENTRY("conn = %p", conn);
	if (!m0_fid_is_set(&conn->c_svc_fid))
		goto leave;
	svc_obj = m0_conf_cache_lookup(&rpc_conn2confc(conn)->cc_cache,
				       &conn->c_svc_fid);
	M0_ASSERT_INFO(svc_obj != NULL, "unknown service " FID_F,
		       FID_P(&conn->c_svc_fid));
	M0_ASSERT(conn->c_ha_clink.cl_cb != NULL);
	M0_LOG(M0_DEBUG, "svc_fid "FID_F", cs_type=%d", FID_P(&conn->c_svc_fid),
	       M0_CONF_CAST(svc_obj, m0_conf_service)->cs_type);
	m0_conf_obj_get(svc_obj);
	m0_clink_add(&svc_obj->co_ha_chan, &conn->c_ha_clink);
leave:
	M0_LEAVE("service fid = "FID_F, FID_P(&conn->c_svc_fid));
}

static void __conn_ha_unsubscribe(struct m0_rpc_conn *conn)
{
	struct m0_conf_obj *svc_obj;

	if (!m0_fid_is_set(&conn->c_svc_fid) ||
	    !m0_clink_is_armed(&conn->c_ha_clink))
		return;
	svc_obj = m0_rpc_conn2svc(conn);
	M0_ASSERT(svc_obj != NULL);
	m0_conf_obj_put(svc_obj);
	m0_clink_del(&conn->c_ha_clink);
	conn->c_ha_clink.cl_chan = NULL;
}

static int __conn_init(struct m0_rpc_conn      *conn,
		       struct m0_net_end_point *ep,
		       struct m0_rpc_machine   *machine,
		       uint64_t			max_rpcs_in_flight)
{
	int rc;

	M0_ENTRY();
	M0_PRE(conn != NULL && ep != NULL &&
	       m0_rpc_machine_is_locked(machine) &&
	       m0_rpc_conn_is_snd(conn) != m0_rpc_conn_is_rcv(conn));

	conn->c_rpcchan = rpc_chan_get(machine, ep, max_rpcs_in_flight);
	if (conn->c_rpcchan == NULL) {
		M0_SET0(conn);
		return M0_RC(-ENOMEM);
	}

	conn->c_rpc_machine = machine;
	conn->c_sender_id   = SENDER_ID_INVALID;
	conn->c_nr_sessions = 0;
	conn->c_ha_cfg      = &rpc_conn_ha_cfg;

	m0_clink_init(&conn->c_ha_clink, rpc_conn__on_service_event_cb);
	/*
	 * conf cache lock is held by the caller at this point if
	 * conn->c_svc_fid has a non-zero value. Also in that case, it is
	 * acquired in the correct order(that is, before rpc machine lock).
	 */
	__conn_ha_subscribe(conn);
	m0_clink_init(&conn->c_conf_exp_clink, rpc_conn__on_cache_expired_cb);
	m0_clink_init(&conn->c_conf_ready_clink, rpc_conn__on_cache_ready_cb);
	if (machine->rm_reqh != NULL && m0_fid_is_set(&conn->c_svc_fid)) {
		m0_clink_add_lock(&machine->rm_reqh->rh_conf_cache_exp,
				  &conn->c_conf_exp_clink);
		m0_clink_add_lock(&machine->rm_reqh->rh_conf_cache_ready,
				  &conn->c_conf_ready_clink);
		M0_LOG(M0_DEBUG, "conn %p has subscribed on rconfc events,\
		       fid "FID_F, conn, FID_P(&conn->c_svc_fid));
	}
	rpc_session_tlist_init(&conn->c_sessions);
	item_source_tlist_init(&conn->c_item_sources);
	rpc_conn_tlink_init(conn);

	rc = session_zero_attach(conn);
	if (rc != 0) {
		__conn_fini(conn);
		M0_SET0(conn);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_rpc_conn_reset(struct m0_rpc_conn *conn)
{
	struct m0_rpc_machine *machine = conn->c_rpc_machine;
	struct m0_rpc_session *session0;

	m0_rpc_machine_lock(machine);
	if (conn_state(conn) == M0_RPC_CONN_INITIALISED) {
		m0_rpc_machine_unlock(machine);
		return;
	}
	conn_state_set(conn, M0_RPC_CONN_INITIALISED);
	session0 = m0_rpc_conn_session0(conn);
	session0->s_xid = 0;
	conn->c_sender_id = SENDER_ID_INVALID;
	M0_POST(m0_rpc_conn_invariant(conn));
	m0_rpc_machine_unlock(machine);
}

static int session_zero_attach(struct m0_rpc_conn *conn)
{
	struct m0_rpc_session *session;
	int                    rc;

	M0_ENTRY("conn: %p", conn);
	M0_PRE(conn != NULL && m0_rpc_machine_is_locked(conn->c_rpc_machine));

	if (M0_FI_ENABLED("out-of-memory")) return -ENOMEM;

	M0_ALLOC_PTR(session);
	if (session == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_rpc_session_init_locked(session, conn);
	if (rc != 0) {
		m0_free(session);
		return M0_RC(rc);
	}

	session->s_session_id = SESSION_ID_0;
	/* It is done as there is no need to establish session0 explicitly
	 * and direct transition from INITIALISED => IDLE is not allowed.
	 */
	session_state_set(session, M0_RPC_SESSION_IDLE);

	M0_ASSERT(m0_rpc_session_invariant(session));
	return M0_RC(0);
}

static void __conn_fini(struct m0_rpc_conn *conn)
{
	M0_ENTRY("conn: %p", conn);
	M0_PRE(conn != NULL);
	/*
	 * There must be no HA subscription to the moment to prevent
	 * rpc_conn__on_service_event_cb() from being called when object is
	 * already about to die.
	 */
	M0_PRE(!m0_clink_is_armed(&conn->c_ha_clink));
	m0_clink_cleanup(&conn->c_conf_exp_clink);
	m0_clink_cleanup(&conn->c_conf_ready_clink);
	m0_rpc_conn_ha_timer_stop(conn);

	rpc_chan_put(conn->c_rpcchan);

	rpc_session_tlist_fini(&conn->c_sessions);
	item_source_tlist_fini(&conn->c_item_sources);
	rpc_conn_tlink_fini(conn);
	m0_clink_fini(&conn->c_ha_clink);
	m0_clink_fini(&conn->c_conf_exp_clink);
	conn->c_svc_fid = M0_FID0;
	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_rcv_conn_init(struct m0_rpc_conn *conn,
				     struct m0_net_end_point *ep,
				     struct m0_rpc_machine *machine,
				     const struct m0_uint128 *uuid)
{
	int rc;

	M0_ENTRY("conn: %p, ep_addr: %s, machine: %p", conn,
		 (char *)ep->nep_addr, machine);
	M0_ASSERT(conn != NULL && ep != NULL);
	M0_PRE(m0_rpc_machine_is_locked(machine));

	M0_SET0(conn);

	conn->c_flags = RCF_RECV_END;
	conn->c_uuid = *uuid;

	rc = __conn_init(conn, ep, machine, 8 /* max packets in flight */);
	if (rc == 0) {
		m0_sm_init(&conn->c_sm, &conn_conf, M0_RPC_CONN_INITIALISED,
			   &machine->rm_sm_grp);
		m0_rpc_machine_add_conn(machine, conn);
		M0_LOG(M0_INFO, "%p INITIALISED \n", conn);
	}

	M0_POST(ergo(rc == 0, m0_rpc_conn_invariant(conn) &&
			      conn_state(conn) == M0_RPC_CONN_INITIALISED &&
			      m0_rpc_conn_is_rcv(conn)));
	M0_POST(m0_rpc_machine_is_locked(machine));

	return M0_RC(rc);
}

M0_INTERNAL void m0_rpc_conn_fini(struct m0_rpc_conn *conn)
{
	struct m0_rpc_machine *machine;
	struct m0_rpc_session *session0;

	M0_ENTRY("conn: %p", conn);
	M0_PRE(conn != NULL && conn->c_rpc_machine != NULL);

	machine = conn->c_rpc_machine;

	m0_rpc_machine_lock(machine);

	session0 = m0_rpc_conn_session0(conn);
	if (!M0_IN(session_state(session0), (M0_RPC_SESSION_BUSY,
					     M0_RPC_SESSION_IDLE)))
		m0_rpc_session_quiesce(session0);
	m0_sm_timedwait(&session0->s_sm, M0_BITS(M0_RPC_SESSION_IDLE),
			M0_TIME_NEVER);

	m0_rpc_conn_fini_locked(conn);
	/* Don't look in conn after this point */
	m0_rpc_machine_unlock(machine);

	M0_LEAVE();
}
M0_EXPORTED(m0_rpc_conn_fini);

M0_INTERNAL int m0_rpc_conn_ha_subscribe(struct m0_rpc_conn *conn,
					 struct m0_fid      *svc_fid)
{
	struct m0_conf_obj   *svc_obj;
	const char           *ep;
	struct m0_conf_cache *cc;

	M0_ENTRY("conn %p, svc_fid "FID_F, conn, FID_P(svc_fid));
	M0_PRE(_0C(conn != NULL) && _0C(svc_fid != NULL));
	M0_PRE(!m0_fid_is_set(&conn->c_svc_fid) ||
	       m0_fid_eq(&conn->c_svc_fid, svc_fid));

	if (M0_IN(conn_state(conn), (M0_RPC_CONN_ACTIVE))) {

		svc_obj = m0_conf_cache_lookup(&rpc_conn2confc(conn)->cc_cache,
					       svc_fid);
		if (svc_obj == NULL)
			return M0_ERR_INFO(-ENOENT, "unknown service " FID_F,
					   FID_P(svc_fid));
		/*
		 * found service object must match to already established
		 * connection endpoint, i.e. the endpoint must be known to the
		 * service configuration
		 */
		ep = m0_rpc_conn_addr(conn);
		if (!m0_conf_service_ep_is_known(svc_obj, ep))
			return M0_ERR_INFO(-EINVAL, "Conn %p ep %s "
					   "unknown to svc_obj " FID_F, conn,
					   ep, FID_P(&svc_obj->co_id));
	}

	cc = &rpc_conn2confc(conn)->cc_cache;
	/*
	 * Lock ordering -
	 * 1. Conf cache lock
	 * 2. RPC machine lock
	 */
	m0_conf_cache_lock(cc);
	m0_rpc_machine_lock(conn->c_rpc_machine);
	conn->c_svc_fid = *svc_fid;
	__conn_ha_subscribe(conn);
	m0_rpc_machine_unlock(conn->c_rpc_machine);
	m0_conf_cache_unlock(cc);

	return M0_RC(0);
}

M0_INTERNAL void m0_rpc_conn_ha_unsubscribe(struct m0_rpc_conn *conn)
{
	struct m0_conf_cache *cc;

	M0_PRE(conn != NULL);
	if (!m0_fid_is_set(&conn->c_svc_fid))
		return;

	cc = &rpc_conn2confc(conn)->cc_cache;
	/*
	 * Lock ordering -
	 * 1. Conf cache lock
	 * 2. RPC machine lock
	 */
	m0_conf_cache_lock(cc);
	m0_rpc_machine_lock(conn->c_rpc_machine);
	__conn_ha_unsubscribe(conn);
	conn->c_svc_fid = M0_FID0;
	m0_rpc_machine_unlock(conn->c_rpc_machine);
	m0_conf_cache_unlock(cc);
}

M0_INTERNAL void m0_rpc_conn_fini_locked(struct m0_rpc_conn *conn)
{
	M0_ENTRY("conn: %p", conn);
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));

	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_PRE(M0_IN(conn_state(conn), (M0_RPC_CONN_TERMINATED,
					M0_RPC_CONN_FAILED,
					M0_RPC_CONN_INITIALISED)));

	rpc_conn_tlist_del(conn);
	M0_LOG(M0_DEBUG, "rpcmach %p conn %p deleted from %s list",
		conn->c_rpc_machine, conn,
		(conn->c_flags & RCF_SENDER_END) ? "outgoing" : "incoming");
	session_zero_detach(conn);
	__conn_fini(conn);
	conn_state_set(conn, M0_RPC_CONN_FINALISED);
	m0_sm_fini(&conn->c_sm);
	M0_LOG(M0_INFO, "%p FINALISED \n", conn);
	M0_SET0(conn);
	M0_LEAVE();
}

static void session_zero_detach(struct m0_rpc_conn *conn)
{
	struct m0_rpc_session *session;

	M0_ENTRY("conn: %p", conn);
	M0_PRE(conn != NULL);
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));

	session = m0_rpc_conn_session0(conn);
	M0_ASSERT(session_state(session) == M0_RPC_SESSION_IDLE);

	session_state_set(session, M0_RPC_SESSION_TERMINATED);
	m0_rpc_session_fini_locked(session);
	m0_free(session);

	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_conn_timedwait(struct m0_rpc_conn *conn,
				      uint64_t            states,
				      const m0_time_t     timeout)
{
	int rc;

	M0_ENTRY("conn: %p, abs_timeout: "TIME_F, conn,
		 TIME_P(timeout));
	M0_PRE(conn != NULL && conn->c_rpc_machine != NULL);

	m0_rpc_machine_lock(conn->c_rpc_machine);
	M0_ASSERT(m0_rpc_conn_invariant(conn));

	rc = m0_sm_timedwait(&conn->c_sm, states, timeout);

	M0_ASSERT(m0_rpc_conn_invariant(conn));
	m0_rpc_machine_unlock(conn->c_rpc_machine);

	return M0_RC(rc ?: conn->c_sm.sm_rc);
}
M0_EXPORTED(m0_rpc_conn_timedwait);

M0_INTERNAL void m0_rpc_conn_add_session(struct m0_rpc_conn *conn,
					 struct m0_rpc_session *session)
{
	struct m0_rpc_machine_watch *watch;

	rpc_session_tlist_add(&conn->c_sessions, session);
	conn->c_nr_sessions++;

	m0_tl_for(rmach_watch, &conn->c_rpc_machine->rm_watch, watch) {
		if (watch->mw_session_added != NULL)
			watch->mw_session_added(watch, session);
	} m0_tl_endfor;
}

M0_INTERNAL void m0_rpc_conn_remove_session(struct m0_rpc_session *session)
{
	M0_ASSERT(session->s_conn->c_nr_sessions > 0);

	rpc_session_tlist_del(session);
	session->s_conn->c_nr_sessions--;
}

/**
   Searches and returns session with session id 0.
   Note: Every rpc connection always has exactly one active session with
   session id 0.
 */
M0_INTERNAL struct m0_rpc_session *m0_rpc_conn_session0(const struct m0_rpc_conn
							*conn)
{
	struct m0_rpc_session *session0;

	session0 = m0_rpc_session_search(conn, SESSION_ID_0);

	M0_ASSERT(session0 != NULL);
	return session0;
}

M0_INTERNAL struct m0_rpc_session *m0_rpc_session_search(const struct
							 m0_rpc_conn *conn,
							 uint64_t session_id)
{
	M0_ENTRY("conn: %p, session_id: %llu", conn,
		 (unsigned long long) session_id);
	M0_ASSERT(conn != NULL);

	return m0_tl_find(rpc_session, session, &conn->c_sessions,
			  session->s_session_id == session_id);
}

M0_INTERNAL struct m0_rpc_session *m0_rpc_session_search_and_pop(
	const struct m0_rpc_conn *conn, uint64_t session_id)
{
	struct m0_rpc_session *session;

	M0_ENTRY("conn: %p, session_id: %" PRIu64, conn, (uint64_t) session_id);
	M0_PRE(conn != NULL);
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));

	session = m0_tl_find(rpc_session, session, &conn->c_sessions,
			     session->s_session_id == session_id);
	if (session != NULL)
		m0_rpc_conn_remove_session(session);

	M0_LEAVE("session: %p", session);
	return session;
}

M0_INTERNAL struct m0_rpc_session *m0_rpc_session_pop(
	const struct m0_rpc_conn *conn)
{
	struct m0_rpc_session *session;

	M0_ENTRY("conn: %p", conn);
	M0_PRE(conn != NULL);
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));

	session = m0_tl_find(rpc_session, session, &conn->c_sessions,
			     session->s_session_id != SESSION_ID_0);

	if (session != NULL)
		m0_rpc_conn_remove_session(session);

	M0_LEAVE("session: %p", session);
	return session;
}

M0_INTERNAL int m0_rpc_conn_create(struct m0_rpc_conn *conn,
				   struct m0_fid *svc_fid,
				   struct m0_net_end_point *ep,
				   struct m0_rpc_machine *rpc_machine,
				   uint64_t max_rpcs_in_flight,
				   m0_time_t abs_timeout)
{
	int rc;

	M0_ENTRY("conn: %p, svc_fid: %p, ep_addr: %s, "
		 "machine: %p max_rpcs_in_flight: %llu",
		 conn, svc_fid, (char *)ep->nep_addr, rpc_machine,
		 (unsigned long long)max_rpcs_in_flight);

	if (M0_FI_ENABLED("fake_error"))
		return M0_RC(-EINVAL);

	rc = m0_rpc_conn_init(conn, svc_fid, ep, rpc_machine,
			      max_rpcs_in_flight);
	if (rc == 0) {
		rc = m0_rpc_conn_establish_sync(conn, abs_timeout);
		if (rc != 0)
			m0_rpc_conn_fini(conn);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_conn_establish_sync(struct m0_rpc_conn *conn,
					   m0_time_t abs_timeout)
{
	int rc;

	M0_ENTRY();

	rc = m0_rpc_conn_establish(conn, abs_timeout);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_rpc_conn_timedwait(conn, M0_BITS(M0_RPC_CONN_ACTIVE,
						 M0_RPC_CONN_FAILED),
				   M0_TIME_NEVER);

	M0_POST(M0_IN(conn_state(conn),
		      (M0_RPC_CONN_ACTIVE, M0_RPC_CONN_FAILED)));
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_conn_establish_sync);

M0_INTERNAL int m0_rpc_conn_establish(struct m0_rpc_conn *conn,
				      m0_time_t abs_timeout)
{
	struct m0_fop         *fop;
	struct m0_rpc_session *session_0;
	struct m0_rpc_machine *machine;
	int                    rc;

	M0_ENTRY("conn: %p", conn);
	M0_PRE(conn != NULL && conn->c_rpc_machine != NULL);

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EINVAL);

	machine = conn->c_rpc_machine;

	fop = m0_fop_alloc(&m0_rpc_fop_conn_establish_fopt, NULL, machine);
	if (fop == NULL) {
		m0_rpc_machine_lock(machine);
		conn_failed(conn, -ENOMEM);
		m0_rpc_machine_unlock(machine);
		return M0_ERR(-ENOMEM);
	}

	m0_rpc_machine_lock(machine);

	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_ASSERT(conn_state(conn) == M0_RPC_CONN_INITIALISED &&
		  m0_rpc_conn_is_snd(conn));

	/* m0_rpc_fop_conn_establish FOP doesn't contain any data. */

	session_0 = m0_rpc_conn_session0(conn);

	rc = m0_rpc__fop_post(fop, session_0, &conn_establish_item_ops,
			      abs_timeout);
	if (rc == 0)
		conn_state_set(conn, M0_RPC_CONN_CONNECTING);
	/*
	 * It is possible that ->rio_replied() was called
	 * and connection is in failed state already.
	 */
	else if (conn_state(conn) == M0_RPC_CONN_INITIALISED)
		conn_failed(conn, M0_ERR(rc));
	m0_fop_put(fop);

	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_POST(ergo(rc != 0, conn_state(conn) == M0_RPC_CONN_FAILED));
	M0_ASSERT(ergo(rc == 0, conn_state(conn) == M0_RPC_CONN_CONNECTING));

	m0_rpc_machine_unlock(machine);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_conn_establish);

/**
   Moves conn to M0_RPC_CONN_FAILED state, setting error code to error.
 */
static void conn_failed(struct m0_rpc_conn *conn, int32_t error)
{
	M0_ENTRY("conn: %p, error: %d", conn, error);

	m0_sm_fail(&conn->c_sm, M0_RPC_CONN_FAILED, error);

	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_conn_establish_reply_received(struct m0_rpc_item *item)
{
	struct m0_rpc_fop_conn_establish_rep *reply = NULL;
	struct m0_rpc_machine                *machine;
	struct m0_rpc_conn                   *conn;
	struct m0_rpc_item                   *reply_item;
	int32_t                               rc;

	M0_ENTRY("item: %p", item);
	M0_PRE(item != NULL &&
	       item->ri_session != NULL &&
	       item->ri_session->s_session_id == SESSION_ID_0);

	conn    = item2conn(item);
	machine = conn->c_rpc_machine;

	M0_PRE(m0_rpc_machine_is_locked(machine));
	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_PRE(M0_IN(conn_state(conn), (M0_RPC_CONN_INITIALISED,
				       M0_RPC_CONN_CONNECTING)));

	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		reply_item = item->ri_reply;
		M0_ASSERT(reply_item != NULL &&
			  item->ri_session == reply_item->ri_session);
		reply = m0_fop_data(m0_rpc_item_to_fop(reply_item));
		rc    = reply->rcer_rc;
	}
	if (rc == 0) {
		M0_ASSERT(reply != NULL);
		if (reply->rcer_sender_id != SENDER_ID_INVALID) {
			conn->c_sender_id = reply->rcer_sender_id;
			conn_state_set(conn, M0_RPC_CONN_ACTIVE);
		} else
			rc = M0_ERR(-EPROTO);
	}
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "rpc item ERROR rc=%d", rc);
		conn_failed(conn, rc);
	}

	M0_POST(m0_rpc_conn_invariant(conn));
	M0_POST(M0_IN(conn_state(conn), (M0_RPC_CONN_FAILED,
					 M0_RPC_CONN_ACTIVE)));
	M0_LEAVE();
}

int m0_rpc_conn_destroy(struct m0_rpc_conn *conn, m0_time_t abs_timeout)
{
	int rc;

	M0_ENTRY("conn: %p", conn);

	m0_rpc_conn_ha_unsubscribe(conn);
	rc = m0_rpc_conn_terminate_sync(conn, abs_timeout);
	m0_rpc_conn_fini(conn);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_conn_destroy);

M0_INTERNAL int m0_rpc_conn_terminate_sync(struct m0_rpc_conn *conn,
					   m0_time_t abs_timeout)
{
	int rc;

	M0_ENTRY();

	rc = m0_rpc_conn_terminate(conn, abs_timeout);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_rpc_conn_timedwait(conn, M0_BITS(M0_RPC_CONN_TERMINATED,
						 M0_RPC_CONN_FAILED),
				   M0_TIME_NEVER);

	M0_POST(M0_IN(conn_state(conn), (M0_RPC_CONN_TERMINATED,
					 M0_RPC_CONN_FAILED)));
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_conn_terminate_sync);

M0_INTERNAL int m0_rpc_conn_terminate(struct m0_rpc_conn *conn,
				      m0_time_t abs_timeout)
{
	struct m0_rpc_fop_conn_terminate *args;
	struct m0_rpc_session            *session_0;
	struct m0_rpc_machine            *machine;
	struct m0_fop                    *fop;
	int                               rc;

	M0_ENTRY("conn: %p", conn);
	M0_PRE(conn != NULL);
	M0_PRE(conn->c_rpc_machine != NULL);

	machine = conn->c_rpc_machine;
	if (M0_FI_ENABLED("fail_allocation"))
		fop = NULL;
	else
		fop = m0_fop_alloc(&m0_rpc_fop_conn_terminate_fopt, NULL,
				   machine);
	m0_rpc_machine_lock(machine);
	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_PRE(M0_IN(conn_state(conn), (M0_RPC_CONN_ACTIVE,
					M0_RPC_CONN_TERMINATING)));

	deregister_all_item_sources(conn);

	if (fop == NULL) {
		/* see note [^1] at the end of function */
		rc = -ENOMEM;
		conn_failed(conn, rc);
		m0_rpc_machine_unlock(machine);
		return M0_ERR(rc);
	}
	if (conn_state(conn) == M0_RPC_CONN_TERMINATING) {
		m0_fop_put(fop);
		m0_rpc_machine_unlock(machine);
		return M0_RC(0);
	}
	args = m0_fop_data(fop);
	args->ct_sender_id = conn->c_sender_id;

	if (m0_rpc_conn_is_known_dead(conn)) {
		/*
		 * Unable to terminate normal way while having other side
		 * dead. Therefore, fail itself and quit.
		 */
		m0_fop_put(fop);
		rc = -ECANCELED;
		conn_failed(conn, rc);
		rpc_conn_sessions_cleanup_fail(conn, true);
		m0_rpc_machine_unlock(machine);
		return M0_ERR_INFO(rc, "Connection is known to be dead:"
				   " sender_id=%"PRIu64" svc_fid="FID_F,
				   conn->c_sender_id, FID_P(&conn->c_svc_fid));
	}

	session_0 = m0_rpc_conn_session0(conn);

	/*
	 * m0_rpc_conn_establish_reply_received() expects the session
	 * to be in M0_RPC_CONN_TERMINATING state. Make sure it is so,
	 * even if item send below fails.
	 */
	conn_state_set(conn, M0_RPC_CONN_TERMINATING);
	rc = m0_rpc__fop_post(fop, session_0, &conn_terminate_item_ops,
			      abs_timeout);
	/*
	 * It is possible that ->rio_replied() was called
	 * and connection is terminated already.
	 */
	if (rc != 0 && conn_state(conn) == M0_RPC_CONN_TERMINATING)
		conn_failed(conn, rc);

	m0_fop_put(fop);
	M0_POST(m0_rpc_conn_invariant(conn));
	M0_POST(ergo(rc != 0, conn_state(conn) == M0_RPC_CONN_FAILED));
	/*
	 * CAUTION: Following assertion is not guaranteed as soon as
	 * rpc_machine is unlocked.
	 */
	M0_ASSERT(ergo(rc == 0, conn_state(conn) == M0_RPC_CONN_TERMINATING));

	m0_rpc_machine_unlock(machine);
	/* see m0_rpc_conn_terminate_reply_received() */
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_conn_terminate);
/*
 * m0_rpc_conn_terminate [^1]
 * There are two choices here:
 *
 * 1. leave conn in TERMNATING state FOREVER.
 *    Then when to fini/cleanup conn.
 *
 * 2. Move conn to FAILED state.
 *    For this conn the receiver side state will still
 *    continue to exist. And receiver can send one-way
 *    items, that will be received on sender i.e. current node.
 *    Current code will drop such items. When/how to fini and
 *    cleanup receiver side state? XXX
 *
 * For now, later is chosen. This can be changed in future
 * to alternative 1, iff required.
 */

static void deregister_all_item_sources(struct m0_rpc_conn *conn)
{
	struct m0_rpc_item_source *source;

	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));

	m0_tl_teardown(item_source, &conn->c_item_sources, source) {
		source->ris_conn = NULL;
		source->ris_ops->riso_conn_terminating(source);
	}
}

M0_INTERNAL void m0_rpc_conn_terminate_reply_received(struct m0_rpc_item *item)
{
	struct m0_rpc_fop_conn_terminate_rep *reply = NULL;
	struct m0_rpc_conn                   *conn;
	struct m0_rpc_machine                *machine;
	struct m0_rpc_item                   *reply_item;
	int32_t                               rc;

	M0_ENTRY("item: %p", item);
	M0_PRE(item != NULL &&
	       item->ri_session != NULL &&
	       item->ri_session->s_session_id == SESSION_ID_0);

	conn    = item2conn(item);
	machine = conn->c_rpc_machine;
	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_PRE(m0_rpc_machine_is_locked(machine));
	M0_PRE(conn_state(conn) == M0_RPC_CONN_TERMINATING);

	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		reply_item = item->ri_reply;
		M0_ASSERT(reply_item != NULL &&
			  item->ri_session == reply_item->ri_session);
		reply = m0_fop_data(m0_rpc_item_to_fop(reply_item));
		rc = reply->ctr_rc;
	}
	if (rc == 0) {
		M0_ASSERT(reply != NULL);
		if (conn->c_sender_id == reply->ctr_sender_id)
			conn_state_set(conn, M0_RPC_CONN_TERMINATED);
		else
			rc = M0_ERR(-EPROTO);
	}
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "rpc item ERROR rc=%d sender_id=%"PRIu64
				 " svc_fid="FID_F, rc, conn->c_sender_id,
				 FID_P(&conn->c_svc_fid));
		conn_failed(conn, rc);
	}

	M0_POST(m0_rpc_conn_invariant(conn));
	M0_POST(M0_IN(conn_state(conn), (M0_RPC_CONN_TERMINATED,
					 M0_RPC_CONN_FAILED)));
	M0_POST(m0_rpc_machine_is_locked(machine));
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_conn_cleanup_all_sessions(struct m0_rpc_conn *conn)
{
	rpc_conn_sessions_cleanup_fail(conn, false);
}

/**
 * Connection's session list cleanup omitting session0. When instructed to fail,
 * all finalising sessions are put to failed state. Otherwise are waited for
 * getting to idle state before being finalised.
 *
 * @note Currently, sessions are to fail in case rpc connection is found dead
 * while terminating. See m0_rpc_conn_terminate().
 */
static void rpc_conn_sessions_cleanup_fail(struct m0_rpc_conn *conn, bool fail)
{
	struct m0_rpc_session *session;

	m0_tl_for(rpc_session, &conn->c_sessions, session) {
		if (session->s_session_id == SESSION_ID_0)
			continue;
		M0_LOG(M0_INFO, "Aborting session %llu",
			(unsigned long long)session->s_session_id);
		if (fail) {
			if (!M0_IN(session_state(session),
				   (M0_RPC_SESSION_FAILED,
				    M0_RPC_SESSION_TERMINATED)))
				m0_sm_fail(&session->s_sm,
					   M0_RPC_SESSION_FAILED,
					   -ECANCELED);
			M0_ASSERT(M0_IN(session_state(session),
					(M0_RPC_SESSION_TERMINATED,
					 M0_RPC_SESSION_FAILED)));
		} else { /* normal cleanup */
			m0_sm_timedwait(&session->s_sm,
					M0_BITS(M0_RPC_SESSION_IDLE),
					M0_TIME_NEVER);
			m0_rpc_rcv_session_terminate(session);
		}
		m0_rpc_session_fini_locked(session);
	} m0_tl_endfor;
	M0_POST(rpc_session_tlist_length(&conn->c_sessions) == 1);
}

M0_INTERNAL int m0_rpc_rcv_conn_terminate(struct m0_rpc_conn *conn)
{
	M0_ENTRY("conn: %p", conn);

	M0_ASSERT(m0_rpc_machine_is_locked(conn->c_rpc_machine));
	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_ASSERT(conn_state(conn) == M0_RPC_CONN_ACTIVE);
	M0_ASSERT(m0_rpc_conn_is_rcv(conn));

	if (conn->c_nr_sessions > 1)
		m0_rpc_conn_cleanup_all_sessions(conn);
	deregister_all_item_sources(conn);
	conn_state_set(conn, M0_RPC_CONN_TERMINATED);

	M0_ASSERT(m0_rpc_conn_invariant(conn));
	/* In-core state will be cleaned up by
	   m0_rpc_conn_terminate_reply_sent() */
	return M0_RC(0);
}

M0_INTERNAL void m0_rpc_conn_terminate_reply_sent(struct m0_rpc_conn *conn)
{
	struct m0_rpc_session *session0;

	M0_ENTRY("conn: %p", conn);
	M0_ASSERT(conn != NULL);
	M0_ASSERT(m0_rpc_machine_is_locked(conn->c_rpc_machine));
	M0_ASSERT(m0_rpc_conn_invariant(conn));
	M0_ASSERT(M0_IN(conn_state(conn), (M0_RPC_CONN_TERMINATED,
					   M0_RPC_CONN_FAILED)));

	session0 = m0_rpc_conn_session0(conn);
	if (session0->s_sm.sm_state == M0_RPC_SESSION_IDLE) {
		m0_rpc_conn_fini_locked(conn);
		m0_free(conn);
	}
	M0_LEAVE();
}

M0_INTERNAL bool m0_rpc_item_is_conn_establish(const struct m0_rpc_item *item)
{
	return item->ri_type ==
	       &m0_rpc_fop_conn_establish_fopt.ft_rpc_item_type;
}

M0_INTERNAL bool m0_rpc_item_is_sess_establish(const struct m0_rpc_item *item)
{
	return item->ri_type ==
	       &m0_rpc_fop_session_establish_fopt.ft_rpc_item_type;
}

/**
   Just for debugging purpose. Useful in gdb.

   dir = 1, to print incoming conn list
   dir = 0, to print outgoing conn list
 */
M0_INTERNAL int m0_rpc_machine_conn_list_dump(struct m0_rpc_machine *machine,
					      int dir)
{
	struct m0_tl       *list;
	struct m0_rpc_conn *conn;

	list = dir ? &machine->rm_incoming_conns : &machine->rm_outgoing_conns;

	m0_tl_for(rpc_conn, list, conn) {
		M0_LOG(M0_DEBUG, "rmach %8p conn %8p id %llu state %x dir %s",
				 machine, conn,
				 (unsigned long long)conn->c_sender_id,
				 conn_state(conn),
				 (conn->c_flags & RCF_SENDER_END)? "S":"R");
	} m0_tl_endfor;
	return 0;
}

M0_INTERNAL int m0_rpc_conn_session_list_dump(const struct m0_rpc_conn *conn)
{
	struct m0_rpc_session *session;

	m0_tl_for(rpc_session, &conn->c_sessions, session) {
		M0_LOG(M0_DEBUG, "session %p id %llu state %x", session,
		       (unsigned long long)session->s_session_id,
		       session_state(session));
	} m0_tl_endfor;
	return 0;
}

M0_INTERNAL const char *m0_rpc_conn_addr(const struct m0_rpc_conn *conn)
{
	return conn->c_rpcchan->rc_destep->nep_addr;
}

M0_INTERNAL bool m0_rpc_conn_is_known_dead(const struct m0_rpc_conn *conn)
{
	struct m0_conf_obj *svc_obj;

	M0_PRE(conn != NULL);
	if (!m0_fid_is_set(&conn->c_svc_fid))
		return false;
	svc_obj = m0_conf_cache_lookup(&rpc_conn2confc(conn)->cc_cache,
				       &conn->c_svc_fid);
	return svc_obj != NULL &&
		svc_obj->co_ha_state == M0_NC_FAILED;
}

M0_INTERNAL void m0_rpc_conn_sessions_cancel(struct m0_rpc_conn *conn)
{
	struct m0_rpc_session *session;

	m0_tl_for(rpc_session, &conn->c_sessions, session) {
		if (session->s_session_id == SESSION_ID_0 ||
		    !M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
						    M0_RPC_SESSION_BUSY)))
			continue;
		m0_rpc_session_cancel(session);
	} m0_tl_endfor;
}

/**
 * Callback called on HA notification for conf service object the connection is
 * established to. In case service found dead, all outgoing requests in all
 * sessions associated with the connection are canceled.
 */
static bool rpc_conn__on_service_event_cb(struct m0_clink *clink)
{
	struct m0_rpc_conn *conn = container_of(clink, struct m0_rpc_conn,
						c_ha_clink);
	struct m0_conf_obj *obj  = container_of(clink->cl_chan,
						struct m0_conf_obj, co_ha_chan);

	M0_PRE(m0_rpc_conn2svc(conn) == obj);
	M0_LOG(M0_DEBUG, "obj->co_ha_state = %d", obj->co_ha_state);
	/*
	 * Ignore M0_NC_TRANSIENT state to keep items re-sending until service
	 * gets M0_NC_ONLINE or becomes M0_NC_FAILED finally.
	 */
	if (obj->co_ha_state == M0_NC_FAILED)
		m0_rpc_conn_sessions_cancel(conn);
	/**
	 * @todo See if to __conn_ha_unsubscribe() right now, but not wait until
	 * rpc connection getting finalised.
	 *
	 * @todo See if anything, or what and when otherwise, we need to do on
	 * getting M0_NC_ONLINE notification.
	 */
	return true;
}

static bool rpc_conn__on_cache_ready_cb(struct m0_clink *clink)
{
	struct m0_rpc_conn *conn = container_of(clink, struct m0_rpc_conn,
						c_conf_ready_clink);
	int                 rc;

	if (!m0_fid_is_set(&conn->c_svc_fid))
		return true;
	M0_LOG(M0_DEBUG, "subscribe %p conn to HA, svc_fid "FID_F, conn,
	       FID_P(&conn->c_svc_fid));
	rc = m0_rpc_conn_ha_subscribe(conn, &conn->c_svc_fid);
	if (rc != 0)
		M0_LOG(M0_WARN, "Conn %p failed to subscribe, rc=%d", conn, rc);
	M0_POST(M0_IN(rc, (0, -ENOENT)));
	/**
	 * @todo See if we can act any smarter than just log the subscription
	 * error. Please note, -ENOENT code is normal in the situation when the
	 * connection previously was established to a service that appears
	 * abandoned when conf updates.
	 */
	return true;
}

static bool rpc_conn__on_cache_expired_cb(struct m0_clink *clink)
{
	struct m0_rpc_conn *conn = container_of(clink, struct m0_rpc_conn,
						c_conf_exp_clink);
	struct m0_conf_cache *cc = &rpc_conn2confc(conn)->cc_cache;

	M0_LOG(M0_DEBUG, "unsubscribe %p conn from HA", conn);
	/*
	 * Lock ordering -
	 * 1. Conf cache lock
	 * 2. RPC machine lock
	 */
	m0_conf_cache_lock(cc);
	m0_rpc_machine_lock(conn->c_rpc_machine);
	__conn_ha_unsubscribe(conn);
	m0_rpc_machine_unlock(conn->c_rpc_machine);
	m0_conf_cache_unlock(cc);
	return true;
}

M0_INTERNAL int m0_rpc_conn_ha_timer_start(struct m0_rpc_conn *conn)
{
	M0_ENTRY("conn %p", conn);
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));
	if (!m0_fid_is_set(&conn->c_svc_fid))
		return M0_RC(0); /* there's no point to arm the timer */
	if (m0_sm_timer_is_armed(&conn->c_ha_timer))
		return M0_RC(0); /* Already started */
	else
		m0_sm_timer_fini(&conn->c_ha_timer);
	if (conn->c_rpc_machine->rm_stopping)
		return M0_RC(0);
	m0_sm_timer_init(&conn->c_ha_timer);
	return M0_RC(m0_sm_timer_start(&conn->c_ha_timer,
				       &conn->c_rpc_machine->rm_sm_grp,
				       conn->c_ha_cfg->rchc_ops.cho_ha_timer_cb,
				       m0_time_add(m0_time_now(),
					      conn->c_ha_cfg->rchc_ha_interval))
		);
}

M0_INTERNAL void m0_rpc_conn_ha_timer_stop(struct m0_rpc_conn *conn)
{
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));
	if (m0_sm_timer_is_armed(&conn->c_ha_timer)) {
		M0_LOG(M0_DEBUG, "Cancelling HA timer; rpc conn=%p", conn);
		m0_sm_timer_cancel(&conn->c_ha_timer);
	}
}

/**
 * HA needs to be notified in case rpc item is not replied within the timer's
 * interval. This is to indicate that peered service may experience issues with
 * networking, and thus the service status has to be considered M0_NC_TRANSIENT
 * until reply comes back to sender.
 *
 * @note The update is to be sent on every timer triggering, i.e. on every
 * re-send of the item.
 *
 * See item__on_reply_postprocess() for complimentary part of the item's
 * processing.
 */
static void rpc_conn_ha_timer_cb(struct m0_sm_timer *timer)
{
	struct m0_rpc_conn *conn;
	struct m0_conf_obj *svc_obj;

	M0_ENTRY();
	M0_PRE(timer != NULL);

	conn = container_of(timer, struct m0_rpc_conn, c_ha_timer);
	conn->c_rpc_machine->rm_stats.rs_nr_ha_timedout_items++;
	M0_ASSERT(conn->c_magic == M0_RPC_CONN_MAGIC);
	M0_ASSERT(m0_rpc_machine_is_locked(conn->c_rpc_machine));
	svc_obj = m0_rpc_conn2svc(conn);
	if (svc_obj != NULL && !conn_flag_is_set(conn, RCF_TRANSIENT_SENT) &&
	    svc_obj->co_ha_state == M0_NC_ONLINE) {
		conn->c_ha_cfg->rchc_ops.cho_ha_notify(conn, M0_NC_TRANSIENT);
		conn_flag_set(conn, RCF_TRANSIENT_SENT);
	}
	M0_LEAVE();
}

static void reqh_service_ha_state_set(struct m0_rpc_conn *conn, uint8_t state)
{
	struct m0_ha_msg *msg;
	uint64_t          tag;

	M0_ENTRY("conn %p, svc_fid "FID_F", state %s", conn,
		 FID_P(&conn->c_svc_fid), m0_ha_state2str(state));

	M0_PRE(m0_fid_is_set(&conn->c_svc_fid));
	M0_PRE(m0_conf_fid_type(&conn->c_svc_fid) == &M0_CONF_SERVICE_TYPE);
	M0_PRE(M0_IN(state, (M0_NC_TRANSIENT, M0_NC_ONLINE)));
	M0_PRE(m0_rpc_machine_is_locked(conn->c_rpc_machine));

	conn->c_ha_attempts++;
	if (conn_flag_is_set(conn, RCF_TRANSIENT_SENT)) {
		M0_LOG(M0_DEBUG, "Already reported about TRANSIENT");
		goto leave;
	}

	M0_ALLOC_PTR(msg);
	if (msg == NULL) {
		M0_LOG(M0_ERROR, "can't allocate memory for msg");
		goto leave;
	}
	*msg = (struct m0_ha_msg){
		.hm_fid  = conn->c_svc_fid,
		.hm_time = m0_time_now(),
		.hm_data = {
			.hed_type        = M0_HA_MSG_EVENT_RPC,
			.u.hed_event_rpc = {
				.hmr_state = state,
				.hmr_attempts = conn->c_ha_attempts
			},
		},
	};
	m0_ha_send(m0_get()->i_ha, m0_get()->i_ha_link, msg, &tag);
	m0_free(msg);
	conn->c_rpc_machine->rm_stats.rs_nr_ha_noted_conns++;
	M0_LOG(M0_DEBUG, "tag=%"PRIu64, tag);
leave:
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_conn_ha_cfg_set(struct m0_rpc_conn              *conn,
					const struct m0_rpc_conn_ha_cfg *cfg)
{
	m0_rpc_machine_lock(conn->c_rpc_machine);
	conn->c_ha_cfg = cfg;
	m0_rpc_machine_unlock(conn->c_rpc_machine);
}

#define S_CASE(x) case x: return #x;
M0_INTERNAL const char *m0_rpc_conn_state_to_str(enum m0_rpc_conn_state state)
{
	switch (state) {
		S_CASE(M0_RPC_CONN_INITIALISED)
		S_CASE(M0_RPC_CONN_CONNECTING)
		S_CASE(M0_RPC_CONN_ACTIVE)
		S_CASE(M0_RPC_CONN_FAILED)
		S_CASE(M0_RPC_CONN_TERMINATING)
		S_CASE(M0_RPC_CONN_TERMINATED)
		S_CASE(M0_RPC_CONN_FINALISED)
	}
	M0_LOG(M0_ERROR, "State %d unknown", state);
	M0_ASSERT(NULL == "No transcript");
	return NULL;
}
#undef S_CASE

#undef M0_TRACE_SUBSYSTEM
/** @} end of rpc group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
