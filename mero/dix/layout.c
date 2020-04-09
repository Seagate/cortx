/* -*- C -*- */
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 31-May-2016
 */


/**
 * @addtogroup dix
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/buf.h"
#include "lib/hash_fnc.h"
#include "lib/misc.h"        /* M0_BYTES */
#include "lib/memory.h"
#include "lib/ext.h"         /* m0_ext */
#include "layout/layout.h"
#include "layout/pdclust.h"
#include "pool/pool.h"
#include "dix/imask.h"
#include "dix/layout.h"


static int layout_create(struct m0_layout_domain *domain,
			 const struct m0_fid     *fid,
			 uint64_t                 layout_id,
			 struct m0_pool_version  *pver,
			 struct m0_dix_linst     *dli)
{
	int                        rc;
	struct m0_layout          *l;
	struct m0_layout_instance *li;

	l = m0_layout_find(domain, layout_id);
	M0_ASSERT(l != NULL);
	dli->li_pl = M0_AMB(dli->li_pl, l, pl_base.sl_base);
	l->l_pver = pver;
	rc = m0_layout_instance_build(l, fid, &li);
	if (rc != 0)
		return M0_ERR(rc);
	dli->li_pi = m0_layout_instance_to_pdi(li);
	return M0_RC(0);
}

M0_INTERNAL uint32_t m0_dix_devices_nr(struct m0_dix_linst *linst)
{
	struct m0_pool_version *pver;

	pver = m0_pdl_to_layout(linst->li_pl)->l_pver;
	return pver->pv_mach.pm_state->pst_nr_devices;
}

M0_INTERNAL struct m0_pooldev *m0_dix_tgt2sdev(struct m0_dix_linst *linst,
					       uint64_t             tgt)
{
	struct m0_pool_version *pver;

	pver = m0_pdl_to_layout(linst->li_pl)->l_pver;
	M0_ASSERT(tgt < pver->pv_mach.pm_state->pst_nr_devices);
	return &pver->pv_mach.pm_state->pst_devices_array[tgt];
}

M0_INTERNAL int m0_dix_layout_init(struct m0_dix_linst     *dli,
				   struct m0_layout_domain *domain,
				   const struct m0_fid     *fid,
				   uint64_t                 layout_id,
				   struct m0_pool_version  *pver,
				   struct m0_dix_ldesc     *dld)
{
	M0_PRE(dli != NULL);
	M0_PRE(domain != NULL);
	M0_PRE(pver != NULL);

	M0_SET0(dli);
	dli->li_ldescr = dld;
	return layout_create(domain, fid, layout_id, pver, dli);
}

M0_INTERNAL void m0_dix_layout_fini(struct m0_dix_linst *li)
{
	M0_PRE(li != NULL);
	if (li->li_pi != NULL)
		m0_layout_instance_fini(&li->li_pi->pi_base);
	if (li->li_pl != NULL)
		m0_layout_put(m0_pdl_to_layout(li->li_pl));
}

static void dix_hash(struct m0_dix_ldesc *ldesc,
		     struct m0_buf       *buf,
		     uint64_t            *hash)
{
	void        *val = buf->b_addr;
	m0_bcount_t  len = buf->b_nob;

	M0_PRE(ldesc != NULL);
	M0_PRE(buf != NULL);
	M0_PRE(buf->b_addr != NULL);

	len = M0_BYTES(len);
	switch(ldesc->ld_hash_fnc) {
		case HASH_FNC_NONE:
			M0_PRE(len <= sizeof(uint64_t));
			*hash = val == NULL ? 0 : *(uint64_t *)val;
			break;
		case HASH_FNC_FNV1:
			*hash = m0_hash_fnc_fnv1(val, len);
			break;
		case HASH_FNC_CITY:
			*hash = m0_hash_fnc_city(val, len);
			break;
		default:
			M0_IMPOSSIBLE("Incorrect hash function type");
	}
}

