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
 * Original author: Nikita_Danilov <Nikita_Danilov@xyratex.com>
 *		    Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 06/27/2012
 */

#pragma once

#ifndef __MERO_RPC_ITEM_H__
#define __MERO_RPC_ITEM_H__

#include "lib/types.h"             /* m0_bcount_t */
#include "lib/tlist.h"
#include "lib/list.h"
#include "lib/time.h"
#include "sm/sm.h"                 /* m0_sm */
#include "rpc/onwire.h"            /* m0_rpc_item_header2 */

/**
   @addtogroup rpc

   @{
 */

/* Imports */
struct m0_rpc_session;
struct m0_bufvec_cursor;
struct m0_rpc_frm;
struct m0_rpc_machine;

/* Forward declarations */
struct m0_rpc_item_ops;
struct m0_rpc_item_type;

enum m0_rpc_item_priority {
	M0_RPC_ITEM_PRIO_MIN,
	M0_RPC_ITEM_PRIO_MID,
	M0_RPC_ITEM_PRIO_MAX,
	M0_RPC_ITEM_PRIO_NR
};

enum m0_rpc_item_state {
	M0_RPC_ITEM_UNINITIALISED,
	M0_RPC_ITEM_INITIALISED,
	/**
	 * Item is in one of the WAITING_ queues maintained by formation.
	 * The item is waiting to be selected by formation machine for sending
	 * on the network.
	 */
	M0_RPC_ITEM_ENQUEUED,
	/**
	 * Deadline of item is expired.
	 * Item is in one of URGENT_* queues maintained by formation.
	 * Formation should send the item as early as possible.
	 */
	M0_RPC_ITEM_URGENT,
	/**
	 * Item is serialised in a network buffer and the buffer is submitted
	 * to network layer for sending.
	 */
	M0_RPC_ITEM_SENDING,
	/**
	 * The item is successfully placed on the network.
	 * Note that it does not state anything about whether the item is
	 * received or not.
	 */
	M0_RPC_ITEM_SENT,
	/**
	 * Only request items which are successfully sent over the wire
	 * can be in this state. Request item in this state is expecting a
	 * reply.
	 */
	M0_RPC_ITEM_WAITING_FOR_REPLY,
	/**
	 * When a reply is received for a request item, RPC moves the request
	 * item to REPLIED state.
	 */
	M0_RPC_ITEM_REPLIED,
	/**
	 * Received item is valid and is accepted.
	 */
	M0_RPC_ITEM_ACCEPTED,
	/**
	 * Item is failed.
	 */
	M0_RPC_ITEM_FAILED,
	M0_RPC_ITEM_NR_STATES,
};

/**
   RPC item direction.
 */
enum m0_rpc_item_dir {
	M0_RPC_ITEM_INCOMING,
	M0_RPC_ITEM_OUTGOING,
};

enum m0_rpc_item_resend {
	/**
	 * @todo Provide proper value for re-send interval
	 */
	M0_RPC_ITEM_RESEND_INTERVAL =   1, /* in secs */
	M0_RPC_ITEM_REPLY_CACHE_TMO = 100, /* refer to M0T1FS_RPC_TIMEOUT */
	M0_RPC_ITEM_REQ_CACHE_TMO   = 100, /* refer to M0T1FS_RPC_TIMEOUT */
};

/**
   A single RPC item, such as a FOP or ADDB Record.  This structure should be
   included in every item being sent via RPC layer core to emulate relationship
   similar to inheritance and to allow extending the set of rpc_items without
   modifying core rpc headers.
   @see m0_fop.
 */
struct m0_rpc_item {
/* Public fields: write-once/read */

	enum m0_rpc_item_priority        ri_prio;

	/** HA epoch transferred by the item. */
	uint64_t                         ri_ha_epoch;

	/** Absolute time after which formation should not delay sending
	    the item.
	 */
	m0_time_t			 ri_deadline;
	/** Time to wait before resending
	 * (defaults to M0_RPC_ITEM_RESEND_INTERVAL seconds). */
	m0_time_t                        ri_resend_interval;
	/** How many resending attempts to make (defaults to ~0). */
	uint64_t                         ri_nr_sent_max;
	struct m0_rpc_session           *ri_session;
	/** item operations */
	const struct m0_rpc_item_ops	*ri_ops;
	/**
	 * Item flags. A bitmask of values from enum m0_rpc_item_flags.
	 *
	 * This field is packed in item header when the item is sent and copied
	 * back in this field on receiver.
	 */
	uint32_t                         ri_flags;

/* Public fields: read only */

