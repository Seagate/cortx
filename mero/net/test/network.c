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
 * Original creation date: 03/22/2012
 */

#include "lib/errno.h"		/* E2BIG */
#include "lib/memory.h"		/* M0_ALLOC_ARR */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/vec.h"		/* M0_SEG_SHIFT */

#include "mero/magic.h"	/* M0_NET_TEST_NETWORK_BD_MAGIC */

#include "net/net.h"		/* m0_net_buffer */
#include "net/lnet/lnet.h"	/* m0_net_lnet_xprt */

#include "net/test/network.h"

/**
   @defgroup NetTestNetworkInternals Network
   @ingroup NetTestInternals

   @todo add timeouts to channels and network buffers
   @todo align code (function parameters etc.)
   @todo cache m0_vec_count()

   @see
   @ref net-test

   @{
 */

/**
   Get net-test network context for the buffer event.
 */
static struct m0_net_test_network_ctx *
cb_ctx_extract(const struct m0_net_buffer_event *ev)
{
	return ev->nbe_buffer->nb_app_private;
}

/**
   Get buffer number in net-test network context for the buffer event.
 */
static uint32_t cb_buf_index_extract(const struct m0_net_buffer_event *ev,
				     struct m0_net_test_network_ctx *ctx,
				     enum m0_net_queue_type q)
{
	struct m0_net_buffer *arr;
	bool		      type_ping;
	int		      index;
	int		      index_max;

	M0_PRE(ctx != NULL);

	type_ping = q == M0_NET_QT_MSG_SEND || q == M0_NET_QT_MSG_RECV;
	arr = type_ping ? ctx->ntc_buf_ping : ctx->ntc_buf_bulk;
	index = ev->nbe_buffer - arr;
	index_max = type_ping ? ctx->ntc_cfg.ntncfg_buf_ping_nr :
				ctx->ntc_cfg.ntncfg_buf_bulk_nr;

	M0_POST(index >= 0 && index < index_max);

	return index;
}

/**
   Default callback for all network buffers.
   Calls user-defined callback for the buffer.
   @see net_test_buf_init()
 */
static void cb_default(const struct m0_net_buffer_event *ev)
{
	struct m0_net_buffer	       *buf = ev->nbe_buffer;
	struct m0_net_test_network_ctx *ctx;
	uint32_t			buf_index;
	enum m0_net_queue_type		q;

	M0_PRE(buf != NULL);
	M0_PRE((buf->nb_flags & M0_NET_BUF_QUEUED) == 0);
	q = ev->nbe_buffer->nb_qtype;

	ctx = cb_ctx_extract(ev);
	M0_ASSERT(ctx != NULL);
	buf_index = cb_buf_index_extract(ev, ctx, q);

	/* m0_net_buffer.nb_max_receive_msgs will be always set to 1 */
	if (q == M0_NET_QT_MSG_RECV || q == M0_NET_QT_ACTIVE_BULK_RECV ||
	    q == M0_NET_QT_PASSIVE_BULK_RECV) {
		buf->nb_length = ev->nbe_length;
		buf->nb_offset = 0;
	}

	ctx->ntc_cfg.ntncfg_buf_cb.ntnbc_cb[q](ctx, buf_index, q, ev);
}

static struct m0_net_buffer_callbacks net_test_network_buf_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_RECV]		= cb_default,
		[M0_NET_QT_MSG_SEND]		= cb_default,
		[M0_NET_QT_PASSIVE_BULK_RECV]	= cb_default,
		[M0_NET_QT_PASSIVE_BULK_SEND]	= cb_default,
		[M0_NET_QT_ACTIVE_BULK_RECV]	= cb_default,
		[M0_NET_QT_ACTIVE_BULK_SEND]	= cb_default,
	}
};

/**
   Initialize network buffer with given size
   (allocate and register within domain).
   @see cb_default()
   @pre buf != NULL
 */
static int net_test_buf_init(struct m0_net_buffer *buf,
			     m0_bcount_t size,
			     struct m0_net_test_network_ctx *ctx)
{
	int		      rc;
	m0_bcount_t	      seg_size;
	uint32_t	      seg_num;
	m0_bcount_t	      seg_size_max;
	m0_bcount_t	      buf_size_max;
	struct m0_net_domain *dom;

