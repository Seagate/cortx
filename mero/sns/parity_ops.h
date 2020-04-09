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
 * Original creation date: 10/19/2010
 */

#pragma once

#ifndef __MERO_SNS_PARITY_OPS_H__
#define __MERO_SNS_PARITY_OPS_H__

#define M0_PARITY_ZERO (0)
#define M0_PARITY_GALOIS_W (8)
typedef unsigned char m0_parity_elem_t;

M0_INTERNAL int m0_parity_ut_init(bool try_ssse3);
M0_INTERNAL void m0_parity_ut_fini(void);

M0_INTERNAL int m0_parity_init(void);
M0_INTERNAL void m0_parity_fini(void);
M0_INTERNAL m0_parity_elem_t m0_parity_mul(m0_parity_elem_t x,
					   m0_parity_elem_t y);
M0_INTERNAL m0_parity_elem_t m0_parity_div(m0_parity_elem_t x,
					   m0_parity_elem_t y);
M0_INTERNAL m0_parity_elem_t m0_parity_pow(m0_parity_elem_t x,
					   m0_parity_elem_t p);

/**
 * Region based multiplication over GF^W.
 *
 * dst[i] = src[i] (GF*) multiplier, for each i in [0, size), where (GF*) is a
 * multiplication operator over GF^W.
 */
M0_INTERNAL void m0_parity_region_mul(m0_parity_elem_t       *dst,
				      const m0_parity_elem_t *src,
				      unsigned int	      size,
				      m0_parity_elem_t        multiplier);

/**
 * Region based multiplication with accumulation over GF^W.
 *
 * dst[i] = dst[i] (GF+) (src[i] (GF*) multiplier), for each i in [0, size),
 * where (GF*) is a multiplication operator and (GF+) is a addition operator
 * over GF^W.
 */
M0_INTERNAL void m0_parity_region_mac(m0_parity_elem_t       *dst,
				      const m0_parity_elem_t *src,
				      unsigned int	      size,
				      m0_parity_elem_t        multiplier);

static inline m0_parity_elem_t m0_parity_add(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x ^ y;
}

static inline m0_parity_elem_t m0_parity_sub(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x ^ y;
}

static inline m0_parity_elem_t m0_parity_lt(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x < y;
}

static inline m0_parity_elem_t m0_parity_gt(m0_parity_elem_t x, m0_parity_elem_t y)
{
	return x > y;
}

/* __MERO_SNS_PARITY_OPS_H__ */
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
