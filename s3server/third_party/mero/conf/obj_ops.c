/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 09-Feb-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/obj_ops.h"
#include "conf/cache.h"
#include "conf/onwire.h"   /* m0_confx_obj */
#include "conf/dir.h"      /* m0_conf_dir_tl */
#include "mero/magic.h"    /* M0_CONF_OBJ_MAGIC */
#include "lib/errno.h"     /* ENOMEM */

/**
 * @defgroup conf_dlspec_objops Configuration Object Operations (lspec)
 *
 * @see @ref conf, @ref conf-lspec
 *
 * @{
 */

static bool _generic_obj_invariant(const void *bob);
static bool _concrete_obj_invariant(const struct m0_conf_obj *obj);

static const struct m0_bob_type generic_obj_bob = {
	.bt_name         = "m0_conf_obj",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_conf_obj, co_gen_magic),
	.bt_magix        = M0_CONF_OBJ_MAGIC,
	.bt_check        = _generic_obj_invariant
};
M0_BOB_DEFINE(static, &generic_obj_bob, m0_conf_obj);

M0_INTERNAL bool m0_conf_obj_invariant(const struct m0_conf_obj *obj)
{
	return _0C(m0_conf_obj_bob_check(obj)) && _concrete_obj_invariant(obj);
}

static bool _generic_obj_invariant(const void *bob)
{
	const struct m0_conf_obj *obj = bob;

	return _0C(m0_fid_is_set(&obj->co_id) && obj->co_ops != NULL) &&
	       _0C(m0_conf_fid_is_valid(&obj->co_id)) &&
	       _0C(obj->co_ha_state >= M0_NC_UNKNOWN) &&
	       _0C(obj->co_ha_state < M0_NC_NR &&
			M0_IN(obj->co_status, (M0_CS_MISSING, M0_CS_LOADING,
					       M0_CS_READY))) &&
	       _0C(ergo(m0_conf_obj_is_stub(obj), obj->co_nrefs == 0));
}

static bool _concrete_obj_invariant(const struct m0_conf_obj *obj)
{
	bool ret = obj->co_ops->coo_invariant(obj);
	if (unlikely(!ret))
		M0_LOG(M0_ERROR, "Configuration object invariant does not hold:"
		       " id="FID_F, FID_P(&obj->co_id));
	return ret;
}

M0_INTERNAL struct m0_conf_obj *
m0_conf_obj_create(const struct m0_fid *id, struct m0_conf_cache *cache)
{
	struct m0_conf_obj            *obj;
	const struct m0_conf_obj_type *type = m0_conf_fid_type(id);

	M0_PRE(cache != NULL && m0_fid_is_set(id));
	M0_PRE(type->cot_create != NULL);

	/* Allocate concrete object; initialise concrete fields. */
	obj = type->cot_create();
	if (obj == NULL)
		return NULL;

	/* Initialise generic fields. */
	obj->co_id = *id;
	obj->co_status = M0_CS_MISSING;
	obj->co_cache = cache;
	m0_conf_obj_bob_init(obj);
	M0_ASSERT(obj->co_gen_magic == M0_CONF_OBJ_MAGIC);
	M0_ASSERT(obj->co_con_magic == type->cot_magic);
	m0_chan_init(&obj->co_chan, cache->ca_lock);
	m0_chan_init(&obj->co_ha_chan, cache->ca_lock);
	m0_conf_cache_tlink_init(obj);
	m0_conf_dir_tlink_init(obj);
	/*
	 * m0_ha_state_accept() does not expect .co_ha_state to be
	 * M0_NC_UNKNOWN. Initialise it to M0_NC_ONLINE.
	 */
	obj->co_ha_state = M0_NC_ONLINE;
	m0_conf_obj_bob_init(obj);
	M0_ASSERT(obj->co_gen_magic == M0_CONF_OBJ_MAGIC);
	M0_ASSERT(obj->co_con_magic == type->cot_magic);

	M0_POST(m0_conf_obj_invariant(obj));
	return obj;
}

