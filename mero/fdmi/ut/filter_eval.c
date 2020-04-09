/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "fdmi/filter.h"
#include "fdmi/filter_xc.h"
#include "fdmi/flt_eval.h"
#include "lib/finject.h"
#include "xcode/xcode.h"
#include "ut/ut.h"

/* ------------------------------------------------------------------
 * Helper: initialize and eval simple binary operand.
 * ------------------------------------------------------------------ */

static int flt_eval_binary_operator(enum m0_fdmi_flt_op_code  op_code,
				    struct m0_fdmi_flt_node  *opnd1,
				    struct m0_fdmi_flt_node  *opnd2,
				    struct m0_fdmi_eval_ctx  *eval_ctx)
{
	struct m0_fdmi_filter    flt;
	struct m0_fdmi_flt_node *root;
	int                      rc;

	root = m0_fdmi_flt_op_node_create(op_code, opnd1, opnd2);
	m0_fdmi_filter_init(&flt);
	m0_fdmi_filter_root_set(&flt, root);

	rc = m0_fdmi_eval_flt(eval_ctx, &flt, NULL);

	m0_fdmi_filter_fini(&flt);

	return rc;
}

/* ------------------------------------------------------------------
 * Test Case: simple OR (M0_FFO_OR)
 * ------------------------------------------------------------------ */

static void flt_eval_simple_or(void)
{
	bool frst_operand[] = { false, false, true,  true };
	bool sec_operand[]  = { false, true,  false, true };
	bool result_vals[]  = { false, true,  true,  true };

	struct m0_fdmi_eval_ctx eval_ctx;
	int                     eval_res;
	int                     i;

	m0_fdmi_eval_init(&eval_ctx);

	for (i = 0; i < ARRAY_SIZE(result_vals); i++) {
		eval_res = flt_eval_binary_operator(
				M0_FFO_OR,
				m0_fdmi_flt_bool_node_create(frst_operand[i]),
				m0_fdmi_flt_bool_node_create(sec_operand[i]),
				&eval_ctx);
		M0_UT_ASSERT(eval_res == result_vals[i]);
	}
	m0_fdmi_eval_fini(&eval_ctx);
}

/* ------------------------------------------------------------------
 * Test Case: simple GT (M0_FFO_GT)
 * ------------------------------------------------------------------ */

static void flt_eval_simple_gt(void)
{
	struct pair0 {
		int64_t lft_arg;
		int64_t rgt_arg;
	} args0[] = { {0,0}, {0,1}, {1,0}, {1,1}, {1,2}, {2,1}, {2,2},
		{0,-1}, {-1,0}, {-1,-1}, {-1,2}, {2,-1},
		{INT64_MIN, 0}, {0, INT64_MIN},
		{INT64_MAX, 0}, {0, INT64_MAX},
		{INT64_MIN, INT64_MIN}, {INT64_MAX, INT64_MAX},
		{INT64_MIN, INT64_MAX}, {INT64_MAX, INT64_MIN},
	};

	struct pair1 {
		uint64_t lft_arg;
		uint64_t rgt_arg;
	} args1[] = { {0,0}, {0,1}, {1,0}, {1,1}, {1,2}, {2,1}, {2,2},
		{UINT64_MAX, 0}, {0, UINT64_MAX},
		{INT64_MAX, UINT64_MAX},
	};

	struct m0_fdmi_eval_ctx  eval_ctx;
	int                      eval_res;
	int                      eval_expected;
	int			 i;

	m0_fdmi_eval_init(&eval_ctx);

	/* Incompatible args */

	/* Incompatible args, #1 */
	eval_res = flt_eval_binary_operator(
				M0_FFO_GT,
				m0_fdmi_flt_bool_node_create(true),
				m0_fdmi_flt_bool_node_create(false),
				&eval_ctx);
	M0_UT_ASSERT(eval_res == -EINVAL);

	/* Incompatible args, #2 */
	eval_res = flt_eval_binary_operator(
				M0_FFO_GT,
				m0_fdmi_flt_int_node_create(1),
				m0_fdmi_flt_bool_node_create(false),
				&eval_ctx);
	M0_UT_ASSERT(eval_res == -EINVAL);

	/* Incompatible args, #3 */
	eval_res = flt_eval_binary_operator(
				M0_FFO_GT,
				m0_fdmi_flt_int_node_create(1),
				m0_fdmi_flt_uint_node_create(2),
				&eval_ctx);
	M0_UT_ASSERT(eval_res == -EINVAL);

	/* Incompatible args, #4 */
	eval_res = flt_eval_binary_operator(
				M0_FFO_GT,
				m0_fdmi_flt_uint_node_create(2),
				m0_fdmi_flt_int_node_create(1),
				&eval_ctx);
	M0_UT_ASSERT(eval_res == -EINVAL);

	/* Int comparison */
	for (i = 0; i < ARRAY_SIZE(args0); i++) {
		eval_res = flt_eval_binary_operator(
				M0_FFO_GT,
				m0_fdmi_flt_int_node_create(args0[i].lft_arg),
				m0_fdmi_flt_int_node_create(args0[i].rgt_arg),
				&eval_ctx);
		eval_expected = !!(args0[i].lft_arg > args0[i].rgt_arg);
		M0_UT_ASSERT(eval_res >= 0);
		M0_UT_ASSERT(eval_res == eval_expected);
	}

	/* UInt comparison */
	for (i = 0; i < ARRAY_SIZE(args1); i++) {
		eval_res = flt_eval_binary_operator(
			M0_FFO_GT,
			m0_fdmi_flt_uint_node_create(args1[i].lft_arg),
			m0_fdmi_flt_uint_node_create(args1[i].rgt_arg),
			&eval_ctx);
		eval_expected = !!(args1[i].lft_arg > args1[i].rgt_arg);
		M0_UT_ASSERT(eval_res >= 0);
		M0_UT_ASSERT(eval_res == eval_expected);
	}

	m0_fdmi_eval_fini(&eval_ctx);
}

