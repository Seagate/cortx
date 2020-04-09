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
 * Original author: Madhavrao Vemuri <madhav_vemuri@xyratex.com>
 * Original creation date: 10/12/2011
 */

#pragma once

#ifndef __MERO_BUFFER_POOL_H__
#define __MERO_BUFFER_POOL_H__

#include "lib/types.h" /* uint64_t */
#include "lib/mutex.h"
#include "net/net.h"   /* m0_net_buffer, m0_net_domain */
#include "lib/tlist.h"

/**
   @defgroup net_buffer_pool Network Buffer Pool
   @ingroup net

   @brief Network buffer pool allocates and manages a pool of network buffers.
	  Users request a buffer from the pool and after its usage is over
	  gives back to the pool.

	  It provides support for a pool of network buffers involving no higher
	  level interfaces than the network module itself.
	  It is associated with a single network domain.
	  Non-blocking interfaces are available to get and put network buffers.
	  Call-backs are provided to announce when the pool is non-empty or
	  low on free buffers.

	  Upon receiving the not_empty call back user can put back buffers which
	  are not in use into the pool.

	  The "coloured" variant of the get operation is done by returning the
	  most recently used buffer that is associated with a specific colour
	  (transfer machine), or if none such are found, the least recently
	  used buffer from the pool, if any.

	  Pool is protected by a lock, to get or put a buffer into the pool user
	  must acquire the lock and release the lock once its usage is over.

	  To finalize the pool all the buffers must be returned back to the pool
	  (i.e number of free buffers must be equal to the total number of
	   buffers).

 To describe a typical buffer pool usage pattern, suppose that one wants
 a buffer pool of 10, size of each segment is 1024, number of segments is
 64 and threshold is 10.

    First, user needs to provide m0_net_buffer_pool_ops:
    @code
	struct m0_net_buffer_pool_ops b_ops = {
		.nbpo_not_empty       = notempty,
		.nbpo_below_threshold = low,
	};
    @endcode

   - Then, buffer pool needs to be assigned to a network domain and initialized
	with above values:
    @code
	struct m0_net_buffer_pool bp;
	struct m0_net_xprt *xprt;
	...
	bp.nbp_ops = &b_ops;
	rc = m0_net_buffer_pool_init(&bp, bp.nbp_ndom, 10, 64, 4096, 10, ...);
	...
    @endcode

   - Now, to add buffers into the pool need to acquire the lock and then specify
	the number of buffers to be added:
    @code
	m0_net_buffer_pool_lock(&bp);
	m0_net_buffer_pool_provision(&bp, 10);
	m0_net_buffer_pool_unlock(&bp);
    @endcode

    - To add a buffer in the pool:
    @code
	m0_net_buffer_pool_lock(&bp);
	m0_net_buffer_pool_provision(&bp, 1);
	m0_net_buffer_pool_unlock(&bp);
    @endcode

    - To get a buffer from the pool:
	To use any colour for the buffer variable colour should be
	M0_BUFFER_ANY_COLOUR.
    @code
	m0_net_buffer_pool_lock(&bp);
	nb = m0_net_buffer_pool_get(&bp, colour);
	if (nb != NULL)
		"Use the buffer"
	else
		"goto sleep until buffer is available"
	m0_net_buffer_pool_unlock(&bp);
    @endcode

   - To put back the buffer in the pool:
	To use any colour for the buffer variable colour should be
	M0_BUFFER_ANY_COLOUR.
    @code
	m0_net_buffer_pool_lock(&bp);
	m0_net_buffer_pool_put(&bp, nb, colour);
	m0_net_buffer_pool_unlock(&bp);
    @endcode

    - To remove a buffer from the pool:
    @code
	m0_net_buffer_pool_lock(&bp);
	m0_net_buffer_pool_prune(&bp);
	m0_net_buffer_pool_unlock(&bp);
    @endcode

    - To finalize the pool:
    @code
	m0_net_buffer_pool_fini(&bp);
    @endcode

    @see Also see m0_net_tm_pool_attach() and @ref NetRQProvDLD
    "Auo-Provisioning of Receive Message Queue Buffers".
   @{
  */

enum {
	M0_BUFFER_ANY_COLOUR	     = ~0,
	M0_NET_BUFFER_POOL_THRESHOLD = 2,
};

struct m0_net_buffer_pool;

/** Call backs that buffer pool can trigger on different memory conditions. */
struct m0_net_buffer_pool_ops {
	/** Buffer pool is not empty. */
	void (*nbpo_not_empty)(struct m0_net_buffer_pool *);
	/** Buffers in pool are lower than threshold. */
	void (*nbpo_below_threshold)(struct m0_net_buffer_pool *);
};

/** Checks the buffer pool. */
M0_INTERNAL bool m0_net_buffer_pool_invariant(const struct m0_net_buffer_pool
					      *pool);

/**
   Initializes fields of a buffer pool and tlist, which are used to populate
   the pool using m0_net_buffer_pool_provision().
   @pre ndom != NULL
   @param pool      Pool to initialize.
   @param ndom      Network domain to associate with the pool.
   @param threshold Number of buffer below which to notify the user.
   @param seg_nr    Number of segments in each buffer.
   @param colours   Number of colours in the pool.
   @param seg_size  Size of each segment in a buffer.
   @param shift	    Alignment needed for network buffers.
   @pre seg_nr   <= m0_net_domain_get_max_buffer_segments(ndom) &&
	seg_size <= m0_net_domain_get_max_buffer_segment_size(ndom)
 */
