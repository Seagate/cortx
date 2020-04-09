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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>,
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 05/01/2010
 */

#pragma once

#ifndef __MERO_LIB_THREAD_H__
#define __MERO_LIB_THREAD_H__

#ifndef __KERNEL__
#  include "lib/user_space/thread.h"
#else
#  include "lib/linux_kernel/thread.h"
#endif
#include "lib/semaphore.h"

#include "lib/list.h"
#include "addb2/counter.h"               /* m0_addb2_sensor */

struct m0_bitmap;
struct m0;
struct m0_addb2_mach;

/**
   @defgroup thread Thread

   Thread creation and manipulation interface (rather spartan at the moment).

   Threads can be created that will start execution by running a specified
   function invoked with a specified argument. Thread functions return void, a
   user must arrange a method of transferring results back from the thread if
   necessary.

   The only operation on a started thread is waiting for its completion
   ("joining" the thread).

   @see m0_thread

   @{
 */

struct m0_thread;

/** Thread-local storage. */
struct m0_thread_tls {
	/** m0 instance this thread belong to. */
	struct m0                 *tls_m0_instance;
	/** Platform specific part of tls. Defined in lib/PLATFORM/thread.h. */
	struct m0_thread_arch_tls  tls_arch;
	struct m0_addb2_mach      *tls_addb2_mach;
	struct m0_thread          *tls_self;
	struct m0_addb2_sensor     tls_clock;
};

/**
   Thread states used to validate thread interface usage.

   @see m0_thread
 */
enum m0_thread_state {
	TS_PARKED = 0,
	TS_RUNNING
};

/**
   Control block for a thread.

   <b>States</b>

   A thread control block can be in one of the following states:

   @li PARKED: thread is not yet started or has been joined already. In this
   state m0_thread::t_func is NULL;

   @li RUNNING: the thread started execution of m0_thread::t_func function, but
   hasn't yet been joined. Note that the thread can be in this state after
   return from m0_thread::t_func.

   <b>Concurrency control</b>

   A user is responsible for serialising access to a control block. For example,
   there should be no concurrent calls to m0_thread_init() or m0_thread_join()
   for the same m0_thread.

   <b>Liveness</b>

   Implementation only accesses control block as part of explicit calls to
   m0_thread interface functions. A user is free to destroy the control block at
   any moment, except for the possible resource leak in the case of running (and
   not yet joined) thread.
 */
struct m0_thread {
	enum m0_thread_state    t_state;
	struct m0_thread_handle t_h;
	int                   (*t_init)(void *);
	void                  (*t_func)(void *);
	void                   *t_arg;
	struct m0_semaphore     t_wait;
	int                     t_initrc;
	struct m0_thread_tls    t_tls;
	char                    t_namebuf[M0_THREAD_NAME_LEN];
};

/**
   Type-safe wrapper around m0_thread_init().

   With this macro one can initialise a thread with a function taking an
   argument of a particular type:

   @code
   static void worker(struct foo *arg) { ... }
   static struct m0_thread tcb;

   result = M0_THREAD_INIT(&tcb, struct foo *, NULL, &worker, arg, "worker");
   @endcode

   M0_THREAD_INIT() checks that type of the argument matches function prototype.

   @note TYPE cannot be void.
 */
