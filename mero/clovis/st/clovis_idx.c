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
 * Original author:  Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 1-Sept-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#include "clovis/clovis.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"

#include "lib/memory.h"

#define ST_VAL_STRING  ("Clovis Index Test.")

enum {
	ST_MAX_INDEX_NUM      = 1,
	ST_MAX_KEY_LEN        = 64,
	ST_SMALL_KV_PAIR_NUM  = 20,
	ST_MEDIUM_KV_PAIR_NUM = 100,
	ST_LARGE_KV_PAIR_NUM  = 1000
};

static struct m0_clovis_container clovis_st_idx_container;
static struct m0_uint128 test_index_ids[ST_MAX_INDEX_NUM];
static bool *deleted_kv_pairs;

static int get_max_nr_kv_pairs(int idx_door_no)
{
	int type;

	type = idx_door_no % 3;
	if (type == 0)
		return ST_SMALL_KV_PAIR_NUM;

	if (type == 1)
		return ST_MEDIUM_KV_PAIR_NUM;

	if (type == 2)
		return ST_LARGE_KV_PAIR_NUM;

	return -EINVAL;
}

static void idx_bufvec_free(struct m0_bufvec *bv)
{
	uint32_t i;
	if (bv == NULL)
		return;

	if (bv->ov_buf != NULL) {
		for (i = 0; i < bv->ov_vec.v_nr; ++i)
			if (bv->ov_buf[i] != NULL)
				mem_free(bv->ov_buf[i]);
		mem_free(bv->ov_buf);
	}
	mem_free(bv->ov_vec.v_count);
	mem_free(bv);
}

static struct m0_bufvec* idx_bufvec_alloc(int nr)
{
	struct m0_bufvec *bv;

	bv = m0_alloc(sizeof *bv);
	if (bv == NULL)
		return NULL;

	bv->ov_vec.v_nr = nr;
	M0_ALLOC_ARR(bv->ov_vec.v_count, nr);
	if (bv->ov_vec.v_count == NULL)
		goto FAIL;

	M0_ALLOC_ARR(bv->ov_buf, nr);
	if (bv->ov_buf == NULL)
		goto FAIL;

	return bv;

FAIL:
	m0_bufvec_free(bv);
	return NULL;
}

static bool is_kv_pair_deleted(int idx_door_no, int key_no)
{
	int loc;

	loc = idx_door_no * ST_LARGE_KV_PAIR_NUM + key_no;
	return deleted_kv_pairs[loc];
}

static int idx_pick_keys(int idx_door_no,
			 struct m0_bufvec *keys, int *key_no_arr)
{
	int   i;
	int   rc;
	int   nr_kvp;
	int   nr_picked = 0;
	int   max_nr_kvp;
	int   key_no;
	int   klen;
	char *prefix = NULL;
	char *tmp_str = NULL;
	char *key_str = NULL;
	struct m0_uint128 id;

	if (keys == NULL)
		return -EINVAL;

	rc = -ENOMEM;
	prefix = m0_alloc(ST_MAX_KEY_LEN);
	if (prefix == NULL)
		goto ERROR;

	tmp_str = m0_alloc(ST_MAX_KEY_LEN);
	if (tmp_str == NULL)
		goto ERROR;

	/*
	 * Keys are flled with this format (index fid:key's serial number).
	 */
	id = test_index_ids[idx_door_no];
	nr_kvp = keys->ov_vec.v_nr;
	max_nr_kvp = get_max_nr_kv_pairs(idx_door_no);
	if (nr_kvp > max_nr_kvp)
		goto ERROR;

	for (i = 0; i < nr_kvp; i++) {
		key_no = generate_random(max_nr_kvp);
		if (is_kv_pair_deleted(idx_door_no, key_no))
			continue;
		sprintf(tmp_str,
			"%"PRIx64":%"PRIx64":%d",
			id.u_hi, id.u_lo, key_no);

		klen = strlen(tmp_str);
		key_str = m0_alloc(klen + 1);
		if (key_str == NULL)
			goto ERROR;

		memcpy(key_str, tmp_str, klen);
		key_str[klen] = '\0';
		/* Set bufvec's of keys and vals*/
		keys->ov_vec.v_count[i] = klen;
		keys->ov_buf[i] = key_str;

		nr_picked++;
		if (key_no_arr)
			key_no_arr[i] = key_no;
	}

	return nr_picked;

ERROR:
	if (prefix) m0_free(prefix);
	if (tmp_str) m0_free(tmp_str);
	if (key_str) m0_free(key_str);

	for (i = 0; i < keys->ov_vec.v_nr; ++i) {
		if (keys->ov_buf[i])
			mem_free(keys->ov_buf[i]);
		keys->ov_buf[i] = NULL;
		keys->ov_vec.v_count[i] = 0;
	}

	return rc;
}

