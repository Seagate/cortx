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
 * Original creation date: 05/05/2012
 */

#include "lib/types.h"		/* m0_bcount_t */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/memory.h"		/* M0_ALLOC_ARR */
#include "lib/errno.h"		/* ENOMEM */
#include "lib/trace.h"		/* M0_LOG */

#include "net/test/serialize.h"	/* m0_net_test_serialize */
#include "net/test/str.h"	/* m0_net_test_str */

#include "net/test/commands.h"

#define NET_TEST_MODULE_NAME commands
#include "net/test/debug.h"	/* LOGD */


/**
   @defgroup NetTestCommandsInternals Commands
   @ingroup NetTestInternals

   @see
   @ref net-test

   @{
 */

/* m0_net_test_cmd_descr */
TYPE_DESCR(m0_net_test_cmd) = {
	FIELD_DESCR(struct m0_net_test_cmd, ntc_type),
};

/* m0_net_test_cmd_done_descr */
TYPE_DESCR(m0_net_test_cmd_done) = {
	FIELD_DESCR(struct m0_net_test_cmd_done, ntcd_errno),
};

/* m0_net_test_cmd_init_descr */
TYPE_DESCR(m0_net_test_cmd_init) = {
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_role),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_type),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_msg_nr),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_msg_size),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_bd_buf_nr),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_bd_buf_size),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_bd_nr_max),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_msg_concurrency),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_buf_send_timeout),
	FIELD_DESCR(struct m0_net_test_cmd_init, ntci_buf_bulk_timeout),
};

/* m0_net_test_msg_nr_descr */
TYPE_DESCR(m0_net_test_msg_nr) = {
	FIELD_DESCR(struct m0_net_test_msg_nr, ntmn_total),
	FIELD_DESCR(struct m0_net_test_msg_nr, ntmn_failed),
	FIELD_DESCR(struct m0_net_test_msg_nr, ntmn_bad),
};

/* m0_net_test_cmd_status_data_descr */
TYPE_DESCR(m0_net_test_cmd_status_data) = {
	FIELD_DESCR(struct m0_net_test_cmd_status_data, ntcsd_time_start),
	FIELD_DESCR(struct m0_net_test_cmd_status_data, ntcsd_time_finish),
	FIELD_DESCR(struct m0_net_test_cmd_status_data, ntcsd_time_now),
	FIELD_DESCR(struct m0_net_test_cmd_status_data, ntcsd_finished),
};

static m0_bcount_t
cmd_status_data_serialize(enum m0_net_test_serialize_op op,
			  struct m0_net_test_cmd_status_data *sd,
			  struct m0_bufvec *bv,
			  m0_bcount_t offset)
{
	m0_bcount_t			 len;
	m0_bcount_t			 len_total;
	int				 i;
	const struct m0_net_test_msg_nr *msg_nr[] = {
			&sd->ntcsd_msg_nr_send,
			&sd->ntcsd_msg_nr_recv,
			&sd->ntcsd_bulk_nr_send,
			&sd->ntcsd_bulk_nr_recv,
			&sd->ntcsd_transfers,
	};
	const struct m0_net_test_stats  *stats[] = {
			&sd->ntcsd_mps_send.ntmps_stats,
			&sd->ntcsd_mps_recv.ntmps_stats,
			&sd->ntcsd_rtt,
	};

	M0_PRE(sd != NULL);

	if (op == M0_NET_TEST_DESERIALIZE)
		M0_SET0(sd);

	len = m0_net_test_serialize(op, sd,
				    USE_TYPE_DESCR(m0_net_test_cmd_status_data),
				    bv, offset);
	len_total = net_test_len_accumulate(0, len);

	for (i = 0; i < ARRAY_SIZE(msg_nr) && len_total != 0; ++i) {
		len = m0_net_test_serialize(op, (void *) msg_nr[i],
					    USE_TYPE_DESCR(m0_net_test_msg_nr),
					    bv, offset + len_total);
		len_total = net_test_len_accumulate(len_total, len);
	}
	for (i = 0; i < ARRAY_SIZE(stats) && len_total != 0; ++i) {
		len = m0_net_test_stats_serialize(op, (void *) stats[i], bv,
						  offset + len_total);
		len_total = net_test_len_accumulate(len_total, len);
	}
	return len_total;
}

