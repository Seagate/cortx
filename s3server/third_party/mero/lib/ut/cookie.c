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
 * Original author: Nachiket Sahasrabudhe <Nachiket_Sahasrabudhe@xyratex.com>
 * Original creation date: 30/07/2012
 */

/**
 * This file has seven test-cases in all. Out of these seven cases,
 * six cases deal with cookie-APIs, and one case tests the function
 * m0_addr_is_sane.
 * The six cases related to cookie-APIs are as below:
 * Test No.0: Test for a valid cookie.
 * Test No.1: Test for a cookie holding a NULL pointer.
 * Test No.2: Test for a cookie with address less than 4096.
 * Test No.3: Test for a cookie with unaligned address.
 * Test No.4: Test for a stale cookie.
 * Test No.5: Test for APIs related to cookie-initialization.
 * */

#ifndef __KERNEL__
#include <unistd.h> /* sbrk(0) */
#endif
#include "lib/types.h"
#include "lib/errno.h" /* -EPROTO */
#include "lib/cookie.h"
#include "ut/ut.h"
#include "lib/memory.h" /* M0_ALLOC_PTR */
#include "lib/arith.h" /* M0_IS_8ALIGNED */

struct obj_struct {
	uint64_t os_val;
};

static uint64_t          bss;
static const uint64_t    readonly = 2;
static struct obj_struct obj_bss;

static void test_init_apis(struct m0_cookie *cookie_test, struct obj_struct*
		           obj)
{
	/* Test No.6: Testing of init-apis. */
	m0_cookie_new(&obj->os_val);
	M0_UT_ASSERT(obj->os_val > 0);
	m0_cookie_init(cookie_test, &obj->os_val);
	M0_UT_ASSERT(cookie_test->co_addr ==
		     (uint64_t)&obj->os_val);
	M0_UT_ASSERT(cookie_test->co_generation == obj->os_val);
}

static void test_valid_cookie(struct m0_cookie *cookie_test,
		              struct obj_struct* obj)
{
	int		   flag;
	uint64_t	  *addr_dummy;
	struct obj_struct *obj_retrvd ;

	/* Test No.0: Testing m0_cookie_dereference for a valid cookie. */
	flag = m0_cookie_dereference(cookie_test, &addr_dummy);
	M0_UT_ASSERT(flag == 0);
	M0_UT_ASSERT(addr_dummy == &obj->os_val);

	/* Test No.0: Testing of the macro m0_cookie_of(...) for a
	 * valid cookie. */
	obj_retrvd = m0_cookie_of(cookie_test, struct obj_struct, os_val);
	M0_UT_ASSERT(obj_retrvd == obj);
}

static void test_m0_cookie_dereference(struct m0_cookie *cookie_test,
		                       struct obj_struct *obj)
{
	uint64_t *addr_dummy;
	int	  flag;
	char     *fake_ptr;

	/* Test No.1: Testing m0_cookie_dereference when address in a
	 * cookie is a NULL pointer. */
	cookie_test->co_addr = (uint64_t)NULL;
	flag = m0_cookie_dereference(cookie_test, &addr_dummy);
	M0_UT_ASSERT(flag == -EPROTO);

	/* Test No.2: Testing m0_cookie_dereference when address in a
	 * cookie is not greater than 4096. */
	cookie_test->co_addr = 2048;
	flag = m0_cookie_dereference(cookie_test, &addr_dummy);
	M0_UT_ASSERT(flag == -EPROTO);

	/* Test No.3: Testing m0_cookie_dereference when address in a
	 * cookie is not aligned to 8-bytes. */
	fake_ptr = (char *)&obj->os_val;
	fake_ptr++;
	cookie_test->co_addr = (uint64_t)fake_ptr;
	flag = m0_cookie_dereference(cookie_test, &addr_dummy);
	M0_UT_ASSERT(flag == -EPROTO);

	/* Test No.4: Testing m0_cookie_dereference for a stale cookie. */
	m0_cookie_new(&obj->os_val);
	M0_UT_ASSERT(obj->os_val != cookie_test->co_generation);

