/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 20-May-2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/chan.h"        /* m0_clink */
#include "lib/vec.h"
#include "sm/sm.h"           /* m0_sm_ast */
#include "net/net.h"         /* m0_net_queue_type */
#include "rpc/session.h"     /* m0_rpc_session */
#include "rpc/bulk.h"        /* m0_rpc_bulk */
#include "rpc/rpc_machine.h" /* m0_rpc_machine */
#include "rpc/conn.h"
#include "fop/fom.h"
#include "fop/fop.h"
#include "rpc/at.h"

/**
 * @addtogroup rpc-at
 *
 * @{
 */

M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);
M0_TL_DECLARE(rpcbulk, M0_INTERNAL, struct m0_rpc_bulk_buf);

struct rpc_at_bulk {
	/** Back link to the AT buffer. */
	struct m0_rpc_at_buf     *ac_atbuf;
	struct m0_rpc_bulk        ac_bulk;
	const struct m0_rpc_conn *ac_conn;
	struct m0_net_buffer      ac_nb;
	/** A single buffer where received buffer vector via bulk is spliced. */
	struct m0_buf             ac_recv;
	/**
	 * Clink to wait when transmission from server to client is complete.
	 */
	struct m0_clink           ac_clink;
	/**
	 * Fom to wakeup once bulk transmission from server to client is
	 * complete.
	 */
	struct m0_fom            *ac_user_fom;
	/**
	 * AST to be executed after transmission from server to client is
	 * complete.
	 */
	struct m0_sm_ast          ac_ast;
	/** Result of a bulk transmission. */
	int                       ac_rc;
};

static struct rpc_at_bulk *rpc_at_bulk(const struct m0_rpc_at_buf *ab)
{
	return ab->u.ab_extra.abr_bulk;
}

static m0_bcount_t rpc_at_bulk_cutoff(const struct m0_rpc_conn *conn)
{
	return conn->c_rpc_machine->rm_bulk_cutoff;
}

static struct m0_net_domain *rpc_at_bulk_ndom(const struct rpc_at_bulk *atbulk)
{
	return atbulk->ac_conn->c_rpc_machine->rm_tm.ntm_dom;
}

static struct m0_rpc_conn *fom_conn(const struct m0_fom *fom)
{
	return fom->fo_fop->f_item.ri_session->s_conn;
}

static uint64_t rpc_at_bulk_segs_nr(const struct rpc_at_bulk *atbulk,
				    m0_bcount_t               data_size,
				    m0_bcount_t              *seg_size)
{
	*seg_size = m0_net_domain_get_max_buffer_segment_size(
			rpc_at_bulk_ndom(atbulk));
	/*
	 * Arbitrary select 1MB as the maximum segment size.
	 */
	*seg_size = min64u(*seg_size, 1024*1024);
	return (data_size + *seg_size - 1) / *seg_size;
}

static int rpc_at_bulk_nb_alloc(struct m0_rpc_at_buf *ab, uint64_t size)
{
	struct rpc_at_bulk   *atbulk = rpc_at_bulk(ab);
	struct m0_net_buffer *nb     = &atbulk->ac_nb;
	struct m0_bufvec     *bvec   = &nb->nb_buffer;
	m0_bcount_t           seg_size;
	uint64_t              segs_nr;
	int                   rc;

	M0_ASSERT(M0_IN(ab->ab_type, (M0_RPC_AT_BULK_SEND,
				      M0_RPC_AT_BULK_RECV)));
	segs_nr = rpc_at_bulk_segs_nr(atbulk, size, &seg_size);
	rc = m0_bufvec_alloc_aligned(bvec, segs_nr, seg_size, PAGE_SHIFT);
	if (rc == 0)
		nb->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	return M0_RC(rc);
}

static void rpc_at_bulk_nb_free(struct rpc_at_bulk *atbulk)
{
	m0_bufvec_free_aligned(&atbulk->ac_nb.nb_buffer, PAGE_SHIFT);
}

