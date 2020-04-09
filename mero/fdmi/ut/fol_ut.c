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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "ut/ut.h"
#include "fop/fom_generic.h"
#include "fdmi/fdmi.h"
#include "fdmi/fops.h"                 /* m0_fop_fdmi_rec_not_fopt */
#include "fdmi/source_dock.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/fol_fdmi_src.h"
#include "rpc/rpc_opcodes.h"           /* M0_FDMI_RECORD_NOT_OPCODE */
#include "lib/finject.h"

#include "fdmi/source_dock_internal.h" /* m0_fdmi__src_ctx_get */

#include "fdmi/ut/sd_common.h"

/* ------------------------------------------------------------------
 * Test to make sure we've registered OK
 * ------------------------------------------------------------------ */

static void fdmi_fol_check_registered(void)
{
	struct m0_fdmi_src_dock *dock;
	struct m0_fdmi_src_ctx  *src;

	M0_ENTRY();

	/**
	 * Register happens in m0_init -- we just need to make sure here that
	 * it went OK.
	 */

	dock = m0_fdmi_src_dock_get();
	M0_UT_ASSERT(dock != NULL);
	src = m0_fdmi__src_ctx_get(M0_FDMI_REC_TYPE_FOL);
	M0_UT_ASSERT(src != NULL);

	M0_LEAVE();
}

/* ------------------------------------------------------------------
 * Test FOL source operations
 * ------------------------------------------------------------------ */

enum ffs_ut_test_op {
	FFS_UT_OPS_TEST_BASIC_OPS,
	FFS_UT_OPS_TEST_SUDDEN_FINI,
};

/* Dummy State machine. */

enum {
	FFS_UT_FOM_INIT   = M0_FOM_PHASE_INIT,
	FFS_UT_FOM_READY  = M0_FOM_PHASE_NR,
	FFS_UT_FOM_FINISH = M0_FOPH_FINISH
};

static struct m0_sm_state_descr ffs_ut_fom_phases[] = {
	[FFS_UT_FOM_INIT] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(FFS_UT_FOM_READY)
	},
	[FFS_UT_FOM_READY] = {
		.sd_name      = "ready",
		.sd_allowed   = M0_BITS(FFS_UT_FOM_FINISH)
	},
	[FFS_UT_FOM_FINISH] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "SM finish",
	}
};

static struct m0_sm_trans_descr ffs_ut_fom_trans[] = {
	{ "Start",   FFS_UT_FOM_INIT,  FFS_UT_FOM_READY },
	{ "Finish",  FFS_UT_FOM_READY,   FFS_UT_FOM_FINISH },
};

/* Test details machine. */

static bool                    dummy_post_called = false;
static bool                    dummy_make_post_fail = false;
static struct m0_fdmi_src_rec *dummy_rec_pointer;

static int dummy_fdmi_post_record(struct m0_fdmi_src_rec *src_rec)
{
	int rc = 0;

	M0_ENTRY();

	if (dummy_make_post_fail) {
		rc = -ENOMEM;
	} else {
		/* Make sure this func is only called ONCE. */
		M0_UT_ASSERT(!dummy_post_called);
		dummy_post_called = true;

		M0_UT_ASSERT(src_rec->fsr_data == NULL);
		dummy_rec_pointer = src_rec;
	}

	return M0_RC(rc);
}

static void dummy_fol_rec_assert_eq(const struct m0_fol_rec *rec1,
				    const struct m0_fol_rec *rec2)
{
	struct m0_fol_frag *frag1;
	struct m0_fol_frag *frag2;

	M0_ASSERT(rec1 != NULL && rec2 != NULL);
	M0_ENTRY("rec1 0x%p, rec2 0x%p", rec1, rec2);

	M0_UT_ASSERT(rec1->fr_tid == rec2->fr_tid);

	frag1 = m0_rec_frag_tlist_head(&rec1->fr_frags);
	frag2 = m0_rec_frag_tlist_head(&rec2->fr_frags);

	do {
		M0_UT_ASSERT((frag1 == NULL) == (frag2 == NULL));
		M0_UT_ASSERT(frag1->rp_ops   == frag2->rp_ops);
		M0_UT_ASSERT(frag1->rp_magic == frag2->rp_magic);

		frag1 = m0_rec_frag_tlist_next(&rec1->fr_frags, frag1);
		frag2 = m0_rec_frag_tlist_next(&rec2->fr_frags, frag2);
	} while (frag1 != NULL || frag2 != NULL);

	M0_LEAVE();
}

