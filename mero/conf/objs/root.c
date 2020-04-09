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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Authors:         Andriy Tkachuk <andriy.tkachuk@seagate.com>
 *
 * Original creation date: 12-Mar-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "lib/string.h"       /* m0_strings_free */
#include "conf/objs/common.h"
#include "conf/onwire_xc.h"   /* m0_confx_root_xc */
#include "mero/magic.h"       /* M0_CONF_ROOT_MAGIC */

#define XCAST(xobj) ((struct m0_confx_root *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_root, xt_header) == 0);

static bool root_check(const void *bob)
{
	const struct m0_conf_root *self = bob;
	const struct m0_conf_obj  *self_obj = &self->rt_obj;

	M0_PRE(m0_conf_obj_type(self_obj) == &M0_CONF_ROOT_TYPE);

	return _0C(m0_fid_eq(&self->rt_obj.co_id, &M0_CONF_ROOT_FID)) &&
	       _0C(self_obj->co_parent == NULL) &&
	       _0C(m0_conf_obj_is_stub(self_obj) || self->rt_verno > 0);
}

M0_CONF__BOB_DEFINE(m0_conf_root, M0_CONF_ROOT_MAGIC, root_check);
M0_CONF__INVARIANT_DEFINE(root_invariant, m0_conf_root);

static int
root_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	int rc;
	const struct m0_confx_root *s = XCAST(src);
	struct m0_conf_root        *d = M0_CONF_CAST(dest, m0_conf_root);

	if (s->xt_verno == 0
	    || !m0_conf_fid_is_valid(&s->xt_mdpool)
	    ||  m0_conf_fid_type(&s->xt_mdpool) != &M0_CONF_POOL_TYPE
	    || (m0_fid_is_set(&s->xt_imeta_pver)
		&& (!m0_conf_fid_is_valid(&s->xt_imeta_pver) ||
		    m0_conf_fid_type(&s->xt_imeta_pver) != &M0_CONF_PVER_TYPE)))
		return M0_ERR(-EINVAL);

	d->rt_verno        = s->xt_verno;
	d->rt_rootfid      = s->xt_rootfid;
	d->rt_mdpool       = s->xt_mdpool;
	d->rt_imeta_pver   = s->xt_imeta_pver;
	d->rt_mdredundancy = s->xt_mdredundancy;

	rc = m0_bufs_to_strings(&d->rt_params, &s->xt_params);
	if (rc != 0)
		return M0_ERR(rc);

	rc = rc ?:
	     dir_create_and_populate(&d->rt_nodes,
			&CONF_DIR_ENTRIES(&M0_CONF_ROOT_NODES_FID,
					  &M0_CONF_NODE_TYPE,
					  &s->xt_nodes), dest) ?:
	     dir_create_and_populate(&d->rt_sites,
			&CONF_DIR_ENTRIES(&M0_CONF_ROOT_SITES_FID,
					  &M0_CONF_SITE_TYPE,
					  &s->xt_sites), dest) ?:
	     dir_create_and_populate(&d->rt_pools,
			&CONF_DIR_ENTRIES(&M0_CONF_ROOT_POOLS_FID,
					  &M0_CONF_POOL_TYPE,
					  &s->xt_pools), dest) ?:
	     dir_create_and_populate(&d->rt_profiles,
			&CONF_DIR_ENTRIES(&M0_CONF_ROOT_PROFILES_FID,
					  &M0_CONF_PROFILE_TYPE,
					  &s->xt_profiles), dest) ?:
	     dir_create_and_populate(&d->rt_fdmi_flt_grps,
			&CONF_DIR_ENTRIES(&M0_CONF_ROOT_FDMI_FLT_GRPS_FID,
					  &M0_CONF_FDMI_FLT_GRP_TYPE,
					  &s->xt_fdmi_flt_grps), dest);
	if (rc != 0) {
		m0_strings_free(d->rt_params);
		d->rt_params = NULL; /* make invariant happy */
	}

	return M0_RC(rc);
}

