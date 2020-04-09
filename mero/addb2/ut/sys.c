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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 19-Mar-2015
 */


/**
 * @addtogroup addb2
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "lib/finject.h"
#include "lib/semaphore.h"
#include "lib/thread.h"                /* m0_thread_tls */
#include "lib/misc.h"                  /* m0_forall, M0_SET0 */
#include "ut/ut.h"
#include "addb2/sys.h"

#include "addb2/ut/common.h"

static const struct m0_addb2_config noqueue = {
		.co_buffer_size = 4096,
		.co_buffer_min  = 0,
		.co_buffer_max  = 1,
		.co_queue_max   = 0,
		.co_pool_min    = 1,
		.co_pool_max    = 1
};

static const struct m0_addb2_config queue = {
		.co_buffer_size = 4096,
		.co_buffer_min  = 0,
		.co_buffer_max  = 1,
		.co_queue_max   = 100,
		.co_pool_min    = 1,
		.co_pool_max    = 1
};

static void init_fini(void)
{
	struct m0_addb2_sys *s;
	int                  result;

	result = m0_addb2_sys_init(&s, &noqueue);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(s != NULL);
	m0_addb2_sys_fini(s);
}

static void mach_1(void)
{
	struct m0_addb2_mach *m;
	struct m0_addb2_sys  *s;
	int                   result;

	result = m0_addb2_sys_init(&s, &noqueue);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(s != NULL);
	m = m0_addb2_sys_get(s);
	M0_UT_ASSERT(m != NULL);
	m0_addb2_sys_put(s, m);
	m0_addb2_sys_fini(s);
}

static void mach_toomany(void)
{
	struct m0_addb2_mach *m0;
	struct m0_addb2_mach *m1;
	struct m0_addb2_sys  *s;
	int                   result;

	result = m0_addb2_sys_init(&s, &noqueue);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(s != NULL);
	m0 = m0_addb2_sys_get(s);
	M0_UT_ASSERT(m0 != NULL);
	m1 = m0_addb2_sys_get(s);
	M0_UT_ASSERT(m1 == NULL);
	m0_addb2_sys_put(s, m0);
	m0_addb2_sys_fini(s);
}

static void mach_cache(void)
{
	struct m0_addb2_mach *m0;
	struct m0_addb2_mach *m1;
	struct m0_addb2_sys  *s;
	int                   result;

	result = m0_addb2_sys_init(&s, &noqueue);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(s != NULL);
	m0 = m0_addb2_sys_get(s);
	M0_UT_ASSERT(m0 != NULL);
	m0_addb2_sys_put(s, m0);
	m1 = m0_addb2_sys_get(s);
	M0_UT_ASSERT(m1 == m0);
	m0_addb2_sys_put(s, m1);
	m0_addb2_sys_fini(s);
}

enum { N = 17 };

static void mach_cache_N(void)
{
	struct m0_addb2_sys *s;
	struct m0_addb2_config conf = noqueue;
	struct m0_addb2_mach *m[N];
	struct m0_addb2_mach *mmm;
	int i;
	int result;

	/* check various pool sizes. */
	for (i = 0; i < N; ++i) {
		int j;

		conf.co_pool_max = i;
		conf.co_pool_min = i;
		result = m0_addb2_sys_init(&s, &conf);
		M0_UT_ASSERT(result == 0);
		M0_UT_ASSERT(s != NULL);
		for (j = 0; j < i; ++j) {
			m[j] = m0_addb2_sys_get(s);
			M0_UT_ASSERT(m[j] != NULL);
			M0_UT_ASSERT(m0_forall(k, j, m[k] != m[j]));
		}
		mmm = m0_addb2_sys_get(s);
		M0_UT_ASSERT(mmm == NULL);
		for (j = 0; j < i; ++j) {
			int t;

			m0_addb2_sys_put(s, m[j]);
			/*
			 * Machines from m[0] to m[j] were released back into
			 * the pool. Reacquire them and check that the same
			 * machines are returned.
			 */
			for (t = 0; t <= j; ++t) {
				mmm = m0_addb2_sys_get(s);
				M0_UT_ASSERT(mmm != NULL);
				M0_UT_ASSERT(m0_exists(k, j + 1, mmm == m[k]));
			}
			mmm = m0_addb2_sys_get(s);
			M0_UT_ASSERT(mmm == NULL);
			for (t = 0; t <= j; ++t)
				m0_addb2_sys_put(s, m[t]);
		}
		m0_addb2_sys_fini(s);
	}
}

