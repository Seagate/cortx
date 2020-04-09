/* -*- c -*- */
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
 * Original creation date: 26-Sep-2012
 */

#include "conf/db.h"
#include "conf/obj.h"       /* m0_conf_fid_type */
#include "conf/onwire.h"    /* m0_confx_obj, m0_confx */
#include "conf/preload.h"   /* m0_confstr_parse, m0_confx_free */
#include "conf/ut/confc.h"  /* m0_ut_conf_fids */
#include "conf/ut/common.h"
#include "lib/finject.h"    /* m0_fi_enable */
#include "lib/fs.h"         /* m0_file_read */
#include "be/ut/helper.h"   /* m0_be_ut_backend_init */
#include "ut/misc.h"        /* M0_UT_PATH */
#include "ut/ut.h"

static struct m0_be_ut_backend ut_be;
static struct m0_be_ut_seg     ut_seg;
static struct m0_be_seg       *seg;

#define XCAST(xobj, type) ((struct type *)(&(xobj)->xo_u))

static void profile_check(const struct m0_confx_obj *xobj)
{
	M0_UT_ASSERT(m0_conf_fid_type(m0_conf_objx_fid(xobj)) ==
		     &M0_CONF_PROFILE_TYPE);
	M0_UT_ASSERT(m0_fid_eq(m0_conf_objx_fid(xobj),
			       &m0_ut_conf_fids[M0_UT_CONF_PROF]));
}

static void node_check(const struct m0_confx_obj *xobj)
{
	const struct m0_confx_node *x = XCAST(xobj, m0_confx_node);

	M0_UT_ASSERT(m0_conf_fid_type(m0_conf_objx_fid(xobj)) ==
		     &M0_CONF_NODE_TYPE);
	M0_UT_ASSERT(m0_fid_eq(m0_conf_objx_fid(xobj),
		     &m0_ut_conf_fids[M0_UT_CONF_NODE]));

	M0_UT_ASSERT(x->xn_memsize == 16000);
	M0_UT_ASSERT(x->xn_nr_cpu == 2);
	M0_UT_ASSERT(x->xn_last_state == 3);
	M0_UT_ASSERT(x->xn_flags == 2);

	M0_UT_ASSERT(x->xn_processes.af_count == 2);
	M0_UT_ASSERT(m0_fid_eq(&x->xn_processes.af_elems[0],
			       &m0_ut_conf_fids[M0_UT_CONF_PROCESS0]));
	M0_UT_ASSERT(m0_fid_eq(&x->xn_processes.af_elems[1],
			       &m0_ut_conf_fids[M0_UT_CONF_PROCESS1]));
}

static void diskv_check(const struct m0_confx_obj *xobj)
{
	const struct m0_confx_objv *x = XCAST(xobj, m0_confx_objv);
	M0_UT_ASSERT(m0_conf_fid_type(m0_conf_objx_fid(xobj)) ==
		     &M0_CONF_OBJV_TYPE);
	M0_UT_ASSERT(m0_fid_eq(m0_conf_objx_fid(xobj),
			       &m0_ut_conf_fids[M0_UT_CONF_DISKV]));
	M0_UT_ASSERT(m0_fid_eq(&x->xj_real,
			       &m0_ut_conf_fids[M0_UT_CONF_DISK]));
	M0_UT_ASSERT(x->xj_children.af_count == 0);
}

static void conf_ut_db_init()
{
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1ULL << 24);
	seg = ut_seg.bus_seg;
}

static void conf_ut_db_fini()
{
	/*
	 * XXX: Call m0_ut_backend_fini_with_reqh() after
	 *      fixing m0_confdb_destroy().
	 */
	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

static int conf_ut_be_tx_create(struct m0_be_tx *tx,
				struct m0_be_ut_backend *ut_be,
				struct m0_be_tx_credit *accum)
{
	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, accum);
	return m0_be_tx_open_sync(tx);
}

