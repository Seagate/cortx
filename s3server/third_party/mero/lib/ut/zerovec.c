/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 * Original creation date: 09/27/2011
 */

#include "ut/ut.h"
#include "lib/vec.h"
#include "lib/memory.h"
#include "lib/arith.h"
#include "lib/errno.h"     /* ENOENT */

#ifdef __KERNEL__
#include <linux/pagemap.h> /* PAGE_SIZE */
#endif

enum ZEROVEC_UT_VALUES {
	ZEROVEC_UT_SEG_SIZE = M0_0VEC_ALIGN,
	ZEROVEC_UT_SEGS_NR = 10,
};

static m0_bindex_t indices[ZEROVEC_UT_SEGS_NR];

static void zerovec_init(struct m0_0vec *zvec, const m0_bcount_t seg_size)
{
	int rc;

	rc = m0_0vec_init(zvec, ZEROVEC_UT_SEGS_NR);
	M0_UT_ASSERT(rc == 0);
}

#ifndef __KERNEL__

static m0_bcount_t counts[ZEROVEC_UT_SEGS_NR];

static void zerovec_init_bvec(void)
{
	uint32_t		i;
	struct m0_0vec		zvec;
	struct m0_bufvec	bufvec;

	zerovec_init(&zvec, ZEROVEC_UT_SEG_SIZE);

	/* Have to manually allocate buffers for m0_bufvec that are
	 aligned on 4k boundary. */
	bufvec.ov_vec.v_nr = ZEROVEC_UT_SEGS_NR;
	M0_ALLOC_ARR(bufvec.ov_vec.v_count, ZEROVEC_UT_SEGS_NR);
	M0_UT_ASSERT(bufvec.ov_vec.v_count != NULL);
	M0_ALLOC_ARR(bufvec.ov_buf, ZEROVEC_UT_SEGS_NR);
	M0_UT_ASSERT(bufvec.ov_buf != NULL);

	for (i = 0; i < bufvec.ov_vec.v_nr; ++i) {
		bufvec.ov_buf[i] = m0_alloc_aligned(ZEROVEC_UT_SEG_SIZE,
						    M0_0VEC_SHIFT);
		M0_UT_ASSERT(bufvec.ov_buf[i] != NULL);
		bufvec.ov_vec.v_count[i] = ZEROVEC_UT_SEG_SIZE;
	}

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i)
		indices[i] = i;

	m0_0vec_bvec_init(&zvec, &bufvec, indices);

	/* Checks if buffer array, index array and segment count array
	   are populated correctly. */
	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		M0_UT_ASSERT(zvec.z_bvec.ov_buf[i] == bufvec.ov_buf[i]);
		M0_UT_ASSERT(zvec.z_bvec.ov_vec.v_count[i] ==
			     ZEROVEC_UT_SEG_SIZE);
		M0_UT_ASSERT(zvec.z_index[i] == indices[i]);
	}

	m0_bufvec_free(&bufvec);
	m0_0vec_fini(&zvec);
}

static void zerovec_init_bufs(void)
{
	char		**bufs;
	uint32_t	  i;
	uint64_t	  seed;
	struct m0_0vec	  zvec;

	seed = 0;
	zerovec_init(&zvec, ZEROVEC_UT_SEG_SIZE);

	M0_ALLOC_ARR(bufs, ZEROVEC_UT_SEGS_NR);
	M0_UT_ASSERT(bufs != NULL);

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		bufs[i] = m0_alloc_aligned(ZEROVEC_UT_SEG_SIZE, M0_0VEC_SHIFT);
		M0_UT_ASSERT(bufs[i] != NULL);
		counts[i] = ZEROVEC_UT_SEG_SIZE;
		indices[i] = m0_rnd(ZEROVEC_UT_SEGS_NR, &seed);
	}

	m0_0vec_bufs_init(&zvec, (void**)bufs, indices, counts,
			  ZEROVEC_UT_SEGS_NR);

	/* Checks if buffer array, index array and segment count array
	   are populated correctly. */
	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		M0_UT_ASSERT(zvec.z_index[i] == indices[i]);
		M0_UT_ASSERT(zvec.z_bvec.ov_vec.v_count[i] == counts[i]);
		M0_UT_ASSERT(zvec.z_bvec.ov_buf[i] == bufs[i]);
	}
	M0_UT_ASSERT(zvec.z_bvec.ov_vec.v_nr == ZEROVEC_UT_SEGS_NR);

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i)
		m0_free(bufs[i]);
	m0_free(bufs);
	m0_0vec_fini(&zvec);
}

