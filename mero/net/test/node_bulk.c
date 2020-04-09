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

#include "lib/arith.h"			/* M0_SWAP */
#include "lib/errno.h"			/* EALREADY */
#include "lib/memory.h"			/* M0_ALLOC_PTR */
#include "lib/misc.h"			/* M0_IN */
#include "lib/atomic.h"			/* m0_atomic64 */

#include "mero/magic.h"		/* M0_NET_TEST_BSB_MAGIC */

#include "net/test/network.h"		/* m0_net_test_network_ctx */
#include "net/test/node.h"		/* m0_net_test_node_ctx */
#include "net/test/node_helper.h"	/* m0_net_test_node_ctx */
#include "net/test/service.h"		/* m0_net_test_service */
#include "net/test/ringbuf.h"		/* m0_net_test_ringbuf */

#include "net/test/node_bulk.h"

#define NET_TEST_MODULE_NAME node_bulk
#include "net/test/debug.h"		/* LOGD */

/**
   @defgroup NetTestBulkNodeInternals Bulk Node
   @ingroup NetTestInternals

   Bulk node service m0_net_test_service_ops:
   - ntso_init
     - allocate bulk node context
   - ntso_fini
     - finalize network context if it was initialized;
     - free bulk node context
   - ntso_step
     - the same as node_ping
   - ntso_cmd_handler
     - M0_NET_TEST_CMD_INIT
       - initialize network context
       - reset statistics
       - send M0_NET_TEST_CMD_INIT_DONE reply
     - M0_NET_TEST_CMD_START
       - the same as node_ping
     - M0_NET_TEST_CMD_STOP
       - the same as node_ping
     - M0_NET_TEST_CMD_STATUS
       - fill and send M0_NET_TEST_CMD_STATUS_DATA reply

  Test client:
  - bulk-buffer-not-in-net-queue queue
  - ping-buffer-not-in-net-queue queue
  - enqueue pair <send buf, recv buf> (with m0_net level timeouts)
  - max_bd_in_one_message (parameter)
  - send bd list to the test server without waiting for reply
    or delivery confirmation
  - number of ping buffers
  - number of bulk buffers

  Test server:
  - number of ping buffers
  - number of bulk buffers
  - all bulk and ping buffers in recv queue with M0_TIME_NEVER timeout

   @{
 */

/**
 * Test message transfer state.
 * @see @ref net-test-lspec-algo-client-bulk,
 *	@ref net-test-lspec-algo-server-bulk.
 */
enum transfer_state {
	TS_UNUSED = 0,		/**< client & server state */
	TS_QUEUED,		/**< client state */
	TS_BD_SENT,		/**< client state */
	TS_CB_LEFT2,		/**< client state */
	TS_CB_LEFT1,		/**< client state */
	TS_FAILED2,		/**< client state */
	TS_FAILED1,		/**< client state */
	TS_BD_RECEIVED,		/**< server state */
	TS_SENDING,		/**< server state */
	TS_RECEIVING,		/**< server state */
	TS_BADMSG,		/**< client & server state */
	TS_FAILED,		/**< client & server state */
	TS_TRANSFERRED,		/**< client & server state */
};

/**
 * Return values for network operations.
 * @todo add timestamp
 */
struct buf_status_errno {
	int bse_func_rc;	/**< Network buffer addition to queue */
	int bse_cb_rc;		/**< Network buffer callback */
};

/** Bulk transfer status */
struct buf_status_bulk {
	/** Magic for bsb_tlink */
	uint64_t		bsb_magic;
	/** State of bulk transfer */
	enum transfer_state	bsb_ts;
	/**
	 * Bulk buffer index for the test server and
	 * bulk buffer pair index for the test client.
	 */
	size_t			bsb_index;
	/** Link for server_status_bulk.ssb_buffers */
	struct m0_tlink		bsb_tlink;
	/** M0_NET_QT_MSG_SEND, M0_NET_QT_MSG_RECV functions and callbacks. */
	struct buf_status_errno bsb_msg;
	/**
	 * M0_NET_QT_ACTIVE_SEND, M0_NET_QT_PASSIVE_SEND
	 * functions and callbacks.
	 */
	struct buf_status_errno bsb_send;
	/**
	 * M0_NET_QT_ACTIVE_RECV, M0_NET_QT_PASSIVE_RECV
	 * functions and callbacks.
	 */
	struct buf_status_errno bsb_recv;
	/**
	 * Bulk buffer network descriptor for M0_NET_QT_ACTIVE_BULK_SEND
	 * network queue (test server).
	 */
	struct m0_net_buf_desc  bsb_desc_send;
	/** Transfer start time */
	m0_time_t		bsb_time_start;
	/** Transfer finish time */
	m0_time_t		bsb_time_finish;
};

/** Buffer of message with bulk buffer network descriptors status */
struct buf_status_ping {
	/** Magic for bsp_tlink */
	uint64_t	bsp_magic;
	/** Ping buffer index */
	size_t		bsp_index;
	/**
	 * Message contains network descriptors for the buffer
	 * pairs for the test client (or buffers for the test server)
	 * from this list.
	 */
	struct m0_tl	bsp_buffers;
	/** Link for ping buffer list */
	struct m0_tlink bsp_tlink;
};

/** Server status for the test client */
struct server_status_bulk {
	/** Magic for ssb_tlink */
	uint64_t	ssb_magic;
	/** Server index */
	size_t		ssb_index;
	/** Link for list of servers */
	struct m0_tlink ssb_tlink;
	/** List of queued buffers for this server */
	struct m0_tl	ssb_buffers;
};

/**
 * State transition. Used in state transition checks instead of
 * transition matrix because number of allowed state transitions is
 * less than total number of possible state transitions in a few times.
 * @see transfer_state
 */
struct state_transition {
	enum transfer_state sta_from;	/**< Current state */
	enum transfer_state sta_to;	/**< New state */
};

M0_TL_DESCR_DEFINE(bsb, "buf_status_bulk", static,
		   struct buf_status_bulk, bsb_tlink, bsb_magic,
		   M0_NET_TEST_BSB_MAGIC, M0_NET_TEST_BSB_HEAD_MAGIC);
M0_TL_DEFINE(bsb, static, struct buf_status_bulk);

M0_TL_DESCR_DEFINE(ssb, "server_status_bulk", static,
		   struct server_status_bulk, ssb_tlink, ssb_magic,
		   M0_NET_TEST_SSB_MAGIC, M0_NET_TEST_SSB_HEAD_MAGIC);
M0_TL_DEFINE(ssb, static, struct server_status_bulk);

M0_TL_DESCR_DEFINE(bsp, "buf_status_ping", static,
		   struct buf_status_ping, bsp_tlink, bsp_magic,
		   M0_NET_TEST_BSP_MAGIC, M0_NET_TEST_BSP_HEAD_MAGIC);
M0_TL_DEFINE(bsp, static, struct buf_status_ping);

/**
 * Bulk test context.
 * Buffer mapping for the test client:
 * even buffers - for sending
 * odd buffers - for receiving
 * first (nbc_client_concurrency * 2) buffers for server #0, next
 * (nbc_client_concurrency * 2) buffers for server #1 and so on.
 * Buffer mapping for the test server:
 * All buffers are used for receiving and sending.
 * @todo move equal parts from node_bulk_ctx & node_ping_ctx to single struct
 */
struct node_bulk_ctx {
	/** Node helper */
	struct m0_net_test_nh		   nbc_nh;
	/** Network context for testing */
	struct m0_net_test_network_ctx	   nbc_net;
	/** Test service. Used when changing service state. */
	struct m0_net_test_service	  *nbc_svc;
	/** Worker thread */
	struct m0_thread		   nbc_thread;
	/** Number of ping buffers */
	size_t				   nbc_buf_ping_nr;
	/** Number of bulk buffers */
	size_t				   nbc_buf_bulk_nr;
	/** Ping buffer size */
	m0_bcount_t			   nbc_buf_size_ping;
	/** Bulk buffer size */
	m0_bcount_t			   nbc_buf_size_bulk;
	/** Maximum number of buffer descriptors, stored to ping buffer */
	size_t				   nbc_bd_nr_max;
	/** Bulk buffer states */
	struct buf_status_bulk		  *nbc_bs;
	/** Number of bulk buffer states */
	size_t				   nbc_bs_nr;
	/** Ping buffer states */
	struct buf_status_ping		  *nbc_bsp;
	/** List of unused message buffers */
	struct m0_net_test_ringbuf	   nbc_rb_ping_unused;
	/** List of unused bulk buffers */
	struct m0_net_test_ringbuf	   nbc_rb_bulk_unused;
	/**
	 * List of added to passive network queue buffers.
	 * Network buffer descriptors wasn't sent yet for these buffers.
	 */
	struct m0_net_test_ringbuf	   nbc_rb_bulk_queued;
	/**
	 * List of bulk transfers, that are in final state.
	 */
	struct m0_net_test_ringbuf	   nbc_rb_bulk_final;
	/**
	 * Test client concurrency.
	 * Test client will send nbc_client_concurrency test messages to
	 * every server simultaneously.
	 */
	size_t				   nbc_client_concurrency;
	/** Per server status for the test client */
	struct server_status_bulk	  *nbc_sstatus;
	/**
	 * Channel for bulk testing inner loop.
	 * STOP command will send signal to this channel.
	 */
	struct m0_chan			   nbc_stop_chan;
	/** Mutex for node_bulk_ctx.nbc_stop_chan */
	struct m0_mutex			   nbc_stop_chan_mutex;
	/**
	 * Clink for bulk testing inner loop.
	 * This clink is head for clink group, consists of this clink
	 * and network transfer machine notification clink.
	 */
	struct m0_clink			   nbc_stop_clink;
	/** Stop flag */
	struct m0_atomic64		   nbc_stop_flag;
	/**
	 * At least one callback was executed.
	 * Set to true in every callback.
	 */
	bool				   nbc_callback_executed;
};

