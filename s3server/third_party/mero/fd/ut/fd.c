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
 * Original creation date: 13-Feb-15.
 */

#include "fd/fd.h"
#include "fd/fd_internal.h" /* m0_fd__tree_root_create */
#include "fd/ut/common.h"   /* la_N */
#include "pool/pool.h"      /* m0_pool_version */
#include "conf/confc.h"     /* m0_confc */
#include "conf/diter.h"     /* m0_conf_diter */
#include "conf/obj_ops.h"   /* M0_CONF_DIRNEXT */
#include "lib/memory.h"     /* m0_alloc */
#include "lib/fs.h"         /* m0_file_read */
#include "lib/errno.h"      /* EINVAL */
#include "conf/ut/common.h" /* m0_conf_ut_ast_thread_init */
#include "ut/ut.h"          /* M0_UT_ASSERT */

/* Conf parameters. */

#ifdef __KERNEL__
static const char local_conf_str[] = "[35:\
   {0x74| ((^t|1:0), 1, (11, 22), ^o|1:23, ^v|1:24, 41212,\
           [3: \"param-0\", \"param-1\", \"param-2\"],\
           [1: ^n|1:2],\
           [1: ^S|2:15],\
           [1: ^o|1:23],\
           [1: ^p|1:0],\
           [0])},\
   {0x70| ((^p|1:0), [1: ^o|1:23])},\
   {0x6e| ((^n|1:2), 16000, 2, 3, 2, [1: ^r|1:3])},\
   {0x72| ((^r|1:3), [1:3], 0, 0, 0, 0, \"addr-0\", [6: ^s|1:4,\
                                                   ^s|1:5,\
                                                   ^s|1:6,\
                                                   ^s|1:7,\
                                                   ^s|1:8,\
                                                   ^s|1:9])},\
   {0x73| ((^s|1:4), @M0_CST_MDS, [3: \"addr-0\", \"addr-1\", \"addr-2\"], [0],\
           [0])},\
   {0x73| ((^s|1:5), @M0_CST_IOS, [3: \"addr-0\", \"addr-1\", \"addr-2\"], [0],\
           [5: ^d|1:10, ^d|1:11, ^d|1:12, ^d|1:13, ^d|1:14])},\
   {0x73| ((^s|1:6), @M0_CST_CONFD, [3: \"addr-0\", \"addr-1\", \"addr-2\"],\
           [0], [0])},\
   {0x73| ((^s|1:7), @M0_CST_RMS, [3: \"addr-0\", \"addr-1\", \"addr-2\"], [0],\
           [0])},\
   {0x73| ((^s|1:8), @M0_CST_HA, [3: \"addr-0\", \"addr-1\", \"addr-2\"], [0],\
           [0])},\
   {0x73| ((^s|1:9), @M0_CST_IOS, [1: \"addr-3\"], [0], [0])},\
   {0x64| ((^d|1:10), 1, 4, 1, 4096, 596000000000, 3, 4, \"/dev/sdev0\")},\
   {0x64| ((^d|1:11), 2, 4, 1, 4096, 596000000000, 3, 4, \"/dev/sdev1\")},\
   {0x64| ((^d|1:12), 3, 7, 2, 8192, 320000000000, 2, 4, \"/dev/sdev2\")},\
   {0x64| ((^d|1:13), 4, 7, 2, 8192, 320000000000, 2, 4, \"/dev/sdev3\")},\
   {0x64| ((^d|1:14), 5, 7, 2, 8192, 320000000000, 2, 4, \"/dev/sdev4\")},\
   {0x53| ((^S|2:15), [1: ^a|1:15], [1: ^v|1:24])},\
   {0x61| ((^a|1:15), [1: ^e|1:16], [1: ^v|1:24])},\
   {0x65| ((^e|1:16), [1: ^c|1:17], [1: ^v|1:24])},\
   {0x63| ((^c|1:17), ^n|1:2, [5: ^k|1:18, ^k|1:19, ^k|1:20,\
                                  ^k|1:21, ^k|1:22], [1: ^v|1:24])},\
   {0x6b| ((^k|1:18), ^d|1:10, [1: ^v|1:24])},\
   {0x6b| ((^k|1:19), ^d|1:11, [1: ^v|1:24])},\
   {0x6b| ((^k|1:20), ^d|1:12, [1: ^v|1:24])},\
   {0x6b| ((^k|1:21), ^d|1:13, [1: ^v|1:24])},\
   {0x6b| ((^k|1:22), ^d|1:14, [1: ^v|1:24])},\
   {0x6f| ((^o|1:23), 0, [1: ^v|1:24])},\
   {0x76| ((^v|1:24), {0| (3, 1, 5, [5: 0,0,0,0,1], [1: ^j|3:25])})},\
   {0x6a| ((^j|3:25), ^S|2:15, [1: ^j|1:25])},\
   {0x6a| ((^j|1:25), ^a|1:15, [1: ^j|1:26])},\
   {0x6a| ((^j|1:26), ^e|1:16, [1: ^j|1:27])},\
   {0x6a| ((^j|1:27), ^c|1:17, [5: ^j|1:28, ^j|1:29,\
                                   ^j|1:30, ^j|1:31, ^j|1:32])},\
   {0x6a| ((^j|1:28), ^k|1:18, [0])},\
   {0x6a| ((^j|1:29), ^k|1:19, [0])},\
   {0x6a| ((^j|1:30), ^k|1:20, [0])},\
   {0x6a| ((^j|1:31), ^k|1:21, [0])},\
   {0x6a| ((^j|1:32), ^k|1:22, [0])}]";
