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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>,
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 04/12/2011
 */

/* This file is included into mem_xprt_xo.c */

/**
   @addtogroup bulkmem
   @{
 */

/**
   Work function for the M0_NET_XOP_STATE_CHANGE work item.
   @param tm the corresponding transfer machine
   @param wi the work item, this will be freed
 */
static void mem_wf_state_change(struct m0_net_transfer_mc *tm,
				struct m0_net_bulk_mem_work_item *wi)
{
	enum m0_net_bulk_mem_tm_state next_state = wi->xwi_next_state;
	struct m0_net_bulk_mem_tm_pvt *tp = mem_tm_to_pvt(tm);
	struct m0_net_tm_event ev = {
		.nte_type   = M0_NET_TEV_STATE_CHANGE,
		.nte_tm     = tm,
		.nte_status = 0
	};

	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_ASSERT(next_state == M0_NET_XTM_STARTED ||
		  next_state == M0_NET_XTM_STOPPED);

	if (next_state == M0_NET_XTM_STARTED) {
		/*
		  Application can call m0_net_tm_stop -> mem_xo_tm_stop
		  before the M0_NET_XTM_STARTED work item gets processed.
		  If that happens, ignore the M0_NET_XTM_STARTED item.
		 */
		if (tp->xtm_state < M0_NET_XTM_STOPPING) {
			M0_ASSERT(tp->xtm_state == M0_NET_XTM_STARTING);
			if (wi->xwi_status != 0) {
				tp->xtm_state = M0_NET_XTM_FAILED;
				ev.nte_next_state = M0_NET_TM_FAILED;
				ev.nte_status = wi->xwi_status;
				if (wi->xwi_nbe_ep != NULL) {
					/* must free ep before posting event */
					m0_ref_put(&wi->xwi_nbe_ep->nep_ref);
					wi->xwi_nbe_ep = NULL;
				}
			} else {
				tp->xtm_state = next_state;
				ev.nte_next_state = M0_NET_TM_STARTED;
				ev.nte_ep = wi->xwi_nbe_ep;
				wi->xwi_nbe_ep = NULL;
			}
			m0_mutex_unlock(&tm->ntm_mutex);
			ev.nte_time = m0_time_now();
			m0_net_tm_event_post(&ev);
			m0_mutex_lock(&tm->ntm_mutex);
		}
		if (wi->xwi_nbe_ep != NULL) {
			/* free the end point if not consumed */
			m0_ref_put(&wi->xwi_nbe_ep->nep_ref);
			wi->xwi_nbe_ep = NULL;
		}
	} else { /* M0_NET_XTM_STOPPED, as per assert */
		M0_ASSERT(tp->xtm_state == M0_NET_XTM_STOPPING);
		tp->xtm_state = next_state;
		ev.nte_next_state = M0_NET_TM_STOPPED;

		/* broadcast on cond and wait for work item queue to empty */
		m0_cond_broadcast(&tp->xtm_work_list_cv);
		while (!m0_list_is_empty(&tp->xtm_work_list) ||
		       tp->xtm_callback_counter > 1)
			m0_cond_wait(&tp->xtm_work_list_cv);

		m0_mutex_unlock(&tm->ntm_mutex);
		ev.nte_time = m0_time_now();
		m0_net_tm_event_post(&ev);
		m0_mutex_lock(&tm->ntm_mutex);
	}

	m0_list_link_fini(&wi->xwi_link);
	m0_free(wi);
}

/**
   Deliver a callback during operation cancel.
 */
static void mem_wf_cancel_cb(struct m0_net_transfer_mc *tm,
			     struct m0_net_bulk_mem_work_item *wi)
{
	struct m0_net_buffer *nb = mem_wi_to_buffer(wi);

	M0_PRE(m0_mutex_is_not_locked(&tm->ntm_mutex));
	M0_PRE(nb->nb_flags & M0_NET_BUF_IN_USE);

	/* post the completion callback (will clear operation flags) */
	if ((nb->nb_flags & M0_NET_BUF_TIMED_OUT) != 0)
		wi->xwi_status = -ETIMEDOUT;
	else
		wi->xwi_status = -ECANCELED;
	mem_wi_post_buffer_event(wi);
	return;
}

