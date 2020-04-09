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
 * Original creation date: 09/03/2012
 */

#include "lib/memory.h"			/* M0_ALLOC_PTR */
#include "lib/misc.h"			/* M0_SET0 */
#include "lib/time.h"			/* m0_time_t */
#include "lib/errno.h"			/* ENOMEM */
#include "lib/thread.h"			/* M0_THREAD_INIT */
#include "lib/tlist.h"			/* m0_tlist */

#include "mero/magic.h"		/* M0_NET_TEST_BS_LINK_MAGIC */

#include "net/test/network.h"		/* m0_net_test_network_ctx */
#include "net/test/node.h"		/* m0_net_test_node_ctx */
#include "net/test/node_helper.h"	/* m0_net_test_node_ctx */

#include "net/test/node_ping.h"

#define NET_TEST_MODULE_NAME node_ping
#include "net/test/debug.h"		/* LOGD */


/**
   @defgroup NetTestPingNodeInternals Ping Node
   @ingroup NetTestInternals

   Ping node service m0_net_test_service_ops:
   - ntso_init
     - allocate ping node context;
     - init buffer queue semaphore
   - ntso_fini
     - finalize network context if it was initialized;
     - fini buffer queue semaphore
     - free ping node context.
   - ntso_step
     - check 'stop test' conditions
     - recovery after failure (send, recv, buffer callback etc.)
   - ntso_cmd_handler
     - M0_NET_TEST_CMD_INIT
       - initialize network context.
       - reset statistics
       - send M0_NET_TEST_CMD_INIT_DONE reply
     - M0_NET_TEST_CMD_START
       - add all buffers to the network queue;
       - send M0_NET_TEST_CMD_START_DONE reply
     - M0_NET_TEST_CMD_STOP
       - remove all buffers from the network queue
       - wait for all buffer callbacks
       - send M0_NET_TEST_CMD_STOP_DONE reply
     - M0_NET_TEST_CMD_STATUS
       - fill and send M0_NET_TEST_CMD_STATUS_DATA reply

   @todo nb_max_receive_msgs > 1 is not supported.

   @{
 */

enum {
	/** Timeout checking interval, ms */
	TO_CHECK_INTERVAL = 10,
};

/** Buffer state */
struct buf_state {
	/** Magic for messages timeout list */
	uint64_t		   bs_link_magic;
	/** Queue type */
	enum m0_net_queue_type	   bs_q;
	/**
	 * Result of last buffer enqueue operation.
	 * Have non-zero value at the start of test.
	 */
	int			   bs_errno;
	/**
	 * Copy of network buffer event.
	 * Makes no sense if bs_errno != 0.
	 * m0_net_buffer_event.nbe_ep will be set in buffer callback
	 * using m0_net_end_point_get().
	 */
	struct m0_net_buffer_event bs_ev;
	/** Callback executing time */
	m0_time_t		   bs_time;
	/** Sequence number for test client messages */
	uint64_t		   bs_seq;
	/** Deadline for messages on the test client */
	m0_time_t		   bs_deadline;
	/**
	 * Number of executed callbacks for the test message
	 * (sent and received) on the test client.
	 */
	uint64_t		   bs_cb_nr;
	/**
	 * Index of corresponding recv buffer for send buffer
	 * and send buffer for recv buffer;
	 * (test client only).
	 */
	size_t			   bs_index_pair;
	/** Link for messages timeout list */
	struct m0_tlink		   bs_link;
};

M0_TL_DESCR_DEFINE(buf_state, "buf_state", static, struct buf_state, bs_link,
		   bs_link_magic, M0_NET_TEST_BS_LINK_MAGIC,
		   M0_NET_TEST_BS_HEAD_MAGIC);
M0_TL_DEFINE(buf_state, static, struct buf_state);

/**
 * Test client context.
 * Buffers assignment:
 * - first half of buffers is for sending and second half is for receiving;
 * - recv buffers can receive message from any server;
 * - send buffers will be used for sending to assigned servers. There is
 *   M = m0_net_test_cmd_init.ntci_concurrency buffers for every server
 *   (first M buffers is for server with index 0, then M buffers for
 *   server with index 1 etc.).
 */
struct node_ping_client_ctx {
	/**
	 * Number of test messages sent to test server and received back
	 * (including failed) for the test client.
	 */
	size_t	     npcc_msg_rt;
	/**
	 * Maximum number of sent to test server and received back
	 * test messages (for the test client). "rt" == "round-trip".
	 */
	size_t	     npcc_msg_rt_max;
	/** Concurrency. @see m0_net_test_cmd_init.ntci_concurrency */
	size_t	     npcc_concurrency;
	/** Messages timeout list */
	struct m0_tl npcc_to;
};