/** Wrapper for m0_net_test_nh_sd_update() with smaller name */
static void sd_update(struct node_bulk_ctx *ctx,
		      enum m0_net_test_nh_msg_type type,
		      enum m0_net_test_nh_msg_status status,
		      enum m0_net_test_nh_msg_direction direction)
{
	m0_net_test_nh_sd_update(&ctx->nbc_nh, type, status, direction);
}

static void node_bulk_tm_event_cb(const struct m0_net_tm_event *ev)
{
	/* nothing for now */
}

static const struct m0_net_tm_callbacks node_bulk_tm_cb = {
	.ntc_event_cb = node_bulk_tm_event_cb
};

static struct node_bulk_ctx *
node_bulk_ctx_from_net_ctx(struct m0_net_test_network_ctx *net_ctx)
{
	return container_of(net_ctx, struct node_bulk_ctx, nbc_net);
}

static bool
node_bulk_state_change_allowed(enum transfer_state from,
			       enum transfer_state to,
			       const struct state_transition allowed[],
			       size_t allowed_size)
{
	size_t i;

	M0_PRE(allowed != NULL);
	M0_PRE(allowed_size > 0);

	for (i = 0; i < allowed_size; ++i) {
		if (allowed[i].sta_from == from && allowed[i].sta_to == to)
			break;
	}
	return i < allowed_size;
}

static bool node_bulk_state_is_final(enum transfer_state state)
{
	return M0_IN(state, (TS_FAILED, TS_TRANSFERRED, TS_BADMSG));
}

static void node_bulk_state_change(struct node_bulk_ctx *ctx,
				   size_t bs_index,
				   enum transfer_state state)
{
	static const struct state_transition allowed_client[] = {
		{ .sta_from = TS_UNUSED,	.sta_to = TS_QUEUED },
		{ .sta_from = TS_QUEUED,	.sta_to = TS_BD_SENT },
		{ .sta_from = TS_QUEUED,	.sta_to = TS_FAILED },
		{ .sta_from = TS_QUEUED,	.sta_to = TS_FAILED1 },
		{ .sta_from = TS_QUEUED,	.sta_to = TS_FAILED2 },
		{ .sta_from = TS_BD_SENT,	.sta_to = TS_CB_LEFT2 },
		{ .sta_from = TS_BD_SENT,	.sta_to = TS_FAILED2 },
		{ .sta_from = TS_CB_LEFT2,	.sta_to = TS_CB_LEFT1 },
		{ .sta_from = TS_CB_LEFT2,	.sta_to = TS_FAILED1 },
		{ .sta_from = TS_CB_LEFT1,	.sta_to = TS_TRANSFERRED },
		{ .sta_from = TS_CB_LEFT1,	.sta_to = TS_FAILED },
		{ .sta_from = TS_FAILED2,	.sta_to = TS_FAILED1 },
		{ .sta_from = TS_FAILED1,	.sta_to = TS_FAILED },
		{ .sta_from = TS_TRANSFERRED,	.sta_to = TS_UNUSED },
		{ .sta_from = TS_FAILED,	.sta_to = TS_UNUSED },
	};
	static const struct state_transition allowed_server[] = {
		{ .sta_from = TS_UNUSED,	.sta_to = TS_BD_RECEIVED },
		{ .sta_from = TS_BD_RECEIVED,	.sta_to = TS_BADMSG },
		{ .sta_from = TS_BD_RECEIVED,	.sta_to = TS_RECEIVING },
		{ .sta_from = TS_RECEIVING,	.sta_to = TS_SENDING },
		{ .sta_from = TS_RECEIVING,	.sta_to = TS_FAILED },
		{ .sta_from = TS_SENDING,	.sta_to = TS_TRANSFERRED },
		{ .sta_from = TS_SENDING,	.sta_to = TS_FAILED },
		{ .sta_from = TS_TRANSFERRED,	.sta_to = TS_UNUSED },
		{ .sta_from = TS_FAILED,	.sta_to = TS_UNUSED },
		{ .sta_from = TS_BADMSG,	.sta_to = TS_UNUSED },
	};
	enum m0_net_test_role	role;
	struct buf_status_bulk *bs;
	bool			can_change;
	bool			role_client;

	M0_PRE(ctx != NULL);
	M0_PRE(bs_index < ctx->nbc_bs_nr);
	M0_PRE(ctx->nbc_bs != NULL);

	LOGD("state_change: role = %d, bs_index = %lu, state = %d",
	     ctx->nbc_nh.ntnh_role, bs_index, state);

	role = ctx->nbc_nh.ntnh_role;
	bs = &ctx->nbc_bs[bs_index];
	role_client = role == M0_NET_TEST_ROLE_CLIENT;
	can_change = node_bulk_state_change_allowed(bs->bsb_ts, state,
			role_client ? allowed_client : allowed_server,
			role_client ? ARRAY_SIZE(allowed_client) :
				      ARRAY_SIZE(allowed_server));
	M0_ASSERT(can_change);
	bs->bsb_ts = state;

	/* add to ringbufs if needed */
	if (state == TS_UNUSED)
		m0_net_test_ringbuf_push(&ctx->nbc_rb_bulk_unused, bs_index);
	if (node_bulk_state_is_final(state))
		m0_net_test_ringbuf_push(&ctx->nbc_rb_bulk_final, bs_index);
	/* set start & finish timestamp */
	if (M0_IN(state, (TS_RECEIVING, TS_QUEUED)))
		bs->bsb_time_start = m0_time_now();
	if (state == TS_TRANSFERRED)
		bs->bsb_time_finish = m0_time_now();
	/* reset buf_status_errno if needed */
	if (state == TS_UNUSED) {
		M0_SET0(&bs->bsb_msg);
		M0_SET0(&bs->bsb_send);
		M0_SET0(&bs->bsb_recv);
	}
}

static const struct state_transition node_bulk_client_success[] = {
	{ .sta_from = TS_BD_SENT,	.sta_to = TS_CB_LEFT2 },
	{ .sta_from = TS_CB_LEFT2,	.sta_to = TS_CB_LEFT1 },
	{ .sta_from = TS_CB_LEFT1,	.sta_to = TS_TRANSFERRED },
	{ .sta_from = TS_FAILED2,	.sta_to = TS_FAILED1 },
	{ .sta_from = TS_FAILED1,	.sta_to = TS_FAILED },
};
static const struct state_transition node_bulk_client_failure[] = {
	{ .sta_from = TS_BD_SENT,	.sta_to = TS_FAILED2 },
	{ .sta_from = TS_CB_LEFT2,	.sta_to = TS_FAILED1 },
	{ .sta_from = TS_CB_LEFT1,	.sta_to = TS_FAILED },
	{ .sta_from = TS_FAILED2,	.sta_to = TS_FAILED1 },
	{ .sta_from = TS_FAILED1,	.sta_to = TS_FAILED },
};
static const struct state_transition node_bulk_server_success[] = {
	{ .sta_from = TS_RECEIVING,	.sta_to = TS_SENDING },
	{ .sta_from = TS_SENDING,	.sta_to = TS_TRANSFERRED },
};
static const struct state_transition node_bulk_server_failure[] = {
	{ .sta_from = TS_RECEIVING,	.sta_to = TS_FAILED },
	{ .sta_from = TS_SENDING,	.sta_to = TS_FAILED },
};

