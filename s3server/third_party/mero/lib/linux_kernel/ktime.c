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
 * Original author: Nathan Rutman <nathan_rutman@xyratex.com>
 *		    Huang Hua <hua_huang@xyratex.com>
 * Original creation date: 12/06/2010
 */

#include "lib/time.h"           /* m0_time_t */
#include "lib/time_internal.h"  /* m0_clock_gettime_wrapper */
#include "lib/misc.h"           /* M0_EXPORTED */

#include <linux/module.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/sched.h>

/**
   @addtogroup time

   <b>Implementation of m0_time_t on top of kernel struct timespec

   @{
*/

M0_INTERNAL m0_time_t m0_clock_gettime_wrapper(enum CLOCK_SOURCES clock_id)
{
	struct timespec ts;
	m0_time_t       ret;

	switch (clock_id) {
	case M0_CLOCK_SOURCE_MONOTONIC:
		getrawmonotonic(&ts);
		ret = M0_MKTIME(ts.tv_sec, ts.tv_nsec);
		break;
	case M0_CLOCK_SOURCE_REALTIME:
		/* ts = current_kernel_time(); */
		getnstimeofday(&ts);
		ret = M0_MKTIME(ts.tv_sec, ts.tv_nsec);
		break;
	default:
		M0_IMPOSSIBLE("Unknown clock source");
		ret = M0_MKTIME(0, 0);
	}
	return ret;
}

M0_INTERNAL m0_time_t m0_clock_gettimeofday_wrapper(void)
{
	struct timespec ts;

	getnstimeofday(&ts);
	return M0_MKTIME(ts.tv_sec, ts.tv_nsec);
}

/**
   Sleep for requested time
*/
int m0_nanosleep(const m0_time_t req, m0_time_t *rem)
{
	struct timespec ts = {
		.tv_sec  = m0_time_seconds(req),
		.tv_nsec = m0_time_nanoseconds(req)
	};
	unsigned long	tj = timespec_to_jiffies(&ts);
	unsigned long	remtj;
	struct timespec remts;

	/* this may use schedule_timeout_interruptible() to capture signals */
	remtj = schedule_timeout_uninterruptible(tj);
	M0_ASSERT(remtj >= 0);
	if (rem != NULL) {
		jiffies_to_timespec(remtj, &remts);
		*rem = m0_time(remts.tv_sec, remts.tv_nsec);
	}
	return remtj == 0 ? 0 : -1;
}
M0_EXPORTED(m0_nanosleep);

/** @} end of time group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
