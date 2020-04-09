/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 9-Mar-2016
 */

#include "net/net.h"  /* m0_net_endpoint_is_valid */
#include "ut/ut.h"

static void test_endpoint_is_valid(void)
{
	const char *good[] = {
		"0@lo:12345:34:1",
		"172.18.1.1@tcp:12345:40:401",
		"255.0.0.0@tcp:12345:1:1",
		"172.18.50.40@o2ib:12345:34:1",
		"172.18.50.40@o2ib1:12345:34:1",
	};
	const char *bad[] = {
		"",
		" 172.18.1.1@tcp:12345:40:401",
		"172.18.1.1@tcp:12345:40:401 ",
		"1@lo:12345:34:1",
		"172.16.64.1:12345:45:41",
		"256.0.0.0@tcp:12345:1:1",
		"172.18.50.40@o2ib:54321:34:1",
	};

	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(good),
			       m0_net_endpoint_is_valid(good[i])));
	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(bad),
			       !m0_net_endpoint_is_valid(bad[i])));
}

struct m0_ut_suite m0_net_misc_ut = {
	.ts_name  = "net-misc-ut",
	.ts_tests = {
		{ "endpoint-is-valid", test_endpoint_is_valid },
		{ NULL, NULL }
	}
};
