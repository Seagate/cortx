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
 * Original creation date: 25th June-15
 */

#include "fd/fd_internal.h"
#include "lib/assert.h"     /* _0C */
#include "lib/memory.h"     /* m0_alloc m0_free */
#include "lib/errno.h"      /* EINVAL ENOMEM */
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FD
#include "lib/trace.h"      /* M0_ERR */


M0_INTERNAL int m0_fd__tree_node_init(struct m0_fd_tree *tree,
				      struct m0_fd_tree_node *node,
				      uint16_t child_nr,
				      const struct m0_fd__tree_cursor *cursor)
{
	M0_PRE(tree != NULL && node != NULL && cursor != NULL);
	M0_PRE(ergo(cursor->ftc_depth != 0, cursor->ftc_node != NULL));
	M0_PRE(ergo(cursor->ftc_depth != tree->ft_depth, child_nr > 0));

	node->ftn_parent    = cursor->ftc_node;
	node->ftn_depth     = cursor->ftc_depth;
	node->ftn_rel_idx   = cursor->ftc_child_idx;
	node->ftn_abs_idx   = cursor->ftc_child_abs_idx;
	node->ftn_ha_state  = M0_NC_ONLINE;
	++tree->ft_cnt;
	node->ftn_child_nr  = child_nr;
	node->ftn_children  = m0_alloc(child_nr * sizeof node->ftn_children[0]);
	if (node->ftn_children == NULL)
		return M0_ERR(-ENOMEM);
	return M0_RC(0);
}

M0_INTERNAL void m0_fd__tree_node_fini(struct m0_fd_tree *tree,
				       struct m0_fd_tree_node *node)
{
	M0_PRE(tree != NULL && node != NULL);
	if (node->ftn_children != NULL)
		m0_free0(&node->ftn_children);
	--tree->ft_cnt;
}

M0_INTERNAL int m0_fd__tree_cursor_init(struct m0_fd__tree_cursor *cursor,
					const struct m0_fd_tree *tree,
					uint16_t depth)
{
	struct m0_fd__tree_cursor coarse_cursor;

	M0_PRE(cursor != NULL);
	M0_PRE(tree   != NULL);
	M0_PRE(depth  <= tree->ft_depth);

	M0_SET0(&coarse_cursor);
	coarse_cursor.ftc_tree  = (struct m0_fd_tree *)tree;
	/* Specially treat the root node. */
	if (depth == 0) {
		coarse_cursor.ftc_node  = NULL;
		coarse_cursor.ftc_depth = 0;
	} else {
		coarse_cursor.ftc_node    = tree->ft_root;
		coarse_cursor.ftc_depth   = 1;
		coarse_cursor.ftc_path[0] = 0;
	}
	while (coarse_cursor.ftc_depth < depth) {
		M0_ASSERT(m0_fd__tree_node_invariant(tree,
						     coarse_cursor.ftc_node));
			coarse_cursor.ftc_node =
			coarse_cursor.ftc_node->ftn_children[0];
		++coarse_cursor.ftc_depth;
	}
	coarse_cursor.ftc_child_idx     = 0;
	coarse_cursor.ftc_child_abs_idx = 0;
	coarse_cursor.ftc_cnt           = 0;
	*cursor                         = coarse_cursor;
	return M0_RC(0);
}

