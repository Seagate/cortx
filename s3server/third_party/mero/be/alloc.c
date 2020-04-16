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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/alloc.h"
#include "be/alloc_internal.h"
#include "be/seg_internal.h"    /* m0_be_seg_hdr */
#include "be/tx.h"              /* M0_BE_TX_CAPTURE_PTR */
#include "be/op.h"              /* m0_be_op */
#include "lib/memory.h"         /* m0_addr_is_aligned */
#include "lib/errno.h"          /* ENOSPC */
#include "lib/misc.h"           /* memset, M0_BITS, m0_forall */
#include "mero/magic.h"
#include "be/domain.h"          /* m0_be_domain */

/*
 * @addtogroup be
 * @todo make a doxygen page
 *
 * Overview
 *
 * Allocator provides API to allocate and free memory in BE segment.
 *
 * Definitions
 *
 * - allocator segment - BE segment which is used as memory for allocations;
 * - allocator space - part of memory inside the allocator segment in which
 *   all allocations will take place. It may be smaller than segment itself;
 * - chunk - memory structure that contains allocator data and user data.
 *   User data follows allocator data in memory. There is no free space
 *   in memory after allocator data and user data of the same chunk;
 * - "chunk B located just after chunk A" - there is no free space in memory
 *   after end of user data and in chunk A and before allocator data in chunk B;
 * - used chunk - chunk for which address of user data was returned
 *   to user from m0_be_alloc() and for which m0_be_free() wasn't called;
 * - free chunk - chunk that is not used;
 * - adjacent chunks - A and B are adjacent chunks iff chunk A located
 *   just after B or vice versa;
 *
 * API
 *
 * - m0_be_alloc() and m0_be_alloc_aligned(): allocate memory;
 * - m0_be_free() and m0_be_free_aligned(): free memory allocated with
 *   m0_be_alloc() and m0_be_alloc_aligned() respectively;
 * - m0_be_alloc_stats(): provide some allocator statistics;
 *
 * Algorithm
 *
 * Allocator has:
 * - list of all chunks;
 * - data structure (m0_be_fl) to keep account of free chunks.
 *
 * m0_be_alloc_aligned()
 * - uses m0_be_fl to pick free chunk that meets alignment and size requirement;
 * - splits free chunk to have wasted space as little as possible.
 *
 * m0_be_free_aligned()
 * - merges free chunk with adjacent chunks if they are free, so at most 2 merge
 *   operations takes place.
 *
 * m0_be_alloc(), m0_be_free()
 * - they are calls to m0_be_alloc_aligned() and m0_be_free_aligned with
 *   M0_BE_ALLOC_SHIFT_MIN alignment shift.
 *
 * Allocator space invariants:
 * - Each byte of allocator space belongs to a chunk. There is one exception -
 *   if there is no space for chunk with at least 1 byte of user data from
 *   the beginning of allocator space to other chunk then this space is
 *   temporary unused;
 *
 * Chunk invariants:
 * - all chunks are mutually disjoint;
 * - chunk is entirely inside the allocator space;
 * - each chunk is either free or used;
 * - user data in chunk located just after allocator data;
 *
 * List of all chunks invariants:
 * - all chunks in a list are ordered by address;
 * - every chunk is in the list;
 * - chunks that are adjacent in the list are adjacent in allocator space.
 *
 * Special cases
 *
 * Chunk split in be_alloc_chunk_split()
 *
 * @verbatim
 * |		   |	|	   | |	      |	|	 |
 * +---------------+	+----------+ +--------+ |	 |
 * |	prev	   |	|  prev	   | |	      | |	 |
 * +---------------+	+----------+ |  prev  | |	 | < start0
 * |		   |	|  chunk0  | |	      | |	 |
 * |		   |	+----------+ +--------+ +--------+ < start_new
 * |    c	   |	|	   | |	      | |	 |
 * |		   |	|  new	   | |  new   | |  new	 |
 * |		   |	|	   | |	      | |	 |
 * |		   |	+----------+ +--------+ |	 | < start1
 * |		   |	|  chunk1  | |	      | |	 |
 * +---------------+	+----------+ |	      | +--------+ < start_next
 * |	next	   |	|  next	   | |	      | |  next	 |
 * +---------------+	+----------+ |	      | +--------+
 * |		   |	|	   | |	      |	|	 |
 *
 * initial position	after split  no space	 no space
 *				     for chunk0  for chunk1
 * @endverbatim
 *
 * Free chunks merge if it is possible in be_alloc_chunk_trymerge()
 *
 * @verbatim
 * |	      | |	   |
 * +----------+ +----------+
 * |	      |	|	   |
 * |	a     |	|	   |
 * |	      |	|	   |
 * |	      |	|	   |
 * +----------+	|	   |
 * |	      |	|    a	   |
 * |	      |	|	   |
 * |	      |	|	   |
 * |	b     |	|	   |
 * |	      |	|	   |
 * |	      |	|	   |
 * |	      |	|	   |
 * +----------+	+----------+
 * |	      | |	   |
 * @endverbatim
 *
 * Time and I/O complexity.
 * - m0_be_alloc_aligned() has the same time complexity and I/O complexity as
 *   m0_be_fl_pick();
 * - m0_be_free_aligned() has time complexity and I/O complexity O(1) -
 *   the same as m0_be_fl_add() and m0_be_fl_del();
 * - m0_be_alloc_aligned() and m0_be_free_aligned() has optimisations for
 *   M0_BE_ALLOC_SHIFT_MIN alignment shift so if there is no special
 *   requirements for alignment it's better to use m0_be_alloc() and
 *   m0_be_free().
 *
 * Limitations
 * - allocator can use only one BE segment;
 * - allocator can use only continuous allocator space;
 *
 * Known issues
 * - all allocator functions are fully synchronous now despite the fact that
 *   they have m0_be_op parameter;
 * - m0_be_allocator_stats are unused;
 * - allocator credit includes 2 * size requested for alignment shift greater
 *   than M0_BE_ALLOC_SHIFT_MIN;
 * - it is not truly O(1) allocator; see m0_be_fl documentation for explanation;
 * - there is one big allocator lock that protects all allocations/deallocation.
 *
 * Locks
 * Allocator lock (m0_mutex) is used to protect all allocator data.
 *
 * Space reservation for DIX recovery
 * ----------------------------------
 *
 * Space is reserved to not get -ENOSPC during DIX repair.
 *
 * A special allocator zone M0_BAP_REPAIR is introduced for a memory allocated
 * during DIX repair. This zone is specified in zone mask in
 * m0_be_alloc_aligned() and its callers up to functions which may be called
 * during repair (from m0_dix_cm_cp_write()). Functions/macro which are never
 * called during repair pass always M0_BITS(M0_BAP_NORMAL) as zone mask.
 *
 * In fact, each zone is implemented as independent space in BE segment
 * represented by own m0_be_allocator_header.
 *
 * Repair uses all available space in the allocator while normal alloc fails
 * if free space is less than reserved. Repair uses M0_BAP_REPAIR zone by
 * default, but if there is no space in M0_BAP_REPAIR, then memory will be
 * allocated in M0_BAP_NORMAL zone.
 *
 * The space is reserved for repair zone during m0mkfs in
 * m0_be_allocator_create(). Percentage of free space is passed to
 * m0_be_allocator_create() via 'zone_percent' argument which is assigned in
 * cs_be_init(). The space is reserved in terms of bytes, not memory region, so
 * fragmentation can prevent successful allocation from reserved space if there
 * is no contiguous memory block with requested size.
 *
 * Repair zone is not accounted in df output.
 */

