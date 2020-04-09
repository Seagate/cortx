/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 11-Feb-2013
 */


#pragma once

#ifndef __MERO_RPC_ITEM_SOURCE_H__
#define __MERO_RPC_ITEM_SOURCE_H__


/**
 * @defgroup rpc
 *
 * @{
 */

struct m0_rpc_item_source;
struct m0_rpc_item_source_ops;

/**
   RPC Item source.

   Most applications "post" rpc items to RPC. This can be thought of as "push"
   model where users push items to RPC module to send them.

   For some applications, such as ADDB, "pull" model is more efficient.
   In pull model, RPC module asks application if the later has any item to
   send.

   RPC module can ask for an item iff application has registered an
   @a m0_rpc_item_source with @a m0_rpc_conn. Once registered application should
   not free the @a m0_rpc_item_source instance without first deregistering it.
   @see m0_rpc_item_source_ops::riso_conn_terminating.

   @note Current implementation allows item-sources to provide only one-way
         items.

   @see item_source_invariant()
 */
struct m0_rpc_item_source {
	/** @see M0_RPC_ITEM_SOURCE_MAGIC */
	uint64_t                             ris_magic;
	const char                          *ris_name;
	const struct m0_rpc_item_source_ops *ris_ops;
	struct m0_rpc_conn                  *ris_conn;
	/** Link in m0_rpc_conn::c_item_sources.
	    List descriptor: item_source
	 */
	struct m0_tlink                      ris_tlink;
};

/**
   Callbacks invoked by RPC on registered item-sources.

   The callback subroutines are invoked within the scope of the RPC machine
   lock so should not make re-entrant calls to the RPC subsystem that take
   RPC machine lock.

   Implementation of all of the callbacks is mandatory.
 */
struct m0_rpc_item_source_ops {
	/** Returns true iff the item-source has item to send.

	    @pre m0_rpc_machine_is_locked(ris->ris_conn->c_rpc_machine)
	 */
	bool (*riso_has_item)(const struct m0_rpc_item_source *ris);

	/** Returns an item to be sent. Returns NULL if couldn't form an item.
	    Payload size should not exceed max_payload_size.

	    Important: RPC reuses reference on returned item. If returned item
	    has ref-count == 1 then the item will be freed as soon as it is
	    sent/failed.

	    @pre m0_rpc_machine_is_locked(ris->ris_conn->c_rpc_machine)
	    @post ergo(result != NULL,
		       m0_rpc_item_is_oneway(result) &&
		       m0_rpc_item_payload_size(result) <= max_payload_size))
	 */
	struct m0_rpc_item *(*riso_get_item)(struct m0_rpc_item_source *ris,
					     m0_bcount_t max_payload_size);

	/** This callback is invoked when rpc-connection is being terminated
	    while the connection still has item-sources registered with it.

	    <a>ris</a> is deregistered before invoking this callback.
	    Implementation of this routine can choose to free <a>ris</a>.
	    RPC won't touch <a>ris</a> after this callback.

	    @pre !m0_rpc_item_source_is_registered(ris)
	 */
	void (*riso_conn_terminating)(struct m0_rpc_item_source *ris);
};

/**
   @pre ris != NULL && name != NULL
   @pre ops != NULL && ops->riso_has_item != NULL &&
	ops->riso_get_item != NULL && ops->riso_conn_terminating != NULL
 */
void m0_rpc_item_source_init(struct m0_rpc_item_source *ris,
			     const char *name,
			     const struct m0_rpc_item_source_ops *ops);

/**
   Returns true iff ris is registered.
 */
bool m0_rpc_item_source_is_registered(const struct m0_rpc_item_source *ris);

/**
   @pre !m0_rpc_item_source_is_registered(ris)
 */
void m0_rpc_item_source_fini(struct m0_rpc_item_source *ris);

/**
   Registers an item-source with rpc-connection.

   @pre !m0_rpc_item_source_is_registered(ris)
   @post m0_rpc_item_source_is_registered(ris)
 */
void m0_rpc_item_source_register(struct m0_rpc_conn *conn,
				struct m0_rpc_item_source *ris);

/**
   Registers an item-source with a locked rpc-connection.

   Identical to m0_rpc_item_source_register() except assumes that the rpc
   machine is already locked.

   @pre !m0_rpc_item_source_is_registered(ris)
   @post m0_rpc_item_source_is_registered(ris)
 */
void m0_rpc_item_source_register_locked(struct m0_rpc_conn *conn,
					struct m0_rpc_item_source *ris);

/**
   Deregisters item-source.

   Calling m0_rpc_item_source_deregister() on an already deregistered
   item-source is safe.

   @post !m0_rpc_item_source_is_registered(ris)
 */
void m0_rpc_item_source_deregister(struct m0_rpc_item_source *ris);

/** @} end of rpc group */

#endif /* __MERO_RPC_ITEM_SOURCE_H__ */


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