static int stub_create(struct m0_conf_cache *cache,
		       const struct m0_fid *id, struct m0_conf_obj **out)
{
	int rc;

	M0_ENTRY();

	*out = m0_conf_obj_create(id, cache);
	if (*out == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_conf_cache_add(cache, *out);
	if (rc != 0) {
		m0_conf_obj_delete(*out);
		*out = NULL;
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_conf_obj_find(struct m0_conf_cache *cache,
				 const struct m0_fid *id,
				 struct m0_conf_obj **out)
{
	int rc = 0;

	M0_ENTRY(FID_F, FID_P(id));
	M0_PRE(m0_conf_cache_is_locked(cache));

	*out = m0_conf_cache_lookup(cache, id);
	if (*out == NULL)
		rc = stub_create(cache, id, out);

	M0_POST(ergo(rc == 0,
		     m0_conf_obj_invariant(*out) && (*out)->co_cache == cache));
	return M0_RC(rc);
}

M0_INTERNAL int m0_conf_obj_find_lock(struct m0_conf_cache *cache,
				      const struct m0_fid *id,
				      struct m0_conf_obj **out)
{
	int rc;

	M0_ENTRY();
	m0_conf_cache_lock(cache);
	rc = m0_conf_obj_find(cache, id, out);
	m0_conf_cache_unlock(cache);
	return M0_RC(rc);
}

M0_INTERNAL void m0_conf_obj_delete(struct m0_conf_obj *obj)
{
	M0_ENTRY("obj=%p/"FID_F, obj, FID_P(&obj->co_id));
	M0_PRE(m0_conf_obj_invariant(obj));
	M0_PRE(obj->co_nrefs == 0);
	M0_PRE(obj->co_status != M0_CS_LOADING);

	/* Finalise generic fields. */
	m0_conf_dir_tlink_fini(obj);
	m0_conf_cache_tlink_fini(obj);
	m0_chan_fini(&obj->co_ha_chan);
	m0_chan_fini(&obj->co_chan);
	m0_conf_obj_bob_fini(obj);

	/* Finalise concrete fields; free the object. */
	obj->co_ops->coo_delete(obj);
	M0_LEAVE();
}

M0_INTERNAL void m0_conf_obj_get(struct m0_conf_obj *obj)
{
	M0_ENTRY("obj=%p/"FID_F" nrefs: %"PRIu64" -> %"PRIu64, obj,
		 FID_P(&obj->co_id), obj->co_nrefs, obj->co_nrefs + 1);
	M0_PRE(m0_conf_obj_invariant(obj));
	M0_PRE(m0_conf_cache_is_locked(obj->co_cache));
	M0_PRE(obj->co_status == M0_CS_READY);

	M0_CNT_INC(obj->co_nrefs);
	M0_LEAVE();
}

M0_INTERNAL void m0_conf_obj_get_lock(struct m0_conf_obj *obj)
{
	m0_conf_cache_lock(obj->co_cache);
	m0_conf_obj_get(obj);
	m0_conf_cache_unlock(obj->co_cache);
}

M0_INTERNAL void m0_conf_obj_put(struct m0_conf_obj *obj)
{
	M0_ENTRY("obj=%p/"FID_F" nrefs: %"PRIu64" -> %"PRIu64, obj,
		 FID_P(&obj->co_id), obj->co_nrefs, obj->co_nrefs - 1);
	M0_PRE(m0_conf_obj_invariant(obj));
	M0_PRE(m0_conf_cache_is_locked(obj->co_cache));
	M0_PRE(obj->co_status == M0_CS_READY);

	M0_CNT_DEC(obj->co_nrefs);
	if (obj->co_nrefs == 0)
		m0_chan_broadcast(&obj->co_chan);
	M0_LEAVE();
}

/** Performs sanity checking of given m0_confx_obj. */
static bool confx_obj_is_valid(const struct m0_confx_obj *flat)
{
	/* XXX TODO
	 * - All of m0_fid-s contained in `flat' are m0_fid_is_set();
	 * - all of fid_arr-s are populated with valid m0_fid-s;
	 * - etc.
	 */
	(void)flat; /* XXX */
	return true;
}

M0_INTERNAL int
m0_conf_obj_fill(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	int                   rc;
	struct m0_conf_cache *cache = dest->co_cache;

	M0_ENTRY("dest="FID_F, FID_P(&dest->co_id));
	M0_PRE(m0_conf_obj_invariant(dest));
	M0_PRE(m0_conf_cache_is_locked(cache));
	M0_PRE(m0_conf_obj_is_stub(dest) && dest->co_nrefs == 0);
	M0_PRE(m0_conf_obj_type(dest) == m0_conf_objx_type(src));
	M0_PRE(m0_fid_eq(&dest->co_id, m0_conf_objx_fid(src)));
	M0_PRE(confx_obj_is_valid(src));

	rc = dest->co_ops->coo_decode(dest, src);
	dest->co_status = rc == 0 ? M0_CS_READY : M0_CS_MISSING;

	M0_POST(m0_conf_cache_is_locked(cache));
	M0_POST(ergo(rc == 0, m0_conf_obj_invariant(dest)));

	return M0_RC(rc);
}

M0_INTERNAL bool m0_conf_obj_match(const struct m0_conf_obj *cached,
				   const struct m0_confx_obj *flat)
{
	M0_PRE(m0_conf_obj_invariant(cached));
	M0_PRE(confx_obj_is_valid(flat));

	return m0_conf_obj_type(cached) == m0_conf_objx_type(flat) &&
		m0_fid_eq(&cached->co_id, m0_conf_objx_fid(flat)) &&
		(m0_conf_obj_is_stub(cached) ||
		 cached->co_ops->coo_match(cached, flat));
}

const struct m0_fid *m0_conf_objx_fid(const struct m0_confx_obj *obj)
{
	return &obj->xo_u.u_header.ch_id;
}

const struct m0_conf_obj_type *m0_conf_objx_type(const struct m0_confx_obj *obj)
{
	return m0_conf_fid_type(m0_conf_objx_fid(obj));
}

/** @} conf_dlspec_objops */
#undef M0_TRACE_SUBSYSTEM
