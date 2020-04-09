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
 * Original creation date: 04/06/2010
 */

#include "lib/types.h"
#include "lib/arith.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/mutex.h"
#include "ut/ut.h"

#include "net/net_internal.h"

static struct m0_net_domain utdom;
static struct m0_net_transfer_mc ut_tm;
static struct m0_net_transfer_mc *ut_evt_tm = &ut_tm;

static void make_desc(struct m0_net_buf_desc *desc);

#if 0
#define KPRN(fmt,...) printk(KERN_ERR fmt, ## __VA_ARGS__)
#else
#define KPRN(fmt,...)
#endif

#define DELAY_MS(ms) m0_nanosleep(m0_time(0, (ms) * 1000000ULL), NULL)

/*
 *****************************************************
   Fake transport for the UT
 *****************************************************
*/
static struct {
	int num;
} ut_xprt_pvt;

static bool ut_dom_init_called = false;
static bool ut_dom_fini_called = false;
static bool ut_get_max_buffer_segment_size_called = false;
static bool ut_get_max_buffer_size_called = false;
static bool ut_get_max_buffer_segments_called = false;
static bool ut_end_point_create_called = false;
static bool ut_end_point_release_called = false;

static int ut_dom_init(struct m0_net_xprt *xprt,
		       struct m0_net_domain *dom)
{
	M0_ASSERT(m0_mutex_is_locked(&m0_net_mutex));
	M0_ASSERT(m0_mutex_is_not_locked(&dom->nd_mutex));
	ut_get_max_buffer_size_called = false;
	ut_get_max_buffer_segment_size_called = false;
	ut_get_max_buffer_segments_called = false;
	ut_end_point_create_called = false;
	ut_end_point_release_called = false;
	ut_dom_fini_called = false;
	ut_dom_init_called = true;
	dom->nd_xprt_private = &ut_xprt_pvt;
	return 0;
}

static void ut_dom_fini(struct m0_net_domain *dom)
{
	M0_ASSERT(m0_mutex_is_locked(&m0_net_mutex));
	M0_ASSERT(m0_mutex_is_not_locked(&dom->nd_mutex));
	ut_dom_fini_called = true;
}

/* params */
enum {
	UT_MAX_BUF_SIZE = 8192,
	UT_MAX_BUF_SEGMENT_SIZE = 2048,
	UT_MAX_BUF_SEGMENTS = 4,
	UT_MAX_BUF_DESC_SIZE = 32,
};

static m0_bcount_t ut_get_max_buffer_size(const struct m0_net_domain *dom)
{
	ut_get_max_buffer_size_called = true;
	return UT_MAX_BUF_SIZE;
}

static m0_bcount_t ut_get_max_buffer_segment_size(const struct m0_net_domain
						  *dom)
{
	ut_get_max_buffer_segment_size_called = true;
	return UT_MAX_BUF_SEGMENT_SIZE;
}

static int32_t ut_get_max_buffer_segments(const struct m0_net_domain *dom)
{
	ut_get_max_buffer_segments_called = true;
	return UT_MAX_BUF_SEGMENTS;
}

static m0_bcount_t ut_get_max_buffer_desc_size(const struct m0_net_domain *dom)
{
	return UT_MAX_BUF_DESC_SIZE;
}

struct ut_ep {
	char *addr;
	struct m0_net_end_point uep;
};

static struct m0_net_end_point *ut_last_ep_released;
static void ut_end_point_release(struct m0_ref *ref)
{
	struct m0_net_end_point *ep;
	struct ut_ep *utep;
	struct m0_net_transfer_mc *tm;
	ut_end_point_release_called = true;
	ep = container_of(ref, struct m0_net_end_point, nep_ref);
	ut_last_ep_released = ep;
	tm = ep->nep_tm;
	M0_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	m0_nep_tlist_del(ep);
	ep->nep_tm = NULL;
	utep = container_of(ep, struct ut_ep, uep);
	m0_free(utep);
}

static int ut_end_point_create(struct m0_net_end_point **epp,
			       struct m0_net_transfer_mc *tm,
			       const char *addr)
{
	char *ap;
	struct ut_ep *utep;
	struct m0_net_end_point *ep;

	M0_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	ut_end_point_create_called = true;
	if (addr == NULL) {
		/* don't support dynamic */
		return -ENOSYS;
	}
	ap = (char *)addr;  /* avoid strdup; this is a ut! */
	/* check if its already on the domain list */
	m0_tl_for(m0_nep, &tm->ntm_end_points, ep) {
		utep = container_of(ep, struct ut_ep, uep);
		if (strcmp(utep->addr, ap) == 0) {
			m0_ref_get(&ep->nep_ref); /* refcnt++ */
			*epp = ep;
			return 0;
		}
	} m0_tl_endfor;
	/* allocate a new end point */
	M0_ALLOC_PTR(utep);
	utep->addr = ap;
	utep->uep.nep_addr = ap;
	m0_ref_init(&utep->uep.nep_ref, 1, ut_end_point_release);
	utep->uep.nep_tm = tm;
	m0_nep_tlink_init_at_tail(&utep->uep, &tm->ntm_end_points);
	*epp = &utep->uep;
	return 0;
}

static bool ut_buf_register_called = false;
static int ut_buf_register(struct m0_net_buffer *nb)
{
	M0_ASSERT(m0_mutex_is_locked(&nb->nb_dom->nd_mutex));
	ut_buf_register_called = true;
	return 0;
}

static bool ut_buf_deregister_called = false;
static void ut_buf_deregister(struct m0_net_buffer *nb)
{
	M0_ASSERT(m0_mutex_is_locked(&nb->nb_dom->nd_mutex));
	ut_buf_deregister_called = true;
	return;
}

static bool ut_buf_add_called = false;
static int ut_buf_add(struct m0_net_buffer *nb)
{
	M0_UT_ASSERT(m0_mutex_is_locked(&nb->nb_tm->ntm_mutex));
	M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_IN_USE));
	switch (nb->nb_qtype) {
	case M0_NET_QT_PASSIVE_BULK_RECV:
	case M0_NET_QT_PASSIVE_BULK_SEND:
		/* passive bulk ops required to set nb_desc */
		make_desc(&nb->nb_desc);
		break;
	default:
		break;
	}
	ut_buf_add_called = true;
	return 0;
}

