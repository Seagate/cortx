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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 *		    Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 06/27/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/tlist.h"
#include "lib/rwlock.h"
#include "lib/misc.h"
#include "lib/errno.h"
#include "lib/finject.h"
#include "conf/obj.h"  /* m0_conf_fid_type */
#include "ha/epoch.h"
#include "ha/note.h"
#include "mero/magic.h"
#include "addb2/addb2.h"
#include "rpc/addb2.h"
#include "rpc/rpc_internal.h"

/**
   @addtogroup rpc

   @{
 */

static int item_entered_in_urgent_state(struct m0_sm *mach);
static void item_timer_cb(struct m0_sm_timer *timer);
static void item_timedout(struct m0_rpc_item *item);
static void item_resend(struct m0_rpc_item *item);
static int item_reply_received(struct m0_rpc_item *reply,
			       struct m0_rpc_item **req_out);
static bool item_reply_received_fi(struct m0_rpc_item *req,
				   struct m0_rpc_item *reply);
static int req_replied(struct m0_rpc_item *req, struct m0_rpc_item *reply);

const struct m0_sm_conf outgoing_item_sm_conf;
const struct m0_sm_conf incoming_item_sm_conf;

M0_TL_DESCR_DEFINE(rpcitem, "rpc item tlist", M0_INTERNAL, struct m0_rpc_item,
		   ri_field, ri_magic, M0_RPC_ITEM_MAGIC,
		   M0_RPC_ITEM_HEAD_MAGIC);
M0_TL_DEFINE(rpcitem, M0_INTERNAL, struct m0_rpc_item);

M0_TL_DESCR_DEFINE(rit, "rpc_item_type_descr", static, struct m0_rpc_item_type,
		   rit_linkage, rit_magic, M0_RPC_ITEM_TYPE_MAGIC,
		   M0_RPC_ITEM_TYPE_HEAD_MAGIC);
M0_TL_DEFINE(rit, static, struct m0_rpc_item_type);

M0_TL_DESCR_DEFINE(ric, "rpc item cache", M0_INTERNAL,
		   struct m0_rpc_item, ri_cache_link, ri_magic,
		   M0_RPC_ITEM_MAGIC, M0_RPC_ITEM_CACHE_HEAD_MAGIC);
M0_TL_DEFINE(ric, M0_INTERNAL, struct m0_rpc_item);

M0_TL_DESCR_DEFINE(pending_item, "pending-item-list", M0_INTERNAL,
		   struct m0_rpc_item, ri_pending_link, ri_magic,
		   M0_RPC_ITEM_MAGIC, M0_RPC_ITEM_PENDING_CACHE_HEAD_MAGIC);
M0_TL_DEFINE(pending_item, M0_INTERNAL, struct m0_rpc_item);

/** Global rpc item types list. */
static struct m0_tl        rpc_item_types_list;
static struct m0_rwlock    rpc_item_types_lock;

/**
  Checks if the supplied opcode has already been registered.
  @param opcode RPC item type opcode.
  @retval true if opcode is a duplicate(already registered)
  @retval false if opcode has not been registered yet.
*/
static bool opcode_is_dup(uint32_t opcode)
{
	M0_PRE(opcode > 0);

	return m0_rpc_item_type_lookup(opcode) != NULL;
}

M0_INTERNAL m0_bcount_t m0_rpc_item_onwire_header_size;
M0_INTERNAL m0_bcount_t m0_rpc_item_onwire_footer_size;

#define HEADER1_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_header1_xc, ptr)
#define HEADER2_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_header2_xc, ptr)
#define FOOTER_XCODE_OBJ(ptr)  M0_XCODE_OBJ(m0_rpc_item_footer_xc,  ptr)

M0_INTERNAL int m0_rpc_item_module_init(void)
{
	struct m0_rpc_item_header1 h1;
	struct m0_xcode_ctx        h1_xc;
	struct m0_rpc_item_header2 h2;
	struct m0_xcode_ctx        h2_xc;
	struct m0_rpc_item_footer  f;
	struct m0_xcode_ctx        f_xc;

	M0_ENTRY();

	m0_rwlock_init(&rpc_item_types_lock);
	rit_tlist_init(&rpc_item_types_list);

	m0_xcode_ctx_init(&h1_xc, &HEADER1_XCODE_OBJ(&h1));
	m0_xcode_ctx_init(&h2_xc, &HEADER2_XCODE_OBJ(&h2));
	m0_xcode_ctx_init(&f_xc,  &FOOTER_XCODE_OBJ(&f));
	m0_rpc_item_onwire_header_size = m0_xcode_length(&h1_xc) +
					 m0_xcode_length(&h2_xc);
	m0_rpc_item_onwire_footer_size = m0_xcode_length(&f_xc);

	return M0_RC(0);
}

M0_INTERNAL void m0_rpc_item_module_fini(void)
{
	struct m0_rpc_item_type		*item_type;

	M0_ENTRY();

	m0_rwlock_write_lock(&rpc_item_types_lock);
	m0_tl_for(rit, &rpc_item_types_list, item_type) {
		rit_tlink_del_fini(item_type);
	} m0_tl_endfor;
	rit_tlist_fini(&rpc_item_types_list);
	m0_rwlock_write_unlock(&rpc_item_types_lock);
	m0_rwlock_fini(&rpc_item_types_lock);

	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_item_type_register(struct m0_rpc_item_type *item_type)
{
	uint64_t dir_flag;
	M0_ENTRY("item_type: %p, item_opcode: %u", item_type,
		 item_type->rit_opcode);
	M0_PRE(item_type != NULL);
	dir_flag = item_type->rit_flags & (M0_RPC_ITEM_TYPE_REQUEST |
		   M0_RPC_ITEM_TYPE_REPLY | M0_RPC_ITEM_TYPE_ONEWAY);
	M0_PRE(!opcode_is_dup(item_type->rit_opcode));
	M0_PRE(m0_is_po2(dir_flag));
	M0_PRE(ergo(item_type->rit_flags & M0_RPC_ITEM_TYPE_MUTABO,
		    dir_flag == M0_RPC_ITEM_TYPE_REQUEST));

	item_type->rit_outgoing_conf = outgoing_item_sm_conf;
	item_type->rit_incoming_conf = incoming_item_sm_conf;
	m0_rwlock_write_lock(&rpc_item_types_lock);
	rit_tlink_init_at(item_type, &rpc_item_types_list);
	m0_rwlock_write_unlock(&rpc_item_types_lock);

	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_item_type_deregister(struct m0_rpc_item_type *item_type)
{
	M0_ENTRY("item_type: %p", item_type);
	M0_PRE(item_type != NULL);

	m0_rwlock_write_lock(&rpc_item_types_lock);
	rit_tlink_del_fini(item_type);
	item_type->rit_magic = 0;
	m0_rwlock_write_unlock(&rpc_item_types_lock);

	M0_LEAVE();
}

M0_INTERNAL struct m0_rpc_item_type *m0_rpc_item_type_lookup(uint32_t opcode)
{
	struct m0_rpc_item_type *item_type;

	M0_ENTRY("opcode: %u", opcode);

	m0_rwlock_read_lock(&rpc_item_types_lock);
	item_type = m0_tl_find(rit, item_type, &rpc_item_types_list,
			       item_type->rit_opcode == opcode);
	m0_rwlock_read_unlock(&rpc_item_types_lock);

	M0_POST(ergo(item_type != NULL, item_type->rit_opcode == opcode));
	M0_LEAVE("item_type: %p", item_type);
	return item_type;
}

static struct m0_sm_state_descr outgoing_item_states[] = {
	[M0_RPC_ITEM_UNINITIALISED] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "UNINITIALISED",
		.sd_allowed = 0,
	},
	[M0_RPC_ITEM_INITIALISED] = {
		.sd_flags   = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name    = "INITIALISED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_ENQUEUED,
				      M0_RPC_ITEM_URGENT,
				      M0_RPC_ITEM_SENDING,
				      M0_RPC_ITEM_FAILED,
				      M0_RPC_ITEM_UNINITIALISED),
	},
	[M0_RPC_ITEM_ENQUEUED] = {
		.sd_name    = "ENQUEUED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_SENDING,
				      M0_RPC_ITEM_URGENT,
				      M0_RPC_ITEM_FAILED,
				      M0_RPC_ITEM_REPLIED),
	},
	[M0_RPC_ITEM_URGENT] = {
		.sd_name    = "URGENT",
		.sd_in      = item_entered_in_urgent_state,
		.sd_allowed = M0_BITS(M0_RPC_ITEM_SENDING,
				      M0_RPC_ITEM_FAILED,
				      M0_RPC_ITEM_REPLIED),
	},
	[M0_RPC_ITEM_SENDING] = {
		.sd_name    = "SENDING",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_SENT, M0_RPC_ITEM_FAILED),
	},
	[M0_RPC_ITEM_SENT] = {
		.sd_flags   = M0_SDF_FINAL,
		.sd_name    = "SENT",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_WAITING_FOR_REPLY,
				      M0_RPC_ITEM_ENQUEUED,/*only reply items*/
				      M0_RPC_ITEM_URGENT,
				      M0_RPC_ITEM_UNINITIALISED,
				      M0_RPC_ITEM_FAILED),
	},
	[M0_RPC_ITEM_WAITING_FOR_REPLY] = {
		.sd_name    = "WAITING_FOR_REPLY",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_REPLIED,
				      M0_RPC_ITEM_FAILED,
				      M0_RPC_ITEM_ENQUEUED,
				      M0_RPC_ITEM_URGENT),
	},
	[M0_RPC_ITEM_REPLIED] = {
		.sd_flags   = M0_SDF_FINAL,
		.sd_name    = "REPLIED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_UNINITIALISED,
				      M0_RPC_ITEM_ENQUEUED,
				      M0_RPC_ITEM_URGENT,
				      M0_RPC_ITEM_FAILED),
	},
	[M0_RPC_ITEM_FAILED] = {
		.sd_flags   = M0_SDF_FINAL,
		.sd_name    = "FAILED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_URGENT, /* resend */
				      M0_RPC_ITEM_UNINITIALISED),
	},
};

