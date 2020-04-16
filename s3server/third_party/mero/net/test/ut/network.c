/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 05/19/2012
 */

#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/semaphore.h"	/* m0_semaphore */
#include "lib/memory.h"		/* m0_alloc */
#include "net/lnet/lnet.h"	/* m0_net_lnet_ifaces_get */

#include "net/test/network.h"

enum {
	NET_TEST_PING_BUF_SIZE = 4096,
	NET_TEST_PING_BUF_STEP = 511,	/** @see m0_net_test_network_ut_ping */
	NET_TEST_BULK_BUF_SIZE = 1024 * 1024,
	NET_TEST_BUF_DESC_NR   = 10,
};

static m0_bcount_t bv_copy(struct m0_bufvec *dst,
			   struct m0_bufvec *src,
			   m0_bcount_t len)
{
	struct m0_bufvec_cursor bcsrc;
	struct m0_bufvec_cursor bcdst;

	m0_bufvec_cursor_init(&bcsrc, src);
	m0_bufvec_cursor_init(&bcdst, dst);
	return m0_bufvec_cursor_copy(&bcdst, &bcsrc, len);
}

/** @todo too expensive, use m0_bufvec_cursor_step() + memcmp() */
static bool net_buf_data_eq(enum m0_net_test_network_buf_type buf_type,
			    struct m0_net_test_network_ctx *ctx1,
			    uint32_t buf_index1,
			    struct m0_net_test_network_ctx *ctx2,
			    uint32_t buf_index2)
{
	void		     *b1_data;
	void		     *b2_data;
	m0_bcount_t	      length;
	struct m0_net_buffer *b1;
	struct m0_net_buffer *b2;
	struct m0_bufvec      bv1 = M0_BUFVEC_INIT_BUF(&b1_data, &length);
	struct m0_bufvec      bv2 = M0_BUFVEC_INIT_BUF(&b2_data, &length);
	m0_bcount_t	      rc_bcount;
	bool		      rc;

	b1 = m0_net_test_network_buf(ctx1, buf_type, buf_index1);
	b2 = m0_net_test_network_buf(ctx2, buf_type, buf_index2);

	if (b1->nb_length != b2->nb_length)
		return false;

	length = b1->nb_length;

	b1_data = m0_alloc(length);
	b2_data = m0_alloc(length);

	rc_bcount = bv_copy(&bv1, &b1->nb_buffer, length);
	M0_ASSERT(rc_bcount == length);
	rc_bcount = bv_copy(&bv2, &b2->nb_buffer, length);
	M0_ASSERT(rc_bcount == length);

	rc = memcmp(b1_data, b2_data, length) == 0;

	m0_free(b1_data);
	m0_free(b2_data);

	return rc;
}

static void ping_tm_event_cb(const struct m0_net_tm_event *ev)
{
}

static struct m0_semaphore recv_sem;
static struct m0_semaphore send_sem;

static void ping_cb_msg_recv(struct m0_net_test_network_ctx *ctx,
			     const uint32_t buf_index,
			     enum m0_net_queue_type q,
			     const struct m0_net_buffer_event *ev)
{
	m0_semaphore_up(&recv_sem);
}

static void ping_cb_msg_send(struct m0_net_test_network_ctx *ctx,
			     const uint32_t buf_index,
			     enum m0_net_queue_type q,
			     const struct m0_net_buffer_event *ev)
{
	m0_semaphore_up(&send_sem);
}

static void ping_cb_impossible(struct m0_net_test_network_ctx *ctx,
			       const uint32_t buf_index,
			       enum m0_net_queue_type q,
			       const struct m0_net_buffer_event *ev)
{

	M0_IMPOSSIBLE("impossible bulk buffer callback in ping test");
}

static const struct m0_net_tm_callbacks ping_tm_cb = {
	.ntc_event_cb = ping_tm_event_cb
};

