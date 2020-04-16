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
 * Adapted for LNet: 04/11/2012
 */

#include "lib/assert.h"
#include "lib/chan.h"
#include "lib/cond.h"
#include "lib/errno.h"
#include "lib/arith.h" /* max64u */
#include "lib/memory.h"
#include "lib/misc.h" /* M0_SET0 */
#include "net/net.h"
#include "net/lnet/lnet.h"
#include "net/lnet/st/ping.h"

#define DEF_RESPONSE "active pong"
#define DEF_SEND "passive ping"
#define SEND_RESP    " pong"
/** Descriptor for the tlist of buffers. */

enum {
	SEND_RETRIES = 3,
	IDBUF_LEN = M0_NET_LNET_XEP_ADDR_LEN + 16,
};

struct ping_work_item {
	enum m0_net_queue_type      pwi_type;
	struct m0_net_buffer       *pwi_nb;
	struct m0_list_link         pwi_link;
};

static struct m0_mutex qstats_mutex;
static struct m0_net_qstats ping_qs_total[M0_NET_QT_NR];
static uint64_t ping_qs_total_errors;
static uint64_t ping_qs_total_retries;

static void ping_print_qstats(struct nlx_ping_ctx *ctx,
			      struct m0_net_qstats *qp,
			      bool accumulate)

{
	int i;
	uint64_t hr;
	uint64_t min;
	uint64_t sec;
	uint64_t msec;
	static const char *qnames[M0_NET_QT_NR] = {
		"mRECV", "mSEND",
		"pRECV", "pSEND",
		"aRECV", "aSEND",
	};
	char tbuf[16];
	const char *lfmt =
"%5s %6lu %6lu %6lu %6lu %13s %18lu %9lu\n";
	const char *hfmt =
"Queue   #Add   #Del  #Succ  #Fail Time in Queue     Total  Bytes  "
"  Max Size\n"
"----- ------ ------ ------ ------ ------------- ------------------"
" ---------\n";

	m0_mutex_lock(&qstats_mutex);
	ctx->pc_ops->pf("%s statistics:\n", ctx->pc_ident);
	ctx->pc_ops->pf("%s", hfmt);
	for (i = 0; i < M0_NET_QT_NR; ++qp, ++i) {
		sec = m0_time_seconds(qp->nqs_time_in_queue);
		hr = sec / SEC_PER_HR;
		min = sec % SEC_PER_HR / SEC_PER_MIN;
		sec %= SEC_PER_MIN;
		msec = (m0_time_nanoseconds(qp->nqs_time_in_queue) +
			ONE_MILLION / 2) / ONE_MILLION;
		sprintf(tbuf, "%02lu:%02lu:%02lu.%03lu",
			(long unsigned int) hr,
			(long unsigned int) min,
			(long unsigned int) sec,
			(long unsigned int) msec);
		ctx->pc_ops->pf(lfmt,
				qnames[i],
				qp->nqs_num_adds, qp->nqs_num_dels,
				qp->nqs_num_s_events, qp->nqs_num_f_events,
				tbuf, qp->nqs_total_bytes, qp->nqs_max_bytes);
		if (accumulate) {
			struct m0_net_qstats *cqp = &ping_qs_total[i];

#define PING_QSTATS_CLIENT_TOTAL(f) cqp->nqs_##f += qp->nqs_##f
			PING_QSTATS_CLIENT_TOTAL(time_in_queue);
			PING_QSTATS_CLIENT_TOTAL(num_adds);
			PING_QSTATS_CLIENT_TOTAL(num_dels);
			PING_QSTATS_CLIENT_TOTAL(num_s_events);
			PING_QSTATS_CLIENT_TOTAL(num_f_events);
			PING_QSTATS_CLIENT_TOTAL(total_bytes);
#undef PING_QSTATS_CLIENT_TOTAL
			cqp->nqs_max_bytes =
				max64u(cqp->nqs_max_bytes, qp->nqs_max_bytes);
		}
	}
	if (ctx->pc_sync_events) {
		ctx->pc_ops->pf("%s Loops: Work=%lu Blocked=%lu\n",
				ctx->pc_ident,
				(unsigned long) ctx->pc_worked_count,
				(unsigned long) ctx->pc_blocked_count);
		ctx->pc_ops->pf("%s Wakeups: WorkQ=%lu Net=%lu\n",
				ctx->pc_ident,
				(unsigned long) ctx->pc_wq_signal_count,
				(unsigned long) ctx->pc_net_signal_count);
	}
	ctx->pc_ops->pf("%s errors: %lu\n", ctx->pc_ident,
			(long unsigned int)m0_atomic64_get(&ctx->pc_errors));
	ctx->pc_ops->pf("%s retries: %lu\n", ctx->pc_ident,
			(long unsigned int)m0_atomic64_get(&ctx->pc_retries));
	if (accumulate) {
		ping_qs_total_errors += m0_atomic64_get(&ctx->pc_errors);
		ping_qs_total_retries += m0_atomic64_get(&ctx->pc_retries);
	}

	m0_mutex_unlock(&qstats_mutex);
}

void nlx_ping_print_qstats_tm(struct nlx_ping_ctx *ctx, bool reset)
{
	struct m0_net_qstats qs[M0_NET_QT_NR];
	bool is_client;
	int rc;

	if (ctx->pc_tm.ntm_state < M0_NET_TM_INITIALIZED)
		return;
	is_client = ctx->pc_ident[0] == 'C';
	rc = m0_net_tm_stats_get(&ctx->pc_tm, M0_NET_QT_NR, qs, reset);
	M0_ASSERT(rc == 0);
	ping_print_qstats(ctx, qs, is_client);
}


int nlx_ping_print_qstats_total(const char *ident,
				const struct nlx_ping_ops *ops)
{
	struct nlx_ping_ctx *tctx;

	M0_ALLOC_PTR(tctx);
	if (tctx == NULL)
		return -ENOMEM;

	tctx->pc_ops   = ops;
	tctx->pc_ident = ident;

	m0_atomic64_set(&tctx->pc_errors, ping_qs_total_errors);
	m0_atomic64_set(&tctx->pc_retries, ping_qs_total_retries);
	ping_print_qstats(tctx, ping_qs_total, false);

	m0_free(tctx);
	return 0;
}

uint64_t nlx_ping_parse_uint64(const char *s)
{
	const char *fmt2 = "%lu%c";
	const char *fmt1 = "%lu";
	uint64_t len;
	uint64_t mult = 1;
	char unit;

	if (s == NULL)
		return 0;
	if (sscanf(s, fmt2, &len, &unit) == 2) {
		if (unit == 'K')
			mult = 1 << 10;
		else if (unit == 'M')
			mult = 1 << 20;
		else if (unit == 'G')
			mult = 1 << 30;
		else if (unit == 'T')
			mult = (uint64_t) 1 << 40;
		else
			M0_ASSERT(unit == 'K' || unit == 'M' || unit == 'G' ||
				  unit == 'T');
	} else
		sscanf(s, fmt1, &len);
	M0_ASSERT(len != 0);
	M0_ASSERT(mult >= 1);
	return len * mult;
}

static void ping_sleep_secs(int secs)
{
	if (secs != 0)
		m0_nanosleep(m0_time(secs, 0), NULL);
}

static int alloc_buffers(int num, uint32_t segs, m0_bcount_t segsize,
			 unsigned shift, struct m0_net_buffer **out)
{
	struct m0_net_buffer *nbs;
	struct m0_net_buffer *nb;
	int                   i;
	int                   rc = 0;

	M0_ALLOC_ARR(nbs, num);
	if (nbs == NULL)
		return -ENOMEM;
	for (i = 0; i < num; ++i) {
		nb = &nbs[i];
		rc = m0_bufvec_alloc_aligned(&nb->nb_buffer, segs, segsize,
					     shift);
		if (rc != 0)
			break;
	}

	if (rc == 0)
		*out = nbs;
	else {
		while (--i >= 0)
			m0_bufvec_free_aligned(&nbs[i].nb_buffer, shift);
		m0_free(nbs);
	}
	return rc;
}

/**
   Get a unused buffer from the context buffer list.
   On success, marks the buffer as in-use and returns it.
   @retval ptr the buffer
   @retval NULL failure
 */
static struct m0_net_buffer *ping_buf_get(struct nlx_ping_ctx *ctx)
{
	int i;
	struct m0_net_buffer *nb;

	m0_mutex_lock(&ctx->pc_mutex);
	for (i = 0; i < ctx->pc_nr_bufs; ++i)
		if (!m0_bitmap_get(&ctx->pc_nbbm, i)) {
			m0_bitmap_set(&ctx->pc_nbbm, i, true);
			break;
		}
	m0_mutex_unlock(&ctx->pc_mutex);

	if (i == ctx->pc_nr_bufs)
		return NULL;

	nb = &ctx->pc_nbs[i];
	M0_ASSERT(nb->nb_flags == M0_NET_BUF_REGISTERED);
	return nb;
}

/**
   Releases a buffer back to the free buffer pool.
   The buffer is marked as not in-use.
 */
static void ping_buf_put(struct nlx_ping_ctx *ctx, struct m0_net_buffer *nb)
{
	int i = nb - &ctx->pc_nbs[0];
	M0_ASSERT(i >= 0 && i < ctx->pc_nr_bufs);
	M0_ASSERT((nb->nb_flags & ~M0_NET_BUF_REGISTERED) == 0);

	m0_mutex_lock(&ctx->pc_mutex);
	M0_ASSERT(m0_bitmap_get(&ctx->pc_nbbm, i));
	m0_bitmap_set(&ctx->pc_nbbm, i, false);
	m0_mutex_unlock(&ctx->pc_mutex);
}