	M0_PRE(buf != NULL);
	M0_PRE(ctx != NULL);

	M0_SET0(buf);

	dom = ctx->ntc_dom;

	buf_size_max = m0_net_domain_get_max_buffer_size(dom);
	if (size > buf_size_max)
		return -E2BIG;

	seg_size_max = m0_net_domain_get_max_buffer_segment_size(dom);

	M0_ASSERT(seg_size_max > 0);

	seg_size = size < seg_size_max ? size : seg_size_max;
	seg_num  = size / seg_size_max + !!(size % seg_size_max);

	if (seg_size * seg_num > buf_size_max)
		return -E2BIG;

	rc = m0_bufvec_alloc_aligned(&buf->nb_buffer, seg_num, seg_size,
			M0_SEG_SHIFT);
	if (rc == 0) {
		buf->nb_length		 = size;
		buf->nb_max_receive_msgs = 1;
		buf->nb_min_receive_size = size;
		buf->nb_offset	         = 0;
		buf->nb_callbacks	 = &net_test_network_buf_cb;
		buf->nb_ep		 = NULL;
		buf->nb_desc.nbd_len	 = 0;
		buf->nb_desc.nbd_data	 = NULL;
		buf->nb_app_private	 = ctx;
		buf->nb_timeout		 = M0_TIME_NEVER;

		rc = m0_net_buffer_register(buf, dom);
		if (rc != 0)
			m0_bufvec_free_aligned(&buf->nb_buffer,
					M0_SEG_SHIFT);
	}
	return rc;
}

static void net_test_buf_fini(struct m0_net_buffer *buf,
			      struct m0_net_domain *dom)
{
	M0_PRE(buf->nb_dom == dom);

	m0_net_buffer_deregister(buf, dom);
	m0_bufvec_free_aligned(&buf->nb_buffer, M0_SEG_SHIFT);
	m0_net_desc_free(&buf->nb_desc);
}

static void net_test_bufs_fini(struct m0_net_buffer *buf,
			       uint32_t buf_nr,
			       struct m0_net_domain *dom)
{
	int i;

	for (i = 0; i < buf_nr; ++i)
		net_test_buf_fini(&buf[i], dom);
}

static int net_test_bufs_init(struct m0_net_buffer *buf,
			      uint32_t buf_nr,
			      m0_bcount_t size,
			      struct m0_net_test_network_ctx *ctx)
{
	int		      i;
	int		      rc = 0;
	struct m0_net_domain *dom = ctx->ntc_dom;

	for (i = 0; i < buf_nr; ++i) {
		rc = net_test_buf_init(&buf[i], size, ctx);
		if (rc != 0)
			break;
		M0_ASSERT(buf[i].nb_dom == dom);
	}
	if (i != buf_nr)
		net_test_bufs_fini(buf, i, dom);
	return rc;
}

/** Stop transfer machine and wait for state transition */
static void net_test_tm_stop(struct m0_net_transfer_mc *tm)
{
	int	        rc;
	struct m0_clink	tmwait;

	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&tm->ntm_chan, &tmwait);

	rc = m0_net_tm_stop(tm, true);
	M0_ASSERT(rc == 0);

	do {
		m0_chan_wait(&tmwait);
	} while (tm->ntm_state != M0_NET_TM_STOPPED &&
		 tm->ntm_state != M0_NET_TM_FAILED);

	m0_clink_del_lock(&tmwait);
	m0_clink_fini(&tmwait);
}

bool m0_net_test_network_ctx_invariant(struct m0_net_test_network_ctx *ctx)
{
	M0_PRE(ctx != NULL);

	return ctx->ntc_ep_nr <= ctx->ntc_cfg.ntncfg_ep_max;
}

