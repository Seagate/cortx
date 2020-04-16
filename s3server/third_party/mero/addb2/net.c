/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 07-Feb-2015
 */


/**
 * @addtogroup addb2
 *
 * Addb2 network implementation
 * ----------------------------
 *
 * Traces are piggy-backed to the outgoing rpc packets by using
 * m0_rpc_item_source interface. No attempt is made to aggregate individual
 * traces before sending.
 *
 * A trace is sent as a one-way fop, with m0_addb2_trace as fop data.
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/trace.h"
#include "lib/tlist.h"
#include "lib/mutex.h"
#include "lib/memory.h"
#include "lib/types.h"
#include "lib/time.h"
#include "mero/magic.h"
#include "fop/fop.h"
#include "rpc/rpc.h"                    /* m0_rpc_oneway_item_post */
#include "rpc/conn.h"
#include "rpc/item_source.h"
#include "rpc/rpc_opcodes.h"            /* M0_ADDB_FOP_OPCODE */
#include "addb2/addb2.h"
#include "addb2/addb2_xc.h"
#include "addb2/service.h"
#include "addb2/internal.h"

/**
 * Addb2 network machine.
 */
struct m0_addb2_net {
	/** Lock serialising all operations. */
	struct m0_mutex ne_lock;
	/**
	 * Queue of pending trace objects, linked via
	 * m0_addb2_trace_obj::o_linkage.
	 */
	struct m0_tl    ne_queue;
	/**
	 * List of connections linked via source::s_linkage.
	 */
	struct m0_tl    ne_src;
	/**
	 * Time when a trace was sent out last time.
	 *
	 * Used by m0_addb2_net_tick() to decide when to push traces out.
	 */
	m0_time_t       ne_last;
	/**
	 * Call-back invoked when the machine goes idle.
	 */
	void          (*ne_callback)(struct m0_addb2_net *, void *);
	/**
	 * Datum passed to the m0_addb2_net::ne_callback() callback.
	 */
	void           *ne_datum;
};

/**
 * A "source" is an outgoing connection through which network machine sends
 * traces out.
 */
struct source {
	struct m0_rpc_item_source  s_src;
	struct m0_addb2_net       *s_net;
	/**
	 * Linkage in m0_addb2_net::ne_src.
	 */
	struct m0_tlink            s_linkage;
	uint64_t                   s_magix;
};

M0_TL_DESCR_DEFINE(src, "addb2 rpc sources",
		   static, struct source, s_linkage, s_magix,
		   M0_ADDB2_SOURCE_MAGIC, M0_ADDB2_SOURCE_HEAD_MAGIC);
M0_TL_DEFINE(src, static, struct source);

static void src_fini     (struct source *s);
static void net_lock     (struct m0_addb2_net *net);
static void net_unlock   (struct m0_addb2_net *net);
static void net_force    (struct m0_addb2_net *net);
static void net_sent     (struct m0_rpc_item *item);
static bool net_invariant(const struct m0_addb2_net *net);
static void net_fop_init (struct m0_fop *fop, struct m0_addb2_net *net,
			  struct m0_addb2_trace *trace);

static bool                src_has_item(const struct m0_rpc_item_source *ris);
static void                src_conn_terminating(struct m0_rpc_item_source *ris);
static struct m0_rpc_item *src_get_item(struct m0_rpc_item_source *ris,
					m0_bcount_t size);

static const struct m0_rpc_item_source_ops src_ops;
static struct m0_fop_type                  net_fopt;
static const struct m0_rpc_item_ops        net_rpc_ops;

static const m0_time_t IDLE_THRESHOLD = M0_MKTIME(10, 0);

M0_INTERNAL struct m0_addb2_net *m0_addb2_net_init(void)
{
	struct m0_addb2_net *net;

	M0_ALLOC_PTR(net);
	if (net != NULL) {
		m0_mutex_init(&net->ne_lock);
		tr_tlist_init(&net->ne_queue);
		src_tlist_init(&net->ne_src);
		M0_POST(net_invariant(net));
	}
	return net;
}

M0_INTERNAL void m0_addb2_net_fini(struct m0_addb2_net *net)
{
	struct source *s;

	/*
	 * This lock-unlock is a barrier against concurrently finishing last
	 * src_get_item(), which executed the idle call-back.
	 *
	 * It *is* valid to acquire and finalise a lock in the same function in
	 * this particular case.
	 */
	net_lock(net);
	net_unlock(net);

	M0_PRE(net->ne_callback == NULL);
	m0_tl_teardown(src, &net->ne_src, s) {
		src_fini(s);
	}
	src_tlist_fini(&net->ne_src);
	tr_tlist_fini(&net->ne_queue);
	m0_mutex_fini(&net->ne_lock);
	m0_free(net);
}

