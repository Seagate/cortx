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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 29-May-2013
 */

#pragma once
#ifndef __MERO_BE_TX_H__
#define __MERO_BE_TX_H__

#include "lib/misc.h"           /* M0_BITS */
#include "lib/buf.h"            /* m0_buf */

#include "sm/sm.h"              /* m0_sm */

#include "be/tx_regmap.h"       /* m0_be_reg_area */

struct m0_be_domain;
struct m0_be_tx_group;
struct m0_be_fmt_tx;
struct m0_fol_rec;

/**
 * @defgroup be Meta-data back-end
 *
 * Transaction engine.
 *
 * Main abstractions provided by this module are:
 *
 *     - transaction m0_be_tx: is a group of updates to BE segment memory,
 *       atomic w.r.t. BE failures. A transaction can update memory within
 *       multiple segments in the same m0_be_domain. A BE user creates a
 *       transaction and then updates segment memory. After each update, the
 *       updated memory is "captured" in the transaction by calling
 *       m0_be_tx_capture();
 *
 *     - transaction credit m0_be_tx_credit: an object describing an update to
 *       segment memory that a transaction could make. Before a memory update
 *       can be captured in a transaction, the transaction should be "prepared"
 *       (m0_be_tx_prep()) for all forthcoming captures. This preparation
 *       reserves internal transaction resources (log space and memory) to avoid
 *       dead-locks;
 *
 *     - transaction engine m0_be_tx_engine: is a part of BE domain that
 *       contains all transaction related state.
 *
 * Overview of operation.
 *
 * When a memory region is captured in a transaction, the contents of this
 * region, i.e., new values placed in the memory by the user, are copied in a
 * transaction-private memory buffer. Eventually the transaction is closed,
 * i.e., the user indicates that no more updates will be captured in the
 * transaction. Closed transactions are collected in "transaction groups"
 * (m0_be_tx_group), which are units of IO. When a group is formed it is written
 * to the log. When log IO for the group completes, transactions from the group
 * are written "in-place", that is, their captured updates are written to the
 * corresponding places in the segment storage. Some time after a transaction is
 * written in-place, it is discarded and the space it uses in the log is
 * reclaimed.
 *
 * Notes:
 *
 *     - transaction engine implements redo-only WAL (write-ahead logging).
 *       There is no way to abort a transaction;
 *
 *     - serializibility and concurrency concerns are delegated to a user.
 *       Specifically, the user must call m0_be_tx_capture() while the lock,
 *       protecting the memory being captured is held. In the current
 *       implementation this lock must be held at least until the transaction
 *       is closed, in the future this requirement will be weakened;
 *
 *     - currently, the transaction engine writes modified memory in place,
 *       as described in the "Overview of operation" section. In the future,
 *       the transaction engine would leave this task to the (currently
 *       non-existing) segment page daemon;
 *
 *     - transaction close call (m0_be_tx_close()) does not guarantee
 *       transaction persistence. Transaction will become persistent later.
 *       The user can set a call-back m0_be_tx::t_persistent() that is called
 *       when the transaction becomes persistent;
 *
 *     - transactions become persistent in the same order as they close.
 *
 * @{
 */

