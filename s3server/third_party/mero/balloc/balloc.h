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
 * Original author: Huang Hua <hua_huang@xyratex.com>
 * Original creation date: 07/27/2010
 */

#pragma once

#ifndef __MERO_BALLOC_BALLOC_H__
#define __MERO_BALLOC_BALLOC_H__

#include "lib/ext.h"
#include "lib/types.h"
#include "lib/list.h"
#include "lib/mutex.h"
#include "be/btree.h"
#include "be/btree_xc.h"
#include "format/format.h"
#include "stob/ad.h"
#include "stob/ad_xc.h"

/**
   @defgroup balloc data-block-allocator

   All data structures stored in db in little-endian format (Intel CPU endian).
   To correctly compare the key value, a custom specific comparison function
   is provided to db.
   @{
*/

/**
   On-disk data structure for group, stored in db.

   When stored in db, {key, value} = {group_nr, m0_balloc_group_desc}.
   Every group description will stay in a separate db.
 */
struct m0_balloc_group_desc {
	struct m0_format_header bgd_header;
	/** group number */
	m0_bindex_t             bgd_groupno;
	/** total free blocks */
	m0_bcount_t             bgd_freeblocks;
	/** nr of freespace fragments */
	m0_bcount_t             bgd_fragments;
	/** max bytes of freespace chunk */
	m0_bcount_t             bgd_maxchunk;
#ifdef __SPARE_SPACE__
	/** number of free spare space fragments */
	m0_bcount_t             bgd_spare_frags;
	/** total free spare blocks */
	m0_bcount_t             bgd_spare_freeblocks;
	/** starting block for a spare. */
	m0_bcount_t             bgd_sparestart;
	/** max contiguous free spare space. */
	m0_bcount_t             bgd_spare_maxchunk;
#endif
	struct m0_format_footer bgd_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_balloc_group_desc_format_version {
	M0_BALLOC_GROUP_DESC_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BALLOC_GROUP_DESC_FORMAT_VERSION */
	/*M0_BALLOC_GROUP_DESC_FORMAT_VERSION_2,*/
	/*M0_BALLOC_GROUP_DESC_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BALLOC_GROUP_DESC_FORMAT_VERSION = M0_BALLOC_GROUP_DESC_FORMAT_VERSION_1
};

enum m0_balloc_allocation_flag {
	/** prefer goal again. length */
	M0_BALLOC_HINT_MERGE              = 1 << 0,

	/** blocks already reserved */
	M0_BALLOC_HINT_RESERVED           = 1 << 1,

	/** metadata is being allocated */
	M0_BALLOC_HINT_METADATA           = 1 << 2,

	/** use the first found extent */
	M0_BALLOC_HINT_FIRST              = 1 << 3,

	/** search for the best chunk */
	M0_BALLOC_HINT_BEST               = 1 << 4,

	/** data is being allocated */
	M0_BALLOC_HINT_DATA               = 1 << 5,

	/** don't preallocate (for tails) */
	M0_BALLOC_HINT_NOPREALLOC         = 1 << 6,

	/** allocate for locality group */
	M0_BALLOC_HINT_GROUP_ALLOC        = 1 << 7,

	/** allocate goal blocks or none */
	M0_BALLOC_HINT_GOAL_ONLY          = 1 << 8,

	/** goal is meaningful */
	M0_BALLOC_HINT_TRY_GOAL           = 1 << 9,

	/** blocks already pre-reserved by delayed allocation */
	M0_BALLOC_DELALLOC_RESERVED       = 1 << 10,

	/** We are doing stream allocation */
	M0_BALLOC_STREAM_ALLOC            = 1 << 11,

	/** Allocate in spare zone. */
	M0_BALLOC_SPARE_ZONE              = 1 << 12,

