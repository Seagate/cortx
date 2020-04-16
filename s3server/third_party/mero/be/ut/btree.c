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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 12-Jun-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/tx_group_fom.h"
#include "be/btree.h"
#include "lib/types.h"     /* m0_uint128_eq */
#include "lib/misc.h"      /* M0_BITS, M0_IN */
#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "lib/errno.h"     /* ENOENT */
#include "be/ut/helper.h"
#include "ut/ut.h"
#ifndef __KERNEL__
#include <stdio.h>	   /* sscanf */
#endif

static struct m0_be_ut_backend ut_be;
static struct m0_be_ut_seg     ut_seg;
static struct m0_be_seg       *seg;

extern void btree_dbg_print(struct m0_be_btree *tree);

static int tree_cmp(const void *key0, const void *key1)
{
	return strcmp(key0, key1);
}

static m0_bcount_t tree_kv_size(const void *kv)
{
	return kv != NULL ? strlen(kv) + 1 : 0;
}

static const struct m0_be_btree_kv_ops kv_ops = {
	.ko_ksize   = tree_kv_size,
	.ko_vsize   = tree_kv_size,
	.ko_compare = tree_cmp
};

enum {
	INSERT_COUNT = BTREE_FAN_OUT * 20,
	INSERT_KSIZE  = 7,
	INSERT_VSIZE  = 11,
	TXN_OPS_NR = 7,
};

static void check(struct m0_be_btree *tree);

static struct m0_be_btree *create_tree(void);

static void destroy_tree(struct m0_be_btree *tree);

void m0_be_ut_btree_simple(void)
{
	struct m0_be_btree *tree0;

	M0_ENTRY();
	/* Init BE */
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1ULL << 24);
	seg = ut_seg.bus_seg;

	/* create btrees */
	tree0 = create_tree();

	m0_be_ut_seg_reload(&ut_seg);

	check(tree0);
	destroy_tree(tree0);

	m0_be_ut_seg_reload(&ut_seg);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);

	M0_LEAVE();
}

static int
btree_insert(struct m0_be_btree *t, struct m0_buf *k, struct m0_buf *v,
	     int nr_left)
{
	struct m0_be_tx_credit  cred = {};
	static struct m0_be_tx *tx = NULL;
	static int              nr;
	struct m0_be_op         op = {};
	int                     rc;

	M0_ENTRY();

	if (tx == NULL) {
		nr = TXN_OPS_NR;
		M0_ALLOC_PTR(tx);
		M0_ASSERT(tx != NULL);
		m0_be_btree_insert_credit2(t, nr, INSERT_KSIZE   + 1,
					          INSERT_VSIZE*2 + 1, &cred);
		m0_be_ut_tx_init(tx, &ut_be);
		m0_be_tx_prep(tx, &cred);

		rc = m0_be_tx_open_sync(tx);
		M0_UT_ASSERT(rc == 0);
	}

	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_insert(t, tx, &op, k, v),
				    bo_u.u_btree.t_rc);

	if (--nr == 0 || nr_left == 0) {
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_free(tx);
		tx = NULL;
	}

	return M0_RC(rc);
}

static int
btree_insert_inplace(struct m0_be_btree *t, struct m0_buf *k, int v,
		     int nr_left)
{
	struct m0_be_tx_credit     cred = {};
	static struct m0_be_tx    *tx = NULL;
	static int                 nr;
	struct m0_be_op            op = {};
	struct m0_be_btree_anchor  anchor;
	int                        rc;

	M0_ENTRY();

	if (tx == NULL) {
		nr = TXN_OPS_NR;
		M0_ALLOC_PTR(tx);
		M0_UT_ASSERT(tx != NULL);
		m0_be_btree_insert_credit2(t, nr, INSERT_KSIZE + 1,
					          INSERT_VSIZE*2 + 1, &cred);
		m0_be_ut_tx_init(tx, &ut_be);
		m0_be_tx_prep(tx, &cred);

		rc = m0_be_tx_open_sync(tx);
		M0_UT_ASSERT(rc == 0);
	}

	if ((v & 1) == 0)
		anchor.ba_value.b_nob = INSERT_VSIZE;
	else
		anchor.ba_value.b_nob = INSERT_VSIZE*2;
	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_insert_inplace(t, tx, &op,
				    k, &anchor, M0_BITS(M0_BAP_NORMAL)),
				    bo_u.u_btree.t_rc);
	/* update value */
	if ((v & 1) == 0)
		sprintf(anchor.ba_value.b_addr, "%0*d", INSERT_VSIZE - 1, v);
	else
		sprintf(anchor.ba_value.b_addr, "%0*d", INSERT_VSIZE*2 - 1, v);
	m0_be_btree_release(tx, &anchor);

	if (--nr == 0 || nr_left == 0) {
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_free(tx);
		tx = NULL;
	}

	return M0_RC(rc);
}