static const struct {
	const struct state_transition *nbst_transition;
	const size_t		       nbst_nr;
} node_bulk_state_transitions[] = {
#define TRANSITION(name) {		\
	.nbst_transition = name,	\
	.nbst_nr = ARRAY_SIZE(name),	\
}
	TRANSITION(node_bulk_client_success),
	TRANSITION(node_bulk_client_failure),
	TRANSITION(node_bulk_server_success),
	TRANSITION(node_bulk_server_failure),
#undef TRANSITION
};

/** Check for unique "from" state in the list */
static void node_bulk_state_check(const struct state_transition state_list[],
				  size_t state_nr)
{
	size_t i;
	size_t j;

	for (i = 0; i < state_nr; ++i) {
		for (j = i + 1; j < state_nr; ++j) {
			M0_ASSERT(state_list[i].sta_from !=
				  state_list[j].sta_from);
		}
	}
}

/** Check for unique "from" state in all state transitions */
static void node_bulk_state_check_all(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(node_bulk_state_transitions); ++i) {
		node_bulk_state_check(
			node_bulk_state_transitions[i].nbst_transition,
			node_bulk_state_transitions[i].nbst_nr);
	}
}

static enum transfer_state
node_bulk_state_search(enum transfer_state state,
		       const struct state_transition state_list[],
		       size_t state_nr)
{
	size_t i;

	for (i = 0; i < state_nr; ++i) {
		if (state_list[i].sta_from == state)
			return state_list[i].sta_to;
	}
	M0_IMPOSSIBLE("Invalid 'from' state in net-test bulk testing.");
	return TS_UNUSED;
}

static void node_bulk_state_change_cb(struct node_bulk_ctx *ctx,
				      size_t bs_index,
				      bool success)
{
	const struct state_transition *transition;
	size_t			       transition_size;
	enum transfer_state	       state;

	M0_PRE(ctx != NULL);
	M0_PRE(bs_index < ctx->nbc_bs_nr);
	M0_PRE(ctx->nbc_bs != NULL);

	if (ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		transition	= success ?
				  node_bulk_client_success :
				  node_bulk_client_failure;
		transition_size = success ?
				  ARRAY_SIZE(node_bulk_client_success) :
				  ARRAY_SIZE(node_bulk_client_failure);
	} else if (ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_SERVER) {
		transition	= success ?
				  node_bulk_server_success :
				  node_bulk_server_failure;
		transition_size = success ?
				  ARRAY_SIZE(node_bulk_server_success) :
				  ARRAY_SIZE(node_bulk_server_failure);
	} else {
		transition	= NULL;
		transition_size = 0;
		M0_IMPOSSIBLE("Invalid node role in net-test bulk testing");
	}
	state = node_bulk_state_search(ctx->nbc_bs[bs_index].bsb_ts,
				       transition, transition_size);
	node_bulk_state_change(ctx, bs_index, state);
}

void node_bulk_state_transition_auto(struct node_bulk_ctx *ctx,
				     size_t bs_index)
{
	struct buf_status_bulk *bs;
	bool			role_client;
	m0_time_t		rtt;

	M0_PRE(ctx != NULL);
	M0_PRE(bs_index < ctx->nbc_bs_nr);

	role_client = ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT;
	bs = &ctx->nbc_bs[bs_index];
	/* Check final states */
	M0_ASSERT(M0_IN(bs->bsb_ts, (TS_TRANSFERRED, TS_FAILED)) ||
		  (!role_client && bs->bsb_ts == TS_BADMSG));
	switch (bs->bsb_ts) {
	case TS_TRANSFERRED:
		rtt = m0_time_sub(bs->bsb_time_finish, bs->bsb_time_start);
		sd_update(ctx, MT_TRANSFER, MS_SUCCESS, MD_BOTH);
		m0_net_test_nh_sd_update_rtt(&ctx->nbc_nh, rtt);
		break;
	case TS_FAILED:
		sd_update(ctx, MT_TRANSFER, MS_FAILED, MD_BOTH);
		break;
	case TS_BADMSG:
		sd_update(ctx, MT_MSG, MS_BAD, MD_RECV);
		break;
	default:
		M0_IMPOSSIBLE("Impossible final state in "
			      "net-test bulk testing");
	}
	node_bulk_state_change(ctx, bs_index, TS_UNUSED);
}

void node_bulk_state_transition_auto_all(struct node_bulk_ctx *ctx)
{
	size_t bs_index;
	size_t i;
	size_t nr;

	M0_PRE(ctx != NULL);

	nr = m0_net_test_ringbuf_nr(&ctx->nbc_rb_bulk_final);
	for (i = 0; i < nr; ++i) {
		bs_index = m0_net_test_ringbuf_pop(&ctx->nbc_rb_bulk_final);
		node_bulk_state_transition_auto(ctx, bs_index);
	}
	M0_POST(m0_net_test_ringbuf_is_empty(&ctx->nbc_rb_bulk_final));
}

static void server_process_unused_ping(struct node_bulk_ctx *ctx)
{
	size_t index;
	size_t i;
	size_t nr;
	int    rc;

	M0_PRE(ctx != NULL);

	nr = m0_net_test_ringbuf_nr(&ctx->nbc_rb_ping_unused);
	for (i = 0; i < nr; ++i) {
		index = m0_net_test_ringbuf_pop(&ctx->nbc_rb_ping_unused);
		rc = m0_net_test_network_msg_recv(&ctx->nbc_net, index);
		if (rc != 0) {
			sd_update(ctx, MT_MSG, MS_FAILED, MD_RECV);
			m0_net_test_ringbuf_push(&ctx->nbc_rb_ping_unused,
						 index);
		}
	}
}

static struct m0_net_buffer *net_buf_bulk_get(struct node_bulk_ctx *ctx,
					      size_t buf_bulk_index)
{
	return m0_net_test_network_buf(&ctx->nbc_net, M0_NET_TEST_BUF_BULK,
				       buf_bulk_index);
}

static void buf_desc_set0(struct node_bulk_ctx *ctx,
			  size_t buf_bulk_index)
{
	M0_PRE(ctx != NULL);
	M0_PRE(buf_bulk_index < ctx->nbc_buf_bulk_nr);

	M0_SET0(&net_buf_bulk_get(ctx, buf_bulk_index)->nb_desc);
	M0_SET0(&ctx->nbc_bs[buf_bulk_index].bsb_desc_send);
}

/**
 * Swap contents of network buf descriptors for bulk network buffer
 * buf_index and buf_status_bulk.bsb_desc_send for this buffer.
 */
static void buf_desc_swap(struct node_bulk_ctx *ctx,
			  size_t buf_bulk_index)
{
	M0_PRE(ctx != NULL);
	M0_PRE(buf_bulk_index < ctx->nbc_buf_bulk_nr);

	M0_SWAP(net_buf_bulk_get(ctx, buf_bulk_index)->nb_desc,
		ctx->nbc_bs[buf_bulk_index].bsb_desc_send);
}

static void buf_desc_server_free(struct node_bulk_ctx *ctx,
				 size_t buf_bulk_index)
{
	M0_PRE(ctx != NULL);
	M0_PRE(buf_bulk_index < ctx->nbc_buf_bulk_nr);

	m0_net_desc_free(&net_buf_bulk_get(ctx, buf_bulk_index)->nb_desc);
	m0_net_desc_free(&ctx->nbc_bs[buf_bulk_index].bsb_desc_send);
}

static void buf_desc_client_free(struct node_bulk_ctx *ctx,
				 size_t bs_index)
{
	M0_PRE(ctx != NULL);
	M0_PRE(bs_index * 2 + 1 < ctx->nbc_buf_bulk_nr);

	m0_net_desc_free(&net_buf_bulk_get(ctx, bs_index * 2)->nb_desc);
	m0_net_desc_free(&net_buf_bulk_get(ctx, bs_index * 2 + 1)->nb_desc);
}

