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
 * Original creation date: 08/13/2010
 */

#pragma once

#ifndef __MERO_BE_EXTMAP_H__
#define __MERO_BE_EXTMAP_H__

/**
   @defgroup extmap Extent map abstraction

   Extent map is a persistent transactional collection of extents in an abstract
   numerical name-space with a numerical value associated to each extent.

   The name-space is a set of all 64-bit unsigned numbers from 0 to
   M0_BINDEX_MAX. An extent of the name-space consisting of numbers from A
   (inclusive) to B (exclusive) is denoted [A, B).

   A segment is an extent together with a 64-bit value associated with it,
   denoted as ([A, B), V).

   An extent map is a collection of segments whose extents are non-empty and
   form a partition of the name-space.

   That is, an extent map is something like

   @f[
           ([0, e_0), v_0), ([e_0, e_1), v_1), \ldots,
	   ([e_n, M0\_BINDEX\_MAX + 1), v_n)
   @f]

   Note that extents cover the whole name-space from 0 to M0_BINDEX_MAX without
   holes.

   Possible applications of extent map include:

     - allocation data for a data object. In this case an extent in the
       name-space is interpreted as an extent in the logical offset space of
       data object. A value associated with the extent is a starting block of a
       physical extent allocated to the logical extent. In addition to allocated
       extents, a map might contain "holes" and "not-allocated" extents, tagged
       with special otherwise impossible values;

     - various resource identifier distribution maps: file identifiers,
       container identifiers, layout identifiers, recording state of resource
       name-spaces: allocated to a certain node, free, etc.

   Extent map interface is based on a notion of map cursor (m0_be_emap_cursor):
   an object recording a position within a map (i.e., a segment reached by the
   iteration).

   A cursor can be positioned at the segment including a given point in the
   name-space (m0_be_emap_lookup()) and moved through the segments
   (m0_be_emap_next() and m0_be_emap_prev()).

   An extent map can be modified by the following functions:

     - m0_be_emap_split(): split a segment into a collection of segments with
       given lengths and values, provided that their total length is the same
       as the length of the original segment;

     - m0_be_emap_merge(): merge part of a segment into the next segment. The
       current segment is shrunk (or deleted if it would become empty) and the
       next segment is expanded downward;

     - m0_be_emap_paste() handles more complicated cases.

   It's easy to see that these operations preserve extent map invariant that
   extents are non-empty and form the name-space partition.

   @note The asymmetry between split and merge interfaces (i.e., the fact that a
   segment can be split into multiple segments at once, but only two segments
   can be merged) is because a user usually wants to inspect a segment before
   merging it with another one. For example, data object truncate goes through
   the allocation data segments downward until the target offset of
   reached. Each segment is analyzed, data-blocks are freed is necessary and the
   segment is merged with the next one.

   @note Currently the length and ordering of prefix and value is fixed by the
   implementation. Should the need arise, prefixes and values of arbitrary size
   and ordering could be easily implemented at the expense of dynamic memory
   allocation during cursor initialization. Prefix comparison function could be
   supplied as m0_be_emap constructor parameter.

   @{
 */

#include "lib/ext.h"       /* m0_ext */
#include "lib/ext_xc.h"	   /* m0_ext_xc */
#include "lib/types.h"     /* struct m0_uint128 */
#include "lib/types_xc.h"  /* m0_uint128_xc */
#include "be/tx.h"
#include "be/btree.h"
#include "be/btree_xc.h"

#include "be/extmap_internal.h"
#include "be/extmap_internal_xc.h"

/* import */
struct m0_be_emap;
struct m0_be_seg;
struct m0_be_tx;
struct m0_indexvec;

/* export */
struct m0_be_emap_seg;
struct m0_be_emap_cursor;

/**
    Initialize maps collection.

    @param db - data-base environment used for persistency and transactional
    support.

    @retval -ENOENT mapname is not found in the segment dictionary.
 */
