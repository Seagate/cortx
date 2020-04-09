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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 11/10/2011
 */

/**
   @page LNetcqueueDLD LNet Buffer Event Circular Queue DLD

   - @ref cqueueDLD-ovw
   - @ref cqueueDLD-def
   - @ref cqueueDLD-req
   - @ref cqueueDLD-depends
   - @ref cqueueDLD-highlights
   - @subpage cqueueDLD-fspec "Functional Specification"
      - @ref bevcqueue "External Interfaces"        <!-- int link -->
   - @ref cqueueDLD-lspec
      - @ref cqueueDLD-lspec-comps
      - @ref cqueueDLD-lspec-q
      - @ref cqueueDLD-lspec-state
      - @ref cqueueDLD-lspec-thread
      - @ref cqueueDLD-lspec-numa
   - @ref cqueueDLD-conformance
   - @ref cqueueDLD-ut
   - @ref cqueueDLD-st
   - @ref cqueueDLD-O
   - @ref cqueueDLD-ref

   <hr>
   @section cqueueDLD-ovw Overview

   The circular queue provides a data structure and interfaces to manage a
   lock-free queue for a single producer and consumer.  The producer and
   consumer can be in different address spaces with the queue in shared memory.
   The circular queue is designed to specifically meet the needs of the
   @ref KLNetCoreDLD "Core API". <!-- ./linux_kernel/klnet_core.c -->
   In particular see the @ref KLNetCoreDLD-lspec-bevq "The Buffer Event Queue"
   section.

   The queue implementation does not address how the consumer gets notified that
   queue elements have been produced. That functionality is provided separately
   by the Core API nlx_core_buf_event_wait() subroutine.

   <hr>
   @section cqueueDLD-def Definitions

   Refer to <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV
779386NtFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>

   <hr>
   @section cqueueDLD-req Requirements

   - @b r.m0.lib.atomic.interoperable-kernel-user-support The
   implementation shall provide a queue that supports atomic,
   interoperable sharing between kernel to user-space.
   - @b r.net.xprt.lnet.growable-event-queue The implementation shall
   support an event queue to which new elements can be added over time.

   <hr>
   @section cqueueDLD-depends Dependencies

   - The @ref atomic <!-- lib/atomic.h --> API.

   <hr>
   @section cqueueDLD-highlights Design Highlights

   - A data structure representing a circular queue.
   - The circular queue efficiently delivers event notifications from the
   LNet Transport Kernel Core layer to the LNet transport layer.
   - Handles atomic access to elements in the queue for a single producer and
   consumer.
   - Handles dynamically adding new elements to the circular queue.

   <hr>
   @section cqueueDLD-lspec Logical Specification

   - @ref cqueueDLD-lspec-comps
   - @ref cqueueDLD-lspec-q
   - @ref cqueueDLD-lspec-xlink
   - @ref cqueueDLD-lspec-qalloc
   - @ref cqueueDLD-lspec-state
   - @ref cqueueDLD-lspec-thread
   - @ref cqueueDLD-lspec-numa

   @subsection cqueueDLD-lspec-comps Component Overview

   The circular queue is a single component.

   @subsection cqueueDLD-lspec-q Logic of the Circular Queue

   The circular queue is a FIFO queue.  The implementation maintains pointers
   for the consumer and producer, a count of the number of elements that can
   currently be consumed, The total number of elements in the queue (both those
   that are consumable and those that are not currently consumable) and
   operations for accessing the pointers and for moving them around the circular
   queue elements.  The application manages the memory containing the queue
   itself, and adds new elements to the queue when the size of the queue needs
   to grow.  In this discussion of the logic, the pointers are named
   @c consumer, @c producer and @c next for brevity.

   @dot
   digraph {
   {
       rank=same;
       node [shape=plaintext];
       nlx_core_bev_cqueue;
       node [shape=record];
       struct1 [label="<f0> consumer|<f1> producer"];
   }
   {
       rank=same;
       ordering=out;
       node [shape=plaintext];
       "element list";
       node [shape=record];
       list1 [label="<f0> |<f1> y|<f2> x|<f3> x|<f4> x|<f5> x|<f6> |<f7> "];
       "element list" -> list1 [style=invis];
   }
   {
       rank=same;
       x1 [shape=point width=0 height=0];
       x2 [shape=point width=0 height=0];
       x1 -> x2 [dir=none];
   }
   nlx_core_bev_cqueue -> "element list" [style=invis];
   struct1:f0 -> list1:f1;
   struct1:f1 -> list1:f6;
   list1:f7 -> x2 [dir=none];
   list1:f0 -> x1 [dir=back];
   }
   @enddot

   The elements starting after @c consumer up to but not including @c producer
   contain data to be consumed (those elements marked with "x" in the diagram).
   So, @c consumer follows @c producer around the circular queue.  When
   @c consumer->next is the same as @c producer, the queue is empty
   (requiring that the queue be initialized with at least 2 elements).  The
   element pointed to by @c consumer (element "y" in the diagram) is the
   element most recently consumed by the consumer.  The producer cannot use this
   element, because if it did, producing that element would result in moving
   @c producer so that it would pass @c consumer.

   In the context of the LNet Buffer Event Queue, the transport should add
   enough elements to the queue strictly before it enqueues buffer operations
   requiring subsequent completion notifications.  The number required is the
   total number of possible events generated by queued buffers, plus one extra
   element for the most recently consumed event notification.  The circular
   queue does not enforce this requirement, but does provide APIs that the
   transport can use to determine the current number of elements in the queue
   and to add new elements.

   The element denoted by @c producer is returned by bev_cqueue_pnext() as
   long as the queue is not full.  This allows the producer to determine the
   next available element and populate it with the data to be produced.  Once
   the element contains the data, the producer then calls bev_cqueue_put()
   to make that element available to the consumer.  This call also moves the
   @c producer pointer to the next element and increments the @c count of
   consumable elements.

   The consumer uses bev_cqueue_get() to get the next available element
   containing data in FIFO order.  Consuming an element causes @c consumer to
   be pointed at the next element in the queue and decrementing the @c count of
   consumable elements.  After this call returns, the
   consumer "owns" the element returned, element "y" in the diagram.  The
   consumer owns this element until it calls bev_cqueue_get() again, at which
   time ownership reverts to the queue and can be reused by the producer.

   @subsection cqueueDLD-lspec-xlink Cross Address Space Linkage Support

   The pointers themselves are more complex than the description above suggests.
   The @c consumer pointer refers to the element just consumed in the consumer's
   (the transport) address space.  The @c producer pointer refers to the element
   in the producer's (the kernel) address space.

   A queue link element (the @c next pointer in the preceding discussion) is
   represented by the nlx_core_bev_link data structure:
   @code
   struct nlx_core_bev_link {
            // Self pointer in the transport address space.
            nlx_core_opaque_ptr_t cbl_c_self;
            // Pointer to the next element in the consumer address space.
            nlx_core_opaque_ptr_t cbl_c_next;
            // Self reference in the producer.
            struct nlx_core_kmem_loc cbl_p_self_loc;
            // Reference to the next element in the producer.
            struct nlx_core_kmem_loc cbl_p_next_loc;
   };
   @endcode
   The data structure maintains separate "opaque" pointer fields for the
   producer and consumer address spaces.  Elements in the queue are linked
   through both flavors of their @c next field.  The initialization of this data
   structure is described in @ref cqueueDLD-lspec-qalloc.  The opaque
   pointer type is derived from ::uint64_t.  In the case of the producer,
   the @c nlx_core_kmem_loc structure is used instead of a pointer.  This
   allows the buffer event object itself to be mapped and unmapped temporarily,
   rather than requiring all buffer events to be mapped in the kernel at all
   times (since this could exhaust the kernel page map table in the case of
   a user space consumer).

   When the producer performs a bev_cqueue_put() call, internally, this call
   uses nlx_core_bev_link::cbl_p_next_loc to refer to the next element (and
   increment the @c count of consumable elements).  Similarly, when the consumer
   performs a bev_cqueue_get() call, internally this subroutine uses
   nlx_core_bev_link::cbl_c_next (and decrements the @c count of consumable
   elements).  Note that only allocation, discussed below,
   modifies any of these pointers.  Steady-state operations on the queue only
   modify the @c consumer and @c producer pointers.

   Because the @c producer "pointer" is implemented as a nlx_core_kmem_loc,
   it cannot be accessed atomically.  So, a comparison like

   @code
       q->producer != q->consumer
   @endcode

   cannot be implemented in general, without synchronization.  However, by
   keeping an atomic @c count of consumable elements, subroutines such as
   @c bev_cqueue_is_empty() can be implemented by testing the @c count rather
   than comparing pointers.

   @subsection cqueueDLD-lspec-qalloc Circular Queue Allocation

   The circular queue must contain at least 2 elements, as discussed above.
   Additional elements can be added to maintain the requirement that the number
   of elements in the queue equals or exceeds the number of pending buffer
   operations, plus one element for the most recently consumed operation.

   The initial condition is shown below.  In this diagram, the queue is empty
   (see the state discussion, below).  There is room in the queue for one
   pending buffer event and one completed/consumed event.

   @dot
   digraph {
   {
       rank=same;
       node [shape=plaintext];
       nlx_core_bev_cqueue;
       node [shape=record];
       struct1 [label="<f0> consumer|<f1> producer"];
   }
   {
       rank=same;
       ordering=out;
       node [shape=plaintext];
       "element list";
       node1 [shape=box];
       node2 [shape=box];
       "element list" -> node1 [style=invis];
       node1 -> node2 [label=next];
       node2 -> node1 [label=next];
   }
   nlx_core_bev_cqueue -> "element list" [style=invis];
   struct1:f0 -> node1;
   struct1:f1 -> node2;
   }
   @enddot

   Before adding additional elements, the following are true:
   - The number of elements in the queue, N, equals the number of pending
   operations plus one for the most recently consumed operation completion
   event.
   - The producer produces one event per pending operation.
   - The producer will never catch up with the consumer.  Given the required
   number of elements, the producer will run out of work to do when it has
   generated one event for each buffer operation, resulting in a state where
   <tt> producer == consumer </tt>.

   This means the queue can @b safely be expanded at the location of the @c
   consumer pointer (i.e. in the consumer address space), without
   affecting the producer.  Elements are added as follows:

   -# Allocate and initialize a new queue element (referred to as @c newnode)
      which sets @c newnode->c_self and @c newnode->p_self.
   -# Set <tt>  newnode->next = consumer->next </tt>
   -# Set <tt> consumer->next = newnode        </tt>
   -# set <tt>       consumer = newnode        </tt>

   Steps 2-4 are performed in bev_cqueue_add().  Because several fields need
   to be updated, simple atomic operations are insufficient.  Thus, the
   transport layer must synchronize calls to bev_cqueue_add() and
   bev_cqueue_get(), because both calls affect the consumer.  Given that
   bev_cqueue_add() completes its three operations before returning, and
   bev_cqueue_add() is called before the new buffer is added to the queue,
   there is no way the producer will try to generate an event and move its
   pointer forward until bev_cqueue_add() completes.  This allows the transport
   layer and core layer to continue interact only using atomic operations.

   A diagrammatic view of these steps is shown below.  The dotted arrows signify
   the pointers before the new node is added.  The Step numbers correspond to
   steps 2-4 above.
   @dot
   digraph {
   {
       rank=same;
       node [shape=plaintext];
       nlx_core_bev_cqueue;
       node [shape=record];
       struct1 [label="<f0> consumer|<f1> producer"];
   }
   newnode [shape=box];
   struct1:f0 -> newnode [label="(4)"];
   node1 -> newnode [label="next (3)"]
   newnode -> node2 [label="next (2)"]
   {
       rank=same;
       ordering=out;
       "element list" [shape=plaintext];
       node1 [shape=box];
       node2 [shape=box];
       node1 -> node2 [style=dotted];
       node2 -> node1 [label=next];
   }
   nlx_core_bev_cqueue -> "element list" [style=invis];
   struct1:f0 -> node1 [style=dotted];
   struct1:f1 -> node2;
   }
   @enddot

   Once again, updating the @c next pointer is less straight forward than the
   diagram suggests.  In step 1, the node is allocated by the transport layer.
   Once allocated, initialization includes the transport layer setting the
   nlx_core_bev_link::cbl_c_self pointer to point at the node and having
   the kernel core layer "bless" the node by setting the
   nlx_core_bev_link::cbl_p_self_loc field.  After the self pointers are set,
   the next fields can be set by using these self fields.  Since allocation
   occurs in the transport address space, the allocation logic uses the
   nlx_core_bev_link::cbl_c_next pointers of the existing nodes for
   navigation, and sets both the @c nlx_core_bev_link::cbl_c_next and
   nlx_core_bev_link::cbl_p_next_loc fields.  The @c cbl_p_next_loc field
   is set by using the @c cbl_c_next->cbl_p_self_loc value, which is treated
   opaquely by the transport layer.  So, steps 2 and 3 update both pairs of
   pointers.  Allocation has no affect on the @c producer reference itself, only
   the @c consumer pointer.

   The resultant 3 element queue looks like this:
   @dot
   digraph {
   {
       rank=same;
       node [shape=plaintext];
       nlx_core_bev_cqueue;
       node [shape=record];
       struct1 [label="<f0> consumer|<f1> producer"];
   }
   {
       rank=same;
       ordering=out;
       node [shape=plaintext];
       "element list";
       node2 [shape=box];
       newnode [shape=box];
       node1 [shape=box];
       "element list" -> newnode [style=invis];
       newnode -> node2 [label=next];
       node2 -> node1 [label=next];
       node1 -> newnode [label=next];
   }
   nlx_core_bev_cqueue -> "element list" [style=invis];
   struct1:f0 -> newnode;
   struct1:f1 -> node2;
   }
   @enddot


   @subsection cqueueDLD-lspec-state State Specification

   The circular queue can be in one of 3 states:
   - empty: This is the initial state and the queue returns to this state
   whenever the count of consumable elements return to zero.
   - full: The queue contains elements and has no room for more. In this state,
   the producer should not attempt to put any more elements into the queue.
   Recall that the consumer "owns" the element that it just consumed, so
   the queue is full when the count of consumable elements is one less than
   the size of the queue. This state can be expressed as
   @code count == (total_number - 1) @endcode
   - partial: In this state, the queue contains elements to be consumed and
   still has room for additional element production. This can be expressed as
   @code count > 0 && count < (total_number - 1) @endcode

   Recall that the @c count is stored as a @c m0_atomic64, so it must
   be access using @c m0_atomic64_get(), requiring the use of a temporary
   variable in the case of testing if the queue is in the partial state.

   @subsection cqueueDLD-lspec-thread Threading and Concurrency Model

   A single producer and consumer are supported.  The variables @c consumer and
   @c producer represent the range of elements in the queue containing data.
   While the @c producer is a compound object, with a single producer, no
   locking is required to access it.  The @c producer cannot safely be accessed
   by the consumer, since they can be in different address spaces, so an atomic
   @c count of consumable elements is used as a surrogate for comparing the
   @c consumer and @c producer.  Multiple producers and/or consumers must
   synchronize externally.

   The transport layer acts both as the consumer and the allocator, and both
   operations use and modify the @c consumer variable and related pointers.  As
   such, calls to bev_cqueue_add() and bev_cqueue_get() must be synchronized.
   The transport layer holds the transfer machine m0_net_transfer_mc::ntm_mutex
   when it calls bev_cqueue_add().  The transport layer will also hold this
   mutex when it calls bev_cqueue_get().

   @subsection cqueueDLD-lspec-numa NUMA optimizations

   None.

   <hr>
   @section cqueueDLD-conformance Conformance

   - @b i.m0.lib.atomic.interoperable-kernel-user-support The
   nlx_core_bev_link data structure allows for tracking the pointers to the
   link in both address spaces.  The atomic operations allow the FIFO to be
   produced and consumed simultaneously in both spaces without synchronization
   or context switches.

   - @b i.net.xprt.lnet.growable-event-queue The implementation supports
   an event queue to which new elements can be added over time.

   <hr>
   @section cqueueDLD-ut Unit Tests

   The following cases will be tested by unit tests:

   @test Initializing a queue of minimum size 2

   @test Successfully producing an element

   @test Successfully consuming an element

   @test Failing to consume an element because the queue is empty

   @test Initializing a queue of larger size

   @test Repeating the producing and consuming tests

   @test Concurrently producing and consuming elements

   <hr>
   @section cqueueDLD-st System Tests

   System testing will include tests where the producer and consumer are
   in separate address spaces.

   <hr>
   @section cqueueDLD-O Analysis

   The circular queue (the struct nlx_core_bev_cqueue) consumes fixed size
   memory, independent of the size of the elements contains the queue's data.
   The number of elements can grow over time, where the number of elements is
   proportional to the number of current and outstanding buffer operations.
   This number of elements will reach some maximum based on the peak activity in
   the application layer.  Operations on the queue are O(1) complexity.

   <hr>
   @section cqueueDLD-ref References

   - <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV779386N
tFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>
   - <a href="http://drdobbs.com/high-performance-computing/210604448">
Writing Lock-Free Code: A Corrected Queue, Herb Sutter, in Dr Dobbs Journal,
2008</a>

 */

