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
 * Original author: Huang Hua <Hua_Huang@xyratex.com>
 *                  Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 03/04/2011
 */

#include "lib/timer.h"

#include "lib/misc.h"		/* M0_SET0 */
#include "lib/assert.h"		/* M0_PRE */
#include "lib/thread.h"		/* m0_enter_awkward */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @addtogroup timer

   Implementation of m0_timer.

 */

M0_INTERNAL int m0_timer_init(struct m0_timer	       *timer,
			      enum m0_timer_type	type,
			      struct m0_timer_locality *loc,
			      m0_timer_callback_t	callback,
			      unsigned long		data)
{
	int rc;

	M0_PRE(callback != NULL);
	M0_PRE(M0_IN(type, (M0_TIMER_SOFT, M0_TIMER_HARD)));

	M0_SET0(timer);
	timer->t_type     = type;
	timer->t_expire	  = 0;
	timer->t_callback = callback;
	timer->t_data     = data;

	rc = m0_timer_ops[timer->t_type].tmr_init(timer, loc);

	if (rc == 0)
		timer->t_state = M0_TIMER_INITED;

	M0_LEAVE("%p, rc=%d", timer, rc);
	return rc;
}

M0_INTERNAL void m0_timer_fini(struct m0_timer *timer)
{
	M0_ENTRY("%p", timer);
	M0_PRE(M0_IN(timer->t_state, (M0_TIMER_STOPPED, M0_TIMER_INITED)));

	m0_timer_ops[timer->t_type].tmr_fini(timer);
	timer->t_state = M0_TIMER_UNINIT;
	M0_LEAVE("%p", timer);
}

M0_INTERNAL void m0_timer_start(struct m0_timer *timer,
				m0_time_t	 expire)
{
	M0_PRE(M0_IN(timer->t_state, (M0_TIMER_STOPPED, M0_TIMER_INITED)));

	timer->t_expire = expire;

	timer->t_state = M0_TIMER_RUNNING;
	m0_timer_ops[timer->t_type].tmr_start(timer);
}

M0_INTERNAL void m0_timer_stop(struct m0_timer *timer)
{
	M0_PRE(timer->t_state == M0_TIMER_RUNNING);

	m0_timer_ops[timer->t_type].tmr_stop(timer);
	timer->t_state = M0_TIMER_STOPPED;
}

M0_INTERNAL bool m0_timer_is_started(const struct m0_timer *timer)
{
	return timer->t_state == M0_TIMER_RUNNING;
}

M0_INTERNAL void m0_timer_callback_execute(struct m0_timer *timer)
{
	m0_enter_awkward();
	timer->t_callback(timer->t_data);
	m0_exit_awkward();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of timer group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