M0_INTERNAL void m0_be_emap_init(struct m0_be_emap *map,
				 struct m0_be_seg  *db);

/** Release the resources associated with the collection. */
M0_INTERNAL void m0_be_emap_fini(struct m0_be_emap *map);

/**
    Create maps collection.

    m0_be_emap_init() should be called beforehand.

    @param db - data-base environment used for persistency and transactional
    support.
    @note m0_be_emap_init() should be called before this routine.
 */
M0_INTERNAL void m0_be_emap_create(struct m0_be_emap *map,
				   struct m0_be_tx   *tx,
				   struct m0_be_op   *op);

/** Destroy maps collection. */
M0_INTERNAL void m0_be_emap_destroy(struct m0_be_emap *map,
				    struct m0_be_tx   *tx,
				    struct m0_be_op   *op);

/**
   Insert a new map with the given prefix into the collection.

   Initially new map consists of a single extent:

   @f[
	   ([0, M0\_BINDEX\_MAX + 1), val)
   @f]
 */
M0_INTERNAL void m0_be_emap_obj_insert(struct m0_be_emap *map,
				       struct m0_be_tx   *tx,
				       struct m0_be_op   *op,
			         const struct m0_uint128 *prefix,
				       uint64_t           val);

/**
   Remove a map with the given prefix from the collection.

   @pre the map must be in initial state: consists of a single extent, covering
   the whole name-space.
 */
M0_INTERNAL void m0_be_emap_obj_delete(struct m0_be_emap *map,
				       struct m0_be_tx   *tx,
				       struct m0_be_op   *op,
				 const struct m0_uint128 *prefix);

