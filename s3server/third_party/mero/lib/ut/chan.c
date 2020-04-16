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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 05/13/2010
 */

#include "ut/ut.h"
#include "lib/thread.h"
#include "lib/mutex.h"
#include "lib/chan.h"
#include "lib/assert.h"
#include "lib/timer.h"

enum {
	NR = 16
};

static struct m0_thread t[NR];
static struct m0_chan   cc[NR];
static struct m0_mutex  m[NR];
static struct m0_clink  l[NR];

static void t0(int self)
{
	int i;
	int j;

	for (i = 0; i < NR; ++i) {
		for (j = 0; j < NR; ++j) {
			if (j != self)
				m0_chan_signal_lock(&cc[j]);
		}

		for (j = 0; j < NR - 1; ++j)
			m0_chan_wait(&l[self]);
	}
}

static int flag;

static bool cb1(struct m0_clink *clink)
{
	flag += 1;
	return false;
}

static bool cb2(struct m0_clink *clink)
{
	flag += 2;
	return false;
}

static bool cb_filter(struct m0_clink *clink)
{
	return flag == 1;
}

static bool mfilter(struct m0_clink *clink)
{
	M0_UT_ASSERT(flag == 0);

	flag = 1;
	return false;
}

unsigned long signal_the_chan_in_timer(unsigned long data)
{
	struct m0_clink *clink = (struct m0_clink *)data;
	m0_clink_signal(clink);
	return 0;
}

