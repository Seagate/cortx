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

#pragma once

#ifndef __MERO_BE_LOG_SCHED_H__
#define __MERO_BE_LOG_SCHED_H__

#include "lib/types.h"          /* m0_bindex_t */
#include "be/io.h"              /* m0_be_io */
#include "be/io_sched.h"        /* m0_be_io_sched */
#include "be/op.h"              /* m0_be_op */

/**
 * @defgroup be
 *
 * @{
 */

struct m0_be_log_record;

struct m0_be_log_sched_cfg {
	struct m0_be_io_sched_cfg lsch_io_sched_cfg;
};

/*
 * Log I/O scheduler.
 *
 * Log scheduler is used when log records and log header are read or
 * written to the log. Log store header doesn't use log scheduler for I/O.
 *
 * @see m0_be_io_sched
 */
struct m0_be_log_sched {
	struct m0_be_io_sched lsh_io_sched;
	m0_bindex_t           lsh_pos;
};

/** @todo document fields owned by m0_be_log and move fields reset there */
struct m0_be_log_io {
	uint32_t                 lio_log_bshift;
	struct m0_buf            lio_buf;
	void                    *lio_buf_addr;
	m0_bcount_t              lio_buf_size;
	struct m0_bufvec         lio_bufvec;
	struct m0_be_io          lio_be_io;
	struct m0_be_log_sched  *lio_sched;
	struct m0_be_log_record *lio_record;
	void                    *lio_user_data;
};

M0_INTERNAL int m0_be_log_sched_init(struct m0_be_log_sched     *sched,
				     struct m0_be_log_sched_cfg *cfg);
M0_INTERNAL void m0_be_log_sched_fini(struct m0_be_log_sched *sched);
M0_INTERNAL void m0_be_log_sched_lock(struct m0_be_log_sched *sched);
M0_INTERNAL void m0_be_log_sched_unlock(struct m0_be_log_sched *sched);
M0_INTERNAL bool m0_be_log_sched_is_locked(struct m0_be_log_sched *sched);
/*
 * Add lio to the scheduler queue.
 *
 * @see m0_be_log_sched
 */
M0_INTERNAL void m0_be_log_sched_add(struct m0_be_log_sched *sched,
				     struct m0_be_log_io    *lio,
				     struct m0_be_op        *op);

M0_INTERNAL int m0_be_log_io_init(struct m0_be_log_io *lio);
M0_INTERNAL void m0_be_log_io_fini(struct m0_be_log_io *lio);
M0_INTERNAL void m0_be_log_io_reset(struct m0_be_log_io *lio);

M0_INTERNAL int m0_be_log_io_allocate(struct m0_be_log_io    *lio,
				      struct m0_be_io_credit *iocred,
				      uint32_t                log_bshift);
M0_INTERNAL void m0_be_log_io_deallocate(struct m0_be_log_io *lio);

M0_INTERNAL struct m0_bufvec *m0_be_log_io_bufvec(struct m0_be_log_io *lio);
M0_INTERNAL struct m0_be_io *m0_be_log_io_be_io(struct m0_be_log_io *lio);

M0_INTERNAL void m0_be_log_io_user_data_set(struct m0_be_log_io *lio,
					    void                *data);
M0_INTERNAL void *m0_be_log_io_user_data(struct m0_be_log_io *lio);
M0_INTERNAL bool m0_be_log_io_is_empty(struct m0_be_log_io *lio);


/** @} end of be group */
#endif /* __MERO_BE_LOG_SCHED_H__ */

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