#define M0_THREAD_INIT(thread, TYPE, init, func, arg, namefmt, ...)	\
({									\
	typeof(func) __func = (func);					\
	typeof(arg)  __arg  = (arg);					\
	TYPE         __dummy;						\
	(void)(__func == (void (*)(TYPE))NULL);				\
	(void)(&__arg == &__dummy);					\
	m0_thread_init(thread,						\
		       (int  (*)(void *))init,				\
		       (void (*)(void *))__func,			\
		       (void *)(unsigned long)__arg,			\
		       namefmt , ## __VA_ARGS__);			\
})

/**
   Internal helper for m0_thread_init() that creates the user or kernel thread
   after the m0_thread q has been initialised.
   @pre q->t_state == TS_RUNNING
   @retval 0 thread created
   @retval -errno failed
 */
M0_INTERNAL int m0_thread_init_impl(struct m0_thread *q, const char *name);

/**
   Threads created by m0_thread_init_impl execute this function to
   perform common bookkeeping, executing t->t_init if appropriate,
   and then executing t->t_func.
   @pre t->t_state == TS_RUNNING && t->t_initrc == 0
   @param t a m0_thread*, passed as void* to be compatible with
   pthread_create function argument.
   @retval NULL
 */
M0_INTERNAL void *m0_thread_trampoline(void *t);

/**
   Creates a new thread.

   If "init" is not NULL, the created thread starts execution by calling
   (*init)(arg). If this call returns non-zero, thread exits and
   m0_thread_init() returns the value returned by "init".

   Otherwise (or in the case where "init" is NULL), m0_thread_init() returns 0
   and the thread calls (*func)(arg) and exits when this call completes.

   The namefmt and its arguments are used to name the thread.  The formatted
   name is truncated to M0_THREAD_NAME_LEN characters (based on TASK_COMM_LEN).

   @note it is possible that after successful return from m0_thread_init() the
   thread hasn't yet entered "func" code, it is also possible that the thread
   has finished its execution.

   @pre q->t_state == TS_PARKED
   @post (result != 0) == (q->t_state == TS_PARKED)
   @post (result == 0) == (q->t_state == TS_RUNNING)
 */
int m0_thread_init(struct m0_thread *q, int (*init)(void *),
		   void (*func)(void *), void *arg, const char *namefmt, ...)
	__attribute__ ((format (printf, 5, 6)));

/**
   Releases resources associated with the thread.

   @pre q->t_state == TS_PARKED
 */
void m0_thread_fini(struct m0_thread *q);

/**
   Waits until the thread exits.

   After this calls returns successfully it is guaranteed that no code would be
   ever executed by the "q", including instructions that touch stack or code
   pages. Note that the same effect can not be reliably achieved by the explicit
   synchronization (e.g., by signalling a condition variable at the end of a
   thread function), because the thread might be still executing instructions
   after it returns from m0_thread::t_func.

   @pre q->t_state == TS_RUNNING
   @pre q is different from the calling thread
   @post (result == 0) == (q->t_state == TS_PARKED)
   @retval 0 thread joined (thread is terminated)
   @retval -errno failed to join, not exit status of thread
 */
int m0_thread_join(struct m0_thread *q);

/**
   Send specified signal to this thread.
*/
M0_INTERNAL int m0_thread_signal(struct m0_thread *q, int sig);

/**
   Sets thread affinity to a given processor bitmap.

   The user space implementation calls pthread_setaffinity_np and the kernel
   implementation modifies fields of the task_struct directly.

   @see http://www.kernel.org/doc/man-pages/online/pages/man3/pthread_setaffinity_np.3.html
   @see lib/processor.h
   @see kthread

   @param q thread whose affinity is to be set (confined)
   @param processors bitmap of processors, true values are those on which the
   thread can run
   @retval 0 success
   @retval !0 failed to set affinity, -errno
 */
M0_INTERNAL int m0_thread_confine(struct m0_thread *q,
				  const struct m0_bitmap *processors);

/**
 * Returns the current thread.
 */
M0_INTERNAL struct m0_thread *m0_thread_self(void);

/**
 * Returns thread-local storage.
 * @note The returned value is never NULL.
 */
M0_INTERNAL struct m0_thread_tls *m0_thread_tls(void);

/**
 * Initialises thread system.
 *
 * m0_threads_init() is usually called at the early stages of Mero
 * initialisation, i.e., early in m0_init().
 *
 * @param instance  Initial m0 instance.
 */
M0_INTERNAL int m0_threads_init(struct m0 *instance);
M0_INTERNAL void m0_threads_fini(void);

M0_INTERNAL int m0_threads_once_init(void);
M0_INTERNAL void m0_threads_once_fini(void);

/** Sets the thread in awkward context. */
M0_INTERNAL void m0_enter_awkward(void);

/** Resets thread from awkward context. */
M0_INTERNAL void m0_exit_awkward(void);

/** Tells if executing thread is in awkward context. */
M0_INTERNAL bool m0_is_awkward(void);

/**
 * Returns "process identifier", depending on the architecture.
 */
M0_INTERNAL uint64_t m0_pid(void);

/**
 * Returns "process identifier" for userspace process and 0 in the kernel.
 * @see m0_pid().
 */
M0_INTERNAL uint64_t m0_process(void);

/**
 * Accepts a thread as a Mero thread.
 *
 * Mero maintains a per-thread context needed for any Mero operation. A thread
 * with such a context is "Mero thread". A thread that calls m0_init() is a Mero
 * thread. A thread created by a call to m0_thread_init() (necessarily made by a
 * Mero thread) is a Mero thread.
 *
 * To turn any other thread into a Mero thread call m0_thread_adopt() before any
 * Mero call is made in the thread. m0_thread_shun() must be called before the
 * thread exits.
 *
 * "thread" object must be owned by the calling thread (not accessed
 * concurrently by multiple threads). It must exist at least until the matching
 * m0_thread_shun() call returns.
 *
 * "instance" is m0 instance to which the thread belongs.
 */
M0_INTERNAL int m0_thread_adopt(struct m0_thread *thread, struct m0 *instance);

/**
 * Discards Mero per-thread context. This is dual to m0_thread_adopt().
 */
M0_INTERNAL void m0_thread_shun(void);

M0_INTERNAL int m0_thread_arch_adopt(struct m0_thread *thread,
				     struct m0 *instance, bool full);
M0_INTERNAL void m0_thread_arch_shun(void);

/** @} end of thread group */
#endif /* __MERO_LIB_THREAD_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
