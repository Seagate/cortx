/* -*- C -*- */
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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 03/11/2011
 */

#include <linux/jiffies.h>  /* timespec_to_jiffies */
#include "lib/semaphore.h"
#include "lib/assert.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"

/**
   @addtogroup semaphore

   <b>Implementation of m0_semaphore on top of Linux struct semaphore.</b>

   @{
 */

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value)
{
	sema_init(&semaphore->s_sem, value);
	return 0;
}

M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore)
{
}

M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore)
{
	int flag_was_set = 0;

	while (down_interruptible(&semaphore->s_sem) != 0)
		flag_was_set |= test_and_clear_thread_flag(TIF_SIGPENDING);

	if (flag_was_set)
		set_thread_flag(TIF_SIGPENDING);
}

M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore)
{
	return !down_trylock(&semaphore->s_sem);
}

M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore)
{
	up(&semaphore->s_sem);
}

M0_INTERNAL unsigned m0_semaphore_value(struct m0_semaphore *semaphore)
{
	return semaphore->s_sem.count;
}

M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t abs_timeout)
{
	m0_time_t       nowtime = m0_time_now();
	m0_time_t       abs_timeout_realtime = m0_time_to_realtime(abs_timeout);
	m0_time_t       reltime;
	unsigned long   reljiffies;
	struct timespec ts;

	/* same semantics as user_space semaphore: allow abs_time < now */
	if (abs_timeout_realtime > nowtime)
		reltime = m0_time_sub(abs_timeout_realtime, nowtime);
	else
		reltime = 0;
	ts.tv_sec  = m0_time_seconds(reltime);
	ts.tv_nsec = m0_time_nanoseconds(reltime);
	reljiffies = timespec_to_jiffies(&ts);
	return down_timeout(&semaphore->s_sem, reljiffies) == 0;
}

/** @} end of semaphore group */

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
