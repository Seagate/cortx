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
 * Original author: Mandar Sawant <mandar.sawant@seagate.com>
 * Original creation date: 25-Nov-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h"  /* m0_confx_objv_xc */
#include "lib/arith.h"       /* M0_CNT_INC */
#include "mero/magic.h"      /* M0_CONF_OBJV_MAGIC */

#define XCAST(xobj) ((struct m0_confx_objv *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_objv, xj_header) == 0);

static const struct m0_fid **objv_downlinks(const struct m0_conf_obj *obj);

static bool objv_check(const void *bob)
{
	const struct m0_conf_objv *self = bob;

	M0_PRE(m0_conf_obj_type(&self->cv_obj) == &M0_CONF_OBJV_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_objv, M0_CONF_OBJV_MAGIC, objv_check);
M0_CONF__INVARIANT_DEFINE(objv_invariant, m0_conf_objv);

static int objv_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	struct m0_conf_objv        *d = M0_CONF_CAST(dest, m0_conf_objv);
	const struct m0_confx_objv *s = XCAST(src);
	const struct m0_fid        *relfid;
	int                         rc;

	d->cv_ix = -1;
	rc = m0_conf_obj_find(dest->co_cache, &s->xj_real, &d->cv_real);
	if (rc != 0)
		return M0_ERR(rc);
	relfid = objv_downlinks(dest)[0];
	if (relfid == NULL)
		return s->xj_children.af_count == 0 ?
			M0_RC(0) /* no children */ :
			M0_ERR_INFO(-EINVAL, FID_F": No children expected",
				    FID_P(&dest->co_id));
	return M0_RC(m0_conf_dir_new(dest, relfid, &M0_CONF_OBJV_TYPE,
				     &s->xj_children, &d->cv_children));
}

static int
objv_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_objv  *s = M0_CONF_CAST(src, m0_conf_objv);
	struct m0_confx_objv *d = XCAST(dest);

	confx_encode(dest, src);
	if (s->cv_real != NULL)
		XCAST(dest)->xj_real = s->cv_real->co_id;
	return s->cv_children == NULL ? 0 :
		arrfid_from_dir(&d->xj_children, s->cv_children);
}

static bool
objv_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_objv *xobj = XCAST(flat);
	const struct m0_conf_objv  *obj = M0_CONF_CAST(cached, m0_conf_objv);

	M0_PRE(obj->cv_real != NULL);
	return m0_fid_eq(&obj->cv_real->co_id, &xobj->xj_real) &&
	       m0_conf_dir_elems_match(obj->cv_children, &xobj->xj_children);
}

static int objv_lookup(const struct m0_conf_obj *parent,
		       const struct m0_fid *name,
		       struct m0_conf_obj **out)
{
	struct m0_conf_objv *objv = M0_CONF_CAST(parent, m0_conf_objv);
	const struct conf_dir_relation dirs[] = {
		{ objv->cv_children, parent->co_ops->coo_downlinks(parent)[0] }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **objv_downlinks(const struct m0_conf_obj *obj)
{
	enum { SITE, RACK, ENCL, CTRL, DISK };
	static const struct m0_fid *downlinks[][2] = {
		[SITE] = { &M0_CONF_SITEV_RACKVS_FID, NULL },
		[RACK] = { &M0_CONF_RACKV_ENCLVS_FID, NULL },
		[ENCL] = { &M0_CONF_ENCLV_CTRLVS_FID, NULL },
		[CTRL] = { &M0_CONF_CTRLV_DRIVEVS_FID, NULL },
		[DISK] = { NULL, NULL } /* no downlinks */
	};
	const struct m0_conf_obj_type *real =
		m0_conf_obj_type(M0_CONF_CAST(obj, m0_conf_objv)->cv_real);

	if (real == &M0_CONF_SITE_TYPE)
		return downlinks[SITE];
	if (real == &M0_CONF_RACK_TYPE)
		return downlinks[RACK];
	if (real == &M0_CONF_ENCLOSURE_TYPE)
		return downlinks[ENCL];
	if (real == &M0_CONF_CONTROLLER_TYPE)
		return downlinks[CTRL];
	M0_ASSERT(real == &M0_CONF_DRIVE_TYPE);
	return downlinks[DISK];
}

static void objv_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_objv *x = M0_CONF_CAST(obj, m0_conf_objv);

	m0_conf_objv_bob_fini(x);
	m0_free(x);
}

static const struct m0_conf_obj_ops objv_ops = {
	.coo_invariant = objv_invariant,
	.coo_decode    = objv_decode,
	.coo_encode    = objv_encode,
	.coo_match     = objv_match,
	.coo_lookup    = objv_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = objv_downlinks,
	.coo_delete    = objv_delete
};

M0_CONF__CTOR_DEFINE(objv_create, m0_conf_objv, &objv_ops);

const struct m0_conf_obj_type M0_CONF_OBJV_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__OBJV_FT_ID,
		.ft_name = "conf_objv"
	},
	.cot_create  = &objv_create,
	.cot_xt      = &m0_confx_objv_xc,
	.cot_branch  = "u_objv",
	.cot_xc_init = &m0_xc_m0_confx_objv_struct_init,
	.cot_magic   = M0_CONF_OBJV_MAGIC
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
