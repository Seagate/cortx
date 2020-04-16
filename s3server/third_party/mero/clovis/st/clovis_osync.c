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
 * Original creation date: 11-11-2014
 */

/*
 * Clovis API system tests to check if Clovis API matches its
 * specifications.
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#include "clovis/clovis.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"

#include "lib/memory.h"

struct m0_clovis_container clovis_st_osync_container;
static uint64_t layout_id;

enum {
	PARGRP_UNIT_SIZE     = 4096,
	PARGRP_DATA_UNIT_NUM = 2,
	PARGRP_DATA_SIZE     = PARGRP_DATA_UNIT_NUM * PARGRP_UNIT_SIZE,
	MAX_OPS              = 16
};

/* Parity group aligned (in units)*/
enum {
	SMALL_OBJ_SIZE  = 1    * PARGRP_DATA_UNIT_NUM,
	MEDIUM_OBJ_SIZE = 100  * PARGRP_DATA_UNIT_NUM,
	LARGE_OBJ_SIZE  = 2000 * PARGRP_DATA_UNIT_NUM
};

static int create_obj(struct m0_uint128 *oid)
{
	int                   rc = 0;
	struct m0_clovis_op  *ops[1] = {NULL};
	struct m0_clovis_obj  obj;
	struct m0_uint128     id;

	/* Make sure everything in 'obj' is clean*/
	memset(&obj, 0, sizeof(obj));

	clovis_oid_get(&id);
	clovis_st_obj_init(&obj, &clovis_st_osync_container.co_realm,
			   &id, layout_id);

	clovis_st_entity_create(NULL, &obj.ob_entity, &ops[0]);
	if (ops[0] == NULL)
		return -ENOENT;

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));

	rc = clovis_st_op_wait(
		ops[0], M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
		m0_time_from_now(3,0));

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_obj_fini(&obj);

	*oid = id;
	return rc;
}

/*
 * We issue 'nr_ops' of WRITES in one go.
 * stride: in units
 */
static int write_obj(struct m0_clovis_obj *obj,
		     int start, int stride, int nr_ops)
{
	int                    i;
	int                    rc = 0;
	struct m0_clovis_op  **ops_w;
	struct m0_indexvec    *ext_w;
	struct m0_bufvec      *data_w;
	struct m0_bufvec      *attr_w;

	M0_CLOVIS_THREAD_ENTER;

	if (nr_ops > MAX_OPS)
		return -EINVAL;

	/* Setup bufvec, indexvec and ops for WRITEs */
	MEM_ALLOC_ARR(ops_w, nr_ops);
	MEM_ALLOC_ARR(ext_w, nr_ops);
	MEM_ALLOC_ARR(data_w, nr_ops);
	MEM_ALLOC_ARR(attr_w, nr_ops);
	if (ops_w == NULL || ext_w == NULL || data_w == NULL || attr_w == NULL)
		goto CLEANUP;

	for (i = 0; i < nr_ops; i++) {
		if (m0_indexvec_alloc(&ext_w[i], 1) ||
		    m0_bufvec_alloc(&data_w[i], 1, 4096 * stride) ||
		    m0_bufvec_alloc(&attr_w[i], 1, 1))
		{
			rc = -ENOMEM;
			goto CLEANUP;
		}

		ext_w[i].iv_index[0] = 4096 * (start +  i * stride);
		ext_w[i].iv_vec.v_count[0] = 4096 * stride;
		attr_w[i].ov_vec.v_count[0] = 0;
	}

	/* Create and launch write requests */
	for (i = 0; i < nr_ops; i++) {
		ops_w[i] = NULL;
		clovis_st_obj_op(obj, M0_CLOVIS_OC_WRITE,
			      &ext_w[i], &data_w[i], &attr_w[i], 0, &ops_w[i]);
		if (ops_w[i] == NULL)
			break;
	}
	if (i == 0) goto CLEANUP;

	clovis_st_op_launch(ops_w, nr_ops);

	/* Wait for write to finish */
	for (i = 0; i < nr_ops; i++) {
		rc = clovis_st_op_wait(ops_w[i],
			M0_BITS(M0_CLOVIS_OS_FAILED,
				M0_CLOVIS_OS_STABLE),
			M0_TIME_NEVER);

		clovis_st_op_fini(ops_w[i]);
		clovis_st_op_free(ops_w[i]);
	}

CLEANUP:

	for (i = 0; i < nr_ops; i++) {
		if (ext_w != NULL && ext_w[i].iv_vec.v_nr != 0)
			m0_indexvec_free(&ext_w[i]);
		if (data_w != NULL && data_w[i].ov_buf != NULL)
			m0_bufvec_free(&data_w[i]);
		if (attr_w != NULL && attr_w[i].ov_buf != NULL)
			m0_bufvec_free(&attr_w[i]);
	}

	if (ops_w != NULL) mem_free(ops_w);
	if (ext_w != NULL) mem_free(ext_w);
	if (data_w != NULL) mem_free(data_w);
	if (attr_w != NULL) mem_free(attr_w);

	return rc;
}

