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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original author: Rajesh Bhalerao <rajesh_bhalerao@xyratex.com>
 * Original creation date: 04/28/2011
 */


#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RM

#include "lib/memory.h"  /* M0_ALLOC_PTR */
#include "lib/misc.h"    /* M0_SET_ARR0 */
#include "lib/errno.h"   /* ETIMEDOUT */
#include "lib/arith.h"   /* M0_CNT_{INC,DEC} */
#include "lib/mutex.h"
#include "lib/trace.h"
#include "lib/bob.h"
#include "lib/finject.h" /* M0_FI_ENABLED */
#include "fid/fid.h"
#include "addb2/addb2.h"
#include "mero/magic.h"
#include "sm/sm.h"
#include "conf/obj.h"    /* m0_conf_obj      */
#include "fid/fid.h"     /* m0_fid           */

#include "rm/rm.h"
#include "rm/rm_internal.h"
#include "rpc/service.h"  /* m0_rpc_service_session_release */

/**
   @addtogroup rm
   @{
 */
struct owner_invariant_state;

static void resource_get           (struct m0_rm_resource *res);
static void resource_put           (struct m0_rm_resource *res);
static bool resource_list_check    (const struct m0_rm_resource *res,
				    void *datum);
static bool resource_type_invariant(const struct m0_rm_resource_type *rt);

static void owner_balance          (struct m0_rm_owner *o);
static bool owner_invariant        (struct m0_rm_owner *owner);
static void pin_del                (struct m0_rm_pin *pin);
static bool owner_invariant_state  (const struct m0_rm_owner *owner,
				    struct owner_invariant_state *is);
static struct m0_rm_incoming *cr2in(const struct m0_rm_credit *cr);
static void incoming_check         (struct m0_rm_incoming *in);
static int  incoming_check_with    (struct m0_rm_incoming *in,
				    struct m0_rm_credit *credit);
static void incoming_complete      (struct m0_rm_incoming *in, int32_t rc);
static void incoming_policy_apply  (struct m0_rm_incoming *in);
static void incoming_policy_none   (struct m0_rm_incoming *in);
static bool incoming_invariant     (const struct m0_rm_incoming *in);
static int  incoming_pin_nr        (const struct m0_rm_incoming *in,
				    uint32_t flags);
static void incoming_release       (struct m0_rm_incoming *in);

static int  credit_pin_nr           (const struct m0_rm_credit *credit,
				    uint32_t flags);
static int  service_locate         (struct m0_rm_resource_type *rtype,
				    struct m0_rm_remote *rem);
static int  resource_locate        (struct m0_rm_resource_type *rtype,
				    struct m0_rm_remote *rem);
static int  outgoing_check         (struct m0_rm_incoming *in,
				    enum m0_rm_outgoing_type,
				    struct m0_rm_credit *credit,
				    struct m0_rm_remote *other);
static int  revoke_send            (struct m0_rm_incoming *in,
				    struct m0_rm_loan *loan,
				    struct m0_rm_credit *credit);
static int  cancel_send            (struct m0_rm_loan *loan);
static int  borrow_send            (struct m0_rm_incoming *in,
				    struct m0_rm_credit *credit);
static bool credit_eq               (const struct m0_rm_credit *c0,
				     const struct m0_rm_credit *c1);
static bool credit_is_empty         (const struct m0_rm_credit *credit);
static bool credit_intersects       (const struct m0_rm_credit *A,
				     const struct m0_rm_credit *B);
static bool credit_conflicts        (const struct m0_rm_credit *A,
				     const struct m0_rm_credit *B);
static int  credit_diff             (struct m0_rm_credit *c0,
				     const struct m0_rm_credit *c1);
static void conflict_notify         (struct m0_rm_credit *credit);
static void windup_incoming_complete(struct m0_rm_incoming *in,
				     int32_t rc);
static void windup_incoming_conflict(struct m0_rm_incoming *in);
static int cached_credits_hold       (struct m0_rm_incoming *in);
static void cached_credits_clear     (struct m0_rm_owner *owner);
static bool owner_is_idle	    (const struct m0_rm_owner *o);
static bool owner_is_liquidated     (const struct m0_rm_owner *o);
static bool incoming_is_complete    (const struct m0_rm_incoming *in);
static int remnant_credit_get	    (const struct m0_rm_credit *src,
				     const struct m0_rm_credit *diff,
				     struct m0_rm_credit **remnant_credit);
static int remnant_loan_get	    (const struct m0_rm_loan *loan,
				     const struct m0_rm_credit *credit,
				     struct m0_rm_loan **remnant_loan);
static int loan_dup		    (const struct m0_rm_loan *src_loan,
				     struct m0_rm_loan **dest_loan);
static void owner_liquidate	    (struct m0_rm_owner *src_owner);
static void owner_cleanup           (struct m0_rm_owner *owner);
static void credit_processor        (struct m0_rm_resource_type *rt);
static bool rm_on_remote_death_cb   (struct m0_clink *link);
static bool owner_smgrp_is_locked   (const struct m0_rm_owner *owner);

static void rm_remote_online_handler(struct m0_rm_remote *remote);
static void rm_remote_death_handler(struct m0_rm_remote *remote);

#define INCOMING_CREDIT(in) in->rin_want.cr_datum

M0_TL_DESCR_DEFINE(res, "resources", , struct m0_rm_resource,
		   r_linkage, r_magix,
		   M0_RM_RESOURCE_MAGIC, M0_RM_RESOURCE_HEAD_MAGIC);
M0_TL_DEFINE(res, M0_INTERNAL, struct m0_rm_resource);

static struct m0_bob_type resource_bob;
M0_BOB_DEFINE(M0_INTERNAL, &resource_bob, m0_rm_resource);

M0_TL_DESCR_DEFINE(m0_rm_ur, "usage credits", , struct m0_rm_credit,
		   cr_linkage, cr_magix,
		   M0_RM_CREDIT_MAGIC, M0_RM_USAGE_CREDIT_HEAD_MAGIC);
M0_TL_DEFINE(m0_rm_ur, M0_INTERNAL, struct m0_rm_credit);

M0_TL_DESCR_DEFINE(m0_remotes, "remote owners", , struct m0_rm_remote,
		   rem_res_linkage, rem_magix,
		   M0_RM_REMOTE_MAGIC, M0_RM_REMOTE_OWNER_HEAD_MAGIC);
M0_TL_DEFINE(m0_remotes, M0_INTERNAL, struct m0_rm_remote);

M0_TL_DESCR_DEFINE(m0_owners, "local owners", , struct m0_rm_owner,
		   ro_owner_linkage, ro_magix,
		   M0_RM_OWNER_LIST_MAGIC, M0_RM_OWNER_LIST_HEAD_MAGIC);
M0_TL_DEFINE(m0_owners, M0_INTERNAL, struct m0_rm_owner);

static const struct m0_bob_type credit_bob = {
        .bt_name         = "credit",
        .bt_magix_offset = offsetof(struct m0_rm_credit, cr_magix),
        .bt_magix        = M0_RM_CREDIT_MAGIC,
        .bt_check        = NULL
};
M0_BOB_DEFINE(M0_INTERNAL, &credit_bob, m0_rm_credit);

M0_TL_DESCR_DEFINE(pr, "pins-of-credit", , struct m0_rm_pin,
		   rp_credit_linkage, rp_magix,
		   M0_RM_PIN_MAGIC, M0_RM_CREDIT_PIN_HEAD_MAGIC);
M0_TL_DEFINE(pr, M0_INTERNAL, struct m0_rm_pin);

M0_TL_DESCR_DEFINE(pi, "pins-of-incoming", , struct m0_rm_pin,
		   rp_incoming_linkage, rp_magix,
		   M0_RM_PIN_MAGIC, M0_RM_INCOMING_PIN_HEAD_MAGIC);
M0_TL_DEFINE(pi, M0_INTERNAL, struct m0_rm_pin);

static bool pin_check(const void *bob);

static const struct m0_bob_type pin_bob = {
	.bt_name         = "pin",
	.bt_magix_offset = offsetof(struct m0_rm_pin, rp_magix),
	.bt_magix        = M0_RM_PIN_MAGIC,
	.bt_check        = pin_check
};
M0_BOB_DEFINE(static, &pin_bob, m0_rm_pin);

