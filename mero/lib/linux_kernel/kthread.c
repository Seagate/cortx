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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 02/24/2011
 */

#include <linux/version.h>    /* LINUX_VERSION_CODE */

#include "lib/thread.h"
#include "lib/misc.h"         /* M0_IS0 */
#include "lib/arith.h"        /* min64u */
#include "lib/bitmap.h"       /* m0_bitmap_get */
#include "module/instance.h"  /* m0_set */
#include "addb2/global.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @addtogroup kthread Kernel Thread Implementation
   @ingroup thread

   Implementation of m0_thread on top of struct task_struct and kthread API.

   Instead of creating a new kthread executing user-supplied function, all
   threads start executing the same trampoline function kthread_trampoline()
   that performs some generic book-keeping.

   The kernel m0_thread_confine() implementation comes with strict usage
   restrictions.  It manipulates fields of the task_struct directly rather than
   using set_cpus_allowed_ptr(), and it does not protect against concurrent task
   termination.  set_cpus_allowed_ptr() is not used because of GPL restrictions.
   The task is not protected from termination because that requires the use of
   get_task_struct() and put_task_struct(), however put_task_struct() is inlined
   and references __put_task_struct() and the latter is not exported.  An easy
   way to ensure the task will not terminate is to call m0_thread_confine() from
   the task to be confined.

   @note Unless the thread being confined by m0_thread_confine() is the current
   thread, the thread will not migrate to a CPU in the bitmap until the next
   time it next blocks (unschedules) and subsequently resumes.  Task migration
   is not an exported function of kernel/sched.c.  When the current thread is
   confined, m0_thread_confine() causes the task to block and resume, so this
   function must be used in a context where scheduling is allowed.

   @{
 */

static struct m0 *__instance;

M0_INTERNAL struct m0_thread_tls *m0_thread_tls(void)
{
	if (current->journal_info == NULL) {
		M0_LOG(M0_FATAL, "No tls.");
		dump_stack();
	}
	M0_ASSERT(current->journal_info != NULL);
	return current->journal_info;
}

M0_INTERNAL int m0_threads_init(struct m0 *instance)
{
	M0_PRE(__instance == NULL);
	__instance = instance;
	m0_set(instance);
	return 0;
}

M0_INTERNAL void m0_threads_fini(void)
{
}

M0_INTERNAL int m0_threads_once_init(void)
{
	return 0;
}

M0_INTERNAL void m0_threads_once_fini(void)
{
}

M0_INTERNAL void m0_thread_enter(struct m0_thread *thread, bool full)
{
	m0_thread_arch_adopt(thread, __instance, full);
}
M0_EXPORTED(m0_thread_enter);

static void tls_fini(struct m0_thread_tls *tls)
{
}

M0_INTERNAL void m0_thread_leave(void)
{
	m0_thread_arch_shun();
}
M0_EXPORTED(m0_thread_leave);

M0_INTERNAL void m0_thread__cleanup(struct m0_thread *bye)
{
	m0_thread_leave();
}
M0_EXPORTED(m0_thread__cleanup);

static int kthread_trampoline(void *arg)
{
	struct m0_thread *t = arg;

	current->journal_info = &t->t_tls;
	/* Required for correct m0_thread_join() behavior in kernel:
	   kthread_stop() will not stop if the thread has been created but has
	   not yet started executing.  So, m0_thread_join() blocks on the
	   semaphore to ensure the thread can be stopped iff t_init != NULL
	   (when t_init == NULL, blocking occurs in m0_thread_init).
	   kthread_stop(), in turn, requires that the thread not exit until
	   kthread_stop() is called, so we must loop on kthread_should_stop()
	   to satisfy that API requirement.  The semaphore is signalled before
	   calling the thread function so that other kernel code can still
	   depend on kthread_should_stop().
	 */
	if (t->t_init == NULL)
		m0_semaphore_up(&t->t_wait);
	m0_thread_trampoline(arg);
	current->journal_info = NULL;
	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);

	return 0;
}

M0_INTERNAL int m0_thread_init_impl(struct m0_thread *q, const char *name)
{
	int result;
	struct task_struct *task;

	M0_PRE(q->t_state == TS_RUNNING);

	task = kthread_create(kthread_trampoline, q, "%s", name);
	if (IS_ERR(task)) {
		result = PTR_ERR(q->t_h.h_tsk);
	} else {
		result = 0;
		q->t_h.h_tsk = task;
		q->t_h.h_pid = task->pid;
		wake_up_process(task);
	}
	return result;
}