M0_INTERNAL int m0_addb2_net_add(struct m0_addb2_net *net,
				 struct m0_rpc_conn *conn)
{
	struct source *s;
	int            result;

	M0_ALLOC_PTR(s);
	if (s != NULL) {
		s->s_net = net;
		/*
		 * Lock ordering: addb-net lock nests within rpc machine lock.
		 */
		m0_rpc_machine_lock(conn->c_rpc_machine);
		net_lock(net);
		src_tlink_init_at_tail(s, &net->ne_src);
		m0_rpc_item_source_init(&s->s_src,
					"addb2 item source", &src_ops);
		m0_rpc_item_source_register_locked(conn, &s->s_src);
		net_unlock(net);
		m0_rpc_machine_unlock(conn->c_rpc_machine);
		result = 0;
	} else
		result = M0_ERR(-ENOMEM);
	return M0_RC(result);
}

M0_INTERNAL void m0_addb2_net_del(struct m0_addb2_net *net,
				  struct m0_rpc_conn *conn)
{
	struct source *s;

	net_lock(net);
	s = m0_tl_find(src, scan, &net->ne_src, scan->s_src.ris_conn == conn);
	M0_ASSERT(s != NULL);
	src_fini(s);
	net_unlock(net);
}

M0_INTERNAL int m0_addb2_net_submit(struct m0_addb2_net *net,
				    struct m0_addb2_trace_obj *obj)
{
	net_lock(net);
	M0_PRE(net->ne_callback == NULL);
	tr_tlink_init_at_tail(obj, &net->ne_queue);
	net_unlock(net);
	return +1;
}

M0_INTERNAL void m0_addb2_net_tick(struct m0_addb2_net *net)
{
	/*
	 * If a trace was sent out recently, do nothing, otherwise send a dummy
	 * null trace through some of connections to trigger piggy-backing of
	 * more traces.
	 */
	if (m0_time_is_in_past(m0_time_add(net->ne_last, IDLE_THRESHOLD)))
		net_force(net);
}

M0_INTERNAL void m0_addb2_net_stop(struct m0_addb2_net *net,
				   void (*callback)(struct m0_addb2_net *,
						    void *),
				   void *datum)
{
	struct m0_tl              *q = &net->ne_queue;
	struct m0_addb2_trace_obj *obj;

	net_lock(net);
	M0_PRE(net->ne_callback == NULL);
	/*
	 * If there are no sources, it makes no sense to wait for queue drain.
	 */
	if (src_tlist_is_empty(&net->ne_src)) {
		m0_tl_teardown(tr, q, obj) {
			m0_addb2_trace_done(&obj->o_tr);
		}
	}
	/*
	 * @todo What to do when addb2 net is stopping, but there are no rpc
	 * packets to piggy-back remaining traces to?
	 *
	 * This usually happens during umount.
	 *
	 * For now, just discard all the traces.
	 */
	if (!tr_tlist_is_empty(q)) {
		M0_LOG(M0_NOTICE, "Traces discarded: %zi", tr_tlist_length(q));
		m0_tl_teardown(tr, q, obj) {
			m0_addb2_trace_done(&obj->o_tr);
		}
	}
	if (tr_tlist_is_empty(q))
		(*callback)(net, datum);
	else {
		net->ne_callback = callback;
		net->ne_datum    = datum;
	}
	net_unlock(net);
}

M0_INTERNAL bool m0_addb2_net__is_not_locked(const struct m0_addb2_net *net)
{
	return m0_mutex_is_not_locked(&net->ne_lock);
}

M0_INTERNAL int m0_addb2_net_module_init(void)
{
	M0_FOP_TYPE_INIT(&net_fopt,
			 .name      = "addb2-fop",
			 .opcode    = M0_ADDB_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_ONEWAY,
			 .xt        = m0_addb2_trace_xc,
			 .fom_ops   = &m0_addb2__fom_type_ops,
			 .sm        = &m0_addb2__sm_conf,
			 .svc_type  = &m0_addb2_service_type);
	return 0;
}

M0_INTERNAL void m0_addb2_net_module_fini(void)
{
	m0_fop_type_fini(&net_fopt);
}


static void src_fini(struct source *s)
{
	m0_rpc_item_source_deregister(&s->s_src);
	m0_rpc_item_source_fini(&s->s_src);
	src_tlist_remove(s);
	m0_free(s);
}

static void net_lock(struct m0_addb2_net *net)
{
	m0_mutex_lock(&net->ne_lock);
	M0_ASSERT(net_invariant(net));
}

static void net_unlock(struct m0_addb2_net *net)
{
	M0_ASSERT(net_invariant(net));
	m0_mutex_unlock(&net->ne_lock);
}

static bool net_invariant(const struct m0_addb2_net *net)
{
	return m0_tl_forall(src, s, &net->ne_src,
			    s->s_net == net);
}