/*
 * @addtogroup be
 *
 * @{
 */

enum {
	/** Alignment for m0_be_allocator_header inside segment. */
	BE_ALLOC_HEADER_SHIFT = 3,
	/** Alignment for zone's size. */
	BE_ALLOC_ZONE_SIZE_SHIFT = 3,
};

M0_BE_LIST_DESCR_DEFINE(chunks_all, "list of all chunks in m0_be_allocator",
			static, struct be_alloc_chunk, bac_linkage, bac_magic,
			M0_BE_ALLOC_ALL_LINK_MAGIC, M0_BE_ALLOC_ALL_MAGIC);
M0_BE_LIST_DEFINE(chunks_all, static, struct be_alloc_chunk);

static const char *be_alloc_zone_name(enum m0_be_alloc_zone_type type)
{
	static const char *zone_names[] = {
		[M0_BAP_REPAIR] = "repair",
		[M0_BAP_NORMAL] = "normal"
	};

	M0_CASSERT(ARRAY_SIZE(zone_names) == M0_BAP_NR);
	M0_PRE(type < M0_BAP_NR);
	return zone_names[type];
}

static void
be_allocator_call_stat_init(struct m0_be_allocator_call_stat *cstat)
{
	*cstat = (struct m0_be_allocator_call_stat){
		.bcs_nr   = 0,
		.bcs_size = 0,
	};
}

static void be_allocator_call_stats_init(struct m0_be_allocator_call_stats *cs)
{
	be_allocator_call_stat_init(&cs->bacs_alloc_success);
	be_allocator_call_stat_init(&cs->bacs_alloc_failure);
	be_allocator_call_stat_init(&cs->bacs_free);
}

static void be_allocator_stats_init(struct m0_be_allocator_stats  *stats,
				    struct m0_be_allocator_header *h)
{
	*stats = (struct m0_be_allocator_stats){
		.bas_chunk_overhead = sizeof(struct be_alloc_chunk),
		.bas_stat0_boundary = M0_BE_ALLOCATOR_STATS_BOUNDARY,
		.bas_print_interval = M0_BE_ALLOCATOR_STATS_PRINT_INTERVAL,
		.bas_print_index    = 0,
		.bas_space_total    = h->bah_size,
		.bas_space_free     = h->bah_size,
		.bas_space_used     = 0,
	};
	be_allocator_call_stats_init(&stats->bas_total);
	be_allocator_call_stats_init(&stats->bas_stat0);
	be_allocator_call_stats_init(&stats->bas_stat1);
}

static void
be_allocator_call_stat_update(struct m0_be_allocator_call_stat *cstat,
                              unsigned long                     nr,
                              m0_bcount_t                       size)
{
	cstat->bcs_nr   += nr;
	cstat->bcs_size += size;
}

static void
be_allocator_call_stats_update(struct m0_be_allocator_call_stats *cs,
			       m0_bcount_t                        size,
			       bool                               alloc,
			       bool                               failed)
{
	struct m0_be_allocator_call_stat *cstat;
	if (alloc && failed) {
		cstat = &cs->bacs_alloc_failure;
	} else if (alloc) {
		cstat = &cs->bacs_alloc_success;
	} else {
		cstat = &cs->bacs_free;
	}
	be_allocator_call_stat_update(cstat, 1, size);
}

static void
be_allocator_call_stats_print(struct m0_be_allocator_call_stats *cs,
                              const char                        *descr)
{
#define P_ACS(acs) (acs)->bcs_nr, (acs)->bcs_size
	M0_LOG(M0_DEBUG, "%s (nr, size): alloc_success=(%lu, %lu), "
	       "free=(%lu, %lu), alloc_failure=(%lu, %lu)", descr,
	       P_ACS(&cs->bacs_alloc_success), P_ACS(&cs->bacs_free),
	       P_ACS(&cs->bacs_alloc_failure));
#undef P_ACS
}

static void be_allocator_stats_print(struct m0_be_allocator_stats *stats)
{
	M0_LOG(M0_DEBUG, "stats=%p chunk_overhead=%lu boundary=%lu "
	       "print_interval=%lu print_index=%lu",
	       stats, stats->bas_chunk_overhead, stats->bas_stat0_boundary,
	       stats->bas_print_interval, stats->bas_print_index);
	M0_LOG(M0_DEBUG, "chunks=%lu free_chunks=%lu",
	       stats->bas_chunks_nr, stats->bas_free_chunks_nr);
	be_allocator_call_stats_print(&stats->bas_total, "           total");
	be_allocator_call_stats_print(&stats->bas_stat0, "size <= boundary");
	be_allocator_call_stats_print(&stats->bas_stat1, "size >  boundary");
}