struct m0_thread ut_del_thread;
static void ut_post_del_thread(struct m0_net_buffer *nb)
{
	struct m0_net_buffer_event ev = {
		.nbe_buffer = nb,
		.nbe_status = 0,
	};
	if (nb->nb_flags & M0_NET_BUF_CANCELLED)
		ev.nbe_status = -ECANCELED; /* required behavior */
	DELAY_MS(1);
	ev.nbe_time = m0_time_now();

	/* post requested event */
	m0_net_buffer_event_post(&ev);
}

static bool ut_buf_del_called = false;
static void ut_buf_del(struct m0_net_buffer *nb)
{
	int rc;
	M0_SET0(&ut_del_thread);
	M0_UT_ASSERT(m0_mutex_is_locked(&nb->nb_tm->ntm_mutex));
	ut_buf_del_called = true;
	if (!(nb->nb_flags & M0_NET_BUF_IN_USE))
		nb->nb_flags |= M0_NET_BUF_CANCELLED;
	rc = M0_THREAD_INIT(&ut_del_thread, struct m0_net_buffer *, NULL,
			    &ut_post_del_thread, nb, "ut_post_del");
	M0_UT_ASSERT(rc == 0);
	return;
}

struct ut_tm_pvt {
	struct m0_net_transfer_mc *tm;
};
static bool ut_tm_init_called = false;
static int ut_tm_init(struct m0_net_transfer_mc *tm)
{
	struct ut_tm_pvt *tmp;
	M0_ASSERT(m0_mutex_is_locked(&tm->ntm_dom->nd_mutex));
	M0_ALLOC_PTR(tmp);
	tmp->tm = tm;
	tm->ntm_xprt_private = tmp;
	ut_tm_init_called = true;
	return 0;
}

static bool ut_tm_fini_called = false;
static void ut_tm_fini(struct m0_net_transfer_mc *tm)
{
	struct ut_tm_pvt *tmp;
	M0_ASSERT(m0_mutex_is_locked(&tm->ntm_dom->nd_mutex));
	tmp = tm->ntm_xprt_private;
	M0_UT_ASSERT(tmp->tm == tm);
	m0_free(tmp);
	ut_tm_fini_called = true;
	return;
}

static struct m0_thread ut_tm_thread;
static void ut_post_tm_started_ev_thread(struct m0_net_end_point *ep)
{
	struct m0_net_tm_event ev = {
		.nte_type = M0_NET_TEV_STATE_CHANGE,
		.nte_tm = ut_evt_tm,
		.nte_ep = ep,
		.nte_status = 0,
		.nte_next_state = M0_NET_TM_STARTED
	};
	DELAY_MS(1);
	ev.nte_time = m0_time_now();

	/* post state change event */
	m0_net_tm_event_post(&ev);
}

static void ut_post_state_change_ev_thread(int n)
{
	struct m0_net_tm_event ev = {
		.nte_type = M0_NET_TEV_STATE_CHANGE,
		.nte_tm = ut_evt_tm,
		.nte_status = 0,
		.nte_next_state = (enum m0_net_tm_state) n
	};
	DELAY_MS(1);
	ev.nte_time = m0_time_now();

	/* post state change event */
	m0_net_tm_event_post(&ev);
}

static bool ut_tm_start_called = false;
static int ut_tm_start(struct m0_net_transfer_mc *tm, const char *addr)
{
	int rc;
	struct m0_net_xprt *xprt;
	struct m0_net_end_point *ep;

	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	ut_tm_start_called = true;
	ut_evt_tm = tm;

	/* create the end point (indirectly via the transport ops vector) */
	xprt = tm->ntm_dom->nd_xprt;
	rc = (*xprt->nx_ops->xo_end_point_create)(&ep, tm, addr);
	if (rc != 0)
		return rc;

	/* create bg thread to post start state change event.
	   cannot do it here: we are in tm lock, post would assert.
	 */
	M0_SET0(&ut_tm_thread);
	rc = M0_THREAD_INIT(&ut_tm_thread, struct m0_net_end_point *, NULL,
			    &ut_post_tm_started_ev_thread, ep,
			    "state_change%d", M0_NET_TM_STARTED);
	M0_UT_ASSERT(rc == 0);
	return rc;
}

static bool ut_tm_stop_called = false;
static int ut_tm_stop(struct m0_net_transfer_mc *tm, bool cancel)
{
	int rc;

	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	ut_tm_stop_called = true;
	ut_evt_tm = tm;
	M0_SET0(&ut_tm_thread);
	rc = M0_THREAD_INIT(&ut_tm_thread, int, NULL,
			    &ut_post_state_change_ev_thread, M0_NET_TM_STOPPED,
			    "state_change%d", M0_NET_TM_STOPPED);
	M0_UT_ASSERT(rc == 0);
	return rc;
}

static bool ut_tm_confine_called = false;
static const struct m0_bitmap *ut_tm_confine_bm;
static int ut_tm_confine(struct m0_net_transfer_mc *tm,
			 const struct m0_bitmap *processors)
{
	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	ut_tm_confine_called = true;
	ut_tm_confine_bm = processors;
	return 0;
}

static bool ut_bev_deliver_sync_called = false;
static int ut_bev_deliver_sync(struct m0_net_transfer_mc *tm)
{
	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	ut_bev_deliver_sync_called = true;
	return 0;
}

static bool ut_bev_deliver_all_called = false;
static void ut_bev_deliver_all(struct m0_net_transfer_mc *tm)
{
	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	ut_bev_deliver_all_called = true;
	return;
}

static bool ut_bev_pending_called = false;
static int ut_bev_pending_last = 1;
static bool ut_bev_pending(struct m0_net_transfer_mc *tm)
{
	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	ut_bev_pending_called = true;
	ut_bev_pending_last = 1 - ut_bev_pending_last;
	return (bool)ut_bev_pending_last;
}