static int rpc_at_bulk_init(struct m0_rpc_at_buf     *ab,
			    const struct m0_rpc_conn *conn)
{
	struct rpc_at_bulk *atbulk;

	M0_PRE(ab   != NULL);
	M0_PRE(conn != NULL);

	M0_ENTRY();
	M0_ALLOC_PTR(atbulk);
	if (atbulk == NULL)
		return M0_ERR(-ENOMEM);
	ab->u.ab_extra.abr_bulk = atbulk;
	atbulk->ac_atbuf = ab;
	atbulk->ac_conn = conn;
	atbulk->ac_rc = 0;
	atbulk->ac_recv = M0_BUF_INIT0;
	m0_rpc_bulk_init(&atbulk->ac_bulk);
	return M0_RC(0);
}

static void rpc_at_bulk_store_del(struct m0_rpc_bulk *rbulk)
{
	struct m0_clink clink;

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&rbulk->rb_chan, &clink);
	m0_rpc_bulk_store_del(rbulk);
	m0_chan_wait(&clink);
	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);
}

static enum m0_net_queue_type rpc_at_bulk_qtype(struct m0_rpc_bulk *rbulk)
{
	struct m0_rpc_bulk_buf *rbuf;
	enum m0_net_queue_type  ret;

	M0_PRE(!m0_rpc_bulk_is_empty(rbulk));

	m0_mutex_lock(&rbulk->rb_mutex);
	rbuf = rpcbulk_tlist_head(&rbulk->rb_buflist);
	ret = rbuf->bb_nbuf->nb_qtype;

	/* Check that all elements have the same type. */
	M0_ASSERT(m0_tl_forall(rpcbulk, rbuf, &rbulk->rb_buflist,
			       rbuf->bb_nbuf->nb_qtype == ret));
	m0_mutex_unlock(&rbulk->rb_mutex);
	return ret;
}

static void rpc_at_bulk_fini(struct rpc_at_bulk *atbulk)
{
	struct m0_rpc_bulk *rbulk = &atbulk->ac_bulk;

	M0_PRE(atbulk != NULL);
	M0_PRE(rbulk  != NULL);

	if (!m0_rpc_bulk_is_empty(rbulk) &&
	    M0_IN(rpc_at_bulk_qtype(rbulk), (M0_NET_QT_PASSIVE_BULK_SEND,
			                     M0_NET_QT_PASSIVE_BULK_RECV)))
		rpc_at_bulk_store_del(rbulk);
	m0_rpc_bulk_buflist_empty(rbulk);
	m0_rpc_bulk_fini(rbulk);
	if (m0_buf_is_set(&atbulk->ac_recv))
		m0_buf_free(&atbulk->ac_recv);
	rpc_at_bulk_nb_free(atbulk);
	m0_free(atbulk);
}

static int rpc_at_bulk_csend(struct m0_rpc_at_buf *ab,
			     const struct m0_buf  *buf)
{
	struct rpc_at_bulk     *atbulk = rpc_at_bulk(ab);
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_net_domain   *nd;
	uint64_t                segs_nr;
	int                     i;
	m0_bcount_t             seg_size;
	m0_bcount_t             blen = buf->b_nob;
	int                     rc;

	ab->ab_type = M0_RPC_AT_BULK_SEND;
	nd = rpc_at_bulk_ndom(atbulk);
	segs_nr = rpc_at_bulk_segs_nr(atbulk, blen, &seg_size);
	rc = m0_rpc_bulk_buf_add(&atbulk->ac_bulk, segs_nr, blen,
				 nd, NULL, &rbuf);
	if (rc == 0) {
		for (i = 0; i < segs_nr; ++i) {
			m0_rpc_bulk_buf_databuf_add(rbuf,
					buf->b_addr + i * seg_size,
					min_check(blen, seg_size), i, nd);
			blen -= seg_size;
		}
		rbuf->bb_nbuf->nb_qtype = M0_NET_QT_PASSIVE_BULK_SEND;
		rc = m0_rpc_bulk_store(&atbulk->ac_bulk,
				       atbulk->ac_conn,
				       &ab->u.ab_send,
				       &m0_rpc__buf_bulk_cb);
	}
	return M0_RC(rc);
}