static m0_bcount_t buf_desc_deserialize(struct node_bulk_ctx *ctx,
					size_t buf_bulk_index,
					size_t buf_ping_index,
					m0_bcount_t offset)
{
	m0_bcount_t len;
	m0_bcount_t len_total;

	M0_PRE(ctx != NULL);
	M0_PRE(buf_ping_index < ctx->nbc_buf_ping_nr);
	M0_PRE(buf_bulk_index < ctx->nbc_buf_bulk_nr);

	buf_desc_set0(ctx, buf_bulk_index);
	/* decode network buffer descriptors for active bulk receiving */
	len = m0_net_test_network_bd_serialize(M0_NET_TEST_DESERIALIZE,
					       &ctx->nbc_net, buf_bulk_index,
					       buf_ping_index, offset);
	if (len == 0)
		return 0;
	len_total = net_test_len_accumulate(0, len);

	/*
	 * buf->nb_desc = zero descriptor
	 * bs->bsb_desc_send = descriptor for active bulk receiving
	 */
	buf_desc_swap(ctx, buf_bulk_index);

	len = m0_net_test_network_bd_serialize(M0_NET_TEST_DESERIALIZE,
					       &ctx->nbc_net, buf_bulk_index,
					       buf_ping_index,
					       offset + len_total);
	if (len == 0) {
		/* free already allocated network descriptor */
		buf_desc_server_free(ctx, buf_bulk_index);
		return 0;
	}
	len_total = net_test_len_accumulate(len_total, len);

	/*
	 * buf->nb_desc = descriptor for active bulk receiving
	 * bs->bsb_desc_send = descriptor for active bulk sending
	 */
	buf_desc_swap(ctx, buf_bulk_index);

	return len_total;
}

/**
 * Start network transfer. Take two network buffer descriptors from
 * buf_ping_index in ctx->nbc_net with given offset in buffer.
 * @return number of bytes read.
 * @return 0 if no free bulk buffer found or deserializing failed.
 */
static m0_bcount_t node_bulk_server_transfer_start(struct node_bulk_ctx *ctx,
						   size_t buf_ping_index,
						   m0_bcount_t offset)
{
	m0_bcount_t len;
	size_t	    buf_bulk_index;
	bool	    no_unused_buf;
	int	    rc;

	no_unused_buf = m0_net_test_ringbuf_is_empty(&ctx->nbc_rb_bulk_unused);
	if (no_unused_buf) {
		LOGD("--- NO UNUSED BUFS");
		sd_update(ctx, MT_TRANSFER, MS_FAILED, MD_BOTH);
		return 0;
	}

	/* get unused buf */
	buf_bulk_index = m0_net_test_ringbuf_pop(&ctx->nbc_rb_bulk_unused);
	M0_ASSERT(buf_bulk_index < ctx->nbc_buf_bulk_nr);
	node_bulk_state_change(ctx, buf_bulk_index, TS_BD_RECEIVED);
	/* deserialize network buffer descriptors */
	len = buf_desc_deserialize(ctx, buf_bulk_index, buf_ping_index, offset);
	if (len == 0) {
		LOGD("BADMSG: buf_bulk_index = %lu, "
		     "buf_ping_index = %lu, offset = %lu",
		     buf_bulk_index, buf_ping_index, (unsigned long) offset);
		/* ping buffer contains invalid data */
		node_bulk_state_change(ctx, buf_bulk_index, TS_BADMSG);
		return 0;
	}
	node_bulk_state_change(ctx, buf_bulk_index, TS_RECEIVING);
	/* start active bulk receiving */
	rc = m0_net_test_network_bulk_enqueue(&ctx->nbc_net, buf_bulk_index, 0,
					      M0_NET_QT_ACTIVE_BULK_RECV);
	ctx->nbc_bs[buf_bulk_index].bsb_recv.bse_func_rc = rc;
	if (rc != 0) {
		/*
		 * Addition buffer to network queue failed.
		 * Free allocated (when deserialized) network descriptors.
		 */
		node_bulk_state_change(ctx, buf_bulk_index, TS_FAILED);
		buf_desc_server_free(ctx, buf_bulk_index);
		sd_update(ctx, MT_BULK, MS_FAILED, MD_RECV);
	}
	return rc == 0 ? len : 0;
}

static void node_bulk_cb_server(struct node_bulk_ctx *ctx,
				size_t buf_index,
				enum m0_net_queue_type q,
				const struct m0_net_buffer_event *ev)
{
	m0_bcount_t offset;
	m0_bcount_t len;
	size_t	    nr;
	size_t	    i;
	int	    rc;

	M0_PRE(ctx != NULL);
	M0_PRE(ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_SERVER);
	M0_PRE(ergo(q == M0_NET_QT_MSG_RECV, buf_index < ctx->nbc_buf_ping_nr));
	M0_PRE(ergo(M0_IN(q, (M0_NET_QT_ACTIVE_BULK_RECV,
			      M0_NET_QT_ACTIVE_BULK_SEND)),
		    buf_index < ctx->nbc_buf_bulk_nr));

	if (q == M0_NET_QT_MSG_RECV) {
		if (ev->nbe_status != 0)
			return;
		nr = m0_net_test_network_bd_nr(&ctx->nbc_net, buf_index);
		if (nr % 2 != 0) {
			LOGD("MS_BAD: nr = %lu", nr);
			sd_update(ctx, MT_MSG, MS_BAD, MD_RECV);
			return;
		}
		nr /= 2;
		offset = 0;
		for (i = 0; i < nr; ++i) {
			len = node_bulk_server_transfer_start(ctx, buf_index,
							      offset);
			offset += len;
		}
	} else if (q == M0_NET_QT_ACTIVE_BULK_RECV) {
		if (ev->nbe_status != 0) {
			LOGD("--- active bulk recv FAILED!");
			buf_desc_server_free(ctx, buf_index);
			return;
		}
		/*
		 * Don't free m0_net_buf_desc here to avoid
		 * memory allocator delays.
		 */
		buf_desc_swap(ctx, buf_index);
		rc = m0_net_test_network_bulk_enqueue(&ctx->nbc_net,
						      buf_index, 0,
						M0_NET_QT_ACTIVE_BULK_SEND);
		ctx->nbc_bs[buf_index].bsb_send.bse_func_rc = rc;
		if (rc != 0) {
			LOGD("--- active bulk send FAILED!");
			buf_desc_server_free(ctx, buf_index);
			node_bulk_state_change(ctx, buf_index, TS_FAILED);
			sd_update(ctx, MT_BULK, MS_FAILED, MD_SEND);
		}
	} else if (q == M0_NET_QT_ACTIVE_BULK_SEND) {
		buf_desc_server_free(ctx, buf_index);
	}
}

static void node_bulk_cb_client(struct node_bulk_ctx *ctx,
				size_t buf_index,
				enum m0_net_queue_type q,
				const struct m0_net_buffer_event *ev)
{
	struct buf_status_bulk *bs;

	M0_PRE(ctx != NULL);
	M0_PRE(ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT);
	M0_PRE(ergo(q == M0_NET_QT_MSG_SEND, buf_index < ctx->nbc_buf_ping_nr));
	M0_PRE(ergo(M0_IN(q, (M0_NET_QT_PASSIVE_BULK_RECV,
			      M0_NET_QT_PASSIVE_BULK_SEND)),
		    buf_index < ctx->nbc_buf_bulk_nr));

	if (q == M0_NET_QT_MSG_SEND) {
		/*
		 * Change state for every bulk buffer, which
		 * descriptor is stored in current message.
		 */
		m0_tl_teardown(bsb, &ctx->nbc_bsp[buf_index].bsp_buffers, bs) {
			bs->bsb_msg.bse_cb_rc = ev->nbe_status;
			node_bulk_state_change_cb(ctx, bs->bsb_index,
						  ev->nbe_status == 0);
		}
	} else if (M0_IN(q, (M0_NET_QT_PASSIVE_BULK_RECV,
			     M0_NET_QT_PASSIVE_BULK_SEND))) {
		bs = &ctx->nbc_bs[buf_index / 2];
		if (node_bulk_state_is_final(bs->bsb_ts))
			buf_desc_client_free(ctx, buf_index / 2);
	}
}

static bool node_bulk_is_stopping(struct node_bulk_ctx *ctx)
{
	return m0_atomic64_get(&ctx->nbc_stop_flag) == 1;
}