static int idx_fill_kv_pairs(struct m0_uint128 id, int start,
			     struct m0_bufvec *keys,
			     struct m0_bufvec *vals)
{
	int   i;
	int   rc;
	int   nr_kvp;
	int   klen;
	int   vlen;
	char *prefix = NULL;
	char *tmp_str = NULL;
	char *key_str = NULL;
	char *val_str = NULL;

	rc = -ENOMEM;
	prefix = m0_alloc(ST_MAX_KEY_LEN);
	if (prefix == NULL)
		goto ERROR;

	tmp_str = m0_alloc(ST_MAX_KEY_LEN);
	if (tmp_str == NULL)
		goto ERROR;

	/*
	 * Keys are flled with this format (index fid:key's serial number).
	 * Values are a dummy string "Clovis Index Test."
	 */
	nr_kvp = keys->ov_vec.v_nr;
	vlen = strlen(ST_VAL_STRING);
	for (i = 0; i < nr_kvp; i++) {
		sprintf(tmp_str,
			"%"PRIx64":%"PRIx64":%d",
			id.u_hi, id.u_lo, start + i);

		klen = strlen(tmp_str);
		key_str = m0_alloc(klen);
		if (key_str == NULL)
			goto ERROR;

		val_str = m0_alloc(vlen);
		if (val_str == NULL)
			goto ERROR;

		memcpy(key_str, tmp_str, klen);
		memcpy(val_str, ST_VAL_STRING, vlen);

		/* Set bufvec's of keys and vals*/
		keys->ov_vec.v_count[i] = klen;
		keys->ov_buf[i] = key_str;
		vals->ov_vec.v_count[i] = vlen;
		vals->ov_buf[i] = val_str;
	}

	if (prefix) m0_free(prefix);
	if (tmp_str) m0_free(tmp_str);
	return 0;

ERROR:
	if (prefix) m0_free(prefix);
	if (tmp_str) m0_free(tmp_str);
	if (key_str) m0_free(key_str);
	if (val_str) m0_free(val_str);
	return rc;
}

static int idx_do_insert_kv_pairs(struct m0_uint128 id,
				  struct m0_bufvec *keys,
				  struct m0_bufvec *vals,
				  int    *rcs)
{
	int                     rc;
	struct m0_clovis_op    *ops[1] = {NULL};
	struct m0_clovis_idx    idx;

	M0_CLOVIS_THREAD_ENTER;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	clovis_st_idx_init(&idx, &clovis_st_idx_container.co_realm, &id);
	clovis_st_idx_op(&idx, M0_CLOVIS_IC_PUT, keys, vals, rcs, 0, &ops[0]);

	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	if (rc < 0) return rc;

	rc = ops[0]->op_rc;
	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&idx.in_entity);

	return rc;
}

static int idx_insert_kv_pairs(struct m0_uint128 id, int nr_kvp)
{
	int               i;
	int               rc = 0;
	int               start;
	int               nr_rounds;
	int               nr_per_round;
	int               nr_to_insert;
	struct m0_bufvec *keys;
	struct m0_bufvec *vals;
	int              *rcs;

	nr_rounds = 1;
	nr_per_round = nr_kvp / nr_rounds;
	for (i = 0; i < nr_rounds; i++) {
		start = i * nr_per_round;
		if (start + nr_per_round > nr_kvp)
			nr_to_insert = nr_kvp - start;
		else
			nr_to_insert = nr_per_round;

		/* Allocate bufvec's for keys and vals. */
		keys = idx_bufvec_alloc(nr_to_insert);
		vals = idx_bufvec_alloc(nr_to_insert);
		M0_ALLOC_ARR(rcs, nr_to_insert);
		if (keys == NULL || vals == NULL || rcs == NULL) {
			rc = -ENOMEM;
			goto ERROR;
		}

		/* Fill keys and values with some data. */
		rc = idx_fill_kv_pairs(id, start, keys, vals);
		if (rc < 0)
			goto ERROR;

		/* Do the real job. */
		rc = idx_do_insert_kv_pairs(id, keys, vals, rcs);
		if (rc < 0)
			goto ERROR;

		idx_bufvec_free(keys);
		idx_bufvec_free(vals);
		m0_free0(&rcs);
	}

	return rc;

ERROR:
	if (keys) idx_bufvec_free(keys);
	if (vals) idx_bufvec_free(vals);
	return rc;
}

static int idx_create_one(struct m0_uint128 id)
{
	int                     rc;
	struct m0_clovis_op    *ops[1] = {NULL};
	struct m0_clovis_idx    idx;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	/* Set an index creation operation. */
	clovis_st_idx_init(&idx,
		&clovis_st_idx_container.co_realm, &id);
	clovis_st_entity_create(NULL, &idx.in_entity, &ops[0]);

	/* Launch and wait for op to complete */
	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	if (rc < 0) return rc;

	rc = ops[0]->op_sm.sm_rc;
	if (rc < 0) return rc;
	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&idx.in_entity);

	return rc;
}