static struct m0_net_test_network_buffer_callbacks ping_buf_cb = {
	.ntnbc_cb = {
		[M0_NET_QT_MSG_RECV]		= ping_cb_msg_recv,
		[M0_NET_QT_MSG_SEND]		= ping_cb_msg_send,
		[M0_NET_QT_PASSIVE_BULK_RECV]	= ping_cb_impossible,
		[M0_NET_QT_PASSIVE_BULK_SEND]	= ping_cb_impossible,
		[M0_NET_QT_ACTIVE_BULK_RECV]	= ping_cb_impossible,
		[M0_NET_QT_ACTIVE_BULK_SEND]	= ping_cb_impossible,
	}
};

void m0_net_test_network_ut_ping(void)
{
	static struct m0_net_test_network_cfg cfg;
	static struct m0_net_test_network_ctx send;
	static struct m0_net_test_network_ctx recv;
	int				      rc;
	bool				      rc_bool;
	m0_bcount_t			      buf_size;

	buf_size = NET_TEST_PING_BUF_SIZE;
	M0_SET0(&cfg);
	cfg.ntncfg_tm_cb	 = ping_tm_cb;
	cfg.ntncfg_buf_cb	 = ping_buf_cb;
	cfg.ntncfg_buf_size_ping = buf_size;
	cfg.ntncfg_buf_ping_nr	 = 1;
	cfg.ntncfg_ep_max	 = 1;
	cfg.ntncfg_timeouts	 = m0_net_test_network_timeouts_never();
	rc = m0_net_test_network_ctx_init(&send, &cfg, "0@lo:12345:42:1000");
	M0_UT_ASSERT(rc == 0);

	rc = m0_net_test_network_ctx_init(&recv, &cfg, "0@lo:12345:42:1001");
	M0_UT_ASSERT(rc == 0);

	rc = m0_net_test_network_ep_add(&send, "0@lo:12345:42:1001");
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_test_network_ep_add(&recv, "0@lo:12345:42:1000");
	M0_UT_ASSERT(rc == 0);

	m0_semaphore_init(&recv_sem, 0);
	m0_semaphore_init(&send_sem, 0);

	while (buf_size > 0) {
		/* test buffer resize. @see m0_net_test_network_buf_resize */
		m0_net_test_network_buf_resize(&send, M0_NET_TEST_BUF_PING, 0,
					       buf_size);
		m0_net_test_network_buf_resize(&recv, M0_NET_TEST_BUF_PING, 0,
					       buf_size);

		m0_net_test_network_buf_fill(&send, M0_NET_TEST_BUF_PING, 0, 1);
		m0_net_test_network_buf_fill(&recv, M0_NET_TEST_BUF_PING, 0, 2);
		rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_PING,
					  &send, 0, &recv, 0);
		M0_ASSERT(!rc_bool);

		rc = m0_net_test_network_msg_recv(&recv, 0);
		M0_UT_ASSERT(rc == 0);
		rc = m0_net_test_network_msg_send(&send, 0, 0);
		M0_UT_ASSERT(rc == 0);

		/** @todo timeddown */
		m0_semaphore_down(&recv_sem);
		m0_semaphore_down(&send_sem);

		rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_PING,
					  &send, 0, &recv, 0);
		M0_ASSERT(rc_bool);

		buf_size = buf_size < NET_TEST_PING_BUF_STEP ?
			   0 : buf_size - NET_TEST_PING_BUF_STEP;
	}

	m0_semaphore_fini(&recv_sem);
	m0_semaphore_fini(&send_sem);

	m0_net_test_network_ctx_fini(&send);
	m0_net_test_network_ctx_fini(&recv);
}

static struct m0_semaphore bulk_cb_sem[M0_NET_QT_NR];
static bool bulk_offset_mismatch;

static void bulk_cb(struct m0_net_test_network_ctx *ctx,
		    const uint32_t buf_index,
		    enum m0_net_queue_type q,
		    const struct m0_net_buffer_event *ev)
{
	M0_PRE(q < M0_NET_QT_NR);

	/*
	   m0_net_buffer_event.nbe_offset can't have non-zero value
	   in this test.
	 */
	if (ev->nbe_offset != 0)
		bulk_offset_mismatch = true;

	m0_semaphore_up(&bulk_cb_sem[q]);
}

static const struct m0_net_tm_callbacks bulk_tm_cb = {
	.ntc_event_cb = ping_tm_event_cb
};

