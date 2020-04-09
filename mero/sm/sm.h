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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 04/01/2010
 */

#pragma once

#ifndef __MERO_SM_SM_H__
#define __MERO_SM_SM_H__

#include "lib/types.h"               /* int32_t, uint64_t */
#include "lib/atomic.h"
#include "lib/time.h"                /* m0_time_t */
#include "lib/timer.h"
#include "lib/semaphore.h"
#include "lib/chan.h"
#include "lib/mutex.h"
#include "lib/tlist.h"
#include "addb2/histogram.h"

/**
   @defgroup sm State machine

   This modules defines interfaces to functionality common to typical
   non-blocking state machines extensively used by Mero.

   The main difference between "state machine" (non-blocking) code and
   "threaded" (blocking) code is that the latter blocks waiting for some events
   while having some computational state stored in the "native" C language stack
   (in the form of automatic variables allocated across the call-chain). Because
   of this the thread must remain dedicated to the same threaded activity not
   only during actual "computation", when processor is actively used, but also
   for the duration of wait. In many circumstances this is too expensive,
   because threads are heavy objects.

   Non-blocking code, on the other hand, packs all its state into a special
   data-structures before some potential blocking points and unpacks it after
   the event of interest occurs. This allows the same thread to be re-used for
   multiple non-blocking computations.

   Which blocking points deserve packing-unpacking depends on
   circumstances. Long-term waits for network or storage communication are prime
   candidates for non-blocking handling. Memory accesses, which can incur
   blocking page faults in a user space process are probably too ubiquitous for
   this. Memory allocations and data-structure locks fall into an intermediate
   group.

   This module defines data-structures and interfaces to handle common
   non-blocking state-machine functionality:

       - state tracking and state transitions;

       - concurrency;

       - interaction between a state machine and external world (both
         non-blocking and threaded);

       - accounting and statistics collection.

   <b>State and transitions.</b>

   State machine state is recorded in m0_sm::sm_state. This is supposed to be a
   relatively coarse-grained high level state, broadly determining state machine
   behaviour. An instance of m0_sm will typically be embedded into a larger
   structure containing fields fully determining state machine behaviour. Each
   state comes with a description. All descriptions for a particular state
   machine are packed into a m0_sm_conf::scf_state[] array.

   State machine is transferred from one state to another by a call to
   m0_sm_state_set() (or its variant m0_sm_fail()) or via "chained" transitions,
   see below.

   <b>Concurrency.</b>

   State machine is a part of a state machine group (m0_sm_group). All state
   machines in a group use group's mutex to serialise their state
   transitions. One possible scenario is to have a group for all state machines
   associated with a given locality (m0_fom_locality). Alternatively a
   group-per-machine can be used.

   <b>Interaction.</b>

   The only "output" event that a state machine communicates to the external
   world is (from this module's point of view) its state transition. State
   transitions are announced on a per-machine channel (m0_sm::sm_chan). This
   mechanism works both for threaded and non-blocking event consumers. The
   formers use m0_sm_timedwait() to wait until the state machine reaches
   desirable state, the latter register a clink with m0_sm::sm_chan.

   "Input" events cause state transitions. Typical examples of such events are:
   completion of a network or storage communication, timeout or a state
   transition in a different state machine. Such events often happen in
   "awkward" context: signal and interrupt handlers, timer call-backs and
   similar. Acquiring the group's mutex, necessary for state transition in such
   places is undesirable for multiple reasons:

       - to avoid self-deadlock in a case where an interrupt or signal is
         serviced by a thread that already holds the mutex, the latter must be
         made "async-safe", which is quite expensive;

       - implementation of a module that provides a call-back must take into
         account the possibility of the call-back blocking waiting for a
         mutex. This is also quite expensive;

       - locking order dependencies arise between otherwise unrelated
         components;

       - all these issues are exasperated in a situation where state transition
         must take additional locks, which it often does.

   The solution to these problems comes from operating system kernels design,
   see the AST section below.

   There are 2 ways to implement state machine input event processing:

       - "external" state transition, where input event processing is done
         outside of state machine, and m0_sm_state_set() is called to record
         state change:

@code
struct foo {
	struct m0_sm f_sm;
	...
};

void event_X(struct foo *f)
{
	m0_sm_group_lock(f->f_sm.sm_grp);
	process_X(f);
	m0_sm_state_set(&f->f_sm, NEXT_STATE);
	m0_sm_group_unlock(f->f_sm.sm_grp);
}
@endcode

      - "chained" state transition, where event processing logic is encoded in
      m0_sm_state_descr::sd_in() methods and a call to m0_sm_state_set() causes
      actual event processing to happen:

@code
int X_in(struct m0_sm *mach)
{
	struct foo *f = container_of(mach, struct foo, f_sm);
	// group lock is held.
	process_X(f);
	return NEXT_STATE;
}

const struct m0_sm_conf foo_sm_conf = {
	...
	.scf_state = foo_sm_states
};

struct m0_sm_state_descr foo_sm_states[] = {
	...
	[STATE_X] = {
		...
		.sd_in = X_in
	},
	...
};

void event_X(struct foo *f)
{
	m0_sm_group_lock(f->f_sm.sm_grp);
	// this calls X_in() and goes through the chain of state transitions.
	m0_sm_state_set(&f->f_sm, STATE_X);
	m0_sm_group_unlock(f->f_sm.sm_grp);
}

@endcode

   <b>Accounting and statistics.</b>

   This module accumulates statistics about state transitions and time spent in
   particular states. Statistics are reported through m0_sm_addb2_stats
   structure, associated with the state machine.

   <b>AST.</b>

   Asynchronous System Trap (AST) is a mechanism that allows a code running in
   an "awkward context" (see above) to post a call-back to be executed at the
   "base level" under a group mutex. UNIX kernels traditionally used a similar
   mechanism, where an interrupt handler does little more than setting a flag
   and returning. This flag is checked when the kernel is just about to return
   to the user space. If the flag is set, the rest of interrupt processing
   happens. In Linux a similar mechanism is called a "top-half" and
   "bottom-half" of interrupt processing. In Windows it is a DPC
   (http://en.wikipedia.org/wiki/Deferred_Procedure_Call) mechanism, in older
   DEC kernels it was called a "fork queue".

   m0_sm_ast structure represents a call-back to be invoked under group
   mutex. An ast is "posted" to a state machine group by a call to
   m0_sm_ast_post(), which can be done in any context, in the sense that it
   doesn't take any locks. Posted asts are executed

       - just after group mutex is taken;

       - just before group mutex is released;

       - whenever m0_sm_asts_run() is called.

   Ast mechanism solves the problems with input events mentioned above at the
   expense of

       - an increased latency: the call-back is not executed immediately;

       - an additional burden of ast-related book-keeping: it is up to the ast
         user to free ast structure when it is safe to do so (i.e., after the
         ast completed execution).

   To deal with the latency problem, a user must arrange m0_sm_asts_run() to be
   called during long state transitions (typically within loops).

   There are a few ways to deal with the ast book-keeping problem:

       - majority of asts will be embedded in some longer living data-structures
         like foms and won't need separate allocation of freeing;

       - some ast users might allocate asts dynamically;

       - the users which have neither a long-living data-structure to embed ast
         in nor can call dynamic allocator, have to pre-allocate a pool of asts
         and to guarantee somehow that it is never exhausted.

   If an ast is posted a m0_sm_group::s_clink clink is signalled. A user
   managing a state machine group might arrange a special "ast" thread (or a
   group of threads) to wait on this channel and to call m0_sm_asts_run() when
   the channel is signalled:

   @code
   while (1) {
           m0_chan_wait(&G->s_clink);
           m0_sm_group_lock(G);
	   m0_sm_asts_run(G);
           m0_sm_group_unlock(G);
   }
   @endcode

   A special "ast" thread is not needed if there is an always running "worker"
   thread or pool of threads associated with the state machine group. In the
   latter case, the worker thread can wait on m0_sm_group::s_clink in addition
   to other channels it waits on (see m0_clink_attach()).

   m0_sm_group_init() initialises m0_sm_group::s_clink with a NULL call-back. If
   a user wants to re-initialise it with a different call-back or to attach it
   to a clink group, it should call m0_clink_fini() followed by m0_clink_init()
   or m0_link_attach() before any state machine is created in the group.

   @{
*/

