/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 11-Mar-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/cache.h"
#include "conf/obj_ops.h"   /* m0_conf_obj_delete */
#include "conf/preload.h"   /* m0_confx_to_string */
#include "mero/magic.h"     /* M0_CONF_OBJ_MAGIC, M0_CONF_CACHE_MAGIC */
#include "conf/onwire.h"    /* m0_confx */
#include "lib/errno.h"      /* EEXIST */
#include "lib/memory.h"     /* M0_ALLOC_PTR, M0_ALLOC_ARR */

/**
 * @defgroup conf_dlspec_cache Configuration Cache (lspec)
 *
 * The implementation of m0_conf_cache::ca_registry is based on linked
 * list data structure.
 *
 * @see @ref conf, @ref conf-lspec
 *
 * @{
 */

M0_TL_DESCR_DEFINE(m0_conf_cache, "registered m0_conf_obj-s", ,
		   struct m0_conf_obj, co_cache_link, co_gen_magic,
		   M0_CONF_OBJ_MAGIC, M0_CONF_CACHE_MAGIC);
M0_TL_DEFINE(m0_conf_cache, M0_INTERNAL, struct m0_conf_obj);

M0_INTERNAL void m0_conf_cache_lock(struct m0_conf_cache *cache)
{
	m0_mutex_lock(cache->ca_lock);
}

M0_INTERNAL void m0_conf_cache_unlock(struct m0_conf_cache *cache)
{
	m0_mutex_unlock(cache->ca_lock);
}

M0_INTERNAL bool m0_conf_cache_is_locked(const struct m0_conf_cache *cache)
{
	return m0_mutex_is_locked(cache->ca_lock);
}

M0_INTERNAL void
m0_conf_cache_init(struct m0_conf_cache *cache, struct m0_mutex *lock)
{
	M0_ENTRY();

	m0_conf_cache_tlist_init(&cache->ca_registry);
	cache->ca_lock = lock;
	cache->ca_ver  = 0;
	cache->ca_fid_counter = 0;

	M0_LEAVE();
}

M0_INTERNAL int
m0_conf_cache_add(struct m0_conf_cache *cache, struct m0_conf_obj *obj)
{
	const struct m0_conf_obj *x;

	M0_ENTRY();
	M0_PRE(m0_conf_cache_is_locked(cache));
	M0_PRE(!m0_conf_cache_tlink_is_in(obj));

	x = m0_conf_cache_lookup(cache, &obj->co_id);
	if (x != NULL)
		return M0_ERR(-EEXIST);
	m0_conf_cache_tlist_add(&cache->ca_registry, obj);
	return M0_RC(0);
}

M0_INTERNAL bool m0_conf_cache_contains(struct m0_conf_cache *cache,
				        const struct m0_fid *fid)
{
	bool ret;

	m0_conf_cache_lock(cache);
	ret = m0_conf_cache_lookup(cache, fid) != NULL;
	m0_conf_cache_unlock(cache);
	return ret;
}

M0_INTERNAL struct m0_conf_obj *
m0_conf_cache_lookup(const struct m0_conf_cache *cache,
		     const struct m0_fid *id)
{
	return m0_tl_find(m0_conf_cache, obj, &cache->ca_registry,
			  m0_fid_eq(&obj->co_id, id));
}

static void _obj_del(struct m0_conf_obj *obj)
{
	M0_ENTRY("obj="FID_F, FID_P(&obj->co_id));

	m0_conf_cache_tlist_del(obj);
	m0_conf_obj_delete(obj);

	M0_LEAVE();
}

M0_INTERNAL void
m0_conf_cache_del(const struct m0_conf_cache *cache, struct m0_conf_obj *obj)
{
	M0_ENTRY();
	M0_PRE(m0_conf_cache_is_locked(cache));
	M0_PRE(m0_conf_cache_tlist_contains(&cache->ca_registry, obj));

	_obj_del(obj);

	M0_LEAVE();
}

static void conf_cache_clean(struct m0_conf_cache *cache,
			     const struct m0_conf_obj_type *type,
			     bool gc)
{
	struct m0_conf_obj *obj;

	M0_ENTRY();
	M0_PRE(m0_conf_cache_is_locked(cache));

	if (type == NULL)
		/*
		 * m0_conf_dirs are intermixed with other objects in
		 * cache->ca_registry.  m0_conf_obj_delete() of a non-dir
		 * object will fail if this object is still linked to some
		 * dir --- there is m0_conf_dir_tlink_fini() in
		 * m0_conf_obj_delete(), and m0_tlink_fini() asserts
		 * that the entry does not belong any list.
		 *
		 * Delete dir objects first.
		 */
		conf_cache_clean(cache, &M0_CONF_DIR_TYPE, gc);

	m0_tl_for(m0_conf_cache, &cache->ca_registry, obj) {
		M0_ASSERT(gc || !obj->co_deleted);
		if (type == NULL || m0_conf_obj_type(obj) == type) {
			if (gc && !obj->co_deleted)
				continue;
			_obj_del(obj);
		}
	} m0_tl_endfor;
	M0_LEAVE();
}

