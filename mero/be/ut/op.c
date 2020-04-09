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
 * Original creation date: 10-Jun-2015
 */


/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/op.h"

#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "lib/semaphore.h" /* m0_semaphore */
#include "ut/ut.h"         /* M0_UT_ASSERT */
#include "ut/threads.h"    /* M0_UT_THREADS_DEFINE */

void m0_be_ut_op_usecase(void)
{
	struct m0_be_op op = {};

	m0_be_op_init(&op);
	M0_UT_ASSERT(!m0_be_op_is_done(&op));
	m0_be_op_active(&op);
	M0_UT_ASSERT(!m0_be_op_is_done(&op));
	m0_be_op_done(&op);
	M0_UT_ASSERT(m0_be_op_is_done(&op));
	m0_be_op_fini(&op);
}

enum be_ut_op_mt_cmd {
	BE_UT_OP_MT_INIT,
	BE_UT_OP_MT_WAIT1,
	BE_UT_OP_MT_ACTIVE,
	BE_UT_OP_MT_WAIT2,
	BE_UT_OP_MT_DONE,
	BE_UT_OP_MT_WAIT3,
	BE_UT_OP_MT_FINI,
	BE_UT_OP_MT_CMD_NR,
};

enum be_ut_op_mt_dep_type {
	BE_UT_OP_MT_WAIT_BEFORE,
	BE_UT_OP_MT_WAIT_AFTER,
};

struct be_ut_op_mt_dep {
	enum be_ut_op_mt_cmd      bod_src;
	enum be_ut_op_mt_dep_type bod_type;
	enum be_ut_op_mt_cmd      bod_dst;
};

struct be_ut_op_mt_thread_cfg {
	struct m0_semaphore   bom_barrier;
	struct m0_semaphore  *bom_signal_to[2];
	struct m0_semaphore  *bom_wait_before;
	struct m0_semaphore  *bom_wait_after;
	struct m0_semaphore  *bom_try_down;
	enum be_ut_op_mt_cmd  bom_cmd;
	struct m0_be_op      *bom_op;
};

static void be_ut_op_mt_thread_func(void *param)
{
	struct be_ut_op_mt_thread_cfg *cfg = param;
	bool                           success;
	bool                           done;
	int                            i;

	M0_ENTRY("enter %d, wait4 %p", cfg->bom_cmd, cfg->bom_wait_before);
	if (cfg->bom_wait_before != NULL)
		m0_semaphore_down(cfg->bom_wait_before);
	M0_LOG(M0_DEBUG, "waited %d", cfg->bom_cmd);
	switch (cfg->bom_cmd) {
	case BE_UT_OP_MT_INIT:
		m0_be_op_init(cfg->bom_op);
		break;
	case BE_UT_OP_MT_ACTIVE:
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(!done);
		m0_be_op_active(cfg->bom_op);
		break;
	case BE_UT_OP_MT_DONE:
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(!done);
		m0_be_op_done(cfg->bom_op);
		break;
	case BE_UT_OP_MT_FINI:
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(done);
		m0_be_op_fini(cfg->bom_op);
		break;
	case BE_UT_OP_MT_WAIT1:
	case BE_UT_OP_MT_WAIT2:
	case BE_UT_OP_MT_WAIT3:
		m0_be_op_wait(cfg->bom_op);
		done = m0_be_op_is_done(cfg->bom_op);
		M0_UT_ASSERT(done);
		break;
	default:
		M0_IMPOSSIBLE("invalid command %d", cfg->bom_cmd);
	}
	if (cfg->bom_try_down != NULL) {
		success = m0_semaphore_trydown(cfg->bom_try_down);
		M0_UT_ASSERT(success);
	}
	if (cfg->bom_wait_after != NULL)
		m0_semaphore_down(cfg->bom_wait_after);
	for (i = 0; i < ARRAY_SIZE(cfg->bom_signal_to); ++i) {
		if (cfg->bom_signal_to[i] != NULL) {
			M0_LOG(M0_DEBUG, "signal to %p", cfg->bom_signal_to[i]);
			m0_semaphore_up(cfg->bom_signal_to[i]);
		}
	}
	M0_LEAVE();
}

M0_UT_THREADS_DEFINE(be_ut_op_mt, &be_ut_op_mt_thread_func);

/**
 *  +---------------+
 *  |               |
 *  |               V
 *  |   INIT ---> WAIT1
 *  |    |          |
 *  |    V          V
 *  |  ACTIVE --> WAIT2
 *  |    |          |
 *  |    V          V
 *  +-- DONE ---> WAIT3 --> FINI
 */
