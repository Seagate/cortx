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
#include "clovis/io_pargrp.c"

#include "layout/layout_internal.h" /* REMOVE ME */

static struct m0_clovis   *dummy_instance;
struct m0_ut_suite         ut_suite_clovis_io_pargrp;

#define DUMMY_PTR 0xdeafdead

#define UT_DEFAULT_BLOCK_SHIFT CLOVIS_DEFAULT_BUF_SHIFT
#define UT_DEFAULT_BLOCK_SIZE  (1ULL << CLOVIS_DEFAULT_BUF_SHIFT)

/**
 * Tests data_buf_invariant().
 */
static void ut_clovis_test_data_buf_invariant(void)
{
	struct data_buf        *db;
	struct data_buf        *aux;
	bool                    ret;

	db = ut_clovis_dummy_data_buf_create();

	/* Base case. */
	ret = data_buf_invariant(db);
	M0_UT_ASSERT(ret == true);

	/* db == NULL */
	ret = data_buf_invariant(NULL);
	M0_UT_ASSERT(ret == false);

	/* !bob_check() */
	aux = db;
	db = (struct data_buf *)0x1;
	ret = data_buf_invariant(db);
	M0_UT_ASSERT(ret == false);
	db = aux;

	/* b_nob <= 0 */
	db->db_buf.b_addr = (void *)DUMMY_PTR;
	db->db_buf.b_nob = 0;
	ret = data_buf_invariant(db);
	M0_UT_ASSERT(ret == false);

	/* fini */
	ut_clovis_dummy_data_buf_delete(db);
}

/**
 * Tests data_buf_invariant_nr().
 */
static void ut_clovis_test_data_buf_invariant_nr(void)
{
	struct pargrp_iomap            *map;
	struct m0_clovis               *instance = NULL;
	bool                            ret;
	struct data_buf                *db;
	int                             data_buf_nob;

	/* init */
	instance = dummy_instance;

	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);
	map->pi_paritybufs = NULL;

	/* Base cases. */
	ret = data_buf_invariant_nr(map);
	M0_UT_ASSERT(ret == true);

	/* Make the first loop return false */
	db = map->pi_databufs[0][0];
	M0_UT_ASSERT(db->db_buf.b_addr != NULL);
	data_buf_nob = db->db_buf.b_nob;
	db->db_buf.b_nob = 0;
	ret = data_buf_invariant_nr(map);
	M0_UT_ASSERT(ret == false);
	db->db_buf.b_nob = data_buf_nob;

	/* TODO: add tests for invariant called on paritybufs */

	/* fini */
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	map->pi_ioo = NULL;
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

/**
 * Tests data_buf_init().
 */
static void ut_clovis_test_data_buf_init(void)
{
	struct data_buf *buf;
	void            *addr;

	addr = (void *)(DUMMY_PTR & (~CLOVIS_NETBUF_MASK));
	M0_ALLOC_PTR(buf);
	M0_UT_ASSERT(buf != NULL);

	/* Check if buf is corectly set*/
	data_buf_init(buf, addr, 4096, 1);
	M0_UT_ASSERT(buf->db_buf.b_addr == addr);
	M0_UT_ASSERT(buf->db_buf.b_nob == 4096);
	M0_UT_ASSERT(buf->db_flags == 1);

	m0_free(buf);
}

/**
 * Tests data_buf_fini().
 */
static void ut_clovis_test_data_buf_fini(void)
{
	struct data_buf *buf;

	/* Check if buf is cleaned correctly*/
	buf = ut_clovis_dummy_data_buf_create();
	M0_UT_ASSERT(buf != NULL);

	buf->db_flags = 1;
	data_buf_fini(buf);
	M0_UT_ASSERT(buf->db_flags == PA_NONE);

	/* Cannot delete the dummy data_buf after data_buf_fini(). */
	m0_free(buf);
}

/**
 * Tests data_buf_dealloc_fini().
 */
static void ut_clovis_test_data_buf_dealloc_fini(void)
{
	struct data_buf *buf;

	/* Base case */
	buf = ut_clovis_dummy_data_buf_create();
	data_buf_dealloc_fini(buf);
}

/**
 * Tests data_buf_alloc_init().
 */
static void ut_clovis_test_data_buf_alloc_init(void)
{
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;
	struct m0_clovis_obj   *obj;
	struct data_buf        *buf;

	/* Pass Pre-conditions*/
	instance = dummy_instance;
	obj = ut_clovis_dummy_obj_create();
	ut_clovis_realm_entity_setup(&realm, &obj->ob_entity, instance);

	/* m0_alloc_aligned return NULL */
	obj->ob_attr.oa_bshift = 63;
	buf = data_buf_alloc_init(obj, 0);
	M0_UT_ASSERT(buf == NULL);

	/* Base case */
	obj->ob_attr.oa_bshift = CLOVIS_MIN_BUF_SHIFT;
	buf = data_buf_alloc_init(obj, 0);
	M0_UT_ASSERT(buf != NULL);
	data_buf_dealloc_fini(buf);

	/* Clean up*/
	ut_clovis_dummy_obj_delete(obj);
}

