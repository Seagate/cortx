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

#pragma once

#ifndef __MERO_BE_PD_H__
#define __MERO_BE_PD_H__

#include "lib/tlist.h"          /* m0_tl */
#include "lib/types.h"          /* uint32_t */

#include "be/io_sched.h"        /* m0_be_io_sched */
#include "be/io.h"              /* m0_be_io_credit */
#include "be/pool.h"            /* m0_be_pool */


/**
 * @defgroup be
 *
 * @{
 */

struct m0_ext;
struct m0_be_io;
struct m0_be_op;
struct m0_be_pd_io;

enum m0_be_pd_io_state {
	/* io is ready to be taken using m0_be_pd_io_get() */
	M0_BPD_IO_IDLE,
	/* io is owned by m0_be_pd::bpd_sched */
	M0_BPD_IO_IN_PROGRESS,
	/* m0_be_pd::bpd_sched had reported about io completion. */
	M0_BPD_IO_DONE,
	M0_BPD_IO_STATE_NR,
};

struct m0_be_pd_cfg {
	struct m0_be_io_sched_cfg bpdc_sched;
	uint32_t                  bpdc_seg_io_nr;
	uint32_t                  bpdc_seg_io_pending_max;
	struct m0_be_io_credit    bpdc_io_credit;
};

struct m0_be_pd {
	struct m0_be_pd_cfg    bpd_cfg;
	struct m0_be_io_sched  bpd_sched;
	struct m0_be_pd_io    *bpd_io;
	struct m0_be_pool      bpd_io_pool;

	struct m0_be_op       *bpd_sync_op;
	m0_time_t              bpd_sync_delay;
	m0_time_t              bpd_sync_runtime;
	m0_time_t              bpd_sync_prev;
	struct m0_be_io        bpd_sync_io;
	bool                   bpd_sync_in_progress;
	char                   bpd_sync_read_to[2];
	struct m0_sm_ast       bpd_sync_ast;
};

M0_INTERNAL int m0_be_pd_init(struct m0_be_pd *pd, struct m0_be_pd_cfg *pd_cfg);
M0_INTERNAL void m0_be_pd_fini(struct m0_be_pd *pd);

M0_INTERNAL void m0_be_pd_io_add(struct m0_be_pd    *pd,
                                 struct m0_be_pd_io *pdio,
                                 struct m0_ext      *ext,
                                 struct m0_be_op    *op);

M0_INTERNAL void m0_be_pd_io_get(struct m0_be_pd     *pd,
				 struct m0_be_pd_io **pdio,
				 struct m0_be_op     *op);
M0_INTERNAL void m0_be_pd_io_put(struct m0_be_pd    *pd,
				 struct m0_be_pd_io *pdio);

M0_INTERNAL struct m0_be_io *m0_be_pd_io_be_io(struct m0_be_pd_io *pdio);

/**
 * Run fdatasync() for the given set of stobs.
 *
 * @note pos parameter is ignored for now.
 */
M0_INTERNAL void m0_be_pd_sync(struct m0_be_pd  *pd,
                               m0_bindex_t       pos,
                               struct m0_stob  **stobs,
                               int               nr,
                               struct m0_be_op  *op);

/** @} end of be group */
#endif /* __MERO_BE_PD_H__ */

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
