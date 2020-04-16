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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 05/07/2010
 */

#include "lib/misc.h"   /* M0_SET0 */
#include "lib/ub.h"
#include "ut/ut.h"
#include "lib/vec.h"    /* M0_SEG_SIZE & M0_SEG_SHIFT */
#include "lib/memory.h"

struct test1 {
	int a;
};

void test_memory(void)
{
	void         *ptr1;
	struct test1 *ptr2;
	size_t        allocated;
	int           i;
	allocated = m0_allocated();
	ptr1 = m0_alloc(100);
	M0_UT_ASSERT(ptr1 != NULL);

	M0_ALLOC_PTR(ptr2);
	M0_UT_ASSERT(ptr2 != NULL);

	m0_free(ptr1);
	/*
	 * +16, because free(3) may use first 16 bytes for its nefarious
	 * purposes.
	 */
#if defined(ENABLE_FREE_POISON)
	M0_UT_ASSERT(m0_is_poisoned((char *)ptr1 + 16));
#endif
	m0_free(ptr2);
	M0_UT_ASSERT(allocated == m0_allocated());

	/* Checking m0_alloc_aligned for buffer sizes from 4K to 64Kb. */
	for (i = 0; i <= M0_SEG_SIZE * 16; i += M0_SEG_SIZE / 2) {
		ptr1 = m0_alloc_aligned(i, M0_SEG_SHIFT);
		M0_UT_ASSERT(m0_addr_is_aligned(ptr1, M0_SEG_SHIFT));
		m0_free_aligned(ptr1, (size_t)i, M0_SEG_SHIFT);
	}

}

enum {
	UB_ITER   = 5000000,
	UB_SMALL  = 1,
	UB_MEDIUM = 17,
	UB_LARGE  = 512,
	UB_HUGE   = 128*1024
};

static void *ubx[UB_ITER];

static int ub_init(const char *opts M0_UNUSED)
{
	M0_SET_ARR0(ubx);
	return 0;
}

static void ub_free(int i)
{
	m0_free(ubx[i]);
}

static void ub_small(int i)
{
	ubx[i] = m0_alloc(UB_SMALL);
}

static void ub_medium(int i)
{
	ubx[i] = m0_alloc(UB_MEDIUM);
}

static void ub_large(int i)
{
	ubx[i] = m0_alloc(UB_LARGE);
}

static void ub_huge(int i)
{
	ubx[i] = m0_alloc(UB_HUGE);
}

#if 0
static void ub_free_all(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ubx); ++i)
		m0_free(ubx[i]);
}
#endif

struct m0_ub_set m0_memory_ub = {
	.us_name = "memory-ub",
	.us_init = ub_init,
	.us_fini = NULL,
	.us_run  = {
		{ .ub_name  = "alloc-small",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_small },

		{ .ub_name  = "free-small",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_free },

		{ .ub_name  = "alloc-medium",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_medium },

		{ .ub_name  = "free-medium",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_free },

		{ .ub_name  = "alloc-large",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_large },

		{ .ub_name  = "free-large",
		  .ub_iter  = UB_ITER,
		  .ub_round = ub_free },

		{ .ub_name  = "alloc-huge",
		  .ub_iter  = UB_ITER/1000,
		  .ub_round = ub_huge },

		{ .ub_name  = "free-huge",
		  .ub_iter  = UB_ITER/1000,
		  .ub_round = ub_free },

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