const struct m0_sm_conf outgoing_item_sm_conf = {
	.scf_name      = "Outgoing-RPC-Item-sm",
	.scf_nr_states = ARRAY_SIZE(outgoing_item_states),
	.scf_state     = outgoing_item_states,
};

static struct m0_sm_state_descr incoming_item_states[] = {
	[M0_RPC_ITEM_UNINITIALISED] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "UNINITIALISED",
		.sd_allowed = 0,
	},
	[M0_RPC_ITEM_INITIALISED] = {
		.sd_flags   = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name    = "INITIALISED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_ACCEPTED,
				      M0_RPC_ITEM_FAILED,
				      M0_RPC_ITEM_UNINITIALISED),
	},
	[M0_RPC_ITEM_ACCEPTED] = {
		.sd_flags   = M0_SDF_FINAL,
		.sd_name    = "ACCEPTED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_REPLIED,
				      M0_RPC_ITEM_FAILED,
				      M0_RPC_ITEM_UNINITIALISED),
	},
	[M0_RPC_ITEM_REPLIED] = {
		.sd_flags   = M0_SDF_FINAL,
		.sd_name    = "REPLIED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_UNINITIALISED),
	},
	[M0_RPC_ITEM_FAILED] = {
		.sd_flags   = M0_SDF_FINAL,
		.sd_name    = "FAILED",
		.sd_allowed = M0_BITS(M0_RPC_ITEM_UNINITIALISED),
	},
};

const struct m0_sm_conf incoming_item_sm_conf = {
	.scf_name      = "Incoming-RPC-Item-sm",
	.scf_nr_states = ARRAY_SIZE(incoming_item_states),
	.scf_state     = incoming_item_states,
};

M0_INTERNAL bool m0_rpc_item_invariant(const struct m0_rpc_item *item)
{
	int  state;
	bool req;
	bool rply;
	bool oneway;

	if (item == NULL || item->ri_type == NULL)
		return false;

	state  = item->ri_sm.sm_state;
	req    = m0_rpc_item_is_request(item);
	rply   = m0_rpc_item_is_reply(item);
	oneway = m0_rpc_item_is_oneway(item);

	return  item->ri_magic == M0_RPC_ITEM_MAGIC &&
		item->ri_prio >= M0_RPC_ITEM_PRIO_MIN &&
		item->ri_prio <= M0_RPC_ITEM_PRIO_MAX &&
		(req + rply + oneway == 1) && /* only one of three is true */
		equi(req || rply, item->ri_session != NULL) &&

		equi(state == M0_RPC_ITEM_FAILED, item->ri_error != 0) &&

		ergo(item->ri_reply != NULL,
			req &&
			M0_IN(state, (M0_RPC_ITEM_SENDING,
				      M0_RPC_ITEM_WAITING_FOR_REPLY,
				      M0_RPC_ITEM_REPLIED))) &&

		equi(itemq_tlink_is_in(item), state == M0_RPC_ITEM_ENQUEUED) &&
		equi(item->ri_itemq != NULL,  state == M0_RPC_ITEM_ENQUEUED) &&

		equi(packet_item_tlink_is_in(item),
		     state == M0_RPC_ITEM_SENDING);

}

M0_INTERNAL const char *item_state_name(const struct m0_rpc_item *item)
{
	return m0_sm_state_name(&item->ri_sm, item->ri_sm.sm_state);
}

M0_INTERNAL const char *item_kind(const struct m0_rpc_item *item)
{
	return  m0_rpc_item_is_request(item) ? "REQUEST" :
		m0_rpc_item_is_reply(item)   ? "REPLY"   :
		m0_rpc_item_is_oneway(item)  ? "ONEWAY"  : "INVALID_KIND";
}

void m0_rpc_item_init(struct m0_rpc_item *item,
		      const struct m0_rpc_item_type *itype)
{
	M0_ENTRY("%p[%u]", item, itype->rit_opcode);
	M0_PRE(item != NULL && itype != NULL);
	M0_PRE(M0_IS0(item));

	item->ri_type  = itype;
	item->ri_magic = M0_RPC_ITEM_MAGIC;
	item->ri_ha_epoch = M0_HA_EPOCH_NONE;

	item->ri_resend_interval = m0_time(M0_RPC_ITEM_RESEND_INTERVAL, 0);
	item->ri_nr_sent_max     = ~(uint64_t)0;

	packet_item_tlink_init(item);
	itemq_tlink_init(item);
        rpcitem_tlink_init(item);
	rpcitem_tlist_init(&item->ri_compound_items);
	ric_tlink_init(item);
	pending_item_tlink_init(item);
	m0_sm_timeout_init(&item->ri_deadline_timeout);
	m0_sm_timer_init(&item->ri_timer);
	/* item->ri_sm will be initialised when the item is posted */
	M0_LEAVE();
}
M0_EXPORTED(m0_rpc_item_init);

void m0_rpc_item_fini(struct m0_rpc_item *item)
{
	M0_ENTRY("%p[%s/%u], rc %d", item, item_kind(item),
		 item->ri_type->rit_opcode, item->ri_error);

	/*
	 * Reset cookie so that a finalised item is not matched
	 * when a late reply arrives.
	 */
	item->ri_cookid = 0;

	m0_sm_timer_fini(&item->ri_timer);
	m0_sm_timeout_fini(&item->ri_deadline_timeout);

	if (item->ri_sm.sm_state > M0_RPC_ITEM_UNINITIALISED)
		m0_rpc_item_sm_fini(item);

	if (item->ri_reply != NULL) {
		m0_rpc_item_put(item->ri_reply);
		item->ri_reply = NULL;
	}
	if (itemq_tlink_is_in(item))
		m0_rpc_frm_remove_item(item->ri_frm, item);

	M0_ASSERT(!ric_tlink_is_in(item));
	M0_ASSERT(!itemq_tlink_is_in(item));
	M0_ASSERT(!packet_item_tlink_is_in(item));
	M0_ASSERT(!rpcitem_tlink_is_in(item));
	M0_ASSERT(!pending_item_tlink_is_in(item));
	ric_tlink_fini(item);
	itemq_tlink_fini(item);
	packet_item_tlink_fini(item);
	rpcitem_tlink_fini(item);
	rpcitem_tlist_fini(&item->ri_compound_items);
	pending_item_tlink_fini(item);
	M0_LEAVE();
}
M0_EXPORTED(m0_rpc_item_fini);

void m0_rpc_item_get(struct m0_rpc_item *item)
{
	M0_PRE(item != NULL && item->ri_type != NULL &&
	       item->ri_type->rit_ops != NULL &&
	       item->ri_type->rit_ops->rito_item_get != NULL);

	item->ri_type->rit_ops->rito_item_get(item);
}

