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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "net/bulk_emulation/mem_xprt_pvt.h"

/**
   @addtogroup bulkmem
   @{
 */

/**
   List of in-memory network domains.
   Protected by struct m0_net_mutex.
 */
static struct m0_list  mem_domains;

/* forward reference */
static const struct m0_net_bulk_mem_ops mem_xprt_methods;

/**
   Transport initialization subroutine called from m0_init().
 */
M0_INTERNAL int m0_mem_xprt_init(void)
{
	m0_list_init(&mem_domains);
	return 0;
}

/**
   Transport finalization subroutine called from m0_fini().
 */
M0_INTERNAL void m0_mem_xprt_fini(void)
{
	m0_list_fini(&mem_domains);
}

/* To reduce global symbols, yet make the code readable, we
   include other .c files with static symbols into this file.
   Dependency information must be captured in Makefile.am.

   Static functions should be declared in the private header file
   so that the order of their definition does not matter.
 */
#include "net/bulk_emulation/mem_xprt_ep.c"
#include "net/bulk_emulation/mem_xprt_tm.c"
#include "net/bulk_emulation/mem_xprt_msg.c"
#include "net/bulk_emulation/mem_xprt_bulk.c"

static m0_bcount_t mem_buffer_length(const struct m0_net_buffer *nb)
{
	return m0_vec_count(&nb->nb_buffer.ov_vec);
}

/**
   Check buffer size limits.
 */
static bool mem_buffer_in_bounds(const struct m0_net_buffer *nb)
{
	const struct m0_vec *v = &nb->nb_buffer.ov_vec;
	uint32_t i;
	m0_bcount_t len = 0;

	if (v->v_nr > M0_NET_BULK_MEM_MAX_BUFFER_SEGMENTS)
		return false;
	for (i = 0; i < v->v_nr; ++i) {
		if (v->v_count[i] > M0_NET_BULK_MEM_MAX_SEGMENT_SIZE)
			return false;
		M0_ASSERT(len + v->v_count[i] >= len);
		len += v->v_count[i];
	}
	return len <= M0_NET_BULK_MEM_MAX_BUFFER_SIZE;
}

/**
   Copy from one buffer to another. Each buffer may have different
   number of segments and segment sizes.
   The subroutine does not set the nb_length field of the d_nb buffer.
   @param d_nb  The destination buffer pointer.
   @param s_nb  The source buffer pointer.
   @param num_bytes The number of bytes to copy.
   @pre
mem_buffer_length(d_nb) >= num_bytes &&
mem_buffer_length(s_nb) >= num_bytes
 */
static int mem_copy_buffer(struct m0_net_buffer *d_nb,
			   struct m0_net_buffer *s_nb,
			   m0_bcount_t num_bytes)
{
	struct m0_bufvec_cursor s_cur;
	struct m0_bufvec_cursor d_cur;
	m0_bcount_t bytes_copied;

	if (mem_buffer_length(d_nb) < num_bytes) {
		return M0_ERR(-EFBIG);
	}
	M0_PRE(mem_buffer_length(s_nb) >= num_bytes);

	m0_bufvec_cursor_init(&s_cur, &s_nb->nb_buffer);
	m0_bufvec_cursor_init(&d_cur, &d_nb->nb_buffer);
	bytes_copied = m0_bufvec_cursor_copy(&d_cur, &s_cur, num_bytes);
	M0_ASSERT(bytes_copied == num_bytes);

	return 0;
}

/**
   Add a work item to the work list
 */
static void mem_wi_add(struct m0_net_bulk_mem_work_item *wi,
		       struct m0_net_bulk_mem_tm_pvt *tp)
{
	m0_list_add_tail(&tp->xtm_work_list, &wi->xwi_link);
	m0_cond_signal(&tp->xtm_work_list_cv);
}

/**
   Post a buffer event.
 */