M0_INTERNAL const struct m0_bob_type loan_bob = {
	.bt_name         = "loan",
	.bt_magix_offset = offsetof(struct m0_rm_loan, rl_magix),
	.bt_magix        = M0_RM_LOAN_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(, &loan_bob, m0_rm_loan);

static const struct m0_bob_type incoming_bob = {
	.bt_name         = "incoming request",
	.bt_magix_offset = offsetof(struct m0_rm_incoming, rin_magix),
	.bt_magix        = M0_RM_INCOMING_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(static, &incoming_bob, m0_rm_incoming);

static const struct m0_bob_type outgoing_bob = {
	.bt_name         = "outgoing request ",
	.bt_magix_offset = offsetof(struct m0_rm_outgoing, rog_magix),
	.bt_magix        = M0_RM_OUTGOING_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(M0_INTERNAL, &outgoing_bob, m0_rm_outgoing);

static const struct m0_bob_type rem_bob = {
	.bt_name         = "proxy for remote owner ",
	.bt_magix_offset = offsetof(struct m0_rm_remote, rem_magix),
	.bt_magix        = M0_RM_REMOTE_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(M0_INTERNAL, &rem_bob, m0_rm_remote);

const struct m0_uint128 m0_rm_no_group = M0_UINT128(0, 0);

enum {CNT_NR, CNT_TIME, CNT_LAST};

M0_INTERNAL void m0_rm_domain_init(struct m0_rm_domain *dom)
{
	M0_ENTRY();
	M0_PRE(dom != NULL);

	M0_SET_ARR0(dom->rd_types);
	m0_mutex_init(&dom->rd_lock);
	m0_bob_type_tlist_init(&resource_bob, &res_tl);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_domain_init);

M0_INTERNAL void m0_rm_domain_fini(struct m0_rm_domain *dom)
{
	M0_ENTRY();
	M0_PRE(m0_forall(i, ARRAY_SIZE(dom->rd_types),
			 dom->rd_types[i] == NULL));
	m0_mutex_fini(&dom->rd_lock);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_domain_fini);

static const struct m0_rm_incoming_ops windup_incoming_ops = {
	.rio_complete = windup_incoming_complete,
	.rio_conflict = windup_incoming_conflict,
};

M0_INTERNAL struct m0_rm_resource *
m0_rm_resource_find(const struct m0_rm_resource_type *rt,
		    const struct m0_rm_resource      *res)
{
	M0_PRE(rt->rt_ops->rto_eq != NULL);

	return m0_tl_find(res, scan, &rt->rt_resources,
			  rt->rt_ops->rto_eq(res, scan));
}

M0_INTERNAL int m0_rm_type_register(struct m0_rm_domain        *dom,
				    struct m0_rm_resource_type *rt)
{
	int rc;

	M0_ENTRY("resource type: %s", rt->rt_name);
	M0_PRE(rt->rt_dom == NULL);
	M0_PRE(IS_IN_ARRAY(rt->rt_id, dom->rd_types));
	M0_PRE(dom->rd_types[rt->rt_id] == NULL);

	m0_mutex_init(&rt->rt_lock);
	res_tlist_init(&rt->rt_resources);
	rt->rt_nr_resources = 0;
	m0_sm_group_init(&rt->rt_sm_grp);

	m0_mutex_init(&rt->rt_queue_guard);
	m0_queue_init(&rt->rt_ha_events);
	rt->rt_stop_worker = false;
	rc = M0_THREAD_INIT(&rt->rt_worker, struct m0_rm_resource_type *, NULL,
			    &credit_processor, rt, "m0_rm_rt_agent");
	if (rc != 0)
		return M0_RC(rc);

	m0_mutex_lock(&dom->rd_lock);
	dom->rd_types[rt->rt_id] = rt;
	rt->rt_dom = dom;
	M0_POST(resource_type_invariant(rt));
	m0_mutex_unlock(&dom->rd_lock);

	M0_POST(dom->rd_types[rt->rt_id] == rt);
	M0_POST(rt->rt_dom == dom);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_type_register);

M0_INTERNAL void m0_rm_type_deregister(struct m0_rm_resource_type *rt)
{
	struct m0_rm_domain *dom = rt->rt_dom;

	M0_ENTRY("resource type: %s", rt->rt_name);
	M0_PRE(dom != NULL);
	M0_PRE(res_tlist_is_empty(&rt->rt_resources));
	M0_PRE(rt->rt_nr_resources == 0);

	m0_mutex_lock(&dom->rd_lock);
	M0_PRE(IS_IN_ARRAY(rt->rt_id, dom->rd_types));
	M0_PRE(dom->rd_types[rt->rt_id] == rt);
	M0_PRE(resource_type_invariant(rt));

	dom->rd_types[rt->rt_id] = NULL;
	m0_mutex_unlock(&dom->rd_lock);

	m0_sm_group_lock(&rt->rt_sm_grp);
	rt->rt_stop_worker = true;
	m0_clink_signal(&rt->rt_sm_grp.s_clink);
	m0_sm_group_unlock(&rt->rt_sm_grp);

	M0_LOG(M0_DEBUG, "Waiting for RM RT agent to join");
	m0_thread_join(&rt->rt_worker);
	m0_thread_fini(&rt->rt_worker);
	m0_sm_group_fini(&rt->rt_sm_grp);
	m0_queue_fini(&rt->rt_ha_events);
	m0_mutex_fini(&rt->rt_queue_guard);

	rt->rt_dom = NULL;
	res_tlist_fini(&rt->rt_resources);
	m0_mutex_fini(&rt->rt_lock);

	M0_POST(rt->rt_dom == NULL);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_type_deregister);

M0_INTERNAL struct m0_rm_resource_type *
m0_rm_resource_type_lookup(const struct m0_rm_domain *dom,
			   const uint64_t             rtype_id)
{
	M0_PRE(dom != NULL);
	M0_PRE(IS_IN_ARRAY(rtype_id, dom->rd_types));
	M0_PRE(dom->rd_types[rtype_id] != NULL);

	return dom->rd_types[rtype_id];
}

M0_INTERNAL void m0_rm_resource_add(struct m0_rm_resource_type *rtype,
				    struct m0_rm_resource      *res)
{
	M0_ENTRY("res-type: %p resource : %p", rtype, res);

	m0_mutex_lock(&rtype->rt_lock);
	M0_PRE_EX(resource_type_invariant(rtype));
	M0_PRE(res->r_ref == 0);
	M0_PRE_EX(m0_rm_resource_find(rtype, res) == NULL);
	res->r_type = rtype;
	res_tlink_init_at(res, &rtype->rt_resources);
	m0_remotes_tlist_init(&res->r_remote);
	m0_owners_tlist_init(&res->r_local);
	m0_rm_resource_bob_init(res);
	M0_CNT_INC(rtype->rt_nr_resources);
	M0_POST(res_tlist_contains(&rtype->rt_resources, res));
	M0_POST_EX(resource_type_invariant(rtype));
	m0_mutex_unlock(&rtype->rt_lock);
	M0_POST(res->r_type == rtype);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_resource_add);

M0_INTERNAL void m0_rm_resource_del(struct m0_rm_resource *res)
{
	struct m0_rm_resource_type *rtype = res->r_type;

	M0_ENTRY("resource : %p", res);
	m0_mutex_lock(&rtype->rt_lock);
	M0_PRE(res->r_ref == 0);
	M0_PRE_EX(res_tlist_contains(&rtype->rt_resources, res));
	M0_PRE(m0_remotes_tlist_is_empty(&res->r_remote));
	M0_PRE(m0_owners_tlist_is_empty(&res->r_local));
	M0_PRE_EX(resource_type_invariant(rtype));

	res_tlink_del_fini(res);
	M0_CNT_DEC(rtype->rt_nr_resources);

	M0_POST_EX(resource_type_invariant(rtype));
	M0_POST_EX(!res_tlist_contains(&rtype->rt_resources, res));
	m0_remotes_tlist_fini(&res->r_remote);
	m0_owners_tlist_fini(&res->r_local);
	m0_rm_resource_bob_fini(res);
	m0_mutex_unlock(&rtype->rt_lock);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_resource_del);

M0_INTERNAL void m0_rm_resource_free(struct m0_rm_resource *res)
{
	M0_PRE(res->r_ops != NULL && res->r_ops->rop_resource_free != NULL);

	res->r_ops->rop_resource_free(res);
}

M0_INTERNAL int m0_rm_resource_encode(struct m0_rm_resource *res,
				      struct m0_buf         *buf)
{
	struct m0_bufvec	datum_buf;
	struct m0_bufvec_cursor cursor;

	M0_PRE(buf != NULL);
	M0_PRE(res->r_type != NULL);
	M0_PRE(res->r_type->rt_ops != NULL);
	M0_PRE(res->r_type->rt_ops->rto_len != NULL);
	M0_PRE(res->r_type->rt_ops->rto_encode != NULL);

	/*
	 * A resource type ID needs to be encoded before encoding resource
	 * onto buffer; This is required because whenever a borrow request
	 * reaches creditor, type needs to be identified to call type specific
	 * decode function.
	 */
	buf->b_nob = sizeof res->r_type->rt_id +
			res->r_type->rt_ops->rto_len(res);
	buf->b_addr = m0_alloc(buf->b_nob);
	if (buf->b_addr == NULL)
		return M0_ERR(-ENOMEM);

	datum_buf.ov_buf = &buf->b_addr;
	datum_buf.ov_vec.v_nr = 1;
	datum_buf.ov_vec.v_count = &buf->b_nob;

	m0_bufvec_cursor_init(&cursor, &datum_buf);

	/*
	 * Copy resource type ID first to buffer, followed by actual
	 * resource description
	 */
	m0_bufvec_cursor_copyto(&cursor, (void *)&res->r_type->rt_id,
				sizeof res->r_type->rt_id);
	return M0_RC(res->r_type->rt_ops->rto_encode(&cursor, res));
}
M0_EXPORTED(m0_rm_resource_encode);

static void resource_get(struct m0_rm_resource *res)
{
	struct m0_rm_resource_type *rtype = res->r_type;
	uint32_t                    count;

	M0_ENTRY("resource : %p", res);
	m0_mutex_lock(&rtype->rt_lock);
	count = res->r_ref;
	M0_CNT_INC(res->r_ref);
	m0_mutex_unlock(&rtype->rt_lock);
	M0_LOG(M0_DEBUG, "ref[%u -> %u]", count, count + 1);
	M0_LEAVE();
}

static void resource_put(struct m0_rm_resource *res)
{
	struct m0_rm_resource_type *rtype = res->r_type;
	uint32_t                    count;

	M0_ENTRY("resource : %p", res);
	m0_mutex_lock(&rtype->rt_lock);
	count = res->r_ref;
	M0_CNT_DEC(res->r_ref);
	m0_mutex_unlock(&rtype->rt_lock);
	M0_LOG(M0_DEBUG, "ref[%u -> %u]", count, count - 1);
	M0_LEAVE();
}

static struct m0_sm_state_descr owner_states[] = {
	[ROS_INITIAL] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Init",
		.sd_allowed   = M0_BITS(ROS_INITIALISING)
	},
	[ROS_INITIALISING] = {
		.sd_name      = "Initialising",
		.sd_allowed   = M0_BITS(ROS_ACTIVE)
	},
	[ROS_ACTIVE] = {
		.sd_name      = "Active",
		.sd_allowed   = M0_BITS(ROS_QUIESCE)
	},
	[ROS_QUIESCE] = {
		.sd_name      = "Quiesce",
		.sd_allowed   = M0_BITS(ROS_FINALISING)
	},
	[ROS_FINALISING] = {
		.sd_name      = "Finalising",
		.sd_allowed   = M0_BITS(ROS_INSOLVENT, ROS_DEAD_CREDITOR,
					ROS_FINAL)
	},
	[ROS_DEAD_CREDITOR] = {
		.sd_flags     = M0_SDF_FAILURE | M0_SDF_FINAL,
		.sd_name      = "Creditor_is_dead",
		.sd_allowed   = M0_BITS(ROS_ACTIVE, ROS_FINAL),
	},
	[ROS_INSOLVENT] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Insolvent"
	},
	[ROS_FINAL] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Fini"
	}
};

static const struct m0_sm_conf owner_conf = {
	.scf_name      = "Resource Owner",
	.scf_nr_states = ARRAY_SIZE(owner_states),
	.scf_state     = owner_states
};

/*
 * Group lock is held
 */
static inline void owner_state_set(struct m0_rm_owner    *owner,
				   enum m0_rm_owner_state state)
{
	M0_LOG(M0_INFO, "Owner: %p, state change:[%s -> %s]\n",
	       owner, m0_sm_state_name(&owner->ro_sm, owner->ro_sm.sm_state),
	       m0_sm_state_name(&owner->ro_sm, state));
	m0_sm_state_set(&owner->ro_sm, state);
}

static inline void owner_fail(struct m0_rm_owner     *owner,
			      enum m0_rm_owner_state  state,
			      int                     rc)
{
	M0_LOG(M0_INFO, "Owner: %p, state change:[%s -> %s] err %d\n",
	       owner, m0_sm_state_name(&owner->ro_sm, owner->ro_sm.sm_state),
	       m0_sm_state_name(&owner->ro_sm, state), rc);
	m0_sm_fail(&owner->ro_sm, state, rc);
}

static bool owner_has_loans(struct m0_rm_owner *owner)
{
	return !m0_rm_ur_tlist_is_empty(&owner->ro_sublet) ||
	       !m0_rm_ur_tlist_is_empty(&owner->ro_borrowed);
}

static void owner_finalisation_check(struct m0_rm_owner *owner)
{
	M0_PRE(owner_smgrp_is_locked(owner));
	switch (owner_state(owner)) {
	case ROS_QUIESCE:
		if (owner_is_idle(owner)) {
			/*
			 * No more user-credit requests are pending.
			 * Flush the loans and cached credits.
			 */
			owner_state_set(owner, ROS_FINALISING);
			if (owner_has_loans(owner)) {
				cached_credits_clear(owner);
				if (M0_FI_ENABLED("drop_loans")) {
					owner_cleanup(owner);
					owner_state_set(owner, ROS_FINAL);
				}
			}
			owner_liquidate(owner);
			M0_POST(owner_invariant(owner));
		}
		break;
	case ROS_FINALISING:
		/*
		 * owner_liquidate() creates requests. Make sure that all those
		 * requests are processed. Once the owner is liquidated, if
		 * there are no pending loans, finalise owner. Otherwise put it
		 * in INSOLVENT state. Currently there is no recovery from
		 * INSOLVENT state.
		 */
		if (owner_is_liquidated(owner)) {
			cached_credits_clear(owner);
			if (owner_has_loans(owner))
				owner_state_set(owner, ROS_INSOLVENT);
			else if (owner->ro_user_windup)
				owner_state_set(owner, ROS_FINAL);
			else
				owner_fail(owner, ROS_DEAD_CREDITOR, -ENODEV);
			if (owner_state(owner) == ROS_INSOLVENT)
				owner_cleanup(owner);
		}
		break;
	case ROS_INSOLVENT:
	case ROS_FINAL:
		break;
	default:
		break;
	}
}

static bool owner_smgrp_is_locked(const struct m0_rm_owner *owner)
{
	struct m0_sm_group *smgrp;

	M0_PRE(owner != NULL);
	smgrp = owner_grp(owner);
	return m0_mutex_is_locked(&smgrp->s_lock);
}

M0_INTERNAL void m0_rm_owner_lock(struct m0_rm_owner *owner)
{
	m0_sm_group_lock(owner_grp(owner));
}
M0_EXPORTED(m0_rm_owner_lock);

static int m0_rm_owner_trylock(struct m0_rm_owner *owner)
{
	return m0_mutex_trylock(&owner_grp(owner)->s_lock);
}

M0_INTERNAL void m0_rm_owner_unlock(struct m0_rm_owner *owner)
{
	m0_sm_group_unlock(owner_grp(owner));
}
M0_EXPORTED(m0_rm_owner_unlock);

M0_INTERNAL void m0_rm_owner_init(struct m0_rm_owner      *owner,
				  struct m0_fid           *fid,
				  const struct m0_uint128 *group,
				  struct m0_rm_resource   *res,
				  struct m0_rm_remote     *creditor)
{
	M0_PRE(ergo(creditor != NULL,
		    creditor->rem_state >= REM_SERVICE_LOCATED));
	M0_PRE(fid != NULL);
	M0_PRE(m0_fid_tget(fid) == M0_RM_OWNER_FT);

	M0_ENTRY("owner: %p resource: %p creditor: %p",
		 owner, res, creditor);
	owner->ro_resource = res;
	m0_sm_init(&owner->ro_sm, &owner_conf, ROS_INITIAL, owner_grp(owner));
	m0_rm_owner_lock(owner);
	owner_state_set(owner, ROS_INITIALISING);
	m0_rm_owner_unlock(owner);
	owner->ro_group_id = *group;
	m0_fid_set(&owner->ro_fid, fid->f_container, fid->f_key);
	owner->ro_seq = 0;

	RM_OWNER_LISTS_FOR(owner, m0_rm_ur_tlist_init);
	resource_get(res);
	m0_rm_owner_lock(owner);
	owner_state_set(owner, ROS_ACTIVE);
	m0_rm_owner_unlock(owner);
	owner->ro_creditor = creditor;
	m0_cookie_new(&owner->ro_id);
	m0_mutex_lock(&res->r_type->rt_lock);
	m0_owners_tlink_init_at_tail(owner, &res->r_local);
	m0_mutex_unlock(&res->r_type->rt_lock);
	M0_POST(owner_invariant(owner));
	M0_POST(owner->ro_resource == res);

	M0_LEAVE();
}
M0_EXPORTED(m0_rm_owner_init);

M0_INTERNAL void m0_rm_owner_init_rfid(struct m0_rm_owner      *owner,
				       const struct m0_uint128 *group,
				       struct m0_rm_resource   *res,
				       struct m0_rm_remote     *creditor)
{
	struct m0_fid fid;

	m0_fid_tgenerate(&fid, M0_RM_OWNER_FT);
	m0_rm_owner_init(owner, &fid, group, res, creditor);
}
M0_EXPORTED(m0_rm_owner_init_rfid);

static void ha_events_handle(struct m0_rm_resource_type *rt)
{
        struct m0_rm_ha_event   *event;
        struct m0_queue_link    *q_link;
        struct m0_rm_ha_tracker *tracker;
        struct m0_rm_remote     *remote;

        m0_mutex_lock(&rt->rt_queue_guard);
        q_link = m0_queue_get(&rt->rt_ha_events);
        m0_mutex_unlock(&rt->rt_queue_guard);

	if (q_link == NULL)
		return;
        event = M0_AMB(event, q_link, rhe_link);
        tracker = event->rhe_tracker;
        remote = M0_AMB(remote, tracker, rem_tracker);

        if (tracker->rht_state == event->rhe_state)
		goto end;
        switch (event->rhe_state) {
                case M0_NC_ONLINE:
                        rm_remote_online_handler(remote);
                        break;
                case M0_NC_FAILED:
                        rm_remote_death_handler(remote);
                        break;
                case M0_NC_TRANSIENT:
                        break;
                default:
                        M0_IMPOSSIBLE("Impossible state (%d) for rm owner",
                                      event->rhe_state);
        }
end:
	m0_free(event);
}

static void credit_processor(struct m0_rm_resource_type *rt)
{
	M0_ENTRY();
	M0_PRE(rt != NULL);

	while (true) {
		m0_sm_group_lock(&rt->rt_sm_grp);
                ha_events_handle(rt);
		if (rt->rt_stop_worker) {
			m0_sm_group_unlock(&rt->rt_sm_grp);
			M0_LEAVE("RM RT agent STOPPED");
			return;
		}
		m0_sm_group_unlock(&rt->rt_sm_grp);
		m0_chan_wait(&rt->rt_sm_grp.s_clink);
	}
}

static void reserve_prio_set(struct m0_rm_reserve_prio *prio,
			     m0_time_t                  timestamp,
			     struct m0_rm_owner        *owner)
{
	M0_ASSERT(prio != NULL);
	M0_ASSERT(owner != NULL);

	prio->rrp_time  = timestamp;
	prio->rrp_owner = owner->ro_fid;
	prio->rrp_seq   = owner->ro_seq++;
}

static bool reserve_prio_is_set(struct m0_rm_reserve_prio *prio)
{
	return m0_fid_is_set(&prio->rrp_owner) && prio->rrp_time != 0;
}

M0_INTERNAL int m0_rm_owner_selfadd(struct m0_rm_owner  *owner,
				    struct m0_rm_credit *r)
{
	struct m0_rm_credit *credit_transfer;
	struct m0_rm_loan   *nominal_capital;
	int                  rc;

	M0_ENTRY("owner: %p", owner);
	M0_PRE(r != NULL);
	M0_PRE(r->cr_owner == owner);
	/* Owner must be "top-most" */
	M0_PRE(owner->ro_creditor == NULL);

	M0_ALLOC_PTR(nominal_capital);
	M0_ALLOC_PTR(credit_transfer);
	if (nominal_capital != NULL && credit_transfer != NULL) {
		/*
		 * Immediately transfer the credits. Otherwise owner will not
		 * be balanced.
		 */
		m0_rm_credit_init(credit_transfer, owner);
		rc = m0_rm_credit_copy(credit_transfer, r) ?:
		     m0_rm_loan_init(nominal_capital, r, NULL);
		if (rc == 0) {
			nominal_capital->rl_other = owner->ro_creditor;
			nominal_capital->rl_id = M0_RM_LOAN_SELF_ID;
			/* Add capital to the borrowed list. */
			m0_rm_ur_tlist_add(&owner->ro_borrowed,
					   &nominal_capital->rl_credit);
			/* Add credit transfer to the CACHED list. */
			m0_rm_ur_tlist_add(&owner->ro_owned[OWOS_CACHED],
					   credit_transfer);
		}
	} else
		rc = M0_ERR(-ENOMEM);
	if (rc != 0) {
		m0_free(nominal_capital);
		m0_free(credit_transfer);
	}
	M0_POST(ergo(rc == 0, owner_invariant(owner)));
	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_owner_selfadd);

static bool owner_is_idle(const struct m0_rm_owner *o)
{
	/*
	 * Owner is considered to be idle when all local requests are
	 * satisfied and there are no outgoing requests.
	 */
	return  m0_forall(i, ARRAY_SIZE(o->ro_incoming),
			m0_forall(j, ARRAY_SIZE(o->ro_incoming[i]),
				m0_tl_forall(m0_rm_ur, c, &o->ro_incoming[i][j],
				    incoming_state(cr2in(c)) == RI_SUCCESS))) &&
		m0_forall(k, ARRAY_SIZE(o->ro_outgoing),
			  m0_rm_ur_tlist_is_empty(&o->ro_outgoing[k]));
}

static bool owner_is_liquidated(const struct m0_rm_owner *o)
{
	/*
	 * Owner is considered to be liquidated if all locally granted requests
	 * are released and there no outgoing requests.
	 */
	return  m0_forall(i, ARRAY_SIZE(o->ro_incoming),
			m0_forall(j, ARRAY_SIZE(o->ro_incoming[i]),
				m0_rm_ur_tlist_is_empty(
					&o->ro_incoming[i][j]))) &&
		m0_forall(k, ARRAY_SIZE(o->ro_outgoing),
			  m0_rm_ur_tlist_is_empty(&o->ro_outgoing[k]));
}

static void owner_liquidate(struct m0_rm_owner *o)
{
	struct m0_rm_credit   *credit;
	struct m0_rm_loan     *loan;
	struct m0_rm_incoming *in;
	int                    rc = 0;

	M0_ENTRY("owner: %p", o);
	/*
	 * While processing the queues, if -ENOMEM or other error occurs
	 * then the owner will be in a limbo. A force cleanup remains one of
	 * the options.
	 */
	m0_tl_for (m0_rm_ur, &o->ro_sublet, credit) {
		M0_ALLOC_PTR(in);
		if (in == NULL)
			break;
		m0_rm_incoming_init(in, o, M0_RIT_LOCAL,
				    RIP_NONE, RIF_MAY_REVOKE);
		in->rin_priority = 0;
		in->rin_ops = &windup_incoming_ops;
		reserve_prio_set(&in->rin_reserve, m0_time_now(), o);
		/*
		 * This is convoluted. Now that user incoming requests have
		 * drained, we add our incoming requests for REVOKE and CANCEL
		 * processing to the incoming queue.
		 *
		 * If there are any errors then loans (sublets, borrows) will
		 * remain in the list. Eventually owner will enter INSOLVENT
		 * state.
		 */
		M0_ASSERT(m0_rm_credit_bob_check(credit));
		rc = m0_rm_credit_copy(&in->rin_want, credit);
		if (rc == 0) {
			m0_rm_ur_tlist_add(
			    &o->ro_incoming[in->rin_priority][OQS_EXCITED],
			    &in->rin_want);
		} else
			break;
	} m0_tl_endfor;

	/*
	 * Call conflict callback for all held credits. Users are expected to
	 * put them, so owner can continue with windup process.
	 */
	m0_tl_for (m0_rm_ur, &o->ro_owned[OWOS_HELD], credit) {
		conflict_notify(credit);
	} m0_tl_endfor;

	m0_tl_for (m0_rm_ur, &o->ro_borrowed, credit) {
		M0_ASSERT(m0_rm_credit_bob_check(credit));
		loan = bob_of(credit, struct m0_rm_loan, rl_credit, &loan_bob);
		if (loan->rl_id == M0_RM_LOAN_SELF_ID ||
		    (o->ro_creditor != NULL && o->ro_creditor->rem_dead)) {
			m0_rm_ur_tlist_del(credit);
			m0_rm_loan_fini(loan);
			m0_free(loan);
		} else
			cancel_send(loan);
	} m0_tl_endfor;
	owner_balance(o);
	M0_LEAVE();
}

static void owner_cleanup(struct m0_rm_owner *o)
{
	struct m0_rm_credit *credit;
	struct m0_rm_loan   *loan;

	M0_PRE(owner_is_idle(o));

	m0_tl_for (m0_rm_ur, &o->ro_sublet, credit) {
		M0_ASSERT(m0_rm_credit_bob_check(credit));
		loan = bob_of(credit, struct m0_rm_loan, rl_credit, &loan_bob);
		m0_rm_ur_tlist_del(credit);
		m0_rm_loan_fini(loan);
		m0_free(loan);
	} m0_tl_endfor;

	m0_tl_for (m0_rm_ur, &o->ro_borrowed, credit) {
		M0_ASSERT(m0_rm_credit_bob_check(credit));
		loan = bob_of(credit, struct m0_rm_loan, rl_credit, &loan_bob);
		m0_rm_ur_tlist_del(credit);
		m0_rm_loan_fini(loan);
		m0_free(loan);
	} m0_tl_endfor;
}

M0_INTERNAL int m0_rm_owner_timedwait(struct m0_rm_owner *owner,
				      uint64_t            state,
				      const m0_time_t     abs_timeout)
{
	int rc;

	m0_rm_owner_lock(owner);
	rc = m0_sm_timedwait(&owner->ro_sm, state, abs_timeout);
	m0_rm_owner_unlock(owner);
	return rc;
}

M0_INTERNAL void m0_rm_owner_creditor_reset(struct m0_rm_owner  *owner,
					    struct m0_rm_remote *creditor)
{
	M0_ENTRY();
	M0_PRE(owner_state(owner) == ROS_DEAD_CREDITOR ||
	       owner->ro_creditor == NULL);
	m0_rm_owner_lock(owner);
	owner->ro_creditor = creditor;
	if (owner_state(owner) == ROS_DEAD_CREDITOR)
		owner_state_set(owner, ROS_ACTIVE);
	m0_rm_owner_unlock(owner);
	M0_LEAVE();
}


static void owner_windup_locked(struct m0_rm_owner *owner)
{
	M0_PRE(owner_smgrp_is_locked(owner));
	/*
	 * Put the owner in ROS_QUIESCE. This will prevent any new
	 * incoming requests on it.
	 */
	owner_state_set(owner, ROS_QUIESCE);
	owner_balance(owner);
}

M0_INTERNAL void m0_rm_owner_windup(struct m0_rm_owner *owner)
{
	M0_ENTRY("owner %p", owner);
	m0_rm_owner_lock(owner);
	owner->ro_user_windup = true;
	if (owner_state(owner) == ROS_ACTIVE)
		owner_windup_locked(owner);
	else if (owner_state(owner) == ROS_DEAD_CREDITOR)
		owner_state_set(owner, ROS_FINAL);
	m0_rm_owner_unlock(owner);
	M0_LEAVE();
}

M0_INTERNAL void m0_rm_owner_fini(struct m0_rm_owner *owner)
{
	struct m0_rm_resource *res = owner->ro_resource;

	M0_ENTRY("owner: %p", owner);
	M0_PRE(owner_invariant(owner));
	M0_PRE(M0_IN(owner_state(owner),
		     (ROS_FINAL, ROS_INSOLVENT, ROS_DEAD_CREDITOR)));

	m0_mutex_lock(&res->r_type->rt_lock);
	m0_owners_tlink_del_fini(owner);
	m0_mutex_unlock(&res->r_type->rt_lock);
	RM_OWNER_LISTS_FOR(owner, m0_rm_ur_tlist_fini);
	owner->ro_resource = NULL;
	owner->ro_creditor = NULL;
	resource_put(res);

	M0_LEAVE();
}
M0_EXPORTED(m0_rm_owner_fini);

M0_INTERNAL void m0_rm_credit_init(struct m0_rm_credit *credit,
				   struct m0_rm_owner  *owner)
{
	M0_ENTRY("credit: %p with owner: %p", credit, owner);
	M0_PRE(credit != NULL);
	M0_PRE(owner->ro_resource->r_ops != NULL);
	M0_PRE(owner->ro_resource->r_ops->rop_credit_init != NULL);

	credit->cr_datum = 0;
	credit->cr_group_id = m0_rm_no_group;
	m0_rm_ur_tlink_init(credit);
	pr_tlist_init(&credit->cr_pins);
	m0_rm_credit_bob_init(credit);
	credit->cr_owner = owner;
	owner->ro_resource->r_ops->rop_credit_init(owner->ro_resource, credit);

	M0_POST(credit->cr_ops != NULL);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_credit_init);

M0_INTERNAL void m0_rm_credit_fini(struct m0_rm_credit *credit)
{
	M0_ENTRY("credit: %p", credit);
	M0_PRE(credit != NULL);

	m0_rm_ur_tlink_fini(credit);
	pr_tlist_fini(&credit->cr_pins);
	m0_rm_credit_bob_fini(credit);
	credit->cr_ops->cro_free(credit);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_credit_fini);

static struct m0_sm_state_descr inc_states[] = {
	[RI_INITIALISED] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Initialised",
		.sd_allowed   = M0_BITS(RI_CHECK, RI_FAILURE, RI_FINAL)
	},
	[RI_CHECK] = {
		.sd_name      = "Check",
		.sd_allowed   = M0_BITS(RI_SUCCESS, RI_FAILURE, RI_WAIT)
	},
	[RI_SUCCESS] = {
		.sd_name      = "Success",
		.sd_allowed   = M0_BITS(RI_RELEASED)
	},
	[RI_FAILURE] = {
		.sd_flags     = M0_SDF_FAILURE,
		.sd_name      = "Failure",
		.sd_allowed   = M0_BITS(RI_FINAL)
	},
	[RI_WAIT] = {
		.sd_name      = "Wait",
		.sd_allowed   = M0_BITS(RI_WAIT, RI_FAILURE, RI_CHECK)
	},
	[RI_RELEASED] = {
		.sd_name      = "Released",
		.sd_allowed   = M0_BITS(RI_FINAL)
	},
	[RI_FINAL] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Final",
	}
};

