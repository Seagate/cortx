/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Andriy Tkachuk <andriy_tkachuk@xyratex.com>
 * Original creation date: 08/13/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_EXTMAP
#include "lib/trace.h"
#include "lib/arith.h"    /* M0_3WAY, m0_uint128 */
#include "lib/errno.h"     /* ENOENT */
#include "lib/vec.h"
#include "lib/types.h"
#include "lib/ub.h"
#include "lib/misc.h"
#include "lib/finject.h"
#include "ut/ut.h"
#include "be/ut/helper.h"
#include "be/extmap.h"

static struct m0_be_ut_backend be_ut_emap_backend;
static struct m0_be_ut_seg     be_ut_emap_seg;

static struct m0_be_tx          tx1;
static struct m0_be_tx          tx2;
static struct m0_be_emap       *emap;
static struct m0_uint128        prefix;
static struct m0_be_emap_cursor it;
static struct m0_be_emap_seg   *seg; /* cursor segment */
static struct m0_be_seg        *be_seg;
static struct m0_be_op         *it_op;

static void emap_be_alloc(struct m0_be_tx *tx)
{
	struct m0_be_tx_credit cred = {};
	int		       rc;

	M0_BE_ALLOC_CREDIT_PTR(emap, be_seg, &cred);

	m0_be_ut_tx_init(tx, &be_ut_emap_backend);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	M0_BE_ALLOC_PTR_SYNC(emap, be_seg, tx);
	M0_UT_ASSERT(emap != NULL);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

static void emap_be_free(struct m0_be_tx *tx)
{
	struct m0_be_tx_credit cred = {};
	int		       rc;

	M0_BE_FREE_CREDIT_PTR(emap, be_seg, &cred);

	m0_be_ut_tx_init(tx, &be_ut_emap_backend);
	m0_be_tx_prep(tx, &cred);

	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc == 0);

	M0_BE_FREE_PTR_SYNC(emap, be_seg, tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

/* XXX DELETEME? */
static void checkpoint(void)
{
}

static void test_obj_init(struct m0_be_tx *tx)
{
	M0_BE_OP_SYNC(op, m0_be_emap_obj_insert(emap, tx, &op, &prefix, 42));
	checkpoint();
}

static void test_obj_fini(struct m0_be_tx *tx)
{
	int rc;

	rc = M0_BE_OP_SYNC_RET(
		op,
		m0_be_emap_obj_delete(emap, tx, &op, &prefix),
		bo_u.u_emap.e_rc);
	M0_UT_ASSERT(rc == 0);
	checkpoint();
}

static void test_init(void)
{
	struct m0_be_domain_cfg cfg;
	struct m0_be_tx_credit	cred = {};
	int			rc;

	M0_ENTRY();

	/* Init BE */
	/** XXX @todo break UT into small transactions */
	M0_SET0(&cfg);
	m0_be_ut_backend_cfg_default(&cfg);
	cfg.bc_engine.bec_tx_size_max = M0_BE_TX_CREDIT(1 << 21, 1 << 26);
	cfg.bc_engine.bec_group_cfg.tgc_size_max =
		M0_BE_TX_CREDIT(1 << 22, 1 << 27);
	rc = m0_be_ut_backend_init_cfg(&be_ut_emap_backend, &cfg, true);
	M0_UT_ASSERT(rc == 0);
	m0_be_ut_seg_init(&be_ut_emap_seg, &be_ut_emap_backend, 1ULL << 26);
	be_seg = be_ut_emap_seg.bus_seg;

	emap_be_alloc(&tx1);
	m0_be_emap_init(emap, be_seg);

	m0_be_emap_credit(emap, M0_BEO_CREATE, 1, &cred);
	m0_be_emap_credit(emap, M0_BEO_DESTROY, 1, &cred);
	m0_be_emap_credit(emap, M0_BEO_INSERT, 1, &cred);
	m0_be_emap_credit(emap, M0_BEO_DELETE, 1, &cred);
	m0_forall(i, 5, m0_be_emap_credit(emap, M0_BEO_SPLIT, 3, &cred), true);
	m0_be_emap_credit(emap, M0_BEO_MERGE, 5 * 3, &cred);
	m0_be_emap_credit(emap, M0_BEO_PASTE, 3, &cred);

	m0_be_ut_tx_init(&tx2, &be_ut_emap_backend);
	m0_be_tx_prep(&tx2, &cred);
	rc = m0_be_tx_open_sync(&tx2);
	M0_UT_ASSERT(rc == 0);

	M0_BE_OP_SYNC(op, m0_be_emap_create(emap, &tx2, &op));

	m0_uint128_init(&prefix, "some random iden");
	seg = m0_be_emap_seg_get(&it);
	it_op = m0_be_emap_op(&it);

	M0_LEAVE();
}

static void test_fini(void)
{
	M0_BE_OP_SYNC(op, m0_be_emap_destroy(emap, &tx2, &op));

	m0_be_tx_close_sync(&tx2);
	m0_be_tx_fini(&tx2);

	emap_be_free(&tx1);

	m0_be_ut_seg_fini(&be_ut_emap_seg);
	m0_be_ut_backend_fini(&be_ut_emap_backend);
}

static int be_emap_lookup(struct m0_be_emap        *map,
			  const struct m0_uint128  *prefix,
			  m0_bindex_t               offset,
			  struct m0_be_emap_cursor *it)
{
	int rc;

	M0_SET0(&it->ec_op);
	m0_be_op_init(&it->ec_op);
	m0_be_emap_lookup(emap, prefix, offset, it);
	m0_be_op_wait(&it->ec_op);
	rc = it->ec_op.bo_u.u_emap.e_rc;
	m0_be_op_fini(&it->ec_op);

	return rc;
}

static void test_lookup(void)
{
	int rc;

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_emap_ext_is_first(&seg->ee_ext));
	M0_UT_ASSERT(m0_be_emap_ext_is_last(&seg->ee_ext));
	M0_UT_ASSERT(seg->ee_val == 42);

	m0_be_emap_close(&it);

	rc = be_emap_lookup(emap, &prefix, 1000000, &it);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_emap_ext_is_first(&seg->ee_ext));
	M0_UT_ASSERT(m0_be_emap_ext_is_last(&seg->ee_ext));
	M0_UT_ASSERT(seg->ee_val == 42);

	m0_be_emap_close(&it);

	++prefix.u_lo;
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == -ENOENT);
	--prefix.u_lo;

	m0_be_emap_close(&it);

	checkpoint();
}