static int idx_delete_one(struct m0_uint128 id)
{
	int                     rc;
	struct m0_clovis_op    *ops[1] = {NULL};
	struct m0_clovis_idx    idx;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;


	/* Set an index creation operation. */
	clovis_st_idx_init(&idx, &clovis_st_idx_container.co_realm, &id);
	clovis_st_idx_open(&idx.in_entity);
	clovis_st_entity_delete(&idx.in_entity, &ops[0]);

	/* Launch and wait for op to complete */
	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	if (rc < 0) return rc;

	rc = ops[0]->op_sm.sm_rc;
	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&idx.in_entity);

	return rc;
}

static int idx_test_prepare(void)
{
	int               i;
	int               rc = 0;
	int               nr_kvp;
	struct m0_uint128 id;
	struct m0_fid     idx_fid;

	M0_CLOVIS_THREAD_ENTER;

	deleted_kv_pairs =
	    m0_alloc(ST_MAX_INDEX_NUM * ST_LARGE_KV_PAIR_NUM * sizeof(bool));
	if (deleted_kv_pairs == NULL)
		return -ENOMEM;

	for (i = 0; i < ST_MAX_INDEX_NUM; i++) {
		/* get index's fid. */
		clovis_oid_get(&id);
		/* Make mero KVS happy */
		idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
		id.u_hi = idx_fid.f_container;
		id.u_lo = idx_fid.f_key;

		/* Create an index. */
		rc = idx_create_one(id);
		if (rc != 0)
			break;

		/* Insert K-V pairs into index. */
		nr_kvp = get_max_nr_kv_pairs(i);
		rc = idx_insert_kv_pairs(id, nr_kvp);
		if (rc < 0)
			break;

		test_index_ids[i] = id;
	}

	return rc;
}

#define idx_query_exec(idx_id, opcode, keys, vals, rcs, flag, exp)   \
	do {                        \
		int                   rc;    \
		struct m0_clovis_idx  idx;    \
		struct m0_clovis_op  *ops[1] = {NULL};  \
	                                      \
		/* Launch DEL query. */       \
		memset(&idx, 0, sizeof idx); \
		ops[0] = NULL;                \
                                              \
		clovis_st_idx_init(&idx,      \
			&clovis_st_idx_container.co_realm, &idx_id); \
		clovis_st_idx_op(&idx, opcode, keys, vals, rcs, flag, &ops[0]); \
								     \
		clovis_st_op_launch(ops, 1);                         \
		rc = clovis_st_op_wait(ops[0],                       \
			    M0_BITS(M0_CLOVIS_OS_FAILED,             \
				    M0_CLOVIS_OS_STABLE),            \
			    M0_TIME_NEVER);			     \
		CLOVIS_ST_ASSERT_FATAL(rc == 0);                     \
		CLOVIS_ST_ASSERT_FATAL(                              \
			ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);\
		CLOVIS_ST_ASSERT_FATAL(ops[0]->op_rc exp 0);      \
								       \
		/* fini and release */                                 \
		clovis_st_op_fini(ops[0]);                             \
		clovis_st_op_free(ops[0]);                             \
		clovis_st_entity_fini(&idx.in_entity);                 \
	} while(0)

#define idx_query_exp_success(idx_id, opcode, keys, vals, rcs, flag)  \
	idx_query_exec(idx_id, opcode, keys, vals, rcs, flag, ==)

#define idx_query_exp_fail(idx_id, opcode, keys, vals, rcs, flag) \
	idx_query_exec(idx_id, opcode, keys, vals, rcs, flag, !=)

static void idx_query_get(void)
{
	int                   i;
	int                   rc;
	int                   idx_door_no;
	int                   nr_tests;
	int                   nr_kvp;
	struct m0_clovis_op  *ops[1] = {NULL};
	struct m0_clovis_idx  idx;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	struct m0_uint128     id;
	int                   j = 0;
	int                  *rcs;

	M0_CLOVIS_THREAD_ENTER;

	nr_kvp = 2;
	nr_tests = 1;
	for (i = 0; i < nr_tests; i++) {
		/* Allocate bufvec's for keys and vals. */
		keys = idx_bufvec_alloc(nr_kvp);
		vals = idx_bufvec_alloc(nr_kvp);
		M0_ALLOC_ARR(rcs, nr_kvp);
		CLOVIS_ST_ASSERT_FATAL(keys != NULL);
		CLOVIS_ST_ASSERT_FATAL(vals != NULL);
		CLOVIS_ST_ASSERT_FATAL(rcs  != NULL);

		/* Fill keys and values with some data. */
		idx_door_no = generate_random(ST_MAX_INDEX_NUM);
		id = test_index_ids[idx_door_no];
		rc = idx_pick_keys(idx_door_no, keys, NULL);
		CLOVIS_ST_ASSERT_FATAL(rc > 0);

		/* Launch GET query. */
		memset(&idx, 0, sizeof idx);
		ops[0] = NULL;

		clovis_st_idx_init(&idx, &clovis_st_idx_container.co_realm, &id);
		clovis_st_idx_op(&idx, M0_CLOVIS_IC_GET, keys, vals, rcs,
				 0, &ops[0]);

		clovis_st_op_launch(ops, 1);
		rc = clovis_st_op_wait(ops[0],
			    M0_BITS(M0_CLOVIS_OS_FAILED,
				    M0_CLOVIS_OS_STABLE),
			    M0_TIME_NEVER);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
		CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
		CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

		for(j = 0; j < nr_kvp; j++) {
			CLOVIS_ST_ASSERT_FATAL(rcs[j] == 0);
		}

		/* fini and release */
		clovis_st_op_fini(ops[0]);
		clovis_st_op_free(ops[0]);
		clovis_st_entity_fini(&idx.in_entity);

		idx_bufvec_free(keys);
		idx_bufvec_free(vals);
		m0_free0(&rcs);
	}
}

