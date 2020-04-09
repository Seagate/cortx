/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 05-May-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/confd_fom.h"
#include "conf/fop.h"         /* m0_conf_fetch_resp_fopt */
#include "conf/confd.h"       /* m0_confd, m0_confd_bob */
#include "conf/obj_ops.h"     /* m0_conf_obj_invariant */
#include "conf/preload.h"     /* m0_confx_free */
#include "conf/onwire.h"
#include "rpc/rpc_opcodes.h"  /* M0_CONF_FETCH_OPCODE */
#include "fop/fom_generic.h"  /* M0_FOPH_NR */
#include "fid/fid.h"          /* m0_fid, m0_fid_arr */
#include "lib/memory.h"       /* m0_free */
#include "lib/misc.h"         /* M0_SET0 */
#include "lib/errno.h"        /* ENOMEM, EOPNOTSUPP */

/**
 * @addtogroup confd_dlspec
 *
 * @{
 */

static int conf_fetch_tick(struct m0_fom *fom);
static int conf_update_tick(struct m0_fom *fom);
static int confx_populate(struct m0_confx *dest, const struct m0_fid *origin,
			  const struct m0_fid_arr *path,
			  struct m0_conf_cache *cache);

static size_t confd_fom_locality(const struct m0_fom *fom)
{
	return m0_fop_opcode(fom->fo_fop);
}

static void confd_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(container_of(fom, struct m0_confd_fom, dm_fom));
}

static const struct m0_fom_ops conf_fetch_fom_ops = {
	.fo_home_locality = confd_fom_locality,
	.fo_tick          = conf_fetch_tick,
	.fo_fini          = confd_fom_fini
};

static const struct m0_fom_ops conf_update_fom_ops = {
	.fo_home_locality = confd_fom_locality,
	.fo_tick          = conf_update_tick,
	.fo_fini          = confd_fom_fini
};

M0_INTERNAL int m0_confd_fom_create(struct m0_fop   *fop,
				    struct m0_fom  **out,
				    struct m0_reqh  *reqh)
{
	struct m0_confd_fom     *m;
	const struct m0_fom_ops *ops;
	struct m0_fop           *rep_fop;

	M0_ENTRY();

	M0_ALLOC_PTR(m);
	if (m == NULL)
		return M0_ERR(-ENOMEM);

	switch (m0_fop_opcode(fop)) {
	case M0_CONF_FETCH_OPCODE:
		ops = &conf_fetch_fom_ops;
		break;
	case M0_CONF_UPDATE_OPCODE:
		ops = &conf_update_fom_ops;
		break;
	default:
		m0_free(m);
		return M0_ERR(-EOPNOTSUPP);
	}
	rep_fop = m0_fop_reply_alloc(fop, &m0_conf_fetch_resp_fopt);
	if (rep_fop == NULL) {
		m0_free(m);
		return M0_ERR(-ENOMEM);
	}
	m0_fom_init(&m->dm_fom, &fop->f_type->ft_fom_type, ops, fop, rep_fop,
	            reqh);
	*out = &m->dm_fom;

	return M0_RC(0);
}

static int conf_fetch_tick(struct m0_fom *fom)
{
	const struct m0_conf_fetch *q;
	struct m0_conf_fetch_resp  *r;
	struct m0_confd            *confd;
	int                         rc;

	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	q = m0_fop_data(fom->fo_fop);
	r = m0_fop_data(fom->fo_rep_fop);

	confd = bob_of(fom->fo_service, struct m0_confd, d_reqh, &m0_confd_bob);
	m0_conf_cache_lock(confd->d_cache);
	rc = confx_populate(&r->fr_data, &q->f_origin, &q->f_path,
			    confd->d_cache);
	m0_conf_cache_unlock(confd->d_cache);
	if (rc != 0)
		M0_ASSERT(r->fr_data.cx_nr == 0 && r->fr_data.cx__objs == NULL);
	r->fr_rc = rc;
	r->fr_ver = confd->d_cache->ca_ver;
	M0_ASSERT(r->fr_ver != M0_CONF_VER_UNKNOWN);
	m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}

