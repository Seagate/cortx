/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Authors: Sining Wu       <sining.wu@seagate.com>
 *	    Pratik Shinde   <pratik.shinde@seagate.com>
 *	    Vishwas Bhat    <vishwas.bhat@seagate.com>
 * Original creation date: 05-May-2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#include "clovis/clovis.h"
#include "clovis/clovis_layout.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"

#include "lib/memory.h"
#include "lib/vec.h"
#include "lib/types.h"
#include "lib/errno.h" /* ENOENT */

#ifndef __KERNEL__
#  include <stdlib.h>
#  include <unistd.h>
#else
#  include <linux/delay.h>
#endif

/*
 * Fill those pre-created objects with some value
 */
enum { CHAR_NUM = 6 };
static char pattern[CHAR_NUM] = {'C', 'L', 'O', 'V', 'I', 'S'};

static struct m0_uint128 test_id;
struct m0_clovis_container clovis_st_layout_container;
static uint64_t layout_id;
static uint32_t unit_size = DEFAULT_PARGRP_UNIT_SIZE;

/**
 * Creates an object.
 */
static int create_obj(struct m0_uint128 id)
{
	struct m0_clovis_op  *ops[] = {NULL};
	struct m0_clovis_obj *obj;
	int                   rc;

	MEM_ALLOC_PTR(obj);
	if (obj == NULL)
		return -ENOMEM;

	clovis_st_obj_init(obj, &clovis_st_layout_container.co_realm,
			   &id, layout_id);

	rc = clovis_st_entity_create(NULL, &obj->ob_entity, &ops[0]);
	rc = rc?:ops[0]->op_sm.sm_rc;
	if (rc != 0) {
		mem_free(obj);
		return rc;
	}

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&obj->ob_entity);
	mem_free(obj);

	return rc;
}

static int write_obj(struct m0_uint128 id)
{
	int                  i;
	int                  rc;
	int                  blk_cnt;
	int                  blk_size;
	char                 value;
	uint64_t             last_index;
	struct m0_clovis_obj obj;
	struct m0_clovis_op *ops[1] = {NULL};
	struct m0_indexvec   ext;
	struct m0_bufvec     data;
	struct m0_bufvec     attr;

	M0_CLOVIS_THREAD_ENTER;

	/* Prepare the data with 'value' */
	blk_cnt = 4;
	blk_size = unit_size;

	rc = m0_bufvec_alloc(&data, blk_cnt, blk_size);
	if (rc != 0)
		return rc;

	for (i = 0; i < blk_cnt; i++) {
		/*
		 * The pattern written to object has to match
		 * to those in read tests
		 */
		value = pattern[i % CHAR_NUM];
		memset(data.ov_buf[i], value, blk_size);
	}

	/* Prepare indexvec for write */
	rc = m0_bufvec_alloc(&attr, blk_cnt, 1);
	if(rc != 0) return rc;

	rc = m0_indexvec_alloc(&ext, blk_cnt);
	if (rc != 0) return rc;

	last_index = 0;
	for (i = 0; i < blk_cnt; i++) {
		ext.iv_index[i] = last_index ;
		ext.iv_vec.v_count[i] = blk_size;
		last_index += blk_size;

		/* we don't want any attributes */
		attr.ov_vec.v_count[i] = 0;
	}

	/* Write to object */
	memset(&obj, 0, sizeof obj);
	ops[0] = NULL;

	clovis_st_obj_init(
		&obj, &clovis_st_layout_container.co_realm,
		&id, layout_id);
	clovis_st_entity_open(&obj.ob_entity);
	clovis_st_obj_op(&obj, M0_CLOVIS_OC_WRITE,
			 &ext, &data, &attr, 0, &ops[0]);
	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);

	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&obj.ob_entity);

	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
	m0_indexvec_free(&ext);
	return rc;
}

/**
 * Test m0_clovis_layout_op() on `normal` object.
 */
