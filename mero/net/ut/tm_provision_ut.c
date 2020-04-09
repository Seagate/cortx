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
 * Original creation date: 19/3/2012
 */

/*
 * This file is depends on bulk_if.c and is included in it, so that bulk
 * interface dummy transport is reused to test auto provisioning of receive
 * message queue.
 */

#include "net/buffer_pool.h"

static int max_recv_msgs = 1;
static struct m0_net_buffer_pool *pool_prov;
enum {
	POOL_COLOURS   = 5,
	POOL_THRESHOLD = 2,
	POOL_BUF_NR    = M0_NET_TM_RECV_QUEUE_DEF_LEN * 4,
	MIN_RECV_SIZE  = 1 << 12,
};

static int ut_tm_prov_event_cb_calls = 0;
void ut_tm_prov_event_cb(const struct m0_net_tm_event *ev)
{
	struct m0_net_transfer_mc *tm;
	tm = ev->nte_tm;
	/*
	 * Check that provisioning is not happened when this cb is called and
	 * TM is in M0_NET_TM_STARTED state.
	 */
	if (tm->ntm_state == M0_NET_TM_STARTED)
		M0_UT_ASSERT(m0_net_tm_tlist_length(
			    &tm->ntm_q[M0_NET_QT_MSG_RECV]) == 0);
	M0_CNT_INC(ut_tm_prov_event_cb_calls);
}

/* UT transfer machine */
static struct m0_net_tm_callbacks ut_tm_prov_cb = {
	.ntc_event_cb = ut_tm_prov_event_cb
};

static void ut_prov_msg_recv_cb(const struct m0_net_buffer_event *ev)
{
	struct m0_net_transfer_mc *tm;
	struct m0_net_buffer	  *nb;

	M0_UT_ASSERT(ev != NULL && ev->nbe_buffer != NULL);
	nb = ev->nbe_buffer;
	tm = nb->nb_tm;
	M0_UT_ASSERT(tm->ntm_recv_pool != NULL && nb->nb_pool != NULL);

	m0_net_buffer_pool_lock(tm->ntm_recv_pool);
	if (nb->nb_tm->ntm_state == M0_NET_TM_STARTED)
		if (nb->nb_pool->nbp_free > 0)
			M0_UT_ASSERT(m0_atomic64_get(
				&tm->ntm_recv_queue_deficit) == 0);
	if (!(nb->nb_flags & M0_NET_BUF_QUEUED))
		m0_net_buffer_pool_put(nb->nb_pool, nb, tm->ntm_pool_colour);
	m0_net_buffer_pool_unlock(tm->ntm_recv_pool);
}

static const struct m0_net_buffer_callbacks ut_buf_prov_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]          = ut_prov_msg_recv_cb,
		[M0_NET_QT_MSG_SEND]          = ut_msg_send_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = ut_passive_bulk_recv_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = ut_passive_bulk_send_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = ut_active_bulk_recv_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = ut_active_bulk_send_cb
	},
};

static void ut_pool_low(struct m0_net_buffer_pool *bp)
{
	/* Buffer pool is below threshold.  */
}

static bool pool_not_empty_called = false;
static void ut_pool_not_empty(struct m0_net_buffer_pool *bp)
{
	m0_net_domain_buffer_pool_not_empty(bp);
	pool_not_empty_called = true;
}

static const struct m0_net_buffer_pool_ops b_ops = {
	.nbpo_not_empty	      = ut_pool_not_empty,
	.nbpo_below_threshold = ut_pool_low,
};