static const struct m0_sm_conf inc_conf = {
	.scf_name      = "Incoming Request",
	.scf_nr_states = ARRAY_SIZE(inc_states),
	.scf_state     = inc_states
};

static struct m0_rm_incoming *cr2in(const struct m0_rm_credit *cr)
{
	return container_of(cr, struct m0_rm_incoming, rin_want);
}

static inline void incoming_state_set(struct m0_rm_incoming     *in,
				      enum m0_rm_incoming_state  state)
{
	/*
	 * Incoming sm group is owner's group, which in turn is the owner
	 * resource type's group. It is not always safe to access the group via
	 * owner, because under certain circumstances incoming object may live
	 * longer than owner does. An example may be incoming CREDIT REVOKE
	 * request arrived right before the referred owner is to pass away due
	 * to natural reasons.
	 */
	M0_PRE(m0_sm_group_is_locked(in->rin_sm.sm_grp));
	M0_LOG(M0_INFO, "Incoming req: %p, state change:[%s -> %s]\n",
	       in, m0_sm_state_name(&in->rin_sm, in->rin_sm.sm_state),
	       m0_sm_state_name(&in->rin_sm, state));
	m0_sm_state_set(&in->rin_sm, state);
}

M0_INTERNAL void m0_rm_incoming_init(struct m0_rm_incoming      *in,
				     struct m0_rm_owner         *owner,
				     enum m0_rm_incoming_type    type,
				     enum m0_rm_incoming_policy  policy,
				     uint64_t                    flags)
{
	M0_ENTRY("incoming: %p for owner: %p", in, owner);
	M0_PRE(in != NULL);

	M0_SET0(in);
	m0_sm_init(&in->rin_sm, &inc_conf, RI_INITIALISED, owner_grp(owner));
	in->rin_type   = type;
	in->rin_policy = policy;
	in->rin_flags  = flags;
	pi_tlist_init(&in->rin_pins);
	m0_rm_credit_init(&in->rin_want, owner);
	m0_rm_incoming_bob_init(in);
	M0_POST(incoming_invariant(in));
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_incoming_init);

M0_INTERNAL void incoming_surrender(struct m0_rm_incoming *in)
{
	incoming_state_set(in, RI_RELEASED);
	m0_rm_ur_tlist_del(&in->rin_want);
	M0_ASSERT(pi_tlist_is_empty(&in->rin_pins));
}

M0_INTERNAL void internal_incoming_fini(struct m0_rm_incoming *in)
{
	M0_ENTRY();
	m0_sm_fini(&in->rin_sm);
	m0_rm_incoming_bob_fini(in);
	m0_rm_credit_fini(&in->rin_want);
	pi_tlist_fini(&in->rin_pins);
	M0_LEAVE();
}

M0_INTERNAL void m0_rm_incoming_fini(struct m0_rm_incoming *in)
{
	M0_ENTRY();
	M0_PRE(incoming_invariant(in));
	/*
	 * This is to lock sm group, which in fact is owner group, which in turn
	 * is resource type group.
	 *
	 * Please note, locking via owner object may be not always safe here due
	 * to races between finalisation of incoming object and owner it's bound
	 * to. Owner may appear lockable at the beginning, but being already
	 * finalised when it is time to unlock sm.
	 */
	m0_sm_group_lock(in->rin_sm.sm_grp);
	M0_PRE(M0_IN(incoming_state(in),
		    (RI_INITIALISED, RI_FAILURE, RI_RELEASED)));
	incoming_state_set(in, RI_FINAL);
	internal_incoming_fini(in);
	m0_sm_group_unlock(in->rin_sm.sm_grp);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_incoming_fini);

/*
 * Impossible condition.
 */
static void windup_incoming_conflict(struct m0_rm_incoming *in)
{
	M0_IMPOSSIBLE("Conflict not possible during windup");
}

static void windup_incoming_complete(struct m0_rm_incoming *in, int32_t rc)
{
	M0_ENTRY();
	if (rc == 0) {
		incoming_release(in);
		incoming_surrender(in);
	}
	M0_PRE(incoming_invariant(in));
	M0_PRE(M0_IN(incoming_state(in),
	       (RI_INITIALISED, RI_FAILURE, RI_RELEASED)));
	incoming_state_set(in, RI_FINAL);
	internal_incoming_fini(in);

	m0_free(in);
	M0_LEAVE();
}

M0_INTERNAL int m0_rm_outgoing_init(struct m0_rm_outgoing    *out,
				    enum m0_rm_outgoing_type  req_type,
				    struct m0_rm_remote      *other,
				    struct m0_rm_credit      *credit)
{
	M0_ENTRY("outgoing: %p", out);
	M0_PRE(out != NULL);

	out->rog_rc = 0;
	out->rog_type = req_type;
	out->rog_sent = false;
	m0_rm_outgoing_bob_init(out);
	return M0_RC(m0_rm_loan_init(&out->rog_want, credit, other));
}
M0_EXPORTED(m0_rm_outgoing_init);

M0_INTERNAL void m0_rm_outgoing_fini(struct m0_rm_outgoing *out)
{
	M0_ENTRY("outgoing: %p", out);
	M0_PRE(out != NULL);
	m0_rm_loan_fini(&out->rog_want);
	m0_rm_outgoing_bob_fini(out);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_outgoing_fini);

static int loan_dup(const struct m0_rm_loan  *src_loan,
		    struct m0_rm_loan       **dest_loan)
{
	return m0_rm_loan_alloc(dest_loan, &src_loan->rl_credit,
				src_loan->rl_other);
}

M0_INTERNAL int m0_rm_loan_alloc(struct m0_rm_loan         **loan,
				 const struct m0_rm_credit  *credit,
				 struct m0_rm_remote        *creditor)
{
	struct m0_rm_loan *new_loan;
	int		   rc = -ENOMEM;

	M0_ENTRY("loan credit: %llu", (long long unsigned) credit->cr_datum);
	M0_PRE(loan != NULL);
	M0_PRE(credit != NULL);

	M0_ALLOC_PTR(new_loan);
	if (new_loan != NULL) {
		rc = m0_rm_loan_init(new_loan, credit, creditor);
		if (rc != 0)
			m0_free0(&new_loan);
	}

	*loan = new_loan;
	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_loan_alloc);

/*
 * Allocates a new loan and calculates the difference between
 * loan->rl_credit and credit.
 */
static int remnant_loan_get(const struct m0_rm_loan    *loan,
			    const struct m0_rm_credit  *credit,
			    struct m0_rm_loan         **remnant_loan)
{
	struct m0_rm_loan *new_loan;
	int		   rc;

	M0_ENTRY();
	M0_PRE(loan != NULL);
	M0_PRE(credit != NULL);
	M0_PRE(remnant_loan != NULL);
	M0_LOG(M0_DEBUG, "split loan credit: %llu with credit: %llu",
	       (long long unsigned) loan->rl_credit.cr_datum,
	       (long long unsigned) credit->cr_datum);

	rc = loan_dup(loan, &new_loan) ?:
		credit_diff(&new_loan->rl_credit, credit);
	if (rc != 0 && new_loan != NULL) {
		m0_rm_loan_fini(new_loan);
		m0_free0(&new_loan);
	}
	*remnant_loan = new_loan;
	return M0_RC(rc);
}

M0_INTERNAL int m0_rm_loan_init(struct m0_rm_loan         *loan,
				const struct m0_rm_credit *credit,
				struct m0_rm_remote       *creditor)
{
	M0_PRE(loan != NULL);
	M0_PRE(credit != NULL);

	M0_ENTRY("loan: %p", loan);
	loan->rl_id = 0;
	m0_cookie_new(&loan->rl_id);
	m0_rm_credit_init(&loan->rl_credit, credit->cr_owner);
	m0_rm_loan_bob_init(loan);
	loan->rl_other = creditor;
	if (loan->rl_other != NULL)
		resource_get(loan->rl_other->rem_resource);

	return M0_RC(m0_rm_credit_copy(&loan->rl_credit, credit));
}
M0_EXPORTED(m0_rm_loan_init);

M0_INTERNAL void m0_rm_loan_fini(struct m0_rm_loan *loan)
{
	M0_PRE(loan != NULL);

	M0_ENTRY("loan: %p", loan);
	m0_rm_credit_fini(&loan->rl_credit);
	if (loan->rl_other != NULL)
		resource_put(loan->rl_other->rem_resource);
	loan->rl_id = 0;
	m0_rm_loan_bob_fini(loan);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_loan_fini);

static void pending_outgoing_send(struct m0_rm_owner *owner,
				  struct m0_clink    *link)
{
	struct m0_rm_outgoing *outgoing;
	struct m0_rm_credit   *credit;

	M0_ENTRY("owner: %p link: %p", owner, link);
	M0_PRE(owner != NULL);
	M0_PRE(link != NULL);
	M0_PRE(owner_smgrp_is_locked(owner));

	m0_tl_for(m0_rm_ur, &owner->ro_outgoing[OQS_GROUND], credit) {
		M0_ASSERT(m0_rm_credit_bob_check(credit));
		outgoing = bob_of(credit, struct m0_rm_outgoing,
				  rog_want.rl_credit, &outgoing_bob);
		if (!outgoing->rog_sent &&
		    &outgoing->rog_want.rl_other->rem_rev_sess_clink == link)
			m0_rm_outgoing_send(outgoing);
	} m0_tl_endfor;
	M0_LEAVE();
}