static int net_test_network_ctx_initfini(struct m0_net_test_network_ctx *ctx,
					 struct m0_net_test_network_cfg *cfg,
					 const char *tm_addr)
{
	struct m0_clink tmwait;
	int		rc;
	int		i;

	M0_PRE(ctx != NULL);
	M0_PRE(equi(cfg != NULL, tm_addr != NULL));

	if (cfg == NULL)
		goto fini;

	M0_SET0(ctx);
	ctx->ntc_cfg = *cfg;

	rc = -ENOMEM;
	/** @todo make ctx->ntc_dom embedded into ctx */
	M0_ALLOC_PTR(ctx->ntc_dom);
	if (ctx->ntc_dom == NULL)
		goto fail;
	M0_ALLOC_PTR(ctx->ntc_tm);
	if (ctx->ntc_tm == NULL)
		goto free_dom;
	M0_ALLOC_ARR(ctx->ntc_buf_ping, ctx->ntc_cfg.ntncfg_buf_ping_nr);
	if (ctx->ntc_buf_ping == NULL)
		goto free_tm;
	M0_ALLOC_ARR(ctx->ntc_buf_bulk, ctx->ntc_cfg.ntncfg_buf_bulk_nr);
	if (ctx->ntc_buf_bulk == NULL)
		goto free_buf_ping;
	M0_ALLOC_ARR(ctx->ntc_ep, ctx->ntc_cfg.ntncfg_ep_max);
	if (ctx->ntc_ep == NULL)
		goto free_buf_bulk;

	rc = m0_net_domain_init(ctx->ntc_dom, &m0_net_lnet_xprt);
	if (rc != 0)
		goto free_ep;

	/* init and start tm */
	ctx->ntc_tm->ntm_state     = M0_NET_TM_UNDEFINED;
	ctx->ntc_tm->ntm_callbacks = &ctx->ntc_cfg.ntncfg_tm_cb;
	/** @todo replace gmc and ctx */
	rc = m0_net_tm_init(ctx->ntc_tm, ctx->ntc_dom);
	if (rc != 0)
		goto fini_dom;

	rc = ctx->ntc_cfg.ntncfg_sync ?
	     m0_net_buffer_event_deliver_synchronously(ctx->ntc_tm) : 0;
	if (rc != 0)
		goto fini_tm;

	m0_clink_init(&tmwait, NULL);
	m0_clink_add_lock(&ctx->ntc_tm->ntm_chan, &tmwait);
	rc = m0_net_tm_start(ctx->ntc_tm, tm_addr);
	m0_chan_wait(&tmwait);
	m0_clink_del_lock(&tmwait);
	m0_clink_fini(&tmwait);
	if (rc != 0)
		goto fini_tm;
	if (ctx->ntc_tm->ntm_state != M0_NET_TM_STARTED) {
		rc = -ECONNREFUSED;
		goto fini_tm;
	}

	/* init and register buffers */
	rc = net_test_bufs_init(ctx->ntc_buf_ping,
				ctx->ntc_cfg.ntncfg_buf_ping_nr,
				ctx->ntc_cfg.ntncfg_buf_size_ping, ctx);
	if (rc != 0)
		goto stop_tm;
	rc = net_test_bufs_init(ctx->ntc_buf_bulk,
				ctx->ntc_cfg.ntncfg_buf_bulk_nr,
				ctx->ntc_cfg.ntncfg_buf_size_bulk, ctx);
	if (rc != 0)
		goto fini_buf_ping;

	M0_POST(m0_net_test_network_ctx_invariant(ctx));
	goto success;
fini:
	M0_PRE(m0_net_test_network_ctx_invariant(ctx));

	rc = 0;
	for (i = 0; i < ctx->ntc_ep_nr; ++i)
		m0_net_end_point_put(ctx->ntc_ep[i]);
	net_test_bufs_fini(ctx->ntc_buf_bulk, ctx->ntc_cfg.ntncfg_buf_bulk_nr,
			   ctx->ntc_dom);
fini_buf_ping:
	net_test_bufs_fini(ctx->ntc_buf_ping, ctx->ntc_cfg.ntncfg_buf_ping_nr,
			   ctx->ntc_dom);
stop_tm:
	net_test_tm_stop(ctx->ntc_tm);
fini_tm:
	m0_net_tm_fini(ctx->ntc_tm);
fini_dom:
	m0_net_domain_fini(ctx->ntc_dom);
free_ep:
	m0_free(ctx->ntc_ep);
free_buf_bulk:
	m0_free(ctx->ntc_buf_bulk);
free_buf_ping:
	m0_free(ctx->ntc_buf_ping);
free_tm:
	m0_free(ctx->ntc_tm);
free_dom:
	m0_free(ctx->ntc_dom);
fail:
success:
	return rc;
}

int m0_net_test_network_ctx_init(struct m0_net_test_network_ctx *ctx,
				 struct m0_net_test_network_cfg *cfg,
				 const char *tm_addr)
{
	return net_test_network_ctx_initfini(ctx, cfg, tm_addr);
}

