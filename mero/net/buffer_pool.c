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

#include "lib/misc.h"  /* m0_forall */
#include "lib/memory.h"/* M0_ALLOC_PTR */
#include "lib/errno.h" /* ENOMEM */
#include "lib/arith.h" /* M0_CNT_INC, M0_CNT_DEC */
#include "mero/magic.h"
#include "net/buffer_pool.h"
#include "net/net_internal.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"

/**
   @addtogroup net_buffer_pool Network Buffer Pool
   @{
 */

/** Descriptor for the tlist of buffers. */
M0_TL_DESCR_DEFINE(m0_net_pool, "net_buffer_pool", M0_INTERNAL,
		   struct m0_net_buffer, nb_lru, nb_magic,
		   M0_NET_BUFFER_LINK_MAGIC, M0_NET_BUFFER_HEAD_MAGIC);
M0_TL_DEFINE(m0_net_pool, M0_INTERNAL, struct m0_net_buffer);

static bool pool_colour_check(const struct m0_net_buffer_pool *pool);
static bool pool_lru_buffer_check(const struct m0_net_buffer_pool *pool);
static bool colour_is_valid(const struct m0_net_buffer_pool *pool,
			    uint32_t colour);

M0_INTERNAL bool m0_net_buffer_pool_invariant(const struct m0_net_buffer_pool
					      *pool)
{
	return _0C(pool != NULL) &&
		/* domain must be set and initialized */
		_0C(pool->nbp_ndom != NULL) &&
		_0C(pool->nbp_ndom->nd_xprt != NULL) &&
		/* must have the appropriate callback */
		_0C(pool->nbp_ops != NULL) &&
		_0C(m0_net_buffer_pool_is_locked(pool)) &&
		_0C(pool->nbp_free <= pool->nbp_buf_nr) &&
		_0C(pool->nbp_free ==
		    m0_net_pool_tlist_length(&pool->nbp_lru)) &&
		_0C(pool_colour_check(pool)) &&
		_0C(pool_lru_buffer_check(pool)) &&
		_0C((pool->nbp_colours_nr == 0) == (pool->nbp_colours == NULL));
}

static bool pool_colour_check(const struct m0_net_buffer_pool *pool)
{
	return m0_forall(i, pool->nbp_colours_nr,
			 m0_tl_forall(m0_net_tm, nb, &pool->nbp_colours[i],
				      m0_net_pool_tlink_is_in(nb)));
}

static bool pool_lru_buffer_check(const struct m0_net_buffer_pool *pool)
{
	return m0_tl_forall(m0_net_pool, nb, &pool->nbp_lru,
			    !(nb->nb_flags & M0_NET_BUF_QUEUED) &&
			    (nb->nb_flags & M0_NET_BUF_REGISTERED));
}

M0_INTERNAL int m0_net_buffer_pool_init(struct m0_net_buffer_pool *pool,
					struct m0_net_domain *ndom,
					uint32_t threshold, uint32_t seg_nr,
					m0_bcount_t seg_size, uint32_t colours,
					unsigned shift, bool dont_dump)
{
	int i;

	M0_PRE(pool != NULL);
	M0_PRE(ndom != NULL);
	M0_PRE(seg_nr   <= m0_net_domain_get_max_buffer_segments(ndom));
	M0_PRE(seg_size <= m0_net_domain_get_max_buffer_segment_size(ndom));

	pool->nbp_threshold  = threshold;
	pool->nbp_ndom       = ndom;
	pool->nbp_free       = 0;
	pool->nbp_buf_nr     = 0;
	pool->nbp_seg_nr     = seg_nr;
	pool->nbp_seg_size   = seg_size;
	pool->nbp_colours_nr = colours;
	pool->nbp_align      = shift;
	pool->nbp_dont_dump  = dont_dump;

	if (colours == 0)
		pool->nbp_colours = NULL;
	else {
		M0_ALLOC_ARR(pool->nbp_colours, colours);
		if (pool->nbp_colours == NULL)
			return M0_ERR(-ENOMEM);
	}
	m0_mutex_init(&pool->nbp_mutex);
	m0_net_pool_tlist_init(&pool->nbp_lru);
	for (i = 0; i < colours; ++i)
		m0_net_tm_tlist_init(&pool->nbp_colours[i]);
	return 0;
}

