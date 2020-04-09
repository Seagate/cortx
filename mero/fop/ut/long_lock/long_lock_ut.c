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
 * Original creation date: 08/06/2012
 */

#include "ut/ut.h"
#include "fop/fop.h"
#include "reqh/reqh.h"
#include "rpc/rpclib.h"
#include "rpc/rpc_opcodes.h"   /* M0_UT_RDWR_OPCODE */
#include "net/lnet/lnet.h"
#include "fop/fom_generic.h"  /* m0_generic_conf */
#include "ut/ut_rpc_machine.h"

enum {
	RDWR_REQUEST_MAX = 48,
	REQH_IN_UT_MAX   = 2
};

#include "fop/ut/long_lock/rdwr_fom.c"
#include "fop/ut/long_lock/rdwr_test_bench.c"

static const char *ll_serv_addr[] = { "0@lo:12345:34:1",
				      "0@lo:12345:34:2"
};

static const int ll_cob_ids[] = { 20, 30 };

static struct m0_ut_rpc_mach_ctx     rmach_ctx[REQH_IN_UT_MAX];
static struct m0_reqh_service       *service[REQH_IN_UT_MAX];
extern struct m0_fom_type            rdwr_fom_type;
extern const struct m0_fom_type_ops  fom_rdwr_type_ops;

static void test_long_lock_n(void)
{
	static struct m0_reqh *r[REQH_IN_UT_MAX] = { &rmach_ctx[0].rmc_reqh,
						     &rmach_ctx[1].rmc_reqh };
	rdwr_send_fop(r, REQH_IN_UT_MAX);
}

static void test_long_lock_1(void)
{
	static struct m0_reqh *r[1] = { &rmach_ctx[0].rmc_reqh };

	rdwr_send_fop(r, 1);
}

static int ut_long_lock_service_start(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	return 0;
}

static void ut_long_lock_service_stop(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
}

static void ut_long_lock_service_fini(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	m0_free(service);
}

static const struct m0_reqh_service_ops ut_long_lock_service_ops = {
	.rso_start = ut_long_lock_service_start,
	.rso_stop  = ut_long_lock_service_stop,
	.rso_fini  = ut_long_lock_service_fini
};

static int
ut_long_lock_service_allocate(struct m0_reqh_service **service,
			      const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_service *serv;

	M0_PRE(stype != NULL && service != NULL);

	M0_ALLOC_PTR(serv);
	M0_ASSERT(serv != NULL);

	serv->rs_type = stype;
	serv->rs_ops = &ut_long_lock_service_ops;
	*service = serv;
	return 0;
}

static const struct m0_reqh_service_type_ops ut_long_lock_service_type_ops = {
	.rsto_service_allocate = ut_long_lock_service_allocate
};

struct m0_reqh_service_type ut_long_lock_service_type = {
	.rst_name  = "ut-long-lock-service",
	.rst_ops   = &ut_long_lock_service_type_ops,
	.rst_level = M0_RS_LEVEL_NORMAL,
};

static int test_long_lock_init(void)
{
	int rc;
	int i;

	rc = m0_reqh_service_type_register(&ut_long_lock_service_type);
	M0_ASSERT(rc == 0);
	m0_fom_type_init(&rdwr_fom_type, M0_UT_RDWR_OPCODE, &fom_rdwr_type_ops,
			 &ut_long_lock_service_type,
			 &m0_generic_conf);
	/*
	 * Instead of using m0d and dealing with network, database and
	 * other subsystems, request handler is initialised in a 'special way'.
	 * This allows it to operate in a 'limited mode' which is enough for
	 * this test.
	 */
	for (i = 0; i < REQH_IN_UT_MAX; ++i) {
		M0_SET0(&rmach_ctx[i]);
		rmach_ctx[i].rmc_cob_id.id = ll_cob_ids[i];
		rmach_ctx[i].rmc_ep_addr   = ll_serv_addr[i];
		m0_ut_rpc_mach_init_and_add(&rmach_ctx[i]);
	}
	for (i = 0; i < REQH_IN_UT_MAX; ++i) {
		rc = m0_reqh_service_allocate(&service[i],
					      &ut_long_lock_service_type, NULL);
		M0_ASSERT(rc == 0);
		m0_reqh_service_init(service[i], &rmach_ctx[i].rmc_reqh, NULL);
		rc = m0_reqh_service_start(service[i]);
		M0_ASSERT(rc == 0);
	}
	return rc;
}

static int test_long_lock_fini(void)
{
	int i;

	for (i = 0; i < REQH_IN_UT_MAX; ++i)
		m0_ut_rpc_mach_fini(&rmach_ctx[i]);

	m0_reqh_service_type_unregister(&ut_long_lock_service_type);

	return 0;
}

struct m0_ut_suite m0_fop_lock_ut = {
	.ts_name = "fop-lock-ut",
	.ts_init = test_long_lock_init,
	.ts_fini = test_long_lock_fini,
	.ts_tests = {
		{ "fop-lock-1reqh", test_long_lock_1 },
		{ "fop-lock-2reqh", test_long_lock_n },
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