static bool unit_is_valid(struct m0_pdclust_attr *attr, uint64_t unit)
{
	return unit < attr->pa_N + 2 * attr->pa_K;
}

M0_INTERNAL void m0_dix_target(struct m0_dix_linst *inst,
			       uint64_t             unit,
			       struct m0_buf       *key,
			       uint64_t            *out_id)
{
	struct m0_pdclust_src_addr src;
	struct m0_pdclust_tgt_addr tgt;

	M0_PRE(inst != NULL);
	M0_PRE(inst->li_pl != NULL);
	M0_PRE(key != NULL);
	M0_PRE(unit_is_valid(&inst->li_pl->pl_attr, unit));
	if (key->b_addr != NULL)
		dix_hash(inst->li_ldescr, key, &src.sa_group);
	else
		src.sa_group = 0;
	src.sa_unit = unit;
	m0_pdclust_instance_map(inst->li_pi, &src, &tgt);
	*out_id = tgt.ta_obj;
	M0_LOG(M0_DEBUG,
	       "Key %p, group/unit [%"PRIx64",%"PRIx64"] -> target %"PRIx64,
			key, src.sa_group, unit, *out_id);
}

M0_INTERNAL int m0_dix_ldesc_init(struct m0_dix_ldesc       *ld,
				  struct m0_ext             *range,
				  m0_bcount_t                range_nr,
				  enum m0_dix_hash_fnc_type  htype,
				  struct m0_fid             *pver)
{
	int rc;

	M0_PRE(ld != NULL);
	M0_SET0(ld);
	rc = m0_dix_imask_init(&ld->ld_imask, range, range_nr);
	if (rc == 0) {
		ld->ld_hash_fnc = htype;
		ld->ld_pver     = *pver;
	}
	return rc;
}

M0_INTERNAL int m0_dix_ldesc_copy(struct m0_dix_ldesc       *dst,
				  const struct m0_dix_ldesc *src)
{
	dst->ld_hash_fnc = src->ld_hash_fnc;
	dst->ld_pver     = src->ld_pver;
	return m0_dix_imask_copy(&dst->ld_imask, &src->ld_imask);
}

M0_INTERNAL void m0_dix_ldesc_fini(struct m0_dix_ldesc *ld)
{
	m0_dix_imask_fini(&ld->ld_imask);
}

M0_INTERNAL
int m0_dix_layout_iter_init(struct m0_dix_layout_iter *iter,
			    const struct m0_fid       *index,
			    struct m0_layout_domain   *ldom,
			    struct m0_pool_version    *pver,
			    struct m0_dix_ldesc       *ldesc,
			    struct m0_buf             *key)
{
	struct m0_fid           layout_fid;
	uint64_t                layout_id;
	struct m0_pdclust_attr *pd_attr;
	int                     rc;

	M0_ENTRY("iter %p", iter);
	M0_PRE(iter  != NULL);
	M0_PRE(index != NULL);
	M0_PRE(ldom  != NULL);
	M0_PRE(pver  != NULL);
	M0_PRE(ldesc != NULL);

	M0_ASSERT(pver->pv_attr.pa_N == 1);

	layout_fid = *index;
	layout_id = m0_pool_version2layout_id(&pver->pv_id,
					      M0_DEFAULT_LAYOUT_ID);
	rc = m0_dix_layout_init(&iter->dit_linst, ldom, &layout_fid,
				layout_id, pver, ldesc);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_dix_imask_apply(key->b_addr, key->b_nob,
				&ldesc->ld_imask,
				&iter->dit_key.b_addr,
				&iter->dit_key.b_nob);
	if (rc != 0) {
		m0_dix_layout_fini(&iter->dit_linst);
		return M0_ERR(rc);
	}

