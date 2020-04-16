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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 29-May-2013
 */

#pragma once
#ifndef __MERO_BE_ALLOC_H__
#define __MERO_BE_ALLOC_H__

#include "lib/types.h"  /* m0_bcount_t */
#include "lib/mutex.h"

struct m0_be_op;
struct m0_be_seg;
struct m0_be_tx;
struct m0_be_tx_credit;

/**
 * @defgroup be Meta-data back-end
 *
 *
 * @{
 */

enum {
	/**
	 * Allocated memory will be aligned using at least this shift.
	 * @see m0_be_alloc(), m0_be_allocator_credit().
	 */
	M0_BE_ALLOC_SHIFT_MIN  = 3,
};

struct m0_be_allocator_call_stat {
	unsigned long bcs_nr;
	m0_bcount_t   bcs_size;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_allocator_call_stats {
	struct m0_be_allocator_call_stat bacs_alloc_success;
	struct m0_be_allocator_call_stat bacs_alloc_failure;
	struct m0_be_allocator_call_stat bacs_free;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum {
	M0_BE_ALLOCATOR_STATS_BOUNDARY       = 1024,
	M0_BE_ALLOCATOR_STATS_PRINT_INTERVAL = 100,
};

/**
 * Zones within BE segment allocator space.
 *
 * Each zone occupies a fixed fraction of total segment space, which is
 * specified when the allocator is initialised. Space can be requested from
 * particular zones using zones bit mask.
 *
 * Zones in enumeration are sorted by priority in ascending order. For example,
 * if zones M0_BAP_NORMAL and M0_BAP_REPAIR are specified in allocation
 * request, then space in M0_BAP_NORMAL will be allocated only if there is not
 * enough space in M0_BAP_REPAIR.
 *
 * The memory is allocated exactly in one zone and can't be distributed among
 * several zones even if several zones are specified in an allocation request.
 *
 * Zones don't have predefined memory boundaries and can be viewed as "labels"
 * for allocated memory chunks. Particular memory chunk can reside in different
 * zones during system life cycle.
 */
enum m0_be_alloc_zone_type {
	/**
	 * Repair zone is used by index repair to allocate space for spare
	 * units.
	 */
	M0_BAP_REPAIR,
	/**
	 * Normal zone contains the bulk of segment space.
	 * It is used for usual allocations.
	 */
	M0_BAP_NORMAL,
	/* Maybe more zones in the future. */
	M0_BAP_NR
} M0_XCA_ENUM;

struct m0_be_alloc_zone_stats {
	m0_bcount_t                        bzs_total;
	m0_bcount_t                        bzs_used;
	m0_bcount_t                        bzs_free;
	uint32_t                           bzs_type M0_XCA_FENUM(
							m0_be_alloc_zone_type);
	struct m0_be_allocator_call_stats  bzs_stats;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
 * @brief Allocator statistics
 *
 * It is embedded into m0_be_allocator_header.
 */
struct m0_be_allocator_stats {
	m0_bcount_t                       bas_chunk_overhead;
	m0_bcount_t                       bas_space_total;
	m0_bcount_t                       bas_space_used;
	m0_bcount_t                       bas_space_free;
	m0_bcount_t                       bas_stat0_boundary;
	m0_bcount_t                       bas_chunks_nr;
	m0_bcount_t                       bas_free_chunks_nr;
	struct m0_be_allocator_call_stats bas_total;
	struct m0_be_allocator_call_stats bas_stat0;
	struct m0_be_allocator_call_stats bas_stat1;
	unsigned long                     bas_print_interval;
	unsigned long                     bas_print_index;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_allocator_header;

/** @brief Allocator */
struct m0_be_allocator {
	/**
	 * Memory is allocated from the segment using first-fit algorithm.
	 * Entire segment except m0_be_seg_hdr is used as a memory
	 * for allocations.
	 */
	struct m0_be_seg	      *ba_seg;
	/**
	 * Lock protects allocator lists and allocator chunks
	 * (but not allocated memory).
	 */
	struct m0_mutex		       ba_lock;
	/** Internal allocator data. It is stored inside the segment. */
	struct m0_be_allocator_header *ba_h[M0_BAP_NR];
};

/**
 * Initialize allocator structure.
 *
 * @see m0_be_allocator_header.
 */
M0_INTERNAL int m0_be_allocator_init(struct m0_be_allocator *a,
				     struct m0_be_seg *seg);

/**
 * Finalize allocator structure.
 *
 * It will not affect allocated memory, allocator space or allocator header.
 */
M0_INTERNAL void m0_be_allocator_fini(struct m0_be_allocator *a);

/**
 * Allocator invariant.
 *
 * It will perform detailed verification of allocator data structures.
 * It will ignore all user data.
 */
M0_INTERNAL bool m0_be_allocator__invariant(struct m0_be_allocator *a);

/**
 * Create allocator on the segment.
 *
 * @see m0_be_allocator.ba_seg, m0_be_allocator_init(),
 * m0_be_allocator_header.
 */
M0_INTERNAL int m0_be_allocator_create(struct m0_be_allocator *a,
				       struct m0_be_tx        *tx,
				       uint32_t               *zone_percent,
				       uint32_t                zones_nr);

/**
 * Destroy allocator on the segment.
 *
 * All memory allocations obtained from m0_be_alloc{,_aligned}()
 * should be m0_be_free{,_aligned}()'d before calling this function.
 */
M0_INTERNAL void m0_be_allocator_destroy(struct m0_be_allocator *a,
					 struct m0_be_tx *tx);

/**
 * Allocator operation.
 *
 * @see m0_be_allocator_credit().
 */
enum m0_be_allocator_op {
	M0_BAO_CREATE,	/**< Allocator credit for m0_be_allocator_create() */
	M0_BAO_DESTROY,	/**< Allocator credit for m0_be_allocator_destroy() */
	M0_BAO_ALLOC,	      /**< Allocator credit for m0_be_alloc() */
	M0_BAO_ALLOC_ALIGNED, /**< Allocator credit for m0_be_alloc_aligned() */
	M0_BAO_FREE,	      /**< Allocator credit for m0_be_free() */
	M0_BAO_FREE_ALIGNED,  /**< Allocator credit for m0_be_free_aligned() */
};

/**
 * Accumulate credits for optype in accum.
 *
 * @param a Allocator
 * @param optype Allocator operation type
 * @param size Size of allocation. Makes sense only for
 *	       M0_IN(optype, (M0_BAO_ALLOC_ALIGNED, M0_BAO_ALLOC)).
 *	       It is ignored for other optypes.
 * @param shift Memory alignment shift. Makes sense only for
 *		optype == M0_BAO_ALLOC_ALIGNED. It is ignored for other optypes.
 * @param accum Accumulator for credits.
 *
 * @see m0_be_alloc_aligned(), m0_be_alloc(), m0_be_free(),
 * m0_be_free_aligned(), m0_be_allocator_op, m0_be_tx_credit.
 */
M0_INTERNAL void m0_be_allocator_credit(struct m0_be_allocator *a,
					enum m0_be_allocator_op optype,
					m0_bcount_t size,
					unsigned shift,
					struct m0_be_tx_credit *accum);

/**
 * Allocate memory.
 *
 * @param a Allocator
 * @param tx Allocation will be done in this transaction
 * @param op See m0_be_op.
 * @param ptr Pointer to allocated memory will be stored to *ptr.
 *	      *ptr shouldn't be used before completion of the operation.
 *	      Can be NULL.
 * @param size Memory size
 * @param shift Memory will be aligned on (shift^2)-byte boundary.
 *		It can be less than M0_BE_ALLOC_SHIFT_MIN - in this case
 *		allocation will be done as if it is equal to
 *		M0_BE_ALLOC_SHIFT_MIN.
 * @param zonemask Bit mask of the zones where memory should be allocated.
 *                 The first zone from the bit mask with sufficient space will
 *                 be chosen for allocation, see m0_be_alloc_zone_type.
 *
 * The memory should be freed using m0_be_free_aligned().
 *
 * @see m0_be_alloc(), m0_be_allocator_credit(), M0_BAO_ALLOC_ALIGNED,
 * m0_alloc_aligned(), M0_BE_ALLOC_SHIFT_MIN.
 */
M0_INTERNAL void m0_be_alloc_aligned(struct m0_be_allocator *a,
				     struct m0_be_tx *tx,
				     struct m0_be_op *op,
				     void **ptr,
				     m0_bcount_t size,
				     unsigned shift,
				     uint64_t zonemask);

/**
 * Allocate memory.
 *
 * The memory allocated is guaranteed to be suitably aligned
 * for any kind of variable. See m0_be_alloc_aligned() for
 * parameters description.
 *
 * @see m0_be_alloc_aligned(), M0_BAO_ALLOC, m0_alloc().
 */
M0_INTERNAL void m0_be_alloc(struct m0_be_allocator *a,
			     struct m0_be_tx *tx,
			     struct m0_be_op *op,
			     void **ptr,
			     m0_bcount_t size);

/**
 * Free memory allocated with m0_be_alloc_aligned().
 *
 * @param a Allocator
 * @param tx Free operation will be done in this transaction
 * @param op See m0_be_op.
 * @param ptr Pointer to memory to release. May be NULL - in this case
 *	      call to this function is no-op. If ptr is not NULL,
 *	      then it should be exactly the pointer returned
 *	      from m0_be_alloc() for allocator a. Double m0_be_free()
 *	      is not allowed for the same pointer from one call to
 *	      m0_be_alloc().
 *
 * @see m0_be_alloc_aligned(), M0_BAO_FREE_ALIGNED, m0_be_allocator_destroy().
 */
M0_INTERNAL void m0_be_free_aligned(struct m0_be_allocator *a,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    void *ptr);

/**
 * Free memory allocated with m0_be_alloc().
 *
 * @see m0_be_free_aligned(), M0_BAO_FREE.
 */
M0_INTERNAL void m0_be_free(struct m0_be_allocator *a,
			    struct m0_be_tx *tx,
			    struct m0_be_op *op,
			    void *ptr);
/**
 * Return allocator statistics.
 *
 * @note Not implemented yet.
 * @see m0_be_allocator_stats.
 */
M0_INTERNAL void m0_be_alloc_stats(struct m0_be_allocator *a,
				   struct m0_be_allocator_stats *out);

M0_INTERNAL void m0_be_alloc_stats_credit(struct m0_be_allocator *a,
                                          struct m0_be_tx_credit *accum);
M0_INTERNAL void m0_be_alloc_stats_capture(struct m0_be_allocator *a,
                                           struct m0_be_tx        *tx);


/**
 * Allocate array of structures.
 *
 * It is a wrapper around m0_be_alloc().
 * @see m0_be_alloc(), M0_ALLOC_ARR().
 */
#define M0_BE_ALLOC_ARR(arr, nr, seg, tx, op)				\
		m0_be_alloc(m0_be_seg_allocator(seg), (tx), (op),	\
			    (void **)&(arr), (nr) * sizeof((arr)[0]))

/**
 * Allocate structure.
 *
 * It is a wrapper around m0_be_alloc().
 * @see m0_be_alloc(), M0_ALLOC_PTR(), M0_BE_ALLOC_ARR().
 */
#define M0_BE_ALLOC_PTR(ptr, seg, tx, op)				\
		M0_BE_ALLOC_ARR((ptr), 1, (seg), (tx), (op))

#define M0_BE_ALLOC_ARR_SYNC(arr, nr, seg, tx)				\
		M0_BE_OP_SYNC(__op,					\
			      M0_BE_ALLOC_ARR((arr), (nr), (seg), (tx), &__op))

#define M0_BE_ALLOC_PTR_SYNC(ptr, seg, tx)				\
		M0_BE_OP_SYNC(__op, M0_BE_ALLOC_PTR((ptr), (seg), (tx), &__op))

#define M0_BE_FREE_PTR(ptr, seg, tx, op)				\
		m0_be_free(m0_be_seg_allocator(seg), (tx), (op), (ptr))

#define M0_BE_FREE_PTR_SYNC(ptr, seg, tx)				\
		M0_BE_OP_SYNC(__op, M0_BE_FREE_PTR((ptr), (seg), (tx), &__op))

#define M0_BE_ALLOC_BUF(buf, seg, tx, op)				\
		m0_be_alloc(m0_be_seg_allocator(seg), (tx), (op),	\
			    &(buf)->b_addr, (buf)->b_nob)

#define M0_BE_ALLOC_BUF_SYNC(buf, seg, tx)				\
		M0_BE_OP_SYNC(__op, M0_BE_ALLOC_BUF((buf), (seg), (tx), &__op))

#define M0_BE_ALLOC_CREDIT_PTR(ptr, seg, accum)				\
		m0_be_allocator_credit(m0_be_seg_allocator(seg),	\
				       M0_BAO_ALLOC, sizeof *(ptr), 0, (accum))

#define M0_BE_FREE_CREDIT_PTR(ptr, seg, accum)				\
		m0_be_allocator_credit(m0_be_seg_allocator(seg),	\
				       M0_BAO_FREE, sizeof *(ptr), 0, (accum))

#define M0_BE_ALLOC_CREDIT_ARR(arr, nr, seg, accum)				\
		m0_be_allocator_credit(m0_be_seg_allocator(seg),	\
				       M0_BAO_ALLOC, (nr) * sizeof((arr)[0]), 0, (accum))

#define M0_BE_FREE_CREDIT_ARR(arr, nr, seg, accum)				\
		m0_be_allocator_credit(m0_be_seg_allocator(seg),	\
				       M0_BAO_FREE, (nr) * sizeof((arr)[0]), 0, (accum))

#define M0_BE_ALLOC_CREDIT_BUF(buf, seg, accum)				\
		m0_be_allocator_credit(m0_be_seg_allocator(seg),	\
				       M0_BAO_ALLOC, (buf)->b_nob, 0, (accum))


/** @} end of be group */
#endif /* __MERO_BE_ALLOC_H__ */
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