/**
 * sync data for each write operation.
 */
static void osync_after_each_write(void)
{
	int                   i;
	int                   rc;
	int                   start;
	int                   stride;
	struct m0_uint128     oid;
	struct m0_clovis_obj *obj_to_sync;

	MEM_ALLOC_PTR(obj_to_sync);
	CLOVIS_ST_ASSERT_FATAL(obj_to_sync != NULL);

	/* Create an object */
	rc = create_obj(&oid);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Init obj */
	clovis_st_obj_init(obj_to_sync,
			   &clovis_st_osync_container.co_realm,
			   &oid, layout_id);

	clovis_st_entity_open(&obj_to_sync->ob_entity);

	/* Write multiple times and sync after each write*/
	start  = 0;
	stride = PARGRP_DATA_UNIT_NUM;
	for (i = 0; i < 2; i++) {
		rc = write_obj(obj_to_sync, start, stride, 1);
		CLOVIS_ST_ASSERT_FATAL(rc == 0)

		rc = m0_clovis_entity_sync(&obj_to_sync->ob_entity);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);

		start += stride;
		stride *= 2;
	}

	clovis_st_obj_fini(obj_to_sync);
	mem_free(obj_to_sync);
}

/**
 * Only sync data after all write ops are done.
 */
static void osync_after_writes(void)
{
	int                   i;
	int                   rc;
	int                   start;
	int                   stride;
	struct m0_uint128     oid;
	struct m0_clovis_obj *obj_to_sync;

	MEM_ALLOC_PTR(obj_to_sync);
	CLOVIS_ST_ASSERT_FATAL(obj_to_sync != NULL);

	/* Create an object */
	rc = create_obj(&oid);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Init obj */
	clovis_st_obj_init(obj_to_sync,
			   &clovis_st_osync_container.co_realm,
			   &oid, layout_id);

	clovis_st_entity_open(&obj_to_sync->ob_entity);

	/* Write multiple times and sync after all writes*/
	start  = 0;
	stride = PARGRP_DATA_UNIT_NUM;
	for (i = 0; i < 2; i++) {
		rc = write_obj(obj_to_sync, start, stride, 1);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);

		start += stride;
		stride *= 2;
	}

	rc = m0_clovis_entity_sync(&obj_to_sync->ob_entity);
	CLOVIS_ST_ASSERT_FATAL (rc == 0)

	clovis_st_obj_fini(obj_to_sync);
	mem_free(obj_to_sync);
}

static void osync_by_sync_op(void)
{
	int                      i;
	int                      rc;
	int                      start;
	int                      stride;
	struct m0_uint128        oid;
	struct m0_clovis_op     *sync_op = {NULL};
	struct m0_clovis_obj    *obj_to_sync;

	MEM_ALLOC_PTR(obj_to_sync);
	CLOVIS_ST_ASSERT_FATAL(obj_to_sync != NULL);

	/* Create an object */
	rc = create_obj(&oid);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Init obj */
	clovis_st_obj_init(obj_to_sync,
			   &clovis_st_osync_container.co_realm,
			   &oid, layout_id);

	clovis_st_entity_open(&obj_to_sync->ob_entity);

	/* Write multiple times and sync after all writes using sync op.*/
	start  = 0;
	stride = PARGRP_DATA_UNIT_NUM;
	for (i = 0; i < 2; i++) {
		rc = write_obj(obj_to_sync, start, stride, 1);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);

		start += stride;
		stride *= 2;
	}

	/* Lauch and wait on sync op. */
	rc = m0_clovis_sync_op_init(&sync_op);
	M0_ASSERT(rc == 0);
	rc = m0_clovis_sync_entity_add(sync_op, &obj_to_sync->ob_entity);
	M0_ASSERT(rc == 0);

	clovis_st_op_launch(&sync_op, 1);
	rc = clovis_st_op_wait(
		sync_op, M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
		m0_time_from_now(3,0));
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	clovis_st_op_fini(sync_op);
	clovis_st_op_free(sync_op);

	/* Clean up objects. */
	clovis_st_obj_fini(obj_to_sync);
	mem_free(obj_to_sync);
}

/**
 * Tests SYNC_ON_OP for objects.
 */
