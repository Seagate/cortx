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
 * Original author: Amit Jambure <amit_jambure@xyratex.com>
 * Original creation date: 06/04/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/errno.h"
#include "lib/arith.h"
#include "lib/bob.h"
#include "lib/finject.h"

#include "mero/magic.h"
#include "net/net.h"
#include "rpc/rpc_internal.h"
#include "rpc/service.h"

/**
 * @addtogroup rpc
 * @{
 */

static int packet_ready(struct m0_rpc_packet *p);

static int net_buffer_allocate(struct m0_net_buffer *netbuf,
			       struct m0_net_domain *ndom,
			       m0_bcount_t           buf_size);

static void net_buffer_free(struct m0_net_buffer *netbuf,
			    struct m0_net_domain *ndom);

static void bufvec_geometry(struct m0_net_domain *ndom,
			    m0_bcount_t           buf_size,
			    int32_t              *out_nr_segments,
			    m0_bcount_t          *out_segment_size);

static void item_done(struct m0_rpc_packet *p, struct m0_rpc_item *item,
		      int rc);
static void item_fail(struct m0_rpc_packet *p, struct m0_rpc_item *item,
		      int rc);
static void item_sent(struct m0_rpc_item *item);

/*
 * This is the only symbol exported from this file.
 */
const struct m0_rpc_frm_ops m0_rpc_frm_default_ops = {
	.fo_packet_ready = packet_ready,
};

enum { M0_RPC_TMO = 2 };

/**
   RPC layer's wrapper on m0_net_buffer. rpc_buffer carries one packet at
   a time.

   NOTE: rpc_buffer and packet are kept separate, even though their
         lifetime is same at this point.
         A buffer can carry only one packet at some point. There is a limit
         on number of packets that can be "in-flight". In near future we
         might want to keep per m0_rpc_chan buffer pool with few pre-allocated
         buffers.  In that case same buffer will carry many packets during
         its lifetime (but only one at a time).
 */
struct rpc_buffer {
	struct m0_net_buffer   rb_netbuf;
	struct m0_rpc_packet  *rb_packet;
	/** see M0_RPC_BUF_MAGIC */
	uint64_t               rb_magic;
};

static const struct m0_bob_type rpc_buffer_bob_type = {
	.bt_name         = "rpc_buffer",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct rpc_buffer, rb_magic),
	.bt_magix        = M0_RPC_BUF_MAGIC,
	.bt_check        = NULL,
};

M0_BOB_DEFINE(static, &rpc_buffer_bob_type, rpc_buffer);

static int rpc_buffer_init(struct rpc_buffer    *rpcbuf,
			   struct m0_rpc_packet *p);

static int rpc_buffer_submit(struct rpc_buffer *rpcbuf);

static void rpc_buffer_fini(struct rpc_buffer *rpcbuf);

static void buf_send_cb(const struct m0_net_buffer_event *ev);

static const struct m0_net_buffer_callbacks rpc_buf_send_cb = {
	.nbc_cb = {
		[M0_NET_QT_MSG_SEND] = buf_send_cb
	}
};

static struct m0_rpc_machine *
rpc_buffer__rmachine(const struct rpc_buffer *rpcbuf)
{
	struct m0_rpc_machine *rmachine;

	M0_PRE(rpc_buffer_bob_check(rpcbuf) &&
	       rpcbuf->rb_packet != NULL &&
	       rpcbuf->rb_packet->rp_frm != NULL);

	rmachine = frm_rmachine(rpcbuf->rb_packet->rp_frm);
	M0_ASSERT(rmachine != NULL);

	return rmachine;
}

/**
   Serialises packet p and its items in a network buffer and submits it to
   network layer.

   @see m0_rpc_frm_ops::fo_packet_ready()
 */
