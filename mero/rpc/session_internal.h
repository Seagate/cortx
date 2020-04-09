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
 * Original author: Amit_Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 05/02/2011
 */

#pragma once

#ifndef __MERO_RPC_SESSION_INT_H__
#define __MERO_RPC_SESSION_INT_H__

#include "rpc/session.h"

/**
   @addtogroup rpc_session

   @{
 */

/* Imports */
struct m0_rpc_item;

enum {
	/** [conn|session]_[create|terminate] items go on session 0 */
	SESSION_ID_0             = 0,
	SESSION_ID_INVALID       = UINT64_MAX,
	/** Range of valid session ids */
	SESSION_ID_MIN           = SESSION_ID_0 + 1,
	SESSION_ID_MAX           = SESSION_ID_INVALID - 1,
};

/**
   checks internal consistency of session
 */
M0_INTERNAL bool m0_rpc_session_invariant(const struct m0_rpc_session *session);

/**
   Holds a session in BUSY state.
   Every call to m0_rpc_session_hold_busy() must accompany
   call to m0_rpc_session_release()

   @pre M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
				       M0_RPC_SESSION_BUSY))
   @pre m0_rpc_machine_is_locked(session_machine(session))
   @post session_state(session) == M0_RPC_SESSION_BUSY
 */
M0_INTERNAL void m0_rpc_session_hold_busy(struct m0_rpc_session *session);

/**
   Decrements hold count. Moves session to IDLE state if it becomes idle.

   @pre session_state(session) == M0_RPC_SESSION_BUSY
   @pre session->s_hold_cnt > 0
   @pre m0_rpc_machine_is_locked(session_machine(session))
   @post ergo(m0_rpc_session_is_idle(session),
	      session_state(session) == M0_RPC_SESSION_IDLE)
 */
M0_INTERNAL void m0_rpc_session_release(struct m0_rpc_session *session);

M0_INTERNAL void session_state_set(struct m0_rpc_session *session, int state);
M0_INTERNAL int session_state(const struct m0_rpc_session *session);

M0_INTERNAL int m0_rpc_session_init_locked(struct m0_rpc_session *session,
					   struct m0_rpc_conn *conn);
M0_INTERNAL void m0_rpc_session_fini_locked(struct m0_rpc_session *session);

/**
   Terminates receiver end of session.

   @pre session->s_state == M0_RPC_SESSION_IDLE
   @post ergo(result == 0, session->s_state == M0_RPC_SESSION_TERMINATED)
   @post ergo(result != 0 && session->s_rc != 0, session->s_state ==
	      M0_RPC_SESSION_FAILED)
 */
M0_INTERNAL int m0_rpc_rcv_session_terminate(struct m0_rpc_session *session);

/**
   Callback routine called through item->ri_ops->rio_replied().

   The routine is executed when reply to session create fop is received
 */
M0_INTERNAL void m0_rpc_session_establish_reply_received(struct m0_rpc_item
							 *req);

/**
   Callback routine called through item->ri_ops->rio_replied().

   The routine is executed when reply to session terminate fop is received
 */
M0_INTERNAL void m0_rpc_session_terminate_reply_received(struct m0_rpc_item
							 *req);

M0_INTERNAL bool m0_rpc_session_is_idle(const struct m0_rpc_session *session);

M0_INTERNAL void m0_rpc_session_item_failed(struct m0_rpc_item *item);

M0_INTERNAL struct m0_rpc_machine *session_machine(const struct m0_rpc_session
						   *s);

M0_TL_DESCR_DECLARE(rpc_session, M0_EXTERN);
M0_TL_DECLARE(rpc_session, M0_INTERNAL, struct m0_rpc_session);

/** @}  End of rpc_session group */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