void m0_be_ut_op_mt(void)
{
	struct be_ut_op_mt_thread_cfg *cfg;
	struct be_ut_op_mt_thread_cfg *src;
	struct be_ut_op_mt_thread_cfg *dst;
	struct be_ut_op_mt_dep        *dep;
	struct m0_semaphore            trigger = {};
	struct m0_semaphore            completion = {};
#define DEP(src, type, dst) { .bod_src = src, .bod_type = type, .bod_dst = dst }
	struct be_ut_op_mt_dep         deps[] = {
	DEP(BE_UT_OP_MT_INIT,   BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_ACTIVE),
	DEP(BE_UT_OP_MT_ACTIVE, BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_DONE),
	DEP(BE_UT_OP_MT_DONE,   BE_UT_OP_MT_WAIT_AFTER,  BE_UT_OP_MT_WAIT1),
	DEP(BE_UT_OP_MT_INIT,   BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_WAIT1),
	DEP(BE_UT_OP_MT_ACTIVE, BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_WAIT2),
	DEP(BE_UT_OP_MT_DONE,   BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_WAIT3),
	DEP(BE_UT_OP_MT_WAIT1,  BE_UT_OP_MT_WAIT_AFTER,  BE_UT_OP_MT_WAIT2),
	DEP(BE_UT_OP_MT_WAIT2,  BE_UT_OP_MT_WAIT_AFTER,  BE_UT_OP_MT_WAIT3),
	DEP(BE_UT_OP_MT_WAIT3,  BE_UT_OP_MT_WAIT_BEFORE, BE_UT_OP_MT_FINI),
	};
#undef DEP
	struct m0_be_op                op = {};
	int                            i;
	int                            rc;

	M0_ALLOC_ARR(cfg, BE_UT_OP_MT_CMD_NR);
	M0_UT_ASSERT(cfg != NULL);
	for (i = 0; i < BE_UT_OP_MT_CMD_NR; ++i) {
		rc = m0_semaphore_init(&cfg[i].bom_barrier, 0);
		M0_UT_ASSERT(rc == 0);
		cfg[i].bom_cmd = i;
		cfg[i].bom_op = &op;
	}
	rc = m0_semaphore_init(&trigger, 0);
	M0_UT_ASSERT(rc == 0);
	rc = m0_semaphore_init(&completion, 0);
	M0_UT_ASSERT(rc == 0);
	cfg[BE_UT_OP_MT_INIT].bom_wait_before  = &trigger;
	cfg[BE_UT_OP_MT_FINI].bom_signal_to[0] = &completion;
	for (i = 0; i < ARRAY_SIZE(deps); ++i) {
		dep = &deps[i];
		src = &cfg[dep->bod_src];
		dst = &cfg[dep->bod_dst];
		if (src->bom_signal_to[0] == NULL) {
			src->bom_signal_to[0] = &dst->bom_barrier;
		} else if (src->bom_signal_to[1] == NULL) {
			src->bom_signal_to[1] = &dst->bom_barrier;
		} else {
			M0_IMPOSSIBLE("invalid deps");
		}
		switch (dep->bod_type) {
		case BE_UT_OP_MT_WAIT_BEFORE:
			M0_UT_ASSERT(dst->bom_wait_before == NULL);
			dst->bom_wait_before = &dst->bom_barrier;
			break;
		case BE_UT_OP_MT_WAIT_AFTER:
			M0_UT_ASSERT(dst->bom_wait_after == NULL);
			dst->bom_wait_after = &dst->bom_barrier;
			break;
		default:
			M0_IMPOSSIBLE("invalid dep type");
		}
	}
	M0_UT_THREADS_START(be_ut_op_mt, BE_UT_OP_MT_CMD_NR, cfg);
	m0_semaphore_up(&trigger);
	m0_semaphore_down(&completion);
	M0_UT_THREADS_STOP(be_ut_op_mt);

	m0_semaphore_fini(&completion);
	m0_semaphore_fini(&trigger);
	for (i = 0; i < BE_UT_OP_MT_CMD_NR; ++i)
		m0_semaphore_fini(&cfg[i].bom_barrier);
	m0_free(cfg);
}

enum {
	BE_UT_OP_SET_USECASE_NR = 0x1000,
};

/*
 * op
 *  \__ set[0]
 *  \__ set[1]
 *  ...
 *  \__ set[BE_UT_OP_SET_USECASE_NR - 1]
 */