/**
   @page cqueueDLD-fspec LNet Buffer Event Queue Functional Specification

   - @ref cqueueDLD-fspec-ds
   - @ref cqueueDLD-fspec-sub
   - @ref cqueueDLD-fspec-usecases
   - @ref bevcqueue "Detailed Functional Specification" <!--
                                      below and ./linux_kernel/kbev_queue.c -->

   @section cqueueDLD-fspec-ds Data Structures

   The circular queue is defined by the nlx_core_bev_cqueue data
   structure.

   @section cqueueDLD-fspec-sub Subroutines

   Subroutines are provided to:
   - initialize and finalize the nlx_core_bev_cqueue
   - produce and consume elements in the queue

   @see @ref bevcqueue "Detailed Functional Specification"

   @section cqueueDLD-fspec-usecases Recipes

   The nlx_core_bev_cqueue provides access to the producer and
   consumer elements in the circular queue.

   In addition, semaphores or other synchronization mechanisms can be used to
   notify the producer or consumer when the queue changes, eg. when it becomes
   not empty.

   @subsection cq-init Initialization

   The circular queue is initialized as follows:

   @code
   struct nlx_core_buffer_event *e1;
   struct nlx_core_buffer_event *e2;
   struct nlx_core_bev_cqueue myqueue;

   NLX_ALLOC_PTR(e1, ...);
   NLX_ALLOC_PTR(e2, ...);
   bev_cqueue_init(&myqueue, &e1->cbe_tm_link, &e2->cbe_tm_link);
   @endcode

   @subsection cq-allocator Allocator

   The event queue can be expanded to make room for additional buffer events.
   This should be performed before buffers are queued.  One element should exist
   on the event queue for each expected buffer operation, plus one additional
   element for the "current" buffer operation.

   @code
   size_t needed;
   struct nlx_core_buffer_event *el;

   ... ; // acquire the lock shared with the consumer
   while (needed > bev_cqueue_size(&myqueue)) {
       NLX_ALLOC_PTR(el, ...);
       ... ; // initialize the new element for both address spaces
       bev_cqueue_add(&myqueue, el);
   }
   ... ; // release the lock shared with the consumer
   @endcode

   @subsection cq-producer Producer

   The (single) producer works in a loop, putting event notifications in the
   queue:

   @code
   bool done;
   struct nlx_core_bev_link *ql;
   struct nlx_core_buffer_event *el;

   while (!done) {
       ql = bev_cqueue_pnext(&myqueue);
       el = container_of(ql, struct nlx_core_buffer_event, cbe_tm_link);
       ... ; // initialize the element
       bev_cqueue_put(&myqueue, ql);
       ... ; // notify blocked consumer that data is available
   }
   @endcode

   @subsection cq-consumer Consumer

   The (single) consumer works in a loop, consuming data from the queue:

   @code
   bool done;
   struct nlx_core_bev_link *ql;
   struct nlx_core_buffer_event *el;

   while (!done) {
       ... ; // acquire the lock shared with the allocator
       ql = bev_cqueue_get(&myqueue);
       if (ql == NULL) {
           ... ; // unlock a lock shared with the allocator
           ... ; // block until data is available
           continue;
       }

       el = container_of(ql, struct nlx_core_buffer_event, cbe_tm_link);
       ... ; // operate on the current element
       ... ; // release the lock shared with the allocator
   }
   @endcode

   @see @ref bevcqueue "Detailed Functional Specification" <!-- below -->
 */

