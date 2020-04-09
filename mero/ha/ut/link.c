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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 7-Apr-2016
 */


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/link.h"
#include "ut/ut.h"

#include "lib/time.h"           /* m0_time_now */
#include "lib/arith.h"          /* m0_rnd64 */
#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/string.h"         /* m0_strdup */
#include "fid/fid.h"            /* M0_FID0 */
#include "reqh/reqh_service.h"  /* m0_reqh_service */
#include "ut/threads.h"         /* M0_UT_THREADS_DEFINE */
#include "ha/ut/helper.h"       /* m0_ha_ut_rpc_ctx */
#include "ha/link_service.h"    /* m0_ha_link_service_init */

struct ha_ut_link_ctx {
	struct m0_ha_link          ulc_link;
	struct m0_clink            ulc_stop_clink;
	struct m0_ha_link_cfg      ulc_cfg;
	struct m0_ha_link_conn_cfg ulc_conn_cfg;
};

static void ha_ut_link_conn_cfg_create(struct m0_ha_link_conn_cfg *hl_conn_cfg,
                                       struct m0_uint128          *id_local,
                                       struct m0_uint128          *id_remote,
                                       struct m0_uint128         *id_connection,
                                       bool                        tag_even,
                                       const char                 *ep)
{
	*hl_conn_cfg = (struct m0_ha_link_conn_cfg){
		.hlcc_params             = {
			.hlp_id_local      = *id_local,
			.hlp_id_remote     = *id_remote,
			.hlp_id_connection = *id_connection,
		},
		.hlcc_rpc_service_fid    = M0_FID0,
		.hlcc_rpc_endpoint       = m0_strdup(ep),
		.hlcc_max_rpcs_in_flight = M0_HA_UT_MAX_RPCS_IN_FLIGHT,
		.hlcc_connect_timeout    = M0_MKTIME(5, 0),
		.hlcc_disconnect_timeout = M0_MKTIME(5, 0),
		.hlcc_resend_interval    = M0_MKTIME(0, 15000000),
		.hlcc_nr_sent_max        = 1,
	};
	m0_ha_link_tags_initial(&hl_conn_cfg->hlcc_params.hlp_tags_local,
				tag_even);
	m0_ha_link_tags_initial(&hl_conn_cfg->hlcc_params.hlp_tags_remote,
				!tag_even);
	M0_UT_ASSERT(hl_conn_cfg->hlcc_rpc_endpoint != NULL);
}

static void ha_ut_link_cfg_create(struct m0_ha_link_cfg   *hl_cfg,
                                  struct m0_ha_ut_rpc_ctx *rpc_ctx,
                                  struct m0_reqh_service  *hl_service)
{
	*hl_cfg = (struct m0_ha_link_cfg){
		.hlc_reqh         = &rpc_ctx->hurc_reqh,
		.hlc_reqh_service = hl_service,
		.hlc_rpc_machine  = &rpc_ctx->hurc_rpc_machine,
		.hlq_q_cfg_in     = {},
		.hlq_q_cfg_out    = {},
	};
}

static void ha_ut_link_conn_cfg_free(struct m0_ha_link_conn_cfg *hl_conn_cfg)
{
	m0_free((char *)hl_conn_cfg->hlcc_rpc_endpoint);
}

static void ha_ut_link_init(struct ha_ut_link_ctx   *link_ctx,
                            struct m0_ha_ut_rpc_ctx *rpc_ctx,
                            struct m0_reqh_service  *hl_service,
                            struct m0_uint128       *id_local,
                            struct m0_uint128       *id_remote,
                            struct m0_uint128       *id_connection,
                            bool                     tag_even,
                            bool                     start)
{
	int rc;

	m0_clink_init(&link_ctx->ulc_stop_clink, NULL);
	link_ctx->ulc_stop_clink.cl_is_oneshot = true;
	ha_ut_link_cfg_create(&link_ctx->ulc_cfg, rpc_ctx, hl_service);
	rc = m0_ha_link_init(&link_ctx->ulc_link, &link_ctx->ulc_cfg);
	M0_UT_ASSERT(rc == 0);
	ha_ut_link_conn_cfg_create(&link_ctx->ulc_conn_cfg, id_local, id_remote,
	                           id_connection, tag_even,
	                         m0_rpc_machine_ep(&rpc_ctx->hurc_rpc_machine));
	if (start)
		m0_ha_link_start(&link_ctx->ulc_link, &link_ctx->ulc_conn_cfg);
}