void m0_rpc_item_put(struct m0_rpc_item *item)
{
	M0_PRE(item != NULL && item->ri_type != NULL &&
	       item->ri_type->rit_ops != NULL &&
	       item->ri_type->rit_ops->rito_item_put != NULL &&
	       item->ri_rmachine != NULL);
	M0_PRE(m0_rpc_machine_is_locked(item->ri_rmachine));

	item->ri_type->rit_ops->rito_item_put(item);
}

void m0_rpc_item_put_lock(struct m0_rpc_item *item)
{
	struct m0_rpc_machine *rmach;

	M0_PRE(item->ri_rmachine != NULL);
	/*
	 * must store rpc machine pointer on stack before rpc item is put, and
	 * has the pointer corrupted as the result
	 */
	rmach = item->ri_rmachine;

	m0_rpc_machine_lock(rmach);
	m0_rpc_item_put(item);
	m0_rpc_machine_unlock(rmach);
}

m0_bcount_t m0_rpc_item_size(struct m0_rpc_item *item)
{
	if (item->ri_size == 0)
		item->ri_size = m0_rpc_item_onwire_header_size +
				m0_rpc_item_payload_size(item) +
				m0_rpc_item_onwire_footer_size;
	M0_ASSERT(item->ri_size != 0);
	return item->ri_size;
}

m0_bcount_t m0_rpc_item_payload_size(struct m0_rpc_item *item)
{
	M0_PRE(item->ri_type != NULL &&
	       item->ri_type->rit_ops != NULL &&
	       item->ri_type->rit_ops->rito_payload_size != NULL);

	return item->ri_type->rit_ops->rito_payload_size(item);
}

M0_INTERNAL
bool m0_rpc_item_max_payload_exceeded(struct m0_rpc_item    *item,
				      struct m0_rpc_session *session)
{
	M0_PRE(item != NULL);
	M0_PRE(session != NULL);

	if (M0_FI_ENABLED("payload_too_large1") ||
	    M0_FI_ENABLED("payload_too_large2"))
		return true;

	return (m0_rpc_item_payload_size(item) >
		m0_rpc_session_get_max_item_payload_size(session));
}

M0_INTERNAL bool m0_rpc_item_is_update(const struct m0_rpc_item *item)
{
	return (item->ri_type->rit_flags & M0_RPC_ITEM_TYPE_MUTABO) != 0;
}

M0_INTERNAL bool m0_rpc_item_is_request(const struct m0_rpc_item *item)
{
	M0_PRE(item != NULL && item->ri_type != NULL);

	return (item->ri_type->rit_flags & M0_RPC_ITEM_TYPE_REQUEST) != 0;
}

M0_INTERNAL bool m0_rpc_item_is_reply(const struct m0_rpc_item *item)
{
	M0_PRE(item != NULL && item->ri_type != NULL);

	return (item->ri_type->rit_flags & M0_RPC_ITEM_TYPE_REPLY) != 0;
}

M0_INTERNAL bool m0_rpc_item_is_oneway(const struct m0_rpc_item *item)
{
	M0_PRE(item != NULL);
	M0_PRE(item->ri_type != NULL);

	return (item->ri_type->rit_flags & M0_RPC_ITEM_TYPE_ONEWAY) != 0;
}

static bool rpc_item_needs_xid(const struct m0_rpc_item *item)
{
	return !m0_rpc_item_is_oneway(item) &&
	       !M0_IN(item->ri_type->rit_opcode,
		      (M0_RPC_CONN_ESTABLISH_OPCODE,
		       M0_RPC_CONN_ESTABLISH_REP_OPCODE,
		       M0_RPC_CONN_TERMINATE_OPCODE,
		       M0_RPC_CONN_TERMINATE_REP_OPCODE));
}

M0_INTERNAL void m0_rpc_item_xid_assign(struct m0_rpc_item *item)
{
	M0_PRE(m0_rpc_machine_is_locked(item->ri_rmachine));

	M0_ENTRY("item: %p nr_sent:%d [%s/%u] xid=%"PRIu64,
			item, item->ri_nr_sent, item_kind(item),
			item->ri_type->rit_opcode, item->ri_header.osr_xid);
	/*
	 * xid needs to be assigned only once.
	 * At this point ri_nr_sent is already incremented.
	 *
	 * xid for reply is not changed. It is already set to the request xid.
	 */
	if (item->ri_nr_sent == 1 && !m0_rpc_item_is_reply(item)) {
		item->ri_header.osr_xid = rpc_item_needs_xid(item) ?
					  ++item->ri_session->s_xid :
					  UINT64_MAX;
		M0_LOG(M0_DEBUG, "%p[%u] set item xid=%"PRIu64" "
		       "s_xid=%"PRIu64, item, item->ri_type->rit_opcode,
		       item->ri_header.osr_xid,
		       item->ri_session == NULL ? UINT64_MAX :
				item->ri_session->s_xid);
	}
	M0_LEAVE();
}

/**
 * Returns either item should be handled or not.
 * Decision is based on the item xid.
 * For duplicate request, the cached reply (if any)
 * will be resent again.
 * Purges the stale items from reply cache also.
 */
M0_INTERNAL bool m0_rpc_item_xid_check(struct m0_rpc_item *item,
				       struct m0_rpc_item **next)
{
	struct m0_rpc_session *sess = item->ri_session;
	uint64_t               xid  = item->ri_header.osr_xid;
	struct m0_rpc_item    *cached;

	/* If item doesn't need xid then xid doesn't need to be checked */
	if (!rpc_item_needs_xid(item))
		return true;

	M0_LOG(M0_DEBUG, "item: %p [%s/%u] session %p xid=%"PRIu64
	       " s_xid=%"PRIu64, item, item_kind(item),
	       item->ri_type->rit_opcode, sess, xid, sess->s_xid);
	/*
	 * Purge cache on every N-th packet
	 * (not on every one - that could be pretty expensive).
	 */
	if ((xid & 0xff) == 0)
		m0_rpc_item_cache_purge(&sess->s_reply_cache);

	/*
	 * The new item which wasn't handled yet.
	 *
	 * XXX: Note, on a high loads, reply cache may grow huge.
	 * This can be optimized by implementing the sending
	 * of the last consequent received reply xid in every
	 * request from the client.
	 */
	if (xid == sess->s_xid + 1) { /* Normal case. */
		++sess->s_xid;
		if (next != NULL)
			*next = m0_rpc_item_cache_lookup(&sess->s_req_cache,
							 xid + 1);
		return M0_RC(true);
	} else if (m0_mod_gt(xid, sess->s_xid)) {
		/* Out-of-order case. Cache it for the future. */
		cached = m0_rpc_item_cache_lookup(&sess->s_req_cache, xid);
		if (cached == NULL)
			m0_rpc_item_cache_add(&sess->s_req_cache, item,
				m0_time_from_now(M0_RPC_ITEM_REQ_CACHE_TMO, 0));
		/*
		 * Misordered request without reply - drop it, now that it
		 * is present in the cache.
		 */
		M0_LOG(M0_NOTICE, "item: %p [%s/%u] session %p, misordered %"
		       PRIu64" != %"PRIu64, item, item_kind(item),
		       item->ri_type->rit_opcode, sess, xid, sess->s_xid + 1);
		return M0_RC(false);
	}

	/* Resend cached reply if it is available for the request. */
	cached = m0_rpc_item_cache_lookup(&sess->s_reply_cache, xid);
	if (cached != NULL) {
		M0_LOG(M0_DEBUG, "cached_reply=%p xid=%"PRIu64" state=%d",
		       cached, xid, cached->ri_sm.sm_state);
		if (M0_IN(cached->ri_sm.sm_state, (M0_RPC_ITEM_SENT,
						   M0_RPC_ITEM_FAILED))) {
			m0_rpc_session_hold_busy(sess);
			m0_rpc_item_change_state(item, M0_RPC_ITEM_ACCEPTED);
			if (cached->ri_error == -ENETDOWN)
				cached->ri_error = 0;
			m0_rpc_item_send_reply(item, cached);
		}
		return M0_RC(false);
	} else
		M0_LOG(M0_DEBUG, "item: %p [%s/%u] xid=%"PRIu64" s_xid=%"PRIu64
		       " No reply found", item, item_kind(item),
		       item->ri_type->rit_opcode, xid, sess->s_xid);

