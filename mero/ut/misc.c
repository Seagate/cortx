/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 26-Sep-2015
 */

/**
 * @addtogroup ut
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ut/misc.h"

#include "lib/arith.h"  /* M0_SWAP */
#include "lib/misc.h"   /* m0_reduce */

M0_INTERNAL void m0_ut_random_shuffle(uint64_t *arr,
				      uint64_t  nr,
				      uint64_t *seed)
{
	uint64_t i;

	for (i = nr - 1; i > 0; --i)
		M0_SWAP(arr[i], arr[m0_rnd64(seed) % (i + 1)]);
}

M0_INTERNAL void m0_ut_random_arr_with_sum(uint64_t *arr,
					   uint64_t  nr,
					   uint64_t  sum,
					   uint64_t *seed)
{
	uint64_t split;
	uint64_t sum_split;

	M0_PRE(nr > 0);
	if (nr == 1) {
		arr[0] = sum;
	} else {
		split = m0_rnd64(seed) % (nr - 1) + 1;
		sum_split = sum == 0 ? 0 : m0_rnd64(seed) % sum;
		m0_ut_random_arr_with_sum(&arr[0], split, sum_split, seed);
		m0_ut_random_arr_with_sum(&arr[split],
					  nr - split, sum - sum_split, seed);
	}
	M0_POST_EX(m0_reduce(i, nr, 0, + arr[i]) == sum);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of ut group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