static void node_bulk_cb(struct m0_net_test_network_ctx *net_ctx,
			 const uint32_t buf_index,
			 enum m0_net_queue_type q,
			 const struct m0_net_buffer_event *ev)
{
	struct buf_status_bulk	*bs;
	size_t			 bs_index;
	struct buf_status_errno *bs_e;
	struct node_bulk_ctx	*ctx = node_bulk_ctx_from_net_ctx(net_ctx);
	bool			 role_client;
	bool			 buf_send;
	bool			 buf_bulk;

	LOGD("node_bulk_cb: tm_addr = %s, buf_index = %u, q = %d"
	     ", ev-nbe_status = %d",
	     net_ctx->ntc_tm->ntm_ep->nep_addr, buf_index, q, ev->nbe_status);
	M0_PRE(net_ctx != NULL);
	role_client = ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT;
	M0_PRE(ergo(q == M0_NET_QT_MSG_RECV, !role_client));
	M0_PRE(ergo(q == M0_NET_QT_MSG_SEND, role_client));
	M0_PRE(ergo(q == M0_NET_QT_PASSIVE_BULK_RECV, role_client));
	M0_PRE(ergo(q == M0_NET_QT_PASSIVE_BULK_SEND, role_client));
	M0_PRE(ergo(q == M0_NET_QT_ACTIVE_BULK_RECV, !role_client));
	M0_PRE(ergo(q == M0_NET_QT_ACTIVE_BULK_SEND, !role_client));
	M0_PRE(ergo(q == M0_NET_QT_MSG_RECV || q == M0_NET_QT_MSG_SEND,
		    buf_index < ctx->nbc_buf_ping_nr));

	if (ev->nbe_status != 0 && ev->nbe_status != -ECANCELED) {
		LOGD("--CALLBACK ERROR! errno = %d", ev->nbe_status);
		LOGD("node_bulk_cb: tm_addr = %s, buf_index = %u, q = %d"
		     ", ev-nbe_status = %d",
		     net_ctx->ntc_tm->ntm_ep->nep_addr, buf_index, q,
		     ev->nbe_status);
	}

	ctx->nbc_callback_executed = true;
	buf_bulk = false;
	if (M0_IN(q,
		  (M0_NET_QT_PASSIVE_BULK_RECV, M0_NET_QT_ACTIVE_BULK_RECV,
		   M0_NET_QT_PASSIVE_BULK_SEND, M0_NET_QT_ACTIVE_BULK_SEND))) {
		buf_bulk = true;
		bs_index = role_client ? buf_index / 2 : buf_index;
		M0_ASSERT(bs_index < ctx->nbc_bs_nr);
		bs = &ctx->nbc_bs[bs_index];
		bs_e = q == M0_NET_QT_PASSIVE_BULK_RECV ||
		       q == M0_NET_QT_ACTIVE_BULK_RECV ? &bs->bsb_recv :
		       q == M0_NET_QT_PASSIVE_BULK_SEND ||
		       q == M0_NET_QT_ACTIVE_BULK_SEND ? &bs->bsb_send : NULL;
		M0_ASSERT(bs_e != NULL);
		bs_e->bse_cb_rc = ev->nbe_status;
		node_bulk_state_change_cb(ctx, bs_index, ev->nbe_status == 0);
	}
	(role_client ? node_bulk_cb_client : node_bulk_cb_server)
		(ctx, buf_index, q, ev);
	if (M0_IN(q, (M0_NET_QT_MSG_SEND, M0_NET_QT_MSG_RECV))) {
		/* ping buffer can be reused now */
		m0_net_test_ringbuf_push(&ctx->nbc_rb_ping_unused, buf_index);
	}
	if (!role_client && q == M0_NET_QT_MSG_RECV) {
		if (!node_bulk_is_stopping(ctx))
			server_process_unused_ping(ctx);
	}
	/* update stats */
	buf_send = q == M0_NET_QT_PASSIVE_BULK_SEND ||
		   q == M0_NET_QT_ACTIVE_BULK_SEND || q == M0_NET_QT_MSG_SEND;
	sd_update(ctx, buf_bulk ? MT_BULK : MT_MSG,
		  ev->nbe_status == 0 ? MS_SUCCESS : MS_FAILED,
		  buf_send ? MD_SEND : MD_RECV);
	/* state transitions from final states */
	node_bulk_state_transition_auto_all(ctx);
}

static struct m0_net_test_network_buffer_callbacks node_bulk_buf_cb = {
	.ntnbc_cb = {
		[M0_NET_QT_MSG_RECV]		= node_bulk_cb,
		[M0_NET_QT_MSG_SEND]		= node_bulk_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV]	= node_bulk_cb,
		[M0_NET_QT_PASSIVE_BULK_SEND]	= node_bulk_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]	= node_bulk_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]	= node_bulk_cb,
	}
};

/** Get server index for a given bs_index */
static size_t client_server_index(struct node_bulk_ctx *ctx, size_t bs_index)
{
	M0_PRE(ctx != NULL);
	M0_PRE(ctx->nbc_client_concurrency > 0);

	return bs_index / ctx->nbc_client_concurrency;
}

/** Enqueue passive send/recv buffers for the test client */
static int client_bulk_enqueue(struct node_bulk_ctx *ctx,
			       size_t buf_index)
{
	enum m0_net_queue_type q;
	size_t		       server_index;

	q = buf_index % 2 == 0 ? M0_NET_QT_PASSIVE_BULK_SEND :
				 M0_NET_QT_PASSIVE_BULK_RECV;
	server_index = client_server_index(ctx, buf_index / 2);
	return m0_net_test_network_bulk_enqueue(&ctx->nbc_net, buf_index,
						server_index, q);
}

static void client_bulk_dequeue(struct node_bulk_ctx *ctx,
				size_t buf_index)
{
	m0_net_test_network_buffer_dequeue(&ctx->nbc_net, M0_NET_TEST_BUF_BULK,
					   buf_index);
}

static void client_transfer_start(struct node_bulk_ctx *ctx,
				  size_t bs_index)
{
	struct buf_status_bulk *bs;
	int			rc;

	M0_PRE(ctx != NULL);
	M0_PRE(bs_index < ctx->nbc_bs_nr);

	bs = &ctx->nbc_bs[bs_index];
	node_bulk_state_change(ctx, bs_index, TS_QUEUED);
	rc = client_bulk_enqueue(ctx, bs_index * 2);
	bs->bsb_send.bse_func_rc = rc;
	if (rc != 0) {
		node_bulk_state_change(ctx, bs_index, TS_FAILED);
		sd_update(ctx, MT_BULK, MS_FAILED, MD_SEND);
		return;
	}
	rc = client_bulk_enqueue(ctx, bs_index * 2 + 1);
	bs->bsb_recv.bse_func_rc = rc;
	if (rc != 0) {
		node_bulk_state_change(ctx, bs_index, TS_FAILED1);
		client_bulk_dequeue(ctx, bs_index * 2);
		sd_update(ctx, MT_BULK, MS_FAILED, MD_RECV);
		return;
	}
	m0_net_test_ringbuf_push(&ctx->nbc_rb_bulk_queued, bs_index);
}

static void client_process_unused_bulk(struct node_bulk_ctx *ctx)
{
	size_t i;
	size_t nr;
	size_t bs_index;
	bool   transfer_next;

	M0_PRE(ctx != NULL);

	nr = m0_net_test_ringbuf_nr(&ctx->nbc_rb_bulk_unused);
	for (i = 0; i < nr; ++i) {
		/* Check stop conditions */
		transfer_next = m0_net_test_nh_transfer_next(&ctx->nbc_nh);
		/*
		LOGD("client: transfer_next = %s",
		       transfer_next ? (char *) "true" : (char *) "false");
		*/
		if (!transfer_next)
			break;
		/* Start next transfer */
		bs_index = m0_net_test_ringbuf_pop(&ctx->nbc_rb_bulk_unused);
		/*
		LOGD("client: next transfer bs_index = %lu",
		       bs_index);
		*/
		client_transfer_start(ctx, bs_index);
	}
}

static m0_bcount_t client_bds_serialize2(struct m0_net_test_network_ctx *net,
					 size_t bsb_index,
					 size_t msg_buf_index,
					 m0_bcount_t offset)
{
	m0_bcount_t len;
	m0_bcount_t len_total;

	len = m0_net_test_network_bd_serialize(M0_NET_TEST_SERIALIZE,
					       net, bsb_index * 2,
					       msg_buf_index, offset);
	if (len == 0)
		return 0;
	len_total = net_test_len_accumulate(0, len);

	len = m0_net_test_network_bd_serialize(M0_NET_TEST_SERIALIZE,
					       net, bsb_index * 2 + 1,
					       msg_buf_index,
					       offset + len_total);
	if (len == 0) {
		/*
		 * If first buffer descriptor serializing succeed,
		 * then number of serialized network buffer descriptors
		 * is increased. But if second serializing fails, then
		 * number of network buffer descriptors inside ping
		 * buffer should be decreased because these two
		 * descriptors should be added both or should not be
		 * added at all.
		 */
		m0_net_test_network_bd_nr_dec(net, msg_buf_index);
	}
	len_total = net_test_len_accumulate(len_total, len);
	return len_total;
}


static void client_bulk_bufs_dequeue(struct node_bulk_ctx *ctx,
				     struct buf_status_bulk *bs)
{
	client_bulk_dequeue(ctx, bs->bsb_index * 2);
	client_bulk_dequeue(ctx, bs->bsb_index * 2 + 1);
}