M0_INTERNAL int m0_net_buffer_pool_init(struct m0_net_buffer_pool *pool,
					struct m0_net_domain *ndom,
					uint32_t threshold, uint32_t seg_nr,
					m0_bcount_t seg_size, uint32_t colours,
					unsigned shift, bool dont_dump);

/**
   It adds the buf_nr buffers in the buffer pool.
   Suppose to add 10 items to the pool, m0_net_buffer_pool_provision(pool, 10)
   can be used.
   @pre m0_net_buffer_pool_is_locked(pool)
   @pre seg_size > 0 && seg_nr > 0 && buf_nr > 0
   @pre pool->nbp_ndom != NULL
   @param pool   Pool to provision.
   @param buf_nr Number of buffers to be added in the pool.
   @return result number of buffers it managed to allocate.
*/
M0_INTERNAL int m0_net_buffer_pool_provision(struct m0_net_buffer_pool *pool,
					     uint32_t buf_nr);
/** Finalizes a buffer pool.
   @pre m0_net_buffer_pool_is_not_locked(pool)
 */
M0_INTERNAL void m0_net_buffer_pool_fini(struct m0_net_buffer_pool *pool);

/** Acquires the lock on buffer pool. */
M0_INTERNAL void m0_net_buffer_pool_lock(struct m0_net_buffer_pool *pool);

/** Check whether buffer pool is locked or not. */
M0_INTERNAL bool m0_net_buffer_pool_is_locked(const struct m0_net_buffer_pool
					      *pool);

/** Returns true when buffer pool is not locked. */
M0_INTERNAL bool m0_net_buffer_pool_is_not_locked(const struct
						  m0_net_buffer_pool *pool);

/** Releases the lock on buffer pool. */
M0_INTERNAL void m0_net_buffer_pool_unlock(struct m0_net_buffer_pool *pool);

/**
   Gets a buffer from the pool.
   If the colour is specified (i.e non zero) and the corresponding coloured
   list is not empty then the buffer is taken from the head of this list.
   Otherwise the buffer is taken from the head of the per buffer pool list.
   @pre m0_net_buffer_pool_is_locked(pool)
   @pre colour == M0_BUFFER_ANY_COLOUR || colour < pool->nbp_colours_nr
   @post ergo(result != NULL, result->nb_flags & M0_NET_BUF_REGISTERED)
   @post ergo(result != NULL, result->nb_pool == pool)
 */
M0_INTERNAL struct m0_net_buffer *
m0_net_buffer_pool_get(struct m0_net_buffer_pool *pool, uint32_t colour);

/**
   Puts the buffer back to the pool.
   If the colour is specfied then the buffer is put at the head of corresponding
   coloured list and also put at the tail of the global list.
   @pre m0_net_buffer_pool_is_locked(pool)
   @pre colour == M0_BUFFER_ANY_COLOUR || colour < pool->nbp_colours_nr
   @pre pool->nbp_ndom == buf->nb_dom
   @pre (buf->nb_flags & M0_NET_BUF_REGISTERED) &&
        !(buf->nb_flags & M0_NET_BUF_QUEUED)
 */
M0_INTERNAL void m0_net_buffer_pool_put(struct m0_net_buffer_pool *pool,
					struct m0_net_buffer *buf,
					uint32_t colour);

/**
   Removes a buffer from the pool to prune it.
   @pre m0_net_buffer_pool_is_locked(pool)
 */
M0_INTERNAL bool m0_net_buffer_pool_prune(struct m0_net_buffer_pool *pool);

/** Buffer pool. */
struct m0_net_buffer_pool {
	/** Number of free buffers in the pool. */
	uint32_t			     nbp_free;
	/** Number of buffer below which low memory condition occurs. */
	uint32_t			     nbp_threshold;
	/** Number of segments in each buffer of the pool. */
	uint32_t			     nbp_seg_nr;
	/** Number of buffers in the pool. */
	uint32_t			     nbp_buf_nr;
	/** Size of buffer segment of the pool. */
	m0_bcount_t			     nbp_seg_size;
	/** Buffer pool lock to protect and synchronize network buffer list.
	    It needs to acquired to do any changes to the pool
	 */
	struct m0_mutex			     nbp_mutex;
	/** Network domain to register the buffers. */
	struct m0_net_domain		    *nbp_ndom;
	/** Call back operations can be triggered by buffer pool. */
	const struct m0_net_buffer_pool_ops *nbp_ops;
	/** Number of colours in the pool. */
	uint32_t			     nbp_colours_nr;
	/** An array of nbp_colours_nr lists of buffers.
	    Each list in the array contains buffers of a particular
	    colour. Lists are maintained in LIFO order (i.e., they are stacks)
	    to improve temporal locality of reference.
	    Buffers are linked through m0_net_buffer::nb_tm_linkage to these
	    lists.
	*/
	struct m0_tl			    *nbp_colours;
	/** Alignment for network buffers */
	unsigned			     nbp_align;
	/** Memory in this pool is excluded in core dump or not */
	bool                                 nbp_dont_dump;
	/**
	   A list of all buffers in the pool.
	   This list is maintained in LRU order. The head of this list (which is
	   the buffer used longest time ago) is used when coloured array is
	   empty.
	   Buffers are linked through m0_net_buffer::nb_lru to this list.
	 */
	struct m0_tl			     nbp_lru;
};

/** @} */ /* end of net_buffer_pool */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
