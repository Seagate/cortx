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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 * Original creation date: 07/31/2013
 */
#include "lib/finject.h"
#include "ut/ut.h"

#include "stats/ut/stats_ut_svc.c"

static int stats_ut_init(void)
{
	return 0;
}

static int stats_ut_fini(void)
{
	return 0;
}

struct m0_ut_suite stats_ut = {
	.ts_name  = "stats-ut",
	.ts_init  = stats_ut_init,
	.ts_fini  = stats_ut_fini,
	.ts_tests = {
		{ "stats-svc-start-stop", stats_ut_svc_start_stop },
		{ "stats-svc-update-fom", stats_ut_svc_update_fom },
		{ "stats-svc-query-fom",  stats_ut_svc_query_fom },
		{ "stats-svc-query-api",  stats_svc_query_api },
		{ NULL,	NULL}
	}
};
M0_EXPORTED(stats_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