static struct m0_net_test_network_buffer_callbacks bulk_buf_cb = {
	.ntnbc_cb = {
		[M0_NET_QT_MSG_RECV]		= bulk_cb,
		[M0_NET_QT_MSG_SEND]		= bulk_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV]	= bulk_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND]	= bulk_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]	= bulk_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]	= bulk_cb,
	}
};

void m0_net_test_network_ut_bulk(void)
{
	static struct m0_net_test_network_cfg cfg;
	static struct m0_net_test_network_ctx client;
	static struct m0_net_test_network_ctx server;
	m0_bcount_t			      offset;
	m0_bcount_t			      bcount;
	int				      rc;
	size_t				      rc_size;
	int				      i;
	bool				      rc_bool;

	M0_SET0(&cfg);
	cfg.ntncfg_tm_cb	 = bulk_tm_cb;
	cfg.ntncfg_buf_cb	 = bulk_buf_cb;
	cfg.ntncfg_buf_size_ping = NET_TEST_PING_BUF_SIZE;
	cfg.ntncfg_buf_ping_nr	 = 1;
	cfg.ntncfg_buf_size_bulk = NET_TEST_PING_BUF_SIZE;
	cfg.ntncfg_buf_bulk_nr	 = 2;
	cfg.ntncfg_ep_max	 = 1;
	cfg.ntncfg_timeouts	 = m0_net_test_network_timeouts_never();
	rc = m0_net_test_network_ctx_init(&client, &cfg, "0@lo:12345:42:1000");
	M0_UT_ASSERT(rc == 0);
	cfg.ntncfg_buf_bulk_nr = 1;
	rc = m0_net_test_network_ctx_init(&server, &cfg, "0@lo:12345:42:1001");
	M0_UT_ASSERT(rc == 0);

	rc = m0_net_test_network_ep_add(&client, "0@lo:12345:42:1001");
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_test_network_ep_add(&server, "0@lo:12345:42:1000");
	M0_UT_ASSERT(rc == 0);

	/* start of bulk send/recv */
	bulk_offset_mismatch = false;
	/* fill bulk buffers with different values */
	m0_net_test_network_buf_fill(&client, M0_NET_TEST_BUF_BULK, 0, 10);
	m0_net_test_network_buf_fill(&client, M0_NET_TEST_BUF_BULK, 1, 20);
	m0_net_test_network_buf_fill(&server, M0_NET_TEST_BUF_BULK, 0, 30);
	rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_BULK, &client, 0, &server, 0);
	M0_ASSERT(!rc_bool);
	rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_BULK, &client, 1, &server, 0);
	M0_ASSERT(!rc_bool);
	rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_BULK, &client, 0, &client, 1);
	M0_ASSERT(!rc_bool);
	/* init callback semaphores */
	for (i = 0; i < M0_NET_QT_NR; ++i)
		m0_semaphore_init(&bulk_cb_sem[i], 0);
	/* server: receive ping buf */
	rc = m0_net_test_network_msg_recv(&server, 0);
	M0_UT_ASSERT(rc == 0);
	/* client: add passive sender->active receiver bulk buffer to q */
	rc = m0_net_test_network_bulk_enqueue(&client, 0, 0,
			M0_NET_QT_PASSIVE_BULK_SEND);
	M0_UT_ASSERT(rc == 0);
	/* client: add passive receiver<-active sender bulk buffer to q */
	rc = m0_net_test_network_bulk_enqueue(&client, 1, 0,
			M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(rc == 0);
	/* client: add buffer descriptors to ping buf */
	offset = 0;
	bcount = m0_net_test_network_bd_serialize(M0_NET_TEST_SERIALIZE,
						  &client, 0, 0, offset);
	M0_UT_ASSERT(bcount != 0);
	offset += bcount;
	bcount = m0_net_test_network_bd_serialize(M0_NET_TEST_SERIALIZE,
						  &client, 1, 0, offset);
	M0_UT_ASSERT(bcount != 0);
	rc_size = m0_net_test_network_bd_nr(&client, 0);
	M0_UT_ASSERT(rc_size == 2);
	/* client: send ping buf */
	rc = m0_net_test_network_msg_send(&client, 0, 0);
	M0_UT_ASSERT(rc == 0);
	/* server: wait for buf from client */
	m0_semaphore_down(&bulk_cb_sem[M0_NET_QT_MSG_RECV]);
	/* server: check ping buffer size and data */
	rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_PING, &client, 0, &server, 0);
	M0_ASSERT(rc_bool);
	/* server: extract buf descriptor for active recv */
	rc_size = m0_net_test_network_bd_nr(&server, 0);
	M0_UT_ASSERT(rc_size == 2);
	offset = 0;
	bcount = m0_net_test_network_bd_serialize(M0_NET_TEST_DESERIALIZE,
						  &server, 0, 0, offset);
	M0_UT_ASSERT(bcount != 0);
	offset += bcount;
	/* server: do active recv */
	rc = m0_net_test_network_bulk_enqueue(&server, 0, 0,
					      M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(rc == 0);
	/* server: wait for active recv callback */
	m0_semaphore_down(&bulk_cb_sem[M0_NET_QT_ACTIVE_BULK_RECV]);
	/* server: extract buf descriptor for active send */
	bcount = m0_net_test_network_bd_serialize(M0_NET_TEST_DESERIALIZE,
						  &server, 0, 0, offset);
	M0_UT_ASSERT(bcount != 0);
	/* server: do active send */
	rc = m0_net_test_network_bulk_enqueue(&server, 0, 0,
					      M0_NET_QT_ACTIVE_BULK_SEND);
	M0_UT_ASSERT(rc == 0);
	/* server: wait for active send callbacks */
	m0_semaphore_down(&bulk_cb_sem[M0_NET_QT_ACTIVE_BULK_SEND]);
	/*
	   client: now all data are actually sent, so check for passive
	   send/recv callbacks called
	 */
	/* send message */
	m0_semaphore_down(&bulk_cb_sem[M0_NET_QT_MSG_SEND]);
	/* passive bulk send */
	m0_semaphore_down(&bulk_cb_sem[M0_NET_QT_PASSIVE_BULK_SEND]);
	/* passive bulk recv */
	m0_semaphore_down(&bulk_cb_sem[M0_NET_QT_PASSIVE_BULK_RECV]);
	/* fini callback semaphores */
	for (i = 0; i < M0_NET_QT_NR; ++i)
		m0_semaphore_fini(&bulk_cb_sem[i]);
	/* check for equal bulk buffers on client and server */
	rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_BULK, &client, 0, &server, 0);
	M0_ASSERT(rc_bool);
	rc_bool = net_buf_data_eq(M0_NET_TEST_BUF_BULK, &client, 0, &client, 1);
	M0_ASSERT(rc_bool);
	/* end of bulk send/recv */
	M0_ASSERT(!bulk_offset_mismatch);

	m0_net_test_network_ctx_fini(&client);
	m0_net_test_network_ctx_fini(&server);
}