#endif

struct m0_pdclust_attr pd_attr  = {
	.pa_N         = la_N,
	.pa_K         = la_K,
	.pa_P         = la_N + 2 * la_K,
	.pa_unit_size = 4096,
	.pa_seed = {
		.u_hi = 0,
		.u_lo = 0,
	}
};

struct m0_pdclust_instance pi;
struct m0_pdclust_src_addr src;
struct m0_pdclust_src_addr src_new;
struct m0_pdclust_tgt_addr tgt;
struct m0_pool_version     pool_ver;

static uint32_t parity_group_size(struct m0_pdclust_attr *la_attr);
static uint32_t pool_width_count(uint64_t *children, uint32_t depth);
static bool __filter_pv(const struct m0_conf_obj *obj);
static uint64_t real_child_cnt_get(uint64_t level);
static uint64_t pool_width_calc(struct m0_fd_tree *tree);
static void tree_generate(struct m0_pool_version *pv, enum tree_attr ta);
static void fd_mapping_check(struct m0_pool_version *pv);
static bool is_tgt_failed(struct m0_pool_version *pv,
			  struct m0_pdclust_tgt_addr *tgt,
			  uint64_t *failed_domains);
static void failed_nodes_mark(struct m0_fd_tree *tree, uint32_t level,
			      uint64_t tol, uint64_t *failed_domains);
static void fd_tolerance_check(struct m0_pool_version *pv);

static void test_fd_mapping_sanity(enum tree_attr ta)
{

	/* Construct a failure domains tree. */
	M0_SET0(&pool_ver);
	tree_generate(&pool_ver, ta);
	fd_mapping_check(&pool_ver);
	m0_fd_tree_destroy(&pool_ver.pv_fd_tree);
	m0_fd_tile_destroy(&pool_ver.pv_fd_tile);
}