/* export */
struct m0_sm;
struct m0_sm_state_descr;
struct m0_sm_state_stats;
struct m0_sm_conf;
struct m0_sm_group;
struct m0_sm_ast;
struct m0_sm_addb2_stats;
struct m0_sm_group_addb2;
struct m0_sm_ast_wait;

/* import */
struct m0_timer;
struct m0_mutex;

/**
   state machine.

   Abstract state machine. Possibly persistent, possibly replicated.

   An instance of m0_sm is embedded in a concrete state machine (e.g., a
   per-endpoint rpc layer formation state machine, a resource owner state
   machine (m0_rm_owner), &c.).

   m0_sm stores state machine current state in m0_sm::sm_state. The semantics of
   state are not defined by this module except for classifying states into a few
   broad classes, see m0_sm_state_descr_flags. The only restriction on states is
   that maximal state (as a number) should not be too large, because all states
   are enumerated in a m0_sm::sm_conf::scf_state[] array.

   @invariant m0_sm_invariant() (under mach->sm_grp->s_lock).
 */
struct m0_sm {
	/**
	   Current state.

	   @invariant mach->sm_state < mach->sm_conf->scf_nr_states
	 */
	uint32_t                   sm_state;
	/**
	   State machine identifier
	 */
	uint64_t                   sm_id;
	/**
	   State machine configuration.

	   The configuration enumerates valid state machine states and
	   associates with every state some attributes that are used by m0_sm
	   code to check state transition correctness and to do some generic
	   book-keeping, including addb-based accounting.
	 */
	const struct m0_sm_conf   *sm_conf;
	struct m0_sm_group        *sm_grp;
	/**
	   The time entered to current state. Used to calculate how long
	   we were in a state (counted at m0_sm_state_stats::smss_times).
	 */
	m0_time_t                  sm_state_epoch;
	struct m0_sm_addb2_stats  *sm_addb2_stats;
	/**
	   Channel on which state transitions are announced.
	 */
	struct m0_chan             sm_chan;
	/**
	   State machine "return code". This is set to a non-zero value when
	   state machine transitions to an M0_SDF_FAILURE state.
	 */
	int32_t                    sm_rc;
	/**
           Sm invariant check could be expensive for some state machines.
         */
	bool                       sm_invariant_chk_off;
};

