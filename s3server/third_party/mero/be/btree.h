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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 29-May-2013
 */
#pragma once
#ifndef __MERO_BE_BTREE_H__
#define __MERO_BE_BTREE_H__

#include "be/op.h"              /* m0_be_op */
#include "be/seg.h"
#include "be/seg_xc.h"
#include "format/format.h"      /* m0_format_header */
#include "format/format_xc.h"
#include "lib/buf.h"

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

/* export */
struct m0_be_btree;
struct m0_be_btree_kv_ops;

/* import */
struct m0_be_bnode;
struct m0_be_tx;
struct m0_be_tx_credit;

/** In-memory B-tree, that can be stored on disk. */
struct m0_be_btree {
	/*
	 * non-volatile fields
	 */
	struct m0_format_header          bb_header;
	/** Root node of the tree. */
	struct m0_be_bnode              *bb_root;
	struct m0_format_footer          bb_footer;
	/*
	 * volatile-only fields
	 */
	/** The lock to acquire when performing operations on the tree. */
	struct m0_be_rwlock              bb_lock;
	/** The segment where we are stored. */
	struct m0_be_seg                *bb_seg;
	/** operation vector, treating keys and values, given by the user */
	const struct m0_be_btree_kv_ops *bb_ops;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_btree_format_version {
	M0_BE_BTREE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_BTREE_FORMAT_VERSION */
	/*M0_BE_BTREE_FORMAT_VERSION_2,*/
	/*M0_BE_BTREE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_BTREE_FORMAT_VERSION = M0_BE_BTREE_FORMAT_VERSION_1
};

struct m0_table;
struct m0_table_ops;

/** Btree operations vector. */
struct m0_be_btree_kv_ops {
	/** Size of key.  XXX RENAMEME? s/ko_ksize/ko_key_size/ */
	m0_bcount_t (*ko_ksize)(const void *key);

	/** Size of value.  XXX RENAMEME? s/ko_vsize/ko_val_size/
	 */
	m0_bcount_t (*ko_vsize)(const void *data);

	/**
	 * Key comparison function.
	 *
	 * Should return -ve, 0 or +ve value depending on how key0 and key1
	 * compare in key ordering.
	 *
	 * XXX RENAMEME? s/ko_compare/ko_key_cmp/
	 */
	int         (*ko_compare)(const void *key0, const void *key1);
};

/**
 * Type of persistent operation over the tree.
 *
 * These values are also re-used to define transaction credit types.
 */
enum m0_be_btree_op {
	M0_BBO_CREATE,	    /**< Used for m0_be_btree_create() */
	M0_BBO_DESTROY,     /**< .. m0_be_btree_destroy() */
	M0_BBO_INSERT,      /**< .. m0_be_btree_{,inplace_}insert() */
	M0_BBO_DELETE,      /**< .. m0_be_btree_{,inplace_}delete() */
	M0_BBO_UPDATE,      /**< .. m0_be_btree_{,inplace_}update() */
	M0_BBO_LOOKUP,      /**< .. m0_be_btree_lookup() */
	M0_BBO_MAXKEY,      /**< .. m0_be_btree_maxkey() */
	M0_BBO_MINKEY,      /**< .. m0_be_btree_minkey() */
	M0_BBO_CURSOR_GET,  /**< .. m0_be_btree_cursor_get() */
	M0_BBO_CURSOR_NEXT, /**< .. m0_be_btree_cursor_next() */
	M0_BBO_CURSOR_PREV, /**< .. m0_be_btree_cursor_prev() */
};

/* ------------------------------------------------------------------
 * Btree construction
 * ------------------------------------------------------------------ */

/**
 * Initalises internal structures of the @tree (e.g., mutexes, @ops),
 * located in virtual memory of the program and not in mmaped() segment
 * memory.
 */
M0_INTERNAL void m0_be_btree_init(struct m0_be_btree *tree,
				  struct m0_be_seg *seg,
				  const struct m0_be_btree_kv_ops *ops);

/**
 * Finalises in-memory structures of btree.
 *
 * Does not touch segment on disk.
 * @see m0_be_btree_destroy(), which does remove tree structure from the
 * segment.
 */
M0_INTERNAL void m0_be_btree_fini(struct m0_be_btree *tree);

/**
 * Creates btree on segment.
 *
 * The operation is asynchronous. Use m0_be_op_wait() or
 * m0_be_op_tick_ret() to wait for its completion.
 *
 * Example:
 * @code
 *         m0_be_btree_init(&tree, seg, kv_ops);
 *         m0_be_btree_create(&tree, tx, op);
 *         m0_be_op_wait(op);
 *         if (op->bo_u.u_btree.t_rc == 0) {
 *                 ... // work with newly created tree
 *         }
 * @endcode
 */
M0_INTERNAL void m0_be_btree_create(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op);

/** Deletes btree from segment, asynchronously. */
M0_INTERNAL void m0_be_btree_destroy(struct m0_be_btree *tree,
				     struct m0_be_tx *tx,
				     struct m0_be_op *op);

/**
 * Truncate btree: delete all records, keep empty root.
 *
 * That routine may be called more than once to fit into transaction.
 * Btree between calls is not in usable state.
 * Typically new transaction must be started for each call.
 * It is ok to continue tree truncate after system restart.
 * 'Limit' is a maximum number of records to be deleted.
 */
M0_INTERNAL void m0_be_btree_truncate(struct m0_be_btree *tree,
				      struct m0_be_tx    *tx,
				      struct m0_be_op    *op,
				      m0_bcount_t         limit);


/* ------------------------------------------------------------------
 * Btree credits
 * ------------------------------------------------------------------ */

/**
 * Calculates the credit needed to create @nr nodes and adds this credit to
 * @accum.
 */
M0_INTERNAL void m0_be_btree_create_credit(const struct m0_be_btree *tree,
					   m0_bcount_t nr,
					   struct m0_be_tx_credit *accum);

/**
 * Calculates the credit needed to destroy @nr nodes and adds this credit
 * to @accum.
 */
M0_INTERNAL void m0_be_btree_destroy_credit(struct m0_be_btree *tree,
					    struct m0_be_tx_credit *accum);


/**
 * Calculates the credit needed to destroy index tree.
 *
 * Separate credits by components to handle big btree clear not fitting into
 * single transaction. The total number of credits to destroy index tree is
 * fixed_part + single_record * records_nr.
 *
 * @param tree btree to proceed
 * @param fixed_part fixed credits part which definitely must be reserved
 * @param single_record credits to delete single index record
 * @param records_nr number of records in that index
 */
M0_INTERNAL void m0_be_btree_clear_credit(struct m0_be_btree     *tree,
					  struct m0_be_tx_credit *fixed_part,
					  struct m0_be_tx_credit *single_record,
					  m0_bcount_t            *records_nr);

/**
 * Calculates how many internal resources of tx_engine, described by
 * m0_be_tx_credit, is needed to perform the insert operation over the @tree.
 * Function updates @accum structure which is an input for m0_be_tx_prep().
 *
 * @param nr     Number of @optype operations.
 * @param ksize  Key data size.
 * @param vsize  Value data size.
 */
M0_INTERNAL void m0_be_btree_insert_credit(const struct m0_be_btree *tree,
					   m0_bcount_t nr,
					   m0_bcount_t ksize,
					   m0_bcount_t vsize,
					   struct m0_be_tx_credit *accum);

/**
 * The same as m0_be_btree_insert_credit() but uses the current btree height
 * for credit calculation making it more accurate. It should be used with
 * caution since it may hide the problems with credits until btree gets
 * filled up. For example, it may be possible that the same operation
 * which successfully works on less filled btree won't work when btree
 * is more filled up because the number of required credits exceed the
 * maximum size of possible credits in the transaction.
 */
M0_INTERNAL void m0_be_btree_insert_credit2(const struct m0_be_btree *tree,
					    m0_bcount_t nr,
					    m0_bcount_t ksize,
					    m0_bcount_t vsize,
					    struct m0_be_tx_credit *accum);

/**
 * Calculates how many internal resources of tx_engine, described by
 * m0_be_tx_credit, is needed to perform the delete operation over the @tree.
 * Function updates @accum structure which is an input for m0_be_tx_prep().
 *
 * @param nr     Number of @optype operations.
 * @param ksize  Key data size.
 * @param vsize  Value data size.
 */
M0_INTERNAL void m0_be_btree_delete_credit(const struct m0_be_btree *tree,
						 m0_bcount_t nr,
						 m0_bcount_t ksize,
						 m0_bcount_t vsize,
						 struct m0_be_tx_credit *accum);

/**
 * Calculates how many internal resources of tx_engine, described by
 * m0_be_tx_credit, is needed to perform the update operation over the @tree.
 * Function updates @accum structure which is an input for m0_be_tx_prep().
 * Should be used for data which has fixed length when existing value area
 * is re-used for a new value and no alloc/free operations are needed.
 *
 * @param nr     Number of @optype operations.
 * @param vsize  Value data size.
 */
M0_INTERNAL void m0_be_btree_update_credit(const struct m0_be_btree *tree,
						 m0_bcount_t nr,
						 m0_bcount_t vsize,
						 struct m0_be_tx_credit *accum);

/**
 * The same as m0_be_btree_update_credit() but should be used for data which has
 * variable length and alloc/free operations may be needed.
 *
 * @param nr     Number of @optype operations.
 * @param ksize  Key data size.
 * @param vsize  Value data size.
 */
M0_INTERNAL void m0_be_btree_update_credit2(const struct m0_be_btree *tree,
					    m0_bcount_t               nr,
					    m0_bcount_t               ksize,
					    m0_bcount_t               vsize,
					    struct m0_be_tx_credit   *accum);


/* ------------------------------------------------------------------
 * Btree manipulation
 * ------------------------------------------------------------------ */

/**
 * Inserts @key and @value into btree. Operation is asynchronous.
 *
 * Note0: interface is asynchronous and relies on op::bo_sm.
 * Operation is considered to be finished after op::bo_sm transits to
 * M0_BOS_DONE - after that point other operations will see the effect
 * of this one.
 */
M0_INTERNAL void m0_be_btree_insert(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    const struct m0_buf *value);

/**
 * This function:
 * - Inserts given @key and @value in btree if @key does not exist.
 * - Updates given @value in btree if @key exists and overwrite flag is
 *   set to true. NOTE: caller must always consider delete credits if it
 *   sets overwrite flag to true.
 * Operation is asynchronous.
 *
 * It's a shortcut for m0_be_btree_lookup() with successive m0_be_btree_insert()
 * if key is not found or m0_be_btree_update() if key exists and overwrite flag
 * is set. This function looks up the key only once compared to double lookup
 * made by m0_btree_lookup() + m0_be_btree_insert()/update().
 *
 * Credits for this operation should be calculated by
 * m0_be_btree_insert_credit() or m0_be_btree_insert_credit2(), because in the
 * worst case insertion is required.
 *
 * @see m0_be_btree_insert()
 */
M0_INTERNAL void m0_be_btree_save(struct m0_be_btree  *tree,
				  struct m0_be_tx     *tx,
				  struct m0_be_op     *op,
				  const struct m0_buf *key,
				  const struct m0_buf *val,
				  bool                 overwrite);

/**
 * Updates the @value at the @key in btree. Operation is asynchronous.
 *
 * -ENOENT is set to @op->bo_u.u_btree.t_rc if not found.
 *
 * @see m0_be_btree_insert()
 */
M0_INTERNAL void m0_be_btree_update(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    const struct m0_buf *value);

/**
 * Deletes the entry by the given @key from btree. Operation is asynchronous.
 *
 * -ENOENT is set to @op->bo_u.u_btree.t_rc if not found.
 *
 * @see m0_be_btree_insert()
 */
M0_INTERNAL void m0_be_btree_delete(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key);

/**
 * Looks up for a @dest_value by the given @key in btree.
 * The result is copied into provided @dest_value buffer.
 *
 * -ENOENT is set to @op->bo_u.u_btree.t_rc if not found.
 *
 * @see m0_be_btree_create() regarding @op structure "mission".
 */
M0_INTERNAL void m0_be_btree_lookup(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    struct m0_buf *dest_value);

/**
 * Looks up for a maximum key value in the given @tree.
 *
 * @see m0_be_btree_create() regarding @op structure "mission".
 */
M0_INTERNAL void m0_be_btree_maxkey(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    struct m0_buf *out);

/**
 * Looks up for a minimum key value in the given @tree.
 *
 * @see m0_be_btree_create() regarding @op structure "mission".
 */
M0_INTERNAL void m0_be_btree_minkey(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    struct m0_buf *out);

/* ------------------------------------------------------------------
 * Btree in-place manipulation
 * ------------------------------------------------------------------ */

/**
 * Btree anchor, used to perform btree inplace operations in which
 * values are not being copied and the ->bb_lock is not released
 * until m0_be_btree_release() is called.
 *
 * In cases, when data in m0_be_btree_anchor::ba_value is updated,
 * m0_be_btree_release() will capture the region data lies in.
 */
struct m0_be_btree_anchor {
	struct m0_be_btree *ba_tree;
	 /**
	  * A value, accessed through m0_be_btree_lookup_inplace(),
	  * m0_be_btree_insert_inplace(), m0_be_btree_update_inplace()
	  */
	struct m0_buf       ba_value;

	/** Is write lock being held? */
	bool                ba_write;
};

/**
 * Returns the @tree record for update at the given @key.
 * User provides the size of the value buffer that will be updated
 * via @anchor->ba_value.b_nob and gets the record address
 * via @anchor->ba_value.b_addr.
 * Note: the updated record size can not exceed the stored record size
 * at the moment.
 *
 * -ENOENT is set to @op->bo_u.u_btree.t_rc if not found.
 *
 * @see m0_be_btree_insert, note0 - note2.
 *
 * Usage:
 * @code
 *         anchor->ba_value.b_nob = new_value_size;
 *         m0_be_btree_update_inplace(tree, tx, op, key, anchor);
 *
 *         m0_be_op_wait(op);
 *
 *         update(anchor->ba_value.b_addr);
 *         m0_be_btree_release(anchor);
 *         ...
 *         m0_be_tx_close(tx);
 * @endcode
 */
M0_INTERNAL void m0_be_btree_update_inplace(struct m0_be_btree *tree,
					    struct m0_be_tx *tx,
					    struct m0_be_op *op,
					    const struct m0_buf *key,
					    struct m0_be_btree_anchor *anchor);

/**
 * Inserts given @key into @tree and returns the value
 * placeholder at @anchor->ba_value. Note: this routine
 * locks the @tree until m0_be_btree_release() is called.
 *
 * @see m0_be_btree_update_inplace()
 */
M0_INTERNAL void m0_be_btree_insert_inplace(struct m0_be_btree *tree,
					    struct m0_be_tx *tx,
					    struct m0_be_op *op,
					    const struct m0_buf *key,
					    struct m0_be_btree_anchor *anchor,
					    uint64_t zonemask);

/**
 * This function:
 * - Inserts given @key and @value in btree if @key does not exist.
 * - Updates given @value in btree if @key exists and overwrite flag is
 *   set to true.
 * User has to allocate his own @value buffer and capture node buffer
 * in which @key is inserted.
 *
 * @see m0_be_btree_update_inplace()
 */
M0_INTERNAL void m0_be_btree_save_inplace(struct m0_be_btree        *tree,
					  struct m0_be_tx           *tx,
					  struct m0_be_op           *op,
					  const struct m0_buf       *key,
					  struct m0_be_btree_anchor *anchor,
					  bool                       overwrite,
					  uint64_t                   zonemask);

/**
 * Looks up a value stored in the @tree by the given @key.
 *
 * -ENOENT is set to @op->bo_u.u_btree.t_rc if not found.
 *
 * @see m0_be_btree_update_inplace()
 */
M0_INTERNAL void m0_be_btree_lookup_inplace(struct m0_be_btree *tree,
					    struct m0_be_op *op,
					    const struct m0_buf *key,
					    struct m0_be_btree_anchor *anchor);

/**
 * Completes m0_be_btree_*_inplace() operation by capturing all affected
 * regions with m0_be_tx_capture() (if needed) and unlocking the tree.
 */
M0_INTERNAL void m0_be_btree_release(struct m0_be_tx           *tx,
				     struct m0_be_btree_anchor *anchor);

/* ------------------------------------------------------------------
 * Btree cursor
 * ------------------------------------------------------------------ */

/**
 * Btree cursor stack entry.
 *
 * Used for cursor depth-first in-order traversing.
 */
struct m0_be_btree_cursor_stack_entry {
	struct m0_be_bnode *bs_node;
	int                 bs_idx;
};

/** Btree configuration constants. */
enum {
	BTREE_FAN_OUT    = 128,
	BTREE_HEIGHT_MAX = 5
};

/**
 * Btree cursor.
 *
 * Read-only cursor can be positioned with m0_be_btree_cursor_get() and moved
 * with m0_be_btree_cursor_next(), m0_be_btree_cursor_prev().
 */
struct m0_be_btree_cursor {
	struct m0_be_bnode                   *bc_node;
	int                                   bc_pos;
	struct m0_be_btree_cursor_stack_entry bc_stack[BTREE_HEIGHT_MAX];
	int                                   bc_stack_pos;
	struct m0_be_btree                   *bc_tree;
	struct m0_be_op                       bc_op; /* XXX DELETEME */
};

/**
 * Initialises cursor and its internal structures.
 *
 * Note: interface is synchronous.
 */
M0_INTERNAL void m0_be_btree_cursor_init(struct m0_be_btree_cursor *it,
					 struct m0_be_btree *tree);

/**
 * Finalizes cursor.
 *
 * Note: interface is synchronous.
 */
M0_INTERNAL void m0_be_btree_cursor_fini(struct m0_be_btree_cursor *it);

/**
 * Fills cursor internal buffers with current key and value obtained from the
 * tree. Operation may cause IO depending on cursor::bc_op state
 *
 * Note: interface is asynchronous and relies on cursor::bc_op::bo_sm. When it
 * transits into M0_BOS_DONE, the operation is believed to be finished.
 *
 * Note: allowed sequence of cursor calls is:
 * m0_be_btree_cursor_init()
 * ( m0_be_btree_cursor_get()
 *   ( m0_be_btree_cursor_next()
 *   | m0_be_btree_cursor_prev()
 *   | m0_be_btree_cursor_get()
 *   | m0_be_btree_cursor_kv_get() )*
 *   m0_be_btree_cursor_put() )*
 * m0_be_btree_cursor_fini()
 *
 * @param slant[in] if slant == true then cursor will return a minimum key not
 *  less than given, otherwise it'll be set on exact key if it's possible.
 */
M0_INTERNAL void m0_be_btree_cursor_get(struct m0_be_btree_cursor *it,
					const struct m0_buf *key, bool slant);

/** Synchronous version of m0_be_btree_cursor_get(). */
M0_INTERNAL int m0_be_btree_cursor_get_sync(struct m0_be_btree_cursor *it,
					    const struct m0_buf *key,
					    bool slant);

/**
 * Fills cursor internal buffers with key and value obtained from the
 * next position in tree. The operation is unprotected from concurrent btree
 * updates and user should protect it with external lock.
 * Operation may cause IO depending on cursor::bc_op state.
 *
 * Note: @see m0_be_btree_cursor_get note.
 */
M0_INTERNAL void m0_be_btree_cursor_next(struct m0_be_btree_cursor *it);

/** Synchronous version of m0_be_btree_cursor_next(). */
M0_INTERNAL int m0_be_btree_cursor_next_sync(struct m0_be_btree_cursor *it);

/**
 * Fills cursor internal buffers with prev key and value obtained from the
 * tree. Operation may cause IO depending on cursor::bc_op state
 *
 * Note: @see m0_be_btree_cursor_get note.
 */
M0_INTERNAL void m0_be_btree_cursor_prev(struct m0_be_btree_cursor *it);

/** Synchronous version of m0_be_btree_cursor_prev(). */
M0_INTERNAL int m0_be_btree_cursor_prev_sync(struct m0_be_btree_cursor *it);

/**
 * Moves cursor to the first key in the btree.
 *
 * @note The call is synchronous.
 */
M0_INTERNAL int m0_be_btree_cursor_first_sync(struct m0_be_btree_cursor *it);

/**
 * Moves cursor to the last key in the btree.
 *
 * @note The call is synchronous.
 */
M0_INTERNAL int m0_be_btree_cursor_last_sync(struct m0_be_btree_cursor *it);

/**
 * Unpins pages associated with cursor, releases cursor values.
 *
 * Note: interface is synchronous.
 */
M0_INTERNAL void m0_be_btree_cursor_put(struct m0_be_btree_cursor *it);

/**
 * Sets key and value buffers to point on internal structures of cursor
 * representing current key and value, cursor is placed on.
 *
 * Any of @key and @val pointers can be NULL, but not both.
 *
 * Note: interface is synchronous.
 */
M0_INTERNAL void m0_be_btree_cursor_kv_get(struct m0_be_btree_cursor *it,
					   struct m0_buf *key,
					   struct m0_buf *val);

/**
 * @pre  tree->bb_root != NULL
 */
M0_INTERNAL bool m0_be_btree_is_empty(struct m0_be_btree *tree);

/** @} end of be group */
#endif /* __MERO_BE_BTREE_H__ */

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
