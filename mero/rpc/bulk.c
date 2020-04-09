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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 *		    Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 06/27/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"
#include "lib/tlist.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"                      /* M0_IN */
#include "lib/finject.h"
#include "mero/magic.h"
#include "net/net.h"
#include "rpc/bulk.h"
#include "rpc/addb2.h"
#include "rpc/rpc_internal.h"

/**
   @addtogroup rpc

   @{
 */
M0_TL_DESCR_DEFINE(rpcbulk, "rpc bulk buffer list", M0_INTERNAL,
		   struct m0_rpc_bulk_buf, bb_link, bb_magic,
		   M0_RPC_BULK_BUF_MAGIC, M0_RPC_BULK_MAGIC);

M0_EXPORTED(rpcbulk_tl);

M0_TL_DEFINE(rpcbulk, M0_INTERNAL, struct m0_rpc_bulk_buf);

static bool rpc_bulk_buf_invariant(const struct m0_rpc_bulk_buf *rbuf)
{
	return rbuf != NULL &&
		rbuf->bb_magic == M0_RPC_BULK_BUF_MAGIC &&
		rbuf->bb_rbulk != NULL &&
		rpcbulk_tlink_is_in(rbuf);
}

static bool rpc_bulk_invariant(const struct m0_rpc_bulk *rbulk)
{
	return
		rbulk != NULL && rbulk->rb_magic == M0_RPC_BULK_MAGIC &&
		m0_mutex_is_locked(&rbulk->rb_mutex) &&
		m0_tl_forall(rpcbulk, buf, &rbulk->rb_buflist,
			     rpc_bulk_buf_invariant(buf) &&
			     buf->bb_rbulk == rbulk);
}

static void rpc_bulk_buf_deregister(struct m0_rpc_bulk_buf *buf)
{
	if (buf->bb_flags & M0_RPC_BULK_NETBUF_REGISTERED) {
		m0_net_buffer_deregister(buf->bb_nbuf, buf->bb_nbuf->nb_dom);
		buf->bb_flags &= ~M0_RPC_BULK_NETBUF_REGISTERED;
	}
}
static void rpc_bulk_buf_fini(struct m0_rpc_bulk_buf *rbuf)
{
	struct m0_net_buffer *nbuf = rbuf->bb_nbuf;

	M0_ENTRY("bulk_buf: %p", rbuf);
	M0_PRE(rbuf != NULL);
	M0_PRE(!(nbuf->nb_flags & M0_NET_BUF_QUEUED));

	rpc_bulk_buf_deregister(rbuf);
	m0_net_desc_free(&nbuf->nb_desc);
	m0_0vec_fini(&rbuf->bb_zerovec);
	if (rbuf->bb_flags & M0_RPC_BULK_NETBUF_ALLOCATED)
		m0_free(nbuf);
	m0_free(rbuf);
	M0_LEAVE();
}

static int rpc_bulk_buf_init(struct m0_rpc_bulk_buf *rbuf, uint32_t segs_nr,
			     m0_bcount_t length, struct m0_net_buffer *nb)
{
	int           rc;
	uint32_t      i;
	struct m0_buf cbuf;
	m0_bindex_t   index = 0;

	M0_ENTRY("bulk_buf: %p, net_buf: %p", rbuf, nb);
	M0_PRE(rbuf != NULL);
	M0_PRE(segs_nr > 0);

	rc = m0_0vec_init(&rbuf->bb_zerovec, segs_nr);
	if (rc != 0)
		return M0_ERR_INFO(rc, "bulk_buf: Zero vector initialization");

	rbuf->bb_flags = 0;
	if (nb == NULL) {
		M0_ALLOC_PTR(rbuf->bb_nbuf);
		if (rbuf->bb_nbuf == NULL) {
			m0_0vec_fini(&rbuf->bb_zerovec);
			return M0_ERR(-ENOMEM);
		}
		rbuf->bb_flags |= M0_RPC_BULK_NETBUF_ALLOCATED;
		rbuf->bb_nbuf->nb_buffer = rbuf->bb_zerovec.z_bvec;
	} else {
		rbuf->bb_nbuf = nb;
		/*
		 * Incoming buffer can be bigger while the bulk transfer
		 * request could refer to smaller size. Hence initialize
		 * the zero vector to get correct size of bulk transfer.
		 */
		for (i = 0; i < segs_nr; ++i) {
			cbuf.b_addr = nb->nb_buffer.ov_buf[i];
			cbuf.b_nob = nb->nb_buffer.ov_vec.v_count[i];
			rc = m0_0vec_cbuf_add(&rbuf->bb_zerovec, &cbuf, &index);
			if (rc != 0) {
				m0_0vec_fini(&rbuf->bb_zerovec);
				return M0_ERR_INFO(rc, "Addition of cbuf");
			}
		}
	}
	if (length != 0)
		rbuf->bb_nbuf->nb_length = length;
	rpcbulk_tlink_init(rbuf);
	rbuf->bb_magic = M0_RPC_BULK_BUF_MAGIC;
	return M0_RC(rc);
}