static void split(m0_bindex_t offset, int nr, bool commit)
{
	int i;
	int rc;
	m0_bcount_t len[] = { 100, 2, 0, 0 };
	uint64_t    val[] = { 1,   2, 3, 4 };
	struct m0_indexvec vec = {
		.iv_vec = {
			.v_nr    = ARRAY_SIZE(len),
			.v_count = len
		},
		.iv_index = val
	};

	rc = be_emap_lookup(emap, &prefix, offset, &it);
	M0_UT_ASSERT(rc == 0);

	M0_LOG(M0_INFO, "off=%lu nr=%d", (unsigned long)offset, nr);
	for (i = 0; i < nr; ++i) {
		m0_bcount_t seglen;
		m0_bcount_t total;

		seglen = m0_ext_length(&seg->ee_ext);
		M0_LOG(M0_DEBUG, "%3i: seglen=%llx", i,
					(unsigned long long)seglen);
		total  = 102; /* 100 + 2, the sum of elements in len[]. */
		M0_UT_ASSERT(seglen > total);
		len[ARRAY_SIZE(len) - 1] = seglen - total;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_split(&it, &tx2, &vec);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it.ec_op.bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
		M0_UT_ASSERT(m0_ext_length(&seg->ee_ext) ==
						len[ARRAY_SIZE(len) - 1]);
	}

	m0_be_emap_close(&it);
	if (commit)
		checkpoint();
}

static void test_split(void)
{
	split(0, 5, true);
}

