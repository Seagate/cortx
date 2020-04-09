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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 17-Jun-2013
 */

#pragma once
#ifndef __MERO_BE_TX_GROUP_H__
#define __MERO_BE_TX_GROUP_H__

#include "lib/tlist.h"          /* m0_tl */

#include "be/tx_credit.h"       /* m0_be_tx_credit */
#include "be/tx_group_format.h" /* m0_be_group_format */
#include "be/tx_group_fom.h"    /* m0_be_group_fom */
#include "be/tx_regmap.h"       /* m0_be_reg_area */
#include "be/tx.h"              /* m0_be_tx_state */

struct m0_be_engine;
struct m0_be_domain;
struct m0_be_tx;
struct be_recovering_tx;
struct m0_be_op;

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

enum m0_be_tx_group_state {
	M0_BGS_READY,
	M0_BGS_OPEN,
	M0_BGS_FROZEN,
	M0_BGS_CLOSED,
	M0_BGS_NR,
};

struct m0_be_tx_group_cfg {
	/** Maximum number of transactions in the group */
	unsigned long		       tgc_tx_nr_max;
	/** Maximum number of segments regions of captured tx belongs to. */
	unsigned long		       tgc_seg_nr_max;
	/** Maximum size of the group reg_area. */
	struct m0_be_tx_credit	       tgc_size_max;
	/**
	 * Maximum total size of tx payload from the group.
	 * Total size is calculated as sum of tx payload size.
	 */
	m0_bcount_t		       tgc_payload_max;
	/** domain contains tgc_engine. */
	struct m0_be_domain	      *tgc_domain;
	/** engine the group belongs to. */
	struct m0_be_engine	      *tgc_engine;
	/** log to write to for the group_format. */
	struct m0_be_log	      *tgc_log;
	struct m0_be_log_discard      *tgc_log_discard;
	struct m0_be_pd               *tgc_pd;
	/** reqh for the group fom. */
	struct m0_reqh		      *tgc_reqh;
	/** Group format configuration. Is set by the group. */
	struct m0_be_group_format_cfg  tgc_format;
};

/**
 * Transaction group is a collection of transactions, consecutive in closing
 * order, that are written to the log and recovered together.
 *
 * A new group is initially empty. It absorbs transactions as they are closed,
 * until the group either grows larger than m0_be_log::lg_gr_size_max or grows
 * older than XXX. At that point, the group is closed and a new group is opened
 * up to an upper limit (XXX) on groups number (1 is a possible and reasonable
 * such limit).
 *
 * Once a group is closed it constructs in memory its persistent representation,
 * consisting of group header, sequence of transaction representations and group
 * commit block. The memory for this representation is pre-allocated in
 * transaction engine to avoid dead-locks. Once representation is constructed,
 * it is written to the log in one or multiple asynchronous IOs. Before group
 * commit block is written, all other IOs for the group must complete. After
 * that, the commit block is written. Once commit block IO completes, it is
 * guaranteed that the entire group is on the log. Waiting for IO completion can
 * be eliminated by using (currently unimplemented) barrier interface provided
 * by m0_stob, or by placing in the commit block a strong checksum of group
 * representation (the latter approach allows to check whether the entire group
 * made it to the log).
 */
struct m0_be_tx_group {
	struct m0_be_tx_group_cfg  tg_cfg;
	/** Total size of all updates in all transactions in this group. */
	struct m0_be_tx_credit     tg_used;
	struct m0_be_tx_credit     tg_size;
	struct m0_be_tx_credit     tg_log_reserved;
	m0_bcount_t                tg_payload_prepared;
	/**
	 * The number of transactions that have not reached M0_BTS_CLOSED state.
	 */
	uint32_t                   tg_nr_unclosed;
	/**
	 * The number of transactions that have not reached M0_BTS_DONE state.
	 */
	uint32_t                   tg_nr_unstable;
	/** List of transactions in the group. */
	struct m0_tl               tg_txs;
	/** List of recovering transactions in the group. */
	struct m0_tl               tg_txs_recovering;
	/** Preallocated array of recovering transactions. */
	struct be_recovering_tx   *tg_rtxs;
	/* Linkage for engine lists (m0_be_engine::eng_txs). */
	struct m0_tlink            tg_engine_linkage;
	/* Magic for tg_engine_linkage. */
	uint64_t                   tg_magic;
	/** XXX DOCUMENTME */
	struct m0_be_group_format  tg_od;               /* XXX rename */
	struct m0_be_log          *tg_log;
	struct m0_be_domain       *tg_domain;
	struct m0_be_engine       *tg_engine;
	struct m0_be_tx_group_fom  tg_fom;
	struct m0_be_reg_area      tg_reg_area;
	bool                       tg_recovering;
	struct m0_be_reg_area_merger  tg_merger;
	/**
	 * Fields for BE engine
	 */
	struct m0_sm_timer         tg_close_timer;
	struct m0_sm_ast           tg_close_timer_arm;
	struct m0_sm_ast           tg_close_timer_disarm;
	m0_time_t                  tg_close_deadline;
	/** Group state. Is used and set by the engine. */
	enum m0_be_tx_group_state  tg_state;
};

M0_INTERNAL bool m0_be_tx_group__invariant(struct m0_be_tx_group *gr);

/* ------------------------------------------------------------------
 *                  Interfaces used by m0_be_engine
 * ------------------------------------------------------------------ */

M0_INTERNAL int m0_be_tx_group_init(struct m0_be_tx_group     *gr,
				    struct m0_be_tx_group_cfg *gr_cfg);
M0_INTERNAL void m0_be_tx_group_fini(struct m0_be_tx_group *gr);

