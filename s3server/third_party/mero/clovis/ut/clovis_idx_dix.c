/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 30-Apr-2016
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#include "lib/misc.h"               /* M0_SRC_PATH */
#include "lib/finject.h"
#include "ut/ut.h"
#include "ut/misc.h"                /* M0_UT_CONF_PROFILE */
#include "rpc/rpclib.h"             /* m0_rpc_server_ctx */
#include "fid/fid.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_idx.h"
#include "dix/layout.h"
#include "dix/client.h"
#include "dix/meta.h"
#include "fop/fom_simple.h"     /* m0_fom_simple */

#define WAIT_TIMEOUT               M0_TIME_NEVER
#define SERVER_LOG_FILE_NAME       "cas_server.log"

static struct m0_clovis         *ut_m0c;
static struct m0_clovis_config   ut_m0c_config;
static struct m0_idx_dix_config  ut_dix_config;

enum {
	MAX_RPCS_IN_FLIGHT = 10,
	CNT = 10,
};

static char *cas_startup_cmd[] = { "m0d", "-T", "linux",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
                                "-e", "lnet:0@lo:12345:34:1",
                                "-H", "0@lo:12345:34:1",
				"-w", "10", "-F",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_SRC_PATH("clovis/ut/dix_conf.xc")};

static const char         *local_ep_addr = "0@lo:12345:34:2";
static const char         *srv_ep_addr   = { "0@lo:12345:34:1" };
static const char         *process_fid   = M0_UT_CONF_PROCESS;
static struct m0_net_xprt *cs_xprts[]    = { &m0_net_lnet_xprt };

static struct m0_rpc_server_ctx dix_ut_sctx = {
		.rsx_xprts            = cs_xprts,
		.rsx_xprts_nr         = ARRAY_SIZE(cs_xprts),
		.rsx_argv             = cas_startup_cmd,
		.rsx_argc             = ARRAY_SIZE(cas_startup_cmd),
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
};

static void dix_config_init()
{
	struct m0_fid pver = M0_FID_TINIT('v', 1, 100);
	int           rc;
	struct m0_ext range[] = {{ .e_start = 0, .e_end = IMASK_INF }};

	/* Create meta indices (root, layout, layout-descr). */
	rc = m0_dix_ldesc_init(&ut_dix_config.kc_layout_ldesc, range,
			       ARRAY_SIZE(range), HASH_FNC_FNV1, &pver);
	M0_UT_ASSERT(rc == 0);
	rc = m0_dix_ldesc_init(&ut_dix_config.kc_ldescr_ldesc, range,
			       ARRAY_SIZE(range), HASH_FNC_FNV1, &pver);
	M0_UT_ASSERT(rc == 0);
	/*
	 * mero/setup.c creates meta indices now. Therefore, we must not
	 * create it twice or it will fail with -EEXIST error.
	 */
	ut_dix_config.kc_create_meta = false;
}

static void dix_config_fini()
{
	m0_dix_ldesc_fini(&ut_dix_config.kc_layout_ldesc);
	m0_dix_ldesc_fini(&ut_dix_config.kc_ldescr_ldesc);
}

static void idx_dix_ut_clovis_init()
{
	int rc;

	ut_m0c = NULL;
	ut_m0c_config.cc_is_oostore            = true;
	ut_m0c_config.cc_is_read_verify        = false;
	ut_m0c_config.cc_local_addr            = local_ep_addr;
	ut_m0c_config.cc_ha_addr               = srv_ep_addr;
	ut_m0c_config.cc_profile               = M0_UT_CONF_PROFILE;
	/* Use fake fid, see clovis_initlift_resource_manager(). */
	ut_m0c_config.cc_process_fid           = process_fid;
	ut_m0c_config.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	ut_m0c_config.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	ut_m0c_config.cc_idx_service_id        = M0_CLOVIS_IDX_DIX;
	ut_m0c_config.cc_idx_service_conf      = &ut_dix_config;

	m0_fi_enable_once("clovis_ha_init", "skip-ha-init");
	/* Skip HA finalisation in case of failure path. */
	m0_fi_enable("clovis_ha_fini", "skip-ha-fini");
	/*
	 * We can't use m0_fi_enable_once() here, because
	 * clovis_initlift_addb2() may be called twice in case of failure path.
	 */
	m0_fi_enable("clovis_initlift_addb2", "no-addb2");
	m0_fi_enable("clovis_ha_process_event", "no-link");
	rc = m0_clovis_init(&ut_m0c, &ut_m0c_config, false);
	M0_UT_ASSERT(rc == 0);
	m0_fi_disable("clovis_ha_process_event", "no-link");
	m0_fi_disable("clovis_initlift_addb2", "no-addb2");
	m0_fi_disable("clovis_ha_fini", "skip-ha-fini");
	ut_m0c->m0c_mero = m0_get();
}

