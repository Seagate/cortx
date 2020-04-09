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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 07/05/2012
 */

/** @todo create lib/limits.h */
#ifndef __KERNEL__
#include <limits.h>		/* ULONG_MAX */
#else
#include <linux/kernel.h>	/* ULONG_MAX */
#endif

#include "lib/misc.h"		/* M0_SET0 */
#include "ut/ut.h"		/* M0_UT_ASSERT */

#include "mero/magic.h"	/* M0_NET_TEST_TIMESTAMP_MAGIC */

#include "net/test/stats.h"

enum {
	STATS_ONE_MILLION = 1000000,
	STATS_BUF_LEN	  = 0x100,
	STATS_BUF_OFFSET  = 42,
	TIMESTAMP_BUF_LEN = 0x100,
	TIMESTAMP_SEQ	  = 123456,
};

struct stats_expected {
	unsigned long se_count;
	unsigned long se_min;
	unsigned long se_max;
#ifndef __KERNEL__
	double	      se_sum;
	double	      se_avg;
	double	      se_stddev;
#else
	unsigned      se_sum:1;
	unsigned      se_avg:1;
	unsigned      se_stddev:1;
#endif
};

#ifndef __KERNEL__
static bool is_in_eps_neighborhood(double a, double b)
{
	const double eps = 1E-5;

	return a * (1. - eps) <= b && b <= a * (1. + eps);
}
#endif

static void sample_check(struct m0_net_test_stats *stats,
			 struct stats_expected *expected)
{
	unsigned long ul;
#ifndef __KERNEL__
	double	      d;
#endif

	M0_PRE(stats	!= NULL);
	M0_PRE(expected != NULL);

	M0_UT_ASSERT(stats->nts_count == expected->se_count);

	ul = m0_net_test_stats_min(stats);
	M0_UT_ASSERT(ul == expected->se_min);

	ul = m0_net_test_stats_max(stats);
	M0_UT_ASSERT(ul == expected->se_max);

#ifndef __KERNEL__
	d = m0_net_test_stats_sum(stats);
	M0_UT_ASSERT(is_in_eps_neighborhood(expected->se_sum, d));

	d = m0_net_test_stats_avg(stats);
	M0_UT_ASSERT(is_in_eps_neighborhood(expected->se_avg, d));

	d = m0_net_test_stats_stddev(stats);
	M0_UT_ASSERT(is_in_eps_neighborhood(expected->se_stddev, d));
#endif
}

static void stats_serialize_ut(struct m0_net_test_stats *stats)
{
	struct m0_net_test_stats stats2;
	char			 bv_buf[STATS_BUF_LEN];
	void			*bv_addr = bv_buf;
	m0_bcount_t		 bv_len = STATS_BUF_LEN;
	struct m0_bufvec	 bv = M0_BUFVEC_INIT_BUF(&bv_addr, &bv_len);
	m0_bcount_t		 serialized_len;
	m0_bcount_t		 len;
	struct stats_expected	 expected;

	serialized_len = m0_net_test_stats_serialize(M0_NET_TEST_SERIALIZE,
						     stats, &bv,
						     STATS_BUF_OFFSET);
	M0_UT_ASSERT(serialized_len > 0);
	M0_SET0(&stats2);

	len = m0_net_test_stats_serialize(M0_NET_TEST_DESERIALIZE,
					  &stats2, &bv, STATS_BUF_OFFSET);
	M0_UT_ASSERT(len == serialized_len);

	expected.se_count  = stats->nts_count;
	expected.se_min	   = m0_net_test_stats_min(stats);
	expected.se_max	   = m0_net_test_stats_max(stats);
#ifndef __KERNEL__
	expected.se_sum	   = m0_net_test_stats_sum(stats);
	expected.se_avg	   = m0_net_test_stats_avg(stats);
	expected.se_stddev = m0_net_test_stats_stddev(stats);
#endif
	sample_check(&stats2, &expected);
}