	return M0_RC(false);
}

M0_INTERNAL void m0_rpc_item_sm_init(struct m0_rpc_item *item,
				     enum m0_rpc_item_dir dir)
{
	const struct m0_sm_conf *conf;

	M0_PRE(item != NULL && item->ri_rmachine != NULL);

	conf = dir == M0_RPC_ITEM_OUTGOING ? &item->ri_type->rit_outgoing_conf :
					     &item->ri_type->rit_incoming_conf;

	M0_LOG(M0_DEBUG, "%p UNINITIALISED -> INITIALISED", item);
	m0_sm_init(&item->ri_sm, conf, M0_RPC_ITEM_INITIALISED,
		   &item->ri_rmachine->rm_sm_grp);
	m0_sm_addb2_counter_init(&item->ri_sm);
}

M0_INTERNAL void m0_rpc_item_sm_fini(struct m0_rpc_item *item)
{
	M0_PRE(item != NULL);

	m0_sm_fini(&item->ri_sm);
	item->ri_sm.sm_state = M0_RPC_ITEM_UNINITIALISED;
}

M0_INTERNAL void m0_rpc_item_change_state(struct m0_rpc_item *item,
					  enum m0_rpc_item_state state)
{
	M0_PRE(item != NULL);
	M0_PRE(m0_rpc_machine_is_locked(item->ri_rmachine));

	M0_LOG(M0_DEBUG, "%p[%s/%u] xid=%"PRIu64 ", %s -> %s, sm %s", item,
	       item_kind(item), item->ri_type->rit_opcode,
	       item->ri_header.osr_xid,
	       item_state_name(item), m0_sm_state_name(&item->ri_sm, state),
	       item->ri_sm.sm_conf->scf_name);

	m0_sm_state_set(&item->ri_sm, state);
}

M0_INTERNAL void m0_rpc_item_failed(struct m0_rpc_item *item, int32_t rc)
{
	M0_PRE(item != NULL && rc != 0);
	M0_PRE(item->ri_sm.sm_state != M0_RPC_ITEM_FAILED);
	M0_PRE(item->ri_sm.sm_state != M0_RPC_ITEM_UNINITIALISED);

	M0_ENTRY("FAILED %p[%s/%u], xid=%"PRIu64", state %s, session %p, "
		 "error %d", item, item_kind(item), item->ri_type->rit_opcode,
		 item->ri_header.osr_xid, item_state_name(item),
		 item->ri_session, rc);

	item->ri_rmachine->rm_stats.rs_nr_failed_items++;

	if (m0_rpc_item_is_request(item) &&
	    M0_IN(item->ri_sm.sm_state,
		  (M0_RPC_ITEM_ENQUEUED, M0_RPC_ITEM_URGENT,
		   M0_RPC_ITEM_SENDING, M0_RPC_ITEM_SENT,
		   M0_RPC_ITEM_WAITING_FOR_REPLY)))
		m0_rpc_item_pending_cache_del(item);

	/*
	 * Request and Reply items take hold on session until
	 * they are SENT/FAILED.
	 * See: m0_rpc__post_locked(), m0_rpc_reply_post()
	 *      m0_rpc_item_send()
	 */
	if (M0_IN(item->ri_sm.sm_state, (M0_RPC_ITEM_ENQUEUED,
					 M0_RPC_ITEM_URGENT,
					 M0_RPC_ITEM_SENDING)) &&
	   (m0_rpc_item_is_request(item) || m0_rpc_item_is_reply(item)))
		m0_rpc_session_release(item->ri_session);

	item->ri_error = rc;
	m0_rpc_item_change_state(item, M0_RPC_ITEM_FAILED);
	m0_rpc_item_timer_stop(item);
	/* XXX ->rio_sent() can be called multiple times (due to cancel). */
	if (m0_rpc_item_is_oneway(item) &&
	    item->ri_ops != NULL && item->ri_ops->rio_sent != NULL)
		item->ri_ops->rio_sent(item);

	m0_rpc_session_item_failed(item);
	/*
	 * Reference release done here is for the reference taken
	 * while submitting item to formation, using m0_rpc_frm_enq_item().
	 */
	m0_rpc_item_put(item);
	M0_LEAVE();
}

int m0_rpc_item_timedwait(struct m0_rpc_item *item,
			  uint64_t states, m0_time_t timeout)
{
	int rc;

	m0_rpc_machine_lock(item->ri_rmachine);
	rc = m0_sm_timedwait(&item->ri_sm, states, timeout);
	m0_rpc_machine_unlock(item->ri_rmachine);
	return M0_RC(rc);
}

int m0_rpc_item_wait_for_reply(struct m0_rpc_item *item, m0_time_t timeout)
{
	int rc;

	M0_PRE(m0_rpc_item_is_request(item));

	M0_LOG(M0_DEBUG, "%p[%s/%u] %s", item, item_kind(item),
	       item->ri_type->rit_opcode, item_state_name(item));
	rc = m0_rpc_item_timedwait(item, M0_BITS(M0_RPC_ITEM_REPLIED,
						 M0_RPC_ITEM_FAILED),
				   timeout);
	if (rc == 0 && item->ri_sm.sm_state == M0_RPC_ITEM_FAILED)
		rc = item->ri_error;

	M0_POST(ergo(rc == 0, item->ri_sm.sm_state == M0_RPC_ITEM_REPLIED));
	return M0_RC(rc);
}

static void item_cancel_fi(struct m0_rpc_item *item)
{
	uint32_t sm_state = item->ri_sm.sm_state;
	uint64_t ref = m0_ref_read(&(m0_rpc_item_to_fop(item)->f_ref));

	if (M0_FI_ENABLED("cancel_enqueued_item")) {
		M0_ASSERT(sm_state == M0_RPC_ITEM_ENQUEUED);
		M0_ASSERT(pending_item_tlink_is_in(item));
		M0_ASSERT(ref == 5);
	} else if (M0_FI_ENABLED("cancel_sending_item")) {
		M0_ASSERT(sm_state == M0_RPC_ITEM_SENDING);
		M0_ASSERT(pending_item_tlink_is_in(item));
		M0_ASSERT(ref == 5);
	} else if (M0_FI_ENABLED("cancel_waiting_for_reply_item")) {
		M0_ASSERT(sm_state == M0_RPC_ITEM_WAITING_FOR_REPLY);
		M0_ASSERT(pending_item_tlink_is_in(item));
		M0_ASSERT(ref == 3);
	} else if (M0_FI_ENABLED("cancel_replied_item")) {
		M0_ASSERT(sm_state == M0_RPC_ITEM_REPLIED &&
			  item->ri_reply->ri_sm.sm_state ==
			  M0_RPC_ITEM_ACCEPTED);
		M0_ASSERT(!pending_item_tlink_is_in(item));
		M0_ASSERT(ref == 1);
	}
}

