/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 18-Mar-2015
 */

#include "ut/ut.h"
#include "lib/arith.h"                  /* max64, min64 */
#include "lib/misc.h"
#include "lib/tlist.h"

struct foo {
	uint64_t        f_val;
	struct m0_tlink f_linkage;
};

M0_TL_DESCR_DEFINE(foo, "fold-foo", static, struct foo, f_linkage, f_val, 0, 0);
M0_TL_DEFINE(foo, static, struct foo);

void test_fold(void)
{
	const int a[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 };
	struct m0_tl head;
	struct foo foos[ARRAY_SIZE(a)];
	int x = 0;

	/* Test summation. */
	M0_UT_ASSERT(m0_reduce(i, 4, 0, + a[i]) == a[0] + a[1] + a[2] + a[3]);
	/* Test empty range. */
	M0_UT_ASSERT(m0_reduce(i, 0, 8, + 1/x) == 8);
	/* Gauss' childhood problem, as in popular sources. */
	M0_UT_ASSERT(m0_reduce(i, 100, 0, + i + 1) == 5050);
	/* Maximum. */
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), -1, max64(s, a[i])) == 9);
	/* Minimum. */
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), 99, min64(s, a[i])) == 0);
	/* Now, find the *index* of the maximum. */
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), 0, a[i] > a[s] ? i : s) == 8);
	M0_UT_ASSERT(m0_fold(i, s, ARRAY_SIZE(a), 0, a[i] < a[s] ? i : s) == 9);
	foo_tlist_init(&head);
	/* Check empty list. */
	M0_UT_ASSERT(m0_tl_reduce(foo, f, &head, 8, + 1/x) == 8);
	for (x = 0; x < ARRAY_SIZE(a); ++x) {
		foo_tlink_init_at(&foos[x], &head);
		foos[x].f_val = a[x];
	}
	/* Sums of squares are the same. */
	M0_UT_ASSERT(m0_tl_reduce(foo, f, &head, 0, + f->f_val * f->f_val) ==
		     m0_reduce(i, ARRAY_SIZE(a), 0, + a[i] * a[i]));
	/* Maximal element in the list has maximal value. */
	M0_UT_ASSERT(m0_tl_fold(foo, f, s, &head, foo_tlist_head(&head),
				f->f_val > s->f_val ? f : s)->f_val ==
		     m0_fold(i, s, ARRAY_SIZE(a), -1, max64(s, a[i])));
}

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
