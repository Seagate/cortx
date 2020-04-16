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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 30-May-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/list.h"

#include "ut/ut.h"              /* M0_UT_ASSERT */

#include "lib/misc.h"           /* M0_SET0 */

#include "be/ut/helper.h"       /* m0_be_ut_backend_init */

/* -------------------------------------------------------------------------
 * Descriptors and stuff
 * ------------------------------------------------------------------------- */
enum {
	TEST_MAGIC      = 0x331fefefefefe177,
	TEST_LINK_MAGIC = 0x331acacacacac277
};

struct test {
	uint64_t               t_magic;
	struct m0_be_list_link t_linkage;
	int                    t_payload;
};

M0_BE_LIST_DESCR_DEFINE(test, "test:m0-be-list", static, struct test,
		   t_linkage, t_magic, TEST_MAGIC, TEST_LINK_MAGIC);
M0_BE_LIST_DEFINE(test, static, struct test);

/* -------------------------------------------------------------------------
 * List construction test
 * ------------------------------------------------------------------------- */

static void check(struct m0_be_list *list, struct m0_be_seg *seg);
M0_UNUSED static void print(struct m0_be_list *list);

M0_INTERNAL void m0_be_ut_list(void)
{
	enum { SHIFT = 0 };
	/* Following variables are static to reduce kernel stack consumption. */
	static struct m0_be_tx_credit   cred_add = {};
	static struct m0_be_list       *list;
	static struct m0_be_ut_backend  ut_be;
	static struct m0_be_ut_seg      ut_seg;
	static struct m0_be_seg        *seg;
	static struct test             *elem[10];
	int                             i;

	M0_ENTRY();

	M0_SET0(&ut_be);
	/* Init BE. */
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1ULL << 16);
	seg = ut_seg.bus_seg;

	M0_BE_UT_ALLOC_PTR(&ut_be, &ut_seg, list);
	for (i = 0; i < ARRAY_SIZE(elem); ++i)
		M0_BE_UT_ALLOC_PTR(&ut_be, &ut_seg, elem[i]);

	M0_BE_UT_TRANSACT(&ut_be, tx, cred,
		  test_be_list_credit(M0_BLO_CREATE, 1, &cred),
		  test_be_list_create(list, tx));

	/* Perform some operations over the list. */

	for (i = 0; i < ARRAY_SIZE(elem); ++i) {
		M0_BE_UT_TRANSACT(&ut_be, tx, cred,
		  test_be_list_credit(M0_BLO_TLINK_CREATE, 1, &cred),
		  test_be_tlink_create(elem[i], tx));
	}
	/* add */
	test_be_list_credit(M0_BLO_ADD, 1, &cred_add);
	for (i = 0; i < ARRAY_SIZE(elem); ++i) {
		M0_BE_UT_TRANSACT(&ut_be, tx, cred,
				  cred = M0_BE_TX_CREDIT_PTR(elem[i]),
				  (elem[i]->t_payload = i,
				   M0_BE_TX_CAPTURE_PTR(seg, tx, elem[i])));

		if (i < ARRAY_SIZE(elem) / 2) {
			if (i % 2 == 0) {
				M0_BE_UT_TRANSACT(&ut_be, tx, cred,
						  cred = cred_add,
				 test_be_list_add(list, tx, elem[i]));
			} else {
				M0_BE_UT_TRANSACT(&ut_be, tx, cred,
						  cred = cred_add,
				 test_be_list_add_tail(list, tx, elem[i]));
			}
		} else {
			if (i % 2 == 0) {
				M0_BE_UT_TRANSACT(&ut_be, tx, cred,
						  cred = cred_add,
				 test_be_list_add_after(list, tx,
						      elem[i - 1], elem[i]));
			} else {
				M0_BE_UT_TRANSACT(&ut_be, tx, cred,
						  cred = cred_add,
				 test_be_list_add_before(list, tx,
						       elem[i - 1], elem[i]));
			}
		}
	}

	/* delete */
	for (i = 0; i < ARRAY_SIZE(elem); ++i) {
		if (!M0_IN(i, (0, 2, 7, 9)))
			continue;

		M0_BE_UT_TRANSACT(&ut_be, tx, cred,
		  test_be_list_credit(M0_BLO_DEL, 1, &cred),
		  test_be_list_del(list, tx, elem[i]));
	}

	/* Reload segment and check data. */
	m0_be_ut_seg_reload(&ut_seg);

	check(list, seg);

	for (i = 0; i < ARRAY_SIZE(elem); ++i) {
		if (M0_IN(i, (0, 2, 7, 9)))
			continue;

		M0_BE_UT_TRANSACT(&ut_be, tx, cred,
		  test_be_list_credit(M0_BLO_DEL, 1, &cred),
		  test_be_list_del(list, tx, elem[i]));
	}

	for (i = 0; i < ARRAY_SIZE(elem); ++i) {
		M0_BE_UT_TRANSACT(&ut_be, tx, cred,
		  test_be_list_credit(M0_BLO_TLINK_DESTROY, 1, &cred),
		  test_be_tlink_destroy(elem[i], tx));
	}
	M0_BE_UT_TRANSACT(&ut_be, tx, cred,
		  test_be_list_credit(M0_BLO_DESTROY, 1, &cred),
		  test_be_list_destroy(list, tx));

	for (i = 0; i < ARRAY_SIZE(elem); ++i)
		M0_BE_UT_FREE_PTR(&ut_be, &ut_seg, elem[i]);
	M0_BE_UT_FREE_PTR(&ut_be, &ut_seg, list);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);

	M0_LEAVE();
}

/* -------------------------------------------------------------------------
 * List reloading test
 * ------------------------------------------------------------------------- */

static void check(struct m0_be_list *list, struct m0_be_seg *seg)
{
	struct test *test;
	int          expected[] = { 5, 8, 6, 4, 1, 3 };
	int          i = 0;

	m0_be_list_for(test, list, test) {
		M0_UT_ASSERT(i < ARRAY_SIZE(expected));
		M0_UT_ASSERT(expected[i++] == test->t_payload);
	} m0_be_list_endfor;
}

M0_UNUSED static void print(struct m0_be_list *list)
{
	struct test *test;

	M0_LOG(M0_DEBUG, "----------");
	m0_be_list_for(test, list, test) {
		M0_LOG(M0_DEBUG, "-- %d", test->t_payload);
	} m0_be_list_endfor;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