/**
   Configuration describes state machine type.

   m0_sm_conf enumerates possible state machine states.

   @invariant m0_sm_desc_invariant()
 */
struct m0_sm_conf {
	uint64_t                        scf_magic;
	const char                     *scf_name;
	/** Number of states in this state machine. */
	uint32_t                        scf_nr_states;
	/** Array of state descriptions. */
	struct m0_sm_state_descr       *scf_state;
	/** Number of state transitions in this state machine. */
	uint32_t                        scf_trans_nr;
	/** Array of state transitions descriptions. */
	struct m0_sm_trans_descr       *scf_trans;
	uint64_t                        scf_addb2_key;
	uint64_t                        scf_addb2_id;
	uint64_t                        scf_addb2_counter;
};

enum {
	M0_SM_MAX_STATES = 64
};

/**
   Description of some state machine state.
 */
struct m0_sm_state_descr {
	/**
	    Flags, broadly classifying the state, taken from
	    m0_sm_state_descr_flags.
	 */
	uint32_t    sd_flags;
	/**
	    Human readable state name for debugging. This field is NULL for
	    "invalid" states, which state machine may never enter.
	 */
	const char *sd_name;
	/**
	   This function (if non-NULL) is called by m0_sm_state_set() when the
	   state is entered.

	   If this function returns a non-negative number, the state machine
	   immediately transits to the returned state (this transition includes
	   all usual side-effects, like machine channel broadcast and invocation
	   of the target state ->sd_in() method). This process repeats until
	   ->sd_in() returns negative number. Such state transitions are called
	   "chained", see "chain" UT for examples. To fail the state machine,
	   set m0_sm::sm_rc manually and return one of M0_SDF_FAILURE states,
	   see C_OVER -> C_LOSE transition in the "chain" UT.
	 */
	int       (*sd_in)(struct m0_sm *mach);
	/**
	   This function (if non-NULL) is called by m0_sm_state_set() when the
	   state is left.
	 */
	void      (*sd_ex)(struct m0_sm *mach);
	/**
	   Invariant that must hold while in this state. Specifically, this
	   invariant is checked under the state machine lock once transition to
	   this state completed, checked under the same lock just before a
	   transition out of the state is about to happen and is checked (under
	   the lock) whenever a m0_sm call finds the target state machine in
	   this state.

	   If this field is NULL, no invariant checks are done.
	 */
	bool      (*sd_invariant)(const struct m0_sm *mach);
	/**
	   A bitmap of states to which transitions from this state are allowed.

	   @note this limits the number of states to 64, which should be more
	   than enough. Should a need in extra complicated machines arise, this
	   can be replaced with m0_bitmap, as the expense of making static
	   m0_sm_state_descr more complicated.
	 */
	uint64_t    sd_allowed;
	/**
	   An index map of allowed transitions from this state.
	   The index here is the state to which transition is allowed.
	   The value maps to the index in transitions array
	   (m0_sm_conf::scf_trans). The value of ~0 means that
	   transition is not allowed.  This field is constructed
	   at run-time in m0_sm_conf_init() routine, which must
	   be called before state machine with this configuration
	   can be constructed.
	 */
	uint32_t    sd_trans[M0_SM_MAX_STATES];
};

