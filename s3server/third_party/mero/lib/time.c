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
 * Original author: Nathan Rutman <Nathan_Rutman@xyratex.com>,
 *                  Huang Hua <Hua_Huang@xyratex.com>
 * Original creation date: 12/10/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include "lib/time.h"
#include "lib/time_internal.h" /* m0_clock_gettime_wrapper */
#include "lib/misc.h"          /* M0_EXPORTED */

/**
   @addtogroup time

   Implementation of time functions on top of all m0_time_t defs

   Time functions can use different clock sources.
   @see M0_CLOCK_SOURCE

   @{
*/

m0_time_t m0_time(uint64_t secs, long ns)
{
	return M0_MKTIME(secs, ns);
}
M0_EXPORTED(m0_time);

m0_time_t m0_time_add(const m0_time_t t1, const m0_time_t t2)
{
	m0_time_t res;

	M0_PRE(M0_TIME_NEVER >= t1);
	M0_PRE(M0_TIME_NEVER >= t2);

	if (t1 == M0_TIME_NEVER || t2 == M0_TIME_NEVER)
		res = M0_TIME_NEVER;
	else
		res = t1 + t2;

	M0_POST(res >= t1);
	M0_POST(res >= t2);
	return res;
}
M0_EXPORTED(m0_time_add);

m0_time_t m0_time_sub(const m0_time_t t1, const m0_time_t t2)
{
	m0_time_t res;
	M0_PRE(M0_TIME_NEVER >= t1);
	M0_PRE(t2 < M0_TIME_NEVER);
	M0_ASSERT_INFO(t1 >= t2,
		       "t1="TIME_F" t2="TIME_F, TIME_P(t1), TIME_P(t2));

	if (t1 == M0_TIME_NEVER)
		res = M0_TIME_NEVER;
	else
		res = t1 - t2;

	M0_POST(t1 >= res);
	return res;
}
M0_EXPORTED(m0_time_sub);

uint64_t m0_time_seconds(const m0_time_t time)
{
	return time / M0_TIME_ONE_SECOND;
}
M0_EXPORTED(m0_time_seconds);

uint64_t m0_time_nanoseconds(const m0_time_t time)
{

        return time % M0_TIME_ONE_SECOND;
}
M0_EXPORTED(m0_time_nanoseconds);

m0_time_t m0_time_from_now(uint64_t secs, long ns)
{
	return m0_time_now() + m0_time(secs, ns);
}
M0_EXPORTED(m0_time_from_now);

bool m0_time_is_in_past(m0_time_t t)
{
	return t < m0_time_now();
}

const m0_time_t M0_TIME_IMMEDIATELY = 0;
const m0_time_t M0_TIME_NEVER       = ~0ULL;
M0_EXPORTED(M0_TIME_IMMEDIATELY);
M0_EXPORTED(M0_TIME_NEVER);

const enum CLOCK_SOURCES M0_CLOCK_SOURCE = M0_CLOCK_SOURCE_REALTIME_MONOTONIC;
m0_time_t		 m0_time_monotonic_offset;

M0_INTERNAL int m0_time_init(void)
{
	m0_time_t realtime;
	m0_time_t monotonic;

	if (M0_CLOCK_SOURCE == M0_CLOCK_SOURCE_REALTIME_MONOTONIC) {
		monotonic = m0_clock_gettime_wrapper(M0_CLOCK_SOURCE_MONOTONIC);
		realtime  = m0_clock_gettime_wrapper(M0_CLOCK_SOURCE_REALTIME);
		m0_time_monotonic_offset = realtime - monotonic;
		if (m0_time_monotonic_offset == 0)
			m0_time_monotonic_offset = 1;
	}
	return 0;
} M0_EXPORTED(m0_time_init);

M0_INTERNAL void m0_time_fini(void)
{
} M0_EXPORTED(m0_time_fini);

m0_time_t m0_time_now(void)
{
	m0_time_t      result;

	switch (M0_CLOCK_SOURCE) {
	case M0_CLOCK_SOURCE_REALTIME_MONOTONIC:
		M0_PRE(m0_time_monotonic_offset != 0);
		result = m0_clock_gettime_wrapper(M0_CLOCK_SOURCE_MONOTONIC) +
			 m0_time_monotonic_offset;
		break;
	case M0_CLOCK_SOURCE_GTOD:
		result = m0_clock_gettimeofday_wrapper();
		break;
	case M0_CLOCK_SOURCE_REALTIME:
	case M0_CLOCK_SOURCE_MONOTONIC:
	case M0_CLOCK_SOURCE_MONOTONIC_RAW:
		result = m0_clock_gettime_wrapper(M0_CLOCK_SOURCE);
		break;
	default:
		M0_IMPOSSIBLE("Unknown clock source");
		result = M0_TIME_NEVER;
	};
	return result;
}
M0_EXPORTED(m0_time_now);

M0_INTERNAL m0_time_t m0_time_to_realtime(m0_time_t abs_time)
{
	m0_time_t source_time;
	m0_time_t realtime;
	m0_time_t monotonic;

	if (abs_time != M0_TIME_NEVER && abs_time != 0) {
		switch (M0_CLOCK_SOURCE) {
		case M0_CLOCK_SOURCE_MONOTONIC:
		case M0_CLOCK_SOURCE_MONOTONIC_RAW:
			source_time = m0_clock_gettime_wrapper(M0_CLOCK_SOURCE);
			realtime    =
			     m0_clock_gettime_wrapper(M0_CLOCK_SOURCE_REALTIME);
			abs_time   += realtime - source_time;
			break;
		case M0_CLOCK_SOURCE_REALTIME_MONOTONIC:
			monotonic =
			    m0_clock_gettime_wrapper(M0_CLOCK_SOURCE_MONOTONIC);
			realtime  =
			    m0_clock_gettime_wrapper(M0_CLOCK_SOURCE_REALTIME);
			/* get monotonic time */
			abs_time -= m0_time_monotonic_offset;
			/* add offset for realtime */
			abs_time += realtime - monotonic;
			/* It will mitigate time jumps between call
			 * to m0_time_now() and call to this function. */
			break;
		case M0_CLOCK_SOURCE_GTOD:
		case M0_CLOCK_SOURCE_REALTIME:
			break;
		default:
			M0_IMPOSSIBLE("Unknown clock source");
			abs_time = 0;
		}
	}
	return abs_time;
} M0_EXPORTED(m0_time_to_realtime);

/** @} end of time group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
