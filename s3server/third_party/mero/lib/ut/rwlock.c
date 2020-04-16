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
 * Original creation date: 05/30/2010
 */

#include "ut/ut.h"
#include "lib/thread.h"
#include "lib/rwlock.h"
#include "lib/assert.h"
#include "lib/semaphore.h"

/**
   @addtogroup rwlock

 */
enum {
	NR = 255,

	/*
	 * Starvation tests.
	 */

	/* A number of "starvers" acquiring and releasing the lock (in the
	   required mode) in an infinite loop. */
	S = 8,
	/* A number of "checkers" which try to squeeze between starvers and
	   acquire the lock in a different mode at least once. */
	C = 3
};

M0_BASSERT(S + C <= NR);

static int counter;
static struct m0_thread t[NR];
static struct m0_rwlock m;
static int sum;
static struct m0_semaphore p;
static struct m0_semaphore q;
static int stop;
static int i;

static void writer(int n)
{
	int i;

	for (i = 0; i < NR; ++i) {
		m0_rwlock_write_lock(&m);
		counter += n;
		m0_rwlock_write_unlock(&m);
	}
}

static void reader(int n)
{
	m0_rwlock_read_lock(&m);
	m0_semaphore_up(&p);
	m0_semaphore_down(&q);
	M0_UT_ASSERT(counter == 0);
	m0_rwlock_read_unlock(&m);
}

static void wstarver(int x)
{
	m0_rwlock_write_lock(&m);
	while (!stop) {
		m0_rwlock_write_unlock(&m);
		m0_rwlock_write_lock(&m);
	}
	m0_rwlock_write_unlock(&m);
}

static void rstarver(int x)
{
	m0_rwlock_read_lock(&m);
	while (!stop) {
		m0_rwlock_read_unlock(&m);
		m0_rwlock_read_lock(&m);
	}
	m0_rwlock_read_unlock(&m);
}


static void rcheck(int x)
{
	m0_rwlock_read_lock(&m);
	m0_rwlock_read_unlock(&m);
}

static void wcheck(int x)
{
	m0_rwlock_write_lock(&m);
	m0_rwlock_write_unlock(&m);
}

/**
   Unit test: mutual exclusion of writers is provided.
 */
static void test_rw_writers(void)
{
	int result;

	counter = 0;

	for (sum = 0, i = 0; i < NR; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &writer, i,
					"writer[%i]", i);
		M0_UT_ASSERT(result == 0);
		sum += i * NR;
	}

	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	M0_UT_ASSERT(counter == sum);
}

/**
   Unit test: test that multiple readers are allowed.
 */
static void test_rw_readers(void)
{
	int result;

	counter = 0;

	for (i = 0; i < NR; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &reader, i,
					"reader[%i]", i);
		M0_UT_ASSERT(result == 0);
		m0_semaphore_down(&p);
	}

	for (i = 0; i < NR; ++i)
		m0_semaphore_up(&q);

	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}
}

/**
   Unit test: test that readers exclude writers.
 */
static void test_rw_excl(void)
{
	int result;

	for (i = 0; i < NR / 2; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &reader, i,
					"reader[%i]", i);
		M0_UT_ASSERT(result == 0);
		m0_semaphore_down(&p);
	}

	for (sum = 0; i < NR; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &writer, i,
					"writer[%i]", i);
		M0_UT_ASSERT(result == 0);
		sum += i * NR;
	}

	for (i = 0; i < NR / 2; ++i) {
		M0_UT_ASSERT(counter == 0);
		m0_semaphore_up(&q);
	}

	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}
	M0_UT_ASSERT(counter == sum);
}

/**
   Unit test: readers are not starved.

   @note Careful: rwlocks are _not_, in fact, fair. If S is increased, the
   starvation tests might become too slow.
 */
static void test_rw_rstarve(void)
{
	int result;

	stop = false;

	for (i = 0; i < S; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &wstarver, 0,
					"wstarver[%i]", i);
		M0_UT_ASSERT(result == 0);
	}

	for (i = S; i < S + C; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &rcheck, 0,
					"rcheck[%i]", i);
		M0_UT_ASSERT(result == 0);
	}

	for (i = S; i < S + C; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	stop = true;

	for (i = 0; i < S; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}
}

/**
   Unit test: writers are not starved.
 */
static void test_rw_wstarve(void)
{
	int result;

	for (i = 0; i < S; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &rstarver, 0,
					"rstarver[%i]", i);
		M0_UT_ASSERT(result == 0);
	}

	for (i = S; i < S + C; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &wcheck, 0,
					"wcheck[%i]", i);
		M0_UT_ASSERT(result == 0);
	}

	for (i = S; i < S + C; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	stop = true;

	for (i = 0; i < S; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}
}

void test_rw(void)
{
	m0_rwlock_init(&m);
	m0_semaphore_init(&p, 0);
	m0_semaphore_init(&q, 0);

	test_rw_writers();
	test_rw_readers();
	test_rw_excl();
	test_rw_rstarve();
	test_rw_wstarve();

	m0_semaphore_fini(&q);
	m0_semaphore_fini(&p);
	m0_rwlock_fini(&m);
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
