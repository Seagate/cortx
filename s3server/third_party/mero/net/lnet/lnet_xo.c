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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 11/16/2011
 */

/**
   @addtogroup LNetXODFS
   @{
 */

static bool nlx_dom_invariant(const struct m0_net_domain *dom)
{
	const struct nlx_xo_domain *dp = dom->nd_xprt_private;
	return _0C(dp != NULL) && _0C(dp->xd_dom == dom) &&
		_0C(dom->nd_xprt == &m0_net_lnet_xprt);
}

static bool nlx_ep_invariant(const struct m0_net_end_point *ep)
{
	const struct nlx_xo_ep *xep;
	if (ep == NULL)
		return false;
	xep = container_of(ep, struct nlx_xo_ep, xe_ep);
	return _0C(xep->xe_magic == M0_NET_LNET_XE_MAGIC) &&
		_0C(xep->xe_ep.nep_addr == &xep->xe_addr[0]);
}

static bool nlx_buffer_invariant(const struct m0_net_buffer *nb)
{
	const struct nlx_xo_buffer *bp = nb->nb_xprt_private;

	return _0C(bp != NULL) && _0C(bp->xb_nb == nb) && _0C(nlx_dom_invariant(nb->nb_dom)) &&
	    _0C(bp->xb_core.cb_buffer_id == (nlx_core_opaque_ptr_t) nb) &&
	    _0C(ergo(nb->nb_flags & M0_NET_BUF_QUEUED,
		 nb->nb_tm != NULL && nlx_tm_invariant(nb->nb_tm))) &&
	    _0C(nlx_xo_buffer_bufvec_invariant(nb));
}

static bool nlx_tm_invariant(const struct m0_net_transfer_mc *tm)
{
	const struct nlx_xo_transfer_mc *tp = tm->ntm_xprt_private;
	return tp != NULL && tp->xtm_tm == tm && nlx_dom_invariant(tm->ntm_dom);
}

/** Unit test intercept support.
   Conventions to use:
   - All such subs must be declared in headers.
   - A macro named for the subroutine, but with the "NLX" portion of the prefix
   in capitals, should be used to call the subroutine via this intercept
   vector.
   - UT should restore the vector upon completion. It is not declared
   const so that the UTs can modify it.
 */
struct nlx_xo_interceptable_subs {
	int (*_nlx_core_buf_event_wait)(struct nlx_core_domain *lcdom,
					struct nlx_core_transfer_mc *lctm,
					m0_time_t timeout);
	int (*_nlx_ep_create)(struct m0_net_end_point **epp,
			      struct m0_net_transfer_mc *tm,
			      const struct nlx_core_ep_addr *cepa);
	m0_time_t (*_nlx_tm_get_buffer_timeout_tick)(const struct
						     m0_net_transfer_mc *tm);
	int (*_nlx_tm_timeout_buffers)(struct m0_net_transfer_mc *tm,
				       m0_time_t now);
};
static struct nlx_xo_interceptable_subs nlx_xo_iv = {
#define _NLXIS(s) ._##s = s

	_NLXIS(nlx_core_buf_event_wait),
	_NLXIS(nlx_ep_create),
	_NLXIS(nlx_tm_get_buffer_timeout_tick),
	_NLXIS(nlx_tm_timeout_buffers),

#undef _NLXI
};

#define NLX_core_buf_event_wait(lcdom, lctm, timeout)		\
	(*nlx_xo_iv._nlx_core_buf_event_wait)(lcdom, lctm, timeout)
#define NLX_ep_create(epp, tm, cepa) \
	(*nlx_xo_iv._nlx_ep_create)(epp, tm, cepa)
#define NLX_tm_get_buffer_timeout_tick(tm) \
	(*nlx_xo_iv._nlx_tm_get_buffer_timeout_tick)(tm)
#define NLX_tm_timeout_buffers(tm, now) \
	(*nlx_xo_iv._nlx_tm_timeout_buffers)(tm, now)

static int nlx_xo_dom_init(struct m0_net_xprt *xprt, struct m0_net_domain *dom)
{
	struct nlx_xo_domain *dp;
	int rc;

	M0_ENTRY();

	M0_PRE(dom->nd_xprt_private == NULL);
	M0_PRE(xprt == &m0_net_lnet_xprt);
	NLX_ALLOC_ALIGNED_PTR(dp);
	if (dp == NULL)
		return M0_RC(-ENOMEM);
	dom->nd_xprt_private = dp;
	dp->xd_dom = dom;

	rc = nlx_core_dom_init(dom, &dp->xd_core);
	if (rc != 0) {
		NLX_FREE_ALIGNED_PTR(dp);
		dom->nd_xprt_private = NULL;
	} else
		nlx_core_dom_set_debug(&dp->xd_core, dp->_debug_);

	M0_POST(ergo(rc == 0, nlx_dom_invariant(dom)));
	return M0_RC(rc);
}