/** encode a string message into a net buffer, not zero-copy */
static int encode_msg(struct m0_net_buffer *nb, const char *str)
{
	char *bp;
	m0_bcount_t len = strlen(str) + 1; /* include trailing nul */
	m0_bcount_t copied;
	struct m0_bufvec in = M0_BUFVEC_INIT_BUF((void **) &str, &len);
	struct m0_bufvec_cursor incur;
	struct m0_bufvec_cursor cur;

	nb->nb_length = len + 1;
	m0_bufvec_cursor_init(&cur, &nb->nb_buffer);
	bp = m0_bufvec_cursor_addr(&cur);
	*bp = 'm';
	M0_ASSERT(!m0_bufvec_cursor_move(&cur, 1));
	m0_bufvec_cursor_init(&incur, &in);
	copied = m0_bufvec_cursor_copy(&cur, &incur, len);
	M0_ASSERT(copied == len);
	return 0;
}

/** encode a descriptor into a net buffer, not zero-copy */
static int encode_desc(struct m0_net_buffer *nb,
		       bool send_desc,
		       unsigned passive_size,
		       const struct m0_net_buf_desc *desc)
{
	struct m0_bufvec_cursor cur;
	char *bp;
	m0_bcount_t step;

	m0_bufvec_cursor_init(&cur, &nb->nb_buffer);
	bp = m0_bufvec_cursor_addr(&cur);
	*bp = send_desc ? 's' : 'r';
	M0_ASSERT(!m0_bufvec_cursor_move(&cur, 1));
	bp = m0_bufvec_cursor_addr(&cur);

	/* only support sending net_desc in single chunks in this test */
	step = m0_bufvec_cursor_step(&cur);
	M0_ASSERT(step >= 18 + desc->nbd_len);
	nb->nb_length = 19 + desc->nbd_len;

	bp += sprintf(bp, "%08u", desc->nbd_len);
	++bp;				/* +nul */
	bp += sprintf(bp, "%08u", passive_size);
	++bp;				/* +nul */
	memcpy(bp, desc->nbd_data, desc->nbd_len);
	return 0;
}

enum ping_msg_type {
	/** client wants to do passive send, server will active recv */
	PM_SEND_DESC,
	/** client wants to do active send, server will passive recv */
	PM_RECV_DESC,
	PM_MSG
};

struct ping_msg {
	enum ping_msg_type pm_type;
	union {
		char *pm_str;
		struct m0_net_buf_desc pm_desc;
	} pm_u;
	unsigned pm_passive_size;
};

/** decode a net buffer, allocates memory and copies payload */
static int decode_msg(struct m0_net_buffer *nb,
		      m0_bcount_t nb_len,
		      m0_bcount_t nb_offset,
		      struct ping_msg *msg)
{
	struct m0_bufvec_cursor cur;
	char *bp;
	m0_bcount_t step;

	m0_bufvec_cursor_init(&cur, &nb->nb_buffer);
	if (nb_offset > 0)
		m0_bufvec_cursor_move(&cur, nb_offset);
	bp = m0_bufvec_cursor_addr(&cur);
	M0_ASSERT(*bp == 'm' || *bp == 's' || *bp == 'r');
	M0_ASSERT(!m0_bufvec_cursor_move(&cur, 1));
	if (*bp == 'm') {
		m0_bcount_t len = nb_len - 1;
		void *str;
		struct m0_bufvec out = M0_BUFVEC_INIT_BUF(&str, &len);
		struct m0_bufvec_cursor outcur;

		msg->pm_type = PM_MSG;
		str = msg->pm_u.pm_str = m0_alloc(len + 1);
		M0_ASSERT(str != NULL);
		m0_bufvec_cursor_init(&outcur, &out);
		step = m0_bufvec_cursor_copy(&outcur, &cur, len);
		M0_ASSERT(step == len);
	} else {
		int len;
		char nine[9];
		int i;
		void *buf;
		m0_bcount_t buflen;
		struct m0_bufvec bv = M0_BUFVEC_INIT_BUF(&buf, &buflen);
		struct m0_bufvec_cursor bv_cur;

		msg->pm_type = (*bp == 's') ? PM_SEND_DESC : PM_RECV_DESC;

		buf = nine;
		buflen = 9;
		m0_bufvec_cursor_init(&bv_cur, &bv);
		i = m0_bufvec_cursor_copy(&bv_cur, &cur, 9);
		M0_ASSERT(i == 9);
		i = sscanf(nine, "%u", &len);
		M0_ASSERT(i == 1);

		m0_bufvec_cursor_init(&bv_cur, &bv);
		i = m0_bufvec_cursor_copy(&bv_cur, &cur, 9);
		M0_ASSERT(i == 9);
		i = sscanf(nine, "%u", &msg->pm_passive_size);
		M0_ASSERT(i == 1);

		buflen = len;
		msg->pm_u.pm_desc.nbd_len = len;
		msg->pm_u.pm_desc.nbd_data = buf = m0_alloc(len);
		M0_ASSERT(buf != NULL);
		m0_bufvec_cursor_init(&bv_cur, &bv);
		i = m0_bufvec_cursor_copy(&bv_cur, &cur, len);
		M0_ASSERT(i == len);
	}
	return 0;
}

static void msg_free(struct ping_msg *msg)
{
	if (msg->pm_type != PM_MSG)
		m0_net_desc_free(&msg->pm_u.pm_desc);
	else
		m0_free(msg->pm_u.pm_str);
}

static void ping_print_interfaces(struct nlx_ping_ctx *ctx)
{
	int i;
	ctx->pc_ops->pf("%s: Available interfaces\n", ctx->pc_ident);
	for (i = 0; ctx->pc_interfaces[i] != NULL; ++i)
		ctx->pc_ops->pf("\t%s\n", ctx->pc_interfaces[i]);
	return;
}

static struct nlx_ping_ctx *
buffer_event_to_ping_ctx(const struct m0_net_buffer_event *ev)
{
	struct m0_net_buffer *nb = ev->nbe_buffer;
	M0_ASSERT(nb != NULL);
	return container_of(nb->nb_tm, struct nlx_ping_ctx, pc_tm);
}

/* client callbacks */
static void c_m_recv_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	int rc;
	int len;
	struct ping_work_item *wi;
	struct ping_msg msg;

	M0_ASSERT(ev->nbe_buffer->nb_qtype == M0_NET_QT_MSG_RECV);
	PING_OUT(ctx, 1, "%s: Msg Recv CB\n", ctx->pc_ident);

	if (ev->nbe_status < 0) {
		if (ev->nbe_status == -ECANCELED)
			PING_OUT(ctx, 1, "%s: msg recv canceled\n",
				 ctx->pc_ident);
		else {
			ctx->pc_ops->pf("%s: msg recv error: %d\n",
					ctx->pc_ident, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);
		}
	} else {
		ev->nbe_buffer->nb_length = ev->nbe_length;
		M0_ASSERT(ev->nbe_offset == 0);
		rc = decode_msg(ev->nbe_buffer, ev->nbe_length, 0, &msg);
		M0_ASSERT(rc == 0);

		if (msg.pm_type != PM_MSG)
			M0_IMPOSSIBLE("Client: got desc\n");

		len = strlen(msg.pm_u.pm_str);
		if (strlen(msg.pm_u.pm_str) < 32)
			PING_OUT(ctx, 1, "%s: got msg: %s\n",
				 ctx->pc_ident, msg.pm_u.pm_str);
		else
			PING_OUT(ctx, 1, "%s: got msg: %u bytes\n",
				 ctx->pc_ident, len + 1);

		if (ctx->pc_compare_buf != NULL) {
			int l = strlen(ctx->pc_compare_buf);
			M0_ASSERT(strlen(msg.pm_u.pm_str) == l + 5);
			M0_ASSERT(strncmp(ctx->pc_compare_buf,
					  msg.pm_u.pm_str, l) == 0);
			M0_ASSERT(strcmp(&msg.pm_u.pm_str[l], SEND_RESP) == 0);
			PING_OUT(ctx, 1, "%s: msg bytes validated\n",
				 ctx->pc_ident);
		}
		msg_free(&msg);
	}

	ping_buf_put(ctx, ev->nbe_buffer);

	M0_ALLOC_PTR(wi);
	m0_list_link_init(&wi->pwi_link);
	wi->pwi_type = M0_NET_QT_MSG_RECV;

	m0_mutex_lock(&ctx->pc_mutex);
	m0_list_add(&ctx->pc_work_queue, &wi->pwi_link);
	m0_cond_signal(&ctx->pc_cond);
	m0_mutex_unlock(&ctx->pc_mutex);
}