/**
   Flags for state classification, used in m0_sm_state_descr::sd_flags.
 */
enum m0_sm_state_descr_flags {
	/**
	    An initial state.

	    State machine, must start execution in a state marked with this
	    flag. Multiple states can be marked with this flag, for example, to
	    share a code between similar state machines, that only differ in
	    initial conditions.

	    @see m0_sm_init()
	 */
	M0_SDF_INITIAL  = 1 << 0,
	/**
	   A state marked with this flag is a failure state. m0_sm::sm_rc is set
	   to a non-zero value on entering this state.

	   In a such state, state machine is supposed to handle or report the
	   error indicated by m0_sm::sm_rc. Typically (but not necessary), the
	   state machine will transit into am M0_SDF_TERMINAL state immediately
	   after a failure state.

	   @see m0_sm_fail()
	 */
	M0_SDF_FAILURE  = 1 << 1,
	/**
	   A state marked with this flag is a terminal state. No transitions out
	   of this state are allowed (checked by m0_sm_conf_invariant()) and an
	   attempt to wait for a state transition, while the state machine is in
	   a terminal state, immediately returns -ESRCH.

	   @see m0_sm_timedwait()
	 */
	M0_SDF_TERMINAL = 1 << 2,
	/**
	   A state marked with this flag is a "final" state. State machine can
	   be finalised iff it is in state marked as M0_SDF_FINAL or
	   M0_SDF_TERMINAL. There can be multiple states marked as
	   M0_SDF_FINAL. M0_SDF_FINAL differs from M0_SDF_TERMINAL in that,
	   state machine can transition out of a final state.
	*/
	M0_SDF_FINAL    = 1 << 3

};

