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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 05/13/2010
 */

#pragma once

#ifndef __MERO_LIB_MUTEX_H__
#define __MERO_LIB_MUTEX_H__

#include "lib/types.h"
#include "addb2/histogram.h"

/**
   @defgroup mutex Mutual exclusion synchronisation object
   @{
*/

#ifndef __KERNEL__
#include "lib/user_space/mutex.h"
#else
#include "lib/linux_kernel/mutex.h"
#endif

/* struct m0_arch_mutex is defined by headers above. */

struct m0_mutex_addb2;
struct m0_thread;

struct m0_mutex {
	struct m0_arch_mutex   m_arch;
	struct m0_thread      *m_owner;
	struct m0_mutex_addb2 *m_addb2;
};

/**
 * Mutex static initialiser.
 *
 * @code
 * static struct m0_mutex lock = M0_MUTEX_SINIT(&lock);
 * @endcode
 *
 * This macro is useful only for global static mutexes, in other cases
 * m0_mutex_init() should be used.
 */
#define M0_MUTEX_SINIT(m) { .m_arch = M0_ARCH_MUTEX_SINIT((m)->m_arch) }

M0_INTERNAL void m0_mutex_init(struct m0_mutex *mutex);
M0_INTERNAL void m0_mutex_fini(struct m0_mutex *mutex);

/**
   Returns with the mutex locked.

   @pre  m0_mutex_is_not_locked(mutex)
   @post m0_mutex_is_locked(mutex)
*/
M0_INTERNAL void m0_mutex_lock(struct m0_mutex *mutex);

/**
   Unlocks the mutex.

   @pre  m0_mutex_is_locked(mutex)
   @post m0_mutex_is_not_locked(mutex)
*/
M0_INTERNAL void m0_mutex_unlock(struct m0_mutex *mutex);

/**
   Try to take a mutex lock.
   Returns 0 with the mutex locked,
   or non-zero if lock is already hold by others.
*/
M0_INTERNAL int m0_mutex_trylock(struct m0_mutex *mutex);


/**
   True iff mutex is locked by the calling thread.

   @note this function can be used only in assertions.
*/
M0_INTERNAL bool m0_mutex_is_locked(const struct m0_mutex *mutex);

/**
   True iff mutex is not locked by the calling thread.

   @note this function can be used only in assertions.
*/
M0_INTERNAL bool m0_mutex_is_not_locked(const struct m0_mutex *mutex);

struct m0_mutex_addb2 {
	m0_time_t            ma_taken;
	struct m0_addb2_hist ma_hold;
	struct m0_addb2_hist ma_wait;
	uint64_t             ma_id;
};

/*
 * Arch functions, implemented in lib/$ARCH/?mutex.c.
 */

M0_INTERNAL void m0_arch_mutex_init   (struct m0_arch_mutex *mutex);
M0_INTERNAL void m0_arch_mutex_fini   (struct m0_arch_mutex *mutex);
M0_INTERNAL void m0_arch_mutex_lock   (struct m0_arch_mutex *mutex);
M0_INTERNAL void m0_arch_mutex_unlock (struct m0_arch_mutex *mutex);
M0_INTERNAL int  m0_arch_mutex_trylock(struct m0_arch_mutex *mutex);

/** @} end of mutex group */

/* __MERO_LIB_MUTEX_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