static void c_m_send_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	struct ping_work_item *wi;

	M0_ASSERT(ev->nbe_buffer->nb_qtype == M0_NET_QT_MSG_SEND);
	PING_OUT(ctx, 1, "%s: Msg Send CB\n", ctx->pc_ident);

	if (ev->nbe_status < 0) {
		if (ev->nbe_status == -ECANCELED)
			PING_OUT(ctx, 1, "%s: msg send canceled\n",
				 ctx->pc_ident);
		else {
			ctx->pc_ops->pf("%s: msg send error: %d\n",
					ctx->pc_ident, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);
		}

		/* let main app deal with it */
		M0_ALLOC_PTR(wi);
		m0_list_link_init(&wi->pwi_link);
		wi->pwi_type = M0_NET_QT_MSG_SEND;
		wi->pwi_nb = ev->nbe_buffer;

		m0_mutex_lock(&ctx->pc_mutex);
		m0_list_add(&ctx->pc_work_queue, &wi->pwi_link);
		m0_cond_signal(&ctx->pc_cond);
		m0_mutex_unlock(&ctx->pc_mutex);
	} else {
		m0_net_desc_free(&ev->nbe_buffer->nb_desc);
		ping_buf_put(ctx, ev->nbe_buffer);

		M0_ALLOC_PTR(wi);
		m0_list_link_init(&wi->pwi_link);
		wi->pwi_type = M0_NET_QT_MSG_SEND;

		m0_mutex_lock(&ctx->pc_mutex);
		m0_list_add(&ctx->pc_work_queue, &wi->pwi_link);
		m0_cond_signal(&ctx->pc_cond);
		m0_mutex_unlock(&ctx->pc_mutex);
	}
}

static void c_p_recv_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	int rc;
	int len;
	struct ping_work_item *wi;
	struct ping_msg msg;

	M0_ASSERT(ev->nbe_buffer->nb_qtype == M0_NET_QT_PASSIVE_BULK_RECV);
	PING_OUT(ctx, 1, "%s: Passive Recv CB\n", ctx->pc_ident);

	if (ev->nbe_status < 0) {
		if (ev->nbe_status == -ECANCELED)
			PING_OUT(ctx, 1, "%s: passive recv canceled\n",
				 ctx->pc_ident);
		else {
			ctx->pc_ops->pf("%s: passive recv error: %d\n",
					ctx->pc_ident, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);
		}
	} else {
		ev->nbe_buffer->nb_length = ev->nbe_length;
		M0_ASSERT(ev->nbe_offset == 0);
		rc = decode_msg(ev->nbe_buffer, ev->nbe_length, 0, &msg);
		M0_ASSERT(rc == 0);

		if (msg.pm_type != PM_MSG)
			M0_IMPOSSIBLE("Client: got desc\n");
		len = strlen(msg.pm_u.pm_str);
		if (strlen(msg.pm_u.pm_str) < 32)
			PING_OUT(ctx, 1, "%s: got data: %s\n",
				 ctx->pc_ident, msg.pm_u.pm_str);
		else
			PING_OUT(ctx, 1, "%s: got data: %u bytes\n",
				 ctx->pc_ident, len + 1);
		M0_ASSERT(ev->nbe_length == len + 2);
		if (strcmp(msg.pm_u.pm_str, DEF_RESPONSE) != 0) {
			int i;
			for (i = 0; i < len - 1; ++i) {
				if (msg.pm_u.pm_str[i] != "abcdefghi"[i % 9]) {
					PING_ERR("%s: data diff @ offset %i: "
						 "%c != %c\n",
						 ctx->pc_ident, i,
						 msg.pm_u.pm_str[i],
						 "abcdefghi"[i % 9]);
					m0_atomic64_inc(&ctx->pc_errors);
					break;
				}
			}
			if (i == len - 1)
				PING_OUT(ctx, 1, "%s: data bytes validated\n",
					 ctx->pc_ident);
		}
		msg_free(&msg);
	}

	m0_net_desc_free(&ev->nbe_buffer->nb_desc);
	ping_buf_put(ctx, ev->nbe_buffer);

	M0_ALLOC_PTR(wi);
	m0_list_link_init(&wi->pwi_link);
	wi->pwi_type = M0_NET_QT_PASSIVE_BULK_RECV;

	m0_mutex_lock(&ctx->pc_mutex);
	m0_list_add(&ctx->pc_work_queue, &wi->pwi_link);
	m0_cond_signal(&ctx->pc_cond);
	m0_mutex_unlock(&ctx->pc_mutex);
}

static void c_p_send_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	struct ping_work_item *wi;

	M0_ASSERT(ev->nbe_buffer->nb_qtype == M0_NET_QT_PASSIVE_BULK_SEND);
	PING_OUT(ctx, 1, "%s: Passive Send CB\n", ctx->pc_ident);

	if (ev->nbe_status < 0) {
		if (ev->nbe_status == -ECANCELED)
			PING_OUT(ctx, 1, "%s: passive send canceled\n",
				 ctx->pc_ident);
		else {
			ctx->pc_ops->pf("%s: passive send error: %d\n",
					ctx->pc_ident, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);
		}
	}

	m0_net_desc_free(&ev->nbe_buffer->nb_desc);
	ping_buf_put(ctx, ev->nbe_buffer);

	M0_ALLOC_PTR(wi);
	m0_list_link_init(&wi->pwi_link);
	wi->pwi_type = M0_NET_QT_PASSIVE_BULK_SEND;

	m0_mutex_lock(&ctx->pc_mutex);
	m0_list_add(&ctx->pc_work_queue, &wi->pwi_link);
	m0_cond_signal(&ctx->pc_cond);
	m0_mutex_unlock(&ctx->pc_mutex);
}

static void c_a_recv_cb(const struct m0_net_buffer_event *ev)
{
	M0_ASSERT(ev->nbe_buffer != NULL &&
		  ev->nbe_buffer->nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_IMPOSSIBLE("Client: Active Recv CB\n");
}

static void c_a_send_cb(const struct m0_net_buffer_event *ev)
{
	M0_ASSERT(ev->nbe_buffer != NULL &&
		  ev->nbe_buffer->nb_qtype == M0_NET_QT_ACTIVE_BULK_SEND);
	M0_IMPOSSIBLE("Client: Active Send CB\n");
}

static void event_cb(const struct m0_net_tm_event *ev)
{
	struct nlx_ping_ctx *ctx = container_of(ev->nte_tm,
						struct nlx_ping_ctx,
						pc_tm);

	if (ev->nte_type == M0_NET_TEV_STATE_CHANGE) {
		const char *s = "unexpected";
		if (ev->nte_next_state == M0_NET_TM_STARTED)
			s = "started";
		else if (ev->nte_next_state == M0_NET_TM_STOPPED)
			s = "stopped";
		else if (ev->nte_next_state == M0_NET_TM_FAILED)
			s = "FAILED";
		PING_OUT(ctx, 1, "%s: Event CB state change to %s, status %d\n",
			 ctx->pc_ident, s, ev->nte_status);
		ctx->pc_status = ev->nte_status;
	} else if (ev->nte_type == M0_NET_TEV_ERROR) {
		PING_OUT(ctx, 0, "%s: Event CB for error %d\n",
			 ctx->pc_ident, ev->nte_status);
		m0_atomic64_inc(&ctx->pc_errors);
	} else if (ev->nte_type == M0_NET_TEV_DIAGNOSTIC)
		PING_OUT(ctx, 0, "%s: Event CB for diagnostic %d\n",
			 ctx->pc_ident, ev->nte_status);
}

static bool server_stop = false;

static struct m0_net_buffer_callbacks cbuf_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]          = c_m_recv_cb,
		[M0_NET_QT_MSG_SEND]          = c_m_send_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = c_p_recv_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = c_p_send_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = c_a_recv_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = c_a_send_cb
	},
};

static struct m0_net_tm_callbacks ctm_cb = {
	.ntc_event_cb = event_cb
};

static void server_event_ident(char *buf, size_t len, const char *ident,
			       const struct m0_net_buffer_event *ev)
{
	const struct m0_net_end_point *ep = NULL;
	if (ev != NULL && ev->nbe_buffer != NULL) {
		if (ev->nbe_buffer->nb_qtype == M0_NET_QT_MSG_RECV) {
			if (ev->nbe_status == 0)
				ep = ev->nbe_ep;
		} else {
			ep = ev->nbe_buffer->nb_ep;
		}
	}
	if (ep != NULL)
		snprintf(buf, len, "%s (peer %s)", ident, ep->nep_addr);
	else
		snprintf(buf, len, "%s", ident);
}

static struct m0_atomic64 s_msg_recv_counter;

