/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 *                  Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 24-Nov-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h"  /* m0_confx_pver_xc */
#include "conf/pvers.h"      /* M0_CONF_PVER_FORMULAIC */
#include "mero/magic.h"      /* M0_CONF_PVER_MAGIC */
#include "layout/pdclust.h"  /* m0_pdclust_attr_check */

#define XCAST(xobj) ((struct m0_confx_pver *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_pver, xv_header) == 0);

static bool pver_check(const void *bob)
{
	const struct m0_conf_pver *self = bob;
	const struct m0_conf_obj  *self_obj = &self->pv_obj;
	enum m0_conf_pver_kind     kind;

	return m0_conf_obj_is_stub(self_obj) ||
		(_0C(m0_conf_pver_fid_read(
			     &self_obj->co_id, &kind, NULL, NULL) == 0) &&
		 _0C(kind == self->pv_kind) &&
		 ergo(kind == M0_CONF_PVER_ACTUAL,
		      _0C(m0_pdclust_attr_check(
				  &self->pv_u.subtree.pvs_attr))) &&
		 ergo(kind == M0_CONF_PVER_FORMULAIC,
		      /* at least one of the numbers != 0 */
		      _0C(!M0_IS0(&self->pv_u.formulaic.pvf_allowance))) &&
		 ergo(kind == M0_CONF_PVER_VIRTUAL,
		      self_obj->co_parent == NULL));
}

M0_CONF__BOB_DEFINE(m0_conf_pver, M0_CONF_PVER_MAGIC, pver_check);
M0_CONF__INVARIANT_DEFINE(pver_invariant, m0_conf_pver);

static int
pver_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	struct m0_conf_pver          *da = M0_CONF_CAST(dest, m0_conf_pver);
	const struct m0_confx_pver_u *sa = &XCAST(src)->xv_u;
	int                           rc;

	M0_ENTRY("dest="FID_F, FID_P(&dest->co_id));

	rc = m0_conf_pver_fid_read(&src->xo_u.u_header.ch_id, &da->pv_kind,
				   NULL, NULL);
	if (rc != 0)
		return M0_ERR(rc);
	if (sa->xpv_is_formulaic != (da->pv_kind == M0_CONF_PVER_FORMULAIC))
		return M0_ERR(-EINVAL);

	switch (da->pv_kind) {
	case M0_CONF_PVER_ACTUAL: {
		struct m0_conf_pver_subtree       *d = &da->pv_u.subtree;
		const struct m0_confx_pver_actual *s = &sa->u.xpv_actual;

		*d = (struct m0_conf_pver_subtree){
			.pvs_attr = (struct m0_pdclust_attr){
				.pa_N = s->xva_N,
				.pa_K = s->xva_K,
				.pa_P = s->xva_P
			}
		};
		if (s->xva_tolerance.au_count != ARRAY_SIZE(d->pvs_tolerance))
			return M0_ERR(-EINVAL);
		memcpy(d->pvs_tolerance, s->xva_tolerance.au_elems,
		       sizeof(d->pvs_tolerance));
		return M0_RC(m0_conf_dir_new(dest, &M0_CONF_PVER_SITEVS_FID,
					     &M0_CONF_OBJV_TYPE, &s->xva_sitevs,
					     &d->pvs_sitevs));
	}
	case M0_CONF_PVER_FORMULAIC: {
		struct m0_conf_pver_formulaic        *d = &da->pv_u.formulaic;
		const struct m0_confx_pver_formulaic *s = &sa->u.xpv_formulaic;

		*d = (struct m0_conf_pver_formulaic){
			.pvf_id   = s->xvf_id,
			.pvf_base = s->xvf_base
		};
		if (s->xvf_allowance.au_count != ARRAY_SIZE(d->pvf_allowance))
			return M0_ERR(-EINVAL);
		memcpy(d->pvf_allowance, s->xvf_allowance.au_elems,
		       sizeof(d->pvf_allowance));
		return M0_RC(0);
	}
	default:
		return M0_ERR(-EINVAL);
	}
}

static int
pver_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	const struct m0_conf_pver *sa = M0_CONF_CAST(src, m0_conf_pver);
	struct m0_confx_pver_u    *da = &XCAST(dest)->xv_u;
	int                        rc;

	confx_encode(dest, src);
	da->xpv_is_formulaic = sa->pv_kind;
	switch (sa->pv_kind) {
	case M0_CONF_PVER_ACTUAL: {
		const struct m0_conf_pver_subtree *s = &sa->pv_u.subtree;
		struct m0_confx_pver_actual       *d = &da->u.xpv_actual;

		*d = (struct m0_confx_pver_actual) {
			.xva_N = s->pvs_attr.pa_N,
			.xva_K = s->pvs_attr.pa_K,
			.xva_P = s->pvs_attr.pa_P
		};
		rc = u32arr_encode(&d->xva_tolerance, s->pvs_tolerance,
				   ARRAY_SIZE(s->pvs_tolerance));
		if (rc != 0)
			return M0_ERR(rc);

		rc = arrfid_from_dir(&d->xva_sitevs, s->pvs_sitevs);
		if (rc != 0)
			u32arr_free(&d->xva_tolerance);
		return M0_RC(rc);
	}
	case M0_CONF_PVER_FORMULAIC: {
		const struct m0_conf_pver_formulaic *s = &sa->pv_u.formulaic;
		struct m0_confx_pver_formulaic      *d = &da->u.xpv_formulaic;

		*d = (struct m0_confx_pver_formulaic) {
			.xvf_id   = s->pvf_id,
			.xvf_base = s->pvf_base
		};
		return M0_RC(u32arr_encode(&d->xvf_allowance, s->pvf_allowance,
					   ARRAY_SIZE(s->pvf_allowance)));
	}
	default:
		M0_IMPOSSIBLE("");
	}
}