M0_INTERNAL int m0_fd__tree_cursor_init_at(struct m0_fd__tree_cursor *cursor,
				           const struct m0_fd_tree *tree,
					   const struct m0_fd_tree_node *node,
				           uint32_t child_idx)
{
	struct m0_fd__tree_cursor  fine_cursor;
	struct m0_fd_tree_node    *parent;
	uint32_t                   depth;

	M0_PRE(cursor != NULL);
	M0_PRE(m0_fd__tree_invariant(tree));
	M0_PRE(m0_fd__tree_node_invariant(tree, node));
	M0_PRE(child_idx < node->ftn_child_nr);

	M0_SET0(&fine_cursor);
	fine_cursor.ftc_tree          = (struct m0_fd_tree *)tree;
	fine_cursor.ftc_node          = (struct m0_fd_tree_node *)node;
	fine_cursor.ftc_depth         = node->ftn_depth + 1;
	fine_cursor.ftc_child_idx     = child_idx;
	fine_cursor.ftc_cnt           = 0;
	if (!m0_fd__tree_node_invariant(tree, node->ftn_children[child_idx]))
		return M0_ERR(-EINVAL);
	fine_cursor.ftc_child_abs_idx =
		node->ftn_children[child_idx]->ftn_abs_idx;
	fine_cursor.ftc_path[node->ftn_depth + 1] =
		fine_cursor.ftc_child_abs_idx;
	parent = (struct m0_fd_tree_node *)node;
	depth  = parent->ftn_depth;
	while (parent != NULL) {
		fine_cursor.ftc_path[depth] = parent->ftn_abs_idx;
		parent = parent->ftn_parent;
		--depth;
	}
	*cursor = fine_cursor;
	memcpy(cursor, &fine_cursor, sizeof fine_cursor);
	return M0_RC(0);
}

M0_INTERNAL struct m0_fd_tree_node **
	m0_fd__tree_cursor_get(struct m0_fd__tree_cursor *cursor)
{
	return cursor->ftc_depth == 0 ? &cursor->ftc_tree->ft_root :
		&(cursor->ftc_node->ftn_children[cursor->ftc_child_idx]);
}

M0_INTERNAL int m0_fd__tree_cursor_next(struct m0_fd__tree_cursor *cursor)
{
	struct m0_fd_tree_node *parent;
	struct m0_fd_tree_node *child;
	uint16_t                depth;
	uint16_t                child_idx;

	if (cursor->ftc_depth == 0)
		goto end;
	parent = cursor->ftc_node;
	child_idx = cursor->ftc_child_idx;
	if (parent->ftn_child_nr > child_idx + 1) {
		++cursor->ftc_child_idx;
	} else {
		depth = cursor->ftc_depth;
		while (parent != NULL &&
		       parent->ftn_child_nr <= child_idx + 1) {
			child_idx = parent->ftn_rel_idx;
			parent    = parent->ftn_parent;
			--depth;
		}
		if (parent == NULL)
			goto end;
		child = parent->ftn_children[child_idx + 1];
		++depth;
		while (depth < cursor->ftc_depth) {
			child = child->ftn_children[0];
			++depth;
		}
		cursor->ftc_node      = child;
		cursor->ftc_child_idx = 0;
	}
	++cursor->ftc_child_abs_idx;
	cursor->ftc_path[cursor->ftc_depth] = cursor->ftc_child_abs_idx;
	return 1;
end:
	return 0;
}

M0_INTERNAL int m0_fd__tree_root_create(struct m0_fd_tree *tree,
				        uint64_t root_children)
{
	struct m0_fd__tree_cursor cursor;
	int                       rc;

	M0_PRE(tree != NULL && tree->ft_root != NULL);
	/* Initialize the root node. */
	rc = m0_fd__tree_cursor_init(&cursor, tree, 0);
	if (rc != 0)
		return M0_RC(rc);
	rc = m0_fd__tree_node_init(tree, tree->ft_root, root_children,
			           &cursor);
	if (rc != 0) {
		tree->ft_depth = 0;
		m0_fd__tree_node_fini(tree, tree->ft_root);
		m0_free0(&tree->ft_root);
	}
	return M0_RC(rc);
}

M0_INTERNAL bool m0_fd__tree_invariant(const struct m0_fd_tree *tree)
{
	return _0C(tree != NULL) && _0C(tree->ft_depth > 0) &&
	       _0C(tree->ft_root != NULL);
}

M0_INTERNAL bool m0_fd__tree_node_invariant(const struct m0_fd_tree *tree,
					    const struct m0_fd_tree_node *node)
{
	return _0C(node != NULL) && _0C(ergo(node->ftn_depth < tree->ft_depth,
				             node->ftn_child_nr > 0 &&
				             node->ftn_children != NULL));
}

#undef M0_TRACE_SUBSYSTEM
