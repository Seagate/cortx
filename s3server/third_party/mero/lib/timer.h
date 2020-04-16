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

#pragma once

#ifndef __MERO_LIB_TIMER_H__
#define __MERO_LIB_TIMER_H__

#include "lib/types.h"
#include "lib/tlist.h"	   /* m0_tl */
#include "lib/mutex.h"	   /* m0_mutex */

/**
   @defgroup timer Generic timer manipulation

   Any timer should call m0_timer_init() function before any function. That
   init function does all initialization for this timer. After that, the
   m0_timer_start() function is called to start the timer. The timer callback
   function will be called repeatedly, if this is a repeatable timer. Function
   m0_timer_stop() is used to stop the timer, and m0_timer_fini() to destroy
   the timer after usage.

   User supplied callback function should be small, run and complete quickly.

   There are two types of timer: soft timer and hard timer. For Linux kernel
   implementation, all timers are hard timer. For userspace implementation,
   soft timer and hard timer have different mechanism:

   - Hard timer has better resolution and is driven by signal. The
     user-defined callback should take short time, should never block
     at any time. Also in user space it should be async-signal-safe
     (see signal(7)), in kernel space it can only take _irq spin-locks.
   - Soft timer creates separate thread to execute the user-defined
     callback for each timer. So the overhead is bigger than hard timer.
     The user-defined callback execution may take longer time and it will
     not impact other timers.

   State machine
   @verbatim

	      +------------+     m0_timer_start()     +-------------+
	      |   INITED   |------------------------->|   RUNNING   |
	      +------------+                          +-------------+
		   ^  |                                    |  ^
   m0_timer_init() |  | m0_timer_fini()    m0_timer_stop() |  | m0_timer_start()
		   |  v                                    v  |
	      +============+                          +-------------+
	      |   UNINIT   |<-------------------------|   STOPPED   |
	      +============+      m0_timer_fini()     +-------------+

   @endverbatim

   Error handling
   - m0_timer_init() can fail;
   - m0_timer_fini() can't fail;
   - m0_timer_start(), m0_timer_stop() is guaranteed to succeed after
     successful m0_timer_init() call.

   Implementation details
   - POSIX timer is used in hard timer implementation;
   - POSIX timer is created in m0_timer_init() and is deleted in
     m0_timer_fini();
   - m0_thread is used in soft timer implementation;
   - m0_thread is started in m0_timer_init() and is finalised in
     m0_timer_fini();
   - add_timer() is used in kernel timer implementation.

   Limitations
   - Number of hard timers is limited by maximum number of pending signals,
     see `ulimit -i`.
   - Number of soft timers is limited by maximum number of threads.

   @note m0_timer_* functions should not be used in the timer callbacks.
   @{
 */

typedef	unsigned long (*m0_timer_callback_t)(unsigned long data);
struct m0_timer;
struct m0_timer_locality;

/**
   Timer type.
 */
enum m0_timer_type {
	M0_TIMER_SOFT,
	M0_TIMER_HARD,
	M0_TIMER_TYPE_NR,
};

/**
   Timer state.
   @see timer_state_change()
 */
enum m0_timer_state {
	/** Not initialized. */
	M0_TIMER_UNINIT = 0,
	/** Initialized. */
	M0_TIMER_INITED,
	/** Timer is running. */
	M0_TIMER_RUNNING,
	/** Timer is stopped */
	M0_TIMER_STOPPED,
	/** Number of timer states */
	M0_TIMER_STATE_NR,
};

struct m0_timer_operations {
	int (*tmr_init)(struct m0_timer *timer, struct m0_timer_locality *loc);
	void (*tmr_fini)(struct m0_timer *timer);
	void (*tmr_start)(struct m0_timer *timer);
	void (*tmr_stop)(struct m0_timer *timer);
};

#ifndef __KERNEL__
#include "lib/user_space/timer.h"
#else
#include "lib/linux_kernel/timer.h"
#endif

/**
   Init the timer data structure.

   @param timer m0_timer structure
   @param type timer type (M0_TIMER_SOFT or M0_TIMER_HARD)
   @param loc timer locality, ignored for M0_TIMER_SOFT timers.
	  Can be NULL - in this case hard timer signal will be delivered
	  to the process. This parameter is ignored in kernel implementation.
   @param callback this callback will be triggered when timer alarms.
   @param data data for the callback.
   @pre callback != NULL
   @pre loc have at least one thread attached
   @post timer is not running
   @see m0_timer_locality
 */
M0_INTERNAL int m0_timer_init(struct m0_timer	       *timer,
			      enum m0_timer_type	type,
			      struct m0_timer_locality *loc,
			      m0_timer_callback_t	callback,
			      unsigned long		data);

/**
   Start a timer.

   @param expire absolute expiration time for timer. If this time is already
	  passed, then the timer callback will be executed ASAP.
   @pre m0_timer_init() successfully called.
   @pre timer is not running
 */
M0_INTERNAL void m0_timer_start(struct m0_timer *timer,
				m0_time_t	 expire);

/**
   Stop a timer.

   @pre m0_timer_init() successfully called.
   @pre timer is running
   @post timer is not running
   @post callback isn't running
 */
M0_INTERNAL void m0_timer_stop(struct m0_timer *timer);

/**
   Returns true iff the timer is running.
 */
M0_INTERNAL bool m0_timer_is_started(const struct m0_timer *timer);

/**
   Destroy the timer.

   @pre m0_timer_init() for this timer was successfully called.
   @pre timer is not running.
 */
M0_INTERNAL void m0_timer_fini(struct m0_timer *timer);

/**
   Execute timer callback.

   It is used in timer implementation.
 */
M0_INTERNAL void m0_timer_callback_execute(struct m0_timer *timer);

/**
   Init timer locality.
   @post timer locality is empty
 */
M0_INTERNAL void m0_timer_locality_init(struct m0_timer_locality *loc);

/**
   Fini timer locality.

   @pre m0_timer_locality_init() successfully called.
   @pre timer locality is empty
 */
M0_INTERNAL void m0_timer_locality_fini(struct m0_timer_locality *loc);

/**
   Add current thread to the list of threads in locality.

   @pre m0_timer_locality_init() successfully called.
   @pre current thread is not attached to locality.
   @post current thread is attached to locality.
 */
M0_INTERNAL int m0_timer_thread_attach(struct m0_timer_locality *loc);

/**
   Remove current thread from the list of threads in locality.
   Current thread must be in this list.

   @pre m0_timer_locality_init() successfully called.
   @pre current thread is attached to locality.
   @post current thread is not attached to locality.
 */
M0_INTERNAL void m0_timer_thread_detach(struct m0_timer_locality *loc);

/** @} end of timer group */
/* __MERO_LIB_TIMER_H__ */
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
