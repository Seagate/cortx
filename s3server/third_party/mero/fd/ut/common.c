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
 * Original author: Nachiket Sahasrabudhe <Nachiket_Sahasrabudhe@xyratex.com>
 * Original creation date: 25th June'15.
 */

#include "fd/fd_internal.h"
#include "fd/ut/common.h"
#include "ut/ut.h"
#include "lib/memory.h"      /* m0_alloc() */
#include "lib/errno.h"       /* ENOME M */
#include "lib/arith.h"       /* min_type() */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"         /* M0_ERR() */

M0_INTERNAL void fd_ut_symm_tree_create(struct m0_fd_tree *tree,
					enum tree_gen_type tg_type,
					uint64_t *child_nr, uint64_t depth)
{
	int      rc;
	uint32_t i;

	M0_UT_ASSERT(tree != NULL);
	M0_UT_ASSERT(M0_IN(tg_type, (TG_DETERM, TG_RANDOM)));
	M0_UT_ASSERT(child_nr != NULL);

	if (tg_type == TG_RANDOM)
		fd_ut_children_populate(child_nr, M0_CONF_PVER_HEIGHT - 1);
	rc = m0_fd__tree_root_create(tree, child_nr[0]);
	M0_UT_ASSERT(rc == 0);

	for (i = 1; i < depth; ++i) {
		rc = fd_ut_tree_level_populate(tree, child_nr[i], i, TA_SYMM);
		M0_UT_ASSERT(rc == 0);
	}
	M0_UT_ASSERT(m0_fd__tree_invariant(tree));
}

M0_INTERNAL void fd_ut_children_populate(uint64_t *children, uint32_t depth)
{
	uint32_t  i;
	uint32_t  j;
	m0_time_t seed;

	seed = m0_time_now();
	for (i = 0; i < depth; ++i) {
		j = m0_rnd(TUA_CHILD_NR_MAX, &seed);
		children[i] = TUA_CHILD_NR_MAX - j;
	}
}

M0_INTERNAL int fd_ut_tree_level_populate(struct m0_fd_tree *tree,
			                  uint64_t max_children,
					  uint16_t level, enum tree_attr ta)
{
	struct m0_fd__tree_cursor   cursor;
	struct m0_fd_tree_node    **node;
	uint64_t                    children;
	int                         rc;

	M0_UT_ASSERT(tree != NULL);
	M0_UT_ASSERT(tree->ft_root != NULL);
	M0_UT_ASSERT(level <= tree->ft_depth);
	M0_UT_ASSERT(ergo(level == tree->ft_depth, max_children == 0));

	rc = m0_fd__tree_cursor_init(&cursor, tree, level);
	M0_UT_ASSERT(rc == 0);
	do {
		node = m0_fd__tree_cursor_get(&cursor);
		*node = m0_alloc(sizeof node[0][0]);
		if (*node == NULL) {
			rc = -ENOMEM;
			goto err;
		}
		children = ta == TA_SYMM ? max_children :
			fd_ut_random_cnt_get(max_children);
		/* To ensure better randomization for asymmetric tree */
		max_children = ta == TA_ASYMM && max_children > 1 ?
			max_children - 1 : max_children;
		rc   = m0_fd__tree_node_init(tree, *node, children, &cursor);
		if (rc != 0)
			goto err;
	} while (m0_fd__tree_cursor_next(&cursor));
	return 0;
err:
	if (*node != NULL) {
		m0_fd__tree_node_fini(tree, *node);
		m0_free(*node);
		*node = NULL;
	}
	--cursor.ftc_child_abs_idx;
	tree->ft_depth = cursor.ftc_child_abs_idx > - 1 ? cursor.ftc_depth :
		cursor.ftc_depth - 1;
	return rc;
}

M0_INTERNAL int fd_ut_tree_init(struct m0_fd_tree *tree, uint64_t tree_depth)
{
	M0_PRE(tree != NULL);
	M0_PRE(tree_depth < M0_CONF_PVER_HEIGHT);

	tree->ft_depth = tree_depth;
	tree->ft_cnt   = 0;
	tree->ft_root  = m0_alloc(sizeof tree->ft_root[0]);
	if (tree->ft_root == NULL)
		goto err;
	return 0;
err:
	tree->ft_depth = 0;
	return -ENOMEM;
}

M0_INTERNAL void fd_ut_symm_tree_get(struct m0_fd_tree *tree,
				     uint64_t *children_nr)
{
	struct m0_fd__tree_cursor  cursor;
	struct m0_fd_tree_node    *node;
	uint64_t                   depth;
	uint64_t                   i;
	uint64_t                   min = 0;
	int                        rc;

	M0_UT_ASSERT(m0_fd__tree_invariant(tree));
	M0_UT_ASSERT(children_nr != NULL);

	for (i = 0; i < M0_CONF_PVER_HEIGHT; ++i)
		children_nr[i] = i >= tree->ft_depth;

	for (depth = 0; depth < tree->ft_depth; ++depth) {
		rc = m0_fd__tree_cursor_init(&cursor, tree, depth);
		M0_UT_ASSERT(rc == 0);
		do {
			node = *(m0_fd__tree_cursor_get(&cursor));
			if (node->ftn_abs_idx == 0)
				min  = node->ftn_child_nr;
			min = min_type(uint64_t, min, node->ftn_child_nr);
			children_nr[depth] = min;
		} while (m0_fd__tree_cursor_next(&cursor));
	}
}

M0_INTERNAL uint64_t fd_ut_random_cnt_get(uint64_t max_cnt)
{
	m0_time_t seed;
	uint64_t  cnt;

	seed = m0_time_now();
	cnt = m0_rnd(max_cnt, &seed);
	return max_cnt - cnt;
}