static void conf_ut_be_tx_fini(struct m0_be_tx *tx)
{
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

static void test_confdb(void)
{
	struct m0_confx        *enc;
	struct m0_confx        *dec;
	struct m0_be_tx_credit  accum = {};
	struct m0_be_tx         tx = {};
	char                   *confstr = NULL;
	bool                    error_desired;
	int                     i;
	int                     j;
	int                     hit;
	int                     rc;
	struct {
		const struct m0_fid *fid;
		void (*check)(const struct m0_confx_obj *xobj);
	} tests[] = {
		{ &m0_ut_conf_fids[M0_UT_CONF_PROF],  &profile_check },
		{ &m0_ut_conf_fids[M0_UT_CONF_NODE],  &node_check    },
		{ &m0_ut_conf_fids[M0_UT_CONF_DISKV], &diskv_check   }
	};

	rc = m0_file_read(M0_UT_PATH("conf.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);

	rc = m0_confstr_parse("[0]", &enc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(enc->cx_nr == 0);
	m0_confx_free(enc);

	rc = m0_confstr_parse(confstr, &enc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(enc->cx_nr == M0_UT_CONF_NR_OBJS);
	m0_free0(&confstr);

	conf_ut_db_init();

	for (i = 0; i < 3; ++i) {
		M0_SET0(&accum);
		rc = m0_confdb_create_credit(seg, enc, &accum);
		M0_UT_ASSERT(rc == 0);
		M0_SET0(&tx);
		rc = conf_ut_be_tx_create(&tx, &ut_be, &accum);
		M0_UT_ASSERT(rc == 0);

		switch (i) {
		case 0:
			m0_fi_enable("confdb_table_init",
				     "ut_confdb_create_failure");
			error_desired = true;
			break;
		case 1:
			m0_fi_enable("confx_obj_dup",
				     "ut_confx_obj_dup_failure");
			error_desired = true;
			break;
		case 2:
			/* Usual case. Should complete without errors. */
			error_desired = false;
			break;
		}
		rc = m0_confdb_create(seg, &tx, enc);
		M0_UT_ASSERT(ergo(error_desired, rc < 0));
		M0_UT_ASSERT(ergo(!error_desired, rc == 0));

		switch (i) {
		case 0:
			m0_fi_disable("confdb_table_init",
				      "ut_confdb_create_failure");
			break;
		case 1:
			m0_fi_disable("confx_obj_dup",
				      "ut_confx_obj_dup_failure");
			error_desired = true;
			break;
		case 2:
			break;
		}
		conf_ut_be_tx_fini(&tx);
	}

	rc = m0_confdb_read(seg, &dec);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(enc->cx_nr == M0_UT_CONF_NR_OBJS);
	/*
	 * @dec can be re-ordered w.r.t. to @enc.
	 */
	for (hit = 0, i = 0; i < dec->cx_nr; ++i) {
		struct m0_confx_obj *o = M0_CONFX_AT(dec, i);

		for (j = 0; j < ARRAY_SIZE(tests); ++j) {
			if (m0_fid_eq(m0_conf_objx_fid(o), tests[j].fid)) {
				tests[j].check(o);
				hit++;
			}
		}
	}
	M0_UT_ASSERT(hit == ARRAY_SIZE(tests));

	m0_confx_free(enc);
	m0_free(dec->cx__objs);
	m0_free(dec);
	m0_confdb_fini(seg);
	M0_SET0(&accum);
	m0_confdb_destroy_credit(seg, &accum);
	rc = conf_ut_be_tx_create(&tx, &ut_be, &accum);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confdb_destroy(seg, &tx);
	M0_UT_ASSERT(rc == 0);
	conf_ut_be_tx_fini(&tx);
	conf_ut_db_fini();
}

struct m0_ut_suite confstr_ut = {
	.ts_name  = "confstr-ut",
	.ts_tests = {
		{ "db", test_confdb },
		{ NULL, NULL }
	}
};