static void zerovec_init_cbuf(void)
{
	int		i;
	int		rc;
	uint64_t	seed;
	struct m0_buf	bufs[ZEROVEC_UT_SEGS_NR];
	struct m0_0vec	zvec;

	seed = 0;
	zerovec_init(&zvec, ZEROVEC_UT_SEG_SIZE);

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		bufs[i].b_addr = m0_alloc_aligned(ZEROVEC_UT_SEG_SIZE,
						  M0_0VEC_SHIFT);
		M0_UT_ASSERT(bufs[i].b_addr != NULL);
		bufs[i].b_nob = ZEROVEC_UT_SEG_SIZE;
		indices[i] = m0_rnd(ZEROVEC_UT_SEGS_NR, &seed);

		rc = m0_0vec_cbuf_add(&zvec, &bufs[i], &indices[i]);
		M0_UT_ASSERT(rc == 0);
	}

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		M0_UT_ASSERT(zvec.z_index[i] == indices[i]);
		M0_UT_ASSERT(zvec.z_bvec.ov_buf[i] == bufs[i].b_addr);
		M0_UT_ASSERT(zvec.z_bvec.ov_vec.v_count[i] == bufs[i].b_nob);
	}

	/* Tries to add more buffers beyond zerovec's capacity. Should fail. */
	rc = m0_0vec_cbuf_add(&zvec, &bufs[0], &indices[0]);
	M0_UT_ASSERT(rc == -EMSGSIZE);

	M0_UT_ASSERT(zvec.z_bvec.ov_vec.v_nr == ZEROVEC_UT_SEGS_NR);

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i)
		m0_free(bufs[i].b_addr);
	m0_0vec_fini(&zvec);
}
#else

static void zerovec_init_pages(void)
{
	int		rc;
	uint32_t	i;
	uint64_t	seed;
	struct page	**pages;
	struct m0_0vec	zvec;

	seed = 0;
	zerovec_init(&zvec, PAGE_SIZE);

	M0_ALLOC_ARR(pages, ZEROVEC_UT_SEGS_NR);
	M0_UT_ASSERT(pages != NULL);
	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		pages[i] = alloc_page(GFP_KERNEL);
		M0_UT_ASSERT(pages[i] != NULL);
	}

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		indices[i] = m0_rnd(ZEROVEC_UT_SEGS_NR, &seed);
		rc = m0_0vec_page_add(&zvec, pages[i], indices[i]);
		M0_UT_ASSERT(rc == 0);
	}

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i) {
		M0_UT_ASSERT(zvec.z_index[i] == indices[i]);
		M0_UT_ASSERT(zvec.z_bvec.ov_buf[i] ==
			     page_address(pages[i]));
		M0_UT_ASSERT(zvec.z_bvec.ov_vec.v_count[i] == PAGE_SIZE);
	}

	M0_UT_ASSERT(zvec.z_bvec.ov_vec.v_nr == ZEROVEC_UT_SEGS_NR);

	/* Tries to add more pages beyond zerovec's capacity. Should fail. */
	rc = m0_0vec_page_add(&zvec, pages[0], indices[0]);
	M0_UT_ASSERT(rc == -EMSGSIZE);

	for (i = 0; i < ZEROVEC_UT_SEGS_NR; ++i)
		__free_page(pages[i]);
	m0_free(pages);
	m0_0vec_fini(&zvec);
}
#endif

void test_zerovec(void)
{
#ifndef __KERNEL__
	/* Populate the zero vector using a m0_bufvec structure. */
	zerovec_init_bvec();

	/* Populate the zero vector using array of buffers, indices
	   and counts. */
	zerovec_init_bufs();

	/* Populate the zero vector using a m0_buf structure and
	   array of indices. */
	zerovec_init_cbuf();
#else
	/* Populate the zero vector using a page. */
	zerovec_init_pages();
#endif
}