static int rpc_at_bulk_srecv(struct m0_rpc_at_buf *ab, struct m0_fom *fom)
{
	struct rpc_at_bulk     *atbulk = rpc_at_bulk(ab);
	struct m0_rpc_bulk     *rbulk = &atbulk->ac_bulk;
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_net_domain   *nd;
	uint64_t                segs_nr;
	m0_bcount_t             seg_size;
	uint64_t                size = ab->u.ab_send.bdd_used;
	int                     rc;

	M0_PRE(ab->ab_type == M0_RPC_AT_BULK_SEND);
	nd = rpc_at_bulk_ndom(atbulk);
	segs_nr = rpc_at_bulk_segs_nr(atbulk, size, &seg_size);
	rc = rpc_at_bulk_nb_alloc(ab, size) ?:
		m0_rpc_bulk_buf_add(rbulk, segs_nr, size,
				    nd, &atbulk->ac_nb, &rbuf);
	if (rc == 0) {
		m0_mutex_lock(&rbulk->rb_mutex);
		m0_fom_wait_on(fom, &rbulk->rb_chan, &fom->fo_cb);
		m0_mutex_unlock(&rbulk->rb_mutex);

		rc = m0_rpc_bulk_load(rbulk, fom_conn(fom), &ab->u.ab_send,
				      &m0_rpc__buf_bulk_cb);
		if (rc != 0) {
			m0_mutex_lock(&rbulk->rb_mutex);
			m0_fom_callback_cancel(&fom->fo_cb);
			m0_mutex_unlock(&rbulk->rb_mutex);
			m0_rpc_bulk_buflist_empty(rbulk);
		}
	}
	if (rc != 0)
		atbulk->ac_rc = M0_ERR(rc);
	return M0_RC(rc);
}

static int rpc_at_bulk_srecv_rc(const struct m0_rpc_at_buf *ab,
				struct m0_buf              *buf)
{
	struct rpc_at_bulk *atbulk = rpc_at_bulk(ab);
	struct m0_bufvec   *bvec = &atbulk->ac_nb.nb_buffer;
	struct m0_buf      *recv = &atbulk->ac_recv;
	int                 rc;

	rc = atbulk->ac_rc ?:
	     atbulk->ac_bulk.rb_rc;
	if (rc == 0 && !m0_buf_is_set(recv))
		rc = m0_bufvec_splice(bvec, atbulk->ac_nb.nb_length, recv);
	if (rc == 0)
		*buf = *recv;
	return M0_RC(rc);
}

static int rpc_at_bulk_crecv(struct m0_rpc_at_buf *ab,
			     uint32_t              len)
{
	struct rpc_at_bulk     *atbulk = rpc_at_bulk(ab);
	struct m0_rpc_bulk     *rbulk  = &atbulk->ac_bulk;
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_net_domain   *nd;
	uint64_t                segs_nr;
	m0_bcount_t             seg_size;
	int                     rc;

	ab->ab_type = M0_RPC_AT_BULK_RECV;
	nd = rpc_at_bulk_ndom(atbulk);
	segs_nr = rpc_at_bulk_segs_nr(atbulk, len, &seg_size);
	rc = rpc_at_bulk_nb_alloc(ab, len) ?:
		m0_rpc_bulk_buf_add(rbulk, segs_nr, len, nd,
				    &atbulk->ac_nb, &rbuf);
	if (rc == 0) {
		rbuf->bb_nbuf->nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
		rc = m0_rpc_bulk_store(rbulk, atbulk->ac_conn, &ab->u.ab_recv,
				       &m0_rpc__buf_bulk_cb);
		if (rc != 0)
			m0_rpc_bulk_buflist_empty(rbulk);
	}
	return M0_RC(rc);
}

