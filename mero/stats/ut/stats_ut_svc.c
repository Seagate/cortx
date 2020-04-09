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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 * Original creation date: 07/31/2013
 */
#include "lib/memory.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "mero/setup.h"
#include "stats/stats_fops.h"
#include "reqh/reqh_service.h"
#include "stats/stats_api.h"

#include "stats/stats_srv.c"
#include "rpc/ut/clnt_srv_ctx.c"
struct m0_mutex ut_stats_mutex;
struct m0_cond  ut_stats_cond;
struct m0_rpc_server_ctx stats_ut_sctx_bk;
struct m0_rpc_machine ut_stats_machine;

static char *stats_ut_server_argv[] = {
        "rpclib_ut", "-T", "AD", "-D", SERVER_DB_NAME, "-w", "10",
	"-f", M0_UT_CONF_PROCESS,
        "-S", SERVER_STOB_NAME, "-A", SERVER_ADDB_STOB_NAME,
        "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
	"-c", M0_UT_PATH("conf.xc")
};

enum stats_id {
	UT_STATS_FOP_RATE = 1,
	UT_STATS_READ_SIZE,
	UT_STATS_DISK
};

enum fop_type {
	UPDATE_FOP = 1,
	QUERY_FOP
};

/* Fake stats */
struct fop_rate {
	uint64_t fr_rate;
	uint64_t fr_avg_turnaround_time_ns;
} f_rate;

struct read_size {
	uint64_t rs_avg_size;
} r_size;

struct disk_stats {
	uint64_t ds_total;
	uint64_t ds_free;
	uint64_t ds_used;
} d_stats;

struct m0_stats_sum stats_sum[3];
uint64_t                 stats_ids[] = {
	UT_STATS_FOP_RATE,
	UT_STATS_READ_SIZE,
	UT_STATS_DISK
};

static void fill_stats_input()
{

	f_rate.fr_rate = 3000;
	f_rate.fr_avg_turnaround_time_ns = 98765;
	stats_sum[0].ss_id = UT_STATS_FOP_RATE;
	stats_sum[0].ss_data.se_nr = 2;
	stats_sum[0].ss_data.se_data = (uint64_t *)&f_rate;

	r_size.rs_avg_size = 8196;
	stats_sum[1].ss_id = UT_STATS_READ_SIZE;
	stats_sum[1].ss_data.se_nr = 1;
	stats_sum[1].ss_data.se_data = (uint64_t *)&r_size;

	d_stats.ds_free  = 2678901234;
	d_stats.ds_used  = 3578901234;
	d_stats.ds_total = 6257802468;
	stats_sum[2].ss_id = UT_STATS_DISK;
	stats_sum[2].ss_data.se_nr = 3;
	stats_sum[2].ss_data.se_data = (uint64_t *)&d_stats;
}

void check_stats(struct m0_tl *stats_list, int count)
{
	int i;
	int id;

	for (i = 0, id = 1; i < count; ++i, ++id) {
		struct m0_stats *stats = m0_stats_get(stats_list, id);
		M0_UT_ASSERT(stats != NULL);
	}
}

static void stats_ut_svc_start_stop()
{
	struct m0_reqh	       *reqh;
	struct m0_reqh_service *stats_srv;
	/*
	 * Test 1: Check stats service start on mero server start
	 * 1. Start mero client-server
	 * 2. verify it's status
	 */
	stats_ut_sctx_bk = sctx;

	sctx.rsx_argv = stats_ut_server_argv;
	sctx.rsx_argc = ARRAY_SIZE(stats_ut_server_argv);

	start_rpc_client_and_server();

	reqh = m0_cs_reqh_get(&sctx.rsx_mero_ctx);
	stats_srv = m0_reqh_service_find(&m0_stats_svc_type, reqh);
	M0_UT_ASSERT(stats_srv != NULL);
	M0_UT_ASSERT(m0_reqh_service_state_get(stats_srv) == M0_RST_STARTED);

	/*
	 * Test 2: Check individual stats service stop
	 * 1. stop stats service
	 * 2. verify it's status
	 */
	m0_reqh_service_prepare_to_stop(stats_srv);
	m0_reqh_service_stop(stats_srv);
	M0_UT_ASSERT(m0_reqh_service_state_get(stats_srv) == M0_RST_STOPPED);
	m0_reqh_service_fini(stats_srv);

	stop_rpc_client_and_server();

	sctx = stats_ut_sctx_bk;
}

void fop_release(struct m0_ref *ref)
{
	struct m0_fop *fop = container_of(ref, struct m0_fop, f_ref);

	M0_UT_ASSERT(fop != NULL);
	m0_fop_fini(fop);
	m0_free(fop);
}