static bool
pver_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_pver_u *xobj = &XCAST(flat)->xv_u;
	const struct m0_conf_pver    *obj  = M0_CONF_CAST(cached, m0_conf_pver);
	/*
	 * This check will fail if the "kind" bits of the confx_obj's fid
	 * do not correspond to those of actual or formulaic pver.
	 */
	M0_PRE(M0_IN(obj->pv_kind,
		     (M0_CONF_PVER_ACTUAL, M0_CONF_PVER_FORMULAIC)));
	return
		xobj->xpv_is_formulaic ==
		(obj->pv_kind == M0_CONF_PVER_FORMULAIC) &&
		(xobj->xpv_is_formulaic ? ({
			const struct m0_confx_pver_formulaic *x =
				&xobj->u.xpv_formulaic;
			const struct m0_conf_pver_formulaic  *c =
				&obj->pv_u.formulaic;

			c->pvf_id == x->xvf_id &&
			m0_fid_eq(&c->pvf_base, &x->xvf_base) &&
			u32arr_cmp(&x->xvf_allowance, c->pvf_allowance,
				   ARRAY_SIZE(c->pvf_allowance));
		 }) : ({
			const struct m0_confx_pver_actual *x =
				&xobj->u.xpv_actual;
			const struct m0_conf_pver_subtree *c =
				&obj->pv_u.subtree;

			c->pvs_attr.pa_N == x->xva_N &&
			c->pvs_attr.pa_K == x->xva_K &&
			c->pvs_attr.pa_P == x->xva_P &&
			u32arr_cmp(&x->xva_tolerance, c->pvs_tolerance,
				   ARRAY_SIZE(c->pvs_tolerance)) &&
			m0_conf_dir_elems_match(c->pvs_sitevs, &x->xva_sitevs);
		 }));
}

static int pver_lookup(const struct m0_conf_obj *parent,
		       const struct m0_fid *name, struct m0_conf_obj **out)
{
	const struct m0_conf_pver *pver = M0_CONF_CAST(parent, m0_conf_pver);

	M0_PRE(parent->co_status == M0_CS_READY);

	if (!m0_fid_eq(name, &M0_CONF_PVER_SITEVS_FID))
		return M0_ERR(-ENOENT);

	if (pver->pv_kind == M0_CONF_PVER_FORMULAIC) {
		/*
		 * XXX FIXME: ->coo_lookup() must not be called for
		 * formulaic pvers.
		 */
		struct m0_conf_obj *obj;
		int                 rc;

		rc = m0_conf_obj_find(parent->co_cache,
				      &pver->pv_u.formulaic.pvf_base, &obj) ?:
			obj->co_ops->coo_lookup(obj, name, out);
		if (rc != 0)
			return M0_ERR(rc);
	} else
		*out = &pver->pv_u.subtree.pvs_sitevs->cd_obj;
	M0_POST(m0_conf_obj_invariant(*out));
	return M0_RC(0);
}

static const struct m0_fid **pver_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_PVER_SITEVS_FID,
					       NULL };
	const struct m0_conf_pver  *pver = M0_CONF_CAST(obj, m0_conf_pver);

	return pver->pv_kind == M0_CONF_PVER_FORMULAIC
		? &rels[1] /* formulaic pver has no downlinks */
		: rels;
}

static void pver_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_pver *x = M0_CONF_CAST(obj, m0_conf_pver);

	m0_conf_pver_bob_fini(x);
	m0_free(x);
}

static const struct m0_conf_obj_ops pver_ops = {
	.coo_invariant = pver_invariant,
	.coo_decode    = pver_decode,
	.coo_encode    = pver_encode,
	.coo_match     = pver_match,
	.coo_lookup    = pver_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = pver_downlinks,
	.coo_delete    = pver_delete
};

M0_CONF__CTOR_DEFINE(pver_create, m0_conf_pver, &pver_ops);

const struct m0_conf_obj_type M0_CONF_PVER_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__PVER_FT_ID,
		.ft_name = "conf_pver"
	},
	.cot_create  = &pver_create,
	.cot_xt      = &m0_confx_pver_xc,
	.cot_branch  = "u_pver",
	.cot_xc_init = &m0_xc_m0_confx_pver_struct_init,
	.cot_magic   = M0_CONF_PVER_MAGIC
};

#undef XCAST
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
