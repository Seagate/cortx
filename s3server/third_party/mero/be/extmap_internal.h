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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 08/13/2010
 */

#pragma once

#ifndef __MERO_BE_EXTMAP_INTERNAL_H__
#define __MERO_BE_EXTMAP_INTERNAL_H__

/**
   @addtogroup extmap

   <b>Extent map implementation.</b>

   Extent map collection (m0_be_emap) is a table. 128-bit prefix is used to store
   multiple extent maps in the same table.

   @{
 */

#include "lib/buf.h"       /* m0_buf */
#include "lib/buf_xc.h"
#include "lib/types.h"     /* m0_uint128 */
#include "lib/types_xc.h"
#include "be/btree.h"      /* m0_btree */
#include "be/btree_xc.h"

enum m0_be_emap_key_format_version {
	M0_BE_EMAP_KEY_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_EMAP_KEY_FORMAT_VERSION */
	/*M0_BE_EMAP_KEY_FORMAT_VERSION_2,*/
	/*M0_BE_EMAP_KEY_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_EMAP_KEY_FORMAT_VERSION = M0_BE_EMAP_KEY_FORMAT_VERSION_1
};

/**
   A key used to identify a particular segment in the map collection.
 */
struct m0_be_emap_key {
	struct m0_format_header ek_header;
	/**
	    Prefix of the map the segment is part of.
	 */
	struct m0_uint128       ek_prefix;

	/**
	    Last offset of the segment's extent. That is, the key of a segment
	    ([A, B), V) has B as an offset.

	    This not entirely intuitive decision is forced by the available
	    range search interfaces of m0_db_cursor: m0_db_cursor_get()
	    positions the cursor on the least key not less than the key sought
	    for.
	 */
	m0_bindex_t             ek_offset;
	struct m0_format_footer ek_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_emap_rec_format_version {
	M0_BE_EMAP_REC_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_EMAP_REC_FORMAT_VERSION */
	/*M0_BE_EMAP_REC_FORMAT_VERSION_2,*/
	/*M0_BE_EMAP_REC_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_EMAP_REC_FORMAT_VERSION = M0_BE_EMAP_REC_FORMAT_VERSION_1
};

/**
   A record stored in the table for each segment in the map collection.

   @note Note that there is a certain amount of redundancy: for any but the
   first segment in the map, its starting offset is equal to the last offset of
   the previous segment and for the first segment, the starting offset is
   0. Consequently, m0_be_emap_rec::er_start field can be eliminated reducing
   storage foot-print at the expense of increase in code complexity and
   possibility of occasional extra IO.
 */
struct m0_be_emap_rec {
	struct m0_format_header er_header;
	/**
	   Starting offset of the segment's extent.
	 */
	m0_bindex_t             er_start;
	/**
	   Value associated with the segment.
	 */
	uint64_t                er_value;
	struct m0_format_footer er_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_be_emap_format_version {
	M0_BE_EMAP_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_EMAP_FORMAT_VERSION */
	/*M0_BE_EMAP_FORMAT_VERSION_2,*/
	/*M0_BE_EMAP_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_EMAP_FORMAT_VERSION = M0_BE_EMAP_FORMAT_VERSION_1
};

/**
   m0_be_emap stores a collection of related extent maps. Individual maps
   within a collection are identified by a prefix.

   @see m0_be_emap_obj_insert()
 */
struct m0_be_emap {
	struct m0_format_header em_header;
	struct m0_format_footer em_footer;
	/*
	 * m0_be_btree has it's own volatile-only fields, so it can't be placed
	 * before the m0_format_footer, where only persistent fields allowed
	 */
	struct m0_be_btree      em_mapping;
	/*
	 * volatile-only fields
	 */
	uint64_t                em_version;
	/** The segment where we are stored. */
	struct m0_be_seg       *em_seg;
	struct m0_be_rwlock     em_lock;
	struct m0_buf           em_key_buf;
	struct m0_buf           em_val_buf;
	struct m0_be_emap_key   em_key;
	struct m0_be_emap_rec   em_rec;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** @} end group extmap */

/* __MERO_BE_EXTMAP_INTERNAL_H__ */
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