static int conf_update_tick(struct m0_fom *fom)
{
	M0_IMPOSSIBLE("XXX Not implemented");

	m0_fom_phase_move(fom, -ENOSYS, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}

static int _count(size_t                   *n,
		  struct m0_confx          *enc M0_UNUSED,
		  const struct m0_conf_obj *obj M0_UNUSED)
{
	++*n;
	return 0;
}

static int
_encode(size_t *n, struct m0_confx *enc, const struct m0_conf_obj *obj)
{
	int rc;

	M0_PRE(m0_conf_obj_invariant(obj) && obj->co_status == M0_CS_READY);

	M0_CNT_DEC(*n);
	rc = obj->co_ops->coo_encode(M0_CONFX_AT(enc, enc->cx_nr), obj);
	if (rc == 0)
		++enc->cx_nr;
	return M0_RC(rc);
}

static int readiness_check(const struct m0_conf_obj *obj)
{
	if (m0_conf_obj_is_stub(obj)) {
		/* All configuration is expected to be loaded already. */
		M0_ASSERT(obj->co_status != M0_CS_LOADING);
		return M0_ERR(-ENOENT);
	}
	return 0;
}

/**
 * Traverses the DAG of configuration objects.
 * Starts at `cur' and follows the `path', applying given function.
 *
 * @param cur    Path origin.
 * @param path   Sequence of path components as requested by confc.
 * @param apply  The action to perform.
 * @param n      Address of the counter to be used by apply().
 * @param enc    Encoded configuration data to be used by apply().
 */
static int confd_path_walk(struct m0_conf_obj *cur,
			   const struct m0_fid_arr *path,
			   int (*apply)(size_t *n, struct m0_confx *enc,
			   const struct m0_conf_obj *obj),
			   size_t *n, struct m0_confx *enc)
{
	struct m0_conf_obj *entry;
	int                 i;
	int                 rc;

	M0_ENTRY();
	M0_PRE(m0_conf_obj_invariant(cur) && cur->co_status == M0_CS_READY);

	for (i = 0; i < path->af_count; ++i) {
		/* Handle intermediate object. */
		if (m0_conf_obj_type(cur) != &M0_CONF_DIR_TYPE) {
			rc = apply(n, enc, cur);
			if (rc != 0)
				return M0_ERR(rc);
		}

		rc = cur->co_ops->coo_lookup(cur, &path->af_elems[i], &cur) ?:
			readiness_check(cur);
		if (rc != 0)
			return M0_ERR(rc);
	}

	/* Handle final object. */

	if (cur->co_parent != NULL && cur->co_parent != cur &&
	    m0_conf_obj_type(cur->co_parent) == &M0_CONF_DIR_TYPE)
		/* Include siblings into the resulting set. */
		cur = cur->co_parent;

	if (m0_conf_obj_type(cur) != &M0_CONF_DIR_TYPE)
		return M0_RC(apply(n, enc, cur));

	m0_conf_obj_get(cur); /* required by ->coo_readdir() */
	for (entry = NULL; (rc = cur->co_ops->coo_readdir(cur, &entry)) > 0; ) {
		/* All configuration is expected to be available. */
		M0_ASSERT(rc != M0_CONF_DIRMISS);

		rc = apply(n, enc, entry);
		if (rc != 0)
			return M0_ERR(rc);
	}
	m0_conf_obj_put(cur);
	return M0_RC(rc);
}

static struct m0_conf_obj *
confd_cache_lookup(const struct m0_conf_cache *cache, const struct m0_fid *fid)
{
	struct m0_conf_obj *obj;

	obj = m0_conf_cache_lookup(cache, fid);
	if (obj == NULL)
		M0_LOG(M0_NOTICE, "fid=" FID_F " not found", FID_P(fid));
	else
		M0_POST(m0_conf_obj_invariant(obj) && obj->co_cache == cache &&
			obj->co_status == M0_CS_READY);
	return obj;
}

static void cache_ver_update(struct m0_conf_cache *cache)
{
	struct m0_conf_obj *root;

	root = confd_cache_lookup(cache, &M0_CONF_ROOT_FID);
	M0_ASSERT(root != NULL); /* conf data must be cached already */

	cache->ca_ver = M0_CONF_CAST(root, m0_conf_root)->rt_verno;
	M0_POST(cache->ca_ver != M0_CONF_VER_UNKNOWN);
}

static int confx_populate(struct m0_confx         *dest,
			  const struct m0_fid     *origin,
			  const struct m0_fid_arr *path,
			  struct m0_conf_cache    *cache)
{
	struct m0_conf_obj *org;
	int                 rc;
	size_t              nr = 0;
	char               *data;

	M0_ENTRY();
	M0_PRE(m0_conf_cache_is_locked(cache));

	if (cache->ca_ver == M0_CONF_VER_UNKNOWN)
		cache_ver_update(cache);

	M0_SET0(dest);

	org = confd_cache_lookup(cache, origin);
	if (org == NULL)
		return M0_ERR_INFO(-ENOENT, "origin="FID_F, FID_P(origin));

	rc = confd_path_walk(org, path, _count, &nr, NULL);
	if (rc != 0 || nr == 0)
		return M0_RC(rc);

	M0_ALLOC_ARR(data, nr * m0_confx_sizeof());
	if (data == NULL)
		return M0_ERR(-ENOMEM);
	/* "data" is freed by m0_confx_free(dest). */
	dest->cx__objs = (void *)data;

	M0_LOG(M0_DEBUG, "Will encode %zu configuration object%s", nr,
	       (char *)(nr > 1 ? "s" : ""));
	rc = confd_path_walk(org, path, _encode, &nr, dest);
	if (rc == 0)
		M0_ASSERT(nr == 0);
	else
		m0_confx_free(dest);
	return M0_RC(rc);
}

/** @} confd_dlspec */
#undef M0_TRACE_SUBSYSTEM