static void idx_dix_ut_init()
{
	int rc;

	M0_SET0(&dix_ut_sctx.rsx_mero_ctx);
	rc = m0_rpc_server_start(&dix_ut_sctx);
	M0_ASSERT(rc == 0);
	dix_config_init();
	idx_dix_ut_clovis_init();
}

static void idx_dix_ut_fini()
{
	/*
	 * Meta-indices are destroyed automatically during m0_rpc_server_stop()
	 * along with the whole BE data.
	 */
	dix_config_fini();
	m0_fi_enable_once("clovis_ha_fini", "skip-ha-fini");
	m0_fi_enable_once("clovis_initlift_addb2", "no-addb2");
	m0_fi_enable("clovis_ha_process_event", "no-link");
	m0_clovis_fini(ut_m0c, false);
	m0_fi_disable("clovis_ha_process_event", "no-link");
	m0_rpc_server_stop(&dix_ut_sctx);
}

static void ut_dix_init_fini(void)
{
	idx_dix_ut_init();
	idx_dix_ut_fini();
}

static int *rcs_alloc(int count)
{
	int  i;
	int *rcs;

	M0_ALLOC_ARR(rcs, count);
	M0_UT_ASSERT(rcs != NULL);
	for (i = 0; i < count; i++)
		/* Set to some value to assert that UT actually changed rc. */
		rcs[i] = 0xdb;
	return rcs;
}

static uint8_t ifid_type(bool dist)
{
	return dist ? m0_dix_fid_type.ft_id : m0_cas_index_fid_type.ft_id;
}

static void general_ifid_fill(struct m0_fid *ifid, bool dist)
{
	*ifid = M0_FID_TINIT(ifid_type(dist), 2, 1);
}