/** Test server context */
struct node_ping_server_ctx {
	int npsc_unused;
};

/** Ping node context */
struct node_ping_ctx {
	/** Node helper */
	struct m0_net_test_nh		    npc_nh;
	/** Network context for testing */
	struct m0_net_test_network_ctx	    npc_net;
	/** Test service. Used when changing service state. */
	struct m0_net_test_service	   *npc_svc;
	/**
	   Number of network buffers to send/receive test messages.
	   @see m0_net_test_cmd_init.ntci_concurrency
	 */
	size_t				    npc_buf_nr;
	/** Size of network buffers. */
	m0_bcount_t			    npc_buf_size;
	/** Timeout for test message sending */
	m0_time_t			    npc_buf_send_timeout;
	/** Test was initialized (successful node_ping_cmd_start()) */
	bool				    npc_test_initialized;
	/**
	 * Buffer enqueue semaphore.
	 * - initial value - number of buffers;
	 * - up() in network buffer callback;
	 * - (down() * number_of_buffers) in node_ping_cmd_stop();
	 * - down() after successful addition to network buffer queue;
	 * - trydown() before addition to queue. if failed -
	 *   then don't add to queue;
	 * - up() after unsuccessful addition to queue.
	 *
	 * @note Problem with semaphore max value can be here.
	 * @see @ref net-test-sem-max-value "Problem Description"
	 */
	struct m0_semaphore		    npc_buf_q_sem;
	/** Ringbuf of buffers that are not in network buffers queue */
	struct m0_net_test_ringbuf	    npc_buf_rb;
	/** up() after adding buffer to queue, down() before receiving */
	struct m0_semaphore		    npc_buf_rb_sem;
	/** Previous up() was from node_ping_cmd_stop() */
	bool				    npc_buf_rb_done;
	/** Array of buffer states */
	struct buf_state		   *npc_buf_state;
	/** Worker thread */
	struct m0_thread		    npc_thread;
	union {
		struct node_ping_client_ctx npc__client;
		struct node_ping_server_ctx npc__server;
	};
	/**
	 * Client context. Set to NULL on the test server,
	 * set to &node_ping_ctx.npc__client on the test client.
	 */
	struct node_ping_client_ctx	   *npc_client;
	/**
	 * Server context. See npc_client.
	 */
	struct node_ping_server_ctx	   *npc_server;
};

/** Wrapper for m0_net_test_nh_sd_update() with smaller name */
static void sd_update(struct node_ping_ctx *ctx,
		      enum m0_net_test_nh_msg_type type,
		      enum m0_net_test_nh_msg_status status,
		      enum m0_net_test_nh_msg_direction direction)
{
	m0_net_test_nh_sd_update(&ctx->npc_nh, type, status, direction);
}

static void node_ping_tm_event_cb(const struct m0_net_tm_event *ev)
{
	/* nothing for now */
}

static const struct m0_net_tm_callbacks node_ping_tm_cb = {
	.ntc_event_cb = node_ping_tm_event_cb
};

static struct node_ping_ctx *
node_ping_ctx_from_net_ctx(struct m0_net_test_network_ctx *net_ctx)
{
	return container_of(net_ctx, struct node_ping_ctx, npc_net);
}

static m0_time_t node_ping_timestamp_put(struct m0_net_test_network_ctx *net_ctx,
					 uint32_t buf_index,
					 uint64_t seq)
{
	struct m0_net_test_timestamp ts;
	struct m0_net_buffer	    *buf;

	buf = m0_net_test_network_buf(net_ctx, M0_NET_TEST_BUF_PING, buf_index);
	m0_net_test_timestamp_init(&ts, seq);
	/* buffer size should be enough to hold timestamp */
	m0_net_test_timestamp_serialize(M0_NET_TEST_SERIALIZE, &ts,
					&buf->nb_buffer, 0);
	return ts.ntt_time;
}

static bool node_ping_timestamp_get(struct m0_net_test_network_ctx *net_ctx,
				    uint32_t buf_index,
				    struct m0_net_test_timestamp *ts)
{
	struct m0_net_buffer *buf;
	m0_bcount_t	      len;

	M0_PRE(net_ctx != NULL);
	M0_PRE(ts != NULL);

	buf = m0_net_test_network_buf(net_ctx, M0_NET_TEST_BUF_PING, buf_index);
	len = m0_net_test_timestamp_serialize(M0_NET_TEST_DESERIALIZE, ts,
					      &buf->nb_buffer, 0);
	return len != 0;
}

static void node_ping_to_add(struct node_ping_ctx *ctx,
			     size_t buf_index)
{
	LOGD("buf_index = %lu", buf_index);
	buf_state_tlist_add_tail(&ctx->npc_client->npcc_to,
				 &ctx->npc_buf_state[buf_index]);
}

