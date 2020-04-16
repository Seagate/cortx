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
 * Original creation date: 28-Jun-2016
 */
#pragma once
#ifndef __MERO_CONF_WALK_H__
#define __MERO_CONF_WALK_H__

struct m0_conf_obj;

/**
 * @defgroup conf_walk
 *
 * @{
 */

/**
 * fn() parameter of m0_conf_walk() should return one of these values.
 * In case of error, negative error code (-Exxx) should be returned.
 */
enum {
	/** Return immediately. */
	M0_CW_STOP,
	/** Continue normally. */
	M0_CW_CONTINUE,
	/**
	 * Skip the subtree that begins at the current entry.
	 * Continue processing with the next sibling.
	 */
	M0_CW_SKIP_SUBTREE,
	/**
	 * Skip siblings of the current entry.
	 * Continue processing in the parent.
	 */
	M0_CW_SKIP_SIBLINGS
};

/**
 * Performs depth-first traversal of the tree of conf objects,
 * starting from `origin', and calls fn() once for each conf object
 * in the tree.
 *
 * fn() should return one of M0_CW_* values (see enum above), or -Exxx
 * in case of error.
 *
 * @pre  m0_conf_cache_is_locked(origin->co_cache)
 */
M0_INTERNAL int m0_conf_walk(int (*fn)(struct m0_conf_obj *obj, void *args),
			     struct m0_conf_obj *origin, void *args);

/** @} conf_walk */
#endif /* __MERO_CONF_WALK_H__ */