static bool ut_bev_notify_called = false;
static void ut_bev_notify(struct m0_net_transfer_mc *tm, struct m0_chan *chan)
{
	M0_UT_ASSERT(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_UT_ASSERT(chan == &tm->ntm_chan);
	ut_bev_notify_called = true;
	return;
}

static struct m0_net_xprt_ops ut_xprt_ops = {
	.xo_dom_init                    = ut_dom_init,
	.xo_dom_fini                    = ut_dom_fini,
	.xo_get_max_buffer_size         = ut_get_max_buffer_size,
	.xo_get_max_buffer_segment_size = ut_get_max_buffer_segment_size,
	.xo_get_max_buffer_segments     = ut_get_max_buffer_segments,
	.xo_get_max_buffer_desc_size    = ut_get_max_buffer_desc_size,
	.xo_end_point_create            = ut_end_point_create,
	.xo_buf_register                = ut_buf_register,
	.xo_buf_deregister              = ut_buf_deregister,
	.xo_buf_add                     = ut_buf_add,
	.xo_buf_del                     = ut_buf_del,
	.xo_tm_init                     = ut_tm_init,
	.xo_tm_fini                     = ut_tm_fini,
	.xo_tm_start                    = ut_tm_start,
	.xo_tm_stop                     = ut_tm_stop,
	/* define at runtime .xo_tm_confine       */
	/* define at runtime .xo_bev_deliver_sync */
        .xo_bev_deliver_all             = ut_bev_deliver_all,
	.xo_bev_pending                 = ut_bev_pending,
	.xo_bev_notify                  = ut_bev_notify
};

static struct m0_net_xprt ut_xprt = {
	.nx_name = "ut/bulk",
	.nx_ops  = &ut_xprt_ops
};

/* utility subs */
static struct m0_net_buffer *
allocate_buffers(m0_bcount_t buf_size,
		 m0_bcount_t buf_seg_size,
		 int32_t buf_segs)
{
	int rc;
	int i;
	struct m0_net_buffer *nbs;
	struct m0_net_buffer *nb;
	m0_bcount_t sz;
	int32_t nr;

	M0_ALLOC_ARR(nbs, M0_NET_QT_NR);
	for (i = 0; i < M0_NET_QT_NR; ++i) {
		nb = &nbs[i];
		M0_SET0(nb);
		nr = buf_segs;
		if ((buf_size / buf_segs) > buf_seg_size) {
			sz = buf_seg_size;
			M0_ASSERT((sz * nr) <= buf_size);
		} else {
			sz = buf_size/buf_segs;
		}
		rc = m0_bufvec_alloc(&nb->nb_buffer, nr, sz);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(nb->nb_buffer.ov_vec.v_nr == nr);
		M0_UT_ASSERT(m0_vec_count(&nb->nb_buffer.ov_vec) == (sz * nr));
	}

	return nbs;
}

static void make_desc(struct m0_net_buf_desc *desc)
{
	static const char *p = "descriptor";
	size_t len = strlen(p)+1;
	desc->nbd_data = m0_alloc(len);
	desc->nbd_len = len;
	strcpy((char *)desc->nbd_data, p);
}

/* callback subs */
static int ut_cb_calls[M0_NET_QT_NR];
static uint64_t num_adds[M0_NET_QT_NR];
static uint64_t num_dels[M0_NET_QT_NR];
static m0_bcount_t total_bytes[M0_NET_QT_NR];
static m0_bcount_t max_bytes[M0_NET_QT_NR];

static void ut_buffer_event_callback(const struct m0_net_buffer_event *ev,
				     enum m0_net_queue_type qt,
				     bool queue_check)
{
	m0_bcount_t len = 0;
	M0_UT_ASSERT(ev->nbe_buffer != NULL);
	M0_UT_ASSERT(ev->nbe_buffer->nb_qtype == qt);
	ut_cb_calls[qt]++;
	if (queue_check)
		M0_UT_ASSERT(!(ev->nbe_buffer->nb_flags & M0_NET_BUF_QUEUED));
	/* Collect stats to test the q stats.
	   Length counted only on success.
	   Receive buffer lengths are in the event.
	*/
	if (qt == M0_NET_QT_MSG_RECV ||
	    qt == M0_NET_QT_PASSIVE_BULK_RECV ||
	    qt == M0_NET_QT_ACTIVE_BULK_RECV) {
		/* assert that the buffer length not set by the API */
		M0_UT_ASSERT(ev->nbe_buffer->nb_length == 0);
		if (ev->nbe_status == 0) {
			len = ev->nbe_length;
			M0_UT_ASSERT(len != 0);
			ev->nbe_buffer->nb_length = ev->nbe_length;
		}
	} else {
		if (ev->nbe_status == 0) {
			len = ev->nbe_buffer->nb_length;
		}
	}
	if (qt == M0_NET_QT_MSG_RECV || qt == M0_NET_QT_MSG_SEND) {
		M0_UT_ASSERT(ev->nbe_buffer->nb_desc.nbd_len == 0);
	} else if (ev->nbe_status == 0 &&
		   !(ev->nbe_buffer->nb_flags & M0_NET_BUF_QUEUED)) {
		M0_UT_ASSERT(ev->nbe_buffer->nb_desc.nbd_len > 0);
		m0_net_desc_free(&ev->nbe_buffer->nb_desc);
	}

	total_bytes[qt] += len;
	max_bytes[qt] = max64u(ev->nbe_buffer->nb_length,max_bytes[qt]);
}

#define UT_CB_CALL(_qt) ut_buffer_event_callback(ev, _qt, true)
static void ut_msg_recv_cb(const struct m0_net_buffer_event *ev)
{
	UT_CB_CALL(M0_NET_QT_MSG_RECV);
}

static void ut_msg_send_cb(const struct m0_net_buffer_event *ev)
{
	UT_CB_CALL(M0_NET_QT_MSG_SEND);
}

static void ut_passive_bulk_recv_cb(const struct m0_net_buffer_event *ev)
{
	UT_CB_CALL(M0_NET_QT_PASSIVE_BULK_RECV);
}

static void ut_passive_bulk_send_cb(const struct m0_net_buffer_event *ev)
{
	UT_CB_CALL(M0_NET_QT_PASSIVE_BULK_SEND);
}

static void ut_active_bulk_recv_cb(const struct m0_net_buffer_event *ev)
{
	UT_CB_CALL(M0_NET_QT_ACTIVE_BULK_RECV);
}

static void ut_active_bulk_send_cb(const struct m0_net_buffer_event *ev)
{
	UT_CB_CALL(M0_NET_QT_ACTIVE_BULK_SEND);
}

static bool ut_multi_use_expect_queued;
static struct m0_net_buffer *ut_multi_use_got_buf;
static void ut_multi_use_cb(const struct m0_net_buffer_event *ev)
{
	M0_UT_ASSERT(ev->nbe_buffer != NULL);
	if (ut_multi_use_expect_queued) {
		M0_UT_ASSERT(ev->nbe_buffer->nb_flags & M0_NET_BUF_QUEUED);
	} else {
		M0_UT_ASSERT(!(ev->nbe_buffer->nb_flags & M0_NET_BUF_QUEUED));
	}
	ut_multi_use_got_buf = ev->nbe_buffer;
	ut_buffer_event_callback(ev, ev->nbe_buffer->nb_qtype, false);
}

static int ut_tm_event_cb_calls = 0;
void ut_tm_event_cb(const struct m0_net_tm_event *ev)
{
	ut_tm_event_cb_calls++;
}

/* UT transfer machine */
struct m0_net_tm_callbacks ut_tm_cb = {
	.ntc_event_cb = ut_tm_event_cb
};

struct m0_net_buffer_callbacks ut_buf_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]          = ut_msg_recv_cb,
		[M0_NET_QT_MSG_SEND]          = ut_msg_send_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = ut_passive_bulk_recv_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = ut_passive_bulk_send_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = ut_active_bulk_recv_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = ut_active_bulk_send_cb
	},
};

