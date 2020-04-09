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
 * Original author: Nathan Rutman <Nathan_Rutman@xyratex.com>
 * Original creation date: 11/17/2010
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "lib/bitstring.h"
#include "lib/arith.h"      /* M0_3WAY */
#include "lib/memory.h"     /* m0_alloc() */

M0_INTERNAL void *m0_bitstring_buf_get(struct m0_bitstring *c)
{
        return c->b_data;
}

M0_INTERNAL uint32_t m0_bitstring_len_get(const struct m0_bitstring *c)
{
        return c->b_len;
}

M0_INTERNAL void m0_bitstring_len_set(struct m0_bitstring *c, uint32_t len)
{
        c->b_len = len;
}

M0_INTERNAL struct m0_bitstring *m0_bitstring_alloc(const char *name,
						    size_t len)
{
        struct m0_bitstring *c = m0_alloc(sizeof(*c) + len);
        if (c == NULL)
                return NULL;
        m0_bitstring_copy(c, name, len);
        return c;
}

M0_INTERNAL void m0_bitstring_free(struct m0_bitstring *c)
{
        m0_free(c);
}

M0_INTERNAL void m0_bitstring_copy(struct m0_bitstring *dst, const char *src,
				   size_t count)
{
        memcpy(m0_bitstring_buf_get(dst), src, count);
        m0_bitstring_len_set(dst, count);
}

/**
   String-like compare: alphanumeric for the length of the shortest string.
   Shorter strings precede longer strings.
   Strings may contain embedded NULLs.
 */
M0_INTERNAL int m0_bitstring_cmp(const struct m0_bitstring *s1,
				 const struct m0_bitstring *s2)
{
	/* Compare leading parts up to min_len. If they differ, return the
	 * result, otherwize compare lengths. */
	return memcmp(s1->b_data, s2->b_data, min_check(s1->b_len, s2->b_len))
		?: M0_3WAY(s1->b_len, s2->b_len);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
