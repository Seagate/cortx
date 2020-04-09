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

#include "lib/arith.h"
#include "lib/assert.h"
#include "lib/misc.h"
#include "lib/thread.h"
#include "ut/ut.h"

#include "net/bulk_emulation/mem_xprt_xo.c"
#include "net/bulk_emulation/st/ping.c"

/* Create buffers with different shapes but same total size.
   Also create identical buffers for exact shape testing.
*/
enum { NR_BUFS = 10 };

static void test_buf_copy(void)
{
	static struct {
		uint32_t    num_segs;
		m0_bcount_t seg_size;
	} shapes[NR_BUFS] = {
		[0] = { 1, 48 },
		[1] = { 1, 48 },
		[2] = { 2, 24 },
		[3] = { 2, 24 },
		[4] = { 3, 16 },
		[5] = { 3, 16 },
		[6] = { 4, 12 },
		[7] = { 4, 12 },
		[8] = { 6,  8 },
		[9] = { 6,  8 },
	};
	static const char *msg = "abcdefghijklmnopqrstuvwxyz0123456789"
		"ABCDEFGHIJK";
	size_t msglen = strlen(msg)+1;
	static struct m0_net_buffer bufs[NR_BUFS];
	int i;
	struct m0_net_buffer *nb;

	M0_SET_ARR0(bufs);
	for (i = 0; i < NR_BUFS; ++i) {
		M0_UT_ASSERT(msglen == shapes[i].num_segs * shapes[i].seg_size);
		M0_UT_ASSERT(m0_bufvec_alloc(&bufs[i].nb_buffer,
					     shapes[i].num_segs,
					     shapes[i].seg_size) == 0);
	}
	nb = &bufs[0]; /* single buffer */
	M0_UT_ASSERT(nb->nb_buffer.ov_vec.v_nr == 1);
	memcpy(nb->nb_buffer.ov_buf[0], msg, msglen);
	M0_UT_ASSERT(memcmp(nb->nb_buffer.ov_buf[0], msg, msglen) == 0);
	for (i = 1; i < NR_BUFS; ++i) {
		int j;
		const char *p = msg;
		M0_UT_ASSERT(mem_copy_buffer(&bufs[i],&bufs[i-1],msglen) == 0);
		M0_UT_ASSERT(bufs[i].nb_length == 0); /* does not set field */
		for (j = 0; j < bufs[i].nb_buffer.ov_vec.v_nr; ++j) {
			int k;
			char *q;
			for (k = 0;
			     k < bufs[i].nb_buffer.ov_vec.v_count[j]; ++k) {
				q = bufs[i].nb_buffer.ov_buf[j] + k;
				M0_UT_ASSERT(*p++ == *q);
			}
		}

	}
	for (i = 0; i < NR_BUFS; ++i)
		m0_bufvec_free(&bufs[i].nb_buffer);
}

