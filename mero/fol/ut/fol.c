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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 16-Sep-2010
 */

#include "be/tx.h"
#include "fol/fol.h"
#include "fol/fol_private.h"
#include "fol/fol_xc.h"
#include "ut/be.h"
/* XXX FIXME: Do not use ut/ directory of other subsystem. */
#include "be/ut/helper.h"   /* m0_be_ut_backend */

#include "fid/fid_xc.h"
#include "rpc/rpc_opcodes.h"
#include "lib/memory.h"
#include "lib/misc.h"         /* M0_SET0 */

#include "ut/ut.h"
#include "lib/ub.h"

static struct m0_fol             g_fol;
static struct m0_fol_rec         g_rec;
static struct m0_be_ut_backend   g_ut_be;
static struct m0_be_ut_seg       g_ut_seg;
static struct m0_be_tx           g_tx;

static int verify_frag_data(struct m0_fol_frag *frag,
			    struct m0_be_tx *tx);
M0_FOL_FRAG_TYPE_DECLARE(ut_frag, static, verify_frag_data, NULL,
				NULL, NULL);

static void test_init(void)
{
	m0_ut_backend_init(&g_ut_be, &g_ut_seg);

	m0_ut_be_tx_begin2(&g_tx, &g_ut_be, &M0_BE_TX_CREDIT(0, 0),
				FOL_REC_MAXSIZE);
	m0_fol_init(&g_fol);
}

static void test_fini(void)
{
	m0_fol_fini(&g_fol);
	m0_ut_be_tx_end(&g_tx);
	m0_ut_backend_fini(&g_ut_be, &g_ut_seg);
}

static void test_fol_frag_type_reg(void)
{
	int rc;

	ut_frag_type = M0_FOL_FRAG_TYPE_XC_OPS("UT record frag", m0_fid_xc,
						   &ut_frag_type_ops);
	rc = m0_fol_frag_type_register(&ut_frag_type);
	M0_ASSERT(rc == 0);
	M0_ASSERT(ut_frag_type.rpt_index > 0);
}

static void test_fol_frag_type_unreg(void)
{
	m0_fol_frag_type_deregister(&ut_frag_type);
	M0_ASSERT(ut_frag_type.rpt_ops == NULL);
	M0_ASSERT(ut_frag_type.rpt_xt == NULL);
	M0_ASSERT(ut_frag_type.rpt_index == 0);
}

static int verify_frag_data(struct m0_fol_frag *frag,
			    struct m0_be_tx *_)
{
	struct m0_fid *dec_rec;

	dec_rec = frag->rp_data;
	M0_UT_ASSERT(dec_rec->f_container == 22);
	M0_UT_ASSERT(dec_rec->f_key == 33);
	return 0;
}

static void test_fol_frag_encdec(void)
{
	struct m0_fid      *rec;
	struct m0_fol_rec   dec_rec;
	struct m0_fol_frag *dec_frag;
	struct m0_fol_frag  ut_rec_frag = {};
	struct m0_buf      *buf         = &g_tx.t_payload;
	int                 rc;

	m0_fol_rec_init(&g_rec, &g_fol);

	M0_ALLOC_PTR(rec);
	M0_UT_ASSERT(rec != NULL);
	*rec = (struct m0_fid){ .f_container = 22, .f_key = 33 };

	m0_fol_frag_init(&ut_rec_frag, rec, &ut_frag_type);
	m0_fol_frag_add(&g_rec, &ut_rec_frag);

	/* Note, this function sets actual tx payload size for the buf. */
	rc = m0_fol_rec_encode(&g_rec, buf);
	M0_UT_ASSERT(rc == 0);

	m0_fol_rec_fini(&g_rec);

	m0_fol_rec_init(&dec_rec, &g_fol);
	rc = m0_fol_rec_decode(&dec_rec, buf);
	M0_UT_ASSERT(rc == 0);

	m0_tl_for(m0_rec_frag, &dec_rec.fr_frags, dec_frag) {
		/* Call verify_part_data() for each part. */
		dec_frag->rp_ops->rpo_undo(dec_frag, &g_tx);
	} m0_tl_endfor;
	m0_fol_rec_fini(&dec_rec);
}

struct m0_ut_suite fol_ut = {
	.ts_name = "fol-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		/*
		 * Note, that there are dependencies between these tests.
		 * Do not reorder them willy-nilly.
		 */
		{ "fol-init",                test_init                },
		{ "fol-frag-type-reg",       test_fol_frag_type_reg   },
		{ "fol-frag-test",           test_fol_frag_encdec     },
		{ "fol-frag-type-unreg",     test_fol_frag_type_unreg },
		{ "fol-fini",                test_fini                },
		{ NULL, NULL }
	}
};

/* ------------------------------------------------------------------
 * UB
 * ------------------------------------------------------------------ */

enum { UB_ITER = 100000 };

static int ub_init(const char *opts M0_UNUSED)
{
	test_init();
	test_fol_frag_type_reg();
	return 0;
}

static void ub_fini(void)
{
	test_fol_frag_type_unreg();
	test_fini();
}

#if 0 /* Convert to BE. */
static m0_lsn_t last;

static void checkpoint()
{
	rc = m0_db_tx_commit(&tx);
	M0_ASSERT(rc == 0);

	rc = m0_db_tx_init(&tx, &db, 0);
	M0_ASSERT(rc == 0);
}

static void ub_insert(int i)
{
	d->rd_lsn = m0_fol_lsn_allocate(&fol);
	rc = m0_fol_rec_encode(&fol, &tx, &r);
	M0_ASSERT(rc == 0);
	last = d->rd_lsn;
	if (i % 1000 == 0)
		checkpoint();
}

static void ub_lookup(int i)
{
	m0_lsn_t lsn;
	struct m0_fol_rec rec;

	lsn = last - i;

	rc = m0_fol_rec_lookup(&fol, &tx, lsn, &rec);
	M0_ASSERT(rc == 0);
	m0_fol_lookup_rec_fini(&rec);
	if (i % 1000 == 0)
		checkpoint();
}

static void ub_insert_buf(int i)
{
	d->rd_lsn = m0_fol_lsn_allocate(&fol);
	rc = m0_fol_add_buf(&fol, &tx, d, &buf);
	M0_ASSERT(rc == 0);
	if (i % 1000 == 0)
		checkpoint();
}
#endif

struct m0_ub_set m0_fol_ub = {
	.us_name = "fol-ub",
	.us_init = ub_init,
	.us_fini = ub_fini,
	.us_run  = {
#if 0 /* Convert to BE. */
		{ .ub_name = "insert",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_insert },

		{ .ub_name = "lookup",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_lookup },

		{ .ub_name = "insert-buf",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_insert_buf },
#endif
		{ .ub_name = NULL }
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