M0_INTERNAL void m0_rpc_bulk_default_cb(const struct m0_net_buffer_event *evt)
{
	struct m0_rpc_bulk	*rbulk;
	struct m0_rpc_bulk_buf	*buf;
	struct m0_net_buffer	*nb;

	M0_ENTRY("net_buf_evt: %p", evt);
	M0_PRE(evt != NULL);
	M0_PRE(evt->nbe_buffer != NULL);

	nb = evt->nbe_buffer;
	buf = (struct m0_rpc_bulk_buf *)nb->nb_app_private;
	rbulk = buf->bb_rbulk;

	M0_LOG(M0_DEBUG, "rbulk %p, nbuf %p, nbuf->nb_qtype %lu, "
	       "evt->nbe_status %d", rbulk, nb,
	       (unsigned long)nb->nb_qtype, evt->nbe_status);
	M0_ASSERT(rpc_bulk_buf_invariant(buf));
	M0_ASSERT(rpcbulk_tlink_is_in(buf));

	if (M0_IN(nb->nb_qtype, (M0_NET_QT_PASSIVE_BULK_RECV,
				 M0_NET_QT_ACTIVE_BULK_RECV)))
		nb->nb_length = evt->nbe_length;

	m0_mutex_lock(&rbulk->rb_mutex);
	M0_ASSERT_EX(rpc_bulk_invariant(rbulk));
	/*
	 * Change the status code of struct m0_rpc_bulk only if it is
	 * zero so far. This will ensure that return code of first failure
	 * from list of net buffers in struct m0_rpc_bulk will be maintained.
	 * Buffers are canceled by io coalescing code which in turn sends
	 * a coalesced buffer and cancels member buffers. Hence -ECANCELED
	 * is not treated as an error here.
	 */
	if (rbulk->rb_rc == 0 && evt->nbe_status != -ECANCELED)
		rbulk->rb_rc = evt->nbe_status;

	rpcbulk_tlist_del(buf);
	rpc_bulk_buf_fini(buf);
	if (rpcbulk_tlist_is_empty(&rbulk->rb_buflist)) {
		M0_ADDB2_ADD(M0_AVI_RPC_BULK_OP, rbulk->rb_id,
			     M0_RPC_BULK_OP_FINISH);
		if (m0_chan_has_waiters(&rbulk->rb_chan))
			m0_chan_signal(&rbulk->rb_chan);
	}
	m0_mutex_unlock(&rbulk->rb_mutex);

	M0_LEAVE("rb_rc=%d", rbulk->rb_rc);
}

M0_INTERNAL size_t m0_rpc_bulk_store_del_unqueued(struct m0_rpc_bulk *rbulk)
{
	struct m0_rpc_bulk_buf *rbuf;
	size_t                  unqueued_nr = 0;

	M0_ENTRY("rbulk %p", rbulk);
	M0_PRE(rbulk != NULL);

	if (rbulk->rb_rc != 0)
		M0_LOG(M0_ERROR, "rbulk:%p rc:%d", rbulk, rbulk->rb_rc);
	M0_PRE_EX(rpc_bulk_invariant(rbulk));
	M0_PRE(m0_chan_has_waiters(&rbulk->rb_chan));
	m0_tl_for (rpcbulk, &rbulk->rb_buflist, rbuf) {
		if (!(rbuf->bb_flags & M0_RPC_BULK_NETBUF_QUEUED)) {
			rpcbulk_tlist_del(rbuf);
			rpc_bulk_buf_fini(rbuf);
			++unqueued_nr;
		}
	} m0_tl_endfor;

	M0_LEAVE("rbulk %p, unqueued_nr %llu", rbulk,
		 (unsigned long long)unqueued_nr);
	return unqueued_nr;
}

