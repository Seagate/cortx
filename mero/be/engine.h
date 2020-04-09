/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 17-Jul-2013
 */


#pragma once

#ifndef __MERO_BE_ENGINE_H__
#define __MERO_BE_ENGINE_H__

#include "lib/types.h"          /* bool */
#include "lib/mutex.h"          /* m0_mutex */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/semaphore.h"      /* m0_semaphore */

#include "be/log.h"             /* m0_be_log */
#include "be/tx.h"              /* m0_be_tx */
#include "be/tx_credit.h"       /* m0_be_tx_credit */
#include "be/tx_group.h"        /* m0_be_tx_group_cfg */

struct m0_reqh_service;
struct m0_be_tx_group;
struct m0_be_domain;
struct m0_be_engine;
struct m0_reqh;
struct m0_stob;
struct m0_be_log_discard;
struct m0_be_pd;

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

struct m0_be_engine_cfg {
	/** Number of groups. */
	size_t			   bec_group_nr;
	/**
	 * Group configuration.
	 *
	 * The following fields should be set by the user:
	 * - m0_be_tx_group_cfg::tgc_tx_nr_max;
	 * - m0_be_tx_group_cfg::tgc_seg_nr_max;
	 * - m0_be_tx_group_cfg::tgc_size_max;
	 * - m0_be_tx_group_cfg::tgc_payload_max.
	 */
	struct m0_be_tx_group_cfg  bec_group_cfg;
	/** Maximum transaction size. */
	struct m0_be_tx_credit	   bec_tx_size_max;
	/** Maximum transaction payload size. */
	m0_bcount_t		   bec_tx_payload_max;
	/**
	 * Time from "first tx is added to the group" to "group is not accepting
	 * new transactions".
	 */
	m0_time_t		   bec_group_freeze_timeout_min;
	m0_time_t		   bec_group_freeze_timeout_max;
	/** Request handler for group foms and engine timeouts */
	struct m0_reqh		  *bec_reqh;
	/** Wait in m0_be_engine_start() until recovery is finished. */
	bool			   bec_wait_for_recovery;
	/** BE domain the engine belongs to. */
	struct m0_be_domain	  *bec_domain;
	struct m0_be_log_discard  *bec_log_discard;
	struct m0_be_pd           *bec_pd;
	/** Configuration for each group. It is set by the engine. */
	struct m0_be_tx_group_cfg *bec_groups_cfg;
	/** The engine lock. Protects all fields of m0_be_engine. */
	struct m0_mutex           *bec_lock;
};

struct m0_be_engine {
	struct m0_be_engine_cfg   *eng_cfg;
	/**
	 * Per-state lists of transaction. Each non-failed transaction is in one
	 * of these lists.
	 */
	struct m0_tl               eng_txs[M0_BTS_NR + 1];
	struct m0_tl               eng_groups[M0_BGS_NR];
	/** Transactional log. */
	struct m0_be_log           eng_log;
	/** Transactional group. */
	struct m0_be_tx_group     *eng_group;
	size_t                     eng_group_nr;
	struct m0_reqh_service    *eng_service;
	uint64_t                   eng_tx_id_next;
	/**
	 * Indicates BE-engine has a transaction opened with
	 * m0_be_tx_exclusive_open() and run under exclusive conditions: no
	 * other transactions are running while @eng_exclusive_mode is set.
	 */
	bool                       eng_exclusive_mode;
	struct m0_be_domain       *eng_domain;
	struct m0_semaphore        eng_recovery_wait_sem;
	bool                       eng_recovery_finished;
};

M0_INTERNAL bool m0_be_engine__invariant(struct m0_be_engine *en);

M0_INTERNAL int m0_be_engine_init(struct m0_be_engine     *en,
				  struct m0_be_domain     *dom,
				  struct m0_be_engine_cfg *en_cfg);
M0_INTERNAL void m0_be_engine_fini(struct m0_be_engine *en);

M0_INTERNAL int m0_be_engine_start(struct m0_be_engine *en);
M0_INTERNAL void m0_be_engine_stop(struct m0_be_engine *en);

/* next functions should be called from m0_be_tx implementation */
M0_INTERNAL void m0_be_engine__tx_init(struct m0_be_engine *en,
				       struct m0_be_tx     *tx,
				       enum m0_be_tx_state  state);

M0_INTERNAL void m0_be_engine__tx_fini(struct m0_be_engine *en,
				       struct m0_be_tx     *tx);

M0_INTERNAL void m0_be_engine__tx_state_set(struct m0_be_engine *en,
					    struct m0_be_tx     *tx,
					    enum m0_be_tx_state  state);
/**
 * Forces the tx group fom to move to LOGGING state and eventually
 * commits all txs to disk.
 */
M0_INTERNAL void m0_be_engine__tx_force(struct m0_be_engine *en,
					struct m0_be_tx     *tx);

M0_INTERNAL void m0_be_engine__tx_group_ready(struct m0_be_engine   *en,
					      struct m0_be_tx_group *gr);
M0_INTERNAL void m0_be_engine__tx_group_discard(struct m0_be_engine   *en,
						struct m0_be_tx_group *gr);

M0_INTERNAL void m0_be_engine_got_log_space_cb(struct m0_be_log *log);
M0_INTERNAL void m0_be_engine_full_log_cb(struct m0_be_log *log);

M0_INTERNAL struct m0_be_tx *m0_be_engine__tx_find(struct m0_be_engine *en,
						   uint64_t             id);
M0_INTERNAL int
m0_be_engine__exclusive_open_invariant(struct m0_be_engine *en,
				       struct m0_be_tx     *excl);

M0_INTERNAL void m0_be_engine_tx_size_max(struct m0_be_engine    *en,
                                          struct m0_be_tx_credit *cred,
                                          m0_bcount_t            *payload_size);

M0_INTERNAL void m0_be_engine__group_limits(struct m0_be_engine *en,
                                            uint32_t            *group_nr,
                                            uint32_t            *tx_per_group);

/** @} end of be group */
#endif /* __MERO_BE_ENGINE_H__ */

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
