/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 11-Nov-2012
 */

#pragma once

#ifndef __MERO_STOB_CACHE_H__
#define __MERO_STOB_CACHE_H__

#include "lib/mutex.h"	/* m0_mutex */
#include "lib/tlist.h"	/* m0_tl */
#include "lib/types.h"	/* uint64_t */
#include "fid/fid.h"    /* m0_fid */

/**
 * @defgroup stob Storage object
 *
 * @todo more scalable object index instead of a list.
 *
 * @{
 */

struct m0_stob;
struct m0_stob_cache;

typedef void (*m0_stob_cache_eviction_cb_t)(struct m0_stob_cache *cache,
					    struct m0_stob *stob);
/**
 * @todo document
 */
struct m0_stob_cache {
	struct m0_mutex             sc_lock;
	struct m0_tl		    sc_busy;
	struct m0_tl		    sc_idle;
	uint64_t		    sc_idle_size;
	uint64_t		    sc_idle_used;
	m0_stob_cache_eviction_cb_t sc_eviction_cb;

	uint64_t		    sc_busy_hits;
	uint64_t		    sc_idle_hits;
	uint64_t		    sc_misses;
	uint64_t		    sc_evictions;
};

/**
 * Initialises stob cache.
 *
 * @param cache stob cache
 * @param idle_size idle list maximum size
 */
M0_INTERNAL int m0_stob_cache_init(struct m0_stob_cache *cache,
				   uint64_t idle_size,
				   m0_stob_cache_eviction_cb_t eviction_cb);
M0_INTERNAL void m0_stob_cache_fini(struct m0_stob_cache *cache);

/**
 * Stob cache invariant.
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL bool m0_stob_cache__invariant(const struct m0_stob_cache *cache);

/**
 * Adds stob to the stob cache. Stob should be deleted from the stob cache using
 * m0_stob_cache_idle().
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL void m0_stob_cache_add(struct m0_stob_cache *cache,
				   struct m0_stob *stob);

/**
 * Deletes item from the stob cache.
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL void m0_stob_cache_idle(struct m0_stob_cache *cache,
				   struct m0_stob *stob);

/**
 * Finds item in the stob cache. Stob found should be deleted from the stob
 * cache using m0_stob_cache_idle().
 *
 * @pre m0_stob_cache_is_locked(cache)
 * @post m0_stob_cache_is_locked(cache)
 */
M0_INTERNAL struct m0_stob *m0_stob_cache_lookup(struct m0_stob_cache *cache,
						 const struct m0_fid *stob_fid);

/**
 * Purges at most nr items from the idle stob cache.
 *
 * @pre m0_stob_cache_is_not_locked(cache)
 * @post m0_stob_cache_is_not_locked(cache)
 */
M0_INTERNAL void m0_stob_cache_purge(struct m0_stob_cache *cache, int nr);

M0_INTERNAL void m0_stob_cache_lock(struct m0_stob_cache *cache);
M0_INTERNAL void m0_stob_cache_unlock(struct m0_stob_cache *cache);
M0_INTERNAL bool m0_stob_cache_is_locked(const struct m0_stob_cache *cache);
M0_INTERNAL bool m0_stob_cache_is_not_locked(const struct m0_stob_cache *cache);

M0_INTERNAL void m0_stob_cache__print(struct m0_stob_cache *cache);


#endif /* __MERO_STOB_CACHE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
