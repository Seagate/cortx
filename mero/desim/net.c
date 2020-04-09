/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 */
/*
 * Copyright 2009 ClusterStor.
 *
 * Nikita Danilov.
 */

#include <stdio.h>

#include "lib/assert.h"
#include "mero/magic.h"
#include "desim/sim.h"
#include "desim/net.h"
#include "desim/elevator.h"
#include "stob/stob.h"		/* m0_stob_id_key_get */

/**
   @addtogroup desim desim
   @{
 */

M0_TL_DESCR_DEFINE(rpc, "rpcs", static, struct net_rpc,
		   nr_inqueue, nr_magic, M0_DESIM_NET_RPC_MAGIC,
		   M0_DESIM_NET_RPC_HEAD_MAGIC);
M0_TL_DEFINE(rpc, static, struct net_rpc);

static void net_srv_loop(struct sim *s, struct sim_thread *t, void *arg)
{
	struct net_srv *srv = arg;
	struct net_rpc *rpc;
	unsigned sect       = srv->ns_el->e_dev->sd_conf->sc_sector_size;
	unsigned trid;

	trid = t - srv->ns_thread;
	M0_ASSERT(trid < srv->ns_nr_threads);

	while (1) {
		unsigned long long count;
		unsigned long long offset;
		unsigned dev;
		unsigned long obj;

		while (rpc_tlist_is_empty(&srv->ns_queue)) {
			sim_chan_wait(&srv->ns_incoming, t);
			if (srv->ns_shutdown)
				sim_thread_exit(t);
		}

		srv->ns_active++;
		rpc = rpc_tlist_pop(&srv->ns_queue);
		count = rpc->nr_todo;
		dev = m0_stob_id_dom_id_get(&rpc->nr_stob_id) % srv->ns_nr_devices;
		obj = rpc->nr_stob_id.si_fid.f_key;
		offset = srv->ns_file_size * obj + rpc->nr_offset;

		sim_log(s, SLL_TRACE, "S#%s %2i/%2i: [%"PRIu64"/%u:%lu] "
			"%10llu %8lu %10llu\n",
			srv->ns_name, trid, srv->ns_active,
			m0_stob_id_dom_id_get(&rpc->nr_stob_id), dev, obj,
			rpc->nr_offset, rpc->nr_todo, offset);
		rpc->nr_srv_thread = t;
		/* delay before bulk */
		sim_sleep(t, sim_rnd(srv->ns_pre_bulk_min,
				     srv->ns_pre_bulk_max));
		sim_chan_signal(&rpc->nr_wait);
		/* wait for bulk completion */
		sim_chan_wait(&rpc->nr_bulk_wait, t);
		/* IO delay after bulk */
		elevator_io(&srv->ns_el[dev], SRT_WRITE,
			    offset / sect, count / sect);
		srv->ns_active--;
	}
}

static int net_srv_threads_start(struct sim_callout *call)
{
	struct net_srv *srv = call->sc_datum;
	unsigned i;

	for (i = 0; i < srv->ns_nr_threads; ++i)
		sim_thread_init(call->sc_sim, &srv->ns_thread[i], 0,
				net_srv_loop, srv);
	return 1;
}


M0_INTERNAL void net_srv_init(struct sim *s, struct net_srv *srv)
{
	rpc_tlist_init(&srv->ns_queue);
	sim_chan_init(&srv->ns_incoming, "srv#%s::incoming", srv->ns_name);
	srv->ns_thread = sim_alloc(srv->ns_nr_threads*sizeof srv->ns_thread[0]);
	srv->ns_el = sim_alloc(srv->ns_nr_devices * sizeof srv->ns_el[0]);
	sim_timer_add(s, 0, net_srv_threads_start, srv);
}

M0_INTERNAL void net_srv_fini(struct net_srv *srv)
{
	int i;

	rpc_tlist_fini(&srv->ns_queue);
	srv->ns_shutdown = 1;
	sim_chan_broadcast(&srv->ns_incoming);

	if (srv->ns_thread != NULL) {
		for (i = 0; i < srv->ns_nr_threads; ++i) {
			/* drain events added during finalisation. */
			sim_run(srv->ns_thread[i].st_sim);
			sim_thread_fini(&srv->ns_thread[i]);
		}
		sim_free(srv->ns_thread);
	}
	sim_chan_fini(&srv->ns_incoming);
}