/**
   Serialize/deserialize m0_net_test_cmd to/from m0_net_buffer
   @param op operation. Can be M0_NET_TEST_SERIALIZE or M0_NET_TEST_DESERIALIZE
   @param cmd command for transforming.
   @param buf can be NULL if op == M0_NET_TEST_SERIALIZE,
	      in this case offset is ignored but length is set.
   @param offset start of serialized data in buf.
   @param length if isn't NULL then store length of serialized command here.
 */
static int cmd_serialize(enum m0_net_test_serialize_op op,
			 struct m0_net_test_cmd *cmd,
			 struct m0_net_buffer *buf,
			 m0_bcount_t offset,
			 m0_bcount_t *length)
{
	struct m0_bufvec *bv = buf == NULL ? NULL : &buf->nb_buffer;
	m0_bcount_t	  len;
	m0_bcount_t	  len_total;

	M0_PRE(cmd != NULL);
	len = m0_net_test_serialize(op, cmd, USE_TYPE_DESCR(m0_net_test_cmd),
				    bv, offset);
	if (len == 0)
		return -EINVAL;
	len_total = net_test_len_accumulate(0, len);

	switch (cmd->ntc_type) {
	case M0_NET_TEST_CMD_INIT:
		len = m0_net_test_serialize(op, &cmd->ntc_init,
					USE_TYPE_DESCR(m0_net_test_cmd_init),
					bv, offset + len_total);
		if (len == 0)
			break;
		len_total = net_test_len_accumulate(len_total, len);

		len = m0_net_test_str_serialize(op, &cmd->ntc_init.ntci_tm_ep,
						bv, offset + len_total);
		if (len == 0)
			break;
		len_total = net_test_len_accumulate(len_total, len);

		len = m0_net_test_slist_serialize(op, &cmd->ntc_init.ntci_ep,
						  bv, offset + len_total);
		break;
	case M0_NET_TEST_CMD_START:
	case M0_NET_TEST_CMD_STOP:
	case M0_NET_TEST_CMD_STATUS:
		break;
	case M0_NET_TEST_CMD_STATUS_DATA:
		len = cmd_status_data_serialize(op, &cmd->ntc_status_data,
						bv, offset + len_total);
		break;
	case M0_NET_TEST_CMD_INIT_DONE:
	case M0_NET_TEST_CMD_START_DONE:
	case M0_NET_TEST_CMD_STOP_DONE:
		len = m0_net_test_serialize(op, &cmd->ntc_done,
					    USE_TYPE_DESCR(m0_net_test_cmd_done),
					    bv, offset + len_total);
		break;
	default:
		return -ENOSYS;
	};

	len_total = net_test_len_accumulate(len_total, len);
	return len_total == 0 ? -EINVAL : 0;
}

/**
   Free m0_net_test_cmd after successful
   cmd_serialize(M0_NET_TEST_DESERIALIZE, ...).
 */
static void cmd_free(struct m0_net_test_cmd *cmd)
{
	M0_PRE(cmd != NULL);

	if (cmd->ntc_type == M0_NET_TEST_CMD_INIT) {
		m0_free0(&cmd->ntc_init.ntci_tm_ep);
		m0_net_test_slist_fini(&cmd->ntc_init.ntci_ep);
	}
}

static struct m0_net_test_cmd_ctx *
cmd_ctx_extract(struct m0_net_test_network_ctx *net_ctx)
{
	M0_PRE(net_ctx != NULL);

	return container_of(net_ctx, struct m0_net_test_cmd_ctx, ntcc_net);
}

static void commands_tm_event_cb(const struct m0_net_tm_event *ev)
{
	/* nothing here for now */
}