static void be_allocator_stats_update(struct m0_be_allocator_stats *stats,
                                      m0_bcount_t                   size,
                                      bool                          alloc,
				      bool                          failed)
{
	unsigned long space_change;
	long          multiplier;

	M0_PRE(ergo(failed, alloc));

	multiplier   = failed ? 0 : alloc ? 1 : -1;
	space_change = size + stats->bas_chunk_overhead;
	stats->bas_space_used += multiplier * space_change;
	stats->bas_space_free -= multiplier * space_change;
	be_allocator_call_stats_update(&stats->bas_total, size, alloc, failed);
	be_allocator_call_stats_update(size <= stats->bas_stat0_boundary ?
				       &stats->bas_stat0 : &stats->bas_stat1,
				       size, alloc, failed);
	if (stats->bas_print_index++ == stats->bas_print_interval) {
		be_allocator_stats_print(stats);
		stats->bas_print_index = 0;
	}
}

static void be_allocator_stats_capture(struct m0_be_allocator *a,
				       enum m0_be_alloc_zone_type ztype,
				       struct m0_be_tx *tx)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];

	if (tx != NULL)
		M0_BE_TX_CAPTURE_PTR(a->ba_seg, tx, &h->bah_stats);
}

static void be_alloc_chunk_capture(struct m0_be_allocator *a,
				   struct m0_be_tx *tx,
				   struct be_alloc_chunk *c)
{
	if (tx != NULL && c != NULL)
		M0_BE_TX_CAPTURE_PTR(a->ba_seg, tx, c);
}

static void be_alloc_free_flag_capture(const struct m0_be_allocator *a,
				       struct m0_be_tx *tx,
				       struct be_alloc_chunk *c)
{
	if (tx != NULL)
		M0_BE_TX_CAPTURE_PTR(a->ba_seg, tx, &c->bac_free);
}

static void be_alloc_size_capture(const struct m0_be_allocator *a,
				  struct m0_be_tx *tx,
				  struct be_alloc_chunk *c)
{
	if (tx != NULL)
		M0_BE_TX_CAPTURE_PTR(a->ba_seg, tx, &c->bac_size);
}

static bool be_alloc_mem_is_in(const struct m0_be_allocator *a,
			       enum m0_be_alloc_zone_type ztype,
			       const void *ptr, m0_bcount_t size)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];

	return ptr >= h->bah_addr &&
	       ptr + size <= h->bah_addr + h->bah_size;
}

static bool be_alloc_chunk_is_in(const struct m0_be_allocator *a,
				 enum m0_be_alloc_zone_type ztype,
				 const struct be_alloc_chunk *c)
{
	return be_alloc_mem_is_in(a, ztype, c, sizeof *c + c->bac_size);
}

static bool be_alloc_chunk_is_not_overlapping(const struct be_alloc_chunk *a,
					      const struct be_alloc_chunk *b)
{
#if 0
	return a == NULL || b == NULL ||
	       (a < b && &a->bac_mem[a->bac_size] <= (char *) b);
#else
	return a == NULL || b == NULL ||
	       (a < b && &a->bac_mem[a->bac_size] <= (char *) b) ||
	       (b < a && &b->bac_mem[b->bac_size] <= (char *) a);
#endif
}

static bool be_alloc_chunk_invariant(struct m0_be_allocator *a,
				     const struct be_alloc_chunk *c)
{
	enum m0_be_alloc_zone_type     ztype = c->bac_zone;
	struct m0_be_allocator_header *h;
	struct be_alloc_chunk         *cprev;
	struct be_alloc_chunk         *cnext;

	M0_PRE(ztype < M0_BAP_NR);

	h = a->ba_h[ztype];
	cprev = chunks_all_be_list_prev(&h->bah_chunks, c);
	cnext = chunks_all_be_list_next(&h->bah_chunks, c);

	return _0C(c != NULL) &&
	       _0C(be_alloc_chunk_is_in(a, ztype, c)) &&
	       _0C(m0_addr_is_aligned(&c->bac_mem, M0_BE_ALLOC_SHIFT_MIN)) &&
	       _0C(ergo(cnext != NULL,
			be_alloc_chunk_is_in(a, ztype, cnext))) &&
	       _0C(ergo(cprev != NULL,
			be_alloc_chunk_is_in(a, ztype, cprev))) &&
	       _0C(c->bac_magic0 == M0_BE_ALLOC_MAGIC0) &&
	       _0C(c->bac_magic1 == M0_BE_ALLOC_MAGIC1) &&
	       _0C(be_alloc_chunk_is_not_overlapping(cprev, c)) &&
	       _0C(be_alloc_chunk_is_not_overlapping(c, cnext));
}

static void be_alloc_chunk_init(struct m0_be_allocator *a,
				enum m0_be_alloc_zone_type ztype,
				struct m0_be_tx *tx,
				struct be_alloc_chunk *c,
				m0_bcount_t size, bool free)
{
	*c = (struct be_alloc_chunk) {
		.bac_magic0 = M0_BE_ALLOC_MAGIC0,
		.bac_size   = size,
		.bac_free   = free,
		.bac_zone   = ztype,
		.bac_magic1 = M0_BE_ALLOC_MAGIC1,
	};
	chunks_all_be_tlink_create(c, tx);
	/*
	 * Move this right before m0_be_tlink_create() to optimize capturing
	 * size. Chunk capturing at the end of the function will help with
	 * debugging credit calculation errors with current regmap
	 * implementation.
	 */
	be_alloc_chunk_capture(a, tx, c);
}

