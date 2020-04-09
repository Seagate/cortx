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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>,
 *                  Carl Braganza <Carl_Braganza@xyratex.com>
 * Original creation date: 04/07/2011
 */

#include "lib/arith.h" /* M0_CNT_INC */
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/misc.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"
#include "lib/finject.h"
#include "mero/magic.h"
#include "net/net_internal.h"
#include "net/buffer_pool.h"

/**
   @addtogroup net
   @{
 */
M0_TL_DESCR_DEFINE(m0_net_tm, "tm list", M0_INTERNAL,
		   struct m0_net_buffer, nb_tm_linkage, nb_magic,
		   M0_NET_BUFFER_LINK_MAGIC, M0_NET_BUFFER_HEAD_MAGIC);
M0_TL_DEFINE(m0_net_tm, M0_INTERNAL, struct m0_net_buffer);
M0_EXPORTED(m0_net_tm_tlist_is_empty);

M0_INTERNAL bool m0_net__tm_state_is_valid(enum m0_net_tm_state ts)
{
	return ts >= M0_NET_TM_UNDEFINED && ts <= M0_NET_TM_FAILED;
}

M0_INTERNAL bool m0_net__tm_ev_type_is_valid(enum m0_net_tm_ev_type et)
{
	return et >= M0_NET_TEV_ERROR && et < M0_NET_TEV_NR;
}

M0_INTERNAL bool m0_net__tm_event_invariant(const struct m0_net_tm_event *ev)
{
	return
		_0C(m0_net__tm_ev_type_is_valid(ev->nte_type)) &&
		_0C(ev->nte_tm != NULL) &&
		_0C(m0_net__tm_invariant(ev->nte_tm)) &&
		_0C(ergo(ev->nte_type == M0_NET_TEV_STATE_CHANGE,
			 m0_net__tm_state_is_valid(ev->nte_next_state))) &&
		_0C(ergo(ev->nte_type == M0_NET_TEV_STATE_CHANGE &&
			 ev->nte_next_state == M0_NET_TM_STARTED,
			 m0_net__ep_invariant(ev->nte_ep, ev->nte_tm, true)));
}

M0_INTERNAL bool m0_net__tm_invariant(const struct m0_net_transfer_mc *tm)
{
	return
		_0C(tm != NULL) &&
		_0C(tm->ntm_callbacks != NULL) &&
		_0C(tm->ntm_dom != NULL) &&
		_0C(tm->ntm_callbacks->ntc_event_cb != NULL) &&
		_0C(tm->ntm_state >= M0_NET_TM_INITIALIZED &&
		    tm->ntm_state <= M0_NET_TM_FAILED) &&
		_0C(ergo(tm->ntm_state == M0_NET_TM_STARTED,
			 tm->ntm_ep != NULL)) &&
		_0C(ergo(!M0_IN(tm->ntm_state, (M0_NET_TM_STARTED,
						M0_NET_TM_STOPPING)),
			 m0_forall(i, ARRAY_SIZE(tm->ntm_q),
				   m0_net_tm_tlist_is_empty(&tm->ntm_q[i]))));
}

M0_INTERNAL void m0_net_tm_event_post(const struct m0_net_tm_event *ev)
{
	struct m0_net_transfer_mc *tm;
	struct m0_net_buffer_pool *pool = NULL;

	M0_PRE(ev != NULL);
	tm = ev->nte_tm;
	M0_PRE(tm != NULL);
	M0_PRE(m0_mutex_is_not_locked(&tm->ntm_mutex));

	/* pre-callback, in mutex */
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_event_invariant(ev));

	if (ev->nte_type == M0_NET_TEV_STATE_CHANGE) {
		tm->ntm_state = ev->nte_next_state;
		if (tm->ntm_state == M0_NET_TM_STARTED) {
			tm->ntm_ep = ev->nte_ep; /* ep now visible */
			pool = tm->ntm_recv_pool;
		}
	}

	M0_CNT_INC(tm->ntm_callback_counter);
	m0_mutex_unlock(&tm->ntm_mutex);

	(*tm->ntm_callbacks->ntc_event_cb)(ev);

	M0_LOG(M0_DEBUG, "tm=%p state=%d", tm, tm->ntm_state);

	/*
	 * post-callback, out of mutex: perform initial provisioning if
	 * required
	 */
	if (pool != NULL)
		m0_net__tm_provision_recv_q(tm);

	m0_net__tm_post_callback(tm);
}

