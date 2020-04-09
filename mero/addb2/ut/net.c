/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 10-Feb-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT

#include "ut/ut.h"
#include "addb2/net.h"

static struct m0_addb2_net *net;

static bool stopped;
static void stop_callback(struct m0_addb2_net *n, void *datum)
{
	M0_UT_ASSERT(!stopped);
	M0_UT_ASSERT(n == net);
	M0_UT_ASSERT(datum == &stopped);
	stopped = true;
}

/**
 * "net-init-fini" test: initialise and finalise network machine.
 */
static void net_init_fini(void)
{
	stopped = false;
	net = m0_addb2_net_init();
	M0_UT_ASSERT(net != NULL);
	m0_addb2_net_tick(net);
	m0_addb2_net_stop(net, &stop_callback, &stopped);
	M0_UT_ASSERT(stopped);
	m0_addb2_net_fini(net);
}

struct m0_ut_suite addb2_net_ut = {
	.ts_name = "addb2-net",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "net-init-fini",               &net_init_fini },
		{ NULL, NULL }
	}
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
