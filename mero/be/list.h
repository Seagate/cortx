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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 29-May-2013
 */

#pragma once
#ifndef __MERO_BE_LIST_H__
#define __MERO_BE_LIST_H__

#include "format/format.h"      /* m0_format_header */
#include "format/format_xc.h"

/* import */
struct m0_be_tx;
struct m0_be_tx_credit;

/**
 * @defgroup be Meta-data back-end
 *
 * Design highlights:
 * - there are no init()/fini() functions as m0_be_list doesn't have
 *   volatile-only fields;
 * - there are no open()/close() functions;
 * - be/list doesn't take reference to a returning object, it may reside in
 *   unmapped memory.
 * @{
 */

enum m0_be_list_format_version {
	M0_BE_LIST_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_LIST_FORMAT_VERSION */
	/*M0_BE_LIST_FORMAT_VERSION_2,*/
	/*M0_BE_LIST_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_LIST_FORMAT_VERSION = M0_BE_LIST_FORMAT_VERSION_1
};

struct m0_be_list_descr {
	const char *bld_name;
	int         bld_link_offset;
	int         bld_link_magic_offset;
	uint64_t    bld_link_magic;
	uint64_t    bld_head_magic;
	size_t      bld_container_size;
};

struct m0_be_list_link {
	struct m0_be_list_link *bll_next;
	struct m0_be_list_link *bll_prev;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_list_head {
	struct m0_be_list_link *blh_head;
	struct m0_be_list_link *blh_tail;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_list {
	struct m0_format_header  bl_format_header;
	struct m0_be_list_head   bl_head;
	uint64_t                 bl_magic;
	struct m0_format_footer  bl_format_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** List operations that modify memory. */
enum m0_be_list_op {
	M0_BLO_CREATE,          /**< m0_be_list_create() */
	M0_BLO_DESTROY,         /**< m0_be_list_destroy() */
	M0_BLO_ADD,             /**< m0_be_list_add(),
				     m0_be_list_add_after(),
				     m0_be_list_add_before(),
				     m0_be_list_add_tail() */
	M0_BLO_DEL,             /**< m0_be_list_del() */
	M0_BLO_TLINK_CREATE,    /**< m0_be_tlink_create() */
	M0_BLO_TLINK_DESTROY,   /**< m0_be_tlink_destroy() */
	M0_BLO_NR
};

/**
 * Calculates the credit needed to perform @nr list operations of type
 * @optype and adds this credit to @accum.
 */
M0_INTERNAL void m0_be_list_credit(enum m0_be_list_op      optype,
				   m0_bcount_t             nr,
				   struct m0_be_tx_credit *accum);

/* -------------------------------------------------------------------------
 * Construction/Destruction:
 * ------------------------------------------------------------------------- */

M0_INTERNAL void m0_be_list_create(struct m0_be_list             *blist,
				   const struct m0_be_list_descr *descr,
				   struct m0_be_tx               *tx);
M0_INTERNAL void m0_be_list_destroy(struct m0_be_list             *blist,
				    const struct m0_be_list_descr *descr,
				    struct m0_be_tx               *tx);

M0_INTERNAL bool m0_be_list_is_empty(struct m0_be_list             *blist,
				     const struct m0_be_list_descr *descr);

/* m0_be_link_*() functions follow BE naming pattern and not m0_tlist naming. */
M0_INTERNAL void m0_be_tlink_create(void                          *obj,
				    const struct m0_be_list_descr *descr,
				    struct m0_be_tx               *tx);
M0_INTERNAL void m0_be_tlink_destroy(void                          *obj,
				     const struct m0_be_list_descr *descr,
				     struct m0_be_tx               *tx);


/* -------------------------------------------------------------------------
 * Iteration interfaces:
 * ------------------------------------------------------------------------- */

M0_INTERNAL void *m0_be_list_tail(struct m0_be_list             *blist,
				  const struct m0_be_list_descr *descr);

M0_INTERNAL void *m0_be_list_head(struct m0_be_list             *blist,
				  const struct m0_be_list_descr *descr);

M0_INTERNAL void *m0_be_list_prev(struct m0_be_list             *blist,
				  const struct m0_be_list_descr *descr,
				  const void                    *obj);

M0_INTERNAL void *m0_be_list_next(struct m0_be_list             *blist,
				  const struct m0_be_list_descr *descr,
				  const void                    *obj);

/* -------------------------------------------------------------------------
 * Modification interfaces:
 * ------------------------------------------------------------------------- */

M0_INTERNAL void        m0_be_list_add(struct m0_be_list             *blist,
				       const struct m0_be_list_descr *descr,
				       struct m0_be_tx               *tx,
				       void                          *obj);

M0_INTERNAL void  m0_be_list_add_after(struct m0_be_list             *blist,
				       const struct m0_be_list_descr *descr,
				       struct m0_be_tx               *tx,
				       void                          *obj,
				       void                          *obj_new);

M0_INTERNAL void m0_be_list_add_before(struct m0_be_list             *blist,
				       const struct m0_be_list_descr *descr,
				       struct m0_be_tx               *tx,
				       void                          *obj,
				       void                          *obj_new);

M0_INTERNAL void   m0_be_list_add_tail(struct m0_be_list             *blist,
				       const struct m0_be_list_descr *descr,
				       struct m0_be_tx               *tx,
				       void                          *obj);

M0_INTERNAL void        m0_be_list_del(struct m0_be_list             *blist,
				       const struct m0_be_list_descr *descr,
				       struct m0_be_tx               *tx,
				       void                          *obj);

/* -------------------------------------------------------------------------
 * Type-safe macros:
 * ------------------------------------------------------------------------- */

/**
 * Initialisator for BE list descriptor.
 */
#define M0_BE_LIST_DESCR(name, amb_type, link_field, magic_field,       \
			 link_magic, head_magic)                        \
{                                                                       \
	.bld_name              = name,                                  \
	.bld_link_offset       = offsetof(amb_type, link_field),        \
	.bld_link_magic_offset = offsetof(amb_type, magic_field),       \
	.bld_link_magic        = link_magic,                            \
	.bld_head_magic        = head_magic,                            \
	.bld_container_size    = sizeof(amb_type),                      \
};                                                                      \
M0_BASSERT(M0_HAS_TYPE(M0_FIELD_VALUE(amb_type, link_field),            \
		       struct m0_be_list_link));                        \
