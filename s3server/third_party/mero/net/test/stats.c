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
 * Original creation date: 03/22/2012
 */

#ifndef __KERNEL__
#include <limits.h>		/* CHAR_MAX */
#else
#include <linux/kernel.h>	/* INT_MIN */
#endif

#include "lib/misc.h"		/* M0_SET0 */
#include "lib/arith.h"		/* min_check */

#include "mero/magic.h"	/* M0_NET_TEST_TIMESTAMP_MAGIC */

#include "net/test/stats.h"

/**
   @defgroup NetTestStatsInternals Statistics Collector
   @ingroup NetTestInternals

   @see
   @ref net-test

   @{
 */

void m0_net_test_stats_reset(struct m0_net_test_stats *stats)
{
	M0_SET0(stats);
	M0_POST(m0_net_test_stats_invariant(stats));
}

bool m0_net_test_stats_invariant(const struct m0_net_test_stats *stats)
{
	static const struct m0_uint128 zero = {
		.u_hi = 0,
		.u_lo = 0
	};

	if (stats == NULL)
		return false;
	if (stats->nts_count == 0 &&
	    (stats->nts_min != 0 || stats->nts_max != 0 ||
	     !m0_uint128_eq(&stats->nts_sum, &zero) ||
	     !m0_uint128_eq(&stats->nts_sum_sqr, &zero)))
	       return false;
	return true;
}

void m0_net_test_stats_add(struct m0_net_test_stats *stats,
			   unsigned long value)
{
	struct m0_uint128 v128;

	M0_PRE(m0_net_test_stats_invariant(stats));

	stats->nts_count++;
	if (stats->nts_count == 1) {
		stats->nts_min = value;
		stats->nts_max = value;
	} else {
		stats->nts_min = min_check(stats->nts_min, value);
		stats->nts_max = max_check(stats->nts_max, value);
	}
	M0_CASSERT(sizeof value <= sizeof stats->nts_sum.u_hi);
	m0_uint128_add(&stats->nts_sum, &stats->nts_sum,
		       &M0_UINT128(0, value));
	m0_uint128_mul64(&v128, value, value);
	m0_uint128_add(&stats->nts_sum_sqr, &stats->nts_sum_sqr, &v128);
}

void m0_net_test_stats_add_stats(struct m0_net_test_stats *stats,
				 const struct m0_net_test_stats *stats2)
{
	M0_PRE(m0_net_test_stats_invariant(stats));
	M0_PRE(m0_net_test_stats_invariant(stats2));

	if (stats->nts_count == 0) {
		stats->nts_min = stats2->nts_min;
		stats->nts_max = stats2->nts_max;
	} else if (stats2->nts_count != 0) {
		stats->nts_min = min_check(stats->nts_min, stats2->nts_min);
		stats->nts_max = max_check(stats->nts_max, stats2->nts_max);
	}
	stats->nts_count += stats2->nts_count;
	m0_uint128_add(&stats->nts_sum, &stats->nts_sum, &stats2->nts_sum);
	m0_uint128_add(&stats->nts_sum_sqr, &stats->nts_sum_sqr,
		       &stats2->nts_sum_sqr);
}

unsigned long m0_net_test_stats_min(const struct m0_net_test_stats *stats)
{
	M0_PRE(m0_net_test_stats_invariant(stats));

	return stats->nts_min;
}

unsigned long m0_net_test_stats_max(const struct m0_net_test_stats *stats)
{
	M0_PRE(m0_net_test_stats_invariant(stats));

	return stats->nts_max;
}

TYPE_DESCR(m0_net_test_stats) = {
	FIELD_DESCR(struct m0_net_test_stats, nts_count),
	FIELD_DESCR(struct m0_net_test_stats, nts_min),
	FIELD_DESCR(struct m0_net_test_stats, nts_max),
};

TYPE_DESCR(m0_uint128) = {
	FIELD_DESCR(struct m0_uint128, u_hi),
	FIELD_DESCR(struct m0_uint128, u_lo),
};

m0_bcount_t m0_net_test_stats_serialize(enum m0_net_test_serialize_op op,
					struct m0_net_test_stats *stats,
					struct m0_bufvec *bv,
					m0_bcount_t bv_offset)
{
	struct m0_uint128 * const pv128[] = {
		&stats->nts_sum,
		&stats->nts_sum_sqr,
	};
	m0_bcount_t	   len_total;
	m0_bcount_t	   len;
	int		   i;

	len = m0_net_test_serialize(op, stats,
				    USE_TYPE_DESCR(m0_net_test_stats),
				    bv, bv_offset);
	len_total = net_test_len_accumulate(0, len);
	for (i = 0; i < ARRAY_SIZE(pv128) && len_total != 0; ++i) {
		len = m0_net_test_serialize(op, pv128[i],
					    USE_TYPE_DESCR(m0_uint128),
					    bv, bv_offset + len_total);
		len_total = net_test_len_accumulate(len_total, len);
	}
	return len_total;
}

void m0_net_test_stats_time_add(struct m0_net_test_stats *stats, m0_time_t time)
{
	m0_net_test_stats_add(stats, (unsigned long)time);
}

m0_time_t m0_net_test_stats_time_min(struct m0_net_test_stats *stats)
{
	return m0_net_test_stats_min(stats);
}

m0_time_t m0_net_test_stats_time_max(struct m0_net_test_stats *stats)
{
	return m0_net_test_stats_max(stats);
}

/** @} end of NetTestStatsInternals group */

/**
   @defgroup NetTestTimestampInternals Timestamp
   @ingroup NetTestInternals

   @{
 */

