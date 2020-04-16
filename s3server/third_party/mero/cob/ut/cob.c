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
 * Original author: Nathan Rutman <nathan_rutman@xyratex.com>
 * Original creation date: 10/24/2010
 */

#include "ut/ut.h"
#include "lib/ub.h"
#include "lib/misc.h"              /* M0_SET0 */
#include "lib/memory.h"
#include "lib/bitstring.h"

#include "be/ut/helper.h"
#include "be/seg.h"
#include "cob/cob.h"
#include "lib/locality.h"

static const char test_name[]  = "hello_world";
static const char add_name[]   = "add_name";
static const char wrong_name[] = "wrong_name";
static struct m0_cob_domain_id id = { 42 };
static struct m0_be_ut_backend ut_be;
static struct m0_sm_group     *grp;
static struct m0_cob_domain   *dom;
static struct m0_cob          *cob;

static int ut_init(void)
{
	return 0;
}

static int ut_fini(void)
{
	return 0;
}

static void ut_tx_open(struct m0_be_tx *tx, struct m0_be_tx_credit *credit)
{
	int rc;

        m0_be_ut_tx_init(tx, &ut_be);
	m0_be_tx_prep(tx, credit);
        rc = m0_be_tx_open_sync(tx);
        M0_UT_ASSERT(rc == 0);
        M0_UT_ASSERT(m0_be_tx_state(tx) == M0_BTS_ACTIVE);
}

static int _locate(int c, int k)
{
	struct m0_fid       fid;
	struct m0_cob_oikey oikey;
	int                 rc;

	m0_fid_set(&fid, c, k);

	oikey.cok_fid = fid;
	oikey.cok_linkno = 0;

	rc = m0_cob_locate(dom, &oikey, 0, &cob);

	return rc;
}

static void test_mkfs(void)
{
	struct m0_be_tx		tx_;
	struct m0_be_tx	       *tx = &tx_;
	struct m0_be_tx_credit	accum = {};
	int			rc;
	struct m0_be_seg       *seg0;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init_cfg(&ut_be, NULL, true);
	seg0 = m0_be_domain_seg0_get(&ut_be.but_dom);

	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	rc = m0_cob_domain_create(&dom, grp, &id, &ut_be.but_dom, seg0); /*XXX*/
	M0_UT_ASSERT(rc == 0);

	m0_cob_tx_credit(dom, M0_COB_OP_DOMAIN_MKFS, &accum);
	ut_tx_open(tx, &accum);

	/* Create root and other structures */
	rc = m0_cob_domain_mkfs(dom, &M0_MDSERVICE_SLASH_FID, tx);
	M0_UT_ASSERT(rc == 0);

	rc = _locate(M0_MDSERVICE_SLASH_FID.f_container,
	             M0_MDSERVICE_SLASH_FID.f_key); /* slash */
	M0_UT_ASSERT(rc == 0);
	rc = _locate(M0_COB_ROOT_FID.f_container,
	             M0_COB_ROOT_FID.f_key); /* root */
	M0_UT_ASSERT(rc != 0);

	m0_be_tx_close_sync(tx);
        m0_be_tx_fini(tx);

	m0_cob_domain_fini(dom);

	m0_be_ut_backend_fini(&ut_be);
}

static void test_init(void)
{
	struct m0_be_seg *seg0;
	int rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init_cfg(&ut_be, NULL, false);

	seg0 = m0_be_domain_seg0_get(&ut_be.but_dom);

	rc = m0_cob_domain_init(dom, seg0);
	M0_UT_ASSERT(rc == 0);
}

static void test_fini(void)
{
	int rc;

	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	rc = m0_cob_domain_destroy(dom, grp, &ut_be.but_dom);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_backend_fini(&ut_be);
}