void m0_rpc_item_cancel_nolock(struct m0_rpc_item *item)
{
	struct m0_rpc_session *session;

	M0_PRE(item != NULL);
	M0_PRE(item->ri_session != NULL);
	M0_PRE(m0_rpc_conn_is_snd(item2conn(item)));
	M0_PRE(m0_rpc_item_is_request(item));
	M0_ASSERT(m0_rpc_machine_is_locked(item->ri_rmachine));

	session = item->ri_session;

	M0_ENTRY("Item %p[%s/%u] session %p", item, item_kind(item),
		 item->ri_type->rit_opcode, session);

	item_cancel_fi(item);

	/*
	 * Note: The item which is requested to be cancelled, may have been
	 * released by the time the API was invoked. Hence, check if the item
	 * is still part of the pending_cache.
	 */
	if (!pending_item_tlink_is_in(item)) {
		M0_LOG(M0_DEBUG, "Item %p, not present in the pending item "
		       "cache", item);
		return;
	}

	M0_LOG(M0_DEBUG, "%p[%s/%u] item->ri_sm.sm_state %d, ri_error %d, "
	       "ref %llu", item, item_kind(item), item->ri_type->rit_opcode,
	       item->ri_sm.sm_state, item->ri_error,
	       (unsigned long long)m0_ref_read(
				&(m0_rpc_item_to_fop(item)->f_ref)));

	if (M0_IN(item->ri_sm.sm_state, (M0_RPC_ITEM_ENQUEUED,
					 M0_RPC_ITEM_URGENT,
					 M0_RPC_ITEM_SENDING))) {
		/*
		 * Release the reference taken either in m0_rpc_item_send() or
		 * in m0_rpc_oneway_item_post_locked().
		 */
		m0_rpc_item_put(item);
	}

	if (item->ri_sm.sm_state == M0_RPC_ITEM_ENQUEUED ||
	    item->ri_sm.sm_state == M0_RPC_ITEM_URGENT)
		m0_rpc_frm_remove_item(item->ri_frm, item);

	if (packet_item_tlink_is_in(item)) {
		M0_ASSERT(item->ri_sm.sm_state == M0_RPC_ITEM_SENDING ||
			  item->ri_sm.sm_state == M0_RPC_ITEM_SENT);
		m0_rpc_packet_remove_item(item->ri_packet, item);
	}

	M0_ASSERT(m0_ref_read(&(m0_rpc_item_to_fop(item)->f_ref)) >= 2);
	m0_rpc_item_failed(item, -ECANCELED);

	M0_POST(!itemq_tlink_is_in(item));
	M0_POST(!packet_item_tlink_is_in(item));
	M0_POST(!rpcitem_tlink_is_in(item));
	M0_POST(!pending_item_tlink_is_in(item));
	M0_LOG(M0_DEBUG, "%p[%u] session %p, item cancelled, ref %llu",
	       item, item->ri_type->rit_opcode, session,
	       (unsigned long long)m0_ref_read(
			&(m0_rpc_item_to_fop(item)->f_ref)));
}

void m0_rpc_item_cancel(struct m0_rpc_item *item)
{
	struct m0_rpc_machine *mach;

	M0_PRE(item != NULL);
	M0_PRE(item->ri_session != NULL);

	M0_ENTRY("item %p, session %p", item, item->ri_session);
	mach = item2conn(item)->c_rpc_machine;
	m0_rpc_machine_lock(mach);
	m0_rpc_item_cancel_nolock(item);
	m0_rpc_machine_unlock(mach);
	M0_LEAVE("item %p, session %p", item, item->ri_session);
}

void m0_rpc_item_cancel_init(struct m0_rpc_item *item)
{
	struct m0_rpc_machine         *mach;
	const struct m0_rpc_item_type *ri_type;

	M0_PRE(item != NULL);
	M0_PRE(item->ri_session != NULL);

	M0_ENTRY("item %p, session %p", item, item->ri_session);
	mach = item2conn(item)->c_rpc_machine;
	m0_rpc_machine_lock(mach);
	m0_rpc_item_cancel_nolock(item);
	/* Re-initialise item. User may want to re-post it. */
	ri_type = item->ri_type;
	item->ri_error = 0;
	m0_rpc_item_fini(item);
	M0_SET0(item);
	m0_rpc_item_init(item, ri_type);
	m0_rpc_machine_unlock(mach);
	M0_LOG(M0_DEBUG, "%p[%s/%u] item re-initialised, ref %llu",
	       item, item_kind(item), item->ri_type->rit_opcode,
	       (unsigned long long)m0_ref_read(
			&(m0_rpc_item_to_fop(item)->f_ref)));
	M0_LEAVE();
}

int32_t m0_rpc_item_error(const struct m0_rpc_item *item)
{
	return item->ri_error ?: (item->ri_reply == NULL ? 0 :
				  m0_rpc_item_generic_reply_rc(item->ri_reply));
}
M0_EXPORTED(m0_rpc_item_error);

static int item_entered_in_urgent_state(struct m0_sm *mach)
{
	struct m0_rpc_item *item = M0_AMB(item, mach, ri_sm);
	struct m0_rpc_frm  *frm = item->ri_frm;

	if (item_is_in_waiting_queue(item, frm)) {
		M0_LOG(M0_DEBUG, "%p [%s/%u] ENQUEUED -> URGENT",
		       item, item_kind(item), item->ri_type->rit_opcode);
		m0_rpc_frm_item_deadline_passed(frm, item);
		/*
		 * m0_rpc_frm_item_deadline_passed() might reenter in
		 * m0_sm_state_set() and modify item state.
		 * So at this point the item may or may not be in URGENT state.
		 */
	}
	return -1;
}

M0_INTERNAL int m0_rpc_item_timer_start(struct m0_rpc_item *item)
{
	M0_PRE(m0_rpc_item_is_request(item));

	if (M0_FI_ENABLED("failed")) {
		M0_LOG(M0_DEBUG, "item %p failed to start timer", item);
		return M0_ERR(-EINVAL);
	}
	if (item->ri_resend_interval == M0_TIME_NEVER)
		return 0;

	M0_LOG(M0_DEBUG, "item %p Starting timer", item);
	m0_sm_timer_fini(&item->ri_timer);
	m0_sm_timer_init(&item->ri_timer);
	return m0_sm_timer_start(&item->ri_timer, &item->ri_rmachine->rm_sm_grp,
				 item_timer_cb,
				 m0_time_add(m0_time_now(),
					     item->ri_resend_interval));
}

M0_INTERNAL void m0_rpc_item_timer_stop(struct m0_rpc_item *item)
{
	if (m0_sm_timer_is_armed(&item->ri_timer)) {
		M0_ASSERT(m0_rpc_item_is_request(item));
		M0_LOG(M0_DEBUG, "%p Stopping timer", item);
		m0_sm_timer_cancel(&item->ri_timer);
	}
}

static void item_timer_cb(struct m0_sm_timer *timer)
{
	struct m0_rpc_item *item;

	M0_ENTRY();
	M0_PRE(timer != NULL);

	item = container_of(timer, struct m0_rpc_item, ri_timer);
	M0_ASSERT(item->ri_magic == M0_RPC_ITEM_MAGIC);
	M0_ASSERT(m0_rpc_machine_is_locked(item->ri_rmachine));

	M0_LOG(M0_DEBUG, "%p [%s/%u] %s Timer elapsed.", item, item_kind(item),
	       item->ri_type->rit_opcode, item_state_name(item));

	if (item->ri_nr_sent >= item->ri_nr_sent_max)
		item_timedout(item);
	else
		item_resend(item);
}

static void item_timedout(struct m0_rpc_item *item)
{
	struct m0_rpc_conn *conn = item->ri_session->s_conn;

	M0_LOG(M0_DEBUG, "%p [%s/%u] %s TIMEDOUT.", item, item_kind(item),
	       item->ri_type->rit_opcode, item_state_name(item));
	item->ri_rmachine->rm_stats.rs_nr_timedout_items++;

	m0_rpc_conn_ha_timer_start(conn);
	switch (item->ri_sm.sm_state) {
	case M0_RPC_ITEM_ENQUEUED:
	case M0_RPC_ITEM_URGENT:
		m0_rpc_frm_remove_item(item->ri_frm, item);
		m0_rpc_item_failed(item, -ETIMEDOUT);
		m0_rpc_item_put(item);
		break;

	case M0_RPC_ITEM_SENDING:
		item->ri_error = -ETIMEDOUT;
		/* item will be moved to FAILED state in item_done() */
		break;

	case M0_RPC_ITEM_WAITING_FOR_REPLY:
		m0_rpc_item_failed(item, -ETIMEDOUT);
		break;

	default:
		M0_ASSERT(false);
	}
	M0_LEAVE();
}

static void item_resend(struct m0_rpc_item *item)
{
	int rc;

	M0_ENTRY("%p[%s/%u] %s", item, item_kind(item),
	       item->ri_type->rit_opcode, item_state_name(item));
	M0_LOG(M0_DEBUG, "%p[%s/%u] %s", item, item_kind(item),
	       item->ri_type->rit_opcode, item_state_name(item));

	item->ri_rmachine->rm_stats.rs_nr_resend_attempts++;
	switch (item->ri_sm.sm_state) {
	case M0_RPC_ITEM_ENQUEUED:
	case M0_RPC_ITEM_URGENT:
		rc = m0_rpc_item_timer_start(item);
		/* XXX already completed requests??? */
		if (rc != 0) {
			m0_rpc_frm_remove_item(item->ri_frm, item);
			m0_rpc_item_failed(item, -ETIMEDOUT);
		} else {
			item->ri_error = m0_rpc_conn_ha_timer_start(
							item2conn(item));
		}
		break;

	case M0_RPC_ITEM_SENDING:
		item->ri_error = m0_rpc_item_timer_start(item) ?:
				 m0_rpc_conn_ha_timer_start(item2conn(item));
		break;

	case M0_RPC_ITEM_WAITING_FOR_REPLY:
		if (m0_rpc_session_validate(item->ri_session) != 0) {
			M0_LOG(M0_DEBUG,
			       "session state %d does not allow sending",
			       session_state(item->ri_session));
			return;
		}
		m0_rpc_item_send(item);
		break;

	default:
		M0_ASSERT_INFO(false, "item->ri_sm.sm_state = %d",
			       item->ri_sm.sm_state);
	}
	M0_LEAVE();
}

