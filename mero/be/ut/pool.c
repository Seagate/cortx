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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 28-Sep-2015
 */

#include "lib/memory.h"
#include "lib/misc.h"
#include "ut/ut.h"

#include "be/op.h"
#include "be/pool.h"

enum {
	BE_UT_POOL_ITEM_NR = 100,
	BE_UT_POOL_Q_SIZE  = 50,
};

#define BE_UT_POOL_MAGIC  0xdeadbeef
#define BE_UT_POOL_MAGIC1 UINT64_MAX
#define BE_UT_POOL_MAGIC2 UINT64_MAX
#define BE_UT_POOL_MAGIC3 UINT64_MAX

struct be_ut_pool_item {
	bool                   bupi_in_pool;
	uint64_t               bupi_magic1;
	struct m0_be_pool_item bupi_pool_item;
	uint64_t               bupi_magic2;
	uint64_t               bupi_pool_magic;
	uint64_t               bupi_magic3;
};

M0_BE_POOL_DESCR_DEFINE(ut, "be_pool UT", static, struct be_ut_pool_item,
			bupi_pool_item, bupi_pool_magic, BE_UT_POOL_MAGIC);
M0_BE_POOL_DEFINE(ut, static, struct be_ut_pool_item);

static void be_ut_pool_item_init(struct be_ut_pool_item *item)
{
	item->bupi_magic1  = BE_UT_POOL_MAGIC1;
	item->bupi_magic2  = BE_UT_POOL_MAGIC2;
	item->bupi_magic3  = BE_UT_POOL_MAGIC3;
	item->bupi_in_pool = false;
}

static void be_ut_pool_items_check(struct be_ut_pool_item *items,
				   int                     nr)
{
	int i;

	for (i = 0; i < nr; ++i) {
		M0_UT_ASSERT(items[i].bupi_magic1 == BE_UT_POOL_MAGIC1);
		M0_UT_ASSERT(items[i].bupi_magic2 == BE_UT_POOL_MAGIC2);
		M0_UT_ASSERT(items[i].bupi_magic3 == BE_UT_POOL_MAGIC3);
	}
}

static void be_ut_pool_op_cb(struct m0_be_op *op, void *param)
{
	struct be_ut_pool_item *item;

	item = *(struct be_ut_pool_item **)param;
	M0_UT_ASSERT(item != NULL && item->bupi_in_pool == true);
}

static void be_ut_pool_usecase(int item_nr, int q_size)
{
	struct m0_be_pool        pool = {};
	struct be_ut_pool_item  *items;
	struct be_ut_pool_item **reqs;
	struct be_ut_pool_item  *item;
	struct m0_be_op         *ops;
	int                      i;
	int                      rc;

	struct m0_be_pool_cfg    cfg = {
		.bplc_q_size = q_size,
	};

	M0_UT_ASSERT(q_size <= item_nr);

	rc = ut_be_pool_init(&pool, &cfg);
	M0_UT_ASSERT(rc == 0);

	M0_ALLOC_ARR(items, item_nr);
	M0_UT_ASSERT(items != NULL);
	M0_ALLOC_ARR(reqs, item_nr + q_size);
	M0_UT_ASSERT(reqs != NULL);
	M0_ALLOC_ARR(ops, q_size);
	M0_UT_ASSERT(ops != NULL);

	for (i = 0; i < item_nr; ++i) {
		be_ut_pool_item_init(&items[i]);
		ut_be_pool_add(&pool, &items[i]);
		items[i].bupi_in_pool = true;
	}
	be_ut_pool_items_check(items, item_nr);

	for (i = 0; i < item_nr; ++i) {
		M0_BE_OP_SYNC(op, ut_be_pool_get(&pool, &reqs[i], &op));
		M0_UT_ASSERT(reqs[i] != NULL);
		M0_UT_ASSERT(reqs[i]->bupi_in_pool);
		reqs[i]->bupi_in_pool = false;
	}
	be_ut_pool_items_check(items, item_nr);

	for (i = 0; i < q_size; ++i) {
		m0_be_op_init(&ops[i]);
		m0_be_op_callback_set(&ops[i], be_ut_pool_op_cb,
				      &reqs[item_nr + i], M0_BOS_DONE);
		ut_be_pool_get(&pool, &reqs[item_nr + i], &ops[i]);
	}

	for (i = 0; i < item_nr; ++i) {
		reqs[i]->bupi_in_pool = true;
		ut_be_pool_put(&pool, reqs[i]);
	}
	be_ut_pool_items_check(items, item_nr);

	for (i = 0; i < q_size; ++i) {
		m0_be_op_wait(&ops[i]);
		M0_UT_ASSERT(reqs[item_nr + i] != NULL);
		m0_be_op_fini(&ops[i]);
	}
	for (i = 0; i < q_size; ++i)
		ut_be_pool_put(&pool, reqs[item_nr + i]);
	be_ut_pool_items_check(items, item_nr);

	for (i = 0; i < item_nr; ++i) {
		item = ut_be_pool_del(&pool);
		M0_UT_ASSERT(item != NULL);
		M0_UT_ASSERT(item->bupi_in_pool);
		item->bupi_in_pool = false;
	}
	item = ut_be_pool_del(&pool);
	M0_UT_ASSERT(item == NULL);
	M0_UT_ASSERT(m0_forall(i, item_nr, !items[i].bupi_in_pool));

	ut_be_pool_fini(&pool);
	be_ut_pool_items_check(items, item_nr);

	m0_free(ops);
	m0_free(reqs);
	m0_free(items);
}

void m0_be_ut_pool_usecase(void)
{
	be_ut_pool_usecase(BE_UT_POOL_ITEM_NR, BE_UT_POOL_Q_SIZE);
	be_ut_pool_usecase(BE_UT_POOL_ITEM_NR, 0);
}

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
