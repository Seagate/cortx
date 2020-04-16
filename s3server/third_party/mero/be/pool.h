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

#pragma once

#ifndef __MERO_BE_POOL_H__
#define __MERO_BE_POOL_H__

#include "lib/misc.h"           /* M0_HAS_TYPE */
#include "lib/mutex.h"          /* m0_mutex */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/assert.h"         /* M0_BASSERT */
#include "lib/types.h"          /* uint64_t */

struct m0_be_op;
struct be_pool_queue_item;

/**
 * @addtogroup be
 *
 * @{
 */

struct m0_be_pool_cfg {
	/** Maximum number of pending requests. */
	unsigned bplc_q_size;
};

/**
 * BE pool is a collection of pre-allocated objects of a single type.
 * User is responsible for the objects allocation and filling of BE pool
 * using interface m0_be_pool_add().
 * m0_be_pool_get/put are used for obtaining and release a free object.
 *
 * Macros M0_BE_POOL_DESCR_DEFINE and M0_BE_POOL_DEFINE hide
 * structure m0_be_pool_descr and simplify interface of BE pool.
 */
struct m0_be_pool {
	struct m0_be_pool_cfg      bpl_cfg;
	/** Protects all lists. */
	struct m0_mutex            bpl_lock;
	/** List of free objects that can be get immediately. */
	struct m0_tl               bpl_free;
	/** List of objects that are in use. */
	struct m0_tl               bpl_used;
	/** Array of preallocated queue items. */
	struct be_pool_queue_item *bpl_q_items;
	/** List of free queue items. */
	struct m0_tl               bpl_q_free;
	/** List of pending queue items. */
	struct m0_tl               bpl_q_pending;
};

/**
 * A be pool object must have m0_be_pool_item field. BE pool maintains objects
 * using this structure.
 */
struct m0_be_pool_item {
	struct m0_tlink bpli_link;
	uint64_t        bpli_magic;
	uint64_t        bpli_pool_magic;
};

/**
 * BE pool object specific information.
 */
struct m0_be_pool_descr {
	/** string for debug purpose. */
	const char *bpld_name;
	/** offset of m0_be_pool_item field. */
	int         bpld_item_offset;
	/** offset of magic field. */
	int         bpld_magic_offset;
	/** magic value. */
	uint64_t    bpld_magic;

};

M0_INTERNAL int m0_be_pool_init(struct m0_be_pool     *pool,
				struct m0_be_pool_cfg *cfg);
M0_INTERNAL void m0_be_pool_fini(struct m0_be_pool *pool);

/** Adds an object to the pool. */
M0_INTERNAL void m0_be_pool_add(const struct m0_be_pool_descr *d,
				struct m0_be_pool             *pool,
				void                          *obj);
/** Removes an object from the pool and returns the object. */
M0_INTERNAL void *m0_be_pool_del(const struct m0_be_pool_descr *d,
				 struct m0_be_pool             *pool);

/**
 * Asynchronously gets an object from the pool. op is signalled on completion.
 */
M0_INTERNAL void m0_be_pool_get(const struct m0_be_pool_descr  *d,
				struct m0_be_pool              *pool,
				void                          **obj,
				struct m0_be_op                *op);
/** Returns the object to the pool. */
M0_INTERNAL void m0_be_pool_put(const struct m0_be_pool_descr *d,
				struct m0_be_pool             *pool,
				void                          *obj);

#define M0_BE_POOL_DESCR(hname, amb_type, pool_field, pool_magic_field, \
			 pool_magic)                                    \
{                                                                       \
	.bpld_name         = hname,                                     \
	.bpld_item_offset  = offsetof(amb_type, pool_field),            \
	.bpld_magic_offset = offsetof(amb_type, pool_magic_field),      \
	.bpld_magic        = pool_magic,                                \
};                                                                      \
M0_BASSERT(M0_HAS_TYPE(M0_FIELD_VALUE(amb_type, pool_field),            \
		       struct m0_be_pool_item));                        \
M0_BASSERT(M0_HAS_TYPE(M0_FIELD_VALUE(amb_type, pool_magic_field),	\
		       uint64_t))

#define M0_BE_POOL_DESCR_DEFINE(name, hname, scope, amb_type, pool_field, \
				pool_magic_field, pool_magic)             \
scope const struct m0_be_pool_descr name ## _pool_d =                     \
	M0_BE_POOL_DESCR(hname, amb_type, pool_field, pool_magic_field,   \
			 pool_magic)

#define M0_BE_POOL_DEFINE(name, scope, amb_type)                   \
                                                                   \
scope int name ## _be_pool_init(struct m0_be_pool     *pool,       \
				struct m0_be_pool_cfg *cfg)        \
{                                                                  \
	return m0_be_pool_init(pool, cfg);                         \
}                                                                  \
                                                                   \
scope void name ## _be_pool_fini(struct m0_be_pool *pool)          \
{                                                                  \
	m0_be_pool_fini(pool);                                     \
}                                                                  \
                                                                   \
scope void name ## _be_pool_add(struct m0_be_pool *pool,           \
				amb_type          *obj)            \
{                                                                  \
	m0_be_pool_add(&name ## _pool_d, pool, obj);               \
}                                                                  \
                                                                   \
scope amb_type * name ## _be_pool_del(struct m0_be_pool *pool)     \
{                                                                  \
	return (amb_type *)m0_be_pool_del(&name ## _pool_d, pool); \
}                                                                  \
                                                                   \
scope void name ## _be_pool_get(struct m0_be_pool  *pool,          \
				amb_type          **obj,           \
				struct m0_be_op    *op)            \
{                                                                  \
	m0_be_pool_get(&name ## _pool_d, pool, (void **)obj, op);  \
}                                                                  \
                                                                   \
scope void name ## _be_pool_put(struct m0_be_pool *pool,           \
				amb_type          *obj)            \
{                                                                  \
	m0_be_pool_put(&name ## _pool_d, pool, obj);               \
}                                                                  \
                                                                   \
struct __ ## name ## _terminate_me_with_a_semicolon { ; }

/** @} end of be group */
#endif /* __MERO_BE_POOL_H__ */

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
