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
 * Original creation date: 05/13/2010
 */

#pragma once

#ifndef __MERO_LIB_CHAN_H__
#define __MERO_LIB_CHAN_H__

#include "lib/tlist.h"
#include "lib/mutex.h"
#include "lib/time.h"
#include "lib/semaphore.h"
#include "addb2/histogram.h"

/**
   @defgroup chan Waiting channels

   Waiting channel.

   A channel (m0_chan) is a stream of asynchronous events that a channel user
   can wait or register a call-back for.

   A clink (m0_clink) is a record of interest in events on a particular
   channel. A user adds a clink to a channel and appearance of new events in
   the stream is recorded in the clink.

   There are two interfaces related to channels:

       - producer interface. It consists of m0_chan_signal(), m0_clink_signal()
         and m0_chan_broadcast() functions. These functions are called to
         declare that new asynchronous event happened in the stream.

       - consumer interface. It consists of m0_clink_add(), m0_clink_del(),
         m0_chan_wait() and m0_chan_trywait() functions.

   When a producer declares an event on a channel, this event is delivered. If
   event is a broadcast (m0_chan_broadcast()) it is delivered to all clinks
   registered with the channel. If event is a signal (m0_chan_signal()) it is
   delivered to a single clink (if any) registered with the channel. Clinks for
   delivery of consecutive signals are selected in a round-robin manner.

   A special m0_clink_signal() function is provided to signal a particular
   clink. m0_clink_signal() delivers a signal to its argument clink. This
   function does not take any locks and is designed to be used in "awkward"
   contexts, like interrupt handler or timer call-backs, where blocking on a
   lock is not allowed. Use sparingly.

   The method of delivery depends on the clink interface used (m0_clink). If
   clink has a call-back, the delivery starts with calling this call-back. If a
   clink has no call-back or the call-back returns false, the delivered event
   becomes pending on the clink. Pending events can be consumed by calls to
   m0_chan_wait(), m0_chan_timedwait() and m0_chan_trywait().

   <b>Filtered wake-ups.</b>

   By returning true from a call-back, it is possible to "filter" some events
   out and avoid potentially expensive thread wake-up. A typical use case for
   this is the following:

   @code
   struct wait_state {
           struct m0_clink f_clink;
	   ...
   };

   static bool callback(struct m0_clink *clink)
   {
           struct wait_state *f =
                   container_of(clink, struct wait_state, f_clink);
	   return !condition_is_right(f);
   }

   {
           struct wait_state g;

	   m0_clink_init(&g.f_clink, &callback);
	   m0_clink_add(chan, &g.f_clink);
	   ...
	   while (!condition_is_right(&g)) {
	           m0_chan_wait(&g.f_clink);
	   }
   }
   @endcode

   The idea behind this idiom is that the call-back is called in the same
   context where the event is declared and it is much cheaper to test whether a
   condition is right than to wake up a waiting thread that would check this
   and go back to sleep if it is not.

   <b>Multiple channels.</b>

   It is possible to wait for an event to be announced on a channel from a
   set. To this end, first a clink is created as usual. Then, additional
   (unintialised) clinks are attached to the first by a call to
   m0_clink_attach(), forming a "clink group" consisting of the original clink
   and all clinks attached. Clinks from the group can be registered with
   multiple (or the same) channels. Events announced on any channel are
   delivered to all clinks in the group.

   Groups are used as following:

       - initialise a "group head" clink;

       - attach other clinks to the group, without initialising them;

       - register the group clinks with their channels, starting with the head;

       - to wait for an event on any channel, wait on the group head.

       - call-backs can be used for event filtering on any channel as usual;

       - de-register the clinks, head last.

   @code
   struct m0_clink cl0;
   struct m0_clink cl1;

   m0_clink_init(&cl0, call_back0);
   m0_clink_attach(&cl1, &cl0, call_back1);

   m0_clink_add(chan0, &cl0);
   m0_clink_add(chan1, &cl1);

   // wait for an event on chan0 or chan1
   m0_chan_wait(&cl0);

   // de-register clinks, head last
   m0_clink_del(&cl1);
   m0_clink_del(&cl0);

   // finalise in any order
   m0_clink_fini(&cl0);
   m0_clink_fini(&cl1);
   @endcode

   @note An interface similar to m0_chan was a part of historical UNIX kernel
   implementations. It is where "CHAN" field in ps(1) output comes from.

   @todo The next scalability improvement is to allow m0_chan to use an
   externally specified mutex instead of a built-in one. This would allow
   larger state machines with multiple channels to operate under fewer locks,
   reducing coherency bus traffic.

   @{
*/