static void rpc_at_ssend_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct rpc_at_bulk     *atbulk   = M0_AMB(atbulk, ast, ac_ast);
	struct m0_rpc_at_buf   *atbuf    = atbulk->ac_atbuf;
	struct m0_rpc_at_extra *atextra  = &atbuf->u.ab_extra;
	struct m0_fom          *user_fom = atbulk->ac_user_fom;

	rpc_at_bulk_fini(atbulk);
	atextra->abr_bulk = NULL;
	M0_ASSERT(m0_buf_is_set(&atextra->abr_user_buf));
	m0_buf_free(&atextra->abr_user_buf);
	M0_ASSERT(user_fom != NULL);
	m0_fom_wakeup(user_fom);
}

static bool rpc_at_ssend_complete_cb(struct m0_clink *clink)
{
	struct rpc_at_bulk   *atbulk = M0_AMB(atbulk, clink, ac_clink);
	struct m0_rpc_at_buf *atbuf  = atbulk->ac_atbuf;

	atbuf->u.ab_rep.abr_rc = atbulk->ac_bulk.rb_rc;
	m0_clink_del(clink);
	m0_clink_fini(clink);
	atbulk->ac_ast.sa_cb = rpc_at_ssend_ast_cb;
	m0_sm_ast_post(&atbulk->ac_user_fom->fo_loc->fl_group, &atbulk->ac_ast);
	return true;
}

static int rpc_at_bulk_ssend(struct m0_rpc_at_buf *in,
			     struct m0_rpc_at_buf *out,
			     struct m0_buf        *buf,
			     struct m0_fom        *fom)
{
	struct rpc_at_bulk     *atbulk = rpc_at_bulk(out);
	struct m0_rpc_bulk     *rbulk = &atbulk->ac_bulk;
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_net_domain   *nd;
	uint64_t                segs_nr;
	m0_bcount_t             seg_size;
	m0_bcount_t             blen = buf->b_nob;
	struct m0_clink        *clink = &atbulk->ac_clink;
	int                     i;
	int                     rc;

