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
 * Original creation date: 04/05/2011
 */

#include "lib/arith.h" /* max_check */
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/time.h"
#include "lib/misc.h"
#include "lib/finject.h"
#include "mero/magic.h"
#include "net/net_internal.h"
#include "net/addb2.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"

/**
   @addtogroup net
   @{
 */

M0_INTERNAL bool m0_net__qtype_is_valid(enum m0_net_queue_type qt)
{
	return qt >= M0_NET_QT_MSG_RECV && qt < M0_NET_QT_NR;
}

M0_INTERNAL bool m0_net__buffer_invariant(const struct m0_net_buffer *buf)
{
	return
		_0C(buf != NULL) &&
		_0C((buf->nb_flags & M0_NET_BUF_REGISTERED) != 0) &&
		_0C(buf->nb_dom != NULL) &&
		_0C(buf->nb_dom->nd_xprt != NULL) &&
		_0C(buf->nb_buffer.ov_buf != NULL) &&
		_0C(m0_vec_count(&buf->nb_buffer.ov_vec) != 0) &&
		ergo((buf->nb_flags & M0_NET_BUF_QUEUED) != 0,
		     _0C(m0_net__qtype_is_valid(buf->nb_qtype)) &&
		     _0C(buf->nb_callbacks != NULL) &&
		     _0C(buf->nb_callbacks->nbc_cb[buf->nb_qtype] != NULL) &&
		     _0C(buf->nb_tm != NULL) &&
		     _0C(buf->nb_dom == buf->nb_tm->ntm_dom) &&
		     M0_CHECK_EX(_0C(m0_net_tm_tlist_contains( /* expensive */
				  &buf->nb_tm->ntm_q[buf->nb_qtype], buf))));
}

M0_INTERNAL int m0_net_buffer_register(struct m0_net_buffer *buf,
				       struct m0_net_domain *dom)
{
	int rc;

	M0_PRE(dom != NULL);
	M0_PRE(dom->nd_xprt != NULL);

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EINVAL);

	m0_mutex_lock(&dom->nd_mutex);

	M0_PRE_EX(buf != NULL &&
	       buf->nb_dom == NULL &&
	       buf->nb_flags == 0 &&
	       buf->nb_buffer.ov_buf != NULL &&
	       m0_vec_count(&buf->nb_buffer.ov_vec) > 0);

	buf->nb_dom = dom;
	buf->nb_xprt_private = NULL;
	buf->nb_timeout = M0_TIME_NEVER;
	buf->nb_magic = M0_NET_BUFFER_LINK_MAGIC;

	/*
	 * The transport will validate buffer size and number of segments, and
	 * optimize it for future use.
	 */
	rc = dom->nd_xprt->nx_ops->xo_buf_register(buf);
	if (rc == 0) {
		buf->nb_flags |= M0_NET_BUF_REGISTERED;
		m0_list_add_tail(&dom->nd_registered_bufs,&buf->nb_dom_linkage);
	}

	M0_POST_EX(ergo(rc == 0, m0_net__buffer_invariant(buf)));
	M0_POST(ergo(rc == 0, buf->nb_timeout == M0_TIME_NEVER));

	m0_mutex_unlock(&dom->nd_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_buffer_register);

M0_INTERNAL void m0_net_buffer_deregister(struct m0_net_buffer *buf,
					  struct m0_net_domain *dom)
{
	M0_PRE(dom != NULL);
	M0_PRE(dom->nd_xprt != NULL);
	m0_mutex_lock(&dom->nd_mutex);

	M0_PRE_EX(m0_net__buffer_invariant(buf) && buf->nb_dom == dom);
	M0_PRE(buf->nb_flags == M0_NET_BUF_REGISTERED);
	M0_PRE_EX(m0_list_contains(&dom->nd_registered_bufs,
				   &buf->nb_dom_linkage));