struct m0_chan;
struct m0_clink;
struct m0_chan_addb2;

/**
   Clink call-back called when event is delivered to the clink. The call-back
   returns true iff the event has been "consumed". Otherwise, the event will
   remain pending on the clink for future consumption by the waiting
   interfaces.
 */
typedef bool (*m0_chan_cb_t)(struct m0_clink *link);

/**
   A stream of asynchronous events.

   <b>Concurrency control</b>

   There are three groups of channel functions with different serialization
   requirements:

   - (A) caller holds the ch_guard lock:

          m0_clink_add(),
          m0_clink_del(),
          m0_chan_signal(),
          m0_chan_broadcast(),
          m0_chan_fini();

         The implementation checks that the channel lock is held.

   - (B) caller does not hold the ch_guard lock:

          m0_clink_add_lock(),
          m0_clink_del_lock(),
          m0_chan_signal_lock(),
          m0_chan_broadcast_lock(),
          m0_chan_fini_lock();

         The implementation checks that the channel lock is not held.

   - (C) caller may hold the ch_guard lock:

          m0_clink_init(),
          m0_clink_fini(),
          m0_clink_attach(),
          m0_clink_is_armed(),
          m0_clink_signal() (note this lock-tolerant signalling facility!),
          m0_chan_init(),
          m0_chan_wait(),
          m0_chan_trywait(),
          m0_chan_timedwait().

         Nothing is assumed by the implementation about the channel lock.

   <b>Liveness</b>

   A user has to enforce a serialization between event production and channel
   destruction.

   <b>Invariants</b>

   m0_chan_invariant()
 */
struct m0_chan {
	/** Protecting lock, should be provided by user. */
	struct m0_mutex      *ch_guard;
	/** List of registered clinks. */
	struct m0_tl          ch_links;
	/** Number of clinks in m0_chan::ch_links. This is used to speed up
	    m0_chan_broadcast(). */
	uint32_t              ch_waiters;
	struct m0_chan_addb2 *ch_addb2;
};

/**
   A record of interest in events on a stream.

   A clink records the appearance of events in the stream.

   There are two ways to use a clink:

   @li an asynchronous call-back can be specified as an argument to clink
   constructor m0_clink_init(). This call-back is called when an event happens
   in the channel the clink is registered with. It is guaranteed that
   a call-back is executed in the same context where event producer declared
   new event. A per-channel mutex m0_chan::ch_guard is held while call-backs
   are executed (except the case when m0_clink_signal() is used by producer).

   @li once a clink is registered with a channel, it is possible to wait until
   an event happens by calling m0_chan_wait().

   See the "Filtered wake-ups" section in the top-level comment on how to
   combine call-backs with waiting.

   <b>Concurrency control</b>

   A user must guarantee that at most one thread waits on a
   clink. Synchronization between call-backs, waits and clink destruction is
   also up to user.

   A user owns a clink before call to m0_clink_add() and after return from the
   m0_clink_del() call. At any other time clink can be concurrently accessed by
   the implementation.

   <b>Liveness</b>

   A user is free to dispose a clink whenever it owns the latter.
 */
struct m0_clink {
	/** Channel this clink is registered with. */
	struct m0_chan     *cl_chan;
	/** Call-back to be called when event is declared. */
	m0_chan_cb_t        cl_cb;
	/** The head of the clink group. */
	struct m0_clink    *cl_group;
	/** Linkage into m0_chan::ch_links */
	struct m0_tlink     cl_linkage;
	struct m0_semaphore cl_wait;
	bool                cl_is_oneshot;
	uint64_t            cl_magic;
};

M0_INTERNAL void m0_chan_init(struct m0_chan *chan, struct m0_mutex *ch_guard);
M0_INTERNAL void m0_chan_fini(struct m0_chan *chan);
M0_INTERNAL void m0_chan_fini_lock(struct m0_chan *chan);

/**
   Notifies a clink currently registered with the channel that a new event
   happened.

   @pre m0_chan_is_locked(chan)
   @see m0_chan_broadcast()
 */
M0_INTERNAL void m0_chan_signal(struct m0_chan *chan);

/**
   Calls m0_chan_signal() with ch_guard locked.
 */
M0_INTERNAL void m0_chan_signal_lock(struct m0_chan *chan);

/**
   Notifies all clinks currently registered with the channel that a new event
   happened.

   No guarantees about behaviour in the case when clinks are added or removed
   while m0_chan_broadcast() is running.

   If clinks with call-backs (m0_clink::cl_cb) are registered with the channel
   at the time of this call, the call-backs are run to completion as part of
   broadcast.

   @pre m0_chan_is_locked(chan)
   @see m0_chan_signal()
 */