static bool rev_session_clink_cb(struct m0_clink *link)
{
	struct m0_rm_remote   *remote;
	struct m0_rm_resource *resource;
	struct m0_rm_owner    *owner;
	bool                   busy;

	M0_ENTRY("link: %p", link);
	remote = bob_of(link, struct m0_rm_remote,
			rem_rev_sess_clink, &rem_bob);
	resource = remote->rem_resource;

	if (m0_rpc_session_status(remote->rem_session) != 0)
		/* The connection has failed. Stop further processing. */
		return true;

	/*
	 * Do not break RM lock ordering.
	 * Use trylock-repeat cycle to avoid deadlocks.
	 *
	 * Problem with possible multiple sending of outgoing requests
	 * for the same owner is solved by m0_rm_outgoing::rog_sent flag.
	 */
	do {
		busy = false;
		m0_mutex_lock(&resource->r_type->rt_lock);
		m0_tl_for(m0_owners, &resource->r_local, owner) {
			busy = m0_rm_owner_trylock(owner);
			if (!busy) {
				pending_outgoing_send(owner, link);
				/*
				 * Use m0_mutex_unlock() directly insted of
				 * m0_rm_owner_unlock(). The latter causes asts
				 * for owner sm group to run, which can lock
				 * already held resource type lock.
				 */
				/**
				 * @todo This would be fixed, when RM will be
				 * using normal locality sm groups, rather than
				 * per-resource-type groups */
				m0_mutex_unlock(&owner_grp(owner)->s_lock);
			} else
				break;
		} m0_tl_endfor;
		m0_mutex_unlock(&resource->r_type->rt_lock);
	} while (busy);

	M0_LEAVE();
	return true;
}

M0_INTERNAL struct m0_rm_remote *
m0_rm_remote_find(struct m0_rm_remote_incoming *rem_in)
{
	struct m0_cookie      *cookie = &rem_in->ri_rem_owner_cookie;
	struct m0_rm_resource *res = incoming_to_resource(&rem_in->ri_incoming);

	M0_PRE(cookie != NULL);
	M0_PRE(res != NULL);
	return m0_tl_find(m0_remotes, other, &res->r_remote,
			  m0_cookie_is_eq(&other->rem_cookie, cookie));
}

M0_INTERNAL void m0_rm_remote_init(struct m0_rm_remote   *rem,
				   struct m0_rm_resource *res)
{
	M0_PRE(M0_IS0(rem));

	M0_ENTRY("remote: %p", rem);
	rem->rem_state = REM_INITIALISED;
	rem->rem_resource = res;
	rem->rem_cookie = M0_COOKIE_NULL;
	m0_chan_init(&rem->rem_signal, &res->r_type->rt_lock);
	m0_clink_init(&rem->rem_rev_sess_clink, rev_session_clink_cb);
	m0_rm_ha_tracker_init(&rem->rem_tracker, rm_on_remote_death_cb);
	m0_remotes_tlink_init(rem);
	m0_rm_remote_bob_init(rem);
	resource_get(res);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_remote_init);

M0_INTERNAL void m0_rm_remote_fini(struct m0_rm_remote *rem)
{
	M0_ENTRY("remote: %p", rem);
	M0_PRE(rem != NULL);
	M0_PRE(M0_IN(rem->rem_state, (REM_INITIALISED,
				      REM_SERVICE_LOCATED,
				      REM_OWNER_LOCATED)));
	/*
	 * It is possible that reverse session is not yet established,
	 * because no revoke requests were sent to remote owner.
	 */
	m0_clink_cleanup(&rem->rem_rev_sess_clink);
	m0_clink_fini(&rem->rem_rev_sess_clink);
	rem->rem_state = REM_FREED;
	m0_chan_fini_lock(&rem->rem_signal);

	m0_rm_ha_unsubscribe_lock(&rem->rem_tracker);
	m0_rm_ha_tracker_fini(&rem->rem_tracker);
	/*
	 * Do nothing with rem->rem_session. If it was used to
	 * borrow credits, then session is managed by user. If
	 * it was used to revoke credits, then session is stored
	 * in RPC service rps_rev_conns list and can be used by
	 * other mero entities.
	 */
	m0_remotes_tlink_fini(rem);
	resource_put(rem->rem_resource);
	m0_rm_remote_bob_fini(rem);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_remote_fini);

static void cached_credits_clear(struct m0_rm_owner *owner)
{
	struct m0_rm_credit *credit;

	M0_ENTRY("owner: %p", owner);
	m0_tl_teardown(m0_rm_ur, &owner->ro_owned[OWOS_CACHED], credit) {
		m0_rm_credit_fini(credit);
		m0_free(credit);
	}
	M0_LEAVE();
}

/*
 * Remove the OWOS_CACHED credits that match incoming credits. If the credit(s)
 * completely intersects the incoming credit, remove it(them) from the cache.
 * If the CACHED credit partly intersects with the incoming credit, retain the
 * difference in the CACHE.
 */
static int cached_credits_remove(struct m0_rm_incoming *in)
{
	struct m0_rm_pin    *pin;
	struct m0_rm_credit *credit;
	struct m0_rm_credit *remnant_credit;
	struct m0_rm_owner  *owner = in->rin_want.cr_owner;
	struct m0_tl	     retain_list;
	struct m0_tl	     remove_list;
	int		     rc = 0;

	M0_ENTRY("owner: %p credit: %llu", owner,
		 (long long unsigned) INCOMING_CREDIT(in));
	/* Credits can be removed for remote requests */
	M0_PRE(in->rin_type != M0_RIT_LOCAL);

	m0_rm_ur_tlist_init(&retain_list);
	m0_rm_ur_tlist_init(&remove_list);
	m0_tl_for (pi, &in->rin_pins, pin) {
		M0_ASSERT(m0_rm_pin_bob_check(pin));
		M0_ASSERT(pin->rp_flags == M0_RPF_PROTECT);
		credit = pin->rp_credit;

		pin_del(pin);
		M0_ASSERT(credit_pin_nr(credit, M0_RPF_TRACK) == 0);
		if (!credit->cr_ops->cro_intersects(credit, &in->rin_want))
			m0_rm_ur_tlist_move(&retain_list, credit);
		else {
			m0_rm_ur_tlist_move(&remove_list, credit);
			/*
			 * The cached credit does not completely intersect
			 * incoming credit.
			 *
			 * Make a copy of the cached credit and calculate
			 * the difference with incoming credit. Store
			 * the difference in the remnant credit.
			 */
			rc = remnant_credit_get(credit, &in->rin_want,
					        &remnant_credit);
			if (rc == 0) {
				if (credit_is_empty(remnant_credit))
					m0_rm_ur_tlist_add(&remove_list,
							   remnant_credit);
				else
					m0_rm_ur_tlist_add(&retain_list,
							   remnant_credit);
			}
		}

	} m0_tl_endfor;

	/*
	 * On successful completion, remove the credits from the "remove-list"
	 * and move the remnant credits to the OWOS_CACHED. Do the opposite
	 * on failure.
	 */
	m0_tl_teardown(m0_rm_ur, rc ? &retain_list : &remove_list, credit) {
		     m0_rm_credit_fini(credit);
		     m0_free(credit);
	}

	m0_tl_for (m0_rm_ur, rc ? &remove_list : &retain_list, credit) {
		m0_rm_ur_tlist_move(&owner->ro_owned[OWOS_CACHED], credit);
	} m0_tl_endfor;

	m0_rm_ur_tlist_fini(&retain_list);
	m0_rm_ur_tlist_fini(&remove_list);
	return M0_RC(rc);
}

static inline struct m0_rm_resource_type *
credit_to_resource_type(struct m0_rm_credit *credit) {
	return credit->cr_owner->ro_resource->r_type;
}

static inline struct m0_rm_resource_type *
rem_incoming_to_resource_type(struct m0_rm_remote_incoming *rem_in) {
	return credit_to_resource_type(&rem_in->ri_incoming.rin_want);
}

M0_INTERNAL int m0_rm_borrow_commit(struct m0_rm_remote_incoming *rem_in)
{
	struct m0_rm_incoming *in     = &rem_in->ri_incoming;
	struct m0_rm_owner    *o      = in->rin_want.cr_owner;
	struct m0_rm_loan     *loan   = NULL;
	struct m0_rm_remote   *debtor = NULL;
	int                    rc;

	M0_ENTRY("owner: %p credit: %llu", o,
		 (long long unsigned) INCOMING_CREDIT(in));
	M0_PRE(in->rin_type == M0_RIT_BORROW);

	/*
	 * Allocate loan and copy the credit (to be borrowed).
	 * Clear the credits cache and remove incoming credits from the cache.
	 * If everything succeeds add loan to the sublet list.
	 *
	 * If there are no pins, sublet within the same group has been
	 * granted.
	 */
	debtor = m0_rm_remote_find(rem_in);
	M0_ASSERT(debtor != NULL);
	rc = m0_rm_loan_alloc(&loan, &in->rin_want, debtor) ?:
		pi_tlist_is_empty(&in->rin_pins) ? 0 :
		cached_credits_remove(in);
	/* @todo */
	loan->rl_credit.cr_group_id = rem_in->ri_group_id;
	if (rc != 0 && loan != NULL) {
		m0_rm_loan_fini(loan);
		m0_free(loan);
	} else if (rc == 0) {
		/*
		 * Store the loan in the sublet list.
		 */
		m0_rm_ur_tlist_add(&o->ro_sublet, &loan->rl_credit);
		m0_cookie_init(&loan->rl_cookie, &loan->rl_id);
		rem_in->ri_loan_cookie = loan->rl_cookie;
	}

	M0_POST(owner_invariant(o));
	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_borrow_commit);

M0_INTERNAL int m0_rm_revoke_commit(struct m0_rm_remote_incoming *rem_in)
{
	struct m0_rm_incoming *in       = &rem_in->ri_incoming;
	struct m0_rm_owner    *owner    = in->rin_want.cr_owner;
	struct m0_rm_loan     *brwd_loan = NULL;
	struct m0_rm_loan     *remnant_loan;
	struct m0_rm_loan     *add_loan = NULL;
	struct m0_rm_loan     *remove_loan = NULL;
	struct m0_rm_credit   *credit;
	struct m0_cookie      *cookie;
	int                    rc         = 0;

	M0_ENTRY("owner: %p credit: %llu", owner,
		 (long long unsigned) INCOMING_CREDIT(in));
	M0_PRE(in->rin_type == M0_RIT_REVOKE);
	M0_PRE(!m0_rm_ur_tlist_is_empty(&owner->ro_borrowed));

	cookie = &rem_in->ri_loan_cookie;
	/*
	 * Clear the credits cache and remove incoming credits from the cache.
	 *
	 * Check the difference between the borrowed credits and the revoke
	 * credits. If the revoke fully intersects the previously borrowed
	 * credit, remove it from the list.
	 *
	 * If it's a partial revoke, credit_diff() will retain the remnant
	 * borrowed credit. cached_credits_remove() will leave
	 * remnant credit in the CACHE.
	 */
	/*
	 * Find the matching loan and remove it from the borrowed list.
	 */
	credit = m0_tl_find(m0_rm_ur, credit, &owner->ro_borrowed,
			    (brwd_loan = bob_of(credit, struct m0_rm_loan,
						rl_credit, &loan_bob),
			     m0_cookie_is_eq(&brwd_loan->rl_cookie, cookie)));

	M0_ASSERT(brwd_loan != NULL);
	M0_ASSERT(credit != NULL);

	/*
	 * Check if there is partial revoke.
	 * Also remove the corresponding credit from the OWOS_CACHED list.
	 */
	rc = remnant_loan_get(brwd_loan, &in->rin_want, &remnant_loan) ?:
		cached_credits_remove(in);

	if (rc == 0) {
		m0_rm_ur_tlist_del(credit);
		m0_rm_loan_fini(brwd_loan);
		m0_free(brwd_loan);
		if (credit_is_empty(&remnant_loan->rl_credit))
			remove_loan = remnant_loan;
		else
			add_loan = remnant_loan;
	} else
		remove_loan = remnant_loan;

	if (add_loan != NULL)
		m0_rm_ur_tlist_add(&owner->ro_borrowed, &add_loan->rl_credit);

	if (remove_loan != NULL) {
		m0_rm_loan_fini(remove_loan);
		m0_free(remove_loan);
	}

	M0_POST(owner_invariant(owner));
	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_revoke_commit);

/**
 * @name Owner state machine
 *
 * m0_rm_owner and m0_rm_incoming together form a state machine where basic
 * resource management functionality is implemented.
 *
 * This state machine reacts to the following external events:
 *
 *     - an incoming request from a local user;
 *
 *     - an incoming loan request from another domain;
 *
 *     - an incoming revocation request from another domain;
 *
 *     - local user releases a pin on a credit (as a by-product of destroying an
 *       incoming request);
 *
 *     - completion of an outgoing request to another domain (including a
 *       timeout or a failure).
 *
 * Any event is processed in a uniform manner:
 *
 *     - m0_rm_owner::ro_sm_grp Group lock is taken;
 *
 *     - m0_rm_owner lists are updated to reflect the event, see details
 *       below. This temporarily violates the owner_invariant();
 *
 *     - owner_balance() is called to restore the invariant, this might create
 *       new imbalances and go through several iterations;
 *
 *     - m0_rm_owner::ro_sm_grp Group lock is released.
 *
 * Event handling is serialised by the owner lock. It is not legal to wait for
 * networking or IO events under this lock.
 *
 */
/** @{ */

/*
 * Returns:
 * True - when both the groups are different or they are m0_rm_no_group.
 * False - when both the groups are same.
 */

static bool credit_group_conflict(const struct m0_uint128 *g1,
			          const struct m0_uint128 *g2)
{
	return (m0_uint128_eq(g1, g2) &&
		m0_uint128_eq(g1, &m0_rm_no_group)) || !m0_uint128_eq(g1, g2);
}

static void incoming_queue(struct m0_rm_owner *owner, struct m0_rm_incoming *in)
{
	/*
	 * Mark incoming request "excited". owner_balance() will process it.
	 */
	m0_rm_ur_tlist_add(&owner->ro_incoming[in->rin_priority][OQS_EXCITED],
			   &in->rin_want);
	in->rin_req_time = in->rin_want.cr_get_time = m0_time_now();

	if (in->rin_type == M0_RIT_LOCAL) {
		M0_ASSERT(!reserve_prio_is_set(&in->rin_reserve));
		reserve_prio_set(&in->rin_reserve, in->rin_req_time, owner);
	}
	M0_ASSERT(reserve_prio_is_set(&in->rin_reserve));
	owner_balance(owner);
}

/**
   External resource manager entry point: request a credit from the resource
   owner.
 */
M0_INTERNAL void m0_rm_credit_get(struct m0_rm_incoming *in)
{
	struct m0_rm_owner *owner = in->rin_want.cr_owner;

	M0_ENTRY("owner: %p credit: %llu", owner,
		 (long long unsigned) INCOMING_CREDIT(in));
	M0_PRE(IS_IN_ARRAY(in->rin_priority, owner->ro_incoming));
	M0_PRE(in->rin_sm.sm_rc == 0);
	M0_PRE(in->rin_rc == 0);
	M0_PRE(pi_tlist_is_empty(&in->rin_pins));

	m0_rm_owner_lock(owner);
	/*
	 * This check will make sure that new requests are added
	 * while owner is in ACTIVE state. This will take care
	 * of races between owner state transition and credit requests.
	 */
	if (owner_state(owner) == ROS_ACTIVE)
		incoming_queue(owner, in);
	else
		/*
		 * Reject all credit requests with -EAGAIN error until owner is
		 * in one of its final states. During finalisation owner still
		 * possesses some credits, but can't grant them. Let client to
		 * re-request them till they are returned to creditor and
		 * overall credits state in cluster is more "stable".
		 * For example, if creditor revokes loan during owner
		 * finalisation, then race is possible between 'REVOKE' and
		 * 'CANCEL' requests. Creditor will re-send 'REVOKE' requests
		 * until 'CANCEL' is received.
		 */
		incoming_complete(in, M0_IN(owner_state(owner), (ROS_FINAL,
				    ROS_INSOLVENT, ROS_DEAD_CREDITOR)) ?
							     -ENODEV : -EAGAIN);
	m0_rm_owner_unlock(owner);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_credit_get);

