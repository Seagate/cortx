/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>
 * Original creation date: 15-Aug-2016
 */

#pragma once

#ifndef __MERO_CAS_CTG_STORE_H__
#define __MERO_CAS_CTG_STORE_H__

#include "fop/fom_generic.h"
#include "fop/fom_long_lock.h"
#include "be/op.h"
#include "be/btree.h"
#include "be/btree_xc.h"
#include "be/tx_credit.h"
#include "format/format.h"
#include "cas/cas.h"

/**
 * @defgroup cas-ctg-store
 *
 * @{
 *
 * CAS catalogue store provides an interface to operate with catalogues.
 * It's a thin layer that hides B-tree implementation details. Most of
 * operations are asynchronous. For usability CAS catalogue store provides
 * separate interfaces for operations over meta, catalogue-index and
 * catalogues created by user.
 * @note All operations over catalogue-index catalogue are synchronous.
 *
 * For now CAS catalogue store has two users: CAS service and DIX
 * repair/rebalance service.
 * @note CAS catalogue store is a singleton in scope of single process.
 * Every user of CAS catalogue store must call m0_ctg_store_init() to initialize
 * static inner structures if the user needs this store to operate. This call is
 * thread-safe. If catalogue store is already initialized then
 * m0_ctg_store_init() does nothing but incrementing inner reference counter.
 *
 * Every user of CAS catalogue store must call m0_ctg_store_fini() if it does
 * not need this store anymore. This call is thread-safe. When
 * m0_ctg_store_fini() is called the inner reference counter is atomically
 * decremented. If the last user of CAS catalogue store calls
 * m0_ctg_store_fini() then release of CAS catalogue store inner structures is
 * initiated.
 *
 * @verbatim
 *
 *     +-----------------+            +------------------+
 *     |                 |            |       DIX        |
 *     |   CAS Service   |            | repair/rebalance |
 *     |                 |            |     service      |
 *     +-----------------+            +------------------+
 *              |                              |
 *              +-----------+      +-----------+
 *                          |      |
 *                          |      |
 *                          V      V
 *                    +------------------+
 *                    |       CAS        |
 *                    |    catalogue     |
 *                    |      store       |
 *                    +------------------+
 *
 * @endverbatim
 *
 * The interface of CAS catalogue store is FOM-oriented. For operations that may
 * be done asynchronous the next phase of FOM should be provided. Corresponding
 * functions return the result that shows whether the FOM should wait or can
 * continue immediately.
 * Every user should take care about locking of CAS catalogues.
 */