static void commands_cb_msg_recv(struct m0_net_test_network_ctx *net_ctx,
				 const uint32_t buf_index,
				 enum m0_net_queue_type q,
				 const struct m0_net_buffer_event *ev)
{
	struct m0_net_test_cmd_ctx *ctx = cmd_ctx_extract(net_ctx);

	M0_PRE(m0_net_test_commands_invariant(ctx));
	M0_PRE(buf_index >= ctx->ntcc_ep_nr && buf_index < ctx->ntcc_ep_nr * 2);
	M0_PRE(q == M0_NET_QT_MSG_RECV);
	LOGD("M0_NET_QT_MSG_RECV commands callback from %s",
	     ev->nbe_ep == NULL ? "NULL" : ev->nbe_ep->nep_addr);

	/* save endpoint and buffer status */
	if (ev->nbe_ep != NULL)
		m0_net_end_point_get(ev->nbe_ep);
	ctx->ntcc_buf_status[buf_index].ntcbs_ep = ev->nbe_ep;
	ctx->ntcc_buf_status[buf_index].ntcbs_buf_status = ev->nbe_status;

	/* put buffer to ringbuf */
	m0_net_test_ringbuf_push(&ctx->ntcc_rb, buf_index);

	/* m0_net_test_commands_recv() will down this semaphore */
	m0_semaphore_up(&ctx->ntcc_sem_recv);
}

static void commands_cb_msg_send(struct m0_net_test_network_ctx *net_ctx,
				 const uint32_t buf_index,
				 enum m0_net_queue_type q,
				 const struct m0_net_buffer_event *ev)
{
	struct m0_net_test_cmd_ctx *ctx = cmd_ctx_extract(net_ctx);

	M0_PRE(m0_net_test_commands_invariant(ctx));
	M0_PRE(q == M0_NET_QT_MSG_SEND);
	M0_PRE(buf_index < ctx->ntcc_ep_nr);
	LOGD("M0_NET_QT_MSG_SEND commands callback to %s",
	     ev->nbe_buffer->nb_ep == NULL ? "NULL" :
	     ev->nbe_buffer->nb_ep->nep_addr);

	/* invoke 'message sent' callback if it is present */
	if (ctx->ntcc_send_cb != NULL)
		ctx->ntcc_send_cb(ctx, buf_index, ev->nbe_status);

	m0_semaphore_up(&ctx->ntcc_sem_send);
}

static void commands_cb_impossible(struct m0_net_test_network_ctx *ctx,
				   const uint32_t buf_index,
				   enum m0_net_queue_type q,
				   const struct m0_net_buffer_event *ev)
{

	M0_IMPOSSIBLE("commands bulk buffer callback is impossible");
}

static const struct m0_net_tm_callbacks net_test_commands_tm_cb = {
	.ntc_event_cb = commands_tm_event_cb
};

static const struct m0_net_test_network_buffer_callbacks commands_buffer_cb = {
	.ntnbc_cb = {
		[M0_NET_QT_MSG_RECV]		= commands_cb_msg_recv,
		[M0_NET_QT_MSG_SEND]		= commands_cb_msg_send,
		[M0_NET_QT_PASSIVE_BULK_RECV]	= commands_cb_impossible,
		[M0_NET_QT_PASSIVE_BULK_SEND]	= commands_cb_impossible,
		[M0_NET_QT_ACTIVE_BULK_RECV]	= commands_cb_impossible,
		[M0_NET_QT_ACTIVE_BULK_SEND]	= commands_cb_impossible,
	}
};

static int commands_recv_enqueue(struct m0_net_test_cmd_ctx *ctx,
				 size_t buf_index)
{
	int rc;

	M0_PRE(buf_index >= ctx->ntcc_ep_nr && buf_index < ctx->ntcc_ep_nr * 2);

	ctx->ntcc_buf_status[buf_index].ntcbs_in_recv_queue = true;
	rc = m0_net_test_network_msg_recv(&ctx->ntcc_net, buf_index);
	if (rc != 0)
		ctx->ntcc_buf_status[buf_index].ntcbs_in_recv_queue = false;

	return rc;
}