static int item_conn_test(struct m0_rpc_item *item)
{
	M0_PRE(item != NULL && item->ri_session != NULL);
	return m0_rpc_conn_is_known_dead(item2conn(item)) ?
		-ECANCELED : 0;
}

M0_INTERNAL void m0_rpc_item_send(struct m0_rpc_item *item)
{
	uint32_t state = item->ri_sm.sm_state;
	int      rc;

	M0_ENTRY("%p[%s/%u] dest_ep=%s ri_session=%p ri_nr_sent_max=%"PRIu64
		 " ri_deadline=%"PRIu64" ri_nr_sent=%u",
		 item, item_kind(item), item->ri_type->rit_opcode,
		 m0_rpc_item_remote_ep_addr(item),
	         item->ri_session, item->ri_nr_sent_max, item->ri_deadline,
		 item->ri_nr_sent);
	M0_PRE(item != NULL && m0_rpc_machine_is_locked(item->ri_rmachine));
	M0_PRE(m0_rpc_item_is_request(item) || m0_rpc_item_is_reply(item));
	M0_PRE(ergo(m0_rpc_item_is_request(item),
		    M0_IN(state, (M0_RPC_ITEM_INITIALISED,
				  M0_RPC_ITEM_WAITING_FOR_REPLY,
				  M0_RPC_ITEM_REPLIED,
				  M0_RPC_ITEM_FAILED))) &&
	       ergo(m0_rpc_item_is_reply(item),
		    M0_IN(state, (M0_RPC_ITEM_INITIALISED,
				  M0_RPC_ITEM_SENT,
				  M0_RPC_ITEM_FAILED))));

	if (m0_rpc_item_is_request(item)) {
		rc = item_conn_test(item) ?: m0_rpc_item_timer_start(item) ?:
			m0_rpc_conn_ha_timer_start(item2conn(item));
		if (rc != 0) {
			m0_rpc_item_failed(item, rc);
			M0_LEAVE();
			return;
		}
	}

	item->ri_nr_sent++;

	if (m0_rpc_item_is_request(item))
		m0_rpc_item_pending_cache_add(item);

	if (M0_FI_ENABLED("advance_deadline")) {
		M0_LOG(M0_DEBUG,"%p deadline advanced", item);
		item->ri_deadline = m0_time_from_now(0, 500 * 1000 * 1000);
	}

	/*
	 * This hold will be released when the item is SENT or FAILED.
	 * See rpc/frmops.c:item_sent() and m0_rpc_item_failed()
	 */

	m0_rpc_session_hold_busy(item->ri_session);
	m0_rpc_frm_enq_item(&item2conn(item)->c_rpcchan->rc_frm, item);
	/*
	 * Rpc always acquires an *internal* reference to "all" items.
	 * This reference is released in item_sent()
	 */
	if (item->ri_error == 0)
		m0_rpc_item_get(item);
	M0_LEAVE();
}

M0_INTERNAL const char *
m0_rpc_item_remote_ep_addr(const struct m0_rpc_item *item)
{
	M0_PRE(item != NULL && item->ri_session != NULL);

	return item2conn(item)->c_rpcchan->rc_destep->nep_addr;
}

M0_INTERNAL int m0_rpc_item_received(struct m0_rpc_item *item,
				     struct m0_rpc_machine *machine)
{
	struct m0_rpc_item    *req;
	struct m0_rpc_conn    *conn;
	struct m0_rpc_session *sess;
	struct m0_rpc_item    *next;
	uint64_t               item_sm_id;
	int rc = 0;

	M0_PRE(item != NULL);
	M0_PRE(m0_rpc_machine_is_locked(machine));

	M0_ENTRY("%p[%s/%u], xid=%llu machine=%p", item,
		 item_kind(item), item->ri_type->rit_opcode,
	         (unsigned long long)item->ri_header.osr_xid, machine);

	item_sm_id = m0_sm_id_get(&item->ri_sm);
	M0_ADDB2_ADD(M0_AVI_RPC_ITEM_ID_FETCH,
		     item_sm_id,
		     (uint64_t)item->ri_type->rit_opcode,
		     item->ri_header.osr_xid,
		     item->ri_header.osr_session_id);

	++machine->rm_stats.rs_nr_rcvd_items;

	if (m0_rpc_item_is_oneway(item)) {
		m0_rpc_item_dispatch(item);
		return M0_RC(0);
	}

	M0_ASSERT(m0_rpc_item_is_request(item) || m0_rpc_item_is_reply(item));

	if (m0_rpc_item_is_conn_establish(item)) {
		m0_rpc_item_dispatch(item);
		return M0_RC(0);
	}

	conn = m0_rpc_machine_find_conn(machine, item);
	if (conn == NULL)
		return M0_RC(-ENOENT);
	sess = m0_rpc_session_search(conn, item->ri_header.osr_session_id);
	if (sess == NULL)
		return M0_RC(-ENOENT);
	item->ri_session = sess;

	/*
	 * If item is a request, then it may be the first arrival of the
	 * request item, or the same request may be sent again if resend
	 * interval passed. In either case item shouldn't be handled again
	 * and reply should be sent again if the item is already handled.
	 *
	 * To prevent second handling of the same item, xid is assigned to each
	 * eligible rpc item (see rpc_item_needs_xid()). It is checked on the
	 * other side (in this function). Session on the client and on the
	 * server has xid counter, and items with wrong xid are dropped after
	 * adding those to the item cache if not present there already.
	 * In case if there is a reply in the reply cache, the reply is sent
	 * again.
	 *
	 * Note that there is no duplicate or out-of-order detection for oneway
	 * or connection establish rpc items.
	 *
	 * xid-based duplicate and out-of-order checks are only temporary
	 * solutions until DTM is implemented.
	 */
	if (m0_rpc_item_is_request(item)) {
		for (next = NULL;
		     item != NULL && m0_rpc_item_xid_check(item, &next);
		     item = next) {
			m0_rpc_session_hold_busy(sess);
			rc = m0_rpc_item_dispatch(item);
			m0_rpc_item_cache_del(&sess->s_req_cache,
					      item->ri_header.osr_xid);
			if (rc != 0) {
				/*
				 * In case of ESHUTDOWN, when we are not fully
				 * started yet, we just drop the request items
				 * without any reply. So we should rollback our
				 * s_xid counter to avoid desynchronization.
				 * Otherwise, the sender would just get stuck on
				 * re-sending its requests (with the same xid)
				 * as we would just drop them as a stale ones
				 * (even after we would become ready to serve
				 * them already).
				 */
				if (rc == -ESHUTDOWN)
					--sess->s_xid;
				m0_rpc_session_release(sess);
				break;
			}
		}
	} else {
		rc = item_reply_received(item, &req);
	}

	return M0_RC(rc);
}

static int item_reply_received(struct m0_rpc_item *reply,
			       struct m0_rpc_item **req_out)
{
	struct m0_rpc_item *req;
	int                 rc;

	M0_PRE(reply != NULL && req_out != NULL);
	M0_ENTRY("item_reply: %p[%u]", reply, reply->ri_type->rit_opcode);

	*req_out = NULL;

	req = m0_cookie_of(&reply->ri_header.osr_cookie,
			   struct m0_rpc_item, ri_cookid);
	if (req == NULL) {
		/*
		 * Either it is a duplicate reply and its corresponding request
		 * item is pruned from the item list, or it is a corrupted
		 * reply, or it is meant for a request that was cancelled.
		 */
		M0_LOG(M0_DEBUG, "request not found for reply item %p[%u]",
		       reply, reply->ri_type->rit_opcode);
		return M0_RC(-EPROTO);
	}
	M0_LOG(M0_DEBUG, "req %p[%s/%u], reply %p[%s/%u]",
		req, item_kind(req), req->ri_type->rit_opcode,
		reply, item_kind(reply), reply->ri_type->rit_opcode);