M0_INTERNAL void m0_net__tm_post_callback(struct m0_net_transfer_mc *tm)
{
	m0_mutex_lock(&tm->ntm_mutex);
	M0_CNT_DEC(tm->ntm_callback_counter);
	if (tm->ntm_callback_counter == 0)
		m0_chan_broadcast(&tm->ntm_chan);
	m0_mutex_unlock(&tm->ntm_mutex);
}

static void m0_net__tm_cleanup(struct m0_net_transfer_mc *tm)
{
	int i;
	tm->ntm_dom = NULL;
	m0_chan_fini_lock(&tm->ntm_chan);
	m0_mutex_fini(&tm->ntm_mutex);
	m0_nep_tlist_fini(&tm->ntm_end_points);
	for (i = 0; i < ARRAY_SIZE(tm->ntm_q); ++i) {
		m0_net_tm_tlist_fini(&tm->ntm_q[i]);
	}
	tm->ntm_xprt_private = NULL;
}

M0_INTERNAL void m0_net__tm_cancel(struct m0_net_transfer_mc *tm)
{
	struct m0_net_buffer *nb;
	int                   qt;

	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));

	for (qt = 0; qt < ARRAY_SIZE(tm->ntm_q); ++qt) {
		m0_tl_for(m0_net_tm, &tm->ntm_q[qt], nb) {
			tm->ntm_dom->nd_xprt->nx_ops->xo_buf_del(nb);
			tm->ntm_qstats[qt].nqs_num_dels++;
		} m0_tl_endfor;
	}
}

M0_INTERNAL int m0_net_tm_init(struct m0_net_transfer_mc *tm,
			       struct m0_net_domain      *dom)
{
	int rc;
	int i;

	m0_mutex_lock(&dom->nd_mutex);
	M0_PRE(tm != NULL);
	M0_PRE(tm->ntm_state == M0_NET_TM_UNDEFINED);
	M0_PRE(tm->ntm_callbacks != NULL &&
	       tm->ntm_callbacks->ntc_event_cb != NULL);

	if (M0_FI_ENABLED("fake_error")) {
		m0_mutex_unlock(&dom->nd_mutex);
		return M0_RC(-EINVAL);
	}
	m0_mutex_init(&tm->ntm_mutex);
	tm->ntm_callback_counter = 0;
	tm->ntm_dom = dom;
	tm->ntm_ep = NULL;
	m0_nep_tlist_init(&tm->ntm_end_points);
	m0_chan_init(&tm->ntm_chan, &tm->ntm_mutex);
	for (i = 0; i < ARRAY_SIZE(tm->ntm_q); ++i) {
		m0_net_tm_tlist_init(&tm->ntm_q[i]);
	}
	M0_SET_ARR0(tm->ntm_qstats);
	tm->ntm_xprt_private = NULL;
	tm->ntm_bev_auto_deliver = true;
	tm->ntm_recv_pool = NULL;
	tm->ntm_recv_queue_min_length = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	m0_atomic64_set(&tm->ntm_recv_queue_deficit, 0);
	tm->ntm_pool_colour = M0_BUFFER_ANY_COLOUR;
	rc = dom->nd_xprt->nx_ops->xo_tm_init(tm);
	if (rc >= 0) {
		m0_list_add_tail(&dom->nd_tms, &tm->ntm_dom_linkage);
		tm->ntm_state = M0_NET_TM_INITIALIZED;
	} else
		m0_net__tm_cleanup(tm);
	m0_mutex_unlock(&dom->nd_mutex);

	return M0_RC(rc);
}
M0_EXPORTED(m0_net_tm_init);