static void commands_recv_dequeue(struct m0_net_test_cmd_ctx *ctx,
				  size_t buf_index)
{
	m0_net_test_network_buffer_dequeue(&ctx->ntcc_net, M0_NET_TEST_BUF_PING,
					   buf_index);
}

static void commands_recv_ep_put(struct m0_net_test_cmd_ctx *ctx,
				 size_t buf_index)
{
	if (ctx->ntcc_buf_status[buf_index].ntcbs_ep != NULL)
		m0_net_end_point_put(ctx->ntcc_buf_status[buf_index].ntcbs_ep);
}

static bool is_buf_in_recv_q(struct m0_net_test_cmd_ctx *ctx,
			     size_t buf_index)
{
	M0_PRE(buf_index >= ctx->ntcc_ep_nr && buf_index < ctx->ntcc_ep_nr * 2);

	return ctx->ntcc_buf_status[buf_index].ntcbs_in_recv_queue;
}

static void commands_recv_dequeue_nr(struct m0_net_test_cmd_ctx *ctx,
				     size_t nr)
{
	size_t i;

	/* remove recv buffers from queue */
	for (i = 0; i < nr; ++i)
		if (is_buf_in_recv_q(ctx, ctx->ntcc_ep_nr + i))
			commands_recv_dequeue(ctx, ctx->ntcc_ep_nr + i);
	/* wait until callbacks executed */
	for (i = 0; i < nr; ++i)
		if (is_buf_in_recv_q(ctx, ctx->ntcc_ep_nr + i))
			m0_semaphore_down(&ctx->ntcc_sem_recv);
	/* release endpoints */
	for (i = 0; i < nr; ++i)
		if (is_buf_in_recv_q(ctx, ctx->ntcc_ep_nr + i))
			commands_recv_ep_put(ctx, ctx->ntcc_ep_nr + i);
}

static int commands_initfini(struct m0_net_test_cmd_ctx *ctx,
			     const char *cmd_ep,
			     m0_time_t send_timeout,
			     m0_net_test_commands_send_cb_t send_cb,
			     struct m0_net_test_slist *ep_list,
			     bool init)
{
	struct m0_net_test_network_cfg net_cfg;
	int			       i;
	int			       rc = -EEXIST;

	M0_PRE(ctx != NULL);
	if (!init)
		goto fini;

	M0_PRE(ep_list->ntsl_nr > 0);
	M0_SET0(ctx);

	if (!m0_net_test_slist_unique(ep_list))
		goto fail;

	ctx->ntcc_ep_nr   = ep_list->ntsl_nr;
	ctx->ntcc_send_cb = send_cb;

	m0_mutex_init(&ctx->ntcc_send_mutex);
	rc = m0_semaphore_init(&ctx->ntcc_sem_send, 0);
	if (rc != 0)
		goto fail;
	rc = m0_semaphore_init(&ctx->ntcc_sem_recv, 0);
	if (rc != 0)
		goto free_sem_send;

	rc = m0_net_test_ringbuf_init(&ctx->ntcc_rb, ctx->ntcc_ep_nr * 2);
	if (rc != 0)
		goto free_sem_recv;

	M0_ALLOC_ARR(ctx->ntcc_buf_status, ctx->ntcc_ep_nr * 2);
	if (ctx->ntcc_buf_status == NULL)
		goto free_rb;

	M0_SET0(&net_cfg);
	net_cfg.ntncfg_tm_cb	     = net_test_commands_tm_cb;
	net_cfg.ntncfg_buf_cb	     = commands_buffer_cb;
	net_cfg.ntncfg_buf_size_ping = M0_NET_TEST_CMD_SIZE_MAX;
	net_cfg.ntncfg_buf_ping_nr   = 2 * ctx->ntcc_ep_nr;
	net_cfg.ntncfg_ep_max	     = ep_list->ntsl_nr;
	net_cfg.ntncfg_timeouts	     = m0_net_test_network_timeouts_never();
	net_cfg.ntncfg_timeouts.ntnt_timeout[M0_NET_QT_MSG_SEND] = send_timeout;

	rc = m0_net_test_network_ctx_init(&ctx->ntcc_net, &net_cfg, cmd_ep);
	if (rc != 0)
		goto free_buf_status;

	rc = m0_net_test_network_ep_add_slist(&ctx->ntcc_net, ep_list);
	if (rc != 0)
		goto free_net_ctx;
	for (i = 0; i < ctx->ntcc_ep_nr; ++i) {
		rc = commands_recv_enqueue(ctx, ctx->ntcc_ep_nr + i);
		if (rc != 0) {
			commands_recv_dequeue_nr(ctx, i);
			goto free_net_ctx;
		}
	}
	M0_POST(m0_net_test_commands_invariant(ctx));
	rc = 0;
	goto success;

    fini:
	M0_PRE(m0_net_test_commands_invariant(ctx));
	m0_net_test_commands_send_wait_all(ctx);
	commands_recv_dequeue_nr(ctx, ctx->ntcc_ep_nr);
    free_net_ctx:
	m0_net_test_network_ctx_fini(&ctx->ntcc_net);
    free_buf_status:
	m0_free(ctx->ntcc_buf_status);
    free_rb:
	m0_net_test_ringbuf_fini(&ctx->ntcc_rb);
    free_sem_recv:
	m0_semaphore_fini(&ctx->ntcc_sem_recv);
    free_sem_send:
	m0_semaphore_fini(&ctx->ntcc_sem_send);
    fail:
	m0_mutex_fini(&ctx->ntcc_send_mutex);
    success:
	return rc;
}