/**
   State transition description
 */
struct m0_sm_trans_descr {
	const char *td_cause;	/**< Cause of transition */
	uint32_t    td_src;	/**< Source state index */
	uint32_t    td_tgt;	/**< Target state index */
};

/**
   Asynchronous system trap.

   A request to execute a call-back under group mutex. An ast can be posted by a
   call to m0_sm_ast_post() in any context.

   It will be executed later, see AST section of the comment at the top of this
   file.

   Only m0_sm_ast::sa_cb and m0_sm_ast::sa_datum fields are public. The rest of
   this structure is for internal use by sm code.
 */
struct m0_sm_ast {
	/** Call-back to be executed. */
	void              (*sa_cb)(struct m0_sm_group *grp, struct m0_sm_ast *);
	/** This field is reserved for the user and not used by the sm code. */
	void               *sa_datum;
	struct m0_sm_ast   *sa_next;
	struct m0_sm       *sa_mach;
};

struct m0_sm_group {
	struct m0_mutex           s_lock;
	unsigned int              s_nesting;
	struct m0_clink           s_clink;
	struct m0_sm_ast         *s_forkq;
	struct m0_chan            s_chan;
	struct m0_sm_group_addb2 *s_addb2;
};

/**
   Initialises a state machine.

   @pre conf->scf_state[state].sd_flags & M0_SDF_INITIAL
 */
M0_INTERNAL void m0_sm_init(struct m0_sm *mach, const struct m0_sm_conf *conf,
			    uint32_t state, struct m0_sm_group *grp);
/**
   Finalises a state machine.

   @pre conf->scf_state[state].sd_flags & (M0_SDF_TERMINAL | M0_SDF_FINAL)
 */
M0_INTERNAL void m0_sm_fini(struct m0_sm *mach);

M0_INTERNAL void m0_sm_group_init(struct m0_sm_group *grp);
M0_INTERNAL void m0_sm_group_fini(struct m0_sm_group *grp);

M0_INTERNAL void m0_sm_group_lock(struct m0_sm_group *grp);
M0_INTERNAL void m0_sm_group_unlock(struct m0_sm_group *grp);
M0_INTERNAL bool m0_sm_group_is_locked(const struct m0_sm_group *grp);
M0_INTERNAL void m0_sm_group_lock_rec(struct m0_sm_group *grp, bool runast);
M0_INTERNAL void m0_sm_group_unlock_rec(struct m0_sm_group *grp, bool runast);
/**
   Waits until a given state machine enters any of states enumerated by a given
   bit-mask.

   @retval 0          - one of the states reached

   @retval -ESRCH     - terminal state reached,
                        see m0_sm_state_descr_flags::M0_SDF_TERMINAL

   @retval -ETIMEDOUT - deadline passed

   In case where multiple wait termination conditions hold simultaneously (e.g.,
   @states includes a terminal state), the result is implementation dependent.

   @note this interface assumes that states are numbered by numbers less than
   64.
 */
M0_INTERNAL int m0_sm_timedwait(struct m0_sm *mach, uint64_t states,
				m0_time_t deadline);

/**
   Moves a state machine into fail_state state atomically with setting rc code.

   @pre rc != 0
   @pre m0_mutex_is_locked(&mach->sm_grp->s_lock)
   @pre mach->sm_rc == 0
   @pre mach->sm_conf->scf_state[fail_state].sd_flags & M0_SDF_FAILURE
   @post mach->sm_rc == rc
   @post mach->sm_state == fail_state
   @post m0_mutex_is_locked(&mach->sm_grp->s_lock)
 */
M0_INTERNAL void m0_sm_fail(struct m0_sm *mach, int fail_state, int32_t rc);

/**
 * Moves a state machine into the next state, calling either m0_sm_state_set()
 * or m0_sm_fail() depending on "rc".
 */
