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
 * Original author: Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 04/28/2011
 */

#pragma once

#ifndef __MERO_RPC_FORMATION2_H__
#define __MERO_RPC_FORMATION2_H__

/**
 * @addtogroup rpc
 * @{
 */

/**
   Formation component for Mero RPC layer is what IO scheduler is for
   block device. Because of network layer overhead associated with each
   message (i.e. buffer), sending each individual RPC item directly to network
   layer can be inefficient. Instead, formation component tries to send
   multiple RPC items in same network layer message to improve performance.

   RPC items that are posted to RPC layer for sending are enqueued in formation
   queue. Formation then prepares "RPC Packets". A RPC Packet is collection
   of RPC items that are sent together in same network layer buffer.

   To improve performance formation component does two things:

   - Batching:
     Formation batches multiple RPC items - that are targeted to same
     destination end-point - in RPC packets.

   - Merging:
     If there are any two items A and B such that they can be merged into
     _one_ item then Formation tries to merge these items and add this
     aggregate RPC item to the packet. How the "contents" of RPC items A and B
     are merged together is dependent on _item type_ of A and B. Formation
     is not aware about how these items are merged.

   While forming RPC packets Formation has to obey several "constraints":
   - max_packet_size:
   - max_nr_bytes_accumulated:
   - max_nr_segments
   - max_nr_packets_enqed
   @see m0_rpc_frm_constraints for more information.

   It is important to note that Formation has something to do only on
   "outgoing path".

   NOTE:
   - RPC Packet is also referred as "RPC" in some other parts of code and docs
   - A "one-way" item is also referred as "unsolicited" item.

   @todo XXX Support for "RPC Group"
   @todo XXX RPC item cancellation
   @todo XXX Better RPC level flow control than the one provided by
             m0_rpc_frm_constraints::fc_max_nr_packets_enqed
 */

#include "lib/types.h"
#include "lib/tlist.h"

/* Imports */
struct m0_rpc_packet;
struct m0_rpc_item;
struct m0_rpc_session;

/* Forward references */
struct m0_rpc_frm_ops;

/**
   Constraints that should be taken into consideration while forming packets.
 */
struct m0_rpc_frm_constraints {

	/**
	   Maximum number of packets such that they are submitted to network
	   layer, but its completion callback is not yet received.
	 */
	uint64_t    fc_max_nr_packets_enqed;

	/**
	   On wire size of a packet should not cross this limit. This is
	   usually set to maximum supported size of SEND network buffer.

	   @see m0_rpc_machine::rm_min_recv_size
	 */
	m0_bcount_t fc_max_packet_size;

	/**
	   Maximum number of non-contiguous memory segments allowed by
	   network layer in a network buffer.
	 */
	uint64_t    fc_max_nr_segments;

	/**
	   If sum of on-wire sizes of all the enqueued RPC items is greater
	   than fc_max_nr_bytes_accumulated, then formation should try to
	   form RPC packet out of them.
	 */
	m0_bcount_t fc_max_nr_bytes_accumulated;
};

/**
   Possible states of formation state machine.

   @see m0_rpc_frm::f_state
 */
enum frm_state {
	FRM_UNINITIALISED,
	/** There are no pending items in the formation queue AND
	    No callback is pending @see frm_is_idle()
	 */
	FRM_IDLE,
	/** There are few items waiting in the formation queue OR
	    some packet done callbacks are yet to be received.
	 */
	FRM_BUSY,
	FRM_NR_STATES
};

/**
   Formation partitions RPC items in these types of queues.
   An item can migrate from one queue to another depending on its state.

   URGENT_* are the queues which contain items whose deadline has been
   passed. These items should be sent as soon as possible.

   WAITING_* are the queues which contain items whose deadline is not yet
   reached. An item from these queues can be picked for formation even
   before its deadline is passed.
 */
enum m0_rpc_frm_itemq_type {
	FRMQ_URGENT,
	FRMQ_WAITING,
	FRMQ_NR_QUEUES
};

/**
   Formation state machine.

   There is one instance of m0_rpc_frm for each destination end-point.

   Events in which the formation state machine is interested are:
   - RPC item is posted for sending
   - RPC packet has been sent or packet sending is failed
   - deadline timer of WAITING item is expired

   Events that formation machine triggers for rest of RPC are:
   - Packet is ready for sending

@verbatim
                FRM_UNINITIALISED
                      |  ^
     m0_rpc_frm_init()|  |
                      |  | m0_rpc_frm_fini()
                      V  |
                    FRM_IDLE
                      |  ^
 m0_rpc_frm_enq_item()|  | frm_itemq_remove() or m0_rpc_frm_packet_done()
   [frm_is_idle()]    |  | [!frm_is_idle()]
                      V  |
                    FRM_BUSY
@endverbatim

   <B>Concurrency and Existence: </B> @n

   Access to m0_rpc_frm instance is synchronised by
   m0_rpc_machine::rm_sm_grp::s_lock.

   m0_rpc_frm is not reference counted. It is responsibility of user to
   free m0_rpc_frm. Ensuring that m0_rpc_frm is in IDLE state, before
   finalising it, is left to user.

   @see frm_invariant()
 */
