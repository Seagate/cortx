/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 02/28/2011
 */

#include "ut/ut.h"
#include "lib/ub.h"
#include "lib/bitmap.h"
#include "lib/assert.h"
#include "lib/misc.h" /* m0_forall */

enum {
	UT_BITMAP_SIZE = 120
};

static void test_bitmap_copy(void)
{
	struct m0_bitmap src;
	struct m0_bitmap dst;
	size_t dst_nr;
	size_t i;
	int n;

	M0_UT_ASSERT(m0_bitmap_init(&src, UT_BITMAP_SIZE) == 0);
	for (i = 0; i < UT_BITMAP_SIZE; i += 3)
		m0_bitmap_set(&src, i, true);

	for (n = 1; n < 3; ++n) {
		/* n == 1: equal sized, n == 2: dst size is bigger */
		dst_nr = n * UT_BITMAP_SIZE;
		M0_UT_ASSERT(m0_bitmap_init(&dst, dst_nr) == 0);
		for (i = 1; i < dst_nr; i += 2)
			m0_bitmap_set(&dst, i, true);

		m0_bitmap_copy(&dst, &src);
		for (i = 0; i < UT_BITMAP_SIZE; ++i)
			M0_UT_ASSERT(m0_bitmap_get(&src, i) ==
				     m0_bitmap_get(&dst, i));
		for (; i < dst_nr; ++i)
			M0_UT_ASSERT(!m0_bitmap_get(&dst, i));
		m0_bitmap_fini(&dst);
	}
	m0_bitmap_fini(&src);
}

void test_bitmap(void)
{
	struct m0_bitmap bm;
	size_t idx;

	M0_UT_ASSERT(m0_bitmap_init(&bm, UT_BITMAP_SIZE) == 0);
	M0_UT_ASSERT(bm.b_nr == UT_BITMAP_SIZE);
	M0_UT_ASSERT(bm.b_words != NULL);

	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx) {
		M0_UT_ASSERT(m0_bitmap_get(&bm, idx) == false);
	}

	m0_bitmap_set(&bm, 0, true);
	M0_UT_ASSERT(m0_bitmap_get(&bm, 0) == true);
	m0_bitmap_set(&bm, 0, false);

	m0_bitmap_set(&bm, 1, true);
	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx) {
		M0_UT_ASSERT(m0_bitmap_get(&bm, idx) == (idx == 1));
	}

	m0_bitmap_set(&bm, 2, true);
	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx) {
		M0_UT_ASSERT(m0_bitmap_get(&bm, idx) == (idx == 1 || idx == 2));
	}

	m0_bitmap_set(&bm, 64, true);
	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx) {
		M0_UT_ASSERT(m0_bitmap_get(&bm, idx) ==
			     (idx == 1 || idx == 2 || idx == 64));
	}

	m0_bitmap_set(&bm, 2, false);
	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx) {
		M0_UT_ASSERT(m0_bitmap_get(&bm, idx) ==
			     (idx == 1 || idx == 64));
	}

	m0_bitmap_set(&bm, 1, false);
	m0_bitmap_set(&bm, 64, false);
	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx) {
		M0_UT_ASSERT(m0_bitmap_get(&bm, idx) == false);
	}

	m0_bitmap_fini(&bm);
	M0_UT_ASSERT(bm.b_nr == 0);
	M0_UT_ASSERT(bm.b_words == NULL);

	test_bitmap_copy();
}

void test_bitmap_onwire(void)
{
	struct m0_bitmap        in_bm;
	struct m0_bitmap        out_bm;
	struct m0_bitmap_onwire ow_bm;

	M0_UT_ASSERT(m0_bitmap_init(&in_bm, UT_BITMAP_SIZE) == 0);
	M0_UT_ASSERT(in_bm.b_nr == UT_BITMAP_SIZE);
	M0_UT_ASSERT(in_bm.b_words != NULL);

	m0_bitmap_set(&in_bm, 1, true);
	m0_bitmap_set(&in_bm, 7, true);
	m0_bitmap_set(&in_bm, 64, true);

	M0_UT_ASSERT(m0_bitmap_onwire_init(&ow_bm, UT_BITMAP_SIZE) == 0);
	m0_bitmap_store(&in_bm, &ow_bm);
	M0_UT_ASSERT(m0_bitmap_init(&out_bm, UT_BITMAP_SIZE) == 0);
	m0_bitmap_load(&ow_bm, &out_bm);

	M0_UT_ASSERT(m0_forall(i, UT_BITMAP_SIZE,
		     m0_bitmap_get(&out_bm, i) == m0_bitmap_get(&in_bm, i)));

	m0_bitmap_fini(&in_bm);
	m0_bitmap_onwire_fini(&ow_bm);
	m0_bitmap_fini(&out_bm);
}

enum {
	UB_ITER = 100000
};

static struct m0_bitmap ub_bm;

static int ub_init(const char *opts M0_UNUSED)
{
	return m0_bitmap_init(&ub_bm, UT_BITMAP_SIZE);
}

static void ub_fini(void)
{
	m0_bitmap_fini(&ub_bm);
}

static void ub_set0(int i)
{
	size_t idx;

	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx)
		m0_bitmap_set(&ub_bm, idx, false);
}

static void ub_set1(int i)
{
	size_t idx;

	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx)
		m0_bitmap_set(&ub_bm, idx, true);
}

static void ub_get(int i)
{
	size_t idx;

	for (idx = 0; idx < UT_BITMAP_SIZE; ++idx)
		m0_bitmap_get(&ub_bm, idx);
}

struct m0_ub_set m0_bitmap_ub = {
	.us_name = "bitmap-ub",
	.us_init = ub_init,
	.us_fini = ub_fini,
	.us_run  = {
		{ .ub_name = "set0",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_set0 },
		{ .ub_name = "set1",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_set1 },
		{ .ub_name = "get",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_get },
		{ .ub_name = NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