/* server callbacks */
static void s_m_recv_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	int rc;
	struct ping_work_item *wi;
	struct ping_msg msg;
	int64_t count;
	char idbuf[IDBUF_LEN];
	int bulk_delay = ctx->pc_server_bulk_delay;


	M0_ASSERT(ev->nbe_buffer != NULL &&
		  ev->nbe_buffer->nb_qtype == M0_NET_QT_MSG_RECV);
	server_event_ident(idbuf, ARRAY_SIZE(idbuf), ctx->pc_ident, ev);
	count = m0_atomic64_add_return(&s_msg_recv_counter, 1);
	PING_OUT(ctx, 1, "%s: Msg Recv CB %ld 0x%lx\n", idbuf, (long int) count,
		 (unsigned long int) ev->nbe_buffer->nb_flags);
	if (ev->nbe_status < 0) {
		if (ev->nbe_status == -ECANCELED && server_stop)
			PING_OUT(ctx, 1, "%s: msg recv canceled on shutdown\n",
				 idbuf);
		else {
			ctx->pc_ops->pf("%s: msg recv error: %d\n",
					idbuf, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);

			ev->nbe_buffer->nb_ep = NULL;
			M0_ASSERT(!(ev->nbe_buffer->nb_flags &
				    M0_NET_BUF_QUEUED));
			ev->nbe_buffer->nb_timeout = M0_TIME_NEVER;
			rc = m0_net_buffer_add(ev->nbe_buffer, &ctx->pc_tm);
			M0_ASSERT(rc == 0);
		}
	} else {
		struct m0_net_buffer *nb;
		unsigned bulk_size = ctx->pc_segments * ctx->pc_seg_size;

		rc = decode_msg(ev->nbe_buffer, ev->nbe_length, ev->nbe_offset,
				&msg);
		M0_ASSERT(rc == 0);

		nb = ping_buf_get(ctx);
		if (nb == NULL) {
			ctx->pc_ops->pf("%s: dropped msg, "
					"no buffer available\n", idbuf);
			m0_atomic64_inc(&ctx->pc_errors);
		} else if ((msg.pm_type == PM_SEND_DESC ||
			    msg.pm_type == PM_RECV_DESC) &&
			   msg.pm_passive_size > bulk_size) {
			const char *req = msg.pm_type == PM_SEND_DESC ?
				"receive" : "send";
			ctx->pc_ops->pf("%s: dropped msg, bulk %s request "
					"too large (%u)\n", idbuf, req,
					msg.pm_passive_size);
			m0_atomic64_inc(&ctx->pc_errors);
			ping_buf_put(ctx, nb);
		} else {
			M0_ALLOC_PTR(wi);
			nb->nb_ep = ev->nbe_ep; /* save for later, if set */
			ev->nbe_buffer->nb_ep = NULL;
			m0_list_link_init(&wi->pwi_link);
			wi->pwi_nb = nb;
			if (msg.pm_type == PM_SEND_DESC) {
				PING_OUT(ctx, 1, "%s: got desc for "
					 "active recv: sz=%u\n", idbuf,
					 msg.pm_passive_size);
				wi->pwi_type = M0_NET_QT_ACTIVE_BULK_RECV;
				nb->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
				nb->nb_length = msg.pm_passive_size;
				m0_net_desc_copy(&msg.pm_u.pm_desc,
						 &nb->nb_desc);
				nb->nb_ep = NULL; /* not needed */
				M0_ASSERT(rc == 0);
				if (bulk_delay != 0) {
					PING_OUT(ctx, 1, "%s: delay %d secs\n",
						 idbuf, bulk_delay);
					ping_sleep_secs(bulk_delay);
				}
			} else if (msg.pm_type == PM_RECV_DESC) {
				PING_OUT(ctx, 1, "%s: got desc for "
					 "active send: sz=%u\n", idbuf,
					 msg.pm_passive_size);
				wi->pwi_type = M0_NET_QT_ACTIVE_BULK_SEND;
				nb->nb_qtype = M0_NET_QT_ACTIVE_BULK_SEND;
				nb->nb_length = 0;
				m0_net_desc_copy(&msg.pm_u.pm_desc,
						 &nb->nb_desc);
				nb->nb_ep = NULL; /* not needed */
				/* reuse encode_msg for convenience */
				if (msg.pm_passive_size == 0)
					rc = encode_msg(nb, DEF_RESPONSE);
				else {
					char *bp;
					int i;
					bp = m0_alloc(msg.pm_passive_size);
					M0_ASSERT(bp != NULL);
					for (i = 0;
					     i < msg.pm_passive_size -
						     PING_MSG_OVERHEAD; ++i)
						bp[i] = "abcdefghi"[i % 9];
					PING_OUT(ctx, 1, "%s: sending data "
						 "%u bytes\n", idbuf,
						 msg.pm_passive_size);
					rc = encode_msg(nb, bp);
					m0_free(bp);
					M0_ASSERT(rc == 0);
				}
				M0_ASSERT(rc == 0);
				if (bulk_delay != 0) {
					PING_OUT(ctx, 1, "%s: delay %d secs\n",
						 idbuf, bulk_delay);
					ping_sleep_secs(bulk_delay);
				}
			} else {
				char *data;
				int len = strlen(msg.pm_u.pm_str);
				if (strlen(msg.pm_u.pm_str) < 32)
					PING_OUT(ctx, 1, "%s: got msg: %s\n",
						 idbuf, msg.pm_u.pm_str);
				else
					PING_OUT(ctx, 1, "%s: got msg: "
						 "%u bytes\n",
						 idbuf, len + 1);

				/* queue wi to send back ping response */
				data = m0_alloc(len + 6);
				m0_net_end_point_get(nb->nb_ep);
				wi->pwi_type = M0_NET_QT_MSG_SEND;
				nb->nb_qtype = M0_NET_QT_MSG_SEND;
				strcpy(data, msg.pm_u.pm_str);
				strcat(data, SEND_RESP);
				rc = encode_msg(nb, data);
				m0_free(data);
				M0_ASSERT(rc == 0);
			}
			m0_mutex_lock(&ctx->pc_mutex);
			m0_list_add(&ctx->pc_work_queue, &wi->pwi_link);
			if (ctx->pc_sync_events)
				m0_chan_signal(&ctx->pc_wq_chan);
			else
				m0_cond_signal(&ctx->pc_cond);
			m0_mutex_unlock(&ctx->pc_mutex);
		}
		ev->nbe_buffer->nb_ep = NULL;
		if (!(ev->nbe_buffer->nb_flags & M0_NET_BUF_QUEUED)) {
			ev->nbe_buffer->nb_timeout = M0_TIME_NEVER;
			PING_OUT(ctx, 1, "%s: re-queuing buffer\n",
				 ctx->pc_ident);
			rc = m0_net_buffer_add(ev->nbe_buffer, &ctx->pc_tm);
			M0_ASSERT(rc == 0);
		}

		msg_free(&msg);
	}
}

static void s_m_send_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	char idbuf[IDBUF_LEN];

	M0_ASSERT(ev->nbe_buffer->nb_qtype == M0_NET_QT_MSG_SEND);
	server_event_ident(idbuf, ARRAY_SIZE(idbuf), ctx->pc_ident, ev);
	PING_OUT(ctx, 1, "%s: Msg Send CB\n", idbuf);

	if (ev->nbe_status < 0) {
		/* no retries here */
		if (ev->nbe_status == -ECANCELED)
			PING_OUT(ctx, 1, "%s: msg send canceled\n", idbuf);
		else {
			ctx->pc_ops->pf("%s: msg send error: %d\n",
					idbuf, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);
		}
	}

	m0_net_end_point_put(ev->nbe_buffer->nb_ep);
	ev->nbe_buffer->nb_ep = NULL;

	ping_buf_put(ctx, ev->nbe_buffer);
}

static void s_p_recv_cb(const struct m0_net_buffer_event *ev)
{
	M0_ASSERT(ev->nbe_buffer != NULL &&
		  ev->nbe_buffer->nb_qtype == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_IMPOSSIBLE("Server: Passive Recv CB\n");
}

static void s_p_send_cb(const struct m0_net_buffer_event *ev)
{
	M0_ASSERT(ev->nbe_buffer != NULL &&
		  ev->nbe_buffer->nb_qtype == M0_NET_QT_PASSIVE_BULK_SEND);
	M0_IMPOSSIBLE("Server: Passive Send CB\n");
}

static void s_a_recv_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	int rc;
	int len;
	struct ping_msg msg;
	char idbuf[IDBUF_LEN];

	M0_ASSERT(ev->nbe_buffer->nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV);
	server_event_ident(idbuf, ARRAY_SIZE(idbuf), ctx->pc_ident, ev);
	PING_OUT(ctx, 1, "%s: Active Recv CB\n", idbuf);

	if (ev->nbe_status < 0) {
		/* no retries here */
		if (ev->nbe_status == -ECANCELED)
			PING_OUT(ctx, 1, "%s: active recv canceled\n", idbuf);
		else {
			ctx->pc_ops->pf("%s: active recv error: %d\n",
					idbuf, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);
		}
	} else {
		ev->nbe_buffer->nb_length = ev->nbe_length;
		M0_ASSERT(ev->nbe_offset == 0);
		rc = decode_msg(ev->nbe_buffer, ev->nbe_length, 0, &msg);
		M0_ASSERT(rc == 0);

		if (msg.pm_type != PM_MSG)
			M0_IMPOSSIBLE("Server: got desc\n");
		len = strlen(msg.pm_u.pm_str);
		if (len < 32)
			PING_OUT(ctx, 1, "%s: got data: %s\n",
				 idbuf, msg.pm_u.pm_str);
		else
			PING_OUT(ctx, 1, "%s: got data: %u bytes\n",
				 idbuf, len + 1);
		M0_ASSERT(ev->nbe_length == len + 2);
		if (strcmp(msg.pm_u.pm_str, DEF_SEND) != 0) {
			int i;
			for (i = 0; i < len - 1; ++i) {
				if (msg.pm_u.pm_str[i] != "abcdefghi"[i % 9]) {
					PING_ERR("%s: data diff @ offset %i: "
						 "%c != %c\n", idbuf, i,
						 msg.pm_u.pm_str[i],
						 "abcdefghi"[i % 9]);
					m0_atomic64_inc(&ctx->pc_errors);
					break;
				}
			}
			if (i == len - 1)
				PING_OUT(ctx, 1, "%s: data bytes validated\n",
					 idbuf);
		}

		msg_free(&msg);
	}

	m0_net_desc_free(&ev->nbe_buffer->nb_desc);
	ping_buf_put(ctx, ev->nbe_buffer);
}

static void s_a_send_cb(const struct m0_net_buffer_event *ev)
{
	struct nlx_ping_ctx *ctx = buffer_event_to_ping_ctx(ev);
	char idbuf[IDBUF_LEN];

	M0_ASSERT(ev->nbe_buffer->nb_qtype == M0_NET_QT_ACTIVE_BULK_SEND);
	server_event_ident(idbuf, ARRAY_SIZE(idbuf), ctx->pc_ident, ev);
	PING_OUT(ctx, 1, "%s: Active Send CB\n", idbuf);

	if (ev->nbe_status < 0) {
		/* no retries here */
		if (ev->nbe_status == -ECANCELED)
			PING_OUT(ctx, 1, "%s: active send canceled\n", idbuf);
		else {
			ctx->pc_ops->pf("%s: active send error: %d\n",
					idbuf, ev->nbe_status);
			m0_atomic64_inc(&ctx->pc_errors);
		}
	}

	m0_net_desc_free(&ev->nbe_buffer->nb_desc);
	ping_buf_put(ctx, ev->nbe_buffer);
}

