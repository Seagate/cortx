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
 * Original author: Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 04/28/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FORMATION
#include "lib/trace.h"
#include "lib/misc.h"    /* M0_SET0 */
#include "lib/memory.h"
#include "lib/tlist.h"
#include "mero/magic.h"
#include "lib/finject.h"       /* M0_FI_ENABLED */
#include "reqh/reqh.h"

#include "rpc/rpc_internal.h"

/**
 * @addtogroup rpc
 * @{
 */
static bool itemq_invariant(const struct m0_tl *q);
static m0_bcount_t itemq_nr_bytes_acc(const struct m0_tl *q);

static enum m0_rpc_frm_itemq_type
frm_which_qtype(struct m0_rpc_frm *frm, const struct m0_rpc_item *item);
static bool frm_is_idle(const struct m0_rpc_frm *frm);
static void frm_insert(struct m0_rpc_frm *frm, struct m0_rpc_item *item);
static void frm_remove(struct m0_rpc_frm *frm, struct m0_rpc_item *item);
static void __itemq_insert(struct m0_tl *q, struct m0_rpc_item *new_item);
static void __itemq_remove(struct m0_rpc_item *item);
static void frm_balance(struct m0_rpc_frm *frm);
static bool frm_is_ready(const struct m0_rpc_frm *frm);
static void frm_fill_packet(struct m0_rpc_frm *frm, struct m0_rpc_packet *p);
static void frm_fill_packet_from_item_sources(struct m0_rpc_frm    *frm,
					      struct m0_rpc_packet *p);
static int  frm_packet_ready(struct m0_rpc_frm *frm, struct m0_rpc_packet *p);
static void frm_try_merging_item(struct m0_rpc_frm  *frm,
				 struct m0_rpc_item *item,
				 m0_bcount_t         limit);

static bool item_less_or_equal(const struct m0_rpc_item *i0,
			       const struct m0_rpc_item *i1);
static void item_move_to_urgent_queue(struct m0_rpc_frm *frm,
				      struct m0_rpc_item *item);

static m0_bcount_t available_space_in_packet(const struct m0_rpc_packet *p,
					     const struct m0_rpc_frm    *frm);
static bool item_will_exceed_packet_size(struct m0_rpc_item         *item,
					 const struct m0_rpc_packet *p,
					 const struct m0_rpc_frm    *frm);

static bool item_supports_merging(const struct m0_rpc_item *item);

static void drop_all_items(struct m0_rpc_frm *frm);

static bool
constraints_are_valid(const struct m0_rpc_frm_constraints *constraints);

static const char *str_qtype[] = {
	[FRMQ_URGENT]   = "URGENT",
	[FRMQ_WAITING]  = "WAITING",
};

M0_BASSERT(ARRAY_SIZE(str_qtype) == FRMQ_NR_QUEUES);

#define frm_first_itemq(frm) (&(frm)->f_itemq[0])
#define frm_end_itemq(frm) (&(frm)->f_itemq[ARRAY_SIZE((frm)->f_itemq)])

#define for_each_itemq_in_frm(itemq, frm)  \
for (itemq = frm_first_itemq(frm); \
     itemq < frm_end_itemq(frm); \
     ++itemq)

M0_TL_DESCR_DEFINE(itemq, "rpc_itemq", M0_INTERNAL, struct m0_rpc_item,
		   ri_iq_link, ri_magic, M0_RPC_ITEM_MAGIC,
		   M0_RPC_ITEMQ_HEAD_MAGIC);
M0_TL_DEFINE(itemq, M0_INTERNAL, struct m0_rpc_item);

static bool frm_invariant(const struct m0_rpc_frm *frm)
{
	m0_bcount_t nr_bytes_acc = 0;
	uint64_t    nr_items = 0;

	return  frm != NULL &&
		frm->f_magic == M0_RPC_FRM_MAGIC &&
		frm->f_state > FRM_UNINITIALISED &&
		frm->f_state < FRM_NR_STATES &&
		frm->f_ops != NULL &&
		equi(frm->f_state == FRM_IDLE,  frm_is_idle(frm)) &&
		m0_forall(i, FRMQ_NR_QUEUES,
			  ({
				  const struct m0_tl *q = &frm->f_itemq[i];

				  nr_items     += itemq_tlist_length(q);
				  nr_bytes_acc += itemq_nr_bytes_acc(q);
				  itemq_invariant(q); })) &&
		frm->f_nr_items == nr_items &&
		frm->f_nr_bytes_accumulated == nr_bytes_acc;
}