	struct m0_rpc_machine           *ri_rmachine;
	int32_t                          ri_error;
	/** reply item */
	struct m0_rpc_item              *ri_reply;

/* Private fields: */
	/** If ri_deadline is not in past the ri_deadline_timeout is used to
	    move item from ENQUEUD to URGENT state.
	 */
	struct m0_sm_timeout             ri_deadline_timeout;
	/** Resend timer.

	    Invokes item_timer_cb() after every item->ri_resend_interval.
	    item_timer_cb() then decides whether to resend the item or timeout
	    the operation, depending on ri_nr_sent and ri_nr_sent_max.
	 */
	struct m0_sm_timer               ri_timer;
	/** RPC item state machine.
	    @see outgoing_item_states
	    @see incoming_item_states
	 */
	struct m0_sm                     ri_sm;
	/** Number of times the item was sent */
	uint32_t                         ri_nr_sent;
	/** Reply received when request is still in SENDING state is kept
	    "pending" until the request item moves to SENT state.
	 */
	struct m0_rpc_item              *ri_pending_reply;

	/** Cookie id by which we find the request from the reply. */
	uint64_t                         ri_cookid;

	/** On-wire data of the item. */
	struct m0_rpc_item_header2       ri_header;

	/** Item size in bytes. header + payload.
	    Set during first call to m0_rpc_item_size() on this item.
	 */
	size_t                           ri_size;
	/** Pointer to the type object for this item */
	const struct m0_rpc_item_type	*ri_type;
	/** Time spent in rpc layer. */
	m0_time_t			 ri_rpc_time;
	/** List of compound items. */
	struct m0_tl			 ri_compound_items;
	/** Link through which items are anchored on list of
	    m0_rpc_item:ri_compound_items. */
	struct m0_tlink			 ri_field;
	/** Link in one of m0_rpc_frm::f_itemq[] list.
	    List descriptor: itemq
	 */
	struct m0_tlink                  ri_iq_link;
	/** Link in RPC packet. m0_rpc_packet::rp_items
	    List descriptor: packet_item.
	    XXX An item cannot be in itemq and in packet at the same time.
	    Hence, iff needed, ri_iq_link and ri_plink can be replaced with
	    just one tlink.
	 */
	struct m0_tlink                  ri_plink;
	/** Packet this item has been placed into.
	    It is required if the item is to be cancelled while
	    being sent.
	 */
	struct m0_rpc_packet            *ri_packet;
	/** One of m0_rpc_frm::f_itemq[], in which this item is placed. */
	struct m0_tl                    *ri_itemq;
	struct m0_rpc_frm               *ri_frm;
	/** Link through which items are anchored on
	    m0_rpc_session::s_pending_cache.
	 */
	struct m0_tlink                  ri_pending_link;
	/** M0_RPC_ITEM_MAGIC */
	uint64_t			 ri_magic;
	/** Link for m0_rpc_item_cache::ric_items. */
	struct m0_tlink			 ri_cache_link;
	/** After this time item can be safely removed from reply cache */
	m0_time_t			 ri_cache_deadline;
};

enum m0_rpc_item_flags {
	/**
	 * Item is being sent not for the first time.
	 *
	 * RPC sets this field internally, when the item is resent.
	 */
	M0_RIF_DUP  = 1 << 0,
	/**
	 * Sender already has the reply.
	 */
	M0_RIF_REPLIED = 1 << 1,
};