static void mem_wi_post_buffer_event(struct m0_net_bulk_mem_work_item *wi)
{
	struct m0_net_buffer *nb = mem_wi_to_buffer(wi);
	struct m0_net_buffer_event ev = {
		.nbe_buffer = nb,
		.nbe_status = wi->xwi_status,
		.nbe_offset = 0,
		.nbe_length = wi->xwi_nbe_length,
		.nbe_ep     = wi->xwi_nbe_ep
	};
	M0_PRE(wi->xwi_status <= 0);
	ev.nbe_time = m0_time_now();
	m0_net_buffer_event_post(&ev);
	return;
}

static bool mem_dom_invariant(const struct m0_net_domain *dom)
{
	struct m0_net_bulk_mem_domain_pvt *dp = mem_dom_to_pvt(dom);
	return dp != NULL && dp->xd_dom == dom;
}

static bool mem_ep_invariant(const struct m0_net_end_point *ep)
{
	const struct m0_net_bulk_mem_end_point *mep = mem_ep_to_pvt(ep);
	return mep->xep_magic == M0_NET_BULK_MEM_XEP_MAGIC &&
		mep->xep_ep.nep_addr == &mep->xep_addr[0];
}

static bool mem_buffer_invariant(const struct m0_net_buffer *nb)
{
	const struct m0_net_bulk_mem_buffer_pvt *bp = mem_buffer_to_pvt(nb);
	return bp != NULL && bp->xb_buffer == nb &&
		mem_dom_invariant(nb->nb_dom);
}

static bool mem_tm_invariant(const struct m0_net_transfer_mc *tm)
{
	const struct m0_net_bulk_mem_tm_pvt *tp = mem_tm_to_pvt(tm);
	return tp != NULL && tp->xtm_tm == tm &&
		mem_dom_invariant(tm->ntm_dom);
}

/**
   This routine will allocate and initialize the private domain data and attach
   it to the domain. It will assume that the domains private pointer is
   allocated if not NULL. This allows for a derived transport to pre-allocate
   this structure before invoking the base method. The method will initialize
   the size and count fields as per the requirements of the in-memory module.
   If the private domain pointer was not allocated, the routine will assume
   that the domain is not derived, and will then link the domain in a private
   list to facilitate in-memory data transfers between transfer machines.
 */
static int mem_xo_dom_init(struct m0_net_xprt *xprt,
			   struct m0_net_domain *dom)
{
	struct m0_net_bulk_mem_domain_pvt *dp;

	M0_ENTRY();

	if (dom->nd_xprt_private != NULL) {
		M0_PRE(xprt != &m0_net_bulk_mem_xprt);
		dp = dom->nd_xprt_private;
	} else {
		M0_ALLOC_PTR(dp);
		if (dp == NULL) {
			return M0_RC(-ENOMEM);
		}
		dom->nd_xprt_private = dp;
	}
	M0_ASSERT(mem_dom_to_pvt(dom) == dp);
	dp->xd_dom = dom;
	dp->xd_ops = &mem_xprt_methods;

	/* tunable parameters */
	dp->xd_addr_tuples       = 2;
	dp->xd_num_tm_threads    = 1;

	dp->xd_buf_id_counter = 0;
	m0_list_link_init(&dp->xd_dom_linkage);

	if (xprt != &m0_net_bulk_mem_xprt) {
		dp->xd_derived = true;
	} else {
		dp->xd_derived = false;
		m0_list_add(&mem_domains, &dp->xd_dom_linkage);
	}
	M0_POST(mem_dom_invariant(dom));

	return M0_RC(0);
}

/**
   This subroutine releases references from the private data.
   If not derived, it will unlink the domain and free the private data.
   Derived transports should free the private data upon return from this
   subroutine.
 */
static void mem_xo_dom_fini(struct m0_net_domain *dom)
{
	struct m0_net_bulk_mem_domain_pvt *dp = mem_dom_to_pvt(dom);
	M0_PRE(mem_dom_invariant(dom));

	if (dp->xd_derived)
		return;
	m0_list_del(&dp->xd_dom_linkage);
	m0_free(dp);
	dom->nd_xprt_private = NULL;
}

static m0_bcount_t mem_xo_get_max_buffer_size(const struct m0_net_domain *dom)
{
	M0_PRE(mem_dom_invariant(dom));
	return M0_NET_BULK_MEM_MAX_BUFFER_SIZE;
}