static void client_bds_send(struct node_bulk_ctx *ctx,
			    struct server_status_bulk *ss)
{
	struct m0_net_test_ringbuf *rb_ping = &ctx->nbc_rb_ping_unused;
	struct buf_status_bulk	   *bs;
	/* Message buffer was taken from unused list */
	bool			   msg_taken;
	/* Message buffer index, makes sense iff (msg_taken) */
	size_t			   msg_index = 0;
	/* Number of buffer descriptors in selected message buffer */
	size_t			   msg_bd_nr = 0;
	struct buf_status_ping	  *msg_bs = 0;
	m0_bcount_t		   msg_offset = 0;
	m0_bcount_t		   len;
	bool			   buf_last;
	int			   rc;
	struct m0_tl		   messages;
	bool			   list_is_empty;

	M0_PRE(ctx != NULL);
	M0_PRE(ss != NULL);
	M0_PRE(ctx->nbc_bd_nr_max > 0 && ctx->nbc_bd_nr_max % 2 == 0);

	bsp_tlist_init(&messages);
	msg_taken = false;
	m0_tl_for(bsb, &ss->ssb_buffers, bs) {
take_msg:
		if (!msg_taken && m0_net_test_ringbuf_is_empty(rb_ping)) {
			/*
			 * No free message to transfer bulk buffer
			 * network descriptors. Cancel tranfers.
			 */
			node_bulk_state_change(ctx, bs->bsb_index, TS_FAILED2);
			client_bulk_bufs_dequeue(ctx, bs);
			sd_update(ctx, MT_MSG, MS_FAILED, MD_SEND);
			bsb_tlist_del(bs);
			continue;
		}
		/* Take unused msg buf number if it wasn't taken before */
		if (!msg_taken) {
			msg_index     = m0_net_test_ringbuf_pop(rb_ping);
			msg_taken     = true;
			msg_bd_nr     = 0;
			msg_offset    = 0;
			msg_bs	      = &ctx->nbc_bsp[msg_index];
			list_is_empty =
				bsb_tlist_is_empty(&msg_bs->bsp_buffers);
			M0_ASSERT(list_is_empty);
		}
		/* Try to serialize two buffer descriptors */
		len = client_bds_serialize2(&ctx->nbc_net, bs->bsb_index,
					    msg_index, msg_offset);
		/*
		LOGD("msg_index = %lu, len = %lu, msg_offset = %lu",
		       (unsigned long ) msg_index, (unsigned long ) len,
		       (unsigned long ) msg_offset);
		*/
		msg_offset += len;
		if (len == 0) {
			if (msg_bd_nr > 0) {
				/* No free space in ping buffer */
				bsp_tlist_add_tail(&messages, msg_bs);
				msg_taken = false;
				goto take_msg;
			} else {
				/*
				 * Serializing failed for unknown reason
				 * (or ping buffer is smaller than
				 * size of two serialized bulk
				 * network buffer descriptors)
				 */
				node_bulk_state_change(ctx, bs->bsb_index,
						       TS_FAILED2);
				client_bulk_bufs_dequeue(ctx, bs);
				bsb_tlist_del(bs);
				msg_taken = false;
			}
		} else {
			buf_last = bsb_tlist_next(&ss->ssb_buffers, bs) == NULL;
			bsb_tlist_del(bs);
			bsb_tlist_add_tail(&msg_bs->bsp_buffers, bs);
			msg_bd_nr += 2;
			if (msg_bd_nr == ctx->nbc_bd_nr_max || buf_last) {
				bsp_tlist_add_tail(&messages, msg_bs);
				msg_taken = false;
			}
		}
	} m0_tl_endfor;
	M0_ASSERT(!msg_taken);
	m0_tl_for(bsp, &messages, msg_bs) {
		list_is_empty = bsb_tlist_is_empty(&msg_bs->bsp_buffers);
		M0_ASSERT(!list_is_empty);
		/*
		 * Change state to BD_SENT for every bulk buffer, which
		 * descriptor is stored in current message.
		 */
		m0_tl_for(bsb, &msg_bs->bsp_buffers, bs) {
			node_bulk_state_change(ctx, bs->bsb_index, TS_BD_SENT);
		} m0_tl_endfor;
		rc = m0_net_test_network_msg_send(&ctx->nbc_net,
						  msg_bs->bsp_index,
						  ss->ssb_index);
		if (rc != 0) {
			LOGD("--- msg send FAILED!");
			sd_update(ctx, MT_MSG, MS_FAILED, MD_SEND);
		}
		/* Save rc for future analysis */
		m0_tl_for(bsb, &msg_bs->bsp_buffers, bs) {
			bs->bsb_msg.bse_func_rc = rc;
			/* Change state if msg sending failed */
			if (rc != 0) {
				node_bulk_state_change(ctx, bs->bsb_index,
						       TS_FAILED2);
				client_bulk_bufs_dequeue(ctx, bs);
				bsb_tlist_del(bs);
			}
		} m0_tl_endfor;
		bsp_tlist_del(msg_bs);
	} m0_tl_endfor;
	bsp_tlist_fini(&messages);
}

static void client_process_queued_bulk(struct node_bulk_ctx *ctx)
{
	struct server_status_bulk *ss;
	struct buf_status_bulk	  *bs;
	struct m0_tl		   servers;
	size_t			   index;
	size_t			   i;
	size_t			   nr;

	M0_PRE(ctx != NULL);

	ssb_tlist_init(&servers);
	nr = m0_net_test_ringbuf_nr(&ctx->nbc_rb_bulk_queued);
	/* Add queued buffer to per server list of queued buffers */
	for (i = 0; i < nr; ++i) {
		index = m0_net_test_ringbuf_pop(&ctx->nbc_rb_bulk_queued);
		bs = &ctx->nbc_bs[index];
		ss = &ctx->nbc_sstatus[client_server_index(ctx, index)];
		bsb_tlist_add_tail(&ss->ssb_buffers, bs);
		if (!ssb_tlink_is_in(ss))
			ssb_tlist_add_tail(&servers, ss);
	}
	/* Send message with buffer descriptors to every server */
	m0_tl_teardown(ssb, &servers, ss) {
		client_bds_send(ctx, ss);
	}
	ssb_tlist_fini(&servers);
}

static void node_bulk_buf_unused(struct node_bulk_ctx *ctx)
{
	size_t i;

	M0_PRE(ctx != NULL);

	for (i = 0; i < ctx->nbc_bs_nr; ++i) {
		ctx->nbc_bs[i].bsb_ts = TS_UNUSED;
		m0_net_test_ringbuf_push(&ctx->nbc_rb_bulk_unused, i);
	}
	for (i = 0; i < ctx->nbc_buf_ping_nr; ++i)
		m0_net_test_ringbuf_push(&ctx->nbc_rb_ping_unused, i);
}

static void node_bulk_buf_dequeue(struct node_bulk_ctx *ctx)
{
	size_t i;

	M0_PRE(ctx != NULL);
	for (i = 0; i < ctx->nbc_buf_ping_nr; ++i) {
		m0_net_test_network_buffer_dequeue(&ctx->nbc_net,
						   M0_NET_TEST_BUF_PING, i);
	}
	for (i = 0; i < ctx->nbc_buf_bulk_nr; ++i) {
		m0_net_test_network_buffer_dequeue(&ctx->nbc_net,
						   M0_NET_TEST_BUF_BULK, i);
	}
}

static bool node_bulk_bufs_unused_all(struct node_bulk_ctx *ctx)
{
	M0_PRE(ctx != NULL);

	return m0_net_test_ringbuf_nr(&ctx->nbc_rb_ping_unused) ==
	       ctx->nbc_buf_ping_nr &&
	       m0_net_test_ringbuf_nr(&ctx->nbc_rb_bulk_unused) ==
	       ctx->nbc_bs_nr;
}

