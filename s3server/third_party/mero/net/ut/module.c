/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 21-Jan-2014
 */

#include "net/module.h"
#include "module/instance.h"  /* m0_get */
#include "lib/memory.h"       /* m0_free0 */
#include "ut/ut.h"

static void test_net_modules(void)
{
	struct m0        *inst = m0_get();
	struct m0_module *net;
	struct m0_module *xprt;
	int               rc;

	M0_UT_ASSERT(inst->i_moddata[M0_MODULE_NET] == NULL);
	net = m0_net_module_type.mt_create(inst);
	M0_UT_ASSERT(net != NULL);
	M0_UT_ASSERT(inst->i_moddata[M0_MODULE_NET] ==
		     container_of(net, struct m0_net_module, n_module));

	xprt = &((struct m0_net_module *)inst->i_moddata[M0_MODULE_NET])
		->n_xprts[M0_NET_XPRT_BULKMEM].nx_module;
	M0_UT_ASSERT(xprt->m_cur == M0_MODLEV_NONE);
	M0_UT_ASSERT(net->m_cur == M0_MODLEV_NONE);

	rc = m0_module_init(xprt, M0_LEVEL_NET_DOMAIN);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(xprt->m_cur == M0_LEVEL_NET_DOMAIN);
	M0_UT_ASSERT(net->m_cur == M0_LEVEL_NET);

	m0_module_fini(xprt, M0_MODLEV_NONE);
	M0_UT_ASSERT(xprt->m_cur == M0_MODLEV_NONE);
	M0_UT_ASSERT(net->m_cur == M0_MODLEV_NONE);

	m0_free0(&inst->i_moddata[M0_MODULE_NET]);
}

struct m0_ut_suite m0_net_module_ut = {
	.ts_name  = "net-module",
	.ts_tests = {
		{ "test", test_net_modules },
		{ NULL, NULL }
	}
};