static void nlx_xo_dom_fini(struct m0_net_domain *dom)
{
	struct nlx_xo_domain *dp = dom->nd_xprt_private;

	M0_PRE(nlx_dom_invariant(dom));
	nlx_core_dom_fini(&dp->xd_core);
	NLX_FREE_ALIGNED_PTR(dp);
	dom->nd_xprt_private = NULL;
}

static m0_bcount_t nlx_xo_get_max_buffer_size(const struct m0_net_domain *dom)
{
	struct nlx_xo_domain *dp = dom->nd_xprt_private;

	M0_PRE(nlx_dom_invariant(dom));
	return nlx_core_get_max_buffer_size(&dp->xd_core);
}

static m0_bcount_t nlx_xo_get_max_buffer_segment_size(const struct
						      m0_net_domain *dom)
{
	struct nlx_xo_domain *dp = dom->nd_xprt_private;

	M0_PRE(nlx_dom_invariant(dom));
	return nlx_core_get_max_buffer_segment_size(&dp->xd_core);
}

static int32_t nlx_xo_get_max_buffer_segments(const struct m0_net_domain *dom)
{
	struct nlx_xo_domain *dp = dom->nd_xprt_private;

	M0_PRE(nlx_dom_invariant(dom));
	return nlx_core_get_max_buffer_segments(&dp->xd_core);
}

static int nlx_xo_end_point_create(struct m0_net_end_point **epp,
				   struct m0_net_transfer_mc *tm,
				   const char *addr)
{
	struct nlx_xo_domain *dp;
	struct nlx_core_ep_addr cepa;
	int rc;

	M0_PRE(nlx_dom_invariant(tm->ntm_dom));
	dp = tm->ntm_dom->nd_xprt_private;
	rc = nlx_core_ep_addr_decode(&dp->xd_core, addr, &cepa);
	if (rc == 0 && cepa.cepa_tmid == M0_NET_LNET_TMID_INVALID)
		rc = M0_ERR(-EINVAL);
	if (rc != 0) {
		return M0_RC(rc);
	}

	return nlx_ep_create(epp, tm, &cepa);
}

static bool nlx_xo_buffer_bufvec_invariant(const struct m0_net_buffer *nb)
{
	const struct m0_vec *v = &nb->nb_buffer.ov_vec;
	const struct m0_bufvec *bv = &nb->nb_buffer;
	m0_bcount_t max_seg_size;
	int i;

	if (m0_vec_count(v) > nlx_xo_get_max_buffer_size(nb->nb_dom) ||
	    v->v_nr > nlx_xo_get_max_buffer_segments(nb->nb_dom))
		return false;
	max_seg_size = nlx_xo_get_max_buffer_segment_size(nb->nb_dom);
	for (i = 0; i < v->v_nr; ++i)
		if (v->v_count[i] == 0 || v->v_count[i] > max_seg_size ||
		    bv->ov_buf[i] == NULL)
			return false;
	return true;
}

static int nlx_xo_buf_register(struct m0_net_buffer *nb)
{
	struct nlx_xo_domain *dp;
	struct nlx_xo_buffer *bp;
	int rc;

	M0_PRE(nb->nb_dom != NULL && nlx_dom_invariant(nb->nb_dom));
	M0_PRE(nb->nb_xprt_private == NULL);
	M0_PRE_EX(nlx_xo_buffer_bufvec_invariant(nb));

	dp = nb->nb_dom->nd_xprt_private;
	NLX_ALLOC_ALIGNED_PTR(bp);
	if (bp == NULL)
		return M0_ERR(-ENOMEM);
	nb->nb_xprt_private = bp;
	bp->xb_nb = nb;

	rc = nlx_core_buf_register(&dp->xd_core, (nlx_core_opaque_ptr_t)nb,
				   &nb->nb_buffer, &bp->xb_core);
	if (rc != 0) {
		NLX_FREE_ALIGNED_PTR(bp);
		nb->nb_xprt_private = NULL;
	}
	M0_POST_EX(ergo(rc == 0, nlx_buffer_invariant(nb)));
	return M0_RC(rc);
}