struct m0_rpc_item_ops {
	/**
	   RPC layer executes this callback when,
	   - item is sent over the network;
	   - or item sending failed in which case item->ri_error != 0.

	   Note that it does not state anything about whether item
	   is received on receiver or not.

	   IMP: Called with rpc-machine mutex held. Do not reenter in RPC.
	 */
	void (*rio_sent)(struct m0_rpc_item *item);
	/**
	   RPC layer executes this callback only for request items when,
	   - a reply is received to the request item;
	   - or any failure has occured (including timeout, item not posted
	     due to service being cancelled) in which case
	     item->ri_error != 0

	   If item->ri_error != 0, then item->ri_reply may or may not be NULL.

	   For a request, sender can receive one of following two types
	   of replies:
	   - generic-reply (m0_fop_generic_reply):
	     This type of reply is received when operation fails in generic
	     fom phases;
	   - operation specific reply.

	   Implementation of rio_replied() should check three levels of error,
	   in specified sequence, to determine operation status:
	   1. item->ri_error;
	   2. error reported by generic-reply fop;
	   3. error code in operation specific part of fop.
	   @see m0_rpc_session_terminate_reply_received() for example.
	   @see m0_rpc_item_is_generic_reply_fop()
	   @see m0_rpc_item_generic_reply_rc()

	   IMP: Called with rpc-machine mutex held. Do not reenter in RPC.
		Implementation of rio_replied() should avoid taking any
		locks, to ensure there are no lock ordering violations within
		application locks and rpc-machine lock. If taking application
		level lock is essential then consider using AST.
		See rm/rm_fops.c:rm_reply_process() for example.
	 */
	void (*rio_replied)(struct m0_rpc_item *item);
};

void m0_rpc_item_init(struct m0_rpc_item *item,
		      const struct m0_rpc_item_type *itype);

void m0_rpc_item_fini(struct m0_rpc_item *item);

/** Increments item's reference counter. */
void m0_rpc_item_get(struct m0_rpc_item *item);

/** Decrements item's reference counter. */
void m0_rpc_item_put(struct m0_rpc_item *item);
void m0_rpc_item_put_lock(struct m0_rpc_item *item);

M0_EXTERN m0_bcount_t m0_rpc_item_onwire_header_size;
M0_EXTERN m0_bcount_t m0_rpc_item_onwire_footer_size;
m0_bcount_t m0_rpc_item_payload_size(struct m0_rpc_item *item);
m0_bcount_t m0_rpc_item_size(struct m0_rpc_item *item);

/** Checks whether item payload exceeds max item payload. */
M0_INTERNAL
bool m0_rpc_item_max_payload_exceeded(struct m0_rpc_item    *item,
				      struct m0_rpc_session *session);

/**
   Waits until item reaches in one of states specified in
   _states_ or absolute timeout specified by _timeout_ is
   passed.

   @see m0_sm_timedwait() to know more about values returned
        by this function.
 */
int m0_rpc_item_timedwait(struct m0_rpc_item *item,
			  uint64_t states, m0_time_t timeout);

/**
   Waits until either item reaches in one of REPLIED/FAILED states
   or timeout is elapsed.

   @return 0 when item is REPLIED,
   @return item->ri_error if item is FAILED.
 */
int m0_rpc_item_wait_for_reply(struct m0_rpc_item *item,
			       m0_time_t timeout);
/**
   Deletes the item from all rpc queues.
   If reply arrives for the original cancelled item, it is ignored.

   - Item is moved to M0_RPC_ITEM_FAILED state
   - Item will be deleted once the user releases the reference/s it had held
     for this item, e.g. at the time of creation and other explicit references,
     if any.
 */
void m0_rpc_item_cancel(struct m0_rpc_item *item);

/**
 * Similar to m0_rpc_item_cancel() except that it does not lock the RPC
 * machine but expects that it is locked by the caller.
 */
void m0_rpc_item_cancel_nolock(struct m0_rpc_item *item);

/**
   Deletes the item from all rpc queues and re-initialises it.
   If reply arrives for the original cancelled item, it is ignored.

   - Item is first moved to M0_RPC_ITEM_FAILED state and then is moved to
     M0_RPC_ITEM_UNINITIALISED state
   - The item can be re-posted immediately after this function returns. When
     re-posted, the item gets new xid and is treated as a completely different
     item.
 */
void m0_rpc_item_cancel_init(struct m0_rpc_item *item);

int32_t m0_rpc_item_error(const struct m0_rpc_item *item);

/**
   For default implementations of these interfaces for fops
   @see M0_FOP_DEFAULT_ITEM_TYPE_OPS
 */
struct m0_rpc_item_type_ops {
	/**
	   Returns item payload size.
	   @see m0_fop_payload_size()
	 */
	m0_bcount_t (*rito_payload_size)(const struct m0_rpc_item *item);

	/**
	   Return true iff item1 and item2 are equal.
	   @todo XXX Implement rito_eq for fops
	 */
	bool (*rito_eq)(const struct m0_rpc_item *i1,
			const struct m0_rpc_item *i2);