static void tm_event_cb_empty(const struct m0_net_tm_event *ev)
{
}

static void cb_empty(struct m0_net_test_network_ctx *ctx,
		     const uint32_t buf_index,
		     enum m0_net_queue_type q,
		     const struct m0_net_buffer_event *ev)
{
}

static const struct m0_net_tm_callbacks tm_cb_empty = {
	.ntc_event_cb = tm_event_cb_empty
};

static struct m0_net_test_network_buffer_callbacks buf_cb_empty = {
	.ntnbc_cb = {
		[M0_NET_QT_MSG_RECV]		= cb_empty,
		[M0_NET_QT_MSG_SEND]		= cb_empty,
		[M0_NET_QT_PASSIVE_BULK_RECV]	= cb_empty,
		[M0_NET_QT_PASSIVE_BULK_SEND]	= cb_empty,
		[M0_NET_QT_ACTIVE_BULK_RECV]	= cb_empty,
		[M0_NET_QT_ACTIVE_BULK_SEND]	= cb_empty,
	}
};

/**
   Compare bulk network buffer descriptors.
 */
static bool buf_desc_eq(struct m0_net_test_network_ctx *ctx1,
			uint32_t buf_index1,
			struct m0_net_test_network_ctx *ctx2,
			uint32_t buf_index2)
{
	struct m0_net_buffer   *b1;
	struct m0_net_buffer   *b2;
	struct m0_net_buf_desc *d1;
	struct m0_net_buf_desc *d2;

	b1 = m0_net_test_network_buf(ctx1, M0_NET_TEST_BUF_BULK, buf_index1);
	b2 = m0_net_test_network_buf(ctx2, M0_NET_TEST_BUF_BULK, buf_index2);
	d1 = &b1->nb_desc;
	d2 = &b2->nb_desc;

	return d1->nbd_len == d2->nbd_len &&
		memcmp(d1->nbd_data, d2->nbd_data, d1->nbd_len) == 0;
}