struct m0_rpc_frm {
	/**
	   Current state.

	   Note: Because of very simple nature of formation state machine,
	   currently we are not using generic sm framework. If need arises
	   in future, we should implement formation state machine using
	   m0_sm framework.
	 */
	enum frm_state                 f_state;

	/**
	   Lists of items enqueued to Formation that are not yet
	   added to any Packet. @see m0_rpc_frm_itemq_type
	   itemq are sorted by m0_rpc_item::ri_prio (highest priority first).
	   All items having equal priority are sorted by
	   m0_rpc_item::ri_deadline (earlier first).
	   An item is removed from itemq immediately upon adding the item to
	   any packet.
	   link: m0_rpc_item::ri_iq_link
	   descriptor: itemq
	 */
	struct m0_tl                   f_itemq[FRMQ_NR_QUEUES];

	/** Total number of items waiting in itemq */
	uint64_t                       f_nr_items;

	/** Sum of on-wire size of all the items in itemq */
	m0_bcount_t                    f_nr_bytes_accumulated;

	/** Number of packets for which "Packet done" callback is pending */
	uint64_t                       f_nr_packets_enqed;

	/** Limits that formation should respect */
	struct m0_rpc_frm_constraints  f_constraints;

	const struct m0_rpc_frm_ops   *f_ops;

	/** FRM_MAGIC */
	uint64_t                       f_magic;
};

/**
   Events reported by formation to rest of RPC layer.

   @see m0_rpc_frm_default_ops
 */
struct m0_rpc_frm_ops {
	/**
	   A packet is ready to be sent over network.
	   @return 0 iff packet has been submitted to network layer.
		   Otherwise the items in packet p are moved to
		   FAILED state and are removed from p.
		   m0_rpc_packet instance pointed by p is freed.
	   @note through there is only one implementation instance of
	         this routine in the real code, the UTs code still
		 implement its own version of this routine also
	         and heavily depend on it - that's why we still
		 have this vector here.
	 */
	int (*fo_packet_ready)(struct m0_rpc_packet *p);
};

/**
   Default implementation of m0_rpc_frm_ops
 */
extern const struct m0_rpc_frm_ops m0_rpc_frm_default_ops;

/**
   Load default values for various constraints, that just works.
   Useful for unit tests.
 */
M0_INTERNAL void
m0_rpc_frm_constraints_get_defaults(struct m0_rpc_frm_constraints *constraint);

/**
   Initialises frm instance.

   Object pointed by constraints is copied inside m0_rpc_frm.

   @pre  frm->f_state == FRM_UNINITIALISED
   @post frm->f_state == FRM_IDLE
 */
M0_INTERNAL void m0_rpc_frm_init(struct m0_rpc_frm *frm,
				 struct m0_rpc_frm_constraints *constraints,
				 const struct m0_rpc_frm_ops *ops);

/**
   Finalises m0_rpc_frm instance.

   @pre  frm->f_state == FRM_IDLE
   @post frm->f_state == FRM_UNINITIALISED
 */
M0_INTERNAL void m0_rpc_frm_fini(struct m0_rpc_frm *frm);

/**
   Enqueue an item for sending.
 */
M0_INTERNAL void m0_rpc_frm_enq_item(struct m0_rpc_frm *frm,
				     struct m0_rpc_item *item);
M0_INTERNAL void m0_rpc_frm_remove_item(struct m0_rpc_frm *frm,
					struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_frm_item_deadline_passed(struct m0_rpc_frm *frm,
						 struct m0_rpc_item *item);

/**
   Callback for a packet which was previously enqueued.
 */
M0_INTERNAL void m0_rpc_frm_packet_done(struct m0_rpc_packet *packet);

/**
   Runs formation algorithm.
 */
M0_INTERNAL void m0_rpc_frm_run_formation(struct m0_rpc_frm *frm);

M0_INTERNAL struct m0_rpc_frm *session_frm(const struct m0_rpc_session *s);

M0_TL_DESCR_DECLARE(itemq, M0_EXTERN);
M0_TL_DECLARE(itemq, M0_INTERNAL, struct m0_rpc_item);

M0_INTERNAL struct m0_rpc_chan *frm_rchan(const struct m0_rpc_frm *frm);
M0_INTERNAL struct m0_rpc_machine *frm_rmachine(const struct m0_rpc_frm *frm);

M0_INTERNAL bool item_is_in_waiting_queue(const struct m0_rpc_item *item,
					  const struct m0_rpc_frm *frm);

/** @} */
#endif /* __MERO_RPC_FORMATION2_H__ */