static void tree_generate(struct m0_pool_version *pv, enum tree_attr ta)
{
	uint64_t G;
	uint64_t P;
	int      rc;
	uint64_t children_cnt;
	int      i;

	P = 1;
	G = parity_group_size(&pd_attr);
	while (G > P) {
		rc = fd_ut_tree_init(&pv->pv_fd_tree, M0_CONF_PVER_HEIGHT - 1);
		M0_UT_ASSERT(rc == 0);
		children_cnt = fd_ut_random_cnt_get(TUA_RACKS);
		rc = m0_fd__tree_root_create(&pv->pv_fd_tree, children_cnt);
		M0_UT_ASSERT(rc == 0);
		for (i = 1; i < M0_CONF_PVER_HEIGHT; ++i) {
			children_cnt = real_child_cnt_get(i);
			children_cnt = i == pv->pv_fd_tree.ft_depth ? 0 :
				children_cnt;
			rc = fd_ut_tree_level_populate(&pv->pv_fd_tree,
						       children_cnt, i,
						       ta);
			M0_UT_ASSERT(rc == 0);
		}
		rc = m0_fd__perm_cache_build(&pv->pv_fd_tree);
		M0_UT_ASSERT(rc == 0);
		P = pool_width_calc(&pv->pv_fd_tree);
		if (G > P)
			m0_fd_tree_destroy(&pv->pv_fd_tree);
	}
	/* Get the attributes of symmetric tree. */
	fd_ut_symm_tree_get(&pv->pv_fd_tree, pv->pv_fd_tile.ft_child);
	rc = m0_fd__tile_init(&pv->pv_fd_tile, &pd_attr,
			      pv->pv_fd_tile.ft_child,
			      pv->pv_fd_tree.ft_depth);
	M0_UT_ASSERT(rc == 0);
	m0_fd__tile_populate(&pv->pv_fd_tile);
}

static uint64_t pool_width_calc(struct m0_fd_tree *tree)
{
	struct m0_fd__tree_cursor cursor;
	uint64_t                  P;
	int                       rc;

	rc = m0_fd__tree_cursor_init(&cursor, tree, tree->ft_depth);
	M0_UT_ASSERT(rc == 0);
	P = 1;
	while (m0_fd__tree_cursor_next(&cursor))
		++P;
	return P;
}

static void fd_mapping_check(struct m0_pool_version *pv)
{
	struct m0_pdclust_instance pi;
	uint64_t                   C;
	m0_time_t                  seed;
	uint64_t                   omega;
	uint64_t                   row;
	uint64_t                   col;
	uint64_t                   unmapped;
	uint64_t                   P;

	M0_SET0(&src);
	M0_SET0(&src_new);
	M0_SET0(&tgt);
	pi.pi_base.li_l = m0_alloc(sizeof pi.pi_base.li_l[0]);
	M0_UT_ASSERT(pi.pi_base.li_l != NULL);
	C = (pv->pv_fd_tile.ft_rows * pv->pv_fd_tile.ft_cols) /
		pv->pv_fd_tile.ft_G;
	seed = m0_time_now();
	omega = m0_rnd(123456, &seed);
	pi.pi_base.li_l->l_pver = pv;
	m0_pdclust_perm_cache_build(pi.pi_base.li_l, &pi);
	for (row = omega * C; row < (omega + 1) * C; ++row) {
		src.sa_group = row;
		for (col = 0; col < pv->pv_fd_tile.ft_G; ++col) {
			src.sa_unit = col;
			M0_SET0(&src_new);
			m0_fd_fwd_map(&pi, &src, &tgt);
			m0_fd_bwd_map(&pi, &tgt, &src_new);
			M0_UT_ASSERT(src.sa_group == src_new.sa_group);
			M0_UT_ASSERT(src.sa_unit == src_new.sa_unit);
		}
	}
	/* Sanity check for unmapped targets. */
	unmapped = 0;
	tgt.ta_frame = omega * pv->pv_fd_tile.ft_rows;
	P = pool_width_calc(&pv->pv_fd_tree);
	for (tgt.ta_obj = 0; tgt.ta_obj < P; ++tgt.ta_obj) {
		m0_fd_bwd_map(&pi, &tgt, &src_new);
		if (src_new.sa_group == ~(uint64_t)0 &&
		    src_new.sa_unit == ~(uint64_t)0)
			++unmapped;
	}
	M0_UT_ASSERT(unmapped + pv->pv_fd_tile.ft_cols == P);
	m0_pdclust_perm_cache_destroy(pi.pi_base.li_l, &pi);
	m0_free(pi.pi_base.li_l);
}

static uint64_t real_child_cnt_get(uint64_t level)
{
	M0_UT_ASSERT(level < M0_CONF_PVER_HEIGHT);

	switch (level) {
	case 0:
		return TUA_RACKS;
	case 1:
		return TUA_ENC;
	case 2:
		return TUA_CON;
	case 3:
		return TUA_DISKS;
	case 4:
		return 0;
	}
	return 0;
}