static bool ut_tm_prov_stop_called = false;
static int ut_tm_prov_stop(struct m0_net_transfer_mc *tm, bool cancel)
{
	int		      rc;
	struct m0_clink	      tmwait;
	struct m0_net_buffer *nb;
	struct m0_tl	     *ql;

	m0_clink_init(&tmwait, NULL);
	ql = &tm->ntm_q[M0_NET_QT_MSG_RECV];

	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	m0_tl_for(m0_net_tm, ql, nb) {
		ut_buf_del_called = false;
		m0_clink_add(&tm->ntm_chan, &tmwait);
		m0_mutex_unlock(&tm->ntm_mutex);
		m0_net_buffer_del(nb, tm);
		M0_UT_ASSERT(ut_buf_del_called);
		/* wait on channel for post (and consume UT thread) */
		m0_chan_wait(&tmwait);
		m0_mutex_lock(&tm->ntm_mutex);
		m0_clink_del(&tmwait);
		rc = m0_thread_join(&ut_del_thread);
		M0_UT_ASSERT(rc == 0);
	} m0_tl_endfor;

	ut_tm_prov_stop_called = true;
	rc = ut_tm_stop(tm, false);
	m0_clink_fini(&tmwait);
	return rc;
}

/*
 * It Checks that when a buffer is de-queued from receive message queue,
 * re-provision of the queue happens before the callback is called. In
 * call back buffers are returned to the pool if they are not queued.
 * Checked in ut_prov_msg_recv_cb after m0_net_buffer_del is called.
 * It adds a deleted buffer with TM's colour into the pool in corresponding
 * colour list.
 */
static struct m0_net_buffer *
pool_colour_buffer_add(struct m0_net_transfer_mc *tm)
{
	struct m0_net_buffer *nb;
	struct m0_clink	      tmwait;
	int		      rc;
	uint32_t	      prov_free;

	ut_buf_del_called = false;
	prov_free = pool_prov->nbp_free;
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm->ntm_pool_colour]) == 0);
	nb = m0_net_tm_tlist_head(&tm->ntm_q[M0_NET_QT_MSG_RECV]);
	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);
	m0_net_buffer_del(nb, tm);
	M0_UT_ASSERT(ut_buf_del_called);
	/* wait on channel for post (and consume UT thread) */
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	rc = m0_thread_join(&ut_del_thread);
	M0_UT_ASSERT(rc == 0);
	/* A buffer with colour of TM is put back into the pool. */
	M0_UT_ASSERT(pool_prov->nbp_free == prov_free);
	M0_UT_ASSERT(m0_net_tm_tlist_length(&tm->ntm_q[M0_NET_QT_MSG_RECV]) ==
		     tm->ntm_recv_queue_min_length);
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm->ntm_pool_colour]) == 1);
	m0_clink_fini(&tmwait);
	return nb;
}

static void provision_buffer_validate_colour(struct m0_net_buffer *nb,
					     struct m0_net_transfer_mc *tm)
{
	m0_net_tm_pool_length_set(tm, tm->ntm_recv_queue_min_length + 1);
	M0_UT_ASSERT(nb == m0_net_tm_tlist_tail(
		&tm->ntm_q[M0_NET_QT_MSG_RECV]));
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm->ntm_pool_colour]) == 0);
}

/* TM stop and fini */
static void ut_tm_stop_fini(struct m0_net_transfer_mc *tm)
{
	struct m0_clink tmwait;
	int		rc;
	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);
	rc = m0_net_tm_stop(tm, false);
	M0_UT_ASSERT(rc == 0);
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	m0_thread_join(&ut_tm_thread); /* cleanup thread */
	m0_thread_fini(&ut_tm_thread);
	m0_net_tm_fini(tm);
	m0_clink_fini(&tmwait);
}

static struct m0_net_transfer_mc ut_prov_tm1 = {
	.ntm_callbacks = &ut_tm_prov_cb,
	.ntm_state = M0_NET_TM_UNDEFINED
};

static struct m0_net_transfer_mc ut_prov_tm2 = {
	.ntm_callbacks = &ut_tm_prov_cb,
	.ntm_state = M0_NET_TM_UNDEFINED
};

static struct m0_net_domain ut_prov_dom;