	/** Allocate in normal zone. */
	M0_BALLOC_NORMAL_ZONE             = 1 << 13,
};

struct m0_balloc_zone_param {
	enum m0_balloc_allocation_flag  bzp_type;
	struct m0_ext                   bzp_range;
	m0_bcount_t                     bzp_freeblocks;
	m0_bcount_t                     bzp_fragments;
	m0_bcount_t                     bzp_maxchunk;
	struct m0_list                  bzp_extents;
};

/** Linked extents */
struct m0_lext {
	/** Is allocated separately from bgi_extents array? */
	bool                le_is_alloc;
	struct m0_list_link le_link;
	struct m0_ext       le_ext;
};

/**
   In-memory data structure for group
 */
struct m0_balloc_group_info {
	/** @see m0_balloc_group_info_state for values */
	uint64_t                     bgi_state;
	/** group number */
	m0_bindex_t                  bgi_groupno;
	struct m0_balloc_zone_param  bgi_normal;
	struct m0_balloc_zone_param  bgi_spare;
	/** Array of group extents */
	struct m0_lext              *bgi_extents;
	/** per-group lock */
	struct m0_be_mutex           bgi_mutex;
};

enum m0_balloc_group_info_state {
	/** inited from disk */
	M0_BALLOC_GROUP_INFO_INIT = 1 << 0,
	/** dirty, need sync */
	M0_BALLOC_GROUP_INFO_DIRTY = 1 << 1,
};

/**
   On-disk and in-memory super block stored in db
 */
struct m0_balloc_super_block {
	uint64_t	bsb_magic;
	uint64_t	bsb_state;
	uint64_t	bsb_version;
	uint8_t		bsb_uuid[128];

        m0_bcount_t	bsb_totalsize;        /**< total size in bytes */
        m0_bcount_t	bsb_freeblocks;       /**< nr of free blocks */
#ifdef __SPARE_SPACE__
	m0_bcount_t     bsb_freespare;        /**< nr free spare blocks */
	m0_bcount_t     bsb_sparesize;        /**< spare blocks per group */
#endif
        m0_bcount_t	bsb_blocksize;        /**< block size in bytes */
        m0_bcount_t	bsb_groupsize;        /**< group size in blocks */
	uint32_t	bsb_bsbits;           /**< block size bits: power of 2*/
	uint32_t	bsb_gsbits;           /**< group size bits: power of 2*/
        m0_bcount_t	bsb_groupcount;       /**< # of group */
        m0_bcount_t	bsb_prealloc_count;   /**< nr of pre-alloc blocks */

	uint64_t	bsb_format_time;
	uint64_t	bsb_write_time;
	uint64_t	bsb_mnt_time;
	uint64_t	bsb_last_check_time;

	uint64_t	bsb_mnt_count;
	uint64_t	bsb_max_mnt_count;

        m0_bcount_t	bsb_stripe_size;      /**< stripe size in blocks */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_balloc_super_block_state {
	M0_BALLOC_SB_DIRTY =  1 << 0,
};

enum m0_balloc_super_block_version {
	M0_BALLOC_SB_VERSION = 1ULL,
};

enum {
	M0_BALLOC_BUDDY_LOOKUP_MAX = 10,
};

/**
   BE-backed in-memory data structure for the balloc environment.

   It includes pointers to db, various flags and parameters.
 */
struct m0_balloc {
	struct m0_format_header      cb_header;

	/** container this block allocator belongs to. */
	uint64_t                     cb_container_id;
	/** the on-disk and in-memory sb */
	struct m0_balloc_super_block cb_sb;

	m0_bindex_t                  cb_last;

	struct m0_ad_balloc          cb_ballroom;
	struct m0_format_footer      cb_footer;

	/*
	 * m0_be_btree has it's own volatile-only fields, so it can't be placed
	 * before the m0_format_footer, where only persistent fields allowed
	 */
	/** db for free extent */
	struct m0_be_btree           cb_db_group_extents;
	/** db for group desc */
	struct m0_be_btree           cb_db_group_desc;

	/*
	 * volatile-only fields
	 */

