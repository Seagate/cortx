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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 29-May-2013
 */

/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BTREE
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/finject.h"       /* M0_FI_ENABLED() */
#include "lib/misc.h"          /* offsetof */
#include "be/alloc.h"
#include "be/btree.h"
#include "be/btree_internal.h" /* m0_be_bnode */
#include "be/seg.h"
#include "be/tx.h"             /* m0_be_tx_capture */

/* btree constants */
enum {
	BTREE_ALLOC_SHIFT = 0,
};

enum btree_save_optype {
	BTREE_SAVE_INSERT,
	BTREE_SAVE_UPDATE,
	BTREE_SAVE_OVERWRITE
};

enum m0_be_bnode_format_version {
	M0_BE_BNODE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_BNODE_FORMAT_VERSION */
	/*M0_BE_BNODE_FORMAT_VERSION_2,*/
	/*M0_BE_BNODE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_BNODE_FORMAT_VERSION = M0_BE_BNODE_FORMAT_VERSION_1
};

struct node_pos {
	struct m0_be_bnode *p_node;
	unsigned int        p_index;
};

static struct m0_be_op__btree *op_tree(struct m0_be_op *op);
static struct m0_rwlock *btree_rwlock(struct m0_be_btree *tree);

static void btree_root_set(struct m0_be_btree *btree,
			   struct m0_be_bnode *new_root)
{
	M0_PRE(btree != NULL);

	btree->bb_root = new_root;
	m0_format_footer_update(btree);
}

/* XXX Shouldn't we set other fields of m0_be_op__btree? */
static void btree_op_fill(struct m0_be_op *op, struct m0_be_btree *btree,
			  struct m0_be_tx *tx, enum m0_be_btree_op optype,
			  struct m0_be_btree_anchor *anchor)
{
	struct m0_be_op__btree *tree;

	M0_PRE(op != NULL);

	tree = &op->bo_u.u_btree;

	op->bo_utype   = M0_BOP_TREE;
	tree->t_tree   = btree;
	tree->t_tx     = tx;
	tree->t_op     = optype;
	tree->t_in     = NULL;
	tree->t_anchor = anchor;
	tree->t_rc     = 0;
}

static struct m0_be_allocator *tree_allocator(const struct m0_be_btree *btree)
{
	return m0_be_seg_allocator(btree->bb_seg);
}

static struct bt_key_val *btree_search(struct m0_be_btree *btree, void *key);

static inline void mem_free(const struct m0_be_btree *btree,
			    struct m0_be_tx *tx, void *ptr)
{
	M0_BE_OP_SYNC(op,
		      m0_be_free_aligned(tree_allocator(btree), tx, &op, ptr));
}

/* XXX: check if region structure itself needed outside m0_be_tx_capture() */
static inline void mem_update(const struct m0_be_btree *btree,
			      struct m0_be_tx *tx, void *ptr, m0_bcount_t size)
{
	m0_be_tx_capture(tx, &M0_BE_REG(btree->bb_seg, size, ptr));
}

static inline void *mem_alloc(const struct m0_be_btree *btree,
			      struct m0_be_tx *tx, m0_bcount_t size,
			      uint64_t zonemask)
{
	void *p;

	M0_BE_OP_SYNC(op,
		      m0_be_alloc_aligned(tree_allocator(btree),
					  tx, &op, &p, size,
					  BTREE_ALLOC_SHIFT,
					  zonemask));
	M0_ASSERT(p != NULL);
	return p;
}

static void btree_mem_alloc_credit(const struct m0_be_btree *btree,
				   m0_bcount_t size,
				   struct m0_be_tx_credit *accum)
{
	m0_be_allocator_credit(NULL, M0_BAO_ALLOC_ALIGNED, size,
			       BTREE_ALLOC_SHIFT, accum);
}

static void btree_mem_free_credit(const struct m0_be_btree *btree,
				  m0_bcount_t size,
				  struct m0_be_tx_credit *accum)
{
	m0_be_allocator_credit(tree_allocator(btree),
			       M0_BAO_FREE_ALIGNED, 0, 0, accum);
}

static m0_bcount_t be_btree_ksize(const struct m0_be_btree *btree, const void *key)
{
	const struct m0_be_btree_kv_ops *ops = btree->bb_ops;
	return ops->ko_ksize(key);
}

static m0_bcount_t be_btree_vsize(const struct m0_be_btree *btree, const void *data)
{
	const struct m0_be_btree_kv_ops *ops = btree->bb_ops;

	return ops->ko_vsize(data);
}

static int be_btree_compare(const struct m0_be_btree *btree,
			    const void *key0, const void *key1)
{
	const struct m0_be_btree_kv_ops *ops = btree->bb_ops;
	return ops->ko_compare(key0, key1);
}

static inline int key_lt(const struct m0_be_btree *btree,
			 const void *key0, const void *key1)
{
	return be_btree_compare(btree, key0, key1)  <  0;
}

static inline int key_gt(const struct m0_be_btree *btree,
			 const void *key0, const void *key1)
{
	return be_btree_compare(btree, key0, key1)  >  0;
}

static inline int key_eq(const struct m0_be_btree *btree,
			 const void *key0, const void *key1)
{
	return be_btree_compare(btree, key0, key1) ==  0;
}

/* ------------------------------------------------------------------
 * Btree internals implementation
 * ------------------------------------------------------------------ */

enum position_t {
	P_LEFT = -1,
	P_RIGHT = 1
};

static struct m0_be_bnode *btree_node_alloc(const struct m0_be_btree *btree,
					    struct m0_be_tx *tx);

static void btree_node_free(struct m0_be_bnode *node,
			    const struct m0_be_btree *btree,
			    struct m0_be_tx *tx);

static void btree_pair_release(struct m0_be_btree *btree,
			       struct m0_be_tx *tx,
			       struct bt_key_val *kv);

static struct node_pos get_btree_node(struct m0_be_btree_cursor *it, void *key,
				      bool slant);

static int delete_key_from_node(struct m0_be_btree *btree,
				struct m0_be_tx *tx,
				struct node_pos *node_pos);

static void move_key(struct m0_be_btree *btree,
		     struct m0_be_tx *tx,
		     struct m0_be_bnode *node,
		     unsigned int index,
		     enum position_t pos);

static void get_max_key_pos(struct m0_be_btree *btree,
			    struct m0_be_bnode *subtree,
			    struct node_pos    *pos);

static void get_min_key_pos(struct m0_be_btree *btree,
			    struct m0_be_bnode *subtree,
			    struct node_pos    *pos);

static int iter_prepare(struct m0_be_bnode *node, bool print);


/* ------------------------------------------------------------------
 * Btree invariant implementation:
 * - assuming that the tree is completely in memory;
 * - checks that keys are in order;
 * - child nodes have keys matching parent;
 * - nodes have expected occupancy: [1..2*order-1] for root and
 *				    [order-1..2*order-1] for leafs.
 *
 * Note: as far as height of practical tree will be 10-15, invariant can be
 * written in recusieve form.
 * ------------------------------------------------------------------ */

static bool btree_node_invariant(const struct m0_be_btree *btree,
				 const struct m0_be_bnode *node,
				 bool root)
{
	return
		ergo(node && node->b_header.hd_magic,
		     m0_format_footer_verify(node) == 0) &&
		node->b_level <= BTREE_HEIGHT_MAX &&
		/* expected occupancy */
		ergo(root, 0 <= node->b_nr_active &&
		     node->b_nr_active <= KV_NR) &&
		ergo(!root, BTREE_FAN_OUT-1 <= node->b_nr_active &&
		     node->b_nr_active <= KV_NR) &&
		/* keys are in order */
		ergo(node->b_nr_active > 1,
		     m0_forall(i, node->b_nr_active - 1,
			       key_gt(btree, node->b_key_vals[i+1].key,
			                     node->b_key_vals[i].key))) &&
		/* kids are in order */
		ergo(node->b_nr_active > 0 && !node->b_leaf,
		     m0_forall(i, node->b_nr_active,
			       key_gt(btree, node->b_key_vals[i].key,
				      node->b_children[i]->
	b_key_vals[node->b_children[i]->b_nr_active - 1].key) &&
			       key_lt(btree, node->b_key_vals[i].key,
				      node->b_children[i+1]->
						   b_key_vals[0].key)) &&
		     m0_forall(i, node->b_nr_active + 1,
			       btree_node_invariant(btree, node->b_children[i],
						    false)));
}

/* ------------------------------------------------------------------
 * b-tree internals.
 * ------------------------------------------------------------------ */

static inline bool btree_invariant(const struct m0_be_btree *btree)
{
	return btree_node_invariant(btree, btree->bb_root, true) &&
	       ergo(btree && btree->bb_header.hd_magic,
	            m0_format_footer_verify(btree) == 0);
}

static void btree_node_update(struct m0_be_bnode       *node,
			      const struct m0_be_btree *btree,
			      struct m0_be_tx          *tx)
{
	mem_update(btree, tx, node, offsetof(struct m0_be_bnode, b_key_vals));