/**
   @defgroup bevcqueue LNet Buffer Event Queue Interface
   @ingroup LNetDFS

   The buffer event FIFO circular queue, used between the LNet Kernel Core
   and LNet transport.

   Unlike the standard m0_queue, this queue supports a producer and consumer in
   different address spaces sharing the queue via shared memory.  No locking is
   required by this single producer or consumer.

   @{
 */

/**
   Buffer event queue invariant.
 */
static bool bev_cqueue_invariant(const struct nlx_core_bev_cqueue *q)
{
	return q != NULL && q->cbcq_consumer != 0 &&
	    q->cbcq_nr >= M0_NET_LNET_BEVQ_MIN_SIZE &&
	    m0_atomic64_get(&q->cbcq_count) < q->cbcq_nr &&
	    !nlx_core_kmem_loc_is_empty(&q->cbcq_producer_loc);
}

/**
   Adds a new element to the circular buffer queue in the consumer address
   space.
   @note The new element must already be blessed via bev_link_bless() in the
   producer address space.  The cbl_c_self of the new element, ql, is set
   by bev_cqueue_add().
   @param q the queue
   @param ql the element to add
   @pre q->cbcq_nr > 0 && q->cbcq_consumer != NULL &&
   nlx_core_kmem_loc_invariant(&ql->cbl_p_self_loc)
 */
