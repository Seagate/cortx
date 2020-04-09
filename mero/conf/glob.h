/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 12-Feb-2016
 */
#pragma once
#ifndef __MERO_CONF_GLOB_H__
#define __MERO_CONF_GLOB_H__

#include "conf/cache.h"  /* M0_CONF_PATH_MAX */

/**
 * @defgroup conf_glob
 *
 * m0_conf_glob() traverses a DAG of M0_CS_READY configuration objects
 * and stores addresses of target objects, which given path leads to,
 * in a preallocated array.
 *
 * Example:
 * @code
 * static int
 * foreach_service(const struct m0_conf_root *root,
 *                 void (*process)(const struct m0_conf_service *svc))
 * {
 *         struct m0_conf_glob       glob;
 *         const struct m0_conf_obj *objv[BATCH];
 *         int                       rc;
 *
 *         M0_PRE(m0_conf_cache_is_locked(root->rt_obj.co_cache));
 *
 *         m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, NULL, &root->rt_obj,
 *                           //
 *                           // "nodes/@/processes/@/services/@"
 *                           // (mentally substitute '@' with '*')
 *                           //
 *                           M0_CONF_ROOT_NODES_FID, M0_CONF_ANY_FID,
 *                           M0_CONF_NODE_PROCESSES_FID, M0_CONF_ANY_FID,
 *                           M0_CONF_PROCESS_SERVICES_FID, M0_CONF_ANY_FID);
 *         while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objv), objv)) > 0) {
 *                 int i;
 *                 for (i = 0; i < rc; ++i)
 *                         process(M0_CONF_CAST(objv[i], m0_conf_service));
 *         }
 *         return M0_RC(rc);
 * }
 * @endcode
 *
 * @{
 */

/** Bits set in the `flags' argument to m0_conf_glob_init(). */
enum {
	/** Return on conf DAG traversal errors. */
	M0_CONF_GLOB_ERR = 1 << 0,
};

typedef int (*m0_conf_glob_errfunc_t)(int errcode,
				      const struct m0_conf_obj *obj,
				      const struct m0_fid *path);

/**
 * Conf DAG traversal context.
 *
 * Prepared by m0_conf_glob_init(), used and modified by m0_conf_glob() calls.
 */
struct m0_conf_glob {
	/** Bitwise OR of zero or more M0_CONF_GLOB_* symbolic constants. */
	int                         cg_flags;

	/**
	 * If set, this function will be called in case of traversal error.
	 *
	 * If .cg_errfunc() returns nonzero, or M0_CONF_GLOB_ERR is set
	 * in .cg_flags, m0_conf_glob() will terminate after the call
	 * to .cg_errfunc().
	 *
	 * A traversal error occurs when the path cannot be went through.
	 * This may happen if some path component refers to nonexistent
	 * object (-ENOENT), or a stub object is reached (-EPERM).
	 *
	 * @see m0_conf_glob_error()
	 */
	m0_conf_glob_errfunc_t      cg_errfunc;

	int                         cg_errcode;
	const struct m0_conf_obj   *cg_errobj;
	const struct m0_fid        *cg_errpath;

	/* XXX TODO: Drop this field and use .cg_origin->co_cache. */
	const struct m0_conf_cache *cg_cache;

	/** Configuration object to start traversal from. */
	const struct m0_conf_obj   *cg_origin;

	/**
	 * Path to target object(s).
	 *
	 * Relative to .cg_origin object.
	 * Terminated with M0_FID0.
	 */
	struct m0_fid               cg_path[M0_CONF_PATH_MAX + 1];

	/** Route from .cg_origin to where m0_conf_glob() currently is. */
	const struct m0_conf_obj   *cg_trace[M0_CONF_PATH_MAX + 1];

	/** Current position in .cg_path[] and .cg_trace[]. */
	uint32_t                    cg_depth;

	/** Whether next conf_glob_step() should move away from the root. */
	bool                        cg_down_p;
#ifdef DEBUG
	/** Whether to M0_LOG() steps of DAG traversal. */
	bool                        cg_debug_print;
#endif
};

/**
 * Initialises `glob'.
 *
 * If `origin' is NULL, the root object (m0_conf_root) is implied.
 * If `cache' is NULL, origin->co_cache is used.
 *
 * @pre  cache != NULL || origin != NULL
 * @pre  ergo(origin != NULL && cache != NULL, origin->co_cache == cache)
 * @pre  m0_conf_cache_is_locked(cache ?: origin->co_cache)
 */
#define m0_conf_glob_init(glob, flags, errfunc, cache, origin, ...)       \
	m0_conf__glob_init((glob), (flags), (errfunc), (cache), (origin), \
			   (const struct m0_fid []){ __VA_ARGS__, M0_FID0 })
M0_INTERNAL void m0_conf__glob_init(struct m0_conf_glob *glob,
				    int flags, m0_conf_glob_errfunc_t errfunc,
				    const struct m0_conf_cache *cache,
				    const struct m0_conf_obj *origin,
				    const struct m0_fid *path);

/**
 * Finds configuration objects that a path leads to.
 *
 * m0_conf_glob() traverses a DAG of M0_CS_READY conf objects and stores
 * addresses of path target objects in `objv' array.
 *
 * @param glob  Conf DAG traversal context.
 * @param nr    Output array capacity.
 * @param objv  Output array.
 *
 * @returns
 *   +N       The number of target objects stored in `objv'.
 *    0       DAG traversal has completed.
 *   -EPERM   Conf object is not ready.
 *   -ENOENT  Unreachable path.
 *
 * @pre   m0_conf_cache_is_locked(glob->cg_cache)
 * @post  ergo(retval > 0, retval <= nr)
 *
 * @see m0_conf_glob_error()
 */
M0_INTERNAL int m0_conf_glob(struct m0_conf_glob *glob,
			     uint32_t nr, const struct m0_conf_obj **objv);

/**
 * Returns string describing the most recent m0_conf_glob() error.
 * Puts error description into `buf'.
 *
 * @pre  glob->cg_errcode < 0
 * @pre  buf != NULL && buflen > 0
 */
M0_INTERNAL char *m0_conf_glob_error(const struct m0_conf_glob *glob,
				     char *buf, size_t buflen);

/** @} conf_glob */
#endif /* __MERO_CONF_GLOB_H__ */