M0_INTERNAL void m0_sm_move(struct m0_sm *mach, int32_t rc, int state);

/**
   Transits a state machine into the indicated state.

   Calls ex- and in- methods of the corresponding states (even if the state
   doesn't change after all).

   The (mach->sm_state == state) post-condition cannot be asserted, because of
   chained state transitions.

   Updates m0_sm_state_stats::smss_times statistics for the current state and
   sets the m0_sm::sm_state_epoch for the next state.

   @pre m0_mutex_is_locked(&mach->sm_grp->s_lock)
   @post m0_mutex_is_locked(&mach->sm_grp->s_lock)
 */
void m0_sm_state_set(struct m0_sm *mach, int state);

/** Get human readable name (m0_sm_state_descr::sd_name) for the given state */
M0_INTERNAL const char *m0_sm_state_name(const struct m0_sm *mach, int state);
M0_INTERNAL const char *m0_sm_conf_state_name(const struct m0_sm_conf *conf,
					      int state);

/**
 * State machine timer.
 *
 * A state machine timer is associated with a state machine group and executes
 * a specified call-back after a specified deadline and under the group lock.
 */
struct m0_sm_timer {
	struct m0_sm_group *tr_grp;
	struct m0_timer     tr_timer;
	struct m0_sm_ast    tr_ast;
	/** Call-back to be executed after timer expiration. */
	void              (*tr_cb)(struct m0_sm_timer *);
	/**
	 * Timer state from enum timer_state (sm.c).
	 */
	int                 tr_state;
};

M0_INTERNAL void m0_sm_timer_init(struct m0_sm_timer *timer);
M0_INTERNAL void m0_sm_timer_fini(struct m0_sm_timer *timer);

/**
 * Starts the timer.
 *
 * When the specified (absolute) deadline expires, an AST is posted in the
 * specified state machine group. When this AST is executed, it calls the
 * user-supplied call-back.
 *
 * If the deadline is already in the past by the time this is called, the AST is
 * posted as soon as possible.
 */
M0_INTERNAL int  m0_sm_timer_start(struct m0_sm_timer *timer,
				   struct m0_sm_group *group,
				   void (*cb)(struct m0_sm_timer *),
				   m0_time_t deadline);
M0_INTERNAL void m0_sm_timer_cancel(struct m0_sm_timer *timer);
M0_INTERNAL bool m0_sm_timer_is_armed(const struct m0_sm_timer *timer);


/**
   Structure used by m0_sm_timeout_arm() to record timeout state.

   This structure is owned by the sm code, user should not access it. The user
   provides initialised (by m0_sm_timeout_init()) instance of m0_sm_timeout to
   m0_sm_timeout_arm(). After the timer expiration, the instance must be
   finalised (with m0_sm_timeout_fini()) and re-initialised before it can be
   used again.
 */
struct m0_sm_timeout {
	/** Timer used to implement delayed state transition. */
	struct m0_sm_timer st_timer;
	/** Clink to watch for state transitions that might cancel the
	    timeout. */
	struct m0_clink    st_clink;
	/** Target state. */
	int                st_state;
	/**
	 * Transitions to states in this bit-mask won't cancel the timeout.
	 */
	uint64_t           st_bitmask;
};

/**
   Initialises a timer structure with a given timeout.
 */
M0_INTERNAL void m0_sm_timeout_init(struct m0_sm_timeout *to);