/* ------------------------------------------------------------------
 * Test Case: Register/deregister custom evaluator callback
 * ------------------------------------------------------------------ */

static bool flt_test_op_cb_called = false;

static int flt_test_op_cb(struct m0_fdmi_flt_operands *opnds,
			  struct m0_fdmi_flt_operand  *res)
{
	m0_fdmi_flt_bool_opnd_fill(res, true);
	flt_test_op_cb_called = true;
	return 0;
}

static void flt_set_op_cb(void)
{
	struct m0_fdmi_eval_ctx eval_ctx;
	int                     eval_res;
	int                     rc;

	m0_fdmi_eval_init(&eval_ctx);

	/* first set must succeed */
	rc = m0_fdmi_eval_add_op_cb(&eval_ctx, M0_FFO_TEST, &flt_test_op_cb);
	M0_UT_ASSERT(rc == 0);

	/* second set must fail */
	rc = m0_fdmi_eval_add_op_cb(&eval_ctx, M0_FFO_TEST, &flt_test_op_cb);
	M0_UT_ASSERT(rc == -EEXIST);

	/* delete the CB */
	m0_fdmi_eval_del_op_cb(&eval_ctx, M0_FFO_TEST);

	/* set must succeed */
	rc = m0_fdmi_eval_add_op_cb(&eval_ctx, M0_FFO_TEST, &flt_test_op_cb);
	M0_UT_ASSERT(rc == 0);

	/* try evaluate, must succeed */
	eval_res = flt_eval_binary_operator(
			M0_FFO_TEST,
			m0_fdmi_flt_bool_node_create(true),
			m0_fdmi_flt_bool_node_create(false),
			&eval_ctx);
	M0_UT_ASSERT(flt_test_op_cb_called);
	M0_UT_ASSERT(eval_res == true);

	m0_fdmi_eval_fini(&eval_ctx);
}

/* ------------------------------------------------------------------
 * Test Case: XCode conversions
 * ------------------------------------------------------------------ */

static void flt_eval_flt_xcode_str(void)
{
	struct m0_fdmi_filter    flt;
	struct m0_fdmi_flt_node *root;
	/* New root constructed by m0_xcode_read() */
	struct m0_fdmi_flt_node *root_read;
	char                     str[256];
	int                      res;

	root = m0_fdmi_flt_op_node_create(
			M0_FFO_OR,
			m0_fdmi_flt_bool_node_create(true),
			m0_fdmi_flt_bool_node_create(false));

	m0_fdmi_filter_init(&flt);

	m0_fdmi_filter_root_set(&flt, root);

	res = m0_xcode_print(&M0_XCODE_OBJ(m0_fdmi_flt_node_xc, flt.ff_root),
			     str, sizeof(str));

	M0_UT_ASSERT(res > 0);

	m0_fdmi_filter_fini(&flt);

	/* Read back searialized root node */
	M0_ALLOC_PTR(root_read);
	M0_ASSERT(root_read != NULL);

	m0_xcode_read(&M0_XCODE_OBJ(m0_fdmi_flt_node_xc, root_read), str);

	m0_fdmi_filter_init(&flt);
	m0_fdmi_filter_root_set(&flt, root_read);
	m0_fdmi_filter_fini(&flt);
}

/* ------------------------------------------------------------------
 * Test Case: Filter to string/from string operations
 * ------------------------------------------------------------------ */