static void ha_ut_link_fini(struct ha_ut_link_ctx *link_ctx)
{
	m0_ha_link_stop(&link_ctx->ulc_link, &link_ctx->ulc_stop_clink);
	m0_chan_wait(&link_ctx->ulc_stop_clink);
	m0_ha_link_fini(&link_ctx->ulc_link);
	ha_ut_link_conn_cfg_free(&link_ctx->ulc_conn_cfg);
	m0_clink_fini(&link_ctx->ulc_stop_clink);
}

static void ha_ut_link_set_some_msg(struct m0_ha_msg *msg)
{
	*msg = (struct m0_ha_msg){
		.hm_fid            = M0_FID_INIT(1, 2),
		.hm_source_process = M0_FID_INIT(3, 4),
		.hm_source_service = M0_FID_INIT(5, 6),
		.hm_time           = m0_time_now(),
		.hm_data = {
			.hed_type = M0_HA_MSG_STOB_IOQ,
			.u.hed_stob_ioq = {
				.sie_conf_sdev = M0_FID_INIT(7, 8),
				.sie_size      = 0x100,
			},
		},
	};
}

void m0_ha_ut_link_usecase(void)
{
	struct m0_ha_ut_rpc_ctx *rpc_ctx;
	struct m0_reqh_service  *hl_service;
	struct ha_ut_link_ctx   *ctx1;
	struct ha_ut_link_ctx   *ctx2;
	struct m0_ha_link       *hl1;
	struct m0_ha_link       *hl2;
	struct m0_uint128        id1 = M0_UINT128(0, 0);
	struct m0_uint128        id2 = M0_UINT128(0, 1);
	struct m0_uint128        id  = M0_UINT128(1, 2);
	struct m0_ha_msg        *msg;
	struct m0_ha_msg        *msg_recv;
	uint64_t                 tag;
	uint64_t                 tag1;
	uint64_t                 tag2;
	int                      rc;

	M0_ALLOC_PTR(rpc_ctx);
	M0_UT_ASSERT(rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(rpc_ctx);
	rc = m0_ha_link_service_init(&hl_service, &rpc_ctx->hurc_reqh);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_PTR(ctx1);
	M0_UT_ASSERT(ctx1 != NULL);
	ha_ut_link_init(ctx1, rpc_ctx, hl_service, &id1, &id2, &id, true, true);
	M0_ALLOC_PTR(ctx2);
	M0_UT_ASSERT(ctx2 != NULL);
	ha_ut_link_init(ctx2, rpc_ctx, hl_service, &id2, &id1, &id, false,
			true);
	hl1 = &ctx1->ulc_link;
	hl2 = &ctx2->ulc_link;

	/* One way transmission. Message is sent from hl1 to hl2.  */
	M0_ALLOC_PTR(msg);
	ha_ut_link_set_some_msg(msg);
	msg_recv = m0_ha_link_recv(hl2, &tag1);
	M0_UT_ASSERT(msg_recv == NULL);
	m0_ha_link_send(hl1, msg, &tag);
	m0_ha_link_wait_arrival(hl2);
	msg_recv = m0_ha_link_recv(hl2, &tag1);
	M0_UT_ASSERT(msg_recv != NULL);
	M0_UT_ASSERT(tag == tag1);
	M0_UT_ASSERT(m0_ha_msg_tag(msg_recv) == tag1);
	M0_UT_ASSERT(m0_ha_msg_eq(msg_recv, msg));
	tag2 = m0_ha_link_delivered_consume(hl1);
	M0_UT_ASSERT(tag2 == M0_HA_MSG_TAG_INVALID);
	m0_ha_link_delivered(hl2, msg_recv);
	m0_ha_link_wait_delivery(hl1, tag);
	tag2 = m0_ha_link_delivered_consume(hl1);
	M0_UT_ASSERT(tag1 == tag2);
	tag2 = m0_ha_link_not_delivered_consume(hl1);
	M0_UT_ASSERT(tag2 == M0_HA_MSG_TAG_INVALID);
	m0_free(msg);
	m0_ha_link_flush(hl1);
	m0_ha_link_flush(hl2);

	ha_ut_link_fini(ctx2);
	m0_free(ctx2);
	ha_ut_link_fini(ctx1);
	m0_free(ctx1);
	m0_ha_link_service_fini(hl_service);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
};

enum {
	HA_UT_THREAD_PAIR_NR = 0x10,
	HA_UT_MSG_PER_THREAD = 0x20,
};

struct ha_ut_link_mt_test {
	struct m0_ha_ut_rpc_ctx *ulmt_ctx;
	struct m0_reqh_service  *ulmt_hl_service;
	struct ha_ut_link_ctx    ulmt_link_ctx;
	struct m0_uint128        ulmt_id_local;
	struct m0_uint128        ulmt_id_remote;
	struct m0_uint128        ulmt_id_connection;
	bool                     ulmt_tag_even;
	struct m0_semaphore      ulmt_barrier_done;
	struct m0_semaphore      ulmt_barrier_wait;
	struct m0_ha_msg        *ulmt_msgs_out;
	struct m0_ha_msg        *ulmt_msgs_in;
	uint64_t                *ulmt_tags_out;
	uint64_t                *ulmt_tags_in;
};

static void ha_ut_link_mt_thread(void *param)
{
	struct ha_ut_link_mt_test  *test = param;
	struct m0_ha_link          *hl;
	struct m0_ha_msg          **msgs;
	struct m0_ha_msg           *msg;
	uint64_t                    tag;
	int                         i;
	int                         j;

	ha_ut_link_init(&test->ulmt_link_ctx, test->ulmt_ctx,
			test->ulmt_hl_service, &test->ulmt_id_local,
			&test->ulmt_id_remote, &test->ulmt_id_connection,
			test->ulmt_tag_even, true);
	/* barrier with the main thread */
	m0_semaphore_up(&test->ulmt_barrier_wait);
	m0_semaphore_down(&test->ulmt_barrier_done);

	hl = &test->ulmt_link_ctx.ulc_link;
	for (i = 0; i < HA_UT_MSG_PER_THREAD; ++i) {
		m0_ha_link_send(hl, &test->ulmt_msgs_out[i],
				&test->ulmt_tags_out[i]);
	}
	i = 0;
	M0_ALLOC_ARR(msgs, HA_UT_MSG_PER_THREAD);
	M0_UT_ASSERT(msgs != NULL);
	while (i < HA_UT_MSG_PER_THREAD) {
		m0_ha_link_wait_arrival(hl);
		j = i;
		while (1) {
			M0_UT_ASSERT(i <= HA_UT_MSG_PER_THREAD);
			msg = m0_ha_link_recv(hl, &test->ulmt_tags_in[i]);
			if (msg == NULL)
				break;
			M0_UT_ASSERT(i < HA_UT_MSG_PER_THREAD);
			msgs[i] = msg;
			test->ulmt_msgs_in[i] = *msg;
			++i;
		}
		M0_ASSERT(j < i);
		for ( ; j < i; ++j)
			m0_ha_link_delivered(hl, msgs[j]);
	}
	m0_free(msgs);
	for (i = 0; i < HA_UT_MSG_PER_THREAD; ++i) {
		m0_ha_link_wait_delivery(hl, test->ulmt_tags_out[i]);
		tag = m0_ha_link_delivered_consume(hl);
		M0_UT_ASSERT(tag == test->ulmt_tags_out[i]);
	}
	tag = m0_ha_link_delivered_consume(hl);
	M0_UT_ASSERT(tag == M0_HA_MSG_TAG_INVALID);
	tag = m0_ha_link_not_delivered_consume(hl);
	M0_UT_ASSERT(tag == M0_HA_MSG_TAG_INVALID);
	m0_ha_link_flush(hl);

	/* barrier with the main thread */
	m0_semaphore_up(&test->ulmt_barrier_wait);
	m0_semaphore_down(&test->ulmt_barrier_done);

	ha_ut_link_fini(&test->ulmt_link_ctx);
}

M0_UT_THREADS_DEFINE(ha_ut_link_mt, &ha_ut_link_mt_thread);

void m0_ha_ut_link_multithreaded(void)
{
	struct m0_ha_ut_rpc_ctx   *rpc_ctx;
	struct m0_reqh_service    *hl_service;
	struct ha_ut_link_mt_test *tests;
	struct ha_ut_link_mt_test *test1;
	struct ha_ut_link_mt_test *test2;
	struct m0_uint128          id1;
	struct m0_uint128          id2;
	struct m0_uint128          id;
	uint64_t                   seed = 42;
	int                        rc;
	int                        i;
	int                        j;

	M0_ALLOC_PTR(rpc_ctx);
	M0_UT_ASSERT(rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(rpc_ctx);
	rc = m0_ha_link_service_init(&hl_service, &rpc_ctx->hurc_reqh);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_ARR(tests, HA_UT_THREAD_PAIR_NR * 2);
	M0_UT_ASSERT(tests != NULL);

	for (i = 0; i < HA_UT_THREAD_PAIR_NR; ++i) {
		id1 = M0_UINT128(i * 2,     m0_rnd64(&seed));
		id2 = M0_UINT128(i * 2 + 1, m0_rnd64(&seed));
		id  = M0_UINT128(i,         m0_rnd64(&seed));
		tests[i * 2] = (struct ha_ut_link_mt_test){
			.ulmt_id_local      = id1,
			.ulmt_id_remote     = id2,
			.ulmt_id_connection = id,
			.ulmt_tag_even      = true,
		};
		tests[i * 2 + 1] = (struct ha_ut_link_mt_test){
			.ulmt_id_local      = id2,
			.ulmt_id_remote     = id1,
			.ulmt_id_connection = id,
			.ulmt_tag_even      = false,
		};
	}
	for (i = 0; i < HA_UT_THREAD_PAIR_NR * 2; ++i) {
		tests[i].ulmt_ctx        = rpc_ctx;
		tests[i].ulmt_hl_service = hl_service;
		rc = m0_semaphore_init(&tests[i].ulmt_barrier_done, 0);
		M0_UT_ASSERT(rc == 0);
		rc = m0_semaphore_init(&tests[i].ulmt_barrier_wait, 0);
		M0_UT_ASSERT(rc == 0);
		M0_ALLOC_ARR(tests[i].ulmt_msgs_out, HA_UT_MSG_PER_THREAD);
		M0_UT_ASSERT(tests[i].ulmt_msgs_out != NULL);
		M0_ALLOC_ARR(tests[i].ulmt_msgs_in,  HA_UT_MSG_PER_THREAD);
		M0_UT_ASSERT(tests[i].ulmt_msgs_in != NULL);
		for (j = 0; j < HA_UT_MSG_PER_THREAD; ++j) {
			tests[i].ulmt_msgs_out[j] = (struct m0_ha_msg){
				.hm_fid = M0_FID_INIT(m0_rnd64(&seed),
				                      m0_rnd64(&seed)),
				.hm_source_process = M0_FID_INIT(
					 m0_rnd64(&seed), m0_rnd64(&seed)),
				.hm_source_service = M0_FID_INIT(
					 m0_rnd64(&seed), m0_rnd64(&seed)),
				.hm_time = m0_time_now(),
				.hm_data = {
					.hed_type = M0_HA_MSG_STOB_IOQ,
				}
			};
		}
		M0_ALLOC_ARR(tests[i].ulmt_tags_out, HA_UT_MSG_PER_THREAD);
		M0_UT_ASSERT(tests[i].ulmt_tags_out != NULL);
		M0_ALLOC_ARR(tests[i].ulmt_tags_in, HA_UT_MSG_PER_THREAD);
		M0_UT_ASSERT(tests[i].ulmt_tags_in != NULL);
	}
	M0_UT_THREADS_START(ha_ut_link_mt, HA_UT_THREAD_PAIR_NR * 2, tests);
	/* Barriers with all threads. One after init, another one before fini */
	for (j = 0; j < 2; ++j) {
		for (i = 0; i < HA_UT_THREAD_PAIR_NR * 2; ++i)
			m0_semaphore_down(&tests[i].ulmt_barrier_wait);
		for (i = 0; i < HA_UT_THREAD_PAIR_NR * 2; ++i)
			m0_semaphore_up(&tests[i].ulmt_barrier_done);
	}
	M0_UT_THREADS_STOP(ha_ut_link_mt);

	for (i = 0; i < HA_UT_THREAD_PAIR_NR; ++i) {
		test1 = &tests[i * 2];
		test2 = &tests[i * 2 + 1];
		for (j = 0; j < HA_UT_MSG_PER_THREAD; ++j) {
			M0_UT_ASSERT(test1->ulmt_tags_in[j] ==
				     test2->ulmt_tags_out[j]);
			M0_UT_ASSERT(test1->ulmt_tags_out[j] ==
				     test2->ulmt_tags_in[j]);
			M0_UT_ASSERT(m0_ha_msg_eq(&test1->ulmt_msgs_in[j],
			                          &test2->ulmt_msgs_out[j]));
			M0_UT_ASSERT(m0_ha_msg_eq(&test1->ulmt_msgs_out[j],
			                          &test2->ulmt_msgs_in[j]));
		}
	}
	for (i = 0; i < HA_UT_THREAD_PAIR_NR * 2; ++i) {
		m0_free(tests[i].ulmt_tags_in);
		m0_free(tests[i].ulmt_tags_out);
		m0_free(tests[i].ulmt_msgs_in);
		m0_free(tests[i].ulmt_msgs_out);
		m0_semaphore_fini(&tests[i].ulmt_barrier_wait);
		m0_semaphore_fini(&tests[i].ulmt_barrier_done);
	}
	m0_free(tests);
	m0_ha_link_service_fini(hl_service);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
}

static void ha_ut_links_init(struct m0_ha_ut_rpc_ctx  **rpc_ctx,
                             struct m0_reqh_service   **hl_service,
                             int                        nr_links,
                             struct ha_ut_link_ctx   ***ctx,
                             struct m0_ha_link       ***hl,
                             struct m0_uint128         *id1,
                             struct m0_uint128         *id2,
                             struct m0_uint128         *id,
                             int                       *tag_even,
                             bool                       start)
{
	int i;
	int rc;

	M0_ALLOC_PTR(*rpc_ctx);
	M0_UT_ASSERT(*rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(*rpc_ctx);
	rc = m0_ha_link_service_init(hl_service, &(*rpc_ctx)->hurc_reqh);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_ARR(*ctx, nr_links);
	M0_UT_ASSERT(*ctx != NULL);
	M0_ALLOC_ARR(*hl, nr_links);
	M0_UT_ASSERT(*hl != NULL);
	for (i = 0; i < nr_links; ++i) {
		M0_ALLOC_PTR((*ctx)[i]);
		M0_UT_ASSERT((*ctx)[i] != NULL);
		ha_ut_link_init((*ctx)[i], *rpc_ctx, *hl_service,
				&id1[i], &id2[i], &id[i], tag_even[i] != 0,
				start);
		(*hl)[i] = &(*ctx)[i]->ulc_link;
	}
}

static void ha_ut_links_fini(struct m0_ha_ut_rpc_ctx  *rpc_ctx,
                             struct m0_reqh_service   *hl_service,
                             int                       nr_links,
                             struct ha_ut_link_ctx   **ctx,
                             struct m0_ha_link       **hl)
{
	int i;

	for (i = 0; i < nr_links; ++i) {
		ha_ut_link_fini(ctx[i]);
		m0_free(ctx[i]);
	}
	m0_free(hl);
	m0_free(ctx);
	m0_ha_link_service_fini(hl_service);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
}

static void ha_ut_link_msg_transfer(struct m0_ha_link *hl1,
                                    struct m0_ha_link *hl2)
{
	struct m0_ha_msg *msg;
	struct m0_ha_msg *msg_recv;
	uint64_t          tag;
	uint64_t          tag_recv;

	M0_ALLOC_PTR(msg);
	M0_UT_ASSERT(msg != NULL);

	m0_ha_link_send(hl1, msg, &tag);
	m0_ha_link_wait_arrival(hl2);
	msg_recv = m0_ha_link_recv(hl2, &tag_recv);
	M0_UT_ASSERT(m0_ha_msg_eq(msg, msg_recv));
	M0_UT_ASSERT(tag == tag_recv);
	m0_ha_link_delivered(hl2, msg_recv);
	m0_ha_link_flush(hl1);
	tag = m0_ha_link_delivered_consume(hl1);
	M0_UT_ASSERT(tag_recv == tag);

	m0_free(msg);
}

void m0_ha_ut_link_reconnect_simple(void)
{
	struct m0_ha_link_conn_cfg *hl_conn_cfg;
	struct m0_ha_ut_rpc_ctx    *rpc_ctx;
	struct m0_reqh_service     *hl_service;
	struct m0_ha_link_params    lp0;
	struct m0_ha_link_params    lp0_new;
	struct m0_ha_link_params    lp2;
	struct m0_ha_link_cfg      *hl_cfg;
	struct m0_ha_link          *hl;
	struct m0_uint128           id1            = M0_UINT128(1, 2);
	struct m0_uint128           id2            = M0_UINT128(3, 4);
	struct m0_uint128           id3            = M0_UINT128(5, 6);
	struct m0_uint128           id4            = M0_UINT128(7, 8);
	struct m0_uint128           id_connection1 = M0_UINT128(9, 10);
	struct m0_uint128           id_connection2 = M0_UINT128(11, 12);
	struct m0_clink             stop_clink = {};
	const char                 *ep;
	int                         i;
	int                         rc;

	M0_ALLOC_PTR(rpc_ctx);
	M0_UT_ASSERT(rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(rpc_ctx);
	ep = m0_rpc_machine_ep(&rpc_ctx->hurc_rpc_machine);
	rc = m0_ha_link_service_init(&hl_service, &rpc_ctx->hurc_reqh);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_ARR(hl, 3);
	M0_UT_ASSERT(hl != NULL);
	M0_ALLOC_ARR(hl_cfg, 3);
	M0_UT_ASSERT(hl_cfg != NULL);
	M0_ALLOC_ARR(hl_conn_cfg, 3);
	M0_UT_ASSERT(hl_conn_cfg != NULL);
	m0_clink_init(&stop_clink, NULL);
	stop_clink.cl_is_oneshot = true;

	for (i = 0; i < 3; ++i) {
		ha_ut_link_cfg_create(&hl_cfg[i], rpc_ctx, hl_service);
		rc = m0_ha_link_init(&hl[i], &hl_cfg[i]);
		M0_UT_ASSERT(rc == 0);
	}

	/* start hl[0] and hl[1] connected to each other */
	ha_ut_link_conn_cfg_create(&hl_conn_cfg[0], &id1, &id2, &id_connection1,
	                           true, ep);
	ha_ut_link_conn_cfg_create(&hl_conn_cfg[1], &id2, &id1, &id_connection1,
	                           false, ep);
	for (i = 0; i < 2; ++i) {
		m0_ha_link_start(&hl[i], &hl_conn_cfg[i]);
		ha_ut_link_conn_cfg_free(&hl_conn_cfg[i]);
	}

	/* send a message from hl[0] to hl[1] */
	ha_ut_link_msg_transfer(&hl[0], &hl[1]);

	/* stop & fini hl[1] */
	m0_ha_link_stop(&hl[1], &stop_clink);
	m0_chan_wait(&stop_clink);
	m0_ha_link_fini(&hl[1]);

	/* reconnect hl[0] to hl[2] */
	m0_ha_link_reconnect_begin(&hl[0], &lp0);
	ha_ut_link_conn_cfg_create(&hl_conn_cfg[0], &id3, &id4, &id_connection2,
	                           true, ep);
	ha_ut_link_conn_cfg_create(&hl_conn_cfg[2], &id4, &id3, &id_connection2,
	                           true, ep);
	m0_ha_link_reconnect_params(&lp0, &lp0_new, &lp2, &id3, &id4,
				    &id_connection2);
	hl_conn_cfg[0].hlcc_params = lp0_new;
	hl_conn_cfg[2].hlcc_params = lp2;
	m0_ha_link_start(&hl[2], &hl_conn_cfg[2]);
	m0_ha_link_reconnect_end(&hl[0], &hl_conn_cfg[0]);
	ha_ut_link_conn_cfg_free(&hl_conn_cfg[0]);
	ha_ut_link_conn_cfg_free(&hl_conn_cfg[2]);

	/* send a message from hl[0] to hl[2] */
	ha_ut_link_msg_transfer(&hl[0], &hl[2]);

	/* stop & fini hl[0] and hl[2] */
	m0_ha_link_stop(&hl[0], &stop_clink);
	m0_chan_wait(&stop_clink);
	m0_ha_link_fini(&hl[0]);
	m0_ha_link_stop(&hl[2], &stop_clink);
	m0_chan_wait(&stop_clink);
	m0_ha_link_fini(&hl[2]);

	m0_free(hl_conn_cfg);
	m0_free(hl_cfg);
	m0_free(hl);
	m0_ha_link_service_fini(hl_service);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
}

enum {
	HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS = 0x2,
};

void m0_ha_ut_link_reconnect_multiple(void)
{
	struct m0_ha_link_conn_cfg   hl_conn_cfg[2];
	struct m0_ha_link_params    lp0;
	struct m0_ha_ut_rpc_ctx     *rpc_ctx;
	struct m0_reqh_service      *hl_service;
	struct ha_ut_link_ctx      **ctx;
	struct m0_ha_link          **hl;
	struct m0_uint128           *id1;
	struct m0_uint128           *id2;
	struct m0_uint128           *id;
	struct m0_ha_msg            *msg;
	struct m0_ha_msg            *msg_recv;
	const char                  *ep;
	int                         *tag_even;
	uint64_t                     seed = 42;
	uint64_t                     tag;
	uint64_t                     tag_recv;
	uint64_t                     tag2;
	int                          i;

	M0_ALLOC_ARR(id1, HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS);
	M0_UT_ASSERT(id1 != NULL);
	M0_ALLOC_ARR(id2, HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS);
	M0_UT_ASSERT(id2 != NULL);
	M0_ALLOC_ARR(id, HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS);
	M0_UT_ASSERT(id != NULL);
	M0_ALLOC_ARR(tag_even, HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS);
	M0_UT_ASSERT(tag_even != NULL);
	M0_ALLOC_PTR(msg);
	M0_UT_ASSERT(msg != NULL);
	for (i = 0; i < HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS; ++i) {
		id1[i]      = M0_UINT128(m0_rnd64(&seed), m0_rnd64(&seed));
		id2[i]      = M0_UINT128(m0_rnd64(&seed), m0_rnd64(&seed));
		id[i]       = M0_UINT128(m0_rnd64(&seed), m0_rnd64(&seed));
		tag_even[i] = i == 0;
	}
	ha_ut_links_init(&rpc_ctx, &hl_service,
			 HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS,
			 &ctx, &hl, id1, id2, id, tag_even, false);
	m0_ha_link_start(hl[0], &ctx[0]->ulc_conn_cfg);

	/* this message shouldn't be delivered at all */
	m0_ha_link_send(hl[0], msg, &tag);
	for (i = 1; i < HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS; ++i) {
		msg_recv = m0_ha_link_recv(hl[i], &tag_recv);
		M0_UT_ASSERT(msg_recv == NULL);
	}
	/* now reconnect hl[0] to all other links */
	ep = m0_rpc_machine_ep(&rpc_ctx->hurc_rpc_machine);
	for (i = 1; i < HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS; ++i) {
		msg_recv = m0_ha_link_recv(hl[i], &tag_recv);
		M0_UT_ASSERT(msg_recv == NULL);
		m0_ha_link_reconnect_begin(hl[0], &lp0);
		ha_ut_link_conn_cfg_create(&hl_conn_cfg[1], &id1[i], &id2[i],
					   &id[i], !tag_even[i], ep);
		ha_ut_link_conn_cfg_create(&hl_conn_cfg[0], &id2[i], &id1[i],
					   &id[i], tag_even[i], ep);
		m0_ha_link_reconnect_params(&lp0, &hl_conn_cfg[0].hlcc_params,
					    &hl_conn_cfg[1].hlcc_params,
					    &id1[i], &id2[i], &id[i]);
		m0_ha_link_reconnect_end(hl[0], &hl_conn_cfg[0]);
		ha_ut_link_conn_cfg_free(&hl_conn_cfg[0]);
		m0_ha_link_start(hl[i], &hl_conn_cfg[1]);
		ha_ut_link_conn_cfg_free(&hl_conn_cfg[1]);
		if (i == 1) {
			m0_ha_link_wait_arrival(hl[i]);
			msg_recv = m0_ha_link_recv(hl[i], &tag_recv);
			M0_UT_ASSERT(msg_recv != NULL);
			M0_UT_ASSERT(m0_ha_msg_eq(msg_recv, msg));
			M0_UT_ASSERT(tag_recv == tag);
			m0_ha_link_delivered(hl[i], msg_recv);
			m0_ha_link_wait_delivery(hl[0], tag);
			tag2 = m0_ha_link_delivered_consume(hl[0]);
			M0_UT_ASSERT(tag2 == tag);
		}
	}

	ha_ut_links_fini(rpc_ctx, hl_service,
			 HA_UT_LINK_RECONNECT_MULTIPLE_NR_LINKS, ctx, hl);
	m0_free(msg);
	m0_free(tag_even);
	m0_free(id);
	m0_free(id2);
	m0_free(id1);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
