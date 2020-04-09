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
 * Original creation date: 05/13/2010
 */

#include "ut/ut.h"
#include "lib/thread.h"
#include "lib/mutex.h"
#include "lib/assert.h"

enum {
	NR = 255
};

static int counter;
static struct m0_thread t[NR];
static struct m0_mutex  m[NR];
static struct m0_mutex  static_m = M0_MUTEX_SINIT(&static_m);

static void t0(int n)
{
	int i;

	for (i = 0; i < NR; ++i) {
		m0_mutex_lock(&m[0]);
		counter += n;
		m0_mutex_unlock(&m[0]);
	}
}

static void t1(int n)
{
	int i;
	int j;

	for (i = 0; i < NR; ++i) {
		for (j = 0; j < NR; ++j)
			m0_mutex_lock(&m[j]);
		counter += n;
		for (j = 0; j < NR; ++j)
			m0_mutex_unlock(&m[j]);
	}
}

static void static_mutex_test(void)
{
	m0_mutex_lock(&static_m);
	m0_mutex_unlock(&static_m);
}

void test_mutex(void)
{
	int i;
	int sum;
	int result;

	counter = 0;

	for (sum = i = 0; i < NR; ++i) {
		m0_mutex_init(&m[i]);
		result = M0_THREAD_INIT(&t[i], int, NULL, &t0, i, "t0");
		M0_UT_ASSERT(result == 0);
		sum += i;
	}

	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	M0_UT_ASSERT(counter == sum * NR);

	counter = 0;

	for (sum = i = 0; i < NR; ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &t1, i, "t1");
		M0_UT_ASSERT(result == 0);
		sum += i;
	}

	for (i = 0; i < NR; ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	for (i = 0; i < NR; ++i)
		m0_mutex_fini(&m[i]);

	M0_UT_ASSERT(counter == sum * NR);

	static_mutex_test();
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