M0_INTERNAL void m0_net_tm_fini(struct m0_net_transfer_mc *tm)
{
	struct m0_net_domain *dom = tm->ntm_dom;
	M0_ENTRY("tm=%p", tm);

	/*
	 * Wait for ongoing event processing to drain without holding lock:
	 * events modify state and end point refcounts. Also applies to ongoing
	 * provisioning, which requires a check for state in addition to
	 * counter.
	 */
	if (tm->ntm_callback_counter > 0) {
		struct m0_clink tmwait;
		M0_LOG(M0_NOTICE, "tm=%p state=%d cc=%d", tm, tm->ntm_state,
			tm->ntm_callback_counter);
		m0_clink_init(&tmwait, NULL);
		m0_clink_add_lock(&tm->ntm_chan, &tmwait);
		while (tm->ntm_callback_counter > 0 ||
		       tm->ntm_state == M0_NET_TM_STARTED)
			m0_chan_wait(&tmwait);
		m0_clink_del_lock(&tmwait);
	}

	m0_mutex_lock(&dom->nd_mutex);
	M0_PRE(M0_IN(tm->ntm_state, (M0_NET_TM_STOPPED, M0_NET_TM_FAILED,
				     M0_NET_TM_INITIALIZED)));
	M0_PRE(m0_forall(i, ARRAY_SIZE(tm->ntm_q),
			 m0_net_tm_tlist_is_empty(&tm->ntm_q[i])));
	M0_PRE((m0_nep_tlist_is_empty(&tm->ntm_end_points) &&
		tm->ntm_ep == NULL) ||
	       (m0_nep_tlist_length(&tm->ntm_end_points) == 1 &&
		tm->ntm_ep != NULL &&
		m0_nep_tlist_contains(&tm->ntm_end_points, tm->ntm_ep) &&
		m0_atomic64_get(&tm->ntm_ep->nep_ref.ref_cnt) == 1));

	/* release method requires TM mutex to be locked */
	m0_mutex_lock(&tm->ntm_mutex);
	if (tm->ntm_ep != NULL) {
		m0_ref_put(&tm->ntm_ep->nep_ref);
		tm->ntm_ep = NULL;
	}
	m0_mutex_unlock(&tm->ntm_mutex);

	dom->nd_xprt->nx_ops->xo_tm_fini(tm);

	M0_ASSERT(m0_nep_tlist_is_empty(&tm->ntm_end_points));

	m0_list_del(&tm->ntm_dom_linkage);
	tm->ntm_state = M0_NET_TM_UNDEFINED;
	m0_net__tm_cleanup(tm);
	m0_list_link_fini(&tm->ntm_dom_linkage);

	m0_mutex_unlock(&dom->nd_mutex);
	M0_LEAVE("tm=%p", tm);
}
M0_EXPORTED(m0_net_tm_fini);

M0_INTERNAL int m0_net_tm_start(struct m0_net_transfer_mc *tm, const char *addr)
{
	int rc;

	M0_ENTRY("tm=%p addr=%s", (void *)tm, addr);

	M0_ASSERT(addr != NULL);
	M0_PRE(tm != NULL);
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_INITIALIZED);

	if (M0_FI_ENABLED("fake_error")) {
		tm->ntm_state = M0_NET_TM_FAILED;
		m0_mutex_unlock(&tm->ntm_mutex);
		return M0_RC(0);
	}

	tm->ntm_state = M0_NET_TM_STARTING;
	rc = tm->ntm_dom->nd_xprt->nx_ops->xo_tm_start(tm, addr);
	if (rc < 0) {
		/* xprt did not start, no retry supported */
		tm->ntm_state = M0_NET_TM_FAILED;
		M0_ASSERT(tm->ntm_ep == NULL);
	}
	M0_POST(m0_net__tm_invariant(tm));
	m0_mutex_unlock(&tm->ntm_mutex);
	M0_ASSERT(rc <= 0);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_tm_start);

M0_INTERNAL int m0_net_tm_stop(struct m0_net_transfer_mc *tm, bool abort)
{
	int rc;
	enum m0_net_tm_state oldstate;

	M0_ENTRY("tm=%p abort=%d", (void *)tm, !!abort);

	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(M0_IN(tm->ntm_state, (M0_NET_TM_INITIALIZED, M0_NET_TM_STARTING,
				     M0_NET_TM_STARTED)));

	oldstate = tm->ntm_state;
	tm->ntm_state = M0_NET_TM_STOPPING;
	rc = tm->ntm_dom->nd_xprt->nx_ops->xo_tm_stop(tm, abort);
	if (rc < 0) {
		tm->ntm_state = oldstate;
	} else
		m0_atomic64_set(&tm->ntm_recv_queue_deficit, 0);

	M0_POST(m0_net__tm_invariant(tm));
	m0_mutex_unlock(&tm->ntm_mutex);
	M0_ASSERT(rc <= 0);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_tm_stop);

