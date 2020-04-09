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
 * Original creation date: 05/17/2010
 */

#pragma once

#ifndef __MERO_LIB_MEMORY_H__
#define __MERO_LIB_MEMORY_H__

#include "lib/types.h"
#include "lib/assert.h"  /* M0_CASSERT */
#include "lib/finject.h" /* M0_FI_ENABLED */

/**
   @defgroup memory Memory allocation handling functions
   @{
*/

/**
 * Allocates zero-filled memory.
 * The memory allocated is guaranteed to be suitably aligned
 * for any kind of variable.
 * @param size - memory size
 *
 * @retval NULL - allocation failed
 * @retval !NULL - allocated memory block
 */
void *m0_alloc(size_t size);

/**
 * Allocates memory without explicit zeroing.
 *
 * Everything else is the same as in m0_alloc().
 */
M0_INTERNAL void *m0_alloc_nz(size_t size);

/**
 * Frees memory block
 *
 * This function must be a no-op when called with NULL argument.
 *
 * @param data pointer to allocated block
 */
void m0_free(void *data);

/**
 * Forces page faults for the memory block [addr, addr + size).
 *
 * Usually it is done with writing at least one byte on each page
 * of the allocated block.
 *
 * It is intented to use in conjunction with m0_alloc_nz().
 *
 * @note It doesn't guarantee to preserve the data in the memory block.
 * @note Kernel version of the function does nothing.
 */
M0_INTERNAL void m0_memory_pagein(void *addr, size_t size);

/** Frees memory and unsets the pointer. */
#define m0_free0(pptr)                        \
	do {                                  \
		typeof(pptr) __pptr = (pptr); \
		m0_free(*__pptr);             \
		*__pptr = NULL;               \
	} while (0)

#define M0_ALLOC_ARR(arr, nr)  ((arr) = M0_FI_ENABLED(#arr "-fail") ? NULL : \
					m0_alloc((nr) * sizeof ((arr)[0])))
#define M0_ALLOC_PTR(ptr)      M0_ALLOC_ARR(ptr, 1)

#define M0_ALLOC_ARR_ALIGNED(arr, nr, shift)		\
	((arr) = m0_alloc_aligned((nr) * sizeof ((arr)[0]), (shift)))

/**
   Allocates zero-filled memory, aligned on (2^shift)-byte boundary.
   In kernel mode due to the usage of __GFP_ZERO, it can't be used from hard or
   soft interrupt context.
 */
M0_INTERNAL void *m0_alloc_aligned(size_t size, unsigned shift);

/**
 * Frees aligned memory block
 * This function must be a no-op when called with NULL argument.
 * @param data pointer to allocated block
 *
 */
M0_INTERNAL void m0_free_aligned(void *data, size_t size, unsigned shift);

/** It returns true when addr is aligned by value shift. */
static inline bool m0_addr_is_aligned(const void *addr, unsigned shift)
{
	M0_CASSERT(sizeof(unsigned long) >= sizeof(void *));
	return ((((unsigned long)addr >> shift) << shift) ==
		  (unsigned long)addr);
}

/**
 * Allocates the memory suitable for DMA, DIRECT_IO or sharing
 * between user and kernel spaces.
 *
 * @note not tested/used in kernel space for now.
 *
 * @param size Memory size.
 * @param shift Alignment, ignored in kernel space.
 * @pre size <= PAGE_SIZE
 */
M0_INTERNAL void *m0_alloc_wired(size_t size, unsigned shift);

/**
 * Frees the memory allocated with m0_alloc_wired().
 */
M0_INTERNAL void m0_free_wired(void *data, size_t size, unsigned shift);

/**
 * Return amount of memory currently allocated.
 */
M0_INTERNAL size_t m0_allocated(void);

/**
 * Returns cumulative amount of memory allocated so far since libmero library
 * loading.
 */
M0_INTERNAL size_t m0_allocated_total(void);

/**
 * Returns cumulative amount of memory freed so far since libmero library
 * loading.
 */
M0_INTERNAL size_t m0_freed_total(void);

/**
 * Same as system getpagesize(3).
 * Used in the code shared between user and kernel.
 */
M0_INTERNAL int m0_pagesize_get(void);

/**
 * Returns true iff "p" points to a freed and poisoned (with ENABLE_FREE_POISON)
 * memory area.
 *
 * If memory poisoning is disabled, this always returns true.
 *
 * This function is not absolutely reliable. m0_arch_free() can overwrite parts
 * of freed memory region. Specifically, libc free(3) uses first 8 bytes of the
 * memory region for its internal purposes.
 */
M0_INTERNAL bool m0_is_poisoned(const void *p);

/**
 * Mark this memory region to be excluded from core dump.
 * see madvise(2).
 */
M0_INTERNAL int m0_dont_dump(void *p, size_t size);


/** @} end of memory group */
#endif /* __MERO_LIB_MEMORY_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