/** Extent map segment. */
struct m0_be_emap_seg {
	/** Map prefix, identifying the map in its collection. */
	struct m0_uint128 ee_pre;
	/** Name-space extent. */
	struct m0_ext     ee_ext;
	/** Value associated with the extent. */
	uint64_t          ee_val;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
   Cursor iterating through the extent map.
 */
struct m0_be_emap_cursor {
	/** Map this cursor is iterating through. */
	struct m0_be_emap        *ec_map;
	/** Segment currently reached. */
	struct m0_be_emap_seg     ec_seg;
	/** Emap current version. */
	uint64_t                  ec_version;
	/** Data-base cursor. */
	struct m0_be_btree_cursor ec_cursor;
	struct m0_be_emap_key     ec_key;
	struct m0_be_emap_rec     ec_rec;
	struct m0_buf             ec_keybuf;
	struct m0_buf             ec_recbuf;
	struct m0_uint128         ec_prefix;
	struct m0_be_op           ec_op;
};

/** True iff the extent is the last one in a map. */
M0_INTERNAL bool m0_be_emap_ext_is_last(const struct m0_ext *ext);

/** True iff the extent is the first one in a map. */
M0_INTERNAL bool m0_be_emap_ext_is_first(const struct m0_ext *ext);

/** Returns an extent at the current cursor position. */
M0_INTERNAL struct m0_be_emap_seg *
	m0_be_emap_seg_get(struct m0_be_emap_cursor *it);

/** Returns the back-end operation of emap cursor */
M0_INTERNAL struct m0_be_op *m0_be_emap_op(struct m0_be_emap_cursor *it);
M0_INTERNAL int m0_be_emap_op_rc(const struct m0_be_emap_cursor *it);

/**
   Initialises extent map cursor to point to the segment containing given
   offset in a map with a given prefix in a given collection.

   All operations done through this cursor are executed in the context of given
   transaction.

   Asynchronous operation, get status via m0_be_emap_op(it)->bo_sm.sm_rc.

   @pre offset <= M0_BINDEX_MAX

   @retval -ESRCH no matching segment is found. The cursor is non-functional,
   but m0_be_emap_seg_get() contains information about the first segment of the
   next map (in prefix lexicographical order);

   @retval -ENOENT no matching segment is found and there is no map following
   requested one.
 */
M0_INTERNAL void m0_be_emap_lookup(struct m0_be_emap        *map,
			     const struct m0_uint128        *prefix,
				   m0_bindex_t               offset,
				   struct m0_be_emap_cursor *it);

/**
   Move cursor to the next segment in its map.

   Asynchronous operation, get status via m0_be_emap_op(it)->bo_sm.sm_rc.

   @pre !m0_be_emap_ext_is_last(m0_be_emap_seg_get(it))
 */
M0_INTERNAL void m0_be_emap_next(struct m0_be_emap_cursor *it);

/**
   Move cursor to the previous segment in its map.

   Asynchronous operation, get status via m0_be_emap_op(it)->bo_sm.sm_rc.

   @pre !m0_be_emap_ext_is_first(m0_be_emap_seg_get(it))
 */
M0_INTERNAL void m0_be_emap_prev(struct m0_be_emap_cursor *it);

/**
   Merge a part of the segment the cursor is currently positioned at with the
   next segment in the map.

   Current segment's extent is shrunk by delta. If this would make it empty, the
   current segment is deleted. The next segment is expanded by delta downwards.

   Asynchronous operation, get status via m0_be_emap_op(it)->bo_sm.sm_rc.

   @pre !m0_be_emap_ext_is_last(m0_be_emap_seg_get(it))
   @pre delta <= m0_ext_length(m0_be_emap_seg_get(it));
 */
M0_INTERNAL void m0_be_emap_merge(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  m0_bindex_t               delta);

/**
   Split the segment the cursor is current positioned at into a collection of
   segments given by the vector.

   Iterator is moved to the next segment after original one automatically.

   @param vec - a vector describing the collection of
   segments. vec->ov_vec.v_count[] array contains lengths of the extents and
   vec->ov_index[] array contains values associated with the corresponding
   extents.

   Empty segments from vec are skipped.  On successful completion, the cursor is
   positioned on the last created segment.

   Asynchronous operation, get status via m0_be_emap_op(it)->bo_sm.sm_rc.

   @pre m0_vec_count(&vec->ov_vec) == m0_ext_length(m0_be_emap_seg_get(it))
 */
M0_INTERNAL void m0_be_emap_split(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  struct m0_indexvec       *vec);

/**
   Paste segment (ext, val) into the map, deleting or truncating overlapping
   segments as necessary.

   Asynchronous operation, get status via m0_be_emap_op(it)->bo_sm.sm_rc.

   @param del - this call-back is called when an existing segment is completely
   covered by a new one and has to be deleted. The segment to be deleted is
   supplied as the call-back argument;

   @param cut_left - this call-back is called when an existing segment has to be
   cut to give place to a new one and some non-empty left part of the existing
   segment remains in the map. m0_ext call-back argument is the extent being cut
   from the existing segment. The last argument is the value associated with the
   existing segment. The call-back must set seg->ee_val to the new value
   associated with the remaining left part of the segment;

   @param cut_right - similar to cut_left, this call-back is called when some
   non-empty right part of an existing segment survives the paste operation.

   @note It is possible that both left and right cut call-backs are called
   against the same segment (in the case where new segment fits completely into
   existing one).

   @note Map invariant is temporarily violated during paste operation. No calls
   against the map should be made from the call-backs or, more generally, from
   the same transaction, while paste is running.

   @note Call-backs are called in the order of cursor iteration, but this is not
   a part of official function contract.
 */
M0_INTERNAL void m0_be_emap_paste(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  struct m0_ext            *ext,
				  uint64_t                  val,
	void (*del)(struct m0_be_emap_seg*),
	void (*cut_left)(struct m0_be_emap_seg*, struct m0_ext*, uint64_t),
	void (*cut_right)(struct m0_be_emap_seg*, struct m0_ext*, uint64_t));

/** Returns number of segments in the map. */
M0_INTERNAL int m0_be_emap_count(struct m0_be_emap_cursor *it,
				 m0_bcount_t *segs);

/**
   Updates the segment at the current cursor with the given
   segment having same prefix.

   @pre m0_uint128_eq(&it->ec_seg.ee_pre, &es->ee_pre) == true
 */
M0_INTERNAL void m0_be_emap_extent_update(struct m0_be_emap_cursor *it,
					  struct m0_be_tx          *tx,
				    const struct m0_be_emap_seg    *es);

/**
   Release the resources associated with the cursor.
 */
M0_INTERNAL void m0_be_emap_close(struct m0_be_emap_cursor *it);

/**
    Extent map caret.

    A caret is an iterator with finer granularity than a cursor. A caret is a
    cursor plus an offset within the segment the cursor is currently over.

    Caret interface is intentionally similar to m0_vec_cursor interface, which
    see for further references.

    Caret implementation is simplified by segment non-emptiness (as guaranteed
    by extent map invariant).
 */
struct m0_be_emap_caret {
	struct m0_be_emap_cursor *ct_it;
	m0_bindex_t            ct_index;
};

M0_INTERNAL void m0_be_emap_caret_init(struct m0_be_emap_caret  *car,
				       struct m0_be_emap_cursor *it,
				       m0_bindex_t               index);

M0_INTERNAL void m0_be_emap_caret_fini(struct m0_be_emap_caret *car);

/**
   Move the caret.

   Asynchronous operation, get status via
   m0_be_emap_op(car->ct_it)->bo_sm.sm_rc.
 */
M0_INTERNAL int m0_be_emap_caret_move(struct m0_be_emap_caret *car,
				      m0_bcount_t              count);

/** Synchronous equivalent of m0_be_emap_caret_move(). */
M0_INTERNAL int m0_be_emap_caret_move_sync(struct m0_be_emap_caret *car,
				           m0_bcount_t              count);

/** Returns how far is the end of extent. */
M0_INTERNAL m0_bcount_t m0_be_emap_caret_step(const struct m0_be_emap_caret*);

/**
 * Possible persistent operations over the tree.
 * Enumeration items are being reused to define transaction credit types also.
 */
enum m0_be_emap_optype {
	M0_BEO_CREATE,	/**< m0_be_emap_create() */
	M0_BEO_DESTROY,	/**< m0_be_emap_destroy() */
	M0_BEO_INSERT,	/**< m0_be_emap_obj_insert() */
	M0_BEO_DELETE,	/**< m0_be_emap_obj_delete() */
	M0_BEO_UPDATE,	/**< m0_be_emap_extent_update() */
	M0_BEO_MERGE,	/**< m0_be_emap_merge() */
	M0_BEO_SPLIT,	/**< m0_be_emap_split() */
	M0_BEO_PASTE,	/**< m0_be_emap_paste() */
};

/**
 * Calculates how many internal resources of tx_engine, described by
 * m0_be_tx_credit, is needed to perform an operation over the @emap.
 * Function updates @accum structure which is an input for m0_be_tx_prep().
 *
 * @param optype operation type over the @emap.
 * @param nr     number of @optype operations.
 *
 * @note in case of M0_BEO_SPLIT @nr is the number of split parts.
 */
M0_INTERNAL void m0_be_emap_credit(struct m0_be_emap      *emap,
				   enum m0_be_emap_optype  optype,
				   m0_bcount_t             nr,
				   struct m0_be_tx_credit *accum);

M0_INTERNAL
struct m0_be_domain *m0_be_emap_seg_domain(const struct m0_be_emap *emap);

/*
 * Dumps the number of cobs and segments from the @emap
 * into the trace logs. Note, on large amount of data
 * when BE segment does not fit into available RAM this
 * may generate a lot of I/O because of page-ins.
 */
M0_INTERNAL int m0_be_emap_dump(struct m0_be_emap *emap);

/** @} end group extmap */

/* __MERO_BE_EXTMAP_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
