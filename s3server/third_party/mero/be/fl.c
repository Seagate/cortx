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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 14-Jan-2014
 */

#include "be/fl.h"

#include "lib/assert.h"         /* M0_PRE */
#include "lib/misc.h"           /* ARRAY_SIZE */
#include "lib/arith.h"          /* min_type */

#include "mero/magic.h"         /* M0_BE_ALLOC_FREE_LINK_MAGIC */

#include "be/tx_credit.h"       /* m0_be_tx_credit */
#include "be/alloc_internal.h"  /* be_alloc_chunk */

/**
 * @addtogroup be
 *
 * m0_be_fl maintains persistent lists of all free allocator chunks.
 *
 * There is 3 operations with free lists:
 * - m0_be_fl_add() adds chunk to free lists;
 * - m0_be_fl_del() deletes chunk from free lists;
 * - m0_be_fl_pick() picks chunk from free lists with size
 *   at least as requested.
 *
 * Algorithm.
 * - size range [0, M0_BCOUNT_MAX] is divided into subranges;
 * - for each subrange doubly-linked LRU list is maintained;
 * - pick() implementation
 *   - find what range the requested size is in, select corresponding list;
 *   - if size is in [0, M0_BE_FL_NR * M0_BE_FL_STEP) then select first chunk
 *     from the list;
 *   - otherwise scan the list to find first suitable chunk.
 *
 * Time and I/O complexity.
 * - m0_be_fl_add() and m0_be_fl_del() have O(1) time and I/O complexity;
 * - m0_be_fl_pick() has different complexity depending on size requested:
 *   - for size requested < (M0_BE_FL_NR * M0_BE_FL_STEP) time and I/O
 *     complexity is O(1);
 *   - for size requested >= (M0_BE_FL_NR * M0_BE_FL_STEP) time and I/O
 *     complexity is O(N), where N is total number of free chunks with
 *     size from this range.
 *
 * Locks
 * m0_be_fl doesn't have any locks. User has to provide concurrency protection.
 *
 * Limitations.
 * - m0_be_fl doesn't take chunks memory aligntment into account.
 * - m0_be_fl doesn't take chunks address into account.
 * - M0_BE_FL_STEP, M0_BE_FL_NR and M0_BE_FL_PICK_SCAN_LIMIT are
 *   compile-time constans.
 *
 * Known issues.
 * - m0_be_fl doesn't have true O(1) time and I/O complexity for size range
 *   [0, M0_BCOUNT_MAX].
 *
 * Future improvement directions.
 * - It is possible to get O(1) time complexity for any size. To do this
 *   m0_be_fl needs to have m0_be_fl_size for size range [2^n, 2^(n+1) - 1] for
 *   sizes >= (M0_BE_FL_NR * M0_BE_FL_STEP).
 *   the future.
 * - It is possible to use memory-only LRU cache for chunks. It can help with
 *   large amount of alocations/deallocations.
 * - The current implementation of m0_be_fl_pick() doesn't take allocation
 *   alignment requirements into account. It's possible to optimise allocations
 *   for non-default alignments (i.e. more than M0_BE_ALLOC_SHIFT_MIN) if the
 *   alignment is also passed to m0_be_fl_pick() and is checked in the loop.
 *
 * @{
 */

M0_BE_LIST_DESCR_DEFINE(fl, "m0_be_fl", static, struct be_alloc_chunk,
			bac_linkage_free, bac_magic_free,
			M0_BE_ALLOC_FREE_LINK_MAGIC, M0_BE_ALLOC_FREE_MAGIC);
M0_BE_LIST_DEFINE(fl, static, struct be_alloc_chunk);

static struct m0_be_list *be_fl_list(struct m0_be_fl *fl, unsigned long index)
{
	M0_PRE(index < ARRAY_SIZE(fl->bfl_free));

	return &fl->bfl_free[index].bfs_list;
};

static bool be_fl_list_is_empty(struct m0_be_fl *fl, unsigned long index)
{
	return fl_be_list_is_empty(be_fl_list(fl, index));
}

M0_INTERNAL void m0_be_fl_create(struct m0_be_fl  *fl,
				 struct m0_be_tx  *tx,
				 struct m0_be_seg *seg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fl->bfl_free); ++i)
		fl_be_list_create(be_fl_list(fl, i), tx);
}

M0_INTERNAL void m0_be_fl_destroy(struct m0_be_fl *fl, struct m0_be_tx *tx)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fl->bfl_free); ++i)
		fl_be_list_destroy(be_fl_list(fl, i), tx);
}

