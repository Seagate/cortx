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
 * Original author: Anatoliy Bilenko <Anatoliy.Bilenko@seagate.com>
 * Original creation date: 21-Nov-2015
 */

#pragma once

#ifndef __MERO_LIB_THREAD_POOL_H__
#define __MERO_LIB_THREAD_POOL_H__

/**
 * @defgroup thread_pool Thread pool
 *
 * Simple parallel pool abstraction gives user controlled number of threads
 * allowing him to add its own jobs for processing. User operates pool by
 * adding its set of jobs and processing function which is called on every such
 * job.
 *
 * Pools have the following life-cycle:
 *
 * // user creates pool, number of threads and `jobs processed at once' are set:
 * struct m0_parallel_pool p = {};
 * m0_parallel_pool_init(&p, THREADS_NR, JOBS_AT_ONCE_NR);
 *
 * // something can be done even repeatingly
 * forall(subjobs:jobs) {
 *     // then user has to add jobs to process with given on the next step
 *     // function
 *     forall (job:subjobs)
 *        m0_parallel_pool_job_add(&p, job);
 *
 *     // then pool starts job processing, `int job_process(void* job)' is being
 *     // applied to every given job
 *     m0_parallel_pool_start(&p, &job_process);
 *
 *     // user may wait job processing end; m0_parallel_pool_wait()
 *     // returns -EINTR if any `int job_process(void* job)' returned error.
 *     rc = m0_parallel_pool_wait(&p);
 *     // handle `rc' if needed.
 *     // ...
 * }
 *
 * // in some cases `jobs' are linked into lists, therefore M0_PARALLEL_FOR()
 * // can be used to perform the same as in loop above;
 * // assume `ljobs' is a typed list with corresponding prefix `m0_ljobs':
 *
 * // equivalent to `forall(subjobs:jobs)' for typed lists
 * rc = M0_PARALLEL_FOR(m0_ljobs, &p, &ljobs, &job_process);
 *
 * // after all, pool has to be terminated and then finalized; terminate waits
 * // until all threads in pool are joined.
 * m0_parallel_pool_terminate_wait(&p);
 * m0_parallel_pool_fini(&p);
 *
 * @{
 */

#include "lib/queue.h"
#include "lib/mutex.h"
#include "lib/thread.h"
#include "lib/semaphore.h"

/* pool */

struct m0_parallel_queue_link;
struct m0_parallel_queue;

struct m0_parallel_pool {
	struct m0_semaphore            pp_sync;
	struct m0_semaphore            pp_ready;
	bool                           pp_done;

	int                          (*pp_process)(void *job);
	struct m0_thread              *pp_threads;
	int                            pp_thread_nr;

	struct m0_parallel_queue      *pp_queue;
	struct m0_parallel_queue_link *pp_qlinks;
	int                            pp_qlinks_nr;

	int                            pp_state;
	int                            pp_next_rc;
};

M0_INTERNAL int m0_parallel_pool_init(struct m0_parallel_pool *pool,
				      int thread_nr, int qlinks_nr);
M0_INTERNAL void m0_parallel_pool_fini(struct m0_parallel_pool *pool);
M0_INTERNAL int  m0_parallel_pool_job_add(struct m0_parallel_pool *pool,
					  void *job);
M0_INTERNAL int  m0_parallel_pool_wait(struct m0_parallel_pool *pool);
M0_INTERNAL void m0_parallel_pool_start(struct m0_parallel_pool *pool,
					int (*process)(void *job));
M0_INTERNAL void m0_parallel_pool_terminate_wait(struct m0_parallel_pool *pool);
M0_INTERNAL int  m0_parallel_pool_rc_next(struct m0_parallel_pool *pool,
					  void **job, int *rc);

/* parallel for macro */

#define M0_PARALLEL_FOR(name, pool, list, process)                      \
({                                                                      \
	int rc = -EINVAL;                                               \
	typeof (name ## _tlist_head(NULL)) obj;                         \
	m0_tl_for(name, list, obj) {                                    \
		rc = m0_parallel_pool_job_add(pool, obj);               \
		if (rc == -EFBIG) {                                     \
			m0_parallel_pool_start(pool, process);          \
			rc = m0_parallel_pool_wait(pool);               \
			if (rc != 0)                                    \
				break;                                  \
			rc = m0_parallel_pool_job_add(pool, obj);       \
			M0_POST(rc == 0);                               \
		}                                                       \
	} m0_tl_endfor;                                                 \
	if (rc == 0) {                                                  \
		m0_parallel_pool_start(pool, process);                  \
		rc = m0_parallel_pool_wait(pool);                       \
	}                                                               \
	(rc);                                                           \
})

/** @} end of thread_pool group */
#endif /* __MERO_LIB_THREAD_POOL_H__ */

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