	if (node->b_nr_active > 0) {
		mem_update(btree, tx, node->b_key_vals,
			   sizeof(*node->b_key_vals) * node->b_nr_active);
		mem_update(btree, tx, node->b_children,
			   sizeof(*node->b_children) * (node->b_nr_active + 1));
	}

	mem_update(btree, tx, &node->b_footer, sizeof(node->b_footer));
}

/**
 * Used to create a btree with just the root node
 */
static void btree_create(struct m0_be_btree *btree, struct m0_be_tx *tx)
{
	m0_format_header_pack(&btree->bb_header, &(struct m0_format_tag){
		.ot_version = M0_BE_BTREE_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_BTREE,
		.ot_footer_offset = offsetof(struct m0_be_btree, bb_footer)
	});
	btree_root_set(btree, btree_node_alloc(btree, tx));
	mem_update(btree, tx, btree, sizeof(struct m0_be_btree));

	/* memory for the node has to be reserved by m0_be_tx_open() */
	M0_ASSERT(btree->bb_root != NULL);
}

/**
 * Function used to allocate memory for the btree node
 * @return The allocated B-tree node
 */
static struct m0_be_bnode *
btree_node_alloc(const struct m0_be_btree *btree, struct m0_be_tx *tx)
{
	struct m0_be_bnode *node;

	/*  Allocate memory for the node */
	node = (struct m0_be_bnode *)mem_alloc(btree, tx,
					       sizeof(struct m0_be_bnode),
					       M0_BITS(M0_BAP_NORMAL));
	M0_ASSERT(node != NULL);	/* @todo: analyse return code */

	m0_format_header_pack(&node->b_header, &(struct m0_format_tag){
		.ot_version = M0_BE_BNODE_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_BNODE,
		.ot_footer_offset = offsetof(struct m0_be_bnode, b_footer)
	});

	node->b_nr_active = 0;
	node->b_leaf = true;
	node->b_level = 0;
	node->b_next = NULL;

	m0_format_footer_update(node);
	mem_update(btree, tx, node, sizeof *node);

	return node;
}

/**
 * Frees the @node.
 */
static void btree_node_free(struct m0_be_bnode       *node,
			    const struct m0_be_btree *btree,
			    struct m0_be_tx          *tx)
{
	mem_free(btree, tx, node);
}

/**
 * Splits the child node at @index and updates the @parent.
 */
static void btree_split_child(struct m0_be_btree *btree,
			      struct m0_be_tx	 *tx,
			      struct m0_be_bnode *parent,
			      unsigned int	  index)
{
	int i;
	unsigned int order = BTREE_FAN_OUT;
	struct m0_be_bnode *child = parent->b_children[index];
	struct m0_be_bnode *new_child = btree_node_alloc(btree, tx);
	M0_ASSERT(new_child != NULL);

	new_child->b_leaf = child->b_leaf;
	new_child->b_level = child->b_level;
	new_child->b_nr_active = order - 1;

	/*  Copy the higher order keys to the new child */
	for (i = 0; i < new_child->b_nr_active; i++) {
		new_child->b_key_vals[i] = child->b_key_vals[i + order];
		if (!child->b_leaf)
			new_child->b_children[i] = child->b_children[i + order];
	}
	/*  Copy the last child pointer */
	if (!child->b_leaf)
		new_child->b_children[i] = child->b_children[i + order];

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(new_child);

	child->b_nr_active = order - 1;
	m0_format_footer_update(child);

	/*  Make room for new child in parent's womb */
	for (i = parent->b_nr_active + 1; i > index + 1; i--)
		parent->b_children[i] = parent->b_children[i - 1];
	for (i = parent->b_nr_active; i > index; i--)
		parent->b_key_vals[i] = parent->b_key_vals[i - 1];

	/*  Update parent */
	parent->b_children[index + 1] = new_child;
	parent->b_key_vals[index] = child->b_key_vals[order - 1];
	parent->b_nr_active++;

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(parent);

	/* Update affected memory regions in tx: */
	btree_node_update(parent, btree, tx);
	btree_node_update(child, btree, tx);
	btree_node_update(new_child, btree, tx);
}

/**
 * Inserts @kv entry into the non-full @node.
 */
static void btree_insert_nonfull(struct m0_be_btree *btree,
				 struct m0_be_tx    *tx,
				 struct m0_be_bnode *node,
				 struct bt_key_val  *kv)
{
	void *key = kv->key;
	int i;

 insert:
	i = node->b_nr_active - 1;
	if (node->b_leaf) {
		while (i >= 0 && key_lt(btree, key, node->b_key_vals[i].key)) {
			node->b_key_vals[i + 1] = node->b_key_vals[i];
			i--;
		}
		node->b_key_vals[i + 1] = *kv;
		node->b_nr_active++;

		m0_format_footer_update(node);
		/* Update affected memory regions */
		btree_node_update(node, btree, tx);
	} else {
		while (i >= 0 && key_lt(btree, key, node->b_key_vals[i].key))
			i--;
		i++;

		if (node->b_children[i]->b_nr_active == KV_NR) {
			btree_split_child(btree, tx, node, i);
			if (key_gt(btree, key, node->b_key_vals[i].key))
				i++;
		}
		node = node->b_children[i];
		goto insert;
	}
}

/**
 * Inserts @kv entry into @btree.
 */
static void btree_insert_key(struct m0_be_btree *btree,
			     struct m0_be_tx	*tx,
			     struct bt_key_val	*kv)
{
	struct m0_be_bnode *rnode;
	struct m0_be_bnode *new_root;

	M0_PRE_EX(btree_invariant(btree));
	M0_PRE_EX(btree_search(btree, kv->key) == NULL);

	rnode = btree->bb_root;
	if (rnode->b_nr_active == KV_NR) {
		new_root = btree_node_alloc(btree, tx);
		M0_ASSERT(new_root != NULL);

		new_root->b_level = btree->bb_root->b_level + 1;
		btree_root_set(btree, new_root);
		new_root->b_leaf = false;
		new_root->b_nr_active = 0;
		new_root->b_children[0] = rnode;
		m0_format_footer_update(new_root);
		btree_split_child(btree, tx, new_root, 0);
		btree_insert_nonfull(btree, tx, new_root, kv);

		/* Update tree structure itself */
		mem_update(btree, tx, btree, sizeof(struct m0_be_btree));
	} else
		btree_insert_nonfull(btree, tx, rnode, kv);

	M0_POST_EX(btree_invariant(btree));
}

/**
 *	Used to get the position of the MAX key within the subtree
 *	@param btree The btree
 *	@param subtree The subtree to be searched
 *	@return The node_pos containing the key and position of the key
 */
static void get_max_key_pos(struct m0_be_btree *btree,
			    struct m0_be_bnode *node,
			    struct node_pos    *pos)
{
	for (; node != NULL && !node->b_leaf;
	     node = node->b_children[node->b_nr_active])
		;

	pos->p_node  = node;
	if (node != NULL && node->b_nr_active > 0)
		pos->p_index = node->b_nr_active - 1;
	else
		pos->p_index = 0;
}

/**
 *	Used to get the position of the MAX key within the subtree
 *	@param btree The btree
 *	@param subtree The subtree to be searched
 *	@return The node_pos containing the key and position of the key
 */
static void get_min_key_pos(struct m0_be_btree *btree,
			    struct m0_be_bnode *node,
			    struct node_pos    *pos)
{
	for (; node != NULL && !node->b_leaf; node = node->b_children[0])
		;

