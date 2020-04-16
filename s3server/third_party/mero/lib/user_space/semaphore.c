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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 03/11/2011
 */

#include "lib/semaphore.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/types.h"      /* INT_MAX */
#include "lib/arith.h"      /* min_check */

/**
   @addtogroup semaphore

   Implementation of m0_semaphore on top of sem_t.

   @{
*/

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value)
{
	int rc;
	int err;

	rc = sem_init(&semaphore->s_sem, 0, value);
	err = rc == 0 ? 0 : errno;
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, err);
	return -err;
}

M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore)
{
	int rc;

	rc = sem_destroy(&semaphore->s_sem);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
}

M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore)
{
	int rc;

	do
		rc = sem_wait(&semaphore->s_sem);
	while (rc == -1 && errno == EINTR);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
}

M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore)
{
	int rc;

	rc = sem_post(&semaphore->s_sem);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
}

M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore)
{
	int rc;

	do
		rc = sem_trywait(&semaphore->s_sem);
	while (rc == -1 && errno == EINTR);
	M0_ASSERT_INFO(rc == 0 || (rc == -1 && errno == EAGAIN),
	               "rc=%d errno=%d", rc, errno);
	errno = 0;
	return rc == 0;
}

M0_INTERNAL unsigned m0_semaphore_value(struct m0_semaphore *semaphore)
{
	int rc;
	int result;

	rc = sem_getvalue(&semaphore->s_sem, &result);
	M0_ASSERT_INFO(rc == 0, "rc=%d errno=%d", rc, errno);
	M0_POST(result >= 0);
	return result;
}

M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t abs_timeout)
{
	m0_time_t	abs_timeout_realtime = m0_time_to_realtime(abs_timeout);
	struct timespec ts = {
			.tv_sec  = m0_time_seconds(abs_timeout_realtime),
			.tv_nsec = m0_time_nanoseconds(abs_timeout_realtime)
	};
	int		rc;

	/*
	 * Workaround for sem_timedwait(3) on Centos >= 7.2, which returns
	 * -ETIMEDOUT immediately if tv_sec is greater than
	 * gettimeofday(2) + INT_MAX.
	 *
	 *  For more information refer to:
	 *    https://bugzilla.redhat.com/show_bug.cgi?id=1412082
	 *    https://jts.seagate.com/browse/CASTOR-1990
	 *    `git blame` these lines and read commit message
	 *    doc/workarounds.md
	 *
	 *  It should be reverted when glibc is fixed in future RedHat releases.
	 */
	ts.tv_sec = min_check(ts.tv_sec, (time_t)(INT_MAX - 1));
	/* ----- end of workaround ----- */

	do
		rc = sem_timedwait(&semaphore->s_sem, &ts);
	while (rc == -1 && errno == EINTR);
	M0_ASSERT_INFO(rc == 0 || (rc == -1 && errno == ETIMEDOUT),
	               "rc=%d errno=%d", rc, errno);
	if (rc == -1 && errno == ETIMEDOUT)
		errno = 0;
	return rc == 0;
}

/** @} end of semaphore group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
