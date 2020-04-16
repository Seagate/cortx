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
 * Original creation date: 6-Sep-2015
 */


/**
 * @addtogroup be
 *
 * * Future directions
 * - concurrent test for auto put()
 * - test discard correctness
 * - test discard for empty I/Os
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/log_discard.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/time.h"           /* M0_TIME_ONE_SECOND */
#include "lib/locality.h"       /* m0_locality0_get */
#include "lib/atomic.h"         /* m0_atomic64 */

#include "be/op.h"              /* M0_BE_OP_SYNC */

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "ut/threads.h"         /* M0_UT_THREADS_DEFINE */

enum {
	BE_UT_LOG_DISCARD_USECASE_ITEMS_MAX       = 0x10,
	BE_UT_LOG_DISCARD_USECASE_ITEMS_THRESHOLD = 0x8,
};

static void be_ut_log_discard_usecase_sync(struct m0_be_log_discard      *ld,
                                           struct m0_be_op               *op,
                                           struct m0_be_log_discard_item *ldi)
{
	m0_be_op_active(op);
	m0_be_op_done(op);
}

static void
be_ut_log_discard_usecase_discard(struct m0_be_log_discard      *ld,
                                  struct m0_be_log_discard_item *ldi)
{
	struct m0_semaphore *sem = m0_be_log_discard_item_user_data(ldi);

	m0_semaphore_up(sem);
}

void m0_be_ut_log_discard_usecase(void)
{
	struct m0_be_log_discard_cfg    ld_cfg = {
		.ldsc_sync              = &be_ut_log_discard_usecase_sync,
		.ldsc_discard           = &be_ut_log_discard_usecase_discard,
		.ldsc_items_max         = BE_UT_LOG_DISCARD_USECASE_ITEMS_MAX,
		.ldsc_items_threshold   = BE_UT_LOG_DISCARD_USECASE_ITEMS_MAX,
		.ldsc_items_pending_max = 0,
		.ldsc_loc               = m0_locality0_get(),
		.ldsc_sync_timeout      = M0_TIME_ONE_SECOND / 10,
	};
	struct m0_be_log_discard_item **ldi;
	struct m0_be_log_discard       *ld;
	struct m0_semaphore             sem;
	int                             rc;
	int                             i;

	M0_ALLOC_ARR(ldi, BE_UT_LOG_DISCARD_USECASE_ITEMS_MAX);
	M0_UT_ASSERT(ldi != NULL);
	M0_ALLOC_PTR(ld);
	M0_UT_ASSERT(ld != NULL);
	m0_semaphore_init(&sem, 0);
	rc = m0_be_log_discard_init(ld, &ld_cfg);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < BE_UT_LOG_DISCARD_USECASE_ITEMS_MAX; ++i) {
		M0_BE_OP_SYNC(op, m0_be_log_discard_item_get(ld, &op, &ldi[i]));
		m0_be_log_discard_item_user_data_set(ldi[i], &sem);
		m0_be_log_discard_item_starting(ld, ldi[i]);
		m0_be_log_discard_item_finished(ld, ldi[i]);
	}
	for (i = 0; i < BE_UT_LOG_DISCARD_USECASE_ITEMS_MAX; ++i)
		m0_semaphore_down(&sem);
	M0_BE_OP_SYNC(op, m0_be_log_discard_flush(ld, &op));
	m0_be_log_discard_fini(ld);
	m0_semaphore_fini(&sem);
	m0_free(ld);
	m0_free(ldi);
}

enum {
	BE_UT_LOG_DISCARD_GP_ITEM_MAX       = 0x10,
	BE_UT_LOG_DISCARD_GP_CONSUMERS      = 0x20,
	BE_UT_LOG_DISCARD_GP_PRODUCERS      = 0x18,
	BE_UT_LOG_DISCARD_GP_OPERATION_NR   = 0x80000,
	BE_UT_LOG_DISCARD_GP_OPERATION_STEP = 0x10,
};

enum be_ut_log_discard_gp_role {
	BE_UT_LOG_DISCARD_GP_PRODUCER,
	BE_UT_LOG_DISCARD_GP_CONSUMER,
};

struct be_ut_log_discard_gp_test {
	struct m0_be_log_discard       blgp_ld;
	struct m0_atomic64             blgp_produce_nr;
	struct m0_atomic64             blgp_consume_nr;
	struct m0_be_log_discard_item *blgp_item[BE_UT_LOG_DISCARD_GP_ITEM_MAX];
	uint64_t                       blgp_first;
	uint64_t                       blgp_last;
	struct m0_semaphore            blgp_sem;
	struct m0_mutex                blgp_lock;
};

struct be_ut_log_discard_gp_test_item {
	bool                              blgi_producer;
	int                               blgi_index;
	int                               blgi_operation_nr;
	struct be_ut_log_discard_gp_test *blgi_test;
};

static void be_ut_log_discard_gp_sync(struct m0_be_log_discard      *ld,
                                      struct m0_be_op               *op,
                                      struct m0_be_log_discard_item *ldi)
{
	M0_IMPOSSIBLE("");
}

static void be_ut_log_discard_gp_discard(struct m0_be_log_discard      *ld,
                                         struct m0_be_log_discard_item *ldi)
{
	M0_IMPOSSIBLE("");
}

static void be_ut_log_discard_gp_consume(struct be_ut_log_discard_gp_test *test)
{
	struct m0_be_log_discard_item *ldi;

	M0_BE_OP_SYNC(op,
		      m0_be_log_discard_item_get(&test->blgp_ld, &op, &ldi));
	m0_mutex_lock(&test->blgp_lock);
	M0_ASSERT(test->blgp_first <= test->blgp_last);
	test->blgp_item[test->blgp_last++ % ARRAY_SIZE(test->blgp_item)] = ldi;
	m0_mutex_unlock(&test->blgp_lock);
	m0_semaphore_up(&test->blgp_sem);
}