M0_INTERNAL void m0_rm_credit_put(struct m0_rm_incoming *in)
{
	struct m0_rm_owner *owner = in->rin_want.cr_owner;

	M0_ENTRY("owner: %p credit: %llu", owner,
		 (long long unsigned) INCOMING_CREDIT(in));
	m0_rm_owner_lock(owner);
	incoming_release(in);
	incoming_surrender(in);
	/*
	 * Release of this credit may excite other waiting incoming-requests.
	 * Hence, call owner_balance() to process them.
	 */
	owner_balance(owner);
	m0_rm_owner_unlock(owner);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_credit_put);

/*
 * After successful completion of incoming request, move OWOS_CACHED credits
 * to OWOS_HELD credits.
 */
static int cached_credits_hold(struct m0_rm_incoming *in)
{
	enum m0_rm_owner_owned_state  ltype;
	struct m0_rm_pin             *pin;
	struct m0_rm_owner           *owner = in->rin_want.cr_owner;
	struct m0_rm_credit          *credit;
	struct m0_rm_credit          *held_credit;
	struct m0_rm_credit           rest;
	struct m0_tl                  transfers;
	int                           rc;

	M0_ENTRY("owner: %p credit: %llu", owner,
		 (long long unsigned) INCOMING_CREDIT(in));
	/* Only local request can hold the credits */
	M0_PRE(in->rin_type == M0_RIT_LOCAL);

	m0_rm_credit_init(&rest, in->rin_want.cr_owner);
	rc = m0_rm_credit_copy(&rest, &in->rin_want);
	if (rc != 0)
		goto out;

	m0_rm_ur_tlist_init(&transfers);
	m0_tl_for (pi, &in->rin_pins, pin) {
		M0_ASSERT(pin->rp_flags == M0_RPF_PROTECT);
		credit = pin->rp_credit;
		M0_ASSERT(credit != NULL);
		M0_ASSERT(credit->cr_ops != NULL);
		M0_ASSERT(credit->cr_ops->cro_is_subset != NULL);
		M0_ASSERT(credit_intersects(&rest, credit));

		/* If the credit is already part of HELD list, skip it */
		if (credit_pin_nr(credit, M0_RPF_PROTECT) > 1) {
			rc = credit_diff(&rest, credit);
			if (rc != 0)
				break;
			else
				continue;
		}

		/*
		 * Check if the cached credit is a subset (including a
		 * proper subset) of incoming credit (request).
		 */
		if (credit->cr_ops->cro_is_subset(credit, &rest)) {
			/* Move the subset from CACHED list to HELD list */
			m0_rm_ur_tlist_move(&transfers, credit);
			rc = credit_diff(&rest, credit);
			if (rc != 0)
				break;
		} else {
			M0_ALLOC_PTR(held_credit);
			if (held_credit == NULL) {
				rc = M0_ERR(-ENOMEM);
				break;
			}

			m0_rm_credit_init(held_credit, owner);
			/*
			 * If incoming credit partly intersects, then move
			 * intersection to the HELD list. Retain the difference
			 * in the CACHED list. This may lead to fragmentation of
			 * credits.
			 */
			rc = credit->cr_ops->cro_disjoin(credit, &rest,
							held_credit);
			if (rc != 0) {
				m0_rm_credit_fini(held_credit);
				m0_free(held_credit);
				break;
			}
			m0_rm_ur_tlist_add(&transfers, held_credit);
			rc = credit_diff(&rest, held_credit);
			if (rc != 0)
				break;
			m0_rm_pin_add(in, held_credit, M0_RPF_PROTECT);
			pin_del(pin);
		}

	} m0_tl_endfor;

	M0_POST(ergo(rc == 0, credit_is_empty(&rest)));
	/*
	 * Only cached credits are part of transfer list.
	 * On success, move the credits to OWOS_HELD list. Otherwise move
	 * them back OWOS_CACHED list.
	 */
	ltype = rc ? OWOS_CACHED : OWOS_HELD;
	m0_tl_for (m0_rm_ur, &transfers, credit) {
		m0_rm_ur_tlist_move(&owner->ro_owned[ltype], credit);
	} m0_tl_endfor;

	m0_rm_ur_tlist_fini(&transfers);

out:
	m0_rm_credit_fini(&rest);
	return M0_RC(rc);
}

/**
 * Deletes all pins set by a given incoming request with given pin flags.
 */
static void incoming_pins_del(struct m0_rm_incoming *in, uint32_t flags)
{
	struct m0_rm_pin *pin;

	m0_tl_for (pi, &in->rin_pins, pin) {
		M0_ASSERT(m0_rm_pin_bob_check(pin));
		if (pin->rp_flags & flags)
			pin_del(pin);
	} m0_tl_endfor;
}

/**
 * Main owner state machine function.
 *
 * Goes through the lists of excited incoming and outgoing requests until all
 * the excitement is gone.
 */
static void owner_balance(struct m0_rm_owner *o)
{
	struct m0_rm_pin      *pin;
	struct m0_rm_credit   *credit;
	struct m0_rm_outgoing *out;
	struct m0_rm_incoming *in;
	bool                   todo;
	int                    prio;

	M0_ENTRY("owner %p", o);
	do {
		todo = false;
		m0_tl_for (m0_rm_ur, &o->ro_outgoing[OQS_EXCITED], credit) {
			M0_ASSERT(m0_rm_credit_bob_check(credit));
			todo = true;
			out = bob_of(credit, struct m0_rm_outgoing,
				     rog_want.rl_credit, &outgoing_bob);
			/*
			 * Outgoing request completes: remove all pins stuck in
			 * and finalise them. Also pass the processing error, if
			 * any, to the corresponding incoming structure(s).
			 *
			 * Removing of pins might excite incoming requests
			 * waiting for outgoing request completion.
			 */
			m0_tl_for (pr, &credit->cr_pins, pin) {
				int32_t out_rc =
				       out->rog_rc == -EAGAIN ? 0 : out->rog_rc;

				M0_ASSERT(m0_rm_pin_bob_check(pin));
				M0_ASSERT(pin->rp_flags == M0_RPF_TRACK);
				/*
				 * If one outgoing request has set an error,
				 * then don't overwrite the error code. It's
				 * possible that an error code could be
				 * reset to 0 if other requests succeed.
				 */
				pin->rp_incoming->rin_rc =
					pin->rp_incoming->rin_rc ?: out_rc;
				pin_del(pin);
			} m0_tl_endfor;
			m0_rm_ur_tlist_del(credit);
			m0_rm_outgoing_fini(out);
		} m0_tl_endfor;
		for (prio = ARRAY_SIZE(o->ro_incoming) - 1; prio >= 0; --prio) {
			m0_tl_for (m0_rm_ur,
				   &o->ro_incoming[prio][OQS_EXCITED], credit) {
				todo = true;
				in = bob_of(credit, struct m0_rm_incoming,
					    rin_want, &incoming_bob);
				/*
				 * All waits completed, go to CHECK state.
				 */
				m0_rm_ur_tlist_move(
					&o->ro_incoming[prio][OQS_GROUND],
					&in->rin_want);
				incoming_state_set(in, RI_CHECK);
				incoming_check(in);
			} m0_tl_endfor;
		}
	} while (todo);
	/*
	 * Check if owner needs to be finalised.
	 */
	owner_finalisation_check(o);
	M0_LEAVE();
}

/**
 * Deletes all M0_RPF_BARRIER pins set by a given incoming request.
 *
 * In other words, function cancels all credit reservations made by incoming
 * request. Also it deletes pins set to track reserved credits. It is guaranteed
 * that if M0_RPF_TRACK pin exists for reserved credit, then it was stuck to
 * track reservation cancel, because reserved credit is cached. In some rare
 * cases credit can also be held (if M0_RIF_LOCAL_WAIT was not set for 'in'),
 * but logic works fine in this case too.
 */
static void barrier_pins_del(struct m0_rm_incoming *in)
{
	struct m0_rm_pin    *in_pin;
	struct m0_rm_pin    *cr_pin;
	struct m0_rm_credit *cr;

	m0_tl_for(pi, &in->rin_pins, in_pin) {
		M0_ASSERT(m0_rm_pin_bob_check(in_pin));
		if (in_pin->rp_flags & M0_RPF_BARRIER) {
			cr = in_pin->rp_credit;
			m0_tl_for(pr, &cr->cr_pins, cr_pin) {
				if (cr_pin->rp_flags & M0_RPF_TRACK) {
					pin_del(cr_pin);
				}
			} m0_tl_endfor;
			pin_del(in_pin);
		}
	} m0_tl_endfor;
}

/**
 * Takes an incoming request in RI_CHECK state and attempts to perform a
 * non-blocking state transition.
 *
 * This function leaves the request either in RI_WAIT, RI_SUCCESS or RI_FAILURE
 * state.
 */
static void incoming_check(struct m0_rm_incoming *in)
{
	struct m0_rm_credit  rest;
	int                  rc;

	M0_ENTRY();
	M0_PRE(m0_rm_incoming_bob_check(in));

	/*
	 * This function is reentrant. An outgoing request might set
	 * the processing error for the incoming structure. Check for the
	 * error. If there is an error, there is no need to continue the
	 * processing.
	 */
	if (in->rin_rc == 0) {
		m0_rm_credit_init(&rest, in->rin_want.cr_owner);
		rc = m0_rm_credit_copy(&rest, &in->rin_want) ?:
			incoming_check_with(in, &rest);
		M0_ASSERT(ergo(rc >= 0, credit_is_empty(&rest)));
		m0_rm_credit_fini(&rest);
	} else
		rc = in->rin_rc;

	if (rc != 0) {
		/*
		 * Delete all PROTECT pins set by incoming request to
		 * allow corresponding credits to be used for other incoming
		 * requests. That prevents credit dependencies and possible
		 * dead-locks.
		 *
		 * Also, request could protect some credits before failure.
		 */
		incoming_pins_del(in, M0_RPF_PROTECT);
	}

	if (rc > 0) {
		incoming_state_set(in, RI_WAIT);
	} else {
		/**
		 * @todo
		 * Here we introduce "thundering herd" problem, potentially
		 * waking up all requests waiting for reserved credit.
		 * It is necessary, because rio_conflict() won't be called for
		 * 'in' if waiting requests are not woken up.
		 */
		barrier_pins_del(in);

		if (rc == 0) {
			M0_ASSERT(incoming_pin_nr(in, M0_RPF_TRACK) == 0);
			incoming_policy_apply(in);
			/*
			 * Transfer the CACHED credits to HELD list. Later
			 * it may be subsumed by policy functions (or
			 * vice versa). Credits are held only for local
			 * request. For remote requests, they are removed
			 * and converted into loans.
			 */
			if (in->rin_type == M0_RIT_LOCAL)
				rc = cached_credits_hold(in);
		}
		/*
		 * Check if incoming request is complete. When there is
		 * partial failure (with part of the request failing)
		 * of incoming request, it's necessary to check that it's
		 * complete (and there are no outstanding outgoing requests
		 * pending against it). On partial failure put the request in
		 * RI_WAIT state.
		 */
		if (incoming_is_complete(in))
			incoming_complete(in, rc);
		else {
			in->rin_rc = rc;
			incoming_state_set(in, RI_WAIT);
		}
	}
	M0_LEAVE();
}

/*
 * Checks if there are outstanding "outgoing requests" for this incoming
 * requests.
 */
static bool incoming_is_complete(const struct m0_rm_incoming *in)
{
	return incoming_pin_nr(in, M0_RPF_TRACK) == 0;
}

static bool credit_is_reserved(const struct m0_rm_credit *cr)
{
	return credit_pin_nr(cr, M0_RPF_BARRIER) > 0;
}

/**
 * Call @ref m0_rm_incoming_ops::rio_conflict()
 * for all incoming requests which pinned the given credit.
 * Function is called when a request arrives which conflicts
 * with a held credit.
 *
 * @param credit Credit which conflicts with incoming request.
 */
static void conflict_notify(struct m0_rm_credit *credit)
{
	struct m0_rm_pin      *pin;
	struct m0_rm_incoming *in;

	M0_PRE(credit != NULL);
	M0_PRE(credit_pin_nr(credit, M0_RPF_PROTECT) > 0);

	m0_tl_for(pr, &credit->cr_pins, pin) {
		M0_ASSERT(m0_rm_pin_bob_check(pin));
		if (pin->rp_flags & M0_RPF_PROTECT) {
			in = pin->rp_incoming;
			/*
			 * In current design rio_conflict() will never be called
			 * before rio_complete() for any incoming request.
			 * M0_RPF_PROTECT pins are deleted if incoming request
			 * can't be fully granted and lifetime of such pins is
			 * incoming_check() function. It is important to hold
			 * this assertion to not confuse incoming request
			 * originator.
			 */
			M0_ASSERT(in->rin_sm.sm_state == RI_SUCCESS);
			in->rin_ops->rio_conflict(in);
		}
	} m0_tl_endfor;
}

/**
 * Add M0_RPF_TRACK pin from incoming request to credit
 * if flag RIF_LOCAL_TRY is not set in request.
 */
static int credit_maybe_track(struct m0_rm_incoming *in,
			      struct m0_rm_credit   *cr,
			      bool                   notify,
			      int                   *wait)
{
	int rc;

	M0_PRE(credit_is_reserved(cr) || in->rin_flags & WAIT_TRY_FLAGS);

	if (!(in->rin_flags & RIF_LOCAL_TRY)) {
		rc = m0_rm_pin_add(in, cr, M0_RPF_TRACK);
		if (rc == 0 && notify)
			conflict_notify(cr);
		(*wait)++;
		return M0_RC(rc);
	} else {
		return M0_ERR(-EBUSY);
	}
}

/**
 * Try to use held credit to satisfy incoming request.
 * Function doesn't reduce 'rest'.
 *
 * @param in       Incoming request.
 * @param rest     Not yet satisfied part of originally requested credit.
 * @param held     Credit to check against incoming request.
 * @param wait     Counter that is increased if held credit can't be used
 *                 immediately and incoming request has to wait until held
 *                 credit is released.
 * @param cr_used  Output parameter. Set to true if credit fulfills request.
 */
static int incoming_check_held(struct m0_rm_incoming *in,
			       struct m0_rm_credit   *rest,
			       struct m0_rm_credit   *held,
			       int                   *wait,
			       bool                  *cr_used)
{
	int  rc = 0;
	/*
	 * Note that second parameter is the whole credit, not 'rest'.
	 * 'Rest' could be reduced the way that it doesn't conflict
	 * with a local credit anymore, but "try/wait" flags by design
	 * are applied to the whole requested credit.
	 */
	bool conflict = credit_conflicts(held, &in->rin_want);

	M0_PRE(credit_intersects(held, rest));

	*cr_used = false;
	/**
	 * @todo
	 * Ignore borrow requests for held non-conflicting credits.
	 * If it is the only credit that can satisfy incoming request, then
	 * eventually creditor will revoke it.
	 *
	 * Ideally, non-conflicting credits should be borrowed unconditionally.
	 * But that means that copy of credit is borrowed, so the same credit
	 * is held by two RM owners and the total number of credits in cluster
	 * increases. Currently, there is no way in RM framework to link credit
	 * and its borrowed copy.
	 */
	if (in->rin_type == M0_RIT_BORROW && !conflict) {
		return M0_RC(0);
	} else if (in->rin_type == M0_RIT_REVOKE ||
		   (conflict && in->rin_flags & WAIT_TRY_FLAGS)) {
		rc = credit_maybe_track(in, held, true, wait);
	} else {
		/*
		 * This is the case of a LOCAL request, when we
		 * don't have to wait until the credit is released.
		 */
		M0_ASSERT(in->rin_type == M0_RIT_LOCAL &&
			  (!conflict || (in->rin_flags & WAIT_TRY_FLAGS) == 0));
	}

	*cr_used = (rc == 0);
	return M0_RC(rc);
}

/**
 * Checks whether 'in1' has higher reserve priority than 'in2'.
 */
static bool has_reserve_priority(struct m0_rm_incoming *in1,
			         struct m0_rm_incoming *in2)
{
	struct m0_rm_reserve_prio *pr1 = &in1->rin_reserve;
	struct m0_rm_reserve_prio *pr2 = &in2->rin_reserve;
	int                        cmp;

	M0_PRE(in1 != in2);
	M0_PRE(reserve_prio_is_set(pr1));
	M0_PRE(reserve_prio_is_set(pr2));
	M0_PRE(in1->rin_flags & RIF_RESERVE);
	M0_PRE(in2->rin_flags & RIF_RESERVE);

	cmp = M0_3WAY(pr1->rrp_time, pr2->rrp_time) ?:
	      m0_fid_cmp(&pr1->rrp_owner, &pr2->rrp_owner) ?:
	      M0_3WAY(pr1->rrp_seq, pr2->rrp_seq);
	M0_ASSERT(cmp != 0);
	return cmp < 0;
}

