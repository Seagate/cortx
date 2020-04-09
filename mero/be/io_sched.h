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
 * Original creation date: 5-Sep-2015
 */

#pragma once

#ifndef __MERO_BE_IO_SCHED_H__
#define __MERO_BE_IO_SCHED_H__

/**
 * @defgroup be
 *
 * @{
 */

#include "lib/types.h"          /* bool */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/mutex.h"          /* m0_mutex */

struct m0_be_op;
struct m0_be_io;
struct m0_ext;

struct m0_be_io_sched_cfg {
	/** start position for m0_be_io_sched::bis_pos */
	m0_bcount_t bisc_pos_start;
};

/*
 * IO scheduler.
 *
 * It launches m0_be_io in m0_ext-based ordered queue.
 *
 * Highlighs:
 * - write I/O:
 *   - each one should have m0_ext;
 *   - m0_ext for one I/O should not intersect with m0_ext for another I/O;
 *   - I/Os are launched in the m0_ext increasing order, without gaps. If there
 *     is no such I/O in the queue at the scheduler's current position then I/O
 *     after the gap is not launched until another I/O is added to fill the gap;
 * - read I/O:
 *   - doesn't have m0_ext assigned (subject to change);
 *   - is launched after the last write I/O (at the time the read I/O is added
 *     to the scheduler's queue) from the queue is finished.
 */
struct m0_be_io_sched {
	struct m0_be_io_sched_cfg bis_cfg;
	/** list of m0_be_io-s under scheduler's control */
	struct m0_tl              bis_ios;
	struct m0_mutex           bis_lock;
	bool                      bis_io_in_progress;
	/** position for the next I/O */
	m0_bcount_t               bis_pos;
};

M0_INTERNAL int m0_be_io_sched_init(struct m0_be_io_sched     *sched,
				    struct m0_be_io_sched_cfg *cfg);
M0_INTERNAL void m0_be_io_sched_fini(struct m0_be_io_sched *sched);
M0_INTERNAL void m0_be_io_sched_lock(struct m0_be_io_sched *sched);
M0_INTERNAL void m0_be_io_sched_unlock(struct m0_be_io_sched *sched);
M0_INTERNAL bool m0_be_io_sched_is_locked(struct m0_be_io_sched *sched);
M0_INTERNAL void m0_be_io_sched_add(struct m0_be_io_sched *sched,
                                    struct m0_be_io       *io,
                                    struct m0_ext         *ext,
                                    struct m0_be_op       *op);

/** @} end of be group */
#endif /* __MERO_BE_IO_SCHED_H__ */

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
