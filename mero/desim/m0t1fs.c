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
 * Original creation date: 2012-Jun-16
 */

#include "lib/assert.h"
#include "desim/sim.h"
#include "desim/net.h"
#include "desim/m0t1fs.h"
#include "layout/linear_enum.h"    /* struct m0_layout_linear_enum */
#include "cob/cob.h"               /* m0_cob_fid_type */
#include "ioservice/fid_convert.h" /* m0_fid_convert_cob2adstob */

/**
   @addtogroup desim desim
   @{
 */

static void thread_loop(struct sim *s, struct sim_thread *t, void *arg)
{
	struct m0t1fs_thread       *cth  = arg;
	struct m0t1fs_client       *cl   = cth->cth_client;
	struct m0t1fs_conf         *conf = cl->cc_conf;
	struct m0_pdclust_instance *pi;
	struct m0_pdclust_layout   *pl;
	uint32_t                    pl_N;
	uint32_t                    pl_K;
	struct m0_layout_enum      *le;
	int64_t                     nob;
	m0_bindex_t                 grp;
	m0_bcount_t                 unit;

	M0_ASSERT(t == &cth->cth_thread);

	pi   = m0_layout_instance_to_pdi(cth->cth_layout_instance);
	pl   = m0_layout_to_pdl(pi->pi_base.li_l);
	pl_N = pl->pl_attr.pa_N;
	pl_K = pl->pl_attr.pa_K;
	sim_log(s, SLL_TRACE, "thread [%i:%i]: seed: ["U128X_F"]\n",
		cl->cc_id, cth->cth_id, U128_P(&pl->pl_attr.pa_seed));

	nob  = conf->ct_total;
	unit = conf->ct_unitsize;
	le = m0_striped_layout_to_enum(&pl->pl_base);
	for (nob = conf->ct_total, grp = 0; nob > 0;
	     nob -= unit * (pl_N + pl_K), grp++) {
		unsigned idx;

		sim_sleep(t, sim_rnd(conf->ct_delay_min, conf->ct_delay_max));
		/* loop over data and parity units, skip spare units. */
		for (idx = 0; idx < pl_N + pl_K; ++idx) {
			const struct m0_pdclust_src_addr src = {
				.sa_group = grp,
				.sa_unit  = idx
			};
			struct m0_pdclust_tgt_addr tgt;
			uint32_t                   obj;
			uint32_t                   srv;
			struct m0t1fs_conn        *conn;
			struct m0_fid              fid;
			struct m0_stob_id          stob_id;

			m0_pdclust_instance_map(pi, &src, &tgt);
			/* @todo for parity unit waste some time calculating
			   parity. Limit bus bandwidth. */
			obj = tgt.ta_obj;
			M0_ASSERT(obj < conf->ct_pool_version.pv_attr.pa_P);
			srv  = obj / conf->ct_nr_devices;
			conn = &cl->cc_srv[srv];
			le->le_ops->leo_get(le, obj, &pi->pi_base.li_gfid,
					    &fid);

			sim_log(s, SLL_TRACE,
				"%c [%3i:%3i] -> %4u@%3u "FID_F" %6lu\n",
				"DPS"[m0_pdclust_unit_classify(pl, idx)],
				cl->cc_id, cth->cth_id, obj, srv,
				FID_P(&fid), tgt.ta_frame);

			/* wait until rpc can be send to the server. */
			while (conn->cs_inflight >= conf->ct_inflight_max)
				sim_chan_wait(&conn->cs_wakeup, t);
			conn->cs_inflight++;
			m0_fid_tassume(&fid, &m0_cob_fid_type);
			m0_fid_convert_cob2adstob(&fid, &stob_id);
			net_rpc_process(t, conf->ct_net, &conf->ct_srv[srv],
					&stob_id, tgt.ta_frame * unit, unit);
			conn->cs_inflight--;
			sim_chan_signal(&conn->cs_wakeup);
		}
	}
	sim_thread_exit(t);
}

static int threads_start(struct sim_callout *call)
{
	struct m0t1fs_conf *conf  = call->sc_datum;
	unsigned i;
	unsigned j;

	for (i = 0; i < conf->ct_nr_clients; ++i) {
		struct m0t1fs_client *c = &conf->ct_client[i];

		for (j = 0; j < conf->ct_nr_threads; ++j) {
			struct m0t1fs_thread *t = &c->cc_thread[j];

			sim_thread_init(call->sc_sim, &t->cth_thread, 0,
					thread_loop, t);
		}
	}
	return 1;
}

