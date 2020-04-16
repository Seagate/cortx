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
 * Original creation date: 12-Jan-15
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FD
#include "lib/trace.h"

#include "fd/fd.h"
#include "fd/fd_internal.h"

#include "conf/walk.h"
#include "conf/obj.h"
#include "conf/obj_ops.h"      /* M0_CONF_DIRNEXT */
#include "conf/dir.h"          /* m0_conf_dir_len */
#include "conf/diter.h"        /* m0_conf_diter */
#include "conf/confc.h"        /* m0_confc_from_obj */
#include "pool/pool_machine.h" /* m0_poolmach */
#include "pool/pool.h"

#include "fid/fid.h"           /* m0_fid_eq m0_fid_set */
#include "lib/errno.h"         /* EINVAL */
#include "lib/memory.h"        /* M0_ALLOC_ARR M0_ALLOC_PTR m0_free */
#include "lib/arith.h"         /* m0_gcd64 m0_enc m0_dec */
#include "lib/hash.h"          /* m0_hash */

/*
 * A structure that summarizes information about a level
 * in pver subtree.
 */
struct pv_subtree_info {
	uint32_t psi_level;
	uint32_t psi_nr_objs;
};

static uint64_t parity_group_size(const struct m0_pdclust_attr *la_attr);

/**
 * Maps an index from the base permutation to appropriate target index
 * in a fault tolerant permutation.
 */
static uint64_t fault_tolerant_idx_get(uint64_t idx, uint64_t *children_nr,
				       uint64_t depth);

/**
 * Fetches the attributes associated with the symmetric tree from a
 * pool version.
 */
static int symm_tree_attr_get(const struct m0_conf_pver *pv, uint32_t *depth,
			      uint64_t *children_nr);

static inline bool fd_tile_invariant(const struct m0_fd_tile *tile);

/**
 * Checks the feasibility of expected tolerance using the attributes of
 * the symmetric tree formed from the pool version.
 */
static int tolerance_check(const struct m0_conf_pver *pv,
			   uint64_t *children_nr,
			   uint32_t first_level, uint32_t *failure_level);

/**
 * Uniformly distributes units from a parent node to all the children.
 */
static void uniform_distribute(uint64_t **units, uint64_t level,
			       uint64_t parent_nr, uint64_t child_nr);

/**
 * Calculates the sum of the units received by the first 'toll' number of
 * nodes from a level 'level', when nodes are arranged in descending order
 * of units they possess.
 */
static uint64_t units_calc(uint64_t **units, uint64_t level, uint64_t parent_nr,
			   uint64_t child_nr, uint64_t tol);


/** Calculates the pool-width associated with the symmetric tree. */
static uint64_t pool_width_calc(uint64_t *children_nr, uint64_t depth);


/** Returns an index of permuted target. **/
static void permuted_tgt_get(struct m0_pdclust_instance *pi, uint64_t omega,
			     uint64_t *rel_vidx, uint64_t *tgt_idx);

/** Returns relative indices from a symmetric tree. **/
static void inverse_permuted_idx_get(struct m0_pdclust_instance *pi,
				     uint64_t omega, uint64_t perm_idx,
				     uint64_t *rel_idx);
/**
 * Returns the permutation cache associated with a node from the pdclust
 * instance.
 */
static struct m0_fd_perm_cache *cache_get(struct m0_pdclust_instance *pi,
					  struct m0_fd_tree_node *node);
/** Permutes the permutation cache. **/
static void fd_permute(struct m0_fd_perm_cache *cache,
		       struct m0_uint128 *seed, struct m0_fid *gfid,
		       uint64_t omega);
/**
 *  Checks if a given count is present in cache_info, and adds it in case
 *  it is absent.
 */
static void cache_info_update(struct m0_fd_cache_info *cache_info,
			      uint64_t cnt);
static bool is_cache_valid(const struct m0_fd_perm_cache *cache,
			   uint64_t omega, const struct m0_fid *gfid);

static uint64_t tree2pv_level_conv(uint64_t level, uint64_t tree_depth);

static bool is_objv(const struct m0_conf_obj *obj,
		    const struct m0_conf_obj_type *type)
{
	return m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE &&
		(type == NULL ||
		 m0_conf_obj_type(M0_CONF_CAST(obj,
					       m0_conf_objv)->cv_real) ==
					       type);
}

static bool is_objv_site(const struct m0_conf_obj *obj)
{
	return is_objv(obj, &M0_CONF_SITE_TYPE);
}

static bool is_objv_rack(const struct m0_conf_obj *obj)
{
	return is_objv(obj, &M0_CONF_RACK_TYPE);
}

static bool is_objv_encl(const struct m0_conf_obj *obj)
{
	return is_objv(obj, &M0_CONF_ENCLOSURE_TYPE);
}

static bool is_objv_ctrl(const struct m0_conf_obj *obj)
{
	return is_objv(obj, &M0_CONF_CONTROLLER_TYPE);
}

static bool is_objv_disk(const struct m0_conf_obj *obj)
{
	return is_objv(obj, &M0_CONF_DRIVE_TYPE);
}