static void node_ping_to_del(struct node_ping_ctx *ctx,
			     size_t buf_index)
{
	LOGD("buf_index = %lu", buf_index);
	buf_state_tlist_del(&ctx->npc_buf_state[buf_index]);
}

static ssize_t node_ping_to_peek(struct node_ping_ctx *ctx)
{
	struct buf_state *bs;
	ssize_t		  buf_index;

	bs = buf_state_tlist_head(&ctx->npc_client->npcc_to);
	buf_index = bs == NULL ? -1 : bs - ctx->npc_buf_state;

	M0_POST(buf_index == -1 ||
	        (buf_index >= 0 && buf_index < ctx->npc_buf_nr));
	return buf_index;
}

static ssize_t node_ping_client_search_seq(struct node_ping_ctx *ctx,
					   size_t server_index,
					   uint64_t seq)
{
	size_t i;
	size_t buf_index;
	size_t concurrency = ctx->npc_client->npcc_concurrency;

	for (i = 0; i < concurrency; ++i) {
		buf_index = concurrency * server_index + i;
		if (ctx->npc_buf_state[buf_index].bs_seq == seq)
			return buf_index;
	}
	return -1;
}

static void node_ping_buf_enqueue(struct node_ping_ctx *ctx,
				  size_t buf_index,
				  enum m0_net_queue_type q,
				  struct m0_net_end_point *ep,
				  size_t ep_index)
{
	struct m0_net_test_network_ctx *nctx = &ctx->npc_net;
	struct buf_state	       *bs = &ctx->npc_buf_state[buf_index];
	bool				decreased;

	M0_PRE(ergo(ep != NULL, q == M0_NET_QT_MSG_SEND));

	decreased = m0_semaphore_trydown(&ctx->npc_buf_q_sem);
	if (!decreased) {
		/* worker thread is stopping */
		M0_ASSERT(ctx->npc_buf_rb_done);
		bs->bs_errno = -EWOULDBLOCK;
		return;
	}
	LOGD("q = %d, buf_index = %lu, ep_index = %lu, %s",
	     q, buf_index, ep_index, ctx->npc_net.ntc_tm->ntm_ep->nep_addr);
	if (q == M0_NET_QT_MSG_SEND) {
		LOGD(" => %s", (ep == NULL ?
			    m0_net_test_network_ep(&ctx->npc_net, ep_index) :
			    ep)->nep_addr);
	} else {
		LOGD(" <= ");
	}
	bs->bs_errno = (bs->bs_q = q) == M0_NET_QT_MSG_RECV ?
		  m0_net_test_network_msg_recv(nctx, buf_index) : ep == NULL ?
		  m0_net_test_network_msg_send(nctx, buf_index, ep_index) :
		  m0_net_test_network_msg_send_ep(nctx, buf_index, ep);
	if (bs->bs_errno != 0) {
		m0_net_test_ringbuf_push(&ctx->npc_buf_rb, buf_index);
		m0_semaphore_up(&ctx->npc_buf_q_sem);
		m0_semaphore_up(&ctx->npc_buf_rb_sem);
	}
}

static void node_ping_buf_enqueue_recv(struct node_ping_ctx *ctx,
				       size_t buf_index)
{
	node_ping_buf_enqueue(ctx, buf_index, M0_NET_QT_MSG_RECV, NULL, 0);
}

static void node_ping_client_send(struct node_ping_ctx *ctx,
				  size_t buf_index)
{
	struct node_ping_client_ctx *cctx;
	struct buf_state	    *bs;
	size_t			     ep_index;
	m0_time_t		     begin;
	bool			     transfer_next;

	M0_PRE(ctx != NULL && ctx->npc_client != NULL);
	M0_PRE(buf_index < ctx->npc_buf_nr / 2);

	bs	 = &ctx->npc_buf_state[buf_index];
	cctx	 = ctx->npc_client;
	ep_index = buf_index / cctx->npcc_concurrency;
	/* check for max number of messages */
	transfer_next = m0_net_test_nh_transfer_next(&ctx->npc_nh);
	if (!transfer_next)
		return;
	/* put timestamp and sequence number */
	bs->bs_seq   = ctx->npc_nh.ntnh_transfers_started_nr;
	bs->bs_cb_nr = 0;
	begin = node_ping_timestamp_put(&ctx->npc_net, buf_index, bs->bs_seq);
	bs->bs_deadline = m0_time_add(begin, ctx->npc_buf_send_timeout);
	/* add message to send queue */
	node_ping_buf_enqueue(ctx, buf_index, M0_NET_QT_MSG_SEND,
			      NULL, ep_index);
	if (bs->bs_errno != 0)
		sd_update(ctx, MT_MSG, MS_FAILED, MD_SEND);
}