/**
 * Transaction state machine.
 *
 * @verbatim
 *
 *                        | m0_be_tx_init()
 *                        |
 *                        V
 *                     PREPARE
 *                        |
 *        m0_be_tx_open() |   no free memory or engine thinks that
 *                        |   the transaction can't be opened
 *                        V                V
 *                     OPENING---------->FAILED
 *                        |
 *                        | log space reserved for the transaction
 *                        |
 *                        V
 *                      ACTIVE
 *                        |
 *       m0_be_tx_close() |
 *                        |
 *                        V
 *                      CLOSED
 *                        |
 *                        | added to group
 *                        |
 *                        V
 *                     GROUPED
 *                        |
 *                        | log io complete
 *                        |
 *                        V
 *                     LOGGED
 *                        |
 *                        | in-place io complete
 *                        |
 *                        V
 *                      PLACED
 *                        |
 *                        | number of m0_be_tx_get() == number of m0_be_tx_put()
 *                        | for the transaction
 *                        V
 *                      DONE
 *
 * @endverbatim
 *
 * A transaction goes through the states sequentially. The table below
 * corresponds to sequence of transactions in the system history. An individual
 * transaction, as it gets older, moves through this table bottom-up.
 *
 * @verbatim
 *
 *      transaction          log record
 *         state                state
 * |                    |                     |
 * |                    |                     |
 * | DONE               |  record discarded   |
 * | (updates in place) |                     |
 * |                    |                     |
 * +--------------------+---------------------+----> start
 * |                    |                     |
 * | PLACED             |  persistent         |
 * | (updates in place  |                     |
 * |  and in log)       |                     |
 * |                    |                     |
 * +--------------------+---------------------+----> placed
 * |                    |                     |
 * | ``LOGGED''         |  persistent         |
 * | (updates in log)   |                     |
 * |                    |                     |
 * +--------------------+---------------------+----> logged
 * |                    |                     |
 * | ``SUBMITTED''      |  in flight          |
 * | (updates in flight |                     |
 * |  to log)           |                     |
 * |                    |                     |
 * +--------------------+---------------------+----> submitted
 * |                    |                     |
 * | GROUPED            |  in memory,         |
 * | (grouped)          |  log location       |
 * |                    |  assigned           |
 * |                    |                     |
 * +--------------------+---------------------+----> grouped
 * |                    |                     |
 * | CLOSED             |  in memory,         |
 * | (ungrouped)        |  log location       |
 * |                    |  not assigned       |
 * |                    |                     |
 * +--------------------+---------------------+----> inmem
 * |                    |                     |
 * | ACTIVE             |  in memory,         |
 * | (capturing         |  log space          |
 * |  updates)          |  reserved           |
 * |                    |                     |
 * +--------------------+---------------------+----> prepared
 * |                    |                     |
 * | OPENING            |  no records         |
 * | (waiting for log   |                     |
 * |  space             |                     |
 * |                    |                     |
 * +--------------------+---------------------+
 * |                    |                     |
 * | PREPARE            | no records          |
 * | (accumulating      |                     |
 * |  credits)          |                     |
 * +--------------------+---------------------+
 *
 * @endverbatim
 *
 */
enum m0_be_tx_state {
	/**
	 * Transaction failed. It cannot be used further and should be finalised
	 * (m0_be_tx_fini()).
	 *
	 * Currently, the only way a transaction can reach this state is by
	 * failing to allocate internal memory in m0_be_tx_open() call or by
	 * growing too large (larger than the total log space) in prepare state.
	 */
	M0_BTS_FAILED = 1,
	/**
	 * State in which transaction is being prepared to opening; initial
	 * state after m0_be_tx_init().
	 *
	 * In this state, m0_be_tx_prep() calls should be made to reserve
	 * internal resources for the future captures. It is allowed to prepare
	 * for more than will be actually captured: typically it is impossible
	 * to precisely estimate updates that will be done as part of
	 * transaction, so a user should conservatively prepare for the
	 * worst-case.
	 */
	M0_BTS_PREPARE,
	/**
	 * In this state transaction waits for internal resource to be
	 * allocated.
	 *
	 * Specifically, the transaction is in this state until there is enough
	 * free space in the log to store transaction updates.
	 */
	M0_BTS_OPENING,
	/**
	 * XXX Transaction is a member of transaction group.
	 */
	M0_BTS_GROUPING,
	/**
	 * In this state transaction is used to capture updates.
	 */
	M0_BTS_ACTIVE,
	/**
	 * Transaction is closed.
	 */
	M0_BTS_CLOSED,
	/*
	 * All transaction updates made it to the log.
	 */
	M0_BTS_LOGGED,
	/**
	 * All transaction in-place updates completed.
	 */
	M0_BTS_PLACED,
	/**
	 * Transaction reached M0_BTS_PLACED state and the number of
	 * m0_be_tx_get() is equal to the number of m0_be_tx_put()
	 * for the transaction.
	 */
	M0_BTS_DONE,
	M0_BTS_NR
};