static int
btree_delete(struct m0_be_btree *t, struct m0_buf *k, int nr_left)
{
	struct m0_be_tx_credit  cred = {};
	static struct m0_be_tx *tx = NULL;
	static int              nr;
	struct m0_be_op         op = {};
	int                     rc;

	M0_ENTRY();

	if (tx == NULL) {
		nr = TXN_OPS_NR;
		M0_ALLOC_PTR(tx);
		M0_UT_ASSERT(tx != NULL);
		m0_be_btree_delete_credit(t, nr, INSERT_KSIZE+1,
					         INSERT_VSIZE*2 + 1, &cred);
		m0_be_ut_tx_init(tx, &ut_be);
		m0_be_tx_prep(tx, &cred);

		rc = m0_be_tx_open_sync(tx);
		M0_UT_ASSERT(rc == 0);
	}

	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_delete(t, tx, &op, k),
				    bo_u.u_btree.t_rc);

	if (--nr == 0 || nr_left == 0) {
		m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_free(tx);
		tx = NULL;
	}

	return M0_RC(rc);
}

static void shuffle_array(int a[], size_t n)
{
	if (n > 1) {
		int      i;
		uint64_t seed;

		seed = 123;
		for (i = 0; i < n - 1; ++i)
			M0_SWAP(a[i], a[m0_rnd(n, &seed)]);
	}
}

static void btree_delete_test(struct m0_be_btree *tree, struct m0_be_tx *tx)
{
	struct m0_buf key;
	struct m0_buf val;
	char          k[INSERT_KSIZE];
	char          v[INSERT_VSIZE*2];
	int          *rand_keys;
	int           rc;
	int           i;

	m0_buf_init(&key, k, INSERT_KSIZE);
	m0_buf_init(&val, v, INSERT_VSIZE);

	M0_LOG(M0_INFO, "Check error code...");
	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT);

	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == -ENOENT);

	btree_dbg_print(tree);

	M0_LOG(M0_INFO, "Delete random keys...");
	M0_ALLOC_ARR(rand_keys, INSERT_COUNT);
	M0_UT_ASSERT(rand_keys != NULL);
	for (i = 0; i < INSERT_COUNT; ++i)
		rand_keys[i] = i;
	shuffle_array(rand_keys, INSERT_COUNT);
	for (i = 0; i < INSERT_COUNT; i+=2) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		M0_LOG(M0_DEBUG, "%04d: delete key=%s", i, (char*)k);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 2);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Make sure nothing deleted is left...");
	for (i = 0; i < INSERT_COUNT; i+=2) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 2);
		M0_UT_ASSERT(rc == -ENOENT);
	}

	M0_LOG(M0_INFO, "Insert back all deleted stuff...");
	for (i = 0; i < INSERT_COUNT; i+=2) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		if ((rand_keys[i] & 1) == 0) {
			m0_buf_init(&val, v, INSERT_VSIZE);
			sprintf(v, "%0*d", INSERT_VSIZE-1, rand_keys[i]);
		} else {
			m0_buf_init(&val, v, INSERT_VSIZE*2);
			sprintf(v, "%0*d", INSERT_VSIZE*2 - 1, rand_keys[i]);
		}
		rc = btree_insert(tree, &key, &val, INSERT_COUNT - i - 2);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Delete everything in random order...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		M0_LOG(M0_DEBUG, "%04d: delete key=%s", i, (char*)k);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 1);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Make sure nothing is left...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		rc = btree_delete(tree, &key, INSERT_COUNT - i - 1);
		M0_UT_ASSERT(rc == -ENOENT);
	}

	M0_LOG(M0_INFO, "Insert everything back...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, rand_keys[i]);
		if ((rand_keys[i] & 1) == 0) {
			m0_buf_init(&val, v, INSERT_VSIZE);
			sprintf(v, "%0*d", INSERT_VSIZE-1, rand_keys[i]);
		} else {
			m0_buf_init(&val, v, INSERT_VSIZE*2);
			sprintf(v, "%0*d", INSERT_VSIZE*2 - 1, rand_keys[i]);
		}
		rc = btree_insert(tree, &key, &val, INSERT_COUNT - i - 1);
		M0_UT_ASSERT(rc == 0);
	}
	m0_free(rand_keys);

	M0_LOG(M0_INFO, "Deleting [%04d, %04d)...", INSERT_COUNT/4,
						    INSERT_COUNT*3/4);
	for (i = INSERT_COUNT/4; i < INSERT_COUNT*3/4; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_LOG(M0_DEBUG, "delete key=%04d", i);
		rc = btree_delete(tree, &key, INSERT_COUNT*3/4 - i - 1);
		M0_UT_ASSERT(rc == 0);
	}

	M0_LOG(M0_INFO, "Check double delete in-the-middle...");
	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT/5 & ~1);
	M0_LOG(M0_DEBUG, "delete key=%s", (char*)k);
	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == 0);
	M0_LOG(M0_DEBUG, "delete key=%s", (char*)k);
	rc = btree_delete(tree, &key, 0);
	M0_UT_ASSERT(rc == -ENOENT);
	M0_LOG(M0_INFO, "Insert it back.");
	m0_buf_init(&val, v, INSERT_VSIZE);
	sprintf(v, "%0*d", INSERT_VSIZE-1, INSERT_COUNT/5 & ~1);
	btree_insert(tree, &key, &val, 0);
}

