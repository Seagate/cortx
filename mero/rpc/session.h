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
 * Original author: Alexey Lyashkov <Alexey_Lyashkov@xyratex.com>
 *                  Rohan Puri <Rohan_Puri@xyratex.com>
 *                  Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 04/09/2010
 */

#pragma once

#ifndef __MERO_RPC_SESSION_H__
#define __MERO_RPC_SESSION_H__

#include "rpc/item.h"

/**

@defgroup rpc_session RPC Sessions

@{

@section Overview

Session module of rpc layer has two objectives:

- If some rpc-items need to be delivered to receiver in same sequence in
  which sender has submitted them, then it is the session module which
  ensures that the items are delivered in FIFO sequence.

- To provide exactly once semantics.
  see http://www.cs.unc.edu/~dewan/242/s06/notes/ipc/node30.html

Aproach taken by session module to achive these two objectives, is similar
to session-slot implementation in NFSv4.1

See section 2.10.6 of rfc 5661 NFSv4.1
http://tools.ietf.org/html/rfc5661#section-2.10.6

Session module defines following types of objects:
- rpc connection @see m0_rpc_conn
- rpc session @see m0_rpc_session

Out of these, m0_rpc_conn and m0_rpc_session are visible to user.

Session module uses following types of objects:
- rpc machine @see m0_rpc_machine
- rpc item @see m0_rpc_item.

<B> Relationships among objects: </B>
rpc_machine has two lists of rpc connections.
- Outgoing connections: Contains m0_rpc_conn objects for which this node is
sender.
- Incoming connections: Contains m0_rpc_conn objects for which this node is
receiver.

Rpc connection has a list of rpc sessions, which are created on this
connection. A rpc connection cannot be terminated until all the sessions
created on the connection are terminated.

Each object of type [m0_rpc_conn|m0_rpc_session] on sender
has counterpart object of same type on receiver. (Note: same structure
definitions are used on both sender and receiver side)

 <B> Using two identifiers for session and conn </B>
 @todo
 currently, receiver assigns identifiers to connections and sessions and
 these identifiers are used by both parties. What we can do, is to allow
 sender to assign identifiers to sessions (this identifier is sent in
 SESSION_ESTABLISH). Then, whenever receiver uses the session to send a
 reply, it uses this identifier (instead of receiver assigned session-id).
 The advantage of this, is that sender can use an identifier that allows
 quick lookup (e.g., an index in some session array or simply a pointer).
 Similarly for connections (i.e., another sender generated identifier in
 addition to uuid, that is not guaranteed to be globally unique)

    @todo
	- Generate ADDB data points for important session events
	- store replies in FOL
	- Optimization: Cache misordered items at receiver, rather than
	  discarding them.
 */

#include "lib/list.h"
#include "lib/tlist.h"
#include "lib/time.h"
#include "sm/sm.h"                /* m0_sm */
#include "rpc/onwire.h"

/* Imports */
struct m0_rpc_conn;

/* Exports */
struct m0_rpc_session;

/**
   Possible states of a session object
 */
enum m0_rpc_session_state {
	/**
	   all lists, mutex and channels of session are initialised.
	   No actual session is established with any end point
	 */
	M0_RPC_SESSION_INITIALISED,
	/**
	   When sender sends a SESSION_ESTABLISH FOP to reciever it
	   is in ESTABLISHING state
	 */
	M0_RPC_SESSION_ESTABLISHING,
	/**
	   A session can be terminated only if it is IDLE.
	 */
	M0_RPC_SESSION_IDLE,
	/**
	   A session is busy if any of following is true
		- There is item for which the reply is not received yet
			(on receive side).
		- Formation queue has item associated with this session.
	 */
	M0_RPC_SESSION_BUSY,
	/**
	   Creation/termination of session failed
	 */
	M0_RPC_SESSION_FAILED,
	/**
	   When sender sends SESSION_TERMINATE fop to receiver and is waiting
	   for reply, then it is in state TERMINATING.
	*/
	M0_RPC_SESSION_TERMINATING,
	/**
	   When sender gets reply to session_terminate fop and reply informs
	   the session termination is successful then the session enters in
	   TERMINATED state
	 */
	M0_RPC_SESSION_TERMINATED,
	/** After m0_rpc_session_fini() the RPC session instance is moved to
	    FINALISED state.
	 */
	M0_RPC_SESSION_FINALISED
};

