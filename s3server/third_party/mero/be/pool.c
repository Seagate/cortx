/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 28-Sep-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/pool.h"

#include "lib/errno.h"          /* ENOMEM */
#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "be/op.h"              /* m0_be_op_active */

/**
 * @addtogroup be
 *
 * @{
 */

/** Represents a pending request when there is not free item in pool. */
struct be_pool_queue_item {
	void            **bplq_obj;
	struct m0_be_op  *bplq_op;
	struct m0_tlink   bplq_link;
	uint64_t          bplq_magic;
};

static const uint64_t BE_POOL_MAGIC_POISON = 0xCCCCCCCCCCCC;

M0_TL_DESCR_DEFINE(be_pool, "list of be_pool_items", static,
		   struct m0_be_pool_item, bpli_link, bpli_magic,
		   M0_BE_POOL_MAGIC, M0_BE_POOL_HEAD_MAGIC);
M0_TL_DEFINE(be_pool, static, struct m0_be_pool_item);

M0_TL_DESCR_DEFINE(be_pool_q, "list of be_pool_queue_items", static,
		   struct be_pool_queue_item, bplq_link, bplq_magic,
		   M0_BE_POOL_QUEUE_MAGIC, M0_BE_POOL_QUEUE_HEAD_MAGIC);
M0_TL_DEFINE(be_pool_q, static, struct be_pool_queue_item);

M0_INTERNAL int m0_be_pool_init(struct m0_be_pool     *pool,
				struct m0_be_pool_cfg *cfg)
{
	pool->bpl_cfg = *cfg;
	M0_ALLOC_ARR(pool->bpl_q_items, cfg->bplc_q_size);
	if (pool->bpl_q_items == NULL)
		return M0_ERR(-ENOMEM);

	m0_mutex_init(&pool->bpl_lock);
	be_pool_tlist_init(&pool->bpl_free);
	be_pool_tlist_init(&pool->bpl_used);
	be_pool_q_tlist_init(&pool->bpl_q_free);
	be_pool_q_tlist_init(&pool->bpl_q_pending);

	m0_forall(i, cfg->bplc_q_size,
		  be_pool_q_tlink_init_at(&pool->bpl_q_items[i],
					  &pool->bpl_q_free), true);
	return 0;
}

M0_INTERNAL void m0_be_pool_fini(struct m0_be_pool *pool)
{
	M0_PRE(be_pool_q_tlist_length(&pool->bpl_q_free) ==
	       pool->bpl_cfg.bplc_q_size);

	m0_forall(i, pool->bpl_cfg.bplc_q_size,
		  be_pool_q_tlink_del_fini(&pool->bpl_q_items[i]), true);
	be_pool_q_tlist_fini(&pool->bpl_q_pending);
	be_pool_q_tlist_fini(&pool->bpl_q_free);
	be_pool_tlist_fini(&pool->bpl_used);
	be_pool_tlist_fini(&pool->bpl_free);
	m0_mutex_fini(&pool->bpl_lock);
	m0_free(pool->bpl_q_items);
}

static void be_pool_lock(struct m0_be_pool *pool)
{
	m0_mutex_lock(&pool->bpl_lock);
}

static void be_pool_unlock(struct m0_be_pool *pool)
{
	m0_mutex_unlock(&pool->bpl_lock);
}

static bool be_pool_is_locked(struct m0_be_pool *pool)
{
	return m0_mutex_is_locked(&pool->bpl_lock);
}

static void *be_pool_amb(const struct m0_be_pool_descr *d,
			 struct m0_be_pool_item        *item)
{
	M0_PRE((unsigned long)item >= (unsigned long)d->bpld_item_offset);

	return (void *)item - d->bpld_item_offset;
}

static struct m0_be_pool_item *be_pool_item(const struct m0_be_pool_descr *d,
					    void                          *obj)
{
	return (struct m0_be_pool_item *)(obj + d->bpld_item_offset);
}

static uint64_t be_pool_magic(const struct m0_be_pool_descr *d,
			      void                          *obj)
{
	return *(uint64_t *)(obj + d->bpld_magic_offset);
}

static void be_pool_magic_set(const struct m0_be_pool_descr *d,
			      void                          *obj,
			      uint64_t                       magic)
{
	struct m0_be_pool_item *item = be_pool_item(d, obj);

	item->bpli_pool_magic = magic;
	*(uint64_t *)(obj + d->bpld_magic_offset) = magic;
}