/*
 * NOTE: Call-backs of this type must be asynchronous, because they can be
 * called from state transition functions.
 */
typedef void (*m0_be_tx_cb_t)(const struct m0_be_tx *tx);

/** Transaction. */
struct m0_be_tx {
	struct m0_sm           t_sm;

	/** Transaction identifier, assigned by the engine. */
	uint64_t               t_id;
	struct m0_be_engine   *t_engine;
	/**
	 * The BE domain can't be changed during the entire lifetime of
	 * the transaction, so it's safe to read this field at any moment
	 * between m0_be_tx_init() is called and the transaction is finalised.
	 */
	struct m0_be_domain   *t_dom;

	/** Linkage in one of m0_be_tx_engine::eng_txs[] lists. */
	struct m0_tlink        t_engine_linkage;
	/** Linkage in m0_be_tx_group::tg_txs. */
	struct m0_tlink        t_group_linkage;
	uint64_t               t_magic;

	/** Updates prepared for at PREPARE state. */
	struct m0_be_tx_credit t_prepared;
	struct m0_be_reg_area  t_reg_area;

	/**
	 * Optional call-back called when the transaction is guaranteed to
	 * survive all further failures. This is invoked upon log IO
	 * completion.
	 */
	m0_be_tx_cb_t          t_persistent;

	/**
	 * This optional call-back is called when a stable transaction is about
	 * to be discarded from the history.
	 *
	 * A typical user of this call-back is ioservice that uses ->t_discarded
	 * to initiate a new transaction to free storage space used by the
	 * COW-ed file extents.
	 */
	m0_be_tx_cb_t          t_discarded;

	/**
	 * XXX update.
	 * An optional call-back called when the transaction is being closed.
	 *
	 * "payload" parameter is the pointer to a m0_be_tx::t_payload_size-d
	 * buffer, that will be written to the log.
	 *
	 * ->t_filler() can capture regions in the transaction.
	 *
	 * A typical use of this call-back is to form a "fol record" used by DTM
	 * for distributed transaction management.
	 */
	void                 (*t_filler)(struct m0_be_tx *tx, void *payload);