static struct m0_net_buffer_callbacks sbuf_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]          = s_m_recv_cb,
		[M0_NET_QT_MSG_SEND]          = s_m_send_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = s_p_recv_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND] = s_p_send_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = s_a_recv_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = s_a_send_cb
	},
};

static struct m0_net_tm_callbacks stm_cb = {
	.ntc_event_cb = event_cb
};

static void ping_fini(struct nlx_ping_ctx *ctx);

static bool ping_workq_clink_cb(struct m0_clink *cl)
{
	struct nlx_ping_ctx *ctx =
		container_of(cl, struct nlx_ping_ctx, pc_wq_clink);
	++ctx->pc_wq_signal_count;
	return false;
}

static bool ping_net_clink_cb(struct m0_clink *cl)
{
	struct nlx_ping_ctx *ctx =
		container_of(cl, struct nlx_ping_ctx, pc_net_clink);
	++ctx->pc_net_signal_count;
	return false;
}

/**
   Initialise a ping client or server.
   Calls all the required m0_net APIs in the correct order, with
   cleanup on failure.
   On success, the transfer machine is started.
   @param ctx the client/server context.  pc_xprt, pc_nr_bufs, pc_tm,
   pc_hostname, pc_port and pc_id must be initialised by the caller.
   @retval 0 success
   @retval -errno failure
 */
static int ping_init(struct nlx_ping_ctx *ctx)
{
	int                i;
	int                rc;
	char               addr[M0_NET_LNET_XEP_ADDR_LEN];
	struct m0_clink    tmwait;
	uint64_t           bsz;

	m0_list_init(&ctx->pc_work_queue);
	m0_atomic64_set(&ctx->pc_errors, 0);
	m0_atomic64_set(&ctx->pc_retries, 0);
	ctx->pc_interfaces = NULL;
	if (ctx->pc_sync_events) {
		m0_chan_init(&ctx->pc_wq_chan, &ctx->pc_mutex);
		m0_chan_init(&ctx->pc_net_chan, &ctx->pc_mutex);

		m0_clink_init(&ctx->pc_wq_clink, &ping_workq_clink_cb);
		m0_clink_attach(&ctx->pc_net_clink, &ctx->pc_wq_clink,
				&ping_net_clink_cb); /* group */

		m0_clink_add_lock(&ctx->pc_wq_chan, &ctx->pc_wq_clink);
		m0_clink_add_lock(&ctx->pc_net_chan, &ctx->pc_net_clink);
	}

	rc = m0_net_domain_init(&ctx->pc_dom, ctx->pc_xprt);
	if (rc != 0) {
		PING_ERR("domain init failed: %d\n", rc);
		goto fail;
	}

	if (ctx->pc_dom_debug > 0)
		m0_net_lnet_dom_set_debug(&ctx->pc_dom, ctx->pc_dom_debug);

	rc = m0_net_lnet_ifaces_get(&ctx->pc_dom, &ctx->pc_interfaces);
	if (rc != 0) {
		PING_ERR("failed to load interface names: %d\n", rc);
		goto fail;
	}
	M0_ASSERT(ctx->pc_interfaces != NULL);

	if (ctx->pc_interfaces[0] == NULL) {
		PING_ERR("no interfaces defined locally\n");
		goto fail;
	}

	ctx->pc_seg_shift = PING_SEGMENT_SHIFT;
	ctx->pc_seg_size = PING_SEGMENT_SIZE;
        bsz = ctx->pc_bulk_size > 0 ? ctx->pc_bulk_size : PING_DEF_BUFFER_SIZE;
	ctx->pc_segments = bsz / ctx->pc_seg_size +
		(bsz % ctx->pc_seg_size != 0 ? 1 : 0);
	M0_ASSERT(ctx->pc_segments * ctx->pc_seg_size <= PING_MAX_BUFFER_SIZE);
	rc = alloc_buffers(ctx->pc_nr_bufs, ctx->pc_segments, ctx->pc_seg_size,
			   ctx->pc_seg_shift, &ctx->pc_nbs);
	if (rc != 0) {
		PING_ERR("buffer allocation %u X %lu([%u][%u]) failed: %d\n",
			 ctx->pc_nr_bufs, (unsigned long) bsz,
			 ctx->pc_segments, ctx->pc_seg_size, rc);
		goto fail;
	}
	rc = m0_bitmap_init(&ctx->pc_nbbm, ctx->pc_nr_bufs);
	if (rc != 0) {
		PING_ERR("buffer bitmap allocation failed: %d\n", rc);
		goto fail;
	}
	M0_ASSERT(ctx->pc_buf_callbacks != NULL);
	for (i = 0; i < ctx->pc_nr_bufs; ++i) {
		rc = m0_net_buffer_register(&ctx->pc_nbs[i], &ctx->pc_dom);
		if (rc != 0) {
			PING_ERR("buffer register failed: %d\n", rc);
			goto fail;
		}
		ctx->pc_nbs[i].nb_callbacks = ctx->pc_buf_callbacks;
	}

	if (ctx->pc_network == NULL) {
		ctx->pc_network = ctx->pc_interfaces[0];
		for (i = 0; ctx->pc_interfaces[i] != NULL; ++i) {
			if (strstr(ctx->pc_interfaces[i], "@lo") != NULL)
				continue;
			ctx->pc_network = ctx->pc_interfaces[i]; /* 1st !@lo */
			break;
		}
	}
	if (ctx->pc_rnetwork == NULL)
		ctx->pc_rnetwork = ctx->pc_network;

	if (ctx->pc_tmid >= 0)
		snprintf(addr, ARRAY_SIZE(addr), "%s:%u:%u:%u", ctx->pc_network,
			 ctx->pc_pid, ctx->pc_portal, ctx->pc_tmid);
	else
		snprintf(addr, ARRAY_SIZE(addr), "%s:%u:%u:*", ctx->pc_network,
			 ctx->pc_pid, ctx->pc_portal);

	/** @todo replace gmc and ctx */
	rc = m0_net_tm_init(&ctx->pc_tm, &ctx->pc_dom);
	if (rc != 0) {
		PING_ERR("transfer machine init failed: %d\n", rc);
		goto fail;
	}

	if (ctx->pc_tm_debug > 0)
		m0_net_lnet_tm_set_debug(&ctx->pc_tm, ctx->pc_tm_debug);

	if (ctx->pc_sync_events) {
		rc = m0_net_buffer_event_deliver_synchronously(&ctx->pc_tm);
		M0_ASSERT(rc == 0);
	}

	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&ctx->pc_tm.ntm_chan, &tmwait);
	rc = m0_net_tm_start(&ctx->pc_tm, addr);
	if (rc != 0) {
		PING_ERR("transfer machine start failed: %d\n", rc);
		goto fail;
	}

	/* wait for tm to notify it has started */
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	if (ctx->pc_tm.ntm_state != M0_NET_TM_STARTED) {
		rc = ctx->pc_status;
		if (rc == 0)
			rc = -EINVAL;
		PING_ERR("transfer machine start failed: %d\n", rc);
		goto fail;
	}

	return rc;
fail:
	ping_fini(ctx);
	return rc;
}

static inline bool ping_tm_timedwait(struct nlx_ping_ctx *ctx,
				     struct m0_clink *cl,
				     m0_time_t timeout)
{
	bool signalled = false;
	if (timeout == M0_TIME_NEVER) {
		if (ctx->pc_sync_events) {
			do {
				timeout = m0_time_from_now(0, 50 * ONE_MILLION);
				signalled = m0_chan_timedwait(cl, timeout);
				m0_net_buffer_event_deliver_all(&ctx->pc_tm);
			} while (!signalled);
		} else
			m0_chan_wait(cl);
	} else {
		signalled = m0_chan_timedwait(cl, timeout);
		if (ctx->pc_sync_events)
			m0_net_buffer_event_deliver_all(&ctx->pc_tm);
	}
	return signalled;
}

static inline void ping_tm_wait(struct nlx_ping_ctx *ctx,
				struct m0_clink *cl)
{
	ping_tm_timedwait(ctx, cl, M0_TIME_NEVER);
}