	M0_PRE(in != NULL);
	M0_PRE(in->ab_type == M0_RPC_AT_BULK_RECV);
	M0_PRE(out->ab_type == M0_RPC_AT_BULK_REP);
	M0_PRE(in->u.ab_recv.bdd_used >= blen);
	nd = rpc_at_bulk_ndom(atbulk);
	segs_nr = rpc_at_bulk_segs_nr(atbulk, blen, &seg_size);
	rc = m0_rpc_bulk_buf_add(&atbulk->ac_bulk, segs_nr, blen,
				 nd, NULL, &rbuf);
	if (rc == 0) {
		for (i = 0; i < segs_nr; ++i) {
			m0_rpc_bulk_buf_databuf_add(rbuf,
					buf->b_addr + i * seg_size,
					min_check(blen, seg_size), i, nd);
			blen -= seg_size;
		}

		m0_clink_init(clink, rpc_at_ssend_complete_cb);
		m0_clink_add_lock(&rbulk->rb_chan, clink);
		atbulk->ac_user_fom = fom;

		rbuf->bb_nbuf->nb_qtype = M0_NET_QT_ACTIVE_BULK_SEND;
		rc = m0_rpc_bulk_load(rbulk, fom_conn(fom), &in->u.ab_recv,
			              &m0_rpc__buf_bulk_cb);
		if (rc != 0) {
			m0_clink_del_lock(clink);
			m0_clink_fini(clink);
			m0_rpc_bulk_buflist_empty(rbulk);
		} else {
			/*
			 * Store reference to the user buffer to free it when
			 * transmission is complete.
			 */
			out->u.ab_extra.abr_user_buf = *buf;
		}
	}
	if (rc != 0)
		atbulk->ac_rc = M0_ERR(rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_at_get(const struct m0_rpc_at_buf *ab,
			      struct m0_buf              *buf)
{
	switch (ab->ab_type) {
	case M0_RPC_AT_INLINE:
		*buf = ab->u.ab_buf;
		return 0;
	case M0_RPC_AT_BULK_SEND:
		return rpc_at_bulk_srecv_rc(ab, buf);
	default:
		return M0_ERR_INFO(-EPROTO, "Incorrect AT type %u",
				   ab->ab_type);
	}
}

M0_INTERNAL int m0_rpc_at_load(struct m0_rpc_at_buf *ab, struct m0_fom *fom,
			       int next_phase)
{
	struct m0_rpc_conn *conn = fom_conn(fom);
	int                 result = M0_FSO_AGAIN;
	int                 rc;

	m0_fom_phase_set(fom, next_phase);

	/* ab is probably received from network, don't assert on its data. */
	if (ab->ab_type == M0_RPC_AT_BULK_SEND) {
		rc = rpc_at_bulk_init(ab, conn) ?:
		     rpc_at_bulk_srecv(ab, fom);
		if (rc == 0)
			result = M0_FSO_WAIT;
	}
	return result;
}

M0_INTERNAL void m0_rpc_at_init(struct m0_rpc_at_buf *ab)
{
	/** @todo M0_PRE(M0_IS0(ab)); */
	ab->ab_type = M0_RPC_AT_EMPTY;
	ab->u.ab_extra.abr_user_buf = M0_BUF_INIT0;
	ab->u.ab_extra.abr_bulk = NULL;
}

M0_INTERNAL void m0_rpc_at_fini(struct m0_rpc_at_buf *ab)
{
	switch (ab->ab_type) {
	case M0_RPC_AT_INLINE:
		m0_buf_free(&ab->u.ab_buf);
		break;
	case M0_RPC_AT_BULK_SEND:
		m0_net_desc_free(&ab->u.ab_send.bdd_desc);
		break;
	case M0_RPC_AT_BULK_RECV:
		m0_net_desc_free(&ab->u.ab_recv.bdd_desc);
		break;
	}

	if (m0_buf_is_set(&ab->u.ab_extra.abr_user_buf))
		m0_buf_free(&ab->u.ab_extra.abr_user_buf);
	if (rpc_at_bulk(ab) != NULL)
		rpc_at_bulk_fini(rpc_at_bulk(ab));
	ab->ab_type = M0_RPC_AT_EMPTY;
}

M0_INTERNAL int m0_rpc_at_add(struct m0_rpc_at_buf     *ab,
			      const struct m0_buf      *buf,
			      const struct m0_rpc_conn *conn)
{
	m0_bcount_t blen = buf->b_nob;
	int         rc   = 0;

	M0_ENTRY();
	M0_PRE(ab   != NULL);
	M0_PRE(buf  != NULL);
	M0_PRE(conn != NULL);

	/*
	 * If buffer is too big to fit in FOP, but inbulk is impossible due to
	 * non-aligned address, then error will be returned during FOP
	 * transmission.
	 */
	if (blen < rpc_at_bulk_cutoff(conn) ||
	    !m0_addr_is_aligned(buf->b_addr, PAGE_SHIFT)) {
		ab->ab_type = M0_RPC_AT_INLINE;
		ab->u.ab_buf = *buf;
	} else {
		rc = rpc_at_bulk_init(ab, conn) ?:
		     rpc_at_bulk_csend(ab, buf);
		if (rc == 0)
			ab->u.ab_extra.abr_user_buf = *buf;
	}
	return M0_RC(rc);
}

M0_INTERNAL bool m0_rpc_at_is_set(const struct m0_rpc_at_buf *ab)
{
	switch (ab->ab_type) {
	case M0_RPC_AT_EMPTY:
	case M0_RPC_AT_BULK_RECV:
		return false;
	case M0_RPC_AT_INLINE:
	case M0_RPC_AT_BULK_SEND:
		return true;
	case M0_RPC_AT_BULK_REP:
		return m0_rpc_at_len(ab) > 0;
	default:
		M0_IMPOSSIBLE("Incorrect AT type");
	}
}

M0_INTERNAL int m0_rpc_at_recv(struct m0_rpc_at_buf     *ab,
			       const struct m0_rpc_conn *conn,
			       uint32_t                  len,
			       bool                      force_bulk)
{
	int rc = 0;

	M0_PRE(!m0_rpc_at_is_set(ab));
	M0_PRE(conn != NULL);

	if ((len == M0_RPC_AT_UNKNOWN_LEN ||
	     len < rpc_at_bulk_cutoff(conn)) &&
	    !force_bulk)
		ab->ab_type = M0_RPC_AT_EMPTY;
	else
		rc = rpc_at_bulk_init(ab, conn) ?:
		     rpc_at_bulk_crecv(ab, len);
	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_at_reply(struct m0_rpc_at_buf *in,
				struct m0_rpc_at_buf *out,
				struct m0_buf        *repbuf,
				struct m0_fom        *fom,
				int                   next_phase)
{
	const struct m0_rpc_conn *conn     = fom_conn(fom);
	m0_bcount_t               blen     = repbuf->b_nob;
	bool                      use_bulk = false;
	int                       result   = M0_FSO_AGAIN;
	int                       rc       = 0;

	M0_PRE(!m0_rpc_at_is_set(out));

	m0_fom_phase_set(fom, next_phase);

	if (blen < rpc_at_bulk_cutoff(conn)) {
		if (in != NULL && in->ab_type == M0_RPC_AT_BULK_RECV) {
			use_bulk = true;
		} else if (in == NULL || in->ab_type == M0_RPC_AT_EMPTY) {
			out->ab_type = M0_RPC_AT_INLINE;
			out->u.ab_buf = *repbuf;
		} else {
			rc = M0_ERR(-EPROTO);
		}
	} else {
		use_bulk = true;
	}

	if (use_bulk) {
		out->ab_type = M0_RPC_AT_BULK_REP;
		if (!m0_buf_is_set(repbuf))
			rc = M0_ERR(-ENODATA);
		if (rc == 0) {
			if (in != NULL && in->ab_type == M0_RPC_AT_BULK_RECV &&
			    in->u.ab_recv.bdd_used >= repbuf->b_nob) {
				rc = rpc_at_bulk_init(out, conn);
				if (rc == 0) {
				     rpc_at_bulk_ssend(in, out, repbuf, fom);
				     if (rc != 0)
					     rpc_at_bulk_fini(rpc_at_bulk(out));
				}
			} else {
				rc = -ENOMSG; /* Not really a error. */
			}
		}
		out->u.ab_rep.abr_rc  = rc;
		out->u.ab_rep.abr_len = repbuf->b_nob;
		result = rc == 0 ? M0_FSO_WAIT : M0_FSO_AGAIN;
	}
	if (rc != 0)
		m0_buf_free(repbuf);
	return result;
}

M0_INTERNAL int m0_rpc_at_reply_rc(struct m0_rpc_at_buf *out)
{
	int rc = 0;

	M0_ASSERT(M0_IN(out->ab_type, (M0_RPC_AT_EMPTY,
				       M0_RPC_AT_INLINE,
				       M0_RPC_AT_BULK_REP)));
	/*
	 * AT bulk structure is already deallocated or wasn't ever allocated.
	 * User calling m0_rpc_at_reply() shouldn't call m0_rpc_at_fini()
	 * afterwards, so AT bulk structure should be de-allocated to not cause
	 * memory leaks.
	 */
	M0_ASSERT(rpc_at_bulk(out) == NULL);

	if (out->ab_type == M0_RPC_AT_EMPTY)
		return M0_ERR(-EPROTO);

	if (out->ab_type == M0_RPC_AT_BULK_REP)
		rc = out->u.ab_rep.abr_rc;
	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_at_rep_get(struct m0_rpc_at_buf *sent,
				  struct m0_rpc_at_buf *rcvd,
				  struct m0_buf        *out)
{
	struct rpc_at_bulk *atbulk;
	struct m0_bufvec   *bvec;
	struct m0_buf      *recv;
	int                 rc = 0;

	M0_PRE(sent == NULL || M0_IN(sent->ab_type,
				     (M0_RPC_AT_BULK_RECV, M0_RPC_AT_EMPTY)));

	if (!M0_IN(rcvd->ab_type, (M0_RPC_AT_EMPTY, M0_RPC_AT_INLINE,
				   M0_RPC_AT_BULK_REP)))
		return M0_ERR_INFO(-EPROTO, "Incorrect AT type rcvd %u",
				   rcvd->ab_type);

	if (rcvd->ab_type == M0_RPC_AT_EMPTY) {
		*out = M0_BUF_INIT0;
	} else if (rcvd->ab_type == M0_RPC_AT_INLINE) {
		*out = rcvd->u.ab_buf;
	} else {
		rc = rcvd->u.ab_rep.abr_rc;
		if (rc == 0) {
			if (sent != NULL &&
			    sent->ab_type == M0_RPC_AT_BULK_RECV) {
					atbulk = rpc_at_bulk(sent);
					bvec = &atbulk->ac_nb.nb_buffer;
					recv = &atbulk->ac_recv;
					if (!m0_buf_is_set(recv))
						rc = m0_bufvec_splice(bvec,
						      atbulk->ac_nb.nb_length,
						      recv);
					if (rc == 0)
						*out = *recv;
			} else {
				return M0_ERR_INFO(-EPROTO,
					"AT type mismatch rcvd %u",
					rcvd->ab_type);
			}
		}
	}
	return M0_RC(rc);
}

M0_INTERNAL bool m0_rpc_at_rep_is_bulk(const struct m0_rpc_at_buf *rcvd,
				       uint64_t                   *len)
{
	if (rcvd->ab_type == M0_RPC_AT_BULK_REP) {
		*len = rcvd->u.ab_rep.abr_len;
		return true;
	} else {
		*len = M0_RPC_AT_UNKNOWN_LEN;
		return false;
	}
}

M0_INTERNAL int m0_rpc_at_rep2inline(struct m0_rpc_at_buf *sent,
				     struct m0_rpc_at_buf *rcvd)
{
	struct rpc_at_bulk     *atbulk;
	const struct m0_bufvec *bvec;
	int                     rc;

	if (rcvd->ab_type == M0_RPC_AT_INLINE) {
		rc = 0;
	} else if (rcvd->ab_type == M0_RPC_AT_BULK_REP) {
		rc = rcvd->u.ab_rep.abr_rc;
		if (rc == 0) {
			if (sent->ab_type == M0_RPC_AT_BULK_RECV) {
				atbulk = rpc_at_bulk(sent);
				bvec = &atbulk->ac_nb.nb_buffer;
				rc = m0_bufvec_splice(bvec,
						      atbulk->ac_nb.nb_length,
						      &rcvd->u.ab_buf);
				if (rc == 0)
					rcvd->ab_type = M0_RPC_AT_INLINE;
			} else {
				rc = M0_ERR(-EPROTO);
			}
		}
	} else {
		rc = -EPROTO;
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_rpc_at_detach(struct m0_rpc_at_buf *ab)
{
	struct rpc_at_bulk *atbulk;

	switch (ab->ab_type) {
	case M0_RPC_AT_INLINE:
		ab->u.ab_buf = M0_BUF_INIT0;
		break;
	case M0_RPC_AT_BULK_RECV:
		atbulk = rpc_at_bulk(ab);
		atbulk->ac_recv = M0_BUF_INIT0;
	}
	ab->u.ab_extra.abr_user_buf = M0_BUF_INIT0;
}

M0_INTERNAL uint64_t m0_rpc_at_len(const struct m0_rpc_at_buf *ab)
{
	uint64_t ret;

	switch (ab->ab_type) {
	case M0_RPC_AT_EMPTY:
		ret = 0;
		break;
	case M0_RPC_AT_INLINE:
		ret = ab->u.ab_buf.b_nob;
		break;
	case M0_RPC_AT_BULK_SEND:
		ret = ab->u.ab_send.bdd_used;
		break;
	case M0_RPC_AT_BULK_RECV:
		ret = ab->u.ab_recv.bdd_used;
		break;
	case M0_RPC_AT_BULK_REP:
		ret = ab->u.ab_rep.abr_len;
		break;
	default:
		M0_IMPOSSIBLE("Incorrect AT buf type");
	}
	return ret;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of rpc-at group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