int m0_net_test_commands_init(struct m0_net_test_cmd_ctx *ctx,
			      const char *cmd_ep,
			      m0_time_t send_timeout,
			      m0_net_test_commands_send_cb_t send_cb,
			      struct m0_net_test_slist *ep_list)
{
	return commands_initfini(ctx, cmd_ep, send_timeout, send_cb, ep_list,
				 true);
}

void m0_net_test_commands_fini(struct m0_net_test_cmd_ctx *ctx)
{
	commands_initfini(ctx, NULL, M0_TIME_NEVER, NULL, NULL, false);
	M0_SET0(ctx);
}

int m0_net_test_commands_send(struct m0_net_test_cmd_ctx *ctx,
			      struct m0_net_test_cmd *cmd)
{
	struct m0_net_buffer *buf;
	int		      rc;
	size_t		      buf_index;

	M0_PRE(m0_net_test_commands_invariant(ctx));
	M0_PRE(cmd != NULL);

	LOGD("m0_net_test_commands_send: from %s to %lu %s cmd->ntc_type = %d",
	     ctx->ntcc_net.ntc_tm->ntm_ep->nep_addr, cmd->ntc_ep_index,
	     ctx->ntcc_net.ntc_ep[cmd->ntc_ep_index]->nep_addr,
	     cmd->ntc_type);

	buf_index = cmd->ntc_ep_index;
	buf = m0_net_test_network_buf(&ctx->ntcc_net, M0_NET_TEST_BUF_PING,
				      buf_index);

	rc = cmd_serialize(M0_NET_TEST_SERIALIZE, cmd, buf, 0, NULL);
	if (rc == 0)
		rc = m0_net_test_network_msg_send(&ctx->ntcc_net, buf_index,
						  cmd->ntc_ep_index);

	if (rc == 0) {
		m0_mutex_lock(&ctx->ntcc_send_mutex);
		ctx->ntcc_send_nr++;
		m0_mutex_unlock(&ctx->ntcc_send_mutex);
	}

	return rc;
}

void m0_net_test_commands_send_wait_all(struct m0_net_test_cmd_ctx *ctx)
{
	int64_t nr;
	int64_t i;

	LOGD("m0_net_test_commands_send_wait_all enter");
	M0_PRE(m0_net_test_commands_invariant(ctx));

	m0_mutex_lock(&ctx->ntcc_send_mutex);
	nr = ctx->ntcc_send_nr;
	ctx->ntcc_send_nr = 0;
	m0_mutex_unlock(&ctx->ntcc_send_mutex);

	LOGD("nr = %ld", (long) nr);
	for (i = 0; i < nr; ++i) {
		LOGD("semaphore_down() #%ld", (long) i);
		m0_semaphore_down(&ctx->ntcc_sem_send);
	}
	LOGD("m0_net_test_commands_send_wait_all leave");
}