static int btree_save(struct m0_be_btree *tree, struct m0_buf *k,
		      struct m0_buf *v, bool overwrite)
{
	struct m0_be_tx        *tx;
	struct m0_be_tx_credit  cred = {};
	struct m0_be_op         op = {};
	int                     rc;

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	m0_be_btree_insert_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE, &cred);
	if (overwrite)
		m0_be_btree_delete_credit(tree, 1, INSERT_KSIZE, INSERT_VSIZE,
					  &cred);
	m0_be_ut_tx_init(tx, &ut_be);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);
	rc = M0_BE_OP_SYNC_RET_WITH(&op,
			m0_be_btree_save(tree, tx, &op, k, v, overwrite),
			bo_u.u_btree.t_rc);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
	m0_free(tx);
	return rc;
}

static void btree_save_test(struct m0_be_btree *tree)
{
	struct m0_buf    key;
	struct m0_buf    val;
	struct m0_buf    ret_val;
	struct m0_be_op *op;
	char             k[INSERT_KSIZE];
	char             v[INSERT_VSIZE];
	char             r[INSERT_VSIZE];
	int              rc;

	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);

	m0_buf_init(&key, k, sizeof k);
	m0_buf_init(&val, v, sizeof v);
	m0_buf_init(&ret_val, r, sizeof r);

	/* Hope that a0a0 is not in the tree. */
	sprintf(k, "%0*x", INSERT_KSIZE-1, 0xa0a0);
	sprintf(v, "%0*x", INSERT_VSIZE-1, 0xa0a0);

	/* Check that key is not already inserted. */
	rc = M0_BE_OP_SYNC_RET_WITH(
		op, m0_be_btree_lookup(tree, op, &key, &ret_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT);

	M0_LOG(M0_INFO, "Save new key...");
	rc = btree_save(tree, &key, &val, false);
	M0_UT_ASSERT(rc == 0);
	btree_dbg_print(tree);
	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(
		op, m0_be_btree_lookup(tree, op, &key, &ret_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(ret_val.b_addr, v) == 0);

	M0_LOG(M0_INFO, "Save existing key without overwrite flag...");
	rc = btree_save(tree, &key, &val, false);
	M0_UT_ASSERT(rc == -EEXIST);

	M0_LOG(M0_INFO, "Save existing key with overwrite flag...");
	sprintf(v, "%0*x", INSERT_VSIZE-1, 0xb0b0);
	rc = btree_save(tree, &key, &val, true);
	M0_UT_ASSERT(rc == 0);
	btree_dbg_print(tree);
	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(
		op, m0_be_btree_lookup(tree, op, &key, &ret_val),
		bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(ret_val.b_addr, v) == 0);

	M0_LOG(M0_INFO, "Cleanup the key...");
	btree_delete(tree, &key, 0);
}

static struct m0_be_btree *create_tree(void)
{
	struct m0_be_tx_credit	cred = {};
	struct m0_be_btree     *tree;
	struct m0_be_tx        *tx;
	struct m0_buf           key;
	struct m0_buf           val;
	char                    k[INSERT_KSIZE];
	char                    v[INSERT_VSIZE * 2];
	char                    v2[INSERT_VSIZE * 3];
	struct m0_be_op         op = {};
	int                     rc;
	int                     i;

	M0_ENTRY();

	{ /* XXX: should calculate these credits not for dummy tree,
	   but for allocated below. This needs at least two transactions. */
		struct m0_be_btree t = { .bb_seg = seg };
		m0_be_btree_create_credit(&t, 1, &cred);
	}
	M0_BE_ALLOC_CREDIT_PTR(tree, seg, &cred);

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	m0_be_ut_tx_init(tx, &ut_be);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	/* start */
	M0_BE_ALLOC_PTR_SYNC(tree, seg, tx);
	m0_be_btree_init(tree, seg, &kv_ops);

	M0_BE_OP_SYNC_WITH(&op, m0_be_btree_create(tree, tx, &op));
	M0_UT_ASSERT(m0_be_btree_is_empty(tree));
	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);

	M0_SET0(&op);
	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_minkey(tree, &op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT && key.b_addr == NULL && key.b_nob == 0);

	M0_SET0(&op);
	rc = M0_BE_OP_SYNC_RET_WITH(&op, m0_be_btree_maxkey(tree, &op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == -ENOENT && key.b_addr == NULL && key.b_nob == 0);

	m0_buf_init(&key, k, INSERT_KSIZE);
	M0_LOG(M0_INFO, "Inserting...");
	/* insert */
	for (i = 0; i < INSERT_COUNT/2; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		if ((i & 1) == 0) {
			m0_buf_init(&val, v, INSERT_VSIZE);
			sprintf(v, "%0*d", INSERT_VSIZE - 1, i);
		} else {
			m0_buf_init(&val, v, INSERT_VSIZE*2);
			sprintf(v, "%0*d", INSERT_VSIZE*2 - 1, i);
		}
		btree_insert(tree, &key, &val, INSERT_COUNT/2 - i - 1);
	}
	M0_UT_ASSERT(!m0_be_btree_is_empty(tree));

	M0_LOG(M0_INFO, "Inserting inplace...");
	/* insert inplace */
	for (i = INSERT_COUNT/2; i < INSERT_COUNT; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		btree_insert_inplace(tree, &key, i, INSERT_COUNT - i - 1);
	}
	btree_dbg_print(tree);

	btree_delete_test(tree, tx);
	btree_save_test(tree);
	M0_LOG(M0_INFO, "Updating...");
	m0_be_ut_tx_init(tx, &ut_be);
	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_btree_update_credit(tree, 1, INSERT_VSIZE, &cred);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT - 1);
	sprintf(v, "XYZ");
	val.b_nob = 4;
	M0_SET0(&op);
	M0_BE_OP_SYNC_WITH(&op, m0_be_btree_update(tree, tx, &op, &key, &val));

	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);

	btree_dbg_print(tree);

	M0_LOG(M0_INFO, "Updating with longer value...");
	m0_be_ut_tx_init(tx, &ut_be);
	cred = M0_BE_TX_CREDIT(0, 0);
	m0_be_btree_update_credit2(tree, 1, INSERT_KSIZE, INSERT_VSIZE * 3,
				   &cred);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT - 2);
	snprintf(v2, sizeof v2, "%s", "ABCDEFGHI");
	m0_buf_init(&val, v2, strlen(v2)+1);
	M0_SET0(&op);
	M0_BE_OP_SYNC_WITH(&op, m0_be_btree_update(tree, tx, &op, &key, &val));

	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);
	m0_free(tx);

	btree_dbg_print(tree);

	M0_LEAVE();
	return tree;
}