	pos->p_node  = node;
	pos->p_index = 0;
}

/**
 *	Merge nodes n1 and n2 (case 3b from Cormen)
 *	@param btree The btree
 *	@param node The parent node
 *	@param index of the child
 */
static struct m0_be_bnode *
merge_siblings(struct m0_be_btree *btree,
	       struct m0_be_tx    *tx,
	       struct m0_be_bnode *parent,
	       unsigned int        index)
{
	unsigned int i;
	struct m0_be_bnode *n1;
	struct m0_be_bnode *n2;

	M0_ENTRY("n=%p i=%d", parent, index);

	if (index == parent->b_nr_active)
		index--;

	n1 = parent->b_children[index];
	n2 = parent->b_children[index + 1];

	n1->b_key_vals[n1->b_nr_active++] = parent->b_key_vals[index];

	M0_ASSERT(n1->b_nr_active + n2->b_nr_active <= KV_NR);
	for (i = 0; i < n2->b_nr_active; i++) {
		n1->b_key_vals[i + n1->b_nr_active] = n2->b_key_vals[i];
		n1->b_children[i + n1->b_nr_active] = n2->b_children[i];
	}
	n1->b_children[i + n1->b_nr_active] = n2->b_children[n2->b_nr_active];
	n1->b_nr_active += n2->b_nr_active;

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(n1);

	/* update parent */
	for (i = index; i < parent->b_nr_active - 1; i++) {
		parent->b_key_vals[i] = parent->b_key_vals[i + 1];
		parent->b_children[i + 1] = parent->b_children[i + 2];
	}
	parent->b_nr_active--;
	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(parent);

	btree_node_free(n2, btree, tx);

	if (parent->b_nr_active == 0 && btree->bb_root == parent) {
		btree_node_free(parent, btree, tx);
		btree_root_set(btree, n1);
		mem_update(btree, tx, btree, sizeof(struct m0_be_btree));
	} else {
		/* Update affected memory regions */
		btree_node_update(parent, btree, tx);
	}

	btree_node_update(n1, btree, tx);

	M0_LEAVE();
	return n1;
}

/**
 * Move the key from node to another
 * @param btree The B-Tree
 * @param parent The parent node
 * @param index of the key to be moved done
 * @param pos the position of the child to receive the key
 */
static void move_key(struct m0_be_btree	  *btree,
		     struct m0_be_tx	  *tx,
		     struct m0_be_bnode	  *parent,
		     unsigned int	   index,
		     enum position_t	   pos)
{
	struct m0_be_bnode *lch;
	struct m0_be_bnode *rch;
	unsigned int i;

	M0_ENTRY("n=%p i=%d dir=%d", parent, index, pos);

	if (pos == P_RIGHT) {
		index--;
	}
	lch = parent->b_children[index];
	rch = parent->b_children[index + 1];

	/*  Move the key from the parent to the left child */
	if (pos == P_LEFT) {
		lch->b_key_vals[lch->b_nr_active] = parent->b_key_vals[index];
		lch->b_children[lch->b_nr_active + 1] = rch->b_children[0];
		lch->b_nr_active++;

		parent->b_key_vals[index] = rch->b_key_vals[0];

		for (i = 0; i < rch->b_nr_active - 1; i++) {
			rch->b_key_vals[i] = rch->b_key_vals[i + 1];
			rch->b_children[i] = rch->b_children[i + 1];
		}
		rch->b_children[i] = rch->b_children[i + 1];
		rch->b_children[i + 1] = NULL;
		rch->b_nr_active--;
	} else {
	/*  Move the key from the parent to the right child */
		/* prepare place */
		for (i = rch->b_nr_active; i > 0; i--) {
			rch->b_key_vals[i] = rch->b_key_vals[i - 1];
			rch->b_children[i + 1] = rch->b_children[i];
		}
		rch->b_children[1] = rch->b_children[0];

		rch->b_key_vals[0] = parent->b_key_vals[index];
		rch->b_children[0] = lch->b_children[lch->b_nr_active];
		lch->b_children[lch->b_nr_active] = NULL;

		parent->b_key_vals[index] = lch->b_key_vals[lch->b_nr_active-1];

		lch->b_nr_active--;
		rch->b_nr_active++;
	}

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(lch);
	m0_format_footer_update(rch);
	m0_format_footer_update(parent);

	/* Update affected memory regions in tx: */
	btree_node_update(parent, btree, tx);
	btree_node_update(lch, btree, tx);
	btree_node_update(rch, btree, tx);

	M0_LEAVE();
}

/**
 * Used to delete a key from the B-tree node
 * @return 0 on success -1 on error
 */
int delete_key_from_node(struct m0_be_btree	 *btree,
			 struct m0_be_tx	 *tx,
			 struct node_pos	 *node_pos)
{
	unsigned int i = node_pos->p_index;
	struct m0_be_bnode *node = node_pos->p_node;

	if (node->b_leaf == false)
		return -1;

	btree_pair_release(btree, tx, &node->b_key_vals[i]);

	for (i = node_pos->p_index; i < node->b_nr_active - 1; i++)
		node->b_key_vals[i] = node->b_key_vals[i + 1];

	node->b_nr_active--;

	/* re-calculate checksum after all fields has been updated */
	m0_format_footer_update(node);

	if (node->b_nr_active == 0 && node != btree->bb_root)
		btree_node_free(node, btree, tx);
	else
		/* Update affected memory regions in tx: */
		btree_node_update(node, btree, tx);

	return 0;
}

/**
 * Delete the entry specified by @key.
 */
static int btree_delete_key(struct m0_be_btree   *btree,
			    struct m0_be_tx      *tx,
			    struct m0_be_bnode   *node,
			    void                 *key)
{
	unsigned int		 i;
	unsigned int		 index;
	struct m0_be_bnode	*rsibling;
	struct m0_be_bnode	*lsibling;
	struct m0_be_bnode	*parent = NULL;
	struct node_pos		 child;
	struct node_pos		 node_pos;
	int			 rc = -1;

	M0_PRE_EX(btree_invariant(btree));

	M0_ENTRY("n=%p", node);

del_loop:
	for (i = 0;; i = 0) {

		/* If there are no keys simply return */
		if (!node->b_nr_active)
			goto out;

		/*  Fix the index of the key greater than or equal */
		/*  to the key that we would like to search */

		while (i < node->b_nr_active &&
		       key_gt(btree, key, node->b_key_vals[i].key))
			i++;

		index = i;

		/*  Found? */
		if (i < node->b_nr_active &&
		    key_eq(btree, key, node->b_key_vals[i].key))
			break;

		if (node->b_leaf) /* No more to find */
			goto out;

		/* Store the parent */
		parent = node;

		node = node->b_children[i];

		/* Not found */
		if (node == NULL)
			goto out;

		if (i == parent->b_nr_active) {
			lsibling = parent->b_children[i - 1];
			rsibling = NULL;
		} else if (i == 0) {
			lsibling = NULL;
			rsibling = parent->b_children[i + 1];
		} else {
			lsibling = parent->b_children[i - 1];
			rsibling = parent->b_children[i + 1];
		}

		if (node->b_nr_active == BTREE_FAN_OUT - 1 && parent) {
			if (rsibling &&
			   (rsibling->b_nr_active > BTREE_FAN_OUT - 1)) {
				move_key(btree, tx, parent, i, P_LEFT);
			} else if (lsibling &&
			   (lsibling->b_nr_active > BTREE_FAN_OUT - 1)) {
				move_key(btree, tx, parent, i, P_RIGHT);
			} else if (lsibling &&
			   (lsibling->b_nr_active == BTREE_FAN_OUT - 1)) {
				M0_LOG(M0_DEBUG, "mergeL");
				node = merge_siblings(btree, tx, parent, i - 1);
			} else if (rsibling &&
			   (rsibling->b_nr_active == BTREE_FAN_OUT - 1)) {
				M0_LOG(M0_DEBUG, "mergeR");
				node = merge_siblings(btree, tx, parent, i);
			}
		}
	}

	M0_LOG(M0_DEBUG, "found node=%p lf=%d nr=%d idx=%d", node,
			!!node->b_leaf, node->b_nr_active, index);
	rc = 0;

	M0_ASSERT(ergo(node->b_leaf && node != btree->bb_root,
			node->b_nr_active > BTREE_FAN_OUT - 1));

	/* Case 1:
	 * The node containing the key is found and is the leaf node. */
	/* Also the leaf node has keys greater than the minimum required. */
	/* If the leaf node is the root permit deletion even if the */
	/* number of keys is less than (t - 1). */
	/* Simply remove the key */
	if (node->b_leaf && (node->b_nr_active > BTREE_FAN_OUT - 1 ||
	                     node == btree->bb_root)) {
		M0_LOG(M0_DEBUG, "case1");
		node_pos.p_node = node;
		node_pos.p_index = index;
		delete_key_from_node(btree, tx, &node_pos);
		goto out;
	} else {
		M0_ASSERT(!node->b_leaf);
	}

	/* Case 2:
	 * The node containing the key is found and is an internal node */
	M0_LOG(M0_DEBUG, "case2");
	if (node->b_children[index]->b_nr_active > BTREE_FAN_OUT - 1) {
		get_max_key_pos(btree, node->b_children[index], &child);
		M0_ASSERT(child.p_node->b_leaf);
		M0_LOG(M0_DEBUG, "swapR with n=%p i=%d", child.p_node,
							 child.p_index);
		M0_SWAP(child.p_node->b_key_vals[child.p_index],
			node->b_key_vals[index]);
		m0_format_footer_update(child.p_node);
		m0_format_footer_update(node);
		mem_update(btree, tx, &node->b_key_vals[index],
			   sizeof(node->b_key_vals[0]));
		node = node->b_children[index];
	} else if (node->b_children[index + 1]->b_nr_active >
		   BTREE_FAN_OUT - 1) {
		get_min_key_pos(btree, node->b_children[index + 1], &child);
		M0_ASSERT(child.p_node->b_leaf);
		M0_LOG(M0_DEBUG, "swapL with n=%p i=%d", child.p_node,
							 child.p_index);
		M0_SWAP(child.p_node->b_key_vals[child.p_index],
			node->b_key_vals[index]);
		m0_format_footer_update(child.p_node);
		m0_format_footer_update(node);
		mem_update(btree, tx, &node->b_key_vals[index],
			   sizeof(node->b_key_vals[0]));
		node = node->b_children[index + 1];
	} else {
		M0_LOG(M0_DEBUG, "case2-merge");
		node = merge_siblings(btree, tx, node, index);
	}
	goto del_loop;

out:
	M0_POST_EX(btree_invariant(btree));
	M0_LEAVE("rc=%d", rc);
	return M0_RC(rc);
}

static void node_push(struct m0_be_btree_cursor *it, struct m0_be_bnode *node,
				int idx)
{
	struct m0_be_btree_cursor_stack_entry *se;

	M0_ASSERT(it->bc_stack_pos < ARRAY_SIZE(it->bc_stack));
	se = &it->bc_stack[it->bc_stack_pos++];
	se->bs_node = node;
	se->bs_idx  = idx;
}

static struct m0_be_bnode *node_pop(struct m0_be_btree_cursor *it, int *idx)
{
	struct m0_be_bnode			*node = NULL;
	struct m0_be_btree_cursor_stack_entry	*se;