static void layout_op_get_obj(void)
{
	int                      rc = 0;
	struct m0_uint128        id;
	struct m0_clovis_op     *ops[] = {NULL};
	struct m0_clovis_obj    *obj;
	struct m0_clovis_layout *layout;

	/* Create an object with default layout type (PDCLUST). */
	clovis_oid_get(&id);
	create_obj(id);

	/* Sample code to retrieve layout of the created object. */
	M0_ALLOC_PTR(obj);
	CLOVIS_ST_ASSERT_FATAL(obj != NULL);

	/* 1. Initialise in-memory object data struct. */
	clovis_st_obj_init(obj, &clovis_st_layout_container.co_realm,
			   &id, layout_id);

	/* 2. Open the object to get its attributes. */
	clovis_st_entity_open(&obj->ob_entity);
	CLOVIS_ST_ASSERT_FATAL(m0_clovis_obj_layout_type(obj) ==
			       M0_CLOVIS_LT_PDCLUST);

	/* 3. Issue LAYOUT_GET op. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_PDCLUST);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	clovis_st_layout_op(obj, M0_CLOVIS_EO_LAYOUT_GET, layout, &ops[0]);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0] != NULL);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&obj->ob_entity);
	m0_clovis_layout_free(layout);
	m0_free(obj);
}

static int layout_create_capture_obj(struct m0_clovis_layout *layout,
				     struct m0_uint128 *ret_id)
{
	int                   rc = 0;
	struct m0_uint128     id;
	struct m0_clovis_op  *ops[] = {NULL};
	struct m0_clovis_obj *obj = NULL;

	clovis_oid_get(&id);
	create_obj(id);
	*ret_id = id;

	M0_ALLOC_PTR(obj);
	if (obj == NULL)
		return -ENOMEM;

	clovis_st_obj_init(obj, &clovis_st_layout_container.co_realm,
			   &id, layout_id);
	clovis_st_entity_open(&obj->ob_entity);
	clovis_st_layout_op(obj, M0_CLOVIS_EO_LAYOUT_SET,
			    layout, &ops[0]);
	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&obj->ob_entity);
	m0_free(obj);

	return rc;
}

/**
 * Test m0_clovis_layout_capture() and create an object with capture
 * layout.
 */
static void layout_capture(void)
{
	int                      rc = 0;
	struct m0_uint128        orig_id;
	struct m0_uint128        id;
	struct m0_clovis_op     *ops[] = {NULL};
	struct m0_clovis_obj    *orig_obj;
	struct m0_clovis_layout *orig_layout;
	struct m0_clovis_layout *layout;

	/* Create an object with default layout type (PDCLUST). */
	clovis_oid_get(&orig_id);
	create_obj(orig_id);

	/*
	 * Sample code to capture layout and set an object with
 	 * captured layout.
 	 */
	M0_ALLOC_PTR(orig_obj);
	CLOVIS_ST_ASSERT_FATAL(orig_obj != NULL);

	/* 1. Initialise in-memory object data struct. */
	clovis_st_obj_init(orig_obj, &clovis_st_layout_container.co_realm,
			   &orig_id, layout_id);

	/* 2. Open the object to get its attributes. */
	clovis_st_entity_open(&orig_obj->ob_entity);
	CLOVIS_ST_ASSERT_FATAL(m0_clovis_obj_layout_type(orig_obj) ==
			       M0_CLOVIS_LT_PDCLUST);

	/* 3. Issue LAYOUT_GET op. */
	orig_layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_PDCLUST);
	CLOVIS_ST_ASSERT_FATAL(orig_layout != NULL);

	clovis_st_layout_op(orig_obj, M0_CLOVIS_EO_LAYOUT_GET,
			    orig_layout, &ops[0]);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0] != NULL);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	ops[0] = NULL;

	/* 4. Capture layout. */
	rc = m0_clovis_layout_capture(orig_layout, orig_obj, &layout);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	/* 5. Create a new object with the captured layout.*/
	rc = layout_create_capture_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Fini and free entities and layouts. */
	m0_clovis_layout_free(layout);
	clovis_st_entity_fini(&orig_obj->ob_entity);
	m0_clovis_layout_free(orig_layout);
	m0_free(orig_obj);
}

/**
 * Test IO on an object with capture layout.
 */
