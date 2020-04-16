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
 * Original author: Nathan Rutman <Nathan_Rutman@xyratex.com>
 * Original creation date: 11/17/2010
 */

#pragma once

#ifndef __MERO_LIB_BITSTRING_H__
#define __MERO_LIB_BITSTRING_H__

#include "lib/types.h"

/**
   @defgroup adt Basic abstract data types
   @{
*/

struct m0_bitstring {
	uint32_t b_len;
	char     b_data[0];
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
  Get a pointer to the data in the bitstring.
  Data may be read or written here.

  User is responsible for allocating large enough contiguous memory.
 */
M0_INTERNAL void *m0_bitstring_buf_get(struct m0_bitstring *c);
/**
 Report the bitstring length
 */
M0_INTERNAL uint32_t m0_bitstring_len_get(const struct m0_bitstring *c);
/**
 Set the bitstring valid length
 */
M0_INTERNAL void m0_bitstring_len_set(struct m0_bitstring *c, uint32_t len);
/**
 String-like compare: alphanumeric for the length of the shortest string.
 Shorter strings are "less" than matching longer strings.
 Bitstrings may contain embedded NULLs.
 */
M0_INTERNAL int m0_bitstring_cmp(const struct m0_bitstring *c1,
				 const struct m0_bitstring *m0);

/**
 Copy @src to @dst.
*/
M0_INTERNAL void m0_bitstring_copy(struct m0_bitstring *dst,
				   const char *src, size_t count);

/**
 Alloc memory for a string of passed len and copy name to it.
*/
M0_INTERNAL struct m0_bitstring *m0_bitstring_alloc(const char *name,
						    size_t len);

/**
 Free memory of passed @c.
*/
M0_INTERNAL void m0_bitstring_free(struct m0_bitstring *c);

/** @} end of adt group */
#endif /* __MERO_LIB_BITSTRING_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