/** Transforms @ref m0_rpc_session_state value to string */
M0_INTERNAL const char *
m0_rpc_session_state_to_str(enum m0_rpc_session_state state);

/**
   Rpc connection can be shared by multiple entities (e.g. users) by
   creating their own "session" on the connection.
   A session can be used to maintain authentication information or QoS
   parameters.

   <B> Liveness: </B>

   On sender side, allocation and deallocation of m0_rpc_session is entirely
   managed by user except for SESSION 0. SESSION 0 is allocated and deallocated
   by rpc-layer internally along with m0_rpc_conn.
   @see m0_rpc_conn for more information on creation and use of SESSION 0.

   On receiver side, m0_rpc_session object will be allocated and deallocated
   by rpc-layer internally, in response to session create and session terminate
   requests respectively.

   <B> Concurrency:</B>

   Users of rpc-layer are never expected to take lock on session. Rpc layer
   will internally synchronise access to m0_rpc_session.

   All access to session are synchronized using
   session->s_conn->c_rpc_machine->rm_sm_grp.s_lock.

   When session is in one of INITIALISED, TERMINATED, FINALISED and
   FAILED state, user is expected to serialise access to the session object.
   (It is assumed that session, in one of {INITIALISED, TERMINATED, FAILED,
    FINALISED} states, does not have concurrent users).

   @verbatim
                                      |
                                      |m0_rpc_session_init()
  m0_rpc_session_establish() != 0     V
          +----------------------INITIALISED
          |                           |
          |                           | m0_rpc_session_establish()
          |                           |
          |     timed-out             V
          +-----------------------ESTABLISHING
	  |   create_failed           | create successful/n = 0
	  V                           |
	FAILED <------+               |   n == 0
	  |           |               +-----------------+
	  |           |               |                 | +-----+
	  |           |failed         |                 | |     | item add/n++
	  |           |               V  item add/n++   | V     | reply rcvd/n--
	  |           +-------------IDLE--------------->BUSY----+
	  |           |               |
	  | fini()    |               | m0_rpc_session_terminate()
	  |           |               V
	  |           +----------TERMINATING
	  |                           |
	  |                           |
	  |                           |
	  |                           |session_terminated
	  |                           V
	  |                       TERMINATED
	  |                           |
	  |                           | fini()
	  |                           V
	  +----------------------> FINALISED

   @endverbatim

   Typical sequence of execution of APIs on sender side. Error checking is
   omitted.

   @code

   // ALLOCATE SESSION

   struct m0_rpc_session *session;
   M0_ALLOC_PTR(session);

   // INITIALISE SESSION

   rc = m0_rpc_session_init(session, conn);
   M0_ASSERT(ergo(rc == 0, session_state(session) ==
                           M0_RPC_SESSION_INITIALISED));

   // ESTABLISH SESSION

   rc = m0_rpc_session_establish(session);

   rc = m0_rpc_session_timedwait(session, M0_BITS(M0_RPC_SESSION_IDLE,
					          M0_RPC_SESSION_FAILED),
				 timeout);

   if (rc == 0 && session_state(session) == M0_RPC_SESSION_IDLE) {
	// Session is successfully established
   } else {
	// timeout has happened or session establish failed
   }

   // Assuming session is successfully established.
   // post unbound items using m0_rpc_post(item)

   item->ri_session = session;
   item->ri_prio = M0_RPC_ITEM_PRIO_MAX;
   item->ri_deadline = absolute_time;
   item->ri_ops = item_ops;   // item_ops contains ->replied() callback which
			      // will be called when reply to this item is
			      // received. DO NOT FREE THIS ITEM.

   rc = m0_rpc_post(item);

   // TERMINATING SESSION
   // Wait until all the items that were posted on this session, are sent and
   // for all those items either reply is received or reply_timeout has
   // triggered.
   m0_rpc_session_timedwait(session, M0_BITS(M0_RPC_SESSION_IDLE), timeout);
   M0_ASSERT(session_state == M0_RPC_SESSION_IDLE);
   rc = m0_rpc_session_terminate(session);
   if (rc == 0) {
	// Wait until session is terminated.
	rc = m0_rpc_session_timedwait(session,
				      M0_BITS(M0_RPC_SESSION_TERMINATED,
					      M0_RPC_SESSION_FAILED),
				      timeout);
   }

   // FINALISE SESSION

   m0_rpc_session_fini(session);
   m0_free(session);
   @endcode

   Receiver is not expected to call any of these APIs. Receiver side session
   structures will be set-up while handling fops
   m0_rpc_fop_[conn|session]_[establish|terminate].

   When receiver needs to post reply, it uses m0_rpc_reply_post().

   @code
   m0_rpc_reply_post(request_item, reply_item);
   @endcode

   m0_rpc_reply_post() will copy all the session related information from
   request item to reply item and process reply item.

   Note: rpc connection is a two-way communication channel. There are requests
   and corresponding reply items, on the same connection. Receiver NEED NOT
   have to establish other separate connection with sender, to be able to
   send replies.
 */
