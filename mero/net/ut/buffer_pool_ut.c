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

#include "ut/ut.h"
#include "lib/memory.h"/* M0_ALLOC_PTR */
#include "lib/misc.h"  /* M0_SET0 */
#include "lib/thread.h"/* M0_THREAD_INIT */
#include "lib/time.h"  /* m0_nanosleep */
#include "net/lnet/lnet.h"
#include "net/buffer_pool.h"
#include "net/net_internal.h"

static void notempty(struct m0_net_buffer_pool *bp);
static void low(struct m0_net_buffer_pool *bp);
static void buffers_get_put(int rc);

static struct m0_net_buffer_pool bp;
static struct m0_chan		 buf_chan;

static const struct m0_net_buffer_pool_ops b_ops = {
	.nbpo_not_empty	      = notempty,
	.nbpo_below_threshold = low,
};

/**
   Initialization of buffer pool.
 */
static void test_init(void)
{
	int         rc;
	uint32_t    seg_nr    = 64;
	m0_bcount_t seg_size  = 4096;
	uint32_t    colours   = 10;
	unsigned    shift     = 12;
	uint32_t    buf_nr    = 10;

	M0_ALLOC_PTR(bp.nbp_ndom);
	M0_UT_ASSERT(bp.nbp_ndom != NULL);
	rc = m0_net_domain_init(bp.nbp_ndom, &m0_net_lnet_xprt);
	M0_ASSERT(rc == 0);
	bp.nbp_ops = &b_ops;
	rc = m0_net_buffer_pool_init(&bp, bp.nbp_ndom,
				      M0_NET_BUFFER_POOL_THRESHOLD, seg_nr,
				      seg_size, colours, shift, false);
	M0_UT_ASSERT(rc == 0);
	m0_chan_init(&buf_chan, &bp.nbp_mutex);
	m0_net_buffer_pool_lock(&bp);
	rc = m0_net_buffer_pool_provision(&bp, buf_nr);
	m0_net_buffer_pool_unlock(&bp);
	M0_UT_ASSERT(rc == buf_nr);
}

static void test_get_put(void)
{
	struct m0_net_buffer *nb;
	uint32_t	      free = bp.nbp_free;
	m0_net_buffer_pool_lock(&bp);
	nb = m0_net_buffer_pool_get(&bp, M0_BUFFER_ANY_COLOUR);
	M0_UT_ASSERT(nb != NULL);
	M0_UT_ASSERT(--free == bp.nbp_free);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	m0_net_buffer_pool_put(&bp, nb, M0_BUFFER_ANY_COLOUR);
	M0_UT_ASSERT(++free == bp.nbp_free);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	m0_net_buffer_pool_unlock(&bp);
}

static void test_get_put_colour(void)
{
	struct m0_net_buffer *nb;
	uint32_t	      free = bp.nbp_free;
	enum {
		COLOUR = 1,
	};
	m0_net_buffer_pool_lock(&bp);
	nb = m0_net_buffer_pool_get(&bp, M0_BUFFER_ANY_COLOUR);
	M0_UT_ASSERT(nb != NULL);
	M0_UT_ASSERT(--free == bp.nbp_free);
	m0_net_buffer_pool_put(&bp, nb, COLOUR);
	M0_UT_ASSERT(++free == bp.nbp_free);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	nb = m0_net_buffer_pool_get(&bp, COLOUR);
	M0_UT_ASSERT(nb != NULL);
	M0_UT_ASSERT(--free == bp.nbp_free);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	m0_net_buffer_pool_put(&bp, nb, M0_BUFFER_ANY_COLOUR);
	M0_UT_ASSERT(++free == bp.nbp_free);
	m0_net_buffer_pool_unlock(&bp);
}

