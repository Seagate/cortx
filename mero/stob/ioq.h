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
 * Original creation date: 12-Mar-2014
 */

#pragma once

#ifndef __MERO_STOB_IOQ_H__
#define __MERO_STOB_IOQ_H__

#include <libaio.h>        /* io_context_t */

#include "lib/types.h"     /* bool */
#include "lib/atomic.h"    /* m0_atomic64 */
#include "lib/thread.h"    /* m0_thread */
#include "lib/mutex.h"     /* m0_mutex */
#include "lib/queue.h"     /* m0_queue */
#include "lib/timer.h"     /* m0_timer */
#include "lib/semaphore.h" /* m0_semaphore */

/**
 * @defgroup stoblinux
 *
 * @{
 */

struct m0_stob;
struct m0_stob_io;

enum {
	/** Default number of threads to create in a storage object domain. */
	M0_STOB_IOQ_NR_THREADS     = 8,
	/** Default size of a ring buffer shared by adieu and the kernel. */
	M0_STOB_IOQ_RING_SIZE      = 1024,
	/** Size of a batch in which requests are moved from the admission queue
	    to the ring buffer. */
	M0_STOB_IOQ_BATCH_IN_SIZE  = 8,
	/** Size of a batch in which completion events are extracted from the
	    ring buffer. */
	M0_STOB_IOQ_BATCH_OUT_SIZE = 8,
};

struct m0_stob_ioq {
	/**
	 *  Controls whether to use O_DIRECT flag for open(2).
	 *  Can be set with m0_stob_ioq_directio_setup().
	 *  Initial value is set to 'false'.
	 */
	bool                     ioq_use_directio;
	/** Set up when domain is being shut down. adieu worker threads
	    (ioq_thread()) check this field on each iteration. */
	/**
	    Ring buffer shared between adieu and the kernel.

	    It contains adieu request fragments currently being executed by the
	    kernel. The kernel delivers AIO completion events through this
	    buffer. */
	io_context_t             ioq_ctx;
	/** Free slots in the ring buffer. */
	struct m0_atomic64       ioq_avail;
	/** Used slots in the ring buffer. */
	int                      ioq_queued;
	/** Worker threads. */
	struct m0_thread         ioq_thread[M0_STOB_IOQ_NR_THREADS];

	/** Mutex protecting all ioq_ fields (except for the ring buffer that is
	    updated by the kernel asynchronously). */
	struct m0_mutex          ioq_lock;
	/** Admission queue where adieu request fragments are kept until there
	    is free space in the ring buffer.  */
	struct m0_queue          ioq_queue;
	struct m0_semaphore      ioq_stop_sem[M0_STOB_IOQ_NR_THREADS];
	struct m0_timer          ioq_stop_timer[M0_STOB_IOQ_NR_THREADS];
	struct m0_timer_locality ioq_stop_timer_loc[M0_STOB_IOQ_NR_THREADS];
};

M0_INTERNAL int m0_stob_ioq_init(struct m0_stob_ioq *ioq);
M0_INTERNAL void m0_stob_ioq_fini(struct m0_stob_ioq *ioq);
M0_INTERNAL void m0_stob_ioq_directio_setup(struct m0_stob_ioq *ioq,
					    bool use_directio);

M0_INTERNAL bool m0_stob_ioq_directio(struct m0_stob_ioq *ioq);
M0_INTERNAL uint32_t m0_stob_ioq_bshift(struct m0_stob_ioq *ioq);
M0_INTERNAL m0_bcount_t m0_stob_ioq_bsize(struct m0_stob_ioq *ioq);
M0_INTERNAL m0_bcount_t m0_stob_ioq_bmask(struct m0_stob_ioq *ioq);

M0_INTERNAL int m0_stob_linux_io_init(struct m0_stob *stob,
				      struct m0_stob_io *io);

/** @} end of stoblinux group */
#endif /* __MERO_STOB_IOQ_H__ */

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