M0_BASSERT(M0_HAS_TYPE(M0_FIELD_VALUE(amb_type, magic_field),           \
		       uint64_t))

/**
 * Definition of BE list descriptor.
 */
#define M0_BE_LIST_DESCR_DEFINE(name, hname, scope, amb_type, link_field, \
				magic_field, link_magic, head_magic)      \
scope const struct m0_be_list_descr name ## _be_list_d =                  \
	M0_BE_LIST_DESCR(hname, amb_type, link_field, magic_field,        \
			 link_magic, head_magic)

/**
 * Declaration of typed functions.
 */
#define M0_BE_LIST_DECLARE(name, scope, amb_type)                              \
                                                                               \
scope void name ## _be_list_credit(enum m0_be_list_op      optype,             \
				   m0_bcount_t             nr,                 \
				   struct m0_be_tx_credit *accum);             \
scope void name ## _be_list_create(struct m0_be_list *blist,                   \
				   struct m0_be_tx   *tx);                     \
scope void name ## _be_list_destroy(struct m0_be_list *blist,                  \
				    struct m0_be_tx   *tx);                    \
scope bool name ## _be_list_is_empty(struct m0_be_list *blist);                \
scope void name ## _be_tlink_create(amb_type        *obj,                      \
				    struct m0_be_tx *tx);                      \
scope void name ## _be_tlink_destroy(amb_type        *obj,                     \
				     struct m0_be_tx *tx);                     \
scope amb_type *name ## _be_list_tail(struct m0_be_list *blist);               \
scope amb_type *name ## _be_list_head(struct m0_be_list *blist);               \
scope amb_type *name ## _be_list_prev(struct m0_be_list *blist,                \
				      const amb_type    *obj);                 \
scope amb_type *name ## _be_list_next(struct m0_be_list *blist,                \
				      const amb_type    *obj);                 \
scope void name ## _be_list_add(struct m0_be_list *blist,                      \
				struct m0_be_tx   *tx,                         \
				amb_type          *obj);                       \
scope void name ## _be_list_add_after(struct m0_be_list *blist,                \
				      struct m0_be_tx   *tx,                   \
				      amb_type          *obj,                  \
				      amb_type          *obj_new);             \
scope void name ## _be_list_add_before(struct m0_be_list *blist,               \
				       struct m0_be_tx   *tx,                  \
				       amb_type          *obj,                 \
				       amb_type          *obj_new);            \
scope void name ## _be_list_add_tail(struct m0_be_list *blist,                 \
				     struct m0_be_tx   *tx,                    \
				     amb_type          *obj);                  \
scope void name ## _be_list_del(struct m0_be_list *blist,                      \
				struct m0_be_tx   *tx,                         \
				amb_type          *obj)


/**
 * Definition of typed functions.
 */
#define M0_BE_LIST_DEFINE(name, scope, amb_type)                               \
                                                                               \