/**
 * Checks whether barrier currently set for credit (if any) is overcome
 * by a given incoming request. If yes, then barrier is replaced, otherwise
 * tracking pin is added.
 */
static int credit_reservation_check(struct m0_rm_incoming *in,
				    struct m0_rm_credit   *cr,
				    int                   *wait)
{
	struct m0_rm_pin *pin;
	int               rc = 0;

	pin = m0_tl_find(pr, p, &cr->cr_pins, p->rp_flags & M0_RPF_BARRIER);
	if (pin != NULL){
		if (in == pin->rp_incoming)
			return M0_RC(0);

		if (!(in->rin_flags & RIF_RESERVE) ||
		    has_reserve_priority(pin->rp_incoming, in)) {
			rc = credit_maybe_track(in, cr, false, wait);
		} else {
			/* That can be a second M0_RPF_TRACK pin added
			 * to a credit from a single request */
			pin->rp_flags &= ~M0_RPF_BARRIER;
			pin->rp_flags |= M0_RPF_TRACK;
			rc = m0_rm_pin_add(in, cr, M0_RPF_BARRIER);
		}
	} else if (in->rin_flags & RIF_RESERVE) {
		rc = m0_rm_pin_add(in, cr, M0_RPF_BARRIER);
	}
	return M0_RC(rc);
}

/**
 * Main helper function to incoming_check(), which starts with "rest" set to the
 * wanted credit and goes though the sequence of checks, reducing "rest".
 *
 * CHECK logic can be described by means of "wait conditions". A wait condition
 * is something that prevents immediate fulfillment of the request.
 *
 *     - A request with RIF_LOCAL_WAIT bit set can be fulfilled iff the credits
 *       on ->ro_owned[OWOS_CACHED] list together imply the wanted credit;
 *
 *     - a request without RIF_LOCAL_WAIT bit can be fulfilled iff the credits
 *       on all ->ro_owned[] lists together imply the wanted credit.
 *
 * If there is not enough credits on ->ro_owned[] lists, an incoming request has
 * to wait until some additional credits are borrowed from the upward creditor
 * or revoked from downward debtors.
 *
 * A RIF_LOCAL_WAIT request, in addition, can wait until a credit moves from
 * ->ro_owned[OWOS_HELD] to ->ro_owned[OWOS_CACHED].
 *
 * This function performs no state transitions by itself. Instead its return
 * value indicates the target state:
 *
 *     - 0: the request is fulfilled, the target state is RI_SUCCESS,
 *     - +ve: more waiting is needed, the target state is RI_WAIT,
 *     - -ve: error, the target state is RI_FAILURE.
 */
static int incoming_check_with(struct m0_rm_incoming *in,
			       struct m0_rm_credit   *rest)
{
	struct m0_rm_credit *want = &in->rin_want;
	struct m0_rm_owner  *o    = want->cr_owner;
	struct m0_rm_credit *r;
	struct m0_rm_loan   *loan;
	bool                 group_matches;
	/**
	 * @todo Will be deleted once borrowing held non-conflicting credits is
	 * allowed.
	 */
	bool                 use_credit = false;
	int                  i;
	int                  wait = 0;
	int                  rc   = 0;

	M0_ENTRY("incoming: %p credit: %llu", in,
		 (long long unsigned) rest->cr_datum);
	M0_PRE_EX(m0_rm_ur_tlist_contains(
		       &o->ro_incoming[in->rin_priority][OQS_GROUND], want));

	/*
	 * 1. Scan owned lists first. Check for "local" wait/try conditions.
	 */
	for (i = ARRAY_SIZE(o->ro_owned) - 1; i >= 0; --i) {
		/*
		 * Make sure cached credits are checked first.
		 * It is better to use cached credit than held
		 * if there are suitable credits in both lists.
		 */
		M0_CASSERT(OWOS_CACHED > OWOS_HELD);
		m0_tl_for (m0_rm_ur, &o->ro_owned[i], r) {
			M0_ASSERT(m0_rm_credit_bob_check(r));
			if (!credit_intersects(r, rest))
				continue;

			if (i == OWOS_HELD)
				rc = incoming_check_held(in, rest, r, &wait,
							 &use_credit);
			else
				use_credit = true;

			if (rc == 0 && use_credit) {
				rc = credit_reservation_check(in, r, &wait);
				if (rc == 0 && wait == 0)
					rc = m0_rm_pin_add(in, r,
							   M0_RPF_PROTECT);
				rc = rc ?: credit_diff(rest, r);
			}

			if (rc != 0)
				return M0_RC(rc);
		} m0_tl_endfor;
	}

	/*
	 * 2. If the credit request cannot still be satisfied, check against the
	 *    sublet list.
	 */
	if (!credit_is_empty(rest)) {
		m0_tl_for (m0_rm_ur, &o->ro_sublet, r) {
			M0_ASSERT(m0_rm_credit_bob_check(r));
			M0_ASSERT(!credit_is_empty(r));
			if (!credit_intersects(r, rest))
				continue;

			group_matches =
				!credit_group_conflict(&r->cr_group_id,
						       &rest->cr_group_id);
			if (group_matches) {
				rc = credit_diff(rest, r);
				if (rc != 0)
					return M0_RC(rc);
				if (!credit_is_empty(rest))
					continue;
				else
					break;
			} else if (!(in->rin_flags & RIF_MAY_REVOKE))
				return M0_ERR(-EREMOTE);

			loan = bob_of(r, struct m0_rm_loan, rl_credit,
				      &loan_bob);
			/*
			 * It is possible that this loop emits multiple
			 * outgoing requests toward the same remote
			 * owner. Don't bother to coalesce them here.
			 * The rpc layer would do this more efficiently.
			 *
			 * @todo use rpc grouping here.
			 */
			/*
			 * @todo - Revoke entire loan?? rest could be
			 * subset of r.
			 */
			wait++;
			rc = revoke_send(in, loan, r) ?:
			     credit_diff(rest, r);
			if (rc != 0)
				return M0_ERR(rc);
		} m0_tl_endfor;
	}

	/*
	 * 3. If the credit request still cannot be satisfied, check
	 *    if it's possible borrow remaining credit from the creditor.
	 */
	if (!credit_is_empty(rest)) {
		if (o->ro_creditor != NULL) {
			if (!(in->rin_flags & RIF_MAY_BORROW))
				return M0_ERR(-EREMOTE);
			wait++;
			rc = borrow_send(in, rest);
		} else
			rc = -ESRCH;
	}

	return M0_RC(rc ?: wait);
}

/**
 * Called when an outgoing request completes (possibly with an error, like a
 * timeout).
 */
M0_INTERNAL void m0_rm_outgoing_complete(struct m0_rm_outgoing *og)
{
	struct m0_rm_owner *owner;

	M0_ENTRY();
	M0_PRE(og != NULL);

	owner = og->rog_want.rl_credit.cr_owner;
	m0_rm_ur_tlist_move(&owner->ro_outgoing[OQS_EXCITED],
			    &og->rog_want.rl_credit);
	owner_balance(owner);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_outgoing_complete);

/**
 * Helper function called when an incoming request processing completes.
 *
 * Sets m0_rm_incoming::rin_sm.sm_rc, updates request state, invokes completion
 * call-back, broadcasts request channel and releases request pins.
 */
static void incoming_complete(struct m0_rm_incoming *in, int32_t rc)
{
	struct m0_rm_owner *owner = in->rin_want.cr_owner;

	M0_ENTRY("incoming: %p error: [%d]", in, rc);
	M0_PRE(in->rin_ops != NULL);
	M0_PRE(in->rin_ops->rio_complete != NULL);
	M0_PRE(M0_IN(incoming_state(in), (RI_CHECK, RI_INITIALISED)));
	M0_PRE(rc <= 0);
	M0_PRE_EX(!m0_rm_ur_tlink_is_in(&in->rin_want) ||
		  m0_rm_ur_tlist_contains(
			&owner->ro_incoming[in->rin_priority][OQS_GROUND],
			&in->rin_want));

	in->rin_rc = rc;
	M0_LOG(M0_DEBUG, "Incoming req: %p, state change:[%s -> %s]\n",
			 in, m0_sm_state_name(&in->rin_sm, in->rin_sm.sm_state),
			 m0_sm_state_name(&in->rin_sm,
				 rc == 0 ? RI_SUCCESS : RI_FAILURE));
	m0_sm_move(&in->rin_sm, rc, rc == 0 ? RI_SUCCESS : RI_FAILURE);
	if (rc != 0) {
		incoming_release(in);
		m0_rm_ur_tlist_remove(&in->rin_want);
		M0_ASSERT(pi_tlist_is_empty(&in->rin_pins));
	}
	M0_ASSERT(incoming_invariant(in));
	in->rin_ops->rio_complete(in, rc);
	M0_POST(owner_invariant(owner));
	M0_LEAVE();
}

static void incoming_policy_none(struct m0_rm_incoming *in)
{
}

static void incoming_policy_apply(struct m0_rm_incoming *in)
{
	static void (*generic[RIP_NR])(struct m0_rm_incoming *) = {
		[RIP_NONE]    = &incoming_policy_none,
		[RIP_INPLACE] = &incoming_policy_none,
		[RIP_STRICT]  = &incoming_policy_none,
		[RIP_JOIN]    = &incoming_policy_none,
		[RIP_MAX]     = &incoming_policy_none
	};

	if (IS_IN_ARRAY(in->rin_policy, generic))
		generic[in->rin_policy](in);
	else {
		struct m0_rm_resource *resource = incoming_to_resource(in);

		resource->r_ops->rop_policy(resource, in);
	}
}

/**
 * Check if the outgoing request for requested credit is already pending.
 * If yes, attach a tracking pin.
 */
static int outgoing_check(struct m0_rm_incoming    *in,
			  enum m0_rm_outgoing_type  otype,
			  struct m0_rm_credit      *credit,
			  struct m0_rm_remote      *other)
{
	int		       i;
	int		       rc = 0;
	struct m0_rm_owner    *owner = in->rin_want.cr_owner;
	struct m0_rm_credit   *scan;
	struct m0_rm_outgoing *out;

	M0_ENTRY();
	for (i = 0; i < ARRAY_SIZE(owner->ro_outgoing); ++i) {
		m0_tl_for (m0_rm_ur, &owner->ro_outgoing[i], scan) {
			M0_ASSERT(m0_rm_credit_bob_check(scan));
			out = bob_of(scan, struct m0_rm_outgoing,
				     rog_want.rl_credit, &outgoing_bob);
			if (out->rog_type == otype &&
			    credit_intersects(scan, credit)) {
				M0_LOG(M0_DEBUG, "want %p, other %p",
				       out->rog_want.rl_other, other);
				/*
				 * Conflicting credits are always requested from
				 * the same remote, since only one owner can
				 * possess this credit at a time. It's also
				 * impossible that 'credit' is composed of
				 * several parts owned by different remotes.
				 * Revoke requests are already split by loans,
				 * borrow requests always go to a single
				 * creditor.
				 */
				M0_ASSERT(ergo(credit_conflicts(scan, credit),
					      out->rog_want.rl_other == other));
				/*
				 * There is only one creditor, all borrow
				 * requests should go to him
				 */
				M0_ASSERT(ergo(otype == M0_ROT_BORROW,
					      out->rog_want.rl_other == other));
				/*
				 * Intersecting non-conflicting credits can be
				 * owned by several remotes in case of revoke.
				 * We should revoke all these loans.
				 */
				if (out->rog_want.rl_other == other)
				{
					/**
					 * @todo adjust outgoing requests
					 * priority (priority inheritance)
					 */
					rc = m0_rm_pin_add(in, scan,
							   M0_RPF_TRACK) ?:
					     credit_diff(credit, scan);
					if (rc != 0)
						return M0_ERR(rc);
				}
			}
		} m0_tl_endfor;
	}
	return M0_RC(rc);
}

/**
 * Sends an outgoing revoke request to remote owner specified by the "loan". The
 * request will revoke the credit "credit", which might be a part of original
 * loan.
 */
static int revoke_send(struct m0_rm_incoming *in,
		       struct m0_rm_loan     *loan,
		       struct m0_rm_credit   *credit)
{
	struct m0_rm_credit  rest;
	struct m0_rm_remote *other;
	int                  rc;

	M0_ENTRY("incoming: %p credit: %llu", in,
		 (long long unsigned)credit->cr_datum);
	M0_PRE(loan != NULL);

	/*
	 * Credit is part of sublet loan (sent from incoming_check_with()).
	 * outgoing_check() destructively modifies outgoing credit. Hence,
	 * make a copy.
	 */
	other = loan->rl_other;
	m0_rm_credit_init(&rest, in->rin_want.cr_owner);
	rc = m0_rm_credit_copy(&rest, credit) ?:
		outgoing_check(in, M0_ROT_REVOKE, &rest, other);
	if (!credit_is_empty(&rest) && rc == 0)
		rc = m0_rm_request_out(M0_ROT_REVOKE, in,
				       loan, &rest, other);

	m0_rm_credit_fini(&rest);
	return M0_RC(rc);
}

/**
 * Sends an outgoing borrow request to the upward creditor. The request will
 * borrow the credit "credit".
 */
static int borrow_send(struct m0_rm_incoming *in, struct m0_rm_credit *credit)
{
	int                  rc;
	struct m0_rm_remote *other = in->rin_want.cr_owner->ro_creditor;

	M0_ENTRY("incoming: %p credit: %llu", in,
		 (long long unsigned) credit->cr_datum);
	M0_PRE(other != NULL);

	/*
	 * Borrow sends the remaining credit request to the creditor. It's
	 * ok if outgoing_check() modifies it. It goes well with
	 * incoming_check_with() - which keeps on reducing the request set.
	 */
	rc = outgoing_check(in, M0_ROT_BORROW, credit, other);
	/*
	 * Sends the entire credit request to the creditor. Empty the credit.
	 */
	if (!credit_is_empty(credit) && rc == 0)
		rc = m0_rm_request_out(M0_ROT_BORROW, in,
				       NULL, credit, other) ?:
		     credit_diff(credit, credit);
	return M0_RC(rc);
}

/**
 * Returns the loan to the creditor
 */
static int cancel_send(struct m0_rm_loan *loan)
{
	int rc;

	M0_ENTRY();
	M0_PRE(loan != NULL);
	M0_LOG(M0_DEBUG, "credit: %llu", (long long unsigned)
	       loan->rl_credit.cr_datum);

	rc = m0_rm_request_out(M0_ROT_CANCEL, NULL, loan,
			       &loan->rl_credit, loan->rl_other);
	return M0_RC(rc);
}

M0_INTERNAL int m0_rm_owner_loan_debit(struct m0_rm_owner *owner,
				       struct m0_rm_loan  *paid_loan,
				       struct m0_tl       *list)
{
	struct m0_rm_credit *cr;
	struct m0_rm_loan   *loan;
	struct m0_rm_loan   *remnant_loan;
	struct m0_tl	     retain_list;
	struct m0_tl	     remove_list;
	int		     rc = 0;

	M0_PRE(owner != NULL);
	M0_PRE(paid_loan != NULL);
	M0_ENTRY("credit: %llu",
			(long long unsigned) paid_loan->rl_credit.cr_datum);

	m0_rm_ur_tlist_init(&retain_list);
	m0_rm_ur_tlist_init(&remove_list);
	m0_tl_for (m0_rm_ur, list, cr) {
		if (!cr->cr_ops->cro_intersects(&paid_loan->rl_credit, cr))
			m0_rm_ur_tlist_move(&retain_list, cr);
		else {
			loan = bob_of(cr, struct m0_rm_loan,
				      rl_credit, &loan_bob);
			if (!m0_cookie_is_eq(&loan->rl_cookie,
					     &paid_loan->rl_cookie)) {
				m0_rm_ur_tlist_move(&retain_list, cr);
				continue;
			}
			rc = remnant_loan_get(loan, &paid_loan->rl_credit,
					      &remnant_loan);
			if (rc == 0) {
				if (credit_is_empty(&remnant_loan->rl_credit)) {
					m0_rm_ur_tlist_move(&remove_list, cr);
					m0_rm_ur_tlist_add(&remove_list,
						&remnant_loan->rl_credit);
				} else
					m0_rm_ur_tlist_move(&retain_list, cr);
			}
		}
	} m0_tl_endfor;
	/*
	 * On successful completion, remove the credits from the "remove-list"
	 * and move the remnant credits to the 'list'. Do the opposite
	 * on failure.
	 */
	m0_tl_teardown(m0_rm_ur, rc ? &retain_list : &remove_list, cr) {
		loan = bob_of(cr, struct m0_rm_loan, rl_credit, &loan_bob);
		m0_rm_loan_fini(loan);
		m0_free(loan);
	}