static void test_net_tm_prov(void)
{
	int rc;
	struct m0_net_transfer_mc *tm1 = &ut_prov_tm1;
	struct m0_net_transfer_mc *tm2 = &ut_prov_tm2;
	struct m0_net_domain	  *dom = &ut_prov_dom;
	m0_bcount_t		   buf_size;
	m0_bcount_t		   buf_seg_size;
	uint32_t		   buf_segs;
	struct m0_clink		   tmwait;
	static uint32_t		   tm_colours = 0;
	struct m0_net_buffer	  *nb_tm1;
	struct m0_net_buffer	  *nb_tm2;
	uint32_t		   shift = 12;

	ut_xprt_ops.xo_tm_stop = ut_tm_prov_stop;

	/* initialize the domain */
	ut_dom_init_called = false;
	rc = m0_net_domain_init(dom, &ut_xprt);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_dom_init_called);
	M0_UT_ASSERT(dom->nd_xprt == &ut_xprt);
	M0_UT_ASSERT(dom->nd_xprt_private == &ut_xprt_pvt);
	M0_ASSERT(m0_mutex_is_not_locked(&dom->nd_mutex));

	M0_ALLOC_PTR(pool_prov);
	M0_UT_ASSERT(pool_prov != NULL);
	pool_prov->nbp_ops = &b_ops;

	/* get max buffer size */
	buf_size = m0_net_domain_get_max_buffer_size(dom);
	M0_UT_ASSERT(buf_size == UT_MAX_BUF_SIZE);

	/* get max buffer segment size */
	buf_seg_size = m0_net_domain_get_max_buffer_segment_size(dom);
	M0_UT_ASSERT(buf_seg_size == UT_MAX_BUF_SEGMENT_SIZE);

	/* get max buffer segments */
	buf_segs = m0_net_domain_get_max_buffer_segments(dom);
	M0_UT_ASSERT(buf_segs == UT_MAX_BUF_SEGMENTS);

	/* allocate buffers for testing */
	rc = m0_net_buffer_pool_init(pool_prov, dom, POOL_THRESHOLD, buf_segs,
				buf_seg_size, POOL_COLOURS, shift, false);
	m0_net_buffer_pool_lock(pool_prov);
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_buffer_pool_provision(pool_prov, POOL_BUF_NR);
	m0_net_buffer_pool_unlock(pool_prov);
	M0_UT_ASSERT(rc == POOL_BUF_NR);

	/* TM init with callbacks */
	rc = m0_net_tm_init(tm1, dom);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_init_called);
	M0_UT_ASSERT(tm1->ntm_state == M0_NET_TM_INITIALIZED);
	M0_UT_ASSERT(m0_list_contains(&dom->nd_tms, &tm1->ntm_dom_linkage));

	/* API Tests */
	m0_net_tm_colour_set(tm1, ++tm_colours);
	M0_UT_ASSERT(tm_colours < POOL_COLOURS);
	M0_UT_ASSERT(m0_net_tm_colour_get(tm1) == tm_colours);

	rc = m0_net_tm_pool_attach(tm1, pool_prov, &ut_buf_prov_cb,
				   MIN_RECV_SIZE, max_recv_msgs,
				   M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(tm1->ntm_recv_pool == pool_prov);
	M0_UT_ASSERT(tm1->ntm_recv_pool_callbacks == &ut_buf_prov_cb);
	M0_UT_ASSERT(tm1->ntm_recv_queue_min_recv_size == MIN_RECV_SIZE);
	M0_UT_ASSERT(tm1->ntm_recv_queue_max_recv_msgs == max_recv_msgs);
	M0_UT_ASSERT(tm1->ntm_recv_queue_min_length ==
		     M0_NET_TM_RECV_QUEUE_DEF_LEN);

	/* TM start */
	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&tm1->ntm_chan, &tmwait);
	ut_end_point_create_called = false;
	M0_UT_ASSERT(m0_net_tm_tlist_length(&tm1->ntm_q[M0_NET_QT_MSG_RECV]) ==
		     0);
	rc = m0_net_tm_start(tm1, "addr1");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_start_called);
	M0_UT_ASSERT(tm1->ntm_state == M0_NET_TM_STARTING ||
		     tm1->ntm_state == M0_NET_TM_STARTED);

	/* wait on channel until TM state changed to started */
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	M0_UT_ASSERT(ut_tm_prov_event_cb_calls == 1);
	M0_UT_ASSERT(tm1->ntm_state == M0_NET_TM_STARTED);
	M0_UT_ASSERT(ut_end_point_create_called);
	M0_UT_ASSERT(tm1->ntm_ep != NULL);
	M0_UT_ASSERT(m0_atomic64_get(&tm1->ntm_ep->nep_ref.ref_cnt) == 1);
	/*
	 * When TM starts initial provisioning happens before the channel is
	 * notified of the state change.
	 * Check for initial provisioning.
	 */
	M0_UT_ASSERT(m0_net_tm_tlist_length(&tm1->ntm_q[M0_NET_QT_MSG_RECV]) !=
		     0);
	M0_UT_ASSERT(m0_net_tm_tlist_length(&tm1->ntm_q[M0_NET_QT_MSG_RECV]) ==
		     tm1->ntm_recv_queue_min_length);
	M0_UT_ASSERT(pool_prov->nbp_free == pool_prov->nbp_buf_nr -
		     tm1->ntm_recv_queue_min_length);
	/* clean up; real xprt would handle this itself */
	m0_thread_join(&ut_tm_thread);
	m0_thread_fini(&ut_tm_thread);

	/*
	 * Check for provisioning when minimum buffers in the receive queue
	 * of tm is changed. Also re-provisioning happens synchronously with
	 * this call.
	 */
	m0_net_tm_pool_length_set(tm1, 4);
	M0_UT_ASSERT(tm1->ntm_recv_queue_min_length == 4);
	M0_UT_ASSERT(pool_prov->nbp_free == pool_prov->nbp_buf_nr -
		     tm1->ntm_recv_queue_min_length);
	M0_UT_ASSERT(m0_atomic64_get(&tm1->ntm_recv_queue_deficit) == 0);

	/* Check for deficit when required buffers are more than that of pool.*/
	m0_net_tm_pool_length_set(tm1, 10);
	M0_UT_ASSERT(tm1->ntm_recv_queue_min_length == 10);
	M0_UT_ASSERT(pool_prov->nbp_free == 0);
	M0_UT_ASSERT(m0_atomic64_get(&tm1->ntm_recv_queue_deficit) == 2);

	/* Check for provisioning when pool is replenished. */
	pool_not_empty_called = false;
	m0_net_buffer_pool_lock(pool_prov);
	rc = m0_net_buffer_pool_provision(pool_prov, 2);
	m0_net_buffer_pool_unlock(pool_prov);
	M0_UT_ASSERT(rc == 2);
	M0_UT_ASSERT(pool_not_empty_called);
	M0_UT_ASSERT(m0_atomic64_get(&tm1->ntm_recv_queue_deficit) == 0);
	M0_UT_ASSERT(tm1->ntm_recv_queue_min_length == 10);
	M0_UT_ASSERT(pool_prov->nbp_free == pool_prov->nbp_buf_nr -
		     tm1->ntm_recv_queue_min_length);

	/* Initialize another TM with different colour. */
	rc = m0_net_tm_init(tm2, dom);
	M0_UT_ASSERT(rc == 0);

	m0_net_tm_colour_set(tm2, ++tm_colours);
	M0_UT_ASSERT(tm_colours < POOL_COLOURS);
	M0_UT_ASSERT(m0_net_tm_colour_get(tm2) == tm_colours);
	max_recv_msgs = 2;
	rc = m0_net_tm_pool_attach(tm2, pool_prov, &ut_buf_prov_cb,
				   MIN_RECV_SIZE, max_recv_msgs,
				   M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_UT_ASSERT(tm2->ntm_recv_pool == pool_prov);
	M0_UT_ASSERT(tm2->ntm_recv_pool_callbacks == &ut_buf_prov_cb);
	M0_UT_ASSERT(tm2->ntm_recv_queue_min_recv_size == MIN_RECV_SIZE);
	M0_UT_ASSERT(tm2->ntm_recv_queue_max_recv_msgs == max_recv_msgs);

	/* TM1 start */
	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&tm2->ntm_chan, &tmwait);
	ut_end_point_create_called = false;
	M0_UT_ASSERT(m0_net_tm_tlist_length(&tm2->ntm_q[M0_NET_QT_MSG_RECV]) ==
		     0);
	rc = m0_net_tm_start(tm2, "addr2");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_start_called);
	M0_UT_ASSERT(tm2->ntm_state == M0_NET_TM_STARTING ||
		     tm2->ntm_state == M0_NET_TM_STARTED);

	/* wait on channel until TM state changed to started */
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	M0_UT_ASSERT(ut_tm_prov_event_cb_calls == 2);
	M0_UT_ASSERT(tm2->ntm_state == M0_NET_TM_STARTED);
	M0_UT_ASSERT(ut_end_point_create_called);
	/* No buffers to initially provision TM1. */
	M0_UT_ASSERT(m0_net_tm_tlist_length(&tm2->ntm_q[M0_NET_QT_MSG_RECV]) ==
		     0);
	M0_UT_ASSERT(m0_atomic64_get(&tm2->ntm_recv_queue_deficit) ==
				      M0_NET_TM_RECV_QUEUE_DEF_LEN);

	/* clean up; real xprt would handle this itself */
	m0_thread_join(&ut_tm_thread);
	m0_thread_fini(&ut_tm_thread);

	/*
	 * Check for domain wide provisioning when pool is replenished.
	 * As pool is empty make deficit in TM1 to 3.so that both the TM's
	 * are provisioned.
	 */
	m0_net_tm_pool_length_set(tm1, 13);
	M0_UT_ASSERT(tm1->ntm_recv_queue_min_length == 13);
	M0_UT_ASSERT(pool_prov->nbp_free == 0);
	M0_UT_ASSERT(m0_atomic64_get(&tm1->ntm_recv_queue_deficit) == 3);

	pool_not_empty_called = false;
	m0_net_buffer_pool_lock(pool_prov);
	rc = m0_net_buffer_pool_provision(pool_prov, 6);
	m0_net_buffer_pool_unlock(pool_prov);
	M0_UT_ASSERT(rc == 6);
	M0_UT_ASSERT(pool_not_empty_called);
	M0_UT_ASSERT(m0_atomic64_get(&tm1->ntm_recv_queue_deficit) == 0);
	M0_UT_ASSERT(m0_atomic64_get(&tm2->ntm_recv_queue_deficit) == 0);
	M0_UT_ASSERT(m0_net_tm_tlist_length(&tm2->ntm_q[M0_NET_QT_MSG_RECV]) ==
		     tm2->ntm_recv_queue_min_length);
	M0_UT_ASSERT(pool_prov->nbp_free == pool_prov->nbp_buf_nr -
		     tm1->ntm_recv_queue_min_length -
		     tm2->ntm_recv_queue_min_length);

	/*
	 * To test use case "buffer colour correctness during auto
	 * provisioning".
	 * - Buffer in TM1 having colour1 will be returned to the empty
	 *   pool when TM1 buffer is deleted.
	 * - Also delete a buffer in TM2 so that pool contain buffers of both
	 *   colours.
	 * - make sure buffer pool will have more than one buffer, otherwise
	 *   TM2 may get TM1 colured buffer.
	 * - Check that corresponding coloured buffers are provisioned to TM1
	 *   and TM2.
	 */
	/* Add some uncoloured buffers. */
	m0_net_buffer_pool_lock(pool_prov);
	rc = m0_net_buffer_pool_provision(pool_prov, 2);
	m0_net_buffer_pool_unlock(pool_prov);
	M0_UT_ASSERT(rc == 2);

	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm1->ntm_pool_colour]) == 0);
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm2->ntm_pool_colour]) == 0);
	/*
	 * Adds a colured buffer into the pool and checks for it's presence in
	 * corresponding coloured list of the pool.
	 */
	nb_tm1 = pool_colour_buffer_add(tm1);
	nb_tm2 = pool_colour_buffer_add(tm2);
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm2->ntm_pool_colour]) == 1);
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm1->ntm_pool_colour]) == 1);

	/* Add some uncoloured buffers. */
	m0_net_buffer_pool_lock(pool_prov);
	rc = m0_net_buffer_pool_provision(pool_prov, 2);
	m0_net_buffer_pool_unlock(pool_prov);

	/*
	 * Provisions coloured buffer from the pool and checks it's correctness
	 * in receive queue of corresponding TM regardless of the order of
	 * provisioning.
	 */
	provision_buffer_validate_colour(nb_tm1, tm1);
	provision_buffer_validate_colour(nb_tm2, tm2);

	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm1->ntm_pool_colour]) == 0);
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm2->ntm_pool_colour]) == 0);

	nb_tm1 = pool_colour_buffer_add(tm1);
	nb_tm2 = pool_colour_buffer_add(tm2);
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm1->ntm_pool_colour]) == 1);
	M0_UT_ASSERT(m0_net_tm_tlist_length(
		&pool_prov->nbp_colours[tm1->ntm_pool_colour]) == 1);

	provision_buffer_validate_colour(nb_tm2, tm2);
	provision_buffer_validate_colour(nb_tm1, tm1);

	/*
	 * When TM stop is called it returns buffers in TM receive queue to
	 * the pool in ut_prov_msg_recv_cb. As pool is empty, adding buffers
	 * to it will trigger m0_net_buffer_pool_not_empty cb which does the
	 * domain wide re-provisioning based on deficit value.
	 *
	 * To test use case "return a buffer to the pool and trigger
	 * re-provisioning", do the following.
	 * - Create some deficit in TM2.
	 * - Stop the TM1
	 *   As a result of this buffers used in TM1 will be returned to
	 *   the empty pool and will be used to provision TM2.
	 */
	pool_not_empty_called = false;
	M0_UT_ASSERT(pool_prov->nbp_free == 1);
	/* Create deficit of 10 buffers in TM2. */
	m0_net_tm_pool_length_set(tm2, 15);
	M0_UT_ASSERT(m0_atomic64_get(&tm2->ntm_recv_queue_deficit) == 10);

	/* TM1 stop and fini */
	ut_tm_stop_fini(tm1);
	M0_UT_ASSERT(ut_tm_prov_event_cb_calls == 3);
	/* Check whether all buffers are returned to the pool. */
	M0_UT_ASSERT(pool_prov->nbp_free == pool_prov->nbp_buf_nr -
		     tm2->ntm_recv_queue_min_length);
	M0_UT_ASSERT(pool_not_empty_called);
	M0_UT_ASSERT(pool_prov->nbp_free == 5);
	/* TM2 is provisioned with buffers of TM1 returned to the pool. */
	M0_UT_ASSERT(m0_atomic64_get(&tm2->ntm_recv_queue_deficit) == 0);

	/* TM2 stop and fini */
	ut_tm_stop_fini(tm2);
	/* Check whether all buffers are returned to the pool. */
	M0_UT_ASSERT(pool_prov->nbp_free == pool_prov->nbp_buf_nr);
	M0_UT_ASSERT(ut_tm_prov_event_cb_calls == 4);

	m0_clink_fini(&tmwait);
	/* Finalize the buffer pool. */
	m0_net_buffer_pool_fini(pool_prov);
	m0_free(pool_prov);

	/* fini the domain */
	ut_dom_fini_called = false;
	m0_net_domain_fini(dom);
	M0_UT_ASSERT(ut_dom_fini_called);
	ut_dom_init_called = false;
}

struct m0_ut_suite m0_net_tm_prov_ut = {
        .ts_name = "net-prov-ut",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "net_prov", test_net_tm_prov},
                { NULL, NULL }
        }
};
M0_EXPORTED(m0_net_tm_prov_ut);