static void ping_fini(struct nlx_ping_ctx *ctx)
{
	struct m0_list_link *link;
	struct ping_work_item *wi;

	if (ctx->pc_tm.ntm_state != M0_NET_TM_UNDEFINED) {
		if (ctx->pc_tm.ntm_state != M0_NET_TM_FAILED) {
			struct m0_clink tmwait;
			m0_clink_init(&tmwait, NULL);
			m0_clink_add_lock(&ctx->pc_tm.ntm_chan, &tmwait);
			m0_net_tm_stop(&ctx->pc_tm, true);
			while (ctx->pc_tm.ntm_state != M0_NET_TM_STOPPED) {
				/* wait for it to stop */
				m0_time_t timeout = m0_time_from_now(0,
							     50 * ONE_MILLION);
				m0_chan_timedwait(&tmwait, timeout);
			}
			m0_clink_del_lock(&tmwait);
		}

		if (ctx->pc_ops->pqs != NULL)
			(*ctx->pc_ops->pqs)(ctx, false);

		m0_net_tm_fini(&ctx->pc_tm);
	}
	if (ctx->pc_nbs != NULL) {
		int i;
		for (i = 0; i < ctx->pc_nr_bufs; ++i) {
			struct m0_net_buffer *nb = &ctx->pc_nbs[i];
			M0_ASSERT(nb->nb_flags == M0_NET_BUF_REGISTERED);
			m0_net_buffer_deregister(nb, &ctx->pc_dom);
			m0_bufvec_free_aligned(&nb->nb_buffer,
					       ctx->pc_seg_shift);
		}
		m0_free(ctx->pc_nbs);
		m0_bitmap_fini(&ctx->pc_nbbm);
	}
	if (ctx->pc_interfaces != NULL)
		m0_net_lnet_ifaces_put(&ctx->pc_dom, &ctx->pc_interfaces);
	if (ctx->pc_dom.nd_xprt != NULL)
		m0_net_domain_fini(&ctx->pc_dom);

	while (!m0_list_is_empty(&ctx->pc_work_queue)) {
		link = m0_list_first(&ctx->pc_work_queue);
		wi = m0_list_entry(link, struct ping_work_item, pwi_link);
		m0_list_del(&wi->pwi_link);
		m0_free(wi);
	}
	if (ctx->pc_sync_events) {
		m0_clink_del_lock(&ctx->pc_net_clink);
		m0_clink_del_lock(&ctx->pc_wq_clink);

		m0_clink_fini(&ctx->pc_net_clink);
		m0_clink_fini(&ctx->pc_wq_clink);

		m0_chan_fini_lock(&ctx->pc_net_chan);
		m0_chan_fini_lock(&ctx->pc_wq_chan);
	}

	m0_list_fini(&ctx->pc_work_queue);
}

static void set_msg_timeout(struct nlx_ping_ctx *ctx,
			    struct m0_net_buffer *nb)
{
	if (ctx->pc_msg_timeout > 0) {
		PING_OUT(ctx, 1, "%s: setting msg nb_timeout to %ds\n",
			 ctx->pc_ident, ctx->pc_msg_timeout);
		nb->nb_timeout = m0_time_from_now(ctx->pc_msg_timeout, 0);
	} else {
		nb->nb_timeout = M0_TIME_NEVER;
	}
}

static void set_bulk_timeout(struct nlx_ping_ctx *ctx,
			     struct m0_net_buffer *nb)
{
	if (ctx->pc_bulk_timeout > 0) {
		PING_OUT(ctx, 1, "%s: setting bulk nb_timeout to %ds\n",
			 ctx->pc_ident, ctx->pc_bulk_timeout);
		nb->nb_timeout = m0_time_from_now(ctx->pc_bulk_timeout, 0);
	} else {
		nb->nb_timeout = M0_TIME_NEVER;
	}
}

static void nlx_ping_server_work(struct nlx_ping_ctx *ctx)
{
	struct m0_list_link *link;
	struct ping_work_item *wi;
	int rc;

	M0_ASSERT(m0_mutex_is_locked(&ctx->pc_mutex));

	while (!m0_list_is_empty(&ctx->pc_work_queue)) {
		link = m0_list_first(&ctx->pc_work_queue);
		wi = m0_list_entry(link, struct ping_work_item,
				   pwi_link);
		switch (wi->pwi_type) {
		case M0_NET_QT_MSG_SEND:
			set_msg_timeout(ctx, wi->pwi_nb);
			rc = m0_net_buffer_add(wi->pwi_nb, &ctx->pc_tm);
			break;
		case M0_NET_QT_ACTIVE_BULK_SEND:
		case M0_NET_QT_ACTIVE_BULK_RECV:
			set_bulk_timeout(ctx, wi->pwi_nb);
			rc = m0_net_buffer_add(wi->pwi_nb, &ctx->pc_tm);
			break;
		default:
			M0_IMPOSSIBLE("unexpected wi->pwi_type");
			rc = -EINVAL;
			break;
		}
		if (rc != 0) {
			m0_atomic64_inc(&ctx->pc_errors);
			ctx->pc_ops->pf("%s buffer_add(%d) failed %d\n",
					ctx->pc_ident, wi->pwi_type, rc);
		}
		m0_list_del(&wi->pwi_link);
		m0_free(wi);
	}
}

static void nlx_ping_server_async(struct nlx_ping_ctx *ctx)
{
	m0_time_t timeout;

	M0_ASSERT(m0_mutex_is_locked(&ctx->pc_mutex));

	while (!server_stop) {
		nlx_ping_server_work(ctx);
		timeout = m0_time_from_now(5, 0);
		m0_cond_timedwait(&ctx->pc_cond, timeout);
	}

	return;
}

static void nlx_ping_server_sync(struct nlx_ping_ctx *ctx)
{
	m0_time_t timeout;

	M0_ASSERT(m0_mutex_is_locked(&ctx->pc_mutex));

	while (!server_stop) {
		while (!server_stop &&
		       m0_list_is_empty(&ctx->pc_work_queue) &&
		       !m0_net_buffer_event_pending(&ctx->pc_tm)) {
			++ctx->pc_blocked_count;
			m0_net_buffer_event_notify(&ctx->pc_tm,
						   &ctx->pc_net_chan);
			m0_mutex_unlock(&ctx->pc_mutex);
			/* wait on the channel group */
			timeout = m0_time_from_now(15, 0);
			m0_chan_timedwait(&ctx->pc_wq_clink, timeout);
			m0_mutex_lock(&ctx->pc_mutex);
		}

		++ctx->pc_worked_count;

		if (m0_net_buffer_event_pending(&ctx->pc_tm)) {
			m0_mutex_unlock(&ctx->pc_mutex);
			/* deliver events synchronously on this thread */
			m0_net_buffer_event_deliver_all(&ctx->pc_tm);
			m0_mutex_lock(&ctx->pc_mutex);
		}

		if (server_stop) {
			PING_OUT(ctx, 1, "%s stopping\n", ctx->pc_ident);
			break;
		}

		nlx_ping_server_work(ctx);
	}

	return;
}


static void nlx_ping_server(struct nlx_ping_ctx *ctx)
{
	int i;
	int rc;
	struct m0_net_buffer *nb;
	struct m0_clink tmwait;
	unsigned int num_recv_bufs = max32u(ctx->pc_nr_bufs / 8, 2);
	int buf_size;

	ctx->pc_tm.ntm_callbacks = &stm_cb;
	ctx->pc_buf_callbacks = &sbuf_cb;

	ctx->pc_ident = "Server";
	M0_ASSERT(ctx->pc_nr_bufs > 2);
	if (ctx->pc_nr_recv_bufs > ctx->pc_nr_bufs / 2)
		ctx->pc_nr_recv_bufs = num_recv_bufs;
	if (ctx->pc_nr_recv_bufs < 2)
		ctx->pc_nr_recv_bufs = num_recv_bufs;
	num_recv_bufs = ctx->pc_nr_recv_bufs;
	M0_ASSERT(num_recv_bufs >= 2);
	rc = ping_init(ctx);
	M0_ASSERT(rc == 0);
	M0_ASSERT(ctx->pc_network != NULL);
	ping_print_interfaces(ctx);
	ctx->pc_ops->pf("Server end point: %s\n", ctx->pc_tm.ntm_ep->nep_addr);

	buf_size = ctx->pc_segments * ctx->pc_seg_size;
	if (ctx->pc_max_recv_msgs > 0 && ctx->pc_min_recv_size <= 0)
		ctx->pc_min_recv_size = buf_size / ctx->pc_max_recv_msgs;
	else if (ctx->pc_min_recv_size > 0 && ctx->pc_max_recv_msgs <= 0)
		ctx->pc_max_recv_msgs = buf_size / ctx->pc_min_recv_size;

	if (ctx->pc_min_recv_size < PING_DEF_MIN_RECV_SIZE ||
	    ctx->pc_min_recv_size > buf_size ||
	    ctx->pc_max_recv_msgs < 1)
		ctx->pc_max_recv_msgs = ctx->pc_min_recv_size = -1;

	if (ctx->pc_min_recv_size <= 0 && ctx->pc_max_recv_msgs <= 0) {
		ctx->pc_min_recv_size = PING_DEF_MIN_RECV_SIZE;
		ctx->pc_max_recv_msgs = buf_size / ctx->pc_min_recv_size;
	}
	M0_ASSERT(ctx->pc_min_recv_size >= PING_DEF_MIN_RECV_SIZE);
	M0_ASSERT(ctx->pc_max_recv_msgs >= 1);
	ctx->pc_ops->pf("%s buffer parameters:\n"
			"\t  total buffers=%u\n"
			"\t    buffer size=%u\n"
			"\treceive buffers=%u\n"
			"\t  min_recv_size=%d\n"
			"\t  max_recv_msgs=%d\n",
			ctx->pc_ident, ctx->pc_nr_bufs, buf_size, num_recv_bufs,
			ctx->pc_min_recv_size, ctx->pc_max_recv_msgs);

	m0_mutex_lock(&ctx->pc_mutex);
	for (i = 0; i < num_recv_bufs; ++i) {
		nb = &ctx->pc_nbs[i];
		nb->nb_qtype = M0_NET_QT_MSG_RECV;
		nb->nb_timeout = M0_TIME_NEVER;
		nb->nb_ep = NULL;
		nb->nb_min_receive_size = ctx->pc_min_recv_size;
		nb->nb_max_receive_msgs = ctx->pc_max_recv_msgs;
		rc = m0_net_buffer_add(nb, &ctx->pc_tm);
		m0_bitmap_set(&ctx->pc_nbbm, i, true);
		M0_ASSERT(rc == 0);
	}

	/* startup synchronization handshake */
	ctx->pc_ready = true;
	m0_cond_signal(&ctx->pc_cond);
	while (ctx->pc_ready)
		m0_cond_wait(&ctx->pc_cond);

	if (ctx->pc_sync_events)
		nlx_ping_server_sync(ctx);
	else
		nlx_ping_server_async(ctx);
	m0_mutex_unlock(&ctx->pc_mutex);

	/* dequeue recv buffers */
	m0_clink_init(&tmwait, NULL);

	for (i = 0; i < num_recv_bufs; ++i) {
		nb = &ctx->pc_nbs[i];
		m0_clink_add_lock(&ctx->pc_tm.ntm_chan, &tmwait);
		m0_net_buffer_del(nb, &ctx->pc_tm);
		m0_bitmap_set(&ctx->pc_nbbm, i, false);
		ping_tm_wait(ctx, &tmwait);
		m0_clink_del_lock(&tmwait);
	}

	/* wait for active buffers to flush */
	m0_clink_add_lock(&ctx->pc_tm.ntm_chan, &tmwait);
	for (i = 0; i < M0_NET_QT_NR; ++i)
		while (!m0_net_tm_tlist_is_empty(&ctx->pc_tm.ntm_q[i])) {
			PING_OUT(ctx, 1, "waiting for queue %d to empty\n", i);
			ping_tm_wait(ctx, &tmwait);
		}
	m0_clink_del_lock(&tmwait);
	m0_clink_fini(&tmwait);

	ping_fini(ctx);
	server_stop = false;
}