M0_INTERNAL int m0_net__tm_stats_get(struct m0_net_transfer_mc *tm,
				     enum m0_net_queue_type qtype,
				     struct m0_net_qstats *qs, bool reset)
{
	M0_PRE(qtype == M0_NET_QT_NR || m0_net__qtype_is_valid(qtype));
	M0_ASSERT(reset || qs != NULL);
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_PRE(m0_net__tm_invariant(tm));
	if (qtype == M0_NET_QT_NR) {
		if (qs != NULL)
			memcpy(qs, tm->ntm_qstats, sizeof tm->ntm_qstats);
		if (reset)
			M0_SET_ARR0(tm->ntm_qstats);
	} else {
		if (qs != NULL)
			*qs = tm->ntm_qstats[qtype];
		if (reset)
			M0_SET0(&tm->ntm_qstats[qtype]);
	}

	return 0;
}

M0_INTERNAL int m0_net_tm_stats_get(struct m0_net_transfer_mc *tm,
				    enum m0_net_queue_type qtype,
				    struct m0_net_qstats *qs, bool reset)
{
	int rc;

	m0_mutex_lock(&tm->ntm_mutex);
	rc = m0_net__tm_stats_get(tm, qtype, qs, reset);
	m0_mutex_unlock(&tm->ntm_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_tm_stats_get);

M0_INTERNAL int m0_net_tm_confine(struct m0_net_transfer_mc *tm,
				  const struct m0_bitmap *processors)
{
	int rc;
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_INITIALIZED);
	M0_PRE(processors != NULL);
	if (tm->ntm_dom->nd_xprt->nx_ops->xo_tm_confine != NULL)
		rc =
		    tm->ntm_dom->nd_xprt->nx_ops->xo_tm_confine(tm, processors);
	else
		rc = -ENOSYS;
	M0_POST(m0_net__tm_invariant(tm));
	M0_POST(tm->ntm_state == M0_NET_TM_INITIALIZED);
	m0_mutex_unlock(&tm->ntm_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_tm_confine);

M0_INTERNAL int
m0_net_buffer_event_deliver_synchronously(struct m0_net_transfer_mc *tm)
{
	int rc;
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_INITIALIZED);
	M0_PRE(tm->ntm_bev_auto_deliver);
	if (tm->ntm_dom->nd_xprt->nx_ops->xo_bev_deliver_sync != NULL) {
		rc = tm->ntm_dom->nd_xprt->nx_ops->xo_bev_deliver_sync(tm);
		if (rc == 0)
			tm->ntm_bev_auto_deliver = false;
	} else
		rc = -ENOSYS;
	M0_POST(ergo(rc == 0, !tm->ntm_bev_auto_deliver));
	M0_POST(m0_net__tm_invariant(tm));
	m0_mutex_unlock(&tm->ntm_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_buffer_event_deliver_synchronously);

M0_INTERNAL void m0_net_buffer_event_deliver_all(struct m0_net_transfer_mc *tm)
{
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_STARTED);
	M0_PRE(!tm->ntm_bev_auto_deliver);
	tm->ntm_dom->nd_xprt->nx_ops->xo_bev_deliver_all(tm);
	M0_POST(m0_net__tm_invariant(tm));
	m0_mutex_unlock(&tm->ntm_mutex);
}
M0_EXPORTED(m0_net_buffer_event_deliver_all);

M0_INTERNAL bool m0_net_buffer_event_pending(struct m0_net_transfer_mc *tm)
{
	bool result;
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_STARTED);
	M0_PRE(!tm->ntm_bev_auto_deliver);
	result = tm->ntm_dom->nd_xprt->nx_ops->xo_bev_pending(tm);
	M0_POST(m0_net__tm_invariant(tm));
	m0_mutex_unlock(&tm->ntm_mutex);
	return result;
}
M0_EXPORTED(m0_net_buffer_event_pending);

