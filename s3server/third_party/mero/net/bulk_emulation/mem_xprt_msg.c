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

static void mem_wf_msg_recv_cb(struct m0_net_transfer_mc *tm,
			       struct m0_net_bulk_mem_work_item *wi)
{
	struct m0_net_buffer *nb = mem_wi_to_buffer(wi);

	M0_PRE(m0_mutex_is_not_locked(&tm->ntm_mutex));
	M0_PRE(nb != NULL &&
	       nb->nb_qtype == M0_NET_QT_MSG_RECV &&
	       nb->nb_tm == tm &&
	       (wi->xwi_status < 0 ||  /* failed or we have a non-zero msg*/
		(wi->xwi_nbe_ep != NULL && wi->xwi_nbe_length > 0)));
	M0_PRE(nb->nb_flags & M0_NET_BUF_IN_USE);

	/* post the recv completion callback (will clear M0_NET_BUF_IN_USE) */
	mem_wi_post_buffer_event(wi);
	return;
}

/**
   Find a remote TM for a msg send or active buffer operation.

   The m0_net_mutex will be obtained to traverse the list of domains.
   Each domain lock will be held to traverse the transfer machines in the
   domain.
   Once the correct transfer machine is found, its lock is obtained and the
   other locks released.

   @param tm Local TM
   @param match_ep End point of remote TM
   @param p_dest_tm Returns remote TM pointer, with TM mutex held.
   @param p_dest_ep Returns end point in remote TM's domain, with local
   TM's address. Optional - only msg send requires this.
   The option exists because the end point object has
   to be created while holding the remote DOM's mutex.
   @retval 0 On success
   @retval -errno On error
 */
static int mem_find_remote_tm(struct m0_net_transfer_mc  *tm,
			      struct m0_net_end_point    *match_ep,
			      struct m0_net_transfer_mc **p_dest_tm,
			      struct m0_net_end_point   **p_dest_ep)
{
	struct m0_net_domain              *mydom = tm->ntm_dom;
	struct m0_net_transfer_mc         *dest_tm = NULL;
	struct m0_net_end_point           *dest_ep = NULL;
	struct m0_net_bulk_mem_domain_pvt *dp;
	struct m0_net_transfer_mc         *itm;
	struct m0_net_bulk_mem_end_point  *mep;
	int rc = 0;

	M0_PRE(m0_mutex_is_not_locked(&tm->ntm_mutex));
	M0_PRE(m0_mutex_is_not_locked(&mydom->nd_mutex));
	M0_PRE(m0_mutex_is_not_locked(&m0_net_mutex));

	/* iterate over in-mem domains to find the destination TM */

	m0_mutex_lock(&m0_net_mutex);
	m0_list_for_each_entry(&mem_domains, dp,
			       struct m0_net_bulk_mem_domain_pvt,
			       xd_dom_linkage) {
		if (dp->xd_dom == mydom)
			continue; /* skip self */
		/* iterate over TM's in domain */
		m0_mutex_lock(&dp->xd_dom->nd_mutex);
		m0_list_for_each_entry(&dp->xd_dom->nd_tms, itm,
				       struct m0_net_transfer_mc,
				       ntm_dom_linkage) {
			m0_mutex_lock(&itm->ntm_mutex);
			M0_ASSERT(m0_net__tm_invariant(itm));
			do { /* provides break context */
				if (itm->ntm_state != M0_NET_TM_STARTED)
					break; /* ignore */
				if (!mem_eps_are_equal(itm->ntm_ep, match_ep))
					break;

				/* Found the matching TM. */
				dest_tm = itm;
				if (p_dest_ep == NULL)
					break;
				/* We need to create an EP for the local TM
				   address in the remote DOM.
				   We don't need the domain lock (do need TM
				   mutex) but the original logic required it so
				   leave it that way...
				 */
				mep = mem_ep_to_pvt(tm->ntm_ep);
				rc = mem_bmo_ep_create(&dest_ep,
						       dest_tm,
						       &mep->xep_sa,
						       mep->xep_service_id);
			} while(0);
			if (dest_tm != NULL) {
				/* found the TM */
				if (rc != 0) {
					/* ... but failed on EP */
					m0_mutex_unlock(&dest_tm->ntm_mutex);
					dest_tm = NULL;
				}
				break;
			}
			m0_mutex_unlock(&itm->ntm_mutex);
		}
		m0_mutex_unlock(&dp->xd_dom->nd_mutex);
		if (dest_tm != NULL || rc != 0)
			break;
	}
	if (rc == 0 && dest_tm == NULL)
		rc = -ENETUNREACH; /* search exhausted */
	m0_mutex_unlock(&m0_net_mutex);
	M0_ASSERT(rc != 0 ||
		  (dest_tm != NULL &&
		   m0_mutex_is_locked(&dest_tm->ntm_mutex) &&
		   dest_tm->ntm_state == M0_NET_TM_STARTED));
	if (!rc) {
		M0_ASSERT(mem_tm_invariant(dest_tm));
		*p_dest_tm = dest_tm;
		if (p_dest_ep != NULL) {
			M0_ASSERT(dest_ep != NULL);
			*p_dest_ep = dest_ep;
		}
	}
	return M0_RC(rc);
}