scope M0_UNUSED void name ## _be_list_credit(enum m0_be_list_op      optype,   \
					     m0_bcount_t             nr,       \
					     struct m0_be_tx_credit *accum)    \
{                                                                              \
	m0_be_list_credit(optype, nr, accum);                                  \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_list_create(struct m0_be_list *blist,         \
					     struct m0_be_tx   *tx)            \
{                                                                              \
	m0_be_list_create(blist, &name ## _be_list_d, tx);                     \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_list_destroy(struct m0_be_list *blist,        \
					      struct m0_be_tx   *tx)           \
{                                                                              \
	m0_be_list_destroy(blist, &name ## _be_list_d, tx);                    \
}                                                                              \
                                                                               \
scope M0_UNUSED bool name ## _be_list_is_empty(struct m0_be_list *blist)       \
{                                                                              \
	return m0_be_list_is_empty(blist, &name ## _be_list_d);                \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_tlink_create(amb_type        *obj,            \
					      struct m0_be_tx *tx)             \
{                                                                              \
	m0_be_tlink_create(obj, &name ## _be_list_d, tx);                      \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_tlink_destroy(amb_type        *obj,           \
					       struct m0_be_tx *tx)            \
{                                                                              \
	m0_be_tlink_destroy(obj, &name ## _be_list_d, tx);                     \
}                                                                              \
                                                                               \
scope M0_UNUSED amb_type *name ## _be_list_tail(struct m0_be_list *blist)      \
{                                                                              \
	return (amb_type *)m0_be_list_tail(blist, &name ## _be_list_d);        \
}                                                                              \
                                                                               \
scope M0_UNUSED amb_type *name ## _be_list_head(struct m0_be_list *blist)      \
{                                                                              \
	return (amb_type *)m0_be_list_head(blist, &name ## _be_list_d);        \
}                                                                              \
                                                                               \
scope M0_UNUSED amb_type *name ## _be_list_prev(struct m0_be_list *blist,      \
						const amb_type    *obj)        \
{                                                                              \
	return (amb_type *)m0_be_list_prev(blist, &name ## _be_list_d, obj);   \
}                                                                              \
                                                                               \
scope M0_UNUSED amb_type *name ## _be_list_next(struct m0_be_list *blist,      \
						const amb_type    *obj)        \
{                                                                              \
	return (amb_type *)m0_be_list_next(blist, &name ## _be_list_d, obj);   \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_list_add(struct m0_be_list *blist,            \
					  struct m0_be_tx   *tx,               \
					  amb_type          *obj)              \
{                                                                              \
	m0_be_list_add(blist, &name ## _be_list_d, tx, obj);                   \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_list_add_after(struct m0_be_list *blist,      \
						struct m0_be_tx   *tx,         \
						amb_type          *obj,        \
						amb_type          *obj_new)    \
{                                                                              \
	m0_be_list_add_after(blist, &name ## _be_list_d, tx, obj, obj_new);    \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_list_add_before(struct m0_be_list *blist,     \
						 struct m0_be_tx   *tx,        \
						 amb_type          *obj,       \
						 amb_type          *obj_new)   \
{                                                                              \
	m0_be_list_add_before(blist, &name ## _be_list_d, tx, obj, obj_new);   \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_list_add_tail(struct m0_be_list *blist,       \
					       struct m0_be_tx   *tx,          \
					       amb_type          *obj)         \
{                                                                              \
	m0_be_list_add_tail(blist, &name ## _be_list_d, tx, obj);              \
}                                                                              \
                                                                               \
scope M0_UNUSED void name ## _be_list_del(struct m0_be_list *blist,            \
					  struct m0_be_tx   *tx,               \
					  amb_type          *obj)              \
{                                                                              \
	m0_be_list_del(blist, &name ## _be_list_d, tx, obj);                   \
}                                                                              \
                                                                               \
struct __ ## name ## __be_list_terminate_me_with_a_semicolon { ; }


/* -------------------------------------------------------------------------
 * List iteration macros.
 * Refer to lib/tlist.h for detailed description and usecases.
 * ------------------------------------------------------------------------- */

#define m0_be_list_for(name, head, obj)                                    \
do {                                                                       \
	const struct m0_be_list_descr *__descr = &name ## _be_list_d;      \
	void                          *__bl;                               \
	struct m0_be_list             *__head = (head);                    \
                                                                           \
	for (obj = m0_be_list_head(__head, __descr);                       \
	     obj != NULL &&                                                \
	     ((void)(__bl = m0_be_list_next(__head, __descr, obj)), true); \
	     obj = __bl)

#define m0_be_list_endfor ;(void)__bl; } while (0)

#define m0_be_list_forall(name, var, head, ...)                 \
({                                                              \
	void *var;                                              \
                                                                \
	m0_be_list_for(name, head, var) {                       \
		if (!({ __VA_ARGS__ ; }))                       \
			break;                                  \
	} m0_be_list_endfor;                                    \
	var == NULL;                                            \
})

/** @} end of be group */
#endif /* __MERO_BE_LIST_H__ */

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