M0_INTERNAL void m0_conf_cache_clean(struct m0_conf_cache *cache,
				     const struct m0_conf_obj_type *type)
{
	M0_ENTRY();
	conf_cache_clean(cache, type, false);
	M0_LEAVE();
}

M0_INTERNAL void m0_conf_cache_gc(struct m0_conf_cache *cache)
{
	M0_ENTRY();
	conf_cache_clean(cache, NULL, true);
	M0_LEAVE();
}

M0_INTERNAL void m0_conf_cache_fini(struct m0_conf_cache *cache)
{
	M0_ENTRY();

	m0_conf_cache_lock(cache);
	m0_conf_cache_clean(cache, NULL);
	m0_conf_cache_tlist_fini(&cache->ca_registry);
	m0_conf_cache_unlock(cache);

	M0_LEAVE();
}

static int
conf_encode(struct m0_confx *enc, const struct m0_conf_obj *obj, bool debug)
{
	int rc;

	M0_PRE(debug ||
	       (m0_conf_obj_invariant(obj) && obj->co_status == M0_CS_READY));

	rc = obj->co_ops->coo_encode(M0_CONFX_AT(enc, enc->cx_nr), obj);
	if (rc == 0)
		++enc->cx_nr;
	return M0_RC(rc);
}

static int conf_cache_encode(const struct m0_conf_cache *cache,
			     struct m0_confx *dest, bool debug)
{
	struct m0_conf_obj *obj;
	int                 rc;
	size_t              nr;
	char               *data;

	M0_ENTRY();
	M0_PRE(m0_conf_cache_is_locked(cache));

	M0_SET0(dest);

	if (!debug && !m0_tl_forall(m0_conf_cache, scan, &cache->ca_registry,
				    m0_conf_obj_invariant((obj = scan)) &&
				    scan->co_status == M0_CS_READY))
		return M0_ERR_INFO(-EINVAL, FID_F" status=%u",
				   FID_P(&obj->co_id), obj->co_status);

	nr = m0_tl_reduce(m0_conf_cache, scan, &cache->ca_registry, 0,
			  + (m0_conf_obj_type(scan) != &M0_CONF_DIR_TYPE));

	M0_ALLOC_ARR(data, nr * m0_confx_sizeof());
	if (data == NULL)
		return M0_ERR(-ENOMEM);
	/* "data" is freed by m0_confx_free(dest). */
	dest->cx__objs = (void *)data;

	M0_LOG(M0_DEBUG, "Will encode %zu configuration objects", nr);
	m0_tl_for(m0_conf_cache, &cache->ca_registry, obj) {
		if (m0_conf_obj_type(obj) != &M0_CONF_DIR_TYPE) {
			rc = conf_encode(dest, obj, debug);
			if (rc != 0)
				break;
		}
	} m0_tl_endfor;

	if (rc == 0) {
		M0_ASSERT(nr == dest->cx_nr);
	} else {
		m0_free(data);
		dest->cx__objs = NULL;
	}
	return M0_RC(rc);
}

M0_INTERNAL int
m0_conf_cache_to_string(struct m0_conf_cache *cache, char **str, bool debug)
{
	struct m0_confx *confx;
	int              rc;

	M0_ENTRY();

	M0_ALLOC_PTR(confx);
	if (confx == NULL)
		return M0_ERR(-ENOMEM);

	m0_conf_cache_lock(cache);
	rc = conf_cache_encode(cache, confx, debug);
	m0_conf_cache_unlock(cache);
	if (rc == 0)
		rc = m0_confx_to_string(confx, str);
	m0_confx_free(confx);
	return M0_RC(rc);
}

M0_INTERNAL int
m0_conf_cache_from_string(struct m0_conf_cache *cache, const char *str)
{
	struct m0_confx *enc;
	uint32_t         i;
	int              rc;

	M0_ENTRY();

	M0_PRE(str != NULL);
	M0_PRE(m0_conf_cache_is_locked(cache));

	rc = m0_confstr_parse(str, &enc);
	if (rc != 0)
		return M0_ERR(rc);

	for (i = 0; i < enc->cx_nr && rc == 0; ++i) {
		struct m0_conf_obj        *obj;
		const struct m0_confx_obj *xobj = M0_CONFX_AT(enc, i);

		rc = m0_conf_obj_find(cache, m0_conf_objx_fid(xobj), &obj) ?:
			m0_conf_obj_fill(obj, xobj);
	}
	m0_confx_free(enc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_conf_version(struct m0_conf_cache *cache)
{
	struct m0_conf_obj *obj;
	int                 ver = 0;

	M0_ENTRY();

	m0_conf_cache_lock(cache);
	obj = m0_conf_cache_lookup(cache, &M0_CONF_ROOT_FID);
	if (obj != NULL)
		ver = M0_CONF_CAST(obj, m0_conf_root)->rt_verno;
	m0_conf_cache_unlock(cache);
	return M0_RC(ver);
}

M0_INTERNAL struct m0_conf_obj *
m0_conf_cache_pinned(const struct m0_conf_cache *cache)
{
	M0_PRE(m0_conf_cache_is_locked(cache));
	return m0_tl_find(m0_conf_cache, obj, &cache->ca_registry,
			  obj->co_nrefs != 0);
}

/** @} conf_dlspec_cache */
#undef M0_TRACE_SUBSYSTEM