int m0_net_test_commands_recv(struct m0_net_test_cmd_ctx *ctx,
			      struct m0_net_test_cmd *cmd,
			      m0_time_t deadline)
{
	struct m0_net_buffer	*buf;
	struct m0_net_end_point *ep;
	bool			 rc_bool;
	size_t			 buf_index;
	int			 rc;

	M0_PRE(m0_net_test_commands_invariant(ctx));
	M0_PRE(cmd != NULL);

	/* wait for received buffer */
	rc_bool = m0_semaphore_timeddown(&ctx->ntcc_sem_recv, deadline);
	/* buffer wasn't received before deadline */
	if (!rc_bool)
		return -ETIMEDOUT;

	/* get buffer */
	buf_index = m0_net_test_ringbuf_pop(&ctx->ntcc_rb);
	M0_ASSERT(is_buf_in_recv_q(ctx, buf_index));
	buf = m0_net_test_network_buf(&ctx->ntcc_net, M0_NET_TEST_BUF_PING,
				      buf_index);

	/* set m0_net_test_cmd.ntc_buf_index */
	cmd->ntc_buf_index = buf_index;

	/* check saved m0_net_buffer_event.nbe_status */
	rc = ctx->ntcc_buf_status[buf_index].ntcbs_buf_status;
	if (rc != 0)
		goto done;

	/* deserialize buffer to cmd */
	rc = cmd_serialize(M0_NET_TEST_DESERIALIZE, cmd, buf, 0, NULL);
	if (rc != 0)
		cmd->ntc_type = M0_NET_TEST_CMD_NR;

	/* set m0_net_test_cmd.ntc_ep_index and release endpoint */
	ep = ctx->ntcc_buf_status[buf_index].ntcbs_ep;
	M0_ASSERT(ep != NULL);
	cmd->ntc_ep_index = m0_net_test_network_ep_search(&ctx->ntcc_net,
							  ep->nep_addr);
	m0_net_end_point_put(ep);

done:
	/* buffer now not in receive queue */
	ctx->ntcc_buf_status[buf_index].ntcbs_in_recv_queue = false;

	LOGD("m0_net_test_commands_recv: from %lu %s to %s "
	     "rc = %d cmd->ntc_type = %d",
	     cmd->ntc_ep_index,
	     ctx->ntcc_net.ntc_ep[cmd->ntc_ep_index]->nep_addr,
	     ctx->ntcc_net.ntc_tm->ntm_ep->nep_addr,
	     rc, cmd->ntc_type);

	return rc;
}

int m0_net_test_commands_recv_enqueue(struct m0_net_test_cmd_ctx *ctx,
				      size_t buf_index)
{
	M0_PRE(m0_net_test_commands_invariant(ctx));

	return commands_recv_enqueue(ctx, buf_index);
}

void m0_net_test_commands_received_free(struct m0_net_test_cmd *cmd)
{
	cmd_free(cmd);
}

bool m0_net_test_commands_invariant(struct m0_net_test_cmd_ctx *ctx)
{

	if (ctx == NULL)
		return false;
	if (ctx->ntcc_ep_nr == 0)
		return false;
	if (ctx->ntcc_ep_nr != ctx->ntcc_net.ntc_ep_nr)
		return false;
	if (ctx->ntcc_ep_nr * 2 != ctx->ntcc_net.ntc_cfg.ntncfg_buf_ping_nr)
		return false;
	if (ctx->ntcc_net.ntc_cfg.ntncfg_buf_bulk_nr != 0)
		return false;
	return true;
}

#undef NET_TEST_MODULE_NAME

/** @} end of NetTestCommandsInternals group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