static void be_ut_log_discard_gp_produce(struct be_ut_log_discard_gp_test *test)
{
	struct m0_be_log_discard_item *ldi;

	m0_semaphore_down(&test->blgp_sem);
	m0_mutex_lock(&test->blgp_lock);
	M0_ASSERT(test->blgp_first <= test->blgp_last);
	ldi = test->blgp_item[test->blgp_first++ % ARRAY_SIZE(test->blgp_item)];
	m0_mutex_unlock(&test->blgp_lock);
	m0_be_log_discard_item_put(&test->blgp_ld, ldi);
}

static void be_ut_log_discard_gp_thread(void *param)
{
	struct be_ut_log_discard_gp_test_item *gp_item = param;
	struct be_ut_log_discard_gp_test      *gp_test = gp_item->blgi_test;
	struct m0_atomic64                    *operation_nr;
	uint64_t                               step;
	uint64_t                               end;
	uint64_t                               i;
	bool                                   producer;

	producer     = gp_item->blgi_producer;
	operation_nr = producer ?
		       &gp_test->blgp_produce_nr : &gp_test->blgp_consume_nr;
	step         = BE_UT_LOG_DISCARD_GP_OPERATION_STEP;
	while (1) {
		end = m0_atomic64_add_return(operation_nr, step);
		if (end > BE_UT_LOG_DISCARD_GP_OPERATION_NR)
			break;
		for (i = end - step; i < end; ++i) {
			if (producer)
				be_ut_log_discard_gp_produce(gp_test);
			else
				be_ut_log_discard_gp_consume(gp_test);
			++gp_item->blgi_operation_nr;
		}
	}
}

static void
be_ut_log_discard_gp_set(struct be_ut_log_discard_gp_test_item *item,
                         bool                                   is_producer,
                         int                                    index,
                         struct be_ut_log_discard_gp_test      *test)
{
	*item = (struct be_ut_log_discard_gp_test_item){
		.blgi_producer     = is_producer,
		.blgi_index        = index,
		.blgi_operation_nr = 0,
		.blgi_test         = test,
	};
}

M0_UT_THREADS_DEFINE(be_ut_log_discard_gp_producers,
                     &be_ut_log_discard_gp_thread);
M0_UT_THREADS_DEFINE(be_ut_log_discard_gp_consumers,
                     &be_ut_log_discard_gp_thread);

void m0_be_ut_log_discard_getput(void)
{
	struct be_ut_log_discard_gp_test      *gp_test;
	struct be_ut_log_discard_gp_test_item *gp_producers;
	struct be_ut_log_discard_gp_test_item *gp_consumers;
	struct m0_be_log_discard_cfg           ld_cfg = {
		.ldsc_sync              = &be_ut_log_discard_gp_sync,
		.ldsc_discard           = &be_ut_log_discard_gp_discard,
		.ldsc_items_max         = BE_UT_LOG_DISCARD_GP_ITEM_MAX,
		.ldsc_items_threshold   = BE_UT_LOG_DISCARD_GP_ITEM_MAX,
		.ldsc_items_pending_max = BE_UT_LOG_DISCARD_GP_CONSUMERS,
		.ldsc_loc               = m0_locality0_get(),
		.ldsc_sync_timeout      = M0_TIME_ONE_SECOND,
	};
	int                                    rc;
	int                                    i;

	M0_ALLOC_PTR(gp_test);
	M0_UT_ASSERT(gp_test != NULL);
	M0_ALLOC_ARR(gp_consumers, BE_UT_LOG_DISCARD_GP_CONSUMERS);
	M0_UT_ASSERT(gp_consumers != NULL);
	M0_ALLOC_ARR(gp_producers, BE_UT_LOG_DISCARD_GP_PRODUCERS);
	M0_UT_ASSERT(gp_producers != NULL);

	for (i = 0; i < BE_UT_LOG_DISCARD_GP_CONSUMERS; ++i)
		be_ut_log_discard_gp_set(&gp_consumers[i], false, i, gp_test);
	for (i = 0; i < BE_UT_LOG_DISCARD_GP_PRODUCERS; ++i)
		be_ut_log_discard_gp_set(&gp_producers[i], true, i, gp_test);

	rc = m0_be_log_discard_init(&gp_test->blgp_ld, &ld_cfg);
	M0_UT_ASSERT(rc == 0);
	m0_atomic64_set(&gp_test->blgp_produce_nr, 0);
	m0_atomic64_set(&gp_test->blgp_consume_nr, 0);
	gp_test->blgp_first = 0;
	gp_test->blgp_last  = 0;
	m0_mutex_init(&gp_test->blgp_lock);
	m0_semaphore_init(&gp_test->blgp_sem, 0);

	M0_UT_THREADS_START(be_ut_log_discard_gp_producers,
			    BE_UT_LOG_DISCARD_GP_PRODUCERS, gp_producers);
	M0_UT_THREADS_START(be_ut_log_discard_gp_consumers,
			    BE_UT_LOG_DISCARD_GP_CONSUMERS, gp_consumers);
	M0_UT_THREADS_STOP(be_ut_log_discard_gp_producers);
	M0_UT_THREADS_STOP(be_ut_log_discard_gp_consumers);

	M0_BE_OP_SYNC(op, m0_be_log_discard_flush(&gp_test->blgp_ld, &op));
	m0_be_log_discard_fini(&gp_test->blgp_ld);

	m0_semaphore_fini(&gp_test->blgp_sem);
	m0_mutex_fini(&gp_test->blgp_lock);
	m0_free(gp_producers);
	m0_free(gp_consumers);
	m0_free(gp_test);
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
