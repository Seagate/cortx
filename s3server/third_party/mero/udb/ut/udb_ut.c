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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 01/24/2011
 */

#include "lib/types.h"
#include "ut/ut.h"
#include "lib/misc.h" /* M0_SET0() */

#include "udb/udb.h"

static void cred_init(struct m0_udb_cred *cred,
		      enum m0_udb_cred_type type,
		      struct m0_udb_domain *dom)
{
	cred->uc_type = type;
	cred->uc_domain = dom;
}

static void cred_fini(struct m0_udb_cred *cred)
{
}

/* uncomment when realization ready */
#if 0
static bool cred_cmp(struct m0_udb_cred *left,
		     struct m0_udb_cred *right)
{
	return
		left->uc_type == right->uc_type &&
		left->uc_domain == right->uc_domain;
}
#endif

static void udb_test(void)
{
	int ret;
	struct m0_udb_domain dom;
	struct m0_udb_ctxt   ctx;
	struct m0_udb_cred   external;
	struct m0_udb_cred   internal;
	struct m0_udb_cred   testcred;

	cred_init(&external, M0_UDB_CRED_EXTERNAL, &dom);
	cred_init(&internal, M0_UDB_CRED_INTERNAL, &dom);

	ret = m0_udb_ctxt_init(&ctx);
	M0_UT_ASSERT(ret == 0);

	/* add mapping */
	ret = m0_udb_add(&ctx, &dom, &external, &internal);
	M0_UT_ASSERT(ret == 0);

	M0_SET0(&testcred);
	ret = m0_udb_e2i(&ctx, &external, &testcred);
	/* means that mapping exists */
	M0_UT_ASSERT(ret == 0);
/* uncomment when realization ready */
#if 0
	/* successfully mapped */
	M0_UT_ASSERT(cred_cmp(&internal, &testcred));
#endif
	M0_SET0(&testcred);
	ret = m0_udb_i2e(&ctx, &internal, &testcred);
	/* means that mapping exists */
	M0_UT_ASSERT(ret == 0);
/* uncomment when realization ready */
#if 0
	/* successfully mapped */
	M0_UT_ASSERT(cred_cmp(&external, &testcred));
#endif
	/* delete mapping */
	ret = m0_udb_del(&ctx, &dom, &external, &internal);
	M0_UT_ASSERT(ret == 0);

/* uncomment when realization ready */
#if 0
	/* check that mapping does not exist */
	M0_SET0(&testcred);
	ret = m0_udb_e2i(&ctx, &external, &testcred);
	M0_UT_ASSERT(ret != 0);

	/* check that mapping does not exist */
	M0_SET0(&testcred);
	ret = m0_udb_i2e(&ctx, &internal, &testcred);
	M0_UT_ASSERT(ret != 0);
#endif
	cred_fini(&internal);
	cred_fini(&external);
	m0_udb_ctxt_fini(&ctx);
}

struct m0_ut_suite udb_ut = {
        .ts_name = "udb-ut",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "udb", udb_test },
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