static void test_ft_mapping(void)
{
	uint64_t  row;
	uint64_t  col;
	uint64_t  G;
	uint64_t  C;
	uint64_t  P;
	uint64_t *children_nr;
	uint64_t  omega;
	m0_time_t seed;
	uint32_t  depth;
	int       rc;


	M0_SET0(&pool_ver);
	G           = parity_group_size(&pd_attr);
	P           = pd_attr.pa_K;
	children_nr = pool_ver.pv_fd_tile.ft_child;
	M0_SET0(&src);
	M0_SET0(&src_new);
	M0_SET0(&tgt);
	for (depth = 1; depth < M0_CONF_PVER_HEIGHT; ++depth) {
		while (G > P || P > TUA_MAX_POOL_WIDTH) {
			fd_ut_children_populate(children_nr, depth);
			P = pool_width_count(children_nr, depth);
		}
		rc = m0_fd__tile_init(&pool_ver.pv_fd_tile, &pd_attr,
				      pool_ver.pv_fd_tile.ft_child,
				      depth);
		M0_UT_ASSERT(rc == 0);
		m0_fd__tile_populate(&pool_ver.pv_fd_tile);
		C     = (pool_ver.pv_fd_tile.ft_rows * P) / G;
		seed  = m0_time_now();
		omega = m0_rnd(123456, &seed);
		for (row = omega * C; row < (omega + 1) * C; ++row) {
			src.sa_group = row;
			for (col = 0; col < G; ++col) {
				src.sa_unit = col;
				M0_SET0(&src_new);
				m0_fd_src_to_tgt(&pool_ver.pv_fd_tile, &src,
						 &tgt);
				m0_fd_tgt_to_src(&pool_ver.pv_fd_tile, &tgt,
						 &src_new);
				M0_UT_ASSERT(src.sa_group == src_new.sa_group);
				M0_UT_ASSERT(src.sa_unit == src_new.sa_unit);
			}
		}
		m0_fd_tile_destroy(&pool_ver.pv_fd_tile);
		P = la_K;
	}
}

static uint32_t parity_group_size(struct m0_pdclust_attr *la_attr)
{
	M0_UT_ASSERT(la_attr != NULL);

	return la_attr->pa_N + 2 * la_attr->pa_K;

}

static uint32_t pool_width_count(uint64_t *children, uint32_t depth)
{
	uint32_t i;
	uint32_t cnt = 1;

	for (i = 0; i < depth; ++i) {
		cnt *= children[i];
	}

	return cnt;
}

