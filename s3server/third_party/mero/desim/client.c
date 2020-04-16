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
 */
/*
 * Copyright 2009, 2010 ClusterStor.
 *
 * Nikita Danilov.
 */

#include <stdio.h>

#include "lib/assert.h"
#include "mero/magic.h"
#include "desim/sim.h"
#include "desim/net.h"
#include "desim/client.h"

/**
   @addtogroup desim desim
   @{
 */

struct client_thread_param {
	struct client_conf *ctp_conf;
	unsigned            ctp_client;
	unsigned            ctp_thread;
};

struct client_write_ext {
	unsigned long long  cwe_offset;
	unsigned long       cwe_count;
	struct m0_tlink     cwe_linkage;
	uint64_t            cwe_magic;
};

M0_TL_DESCR_DEFINE(cl, "client write extents", static, struct client_write_ext,
		   cwe_linkage, cwe_magic, M0_DESIM_CLIENT_WRITE_EXT_MAGIC,
		   M0_DESIM_CLIENT_WRITE_EXT_HEAD_MAGIC);
M0_TL_DEFINE(cl, static, struct client_write_ext);

static void client_pageout(struct sim *s, struct sim_thread *t, void *arg)
{
	struct client           *c    = arg;
	struct client_conf      *conf = c->cl_conf;
	unsigned                 size = conf->cc_opt_count;
	struct client_write_ext *ext;
	struct m0_stob_id        stob_id;

	while (1) {
		while (c->cl_dirty - c->cl_io < size) {
			sim_chan_wait(&c->cl_cache_busy, t);
			if (conf->cc_shutdown)
				sim_thread_exit(t);
		}
		M0_ASSERT(!cl_tlist_is_empty(&c->cl_write_ext));
		ext = cl_tlist_head(&c->cl_write_ext);
		/* no real cache management for now */
		M0_ASSERT(ext->cwe_count == size);
		cl_tlink_del_fini(ext);
		sim_log(s, SLL_TRACE, "P%2i/%2i: %6lu %10llu %8u\n", c->cl_id,
			c->cl_inflight, c->cl_fid, ext->cwe_offset, size);
		c->cl_io += size;
		c->cl_inflight++;
		m0_stob_id_make(0, c->cl_fid, &M0_FID_INIT(0, c->cl_fid), &stob_id);
		net_rpc_process(t, conf->cc_net, conf->cc_srv,
				&stob_id, ext->cwe_offset, size);
		c->cl_inflight--;
		c->cl_io -= size;
		M0_ASSERT(c->cl_cached >= size);
		M0_ASSERT(c->cl_dirty  >= size);
		c->cl_cached -= size;
		c->cl_dirty  -= size;
		sim_chan_broadcast(&c->cl_cache_free);
		sim_free(ext);
	}
}

static void client_write_loop(struct sim *s, struct sim_thread *t, void *arg)
{
	struct client_thread_param *param = arg;
	unsigned                    clid  = param->ctp_client;
	unsigned                    trid  = param->ctp_thread;
	struct client_conf         *conf  = param->ctp_conf;
	struct client              *cl    = &conf->cc_client[clid];
	unsigned long               nob   = 0;
	unsigned                    count = conf->cc_count;
	unsigned long long          off   = conf->cc_total * trid;
	struct client_write_ext    *ext;

	M0_ASSERT(t == &cl->cl_thread[trid]);
	M0_ASSERT(cl->cl_id == clid);

	while (nob < conf->cc_total) {
		sim_sleep(t, sim_rnd(conf->cc_delay_min, conf->cc_delay_max));
		while (cl->cl_dirty + count > conf->cc_dirty_max)
			sim_chan_wait(&cl->cl_cache_free, t);
		ext = sim_alloc(sizeof *ext);
		ext->cwe_offset = off;
		ext->cwe_count  = count;
		cl_tlink_init_at_tail(ext, &cl->cl_write_ext);
		cl->cl_cached += count;
		cl->cl_dirty  += count;
		sim_chan_broadcast(&cl->cl_cache_busy);
		sim_log(s, SLL_TRACE, "W%2i/%2i: %6lu %10llu %8u\n",
			clid, trid, cl->cl_fid, off, count);
		nob += count;
		off += count;
	}
	sim_thread_exit(t);
}

