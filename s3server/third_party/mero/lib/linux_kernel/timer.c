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
 * Original author: Huang Hua <hua_huang@xyratex.com>
 *                  Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 03/04/2011
 */

#include "lib/timer.h"
#include "lib/thread.h"         /* M0_THREAD_ENTER */

#include <linux/jiffies.h>	/* timespec_to_jiffies */
#include <linux/version.h>
/**
   @addtogroup timer

   <b>Implementation of m0_timer on top of Linux struct timer_list.</b>

   @{
*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static void timer_kernel_trampoline_callback(struct timer_list *tl)
#else
static void timer_kernel_trampoline_callback(unsigned long tl)
#endif
{

	struct m0_timer *timer = container_of((struct timer_list *)tl,
					       struct m0_timer, t_timer);
	struct m0_thread th    = { 0, };
	m0_thread_enter(&th, false);
	m0_timer_callback_execute(timer);
	m0_thread_leave();
}

static int timer_kernel_init(struct m0_timer	      *timer,
			     struct m0_timer_locality *loc)
{
	struct timer_list *tl = &timer->t_timer;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	timer_setup(tl, timer_kernel_trampoline_callback, tl->flags);
#else
	init_timer(tl);
	tl->data     = (unsigned long)tl;
	tl->function = timer_kernel_trampoline_callback;
#endif
	return 0;
}

static void timer_kernel_fini(struct m0_timer *timer)
{
}

static void timer_kernel_start(struct m0_timer *timer)
{
	struct timespec ts;
	m0_time_t	expire = timer->t_expire;
	m0_time_t       now    = m0_time_now();

	expire = expire > now ? m0_time_sub(expire, now) : 0;
	ts.tv_sec  = m0_time_seconds(expire);
	ts.tv_nsec = m0_time_nanoseconds(expire);
	timer->t_timer.expires = jiffies + timespec_to_jiffies(&ts);

	add_timer(&timer->t_timer);
}

static void timer_kernel_stop(struct m0_timer *timer)
{
	/*
	 * This function returns whether it has deactivated
	 * a pending timer or not. It always successful.
	 */
	del_timer_sync(&timer->t_timer);
}

M0_INTERNAL const struct m0_timer_operations m0_timer_ops[] = {
	[M0_TIMER_SOFT] = {
		.tmr_init  = timer_kernel_init,
		.tmr_fini  = timer_kernel_fini,
		.tmr_start = timer_kernel_start,
		.tmr_stop  = timer_kernel_stop,
	},
	[M0_TIMER_HARD] = {
		.tmr_init  = timer_kernel_init,
		.tmr_fini  = timer_kernel_fini,
		.tmr_start = timer_kernel_start,
		.tmr_stop  = timer_kernel_stop,
	},
};

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
