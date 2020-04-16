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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 *                  Trupti Patil <trupti_patil@xyratex.com>
 * Original creation date: 07/09/2010
 */

/**
 * @addtogroup layout
 * @{
 *
 * @section layout-thread Layout Threading and Concurrency Model
 * - Arrays from the struct m0_layout_domain, storing registered layout types
 *   and registered enum types viz. ld_type[] and ld_enum[] are protected by
 *   using m0_layout_domain::ld_lock.
 * - Reference count is maintained for each of the layout types and enum types.
 *   This is to help verify that no layout type or enum type gets unregistered
 *   while any of the in-memory layout object or enum object is using it.
 * - The list of the in-memory layout objects stored in the struct
 *   m0_layout_domain viz. ld_layout_list is protected by using
 *   m0_layout_domain::ld_lock.
 * - Reference count viz. m0_layout::l_ref is maintained for each of the
 *   in-memory layout object.
 *   - It is initialised to 1 during an in-memory layout object creation.
 *   - It gets incremented during every m0_layout_find() and m0_layout_lookup()
 *     operations so that the respective user has a hold on that in-memory
 *     layout object, during its usage. The user explicitly needs to release
 *     this reference once done with the usage, by using m0_layout_put().
 *   - User can explicitly acquire an additional reference on the layout using
 *     m0_layout_get() and needs to release it using m0_layout_put().
 *   - Whenever it is the last reference being released, the in-memory layout
 *     gets deleted.
 * - The in-memory layout object is protected by using m0_layout::l_lock.
 * - DB takes its own locks internally to guarantee that concurrent calls to
 *   data-base operations for different layouts do not mess with each other.
 * - m0_layout::l_lock is used to serialise data-base operations for a
 *   particular layout. This relates to the concept of "key locking" in the
 *   data-base theory.
 *
 * - m0_layout_domain::ld_lock is held during the following operations:
 *   - Registration and unregistration routines for various layout types and
 *     enum types.
 *   - While increasing/decreasing references on the layout types and enum
 *     types through layout_type_get(), layout_type_put(), enum_type_get()
 *     and enum_type_put().
 *   - While adding/deleting an entry to/from the layout list that happens
 *     through m0_layout__populate() and m0_layout_put() respectively.
 *   - While trying to locate an entry into the layout list using either
 *     m0_layout_find() or m0_layout_lookup().
 * - m0_layout::l_lock is held during the following operations:
 *   - The in-memory operations: m0_layout_get(), m0_layout_put(),
 *     m0_layout_user_count_inc() and m0_layout_user_count_dec().
 *   - The DB operation: m0_layout_lookup(), m0_layout_add()
 *     m0_layout_update() and m0_layout_delete().
 *   - The user is required to perform lt->lt_ops->lto_allocate() before
 *     invoking m0_layout_decode() on a buffer. lt->lt_ops->lto_allocate()
 *     has the m0_layout::l_lock held for the layout allocated. As an effect,
 *     m0_layout::l_lock is locked throughout the m0_layout_decode() operation.
 *   - The user is explicitly required to hold this lock while invoking
 *     m0_layout_encode().
 */

#include "lib/errno.h"
#include "lib/memory.h" /* M0_ALLOC_PTR() */
#include "lib/misc.h"   /* strlen(), M0_IN() */
#include "lib/vec.h"    /* m0_bufvec_cursor_step(), m0_bufvec_cursor_addr() */
#include "lib/bob.h"
#include "lib/finject.h"
#include "lib/hash.h"   /* m0_hash */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

#include "mero/magic.h"
#include "layout/layout_internal.h"
#include "layout/layout.h"
#include "pool/pool.h" /* M0_TL_DESCR_DECLARE(pools, M0_EXTERN) */

enum {
	LNET_MAX_PAYLOAD = 1 << 20,
};

extern struct m0_layout_type m0_pdclust_layout_type;
//extern struct m0_layout_enum_type m0_list_enum_type;
extern struct m0_layout_enum_type m0_linear_enum_type;