M0_INTERNAL void m0_rpc_bulk_store_del(struct m0_rpc_bulk *rbulk)
{
	struct m0_rpc_bulk_buf *rbuf;

	M0_ENTRY("rbulk %p", rbulk);
	M0_PRE(rbulk != NULL);

	m0_mutex_lock(&rbulk->rb_mutex);

	if (rbulk->rb_rc != 0)
		M0_LOG(M0_ERROR, "rbulk:%p rc:%d", rbulk, rbulk->rb_rc);
	M0_PRE_EX(rpc_bulk_invariant(rbulk));
	M0_PRE(m0_chan_has_waiters(&rbulk->rb_chan));

	m0_tl_for (rpcbulk, &rbulk->rb_buflist, rbuf) {
		m0_net_buffer_del(rbuf->bb_nbuf, rbuf->bb_nbuf->nb_tm);
	} m0_tl_endfor;

	m0_mutex_unlock(&rbulk->rb_mutex);

	M0_LEAVE("rbulk %p", rbulk);
}

const struct m0_net_buffer_callbacks m0_rpc__buf_bulk_cb = {
	.nbc_cb = {
		[M0_NET_QT_PASSIVE_BULK_SEND] = m0_rpc_bulk_default_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = m0_rpc_bulk_default_cb,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = m0_rpc_bulk_default_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = m0_rpc_bulk_default_cb
	}
};

M0_INTERNAL void m0_rpc_bulk_init(struct m0_rpc_bulk *rbulk)
{
	M0_ENTRY("rbulk: %p", rbulk);
	M0_PRE(rbulk != NULL);

	rpcbulk_tlist_init(&rbulk->rb_buflist);
	m0_mutex_init(&rbulk->rb_mutex);
	m0_chan_init(&rbulk->rb_chan, &rbulk->rb_mutex);
	rbulk->rb_magic = M0_RPC_BULK_MAGIC;
	rbulk->rb_bytes = 0;
	rbulk->rb_rc = 0;
	rbulk->rb_id = m0_dummy_id_generate();
	M0_LEAVE();
}
M0_EXPORTED(m0_rpc_bulk_init);

M0_INTERNAL void m0_rpc_bulk_fini(struct m0_rpc_bulk *rbulk)
{
	M0_ENTRY("rbulk: %p", rbulk);
	M0_PRE(rbulk != NULL);
	m0_mutex_lock(&rbulk->rb_mutex);
	M0_PRE_EX(rpc_bulk_invariant(rbulk));
	M0_PRE(rpcbulk_tlist_is_empty(&rbulk->rb_buflist));
	m0_mutex_unlock(&rbulk->rb_mutex);

	m0_chan_fini_lock(&rbulk->rb_chan);
	m0_mutex_fini(&rbulk->rb_mutex);
	rpcbulk_tlist_fini(&rbulk->rb_buflist);
	M0_LEAVE();
}
M0_EXPORTED(m0_rpc_bulk_fini);

M0_INTERNAL void m0_rpc_bulk_buflist_empty(struct m0_rpc_bulk *rbulk)
{
	struct m0_rpc_bulk_buf *buf;

	m0_mutex_lock(&rbulk->rb_mutex);
	M0_ASSERT_EX(rpc_bulk_invariant(rbulk));
	m0_tl_teardown(rpcbulk, &rbulk->rb_buflist, buf) {
		rpc_bulk_buf_fini(buf);
	}
	m0_mutex_unlock(&rbulk->rb_mutex);
}