/**
   Adds a buffer to the pool to increase the capacity.
   @pre m0_net_buffer_pool_is_locked(pool)
 */
static bool net_buffer_pool_grow(struct m0_net_buffer_pool *pool);


M0_INTERNAL int m0_net_buffer_pool_provision(struct m0_net_buffer_pool *pool,
					     uint32_t buf_nr)
{
	int buffers = 0;
	M0_PRE(m0_net_buffer_pool_invariant(pool));

	while (buf_nr--) {
		if (!net_buffer_pool_grow(pool))
			return buffers;
		buffers++;
	}
	M0_POST(m0_net_buffer_pool_invariant(pool));
	return buffers;
}

/** It removes the given buffer from the pool */
static void buffer_remove(struct m0_net_buffer_pool *pool,
			  struct m0_net_buffer *nb)
{
	m0_net_pool_tlink_del_fini(nb);
	m0_net_tm_tlist_remove(nb);
	m0_net_tm_tlink_fini(nb);
	m0_net_buffer_deregister(nb, pool->nbp_ndom);
	m0_bufvec_free_aligned_packed(&nb->nb_buffer, pool->nbp_align);
	m0_free(nb);
	M0_CNT_DEC(pool->nbp_buf_nr);
	M0_POST(m0_net_buffer_pool_invariant(pool));
}

M0_INTERNAL void m0_net_buffer_pool_fini(struct m0_net_buffer_pool *pool)
{
	int		      i;
	struct m0_net_buffer *nb;

	M0_PRE(m0_net_buffer_pool_is_not_locked(pool));

	if (pool->nbp_colours == NULL && pool->nbp_colours_nr != 0)
		return;
	/*
	 * The lock here is only needed to keep m0_net_buffer_pool_invariant()
	 * happy. The caller must guarantee that there is no concurrency at this
	 * point.
	 */
	m0_net_buffer_pool_lock(pool);
	M0_ASSERT(m0_net_buffer_pool_invariant(pool));

	M0_ASSERT(pool->nbp_free == pool->nbp_buf_nr);

	m0_tl_for(m0_net_pool, &pool->nbp_lru, nb) {
		M0_CNT_DEC(pool->nbp_free);
		buffer_remove(pool, nb);
	} m0_tl_endfor;
	m0_net_buffer_pool_unlock(pool);
	m0_net_pool_tlist_fini(&pool->nbp_lru);
	for (i = 0; i < pool->nbp_colours_nr; i++)
		m0_net_tm_tlist_fini(&pool->nbp_colours[i]);
	if (pool->nbp_colours != NULL)
		m0_free(pool->nbp_colours);
	m0_mutex_fini(&pool->nbp_mutex);
}

M0_INTERNAL void m0_net_buffer_pool_lock(struct m0_net_buffer_pool *pool)
{
	m0_mutex_lock(&pool->nbp_mutex);
}

M0_INTERNAL bool m0_net_buffer_pool_is_locked(const struct m0_net_buffer_pool
					      *pool)
{
	return m0_mutex_is_locked(&pool->nbp_mutex);
}

M0_INTERNAL bool m0_net_buffer_pool_is_not_locked(const struct
						  m0_net_buffer_pool *pool)
{
	return m0_mutex_is_not_locked(&pool->nbp_mutex);
}

M0_INTERNAL void m0_net_buffer_pool_unlock(struct m0_net_buffer_pool *pool)
{
	m0_mutex_unlock(&pool->nbp_mutex);
}

static bool colour_is_valid(const struct m0_net_buffer_pool *pool,
			    uint32_t colour)
{
	return colour == M0_BUFFER_ANY_COLOUR || colour < pool->nbp_colours_nr;
}