static void node_ping_client_cb2(struct node_ping_ctx *ctx,
				 size_t buf_index)
{
	struct buf_state *bs = &ctx->npc_buf_state[buf_index];

	M0_PRE(bs->bs_cb_nr == 0 || bs->bs_cb_nr == 1);

	++bs->bs_cb_nr;
	if (bs->bs_cb_nr != 2) {
		node_ping_to_add(ctx, buf_index);
	} else {
		node_ping_to_del(ctx, buf_index);
		/* enqueue recv and send buffers */
		node_ping_buf_enqueue_recv(ctx, bs->bs_index_pair);
		node_ping_client_send(ctx, buf_index);
	}
}

static bool node_ping_client_recv_cb(struct node_ping_ctx *ctx,
				     struct buf_state *bs,
				     size_t buf_index)
{
	struct m0_net_test_timestamp  ts;
	struct buf_state	     *bs_send = NULL;
	ssize_t			      server_index;
	ssize_t			      buf_index_send;
	bool			      decoded;
	m0_time_t		      rtt;

	M0_PRE(ctx != NULL && ctx->npc_client != NULL);
	M0_PRE(buf_index >= ctx->npc_buf_nr / 2 &&
	       buf_index < ctx->npc_buf_nr);
	M0_PRE(bs != NULL);

	/* check buffer length and offset */
	if (bs->bs_ev.nbe_length != ctx->npc_buf_size ||
	    bs->bs_ev.nbe_offset != 0)
		goto bad_buf;
	/* search for test server index */
	server_index = m0_net_test_network_ep_search(&ctx->npc_net,
					bs->bs_ev.nbe_ep->nep_addr);
	if (server_index == -1)
		goto bad_buf;
	/* decode buffer */
	decoded = node_ping_timestamp_get(&ctx->npc_net, buf_index, &ts);
	if (!decoded)
		goto bad_buf;
	/* check time in received buffer */
	if (bs->bs_time < ts.ntt_time)
		goto bad_buf;
	/* search sequence number */
	buf_index_send = node_ping_client_search_seq(ctx, server_index,
						     ts.ntt_seq);
	if (buf_index_send == -1)
		goto bad_buf;
	bs_send		       = &ctx->npc_buf_state[buf_index_send];
	bs_send->bs_index_pair = buf_index;
	bs->bs_index_pair      = buf_index_send;
	/* successfully received message */
	sd_update(ctx, MT_TRANSFER, MS_SUCCESS, MD_BOTH);
	/* update RTT statistics */
	rtt = m0_time_sub(bs->bs_time, ts.ntt_time);
	m0_net_test_nh_sd_update_rtt(&ctx->npc_nh, rtt);
	goto good_buf;
bad_buf:
	sd_update(ctx, MT_TRANSFER, MS_BAD, MD_BOTH);
good_buf:
	/* enqueue recv buffer */
	if (bs_send == NULL) {
		node_ping_buf_enqueue_recv(ctx, buf_index);
	}
	return bs_send != NULL;
}

static void node_ping_msg_cb(struct m0_net_test_network_ctx *net_ctx,
			     uint32_t buf_index,
			     enum m0_net_queue_type q,
			     const struct m0_net_buffer_event *ev)
{
	struct node_ping_ctx *ctx;
	struct buf_state     *bs;
	m0_time_t	      now = m0_time_now();

	M0_PRE(q == M0_NET_QT_MSG_RECV || q == M0_NET_QT_MSG_SEND);

	ctx = node_ping_ctx_from_net_ctx(net_ctx);
	bs = &ctx->npc_buf_state[buf_index];

	LOGD("role = %d, buf_index = %u, nbe_status = %d, q = %d",
	     ctx->npc_nh.ntnh_role, buf_index, ev->nbe_status, q);
	LOGD(", ev->nbe_length = %lu", (long unsigned) ev->nbe_length);

	if (q == M0_NET_QT_MSG_RECV && ev->nbe_status == 0)
		LOGD(", ev->nbe_ep->nep_addr = %s", ev->nbe_ep->nep_addr);

	if (ev->nbe_status == -ECANCELED) {
		m0_semaphore_up(&ctx->npc_buf_q_sem);
		return;
	}
	/* save buffer event */
	bs->bs_ev = *ev;
	/* save endpoint from successfully received buffer */
	if (ev->nbe_status == 0 &&
	    ev->nbe_buffer->nb_qtype == M0_NET_QT_MSG_RECV)
		m0_net_end_point_get(ev->nbe_ep);
	bs->bs_time = now;
	bs->bs_q    = ev->nbe_buffer->nb_qtype;

	m0_net_test_ringbuf_push(&ctx->npc_buf_rb, buf_index);
	m0_semaphore_up(&ctx->npc_buf_q_sem);
	m0_semaphore_up(&ctx->npc_buf_rb_sem);
}