static void bev_cqueue_add(struct nlx_core_bev_cqueue *q,
			   struct nlx_core_bev_link *ql)
{
	struct nlx_core_bev_link *consumer =
	    (struct nlx_core_bev_link *) (q->cbcq_consumer);
	M0_PRE(q->cbcq_nr > 0 && consumer != NULL);
	M0_PRE(nlx_core_kmem_loc_invariant(&ql->cbl_p_self_loc));
	ql->cbl_c_self = (nlx_core_opaque_ptr_t) ql;

	ql->cbl_c_next = consumer->cbl_c_next;
	ql->cbl_p_next_loc = consumer->cbl_p_next_loc;
	consumer->cbl_c_next = (nlx_core_opaque_ptr_t) ql;
	consumer->cbl_p_next_loc = ql->cbl_p_self_loc;
	q->cbcq_consumer = (nlx_core_opaque_ptr_t) ql;
	q->cbcq_nr++;

	M0_POST(bev_cqueue_invariant(q));
}

/**
   Initialises the buffer event queue. Should be invoked in the consumer address
   space only.
   @note both elements, ql1 and ql2 must be blessed via bev_link_bless() in
   the producer address space before they are used here.
   @param q buffer event queue to initialise
   @param ql1 the first element in the new queue
   @param ql2 the second element in the new queue
   @pre q != NULL && q->cbcq_nr == 0 && ql1 != NULL && ql2 != NULL
   @post bev_cqueue_invariant(q) && q->cbcq_count == 0
 */
