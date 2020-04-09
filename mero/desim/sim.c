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
 */
/*
 * Copyright 2009 ClusterStor.
 *
 * Nikita Danilov.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <signal.h>     /* MINSIGSTKSZ */
#include <sysexits.h>

#include <execinfo.h>

#include "mero/magic.h"
#include "lib/assert.h"
#include "desim/sim.h"

/**
   @addtogroup desim desim
   @{
 */

M0_TL_DESCR_DEFINE(ca, "call-outs", static, struct sim_callout,
		   sc_linkage, sc_magic, M0_DESIM_SIM_CALLOUT_MAGIC,
		   M0_DESIM_SIM_CALLOUT_HEAD_MAGIC);
M0_TL_DEFINE(ca, static, struct sim_callout);

M0_TL_DESCR_DEFINE(sim_thr, "threads", static, struct sim_thread,
		   st_block, st_magic, M0_DESIM_SIM_THREAD_MAGIC,
		   M0_DESIM_SIM_THREAD_HEAD_MAGIC);
M0_TL_DEFINE(sim_thr, static, struct sim_thread);

int vasprintf(char **strp, const char *fmt, va_list ap);

#if 0
static int workload_debug(struct sim_callout *call)
{
	long n = (long)call->sc_datum;

	sim_log(call->sc_sim,
		SLL_DEBUG, "here: %p %llu\n", call, call->sc_sim->ss_bolt);
	if (n < 1000000) {
		sim_timer_rearm(call, 1, workload_debug, (void *)(n + 1));
		return 0;
	} else
		return 1;
}
#endif

/**
 * Currently executing thread in the thread mode of execution (see struct
 * sim_thread).
 */
static struct sim_thread *sim_current = NULL;

/**
 * Scheduler context.
 *
 * Here the context of a main simulation loop is stored before switching in the
 * thread mode (see struct sim_thread). This context is restored to switch to
 * the scheduler mode.
 */
static ucontext_t sim_idle_ctx;

/**
 * Wrapper around malloc(3), aborting the simulation when allocation fails.
 */
M0_INTERNAL void *sim_alloc(size_t size)
{
	void *area;

	area = malloc(size);
	M0_ASSERT(area != NULL);
	memset(area, 0, size);
	return area;
}

/**
 * Wrapper around free(3), dual to sim_alloc().
 */
M0_INTERNAL void sim_free(void *ptr)
{
	free(ptr);
}

/**
 * Initialize simulator state (struct sim).
 */
M0_INTERNAL void sim_init(struct sim *state)
{
	state->ss_bolt = 0;
	ca_tlist_init(&state->ss_future);
	getcontext(&sim_idle_ctx);
}

/**
 * Finalize simulator state.
 */
M0_INTERNAL void sim_fini(struct sim *state)
{
	ca_tlist_fini(&state->ss_future);
}

/**
 * Execute the simulation.
 *
 * This function runs main simulation loop, taking callouts from simulator
 * logical time queue and executing them. Callouts add other callouts to the
 * queue (ordered by the logical time). The loop exits when the queue becomes
 * empty.
 */
M0_INTERNAL void sim_run(struct sim *state)
{
	struct sim_callout *call;

	while (!ca_tlist_is_empty(&state->ss_future)) {
		M0_ASSERT(sim_current == NULL);
		call = ca_tlist_pop(&state->ss_future);
		M0_ASSERT(call->sc_time >= state->ss_bolt);
		/* jump to the future */
		state->ss_bolt = call->sc_time;
		if (call->sc_call(call))
			/*
			 * timer wasn't rearmed.
			 */
			sim_free(call);
	}
}

/**
 * Insert a callout in sorted events list.
 */
static void sim_call_place(struct sim *sim, struct sim_callout *call)
{
	struct sim_callout *found;

	/*
	 * This is the most time consuming function for long simulations. To
	 * speed it up, sim::ss_future list can be replaced with a more advanced
	 * data structure, like a tree or a skip-list of some sort.
	 */

	found = m0_tl_find(ca, scan, &sim->ss_future,
			   scan->sc_time > call->sc_time);

	if (found != NULL)
		ca_tlist_add_before(found, call);
	else
		ca_tlist_add_tail(&sim->ss_future, call);
}

