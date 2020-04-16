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
 * Original creation date: 04-Mar-2012
 */
#pragma once
#ifndef __MERO_CONF_CACHE_H__
#define __MERO_CONF_CACHE_H__

#include "conf/obj.h"
#include "lib/tlist.h"  /* M0_TL_DESCR_DECLARE */

struct m0_mutex;

M0_TL_DESCR_DECLARE(m0_conf_cache, extern);
M0_TL_DECLARE(m0_conf_cache, M0_INTERNAL, struct m0_conf_obj);

/**
 * @page conf-fspec-cache Configuration Cache
 *
 * Configuration cache comprises a set of dynamically allocated
 * configuration objects, interconnected into directed acyclic graph
 * (DAG).  The cache is represented by m0_conf_cache structure.
 *
 * A registry of cached configuration objects --
 * m0_con_cache::ca_registry -- performs the following functions:
 *
 *   - maps object identities to memory addresses of these objects;
 *
 *   - ensures uniqueness of configuration objects in the cache.
 *     After an object has been added to the registry, any attempt to
 *     add another one with similar identity will fail;
 *
 *   - simplifies erasing of configuration cache.
 *     m0_conf_cache_fini() frees all configuration objects that are
 *     registered. No sophisticated DAG traversal is needed.
 *
 * @note Configuration consumers should not #include "conf/cache.h".
 *       This is "internal" API, used by confc and confd
 *       implementations.
 *
 * @section conf-fspec-cache-thread Concurrency control
 *
 * m0_conf_cache::ca_lock should be acquired prior to modifying cached
 * configuration objects.
 *
 * @see @ref conf_dfspec_cache "Detailed Functional Specification"
 */

/**
 * @defgroup conf_dfspec_cache Configuration Cache
 * @brief Detailed Functional Specification.
 *
 * @see @ref conf, @ref conf-fspec-cache "Functional Specification"
 *
 * @{
 */

enum m0_conf_version {
	/**
	 * Reserved version number indicating that version election has
	 * never been carried out, or has failed.
	 *
	 * @see m0_rconfc
	 */
	M0_CONF_VER_UNKNOWN = 0,
	/**
	 * Reserved version number indicating that m0_conf_root::rt_verno is not
	 * set yet, i. e. Spiel transaction was opened but not committed yet.
	 *
	 * @see m0_spiel_tx_open()
	 * @see m0_spiel_tx_commit()
	 */
	M0_CONF_VER_TEMP = ~0,
};

/** Configuration cache. */
struct m0_conf_cache {
	/**
	 * Registry of cached configuration objects.
	 * List of m0_conf_obj-s, linked through m0_conf_obj::co_cache_link.
	 */
	struct m0_tl     ca_registry;

	/** Cache lock. */
	struct m0_mutex *ca_lock;

	/** Configuration version number. */
	uint64_t         ca_ver;

	/**
	 * Running counter, used by conf_objv_virtual_fid() to generate
	 * fids of newly created m0_conf_objv objects.
	 */
	uint64_t         ca_fid_counter;
};

/** Initialises configuration cache. */
M0_INTERNAL void m0_conf_cache_init(struct m0_conf_cache *cache,
				    struct m0_mutex *lock);

/**
 * Finalises configuration cache.
 *
 * m0_conf_obj_delete()s every registered configuration object.
 */
M0_INTERNAL void m0_conf_cache_fini(struct m0_conf_cache *cache);

M0_INTERNAL void m0_conf_cache_lock(struct m0_conf_cache *cache);
M0_INTERNAL void m0_conf_cache_unlock(struct m0_conf_cache *cache);
M0_INTERNAL bool m0_conf_cache_is_locked(const struct m0_conf_cache *cache);

/**
 * Deletes registered objects of specific type or, if `type' is NULL,
 * all registered configuration objects.
 *
 * Note that m0_conf_cache_clean(cache, NULL) does not finalise
 * configuration cache.
 *
 * @pre  m0_conf_cache_is_locked(cache)
 *
 * @see m0_conf_cache_fini(), m0_conf_obj_delete()
 */
M0_INTERNAL void m0_conf_cache_clean(struct m0_conf_cache *cache,
				     const struct m0_conf_obj_type *type);

/**
 * Deletes registered objects with m0_conf_cache::co_deleted flag set.
 *
 * @pre  m0_conf_cache_is_locked(cache)
 */
M0_INTERNAL void m0_conf_cache_gc(struct m0_conf_cache *cache);

/**
 * Adds configuration object to the cache.
 *
 * @pre  m0_conf_cache_is_locked(cache)
 * @pre  !m0_conf_cache_tlink_is_in(obj)
 */
M0_INTERNAL int m0_conf_cache_add(struct m0_conf_cache *cache,
				  struct m0_conf_obj *obj);

/**
 * Unregisters and m0_conf_obj_delete()s configuration object.
 *
 * @pre  m0_conf_cache_is_locked(cache)
 * @pre  m0_conf_cache_tlist_contains(&cache->ca_registry, obj)
 */
M0_INTERNAL void m0_conf_cache_del(const struct m0_conf_cache *cache,
				   struct m0_conf_obj *obj);

/**
 * Checks if an object with given fid exists in conf cache.
 */
M0_INTERNAL bool m0_conf_cache_contains(struct m0_conf_cache *cache,
				        const struct m0_fid *fid);
/**
 * Searches for a configuration object given its identity (type & id).
 *
 * Returns NULL if there is no such object in the cache.
 */
M0_INTERNAL struct m0_conf_obj *
m0_conf_cache_lookup(const struct m0_conf_cache *cache,
		     const struct m0_fid *id);

/**
 * Creates conf string representation of all objects in the cache,
 * except m0_conf_dir objects.
 *
 * If `debug' is true, the checking of conf objects' invariants will be
 * skipped.
 *
 * @note If the call succeeds, the user is responsible for freeing
 *       allocated memory with m0_confx_string_free(*str).
 *
 * @see m0_conf_cache_from_string()
 */
M0_INTERNAL int m0_conf_cache_to_string(struct m0_conf_cache *cache, char **str,
					bool debug);

/**
 * Loads conf cache from a string.
 *
 * @pre str != NULL
 * @pre m0_conf_cache_is_locked(cache)
 *
 * @see m0_conf_cache_to_string()
 */
M0_INTERNAL int m0_conf_cache_from_string(struct m0_conf_cache *cache,
					  const char           *str);

/** Returns m0_conf_root::rt_verno of the root object. */
M0_INTERNAL int m0_conf_version(struct m0_conf_cache *cache);

/**
 * Searches the configuration cache for a pinned object.
 * Returns NULL if none is found.
 *
 * @pre  m0_conf_cache_is_locked(cache)
 */
M0_INTERNAL struct m0_conf_obj *
m0_conf_cache_pinned(const struct m0_conf_cache *cache);

/**
 * Maximum number of path components.
 *
 * Path is a sequence of m0_fids, used for conf DAG traversal.
 */
enum { M0_CONF_PATH_MAX = 15 };

/** @} conf_dfspec_cache */
#endif /* __MERO_CONF_CACHE_H__ */