static const struct m0_bob_type layout_bob = {
	.bt_name         = "layout",
	.bt_magix_offset = offsetof(struct m0_layout, l_magic),
	.bt_magix        = M0_LAYOUT_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(static, &layout_bob, m0_layout);

static const struct m0_bob_type enum_bob = {
	.bt_name         = "enum",
	.bt_magix_offset = offsetof(struct m0_layout_enum, le_magic),
	.bt_magix        = M0_LAYOUT_ENUM_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(static, &enum_bob, m0_layout_enum);

static const struct m0_bob_type layout_instance_bob = {
	.bt_name         = "layout_instance",
	.bt_magix_offset = offsetof(struct m0_layout_instance, li_magic),
	.bt_magix        = M0_LAYOUT_INSTANCE_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(static, &layout_instance_bob, m0_layout_instance);

M0_TL_DESCR_DEFINE(layout, "layout-list", static,
		   struct m0_layout, l_list_linkage, l_magic,
		   M0_LAYOUT_MAGIC, M0_LAYOUT_HEAD_MAGIC);
M0_TL_DEFINE(layout, static, struct m0_layout);

M0_INTERNAL bool m0_layout__domain_invariant(const struct m0_layout_domain *dom)
{
	return dom != NULL;
}

static bool layout_invariant_internal(const struct m0_layout *l)
{
	return
		m0_layout_bob_check(l) &&
		l->l_id > 0 &&
		l->l_type != NULL &&
		l->l_dom->ld_type[l->l_type->lt_id] == l->l_type &&
		m0_layout__domain_invariant(l->l_dom) &&
		l->l_ops != NULL;
}

M0_INTERNAL bool m0_layout__allocated_invariant(const struct m0_layout *l)
{
	return
		layout_invariant_internal(l) &&
		m0_ref_read(&l->l_ref) == 1 &&
		l->l_user_count == 0;
}

M0_INTERNAL bool m0_layout__invariant(const struct m0_layout *l)
{
	/*
	 * l->l_ref is always going to be > 0 throughout the life of
	 * an in-memory layout except when 'its last reference is
	 * released through m0_layout_put() causing it to get deleted using
	 * l->l_ops->lo_fini()'. In that exceptional case, l->l_ref will be
	 * equal to 0.
	 */
	return
		layout_invariant_internal(l) &&
		m0_ref_read(&l->l_ref) >= 0 &&
		l->l_user_count >= 0;
}

M0_INTERNAL bool m0_layout__enum_invariant(const struct m0_layout_enum *e)
{
	return
		m0_layout_enum_bob_check(e) &&
		e->le_type != NULL &&
		ergo(!e->le_sl_is_set, e->le_sl == NULL) &&
		ergo(e->le_sl_is_set, e->le_sl != NULL) &&
		e->le_ops != NULL;
}

M0_INTERNAL bool
m0_layout__striped_allocated_invariant(const struct m0_striped_layout *stl)
{
	return
		stl != NULL &&
		stl->sl_enum == NULL &&
		m0_layout__allocated_invariant(&stl->sl_base);
}

M0_INTERNAL bool m0_layout__striped_invariant(const struct m0_striped_layout
					      *stl)
{
	return
		stl != NULL &&
		m0_layout__enum_invariant(stl->sl_enum) &&
		m0_layout__invariant(&stl->sl_base);
}

M0_INTERNAL bool m0_layout__instance_invariant(const struct m0_layout_instance
					       *li)
{
	return
		m0_layout_instance_bob_check(li) &&
		m0_fid_is_valid(&li->li_gfid) &&
		li->li_ops != NULL;
}

/** Adds a reference to the layout type. */
static void layout_type_get(struct m0_layout_domain *ldom,
			    struct m0_layout_type *lt)
{
	M0_PRE(ldom != NULL);
	M0_PRE(lt != NULL);

	m0_mutex_lock(&ldom->ld_lock);
	M0_PRE(lt == ldom->ld_type[lt->lt_id]);
	M0_CNT_INC(lt->lt_ref_count);
	m0_mutex_unlock(&ldom->ld_lock);
}

/** Releases a reference on the layout type. */
static void layout_type_put(struct m0_layout_domain *ldom,
			    struct m0_layout_type *lt)
{
	M0_PRE(ldom != NULL);
	M0_PRE(lt != NULL);

	m0_mutex_lock(&ldom->ld_lock);
	M0_PRE(lt == ldom->ld_type[lt->lt_id]);
	M0_CNT_DEC(lt->lt_ref_count);
	m0_mutex_unlock(&ldom->ld_lock);
}

/** Adds a reference on the enum type. */
static void enum_type_get(struct m0_layout_domain *ldom,
			  struct m0_layout_enum_type *let)
{
	M0_PRE(ldom != NULL);
	M0_PRE(let != NULL);

	m0_mutex_lock(&ldom->ld_lock);
	M0_PRE(let == ldom->ld_enum[let->let_id]);
	M0_CNT_INC(let->let_ref_count);
	m0_mutex_unlock(&ldom->ld_lock);
}

/** Releases a reference on the enum type. */
static void enum_type_put(struct m0_layout_domain *ldom,
			  struct m0_layout_enum_type *let)
{
	M0_PRE(ldom != NULL);
	M0_PRE(let != NULL);

	m0_mutex_lock(&ldom->ld_lock);
	M0_PRE(let == ldom->ld_enum[let->let_id]);
	M0_CNT_DEC(let->let_ref_count);
	m0_mutex_unlock(&ldom->ld_lock);
}

/**
 * Adds an entry in the layout list, with the specified layout pointer and id.
 */
M0_INTERNAL void m0_layout_add(struct m0_layout_domain *dom, struct m0_layout *l)
{
	M0_ENTRY("dom %p, lid %llu", dom, (unsigned long long)l->l_id);
	m0_mutex_lock(&dom->ld_lock);
	M0_PRE(m0_layout__list_lookup(dom, l->l_id, false) == NULL);
	layout_tlink_init_at(l, &l->l_dom->ld_layout_list);
	m0_mutex_unlock(&dom->ld_lock);
	M0_LEAVE("lid %llu", (unsigned long long)l->l_id);
}

/**
 * Looks up for an entry from the layout list, with the specified layout id.
 * If the entry is found, and if the argument ref_increment has its value
 * set to 'true' then it acquires an additional reference on the layout
 * object.
 * @pre m0_mutex_is_locked(&dom->ld_lock).
 * @param ref_increment Once layout with specified lid is found, an additional
 * reference is acquired on it if value of ref_increment is true.
 * @post ergo(l != NULL && ref_increment, m0_ref_read(&l->l_ref) > 1);
 */
M0_INTERNAL struct m0_layout *m0_layout__list_lookup(const struct
						     m0_layout_domain *dom,
						     uint64_t lid,
						     bool ref_increment)
{
	struct m0_layout *l;

	M0_PRE(m0_mutex_is_locked(&dom->ld_lock));

	l = m0_tl_find(layout, l, &dom->ld_layout_list, l->l_id == lid);
	if (l != NULL && ref_increment)
		/*
		 * The dom->ld_lock is held at this points that protects
		 * the deletion of a layout entry from the layout list.
		 * Hence, it is safe to increment the l->l_ref without
		 * acquiring the l->l_lock. Acquiring the l->l_lock here would
		 * have violated the locking sequence that 'first the layout
		 * lock should be held and then the domain lock'.
		 */
		m0_ref_get(&l->l_ref);
	return l;
}

/* Used for assertions. */
static struct m0_layout *list_lookup(struct m0_layout_domain *dom,
				     uint64_t lid)
{
	struct m0_layout *l;

	m0_mutex_lock(&dom->ld_lock);
	l = m0_layout__list_lookup(dom, lid, false);
	m0_mutex_unlock(&dom->ld_lock);
	return l;
}

/**
 * Initialises a layout with initial l_ref as 1, adds a reference on the
 * respective layout type.
 */
M0_INTERNAL void m0_layout__init(struct m0_layout *l,
				 struct m0_layout_domain *dom,
				 uint64_t lid,
				 struct m0_layout_type *lt,
				 const struct m0_layout_ops *ops)
{
	M0_PRE(l != NULL);
	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(lid > 0);
	M0_PRE(lt != NULL);
	M0_PRE(lt == dom->ld_type[lt->lt_id]);
	M0_PRE(ops != NULL);

	M0_ENTRY("lid %llu, layout-type-id %lu", (unsigned long long)lid,
		 (unsigned long)lt->lt_id);

	l->l_id         = lid;
	l->l_dom        = dom;
	l->l_user_count = 0;
	l->l_ops        = ops;
	l->l_type       = lt;

	m0_ref_init(&l->l_ref, 1, l->l_ops->lo_fini);
	layout_type_get(dom, lt);
	m0_mutex_init(&l->l_lock);
	m0_layout_bob_init(l);

	M0_POST(m0_layout__allocated_invariant(l));
	M0_LEAVE("lid %llu", (unsigned long long)lid);
}

/**
 * @post m0_layout__list_lookup(l->l_dom, l->l_id, false) == l
 */
M0_INTERNAL void m0_layout__populate(struct m0_layout *l, uint32_t user_count)
{
	M0_PRE(m0_layout__allocated_invariant(l));
	M0_PRE(user_count >= 0);

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	l->l_user_count = user_count;
	m0_layout_add(l->l_dom, l);
	M0_POST(m0_layout__invariant(l));
	M0_LEAVE("lid %llu", (unsigned long long)l->l_id);
}

M0_INTERNAL void m0_layout__fini_internal(struct m0_layout *l)
{
	M0_PRE(m0_mutex_is_not_locked(&l->l_lock));
	m0_mutex_fini(&l->l_lock);
	layout_type_put(l->l_dom, l->l_type);
	l->l_type = NULL;
	m0_layout_bob_fini(l);
}

/* Used only in case of exceptions or errors. */
M0_INTERNAL void m0_layout__delete(struct m0_layout *l)
{
	M0_PRE(m0_layout__allocated_invariant(l));
	M0_PRE(list_lookup(l->l_dom, l->l_id) != l);
	M0_PRE(m0_ref_read(&l->l_ref) == 1);

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	m0_layout__fini_internal(l);
	M0_LEAVE();
}

/** Finalises a layout, releases a reference on the respective layout type. */
M0_INTERNAL void m0_layout__fini(struct m0_layout *l)
{
	M0_PRE(m0_layout__invariant(l));
	M0_PRE(list_lookup(l->l_dom, l->l_id) == NULL);
	M0_PRE(m0_ref_read(&l->l_ref) == 0);

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	layout_tlink_fini(l);
	m0_layout__fini_internal(l);
	M0_LEAVE();
}

M0_INTERNAL void m0_layout__striped_init(struct m0_striped_layout *stl,
					 struct m0_layout_domain *dom,
					 uint64_t lid,
					 struct m0_layout_type *type,
					 const struct m0_layout_ops *ops)
{
	M0_PRE(stl != NULL);
	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(lid > 0);
	M0_PRE(type != NULL);
	M0_PRE(ops != NULL);

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	m0_layout__init(&stl->sl_base, dom, lid, type, ops);
	/* stl->sl_enum will be set through m0_layout__striped_populate(). */
	stl->sl_enum = NULL;
	M0_POST(m0_layout__striped_allocated_invariant(stl));
	M0_LEAVE("lid %llu", (unsigned long long)lid);
}

/**
 * Initialises a striped layout object, using provided enumeration object.
 * @pre The enumeration object e is already initialised by internally elevating
 * reference of the respective enum type.
 * @post Pointer to the m0_layout object is set back in the m0_layout_enum
 * object.
 */
M0_INTERNAL void m0_layout__striped_populate(struct m0_striped_layout *str_l,
					     struct m0_layout_enum *e,
					     uint32_t user_count)
{
	M0_PRE(m0_layout__striped_allocated_invariant(str_l));
	M0_PRE(e != NULL);

	M0_ENTRY("lid %llu, enum-type-id %lu",
		 (unsigned long long)str_l->sl_base.l_id,
		 (unsigned long)e->le_type->let_id);
	m0_layout__populate(&str_l->sl_base, user_count);
	str_l->sl_enum = e;
	str_l->sl_enum->le_sl_is_set = true;
	str_l->sl_enum->le_sl = str_l;

	/*
	 * m0_layout__enum_invariant() invoked internally from within
	 * m0_layout__striped_invariant() verifies that
	 * str_l->sl_base->le_sl is set appropriately, using the enum
	 * invariant.
	 */
	M0_POST(m0_layout__striped_invariant(str_l));
	M0_LEAVE("lid %llu", (unsigned long long)str_l->sl_base.l_id);
}

M0_INTERNAL void m0_layout__striped_delete(struct m0_striped_layout *stl)
{
	M0_PRE(m0_layout__striped_allocated_invariant(stl));

	M0_ENTRY("lid %llu", (unsigned long long)stl->sl_base.l_id);
	m0_layout__delete(&stl->sl_base);
	M0_LEAVE("lid %llu", (unsigned long long)stl->sl_base.l_id);
}

/**
 * Finalises a striped layout object.
 * @post The enum object which is part of striped layout object, is finalised
 * as well.
 */
M0_INTERNAL void m0_layout__striped_fini(struct m0_striped_layout *str_l)
{
	M0_PRE(m0_layout__striped_invariant(str_l));

	M0_ENTRY("lid %llu", (unsigned long long)str_l->sl_base.l_id);
	str_l->sl_enum->le_ops->leo_fini(str_l->sl_enum);
	m0_layout__fini(&str_l->sl_base);
	M0_LEAVE("lid %llu", (unsigned long long)str_l->sl_base.l_id);
}

/**
 * Initialises an enumeration object, adds a reference on the respective
 * enum type.
 */
M0_INTERNAL void m0_layout__enum_init(struct m0_layout_domain *dom,
				      struct m0_layout_enum *le,
				      struct m0_layout_enum_type *let,
				      const struct m0_layout_enum_ops *ops)
{
	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(le != NULL);
	M0_PRE(let != NULL);
	M0_PRE(let == dom->ld_enum[let->let_id]);
	M0_PRE(ops != NULL);

	M0_ENTRY("Enum-type-id %lu", (unsigned long)let->let_id);
	/* le->le_sl will be set through m0_layout__striped_populate(). */
	le->le_sl_is_set = false;
	le->le_sl = NULL;
	le->le_ops = ops;
	le->le_dom = dom;
	enum_type_get(dom, let);
	le->le_type = let;
	m0_layout_enum_bob_init(le);
	M0_LEAVE("Enum-type-id %lu", (unsigned long)let->let_id);
}

/**
 * Finalises an enum object, releases a reference on the respective enum
 * type.
 */
M0_INTERNAL void m0_layout__enum_fini(struct m0_layout_enum *le)
{
	M0_PRE(m0_layout__enum_invariant(le));

	M0_ENTRY("Enum-type-id %lu", (unsigned long)le->le_type->let_id);
	enum_type_put(le->le_dom, le->le_type);
	le->le_type = NULL;
	m0_layout_enum_bob_fini(le);
	M0_LEAVE();
}

M0_INTERNAL void m0_layout_enum_fini(struct m0_layout_enum *le)
{
	M0_PRE(le != NULL);
	M0_PRE(le->le_ops != NULL);
	M0_PRE(le->le_ops->leo_fini != NULL);

	le->le_ops->leo_fini(le);
}

M0_INTERNAL m0_bcount_t m0_layout__enum_max_recsize(struct m0_layout_domain
						    *dom)
{
	m0_bcount_t e_recsize;
	m0_bcount_t max_recsize = 0;
	uint32_t    i;

	M0_PRE(dom != NULL);

	/*
	 * Iterate over all the enum types to find the maximum possible
	 * recsize.
	 */
        for (i = 0; i < ARRAY_SIZE(dom->ld_enum); ++i) {
		if (dom->ld_enum[i] == NULL)
			continue;
                e_recsize = dom->ld_enum[i]->let_ops->leto_max_recsize();
		max_recsize = max64u(max_recsize, e_recsize);
        }
	return max_recsize;
}

/**
 * Maximum possible size for a record in the layouts table (without
 * considering the data in the tables other than the layouts) is maintained in
 * m0_layout_domain::ld_max_recsize.
 * This function updates m0_layout_domain::ld_max_recsize, by re-calculating it.
 */
static void max_recsize_update(struct m0_layout_domain *dom)
{
	uint32_t    i;
	m0_bcount_t recsize;
	m0_bcount_t max_recsize = 0;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(m0_mutex_is_locked(&dom->ld_lock));

	/*
	 * Iterate over all the layout types to find the maximum possible
	 * recsize.
	 */
	for (i = 0; i < ARRAY_SIZE(dom->ld_type); ++i) {
		if (dom->ld_type[i] == NULL)
			continue;
		recsize = dom->ld_type[i]->lt_ops->lto_max_recsize(dom);
		max_recsize = max64u(max_recsize, recsize);
	}
	dom->ld_max_recsize = sizeof(struct m0_layout_rec) + max_recsize;
}

/**
 * Adds a M0_LOG record (trace record), indicating failure, along with a short
 * error message string and the error code.
 *
 * @param fn_name  Function name for the trace record.
 * @param err_msg  Message for the trace record.
 * @param lid      Layout id for the trace record.
 * @param rc       Return code for the trace records.
 */
M0_INTERNAL void m0_layout__log(const char         *fn_name,
				const char         *err_msg,
				uint64_t            lid,
				int                 rc)
{
	M0_PRE(fn_name != NULL);
	M0_PRE(err_msg != NULL);
	M0_PRE(rc < 0);

	/* Trace record logging. */
	M0_LOG(M0_DEBUG, "%s(): lid %llu, %s, rc %d",
	       (const char *)fn_name, (unsigned long long)lid,
	       (const char *)err_msg, rc);
}

M0_INTERNAL int m0_layouts_init(void)
{
	return 0;
}

M0_INTERNAL void m0_layouts_fini(void)
{
}

M0_INTERNAL int m0_layout_domain_init(struct m0_layout_domain *dom)
{
	int rc = 0;

	M0_PRE(dom != NULL);

	M0_SET0(dom);

	if (M0_FI_ENABLED("table_init_err"))
		{ rc = L_TABLE_INIT_ERR; goto err1_injected; }
err1_injected:
	if (rc != 0) {
		m0_layout__log("m0_layout_domain_init",
			       "m0_table_init() failed",
			       LID_NONE, rc);
		return M0_RC(rc);
	}
	layout_tlist_init(&dom->ld_layout_list);
	m0_mutex_init(&dom->ld_lock);
	M0_POST(m0_layout__domain_invariant(dom));
	return M0_RC(rc);
}

M0_INTERNAL void m0_layout_domain_fini(struct m0_layout_domain *dom)
{
	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(m0_mutex_is_not_locked(&dom->ld_lock));
	/*
	 * Verify that all the layout objects belonging to this domain have
	 * been finalised.
	 */
	M0_PRE(layout_tlist_is_empty(&dom->ld_layout_list));

	/* Verify that all the layout types are unregistered. */
	M0_PRE(m0_forall(i, ARRAY_SIZE(dom->ld_type), dom->ld_type[i] == NULL));

	/* Verify that all the enum types are unregistered. */
	M0_PRE(m0_forall(i, ARRAY_SIZE(dom->ld_enum), dom->ld_enum[i] == NULL));

	m0_mutex_fini(&dom->ld_lock);
	layout_tlist_fini(&dom->ld_layout_list);
}

M0_INTERNAL void m0_layout_domain_cleanup(struct m0_layout_domain *dom)
{
	int 		  count = 0;
	struct m0_layout *l;

	M0_ENTRY();
	M0_PRE(dom != NULL);

	while (!layout_tlist_is_empty(&dom->ld_layout_list)) {
		l = layout_tlist_head(&dom->ld_layout_list);
		m0_layout_put(l);
		count++;
	}
	if (count > 0)
		M0_LOG(M0_INFO, "Killed %d layout(s)", count);
	M0_LEAVE();
}

M0_INTERNAL int m0_layout_standard_types_register(struct m0_layout_domain *dom)
{
	int rc;

	M0_PRE(m0_layout__domain_invariant(dom));

	rc = m0_layout_type_register(dom, &m0_pdclust_layout_type);
	if (rc != 0)
		return M0_RC(rc);

/*	rc = m0_layout_enum_type_register(dom, &m0_list_enum_type);
	if (rc != 0) {
		m0_layout_type_unregister(dom, &m0_pdclust_layout_type);
		return M0_RC(rc);
	}*/

	rc = m0_layout_enum_type_register(dom, &m0_linear_enum_type);
	if (rc != 0) {
		m0_layout_type_unregister(dom, &m0_pdclust_layout_type);
//		m0_layout_enum_type_unregister(dom, &m0_list_enum_type);
		return M0_RC(rc);
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_layout_standard_types_unregister(struct m0_layout_domain
						     *dom)
{
	M0_PRE(m0_layout__domain_invariant(dom));

//	m0_layout_enum_type_unregister(dom, &m0_list_enum_type);
	m0_layout_enum_type_unregister(dom, &m0_linear_enum_type);
	m0_layout_type_unregister(dom, &m0_pdclust_layout_type);
}

M0_INTERNAL int m0_layout_type_register(struct m0_layout_domain *dom,
					struct m0_layout_type *lt)
{
	int rc;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(lt != NULL);
	M0_PRE(IS_IN_ARRAY(lt->lt_id, dom->ld_type));
	M0_PRE(lt->lt_ops != NULL);

	M0_ENTRY("Layout-type-id %lu, domain %p",
		 (unsigned long)lt->lt_id, dom);
	m0_mutex_lock(&dom->ld_lock);
	M0_PRE(dom->ld_type[lt->lt_id] == NULL);
	dom->ld_type[lt->lt_id] = lt;

	/* Allocate type specific schema data. */
	if (M0_FI_ENABLED("lto_reg_err"))
		{ rc = LTO_REG_ERR; goto err1_injected; }
	rc = lt->lt_ops->lto_register(dom, lt);
err1_injected:
	if (rc == 0) {
		max_recsize_update(dom);
	} else {
		m0_layout__log("m0_layout_type_register",
			       "lto_register() failed",
			       LID_NONE, rc);
		dom->ld_type[lt->lt_id] = NULL;
	}
	m0_mutex_unlock(&dom->ld_lock);
	M0_LEAVE("Layout-type-id %lu, rc %d", (unsigned long)lt->lt_id, rc);
	return M0_RC(rc);
}

M0_INTERNAL void m0_layout_type_unregister(struct m0_layout_domain *dom,
					   struct m0_layout_type *lt)
{
	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(lt != NULL);
	M0_PRE(dom->ld_type[lt->lt_id] == lt); /* Registered layout type */
	M0_PRE(lt->lt_ops != NULL);

	M0_ENTRY("Layout-type-id %lu, domain %p",
		 (unsigned long)lt->lt_id, dom);
	m0_mutex_lock(&dom->ld_lock);
	lt->lt_ops->lto_unregister(dom, lt);
	dom->ld_type[lt->lt_id] = NULL;
	max_recsize_update(dom);
	m0_mutex_unlock(&dom->ld_lock);
	M0_LEAVE("Layout-type-id %lu", (unsigned long)lt->lt_id);
}

M0_INTERNAL int m0_layout_enum_type_register(struct m0_layout_domain *dom,
					     struct m0_layout_enum_type *let)
{
	int rc;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(let != NULL);
	M0_PRE(IS_IN_ARRAY(let->let_id, dom->ld_enum));
	M0_PRE(let->let_ops != NULL);

	M0_ENTRY("Enum_type_id %lu, domain %p",
		 (unsigned long)let->let_id, dom);
	m0_mutex_lock(&dom->ld_lock);
	M0_PRE(dom->ld_enum[let->let_id] == NULL);
	dom->ld_enum[let->let_id] = let;

	/* Allocate enum type specific schema data. */
	if (M0_FI_ENABLED("leto_reg_err"))
		{ rc = LETO_REG_ERR; goto err1_injected; }
	rc = let->let_ops->leto_register(dom, let);
err1_injected:
	if (rc == 0) {
		max_recsize_update(dom);
	} else {
		m0_layout__log("m0_layout_enum_type_register",
			       "leto_register() failed",
			       LID_NONE, rc);
		dom->ld_enum[let->let_id] = NULL;
	}
	m0_mutex_unlock(&dom->ld_lock);
	M0_LEAVE("Enum_type_id %lu, rc %d", (unsigned long)let->let_id, rc);
	return M0_RC(rc);
}

M0_INTERNAL void m0_layout_enum_type_unregister(struct m0_layout_domain *dom,
						struct m0_layout_enum_type *let)
{
	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(let != NULL);
	M0_PRE(dom->ld_enum[let->let_id] == let); /* Registered enum type */

	M0_ENTRY("Enum_type_id %lu, domain %p",
		 (unsigned long)let->let_id, dom);
	m0_mutex_lock(&dom->ld_lock);
	let->let_ops->leto_unregister(dom, let);
	dom->ld_enum[let->let_id] = NULL;
	max_recsize_update(dom);
	m0_mutex_unlock(&dom->ld_lock);
	M0_LEAVE("Enum_type_id %lu", (unsigned long)let->let_id);
}

M0_INTERNAL uint64_t m0_layout_find_by_buffsize(struct m0_layout_domain *dom,
						struct m0_fid *pver,
						size_t buffsize)
{
	uint64_t lid = M0_DEFAULT_LAYOUT_ID;
	struct m0_pdclust_attr *pa;
	struct m0_layout *l;
	uint64_t hash;
	int i;

	m0_mutex_lock(&dom->ld_lock);
	for (i = M0_DEFAULT_LAYOUT_ID; i < m0_lid_to_unit_map_nr; ++i) {
		/* Current BE max tx size constraints. */
		if (m0_lid_to_unit_map[i] > 4 * LNET_MAX_PAYLOAD)
			break;
		hash = m0_pool_version2layout_id(pver, i);
		l = m0_layout__list_lookup(dom, hash, true);
		if (l != NULL) {
			pa = &m0_layout_to_pdl(l)->pl_attr;
			if (pa->pa_unit_size * pa->pa_N <= buffsize) {
				lid = i;
			} else {
				m0_ref_put(&l->l_ref);
				break;
			}
			m0_ref_put(&l->l_ref);
		}
	}

	hash = m0_pool_version2layout_id(pver, lid);
	l = m0_layout__list_lookup(dom, hash, true);
	if (l != NULL) {
		pa = &m0_layout_to_pdl(l)->pl_attr;
		M0_LOG(M0_INFO,
			"Found lid=%d (pver+lid hash=%lx, unit_size=%d, N=%d) "
			"by buffer size %d.", (int)lid, (unsigned long int)hash,
			(int)pa->pa_unit_size, (int)pa->pa_N, (int)buffsize);
		m0_ref_put(&l->l_ref);
	}
	m0_mutex_unlock(&dom->ld_lock);

	return lid;
}

M0_INTERNAL struct m0_layout *m0_layout_find(struct m0_layout_domain *dom,
					     uint64_t lid)
{
	struct m0_layout *l;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(lid != LID_NONE);

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	m0_mutex_lock(&dom->ld_lock);
	l = m0_layout__list_lookup(dom, lid, true);
	m0_mutex_unlock(&dom->ld_lock);

	M0_POST(ergo(l != NULL, m0_layout__invariant(l) &&
				m0_ref_read(&l->l_ref) > 1));
	M0_LEAVE("lid %llu, l_pointer %p", (unsigned long long)lid, l);
	return l;
}

M0_INTERNAL void m0_layout_get(struct m0_layout *l)
{
	M0_PRE(m0_layout__invariant(l));

	M0_ENTRY("lid %llu, ref_count %ld", (unsigned long long)l->l_id,
		 (long)m0_ref_read(&l->l_ref));
	m0_mutex_lock(&l->l_lock);
	M0_PRE(list_lookup(l->l_dom, l->l_id) == l);
	m0_ref_get(&l->l_ref);
	m0_mutex_unlock(&l->l_lock);
	M0_LEAVE("lid %llu", (unsigned long long)l->l_id);
}

M0_INTERNAL void m0_layout_put(struct m0_layout *l)
{
	bool killme;

	M0_PRE(m0_layout__invariant(l));

	M0_ENTRY("lid %llu, ref_count %ld", (unsigned long long)l->l_id,
		 (long)m0_ref_read(&l->l_ref));
	m0_mutex_lock(&l->l_lock);
	m0_mutex_lock(&l->l_dom->ld_lock);
	killme = m0_ref_read(&l->l_ref) == 1;
	if (killme)
		/*
		 * The layout should not be found anymore using
		 * m0_layout_find().
		 */
		layout_tlist_del(l);
	else
		m0_ref_put(&l->l_ref);
	m0_mutex_unlock(&l->l_dom->ld_lock);
	m0_mutex_unlock(&l->l_lock);

	/* Finalise outside of the domain lock to improve concurrency. */
	if (killme)
		m0_ref_put(&l->l_ref);
	M0_LEAVE();
}

M0_INTERNAL void m0_layout_user_count_inc(struct m0_layout *l)
{
	M0_PRE(m0_layout__invariant(l));

	M0_ENTRY("lid %llu, user_count %lu", (unsigned long long)l->l_id,
		 (unsigned long)l->l_user_count);
	m0_mutex_lock(&l->l_lock);
	M0_PRE(list_lookup(l->l_dom, l->l_id) == l);
	M0_CNT_INC(l->l_user_count);
	m0_mutex_unlock(&l->l_lock);
	M0_LEAVE("lid %llu", (unsigned long long)l->l_id);
}

M0_INTERNAL void m0_layout_user_count_dec(struct m0_layout *l)
{
	M0_PRE(m0_layout__invariant(l));

	M0_ENTRY("lid %llu, user_count %lu", (unsigned long long)l->l_id,
		 (unsigned long)l->l_user_count);
	m0_mutex_lock(&l->l_lock);
	M0_PRE(list_lookup(l->l_dom, l->l_id) == l);
	M0_CNT_DEC(l->l_user_count);
	m0_mutex_unlock(&l->l_lock);
	M0_LEAVE("lid %llu", (unsigned long long)l->l_id);
}

M0_INTERNAL int m0_layout_decode(struct m0_layout *l,
				 struct m0_bufvec_cursor *cur,
				 enum m0_layout_xcode_op op,
				 struct m0_be_tx *tx)
{
	struct m0_layout_rec *rec;
	int                   rc;

	M0_PRE(m0_layout__allocated_invariant(l));
	M0_PRE(m0_mutex_is_locked(&l->l_lock));
	M0_PRE(list_lookup(l->l_dom, l->l_id) == NULL);
	M0_PRE(cur != NULL);
	M0_PRE(m0_bufvec_cursor_step(cur) >= sizeof *rec);
	M0_PRE(M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op == M0_LXO_DB_LOOKUP, tx != NULL));

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);

	rec = m0_bufvec_cursor_addr(cur);
	/* Move the cursor to point to the layout type specific payload. */
	m0_bufvec_cursor_move(cur, sizeof *rec);
	/*
	 * It is fine if any of the layout does not contain any data in
	 * rec->lr_data[], unless it is required by the specific layout type,
	 * which will be caught by the respective lo_decode() implementation.
	 * Hence, ignoring the return status of m0_bufvec_cursor_move() here.
	 */

	if (M0_FI_ENABLED("attr_err"))
		{ rec->lr_lt_id = M0_LAYOUT_TYPE_MAX + 1; }
	if (!IS_IN_ARRAY(rec->lr_lt_id, l->l_dom->ld_type)) {
		m0_layout__log("m0_layout_decode", "Invalid layout type",
			       l->l_id, -EPROTO);
		return M0_ERR(-EPROTO);
	}
	M0_ASSERT(rec->lr_lt_id == l->l_type->lt_id);

	if (M0_FI_ENABLED("lo_decode_err"))
		{ rc = LO_DECODE_ERR; goto err1_injected; }
	rc = l->l_ops->lo_decode(l, cur, op, tx, rec->lr_user_count);
err1_injected:
	if (rc != 0)
		m0_layout__log("m0_layout_decode", "lo_decode() failed",
			       l->l_id, rc);

	M0_POST(ergo(rc == 0, m0_layout__invariant(l) &&
			      list_lookup(l->l_dom, l->l_id) == l));
	M0_POST(ergo(rc != 0, m0_layout__allocated_invariant(l)));
	M0_POST(m0_mutex_is_locked(&l->l_lock));
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)l->l_id, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_layout_encode(struct m0_layout *l,
				 enum m0_layout_xcode_op op,
				 struct m0_be_tx *tx,
				 struct m0_bufvec_cursor *out)
{
	struct m0_layout_rec  rec;
	m0_bcount_t           nbytes;
	int                   rc;

	M0_PRE(m0_layout__invariant(l));
	M0_PRE(m0_mutex_is_locked(&l->l_lock));
	M0_PRE(list_lookup(l->l_dom, l->l_id) == l);
	M0_PRE(M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_UPDATE,
			  M0_LXO_DB_DELETE, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op != M0_LXO_BUFFER_OP, tx != NULL));
	M0_PRE(out != NULL);
	M0_PRE(m0_bufvec_cursor_step(out) >= sizeof rec);

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	rec.lr_lt_id      = l->l_type->lt_id;
	rec.lr_user_count = l->l_user_count;
	nbytes = m0_bufvec_cursor_copyto(out, &rec, sizeof rec);
	M0_ASSERT(nbytes == sizeof rec);

	if (M0_FI_ENABLED("lo_encode_err"))
		{ rc = LO_ENCODE_ERR; goto err1_injected; }
	rc = l->l_ops->lo_encode(l, op, tx, out);
err1_injected:
	if (rc != 0)
		m0_layout__log("m0_layout_encode", "lo_encode() failed",
			       l->l_id, rc);

	M0_POST(m0_mutex_is_locked(&l->l_lock));
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)l->l_id, rc);
	return M0_RC(rc);
}