static void destroy_tree(struct m0_be_btree *tree)
{
	struct m0_be_tx_credit	cred = {};
	struct m0_be_tx        *tx;
	struct m0_be_op        *op;
	struct m0_buf           key;
	char                    k[INSERT_KSIZE];
	int                     rc;
	int                     i;

	M0_ENTRY();

	m0_buf_init(&key, k, sizeof k);

	M0_LOG(M0_INFO, "Delete everything...");
	for (i = 0; i < INSERT_COUNT; i++) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		btree_delete(tree, &key, INSERT_COUNT - i - 1);
	}

	m0_be_btree_destroy_credit(tree, &cred);
	M0_BE_FREE_CREDIT_PTR(tree, seg, &cred);

	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);
	m0_be_ut_tx_init(tx, &ut_be);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	M0_LOG(M0_INFO, "Btree %p destroy...", tree);
	M0_BE_OP_SYNC_WITH(op, m0_be_btree_destroy(tree, tx, op));
	btree_dbg_print(tree);
	M0_BE_FREE_PTR_SYNC(tree, seg, tx);
	m0_be_tx_close_sync(tx); /* Make things persistent. */
	m0_be_tx_fini(tx);
	m0_free(op);
	m0_free(tx);
	M0_LEAVE();
}

static void cursor_test(struct m0_be_btree *tree)
{
	/* the structure is too large for kernel stack to be local */
	static struct m0_be_btree_cursor *cursor;
	struct m0_buf                    key;
	struct m0_buf                    val;
	char                             sbuf[INSERT_KSIZE+INSERT_VSIZE];
	struct m0_buf                    start = M0_BUF_INIT(sizeof sbuf, sbuf);
	int                              v;
	int                              i;
	int                              rc;

	M0_ALLOC_PTR(cursor);
	M0_UT_ASSERT(cursor != NULL);

	m0_be_btree_cursor_init(cursor, tree);

	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT/2);
	rc = m0_be_btree_cursor_get_sync(cursor, &start, true);
	M0_UT_ASSERT(rc == 0);

	m0_be_btree_cursor_kv_get(cursor, &key, &val);

	for (i = 0; i < INSERT_COUNT/4; ++i) {
		sscanf(key.b_addr, "%d", &v);
		M0_LOG(M0_DEBUG, "i=%i k=%d", i, v);
		M0_UT_ASSERT(v == i + INSERT_COUNT*3/4);

		M0_SET0(&cursor->bc_op);
		m0_be_op_init(&cursor->bc_op);
		m0_be_btree_cursor_next(cursor);
		m0_be_op_wait(&cursor->bc_op);
		rc = cursor->bc_op.bo_u.u_btree.t_rc;
		m0_be_op_fini(&cursor->bc_op);
		if (i < INSERT_COUNT/4 - 1)
			M0_UT_ASSERT(rc == 0);

		m0_be_btree_cursor_kv_get(cursor, &key, &val);
	}

	M0_UT_ASSERT(key.b_addr == NULL);
	M0_UT_ASSERT(rc == -ENOENT);

	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT/4 - 1);
	rc = m0_be_btree_cursor_get_sync(cursor, &start, false);
	M0_UT_ASSERT(rc == 0);

	m0_be_btree_cursor_kv_get(cursor, &key, &val);

	for (i = INSERT_COUNT/4 - 1; i >= 0; --i) {
		sscanf(key.b_addr, "%d", &v);
		M0_LOG(M0_DEBUG, "i=%i k=%d", i, v);
		M0_UT_ASSERT(v == i);

		M0_SET0(&cursor->bc_op);
		m0_be_op_init(&cursor->bc_op);
		m0_be_btree_cursor_prev(cursor);
		m0_be_op_wait(&cursor->bc_op);
		rc = cursor->bc_op.bo_u.u_btree.t_rc;
		m0_be_op_fini(&cursor->bc_op);
		if (i > 0)
			M0_UT_ASSERT(rc == 0);

		m0_be_btree_cursor_kv_get(cursor, &key, &val);
	}

	M0_UT_ASSERT(key.b_addr == NULL);
	M0_UT_ASSERT(rc == -ENOENT);

	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT);
	/* just to avoid CppCheck warning about changing unused sbuf below */
	start = M0_BUF_INIT(sizeof sbuf, sbuf);
	rc = m0_be_btree_cursor_get_sync(cursor, &start, true);
	M0_UT_ASSERT(rc == -ENOENT);

	rc = m0_be_btree_cursor_last_sync(cursor);
	M0_UT_ASSERT(rc == 0);
	m0_be_btree_cursor_kv_get(cursor, &key, NULL);
	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, INSERT_COUNT -1);
	M0_UT_ASSERT(strcmp(key.b_addr, sbuf) == 0);

	rc = m0_be_btree_cursor_first_sync(cursor);
	M0_UT_ASSERT(rc == 0);
	m0_be_btree_cursor_kv_get(cursor, &key, &val);
	sprintf(sbuf, "%0*d", INSERT_KSIZE-1, 0);
	M0_UT_ASSERT(strcmp(key.b_addr, sbuf) == 0);
	sprintf(sbuf, "%0*d", INSERT_VSIZE-1, 0);
	M0_UT_ASSERT(strcmp(val.b_addr, sbuf) == 0);

	m0_be_btree_cursor_fini(cursor);
	m0_free(cursor);
}