static void be_alloc_chunk_del_fini(struct m0_be_allocator *a,
				    enum m0_be_alloc_zone_type ztype,
				    struct m0_be_tx *tx,
				    struct be_alloc_chunk *c)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];

	M0_PRE(be_alloc_chunk_invariant(a, c));
	M0_PRE(c->bac_zone == ztype);

	m0_be_fl_del(&h->bah_fl, tx, c);

	chunks_all_be_list_del(&h->bah_chunks, tx, c);
	chunks_all_be_tlink_destroy(c, tx);
}

static struct be_alloc_chunk *be_alloc_chunk_addr(void *ptr)
{
	return container_of(ptr, struct be_alloc_chunk, bac_mem);
}

static struct be_alloc_chunk *
be_alloc_chunk_prev(struct m0_be_allocator *a,
		    enum m0_be_alloc_zone_type ztype,
		    struct be_alloc_chunk *c)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];
	struct be_alloc_chunk         *r;

	M0_PRE(c->bac_zone == ztype);

	r = chunks_all_be_list_prev(&h->bah_chunks, c);
	M0_ASSERT(ergo(r != NULL, be_alloc_chunk_invariant(a, r)));
	return r;
}

static struct be_alloc_chunk *
be_alloc_chunk_next(struct m0_be_allocator *a,
		    enum m0_be_alloc_zone_type ztype,
		    struct be_alloc_chunk *c)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];
	struct be_alloc_chunk         *r;

	M0_PRE(c->bac_zone == ztype);

	r = chunks_all_be_list_next(&h->bah_chunks, c);
	M0_ASSERT_EX(ergo(r != NULL, be_alloc_chunk_invariant(a, r)));
	return r;
}

static void be_alloc_chunk_mark_free(struct m0_be_allocator *a,
				     enum m0_be_alloc_zone_type ztype,
				     struct m0_be_tx *tx,
				     struct be_alloc_chunk *c)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];

	M0_PRE(be_alloc_chunk_invariant(a, c));
	M0_PRE(!c->bac_free);
	M0_PRE(c->bac_zone == ztype);

	m0_be_fl_add(&h->bah_fl, tx, c);
	c->bac_free = true;
	be_alloc_free_flag_capture(a, tx, c);

	M0_POST(c->bac_free);
	M0_POST(be_alloc_chunk_invariant(a, c));
}

static uintptr_t be_alloc_chunk_after(struct m0_be_allocator *a,
				      enum m0_be_alloc_zone_type ztype,
				      struct be_alloc_chunk *c)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];

	M0_PRE(ergo(c != NULL, c->bac_zone == ztype));

	return c == NULL ? (uintptr_t) h->bah_addr :
			   (uintptr_t) &c->bac_mem[c->bac_size];
}

/** try to add a chunk after the c */
static struct be_alloc_chunk *
be_alloc_chunk_add_after(struct m0_be_allocator *a,
			 enum m0_be_alloc_zone_type ztype,
			 struct m0_be_tx *tx,
			 struct be_alloc_chunk *c,
			 uintptr_t offset,
			 m0_bcount_t size_total,
			 bool free)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];
	struct be_alloc_chunk         *new;

	M0_PRE(ergo(c != NULL, be_alloc_chunk_invariant(a, c)));
	M0_PRE(size_total > sizeof *new);
	M0_PRE(ergo(c != NULL, c->bac_zone == ztype));

	new = c == NULL ? (struct be_alloc_chunk *)
			  ((uintptr_t) h->bah_addr + offset) :
			  (struct be_alloc_chunk *)
			  be_alloc_chunk_after(a, ztype, c);
	be_alloc_chunk_init(a, ztype, tx, new, size_total - sizeof *new, free);

	if (c != NULL)
		chunks_all_be_list_add_after(&h->bah_chunks, tx, c, new);
	else
		chunks_all_be_list_add(&h->bah_chunks, tx, new);

	if (free)
		m0_be_fl_add(&h->bah_fl, tx, new);

	M0_POST(be_alloc_chunk_invariant(a, new));
	M0_PRE(ergo(c != NULL, be_alloc_chunk_invariant(a, c)));
	return new;
}

static void be_alloc_chunk_resize(struct m0_be_allocator *a,
				  enum m0_be_alloc_zone_type ztype,
				  struct m0_be_tx *tx,
				  struct be_alloc_chunk *c,
				  m0_bcount_t new_size)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];

	M0_PRE(c->bac_zone == ztype);

	if (c->bac_free)
		m0_be_fl_del(&h->bah_fl, tx, c);
	c->bac_size = new_size;
	if (c->bac_free)
		m0_be_fl_add(&h->bah_fl, tx, c);
	be_alloc_size_capture(a, tx, c);
}

static struct be_alloc_chunk *
be_alloc_chunk_tryadd_free_after(struct m0_be_allocator *a,
				 enum m0_be_alloc_zone_type ztype,
				 struct m0_be_tx *tx,
				 struct be_alloc_chunk *c,
				 uintptr_t offset,
				 m0_bcount_t size_total)
{
	if (size_total <= sizeof *c) {
		if (c != NULL) {
			be_alloc_chunk_resize(a, ztype, tx, c,
					      c->bac_size + size_total);
		} else
			; /* space before the first chunk is temporary lost */
	} else {
		c = be_alloc_chunk_add_after(a, ztype, tx, c,
					     offset, size_total, true);
	}
	return c;
}