static void bev_cqueue_init(struct nlx_core_bev_cqueue *q,
			    struct nlx_core_bev_link *ql1,
			    struct nlx_core_bev_link *ql2)
{
	M0_PRE(q != NULL && q->cbcq_nr == 0 && ql1 != NULL && ql2 != NULL);
	M0_PRE(nlx_core_kmem_loc_invariant(&ql1->cbl_p_self_loc));
	/* special case: add first element to the circular queue */
	ql1->cbl_c_self = (nlx_core_opaque_ptr_t) ql1;
	ql1->cbl_c_next = (nlx_core_opaque_ptr_t) ql1;
	ql1->cbl_p_next_loc = ql1->cbl_p_self_loc;
	q->cbcq_consumer = (nlx_core_opaque_ptr_t) ql1;
	q->cbcq_producer_loc = ql1->cbl_p_self_loc;
	q->cbcq_nr++;

	bev_cqueue_add(q, ql2);
	m0_atomic64_set(&q->cbcq_count, 0);
	M0_POST(bev_cqueue_invariant(q));
}

/**
   Finalise the buffer event queue.
   Buffer events in the queue are freed using the specified callback.
   @note This operation is to be used only by the consumer.
 */
static void bev_cqueue_fini(struct nlx_core_bev_cqueue *q,
			    void (*free_cb)(struct nlx_core_bev_link *))
{
	struct nlx_core_bev_link *ql;
	struct nlx_core_bev_link *nql = NULL;

	M0_PRE(bev_cqueue_invariant(q));
	M0_PRE(free_cb != NULL);
	for (ql = (struct nlx_core_bev_link *) q->cbcq_consumer;
	     q->cbcq_nr > 0; ql = nql, --q->cbcq_nr) {
		nql = (struct nlx_core_bev_link *) ql->cbl_c_next;
		free_cb(ql);
	}

	q->cbcq_consumer = 0;
}