static int packet_ready(struct m0_rpc_packet *p)
{
	struct rpc_buffer *rpcbuf;
	int                rc;

	M0_ENTRY("packet: %p", p);
	M0_PRE(m0_rpc_packet_invariant(p));

	M0_ALLOC_PTR(rpcbuf);
	if (rpcbuf == NULL) {
		rc = M0_ERR(-ENOMEM);
		M0_LOG(M0_ERROR, "Failed to allocate rpcbuf");
		goto err;
	}
	rc = rpc_buffer_init(rpcbuf, p);
	if (rc != 0)
		goto err_free;

	if (M0_FI_ENABLED("set_reply_error")) {
		struct m0_rpc_item *item;

		for_each_item_in_packet(item, p) {
			if (m0_rpc_item_is_reply(item)) {
				rc = -ENETDOWN;
				M0_LOG(M0_ERROR, "packet %p, item %p[%"PRIu32"]"
				       " set error to %d", p, item,
				       item->ri_type->rit_opcode, rc);
				goto out;
			}
		} end_for_each_item_in_packet;
	}

	rc = rpc_buffer_submit(rpcbuf);
	if (rc == 0)
		return M0_RC(rc);
out:
	rpc_buffer_fini(rpcbuf);
err_free:
	m0_free(rpcbuf);
err:
	m0_rpc_packet_traverse_items(p, item_fail, rc);
	m0_rpc_packet_discard(p);

	return M0_RC(rc);
}

/**
   Initialises rpcbuf, allocates network buffer of size enough to
   accomodate serialised packet p.
 */
static int rpc_buffer_init(struct rpc_buffer    *rpcbuf,
			   struct m0_rpc_packet *p)
{
	struct m0_net_buffer  *netbuf;
	struct m0_net_domain  *ndom;
	struct m0_rpc_machine *machine;
	struct m0_rpc_chan    *rchan;
	int                    rc;

	M0_ENTRY("rbuf: %p packet: %p", rpcbuf, p);
	M0_PRE(rpcbuf != NULL && p != NULL);

	machine = frm_rmachine(p->rp_frm);
	ndom    = machine->rm_tm.ntm_dom;
	M0_ASSERT(ndom != NULL);

	netbuf = &rpcbuf->rb_netbuf;
	rc = net_buffer_allocate(netbuf, ndom, p->rp_size);
	if (rc != 0)
		goto out;

	rc = m0_rpc_packet_encode(p, &netbuf->nb_buffer);
	if (rc != 0) {
		net_buffer_free(netbuf, ndom);
		goto out;
	}
	rchan = frm_rchan(p->rp_frm);
	netbuf->nb_length = m0_vec_count(&netbuf->nb_buffer.ov_vec);
	netbuf->nb_ep     = rchan->rc_destep;

	rpcbuf->rb_packet = p;
	rpc_buffer_bob_init(rpcbuf);

out:
	return M0_RC(rc);
}

/**
   Allocates network buffer and register it with network domain ndom.
 */
static int net_buffer_allocate(struct m0_net_buffer *netbuf,
			       struct m0_net_domain *ndom,
			       m0_bcount_t           buf_size)
{
	m0_bcount_t segment_size;
	int32_t     nr_segments;
	int         rc;

	M0_ENTRY("netbuf: %p ndom: %p bufsize: %llu", netbuf, ndom,
						 (unsigned long long)buf_size);
	M0_PRE(netbuf != NULL && ndom != NULL && buf_size > 0);

	bufvec_geometry(ndom, buf_size, &nr_segments, &segment_size);

	M0_SET0(netbuf);
	rc = m0_bufvec_alloc_aligned(&netbuf->nb_buffer, nr_segments,
				     segment_size, M0_SEG_SHIFT);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "buffer allocation failed");
		goto out;
	}

	rc = m0_net_buffer_register(netbuf, ndom);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "net buf registeration failed");
		m0_bufvec_free_aligned(&netbuf->nb_buffer, M0_SEG_SHIFT);
	}
out:
	return M0_RC(rc);
}

/**
   Depending on buf_size and maximum network buffer segment size,
   returns number and size of segments to required to carry contents of
   size buf_size.
 */
