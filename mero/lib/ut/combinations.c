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

#include "lib/misc.h" /* ARRAY_SIZE */
#include "lib/combinations.h"
#include "ut/ut.h"

void test_combinations(void)
{
	enum { NR_COMBINATIONS = 35 };
	int A[] = { 0, 1, 2, 3, 4, 5, 6 };
	int X[] = { 2, 4, 6 };
	int comb[] = { 0, 0, 0 };
	int N = ARRAY_SIZE(A);
	int K = ARRAY_SIZE(X);
	int cid;
	int i;

	M0_UT_ASSERT(m0_fact(K) == 6);
	M0_UT_ASSERT(m0_fact(N) / (m0_fact(K) * m0_fact(N - K)) ==
		     m0_ncr(N, K));

	cid = m0_combination_index(N, K, X);
	M0_UT_ASSERT(cid == 29);
	m0_combination_inverse(cid, N, K, comb);
	M0_UT_ASSERT(m0_forall(j, K, X[j] == comb[j]));

	M0_UT_ASSERT(m0_ncr(N, K) == NR_COMBINATIONS);
	for (i = 0; i < NR_COMBINATIONS; i++) {
		m0_combination_inverse(i, N, K, comb);
		cid = m0_combination_index(N, K, comb);
		M0_UT_ASSERT(cid == i);
	}
}
M0_EXPORTED(test_combinations);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

