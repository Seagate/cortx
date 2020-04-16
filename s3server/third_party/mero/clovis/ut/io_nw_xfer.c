/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 20-Oct-2014
 */

#include "layout/layout.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"        /* M0_LOG */
#include "lib/uuid.h"         /* m0_uuid_generate */

#include "ut/ut.h"            /* M0_UT_ASSERT */
#include "clovis/ut/clovis.h"

/*
 * Including the c files so we can replace the M0_CLOVIS_PRE asserts
 * in order to test them.
 */
#if defined(round_down)
#undef round_down
#endif
#if defined(round_up)
#undef round_up
#endif
#include "clovis/io_nw_xfer.c"

#include "layout/layout_internal.h" /* REMOVE ME */

struct m0_ut_suite        ut_suite_clovis_io_nw_xfer;
static struct m0_clovis  *dummy_instance;

#define DUMMY_PTR 0xdeafdead

#define UT_DEFAULT_BLOCK_SIZE (1ULL << CLOVIS_DEFAULT_BUF_SHIFT)

static void ut_clovis_test_io_di_size(void)
{
	/* XXX Base case (dealing with m0_resource is needed) */
}

/**
 * Helper function for ut_clovis_test_tioreqs_hash_func().
 */
static void ut_clovis_helper_tioreqs_hash_func(uint64_t b_nr, uint64_t key,
						  uint64_t exp_ret)
{
	struct m0_htable        htable;
	uint64_t                hash;

	htable.h_bucket_nr = b_nr;
	hash = tioreqs_hash_func(&htable, (void *)&key);
	M0_UT_ASSERT(hash == exp_ret);
}

/**
 * Tests tioreqs_hash_func().
 */
static void ut_clovis_test_tioreqs_hash_func(void)
{
	uint64_t                key;

	/* Keep gcc quiet during debug build */
	M0_SET0(&key);

	/* Base cases: bucket_nr == 2 */
	ut_clovis_helper_tioreqs_hash_func(2, 1, 1);
	ut_clovis_helper_tioreqs_hash_func(2, 777, 1);
	ut_clovis_helper_tioreqs_hash_func(2, 12345, 1);
	ut_clovis_helper_tioreqs_hash_func(2, 2, 0);
	ut_clovis_helper_tioreqs_hash_func(2, 18, 0);
	ut_clovis_helper_tioreqs_hash_func(2, 5000, 0);

	/* Base case: bucket_nr == 1 */
	ut_clovis_helper_tioreqs_hash_func(1, 12345, 0);
	ut_clovis_helper_tioreqs_hash_func(1, 2, 0);
	ut_clovis_helper_tioreqs_hash_func(1, 23431423, 0);
	ut_clovis_helper_tioreqs_hash_func(1, 0, 0);
}

/**
 * Helper function for ut_clovis_test_tioreq_key_eq().
 */
static void ut_clovis_helper_tioreq_key_eq(uint64_t k1, uint64_t k2,
					      bool exp_ret)
{
	bool eq;

	eq = tioreq_key_eq(&k1, &k2);
	M0_UT_ASSERT(eq == exp_ret);
}

/**
 * Tests tioreq_key_eq().
 */
static void ut_clovis_test_tioreq_key_eq(void)
{
	uint64_t k1;
	uint64_t k2;

	/* Keep gcc quiet during debug build */
	M0_SET0(&k1);
	M0_SET0(&k2);

	/* Base case. */
	ut_clovis_helper_tioreq_key_eq(2, 2, true);
	ut_clovis_helper_tioreq_key_eq(123456, 123456, true);
	ut_clovis_helper_tioreq_key_eq(123456, 2, false);
}

/**
 * Tests target_ioreq_invariant().
 */
static void ut_clovis_test_target_ioreq_invariant(void)
{
	struct target_ioreq    *ti;
	bool                    ret;

	/* Base case. */
	ti = ut_clovis_dummy_target_ioreq_create();
	ret = target_ioreq_invariant(ti);
	M0_UT_ASSERT(ret == true);
	ut_clovis_dummy_target_ioreq_delete(ti);
}