/**
   Tests if the buffer event queue is empty.
 */
static bool bev_cqueue_is_empty(const struct nlx_core_bev_cqueue *q)
{
	M0_PRE(bev_cqueue_invariant(q));
	return m0_atomic64_get(&q->cbcq_count) == 0;
}

/**
   Returns total size of the event queue, including in-use and free elements.
 */
static size_t bev_cqueue_size(const struct nlx_core_bev_cqueue *q)
{
	return q->cbcq_nr;
}

/**
   Gets the oldest element in the FIFO circular queue, advancing the divider.
   @param q the queue
   @returns the link to the element in the consumer context,
   NULL when the queue is empty
 */
static struct nlx_core_bev_link *bev_cqueue_get(struct nlx_core_bev_cqueue *q)
{
	struct nlx_core_bev_link *link;

	if (bev_cqueue_is_empty(q)) /* also checks invariant */
		return NULL;
	link = (struct nlx_core_bev_link *) q->cbcq_consumer;
	M0_ASSERT(link->cbl_c_next != 0);
	q->cbcq_consumer = (nlx_core_opaque_ptr_t) link->cbl_c_next;
	m0_atomic64_dec(&q->cbcq_count);
	return (struct nlx_core_bev_link *) (q->cbcq_consumer);
}

/**
   @}
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