	/**
	 * User-specified value, associated with the transaction. Transaction
	 * engine doesn't interpret this value. It can be used to pass
	 * additional information to the call-backs.
	 */
	void                  *t_datum;
	/**
	 * lsn of transaction representation in the log. Assigned when the
	 * transaction reaches GROUPED state.
	 */
	uint64_t               t_lsn;
	/**
	 * Payload area.
	 *
	 * - memory for the payload area is managed by the transaction. It is
	 *   allocated when transaction opens and it is deallocated inside
	 *   m0_be_tx_fini();
	 * - user should call m0_be_tx_payload_prep() at M0_BTS_PREPARE state
	 *   to accumulate payload area size.
	 *
	 * @todo Don't allocate m0_be_tx::t_payload separately.
	 *       Use m0_be_tx_group preallocated payload area.
	 * @todo Use m0_be_tx::t_filler callback to fill m0_be_tx::t_payload.
	 */
	struct m0_buf          t_payload;
	m0_bcount_t            t_payload_prepared;
	struct m0_sm_ast       t_ast_grouping;
	struct m0_sm_ast       t_ast_active;
	struct m0_sm_ast       t_ast_failed;
	struct m0_sm_ast       t_ast_logged;
	struct m0_sm_ast       t_ast_placed;
	struct m0_sm_ast       t_ast_done;
	struct m0_be_tx_group *t_group;
	/** Reference counter. */
	uint32_t               t_ref;
	/** Set when space in log is reserved by engine. */
	bool                   t_log_reserved;
	/* XXX TODO merge with t_log_reserved. */
	m0_bcount_t            t_log_reserved_size;
	/**
	 * Flag indicates that tx_group should be closed immediately
	 * @todo Remove when m0_be_tx_close_sync() is removed
	 */
	bool                   t_fast;
	/**
	 * Flag indicates that this transaction was opened with
	 * m0_be_tx_exclusive_open().
	 */
	bool                   t_exclusive;
	/**
	 * @see m0_be_tx_gc_enable().
	 */
	bool                   t_gc_enabled;
	void                 (*t_gc_free)(struct m0_be_tx *tx, void *param);
	void                  *t_gc_param;
	/** @see m0_be_engine::eng_tx_first_capture */
	struct m0_tlink        t_first_capture_linkage;
	/**
	 * Minimal generation index for all captured regions.
	 *
	 * This field is set and managed by engine.
	 */
	bool                   t_recovering;
	/**
	 * Set by engine when tx is grouped. Prevents the tx from grouping
	 * twice.
	 */
	bool                   t_grouped;
	/**
	 * FDMI reference counter. Used in FOL FDMI source implementation.
	 * @todo Fix this when proper refcounting is implemented.
	 */
	struct m0_atomic64     t_fdmi_ref;
	/**
	 * Locked txn list. Used in FOL FDMI source implementation.
	 *
	 * _ini and _fini is done in fdmi fol source code.
	 */
	struct m0_tlink        t_fdmi_linkage;
	/**
	 * Used by FOL source to put tx when FDMI finishes with it.
	 * @todo Will be fixed when proper refcounting is implemented
	 * in second phase of FDMI work.
	 */
	struct m0_sm_ast       t_fdmi_put_ast;
};

/**
 * Transaction identifier for remote nodes.
 */
struct m0_be_tx_remid {
	uint64_t tri_txid;
	uint64_t tri_locality;
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

#if M0_DEBUG_BE_CREDITS == 1

/**
 * Increase credits counter for the specified @cr_user.
 * The correspondent number of credit decrements should be called.
 */
#define M0_BE_CREDIT_INC(n, cr_user, credit) ({                          \
	typeof(cr_user) cu = (cr_user);                                  \
	typeof(credit)  cr = (credit);                                   \
	cr->tc_balance[cu] += (n);                                       \
	M0_LOG(M0_DEBUG, "INC cr=%p user=%d balance=%d", cr, (int)cu,    \
			 cr->tc_balance[cu]);                            \
})

/**
 * Decrement credits counter for specified @cr_user.
 * If counter exhausts - assertion will fail. Thus we will know,
 * who and where uses more credits than it was provisioned.
 */
#define M0_BE_CREDIT_DEC(cr_user, tx) ({                                 \
	struct m0_be_tx_credit *cr = &(tx)->t_prepared;                  \
	typeof(cr_user)         cu = (cr_user);                          \
	M0_LOG(M0_DEBUG, "DEC cr=%p user=%d balance=%d", cr, (int)cu,    \
			 cr->tc_balance[cu]);                            \
	M0_CNT_DEC(cr->tc_balance[cu]);                                  \
})

#else

#define M0_BE_CREDIT_INC(n, cr_user, credit)
#define M0_BE_CREDIT_DEC(cr_user, tx)

#endif

M0_INTERNAL bool m0_be_tx__invariant(const struct m0_be_tx *tx);

M0_INTERNAL void m0_be_tx_init(struct m0_be_tx     *tx,
			       uint64_t             tid,
			       struct m0_be_domain *dom,
			       struct m0_sm_group  *sm_group,
			       m0_be_tx_cb_t        persistent,
			       m0_be_tx_cb_t        discarded,
			       void               (*filler)(struct m0_be_tx *tx,
							    void *payload),
			       void                *datum);

M0_INTERNAL void m0_be_tx_fini(struct m0_be_tx *tx);

M0_INTERNAL void m0_be_tx_prep(struct m0_be_tx              *tx,
			       const struct m0_be_tx_credit *credit);