/**
 * Tests target_session().
 */
static void ut_clovis_test_target_session(void)
{
	struct m0_fid               fid;
	struct m0_fid               tfid;
	struct m0_clovis           *instance;
	struct m0_reqh_service_ctx *ctx;
	struct m0_clovis_realm      realm;
	struct m0_clovis_entity     entity;
	struct m0_clovis_op_io     *ioo;
	struct m0_rpc_session      *session;
	struct m0_pool_version     *pv;

	/* initialise clovis */
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &entity, instance);

	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_oo.oo_oc.oc_op.op_entity = &entity;

	/* Keep gcc quiet during debug build */
	M0_SET0(&tfid);

	/* Base case. */
	m0_fid_gob_make(&fid, 0, 0);
	m0_fid_convert_gob2cob(&fid, &tfid, 0);

	pv = instance->m0c_pools_common.pc_cur_pver;
	pv->pv_pc = &instance->m0c_pools_common;
	pv->pv_pc->pc_nr_devices = 1;
	M0_ALLOC_ARR(pv->pv_pc->pc_dev2svc, 1);
	M0_ALLOC_PTR(ctx);
	ctx->sc_type = M0_CST_IOS;
	pv->pv_pc->pc_dev2svc[0].pds_ctx = ctx;

	ioo->ioo_pver = pv->pv_id;
	session = target_session(ioo, tfid);
	M0_UT_ASSERT(session == &ctx->sc_rlink.rlk_sess);

	m0_free(pv->pv_pc->pc_dev2svc);
	m0_free(ctx);

	/* fini */
	m0_clovis_entity_fini(&entity);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests target_ioreq_locate().
 */
static void ut_clovis_test_target_ioreq_locate(void)
{
	struct nw_xfer_request *xfer;
	struct m0_fid          *fid;
	struct target_ioreq    *ti;
	struct target_ioreq    *aux_ti;

	/* Base case: Found. */
	M0_ALLOC_PTR(fid);
	m0_fid_set(fid, 0xDEAD, 0xBEEF);
	xfer = ut_clovis_dummy_xfer_req_create();
	tioreqht_htable_init(&xfer->nxr_tioreqs_hash, 1);
	ti = ut_clovis_dummy_target_ioreq_create();
	m0_fid_set(&ti->ti_fid, 0xDEAD, 0xBEEF);
	tioreqht_htable_add(&xfer->nxr_tioreqs_hash, ti);

	aux_ti = target_ioreq_locate(xfer, fid);
	M0_UT_ASSERT(aux_ti == ti);

	tioreqht_htable_del(&xfer->nxr_tioreqs_hash, ti);
	ut_clovis_dummy_target_ioreq_delete(ti);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);
	ut_clovis_dummy_xfer_req_delete(xfer);
	m0_free(fid);

	/* Base case: Not found. */
	M0_ALLOC_PTR(fid);
	m0_fid_set(fid, 0xDEAD, 0xBEEF);
	xfer = ut_clovis_dummy_xfer_req_create();
	tioreqht_htable_init(&xfer->nxr_tioreqs_hash, 1);
	ti = target_ioreq_locate(xfer, fid);
	M0_UT_ASSERT(ti == NULL);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);
	ut_clovis_dummy_xfer_req_delete(xfer);
	m0_free(fid);
}

/**
 * Tests target_ioreq_seg_add().
 */