static void bufvec_geometry(struct m0_net_domain *ndom,
			    m0_bcount_t           buf_size,
			    int32_t              *out_nr_segments,
			    m0_bcount_t          *out_segment_size)
{
	m0_bcount_t max_buf_size;
	m0_bcount_t max_segment_size;
	m0_bcount_t segment_size;
	int32_t     max_nr_segments;
	int32_t     nr_segments;

	M0_ENTRY();

	max_buf_size     = m0_net_domain_get_max_buffer_size(ndom);
	max_segment_size = m0_net_domain_get_max_buffer_segment_size(ndom);
	max_nr_segments  = m0_net_domain_get_max_buffer_segments(ndom);

	M0_LOG(M0_DEBUG,
		"max_buf_size: %llu max_segment_size: %llu max_nr_seg: %d",
		(unsigned long long)max_buf_size,
		(unsigned long long)max_segment_size, max_nr_segments);

	M0_ASSERT(buf_size <= max_buf_size);

	/* encoding routine requires buf_size to be 8 byte aligned */
	buf_size = m0_align(buf_size, 8);
	M0_LOG(M0_DEBUG, "bufsize: 0x%llx", (unsigned long long)buf_size);

	if (buf_size <= max_segment_size) {
		segment_size = buf_size;
		nr_segments  = 1;
	} else {
		segment_size = max_segment_size;

		nr_segments = buf_size / max_segment_size;
		if (buf_size % max_segment_size != 0)
			++nr_segments;
	}

	*out_segment_size = segment_size;
	*out_nr_segments  = nr_segments;

	M0_LEAVE("seg_size: %llu nr_segments: %d",
		 (unsigned long long)*out_segment_size, *out_nr_segments);
}

static void net_buffer_free(struct m0_net_buffer *netbuf,
			    struct m0_net_domain *ndom)
{
	M0_ENTRY("netbuf: %p ndom: %p", netbuf, ndom);
	M0_PRE(netbuf != NULL && ndom != NULL);

	m0_net_buffer_deregister(netbuf, ndom);
	m0_bufvec_free_aligned(&netbuf->nb_buffer, M0_SEG_SHIFT);

	M0_LEAVE();
}

/**
   Submits buffer to network layer for sending.
 */
static int rpc_buffer_submit(struct rpc_buffer *rpcbuf)
{
	struct m0_net_buffer  *netbuf;
	struct m0_rpc_machine *machine;
	int                    rc;

	M0_ENTRY("rpcbuf: %p", rpcbuf);
	M0_PRE(rpc_buffer_bob_check(rpcbuf));

	netbuf = &rpcbuf->rb_netbuf;

	netbuf->nb_qtype     = M0_NET_QT_MSG_SEND;
	netbuf->nb_callbacks = &rpc_buf_send_cb;

	machine = rpc_buffer__rmachine(rpcbuf);
	netbuf->nb_timeout = m0_time_from_now(M0_RPC_TMO, 0);
	rc = m0_net_buffer_add(netbuf, &machine->rm_tm);
	if (rc == 0) {
		M0_CNT_INC(machine->rm_active_nb);
		M0_LOG(M0_DEBUG,"+%p->rm_active_nb: %"PRIi64" %p\n",
		       machine, machine->rm_active_nb, rpcbuf);
	}

	return M0_RC(rc);
}

static void rpc_buffer_fini(struct rpc_buffer *rpcbuf)
{
	struct m0_net_domain  *ndom;
	struct m0_rpc_machine *machine;

	M0_ENTRY("rpcbuf: %p", rpcbuf);
	M0_PRE(rpc_buffer_bob_check(rpcbuf));

	machine = rpc_buffer__rmachine(rpcbuf);
	ndom    = machine->rm_tm.ntm_dom;
	M0_ASSERT(ndom != NULL);

	net_buffer_free(&rpcbuf->rb_netbuf, ndom);
	rpc_buffer_bob_fini(rpcbuf);

	M0_LEAVE();
}

/**
   Network layer calls this function, whenever there is any event on
   network buffer which was previously submitted for sending by RPC layer.
 */