void m0_net_test_network_ctx_fini(struct m0_net_test_network_ctx *ctx)
{
	int rc = net_test_network_ctx_initfini(ctx, NULL, NULL);
	M0_ASSERT(rc == 0);
}

int m0_net_test_network_ep_add(struct m0_net_test_network_ctx *ctx,
			       const char *ep_addr)
{
	int rc;

	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	M0_PRE(ep_addr != NULL);

	if (ctx->ntc_ep_nr != ctx->ntc_cfg.ntncfg_ep_max) {
		rc = m0_net_end_point_create(&ctx->ntc_ep[ctx->ntc_ep_nr],
					     ctx->ntc_tm, ep_addr);
		M0_ASSERT(rc <= 0);
		if (rc == 0)
			rc = ctx->ntc_ep_nr++;
	} else {
		rc = -E2BIG;
	}
	return rc;
}

int m0_net_test_network_ep_add_slist(struct m0_net_test_network_ctx *ctx,
				     const struct m0_net_test_slist *eps)
{
	int    rc = 0;
	size_t i;

	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	M0_PRE(m0_net_test_slist_invariant(eps));

	for (i = 0; i < eps->ntsl_nr; ++i) {
		rc = m0_net_test_network_ep_add(ctx, eps->ntsl_list[i]);
		if (rc < 0)
			break;
	}
	if (rc < 0) {
		/* m0_net_end_point_put() for last i endpoints */
		for (; i != 0; --i)
			m0_net_end_point_put(ctx->ntc_ep[--ctx->ntc_ep_nr]);
	}
	M0_POST(m0_net_test_network_ctx_invariant(ctx));

	return rc >= 0 ? 0 : rc;
}

static int net_test_buf_queue(struct m0_net_test_network_ctx *ctx,
			      struct m0_net_buffer *nb,
			      enum m0_net_queue_type q)
{
	m0_time_t timeout = ctx->ntc_cfg.ntncfg_timeouts.ntnt_timeout[q];

	M0_PRE((nb->nb_flags & M0_NET_BUF_QUEUED) == 0);
	M0_PRE(ergo(q == M0_NET_QT_MSG_SEND, nb->nb_ep != NULL));

	nb->nb_qtype   = q;
	nb->nb_offset  = 0;	/* nb->nb_length already set */
	nb->nb_ep      = q != M0_NET_QT_MSG_SEND ? NULL : nb->nb_ep;
	nb->nb_timeout = timeout == M0_TIME_NEVER ?
			 M0_TIME_NEVER : m0_time_add(m0_time_now(), timeout);

	return m0_net_buffer_add(nb, ctx->ntc_tm);
}

int m0_net_test_network_msg_send_ep(struct m0_net_test_network_ctx *ctx,
				    uint32_t buf_ping_index,
				    struct m0_net_end_point *ep)
{
	struct m0_net_buffer *nb;

	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	M0_PRE(buf_ping_index < ctx->ntc_cfg.ntncfg_buf_ping_nr);

	nb = &ctx->ntc_buf_ping[buf_ping_index];
	nb->nb_ep = ep;

	return net_test_buf_queue(ctx, nb, M0_NET_QT_MSG_SEND);
}

int m0_net_test_network_msg_send(struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_ping_index,
				 uint32_t ep_index)
{
	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	M0_PRE(buf_ping_index < ctx->ntc_cfg.ntncfg_buf_ping_nr);
	M0_PRE(ep_index < ctx->ntc_ep_nr);

	return m0_net_test_network_msg_send_ep(ctx, buf_ping_index,
					       ctx->ntc_ep[ep_index]);
}

int m0_net_test_network_msg_recv(struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_ping_index)
{
	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	M0_PRE(buf_ping_index < ctx->ntc_cfg.ntncfg_buf_ping_nr);

	return net_test_buf_queue(ctx, &ctx->ntc_buf_ping[buf_ping_index],
			M0_NET_QT_MSG_RECV);
}

int m0_net_test_network_bulk_enqueue(struct m0_net_test_network_ctx *ctx,
				     int32_t buf_bulk_index,
				     int32_t ep_index,
				     enum m0_net_queue_type q)
{
	struct m0_net_buffer *buf;

	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	M0_PRE(buf_bulk_index < ctx->ntc_cfg.ntncfg_buf_bulk_nr);