/**
 * Cancel launched operation.
 */
static void idx_query_get_cancel(void)
{
	int                   i;
	int                   rc;
	int                   idx_door_no;
	int                   nr_tests;
	int                   nr_kvp;
	struct m0_clovis_op  *ops[1] = {NULL};
	struct m0_clovis_idx  idx;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	struct m0_uint128     id;
	int                   j = 0;
	int                  *rcs;

	M0_CLOVIS_THREAD_ENTER;

	nr_kvp = 2;
	nr_tests = 1;
	for (i = 0; i < nr_tests; i++) {
		/* Allocate bufvec's for keys and vals. */
		keys = idx_bufvec_alloc(nr_kvp);
		vals = idx_bufvec_alloc(nr_kvp);
		M0_ALLOC_ARR(rcs, nr_kvp);
		CLOVIS_ST_ASSERT_FATAL(keys != NULL);
		CLOVIS_ST_ASSERT_FATAL(vals != NULL);
		CLOVIS_ST_ASSERT_FATAL(rcs  != NULL);

		/* Fill keys and values with some data. */
		idx_door_no = generate_random(ST_MAX_INDEX_NUM);
		id = test_index_ids[idx_door_no];
		rc = idx_pick_keys(idx_door_no, keys, NULL);
		CLOVIS_ST_ASSERT_FATAL(rc > 0);

		/* Launch GET query. */
		memset(&idx, 0, sizeof idx);
		ops[0] = NULL;

		clovis_st_idx_init(&idx, &clovis_st_idx_container.co_realm,
				   &id);
		clovis_st_idx_op(&idx, M0_CLOVIS_IC_GET, keys, vals, rcs,
				 0, &ops[0]);

		clovis_st_op_launch(ops, 1);
		rc = clovis_st_op_wait(ops[0],
				       M0_BITS(M0_CLOVIS_OS_FAILED,
				       M0_CLOVIS_OS_STABLE),
				       m0_time_from_now(0, 0));
		if (rc == -ETIMEDOUT) {
			m0_clovis_op_cancel(ops, 1);
			rc = clovis_st_op_wait(ops[0],
					       M0_BITS(M0_CLOVIS_OS_FAILED,
				               M0_CLOVIS_OS_STABLE),
					       M0_TIME_NEVER);
		}
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
		CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state ==
				       M0_CLOVIS_OS_STABLE);
		CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

		for(j = 0; j < nr_kvp; j++) {
			CLOVIS_ST_ASSERT_FATAL(rcs[j] == 0);
		}

		/* fini and release */
		clovis_st_op_fini(ops[0]);
		clovis_st_op_free(ops[0]);
		clovis_st_entity_fini(&idx.in_entity);

		idx_bufvec_free(keys);
		idx_bufvec_free(vals);
		m0_free0(&rcs);
	}
}


#if 0
static void idx_query_get_null(void)
{
	int                   i;
	int                   rc;
	int                   idx_door_no;
	int                   nr_tests;
	int                   nr_kvp;
	struct m0_clovis_op  *ops[1] = {NULL};
	struct m0_clovis_idx  idx;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	struct m0_uint128     id;

	M0_CLOVIS_THREAD_ENTER;

	nr_kvp = 1;
	nr_tests = 1;
	for (i = 0; i < nr_tests; i++) {
		/* Allocate bufvec's for keys and vals. */
		keys = idx_bufvec_alloc(nr_kvp);
		vals = idx_bufvec_alloc(nr_kvp);
		CLOVIS_ST_ASSERT_FATAL(keys != NULL);
		CLOVIS_ST_ASSERT_FATAL(vals != NULL);

		idx_door_no = generate_random(ST_MAX_INDEX_NUM);
		id = test_index_ids[idx_door_no];

		keys->ov_buf[0] = NULL;
		keys->ov_vec.v_count[0] = 0;

		/* Launch GET query. */
		memset(&idx, 0, sizeof idx);
		ops[0] = NULL;

		clovis_st_idx_init(&idx, &clovis_st_idx_container.co_realm, &id);
		clovis_st_idx_op(&idx, M0_CLOVIS_IC_GET, keys, vals, rcs, &ops[0]);

		clovis_st_op_launch(ops, 1);
		rc = clovis_st_op_wait(ops[0],
			    M0_BITS(M0_CLOVIS_OS_FAILED,
				    M0_CLOVIS_OS_STABLE),
			    M0_TIME_NEVER);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
		CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state != M0_CLOVIS_OS_STABLE);
		CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc != 0);

		/* fini and release */
		clovis_st_op_fini(ops[0]);
		clovis_st_op_free(ops[0]);
		clovis_st_entity_fini(&idx.in_entity);

		idx_bufvec_free(keys);
		idx_bufvec_free(vals);
	}
}
#endif