M0_INTERNAL void m0_chan_broadcast(struct m0_chan *chan);

/**
   Calls m0_chan_broadcast() with ch_guard locked.
 */
M0_INTERNAL void m0_chan_broadcast_lock(struct m0_chan *chan);

/**
   Notifies a given clink that a new event happened.

   This function takes no locks.

   m0_chan_signal() should be used instead, unless the event is announced in a
   context where blocking is not allowed.

   @see m0_chan_signal()
   @see m0_chan_broadcast()
 */
M0_INTERNAL void m0_clink_signal(struct m0_clink *clink);

/**
   True iff there are clinks registered with the chan.

   @note the return value of this function can, in general, be obsolete by the
   time it returns. It is up to the user to provide concurrency control
   mechanisms that would make this function useful.
 */
M0_INTERNAL bool m0_chan_has_waiters(struct m0_chan *chan);

M0_INTERNAL void m0_clink_init(struct m0_clink *link, m0_chan_cb_t cb);
M0_INTERNAL void m0_clink_fini(struct m0_clink *link);

/**
   Attaches @link to a clink group. @group is the original clink in the group.
 */
M0_INTERNAL void m0_clink_attach(struct m0_clink *link,
				 struct m0_clink *group, m0_chan_cb_t cb);

/**
   Registers the clink with the channel.

   @pre m0_chan_is_locked(chan)
   @pre !m0_clink_is_armed(link)
   @post m0_clink_is_armed(link)
 */
M0_INTERNAL void m0_clink_add(struct m0_chan *chan, struct m0_clink *link);

/**
   Un-registers the clink from the channel.

   @pre m0_chan_is_locked(chan)
   @pre   m0_clink_is_armed(link)
   @post !m0_clink_is_armed(link)
 */
M0_INTERNAL void m0_clink_del(struct m0_clink *link);

/**
   Calls m0_clink_add() with ch_guard locked.
 */
M0_INTERNAL void m0_clink_add_lock(struct m0_chan *chan, struct m0_clink *link);

/**
   Calls m0_clink_del() with ch_guard locked.
 */
M0_INTERNAL void m0_clink_del_lock(struct m0_clink *link);

/**
   True iff the clink is registered with a channel.
 */
M0_INTERNAL bool m0_clink_is_armed(const struct m0_clink *link);

/**
   If clink armed, deletes the one from its channel. Otherwise, does nothing.

   @pre !m0_chan_is_locked(link->cl_chan)
 */
M0_INTERNAL void m0_clink_cleanup(struct m0_clink *link);

/**
   If clink armed, deletes the one from its channel. Otherwise, does nothing.

   @pre m0_chan_is_locked(link->cl_chan)
 */
M0_INTERNAL void m0_clink_cleanup_locked(struct m0_clink *link);

/**
   Returns when there is an event pending in the clink. The event is consumed
   before the call returns.

   Note that this implies that if an event happened after the clink has been
   registered (by a call to m0_clink_add()) and before call to m0_chan_wait(),
   the latter returns immediately.

   User must guarantee that no more than one thread waits on the clink.
 */
M0_INTERNAL void m0_chan_wait(struct m0_clink *link);

/**
   True there is an event pending in the clink. When this function returns true,
   the event is consumed, exactly like if m0_chan_wait() were called instead.
 */
M0_INTERNAL bool m0_chan_trywait(struct m0_clink *link);

/**
   This is the same as m0_chan_wait, except that it has an expire time. If the
   time expires before event is pending, this function will return false.

   @param abs_timeout absolute time since Epoch (00:00:00, 1 January 1970)
   @return true if the there is an event pending before timeout;
   @return false if there is no events pending and timeout expires;
 */
M0_INTERNAL bool m0_chan_timedwait(struct m0_clink *link,
				   const m0_time_t abs_timeout);

/**
   Locks ch_guard.
 */
M0_INTERNAL void m0_chan_lock(struct m0_chan *ch);

/**
   Unlocks ch_guard.
 */
M0_INTERNAL void m0_chan_unlock(struct m0_chan *ch);

/**
   Tests ch_guard for being locked.
 */
M0_INTERNAL bool m0_chan_is_locked(const struct m0_chan *ch);

struct m0_chan_addb2 {
	uint64_t             ca_wait;
	uint64_t             ca_cb;
	struct m0_addb2_hist ca_wait_hist;
	struct m0_addb2_hist ca_cb_hist;
	struct m0_addb2_hist ca_queue_hist;
};

/** @} end of chan group */
#endif /* __MERO_LIB_CHAN_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