	buf = &ctx->ntc_buf_bulk[buf_bulk_index];
	if (q == M0_NET_QT_PASSIVE_BULK_SEND ||
	    q == M0_NET_QT_PASSIVE_BULK_RECV) {
		M0_PRE(ep_index < ctx->ntc_ep_nr);
		buf->nb_ep = ctx->ntc_ep[ep_index];
	} else
		buf->nb_ep = NULL;

	return net_test_buf_queue(ctx, buf, q);
}

void m0_net_test_network_buffer_dequeue(struct m0_net_test_network_ctx *ctx,
					enum m0_net_test_network_buf_type
					buf_type,
					int32_t buf_index)
{
	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	m0_net_buffer_del(m0_net_test_network_buf(ctx, buf_type, buf_index),
			  ctx->ntc_tm);
}

/** Structure to help m0_net_buf_desc serialization */
struct net_test_network_bd {
	/** M0_NET_TEST_NETWORK_BD_MAGIC */
	uint64_t    ntnbd_magic;
	/** Passive buffer size */
	m0_bcount_t ntnbd_buf_size;
	/** m0_net_buf_desc.nbd_len */
	uint32_t    ntnbd_len;
};

/** net_test_network_bd_descr */
TYPE_DESCR(net_test_network_bd) = {
	FIELD_DESCR(struct net_test_network_bd, ntnbd_magic),
	FIELD_DESCR(struct net_test_network_bd, ntnbd_buf_size),
	FIELD_DESCR(struct net_test_network_bd, ntnbd_len),
};

/** Header for ping buffer with serialized m0_net_buf_desc's */
struct net_test_network_bds_header {
	/** M0_NET_TEST_NETWORK_BDS_MAGIC */
	uint64_t ntnbh_magic;
	/** Number of buffer descriptors in ping buffer */
	uint64_t ntnbh_nr;
};

/** net_test_network_bds_header_descr */
TYPE_DESCR(net_test_network_bds_header) = {
	FIELD_DESCR(struct net_test_network_bds_header, ntnbh_magic),
	FIELD_DESCR(struct net_test_network_bds_header, ntnbh_nr),
};

/** @see m0_net_test_serialize() */
static m0_bcount_t network_bd_serialize(enum m0_net_test_serialize_op op,
					struct m0_net_buffer *buf,
					struct m0_bufvec *bv,
					m0_bcount_t bv_offset)
{
	struct net_test_network_bd bd;
	m0_bcount_t		   len;
	m0_bcount_t		   len_total;

	M0_PRE(buf != NULL);
	M0_PRE(bv != NULL);

	bd.ntnbd_magic	  = M0_NET_TEST_NETWORK_BD_MAGIC;
	bd.ntnbd_buf_size = buf->nb_length;
	bd.ntnbd_len	  = buf->nb_desc.nbd_len;

	len = m0_net_test_serialize(op, &bd,
				    USE_TYPE_DESCR(net_test_network_bd),
				    bv, bv_offset);
	if (len == 0)
		return 0;
	len_total = net_test_len_accumulate(0, len);

	/* optimizing memory allocation */
	if (op == M0_NET_TEST_DESERIALIZE &&
	    buf->nb_desc.nbd_len != bd.ntnbd_len) {
		/* free old */
		m0_free(buf->nb_desc.nbd_data);
		buf->nb_desc.nbd_len = 0;
		/* alloc new */
		buf->nb_desc.nbd_data = m0_alloc(bd.ntnbd_len);
		if (buf->nb_desc.nbd_data == NULL)
			return 0;
		buf->nb_desc.nbd_len = bd.ntnbd_len;
	}

	len = m0_net_test_serialize_data(op, buf->nb_desc.nbd_data,
					 buf->nb_desc.nbd_len, true,
					 bv, bv_offset + len_total);
	len_total = net_test_len_accumulate(len_total, len);
	return len_total;
}

static m0_bcount_t network_bds_serialize(enum m0_net_test_serialize_op op,
					 size_t *nr,
					 struct m0_bufvec *bv)
{
	struct net_test_network_bds_header header;
	m0_bcount_t			   len;

	M0_PRE(nr != NULL);
	M0_PRE(bv != NULL);

