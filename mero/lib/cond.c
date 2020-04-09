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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 05/17/2010
 */

#include "lib/cond.h"
#include "lib/mutex.h"
#include "lib/assert.h"

/**
   @addtogroup cond

   Very simple implementation of condition variables on top of waiting
   channels.

   Self-explanatory.

   @see m0_chan

   @{
 */

M0_INTERNAL void m0_cond_init(struct m0_cond *cond, struct m0_mutex *mutex)
{
	m0_chan_init(&cond->c_chan, mutex);
}
M0_EXPORTED(m0_cond_init);

M0_INTERNAL void m0_cond_fini(struct m0_cond *cond)
{
	m0_chan_fini_lock(&cond->c_chan);
}
M0_EXPORTED(m0_cond_fini);

M0_INTERNAL void m0_cond_wait(struct m0_cond *cond)
{
	struct m0_clink clink;

	/*
	 * First, register the clink with the channel, *then* unlock the
	 * mutex. This guarantees that signals to the condition variable are not
	 * missed, because they are done under the mutex.
	 */

	M0_PRE(m0_chan_is_locked(&cond->c_chan));

	m0_clink_init(&clink, NULL);
	m0_clink_add(&cond->c_chan, &clink);
	m0_chan_unlock(&cond->c_chan);
	m0_chan_wait(&clink);
	m0_chan_lock(&cond->c_chan);
	m0_clink_del(&clink);
	m0_clink_fini(&clink);
}
M0_EXPORTED(m0_cond_wait);

M0_INTERNAL bool m0_cond_timedwait(struct m0_cond *cond,
				   const m0_time_t abs_timeout)
{
	struct m0_clink clink;
	bool            retval;

	M0_PRE(m0_chan_is_locked(&cond->c_chan));

	m0_clink_init(&clink, NULL);
	m0_clink_add(&cond->c_chan, &clink);
	m0_chan_unlock(&cond->c_chan);
	retval = m0_chan_timedwait(&clink, abs_timeout);
	m0_chan_lock(&cond->c_chan);
	m0_clink_del(&clink);
	m0_clink_fini(&clink);

	return retval;
}
M0_EXPORTED(m0_cond_timedwait);

M0_INTERNAL void m0_cond_signal(struct m0_cond *cond)
{
	m0_chan_signal(&cond->c_chan);
}
M0_EXPORTED(m0_cond_signal);

M0_INTERNAL void m0_cond_broadcast(struct m0_cond *cond)
{
	m0_chan_broadcast(&cond->c_chan);
}

/** @} end of cond group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
