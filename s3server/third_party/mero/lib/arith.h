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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 05/08/2010
 */

#pragma once

#ifndef __MERO_LIB_ARITH_H__
#define __MERO_LIB_ARITH_H__

#include "lib/types.h"
#include "lib/assert.h"

/**
   @defgroup arith Miscellaneous arithmetic functions.
   @{
 */

static inline int32_t min32(int32_t a, int32_t b)
{
	return a < b ? a : b;
}

static inline int32_t max32(int32_t a, int32_t b)
{
	return a > b ? a : b;
}

static inline int64_t min64(int64_t a, int64_t b)
{
	return a < b ? a : b;
}

static inline int64_t max64(int64_t a, int64_t b)
{
	return a > b ? a : b;
}

static inline uint32_t min32u(uint32_t a, uint32_t b)
{
	return a < b ? a : b;
}

static inline uint32_t max32u(uint32_t a, uint32_t b)
{
	return a > b ? a : b;
}

static inline uint64_t min64u(uint64_t a, uint64_t b)
{
	return a < b ? a : b;
}

static inline uint64_t max64u(uint64_t a, uint64_t b)
{
	return a > b ? a : b;
}

#define min_type(t, a, b) ({			\
	t __a = (a);				\
	t __b = (b);				\
	__a < __b ? __a : __b;			\
})

#define max_type(t, a, b) ({			\
	t __a = (a);				\
	t __b = (b);				\
	__a > __b ? __a : __b;			\
})

#define min_check(a, b) ({			\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	(void)(&__a == &__b);			\
	__a < __b ? __a : __b;			\
})

#define max_check(a, b) ({			\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	(void)(&__a == &__b);			\
	__a > __b ? __a : __b;			\
})

#ifndef __KERNEL__
#  define min3(a, b, c) (min_check((a), min_check((b), (c))))
#  define max3(a, b, c) (max_check((a), max_check((b), (c))))
#endif

/**
   Compares two 64bit numbers "modulo overflow".

   This function returns true iff x0 == x1 + delta, where delta is a positive
   signed 64bit number (alternatively speaking, delta is an unsigned 64bit
   number less than UINT64_MAX/2).

   This function is useful for comparing rolling counters (like epoch numbers,
   times, log sequence numbers, etc.) which can overflow, but only "close
   enough" values ever get compared.

   As a safety measure, function checks that values are close enough.

   @see m0_mod_ge()
 */
M0_INTERNAL bool m0_mod_gt(uint64_t x0, uint64_t x1);

/**
   Compares two 64bit numbers "modulo overflow".

   @see m0_mod_gt()
 */
M0_INTERNAL bool m0_mod_ge(uint64_t x0, uint64_t x1);

static inline uint64_t m0_clip64u(uint64_t lo, uint64_t hi, uint64_t x)
{
	M0_PRE(lo <= hi);
	return min64u(max64u(lo, x), hi);
}

/**
   A very simple and fast re-entrant PRNG from Knuth.

   Generates a pseudo-random number using "seed" and stores the number back in
   seed. Result is no greater than max.

   @post result < max
 */
M0_INTERNAL uint64_t m0_rnd(uint64_t max, uint64_t * seed);
M0_INTERNAL uint64_t m0_rnd64(uint64_t *seed);

/**
   Greatest common divisor.
 */
M0_INTERNAL uint64_t m0_gcd64(uint64_t p, uint64_t q);

static inline bool m0_is_po2(uint64_t val)
{
	return (val & (val - 1)) == 0;
}

/**
 * @note This function will return 0 for val = 0.
 */
static inline unsigned m0_log2(uint64_t val)
{
	unsigned shift = 0;

	for (val >>= 1; val != 0; val >>= 1)
		++shift;
	return shift;
}

static inline uint64_t m0_align(uint64_t val, uint64_t alignment)
{
	uint64_t mask;

	M0_PRE(m0_is_po2(alignment));
	mask = alignment - 1;
	return (val + mask) & ~mask;
}

static inline bool m0_is_aligned(uint64_t val, uint64_t alignment)
{
	uint64_t mask;

	M0_PRE(m0_is_po2(alignment));
	mask = alignment - 1;
	return (val & mask) == 0;
}

/** True iff @val is a multiple of 8. This macro can be used to check that a
    pointer is aligned at a 64-bit boundary. */
#define M0_IS_8ALIGNED(val) ((((uint64_t)(val)) & 07) == 0)

/**
 * 3-way comparison.
 *
 * +1 when v0 >  v1
 *  0 when v0 == v2
 * -1 when v0 <  v2
 */
#define M0_3WAY(v0, v1)				\
({						\
	typeof(v0) __a0 = (v0);			\
	typeof(v1) __a1 = (v1);			\
						\
	(__a0 < __a1) ? -1 : __a0 != __a1;	\
})

#define M0_SWAP(v0, v1)					\
({							\
	typeof(v0) *__a0 = &(v0);			\
	typeof(v1) *__a1 = &(v1);			\
	typeof(v0) __tmp = *__a0;			\
	(void)(__a0 == __a1);				\
							\
	*__a0 = *__a1;					\
	*__a1 = __tmp;					\
})

/** Decrements a counter checking for underflow. */
#define M0_CNT_DEC(cnt)					\
({							\
	M0_ASSERT((cnt) != 0);				\
	--cnt;						\
})

/** Increments a counter checking for overflow. */
#define M0_CNT_INC(cnt)					\
({							\
	++cnt;						\
	M0_ASSERT((cnt) != 0);				\
})

/** Check to determine if the sum of two 64 bit unsigned numbers
 *  will overflow
 */
static inline bool m0_addu64_will_overflow(uint64_t a, uint64_t b)
{
	return a + b < a;
}

/**
 * "Encoding" function: returns the number that a (row, column) element of a
 * matrix with "width" columns has when elements are counted row by row.
 * @see m0_dec()
 */
static inline uint64_t m0_enc(uint64_t width, uint64_t row, uint64_t column)
{
	M0_PRE(column < width);
	return row * width + column;
}

/**
 * "Decoding" function: returns (row, column) coordinates of a pos-th element
 *  in a matrix with "width" column when elements are counted row by row.
 * @see m0_enc()
 */
static inline void m0_dec(uint64_t width, uint64_t pos, uint64_t *row,
			  uint64_t *column)
{
	*row    = pos / width;
	*column = pos % width;
}

/** @} end of arith group */
#endif /* __MERO_LIB_ARITH_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
