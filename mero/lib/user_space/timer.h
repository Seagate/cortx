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
 * Original author: Huang Hua <Hua_Huang@xyratex.com>
 *		    Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 03/04/2011
 */

#pragma once

#ifndef __MERO_LIB_USER_SPACE_TIMER_H__
#define __MERO_LIB_USER_SPACE_TIMER_H__

#include "lib/time.h"      /* m0_time_t */
#include "lib/thread.h"    /* m0_thread */
#include "lib/semaphore.h" /* m0_semaphore */

/**
   @addtogroup timer

   <b>User space timer.</b>
   @{
 */

struct m0_timer {
	/** Timer type: M0_TIMER_SOFT or M0_TIMER_HARD. */
	enum m0_timer_type  t_type;
	/** Timer triggers this callback. */
	m0_timer_callback_t t_callback;
	/** User data. It is passed to m0_timer::t_callback(). */
	unsigned long	    t_data;
	/** Expire time in future of this timer. */
	m0_time_t	    t_expire;
	/** Timer state.  Used in state changes checking. */
	enum m0_timer_state t_state;

	/** Semaphore for m0_timer_stop() and user callback synchronisation. */
	struct m0_semaphore t_cb_sync_sem;

	/** Soft timer working thread. */
	struct m0_thread    t_thread;
	/** Soft timer working thread sleeping semaphore. */
	struct m0_semaphore t_sleep_sem;
	/** Thread is stopped by m0_timer_fini(). */
	bool		    t_thread_stop;

	/** POSIX timer ID, returned by timer_create(). */
	timer_t		    t_ptimer;
	/**
	   Target thread ID for hard timer callback.
	   If it is 0 then signal will be sent to the process
	   but not to any specific thread.
	 */
	pid_t		    t_tid;

};

/**
   Item of threads ID list in locality.
   Used in the implementation of userspace hard timer.
 */
struct m0_timer_tid {
	pid_t		tt_tid;
	struct m0_tlink tt_linkage;
	uint64_t	tt_magic;
};

/**
   Timer locality.

   The signal for M0_TIMER_HARD timers will be delivered to a thread
   from the locality.
 */
struct m0_timer_locality {
	/** Lock for tlo_tids */
	struct m0_mutex tlo_lock;
	/** List of thread ID's, associated with this locality */
	struct m0_tl tlo_tids;
	/** ThreadID of next thread for round-robin timer thread selection */
	struct m0_timer_tid *tlo_rrtid;
};

M0_EXTERN const struct m0_timer_operations m0_timer_ops[];

M0_INTERNAL int m0_timers_init(void);
M0_INTERNAL void m0_timers_fini(void);

/** @} end of timer group */

/* __MERO_LIB_USER_SPACE_TIMER_H__ */
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