static struct be_alloc_chunk *
be_alloc_chunk_split(struct m0_be_allocator *a,
		     enum m0_be_alloc_zone_type ztype,
		     struct m0_be_tx *tx,
		     struct be_alloc_chunk *c,
		     uintptr_t start_new,
		     m0_bcount_t size)
{
	struct be_alloc_chunk *prev;
	struct be_alloc_chunk *new;
	uintptr_t	       start0;
	uintptr_t	       start1;
	uintptr_t	       start_next;
	m0_bcount_t            size_min_aligned;
	m0_bcount_t	       chunk0_size;
	m0_bcount_t	       chunk1_size;

	M0_PRE(be_alloc_chunk_invariant(a, c));
	M0_PRE(c->bac_free);

	size_min_aligned = m0_align(size, 1UL << M0_BE_ALLOC_SHIFT_MIN);

	prev	    = be_alloc_chunk_prev(a, ztype, c);

	start0	    = be_alloc_chunk_after(a, ztype, prev);
	start1	    = start_new + sizeof *new + size_min_aligned;
	start_next  = be_alloc_chunk_after(a, ztype, c);
	chunk0_size = start_new - start0;
	chunk1_size = start_next - start1;
	M0_ASSERT(start0    <= start_new);
	M0_ASSERT(start_new <= start1);
	M0_ASSERT(start1    <= start_next);

	be_alloc_chunk_del_fini(a, ztype, tx, c);
	/* c is not a valid chunk now */

	prev = be_alloc_chunk_tryadd_free_after(a, ztype, tx, prev, 0,
						chunk0_size);
	new = be_alloc_chunk_add_after(a, ztype, tx, prev,
				       prev == NULL ? chunk0_size : 0,
				       sizeof *new + size_min_aligned, false);
	M0_ASSERT(new != NULL);
	be_alloc_chunk_tryadd_free_after(a, ztype, tx, new, 0, chunk1_size);

	M0_POST(!new->bac_free);
	M0_POST(new->bac_size >= size);
	M0_POST(be_alloc_chunk_invariant(a, new));
	return new;
}

static struct be_alloc_chunk *
be_alloc_chunk_trysplit(struct m0_be_allocator *a,
			enum m0_be_alloc_zone_type ztype,
			struct m0_be_tx *tx,
			struct be_alloc_chunk *c,
			m0_bcount_t size, unsigned shift)
{
	struct be_alloc_chunk *result = NULL;
	uintptr_t	       alignment = 1UL << shift;
	uintptr_t	       addr_mem;
	uintptr_t	       addr_start;
	uintptr_t	       addr_end;

	M0_PRE(be_alloc_chunk_invariant(a, c));
	M0_PRE(alignment != 0);
	if (c->bac_free) {
		addr_start = (uintptr_t) c;
		addr_end   = (uintptr_t) &c->bac_mem[c->bac_size];
		/* find aligned address for memory block */
		addr_mem   = addr_start + sizeof *c + alignment - 1;
		addr_mem  &= ~(alignment - 1);
		/* if block fits inside free chunk */
		result = addr_mem + size <= addr_end ?
			 be_alloc_chunk_split(a, ztype, tx, c,
					     addr_mem - sizeof *c, size) : NULL;
	}
	M0_POST(ergo(result != NULL, be_alloc_chunk_invariant(a, result)));
	return result;
}

static bool be_alloc_chunk_trymerge(struct m0_be_allocator *a,
				    enum m0_be_alloc_zone_type ztype,
				    struct m0_be_tx *tx,
				    struct be_alloc_chunk *x,
				    struct be_alloc_chunk *y)
{
	m0_bcount_t y_size_total;
	bool	    chunks_were_merged = false;

	M0_PRE(ergo(x != NULL, be_alloc_chunk_invariant(a, x)));
	M0_PRE(ergo(y != NULL, be_alloc_chunk_invariant(a, y)));
	M0_PRE(ergo(x != NULL && y != NULL, (char *) x < (char *) y));
	M0_PRE(ergo(x != NULL, x->bac_free) || ergo(y != NULL, y->bac_free));
	if (x != NULL && y != NULL && x->bac_free && y->bac_free) {
		y_size_total = sizeof *y + y->bac_size;
		be_alloc_chunk_del_fini(a, ztype, tx, y);
		be_alloc_chunk_resize(a, ztype, tx, x,
				      x->bac_size + y_size_total);
		chunks_were_merged = true;
	}
	M0_POST(ergo(x != NULL, be_alloc_chunk_invariant(a, x)));
	M0_POST(ergo(y != NULL && !chunks_were_merged,
		     be_alloc_chunk_invariant(a, y)));
	return chunks_were_merged;
}

M0_INTERNAL int m0_be_allocator_init(struct m0_be_allocator *a,
				     struct m0_be_seg *seg)
{
	struct m0_be_seg_hdr *seg_hdr;
	int                   i;

	M0_ENTRY("a=%p seg=%p seg->bs_addr=%p seg->bs_size=%lu",
		 a, seg, seg->bs_addr, seg->bs_size);

	M0_PRE(m0_be_seg__invariant(seg));

	m0_mutex_init(&a->ba_lock);

	a->ba_seg = seg;
	seg_hdr = (struct m0_be_seg_hdr *)seg->bs_addr;
	for (i = 0; i < M0_BAP_NR; ++i) {
		a->ba_h[i] = &seg_hdr->bh_alloc[i];
		M0_ASSERT(m0_addr_is_aligned(a->ba_h[i],
					     BE_ALLOC_HEADER_SHIFT));
	}

	return 0;
}

M0_INTERNAL void m0_be_allocator_fini(struct m0_be_allocator *a)
{
	int i;

	M0_ENTRY("a=%p", a);

	for (i = 0; i < M0_BAP_NR; ++i)
		be_allocator_stats_print(&a->ba_h[i]->bah_stats);
	m0_mutex_fini(&a->ba_lock);

	M0_LEAVE();
}

M0_INTERNAL bool m0_be_allocator__invariant(struct m0_be_allocator *a)
{
	return m0_mutex_is_locked(&a->ba_lock) &&
	       (true || /* XXX Disabled as it's too slow. */
		m0_forall(z, M0_BAP_NR,
			  m0_be_list_forall(chunks_all, iter,
					   &a->ba_h[z]->bah_chunks,
					   be_alloc_chunk_invariant(a, iter))));
}