struct m0_rpc_session {
	/** identifies a particular session. Unique in all sessions belonging
	    to same m0_rpc_conn
	 */
	uint64_t                  s_session_id;

	/** rpc connection on which this session is created */
	struct m0_rpc_conn       *s_conn;

	/** Link in RPC conn. m0_rpc_conn::c_sessions
	    List descriptor: session
	 */
	struct m0_tlink           s_link;

	/** if > 0, then session is in BUSY state */
	uint32_t                  s_hold_cnt;

	/** RPC session state machine
	    @see m0_rpc_session_state, session_conf
	 */
	struct m0_sm              s_sm;

	/** M0_RPC_SESSION_MAGIC */
	uint64_t                  s_magic;

	/** Unique item identifier counter */
	uint64_t                  s_xid;

	/**
	 * Replies to resend if needed.
	 * This cache is protected with rpc machine lock.
	 */
	struct m0_rpc_item_cache  s_reply_cache;

	/**
	 * Requests which are handled already.
	 * This cache is protected with rpc machine lock.
	 */
	struct m0_rpc_item_cache  s_req_cache;

	/**
	 * Flag to indicate if this session has been cancelled.
	 * This flag is set to TRUE at the beginning of m0_rpc_session_cancel()
	 * execution.
	 * Once this flag is set to TRUE, subsequent m0_rpc_post() against
	 * the same session returns -ECANCELED error.
	 */
	bool                      s_cancelled;

	/**
	 * Items submitted to formation.
	 * Required in case RPC session is to be cancelled so as to cancel
	 * all such items.
	 * This cache is protected with rpc machine lock.
	 */
	struct m0_tl              s_pending_cache;
};

/**
   Initialises all fields of session.
   No network communication is involved.

   @param session session being initialised
   @param conn rpc connection with which this session is associated

   @post ergo(rc == 0, session_state(session) == M0_RPC_SESSION_INITIALISED &&
		       session->s_conn == conn &&
		       session->s_session_id == SESSION_ID_INVALID)
 */
M0_INTERNAL int m0_rpc_session_init(struct m0_rpc_session *session,
				    struct m0_rpc_conn *conn);

M0_INTERNAL void m0_rpc_session_reset(struct m0_rpc_session *session);

/**
    Sends a SESSION_ESTABLISH fop across pre-defined session-0 in
    session->s_conn. Use m0_rpc_session_timedwait() to wait
    until session reaches IDLE or FAILED state.

    @pre session_state(session) == M0_RPC_SESSION_INITIALISED
    @pre conn_state(session->s_conn) == M0_RPC_CONN_ACTIVE
    @post ergo(result != 0, session_state(session) == M0_RPC_SESSION_FAILED)
 */
M0_INTERNAL int m0_rpc_session_establish(struct m0_rpc_session *session,
					 m0_time_t abs_timeout);

/**
 * Same as m0_rpc_session_establish(), but in addition uses
 * m0_rpc_session_timedwait() to ensure that session is in idle state after
 * m0_rpc_session_establish() call.
 *
 * @param session     A session object to operate on.
 * @param abs_timeout Absolute time after which session establish operation
 *                    is aborted and session is moved to FAILED state.
 *
 * @pre  session_state(session) == M0_RPC_SESSION_INITIALISED
 * @pre  conn_state(session->s_conn) == M0_RPC_CONN_ACTIVE
 * @post session_state(session) == M0_RPC_SESSION_IDLE
 */
M0_INTERNAL int m0_rpc_session_establish_sync(struct m0_rpc_session *session,
					      m0_time_t abs_timeout);

/**
 * A combination of m0_rpc_session_init() and m0_rpc_session_establish_sync() in
 * a single routine - initialize session object, establish a session and wait
 * until it become idle.
 */
