/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 02/18/2011
 */

#include "lib/bitmap.h"
#include "lib/misc.h"   /* M0_SET0 */
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/memory.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @defgroup bitmap Bitmap
   @{
 */

/**
   Number of bits in a word (m0_bitmap.b_words).
   And the number of bits to shift to convert bit offset to word index.
 */
enum {
	M0_BITMAP_BITS = (8 * sizeof ((struct m0_bitmap *)0)->b_words[0]),
	M0_BITMAP_BITSHIFT = 6
};

/*
  Note that the following assertion validates both the relationship between
  M0_BITMAP_BITS and M0_BITMAP_BITSHIFT, and that M0_BITMAP_BITS is
  a power of 2.
*/
M0_BASSERT(M0_BITMAP_BITS == (1UL << M0_BITMAP_BITSHIFT));

/**
   Number of words needed to be allocated to store nr bits.  This is
   an allocation macro.  For indexing, M0_BITMAP_SHIFT is used.

   @param nr number of bits to be allocated
 */
#define M0_BITMAP_WORDS(nr) (((nr) + (M0_BITMAP_BITS-1)) >> M0_BITMAP_BITSHIFT)

/* verify the bounds of size macro */
M0_BASSERT(M0_BITMAP_WORDS(0) == 0);
M0_BASSERT(M0_BITMAP_WORDS(1) == 1);
M0_BASSERT(M0_BITMAP_WORDS(63) == 1);
M0_BASSERT(M0_BITMAP_WORDS(64) == 1);
M0_BASSERT(M0_BITMAP_WORDS(65) == 2);

/**
   Shift a m0_bitmap bit index to get the word index.
   Use M0_BITMAP_SHIFT() to select the correct word, then use M0_BITMAP_MASK()
   to access the individual bit within that word.

   @param idx bit offset into the bitmap
 */
#define M0_BITMAP_SHIFT(idx) ((idx) >> M0_BITMAP_BITSHIFT)

/**
   Mask off a single bit within a word.
   Use M0_BITMAP_SHIFT() to select the correct word, then use M0_BITMAP_MASK()
   to access the individual bit within that word.

   @param idx bit offset into the bitmap
 */
#define M0_BITMAP_MASK(idx) (1UL << ((idx) & (M0_BITMAP_BITS-1)))

M0_INTERNAL int m0_bitmap_init(struct m0_bitmap *map, size_t nr)
{
	M0_ALLOC_ARR(map->b_words, M0_BITMAP_WORDS(nr));
	if (map->b_words == NULL)
		return M0_ERR(-ENOMEM);
	map->b_nr = nr;

	return 0;
}
M0_EXPORTED(m0_bitmap_init);

M0_INTERNAL void m0_bitmap_fini(struct m0_bitmap *map)
{
	M0_ASSERT(map->b_words != NULL);
	m0_free(map->b_words);
	M0_SET0(map);
}
M0_EXPORTED(m0_bitmap_fini);

M0_INTERNAL bool m0_bitmap_get(const struct m0_bitmap *map, size_t idx)
{
	M0_PRE(idx < map->b_nr && map->b_words != NULL);
	return map->b_words[M0_BITMAP_SHIFT(idx)] & M0_BITMAP_MASK(idx);
}
M0_EXPORTED(m0_bitmap_get);

M0_INTERNAL size_t m0_bitmap_ffz(const struct m0_bitmap *map)
{
	size_t idx;

	/* use linux find_first_zero_bit() ? */
	for (idx = 0; idx < map->b_nr; idx++) {
		if (!m0_bitmap_get(map, idx))
			return idx;
	}
	return (size_t)-1;
}
M0_EXPORTED(m0_bitmap_ffz);

M0_INTERNAL void m0_bitmap_set(struct m0_bitmap *map, size_t idx, bool val)
{
	M0_ASSERT(idx < map->b_nr && map->b_words != NULL);
	if (val)
		map->b_words[M0_BITMAP_SHIFT(idx)] |= M0_BITMAP_MASK(idx);
	else
		map->b_words[M0_BITMAP_SHIFT(idx)] &= ~M0_BITMAP_MASK(idx);
}
M0_EXPORTED(m0_bitmap_set);

M0_INTERNAL void m0_bitmap_copy(struct m0_bitmap *dst,
				const struct m0_bitmap *src)
{
	int s = M0_BITMAP_WORDS(src->b_nr);
	int d = M0_BITMAP_WORDS(dst->b_nr);

	M0_PRE(dst->b_nr >= src->b_nr &&
	       src->b_words != NULL && dst->b_words != NULL);

	memcpy(dst->b_words, src->b_words, s * sizeof src->b_words[0]);
	if (d > s)
		memset(&dst->b_words[s], 0, (d - s) * sizeof dst->b_words[0]);
}

M0_INTERNAL size_t m0_bitmap_set_nr(const struct m0_bitmap *map)
{
	size_t i;
	size_t nr;
	M0_PRE(map != NULL);
	for (nr = 0, i = 0; i < map->b_nr; ++i)
		nr += m0_bitmap_get(map, i);
	return nr;
}

M0_INTERNAL int m0_bitmap_onwire_init(struct m0_bitmap_onwire *ow_map, size_t nr)
{
	ow_map->bo_size = M0_BITMAP_WORDS(nr);
	M0_ALLOC_ARR(ow_map->bo_words, M0_BITMAP_WORDS(nr));
	if (ow_map->bo_words == NULL)
		return M0_ERR(-ENOMEM);

	return 0;
}

M0_INTERNAL void m0_bitmap_onwire_fini(struct m0_bitmap_onwire *ow_map)
{
	M0_PRE(ow_map != NULL);

	m0_free(ow_map->bo_words);
	M0_SET0(ow_map);
}

M0_INTERNAL void m0_bitmap_store(const struct m0_bitmap *im_map,
                                 struct m0_bitmap_onwire *ow_map)
{
	size_t s = M0_BITMAP_WORDS(im_map->b_nr);

	M0_PRE(im_map != NULL && ow_map != NULL);
	M0_PRE(im_map->b_words != NULL);
	M0_PRE(s == ow_map->bo_size);

	memcpy(ow_map->bo_words, im_map->b_words,
	       s * sizeof im_map->b_words[0]);
}

M0_INTERNAL void m0_bitmap_load(const struct m0_bitmap_onwire *ow_map,
                                 struct m0_bitmap *im_map)
{
	M0_PRE(ow_map != NULL && im_map != NULL);
	M0_PRE(M0_BITMAP_WORDS(im_map->b_nr) == ow_map->bo_size);

	/* copy onwire bitmap words to in-memory bitmap words. */
	memcpy(im_map->b_words, ow_map->bo_words,
	       ow_map->bo_size * sizeof ow_map->bo_words[0]);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of bitmap group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