static m0_bcount_t mem_xo_get_max_buffer_segment_size(
					      const struct m0_net_domain *dom)
{
	M0_PRE(mem_dom_invariant(dom));
	return M0_NET_BULK_MEM_MAX_SEGMENT_SIZE;
}

static int32_t mem_xo_get_max_buffer_segments(const struct m0_net_domain *dom)
{
	M0_PRE(mem_dom_invariant(dom));
	return M0_NET_BULK_MEM_MAX_BUFFER_SEGMENTS;
}

/**
   This routine will search for an existing end point in the domain, and if not
   found, will allocate and zero out space for a new end point using the
   xd_sizeof_ep field to determine the size. It will fill in the xep_address
   field with the IP and port number, and will link the end point to the domain
   link list.

   Dynamic address assignment is not supported.
   @param epp  Returns the pointer to the end point.
   @param dom  Domain pointer.
   @param addr Address string in one of the following two formats:
   - "dottedIP:portNumber" if 2-tuple addressing used.
   - "dottedIP:portNumber:serviceId" if 3-tuple addressing used.
 */
static int mem_xo_end_point_create(struct m0_net_end_point **epp,
				   struct m0_net_transfer_mc *tm,
				   const char *addr)
{
	char buf[M0_NET_BULK_MEM_XEP_ADDR_LEN];
	const char *dot_ip;
	char *p;
	char *pp;
	int pnum;
	struct sockaddr_in sa;
	uint32_t id = 0;
	struct m0_net_bulk_mem_domain_pvt *dp = mem_dom_to_pvt(tm->ntm_dom);

	M0_PRE(M0_IN(dp->xd_addr_tuples, (2, 3)));

	if (addr == NULL)
		return M0_ERR(-ENOSYS); /* no dynamic addressing */

	strncpy(buf, addr, sizeof(buf)-1); /* copy to modify */
	buf[sizeof(buf)-1] = '\0';
	for (p=buf; *p && *p != ':'; p++);
	if (*p == '\0')
		return M0_ERR(-EINVAL);
	*p++ = '\0'; /* terminate the ip address : */
	pp = p;
	for (p=pp; *p && *p != ':'; p++);
	if (dp->xd_addr_tuples == 3) {
		*p++ = '\0'; /* terminate the port number */
		sscanf(p, "%u", &id);
		if (id == 0)
			return M0_ERR(-EINVAL);
	}
	else if (*p == ':')
		return M0_ERR(-EINVAL); /* 3-tuple where expecting 2 */
	sscanf(pp, "%d", &pnum);
	sa.sin_port = htons(pnum);
	dot_ip = buf;
#ifdef __KERNEL__
	sa.sin_addr.s_addr = in_aton(dot_ip);
	if (sa.sin_addr.s_addr == 0)
		return M0_ERR(-EINVAL);
#else
	if (inet_aton(dot_ip, &sa.sin_addr) == 0)
		return M0_ERR(-EINVAL);
#endif
	return mem_bmo_ep_create(epp, tm, &sa, id);
}

/**
   This routine initializes the private data associated with the buffer.

   The private data is allocated here when the transport is used directly.
   Derived transports should allocate the buffer private data before
   invoking this method. The nb_xprt_private field should be set to
   point to this transports private data.
 */
static int mem_xo_buf_register(struct m0_net_buffer *nb)
{
	struct m0_net_bulk_mem_domain_pvt *dp;
	struct m0_net_bulk_mem_buffer_pvt *bp;

	M0_PRE(nb->nb_dom != NULL && mem_dom_invariant(nb->nb_dom));

	if (!mem_bmo_buffer_in_bounds(nb))
		return M0_ERR(-EFBIG);

	dp = mem_dom_to_pvt(nb->nb_dom);
	if (dp->xd_derived) {
		M0_PRE(nb->nb_xprt_private != NULL);
		bp = nb->nb_xprt_private;
	} else {
		M0_PRE(nb->nb_xprt_private == NULL);
		M0_ALLOC_PTR(bp);
		if (bp == NULL)
			return M0_ERR(-ENOMEM);
		nb->nb_xprt_private = bp;
	}
	M0_ASSERT(mem_buffer_to_pvt(nb) == bp);

	bp->xb_buffer = nb;
	m0_list_link_init(&bp->xb_wi.xwi_link);
	bp->xb_wi.xwi_op = M0_NET_XOP_NR;
	M0_POST(mem_buffer_invariant(nb));
	return 0;
}