/**
 * Initialize callout
 */
static void sim_timer_init(struct sim *state, struct sim_callout *call,
			   sim_time_t delta, sim_call_t *cfunc, void *datum)
{
	call->sc_time  = state->ss_bolt + delta;
	M0_ASSERT(call->sc_time >= state->ss_bolt); /* overflow */
	call->sc_call  = cfunc;
	call->sc_datum = datum;
	call->sc_sim   = state;
	ca_tlink_init(call);
	sim_call_place(state, call);
}

/**
 * Allocate and initialize call-out. Allocated call-out will be executed after
 * delta units of simulation logical time by calling cfunc call-back. datum in
 * installed into sim_callout::sc_datum field of a newly allocated call-out.
 */
M0_INTERNAL void sim_timer_add(struct sim *state, sim_time_t delta,
			       sim_call_t * cfunc, void *datum)
{
	struct sim_callout *call;

	call = sim_alloc(sizeof *call);
	sim_timer_init(state, call, delta, cfunc, datum);
}

/**
 * Re-arm already allocated call-out to be executed (possibly again) after delta
 * units of logical time. The call-out must be not in the logical time queue.
 */
M0_INTERNAL void sim_timer_rearm(struct sim_callout *call, sim_time_t delta,
				 sim_call_t * cfunc, void *datum)
{
	sim_timer_init(call->sc_sim, call, delta, cfunc, datum);
}

/**
 * Mac OS X getmcontext() (Libc-498/i386/gen/) resets uc_stack.ss_size
 * incorrectly.
 */
static void sim_thread_fix(struct sim_thread *thread)
{
	thread->st_ctx.uc_stack.ss_size = thread->st_size;
}

/**
 * Resume a thread by jumping onto its context.
 */
static void sim_thread_resume(struct sim_thread *thread)
{
	int rc;

	M0_ASSERT(sim_current == NULL);
	sim_current = thread;
	rc = swapcontext(&sim_idle_ctx, &thread->st_ctx);
	if (rc != 0)
		err(EX_UNAVAILABLE, "resume: swapcontext");
}

/**
 * Stash current state of the read (registers) into sim_thread::st_ctx and jump
 * onto main simulation loop context (sim_idle_ctx).
 */
static void sim_thread_suspend(struct sim_thread *thread)
{
	int rc;

	M0_ASSERT(sim_current == thread);
	sim_current = NULL;
	rc = swapcontext(&thread->st_ctx, &sim_idle_ctx);
	if (rc != 0)
		err(EX_UNAVAILABLE, "suspend: swapcontext");
	sim_thread_fix(thread);
}

/*
 * The sim{en,de}code*() and sim_trampoline() functions below are for
 * portability: makecontext(3) creates a context to execute a supplied function
 * with a given number of integer (int) arguments. To pass non-integer
 * parameters (pointers), they have to be encoded as integers.
 *
 * Very simple encoding scheme is used, where each pointer is encoded as a
 * couple of integers.
 *
 * Note, that this is not an idle experiment in obfuscation: in some
 * configurations alignments of integer and pointer types are different.
 */

static void *sim_decode(int p0, int p1)
{
	return (void *)((((uint64_t)p0) << 32) | (((uint64_t)p1) & 0xffffffff));
}

static int sim_encode0(void *p)
{
	return ((uint64_t)p) >> 32;
}

static int sim_encode1(void *p)
{
	return ((uint64_t)p) & 0xffffffff;
}

static void sim_trampoline(int func0, int func1,
			   int state0, int state1, int thread0, int thread1,
			   int datum0, int datum1)
{
	sim_func_t *func;

	func = sim_decode(func0, func1);
	func(sim_decode(state0, state1), sim_decode(thread0, thread1),
	     sim_decode(datum0, datum1));
}

/**
 * Initialize and start a new simulation thread that will be running a function
 * func with an argument arg.
 *
 * The newly initialized thread is immediately switched to. This function can be
 * called only in the scheduler mode.
 */