static bool
(*is_obj_at_level[M0_CONF_PVER_HEIGHT])(const struct m0_conf_obj *obj) = {
	is_objv_site,
	is_objv_rack,
	is_objv_encl,
	is_objv_ctrl,
	is_objv_disk
};

#define pv_for(pv, level, obj, rc)                                         \
({                                                                         \
	struct m0_confc      *__confc;                                     \
	struct m0_conf_pver  *__pv    = (struct m0_conf_pver *)(pv);       \
	uint64_t              __level = (level);                           \
	struct m0_conf_diter  __it;                                        \
	struct m0_fid conf_path[M0_CONF_PVER_HEIGHT][M0_CONF_PVER_HEIGHT] = { \
		{ M0_CONF_PVER_SITEVS_FID },                               \
		{ M0_CONF_PVER_SITEVS_FID, M0_CONF_SITEV_RACKVS_FID },     \
		{ M0_CONF_PVER_SITEVS_FID, M0_CONF_SITEV_RACKVS_FID,       \
		  M0_CONF_RACKV_ENCLVS_FID },                              \
		{ M0_CONF_PVER_SITEVS_FID, M0_CONF_SITEV_RACKVS_FID,       \
		  M0_CONF_RACKV_ENCLVS_FID, M0_CONF_ENCLV_CTRLVS_FID },    \
		{ M0_CONF_PVER_SITEVS_FID, M0_CONF_SITEV_RACKVS_FID,       \
		  M0_CONF_RACKV_ENCLVS_FID, M0_CONF_ENCLV_CTRLVS_FID,      \
		  M0_CONF_CTRLV_DRIVEVS_FID },                             \
	 };                                                                \
	__confc = (struct m0_confc *)m0_confc_from_obj(&__pv->pv_obj);     \
	M0_ASSERT(__confc != NULL);                                        \
	rc = m0_conf__diter_init(&__it, __confc, &__pv->pv_obj,            \
				 __level + 1, conf_path[__level]);         \
	while (rc >= 0  &&                                                 \
	       (rc = m0_conf_diter_next_sync(&__it,                        \
					     is_obj_at_level[__level])) != \
		M0_CONF_DIRNEXT) {;}                                       \
	for (obj = m0_conf_diter_result(&__it);                            \
	     rc > 0 && (obj = m0_conf_diter_result(&__it));                \
	     rc = m0_conf_diter_next_sync(&__it,                           \
					  is_obj_at_level[__level])) {     \

#define pv_endfor } if (rc >= 0) m0_conf_diter_fini(&__it); })


M0_INTERNAL int m0_fd__tile_init(struct m0_fd_tile *tile,
				 const struct m0_pdclust_attr *la_attr,
				 uint64_t *children, uint64_t depth)
{
	M0_PRE(tile != NULL && la_attr != NULL && children != NULL);
	M0_PRE(depth > 0);

	tile->ft_G     = parity_group_size(la_attr);
	tile->ft_cols  = pool_width_calc(children, depth);
	tile->ft_rows  = tile->ft_G / m0_gcd64(tile->ft_G, tile->ft_cols);
	tile->ft_depth = depth;
	M0_ALLOC_ARR(tile->ft_cell, tile->ft_rows * tile->ft_cols);
	if (tile->ft_cell == NULL)
		return M0_ERR(-ENOMEM);
	memcpy(tile->ft_child, children,
	       M0_CONF_PVER_HEIGHT * sizeof tile->ft_child[0]);

	M0_POST(parity_group_size(la_attr) <= tile->ft_cols);
	return M0_RC(0);
}

static int objs_of_level_count(struct m0_conf_obj *obj, void *arg)
{
	struct pv_subtree_info *level_info = (struct pv_subtree_info *)arg;

	if (is_obj_at_level[level_info->psi_level](obj))
		++level_info->psi_nr_objs;
	return M0_CW_CONTINUE;
}

static int min_children_get(struct m0_conf_obj *obj, void *arg)
{
	struct pv_subtree_info *level_info = (struct pv_subtree_info *)arg;
	struct m0_conf_objv    *objv;

	if (level_info->psi_level == M0_CONF_PVER_LVL_DRIVES)
		level_info->psi_nr_objs = 0;
	else if (is_obj_at_level[level_info->psi_level](obj)) {
		objv = M0_CONF_CAST(obj, m0_conf_objv);
		level_info->psi_nr_objs =
			min_type(uint32_t, level_info->psi_nr_objs,
				 m0_conf_dir_len(objv->cv_children));
	}
	return M0_CW_CONTINUE;
}

M0_INTERNAL int m0_fd_tolerance_check(struct m0_conf_pver *pv,
				      uint32_t *failure_level)
{
	uint64_t children_nr[M0_CONF_PVER_HEIGHT];
	int      rc;

	rc = symm_tree_attr_get(pv, failure_level, children_nr);
	return M0_RC(rc);
}

M0_INTERNAL int m0_fd_tile_build(const struct m0_conf_pver *pv,
				 struct m0_pool_version *pool_ver,
				 uint32_t *failure_level)
{
	uint64_t             children_nr[M0_CONF_PVER_HEIGHT];
	int                  rc;

	M0_PRE(pv != NULL && pool_ver != NULL && failure_level != NULL);

	/*
	 * Override the disk level tolerance in pool-version
	 * with layout parameter K.
	 */
	pool_ver->pv_fd_tol_vec[M0_CONF_PVER_LVL_DRIVES] =
		pool_ver->pv_attr.pa_K;

	m0_conf_cache_lock(pv->pv_obj.co_cache);
	rc = symm_tree_attr_get(pv, failure_level, children_nr);
	m0_conf_cache_unlock(pv->pv_obj.co_cache);
	if (rc != 0)
		return M0_RC(rc);
	rc = m0_fd__tile_init(&pool_ver->pv_fd_tile, &pv->pv_u.subtree.pvs_attr,
			       children_nr, *failure_level);
	if (rc != 0)
		return M0_RC(rc);
	m0_fd__tile_populate(&pool_ver->pv_fd_tile);

	M0_LEAVE("Symm tree pool width = %d\tFD tree depth = %d",
		 (int)pool_ver->pv_fd_tile.ft_cols, (int)*failure_level);
	return M0_RC(rc);
}

static uint64_t tree2pv_level_conv(uint64_t level, uint64_t tree_depth)
{
	M0_PRE(tree_depth < M0_CONF_PVER_HEIGHT);
	return level + (M0_CONF_PVER_HEIGHT - tree_depth);
}

static int symm_tree_attr_get(const struct m0_conf_pver *pv, uint32_t *depth,
			      uint64_t *children_nr)
{
	uint32_t               pver_level;
	uint32_t               i;
	uint32_t               j;
	struct pv_subtree_info level_info = {0};
	/* drop const */
	struct m0_conf_obj    *pver = (struct m0_conf_obj *)&pv->pv_obj;
	int                    rc;

	M0_PRE(pv != NULL && depth != NULL);
	M0_PRE(m0_conf_cache_is_locked(pv->pv_obj.co_cache));

	pver_level = M0_CONF_PVER_LVL_SITES;
	/* Get the first level having a non-zero tolerance. */
	while (pver_level < M0_CONF_PVER_HEIGHT &&
	       pv->pv_u.subtree.pvs_tolerance[pver_level] == 0)
		++pver_level;

	if (pver_level == M0_CONF_PVER_HEIGHT)
		pver_level = M0_CONF_PVER_LVL_DRIVES;
	level_info.psi_level = pver_level;
	rc = m0_conf_walk(objs_of_level_count, pver, &level_info);
	if (rc < 0)
		return M0_RC(rc);
	*depth = M0_CONF_PVER_HEIGHT - pver_level;
	children_nr[0] = level_info.psi_nr_objs;
	M0_LOG(M0_DEBUG, FID_F": pver_level=%u psi_nr_objs=%u",
	       FID_P(&pver->co_id), pver_level, level_info.psi_nr_objs);
	/*
	 * Extract the attributes of the symmetric tree associated with
	 * the pool version.
	 */
	for (i = 1, j = pver_level; i < *depth; ++i, ++j) {
		level_info.psi_nr_objs = UINT32_MAX;
		level_info.psi_level = j;
		/*
		 * Calculate the degree of nodes at level 'i' in the
		 * symmetric tree associated with a given pool version.
		 */
		rc = m0_conf_walk(min_children_get, pver, &level_info);
		if (rc < 0)
			return M0_RC(rc);
		children_nr[i] = level_info.psi_nr_objs;
	}
	/*
	 * Total number of leaf nodes can be calculated by reducing elements of
	 * children_nr using multiplication. In order to enable this operation,
	 * children count for leaf-nodes is stored as unity,
	 */
	children_nr[*depth] = 1;
	/*
	 * Check if the skeleton tree (a.k.a. symmetric tree) meets the
	 * required tolerance at all the levels.
	 */
	if (pv->pv_u.subtree.pvs_tolerance[M0_CONF_PVER_LVL_DRIVES] > 0)
		rc = tolerance_check(pv, children_nr, pver_level, depth);
	return M0_RC(rc);
}

/*
 * We are distributing units of a parity group across tree,
 * one level at a time. The root receives all `G = N + 2K` units.
 * uniform_distribute() calculates how many units each node
 * (conf-tree node, not a cluster node) from subsequent level
 * will get, when units of each node are distributed _uniformly_
 * among its children. In uniform distribution if a parent has
 * `p` units and has `c` children, each child gets at least `p/c`
 * units, and `r = p % c` children get one extra unit.
 *
 * Suppose level L, has tolerance of K_L, then units_calc() calculates
 * the sum of units held by first K_L nodes of that level, when arranged
 * in descending order of units they hold. And if this sum is more than
 * `K` it means K_L failures at that level are not feasible to support.
 * (Note: tree slightly differs from actual conf tree. It's a subtree of
 * conf tree. Here two nodes at same level have same number of children.)
 *
 * Idea is that we can support K disk failures, when each unit of
 * a parity group goes to different disk. So at any level we distribute
 * units uniformly, and check: maximum how many units of a parity group
 * are lost when K_L nodes of that level are taken down. This shall not
 * be more than K as then data is not recoverable.
 *
 *                                                 (Nachiket)
 */
static int tolerance_check(const struct m0_conf_pver *pv, uint64_t *children_nr,
			   uint32_t first_level, uint32_t *failure_level)
{
	int       rc = 0;
	int       i;
	uint64_t  G;
	uint64_t  K;
	uint64_t  nodes = 1;
	uint64_t  sum;
	uint64_t *units[M0_CONF_PVER_HEIGHT];
	uint32_t  depth = *failure_level;

	M0_ENTRY("pv="FID_F" depth=%u", FID_P(&pv->pv_obj.co_id), depth);

	G = parity_group_size(&pv->pv_u.subtree.pvs_attr);
	K = pv->pv_u.subtree.pvs_attr.pa_K;

	/* total nodes at given level. */
	for (i = 0; i <= depth; ++i) {
		M0_ALLOC_ARR(units[i], nodes);
		if (units[i] == NULL) {
			*failure_level = i;
			for (i = i - 1; i >= 0; --i)
				m0_free(units[i]);
			return M0_ERR(-ENOMEM);
		}
		nodes *= children_nr[i];

	}
	units[0][0] = G;
	for (i = 1, nodes = 1; i < depth; ++i) {
		/* Distribute units from parents to children. */
		uniform_distribute(units, i, nodes, children_nr[i - 1]);
		/* Calculate the sum of units held by top five nodes. */
		sum = units_calc(units, i, nodes, children_nr[i - 1],
				 pv->pv_u.subtree.pvs_tolerance[first_level +
								i - 1]);
		M0_LOG(M0_DEBUG, "%d: sum=%d K=%d children_nr=%d",
		       i, (int)sum, (int)K, (int)children_nr[i - 1]);
		if (sum > K) {
			*failure_level = first_level + i - 1;
			rc = M0_ERR(-EINVAL);
			break;
		}
		nodes *= children_nr[i - 1];
	}
	for (i = 0; i <= depth; ++i) {
		m0_free(units[i]);
	}
	return M0_RC(rc);
}

static void uniform_distribute(uint64_t **units, uint64_t level,
			       uint64_t parent_nr, uint64_t child_nr)
{
	uint64_t pid;
	uint64_t cid;
	uint64_t g_cid;
	uint64_t u_nr;

	for (pid = 0; pid < parent_nr; ++pid) {
		u_nr = units[level - 1][pid];
		for (cid = 0; cid < child_nr; ++cid) {
			g_cid = pid * child_nr + cid;
			units[level][g_cid] = (uint64_t)(u_nr / child_nr);
		}
		for (cid = 0; cid < (u_nr % child_nr); ++cid) {
			g_cid = pid * child_nr + cid;
			units[level][g_cid] += 1;
		}
	}
}

static uint64_t units_calc(uint64_t **units, uint64_t level, uint64_t parent_nr,
			   uint64_t child_nr,  uint64_t tol)
{
	uint64_t pid;
	uint64_t cid;
	uint64_t sum = 0;
	uint64_t cnt = 0;

	for (cid = 0; cnt < tol && cid < child_nr; ++cid) {
		for (pid = 0; cnt < tol && pid < parent_nr; ++pid) {
			sum += units[level][pid * child_nr + cid];
			++cnt;
		}
	}
	return sum;
}

static uint64_t parity_group_size(const struct m0_pdclust_attr *la_attr)
{
	return la_attr->pa_N + 2 * la_attr->pa_K;
}

static uint64_t pool_width_calc(uint64_t *children_nr, uint64_t depth)
{
	M0_PRE(children_nr != NULL);
	M0_PRE(m0_forall(i, depth, children_nr[i] != 0));

	return m0_reduce(i, depth, 1, * children_nr[i]);
}

M0_INTERNAL void m0_fd__tile_populate(struct m0_fd_tile *tile)
{
	uint64_t  row;
	uint64_t  col;
	uint64_t  idx;
	uint64_t  fidx;
	uint64_t  tidx;
	uint64_t *children_nr;

	M0_PRE(fd_tile_invariant(tile));

	children_nr = tile->ft_child;
	for (row = 0; row < tile->ft_rows; ++row) {
		for (col = 0; col < tile->ft_cols; ++col) {
			idx = m0_enc(tile->ft_cols, row, col);
			tidx = fault_tolerant_idx_get(idx, children_nr,
						      tile->ft_depth);
			tile->ft_cell[idx].ftc_tgt.ta_frame = row;
			tile->ft_cell[idx].ftc_tgt.ta_obj   = tidx;
			fidx = m0_enc(tile->ft_cols, row, tidx);
			m0_dec(tile->ft_G, idx,
			       &tile->ft_cell[fidx].ftc_src.sa_group,
			       &tile->ft_cell[fidx].ftc_src.sa_unit);
		}
	}
}

static inline bool fd_tile_invariant(const struct m0_fd_tile *tile)
{
	return _0C(tile != NULL) && _0C(tile->ft_rows > 0) &&
	       _0C(tile->ft_cols > 0)                      &&
	       _0C(tile->ft_G > 0)                         &&
	       _0C(tile->ft_cell != NULL);
}

static uint64_t fault_tolerant_idx_get(uint64_t idx, uint64_t *children_nr,
				       uint64_t depth)
{
	uint64_t i;
	uint64_t prev;
	uint64_t r;
	uint64_t fd_idx[M0_CONF_PVER_HEIGHT];
	uint64_t tidx;

	M0_SET0(&fd_idx);
	for (prev = 1, i = 1; i <= depth; ++i) {
		r  = idx % (prev * children_nr[i - 1]);
		idx -= r;
		fd_idx[i] = r / prev;
		prev *= children_nr[i - 1];
	}
	tidx = fd_idx[depth];
	prev = 1;
	for (i = depth; i > 0; --i) {
		tidx += fd_idx[i - 1] * children_nr[i - 1] * prev;
		prev *= children_nr[i - 1];
	}
	return tidx;
}

M0_INTERNAL void m0_fd_src_to_tgt(const struct m0_fd_tile *tile,
				  const struct m0_pdclust_src_addr *src,
				  struct m0_pdclust_tgt_addr *tgt)
{
	/* A parity group normalized to the first tile. */
	struct m0_pdclust_src_addr src_norm;
	uint64_t                   idx;
	uint64_t                   C;
	uint64_t                   omega;

	M0_PRE(tile != NULL && src != NULL && tgt != NULL);
	M0_PRE(fd_tile_invariant(tile));
	C = tile->ft_rows * tile->ft_cols / tile->ft_G;

	/* Get normalized location. */
	m0_dec(C, src->sa_group, &omega, &src_norm.sa_group);
	src_norm.sa_unit = src->sa_unit;
	M0_ASSERT(src_norm.sa_group < C);
	idx = m0_enc(tile->ft_G, src_norm.sa_group, src_norm.sa_unit);
	*tgt = tile->ft_cell[idx].ftc_tgt;
	/* Denormalize the frame location. */
	tgt->ta_frame += omega * tile->ft_rows;
}

M0_INTERNAL void m0_fd_tgt_to_src(const struct m0_fd_tile *tile,
				  const struct m0_pdclust_tgt_addr *tgt,
				  struct m0_pdclust_src_addr *src)
{
	struct m0_pdclust_tgt_addr tgt_norm;
	uint64_t                   idx;
	uint64_t                   C;
	uint64_t                   omega;

	M0_PRE(tile != NULL && src != NULL && tgt != NULL);
	M0_PRE(fd_tile_invariant(tile));

	C = (tile->ft_rows * tile->ft_cols) / tile->ft_G;
	m0_dec(tile->ft_rows, tgt->ta_frame, &omega, &tgt_norm.ta_frame);
	idx = m0_enc(tile->ft_cols, tgt_norm.ta_frame, tgt->ta_obj);
	*src = tile->ft_cell[idx].ftc_src;
	src->sa_group += omega * C;
}

M0_INTERNAL void m0_fd_tile_destroy(struct m0_fd_tile *tile)
{
	M0_PRE(tile != NULL);

	m0_free0(&tile->ft_cell);
	M0_SET0(&tile);
}

M0_INTERNAL int m0_fd_tree_build(const struct m0_conf_pver *pv,
				 struct m0_fd_tree *tree)
{
	int                  rc;
	uint64_t             children_nr[M0_CONF_PVER_HEIGHT];
	uint32_t             depth;
	uint32_t             level;

	M0_PRE(pv != NULL && tree != NULL);

	m0_conf_cache_lock(pv->pv_obj.co_cache);
	rc = symm_tree_attr_get(pv, &depth, children_nr);
	m0_conf_cache_unlock(pv->pv_obj.co_cache);
	if (rc != 0)
		return M0_RC(rc);
	tree->ft_depth = depth;
	tree->ft_cnt   = 0;
	tree->ft_root  = m0_alloc(sizeof tree->ft_root[0]);
	if (tree->ft_root == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_fd__tree_root_create(tree, children_nr[0]);
	if (rc != 0)
		return M0_RC(rc);
	for (level = 0; level < tree->ft_depth; ++level) {
		rc = m0_fd__tree_level_populate(pv, tree, level);
		if (rc != 0)
			return M0_RC(rc);
	}
	rc = m0_fd__perm_cache_build(tree);
	return M0_RC(rc);
}

M0_INTERNAL int m0_fd__tree_level_populate(const struct m0_conf_pver *pv,
					   struct m0_fd_tree *tree,
					   uint32_t level)
{
	struct m0_conf_objv        *objv;
	struct m0_conf_obj         *obj;
	struct m0_fd__tree_cursor   cursor;
	struct m0_fd_tree_node    **node;
	uint64_t                    children_nr;
	uint64_t                    pv_level;
	int                         rc;

	M0_PRE(pv != NULL && tree != NULL);
	M0_PRE(tree->ft_root != NULL);
	M0_PRE(level >= 0 && level < tree->ft_depth);

	/* Initialize the cursor for failure-domain tree. */
	rc = m0_fd__tree_cursor_init(&cursor, tree, level + 1);
	if (rc != 0)
		return M0_RC(rc);
	pv_level = tree2pv_level_conv(level, tree->ft_depth);
	M0_LOG(M0_DEBUG, "depth=%u level=%u pv_level=%u",
	       (unsigned)tree->ft_depth, level, (unsigned)pv_level);
	pv_for (pv, pv_level, obj, rc) {
		M0_ASSERT(m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE);
		M0_LOG(M0_DEBUG, FID_F, FID_P(&obj->co_id));
		objv = M0_CONF_CAST(obj, m0_conf_objv);
		children_nr = level < tree->ft_depth - 1 ?
			m0_conf_dir_len(objv->cv_children) : 0;
		node = m0_fd__tree_cursor_get(&cursor);
		*node = m0_alloc(sizeof node[0][0]);
		if (*node == NULL)
			goto rewind;
		rc = m0_fd__tree_node_init(tree, *node, children_nr, &cursor);
		if (rc != 0)
			goto rewind;
		m0_fd__tree_cursor_next(&cursor);
	} pv_endfor;
	M0_ASSERT(ergo(rc == 0, !m0_fd__tree_cursor_next(&cursor)));

	return M0_RC(rc);
rewind:
	if (*node != NULL) {
		m0_fd__tree_node_fini(tree, *node);
		m0_free(*node);
		*node = NULL;
	}
	--cursor.ftc_child_abs_idx;
	tree->ft_depth = cursor.ftc_child_abs_idx > -1 ? cursor.ftc_depth :
		cursor.ftc_depth - 1;
	return	M0_ERR(rc);
}

M0_INTERNAL int m0_fd__perm_cache_build(struct m0_fd_tree *tree)
{
	struct m0_fd__tree_cursor  cursor;
	struct m0_fd_tree_node    *node;
	uint64_t                   children_nr;
	uint64_t                  *cache_len;
	uint64_t                  *cache_info;
	uint64_t                   level;
	int                        rc;

	M0_ALLOC_ARR(cache_info, tree->ft_cnt);
	if (cache_info == NULL)
		return M0_ERR(-ENOMEM);
	tree->ft_cache_info.fci_info = cache_info;
	tree->ft_cache_info.fci_nr = 0;
	for (level = 0; level < tree->ft_depth; ++level) {
		rc = m0_fd__tree_cursor_init(&cursor, tree, level);
		if (rc != 0) {
			m0_fd__perm_cache_destroy(tree);
			return M0_RC(rc);
		}
		do {
			node = *(m0_fd__tree_cursor_get(&cursor));
			children_nr = node->ftn_child_nr;
			cache_info_update(&tree->ft_cache_info, children_nr);
		} while (m0_fd__tree_cursor_next(&cursor));
	}
	m0_array_sort(cache_info, tree->ft_cache_info.fci_nr);
	/*
	 * Since tree->ft_cnt is likely to be much greater than actual length
	 * of tree->ft_cache_info.fci_info, we reallocate
	 * tree->ft_cache_info.fci_info once the actual length is obtained.
	 */
	M0_ALLOC_ARR(cache_len, tree->ft_cache_info.fci_nr);
	if (cache_len == NULL) {
		m0_free(tree->ft_cache_info.fci_info);
		return M0_ERR(-ENOMEM);
	}
	memcpy(cache_len, tree->ft_cache_info.fci_info,
	       tree->ft_cache_info.fci_nr * sizeof cache_len[0]);
	m0_free(tree->ft_cache_info.fci_info);
	tree->ft_cache_info.fci_info = cache_len;
	return M0_RC(0);
}

static void cache_info_update(struct m0_fd_cache_info *cache_info, uint64_t cnt)
{
	uint64_t i;

	for (i = 0; i < cache_info->fci_nr; ++i)
		if (cache_info->fci_info[i] == cnt)
			break;
	if (i == cache_info->fci_nr) {
		cache_info->fci_info[i] = cnt;
		++cache_info->fci_nr;
	}
}

M0_INTERNAL int m0_fd_perm_cache_init(struct m0_fd_perm_cache *cache,
				      uint64_t len)
{
	struct m0_uint128 seed;
	struct m0_fid     gfid;
	uint64_t         *permute;
	uint64_t         *inverse;
	uint64_t         *lcode;


	M0_PRE(cache != NULL);

	M0_SET0(cache);
	cache->fpc_len = len;
	M0_ALLOC_ARR(permute, len);
	if (permute == NULL)
		goto err;

	cache->fpc_permute = permute;
	M0_ALLOC_ARR(inverse, len);
	if (inverse == NULL)
		goto err;

	cache->fpc_inverse = inverse;
	M0_ALLOC_ARR(lcode, len);
	if (lcode == NULL)
		goto err;

	cache->fpc_lcode = lcode;
	/* Initialize the permutation present in the cache. */
	cache->fpc_omega = ~(uint64_t)0;
	m0_fid_set(&cache->fpc_gfid, ~(uint64_t)0, ~(uint64_t)0);
	m0_uint128_init(&seed, M0_PDCLUST_SEED);
	m0_fid_set(&gfid, 0, 0);
	fd_permute(cache, &seed, &gfid, 0);
	return M0_RC(0);
err:
	m0_free0(&cache->fpc_permute);
	m0_free0(&cache->fpc_inverse);
	m0_free0(&cache->fpc_lcode);
	M0_SET0(cache);
	return M0_ERR(-ENOMEM);
}

M0_INTERNAL void m0_fd_tree_destroy(struct m0_fd_tree *tree)
{
	struct m0_fd__tree_cursor  cursor;
	struct m0_fd_tree_node   **node;
	uint16_t                   depth;
	int32_t                    i;
	int                        rc;

	depth = tree->ft_depth;
	for (i = depth; i > 0; --i) {
		rc = m0_fd__tree_cursor_init(&cursor, tree, i);
		M0_ASSERT(rc == 0);
		do {
			node = m0_fd__tree_cursor_get(&cursor);
			/*
			 * This condition will hit when
			 * m0_fd__tree_level_populate() has got terminated
			 * intermittently.
			 */
			if (*node == NULL)
				break;
			m0_fd__tree_node_fini(tree, *node);
			m0_free0(node);
		} while (m0_fd__tree_cursor_next(&cursor));
	}
	if (tree->ft_root != NULL)
		m0_fd__tree_node_fini(tree, tree->ft_root);
	m0_fd__perm_cache_destroy(tree);
	m0_free0(&tree->ft_root);
	M0_POST(tree->ft_cnt == 0);
	M0_SET0(tree);
}

M0_INTERNAL void m0_fd__perm_cache_destroy(struct m0_fd_tree *tree)
{
	M0_PRE(tree != NULL);
	m0_free(tree->ft_cache_info.fci_info);
	tree->ft_cache_info.fci_nr = 0;
}

M0_INTERNAL void m0_fd_perm_cache_fini(struct m0_fd_perm_cache *cache)
{
	m0_free0(&cache->fpc_lcode);
	m0_free0(&cache->fpc_permute);
	m0_free0(&cache->fpc_inverse);
}

static struct m0_pool_version *
pool_ver_get(const struct m0_pdclust_instance *pd_instance)
{
	return pd_instance->pi_base.li_l->l_pver;
}

M0_INTERNAL void m0_fd_fwd_map(struct m0_pdclust_instance *pi,
			       const struct m0_pdclust_src_addr *src,
			       struct m0_pdclust_tgt_addr *tgt)
{
	struct m0_fd_tile          *tile;
	struct m0_pool_version     *pver;
	struct m0_pdclust_src_addr  src_base;
	uint64_t                    rel_vidx[M0_CONF_PVER_HEIGHT];
	uint64_t                    omega;
	uint64_t                    children;
	uint64_t                    C;
	uint64_t                    tree_depth;
	uint64_t                    i;
	uint64_t                    vidx;

	M0_PRE(pi != NULL);
	M0_PRE(src != NULL && tgt != NULL);

	pver = pool_ver_get(pi);
	tile = &pver->pv_fd_tile;
	M0_ASSERT(tile != NULL);
	/* Get location in fault-tolerant permutation. */
	m0_fd_src_to_tgt(tile, src, tgt);
	tree_depth = pver->pv_fd_tile.ft_depth;
	for (i = 1, children = 1; i < tree_depth; ++i) {
		children *= tile->ft_child[i];
	}
	for (i = 1, vidx = tgt->ta_obj; i <= tree_depth; ++i) {
		rel_vidx[i]  = vidx / children;
		vidx        %= children;
		children    /= tile->ft_child[i];
	}
	M0_ASSERT(tile->ft_G != 0);
	C = tile->ft_rows * tile->ft_cols / tile->ft_G;
	m0_dec(C, src->sa_group, &omega, &src_base.sa_group);
	permuted_tgt_get(pi, omega, rel_vidx, &tgt->ta_obj);
}

static void permuted_tgt_get(struct m0_pdclust_instance *pi, uint64_t omega,
			     uint64_t *rel_vidx, uint64_t *tgt_idx)
{
	struct m0_fd_tree         *tree;
	struct m0_fd_perm_cache   *cache;
	struct m0_pool_version    *pver;
	struct m0_fd_tree_node    *node;
	struct m0_fd__tree_cursor  cursor = {};
	struct m0_pdclust_attr    *attr;
	struct m0_fid             *gfid;
	uint64_t                   depth;
	uint64_t                   perm_idx;
	uint64_t                   rel_idx;
	int                        rc;

	pver = pool_ver_get(pi);
	tree = &pver->pv_fd_tree;
	node = tree->ft_root;
	gfid = &pi->pi_base.li_gfid;
	attr = &pool_ver_get(pi)->pv_attr;

	for (depth = 1; depth <= tree->ft_depth; ++depth) {
		rel_idx = rel_vidx[depth];
		cache = cache_get(pi, node);
		fd_permute(cache, &attr->pa_seed, gfid, omega);
		M0_ASSERT(rel_idx < cache->fpc_len);
		perm_idx = cache->fpc_permute[rel_idx];
		rc = m0_fd__tree_cursor_init_at(&cursor, tree, node, perm_idx);
		M0_ASSERT(rc == 0);
		node = *(m0_fd__tree_cursor_get(&cursor));
		M0_ASSERT(node != NULL);
	}
	*tgt_idx = node->ftn_abs_idx;
}

static struct m0_fd_perm_cache *cache_get(struct m0_pdclust_instance *pi,
					  struct m0_fd_tree_node *node)
{
	uint64_t i;
	uint64_t cache_len;

	cache_len = node->ftn_child_nr;
	for (i = 0; i < pi->pi_cache_nr; ++i)
		if (pi->pi_perm_cache[i].fpc_len == cache_len)
			return &pi->pi_perm_cache[i];
	return NULL;
}

static void fd_permute(struct m0_fd_perm_cache *cache,
		       struct m0_uint128 *seed, struct m0_fid *gfid,
		       uint64_t omega)
{
	uint32_t i;
	uint64_t rstate;


	if (!is_cache_valid(cache, omega, gfid)) {
		/* Initialise columns array that will be permuted. */
		for (i = 0; i < cache->fpc_len; ++i)
			cache->fpc_permute[i] = i;

		/* Initialise PRNG. */
		rstate = m0_hash(seed->u_hi + gfid->f_key) ^
			m0_hash(seed->u_lo + omega + gfid->f_container);

		/* Generate permutation number in lexicographic ordering. */
		for (i = 0; i < cache->fpc_len - 1; ++i)
			cache->fpc_lcode[i] = m0_rnd(cache->fpc_len - i,
					&rstate);
		/* Apply the permutation. */
		m0_permute(cache->fpc_len, cache->fpc_lcode,
			   cache->fpc_permute, cache->fpc_inverse);
		cache->fpc_omega = omega;
		cache->fpc_gfid  = *gfid;
	}
}

static  bool is_cache_valid(const struct m0_fd_perm_cache *cache,
			    uint64_t omega, const struct m0_fid *gfid)
{
	return cache->fpc_omega == omega && m0_fid_eq(&cache->fpc_gfid, gfid);
}

M0_INTERNAL void m0_fd_bwd_map(struct m0_pdclust_instance *pi,
			       const struct m0_pdclust_tgt_addr *tgt,
			       struct m0_pdclust_src_addr *src)
{
	struct m0_pdclust_tgt_addr tgt_ft;
	struct m0_pool_version    *pver;
	struct m0_fd_tile         *tile;
	uint64_t                   omega;
	uint64_t                   children;
	uint64_t                   i;
	uint64_t                   vidx;
	uint64_t                   tree_depth;
	uint64_t                   rel_idx[M0_CONF_PVER_HEIGHT];

	M0_PRE(pi != NULL);
	M0_PRE(tgt != NULL && src != NULL);

	pver = pool_ver_get(pi);
	tile = &pver->pv_fd_tile;
	m0_dec(pver->pv_fd_tile.ft_rows, tgt->ta_frame, &omega,
	       &tgt_ft.ta_frame);
	inverse_permuted_idx_get(pi, omega, tgt->ta_obj, rel_idx);
	tree_depth = pver->pv_fd_tree.ft_depth;
	for (i = 1, children = 1; i < tree_depth; ++i) {
		children *= tile->ft_child[i];
	}
	for (i = 1, vidx = 0; i <= tree_depth; ++i) {
		vidx     += rel_idx[i] * children;
		if (rel_idx[i] >= tile->ft_child[i - 1])
			break;
		children /= tile->ft_child[i];
	}
	if (i > tree_depth) {
		tgt_ft.ta_frame = tgt->ta_frame;
		tgt_ft.ta_obj   = vidx;
		m0_fd_tgt_to_src(&pver->pv_fd_tile, &tgt_ft, src);
	} else {
		/* Input target and frame are unmapped. */
		src->sa_group  = UINT64_MAX;
		src->sa_unit   = UINT64_MAX;
	}
}

static void inverse_permuted_idx_get(struct m0_pdclust_instance *pi,
				     uint64_t omega, uint64_t perm_idx,
				     uint64_t *rel_idx)
{
	struct m0_fd_tree         *tree;
	struct m0_pool_version    *pver;
	struct m0_fd__tree_cursor  cursor;
	struct m0_fd_perm_cache   *cache;
	struct m0_fd_tree_node    *node;
	struct m0_pdclust_attr    *attr;
	struct m0_fid             *gfid;
	int                        rc;
	int                        depth;

	pver = pool_ver_get(pi);
	tree = &pver->pv_fd_tree;
	gfid = &pi->pi_base.li_gfid;
	attr = &pool_ver_get(pi)->pv_attr;

	rc = m0_fd__tree_cursor_init(&cursor, tree, tree->ft_depth);
	M0_ASSERT(rc == 0);
	while (cursor.ftc_child_abs_idx < perm_idx &&
	       m0_fd__tree_cursor_next(&cursor));
	M0_ASSERT(cursor.ftc_child_abs_idx == perm_idx);
	perm_idx = cursor.ftc_child_idx;
	node = cursor.ftc_node;
	M0_ASSERT(node != NULL);
	for (depth = tree->ft_depth; depth > 0; --depth) {
		cache = cache_get(pi, node);
		fd_permute(cache, &attr->pa_seed, gfid, omega);
		rel_idx[depth] = cache->fpc_inverse[perm_idx];
		perm_idx = node->ftn_rel_idx;
		node = node->ftn_parent;
	}
}

#undef M0_TRACE_SUBSYSTEM