/**
 * Tests pargrp_iomap_invariant().
 */
static void ut_clovis_test_pargrp_iomap_invariant(void)
{
	bool                    ret;
	struct pargrp_iomap    *map;
	struct m0_clovis       *instance = NULL;

	/* init */
	instance = dummy_instance;
	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);

	/* Base case. */
	ret = pargrp_iomap_invariant(map);
	M0_UT_ASSERT(ret == true);

	/* map == NULL */
	ret = pargrp_iomap_invariant(NULL);
	M0_UT_ASSERT(ret == false);

	/* fini */
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

/**
 * Tests pargrp_iomap_invariant_nr().
 */
static void ut_clovis_test_pargrp_iomap_invariant_nr(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	bool                    ret;

	/* init */
	instance = dummy_instance;

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ret = pargrp_iomap_invariant_nr(ioo);
	M0_UT_ASSERT(ret == true);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_seg_collate(void)
{
}

/*
 * xxx_dummy_pargrp_iomap_create allocates memory for data bufs. But
 * pargrp_iomap_databuf_alloc requires the bufs to be null.
 */
static void ut_clovis_pargrp_iomap_free_data_buf(struct pargrp_iomap *map,
						 int row, int col)
{
	ut_clovis_dummy_data_buf_fini(map->pi_databufs[row][col]);
	m0_free(map->pi_databufs[row][col]);
	map->pi_databufs[row][col] = NULL;
}

static void ut_clovis_test_pargrp_iomap_populate(void)
{
	int                    rc;
	int                    blk_size;
	struct pargrp_iomap   *map;
	struct m0_indexvec     ivec;
	struct m0_ivec_cursor  cursor;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	/*
	 * Pre-condition: cursor == NULL.
	 * Note: pargrp_iomap_populate doenn't check this!
	 */

	/* Create a valid map to pass those pre-condition checks*/
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	ioo->ioo_pbuf_type = M0_CLOVIS_PBUF_NONE;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	map->pi_ops = &iomap_ops;
	map->pi_grpid = 0;

	/*
	 * A few simple test cases
	 * Note: indexvec[index, count] has to be multiple of block sizes
	 *
	 * 1. Only one seg
	 *    A. one block
	 *    B. multiple blocks (3 blocks)
	 *       - it spans 2 (or more) parity groups
	 *       - only spans one group
	 *
	 * 2. Multiple segs (2 for now)
	 *    A. Only spans this group
	 *    B. 1 or 2 segs spans groups
	 *
	 * 3. RMW (1 block)
	 */
	blk_size = 1UL << UT_DEFAULT_BLOCK_SHIFT;

	/* Test 1.A one segment, one block */
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);

	rc = m0_indexvec_alloc(&ivec, 1);
	M0_UT_ASSERT (rc == 0);
	ivec.iv_index[0] = 0;
	ivec.iv_vec.v_count[0] = blk_size;

	map->pi_ivec.iv_vec.v_nr = 0;
	m0_ivec_cursor_init(&cursor, &ivec);
	rc = pargrp_iomap_populate(map, &ivec, &cursor, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_indexvec_free(&ivec);
	map->pi_ivec.iv_vec.v_nr = 0;

	/* Test 1.B one segment, multiple blocks*/
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 1);

	rc = m0_indexvec_alloc(&ivec, 1);
	M0_UT_ASSERT (rc == 0);
	ivec.iv_index[0] = 0;
	ivec.iv_vec.v_count[0] = 2 * blk_size;
	ioo->ioo_data.ov_vec.v_count[0] = 2 * blk_size;

	m0_ivec_cursor_init(&cursor, &ivec);
	rc = pargrp_iomap_populate(map, &ivec, &cursor, NULL);
	M0_UT_ASSERT(rc == 0);

	m0_indexvec_free(&ivec);
	map->pi_ivec.iv_vec.v_nr = 0;
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = 0;

	/* Test 2.A 2 segments, one block each */
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);

	rc = m0_indexvec_alloc(&ivec, 2);
	M0_UT_ASSERT (rc == 0);
	ivec.iv_index[0] = 0;
	ivec.iv_vec.v_count[0] = blk_size;
	ivec.iv_index[1] = 2 * blk_size;
	ivec.iv_vec.v_count[1] = blk_size;
	ioo->ioo_data.ov_vec.v_count[0] = 2 * blk_size;

	m0_ivec_cursor_init(&cursor, &ivec);
	rc = pargrp_iomap_populate(map, &ivec, &cursor, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ivec_cursor_index(&cursor) == 2 * blk_size);

	m0_indexvec_free(&ivec);
	map->pi_ivec.iv_vec.v_nr = 0;
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = 0;

	/* Test 2.B 2 segments, 2 blocks each (one seg will spans 2 group)*/
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 1);

	rc = m0_indexvec_alloc(&ivec, 2);
	M0_UT_ASSERT (rc == 0);
	ivec.iv_index[0] = 0;
	ivec.iv_vec.v_count[0] = 3 * blk_size;
	ivec.iv_index[1] = 4 * (blk_size);
	ivec.iv_vec.v_count[1] = 1 * blk_size;
	ioo->ioo_data.ov_vec.v_count[0] = 4 * blk_size;

	m0_ivec_cursor_init(&cursor, &ivec);
	rc = pargrp_iomap_populate(map, &ivec, &cursor, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ivec_cursor_index(&cursor) == 2 * blk_size);

	m0_indexvec_free(&ivec);
	map->pi_ivec.iv_vec.v_nr = 0;
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = 0;

	/* Test 3. RMW */
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_WRITE;
	ioo->ioo_pbuf_type = M0_CLOVIS_PBUF_DIR;
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);
	ut_clovis_dummy_paritybufs_create(map, false);

	rc = m0_indexvec_alloc(&ivec, 1);
	M0_UT_ASSERT (rc == 0);
	ivec.iv_index[0] = 0;
	ivec.iv_vec.v_count[0] = blk_size;

	m0_ivec_cursor_init(&cursor, &ivec);
	rc = pargrp_iomap_populate(map, &ivec, &cursor, NULL);
	M0_UT_ASSERT(rc == 0);

	m0_indexvec_free(&ivec);
	ut_clovis_dummy_paritybufs_delete(map, true);

	/* Clean up */
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_pargrp_iomap_fullpages_count(void)
{
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/* Check on a valid map */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	M0_UT_ASSERT(pargrp_iomap_fullpages_count(map) == 0);

	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_pargrp_iomap_seg_process(void)
{
	int                     rc;
	int                     blk_size;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	blk_size = 1UL << UT_DEFAULT_BLOCK_SHIFT;

	/* Note: rmw is out of realm at current test */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	map->pi_ops = &iomap_ops;
	map->pi_grpid = 0;

	/*
	 * A few simple test cases
	 * 1. one block
	 * 2. multiple blocks
	 *
	 * No need to test the case where a seg spans multiple groups
	 * as seg_process assumes 'map' is not larger than a group.
	 */

	/* Test 1. One block */
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = blk_size;

	rc = pargrp_iomap_seg_process(map, 0, 0, 0, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(map->pi_databufs != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][0] != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][0]->db_flags != 0);

	/* Test 2. Multiple blocks */
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 1);
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = 2 * blk_size;
	ioo->ioo_ext.iv_index[0] = 0;
	ioo->ioo_ext.iv_vec.v_count[0] = 2 * blk_size;
	ioo->ioo_data.ov_vec.v_count[0] = 2 * blk_size;

	rc = pargrp_iomap_seg_process(map, 0, 0, 0, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(map->pi_databufs != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][0] != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][0]->db_flags != 0);

	/*
	 * Test 3. RMW.
	 * Note: reset ioo->ioo_ext & ioo_data to original settings.
	 */
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = blk_size;
	ioo->ioo_ext.iv_index[0] = 0;
	ioo->ioo_ext.iv_vec.v_count[0] = 1 * blk_size;
	ioo->ioo_data.ov_vec.v_count[0] = 1 * blk_size;

	rc = pargrp_iomap_seg_process(map, 0, 1, 0, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(map->pi_databufs != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][0] != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][0]->db_flags != 0);

	/* Clean up */
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests pargrp_iomap_databuf_alloc().
 */