M0_INTERNAL struct m0_net_buffer *
m0_net_buffer_pool_get(struct m0_net_buffer_pool *pool, uint32_t colour)
{
	struct m0_net_buffer *nb;

	M0_ENTRY();
	M0_PRE_EX(m0_net_buffer_pool_invariant(pool));
	M0_PRE(colour_is_valid(pool, colour));

	if (pool->nbp_free <= 0)
		return NULL;
	if (colour != M0_BUFFER_ANY_COLOUR &&
	    !m0_net_tm_tlist_is_empty(&pool->nbp_colours[colour]))
		nb = m0_net_tm_tlist_head(&pool->nbp_colours[colour]);
	else
		nb = m0_net_pool_tlist_head(&pool->nbp_lru);
	M0_ASSERT(nb != NULL);
	m0_net_pool_tlist_del(nb);
	m0_net_tm_tlist_remove(nb);
	M0_CNT_DEC(pool->nbp_free);
	if (pool->nbp_free < pool->nbp_threshold)
		pool->nbp_ops->nbpo_below_threshold(pool);
	nb->nb_pool = pool;
	M0_POST_EX(m0_net_buffer_pool_invariant(pool));
	M0_POST(nb->nb_ep == NULL);
	M0_LEAVE();
	return nb;
}

M0_INTERNAL void m0_net_buffer_pool_put(struct m0_net_buffer_pool *pool,
					struct m0_net_buffer *buf,
					uint32_t colour)
{
	M0_PRE(buf != NULL);
	M0_PRE_EX(m0_net_buffer_pool_invariant(pool));
	M0_PRE(buf->nb_ep == NULL);
	M0_PRE(colour_is_valid(pool, colour));
	M0_PRE(!(buf->nb_flags & M0_NET_BUF_QUEUED));
	M0_PRE(buf->nb_flags & M0_NET_BUF_REGISTERED);
	M0_PRE(pool->nbp_ndom == buf->nb_dom);

	M0_ENTRY();
	M0_ASSERT(buf->nb_magic == M0_NET_BUFFER_LINK_MAGIC);
	M0_ASSERT(!m0_net_pool_tlink_is_in(buf));
	if (colour != M0_BUFFER_ANY_COLOUR) {
		M0_ASSERT(!m0_net_tm_tlink_is_in(buf));
		m0_net_tm_tlist_add(&pool->nbp_colours[colour], buf);
	}
	m0_net_pool_tlist_add_tail(&pool->nbp_lru, buf);
	M0_CNT_INC(pool->nbp_free);
	if (pool->nbp_free == 1)
		pool->nbp_ops->nbpo_not_empty(pool);
	M0_POST_EX(m0_net_buffer_pool_invariant(pool));
	M0_LEAVE();
}

static bool net_buffer_pool_grow(struct m0_net_buffer_pool *pool)
{
	int		      rc;
	struct m0_net_buffer *nb;

	M0_PRE(m0_net_buffer_pool_invariant(pool));

	M0_ALLOC_PTR(nb);
	if (nb == NULL)
		return false;
	rc = m0_bufvec_alloc_aligned_packed(&nb->nb_buffer, pool->nbp_seg_nr,
					    pool->nbp_seg_size, pool->nbp_align);
	if (rc != 0)
		goto clean;
	if(pool->nbp_align != 0 && pool->nbp_dont_dump) {
		rc = m0__bufvec_dont_dump(&nb->nb_buffer);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "failed to mark bufvec %p dont_dump",
					 &nb->nb_buffer);
			goto clean;
		}
	}

	rc = m0_net_buffer_register(nb, pool->nbp_ndom);
	if (rc != 0)
		goto clean;
	m0_net_pool_tlink_init(nb);
	m0_net_tm_tlink_init(nb);

	M0_CNT_INC(pool->nbp_buf_nr);
	m0_net_buffer_pool_put(pool, nb, M0_BUFFER_ANY_COLOUR);
	M0_POST(m0_net_buffer_pool_invariant(pool));
	return true;
clean:
	M0_ASSERT(rc != 0);
	m0_bufvec_free_aligned_packed(&nb->nb_buffer, pool->nbp_align);
	m0_free(nb);
	return false;
}

M0_INTERNAL bool m0_net_buffer_pool_prune(struct m0_net_buffer_pool *pool)
{
	struct m0_net_buffer *nb;

	M0_PRE(m0_net_buffer_pool_invariant(pool));

	if (pool->nbp_free <= pool->nbp_threshold)
		return false;
	M0_CNT_DEC(pool->nbp_free);
	nb = m0_net_pool_tlist_head(&pool->nbp_lru);
	M0_ASSERT(nb != NULL);
	buffer_remove(pool, nb);
	return true;
}

#undef M0_TRACE_SUBSYSTEM

/** @} */ /* end of net_buffer_pool */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