M0_INTERNAL int m0_rpc_session_create(struct m0_rpc_session *session,
				      struct m0_rpc_conn *conn,
				      m0_time_t abs_timeout);

/**
   Sends terminate session fop to receiver.
   Acts as no-op if session is already in TERMINATING state.
   Does not wait for reply. Use m0_rpc_session_timedwait() to wait
   until session reaches TERMINATED or FAILED state.

   @pre M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
				       M0_RPC_SESSION_TERMINATING))
   @post ergo(rc != 0, session_state(session) == M0_RPC_SESSION_FAILED)
 */
M0_INTERNAL int m0_rpc_session_terminate(struct m0_rpc_session *session,
					 m0_time_t abs_timeout);

/**
 * Same as m0_rpc_session_terminate(), but in addition uses
 * m0_rpc_session_timedwait() to ensure that session is in terminated state
 * after m0_rpc_session_terminate() call.
 *
 * @param session     A session object to operate on.
 * @param abs_timeout Absolute time after which session terminate operation
 *                    is considered as failed and session is moved to
 *                    FAILED state.
 *
 * @pre M0_IN(session_state(session), (M0_RPC_SESSION_IDLE,
 *				       M0_RPC_SESSION_BUSY,
 *				       M0_RPC_SESSION_TERMINATING))
 * @post M0_IN(session_state(session), (M0_RPC_SESSION_TERMINATED,
 *					M0_RPC_SESSION_FAILED))
 */
M0_INTERNAL int m0_rpc_session_terminate_sync(struct m0_rpc_session *session,
					      m0_time_t abs_timeout);

/**
    Waits until @session object reaches in one of states given by @state_flags.

    @param states can specify multiple states by using M0_BITS()
    @param abs_timeout thread does not sleep past abs_timeout waiting for conn
		to reach in desired state.
    @return 0 if session reaches in one of the state(s) specified by
		@state_flags
            -ETIMEDOUT if time out has occured before session reaches in
                desired state.
 */
M0_INTERNAL int m0_rpc_session_timedwait(struct m0_rpc_session *session,
					 uint64_t states,
					 const m0_time_t abs_timeout);

/**
 * Validates if session allows posting items. Validation includes testing
 * session state as well as connection and rpc machine pointers being not NULL.
 */
M0_INTERNAL int m0_rpc_session_validate(struct m0_rpc_session *session);

/**
   Finalises session object

   @pre M0_IN(session_state(session), (M0_RPC_SESSION_TERMINATED,
				       M0_RPC_SESSION_FAILED,
				       M0_RPC_SESSION_INITIALISED))
 */
M0_INTERNAL void m0_rpc_session_fini(struct m0_rpc_session *session);

/**
 * A combination of m0_rpc_session_terminate_sync() and m0_rpc_session_fini()
 * in a single routine - terminate the session, wait until it switched to
 * terminated state and finalize session object.
 */
int m0_rpc_session_destroy(struct m0_rpc_session *session,
			   m0_time_t abs_timeout);

/**
   Iterates over all the items 'submitted to RPC and which are yet to receive
   reply' and invokes m0_rpc_item_cancel() for each of those.
 */
M0_INTERNAL void m0_rpc_session_cancel(struct m0_rpc_session *session);

/**
   Checks if a session is marked as cancelled.
 */
M0_INTERNAL bool m0_rpc_session_is_cancelled(struct m0_rpc_session *session);

/**
   Does forced session state transition: ESTABLISHING --> IDLE
   Intended to prepare non-established session for termination.
 */
M0_INTERNAL void m0_rpc_session_quiesce(struct m0_rpc_session *session);

/**
   Returns maximum size of an RPC item allowed on this session.
 */
M0_INTERNAL m0_bcount_t
m0_rpc_session_get_max_item_size(const struct m0_rpc_session *session);

/** Returns maximum possible size of RPC item payload. */
M0_INTERNAL m0_bcount_t
m0_rpc_session_get_max_item_payload_size(const struct m0_rpc_session *session);

M0_INTERNAL struct m0_rpc_machine *
session_machine(const struct m0_rpc_session *s);

M0_TL_DESCR_DECLARE(pending_item, M0_EXTERN);
M0_TL_DECLARE(pending_item, M0_INTERNAL, struct m0_rpc_item);

/** @} end of session group */

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