void test_chan(void)
{
	struct m0_mutex mutex = {};
	struct m0_chan  chan = {};
	struct m0_clink clink1;
	struct m0_clink clink2;
	struct m0_clink clink3;
	struct m0_timer timer;
	int             i;
	int             j;
	bool            got;
	int		rc;

	m0_mutex_init(&mutex);
	m0_chan_init(&chan, &mutex);

	/* test call-back notification */
	flag = 0;
	m0_clink_init(&clink1, &cb1);

	m0_mutex_lock(&mutex);
	m0_clink_add(&chan, &clink1);
	m0_chan_signal(&chan);
	M0_UT_ASSERT(flag == 1);
	m0_chan_broadcast(&chan);
	M0_UT_ASSERT(flag == 2);
	m0_mutex_unlock(&mutex);

	m0_clink_init(&clink2, &cb2);
	m0_clink_add_lock(&chan, &clink2);

	flag = 0;
	m0_chan_signal_lock(&chan);
	M0_UT_ASSERT(flag == 1 || flag == 2);
	flag = 0;
	m0_chan_broadcast_lock(&chan);
	M0_UT_ASSERT(flag == 3);

	m0_mutex_lock(&mutex);
	m0_clink_del(&clink1);
	m0_mutex_unlock(&mutex);

	flag = 0;
	m0_chan_signal_lock(&chan);
	M0_UT_ASSERT(flag == 2);
	flag = 0;
	m0_chan_broadcast_lock(&chan);
	M0_UT_ASSERT(flag == 2);

	m0_clink_del_lock(&clink2);

	m0_clink_fini(&clink1);
	m0_clink_fini(&clink2);

	/* test synchronous notification */

	m0_clink_init(&clink1, NULL);
	m0_clink_add_lock(&chan, &clink1);

	got = m0_chan_trywait(&clink1);
	M0_UT_ASSERT(!got);

	m0_chan_signal_lock(&chan);
	got = m0_chan_trywait(&clink1);
	M0_UT_ASSERT(got);

	m0_chan_signal_lock(&chan);
	m0_chan_wait(&clink1);

	/* wait will expire after 1/5 second */
	got = m0_chan_timedwait(&clink1,
				m0_time_from_now(0, M0_TIME_ONE_SECOND/5));
	M0_UT_ASSERT(!got);

	/* chan is signaled after 1/10 second. so the wait will return true */
	rc = m0_timer_init(&timer, M0_TIMER_HARD, NULL,
			   &signal_the_chan_in_timer, (unsigned long)&clink1);
	M0_UT_ASSERT(rc == 0);
	m0_timer_start(&timer, m0_time_from_now(0, M0_TIME_ONE_SECOND/10));
	got = m0_chan_timedwait(&clink1,
				m0_time_from_now(0, M0_TIME_ONE_SECOND/5));
	M0_UT_ASSERT(got);
	m0_timer_stop(&timer);
	m0_timer_fini(&timer);

	/* chan is signaled after 1/3 seconds. so the wait will timeout and
	   return false. Another wait should work.*/
	rc = m0_timer_init(&timer, M0_TIMER_HARD, NULL,
			   &signal_the_chan_in_timer, (unsigned long)&clink1);
	M0_UT_ASSERT(rc == 0);
	m0_timer_start(&timer, m0_time_from_now(0, M0_TIME_ONE_SECOND/3));
	got = m0_chan_timedwait(&clink1,
				m0_time_from_now(0, M0_TIME_ONE_SECOND/5));
	M0_UT_ASSERT(!got);
	m0_chan_wait(&clink1); /* another wait. Timer will signal in 1 second */
	m0_timer_stop(&timer);
	m0_timer_fini(&timer);

	m0_clink_del_lock(&clink1);
	m0_clink_fini(&clink1);

	/* test filtered events. */
	m0_clink_init(&clink3, &cb_filter);
	m0_clink_add_lock(&chan, &clink3);

	flag = 1;
	m0_chan_signal_lock(&chan);
	got = m0_chan_trywait(&clink3);
	M0_UT_ASSERT(!got);

	flag = 0;
	m0_chan_signal_lock(&chan);
	got = m0_chan_trywait(&clink3);
	M0_UT_ASSERT(got);

	m0_clink_del_lock(&clink3);
	m0_clink_fini(&clink3);

	m0_chan_fini_lock(&chan);
	m0_mutex_fini(&mutex);

	/* multi-threaded test */

	for (i = 0; i < ARRAY_SIZE(cc); ++i) {
		m0_mutex_init(&m[i]);
		m0_chan_init(&cc[i], &m[i]);
		m0_clink_init(&l[i], NULL);
		m0_clink_add_lock(&cc[i], &l[i]);
	}

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		got = M0_THREAD_INIT(&t[i], int, NULL, &t0, i, "t0");
		M0_UT_ASSERT(got == 0);
	}

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}

	for (i = 0; i < ARRAY_SIZE(cc); ++i) {
		m0_clink_del_lock(&l[i]);
		m0_clink_fini(&l[i]);
		m0_chan_fini_lock(&cc[i]);
		m0_mutex_fini(&m[i]);
	}

	/*
	 * multi-channel test
	 *
	 * NR clinks are arranged in a group, with cc[0] as a head. Each clink is
	 * added to the corresponding channel.
	 *
	 * j-th channel is signalled and the signal is awaited for on the (j+1)
	 * (in cyclic order) channel.
	 *
	 * mfilter() attached to j-th channel to check filtering for groups.
	 */

	for (j = 0; j < ARRAY_SIZE(cc); ++j) {
		for (i = 0; i < ARRAY_SIZE(cc); ++i) {
			m0_mutex_init(&m[i]);
			m0_chan_init(&cc[i], &m[i]);
		}

		m0_clink_init(&l[0], j == 0 ? mfilter : NULL);
		for (i = 1; i < ARRAY_SIZE(cc); ++i)
			m0_clink_attach(&l[i], &l[0], j == i ? mfilter : NULL);

		for (i = 0; i < ARRAY_SIZE(cc); ++i)
			m0_clink_add_lock(&cc[i], &l[i]);

		flag = 0;
		rc = m0_timer_init(&timer, M0_TIMER_HARD, NULL,
				   &signal_the_chan_in_timer, (unsigned long)&l[j]);
		M0_UT_ASSERT(rc == 0);
		m0_timer_start(&timer, m0_time_from_now(0, M0_TIME_ONE_SECOND/100));

		m0_chan_wait(&l[(j + 1) % ARRAY_SIZE(cc)]);
		M0_UT_ASSERT(flag == 1);

		m0_timer_stop(&timer);
		m0_timer_fini(&timer);

		for (i = ARRAY_SIZE(cc) - 1; i >= 0; --i) {
			m0_clink_del_lock(&l[i]);
			m0_clink_fini(&l[i]);
			m0_chan_fini_lock(&cc[i]);
			m0_mutex_fini(&m[i]);
		}
	}

}
M0_EXPORTED(test_chan);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
