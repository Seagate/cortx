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
 * Original author: Nathan Rutman <Nathan_Rutman@xyratex.com>,
 *                  Huang Hua <Hua_Huang@xyratex.com>
 *		    Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 12/06/2010
 */

#include "lib/time.h"		/* m0_time_t */

#include "lib/arith.h"		/* max_check */
#include "lib/assert.h"		/* M0_PRE */
#include "lib/atomic.h"		/* m0_atomic64 */
#include "lib/semaphore.h"	/* m0_semaphore */
#include "lib/thread.h"		/* m0_thread */
#include "lib/trace.h"		/* M0_LOG */
#include "lib/ub.h"		/* m0_ub_set */

#include "ut/ut.h"		/* M0_UT_ASSERT */

enum {
	THREADS_NR_MAX = 32,
	TIME_VALUES_NR = 0x10000,
	/** 2ms accepted difference between m0_time_now() on different cores */
	DIFF_ACCEPTED  = 2000000,
};

struct time_test {
	m0_time_t tt_now;
	int	  tt_thread_index;
	int64_t	  tt_prev_index;
};

struct time_test_thread {
	struct m0_thread    tth_thread;
	struct m0_semaphore tth_start;
};

static struct time_test	       time_values[TIME_VALUES_NR];
static struct time_test_thread time_threads[THREADS_NR_MAX];
static struct m0_atomic64      time_index;
static unsigned long	       err_nr;	/**< Number of out-of-sync errors */
static m0_time_t	       err_max;	/**< Maximum out-of-sync error */

static void time_test_simple(void)
{
	m0_time_t t1, t2, t3;
	int rc;

	t1 = m0_time(1, 0);
	t2 = m0_time(2, 0);
	M0_UT_ASSERT(t2 > t1);
	M0_UT_ASSERT(M0_TIME_NEVER > t1);
	M0_UT_ASSERT(t2 < M0_TIME_NEVER);

	t1 = m0_time(1234, 0);
	M0_UT_ASSERT(M0_TIME_NEVER > t1);

	t1 = m0_time_now();
	t2 = t1;
	M0_UT_ASSERT(t1 != 0);

	t1 = m0_time(1234, 987654321);
	M0_UT_ASSERT(t1 == 1234987654321);

	t2 = t1;
	M0_UT_ASSERT(t2 == t1);

	t2 = m0_time(1235, 987654322);
	M0_UT_ASSERT(t2 > t1);

	t3 = m0_time_sub(t2, t1);
	M0_UT_ASSERT(t3 == 1000000001);

	t2 = m0_time(1, 500000000);
	t3 = m0_time_add(t1, t2);
	M0_UT_ASSERT(t3 == 1236487654321);

	t2 = m0_time(0, M0_TIME_ONE_SECOND / 100);
	rc = m0_nanosleep(t2, &t1);
	M0_UT_ASSERT(rc == 0);

	t1 = m0_time(1234, 987654321);
	t2 = m0_time_add(t1, M0_TIME_NEVER);
	M0_UT_ASSERT(t2 == M0_TIME_NEVER);

	t2 = m0_time(1, 500000000);
	t2 = m0_time_add(M0_TIME_NEVER, t1);
	M0_UT_ASSERT(t2 == M0_TIME_NEVER);

	t2 = m0_time_sub(M0_TIME_NEVER, t1);
	M0_UT_ASSERT(t2 == M0_TIME_NEVER);
}

static void time_thread(int thread_index)
{
	struct time_test_thread *th = &time_threads[thread_index];
	int64_t			 prev_index = -1;
	int64_t			 index;

	m0_semaphore_down(&th->tth_start);
	while ((index = m0_atomic64_add_return(&time_index, 1)) <=
	       TIME_VALUES_NR) {
		m0_mb();
		time_values[--index] = (struct time_test) {
			.tt_now		 = m0_time_now(),
			.tt_thread_index = thread_index,
			.tt_prev_index	 = prev_index,
		};
		prev_index = index;
		m0_mb();
	}
}

static m0_time_t tv(int index)
{
	M0_ASSERT(index >= -1 && index < TIME_VALUES_NR);
	return index == -1 ? 0 : time_values[index].tt_now;
}

static m0_time_t tv_prev(int index)
{
	M0_ASSERT(index >= 0 && index < TIME_VALUES_NR);
	return tv(time_values[index].tt_prev_index);
}