static unsigned long be_fl_index_round_up(struct m0_be_fl *fl,
                                          m0_bcount_t      size)
{
	return min_type(unsigned long, M0_BE_FL_NR,
			m0_align(size, M0_BE_FL_STEP) / M0_BE_FL_STEP);
}

static unsigned long
be_fl_index_round_down_chunk(struct m0_be_fl             *fl,
                             const struct be_alloc_chunk *chunk)
{
	return min_type(unsigned long, M0_BE_FL_NR,
			chunk->bac_size / M0_BE_FL_STEP);
}

M0_INTERNAL bool m0_be_fl__invariant(struct m0_be_fl *fl)
{
	return m0_forall(i, ARRAY_SIZE(fl->bfl_free),
			m0_be_list_forall(fl, chunk, be_fl_list(fl, i),
				_0C(be_fl_index_round_down_chunk(fl, chunk) ==
				    i)));
}

M0_INTERNAL void m0_be_fl_add(struct m0_be_fl       *fl,
			      struct m0_be_tx       *tx,
			      struct be_alloc_chunk *chunk)
{
	unsigned long index = be_fl_index_round_down_chunk(fl, chunk);

	M0_PRE_EX(m0_be_fl__invariant(fl));

	fl_be_tlink_create(chunk, tx);
	fl_be_list_add(be_fl_list(fl, index), tx, chunk);

	M0_POST_EX(m0_be_fl__invariant(fl));
}

M0_INTERNAL void m0_be_fl_del(struct m0_be_fl       *fl,
			      struct m0_be_tx       *tx,
			      struct be_alloc_chunk *chunk)
{
	unsigned long index = be_fl_index_round_down_chunk(fl, chunk);

	M0_PRE_EX(m0_be_fl__invariant(fl));

	fl_be_list_del(be_fl_list(fl, index), tx, chunk);
	fl_be_tlink_destroy(chunk, tx);
	M0_POST_EX(m0_be_fl__invariant(fl));
}

M0_INTERNAL struct be_alloc_chunk *m0_be_fl_pick(struct m0_be_fl *fl,
						 m0_bcount_t      size)
{
	struct be_alloc_chunk *chunk;
	struct be_alloc_chunk *iter;
	unsigned long          index;
	struct m0_be_list     *flist;
	int                    i;

	M0_PRE_EX(m0_be_fl__invariant(fl));

	for (index = be_fl_index_round_up(fl, size);
	     index < ARRAY_SIZE(fl->bfl_free); ++index) {
		if (!be_fl_list_is_empty(fl, index))
			break;
	}

	flist = index < ARRAY_SIZE(fl->bfl_free) ? be_fl_list(fl, index) : NULL;
	chunk = flist == NULL ? NULL : fl_be_list_head(flist);
	if (index == M0_BE_FL_NR && chunk != NULL) {
		chunk = NULL;
		i = 0;
		m0_be_list_for(fl, flist, iter) {
			if (iter->bac_size > size &&
			    ergo(chunk != NULL,
				 chunk->bac_size > iter->bac_size)) {
				chunk = iter;
			}
			++i;
			if (i >= M0_BE_FL_PICK_SCAN_LIMIT && chunk != NULL)
				break;
		} m0_be_list_endfor;
	}
	M0_POST(ergo(chunk != NULL, chunk->bac_size >= size));
	return chunk;
}

M0_INTERNAL void m0_be_fl_credit(struct m0_be_fl        *fl,
				 enum m0_be_fl_op        fl_op,
				 struct m0_be_tx_credit *accum)
{
	size_t list_nr = ARRAY_SIZE(fl->bfl_free);

	M0_ASSERT_INFO(M0_IN(fl_op, (M0_BFL_CREATE, M0_BFL_DESTROY,
	                             M0_BFL_ADD, M0_BFL_DEL)),
	               "fl_op=%d", fl_op);
	switch (fl_op) {
	case M0_BFL_CREATE:
		fl_be_list_credit(M0_BLO_CREATE, list_nr, accum);
	case M0_BFL_DESTROY:
		fl_be_list_credit(M0_BLO_DESTROY, list_nr, accum);
		break;
	case M0_BFL_ADD:
		fl_be_list_credit(M0_BLO_TLINK_CREATE, 1, accum);
		fl_be_list_credit(M0_BLO_ADD, 1, accum);
		break;
	case M0_BFL_DEL:
		fl_be_list_credit(M0_BLO_DEL, 1, accum);
		fl_be_list_credit(M0_BLO_TLINK_DESTROY, 1, accum);
		break;
	}
}


/** @} end of be group */

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