static void buf_send_cb(const struct m0_net_buffer_event *ev)
{
	struct m0_net_buffer  *netbuf;
	struct rpc_buffer     *rpcbuf;
	struct m0_rpc_machine *machine;
	struct m0_rpc_stats   *stats;
	struct m0_rpc_packet  *p;

	M0_ENTRY("ev: %p", ev);
	M0_PRE(ev != NULL);

	netbuf = ev->nbe_buffer;
	M0_ASSERT(netbuf != NULL &&
		  netbuf->nb_qtype == M0_NET_QT_MSG_SEND &&
		  (netbuf->nb_flags & M0_NET_BUF_QUEUED) == 0);

	rpcbuf = bob_of(netbuf, struct rpc_buffer, rb_netbuf,
			&rpc_buffer_bob_type);

	machine = rpc_buffer__rmachine(rpcbuf);

	if (M0_FI_ENABLED("delay_callback"))
		m0_nanosleep(m0_time(0, 300000000), NULL); /* 300 msec */

	m0_rpc_machine_lock(machine);

	stats = &machine->rm_stats;
	p = rpcbuf->rb_packet;
	p->rp_status = ev->nbe_status;

	if (M0_FI_ENABLED("fake_err"))
		p->rp_status = -EINVAL;
	rpc_buffer_fini(rpcbuf);
	m0_free(rpcbuf);

	if (p->rp_status == 0) {
		stats->rs_nr_sent_packets++;
		stats->rs_nr_sent_bytes += p->rp_size;
	} else {
                stats->rs_nr_failed_packets++;
	}
	M0_CNT_DEC(machine->rm_active_nb);
	M0_LOG(M0_DEBUG, "-%p->rm_active_nb: %"PRIi64" %p\n",
	       machine, machine->rm_active_nb, rpcbuf);
	if (machine->rm_active_nb == 0)
		m0_chan_broadcast(&machine->rm_nb_idle);
	/*
	 * At this point, rpc subsystem is normally having 4 refs on item/fop:
	 * - m0_rpc__post_locked()
	 * - m0_rpc_item_send()
	 * - item pending cache
	 * - for the formation queue or package (depends on item state)
	 *
	 * Two more refs can be taken in the following cases:
	 * - sync rpc_post (normally used in UT)
	 * - fop_alloc takes first ref but the caller may release it right
	 * after sending to rpc subsystem
	 *
	 * Exceptions:
	 * - session0 fops (such as session termination) normally have 3 refs,
	 * as they don't add items to pending cache
	 * - reply item may have 4 refs but they are taken other places
	 */
	m0_rpc_packet_traverse_items(p, item_done, p->rp_status);
	m0_rpc_frm_packet_done(p);
	m0_rpc_packet_discard(p);

	m0_rpc_machine_unlock(machine);
	M0_LEAVE();
}

static void item_done(struct m0_rpc_packet *p, struct m0_rpc_item *item, int rc)
{
	M0_PRE(item != NULL);
	M0_ENTRY("item=%p[%"PRIu32"] ri_error=%"PRIi32" rc=%d",
		 item, item->ri_type->rit_opcode, item->ri_error, rc);

	if (item->ri_pending_reply != NULL) {
		/* item that is never sent, i.e. item->ri_nr_sent == 0,
		   can never have a (pending/any) reply.
		 */
		M0_ASSERT(ergo(rc != 0, item->ri_nr_sent > 0));
		rc = 0;
		item->ri_error = 0;
	}

	/*
	 * Item timeout by sending deadline is also counted as sending error
	 * and the ref, released in processing reply, is released here.
	 */
	item->ri_error = item->ri_error ?: rc;
	if (item->ri_error != 0) {
		/*
		 * Normally this put() would call at m0_rpc_item_process_reply(),
		 * but there won't be any replies for non-oneway items of this
		 * packet already.
		 */
		if (m0_rpc_item_is_request(item) && !m0_rpc_item_is_oneway(item))
			m0_rpc_item_put(item);
	}
	if (item->ri_error != 0 &&
	    item->ri_sm.sm_state != M0_RPC_ITEM_FAILED) {
		M0_LOG(M0_ERROR, "packet %p, item %p[%"PRIu32"] failed with"
		       " ri_error=%"PRIi32, p, item, item->ri_type->rit_opcode,
		       item->ri_error);
		m0_rpc_item_failed(item, item->ri_error);
	} else
		item_sent(item);

	M0_LEAVE();
}

