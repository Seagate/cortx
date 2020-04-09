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
 * Original author: Nathan Rutman <Nathan_Rutman@xyratex.com>,
 *                  Huang Hua <Hua_Huang@xyratex.com>
 *		    Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 01/27/2011
 */

#include "lib/time.h"            /* m0_time_t */
#include "lib/time_internal.h"   /* m0_clock_gettime_wrapper */

#include "lib/assert.h"          /* M0_ASSERT */
#include "lib/misc.h"            /* M0_IN */
#include "lib/errno.h"           /* ENOSYS */

#include <sys/time.h>            /* gettimeofday */
#include <time.h>                /* clock_gettime */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

M0_INTERNAL m0_time_t m0_clock_gettime_wrapper(enum CLOCK_SOURCES clock_id)
{
	struct timespec tp;
	int             rc;

	rc = clock_gettime((clockid_t)clock_id, &tp);
	/* clock_gettime() can fail iff clock_id is invalid */
	M0_ASSERT(rc == 0);
	return M0_MKTIME(tp.tv_sec, tp.tv_nsec);
}

M0_INTERNAL m0_time_t m0_clock_gettimeofday_wrapper(void)
{
	struct timeval tv;
	int            rc;

	rc = gettimeofday(&tv, NULL);
	M0_ASSERT(rc == 0);
	return M0_MKTIME(tv.tv_sec, tv.tv_usec * 1000);
}

/** Sleep for requested time */
int m0_nanosleep(const m0_time_t req, m0_time_t *rem)
{
	struct timespec	reqts = {
		.tv_sec  = m0_time_seconds(req),
		.tv_nsec = m0_time_nanoseconds(req)
	};
	struct timespec remts;
	int		rc;

	rc = nanosleep(&reqts, &remts);
	if (rem != NULL)
		*rem = rc != 0 ? m0_time(remts.tv_sec, remts.tv_nsec) : 0;
	return M0_RC(rc);
}
M0_EXPORTED(m0_nanosleep);

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
