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
 * Original creation date: 1-Mar-2015
 */


/**
 * @addtogroup be
 *
 * Tests to add:
 * - ordering for SIO_READ be IOs;
 * - sync-enabled be IOs.
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/io_sched.h"

#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/time.h"           /* m0_time_now */
#include "lib/atomic.h"         /* m0_atomic64 */
#include "lib/semaphore.h"      /* m0_semaphore */

#include "be/op.h"              /* m0_be_op_init */
#include "be/io.h"              /* m0_be_io_init */

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "ut/threads.h"         /* M0_UT_THREADS_DEFINE */
#include "ut/stob.h"            /* m0_ut_stob_linux_get */
#include "stob/stob.h"          /* m0_stob_block_shift */

enum {
	BE_UT_IO_SCHED_THREAD_NR     = 0x10,
	BE_UT_IO_SCHED_IO_NR         = 0x10,
	BE_UT_IO_SCHED_ADD_NR        = 0x400,
	BE_UT_IO_SCHED_IO_OFFSET_MAX = 0x10000,
	BE_UT_IO_SCHED_EXT_SIZE_MAX  = 0xdf3,
};

enum be_ut_io_sched_io_op {
	BE_UT_IO_SCHED_IO_START,
	BE_UT_IO_SCHED_IO_FINISH,
};

struct be_ut_io_sched_io_state {
	enum be_ut_io_sched_io_op  sis_op;
	m0_time_t                  sis_time;
	struct m0_be_io           *sis_io;
	/* TODO dependencies etc. */
};

struct be_ut_io_sched_test {
	/* number of different struct m0_be_io for the test */
	int                               st_io_nr;
	/* number of m0_be_io_sched_add() calls */
	int                               st_sched_add_nr;
	/* stob for I/O operations */
	struct m0_stob                   *st_stob;
	/* data for I/O operation */
	uint64_t                          st_data;
	/* seed for m0_rnd64() */
	uint64_t                          st_seed;
	/* callback execution queue */
	struct be_ut_io_sched_io_state  *st_states;
	/* st_states array size */
	int                              st_states_nr;
	/* current pos in st_states */
	struct m0_atomic64               *st_states_pos;

	/* array of m0_be_io for the test */
	struct m0_be_io                  *st_io;
	/* array of m0_be_op for the array of m0_be_io */
	struct m0_be_op                  *st_op;
	/* circular buffer of m0_be_io ready to be used */
	struct m0_be_io                 **st_io_ready;
	/* circular buffer of m0_be_op for the m0_be_io */
	struct m0_be_op                 **st_op_ready;
	/*
	 * Current position in st_io_ready for add() operation.
	 * Absolute value.
	 * It is atomic because it is incremented in m0_be_op callbacks,
	 * which may be called from different threads (ioq threads in our case)
	 * at the same time.
	 */
	struct m0_atomic64                st_io_ready_pos_add;
	/* The same for del() operation. */
	struct m0_atomic64                st_io_ready_pos_del;
	struct m0_semaphore               st_io_ready_sem;
	struct m0_atomic64               *st_ext_index;
};

static struct m0_be_io_sched  be_ut_io_sched_scheduler;

static void be_ut_io_sched_io_ready_add(struct be_ut_io_sched_test *test,
					struct m0_be_io            *bio,
					struct m0_be_op            *op)
{
	uint64_t pos;

	pos =  m0_atomic64_add_return(&test->st_io_ready_pos_add, 1) - 1;
	pos %= test->st_io_nr;
	test->st_io_ready[pos] = bio;
	test->st_op_ready[pos] = op;
	m0_semaphore_up(&test->st_io_ready_sem);
}

static void be_ut_io_sched_io_ready_get(struct be_ut_io_sched_test  *test,
                                        struct m0_be_io            **bio,
                                        struct m0_be_op            **op)
{
	uint64_t pos;
	bool     down;

	down = m0_semaphore_timeddown(&test->st_io_ready_sem,
				      m0_time_from_now(600, 0));
	M0_ASSERT_INFO(down, "There is either bug in m0_be_io_sched "
		       "implementation or I/O on be_ut_io_sched_stob "
		       "timed out");
	M0_ASSERT_INFO(m0_atomic64_get(&test->st_io_ready_pos_del) <
		       m0_atomic64_get(&test->st_io_ready_pos_add),
	               "pos_del=%"PRId64" pos_add=%"PRId64,
	               m0_atomic64_get(&test->st_io_ready_pos_del),
		       m0_atomic64_get(&test->st_io_ready_pos_add));
	pos =  m0_atomic64_add_return(&test->st_io_ready_pos_del, 1) - 1;
	pos %= test->st_io_nr;
	*bio = test->st_io_ready[pos];
	*op  = test->st_op_ready[pos];
}