M0_INTERNAL m0_bcount_t m0_layout_max_recsize(const struct m0_layout_domain
					      *dom)
{
	M0_PRE(m0_layout__domain_invariant(dom));
	M0_POST(dom->ld_max_recsize >= sizeof (struct m0_layout_rec));
	return dom->ld_max_recsize;
}

M0_INTERNAL struct m0_striped_layout *m0_layout_to_striped(const struct
							   m0_layout *l)
{
	struct m0_striped_layout *stl;

	M0_PRE(m0_layout__invariant(l));
	stl = bob_of(l, struct m0_striped_layout, sl_base, &layout_bob);
	M0_ASSERT(m0_layout__striped_invariant(stl));
	return stl;
}

M0_INTERNAL struct m0_layout_enum *m0_striped_layout_to_enum(const struct
							     m0_striped_layout
							     *stl)
{
	M0_PRE(m0_layout__striped_invariant(stl));
	return stl->sl_enum;
}

M0_INTERNAL struct m0_layout_enum *m0_layout_to_enum(const struct m0_layout *l)
{
	struct m0_striped_layout *stl;

	M0_PRE(l != NULL);
	stl = bob_of(l, struct m0_striped_layout, sl_base, &layout_bob);
	M0_ASSERT(m0_layout__striped_invariant(stl));
	return stl->sl_enum;
}