#ifndef __KERNEL__
static void idx_query_get_nonexist(void)
{
	int                   i;
	int                   rc;
	int                   idx_door_no;
	int                   nr_kvp;
	struct m0_clovis_op  *ops[1] = {NULL};
	struct m0_clovis_idx  idx;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	struct m0_uint128     id;
	int                  *rcs;

	M0_CLOVIS_THREAD_ENTER;

	/* Allocate bufvec's for keys and vals. */
	nr_kvp = 2;
	keys = idx_bufvec_alloc(nr_kvp);
	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	M0_ALLOC_ARR(rcs, nr_kvp);
	/* Fill keys and values with some data. */
	idx_door_no = generate_random(ST_MAX_INDEX_NUM);
	id = test_index_ids[idx_door_no];
	rc = idx_pick_keys(idx_door_no, keys, NULL);

	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		keys->ov_vec.v_count[i] = 10;
		memcpy((char *)(keys->ov_buf[i]), "nonexistkv", 10);
	}

	/* Launch GET query. */
	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	clovis_st_idx_init(&idx, &clovis_st_idx_container.co_realm, &id);
	clovis_st_idx_op(&idx, M0_CLOVIS_IC_GET, keys, vals, rcs, 0, &ops[0]);

	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_rc == 0);

	for(i = 0; i < nr_kvp; i++) {
		CLOVIS_ST_ASSERT_FATAL(rcs[i] != 0);
	}

	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&idx.in_entity);

	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);
}

static void idx_query_get_non_existing_index(void)
{
	int                   i;
	int                   rc;
	int                   idx_door_no;
	int                   nr_kvp;
	struct m0_clovis_op  *ops[1] = {NULL};
	struct m0_clovis_idx  idx;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	struct m0_uint128     id;
	struct m0_fid         idx_fid;
	int                  *rcs;

	M0_CLOVIS_THREAD_ENTER;

	clovis_oid_get(&id);
	/* Make mero kvs happy*/
	idx_fid = M0_FID_TINIT('i', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Allocate bufvec's for keys and vals. */
	nr_kvp = 2;
	keys = idx_bufvec_alloc(nr_kvp);
	vals = idx_bufvec_alloc(nr_kvp);
	M0_ALLOC_ARR(rcs, nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);
	CLOVIS_ST_ASSERT_FATAL(rcs  != NULL);

	/* Fill keys and values with some data. */
	idx_door_no = generate_random(ST_MAX_INDEX_NUM);
	idx_pick_keys(idx_door_no, keys, NULL);

	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		keys->ov_vec.v_count[i] = 10;
		memcpy((char *)(keys->ov_buf[i]), "nonexistkv", 10);
	}

	/* Launch GET query. */
	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	clovis_st_idx_init(&idx, &clovis_st_idx_container.co_realm, &id);
	clovis_st_idx_op(&idx, M0_CLOVIS_IC_GET, keys, vals, rcs, 0, &ops[0]);

	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	CLOVIS_ST_ASSERT_FATAL(ops[0]->op_rc != 0);

	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&idx.in_entity);

	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);
}

#endif

static void idx_query_del(void)
{
	int                i;
	int                j;
	int                rc;
	int                idx_door_no;
	int                nr_tests;
	int                nr_kvp;
	int               *key_no_arr;
	int                key_no;
	struct m0_bufvec  *keys;
	struct m0_uint128  id;
	int               *rcs;

	M0_CLOVIS_THREAD_ENTER;

	nr_kvp = 2;
	nr_tests = 1;

	key_no_arr = m0_alloc(nr_kvp * sizeof(*key_no_arr));
	if (key_no_arr == NULL)
		return;

	M0_ALLOC_ARR(rcs, nr_kvp);
	for (i = 0; i < nr_tests; i++) {
		/* Allocate bufvec's for keys and vals. */
		keys = idx_bufvec_alloc(nr_kvp);
		CLOVIS_ST_ASSERT_FATAL(keys != NULL);

		/* Pick those keys to be deleted. */
		idx_door_no = generate_random(ST_MAX_INDEX_NUM);
		id = test_index_ids[idx_door_no];
		rc = idx_pick_keys(idx_door_no, keys, key_no_arr);
		if (rc < 0)
			goto BUFVEC_FREE;

		/* Launch DEL query. */
		idx_query_exp_success(id, M0_CLOVIS_IC_DEL, keys, NULL, rcs, 0);

		for (j = 0; j < nr_kvp; j++) {
			key_no = idx_door_no * ST_LARGE_KV_PAIR_NUM +
				 key_no_arr[j];
			deleted_kv_pairs[key_no] = true;
		}
BUFVEC_FREE:
		idx_bufvec_free(keys);
	}
	m0_free(key_no_arr);
}

