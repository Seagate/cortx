/* -*- C -*- */
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
 * Original creation date: 18-Apr-2015
 */


/**
 * @addtogroup mutex
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/mutex.h"
#include "lib/thread.h"               /* m0_thread_self */
#include "lib/misc.h"                 /* M0_IS0 */

M0_INTERNAL void m0_mutex_init(struct m0_mutex *mutex)
{
	M0_SET0(mutex);
	m0_arch_mutex_init(&mutex->m_arch);
}
M0_EXPORTED(m0_mutex_init);

M0_INTERNAL void m0_mutex_fini(struct m0_mutex *mutex)
{
	M0_PRE(mutex->m_owner == NULL);
	m0_arch_mutex_fini(&mutex->m_arch);
}
M0_EXPORTED(m0_mutex_fini);

M0_INTERNAL void m0_mutex_lock(struct m0_mutex *mutex)
{
	struct m0_mutex_addb2 *ma = mutex->m_addb2;

	M0_PRE(m0_mutex_is_not_locked(mutex));
	if (ma == NULL)
		m0_arch_mutex_lock(&mutex->m_arch);
	else {
		M0_ADDB2_HIST(ma->ma_id, &ma->ma_wait, m0_ptr_wrap(mutex),
			      m0_arch_mutex_lock(&mutex->m_arch));
		ma->ma_taken = m0_time_now();
	}
	M0_ASSERT(mutex->m_owner == NULL);
	mutex->m_owner = m0_thread_self();
}
M0_EXPORTED(m0_mutex_lock);

M0_INTERNAL void m0_mutex_unlock(struct m0_mutex *mutex)
{
	struct m0_mutex_addb2 *ma = mutex->m_addb2;

	M0_PRE(m0_mutex_is_locked(mutex));
	mutex->m_owner = NULL;
	if (ma != NULL) {
		m0_time_t hold  = m0_time_now() - ma->ma_taken;
		uint64_t  datum = m0_ptr_wrap(mutex);

		m0_addb2_hist_mod_with(&ma->ma_hold, hold, datum);
		if (ma->ma_id != 0)
			M0_ADDB2_ADD(ma->ma_id + 1, hold, datum);
	}
	m0_arch_mutex_unlock(&mutex->m_arch);
}
M0_EXPORTED(m0_mutex_unlock);

M0_INTERNAL int m0_mutex_trylock(struct m0_mutex *mutex)
{
	int try = m0_arch_mutex_trylock(&mutex->m_arch);
	if (try == 0) {
		M0_ASSERT(mutex->m_owner == NULL);
		mutex->m_owner = m0_thread_self();
	}
	return try;
}
M0_EXPORTED(m0_mutex_trylock);

M0_INTERNAL bool m0_mutex_is_locked(const struct m0_mutex *mutex)
{
	return mutex->m_owner == m0_thread_self();
}
M0_EXPORTED(m0_mutex_is_locked);

M0_INTERNAL bool m0_mutex_is_not_locked(const struct m0_mutex *mutex)
{
	return mutex->m_owner != m0_thread_self();
}
M0_EXPORTED(m0_mutex_is_not_locked);

#undef M0_TRACE_SUBSYSTEM

/** @} end of XXX group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
