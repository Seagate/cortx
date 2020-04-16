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
 * Original creation date: 03/11/2011
 */

#pragma once

#ifndef __MERO_LIB_SEMAPHORE_H__
#define __MERO_LIB_SEMAPHORE_H__

#include "lib/types.h"
#include "lib/time.h"  /* m0_time_t */

/**
   @defgroup semaphore Dijkstra semaphore

   @see http://en.wikipedia.org/wiki/Semaphore_(programming)

   @{
 */

#ifdef __KERNEL__
#  include "lib/linux_kernel/semaphore.h"
#else
#  include "lib/user_space/semaphore.h"
#endif

M0_INTERNAL int m0_semaphore_init(struct m0_semaphore *semaphore,
				  unsigned value);
M0_INTERNAL void m0_semaphore_fini(struct m0_semaphore *semaphore);

/** Downs the semaphore (P-operation). */
M0_INTERNAL void m0_semaphore_down(struct m0_semaphore *semaphore);

/** Ups the semaphore (V-operation). */
M0_INTERNAL void m0_semaphore_up(struct m0_semaphore *semaphore);

/**
   Tries to down a semaphore without blocking.

   Returns true iff the P-operation succeeded without blocking.
 */
M0_INTERNAL bool m0_semaphore_trydown(struct m0_semaphore *semaphore);

/**
 * Brings down the semaphore to 0.
 */
M0_INTERNAL void m0_semaphore_drain(struct m0_semaphore *semaphore);

/**
   Returns the number of times a P-operation could be executed without blocking.

   @note the return value might, generally, be invalid by the time
   m0_semaphore_value() returns.

   @note that the parameter is not const. This is because of POSIX
   sem_getvalue() prototype.
 */
M0_INTERNAL unsigned m0_semaphore_value(struct m0_semaphore *semaphore);

/**
   Downs the semaphore, blocking for not longer than the (absolute) timeout
   given.

   @note this call with cause the thread to wait on semaphore in
         non-interruptable state: signals won't preempt it.
         Use it to wait for events that are expected to arrive in a
         "short time".

   @param abs_timeout absolute time since Epoch (00:00:00, 1 January 1970)
   @return true if P-operation succeed immediately or before timeout;
   @return false otherwise.

 */
M0_INTERNAL bool m0_semaphore_timeddown(struct m0_semaphore *semaphore,
					const m0_time_t abs_timeout);

/** @} end of semaphore group */
#endif /* __MERO_LIB_SEMAPHORE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