struct m0_net_buffer_callbacks ut_buf_multi_use_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]          = ut_multi_use_cb,
		[M0_NET_QT_MSG_SEND]          = ut_multi_use_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = ut_multi_use_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = ut_multi_use_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = ut_multi_use_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = ut_multi_use_cb,
	},
};

static struct m0_net_transfer_mc ut_tm = {
	.ntm_callbacks = &ut_tm_cb,
	.ntm_state = M0_NET_TM_UNDEFINED
};

/*
  Unit test starts
 */
static void test_net_bulk_if(void)
{
	int rc, i, reuse_cnt;
	bool brc;
	m0_bcount_t buf_size, buf_seg_size;
	int32_t   buf_segs;
	struct m0_net_domain *dom = &utdom;
	struct m0_net_transfer_mc *tm = &ut_tm;
	struct m0_net_buffer *nbs;
	struct m0_net_buffer *nb;
	struct m0_net_end_point *ep1, *ep2, *ep;
	struct m0_net_buf_desc d1, d2;
	struct m0_clink tmwait;
	struct m0_net_qstats qs[M0_NET_QT_NR];
	m0_time_t m0tt_to_period;
	struct m0_bitmap *procmask = (void *) -1; /* fake not null UT value */
	enum { NUM_REUSES = 2 };

	M0_SET0(&d1);
	M0_SET0(&d2);
	make_desc(&d1);
	M0_UT_ASSERT(d1.nbd_data != NULL);
	M0_UT_ASSERT(d1.nbd_len > 0);
	rc = m0_net_desc_copy(&d1, &d2);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(d2.nbd_data != NULL);
	M0_UT_ASSERT(d2.nbd_len > 0);
	M0_UT_ASSERT(d1.nbd_data != d2.nbd_data);
	M0_UT_ASSERT(d1.nbd_len == d2.nbd_len);
	M0_UT_ASSERT(memcmp(d1.nbd_data, d2.nbd_data, d1.nbd_len) == 0);
	m0_net_desc_free(&d2);
	M0_UT_ASSERT(d2.nbd_data == NULL);
	M0_UT_ASSERT(d2.nbd_len == 0);
	m0_net_desc_free(&d1);

	/* initialize the domain */
	M0_UT_ASSERT(ut_dom_init_called == false);
	/* get max buffer size */
	M0_UT_ASSERT(ut_get_max_buffer_size_called == false);
	buf_size = 0;
	/* get max buffer segment size */
	M0_UT_ASSERT(ut_get_max_buffer_segment_size_called == false);
	buf_seg_size = 0;
	/* get max buffer segments */
	M0_UT_ASSERT(ut_get_max_buffer_segments_called == false);
	buf_segs = 0;
	rc = m0_net_domain_init(dom, &ut_xprt);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_dom_init_called);
	M0_UT_ASSERT(dom->nd_xprt == &ut_xprt);
	M0_UT_ASSERT(dom->nd_xprt_private == &ut_xprt_pvt);
	M0_ASSERT(m0_mutex_is_not_locked(&dom->nd_mutex));

	buf_size = m0_net_domain_get_max_buffer_size(dom);
	M0_UT_ASSERT(ut_get_max_buffer_size_called);
	M0_ASSERT(m0_mutex_is_not_locked(&dom->nd_mutex));
	M0_UT_ASSERT(buf_size == UT_MAX_BUF_SIZE);

	buf_seg_size = m0_net_domain_get_max_buffer_segment_size(dom);
	M0_UT_ASSERT(ut_get_max_buffer_segment_size_called);
	M0_ASSERT(m0_mutex_is_not_locked(&dom->nd_mutex));
	M0_UT_ASSERT(buf_seg_size == UT_MAX_BUF_SEGMENT_SIZE);

	buf_segs = m0_net_domain_get_max_buffer_segments(dom);
	M0_UT_ASSERT(ut_get_max_buffer_segments_called);
	M0_ASSERT(m0_mutex_is_not_locked(&dom->nd_mutex));
	M0_UT_ASSERT(buf_segs == UT_MAX_BUF_SEGMENTS);

	/* allocate buffers for testing */
	nbs = allocate_buffers(buf_size, buf_seg_size, buf_segs);

	/* register the buffers */
	for (i = 0; i < M0_NET_QT_NR; ++i) {
		nb = &nbs[i];
		nb->nb_flags = 0;
		nb->nb_timeout = 0;
		ut_buf_register_called = false;
		rc = m0_net_buffer_register(nb, dom);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(ut_buf_register_called);
		M0_UT_ASSERT(nb->nb_flags & M0_NET_BUF_REGISTERED);
		M0_UT_ASSERT(nb->nb_timeout == M0_TIME_NEVER);
		num_adds[i] = 0;
		num_dels[i] = 0;
		total_bytes[i] = 0;
	}
	m0tt_to_period = m0_time(120, 0); /* 2 min */

	/* TM init with callbacks */
	rc = m0_net_tm_init(tm, dom);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_init_called);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_INITIALIZED);
	M0_UT_ASSERT(m0_list_contains(&dom->nd_tms, &tm->ntm_dom_linkage));
	/* should be able to fini it immediately */
	ut_tm_fini_called = false;
	m0_net_tm_fini(tm);
	M0_UT_ASSERT(ut_tm_fini_called);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_UNDEFINED);

	/* should be able to init it again */
	ut_tm_init_called = false;
	ut_tm_fini_called = false;
	rc = m0_net_tm_init(tm, dom);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_init_called);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_INITIALIZED);
	M0_UT_ASSERT(m0_list_contains(&dom->nd_tms, &tm->ntm_dom_linkage));

	/* check the confine API */
	M0_UT_ASSERT(dom->nd_xprt->nx_ops->xo_tm_confine == NULL);
	rc = m0_net_tm_confine(tm, procmask);
	M0_UT_ASSERT(rc == -ENOSYS); /* optional support */
	M0_UT_ASSERT(!ut_tm_confine_called);
	ut_xprt_ops.xo_tm_confine = ut_tm_confine; /* provide the operation */
	M0_UT_ASSERT(dom->nd_xprt->nx_ops->xo_tm_confine != NULL);
	rc = m0_net_tm_confine(tm, procmask);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_confine_called);
	M0_UT_ASSERT(ut_tm_confine_bm == procmask);

	/* TM start */
	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);

	M0_UT_ASSERT(ut_end_point_create_called == false);
	rc = m0_net_tm_start(tm, "addr2");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_start_called);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_STARTING ||
		     tm->ntm_state == M0_NET_TM_STARTED);

	/* wait on channel for started */
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	M0_UT_ASSERT(ut_tm_event_cb_calls == 1);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_STARTED);
	M0_UT_ASSERT(ut_end_point_create_called);

	M0_UT_ASSERT(tm->ntm_ep != NULL);
	M0_UT_ASSERT(m0_atomic64_get(&tm->ntm_ep->nep_ref.ref_cnt) == 1);

	/* Test desired end point behavior
	   A real transport isn't actually forced to maintain
	   reference counts this way, but ought to do so.
	 */
	ut_end_point_create_called = false;
	rc = m0_net_end_point_create(&ep1, tm, NULL);
	M0_UT_ASSERT(rc != 0); /* no dynamic */
	M0_UT_ASSERT(ut_end_point_create_called);
	M0_ASSERT(m0_mutex_is_not_locked(&tm->ntm_mutex));
	M0_ASSERT(m0_nep_tlist_length(&tm->ntm_end_points) == 1);

	ut_end_point_create_called = false;
	rc = m0_net_end_point_create(&ep1, tm, "addr1");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_end_point_create_called);
	M0_ASSERT(m0_mutex_is_not_locked(&tm->ntm_mutex));
	M0_ASSERT(!m0_nep_tlist_is_empty(&tm->ntm_end_points));
	M0_UT_ASSERT(m0_atomic64_get(&ep1->nep_ref.ref_cnt) == 1);

	rc = m0_net_end_point_create(&ep2, tm, "addr2");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ep2 != ep1);
	M0_UT_ASSERT(ep2 == tm->ntm_ep);
	M0_UT_ASSERT(m0_atomic64_get(&ep2->nep_ref.ref_cnt) == 2);

	rc = m0_net_end_point_create(&ep, tm, "addr1");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ep == ep1);
	M0_UT_ASSERT(m0_atomic64_get(&ep->nep_ref.ref_cnt) == 2);

	M0_UT_ASSERT(ut_end_point_release_called == false);
	m0_net_end_point_get(ep); /* refcnt=3 */
	M0_UT_ASSERT(m0_atomic64_get(&ep->nep_ref.ref_cnt) == 3);

	M0_UT_ASSERT(ut_end_point_release_called == false);
	m0_net_end_point_put(ep); /* refcnt=2 */
	M0_UT_ASSERT(ut_end_point_release_called == false);
	M0_UT_ASSERT(m0_atomic64_get(&ep->nep_ref.ref_cnt) == 2);

	m0_net_end_point_put(ep); /* refcnt=1 */
	M0_UT_ASSERT(ut_end_point_release_called == false);
	M0_UT_ASSERT(m0_atomic64_get(&ep->nep_ref.ref_cnt) == 1);

	m0_net_end_point_put(ep); /* refcnt=0 */
	M0_UT_ASSERT(ut_end_point_release_called);
	M0_UT_ASSERT(ut_last_ep_released == ep);
	ep1 = NULL; /* not valid! */

	/* add MSG_RECV buf with a timeout in the past - should fail */
	nb = &nbs[M0_NET_QT_MSG_RECV];
	nb->nb_callbacks = &ut_buf_cb;
	M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_QUEUED));
	nb->nb_qtype = M0_NET_QT_MSG_RECV;
	nb->nb_timeout = m0_time_sub(m0_time_now(), m0tt_to_period);
	nb->nb_min_receive_size = buf_size;
	nb->nb_max_receive_msgs = 1;
	rc = m0_net_buffer_add(nb, tm);
	M0_UT_ASSERT(rc == -ETIME);

	/* add MSG_RECV buf - should succeeded as now started */
	nb = &nbs[M0_NET_QT_MSG_RECV];
	nb->nb_callbacks = &ut_buf_cb;
	M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_QUEUED));
	nb->nb_qtype = M0_NET_QT_MSG_RECV;
	nb->nb_timeout = m0_time_add(m0_time_now(), m0tt_to_period);
	nb->nb_min_receive_size = buf_size;
	nb->nb_max_receive_msgs = 1;
	nb->nb_msgs_received = 999; /* arbitrary */
	rc = m0_net_buffer_add(nb, tm);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_buf_add_called);
	M0_UT_ASSERT(nb->nb_flags & M0_NET_BUF_QUEUED);
	M0_UT_ASSERT(nb->nb_tm == tm);
	M0_UT_ASSERT(nb->nb_msgs_received == 0); /* reset */
	num_adds[nb->nb_qtype]++;
	max_bytes[nb->nb_qtype] = max64u(nb->nb_length,
					 max_bytes[nb->nb_qtype]);

	/* clean up; real xprt would handle this itself */
	m0_thread_join(&ut_tm_thread);
	m0_thread_fini(&ut_tm_thread);

	/* initialize and add remaining types of buffers
	   use buffer private callbacks for the bulk
	 */
	for (i = M0_NET_QT_MSG_SEND; i < M0_NET_QT_NR; ++i) {
		nb = &nbs[i];
		M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_QUEUED));
		nb->nb_qtype = i;
		nb->nb_callbacks = &ut_buf_cb;
		/* NB: real code sets nb_ep to server ep */
		switch (i) {
		case M0_NET_QT_MSG_SEND:
			nb->nb_ep = ep2;
			nb->nb_length = buf_size;
			break;
		case M0_NET_QT_PASSIVE_BULK_RECV:
			M0_UT_ASSERT(nb->nb_length == 0);
			nb->nb_ep = ep2;
			break;
		case M0_NET_QT_PASSIVE_BULK_SEND:
			nb->nb_ep = ep2;
			nb->nb_length = buf_size;
			break;
		case M0_NET_QT_ACTIVE_BULK_RECV:
			M0_UT_ASSERT(nb->nb_length == 0);
			make_desc(&nb->nb_desc);
			break;
		case M0_NET_QT_ACTIVE_BULK_SEND:
			nb->nb_length = buf_size;
			make_desc(&nb->nb_desc);
			break;
		}
		nb->nb_timeout = m0_time_add(m0_time_now(), m0tt_to_period);
		rc = m0_net_buffer_add(nb, tm);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(nb->nb_flags & M0_NET_BUF_QUEUED);
		M0_UT_ASSERT(nb->nb_tm == tm);
		num_adds[nb->nb_qtype]++;
		max_bytes[nb->nb_qtype] = max64u(buf_size,
						 max_bytes[nb->nb_qtype]);
	}
	M0_UT_ASSERT(m0_atomic64_get(&ep2->nep_ref.ref_cnt) == 3);

	/* fake each type of buffer "post" response.
	   xprt normally does this
	 */
	for (i = M0_NET_QT_MSG_RECV; i < M0_NET_QT_NR; ++i) {
		struct m0_net_buffer_event ev = {
			.nbe_buffer = &nbs[i],
			.nbe_status = 0,
		};
		DELAY_MS(1);
		ev.nbe_time = m0_time_now();
		nb = &nbs[i];

		if (i == M0_NET_QT_MSG_RECV) {
			/* simulate transport ep in recv msg */
			ev.nbe_ep = ep2;
			m0_net_end_point_get(ep2);
		}

		if (i == M0_NET_QT_MSG_RECV ||
		    i == M0_NET_QT_PASSIVE_BULK_RECV ||
		    i == M0_NET_QT_ACTIVE_BULK_RECV) {
			/* fake the length in the event */
			ev.nbe_length = buf_size;
		}

		nb->nb_flags |= M0_NET_BUF_IN_USE;
		m0_net_buffer_event_post(&ev);
		M0_UT_ASSERT(ut_cb_calls[i] == 1);
		M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_IN_USE));
		M0_UT_ASSERT(nb->nb_timeout == M0_TIME_NEVER);
		if (i == M0_NET_QT_MSG_RECV)
			M0_UT_ASSERT(nb->nb_msgs_received == 1);
	}
	M0_UT_ASSERT(m0_atomic64_get(&ep2->nep_ref.ref_cnt) == 2);
	/* add a buffer and fake del - check callback */
	nb = &nbs[M0_NET_QT_PASSIVE_BULK_SEND];
	M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_QUEUED));
	nb->nb_qtype = M0_NET_QT_PASSIVE_BULK_SEND;
	m0_net_desc_free(&nb->nb_desc);
	rc = m0_net_buffer_add(nb, tm);
	M0_UT_ASSERT(rc == 0);
	num_adds[nb->nb_qtype]++;
	max_bytes[nb->nb_qtype] = max64u(nb->nb_length,
					 max_bytes[nb->nb_qtype]);

	ut_buf_del_called = false;
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);
	m0_net_buffer_del(nb, tm);
	M0_UT_ASSERT(ut_buf_del_called);
	m0_net_desc_free(&nb->nb_desc);
	num_dels[nb->nb_qtype]++;

	/* wait on channel for post (and consume UT thread) */
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	rc = m0_thread_join(&ut_del_thread);
	M0_UT_ASSERT(rc == 0);

	/* Initialize and add buffers for multi-use.
	   Note: the net API does not restrict multi-use to the recv queue.
	 */
	M0_UT_ASSERT(m0_atomic64_get(&ep2->nep_ref.ref_cnt) == 2);
	for (i = M0_NET_QT_MSG_RECV; i < M0_NET_QT_NR; ++i) {
		nb = &nbs[i];
		M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_QUEUED));
		nb->nb_qtype = i;
		nb->nb_callbacks = &ut_buf_multi_use_cb;
		nb->nb_length = 0;
		/* NB: real code sets nb_ep to server ep */
		switch (i) {
		case M0_NET_QT_MSG_RECV:
			nb->nb_min_receive_size = buf_size;
			nb->nb_max_receive_msgs = 2;
			break;
		case M0_NET_QT_MSG_SEND:
			nb->nb_ep = ep2;
			nb->nb_length = buf_size;
			break;
		case M0_NET_QT_PASSIVE_BULK_RECV:
			nb->nb_ep = ep2;
			break;
		case M0_NET_QT_PASSIVE_BULK_SEND:
			nb->nb_ep = ep2;
			nb->nb_length = buf_size;
			break;
		case M0_NET_QT_ACTIVE_BULK_RECV:
			make_desc(&nb->nb_desc);
			break;
		case M0_NET_QT_ACTIVE_BULK_SEND:
			nb->nb_length = buf_size;
			make_desc(&nb->nb_desc);
			break;
		}
		nb->nb_timeout = m0_time_add(m0_time_now(),
					     m0tt_to_period);
		nb->nb_tm = NULL;
		rc = m0_net_buffer_add(nb, tm);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(nb->nb_flags & M0_NET_BUF_QUEUED);
		M0_UT_ASSERT(nb->nb_tm == tm);
		num_adds[nb->nb_qtype]++;
		max_bytes[nb->nb_qtype] = max64u(buf_size,
						 max_bytes[nb->nb_qtype]);
	}
	M0_UT_ASSERT(m0_atomic64_get(&ep2->nep_ref.ref_cnt) == 3);

	/* Issue multiple fake buffer "post" with the RETAIN flag. */
	for (reuse_cnt = 0; reuse_cnt < NUM_REUSES; ++reuse_cnt) {
		bool retain = true;
		if (reuse_cnt == NUM_REUSES - 1)
			retain = false;
		for (i = M0_NET_QT_MSG_RECV; i < M0_NET_QT_NR; ++i) {
			m0_time_t to_before = 0;
			struct m0_net_buffer_event ev = {
				.nbe_buffer = &nbs[i],
				.nbe_status = 0,
			};
			DELAY_MS(1);
			ev.nbe_time = m0_time_now();
			nb = &nbs[i];

			if (i == M0_NET_QT_MSG_RECV) {
				/* simulate transport ep in recv msg */
				ev.nbe_ep = ep2;
				m0_net_end_point_get(ep2);
			}

			if (i == M0_NET_QT_MSG_RECV ||
			    i == M0_NET_QT_PASSIVE_BULK_RECV ||
			    i == M0_NET_QT_ACTIVE_BULK_RECV) {
				/* fake the length in the event */
				ev.nbe_length = buf_size;
				nb->nb_length = 0; /* the tests expect this */
			}

			nb->nb_flags |= M0_NET_BUF_IN_USE;
			if (retain) {
				nb->nb_flags |= M0_NET_BUF_RETAIN;
				to_before = nb->nb_timeout;
				ut_multi_use_expect_queued = true;
				if (i == M0_NET_QT_MSG_SEND)
					m0_net_end_point_get(ep2); /* adjust */
			} else {
				ut_multi_use_expect_queued = false;
			}
			m0_net_buffer_event_post(&ev);
			M0_UT_ASSERT(ut_multi_use_got_buf == nb);
			M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_RETAIN));
			if (retain) {
				M0_UT_ASSERT(to_before == nb->nb_timeout);
				M0_UT_ASSERT(nb->nb_flags & M0_NET_BUF_QUEUED);
				M0_UT_ASSERT(nb->nb_flags & M0_NET_BUF_IN_USE);
			} else {
				M0_UT_ASSERT(nb->nb_timeout == M0_TIME_NEVER);
				M0_UT_ASSERT(!(nb->nb_flags &
					       M0_NET_BUF_QUEUED));
				M0_UT_ASSERT(!(nb->nb_flags &
					       M0_NET_BUF_IN_USE));
			}
			if (i == M0_NET_QT_MSG_RECV)
				M0_UT_ASSERT(nb->nb_msgs_received ==
					     reuse_cnt + 1);
		}
	}
	/* free end point */
	M0_UT_ASSERT(m0_atomic64_get(&ep2->nep_ref.ref_cnt) == 2);
	m0_net_end_point_put(ep2);

	/* TM stop */
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);
	rc = m0_net_tm_stop(tm, false);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_stop_called);

	/* wait on channel for stopped */
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	M0_UT_ASSERT(ut_tm_event_cb_calls == 2);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_STOPPED);

	/* clean up; real xprt would handle this itself */
	m0_thread_join(&ut_tm_thread);
	m0_thread_fini(&ut_tm_thread);

	/* de-register channel waiter */
	m0_clink_fini(&tmwait);

	/* get stats (specific queue, then all queues) */
	i = M0_NET_QT_PASSIVE_BULK_SEND;
	rc = m0_net_tm_stats_get(tm, i, &qs[0], false);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(qs[0].nqs_num_adds == num_adds[i]);
	M0_UT_ASSERT(qs[0].nqs_num_dels == num_dels[i]);
	M0_UT_ASSERT(qs[0].nqs_total_bytes == total_bytes[i]);
	M0_UT_ASSERT(qs[0].nqs_max_bytes == max_bytes[i]);
	M0_UT_ASSERT((qs[0].nqs_num_f_events + qs[0].nqs_num_s_events)
		     == num_adds[i] + reuse_cnt - 1);
	M0_UT_ASSERT(qs[0].nqs_num_f_events + qs[0].nqs_num_s_events > 0 &&
		     qs[0].nqs_time_in_queue > 0);

	rc = m0_net_tm_stats_get(tm, M0_NET_QT_NR, qs, true);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < M0_NET_QT_NR; i++) {
		KPRN("i=%d\n", i);
#define QS(x)  KPRN("\t" #x "=%"PRId64"\n", qs[i].nqs_##x)
#define QS2(x) KPRN("\t" #x "=%"PRId64" [%"PRId64"]\n", qs[i].nqs_##x, x[i])
		QS2(num_adds);
		QS2(num_dels);
		QS2(total_bytes);
		QS(max_bytes);
		QS(num_f_events);
		QS(num_s_events);
		QS(time_in_queue);
		M0_UT_ASSERT(qs[i].nqs_num_adds == num_adds[i]);
		M0_UT_ASSERT(qs[i].nqs_num_dels == num_dels[i]);
		M0_UT_ASSERT(qs[i].nqs_total_bytes == total_bytes[i]);
		M0_UT_ASSERT(qs[i].nqs_total_bytes >= qs[i].nqs_max_bytes);
		M0_UT_ASSERT(qs[i].nqs_max_bytes == max_bytes[i]);
		M0_UT_ASSERT((qs[i].nqs_num_f_events + qs[i].nqs_num_s_events)
			     == num_adds[i] + reuse_cnt - 1);
		M0_UT_ASSERT(qs[i].nqs_num_f_events +
			     qs[i].nqs_num_s_events > 0 &&
			     qs[i].nqs_time_in_queue > 0);
	}

	rc = m0_net_tm_stats_get(tm, M0_NET_QT_NR, qs, false);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < M0_NET_QT_NR; i++) {
		M0_UT_ASSERT(qs[i].nqs_num_adds == 0);
		M0_UT_ASSERT(qs[i].nqs_num_dels == 0);
		M0_UT_ASSERT(qs[i].nqs_num_f_events == 0);
		M0_UT_ASSERT(qs[i].nqs_num_s_events == 0);
		M0_UT_ASSERT(qs[i].nqs_total_bytes == 0);
		M0_UT_ASSERT(qs[i].nqs_max_bytes == 0);
		M0_UT_ASSERT(qs[i].nqs_time_in_queue == 0);
	}

	/* fini the TM */
	ut_tm_fini_called = false;
	m0_net_tm_fini(tm);
	M0_UT_ASSERT(ut_tm_fini_called);

	/* TM fini releases final end point */
	M0_UT_ASSERT(ut_last_ep_released == ep2);

	/*
	 * Test APIs for synchronous buffer event delivery
	 */

	/* restart the TM */
	ut_tm_init_called = false;
	ut_tm_fini_called = false;
	rc = m0_net_tm_init(tm, dom);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ut_tm_init_called);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_INITIALIZED);
	M0_UT_ASSERT(m0_list_contains(&dom->nd_tms, &tm->ntm_dom_linkage));
	M0_UT_ASSERT(tm->ntm_bev_auto_deliver);

	/* request synchronous buffer event delivery */
	M0_UT_ASSERT(dom->nd_xprt->nx_ops->xo_bev_deliver_sync == NULL);
	rc = m0_net_buffer_event_deliver_synchronously(tm);
	M0_UT_ASSERT(rc == -ENOSYS); /* optional support */
	M0_UT_ASSERT(tm->ntm_bev_auto_deliver);
	M0_UT_ASSERT(!ut_bev_deliver_sync_called);
	ut_xprt_ops.xo_bev_deliver_sync = ut_bev_deliver_sync; /* set op */
	M0_UT_ASSERT(dom->nd_xprt->nx_ops->xo_bev_deliver_sync != NULL);
	rc = m0_net_buffer_event_deliver_synchronously(tm);
	M0_UT_ASSERT(ut_bev_deliver_sync_called);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(!tm->ntm_bev_auto_deliver);

	/* start the TM */
	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);
	rc = m0_net_tm_start(tm, "addr3");
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	M0_UT_ASSERT(tm->ntm_state == M0_NET_TM_STARTED);
	m0_thread_join(&ut_tm_thread); /* cleanup thread */
	m0_thread_fini(&ut_tm_thread);

	/* test the synchronous buffer event delivery APIs */
	M0_UT_ASSERT(!ut_bev_pending_called);
	brc = m0_net_buffer_event_pending(tm);
	M0_UT_ASSERT(ut_bev_pending_called);
	M0_UT_ASSERT(!brc);

	M0_UT_ASSERT(!ut_bev_notify_called);
	m0_net_buffer_event_notify(tm, &tm->ntm_chan);
	M0_UT_ASSERT(ut_bev_notify_called);

	ut_bev_pending_called = false;
	brc = m0_net_buffer_event_pending(tm);
	M0_UT_ASSERT(ut_bev_pending_called);
	M0_UT_ASSERT(brc);

	M0_UT_ASSERT(!ut_bev_deliver_all_called);
	m0_net_buffer_event_deliver_all(tm);
	M0_UT_ASSERT(ut_bev_deliver_all_called);

	/* TM stop and fini */
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);
	rc = m0_net_tm_stop(tm, false);
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	m0_thread_join(&ut_tm_thread); /* cleanup thread */
	m0_thread_fini(&ut_tm_thread);
	m0_clink_fini(&tmwait);
	m0_net_tm_fini(tm);

	/* de-register and free buffers */
	for (i = 0; i < M0_NET_QT_NR; ++i) {
		nb = &nbs[i];
		ut_buf_deregister_called = false;
		m0_net_desc_free(&nb->nb_desc);
		m0_net_buffer_deregister(nb, dom);
		M0_UT_ASSERT(ut_buf_deregister_called);
		M0_UT_ASSERT(!(nb->nb_flags & M0_NET_BUF_REGISTERED));
		m0_bufvec_free(&nb->nb_buffer);
	}
	m0_free(nbs);

	/* fini the domain */
	M0_UT_ASSERT(ut_dom_fini_called == false);
	m0_net_domain_fini(dom);
	M0_UT_ASSERT(ut_dom_fini_called);
}

#include "net/ut/tm_provision_ut.c"

struct m0_ut_suite m0_net_bulk_if_ut = {
        .ts_name = "net-bulk-if",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "net_bulk_if", test_net_bulk_if },
                { NULL, NULL }
        }
};
M0_EXPORTED(m0_net_bulk_if_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