int m0_thread_join(struct m0_thread *q)
{
	int result;

	M0_PRE(q->t_state == TS_RUNNING);
	M0_PRE(q->t_h.h_tsk != current);

	/* see comment in kthread_trampoline */
	if (q->t_init == NULL)
		m0_semaphore_down(&q->t_wait);
	/*
	  m0_thread provides no wrappers for do_exit(), so this will block
	  until the thread exits by returning from kthread_trampoline.
	  kthread_trampoline() always returns 0, but kthread_stop() can return
	  -errno on failure.
	 */
	result = kthread_stop(q->t_h.h_tsk);
	if (result == 0)
		q->t_state = TS_PARKED;
	return result;
}
M0_EXPORTED(m0_thread_join);

M0_INTERNAL int m0_thread_signal(struct m0_thread *q, int sig)
{
	return M0_ERR(-ENOSYS);
}

M0_INTERNAL int m0_thread_confine(struct m0_thread *q,
				  const struct m0_bitmap *processors)
{
	int                 result = 0;
	size_t              idx;
	size_t              nr_bits = min64u(processors->b_nr, nr_cpu_ids);
	cpumask_var_t       cpuset;
	struct task_struct *p = q->t_h.h_tsk;
	int                 nr_allowed;

	if (!zalloc_cpumask_var(&cpuset, GFP_KERNEL))
		return M0_ERR(-ENOMEM);

	for (idx = 0; idx < nr_bits; ++idx)
		if (m0_bitmap_get(processors, idx))
			cpumask_set_cpu(idx, cpuset);
	nr_allowed = cpumask_weight(cpuset);

	if (nr_allowed == 0) {
		result = M0_ERR(-EINVAL);
	} else {
		/*
		  The following code would safely access the task_struct and
		  ensure it would not disappear, however put_task_struct is an
		  inline that references __put_task_struct, and the latter is
		  not exported.  See the notes at the top of this file.

		  get_task_struct(p);

		  ...

		  put_task_struct(p);
		*/

		cpumask_copy(&p->cpus_allowed, cpuset);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
		p->nr_cpus_allowed = nr_allowed;
#else
		p->rt.nr_cpus_allowed = nr_allowed;
#endif

		/* cause current task to migrate immediately by blocking */
		if (p == current && !cpumask_test_cpu(task_cpu(p), cpuset))
			schedule_timeout_uninterruptible(1);
	}
	free_cpumask_var(cpuset);
	return result;
}

M0_INTERNAL void m0_enter_awkward(void)
{
	/*
	 * To avoid kernel warnings from del_timer_sync() when
	 * canceling m0_sm_timer at m0_sm_state_set() -> clink_signal()
	 * we disable the effect of this function in kernel mode.
	 * Luckily, m0_is_awkward() is used in kernel in its UT only
	 * at the moment, so we disabled it there also.
	 *
	 * add_preempt_count(HARDIRQ_OFFSET);
	 */
}

M0_INTERNAL void m0_exit_awkward(void)
{
	/*
	 * sub_preempt_count(HARDIRQ_OFFSET);
	 */
}

M0_INTERNAL bool m0_is_awkward(void)
{
	return in_irq();
}

M0_INTERNAL struct m0_thread_tls *m0_thread_tls_pop(void)
{
	struct m0_thread_tls *tls = current->journal_info;

	current->journal_info = tls->tls_arch.tat_prev;
	return tls;
}

M0_INTERNAL void m0_thread_tls_back(struct m0_thread_tls *tls)
{
	M0_PRE(__instance != NULL);
	M0_PRE(tls->tls_arch.tat_prev == current->journal_info);

	current->journal_info = tls;
}

M0_INTERNAL uint64_t m0_pid(void)
{
	return current->pid;
}

M0_INTERNAL uint64_t m0_process(void)
{
	return 0;
}

M0_INTERNAL int m0_thread_arch_adopt(struct m0_thread *thread,
				     struct m0 *instance, bool full)
{
	M0_PRE(M0_IS0(thread));
	M0_PRE(instance == __instance);

	thread->t_tls.tls_arch.tat_prev = current->journal_info;
	thread->t_tls.tls_self = thread;
	thread->t_h.h_tsk = current;
	thread->t_h.h_pid = current->pid;
	current->journal_info = &thread->t_tls;
	if (__instance != NULL) {
		m0_set(__instance);
		if (full)
			m0_addb2_global_thread_enter();
	}
	return 0;
}
M0_EXPORTED(m0_thread_arch_adopt);

M0_INTERNAL void m0_thread_arch_shun(void)
{
	struct m0_thread_tls *tls  = current->journal_info;
	void                 *prev = tls->tls_arch.tat_prev;

	tls_fini(tls);
	m0_addb2_global_thread_leave();
	current->journal_info = prev;
}
M0_EXPORTED(m0_thread_arch_shun);

#undef M0_TRACE_SUBSYSTEM

/** @} end of thread group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
