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
 * Original creation date: 09-Mar-2012
 */
#pragma once
#ifndef __MERO_CONF_OBJOPS_H__
#define __MERO_CONF_OBJOPS_H__

#include "conf/obj.h"

struct m0_conf_cache;
struct m0_confx_obj;
struct m0_fid_arr;

/**
 * @page conf-fspec-objops Configuration Object Operations
 *
 * @section conf-fspec-objops-data Data Structures
 *
 * - m0_conf_obj_ops --- configuration object operations vector.
 *
 * @section conf-fspec-objops-sub Subroutines
 *
 * - m0_conf_obj_create() allocates and initialises configuration object.
 * - m0_conf_obj_find() finds registered object or creates and a stub.
 * - m0_conf_obj_delete() finalises and frees configuration object.
 * - m0_conf_obj_invariant() validates given configuration object.
 * - m0_conf_obj_get() increases and m0_conf_obj_put() decreases
 *   object's number of references.
 * - m0_conf_obj_fill() enriches a stub with configuration data.
 * - m0_conf_obj_match() compares cached configuration object with its
 *   on-wire representation.
 *
 * @see @ref conf_dfspec_objops "Detailed Functional Specification"
 */

/**
 * @defgroup conf_dfspec_objops Configuration Object Operations
 * @brief Detailed Functional Specification.
 *
 * @see @ref conf, @ref conf-fspec-objops "Functional Specification"
 *
 * @{
 */

/** Symbolic names for m0_conf_obj_ops::coo_readdir() return values. */
enum m0_conf_dirval {
	/** End of directory is reached. */
	M0_CONF_DIREND = 0,
	/** The next directory entry is available immediately. */
	M0_CONF_DIRNEXT,
	/**
	 * The next directory entry is missing from configuration
	 * cache or is a stub.
	 */
	M0_CONF_DIRMISS
};

/** Configuration object operations. */
struct m0_conf_obj_ops {
	/** Validates concrete fields of given configuration object. */
	bool (*coo_invariant)(const struct m0_conf_obj *obj);

	/**
	 * Populates concrete object with configuration data taken
	 * from m0_confx_obj.
	 *
	 * Creates stubs of object's neighbours, if necessary.
	 */
	int (*coo_decode)(struct m0_conf_obj *dest,
			  const struct m0_confx_obj *src);

	/**
	 * Serialises concrete configuration object into its network
	 * representation.
	 */
	int (*coo_encode)(struct m0_confx_obj *dest,
			  const struct m0_conf_obj *src);

	/**
	 * Returns false iff cached configuration object and "flat" object
	 * have conflicting data.
	 */
	bool (*coo_match)(const struct m0_conf_obj *cached,
			  const struct m0_confx_obj *flat);

	/**
	 * Finds a child of given object.
	 *
	 * @param parent  The object being searched.
	 * @param name    Name of the relation leading to a child object.
	 *                Identifier of the child object, if parent is
	 *                a directory.
	 * @param out     If the function succeeds, *out will point to the
	 *                sought-for object.
	 *
	 * @pre   parent->co_status == M0_CS_READY
	 * @post  M0_IN(retval, (0, -ENOENT))
	 * @post  ergo(retval == 0, m0_conf_obj_invariant(*out))
	 */
	int (*coo_lookup)(const struct m0_conf_obj *parent,
			  const struct m0_fid *name, struct m0_conf_obj **out);

	/**
	 * Gets next directory entry.
	 *
	 * @param      dir   Directory.
	 * @param[in]  pptr  "Current" entry.
	 * @param[out] pptr  "Next" entry.
	 *
	 * @retval M0_CONF_DIRMISS  The next directory entry is missing from
	 *                          configuration cache or is a stub.
	 * @retval M0_CONF_DIRNEXT  *pptr now points to the next entry.
	 * @retval M0_CONF_DIREND   End of directory is reached.
	 * @retval -Exxx            Error.
	 *
	 * ->coo_readdir() puts (m0_conf_obj_put()) the configuration
	 * object referred to via `pptr' input parameter.
	 *
	 * ->coo_readdir() pins (m0_conf_obj_get()) the resulting
	 * object in case of M0_CONF_DIRNEXT.
	 */
	int (*coo_readdir)(const struct m0_conf_obj *dir,
			   struct m0_conf_obj **pptr);