static int be_allocator_header_create(struct m0_be_allocator     *a,
				      enum m0_be_alloc_zone_type  ztype,
				      struct m0_be_tx            *tx,
				      uintptr_t                   offset,
				      m0_bcount_t                 size)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];
	struct be_alloc_chunk         *c;

	M0_PRE(ztype < M0_BAP_NR);

	if (size != 0 && size < sizeof *c + 1)
		return M0_ERR(-ENOSPC);

	h->bah_addr = (void *)offset;
	h->bah_size = size;
	M0_BE_TX_CAPTURE_PTR(a->ba_seg, tx, &h->bah_addr);
	M0_BE_TX_CAPTURE_PTR(a->ba_seg, tx, &h->bah_size);

	chunks_all_be_list_create(&h->bah_chunks, tx);
	m0_be_fl_create(&h->bah_fl, tx, a->ba_seg);
	be_allocator_stats_init(&h->bah_stats, h);
	be_allocator_stats_capture(a, ztype, tx);

	/* init main chunk */
	if (size != 0) {
		c = be_alloc_chunk_add_after(a, ztype, tx, NULL, 0, size, true);
		M0_ASSERT(c != NULL);
	}
	return 0;
}

static void be_allocator_header_destroy(struct m0_be_allocator     *a,
					enum m0_be_alloc_zone_type  ztype,
					struct m0_be_tx            *tx)
{
	struct m0_be_allocator_header *h = a->ba_h[ztype];
	struct be_alloc_chunk         *c;

	/*
	 * We destroy allocator when all objects are de-allocated. Therefore,
	 * bah_chunks contains only 1 element. The list is empty for an unused
	 * zone (bah_size == 0).
	 */
	c = chunks_all_be_list_head(&h->bah_chunks);
	M0_ASSERT(equi(c == NULL, h->bah_size == 0));
	if (c != NULL)
		be_alloc_chunk_del_fini(a, ztype, tx, c);

	m0_be_fl_destroy(&h->bah_fl, tx);
	chunks_all_be_list_destroy(&h->bah_chunks, tx);
}

M0_INTERNAL int m0_be_allocator_create(struct m0_be_allocator *a,
				       struct m0_be_tx        *tx,
				       uint32_t               *zone_percent,
				       uint32_t                zones_nr)
{
	m0_bcount_t reserved;
	m0_bcount_t free_space;
	m0_bcount_t remain;
	m0_bcount_t size;
	uintptr_t   offset;
	int         i;
	int         z;
	int         rc;

	M0_ENTRY("a=%p tx=%p", a, tx);
	M0_PRE(zones_nr <= M0_BAP_NR);
	M0_PRE(m0_reduce(i, zones_nr, 0ULL, + zone_percent[i]) == 100);

	reserved   = m0_be_seg_reserved(a->ba_seg);
	free_space = a->ba_seg->bs_size - reserved;
	offset     = (uintptr_t)a->ba_seg->bs_addr + reserved;

	m0_mutex_lock(&a->ba_lock);

	remain = free_space;
	for (i = 0; i < zones_nr; ++i) {
		if (i < zones_nr - 1) {
			size = free_space * zone_percent[i] / 100;
			size = m0_align(size, 1UL << BE_ALLOC_ZONE_SIZE_SHIFT);
		} else
			size = remain;
		M0_ASSERT(size <= remain);
		rc = be_allocator_header_create(a, i, tx, offset, size);
		if (rc != 0) {
			for (z = 0; z < i; ++z)
				be_allocator_header_destroy(a, z, tx);
			m0_mutex_unlock(&a->ba_lock);
			return M0_RC(rc);
		}
		remain -= size;
		offset += size;
	}
	M0_ASSERT(remain == 0);

	/* Create the rest of zones as empty/unused. */
	for (i = zones_nr; i < M0_BAP_NR; ++i) {
		rc = be_allocator_header_create(a, i, tx, 0, 0);
		M0_ASSERT(rc == 0);
	}

	M0_LOG(M0_DEBUG, "free_space=%"PRIu64, free_space);
	for (i = 0; i < zones_nr; ++i)
		M0_LOG(M0_DEBUG, "%s zone size=%"PRIu64,
		       be_alloc_zone_name(i), a->ba_h[i]->bah_size);

	M0_POST(m0_be_allocator__invariant(a));
	m0_mutex_unlock(&a->ba_lock);

	M0_LEAVE();
	return 0;
}

M0_INTERNAL void m0_be_allocator_destroy(struct m0_be_allocator *a,
					 struct m0_be_tx *tx)
{
	int z;

	M0_ENTRY("a=%p tx=%p", a, tx);

	m0_mutex_lock(&a->ba_lock);
	M0_PRE_EX(m0_be_allocator__invariant(a));

	for (z = 0; z < M0_BAP_NR; ++z)
		be_allocator_header_destroy(a, z, tx);

	m0_mutex_unlock(&a->ba_lock);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_allocator_credit(struct m0_be_allocator *a,
					enum m0_be_allocator_op optype,
					m0_bcount_t             size,
					unsigned                shift,
					struct m0_be_tx_credit *accum)
{
	/*
	 * XXX `a' can be NULL, &(struct m0_be_seg)(0)->bs_allocator or
	 * uninitialised value. Use h == NULL to avoid dereferencing in the
	 * above cases.
	 */
	struct m0_be_allocator_header *h = NULL;
	struct m0_be_tx_credit         cred_list_create = {};
	struct m0_be_tx_credit         cred_list_destroy = {};
	struct m0_be_tx_credit         chunk_add_after_credit;
	struct m0_be_tx_credit         chunk_del_fini_credit;
	struct m0_be_tx_credit         chunk_trymerge_credit = {};
	struct m0_be_tx_credit         chunk_resize_credit = {};
	struct m0_be_tx_credit         tryadd_free_after_credit;
	struct m0_be_tx_credit         cred_mark_free = {};
	struct m0_be_tx_credit         cred_split = {};
	struct m0_be_tx_credit         mem_zero_credit;
	struct m0_be_tx_credit         chunk_credit;
	struct m0_be_tx_credit         cred_allocator = {};
	struct m0_be_tx_credit         cred_free_flag;
	struct m0_be_tx_credit         cred_chunk_size;
	struct m0_be_tx_credit         stats_credit;
	struct m0_be_tx_credit         tmp;
	struct be_alloc_chunk          chunk;