void nlx_ping_server_should_stop(struct nlx_ping_ctx *ctx)
{
	m0_mutex_lock(&ctx->pc_mutex);
	server_stop = true;
	if (ctx->pc_sync_events)
		m0_chan_signal(&ctx->pc_wq_chan);
	else
		m0_cond_signal(&ctx->pc_cond);
	m0_mutex_unlock(&ctx->pc_mutex);
}

void nlx_ping_server_spawn(struct m0_thread *server_thread,
			   struct nlx_ping_ctx *sctx)
{
	int rc;

	sctx->pc_xprt = &m0_net_lnet_xprt;
	sctx->pc_pid = M0_NET_LNET_PID;

	m0_mutex_lock(&sctx->pc_mutex);
	M0_SET0(server_thread);
	rc = M0_THREAD_INIT(server_thread, struct nlx_ping_ctx *,
			    NULL, &nlx_ping_server, sctx, "ping_server");
	M0_ASSERT(rc == 0);
	while (!sctx->pc_ready)
		m0_cond_wait(&sctx->pc_cond);
	sctx->pc_ready = false;
	m0_cond_signal(&sctx->pc_cond);
	m0_mutex_unlock(&sctx->pc_mutex);
}

/**
   Test an RPC-like exchange, sending data in a message to the server and
   getting back a response.
   @param ctx client context
   @param server_ep endpoint of the server
   @retval 0 successful test
   @retval -errno failed to send to server
 */
static int nlx_ping_client_msg_send_recv(struct nlx_ping_ctx *ctx,
					 struct m0_net_end_point *server_ep,
					 const char *data)
{
	int rc;
	struct m0_net_buffer *nb;
	struct m0_list_link *link;
	struct ping_work_item *wi;
	int recv_done = 0;
	int retries = SEND_RETRIES;
	m0_time_t session_timeout = M0_TIME_NEVER;

	if (data == NULL)
		data = "ping";
	ctx->pc_compare_buf = data;

	PING_OUT(ctx, 1, "%s: starting msg send/recv sequence\n",
		 ctx->pc_ident);
	/* queue buffer for response, must do before sending msg */
	nb = ping_buf_get(ctx);
	M0_ASSERT(nb != NULL);
	nb->nb_qtype = M0_NET_QT_MSG_RECV;
	nb->nb_timeout = M0_TIME_NEVER;
	nb->nb_ep = NULL;
	nb->nb_min_receive_size = ctx->pc_segments * ctx->pc_seg_size;
	nb->nb_max_receive_msgs = 1;
	rc = m0_net_buffer_add(nb, &ctx->pc_tm);
	M0_ASSERT(rc == 0);

	nb = ping_buf_get(ctx);
	M0_ASSERT(nb != NULL);
	rc = encode_msg(nb, data);
	nb->nb_qtype = M0_NET_QT_MSG_SEND;
	nb->nb_ep = server_ep;
	M0_ASSERT(rc == 0);
	set_msg_timeout(ctx, nb);
	rc = m0_net_buffer_add(nb, &ctx->pc_tm);
	M0_ASSERT(rc == 0);

	/* wait for receive response to complete */
	m0_mutex_lock(&ctx->pc_mutex);
	while (1) {
		while (!m0_list_is_empty(&ctx->pc_work_queue)) {
			link = m0_list_first(&ctx->pc_work_queue);
			wi = m0_list_entry(link, struct ping_work_item,
					   pwi_link);
			m0_list_del(&wi->pwi_link);
			if (wi->pwi_type == M0_NET_QT_MSG_RECV) {
				ctx->pc_compare_buf = NULL;
				recv_done++;
			} else if (wi->pwi_type == M0_NET_QT_MSG_SEND &&
				   wi->pwi_nb != NULL) {
				m0_time_t delay;
				/* send error, retry a few times */
				if (retries == 0) {
					ctx->pc_compare_buf = NULL;
					ctx->pc_ops->pf("%s: send failed, "
							"no more retries\n",
							ctx->pc_ident);
					m0_mutex_unlock(&ctx->pc_mutex);
					ping_buf_put(ctx, nb);
					m0_free(wi);
					m0_atomic64_inc(&ctx->pc_errors);
					return -ETIMEDOUT;
				}
				delay = m0_time(SEND_RETRIES + 1 - retries, 0);
				--retries;
				m0_nanosleep(delay, NULL);
				m0_atomic64_inc(&ctx->pc_retries);
				set_msg_timeout(ctx, nb);
				rc = m0_net_buffer_add(nb, &ctx->pc_tm);
				M0_ASSERT(rc == 0);
			} else if (wi->pwi_type == M0_NET_QT_MSG_SEND) {
				recv_done++;
				if (ctx->pc_msg_timeout > 0)
					session_timeout =
						m0_time_from_now(
							ctx->pc_msg_timeout, 0);
			}
			m0_free(wi);
		}
		if (recv_done == 2)
			break;
		if (session_timeout == M0_TIME_NEVER)
			m0_cond_wait(&ctx->pc_cond);
		else if (!m0_cond_timedwait(&ctx->pc_cond,
					    session_timeout)) {
			ctx->pc_ops->pf("%s: Receive TIMED OUT\n",
					ctx->pc_ident);
			rc = -ETIMEDOUT;
			m0_atomic64_inc(&ctx->pc_errors);
			break;
		}
	}

	m0_mutex_unlock(&ctx->pc_mutex);
	return rc;
}

static int nlx_ping_client_passive_recv(struct nlx_ping_ctx *ctx,
					struct m0_net_end_point *server_ep)
{
	int rc;
	struct m0_net_buffer *nb;
	struct m0_net_buf_desc nbd;
	struct m0_list_link *link;
	struct ping_work_item *wi;
	int recv_done = 0;
	int retries = SEND_RETRIES;

	PING_OUT(ctx, 1, "%s: starting passive recv sequence\n", ctx->pc_ident);
	/* queue our passive receive buffer */
	nb = ping_buf_get(ctx);
	M0_ASSERT(nb != NULL);
	nb->nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
	nb->nb_ep = server_ep;
	set_bulk_timeout(ctx, nb);
	rc = m0_net_buffer_add(nb, &ctx->pc_tm);
	M0_ASSERT(rc == 0);
	rc = m0_net_desc_copy(&nb->nb_desc, &nbd);
	M0_ASSERT(rc == 0);

	/* send descriptor in message to server */
	nb = ping_buf_get(ctx);
	M0_ASSERT(nb != NULL);
	rc = encode_desc(nb, false, ctx->pc_seg_size * ctx->pc_segments, &nbd);
	m0_net_desc_free(&nbd);
	nb->nb_qtype = M0_NET_QT_MSG_SEND;
	nb->nb_ep = server_ep;
	M0_ASSERT(rc == 0);
	nb->nb_timeout = M0_TIME_NEVER;
	rc = m0_net_buffer_add(nb, &ctx->pc_tm);
	M0_ASSERT(rc == 0);

	/* wait for receive to complete */
	m0_mutex_lock(&ctx->pc_mutex);
	while (1) {
		while (!m0_list_is_empty(&ctx->pc_work_queue)) {
			link = m0_list_first(&ctx->pc_work_queue);
			wi = m0_list_entry(link, struct ping_work_item,
					   pwi_link);
			m0_list_del(&wi->pwi_link);
			if (wi->pwi_type == M0_NET_QT_PASSIVE_BULK_RECV)
				recv_done++;
			else if (wi->pwi_type == M0_NET_QT_MSG_SEND &&
				 wi->pwi_nb != NULL) {
				m0_time_t delay;
				/* send error, retry a few times */
				if (retries == 0) {
					ctx->pc_ops->pf("%s: send failed, "
							"no more retries\n",
							ctx->pc_ident);
					m0_net_desc_free(&nb->nb_desc);
					m0_mutex_unlock(&ctx->pc_mutex);
					ping_buf_put(ctx, nb);
					m0_atomic64_inc(&ctx->pc_errors);
					return -ETIMEDOUT;
				}
				delay = m0_time(SEND_RETRIES + 1 - retries, 0);
				--retries;
				m0_nanosleep(delay, NULL);
				m0_atomic64_inc(&ctx->pc_retries);
				set_msg_timeout(ctx, nb);
				rc = m0_net_buffer_add(nb, &ctx->pc_tm);
				M0_ASSERT(rc == 0);
			} else if (wi->pwi_type == M0_NET_QT_MSG_SEND) {
				recv_done++;
			}
			m0_free(wi);
		}
		if (recv_done == 2)
			break;
		m0_cond_wait(&ctx->pc_cond);
	}