static void node_bulk_worker(struct node_bulk_ctx *ctx)
{
	struct m0_clink tm_clink;
	struct m0_chan  tm_chan = {};
	struct m0_mutex tm_chan_mutex = {};
	bool		pending;
	bool		running;

	M0_PRE(ctx != NULL);

	/* all buffers are unused */
	node_bulk_buf_unused(ctx);
	/* attach tm_clink to clink group to wait for two signals at once */
	m0_clink_attach(&tm_clink, &ctx->nbc_stop_clink, NULL);
	/*
	 * Init wait channel and clink.
	 * Transfer machine will signal to this channel.
	 */
	m0_mutex_init(&tm_chan_mutex);
	m0_chan_init(&tm_chan, &tm_chan_mutex);
	m0_clink_add_lock(&tm_chan, &tm_clink);
	/* main loop */
	running = true;
	while (running || !node_bulk_bufs_unused_all(ctx)) {
		if (running) {
			if (ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
				client_process_unused_bulk(ctx);
				client_process_queued_bulk(ctx);
			} else {
				server_process_unused_ping(ctx);
			}
		}
		/* notification for buffer events */
		pending = m0_net_buffer_event_pending(ctx->nbc_net.ntc_tm);
		if (!pending) {
			m0_net_buffer_event_notify(ctx->nbc_net.ntc_tm,
						   &tm_chan);
		}
		ctx->nbc_callback_executed = false;
		/* execute network buffer callbacks in this thread context */
		m0_net_buffer_event_deliver_all(ctx->nbc_net.ntc_tm);
		M0_ASSERT(ergo(pending, ctx->nbc_callback_executed));
		/* state transitions from final states */
		node_bulk_state_transition_auto_all(ctx);
		/* update copy of statistics */
		m0_net_test_nh_sd_copy_locked(&ctx->nbc_nh);
		/* wait for STOP command or buffer event */
		if (!ctx->nbc_callback_executed)
			m0_chan_wait(&ctx->nbc_stop_clink);
		if (running && node_bulk_is_stopping(ctx)) {
			/* dequeue all queued network buffers */
			node_bulk_buf_dequeue(ctx);
			running = false;
		}
	}
	/* transfer machine should't signal to tm_chan */
	m0_net_buffer_event_notify(ctx->nbc_net.ntc_tm, NULL);
	m0_clink_del_lock(&tm_clink);
	m0_clink_fini(&tm_clink);
	m0_chan_fini_lock(&tm_chan);
	m0_mutex_fini(&tm_chan_mutex);
}

static int node_bulk_test_init_fini(struct node_bulk_ctx *ctx,
				    const struct m0_net_test_cmd *cmd)
{
	struct m0_net_test_network_timeouts *timeouts;
	struct m0_net_test_network_cfg	     net_cfg;
	const struct m0_net_test_cmd_init   *icmd;
	struct server_status_bulk	    *ss;
	struct buf_status_ping		    *msg_bs;
	int				     rc;
	size_t				     i;
	bool				     role_client;
	m0_time_t			     to_send;
	m0_time_t			     to_bulk;
	size_t				     nr;

	M0_PRE(ctx != NULL);

	if (cmd == NULL)
		goto fini;
	icmd	    = &cmd->ntc_init;
	role_client = icmd->ntci_role == M0_NET_TEST_ROLE_CLIENT;

	rc = -ENOMEM;
	M0_ALLOC_ARR(ctx->nbc_bs, ctx->nbc_bs_nr);
	if (ctx->nbc_bs == NULL)
		goto fail;
	for (i = 0; i < ctx->nbc_bs_nr; ++i) {
		ctx->nbc_bs[i].bsb_index = i;
		bsb_tlink_init(&ctx->nbc_bs[i]);
		M0_SET0(&ctx->nbc_bs[i].bsb_desc_send);
	}

	if (role_client) {
		M0_ALLOC_ARR(ctx->nbc_sstatus, icmd->ntci_ep.ntsl_nr);
		if (ctx->nbc_sstatus == NULL)
			goto free_bs_bulk;
		for (i = 0; i < icmd->ntci_ep.ntsl_nr; ++i) {
			ss = &ctx->nbc_sstatus[i];
			ss->ssb_index = i;
			bsb_tlist_init(&ss->ssb_buffers);
			ssb_tlink_init(ss);
		}
		M0_ALLOC_ARR(ctx->nbc_bsp, ctx->nbc_buf_ping_nr);
		if (ctx->nbc_bsp == NULL)
			goto free_sstatus;
		for (i = 0; i < ctx->nbc_buf_ping_nr; ++i) {
			msg_bs = &ctx->nbc_bsp[i];
			msg_bs->bsp_index = i;
			bsb_tlist_init(&msg_bs->bsp_buffers);
			bsp_tlink_init(msg_bs);
		}
	}

	M0_ASSERT(equi(role_client, ctx->nbc_sstatus != NULL));

	rc = m0_net_test_ringbuf_init(&ctx->nbc_rb_ping_unused,
				      ctx->nbc_buf_ping_nr);
	if (rc != 0)
		goto free_bsp;
	rc = m0_net_test_ringbuf_init(&ctx->nbc_rb_bulk_unused,
				      ctx->nbc_bs_nr);
	if (rc != 0)
		goto free_rb_ping_unused;
	rc = m0_net_test_ringbuf_init(&ctx->nbc_rb_bulk_queued,
				      ctx->nbc_bs_nr);
	if (rc != 0)
		goto free_rb_bulk_unused;
	rc = m0_net_test_ringbuf_init(&ctx->nbc_rb_bulk_final,
				      ctx->nbc_bs_nr);
	if (rc != 0)
		goto free_rb_bulk_queued;

	M0_SET0(&net_cfg);
	net_cfg.ntncfg_tm_cb	     = node_bulk_tm_cb;
	net_cfg.ntncfg_buf_cb	     = node_bulk_buf_cb;
	net_cfg.ntncfg_buf_size_ping = ctx->nbc_buf_size_ping,
	net_cfg.ntncfg_buf_ping_nr   = ctx->nbc_buf_ping_nr,
	net_cfg.ntncfg_buf_size_bulk = ctx->nbc_buf_size_bulk,
	net_cfg.ntncfg_buf_bulk_nr   = ctx->nbc_buf_bulk_nr,
	net_cfg.ntncfg_ep_max	     = icmd->ntci_ep.ntsl_nr,
	net_cfg.ntncfg_timeouts	     = m0_net_test_network_timeouts_never();
	net_cfg.ntncfg_sync	     = true;
	/* configure timeouts */
	to_send  = icmd->ntci_buf_send_timeout;
	to_bulk  = icmd->ntci_buf_bulk_timeout;
	timeouts = &net_cfg.ntncfg_timeouts;
	timeouts->ntnt_timeout[M0_NET_QT_MSG_SEND]	    = to_send;
	timeouts->ntnt_timeout[M0_NET_QT_PASSIVE_BULK_RECV] = to_bulk;
	timeouts->ntnt_timeout[M0_NET_QT_PASSIVE_BULK_SEND] = to_bulk;
	timeouts->ntnt_timeout[M0_NET_QT_ACTIVE_BULK_RECV]  = to_bulk;
	timeouts->ntnt_timeout[M0_NET_QT_ACTIVE_BULK_SEND]  = to_bulk;

	rc = m0_net_test_network_ctx_init(&ctx->nbc_net, &net_cfg,
					  icmd->ntci_tm_ep);
	if (rc != 0)
		goto free_rb_bulk_final;
	rc = m0_net_test_network_ep_add_slist(&ctx->nbc_net, &icmd->ntci_ep);
	if (rc != 0)
		goto fini;
	m0_mutex_init(&ctx->nbc_stop_chan_mutex);
	m0_chan_init(&ctx->nbc_stop_chan, &ctx->nbc_stop_chan_mutex);
	m0_clink_init(&ctx->nbc_stop_clink, NULL);
	m0_clink_add_lock(&ctx->nbc_stop_chan, &ctx->nbc_stop_clink);
	goto success;
fini:
	icmd = NULL;
	rc = 0;
	m0_clink_del_lock(&ctx->nbc_stop_clink);
	m0_clink_fini(&ctx->nbc_stop_clink);
	m0_chan_fini_lock(&ctx->nbc_stop_chan);
	m0_mutex_fini(&ctx->nbc_stop_chan_mutex);
	m0_net_test_network_ctx_fini(&ctx->nbc_net);
free_rb_bulk_final:
	m0_net_test_ringbuf_fini(&ctx->nbc_rb_bulk_final);
free_rb_bulk_queued:
	m0_net_test_ringbuf_fini(&ctx->nbc_rb_bulk_queued);
free_rb_bulk_unused:
	m0_net_test_ringbuf_fini(&ctx->nbc_rb_bulk_unused);
free_rb_ping_unused:
	m0_net_test_ringbuf_fini(&ctx->nbc_rb_ping_unused);
free_bsp:
	if (ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		for (i = 0; i < ctx->nbc_buf_ping_nr; ++i) {
			msg_bs = &ctx->nbc_bsp[i];
			bsp_tlink_init(msg_bs);
			bsb_tlist_fini(&msg_bs->bsp_buffers);
		}
		m0_free(ctx->nbc_bsp);
	}
free_sstatus:
	if (ctx->nbc_nh.ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		nr = cmd != NULL ? icmd->ntci_ep.ntsl_nr :
				   ctx->nbc_net.ntc_ep_nr;
		for (i = 0; i < nr; ++i) {
			ss = &ctx->nbc_sstatus[i];
			ssb_tlink_fini(ss);
			bsb_tlist_fini(&ss->ssb_buffers);
		}
		m0_free(ctx->nbc_sstatus);
	}
free_bs_bulk:
	for (i = 0; i < ctx->nbc_bs_nr; ++i)
		bsb_tlink_fini(&ctx->nbc_bs[i]);
	m0_free(ctx->nbc_bs);
fail:
success:
	return rc;
}

