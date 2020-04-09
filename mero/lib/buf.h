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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 12/02/2010
 */

#pragma once

#ifndef __MERO_LIB_BUF_H__
#define __MERO_LIB_BUF_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

/**
 * @defgroup buf Basic buffer type
 * @{
 */

/** Memory buffer. */
struct m0_buf {
	m0_bcount_t b_nob;
	void       *b_addr;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(conf|rpc);

/** Sequence of memory buffers. */
struct m0_bufs {
	uint32_t       ab_count;
	struct m0_buf *ab_elems;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(conf|rpc);

/**
 * Initialisers for struct m0_buf.
 *
 * @note
 *
 *   1. #include "lib/misc.h" for M0_BUF_INITS().
 *
 *   2. M0_BUF_INITS() cannot be used with `static' variables.
 * @code
 *         // static const struct m0_buf bad = M0_BUF_INITS("foo");
 *         //  ==> warning: initializer element is not constant
 *
 *         static char str[] = "foo";
 *         static const struct m0_buf good = M0_BUF_INIT(sizeof str, str);
 * @endcode
 */
#define M0_BUF_INIT(size, data) \
	((struct m0_buf){ .b_nob = (size), .b_addr = (data) })
#define M0_BUF_INIT_CONST(size, data) \
	(const struct m0_buf) {	.b_nob = (size), .b_addr = (void *)(data) }

#define M0_BUF_INIT_PTR(p) M0_BUF_INIT(sizeof *(p), (p))
#define M0_BUF_INITS(str)  M0_BUF_INIT(strlen(str), (str))
#define M0_BUF_INIT0       M0_BUF_INIT(0, NULL)

#define M0_BUF_INIT_PTR_CONST(p) M0_BUF_INIT_CONST(sizeof *(p), (p))

#define BUF_F    "[%p,%llu]"
#define BUF_P(p) (p)->b_addr, (unsigned long long)(p)->b_nob

/** Initialises struct m0_buf. */
M0_INTERNAL void m0_buf_init(struct m0_buf *buf, void *data, uint32_t nob);

/** Allocates memory pointed to by @buf->b_addr of size @size. */
M0_INTERNAL int m0_buf_alloc(struct m0_buf *buf, size_t size);

/** Frees memory pointed to by buf->b_addr and zeroes buffer's fields. */
M0_INTERNAL void m0_buf_free(struct m0_buf *buf);

/**
 * Compares memory of two buffers.
 *
 * Returns zero if buffers are equal (including their sizes). Otherwise, returns
 * difference between the first pair of bytes that differ in `x' and `y'.
 * If buffers have different sizes and one is a prefix of the second,
 * the sign of the expression (x->b_nob - y->b_nob) is returned.
 *
 * Therefore, sign of the return value can be used to determine lexicographical
 * order of the buffers.
 *
 * m0_buf_cmp() can be treated as an analogue of strcmp(3) for m0_buf strings
 * that may contain '\0'.
 */
M0_INTERNAL int m0_buf_cmp(const struct m0_buf *x, const struct m0_buf *y);

/** Returns true iff two buffers are equal. */
M0_INTERNAL bool m0_buf_eq(const struct m0_buf *x, const struct m0_buf *y);

/**
 * Copies a buffer.
 *
 * User is responsible for m0_buf_free()ing `dest'.
 *
 * @pre   dest->b_nob == 0 && dest->b_addr == NULL
 * @post  ergo(result == 0, m0_buf_eq(dest, src))
 */
M0_INTERNAL int m0_buf_copy(struct m0_buf *dest, const struct m0_buf *src);

/**
 * Allocates 'dst' buffer aligned on (2^shift)-byte boundary and copies 'src'
 * into it.
 *
 * @pre   dst->b_nob == 0 && dst->b_addr == NULL
 */
M0_INTERNAL int m0_buf_copy_aligned(struct m0_buf *dst,
				    struct m0_buf *src,
				    unsigned       shift);

/** Does the buffer point at anything? */
M0_INTERNAL bool m0_buf_is_set(const struct m0_buf *buf);

/**
 * Do `buf' and `str' contain equal sequences of non-'\0' characters?
 *
 * @pre  str != NULL
 */
M0_INTERNAL bool m0_buf_streq(const struct m0_buf *buf, const char *str);

/**
 * Duplicates a string pointed to by buf->b_addr.
 *
 * Maximum length of the resulting string, including null character,
 * is buf->b_nob.
 */
M0_INTERNAL char *m0_buf_strdup(const struct m0_buf *buf);

/**
 * Constructs a sequence of memory buffers, copying data from
 * NULL-terminated array of C strings.
 *
 * User is responsible for m0_bufs_free()ing `dest'.
 */
M0_INTERNAL int m0_bufs_from_strings(struct m0_bufs *dest, const char **src);

/**
 * Constructs a NULL-terminated array of C strings, copying data from a
 * sequence of memory buffers.
 *
 * The elements of `*dest' should be freed eventually.
 * @see  m0_strings_free()
 */
M0_INTERNAL int m0_bufs_to_strings(const char ***dest,
				   const struct m0_bufs *src);

/**
 * Checks equality of given sequences.
 *
 * @see  m0_buf_streq()
 */
M0_INTERNAL bool m0_bufs_streq(const struct m0_bufs *bufs, const char **strs);

/** Frees memory buffers. */
M0_INTERNAL void m0_bufs_free(struct m0_bufs *bufs);

/** @} end of buf group */
#endif /* __MERO_LIB_BUF_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