/**
   Work function for the M0_NET_XOP_ERROR_CB work item.
   @param tm The corresponding transfer machine (unlocked).
   @param wi The work item. This will be freed.
 */
static void mem_wf_error_cb(struct m0_net_transfer_mc *tm,
			    struct m0_net_bulk_mem_work_item *wi)
{
	struct m0_net_tm_event ev = {
		.nte_type   = M0_NET_TEV_ERROR,
		.nte_tm     = tm,
		.nte_status = wi->xwi_status,
	};

	M0_PRE(wi->xwi_op == M0_NET_XOP_ERROR_CB);
	M0_PRE(wi->xwi_status < 0);
	ev.nte_time = m0_time_now();
	m0_net_tm_event_post(&ev);
	m0_free(wi);
}

/**
   Support subroutine to send an error event.
   @param tm     The transfer machine.
   @param status The error code.
   @pre m0_mutex_is_locked(&tm->ntm_mutex)
 */
static void mem_post_error(struct m0_net_transfer_mc *tm, int32_t status)
{
	struct m0_net_bulk_mem_tm_pvt *tp = mem_tm_to_pvt(tm);
	struct m0_net_bulk_mem_work_item *wi;
	M0_PRE(status < 0);
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_ALLOC_PTR(wi);
	if (wi == NULL)
		return;
	wi->xwi_op = M0_NET_XOP_ERROR_CB;
	wi->xwi_status = status;
	mem_wi_add(wi, tp);
}

enum {
	/**
	   Maximum time for condition to wait, avoids hung thread warning
	   in kernel while TM is idle.
	 */
	MEM_XO_TIMEOUT_SECS = 20,
};

/**
   The entry point of the worker thread started when a transfer machine
   transitions from STARTING to STARTED.  The thread runs its main loop
   until it the transfer machine state changes to X2_NET_XTM_STOPPED, at which
   time the function returns, causing the thread to exit.
   @param tm Transfer machine pointer
 */
static void mem_xo_tm_worker(struct m0_net_transfer_mc *tm)
{
	struct m0_net_bulk_mem_tm_pvt *tp;
	struct m0_net_bulk_mem_domain_pvt *dp;
	struct m0_list_link *link;
	struct m0_net_bulk_mem_work_item *wi;
	m0_net_bulk_mem_work_fn_t fn;
	m0_time_t timeout;
	bool rc;

	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(m0_net__tm_invariant(tm));
	tp = mem_tm_to_pvt(tm);
	dp = mem_dom_to_pvt(tm->ntm_dom);

	while (1) {
		while (!m0_list_is_empty(&tp->xtm_work_list)) {
			link = m0_list_first(&tp->xtm_work_list);
			wi = m0_list_entry(link,
					   struct m0_net_bulk_mem_work_item,
					   xwi_link);
			m0_list_del(&wi->xwi_link);
			fn = dp->xd_ops->bmo_work_fn[wi->xwi_op];
			M0_ASSERT(fn != NULL);

			tp->xtm_callback_counter++;
			if (wi->xwi_op == M0_NET_XOP_STATE_CHANGE) {
				fn(tm, wi);
			} else {
				/* others expect mutex to be released
				   and the M0_NET_BUF_IN_USE flag set
				   if a buffer is involved.
				*/
				if (wi->xwi_op != M0_NET_XOP_ERROR_CB) {
					struct m0_net_buffer *nb =
						mem_wi_to_buffer(wi);
					nb->nb_flags |= M0_NET_BUF_IN_USE;
				}
				m0_mutex_unlock(&tm->ntm_mutex);
				fn(tm, wi);
				m0_mutex_lock(&tm->ntm_mutex);
				M0_ASSERT(m0_net__tm_invariant(tm));
			}
			tp->xtm_callback_counter--;
			/* signal that wi was removed from queue */
			m0_cond_signal(&tp->xtm_work_list_cv);
		}
		if (tp->xtm_state == M0_NET_XTM_STOPPED)
			break;
		do {
			timeout = m0_time_from_now(MEM_XO_TIMEOUT_SECS, 0);
			rc = m0_cond_timedwait(&tp->xtm_work_list_cv, timeout);
		} while (!rc);
	}

	m0_mutex_unlock(&tm->ntm_mutex);
}

/**
   @} bulkmem
*/

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
