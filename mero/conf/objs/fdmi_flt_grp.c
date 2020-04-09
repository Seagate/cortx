/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h" /* m0_confx_fdmi_filter_xc */
#include "mero/magic.h"     /* M0_CONF_FDMI_FILTER_MAGIC */

static bool fdmi_flt_grp_check(const void *bob)
{
	const struct m0_conf_fdmi_flt_grp *self = bob;
	const struct m0_conf_obj          *self_obj = &self->ffg_obj;

	M0_PRE(m0_conf_obj_type(self_obj) == &M0_CONF_FDMI_FLT_GRP_TYPE);
	return true; /* XXX IMPLEMENTME */
}

M0_CONF__BOB_DEFINE(m0_conf_fdmi_flt_grp, M0_CONF_FDMI_FLT_GRP_MAGIC,
		    fdmi_flt_grp_check);

M0_CONF__INVARIANT_DEFINE(fdmi_flt_grp_invariant, m0_conf_fdmi_flt_grp);

#define XCAST(xobj) ((struct m0_confx_fdmi_flt_grp *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_fdmi_flt_grp, xfg_header) == 0);

static int
fdmi_flt_grp_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	int                                 rc;
	const struct m0_confx_fdmi_flt_grp *s = XCAST(src);
	struct m0_conf_fdmi_flt_grp        *d = M0_CONF_CAST(
		dest, m0_conf_fdmi_flt_grp);

	M0_ENTRY();
	d->ffg_rec_type = s->xfg_rec_type;
	rc = m0_conf_dir_new(dest, &M0_CONF_FDMI_FGRP_FILTERS_FID,
			     &M0_CONF_FDMI_FILTER_TYPE, &s->xfg_filters,
			     &d->ffg_filters);
	if (rc == 0)
		m0_conf_child_adopt(dest, &d->ffg_filters->cd_obj);
	return M0_RC(rc);
}

static int
fdmi_flt_grp_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	int                           rc;
	struct m0_confx_fdmi_flt_grp *d = XCAST(dest);
	struct m0_conf_fdmi_flt_grp  *s = M0_CONF_CAST(
		src, m0_conf_fdmi_flt_grp);

	M0_ENTRY();
	confx_encode(dest, src);
	rc = arrfid_from_dir(&d->xfg_filters, s->ffg_filters);
	if (rc == 0)
		d->xfg_rec_type = s->ffg_rec_type;
	return M0_RC(rc);
}

static bool fdmi_flt_grp_match(const struct m0_conf_obj *cached,
			       const struct m0_confx_obj *flat)
{
	const struct m0_confx_fdmi_flt_grp *xobj = XCAST(flat);
	const struct m0_conf_fdmi_flt_grp  *obj = M0_CONF_CAST(
		cached, m0_conf_fdmi_flt_grp);

	M0_ENTRY();
	M0_PRE(xobj->xfg_filters.af_count != 0);

	/* XXX TODO: Compare ffg_filters dir */
	return M0_RC(xobj->xfg_rec_type == obj->ffg_rec_type);
}

static int fdmi_flt_grp_lookup(const struct m0_conf_obj *parent,
			       const struct m0_fid *name,
			       struct m0_conf_obj **out)
{
	M0_ENTRY();
	M0_PRE(parent->co_status == M0_CS_READY);

	if (!m0_fid_eq(name, &M0_CONF_FDMI_FGRP_FILTERS_FID))
		return M0_ERR(-ENOENT);

	*out = &M0_CONF_CAST(parent, m0_conf_fdmi_flt_grp)->ffg_filters->cd_obj;
	M0_POST(m0_conf_obj_invariant(*out));
	return M0_RC(0);
}

static void fdmi_flt_grp_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_fdmi_flt_grp *x = M0_CONF_CAST(obj,
						      m0_conf_fdmi_flt_grp);
	m0_conf_fdmi_flt_grp_bob_fini(x);
	m0_free(x);
}

static const struct m0_fid **fdmi_flt_grp_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_FDMI_FGRP_FILTERS_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_FDMI_FLT_GRP_TYPE);
	return rels;
}

static const struct m0_conf_obj_ops conf_fdmi_flt_grp_ops = {
	.coo_downlinks = fdmi_flt_grp_downlinks,
	.coo_invariant = fdmi_flt_grp_invariant,
	.coo_decode    = fdmi_flt_grp_decode,
	.coo_encode    = fdmi_flt_grp_encode,
	.coo_match     = fdmi_flt_grp_match,
	.coo_lookup    = fdmi_flt_grp_lookup,
	.coo_readdir   = NULL,
	.coo_delete    = fdmi_flt_grp_delete
};

static struct m0_conf_obj *fdmi_flt_grp_create(void)
{
	struct m0_conf_fdmi_flt_grp *x;
	struct m0_conf_obj          *ret;

	M0_ALLOC_PTR(x);
	if (x == NULL)
		return NULL;
	m0_conf_fdmi_flt_grp_bob_init(x);
	ret = &x->ffg_obj;
	ret->co_ops = &conf_fdmi_flt_grp_ops;
	return ret;
}

const struct m0_conf_obj_type M0_CONF_FDMI_FLT_GRP_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__FDMI_FLT_GRP_FT_ID,
		.ft_name = "conf_fdmi_flt_grp"
	},
	.cot_create  = &fdmi_flt_grp_create,
	.cot_xt      = &m0_confx_fdmi_flt_grp_xc,
	.cot_branch  = "u_fdmi_flt_grp",
	.cot_xc_init = &m0_xc_m0_confx_fdmi_flt_grp_struct_init,
	.cot_magic   = M0_CONF_FDMI_FLT_GRP_MAGIC
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