	if (it->bc_stack_pos > 0) {
		se = &it->bc_stack[--it->bc_stack_pos];
		node = se->bs_node;
		*idx = se->bs_idx;
	}
	return node;
}

/**
 * Function used to get the node containing the given key
 * @param btree The btree to be searched
 * @param key The the key to be searched
 * @return The node and position of the key within the node
 */
struct node_pos
get_btree_node(struct m0_be_btree_cursor *it, void *key, bool slant)
{
	struct m0_be_btree *btree = it->bc_tree;
	struct node_pos kp = { .p_node = NULL };
	struct m0_be_bnode *node;
	int i = 0;

	node = btree->bb_root;
	it->bc_stack_pos = 0;

	for (;; i = 0) {

		/*  Find the index of the key greater than or equal */
		/*  to the key that we would like to search */
		while (i < node->b_nr_active &&
		       key_gt(btree, key, node->b_key_vals[i].key)) {
			i++;
		}

		/*  If we find such key return the key-value pair */
		if (i < node->b_nr_active &&
		    key_eq(btree, key, node->b_key_vals[i].key)) {
			kp.p_node = node;
			kp.p_index = i;
			return kp;
		}
		/*  If the node is leaf and if we did not find the key */
		/*  return NULL */
		if (node->b_leaf) {
			while (node != NULL && i == node->b_nr_active)
				node = node_pop(it, &i);
			if (slant && node != NULL) {
				kp.p_node = node;
				kp.p_index = i;
			}
			return kp;
		}
		/*  Go to a child node */
		node_push(it, node, i);
		node = node->b_children[i];
	}
	return kp;
}

/**
 * Used to destory btree
 * @param btree The B-tree
 */
static void btree_destroy(struct m0_be_btree *btree, struct m0_be_tx *tx)
{
	int i = 0;
	struct m0_be_bnode *head, *tail, *node;
	struct m0_be_bnode *del_node;
	struct m0_be_bnode *child = NULL;

	node = btree->bb_root;
	head = node;
	tail = node;

	head->b_next = NULL;
	while (head != NULL) {
		if (!head->b_leaf) {
			for (i = 0; i < head->b_nr_active + 1; i++) {
				child = head->b_children[i];
				tail->b_next = child;
				m0_format_footer_update(tail);
				tail = child;
				child->b_next = NULL;
			}
			m0_format_footer_update(child);
		}
		del_node = head;
		head = head->b_next;
		for (i = 0; i < del_node->b_nr_active; i++)
			btree_pair_release(btree, tx, &del_node->b_key_vals[i]);
		btree_node_free(del_node, btree, tx);
	}
	m0_format_footer_update(head);
	btree_root_set(btree, NULL);
	mem_update(btree, tx, btree, sizeof(struct m0_be_btree));
}

/**
 * Truncate btree: delete all records and keep empty root node.
 *
 * That function can be called multiple times, having maximum number of records
 * to be deleted limited to not exceed transaction capacity.
 * After first call tree can't be used for operations other than truncate or
 * destroy.
 *
 * @param btee btree to truncate
 * @param tx transaction
 * @param limit maximum number of records to delete
 */
static void btree_truncate(struct m0_be_btree *btree, struct m0_be_tx *tx,
			   m0_bcount_t limit)

{
	struct m0_be_bnode *node;
	struct m0_be_bnode *parent;
	int                 i;

	/* Add one more reserve for non-leaf node. */
	if (limit > 1)
		limit--;

	node = btree->bb_root;

	while (node != NULL && limit > 0) {
		parent = NULL;
		if (!node->b_leaf) {
			parent = node;
			i = node->b_nr_active;
			node = node->b_children[i];
		}
		if (!node->b_leaf)
			continue;

		while (node->b_nr_active > 0 && limit > 0) {
			limit--;
			node->b_nr_active--;
			i = node->b_nr_active;
			btree_pair_release(btree, tx, &node->b_key_vals[i]);
		}
		m0_format_footer_update(node);
		if (node->b_nr_active > 0)
			continue;
		/*
		 * Cleared all keys in the leaf node.
		 */
		if (node == btree->bb_root) {
			/*
			 * Do not destroy tree root. Keep empty
			 * tree alive. So, we are done.
			 */
			break;
		}
		btree_node_free(node, btree, tx);
		if (parent != NULL) {
			/*
			 * If this is not a root (checked above), sure node has
			 * a parent.
			 * If parent is empty, reclassify it to a leaf.
			 */
			i = parent->b_nr_active;
			btree_pair_release(btree, tx, &parent->b_key_vals[i]);
			if (limit > 0)
				limit--;
			if (i == 0)
				parent->b_leaf = true;
			else
				parent->b_nr_active--;
			if (parent == btree->bb_root &&
			    parent->b_nr_active == 0) {
				/*
				 * Cleared the root, but still have 1
				 * child. Move the root.
				 */
				btree_root_set(btree, parent->b_children[0]);
				mem_update(btree, tx, btree,
					   sizeof(struct m0_be_btree));
				btree_node_free(parent, btree, tx);
			}
			m0_format_footer_update(parent);
			/* Simplify our life: restart from the root. */
			node = btree->bb_root;
		}
	}
	m0_format_footer_update(btree->bb_root);
}

/**
 * Function used to search a node in a B-Tree
 * @param btree The B-tree to be searched
 * @param key Key of the node to be search
 * @return The key-value pair
 */
static struct bt_key_val *btree_search(struct m0_be_btree *btree, void *key)
{

	struct m0_be_btree_cursor	 it;
	struct bt_key_val		*key_val = NULL;
	struct node_pos			 kp;

	it.bc_tree = btree;
	kp = get_btree_node(&it, key, false);

	if (kp.p_node)
		key_val = &kp.p_node->b_key_vals[kp.p_index];

