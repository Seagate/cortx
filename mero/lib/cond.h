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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 05/17/2010
 */

#pragma once

#ifndef __MERO_LIB_COND_H__
#define __MERO_LIB_COND_H__

#include "chan.h"

struct m0_mutex;

/**
   @defgroup cond Conditional variable.

   Condition variable is a widely known and convenient synchronization
   mechanism.

   Notionally, a condition variable packages two things: a predicate ("a
   condition", hence the name) on computation state, e.g., "a free buffer is
   available", "an incoming request waits for processing" or "all worker threads
   have finished", and a mutex (m0_mutex) protecting changes to the state
   affecting the predicate.

   There are two parts in using condition variable:

   @li in all places where state is changed in a way that affects the predicate,
   the condition variable associated with the predicate has to be signalled. For
   example:

   @code
   void buffer_put(struct m0_buffer *buf) {
           // buffer is being returned to the pool...
           m0_mutex_lock(&buffer_pool_lock);
           // add the buffer to the free list
           buf_tlist_add(&buffer_pool_free, buf);
           // and signal the condition variable
           m0_cond_signal(&buffer_pool_hasfree);
           m0_mutex_unlock(&buffer_pool_lock);
   }
   @endcode

   @li to wait for predicate to change, one takes the lock, checks the predicate
   and calls m0_cond_wait() until the predicate becomes true:

   @code
   struct m0_buffer *buffer_get(void) {
           struct m0_buffer *buf;

           m0_mutex_lock(&buffer_pool_lock);
           while (buf_tlist_is_empty(&buffer_pool_free))
                   m0_cond_wait(&buffer_pool_hasfree);
           buf = buf_tlist_head(&buffer_pool_free);
           buf_tlist_del(buf);
           m0_mutex_unlock(&buffer_pool_lock);
           return buf;
   }
   @endcode

   Note that one has to re-check the predicate after m0_cond_wait() returns,
   because it might, generally, be false if multiple threads are waiting for
   predicate change (in the above example, if there are multiple concurrent
   calls to buffer_get()). This introduces one of the nicer features of
   condition variables: de-coupling of producers and consumers.

   Condition variables are more reliable and structured synchronization
   primitive than channels (m0_chan), because the lock, protecting the predicate
   is part of the interface and locking state can be checked. On the other hand,
   channels can be used with predicates protected by read-write locks, atomic
   variables, etc.---where condition variables are not applicable.

   @see m0_chan
   @see http://opengroup.org/onlinepubs/007908799/xsh/pthread_cond_wait.html

   @todo Consider supporting other types of locks in addition to m0_mutex.

   @{
*/

struct m0_cond {
	struct m0_chan c_chan;
};

M0_INTERNAL void m0_cond_init(struct m0_cond *cond, struct m0_mutex *mutex);
M0_INTERNAL void m0_cond_fini(struct m0_cond *cond);

/**
   Atomically unlocks the mutex, waits on the condition variable and locks the
   mutex again before returning.

   @pre  m0_mutex_is_locked(mutex)
   @post m0_mutex_is_locked(mutex)
 */
M0_INTERNAL void m0_cond_wait(struct m0_cond *cond);

/**
   This is the same as m0_cond_wait, except that it has a timeout value. If the
   time expires before event is pending, this function will return false.

   @note Unlike pthread_cond_timedwait, m0_cond_timedwait can succeed if the
   event is immediately pending, even if the abs_timeout is in the past.  If
   blocking occurs and abs_timeout is in the past, m0_cond_timedwait will return
   false.  pthread_cond_timedwait always fails when abs_timeout is in the past.

   @param abs_timeout this is the time since Epoch (00:00:00, 1 January 1970).
   @return true if condition is signaled before timeout.
   @return false if condition variable is not signaled but timeout expires.
	   errno is ETIMEDOUT;
 */
M0_INTERNAL bool m0_cond_timedwait(struct m0_cond *cond,
				   const m0_time_t abs_timeout);

/**
   Wakes up no more than one thread waiting on the condition variable.

   @pre m0_mutex_is_locked(mutex)
 */
M0_INTERNAL void m0_cond_signal(struct m0_cond *cond);

/**
   Wakes up all threads waiting on the condition variable.

   @pre m0_mutex_is_locked(mutex)
 */
M0_INTERNAL void m0_cond_broadcast(struct m0_cond *cond);

/** @} end of cond group */

/* __MERO_LIB_COND_H__ */
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