/* atomically add io_state to states queue in the test */
static void be_ut_io_sched_io_state_add(struct be_ut_io_sched_test     *test,
					struct be_ut_io_sched_io_state *state)
{
	uint64_t pos;

	pos = m0_atomic64_add_return(test->st_states_pos, 1) - 1;
	M0_UT_ASSERT(pos < test->st_states_nr);
	test->st_states[pos] = *state;
}

static void be_ut_io_sched_io_completion_cb(struct m0_be_op *op, void *param)
{
	struct m0_be_io                *bio = param;
	struct be_ut_io_sched_test     *test = m0_be_io_user_data(bio);
	struct be_ut_io_sched_io_state  io_state = {
		.sis_op   = BE_UT_IO_SCHED_IO_FINISH,
		.sis_time = m0_time_now(),
		.sis_io   = bio,
	};
	be_ut_io_sched_io_state_add(test, &io_state);
	be_ut_io_sched_io_ready_add(test, bio, op);
}

static void be_ut_io_sched_io_start_cb(struct m0_be_op *op, void *param)
{
	struct m0_be_io                *bio = param;
	struct be_ut_io_sched_io_state  io_state = {
		.sis_op   = BE_UT_IO_SCHED_IO_START,
		.sis_time = m0_time_now(),
		.sis_io   = bio,
	};
	be_ut_io_sched_io_state_add(m0_be_io_user_data(bio), &io_state);
}

static void be_ut_io_sched_thread(void *param)
{
	struct be_ut_io_sched_test *test = param;
	struct m0_be_io_sched      *sched = &be_ut_io_sched_scheduler;
	struct m0_be_io_credit      iocred;
	struct m0_be_io            *bio;
	struct m0_be_op            *op;
	struct m0_stob             *stob = test->st_stob;
	struct m0_ext               ext;
	m0_bcount_t                 len;
	m0_bindex_t                 offset;
	int                         rc;
	int                         i;

	M0_ALLOC_ARR(test->st_io, test->st_io_nr);
	M0_UT_ASSERT(test->st_io != NULL);
	M0_ALLOC_ARR(test->st_op, test->st_io_nr);
	M0_UT_ASSERT(test->st_op != NULL);
	M0_ALLOC_ARR(test->st_io_ready, test->st_io_nr);
	M0_UT_ASSERT(test->st_io_ready != NULL);
	M0_ALLOC_ARR(test->st_op_ready, test->st_io_nr);
	M0_UT_ASSERT(test->st_op_ready != NULL);
	m0_semaphore_init(&test->st_io_ready_sem, 0);
	m0_atomic64_set(&test->st_io_ready_pos_add, 0);
	m0_atomic64_set(&test->st_io_ready_pos_del, 0);

	iocred = M0_BE_IO_CREDIT(1, sizeof(test->st_data), 1);
	for (i = 0; i < test->st_io_nr; ++i) {
		bio = &test->st_io[i];
		op  = &test->st_op[i];
		m0_be_op_init(op);
		m0_be_op_callback_set(op, &be_ut_io_sched_io_start_cb,
		                      bio, M0_BOS_ACTIVE);
		m0_be_op_callback_set(op, &be_ut_io_sched_io_completion_cb,
		                      bio, M0_BOS_DONE);
		rc = m0_be_io_init(bio);
		M0_UT_ASSERT(rc == 0);
		rc = m0_be_io_allocate(bio, &iocred);
		M0_UT_ASSERT(rc == 0);
		m0_be_io_user_data_set(bio, test);
		be_ut_io_sched_io_ready_add(test, bio, op);
	}
	for (i = 0; i < test->st_sched_add_nr; ++i) {
		be_ut_io_sched_io_ready_get(test, &bio, &op);
		m0_be_io_reset(bio);
		m0_be_op_reset(op);
		offset = m0_rnd64(&test->st_seed) %
			 BE_UT_IO_SCHED_IO_OFFSET_MAX;
		m0_be_io_add(bio, stob, &test->st_data, offset,
			     sizeof(test->st_data));
		m0_be_io_configure(bio, SIO_WRITE);
		len = m0_rnd64(&test->st_seed) % BE_UT_IO_SCHED_EXT_SIZE_MAX +
		      (m0_rnd64(&test->st_seed) & 0xff) + 1;
		ext.e_end   = m0_atomic64_add_return(test->st_ext_index, len);
		ext.e_start = ext.e_end - len;
		m0_be_io_sched_lock(sched);
		m0_be_io_sched_add(sched, bio, &ext, op);
		m0_be_io_sched_unlock(sched);
	}
	for (i = 0; i < test->st_io_nr; ++i) {
		be_ut_io_sched_io_ready_get(test, &bio, &op);
		m0_be_io_deallocate(bio);
		m0_be_io_fini(bio);
		m0_be_op_fini(op);
	}

	m0_semaphore_fini(&test->st_io_ready_sem);
	m0_free(test->st_op_ready);
	m0_free(test->st_io_ready);
	m0_free(test->st_op);
	m0_free(test->st_io);
}