M0_INTERNAL int m0_rpc_bulk_buf_add(struct m0_rpc_bulk *rbulk,
				    uint32_t segs_nr, m0_bcount_t length,
				    struct m0_net_domain *netdom,
				    struct m0_net_buffer *nb,
				    struct m0_rpc_bulk_buf **out)
{
	int			rc;
	struct m0_rpc_bulk_buf *buf;

	M0_ENTRY("rbulk: %p, net_dom: %p, net_buf: %p", rbulk, netdom, nb);
	M0_PRE(rbulk != NULL);
	M0_PRE(netdom != NULL);
	M0_PRE(out != NULL);

	if (segs_nr > m0_net_domain_get_max_buffer_segments(netdom) ||
	    length > m0_net_domain_get_max_buffer_size(netdom))
		return M0_ERR_INFO(-EMSGSIZE, "Cannot exceed net_max_buf_seg");

	M0_ALLOC_PTR(buf);
	if (buf == NULL)
		return M0_ERR(-ENOMEM);

	rc = rpc_bulk_buf_init(buf, segs_nr, length, nb);
	if (rc != 0) {
		m0_free(buf);
		return M0_RC(rc);
	}

	m0_mutex_lock(&rbulk->rb_mutex);
	buf->bb_rbulk = rbulk;
	rpcbulk_tlist_add_tail(&rbulk->rb_buflist, buf);
	M0_POST_EX(rpc_bulk_invariant(rbulk));
	m0_mutex_unlock(&rbulk->rb_mutex);
	*out = buf;
	M0_POST(rpc_bulk_buf_invariant(buf));

	return M0_RC(0);
}
M0_EXPORTED(m0_rpc_bulk_buf_add);

M0_INTERNAL int m0_rpc_bulk_buf_databuf_add(struct m0_rpc_bulk_buf *rbuf,
					    void *buf,
					    m0_bcount_t count,
					    m0_bindex_t index,
					    struct m0_net_domain *netdom)
{
	int			 rc;
	struct m0_buf		 cbuf;
	struct m0_rpc_bulk	*rbulk;

	M0_ENTRY("rbuf: %p, netdom: %p", rbuf, netdom);
	M0_PRE(rbuf != NULL);
	M0_PRE(rpc_bulk_buf_invariant(rbuf));
	M0_PRE(buf != NULL);
	M0_PRE(count != 0);
	M0_PRE(netdom != NULL);

	if (rbuf->bb_zerovec.z_count + count >
	    m0_net_domain_get_max_buffer_size(netdom) ||
	    count > m0_net_domain_get_max_buffer_segment_size(netdom)) {
		M0_LOG(M0_DEBUG, "Cannot exceed net_dom_max_buf_segs");
		return M0_RC(-EMSGSIZE); /* Not an error, no M0_ERR(). */
	}

	cbuf.b_addr = buf;
	cbuf.b_nob = count;
	rbulk = rbuf->bb_rbulk;
	rc = m0_0vec_cbuf_add(&rbuf->bb_zerovec, &cbuf, &index);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Addition of cbuf");

	rbuf->bb_nbuf->nb_buffer = rbuf->bb_zerovec.z_bvec;
	M0_POST(rpc_bulk_buf_invariant(rbuf));
	m0_mutex_lock(&rbulk->rb_mutex);
	M0_POST_EX(rpc_bulk_invariant(rbulk));
	m0_mutex_unlock(&rbulk->rb_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_bulk_buf_databuf_add);

M0_INTERNAL void m0_rpc_bulk_qtype(struct m0_rpc_bulk *rbulk,
				   enum m0_net_queue_type q)
{
	struct m0_rpc_bulk_buf *rbuf;

	M0_ENTRY("rpc_bulk: %p, qtype: %d", rbulk, q);
	M0_PRE(rbulk != NULL);
	M0_PRE(m0_mutex_is_locked(&rbulk->rb_mutex));
	M0_PRE(!rpcbulk_tlist_is_empty(&rbulk->rb_buflist));
	M0_PRE(M0_IN(q, (M0_NET_QT_PASSIVE_BULK_RECV,
			 M0_NET_QT_PASSIVE_BULK_SEND,
			 M0_NET_QT_ACTIVE_BULK_RECV,
			 M0_NET_QT_ACTIVE_BULK_SEND)));

