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

struct m0_clovis_container clovis_st_write_container;
extern struct m0_addb_ctx m0_clovis_addb_ctx;

enum { MAX_OPS = 16 };

/* Parity group aligned (in units)*/
enum {
	SMALL_OBJ_SIZE  = 1   * DEFAULT_PARGRP_DATA_SIZE,
	MEDIUM_OBJ_SIZE = 12  * DEFAULT_PARGRP_DATA_SIZE,
	LARGE_OBJ_SIZE  = 120 * DEFAULT_PARGRP_DATA_SIZE
};

static int create_obj(struct m0_uint128 *oid, int unit_size)
{
	int                   rc = 0;
	struct m0_clovis_op  *ops[1] = {NULL};
	struct m0_clovis_obj  obj;
	struct m0_uint128     id;
	uint64_t              lid;

	/* Make sure everything in 'obj' is clean*/
	memset(&obj, 0, sizeof(obj));

	clovis_oid_get(&id);
	lid = m0_clovis_obj_unit_size_to_layout_id(unit_size);
	clovis_st_obj_init(&obj, &clovis_st_write_container.co_realm, &id, lid);

	clovis_st_entity_create(NULL, &obj.ob_entity, &ops[0]);
	if (ops[0] == NULL)
		return -ENOENT;

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));

	rc = clovis_st_op_wait(
		ops[0], M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
		m0_time_from_now(3,0));

	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&obj.ob_entity);

	*oid = id;
	return rc;
}

static int compare_data(struct m0_bufvec b1, struct m0_bufvec b2)
{
	int                     i;
	int                     rc = 0;
	void                   *d1;
	void                   *d2;
	uint64_t                l1;
	uint64_t                l2;
	uint64_t                step;
	struct m0_bufvec_cursor c1;
	struct m0_bufvec_cursor c2;

	m0_bufvec_cursor_init(&c1, &b1);
	m0_bufvec_cursor_init(&c2, &b2);

	step = 0;
	while (m0_bufvec_cursor_move(&c1, step) != 0
	       && m0_bufvec_cursor_move(&c2, step) != 0)
	{
		d1 = m0_bufvec_cursor_addr(&c1);
		d2 = m0_bufvec_cursor_addr(&c2);
		if (d1 == NULL || d2 == NULL) {
			rc = -EINVAL;
			break;
		}

		l1 = m0_bufvec_cursor_step(&c1);
		l2 = m0_bufvec_cursor_step(&c2);
		step = (l1 < l2)?l1:l2;

		/* Check the content*/
		for (i = 0; i < step; i++)
			if (((char *)d1)[i] != ((char *)d2)[i])
				break;
		if (i < step) {
			rc = -1;
			break;
		}
	}

	return rc;
}