static void nlx_xo_buf_deregister(struct m0_net_buffer *nb)
{
	struct nlx_xo_domain *dp;
	struct nlx_xo_buffer *bp = nb->nb_xprt_private;

	M0_PRE(nb->nb_dom != NULL && nlx_dom_invariant(nb->nb_dom));
	M0_PRE_EX(nlx_buffer_invariant(nb));
	dp = nb->nb_dom->nd_xprt_private;

	nlx_core_buf_deregister(&dp->xd_core, &bp->xb_core);
	NLX_FREE_ALIGNED_PTR(bp);
	nb->nb_xprt_private = NULL;
	return;
}

/**
   Helper function to allocate a network buffer descriptor.
   Since it is encoded in little endian format and its size is predefined,
   it is simply copied to allocated memory.
 */
static int nlx_xo__nbd_allocate(struct m0_net_transfer_mc *tm,
				const struct nlx_core_buf_desc *cbd,
				struct m0_net_buf_desc *nbd)
{
	nbd->nbd_len = sizeof *cbd;
	NLX_ALLOC(nbd->nbd_data, nbd->nbd_len);
	if (nbd->nbd_data == NULL) {
		nbd->nbd_len = 0; /* for m0_net_desc_free() safety */
		return M0_ERR(-ENOMEM);
	}
	memcpy(nbd->nbd_data, cbd, nbd->nbd_len);

	return 0;
}

/**
   Helper function to recover the internal network buffer descriptor.
 */
static int nlx_xo__nbd_recover(struct m0_net_transfer_mc *tm,
			       const struct m0_net_buf_desc *nbd,
			       struct nlx_core_buf_desc *cbd)
{
	if (nbd->nbd_len != sizeof *cbd) {
		return M0_RC(M0_ERR(-EINVAL));
	}
	memcpy(cbd, nbd->nbd_data, nbd->nbd_len);

	return 0;
}

static int nlx_xo_buf_add(struct m0_net_buffer *nb)
{
	struct nlx_xo_domain *dp;
	struct nlx_xo_transfer_mc *tp;
	struct nlx_xo_buffer *bp = nb->nb_xprt_private;
	struct nlx_core_domain *cd;
	struct nlx_core_transfer_mc *ctp;
	struct nlx_core_buffer *cbp;
	struct nlx_core_buf_desc cbd;
	m0_bcount_t bufsize;
	size_t need;
	int rc;

	M0_PRE_EX(nlx_buffer_invariant(nb) && nb->nb_tm != NULL);
	M0_PRE(m0_mutex_is_locked(&nb->nb_tm->ntm_mutex));
	M0_PRE(nb->nb_offset == 0); /* do not support an offset during add */
	M0_PRE((nb->nb_flags & M0_NET_BUF_RETAIN) == 0);
	dp = nb->nb_dom->nd_xprt_private;
	tp = nb->nb_tm->ntm_xprt_private;
	cd = &dp->xd_core;
	ctp = &tp->xtm_core;
	cbp = &bp->xb_core;

	NLXDBGP(tp, 1, "%p: nlx_xo_buf_add(%p, %d)\n", tp, nb, nb->nb_qtype);

	/* Provision the required number of internal buffer event structures for
	   the maximum expected completion notifications.
	   Release is done in nlx_xo_bev_deliver_all().
	*/
	need = nb->nb_qtype == M0_NET_QT_MSG_RECV ? nb->nb_max_receive_msgs : 1;
	rc = nlx_core_bevq_provision(cd, ctp, need);
	if (rc != 0)
		return M0_RC(rc);
	cbp->cb_max_operations = need;

	bufsize = m0_vec_count(&nb->nb_buffer.ov_vec);
	cbp->cb_length = bufsize; /* default for receive cases */

	cbp->cb_qtype = nb->nb_qtype;
	switch (nb->nb_qtype) {
	case M0_NET_QT_MSG_RECV:
		cbp->cb_min_receive_size = nb->nb_min_receive_size;
		rc = nlx_core_buf_msg_recv(&dp->xd_core, ctp, cbp);
		break;

	case M0_NET_QT_MSG_SEND:
		M0_ASSERT(nb->nb_length <= bufsize);
		cbp->cb_length = nb->nb_length;
		cbp->cb_addr = *nlx_ep_to_core(nb->nb_ep); /* dest addr */
		rc = nlx_core_buf_msg_send(&dp->xd_core, ctp, cbp);
		break;

	case M0_NET_QT_PASSIVE_BULK_RECV:
		nlx_core_buf_desc_encode(ctp, cbp, &cbd);
		rc = nlx_xo__nbd_allocate(nb->nb_tm, &cbd, &nb->nb_desc);
		if (rc == 0)
			rc = nlx_core_buf_passive_recv(&dp->xd_core, ctp, cbp);
		if (rc != 0)
			m0_net_desc_free(&nb->nb_desc);
		break;

	case M0_NET_QT_PASSIVE_BULK_SEND:
		M0_ASSERT(nb->nb_length <= bufsize);
		cbp->cb_length = nb->nb_length;
		nlx_core_buf_desc_encode(ctp, cbp, &cbd);
		rc = nlx_xo__nbd_allocate(nb->nb_tm, &cbd, &nb->nb_desc);
		if (rc == 0)
			rc = nlx_core_buf_passive_send(&dp->xd_core, ctp, cbp);
		if (rc != 0)
			m0_net_desc_free(&nb->nb_desc);
		break;

	case M0_NET_QT_ACTIVE_BULK_RECV:
		rc = nlx_xo__nbd_recover(nb->nb_tm, &nb->nb_desc, &cbd);
		if (rc == 0)
			rc = nlx_core_buf_desc_decode(ctp, cbp, &cbd);
		if (rc == 0) /* remote addr and size decoded */
			rc = nlx_core_buf_active_recv(&dp->xd_core, ctp, cbp);
		break;

	case M0_NET_QT_ACTIVE_BULK_SEND:
		M0_ASSERT(nb->nb_length <= bufsize);
		cbp->cb_length = nb->nb_length;
		rc = nlx_xo__nbd_recover(nb->nb_tm, &nb->nb_desc, &cbd);
		if (rc == 0) /* remote addr and size decoded */
			rc = nlx_core_buf_desc_decode(ctp, cbp, &cbd);
		if (rc == 0)
			rc = nlx_core_buf_active_send(&dp->xd_core, ctp, cbp);
		break;

	default:
		M0_IMPOSSIBLE("invalid queue type");
		break;
	}
	if (rc != 0)
		nlx_core_bevq_release(ctp, need);
	return M0_RC(rc);
}