/**
 * Accumulates transaction payload size.
 *
 * This function adds size to the number of bytes
 * which is allocated for the payload area.
 *
 * @see m0_be_tx::t_payload
 */
M0_INTERNAL void m0_be_tx_payload_prep(struct m0_be_tx *tx, m0_bcount_t size);

M0_INTERNAL void m0_be_tx_open(struct m0_be_tx *tx);
M0_INTERNAL void m0_be_tx_exclusive_open(struct m0_be_tx *tx);

M0_INTERNAL void m0_be_tx_capture(struct m0_be_tx        *tx,
				  const struct m0_be_reg *reg);
M0_INTERNAL void m0_be_tx_uncapture(struct m0_be_tx        *tx,
				    const struct m0_be_reg *reg);

/* XXX change to (tx, seg, ptr) */
#define M0_BE_TX_CAPTURE_PTR(seg, tx, ptr) \
	m0_be_tx_capture((tx), &M0_BE_REG((seg), sizeof *(ptr), (ptr)))
#define M0_BE_TX_CAPTURE_ARR(seg, tx, arr, nr) \
	m0_be_tx_capture((tx), &M0_BE_REG((seg), (nr) * sizeof((arr)[0]), (arr)))
#define M0_BE_TX_CAPTURE_BUF(seg, tx, buf) \
	m0_be_tx_capture((tx), &M0_BE_REG((seg), (buf)->b_nob, (buf)->b_addr))

M0_INTERNAL void m0_be_tx_close(struct m0_be_tx *tx);

/**
 * Gets additional reference to the transaction.
 *
 * @pre !M0_IN(m0_be_tx_state(tx), (M0_BTS_FAILED, M0_BTS_DONE))
 * @see m0_be_tx_put()
 */
M0_INTERNAL void m0_be_tx_get(struct m0_be_tx *tx);
/**
 * Puts reference to the transaction.
 *
 * Transaction is not shifted to M0_BTS_DONE state until number
 * of m0_be_tx_get() calls is equal to the number of m0_be_tx_put() calls
 * for the transaction.
 *
 * @see m0_be_tx_get()
 */
M0_INTERNAL void m0_be_tx_put(struct m0_be_tx *tx);

/**
 * Forces the tx's group to close immediately by explictly calling
 * m0_be_tx_group_close(), which in turn triggers the logging of all
 * the lingering transactions in this group.
 */
M0_INTERNAL void m0_be_tx_force(struct m0_be_tx *tx);

/**
 * Waits until transacion reaches one of the given states.
 *
 * @note To wait for a M0_BTS_PLACED state, caller must guarantee that the
 * transaction are not in M0_BTS_DONE state, e.g., by calling m0_be_tx_get().
 */
M0_INTERNAL int m0_be_tx_timedwait(struct m0_be_tx *tx,
				   uint64_t         states,
				   m0_time_t        deadline);

M0_INTERNAL enum m0_be_tx_state m0_be_tx_state(const struct m0_be_tx *tx);
M0_INTERNAL const char *m0_be_tx_state_name(enum m0_be_tx_state state);

/**
 * Calls m0_be_tx_open() and then waits until transaction reaches
 * M0_BTS_ACTIVE or M0_BTS_FAILED state.
 *
 * @post equi(rc == 0, m0_be_tx_state(tx) == M0_BTS_ACTIVE)
 * @post equi(rc != 0, m0_be_tx_state(tx) == M0_BTS_FAILED)
 */
M0_INTERNAL int m0_be_tx_open_sync(struct m0_be_tx *tx);
M0_INTERNAL int m0_be_tx_exclusive_open_sync(struct m0_be_tx *tx);

/**
 * Calls m0_be_tx_close() and then waits until transaction reaches
 * M0_BTS_DONE state.
 */
M0_INTERNAL void m0_be_tx_close_sync(struct m0_be_tx *tx);

/**
 * Used by engine to check whether tx_group should be closed immediately.
 * @todo Remove when m0_be_tx_close_sync() is removed.
 */
