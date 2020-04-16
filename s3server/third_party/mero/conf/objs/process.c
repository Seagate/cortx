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
#include "conf/onwire_xc.h"  /* m0_confx_process_xc */
#include "mero/magic.h"      /* M0_CONF_PROCESS_MAGIC */

#define XCAST(xobj) ((struct m0_confx_process *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_process, xr_header) == 0);

static bool process_check(const void *bob)
{
	const struct m0_conf_process *self = bob;
	const struct m0_conf_obj     *self_obj = &self->pc_obj;

	M0_PRE(m0_conf_obj_type(self_obj) == &M0_CONF_PROCESS_TYPE);

	return m0_conf_obj_is_stub(self_obj) || _0C(self->pc_endpoint != NULL);
}

M0_CONF__BOB_DEFINE(m0_conf_process, M0_CONF_PROCESS_MAGIC, process_check);
M0_CONF__INVARIANT_DEFINE(process_invariant, m0_conf_process);

static size_t _bitmap_width(const struct m0_bitmap_onwire *bow)
{
	return bow->bo_size * CHAR_BIT * sizeof bow->bo_words[0];
}

static int
process_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	int                            rc;
	struct m0_conf_process        *d = M0_CONF_CAST(dest, m0_conf_process);
	const struct m0_confx_process *s = XCAST(src);

	rc = m0_bitmap_init(&d->pc_cores, _bitmap_width(&s->xr_cores));
	if (rc != 0)
		return M0_ERR(rc);
	m0_bitmap_load(&s->xr_cores, &d->pc_cores);

	d->pc_memlimit_as      = s->xr_mem_limit_as;
	d->pc_memlimit_rss     = s->xr_mem_limit_rss;
	d->pc_memlimit_stack   = s->xr_mem_limit_stack;
	d->pc_memlimit_memlock = s->xr_mem_limit_memlock;

	d->pc_endpoint = m0_buf_strdup(&s->xr_endpoint);
	if (d->pc_endpoint == NULL) {
		m0_bitmap_fini(&d->pc_cores);
		return M0_ERR(-ENOMEM);
	}
	return M0_RC(m0_conf_dir_new(dest, &M0_CONF_PROCESS_SERVICES_FID,
				     &M0_CONF_SERVICE_TYPE, &s->xr_services,
				     &d->pc_services));
}

static int
process_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	int                      rc;
	struct m0_conf_process  *s = M0_CONF_CAST(src, m0_conf_process);
	struct m0_confx_process *d = XCAST(dest);

	confx_encode(dest, src);

	rc = m0_bitmap_onwire_init(&d->xr_cores, s->pc_cores.b_nr);
	if (rc != 0)
		return M0_ERR(rc);
	if (s->pc_cores.b_words != NULL)
		m0_bitmap_store(&s->pc_cores, &d->xr_cores);

	d->xr_mem_limit_as      = s->pc_memlimit_as;
	d->xr_mem_limit_rss     = s->pc_memlimit_rss;
	d->xr_mem_limit_stack   = s->pc_memlimit_stack;
	d->xr_mem_limit_memlock = s->pc_memlimit_memlock;
	return M0_RC((s->pc_endpoint == NULL ? 0 :
			m0_buf_copy(&d->xr_endpoint,
				&M0_BUF_INITS((char *)s->pc_endpoint))) ?:
		     arrfid_from_dir(&d->xr_services, s->pc_services));
}

static bool
process_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_process *xobj = XCAST(flat);
	const struct m0_conf_process  *obj =
		M0_CONF_CAST(cached, m0_conf_process);

	return  obj->pc_memlimit_as      == xobj->xr_mem_limit_as &&
		obj->pc_memlimit_rss     == xobj->xr_mem_limit_rss &&
		obj->pc_memlimit_stack   == xobj->xr_mem_limit_stack &&
		obj->pc_memlimit_memlock == xobj->xr_mem_limit_memlock &&
		m0_buf_streq(&xobj->xr_endpoint, obj->pc_endpoint) &&
		m0_conf_dir_elems_match(obj->pc_services, &xobj->xr_services);
}

static int process_lookup(const struct m0_conf_obj *parent,
			  const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_process *proc = M0_CONF_CAST(parent, m0_conf_process);
	const struct conf_dir_relation dirs[] = {
		{ proc->pc_services, &M0_CONF_PROCESS_SERVICES_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **process_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_PROCESS_SERVICES_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_PROCESS_TYPE);
	return rels;
}

static void process_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_process *x = M0_CONF_CAST(obj, m0_conf_process);

	m0_free((void *)x->pc_endpoint);
	m0_conf_process_bob_fini(x);
	if (x->pc_cores.b_nr != 0)
		m0_bitmap_fini(&x->pc_cores);
	m0_free(x);
}

static const struct m0_conf_obj_ops process_ops = {
	.coo_invariant = process_invariant,
	.coo_decode    = process_decode,
	.coo_encode    = process_encode,
	.coo_match     = process_match,
	.coo_lookup    = process_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = process_downlinks,
	.coo_delete    = process_delete
};

M0_CONF__CTOR_DEFINE(process_create, m0_conf_process, &process_ops);

const struct m0_conf_obj_type M0_CONF_PROCESS_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__PROCESS_FT_ID,
		.ft_name = "conf_process",
	},
	.cot_create  = &process_create,
	.cot_xt      = &m0_confx_process_xc,
	.cot_branch  = "u_process",
	.cot_xc_init = &m0_xc_m0_confx_process_struct_init,
	.cot_magic   = M0_CONF_PROCESS_MAGIC
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