static void
be_ut_io_sched_states_check(struct be_ut_io_sched_io_state *states,
			     int                            states_nr,
			     struct m0_atomic64            *states_pos)
{
	int pos = m0_atomic64_get(states_pos);

	M0_UT_ASSERT(pos == states_nr);
	/* TODO additional checks */
}

M0_UT_THREADS_DEFINE(be_ut_io_sched, &be_ut_io_sched_thread);

/**
 * The test:
 * 1) Creates BE_UT_IO_SCHED_THREAD_NR threads.
 * 2) Each thread:
 *    - creates st_io_nr structs m0_be_io;
 *    - st_sched_add_nr times adds one m0_be_io that is either never added
 *      to the scheduler queue or completion callback was called for it;
 * 3) Checks that all start and completion callbacks for m0_be_io was called
 * in the right order.
 *
 * @note Currently m0_be_io_sched executes m0_be_io one by one in the
 * order they are added to the scheduler queue.
 */
void m0_be_ut_io_sched(void)
{
	struct be_ut_io_sched_io_state *states;
	struct be_ut_io_sched_test     *tests;
	struct m0_be_io_sched_cfg       cfg = {
		.bisc_pos_start = 0x1234,
	};
	struct m0_be_io_sched          *sched = &be_ut_io_sched_scheduler;
	struct m0_atomic64              states_pos;
	struct m0_atomic64              ext_index;
	struct m0_stob                 *stob;
	uint64_t                        seed = 0;
	int                             states_nr;
	int                             rc;
	int                             i;

	M0_ALLOC_ARR(tests, BE_UT_IO_SCHED_THREAD_NR);
	M0_UT_ASSERT(tests != NULL);
	states_nr = BE_UT_IO_SCHED_THREAD_NR * BE_UT_IO_SCHED_ADD_NR * 2;
	M0_ALLOC_ARR(states, states_nr);
	M0_UT_ASSERT(states != NULL);
	stob = m0_ut_stob_linux_get();
	M0_UT_ASSERT(stob != NULL);
	m0_atomic64_set(&states_pos, 0);
	m0_atomic64_set(&ext_index, cfg.bisc_pos_start);
	for (i = 0; i < BE_UT_IO_SCHED_THREAD_NR; ++i) {
		tests[i] = (struct be_ut_io_sched_test){
			.st_io_nr        = BE_UT_IO_SCHED_IO_NR,
			.st_sched_add_nr = BE_UT_IO_SCHED_ADD_NR,
			.st_stob         = stob,
			.st_data         = m0_rnd64(&seed),
			.st_seed         = i,
			.st_states       = states,
			.st_states_nr    = states_nr,
			.st_states_pos   = &states_pos,
			.st_ext_index    = &ext_index,
		};
	}

	M0_SET0(sched);
	rc = m0_be_io_sched_init(sched, &cfg);
	M0_UT_ASSERT(rc == 0);
	M0_UT_THREADS_START(be_ut_io_sched, BE_UT_IO_SCHED_THREAD_NR, tests);
	M0_UT_THREADS_STOP(be_ut_io_sched);
	m0_be_io_sched_fini(sched);

	be_ut_io_sched_states_check(states, states_nr, &states_pos);

	m0_ut_stob_put(stob, true);
	m0_free(states);
	m0_free(tests);
}

/** @} end of be group */
#undef M0_TRACE_SUBSYSTEM

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