static void idx_query_next(void)
{
	int                i;
	int                idx_door_no;
	int                nr_rounds = 1;
	int                nr_kvp;
	int                last_key_len = 0;
	void              *last_key = NULL;
	struct m0_bufvec  *keys;
	struct m0_bufvec  *vals;
	struct m0_uint128  id;
	int               *rcs;

	M0_CLOVIS_THREAD_ENTER;

	for (i = 0; i < nr_rounds; i++) {
		/* Allocate bufvec's for keys and vals. */
		nr_kvp = 2;  /* or a random number. */
		keys = idx_bufvec_alloc(nr_kvp);
		CLOVIS_ST_ASSERT_FATAL(keys != NULL);

		vals = idx_bufvec_alloc(nr_kvp);
		CLOVIS_ST_ASSERT_FATAL(vals != NULL);

		M0_ALLOC_ARR(rcs, nr_kvp);
		/* Launch NEXT query. */
		idx_door_no = generate_random(ST_MAX_INDEX_NUM);
		id = test_index_ids[idx_door_no];
		keys->ov_buf[0] = last_key;
		keys->ov_vec.v_count[0] = last_key_len;
		idx_query_exp_success(id, M0_CLOVIS_IC_NEXT, keys, vals, rcs, 0);

		/* Extract the last key for next round. */
		if (i == nr_rounds - 1)
			goto FREE_BUFVEC;

		if (keys->ov_buf[nr_kvp - 1] == NULL)
			break;

		if (last_key != NULL)
			m0_free(last_key);
		last_key_len = keys->ov_vec.v_count[nr_kvp - 1];
		last_key = m0_alloc(last_key_len);
		memcpy(last_key, keys->ov_buf[nr_kvp - 1], last_key_len);

FREE_BUFVEC:
		idx_bufvec_free(keys);
		idx_bufvec_free(vals);
		m0_free0(&rcs);
	}
}

static void idx_query_next_exclude_start(void)
{
	int                idx_door_no;
	int                nr_kvp;
	int                last_key_len = 0;
	int                stored_key_len = 0;
	void              *last_key = NULL;
	void              *stored_key = NULL;
	struct m0_bufvec  *keys;
	struct m0_bufvec  *vals;
	struct m0_uint128  id;
	int               *rcs;

	M0_CLOVIS_THREAD_ENTER;

	/** PHASE 1: Fetch nr_kvp KV. */
	nr_kvp = 2;
	keys = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);

	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	M0_ALLOC_ARR(rcs, nr_kvp);
	/* Launch NEXT query. */
	idx_door_no = generate_random(ST_MAX_INDEX_NUM);
	id = test_index_ids[idx_door_no];
	keys->ov_buf[0] = last_key;
	keys->ov_vec.v_count[0] = last_key_len;
	idx_query_exp_success(id, M0_CLOVIS_IC_NEXT, keys, vals, rcs,
			      M0_OIF_EXCLUDE_START_KEY);

	CLOVIS_ST_ASSERT_FATAL(keys->ov_buf[nr_kvp - 1] != NULL)

	/** Use last key as the starting point of NEXT Op. */
	last_key_len = keys->ov_vec.v_count[nr_kvp - 1];
	last_key = m0_alloc(last_key_len);

	/** Store the last key for compariosn. */
	stored_key_len = last_key_len;
	stored_key = m0_alloc(last_key_len);
	memcpy(last_key, keys->ov_buf[nr_kvp - 1], last_key_len);
	memcpy(stored_key, keys->ov_buf[nr_kvp - 1], last_key_len);

	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);

	/** PHASE 2: Fetch next nr_kvp KV. */
	nr_kvp = 2;
	keys = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);

	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	M0_ALLOC_ARR(rcs, nr_kvp);
	/* Launch NEXT query. */
	idx_door_no = generate_random(ST_MAX_INDEX_NUM);
	id = test_index_ids[idx_door_no];
	keys->ov_buf[0] = last_key;
	keys->ov_vec.v_count[0] = last_key_len;
	idx_query_exp_success(id, M0_CLOVIS_IC_NEXT, keys, vals, rcs,
			      M0_OIF_EXCLUDE_START_KEY);

	CLOVIS_ST_ASSERT_FATAL(keys->ov_buf[nr_kvp - 1] != NULL)

	/** Result should not contain a stored key. */
	if(keys->ov_vec.v_count[0] == stored_key_len) {
		CLOVIS_ST_ASSERT_FATAL(memcmp(stored_key,
				       keys->ov_buf[0], stored_key_len) != 0);
	}

	m0_free(stored_key);
	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);

}

