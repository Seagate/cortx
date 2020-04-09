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
 * Original creation date: 7-Sep-2015
 */


/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/pd.h"

#include "lib/memory.h" /* M0_ALLOC_PTR */
#include "lib/ext.h"    /* m0_ext */
#include "lib/types.h"  /* bool */
#include "lib/atomic.h" /* m0_atomic64 */

#include "be/op.h"      /* M0_BE_OP_SYNC */

#include "ut/ut.h"      /* M0_UT_ASSERT */
#include "ut/stob.h"    /* m0_ut_stob_linux_get */
#include "ut/threads.h" /* M0_UT_THREADS_DEFINE */

enum {
	BE_UT_PD_USECASE_POS_START = 0x12345,
	BE_UT_PD_USECASE_PD_IO_NR  = 0x10,
	BE_UT_PD_USECASE_REG_NR    = 0x10,
	BE_UT_PD_USECASE_THREAD_NR = 0x10,
	BE_UT_PD_USECASE_ITER_NR   = 0x10,
};

struct be_ut_pd_usecase_test {
	bool                bput_read;
	int                 bput_iter_nr;
	int                 bput_pd_io_nr;
	int                 bput_pd_reg_nr;
	struct m0_be_pd    *bput_pd;
	struct m0_atomic64 *bput_pos;
	struct m0_stob     *bput_stob;
};

static void be_ut_pd_usecase_thread(void *param)
{
	struct be_ut_pd_usecase_test  *test = param;
	struct m0_be_pd_io           **pdio;
	struct m0_be_op               *op;
	struct m0_be_pd               *pd = test->bput_pd;
	struct m0_be_io               *bio;
	m0_bindex_t                    offset;
	m0_bindex_t                    pos;
	char                          *mem;
	int                            iter;
	int                            i;
	int                            j;

	M0_ALLOC_ARR(pdio, test->bput_pd_io_nr);
	M0_UT_ASSERT(pdio != NULL);
	M0_ALLOC_ARR(op, test->bput_pd_io_nr);
	M0_UT_ASSERT(op != NULL);
	M0_ALLOC_ARR(mem, test->bput_pd_io_nr * test->bput_pd_reg_nr * 2);
	for (iter = 0; iter < test->bput_iter_nr; ++iter) {
		for (i = 0; i < test->bput_pd_io_nr; ++i) {
			M0_SET0(&op[i]);
			m0_be_op_init(&op[i]);
			M0_BE_OP_SYNC(op1, m0_be_pd_io_get(pd, &pdio[i], &op1));
		}
		for (i = 0; i < test->bput_pd_io_nr; ++i) {
			bio = m0_be_pd_io_be_io(pdio[i]);
			for (j = 0; j < test->bput_pd_reg_nr; ++j) {
				offset = (i * test->bput_pd_reg_nr + j) * 2;
				m0_be_io_add(bio, test->bput_stob,
				             &mem[offset], offset, 1);
			}
			m0_be_io_configure(bio, test->bput_read ?
					   SIO_READ : SIO_WRITE);
			if (!test->bput_read) {
				pos = m0_atomic64_add_return(test->bput_pos,
							     1) - 1;
			}
			m0_be_pd_io_add(pd, pdio[i], test->bput_read ?
					NULL : &M0_EXT(pos, pos + 1), &op[i]);
		}
		for (i = 0; i < test->bput_pd_io_nr; ++i)
			m0_be_op_wait(&op[i]);
		for (i = 0; i < test->bput_pd_io_nr; ++i) {
			m0_be_pd_io_put(pd, pdio[i]);
			m0_be_op_fini(&op[i]);
		}
	}
	m0_free(mem);
	m0_free(op);
	m0_free(pdio);
}

M0_UT_THREADS_DEFINE(be_ut_pd_usecase, &be_ut_pd_usecase_thread);

void m0_be_ut_pd_usecase(void)
{
	struct m0_be_pd_cfg           pd_cfg = {
		.bpdc_sched = {
			.bisc_pos_start = BE_UT_PD_USECASE_POS_START,
		},
		.bpdc_seg_io_nr = BE_UT_PD_USECASE_PD_IO_NR * 2 *
				  BE_UT_PD_USECASE_THREAD_NR,
		.bpdc_seg_io_pending_max = 0,
		.bpdc_io_credit = {
			.bic_reg_nr   = BE_UT_PD_USECASE_REG_NR,
			.bic_reg_size = BE_UT_PD_USECASE_REG_NR,
			.bic_part_nr  = 1,
		},
	};
	struct be_ut_pd_usecase_test *tests;
	struct m0_atomic64            pos;
	struct m0_be_pd              *pd;
	struct m0_stob               *stob;
	uint32_t                      i;
	int                           rc;

	stob = m0_ut_stob_linux_get();
	M0_ALLOC_PTR(pd);
	M0_UT_ASSERT(pd != NULL);
	M0_ALLOC_ARR(tests, BE_UT_PD_USECASE_THREAD_NR * 2);
	M0_UT_ASSERT(tests != NULL);
	for (i = 0; i < BE_UT_PD_USECASE_THREAD_NR * 2; ++i) {
		tests[i] = (struct be_ut_pd_usecase_test){
			.bput_read      = (i % 2) == 0,
			.bput_iter_nr   = BE_UT_PD_USECASE_ITER_NR,
			.bput_pd_io_nr  = BE_UT_PD_USECASE_PD_IO_NR,
			.bput_pd_reg_nr = BE_UT_PD_USECASE_REG_NR,
			.bput_pd        = pd,
			.bput_pos       = &pos,
			.bput_stob      = stob,
		};
	}
	rc = m0_be_pd_init(pd, &pd_cfg);
	M0_UT_ASSERT(rc == 0);
	m0_atomic64_set(&pos, BE_UT_PD_USECASE_POS_START);

	M0_UT_THREADS_START(be_ut_pd_usecase, BE_UT_PD_USECASE_THREAD_NR * 2,
			    tests);
	M0_UT_THREADS_STOP(be_ut_pd_usecase);

	m0_be_pd_fini(pd);
	m0_free(tests);
	m0_free(pd);
	m0_ut_stob_put(stob, true);
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