static void layout_capture_io(void)
{
	int                      rc = 0;
	struct m0_uint128        orig_id;
	struct m0_uint128        id;
	struct m0_clovis_op     *ops[] = {NULL};
	struct m0_clovis_obj    *orig_obj;
	struct m0_clovis_layout *orig_layout;
	struct m0_clovis_layout *layout;

	/* Create an object with default layout type (PDCLUST). */
	clovis_oid_get(&orig_id);
	create_obj(orig_id);

	/*
	 * Sample code to capture layout and set an object with
 	 * captured layout.
 	 */
	M0_ALLOC_PTR(orig_obj);
	CLOVIS_ST_ASSERT_FATAL(orig_obj != NULL);

	/* 1. Initialise in-memory object data struct. */
	clovis_st_obj_init(orig_obj, &clovis_st_layout_container.co_realm,
			   &orig_id, layout_id);

	/* 2. Open the object to get its attributes. */
	clovis_st_entity_open(&orig_obj->ob_entity);
	CLOVIS_ST_ASSERT_FATAL(m0_clovis_obj_layout_type(orig_obj) ==
			       M0_CLOVIS_LT_PDCLUST);

	/* 3. Issue LAYOUT_GET op. */
	orig_layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_PDCLUST);
	CLOVIS_ST_ASSERT_FATAL(orig_layout != NULL);

	clovis_st_layout_op(orig_obj, M0_CLOVIS_EO_LAYOUT_GET,
			    orig_layout, &ops[0]);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0] != NULL);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	ops[0] = NULL;

	/* 4. Capture layout. */
	rc = m0_clovis_layout_capture(orig_layout, orig_obj, &layout);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	/* 5. Create a new object with the captured layout.*/
	rc = layout_create_capture_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* 6. Write to the newly created object above.*/
	rc = write_obj(id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Fini and free entities and layouts. */
	m0_clovis_layout_free(layout);
	clovis_st_entity_fini(&orig_obj->ob_entity);
	m0_clovis_layout_free(orig_layout);
	m0_free(orig_obj);
}

static int layout_composite_create_obj(struct m0_clovis_layout *layout,
				       struct m0_uint128 *ret_id)
{
	int                   rc = 0;
	struct m0_uint128     id;
	struct m0_clovis_op  *ops[] = {NULL};
	struct m0_clovis_obj *obj = NULL;

	clovis_oid_get(&id);
	create_obj(id);
	*ret_id = id;

	M0_ALLOC_PTR(obj);
	if (obj == NULL)
		return -ENOMEM;

	clovis_st_obj_init(obj, &clovis_st_layout_container.co_realm,
			   &id, layout_id);
	clovis_st_entity_open(&obj->ob_entity);
	clovis_st_layout_op(obj, M0_CLOVIS_EO_LAYOUT_SET,
			    layout, &ops[0]);

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);

	clovis_st_entity_fini(&obj->ob_entity);
	m0_free(obj);

	return rc;
}

static int layout_composite_add_layers(struct m0_clovis_layout *layout,
				       int nr_layers,
				       struct m0_uint128 *layer_ids)
{
	int                  i;
	int                  j;
	int                  rc = 0;
	struct m0_uint128    id;
	struct m0_clovis_obj obj;

	for (i = 0; i < nr_layers; i++) {
		/* Create a sub-object. */
		clovis_oid_get(&id);
		rc = create_obj(id);
		if (rc != 0)
			break;

		M0_SET0(&obj);
		clovis_st_obj_init(&obj, &clovis_st_layout_container.co_realm,
				   &id, layout_id);
		rc = m0_clovis_composite_layer_add(layout, &obj, i);
		clovis_st_entity_fini(&obj.ob_entity);
		if (rc != 0)
			break;
		layer_ids[i] = id;
	}

	if (rc != 0) {
		for (j = 0; j < i; j++)
			m0_clovis_composite_layer_del(layout, layer_ids[j]);
	}

	return rc;
}

/**
 * Test creating a new object with composite layout.
 */