static void _add(const struct m0_addb2_config *conf, unsigned nr)
{
	struct m0_addb2_sys  *s;
	struct m0_addb2_mach *orig;
	struct m0_addb2_mach *m;
	struct m0_thread_tls *tls = m0_thread_tls();
	int i;
	int result;

	result = m0_addb2_sys_init(&s, conf);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(s != NULL);
	m = m0_addb2_sys_get(s);
	M0_UT_ASSERT(m != NULL);
	orig = tls->tls_addb2_mach;
	tls->tls_addb2_mach = m;
	for (i = 0; i < nr; ++i)
		M0_ADDB2_ADD(10 + i, 9, 8, 7, 6 + i, 5, 4, 3 - i, 2, 1, 0);
	tls->tls_addb2_mach = orig;
	m0_addb2_sys_put(s, m);
	m0_addb2_sys_fini(s);
}

static void add_loop(const struct m0_addb2_config *conf)
{
	_add(conf, 1);
	_add(conf, 10);
	_add(conf, 100);
	_add(conf, 1000);
}

static void noqueue_add(void)
{
	add_loop(&noqueue);
}

static void queue_add(void)
{
	add_loop(&queue);
}

extern void (*m0_addb2__sys_submit_trap)(struct m0_addb2_sys *sys,
					 struct m0_addb2_trace_obj *obj);
extern void (*m0_addb2__sys_ast_trap)(struct m0_addb2_sys *sys);

static unsigned sys_submitted;
static void submit_trap(struct m0_addb2_sys *sys,
			struct m0_addb2_trace_obj *obj)
{
	++sys_submitted;
}

static struct m0_semaphore ast_wait;
static void ast_trap(struct m0_addb2_sys *sys)
{
	m0_semaphore_up(&ast_wait);
}

static void sm_add(void)
{
	struct m0_addb2_sys  *s;
	struct m0_addb2_mach *m;
	struct m0_addb2_config longqueue = queue;
	struct m0_addb2_mach *orig;
	struct m0_thread_tls *tls = m0_thread_tls();
	int result;

	longqueue.co_queue_max = 1000000;
	m0_addb2__sys_submit_trap = &submit_trap;
	m0_addb2__sys_ast_trap = &ast_trap;
	m0_semaphore_init(&ast_wait, 0);
	sys_submitted = 0;
	m0_fi_enable("sys_submit", "trap");
	m0_fi_enable("sys_ast", "trap");

	result = m0_addb2_sys_init(&s, &longqueue);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(s != NULL);
	m0_addb2_sys_sm_start(s);
	m = m0_addb2_sys_get(s);
	M0_UT_ASSERT(m != NULL);
	orig = tls->tls_addb2_mach;
	tls->tls_addb2_mach = m;

	while (sys_submitted == 0)
		M0_ADDB2_ADD(1132); /* FW */

	m0_semaphore_down(&ast_wait);
	m0_addb2__sys_ast_trap = NULL;

	tls->tls_addb2_mach = orig;
	m0_addb2_sys_put(s, m);
	m0_addb2_sys_fini(s);

	m0_fi_disable("sys_submit", "trap");
	m0_fi_disable("sys_ast", "trap");
	m0_semaphore_fini(&ast_wait);
}

struct m0_ut_suite addb2_sys_ut = {
	.ts_name = "addb2-sys",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "init-fini",     &init_fini,                "Nikita" },
		{ "mach-1",        &mach_1,                   "Nikita" },
		{ "mach-toomany",  &mach_toomany,             "Nikita" },
		{ "mach-cache",    &mach_cache,               "Nikita" },
		{ "mach-cache-N",  &mach_cache_N,             "Nikita" },
		{ "noqueue-add",   &noqueue_add,              "Nikita" },
		{ "queue-add",     &queue_add,                "Nikita" },
		{ "sm-add",        &sm_add,                   "Nikita" },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