/** CAS catalogue. */
struct m0_cas_ctg {
	struct m0_format_header cc_head;
	struct m0_format_footer cc_foot;
	/*
	 * m0_be_btree has it's own volatile-only fields, so it can't be placed
	 * before the m0_format_footer, where only persistent fields allowed
	 */
	struct m0_be_btree      cc_tree;
	struct m0_be_long_lock  cc_lock;
	/** Channel to announce catalogue modifications (put, delete). */
	struct m0_be_chan       cc_chan;
	/** Mutex protecting cc_chan. */
	struct m0_be_mutex      cc_chan_guard;
	/*
	 * Indicates whether catalogue is initialised after its fetch from BE
	 * segment. Non-meta catalogue is initialised on its first lookup in
	 * meta.
	 */
	bool                    cc_inited;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_cas_ctg_format_version {
	M0_CAS_CTG_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_CAS_CTG_FORMAT_VERSION */
	/*M0_CAS_CTG_FORMAT_VERSION_2,*/
	/*M0_CAS_CTG_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_CAS_CTG_FORMAT_VERSION = M0_CAS_CTG_FORMAT_VERSION_1
};

/** Catalogue store state persisted on a disk. */
struct m0_cas_state {
        struct m0_format_header cs_header;
	/**
	 * Pointer to descriptor of meta catalogue. Descriptor itself is
	 * allocated elsewhere. This pointer is there (not in m0_ctg_store)
	 * because m0_cas_state is saved in seg_dict.
	 */
        struct m0_cas_ctg      *cs_meta;
        /**
	 * Total number of records in all catalogues in catalogue store. It's
	 * used by repair/re-balance services to report progress.
	 */
        uint64_t                cs_rec_nr;
        /**
	 * Total size of all records in catalogue store. The value is
	 * incremented on every record insertion. On record deletion it's not
	 * decremented, because before record deletion the size of the value is
	 * unknown.
	 */
        uint64_t                cs_rec_size;
        struct m0_format_footer cs_footer;
	/**
	 * Mutex to protect m0_cas_ctg init after load.
	 */
	struct m0_be_mutex      cs_ctg_init_mutex;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** Structure that describes catalogue operation. */
struct m0_ctg_op {
	/** Caller FOM. */
	struct m0_fom            *co_fom;
	/** Catalogue for which the operation will be performed. */
	struct m0_cas_ctg        *co_ctg;
	/**
	 * BE operation structure for b-tree operations, except for
	 * CO_CUR. Cursor operations use m0_ctg_op::co_cur.
	 *
	 * @note m0_ctg_op::co_cur has its own m0_be_op. It could be used for
	 * all operations, but it is marked deprecated in btree.h.
	 */
	struct m0_be_op           co_beop;
	/** BTree anchor used for inplace operations. */
	struct m0_be_btree_anchor co_anchor;
	/** BTree cursor used for cursor operations. */
	struct m0_be_btree_cursor co_cur;
	/** Shows whether catalogue cursor is initialised. */
	bool                      co_cur_initialised;
	/** Current cursor phase. */
	int                       co_cur_phase;
	/** Manages calling of callback on completion of BE operation. */
	struct m0_clink           co_clink;
	/** Channel to communicate with caller FOM. */
	struct m0_chan            co_channel;
	/** Channel guard. */
	struct m0_mutex           co_channel_lock;
	/** Key buffer. */
	struct m0_buf             co_key;
	/** Value buffer. */
	struct m0_buf             co_val;
	/** Key out buffer. */
	struct m0_buf             co_out_key;
	/** Value out buffer. */
	struct m0_buf             co_out_val;
	struct m0_buf             co_mem_buf;
	/** Operation code to be executed. */
	int                       co_opcode;
	/** Operation type (meta or ordinary catalogue). */
	int                       co_ct;
	/** Operation flags. */
	uint32_t                  co_flags;
	/** Operation result code. */
	int                       co_rc;
	/**
	 * Maximum number of records (limit) allowed to be deleted in CO_TRUNC
	 * operation, see m0_ctg_truncate().
	 */
	m0_bcount_t               co_cnt;
};

#define CTG_OP_COMBINE(opc, ct) (((uint64_t)(opc)) | ((ct) << 16))

/**
 * Initialises catalogue store.
 * @note Every user of catalogue store must call this function as catalogue
 * store is a singleton and initialisation function may be called many times
 * from different threads: the first call does actual initialisation, following
 * calls does nothing but returning 0.
 *
 * Firstly the function looks up into BE segment dictionary for meta catalogue.
 * There are three cases:
 * 1. Meta catalogue is presented.
 * 2. meta catalogue is not presented.
 * 3. An error occurred.
 *
 * In the first case the function looks up into meta catalogue for
 * catalogue-index catalogue. If it is not presented or some error occurred then
 * an error returned.
 * @note Both meta catalogue and catalogue-index catalogue must be presented.
 * In the second case the function creates meta catalogue, creates
 * catalogue-index catalogue and inserts catalogue-index context into meta
 * catalogue.
 * In the third case an error returned.
 *
 * @ret 0 on success or negative error code.
 */
M0_INTERNAL int  m0_ctg_store_init(struct m0_be_domain *dom);

/**
 * Releases one reference to catalogue store context. If the last reference is
 * released then actual finalisation will be done.
 * @note Every user of catalogue store must call this function.
 *
 * @see m0_ctg_store_init()
 */
M0_INTERNAL void m0_ctg_store_fini(void);

/** Returns a pointer to meta catalogue context. */
M0_INTERNAL struct m0_cas_ctg *m0_ctg_meta(void);

/** Returns a pointer to "dead index" catalogue context. */
M0_INTERNAL struct m0_cas_ctg *m0_ctg_dead_index(void);

/** Returns a pointer to catalogue-index catalogue context. */
M0_INTERNAL struct m0_cas_ctg *m0_ctg_ctidx(void);

/**
 * Initialises catalogue operation.
 * @note Catalogue operation must be initialised before executing of any
 * operation on catalogues but getting lookup/cursor results and moving of
 * cursor.
 *
 * @param ctg_op Catalogue operation context.
 * @param fom    FOM that needs to execute catalogue operation.
 * @param flags  Catalogue operation flags.
 */
M0_INTERNAL void m0_ctg_op_init    (struct m0_ctg_op *ctg_op,
				    struct m0_fom    *fom,
				    uint32_t          flags);

/**
 * Gets result code of executed catalogue operation.
 *
 * @param ctg_op Catalogue operation context.
 *
 * @ret Result code of executed catalogue operation.
 */
M0_INTERNAL int  m0_ctg_op_rc      (struct m0_ctg_op *ctg_op);

/**
 * Finalises catalogue operation.
 *
 * @param ctg_op Catalogue operation context.
 */
M0_INTERNAL void m0_ctg_op_fini    (struct m0_ctg_op *ctg_op);

/**
 * Creates a new catalogue context and inserts it into meta catalogue.
 *
 * @param ctg_op     Catalogue operation context.
 * @param fid        FID of catalogue to be created and inserted into meta
 *                   catalogue.
 * @param next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int  m0_ctg_meta_insert(struct m0_ctg_op    *ctg_op,
				    const struct m0_fid *fid,
				    int                  next_phase);

/**
 * Forces FOM to wait until index garbage collector removes all catalogues
 * pending for deletion in "dead index" catalogue. After garbage collector is
 * done, FOM is waked up in the 'next_phase' phase.
 *
 * @param ctg_op     Catalogue operation context.
 * @param next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_ctg_gc_wait(struct m0_ctg_op *ctg_op,
			       int               next_phase);

/**
 * Inserts 'ctg' into "dead index" catalogue, therefore scheduling 'ctg' for
 * deletion by index garbage collector.
 *
 * @param ctg_op     Catalogue operation context.
 * @param ctg        Catalogue to be inserted in "dead index" catalogue.
 * @param next_phase Next phase of caller FOM.
 */
M0_INTERNAL int m0_ctg_dead_index_insert(struct m0_ctg_op  *ctg_op,
					 struct m0_cas_ctg *ctg,
					 int                next_phase);

/**
 * Looks up a catalogue in meta catalogue.
 *
 * @param ctg_op     Catalogue operation context.
 * @param fid        FID of catalogue to be looked up.
 * @param next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int  m0_ctg_meta_lookup(struct m0_ctg_op    *ctg_op,
				    const struct m0_fid *fid,
				    int                  next_phase);

/** Gets a pointer to catalogue context that was looked up. */
M0_INTERNAL
struct m0_cas_ctg *m0_ctg_meta_lookup_result(struct m0_ctg_op *ctg_op);

/**
 * Deletes a catalogue from meta catalogue.
 *
 * @param ctg_op     Catalogue operation context.
 * @param fid        FID of catalogue to be deleted.
 * @param next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_ctg_meta_delete(struct m0_ctg_op    *ctg_op,
				   const struct m0_fid *fid,
				   int                  next_phase);

/**
 * Inserts a key/value record into catalogue.
 * @note Value itself is not copied inside of this function, user should keep it
 *       until operation is complete. Key is copied before operation execution,
 *       user does not need to keep it since this function is called.
 *
 * @param ctg_op     Catalogue operation context.
 * @param ctg        Context of catalogue for record insertion.
 * @param key        Key to be inserted.
 * @param val        Value to be inserted.
 * @param next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_ctg_insert(struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      const struct m0_buf *val,
			      int                  next_phase);

/**
 * Deletes a key/value record from catalogue.
 * @note Key is copied before execution of operation, user does not need to keep
 *       it since this function is called.
 *
 * @param ctg_op     Catalogue operation context.
 * @param ctg        Context of catalogue for record deletion.
 * @param key        Key of record to be inserted.
 * @param next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_ctg_delete(struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      int                  next_phase);

/**
 * Looks up a key/value record in catalogue.
 * @note Key is copied before execution of operation, user does not need to keep
 *       it since this function is called.
 *
 * @param ctg_op     Catalogue operation context.
 * @param ctg        Context of catalogue for record looking up.
 * @param key        Key of record to be looked up.
 * @param next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_ctg_lookup(struct m0_ctg_op    *ctg_op,
			      struct m0_cas_ctg   *ctg,
			      const struct m0_buf *key,
			      int                  next_phase);

/**
 * Gets lookup result.
 * @note Returned pointer is a pointer to original value placed in tree.
 *
 * @param[in]  ctg_op Catalogue operation context.
 * @param[out] buf    Buffer for value.
 */
M0_INTERNAL void m0_ctg_lookup_result(struct m0_ctg_op *ctg_op,
				      struct m0_buf    *buf);

/**
 * Gets the minimal key in the tree (wrapper over m0_be_btree_minkey()).
 *
 * After operation complete min key value is in ctg_op->co_out_key.
 *
 * @param  ctg_op     Catalogue operation context.
 * @parami ctg        Catalogue descriptor.
 * @param  next_phase Next phase of caller FOM.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_ctg_minkey(struct m0_ctg_op  *ctg_op,
			      struct m0_cas_ctg *ctg,
			      int                next_phase);
/**
 * Truncates catalogue: destroy all records, but not the B-tree root.
 *
 * That routine may be called more than once to fit into transaction. 'Limit'
 * is a maximum number of records to be deleted. 'Limit' may be calculated
 * through m0_ctg_drop_credit() (as well as necessary credits).
 *
 * After truncation is done, m0_ctg_drop() should be called to destroy
 * associated B-tree completely.
 */
M0_INTERNAL int m0_ctg_truncate(struct m0_ctg_op  *ctg_op,
				struct m0_cas_ctg *ctg,
				m0_bcount_t        limit,
				int                next_phase);

/**
 * Destroys B-tree associated with a catalogue. B-tree should be empty.
 */
M0_INTERNAL int m0_ctg_drop(struct m0_ctg_op  *ctg_op,
			    struct m0_cas_ctg *ctg,
			    int                next_phase);

/**
 * Initialises cursor for meta catalogue.
 * @note Passed catalogue operation must be initialised using m0_ctg_op_init()
 *       before calling of this function.
 *
 * @param ctg_op Catalogue operation context.
 */
M0_INTERNAL void m0_ctg_meta_cursor_init(struct m0_ctg_op *ctg_op);

/**
 * Positions cursor on catalogue with FID @fid. Should be called after
 * m0_ctg_meta_cursor_init().
 *
 * @param ctg_op     Catalogue operation context.
 * @param fid        FID of catalogue for cursor positioning.
 * @param next_phase Next phase of caller FOM.
 *
 * @see m0_ctg_meta_cursor_init()
 */
M0_INTERNAL int  m0_ctg_meta_cursor_get(struct m0_ctg_op    *ctg_op,
					const struct m0_fid *fid,
					int                  next_phase);

/**
 * Moves meta catalogue cursor to the next record.
 *
 * @param ctg_op     Catalogue operation context.
 * @param next_phase Next phase of caller FOM.
 */
M0_INTERNAL int  m0_ctg_meta_cursor_next(struct m0_ctg_op *ctg_op,
					 int               next_phase);

/**
 * Initialises cursor for catalogue @ctg.
 * @note Passed catalogue operation must be initialised using m0_ctg_op_init()
 *       before calling of this function.
 *
 * @param ctg_op Catalogue operation context.
 * @param ctg    Catalogue context.
 */
M0_INTERNAL void m0_ctg_cursor_init(struct m0_ctg_op  *ctg_op,
				    struct m0_cas_ctg *ctg);

/**
 * Checks whether catalogue cursor is initialised.
 *
 * @param ctg_op Catalogue operation context.
 *
 * @see m0_ctg_meta_cursor_init()
 * @see m0_ctg_cursor_init()
 */
M0_INTERNAL bool m0_ctg_cursor_is_initialised(struct m0_ctg_op *ctg_op);

/**
 * Positions cursor on record with key @key. Should be called after
 * m0_ctg_cursor_init().
 *
 * @param ctg_op     Catalogue operation context.
 * @param key        Key for cursor positioning.
 * @param next_phase Next phase of caller FOM.
 *
 * @see m0_ctg_cursor_init()
 */
M0_INTERNAL int  m0_ctg_cursor_get(struct m0_ctg_op    *ctg_op,
				   const struct m0_buf *key,
				   int                  next_phase);

/**
 * Moves catalogue cursor to the next record.
 *
 * @param ctg_op     Catalogue operation context.
 * @param next_phase Next phase of caller FOM.
 */
M0_INTERNAL int  m0_ctg_cursor_next(struct m0_ctg_op *ctg_op,
				    int               next_phase);

/**
 * Gets current key/value under cursor.
 * @note Key/value data pointers are set to actual record placed in tree.
 *
 * @param[in]  ctg_op Catalogue operation context.
 * @param[out] key    Key buffer.
 * @param[out] val    Value buffer.
 */
M0_INTERNAL void m0_ctg_cursor_kv_get(struct m0_ctg_op *ctg_op,
				      struct m0_buf    *key,
				      struct m0_buf    *val);

/**
 * Releases catalogue cursor, should be called before m0_ctg_cursor_fini().
 *
 * @param ctg_op Catalogue operation context.
 *
 * @see m0_ctg_cursor_fini()
 */
M0_INTERNAL void m0_ctg_cursor_put(struct m0_ctg_op *ctg_op);

/**
 * Finalises catalogue cursor.
 *
 * @param ctg_op Catalogue operation context.
 */
M0_INTERNAL void m0_ctg_cursor_fini(struct m0_ctg_op *ctg_op);

/**
 * Calculates credits for insertion of record into catalogue.
 *
 * @param ctg   Catalogue context.
 * @param knob  Key length.
 * @param knob  Value length.
 * @param accum Accumulated credits.
 */
M0_INTERNAL void m0_ctg_insert_credit(struct m0_cas_ctg      *ctg,
				      m0_bcount_t             knob,
				      m0_bcount_t             vnob,
				      struct m0_be_tx_credit *accum);

/**
 * Calculates credits for deletion of record from catalogue.
 *
 * @param ctg   Catalogue context.
 * @param knob  Key length.
 * @param knob  Value length.
 * @param accum Accumulated credits.
 */
M0_INTERNAL void m0_ctg_delete_credit(struct m0_cas_ctg      *ctg,
				      m0_bcount_t             knob,
				      m0_bcount_t             vnob,
				      struct m0_be_tx_credit *accum);

/**
 * Calculates credits for insertion into catalogue-index catalogue.
 *
 * @param accum Accumulated credits.
 */
M0_INTERNAL void m0_ctg_ctidx_insert_credits(struct m0_cas_id       *cid,
					     struct m0_be_tx_credit *accum);

/**
 * Calculates credits for destroying catalogue records.
 *
 * If it's not possible to destroy the catalogue in one BE transaction, then
 * 'accum' contains credits that are necessary to delete 'limit' number of
 * records. 'limit' is a maximum number of records that can be deleted in one BE
 * transaction.
 */
M0_INTERNAL void m0_ctg_drop_credit(struct m0_fom          *fom,
				    struct m0_be_tx_credit *accum,
				    struct m0_cas_ctg      *ctg,
				    m0_bcount_t            *limit);

/**
 * Finalises and deallocates the catalogue.
 *
 * Should be called after m0_ctg_drop().
 */
M0_INTERNAL int m0_ctg_fini(struct m0_ctg_op  *ctg_op,
			    struct m0_cas_ctg *ctg,
			    int                next_phase);

/**
 * Calculates credits necessary to move catalogue to "dead index" catalogue.
 */
M0_INTERNAL void m0_ctg_mark_deleted_credit(struct m0_be_tx_credit *accum);

/**
 * Calculates credits necessary to allocate new catalogue and add it to the meta
 * catalogue.
 */
M0_INTERNAL void m0_ctg_create_credit(struct m0_be_tx_credit *accum);

/**
 * Calculates credits necessary to delete catalogue from "dead index" catalogue
 * and to deallocate it.
 */
M0_INTERNAL void m0_ctg_dead_clean_credit(struct m0_be_tx_credit *accum);

/**
 * Calculates credits for deletion from catalogue-index catalogue.
 *
 * @param accum Accumulated credits.
 */
M0_INTERNAL void m0_ctg_ctidx_delete_credits(struct m0_cas_id       *cid,
					     struct m0_be_tx_credit *accum);

/**
 * Synchronous record insertion into catalogue-index catalogue.
 *
 * @param cid CAS ID containing FID/layout to be inserted.
 * @param tx  BE transaction.
 *
 * @ret 0 on success or negative error code.
 */
M0_INTERNAL int m0_ctg_ctidx_insert_sync(const struct m0_cas_id *cid,
					 struct m0_be_tx        *tx);

/**
 * Synchronous record deletion from catalogue-index catalogue.
 *
 * @param cid CAS ID containing FID/layout to be deleted.
 * @param tx  BE transaction.
 *
 * @ret 0 on success or negative error code.
 */
M0_INTERNAL int m0_ctg_ctidx_delete_sync(const struct m0_cas_id *cid,
					 struct m0_be_tx        *tx);

/**
 * Synchronous record lookup in catalogue-index catalogue.
 *
 * @param[in]  fid    FID of component catalogue.
 * @param[out] layout Layout of index which component catalogue with FID @fid
 *                    belongs to.
 *
 * @ret 0 on success or negative error code.
 */
M0_INTERNAL int m0_ctg_ctidx_lookup_sync(const struct m0_fid   *fid,
					 struct m0_dix_layout **layout);

M0_INTERNAL int  m0_ctg_mem_place(struct m0_ctg_op    *ctg_op,
				  const struct m0_buf *buf,
				  int                  next_phase);
M0_INTERNAL void m0_ctg_mem_place_get(struct m0_ctg_op *ctg_op,
				      struct m0_buf    *buf);
M0_INTERNAL int  m0_ctg_mem_free(struct m0_ctg_op *ctg_op,
				 void             *area,
				 int               next_phase);

/**
 * Total number of records in catalogue store in all user (non-meta) catalogues.
 */
M0_INTERNAL uint64_t m0_ctg_rec_nr(void);

/**
 * Total size of records in catalogue store in all user (non-meta) catalogues.
 */
M0_INTERNAL uint64_t m0_ctg_rec_size(void);

/**
 * Returns a reference to the catalogue store "delete" long lock.
 *
 * This lock is used to protect DIX CM iterator current record from concurrent
 * deletion by the client. Iterator holds this lock when some record is in
 * processing and releases it when this record is fully processed, i.e. when a
 * reply from remote copy machine is received. See dix/client.h, "Operation in
 * degraded mode" for more info.
 *
 * Theoretically, catalogue lock m0_cas_ctg::cc_lock may be used for this
 * purpose, but it leads to a deadlock during repair in the following case:
 *           Node 1                                      Node 2
 *   Iterator gets the catalogue lock      Iterator gets the catalogue lock
 *   Iterator sends the record to Node 2   Iterator sends the record to Node 1
 *   Iterator waits for a reply            Iterator waits for a reply
 * Copy packet foms on both nodes have no chance to insert incoming record into
 * local catalogue, because the catalogue lock is held by the iterator.
 *
 * This lock is also used exactly the same way when client deletes the whole
 * catalogue in degraded mode.
 */
M0_INTERNAL struct m0_long_lock *m0_ctg_del_lock(void);

/**
 * Returns a reference to the catalogue long lock. This accessor hides internal
 * representation of the long lock, because it needs to be padded.
 */
M0_INTERNAL struct m0_long_lock *m0_ctg_lock(struct m0_cas_ctg *ctg);

/** @} end of cas-ctg-store group */
#endif /* __MERO_CAS_CTG_STORE_H__ */

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