static void ut_clovis_test_target_ioreq_seg_add(void)
{
	struct target_ioreq            *ti;
	struct m0_clovis_op_io         *ioo;
	struct m0_clovis               *instance;
	struct m0_pdclust_src_addr     *src;
	struct m0_pdclust_tgt_addr     *tgt;
	struct pargrp_iomap            *map;
	struct m0_pdclust_layout       *play;

	/* see ut_clovis_dummy_pdclust_layout_create() */
	const uint32_t unit_size = UT_DEFAULT_BLOCK_SIZE;

	/* Initialise clovis. */
	instance = dummy_instance;

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	M0_ALLOC_PTR(src);
	M0_ALLOC_PTR(tgt);
	src->sa_unit = 1;
	tgt->ta_frame = 1;
	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	m0_free(map->pi_databufs[0][0]->db_buf.b_addr);/* don't use this allocated buf*/
	map->pi_ioo = ioo;
	map->pi_databufs[0][0]->db_buf.b_addr = NULL;
	map->pi_databufs[0][0]->db_flags |= 777;

	play = pdlayout_get(ioo);
	play->pl_attr.pa_unit_size = unit_size;

	ti = ut_clovis_dummy_target_ioreq_create();
	ti->ti_nwxfer = &ioo->ioo_nwxfer;
	m0_indexvec_alloc(&ti->ti_ivec, 1);
	ti->ti_ivec.iv_vec.v_nr = 0;
	m0_bufvec_alloc(&ti->ti_bufvec, 1, unit_size);
	m0_free(ti->ti_bufvec.ov_buf[0]); /* don't use this buf*/
	m0_bufvec_alloc(&ti->ti_auxbufvec, 1, unit_size);
	M0_ALLOC_ARR(ti->ti_pageattrs, 1);

	target_ioreq_seg_add(ti, src, tgt, 111, 1, map);
	M0_UT_ASSERT(ti->ti_ivec.iv_vec.v_nr == 1);
	M0_UT_ASSERT(ti->ti_ivec.iv_index[0] == unit_size + 111);
	M0_UT_ASSERT(ti->ti_ivec.iv_vec.v_count[0] == 1);
	M0_UT_ASSERT(ti->ti_bufvec.ov_vec.v_count[0] == 1);
	M0_UT_ASSERT(ti->ti_bufvec.ov_buf[0] == NULL);
	M0_UT_ASSERT(ti->ti_pageattrs[0] & PA_DATA);
	M0_UT_ASSERT(ti->ti_pageattrs[0] & 777);

	/* we want to re-use ti - free this one */
	m0_free(ti->ti_pageattrs);
	m0_bufvec_free(&ti->ti_bufvec);
	m0_bufvec_free(&ti->ti_auxbufvec);
	m0_indexvec_free(&ti->ti_ivec);

	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
	m0_free(tgt);
	m0_free(src);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_bulk_buffer_add(void)
{
	/* XXX Base case (when io_desc_size can be tested) */

}

static void ut_clovis_test_irfop_fini(void)
{
	/* XXX: Base case (if you feel like testing rpc_bulk) */
}

static void ut_clovis_test_target_ioreq_iofops_prepare(void)
{
	/* XXX Base case (when bulk_buffer_add is testable). */
}

/**
 * Tests target_ioreq_init().
 */
static void ut_clovis_test_target_ioreq_init(void)
{
	struct target_ioreq    *ti;
	struct m0_fid           gfid;
	struct m0_fid           fid;
	struct m0_rpc_session  *session;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance;
	int                     rc;
	struct m0_clovis_realm  realm;

	/* initialise clovis */
	instance = dummy_instance;

	/* Base case. */
	m0_fid_gob_make(&gfid, 0, 1);
	m0_fid_convert_gob2cob(&gfid, &fid, 0);
	session = (struct m0_rpc_session *)DUMMY_PTR;
	M0_ALLOC_PTR(ti);
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ut_clovis_realm_entity_setup(&realm, ioo->ioo_oo.oo_oc.oc_op.op_entity,
				     instance);
	rc = target_ioreq_init(ti, &ioo->ioo_nwxfer, &fid, 777,
			       session, UT_DEFAULT_BLOCK_SIZE );
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ti->ti_rc == 0);
	M0_UT_ASSERT(ti->ti_ops == &tioreq_ops);
	M0_UT_ASSERT(m0_fid_cmp(&ti->ti_fid, &fid) == 0);
	M0_UT_ASSERT(ti->ti_nwxfer == &ioo->ioo_nwxfer);
	M0_UT_ASSERT(ti->ti_dgvec == NULL);
	M0_UT_ASSERT(ti->ti_state == M0_PNDS_ONLINE);
	M0_UT_ASSERT(ti->ti_session == session);
	M0_UT_ASSERT(ti->ti_parbytes == 0);
	M0_UT_ASSERT(ti->ti_databytes == 0);
	M0_UT_ASSERT(ti->ti_obj == 777);
	M0_UT_ASSERT(ti->ti_bufvec.ov_vec.v_nr == 1);
	M0_UT_ASSERT(ti->ti_bufvec.ov_vec.v_count != NULL);
	M0_UT_ASSERT(ti->ti_bufvec.ov_buf != NULL);
	M0_UT_ASSERT(ti->ti_pageattrs != NULL);
	M0_UT_ASSERT(ti->ti_ivec.iv_vec.v_nr == 0);

	target_ioreq_fini(ti);

	m0_clovis_entity_fini(ioo->ioo_oo.oo_oc.oc_op.op_entity);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests target_ioreq_fini().
 */
static void ut_clovis_test_target_ioreq_fini(void)
{
	struct target_ioreq    *ti;
	int                     rc;

	ti = ut_clovis_dummy_target_ioreq_create();

	/* Base case. */
	tioreqht_tlink_init(ti);
	ti->ti_dgvec = NULL;
	M0_ALLOC_PTR(ti->ti_bufvec.ov_buf);
	M0_ALLOC_PTR(ti->ti_bufvec.ov_vec.v_count);
	M0_ALLOC_PTR(ti->ti_auxbufvec.ov_buf);
	M0_ALLOC_PTR(ti->ti_auxbufvec.ov_vec.v_count);
	M0_ALLOC_PTR(ti->ti_pageattrs);
	rc = m0_indexvec_alloc(&ti->ti_ivec, 1);
	M0_UT_ASSERT(rc == 0);
	target_ioreq_fini(ti);
}

/**
 * Tests nw_xfer_request_invariant().
 */
static void ut_clovis_test_nw_xfer_request_invariant(void)
{
	struct nw_xfer_request *xfer;
	bool                    ret;

	/* Base case. */
	xfer = ut_clovis_dummy_xfer_req_create();
	ret = nw_xfer_request_invariant(xfer);
	/* XXX: Other base cases can be covered. */
	M0_UT_ASSERT(ret == true);
	ut_clovis_dummy_xfer_req_delete(xfer);

}

/**
 * Tests nw_xfer_tioreq_get().
 * @see ut_clovis_test_target_ioreq_init() and
 * ut_clovis_test_target_ioreq_locate().
 */
static void ut_clovis_test_nw_xfer_tioreq_get(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_fid           gfid;
	struct m0_fid           fid;
	struct m0_rpc_session  *session;
	struct target_ioreq    *out;
	struct m0_clovis       *instance;
	int                     rc;
	struct m0_clovis_realm  realm;

	/* Initialise clovis. */
	instance = dummy_instance;

	/* Base case. */
	M0_SET0(&out);
	session = (struct m0_rpc_session *)DUMMY_PTR;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ut_clovis_realm_entity_setup(&realm, ioo->ioo_oo.oo_oc.oc_op.op_entity,
				     instance);
	tioreqht_htable_init(&ioo->ioo_nwxfer.nxr_tioreqs_hash, 1);
	m0_fid_gob_make(&gfid, 0, 1);
	m0_fid_convert_gob2cob(&gfid, &fid, 0);
	rc = nw_xfer_tioreq_get(&ioo->ioo_nwxfer, &fid, 777,
				session, UT_DEFAULT_BLOCK_SIZE, &out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(out != NULL);
	tioreqht_htable_del(&ioo->ioo_nwxfer.nxr_tioreqs_hash, out);
	target_ioreq_fini(out);

	tioreqht_htable_fini(&ioo->ioo_nwxfer.nxr_tioreqs_hash);
	m0_clovis_entity_fini(ioo->ioo_oo.oo_oc.oc_op.op_entity);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_nw_xfer_io_distribute(void)
{
}

/**
 * Tests nw_xfer_req_complete().
 */
static void ut_clovis_test_nw_xfer_req_complete(void)
{
	struct m0_clovis_op_io  *ioo;
	struct m0_clovis        *instance;
	struct m0_clovis_realm   realm;
	struct m0_clovis_entity  entity;

	/* Initialise. */
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &entity, instance);

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_oo.oo_oc.oc_op.op_entity = &entity;
	tioreqht_htable_init(&ioo->ioo_nwxfer.nxr_tioreqs_hash, 1);

	ioo->ioo_nwxfer.nxr_rc = -777;
	ioo->ioo_nwxfer.nxr_bytes = 99;
	ioo->ioo_sm.sm_state = IRS_READ_COMPLETE;
	m0_sm_group_lock(ioo->ioo_sm.sm_grp);
	nw_xfer_req_complete(&ioo->ioo_nwxfer, true);
	m0_sm_group_unlock(ioo->ioo_sm.sm_grp);
	M0_UT_ASSERT(ioo->ioo_rc == -777);
	M0_UT_ASSERT(ioo->ioo_nwxfer.nxr_bytes == 0);

	tioreqht_htable_fini(&ioo->ioo_nwxfer.nxr_tioreqs_hash);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_nw_xfer_req_dispatch(void)
{
}

static void ut_clovis_test_nw_xfer_tioreq_map(void)
{
}

/**
 * Tests nw_xfer_request_init().
 */
static void ut_clovis_test_nw_xfer_request_init(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance;
	struct nw_xfer_request *xfer;

	/* initialise clovis */
	instance = dummy_instance;

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	nw_xfer_request_init(&ioo->ioo_nwxfer);
	xfer = &ioo->ioo_nwxfer;
	M0_UT_ASSERT(xfer->nxr_rc == 0);
	M0_UT_ASSERT(xfer->nxr_bytes == 0);
	M0_UT_ASSERT(xfer->nxr_rc == 0);
	M0_UT_ASSERT(m0_atomic64_get(&xfer->nxr_iofop_nr) == 0);
	M0_UT_ASSERT(m0_atomic64_get(&xfer->nxr_rdbulk_nr) == 0);
	M0_UT_ASSERT(xfer->nxr_state == NXS_INITIALIZED);
	M0_UT_ASSERT(xfer->nxr_ops == &xfer_ops);

	/* clean after the function */
	m0_mutex_fini(&xfer->nxr_lock);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests nw_xfer_request_fini().
 */
static void ut_clovis_test_nw_xfer_request_fini(void)
{
	struct nw_xfer_request *xfer;

	/* Base case. */
	M0_ALLOC_PTR(xfer);
	xfer->nxr_state = NXS_COMPLETE;
	nw_xfer_request_bob_init(xfer);
	xfer->nxr_ops = (struct nw_xfer_ops *)DUMMY_PTR;
	m0_mutex_init(&xfer->nxr_lock);
	tioreqht_htable_init(&xfer->nxr_tioreqs_hash, 1);
	nw_xfer_request_fini(xfer);
	M0_UT_ASSERT(xfer->nxr_ops == NULL);
	m0_free(xfer);
}

static void ut_clovis_test_dgmode_rwvec_alloc_init(void)
{
	int                     rc;
	struct m0_clovis       *instance;
	struct m0_clovis_op_io *ioo;
	struct target_ioreq    *ti;

	/* init */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ti = ut_clovis_dummy_target_ioreq_create();
	ti->ti_nwxfer = &ioo->ioo_nwxfer;

	/* base cases */
	rc = dgmode_rwvec_alloc_init(ti);
	M0_UT_ASSERT(rc == 0);
	dgmode_rwvec_dealloc_fini(ti->ti_dgvec); /* a shortcut to fix memory leak*/

	/* fini */
	ut_clovis_dummy_target_ioreq_delete(ti);
	ut_clovis_dummy_ioo_delete(ioo, instance);

}

static void ut_clovis_test_dgmode_rwvec_dealloc_fini(void)
{
	int                     rc;
	struct m0_clovis       *instance;
	struct m0_clovis_op_io *ioo;
	struct target_ioreq    *ti;

	/* init */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ti = ut_clovis_dummy_target_ioreq_create();
	ti->ti_nwxfer = &ioo->ioo_nwxfer;

	/* base cases */
	rc = dgmode_rwvec_alloc_init(ti);
	M0_UT_ASSERT(rc == 0);

	dgmode_rwvec_dealloc_fini(ti->ti_dgvec);

	/* fini */
	ut_clovis_dummy_target_ioreq_delete(ti);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

M0_INTERNAL int m0_clovis_io_nw_xfer_ut_init(void)
{
	int                       rc;
	struct m0_pdclust_layout *dummy_pdclust_layout;

#ifndef __KERNEL__
	ut_clovis_shuffle_test_order(&ut_suite_clovis_io_nw_xfer);
#endif

	m0_clovis_init_io_op();

	rc = ut_m0_clovis_init(&dummy_instance);
	M0_UT_ASSERT(rc == 0);

	ut_clovis_layout_domain_fill(dummy_instance);
	dummy_pdclust_layout =
		ut_clovis_dummy_pdclust_layout_create(dummy_instance);
	M0_UT_ASSERT(dummy_pdclust_layout != NULL);

	return 0;
}

M0_INTERNAL int m0_clovis_io_nw_xfer_ut_fini(void)
{
	ut_clovis_layout_domain_empty(dummy_instance);
	ut_m0_clovis_fini(&dummy_instance);
	return 0;
}

struct m0_ut_suite ut_suite_clovis_io_nw_xfer = {
	.ts_name = "clovis-io-nw-xfer-ut",
	.ts_init = m0_clovis_io_nw_xfer_ut_init,
	.ts_fini = m0_clovis_io_nw_xfer_ut_fini,
	.ts_tests = {

		{ "io_di_size",
				    &ut_clovis_test_io_di_size},
		{ "tioreqs_hash_func",
				    &ut_clovis_test_tioreqs_hash_func},
		{ "tioreq_key_eq",
				    &ut_clovis_test_tioreq_key_eq},
		{ "target_ioreq_invariant",
				    &ut_clovis_test_target_ioreq_invariant},
		{ "target_session",
				    &ut_clovis_test_target_session},
		{ "target_ioreq_locate",
				    &ut_clovis_test_target_ioreq_locate},
		{ "target_ioreq_seg_add",
				    &ut_clovis_test_target_ioreq_seg_add},
		{ "bulk_buffer_add",
				    &ut_clovis_test_bulk_buffer_add},
		{ "irfop_fini",
				    &ut_clovis_test_irfop_fini},
		{ "target_ioreq_iofops_prepare",
				    &ut_clovis_test_target_ioreq_iofops_prepare},
		{ "target_ioreq_init",
				    &ut_clovis_test_target_ioreq_init},
		{ "target_ioreq_fini",
				    &ut_clovis_test_target_ioreq_fini},
		{ "nw_xfer_request_invariant",
				    &ut_clovis_test_nw_xfer_request_invariant},
		{ "nw_xfer_tioreq_get",
				    &ut_clovis_test_nw_xfer_tioreq_get},
		{ "nw_xfer_io_distribute",
				    &ut_clovis_test_nw_xfer_io_distribute},
		{ "nw_xfer_req_complete",
				    &ut_clovis_test_nw_xfer_req_complete},
		{ "nw_xfer_req_dispatch",
				    &ut_clovis_test_nw_xfer_req_dispatch},
		{ "nw_xfer_tioreq_map",
				    &ut_clovis_test_nw_xfer_tioreq_map},
		{ "nw_xfer_request_init",
				    &ut_clovis_test_nw_xfer_request_init},
		{ "nw_xfer_request_fini",
				    &ut_clovis_test_nw_xfer_request_fini},
		{ "dgmode_rwvec_alloc_init",
				    &ut_clovis_test_dgmode_rwvec_alloc_init},
		{ "dgmode_rwvec_dealloc_fini",
				    &ut_clovis_test_dgmode_rwvec_dealloc_fini},
		{ NULL, NULL },
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