	if (item_reply_received_fi(req, reply))
		return M0_RC(-EREMOTE);

	rc = req_replied(req, reply);
	if (rc == 0)
		*req_out = req;

	return M0_RC(rc);
}

static bool item_reply_received_fi(struct m0_rpc_item *req,
				   struct m0_rpc_item *reply)
{
	if (M0_FI_ENABLED("drop_create_item_reply") &&
	    req->ri_type->rit_opcode == M0_IOSERVICE_COB_CREATE_OPCODE) {
		M0_LOG(M0_DEBUG, "%p[%s/%u] create reply dropped", reply,
		       item_kind(reply), reply->ri_type->rit_opcode);
		return true;
	}
	if (M0_FI_ENABLED("drop_delete_item_reply") &&
	    req->ri_type->rit_opcode == M0_IOSERVICE_COB_DELETE_OPCODE) {
		M0_LOG(M0_DEBUG, "%p[%s/%u] delete reply dropped", reply,
		       item_kind(reply), reply->ri_type->rit_opcode);
		return true;
	}
	return false;
}

static int req_replied(struct m0_rpc_item *req, struct m0_rpc_item *reply)
{
	int rc = 0;

	M0_PRE(req != NULL && reply != NULL);

	if (req->ri_error == -ETIMEDOUT) {
		/*
		 * The reply is valid but too late. Do nothing.
		 */
		M0_LOG(M0_DEBUG, "rply rcvd, timedout req %p [%s/%u]",
			req, item_kind(req), req->ri_type->rit_opcode);
		rc = -EPROTO;
	} else if (req->ri_error != 0) {
		M0_LOG(M0_NOTICE, "rply rcvd, req %p [%s/%u], error=%d",
			req, item_kind(req), req->ri_type->rit_opcode,
			req->ri_error);
		rc = -EPROTO;
	} else {
		/*
		 * This is valid reply case.
		 */
		m0_rpc_item_get(reply);

		switch (req->ri_sm.sm_state) {
		case M0_RPC_ITEM_ENQUEUED:
		case M0_RPC_ITEM_URGENT:
			/*
			 * Reply received while we were trying to resend.
			 */
			m0_rpc_frm_remove_item(
				&item2conn(req)->c_rpcchan->rc_frm,
				req);
			m0_rpc_item_process_reply(req, reply);
			m0_rpc_session_release(req->ri_session);
			/*
			 * We took get(req) at m0_rpc_item_send() and but
			 * the correspondent put() at item_sent() won't be
			 * called already, so we have to put() it here.
			 */
			m0_rpc_item_put(req);
			break;

		case M0_RPC_ITEM_SENDING:
			/*
			 * Buffer sent callback is still pending;
			 * postpone reply processing.
			 * item_sent() will process the reply.
			 */
			M0_LOG(M0_DEBUG, "req: %p rply: %p rply postponed",
			       req, reply);
			req->ri_pending_reply = reply;
			break;

		case M0_RPC_ITEM_ACCEPTED:
		case M0_RPC_ITEM_WAITING_FOR_REPLY:
			m0_rpc_item_process_reply(req, reply);
			break;

		case M0_RPC_ITEM_REPLIED:
			/* Duplicate reply. Drop it. */
			req->ri_rmachine->rm_stats.rs_nr_dropped_items++;
			m0_rpc_item_put(reply);
			break;

		default:
			M0_ASSERT(false);
		}
	}
	return M0_RC(rc);
}

/**
 * HA to be updated in case the peered service replied after experiencing
 * issues, and the item was re-sending due to those.
 */
static void item__on_reply_postprocess(struct m0_rpc_item *item)
{
	struct m0_rpc_conn *conn;
	struct m0_conf_obj *svc_obj;

	M0_ENTRY("%p[%s/%u] %s", item, item_kind(item),
	       item->ri_type->rit_opcode, item_state_name(item));
	M0_LOG(M0_DEBUG, "%p[%s/%u] %s", item, item_kind(item),
	       item->ri_type->rit_opcode, item_state_name(item));
	conn = item2conn(item);
	svc_obj = m0_rpc_conn2svc(conn);
	if (svc_obj == NULL)
		goto leave; /*
			     * connection is not subscribed to HA notes, so no
			     * reaction is expected on reply
			     */
	if (svc_obj->co_ha_state == M0_NC_TRANSIENT) {
		conn->c_ha_cfg->rchc_ops.cho_ha_notify(conn, M0_NC_ONLINE);
		conn_flag_unset(conn, RCF_TRANSIENT_SENT);
	}
leave:
	M0_LEAVE();
}

static void addb2_add_rpc_attrs(const struct m0_rpc_item *req)
{
	M0_ADDB2_ADD(M0_AVI_ATTR, m0_sm_id_get(&req->ri_sm),
		     M0_AVI_RPC_ATTR_OPCODE, req->ri_type->rit_opcode);
	M0_ADDB2_ADD(M0_AVI_ATTR, m0_sm_id_get(&req->ri_sm),
		     M0_AVI_RPC_ATTR_NR_SENT, req->ri_nr_sent);
}

M0_INTERNAL void m0_rpc_item_process_reply(struct m0_rpc_item *req,
					   struct m0_rpc_item *reply)
{
	M0_ENTRY("%p[%s/%u]", req, item_kind(req), req->ri_type->rit_opcode);

	M0_PRE(req != NULL && reply != NULL);
	M0_PRE(m0_rpc_item_is_request(req));
	M0_PRE(M0_IN(req->ri_sm.sm_state, (M0_RPC_ITEM_WAITING_FOR_REPLY,
					   M0_RPC_ITEM_ENQUEUED,
					   M0_RPC_ITEM_URGENT)));
	m0_rpc_item_timer_stop(req);
	m0_rpc_conn_ha_timer_stop(item2conn(req));
	req->ri_reply = reply;
	m0_rpc_item_replied_invoke(req);
	m0_rpc_item_change_state(req, M0_RPC_ITEM_REPLIED);
	m0_rpc_item_pending_cache_del(req);
	item__on_reply_postprocess(req);

	/*
	 * Only attributes of reasonable RPC items are logged
	 * due to performance aspect.
	 */
	if (M0_IN(req->ri_type->rit_opcode, (M0_IOSERVICE_READV_OPCODE,
					     M0_IOSERVICE_WRITEV_OPCODE,
					     M0_IOSERVICE_COB_CREATE_OPCODE,
					     M0_IOSERVICE_COB_DELETE_OPCODE,
					     M0_CAS_GET_FOP_OPCODE,
					     M0_CAS_PUT_FOP_OPCODE,
					     M0_CAS_DEL_FOP_OPCODE,
					     M0_CAS_CUR_FOP_OPCODE)))
		addb2_add_rpc_attrs(req);
	/*
	 * Reference release done here is for the reference taken in
	 * m0_rpc__post_locked() for request items.
	 */
	m0_rpc_item_put(req);
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_item_send_reply(struct m0_rpc_item *req,
					struct m0_rpc_item *reply)
{
	struct m0_rpc_session *sess;

	M0_PRE(req != NULL && reply != NULL);
	M0_PRE(m0_rpc_item_is_request(req));
	M0_PRE(M0_IN(req->ri_sm.sm_state, (M0_RPC_ITEM_ACCEPTED)));
	M0_PRE(M0_IN(req->ri_reply, (NULL, reply)));

	M0_ENTRY("req=%p[%u], reply=%p[%u]",
		 req, req->ri_type->rit_opcode, reply, reply->ri_type->rit_opcode);

	if (req->ri_reply == NULL) {
		m0_rpc_item_get(reply);
		req->ri_reply = reply;
	}
	m0_rpc_item_change_state(req, M0_RPC_ITEM_REPLIED);

	sess = req->ri_session;
	reply->ri_header = req->ri_header;
	if (rpc_item_needs_xid(req))
		m0_rpc_item_cache_add(&sess->s_reply_cache, reply,
			m0_time_from_now(M0_RPC_ITEM_REPLY_CACHE_TMO, 0));
	m0_rpc_item_send(reply);
	m0_rpc_session_release(sess);