static void osync_on_op(void)
{
	int                    i;
	int                    rc = 0;
	int                    bytes_to_write;
	int                    nr_objs;
	int                    nr_objs_written;
	int                   *sync_rcs;
	struct m0_clovis_obj  *objs_to_sync;
	struct m0_clovis_op  **ops_w;
	struct m0_clovis_op   *sync_op = {NULL};
	struct m0_indexvec    *ext_w;
	struct m0_bufvec      *data_w;
	struct m0_bufvec      *attr_w;
	struct m0_uint128      oid;

	M0_CLOVIS_THREAD_ENTER;

	bytes_to_write = 4096;
	nr_objs = 1;

	/* Setup bufvec, indexvec and ops for WRITEs */
	MEM_ALLOC_ARR(objs_to_sync, nr_objs);
	MEM_ALLOC_ARR(ops_w, nr_objs);
	MEM_ALLOC_ARR(ext_w, nr_objs);
	MEM_ALLOC_ARR(data_w, nr_objs);
	MEM_ALLOC_ARR(attr_w, nr_objs);
	if (objs_to_sync == NULL || ops_w == NULL || ext_w == NULL ||
	    data_w == NULL || attr_w == NULL)
		goto cleanup;

	for (i = 0; i < nr_objs; i++) {
		if (m0_indexvec_alloc(&ext_w[i], 1) ||
		    m0_bufvec_alloc(&data_w[i], 1, bytes_to_write) ||
		    m0_bufvec_alloc(&attr_w[i], 1, 1))
		{
			rc = -ENOMEM;
			goto cleanup;
		}

		ext_w[i].iv_index[0] = 0;
		ext_w[i].iv_vec.v_count[0] = bytes_to_write;
		attr_w[i].ov_vec.v_count[0] = 0;
	}

	/* Create and write objects. */
	for (i = 0; i < nr_objs; i++) {
		rc = create_obj(&oid);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);

		/* Init obj */
		clovis_st_obj_init(objs_to_sync + i,
				   &clovis_st_osync_container.co_realm,
				   &oid, layout_id);

		clovis_st_entity_open(&(objs_to_sync + i)->ob_entity);

		/* Create and launch write requests */
		ops_w[i] = NULL;
		clovis_st_obj_op(objs_to_sync + i, M0_CLOVIS_OC_WRITE,
			      &ext_w[i], &data_w[i], &attr_w[i], 0, &ops_w[i]);
		if (ops_w[i] == NULL)
			break;

		clovis_st_op_launch(ops_w, 1);

		/* Wait for write to finish */
		rc = clovis_st_op_wait(ops_w[i],
			M0_BITS(M0_CLOVIS_OS_FAILED,
				M0_CLOVIS_OS_STABLE),
			M0_TIME_NEVER);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
	}
	nr_objs_written = i;

	/* Send an SYNC_ON_OPS op. */
	M0_ALLOC_ARR(sync_rcs, nr_objs_written);
	if (sync_rcs == NULL)
		goto ops_w_fini;

	rc = m0_clovis_sync_op_init(&sync_op);
	M0_ASSERT(rc == 0);
	for (i = 0; i < nr_objs_written; i++) {
		rc = m0_clovis_sync_op_add(sync_op, ops_w[i]);
		M0_ASSERT(rc == 0);
	}

	clovis_st_op_launch(&sync_op, 1);
	rc = clovis_st_op_wait(
		sync_op, M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
		M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	clovis_st_op_fini(sync_op);
	clovis_st_op_free(sync_op);
	mem_free(sync_rcs);

ops_w_fini:
	for (i = 0; i < nr_objs_written; i++) {
		clovis_st_op_fini(ops_w[i]);
		clovis_st_op_free(ops_w[i]);
		clovis_st_obj_fini(objs_to_sync + i);
	}

cleanup:

	for (i = 0; i < nr_objs; i++) {
		if (ext_w != NULL && ext_w[i].iv_vec.v_nr != 0)
			m0_indexvec_free(&ext_w[i]);
		if (data_w != NULL && data_w[i].ov_buf != NULL)
			m0_bufvec_free(&data_w[i]);
		if (attr_w != NULL && attr_w[i].ov_buf != NULL)
			m0_bufvec_free(&attr_w[i]);
	}

	if (ops_w != NULL) mem_free(ops_w);
	if (ext_w != NULL) mem_free(ext_w);
	if (data_w != NULL) mem_free(data_w);
	if (attr_w != NULL) mem_free(attr_w);
	if (objs_to_sync != NULL) mem_free(objs_to_sync);
}


/* Initialises the Clovis environment.*/
static int clovis_st_osync_init(void)
{
	int rc = 0;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	clovis_st_container_init(&clovis_st_osync_container,
			      NULL, &M0_CLOVIS_UBER_REALM,
			      clovis_st_get_instance());
	rc = clovis_st_osync_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	layout_id = m0_clovis_layout_id(clovis_st_get_instance());
	return rc;
}

/* Finalises the Clovis environment.*/
static int clovis_st_osync_fini(void)
{
	return 0;
}

struct clovis_st_suite st_suite_clovis_osync = {
	.ss_name = "clovis_osync_st",
	.ss_init = clovis_st_osync_init,
	.ss_fini = clovis_st_osync_fini,
	.ss_tests = {
		{ "osync_after_each_write", &osync_after_each_write},
		{ "osync_after_writes",     &osync_after_writes},
		{ "osync_by_sync_op",       &osync_by_sync_op},
		{ "osync_on_op",            &osync_on_op},
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