	pd_attr = &iter->dit_linst.li_pl->pl_attr;
	M0_ASSERT_INFO(pd_attr->pa_N == 1,
		       "N=%u. Only layouts with N=1 are supported.",
		       pver->pv_attr.pa_N);
	iter->dit_W = pd_attr->pa_N + 2 * pd_attr->pa_K;
	iter->dit_unit = 0;
	return M0_RC(rc);
}

M0_INTERNAL void m0_dix_layout_iter_next(struct m0_dix_layout_iter *iter,
					 uint64_t                  *tgt)
{
	M0_ENTRY("iter %p", iter);
	M0_PRE(iter->dit_unit < iter->dit_W);
	m0_dix_layout_iter_get_at(iter, iter->dit_unit, tgt);
	iter->dit_unit++;
}

M0_INTERNAL void m0_dix_layout_iter_get_at(struct m0_dix_layout_iter *iter,
					   uint64_t                   unit,
					   uint64_t                  *tgt)
{
	m0_dix_target(&iter->dit_linst, unit, &iter->dit_key, tgt);
}

M0_INTERNAL uint32_t m0_dix_liter_W(struct m0_dix_layout_iter *iter)
{
	return iter->dit_W;
}

M0_INTERNAL uint32_t m0_dix_liter_N(struct m0_dix_layout_iter *iter)
{
	return iter->dit_linst.li_pl->pl_attr.pa_N;
}

M0_INTERNAL uint32_t m0_dix_liter_P(struct m0_dix_layout_iter *iter)
{
	return iter->dit_linst.li_pl->pl_attr.pa_P;
}

M0_INTERNAL uint32_t m0_dix_liter_K(struct m0_dix_layout_iter *iter)
{
	return iter->dit_linst.li_pl->pl_attr.pa_K;
}

M0_INTERNAL uint32_t m0_dix_liter_spare_offset(struct m0_dix_layout_iter *iter)
{
	return m0_dix_liter_N(iter) + m0_dix_liter_K(iter);
}

M0_INTERNAL uint32_t m0_dix_liter_unit_classify(struct m0_dix_layout_iter *iter,
						uint64_t                   unit)
{
	return m0_pdclust_unit_classify(iter->dit_linst.li_pl, unit);
}


M0_INTERNAL void m0_dix_layout_iter_goto(struct m0_dix_layout_iter *iter,
					 uint64_t                   unit)
{
	M0_ENTRY("iter %p", iter);
	M0_PRE(unit_is_valid(&iter->dit_linst.li_pl->pl_attr, unit));
	iter->dit_unit = unit;
}

M0_INTERNAL void m0_dix_layout_iter_reset(struct m0_dix_layout_iter *iter)
{
	m0_dix_layout_iter_goto(iter, 0);
}

M0_INTERNAL void m0_dix_layout_iter_fini(struct m0_dix_layout_iter *iter)
{
	M0_ENTRY("iter %p", iter);
	m0_buf_free(&iter->dit_key);
	m0_dix_layout_fini(&iter->dit_linst);
}

M0_INTERNAL bool m0_dix_layout_eq(const struct m0_dix_layout *layout1,
				  const struct m0_dix_layout *layout2)
{
	const struct m0_dix_ldesc *ldesc1;
	const struct m0_dix_ldesc *ldesc2;

	M0_PRE(layout1 != NULL);
	M0_PRE(layout2 != NULL);

	if (layout1->dl_type != layout2->dl_type)
		return false;

	if (layout1->dl_type == DIX_LTYPE_ID)
		return layout1->u.dl_id == layout2->u.dl_id;

	/* Compare layout descriptors. */
	ldesc1 = &layout1->u.dl_desc;
	ldesc2 = &layout2->u.dl_desc;

	return ldesc1->ld_hash_fnc == ldesc2->ld_hash_fnc &&
		m0_fid_eq(&ldesc1->ld_pver, &ldesc2->ld_pver) &&
		m0_dix_imask_eq(&ldesc1->ld_imask, &ldesc2->ld_imask);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of DIX group */

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