static void test_state_update_fom_fini(struct m0_fom *fom)
{
	stats_update_fom_fini(fom);
	m0_mutex_lock(&ut_stats_mutex);
	m0_cond_signal(&ut_stats_cond);
	m0_mutex_unlock(&ut_stats_mutex);
}

/**
 *	Fake stats update FOM operation vector to fom.
 *	It only override fom_finish op which is wrapper of original one.
 */
static const struct m0_fom_ops ut_stats_update_fom_ops = {
	.fo_tick          = stats_update_fom_tick,
	.fo_home_locality = stats_fom_home_locality,
	.fo_fini          = test_state_update_fom_fini
};

static struct m0_fop *get_fake_stats_fop(uint32_t nsum, enum fop_type type)
{
	struct m0_fop                   *fop;
	struct m0_stats_update_fop      *ufop;
	struct m0_stats_query_fop       *qfop;


	M0_ALLOC_PTR(fop);
	M0_UT_ASSERT(fop != NULL);

	switch (type) {
	case UPDATE_FOP:
		M0_ALLOC_PTR(ufop);
		M0_UT_ASSERT(ufop != NULL);

		M0_ALLOC_ARR(ufop->suf_stats.sf_stats, nsum);
		M0_UT_ASSERT(ufop->suf_stats.sf_stats != NULL);

		m0_fop_init(fop, &m0_fop_stats_update_fopt, (void *)ufop,
			    fop_release);
		break;
	case QUERY_FOP:
		M0_ALLOC_PTR(qfop);
		M0_UT_ASSERT(qfop != NULL);

		m0_fop_init(fop, &m0_fop_stats_query_fopt, (void *)qfop,
			    fop_release);
		break;
	}

	return fop;
}

static void update_fom_test(struct stats_svc *srv, struct m0_reqh *reqh,
			    int count)
{
	struct m0_fop			*fop;
	struct m0_fom			*fom;
	struct m0_stats_update_fop      *ufop;
	int				 i;
	int				 rc;

	m0_sm_group_init(&ut_stats_machine.rm_sm_grp);
	m0_mutex_init(&ut_stats_mutex);
	m0_cond_init(&ut_stats_cond, &ut_stats_mutex);

	fop = get_fake_stats_fop(count, UPDATE_FOP);
	M0_UT_ASSERT(fop != NULL);

	ufop = m0_stats_update_fop_get(fop);
	ufop->suf_stats.sf_nr = count;
	for (i = 0; i < count; ++i)
		stats_sum_copy(&stats_sum[i], &ufop->suf_stats.sf_stats[i]);

	m0_fop_rpc_machine_set(fop, &ut_stats_machine);
	rc = stats_update_fom_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == 0);

	m0_fop_put_lock(fop);

	fom->fo_ops   = &ut_stats_update_fom_ops;
	fom->fo_local = true;

	m0_mutex_lock(&ut_stats_mutex);

	m0_fom_queue(fom);
	m0_cond_wait(&ut_stats_cond);

	m0_mutex_unlock(&ut_stats_mutex);
	m0_cond_fini(&ut_stats_cond);

	check_stats(&srv->ss_stats, count);
	m0_sm_group_fini(&ut_stats_machine.rm_sm_grp);
}

static void stats_ut_svc_update_fom()
{
	struct m0_reqh	       *reqh;
	struct m0_reqh_service *reqh_srv;
	struct stats_svc       *srv;

	stats_ut_sctx_bk = sctx;

	sctx.rsx_argv = stats_ut_server_argv;
	sctx.rsx_argc = ARRAY_SIZE(stats_ut_server_argv);

	start_rpc_client_and_server();

	reqh = m0_cs_reqh_get(&sctx.rsx_mero_ctx);
	reqh_srv = m0_reqh_service_find(&m0_stats_svc_type, reqh);
	M0_UT_ASSERT(reqh_srv != NULL);
	M0_UT_ASSERT(m0_reqh_service_state_get(reqh_srv) == M0_RST_STARTED);

	srv = container_of(reqh_srv, struct stats_svc, ss_reqhs);
	stats_svc_invariant(srv);

	/*
	 * Test 1:
	 * verify creation of new stats type
	 */
	f_rate.fr_rate = 1000;
	f_rate.fr_avg_turnaround_time_ns = 1234;
	stats_sum[0].ss_id = UT_STATS_FOP_RATE;
	stats_sum[0].ss_data.se_nr = 2;
	stats_sum[0].ss_data.se_data = (uint64_t *)&f_rate;

	update_fom_test(srv, reqh, 1);

	/*
	 * Test 2:
	 * verify creation (new stats type) & update (existing stats type)
	 * from stats list
	 */
	f_rate.fr_rate = 2000;
	f_rate.fr_avg_turnaround_time_ns = 4321;
	stats_sum[0].ss_id = UT_STATS_FOP_RATE;
	stats_sum[0].ss_data.se_nr = 2;
	stats_sum[0].ss_data.se_data = (uint64_t *)&f_rate;

	r_size.rs_avg_size = 1024;
	stats_sum[1].ss_id = UT_STATS_READ_SIZE;
	stats_sum[1].ss_data.se_nr = 1;
	stats_sum[1].ss_data.se_data = (uint64_t *)&r_size;

	update_fom_test(srv, reqh, 2);

	/*
	 * Test 3:
	 * verify creation (new stats type) & multiple update
	 * (existing stats type) from stats list
	 */
	fill_stats_input();

	update_fom_test(srv, reqh, 3);

	stop_rpc_client_and_server();

	sctx = stats_ut_sctx_bk;
}