static void add_one_by_one(struct m0_net_test_stats *stats,
			   unsigned long *arr,
			   unsigned long arr_len)
{
	unsigned long i;

	M0_PRE(stats != NULL);
	for (i = 0; i < arr_len; ++i) {
		m0_net_test_stats_add(stats, arr[i]);
		stats_serialize_ut(stats);
	}
}

#define STATS_SAMPLE(sample_name)					\
	static unsigned long sample_name ## _sample[]

#define STATS__EXPECTED(sample_name, count, min, max, sum, avg, stddev) \
	static struct stats_expected sample_name ## _expected = {	\
		.se_count  = (count),					\
		.se_min	   = (min),					\
		.se_max    = (max),					\
		.se_sum    = (sum),					\
		.se_avg    = (avg),					\
		.se_stddev = (stddev),					\
	}

#ifndef __KERNEL__
#define STATS_EXPECTED STATS__EXPECTED
#else
#define STATS_EXPECTED(sample_name, count, min, max, sum, avg, stddev) \
	STATS__EXPECTED(sample_name, count, min, max, 0, 0, 0)
#endif

#define STATS_SAMPLE_ADD(stats_name, sample_name)			\
	do {								\
		add_one_by_one(stats_name, sample_name ## _sample,	\
		ARRAY_SIZE(sample_name ## _sample));			\
	} while (0)

#define STATS_CHECK(stats_name, sample_name) \
	sample_check(stats_name, &sample_name ## _expected)

#define STATS_ADD_CHECK(stats_name, sample_name)			\
	do {								\
		STATS_SAMPLE_ADD(stats_name, sample_name);		\
		STATS_CHECK(stats_name, sample_name);			\
	} while (0)


STATS_SAMPLE(one_value) = { 1 };
STATS_EXPECTED(one_value, 1, 1, 1, 1., 1., 0.);

STATS_SAMPLE(five_values) = { 1, 2, 3, 4, 5 };
STATS_EXPECTED(five_values, 5, 1, 5, 15., 3., 1.58113883);

STATS_EXPECTED(zero_values, 0, 0, 0, 0., 0., 0.);

STATS_EXPECTED(million_values, STATS_ONE_MILLION,
	       ULONG_MAX, ULONG_MAX, 1. * ULONG_MAX * STATS_ONE_MILLION,
	       ULONG_MAX, 0.);

STATS_EXPECTED(one_plus_five_values, 6, 1, 5, 16., 16./6, 1.632993161);

static void stats_time_ut(void)
{
	struct m0_net_test_stats stats;
	m0_time_t		 time;
	int			 i;

	m0_net_test_stats_reset(&stats);
	/* sample: .5s, 1.5s, 2.5s, 3.5s, 4.5s */
	for (i = 0; i < 5; ++i) {
		m0_net_test_stats_time_add(&stats, m0_time(i, 500000000));
		stats_serialize_ut(&stats);
	}
	/* check */
	time = m0_net_test_stats_time_min(&stats);
	M0_UT_ASSERT(m0_time_seconds(time) == 0);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 500000000);
	time = m0_net_test_stats_time_max(&stats);
	M0_UT_ASSERT(m0_time_seconds(time) == 4);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 500000000);
#ifndef __KERNEL__
	time = m0_net_test_stats_time_sum(&stats);
	M0_UT_ASSERT(m0_time_seconds(time) == 12);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 500000000);
	time = m0_net_test_stats_time_avg(&stats);
	M0_UT_ASSERT(m0_time_seconds(time) == 2);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 500000000);
	time = m0_net_test_stats_time_stddev(&stats);
	M0_UT_ASSERT(m0_time_seconds(time) == 1);
	M0_UT_ASSERT(581138830 <= m0_time_nanoseconds(time) &&
		     581138840 >= m0_time_nanoseconds(time));
#endif
}

