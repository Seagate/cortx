/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nachiket Sahasrabudhe <nachiket_sahasrabuddhe@xyratex.com>
 * Original creation date: 06th Jan-15
 */

#pragma once

#ifndef __MERO_FD_UT_COMMON_H__
#define __MERO_FD_UT_COMMON_H__

enum layout_attr {
	la_N = 8,
        la_K = 2,
};

enum tree_ut_attr {
	/* Max number of racks in a pool version. */
	TUA_RACKS        = 8,
	/* Max number of enclosures per rack. */
	TUA_ENC          = 7,
	/* Max number of controllers per enclosure. */
	TUA_CON          = 2,
	/* Max number of disks per controller. */
	TUA_DISKS        = 82,
	TUA_ITER         = 20,
	/* Max number of children for any node in UT for the m0_fd_tree-tree. */
	TUA_CHILD_NR_MAX = 13,
	/* Maximum pool width of system. */
	TUA_MAX_POOL_WIDTH = 5000,
};
M0_BASSERT(TUA_CHILD_NR_MAX >= la_N + 2 * la_K &&
	   TUA_MAX_POOL_WIDTH >= TUA_CHILD_NR_MAX);

enum tree_type {
	TP_LEAF,
	TP_UNARY,
	TP_BINARY,
	TP_TERNARY,
	TP_QUATERNARY,
	TP_NR,
};

enum tree_gen_type {
	/* Generation with deterministic parameters. */
	TG_DETERM,
	/* Generation with random parameters. */
	TG_RANDOM,
};

enum tree_attr {
	/* A tree in which all nodes at same level have same number of
	 * children. */
	TA_SYMM,
	/* A tree that is not TA_SYMM. */
	TA_ASYMM,
};

M0_INTERNAL int fd_ut_tree_init(struct m0_fd_tree *tree, uint64_t tree_depth);

M0_INTERNAL void fd_ut_children_populate(uint64_t *child_nr, uint32_t length);


M0_INTERNAL void fd_ut_symm_tree_create(struct m0_fd_tree *tree,
					enum tree_gen_type tg_type,
					uint64_t *child_nr, uint64_t depth);

M0_INTERNAL int fd_ut_tree_level_populate(struct m0_fd_tree *tree,
			                  uint64_t children, uint16_t level,
					  enum tree_attr ta);

M0_INTERNAL void fd_ut_symm_tree_get(struct m0_fd_tree *tree, uint64_t *children_nr);
M0_INTERNAL uint64_t fd_ut_random_cnt_get(uint64_t max_cnt);

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