	return key_val;
}

/**
 * Get the max key in the btree
 * @param btree The btree
 * @return The max key
 */
static void *btree_get_max_key(struct m0_be_btree *btree)
{
	struct node_pos node_pos;

	get_max_key_pos(btree, btree->bb_root, &node_pos);
	if (node_pos.p_node->b_nr_active == 0)
		return NULL;
	else
		return node_pos.p_node->b_key_vals[node_pos.p_index].key;
}

/**
 * Get the min key in the btree
 * @param btree The btree
 * @return The max key
 */
static void *btree_get_min_key(struct m0_be_btree *btree)
{
	struct node_pos node_pos;

	get_min_key_pos(btree, btree->bb_root, &node_pos);
	if (node_pos.p_node->b_nr_active == 0)
		return NULL;
	else
		return node_pos.p_node->b_key_vals[0].key;
}

static void btree_pair_release(struct m0_be_btree *btree, struct m0_be_tx *tx,
			       struct bt_key_val *kv)
{
	mem_free(btree, tx, kv->key);
}

/**
 * Inserts or updates value by key
 * @param tree The btree
 * @param tx The transaction
 * @param op The operation
 * @param key Key of the node to be searched
 * @param value Value to be copied
 * @param optype Save operation type: insert, update or overwrite
 * @param zonemask Bitmask of allowed allocation zones for memory allocation
 */
static void btree_save(struct m0_be_btree        *tree,
		       struct m0_be_tx           *tx,
		       struct m0_be_op           *op,
		       const struct m0_buf       *key,
		       const struct m0_buf       *val,
		       struct m0_be_btree_anchor *anchor,
		       enum btree_save_optype     optype,
		       uint64_t                   zonemask)
{
	m0_bcount_t        ksz;
	m0_bcount_t        vsz;
	struct bt_key_val  new_kv;
	struct bt_key_val *cur_kv;
	bool               val_overflow = false;

	M0_ENTRY("tree=%p", tree);

	M0_PRE(M0_IN(optype, (BTREE_SAVE_INSERT, BTREE_SAVE_UPDATE,
			      BTREE_SAVE_OVERWRITE)));

	switch (optype) {
		case BTREE_SAVE_OVERWRITE:
			M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_DELETE, tx);
			/* fallthrough */
		case BTREE_SAVE_INSERT:
			M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_INSERT, tx);
			break;
		case BTREE_SAVE_UPDATE:
			M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_UPDATE, tx);
			break;
	}

	btree_op_fill(op, tree, tx, optype == BTREE_SAVE_UPDATE ?
		      M0_BBO_UPDATE : M0_BBO_INSERT, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));
	if (anchor != NULL) {
		anchor->ba_tree = tree;
		anchor->ba_write = true;
		vsz = anchor->ba_value.b_nob;
		anchor->ba_value.b_addr = NULL;
	} else
		vsz = val->b_nob;

	if (M0_FI_ENABLED("already_exists"))
		goto fi_exist;

	op_tree(op)->t_rc = 0;
	cur_kv = btree_search(tree, key->b_addr);
	if ((cur_kv == NULL && optype != BTREE_SAVE_UPDATE) ||
	    (cur_kv != NULL && optype == BTREE_SAVE_UPDATE) ||
	    optype == BTREE_SAVE_OVERWRITE) {
		if (cur_kv != NULL && optype != BTREE_SAVE_INSERT) {
			if (vsz > be_btree_vsize(tree, cur_kv->val)) {
				/*
				 * The size of new value is greater than the
				 * size of old value, old value area can not be
				 * re-used for new value. Delete old key/value
				 * and add new key/value.
				 */
				op_tree(op)->t_rc =
					btree_delete_key(tree, tx,
							 tree->bb_root,
							 cur_kv->key);
				val_overflow = true;
			} else {
				/*
				 * The size of new value is less than or equal
				 * to the size of old value, simply rewrite
				 * old value in this case.
				 */
				if (val != NULL) {
					memcpy(cur_kv->val, val->b_addr,
					       val->b_nob);
					mem_update(tree, tx, cur_kv->val,
						   val->b_nob);
				} else
					anchor->ba_value.b_addr = cur_kv->val;
			}
		}

		if (op_tree(op)->t_rc == 0 &&
		    (cur_kv == NULL || val_overflow)) {
			/* Avoid CPU alignment overhead on values. */
			ksz = m0_align(key->b_nob, sizeof(void*));
			new_kv.key = mem_alloc(tree, tx, ksz + vsz, zonemask);
			new_kv.val = new_kv.key + ksz;
			memcpy(new_kv.key, key->b_addr, key->b_nob);
			memset(new_kv.key + key->b_nob, 0, ksz - key->b_nob);
			if (val != NULL) {
				memcpy(new_kv.val, val->b_addr, vsz);
				mem_update(tree, tx, new_kv.key, ksz + vsz);
			} else {
				mem_update(tree, tx, new_kv.key, ksz);
				anchor->ba_value.b_addr = new_kv.val;
			}

			btree_insert_key(tree, tx, &new_kv);
		}
	} else {
fi_exist:
		op_tree(op)->t_rc = -EEXIST;
		M0_LOG(M0_NOTICE, "the key entry at %p already exist",
			key->b_addr);
	}

	if (anchor == NULL)
		m0_rwlock_write_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
	M0_LEAVE("tree=%p", tree);
}


/* ------------------------------------------------------------------
 * Btree external interfaces implementation
 * ------------------------------------------------------------------ */

M0_INTERNAL void m0_be_btree_init(struct m0_be_btree *tree,
				  struct m0_be_seg   *seg,
				  const struct m0_be_btree_kv_ops *ops)
{
	M0_ENTRY("tree=%p seg=%p", tree, seg);
	M0_PRE(ops != NULL);

	m0_rwlock_init(btree_rwlock(tree));
	tree->bb_ops = ops;
	tree->bb_seg = seg;

	if (!m0_be_seg_contains(seg, tree->bb_root))
		tree->bb_root = NULL;

	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_fini(struct m0_be_btree *tree)
{
	M0_ENTRY("tree=%p", tree);
	m0_rwlock_fini(btree_rwlock(tree));
	M0_ASSERT(ergo(tree->bb_header.hd_magic,
			m0_format_footer_verify(tree) == 0));
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_create(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root == NULL && tree->bb_ops != NULL);
	/* M0_PRE(m0_rwlock_is_locked(tx->t_be.b_tx.te_lock)); */

	btree_op_fill(op, tree, tx, M0_BBO_CREATE, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	btree_create(tree, tx);

	m0_rwlock_write_unlock(btree_rwlock(tree));
	op_tree(op)->t_rc = 0;
	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_destroy(struct m0_be_btree *tree,
				     struct m0_be_tx *tx,
				     struct m0_be_op *op)
{
	M0_ENTRY("tree=%p", tree);
	/* XXX TODO The right approach to pursue is to let the user
	 * destroy only empty trees. So ideally here would be
	 * M0_PRE(m0_be_btree_is_empty(tree)); */
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, tx, M0_BBO_DESTROY, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	btree_destroy(tree, tx);

	m0_rwlock_write_unlock(btree_rwlock(tree));
	op_tree(op)->t_rc = 0;
	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_truncate(struct m0_be_btree *tree,
				      struct m0_be_tx    *tx,
				      struct m0_be_op    *op,
				      m0_bcount_t         limit)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, tx, M0_BBO_DESTROY, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	btree_truncate(tree, tx, limit);

	m0_rwlock_write_unlock(btree_rwlock(tree));
	op_tree(op)->t_rc = 0;
	m0_be_op_done(op);
	M0_LEAVE();
}

static void btree_node_alloc_credit(const struct m0_be_btree     *tree,
					  struct m0_be_tx_credit *accum)
{
	btree_mem_alloc_credit(tree, sizeof(struct m0_be_bnode), accum);
}

static void btree_node_update_credit(struct m0_be_tx_credit *accum,
				     m0_bcount_t nr)
{
	struct m0_be_tx_credit cred = {};

	/* struct m0_be_bnode update x2 */
	m0_be_tx_credit_mac(&cred,
			    &M0_BE_TX_CREDIT_TYPE(struct m0_be_bnode), 2);

	m0_be_tx_credit_mac(accum, &cred, nr);
}

static void btree_node_free_credit(const struct m0_be_btree     *tree,
					 struct m0_be_tx_credit *accum)
{
	btree_mem_free_credit(tree, sizeof(struct m0_be_bnode), accum);
	btree_node_update_credit(accum, 1); /* for parent */
}

/* XXX */
static void btree_credit(const struct m0_be_btree     *tree,
			       struct m0_be_tx_credit *accum)
{
	uint32_t height;

	height = tree->bb_root == NULL ? 2 : tree->bb_root->b_level;
	m0_be_tx_credit_mul(accum, 2*height + 1);
}

static void btree_rebalance_credit(const struct m0_be_btree *tree,
				   struct m0_be_tx_credit   *accum)
{
	struct m0_be_tx_credit cred = {};

	btree_node_alloc_credit(tree, &cred);
	btree_node_update_credit(&cred, 1);
	btree_credit(tree, &cred);

	m0_be_tx_credit_add(accum, &cred);
}

static void kv_insert_credit(const struct m0_be_btree     *tree,
				   m0_bcount_t             ksize,
				   m0_bcount_t             vsize,
				   struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit kv_update_cred;

	ksize = m0_align(ksize, sizeof(void*));
	kv_update_cred = M0_BE_TX_CREDIT(1, ksize + vsize);
	btree_mem_alloc_credit(tree, ksize + vsize, accum);
	m0_be_tx_credit_add(accum, &kv_update_cred);
}

static void kv_delete_credit(const struct m0_be_btree     *tree,
				   m0_bcount_t             ksize,
				   m0_bcount_t             vsize,
				   struct m0_be_tx_credit *accum)
{
	btree_mem_free_credit(tree, m0_align(ksize, sizeof(void*)) + vsize,
			      accum);
	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_TYPE(struct bt_key_val));
}

static void btree_node_split_child_credit(const struct m0_be_btree *tree,
					  struct m0_be_tx_credit   *accum)
{
	btree_node_alloc_credit(tree, accum);
	btree_node_update_credit(accum, 3);
}

static void insert_credit(const struct m0_be_btree *tree,
			  m0_bcount_t               nr,
			  m0_bcount_t               ksize,
			  m0_bcount_t               vsize,
			  struct m0_be_tx_credit   *accum,
			  bool                      use_current_height)
{
	struct m0_be_tx_credit cred = {};
	uint32_t               height;

	if (use_current_height)
		height = tree->bb_root == NULL ? 2 : tree->bb_root->b_level;
	else
		height = BTREE_HEIGHT_MAX;

	/* for btree_insert_nonfull() */
	btree_node_split_child_credit(tree, &cred);
	m0_be_tx_credit_mul(&cred, height);
	btree_node_update_credit(&cred, 1);

	/* for btree_insert_key() */
	btree_node_alloc_credit(tree, &cred);
	btree_node_split_child_credit(tree, &cred);
	m0_be_tx_credit_add(&cred,
			    &M0_BE_TX_CREDIT(1, sizeof(struct m0_be_btree)));

	kv_insert_credit(tree, ksize, vsize, &cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

static void delete_credit(const struct m0_be_btree *tree,
			  m0_bcount_t               nr,
			  m0_bcount_t               ksize,
			  m0_bcount_t               vsize,
			  struct m0_be_tx_credit   *accum)
{
	struct m0_be_tx_credit cred = {};

	kv_delete_credit(tree, ksize, vsize, &cred);
	btree_node_update_credit(&cred, 1);
	btree_node_free_credit(tree, &cred);
	btree_rebalance_credit(tree, &cred);
	m0_be_tx_credit_mac(accum, &cred, nr);
}


M0_INTERNAL void m0_be_btree_insert_credit(const struct m0_be_btree *tree,
					   m0_bcount_t               nr,
					   m0_bcount_t               ksize,
					   m0_bcount_t               vsize,
					   struct m0_be_tx_credit   *accum)
{
	insert_credit(tree, nr, ksize, vsize, accum, false);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_INSERT, accum);
}

M0_INTERNAL void m0_be_btree_insert_credit2(const struct m0_be_btree *tree,
					    m0_bcount_t               nr,
					    m0_bcount_t               ksize,
					    m0_bcount_t               vsize,
					    struct m0_be_tx_credit   *accum)
{
	insert_credit(tree, nr, ksize, vsize, accum, true);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_INSERT, accum);
}

M0_INTERNAL void m0_be_btree_delete_credit(const struct m0_be_btree     *tree,
						 m0_bcount_t             nr,
						 m0_bcount_t             ksize,
						 m0_bcount_t             vsize,
						 struct m0_be_tx_credit *accum)
{
	delete_credit(tree, nr, ksize, vsize, accum);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_DELETE, accum);
}

M0_INTERNAL void m0_be_btree_update_credit(const struct m0_be_btree     *tree,
						 m0_bcount_t             nr,
						 m0_bcount_t             vsize,
						 struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = {};
	struct m0_be_tx_credit val_update_cred =
		M0_BE_TX_CREDIT(1, vsize + sizeof(struct bt_key_val));

	/* @todo: is alloc/free credits are really needed??? */
	btree_mem_alloc_credit(tree, vsize, &cred);
	btree_mem_free_credit(tree, vsize, &cred);
	m0_be_tx_credit_add(&cred, &val_update_cred);
	m0_be_tx_credit_mac(accum, &cred, nr);

	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_UPDATE, accum);
}