static void item_sent(struct m0_rpc_item *item)
{
	struct m0_rpc_stats *stats;

	M0_ENTRY("%p[%s/%"PRIu32"], sent=%u max=%lx"
		 " item->ri_sm.sm_state %"PRIu32" ri_error=%"PRIi32,
		 item, item_kind(item), item->ri_type->rit_opcode,
		 item->ri_nr_sent, (unsigned long)item->ri_nr_sent_max,
		 item->ri_sm.sm_state, item->ri_error);

	if (item->ri_sm.sm_state == M0_RPC_ITEM_FAILED) {
		/*
		 * Request might have been cancelled while in SENDING state
		 * and before reaching M0_RPC_ITEM_SENT state.
		 */
		M0_ASSERT(m0_rpc_item_is_request(item));
		M0_ASSERT(item->ri_error == -ECANCELED);
		return;
	}

	M0_PRE(ergo(m0_rpc_item_is_request(item),
		    M0_IN(item->ri_error, (0, -ETIMEDOUT))) &&
	       item->ri_sm.sm_state == M0_RPC_ITEM_SENDING);

	m0_rpc_item_change_state(item, M0_RPC_ITEM_SENT);

	stats = &item->ri_rmachine->rm_stats;
	stats->rs_nr_sent_items++;

	if (item->ri_nr_sent == 1) { /* not resent */
		stats->rs_nr_sent_items_uniq++;
		if (item->ri_ops != NULL && item->ri_ops->rio_sent != NULL)
			item->ri_ops->rio_sent(item);
	} else if (item->ri_nr_sent == 2) {
		/* item with ri_nr_sent >= 2 are counted as 1 in
		   rs_nr_resent_items i.e. rs_nr_resent_items counts number
		   of items that required resending.
		 */
		stats->rs_nr_resent_items++;
	}

	/*
	 * Reference release done here is for the reference taken in
	 * m0_rpc_item_send() and also for one-way items corresponding
	 * reference taken in m0_rpc_oneway_item_post_locked().
	 */
	m0_rpc_item_put(item);

	/*
	 * Request and Reply items take hold on session until
	 * they are SENT/FAILED.
	 * See: m0_rpc__post_locked(), m0_rpc_reply_post()
	 *      m0_rpc_item_send()
	 */
	if (m0_rpc_item_is_request(item) || m0_rpc_item_is_reply(item))
		m0_rpc_session_release(item->ri_session);

	if (m0_rpc_item_is_request(item)) {
		m0_rpc_item_change_state(item, M0_RPC_ITEM_WAITING_FOR_REPLY);
		if (item->ri_pending_reply != NULL) {
			/* Reply has already been received when we
			   were waiting for buffer callback */
			m0_rpc_item_process_reply(item, item->ri_pending_reply);
			item->ri_pending_reply = NULL;
			M0_ASSERT(item->ri_sm.sm_state == M0_RPC_ITEM_REPLIED);
		}
	}

	M0_LEAVE();
}

static void item_fail(struct m0_rpc_packet *p, struct m0_rpc_item *item, int rc)
{
        /* This is only called from packet_ready() error handling code path. */
        M0_PRE(item != NULL);
        M0_ENTRY("item=%p[%"PRIu32"] ri_error=%"PRIi32" rc=%d",
                 item, item->ri_type->rit_opcode, item->ri_error, rc);

        item->ri_error = rc;
        if (item->ri_error != 0) {
                M0_LOG(M0_ERROR, "packet %p, item %p[%"PRIu32"] failed with"
                       " ri_error=%"PRIi32, p, item, item->ri_type->rit_opcode,
                       item->ri_error);
                if (item->ri_sm.sm_state != M0_RPC_ITEM_FAILED)
                        m0_rpc_item_failed(item, item->ri_error);
        }

        M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} */