static void layout_composite_create(void)
{
	int                      rc = 0;
	int                      nr_layers;
	struct m0_uint128        id;
	struct m0_uint128       *layer_ids;
	struct m0_clovis_layout *layout;

	/* Create a composite layout with layers. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	nr_layers = 3;
	M0_ALLOC_ARR(layer_ids, nr_layers);
	CLOVIS_ST_ASSERT_FATAL(layer_ids != NULL);
	rc = layout_composite_add_layers(layout, nr_layers, layer_ids);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Create a new object with the captured layout.*/
	rc = layout_composite_create_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	m0_clovis_layout_free(layout);
}

/**
 * Test creating a new object with composite layout then issuing LAYOUT_GET
 * op to verify.
 */
static void layout_composite_create_then_get(void)
{
	int                                rc = 0;
	int                                nr_layers;
	struct m0_uint128                  id;
	struct m0_uint128                 *layer_ids;
	struct m0_clovis_op               *ops[] = {NULL};
	struct m0_clovis_obj               obj;
	struct m0_clovis_layout           *layout;
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_layout           *layout_to_check;
	struct m0_clovis_composite_layout *clayout_to_check;

	/* Create a composite layout with layers. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	nr_layers = 1;
	M0_ALLOC_ARR(layer_ids, nr_layers);
	CLOVIS_ST_ASSERT_FATAL(layer_ids != NULL);
	rc = layout_composite_add_layers(layout, nr_layers, layer_ids);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Create a new object with the captured layout.*/
	rc = layout_composite_create_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);


	/* Prepare and issue LAYOUT_GET op. */
	M0_SET0(&obj);
	clovis_st_obj_init(&obj, &clovis_st_layout_container.co_realm,
			   &id, layout_id);
	clovis_st_entity_open(&obj.ob_entity);
	CLOVIS_ST_ASSERT_FATAL(m0_clovis_obj_layout_type(&obj) ==
			       M0_CLOVIS_LT_COMPOSITE);

	layout_to_check = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout_to_check != NULL);
	clovis_st_layout_op(&obj, M0_CLOVIS_EO_LAYOUT_GET,
			    layout_to_check, &ops[0]);
	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_rc == 0);

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&obj.ob_entity);

	/* Check returned layout. */
	clayout = M0_AMB(clayout, layout, ccl_layout);
	clayout_to_check =
		M0_AMB(clayout_to_check, layout_to_check, ccl_layout);
	CLOVIS_ST_ASSERT_FATAL(
		clayout->ccl_nr_layers == clayout->ccl_nr_layers);

	m0_clovis_layout_free(layout);
	m0_clovis_layout_free(layout_to_check);
}

/*---------------------------------------------------------------------------*
 *                             Composite IO                                  *
 *---------------------------------------------------------------------------*/

struct composite_extent {
	struct m0_uint128 ce_id;
	m0_bindex_t       ce_off;
	m0_bcount_t       ce_len;
};

struct io_seg {
	m0_bindex_t is_off;
	m0_bcount_t is_len;
};

static int write_io_segs(struct m0_uint128 id, int nr_io_segs,
			 struct io_seg *io_segs)
{
	int                  i;
	int                  rc;
	char                 value;
	struct m0_clovis_obj obj;
	struct m0_clovis_op *ops[1] = {NULL};
	struct m0_indexvec   ext;
	struct m0_bufvec     data;
	struct m0_bufvec     attr;

	/* Prepare the data with 'value' */
	rc = m0_bufvec_empty_alloc(&data, nr_io_segs);
	if (rc != 0)
		return rc;

	for (i = 0; i < nr_io_segs; i++) {
		data.ov_vec.v_count[i] = io_segs[i].is_len;
		data.ov_buf[i] = m0_alloc(io_segs[i].is_len);
		if (data.ov_buf[i] == NULL)
			goto free;

		/*
		 * The pattern written to object has to match
		 * to those in read tests
		 */
		value = pattern[i % CHAR_NUM];
		memset(data.ov_buf[i], value, io_segs[i].is_len);
	}

	/* Prepare indexvec and attr for write */
	rc = m0_bufvec_alloc(&attr, nr_io_segs, 1);
	if(rc != 0)
		goto free;
	rc = m0_indexvec_alloc(&ext, nr_io_segs);
	if (rc != 0)
		goto free;
	for (i = 0; i < nr_io_segs; i++) {
		ext.iv_index[i] = io_segs[i].is_off;
		ext.iv_vec.v_count[i] = io_segs[i].is_len;

		/* we don't want any attributes */
		attr.ov_vec.v_count[i] = 0;
	}