static void layout_build(struct m0t1fs_conf *conf)
{
	int                           result;
	struct m0_layout_linear_attr  lin_attr;
	struct m0_layout_linear_enum *lin_enum;
	struct m0_pdclust_attr        pl_attr;
	uint64_t                      lid;

	result = m0_layout_domain_init(&conf->ct_l_dom);
	M0_ASSERT(result == 0);

	result = m0_layout_standard_types_register(&conf->ct_l_dom);
	M0_ASSERT(result == 0);

	lin_attr.lla_nr = conf->ct_pool_version.pv_attr.pa_P;
	lin_attr.lla_A = 0;
	lin_attr.lla_B = 1;
	result = m0_linear_enum_build(&conf->ct_l_dom, &lin_attr, &lin_enum);
	M0_ASSERT(result == 0);

	pl_attr.pa_N = conf->ct_N;
	pl_attr.pa_K = conf->ct_K;
	pl_attr.pa_P = conf->ct_pool_version.pv_attr.pa_P;
	pl_attr.pa_unit_size = conf->ct_unitsize;
	lid = 0x4332543146535349; /* M0T1FSSI */
	m0_uint128_init(&pl_attr.pa_seed, "m0t1fs_si_pdclus");

	result = m0_pdclust_build(&conf->ct_l_dom, lid, &pl_attr,
				  &lin_enum->lle_base, &conf->ct_pdclust);
	M0_ASSERT(result == 0);
}

static void m0t1fs_layout_fini(struct m0t1fs_conf *conf)
{
	/*
	 * Delete the reference on the layout object that was acquired in
	 * layout_build() so that the layout gets deleted.
	 */
	m0_layout_put(&conf->ct_pdclust->pl_base.sl_base);

	m0_layout_standard_types_unregister(&conf->ct_l_dom);
	m0_layout_domain_fini(&conf->ct_l_dom);
}

M0_INTERNAL void m0t1fs_init(struct sim *s, struct m0t1fs_conf *conf)
{
	unsigned      i;
	unsigned      j;
	struct m0_fid gfid0;
	struct m0_fid p_id  = {0x123, 0x456};
	struct m0_fid pv_id = {0x789, 0x101112};
	int           result;

	m0_pool_init(&conf->ct_pool, &p_id, 0);
	m0_pool_version_init(&conf->ct_pool_version, &pv_id, &conf->ct_pool,
			     conf->ct_nr_servers * conf->ct_nr_devices,
			     conf->ct_nr_servers, conf->ct_N, conf->ct_K);
	conf->ct_srv = sim_alloc(conf->ct_nr_servers * sizeof conf->ct_srv[0]);
	for (i = 0; i < conf->ct_nr_servers; ++i) {
		struct net_srv *srv = &conf->ct_srv[i];

		*srv = *conf->ct_srv0;
		srv->ns_nr_devices = conf->ct_nr_devices;
		net_srv_init(s, srv);
		sim_name_set(&srv->ns_name, "%u", i);
	}

	conf->ct_client = sim_alloc(conf->ct_nr_clients *
				    sizeof conf->ct_client[0]);

	layout_build(conf);
	m0_fid_set(&gfid0, 0, 999);

	for (i = 0; i < conf->ct_nr_clients; ++i) {
		struct m0t1fs_client *c = &conf->ct_client[i];

		c->cc_conf   = conf;
		c->cc_id     = i;
		c->cc_thread = sim_alloc(conf->ct_nr_threads *
					 sizeof c->cc_thread[0]);
		c->cc_srv    = sim_alloc(conf->ct_nr_servers *
					 sizeof c->cc_srv[0]);
		for (j = 0; j < conf->ct_nr_servers; ++j)
			sim_chan_init(&c->cc_srv[j].cs_wakeup,
				      "inflight:%i:%i", i, j);
		for (j = 0; j < conf->ct_nr_threads; ++j) {
			struct m0t1fs_thread *th = &c->cc_thread[j];
			uint64_t              delta;
			struct m0_fid         gfid;

			th->cth_id     = j;
			th->cth_client = c;

			delta = i * conf->ct_client_step +
				j * conf->ct_thread_step;

			gfid.f_container = gfid0.f_container + delta;
			gfid.f_key       = gfid0.f_key       - delta;

			result = m0_layout_instance_build(
					m0_pdl_to_layout(conf->ct_pdclust),
					&gfid, &th->cth_layout_instance);
			M0_ASSERT(result == 0);
		}
	}
	sim_timer_add(s, 0, threads_start, conf);
}

M0_INTERNAL void m0t1fs_fini(struct m0t1fs_conf *conf)
{
	unsigned                    i;
	unsigned                    j;

	if (conf->ct_client != NULL) {
		for (i = 0; i < conf->ct_nr_clients; ++i) {
			struct m0t1fs_client *c = &conf->ct_client[i];

			if (c->cc_thread != NULL) {
				for (j = 0; j < conf->ct_nr_threads; ++j) {
					struct m0t1fs_thread *cth;

					cth = &c->cc_thread[j];
					sim_thread_fini(&cth->cth_thread);
					m0_layout_instance_fini(
						cth->cth_layout_instance);
				}
				sim_free(c->cc_thread);
			}
			if (c->cc_srv != NULL) {
				for (j = 0; j < conf->ct_nr_servers; ++j)
					sim_chan_fini(&c->cc_srv[j].cs_wakeup);
				sim_free(c->cc_srv);
			}
		}
		sim_free(conf->ct_client);
	}
	if (conf->ct_srv != NULL) {
		for (i = 0; i < conf->ct_nr_servers; ++i)
			net_srv_fini(&conf->ct_srv[i]);
	}
	m0t1fs_layout_fini(conf);
	m0_pool_fini(&conf->ct_pool);
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
