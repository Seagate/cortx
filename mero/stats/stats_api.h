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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 * Original creation date: 09/16/2013
 */
#pragma once

#ifndef __MERO_STATS_UTIL_STATS_API_H__
#define __MERO_STATS_UTIL_STATS_API_H__
/**
 * @defgroup stats_api Stats Query API
 * This module provide stats query interfaces. These interfaces are used by
 * mero monitoring/administrating utilities/console.
 *
 * Interfaces
 *   m0_stats_query
 *   m0_stats_free
 *
 * @{
 */
struct m0_uint64_seq;
struct m0_stats_recs;
struct m0_rpc_session;

/**
 * Stats query API
 * It retrive stats from stats service of provided stats ids.
 * @param session   The session to be used for the stats query.
 * @param stats_ids Sequence of stats ids.
 * @param stats On success, stats information is returned here.  It must
 *              be released using m0_stats_free().
 * @retval Pointer to stats return by stats service. This should be freed by
 *         caller after use. It can be freed using m0_stats_free().
 */
int m0_stats_query(struct m0_rpc_session  *session,
		   struct m0_uint64_seq   *stats_ids,
		   struct m0_stats_recs  **stats);

/**
 * Free stats sequence
 * It frees stats sequence returned by m0_stats_query().
 * @param stats Stats sequence.
 */
void m0_stats_free(struct m0_stats_recs *stats);

/** @} end group stats_api */
#endif
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