static void mock_op_cb_stable(struct m0_clovis_op *op)
{
	int *val;

	val = (int *)op->op_datum;
	*val = 'S';
}

static void mock_op_cb_failed(struct m0_clovis_op *op)
{
	int *val;

	val = (int *)op->op_datum;
	*val = 'F';
}


/* Setup callbacks instead of using m0_clovis_op_wait. */
static void idx_query_callbacks(void)
{
	int                     rc;
	int                     val = 0;
	struct m0_uint128       id;
	struct m0_fid           idx_fid;
	struct m0_clovis_op    *ops[1] = {NULL};
	struct m0_clovis_idx    idx;
	struct m0_clovis_op_ops cbs;

	/* get index's fid. */
	clovis_oid_get(&id);
	/* Make mero DIX happy. */
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/*
	 * Set callbacks for operations.
	 * ocb_executed is not supported (see comments in clovis/clovis.h)
	 */
	cbs.oop_executed = NULL;
	cbs.oop_stable = mock_op_cb_stable;
	cbs.oop_failed = mock_op_cb_failed;

	/* Set an index creation operation. */
	ops[0] = NULL;
	memset(&idx, 0, sizeof idx);

	clovis_st_idx_init(&idx,
		&clovis_st_idx_container.co_realm, &id);
	clovis_st_entity_create(NULL, &idx.in_entity, &ops[0]);

	/* Launch and wait for op to complete */
	ops[0]->op_datum = (void *)&val;
	m0_clovis_op_setup(ops[0], &cbs, 0);
	clovis_st_op_launch(ops, 1);

	/* Test callback functions for OS_STABLE*/
	rc = clovis_st_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	if (rc == 0) {
		if (ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE)
			CLOVIS_ST_ASSERT_FATAL(val == 'S')
		else if (ops[0]->op_sm.sm_state == M0_CLOVIS_OS_FAILED)
			CLOVIS_ST_ASSERT_FATAL(val == 'F')
	}

	idx_delete_one(id);
	/* fini and release */
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
	clovis_st_entity_fini(&idx.in_entity);
}

static void idx_query_empty_next(void)
{
	int rc = 0;
	struct m0_uint128     id;
	struct m0_fid         idx_fid;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	int                  *rcs;
	int nr_kvp = 1;  /* or a random number. */

	clovis_oid_get(&id);
	/* Make mero kvs happy*/
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_one(id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Allocate bufvec's for keys and vals. */
	keys = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);

	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	/* Launch NEXT query. */
	keys->ov_buf[0] = NULL;
	keys->ov_vec.v_count[0] = 0;

	M0_ALLOC_ARR(rcs, nr_kvp);
	idx_query_exp_success(id, M0_CLOVIS_IC_NEXT, keys, vals, rcs, 0);
	CLOVIS_ST_ASSERT_FATAL(keys->ov_vec.v_nr == nr_kvp && keys->ov_buf[0] == NULL);

	rc = idx_delete_one(id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* fini and release */
	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);
}

static void idx_query_next_not_exist_index(void)
{
	struct m0_uint128     id;
	struct m0_fid         idx_fid;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	int                  *rcs;
	int nr_kvp = 1;  /* or a random number. */

	clovis_oid_get(&id);
	/* Make mero kvs happy*/
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Allocate bufvec's for keys and vals. */
	keys = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);

	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	M0_ALLOC_ARR(rcs, nr_kvp);
	/* Launch NEXT query. */
	keys->ov_buf[0] = NULL;
	keys->ov_vec.v_count[0] = 0;

	idx_query_exp_fail(id, M0_CLOVIS_IC_NEXT, keys, vals, rcs, 0);
	CLOVIS_ST_ASSERT_FATAL(keys->ov_vec.v_nr == nr_kvp && keys->ov_buf[0] == NULL);

	/* fini and release */
	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);

}

