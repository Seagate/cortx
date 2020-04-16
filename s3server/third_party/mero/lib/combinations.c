/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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
 * Original author: Madhavrao Vemuri <madhav.vemuri@seagate.com>
 *                  Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
 * Original creation date: 10/06/2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @addtogroup comb Combinations
   @{
*/

M0_INTERNAL uint64_t m0_fact(uint64_t n)
{
	M0_PRE(n > 0);

	return n == 1 || n == 0 ? 1 : n * m0_fact(n - 1);
}

M0_INTERNAL uint32_t m0_ncr(uint64_t n, uint64_t r)
{
	uint64_t i;
	uint64_t m = n;

	M0_PRE(n >= r);

	if (r == 0)
		return 1;
	for (i = 1; i < r; i++)
		m *= n - i;
	return m / m0_fact(r);
}

M0_INTERNAL int m0_combination_index(int N, int K, int *x)
{
	int m;
	int q;
	int n;
	int r;
	int idx = 0;

	M0_ENTRY("N:%d K=%d", N, K);
	M0_PRE(0 < K && K <= N);
	M0_PRE(m0_forall(i, K, x[i] < N));

	for (q = 0; q < x[0]; q++) {
		n = N - (q + 1);
		r = K - 1;
		idx += m0_ncr(n, r);
	}
	for (m = 1; m < K; m++) {
		for (q = 0; q < (x[m] - x[m - 1] - 1); q++) {
			n = N - (x[m - 1] + 1) - (q + 1);
			r = K - m - 1;
			idx += m0_ncr(n, r);
		}
	}
	return M0_RC(idx);
}

M0_INTERNAL void m0_combination_inverse(int cid, int N, int K, int *x)
{
	int m;
	int q;
	int n;
	int r;
	int idx = 0;
	int old_idx = 0;
	int i = 0;
	int j;

	M0_ENTRY("N:%d K=%d cid:%d \n", N, K, cid);
	M0_PRE(0 < K && K <= N);

	for (q = 0; idx < cid + 1; q++) {
		old_idx = idx;
		n = N - (q + 1);
		r = K - 1;
		idx += m0_ncr(n, r);
	}
	idx = old_idx;
	x[i++] = q - 1;

	for (m = 1; m < K; m++) {
		for (q = 0; idx < cid + 1; q++) {
			old_idx = idx;
			n = N - (x[i - 1] + 1) - (q + 1);
			r = K - m - 1;
			idx += m0_ncr(n, r);
		}
		if (idx >= cid + 1)
			idx = old_idx;
		x[i] = x[i - 1] + q;
		i++;
	}

	M0_LOG(M0_DEBUG, "Combinations");
	for (j = 0; j < i; j++)
		M0_LOG(M0_DEBUG, "%d \t", x[j]);
	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of comb group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