static void nlx_xo_buf_del(struct m0_net_buffer *nb)
{
	struct nlx_xo_domain *dp;
	struct nlx_xo_transfer_mc *tp;
	struct nlx_xo_buffer *bp = nb->nb_xprt_private;

	M0_PRE_EX(nlx_buffer_invariant(nb) && nb->nb_tm != NULL);
	dp = nb->nb_dom->nd_xprt_private;
	tp = nb->nb_tm->ntm_xprt_private;
	NLXDBGP(tp, 1, "%p: nlx_xo_buf_del(%p, %lX)\n", tp, nb,
		(unsigned long) nb->nb_flags);
	nlx_core_buf_del(&dp->xd_core, &tp->xtm_core, &bp->xb_core);
}

static int nlx_xo_tm_init(struct m0_net_transfer_mc *tm)
{
	struct nlx_xo_transfer_mc *tp;

	M0_PRE(nlx_dom_invariant(tm->ntm_dom));
	M0_PRE(tm->ntm_xprt_private == NULL);

	NLX_ALLOC_ALIGNED_PTR(tp);
	if (tp == NULL)
		return M0_ERR(-ENOMEM);
	tm->ntm_xprt_private = tp;

	/* defer init of processors, thread and xtm_core to TM confine/start */
	m0_cond_init(&tp->xtm_ev_cond, &tm->ntm_mutex);
	tp->xtm_tm = tm;

	M0_POST(nlx_tm_invariant(tm));
	return 0;
}

static void nlx_xo_tm_fini(struct m0_net_transfer_mc *tm)
{
	struct nlx_xo_transfer_mc *tp = tm->ntm_xprt_private;

	M0_PRE(m0_mutex_is_locked(&tm->ntm_dom->nd_mutex));
	M0_PRE(nlx_tm_invariant(tm));
	M0_PRE(tm->ntm_callback_counter == 0);

	if (tp->xtm_ev_thread.t_state != TS_PARKED)
		m0_thread_join(&tp->xtm_ev_thread);

	if (tp->xtm_processors.b_words != NULL)
		m0_bitmap_fini(&tp->xtm_processors);
	m0_cond_fini(&tp->xtm_ev_cond);
	tm->ntm_xprt_private = NULL;
	NLX_FREE_ALIGNED_PTR(tp);
}

