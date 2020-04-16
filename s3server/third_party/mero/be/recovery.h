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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 8-Dec-2014
 */

#pragma once

#ifndef __MERO_BE_RECOVERY_H__
#define __MERO_BE_RECOVERY_H__

#include "lib/tlist.h"          /* m0_tl */
#include "lib/mutex.h"          /* m0_mutex */
#include "lib/types.h"          /* bool */

/**
 * @page recovery-fspec Recovery Functional Specification
 *
 * - @ref recovery-fspec-sub
 * - @ref recovery-fspec-usecases
 *
 * @section recovery-fspec-sub Subroutines
 *
 *   - Read/write dirty bit from (non-)consistent meta segment (seg0);
 *   - Log scanning procedures including all valid log records, last written
 *     log record, pointer to the last discarded log record;
 *   - Interface for pick next log record in order which needs to be re-applied;
 *
 * @section recovery-fspec-usecases Recipes
 *
 * FIXME: insert link to sequence diagram.
 */

/**
 * @addtogroup be
 *
 * @{
 */

struct m0_be_log;
struct m0_be_log_record_iter;

struct m0_be_recovery_cfg {
	struct m0_be_log *brc_log;
};

struct m0_be_recovery {
	struct m0_be_recovery_cfg brec_cfg;
	struct m0_mutex           brec_lock;
	struct m0_tl              brec_iters;
	m0_bindex_t               brec_last_record_pos;
	m0_bcount_t               brec_last_record_size;
	m0_bindex_t               brec_current;
	m0_bindex_t               brec_discarded;
};

M0_INTERNAL void m0_be_recovery_init(struct m0_be_recovery     *rvr,
                                     struct m0_be_recovery_cfg *cfg);
M0_INTERNAL void m0_be_recovery_fini(struct m0_be_recovery *rvr);
/**
 * Scans log for all log records that need to be re-applied and stores them in
 * the list of log-only records.
 */
M0_INTERNAL int m0_be_recovery_run(struct m0_be_recovery *rvr);

M0_INTERNAL m0_bindex_t m0_be_recovery_current(struct m0_be_recovery *rvr);
M0_INTERNAL m0_bindex_t m0_be_recovery_discarded(struct m0_be_recovery *rvr);
M0_INTERNAL m0_bindex_t
m0_be_recovery_last_record_pos(struct m0_be_recovery *rvr);
M0_INTERNAL m0_bcount_t
m0_be_recovery_last_record_size(struct m0_be_recovery *rvr);

/** Returns true if there is a log record which needs to be re-applied. */
M0_INTERNAL bool
m0_be_recovery_log_record_available(struct m0_be_recovery *rvr);

/**
 * Picks the next log record from the list of log-only records. Must not be
 * called if m0_be_recovery_log_record_available() returns false.
 *
 * @param iter Log record iterator where header of the log record is stored.
 */
M0_INTERNAL void
m0_be_recovery_log_record_get(struct m0_be_recovery        *rvr,
			      struct m0_be_log_record_iter *iter);

/** @} end of be group */

#endif /* __MERO_BE_RECOVERY_H__ */

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