	/* Write to object */
	memset(&obj, 0, sizeof obj);
	clovis_st_obj_init(
		&obj, &clovis_st_layout_container.co_realm,
		&id, layout_id);
	clovis_st_entity_open(&obj.ob_entity);
	clovis_st_obj_op(&obj, M0_CLOVIS_OC_WRITE,
			 &ext, &data, &attr, 0, &ops[0]);
	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);

	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&obj.ob_entity);

free:
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
	m0_indexvec_free(&ext);
	return rc;
}

static int fill_ext_kv_pairs(struct composite_extent *exts, int nr_exts,
			     struct m0_bufvec *keys, struct m0_bufvec *vals)
{
	int                                      i;
	int                                      rc = 0;
	int                                      nr_kvp;
	struct m0_clovis_composite_layer_idx_key key;
	struct m0_clovis_composite_layer_idx_val val;


	nr_kvp = keys->ov_vec.v_nr;
	for (i = 0; i < nr_kvp; i++) {
		/* Set key and value. */
		key.cek_layer_id = exts[i].ce_id;
		key.cek_off = exts[i].ce_off;
		val.cev_len = exts[i].ce_len;
		rc = m0_clovis_composite_layer_idx_key_to_buf(
			&key, &keys->ov_buf[i], &keys->ov_vec.v_count[i])?:
		     m0_clovis_composite_layer_idx_val_to_buf(
			&val, &vals->ov_buf[i], &vals->ov_vec.v_count[i]);
		if (rc != 0)
			return rc;
	}

	return 0;
}

static int do_add_extents(struct m0_uint128 id,
		       struct m0_bufvec *keys,
		       struct m0_bufvec *vals,
		       int    *rcs)
{
	int                  rc;
	struct m0_clovis_op *ops[1] = {NULL};
	struct m0_clovis_idx idx;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	m0_clovis_composite_layer_idx(id, true, &idx);
	clovis_st_idx_op(&idx, M0_CLOVIS_IC_PUT, keys, vals, rcs, 0, &ops[0]);
	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	rc = rc != 0?rc:ops[0]->op_sm.sm_rc;

	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&idx.in_entity);

	return rc;
}

static int
add_extents(struct m0_uint128 id, int nr_exts, struct composite_extent *exts)
{
	int               rc = 0;
	struct m0_bufvec  keys;
	struct m0_bufvec  vals;
	int              *rcs = NULL;

	/* Allocate bufvec's for keys and vals. */
	rc = m0_bufvec_empty_alloc(&keys, nr_exts)?:
	     m0_bufvec_empty_alloc(&vals, nr_exts);
	if (rc != 0) {
		rc = -ENOMEM;
		goto exit;
	}
	M0_ALLOC_ARR(rcs, nr_exts);
	if (rcs == NULL) {
		rc = -ENOMEM;
		goto exit;
	}

	/* Fill keys and values with some data. */
	rc = fill_ext_kv_pairs(exts, nr_exts, &keys, &vals);
	if (rc < 0)
		goto exit;

	/* Do the real job. */
	rc = do_add_extents(id, &keys, &vals, rcs);

exit:
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	m0_free0(&rcs);
	return rc;
}