M0_INTERNAL uint32_t m0_layout_enum_nr(const struct m0_layout_enum *e)
{
	M0_PRE(m0_layout__enum_invariant(e));
	return e->le_ops->leo_nr(e);
}

M0_INTERNAL void m0_layout_enum_get(const struct m0_layout_enum *e,
				    uint32_t idx,
				    const struct m0_fid *gfid,
				    struct m0_fid *out)
{
	M0_PRE(m0_layout__enum_invariant(e));
	e->le_ops->leo_get(e, idx, gfid, out);
}

M0_INTERNAL void m0_layout__instance_init(struct m0_layout_instance *li,
					  const struct m0_fid *gfid,
					  struct m0_layout *l,
					  const struct m0_layout_instance_ops
					  *ops)
{
	M0_PRE(li != NULL);
	M0_PRE(m0_layout__invariant(l));

	li->li_gfid = *gfid;
	li->li_l = l;
	li->li_ops = ops;
	m0_layout_instance_bob_init(li);
	m0_layout_get(l);
	M0_POST(m0_layout__instance_invariant(li));
}

M0_INTERNAL void m0_layout__instance_fini(struct m0_layout_instance *li)
{
	M0_PRE(m0_layout__instance_invariant(li));
	m0_layout_put(li->li_l);
	m0_layout_instance_bob_fini(li);
}