	if (op == M0_NET_TEST_SERIALIZE) {
		header.ntnbh_magic = M0_NET_TEST_NETWORK_BDS_MAGIC;
		header.ntnbh_nr	   = *nr;
	}

	len = m0_net_test_serialize(op, &header,
				USE_TYPE_DESCR(net_test_network_bds_header),
				    bv, 0);

	if (op == M0_NET_TEST_DESERIALIZE) {
		if (header.ntnbh_magic == M0_NET_TEST_NETWORK_BDS_MAGIC)
			*nr = header.ntnbh_nr;
		else
			len = 0;
	}

	return len;
}

/**
   Number of serialized network buffer descriptors in ping buffer
   is stored inside ping buffer in serialized form. This function
   modifies this number.
   @param ctx Network context
   @param buf_ping_index Index of ping buffer
   @param value This value will be added to the number of serialized
		network buffer descriptors. Can be -1, 0, 1. If it is
		-1, then function will M0_ASSERT() that number will
		not underflow.
   @return New value of number of serialized network buffer descriptors.
 */
static size_t network_bd_nr_add(struct m0_net_test_network_ctx *ctx,
				uint32_t buf_ping_index,
				int32_t value)
{
	struct m0_bufvec *bv;
	m0_bcount_t	  len;
	size_t		  nr;

	M0_PRE(ctx != NULL);
	M0_PRE(buf_ping_index < ctx->ntc_cfg.ntncfg_buf_ping_nr);
	M0_PRE(M0_IN(value, (-1, 0, 1)));

	bv = &m0_net_test_network_buf(ctx, M0_NET_TEST_BUF_PING,
				      buf_ping_index)->nb_buffer;
	len = network_bds_serialize(M0_NET_TEST_DESERIALIZE, &nr, bv);
	M0_ASSERT(len != 0);
	/* simply "get number of network bd" */
	if (value == 0)
		return nr;

	M0_ASSERT(ergo(value == -1, nr > 0));
	nr += value;
	len = network_bds_serialize(M0_NET_TEST_SERIALIZE, &nr, bv);
	M0_ASSERT(len != 0);

	return nr;
}

m0_bcount_t
m0_net_test_network_bd_serialize(enum m0_net_test_serialize_op op,
				 struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_bulk_index,
				 uint32_t buf_ping_index,
				 m0_bcount_t offset)
{
	struct m0_net_buffer *buf;
	struct m0_bufvec     *bv;
	m0_bcount_t	      len;
	m0_bcount_t	      len_total;
	size_t		      nr;

	M0_PRE(op == M0_NET_TEST_SERIALIZE || op == M0_NET_TEST_DESERIALIZE);
	M0_PRE(ctx != NULL);
	M0_PRE(buf_bulk_index < ctx->ntc_cfg.ntncfg_buf_bulk_nr);
	M0_PRE(buf_ping_index < ctx->ntc_cfg.ntncfg_buf_ping_nr);

	/*
	M0_LOG(M0_DEBUG, "%d %s", op, ctx->ntc_tm->ntm_ep->nep_addr);
	M0_LOG(M0_DEBUG, "bd_serialize: op = %d, tm_addr = %s, "
			 "buf_bulk_index = %u, buf_ping_index = %u, "
			 "offset = %lu",
			 op, ctx->ntc_tm->ntm_ep->nep_addr,
			 buf_bulk_index, buf_ping_index,
			 (long unsigned) offset);
	*/

	bv = &m0_net_test_network_buf(ctx, M0_NET_TEST_BUF_PING,
				      buf_ping_index)->nb_buffer;
	M0_ASSERT(bv != NULL);
	buf = m0_net_test_network_buf(ctx, M0_NET_TEST_BUF_BULK,
				      buf_bulk_index);
	M0_ASSERT(buf != NULL);

	if (offset == 0) {
		nr = 0;
		/* include header length to the first descriptor length */
		len = network_bds_serialize(op, &nr, bv);
		if (len == 0)
			return 0;
		len_total = net_test_len_accumulate(0, len);
	} else {
		len_total = 0;
	}
	len = network_bd_serialize(op, buf, bv, offset + len_total);
	len_total = net_test_len_accumulate(len_total, len);
	/* increase number of descriptors for 'serialize' operation */
	if (len_total != 0 && op == M0_NET_TEST_SERIALIZE)
		network_bd_nr_add(ctx, buf_ping_index, 1);

	/*
	M0_LOG(M0_DEBUG, "bd_serialize: len_total = %lu",
	       (long unsigned) len_total);
	*/
	return len_total;
}