static void idx_query_drop_index(void)
{
	int rc = 0;
	struct m0_uint128       id;
	struct m0_fid     	idx_fid;
	struct m0_bufvec        *keys;
	struct m0_bufvec        *vals;
	int                     *rcs;
	int nr_kvp = 1;  /* or a random number. */

	clovis_oid_get(&id);
	/* Make mero kvs happy*/
	idx_fid = M0_FID_TINIT('x', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_one(id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Allocate bufvec's for keys and vals. */
	keys = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);

	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	M0_ALLOC_ARR(rcs, nr_kvp);
	idx_fill_kv_pairs(id, 0, keys, vals);

	rc = idx_do_insert_kv_pairs(id, keys, vals, rcs);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	rc = idx_delete_one(id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Launch NEXT query. */
	keys->ov_buf[0] = NULL;
	keys->ov_vec.v_count[0] = 0;

	idx_query_exp_fail(id, M0_CLOVIS_IC_NEXT, keys, vals, rcs, 0);
	CLOVIS_ST_ASSERT_FATAL(keys->ov_vec.v_nr == nr_kvp && keys->ov_buf[0] == NULL);

	/* fini and release */
	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);
}

/* This test case is inconsistent.
 * Cassandra and KVS behaviour in this case different.
 */
#if 0
static void idx_query_put_duplicate(void)
{
	int rc = 0;
	struct m0_uint128       id;
	struct m0_fid     idx_fid;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;

	int nr_kvp = 1;  /* or a random number. */

	clovis_oid_get(&id);
	/* Make mero kvs happy*/
	idx_fid = M0_FID_TINIT('i', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Create an index. */
	rc = idx_create_one(id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Allocate bufvec's for keys and vals. */
	keys = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);

	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	idx_fill_kv_pairs(id, 0, keys, vals);

	rc = idx_do_insert_kv_pairs(id, keys, vals);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);

	/* Try to insert the same kv pair again */
	rc = idx_do_insert_kv_pairs(id, keys, vals);
	CLOVIS_ST_ASSERT_FATAL(rc != 0);

	rc = idx_delete_one(id);
	CLOVIS_ST_ASSERT_FATAL(rc == 0);
	/* fini and release */
	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
}
#endif

static void idx_query_put_not_existent_index(void)
{
	int rc = 0;
	struct m0_uint128       id;
	struct m0_fid     idx_fid;
	struct m0_bufvec     *keys;
	struct m0_bufvec     *vals;
	int                  *rcs;

	int nr_kvp = 1;  /* or a random number. */

	clovis_oid_get(&id);
	/* Make mero kvs happy*/
	idx_fid = M0_FID_TINIT('i', id.u_hi, id.u_lo);
	id.u_hi = idx_fid.f_container;
	id.u_lo = idx_fid.f_key;

	/* Allocate bufvec's for keys and vals. */
	keys = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(keys != NULL);

	vals = idx_bufvec_alloc(nr_kvp);
	CLOVIS_ST_ASSERT_FATAL(vals != NULL);

	M0_ALLOC_ARR(rcs, nr_kvp);
	idx_fill_kv_pairs(id, 0, keys, vals);

	rc = idx_do_insert_kv_pairs(id, keys, vals, rcs);
	CLOVIS_ST_ASSERT_FATAL(rc != 0);

	/* fini and release */
	idx_bufvec_free(keys);
	idx_bufvec_free(vals);
	m0_free0(&rcs);
}

/**
 * Initialises the index suite's environment.
 */
static int clovis_st_idx_suite_init(void)
{
	int rc = 0;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	clovis_st_container_init(&clovis_st_idx_container,
			      NULL, &M0_CLOVIS_UBER_REALM,
			      clovis_st_get_instance());
	rc = clovis_st_idx_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0) {
		console_printf("Failed to open uber realm\n");
		goto EXIT;
	}

	/* Create and populate the indices for query tests. */
	rc = idx_test_prepare();

EXIT:
	return rc;
}

/**
 * Finalises the idx suite's environment.
 */
static int clovis_st_idx_suite_fini(void)
{
	return 0;
}

struct clovis_st_suite st_suite_clovis_idx = {
	.ss_name = "clovis_idx_st",
	.ss_init = clovis_st_idx_suite_init,
	.ss_fini = clovis_st_idx_suite_fini,
	.ss_tests = {
		{ "idx_query_get",
		  &idx_query_get },
		{ "idx_query_get_cancel",
		  &idx_query_get_cancel },
#if 0
		{ "idx_query_get_null",
		  &idx_query_get_null },
		{ "idx_query_put_duplicate",
		  &idx_query_put_duplicate },
#endif
		{ "idx_query_put_not_existent_index",
		  &idx_query_put_not_existent_index },
#ifndef __KERNEL__
		{ "idx_query_get_nonexist",
		  &idx_query_get_nonexist},
		{ "idx_query_get_non_existing_index",
		  &idx_query_get_non_existing_index },
#endif
		{ "idx_query_del",
		  &idx_query_del },
		{ "idx_query_next",
		  &idx_query_next },
		{ "idx_query_next_exclude_start",
		  &idx_query_next_exclude_start },
		{ "idx_query_empty_next",
		  &idx_query_empty_next },
		{ "idx_query_next_not_exist_index",
		  &idx_query_next_not_exist_index },
		{ "idx_query_drop_index",
		  &idx_query_drop_index },
		{ "idx_query_callbacks",
		  &idx_query_callbacks},
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
