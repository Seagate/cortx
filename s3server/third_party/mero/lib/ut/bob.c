/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 09/09/2010
 */

#include "lib/ub.h"
#include "ut/ut.h"
#include "lib/tlist.h"
#include "lib/bob.h"

enum {
	N = 256
};

struct foo {
	void           *f_payload;
	struct m0_tlink f_linkage;
	char            f_x[7];
	uint64_t        f_magix;
};

enum {
	magix = 0xbeda551edcaca0ffULL
};

M0_TL_DESCR_DEFINE(foo, "foo-s", static, struct foo, f_linkage,
		   f_magix, magix, 0);
M0_TL_DEFINE(foo, static, struct foo);

static struct m0_bob_type foo_bob;
static struct foo F;
static struct foo rank[N];

M0_BOB_DEFINE(static, &foo_bob, foo);

static void test_tlist_init(void)
{
	M0_SET0(&foo_bob);
	m0_bob_type_tlist_init(&foo_bob, &foo_tl);
	M0_UT_ASSERT(!strcmp(foo_bob.bt_name, foo_tl.td_name));
	M0_UT_ASSERT(foo_bob.bt_magix == magix);
	M0_UT_ASSERT(foo_bob.bt_magix_offset == foo_tl.td_link_magic_offset);
}

static void test_bob_init(void)
{
	foo_bob_init(&F);
	M0_UT_ASSERT(F.f_magix == magix);
	M0_UT_ASSERT(foo_bob_check(&F));
}

static void test_bob_fini(void)
{
	foo_bob_fini(&F);
	M0_UT_ASSERT(F.f_magix == 0);
	M0_UT_ASSERT(!foo_bob_check(&F));
}

static void test_tlink_init(void)
{
	foo_tlink_init(&F);
	M0_UT_ASSERT(foo_bob_check(&F));
}

static void test_tlink_fini(void)
{
	foo_tlink_fini(&F);
	M0_UT_ASSERT(foo_bob_check(&F));
	F.f_magix = 0;
	M0_UT_ASSERT(!foo_bob_check(&F));
}

static bool foo_check(const void *bob)
{
	const struct foo *f = bob;

	return f->f_payload == f + 1;
}

static void test_check(void)
{
	int i;

	foo_bob.bt_check = &foo_check;

	for (i = 0; i < N; ++i) {
		foo_bob_init(&rank[i]);
		rank[i].f_payload = rank + i + 1;
	}

	for (i = 0; i < N; ++i)
		M0_UT_ASSERT(foo_bob_check(&rank[i]));

	for (i = 0; i < N; ++i)
		foo_bob_fini(&rank[i]);

	for (i = 0; i < N; ++i)
		M0_UT_ASSERT(!foo_bob_check(&rank[i]));

}

static void test_bob_of(void)
{
	void *p;
	int   i;

	foo_bob_init(&F);
	for (i = -1; i < ARRAY_SIZE(F.f_x) + 3; ++i) {
		p = &F.f_x[i];
		M0_UT_ASSERT(bob_of(p, struct foo, f_x[i], &foo_bob) == &F);
	}
}

void test_bob(void)
{
	test_tlist_init();
	test_bob_init();
	test_bob_fini();
	test_tlink_init();
	test_tlink_fini();
	test_check();
	test_bob_of();
	/*
	 * Some of the above tests make an unsuccessful m0_bob_check(),
	 * setting m0_failed_condition. Don't let m0_panic() use it.
	 */
	m0_failed_condition = NULL;
}
M0_EXPORTED(test_bob);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