	chunk_credit    = M0_BE_TX_CREDIT_TYPE(struct be_alloc_chunk);
	cred_free_flag  = M0_BE_TX_CREDIT_PTR(&chunk.bac_free);
	cred_chunk_size = M0_BE_TX_CREDIT_PTR(&chunk.bac_size);
	stats_credit    = M0_BE_TX_CREDIT_PTR(&h->bah_stats);

	m0_be_tx_credit_add(&cred_allocator,
			    &M0_BE_TX_CREDIT_PTR(&h->bah_size));
	m0_be_tx_credit_add(&cred_allocator,
			    &M0_BE_TX_CREDIT_PTR(&h->bah_addr));

	shift = max_check(shift, (unsigned) M0_BE_ALLOC_SHIFT_MIN);
	mem_zero_credit = M0_BE_TX_CREDIT(1, size);

	chunks_all_be_list_credit(M0_BLO_CREATE,  1, &cred_list_create);
	chunks_all_be_list_credit(M0_BLO_DESTROY, 1, &cred_list_destroy);

	tmp = M0_BE_TX_CREDIT(0, 0);
	chunks_all_be_list_credit(M0_BLO_TLINK_CREATE, 1, &tmp);
	m0_be_tx_credit_add(&tmp, &chunk_credit);
	chunks_all_be_list_credit(M0_BLO_ADD,          1, &tmp);
	m0_be_fl_credit(&h->bah_fl, M0_BFL_ADD, &tmp);
	chunk_add_after_credit = tmp;

	tmp = M0_BE_TX_CREDIT(0, 0);
	chunks_all_be_list_credit(M0_BLO_DEL,           1, &tmp);
	chunks_all_be_list_credit(M0_BLO_TLINK_DESTROY, 1, &tmp);
	m0_be_fl_credit(&h->bah_fl, M0_BFL_DEL, &tmp);
	chunk_del_fini_credit = tmp;

	m0_be_fl_credit(&h->bah_fl, M0_BFL_DEL, &chunk_resize_credit);
	m0_be_fl_credit(&h->bah_fl, M0_BFL_ADD, &chunk_resize_credit);
	m0_be_tx_credit_add(&chunk_resize_credit, &cred_chunk_size);

	m0_be_tx_credit_add(&chunk_trymerge_credit, &chunk_del_fini_credit);
	m0_be_tx_credit_add(&chunk_trymerge_credit, &chunk_resize_credit);

	m0_be_tx_credit_max(&tryadd_free_after_credit,
			    &chunk_resize_credit,
			    &chunk_add_after_credit);

	m0_be_tx_credit_add(&cred_split, &chunk_del_fini_credit);
	m0_be_tx_credit_mac(&cred_split, &tryadd_free_after_credit, 2);
	m0_be_tx_credit_mac(&cred_split, &chunk_add_after_credit, 1);

	m0_be_tx_credit_add(&cred_mark_free, &cred_free_flag);
	m0_be_fl_credit(&h->bah_fl, M0_BFL_ADD, &cred_mark_free);

	switch (optype) {
		case M0_BAO_CREATE:
			tmp = M0_BE_TX_CREDIT(0, 0);
			m0_be_tx_credit_mac(&tmp, &cred_list_create, 2);
			m0_be_fl_credit(&h->bah_fl, M0_BFL_CREATE, &tmp);
			m0_be_tx_credit_add(&tmp, &chunk_add_after_credit);
			m0_be_tx_credit_add(&tmp, &cred_allocator);
			m0_be_tx_credit_add(&tmp, &stats_credit);
			m0_be_tx_credit_mac(accum, &tmp, M0_BAP_NR);
			break;
		case M0_BAO_DESTROY:
			tmp = M0_BE_TX_CREDIT(0, 0);
			m0_be_fl_credit(&h->bah_fl, M0_BFL_DESTROY, &tmp);
			m0_be_tx_credit_add(&tmp, &chunk_del_fini_credit);
			m0_be_tx_credit_mac(&tmp, &cred_list_destroy, 2);
			m0_be_tx_credit_mac(accum, &tmp, M0_BAP_NR);
			break;
		case M0_BAO_ALLOC_ALIGNED:
			m0_be_tx_credit_add(accum, &cred_split);
			m0_be_tx_credit_add(accum, &mem_zero_credit);
			m0_be_tx_credit_add(accum, &stats_credit);
			break;
		case M0_BAO_ALLOC:
			m0_be_allocator_credit(a, M0_BAO_ALLOC_ALIGNED, size,
					       M0_BE_ALLOC_SHIFT_MIN, accum);
			break;
		case M0_BAO_FREE_ALIGNED:
			m0_be_tx_credit_add(accum, &cred_mark_free);
			m0_be_tx_credit_mac(accum, &chunk_trymerge_credit, 2);
			m0_be_tx_credit_add(accum, &stats_credit);
			break;
		case M0_BAO_FREE:
			m0_be_allocator_credit(a, M0_BAO_FREE_ALIGNED, size,
					       shift, accum);
			break;
		default:
			M0_IMPOSSIBLE("Invalid allocator operation type");
	}
	/* XXX FIXME ASAP workaround for allocator/btree/etc. credit bugs */
	/* 640 bytes ought to be enough for anybody */
	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(40, 640));
}