static int test_print(void)
{
	int i;
	int rc;

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	M0_LOG(M0_DEBUG, U128X_F":", U128_P(&prefix));
	for (i = 0; ; ++i) {
		M0_LOG(M0_DEBUG, "\t%5.5i %16lx .. %16lx: %16lx %10lx", i,
		       (unsigned long)seg->ee_ext.e_start,
		       (unsigned long)seg->ee_ext.e_end,
		       (unsigned long)m0_ext_length(&seg->ee_ext),
		       (unsigned long)seg->ee_val);
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_next(&it);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	m0_be_emap_close(&it);

	return i;
}

static void test_next_prev(void)
{
	int i;
	int rc;
	int n;

	n = test_print();

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	m0_fi_enable_once("be_emap_changed", "yes");
	for (i = 0; ; ++i) {
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_next(&it);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	M0_UT_ASSERT(i == n);

	m0_fi_enable_once("be_emap_changed", "yes");
	for (i = 0; ; ++i) {
		if (m0_be_emap_ext_is_first(&seg->ee_ext))
			break;
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_prev(&it);
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	M0_UT_ASSERT(i == n);
	m0_be_emap_close(&it);

}

static void test_merge(void)
{
	int rc;

	M0_LOG(M0_INFO, "Merge all segments...");
	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	while (!m0_be_emap_ext_is_last(&seg->ee_ext)) {
		M0_SET0(it_op);
		m0_be_op_init(it_op);
		m0_be_emap_merge(&it, &tx2, m0_ext_length(&seg->ee_ext));
		m0_be_op_wait(it_op);
		M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
		m0_be_op_fini(it_op);
	}
	m0_be_emap_close(&it);
	checkpoint();
}

static void test_paste(void)
{
	int		 rc;
	struct m0_ext	 e;

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	e.e_start = 10;
	e.e_end   = 20;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_be_emap_paste(&it, &tx2, &e, 12, NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);

	M0_UT_ASSERT(seg->ee_ext.e_start == 20);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	test_print();

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start ==  0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == 10);

	rc = be_emap_lookup(emap, &prefix, 10, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == 10);
	M0_UT_ASSERT(seg->ee_ext.e_end   == 20);
	M0_UT_ASSERT(seg->ee_val         == 12);

	rc = be_emap_lookup(emap, &prefix, 20, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == 20);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	e.e_start = 5;
	e.e_end   = 25;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_be_emap_paste(&it, &tx2, &e, 11, NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);

	M0_UT_ASSERT(seg->ee_ext.e_start == 25);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	test_print();

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == 0);
	M0_UT_ASSERT(seg->ee_ext.e_end   == 5);

	rc = be_emap_lookup(emap, &prefix, 5, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start ==  5);
	M0_UT_ASSERT(seg->ee_ext.e_end   == 25);
	M0_UT_ASSERT(seg->ee_val         == 11);

	rc = be_emap_lookup(emap, &prefix, 25, &it);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(seg->ee_ext.e_start == 25);
	M0_UT_ASSERT(seg->ee_ext.e_end   == M0_BINDEX_MAX + 1);

	rc = be_emap_lookup(emap, &prefix, 0, &it);
	M0_UT_ASSERT(rc == 0);

	e.e_start = 0;
	e.e_end   = M0_BINDEX_MAX + 1;

	M0_LOG(M0_INFO, "Paste [%d, %d)...", (int)e.e_start, (int)e.e_end);
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	m0_be_emap_paste(&it, &tx2, &e, 0, NULL, NULL, NULL);
	m0_be_op_wait(it_op);
	M0_UT_ASSERT(it_op->bo_u.u_emap.e_rc == 0);
	m0_be_op_fini(it_op);

	test_print();

	m0_be_emap_close(&it);
}

void m0_be_ut_emap(void)
{
	test_init();
	test_obj_init(&tx2);
	test_lookup();
	test_split();
	test_print();
	test_next_prev();
	test_merge();
	test_paste();
	test_obj_fini(&tx2);
	test_fini();
}

#if 0 /* XXX RESTOREME */
struct m0_ut_suite m0_be_ut_emap = {
	.ts_name = "be-emap-ut",
	.ts_tests = {
		{ "emap-init", test_init },
		{ "obj-init", test_obj_init },
		{ "lookup", test_lookup },
		{ "split", test_split },
		{ "print", test_print },
		{ "merge", test_merge },
		{ "obj-fini", test_obj_fini },
		{ "emap-fini", test_fini },
		{ NULL, NULL }
	}
};
#endif

/*
 * UB
 */
//
//enum {
//	UB_ITER = 100000,
//	UB_ITER_TX = 10000
//};
//
//static int ub_init(const char *opts M0_UNUSED)
//{
//	test_init();
//	return 0;
//}
//
//static void ub_fini(void)
//{
//	test_fini();
//}
//
//static struct m0_uint128 p;
//
//static void ub_obj_init(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_insert(emap, &tx, &op, &p, 42);
//	M0_ASSERT(m0_be_op_is_done(&op));
//	checkpoint();
//}
//
//static void ub_obj_fini(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_delete(emap, &tx, &op, &p);
//	M0_ASSERT(m0_be_op_is_done(&op));
//	checkpoint();
//}
//
//static void ub_obj_init_same(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_insert(emap, &tx, &op, &p, 42);
//	M0_ASSERT(m0_be_op_is_done(&op));
//}
//
//static void ub_obj_fini_same(int i)
//{
//	p = prefix;
//
//	p.u_hi += i;
//	p.u_lo -= i*i;
//
//	m0_be_emap_obj_delete(emap, &tx, &op, &p);
//	M0_ASSERT(m0_be_op_is_done(&op));
//}
//
//static void ub_split(int i)
//{
//	split(5000, 1, false);
//}
//
//struct m0_ub_set m0_be_emap_ub = {
//	.us_name = "emap-ub",
//	.us_init = ub_init,
//	.us_fini = ub_fini,
//	.us_run  = {
//		{ .ub_name = "obj-init",
//		  .ub_iter = UB_ITER,
//		  .ub_round = ub_obj_init },
//
//		{ .ub_name = "obj-fini",
//		  .ub_iter = UB_ITER,
//		  .ub_round = ub_obj_fini },
//
//		{ .ub_name = "obj-init-same-tx",
//		  .ub_iter = UB_ITER_TX,
//		  .ub_round = ub_obj_init_same },
//
//		{ .ub_name = "obj-fini-same-tx",
//		  .ub_iter = UB_ITER_TX,
//		  .ub_round = ub_obj_fini_same },
//
//		{ .ub_name = "split",
//		  .ub_iter = UB_ITER/5,
//		  .ub_init = test_obj_init,
//		  .ub_round = ub_split },
//
//		{ .ub_name = NULL }
//	}
//};

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
