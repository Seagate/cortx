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
 * Original author: Huang Hua <hua_huang@xyratex.com>
 *		    Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 12/02/2010
 */

#include "lib/rwlock.h"
#include "lib/assert.h"

/**
   @addtogroup rwlock Read-write lock

   @{
 */

M0_INTERNAL void m0_rwlock_init(struct m0_rwlock *lock)
{
	init_rwsem(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_fini(struct m0_rwlock *lock)
{
	M0_ASSERT(!rwsem_is_locked(&lock->rw_sem));
}

M0_INTERNAL void m0_rwlock_write_lock(struct m0_rwlock *lock)
{
	down_write(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_write_unlock(struct m0_rwlock *lock)
{
	up_write(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_read_lock(struct m0_rwlock *lock)
{
	down_read(&lock->rw_sem);
}

M0_INTERNAL void m0_rwlock_read_unlock(struct m0_rwlock *lock)
{
	up_read(&lock->rw_sem);
}

/** @} end of rwlock group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