M0_INTERNAL void sim_thread_init(struct sim *state, struct sim_thread *thread,
				 unsigned stacksize, sim_func_t func, void *arg)
{
	int rc;

	if (stacksize == 0)
		/*
		 * In the kernel land people coded distrbuted file system
		 * servers with a 4K stack.
		 *
		 * Unfortunately, in user land, glibc vfprintf alone eats 1.3KB
		 * of stack space.
		 */
		stacksize = 4 * 1024 * sizeof(int);
	if (stacksize < MINSIGSTKSZ)
		stacksize = MINSIGSTKSZ;

	rc = getcontext(&thread->st_ctx);
	if (rc != 0)
		err(EX_UNAVAILABLE, "getcontext");
	thread->st_sim                   = state;
	thread->st_ctx.uc_link           = &sim_idle_ctx;
	thread->st_ctx.uc_stack.ss_sp    = thread->st_stack = valloc(stacksize);
	thread->st_ctx.uc_stack.ss_size  = thread->st_size = stacksize;
	thread->st_ctx.uc_stack.ss_flags = 0;
	sim_thr_tlink_init(thread);
	if (thread->st_stack == NULL)
		err(EX_TEMPFAIL, "malloc(%d) of a stack", stacksize);
	makecontext(&thread->st_ctx, (void (*)())sim_trampoline,
		    8,
		    sim_encode0(func),   sim_encode1(func),
		    sim_encode0(state),  sim_encode1(state),
		    sim_encode0(thread), sim_encode1(thread),
		    sim_encode0(arg),    sim_encode1(arg));
	sim_thread_resume(thread);
}

/**
 * Finalize thread, releasing its resources (stack).
 */
M0_INTERNAL void sim_thread_fini(struct sim_thread *thread)
{
	M0_PRE(sim_thread_current() != thread);
	M0_PRE(!ca_tlink_is_in(&thread->st_wake));
	sim_thr_tlink_fini(thread);
	if (thread->st_stack != NULL)
		sim_free(thread->st_stack);
}

/**
 * Exit thread execution. Simulation threads must call this before returning
 * from their top-level function.
 */
M0_INTERNAL void sim_thread_exit(struct sim_thread *thread)
{
	sim_thread_suspend(thread);
}

/**
 * Wake-up a thread.
 */
static int sim_wakeup(struct sim_callout *call)
{
	sim_thread_resume(call->sc_datum);
	return 0;
}

/**
 * Schedule a thread wake-up after a given amount of logical time.
 */
static void sim_wakeup_post(struct sim *sim,
			    struct sim_thread *thread, sim_time_t nap)
{
	sim_timer_init(sim, &thread->st_wake, nap, sim_wakeup, thread);
}

/**
 * Delay thread execution for a given amount of logical time.
 */
M0_INTERNAL void sim_sleep(struct sim_thread *thread, sim_time_t nap)
{
	M0_ASSERT(sim_current == thread);
	sim_wakeup_post(thread->st_sim, thread, nap);
	sim_thread_suspend(thread);
}

/**
 * Initialize synchronization channel. If format is not-NULL it (after
 * sprintf-ing remaining arguments into it) will be used as a name of
 * statistical counter embedded into the channel. This name is used in the final
 * statistics dump after simulation completes.
 */
M0_INTERNAL void sim_chan_init(struct sim_chan *chan, char *format, ...)
{
	sim_thr_tlist_init(&chan->ch_threads);
	cnt_init(&chan->ch_cnt_sleep, NULL, "chan#%p", chan);
	if (format != NULL) {
		va_list varg;
		va_start(varg, format);
		sim_name_vaset(&chan->ch_cnt_sleep.c_name, format, varg);
		va_end(varg);
	}
}

/**
 * Finalize the channel, releasing its resources.
 */
M0_INTERNAL void sim_chan_fini(struct sim_chan *chan)
{
	sim_thr_tlist_fini(&chan->ch_threads);
	cnt_fini(&chan->ch_cnt_sleep);
}

/**
 * Wait on a channel. This call suspends calling sim_thread until it is woken-up
 * by a sim_chan_{signal,broadcast}() call. The calling thread is added to the
 * tail of the list, so that sim_chan_signal() wakes threads up in FIFO order.
 */
