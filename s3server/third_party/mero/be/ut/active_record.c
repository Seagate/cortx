/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy Bilenko <Anatoliy.Bilenko@segate.com>
 * Original creation date: 02/02/2017
 */

#include "ut/ut.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "reqh/reqh.h"
#include "be/ut/helper.h"
#include "be/active_record.h"
#include "module/instance.h"

static struct m0_be_seg                  *seg0;
static struct m0_be_ut_backend		  ut_be;
static struct m0_be_active_record_domain *dom_created;
static struct m0_be_active_record_domain *dom;
static struct m0_be_active_record_domain  dummy;


static void ut_tx_open(struct m0_be_tx *tx, struct m0_be_tx_credit *credit)
{
	int rc;

        m0_be_ut_tx_init(tx, &ut_be);
	m0_be_tx_prep(tx, credit);
        rc = m0_be_tx_exclusive_open_sync(tx);
        M0_UT_ASSERT(rc == 0);
        M0_UT_ASSERT(m0_be_tx_state(tx) == M0_BTS_ACTIVE);
}

/*
 * 1. Create data structures, store them on disk.
 * 2. Then, load them back from disk
 */
static void actrec_mkfs(void)
{
	struct m0_be_tx		tx_;
	struct m0_be_tx	       *tx = &tx_;
	struct m0_be_tx_credit	accum = {};
	int			rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init_cfg(&ut_be, NULL, true);
	seg0  = m0_be_domain_seg0_get(&ut_be.but_dom);
	dummy.ard_seg = seg0;

	m0_be_active_record_domain_credit(&dummy, ARO_CREATE, 3, &accum);
	ut_tx_open(tx, &accum);
	rc = m0_be_active_record_domain_create(&dom_created, tx, seg0,
					       M0_BUF_INITS("xxx"),
					       M0_BUF_INITS("yyy"),
					       M0_BUF_INITS("zzz"));
	M0_UT_ASSERT(rc == 0);

	rc = m0_be_active_record_domain__invariant(dom_created);
	M0_UT_ASSERT(rc == 0);

	m0_be_tx_close_sync(tx);
        m0_be_tx_fini(tx);

	m0_be_active_record_domain_init(dom_created, seg0);
	m0_be_active_record_domain_fini(dom_created);

	m0_be_ut_backend_fini(&ut_be);
}

static void actrec_init(void)
{
	struct m0_reqh   *reqh;
	struct m0_be_seg *seg0;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init_cfg(&ut_be, NULL, false);

	reqh = ut_be.but_dom_cfg.bc_engine.bec_reqh;
	dom  = m0_reqh_lockers_get(reqh, m0_get()->i_actrec_dom_key);
	M0_UT_ASSERT(dom != NULL);
	seg0 = m0_be_domain_seg0_get(&ut_be.but_dom);

	m0_be_active_record_domain_init(dom, seg0);
	M0_UT_ASSERT(m0_be_active_record_domain__invariant(dom));
}

static void actrec_fini(void)
{
	struct m0_be_tx		tx_;
	struct m0_be_tx	       *tx = &tx_;
	struct m0_be_tx_credit	accum = {};
	int                     rc;

	m0_be_active_record_domain_credit(dom, ARO_DESTROY, 3, &accum);
	ut_tx_open(tx, &accum);
	rc = m0_be_active_record_domain_destroy(dom, tx);
	M0_UT_ASSERT(rc == 0);
	m0_be_tx_close_sync(tx);
        m0_be_tx_fini(tx);

	m0_be_ut_backend_fini(&ut_be);
}

static void actrec_case(void)
{
	struct m0_be_tx		    tx_;
	struct m0_be_tx	           *tx = &tx_;
	struct m0_be_tx_credit	    create  = {};
	struct m0_be_tx_credit	    destroy = {};
	struct m0_be_tx_credit	    add     = {};
	struct m0_be_tx_credit	    del     = {};
	struct m0_be_tx_credit	    accum   = {};
	struct m0_be_active_record  dummy   = { .ar_dom = dom };
	struct m0_be_active_record *rec1;
	struct m0_be_active_record *rec2;
	struct m0_be_active_record *rec3;
	int                         rc;

	m0_be_active_record_credit(&dummy, ARO_CREATE, &create);
	m0_be_tx_credit_mac(&accum, &create, 3);

	m0_be_active_record_credit(&dummy, ARO_ADD, &add);
	m0_be_tx_credit_mac(&accum, &add, 3);

	m0_be_active_record_credit(&dummy, ARO_DEL, &del);
	m0_be_tx_credit_mac(&accum, &del, 3);

	m0_be_active_record_credit(&dummy, ARO_DESTROY, &destroy);
	m0_be_tx_credit_mac(&accum, &destroy, 3);

	ut_tx_open(tx, &accum);

	rc =
		m0_be_active_record_create(&rec1, tx, dom) ?:
		m0_be_active_record_create(&rec2, tx, dom) ?:
		m0_be_active_record_create(&rec3, tx, dom) ?:

		m0_be_active_record_add("xxx", rec1, tx) ?:
		m0_be_active_record_add("yyy", rec2, tx) ?:
		m0_be_active_record_add("xxx", rec3, tx) ?:

		m0_be_active_record_del("xxx", rec1, tx) ?:
		m0_be_active_record_del("yyy", rec2, tx) ?:
		m0_be_active_record_del("xxx", rec3, tx) ?:

		m0_be_active_record_destroy(rec1, tx) ?:
		m0_be_active_record_destroy(rec2, tx) ?:
		m0_be_active_record_destroy(rec3, tx);

	M0_UT_ASSERT(rc == 0);

	m0_be_tx_close_sync(tx);
        m0_be_tx_fini(tx);
}

void m0_be_ut_actrec_test(void)
{
	actrec_mkfs();
	actrec_init();
	actrec_case();
	actrec_fini();
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
