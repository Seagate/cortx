/* -*- c -*- */
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
 * Original creation date: 30-Aug-2012
 */
#pragma once
#ifndef __MERO_CONF_OBJS_COMMON_H__
#define __MERO_CONF_OBJS_COMMON_H__

/* ================================================================
 * DO NOT #include "conf/objs/common.h"!
 * It should only be used by files in conf/objs/ directory.
 * ================================================================ */

#include "conf/obj.h"     /* m0_conf_obj */
#include "conf/obj_ops.h" /* m0_conf_obj_ops */
#include "conf/onwire.h"  /* m0_confx_obj */
#include "conf/dir.h"     /* m0_conf_dir_elems_match */
#include "fid/fid.h"
#include "lib/memory.h"   /* m0_free */
#include "lib/errno.h"    /* ENOMEM, ENOENT */
#include "lib/misc.h"     /* M0_IN */

enum {
#define X_CONF(_, NAME, ft_id) \
	M0_CONF__ ## NAME ## _FT_ID = ft_id,

M0_CONF_OBJ_TYPES
#undef X_CONF
};

#define M0_CONF__BOB_DEFINE(type, magic, check)                               \
const struct m0_bob_type type ## _bob = {                                     \
	.bt_name         = #type,                                             \
	.bt_magix_offset = M0_MAGIX_OFFSET(struct type,                       \
					   type ## _cast_field.co_con_magic), \
	.bt_magix        = magic,                                             \
	.bt_check        = check                                              \
};                                                                            \
M0_BOB_DEFINE(static, &type ## _bob, type)

#define M0_CONF__INVARIANT_DEFINE(name, type)                    \
static bool name(const struct m0_conf_obj *obj)                  \
{                                                                \
	return type ## _bob_check(container_of(obj, struct type, \
				    type ## _cast_field));       \
}                                                                \
struct __ ## type ## _semicolon_catcher

#define M0_CONF__CTOR_DEFINE(name, type, ops) \
static struct m0_conf_obj *name(void)         \
{                                             \
	struct type        *x;                \
	struct m0_conf_obj *ret;              \
					      \
	M0_ALLOC_PTR(x);                      \
	if (x == NULL)                        \
		return NULL;                  \
					      \
	type ## _bob_init(x);                 \
	ret = &x->type ## _cast_field;        \
	ret->co_ops = ops;                    \
	return ret;                           \
}                                             \
struct __ ## type ## _semicolon_catcher

struct conf_dir_entries {
	const struct m0_fid           *de_relfid;
	const struct m0_conf_obj_type *de_entry_type;
	const struct m0_fid_arr       *de_entries;
};
#define CONF_DIR_ENTRIES(relfid, entry_type, entries) \
	((struct conf_dir_entries){ (relfid), (entry_type), (entries) })

M0_INTERNAL int dir_create_and_populate(struct m0_conf_dir **result,
					const struct conf_dir_entries *de,
					struct m0_conf_obj *dir_parent);

struct conf_dir_encoding_pair {
	const struct m0_conf_dir *dep_src;
	struct m0_fid_arr        *dep_dest;
};

M0_INTERNAL int conf_dirs_encode(const struct conf_dir_encoding_pair *how,
				 size_t how_nr);

struct conf_dir_relation {
	struct m0_conf_dir  *dr_dir;
	const struct m0_fid *dr_relfid;
};

M0_INTERNAL int conf_dirs_lookup(struct m0_conf_obj            **out,
				 const struct m0_fid            *name,
				 const struct conf_dir_relation *rels,
				 size_t                          nr_rels);

M0_INTERNAL bool arrays_eq(const char **cached, const struct m0_bufs *flat);

M0_INTERNAL int arrfid_from_dir(struct m0_fid_arr *dest,
				const struct m0_conf_dir *dir);
M0_INTERNAL void arrfid_free(struct m0_fid_arr *arr);

M0_INTERNAL void confx_encode(struct m0_confx_obj *dest,
			      const struct m0_conf_obj *src);

M0_INTERNAL int u32arr_decode(const struct arr_u32 *src, uint32_t **dest);
M0_INTERNAL int u32arr_encode(struct arr_u32 *dest, const uint32_t *src,
			      uint32_t src_nr);
M0_INTERNAL bool u32arr_cmp(const struct arr_u32 *a1, const uint32_t *a2,
			    uint32_t a2_nr);
M0_INTERNAL void u32arr_free(struct arr_u32 *arr);

M0_INTERNAL int conf_pvers_decode(struct m0_conf_pver     ***dest,
				  const struct m0_fid_arr   *src,
				  struct m0_conf_cache      *cache);

M0_INTERNAL int conf_pvers_encode(struct m0_fid_arr          *dest,
				  const struct m0_conf_pver **src);

M0_INTERNAL int conf_obj_lookup_denied(const struct m0_conf_obj *parent,
				       const struct m0_fid *name,
				       struct m0_conf_obj **out);

M0_INTERNAL const struct m0_fid **
conf_obj_downlinks_none(const struct m0_conf_obj *obj);

#endif /* __MERO_CONF_OBJS_COMMON_H__ */

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