/**
   Arms a timer to move a machine into a given state after a given timeout.

   If a state transition happens before the timeout expires, the timeout is
   cancelled, unless the transition is to a state from "bitmask" parameter.

   It is possible to arm multiple timeouts against the same state machine.

   The m0_sm_timeout instance, supplied to this call can be freed after timeout
   expires or is cancelled.

   @param timeout absolute time at which the state transition will take place
   @param state the state to which the state machine will transition after the
   timeout.
   @param bitmask a mask of state machine states, transitions which won't cancel
   the timeout.

   @pre m0_mutex_is_locked(&mach->sm_grp->s_lock)
   @pre sm_state(mach)->sd_allowed & M0_BITS(state)
   @pre m0_forall(i, mach->sm_conf->scf_nr_states,
		  ergo(M0_BITS(i) & bitmask,
		       state_get(mach, i)->sd_allowed & M0_BITS(state)))
   @post m0_mutex_is_locked(&mach->sm_grp->s_lock)
 */
M0_INTERNAL int m0_sm_timeout_arm(struct m0_sm *mach, struct m0_sm_timeout *to,
				  m0_time_t timeout, int state,
				  uint64_t bitmask);
/**
   Finaliser that must be called before @to can be freed.
 */
M0_INTERNAL void m0_sm_timeout_fini(struct m0_sm_timeout *to);

/**
   Returns true iff timer associated with the timeout is running.
 */
M0_INTERNAL bool m0_sm_timeout_is_armed(const struct m0_sm_timeout *to);

/**
 * Posts an AST to a group.
 *
 * An AST must not be re-posted until its previous (already posted) execution
 * completes.
 */
M0_INTERNAL void m0_sm_ast_post(struct m0_sm_group *grp, struct m0_sm_ast *ast);

/**
 * Cancels a posted AST.
 *
 * If the AST has already been executed, nothing is done.
 *
 * @post ast->sa_next == NULL
 */
M0_INTERNAL void m0_sm_ast_cancel(struct m0_sm_group *grp,
				  struct m0_sm_ast   *ast);

/**
   Runs posted, but not yet executed ASTs.

   @pre m0_mutex_is_locked(&grp->s_lock)
   @post m0_mutex_is_locked(&grp->s_lock)
 */
M0_INTERNAL void m0_sm_asts_run(struct m0_sm_group *grp);

enum m0_sm_return {
	/**
	 * Negative mumbers are used to return from state function without
	 * transitioning to next state.
	 */
	M0_SM_BREAK = -1,
};

/**
 * Extends transition table of "base" with new transitions from "sub".
 *
 * Resulting table is stored in "sub", which should be of sufficient size.
 * Transitions in "sub" override matching transitions in "base".
 *
 * sub->scf_trans[] reserves array elements for base. Empty slots in
 * sub->scf_trans[] could be in arbitrary places.
 */
M0_INTERNAL void m0_sm_conf_trans_extend(const struct m0_sm_conf *base,
					 struct m0_sm_conf *sub);
/**
 * "Extends" base state descriptions with the given sub descriptions.
 *
 * Updates sub in place to become a merged state machine descriptions array that
 * uses base state descriptors, unless overridden by sub.
 */
M0_INTERNAL void m0_sm_conf_extend(const struct m0_sm_state_descr *base,
				   struct m0_sm_state_descr *sub, uint32_t nr);

M0_INTERNAL bool m0_sm_invariant(const struct m0_sm *mach);

/**
 * Initialises state machine configuration.
 *
 * Traverses transitions description array and constructs
 * m0_sm_state_descr::sd_trans transitions map array for each state.
 * It also makes sure (asserts) that transitions configuration in
 * transitions description array matches with the same at states
 * description array according to m0_sm_state_descr::sd_allowed flags.
 *
 * @pre !m0_sm_conf_is_initialized(conf)
 * @pre conf->scf_trans_nr > 0
 */
M0_INTERNAL void m0_sm_conf_init(struct m0_sm_conf *conf);

/**
 * Finalises state machine configuration.
 *
 * @see m0_addb_rec_type_umregister()
 *
 * @pre conf->scf_magic == M0_SM_CONF_MAGIC
 */
M0_INTERNAL void m0_sm_conf_fini(struct m0_sm_conf *conf);

/**
 * Returns true if sm configuration was initialized already.
 */
