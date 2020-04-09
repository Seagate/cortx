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
 * Original author: Mandar Sawant <mandar_sawant@seagate.com>
 * Authors:         Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 12-Jan-2015
 */
#pragma once
#ifndef __MERO_CONF_DITER_H__
#define __MERO_CONF_DITER_H__

#include "conf/confc.h"

/**
 * @page confditer-fspec Configuration directory iterator
 *
 * @note m0_conf_glob() is more convenient to use in situations when
 *       configuration cache is fully loaded.
 *
 * - @ref confditer-fspec-ds
 * - @ref confditer-fspec-sub
 *   - @ref confditer-fspec-sub-setup
 *   - @ref confditer-fspec-sub-use
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-fspec-ds Data Structures
 *
 * - m0_conf_diter --- an instance of configuration directory iterator.
 *   This structure maintains multiple levels of a configuration directory path
 *   and data needed for its traversal. Each directory level is represented by
 *   m0_conf_diter_lvl.
 *   m0_conf_diter traverses the configuration directory tree in depth first
 *   order.
 *
 * - m0_conf_diter_lvl --- an instance of configuration directory level.
 *   This structure represents a configuration directory level in the iterator.
 *   It consists of a configuration directory relation fid specifying the
 *   path to the next level directory to be accessed from the configurations
 *   object's ambient data structure.
 *   Each m0_conf_diter_lvl contains 2 objects of m0_confc_ctx corresponding
 *   corresponding to m0_confc_open() and m0_confc_readdir() operations, which
 *   are re-used for subsequent iterations for that directory level.
 *
 * - m0_conf_diter_lvl_ctx --- directory iterator level context.
 *   This structure comprises of struct m0_confc_ctx used for asynchronous
 *   configuration operations, viz. m0_confc_open(), m0_confc_readdir().
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-fspec-sub Subroutines
 *
 * - m0_conf_diter_init() initialises configuration directory iterator, invokes
 *   m0_conf__diter_init() with the given configuration directory path.
 *   Initialises directory iterator levels according to the directory traversal
 *   path.
 *
 * - m0_conf_diter_fini() finalises configuration directory iterator and all its
 *   levels.
 *
 * - m0_conf_diter_next() traverses directory path asynchronously.
 * - m0_conf_diter_next_sync() traverses directory path synchronously.
 *   Internally invokes m0_conf_diter_next() and waits on iterator
 *   channel.
 *
 * - m0_conf_diter_wait_arm() adds clink to iterator channel
 *   (m0_conf_diter::di_wait).
 *
 * - m0_conf_diter_result() is used to obtain result of the iteration.
 *
 * <hr> <!------------------------------------------------------------>
 * @subsection confditer-fspec-sub-setup Initialisation and termination
 *
 * Prior to initialising the configuration directory iterator, confc instance
 * must be initialised and the origin configuration object must be
 * m0_confc_open()ed.
 *
 * Configuration directory iterator uses confc's state machine group for
 * asynchronous operations, viz. m0_confc_open() and m0_confc_readdir().
 *
 * @code
 * struct m0_conf_diter  it;
 * struct m0_conf_obj   *obj;
 * int                   rc;
 *
 * rc = m0_conf_diter_init(&it, confc, root,
 *                         M0_CONF_ROOT_NODES_FID,
 *                         M0_CONF_NODE_PROCESSES_FID,
 *                         M0_CONF_PROCESS_SERVICES_FID,
 *                         M0_CONF_SERVICE_SDEVS_FID);
 *
 * ... Access configuration objects usinfg iterator interfaces. ...
 *
 * m0_conf_diter_fini(&it);
 * @endcode
 *
 * <hr> <!------------------------------------------------------------>
 * @subsection confditer-fspec-sub-use Accessing directory objects.
 *
 * m0_conf_diter_next() is an asynchronous function.
 * Prior to invoking it, the application must invoke m0_conf_diter_wait_arm()
 * and register a clink to receive the completion event.
 * Result of the iteration must be accessed using m0_conf_diter_result().
 *
 * Callers of m0_conf_diter_next_sync() will be blocked until the iterator
 * fetches next configuration object in the given directory path.
 * A typical use case of the configuration directory iterator using
 * a synchronous version of this functions is as follows,
 * @code
 *	rc = m0_conf_diter_init(&it, confc, &root->rt_obj,
 *				M0_CONF_XXX_XXX_FID,
 *				M0_CONF_XXX_XXX_FID);
 *	if (rc != 0)
 *		return M0_ERR(rc);
 *
 *	while ((rc = m0_conf_diter_next_sync(&it, _filter_xxx)) ==
 *							M0_CONF_DIRNEXT) {
 *		obj = m0_conf_diter_result(&it);
 *		M0_ASSERT(m0_conf_obj_type(obj) == &M0_CONF_XXX_TYPE);
 *		x = M0_CONF_CAST(obj, m0_conf_xxx);
 *	}
 *
 *	m0_conf_diter_fini(&it);
 * @endcode
 */

/**
 * @defgroup confditer Configuration directory iterator
 * @{
 */

enum m0_diter_lvl_mode {
	M0_DLM_DIR,
	M0_DLM_ENTRY,
	M0_DLM_NR
};