static void net_force(struct m0_addb2_net *net)
{
	struct source               *s;
	struct m0_fop               *fop;
	static struct m0_addb2_trace NULL_TRACE = {
		.tr_nr   = 0,
		.tr_body = NULL
	};

	M0_ALLOC_PTR(fop);
	if (fop != NULL) {
		net_fop_init(fop, net, &NULL_TRACE);
		net_lock(net);
		/* Send everything to the first addb service. Scalability
		   problem. */
		s = src_tlist_head(&net->ne_src);
		net_unlock(net);
		if (s != NULL) {
			m0_rpc_oneway_item_post(s->s_src.ris_conn,
						&fop->f_item);
		} else {
			fop->f_data.fd_data = NULL;
			m0_fop_fini(fop);
			m0_free(fop);
		}
	} else
		M0_LOG(M0_ERROR, "Cannot allocate fop.");
}

/**
 * Implementation of m0_rpc_item_source_ops::riso_has_item().
 *
 * Returns true iff the machine has any items to send out.
 *
 * This is called by rpc after an rpc packet (m0_rpc_packet) is formed.
 */
static bool src_has_item(const struct m0_rpc_item_source *ris)
{
	struct source       *s   = M0_AMB(s, ris, s_src);
	struct m0_addb2_net *net = s->s_net;
	bool                 empty;

	net_lock(net);
	empty = tr_tlist_is_empty(&net->ne_queue);
	net_unlock(net);
	return !empty;
}

/**
 * Helper to initialise an outgoing one-way fop.
 */
static void net_fop_init(struct m0_fop *fop, struct m0_addb2_net *net,
			 struct m0_addb2_trace *trace)
{
	/* Do not bother to xcode the trace. */
	m0_fop_init(fop, &net_fopt, trace, &m0_fop_release);
	fop->f_item.ri_ops = &net_rpc_ops;
}

/**
 * An implementation of m0_rpc_item_source_ops::riso_get_item().
 *
 * Allocates and initialises a fop with a trace of suitable size (not exceeding
 * the given "size").
 *
 * This is called by rpc after an rpc packet is formed to fill remaining space
 * (frm_fill_packet_from_item_sources()).
 */
static struct m0_rpc_item *src_get_item(struct m0_rpc_item_source *ris,
					m0_bcount_t size)
{
	struct source             *s    = M0_AMB(s, ris, s_src);
	struct m0_addb2_net       *net  = s->s_net;
	struct m0_rpc_item        *item = NULL;
	struct m0_fop             *fop;
	struct m0_addb2_trace_obj *obj;
	struct m0_addb2_trace     *t;

	M0_ALLOC_PTR(fop);
	if (fop != NULL) {
		net_lock(net);
		obj = tr_tlist_head(&net->ne_queue);
		if (obj != NULL) {
			t = &obj->o_tr;
			if (m0_addb2_trace_size(t) <= size) {
				net_fop_init(fop, net, t);
				item = &fop->f_item;
				tr_tlist_del(obj);
				net->ne_last = m0_time_now();
				if (tr_tlist_is_empty(&net->ne_queue) &&
				    net->ne_callback != NULL) {
					net->ne_callback(net, net->ne_datum);
					net->ne_callback = NULL;
				}
			}
		}
		net_unlock(net);
	} else
		M0_LOG(M0_ERROR, "Cannot allocate fop.");
	if (item == NULL)
		m0_free(fop);
	return item;
}

/**
 * An implementation of m0_rpc_item_source_ops::riso_conn_terminating().
 *
 * This is called by rpc when a connection is terminated.
 */
static void src_conn_terminating(struct m0_rpc_item_source *ris)
{
	struct source *s = M0_AMB(s, ris, s_src);

	m0_addb2_net_del(s->s_net, ris->ris_conn);
}

/**
 * An implementation of m0_rpc_item_ops::rio_sent() for one-way trace fops.
 *
 * This is called by rpc after the fop has been placed on wire.
 */
static void net_sent(struct m0_rpc_item *item)
{
	struct m0_fop *fop = m0_rpc_item_to_fop(item);
	m0_addb2_trace_done(m0_fop_data(fop));
	if (item->ri_error != 0)
		M0_LOG(M0_ERROR, "Addb trace lost in rpc.");
	/*
	 * Clear fop data, which points to a trace, so that m0_fop_fini() won't
	 * try to free it.
	 */
	fop->f_data.fd_data = NULL;
}

static const struct m0_rpc_item_source_ops src_ops = {
	.riso_has_item         = &src_has_item,
	.riso_get_item         = &src_get_item,
	.riso_conn_terminating = &src_conn_terminating
};

static const struct m0_rpc_item_ops net_rpc_ops = {
	.rio_sent = &net_sent
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
