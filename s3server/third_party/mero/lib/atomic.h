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
 *		    Alexey Lyashkov <alexey_lyashkov@xyratex.com>
 * Original creation date: 04/08/2010
 */

#pragma once

#ifndef __MERO_LIB_ATOMIC_H__
#define __MERO_LIB_ATOMIC_H__

#include "lib/assert.h"
#include "lib/types.h"

#ifdef __KERNEL__
#  include "lib/linux_kernel/atomic64.h"
#else
#  ifdef ENABLE_SYNC_ATOMIC
#    include "lib/user_space/__sync_atomic.h"
#  else
#    include "lib/user_space/user_x86_64_atomic.h"
#  endif
#endif

/**
   @defgroup atomic

   Atomic operations on 64bit quantities.

   Implementation of these is platform-specific.

   @{
 */

/**
   atomic counter
 */
struct m0_atomic64;

/**
   Assigns a value to a counter.
 */
static inline void m0_atomic64_set(struct m0_atomic64 *a, int64_t num);

/**
   Returns value of an atomic counter.
 */
static inline int64_t m0_atomic64_get(const struct m0_atomic64 *a);

/**
   Atomically increments a counter.
 */
static inline void m0_atomic64_inc(struct m0_atomic64 *a);

/**
   Atomically decrements a counter.
 */
static inline void m0_atomic64_dec(struct m0_atomic64 *a);

/**
   Atomically adds given amount to a counter.
 */
static inline void m0_atomic64_add(struct m0_atomic64 *a, int64_t num);

/**
   Atomically subtracts given amount from a counter.
 */
static inline void m0_atomic64_sub(struct m0_atomic64 *a, int64_t num);

/**
   Atomically increments a counter and returns the result.
 */
static inline int64_t m0_atomic64_add_return(struct m0_atomic64 *a,
						  int64_t d);

/**
   Atomically decrements a counter and returns the result.
 */
static inline int64_t m0_atomic64_sub_return(struct m0_atomic64 *a,
						  int64_t d);

/**
   Atomically increments a counter and returns true iff the result is 0.
 */
static inline bool m0_atomic64_inc_and_test(struct m0_atomic64 *a);

/**
   Atomically decrements a counter and returns true iff the result is 0.
 */
static inline bool m0_atomic64_dec_and_test(struct m0_atomic64 *a);

/**
   Atomic compare-and-swap: compares value stored in @loc with @oldval and, if
   equal, replaces it with @newval, all atomic w.r.t. concurrent accesses to @loc.

   Returns true iff new value was installed.
 */
static inline bool m0_atomic64_cas(int64_t * loc, int64_t oldval,
					int64_t newval);

/**
   Atomic compare-and-swap for pointers.

   @see m0_atomic64_cas().
 */
static inline bool m0_atomic64_cas_ptr(void **loc, void *oldval, void *newval)
{
	M0_CASSERT(sizeof loc == sizeof(int64_t *));
	M0_CASSERT(sizeof oldval == sizeof(int64_t));

	return m0_atomic64_cas((int64_t *)loc, (int64_t)oldval, (int64_t)newval);
}

#define M0_ATOMIC64_CAS(loc, oldval, newval)				\
({									\
	M0_CASSERT(__builtin_types_compatible_p(typeof(*(loc)), typeof(oldval))); \
	M0_CASSERT(__builtin_types_compatible_p(typeof(oldval), typeof(newval))); \
	m0_atomic64_cas_ptr((void **)(loc), oldval, newval);		\
})

/**
   Hardware memory barrier. Forces strict CPU ordering.
 */
static inline void m0_mb(void);

/** @} end of atomic group */

/* __MERO_LIB_ATOMIC_H__ */
#endif
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
