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
 * Original creation date: 12/15/2011
 */

/**
   @addtogroup LNetXODFS
   @{
 */

static inline bool all_tm_queues_are_empty(struct m0_net_transfer_mc *tm)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tm->ntm_q); ++i)
		if (!m0_net_tm_tlist_is_empty(&tm->ntm_q[i]))
			return false;
	return true;
}

/**
   Cancel buffer operations if they have timed out.
   @param tm The transfer machine concerned.
   @param now The current time.
   @pre m0_mutex_is_locked(&tm->ntm_mutex);
   @retval The number of buffers timed out.
 */
static int nlx_tm_timeout_buffers(struct m0_net_transfer_mc *tm, m0_time_t now)
{
	int                        qt;
	struct m0_net_buffer      *nb;
	struct nlx_xo_transfer_mc *tp M0_UNUSED;
	int                        i;

	M0_PRE(tm != NULL && nlx_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));

	tp = tm->ntm_xprt_private;
	NLXDBGP(tp, 2, "%p: nlx_tm_timeout_buffers\n", tp);
	for (i = 0, qt = M0_NET_QT_MSG_RECV; qt < M0_NET_QT_NR; ++qt) {
		m0_tl_for(m0_net_tm, &tm->ntm_q[qt], nb) {
			/* nb_timeout set to M0_TIME_NEVER if disabled */
			if (nb->nb_timeout > now)
				continue;
			nb->nb_flags |= M0_NET_BUF_TIMED_OUT;
			nlx_xo_buf_del(nb); /* cancel if possible; !dequeued */
			++i;
		} m0_tl_endfor;
	}
	return i;
}

/**
   Subroutine to return the buffer timeout period for a transfer machine.
   The subroutine exists only for unit test control.
   It is only called once in the lifetime of a transfer machine.
 */
static m0_time_t
nlx_tm_get_buffer_timeout_tick(const struct m0_net_transfer_mc *tm)
{
	return m0_time(M0_NET_LNET_BUF_TIMEOUT_TICK_SECS, 0);
}

/**
   The entry point of the LNet transport event processing thread.
   It is spawned when the transfer machine starts.  It completes
   the start-up process and then loops, handling asynchronous buffer event
   delivery, until the transfer machine enters the M0_NET_TM_STOPPING state.
   Once that state transition is detected, the thread completes its
   processing and terminates.
 */
static void nlx_tm_ev_worker(struct m0_net_transfer_mc *tm)
{
	struct nlx_xo_transfer_mc   *tp;
	struct nlx_core_transfer_mc *ctp;
	struct nlx_xo_domain        *dp;
	struct nlx_core_domain      *cd;
	struct m0_net_tm_event tmev     = {
		.nte_type               = M0_NET_TEV_STATE_CHANGE,
		.nte_tm                 = tm,
		.nte_status             = 0
	};
	m0_time_t                    timeout;
	m0_time_t                    next_buffer_timeout;
	m0_time_t                    buffer_timeout_tick;
	m0_time_t                    now;
	int                          rc = 0;

	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(nlx_tm_invariant(tm));
	tp = tm->ntm_xprt_private;
	ctp = &tp->xtm_core;
	dp = tm->ntm_dom->nd_xprt_private;
	cd = &dp->xd_core;

	nlx_core_tm_set_debug(ctp, tp->_debug_);

	if (tp->xtm_processors.b_nr != 0) {
		M0_ASSERT(m0_thread_self() == &tp->xtm_ev_thread);
		rc = m0_thread_confine(&tp->xtm_ev_thread, &tp->xtm_processors);
	}

	if (rc == 0)
		rc = nlx_core_tm_start(cd, tm, ctp);
	if (rc == 0) {
		rc = nlx_ep_create(&tmev.nte_ep, tm, &ctp->ctm_addr);
		if (rc != 0)
			nlx_core_tm_stop(cd, ctp);
	}

	/*
	  Deliver a M0_NET_TEV_STATE_CHANGE event to transition the TM to
	  the M0_NET_TM_STARTED or M0_NET_TM_FAILED states.
	  Set the transfer machine's end point in the event on success.
	 */
	if (rc == 0) {
		tmev.nte_next_state = M0_NET_TM_STARTED;
	} else {
		tmev.nte_next_state = M0_NET_TM_FAILED;
		tmev.nte_status = rc;
	}
	tmev.nte_time = m0_time_now();
	tm->ntm_ep = NULL;
	m0_mutex_unlock(&tm->ntm_mutex);
	m0_net_tm_event_post(&tmev);
	if (rc != 0)
		return;

	m0_mutex_lock(&tm->ntm_mutex);
	now = m0_time_now();
	m0_mutex_unlock(&tm->ntm_mutex);

	buffer_timeout_tick = NLX_tm_get_buffer_timeout_tick(tm);
	next_buffer_timeout = m0_time_add(now, buffer_timeout_tick);

	NLXDBGP(tp, 1, "%p: tm_worker_thread started\n", tp);

	while (1) {
		/* Compute next timeout (short if automatic or stopping).
		   Upper bound constrained by the next stat schedule time.
		 */
		if (tm->ntm_bev_auto_deliver ||
		    tm->ntm_state == M0_NET_TM_STOPPING)
			timeout = m0_time_from_now(
					   M0_NET_LNET_EVT_SHORT_WAIT_SECS, 0);
		else
			timeout = m0_time_from_now(
					    M0_NET_LNET_EVT_LONG_WAIT_SECS, 0);
		if (timeout > next_buffer_timeout)
			timeout = next_buffer_timeout;

		if (tm->ntm_bev_auto_deliver) {
			rc = NLX_core_buf_event_wait(cd, ctp, timeout);
			/* buffer event processing */
			if (rc == 0) { /* did not time out - events pending */
				m0_mutex_lock(&tm->ntm_mutex);
				nlx_xo_bev_deliver_all(tm);
				m0_mutex_unlock(&tm->ntm_mutex);
			}
		} else {		/* application initiated delivery */
			m0_mutex_lock(&tm->ntm_mutex);
			if (tp->xtm_ev_chan == NULL)
				m0_cond_timedwait(&tp->xtm_ev_cond, timeout);
			if (tp->xtm_ev_chan != NULL) {
				m0_mutex_unlock(&tm->ntm_mutex);
				rc = nlx_core_buf_event_wait(cd, ctp, timeout);
				m0_mutex_lock(&tm->ntm_mutex);
				if (rc == 0 && tp->xtm_ev_chan != NULL) {
					if (tp->xtm_ev_chan == &tm->ntm_chan) {
						m0_chan_signal(tp->xtm_ev_chan);
					} else {
						m0_chan_signal_lock(
							tp->xtm_ev_chan);
					}
					tp->xtm_ev_chan = NULL;
				}
			}
			m0_mutex_unlock(&tm->ntm_mutex);
		}

		/* periodically record statistics and time out buffers */
		now = m0_time_now();
		m0_mutex_lock(&tm->ntm_mutex);
		if (now >= next_buffer_timeout) {
			NLX_tm_timeout_buffers(tm, now);
			next_buffer_timeout = m0_time_add(now,
							  buffer_timeout_tick);
		}
		m0_mutex_unlock(&tm->ntm_mutex);

		/* termination processing */
		if (tm->ntm_state == M0_NET_TM_STOPPING) {
			bool must_stop = false;
			m0_mutex_lock(&tm->ntm_mutex);
			if (all_tm_queues_are_empty(tm) &&
			    tm->ntm_callback_counter == 0) {
				nlx_core_tm_stop(cd, ctp);
				must_stop = true;
			}
			m0_mutex_unlock(&tm->ntm_mutex);
			if (must_stop) {
				tmev.nte_next_state = M0_NET_TM_STOPPED;
				tmev.nte_time = m0_time_now();
				m0_net_tm_event_post(&tmev);
				break;
			}
		}
	}
}

