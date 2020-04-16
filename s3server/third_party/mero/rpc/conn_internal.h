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
 * Original creation date: 10/31/2012
 */

#pragma once

#ifndef __MERO_RPC_CONN_INT_H__
#define __MERO_RPC_CONN_INT_H__

#include "rpc/conn.h"

/* Imports */
struct m0_rpc_session;
struct m0_rpc_item;
struct m0_rpc_item_source;

/**
   @addtogroup rpc_session

   @{
 */

enum {
	SENDER_ID_INVALID = UINT64_MAX,
};

M0_INTERNAL bool m0_rpc_conn_invariant(const struct m0_rpc_conn *conn);
M0_INTERNAL int m0_rpc_conn_ha_timer_start(struct m0_rpc_conn *conn);
M0_INTERNAL void m0_rpc_conn_ha_timer_stop(struct m0_rpc_conn *conn);

struct m0_rpc_conn_ha_ops {
	/**
	 * Conn HA timeout callback intended for reporting transient state to HA
	 * in case HA subscription exists.
	 */
	void (*cho_ha_timer_cb)(struct m0_sm_timer *timer);
	/**
	 * HA notification procedure.
	 */
	void (*cho_ha_notify)(struct m0_rpc_conn *conn, uint8_t state);
};

struct m0_rpc_conn_ha_cfg {
	struct m0_rpc_conn_ha_ops rchc_ops;
	m0_time_t                 rchc_ha_interval;
};

static inline int conn_state(const struct m0_rpc_conn *conn)
{
	return conn->c_sm.sm_state;
}

static inline void conn_flag_set(struct m0_rpc_conn *conn, uint64_t flag)
{
	conn->c_flags |= flag;
}

static inline void conn_flag_unset(struct m0_rpc_conn *conn, uint64_t flag)
{
	conn->c_flags &= ~flag;
}

static inline bool conn_flag_is_set(const struct m0_rpc_conn *conn,
				    uint64_t                  flag)
{
	return conn->c_flags & flag;
}

M0_INTERNAL void conn_state_set(struct m0_rpc_conn *conn, int state);

/**
   Searches in conn->c_sessions list, a session object whose session id
   matches with given @session_id.

   Caller is expected to decide whether conn will be locked or not
   The function is also called from session_foms.c, that's why is not static.

   @return pointer to session if found, NULL otherwise
   @post ergo(result != NULL, result->s_session_id == session_id)
 */
M0_INTERNAL struct m0_rpc_session *m0_rpc_session_search(const struct
							 m0_rpc_conn *conn,
							 uint64_t session_id);

/**
   Searches in conn->c_sessions list, a session object whose session id
   matches with given @session_id and pops it from this list.
   @see m0_rpc_session_search for more details
 */
M0_INTERNAL struct m0_rpc_session *m0_rpc_session_search_and_pop(
	const struct m0_rpc_conn *conn, uint64_t session_id);

/**
   Pops first valid session from conn->c_sessions list
   @see m0_rpc_session_search for more details
 */
M0_INTERNAL struct m0_rpc_session *m0_rpc_session_pop(
	const struct m0_rpc_conn *conn);

/**
   Searches and returns session with session_id 0.
   Each rpc connection always has exactly one instance of session with
   SESSION_ID_0 in its c_sessions list.

   @post result != NULL && result->s_session_id == SESSION_ID_0
 */
M0_INTERNAL struct m0_rpc_session *m0_rpc_conn_session0(const struct m0_rpc_conn
							*conn);

M0_INTERNAL void m0_rpc_conn_fini_locked(struct m0_rpc_conn *conn);

/**
   Initalises receiver end of conn object.

   @post ergo(result == 0, conn_state(conn) == M0_RPC_CONN_INITIALISED &&
			   conn->c_rpc_machine == machine &&
			   conn->c_sender_id == SENDER_ID_INVALID &&
			   (conn->c_flags & RCF_RECV_END) != 0)
 */
M0_INTERNAL int m0_rpc_rcv_conn_init(struct m0_rpc_conn *conn,
				     struct m0_net_end_point *ep,
				     struct m0_rpc_machine *machine,
				     const struct m0_uint128 *uuid);
/**
   Terminates receiver end of rpc connection.

   Terminates alive sessions if any.

   @pre conn_state(conn) == M0_RPC_CONN_ACTIVE
   @post ergo(result == 0, conn_state(conn) == M0_RPC_CONN_TERMINATED)
 */
M0_INTERNAL int m0_rpc_rcv_conn_terminate(struct m0_rpc_conn *conn);

/**
   Callback routine called through item->ri_ops->rio_replied().

   The routine is executed when reply to conn create fop is received
 */
M0_INTERNAL void m0_rpc_conn_establish_reply_received(struct m0_rpc_item *req);

/**
   Cleans up in memory state of rpc connection.

   The conn_terminate FOM cannot free in-memory state of rpc connection.
   Because it needs to send conn_terminate_reply fop, by using session-0
   of the rpc connection being terminated. Hence we cleanup in memory
   state of the conn when conn_terminate_reply has been sent.

   @pre conn_state(conn) == M0_RPC_CONN_TERMINATING
 */
M0_INTERNAL void m0_rpc_conn_terminate_reply_sent(struct m0_rpc_conn *conn);

/**
   Callback routine called through item->ri_ops->rio_replied().

   The routine is executed when reply to conn terminate fop is received
 */
M0_INTERNAL void m0_rpc_conn_terminate_reply_received(struct m0_rpc_item *req);

/**
   Returns true iff given rpc item is conn_establish.
 */
M0_INTERNAL bool m0_rpc_item_is_conn_establish(const struct m0_rpc_item *item);

/**
   Returns true if given rpc item is session_establish.
 */
M0_INTERNAL bool m0_rpc_item_is_sess_establish(const struct m0_rpc_item *item);

/**
   @see m0_rpc_fop_conn_establish_ctx for more information.
 */
M0_INTERNAL void m0_rpc_fop_conn_establish_ctx_init(struct m0_rpc_item *item,
						  struct m0_net_end_point *ep);

/**
   Return true iff @conn is sender side object of rpc-connection.
 */
M0_INTERNAL bool m0_rpc_conn_is_snd(const struct m0_rpc_conn *conn);

/**
   Return true iff @conn is receiver side object of rpc-connection.
 */
M0_INTERNAL bool m0_rpc_conn_is_rcv(const struct m0_rpc_conn *conn);

M0_INTERNAL void m0_rpc_conn_add_session(struct m0_rpc_conn *conn,
					 struct m0_rpc_session *session);
M0_INTERNAL void m0_rpc_conn_remove_session(struct m0_rpc_session *session);

M0_INTERNAL void m0_rpc_conn_cleanup_all_sessions(struct m0_rpc_conn *conn);

/** @}  End of rpc_session group */
#endif /* __MERO_RPC_CONN_INT_H__ */
