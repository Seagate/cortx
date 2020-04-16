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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 08/04/2010
 */

#pragma once

#ifndef __MERO_LIB_USER_X86_64_ATOMIC_H__
#define __MERO_LIB_USER_X86_64_ATOMIC_H__

#include "lib/types.h"
#include "lib/assert.h"

/**
   @addtogroup atomic

   Implementation of atomic operations for Linux user space uses x86_64 assembly
   language instructions (with gcc syntax). "Lock" prefix is used
   everywhere---no optimisation for non-SMP configurations in present.
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
	asm volatile("lock incq %0"
		     : "=m" (a->a_value)
		     : "m" (a->a_value));
}

/**
 atomically decrement counter

 @param a pointer to atomic counter

 @return none
 */
static inline void m0_atomic64_dec(struct m0_atomic64 *a)
{
	asm volatile("lock decq %0"
		     : "=m" (a->a_value)
		     : "m" (a->a_value));
}

/**
   Atomically adds given amount to a counter
 */
static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num)
{
	asm volatile("lock addq %1,%0"
		     : "=m" (a->a_value)
		     : "er" (num), "m" (a->a_value));
}

/**
   Atomically subtracts given amount from a counter
 */
static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num)
{
	asm volatile("lock subq %1,%0"
		     : "=m" (a->a_value)
		     : "er" (num), "m" (a->a_value));
}


/**
 atomically increment counter and return result

 @param a pointer to atomic counter

 @return new value of atomic counter
 */
static inline int64_t m0_atomic64_add_return(struct m0_atomic64 *a,
						  int64_t delta)
{
	long result;

	result = delta;
	asm volatile("lock xaddq %0, %1;"
		     : "+r" (delta), "+m" (a->a_value)
		     : : "memory");
	return delta + result;
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
	unsigned char result;

	asm volatile("lock incq %0; sete %1"
		     : "=m" (a->a_value), "=qm" (result)
		     : "m" (a->a_value) : "memory");
	return result != 0;
}

static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a)
{
	unsigned char result;

	asm volatile("lock decq %0; sete %1"
		     : "=m" (a->a_value), "=qm" (result)
		     : "m" (a->a_value) : "memory");
	return result != 0;
}

static inline bool m0_atomic64_cas(int64_t * loc, int64_t oldval, int64_t newval)
{
	int64_t val;

	M0_CASSERT(8 == sizeof oldval);

	asm volatile("lock cmpxchgq %2,%1"
		     : "=a" (val), "+m" (*(volatile long *)(loc))
		     : "r" (newval), "0" (oldval)
		     : "memory");
	return val == oldval;
}

static inline void m0_mb(void)
{
	asm volatile("mfence":::"memory");
}

/** @} end of atomic group */
#endif /* __MERO_LIB_USER_X86_64_ATOMIC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