static bool itemq_invariant(const struct m0_tl *q)
{
	return  q != NULL &&
		m0_tl_forall(itemq, item, q, ({
					const struct m0_rpc_item *prev =
						itemq_tlist_prev(q, item);
					ergo(prev != NULL,
					     item_less_or_equal(prev, item));
				}));
}

/**
   Defines total order of rpc items in itemq.
 */
static bool item_less_or_equal(const struct m0_rpc_item *i0,
			       const struct m0_rpc_item *i1)
{
	return	i0->ri_prio > i1->ri_prio ||
		(i0->ri_prio == i1->ri_prio &&
		 i0->ri_deadline <= i1->ri_deadline);
}

/**
   Returns sum of on-wire sizes of all the items in q.
 */
static m0_bcount_t itemq_nr_bytes_acc(const struct m0_tl *q)
{
	struct m0_rpc_item *item;
	m0_bcount_t         size;

	size = 0;
	m0_tl_for(itemq, q, item)
		size += m0_rpc_item_size(item);
	m0_tl_endfor;

	return size;
}

M0_INTERNAL struct m0_rpc_chan *frm_rchan(const struct m0_rpc_frm *frm)
{
	return container_of(frm, struct m0_rpc_chan, rc_frm);
}

M0_INTERNAL struct m0_rpc_machine *frm_rmachine(const struct m0_rpc_frm *frm)
{
	return frm_rchan(frm)->rc_rpc_machine;
}

static bool frm_rmachine_is_locked(const struct m0_rpc_frm *frm)
{
	return m0_rpc_machine_is_locked(frm_rmachine(frm));
}

M0_INTERNAL void m0_rpc_frm_constraints_get_defaults(struct
						     m0_rpc_frm_constraints *c)
{
	M0_ENTRY();

	/** @todo XXX decide default values for constraints */
	c->fc_max_nr_packets_enqed     = 100;
	c->fc_max_nr_segments          = 128;
	c->fc_max_packet_size          = 4096;
	c->fc_max_nr_bytes_accumulated = 4096;

	M0_LEAVE();
}

static bool
constraints_are_valid(const struct m0_rpc_frm_constraints *constraints)
{
	/** @todo XXX Check whether constraints are consistent */
	return constraints != NULL;
}

static bool frm_is_idle(const struct m0_rpc_frm *frm)
{
	return frm->f_nr_items == 0 && frm->f_nr_packets_enqed == 0;
}

