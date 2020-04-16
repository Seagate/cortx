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

#pragma once

#ifndef __MERO_BE_FL_H__
#define __MERO_BE_FL_H__

#include "be/list.h"    /* m0_be_list */
#include "be/list_xc.h"

/**
 * @defgroup be
 *
 * @{
 */

struct be_alloc_chunk;

enum {
	M0_BE_FL_STEP = 8,      /**< each size is aligned on this boundary */
	M0_BE_FL_NR = 128,      /**< number of free lists */
	M0_BE_FL_PICK_SCAN_LIMIT = 0x10, /**< scan limit for best-fit search
					      for chunks with size >=
					      M0_BE_FL_NR * M0_BE_FL_STEP */
};

M0_BASSERT(M0_BE_FL_STEP * M0_BE_FL_NR < M0_BCOUNT_MAX);

/** m0_be_fl operation. @see m0_be_fl_credit() */
enum m0_be_fl_op {
	M0_BFL_CREATE,
	M0_BFL_DESTROY,
	M0_BFL_ADD,
	M0_BFL_DEL,
};

/**
 * m0_be free list for some size of chunks.
 * Currently it is possible to use just list a list instead of struct,
 * but in the future it may be expanded to contain some kind of cache.
 */
struct m0_be_fl_size {
	struct m0_be_list bfs_list;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** m0_be free list */
struct m0_be_fl {
	/*
	 * If i is from range [0,  M0_BE_FL_NR) then i-th element of array
	 * contains free list with chunk sizes in range
	 * [i * M0_BE_FL_STEP, (i + 1) * M0_BE_FL_STEP).
	 * M0_BE_FL_NR-th element contains list of chunks
	 * with size >= (M0_BE_FL_NR * M0_BE_FL_STEP).
	 * @see m0_be_fl_add(), m0_be_fl_pick().
	 */
	struct m0_be_fl_size bfl_free[M0_BE_FL_NR + 1];
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

M0_INTERNAL bool m0_be_fl__invariant(struct m0_be_fl *fl);

M0_INTERNAL void m0_be_fl_create(struct m0_be_fl  *fl,
				 struct m0_be_tx  *tx,
				 struct m0_be_seg *seg);
M0_INTERNAL void m0_be_fl_destroy(struct m0_be_fl *fl,
				  struct m0_be_tx *tx);

M0_INTERNAL void m0_be_fl_add(struct m0_be_fl       *fl,
			      struct m0_be_tx       *tx,
			      struct be_alloc_chunk *chunk);
M0_INTERNAL void m0_be_fl_del(struct m0_be_fl       *fl,
			      struct m0_be_tx       *tx,
			      struct be_alloc_chunk *chunk);

/** find free chunk with size at least `size' */
M0_INTERNAL struct be_alloc_chunk *m0_be_fl_pick(struct m0_be_fl *fl,
						 m0_bcount_t      size);

M0_INTERNAL void m0_be_fl_credit(struct m0_be_fl        *fl,
				 enum m0_be_fl_op        fl_op,
				 struct m0_be_tx_credit *accum);

/** @} end of be group */
#endif /* __MERO_BE_FL_H__ */

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