M0_INTERNAL void sim_chan_wait(struct sim_chan *chan, struct sim_thread *thread)
{
	M0_ASSERT(sim_current == thread);
	sim_thr_tlist_add_tail(&chan->ch_threads, thread);
	/*
	 * The simplest way to measure the time threads are blocked on a channel
	 * is to modify sim_chan::ch_cmt_sleep in this function, right after
	 * return from sim_thread_suspend(). Unfortunately, chan might be
	 * deallocated by that time.
	 *
	 * Instead, the time is stored in sim_thread::st_blocked, and
	 * sim_chan::ch_cnt_sleep is updated by sim_chan_wake_head().
	 */
	thread->st_blocked = thread->st_sim->ss_bolt;
	sim_thread_suspend(thread);
	/*
	 * No access to chan after this point.
	 */
}

/**
 * A helper function, waking up a thread at head of the sim_chan::ch_threads
 * list.
 */
static void sim_chan_wake_head(struct sim_chan *chan)
{
	struct sim_thread *t;

	t = sim_thr_tlist_pop(&chan->ch_threads);
	cnt_mod(&chan->ch_cnt_sleep, t->st_sim->ss_bolt - t->st_blocked);
	sim_wakeup_post(t->st_sim, t, 0);
}

/**
 * Wake-up a thread at head of the channel list of waiting threads.
 */
M0_INTERNAL void sim_chan_signal(struct sim_chan *chan)
{
	if (!sim_thr_tlist_is_empty(&chan->ch_threads))
		sim_chan_wake_head(chan);
}

/**
 * Wake-up all threads waiting on a channel.
 */
M0_INTERNAL void sim_chan_broadcast(struct sim_chan *chan)
{
	while (!sim_thr_tlist_is_empty(&chan->ch_threads))
		sim_chan_wake_head(chan);
}

/**
 * Currently executing thread or NULL if in the scheduler mode.
 */
M0_INTERNAL struct sim_thread *sim_thread_current(void)
{
	return sim_current;
}

/** get a pseudo-random number in the interval [a, b] */
M0_INTERNAL unsigned long long sim_rnd(unsigned long long a,
				       unsigned long long b)
{
	/*
	 * PRNG is a time critical piece of DES. Use very simple and fast linear
	 * congruential generator for now.
	 */

	unsigned long long scaled;
#if 0

	M0_ASSERT(a <= b);

	scaled = ((random()*(b - a + 1)) >> 31) + a;
#else /* glibc */
	static unsigned long seed = 1;

	seed = (1103515245 * seed + 12345) & 0xffffffff;

	scaled = ((seed * (b - a + 1)) >> 32) + a;
#endif
	M0_ASSERT(a <= scaled && scaled <= b);
	return scaled;
}

/**
 * Format optional arguments according to the format and store resulting
 * allocated string at a given place, freeing its previous contents if any.
 */
M0_INTERNAL void sim_name_set(char **name, const char *format, ...)
{
	va_list valist;

	va_start(valist, format);
	sim_name_vaset(name, format, valist);
	va_end(valist);
}

/**
 * Format arguments in valist according to the format and store resulting
 * allocated string at a given place, freeing its previous contents if any.
 */
M0_INTERNAL void sim_name_vaset(char **name, const char *format, va_list valist)
{
	if (*name != NULL)
		free(*name);

	if (vasprintf(name, format, valist) == -1)
		err(EX_SOFTWARE, "vasprintf");
}

enum sim_log_level sim_log_level = SLL_INFO;

/**
 * Write a log message to the console.
 */
M0_INTERNAL void sim_log(struct sim *s, enum sim_log_level level,
			 const char *format, ...)
{
	if (level <= sim_log_level) {
	    va_list valist;

	    va_start(valist, format);
	    if (s != NULL)
		    printf("[%15.9f] ", s->ss_bolt / 1000000000.);
	    else
		    printf("[---------------] ");
	    vprintf(format, valist);
	    va_end(valist);
	}
}

M0_INTERNAL int sim_global_init(void)
{
	cnt_global_init();
	return 0;
}

M0_INTERNAL void sim_global_fini(void)
{
	cnt_global_fini();
}

/** @} end of desim group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