	dom->nd_xprt->nx_ops->xo_buf_deregister(buf);
	buf->nb_flags &= ~M0_NET_BUF_REGISTERED;
	m0_list_del(&buf->nb_dom_linkage);
	buf->nb_xprt_private = NULL;
	buf->nb_magic = 0;
	buf->nb_dom = NULL;

	m0_mutex_unlock(&dom->nd_mutex);
}
M0_EXPORTED(m0_net_buffer_deregister);

M0_INTERNAL int m0_net__buffer_add(struct m0_net_buffer *buf,
				   struct m0_net_transfer_mc *tm)
{
	int rc;
	struct m0_net_domain *dom;
	struct m0_tl	     *ql;
	struct buf_add_checks {
		bool check_length;
		bool check_ep;
		bool check_desc;
		bool post_check_desc;
	};
	static const struct buf_add_checks checks[M0_NET_QT_NR] = {
		[M0_NET_QT_MSG_RECV]          = { false, false, false, false },
		[M0_NET_QT_MSG_SEND]          = { true,  true,  false, false },
		[M0_NET_QT_PASSIVE_BULK_RECV] = { false, false, false, true  },
		[M0_NET_QT_PASSIVE_BULK_SEND] = { true,  false, false, true  },
		[M0_NET_QT_ACTIVE_BULK_RECV]  = { false, false, true,  false },
		[M0_NET_QT_ACTIVE_BULK_SEND]  = { true,  false, true,  false }
	};
	const struct buf_add_checks *todo;
	m0_bcount_t count = m0_vec_count(&buf->nb_buffer.ov_vec);
	m0_time_t   now   = m0_time_now();

	M0_PRE(tm != NULL);
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_PRE_EX(m0_net__tm_invariant(tm));
	M0_PRE_EX(m0_net__buffer_invariant(buf));
	M0_PRE(buf->nb_dom == tm->ntm_dom);

	dom = tm->ntm_dom;
	M0_PRE(dom->nd_xprt != NULL);

	M0_PRE(!(buf->nb_flags &
	       (M0_NET_BUF_QUEUED | M0_NET_BUF_IN_USE | M0_NET_BUF_CANCELLED |
		M0_NET_BUF_TIMED_OUT | M0_NET_BUF_RETAIN)));
	M0_PRE(ergo(buf->nb_qtype == M0_NET_QT_MSG_RECV,
		    buf->nb_ep == NULL &&
		    buf->nb_min_receive_size != 0 &&
		    buf->nb_min_receive_size <= count &&
		    buf->nb_max_receive_msgs != 0));
	M0_PRE(tm->ntm_state == M0_NET_TM_STARTED);

	/* determine what to do by queue type */
	todo = &checks[buf->nb_qtype];
	ql = &tm->ntm_q[buf->nb_qtype];

	/*
	 * Validate that the length is set and is within buffer bounds. The
	 * transport will make other checks on the buffer, such as the max size
	 * and number of segments.
	 */
	M0_PRE(ergo(todo->check_length, buf->nb_length > 0 &&
		    (buf->nb_length + buf->nb_offset) <= count));

	/* validate end point usage; increment ref count later */
	M0_PRE(ergo(todo->check_ep,
		    buf->nb_ep != NULL &&
		    m0_net__ep_invariant(buf->nb_ep, tm, true)));

	/* validate that the descriptor is present */
	if (todo->post_check_desc) {
		/** @todo should be m0_net_desc_free()? */
		buf->nb_desc.nbd_len = 0;
		buf->nb_desc.nbd_data = NULL;
	}
	M0_PRE(ergo(todo->check_desc,
		    buf->nb_desc.nbd_len > 0 &&
		    buf->nb_desc.nbd_data != NULL));

	/* validate that a timeout, if set, is in the future */
	if (buf->nb_timeout != M0_TIME_NEVER) {
		/* Don't want to assert here as scheduling is unpredictable. */
		if (now >= buf->nb_timeout) {
			rc = -ETIME; /* not -ETIMEDOUT */
			goto m_err_exit;
		}
	}