static void test_state_query_fom_fini(struct m0_fom *fom)
{
	int				     i;
	struct m0_stats_query_fop      *qfop;
	struct m0_stats_query_rep_fop  *qfop_rep;

	qfop = m0_stats_query_fop_get(fom->fo_fop);
	qfop_rep = m0_stats_query_rep_fop_get(fom->fo_rep_fop);

	M0_UT_ASSERT(qfop_rep->sqrf_stats.sf_nr == qfop->sqf_ids.se_nr);

	for (i = 0; i < qfop_rep->sqrf_stats.sf_nr; ++i) {
		struct m0_stats_sum *sum;
		sum = &(qfop_rep->sqrf_stats.sf_stats[i]);
		M0_UT_ASSERT(sum->ss_id == stats_sum[i].ss_id);
	}

	fom->fo_rep_fop->f_item.ri_rmachine = &ut_stats_machine;
	stats_query_fom_fini(fom);

	m0_mutex_lock(&ut_stats_mutex);
	m0_cond_signal(&ut_stats_cond);
	m0_mutex_unlock(&ut_stats_mutex);
}

/**
 *	Fake stats query FOM operation vector to fom.
 *	It only override fom_finish op which is wrapper of original one.
 */
static const struct m0_fom_ops ut_stats_query_fom_ops = {
	.fo_tick          = stats_query_fom_tick,
	.fo_home_locality = stats_fom_home_locality,
	.fo_fini          = test_state_query_fom_fini
};

static void query_fom_test(struct stats_svc *srv, struct m0_reqh *reqh,
			   int count)
{
	struct m0_fop			*fop;
	struct m0_fom			*fom;
	struct m0_stats_query_fop  *qfop;
	int				 rc;

	m0_sm_group_init(&ut_stats_machine.rm_sm_grp);
	m0_mutex_init(&ut_stats_mutex);
	m0_cond_init(&ut_stats_cond, &ut_stats_mutex);

	fop = get_fake_stats_fop(count, QUERY_FOP);
	M0_UT_ASSERT(fop != NULL);

	qfop = m0_stats_query_fop_get(fop);

	M0_ALLOC_ARR(qfop->sqf_ids.se_data, count);
	M0_UT_ASSERT(qfop->sqf_ids.se_data != NULL);
	qfop->sqf_ids.se_nr = count;
	memcpy(qfop->sqf_ids.se_data, stats_ids, count * sizeof(uint64_t));

	m0_fop_rpc_machine_set(fop, &ut_stats_machine);
	rc = stats_query_fom_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == 0);

	m0_fop_put_lock(fop);

	fom->fo_ops   = &ut_stats_query_fom_ops;
	fom->fo_local = true;

	m0_mutex_lock(&ut_stats_mutex);

	m0_fom_queue(fom);
	m0_cond_wait(&ut_stats_cond);

	m0_mutex_unlock(&ut_stats_mutex);
	m0_cond_fini(&ut_stats_cond);
	m0_sm_group_fini(&ut_stats_machine.rm_sm_grp);
}

