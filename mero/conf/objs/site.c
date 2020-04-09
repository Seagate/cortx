/*
 * COPYRIGHT 2018 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 24-Jan-2018
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h"  /* m0_confx_site_xc */
#include "mero/magic.h"      /* M0_CONF_SITE_MAGIC */

#define XCAST(xobj) ((struct m0_confx_site *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_site, xi_header) == 0);

static bool site_check(const void *bob)
{
	const struct m0_conf_site *self = bob;

	M0_PRE(m0_conf_obj_type(&self->ct_obj) == &M0_CONF_SITE_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_site, M0_CONF_SITE_MAGIC, site_check);
M0_CONF__INVARIANT_DEFINE(site_invariant, m0_conf_site);

static int site_decode(struct m0_conf_obj        *dest,
		       const struct m0_confx_obj *src)
{
	struct m0_conf_site        *d = M0_CONF_CAST(dest, m0_conf_site);
	const struct m0_confx_site *s = XCAST(src);

	return M0_RC(dir_create_and_populate(&d->ct_racks,
			     &CONF_DIR_ENTRIES(&M0_CONF_SITE_RACKS_FID,
					       &M0_CONF_RACK_TYPE,
					       &s->xi_racks), dest) ?:
		     conf_pvers_decode(&d->ct_pvers, &s->xi_pvers,
				       dest->co_cache));
}

static int
site_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_site  *s = M0_CONF_CAST(src, m0_conf_site);
	struct m0_confx_site *d = XCAST(dest);
	const struct conf_dir_encoding_pair dirs[] = {
		{ s->ct_racks, &d->xi_racks }
	};

	confx_encode(dest, src);
	return M0_RC(conf_dirs_encode(dirs, ARRAY_SIZE(dirs)) ?:
		     conf_pvers_encode(&d->xi_pvers,
			     (const struct m0_conf_pver**)s->ct_pvers));
}

static bool
site_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_site *xobj = XCAST(flat);
	const struct m0_conf_site  *obj  = M0_CONF_CAST(cached, m0_conf_site);

	return m0_conf_dir_elems_match(obj->ct_racks, &xobj->xi_racks);
}

static int site_lookup(const struct m0_conf_obj *parent,
		       const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_site *r = M0_CONF_CAST(parent, m0_conf_site);
	const struct conf_dir_relation dirs[] = {
		{ r->ct_racks, &M0_CONF_SITE_RACKS_FID } };

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **site_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_SITE_RACKS_FID, NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_SITE_TYPE);
	return rels;
}

static void site_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_site *x = M0_CONF_CAST(obj, m0_conf_site);
	m0_conf_site_bob_fini(x);
	m0_free(x->ct_pvers);
	m0_free(x);
}

static const struct m0_conf_obj_ops site_ops = {
	.coo_invariant = site_invariant,
	.coo_decode    = site_decode,
	.coo_encode    = site_encode,
	.coo_match     = site_match,
	.coo_lookup    = site_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = site_downlinks,
	.coo_delete    = site_delete
};

M0_CONF__CTOR_DEFINE(site_create, m0_conf_site, &site_ops);

const struct m0_conf_obj_type M0_CONF_SITE_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__SITE_FT_ID,
		.ft_name = "conf_site"
	},
	.cot_create  = &site_create,
	.cot_xt      = &m0_confx_site_xc,
	.cot_branch  = "u_site",
	.cot_xc_init = &m0_xc_m0_confx_site_struct_init,
	.cot_magic   = M0_CONF_SITE_MAGIC
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