	/*
	 * An extra reference is acquired for this reply item
	 * at the end of m0_rpc_item_send() if it is sent successfully.
	 * This extra reference will be released along with request
	 * put in m0_fom_fini() -> m0_fop_put() -> m0_ref_put() ->
	 * m0_fop_release() -> m0_fop_fini() -> m0_rpc_item_put() if
	 * req->ri_reply is set. If the reply item is failed to send,
	 * no extra reference is acquired for that reply item in
	 * m0_rpc_item_send(). So this req->ri_reply must be cleared
	 * in this error case.
	 */
	if (reply->ri_error != 0)
		req->ri_reply = NULL;

	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_item_cache_init(struct m0_rpc_item_cache *ic,
				       struct m0_mutex          *lock)
{
	int i;

	M0_ALLOC_ARR(ic->ric_items, RIC_HASH_SIZE);
	if (ic->ric_items == NULL)
		return M0_ERR(-ENOMEM);

	for (i = 0; i < RIC_HASH_SIZE; ++i)
		ric_tlist_init(&ic->ric_items[i]);

	ic->ric_lock = lock;

	M0_POST(m0_rpc_item_cache__invariant(ic));

	return 0;
}

M0_INTERNAL void m0_rpc_item_cache_fini(struct m0_rpc_item_cache *ic)
{
	int i;

	M0_PRE(m0_rpc_item_cache__invariant(ic));

	m0_rpc_item_cache_clear(ic);
	for (i = 0; i < RIC_HASH_SIZE; ++i)
		ric_tlist_fini(&ic->ric_items[i]);
	m0_free(ic->ric_items);
}

M0_INTERNAL bool m0_rpc_item_cache__invariant(struct m0_rpc_item_cache *ic)
{
	return m0_mutex_is_locked(ic->ric_lock);
}

M0_INTERNAL bool m0_rpc_item_cache_add(struct m0_rpc_item_cache *ic,
				       struct m0_rpc_item	*item,
				       m0_time_t		 deadline)
{
	struct m0_rpc_item *cached;

	M0_ENTRY("item: %p [%s/%u] xid=%"PRIu64, item, item_kind(item),
			item->ri_type->rit_opcode, item->ri_header.osr_xid);
	M0_PRE(m0_rpc_item_cache__invariant(ic));

	cached = m0_rpc_item_cache_lookup(ic, item->ri_header.osr_xid);
	if (cached == NULL) {
		m0_rpc_item_get(item);
		ric_tlink_init_at(item, &ic->ric_items[item->ri_header.osr_xid &
		                                      RIC_HASH_MASK]);
	}
	M0_ASSERT_INFO(M0_IN(cached, (NULL, item)), "duplicate xid=%"PRIu64
		       " item=%p", item->ri_header.osr_xid, item);
	item->ri_cache_deadline = deadline;

	return M0_RC(cached == NULL);
}

static void rpc_item_cache_del(struct m0_rpc_item_cache *ic,
			       struct m0_rpc_item	*item)
{
	M0_ENTRY("item: %p [%s/%u] xid=%"PRIu64, item, item_kind(item),
		 item->ri_type->rit_opcode, item->ri_header.osr_xid);
	ric_tlink_del_fini(item);
	m0_rpc_item_put(item);
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_item_cache_del(struct m0_rpc_item_cache *ic,
				       uint64_t			 xid)
{
	struct m0_rpc_item *cached;

	M0_ENTRY("xid=%"PRIu64, xid);
	M0_PRE(m0_rpc_item_cache__invariant(ic));

	cached = m0_rpc_item_cache_lookup(ic, xid);
	if (cached != NULL)
		rpc_item_cache_del(ic, cached);
	M0_LEAVE();
}

M0_INTERNAL struct m0_rpc_item *
m0_rpc_item_cache_lookup(struct m0_rpc_item_cache *ic, uint64_t xid)
{
	M0_PRE(m0_rpc_item_cache__invariant(ic));

	return m0_tl_find(ric, item, &ic->ric_items[xid & RIC_HASH_MASK],
			  item->ri_header.osr_xid == xid);
}

M0_INTERNAL void m0_rpc_item_cache_purge(struct m0_rpc_item_cache *ic)
{
	int i;
	struct m0_rpc_item *item;
	m0_time_t	    now = m0_time_now();

	M0_PRE(m0_rpc_item_cache__invariant(ic));

	for (i = 0; i < RIC_HASH_SIZE; ++i) {
		m0_tl_for(ric, &ic->ric_items[i], item) {
			if (now > item->ri_cache_deadline)
				rpc_item_cache_del(ic, item);
		} m0_tl_endfor;
	}
}

M0_INTERNAL void m0_rpc_item_cache_clear(struct m0_rpc_item_cache *ic)
{
	int i;
	struct m0_rpc_item *item;

	M0_PRE(m0_rpc_item_cache__invariant(ic));

	for (i = 0; i < RIC_HASH_SIZE; ++i) {
		m0_tl_for(ric, &ic->ric_items[i], item) {
			rpc_item_cache_del(ic, item);
		} m0_tl_endfor;
	}
}

M0_INTERNAL void m0_rpc_item_pending_cache_init(struct m0_rpc_session *session)
{
	M0_PRE(m0_rpc_machine_is_locked(session->s_conn->c_rpc_machine));
	pending_item_tlist_init(&session->s_pending_cache);
}

static void pending_cache_drain(struct m0_rpc_session *session)
{
	struct m0_rpc_item *item;

	M0_PRE(m0_rpc_machine_is_locked(session->s_conn->c_rpc_machine));
	m0_tl_teardown(pending_item, &session->s_pending_cache, item) {
		m0_rpc_item_put(item);
	};
}

M0_INTERNAL void m0_rpc_item_pending_cache_fini(struct m0_rpc_session *session)
{
	M0_PRE(m0_rpc_machine_is_locked(session->s_conn->c_rpc_machine));
	pending_cache_drain(session);
	pending_item_tlist_fini(&session->s_pending_cache);
}

M0_INTERNAL void m0_rpc_item_pending_cache_add(struct m0_rpc_item *item)
{
	if (item->ri_session->s_session_id == SESSION_ID_0)
		return;

	M0_ENTRY("%p[%u] xid=%"PRIu64", nr_sent %lu, item->ri_reply %p", item,
		 item->ri_type->rit_opcode, item->ri_header.osr_xid,
		 (unsigned long)item->ri_nr_sent, item->ri_reply);

	M0_PRE(m0_rpc_item_is_request(item));
	M0_PRE(m0_rpc_machine_is_locked(item->ri_rmachine));

	if (item->ri_nr_sent > 1 && item->ri_reply == NULL) {
		M0_ASSERT(pending_item_tlink_is_in(item));
		M0_LEAVE("%p", item);
		return;
	}

	M0_ASSERT(!pending_item_tlink_is_in(item));
	/*
	 * The item is sent for the first time OR
	 * it has been resent after it has received reply e.g. during recovery.
	 */
	M0_ASSERT((item->ri_nr_sent == 1 && item->ri_reply == NULL) ||
		  (item->ri_nr_sent > 1 && item->ri_reply != NULL));
	m0_rpc_item_get(item);
	pending_item_tlink_init_at(item, &item->ri_session->s_pending_cache);
	M0_POST(pending_item_tlink_is_in(item));
	M0_LEAVE("%p Added to pending cache", item);
}

M0_INTERNAL void m0_rpc_item_pending_cache_del(struct m0_rpc_item *item)
{
	if (item->ri_session->s_session_id == SESSION_ID_0)
		return;

	M0_ENTRY("%p[%u]", item, item->ri_type->rit_opcode);
	M0_PRE(m0_rpc_item_is_request(item));
	M0_PRE(m0_rpc_machine_is_locked(item->ri_rmachine));
	M0_PRE(pending_item_tlink_is_in(item));

	pending_item_tlink_del_fini(item);
	m0_rpc_item_put(item);
	M0_LEAVE("%p Removed from pending cache", item);
}

M0_INTERNAL void m0_rpc_item_replied_invoke(struct m0_rpc_item *req)
{
	if (req->ri_ops != NULL && req->ri_ops->rio_replied != NULL) {
		M0_ADDB2_PUSH(M0_AVI_RPC_REPLIED,
			      (uint64_t)req, req->ri_type->rit_opcode);
		req->ri_ops->rio_replied(req);
		m0_addb2_pop(M0_AVI_RPC_REPLIED);
	}
}

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