/**
   This item involves copying the buffer to the appropriate receive buffer (if
   available) of the target transfer machine in the destination domain, adding
   a M0_NET_XOP_MSG_RECV_CB item on that queue, and then invoking the
   completion callback on the send buffer.
   An end point object describing the sender's address will be referenced
   from the receiving buffer.


   If a suitable transfer machine is not found then the message send fails.
 */
static void mem_wf_msg_send(struct m0_net_transfer_mc *tm,
			    struct m0_net_bulk_mem_work_item *wi)
{
	struct m0_net_buffer      *nb = mem_wi_to_buffer(wi);
	int                        rc;
	struct m0_net_transfer_mc *dest_tm = NULL;
	struct m0_net_end_point   *dest_ep = NULL;
	struct m0_net_buffer      *dest_nb = NULL;

	M0_PRE(nb != NULL &&
	       nb->nb_qtype == M0_NET_QT_MSG_SEND &&
	       nb->nb_tm == tm &&
	       nb->nb_ep != NULL);
	M0_PRE(nb->nb_flags & M0_NET_BUF_IN_USE);

	do {
		bool found_dest_nb = false;
		struct m0_net_bulk_mem_work_item *dest_wi;
		struct m0_net_bulk_mem_tm_pvt *dest_tp;

		/* Search for a remote TM matching the destination address,
		   and if found, create an EP in the remote TM's domain with
		   the local TM's address.
		 */
		rc = mem_find_remote_tm(tm, nb->nb_ep, &dest_tm, &dest_ep);
		if (rc != 0)
			break;

		/* We're now operating in the destination TM while holding
		   its mutex.  The destination TM is operative.
		 */

		/* get the first available receive buffer */
		m0_tl_for(m0_net_tm, &dest_tm->ntm_q[M0_NET_QT_MSG_RECV],
			  dest_nb) {
			if ((dest_nb->nb_flags &
			     (M0_NET_BUF_IN_USE | M0_NET_BUF_CANCELLED)) == 0) {
				found_dest_nb = true;
				break;
			}
		} m0_tl_endfor;
		if (!found_dest_nb) {
			dest_tm->ntm_qstats[M0_NET_QT_MSG_RECV].nqs_num_f_events
				++;
			rc = -ENOBUFS;
			mem_post_error(dest_tm, rc);
			break;
		}
		M0_ASSERT(mem_buffer_invariant(dest_nb));
		M0_ASSERT(dest_nb->nb_flags & M0_NET_BUF_QUEUED);
		dest_nb->nb_flags |= M0_NET_BUF_IN_USE;
		dest_wi = mem_buffer_to_wi(dest_nb);
		if (nb->nb_length > mem_buffer_length(dest_nb)) {
			rc = -EMSGSIZE;
			/* set the desired length in the event */
			dest_wi->xwi_nbe_length = nb->nb_length;
		} else {
			rc = mem_copy_buffer(dest_nb, nb, nb->nb_length);
			dest_wi->xwi_nbe_length = nb->nb_length;
		}
		if (rc == 0) {
			/* commit to using the destination EP */
			dest_wi->xwi_nbe_ep = dest_ep;
			dest_ep = NULL; /* do not release below */
		}
		dest_wi->xwi_status = rc; /* recv error code */

		/* schedule the receive msg callback */
		dest_wi->xwi_op = M0_NET_XOP_MSG_RECV_CB;

		dest_tp = mem_tm_to_pvt(dest_tm);
		mem_wi_add(dest_wi, dest_tp);
	} while (0);

	/* release the destination TM mutex */
	if (dest_tm != NULL)
		m0_mutex_unlock(&dest_tm->ntm_mutex);

	/* release the destination EP, if still referenced,
	   outside of any mutex
	 */
	if (dest_ep != NULL)
		m0_net_end_point_put(dest_ep);

	/* post the send completion callback (will clear M0_NET_BUF_IN_USE) */
	wi->xwi_status = rc;
	mem_wi_post_buffer_event(wi);
	return;
}

/** @} */ /* bulkmem */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
