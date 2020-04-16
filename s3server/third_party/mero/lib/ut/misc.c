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
 * Original creation date: 08/22/2012
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "lib/types.h"   /* m0_uint128 */
#include "lib/string.h"  /* m0_startswith */
#include "lib/time.h"    /* m0_time_ t */
#include "lib/memory.h"  /* M0_ALLOC_ARR */
#include "lib/arith.h"   /* m0_rnd */
#include "ut/ut.h"       /* M0_UT_ASSERT */

#define BM_COMMON_STR "bm-majority-algorithm"

enum {
	BM_MJR_EVEN_ARR_LEN = 1000,
	BM_MJR_ODD_ARR_LEN,
};

static const struct m0_uint128 zero     = M0_UINT128(0, 0);
static const struct m0_uint128 one      = M0_UINT128(0, 1);
static const struct m0_uint128 two      = M0_UINT128(0, 2);
static const struct m0_uint128 three    = M0_UINT128(0, 3);
static const struct m0_uint128 cmax64   = M0_UINT128(0, UINT64_MAX);
static const struct m0_uint128 cmax64_1 = M0_UINT128(1, 0);
static const struct m0_uint128 cmax64_2 = M0_UINT128(1, 1);
static const struct m0_uint128 cmax128  = M0_UINT128(UINT64_MAX, UINT64_MAX);

static void uint128_add_check(const struct m0_uint128 *a,
			      const struct m0_uint128 *b,
			      const struct m0_uint128 *sum)
{
	struct m0_uint128 result;

	m0_uint128_add(&result, a, b);
	M0_UT_ASSERT(m0_uint128_eq(&result, sum));

	m0_uint128_add(&result, b, a);
	M0_UT_ASSERT(m0_uint128_eq(&result, sum));
}

static void uint128_add_ut(void)
{
	uint128_add_check(&zero,    &zero,     &zero);
	uint128_add_check(&zero,    &one,      &one);
	uint128_add_check(&one,     &one,      &two);
	uint128_add_check(&one,     &two,      &three);
	uint128_add_check(&cmax64,  &zero,     &cmax64);
	uint128_add_check(&cmax64,  &one,      &cmax64_1);
	uint128_add_check(&cmax64,  &two,      &cmax64_2);
	uint128_add_check(&cmax128, &one,      &zero);
	uint128_add_check(&cmax128, &two,      &one);
	uint128_add_check(&cmax128, &three,    &two);
	uint128_add_check(&cmax128, &cmax64_1, &cmax64);
	uint128_add_check(&cmax128, &cmax64_2, &cmax64_1);
}

/* a * b = c */
static void uint128_mul_check(uint64_t a,
			      uint64_t b,
			      const struct m0_uint128 *c)
{
	struct m0_uint128 result;

	m0_uint128_mul64(&result, a, b);
	M0_UT_ASSERT(m0_uint128_eq(&result, c));
}

static void uint128_mul_check1(uint64_t a,
			       uint64_t b,
			       const struct m0_uint128 *c)
{
	uint128_mul_check(a, b, c);
	uint128_mul_check(b, a, c);
}

static void uint128_mul_ut(void)
{
	uint128_mul_check1(0, 0, &zero);
	uint128_mul_check1(0, 1, &zero);
	uint128_mul_check1(0, UINT64_MAX, &zero);
	uint128_mul_check1(1, 1, &one);
	uint128_mul_check1(1, 2, &two);
	uint128_mul_check1(1, UINT64_MAX, &cmax64);
	uint128_mul_check1(2, UINT64_MAX, &M0_UINT128(1, UINT64_MAX - 1));
	uint128_mul_check1(3, UINT64_MAX, &M0_UINT128(2, UINT64_MAX - 2));
	uint128_mul_check1(UINT64_MAX, UINT64_MAX,
			   &M0_UINT128(UINT64_MAX - 1, 1));
	uint128_mul_check1(UINT32_MAX + 1ul, UINT32_MAX + 1ul, &cmax64_1);
	uint128_mul_check1(UINT32_MAX + 1ul, UINT64_MAX,
			   &M0_UINT128(UINT32_MAX,
				       (uint64_t) UINT32_MAX << 32));
	uint128_mul_check1(UINT32_MAX + 2ul, UINT32_MAX, &cmax64);
}

static void test_forall_exists(void)
{
	const char s[] = "0123456789";

	M0_UT_ASSERT(m0_forall(i, sizeof s, s[i] != 'a'));
	M0_UT_ASSERT(m0_exists(i, sizeof s, s[i] == '0'));
	M0_UT_ASSERT(m0_exists(i, sizeof s, s[i] == '5'));
	M0_UT_ASSERT(m0_exists(i, sizeof s, s[i] == '9'));
	M0_UT_ASSERT(!m0_exists(i, sizeof s, s[i] == 'a'));
}

static void test_str_startswith(void)
{
	const char s[] = "foobar";

	M0_UT_ASSERT(m0_startswith("foo", s));
	M0_UT_ASSERT(m0_startswith("f", s));
	M0_UT_ASSERT(!m0_startswith("bar", s));
	M0_UT_ASSERT(!m0_startswith("foobarbaz", s));
	M0_UT_ASSERT(m0_startswith("", s));
	M0_UT_ASSERT(m0_startswith("", ""));
}