void m0_net_test_timestamp_init(struct m0_net_test_timestamp *t, uint64_t seq)
{
	M0_PRE(t != NULL);

	t->ntt_seq   = seq;
	t->ntt_magic = M0_NET_TEST_TIMESTAMP_MAGIC;
	t->ntt_time  = m0_time_now();
}

TYPE_DESCR(m0_net_test_timestamp) = {
	FIELD_DESCR(struct m0_net_test_timestamp, ntt_magic),
	FIELD_DESCR(struct m0_net_test_timestamp, ntt_time),
	FIELD_DESCR(struct m0_net_test_timestamp, ntt_seq),
};

m0_bcount_t m0_net_test_timestamp_serialize(enum m0_net_test_serialize_op op,
					    struct m0_net_test_timestamp *t,
					    struct m0_bufvec *bv,
					    m0_bcount_t bv_offset)
{
	m0_bcount_t len;

	M0_PRE(ergo(op == M0_NET_TEST_DESERIALIZE, t != NULL));

	len = m0_net_test_serialize(op, t,
				    USE_TYPE_DESCR(m0_net_test_timestamp),
				    bv, bv_offset);
	return op == M0_NET_TEST_DESERIALIZE ?
	       t->ntt_magic == M0_NET_TEST_TIMESTAMP_MAGIC ? len : 0 : len;
}

/**
   @} end of NetTestTimestampInternals group
 */

/**
   @defgroup NetTestStatsMPSInternals Messages Per Second Statistics
   @ingroup NetTestInternals

   @{
 */

void m0_net_test_mps_init(struct m0_net_test_mps *mps,
			  unsigned long messages,
			  m0_time_t timestamp,
			  m0_time_t interval)
{
	M0_PRE(mps != NULL);

	m0_net_test_stats_reset(&mps->ntmps_stats);

	mps->ntmps_last_nr	 = messages;
	mps->ntmps_last_time     = timestamp;
	mps->ntmps_time_interval = interval;
}

m0_time_t m0_net_test_mps_add(struct m0_net_test_mps *mps,
			      unsigned long messages,
			      m0_time_t timestamp)
{
	unsigned long		   messages_delta;
	m0_time_t		   time_delta;
	m0_time_t		   time_next;
	uint64_t		   time_delta_ns;
	unsigned long		   m_per_sec;
	unsigned		   i;
	static const struct {
		unsigned long pow;
		unsigned long value;
	}			   pow10[] = {
		{ .pow = 1000000000,	.value = ULONG_MAX / 1000000000 },
		{ .pow = 100000000,	.value = ULONG_MAX / 100000000 },
		{ .pow = 10000000,	.value = ULONG_MAX / 10000000 },
		{ .pow = 1000000,	.value = ULONG_MAX / 1000000 },
		{ .pow = 100000,	.value = ULONG_MAX / 100000 },
		{ .pow = 10000,		.value = ULONG_MAX / 10000 },
		{ .pow = 1000,		.value = ULONG_MAX / 1000 },
		{ .pow = 100,		.value = ULONG_MAX / 100 },
		{ .pow = 10,		.value = ULONG_MAX / 10 },
		{ .pow = 1,		.value = ULONG_MAX },
	};

	M0_PRE(mps != NULL);
	M0_PRE(messages >= mps->ntmps_last_nr);
	M0_PRE(timestamp >= mps->ntmps_last_time);

	messages_delta = messages - mps->ntmps_last_nr;
	time_delta     = m0_time_sub(timestamp, mps->ntmps_last_time);
	time_next      = m0_time_add(mps->ntmps_last_time,
				     mps->ntmps_time_interval);

	if (timestamp < time_next)
		return time_next;

	mps->ntmps_last_nr = messages;
	/** @todo problem with small mps->ntmps_time_interval can be here */
	mps->ntmps_last_time  = time_next;

	time_delta_ns = m0_time_seconds(time_delta) * M0_TIME_ONE_SECOND +
			m0_time_nanoseconds(time_delta);
	/*
	   To measure bandwidth in messages/sec it needs to be calculated
	   (messages_delta / time_delta_ns) * 1'000'000'000 =
	   (messages_delta * 1'000'000'000) / time_delta_ns =
	   ((messages_delta * (10^M)) / time_delta_ns) * (10^(9-M)),
	   where M is some parameter. To perform integer division M
	   should be maximized in range [0, 9] - in case if M < 9
	   there is a loss of precision.
	 */
	for (i = 0; i < ARRAY_SIZE(pow10); ++i) {
		if (messages_delta <= pow10[i].value)
			break;
	}
	m_per_sec = (messages_delta * pow10[i].pow / time_delta_ns) *
		    (M0_TIME_ONE_SECOND / pow10[i].pow);
	m0_net_test_stats_add(&mps->ntmps_stats, m_per_sec);

	return time_next;
}

/**
   @} end of NetTestStatsMPSInternals group
 */

/**
   @defgroup NetTestStatsMsgNRInternals Messages Number
   @ingroup NetTestInternals

   @{
 */

void m0_net_test_msg_nr_add(struct m0_net_test_msg_nr *msg_nr,
			    const struct m0_net_test_msg_nr *msg_nr2)
{
	M0_PRE(msg_nr != NULL);
	M0_PRE(msg_nr2 != NULL);

	msg_nr->ntmn_total  += msg_nr2->ntmn_total;
	msg_nr->ntmn_failed += msg_nr2->ntmn_failed;
	msg_nr->ntmn_bad    += msg_nr2->ntmn_bad;
}

/**
   @} end of NetTestStatsMsgNRInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