	/** array of group info */
	struct m0_balloc_group_info *cb_group_info;
	/** super block lock */
	struct m0_be_mutex           cb_sb_mutex;
	struct m0_be_seg            *cb_be_seg;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_balloc_format_version {
	M0_BALLOC_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BALLOC_FORMAT_VERSION */
	/*M0_BALLOC_FORMAT_VERSION_2,*/
	/*M0_BALLOC_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BALLOC_FORMAT_VERSION = M0_BALLOC_FORMAT_VERSION_1
};

static inline struct m0_balloc *b2m0(const struct m0_ad_balloc *ballroom)
{
	return container_of(ballroom, struct m0_balloc, cb_ballroom);
}

/**
   Request to format a container.
 */
struct m0_balloc_format_req {
	/** Total size in bytes. */
	m0_bcount_t	bfr_totalsize;
	/** Block size in bytes. */
	m0_bcount_t	bfr_blocksize;
	/** Group size in blocks. */
	m0_bcount_t	bfr_groupsize;
	/** XXX Spare blocks per group. Should be a power of two. */
	m0_bcount_t     bfr_spare_reserved_blocks;
};

struct m0_balloc_free_extent {
        m0_bindex_t bfe_logical;       /*< logical offset within the object */
        m0_bindex_t bfe_groupno;       /*< goal group # */
        m0_bindex_t bfe_start;         /*< relative start offset */
        m0_bindex_t bfe_end;           /*< relative start offset */
};

/**
   Request to allocate multiple blocks from a container.

   Result is stored in bar_physical. On error case, error number is returned
   in bar_err. If all available free chunks are smaller that requested, then
   the maximum available chunk size is returned in bar_max_avail.
 */
struct m0_balloc_allocate_req {
	m0_bindex_t	bar_logical; /*< [in]logical offset within the object */
	m0_bcount_t	bar_len;     /*< [in]count of blocks, */
	m0_bindex_t	bar_goal;    /*< [in]prefered physical block number */
	uint64_t	bar_flags;   /*< [in]allocation flags from
				      * m0_balloc_allocation_flag */
        struct m0_ext   bar_result;  /*< [out]physical offset, result */

	void           *bar_prealloc;/*< [in][out]User opaque prealloc result */
};

/**
   Request to free multiple blocks to a container.
 */
struct m0_balloc_free_req {
	m0_bindex_t	bfr_logical;  /*< logical offset within the object */
	m0_bcount_t	bfr_len;      /*< count of blocks */
	m0_bindex_t	bfr_physical; /*< physical block number */
	uint32_t	bfr_flags;    /*< free flags */
};

struct m0_balloc_discard_req {
	void           *bdr_prealloc; /*< User opaque prealloc result */
};

/*
 * BALLOC_DEF_BLOCKS_PER_GROUP * (1 << BALLOC_DEF_BLOCK_SHIFT) = 128 MB -->
 * which equals group size in ext4
 */
enum {
	/** @todo should equal to size of HDD */
	BALLOC_DEF_CONTAINER_SIZE	= 4096ULL * 32 * 1024 * 1000,
	BALLOC_DEF_BLOCK_SHIFT		= 12,// 4K Blocks
	/**
	 * Approximate number of groups. Exact number of groups can differ
	 * depending on group size restrictions.
	 */
	BALLOC_DEF_GROUPS_NR            = 64,
	/** Used as minimal group size */
	BALLOC_DEF_BLOCKS_PER_GROUP     = 32768,
};

/**
   Initialises volatile only fields in m0_balloc
 */
M0_INTERNAL void m0_balloc_init(struct m0_balloc *cb);

/**
   Creates struct m0_balloc instance @out with container @id
   on back-end segment @seg.

   One balloc instance is allocated and initialised per storage domain.

   @see struct ad_balloc_ops
   @pre out != NULL
 */
M0_INTERNAL int m0_balloc_create(uint64_t           cid,
				 struct m0_be_seg  *seg,
				 struct m0_sm_group *grp,
				 struct m0_balloc **out);

M0_INTERNAL void m0_balloc_group_desc_init(struct m0_balloc_group_desc *desc);

/* Interfaces for UT */
M0_INTERNAL void m0_balloc_debug_dump_sb(const char *tag,
					 struct m0_balloc_super_block *sb);
M0_INTERNAL void m0_balloc_debug_dump_group_extent(const char *tag,
						   struct m0_balloc_group_info
						   *grp);

M0_INTERNAL int m0_balloc_release_extents(struct m0_balloc_group_info *grp);
M0_INTERNAL int m0_balloc_load_extents(struct m0_balloc *cb,
				       struct m0_balloc_group_info *grp);
M0_INTERNAL struct m0_balloc_group_info *m0_balloc_gn2info(struct m0_balloc *cb,
							   m0_bindex_t groupno);
M0_INTERNAL void m0_balloc_debug_dump_group(const char *tag,
					    struct m0_balloc_group_info *grp);
M0_INTERNAL void m0_balloc_lock_group(struct m0_balloc_group_info *grp);
M0_INTERNAL int m0_balloc_trylock_group(struct m0_balloc_group_info *grp);
M0_INTERNAL void m0_balloc_unlock_group(struct m0_balloc_group_info *grp);

/** @} end of balloc */

#endif /*__MERO_BALLOC_BALLOC_H__*/

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