static void node_ping_cb_impossible(struct m0_net_test_network_ctx *ctx,
				    const uint32_t buf_index,
				    enum m0_net_queue_type q,
				    const struct m0_net_buffer_event *ev)
{
	M0_IMPOSSIBLE("Impossible network bulk callback: "
		      "net-test ping node can't have it.");
}

static struct m0_net_test_network_buffer_callbacks node_ping_buf_cb = {
	.ntnbc_cb = {
		[M0_NET_QT_MSG_RECV]		= node_ping_msg_cb,
		[M0_NET_QT_MSG_SEND]		= node_ping_msg_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV]	= node_ping_cb_impossible,
		[M0_NET_QT_PASSIVE_BULK_SEND]	= node_ping_cb_impossible,
		[M0_NET_QT_ACTIVE_BULK_RECV]	= node_ping_cb_impossible,
		[M0_NET_QT_ACTIVE_BULK_SEND]	= node_ping_cb_impossible,
	}
};

static void node_ping_to_check(struct node_ping_ctx *ctx)
{
	struct buf_state *bs;
	m0_time_t	  now = m0_time_now();
	ssize_t		  buf_index;

	while ((buf_index = node_ping_to_peek(ctx)) != -1) {
		bs = &ctx->npc_buf_state[buf_index];
		if (bs->bs_deadline > now)
			break;
		/* message timed out */
		node_ping_to_del(ctx, buf_index);
		++ctx->npc_client->npcc_msg_rt;
		node_ping_client_send(ctx, buf_index);
	}
}

static void node_ping_client_handle(struct node_ping_ctx *ctx,
				    struct buf_state *bs,
				    size_t buf_index)
{
	bool good_buf;

	M0_PRE(ctx != NULL);
	M0_PRE(bs != NULL);

	if (bs->bs_q == M0_NET_QT_MSG_SEND) {
		if (bs->bs_errno != 0 || bs->bs_ev.nbe_status != 0) {
			/* try to send again */
			node_ping_client_send(ctx, buf_index);
		} else {
			node_ping_client_cb2(ctx, buf_index);
		}
	} else {
		if (bs->bs_errno != 0 || bs->bs_ev.nbe_status != 0) {
			/* try to receive again */
			node_ping_buf_enqueue_recv(ctx, buf_index);
		} else {
			/* buffer was successfully received from test server */
			good_buf = node_ping_client_recv_cb(ctx, bs, buf_index);
			if (good_buf)
				node_ping_client_cb2(ctx, bs->bs_index_pair);
		}
	}
}

static void node_ping_server_handle(struct node_ping_ctx *ctx,
				    struct buf_state *bs,
				    size_t buf_index)
{
	if (bs->bs_q == M0_NET_QT_MSG_RECV && bs->bs_errno == 0 &&
	    bs->bs_ev.nbe_status == 0) {
		/* send back to test client */
		node_ping_buf_enqueue(ctx, buf_index, M0_NET_QT_MSG_SEND,
				      bs->bs_ev.nbe_ep, 0);
	} else {
		/* add to recv queue */
		node_ping_buf_enqueue_recv(ctx, buf_index);
	}
}