/*
 * [a], [b], [c] - sequential time measurements for some thread.
 * prev[index] - previous measurement for thread that have measurement #'index'
 * The following statements should be true if time is monotonic:
 * - [a] <= [b]
 * - [b] <= [c]
 * - for each measurement [i] in range (b, c): prev[i] <= [c]
 * - for each measurement [i] in range (b, c): [a] <= [i]
 */
static void time_test_check(int64_t a, int64_t b, int64_t c)
{
	int i;

	M0_PRE(b != -1 && c != -1);
	M0_UT_ASSERT(tv(a) <= tv(b) && tv(b) <= tv(c));
	for (i = b + 1; i <= c - 1; ++i) {
		/* check if m0_time_now() is out-of-sync and record mistiming */
		err_max = max3(err_max, tv_prev(i) > tv(c) ?
					tv_prev(i) - tv(c) : err_max,
					tv(a) > tv(i) ?
					tv(a) - tv(i) : err_max);
		err_nr += tv_prev(i) > tv(c);
		err_nr += tv(a) > tv(i);
		M0_UT_ASSERT(tv_prev(i) <= tv(c) + DIFF_ACCEPTED);
		M0_UT_ASSERT(tv(a) <= tv(i) + DIFF_ACCEPTED);
	}
}

static void time_test_mt_nr(int threads_nr)
{
	static struct time_test_thread *th;
	int				i;
	int				rc;
	int64_t				b;
	int64_t				c;
	bool				checked[THREADS_NR_MAX] = { };

	m0_atomic64_set(&time_index, 0);
	err_nr  = 0;
	err_max = 0;
	/* start time_thread()s */
	for (i = 0; i < threads_nr; ++i) {
		th = &time_threads[i];
		rc = m0_semaphore_init(&th->tth_start, 0);
		M0_UT_ASSERT(rc == 0);
		rc = M0_THREAD_INIT(&th->tth_thread, int, NULL, &time_thread, i,
				    "#%d_time_thread", i);
		M0_UT_ASSERT(rc == 0);
	}
	/* barrier with all time_thread()s */
	for (i = 0; i < threads_nr; ++i)
		m0_semaphore_up(&time_threads[i].tth_start);
	/* wait until all threads finished */
	for (i = 0; i < threads_nr; ++i) {
		th = &time_threads[i];
		rc = m0_thread_join(&th->tth_thread);
		M0_UT_ASSERT(rc == 0);
		m0_thread_fini(&th->tth_thread);
		m0_semaphore_fini(&th->tth_start);
	}
	M0_UT_ASSERT(m0_atomic64_get(&time_index) >= TIME_VALUES_NR);
	/* check monotony of m0_time_now() results */
	for (i = TIME_VALUES_NR - 1; i >= 0; --i) {
		if (checked[time_values[i].tt_thread_index])
			continue;
		checked[time_values[i].tt_thread_index] = true;
		b = i;
		while (1) {
			c = b;
			b = time_values[c].tt_prev_index;
			if (b == -1)
				break;
			time_test_check(time_values[b].tt_prev_index, b, c);
		}
	}
	if (err_nr != 0) {
		M0_LOG(M0_DEBUG, "time UT: threads = %d, samples = %d, "
				 "number of out-of-sync errors = %lu, "
				 "maximum out-of-sync error = %lu ns",
		       threads_nr, TIME_VALUES_NR,
		       err_nr, (unsigned long) err_max);
	} else {
		M0_LOG(M0_DEBUG, "time UT: threads = %d, "
				 "no out-of-sync errors found", threads_nr);
	}
}

void m0_ut_time_test(void)
{
	int t;

	time_test_simple();
	for (t = 1; t <= THREADS_NR_MAX; ++t)
		time_test_mt_nr(t);
}

enum { UB_TIME_ITER = 0x1000000 };

static void ub_time_round(int unused)
{
	(void) m0_time_now();
}

struct m0_ub_set m0_time_ub = {
	.us_name = "time-ub",
	.us_init = NULL,
	.us_fini = NULL,
	.us_run  = {
			{	.ub_name = "now",
				.ub_iter = UB_TIME_ITER,
				.ub_round = ub_time_round },
			{	.ub_name = NULL		  }
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