static void layout_composite_io_one_layer(void)
{
	int                      i;
	int                      j;
	int                      rc = 0;
	int                      nr_comp_exts = 0;
	int                      nr_io_segs;
	int                      nr_layers;
	struct m0_uint128        id;
	struct m0_uint128       *layer_ids;
	struct m0_clovis_layout *layout;
	struct io_seg           *io_segs;
	struct composite_extent *comp_exts;
	m0_bindex_t              off;
	m0_bcount_t              len;

	/* Create a composite layout with layers. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	nr_layers = 3;
	M0_ALLOC_ARR(layer_ids, nr_layers);
	CLOVIS_ST_ASSERT_FATAL(layer_ids != NULL);
	rc = layout_composite_add_layers(layout, nr_layers, layer_ids);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Create a new object with the captured layout.*/
	rc = layout_composite_create_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Add extents to one layer. */
	nr_comp_exts = 10;
	M0_ALLOC_ARR(comp_exts, nr_comp_exts);
	CLOVIS_ST_ASSERT_FATAL(comp_exts != NULL);
	for (i = 0; i < nr_layers; i++) {
		off = 0;
		for (j = 0; j < nr_comp_exts; j++) {
			len = (j + 1) *1024 * 1024;

			/* Set key and value. */
			comp_exts[j].ce_id = layer_ids[i];
			comp_exts[j].ce_off = off;
			comp_exts[j].ce_len = len;

			off += len + 1024 * 1024;
		}
		rc = add_extents(layer_ids[i], nr_comp_exts, comp_exts);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
	}

	/* Write to the newly created object above.*/
	nr_io_segs= 4;
	M0_ALLOC_ARR(io_segs, nr_io_segs);
	CLOVIS_ST_ASSERT_FATAL(io_segs != NULL);
	for (i = 0; i < nr_io_segs; i++) {
		io_segs[i].is_off = i * unit_size;
		io_segs[i].is_len = unit_size;
	}
	rc = write_io_segs(id, nr_io_segs, io_segs);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	m0_clovis_layout_free(layout);
	m0_free(layer_ids);
	m0_free(comp_exts);
	m0_free(io_segs);
}

static void layout_composite_io_multi_layers(void)
{
	int                      i;
	int                      rc = 0;
	int                      nr_io_segs;
	int                      nr_layers;
	struct m0_uint128        id;
	struct m0_uint128       *layer_ids;
	struct m0_clovis_layout *layout;
	struct io_seg           *io_segs;
	struct composite_extent  comp_ext;
	m0_bindex_t              off;
	m0_bcount_t              len;

	/* Create a composite layout with layers. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	nr_layers = 3;
	M0_ALLOC_ARR(layer_ids, nr_layers);
	CLOVIS_ST_ASSERT_FATAL(layer_ids != NULL);
	rc = layout_composite_add_layers(layout, nr_layers, layer_ids);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Create a new object with the captured layout.*/
	rc = layout_composite_create_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Add extents to one layer. */
	off = 0;
	for (i = 0; i < nr_layers; i++) {
		/* Set key and value. */
		off = i * 16 * unit_size;
		len = 16 * unit_size;
		comp_ext.ce_id = layer_ids[i];
		comp_ext.ce_off = off;
		comp_ext.ce_len = len;
		rc = add_extents(layer_ids[i], 1, &comp_ext);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
	}

	/* Write to the newly created object above.*/
	nr_io_segs= 3;
	M0_ALLOC_ARR(io_segs, nr_io_segs);
	CLOVIS_ST_ASSERT_FATAL(io_segs != NULL);
	for (i = 0; i < nr_io_segs; i++) {
		io_segs[i].is_off = i * 16 * unit_size;
		io_segs[i].is_len = 16 * unit_size;
	}
	rc = write_io_segs(id, nr_io_segs, io_segs);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	m0_clovis_layout_free(layout);
	m0_free(layer_ids);
	m0_free(io_segs);
}
static void layout_composite_io_overlapping_layers(void)
{
	int                      i;
	int                      rc = 0;
	int                      nr_io_segs;
	int                      nr_layers;
	struct m0_uint128        id;
	struct m0_uint128       *layer_ids;
	struct m0_clovis_layout *layout;
	struct io_seg           *io_segs;
	struct composite_extent  comp_ext;
	m0_bindex_t              off;
	m0_bcount_t              len;

	/* Create a composite layout with layers. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	nr_layers = 3;
	M0_ALLOC_ARR(layer_ids, nr_layers);
	CLOVIS_ST_ASSERT_FATAL(layer_ids != NULL);
	rc = layout_composite_add_layers(layout, nr_layers, layer_ids);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Create a new object with the captured layout.*/
	rc = layout_composite_create_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Add extents to one layer. */
	off = 0;
	for (i = 0; i < nr_layers; i++) {
		/* Set key and value. */
		len = 16 * unit_size;
		comp_ext.ce_id = layer_ids[i];
		comp_ext.ce_off = off;
		comp_ext.ce_len = len;
		rc = add_extents(layer_ids[i], 1, &comp_ext);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
		off += len - 4 * unit_size;
	}

	/* Write to the newly created object above.*/
	nr_io_segs= 3;
	M0_ALLOC_ARR(io_segs, nr_io_segs);
	CLOVIS_ST_ASSERT_FATAL(io_segs != NULL);
	for (i = 0; i < nr_io_segs; i++) {
		io_segs[i].is_off = i * 16 * unit_size;
		if (i == nr_io_segs - 1)
			io_segs[i].is_len = (16 - 8) * unit_size;
		else
			io_segs[i].is_len = 16 * unit_size;
	}
	rc = write_io_segs(id, nr_io_segs, io_segs);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	m0_clovis_layout_free(layout);
	m0_free(layer_ids);
	m0_free(io_segs);
}