static void test_create(void)
{
	struct m0_be_tx_credit	accum = {};
	struct m0_cob_nskey    *key;
	struct m0_cob_nsrec	nsrec;
	struct m0_cob_fabrec  *fabrec;
	struct m0_cob_omgrec	omgrec;
	struct m0_fid		pfid;
	struct m0_be_tx		tx_;
	struct m0_be_tx	       *tx = &tx_;
	int			rc;

	M0_SET0(&nsrec);
	M0_SET0(&omgrec);

	/* pfid, filename */
	m0_fid_set(&pfid, 0x123, 0x456);
	m0_cob_nskey_make(&key, &pfid, test_name, strlen(test_name));

	m0_fid_set(&nsrec.cnr_fid, 0xabc, 0xdef);
	nsrec.cnr_nlink = 0;

	rc = m0_cob_alloc(dom, &cob);
	M0_UT_ASSERT(rc == 0);
	m0_cob_fabrec_make(&fabrec, NULL, 0);
	/* Set pool version. */
	fabrec->cfb_pver = M0_FID_TINIT('v', 1, 24);
	m0_cob_tx_credit(cob->co_dom, M0_COB_OP_CREATE, &accum);
	m0_cob_tx_credit(cob->co_dom, M0_COB_OP_CREATE, &accum);
	m0_cob_tx_credit(cob->co_dom, M0_COB_OP_UPDATE, &accum);
	ut_tx_open(tx, &accum);
	rc = m0_cob_create(cob, key, &nsrec, fabrec, &omgrec, tx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_cob_create(cob, key, &nsrec, fabrec, &omgrec, tx);
	M0_UT_ASSERT(rc == -EEXIST);

	++nsrec.cnr_nlink;
	rc = m0_cob_update(cob, &nsrec, NULL, NULL, tx);
	M0_UT_ASSERT(rc == 0);
	m0_cob_put(cob);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

static void test_add_name(void)
{
	struct m0_cob_nskey    *nskey;
	struct m0_fid		pfid;
	struct m0_be_tx		tx_;
	struct m0_be_tx	       *tx = &tx_;
	int			rc;
	struct m0_be_tx_credit	accum = {};

	/* pfid, filename */
	m0_fid_set(&pfid, 0x123, 0x456);

	m0_cob_tx_credit(dom, M0_COB_OP_NAME_ADD, &accum);
	ut_tx_open(tx, &accum);

	/* lookup for cob created before using @test_name. */
	m0_cob_nskey_make(&nskey, &pfid, test_name, strlen(test_name));
	rc = m0_cob_lookup(dom, nskey, M0_CA_NSKEY_FREE, &cob);
	M0_UT_ASSERT(rc == 0);

	/* add new name to existing cob */
	m0_cob_nskey_make(&nskey, &pfid, add_name, strlen(add_name));
	cob->co_nsrec.cnr_linkno = cob->co_nsrec.cnr_cntr;
	rc = m0_cob_name_add(cob, nskey, &cob->co_nsrec, tx);
	M0_UT_ASSERT(rc == 0);
	m0_cob_put(cob);

	/* lookup for new name */
	rc = m0_cob_lookup(dom, nskey, 0, &cob);
	M0_UT_ASSERT(rc == 0);
	m0_cob_put(cob);
	m0_free(nskey);

	/* lookup for wrong name, should fail. */
	m0_cob_nskey_make(&nskey, &pfid, wrong_name, strlen(wrong_name));
	rc = m0_cob_lookup(dom, nskey, 0, &cob);
	M0_UT_ASSERT(rc != 0);
	m0_free(nskey);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

static void test_del_name(void)
{
	struct m0_cob_nskey    *nskey;
	struct m0_fid		pfid;
	struct m0_be_tx		tx_;
	struct m0_be_tx	       *tx = &tx_;
	int			rc;
	struct m0_be_tx_credit	accum = {};

	/* pfid, filename */
	m0_fid_set(&pfid, 0x123, 0x456);

	m0_cob_tx_credit(dom, M0_COB_OP_NAME_DEL, &accum);
	ut_tx_open(tx, &accum);

	/* lookup for cob created before using @test_name. */
	m0_cob_nskey_make(&nskey, &pfid, test_name, strlen(test_name));
	rc = m0_cob_lookup(dom, nskey, M0_CA_NSKEY_FREE, &cob);
	M0_UT_ASSERT(rc == 0);

	/* del name that we created in prev test */
	m0_cob_nskey_make(&nskey, &pfid, add_name, strlen(add_name));
	rc = m0_cob_name_del(cob, nskey, tx);
	M0_UT_ASSERT(rc == 0);
	m0_cob_put(cob);

	/* lookup for new name */
	rc = m0_cob_lookup(dom, nskey, 0, &cob);
	M0_UT_ASSERT(rc != 0);
	m0_free(nskey);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

/** Lookup by name, make sure cfid is right. */
static void test_lookup(void)
{
	struct m0_cob_nskey *nskey;
	struct m0_fid        pfid;
	int                  rc;

	m0_fid_set(&pfid, 0x123, 0x456);
	m0_cob_nskey_make(&nskey, &pfid, test_name, strlen(test_name));
	rc = m0_cob_lookup(dom, nskey, M0_CA_NSKEY_FREE, &cob);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(cob != NULL);
	M0_UT_ASSERT(cob->co_dom == dom);
	M0_UT_ASSERT(cob->co_flags & M0_CA_NSREC);
	M0_UT_ASSERT(cob->co_nsrec.cnr_fid.f_container == 0xabc);
	M0_UT_ASSERT(cob->co_nsrec.cnr_fid.f_key == 0xdef);

	/* We should have cached the key also, unless oom */
	M0_UT_ASSERT(cob->co_flags & M0_CA_NSKEY);

	m0_cob_put(cob);
}

/** Lookup by fid, make sure pfid is right. */
static void test_locate(void)
{
	int rc;

	rc = _locate(0xabc, 0xdef);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(cob != NULL);
	M0_UT_ASSERT(cob->co_dom == dom);

	/* We should have saved the NSKEY */
	M0_UT_ASSERT(cob->co_flags & M0_CA_NSKEY);
	M0_UT_ASSERT(cob->co_nskey->cnk_pfid.f_container == 0x123);
	M0_UT_ASSERT(cob->co_nskey->cnk_pfid.f_key == 0x456);

	/* Assuming we looked up the NSREC at the same time */
	M0_UT_ASSERT(cob->co_flags & M0_CA_NSREC);

	m0_cob_put(cob);

	/* We should fail here, since there is no such cob */
	rc = _locate(0x123, 0x456);
	M0_UT_ASSERT(rc != 0);
}

static void test_delete(void)
{
	struct m0_be_tx		tx_;
	struct m0_be_tx	       *tx = &tx_;
	int		        rc;
	struct m0_be_tx_credit	accum = {};

	/* gets ref */
	rc = _locate(0xabc, 0xdef);
	M0_UT_ASSERT(rc == 0);

	m0_cob_tx_credit(dom, M0_COB_OP_DELETE_PUT, &accum);
	ut_tx_open(tx, &accum);
	rc = m0_cob_delete_put(cob, tx);
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);

	M0_UT_ASSERT(rc == 0);

	/* should fail now */
	rc = _locate(0xabc, 0xdef);
	M0_UT_ASSERT(rc != 0);
}

struct m0_ut_suite cob_ut = {
	.ts_name = "cob-ut",
	.ts_init = ut_init,
	.ts_fini = ut_fini,
	.ts_tests = {
		{ "cob-mkfs",     test_mkfs },
		{ "cob-init",     test_init },
		{ "cob-create",   test_create },
		{ "cob-lookup",   test_lookup },
		{ "cob-locate",   test_locate },
		{ "cob-add-name", test_add_name },
		{ "cob-del-name", test_del_name },
		{ "cob-delete",   test_delete },
		{ "cob-fini",     test_fini },
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