M0_INTERNAL int m0_layout_instance_build(struct m0_layout *l,
					 const struct m0_fid *fid,
					 struct m0_layout_instance **out)
{
	M0_PRE(m0_layout__invariant(l));
	M0_PRE(l->l_ops->lo_instance_build != NULL);

	return l->l_ops->lo_instance_build(l, fid, out);
}

M0_INTERNAL void m0_layout_instance_fini(struct m0_layout_instance *li)
{
	M0_PRE(m0_layout__instance_invariant(li));
	M0_PRE(li->li_ops->lio_fini != NULL);

	/* For example, see pdclust_instance_fini() in layout/pdclust.c */
	li->li_ops->lio_fini(li);
}

M0_INTERNAL struct m0_layout_enum *m0_layout_instance_to_enum(const struct
							      m0_layout_instance
							      *li)
{
	M0_PRE(m0_layout__instance_invariant(li));
	M0_PRE(li->li_ops->lio_to_enum != NULL);

	return li->li_ops->lio_to_enum(li);
}

M0_INTERNAL uint32_t m0_layout_enum_find(const struct m0_layout_enum *e,
					 const struct m0_fid *gfid,
					 const struct m0_fid *target)
{
	uint32_t      i;
	uint32_t      nr;
	struct m0_fid cob;

	nr = e->le_ops->leo_nr(e);
	for (i = 0; i < nr; ++i) {
		e->le_ops->leo_get(e, i, gfid, &cob);
		if (m0_fid_eq(&cob, target))
			return i;
	}
	return ~0;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end group layout */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