void tf_tm_cb1(const struct m0_net_tm_event *ev);
static void test_ep(void)
{
	/* dom1 */
	static struct m0_net_domain dom1 = {
		.nd_xprt = NULL
	};
	static struct m0_net_tm_callbacks tm_cbs1 = {
		.ntc_event_cb = tf_tm_cb1
	};
	static struct m0_net_transfer_mc d1tm1 = {
		.ntm_callbacks = &tm_cbs1,
		.ntm_state = M0_NET_TM_UNDEFINED
	};
	struct m0_clink tmwait;
	static struct m0_net_end_point *ep1;
	static struct m0_net_end_point *ep2;
	static struct m0_net_end_point *ep3;
	const char *addr;

	M0_UT_ASSERT(!m0_net_domain_init(&dom1, &m0_net_bulk_mem_xprt));
	M0_UT_ASSERT(!m0_net_tm_init(&d1tm1, &dom1));

	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait);
	M0_UT_ASSERT(!m0_net_tm_start(&d1tm1, "255.255.255.255:54321"));
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	M0_UT_ASSERT(d1tm1.ntm_ep != NULL);

	addr = "255.255.255.255:65535:4294967295";
	M0_UT_ASSERT(m0_net_end_point_create(&ep1, &d1tm1, addr) == -EINVAL);
	addr = "255.255.255.255:65535";
	M0_UT_ASSERT(!m0_net_end_point_create(&ep1, &d1tm1, addr));
	M0_UT_ASSERT(strcmp(ep1->nep_addr, addr) == 0);
	M0_UT_ASSERT(m0_atomic64_get(&ep1->nep_ref.ref_cnt) == 1);
	M0_UT_ASSERT(ep1->nep_addr != addr);

	M0_UT_ASSERT(!m0_net_end_point_create(&ep2, &d1tm1, addr));
	M0_UT_ASSERT(strcmp(ep2->nep_addr, addr) == 0);
	M0_UT_ASSERT(ep2->nep_addr != addr);
	M0_UT_ASSERT(m0_atomic64_get(&ep2->nep_ref.ref_cnt) == 2);
	M0_UT_ASSERT(ep1 == ep2);

	M0_UT_ASSERT(!m0_net_end_point_create(&ep3, &d1tm1, addr));

	M0_UT_ASSERT(strcmp(ep3->nep_addr, "255.255.255.255:65535") == 0);
	M0_UT_ASSERT(strcmp(ep3->nep_addr, addr) == 0);
	M0_UT_ASSERT(ep3->nep_addr != addr);
	M0_UT_ASSERT(m0_atomic64_get(&ep3->nep_ref.ref_cnt) == 3);
	M0_UT_ASSERT(ep1 == ep3);

	m0_net_end_point_put(ep1);
	m0_net_end_point_put(ep2);
	m0_net_end_point_put(ep3);

	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait);
	M0_UT_ASSERT(!m0_net_tm_stop(&d1tm1, false));
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);

	m0_net_tm_fini(&d1tm1);
	m0_net_domain_fini(&dom1);
}

static enum m0_net_tm_ev_type cb_evt1;
static enum m0_net_queue_type cb_qt1;
static struct m0_net_buffer *cb_nb1;
static enum m0_net_tm_state cb_tms1;
static int32_t cb_status1;
void tf_tm_cb1(const struct m0_net_tm_event *ev)
{
	cb_evt1    = ev->nte_type;
	cb_nb1     = NULL;
	cb_qt1     = M0_NET_QT_NR;
	cb_tms1    = ev->nte_next_state;
	cb_status1 = ev->nte_status;
}

void tf_buf_cb1(const struct m0_net_buffer_event *ev)
{
	cb_evt1    = M0_NET_TEV_NR;
	cb_nb1     = ev->nbe_buffer;
	cb_qt1     = cb_nb1->nb_qtype;
	cb_tms1    = M0_NET_TM_UNDEFINED;
	cb_status1 = ev->nbe_status;
}

void tf_cbreset1(void)
{
	cb_evt1    = M0_NET_TEV_NR;
	cb_nb1     = NULL;
	cb_qt1     = M0_NET_QT_NR;
	cb_tms1    = M0_NET_TM_UNDEFINED;
	cb_status1 = 9999999;
}

static enum m0_net_tm_ev_type cb_evt2;
static enum m0_net_queue_type cb_qt2;
static struct m0_net_buffer *cb_nb2;
static enum m0_net_tm_state cb_tms2;
static int32_t cb_status2;
void tf_tm_cb2(const struct m0_net_tm_event *ev)
{
	cb_evt2    = ev->nte_type;
	cb_nb2     = NULL;
	cb_qt2     = M0_NET_QT_NR;
	cb_tms2    = ev->nte_next_state;
	cb_status2 = ev->nte_status;
}

void tf_buf_cb2(const struct m0_net_buffer_event *ev)
{
	cb_evt2    = M0_NET_TEV_NR;
	cb_nb2     = ev->nbe_buffer;
	cb_qt2     = cb_nb2->nb_qtype;
	cb_tms2    = M0_NET_TM_UNDEFINED;
	cb_status2 = ev->nbe_status;
}

void tf_cbreset2(void)
{
	cb_evt2    = M0_NET_TEV_NR;
	cb_nb2     = NULL;
	cb_qt2     = M0_NET_QT_NR;
	cb_tms2    = M0_NET_TM_UNDEFINED;
	cb_status2 = 9999999;
}