static void stats_ut_svc_query_fom()
{
	struct m0_reqh	       *reqh;
	struct m0_reqh_service *reqh_srv;
	struct stats_svc       *srv;

	stats_ut_sctx_bk = sctx;

	sctx.rsx_argv = stats_ut_server_argv;
	sctx.rsx_argc = ARRAY_SIZE(stats_ut_server_argv);

	start_rpc_client_and_server();

	reqh = m0_cs_reqh_get(&sctx.rsx_mero_ctx);
	reqh_srv = m0_reqh_service_find(&m0_stats_svc_type, reqh);
	M0_UT_ASSERT(reqh_srv != NULL);
	M0_UT_ASSERT(m0_reqh_service_state_get(reqh_srv) == M0_RST_STARTED);

	srv = container_of(reqh_srv, struct stats_svc, ss_reqhs);
	stats_svc_invariant(srv);

	/*
	 * Populate stats on stats service.
	 */
	fill_stats_input();
	update_fom_test(srv, reqh, 3);

	/* Test 1 : Query single stats. */
	query_fom_test(srv, reqh, 1);

	/* Test 2 : Query two stats. */
	query_fom_test(srv, reqh, 2);

	/* Test 3 : Query all stats. */
	query_fom_test(srv, reqh, 3);

	/*
	 * Test 4 : Query stats with some undfined stats ids.
	 *          Reply fops should get ss_id for queried stats is
	 *          M0_UNDEF_STATS.
	 */
	stats_ids[1] = 9999;
	query_fom_test(srv, reqh, 3);
	stats_ids[1] = UT_STATS_READ_SIZE;

	stop_rpc_client_and_server();

	sctx = stats_ut_sctx_bk;
}

static struct m0_uint64_seq *create_stats_id_seq(int count)
{
	struct m0_uint64_seq *ids;

	M0_PRE(count != 0);

	M0_ALLOC_PTR(ids);
	M0_UT_ASSERT(ids != NULL);

	ids->se_nr = count;
	M0_ALLOC_ARR(ids->se_data, ids->se_nr);
	M0_UT_ASSERT(ids->se_data != NULL);

	memcpy(ids->se_data, stats_ids, count * sizeof(uint64_t));

	return ids;
}

static void check_stats_recs(struct m0_stats_recs *recs, int num)
{
	int i;

	M0_UT_ASSERT(recs->sf_nr == num);
	for (i = 0; i < recs->sf_nr; ++i) {
		M0_UT_ASSERT(recs->sf_stats[i].ss_id == stats_sum[i].ss_id);
	}
}

static void stats_svc_query_api()
{
	struct m0_reqh            *reqh;
	struct m0_reqh_service    *reqh_srv;
	struct stats_svc          *srv;
	struct m0_uint64_seq      *ids;
	struct m0_stats_recs      *stats_recs = NULL;
	int                        rc;

	stats_ut_sctx_bk = sctx;

	sctx.rsx_argv = stats_ut_server_argv;
	sctx.rsx_argc = ARRAY_SIZE(stats_ut_server_argv);

	start_rpc_client_and_server();

	reqh = m0_cs_reqh_get(&sctx.rsx_mero_ctx);
	reqh_srv = m0_reqh_service_find(&m0_stats_svc_type, reqh);
	M0_UT_ASSERT(reqh_srv != NULL);
	M0_UT_ASSERT(m0_reqh_service_state_get(reqh_srv) == M0_RST_STARTED);

	srv = container_of(reqh_srv, struct stats_svc, ss_reqhs);
	stats_svc_invariant(srv);

	/*
	 * Populate stats on stats service.
	 */
	fill_stats_input();

	update_fom_test(srv, reqh, 3);

	/* Test 1 : Query single stats. */
	ids = create_stats_id_seq(1);
	M0_UT_ASSERT(ids != NULL);
	rc = m0_stats_query(&cctx.rcx_session, ids, &stats_recs);
	M0_UT_ASSERT(rc == 0);
	check_stats_recs(stats_recs, 1);
	m0_stats_free(stats_recs);

	/* Test 2 : Query two stats. */
	ids = create_stats_id_seq(2);
	M0_UT_ASSERT(ids != NULL);
	rc = m0_stats_query(&cctx.rcx_session, ids, &stats_recs);
	M0_UT_ASSERT(rc == 0);
	check_stats_recs(stats_recs, 2);
	m0_stats_free(stats_recs);

	/* Test 3 : Query all stats. */
	ids = create_stats_id_seq(3);
	M0_UT_ASSERT(ids != NULL);
	rc = m0_stats_query(&cctx.rcx_session, ids, &stats_recs);
	M0_UT_ASSERT(rc == 0);
	check_stats_recs(stats_recs, 3);
	m0_stats_free(stats_recs);

	/*
	 * Test 4 : Query stats with some undfined stats ids.
	 *          Reply fops should get ss_id for queried stats is
	 *          M0_UNDEF_STATS.
	 */
	stats_ids[1] = 9999;
	ids = create_stats_id_seq(3);
	M0_UT_ASSERT(ids != NULL);
	rc = m0_stats_query(&cctx.rcx_session, ids, &stats_recs);
	M0_UT_ASSERT(rc == 0);
	check_stats_recs(stats_recs, 3);
	m0_stats_free(stats_recs);
	stats_ids[1] = UT_STATS_READ_SIZE;

	stop_rpc_client_and_server();

	sctx = stats_ut_sctx_bk;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