static int root_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	int rc;
	const struct m0_conf_root *s = M0_CONF_CAST(src, m0_conf_root);
	struct m0_confx_root      *d = XCAST(dest);
	const struct conf_dir_encoding_pair dirs[] = {
		{ s->rt_nodes,         &d->xt_nodes },
		{ s->rt_sites,         &d->xt_sites },
		{ s->rt_pools,         &d->xt_pools },
		{ s->rt_profiles,      &d->xt_profiles },
		{ s->rt_fdmi_flt_grps, &d->xt_fdmi_flt_grps }
	};

	confx_encode(dest, src);

	d->xt_verno        = s->rt_verno;
	d->xt_rootfid      = s->rt_rootfid;
	d->xt_mdpool       = s->rt_mdpool;
	d->xt_imeta_pver   = s->rt_imeta_pver;
	d->xt_mdredundancy = s->rt_mdredundancy;

	rc = m0_bufs_from_strings(&d->xt_params, s->rt_params);
	if (rc != 0)
		return M0_ERR(rc);

	rc = conf_dirs_encode(dirs, ARRAY_SIZE(dirs));
	if (rc != 0)
		m0_bufs_free(&d->xt_params);

	if (rc == 0 && s->rt_fdmi_flt_grps != NULL)
		/**
		 * @todo Make spiel happy for now as it does not know about
		 * fdmi yet.
		 */
		rc = arrfid_from_dir(&d->xt_fdmi_flt_grps, s->rt_fdmi_flt_grps);
	return M0_RC(rc);
}

static bool
root_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_root *xobj = XCAST(flat);
	const struct m0_conf_root  *obj = M0_CONF_CAST(cached, m0_conf_root);

	return obj->rt_verno == xobj->xt_verno &&
	       m0_fid_eq(&obj->rt_rootfid, &xobj->xt_rootfid) &&
	       m0_fid_eq(&obj->rt_mdpool, &xobj->xt_mdpool) &&
	       m0_fid_eq(&obj->rt_imeta_pver, &xobj->xt_imeta_pver) &&
	       obj->rt_mdredundancy == xobj->xt_mdredundancy &&
	       m0_bufs_streq(&xobj->xt_params, obj->rt_params) &&
	       m0_conf_dir_elems_match(obj->rt_nodes, &xobj->xt_nodes) &&
	       m0_conf_dir_elems_match(obj->rt_sites, &xobj->xt_sites) &&
	       m0_conf_dir_elems_match(obj->rt_pools, &xobj->xt_pools) &&
	       m0_conf_dir_elems_match(obj->rt_profiles, &xobj->xt_profiles) &&
	       m0_conf_dir_elems_match(obj->rt_fdmi_flt_grps,
				       &xobj->xt_fdmi_flt_grps);
}

static int root_lookup(const struct m0_conf_obj *parent,
		       const struct m0_fid *name,
		       struct m0_conf_obj **out)
{
	struct m0_conf_root *root = M0_CONF_CAST(parent, m0_conf_root);
	const struct conf_dir_relation dirs[] = {
		{ root->rt_nodes,         &M0_CONF_ROOT_NODES_FID },
		{ root->rt_sites,         &M0_CONF_ROOT_SITES_FID },
		{ root->rt_pools,         &M0_CONF_ROOT_POOLS_FID },
		{ root->rt_profiles,      &M0_CONF_ROOT_PROFILES_FID },
		{ root->rt_fdmi_flt_grps, &M0_CONF_ROOT_FDMI_FLT_GRPS_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **root_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = {
		&M0_CONF_ROOT_NODES_FID,
		&M0_CONF_ROOT_SITES_FID,
		&M0_CONF_ROOT_POOLS_FID,
		&M0_CONF_ROOT_PROFILES_FID,
		&M0_CONF_ROOT_FDMI_FLT_GRPS_FID,
		NULL
	};

	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_ROOT_TYPE);
	return rels;
}

static void root_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_root *root = M0_CONF_CAST(obj, m0_conf_root);

	m0_strings_free(root->rt_params);
	m0_conf_root_bob_fini(root);
	m0_free(root);
}

static const struct m0_conf_obj_ops root_ops = {
	.coo_invariant = root_invariant,
	.coo_decode    = root_decode,
	.coo_encode    = root_encode,
	.coo_match     = root_match,
	.coo_lookup    = root_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = root_downlinks,
	.coo_delete    = root_delete
};

M0_CONF__CTOR_DEFINE(root_create, m0_conf_root, &root_ops);

const struct m0_conf_obj_type M0_CONF_ROOT_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__ROOT_FT_ID,
		.ft_name = "conf_root"
	},
	.cot_create  = &root_create,
	.cot_xt      = &m0_confx_root_xc,
	.cot_branch  = "u_root",
	.cot_xc_init = &m0_xc_m0_confx_root_struct_init,
	.cot_magic   = M0_CONF_ROOT_MAGIC
};

const struct m0_fid M0_CONF_ROOT_FID = M0_FID_TINIT(M0_CONF__ROOT_FT_ID, 1, 0);

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