	/**
	 * Returns NULL-terminated array of the downlink fids of this
	 * conf object.
	 *
	 * Downlink fid can be passed as `name' parameter to ->coo_lookup().
	 *
	 * @note .coo_downlinks == NULL in the case of m0_conf_dir only.
	 */
	const struct m0_fid **(*coo_downlinks)(const struct m0_conf_obj *obj);

	/**
	 * Finalises concrete fields of given configuration object and
	 * frees it.
	 *
	 * @see m0_conf_obj_delete()
	 */
	void (*coo_delete)(struct m0_conf_obj *obj);
};

/**
 * Allocates and initialises configuration object of given type.
 *
 * Copies `id' into ->co_id of the resulting object.
 *
 * Note, that m0_conf_obj_create() does not add the resulting object
 * into configuration cache.
 *
 * @pre   cache != NULL && m0_fid_is_set(id)
 * @post  ergo(retval != NULL, retval->co_status == M0_CS_MISSING)
 */
M0_INTERNAL struct m0_conf_obj *m0_conf_obj_create(const struct m0_fid *id,
						   struct m0_conf_cache *cache);

/**
 * Finds registered object with given identity or, if no object is
 * found, creates and registers a stub.
 *
 * @pre   m0_mutex_is_locked(cache->ca_lock)
 * @post  ergo(retval == 0,
 *             m0_conf_obj_invariant(*out) && (*out)->co_cache == cache)
 */
M0_INTERNAL int m0_conf_obj_find(struct m0_conf_cache *cache,
				 const struct m0_fid *id,
				 struct m0_conf_obj **out);

M0_INTERNAL int m0_conf_obj_find_lock(struct m0_conf_cache *cache,
				      const struct m0_fid *id,
				      struct m0_conf_obj **out);
/**
 * Finalises and frees configuration object.
 *
 * @pre  obj->co_nrefs == 0 && obj->co_status != M0_CS_LOADING
 */
M0_INTERNAL void m0_conf_obj_delete(struct m0_conf_obj *obj);

/** Validates given configuration object. */
M0_INTERNAL bool m0_conf_obj_invariant(const struct m0_conf_obj *obj);

/**
 * Increments reference counter of given configuration object.
 *
 * @pre   m0_mutex_is_locked(obj->co_cache->ca_lock)
 * @pre   obj->co_status == M0_CS_READY
 * @post  obj->co_nrefs > 0
 */
M0_INTERNAL void m0_conf_obj_get(struct m0_conf_obj *obj);

M0_INTERNAL void m0_conf_obj_get_lock(struct m0_conf_obj *obj);
/**
 * Decrements reference counter of given configuration object.
 *
 * Broadcasts obj->co_chan if the object becomes unpinned (i.e., if
 * the decremented counter reaches 0).
 *
 * @pre  m0_mutex_is_locked(obj->co_cache->ca_lock)
 * @pre  obj->co_nrefs > 0 && obj->co_status == M0_CS_READY
 */
M0_INTERNAL void m0_conf_obj_put(struct m0_conf_obj *obj);

/**
 * Enriches a stub with configuration data.
 *
 * @param dest   A stub to be filled with configuration data.
 * @param src    On-wire object, providing the configuration data.
 *
 * Note, that the caller is responsible for passing valid m0_confx_obj
 * via `src' parameter.
 *
 * @pre   `src' is valid
 * @pre   m0_mutex_is_locked(dest->co_cache->ca_lock)
 * @pre   m0_conf_obj_is_stub(dest) && dest->co_nrefs == 0
 * @pre   m0_conf_obj_type(dest) == m0_conf_objx_type(src)
 * @pre   m0_fid_eq(&dest->co_id, &src->o_id)
 *
 * @post  m0_conf_obj_invariant(dest)
 * @post  m0_mutex_is_locked(dest->co_cache->ca_lock)
 * @post  dest->co_status == (retval == 0 ? M0_CS_READY : M0_CS_MISSING)
 */
M0_INTERNAL int m0_conf_obj_fill(struct m0_conf_obj *dest,
				 const struct m0_confx_obj *src);

/**
 * Returns false iff cached configuration object and on-wire object
 * have conflicting data.
 *
 * Note, that the caller is responsible for passing valid m0_confx_obj
 * via `flat' parameter.
 *
 * @pre  `flat' is valid
 */
M0_INTERNAL bool m0_conf_obj_match(const struct m0_conf_obj *cached,
				   const struct m0_confx_obj *flat);

/** @} conf_dfspec_objops */
#endif /* __MERO_CONF_OBJOPS_H__ */
