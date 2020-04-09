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
 * Original creation date: 08/18/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_USER_SPACE_STATS_U_H__
#define __MERO_NET_TEST_USER_SPACE_STATS_U_H__

#include "lib/time.h"		/* m0_time_t */
#include "net/test/stats.h"	/* m0_net_test_stats */

/**
   @addtogroup NetTestStatsDFS

   @{
 */

struct m0_net_test_stats;

/**
   Get sum of all elements from sample.
   @pre m0_net_test_stats_invariant(stats)
 */
double m0_net_test_stats_sum(const struct m0_net_test_stats *stats);

/**
   Get sample average (arithmetic mean).
   @pre m0_net_test_stats_invariant(stats)
 */
double m0_net_test_stats_avg(const struct m0_net_test_stats *stats);

/**
   Get sample standard deviation.
   @pre m0_net_test_stats_invariant(stats)
 */
double m0_net_test_stats_stddev(const struct m0_net_test_stats *stats);

/**
   @see m0_net_test_stats_time_add()
 */
m0_time_t m0_net_test_stats_time_sum(struct m0_net_test_stats *stats);

/**
   @see m0_net_test_stats_time_add()
 */
m0_time_t m0_net_test_stats_time_avg(struct m0_net_test_stats *stats);

/**
   @see m0_net_test_stats_time_add()
 */
m0_time_t m0_net_test_stats_time_stddev(struct m0_net_test_stats *stats);

/**
   @} end of NetTestStatsDFS group
 */

#endif /*  __MERO_NET_TEST_USER_SPACE_STATS_U_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