static void layout_composite_io_on_capture_layer(void)
{
	int                      i;
	int                      rc = 0;
	int                      nr_comp_exts = 0;
	int                      nr_io_segs;
	struct m0_uint128        orig_id;
	struct m0_uint128        cap_id;
	struct m0_uint128        id;
	struct m0_clovis_layout *orig_layout;
	struct m0_clovis_layout *cap_layout;
	struct m0_clovis_layout *layout;
	struct m0_clovis_op     *ops[] = {NULL};
	struct m0_clovis_obj    *orig_obj;
	struct m0_clovis_obj     obj;
	struct io_seg           *io_segs;
	struct composite_extent *comp_exts;
	m0_bindex_t              off;
	m0_bcount_t              len;

	/* Create an object with default layout type (PDCLUST). */
	clovis_oid_get(&orig_id);
	create_obj(orig_id);

	/*
	 * Sample code to capture layout and set an object with
 	 * captured layout.
 	 */
	M0_ALLOC_PTR(orig_obj);
	CLOVIS_ST_ASSERT_FATAL(orig_obj != NULL);

	/* 1. Initialise in-memory object data struct. */
	clovis_st_obj_init(orig_obj, &clovis_st_layout_container.co_realm,
			   &orig_id, layout_id);

	/* 2. Open the object to get its attributes. */
	clovis_st_entity_open(&orig_obj->ob_entity);
	CLOVIS_ST_ASSERT_FATAL(m0_clovis_obj_layout_type(orig_obj) ==
			       M0_CLOVIS_LT_PDCLUST);

	/* 3. Issue LAYOUT_GET op. */
	orig_layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_PDCLUST);
	CLOVIS_ST_ASSERT_FATAL(orig_layout != NULL);

	clovis_st_layout_op(orig_obj, M0_CLOVIS_EO_LAYOUT_GET,
			    orig_layout, &ops[0]);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0] != NULL);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	ops[0] = NULL;

	/* 4. Capture layout. */
	rc = m0_clovis_layout_capture(orig_layout, orig_obj, &cap_layout);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(cap_layout != NULL);

	/* 5. Create a new object with the captured layout.*/
	rc = layout_create_capture_obj(cap_layout, &cap_id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* 6. Create a composite layout including the capture subobject. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	M0_SET0(&obj);
	clovis_st_obj_init(&obj, &clovis_st_layout_container.co_realm,
			   &cap_id, layout_id);
	rc = m0_clovis_composite_layer_add(layout, &obj, 0);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	clovis_st_entity_fini(&obj.ob_entity);

	rc = layout_composite_create_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Add extents to one layer. */
	nr_comp_exts = 10;
	M0_ALLOC_ARR(comp_exts, nr_comp_exts);
	CLOVIS_ST_ASSERT_FATAL(comp_exts != NULL);

	off = 0;
	for (i = 0; i < nr_comp_exts; i++) {
		len = (i + 1) *1024 * 1024;

		/* Set key and value. */
		comp_exts[i].ce_id = cap_id;
		comp_exts[i].ce_off = off;
		comp_exts[i].ce_len = len;

		off += len + 1024 * 1024;
	}
	rc = add_extents(cap_id, nr_comp_exts, comp_exts);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* 7. Write to the newly created object above.*/
	nr_io_segs= 4;
	M0_ALLOC_ARR(io_segs, nr_io_segs);
	CLOVIS_ST_ASSERT_FATAL(io_segs != NULL);
	for (i = 0; i < nr_io_segs; i++) {
		io_segs[i].is_off = i * unit_size;
		io_segs[i].is_len = unit_size;
	}
	rc = write_io_segs(id, nr_io_segs, io_segs);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Fini and free entities and layouts. */
	m0_clovis_layout_free(cap_layout);
	clovis_st_entity_fini(&orig_obj->ob_entity);
	m0_clovis_layout_free(orig_layout);
	m0_free(orig_obj);

	m0_clovis_layout_free(layout);
	m0_free(comp_exts);
	m0_free(io_segs);
}