	m0_tl_for(rpcbulk, &rbulk->rb_buflist, rbuf) {
		rbuf->bb_nbuf->nb_qtype = q;
	} m0_tl_endfor;
	M0_LEAVE();
}

static void addb2_add_rpc_bulk_attr(struct m0_rpc_bulk       *rbulk,
				    enum m0_rpc_bulk_op_type  op,
				    uint32_t                  buf_nr,
				    uint64_t                  seg_nr)
{
	M0_ADDB2_ADD(M0_AVI_ATTR, rbulk->rb_id,
		     M0_AVI_RPC_BULK_ATTR_OP,
		     op);
	M0_ADDB2_ADD(M0_AVI_ATTR, rbulk->rb_id,
		     M0_AVI_RPC_BULK_ATTR_BUF_NR,
		     buf_nr);
	M0_ADDB2_ADD(M0_AVI_ATTR, rbulk->rb_id,
		     M0_AVI_RPC_BULK_ATTR_BYTES,
		     rbulk->rb_bytes);
	M0_ADDB2_ADD(M0_AVI_ATTR, rbulk->rb_id,
		     M0_AVI_RPC_BULK_ATTR_SEG_NR,
		     seg_nr);
}

static int rpc_bulk_op(struct m0_rpc_bulk                   *rbulk,
		       const struct m0_rpc_conn             *conn,
		       struct m0_net_buf_desc_data          *descs,
		       enum m0_rpc_bulk_op_type              op,
		       const struct m0_net_buffer_callbacks *bulk_cb)
{
	int				 rc = 0;
	int				 cnt = 0;
	struct m0_rpc_bulk_buf		*rbuf;
	struct m0_net_transfer_mc	*tm;
	struct m0_net_buffer		*nb;
	struct m0_net_domain		*netdom;
	struct m0_rpc_machine		*rpcmach;
	uint64_t                         seg_nr = 0;

	M0_ENTRY("rbulk: %p, rpc_conn: %p, rbulk_op_type: %d", rbulk, conn, op);
	M0_PRE(rbulk != NULL);
	M0_PRE(descs != NULL);
	M0_PRE(M0_IN(op, (M0_RPC_BULK_STORE, M0_RPC_BULK_LOAD)));
	M0_PRE(bulk_cb != NULL);

	rpcmach = conn->c_rpc_machine;
	tm = &rpcmach->rm_tm;
	netdom = tm->ntm_dom;
	m0_mutex_lock(&rbulk->rb_mutex);
	M0_ASSERT_EX(rpc_bulk_invariant(rbulk));
	M0_ASSERT(!rpcbulk_tlist_is_empty(&rbulk->rb_buflist));

	M0_ADDB2_ADD(M0_AVI_RPC_BULK_OP, rbulk->rb_id,
		     M0_RPC_BULK_OP_INITIATE);

	m0_tl_for(rpcbulk, &rbulk->rb_buflist, rbuf) {
		nb = rbuf->bb_nbuf;
		if (nb->nb_length == 0)
			nb->nb_length =
				m0_vec_count(&rbuf->bb_zerovec.z_bvec.ov_vec);
		M0_ASSERT(rpc_bulk_buf_invariant(rbuf));
		M0_ASSERT(ergo(op == M0_RPC_BULK_STORE, M0_IN(nb->nb_qtype,
				            (M0_NET_QT_PASSIVE_BULK_RECV,
				             M0_NET_QT_PASSIVE_BULK_SEND))) &&
			  ergo(op == M0_RPC_BULK_LOAD, M0_IN(nb->nb_qtype,
					    (M0_NET_QT_ACTIVE_BULK_RECV,
					     M0_NET_QT_ACTIVE_BULK_SEND))));
		nb->nb_callbacks = bulk_cb;

		/*
		 * Registers the net buffer with net domain if it is not
		 * registered already.
		 */
		if (!(nb->nb_flags & M0_NET_BUF_REGISTERED)) {
			rc = m0_net_buffer_register(nb, netdom);
			if (rc != 0)
				goto cleanup;
			rbuf->bb_flags |= M0_RPC_BULK_NETBUF_REGISTERED;
		}
		nb->nb_timeout = m0_time_from_now(M0_RPC_BULK_TMO, 0);
		if (M0_FI_ENABLED("timeout_2s"))
			nb->nb_timeout = m0_time_from_now(2, 0);

		if (op == M0_RPC_BULK_LOAD) {
			rc = m0_net_desc_copy(&descs[cnt].bdd_desc,
					      &nb->nb_desc);
			if (rc != 0)
				goto cleanup;
		}

		nb->nb_app_private = rbuf;
		rc = m0_net_buffer_add(nb, tm);
		if (rc != 0)
			goto cleanup;
		rbuf->bb_flags |= M0_RPC_BULK_NETBUF_QUEUED;

		if (op == M0_RPC_BULK_STORE) {
			rc = m0_net_desc_copy(&nb->nb_desc,
					      &descs[cnt].bdd_desc);
			if (rc != 0) {
				m0_net_buffer_del(nb, tm);
				rbuf->bb_flags &= ~M0_RPC_BULK_NETBUF_QUEUED;
				goto cleanup;
			}
			descs[cnt].bdd_used = nb->nb_length;
		}

		seg_nr += rbuf->bb_zerovec.z_bvec.ov_vec.v_nr;

		rbulk->rb_bytes += nb->nb_length;
		++cnt;
	} m0_tl_endfor;
	addb2_add_rpc_bulk_attr(rbulk, op, cnt, seg_nr);
	M0_POST_EX(rpc_bulk_invariant(rbulk));
	m0_mutex_unlock(&rbulk->rb_mutex);

	return M0_RC(rc);
cleanup:
	M0_ASSERT(rc != 0);

	M0_LOG(M0_DEBUG, "rbulk %p, rc %d", rbulk, rc);
	rpcbulk_tlist_del(rbuf);
	m0_tl_for(rpcbulk, &rbulk->rb_buflist, rbuf) {
		if (rbuf->bb_flags & M0_RPC_BULK_NETBUF_QUEUED) {
			m0_net_buffer_del(rbuf->bb_nbuf, tm);
			rbuf->bb_flags &= ~M0_RPC_BULK_NETBUF_QUEUED;
		}
	} m0_tl_endfor;
	m0_mutex_unlock(&rbulk->rb_mutex);
	return M0_ERR(rc);
}

M0_INTERNAL int
m0_rpc_bulk_store(struct m0_rpc_bulk                   *rbulk,
		  const struct m0_rpc_conn             *conn,
		  struct m0_net_buf_desc_data          *to_desc,
		  const struct m0_net_buffer_callbacks *bulk_cb)
{
	return rpc_bulk_op(rbulk, conn, to_desc, M0_RPC_BULK_STORE, bulk_cb);
}
M0_EXPORTED(m0_rpc_bulk_store);

M0_INTERNAL int
m0_rpc_bulk_load(struct m0_rpc_bulk                   *rbulk,
		 const struct m0_rpc_conn             *conn,
		 struct m0_net_buf_desc_data          *from_desc,
		 const struct m0_net_buffer_callbacks *bulk_cb)
{
	return rpc_bulk_op(rbulk, conn, from_desc, M0_RPC_BULK_LOAD, bulk_cb);
}
M0_EXPORTED(m0_rpc_bulk_load);

M0_INTERNAL bool m0_rpc_bulk_is_empty(struct m0_rpc_bulk *rbulk)
{
	bool empty;

	m0_mutex_lock(&rbulk->rb_mutex);
	empty = rpcbulk_tlist_is_empty(&rbulk->rb_buflist);
	m0_mutex_unlock(&rbulk->rb_mutex);

	return empty;
}

M0_INTERNAL size_t m0_rpc_bulk_buf_length(struct m0_rpc_bulk *rbulk)
{
	size_t buf_nr;

	m0_mutex_lock(&rbulk->rb_mutex);
	buf_nr = rpcbulk_tlist_length(&rbulk->rb_buflist);
	m0_mutex_unlock(&rbulk->rb_mutex);
	return buf_nr;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of rpc group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