static void node_ping_worker(struct node_ping_ctx *ctx)
{
	struct buf_state	  *bs;
	size_t			  buf_index;
	bool			  failed;
	size_t			  i;
	m0_time_t		  to_check_interval;
	m0_time_t		  deadline;
	struct m0_net_end_point	 *ep;
	bool			  rb_is_empty;
	ssize_t			  to_index;

	M0_PRE(ctx != NULL);

	to_check_interval = M0_MKTIME(TO_CHECK_INTERVAL / 1000,
				      TO_CHECK_INTERVAL * 1000000);
	while (1) {
		/* get buffer index from ringbuf */
		deadline = m0_time_add(m0_time_now(), to_check_interval);
		rb_is_empty = !m0_semaphore_timeddown(&ctx->npc_buf_rb_sem,
						      deadline);
		/* check timeout list */
		if (ctx->npc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT)
			node_ping_to_check(ctx);
		if (rb_is_empty)
			continue;
		if (ctx->npc_buf_rb_done)
			break;
		buf_index = m0_net_test_ringbuf_pop(&ctx->npc_buf_rb);
		bs = &ctx->npc_buf_state[buf_index];
		LOGD("POP from ringbuf: %lu, role = %d",
		     buf_index, ctx->npc_nh.ntnh_role);
		/* update total/failed stats */
		failed = bs->bs_errno != 0 || bs->bs_ev.nbe_status != 0;
		sd_update(ctx, MT_MSG, failed ? MS_FAILED : MS_SUCCESS,
			  bs->bs_q == M0_NET_QT_MSG_RECV ? MD_RECV : MD_SEND);
		ep = bs->bs_errno == 0 && bs->bs_ev.nbe_status == 0 &&
		     bs->bs_q == M0_NET_QT_MSG_RECV ? bs->bs_ev.nbe_ep : NULL;
		/* handle buffer */
		if (ctx->npc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT)
			node_ping_client_handle(ctx, bs, buf_index);
		else
			node_ping_server_handle(ctx, bs, buf_index);
		if (ep != NULL)
			m0_net_end_point_put(ep);
		/* update copy of statistics */
		m0_net_test_nh_sd_copy_locked(&ctx->npc_nh);
	}
	/* dequeue all buffers */
	for (i = 0; i < ctx->npc_buf_nr; ++i) {
		m0_net_test_network_buffer_dequeue(&ctx->npc_net,
						   M0_NET_TEST_BUF_PING, i);
	}
	/* wait for buffer callbacks */
	for (i = 0; i < ctx->npc_buf_nr; ++i)
		m0_semaphore_down(&ctx->npc_buf_q_sem);
	/* clear timeouts list for the test client */
	if (ctx->npc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		while ((to_index = node_ping_to_peek(ctx)) != -1)
			node_ping_to_del(ctx, to_index);
	}
	/*
	 * Clear ringbuf, put() every saved endpoint.
	 * Use !m0_net_test_ringbuf_is_empty(&ctx->npc_buf_rb) instead of
	 * m0_semaphore_trydown(&ctx->npc_buf_rb_sem) because
	 * ctx->npc_buf_rb_sem may not be up()'ed in buffer callback.
	 */
	while (!m0_net_test_ringbuf_is_empty(&ctx->npc_buf_rb)) {
		buf_index = m0_net_test_ringbuf_pop(&ctx->npc_buf_rb);
		bs = &ctx->npc_buf_state[buf_index];
		if (bs->bs_q == M0_NET_QT_MSG_RECV &&
		    bs->bs_errno == 0 && bs->bs_ev.nbe_status == 0)
			m0_net_end_point_put(bs->bs_ev.nbe_ep);
	}
}

static void node_ping_rb_fill(struct node_ping_ctx *ctx)
{
	size_t i;
	size_t half_buf = ctx->npc_buf_nr / 2;

	if (ctx->npc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		M0_ASSERT(ctx->npc_buf_nr % 2 == 0);
		/* add recv buffers */
		for (i = 0; i < half_buf; ++i)
			node_ping_buf_enqueue_recv(ctx, half_buf + i);
		/* add send buffers */
		for (i = 0; i < half_buf; ++i)
			node_ping_client_send(ctx, i);
	} else {
		for (i = 0; i < ctx->npc_buf_nr; ++i)
			node_ping_buf_enqueue_recv(ctx, i);
	}
}