/* Verify each stride one by one */
static int write_verify(struct m0_bufvec *data_w, struct m0_uint128 oid,
			int start, int stride, int unit_size, int nr_ops)
{
	int                   i;
	int                   rc;
	struct m0_clovis_obj  obj;
	struct m0_clovis_op  *ops_r[1] = {NULL};
	struct m0_indexvec    ext_r;
	struct m0_bufvec      data_r;
	struct m0_bufvec      attr_r;
	uint64_t              lid;

	rc = 0;
	lid = m0_clovis_obj_unit_size_to_layout_id(unit_size);

	/* Setup bufvec, indexvec and ops for READs */
	for (i = 0; i < nr_ops; i++) {
		M0_SET0(&obj);
		ops_r[0] = NULL;

		clovis_st_obj_init(&obj, &clovis_st_write_container.co_realm,
				   &oid, lid);

		clovis_st_entity_open(&obj.ob_entity);

		if (m0_indexvec_alloc(&ext_r, 1) ||
		    m0_bufvec_alloc(&data_r, 1, unit_size * stride) ||
		    m0_bufvec_alloc(&attr_r, 1, 1))
		{
			rc = -ENOMEM;
			goto CLEANUP;
		}

		ext_r.iv_index[0] = unit_size * (start + i * stride);
		ext_r.iv_vec.v_count[0] = unit_size * stride;
		attr_r.ov_vec.v_count[0] = 0;

		/* Create and launch the read requests */
		clovis_st_obj_op(&obj, M0_CLOVIS_OC_READ,
			      &ext_r, &data_r, &attr_r, 0, &ops_r[0]);
		if (ops_r[0] == NULL)
			goto CLEANUP;

		clovis_st_op_launch(ops_r, 1);

		/* Wait for read to finish */
		rc = clovis_st_op_wait(ops_r[0],
			    M0_BITS(M0_CLOVIS_OS_FAILED,
				    M0_CLOVIS_OS_STABLE),
			    M0_TIME_NEVER);

		/* Compare the data */
		if (rc == 0 && ops_r[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE)
			rc = compare_data(data_r, data_w[i]);

		clovis_st_op_fini(ops_r[0]);
		clovis_st_op_free(ops_r[0]);

CLEANUP:
		clovis_st_entity_fini(&obj.ob_entity);

		if (ext_r.iv_vec.v_nr != 0)
			m0_indexvec_free(&ext_r);
		if (data_r.ov_buf != NULL)
			m0_bufvec_free(&data_r);
		if (attr_r.ov_buf != NULL)
			m0_bufvec_free(&attr_r);

		if (rc != 0) {
			console_printf("write_verfiy: m0_clovis_op_wait failed\n");
			break;
		}
	}

	return rc;
}

/*
 * We issue 'nr_ops' of WRITES in one go.
 * stride: in units
 */
static int write_obj(struct m0_uint128 oid, int start,
		     int stride, int unit_size, int nr_ops, bool verify)
{
	int                    i;
	int                    rc = 0;
	uint64_t               lid;
	struct m0_clovis_obj   obj;
	struct m0_clovis_op  **ops_w;
	struct m0_indexvec    *ext_w;
	struct m0_bufvec      *data_w;
	struct m0_bufvec      *attr_w;

	M0_CLOVIS_THREAD_ENTER;

	if (nr_ops > MAX_OPS)
		return -EINVAL;

	/* Init obj */
	M0_SET0(&obj);
	lid = m0_clovis_obj_unit_size_to_layout_id(unit_size);
	clovis_st_obj_init(&obj, &clovis_st_write_container.co_realm, &oid, lid);

	clovis_st_entity_open(&obj.ob_entity);

	/* Setup bufvec, indexvec and ops for WRITEs */
	MEM_ALLOC_ARR(ops_w, nr_ops);
	MEM_ALLOC_ARR(ext_w, nr_ops);
	MEM_ALLOC_ARR(data_w, nr_ops);
	MEM_ALLOC_ARR(attr_w, nr_ops);
	if (ops_w == NULL || ext_w == NULL || data_w == NULL || attr_w == NULL)
		goto CLEANUP;

	for (i = 0; i < nr_ops; i++) {
		if (m0_indexvec_alloc(&ext_w[i], 1) ||
		    m0_bufvec_alloc(&data_w[i], 1, unit_size * stride) ||
		    m0_bufvec_alloc(&attr_w[i], 1, 1))
		{
			rc = -ENOMEM;
			goto CLEANUP;
		}

		memset(data_w[i].ov_buf[0], 'A', unit_size * stride);
		ext_w[i].iv_index[0] = unit_size * (start +  i * stride);
		ext_w[i].iv_vec.v_count[0] = unit_size * stride;
		attr_w[i].ov_vec.v_count[0] = 0;
	}

	/* Create and launch write requests */
	for (i = 0; i < nr_ops; i++) {
		ops_w[i] = NULL;
		clovis_st_obj_op(&obj, M0_CLOVIS_OC_WRITE,
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

	/* 3. Verify the data written */
	if (!verify)
		goto CLEANUP;
	rc = write_verify(data_w, oid, start, stride, unit_size, nr_ops);

CLEANUP:
	clovis_st_entity_fini(&obj.ob_entity);

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

/*
 * Because of the current limitation of CLOVIS_ST_ASSERT (can only be used
 * in top function), we duplicate for the following small/medium/large tests
 * to get more assert checks.
 */
#define write_objs(nr_objs, size, unit_size, verify) \
	do { \
		int               rc; \
		int               i; \
		int               j; \
		int               start = 0; \
		int               nr_units; \
		int               nr_pargrps; \
		struct m0_uint128 oid; \
                                                \
		nr_units = size / unit_size; \
		nr_pargrps = nr_units / DEFAULT_PARGRP_DATA_UNIT_NUM; \
                                                                      \
		for (i = 0; i < nr_objs; i++) { \
			rc = create_obj(&oid, unit_size); \
			CLOVIS_ST_ASSERT_FATAL(rc == 0);\
                                                        \
			for (j = 0; j < nr_pargrps; j++) { \
				start = j * DEFAULT_PARGRP_DATA_UNIT_NUM; \
				rc = write_obj(oid, start, \
					DEFAULT_PARGRP_DATA_UNIT_NUM, \
					unit_size, 1, verify);\
				CLOVIS_ST_ASSERT_FATAL(rc == 0);	\
			}						\
									\
			/* write the rest of data */			\
			start += DEFAULT_PARGRP_DATA_UNIT_NUM;		\
			if (start >= nr_units)				\
				continue;				\
									\
			rc = write_obj(oid, start, \
				nr_units - start, unit_size, 1, verify); \
			CLOVIS_ST_ASSERT_FATAL(rc == 0); \
		} \
	} while(0)

static void write_small_objs(void)
{
	write_objs(10, SMALL_OBJ_SIZE, DEFAULT_PARGRP_UNIT_SIZE, true);
}

static void write_medium_objs(void)
{
	write_objs(10, MEDIUM_OBJ_SIZE, DEFAULT_PARGRP_UNIT_SIZE, false);
}

static void write_large_objs(void)
{
	write_objs(1, LARGE_OBJ_SIZE, DEFAULT_PARGRP_UNIT_SIZE, false);
}

static void write_with_layout_id(void)
{
	write_objs(1, LARGE_OBJ_SIZE, 16384, true);
}

static void write_pargrps_in_parallel_ops(void)
{
	int               i;
	int               rc;
	int               nr_rounds;
	int               nr_ops;
	struct m0_uint128 oid;

	nr_rounds = 4;

	/* Create an object */
	rc = create_obj(&oid, DEFAULT_PARGRP_UNIT_SIZE);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* We change the number of ops issued together in each round */
	nr_ops = 2^nr_rounds;
	for (i = 0; i < nr_rounds; i++) {
		rc = write_obj(oid, 0,
			       DEFAULT_PARGRP_DATA_UNIT_NUM,
			       DEFAULT_PARGRP_UNIT_SIZE,
			       nr_ops, true);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);

		nr_ops /= 2;
	}
}

static void write_pargrps_rmw(void)
{
	int               i;
	int               rc;
	int               start;
	int               strides[10] = {1, 3, 5, 7, 11, 13, 17, 19, 23, 29};
	struct m0_uint128 oid;

	/* Create an object */
	rc = create_obj(&oid, DEFAULT_PARGRP_UNIT_SIZE);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/*
 	 * Write prime number of units to an object.
	 */
	start  = 0;
	for (i = 0; i < 10; i++) {
		rc = write_obj(oid, start, strides[i],
			       DEFAULT_PARGRP_UNIT_SIZE, 1, true);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
		start += strides[i];
	}
}

/**
 * write a number of parity groups of data to an object then read.
 */
static void write_pargrps(void)
{
	int               i;
	int               rc;
	int               nr_rounds;
	int               start;
	int               stride;
	struct m0_uint128 oid;

	/* 1. Create an object */
	rc = create_obj(&oid, DEFAULT_PARGRP_UNIT_SIZE);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/*
	 * 2. We want to write/read one group from the beginning
	 *    of the object
	 */
	start  = 0;
	stride = 100; /* or DEFAULT_PARGRP_DATA_UNIT_NUM */;
	nr_rounds = 2;
	for (i = 0; i < nr_rounds; i++) {
		rc = write_obj(oid, start, stride,
			       DEFAULT_PARGRP_UNIT_SIZE, 1, true);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
		start += stride;
	}
}

/* Initialises the Clovis environment.*/
static int clovis_st_write_init(void)
{
	int rc = 0;

#if 0
	clovis_st_obj_prev_trace_level = m0_trace_level;
	m0_trace_level = M0_DEBUG;
#endif

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	clovis_st_container_init(&clovis_st_write_container,
			      NULL, &M0_CLOVIS_UBER_REALM,
			      clovis_st_get_instance());
	rc = clovis_st_write_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	(void)m0_clovis_layout_id(clovis_st_get_instance());
	return rc;
}

/* Finalises the Clovis environment.*/
static int clovis_st_write_fini(void)
{
	//m0_trace_level = clovis_st_obj_prev_trace_level;
	return 0;
}

struct clovis_st_suite st_suite_clovis_write = {
	.ss_name = "clovis_write_st",
	.ss_init = clovis_st_write_init,
	.ss_fini = clovis_st_write_fini,
	.ss_tests = {
		{ "write_pargrps", &write_pargrps},
		{ "write_pargrps_rmw", &write_pargrps_rmw},
		{ "write_pargrps_in_parallel_ops", &write_pargrps_in_parallel_ops},
		{ "write_small_objs",  &write_small_objs},
		{ "write_medium_objs", &write_medium_objs},
		{ "write_large_objs", &write_large_objs},
		{ "write_with_layout_id", &write_with_layout_id},
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