	/*
	 * Optimistically add it to the queue's list before calling the xprt.
	 * Post will unlink on completion, or del on cancel.
	 */
	m0_net_tm_tlink_init_at_tail(buf, ql);
	buf->nb_flags |= M0_NET_BUF_QUEUED;
	buf->nb_add_time = now; /* record time added */
	buf->nb_msgs_received = 0;

	/* call the transport */
	buf->nb_tm = tm;
	rc = dom->nd_xprt->nx_ops->xo_buf_add(buf);
	if (rc != 0) {
		m0_net_tm_tlink_del_fini(buf);
		buf->nb_flags &= ~M0_NET_BUF_QUEUED;
		goto m_err_exit;
	}

	tm->ntm_qstats[buf->nb_qtype].nqs_num_adds += 1;

	if (todo->check_ep) {
		/* Bump the reference count.
		   Should be decremented in m0_net_buffer_event_post().
		   The caller holds a reference to the end point.
		 */
		m0_net_end_point_get(buf->nb_ep);
	}

	M0_POST(ergo(todo->post_check_desc,
		     buf->nb_desc.nbd_len != 0 &&
		     buf->nb_desc.nbd_data != NULL));
	M0_POST_EX(m0_net__buffer_invariant(buf));
	M0_POST_EX(m0_net__tm_invariant(tm));

 m_err_exit:
	return M0_RC(rc);
}

M0_INTERNAL int m0_net_buffer_add(struct m0_net_buffer *buf,
				  struct m0_net_transfer_mc *tm)
{
	int rc;
	M0_PRE(tm != NULL);
	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EMSGSIZE);
	m0_mutex_lock(&tm->ntm_mutex);
	rc = m0_net__buffer_add(buf, tm);
	m0_mutex_unlock(&tm->ntm_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_buffer_add);

M0_INTERNAL bool m0_net_buffer_del(struct m0_net_buffer *buf,
				   struct m0_net_transfer_mc *tm)
{
	struct m0_net_domain *dom;
	bool                  rc = true;

	M0_PRE(tm != NULL && tm->ntm_dom != NULL);
	M0_PRE(buf != NULL);

	dom = tm->ntm_dom;
	M0_PRE(dom->nd_xprt != NULL);
	m0_mutex_lock(&tm->ntm_mutex);

	M0_PRE_EX(m0_net__buffer_invariant(buf));
	M0_PRE(buf->nb_tm == NULL || buf->nb_tm == tm);

	if (!(buf->nb_flags & M0_NET_BUF_QUEUED)) {
		/* completion race condition? no error */
		rc = false;
		goto m_err_exit;
	}

	/* tell the transport to cancel */
	dom->nd_xprt->nx_ops->xo_buf_del(buf);

	tm->ntm_qstats[buf->nb_qtype].nqs_num_dels += 1;

	M0_POST_EX(m0_net__buffer_invariant(buf));
	M0_POST_EX(m0_net__tm_invariant(tm));
 m_err_exit:
	m0_mutex_unlock(&tm->ntm_mutex);

	return rc;
}
M0_EXPORTED(m0_net_buffer_del);

M0_INTERNAL bool m0_net__buffer_event_invariant(const struct m0_net_buffer_event
						*ev)
{
	int32_t  status = ev->nbe_status;
	uint64_t flags  = ev->nbe_buffer->nb_qtype;

	return
		_0C(status <= 0) &&
		_0C(ergo(status == 0 &&
			 ev->nbe_buffer->nb_qtype == M0_NET_QT_MSG_RECV,
			 /* don't check ep invariant here */
			 ev->nbe_ep != NULL)) &&
		_0C(ergo(flags & M0_NET_BUF_CANCELLED, status == -ECANCELED)) &&
		_0C(ergo(flags & M0_NET_BUF_TIMED_OUT, status == -ETIMEDOUT)) &&
		_0C(ergo(flags & M0_NET_BUF_RETAIN,    status == 0));
}