static void test_pv2fd_conv(void)
{
	struct m0_confc     *confc;
	struct m0_conf_obj  *root_obj = NULL;
	struct m0_conf_diter it;
	struct m0_conf_obj  *pv_obj;
	struct m0_conf_pver *pv;
#ifndef __KERNEL__
	char                *confstr = NULL;
#endif
	uint32_t             failure_level;
	uint64_t             max_failures = 1;
	int                  i;
	int                  rc;

	M0_ALLOC_PTR(confc);
	M0_SET0(confc);
	M0_SET0(&pool_ver);
#ifndef __KERNEL__
	rc = m0_file_read(M0_SRC_PATH("fd/ut/failure-domains.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(confc, &m0_conf_ut_grp, NULL, NULL, confstr);
	M0_UT_ASSERT(rc == 0);
	m0_free0(&confstr);
#else
	rc = m0_confc_init(confc, &m0_conf_ut_grp, NULL, NULL, local_conf_str);
	M0_UT_ASSERT(rc == 0);
#endif
	rc = m0_confc_open_sync(&root_obj, confc->cc_root, M0_FID0);
	M0_UT_ASSERT(rc == 0);
	rc = m0_conf_diter_init(&it, confc, root_obj,
				M0_CONF_ROOT_POOLS_FID,
				M0_CONF_POOL_PVERS_FID);
	M0_UT_ASSERT(rc == 0);
	rc = m0_conf_diter_next_sync(&it, __filter_pv);
	M0_UT_ASSERT(rc == M0_CONF_DIRNEXT);
	pv_obj = m0_conf_diter_result(&it);
	pv = M0_CONF_CAST(pv_obj, m0_conf_pver);

	/*
	 * Figure out the first level for which user specified tolerance is
	 * non-zero.
	 */
	for (i = 0; i < M0_CONF_PVER_HEIGHT; ++i)
		if (pv->pv_u.subtree.pvs_tolerance[i] != 0) {
			max_failures = pv->pv_u.subtree.pvs_tolerance[i];
			failure_level     = i;
			break;
		}
	/*
	 * We intend to test those cases whenever supported cases are greater
	 * than or equal to the actual failures.
	 */
#ifndef __KERNEL__
	M0_UT_ASSERT(i < M0_CONF_PVER_LVL_DRIVES);
	pv->pv_u.subtree.pvs_tolerance[failure_level] = 0;
#else
	max_failures = 1;
#endif
	for (i = 0; i < max_failures; ++i) {
		rc = m0_fd_tile_build(pv, &pool_ver, &failure_level);
		M0_UT_ASSERT(rc == 0);
		rc = m0_fd_tree_build(pv, &pool_ver.pv_fd_tree);
		M0_UT_ASSERT(rc == 0);
		memcpy(pool_ver.pv_fd_tol_vec, pv->pv_u.subtree.pvs_tolerance,
		       M0_CONF_PVER_HEIGHT * sizeof pool_ver.pv_fd_tol_vec[0]);
		fd_mapping_check(&pool_ver);
		fd_tolerance_check(&pool_ver);
		m0_fd_tree_destroy(&pool_ver.pv_fd_tree);
		m0_fd_tile_destroy(&pool_ver.pv_fd_tile);
		++pv->pv_u.subtree.pvs_tolerance[failure_level];
	}
	/* Test the case when entire tolerance vector is zero. */
	memset(pv->pv_u.subtree.pvs_tolerance, 0,
	      M0_CONF_PVER_HEIGHT * sizeof pv->pv_u.subtree.pvs_tolerance[0]);
	memcpy(pool_ver.pv_fd_tol_vec, pv->pv_u.subtree.pvs_tolerance,
	       M0_CONF_PVER_HEIGHT * sizeof pool_ver.pv_fd_tol_vec[0]);
	pv->pv_u.subtree.pvs_attr.pa_K = 0;
	rc = m0_fd_tile_build(pv, &pool_ver, &failure_level);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pool_ver.pv_fd_tile.ft_depth == 1);
	rc = m0_fd_tree_build(pv, &pool_ver.pv_fd_tree);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pool_ver.pv_fd_tree.ft_depth == 1);
	fd_mapping_check(&pool_ver);
	m0_fd_tree_destroy(&pool_ver.pv_fd_tree);
	m0_fd_tile_destroy(&pool_ver.pv_fd_tile);

	m0_conf_diter_fini(&it);
	m0_confc_close(root_obj);
	m0_confc_fini(confc);
	m0_free(confc);
}

static bool __filter_pv(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_PVER_TYPE;
}

static  uint64_t pv2tree_level_conv(uint64_t level, uint64_t tree_depth)
{
	M0_PRE(tree_depth < M0_CONF_PVER_HEIGHT);
	return level - ((M0_CONF_PVER_HEIGHT - 1)  - tree_depth);
}