static void test_grow(void)
{
	uint32_t buf_nr = bp.nbp_buf_nr;
	m0_net_buffer_pool_lock(&bp);
	/* Buffer pool grow by one */
	M0_UT_ASSERT(m0_net_buffer_pool_provision(&bp, 1) == 1);
	M0_UT_ASSERT(++buf_nr == bp.nbp_buf_nr);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	m0_net_buffer_pool_unlock(&bp);
}

static void test_prune(void)
{
	uint32_t buf_nr = bp.nbp_buf_nr;
	m0_net_buffer_pool_lock(&bp);
	M0_UT_ASSERT(m0_net_buffer_pool_prune(&bp));
	M0_UT_ASSERT(--buf_nr == bp.nbp_buf_nr);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	m0_net_buffer_pool_unlock(&bp);
}

static void test_get_put_multiple(void)
{
	int		  i;
	int		  rc;
	const int	  nr_client_threads = 10;
	struct m0_thread *client_thread;

	M0_ALLOC_ARR(client_thread, nr_client_threads);
	M0_UT_ASSERT(client_thread != NULL);
	for (i = 0; i < nr_client_threads; i++) {
		M0_SET0(&client_thread[i]);
		rc = M0_THREAD_INIT(&client_thread[i], int,
				     NULL, &buffers_get_put,
				     M0_BUFFER_ANY_COLOUR, "client_%d", i);
		M0_ASSERT(rc == 0);
		M0_SET0(&client_thread[++i]);
		/* value of integer 'i' is used to put or get the
		   buffer in coloured list */
		rc = M0_THREAD_INIT(&client_thread[i], int,
				     NULL, &buffers_get_put,
					i, "client_%d", i);
		M0_ASSERT(rc == 0);
	}
	for (i = 0; i < nr_client_threads; i++) {
		m0_thread_join(&client_thread[i]);
	}
	m0_free(client_thread);
	m0_net_buffer_pool_lock(&bp);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	m0_net_buffer_pool_unlock(&bp);
}

static void test_fini(void)
{
	m0_net_buffer_pool_lock(&bp);
	M0_UT_ASSERT(m0_net_buffer_pool_invariant(&bp));
	m0_net_buffer_pool_unlock(&bp);
	m0_chan_fini_lock(&buf_chan);
	m0_net_buffer_pool_fini(&bp);
	m0_net_domain_fini(bp.nbp_ndom);
	m0_free(bp.nbp_ndom);
}

static void buffers_get_put(int rc)
{
	struct m0_net_buffer *nb;
	struct m0_clink buf_link;

	m0_clink_init(&buf_link, NULL);
	m0_clink_add_lock(&buf_chan, &buf_link);
	do {
		m0_net_buffer_pool_lock(&bp);
		nb = m0_net_buffer_pool_get(&bp, rc);
		m0_net_buffer_pool_unlock(&bp);
		if (nb == NULL)
			m0_chan_wait(&buf_link);
	} while (nb == NULL);
	m0_nanosleep(m0_time(0, 100), NULL);
	m0_net_buffer_pool_lock(&bp);
	if (nb != NULL)
		m0_net_buffer_pool_put(&bp, nb, rc);
	m0_net_buffer_pool_unlock(&bp);
	m0_clink_del_lock(&buf_link);
	m0_clink_fini(&buf_link);
}

static void notempty(struct m0_net_buffer_pool *bp)
{
	m0_chan_signal(&buf_chan);
}

static void low(struct m0_net_buffer_pool *bp)
{
	/* Buffer pool is LOW */
}

struct m0_ut_suite buffer_pool_ut = {
	.ts_name = "buffer_pool_ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "buffer_pool_init",              test_init },
		{ "buffer_pool_get_put",           test_get_put },
		{ "buffer_pool_get_put_colour",    test_get_put_colour },
		{ "buffer_pool_grow",              test_grow },
		{ "buffer_pool_prune",             test_prune },
		{ "buffer_pool_get_put_multiple",  test_get_put_multiple },
		{ "buffer_pool_fini",              test_fini },
		{ NULL,                            NULL }
	}
};
M0_EXPORTED(buffer_pool_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
