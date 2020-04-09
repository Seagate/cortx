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
#include "conf/onwire_xc.h"  /* m0_confx_enclosure_xc */
#include "mero/magic.h"      /* M0_CONF_ENCLOSURE_MAGIC */

#define XCAST(xobj) ((struct m0_confx_enclosure *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_enclosure, xe_header) == 0);

static bool enclosure_check(const void *bob)
{
	const struct m0_conf_enclosure *self = bob;

	M0_PRE(m0_conf_obj_type(&self->ce_obj) == &M0_CONF_ENCLOSURE_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_enclosure, M0_CONF_ENCLOSURE_MAGIC,
		    enclosure_check);
M0_CONF__INVARIANT_DEFINE(enclosure_invariant, m0_conf_enclosure);

static int
enclosure_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	struct m0_conf_enclosure *d = M0_CONF_CAST(dest, m0_conf_enclosure);
	const struct m0_confx_enclosure *s = XCAST(src);

	return M0_RC(dir_create_and_populate(
			     &d->ce_ctrls,
			     &CONF_DIR_ENTRIES(&M0_CONF_ENCLOSURE_CTRLS_FID,
					       &M0_CONF_CONTROLLER_TYPE,
					       &s->xe_ctrls), dest) ?:
                     conf_pvers_decode(&d->ce_pvers, &s->xe_pvers,
				       dest->co_cache));
}

static int
enclosure_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_enclosure  *s = M0_CONF_CAST(src, m0_conf_enclosure);
	struct m0_confx_enclosure *d = XCAST(dest);
	const struct conf_dir_encoding_pair dirs[] = {
		{ s->ce_ctrls, &d->xe_ctrls }
	};

	confx_encode(dest, src);
	return M0_RC(conf_dirs_encode(dirs, ARRAY_SIZE(dirs)) ?:
		     conf_pvers_encode(
			     &d->xe_pvers,
			     (const struct m0_conf_pver**)s->ce_pvers));
}

static bool enclosure_match(const struct m0_conf_obj  *cached,
			    const struct m0_confx_obj *flat)
{
	const struct m0_confx_enclosure *xobj = XCAST(flat);
	const struct m0_conf_enclosure  *obj  =
		M0_CONF_CAST(cached, m0_conf_enclosure);

	return m0_conf_dir_elems_match(obj->ce_ctrls, &xobj->xe_ctrls);
}

static int enclosure_lookup(const struct m0_conf_obj *parent,
			    const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_enclosure *e = M0_CONF_CAST(parent, m0_conf_enclosure);
	const struct conf_dir_relation dirs[] = {
		{ e->ce_ctrls, &M0_CONF_ENCLOSURE_CTRLS_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **enclosure_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_ENCLOSURE_CTRLS_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_ENCLOSURE_TYPE);
	return rels;
}

static void enclosure_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_enclosure *x = M0_CONF_CAST(obj, m0_conf_enclosure);
	m0_conf_enclosure_bob_fini(x);
	m0_free(x->ce_pvers);
	m0_free(x);
}

static const struct m0_conf_obj_ops enclosure_ops = {
	.coo_invariant = enclosure_invariant,
	.coo_decode    = enclosure_decode,
	.coo_encode    = enclosure_encode,
	.coo_match     = enclosure_match,
	.coo_lookup    = enclosure_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = enclosure_downlinks,
	.coo_delete    = enclosure_delete,
};

M0_CONF__CTOR_DEFINE(enclosure_create, m0_conf_enclosure, &enclosure_ops);

const struct m0_conf_obj_type M0_CONF_ENCLOSURE_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__ENCLOSURE_FT_ID,
		.ft_name = "conf_enclosure"
	},
	.cot_create  = &enclosure_create,
	.cot_xt      = &m0_confx_enclosure_xc,
	.cot_branch  = "u_enclosure",
	.cot_xc_init = &m0_xc_m0_confx_enclosure_struct_init,
	.cot_magic   = M0_CONF_ENCLOSURE_MAGIC
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