static void *node_bulk_initfini(void *ctx_, struct m0_net_test_service *svc)
{
	struct node_bulk_ctx *ctx;
	int		      rc;

	M0_PRE(equi(ctx_ == NULL, svc != NULL));

	if (svc != NULL) {
		M0_ALLOC_PTR(ctx);
		if (ctx != NULL) {
			ctx->nbc_svc			  = svc;
			ctx->nbc_nh.ntnh_test_initialized = false;
		}
	} else {
		ctx = ctx_;
		rc = node_bulk_test_init_fini(ctx, NULL);
		M0_ASSERT(rc == 0);
		m0_free(ctx);
	}
	return svc != NULL ? ctx : NULL;
}

static void *node_bulk_init(struct m0_net_test_service *svc)
{
	return node_bulk_initfini(NULL, svc);
}

static void node_bulk_fini(void *ctx_)
{
	void *rc = node_bulk_initfini(ctx_, NULL);
	M0_POST(rc == NULL);
}

static int node_bulk_step(void *ctx_)
{
	return 0;
}

static int node_bulk_cmd_init(void *ctx_,
			      const struct m0_net_test_cmd *cmd,
			      struct m0_net_test_cmd *reply)
{
	const struct m0_net_test_cmd_init *icmd;
	struct node_bulk_ctx		  *ctx = ctx_;
	int				   rc;
	bool				   role_client;

	M0_PRE(ctx != NULL);
	M0_PRE(cmd != NULL);
	M0_PRE(reply != NULL);

	if (ctx->nbc_nh.ntnh_test_initialized) {
		rc = -EALREADY;
		goto reply;
	}
	icmd		       = &cmd->ntc_init;
	m0_net_test_nh_init(&ctx->nbc_nh, icmd);
	role_client	       = icmd->ntci_role == M0_NET_TEST_ROLE_CLIENT;
	ctx->nbc_buf_size_bulk = icmd->ntci_msg_size;
	ctx->nbc_buf_ping_nr   = icmd->ntci_bd_buf_nr;
	ctx->nbc_buf_size_ping = icmd->ntci_bd_buf_size;
	ctx->nbc_bd_nr_max     = icmd->ntci_bd_nr_max;
	ctx->nbc_bs_nr	       = icmd->ntci_msg_concurrency;
	ctx->nbc_buf_bulk_nr   = icmd->ntci_msg_concurrency;

	if (role_client) {
		ctx->nbc_client_concurrency  = icmd->ntci_msg_concurrency;
		ctx->nbc_buf_bulk_nr	    *= 2 * icmd->ntci_ep.ntsl_nr;
		ctx->nbc_bs_nr		     = ctx->nbc_buf_bulk_nr / 2;
	}

	/* do sanity check */
	rc = -EINVAL;
	if (!ergo(role_client, ctx->nbc_buf_bulk_nr % 2 == 0) ||
	    ctx->nbc_buf_bulk_nr < 1 || ctx->nbc_buf_size_bulk < 1 ||
	    ctx->nbc_buf_ping_nr < 1 || ctx->nbc_buf_size_ping < 1 ||
	    ctx->nbc_bd_nr_max < 1 || ctx->nbc_bs_nr < 1 ||
	    (ctx->nbc_nh.ntnh_role != M0_NET_TEST_ROLE_CLIENT &&
	     ctx->nbc_nh.ntnh_role != M0_NET_TEST_ROLE_SERVER) ||
	    !ergo(role_client, ctx->nbc_client_concurrency != 0) ||
	    !ergo(!role_client, ctx->nbc_bs_nr == ctx->nbc_buf_bulk_nr) ||
	    !ergo(role_client, 2 * ctx->nbc_bs_nr == ctx->nbc_buf_bulk_nr))
		goto reply;

	rc = node_bulk_test_init_fini(ctx, cmd);
reply:
	/* fill reply */
	reply->ntc_type = M0_NET_TEST_CMD_INIT_DONE;
	reply->ntc_done.ntcd_errno = rc;
	return rc;
}

/** @todo copy-paste from node_ping.c. refactor it. */
static int node_bulk_cmd_start(void *ctx_,
			       const struct m0_net_test_cmd *cmd,
			       struct m0_net_test_cmd *reply)
{
	struct m0_net_test_cmd_status_data *sd;
	int				    rc;
	struct node_bulk_ctx		   *ctx = ctx_;
	const m0_time_t			    _1s = M0_MKTIME(1, 0);

	M0_PRE(ctx != NULL);
	M0_PRE(cmd != NULL);
	M0_PRE(reply != NULL);

	sd = &ctx->nbc_nh.ntnh_sd;

	/** @todo copy-paste from node_ping.c */
	/* fill test start time */
	sd->ntcsd_time_start = m0_time_now();
	/* initialize stats */
	m0_net_test_mps_init(&sd->ntcsd_mps_send, 0, sd->ntcsd_time_start, _1s);
	m0_net_test_mps_init(&sd->ntcsd_mps_recv, 0, sd->ntcsd_time_start, _1s);
	m0_atomic64_set(&ctx->nbc_stop_flag, 0);
	rc = M0_THREAD_INIT(&ctx->nbc_thread, struct node_bulk_ctx *, NULL,
			    &node_bulk_worker, ctx, "net-test bulk");
	if (rc != 0) {
		/* change service state */
		m0_net_test_service_state_change(ctx->nbc_svc,
						 M0_NET_TEST_SERVICE_FAILED);
	}
	/* fill reply */
	reply->ntc_type = M0_NET_TEST_CMD_START_DONE;
	reply->ntc_done.ntcd_errno = rc;
	return rc;
}

/** @todo copy-paste from node_ping.c. refactor it. */
static int node_bulk_cmd_stop(void *ctx_,
			      const struct m0_net_test_cmd *cmd,
			      struct m0_net_test_cmd *reply)
{
	struct node_bulk_ctx *ctx = ctx_;
	int		      rc;

	M0_PRE(ctx != NULL);
	M0_PRE(cmd != NULL);
	M0_PRE(reply != NULL);

	if (!ctx->nbc_nh.ntnh_test_initialized) {
		reply->ntc_done.ntcd_errno = -EINVAL;
		goto reply;
	}
	/* stop worker thread */
	m0_atomic64_set(&ctx->nbc_stop_flag, 1);
	m0_chan_signal_lock(&ctx->nbc_stop_chan);
	rc = m0_thread_join(&ctx->nbc_thread);
	M0_ASSERT(rc == 0);
	m0_thread_fini(&ctx->nbc_thread);
	/* change service state */
	m0_net_test_service_state_change(ctx->nbc_svc,
					 M0_NET_TEST_SERVICE_FINISHED);
	/* fill reply */
	reply->ntc_done.ntcd_errno = 0;
reply:
	reply->ntc_type = M0_NET_TEST_CMD_STOP_DONE;
	return 0;
}

static int node_bulk_cmd_status(void *ctx_,
				const struct m0_net_test_cmd *cmd,
				struct m0_net_test_cmd *reply)
{
	struct node_bulk_ctx *ctx = ctx_;

	M0_PRE(ctx != NULL);

	m0_net_test_nh_cmd_status(&ctx->nbc_nh, cmd, reply);
	return 0;
}

static struct m0_net_test_service_cmd_handler node_bulk_cmd_handler[] = {
	{
		.ntsch_type    = M0_NET_TEST_CMD_INIT,
		.ntsch_handler = node_bulk_cmd_init,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_START,
		.ntsch_handler = node_bulk_cmd_start,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_STOP,
		.ntsch_handler = node_bulk_cmd_stop,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_STATUS,
		.ntsch_handler = node_bulk_cmd_status,
	},
};

struct m0_net_test_service_ops m0_net_test_node_bulk_ops = {
	.ntso_init	     = node_bulk_init,
	.ntso_fini	     = node_bulk_fini,
	.ntso_step	     = node_bulk_step,
	.ntso_cmd_handler    = node_bulk_cmd_handler,
	.ntso_cmd_handler_nr = ARRAY_SIZE(node_bulk_cmd_handler),
};

int m0_net_test_node_bulk_init(void)
{
	node_bulk_state_check_all();
	return 0;
}

void m0_net_test_node_bulk_fini(void)
{
}

#undef NET_TEST_MODULE_NAME

/** @} end of NetTestBulkNodeInternals group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