static void test_majority_ident_arr(void)
{
	struct m0_key_val *kv_arr;
	struct m0_key_val *mjr;
	uint32_t          *key_arr;
	struct m0_buf     *val_arr;
	m0_time_t          seed;
	uint32_t           i;
	uint32_t           vote_nr;

	M0_ALLOC_ARR(kv_arr, BM_MJR_ODD_ARR_LEN);
	M0_UT_ASSERT(kv_arr != NULL);
	M0_ALLOC_ARR(val_arr, BM_MJR_ODD_ARR_LEN);
	M0_UT_ASSERT(val_arr != NULL);
	M0_ALLOC_ARR(key_arr, BM_MJR_ODD_ARR_LEN);
	M0_UT_ASSERT(key_arr != NULL);

	for (i = 0; i < BM_MJR_ODD_ARR_LEN; ++i) {
		m0_buf_init(&val_arr[i], BM_COMMON_STR, strlen(BM_COMMON_STR));
	}
	for (i = 0; i < BM_MJR_ODD_ARR_LEN; ++i) {
		key_arr[i] = i;
	}
	for (i = 0; i < BM_MJR_ODD_ARR_LEN; ++i) {
		m0_key_val_init(&kv_arr[i], &M0_BUF_INIT_PTR(&key_arr[i]),
				&val_arr[i]);
	}
	vote_nr = 0;
	mjr = m0_vote_majority_get(kv_arr, BM_MJR_ODD_ARR_LEN, m0_buf_eq,
				   &vote_nr);
	M0_UT_ASSERT(mjr != NULL);
	M0_UT_ASSERT(vote_nr == BM_MJR_ODD_ARR_LEN);

	/* Since all values are identical, pick a random one to compare. */
	seed = m0_time_now();
	i = m0_rnd(BM_MJR_ODD_ARR_LEN, &seed);
	M0_UT_ASSERT(m0_buf_eq(&kv_arr[i].kv_val, &mjr->kv_val));
	m0_free(key_arr);
	m0_free(val_arr);
	m0_free(kv_arr);
}

static void test_majority_dist_val(void)
{
	struct m0_key_val *kv_arr;
	struct m0_key_val *mjr;
	uint32_t          *key_arr;
	uint32_t          *val_arr;
	uint32_t           i;
	uint32_t           vote_nr;

	M0_ALLOC_ARR(kv_arr, BM_MJR_ODD_ARR_LEN);
	M0_UT_ASSERT(kv_arr != NULL);
	M0_ALLOC_ARR(val_arr, BM_MJR_ODD_ARR_LEN);
	M0_UT_ASSERT(val_arr != NULL);
	M0_ALLOC_ARR(key_arr, BM_MJR_ODD_ARR_LEN);
	M0_UT_ASSERT(key_arr != NULL);

	for (i = 0; i < BM_MJR_ODD_ARR_LEN; ++i) {
		val_arr[i] = i;
	}
	for (i = 0; i < BM_MJR_ODD_ARR_LEN; ++i) {
		key_arr[i] = i;
	}
	for (i = 0; i < BM_MJR_ODD_ARR_LEN; ++i) {
		m0_key_val_init(&kv_arr[i], &M0_BUF_INIT_PTR(&key_arr[i]),
				&M0_BUF_INIT_PTR(&val_arr[i]));
	}
	vote_nr = 0;
	mjr = m0_vote_majority_get(kv_arr, BM_MJR_ODD_ARR_LEN, m0_buf_eq,
				   &vote_nr);
	M0_UT_ASSERT(mjr == NULL);
	M0_UT_ASSERT(vote_nr == 1);

	m0_free(key_arr);
	m0_free(val_arr);
	m0_free(kv_arr);
}

static void test_majority_tie(void)
{
	struct m0_key_val *kv_arr;
	struct m0_key_val *mjr;
	uint32_t          *key_arr;
	uint32_t          *val_arr;
	uint32_t           i;
	uint32_t           vote_nr;

	M0_ALLOC_ARR(kv_arr, BM_MJR_EVEN_ARR_LEN);
	M0_UT_ASSERT(kv_arr != NULL);
	M0_ALLOC_ARR(val_arr, BM_MJR_EVEN_ARR_LEN);
	M0_UT_ASSERT(val_arr != NULL);
	M0_ALLOC_ARR(key_arr, BM_MJR_EVEN_ARR_LEN);
	M0_UT_ASSERT(key_arr != NULL);

	for (i = 0; i < BM_MJR_EVEN_ARR_LEN; ++i) {
		val_arr[i] = i % 2;
	}
	for (i = 0; i < BM_MJR_EVEN_ARR_LEN; ++i) {
		key_arr[i] = i;
	}
	for (i = 0; i < BM_MJR_EVEN_ARR_LEN; ++i) {
		m0_key_val_init(&kv_arr[i], &M0_BUF_INIT_PTR(&key_arr[i]),
				&M0_BUF_INIT_PTR(&val_arr[i]));
	}
	vote_nr = 0;
	mjr = m0_vote_majority_get(kv_arr, BM_MJR_EVEN_ARR_LEN, m0_buf_eq,
				   &vote_nr);
	M0_UT_ASSERT(mjr == NULL);
	M0_UT_ASSERT(vote_nr == BM_MJR_EVEN_ARR_LEN / 2);

	m0_free(key_arr);
	m0_free(val_arr);
	m0_free(kv_arr);
}

static void test_majority_get(void)
{
	test_majority_ident_arr();
	test_majority_dist_val();
	test_majority_tie();
}

void m0_test_misc(void)
{
	uint128_add_ut();
	uint128_mul_ut();
	test_str_startswith();
	test_forall_exists();
	test_majority_get();
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
