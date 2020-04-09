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
 * Original creation date: 1-Sep-2015
 */

#pragma once

#ifndef __MERO_BE_TX_BULK_H__
#define __MERO_BE_TX_BULK_H__

#include "lib/types.h"          /* uint32_t */
#include "lib/mutex.h"          /* m0_mutex */

/**
 * @defgroup be
 *
 * tx_bulk is an abstraction that allows user to execute uniform transactions as
 * fast as possible.
 *
 * @see m0_be_ut_tx_bulk_usecase() for an example.
 *
 * Future directions
 * - allow multiple operations in a single transaction;
 * - use m0_fom for tx state change or user operation wait;
 * - use m0_module for init()/fini().
 * - handle rc returned from tbc_next: if (!M0_IN(rc, (0, -ENOENT))) then abort
 *   tx_bulk execution.
 *
 * @{
 */

struct m0_be_op;
struct m0_be_domain;
struct m0_be_tx_credit;
struct m0_be_tx;
struct m0_be_tx_bulk;
struct be_tx_bulk_worker;

/**
 * User configuration with user-supplied callbacks for m0_be_tx_bulk.
 *
 * tbc_next(), tbc_credit() and tbc_do() should be thread-safe because they can
 * be called from any locality.
 */
struct m0_be_tx_bulk_cfg {
	/** BE domain for transactions */
	struct m0_be_domain  *tbc_dom;
	/** It is passed as parameter to tbc_next, tbc_credit and tbc_do */
	void                 *tbc_datum;
	/**
	 * Is there any remaining work to do?
	 * If there is, then set *user to the some pointer and set op rc to 0.
	 * It will be passed to tbc_credit() and tbc_do().
	 * If there is no work left, set op rc to -ENOENT.
	 *
	 * @see m0_be_op_rc_set()
	 */
	void                (*tbc_next)(struct m0_be_tx_bulk  *tb,
	                                struct m0_be_op       *op,
	                                void                  *datum,
	                                void                 **user);
	/** Accumulate m0_be_tx_credit and payload size for a new transaction */
	void                (*tbc_credit)(struct m0_be_tx_bulk   *tb,
	                                  struct m0_be_tx_credit *accum,
	                                  m0_bcount_t            *accum_payload,
	                                  void                   *datum,
	                                  void                   *user);
	/** Do some work in context of BE transaction */
	void                (*tbc_do)(struct m0_be_tx_bulk   *tb,
	                              struct m0_be_tx        *tx,
	                              struct m0_be_op        *op,
	                              void                   *datum,
	                              void                   *user);
};

struct m0_be_tx_bulk {
	struct m0_be_tx_bulk_cfg  btb_cfg;
	uint32_t                  btb_worker_nr;
	struct be_tx_bulk_worker *btb_worker;
	/** @see m0_be_tx_bulk_status */
	int                       btb_rc;
	/** protects access to m0_be_tx_bulk fields */
	struct m0_mutex           btb_lock;
	uint32_t                  btb_done_nr;
	bool                      btb_stopping;
	bool                      btb_done;
	struct m0_be_op          *btb_op;
};

M0_INTERNAL int m0_be_tx_bulk_init(struct m0_be_tx_bulk     *tb,
                                   struct m0_be_tx_bulk_cfg *tb_cfg);
M0_INTERNAL void m0_be_tx_bulk_fini(struct m0_be_tx_bulk *tb);

/**
 * Runs the work.
 * op is signalled after all work is done or some of m0_be_tx_open() failed.
 */
M0_INTERNAL void m0_be_tx_bulk_run(struct m0_be_tx_bulk *tb,
                                   struct m0_be_op      *op);
/**
 * Gets m0_be_tx_bulk result.
 * Can be called only after op from m0_be_tx_bulk_run is signalled.
 */
M0_INTERNAL int m0_be_tx_bulk_status(struct m0_be_tx_bulk *tb);


/** @} end of be group */
#endif /* __MERO_BE_TX_BULK_H__ */

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