	m0_tl_for (m0_rm_ur, rc ? &remove_list : &retain_list, cr) {
		m0_rm_ur_tlist_move(list, cr);
	} m0_tl_endfor;

	m0_rm_ur_tlist_fini(&retain_list);
	m0_rm_ur_tlist_fini(&remove_list);
	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_owner_loan_debit);

M0_INTERNAL int granted_maybe_reserve(struct m0_rm_credit *granted,
				      struct m0_rm_credit *to_cache)
{
	struct m0_rm_pin      *pin;
	struct m0_rm_incoming *curr;
	struct m0_rm_incoming *best = NULL;
	int                    rc = 0;

	M0_ENTRY("granted %p to_cache %p", granted, to_cache);
	/*
	 * First iteration: find request with highest reserve priority.
	 */
	m0_tl_for(pr, &granted->cr_pins, pin) {
		M0_ASSERT(m0_rm_pin_bob_check(pin));
		curr = pin->rp_incoming;
		if (curr->rin_flags & RIF_RESERVE) {
			if (best == NULL ||
			    has_reserve_priority(curr, best))
				best = curr;
		}
	} m0_tl_endfor;

	if (best == NULL)
		return M0_RC(0);
	/*
	 * Second iteration: add M0_RPF_BARRIER to 'best'
	 * and M0_RPF_TRACK pins for others.
	 */
	m0_tl_for(pr, &granted->cr_pins, pin) {
		curr = pin->rp_incoming;
		rc = m0_rm_pin_add(curr, to_cache,
		                   (curr == best) ?
				   M0_RPF_BARRIER : M0_RPF_TRACK);
		if (rc != 0)
			break;
	} m0_tl_endfor;

	return M0_RC(rc);
}

/* Checks if the loaned credit can be cached */
static int loan_check(struct m0_rm_owner  *owner,
		      struct m0_tl        *list,
		      struct m0_rm_credit *rest)
{
	int                  rc = 0;
	struct m0_rm_credit *cr;

	M0_ENTRY("loan check against credit: %llu",
			(long long unsigned) rest->cr_datum);
	m0_tl_for (m0_rm_ur, list, cr) {
		/*
		 * Credits that are granted to the owners from the same group
		 * never conflicts, so intersecting (or even conflicting)
		 * credits can be potentially granted to several owners.
		 * Don't put the credit to cached list until all copies given
		 * to the owners from the same group are returned back.
		 */
		if (cr->cr_ops->cro_intersects(rest, cr) &&
		    !credit_group_conflict(&rest->cr_group_id,
					   &cr->cr_group_id))
		{
			rc = credit_diff(rest, cr);
			if (rc != 0 || credit_is_empty(rest))
				break;
		}
	} m0_tl_endfor;
	return M0_RC(rc);
}

M0_INTERNAL int m0_rm_loan_settle(struct m0_rm_owner *owner,
				  struct m0_rm_loan  *loan)
{
	int                  rc;
	struct m0_rm_credit *cached = NULL;

	M0_PRE(owner != NULL);
	M0_PRE(loan != NULL);

	rc = m0_rm_credit_dup(&loan->rl_credit, &cached) ?:
	     granted_maybe_reserve(&loan->rl_credit, cached);
	if (rc == 0) {
		rc = m0_rm_owner_loan_debit(owner, loan, &owner->ro_sublet) ?:
			loan_check(owner, &owner->ro_sublet, cached);
		if (rc == 0 && !credit_is_empty(cached)) {
			/* @todo */
			cached->cr_group_id = m0_rm_no_group;
			m0_rm_ur_tlist_add(&owner->ro_owned[OWOS_CACHED],
					   cached);
			M0_LOG(M0_INFO, "credit cached\n");
		} else {
			m0_free(cached);
			if (rc != 0)
				M0_LOG(M0_ERROR, "credit removal failed: "
						 "rc [%d]\n", rc);
		}
	} else
		M0_LOG(M0_ERROR, "credit allocation failed: rc [%d]\n", rc);

	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_loan_settle);

/** @} end of Owner state machine group */

/**
 * @name Invariants group
 *
 * Resource manager maintains a number of interrelated data-structures in
 * memory. Invariant checking functions, defined in this section assert internal
 * consistency of these structures.
 *
 * @{
 */

/**
 * Helper function used by resource_type_invariant() to check all elements of
 *  m0_rm_resource_type::rt_resources.
 */
static bool resource_list_check(const struct m0_rm_resource *res, void *datum)
{
	const struct m0_rm_resource_type *rt = datum;

	return m0_rm_resource_find(rt, res) == res && res->r_type == rt;
}

static bool resource_type_invariant(const struct m0_rm_resource_type *rt)
{
	struct m0_rm_domain *dom   = rt->rt_dom;
	const struct m0_tl  *rlist = &rt->rt_resources;

	return
		res_tlist_invariant_ext(rlist, resource_list_check,
					(void *)rt) &&
		rt->rt_nr_resources == res_tlist_length(rlist) &&
		dom != NULL && IS_IN_ARRAY(rt->rt_id, dom->rd_types) &&
		dom->rd_types[rt->rt_id] == rt;
}

static bool pin_check(const void *bob)
{
	const struct m0_rm_pin *pin = bob;

	return pin->rp_credit != NULL &&
	       pin->rp_incoming != NULL &&
	       M0_IN(pin->rp_flags,
			(M0_RPF_BARRIER, M0_RPF_TRACK, M0_RPF_PROTECT));
}

/**
 * Invariant for m0_rm_incoming.
 */
static bool incoming_invariant(const struct m0_rm_incoming *in)
{
	return
		_0C((in->rin_rc != 0) == (incoming_state(in) == RI_FAILURE)) &&
		_0C(!(in->rin_flags & ~(RIF_MAY_REVOKE|RIF_MAY_BORROW|
				RIF_LOCAL_WAIT|RIF_LOCAL_TRY|RIF_RESERVE))) &&
		/* RIF_LOCAL_WAIT and RIF_LOCAL_TRY can't be set together */
		_0C((in->rin_flags & WAIT_TRY_FLAGS) != WAIT_TRY_FLAGS) &&
		/* M0_RIT_BORROW and M0_RIT_REVOKE should have exactly one of
		 * RIF_LOCAL_WAIT, RIF_LOCAL_TRY flags set */
		_0C(ergo(M0_IN(in->rin_type, (M0_RIT_BORROW, M0_RIT_REVOKE)),
		     m0_is_po2(in->rin_flags & WAIT_TRY_FLAGS))) &&
		_0C(IS_IN_ARRAY(in->rin_priority,
			    in->rin_want.cr_owner->ro_incoming)) &&
		/* a request can be in "check" state only during owner_balance()
		   execution. */
		_0C(incoming_state(in) != RI_CHECK) &&
		_0C(pi_tlist_invariant(&in->rin_pins)) &&
		/* a request in the WAIT state... */
		_0C(ergo(incoming_state(in) == RI_WAIT,
		         /* waits on something... */
		         incoming_pin_nr(in, M0_RPF_TRACK) > 0 &&
		         /* and doesn't hold anything. */
		         incoming_pin_nr(in, M0_RPF_PROTECT) == 0)) &&
		/* a fulfilled request... */
		_0C(ergo(incoming_state(in) == RI_SUCCESS,
#if 0
		         /* holds something... */
		         incoming_pin_nr(in, M0_RPF_PROTECT) > 0 &&
#endif
		         /* and waits on nothing. */
		         incoming_pin_nr(in, M0_RPF_TRACK) == 0)) &&
		_0C(ergo(incoming_state(in) == RI_FAILURE ||
		         incoming_state(in) == RI_INITIALISED,
		         incoming_pin_nr(in, ~0) == 0)) &&
		_0C(pr_tlist_is_empty(&in->rin_want.cr_pins));
}

enum credit_queue {
	OIS_BORROWED = 0,
	OIS_SUBLET,
	OIS_OUTGOING,
	OIS_OWNED,
	OIS_INCOMING,
	OIS_NR
};

struct owner_invariant_state {
	enum credit_queue    is_phase;
	int                  is_owned_idx;
	struct m0_rm_credit  is_debit;
	struct m0_rm_credit  is_credit;
	struct m0_rm_owner  *is_owner;
};

static bool credit_invariant(const struct m0_rm_credit *credit, void *data)
{
	struct owner_invariant_state *is =
		(struct owner_invariant_state *) data;
	return
		/* only held credits have PROTECT pins */
		_0C(ergo((is->is_phase == OIS_OWNED &&
			  is->is_owned_idx == OWOS_HELD),
			  credit_pin_nr(credit, M0_RPF_PROTECT) > 0)) &&
		/* only held/cached credits can be reserved */
		_0C(ergo(credit_is_reserved(credit),
			 (is->is_phase == OIS_OWNED))) &&
		_0C(ergo(is->is_phase == OIS_INCOMING,
			 incoming_invariant(cr2in(credit)))) &&
		_0C(credit_pin_nr(credit, M0_RPF_BARRIER) <= 1);
}

static bool conflict_exists(const struct m0_rm_credit *cr,
			    const struct m0_rm_owner  *owner)
{
	return m0_exists(i, ARRAY_SIZE(owner->ro_owned),
			m0_tl_exists(m0_rm_ur, c2, &owner->ro_owned[i],
				cr != c2 && cr->cr_ops->cro_conflicts(cr, c2)));
}

/**
 * Checks internal consistency of a resource owner.
 */
static bool owner_invariant_state(const struct m0_rm_owner     *owner,
				  struct owner_invariant_state *is)
{
	int i;

	/*
	 * Iterate over all credits lists:
	 *
	 *    - checking their consistency as double-linked lists
	 *      (m0_rm_ur_tlist_invariant_ext());
	 *
	 *    - making additional consistency checks:
	 *
	 *    - that a credit is for the same resource as the owner,
	 *
	 *    - that a credit on m0_rm_owner::ro_owned[X] is pinned iff X
	 *            == OWOS_HELD.
	 *
	 *    - accumulating total credit and debit.
	 */
	is->is_phase = OIS_BORROWED;
	if (!m0_rm_ur_tlist_invariant_ext(&owner->ro_borrowed,
					  &credit_invariant, (void *)is))
		return M0_ERR(false);
	is->is_phase = OIS_SUBLET;
	if (!m0_rm_ur_tlist_invariant_ext(&owner->ro_sublet,
					  &credit_invariant, (void *)is))
		return M0_ERR(false);
	is->is_phase = OIS_OUTGOING;
	if (!m0_rm_ur_tlist_invariant_ext(&owner->ro_outgoing[0],
					  &credit_invariant, (void *)is))
		return M0_ERR(false);

	is->is_phase = OIS_OWNED;
	for (i = 0; i < ARRAY_SIZE(owner->ro_owned); ++i) {
		is->is_owned_idx = i;
		if (!m0_rm_ur_tlist_invariant_ext(&owner->ro_owned[i],
					   &credit_invariant, (void *)is))
		    return M0_ERR(false);
	}
	is->is_phase = OIS_INCOMING;

	/* No any pair of owned credits conflicts */
	if (m0_exists(i, ARRAY_SIZE(owner->ro_owned),
			m0_tl_exists(m0_rm_ur, cr, &owner->ro_owned[i],
				conflict_exists(cr, owner))))
		return M0_ERR(false);

	/* Calculate credit */
	return M0_RC(_0C(m0_forall(i, ARRAY_SIZE(owner->ro_incoming),
			 m0_forall(j, ARRAY_SIZE(owner->ro_incoming[i]),
				   m0_rm_ur_tlist_invariant
				   (&owner->ro_incoming[i][j])))) &&
		_0C(m0_forall(i, ARRAY_SIZE(owner->ro_owned),
			  !m0_tl_exists(m0_rm_ur, credit, &owner->ro_owned[i],
					credit->cr_ops->cro_join(&is->is_credit,
								 credit)))) &&
		_0C(!m0_tl_exists(m0_rm_ur, credit, &owner->ro_sublet,
			      credit->cr_ops->cro_join(&is->is_credit,
						       credit))) &&
		/* Calculate debit */
		_0C(!m0_tl_exists(m0_rm_ur, credit, &owner->ro_borrowed,
			     credit->cr_ops->cro_join(&is->is_debit, credit))));
}

/**
 * Checks internal consistency of a resource owner.
 */
static bool owner_invariant(struct m0_rm_owner *owner)
{
	bool                         rc;
	struct owner_invariant_state is;

	M0_ENTRY("owner %p", owner);
	M0_ASSERT(m0_fid_is_set(&owner->ro_fid));
	M0_ASSERT(m0_fid_tget(&owner->ro_fid) == M0_RM_OWNER_FT);
	M0_SET0(&is);
	m0_rm_credit_init(&is.is_debit, owner);
	m0_rm_credit_init(&is.is_credit, owner);

	rc = owner_invariant_state(owner, &is) &&
		 (ergo(owner_state(owner) == ROS_ACTIVE,
		       credit_eq(&is.is_debit, &is.is_credit)));

	m0_rm_credit_fini(&is.is_debit);
	m0_rm_credit_fini(&is.is_credit);
	return M0_RC(rc);
}

/** @} end of invariant group */

/**
   @name Pin helpers

  @{
 */

/**
 * Number of pins with a given flag combination, stuck in a given credit.
 */
static int credit_pin_nr(const struct m0_rm_credit *credit, uint32_t flags)
{
	int               nr = 0;
	struct m0_rm_pin *pin;

	m0_tl_for (pr, &credit->cr_pins, pin) {
		M0_ASSERT(m0_rm_pin_bob_check(pin));
		if (pin->rp_flags & flags)
			++nr;
	} m0_tl_endfor;
	return nr;
}

/**
 * Number of pins with a given flag combination, issued by a given incoming
 * request.
 */
static int incoming_pin_nr(const struct m0_rm_incoming *in, uint32_t flags)
{
	int               nr;
	struct m0_rm_pin *pin;

	nr = 0;
	m0_tl_for (pi, &in->rin_pins, pin) {
		M0_ASSERT(m0_rm_pin_bob_check(pin));
		if (pin->rp_flags & flags)
			++nr;
	} m0_tl_endfor;
	return nr;
}

/**
 * Releases credits pinned by an incoming request, waking up other pending
 * incoming requests if necessary.
 */
static void incoming_release(struct m0_rm_incoming *in)
{
	struct m0_rm_pin    *kingpin;
	struct m0_rm_pin    *pin;
	struct m0_rm_credit *credit;
	struct m0_rm_owner  *o = in->rin_want.cr_owner;

	M0_ENTRY("incoming: %p", in);
	M0_PRE(ergo(in->rin_type == M0_RIT_LOCAL, incoming_invariant(in)));

	m0_tl_for (pi, &in->rin_pins, kingpin) {
		M0_ASSERT(m0_rm_pin_bob_check(kingpin));
		if (kingpin->rp_flags & M0_RPF_PROTECT) {
			credit = kingpin->rp_credit;
			/*
			 * If this was the last protecting pin, wake up incoming
			 * requests waiting on this credit release.
			 */
			if (credit_pin_nr(credit, M0_RPF_PROTECT) == 1) {
				/*
				 * Move the credit back to the CACHED list.
				 */
				m0_rm_ur_tlist_move(&o->ro_owned[OWOS_CACHED],
						    credit);
				/*
				 * I think we are introducing "thundering herd"
				 * problem here.
				 */
				m0_tl_for (pr, &credit->cr_pins, pin) {
					M0_ASSERT(m0_rm_pin_bob_check(pin));
					if (pin->rp_flags & M0_RPF_TRACK)
						pin_del(pin);
				} m0_tl_endfor;
			}
		}
		pin_del(kingpin);
	} m0_tl_endfor;
	M0_LEAVE();
}

/**
 * Removes a pin on a resource usage credit.
 *
 * If this was a last tracking pin issued by the request---excite the latter.
 * The function returns true if it excited an incoming request.
 */