static void fd_tolerance_check(struct m0_pool_version *pv)
{
	struct m0_pdclust_instance  pi;
	uint64_t                    C;
	uint64_t                    omega;
	uint64_t                    row;
	uint64_t                    col;
	m0_time_t                   seed;
	uint32_t                    i;
	uint32_t                    fail_cnt;
	uint32_t                    tol;
	uint64_t                   *failed_domains;

	for (i = M0_CONF_PVER_LVL_SITES; i < M0_CONF_PVER_HEIGHT; ++i) {
		tol = pv->pv_fd_tol_vec[i];
		if (tol == 0)
			continue;
		failed_domains = m0_alloc(tol * sizeof failed_domains[0]);
		failed_nodes_mark(&pv->pv_fd_tree, i, tol, failed_domains);
		pi.pi_base.li_l = m0_alloc(sizeof pi.pi_base.li_l[0]);
		M0_UT_ASSERT(pi.pi_base.li_l != NULL);
		C = (pv->pv_fd_tile.ft_rows * pv->pv_fd_tile.ft_cols) /
			pv->pv_fd_tile.ft_G;
		seed = m0_time_now();
		omega = m0_rnd(123456, &seed);
		pi.pi_base.li_l->l_pver = pv;
		m0_pdclust_perm_cache_build(pi.pi_base.li_l, &pi);
		fail_cnt = 0;
		for (row = omega * C; row < (omega + 1) * C; ++row) {
			src.sa_group = row;
			for (col = 0; col < pv->pv_fd_tile.ft_G; ++col) {
				src.sa_unit = col;
				m0_fd_fwd_map(&pi, &src, &tgt);
				if (is_tgt_failed(pv, &tgt, failed_domains))
					++fail_cnt;
				M0_UT_ASSERT(fail_cnt <=
					     pv->pv_fd_tol_vec[M0_CONF_PVER_LVL_DRIVES]);
			}
			fail_cnt = 0;
		}
		m0_free(failed_domains);
		m0_pdclust_perm_cache_destroy(pi.pi_base.li_l, &pi);
	}
}

static void failed_nodes_mark(struct m0_fd_tree *tree, uint32_t level,
			      uint64_t tol, uint64_t *failed_domains)
{
	struct m0_fd__tree_cursor cursor;
	struct m0_fd_tree_node *node;
	uint32_t  toss;
	uint32_t  cnt;
	uint32_t tree_level;
	int rc;
	m0_time_t seed;

	cnt = 0;
	tree_level = pv2tree_level_conv(level, tree->ft_depth);
	while (cnt < tol) {
		rc = m0_fd__tree_cursor_init(&cursor, tree, tree_level);
		M0_UT_ASSERT(rc == 0);
		toss = 0;
		seed = m0_time_now();
		do {
			toss = m0_rnd(2, &seed);
			node = *(m0_fd__tree_cursor_get(&cursor));
		} while (toss == 0 && m0_fd__tree_cursor_next(&cursor));

		if (node->ftn_ha_state != M0_NC_FAILED) {
			node->ftn_ha_state = M0_NC_FAILED;
			failed_domains[cnt] = node->ftn_abs_idx;
			++cnt;
		}
	}
}

static bool is_tgt_failed(struct m0_pool_version *pv,
			  struct m0_pdclust_tgt_addr *tgt,
			  uint64_t *failed_domains)
{
	struct m0_fd__tree_cursor cursor;
	uint64_t                  i;
	uint64_t                  P;
	int                       rc;

	P = pool_width_calc(&pv->pv_fd_tree);
	M0_UT_ASSERT(tgt->ta_obj < P);
	rc = m0_fd__tree_cursor_init(&cursor, &pv->pv_fd_tree,
				     pv->pv_fd_tree.ft_depth);
	M0_UT_ASSERT(rc == 0);
	i = 0;
	while (i < tgt->ta_obj) {
		 m0_fd__tree_cursor_next(&cursor);
		++i;
	}
	return (*(m0_fd__tree_cursor_get(&cursor)))->ftn_ha_state != M0_NC_ONLINE;
}

void test_fd_mapping(void)
{
	test_fd_mapping_sanity(TA_ASYMM);
	test_fd_mapping_sanity(TA_SYMM);
}

struct m0_ut_suite failure_domains_ut = {
	.ts_name  = "failure_domains-ut",
	.ts_init  = m0_conf_ut_ast_thread_init,
	.ts_fini  = m0_conf_ut_ast_thread_fini,
	.ts_tests = {
		{"test_ft_mapping", test_ft_mapping},
		{"test_pv2fd_conv", test_pv2fd_conv},
		{"test_fd_mapping", test_fd_mapping},
		{ NULL, NULL }
	}
};
