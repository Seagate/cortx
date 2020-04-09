/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 4-Mar-2014
 */

#include "ut/threads.h"

#include "lib/memory.h"	/* M0_ALLOC_ARR */
#include "lib/thread.h" /* m0_thread */

M0_INTERNAL void m0_ut_threads_start(struct m0_ut_threads_descr *descr,
				     int			 thread_nr,
				     void			*param_array,
				     size_t			 param_size)
{
	int rc;
	int i;

	M0_PRE(descr->utd_thread_nr == 0);
	descr->utd_thread_nr = thread_nr;

	M0_ALLOC_ARR(descr->utd_thread, descr->utd_thread_nr);
	M0_ASSERT(descr->utd_thread != NULL);

	for (i = 0; i < thread_nr; ++i) {
		rc = M0_THREAD_INIT(&descr->utd_thread[i],
				    void *, NULL,
				    descr->utd_thread_func,
				    param_array + i * param_size,
				    "ut_thread%d", i);
		M0_ASSERT(rc == 0);
	}
}

M0_INTERNAL void m0_ut_threads_stop(struct m0_ut_threads_descr *descr)
{
	int rc;
	int i;

	M0_PRE(descr->utd_thread_nr > 0);

	for (i = 0; i < descr->utd_thread_nr; ++i) {
		rc = m0_thread_join(&descr->utd_thread[i]);
		M0_ASSERT(rc == 0);
		m0_thread_fini(&descr->utd_thread[i]);
	}
	m0_free(descr->utd_thread);

	descr->utd_thread    = NULL;
	descr->utd_thread_nr = 0;
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