static void pin_del(struct m0_rm_pin *pin)
{
	struct m0_rm_incoming *in;
	struct m0_rm_owner    *owner;

	M0_ENTRY("pin %p", pin);
	M0_ASSERT(pin != NULL);

	in = pin->rp_incoming;
	owner = in->rin_want.cr_owner;
	M0_ASSERT(owner_smgrp_is_locked(owner));
	pi_tlink_del_fini(pin);
	pr_tlink_del_fini(pin);
	if (incoming_pin_nr(in, M0_RPF_TRACK) == 0 &&
	    pin->rp_flags & M0_RPF_TRACK) {
		/*
		 * Last tracking pin removed, excite the request.
		 */
		M0_LOG(M0_INFO, "Exciting incoming: %p\n", in);
		M0_ASSERT(incoming_state(in) == RI_WAIT);
		m0_rm_ur_tlist_move(
			&owner->ro_incoming[in->rin_priority][OQS_EXCITED],
			&in->rin_want);
	}
	m0_rm_pin_bob_fini(pin);
	m0_free(pin);
	M0_LEAVE();
}

/**
 * Sticks a tracking pin on credit. When credit is released, the all incoming
 * requests that stuck pins into it are notified.
 */
M0_INTERNAL int m0_rm_pin_add(struct m0_rm_incoming *in,
                              struct m0_rm_credit   *credit,
                              uint32_t               flags)
{
	struct m0_rm_pin *pin;

	M0_ENTRY("in %p credit %p flags %u", in, credit, flags);

	M0_ALLOC_PTR(pin);
	if (pin != NULL) {
		pin->rp_flags = flags;
		pin->rp_credit = credit;
		pin->rp_incoming = in;
		pr_tlink_init(pin);
		pi_tlink_init(pin);
		pr_tlist_add(&credit->cr_pins, pin);
		pi_tlist_add(&in->rin_pins, pin);
		m0_rm_pin_bob_init(pin);
		M0_LEAVE("new pin %p", pin);
		return M0_RC(0);
	} else
		return M0_ERR(-ENOMEM);
}

/** @} end of pin group */

/**
 *  @name Credit helpers
 *
 * @{
 */

static bool credit_intersects(const struct m0_rm_credit *A,
			      const struct m0_rm_credit *B)
{
	M0_PRE(A->cr_ops != NULL);
	M0_PRE(A->cr_ops->cro_intersects != NULL);

	return A->cr_ops->cro_intersects(A, B);
}

static bool credit_conflicts(const struct m0_rm_credit *A,
			     const struct m0_rm_credit *B)
{
	M0_PRE(A->cr_ops != NULL);
	M0_PRE(A->cr_ops->cro_conflicts != NULL);

	return A->cr_ops->cro_conflicts(A, B) ?
	       credit_group_conflict(&A->cr_group_id, &B->cr_group_id) : false;
}


static int credit_diff(struct m0_rm_credit *c0, const struct m0_rm_credit *c1)
{
	M0_PRE(c0->cr_ops != NULL);
	M0_PRE(c0->cr_ops->cro_diff != NULL);

	return c0->cr_ops->cro_diff(c0, c1);
}

static bool credit_eq(const struct m0_rm_credit *c0,
		      const struct m0_rm_credit *c1)
{
	int                 rc;
	bool                res;
	struct m0_rm_credit credit;

	/* no apples and oranges comparison. */
	M0_PRE(c0->cr_owner == c1->cr_owner);
	m0_rm_credit_init(&credit, c0->cr_owner);
	rc = m0_rm_credit_copy(&credit, c0);
	rc = rc ?: credit_diff(&credit, c1);

	res = rc ? false : credit_is_empty(&credit);
	m0_rm_credit_fini(&credit);

	return res;
}

/*
 * Allocates a new credit and calculates the difference between src and diff.
 * Stores the diff(src, diff) in the newly allocated credit.
 */
static int remnant_credit_get(const struct m0_rm_credit *src,
			     const struct m0_rm_credit  *diff,
			     struct m0_rm_credit       **remnant_credit)
{
	struct m0_rm_credit *new_credit;
	int		     rc;

	M0_ENTRY("splitting credits %llu and %llu",
		 (long long unsigned) src->cr_datum,
		 (long long unsigned) diff->cr_datum);
	M0_PRE(remnant_credit != NULL);
	M0_PRE(src != NULL);
	M0_PRE(diff != NULL);

	rc = m0_rm_credit_dup(src, &new_credit) ?:
		credit_diff(new_credit, diff);
	if (rc != 0 && new_credit != NULL) {
		m0_rm_credit_fini(new_credit);
		m0_free0(&new_credit);
	}
	*remnant_credit = new_credit;
	return M0_RC(rc);
}

/**
 * Allocates memory and makes another copy of credit struct.
 */
M0_INTERNAL int m0_rm_credit_dup(const struct m0_rm_credit *src_credit,
				 struct m0_rm_credit      **dest_credit)
{
	struct m0_rm_credit *credit;
	int		     rc = -ENOMEM;

	M0_ENTRY();
	M0_PRE(src_credit != NULL);

	M0_ALLOC_PTR(credit);
	if (credit != NULL) {
		m0_rm_credit_init(credit, src_credit->cr_owner);
		credit->cr_ops = src_credit->cr_ops;
		rc = m0_rm_credit_copy(credit, src_credit);
		if (rc != 0) {
			m0_rm_credit_fini(credit);
			m0_free0(&credit);
		}
	}
	*dest_credit = credit;
	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_credit_dup);

M0_INTERNAL int
m0_rm_credit_copy(struct m0_rm_credit *dst, const struct m0_rm_credit *src)
{
	M0_PRE(src != NULL);
	M0_PRE(credit_is_empty(dst));

	dst->cr_group_id = src->cr_group_id;
	return src->cr_ops->cro_copy(dst, src);
}

/**
 * Returns true when cr_datum is 0, else returns false.
 */
static bool credit_is_empty(const struct m0_rm_credit *credit)
{
	return credit->cr_datum == 0;
}

M0_INTERNAL int m0_rm_credit_encode(struct m0_rm_credit *credit,
				    struct m0_buf       *buf)
{
	struct m0_bufvec	datum_buf;
	struct m0_bufvec_cursor cursor;

	M0_ENTRY("credit: %"PRIx64, credit->cr_datum);
	M0_PRE(buf != NULL);
	M0_PRE(credit->cr_ops != NULL);
	M0_PRE(credit->cr_ops->cro_len != NULL);
	M0_PRE(credit->cr_ops->cro_encode != NULL);

	buf->b_nob = credit->cr_ops->cro_len(credit);
	buf->b_addr = m0_alloc(buf->b_nob);
	if (buf->b_addr == NULL)
		return M0_ERR(-ENOMEM);

	datum_buf.ov_buf = &buf->b_addr;
	datum_buf.ov_vec.v_nr = 1;
	datum_buf.ov_vec.v_count = &buf->b_nob;

	m0_bufvec_cursor_init(&cursor, &datum_buf);
	return M0_RC(credit->cr_ops->cro_encode(credit, &cursor));
}
M0_EXPORTED(m0_rm_credit_encode);

M0_INTERNAL int m0_rm_credit_decode(struct m0_rm_credit *credit,
				    struct m0_buf       *buf)
{
	struct m0_bufvec	datum_buf = M0_BUFVEC_INIT_BUF(&buf->b_addr,
							       &buf->b_nob);
	struct m0_bufvec_cursor cursor;

	M0_ENTRY("credit: %llu",
		 (long long unsigned) credit->cr_datum);
	M0_PRE(credit->cr_ops != NULL);
	M0_PRE(credit->cr_ops->cro_decode != NULL);

	m0_bufvec_cursor_init(&cursor, &datum_buf);
	return M0_RC(credit->cr_ops->cro_decode(credit, &cursor));
}
M0_EXPORTED(m0_rm_credit_decode);

/** @} end of credit group */

/**
 * @name remote Code to deal with remote owners
 *
 * @{
 */
M0_INTERNAL int m0_rm_db_service_query(const char          *name,
				       struct m0_rm_remote *rem)
{
	/* Create search query for DB using name as key and
	 * find record  and assign service ID */
	rem->rem_state = REM_SERVICE_LOCATED;
	return 0;
}

M0_INTERNAL int m0_rm_remote_resource_locate(struct m0_rm_remote *rem)
{
	/* Send resource management fop to locate resource */
	rem->rem_state = REM_OWNER_LOCATED;
	return 0;
}

/**
 * A distributed resource location data-base is consulted to locate the service.
 */
static int service_locate(struct m0_rm_resource_type *rtype,
			  struct m0_rm_remote        *rem)
{
	struct m0_clink clink;
	int		rc;

	M0_PRE(m0_mutex_is_locked(&rtype->rt_lock));
	M0_PRE(rem->rem_state == REM_SERVICE_LOCATING);

	m0_clink_init(&clink, NULL);
	m0_clink_add(&rem->rem_signal, &clink);
	/*
	 * DB callback should assign value to rem_service and
	 * rem_state should be changed to REM_SERVICE_LOCATED.
	 */
	rc = m0_rm_db_service_query(rtype->rt_name, rem);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "m0_rm_db_service_query failed!\n");
		goto error;
	}
	if (rem->rem_state != REM_SERVICE_LOCATED)
		m0_chan_wait(&clink);
	if (rem->rem_state != REM_SERVICE_LOCATED)
		rc = -EINVAL;

error:
	m0_clink_del(&clink);
	m0_clink_fini(&clink);

	return M0_RC(rc);
}

/**
 * Sends a resource management fop to the service. The service responds
 * with the remote owner identifier (m0_rm_remote::rem_id) used for
 * further communications.
 */
static int resource_locate(struct m0_rm_resource_type *rtype,
			   struct m0_rm_remote        *rem)
{
	struct m0_clink clink;
	int		rc;

	M0_PRE(m0_mutex_is_locked(&rtype->rt_lock));
	M0_PRE(rem->rem_state == REM_OWNER_LOCATING);

	m0_clink_init(&clink, NULL);
	m0_clink_add(&rem->rem_signal, &clink);
	/*
	 * RPC callback should assign value to rem_id and
	 * rem_state should be set to REM_OWNER_LOCATED.
	 */
	rc = m0_rm_remote_resource_locate(rem);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "m0_rm_remote_resource_find failed!\n");
		goto error;
	}
	if (rem->rem_state != REM_OWNER_LOCATED)
		m0_chan_wait(&clink);
	if (rem->rem_state != REM_OWNER_LOCATED)
		rc = M0_ERR(-EINVAL);
error:
	m0_clink_del(&clink);
	m0_clink_fini(&clink);

	return M0_RC(rc);
}

M0_INTERNAL int m0_rm_net_locate(struct m0_rm_credit *credit,
				 struct m0_rm_remote *other)
{
	struct m0_rm_resource_type *rtype;
	struct m0_rm_resource	   *res;
	int			    rc;

	M0_PRE(other->rem_state == REM_INITIALISED);

	rtype = credit->cr_owner->ro_resource->r_type;
	other->rem_state = REM_SERVICE_LOCATING;
	rc = service_locate(rtype, other);
	if (rc != 0)
		goto error;

	other->rem_state = REM_OWNER_LOCATING;
	rc = resource_locate(rtype, other);
	if (rc != 0)
		goto error;

	/* Search for resource having resource id equal to remote id */
	m0_mutex_lock(&rtype->rt_lock);
	m0_tl_for (res, &rtype->rt_resources, res) {
		if (rtype->rt_ops->rto_is(res, other->rem_id)) {
			other->rem_resource = res;
			break;
		}
	} m0_tl_endfor;
	m0_mutex_unlock(&rtype->rt_lock);

error:
	return M0_RC(rc);
}
M0_EXPORTED(m0_rm_net_locate);

/** @} end of remote group */

/**
 * @name HA-notification Code to deal with HA notifications on creditors/debtors
 * death
 *
 * @{
 */

/**
 * Processes remote RM service failure. Corresponding remote instance of type
 * m0_rm_remote played one of two exclusive roles for local owners: debtor or
 * creditor. Both roles are handled in this function to make it generic.
 *
 * If remote instance was created to represent debtor part in the loans, then
 * these loans are instantly settled to cached list. If there are outgoing
 * revoke requests in progress to this remote, then they will eventually be
 * completed with successful return code (@see revoke_ast()).
 *
 * If remote instance was created to represent creditor of some local owner,
 * then further functioning of this owner is impossible, because all borrowed
 * credits should be dropped. The potential problem is that borrowed credits
 * could be sub-let to other owners or could be in active use ("held"). In order
 * to gracefully drop these credits owner does "self-windup" process. Eventually
 * this owner will transit to ROS_FINAL state.
 *
 * @note Remote structures (m0_rm_remote) are not destroyed until rm resource
 * finalisation, because there can be outgoing requests to them in progress.
 * Also remote can recover from failed state and become online again.
 */
static void rm_remote_death_handler(struct m0_rm_remote *remote)
{
	struct m0_rm_owner  *owner;
	struct m0_rm_credit *credit;
	struct m0_rm_loan   *loan;
        int                  rc = 0;

	remote->rem_dead = true;
	m0_tl_for(m0_owners, &remote->rem_resource->r_local, owner) {
		m0_tl_for(m0_rm_ur, &owner->ro_sublet, credit) {
			loan = bob_of(credit, struct m0_rm_loan, rl_credit,
				      &loan_bob);
			if (loan->rl_other == remote) {
				/* put the loan's credit back to CACHED */
				rc = m0_rm_loan_settle(owner, loan);
				/**
				 * @todo
				 * if rc != 0, then credits remain in sub-let
				 * list and can't be revoked anymore. Also we
				 * can't return error code to user. Maybe we
				 * should notify HA about error?
				 */
				if (rc != 0)
					M0_LOG(M0_WARN, "continuing even"
					       " after rc = %d with ep = %s",
					       rc, remote->rem_tracker.rht_ep);
			}
		} m0_tl_endfor;

		if (owner->ro_creditor == remote &&
		    owner_state(owner) == ROS_ACTIVE) {
			owner_windup_locked(owner);
		}
	} m0_tl_endfor;
	if (rc == 0)
		remote->rem_tracker.rht_state = M0_NC_FAILED;
}

/**
 * Processes HA notification saying remote is ONLINE. We are interested only in
 * the case when remote recovered from a failure. If remote is a debtor,
 * then local owner will accept requests from it again. If remote is a creditor,
 * then local owner regain an opportunity to satisfy incoming requests.
 *
 * @note It is assumed that all RPC sessions to remote are still valid and
 * operational.
 */
static void rm_remote_online_handler(struct m0_rm_remote *remote)
{
        struct m0_rm_owner *owner;

	remote->rem_dead = false;
	m0_tl_for(m0_owners, &remote->rem_resource->r_local, owner) {
		if (owner->ro_creditor == remote &&
		    owner_state(owner) == ROS_DEAD_CREDITOR) {
			owner_state_set(owner, ROS_ACTIVE);
		}
	} m0_tl_endfor;
        remote->rem_tracker.rht_state = M0_NC_ONLINE;
}

static struct m0_queue *remote_to_queue(struct m0_rm_remote *remote)
{
        return &remote->rem_resource->r_type->rt_ha_events;
}

/**
 * Callback that is associated with remote RM service configuration object state
 * change. If HA notification about service failure is accepted, then post AST
 * to resource type sm group to process remote failure under group lock.
 *
 * @note RM service can recover from failure in the feature. HA will send
 * notification with M0_NC_ONLINE ha state, so keep tracking RM service state.
 */
static bool rm_on_remote_death_cb(struct m0_clink *link)
{
	struct m0_rm_remote        *remote;
	struct m0_conf_obj         *obj;
	struct m0_rm_ha_event      *event;
	struct m0_rm_resource_type *r_type;
	enum m0_ha_obj_state        new_state;

	obj = container_of(link->cl_chan, struct m0_conf_obj, co_ha_chan);
	new_state = obj->co_ha_state;
	M0_LOG(M0_DEBUG, "link=%p new_state=%d", link, new_state);
	if (M0_IN(new_state, (M0_NC_FAILED,M0_NC_ONLINE))) {
		remote = container_of(link, struct m0_rm_remote,
				      rem_tracker.rht_clink);
		M0_ALLOC_PTR(event);
		if (event == NULL) {
			M0_LOG(M0_ERROR, "Insufficient memory.");
			return false;
		}
		event->rhe_tracker = &remote->rem_tracker;
                event->rhe_state   = new_state;
                r_type = remote->rem_resource->r_type;
                m0_queue_link_init(&event->rhe_link);
                m0_mutex_lock(&r_type->rt_queue_guard);
                m0_queue_put(remote_to_queue(remote), &event->rhe_link);
                m0_mutex_unlock(&r_type->rt_queue_guard);
                m0_clink_signal(&r_type->rt_sm_grp.s_clink);
	}
	return false;
}

/** @} end of HA-notification group */

/** @} end of rm group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
