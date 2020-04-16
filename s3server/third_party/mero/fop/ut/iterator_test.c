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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 01/05/2011
 */

#include "lib/errno.h"
#include "ut/ut.h"
#include "lib/memory.h"

#include "fop/fop.h"
#include "fop/ut/iterator_test.h"
#include "fop/ut/iterator_test_xc.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/rpc_machine.h"
#include "xcode/xcode.h"


/* FOP object iterator test tests iterations of the following types:
 *   - FFA_RECORD,
 *   - FFA_SEQUENCE.
 */

static struct m0_fop_type m0_fop_iterator_test_fopt;

static void fop_fini(void)
{
	m0_fop_type_fini(&m0_fop_iterator_test_fopt);
	m0_xc_fop_ut_iterator_test_fini();
}

static int fop_init(void)
{
	m0_xc_fop_ut_iterator_test_init();

	m0_fop_seg_xc->xct_flags           = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fop_vec_xc->xct_flags           = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fop_optfid_xc->xct_flags        = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fop_recursive1_xc->xct_flags    = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fop_recursive2_xc->xct_flags    = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fop_iterator_test_xc->xct_flags = M0_XCODE_TYPE_FLAG_DOM_RPC;

	M0_FOP_TYPE_INIT(&m0_fop_iterator_test_fopt,
			 .name   = "FOP Iterator Test",
			 .opcode = M0_FOP_ITERATOR_TEST_OPCODE,
			 .xt     = m0_fop_iterator_test_xc);
	return 0;
}

/* Just fill fop object fields */
static void fop_obj_init(struct m0_fop_iterator_test *fop)
{
	int i;

        m0_fid_set(&fop->fit_fid, 1, 2);

	fop->fit_vec.fv_count = 2;
	M0_ALLOC_ARR(fop->fit_vec.fv_seg, fop->fit_vec.fv_count);
	M0_UT_ASSERT(fop->fit_vec.fv_seg != NULL);
	for (i = 0; i < fop->fit_vec.fv_count; ++i) {
		fop->fit_vec.fv_seg[i].fs_count = i;
		fop->fit_vec.fv_seg[i].fs_offset = i*2;
	}

        m0_fid_set(&fop->fit_opt0.fo_fid, 131, 132);
        m0_fid_set(&fop->fit_opt1.fo_fid, 31, 32);
        m0_fid_set(&fop->fit_topt.fo_fid, 41, 42);
        m0_fid_set(&fop->fit_rec.fr_fid, 5, 6);
        m0_fid_set(&fop->fit_rec.fr_seq.fr_fid, 7, 8);
	fop->fit_rec.fr_seq.fr_seq.fv_count = 3;
	M0_ALLOC_ARR(fop->fit_rec.fr_seq.fr_seq.fv_seg,
		     fop->fit_rec.fr_seq.fr_seq.fv_count);
	M0_UT_ASSERT(fop->fit_rec.fr_seq.fr_seq.fv_seg != NULL);
	for (i = 0; i < fop->fit_rec.fr_seq.fr_seq.fv_count; ++i) {
		fop->fit_rec.fr_seq.fr_seq.fv_seg[i].fs_count = i;
		fop->fit_rec.fr_seq.fr_seq.fv_seg[i].fs_offset = i*2;
	}
	m0_fid_set(&fop->fit_rec.fr_seq.fr_unn.fo_fid, 41, 42);
}

static void fit_test(void)
{
	int                          result;
	int                          i = 0;
	struct m0_fop		    *f;
	struct m0_fid		    *fid;
	struct m0_fop_iterator_test *fop;
	struct m0_xcode_ctx          ctx;
	struct m0_xcode_cursor      *it;
	static struct m0_fid         expected[] = {
		{ 1,  2},   /* fop->fit_fid */
		{131, 132},
		{31, 32},   /* fop->fit_opt1.fo_fid */
		{41, 42},   /* fop->fit_topt.fo_fid */
		{ 5,  6},   /* fop->fit_rec.fr_fid */
		{ 7,  8},   /* fop->fit_rec.fr_seq.fr_fid */
                {41, 42}    /* fop->fit_rec.fr_seq.fr_unn.fo_fid */
	};
	struct m0_rpc_machine machine;

	m0_sm_group_init(&machine.rm_sm_grp);
	result = fop_init();
	M0_UT_ASSERT(result == 0);

	f = m0_fop_alloc(&m0_fop_iterator_test_fopt, NULL, &machine);
	M0_UT_ASSERT(f != NULL);
	fop = m0_fop_data(f);
	fop_obj_init(fop);

	m0_xcode_ctx_init(&ctx, &M0_FOP_XCODE_OBJ(f));
	it = &ctx.xcx_it;

	while ((result = m0_xcode_next(it)) > 0) {
		const struct m0_xcode_type     *xt;
		struct m0_xcode_obj            *cur;
		struct m0_xcode_cursor_frame   *top;

		top = m0_xcode_cursor_top(it);

		if (top->s_flag != M0_XCODE_CURSOR_PRE)
			continue;

		cur = &top->s_obj;
		xt  = cur->xo_type;

		if (xt == m0_fid_xc) {
			fid = cur->xo_ptr;
			M0_UT_ASSERT(fid->f_container ==
				     expected[i].f_container);
			M0_UT_ASSERT(fid->f_key ==
				     expected[i].f_key);
			++i;
		}
	}
	M0_UT_ASSERT(i == ARRAY_SIZE(expected));

	m0_fop_put_lock(f);
	fop_fini();
	m0_sm_group_fini(&machine.rm_sm_grp);
}

struct m0_ut_suite fit_ut = {
	.ts_name = "fit-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "fop-iterator", fit_test },
		{ NULL, NULL }
	}
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