static void layout_composite_extent_idx(void)
{
	int                      i;
	int                      j;
	int                      rc = 0;
	int                      nr_comp_exts = 0;
	int                      nr_layers;
	struct m0_uint128        id;
	struct m0_uint128       *layer_ids;
	struct m0_clovis_layout *layout;
	struct composite_extent *comp_exts;
	m0_bindex_t              off;
	m0_bcount_t              len;

	/* Create a composite layout with layers. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	CLOVIS_ST_ASSERT_FATAL(layout != NULL);

	nr_layers = 3;
	M0_ALLOC_ARR(layer_ids, nr_layers);
	CLOVIS_ST_ASSERT_FATAL(layer_ids != NULL);
	rc = layout_composite_add_layers(layout, nr_layers, layer_ids);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Create a new object with composite layout.*/
	rc = layout_composite_create_obj(layout, &id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Add extents to one layer. */
	for (i = 0; i < nr_layers; i++) {
		off = 0;
		nr_comp_exts = 10;
		M0_ALLOC_ARR(comp_exts, nr_comp_exts);
		CLOVIS_ST_ASSERT_FATAL(comp_exts != NULL);
		for (j = 0; j < nr_comp_exts; j++) {
			len = (j + 1) *1024 * 1024;

			/* Set key and value. */
			comp_exts[j].ce_id = layer_ids[i];
			comp_exts[j].ce_off = off;
			comp_exts[j].ce_len = len;

			off += len + 1024 * 1024;
		}
		rc = add_extents(layer_ids[i], nr_comp_exts, comp_exts);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
	}

	m0_clovis_layout_free(layout);
	m0_free(layer_ids);
	m0_free(comp_exts);
}

/**
 * Initialises the obj suite's environment.
 */
static int clovis_st_layout_suite_init(void)
{
	int rc = 0;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	m0_clovis_container_init(&clovis_st_layout_container, NULL,
				 &M0_CLOVIS_UBER_REALM,
				 clovis_st_get_instance());
	rc = clovis_st_layout_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	test_id = M0_CLOVIS_ID_APP;
	test_id.u_lo += generate_random(0xffff);

	layout_id = m0_clovis_layout_id(clovis_st_get_instance());
	return rc;
}

/**
 * Finalises the obj suite's environment.
 */
static int clovis_st_layout_suite_fini(void)
{
	return 0;
}

struct clovis_st_suite st_suite_clovis_layout = {
	.ss_name = "clovis_layout_st",
	.ss_init = clovis_st_layout_suite_init,
	.ss_fini = clovis_st_layout_suite_fini,
	.ss_tests = {
		{ "layout_op_get_obj",
		  &layout_op_get_obj},
		{ "layout_capture",
		  &layout_capture},
		{ "layout_capture_io",
		  &layout_capture_io},
		{ "layout_composite_create",
		  &layout_composite_create},
		{ "layout_composite_create_then_get",
		  &layout_composite_create_then_get},
		{ "layout_composite_extent_idx",
		  &layout_composite_extent_idx},
		{ "layout_composite_io_one_layer",
		  &layout_composite_io_one_layer},
		{ "layout_composite_io_multi_layers",
		  &layout_composite_io_multi_layers},
		{ "layout_composite_io_overlapping_layers",
		  &layout_composite_io_overlapping_layers},
		{ "layout_composite_io_on_capture_layer",
		  &layout_composite_io_on_capture_layer},
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