M0_INTERNAL void net_init(struct net_conf *net)
{
	sim_chan_init(&net->nc_queue, "net::queue");
	cnt_init(&net->nc_rpc_wait, NULL, "net::rpc_wait");
	cnt_init(&net->nc_rpc_bulk_wait, NULL, "net::rpc_bulk_wait");
}

M0_INTERNAL void net_fini(struct net_conf *net)
{
	sim_chan_fini(&net->nc_queue);
	cnt_fini(&net->nc_rpc_wait);
	cnt_fini(&net->nc_rpc_bulk_wait);
}

M0_INTERNAL void net_rpc_init(struct net_rpc *rpc, struct net_conf *conf,
			      struct net_srv *srv, struct m0_stob_id *stob_id,
			      unsigned long long offset, unsigned long nob)
{
	rpc->nr_conf     = conf;
	rpc->nr_srv      = srv;
	rpc->nr_stob_id = *stob_id;
	rpc->nr_offset   = offset;
	rpc->nr_todo     = nob;
	rpc_tlink_init(rpc);
	sim_chan_init(&rpc->nr_wait, NULL);
	sim_chan_init(&rpc->nr_bulk_wait, NULL);
	rpc->nr_wait.ch_cnt_sleep.c_parent = &conf->nc_rpc_wait;
	rpc->nr_bulk_wait.ch_cnt_sleep.c_parent = &conf->nc_rpc_bulk_wait;
}

M0_INTERNAL void net_rpc_fini(struct net_rpc *rpc)
{
	M0_ASSERT(rpc->nr_todo == 0);
	rpc_tlink_fini(rpc);
	sim_chan_fini(&rpc->nr_wait);
	sim_chan_fini(&rpc->nr_bulk_wait);
	rpc->nr_magic = 0;
}

static void net_enter(struct sim_thread *t, struct net_conf *n,
		      unsigned long nob)
{
	while (n->nc_nob_inflight + nob > n->nc_nob_max ||
	       n->nc_msg_inflight + 1   > n->nc_msg_max)
		sim_chan_wait(&n->nc_queue, t);
	n->nc_nob_inflight += nob;
	n->nc_msg_inflight += 1;
}

static void net_leave(struct sim_thread *t, struct net_conf *n,
		      unsigned long nob)
{
	M0_ASSERT(n->nc_nob_inflight >= nob);
	M0_ASSERT(n->nc_msg_inflight >= 1);
	n->nc_msg_inflight -= 1;
	n->nc_nob_inflight -= nob;
	sim_chan_broadcast(&n->nc_queue);
}

static void net_tx(struct sim_thread *t, struct net_conf *n,
		   unsigned long nob, sim_time_t min, sim_time_t max)
{
	net_enter(t, n, nob);
	sim_sleep(t, sim_rnd(min, max));
	net_leave(t, n, nob);
}

M0_INTERNAL void net_rpc_send(struct sim_thread *t, struct net_rpc *rpc)
{
	struct net_conf *n;

	n = rpc->nr_conf;
	net_tx(t, n, n->nc_rpc_size, n->nc_rpc_delay_min, n->nc_rpc_delay_max);
	sim_chan_signal(&rpc->nr_srv->ns_incoming);
	rpc_tlist_add_tail(&rpc->nr_srv->ns_queue, rpc);
	sim_chan_wait(&rpc->nr_wait, t);
	net_tx(t, n, n->nc_rpc_size, n->nc_rpc_delay_min, n->nc_rpc_delay_max);
}

M0_INTERNAL void net_rpc_bulk(struct sim_thread *t, struct net_rpc *rpc)
{
	struct net_conf *n;
	sim_time_t       tx_min;
	sim_time_t       tx_max;

	n = rpc->nr_conf;
	tx_min = rpc->nr_todo * 1000000000 / n->nc_rate_max;
	tx_max = rpc->nr_todo * 1000000000 / n->nc_rate_min;
	net_tx(t, n, rpc->nr_todo, tx_min, tx_max);
	rpc->nr_todo = 0;
	sim_chan_signal(&rpc->nr_bulk_wait);
}

M0_INTERNAL void net_rpc_process(struct sim_thread *t,
				 struct net_conf *net, struct net_srv *srv,
				 struct m0_stob_id *stob_id,
				 unsigned long long offset, unsigned long count)
{
	struct net_rpc rpc;

	net_rpc_init(&rpc, net, srv, stob_id, offset, count);
	net_rpc_send(t, &rpc);
	net_rpc_bulk(t, &rpc);
	net_rpc_fini(&rpc);
}

/** @} end of desim group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