M0_INTERNAL void m0_be_btree_update_credit2(const struct m0_be_btree *tree,
					    m0_bcount_t               nr,
					    m0_bcount_t               ksize,
					    m0_bcount_t               vsize,
					    struct m0_be_tx_credit   *accum)
{
	delete_credit(tree, nr, ksize, vsize, accum);
	insert_credit(tree, nr, ksize, vsize, accum, true);
	M0_BE_CREDIT_INC(nr, M0_BE_CU_BTREE_UPDATE, accum);
}

M0_INTERNAL void m0_be_btree_create_credit(const struct m0_be_btree     *tree,
						 m0_bcount_t             nr,
						 struct m0_be_tx_credit *accum)
{
	struct m0_be_tx_credit cred = {};

	btree_node_alloc_credit(tree, &cred);
	btree_node_update_credit(&cred, 1);
	m0_be_tx_credit_mac(accum, &cred, nr);
}

static int btree_count_items(struct m0_be_btree *tree, m0_bcount_t *ksize,
			     m0_bcount_t *vsize)
{
	struct m0_be_btree_cursor cur;
	struct m0_be_op          *op = &cur.bc_op;
	struct m0_buf             start;
	int                       count = 0;
	struct m0_buf             key;
	struct m0_buf             val;
	int                       rc;

	*ksize = 0;
	*vsize = 0;
	if (tree->bb_root != NULL) {
		m0_be_btree_cursor_init(&cur, tree);

		M0_SET0(op);
		M0_BE_OP_SYNC_WITH(op, m0_be_btree_minkey(tree, op, &start));

		rc = m0_be_btree_cursor_get_sync(&cur, &start, true);

		while (rc != -ENOENT) {
			m0_be_btree_cursor_kv_get(&cur, &key, &val);
			if (key.b_nob > *ksize)
				*ksize = key.b_nob;
			if (val.b_nob > *vsize)
				*vsize = val.b_nob;
			rc = m0_be_btree_cursor_next_sync(&cur);
			++count;
		}

		m0_be_btree_cursor_fini(&cur);
	}

	return count;
}

M0_INTERNAL void m0_be_btree_destroy_credit(struct m0_be_btree     *tree,
					    struct m0_be_tx_credit *accum)
{
	/* XXX
	 * Current implementation of m0_be_btree_destroy_credit() is
	 * not right. First of all, `tree' parameter must be const.
	 * Secondly, it is user's responsibility to ensure that the
	 * tree being deleted is empty.
	 */
	struct m0_be_tx_credit cred = {};
	int		       nodes_nr;
	int		       items_nr;
	m0_bcount_t	       ksize;
	m0_bcount_t	       vsize;

	nodes_nr = iter_prepare(tree->bb_root, false);
	items_nr = btree_count_items(tree, &ksize, &vsize);
	M0_LOG(M0_DEBUG, "nodes=%d items=%d ksz=%d vsz%d",
		nodes_nr, items_nr, (int)ksize, (int)vsize);

	kv_delete_credit(tree, ksize, vsize, &cred);
	m0_be_tx_credit_mac(accum, &cred, items_nr);

	M0_SET0(&cred);
	btree_node_free_credit(tree, &cred);
	m0_be_tx_credit_mac(accum, &cred, nodes_nr);

	cred = M0_BE_TX_CREDIT_TYPE(struct m0_be_btree);
	m0_be_tx_credit_add(accum, &cred);
}

M0_INTERNAL void m0_be_btree_clear_credit(struct m0_be_btree     *tree,
					  struct m0_be_tx_credit *fixed_part,
					  struct m0_be_tx_credit *single_record,
					  m0_bcount_t            *records_nr)
{
	struct m0_be_tx_credit cred = {};
	int                    nodes_nr;
	int                    items_nr;
	m0_bcount_t            ksize;
	m0_bcount_t            vsize;

	nodes_nr = iter_prepare(tree->bb_root, false);
	items_nr = btree_count_items(tree, &ksize, &vsize);
	items_nr++;
	M0_LOG(M0_DEBUG, "nodes=%d items=%d ksz=%d vsz%d",
		nodes_nr, items_nr, (int)ksize, (int)vsize);

	M0_SET0(single_record);
	kv_delete_credit(tree, ksize, vsize, single_record);
	*records_nr = items_nr;

	M0_SET0(&cred);
	btree_node_free_credit(tree, &cred);
	m0_be_tx_credit_mac(fixed_part, &cred, nodes_nr);

	cred = M0_BE_TX_CREDIT_TYPE(struct m0_be_btree);
	m0_be_tx_credit_add(fixed_part, &cred);
	m0_be_tx_credit_add(fixed_part, single_record);
}

static void be_btree_insert(struct m0_be_btree        *tree,
			    struct m0_be_tx           *tx,
			    struct m0_be_op           *op,
			    const struct m0_buf       *key,
			    const struct m0_buf       *val,
			    struct m0_be_btree_anchor *anchor,
			    uint64_t                   zonemask)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));
	M0_PRE((val != NULL) == (anchor == NULL));
	M0_PRE(ergo(anchor == NULL,
		    val->b_nob == be_btree_vsize(tree, val->b_addr)));

	btree_save(tree, tx, op, key, val, anchor, BTREE_SAVE_INSERT, zonemask);

	M0_LEAVE("tree=%p", tree);
}

