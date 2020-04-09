/* -*- C -*- */
/*
 * COPYRIGHT 2018 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nachiket Sahasrabudhe <nachikets@gmail.com>
 * Original creation date: 23-Aug-2018
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ISCS
#include "lib/trace.h"

#include "iscservice/ut/common.h"
#include "lib/memory.h"

M0_INTERNAL void cc_block_init(struct cnc_cntrl_block *cc_block, size_t size,
			       void (*t_data_init)(void *, int))
{
	int rc;
	int i;

	M0_SET0(cc_block);
	rc = m0_semaphore_init(&cc_block->ccb_barrier, THR_NR);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < THR_NR; ++i) {
		m0_semaphore_down(&cc_block->ccb_barrier);
		cc_block->ccb_args[i].ta_data = m0_alloc(size);
		M0_UT_ASSERT(cc_block->ccb_args[i].ta_data != NULL);
		t_data_init(cc_block->ccb_args[i].ta_data, i);
		cc_block->ccb_args[i].ta_barrier = &cc_block->ccb_barrier;
	}
}

M0_INTERNAL void cc_block_launch(struct cnc_cntrl_block *cc_block,
				 void (*t_op)(void *))
{
	int i;
	int rc;

	for (i = 0; i < THR_NR; ++i) {
		rc = M0_THREAD_INIT(&cc_block->ccb_threads[i],
				    void *, NULL, t_op,
				    (void *)&cc_block->ccb_args[i], "isc_thrd");
		M0_UT_ASSERT(rc == 0);
	}
	for (i = 0; i < THR_NR; ++i) {
		m0_thread_join(&cc_block->ccb_threads[i]);
		M0_UT_ASSERT(cc_block->ccb_args[i].ta_rc == 0);
		m0_free(cc_block->ccb_args[i].ta_data);
	}
}

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