M0_INTERNAL int m0_be_tx_group_start(struct m0_be_tx_group *gr);
M0_INTERNAL void m0_be_tx_group_stop(struct m0_be_tx_group *gr);

/** Adds the transaction to m0_be_tx_group::tg_txs. */
M0_INTERNAL int m0_be_tx_group_tx_add(struct m0_be_tx_group *gr,
				      struct m0_be_tx       *tx);
/** Number of transactions in the group. */
M0_INTERNAL size_t m0_be_tx_group_tx_nr(struct m0_be_tx_group *gr);

M0_INTERNAL void m0_be_tx_group_tx_closed(struct m0_be_tx_group *gr,
                                          struct m0_be_tx       *tx);

M0_INTERNAL void m0_be_tx_group_close(struct m0_be_tx_group *gr);

/**
 * Notifies the group that all of its transactions have reached M0_BTS_DONE
 * state.
 */
M0_INTERNAL void m0_be_tx_group_stable(struct m0_be_tx_group *gr);

M0_INTERNAL struct m0_sm_group *
m0_be_tx_group__sm_group(struct m0_be_tx_group *gr);
M0_INTERNAL bool m0_be_tx_group_is_recovering(struct m0_be_tx_group *gr);

/* ------------------------------------------------------------------
 *              Interfaces used by m0_be_tx_group_fom
 * ------------------------------------------------------------------ */

/** Makes the tx_group ready for reuse. */
M0_INTERNAL void m0_be_tx_group_reset(struct m0_be_tx_group *gr);

M0_INTERNAL void m0_be_tx_group_prepare(struct m0_be_tx_group *gr,
                                        struct m0_be_op       *op);

/** Deletes the transaction from m0_be_tx_group::tg_txs. */
M0_INTERNAL void m0_be_tx_group_tx_del(struct m0_be_tx_group *gr,
				       struct m0_be_tx       *tx);
M0_INTERNAL void m0_be_tx_group_open(struct m0_be_tx_group *gr);

/**
 * m0_be_tx__state_post()s each transaction of the group.
 * XXX RENAMEME
 */
M0_INTERNAL void
m0_be_tx_group__tx_state_post(struct m0_be_tx_group *gr,
			      enum m0_be_tx_state    state,
			      bool                   del_tx_from_group);
M0_INTERNAL int m0_be_tx_group__allocate(struct m0_be_tx_group *gr);
M0_INTERNAL void m0_be_tx_group__deallocate(struct m0_be_tx_group *gr);

M0_INTERNAL void m0_be_tx_group_seg_place_prepare(struct m0_be_tx_group *gr);
M0_INTERNAL void m0_be_tx_group_seg_place(struct m0_be_tx_group *gr,
					  struct m0_be_op       *op);
M0_INTERNAL void m0_be_tx_group_encode(struct m0_be_tx_group *gr);

M0_INTERNAL void m0_be_tx_group_log_write(struct m0_be_tx_group *gr,
					  struct m0_be_op       *op);

/* ------------------------------------------------------------------
 *                      Interfaces used by recovery.
 * ------------------------------------------------------------------ */

M0_INTERNAL void m0_be_tx_group_recovery_prepare(struct m0_be_tx_group *gr,
						 struct m0_be_log      *log);
M0_INTERNAL void m0_be_tx_group_log_read(struct m0_be_tx_group *gr,
					 struct m0_be_op       *op);
M0_INTERNAL int m0_be_tx_group_decode(struct m0_be_tx_group *gr);
M0_INTERNAL int m0_be_tx_group_reconstruct(struct m0_be_tx_group *gr,
					   struct m0_sm_group    *sm_grp);
M0_INTERNAL void m0_be_tx_group_reconstruct_tx_open(struct m0_be_tx_group *gr,
						    struct m0_be_op       *op);
/** @note Don't wait on op after the function call. */
M0_INTERNAL void
m0_be_tx_group_reconstruct_tx_close(struct m0_be_tx_group *gr,
                                    struct m0_be_op       *op_gc);
M0_INTERNAL int m0_be_tx_group_reapply(struct m0_be_tx_group *gr,
				       struct m0_be_op       *op);

/* ------------------------------------------------------------------
 *                      Interfaces used by domain.
 * ------------------------------------------------------------------ */

M0_INTERNAL void m0_be_tx_group_discard(struct m0_be_log_discard      *ld,
                                        struct m0_be_log_discard_item *ldi);

M0_INTERNAL void
m0_be_tx_group_seg_io_credit(struct m0_be_tx_group_cfg *gr_cfg,
                             struct m0_be_io_credit    *io_cred);

/* XXX see comment in be/tx_group.c */
#if 0
M0_INTERNAL void tx_group_init(struct m0_be_tx_group *gr,
			       struct m0_stob *log_stob);
M0_INTERNAL void tx_group_fini(struct m0_be_tx_group *gr);

M0_INTERNAL void tx_group_add(struct m0_be_tx_engine *eng,
			      struct m0_be_tx_group *gr,
			      struct m0_be_tx *tx);
M0_INTERNAL void tx_group_close(struct m0_be_tx_engine *eng,
				struct m0_be_tx_group *gr);
/* Note the absence of tx_group_open(). */
#endif

#define M0_BE_TX_GROUP_TX_FORALL(gr, tx) \
	m0_tl_for(grp, &(gr)->tg_txs, (tx))

#define M0_BE_TX_GROUP_TX_ENDFOR m0_tl_endfor

M0_TL_DESCR_DECLARE(grp, M0_EXTERN);
M0_TL_DECLARE(grp, M0_INTERNAL, struct m0_be_tx);

/** @} end of be group */
#endif /* __MERO_BE_TX_GROUP_H__ */

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