static void ut_dix_namei_ops(bool dist)
{
	struct m0_clovis_container realm;
	struct m0_clovis_idx       idx;
	struct m0_clovis_idx       idup;
	struct m0_fid              ifid;
	struct m0_clovis_op       *op = NULL;
	struct m0_bufvec           keys;
	int                       *rcs;
	int                        rc;

	idx_dix_ut_init();
	m0_clovis_container_init(&realm, NULL, &M0_CLOVIS_UBER_REALM, ut_m0c);

	general_ifid_fill(&ifid, dist);
	/* Create the index. */
	m0_clovis_idx_init(&idx, &realm.co_realm, (struct m0_uint128 *)&ifid);
	rc = m0_clovis_entity_create(NULL, &idx.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_fini(op);
	m0_free0(&op);

	/* Create an index with the same fid once more => -EEXIST. */
	m0_clovis_idx_init(&idup, &realm.co_realm, (struct m0_uint128 *)&ifid);
	rc = m0_clovis_entity_create(NULL, &idup.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_rc == -EEXIST);
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_clovis_idx_fini(&idup);

	/* Check that index exists. */
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_LOOKUP, NULL, NULL, NULL, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_fini(op);
	m0_free0(&op);

	/* List all indices (only one exists). */
	rcs = rcs_alloc(2);
	rc = m0_bufvec_alloc(&keys, 2, sizeof(struct m0_fid));
	M0_UT_ASSERT(rc == 0);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_LIST, &keys, NULL, rcs, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rcs[0] == 0);
	M0_UT_ASSERT(rcs[1] == -ENOENT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == 2);
	M0_UT_ASSERT(keys.ov_vec.v_count[0] == sizeof(struct m0_fid));
	M0_UT_ASSERT(m0_fid_eq(keys.ov_buf[0], &ifid));
	M0_UT_ASSERT(keys.ov_vec.v_count[1] == sizeof(struct m0_fid));
	M0_UT_ASSERT(!m0_fid_is_set(keys.ov_buf[1]));
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);
	m0_bufvec_free(&keys);

	/* Delete the index. */
	rc = m0_clovis_entity_delete(&idx.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_fini(op);
	m0_free0(&op);

	/* Delete an index with the same fid once more => -ENOENT. */
	m0_clovis_idx_init(&idup, &realm.co_realm, (struct m0_uint128 *)&ifid);
	rc = m0_clovis_entity_open(&idup.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	rc = m0_clovis_entity_delete(&idup.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_rc == -ENOENT);
	m0_clovis_op_fini(op);
	m0_free0(&op);

	m0_clovis_idx_fini(&idx);
	idx_dix_ut_fini();
}

static void ut_dix_namei_ops_dist(void)
{
	ut_dix_namei_ops(true);
}

static void ut_dix_namei_ops_non_dist(void)
{
	ut_dix_namei_ops(false);
}

static uint64_t dix_key(uint64_t i)
{
	return 100 + i;
}

static uint64_t dix_val(uint64_t i)
{
	return 100 + i * i;
}

static void ut_dix_record_ops(bool dist)
{
	struct m0_clovis_container realm;
	struct m0_clovis_idx       idx;
	struct m0_fid              ifid;
	struct m0_clovis_op       *op = NULL;
	struct m0_bufvec           keys;
	struct m0_bufvec           vals;
	uint64_t                   i;
	bool                       eof;
	uint64_t                   accum;
	uint64_t                   recs_nr;
	uint64_t                   cur_key;
	int                        rc;
	int                       *rcs;

	idx_dix_ut_init();
	general_ifid_fill(&ifid, dist);
	m0_clovis_container_init(&realm, NULL, &M0_CLOVIS_UBER_REALM, ut_m0c);
	m0_clovis_idx_init(&idx, &realm.co_realm, (struct m0_uint128 *)&ifid);

	/* Create index. */
	rc = m0_clovis_entity_create(NULL, &idx.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_fini(op);
	m0_free0(&op);

	/* Get non-existing key. */
	rcs = rcs_alloc(1);
	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t)) ?:
	     m0_bufvec_empty_alloc(&vals, 1);
	M0_UT_ASSERT(rc == 0);
	*(uint64_t*)keys.ov_buf[0] = dix_key(10);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_GET, &keys, &vals, rcs, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(vals.ov_buf[0] == NULL);
	M0_UT_ASSERT(vals.ov_vec.v_count[0] == 0);
	M0_UT_ASSERT(rcs[0] == -ENOENT);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);


	/* Add records to the index. */
	rcs = rcs_alloc(CNT);
	rc = m0_bufvec_alloc(&keys, CNT, sizeof(uint64_t)) ?:
	     m0_bufvec_alloc(&vals, CNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < keys.ov_vec.v_nr; i++) {
		*(uint64_t *)keys.ov_buf[i] = dix_key(i);
		*(uint64_t *)vals.ov_buf[i] = dix_val(i);
	}
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_PUT, &keys, &vals, rcs, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc  == 0);
	M0_UT_ASSERT(m0_forall(i, CNT,
			       rcs[i] == 0 &&
		               *(uint64_t *)vals.ov_buf[i] == dix_val(i)));
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	/* Try to add recs again without OVERWRITE flag. */
	rcs = rcs_alloc(CNT);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_PUT, &keys, &vals, rcs, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(op->op_sm.sm_rc == 0);
	M0_UT_ASSERT(m0_forall(i, CNT, rcs[i] == -EEXIST));
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	/* Try to add recs again with OVERWRITE flag. */
	rcs = rcs_alloc(CNT);
	for (i = 0; i < keys.ov_vec.v_nr; i++)
		*(uint64_t *)vals.ov_buf[i] = dix_val(i * 10);

	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_PUT, &keys, &vals, rcs,
			      M0_OIF_OVERWRITE, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_sm.sm_rc == 0);
	M0_UT_ASSERT(m0_forall(i, CNT, rcs[i] == 0));
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);

	/* Get records from the index by keys. */
	rcs = rcs_alloc(CNT);
	rc = m0_bufvec_alloc(&keys, CNT, sizeof(uint64_t)) ?:
	     m0_bufvec_empty_alloc(&vals, CNT);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < keys.ov_vec.v_nr; i++)
		*(uint64_t*)keys.ov_buf[i] = dix_key(i);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_GET, &keys, &vals, rcs, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, CNT,
			       rcs[i] == 0 &&
		               *(uint64_t *)vals.ov_buf[i] == dix_val(i * 10)));
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	/* Get records with all existing keys, except the one. */
	rcs = rcs_alloc(CNT);
	rc = m0_bufvec_alloc(&keys, CNT, sizeof(uint64_t)) ?:
	     m0_bufvec_empty_alloc(&vals, CNT);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < keys.ov_vec.v_nr; i++)
		*(uint64_t*)keys.ov_buf[i] = dix_key(i);
	*(uint64_t *)keys.ov_buf[5] = dix_key(999);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_GET, &keys, &vals, rcs, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < CNT; i++) {
		if (i != 5) {
			M0_UT_ASSERT(rcs[i] == 0);
			M0_UT_ASSERT(*(uint64_t *)vals.ov_buf[i] ==
							dix_val(i * 10));
		} else {
			M0_UT_ASSERT(rcs[i] == -ENOENT);
			M0_UT_ASSERT(vals.ov_buf[5] == NULL);
		}
	}
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	/* Iterate over all records in the index. */
	rcs = rcs_alloc(CNT + 1);
	rc = m0_bufvec_empty_alloc(&keys, CNT + 1) ?:
	     m0_bufvec_empty_alloc(&vals, CNT + 1);
	M0_UT_ASSERT(rc == 0);
	cur_key = dix_key(0);
	keys.ov_buf[0] = &cur_key;
	keys.ov_vec.v_count[0] = sizeof(uint64_t);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_NEXT, &keys, &vals, rcs, 0,
			      &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, CNT,
			       rcs[i] == 0 &&
			       *(uint64_t*)keys.ov_buf[i] == dix_key(i) &&
			       *(uint64_t*)vals.ov_buf[i] == dix_val(i * 10)));
	M0_UT_ASSERT(rcs[CNT] == -ENOENT);
	M0_UT_ASSERT(keys.ov_buf[CNT] == NULL);
	M0_UT_ASSERT(vals.ov_buf[CNT] == NULL);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	/* Iterate over all records in the index excluding the start key. */
	rcs = rcs_alloc(CNT + 1);
	rc = m0_bufvec_empty_alloc(&keys, CNT + 1) ?:
	     m0_bufvec_empty_alloc(&vals, CNT + 1);
	M0_UT_ASSERT(rc == 0);
	cur_key = dix_key(0);
	keys.ov_buf[0] = &cur_key;
	keys.ov_vec.v_count[0] = sizeof(uint64_t);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_NEXT, &keys, &vals, rcs,
			      M0_OIF_EXCLUDE_START_KEY, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, CNT - 1,
			       rcs[i] == 0 &&
			       *(uint64_t*)keys.ov_buf[i] == dix_key(i + 1) &&
			       *(uint64_t*)vals.ov_buf[i] ==
			       dix_val((i + 1) * 10)));
	M0_UT_ASSERT(rcs[CNT - 1] == -ENOENT);
	M0_UT_ASSERT(keys.ov_buf[CNT - 1] == NULL);
	M0_UT_ASSERT(vals.ov_buf[CNT - 1] == NULL);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	/* Try to add recs again with OVERWRITE flag. */
	rcs = rcs_alloc(CNT + 1);
	rc = m0_bufvec_alloc(&keys, CNT, sizeof(uint64_t)) ?:
	     m0_bufvec_alloc(&vals, CNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < keys.ov_vec.v_nr; i++) {
		*(uint64_t *)vals.ov_buf[i] = dix_val(i);
		*(uint64_t *)keys.ov_buf[i] = dix_key(i);
	}

	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_PUT, &keys, &vals, rcs,
			      M0_OIF_OVERWRITE, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_sm.sm_rc == 0);
	M0_UT_ASSERT(m0_forall(i, CNT, rcs[i] == 0));
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);
	m0_bufvec_free(&vals);
	m0_bufvec_free(&keys);
	/*
	 * Iterate over all records in the index, starting from the beginning
	 * and requesting two records at a time.
	 */
	accum = 0;
	cur_key = 0;
	do {
		rcs = rcs_alloc(2);
		rc = m0_bufvec_empty_alloc(&keys, 2) ?:
		     m0_bufvec_empty_alloc(&vals, 2);
		M0_UT_ASSERT(rc == 0);
		if (cur_key != 0) {
			keys.ov_buf[0] = &cur_key;
			keys.ov_vec.v_count[0] = sizeof(uint64_t);
		} else {
			/*
			 * Pass NULL in order to request records starting from
			 * the smallest key.
			 */
			keys.ov_buf[0] = NULL;
			keys.ov_vec.v_count[0] = 0;
		}
		rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_NEXT, &keys, &vals,
				      rcs, 0, &op);
		M0_UT_ASSERT(rc == 0);
		m0_clovis_op_launch(&op, 1);
		rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE),
				       WAIT_TIMEOUT);
		M0_UT_ASSERT(rc == 0);
		for (i = 0; i < vals.ov_vec.v_nr && rcs[i] == 0; i++)
			;
		recs_nr = i;
		eof = recs_nr < keys.ov_vec.v_nr;
		for (i = 0; i < recs_nr; i++) {
			M0_UT_ASSERT(*(uint64_t *)keys.ov_buf[i] ==
				     dix_key(accum + i));
			M0_UT_ASSERT(*(uint64_t *)vals.ov_buf[i] ==
				     dix_val(accum + i));
			cur_key = *(uint64_t *)keys.ov_buf[i];
		}
		m0_bufvec_free(&keys);
		m0_bufvec_free(&vals);
		m0_clovis_op_fini(op);
		m0_free0(&op);
		m0_free0(&rcs);
		/*
		 * Starting key is also included in returned number of records,
		 * so extract 1. The only exception is the first request, when
		 * starting key is unknown. It is accounted before accum check
		 * after eof is reached.
		 */
		accum += recs_nr - 1;
	} while (!eof);
	accum++;
	M0_UT_ASSERT(accum == CNT);

	/* Remove the records from the index. */
	rcs = rcs_alloc(CNT);
	rc = m0_bufvec_alloc(&keys, CNT, sizeof(uint64_t));
	for (i = 0; i < keys.ov_vec.v_nr; i++)
		*(uint64_t *)keys.ov_buf[i] = dix_key(i);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_DEL, &keys, NULL, rcs, 0, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, CNT, rcs[i] == 0));
	m0_bufvec_free(&keys);
	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_free0(&rcs);

	/* Remove the index. */
	rc = m0_clovis_entity_delete(&idx.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	m0_clovis_op_fini(op);
	m0_free0(&op);

	m0_clovis_idx_fini(&idx);
	idx_dix_ut_fini();
}