static int node_ping_test_init_fini(struct node_ping_ctx *ctx,
				    const struct m0_net_test_cmd *cmd)
{
	struct m0_net_test_network_cfg net_cfg;
	int			       rc;
	int			       i;

	if (cmd == NULL) {
		rc = 0;
		if (ctx->npc_nh.ntnh_test_initialized)
			goto fini;
		else
			goto exit;
	}

	rc = m0_semaphore_init(&ctx->npc_buf_q_sem, ctx->npc_buf_nr);
	if (rc != 0)
		goto exit;
	rc = m0_semaphore_init(&ctx->npc_buf_rb_sem, 0);
	if (rc != 0)
		goto free_buf_q_sem;
	rc = m0_net_test_ringbuf_init(&ctx->npc_buf_rb, ctx->npc_buf_nr);
	if (rc != 0)
		goto free_buf_rb_sem;
	M0_ALLOC_ARR(ctx->npc_buf_state, ctx->npc_buf_nr);
	if (ctx->npc_buf_state == NULL)
		goto free_buf_rb;

	/* initialize network context */
	M0_SET0(&net_cfg);
	net_cfg.ntncfg_tm_cb	     = node_ping_tm_cb;
	net_cfg.ntncfg_buf_cb	     = node_ping_buf_cb;
	net_cfg.ntncfg_buf_size_ping = ctx->npc_buf_size;
	net_cfg.ntncfg_buf_ping_nr   = ctx->npc_buf_nr;
	net_cfg.ntncfg_ep_max	     = cmd->ntc_init.ntci_ep.ntsl_nr;
	net_cfg.ntncfg_timeouts	     = m0_net_test_network_timeouts_never();
	net_cfg.ntncfg_timeouts.ntnt_timeout[M0_NET_QT_MSG_SEND] =
		ctx->npc_buf_send_timeout;
	rc = m0_net_test_network_ctx_init(&ctx->npc_net, &net_cfg,
					  cmd->ntc_init.ntci_tm_ep);
	if (rc != 0)
		goto free_buf_state;
	/* add test node endpoints to the network context endpoint list */
	rc = m0_net_test_network_ep_add_slist(&ctx->npc_net,
					      &cmd->ntc_init.ntci_ep);
	if (rc != 0)
		goto fini;
	if (ctx->npc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		buf_state_tlist_init(&ctx->npc_client->npcc_to);
		for (i = 0; i < ctx->npc_buf_nr; ++i)
			buf_state_tlink_init(&ctx->npc_buf_state[i]);
	}
	rc = 0;
	goto exit;
fini:
	if (ctx->npc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		for (i = 0; i < ctx->npc_buf_nr; ++i)
			buf_state_tlink_fini(&ctx->npc_buf_state[i]);
		buf_state_tlist_fini(&ctx->npc_client->npcc_to);
	}
	m0_net_test_network_ctx_fini(&ctx->npc_net);
free_buf_state:
	m0_free(ctx->npc_buf_state);
free_buf_rb:
	m0_net_test_ringbuf_fini(&ctx->npc_buf_rb);
free_buf_rb_sem:
	m0_semaphore_fini(&ctx->npc_buf_rb_sem);
free_buf_q_sem:
	m0_semaphore_fini(&ctx->npc_buf_q_sem);
exit:
	return rc;
}

static void *node_ping_initfini(void *ctx_, struct m0_net_test_service *svc)
{
	struct node_ping_ctx *ctx;
	int		      rc;

	M0_PRE(equi(ctx_ == NULL, svc != NULL));

	if (svc != NULL) {
		M0_ALLOC_PTR(ctx);
		if (ctx != NULL) {
			ctx->npc_svc			  = svc;
			ctx->npc_nh.ntnh_test_initialized = false;
		}
	} else {
		ctx = ctx_;
		rc = node_ping_test_init_fini(ctx, NULL);
		M0_ASSERT(rc == 0);
		m0_free(ctx);
	}
	return svc != NULL ? ctx : NULL;
}

static void *node_ping_init(struct m0_net_test_service *svc)
{
	return node_ping_initfini(NULL, svc);
}

static void node_ping_fini(void *ctx_)
{
	void *rc = node_ping_initfini(ctx_, NULL);
	M0_POST(rc == NULL);
}

static int node_ping_step(void *ctx_)
{
	return 0;
}

static int node_ping_cmd_init(void *ctx_,
			      const struct m0_net_test_cmd *cmd,
			      struct m0_net_test_cmd *reply)
{
	const struct m0_net_test_cmd_init *icmd;
	struct node_ping_ctx		  *ctx = ctx_;
	int				   rc;
	bool				   role_client;

	M0_PRE(ctx != NULL);
	M0_PRE(cmd != NULL);
	M0_PRE(reply != NULL);

	icmd	    = &cmd->ntc_init;
	role_client = icmd->ntci_role == M0_NET_TEST_ROLE_CLIENT;

	/* ep wasn't recognized */
	if (cmd->ntc_ep_index == -1) {
		rc = -ENOENT;
		goto reply;
	}
	/* network context already initialized */
	if (ctx->npc_nh.ntnh_test_initialized) {
		rc = -EALREADY;
		goto reply;
	}
	/* check command type */
	M0_ASSERT(icmd->ntci_type == M0_NET_TEST_TYPE_PING);
	/* parse INIT command */
	m0_net_test_nh_init(&ctx->npc_nh, icmd);
	ctx->npc_buf_size	  = icmd->ntci_msg_size;
	ctx->npc_buf_send_timeout = icmd->ntci_buf_send_timeout;

	ctx->npc_buf_nr	 = icmd->ntci_msg_concurrency;
	ctx->npc_buf_nr *= role_client ?  2 * icmd->ntci_ep.ntsl_nr : 1;

	ctx->npc_client = role_client  ? &ctx->npc__client : NULL;
	ctx->npc_server = !role_client ? &ctx->npc__server : NULL;

	if (role_client) {
		M0_SET0(ctx->npc_client);
		ctx->npc_client->npcc_msg_rt_max  = icmd->ntci_msg_nr;
		ctx->npc_client->npcc_concurrency = icmd->ntci_msg_concurrency;
	}