	/**
	   Coalesce rpc items that share same fid and intent(read/write).
	 */
	void (*rito_io_coalesce)(struct m0_rpc_item *head,
				 struct m0_list *list, uint64_t size);

	/**
	   Serialises item at location given by cur.
	   @see m0_fop_item_type_default_encode()
	 */
	int (*rito_encode)(const struct m0_rpc_item_type *item_type,
		           struct m0_rpc_item *item,
	                   struct m0_bufvec_cursor *cur);
	/**
	   Create in memory item from serialised representation of item
	   @see m0_fop_item_type_default_decode()
	 */
	int (*rito_decode)(const struct m0_rpc_item_type *item_type,
			   struct m0_rpc_item **item,
			   struct m0_bufvec_cursor *cur);

	bool (*rito_try_merge)(struct m0_rpc_item *container,
			       struct m0_rpc_item *component,
			       m0_bcount_t         limit);

	/**
	   RPC item type specific routine that will take reference on the item.
           For fops, this routine is almost always set to m0_fop_item_get().
	 */
	void (*rito_item_get)(struct m0_rpc_item *item);
	/**
	   RPC item type specific routine that will drop reference on the item.
           For fops, this routine is almost always set to m0_fop_item_put().
	 */
	void (*rito_item_put)(struct m0_rpc_item *item);

	/**
	   This method is called on receiver side when RPC layer processed the
	   item of this type and wants to deliver it to the upper layer. If this
	   method is NULL, the default action is to call m0_reqh_fop_handle().

	   @note rpcmach can be different from item_machine(item) for connection
	   establishing items.
	*/
	int (*rito_deliver)(struct m0_rpc_machine *rpcmach,
			    struct m0_rpc_item *item);
};

/**
   Possible values for m0_rpc_item_type::rit_flags.
   Flags M0_RPC_ITEM_TYPE_REQUEST, M0_RPC_ITEM_TYPE_REPLY and
   M0_RPC_ITEM_TYPE_ONEWAY are mutually exclusive.
 */
enum m0_rpc_item_type_flags {
	/** Receiver of item is expected to send reply to item of this type */
	M0_RPC_ITEM_TYPE_REQUEST = (1 << 0),
	/**
	  Item of this type is reply to some item of M0_RPC_ITEM_TYPE_REQUEST
	  type.
	*/
	M0_RPC_ITEM_TYPE_REPLY   = (1 << 1),
	/**
	  This is a one-way item. There is no reply for this type of
	  item
	*/
	M0_RPC_ITEM_TYPE_ONEWAY  = (1 << 2),
	/**
	  Item of this type can modify file-system state on receiver.
	*/
	M0_RPC_ITEM_TYPE_MUTABO  = (1 << 3),

	M0_RPC_MUTABO_REQ = M0_RPC_ITEM_TYPE_MUTABO | M0_RPC_ITEM_TYPE_REQUEST
};

/**
   Type of an RPC item.
   There is an instance of m0_rpc_item_type for each value of rit_opcode.
 */
struct m0_rpc_item_type {
	/** Unique operation code. */
	uint32_t                           rit_opcode;
	/** Operations that can be performed on the type */
	const struct m0_rpc_item_type_ops *rit_ops;
	/** @see m0_rpc_item_type_flags */
	uint64_t                           rit_flags;
	/** Linkage into rpc item types list (m0_rpc_item_types_list) */
	struct m0_tlink                    rit_linkage;
	/** Magic no for the item type struct */
	uint64_t                           rit_magic;
	struct m0_sm_conf                  rit_incoming_conf;
	struct m0_sm_conf                  rit_outgoing_conf;
};

/**
  Registers a new rpc item type by adding an entry to the rpc item types list.
  Asserts when an entry for that opcode already exists in the item types
  list.

  @param item_type The rpc item type to be registered.
*/
M0_INTERNAL void m0_rpc_item_type_register(struct m0_rpc_item_type *item_type);

/** De-registers an rpc item type by deleting the corresponding entry in the
    rpc item types list.

    @param item_type The rpc item type to be deregistered.
*/
M0_INTERNAL void m0_rpc_item_type_deregister(struct m0_rpc_item_type
					     *item_type);

