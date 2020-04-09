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
 * Original author: Manish Honap <Manish_honap@xyratex.com>
 * Original creation date: 2012-Aug-07
 */

#pragma once

#ifndef __MERO_LIB___SYNC_ATOMIC_H__
#define __MERO_LIB___SYNC_ATOMIC_H__

#include "lib/types.h"
#include "lib/assert.h"

/**
   @addtogroup atomic

   Implementation of atomic operations for Linux user space;
   uses gcc built-ins __sync_fetch* functions

   @ref http://gcc.gnu.org/onlinedocs/gcc-4.5.4/gcc/Atomic-Builtins.html

   We did a benchmark of these function against the assembly based functions in
   lib/user_space/user_x86_64_atomic.h and we came to know that these are
   a bit slower than their assembly counterparts. So these are kept off by
   default (a configure option --enable-sync_atomic will enable them.)

   * With assembly
   [xyratex@guest0 sandbox1]$ utils/ub 10
   set:    atomic-ub
    bench: [   iter]    min    max    avg   std   sec/op   op/sec
   atomic: [   1000]  73.68  83.15  79.57  3.40% 7.957e-02/1.257e+01

   * with gcc built-ins
   [xyratex@guest0 sandbox1]$ utils/ub 10
   set:    atomic-ub
    bench: [   iter]    min    max    avg   std   sec/op   op/sec
   atomic: [   1000]  74.35  95.29  81.38  6.37% 8.138e-02/1.229e+01
 */

struct m0_atomic64 {
	long a_value;
};

static inline void m0_atomic64_set(struct m0_atomic64 *a, int64_t num)
{
	M0_CASSERT(sizeof a->a_value == sizeof num);

	a->a_value = num;
}

/**
   Returns value of an atomic counter.
 */
static inline int64_t m0_atomic64_get(const struct m0_atomic64 *a)
{
	return a->a_value;
}

/**
   atomically increment counter

   @param a pointer to atomic counter

   @return none
 */
static inline void m0_atomic64_inc(struct m0_atomic64 *a)
{
	__sync_fetch_and_add(&a->a_value, 1);
}

/**
   atomically decrement counter

   @param a pointer to atomic counter

   @return none
 */
static inline void m0_atomic64_dec(struct m0_atomic64 *a)
{
	__sync_fetch_and_sub(&a->a_value, 1);
}

/**
   Atomically adds given amount to a counter
 */
static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num)
{
	__sync_fetch_and_add(&a->a_value, num);
}

/**
   Atomically subtracts given amount from a counter
 */
static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num)
{
	__sync_fetch_and_sub(&a->a_value, num);
}

/**
   atomically increment counter and return result

   @param a pointer to atomic counter

   @return new value of atomic counter
 */
static inline int64_t m0_atomic64_add_return(struct m0_atomic64 *a,
					     int64_t delta)
{
	return __sync_add_and_fetch(&a->a_value, delta);
}

/**
   atomically decrement counter and return result

   @param a pointer to atomic counter

   @return new value of atomic counter
 */
static inline int64_t m0_atomic64_sub_return(struct m0_atomic64 *a,
					     int64_t delta)
{
	return m0_atomic64_add_return(a, -delta);
}

static inline bool m0_atomic64_inc_and_test(struct m0_atomic64 *a)
{
	return __sync_add_and_fetch(&a->a_value, 1) == 0;
}

static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a)
{
	return __sync_sub_and_fetch(&a->a_value, 1) == 0;
}

static inline bool
m0_atomic64_cas(int64_t * loc, int64_t oldval, int64_t newval)
{
	return __sync_bool_compare_and_swap(loc, oldval, newval);
}

static inline void m0_mb(void)
{
	__sync_synchronize();
}

/** @} end of atomic group */
#endif /* __MERO_LIB___SYNC_ATOMIC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