static int client_threads_start(struct sim_callout *call)
{
	struct client_conf        *conf  = call->sc_datum;
	struct client_thread_param param = {
		.ctp_conf = conf
	};
	unsigned i;
	unsigned j;

	for (i = 0; i < conf->cc_nr_clients; ++i) {
		struct client *c = &conf->cc_client[i];
		for (j = 0; j < conf->cc_nr_threads; ++j) {
			param.ctp_client = i;
			param.ctp_thread = j;
			sim_thread_init(call->sc_sim, &c->cl_thread[j], 0,
					client_write_loop, &param);
		}
		for (j = 0; j < conf->cc_inflight_max; ++j)
			sim_thread_init(call->sc_sim, &c->cl_pageout[j], 0,
					client_pageout, c);
	}
	return 1;
}

M0_INTERNAL void client_init(struct sim *s, struct client_conf *conf)
{
	unsigned i;

	cnt_init(&conf->cc_cache_free, NULL, "client::cache-free");
	cnt_init(&conf->cc_cache_busy, NULL, "client::cache-busy");
	conf->cc_client = sim_alloc(conf->cc_nr_clients *
				    sizeof conf->cc_client[0]);
	for (i = 0; i < conf->cc_nr_clients; ++i) {
		struct client *c;

		c = &conf->cc_client[i];
		cl_tlist_init(&c->cl_write_ext);
		c->cl_conf = conf;
		c->cl_fid  = i;
		c->cl_id   = i;
		sim_chan_init(&c->cl_cache_free, "client-%04i::cache-free", i);
		sim_chan_init(&c->cl_cache_busy, "client-%04i::cache-busy", i);
		c->cl_cache_free.ch_cnt_sleep.c_parent = &conf->cc_cache_free;
		c->cl_cache_busy.ch_cnt_sleep.c_parent = &conf->cc_cache_busy;

		c->cl_thread = sim_alloc(conf->cc_nr_threads *
					 sizeof c->cl_thread[0]);
		c->cl_pageout = sim_alloc(conf->cc_inflight_max *
					  sizeof c->cl_pageout[0]);
	}
	sim_timer_add(s, 0, client_threads_start, conf);
}

M0_INTERNAL void client_fini(struct client_conf *conf)
{
	unsigned i;
	unsigned j;

	conf->cc_shutdown = 1;
	if (conf->cc_client != NULL) {
		for (i = 0; i < conf->cc_nr_clients; ++i) {
			struct client *c = &conf->cc_client[i];

			sim_chan_broadcast(&c->cl_cache_busy);
			sim_chan_fini(&c->cl_cache_free);
			sim_chan_fini(&c->cl_cache_busy);
			if (c->cl_thread != NULL) {
				for (j = 0; j < conf->cc_nr_threads; ++j) {
					/*
					 * run simulation to drain events posted
					 * during finalisation (e.g., by
					 * sim_chan_broadcast() above).
					 */
					sim_run(c->cl_thread[j].st_sim);
					sim_thread_fini(&c->cl_thread[j]);
				}
				sim_free(c->cl_thread);
			}
			if (c->cl_pageout != NULL) {
				for (j = 0; j < conf->cc_inflight_max; ++j) {
					sim_run(c->cl_pageout[j].st_sim);
					sim_thread_fini(&c->cl_pageout[j]);
				}
				sim_free(c->cl_pageout);
			}
			cl_tlist_fini(&c->cl_write_ext);
		}
		sim_free(conf->cc_client);
	}
	cnt_fini(&conf->cc_cache_free);
	cnt_fini(&conf->cc_cache_busy);
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
