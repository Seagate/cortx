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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 11/01/2011
 *
 */

#pragma once

#ifndef __MERO_NET_ULNET_CORE_H__
#define __MERO_NET_ULNET_CORE_H__

/**
   @defgroup ULNetCore LNet Transport Core User Space Private Interface
   @ingroup LNetCore

   @{
 */

/**
   Userspace domain private data.
   This structure is pointed to by nlx_core_domain::cd_upvt.
 */
struct nlx_ucore_domain {
	uint64_t                        ud_magic;
	/** Cached maximum buffer size (counting all segments). */
	m0_bcount_t                     ud_max_buffer_size;
	/** Cached maximum size of a buffer segment. */
	m0_bcount_t                     ud_max_buffer_segment_size;
	/** Cached maximum number of buffer segments. */
	int32_t                         ud_max_buffer_segments;
	/** Cached NID strings.  If LNet were to support dynamically configured
	    NIDs, then this simple caching would have to be re-addressed.
	 */
        char                          **ud_nidstrs;
	/** Number of references to the NID strings */
	struct m0_atomic64              ud_nidstrs_refcount;
	/** File descriptor to the kernel device. */
	int                             ud_fd;
};

/**
   Userspace transfer machine private data.
   This structure is pointed to by nlx_core_transfer_mc::ctm_upvt.
 */
struct nlx_ucore_transfer_mc {
	uint64_t utm_magic;
};

/**
   Userspace buffer private data.
   This structure is pointed to by nlx_core_buffer::cb_upvt.
 */
struct nlx_ucore_buffer {
	uint64_t                        ub_magic;
};

/** @} */ /* ULNetCore */

#endif /* __MERO_NET_ULNET_CORE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