/**
 * Directory iterator level context.
 * Result of the operation is fetched from the struct m0_confc_ctx and saved
 * in struct m0_conf_diter_lvl_ctx::lc_result for later use.
 */
struct m0_conf_diter_lvl_ctx {
	/**
	 * Configuration context in which the configuration object is retrieved
	 * for the given directory level.
	 */
	struct m0_confc_ctx  lc_ctx;
	/**
	 * Result of the configuration operation obtained from
	 * ->lc_ctx.
	 */
	struct m0_conf_obj  *lc_result;
};

/** Represents configuration directory level. */
struct m0_conf_diter_lvl {
	struct m0_conf_diter         *dl_di;
	/** Configuration path of the directory to be accessed. */
	struct m0_fid                 dl_rel_fid;
	/**
	 * Contexts used for asynchronous configuration operation,
	 * viz. m0_confc_open() and m0_confc_readdir().
	 */
	struct m0_conf_diter_lvl_ctx  dl_cctx[M0_DLM_NR];
	/* See m0_diter_lvl_mode for values. */
	enum m0_diter_lvl_mode        dl_mode;
	/** Directory level. */
	uint32_t                      dl_lvl;
	/** Number of times this directory level is m0_confc_open()ed. */
	uint32_t                      dl_nr_open;
	/**
	 * Number of times this directory level is read using
	 * m0_confc_readdir().
	 */
	uint32_t                      dl_nr_read;
};

/**
 * Configuration directory iterator.
 *
 * Uses depth-first traversal.
 * @see m0_conf_diter_next()
 */
struct m0_conf_diter {
	struct m0_confc          *di_confc;
	/** Configuration object from which the iterator begins. */
	struct m0_conf_obj       *di_origin;
	/** Configuration directory levels on which the iterator iterates. */
	struct m0_conf_diter_lvl *di_lvls;
	/** Current phase of the iterator. */
	uint32_t                  di_phase;
	/** Link to receive configuration operation completion notification. */
	struct m0_clink           di_clink;
	/**
	 * Channel for external users to wait on for asynchronous operations.
	 * This is signalled once the operation completion event is received on
	 * @di_clink.
	 */
	struct m0_chan            di_wait;
	struct m0_mutex           di_wait_mutex;
	/* Total number of configuration directory levels to iterate. */
	uint32_t                  di_nr_lvls;
	/** Current level. */
	uint32_t                  di_lvl;
	/**
	 * Flag determines if diter is protected by locality lock or not.
	 * This field is workaround for a case when m0_conf_diter_next() or
	 * m0_conf_diter_fini() are called under locality lock which protects
	 * m0_confc_ctx::fc_mach.
	 *
	 * @see MERO-1307
	 */
	bool                      di_locked;
};

/**
 * Initialises configuration directory iterator with the given configuration
 * path.
 *
 * @pre  origin->co_status == M0_CS_READY
 */
#define m0_conf_diter_init(iter, confc, origin, ...)          \
	m0_conf__diter_init(iter, confc, origin,              \
			    M0_COUNT_PARAMS(__VA_ARGS__) + 1, \
			    (const struct m0_fid []){         \
			    __VA_ARGS__, M0_FID0 })

M0_INTERNAL int m0_conf__diter_init(struct m0_conf_diter *it,
				    struct m0_confc *confc,
				    struct m0_conf_obj *origin,
				    uint32_t nr_lvls,
				    const struct m0_fid *path);
M0_INTERNAL void m0_conf_diter_fini(struct m0_conf_diter *it);

/** @see m0_conf_diter::di_locked */
M0_INTERNAL void m0_conf_diter_locked_set(struct m0_conf_diter *it,
					  bool locked);

/**
 * Returns next struct m0_conf_obj in the given configuration path using the
 * given filter.
 * Result of an iteration must be accessed using m0_conf_diter_result().
 *
 * @retval M0_CONF_DIRNEXT @see m0_confc_readdir()
 *
 * @retval M0_CONF_DIRMISS @see m0_confc_readdir()
 *
 * @retval -ENODATA        There are no more directory objects to traverse.
 */
M0_INTERNAL int
m0_conf_diter_next(struct m0_conf_diter *it,
		   bool (*filter)(const struct m0_conf_obj *obj));

M0_INTERNAL int
m0_conf_diter_next_sync(struct m0_conf_diter *it,
			bool (*filter)(const struct m0_conf_obj *obj));

/** Registers given @clink with m0_conf_diter::di_wait channel. */
M0_INTERNAL void m0_conf_diter_wait_arm(struct m0_conf_diter *it,
                                        struct m0_clink *clink);

M0_INTERNAL struct m0_conf_obj *m0_conf_diter_result(const struct m0_conf_diter *it);

M0_INTERNAL void m0_conf_diter_lvl_init(struct m0_conf_diter_lvl *l,
					struct m0_conf_diter *it,
					struct m0_confc *confc, uint32_t lvl,
					const struct m0_fid *path);

M0_INTERNAL void m0_conf_diter_lvl_fini(struct m0_conf_diter_lvl *l);

/** @} endgroup confditer */
#endif /* __MERO_CONF_DITER_H__ */