M0_INTERNAL bool m0_sm_conf_is_initialized(const struct m0_sm_conf *conf);

int m0_sm_group_call(struct m0_sm_group *group,
		     int (*cb)(void *), void *data);

struct m0_sm_addb2_stats {
	uint64_t             as_id;
	int                  as_nr;
	struct m0_addb2_hist as_hist[0];
};

struct m0_sm_group_addb2 {
	uint64_t             ga_forq;
	struct m0_addb2_hist ga_forq_hist;
};

M0_INTERNAL int m0_sm_addb2_init(struct m0_sm_conf *conf,
         uint64_t id, uint64_t counter);

M0_INTERNAL void m0_sm_addb2_fini(struct m0_sm_conf *conf);

M0_INTERNAL bool m0_sm_addb2_counter_init(struct m0_sm *sm);

/**
 * API of waiting for AST completion.
 *
 * Use example:
 *
 * @code
 * struct m0_sm_ast_wait *wait  = ...;
 * struct m0_mutex       *guard = ...; // m0_sm_group::s_lock, when possible.
 *
 * m0_sm_ast_wait_init(wait, guard);
 *
 * // Thread A:
 * ast->sa_cb = ast_cb; // Must call m0_sm_ast_wait_signal() at the end.
 * m0_mutex_lock(guard);
 * m0_sm_ast_wait_post(wait, grp, ast);
 * m0_mutex_unlock(guard);
 *
 * // Thread B:
 * m0_mutex_lock(guard);
 * m0_sm_ast_wait(wait);
 * m0_mutex_unlock(guard);
 * @endcode
 */
struct m0_sm_ast_wait {
	/** Whether posting of ASTs is allowed. */
	bool               aw_allowed;
	/** Number of posted, still not completed ASTs. */
	struct m0_atomic64 aw_active;
	/** Channel on which AST completion is signalled. */
	struct m0_chan     aw_chan;
};

M0_INTERNAL void m0_sm_ast_wait_init(struct m0_sm_ast_wait *wait,
				     struct m0_mutex *ch_guard);
M0_INTERNAL void m0_sm_ast_wait_fini(struct m0_sm_ast_wait *wait);

/**
 * Waits until all m0_sm_ast_wait_post()ed ASTs are executed.
 *
 * @pre  m0_chan_is_locked(&wait->aw_chan)
 */
M0_INTERNAL void m0_sm_ast_wait(struct m0_sm_ast_wait *wait);

/**
 * Posts an AST that might be waited for.
 *
 * @note ast->sa_cb must call m0_sm_ast_wait_signal() as its last action.
 */
M0_INTERNAL void m0_sm_ast_wait_post(struct m0_sm_ast_wait *wait,
				     struct m0_sm_group *grp,
				     struct m0_sm_ast *ast);

/**
 * Signifies completion of an AST, posted with m0_sm_ast_wait_post().
 *
 * @pre  m0_chan_is_locked(&wait->aw_chan)
 */
M0_INTERNAL void m0_sm_ast_wait_signal(struct m0_sm_ast_wait *wait);

M0_INTERNAL void m0_sm_ast_wait_prepare(struct m0_sm_ast_wait *wait,
					struct m0_clink *clink);
M0_INTERNAL void m0_sm_ast_wait_complete(struct m0_sm_ast_wait *wait,
					 struct m0_clink *clink);
M0_INTERNAL void m0_sm_ast_wait_loop(struct m0_sm_ast_wait *wait,
				     struct m0_clink *clink);

/**
 * Outputs the dot-language description of the configuration to the console.
 */
M0_INTERNAL void m0_sm_conf_print(const struct m0_sm_conf *conf);

/**
 * @return given SM identifier.
 */
M0_INTERNAL uint64_t m0_sm_id_get(const struct m0_sm *sm);


/** @} end of sm group */
#endif /* __MERO_SM_SM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
