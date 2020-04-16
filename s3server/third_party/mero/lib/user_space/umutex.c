/* -*- C -*- */
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 05/13/2010
 */

#include "lib/misc.h"   /* M0_SET0 */
#include "lib/mutex.h"
#include "lib/assert.h"
#include "lib/errno.h"  /* EBUSY */

/**
   @addtogroup mutex

   Implementation of m0_arch_mutex on top of pthread_mutex_t.

   @{
*/

M0_INTERNAL void m0_arch_mutex_init(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_init(&mutex->m_impl, NULL);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL void m0_arch_mutex_fini(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_destroy(&mutex->m_impl);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL void m0_arch_mutex_lock(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_lock(&mutex->m_impl);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL void m0_arch_mutex_unlock(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_unlock(&mutex->m_impl);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
}

M0_INTERNAL int m0_arch_mutex_trylock(struct m0_arch_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_trylock(&mutex->m_impl);
	M0_ASSERT_INFO(M0_IN(rc, (0, EBUSY)), "rc=%d", rc);
	return rc;
}

/** @} end of mutex group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