static void check(struct m0_be_btree *tree)
{
	struct m0_be_op *op;
	struct m0_buf    key;
	struct m0_buf    val;
	char             k[INSERT_KSIZE];
	char             v[INSERT_VSIZE * 2];
	char             v2[INSERT_VSIZE * 3];
	char             s[INSERT_VSIZE * 2];
	int              i;
	int              rc;

	M0_ALLOC_PTR(op);
	M0_ASSERT(op != NULL);

	m0_be_btree_init(tree, seg, &kv_ops);

	m0_buf_init(&key, k, INSERT_KSIZE);

	/* lookup */
	for (i = 0; i < INSERT_COUNT; ++i) {
		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_SET0(op);

		if (i == INSERT_COUNT - 2)
			m0_buf_init(&val, v2, ARRAY_SIZE(v2));
		else
			m0_buf_init(&val, v, INSERT_VSIZE*2);

		rc = M0_BE_OP_SYNC_RET_WITH(
			op, m0_be_btree_lookup(tree, op, &key, &val),
			bo_u.u_btree.t_rc);

		if (INSERT_COUNT/4 <= i && i < INSERT_COUNT*3/4)
			M0_UT_ASSERT(rc == -ENOENT);
		else if (i == INSERT_COUNT - 1)
			M0_UT_ASSERT(strcmp(v, "XYZ") == 0);
		else if (i == INSERT_COUNT - 2)
			M0_UT_ASSERT(strcmp(v2, "ABCDEFGHI") == 0);
		else {
			if ((i & 1) == 0) {
				sprintf(s, "%0*d", INSERT_VSIZE-1, i);
				M0_UT_ASSERT(strcmp(v, s) == 0);
			} else {
				sprintf(s, "%0*d", INSERT_VSIZE*2 - 1, i);
				M0_UT_ASSERT(strcmp(v, s) == 0);
			}
		}
	}

	/* lookup inplace */
	for (i = 0; i < INSERT_COUNT; ++i) {
		struct m0_be_btree_anchor anchor;

		sprintf(k, "%0*d", INSERT_KSIZE-1, i);
		M0_SET0(op);
		rc = M0_BE_OP_SYNC_RET_WITH(
			op,
			m0_be_btree_lookup_inplace(tree, op, &key, &anchor),
			bo_u.u_btree.t_rc);
		val = anchor.ba_value;

		if (INSERT_COUNT/4 <= i && i < INSERT_COUNT*3/4)
			M0_UT_ASSERT(rc == -ENOENT);
		else if (i == INSERT_COUNT - 1)
			M0_UT_ASSERT(strcmp(val.b_addr, "XYZ") == 0);
		else if (i == INSERT_COUNT - 2)
			M0_UT_ASSERT(strcmp(val.b_addr, "ABCDEFGHI") == 0);
		else {
			if ((i & 1) == 0) {
				sprintf(s, "%0*d", INSERT_VSIZE-1, i);
				M0_UT_ASSERT(strcmp(val.b_addr, s) == 0);
			} else {
				sprintf(s, "%0*d", INSERT_VSIZE*2 - 1, i);
				M0_UT_ASSERT(strcmp(val.b_addr, s) == 0);
			}
		}

		m0_be_btree_release(NULL, &anchor);
	}

	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(op, m0_be_btree_minkey(tree, op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	sprintf(s, "%0*d", INSERT_KSIZE-1, 0);
	M0_UT_ASSERT(strcmp(key.b_addr, s) == 0);

	sprintf(k, "%0*d", INSERT_KSIZE-1, INSERT_COUNT - 1);
	M0_SET0(op);
	rc = M0_BE_OP_SYNC_RET_WITH(op, m0_be_btree_maxkey(tree, op, &key),
	                            bo_u.u_btree.t_rc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(key.b_addr, k) == 0);

	cursor_test(tree);
	btree_dbg_print(tree);
	m0_be_btree_fini(tree);
	m0_free(op);
}

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