void tf_cbreset(void)
{
	tf_cbreset1();
	tf_cbreset2();
}

static void test_failure(void)
{
	/* some variables below are static to reduce kernel stack
	   consumption. */

	/* dom1 */
	static struct m0_net_domain dom1 = {
		.nd_xprt = NULL
	};
	static struct m0_net_tm_callbacks tm_cbs1 = {
		.ntc_event_cb = tf_tm_cb1
	};
	static struct m0_net_transfer_mc d1tm1 = {
		.ntm_callbacks = &tm_cbs1,
		.ntm_state = M0_NET_TM_UNDEFINED
	};
	static struct m0_net_buffer_callbacks buf_cbs1 = {
		.nbc_cb = {
			[M0_NET_QT_MSG_RECV]          = tf_buf_cb1,
			[M0_NET_QT_MSG_SEND]          = tf_buf_cb1,
			[M0_NET_QT_PASSIVE_BULK_RECV] = tf_buf_cb1,
			[M0_NET_QT_PASSIVE_BULK_SEND] = tf_buf_cb1,
			[M0_NET_QT_ACTIVE_BULK_RECV]  = tf_buf_cb1,
			[M0_NET_QT_ACTIVE_BULK_SEND]  = tf_buf_cb1,
		},
	};
	static struct m0_net_buffer d1nb1;
	static struct m0_net_buffer d1nb2;
	static struct m0_clink tmwait1;

	/* dom 2 */
	static struct m0_net_domain dom2 = {
		.nd_xprt = NULL
	};
	static const struct m0_net_tm_callbacks tm_cbs2 = {
		.ntc_event_cb = tf_tm_cb2
	};
	static struct m0_net_transfer_mc d2tm1 = {
		.ntm_callbacks = &tm_cbs2,
		.ntm_state = M0_NET_TM_UNDEFINED
	};
	static struct m0_net_transfer_mc d2tm2 = {
		.ntm_callbacks = &tm_cbs2,
		.ntm_state = M0_NET_TM_UNDEFINED
	};
	static const struct m0_net_buffer_callbacks buf_cbs2 = {
		.nbc_cb = {
			[M0_NET_QT_MSG_RECV]          = tf_buf_cb2,
			[M0_NET_QT_MSG_SEND]          = tf_buf_cb2,
			[M0_NET_QT_PASSIVE_BULK_RECV] = tf_buf_cb2,
			[M0_NET_QT_PASSIVE_BULK_SEND] = tf_buf_cb2,
			[M0_NET_QT_ACTIVE_BULK_RECV]  = tf_buf_cb2,
			[M0_NET_QT_ACTIVE_BULK_SEND]  = tf_buf_cb2,
		},
	};
	static struct m0_net_buffer d2nb1;
	static struct m0_net_buffer d2nb2;
	static m0_bcount_t d2nb2_len;
	static struct m0_clink tmwait2;

	static struct m0_net_end_point *ep;
	static struct m0_net_qstats qs;

	/* setup the first dom */
	M0_UT_ASSERT(!m0_net_domain_init(&dom1, &m0_net_bulk_mem_xprt));
	M0_UT_ASSERT(!m0_net_tm_init(&d1tm1, &dom1));
	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_tm_start(&d1tm1, "127.0.0.1:10"));
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(d1tm1.ntm_state == M0_NET_TM_STARTED);
	M0_UT_ASSERT(strcmp(d1tm1.ntm_ep->nep_addr, "127.0.0.1:10") == 0);
	M0_SET0(&d1nb1);
	M0_UT_ASSERT(!m0_bufvec_alloc(&d1nb1.nb_buffer, 4, 10));
	M0_UT_ASSERT(!m0_net_buffer_register(&d1nb1, &dom1));
	d1nb1.nb_callbacks = &buf_cbs1;
	M0_SET0(&d1nb2);
	M0_UT_ASSERT(!m0_bufvec_alloc(&d1nb2.nb_buffer, 1, 10));
	M0_UT_ASSERT(!m0_net_buffer_register(&d1nb2, &dom1));
	d1nb2.nb_callbacks = &buf_cbs1;

	/* setup the second dom */
	M0_UT_ASSERT(!m0_net_domain_init(&dom2, &m0_net_bulk_mem_xprt));
	M0_UT_ASSERT(!m0_net_tm_init(&d2tm1, &dom2));
	/* don't start the TM on port 20 yet */
	M0_UT_ASSERT(!m0_net_tm_init(&d2tm2, &dom2));
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm2.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_tm_start(&d2tm2, "127.0.0.1:21"));
	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(d2tm2.ntm_state == M0_NET_TM_STARTED);
	M0_UT_ASSERT(strcmp(d2tm2.ntm_ep->nep_addr, "127.0.0.1:21") == 0);

	M0_SET0(&d2nb1);
	M0_UT_ASSERT(!m0_bufvec_alloc(&d2nb1.nb_buffer, 4, 10));
	M0_UT_ASSERT(!m0_net_buffer_register(&d2nb1, &dom2));
	d2nb1.nb_callbacks = &buf_cbs2;
	M0_SET0(&d2nb2);
	M0_UT_ASSERT(!m0_bufvec_alloc(&d2nb2.nb_buffer, 1, 10));
	d2nb2_len = 1 * 10;
	M0_UT_ASSERT(!m0_net_buffer_register(&d2nb2, &dom2));
	d2nb2.nb_callbacks = &buf_cbs2;

	/* test failure situations */

	/* TEST
	   Send a message from d1tm1 to d2tm1 - should fail because
	   the destination TM not started.
	*/
	tf_cbreset();
	M0_UT_ASSERT(!m0_net_end_point_create(&ep, &d1tm1, "127.0.0.1:20"));
	M0_UT_ASSERT(strcmp(ep->nep_addr, "127.0.0.1:20") == 0);
	d1nb1.nb_qtype = M0_NET_QT_MSG_SEND;
	d1nb1.nb_ep = ep;
	d1nb1.nb_length = 10; /* don't care */
	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_buffer_add(&d1nb1, &d1tm1));
	m0_net_end_point_put(ep);
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_SEND);
	M0_UT_ASSERT(cb_nb1 == &d1nb1);
	M0_UT_ASSERT(cb_status1 == -ENETUNREACH);

	/* start the TM on port 20 in the second dom */
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_tm_start(&d2tm1, "127.0.0.1:20"));
	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(d2tm1.ntm_state == M0_NET_TM_STARTED);
	M0_UT_ASSERT(strcmp(d2tm1.ntm_ep->nep_addr, "127.0.0.1:20") == 0);

	/* TEST
	   Send a message from d1tm1 to d2tm1 - should fail because
	   no receive buffers available.
	   The failure count on the receive queue of d2tm1 should
	   be bumped, and an -ENOBUFS error callback delivered.
	*/
	tf_cbreset();
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_MSG_RECV,&qs,true));
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);

	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_MSG_SEND,&qs,true));
	M0_UT_ASSERT(!m0_net_end_point_create(&ep, &d1tm1, "127.0.0.1:20"));
	M0_UT_ASSERT(strcmp(ep->nep_addr, "127.0.0.1:20") == 0);
	d1nb1.nb_qtype = M0_NET_QT_MSG_SEND;
	d1nb1.nb_ep = ep;
	d1nb1.nb_length = 10; /* don't care */
	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_buffer_add(&d1nb1, &d1tm1));
	m0_net_end_point_put(ep);
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_SEND);
	M0_UT_ASSERT(cb_nb1 == &d1nb1);
	M0_UT_ASSERT(cb_status1 == -ENOBUFS);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_MSG_SEND,&qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);

	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_MSG_RECV,&qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 0);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);
	M0_UT_ASSERT(cb_evt2 == M0_NET_TEV_ERROR);
	M0_UT_ASSERT(cb_status2 == -ENOBUFS);

	/* TEST
	   Add a receive buffer in d2tm1.
	   Send a larger message from d1tm1 to d2tm1.
	   Both buffers should fail with -EMSGSIZE.
	*/
	tf_cbreset();
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_MSG_RECV,&qs,true));
	d2nb2.nb_qtype = M0_NET_QT_MSG_RECV;
	d2nb2.nb_ep = NULL;
	d2nb2.nb_min_receive_size = d2nb2_len;
	d2nb2.nb_max_receive_msgs = 1;
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_buffer_add(&d2nb2, &d2tm1));

	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_MSG_SEND,&qs,true));
	M0_UT_ASSERT(!m0_net_end_point_create(&ep, &d1tm1, "127.0.0.1:20"));
	M0_UT_ASSERT(strcmp(ep->nep_addr, "127.0.0.1:20") == 0);
	d1nb1.nb_qtype = M0_NET_QT_MSG_SEND;
	d1nb1.nb_ep = ep;
	d1nb1.nb_length = 40;
	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_buffer_add(&d1nb1, &d1tm1));
	m0_net_end_point_put(ep);
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_SEND);
	M0_UT_ASSERT(cb_nb1 == &d1nb1);
	M0_UT_ASSERT(cb_status1 == -EMSGSIZE);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_MSG_SEND,&qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);

	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(cb_qt2 == M0_NET_QT_MSG_RECV);
	M0_UT_ASSERT(cb_nb2 == &d2nb2);
	M0_UT_ASSERT(cb_status2 == -EMSGSIZE);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_MSG_RECV,&qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);

	/* TEST
	   Set up a passive receive buffer in one dom, and
	   try to actively receive from it.
	*/
	tf_cbreset();
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_PASSIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(!m0_net_end_point_create(&ep, &d2tm1, "127.0.0.1:10"));
	M0_UT_ASSERT(strcmp(ep->nep_addr, "127.0.0.1:10") == 0);
	d2nb1.nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
	d2nb1.nb_ep = ep;
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_buffer_add(&d2nb1, &d2tm1));
	M0_UT_ASSERT(d2nb1.nb_desc.nbd_len != 0);
	m0_net_end_point_put(ep);

	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_ACTIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(!m0_net_desc_copy(&d2nb1.nb_desc, &d1nb1.nb_desc));
	d1nb1.nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	d1nb1.nb_length = 10;
	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_buffer_add(&d1nb1, &d1tm1));
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_nb1 == &d1nb1);
	M0_UT_ASSERT(cb_status1 == -EPERM);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_ACTIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);
	m0_net_desc_free(&d1nb1.nb_desc);

	m0_net_buffer_del(&d2nb1, &d2tm1);
	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(cb_qt2 == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(cb_nb2 == &d2nb1);
	M0_UT_ASSERT(cb_status2 == -ECANCELED);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_PASSIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 1);
	m0_net_desc_free(&d2nb1.nb_desc);

	/* TEST
	   Set up a passive receive buffer in one dom, and
	   try to send a larger message from the other dom.
	*/
	tf_cbreset();
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_PASSIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(!m0_net_end_point_create(&ep, &d2tm1, "127.0.0.1:10"));
	M0_UT_ASSERT(strcmp(ep->nep_addr, "127.0.0.1:10") == 0);
	d2nb2.nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
	d2nb2.nb_ep = ep;
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_buffer_add(&d2nb2, &d2tm1));
	M0_UT_ASSERT(d2nb2.nb_desc.nbd_len != 0);
	m0_net_end_point_put(ep);

	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_ACTIVE_BULK_SEND,
					  &qs,true));
	M0_UT_ASSERT(!m0_net_desc_copy(&d2nb2.nb_desc, &d1nb1.nb_desc));
	d1nb1.nb_qtype = M0_NET_QT_ACTIVE_BULK_SEND;
	d1nb1.nb_length = 40; /* larger than d2nb2 */
	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_buffer_add(&d1nb1, &d1tm1));
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_SEND);
	M0_UT_ASSERT(cb_nb1 == &d1nb1);
	M0_UT_ASSERT(cb_status1 == -EFBIG);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_ACTIVE_BULK_SEND,
					  &qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);
	m0_net_desc_free(&d1nb1.nb_desc);

	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(cb_qt2 == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(cb_nb2 == &d2nb2);
	M0_UT_ASSERT(cb_status2 == -EFBIG);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_PASSIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);
	m0_net_desc_free(&d2nb2.nb_desc);

	/* TEST
	   Setup a passive send buffer and add it. Save the descriptor in the
	   active buffer of the other dom.  Do not start the active operation
	   yet. Del the passive buffer. Re-submit the same buffer for the same
	   passive operation. Try the active operation in the other dom, using
	   the original desc. Should fail because buffer id changes per add.
	 */
	tf_cbreset();
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_PASSIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(!m0_net_end_point_create(&ep, &d2tm1, "127.0.0.1:10"));
	M0_UT_ASSERT(strcmp(ep->nep_addr, "127.0.0.1:10") == 0);
	d2nb1.nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
	d2nb1.nb_ep = ep;
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_buffer_add(&d2nb1, &d2tm1));
	M0_UT_ASSERT(d2nb1.nb_desc.nbd_len != 0);
	/* m0_net_end_point_put(ep); reuse it on resubmit */

	/* copy the desc but don't start the active operation yet */
	M0_UT_ASSERT(!m0_net_desc_copy(&d2nb1.nb_desc, &d1nb1.nb_desc));

	/* cancel the original passive operation */
	m0_net_buffer_del(&d2nb1, &d2tm1);
	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(cb_qt2 == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(cb_nb2 == &d2nb1);
	M0_UT_ASSERT(cb_status2 == -ECANCELED);
	m0_net_desc_free(&d2nb1.nb_desc);

	/* resubmit */
	tf_cbreset2();
	d2nb1.nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
	d2nb1.nb_ep = ep;
	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_buffer_add(&d2nb1, &d2tm1));
	M0_UT_ASSERT(d2nb1.nb_desc.nbd_len != 0);
	m0_net_end_point_put(ep);

	/* descriptors should have changed */
	M0_UT_ASSERT(d1nb1.nb_desc.nbd_len != d2nb1.nb_desc.nbd_len ||
		     memcmp(d1nb1.nb_desc.nbd_data, d2nb1.nb_desc.nbd_data,
			    d1nb1.nb_desc.nbd_len) != 0);

	/* start the active operation */
	tf_cbreset1();
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_ACTIVE_BULK_SEND,
					  &qs,true));
	d1nb1.nb_qtype = M0_NET_QT_ACTIVE_BULK_SEND;
	d1nb1.nb_length = 10;
	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_buffer_add(&d1nb1, &d1tm1));
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_SEND);
	M0_UT_ASSERT(cb_nb1 == &d1nb1);
	M0_UT_ASSERT(cb_status1 == -ENOENT);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d1tm1,M0_NET_QT_ACTIVE_BULK_SEND,
					  &qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 1);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 1);
	M0_UT_ASSERT(qs.nqs_num_dels == 0);
	m0_net_desc_free(&d1nb1.nb_desc);

	m0_net_buffer_del(&d2nb1, &d2tm1);
	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(cb_qt2 == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(cb_nb2 == &d2nb1);
	M0_UT_ASSERT(cb_status2 == -ECANCELED);
	M0_UT_ASSERT(!m0_net_tm_stats_get(&d2tm1,M0_NET_QT_PASSIVE_BULK_RECV,
					  &qs,true));
	M0_UT_ASSERT(qs.nqs_num_f_events == 2);
	M0_UT_ASSERT(qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(qs.nqs_num_adds == 2);
	M0_UT_ASSERT(qs.nqs_num_dels == 2);
	m0_net_desc_free(&d2nb1.nb_desc);

	/* fini */
	m0_net_buffer_deregister(&d1nb1, &dom1);
	m0_bufvec_free(&d1nb1.nb_buffer);
	m0_net_buffer_deregister(&d1nb2, &dom1);
	m0_bufvec_free(&d1nb2.nb_buffer);
	m0_net_buffer_deregister(&d2nb1, &dom2);
	m0_bufvec_free(&d2nb1.nb_buffer);
	m0_net_buffer_deregister(&d2nb2, &dom2);
	m0_bufvec_free(&d2nb2.nb_buffer);

	m0_clink_init(&tmwait1, NULL);
	m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
	M0_UT_ASSERT(!m0_net_tm_stop(&d1tm1, false));
	m0_chan_wait(&tmwait1);
	m0_clink_del_lock(&tmwait1);
	M0_UT_ASSERT(d1tm1.ntm_state == M0_NET_TM_STOPPED);

	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm1.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_tm_stop(&d2tm1, false));
	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(d2tm1.ntm_state == M0_NET_TM_STOPPED);

	m0_clink_init(&tmwait2, NULL);
	m0_clink_add_lock(&d2tm2.ntm_chan, &tmwait2);
	M0_UT_ASSERT(!m0_net_tm_stop(&d2tm2, false));
	m0_chan_wait(&tmwait2);
	m0_clink_del_lock(&tmwait2);
	M0_UT_ASSERT(d2tm2.ntm_state == M0_NET_TM_STOPPED);

	m0_net_tm_fini(&d1tm1);
	m0_net_tm_fini(&d2tm1);
	m0_net_tm_fini(&d2tm2);

	m0_net_domain_fini(&dom1);
	m0_net_domain_fini(&dom2);
}