M0_INTERNAL void m0_be_btree_save(struct m0_be_btree  *tree,
				  struct m0_be_tx     *tx,
				  struct m0_be_op     *op,
				  const struct m0_buf *key,
				  const struct m0_buf *val,
				  bool                 overwrite)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));
	M0_PRE(val->b_nob == be_btree_vsize(tree, val->b_addr));

	/* We can't be here during DIX Repair, so never use repair zone. */
	btree_save(tree, tx, op, key, val, NULL, overwrite ?
		   BTREE_SAVE_OVERWRITE : BTREE_SAVE_INSERT,
		   M0_BITS(M0_BAP_NORMAL));

	M0_LEAVE("tree=%p", tree);
}

M0_INTERNAL void m0_be_btree_insert(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    const struct m0_buf *val)
{
	be_btree_insert(tree, tx, op, key, val, NULL, M0_BITS(M0_BAP_NORMAL));
}

M0_INTERNAL void m0_be_btree_update(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    const struct m0_buf *val)
{
	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));
	M0_PRE(val->b_nob == be_btree_vsize(tree, val->b_addr));

	btree_save(tree, tx, op, key, val, NULL, BTREE_SAVE_UPDATE,
		   M0_BITS(M0_BAP_NORMAL));

	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_delete(struct m0_be_btree *tree,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    const struct m0_buf *key)
{
	int rc;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	M0_BE_CREDIT_DEC(M0_BE_CU_BTREE_DELETE, tx);

	btree_op_fill(op, tree, tx, M0_BBO_DELETE, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	op_tree(op)->t_rc = rc = btree_delete_key(tree, tx, tree->bb_root,
						  key->b_addr);
	if (rc != 0)
		op_tree(op)->t_rc = -ENOENT;

	m0_rwlock_write_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
	M0_LEAVE("tree=%p", tree);
}

M0_INTERNAL void m0_be_btree_lookup(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    const struct m0_buf *key,
				    struct m0_buf *dest_value)
{
	struct bt_key_val *kv;
	m0_bcount_t        vsize;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_LOOKUP, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	kv = btree_search(tree, key->b_addr);
	if (kv != NULL) {
		vsize = be_btree_vsize(tree, kv->val);
		if (vsize < dest_value->b_nob)
			dest_value->b_nob = vsize;
		/* XXX handle vsize > dest_value->b_nob */
		memcpy(dest_value->b_addr, kv->val, dest_value->b_nob);
		op_tree(op)->t_rc = 0;
	} else
		op_tree(op)->t_rc = -ENOENT;

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_maxkey(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    struct m0_buf *out)
{
	void *key;

	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_MAXKEY, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	key = btree_get_max_key(tree);
	op_tree(op)->t_rc = key == NULL ? -ENOENT : 0;
	m0_buf_init(out, key, key == NULL ? 0 : be_btree_ksize(tree, key));

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_btree_minkey(struct m0_be_btree *tree,
				    struct m0_be_op *op,
				    struct m0_buf *out)
{
	void *key;

	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_MINKEY, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	key = btree_get_min_key(tree);
	op_tree(op)->t_rc = key == NULL ? -ENOENT : 0;
	m0_buf_init(out, key, key == NULL ? 0 : be_btree_ksize(tree, key));

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

/* ------------------------------------------------------------------
 * Btree external inplace interfaces implementation
 * ------------------------------------------------------------------ */

M0_INTERNAL void m0_be_btree_update_inplace(struct m0_be_btree        *tree,
					    struct m0_be_tx           *tx,
					    struct m0_be_op           *op,
					    const struct m0_buf       *key,
					    struct m0_be_btree_anchor *anchor)
{
	struct bt_key_val *kv;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));

	btree_op_fill(op, tree, tx, M0_BBO_UPDATE, NULL);

	m0_be_op_active(op);
	m0_rwlock_write_lock(btree_rwlock(tree));

	anchor->ba_write = true;
	anchor->ba_tree  = tree;
	kv = btree_search(tree, key->b_addr);
	if (kv != NULL) {
		M0_ASSERT(anchor->ba_value.b_nob <=
			  be_btree_vsize(tree, kv->val));
		anchor->ba_value.b_addr = kv->val;
	} else
		op_tree(op)->t_rc = -ENOENT;

	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_insert_inplace(struct m0_be_btree        *tree,
					    struct m0_be_tx           *tx,
					    struct m0_be_op           *op,
					    const struct m0_buf       *key,
					    struct m0_be_btree_anchor *anchor,
					    uint64_t                   zonemask)
{
	be_btree_insert(tree, tx, op, key, NULL, anchor, zonemask);
}

M0_INTERNAL void m0_be_btree_save_inplace(struct m0_be_btree        *tree,
					  struct m0_be_tx           *tx,
					  struct m0_be_op           *op,
					  const struct m0_buf       *key,
					  struct m0_be_btree_anchor *anchor,
					  bool                       overwrite,
					  uint64_t                   zonemask)
{
	M0_ENTRY("tree=%p zonemask=%"PRIx64, tree, zonemask);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);
	M0_PRE(key->b_nob == be_btree_ksize(tree, key->b_addr));

	btree_save(tree, tx, op, key, NULL, anchor, overwrite ?
		   BTREE_SAVE_OVERWRITE : BTREE_SAVE_INSERT, zonemask);
}

M0_INTERNAL void m0_be_btree_lookup_inplace(struct m0_be_btree        *tree,
					    struct m0_be_op           *op,
					    const struct m0_buf       *key,
					    struct m0_be_btree_anchor *anchor)
{
	struct bt_key_val *kv;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(tree->bb_root != NULL && tree->bb_ops != NULL);

	btree_op_fill(op, tree, NULL, M0_BBO_INSERT, anchor);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	anchor->ba_tree = tree;
	anchor->ba_write = false;
	kv = btree_search(tree, key->b_addr);
	if (kv == NULL)
		op_tree(op)->t_rc = -ENOENT;
	else
		m0_buf_init(&anchor->ba_value, kv->val,
			    be_btree_vsize(tree, kv->val));

	m0_be_op_done(op);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_btree_release(struct m0_be_tx           *tx,
				     struct m0_be_btree_anchor *anchor)
{
	struct m0_be_btree *tree = anchor->ba_tree;

	M0_ENTRY("tree=%p", tree);
	M0_PRE(ergo(anchor->ba_write, tx != NULL));

	if (tree != NULL) {
		if (anchor->ba_write) {
			if (anchor->ba_value.b_addr != NULL) {
				mem_update(tree, tx, anchor->ba_value.b_addr,
					   anchor->ba_value.b_nob);
				anchor->ba_value.b_addr = NULL;
			}
			m0_rwlock_write_unlock(btree_rwlock(tree));
		} else
			m0_rwlock_read_unlock(btree_rwlock(tree));
		anchor->ba_tree = NULL;
	}
	M0_LEAVE();
}

/* ------------------------------------------------------------------
 * Btree cursor interfaces implementation
 * ------------------------------------------------------------------ */

static void print_single_node(struct m0_be_bnode *node)
{
	int i;

	M0_LOG(M0_DEBUG, "{");
	for (i = 0; i < node->b_nr_active; ++i) {
		void *key = node->b_key_vals[i].key;
		void *val = node->b_key_vals[i].val;

		if (node->b_leaf)
			M0_LOG(M0_DEBUG, "%02d: key=%s val=%s", i,
			       (char *)key, (char *)val);
		else
			M0_LOG(M0_DEBUG, "%02d: key=%s val=%s child=%p", i,
			       (char *)key, (char *)val, node->b_children[i]);
	}
	if (!node->b_leaf)
		M0_LOG(M0_DEBUG, "%02d: child=%p", i, node->b_children[i]);
	M0_LOG(M0_DEBUG, "} (%p, %d)", node, node->b_level);
}

static int iter_prepare(struct m0_be_bnode *node, bool print)
{

	int		 i = 0;
	int		 count = 0;
	unsigned int	 current_level;

	struct m0_be_bnode *head;
	struct m0_be_bnode *tail;
	struct m0_be_bnode *child = NULL;

	if (print)
		M0_LOG(M0_DEBUG, "---8<---8<---8<---8<---8<---8<---");

	if (node == NULL)
		goto out;

	count = 1;
	current_level = node->b_level;
	head = node;
	tail = node;

	head->b_next = NULL;
	while (head != NULL) {
		if (head->b_level < current_level) {
			current_level = head->b_level;
			if (print)
				M0_LOG(M0_DEBUG, "***");
		}
		if (print)
			print_single_node(head);

		if (!head->b_leaf) {
			for (i = 0; i < head->b_nr_active + 1; i++) {
				child = head->b_children[i];
				tail->b_next = child;
				m0_format_footer_update(tail);
				tail = child;
				child->b_next = NULL;
			}
			m0_format_footer_update(child);
		}
		head = head->b_next;
		count++;
	}
	m0_format_footer_update(head);
out:
	if (print)
		M0_LOG(M0_DEBUG, "---8<---8<---8<---8<---8<---8<---");

	return count;
}

M0_INTERNAL void m0_be_btree_cursor_init(struct m0_be_btree_cursor *cur,
					 struct m0_be_btree *btree)
{
	cur->bc_tree = btree;
	cur->bc_node = NULL;
	cur->bc_pos = 0;
	cur->bc_stack_pos = 0;
}

M0_INTERNAL void m0_be_btree_cursor_fini(struct m0_be_btree_cursor *cursor)
{
	cursor->bc_tree = NULL;
}

M0_INTERNAL void m0_be_btree_cursor_get(struct m0_be_btree_cursor *cur,
					const struct m0_buf *key, bool slant)
{
	struct node_pos     last;
	struct bt_key_val  *kv;
	struct m0_be_op    *op   = &cur->bc_op;
	struct m0_be_btree *tree = cur->bc_tree;

	btree_op_fill(op, tree, NULL, M0_BBO_CURSOR_GET, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	last = get_btree_node(cur, key->b_addr, slant);

	if (last.p_node == NULL) {
		M0_SET0(&op_tree(op)->t_out_val);
		M0_SET0(&op_tree(op)->t_out_key);
		op_tree(op)->t_rc = -ENOENT;
	} else {
		cur->bc_pos  = last.p_index;
		cur->bc_node = last.p_node;

		kv = &cur->bc_node->b_key_vals[cur->bc_pos];

		m0_buf_init(&op_tree(op)->t_out_val, kv->val,
			    be_btree_vsize(tree, kv->val));
		m0_buf_init(&op_tree(op)->t_out_key, kv->key,
			    be_btree_ksize(tree, kv->key));
		op_tree(op)->t_rc = 0;
	}

	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL int m0_be_btree_cursor_get_sync(struct m0_be_btree_cursor *cur,
					    const struct m0_buf *key,
					    bool slant)
{
	M0_SET0(&cur->bc_op);
	return M0_RC(M0_BE_OP_SYNC_RET_WITH(&cur->bc_op,
			      m0_be_btree_cursor_get(cur, key, slant),
			      bo_u.u_btree.t_rc));
}

static int btree_cursor_seek(struct m0_be_btree_cursor *cur, void *key)
{
	const struct m0_buf kbuf =
		M0_BUF_INIT(be_btree_ksize(cur->bc_tree, key), key);
	return m0_be_btree_cursor_get_sync(cur, &kbuf, true);
}

M0_INTERNAL int m0_be_btree_cursor_first_sync(struct m0_be_btree_cursor *cur)
{
	return btree_cursor_seek(cur, btree_get_min_key(cur->bc_tree));
}

M0_INTERNAL int m0_be_btree_cursor_last_sync(struct m0_be_btree_cursor *cur)
{
	return btree_cursor_seek(cur, btree_get_max_key(cur->bc_tree));
}

M0_INTERNAL void m0_be_btree_cursor_next(struct m0_be_btree_cursor *cur)
{
	struct bt_key_val  *kv;
	struct m0_be_op    *op   = &cur->bc_op;
	struct m0_be_btree *tree = cur->bc_tree;
	struct m0_be_bnode *node;

	btree_op_fill(op, tree, NULL, M0_BBO_CURSOR_NEXT, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	node = cur->bc_node;
	if (node == NULL) {
		op_tree(op)->t_rc = -EINVAL;
		goto out;
	}

	/* cursor move */
	++cur->bc_pos;
	if (node->b_leaf) {
		while (node && cur->bc_pos >= node->b_nr_active)
			node = node_pop(cur, &cur->bc_pos);
	} else {
		for (;;) {
			node_push(cur, node, cur->bc_pos);
			node = node->b_children[cur->bc_pos];
			cur->bc_pos = 0;
			if (node->b_leaf)
				break;
		}
	}

	if (node == NULL) {
		M0_SET0(&op_tree(op)->t_out_val);
		M0_SET0(&op_tree(op)->t_out_key);
		op_tree(op)->t_rc = -ENOENT;
		goto out;
	}
	/* cursor end move */

	cur->bc_node = node;

	kv = &node->b_key_vals[cur->bc_pos];
	m0_buf_init(&op_tree(op)->t_out_val, kv->val,
		    be_btree_vsize(tree, kv->val));
	m0_buf_init(&op_tree(op)->t_out_key, kv->key,
		    be_btree_ksize(tree, kv->key));
out:
	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_btree_cursor_prev(struct m0_be_btree_cursor *cur)
{
	struct bt_key_val  *kv;
	struct m0_be_op    *op   = &cur->bc_op;
	struct m0_be_btree *tree = cur->bc_tree;
	struct m0_be_bnode *node;

	btree_op_fill(op, tree, NULL, M0_BBO_CURSOR_PREV, NULL);

	m0_be_op_active(op);
	m0_rwlock_read_lock(btree_rwlock(tree));

	node = cur->bc_node;

	/* cursor move */
	if (node->b_leaf) {
		--cur->bc_pos;
		while (node && cur->bc_pos < 0) {
			node = node_pop(cur, &cur->bc_pos);
			--cur->bc_pos;
		}
	} else {
		for (;;) {
			node_push(cur, node, cur->bc_pos);
			node = node->b_children[cur->bc_pos];
			if (node->b_leaf) {
				cur->bc_pos = node->b_nr_active - 1;
				break;
			} else
				cur->bc_pos = node->b_nr_active;
		}
	}

	if (node == NULL) {
		M0_SET0(&op_tree(op)->t_out_val);
		M0_SET0(&op_tree(op)->t_out_key);
		op_tree(op)->t_rc = -ENOENT;
		goto out;
	}
	/* cursor end move */

	cur->bc_node = node;

	kv = &cur->bc_node->b_key_vals[cur->bc_pos];
	m0_buf_init(&op_tree(op)->t_out_val, kv->val,
		    be_btree_vsize(tree, kv->val));
	m0_buf_init(&op_tree(op)->t_out_key, kv->key,
		    be_btree_ksize(tree, kv->key));
out:
	m0_rwlock_read_unlock(btree_rwlock(tree));
	m0_be_op_done(op);
}

M0_INTERNAL int m0_be_btree_cursor_next_sync(struct m0_be_btree_cursor *cur)
{
	M0_SET0(&cur->bc_op);
	return M0_BE_OP_SYNC_RET_WITH(&cur->bc_op,
				      m0_be_btree_cursor_next(cur),
				      bo_u.u_btree.t_rc);
}

M0_INTERNAL int m0_be_btree_cursor_prev_sync(struct m0_be_btree_cursor *cur)
{
	M0_SET0(&cur->bc_op);
	return M0_BE_OP_SYNC_RET_WITH(&cur->bc_op,
				      m0_be_btree_cursor_prev(cur),
				      bo_u.u_btree.t_rc);
}

M0_INTERNAL void m0_be_btree_cursor_put(struct m0_be_btree_cursor *cursor)
{
	cursor->bc_node = NULL;
}

M0_INTERNAL void m0_be_btree_cursor_kv_get(struct m0_be_btree_cursor *cur,
					   struct m0_buf *key,
					   struct m0_buf *val)
{
	struct m0_be_op *op = &cur->bc_op;
	M0_PRE(m0_be_op_is_done(op));
	M0_PRE(key != NULL || val != NULL);

	if (key != NULL)
		*key = op_tree(op)->t_out_key;
	if (val != NULL)
		*val = op_tree(op)->t_out_val;
}

M0_INTERNAL bool m0_be_btree_is_empty(struct m0_be_btree *tree)
{
	M0_PRE(tree->bb_root != NULL);
	return tree->bb_root->b_nr_active == 0;
}

M0_INTERNAL void btree_dbg_print(struct m0_be_btree *tree)
{
	iter_prepare(tree->bb_root, true);
}

static struct m0_be_op__btree *op_tree(struct m0_be_op *op)
{
	M0_PRE(op->bo_utype == M0_BOP_TREE);
	return &op->bo_u.u_btree;
}

static struct m0_rwlock *btree_rwlock(struct m0_be_btree *tree)
{
	return &tree->bb_lock.bl_u.rwlock;
}

/** @} end of be group */
#undef M0_TRACE_SUBSYSTEM

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
