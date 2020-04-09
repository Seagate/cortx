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
 * Original creation date: 28-May-2013
 */

#pragma once
#ifndef __MERO_BE_SEG_H__
#define __MERO_BE_SEG_H__

#include "be/alloc.h"           /* m0_be_allocator */
#include "be/seg_dict.h"        /* m0_be_seg_dict_init */       /* XXX */

#include "lib/tlist.h"          /* m0_tlink */
#include "lib/types.h"          /* m0_bcount_t */

struct m0_be_op;
struct m0_be_reg_d;
struct m0_stob;
struct m0_stob_id;

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

enum m0_be_seg_states {
	M0_BSS_INIT,
	M0_BSS_OPENED,
	M0_BSS_CLOSED,
};

enum {
	M0_BE_SEG_HEADER_OFFSET = 0ULL,
	/** Maximum size for segment I/O while reading in m0_be_seg_open(). */
	M0_BE_SEG_READ_SIZE_MAX = 1ULL << 26,
	/** Core dump only first given MBs of the segment. */
	M0_BE_SEG_CORE_DUMP_LIMIT = 64ULL << 20,
	/** Fake segment id, used before m0_be_seg_create_multiple is used
	 * everywhere around the code */
	M0_BE_SEG_FAKE_ID = ~0,
	/** Segments' addr, size, offset has to be aligned by this boundary */
	M0_BE_SEG_PAGE_SIZE = 1ULL << 12,
};

#define M0_BE_SEG_PG_PRESENT       0x8000000000000000ULL
#define M0_BE_SEG_PG_PIN_CNT_MASK  (~M0_BE_SEG_PG_PRESENT)

struct m0_be_seg {
	uint64_t               bs_id;
	struct m0_stob        *bs_stob;
	m0_bcount_t            bs_size;
	m0_bcount_t            bs_offset;
	void                  *bs_addr;
	/** Size at the start of segment which is used by segment internals. */
	/** XXX use it in all UTs */
	m0_bcount_t            bs_reserved;
	/**
	 * Segment allocator.
	 * m0_be_seg_allocator() should be used to access segment allocator.
	 */
	struct m0_be_allocator bs_allocator;
	struct m0_be_domain   *bs_domain;
	int                    bs_state;
	uint64_t               bs_magic;
	struct m0_tlink        bs_linkage;
};

/* helper for m0_be_seg__create_multiple() */
struct m0_be_seg_geom {
	m0_bcount_t            sg_size;
	void                  *sg_addr;
	m0_bcount_t            sg_offset;
	uint64_t               sg_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/* this is passed as the last element inside @geom of
 * m0_be_seg_create_multiple() */
#define M0_BE_SEG_GEOM0                         \
	((struct m0_be_seg_geom) {              \
		.sg_size = (0ULL),              \
		.sg_addr = (NULL),              \
		.sg_offset = (0ULL),            \
		.sg_id = (0ULL)                 \
	})

M0_INTERNAL bool m0_be_seg_geom_eq(const struct m0_be_seg_geom *left,
				   const struct m0_be_seg_geom *right);

M0_INTERNAL int m0_be_seg_create_multiple(struct m0_stob *stob,
					  const struct m0_be_seg_geom *geom);

M0_INTERNAL void m0_be_seg_init(struct m0_be_seg *seg,
				struct m0_stob *stob,
				struct m0_be_domain *dom,
				uint64_t seg_id);
M0_INTERNAL void m0_be_seg_fini(struct m0_be_seg *seg);
M0_INTERNAL bool m0_be_seg__invariant(const struct m0_be_seg *seg);

/** Opens existing stob, reads segment header from it, etc. */
M0_INTERNAL int m0_be_seg_open(struct m0_be_seg *seg);
M0_INTERNAL void m0_be_seg_close(struct m0_be_seg *seg);

/** Creates the segment of specified size on the storage. */
M0_INTERNAL int m0_be_seg_create(struct m0_be_seg *seg,
				 m0_bcount_t size,
				 void *addr);

M0_INTERNAL int m0_be_seg_destroy(struct m0_be_seg *seg);

M0_INTERNAL bool m0_be_seg_contains(const struct m0_be_seg *seg,
				    const void *addr);

M0_INTERNAL m0_bindex_t m0_be_seg_offset(const struct m0_be_seg *seg,
					 const void *addr);

/** XXX @todo s/bs_reserved/m0_be_seg_reserved/ everywhere */
M0_INTERNAL m0_bcount_t m0_be_seg_reserved(const struct m0_be_seg *seg);
M0_INTERNAL struct m0_be_allocator *m0_be_seg_allocator(struct m0_be_seg *seg);

struct m0_be_reg {
	struct m0_be_seg *br_seg;
	m0_bcount_t       br_size;
	void             *br_addr;
};

#define M0_BE_REG(seg, size, addr) \
	((struct m0_be_reg) {      \
		.br_seg  = (seg),  \
		.br_size = (size), \
		.br_addr = (addr) })

#define M0_BE_REG_PTR(seg, ptr)         M0_BE_REG((seg), sizeof *(ptr), (ptr))
#define M0_BE_REG_SEG(seg) M0_BE_REG((seg), (seg)->bs_size, (seg)->bs_addr)

M0_INTERNAL m0_bindex_t m0_be_reg_offset(const struct m0_be_reg *reg);

M0_INTERNAL bool m0_be_reg_eq(const struct m0_be_reg *r1,
			      const struct m0_be_reg *r2);

/** XXX @todo make m0_be_reg_copy_to(reg, dst_addr) and
 * m0_be_reg_copy_from(reg, src_addr) */

M0_INTERNAL bool m0_be_reg__invariant(const struct m0_be_reg *reg);

/*
 * XXX Synchronous operations.
 * m0_be_io and m0_be_op should be be added if necessary.
 */
M0_INTERNAL int m0_be_seg__read(struct m0_be_reg *reg, void *dst);
M0_INTERNAL int m0_be_seg__write(struct m0_be_reg *reg, void *src);
M0_INTERNAL int m0_be_reg__read(struct m0_be_reg *reg);
M0_INTERNAL int m0_be_reg__write(struct m0_be_reg *reg);

/**
 * Returns generation index for a region.
 *
 * It is guaranteed that subsequent call to this function with the same region
 * or region that intersects with the current region gives value greater
 * than previous.
 *
 * @todo add UT similar to libm0-ut:time.
 */
M0_INTERNAL unsigned long m0_be_reg_gen_idx(const struct m0_be_reg *reg);

M0_INTERNAL bool m0_be_seg_contains_stob(struct m0_be_seg        *seg,
                                         const struct m0_stob_id *stob_id);

/** @} end of be group */
#endif /* __MERO_BE_SEG_H__ */

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