/**
   This routine releases references from the private data associated with
   the buffer, and frees the private data if the transport was used directly.
   Derived transports should free the private data upon return from this
   subroutine.
 */
static void mem_xo_buf_deregister(struct m0_net_buffer *nb)
{
	struct m0_net_bulk_mem_domain_pvt *dp;
	struct m0_net_bulk_mem_buffer_pvt *bp;

	M0_PRE(mem_buffer_invariant(nb));
	dp = mem_dom_to_pvt(nb->nb_dom);
	bp = mem_buffer_to_pvt(nb);
	m0_list_link_fini(&bp->xb_wi.xwi_link);
	if (!dp->xd_derived) {
		m0_free(bp);
		nb->nb_xprt_private = NULL;
	}
	return;
}

/**
   This routine initiates processing of the buffer operation.
 */
static int mem_xo_buf_add(struct m0_net_buffer *nb)
{
	struct m0_net_transfer_mc *tm;
	struct m0_net_bulk_mem_tm_pvt *tp;
	struct m0_net_bulk_mem_domain_pvt *dp;
	struct m0_net_bulk_mem_buffer_pvt *bp;
	struct m0_net_bulk_mem_work_item *wi;
	int rc;

	M0_PRE(mem_buffer_invariant(nb));
	M0_PRE(nb->nb_flags & M0_NET_BUF_QUEUED &&
	       (nb->nb_flags & M0_NET_BUF_IN_USE) == 0);

	M0_PRE(nb->nb_offset == 0); /* don't support any other value */

	tm = nb->nb_tm;
	M0_PRE(mem_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	tp = mem_tm_to_pvt(tm);

	if (tp->xtm_state > M0_NET_XTM_STARTED)
		return M0_ERR(-EPERM);

	dp = mem_dom_to_pvt(tm->ntm_dom);
	bp = mem_buffer_to_pvt(nb);
	wi = &bp->xb_wi;
	wi->xwi_op = M0_NET_XOP_NR;

	switch (nb->nb_qtype) {
	case M0_NET_QT_MSG_RECV:
		break;
	case M0_NET_QT_MSG_SEND:
		M0_ASSERT(nb->nb_ep != NULL);
		wi->xwi_op = M0_NET_XOP_MSG_SEND;
		break;
	case M0_NET_QT_PASSIVE_BULK_RECV:
		nb->nb_length = 0;
	case M0_NET_QT_PASSIVE_BULK_SEND:
		bp->xb_buf_id = ++dp->xd_buf_id_counter;
		rc = mem_bmo_desc_create(&nb->nb_desc, tm,
					 nb->nb_qtype, nb->nb_length,
					 bp->xb_buf_id);
		if (rc != 0)
			return M0_RC(rc);
		break;
	case M0_NET_QT_ACTIVE_BULK_RECV:
	case M0_NET_QT_ACTIVE_BULK_SEND:
		wi->xwi_op = M0_NET_XOP_ACTIVE_BULK;
		break;
	default:
		M0_IMPOSSIBLE("invalid queue type");
		break;
	}
	wi->xwi_status = -1;

	if (wi->xwi_op != M0_NET_XOP_NR) {
		mem_wi_add(wi, tp);
	}

	return 0;
}

/**
   Cancel ongoing buffer operations.  May also be invoked to time out a pending
   buffer operation by first setting the M0_NET_BUF_TIMED_OUT flag.
   @param nb Buffer pointer
 */
static void mem_xo_buf_del(struct m0_net_buffer *nb)
{
	struct m0_net_bulk_mem_buffer_pvt *bp = mem_buffer_to_pvt(nb);
	struct m0_net_transfer_mc *tm;
	struct m0_net_bulk_mem_tm_pvt *tp;
	struct m0_net_bulk_mem_work_item *wi;

	M0_PRE(mem_buffer_invariant(nb));
	M0_PRE(nb->nb_flags & M0_NET_BUF_QUEUED);

	tm = nb->nb_tm;
	M0_PRE(mem_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	tp = mem_tm_to_pvt(tm);

	if (nb->nb_flags & M0_NET_BUF_IN_USE)
		return;

	wi = &bp->xb_wi;
	wi->xwi_op = M0_NET_XOP_CANCEL_CB;
	if (!(nb->nb_flags & M0_NET_BUF_TIMED_OUT))
		nb->nb_flags |= M0_NET_BUF_CANCELLED;

	switch (nb->nb_qtype) {
	case M0_NET_QT_MSG_RECV:
	case M0_NET_QT_PASSIVE_BULK_RECV:
	case M0_NET_QT_PASSIVE_BULK_SEND:
		/* must be added to the work list */
		M0_ASSERT(!m0_list_contains(&tp->xtm_work_list,&wi->xwi_link));
		mem_wi_add(wi, tp);
		break;

	case M0_NET_QT_MSG_SEND:
	case M0_NET_QT_ACTIVE_BULK_RECV:
	case M0_NET_QT_ACTIVE_BULK_SEND:
		/* these are already queued */
		M0_ASSERT(m0_list_contains(&tp->xtm_work_list,&wi->xwi_link));
		break;

	default:
		M0_IMPOSSIBLE("invalid queue type");
		break;
	}
}

/**
   Initialize a transfer machine.

   The private data is allocated here if the transport is used directly.
   Derived transports should allocate their private data prior to calling
   this subroutine, and set the TM private pointer to point to the embedded
   private structure of this transport.
   @param tm Transfer machine pointer
   @retval 0 on success
   @retval -ENOMEM if memory not available
 */
static int mem_xo_tm_init(struct m0_net_transfer_mc *tm)
{
	struct m0_net_bulk_mem_domain_pvt *dp;
	struct m0_net_bulk_mem_tm_pvt *tp;

	M0_PRE(mem_dom_invariant(tm->ntm_dom));

	dp = mem_dom_to_pvt(tm->ntm_dom);
	if (dp->xd_derived) {
		M0_PRE(tm->ntm_xprt_private != NULL);
		tp = tm->ntm_xprt_private;
	} else {
		M0_PRE(tm->ntm_xprt_private == NULL);
		M0_ALLOC_PTR(tp);
		if (tp == NULL)
			return M0_ERR(-ENOMEM);
		tm->ntm_xprt_private = tp;
	}
	M0_ASSERT(mem_tm_to_pvt(tm) == tp);

	tp->xtm_num_workers = dp->xd_num_tm_threads;
	/* defer allocation of thread array to start time */
	tp->xtm_tm = tm;
	tp->xtm_state = M0_NET_XTM_INITIALIZED;
	m0_list_init(&tp->xtm_work_list);
	m0_cond_init(&tp->xtm_work_list_cv, &tm->ntm_mutex);
	M0_POST(mem_tm_invariant(tm));
	return 0;
}

/**
   Finalize a transfer machine.
   Derived transports should free the private data upon return from this
   subroutine.
   @param tm Transfer machine pointer
 */
static void mem_xo_tm_fini(struct m0_net_transfer_mc *tm)
{
	struct m0_net_bulk_mem_domain_pvt *dp;
	struct m0_net_bulk_mem_tm_pvt *tp = mem_tm_to_pvt(tm);

	M0_PRE(mem_tm_invariant(tm));
	M0_PRE(tp->xtm_state == M0_NET_XTM_STOPPED ||
	       tp->xtm_state == M0_NET_XTM_FAILED  ||
	       tp->xtm_state == M0_NET_XTM_INITIALIZED);

	dp = mem_dom_to_pvt(tm->ntm_dom);
	m0_mutex_lock(&tm->ntm_mutex);
	tp->xtm_state = M0_NET_XTM_STOPPED; /* to stop the workers */
	m0_cond_broadcast(&tp->xtm_work_list_cv);
	m0_mutex_unlock(&tm->ntm_mutex);
	if (tp->xtm_worker_threads != NULL) {
		int i;
		for (i = 0; i < tp->xtm_num_workers; ++i) {
			if (tp->xtm_worker_threads[i].t_state != TS_PARKED)
				m0_thread_join(&tp->xtm_worker_threads[i]);
		}
		m0_free(tp->xtm_worker_threads);
	}
	m0_cond_fini(&tp->xtm_work_list_cv);
	m0_list_fini(&tp->xtm_work_list);
	tp->xtm_tm = NULL;
	if (!dp->xd_derived) {
		tm->ntm_xprt_private = NULL;
		m0_free(tp);
	}
}

M0_INTERNAL void m0_net_bulk_mem_tm_set_num_threads(struct m0_net_transfer_mc
						    *tm, size_t num)
{
	struct m0_net_bulk_mem_tm_pvt *tp = mem_tm_to_pvt(tm);
	M0_PRE(mem_tm_invariant(tm));
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(tm->ntm_state == M0_NET_TM_INITIALIZED);
	M0_PRE(tp->xtm_state == M0_NET_XTM_INITIALIZED);
	tp->xtm_num_workers = num;
	m0_mutex_unlock(&tm->ntm_mutex);
}

M0_INTERNAL size_t m0_net_bulk_mem_tm_get_num_threads(const struct
						      m0_net_transfer_mc *tm)
{
	struct m0_net_bulk_mem_tm_pvt *tp = mem_tm_to_pvt(tm);
	M0_PRE(mem_tm_invariant(tm));
	return tp->xtm_num_workers;
}

static int mem_xo_tm_start(struct m0_net_transfer_mc *tm, const char *addr)
{
	struct m0_net_bulk_mem_tm_pvt *tp;
	struct m0_net_bulk_mem_work_item *wi_st_chg;
	struct m0_net_xprt *xprt;
	int rc = 0;
	int i;

	M0_PRE(mem_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));

	tp = mem_tm_to_pvt(tm);
	if (tp->xtm_state == M0_NET_XTM_STARTED)
		return 0;
	if (tp->xtm_state == M0_NET_XTM_STARTING)
		return 0;
	if (tp->xtm_state != M0_NET_XTM_INITIALIZED)
		return M0_ERR(-EPERM);

	/* allocate worker thread array */
	if (tp->xtm_worker_threads == NULL) {
		/* allocation creates parked threads in case of failure */
		M0_CASSERT(TS_PARKED == 0);
		M0_ALLOC_ARR(tp->xtm_worker_threads, tp->xtm_num_workers);
		if (tp->xtm_worker_threads == NULL)
			return M0_ERR(-ENOMEM);
	}

	/* allocate a state change work item */
	M0_ALLOC_PTR(wi_st_chg);
	if (wi_st_chg == NULL)
		return M0_ERR(-ENOMEM);
	m0_list_link_init(&wi_st_chg->xwi_link);
	wi_st_chg->xwi_op = M0_NET_XOP_STATE_CHANGE;
	wi_st_chg->xwi_next_state = M0_NET_XTM_STARTED;
	wi_st_chg->xwi_status = 0;

	/* create the end point (indirectly via the transport ops vector) */
	xprt = tm->ntm_dom->nd_xprt;
	rc = (*xprt->nx_ops->xo_end_point_create)(&wi_st_chg->xwi_nbe_ep,
						  tm, addr);
	if (rc != 0) {
		m0_free(wi_st_chg);
		return M0_RC(rc);
	}

	/* start worker threads */
	for (i = 0; i < tp->xtm_num_workers && rc == 0; ++i)
		rc = M0_THREAD_INIT(&tp->xtm_worker_threads[i],
				    struct m0_net_transfer_mc *, NULL,
				    &mem_xo_tm_worker, tm,
				    "mem_tm_worker%d", i);

	if (rc == 0) {
		/* set transition state and add the state change work item */
		tp->xtm_state = M0_NET_XTM_STARTING;
		mem_wi_add(wi_st_chg, tp);
	} else {
		tp->xtm_state = M0_NET_XTM_FAILED;
		m0_list_link_fini(&wi_st_chg->xwi_link);
		m0_free(wi_st_chg); /* fini cleans up threads */
	}

	return M0_RC(rc);
}

static int mem_xo_tm_stop(struct m0_net_transfer_mc *tm, bool cancel)
{
	struct m0_net_bulk_mem_tm_pvt *tp;
	struct m0_net_bulk_mem_work_item *wi_st_chg;

	M0_PRE(mem_tm_invariant(tm));
	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));

	tp = mem_tm_to_pvt(tm);
	if (tp->xtm_state >= M0_NET_XTM_STOPPING)
		return 0;
	/* allocate a state change work item */
	M0_ALLOC_PTR(wi_st_chg);
	if (wi_st_chg == NULL)
		return M0_ERR(-ENOMEM);
	m0_list_link_init(&wi_st_chg->xwi_link);
	wi_st_chg->xwi_op = M0_NET_XOP_STATE_CHANGE;
	wi_st_chg->xwi_next_state = M0_NET_XTM_STOPPED;
	if (cancel)
		m0_net__tm_cancel(tm);
	/* set transition state and add the state change work item */
	tp->xtm_state = M0_NET_XTM_STOPPING;
	mem_wi_add(wi_st_chg, tp);

	return 0;
}