	/* do sanity check */
	rc = 0;
	if (ctx->npc_buf_size < 1 || ctx->npc_buf_nr < 1 ||
	    equi(ctx->npc_client == NULL, ctx->npc_server == NULL))
		rc = -EINVAL;
	/* init node_ping_ctx fields */
	if (rc == 0)
		rc = node_ping_test_init_fini(ctx, cmd);
	if (rc != 0) {
		/* change service state */
		m0_net_test_service_state_change(ctx->npc_svc,
						 M0_NET_TEST_SERVICE_FAILED);
	}
reply:
	/* fill reply */
	reply->ntc_type = M0_NET_TEST_CMD_INIT_DONE;
	reply->ntc_done.ntcd_errno = rc;
	return rc;
}

static int node_ping_cmd_start(void *ctx_,
			       const struct m0_net_test_cmd *cmd,
			       struct m0_net_test_cmd *reply)
{
	struct m0_net_test_cmd_status_data *sd;
	struct node_ping_ctx		   *ctx = ctx_;
	int				    rc;
	const m0_time_t			    _1s = M0_MKTIME(1, 0);

	M0_PRE(ctx != NULL);
	M0_PRE(cmd != NULL);
	M0_PRE(reply != NULL);

	sd = &ctx->npc_nh.ntnh_sd;
	M0_SET0(sd);

	/* fill test start time */
	sd->ntcsd_time_start = m0_time_now();
	/* initialize stats */
	m0_net_test_mps_init(&sd->ntcsd_mps_send, 0, sd->ntcsd_time_start, _1s);
	m0_net_test_mps_init(&sd->ntcsd_mps_recv, 0, sd->ntcsd_time_start, _1s);
	/* add buffer indexes to ringbuf */
	node_ping_rb_fill(ctx);
	/* start test */
	ctx->npc_buf_rb_done = false;
	rc = M0_THREAD_INIT(&ctx->npc_thread, struct node_ping_ctx *, NULL,
			    &node_ping_worker, ctx, "net-test ping");
	if (rc != 0) {
		/* change service state */
		m0_net_test_service_state_change(ctx->npc_svc,
						 M0_NET_TEST_SERVICE_FAILED);
	}
	/* fill reply */
	reply->ntc_type = M0_NET_TEST_CMD_START_DONE;
	reply->ntc_done.ntcd_errno = rc;
	return rc;
}

static int node_ping_cmd_stop(void *ctx_,
			      const struct m0_net_test_cmd *cmd,
			      struct m0_net_test_cmd *reply)
{
	struct node_ping_ctx *ctx = ctx_;
	int		      rc;

	M0_PRE(ctx != NULL);
	M0_PRE(cmd != NULL);
	M0_PRE(reply != NULL);

	/* stop worker thread */
	ctx->npc_buf_rb_done = true;
	m0_semaphore_up(&ctx->npc_buf_rb_sem);
	rc = m0_thread_join(&ctx->npc_thread);
	M0_ASSERT(rc == 0);
	m0_thread_fini(&ctx->npc_thread);
	/* change service state */
	m0_net_test_service_state_change(ctx->npc_svc,
					 M0_NET_TEST_SERVICE_FINISHED);
	/* fill reply */
	reply->ntc_type = M0_NET_TEST_CMD_STOP_DONE;
	reply->ntc_done.ntcd_errno = 0;
	return 0;
}

static int node_ping_cmd_status(void *ctx_,
				const struct m0_net_test_cmd *cmd,
				struct m0_net_test_cmd *reply)
{
	struct node_ping_ctx *ctx = ctx_;

	M0_PRE(ctx != NULL);

	m0_net_test_nh_cmd_status(&ctx->npc_nh, cmd, reply);

	return 0;
}

static struct m0_net_test_service_cmd_handler node_ping_cmd_handler[] = {
	{
		.ntsch_type    = M0_NET_TEST_CMD_INIT,
		.ntsch_handler = node_ping_cmd_init,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_START,
		.ntsch_handler = node_ping_cmd_start,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_STOP,
		.ntsch_handler = node_ping_cmd_stop,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_STATUS,
		.ntsch_handler = node_ping_cmd_status,
	},
};

struct m0_net_test_service_ops m0_net_test_node_ping_ops = {
	.ntso_init	     = node_ping_init,
	.ntso_fini	     = node_ping_fini,
	.ntso_step	     = node_ping_step,
	.ntso_cmd_handler    = node_ping_cmd_handler,
	.ntso_cmd_handler_nr = ARRAY_SIZE(node_ping_cmd_handler),
};

#undef NET_TEST_MODULE_NAME

/**
   @} end of NetTestPingNodeInternals group
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