/** Returns a pointer to rpc item type registered for an opcode

  @param opcode Unique operation code for the rpc item type to be looked up.
  @retval Pointer to the rpc item type for that opcode.
  @retval NULL if the item type is not registered.
*/
M0_INTERNAL struct m0_rpc_item_type *m0_rpc_item_type_lookup(uint32_t opcode);

/**
   Extract remote endpoint address from given item
 */
M0_INTERNAL const char *
m0_rpc_item_remote_ep_addr(const struct m0_rpc_item *item);

enum {
	RIC_HASH_MASK = 0xff,
	RIC_HASH_SIZE = RIC_HASH_MASK + 1,
};

/**
 * Simple rpc item cache.
 *
 * Highlights
 * - cache is protected with external lock m0_rpc_item_cache::ric_lock.
 *   It should be taken before calling any m0_rpc_item_cache function;
 * - cache assumes that same items have exactly the same xid;
 * - item xid uniqueness is responsibility of the user;
 * - cache protects items contained in from preliminary finalisation using
 *   rpc item reference counting (m0_rpc_item_get() and m0_rpc_item_put());
 * - m0_rpc_item_cache_lookup() doesn't take any reference. If user wants to
 *   use rpc item after deadline then reference should be taken by the user;
 * - rpc items are added to cache using m0_rpc_item_cache_add();
 * - rpc items are removed from the cache if:
 *   - user calls m0_rpc_item_cache_del();
 *   - user calls m0_rpc_item_cache_purge() - in this case all items with
 *     deadline passed are removed;
 *   - deadline passed.
 *     Note: rpc items may remain in cache even after deadline.
 */
struct m0_rpc_item_cache {
	/** Uses m0_rpc_item::ri_cache_link */
	struct m0_tl    *ric_items;
	/** External lock for item list protection */
	struct m0_mutex *ric_lock;
};

/**
 * Besides other cache initialization stuff allocates memory for the cache,
 * so may return -ENOMEM.
 */
M0_INTERNAL int m0_rpc_item_cache_init(struct m0_rpc_item_cache *ic,
				       struct m0_mutex          *lock);
M0_INTERNAL void m0_rpc_item_cache_fini(struct m0_rpc_item_cache *ic);
M0_INTERNAL bool m0_rpc_item_cache__invariant(struct m0_rpc_item_cache *ic);

/**
 * Adds item to the cache.
 *
 * If an item is already in the cache, updates its deadline only.
 * It takes one reference to the item if item is actually added to the cache.
 * There is no guarantee that item will remain in cache after deadline passed.
 * @param ic - items cache.
 * @param item to be added.
 * @param deadline - timeout in absolute time value.
 * @return true if item was added,
 * @return false otherwise.
 */
M0_INTERNAL bool m0_rpc_item_cache_add(struct m0_rpc_item_cache *ic,
				       struct m0_rpc_item	*item,
				       m0_time_t		 deadline);
/**
 * Remove item from the cache.
 *
 * If item is not in cache, does nothing.
 * Puts one reference to the item if item is actually removed from the cache.
 */
M0_INTERNAL void m0_rpc_item_cache_del(struct m0_rpc_item_cache *ic,
				       uint64_t			 xid);
/**
 * Searches cache for the item with given xid.
 *
 * Returns NULL if there is no item with the given xid in the cache.
 * It doesn't take any reference to the item.
 */
M0_INTERNAL struct m0_rpc_item *
m0_rpc_item_cache_lookup(struct m0_rpc_item_cache *ic, uint64_t xid);

/** Deletes expired items from the cache. */
M0_INTERNAL void m0_rpc_item_cache_purge(struct m0_rpc_item_cache *ic);
/** Deletes all items from the cache. */
M0_INTERNAL void m0_rpc_item_cache_clear(struct m0_rpc_item_cache *ic);

/** Initialises the pending item cache. */
M0_INTERNAL void m0_rpc_item_pending_cache_init(struct m0_rpc_session *session);
/** Finalises the pending item cache. */
M0_INTERNAL void m0_rpc_item_pending_cache_fini(struct m0_rpc_session *session);
/**
 * Adds item to the pending item cache only if it is not already present
 * there. It is going to be present there in case of resend.
 */
M0_INTERNAL void m0_rpc_item_pending_cache_add( struct m0_rpc_item *item);
/** Deletes item from the pending item cache. */
M0_INTERNAL void m0_rpc_item_pending_cache_del(struct m0_rpc_item *item);

#endif

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
