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
 * Original creation date: 12-Dec-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h"  /* m0_confx_drive_xc */
#include "mero/magic.h"      /* M0_CONF_DRIVE_MAGIC */

#define XCAST(xobj) ((struct m0_confx_drive *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_drive, xk_header) == 0);

static bool drive_check(const void *bob)
{
	const struct m0_conf_drive *self = bob;

	M0_PRE(m0_conf_obj_type(&self->ck_obj) == &M0_CONF_DRIVE_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_drive, M0_CONF_DRIVE_MAGIC, drive_check);
M0_CONF__INVARIANT_DEFINE(drive_invariant, m0_conf_drive);

static int
drive_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	struct m0_conf_drive  *d = M0_CONF_CAST(dest, m0_conf_drive);
	struct m0_confx_drive *s = XCAST(src);
	struct m0_conf_obj    *child;
	int                    rc;

	rc = m0_conf_obj_find(dest->co_cache, &s->xk_sdev, &child);
	if (rc == 0) {
		d->ck_sdev = M0_CONF_CAST(child, m0_conf_sdev);
		/* back pointer to drive objects */
		d->ck_sdev->sd_drive = dest->co_id;
	}
	return M0_RC(conf_pvers_decode(&d->ck_pvers, &s->xk_pvers,
				       dest->co_cache));

}

static int
drive_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_drive  *s = M0_CONF_CAST(src, m0_conf_drive);
	struct m0_confx_drive *d = XCAST(dest);

	confx_encode(dest, src);
	if (s->ck_sdev != NULL)
		XCAST(dest)->xk_sdev = s->ck_sdev->sd_obj.co_id;
	return M0_RC(conf_pvers_encode(&d->xk_pvers,
			  (const struct m0_conf_pver**)s->ck_pvers));
}

static bool
drive_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_drive *xobj = XCAST(flat);
	const struct m0_conf_sdev  *child =
		M0_CONF_CAST(cached, m0_conf_drive)->ck_sdev;

	return m0_fid_eq(&child->sd_obj.co_id, &xobj->xk_sdev);
}

static void drive_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_drive *x = M0_CONF_CAST(obj, m0_conf_drive);

	m0_conf_drive_bob_fini(x);
	m0_free(x->ck_pvers);
	m0_free(x);
}

static const struct m0_conf_obj_ops drive_ops = {
	.coo_invariant = drive_invariant,
	.coo_decode    = drive_decode,
	.coo_encode    = drive_encode,
	.coo_match     = drive_match,
	.coo_lookup    = conf_obj_lookup_denied,
	.coo_readdir   = NULL,
	.coo_downlinks = conf_obj_downlinks_none,
	.coo_delete    = drive_delete
};

M0_CONF__CTOR_DEFINE(drive_create, m0_conf_drive, &drive_ops);

const struct m0_conf_obj_type M0_CONF_DRIVE_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__DRIVE_FT_ID,
		.ft_name = "conf_drive"
	},
	.cot_create  = &drive_create,
	.cot_xt      = &m0_confx_drive_xc,
	.cot_branch  = "u_drive",
	.cot_xc_init = &m0_xc_m0_confx_drive_struct_init,
	.cot_magic   = M0_CONF_DRIVE_MAGIC
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