static void fdmi_fol_test_ops(enum ffs_ut_test_op test_op)
{
	/* fdmi/ffs structures */
	struct m0_fdmi_src_dock    *dock;
	struct m0_fdmi_src_ctx     *src_ctx;
	struct m0_fdmi_src         *src_reg;

	/* system structures (be) */
	struct m0_fom               fom      = {};
	struct m0_be_tx            *betx     = &fom.fo_tx.tx_betx;
	struct m0_fol_rec          *fol_rec  = &fom.fo_tx.tx_fol_rec;
	struct m0_be_ut_backend     ut_be    = {};
	struct m0_sm_group         *grp      = NULL;
	struct m0_fop              *fop      = NULL;
	struct m0_fop_fdmi_record  *fop_data = NULL;

	/* temp vars */
	struct m0_buf               buf         = {};
	struct m0_fol_rec          *fol_rec_cpy = NULL;
	struct m0_fdmi_flt_var_node var_node    = {};
	struct m0_fdmi_flt_operand  flt_operand = {};
	struct m0_fdmi_src_rec      *src_rec    =
	    &fom.fo_tx.tx_fol_rec.fr_fdmi_rec;
	int (*saved_fs_record_post)(struct m0_fdmi_src_rec *src_rec);

	int rc;


	struct m0_sm_conf ffs_ut_sm_conf = {
		.scf_name      = "fol src dummy fom phases",
		.scf_nr_states = ARRAY_SIZE(ffs_ut_fom_phases),
		.scf_state     = ffs_ut_fom_phases,
		.scf_trans_nr  = ARRAY_SIZE(ffs_ut_fom_trans),
		.scf_trans     = ffs_ut_fom_trans
	};

	M0_ENTRY();

	/* get ops */
	dock = m0_fdmi_src_dock_get();
	M0_UT_ASSERT(dock != NULL);
	src_ctx = m0_fdmi__src_ctx_get(M0_FDMI_REC_TYPE_FOL);
	M0_UT_ASSERT(src_ctx != NULL);
	src_reg = &src_ctx->fsc_src;

	/* create system structures */
	m0_be_ut_backend_init(&ut_be);
	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);

	/* TXN ini */
	m0_be_ut_tx_init(betx, &ut_be);
	M0_UT_ASSERT(betx->t_ref == 1);
	m0_sm_conf_init(&ffs_ut_sm_conf);
	m0_sm_init(&betx->t_sm, &ffs_ut_sm_conf, FFS_UT_FOM_INIT, grp);
	m0_fol_rec_init(fol_rec, NULL);

	/* FOM ini */
	src_rec->fsr_magic = M0_FDMI_SRC_DOCK_REC_MAGIC;
	src_rec->fsr_src_ctx = src_ctx;

	/* FOP ini */
	fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_fopt, NULL, (void*)1);
	fop_data = m0_fop_data(fop);
	fop_data->fr_rec_id = M0_UINT128(1,1);
	fop_data->fr_rec_type = 1000;
	fop_data->fr_payload = M0_BUF_INIT0;
	fop_data->fr_matched_flts.fmf_count = 0;
	fop_data->fr_matched_flts.fmf_flt_id = NULL;
	rc = m0_fop_fol_add(fop, fop, &fom.fo_tx);

	/* inject our own post-record method */
	saved_fs_record_post = src_reg->fs_record_post;
	src_reg->fs_record_post = dummy_fdmi_post_record;

	/* start ops */
	dummy_post_called = false;
	dummy_rec_pointer = NULL;
	switch (test_op) {

	case FFS_UT_OPS_TEST_BASIC_OPS:
		/* post record failure #1 */
		dummy_make_post_fail = true;
		rc = m0_fol_fdmi_post_record(&fom);
		dummy_make_post_fail = false;
		M0_UT_ASSERT(rc == -ENOMEM);
		M0_UT_ASSERT(!dummy_post_called);
		m0_sm_asts_run(grp);
		M0_UT_ASSERT(betx->t_ref == 1);
		/* post record */
		rc = m0_fol_fdmi_post_record(&fom);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(dummy_post_called);
		m0_sm_asts_run(grp);
		M0_UT_ASSERT(betx->t_ref == 2);
		/* processing start */
		src_reg->fs_begin(dummy_rec_pointer);
		/* get value */
		src_reg->fs_node_eval(dummy_rec_pointer, &var_node,
				      &flt_operand);
		M0_UT_ASSERT(flt_operand.ffo_type == M0_FF_OPND_PLD_UINT);
		M0_UT_ASSERT(flt_operand.ffo_data.fpl_pld.fpl_uinteger ==
			     M0_FDMI_RECORD_NOT_OPCODE);
		/* inc ref */
		src_reg->fs_get(dummy_rec_pointer);
		/* encode failure #1 */
		m0_fi_enable("m0_alloc", "fail_allocation");
		rc = src_reg->fs_encode(dummy_rec_pointer, &buf);
		m0_fi_disable("m0_alloc", "fail_allocation");
		M0_UT_ASSERT(rc == -ENOMEM);
		M0_UT_ASSERT(buf.b_addr == NULL && buf.b_nob == 0);
		/* encode failure #2 */
		m0_fi_enable("ffs_op_encode", "fail_in_final");
		rc = src_reg->fs_encode(dummy_rec_pointer, &buf);
		m0_fi_disable("ffs_op_encode", "fail_in_final");
		M0_UT_ASSERT(rc == -EINVAL);
		M0_UT_ASSERT(buf.b_addr == NULL && buf.b_nob == 0);
		/* encode */
		rc = src_reg->fs_encode(dummy_rec_pointer, &buf);
		M0_UT_ASSERT(rc >= 0);
		M0_UT_ASSERT(buf.b_nob > 0 && buf.b_addr != NULL);
		/* decode failure #1 */
		m0_fi_enable("m0_alloc", "fail_allocation");
		rc = src_reg->fs_decode(&buf, (void**)&fol_rec_cpy);
		m0_fi_disable("m0_alloc", "fail_allocation");
		M0_UT_ASSERT(rc == -ENOMEM);
		M0_UT_ASSERT(fol_rec_cpy == NULL);
		/* decode failure #2 */
		m0_fi_enable("ffs_op_decode", "fail_in_final");
		rc = src_reg->fs_decode(&buf, (void**)&fol_rec_cpy);
		m0_fi_disable("ffs_op_decode", "fail_in_final");
		M0_UT_ASSERT(rc == -EINVAL);
		M0_UT_ASSERT(fol_rec_cpy == NULL);
		/* decode */
		fol_rec_cpy = NULL;
		rc = src_reg->fs_decode(&buf, (void**)&fol_rec_cpy);
		M0_UT_ASSERT(rc >= 0);
		M0_UT_ASSERT(fol_rec_cpy != NULL);
		dummy_fol_rec_assert_eq(fol_rec, fol_rec_cpy);
		/* dec ref */
		src_reg->fs_put(dummy_rec_pointer);
		/* finalize processing */
		m0_sm_asts_run(grp);
		M0_UT_ASSERT(betx->t_ref == 2);
		src_reg->fs_end(dummy_rec_pointer);
		src_reg->fs_put(dummy_rec_pointer);

		/* Run asts directly, so posted AST is invoked to
		 * decrement betx->t_ref. Maybe better way to run
		 * ASTs exist */
		m0_sm_asts_run(grp);
		M0_UT_ASSERT(betx->t_ref == 1);

		/* reset record_post back to orig value */
		src_reg->fs_record_post = saved_fs_record_post;

		break;
	case FFS_UT_OPS_TEST_SUDDEN_FINI:
		rc = m0_fol_fdmi_post_record(&fom);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(dummy_post_called);
		m0_sm_asts_run(grp);
		M0_UT_ASSERT(betx->t_ref == 2);

		m0_fol_fdmi_src_fini();

		/* Run asts directly, so posted AST is invoked to
		 * decrement betx->t_ref. Maybe better way to run
		 * ASTs exist */
		m0_sm_asts_run(grp);
		M0_UT_ASSERT(betx->t_ref == 1);

		/* We have to call this init manually, turns out UT framework
		 * does not re-initialize everything before a test suite... :( */
		rc = m0_fol_fdmi_src_init();
		M0_UT_ASSERT(rc == 0);

		/* reset record_post back to orig value is not here because
		 * m0_fol_fdmi_src_init() does it. */
		break;
	default:
		M0_UT_ASSERT(false);
	}

	/* Temp vals fini */
	if (buf.b_addr != NULL)
		m0_buf_free(&buf);
	if (fol_rec_cpy != NULL) {
		m0_fol_rec_fini(fol_rec_cpy);
		m0_free0(&fol_rec_cpy);
	}

	/* FOP fini */
	m0_fop_fini(fop);
	m0_free0(&fop);

	/* transaction fini */
	m0_fol_rec_fini(fol_rec);
	betx->t_ref = 0;
	betx->t_sm.sm_state = M0_BTS_FAILED;
	m0_be_tx_fini(betx);

	/* system fini */
	m0_be_ut_backend_fini(&ut_be);

	M0_LEAVE();
}

static void fdmi_fol_test_basic_ops(void)
{
	M0_ENTRY();
	fdmi_fol_test_ops(FFS_UT_OPS_TEST_BASIC_OPS);
	M0_LEAVE();
}

static void fdmi_fol_test_sudden_fini(void)
{
	M0_ENTRY();
	fdmi_fol_test_ops(FFS_UT_OPS_TEST_SUDDEN_FINI);
	M0_LEAVE();
}

/* ------------------------------------------------------------------
 * Test Suite definition
 * ------------------------------------------------------------------ */

struct m0_ut_suite fdmi_fol_ut = {
	.ts_name = "fdmi-fol-ut",
	.ts_tests = {
		{ "fdmi-fol-register",  fdmi_fol_check_registered},
		{ "fdmi-fol-ops",       fdmi_fol_test_basic_ops},
		{ NULL, NULL },
	},
};

struct m0_ut_suite fdmi_fol_fini_ut = {
	.ts_name = "fdmi-fol-fini-ut",
	.ts_tests = {
		{ "fdmi-fol-fini",      fdmi_fol_test_sudden_fini},
		{ NULL, NULL },
	},
};

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
