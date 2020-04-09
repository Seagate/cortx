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

#pragma once

#ifndef __MERO_RPC_SESSION_FOPS_H__
#define __MERO_RPC_SESSION_FOPS_H__

#include "fop/fop.h"
#include "rpc/rpc_opcodes.h"
#include "lib/types.h"
#include "lib/protocol.h"
#include "lib/protocol_xc.h"
#include "xcode/xcode_attr.h"

/**
   @addtogroup rpc_session

   @{

   Declarations of all the fops belonging to rpc-session module along with
   associated item types.
 */

enum m0_rpc_conn_sess_terminate_phases {
	M0_RPC_CONN_SESS_TERMINATE_INIT = M0_FOM_PHASE_INIT,
	M0_RPC_CONN_SESS_TERMINATE_DONE = M0_FOM_PHASE_FINISH,
	M0_RPC_CONN_SESS_TERMINATE_WAIT,
};

extern const struct m0_fop_type_ops m0_rpc_fop_conn_establish_ops;
extern const struct m0_fop_type_ops m0_rpc_fop_conn_terminate_ops;
extern const struct m0_fop_type_ops m0_rpc_fop_session_establish_ops;
extern const struct m0_fop_type_ops m0_rpc_fop_session_terminate_ops;

extern struct m0_fop_type m0_rpc_fop_conn_establish_fopt;
extern struct m0_fop_type m0_rpc_fop_conn_establish_rep_fopt;
extern struct m0_fop_type m0_rpc_fop_conn_terminate_fopt;
extern struct m0_fop_type m0_rpc_fop_conn_terminate_rep_fopt;
extern struct m0_fop_type m0_rpc_fop_session_establish_fopt;
extern struct m0_fop_type m0_rpc_fop_session_establish_rep_fopt;
extern struct m0_fop_type m0_rpc_fop_session_terminate_fopt;
extern struct m0_fop_type m0_rpc_fop_session_terminate_rep_fopt;

M0_INTERNAL int m0_rpc_session_fop_init(void);

M0_INTERNAL void m0_rpc_session_fop_fini(void);

/**
   Container for CONN_ESTABLISH fop.

   This is required only on receiver side so that,
   m0_rpc_fom_conn_establish_state() can find out sender's endpoint, while
   initialising receiver side m0_rpc_conn object.

   item_received() calls m0_rpc_fop_conn_establish_ctx_init() to
   initialise cec_fop and cec_sender_ep.
 */
struct m0_rpc_fop_conn_establish_ctx {
	/** fop instance of type m0_rpc_fop_conn_establish_fopt */
	struct m0_fop            cec_fop;

	/** end point of sender, who has sent the conn_establish request fop */
	struct m0_net_end_point *cec_sender_ep;
};

struct m0_rpc_fop_conn_establish {
	/**
	 * Protocol version string checked during rpc connection
	 * establinshing procedure
	 */
	struct m0_protocol_id rce_protocol;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
   FOP sent by receiver back to sender as a reply to m0_rpc_fop_conn_establish
   FOP.
 */
struct m0_rpc_fop_conn_establish_rep {
	/**
	   Contains 0 if CONN_ESTABLISH operation is successful, error code
	   otherwise.
	 */
	int32_t  rcer_rc;
	/**
	   sender_id assigned by receiver to the established rpc-connection.
	   Has value SENDER_ID_INVALID if CONN_ESTABLISH operation fails.
	 */
	uint64_t rcer_sender_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
   Request FOP to terminate rpc-connection. Sent from sender to receiver.
 */
struct m0_rpc_fop_conn_terminate {
	/**
	   sender_id of rpc-connection being terminated.
	 */
	uint64_t ct_sender_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
   Reply FOP to m0_rpc_conn_terminate. Sent from receiver to sender.
 */
struct  m0_rpc_fop_conn_terminate_rep {
	/**
	   Contains 0 if CONN_TERMINATE operation is successful, error code
	   otherwise.
	 */
	int32_t ctr_rc;
	/**
	   sender_id of rpc-connection being terminated.
	 */
	uint64_t ctr_sender_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


/**
   Request FOP to establish a new session. Sent from sender to receiver.
 */
struct m0_rpc_fop_session_establish {
	/**
	   sender_id of rpc-connection in which a new session is to be created.
	 */
	uint64_t rse_sender_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
   Reply of m0_rpc_fop_session_establish. Sent from receiver to
   sender.
 */
struct m0_rpc_fop_session_establish_rep {
	/**
	   Contains 0 if SESSION_ESTABLISH operation is successful, error code
	   otherwise.
	 */
	int32_t  rser_rc;
	/**
	   session_id assigned by receiver to the newly created session.
	   Has value SESSION_ID_INVALID if SESSION_ESTABLISH operation fails.
	 */
	uint64_t rser_session_id;
	/**
	   sender_id copied from m0_rpc_fop_session_establish.
	 */
	uint64_t rser_sender_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
   Request FOP to terminate a session. Sent from sender to receiver.
 */
struct m0_rpc_fop_session_terminate {
	/**
	   sender_id of rpc-connection to which the session being terminated
	   belongs.
	 */
	uint64_t rst_sender_id;
	/**
	   session_id of session being terminated.
	 */
	uint64_t rst_session_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
   Reply FOP to m0_rpc_fop_session_terminate. Sent from receiver to sender.
 */
struct m0_rpc_fop_session_terminate_rep {
	/**
	   Contains 0 if SESSION_TERMINATE operation is successful, error code
	   otherwise.
	 */
	int32_t  rstr_rc;
	/**
	   session_id of the session being terminated.
	 */
	uint64_t rstr_session_id;
	/**
	   sender_id of rpc-connection to which the session being terminated
	   belongs.
	 */
	uint64_t rstr_sender_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MERO_RPC_SESSION_FOPS_H__ */

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