static m0_bcount_t mem_xo_get_max_buffer_desc_size(const struct m0_net_domain *dom)
{
	M0_PRE(mem_dom_invariant(dom));

	return sizeof(struct mem_desc);
}

/* Internal methods of this transport; visible to derived transports. */
static const struct m0_net_bulk_mem_ops mem_xprt_methods = {
	.bmo_work_fn = {
		[M0_NET_XOP_STATE_CHANGE]    = mem_wf_state_change,
		[M0_NET_XOP_CANCEL_CB]       = mem_wf_cancel_cb,
		[M0_NET_XOP_MSG_RECV_CB]     = mem_wf_msg_recv_cb,
		[M0_NET_XOP_MSG_SEND]        = mem_wf_msg_send,
		[M0_NET_XOP_PASSIVE_BULK_CB] = mem_wf_passive_bulk_cb,
		[M0_NET_XOP_ACTIVE_BULK]     = mem_wf_active_bulk,
		[M0_NET_XOP_ERROR_CB]        = mem_wf_error_cb,
	},
	.bmo_ep_create                       = mem_ep_create,
	.bmo_ep_alloc                        = mem_ep_alloc,
	.bmo_ep_free                         = mem_ep_free,
	.bmo_ep_release                      = mem_xo_end_point_release,
	.bmo_ep_get                          = mem_ep_get,
	.bmo_wi_add                          = mem_wi_add,
	.bmo_buffer_in_bounds                = mem_buffer_in_bounds,
	.bmo_desc_create                     = mem_desc_create,
	.bmo_post_error                      = mem_post_error,
	.bmo_wi_post_buffer_event            = mem_wi_post_buffer_event,
};