void m0_be_ut_op_set_usecase(void)
{
	struct m0_be_op  op = {};
	struct m0_be_op *set;
	int              i;

	M0_ALLOC_ARR(set, BE_UT_OP_SET_USECASE_NR);
	M0_UT_ASSERT(set != NULL);
	m0_be_op_init(&op);
	M0_UT_ASSERT(!m0_be_op_is_done(&op));
	for (i = 0; i < BE_UT_OP_SET_USECASE_NR; ++i) {
		m0_be_op_init(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
		m0_be_op_set_add(&op, &set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	for (i = 0; i < BE_UT_OP_SET_USECASE_NR / 2; ++i) {
		m0_be_op_active(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	for (i = 1; i < BE_UT_OP_SET_USECASE_NR / 2; ++i) {
		m0_be_op_done(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	for (i = BE_UT_OP_SET_USECASE_NR / 2;
	     i < BE_UT_OP_SET_USECASE_NR; ++i) {
		m0_be_op_active(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
		m0_be_op_done(&set[i]);
		M0_UT_ASSERT(!m0_be_op_is_done(&op));
	}
	m0_be_op_done(&set[0]);
	M0_UT_ASSERT(m0_be_op_is_done(&op));

	for (i = 0; i < BE_UT_OP_SET_USECASE_NR; ++i)
		m0_be_op_fini(&set[i]);
	m0_be_op_fini(&op);

	m0_free(set);
}

enum {
	BE_UT_OP_SET_TREE_LEVEL_SIZE   = 5,
	BE_UT_OP_SET_TREE_LEVEL_NR     = 8,
	BE_UT_OP_SET_TREE_RNG_SEED     = 5,
	BE_UT_OP_SET_TREE_SHUFFLE_ITER = 0x100,
};

enum be_ut_op_set_tree_cmd {
	BE_UT_OP_SET_TREE_INIT,
	BE_UT_OP_SET_TREE_SET_ADD,
	BE_UT_OP_SET_TREE_STATES,
	BE_UT_OP_SET_TREE_FINI,
	BE_UT_OP_SET_TREE_SET_ACTIVE,
	BE_UT_OP_SET_TREE_SET_DONE,
	BE_UT_OP_SET_TREE_ASSERT_DONE,
	BE_UT_OP_SET_TREE_ASSERT_NOT_DONE,
};

static void be_ut_op_set_tree_swap(unsigned *a, unsigned *b)
{
	unsigned t;

	 t = *a;
	*a = *b;
	*b =  t;
}

static void be_ut_op_set_tree_random_shuffle(unsigned *arr,
                                             unsigned  nr,
                                             bool      keep_half_dist_order,
                                             uint64_t *seed)
{
	unsigned half_dist = nr / 2 + 1;
	unsigned a;
	unsigned b;
	int      i;
	int      j;

	for (i = 0; i < BE_UT_OP_SET_TREE_SHUFFLE_ITER; ++i) {
		a = m0_rnd64(seed) % nr;
		b = m0_rnd64(seed) % nr;
		be_ut_op_set_tree_swap(&arr[a], &arr[b]);
	}
	if (keep_half_dist_order) {
		for (i = 0; i < nr; ++i) {
			for (j = i + 1; j < nr; ++j) {
				if (arr[i] > arr[j] &&
				    (arr[i] + half_dist == arr[j] ||
				     arr[j] + half_dist == arr[i])) {
					be_ut_op_set_tree_swap(&arr[i],
							       &arr[j]);
				}
			}
		}
	}
}

static void be_ut_op_set_tree_do(struct m0_be_op            *op,
                                 struct m0_be_op            *child,
                                 enum be_ut_op_set_tree_cmd  cmd)
{
	bool done;

	switch (cmd) {
	case BE_UT_OP_SET_TREE_INIT:
		m0_be_op_init(op);
		break;
	case BE_UT_OP_SET_TREE_SET_ADD:
		m0_be_op_set_add(op, child);
		break;
	case BE_UT_OP_SET_TREE_FINI:
		m0_be_op_fini(op);
		break;
	case BE_UT_OP_SET_TREE_SET_ACTIVE:
		m0_be_op_active(op);
		break;
	case BE_UT_OP_SET_TREE_SET_DONE:
		m0_be_op_done(op);
		break;
	case BE_UT_OP_SET_TREE_ASSERT_DONE:
		done = m0_be_op_is_done(op);
		M0_UT_ASSERT(done);
		break;
	case BE_UT_OP_SET_TREE_ASSERT_NOT_DONE:
		done = m0_be_op_is_done(op);
		M0_UT_ASSERT(!done);
		break;
	default:
		M0_IMPOSSIBLE("impossible branch");
	}
}

static void be_ut_op_set_tree_recursive(struct m0_be_op            *op,
                                        enum be_ut_op_set_tree_cmd  cmd,
                                        int                         level,
                                        int                         index,
                                        uint64_t                   *seed)
{
	enum be_ut_op_set_tree_cmd cmd2;
	const int                  level_size = BE_UT_OP_SET_TREE_LEVEL_SIZE;
	const int                  level_nr   = BE_UT_OP_SET_TREE_LEVEL_NR;
	unsigned                   order[BE_UT_OP_SET_TREE_LEVEL_SIZE * 2] = {};
	unsigned                   i;
	unsigned                   j;

	for (i = 0; i < ARRAY_SIZE(order); ++i)
		order[i] = i;
	if (cmd == BE_UT_OP_SET_TREE_STATES) {
		be_ut_op_set_tree_do(&op[index], NULL,
				     BE_UT_OP_SET_TREE_ASSERT_NOT_DONE);
	}
	if (cmd == BE_UT_OP_SET_TREE_STATES && level + 2 == level_nr) {
		be_ut_op_set_tree_random_shuffle(order, level_size * 2 - 1,
						 true, seed);
		for (i = 0; i < ARRAY_SIZE(order); ++i) {
			j = index * level_size + order[i] % level_size + 1;
			cmd2 = order[i] / level_size == 0 ?
			       BE_UT_OP_SET_TREE_SET_ACTIVE :
			       BE_UT_OP_SET_TREE_SET_DONE;
			be_ut_op_set_tree_do(&op[j], NULL, cmd2);
			cmd2 = i < ARRAY_SIZE(order) - 1 ?
			       BE_UT_OP_SET_TREE_ASSERT_NOT_DONE :
			       BE_UT_OP_SET_TREE_ASSERT_DONE;
			be_ut_op_set_tree_do(&op[index], NULL, cmd2);
		}
	} else {
		if (level < level_nr - 1) {
			/* only first half of order[] is used in this branch */
			be_ut_op_set_tree_random_shuffle(order, level_size,
							 false, seed);
			/*
			 * Order is interpreted as order of op processing
			 * on the current level.
			 */
			for (i = 0; i < level_size; ++i) {
				j = index * level_size + order[i] + 1;
				if (cmd == BE_UT_OP_SET_TREE_SET_ADD) {
					be_ut_op_set_tree_do(&op[index],
							     &op[j], cmd);
				}
				be_ut_op_set_tree_recursive(op, cmd, level + 1,
							    j, seed);
			}
		}
		if (!M0_IN(cmd, (BE_UT_OP_SET_TREE_SET_ADD,
		                 BE_UT_OP_SET_TREE_STATES)))
			be_ut_op_set_tree_do(&op[index], NULL, cmd);
	}
	if (cmd == BE_UT_OP_SET_TREE_STATES) {
		be_ut_op_set_tree_do(&op[index], NULL,
				     BE_UT_OP_SET_TREE_ASSERT_DONE);
	}
}

/*
 * op[0]
 * \__ op[1]
 * |   \___ op[LEVEL_SIZE + 1]
 * |   |    \___ op[LEVEL_SIZE + LEVEL_SIZE * LEVEL_SIZE + 1]
 * |   ...........
 * |   \___ op[LEVEL_SIZE + 2]
 * |   ...
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE]
 * \__ op[2]
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE + 1]
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE + 2]
 * |   ...
 * |   \___ op[LEVEL_SIZE + LEVEL_SIZE + LEVEL_SIZE]
 * \__ op[3]
 * |   \___ op[LEVEL_SIZE + 2 * LEVEL_SIZE + 1]
 * |   \___ op[LEVEL_SIZE + 2 * LEVEL_SIZE + 2]
 * |   ...
 * |   \___ op[LEVEL_SIZE + 2 * LEVEL_SIZE + LEVEL_SIZE]
 * ...
 * \__ op[LEVEL_SIZE]
 *     \___ op[LEVEL_SIZE + (LEVEL_SIZE - 1) * LEVEL_SIZE + 1]
 *     \___ op[LEVEL_SIZE + (LEVEL_SIZE - 1) * LEVEL_SIZE + 2]
 *     ...
 *     \___ op[LEVEL_SIZE + (LEVEL_SIZE - 1) * LEVEL_SIZE + LEVEL_SIZE]
 */
void m0_be_ut_op_set_tree(void)
{
	struct m0_be_op *op;
	unsigned         op_nr;
	unsigned         op_per_lvl;
	int              level;
	uint64_t         seed = BE_UT_OP_SET_TREE_RNG_SEED;

	op_nr = 0;
	op_per_lvl = 1;
	for (level = 0; level < BE_UT_OP_SET_TREE_LEVEL_NR; ++level) {
		op_nr      += op_per_lvl;
		op_per_lvl *= BE_UT_OP_SET_TREE_LEVEL_SIZE;
	}
	M0_ALLOC_ARR(op, op_nr);
	M0_UT_ASSERT(op != NULL);

	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_INIT,    0, 0, &seed);
	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_SET_ADD, 0, 0, &seed);
	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_STATES,  0, 0, &seed);
	be_ut_op_set_tree_recursive(op, BE_UT_OP_SET_TREE_FINI,    0, 0, &seed);

	m0_free(op);
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