static void multiple_buf_desc_encode_decode(struct m0_net_test_network_ctx *ctx,
					    int count)
{
	m0_bcount_t bcount;
	m0_bcount_t offset;
	size_t	    rc_size;
	int	    i;
	bool	    rc_bool;

	offset = 0;
	for (i = 0; i < count; ++i) {
		/* encode */
		bcount = m0_net_test_network_bd_serialize(M0_NET_TEST_SERIALIZE,
							 ctx, i % 2, 0, offset);
		M0_UT_ASSERT(bcount != 0);
		offset += bcount;
		/* check number of buf descriptors in the ping buffer */
		rc_size = m0_net_test_network_bd_nr(ctx, 0);
		M0_UT_ASSERT(rc_size == i + 1);
	}
	/* prepare to decode */
	offset = 0;
	for (i = 0; i < count; ++i) {
		/* decode */
		bcount = m0_net_test_network_bd_serialize(
				M0_NET_TEST_DESERIALIZE,
				ctx, 2 + i % 2, 0, offset);
		M0_UT_ASSERT(bcount != 0);
		offset += bcount;
		/* compare m0_net_buf_desc's */
		rc_bool = buf_desc_eq(ctx, i % 2, ctx, 2 + i % 2);
		M0_UT_ASSERT(rc_bool);
	}
}

void m0_net_test_network_ut_buf_desc(void)
{
	static struct m0_net_test_network_cfg cfg;
	static struct m0_net_test_network_ctx ctx;
	static struct m0_clink		      tmwait;
	int				      i;
	int				      rc;

	M0_SET0(&cfg);
	cfg.ntncfg_tm_cb	 = tm_cb_empty;
	cfg.ntncfg_buf_cb	 = buf_cb_empty;
	cfg.ntncfg_buf_size_ping = NET_TEST_PING_BUF_SIZE;
	cfg.ntncfg_buf_ping_nr	 = 2;
	cfg.ntncfg_buf_size_bulk = NET_TEST_PING_BUF_SIZE;
	cfg.ntncfg_buf_bulk_nr	 = 4;
	cfg.ntncfg_ep_max	 = 1;
	cfg.ntncfg_timeouts	 = m0_net_test_network_timeouts_never();
	rc = m0_net_test_network_ctx_init(&ctx, &cfg, "0@lo:12345:42:*");
	M0_UT_ASSERT(rc == 0);

	/* add some ep - tranfer machine ep */
	rc = m0_net_test_network_ep_add(&ctx, ctx.ntc_tm->ntm_ep->nep_addr);
	M0_UT_ASSERT(rc == 0);

	/* obtain some m0_net_buf_desc */
	rc = m0_net_test_network_bulk_enqueue(&ctx, 0, 0,
					      M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_test_network_bulk_enqueue(&ctx, 1, 0,
					      M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(rc == 0);

	/* run multiple tests */
	for (i = 0; i < NET_TEST_BUF_DESC_NR; ++i)
		multiple_buf_desc_encode_decode(&ctx, i);

	/* remove bulk buffer from queue */
	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&ctx.ntc_tm->ntm_chan, &tmwait);
	m0_net_test_network_buffer_dequeue(&ctx, M0_NET_TEST_BUF_BULK, 0);
	m0_chan_wait(&tmwait);
	m0_net_test_network_buffer_dequeue(&ctx, M0_NET_TEST_BUF_BULK, 1);
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	m0_clink_fini(&tmwait);

	m0_net_test_network_ctx_fini(&ctx);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