M0_INTERNAL void m0_net_buffer_event_post(const struct m0_net_buffer_event *ev)
{
	struct m0_net_buffer      *buf = NULL;
	struct m0_net_end_point   *ep;
	bool                       check_ep;
	bool                       retain;
	enum m0_net_queue_type	   qtype = M0_NET_QT_NR;
	struct m0_net_transfer_mc *tm;
	struct m0_net_qstats      *q;
	m0_time_t                  tdiff;
	m0_time_t                  addtime;
	m0_net_buffer_cb_proc_t    cb;
	m0_bcount_t                len = 0;
	struct m0_net_buffer_pool *pool = NULL;
	uint64_t                   flags;

	M0_PRE(m0_net__buffer_event_invariant(ev));
	buf = ev->nbe_buffer;
	tm  = buf->nb_tm;
	M0_PRE(m0_mutex_is_not_locked(&tm->ntm_mutex));

	/*
	 * pre-callback, in mutex:
	 * update buffer (if present), state and statistics
	 */
	m0_mutex_lock(&tm->ntm_mutex);
	flags = buf->nb_flags;
	M0_PRE_EX(m0_net__tm_invariant(tm));
	M0_PRE_EX(m0_net__buffer_invariant(buf));
	M0_PRE(flags & M0_NET_BUF_QUEUED);

	retain = flags & M0_NET_BUF_RETAIN;
	if (retain) {
		flags &= ~M0_NET_BUF_RETAIN;
	} else {
		m0_net_tm_tlist_del(buf);
		flags &= ~(M0_NET_BUF_QUEUED | M0_NET_BUF_CANCELLED |
				   M0_NET_BUF_IN_USE | M0_NET_BUF_TIMED_OUT);
		buf->nb_timeout = M0_TIME_NEVER;
	}

	qtype = buf->nb_qtype;
	q = &tm->ntm_qstats[qtype];
	if (ev->nbe_status < 0) {
		q->nqs_num_f_events++;
		len = 0; /* length not counted on failure */
	} else {
		q->nqs_num_s_events++;
		if (M0_IN(qtype, (M0_NET_QT_MSG_RECV,
				  M0_NET_QT_PASSIVE_BULK_RECV,
				  M0_NET_QT_ACTIVE_BULK_RECV)))
			len = ev->nbe_length;
		else
			len = buf->nb_length;
	}
	addtime = buf->nb_add_time;
	tdiff = m0_time_sub(ev->nbe_time, addtime);
	if (!retain)
		q->nqs_time_in_queue = m0_time_add(q->nqs_time_in_queue, tdiff);
	q->nqs_total_bytes += len;
	q->nqs_max_bytes = max_check(q->nqs_max_bytes, len);

	ep = NULL;
	check_ep = false;
	switch (qtype) {
	case M0_NET_QT_MSG_RECV:
		if (ev->nbe_status == 0) {
			check_ep = true;
			ep = ev->nbe_ep; /* from event */
			++buf->nb_msgs_received;
		}
		if (!retain && tm->ntm_state == M0_NET_TM_STARTED &&
		    tm->ntm_recv_pool != NULL)
			pool = tm->ntm_recv_pool;
		break;
	case M0_NET_QT_MSG_SEND:
		/* must put() ep to match get in buffer_add() */
		ep = buf->nb_ep;   /* from buffer */
		break;
	default:
		break;
	}

	if (check_ep) {
		M0_ASSERT(m0_net__ep_invariant(ep, tm, true));
	}
	cb = buf->nb_callbacks->nbc_cb[qtype];
	M0_CNT_INC(tm->ntm_callback_counter);
	buf->nb_flags = flags;
	m0_mutex_unlock(&tm->ntm_mutex);
	if (pool != NULL && !retain)
		m0_net__tm_provision_recv_q(tm);
	cb(ev);
	M0_ADDB2_ADD(M0_AVI_NET_BUF, (uint64_t)buf, qtype,
		     addtime, tdiff, ev->nbe_status, len);
	/* Decrement the reference to the ep */
	if (ep != NULL)
		m0_net_end_point_put(ep);
	m0_net__tm_post_callback(tm);
}

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