/* External interface */
static const struct m0_net_xprt_ops mem_xo_xprt_ops = {
	.xo_dom_init                    = mem_xo_dom_init,
	.xo_dom_fini                    = mem_xo_dom_fini,
	.xo_get_max_buffer_size         = mem_xo_get_max_buffer_size,
	.xo_get_max_buffer_segment_size = mem_xo_get_max_buffer_segment_size,
	.xo_get_max_buffer_segments     = mem_xo_get_max_buffer_segments,
	.xo_end_point_create            = mem_xo_end_point_create,
	.xo_buf_register                = mem_xo_buf_register,
	.xo_buf_deregister              = mem_xo_buf_deregister,
	.xo_buf_add                     = mem_xo_buf_add,
	.xo_buf_del                     = mem_xo_buf_del,
	.xo_tm_init                     = mem_xo_tm_init,
	.xo_tm_fini                     = mem_xo_tm_fini,
	.xo_tm_start                    = mem_xo_tm_start,
	.xo_tm_stop                     = mem_xo_tm_stop,
	.xo_get_max_buffer_desc_size    = mem_xo_get_max_buffer_desc_size,
};

struct m0_net_xprt m0_net_bulk_mem_xprt = {
	.nx_name = "bulk-mem",
	.nx_ops  = &mem_xo_xprt_ops
};

#undef M0_TRACE_SUBSYSTEM

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
