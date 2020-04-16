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
 * Original author: Anatoliy Bilenko <Anatoliy.Bilenko@seagate.com>
 * Original creation date: 10/20/2015
 */

#include "ut/ut.h"
#include "lib/errno.h"
#include "lib/tlist.h"
#include "lib/atomic.h"
#include "lib/thread_pool.h"

static const int ROUNDS_NR = 200;
static const int THREAD_NR = 10;
static const int QLINKS_NR = 10;
static uint64_t test_counts = 0;
static struct m0_atomic64 counts;
struct tpool_test {
	uint64_t        t_count;   /* payload */
	int             t_rc;
	struct m0_tlink t_linkage;
	uint64_t        t_magic;
};
M0_TL_DESCR_DEFINE(tpt, "tpt-s", static, struct tpool_test, t_linkage,
		   t_magic, 0x1111111111111111, 0x1111111111111111);
M0_TL_DEFINE(tpt, static, struct tpool_test);
struct tpool_test test[] = {
	{ .t_count = 1 }, { .t_count = 2 }, { .t_count = 3 }, { .t_count = 4 },
	{ .t_count = 5 }, { .t_count = 6 }, { .t_count = 7 }, { .t_count = 8 },
};
struct tpool_test error_test[] = {
	{ .t_count = 1 }, { .t_count = 2, .t_rc = -EINTR,  },
	{ .t_count = 3 }, { .t_count = 4, .t_rc = -ENOMEM, },
	{ .t_count = 5 }, { .t_count = 6, .t_rc = -E2BIG,  },
	{ .t_count = 7 }, { .t_count = 8, .t_rc = -EINTR,  },
};

static int thread_pool_process(void *item)
{
	struct tpool_test *t = (struct tpool_test *)item;

	if (t->t_rc == 0)
		m0_atomic64_add(&counts, t->t_count);

	return t->t_rc;
}

static void thread_pool_init(struct m0_parallel_pool *pool)
{
	int rc;

	rc = m0_parallel_pool_init(pool, THREAD_NR, QLINKS_NR);
	M0_UT_ASSERT(rc == 0);
}

static void small_thread_pool_init(struct m0_parallel_pool *pool)
{
	int rc;

	rc = m0_parallel_pool_init(pool, THREAD_NR, QLINKS_NR/2);
	M0_UT_ASSERT(rc == 0);
}

static void thread_pool_fini(struct m0_parallel_pool *pool)
{
	m0_parallel_pool_terminate_wait(pool);
	m0_parallel_pool_fini(pool);
}

static void feed(struct m0_parallel_pool *pool, bool err_test)
{
	struct tpool_test *t = !err_test ? test : error_test;
	int                sz = !err_test ? ARRAY_SIZE(test)
					  : ARRAY_SIZE(error_test);
	int                rc;
	int                i;

	for (i = 0; i < sz; ++i) {
		rc = m0_parallel_pool_job_add(pool, &t[i]);
		M0_UT_ASSERT(rc == 0);
		test_counts += !err_test ? t[i].t_count : (t[i].t_rc == 0 ?
							   t[i].t_count : 0);
	}
}

static void simple_thread_pool_test(struct m0_parallel_pool *pool)
{
	int rounds = ROUNDS_NR;
	int rc;

	for (; rounds > 0; --rounds) {
		feed(pool, false);
		m0_parallel_pool_start(pool, thread_pool_process);
		rc = m0_parallel_pool_wait(pool);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_atomic64_get(&counts) == test_counts);
	}

}

static void parallel_for_pool_test(struct m0_parallel_pool *pool)
{
	int rounds = ROUNDS_NR;

	for (; rounds > 0; --rounds) {
		int i;
		int rc;
		struct m0_tl       items;
		struct tpool_test *item;

		tpt_tlist_init(&items);

		for (i = 0; i < ARRAY_SIZE(test); ++i) {
			tpt_tlink_init_at(&test[i], &items);
			test_counts += test[i].t_count;
		}


		rc = M0_PARALLEL_FOR(tpt, pool, &items, thread_pool_process);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_atomic64_get(&counts) == test_counts);

		m0_tl_teardown(tpt, &items, item);
		tpt_tlist_fini(&items);
	}
}

static void simple_thread_pool_error_test(struct m0_parallel_pool *pool)
{
	struct tpool_test *job = NULL;
	int                rounds = ROUNDS_NR;
	int                rc;

	for (; rounds > 0; --rounds) {
		feed(pool, true);
		m0_parallel_pool_start(pool, thread_pool_process);
		rc = m0_parallel_pool_wait(pool);
		M0_UT_ASSERT(rc == -EINTR);
		M0_UT_ASSERT(m0_atomic64_get(&counts) == test_counts);
		while (m0_parallel_pool_rc_next(pool, (void **)&job, &rc) > 0) {
			M0_UT_ASSERT(job != NULL);
			M0_UT_ASSERT(rc == job->t_rc);
		}
	}

}

void m0_ut_lib_thread_pool_test(void)
{
	struct m0_parallel_pool pool = {};
	struct m0_parallel_pool small_pool = {};

	m0_atomic64_set(&counts, 0);

	thread_pool_init(&pool);
	simple_thread_pool_test(&pool);
	parallel_for_pool_test(&pool);
	simple_thread_pool_error_test(&pool);
	thread_pool_fini(&pool);

	small_thread_pool_init(&small_pool);
	parallel_for_pool_test(&small_pool);
	thread_pool_fini(&small_pool);
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