static bool be_pool_obj__invariant(const struct m0_be_pool_descr *d,
				   void                          *obj)
{
	struct m0_be_pool_item *item = be_pool_item(d, obj);

	return _0C(item->bpli_pool_magic == d->bpld_magic) &&
	       _0C(be_pool_magic(d, obj) == d->bpld_magic);
}

static void be_pool_get(const struct m0_be_pool_descr  *d,
			struct m0_be_pool              *pool,
			void                          **obj)
{
	struct m0_be_pool_item *item;

	M0_PRE(be_pool_is_locked(pool));
	M0_PRE(!be_pool_tlist_is_empty(&pool->bpl_free));

	item = be_pool_tlist_head(&pool->bpl_free);
	be_pool_tlist_del(item);
	be_pool_tlist_add_tail(&pool->bpl_used, item);

	*obj = be_pool_amb(d, item);
	M0_POST(be_pool_obj__invariant(d, *obj));
}


static void be_pool_got_free_item(const struct m0_be_pool_descr *d,
				  struct m0_be_pool             *pool)
{
	struct be_pool_queue_item *qi;
	struct m0_be_op           *op;

	M0_PRE(be_pool_is_locked(pool));

	if (!be_pool_q_tlist_is_empty(&pool->bpl_q_pending)) {
		qi = be_pool_q_tlist_head(&pool->bpl_q_pending);
		be_pool_q_tlist_del(qi);
		be_pool_q_tlist_add(&pool->bpl_q_free, qi);
		be_pool_get(d, pool, qi->bplq_obj);
		op = qi->bplq_op;
		/*
		 * be_op can have a callback that calls be_pool functions.
		 * Therefore unlock pool here to prevent a deadlock.
		 * This function should be the last step of logically atomic
		 * operation with the pool lists.
		 */
		be_pool_unlock(pool);
		m0_be_op_done(op);
		be_pool_lock(pool);
	}
}

M0_INTERNAL void m0_be_pool_add(const struct m0_be_pool_descr *d,
				struct m0_be_pool             *pool,
				void                          *obj)
{
	struct m0_be_pool_item *item = be_pool_item(d, obj);

	be_pool_magic_set(d, obj, d->bpld_magic);

	be_pool_lock(pool);
	be_pool_tlink_init_at(item, &pool->bpl_free);
	be_pool_got_free_item(d, pool);
	be_pool_unlock(pool);

	M0_POST(be_pool_obj__invariant(d, obj));
}

M0_INTERNAL void *m0_be_pool_del(const struct m0_be_pool_descr *d,
				 struct m0_be_pool             *pool)
{
	struct m0_be_pool_item *item;
	void                   *obj = NULL;

	be_pool_lock(pool);
	item = be_pool_tlist_head(&pool->bpl_free);
	if (item != NULL) {
		obj = be_pool_amb(d, item);
		M0_ASSERT(be_pool_obj__invariant(d, obj));
		be_pool_magic_set(d, obj, BE_POOL_MAGIC_POISON);
		be_pool_tlink_del_fini(item);
	}
	be_pool_unlock(pool);

	return obj;
}

M0_INTERNAL void m0_be_pool_get(const struct m0_be_pool_descr  *d,
				struct m0_be_pool              *pool,
				void                          **obj,
				struct m0_be_op                *op)
{
	struct be_pool_queue_item *qi;
	bool                       got = false;

	m0_be_op_active(op);
	be_pool_lock(pool);
	if (!be_pool_tlist_is_empty(&pool->bpl_free)) {
		be_pool_get(d, pool, obj);
		got = true;
	} else {
		qi = be_pool_q_tlist_head(&pool->bpl_q_free);
		M0_ASSERT_INFO(qi != NULL, "pool=%p op=%p bplc_q_size=%u",
		               pool, op, pool->bpl_cfg.bplc_q_size);
		be_pool_q_tlist_del(qi);
		be_pool_q_tlist_add_tail(&pool->bpl_q_pending, qi);
		qi->bplq_obj = obj;
		qi->bplq_op  = op;
	}
	be_pool_unlock(pool);
	if (got)
		m0_be_op_done(op);
}

M0_INTERNAL void m0_be_pool_put(const struct m0_be_pool_descr *d,
				struct m0_be_pool             *pool,
				void                          *obj)
{
	struct m0_be_pool_item *item = be_pool_item(d, obj);

	M0_PRE(be_pool_obj__invariant(d, obj));

	be_pool_lock(pool);
	M0_PRE_EX(be_pool_tlist_contains(&pool->bpl_used, item));
	be_pool_tlist_del(item);
	be_pool_tlist_add(&pool->bpl_free, item);
	be_pool_got_free_item(d, pool);
	be_pool_unlock(pool);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