	/* Restoring an address in cookie, that got tampered in the last Test.
	 */
	cookie_test->co_addr = (uint64_t)&obj->os_val;
	flag = m0_cookie_dereference(cookie_test, &addr_dummy);
	M0_UT_ASSERT(flag == -EPROTO);
}

static void test_m0_cookie_of(struct m0_cookie *cookie_test,
		              struct obj_struct *obj)
{
	struct obj_struct *obj_retrvd;

	/* Test No.1: Testing m0_cookie_of when address in a cookie is a
	 * NULL pointer. */
	cookie_test->co_addr = (uint64_t)NULL;
	obj_retrvd = m0_cookie_of(cookie_test, struct obj_struct, os_val);
	M0_UT_ASSERT(obj_retrvd == NULL);
}

static void addr_sanity(const uint64_t *addr, bool sane, bool aligned)
{
	M0_UT_ASSERT(m0_addr_is_sane(addr) == sane);
	M0_UT_ASSERT(M0_IS_8ALIGNED(addr) == aligned);
	M0_UT_ASSERT(m0_addr_is_sane_and_aligned(addr) == (sane && aligned));
}

void test_cookie(void)
{
	uint64_t	   automatic;
	uint64_t	  *dynamic;
	uint64_t	   i;
	bool		   insane;
	struct m0_cookie   cookie_test;
	struct obj_struct *obj_dynamic;
	struct obj_struct  obj_automatic;
	struct obj_struct *obj_ptrs[3];
	char               not_aligned[sizeof(uint64_t) * 2];

	M0_ALLOC_PTR(dynamic);
	M0_UT_ASSERT(dynamic != NULL);

	/* Address-sanity testing */
	addr_sanity(NULL, false, true);
	addr_sanity((uint64_t*)1, false, false);
	addr_sanity((uint64_t*)8, false, true);
	addr_sanity(&automatic, true, true);
	addr_sanity(dynamic, true, true);
	addr_sanity(&bss, true, true);
	addr_sanity(&readonly, true, true);
	addr_sanity((uint64_t *)&test_cookie, true,
		    M0_IS_8ALIGNED(&test_cookie));
	for (i = 1; i < sizeof(uint64_t); ++i)
		addr_sanity((const uint64_t *)&not_aligned[i], true, false);

	m0_free(dynamic);

	/*
	 * run through address space, checking that m0_addr_is_sane() doesn't
	 * crash.
	 */
	for (i = 1, insane = false; i <= 0xffff; i++) {
		uint64_t word;
		void    *addr;
		bool     sane;

		word = (i & ~0xf) | (i << 16) | (i << 32) | (i << 48);
		addr = (uint64_t *)word;
		sane = m0_addr_is_sane(addr);
#ifndef __KERNEL__
		M0_UT_ASSERT(ergo(addr < sbrk(0), sane));
#endif
		insane |= !sane;
	}

	/* check that at least one really invalid address was tested. */
	M0_UT_ASSERT(insane);

	/*Testing cookie-APIs*/
	M0_ALLOC_PTR(obj_dynamic);
	M0_UT_ASSERT(obj_dynamic != NULL);

	obj_ptrs[0] = obj_dynamic;
	obj_ptrs[1] = &obj_automatic;
	obj_ptrs[2] = &obj_bss;

	for (i = 0; i < 3; ++i) {
		test_init_apis(&cookie_test, obj_ptrs[i]);
		test_valid_cookie(&cookie_test, obj_ptrs[i]);
		test_init_apis(&cookie_test, obj_ptrs[i]);
		test_m0_cookie_dereference(&cookie_test, obj_ptrs[i]);
		test_init_apis(&cookie_test, obj_ptrs[i]);
		test_m0_cookie_of(&cookie_test, obj_ptrs[i]);
	}

	m0_free(obj_dynamic);
}
M0_EXPORTED(test_cookie);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