static void ut_clovis_test_pargrp_iomap_databuf_alloc(void)
{
	int                     rc;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	M0_ALLOC_PTR(map);
	M0_UT_ASSERT(map != NULL);
	pargrp_iomap_bob_init(map);
	map->pi_ops = (struct pargrp_iomap_ops *)DUMMY_PTR;
	map->pi_ioo = ioo;
	map->pi_max_col = 4;
	map->pi_max_row = 1;

	rc = m0_indexvec_alloc(&map->pi_ivec, 1);
	M0_UT_ASSERT(rc == 0);

	M0_ALLOC_ARR(map->pi_databufs, 1);
	M0_UT_ASSERT(map->pi_databufs != NULL);
	M0_ALLOC_ARR(map->pi_databufs[0], 4);
	M0_UT_ASSERT(map->pi_databufs[0] != NULL);

	/* pi_databufs[row][col] != NULL */
	rc = pargrp_iomap_databuf_alloc(map, 0, 0, NULL);
	M0_UT_ASSERT(rc == 0);
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);

	m0_free(map->pi_databufs[0]);
	m0_free(map->pi_databufs);
	m0_indexvec_free(&map->pi_ivec);
	m0_free(map);

	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_pargrp_iomap_auxbuf_alloc(void)
{
	int                     rc;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	map->pi_ops = &iomap_ops;
	map->pi_grpid = 0;
	map->pi_rtype = PIR_READOLD;

	/* Base case */
	rc = pargrp_iomap_auxbuf_alloc(map, 0, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(map->pi_databufs[0][0]->db_auxbuf.b_addr != NULL);
	m0_free_aligned(map->pi_databufs[0][0]->db_auxbuf.b_addr,
			UT_DEFAULT_BLOCK_SIZE,
			UT_DEFAULT_BLOCK_SHIFT);

	/* Clean up */
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_pargrp_iomap_readold_auxbuf_alloc(void)
{
	int                     rc;
	int                     blk_size;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	blk_size = 1UL << UT_DEFAULT_BLOCK_SHIFT;

	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	map->pi_ops = &iomap_ops;
	map->pi_grpid = 0;
	map->pi_rtype = PIR_READOLD;

	/* Base case */
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = blk_size;
	rc = pargrp_iomap_readold_auxbuf_alloc(map);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT((map->pi_databufs[0][0]->db_flags & PA_READ) == PA_READ);
	m0_free_aligned(map->pi_databufs[0][0]->db_auxbuf.b_addr,
			UT_DEFAULT_BLOCK_SIZE,
			UT_DEFAULT_BLOCK_SHIFT);

	/* Clean up */
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_pargrp_iomap_readrest(void)
{
	int                     rc;
	int                     blk_size;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	blk_size = 1UL << UT_DEFAULT_BLOCK_SHIFT;

	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	map->pi_ops = &iomap_ops;
	map->pi_grpid = 0;
	map->pi_rtype = PIR_READREST;

	/* Test 1. One segment (one block)*/
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = blk_size;
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);

	rc = pargrp_iomap_readrest(map);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(map->pi_databufs != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][0] != NULL);
	M0_UT_ASSERT((map->pi_databufs[0][0]->db_flags & PA_READ) == PA_READ);

	/* Test 2. 1 segment, 2 blocks*/
	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = 2 * blk_size;
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 0);
	ut_clovis_pargrp_iomap_free_data_buf(map, 0, 1);

	rc = pargrp_iomap_readrest(map);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(map->pi_databufs != NULL);
	M0_UT_ASSERT(map->pi_databufs[0][1] != NULL);
	M0_UT_ASSERT((map->pi_databufs[0][1]->db_flags & PA_READ) == PA_READ);

	/* Clean up */
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_pargrp_iomap_parity_recalc(void)
{
	int                     rc;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	map->pi_ops = &iomap_ops;
	map->pi_grpid = 0;

	/*
	 * Base cases: as we assume the m0_parity_math_xxx do the proper
	 * job, we don't check the content of calculated parity buffers.
	 *
	 * Set parity bufs first.
	 */
	ut_clovis_dummy_paritybufs_create(map, true);

	/*
 	 * Test 1. map->pi_rtype == PIR_NONE (normal case)
 	 */
	map->pi_ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_WRITE;
	map->pi_rtype = PIR_READREST;
	rc = pargrp_iomap_parity_recalc(map);
	M0_UT_ASSERT(rc == 0);

	/* Test 2. Read rest method */
	map->pi_rtype = PIR_READREST;
	rc = pargrp_iomap_parity_recalc(map);
	M0_UT_ASSERT(rc == 0);

	/* Test 3. Read old method */
	map->pi_rtype = PIR_READOLD;
	rc = pargrp_iomap_parity_recalc(map);
	M0_UT_ASSERT(rc == 0);

	/* free parity bufs*/
	ut_clovis_dummy_paritybufs_delete(map, true);

	/* Clean up */
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_pargrp_iomap_paritybufs_alloc(void)
{
	int                     i;
	int                     j;
	int                     rc;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	/* Base cases */
	instance = dummy_instance;
	realm.re_instance = instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ioo->ioo_iomaps[0];
	map->pi_ioo = ioo;
	map->pi_rtype = PIR_READOLD;
	ut_clovis_dummy_paritybufs_create(map, false);

	/* Check if this function behaves ok */
	rc = pargrp_iomap_paritybufs_alloc(map);
	M0_UT_ASSERT(rc == 0 || rc == -ENOMEM);

	/*
	 * The settings in ioo->ioo_obj(created by layout_domain_fill)
	 * for layout don't match those in map, disable the following
	 * checks at this moment
	 */
	if (rc == 0) {
		for (i = 0; i < map->pi_max_row; i++) {
			for (j = 0; j < CLOVIS_M0T1FS_LAYOUT_K; j++) {
				M0_UT_ASSERT(map->pi_paritybufs[i][j] != NULL);
				M0_UT_ASSERT((map->pi_paritybufs[i][j]->db_flags
					     & PA_READ) == PA_READ);
			}
		}
	}

	ut_clovis_dummy_paritybufs_delete(map, true);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests pargrp_iomap_spans_seg().
 */
static void ut_clovis_test_pargrp_iomap_spans_seg(void)
{
	int                     n;
	bool                    is_spanned;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm  realm;

	/* Base cases */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/*
	 * Create a dummy map, it covers the first parity group.
	 */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	n = map->pi_max_col;

	map->pi_ivec.iv_index[0] = 0;
	map->pi_ivec.iv_vec.v_count[0] = 4096;

	/* 1. one unit */
	is_spanned = pargrp_iomap_spans_seg(map, 0,
					    1 << CLOVIS_MIN_BUF_SHIFT);
	M0_UT_ASSERT(is_spanned == true);

	/* 2. n- 1 units (n > 2) */
	is_spanned = pargrp_iomap_spans_seg(map, 0,
					    (n - 1) *
					    (1 << CLOVIS_MIN_BUF_SHIFT));
	M0_UT_ASSERT(is_spanned == true);

	/* 3. out of the boundary of this iomap */
	is_spanned = pargrp_iomap_spans_seg(map, 4096,
					    1 << CLOVIS_MIN_BUF_SHIFT);
	M0_UT_ASSERT(is_spanned == false);

	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_free_pargrp_iomap(struct pargrp_iomap *map)
{
	struct m0_pdclust_layout *play;
	int                       row;
	int                       col;

	play = pdlayout_get(map->pi_ioo);
	m0_free(map->pi_ivec.iv_index);
	m0_free(map->pi_ivec.iv_vec.v_count);

	if (map->pi_databufs != NULL) {
		for (row = 0; row < map->pi_max_row; ++row) {
			for (col = 0; col < map->pi_max_col; ++col) {
				if (map->pi_databufs[row][col] != NULL)
					m0_free(map->pi_databufs[row][col]);
			}
			m0_free(map->pi_databufs[row]);
		}
	}

	if (map->pi_paritybufs != NULL) {
		for (row = 0; row < map->pi_max_row; ++row) {
			for (col = 0; col < parity_col_nr(play); ++col) {
				if (map->pi_paritybufs[row][col] != NULL)
					m0_free(map->pi_paritybufs[row][col]);
			}
			m0_free(map->pi_paritybufs[row]);
		}
	}

	m0_free(map->pi_databufs);
	m0_free(map->pi_paritybufs);
}

/**
 * Tests pargrp_iomap_init().
 */
static void ut_clovis_test_pargrp_iomap_init(void)
{
	int                     i;
	int                     rc;
	struct m0_clovis       *instance = NULL;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis_realm  realm;

	/* Base case */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);

	ioo->ioo_obj->ob_entity.en_realm = &realm;
	ioo->ioo_pbuf_type = M0_CLOVIS_PBUF_DIR;
	realm.re_instance = instance;

	M0_ALLOC_PTR(map);
	M0_UT_ASSERT(map != NULL);

	rc = pargrp_iomap_init(map, ioo, 1);
	M0_UT_ASSERT(rc == 0 || rc == -ENOMEM);
	if (rc == 0) {
		M0_UT_ASSERT(map->pi_databufs != NULL);
		for (i = 0; i < map->pi_max_row; i++) {
			M0_UT_ASSERT(map->pi_databufs[i] != NULL);
		}

		ut_clovis_free_pargrp_iomap(map);
	}
	m0_free(map);

	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests pargrp_iomap_fini().
 */
static void ut_clovis_test_pargrp_iomap_fini(void)
{
	struct pargrp_iomap    *map;
	struct m0_clovis_obj   *obj;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_realm   realm;

	/* Init. */
	instance = dummy_instance;

	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	obj = ut_clovis_dummy_obj_create();
	M0_UT_ASSERT(obj != NULL);

	/* Don't use the map in ioo beacuse pargrp_iomap_fini will free it*/
	//map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	M0_ALLOC_PTR(map);
	M0_UT_ASSERT(map != NULL);
	pargrp_iomap_bob_init(map);
	map->pi_ops = (struct pargrp_iomap_ops *)DUMMY_PTR;
	map->pi_ioo = (struct m0_clovis_op_io *)DUMMY_PTR;

	map->pi_ioo = ioo;
	map->pi_max_col = 4;
	map->pi_max_row = 1;
	map->pi_ivec.iv_vec.v_nr = 1;
	M0_ALLOC_ARR(map->pi_ivec.iv_index, 1);
	M0_UT_ASSERT(map->pi_ivec.iv_index != NULL);
	M0_ALLOC_ARR(map->pi_ivec.iv_vec.v_count, 1);
	M0_UT_ASSERT(map->pi_ivec.iv_vec.v_count != NULL);

	/* n = 4, p = 1, k = 1, blk_size=512, blk_shift=9*/
	M0_ALLOC_ARR(map->pi_databufs, 1);
	M0_UT_ASSERT(map->pi_databufs != NULL);
	M0_ALLOC_ARR(map->pi_databufs[0], 4);
	M0_UT_ASSERT(map->pi_databufs[0] != NULL);

	pargrp_iomap_fini(map, obj);
	M0_UT_ASSERT(map->pi_state == PI_NONE);
	M0_UT_ASSERT(map->pi_databufs == NULL);

	m0_free(map);
	ut_clovis_dummy_obj_delete(obj);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/* Hack to reproduce some functionality from layout/layout.c */
M0_BOB_DECLARE(M0_INTERNAL, m0_layout_enum);

static struct m0_fid mock_fid;
static void mock_layout_enum_get(const struct m0_layout_enum *e, uint32_t idx,
				 const struct m0_fid *gfid, struct m0_fid *out)
{
	m0_fid_gob_make(&mock_fid, 0, 1);
	m0_fid_convert_gob2cob(&mock_fid, out, 0);
}

static struct m0_layout_enum_ops mock_layout_enum_ops = {
	.leo_get = &mock_layout_enum_get,
};

static struct m0_layout_enum_type mock_layout_enum_type;
static struct m0_layout_enum mock_layout_enum;

static struct m0_layout_enum *
mock_layout_instance_to_enum(const struct m0_layout_instance *li)
{
	m0_layout_enum_bob_init(&mock_layout_enum);
	mock_layout_enum.le_type = &mock_layout_enum_type;
	mock_layout_enum.le_ops = &mock_layout_enum_ops;
	return &mock_layout_enum;
}

static struct m0_layout_instance_ops mock_layout_instance_ops = {
	.lio_to_enum = mock_layout_instance_to_enum,
};

M0_INTERNAL void ut_clovis_set_device_state(struct m0_poolmach *pm, int dev,
				       enum m0_pool_nd_state state)
{
	pm->pm_state->pst_devices_array[dev].pd_state = state;
}

static void ut_clovis_test_pargrp_src_addr(void)
{
}

static void ut_clovis_test_pargrp_id_find(void)
{
}

static void ut_clovis_test_gobj_offset(void)
{
	struct pargrp_iomap       *map;
	struct m0_clovis          *instance = NULL;
	m0_bindex_t                ret;
	struct m0_pdclust_src_addr saddr;
	struct m0_pdclust_layout  *pl;

	/* init */
	instance = dummy_instance;
	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);
	map->pi_paritybufs = NULL;
	pl = ut_get_pdclust_layout_from_ioo(map->pi_ioo);
	saddr.sa_unit = 0;

	/* Base cases. */
	ret = gobj_offset(0, map, pl, &saddr);
	M0_UT_ASSERT(ret == 0);

	ret = gobj_offset(UT_DEFAULT_BLOCK_SIZE + 32, map, pl, &saddr);
	M0_UT_ASSERT(ret == 32);

	/* fini */
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	map->pi_ioo = NULL;
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

static void ut_clovis_test_is_page_read(void)
{
	struct data_buf     *db;
	struct target_ioreq *ti;
	bool		    yes;

	db = ut_clovis_dummy_data_buf_create();
	db->db_flags |= PA_READ;

	ti = ut_clovis_dummy_target_ioreq_create();
	M0_UT_ASSERT(ti != NULL);
	ti->ti_rc = 0;
	db->db_tioreq = ti;

	/* true case*/
	yes = is_page_read(db);
	M0_UT_ASSERT(yes == true);

	/* false cases*/
	ti->ti_rc = -1;
	yes = is_page_read(db);
	M0_UT_ASSERT(yes == false);

	db->db_tioreq = NULL;
	yes = is_page_read(db);
	M0_UT_ASSERT(yes == false);

	db->db_flags = 0;
	yes = is_page_read(db);
	M0_UT_ASSERT(yes == false);

	ut_clovis_dummy_target_ioreq_delete(ti);
	ut_clovis_dummy_data_buf_delete(db);
}

static void ut_clovis_test_data_page_offset_get(void)
{
	struct pargrp_iomap *map;
	struct m0_clovis    *instance = NULL;

	/* init */
	instance = dummy_instance;
	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);
	map->pi_paritybufs = NULL;

	/* Base cases. */
	data_page_offset_get(map, 0, 0);

	/* fini */
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	map->pi_ioo = NULL;
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

static void ut_clovis_test_pargrp_iomap_pages_mark_as_failed(void)
{
	int                 rc;
	struct pargrp_iomap *map;
	struct m0_clovis    *instance = NULL;

	/* init */
	instance = dummy_instance;
	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);

	/* Base cases. */
	rc = pargrp_iomap_pages_mark_as_failed(map, M0_PUT_DATA);
	M0_UT_ASSERT(rc == 0);

	ut_clovis_dummy_paritybufs_create(map, false);
	rc = pargrp_iomap_pages_mark_as_failed(map, M0_PUT_PARITY);
	M0_UT_ASSERT(rc == 0);
	ut_clovis_dummy_paritybufs_delete(map, false);

	/* fini */
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	map->pi_ioo = NULL;
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

static void ut_clovis_test_unit_state(void)
{
}

/**
 * No tests for the following functions by now as don't know how to set sns
 * repair related parameters and unit_state
 */
static void ut_clovis_test_io_spare_map(void)
{
}

static void ut_clovis_test_mark_page_as_read_failed(void)
{
}

static void ut_clovis_test_pargrp_iomap_dgmode_process(void)
{
}

static void ut_clovis_test_pargrp_iomap_dgmode_postprocess(void)
{
	struct pargrp_iomap   *map;
	struct m0_clovis      *instance = NULL;
	struct m0_clovis_realm realm;

	/* init */
	instance = dummy_instance;

	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ut_clovis_realm_entity_setup(&realm,
		map->pi_ioo->ioo_oo.oo_oc.oc_op.op_entity, instance);
	map->pi_ioo->ioo_oo.oo_layout_instance->li_ops =
					&mock_layout_instance_ops;

	/* Base cases. */
	ut_clovis_set_device_state(
		&instance->m0c_pools_common.pc_cur_pver->pv_mach,
		0, M0_PNDS_FAILED);
	pargrp_iomap_dgmode_postprocess(map);

	/* fini */
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	map->pi_ioo = NULL;
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

static void ut_clovis_test_pargrp_iomap_dgmode_recover(void)
{
	int                    i;
	int                    rc;
	struct pargrp_iomap   *map;
	struct m0_clovis      *instance = NULL;
	struct m0_clovis_realm realm;
	struct m0_parity_math *math;

	/* init */
	instance = dummy_instance;

	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ut_clovis_realm_entity_setup(&realm,
		map->pi_ioo->ioo_oo.oo_oc.oc_op.op_entity, instance);
	map->pi_ioo->ioo_oo.oo_layout_instance->li_ops =
					&mock_layout_instance_ops;

	math = parity_math(map->pi_ioo);
	math->pmi_data_count = CLOVIS_M0T1FS_LAYOUT_N;
	math->pmi_parity_count = CLOVIS_M0T1FS_LAYOUT_K;

	/* Base cases. */
	ut_clovis_set_device_state(
		&instance->m0c_pools_common.pc_cur_pver->pv_mach,
		0, M0_PNDS_FAILED);

	map->pi_state = PI_DEGRADED;
	ut_clovis_dummy_paritybufs_create(map, true);

	for (i = 0; i < CLOVIS_M0T1FS_LAYOUT_N; i++)
		map->pi_databufs[0][i]->db_flags = 0;
	for (i = 0; i < CLOVIS_M0T1FS_LAYOUT_K; i++)
		map->pi_paritybufs[0][i]->db_flags = 0;
	map->pi_paritybufs[0][0]->db_flags = PA_READ_FAILED;


	rc = pargrp_iomap_dgmode_recover(map);
	M0_UT_ASSERT(rc == 0);

	/* fini */
	ut_clovis_dummy_paritybufs_delete(map, true);
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	map->pi_ioo = NULL;
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

M0_INTERNAL int ut_clovis_io_pargrp_init(void)
{
	int                       rc;
	struct m0_pdclust_layout *dummy_pdclust_layout;

#ifndef __KERNEL__
	ut_clovis_shuffle_test_order(&ut_suite_clovis_io_pargrp);
#endif

	m0_clovis_init_io_op();

	rc = ut_m0_clovis_init(&dummy_instance);
	M0_UT_ASSERT(rc == 0);

	ut_clovis_layout_domain_fill(dummy_instance);
	dummy_pdclust_layout =
		ut_clovis_dummy_pdclust_layout_create(dummy_instance);
	M0_UT_ASSERT(dummy_pdclust_layout != NULL);

	ut_clovis_dummy_poolmach_create(
		dummy_instance->m0c_pools_common.pc_cur_pver);

	return 0;
}

M0_INTERNAL int ut_clovis_io_pargrp_fini(void)
{
	ut_clovis_dummy_poolmach_delete(
		dummy_instance->m0c_pools_common.pc_cur_pver);
	dummy_instance->m0c_pools_common.pc_cur_pver = NULL;
	ut_clovis_layout_domain_empty(dummy_instance);
	ut_m0_clovis_fini(&dummy_instance);

	return 0;
}

struct m0_ut_suite ut_suite_clovis_io_pargrp = {
	.ts_name = "clovis-io-pargrp-ut",
	.ts_init = ut_clovis_io_pargrp_init,
	.ts_fini = ut_clovis_io_pargrp_fini,
	.ts_tests = {
		{ "data_buf_invariant",
				    &ut_clovis_test_data_buf_invariant},
		{ "data_buf_invariant_nr",
				    &ut_clovis_test_data_buf_invariant_nr},
		{ "data_buf_init",
				    &ut_clovis_test_data_buf_init},
		{ "data_buf_fini",
				    &ut_clovis_test_data_buf_fini},
		{ "data_buf_dealloc_fini",
				    &ut_clovis_test_data_buf_dealloc_fini},
		{ "data_buf_alloc_init",
				    &ut_clovis_test_data_buf_alloc_init},
		{ "pargrp_iomap_invariant",
				    &ut_clovis_test_pargrp_iomap_invariant},
		{ "pargrp_iomap_invariant_nr",
				    &ut_clovis_test_pargrp_iomap_invariant_nr},
		{ "seg_collate",
				    &ut_clovis_test_seg_collate},
		{ "pargrp_iomap_populate",
				    &ut_clovis_test_pargrp_iomap_populate},
		{ "pargrp_iomap_fullpages_count",
				    &ut_clovis_test_pargrp_iomap_fullpages_count},
		{ "pargrp_iomap_seg_process",
				    &ut_clovis_test_pargrp_iomap_seg_process},
		{ "pargrp_iomap_databuf_alloc",
				    &ut_clovis_test_pargrp_iomap_databuf_alloc},
		{ "pargrp_iomap_auxbuf_alloc",
				    &ut_clovis_test_pargrp_iomap_auxbuf_alloc},
		{ "pargrp_iomap_readold_auxbuf_alloc",
				    &ut_clovis_test_pargrp_iomap_readold_auxbuf_alloc},
		{ "pargrp_iomap_readrest",
				    &ut_clovis_test_pargrp_iomap_readrest},
		{ "pargrp_iomap_parity_recalc",
				    &ut_clovis_test_pargrp_iomap_parity_recalc},
		{ "pargrp_iomap_paritybufs_alloc",
 				    &ut_clovis_test_pargrp_iomap_paritybufs_alloc},
		{ "pargrp_iomap_spans_seg",
				    &ut_clovis_test_pargrp_iomap_spans_seg},
		{ "pargrp_iomap_init",
				    &ut_clovis_test_pargrp_iomap_init},
		{ "pargrp_iomap_fini",
				    &ut_clovis_test_pargrp_iomap_fini},
		{ "pargrp_src_addr",
				    &ut_clovis_test_pargrp_src_addr},
		{ "pargrp_id_find",
				    &ut_clovis_test_pargrp_id_find},
		{ "gobj_offset",
				    &ut_clovis_test_gobj_offset},
		{ "is_page_read",
				    &ut_clovis_test_is_page_read},
		{ "data_page_offset_get",
				    &ut_clovis_test_data_page_offset_get},
		{ "pargrp_iomap_pages_mark_as_failed",
				    &ut_clovis_test_pargrp_iomap_pages_mark_as_failed},
		{ "unit_state",
				    &ut_clovis_test_unit_state},
		{ "io_spare_map",
				    &ut_clovis_test_io_spare_map},
		{ "mark_page_as_read_failed",
				    &ut_clovis_test_mark_page_as_read_failed},
		{ "pargrp_iomap_dgmode_process",
				    &ut_clovis_test_pargrp_iomap_dgmode_process},
		{ "pargrp_iomap_dgmode_postprocess",
				    &ut_clovis_test_pargrp_iomap_dgmode_postprocess},
		{ "pargrp_iomap_dgmode_recover",
				    &ut_clovis_test_pargrp_iomap_dgmode_recover},
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