M0_INTERNAL bool m0_be_tx__is_fast(struct m0_be_tx *tx);

/** Adds fol record @rec into the transaction @tx payload */
M0_INTERNAL int m0_be_tx_fol_add(struct m0_be_tx *tx, struct m0_fol_rec *rec);

/**
 * Forces the tx to move forward to PLACED state, which will in turn force
 * all the transactions in the same tx group to commit to disk as well.
 */
M0_INTERNAL void m0_be_tx_force (struct m0_be_tx *tx);

/**
 * true if transaction is opened exclusively. Private for BE.
 */
M0_INTERNAL bool m0_be_tx__is_exclusive(const struct m0_be_tx *tx);

/**
 * Mark tx as a tx that is recovering from log.
 *
 * Such transaction has the following properties:
 * - it is already in log, so:
 *   - log space is not reserved;
 *   - it isn't going to log;
 * - reg_area and payload buf are not allocated;
 * - it can't fail on m0_be_tx_open();
 * - it doesn't have new tx id allocated - it uses the old one from the log.
 *
 * @note This function is used by recovery only.
 */
M0_INTERNAL void m0_be_tx__recovering_set(struct m0_be_tx *tx);
M0_INTERNAL bool m0_be_tx__is_recovering(struct m0_be_tx *tx);

M0_INTERNAL void m0_be_tx_deconstruct(struct m0_be_tx     *tx,
				      struct m0_be_fmt_tx *ftx);
/*
 * @pre m0_be_tx_state(tx) == M0_BTS_PREPARE
 */
M0_INTERNAL void m0_be_tx_reconstruct(struct m0_be_tx           *tx,
				      const struct m0_be_fmt_tx *ftx);

/*
 * Assign tx to a group.
 *
 * Does nothing if m0_be_tx__is_recovering(tx).
 */
M0_INTERNAL void m0_be_tx__group_assign(struct m0_be_tx       *tx,
					struct m0_be_tx_group *gr);

M0_INTERNAL bool m0_be_tx_should_break(struct m0_be_tx *tx,
				       const struct m0_be_tx_credit *c);

/**
 * Calls @gc_free function after tx reaches M0_BTS_DONE state.
 * Signals to @gc_chan just before @gc_free call.
 * Garbage collector may be enabled at any time before m0_be_tx_close().
 * @note User should handle M0_BTS_FAILED state explicitly.
 *
 * Typical use case:
 * @code
 * struct m0_be_tx *tx;
 *
 * M0_ALLOC_PTR(tx);
 * if (tx == NULL)
 *      ...;
 * m0_be_tx_init(tx, <...>);
 * m0_be_tx_gc_enable(tx, NULL, NULL);
 * < prepare credits >
 * m0_be_tx_open(tx);
 * < wait until tx reaches M0_BTS_ACTIVE or M0_BTS_FAILED state >;
 * if (m0_be_tx_state(tx) == M0_BTS_FAILED) {
 *      m0_be_tx_fini(tx);
 *      m0_free(tx);
 * } else {
 *      < capture >;
 *      m0_be_tx_close(tx);
 * }
 * < tx pointer should be considered invalid after this point >
 * // m0_free() will be automatically called when tx reaches M0_BTS_DONE state.
 * @endcode
 *
 * @note m0_be_tx_timedwait() can't be used on transaction with gc enabled after
 * transaction reaches M0_BTS_ACTIVE state.
 * @note @gc_free can be NULL. m0_free() is called in this case to free tx.
 */
M0_INTERNAL void m0_be_tx_gc_enable(struct m0_be_tx *tx,
				    void           (*gc_free)(struct m0_be_tx *,
							      void *param),
				    void            *param);

M0_INTERNAL bool m0_be_should_break(struct m0_be_engine          *eng,
				    const struct m0_be_tx_credit *accum,
				    const struct m0_be_tx_credit *delta);

/** @} end of be group */
#endif /* __MERO_BE_TX_H__ */

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
