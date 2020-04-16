/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 5-Jun-2013
 */

#pragma once
#ifndef __MERO_BE_SEG_INTERNAL_H__
#define __MERO_BE_SEG_INTERNAL_H__

#include "be/alloc_internal.h"  /* m0_be_allocator_header */
#include "be/alloc_internal_xc.h"
#include "be/list.h"
#include "be/seg.h"
#include "be/seg_xc.h"
#include "format/format.h"      /* m0_format_header */
#include "format/format_xc.h"

/**
 * @addtogroup be
 *
 * @{
 */

enum {
	M0_BE_SEG_HDR_GEOM_ITMES_MAX = 16,
	M0_BE_SEG_HDR_VERSION_LEN_MAX = 64,
};

/** "On-disk" header for segment, stored in STOB at zero offset */
struct m0_be_seg_hdr {
	struct m0_format_header       bh_header;
	uint64_t                      bh_id;
	struct m0_be_allocator_header bh_alloc[M0_BAP_NR];
	uint32_t                      bh_items_nr;
	struct m0_be_seg_geom         bh_items[M0_BE_SEG_HDR_GEOM_ITMES_MAX];
	char                          bh_be_version[
					       M0_BE_SEG_HDR_VERSION_LEN_MAX+1];
	struct m0_be_list             bh_dict;
	struct m0_format_footer       bh_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_seg_hdr_format_version {
	M0_BE_SEG_HDR_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_SEG_HDR_FORMAT_VERSION */
	/*M0_BE_SEG_HDR_FORMAT_VERSION_2,*/
	/*M0_BE_SEG_HDR_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_SEG_HDR_FORMAT_VERSION = M0_BE_SEG_HDR_FORMAT_VERSION_1
};

/** @} end of be group */
#endif /* __MERO_BE_SEG_INTERNAL_H__ */

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