/**
   Helper subroutine to create the network buffer event from the internal
   core buffer event.

   @param tm Pointer to TM.
   @param lcbev Pointer to LNet transport core buffer event.
   @param nbev Pointer to network buffer event to fill in.
   @pre m0_mutex_is_locked(&tm->ntm_mutex)
   @post ergo(nbev->nbe_buffer->nb_flags & M0_NET_BUF_RETAIN,
              rc == 0 && !lcbev->cbe_unlinked);
   @post rc == 0 || rc == -ENOMEM
 */
M0_INTERNAL int nlx_xo_core_bev_to_net_bev(struct m0_net_transfer_mc *tm,
					   struct nlx_core_buffer_event *lcbev,
					   struct m0_net_buffer_event *nbev)
{
	struct nlx_xo_transfer_mc *tp M0_UNUSED;
	struct m0_net_buffer      *nb;
	int                        rc = 0;

	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_PRE(nlx_tm_invariant(tm));
	tp = tm->ntm_xprt_private;

	/* Recover the transport space network buffer address from the
	   opaque data set during registration.
	 */
	nb = (struct m0_net_buffer *) lcbev->cbe_buffer_id;
	M0_ASSERT(m0_net__buffer_invariant(nb));

	M0_SET0(nbev);
	nbev->nbe_buffer = nb;
	nbev->nbe_status = lcbev->cbe_status;
	nbev->nbe_time   = m0_time_add(lcbev->cbe_time, nb->nb_add_time);
	if (!lcbev->cbe_unlinked)
		nb->nb_flags |= M0_NET_BUF_RETAIN;
	else
		nb->nb_flags &= ~M0_NET_BUF_RETAIN;
	if (nbev->nbe_status != 0) {
		if (nbev->nbe_status == -ECANCELED &&
		    nb->nb_flags & M0_NET_BUF_TIMED_OUT)
			nbev->nbe_status = -ETIMEDOUT;
		goto done; /* this is not an error from this sub */
	} else
		nb->nb_flags &= ~M0_NET_BUF_TIMED_OUT;

	if (nb->nb_qtype == M0_NET_QT_MSG_RECV) {
		rc = NLX_ep_create(&nbev->nbe_ep, tm, &lcbev->cbe_sender);
		if (rc != 0) {
			nbev->nbe_status = rc;
			goto done;
		}
		nbev->nbe_offset = lcbev->cbe_offset;
	}
	if (nb->nb_qtype == M0_NET_QT_MSG_RECV ||
	    nb->nb_qtype == M0_NET_QT_PASSIVE_BULK_RECV ||
	    nb->nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV) {
		nbev->nbe_length = lcbev->cbe_length;
	}
 done:
	NLXDBG(tp,2,nlx_print_core_buffer_event("bev_to_net_bev: cbev", lcbev));
	NLXDBG(tp,2,nlx_print_net_buffer_event("bev_to_net_bev: nbev:", nbev));
	NLXDBG(tp,2,NLXP("bev_to_net_bev: rc=%d\n", rc));

	M0_POST(ergo(nb->nb_flags & M0_NET_BUF_RETAIN,
		     !lcbev->cbe_unlinked));
	/* currently we only support RETAIN for received messages */
	M0_POST(ergo(nb->nb_flags & M0_NET_BUF_RETAIN,
		     nb->nb_qtype == M0_NET_QT_MSG_RECV));
	M0_POST(rc == 0 || rc == -ENOMEM);
	M0_POST(m0_net__buffer_event_invariant(nbev));
	return M0_RC(rc);
}

/** @} */ /* LNetXODFS */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