static void flt_str_ops(void)
{
	struct m0_fdmi_filter    flt       = {};
	struct m0_fdmi_flt_node *root      = NULL;
	struct m0_fdmi_flt_node *root_read = NULL;
	char                    *str       = NULL;
	int                      res;
	int                      i;

	root = m0_fdmi_flt_op_node_create(
			M0_FFO_OR,
			m0_fdmi_flt_bool_node_create(true),
			m0_fdmi_flt_bool_node_create(false));
	M0_UT_ASSERT(root != NULL);

	m0_fdmi_filter_init(&flt);

	m0_fdmi_filter_root_set(&flt, root);

	/* Conversion fail (malloc fail) */
	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 0, 1);
	res = m0_fdmi_flt_node_print(root, &str);
	m0_fi_disable("m0_alloc", "fail_allocation");
	M0_UT_ASSERT(res == -ENOMEM);

	/* Conversion success when xcode print rc < FIRST_SIZE_GUESS*/
	m0_fi_enable_off_n_on_m("m0_fdmi_flt_node_print",
					"rc_bigger_than_size_guess", 0, 1);
	res = m0_fdmi_flt_node_print(root, &str);
	m0_fi_disable("m0_fdmi_flt_node_print", "rc_bigger_than_size_guess");
	M0_UT_ASSERT(res == 0);

	/* Conversion fail (malloc failed) */
	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 1, 1);
	m0_fi_enable_off_n_on_m("m0_fdmi_flt_node_print",
					"rc_bigger_than_size_guess", 0, 1);
	res = m0_fdmi_flt_node_print(root, &str);
	m0_fi_disable("m0_fdmi_flt_node_print", "rc_bigger_than_size_guess");
	m0_fi_disable("m0_alloc", "fail_allocation");
	M0_UT_ASSERT(res == -ENOMEM);

	/* Conversion success */
	res = m0_fdmi_flt_node_print(root, &str);
	M0_UT_ASSERT(res >= 0);
	M0_UT_ASSERT(str != NULL);

	/* Back conversion */
	M0_ALLOC_PTR(root_read);
	M0_ASSERT(root_read != NULL);
	res = m0_fdmi_flt_node_parse(str, root_read);
	M0_UT_ASSERT(res >= 0);
	M0_UT_ASSERT(root->ffn_type == root_read->ffn_type);
	M0_UT_ASSERT(root->ffn_u.ffn_oper.ffon_op_code ==
		     root_read->ffn_u.ffn_oper.ffon_op_code);
	M0_UT_ASSERT(root->ffn_u.ffn_oper.ffon_opnds.fno_cnt ==
		     root_read->ffn_u.ffn_oper.ffon_opnds.fno_cnt);

	/* deinit */
	m0_free0(&str);

	m0_fdmi_filter_fini(&flt);

	m0_fdmi_filter_init(&flt);
	m0_fdmi_filter_root_set(&flt, root_read);
	m0_fdmi_filter_fini(&flt);

	/* Test case with nested tree (it's separate branch in str encode
	 * function. */
	root = m0_fdmi_flt_op_node_create(
			M0_FFO_OR,
			m0_fdmi_flt_bool_node_create(true),
			m0_fdmi_flt_bool_node_create(false));
	M0_UT_ASSERT(root != NULL);
	for (i = 0; i < 1; i++) {
		root = m0_fdmi_flt_op_node_create(
				M0_FFO_OR,
				m0_fdmi_flt_bool_node_create(false),
				root);
		M0_UT_ASSERT(root != NULL);
	}

	m0_fdmi_filter_init(&flt);

	m0_fdmi_filter_root_set(&flt, root);

#if 0
	/* FIXME: fails; XCode is not able to process our nested structures. */
	/* This functionality is not required for Phase1, so commenting the
	 * test out. */
	{
		/* Approach one: use our node-to-str; this is the target test,
		 * to get higher coverage, this is the latest non-covered code
		 * branch in filter.c */
		// res = m0_fdmi_flt_node_to_str(root, &str);
		// M0_UT_ASSERT(res >= 0);
		// M0_UT_ASSERT(str != NULL);
	}
	{ /* Hard-core approach, use core xcode functions. */
		/* In my case, it fails on data_size call, not able to
		 * traverse the tree. */
		struct m0_bufvec_cursor  cur  = {};
		struct m0_bufvec         bvec = {};
		struct m0_xcode_ctx	 ctx  = {};
		struct m0_xcode_obj	 obj  = {};
		int			 len;

		obj = M0_XCODE_OBJ(m0_fdmi_flt_node_xc, root);
		m0_xcode_ctx_init(&ctx, &obj);
		len = m0_xcode_data_size(&ctx, &obj);
		res = m0_bufvec_alloc(&bvec, 1, len);
		m0_bufvec_cursor_init(&cur, &bvec);

		res = m0_xcode_encdec(&M0_XCODE_OBJ(m0_fdmi_flt_node_xc, root),
				      &cur, M0_XCODE_ENCODE);
	}
#endif

	/* deinit */
	// m0_free0(&str);
	m0_fdmi_filter_fini(&flt);
}

/* ------------------------------------------------------------------
 * Test Sute definition
 * ------------------------------------------------------------------ */

struct m0_ut_suite fdmi_filter_eval_ut = {
	.ts_name = "fdmi-filter-eval-ut",
	.ts_tests = {
		{ "simple-or",        flt_eval_simple_or },
		{ "simple-gt",        flt_eval_simple_gt },
		{ "callback",         flt_set_op_cb },
		/** @todo Move to filter tests */
		{ "filter-xcode-str", flt_eval_flt_xcode_str },
		{ "filter-str-ops",   flt_str_ops },
		{ NULL, NULL },
	},
};

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