size_t m0_net_test_network_bd_nr(struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_ping_index)
{
	return network_bd_nr_add(ctx, buf_ping_index, 0);
}

void m0_net_test_network_bd_nr_dec(struct m0_net_test_network_ctx *ctx,
				   uint32_t buf_ping_index)
{
	network_bd_nr_add(ctx, buf_ping_index, -1);
}

struct m0_net_buffer *
m0_net_test_network_buf(struct m0_net_test_network_ctx *ctx,
			enum m0_net_test_network_buf_type buf_type,
			uint32_t buf_index)
{
	M0_PRE(ctx != NULL);
	M0_PRE(buf_type == M0_NET_TEST_BUF_PING ||
	       buf_type == M0_NET_TEST_BUF_BULK);
	M0_PRE(buf_index < (buf_type == M0_NET_TEST_BUF_PING ?
	       ctx->ntc_cfg.ntncfg_buf_ping_nr :
	       ctx->ntc_cfg.ntncfg_buf_bulk_nr));

	return buf_type == M0_NET_TEST_BUF_PING ?
		&ctx->ntc_buf_ping[buf_index] : &ctx->ntc_buf_bulk[buf_index];
}

/** @todo isn't safe because net_test_buf_init() can fail */
int m0_net_test_network_buf_resize(struct m0_net_test_network_ctx *ctx,
				   enum m0_net_test_network_buf_type buf_type,
				   uint32_t buf_index,
				   m0_bcount_t new_size) {
	struct m0_net_buffer *buf;

	M0_PRE(ctx != NULL);

	buf = m0_net_test_network_buf(ctx, buf_type, buf_index);
	M0_ASSERT(buf != NULL);

	net_test_buf_fini(buf, ctx->ntc_dom);
	return net_test_buf_init(buf, new_size, ctx);
}

void m0_net_test_network_buf_fill(struct m0_net_test_network_ctx *ctx,
				  enum m0_net_test_network_buf_type buf_type,
				  uint32_t buf_index,
				  uint8_t fill)
{
	struct m0_bufvec       *bv;
	struct m0_bufvec_cursor bc;
	m0_bcount_t		length;
	m0_bcount_t		i;
	bool			rc_bool;

	M0_PRE(m0_net_test_network_ctx_invariant(ctx));

	bv = &m0_net_test_network_buf(ctx, buf_type, buf_index)->nb_buffer;
	M0_ASSERT(bv != NULL);
	length = m0_vec_count(&bv->ov_vec);
	m0_bufvec_cursor_init(&bc, bv);
	/** @todo use m0_bufvec_cursor_step */
	for (i = 0; i < length; ++i) {
		* (uint8_t *) m0_bufvec_cursor_addr(&bc) = fill;
		m0_bufvec_cursor_move(&bc, 1);
	}
	rc_bool = m0_bufvec_cursor_move(&bc, 0);
	M0_ASSERT(rc_bool);
}

struct m0_net_end_point *
m0_net_test_network_ep(struct m0_net_test_network_ctx *ctx, size_t ep_index)
{
	M0_PRE(m0_net_test_network_ctx_invariant(ctx));
	M0_PRE(ep_index < ctx->ntc_ep_nr);

	return ctx->ntc_ep[ep_index];
}

ssize_t m0_net_test_network_ep_search(struct m0_net_test_network_ctx *ctx,
				      const char *ep_addr)
{
	size_t addr_len = strlen(ep_addr) + 1;
	size_t i;

	for (i = 0; i < ctx->ntc_ep_nr; ++i)
		if (strncmp(ep_addr, ctx->ntc_ep[i]->nep_addr, addr_len) == 0)
			return i;
	return -1;
}

struct m0_net_test_network_timeouts m0_net_test_network_timeouts_never(void)
{
	struct m0_net_test_network_timeouts result;
	int				    i;

	for (i = 0; i < M0_NET_QT_NR; ++i)
		result.ntnt_timeout[i] = M0_TIME_NEVER;
	return result;
}

/** @} end of NetTestNetworkInternals group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
