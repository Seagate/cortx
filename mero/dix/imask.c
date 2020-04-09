/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 31-May-2016
 */


/**
 * @addtogroup dix
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "dix/imask.h"
#include "lib/misc.h"   /* M0_BYTES, m0_bit_get, m0_bit_set */
#include "lib/ext.h"    /* struct m0_ext */
#include "lib/ext_xc.h"	/* m0_ext_xc */

#define AT(mask, idx) ((mask)->im_range[idx])

static m0_bcount_t range_size(const struct m0_ext *range)
{
	return range ? (range->e_end == IMASK_INF) ? IMASK_INF :
		       range->e_end - range->e_start + 1 :
		       0;
}

static m0_bcount_t range_actual_size(const struct m0_ext *range,
				     uint64_t		  bs_len)
{
	m0_bcount_t size = range_size(range);

	return size == IMASK_INF || size > bs_len ?
	       bs_len > range->e_start ? bs_len - range->e_start : 0 :
	       size;
}

static int dix_imask_range_alloc(struct m0_dix_imask *mask,
				 uint64_t             nr)
{
	M0_PRE(mask->im_range == NULL);

	M0_ALLOC_ARR(mask->im_range, nr);
	if (mask->im_range == NULL)
		return M0_ERR(-ENOMEM);
	mask->im_nr   = nr;
	return 0;
}

static void dix_imask_range_free(struct m0_dix_imask *mask)
{
	M0_PRE(mask != NULL);
	m0_free(mask->im_range);
}

static uint64_t ranges_size(struct m0_ext *range,
			    uint64_t	   nr,
			    uint64_t	   bs_len)
{
	uint64_t res = 0;
	uint64_t i   = 0;

	for (i = 0; i < nr; i++)
		res += range_actual_size(&range[i], bs_len);
	return res;
}
/**
 * Returns total size in bits of all ranges for the given bit-string length in
 * bits. Bit-string length is required in case if mask contains infinite range
 * or bit-string is shorter than mask being applied.
 */
static uint64_t imask_size(struct m0_dix_imask *mask, uint64_t bs_len)
{
	return ranges_size(mask->im_range, mask->im_nr, bs_len);
}

M0_INTERNAL int m0_dix_imask_init(struct m0_dix_imask *mask,
				  struct m0_ext       *range,
				  uint64_t             nr)
{
	int      rc;
	uint64_t i;

	M0_ENTRY();
	M0_SET0(mask);
	if (range == NULL || nr == 0) {
		M0_LOG(M0_DEBUG, "Empty imask %p initialisation", mask);
		return M0_RC(0);
	}
	rc = dix_imask_range_alloc(mask, nr);
	if (rc != 0)
		return M0_ERR(rc);
	for (i = 0; i < mask->im_nr; i++)
		mask->im_range[i] = range[i];
	return M0_RC(rc);
}

M0_INTERNAL void m0_dix_imask_fini(struct m0_dix_imask *mask)
{
	dix_imask_range_free(mask);
	M0_SET0(mask);
}

static void mask_bit_copy(void *buffer, m0_bcount_t pos, void *res,
			  m0_bcount_t respos)
{
	m0_bit_set(res, respos, m0_bit_get(buffer, pos));
}

M0_INTERNAL bool m0_dix_imask_is_empty(const struct m0_dix_imask *mask)
{
	return mask->im_range == NULL || mask->im_nr == 0;
}

M0_INTERNAL int m0_dix_imask_apply(void                 *buffer,
				   m0_bcount_t           buf_len_bytes,
				   struct m0_dix_imask  *mask,
				   void                **res,
				   m0_bcount_t          *res_len_bits)
{
	char                      *result;
	uint64_t                   mask_size;
	uint64_t                   i;
	m0_bcount_t                k;
	m0_bcount_t                j;
	m0_bcount_t                rsize;

	M0_PRE(buffer != NULL);
	M0_PRE(buf_len_bytes != 0);
	mask_size = imask_size(mask, buf_len_bytes * 8);
	*res_len_bits = 0;
	*res = NULL;
	if (mask_size == 0)
		return M0_RC(0);
	result = m0_alloc(M0_BYTES(mask_size));
	if (result == NULL)
		return M0_ERR(-ENOMEM);
	k = 0;
	for (i = 0; i < mask->im_nr; i++) {
		rsize = range_actual_size(&AT(mask, i), buf_len_bytes * 8);
		for (j = 0; j < rsize && k < mask_size; j++) {
			mask_bit_copy(buffer, AT(mask, i).e_start + j,
				      result, k);
			k++;
		}
	}
	*res_len_bits = mask_size;
	*res          = result;
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_imask_copy(struct m0_dix_imask       *dst,
				  const struct m0_dix_imask *src)
{
	uint64_t i;

	dst->im_nr = src->im_nr;
	M0_ALLOC_ARR(dst->im_range, dst->im_nr);
	if (dst->im_range == NULL)
		return M0_ERR(-ENOMEM);
	for (i = 0; i < dst->im_nr; i++)
		dst->im_range[i] = src->im_range[i];
	return 0;
}


M0_INTERNAL bool m0_dix_imask_eq(const struct m0_dix_imask *imask1,
				 const struct m0_dix_imask *imask2)
{
	M0_PRE(imask1 != NULL);
	M0_PRE(imask2 != NULL);

	if (imask1->im_nr != imask2->im_nr)
		return false;

	return m0_forall(i, imask1->im_nr,
			 imask1->im_range[i].e_start ==
			 imask2->im_range[i].e_start &&
			 imask1->im_range[i].e_end ==
			 imask2->im_range[i].e_end);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dix group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