static void ut_dix_record_ops_dist(void)
{
	ut_dix_record_ops(true);
}

static void ut_dix_record_ops_non_dist(void)
{
	ut_dix_record_ops(false);
}

struct m0_ut_suite ut_suite_clovis_idx_dix = {
	.ts_name   = "clovis-idx-dix",
	.ts_owners = "Egor",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "init-fini",           ut_dix_init_fini,             "Egor" },
		{ "namei-ops-dist",      ut_dix_namei_ops_dist,        "Egor" },
		{ "namei-ops-non-dist",  ut_dix_namei_ops_non_dist,    "Egor" },
		{ "record-ops-dist",     ut_dix_record_ops_dist,       "Egor" },
		{ "record-ops-non-dist", ut_dix_record_ops_non_dist,   "Egor" },
		{ NULL, NULL }
	}
};

static int ut_suite_clovis_mt_idx_dix_init(void)
{
	idx_dix_ut_init();
	return 0;
}

static int ut_suite_clovis_mt_idx_dix_fini(void)
{
	idx_dix_ut_fini();
	return 0;
}

extern void clovis_st_mt(void);
extern void clovis_st_lsfid(void);

struct m0_clovis* clovis_st_get_instance()
{
	return ut_m0c;
}

struct m0_ut_suite ut_suite_clovis_mt_idx_dix = {
	.ts_name   = "clovis-idx-dix-mt",
	.ts_owners = "Anatoliy",
	.ts_init   = ut_suite_clovis_mt_idx_dix_init,
	.ts_fini   = ut_suite_clovis_mt_idx_dix_fini,
	.ts_tests  = {
		{ "fom", clovis_st_mt,    "Anatoliy" },
		{ "lsf", clovis_st_lsfid, "Anatoliy" },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

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
