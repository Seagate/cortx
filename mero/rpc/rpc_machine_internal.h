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

#ifndef __MERO_RPC_MACHINE_INT_H__
#define __MERO_RPC_MACHINE_INT_H__

#include "lib/tlist.h"
#include "lib/refs.h"
#include "rpc/formation2_internal.h"

/* Imports */
struct m0_net_end_point;
struct m0_rpc_machine;
struct m0_rpc_machine_watch;
struct m0_rpc_conn;


/**
   @addtogroup rpc

   @{
 */

/**
   Struct m0_rpc_chan provides information about a target network endpoint.
   An rpc machine (struct m0_rpc_machine) contains list of m0_rpc_chan
   structures targeting different net endpoints.
   Rationale A physical node can have multiple endpoints associated with it.
   And multiple services can share endpoints for transport.
   The rule of thumb is to use one transfer machine per endpoint.
   So to make sure that services using same endpoint,
   use the same transfer machine, this structure has been introduced.
   Struct m0_rpc_conn is used for a particular service and now it
   points to a struct m0_rpc_chan to identify the transfer machine
   it is working with.
 */
struct m0_rpc_chan {
	/** Link in m0_rpc_machine::rm_chans list.
	    List descriptor: rpc_chan
	 */
	struct m0_tlink			  rc_linkage;
	/** Number of m0_rpc_conn structures using this transfer machine.*/
	struct m0_ref			  rc_ref;
	/** Formation state machine associated with chan. */
	struct m0_rpc_frm                 rc_frm;
	/** Destination end point to which rpcs will be sent. */
	struct m0_net_end_point		 *rc_destep;
	/** The rpc_machine, this chan structure is associated with.*/
	struct m0_rpc_machine		 *rc_rpc_machine;
	/** M0_RPC_CHAN_MAGIC */
	uint64_t			  rc_magic;
};

M0_INTERNAL void m0_rpc_machine_add_conn(struct m0_rpc_machine *rmach,
					 struct m0_rpc_conn    *conn);

M0_INTERNAL struct m0_rpc_conn *
m0_rpc_machine_find_conn(const struct m0_rpc_machine *machine,
			 const struct m0_rpc_item    *item);

M0_TL_DESCR_DECLARE(rpc_conn, M0_EXTERN);
M0_TL_DECLARE(rpc_conn, M0_INTERNAL, struct m0_rpc_conn);

M0_TL_DESCR_DECLARE(rmach_watch, M0_EXTERN);
M0_TL_DECLARE(rmach_watch, M0_INTERNAL, struct m0_rpc_machine_watch);

/**
  * Terminates all active incoming sessions and connections.
  *
  * Such cleanup is required to handle case where receiver is terminated
  * while one or more senders are still connected to it.
  *
  * For more information on this issue visit
  * <a href="http://goo.gl/5vXUS"> here </a>
  */
M0_INTERNAL void
m0_rpc_machine_cleanup_incoming_connections(struct m0_rpc_machine *machine);

/** @} */
#endif /* __MERO_RPC_MACHINE_INT_H__ */