	m0_mutex_unlock(&ctx->pc_mutex);
	return rc;
}

static int nlx_ping_client_passive_send(struct nlx_ping_ctx *ctx,
					struct m0_net_end_point *server_ep,
					const char *data)
{
	int rc;
	struct m0_net_buffer *nb;
	struct m0_net_buf_desc nbd;
	struct m0_list_link *link;
	struct ping_work_item *wi;
	int send_done = 0;
	int retries = SEND_RETRIES;

	if (data == NULL)
		data = "passive ping";
	PING_OUT(ctx, 1, "%s: starting passive send sequence\n", ctx->pc_ident);
	/* queue our passive receive buffer */
	nb = ping_buf_get(ctx);
	M0_ASSERT(nb != NULL);
	/* reuse encode_msg for convenience */
	rc = encode_msg(nb, data);
	nb->nb_qtype = M0_NET_QT_PASSIVE_BULK_SEND;
	nb->nb_ep = server_ep;
	set_bulk_timeout(ctx, nb);
	rc = m0_net_buffer_add(nb, &ctx->pc_tm);
	M0_ASSERT(rc == 0);
	rc = m0_net_desc_copy(&nb->nb_desc, &nbd);
	M0_ASSERT(rc == 0);

	/* send descriptor in message to server */
	nb = ping_buf_get(ctx);
	M0_ASSERT(nb != NULL);
	rc = encode_desc(nb, true, ctx->pc_seg_size * ctx->pc_segments, &nbd);
	m0_net_desc_free(&nbd);
	nb->nb_qtype = M0_NET_QT_MSG_SEND;
	nb->nb_ep = server_ep;
	M0_ASSERT(rc == 0);
	nb->nb_timeout = M0_TIME_NEVER;
	rc = m0_net_buffer_add(nb, &ctx->pc_tm);
	M0_ASSERT(rc == 0);

	/* wait for send to complete */
	m0_mutex_lock(&ctx->pc_mutex);
	while (1) {
		while (!m0_list_is_empty(&ctx->pc_work_queue)) {
			link = m0_list_first(&ctx->pc_work_queue);
			wi = m0_list_entry(link, struct ping_work_item,
					   pwi_link);
			m0_list_del(&wi->pwi_link);
			if (wi->pwi_type == M0_NET_QT_PASSIVE_BULK_SEND)
				send_done++;
			else if (wi->pwi_type == M0_NET_QT_MSG_SEND &&
				 wi->pwi_nb != NULL) {
				m0_time_t delay;
				/* send error, retry a few times */
				if (retries == 0) {
					ctx->pc_ops->pf("%s: send failed, "
							"no more retries\n",
							ctx->pc_ident);
					m0_net_desc_free(&nb->nb_desc);
					m0_mutex_unlock(&ctx->pc_mutex);
					ping_buf_put(ctx, nb);
					m0_atomic64_inc(&ctx->pc_errors);
					return -ETIMEDOUT;
				}
				delay = m0_time(SEND_RETRIES + 1 - retries, 0);
				--retries;
				m0_nanosleep(delay, NULL);
				m0_atomic64_inc(&ctx->pc_retries);
				set_msg_timeout(ctx, nb);
				rc = m0_net_buffer_add(nb, &ctx->pc_tm);
				M0_ASSERT(rc == 0);
			} else if (wi->pwi_type == M0_NET_QT_MSG_SEND) {
				send_done++;
			}
			m0_free(wi);
		}
		if (send_done == 2)
			break;
		m0_cond_wait(&ctx->pc_cond);
	}

	m0_mutex_unlock(&ctx->pc_mutex);
	return rc;
}

static int nlx_ping_client_init(struct nlx_ping_ctx *ctx,
			 struct m0_net_end_point **server_ep)
{
	int rc;
	char addr[M0_NET_LNET_XEP_ADDR_LEN];
	const char *fmt = "Client %s";
	char *ident;

	M0_ALLOC_ARR(ident, ARRAY_SIZE(addr) + strlen(fmt) + 1);
	if (ident == NULL)
		return -ENOMEM;
	sprintf(ident, fmt, "starting"); /* temporary */
	ctx->pc_ident = ident;

	ctx->pc_tm.ntm_callbacks = &ctm_cb;
	ctx->pc_buf_callbacks = &cbuf_cb;
	rc = ping_init(ctx);
	if (rc != 0) {
		m0_free0((char **)&ctx->pc_ident);
		return rc;
	}
	M0_ASSERT(ctx->pc_network != NULL);
	M0_ASSERT(ctx->pc_rnetwork != NULL);

	/* need end point for the server */
	snprintf(addr, ARRAY_SIZE(addr), "%s:%u:%u:%u", ctx->pc_rnetwork,
		 ctx->pc_rpid, ctx->pc_rportal, ctx->pc_rtmid);
	rc = m0_net_end_point_create(server_ep, &ctx->pc_tm, addr);
	if (rc != 0) {
		ping_fini(ctx);
		m0_free0((char **)&ctx->pc_ident);
		return rc;
	}

	/* clients can have dynamically assigned TMIDs so use the EP addr
	   in the ident.
	 */
	sprintf(ident, fmt, ctx->pc_tm.ntm_ep->nep_addr);
	return 0;
}

static void nlx_ping_client_fini(struct nlx_ping_ctx *ctx,
			 struct m0_net_end_point *server_ep)
{
	m0_net_end_point_put(server_ep);
	ping_fini(ctx);
	m0_free0((char **)&ctx->pc_ident);
}

void nlx_ping_client(struct nlx_ping_client_params *params)
{
	int			 i;
	int			 rc;
	struct m0_net_end_point *server_ep;
	char			*bp = NULL;
	char                    *send_msg = NULL;
	struct nlx_ping_ctx     *cctx;

	M0_ALLOC_PTR(cctx);
	if (cctx == NULL)
		goto free_ctx;

	cctx->pc_xprt         = &m0_net_lnet_xprt;
	cctx->pc_ops          = params->ops;
	cctx->pc_nr_bufs      = params->nr_bufs;
	cctx->pc_bulk_size    = params->bulk_size;
	cctx->pc_tm.ntm_state = M0_NET_TM_UNDEFINED;
	cctx->pc_bulk_timeout = params->bulk_timeout;
	cctx->pc_msg_timeout  = params->msg_timeout;
	cctx->pc_network      = params->client_network;
	cctx->pc_pid          = params->client_pid;
	cctx->pc_portal       = params->client_portal;
	cctx->pc_tmid         = params->client_tmid;
	cctx->pc_rnetwork     = params->server_network;
	cctx->pc_rpid         = params->server_pid;
	cctx->pc_rportal      = params->server_portal;
	cctx->pc_rtmid        = params->server_tmid;
	cctx->pc_dom_debug    = params->debug;
	cctx->pc_tm_debug     = params->debug;
	cctx->pc_verbose      = params->verbose;
	cctx->pc_sync_events  = false;

	m0_mutex_init(&cctx->pc_mutex);
	m0_cond_init(&cctx->pc_cond, &cctx->pc_mutex);
	rc = nlx_ping_client_init(cctx, &server_ep);
	if (rc != 0)
		goto fail;

	if (params->client_id == 1)
		ping_print_interfaces(cctx);

	if (params->bulk_size != 0) {
		bp = m0_alloc(params->bulk_size);
		M0_ASSERT(bp != NULL);
		for (i = 0; i < params->bulk_size - PING_MSG_OVERHEAD; ++i)
			bp[i] = "abcdefghi"[i % 9];
	}

	if (params->send_msg_size > 0) {
		send_msg = m0_alloc(params->send_msg_size);
		M0_ASSERT(send_msg);
		for (i = 0; i < params->send_msg_size - 1; ++i)
			send_msg[i] = "ABCDEFGHI"[i % 9];
	}

	for (i = 1; i <= params->loops; ++i) {
		PING_OUT(cctx, 1, "%s: Loop %d\n", cctx->pc_ident, i);
		rc = nlx_ping_client_msg_send_recv(cctx, server_ep, send_msg);
		if (rc != 0)
			break;
		rc = nlx_ping_client_passive_recv(cctx, server_ep);
		if (rc != 0)
			break;
		rc = nlx_ping_client_passive_send(cctx, server_ep, bp);
		if (rc != 0)
			break;
	}

	cctx->pc_ops->pqs(cctx, false);
	nlx_ping_client_fini(cctx, server_ep);
	m0_free(bp);
	m0_free(send_msg);
fail:
	m0_cond_fini(&cctx->pc_cond);
	m0_mutex_fini(&cctx->pc_mutex);
free_ctx:
	m0_free(cctx);
}

void nlx_ping_init()
{
	m0_mutex_init(&qstats_mutex);
}

void nlx_ping_fini()
{
	m0_mutex_fini(&qstats_mutex);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