enum {
	PING_CLIENT_SEGMENTS = 8,
	PING_CLIENT_SEGMENT_SIZE = 512,
	PING_SERVER_SEGMENTS = 4,
	PING_SERVER_SEGMENT_SIZE = 1024,
	PING_NR_BUFS = 20
};
static int quiet_printf(const char *fmt, ...)
{
	return 0;
}

static struct ping_ops quiet_ops = {
    .pf = quiet_printf
};

static void test_ping(void)
{
	/* some variables below are static to reduce kernel stack
	   consumption. */

	static struct ping_ctx cctx = {
		.pc_ops = &quiet_ops,
		.pc_xprt = &m0_net_bulk_mem_xprt,
		.pc_nr_bufs = PING_NR_BUFS,
		.pc_segments = PING_CLIENT_SEGMENTS,
		.pc_seg_size = PING_CLIENT_SEGMENT_SIZE,
		.pc_tm = {
			.ntm_state     = M0_NET_TM_UNDEFINED
		}
	};
	static struct ping_ctx sctx = {
		.pc_ops = &quiet_ops,
		.pc_xprt = &m0_net_bulk_mem_xprt,
		.pc_nr_bufs = PING_NR_BUFS,
		.pc_segments = PING_SERVER_SEGMENTS,
		.pc_seg_size = PING_SERVER_SEGMENT_SIZE,
		.pc_tm = {
			.ntm_state     = M0_NET_TM_UNDEFINED
		}
	};
	int rc;
	struct m0_net_end_point *server_ep;
	struct m0_thread server_thread;
	int i;
	char *data;
	int len;

	m0_mutex_init(&sctx.pc_mutex);
	m0_cond_init(&sctx.pc_cond, &sctx.pc_mutex);
	m0_mutex_init(&cctx.pc_mutex);
	m0_cond_init(&cctx.pc_cond, &cctx.pc_mutex);

	M0_UT_ASSERT(ping_client_init(&cctx, &server_ep) == 0);
	/* client times out because server is not ready */
	M0_UT_ASSERT(ping_client_msg_send_recv(&cctx, server_ep, NULL) != 0);
	/* server runs in background thread */
	M0_SET0(&server_thread);
	rc = M0_THREAD_INIT(&server_thread, struct ping_ctx *, NULL,
			    &ping_server, &sctx, "ping_server");
	if (rc != 0) {
		M0_IMPOSSIBLE("failed to start ping server");
		return;
	}

	M0_UT_ASSERT(ping_client_msg_send_recv(&cctx, server_ep, NULL) == 0);
	M0_UT_ASSERT(ping_client_passive_recv(&cctx, server_ep) == 0);
	M0_UT_ASSERT(ping_client_passive_send(&cctx, server_ep, NULL) == 0);

	/* test sending/receiving a bigger payload */
	data = m0_alloc(PING_CLIENT_SEGMENTS * PING_CLIENT_SEGMENT_SIZE);
	M0_UT_ASSERT(data != NULL);
	len = (PING_CLIENT_SEGMENTS-1) * PING_CLIENT_SEGMENT_SIZE + 1;
	for (i = 0; i < len; ++i)
		data[i] = "abcdefghi"[i % 9];
	M0_UT_ASSERT(ping_client_msg_send_recv(&cctx, server_ep, data) == 0);
	M0_UT_ASSERT(ping_client_passive_send(&cctx, server_ep, data) == 0);

	M0_UT_ASSERT(ping_client_fini(&cctx, server_ep) == 0);

	ping_server_should_stop(&sctx);
	M0_UT_ASSERT(m0_thread_join(&server_thread) == 0);

	m0_cond_fini(&cctx.pc_cond);
	m0_mutex_fini(&cctx.pc_mutex);
	m0_cond_fini(&sctx.pc_cond);
	m0_mutex_fini(&sctx.pc_mutex);
	m0_free(data);
}

