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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"       /* M0_ERR() */

#include "lib/finject.h"     /* m0_fi_enable() */
#include "lib/memory.h"      /* m0_alloc() */
#include "lib/misc.h"        /* M0_SET0() */
#include "fd/fd_internal.h"
#include "fd/fd.h"
#include "fd/ut/common.h"
#include "ut/ut.h"           /* M0_UT_ASSERT() */


static int tree_populate(struct m0_fd_tree *tree, enum tree_type param_type);
static uint32_t geometric_sum(uint16_t r, uint16_t k);
static uint32_t int_pow(uint32_t num, uint32_t exp);

static void test_cache_init_fini(void)
{
	struct m0_fd_tree        tree;
	uint32_t                 children_nr;
	uint32_t                 i;
	uint32_t                 unique_chld_nr[TP_NR];
	uint32_t                 list_chld_nr[TP_NR];
	uint64_t                 cache_len;
	m0_time_t                seed;
	int                      rc;

	M0_SET0(&unique_chld_nr);
	M0_SET0(&list_chld_nr);
	seed = m0_time_now();
	children_nr = m0_rnd(TP_QUATERNARY, &seed);
	children_nr = TP_QUATERNARY - children_nr;
	rc = fd_ut_tree_init(&tree, M0_CONF_PVER_HEIGHT - 1);
	M0_UT_ASSERT(rc == 0);
	rc = m0_fd__tree_root_create(&tree, children_nr);
	M0_UT_ASSERT(rc == 0);
	unique_chld_nr[children_nr] = 1;
	for (i = 1; i < M0_CONF_PVER_HEIGHT; ++i) {
		children_nr = m0_rnd(TP_QUATERNARY, &seed);
		children_nr = TP_QUATERNARY - children_nr;
		children_nr = i == tree.ft_depth ? 0 : children_nr;
		rc = fd_ut_tree_level_populate(&tree, children_nr, i, TA_SYMM);
		M0_UT_ASSERT(rc == 0);
		if (i < M0_CONF_PVER_HEIGHT - 1)
			unique_chld_nr[children_nr] = 1;
	}
	m0_fd__perm_cache_build(&tree);
	for (i = 0; i < tree.ft_cache_info.fci_nr; ++i) {
		cache_len = tree.ft_cache_info.fci_info[i];
		M0_UT_ASSERT(unique_chld_nr[cache_len] == 1);
		list_chld_nr[cache_len] = 1;
	}
	M0_UT_ASSERT(!memcmp(unique_chld_nr, list_chld_nr,
			     sizeof unique_chld_nr));
	m0_fd_tree_destroy(&tree);
}

static void test_init_fini(void)
{
	struct m0_fd_tree tree;
	uint16_t          i;
	uint16_t          j;
	int               rc;

	for (j = TP_BINARY; j < TP_QUATERNARY + 1; ++j) {
		for (i = 1; i < M0_CONF_PVER_HEIGHT; ++i) {
			rc = fd_ut_tree_init(&tree, i);
			M0_UT_ASSERT(rc == 0);
			rc = m0_fd__tree_root_create(&tree, j);
			M0_UT_ASSERT(rc == 0);
			rc = tree_populate(&tree, j);
			M0_UT_ASSERT(rc == 0);
			M0_UT_ASSERT(geometric_sum(j, i) == tree.ft_cnt);
			m0_fd_tree_destroy(&tree);
			M0_UT_ASSERT(tree.ft_cnt == 0);
		}
	}
}

static void test_fault_inj(void)
{
	struct m0_fd_tree tree;
	m0_time_t         seed;
	uint64_t          n;
	int               rc;

	M0_SET0(&tree);
	rc = fd_ut_tree_init(&tree, M0_CONF_PVER_HEIGHT - 1);
	M0_UT_ASSERT(rc == 0);
	rc = m0_fd__tree_root_create(&tree, TP_BINARY);
	M0_UT_ASSERT(rc == 0);
	rc = tree_populate(&tree, TP_BINARY);
	M0_UT_ASSERT(rc == 0);
	m0_fd_tree_destroy(&tree);

	/* Fault injection. */
	seed = m0_time_now();
	/* Maximum nodes in a tree. */
	n    = m0_rnd(geometric_sum(TP_BINARY, M0_CONF_PVER_HEIGHT - 1),
		      &seed);
	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", n, 1);
	M0_SET0(&tree);
	rc = fd_ut_tree_init(&tree, M0_CONF_PVER_HEIGHT - 1) ?:
	     m0_fd__tree_root_create(&tree, TP_BINARY) ?:
	     tree_populate(&tree, TP_BINARY);
	m0_fi_disable("m0_alloc","fail_allocation");
	/*
	 * NOTE: m0_fd__tree_root_create and tree_populate call
	 * m0_alloc two times per node, but m0_alloc may be recursively:
	 * m0_alloc call alloc_tail, but alloc_tail may call m0_alloc
	 */
	M0_UT_ASSERT((n >> 1) >= tree.ft_cnt);
	m0_fd_tree_destroy(&tree);
	M0_UT_ASSERT(tree.ft_cnt == 0);
}

static uint32_t geometric_sum(uint16_t r, uint16_t k)
{
	return (int_pow(r, k + 1) - 1) / (r - 1);
}

static uint32_t int_pow(uint32_t num, uint32_t exp)
{
	uint32_t ret = 1;
	uint32_t i;

	for (i = 0; i < exp; ++i) {
		ret *= num;
	}
	return ret;
}

static int tree_populate(struct m0_fd_tree *tree, enum tree_type param_type)
{
	uint16_t i;
	int      rc;
	uint64_t children_nr;

	for (i = 1; i <= tree->ft_depth; ++i) {
		children_nr = i == tree->ft_depth ? 0 : param_type;
		rc = fd_ut_tree_level_populate(tree, children_nr, i, TA_SYMM);
		if (rc != 0)
			return rc;
	}
	rc = m0_fd__perm_cache_build(tree);
	return rc;
}

static void test_perm_cache(void)
{
	test_cache_init_fini();
}
static void test_fd_tree(void)
{
	test_init_fini();
	test_fault_inj();
}

struct m0_ut_suite failure_domains_tree_ut = {
	.ts_name = "failure_domains_tree-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{"test_fd_tree", test_fd_tree},
		{"test_perm_cache", test_perm_cache},
		{ NULL, NULL }
	}
};
