/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 5-Jul-2016
 */

/**
 * @addtogroup conf_dir
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/obj.h"
#include "conf/obj_ops.h"  /* m0_conf_obj_invariant */
#include "conf/cache.h"    /* m0_conf_cache_lookup */
#include "mero/magic.h"    /* M0_CONF_OBJ_MAGIC, M0_CONF_DIR_MAGIC */
#include "lib/errno.h"     /* ENOMEM */

M0_TL_DESCR_DEFINE(m0_conf_dir, "m0_conf_dir::cd_items", M0_INTERNAL,
		   struct m0_conf_obj, co_dir_link, co_gen_magic,
		   M0_CONF_OBJ_MAGIC, M0_CONF_DIR_MAGIC);
M0_TL_DEFINE(m0_conf_dir, M0_INTERNAL, struct m0_conf_obj);

M0_INTERNAL void
m0_conf_dir_add(struct m0_conf_dir *dir, struct m0_conf_obj *obj)
{
	M0_PRE(m0_conf_obj_invariant(obj));
	M0_PRE(m0_conf_obj_type(obj) == dir->cd_item_type);

	m0_conf_child_adopt(&dir->cd_obj, obj);
	m0_conf_dir_tlist_add_tail(&dir->cd_items, obj);
}

M0_INTERNAL void
m0_conf_dir_del(struct m0_conf_dir *dir, struct m0_conf_obj *obj)
{
	M0_PRE(m0_conf_obj_invariant(obj));
	M0_PRE(m0_conf_obj_type(obj) == dir->cd_item_type);
	M0_PRE(_0C(obj->co_cache == dir->cd_obj.co_cache) &&
	       _0C(obj->co_parent == &dir->cd_obj));

	m0_conf_dir_tlist_del(obj);
}

M0_INTERNAL bool m0_conf_dir_elems_match(const struct m0_conf_dir *dir,
					 const struct m0_fid_arr  *fids)
{
	int i = 0;

	return m0_conf_dir_tlist_length(&dir->cd_items) == fids->af_count &&
		m0_tl_forall(m0_conf_dir, obj, &dir->cd_items,
			     m0_fid_eq(&fids->af_elems[i++], &obj->co_id));
}

/**
 * Constructs directory identifier.
 *
 * @todo This would produce non-unique identifier, if an object has two
 * different directories with the same children types. Perhaps relation fid
 * should be factored in somehow.
 */
static void conf_dir_id_build(struct m0_fid *out, const struct m0_fid *parent,
			      const struct m0_conf_obj_type *children_type)
{
	*out = *parent;
	m0_fid_tassume(out, &M0_CONF_DIR_TYPE.cot_ftype);
	/* clear the next 16 bits after fid type... */
	out->f_container &= ~0x00ffff0000000000ULL;
	/* ... place parent type there... */
	out->f_container |= ((uint64_t)m0_fid_type_getfid(parent)->ft_id) << 48;
	/* ... and place children type there. */
	out->f_container |= ((uint64_t)children_type->cot_ftype.ft_id) << 40;
}

static int conf_dir_new(struct m0_conf_cache *cache,
			const struct m0_fid *parent,
			const struct m0_fid *relfid,
			const struct m0_conf_obj_type *children_type,
			const struct m0_fid_arr *children_ids,
			struct m0_conf_dir **out)
{
	struct m0_fid       dir_id;
	struct m0_conf_obj *dir_obj;
	struct m0_conf_dir *dir;
	struct m0_conf_obj *child;
	uint32_t            i;
	int                 rc;

	M0_PRE(m0_fid_type_getfid(relfid) == &M0_CONF_RELFID_TYPE);
	M0_PRE(children_ids == NULL ||
	       m0_forall(i, children_ids->af_count,
			 m0_conf_fid_type(&children_ids->af_elems[i]) ==
			 children_type));
	M0_PRE(*out == NULL);

	conf_dir_id_build(&dir_id, parent, children_type);
	M0_ASSERT(m0_conf_cache_lookup(cache, &dir_id) == NULL);

	dir_obj = m0_conf_obj_create(&dir_id, cache);
	if (dir_obj == NULL)
		return M0_ERR(-ENOMEM);
	dir = M0_CONF_CAST(dir_obj, m0_conf_dir);

	dir->cd_item_type = children_type;
	dir->cd_relfid = *relfid;

	if (children_ids != NULL) {
		for (i = 0; i < children_ids->af_count; ++i) {
			rc = m0_conf_obj_find(cache, &children_ids->af_elems[i],
					      &child);
			if (rc != 0)
				goto err;
			if (m0_conf_dir_tlist_contains(&dir->cd_items, child)) {
				rc = M0_ERR_INFO(
					-EEXIST, FID_F": Directory element "
					FID_F" is not unique", FID_P(parent),
					FID_P(&child->co_id));
				goto err;
			}
			/* Link the directory and its element together. */
			m0_conf_dir_add(dir, child);
		}
	}
	rc = m0_conf_cache_add(cache, dir_obj);
	if (rc == 0) {
		dir_obj->co_status = M0_CS_READY;
		*out = dir;
		return M0_RC(0);
	}
err:
	if (children_ids != NULL) {
		/* Restore consistency. */
		m0_tl_teardown(m0_conf_dir, &dir->cd_items, child) {
			m0_conf_cache_del(cache, child);
		}
	}
	m0_conf_obj_delete(dir_obj);
	return M0_ERR(rc);
}

M0_INTERNAL int m0_conf_dir_new(struct m0_conf_obj *parent,
				const struct m0_fid *relfid,
				const struct m0_conf_obj_type *children_type,
				const struct m0_fid_arr *children_ids,
				struct m0_conf_dir **out)
{
	int rc;

	rc = conf_dir_new(parent->co_cache, &parent->co_id, relfid,
			  children_type, children_ids, out);
	if (rc == 0)
		m0_conf_child_adopt(parent, &(*out)->cd_obj);
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM
/** @} conf_dir */