static void ntc_event_callback(const struct m0_net_tm_event *ev)
{
}

static void test_tm(void)
{
	static struct m0_net_domain dom1 = {
		.nd_xprt = NULL
	};
	const struct m0_net_tm_callbacks cbs1 = {
		.ntc_event_cb = ntc_event_callback
	};
	struct m0_net_transfer_mc d1tm1 = {
		.ntm_callbacks = &cbs1,
		.ntm_state = M0_NET_TM_UNDEFINED
	};
	struct m0_clink tmwait1;

	/* should be able to init/fini a dom back-to-back */
	M0_UT_ASSERT(!m0_net_domain_init(&dom1, &m0_net_bulk_mem_xprt));
	m0_net_domain_fini(&dom1);

	M0_UT_ASSERT(!m0_net_domain_init(&dom1, &m0_net_bulk_mem_xprt));
	M0_UT_ASSERT(!m0_net_tm_init(&d1tm1, &dom1));

	/* should be able to fini it immediately */
	m0_net_tm_fini(&d1tm1);
	M0_UT_ASSERT(d1tm1.ntm_state == M0_NET_TM_UNDEFINED);

	/* should be able to init it again */
	M0_UT_ASSERT(!m0_net_tm_init(&d1tm1, &dom1));
	M0_UT_ASSERT(d1tm1.ntm_state == M0_NET_TM_INITIALIZED);
	M0_UT_ASSERT(m0_list_contains(&dom1.nd_tms, &d1tm1.ntm_dom_linkage));

	/* check thread counts */
	M0_UT_ASSERT(m0_net_bulk_mem_tm_get_num_threads(&d1tm1) == 1);
	m0_net_bulk_mem_tm_set_num_threads(&d1tm1, 2);
	M0_UT_ASSERT(m0_net_bulk_mem_tm_get_num_threads(&d1tm1) == 2);

	/* fini */
	if (d1tm1.ntm_state > M0_NET_TM_INITIALIZED) {
		m0_clink_init(&tmwait1, NULL);
		m0_clink_add_lock(&d1tm1.ntm_chan, &tmwait1);
		M0_UT_ASSERT(!m0_net_tm_stop(&d1tm1, false));
		m0_chan_wait(&tmwait1);
		m0_clink_del_lock(&tmwait1);
		M0_UT_ASSERT(d1tm1.ntm_state == M0_NET_TM_STOPPED);
	}
	m0_net_tm_fini(&d1tm1);
	m0_net_domain_fini(&dom1);
}

struct m0_ut_suite m0_net_bulk_mem_ut = {
        .ts_name = "net-bulk-mem",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "net_bulk_mem_buf_copy_test", test_buf_copy },
		{ "net_bulk_mem_tm_test",       test_tm },
                { "net_bulk_mem_ep",            test_ep },
                { "net_bulk_mem_failure_tests", test_failure },
                { "net_bulk_mem_ping_tests",    test_ping },
                { NULL, NULL }
        }
};
M0_EXPORTED(m0_net_bulk_mem_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
