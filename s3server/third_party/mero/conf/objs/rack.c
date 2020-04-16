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
 * Original creation date: 24-Nov-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h"  /* m0_confx_rack_xc */
#include "mero/magic.h"      /* M0_CONF_RACK_MAGIC */

#define XCAST(xobj) ((struct m0_confx_rack *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_rack, xr_header) == 0);

static bool rack_check(const void *bob)
{
	const struct m0_conf_rack *self = bob;

	M0_PRE(m0_conf_obj_type(&self->cr_obj) == &M0_CONF_RACK_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_rack, M0_CONF_RACK_MAGIC, rack_check);
M0_CONF__INVARIANT_DEFINE(rack_invariant, m0_conf_rack);

static int
rack_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	struct m0_conf_rack        *d = M0_CONF_CAST(dest, m0_conf_rack);
	const struct m0_confx_rack *s = XCAST(src);

	return M0_RC(dir_create_and_populate(
			     &d->cr_encls,
			     &CONF_DIR_ENTRIES(&M0_CONF_RACK_ENCLS_FID,
					       &M0_CONF_ENCLOSURE_TYPE,
					       &s->xr_encls), dest) ?:
		     conf_pvers_decode(&d->cr_pvers, &s->xr_pvers,
				       dest->co_cache));
}

static int
rack_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_rack  *s = M0_CONF_CAST(src, m0_conf_rack);
	struct m0_confx_rack *d = XCAST(dest);
	const struct conf_dir_encoding_pair dirs[] = {
		{ s->cr_encls, &d->xr_encls }
	};

	confx_encode(dest, src);
	return M0_RC(conf_dirs_encode(dirs, ARRAY_SIZE(dirs)) ?:
		     conf_pvers_encode(
			     &d->xr_pvers,
			     (const struct m0_conf_pver**)s->cr_pvers));
}

static bool
rack_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_rack *xobj = XCAST(flat);
	const struct m0_conf_rack  *obj  = M0_CONF_CAST(cached, m0_conf_rack);

	return m0_conf_dir_elems_match(obj->cr_encls, &xobj->xr_encls);
}

static int rack_lookup(const struct m0_conf_obj *parent,
		       const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_rack *r = M0_CONF_CAST(parent, m0_conf_rack);
	const struct conf_dir_relation dirs[] = {
		{ r->cr_encls, &M0_CONF_RACK_ENCLS_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **rack_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_RACK_ENCLS_FID, NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_RACK_TYPE);
	return rels;
}

static void rack_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_rack *x = M0_CONF_CAST(obj, m0_conf_rack);
	m0_conf_rack_bob_fini(x);
	m0_free(x->cr_pvers);
	m0_free(x);
}

static const struct m0_conf_obj_ops rack_ops = {
	.coo_invariant = rack_invariant,
	.coo_decode    = rack_decode,
	.coo_encode    = rack_encode,
	.coo_match     = rack_match,
	.coo_lookup    = rack_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = rack_downlinks,
	.coo_delete    = rack_delete
};

M0_CONF__CTOR_DEFINE(rack_create, m0_conf_rack, &rack_ops);

const struct m0_conf_obj_type M0_CONF_RACK_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__RACK_FT_ID,
		.ft_name = "conf_rack"
	},
	.cot_create  = &rack_create,
	.cot_xt      = &m0_confx_rack_xc,
	.cot_branch  = "u_rack",
	.cot_xc_init = &m0_xc_m0_confx_rack_struct_init,
	.cot_magic   = M0_CONF_RACK_MAGIC
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