void m0_net_test_stats_ut(void)
{
	struct m0_net_test_stats stats;
	struct m0_net_test_stats stats2;
	int			 i;

	/* test #0: no elements in sample */
	m0_net_test_stats_reset(&stats);
	STATS_CHECK(&stats, zero_values);
	stats_serialize_ut(&stats);
	/* test #1: one value in sample */
	m0_net_test_stats_reset(&stats);
	STATS_ADD_CHECK(&stats, one_value);
	/* test #2: five values in sample */
	m0_net_test_stats_reset(&stats);
	STATS_ADD_CHECK(&stats, five_values);
	/* test #3: one million identical values */
	m0_net_test_stats_reset(&stats);
	for (i = 0; i < STATS_ONE_MILLION; ++i)
		m0_net_test_stats_add(&stats, ULONG_MAX);
	STATS_CHECK(&stats, million_values);
	/* test #4: six values */
	m0_net_test_stats_reset(&stats);
	STATS_SAMPLE_ADD(&stats, one_value);
	STATS_SAMPLE_ADD(&stats, five_values);
	STATS_CHECK(&stats, one_plus_five_values);
	/* test #5: merge two stats */
	m0_net_test_stats_reset(&stats);
	STATS_SAMPLE_ADD(&stats, one_value);
	m0_net_test_stats_reset(&stats2);
	STATS_SAMPLE_ADD(&stats2, five_values);
	m0_net_test_stats_add_stats(&stats, &stats2);
	STATS_CHECK(&stats, one_plus_five_values);
	/* test #6: merge two stats, second is empty */
	m0_net_test_stats_reset(&stats);
	STATS_SAMPLE_ADD(&stats, one_value);
	m0_net_test_stats_reset(&stats2);
	m0_net_test_stats_add_stats(&stats, &stats2);
	STATS_CHECK(&stats, one_value);
	/* test #7: merge two stats, first is empty */
	m0_net_test_stats_reset(&stats);
	m0_net_test_stats_reset(&stats2);
	STATS_SAMPLE_ADD(&stats2, five_values);
	m0_net_test_stats_add_stats(&stats, &stats2);
	STATS_CHECK(&stats, five_values);
	/* test #8: test stats_time functions */
	stats_time_ut();
}

void m0_net_test_timestamp_ut(void)
{
	struct m0_net_test_timestamp ts;
	struct m0_net_test_timestamp ts1;
	m0_time_t		     before;
	m0_time_t		     after;
	m0_bcount_t		     serialized_len;
	m0_bcount_t		     len;
	char			     bv_buf[TIMESTAMP_BUF_LEN];
	void			    *bv_addr = bv_buf;
	m0_bcount_t		     bv_len = TIMESTAMP_BUF_LEN;
	struct m0_bufvec	     bv = M0_BUFVEC_INIT_BUF(&bv_addr, &bv_len);

	before = m0_time_now();
	m0_net_test_timestamp_init(&ts, TIMESTAMP_SEQ);
	after = m0_time_now();

	M0_UT_ASSERT(ts.ntt_time >= before);
	M0_UT_ASSERT(after >= ts.ntt_time);
	M0_UT_ASSERT(ts.ntt_magic == M0_NET_TEST_TIMESTAMP_MAGIC);
	M0_UT_ASSERT(ts.ntt_seq == TIMESTAMP_SEQ);

	serialized_len = m0_net_test_timestamp_serialize(M0_NET_TEST_SERIALIZE,
							 &ts, &bv, 0);
	M0_UT_ASSERT(serialized_len > 0);
	M0_SET0(&ts1);
	len = m0_net_test_timestamp_serialize(M0_NET_TEST_DESERIALIZE,
					      &ts1, &bv, 0);
	M0_UT_ASSERT(serialized_len == len);
	M0_UT_ASSERT(ts.ntt_time == ts1.ntt_time);
	M0_UT_ASSERT(ts1.ntt_magic == ts.ntt_magic);
	M0_UT_ASSERT(ts1.ntt_seq == ts.ntt_seq);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
