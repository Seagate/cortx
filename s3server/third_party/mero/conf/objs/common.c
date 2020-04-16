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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"

#define X_CONF(name, key) \
const struct m0_fid M0_CONF_ ## name ## _FID = M0_FID_TINIT('/', 0, (key));

M0_CONF_REL_FIDS
#undef X_CONF

M0_INTERNAL int dir_create_and_populate(struct m0_conf_dir **result,
					const struct conf_dir_entries *de,
					struct m0_conf_obj *dir_parent)
{
	return M0_RC(m0_conf_dir_new(dir_parent, de->de_relfid,
				     de->de_entry_type, de->de_entries,
				     result));
}

M0_INTERNAL int
conf_dirs_encode(const struct conf_dir_encoding_pair *how, size_t how_nr)
{
	const struct conf_dir_encoding_pair *p;
	size_t                               i;
	int                                  rc = 0;

	for (i = 0; i < how_nr; ++i) {
		p = &how[i];
		M0_ASSERT(_0C(p->dep_dest->af_count == 0) &&
			  _0C(p->dep_dest->af_elems == NULL));
		rc = arrfid_from_dir(p->dep_dest, p->dep_src);
		if (rc != 0)
			break;
	}
	if (rc != 0) {
		while (i > 0)
			arrfid_free(how[--i].dep_dest);
	}
	return M0_RC(rc);
}

M0_INTERNAL int conf_dirs_lookup(struct m0_conf_obj            **out,
				 const struct m0_fid            *name,
				 const struct conf_dir_relation *rels,
				 size_t                          nr_rels)
{
	size_t i;

	for (i = 0; i < nr_rels; ++i) {
		if (m0_fid_eq(rels[i].dr_relfid, name)) {
			*out = &rels[i].dr_dir->cd_obj;
			M0_POST(m0_conf_obj_invariant(*out));
			return 0;
		}
	}
	return -ENOENT;
}

M0_INTERNAL int
arrfid_from_dir(struct m0_fid_arr *dest, const struct m0_conf_dir *dir)
{
	struct m0_conf_obj *obj;
	struct m0_fid      *fid;

	*dest = (struct m0_fid_arr){
		.af_count = m0_conf_dir_len(dir)
	};
	if (dest->af_count == 0)
		return 0;

	M0_ALLOC_ARR(dest->af_elems, dest->af_count);
	if (dest->af_elems == NULL)
		return M0_ERR(-ENOMEM);

	fid = dest->af_elems;
	m0_tl_for (m0_conf_dir, &dir->cd_items, obj) {
		*fid++ = obj->co_id;
	} m0_tl_endfor;
	return 0;
}

M0_INTERNAL void arrfid_free(struct m0_fid_arr *arr)
{
	m0_free0(&arr->af_elems);
}

M0_INTERNAL void
confx_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	dest->xo_u.u_header.ch_id = src->co_id;
	dest->xo_type = m0_conf_obj_type(src)->cot_ftype.ft_id;
}

M0_INTERNAL int u32arr_decode(const struct arr_u32 *src, uint32_t **dest)
{
	M0_PRE(src->au_count != 0 && src->au_elems != NULL);

	M0_ALLOC_ARR(*dest, src->au_count);
	if (*dest == NULL)
		return M0_ERR(-ENOMEM);

	memcpy(*dest, src->au_elems, src->au_count * sizeof *dest[0]);
	return 0;
}

M0_INTERNAL int
u32arr_encode(struct arr_u32 *dest, const uint32_t *src, uint32_t src_nr)
{
	M0_PRE((src == NULL) == (src_nr == 0));

	if (src != NULL) {
		M0_ALLOC_ARR(dest->au_elems, src_nr);
		if (dest->au_elems == NULL)
			return M0_ERR(-ENOMEM);
		dest->au_count = src_nr;
		memcpy(dest->au_elems, src,
		       dest->au_count * sizeof dest->au_elems[0]);
	}
	return 0;
}

M0_INTERNAL bool
u32arr_cmp(const struct arr_u32 *a1, const uint32_t *a2, uint32_t a2_nr)
{
	return a1->au_count == a2_nr &&
		m0_forall(i, a2_nr, a1->au_elems[i] == a2[i]);
}

M0_INTERNAL void u32arr_free(struct arr_u32 *arr)
{
	m0_free0(&arr->au_elems);
	arr->au_count = 0;
}

/*
 * XXX REFACTORME
 * This code resembles that of profile_decode() and m0_bufs_to_strings().
 */
M0_INTERNAL int conf_pvers_decode(struct m0_conf_pver     ***dest,
				  const struct m0_fid_arr   *src,
				  struct m0_conf_cache      *cache)
{
	uint32_t            i;
	struct m0_conf_obj *obj;

	M0_PRE(*dest == NULL);
	M0_PRE((src->af_count == 0) == (src->af_elems == NULL));

	if (src->af_count == 0)
		return M0_RC(0);

	M0_ALLOC_ARR(*dest, src->af_count + 1);
	if (*dest == NULL)
		return M0_ERR(-ENOMEM);

	for (i = 0; i < src->af_count; ++i) {
		int rc = m0_conf_obj_find(cache, &src->af_elems[i], &obj);
		if (rc != 0) {
			m0_free0(dest);
			return M0_ERR(rc);
		}
		(*dest)[i] = M0_CONF_CAST(obj, m0_conf_pver);
	}
	return M0_RC(0);
}

/*
 * XXX REFACTORME
 * This code resembles that of profile_encode() and m0_bufs_from_strings().
 */
M0_INTERNAL int
conf_pvers_encode(struct m0_fid_arr *dest, const struct m0_conf_pver **src)
{
	uint32_t i;

	M0_SET0(dest);

	if (src == NULL)
		return M0_RC(0);

	while (src[dest->af_count] != NULL)
		++dest->af_count;
	if (dest->af_count == 0)
		return M0_RC(0);

	M0_ALLOC_ARR(dest->af_elems, dest->af_count);
	if (dest->af_elems == NULL)
		return M0_ERR(-ENOMEM);

	for (i = 0; i < dest->af_count; ++i)
		dest->af_elems[i] = src[i]->pv_obj.co_id;
	return M0_RC(0);
}

M0_INTERNAL int conf_obj_lookup_denied(const struct m0_conf_obj *parent,
				       const struct m0_fid *name,
				       struct m0_conf_obj **out)
{
	M0_IMPOSSIBLE("Leaf object");
	return M0_ERR(-EPERM);
}

M0_INTERNAL const struct m0_fid **
conf_obj_downlinks_none(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { NULL };
	return rels;
}

#undef M0_TRACE_SUBSYSTEM

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