M0_INTERNAL void m0_net_buffer_event_notify(struct m0_net_transfer_mc *tm,
					    struct m0_chan *chan)
{
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_STARTED);
	M0_PRE(!tm->ntm_bev_auto_deliver);
	tm->ntm_dom->nd_xprt->nx_ops->xo_bev_notify(tm, chan);
	M0_POST(m0_net__tm_invariant(tm));
	m0_mutex_unlock(&tm->ntm_mutex);
}
M0_EXPORTED(m0_net_buffer_event_notify);

M0_INTERNAL void m0_net_tm_colour_set(struct m0_net_transfer_mc *tm,
				      uint32_t colour)
{
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(M0_IN(tm->ntm_state, (M0_NET_TM_INITIALIZED, M0_NET_TM_STARTING,
				     M0_NET_TM_STARTED)));
	tm->ntm_pool_colour = colour;
	m0_mutex_unlock(&tm->ntm_mutex);
}
M0_EXPORTED(m0_net_tm_colour_set);

M0_INTERNAL uint32_t m0_net_tm_colour_get(struct m0_net_transfer_mc *tm)
{
	uint32_t colour;
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	colour = tm->ntm_pool_colour;
	m0_mutex_unlock(&tm->ntm_mutex);
	return colour;
}
M0_EXPORTED(m0_net_tm_colour_get);

M0_INTERNAL int m0_net_tm_pool_attach(struct m0_net_transfer_mc *tm,
				      struct m0_net_buffer_pool *bufpool,
				      const struct m0_net_buffer_callbacks
				      *callbacks, m0_bcount_t min_recv_size,
				      uint32_t max_recv_msgs,
				      uint32_t min_recv_queue_len)
{
	int rc;
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_INITIALIZED);
	M0_PRE(bufpool != NULL);
	M0_PRE(callbacks != NULL &&
	       callbacks->nbc_cb[M0_NET_QT_MSG_RECV] != NULL);
	M0_PRE(min_recv_size > 0);
	M0_PRE(max_recv_msgs > 0);
	if (bufpool->nbp_ndom == tm->ntm_dom) {
		tm->ntm_recv_pool		 = bufpool;
		tm->ntm_recv_pool_callbacks	 = callbacks;
		tm->ntm_recv_queue_min_recv_size = min_recv_size;
		tm->ntm_recv_queue_max_recv_msgs = max_recv_msgs;
		if(min_recv_queue_len > M0_NET_TM_RECV_QUEUE_DEF_LEN)
			tm->ntm_recv_queue_min_length = min_recv_queue_len;
		rc = 0;
	} else
		rc = -EINVAL;
	m0_mutex_unlock(&tm->ntm_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_tm_pool_attach);

M0_INTERNAL void m0_net_tm_pool_length_set(struct m0_net_transfer_mc *tm,
					   uint32_t len)
{
	struct m0_net_buffer_pool *pool = NULL;

	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	M0_PRE(tm->ntm_state == M0_NET_TM_INITIALIZED ||
	       tm->ntm_state == M0_NET_TM_STARTING ||
	       tm->ntm_state == M0_NET_TM_STARTED);
	if (len > M0_NET_TM_RECV_QUEUE_DEF_LEN)
		tm->ntm_recv_queue_min_length = len;
	if (tm->ntm_recv_pool != NULL && tm->ntm_state == M0_NET_TM_STARTED) {
		pool = tm->ntm_recv_pool;
		M0_CNT_INC(tm->ntm_callback_counter);
	}
	m0_mutex_unlock(&tm->ntm_mutex);
	if (pool != NULL) {
		m0_net__tm_provision_recv_q(tm);
		m0_mutex_lock(&tm->ntm_mutex);
		M0_CNT_DEC(tm->ntm_callback_counter);
		m0_chan_broadcast(&tm->ntm_chan);
		m0_mutex_unlock(&tm->ntm_mutex);
	}
	return;
}
M0_EXPORTED(m0_net_tm_pool_length_set);

#undef M0_TRACE_SUBSYSTEM

/** @} end of net group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