M0_INTERNAL void m0_be_alloc_aligned(struct m0_be_allocator *a,
				     struct m0_be_tx *tx,
				     struct m0_be_op *op,
				     void **ptr,
				     m0_bcount_t size,
				     unsigned shift,
				     uint64_t zonemask)
{
	enum  m0_be_alloc_zone_type  ztype;
	struct be_alloc_chunk       *c = NULL;
	m0_bcount_t                  size_to_pick;
	int                          z;

	shift = max_check(shift, (unsigned) M0_BE_ALLOC_SHIFT_MIN);
	M0_ASSERT_INFO(size <= (M0_BCOUNT_MAX - (1UL << shift)) / 2,
		       "size=%lu", size);

	m0_be_op_active(op);

	m0_mutex_lock(&a->ba_lock);
	M0_PRE_EX(m0_be_allocator__invariant(a));

	/* algorithm starts here */
	size_to_pick = (1UL << shift) - (1UL << M0_BE_ALLOC_SHIFT_MIN) +
		       m0_align(size, 1UL << M0_BE_ALLOC_SHIFT_MIN);
	for (z = 0; z < M0_BAP_NR; ++z) {
		if ((zonemask & M0_BITS(z)) != 0)
			c = m0_be_fl_pick(&a->ba_h[z]->bah_fl, size_to_pick);
		if (c != NULL)
			break;
	}
	/* XXX If allocation fails then stats are updated for normal zone. */
	ztype = c != NULL ? z : M0_BAP_NORMAL;
	if (c != NULL) {
		c = be_alloc_chunk_trysplit(a, ztype, tx, c, size, shift);
		M0_ASSERT(c != NULL);
		M0_ASSERT(c->bac_zone == ztype);
		memset(&c->bac_mem, 0, size);
		m0_be_tx_capture(tx, &M0_BE_REG(a->ba_seg, size, &c->bac_mem));
	}
	*ptr = c == NULL ? NULL : &c->bac_mem;
	be_allocator_stats_update(&a->ba_h[ztype]->bah_stats,
				  c == NULL ? size : c->bac_size, true, c == 0);
	be_allocator_stats_capture(a, ztype, tx);
	/* and ends here */

	M0_LOG(M0_DEBUG, "allocator=%p size=%lu shift=%u "
	       "c=%p c->bac_size=%lu ptr=%p", a, size, shift, c,
	       c == NULL ? 0 : c->bac_size, *ptr);
	if (*ptr == NULL)
		be_allocator_stats_print(&a->ba_h[ztype]->bah_stats);

	if (c != NULL) {
		M0_POST(!c->bac_free);
		M0_POST(c->bac_size >= size);
		M0_POST(m0_addr_is_aligned(&c->bac_mem, shift));
		M0_POST(be_alloc_chunk_is_in(a, ztype, c));
	}
	/*
	 * unlock mutex after post-conditions which are using allocator
	 * internals
	 */
	M0_POST_EX(m0_be_allocator__invariant(a));
	m0_mutex_unlock(&a->ba_lock);

	/* set op state after post-conditions because they are using op */
	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_alloc(struct m0_be_allocator *a,
			     struct m0_be_tx *tx,
			     struct m0_be_op *op,
			     void **ptr,
			     m0_bcount_t size)
{
	m0_be_alloc_aligned(a, tx, op, ptr, size, M0_BE_ALLOC_SHIFT_MIN,
			    M0_BITS(M0_BAP_NORMAL));
}

M0_INTERNAL void m0_be_free_aligned(struct m0_be_allocator *a,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    void *ptr)
{
	enum m0_be_alloc_zone_type ztype;
	struct be_alloc_chunk   *c;
	struct be_alloc_chunk   *prev;
	struct be_alloc_chunk   *next;
	bool		         chunks_were_merged;

	M0_PRE(ergo(ptr != NULL,
		    m0_reduce(z, M0_BAP_NR, 0,
			      +(int)be_alloc_mem_is_in(a, z, ptr, 1)) == 1));

	m0_be_op_active(op);

	if (ptr != NULL) {
		m0_mutex_lock(&a->ba_lock);
		M0_PRE_EX(m0_be_allocator__invariant(a));

		c = be_alloc_chunk_addr(ptr);
		M0_PRE(be_alloc_chunk_invariant(a, c));
		M0_PRE(!c->bac_free);
		ztype = c->bac_zone;
		M0_LOG(M0_DEBUG, "allocator=%p c=%p c->bac_size=%lu zone=%d "
		       "data=%p", a, c, c->bac_size, c->bac_zone, &c->bac_mem);
		/* algorithm starts here */
		be_alloc_chunk_mark_free(a, ztype, tx, c);
		prev = be_alloc_chunk_prev(a, ztype, c);
		next = be_alloc_chunk_next(a, ztype, c);
		chunks_were_merged = be_alloc_chunk_trymerge(a, ztype, tx,
							     prev, c);
		if (chunks_were_merged)
			c = prev;
		be_alloc_chunk_trymerge(a, ztype, tx, c, next);
		be_allocator_stats_update(&a->ba_h[ztype]->bah_stats,
					  c->bac_size, false, false);
		be_allocator_stats_capture(a, ztype, tx);
		/* and ends here */
		M0_POST(c->bac_free);
		M0_POST(c->bac_size > 0);
		M0_POST(be_alloc_chunk_invariant(a, c));

		M0_POST_EX(m0_be_allocator__invariant(a));
		m0_mutex_unlock(&a->ba_lock);
	}

	m0_be_op_done(op);

}

M0_INTERNAL void m0_be_free(struct m0_be_allocator *a,
			    struct m0_be_tx *tx,
			    struct m0_be_op *op,
			    void *ptr)
{
	m0_be_free_aligned(a, tx, op, ptr);
}

M0_INTERNAL void m0_be_alloc_stats(struct m0_be_allocator *a,
				   struct m0_be_allocator_stats *out)
{
	m0_mutex_lock(&a->ba_lock);
	M0_PRE_EX(m0_be_allocator__invariant(a));
	*out = a->ba_h[M0_BAP_NORMAL]->bah_stats;
	m0_mutex_unlock(&a->ba_lock);
}

M0_INTERNAL void m0_be_alloc_stats_credit(struct m0_be_allocator *a,
                                          struct m0_be_tx_credit *accum)
{
	m0_be_tx_credit_add(accum,
		&M0_BE_TX_CREDIT_PTR(&a->ba_h[M0_BAP_NORMAL]->bah_stats));
}

M0_INTERNAL void m0_be_alloc_stats_capture(struct m0_be_allocator *a,
                                           struct m0_be_tx        *tx)
{
	if (tx != NULL) {
		m0_be_tx_capture(tx, &M0_BE_REG_PTR(a->ba_seg,
					&a->ba_h[M0_BAP_NORMAL]->bah_stats));
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