M0_INTERNAL void m0_rpc_frm_init(struct m0_rpc_frm *frm,
				 struct m0_rpc_frm_constraints *constraints,
				 const struct m0_rpc_frm_ops *ops)
{
	struct m0_tl *q;

	M0_ENTRY("frm: %p", frm);
	M0_PRE(frm != NULL &&
	       ops != NULL &&
	       constraints_are_valid(constraints));

	M0_SET0(frm);
	frm->f_ops         =  ops;
	frm->f_constraints = *constraints; /* structure instance copy */
	frm->f_magic       =  M0_RPC_FRM_MAGIC;

	for_each_itemq_in_frm(q, frm)
		itemq_tlist_init(q);

	frm->f_state = FRM_IDLE;

	M0_POST_EX(frm_invariant(frm) && frm->f_state == FRM_IDLE);
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_frm_fini(struct m0_rpc_frm *frm)
{
	struct m0_tl *q;

	M0_ENTRY("frm: %p", frm);
	M0_PRE(frm_invariant(frm));
	M0_LOG(M0_DEBUG, "frm state: %d", frm->f_state);

	drop_all_items(frm);
	M0_ASSERT(frm->f_state == FRM_IDLE);
	for_each_itemq_in_frm(q, frm)
		itemq_tlist_fini(q);

	frm->f_state = FRM_UNINITIALISED;
	frm->f_magic = 0;

	M0_LEAVE();
}

static void drop_all_items(struct m0_rpc_frm *frm)
{
	struct m0_rpc_item *item;
	struct m0_tl       *q;
	int                 i;

	for (i = 0; i < FRMQ_NR_QUEUES; i++) {
		q = &frm->f_itemq[i];
		m0_tl_for(itemq, q, item) {
			M0_ASSERT(m0_rpc_item_is_oneway(item));
			m0_rpc_item_get(item);
			frm_remove(frm, item);
			m0_rpc_item_failed(item, -ECANCELED);
			m0_rpc_item_put(item);
		} m0_tl_endfor;
		M0_ASSERT(itemq_tlist_is_empty(q));
	}
}

M0_INTERNAL void m0_rpc_frm_enq_item(struct m0_rpc_frm *frm,
				     struct m0_rpc_item *item)
{
	M0_ENTRY("frm: %p item: %p", frm, item);
	M0_PRE(frm_rmachine_is_locked(frm));
	M0_PRE_EX(frm_invariant(frm) && item != NULL);

	frm_insert(frm, item);
	frm_balance(frm);

	M0_LEAVE();
}

static void frm_insert(struct m0_rpc_frm *frm, struct m0_rpc_item *item)
{
	enum m0_rpc_frm_itemq_type  qtype;
	struct m0_tl               *q;
	int                         rc;

	M0_ENTRY("frm: %p item: %p size %zu opcode %lu xid %lu",
		 frm, item, item->ri_size,
		 (unsigned long)item->ri_type->rit_opcode,
		 (unsigned long)item->ri_header.osr_xid);
	M0_PRE(item != NULL && !itemq_tlink_is_in(item));
	M0_LOG(M0_DEBUG, "priority: %d", item->ri_prio);

	qtype = frm_which_qtype(frm, item);
	q     = &frm->f_itemq[qtype];

	m0_rpc_item_get(item);
	__itemq_insert(q, item);

	M0_CNT_INC(frm->f_nr_items);
	frm->f_nr_bytes_accumulated += m0_rpc_item_size(item);
	item->ri_frm = frm;
	if (frm->f_state == FRM_IDLE)
		frm->f_state = FRM_BUSY;

up:
	if (item_is_in_waiting_queue(item, frm)) {
		m0_rpc_item_change_state(item, M0_RPC_ITEM_ENQUEUED);
		M0_LOG(M0_DEBUG, "%p Starting deadline timer", item);
		M0_ASSERT(!m0_sm_timeout_is_armed(&item->ri_deadline_timeout));
		/* For resent item, we may need to "re-arm"
		   ri_deadline_timeout.
		 */
		m0_sm_timeout_fini(&item->ri_deadline_timeout);
		m0_sm_timeout_init(&item->ri_deadline_timeout);
		rc = m0_sm_timeout_arm(&item->ri_sm,
				       &item->ri_deadline_timeout,
				       item->ri_deadline,
				       M0_RPC_ITEM_URGENT, 0);
		if (rc != 0) {
			M0_LOG(M0_NOTICE, "%p failed to start deadline timer",
			       item);
			item->ri_deadline = 0;
			item_move_to_urgent_queue(frm, item);
			goto up;
		}
	} else {
		m0_rpc_item_change_state(item, M0_RPC_ITEM_URGENT);
	}
	M0_LEAVE("nr_items: %llu bytes: %llu",
			(unsigned long long)frm->f_nr_items,
			(unsigned long long)frm->f_nr_bytes_accumulated);
}

M0_INTERNAL bool item_is_in_waiting_queue(const struct m0_rpc_item *item,
					  const struct m0_rpc_frm *frm)
{
	return item->ri_itemq == &frm->f_itemq[FRMQ_WAITING];
}

/**
   Depending on item->ri_deadline and item->ri_prio returns one of
   enum m0_rpc_frm_itemq_type in which the item should be placed.
 */
static enum m0_rpc_frm_itemq_type
frm_which_qtype(struct m0_rpc_frm *frm, const struct m0_rpc_item *item)
{
	enum m0_rpc_frm_itemq_type qtype;
	bool                       deadline_passed;

	M0_ENTRY("item: %p", item);
	M0_PRE(item != NULL);

	deadline_passed = m0_time_now() >= item->ri_deadline;

	M0_LOG(M0_DEBUG,
	       "deadline: "TIME_F" deadline_passed: %s",
	       TIME_P(item->ri_deadline),
	       m0_bool_to_str(deadline_passed));

	qtype = deadline_passed ? FRMQ_URGENT : FRMQ_WAITING;
	M0_LEAVE("qtype: %s", str_qtype[qtype]);
	return qtype;
}

/**
   q is sorted by m0_rpc_item::ri_prio and then by m0_rpc_item::ri_deadline.

   Insert new_item such that the ordering of q is maintained.
 */
static void __itemq_insert(struct m0_tl *q, struct m0_rpc_item *new_item)
{
	struct m0_rpc_item *item;

	M0_ENTRY();

	/* insertion sort. */
	m0_tl_for(itemq, q, item) {
		if (!item_less_or_equal(item, new_item)) {
			itemq_tlist_add_before(item, new_item);
			break;
		}
	} m0_tl_endfor;
	if (item == NULL)
		itemq_tlist_add_tail(q, new_item);
	new_item->ri_itemq = q;

	M0_ASSERT_EX(itemq_invariant(q));
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_frm_item_deadline_passed(struct m0_rpc_frm *frm,
						 struct m0_rpc_item *item)
{
	M0_ENTRY("frm: %p item: %p", frm, item);

	item_move_to_urgent_queue(frm, item);
	frm_balance(frm);

	M0_LEAVE();
}

static void item_move_to_urgent_queue(struct m0_rpc_frm *frm,
				      struct m0_rpc_item *item)
{
	M0_PRE(item != NULL);

	__itemq_remove(item);
	__itemq_insert(&frm->f_itemq[FRMQ_URGENT], item);
}

/**
   Core of formation algorithm.

   @pre frm_rmachine_is_locked(frm)
 */
static void frm_balance(struct m0_rpc_frm *frm)
{
	struct m0_rpc_packet *p;
	int                   packet_count;
	int                   item_count;
	int                   rc;

	M0_ENTRY("frm: %p", frm);

	M0_PRE(frm_rmachine_is_locked(frm));
	M0_PRE_EX(frm_invariant(frm));

	M0_LOG(M0_DEBUG, "ready: %s",
	       (char *)m0_bool_to_str(frm_is_ready(frm)));
	packet_count = item_count = 0;

	if (M0_FI_ENABLED("do_nothing"))
		return;

	while (frm_is_ready(frm)) {
		M0_ALLOC_PTR(p);
		if (p == NULL) {
			M0_LOG(M0_ERROR, "Error: packet allocation failed");
			break;
		}
		m0_rpc_packet_init(p, frm_rmachine(frm));
		frm_fill_packet(frm, p);
		if (m0_rpc_packet_is_empty(p)) {
			/* See FRM_BALANCE_NOTE_1 at the end of this function */
			m0_rpc_packet_fini(p);
			m0_free(p);
			break;
		}
		++packet_count;
		item_count += p->rp_ow.poh_nr_items;
		rc = frm_packet_ready(frm, p);
		if (rc == 0) {
			++frm->f_nr_packets_enqed;
			/*
			 * f_nr_packets_enqed will be decremented in packet
			 * done callback, see m0_rpc_frm_packet_done()
			 */
			if (frm->f_state == FRM_IDLE)
				frm->f_state = FRM_BUSY;
		}
	}

	M0_POST_EX(frm_invariant(frm));
	M0_LEAVE("formed %d packet(s) [%d items]", packet_count, item_count);
}
/*
 * FRM_BALANCE_NOTE_1
 * This case can arise if:
 * - Accumulated bytes are >= max_nr_bytes_accumulated,
 *   hence frm is READY
 */

/**
   Is frm ready to form a packet?

   It is possible that frm_is_ready() returns true but no packet could
   be formed. See FRM_BALANCE_NOTE_1
 */
static bool frm_is_ready(const struct m0_rpc_frm *frm)
{
	const struct m0_rpc_frm_constraints *c;
	bool                                 has_urgent_items;

	M0_PRE(frm != NULL);

	if (M0_FI_ENABLED("ready"))
		return true;
	has_urgent_items =
		!itemq_tlist_is_empty(&frm->f_itemq[FRMQ_URGENT]);

	c = &frm->f_constraints;
	return frm->f_nr_packets_enqed < c->fc_max_nr_packets_enqed &&
	       (has_urgent_items ||
		frm->f_nr_bytes_accumulated >= c->fc_max_nr_bytes_accumulated);
}

/**
   Adds RPC items in packet p, taking the constraints into account.

   An item is removed from itemq, once it is added to packet.
 */
static void frm_fill_packet(struct m0_rpc_frm *frm, struct m0_rpc_packet *p)
{
	struct m0_rpc_item *item;
	struct m0_tl       *q;
	m0_bcount_t         limit;

	M0_ENTRY("frm: %p packet: %p", frm, p);

	M0_ASSERT_EX(frm_invariant(frm));

	for_each_itemq_in_frm(q, frm) {
		m0_tl_for(itemq, q, item) {
			/* See FRM_FILL_PACKET_NOTE_1 at the end of this func */
			if (available_space_in_packet(p, frm) == 0)
				goto out;
			if (item_will_exceed_packet_size(item, p, frm))
				continue;
			if (item->ri_sm.sm_state == M0_RPC_ITEM_FAILED) {
				/*
				 * Request might have been cancelled while in
				 * URGENT state
				 */
				M0_ASSERT(m0_rpc_item_is_request(item));
				frm_remove(frm, item);
				continue;
			}
			if (M0_FI_ENABLED("skip_oneway_items") &&
			    m0_rpc_item_is_oneway(item))
				continue;
			m0_rpc_item_get(item);
			frm_remove(frm, item);
			if (item_supports_merging(item)) {
				limit = available_space_in_packet(p, frm);
				frm_try_merging_item(frm, item, limit);
			}
			M0_ASSERT(!item_will_exceed_packet_size(item, p, frm));
			m0_rpc_packet_add_item(p, item);
			m0_rpc_item_change_state(item, M0_RPC_ITEM_SENDING);
			m0_rpc_item_put(item);
		} m0_tl_endfor;
	}
	frm_fill_packet_from_item_sources(frm, p);
out:
	M0_ASSERT_EX(frm_invariant(frm));
	M0_LEAVE();
}
/*
 * FRM_FILL_PACKET_NOTE_1
 * I know that this loop is inefficient. But for now
 * let's just stick to simplicity. We can optimize it
 * later if need arises. --Amit
 */

static m0_bcount_t available_space_in_packet(const struct m0_rpc_packet *p,
					     const struct m0_rpc_frm    *frm)
{
	M0_PRE(p->rp_size <= frm->f_constraints.fc_max_packet_size);
	return frm->f_constraints.fc_max_packet_size - p->rp_size;
}

static bool item_will_exceed_packet_size(struct m0_rpc_item         *item,
					 const struct m0_rpc_packet *p,
					 const struct m0_rpc_frm    *frm)
{
	return m0_rpc_item_size(item) > available_space_in_packet(p, frm);
}

static bool item_supports_merging(const struct m0_rpc_item *item)
{
	M0_PRE(item->ri_type != NULL &&
	       item->ri_type->rit_ops != NULL);

	return item->ri_type->rit_ops->rito_try_merge != NULL;
}

static void frm_fill_packet_from_item_sources(struct m0_rpc_frm    *frm,
					      struct m0_rpc_packet *p)
{
	struct m0_rpc_machine     *machine = frm_rmachine(frm);
	struct m0_rpc_conn        *conn;
	struct m0_rpc_item_source *source;
	struct m0_rpc_item        *item;
	m0_bcount_t                available_space;
	m0_bcount_t                header_footer_size;

	M0_ENTRY();

	header_footer_size = m0_rpc_item_onwire_header_size +
			     m0_rpc_item_onwire_footer_size;
	m0_tl_for(rpc_conn, &machine->rm_outgoing_conns, conn) {
		M0_LOG(M0_DEBUG, "conn: %p", conn);
		if (&conn->c_rpcchan->rc_frm != frm ||
		    conn_state(conn) != M0_RPC_CONN_ACTIVE)
			continue;
		m0_tl_for(item_source, &conn->c_item_sources, source) {
			M0_LOG(M0_DEBUG, "source: %p", source);
			while (source->ris_ops->riso_has_item(source)) {
				available_space = available_space_in_packet(p,
									frm);
				if (available_space <= header_footer_size)
					goto out;
				item = source->ris_ops->riso_get_item(source,
						available_space - header_footer_size);
				if (item == NULL)
					break; /* next item source */
				M0_ASSERT(m0_rpc_item_is_oneway(item));
				/*
				 * Rpc always acquires an *internal* reference
				 * to "all" items (Here sourced one-way items).
				 * This reference is released when the item is
				 * sent.
				 */
				m0_rpc_item_get(item);
				item->ri_rmachine = frm_rmachine(frm);
				item->ri_nr_sent++;
				m0_rpc_item_sm_init(item, M0_RPC_ITEM_OUTGOING);
				M0_LOG(M0_DEBUG, "item: %p", item);
				M0_ASSERT(!item_will_exceed_packet_size(item,
								p, frm));
				m0_rpc_packet_add_item(p, item);
				m0_rpc_item_change_state(item,
							 M0_RPC_ITEM_SENDING);
				m0_rpc_item_put(item);
			}
		} m0_tl_endfor;
	} m0_tl_endfor;

out:
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_frm_remove_item(struct m0_rpc_frm  *frm,
					struct m0_rpc_item *item)
{
	frm_remove(frm, item);
}

static void frm_remove(struct m0_rpc_frm *frm, struct m0_rpc_item *item)
{
	M0_ENTRY("frm: %p item: %p", frm, item);
	M0_PRE(frm != NULL && item != NULL);
	M0_PRE(frm->f_nr_items > 0 && item->ri_itemq != NULL);

	__itemq_remove(item);
	item->ri_frm = NULL;
	M0_CNT_DEC(frm->f_nr_items);
	frm->f_nr_bytes_accumulated -= m0_rpc_item_size(item);
	M0_ASSERT(frm->f_nr_bytes_accumulated >= 0);

	if (frm_is_idle(frm))
		frm->f_state = FRM_IDLE;
	m0_rpc_item_put(item);
	M0_LEAVE();
}

static void __itemq_remove(struct m0_rpc_item *item)
{
	itemq_tlink_del_fini(item);
	item->ri_itemq = NULL;
}

static void frm_try_merging_item(struct m0_rpc_frm  *frm,
				 struct m0_rpc_item *item,
				 m0_bcount_t         limit)
{
	M0_ENTRY("frm: %p item: %p limit: %llu", frm, item,
						 (unsigned long long)limit);
	/** @todo XXX implement item merging */
	M0_LEAVE();
	return;
}

/**
   @see m0_rpc_frm_ops::fo_packet_ready()
 */
static int frm_packet_ready(struct m0_rpc_frm *frm, struct m0_rpc_packet *p)
{
	M0_ENTRY("frm: %p packet %p", frm, p);

	M0_PRE(frm != NULL && p != NULL && !m0_rpc_packet_is_empty(p));
	M0_PRE(frm->f_ops != NULL && frm->f_ops->fo_packet_ready != NULL);
	M0_LOG(M0_DEBUG, "nr_items: %llu",
	       (unsigned long long)p->rp_ow.poh_nr_items);

	p->rp_frm = frm;
	/* See packet_ready() in rpc/frmops.c */
	return M0_RC(frm->f_ops->fo_packet_ready(p));
}

M0_INTERNAL void m0_rpc_frm_run_formation(struct m0_rpc_frm *frm)
{
	if (M0_FI_ENABLED("do_nothing"))
		return;

	M0_ENTRY("frm: %p", frm);
	M0_ASSERT_EX(frm_invariant(frm));
	M0_PRE(frm_rmachine_is_locked(frm));

	frm_balance(frm);

	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_frm_packet_done(struct m0_rpc_packet *p)
{
	struct m0_rpc_frm *frm;

	M0_ENTRY("packet: %p", p);
	M0_ASSERT_EX(m0_rpc_packet_invariant(p));

	frm = p->rp_frm;
	M0_ASSERT_EX(frm_invariant(frm));
	M0_PRE(frm_rmachine_is_locked(frm));

	M0_CNT_DEC(frm->f_nr_packets_enqed);
	M0_LOG(M0_DEBUG, "nr_packets_enqed: %llu",
		(unsigned long long)frm->f_nr_packets_enqed);

	if (frm_is_idle(frm))
		frm->f_state = FRM_IDLE;

	frm_balance(frm);

	M0_LEAVE();
}

M0_INTERNAL struct m0_rpc_frm *session_frm(const struct m0_rpc_session *s)
{
	return &s->s_conn->c_rpcchan->rc_frm;
}

#undef M0_TRACE_SUBSYSTEM

/** @} */