static int nlx_xo_tm_start(struct m0_net_transfer_mc *tm, const char *addr)
{
	struct nlx_xo_domain *dp;
	struct nlx_xo_transfer_mc *tp;
	int rc;

	M0_PRE(nlx_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));

	dp = tm->ntm_dom->nd_xprt_private;
	tp = tm->ntm_xprt_private;

	rc = nlx_core_ep_addr_decode(&dp->xd_core, addr,
				     &tp->xtm_core.ctm_addr) ?:
	     M0_THREAD_INIT(&tp->xtm_ev_thread, struct m0_net_transfer_mc *,
			    NULL, &nlx_tm_ev_worker, tm, "m0_nlx_tm");
	return M0_RC(rc);
}

static int nlx_xo_tm_stop(struct m0_net_transfer_mc *tm, bool cancel)
{
	struct nlx_xo_transfer_mc *tp = tm->ntm_xprt_private;

	M0_PRE(nlx_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));

	if (cancel)
		m0_net__tm_cancel(tm);
	m0_cond_signal(&tp->xtm_ev_cond);
	return 0;
}

static int nlx_xo_tm_confine(struct m0_net_transfer_mc *tm,
			     const struct m0_bitmap *processors)
{
	struct nlx_xo_transfer_mc *tp = tm->ntm_xprt_private;
	int rc;

	M0_PRE(nlx_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_PRE(processors != NULL);
	if (tp->xtm_processors.b_words != NULL)
		m0_bitmap_fini(&tp->xtm_processors);
	rc = m0_bitmap_init(&tp->xtm_processors, processors->b_nr);
	if (rc == 0)
		m0_bitmap_copy(&tp->xtm_processors, processors);
	return M0_RC(rc);
}

static void nlx_xo_bev_deliver_all(struct m0_net_transfer_mc *tm)
{
	struct nlx_xo_transfer_mc *tp = tm->ntm_xprt_private;
	struct nlx_core_buffer_event cbev;
	int num_events = 0;

	M0_PRE(nlx_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_PRE(tm->ntm_state == M0_NET_TM_STARTED ||
	       tm->ntm_state == M0_NET_TM_STOPPING);

	while (nlx_core_buf_event_get(&tp->xtm_core, &cbev)) {
		struct m0_net_buffer_event nbev;
		int rc;

		rc = nlx_xo_core_bev_to_net_bev(tm, &cbev, &nbev);
		if (rc != 0) {
			/* Failure can only happen for receive message events
			   when end point creation fails due to lack
			   of memory.
			   We can ignore the event unless LNet also indicates
			   that it has unlinked the buffer; in the latter case
			   we will deliver a failure buffer operation to the
			   application.
			   We will increment the failure counters for the
			   cases where we eat the event.  Note that LNet still
			   knows about the buffer.
			*/
			M0_ASSERT(nbev.nbe_buffer->nb_qtype ==
				  M0_NET_QT_MSG_RECV);
			M0_ASSERT(rc == -ENOMEM);
			if (!cbev.cbe_unlinked) {
				struct m0_net_qstats *q;
				q = &tm->ntm_qstats[nbev.nbe_buffer->nb_qtype];
				q->nqs_num_f_events++;
				NLXDBGP(tp, 1, "%p: skipping event\n", tp);
				continue;
			}
			NLXDBGP(tp, 1, "%p: event conversion failed\n", tp);
		}
		M0_ASSERT_EX(nlx_buffer_invariant(nbev.nbe_buffer));

		/* Release provisioned internal buffer event structures.  Done
		   on unlink only rather than piece-meal with each
		   nlx_core_buf_event_get() so as to not lose count due to
		   premature termination on failure and cancellation.
		*/
		if (cbev.cbe_unlinked) {
			struct nlx_xo_buffer *bp;
			struct nlx_core_buffer *cbp;
			size_t need;
			bp = nbev.nbe_buffer->nb_xprt_private;
			cbp = &bp->xb_core;
			need = cbp->cb_max_operations;
			NLXDBGP(tp, 3, "%p: reducing need by %d\n",
				tp, (int) need);
			nlx_core_bevq_release(&tp->xtm_core, need);
		}
		NLXDBGP(tp, 1, "%p: event buf:%p qt:%d status:%d flags:%lx\n",
			tp, nbev.nbe_buffer, (int) nbev.nbe_buffer->nb_qtype,
			(int) nbev.nbe_status,
			(unsigned long) nbev.nbe_buffer->nb_flags);

		/* Deliver the event out of the mutex.
		   Suppress signalling on the TM channel by incrementing
		   the callback counter.
		 */
		tm->ntm_callback_counter++;
		m0_mutex_unlock(&tm->ntm_mutex);

		num_events++;
		m0_net_buffer_event_post(&nbev);

		/* re-enter the mutex */
		m0_mutex_lock(&tm->ntm_mutex);
		tm->ntm_callback_counter--;

		M0_ASSERT(tm->ntm_state == M0_NET_TM_STARTED ||
			  tm->ntm_state == M0_NET_TM_STOPPING);
	}
	NLXDBGP(tp,2,"%p: delivered %d events\n", tp, num_events);

	/* if we ever left the mutex, wake up waiters on the TM channel */
	if (num_events > 0 && tm->ntm_callback_counter == 0)
		m0_chan_broadcast(&tm->ntm_chan);
}

static int nlx_xo_bev_deliver_sync(struct m0_net_transfer_mc *tm)
{
	M0_PRE(nlx_tm_invariant(tm));
	return 0;
}

static bool nlx_xo_bev_pending(struct m0_net_transfer_mc *tm)
{
	struct nlx_xo_domain *dp;
	struct nlx_xo_transfer_mc *tp;

	M0_PRE(nlx_tm_invariant(tm));
	tp = tm->ntm_xprt_private;
	M0_PRE(nlx_dom_invariant(tm->ntm_dom));
	dp = tm->ntm_dom->nd_xprt_private;
	return nlx_core_buf_event_wait(&dp->xd_core, &tp->xtm_core, 0) == 0;
}

static void nlx_xo_bev_notify(struct m0_net_transfer_mc *tm,
			      struct m0_chan *chan)
{
	struct nlx_xo_transfer_mc *tp;

	M0_PRE(nlx_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	tp = tm->ntm_xprt_private;

	/* set the notification channel and awaken nlx_tm_ev_worker() */
	tp->xtm_ev_chan = chan;
	m0_cond_signal(&tp->xtm_ev_cond);

	return;
}

static m0_bcount_t nlx_xo_get_max_buffer_desc_size(const struct m0_net_domain
						   *dom)
{
	M0_PRE(nlx_dom_invariant(dom));

	return sizeof(struct nlx_core_buf_desc);
}

static const struct m0_net_xprt_ops nlx_xo_xprt_ops = {
	.xo_dom_init                    = nlx_xo_dom_init,
	.xo_dom_fini                    = nlx_xo_dom_fini,
	.xo_get_max_buffer_size         = nlx_xo_get_max_buffer_size,
	.xo_get_max_buffer_segment_size = nlx_xo_get_max_buffer_segment_size,
	.xo_get_max_buffer_segments     = nlx_xo_get_max_buffer_segments,
	.xo_end_point_create            = nlx_xo_end_point_create,
	.xo_buf_register                = nlx_xo_buf_register,
	.xo_buf_deregister              = nlx_xo_buf_deregister,
	.xo_buf_add                     = nlx_xo_buf_add,
	.xo_buf_del                     = nlx_xo_buf_del,
	.xo_tm_init                     = nlx_xo_tm_init,
	.xo_tm_fini                     = nlx_xo_tm_fini,
	.xo_tm_start                    = nlx_xo_tm_start,
	.xo_tm_stop                     = nlx_xo_tm_stop,
	.xo_tm_confine                  = nlx_xo_tm_confine,
	.xo_bev_deliver_all             = nlx_xo_bev_deliver_all,
	.xo_bev_deliver_sync            = nlx_xo_bev_deliver_sync,
	.xo_bev_pending                 = nlx_xo_bev_pending,
	.xo_bev_notify                  = nlx_xo_bev_notify,
	.xo_get_max_buffer_desc_size    = nlx_xo_get_max_buffer_desc_size
};

/**
   @}
 */

/**
   @addtogroup LNetDFS
   @{
 */

#ifdef ENABLE_LUSTRE
struct m0_net_xprt m0_net_lnet_xprt = {
	.nx_name = "lnet",
	.nx_ops  = &nlx_xo_xprt_ops
};
M0_EXPORTED(m0_net_lnet_xprt);
#endif

/**
   @}
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
