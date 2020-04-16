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
 * Original creation date: 11/01/2011
 */

/**
   @page LNetDLD LNet Transport DLD

   - @ref LNetDLD-ovw
   - @ref LNetDLD-def
   - @ref LNetDLD-req
   - @ref LNetDLD-depends
   - @ref LNetDLD-highlights
   - @subpage LNetDLD-fspec "Functional Specification" <!-- ./lnet.h" -->
      - @ref LNetDFS "LNet Transport"                  <!-- net/lnet/lnet.h -->
      - @ref LNetXODFS "XO Interface"                  <!-- ./lnet_xo.h -->
   - @ref LNetDLD-lspec
      - @ref LNetDLD-lspec-comps
      - @ref LNetDLD-lspec-ep
      - @ref LNetDLD-lspec-tm-start
      - @ref LNetDLD-lspec-tm-stop
      - @ref LNetDLD-lspec-bev-sync
      - @ref LNetDLD-lspec-tm-thread
      - @ref LNetDLD-lspec-buf-nbd
      - @ref LNetDLD-lspec-buf-op
      - @ref LNetDLD-lspec-state
      - @ref LNetDLD-lspec-thread
      - @ref LNetDLD-lspec-numa
   - @ref LNetDLD-conformance
   - @ref LNetDLD-ut
   - @ref LNetDLD-st
   - @ref LNetDLD-O
   - @ref LNetDLD-ref

   <hr>
   @section LNetDLD-ovw Overview
   This document describes the Mero network transport for LNet. The
   transport is composed of multiple layers.  The document describes the
   layering and then focuses mainly on the transport operations layer.

   The design of the other layers can be found here:
   - @subpage LNetCoreDLD-fspec "LNet Transport Core API"
     <!-- ./lnet_core.h -->
   - @ref LNetcqueueDLD "LNet Buffer Event Circular Queue DLD"
     <!-- ./bev_cqueue.c -->
   - @ref KLNetCoreDLD "LNet Transport Kernel Core DLD"
     <!-- ./linux_kernel/klnet_core.c -->
   - @ref ULNetCoreDLD "LNet Transport User Space Core DLD"
     <!-- ./ulnet_core.c -->
   - @ref LNetDRVDLD "LNet Transport Device DLD"
     <!-- ./linux_kernel/klnet_drv.c -->

   <hr>
   @section LNetDLD-def Definitions
   Refer to <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV
779386NtFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>

   <hr>
   @section LNetDLD-req Requirements
   - @b r.m0.net.xprt.lnet.transport-variable The implementation
     shall name the transport variable as specified in the HLD.

   - @b r.m0.net.lnet.buffer-registration Provide support for
     hardware optimization through buffer pre-registration.

   - @b r.m0.net.xprt.lnet.end-point-address The implementation
     should support the mapping of end point address to LNet address
     as described in the Refinement section of the HLD.

   - @b r.m0.net.xprt.lnet.multiple-messages-in-buffer Provide
     support for this feature as described in the HLD.

   - @b r.m0.net.xprt.lnet.dynamic-address-assignment Provide
     support for dynamic address assignment as described in the HLD.

   - @b r.m0.net.xprt.lnet.processor-affinity The implementation
     must support processor affinity as described in the HLD.

   - @b r.m0.net.xprt.lnet.user-space The implementation must
     accommodate the needs of the user space LNet transport.

   - @b r.m0.net.synchronous-buffer-event-delivery The implementation must
     provide support for this feature as described in the HLD.

   <hr>
   @section LNetDLD-depends Dependencies
   <ul>

   <li>@ref LNetCore "LNet Transport Core Interface" <!-- ./lnet_core.h -->
   </li>

   <li>The @ref net "Networking Module". <!-- net/net.h -->
   Some modifications are required:

     The design adds two additional fields to the m0_net_buffer structure:
     @code
     struct m0_net_buffer {
        ...
	m0_bcount_t   nb_min_receive_size;
	uint32_t      nb_max_receive_msgs;
     };
     @endcode
     These fields are required to be set to non-zero values in receive buffers,
     and control the reception of multiple messages into a single receive
     buffer.

     Additionally, the semantics of the @c nb_ep field is modified to not
     require the end point of the active transfer machine when enqueuing a
     passive buffer.  This effectively says that there will be no constraint on
     which transfer machine performs the active operation, and the application
     with the passive buffer is not required to know the address of this active
     transfer machine in advance. This enables the conveyance of the network
     buffer descriptor to the active transfer machine through intermediate
     proxies, and the use of load balancing algorithms to spread the I/O
     traffic across multiple servers.

     The m0_net_tm_confine() subroutine is added to set the processor
     affinity for transfer machine thread if desired.  This results in an
     additional operation being added to the m0_net_xprt_ops structure:

     @code
     struct m0_net_xprt_ops {
        ...
        int  (*xo_tm_confine)(struct m0_net_transfer_mc *tm,
	                      const struct m0_bitmap *processors);
     };
     @endcode

     The behavior of the m0_net_buffer_event_post() subroutine is modified
     slightly to allow for multiple buffer events to be delivered for a single
     receive buffer, without removing it from a transfer machine queue.
     This is indicated by the ::M0_NET_BUF_RETAIN flag.

     The design adds the following fields to the m0_net_transfer_mc structure
     to support the synchronous delivery of network buffer events:
     @code
     struct m0_net_transfer_mc {
        ...
	bool                        ntm_bev_auto_deliver;
     };
     @endcode
     By default, @c ntm_bev_auto_deliver is set to @c true.  In addition
     the following subroutines are defined:
     - m0_net_buffer_event_deliver_all()
     - m0_net_buffer_event_deliver_synchronously()
     - m0_net_buffer_event_pending()
     - m0_net_buffer_event_notify()
     .
     This results in corresponding operations being added to the
     m0_net_xprt_ops structure:
     @code
     struct m0_net_xprt_ops {
        ...
        void (*xo_bev_deliver_all)(struct m0_net_transfer_mc *tm);
        int  (*xo_bev_deliver_sync)(struct m0_net_transfer_mc *tm);
	bool (*xo_bev_pending)(struct m0_net_transfer_mc *tm);
	void (*xo_bev_notify)(struct m0_net_transfer_mc *tm,
                              struct m0_chan *chan);
     };
     @endcode

   </li> <!-- end net module changes -->

   <li>The @ref bitmap "Bitmap Module". <!-- lib/bitmap.h -->
   New subroutines to copy a bitmap and to compare bitmaps are required. The
   copy subroutine should be refactored out of the processors_copy_m0bitmap()
   subroutine. </li>

   <li>The @ref Processor <!-- lib/processor.h -->
   API for the application to determine processor bitmaps with which to specify
   thread affinity.</li>

   <li>The @ref thread "Thread Module". <!-- lib/thread.h -->
   Modifications are required in m0_thread_init() subroutine or a variant
   should be provided to support thread creation with processor affinity set.
   This is essential for the kernel implementation where processor affinity can
   only be set during thread creation.</li>

   </ul>

   <hr>
   @section LNetDLD-highlights Design Highlights
   - Common user and kernel space implementation over an underlying "Core" I/O
     layer that communicates with the kernel LNet module.
   - Supports the reception of multiple messages in a single receive buffer.
   - Provides processor affinity.
   - Support for hardware optimizations in buffer access.
   - Support for dynamic transfer machine identifier assignment.
   - Efficient communication between user and kernel address spaces in the user
     space transport through the use of shared memory.  This includes the
     efficient conveyance of buffer operation completion event data through the
     use of a circular queue in shared memory, and the minimal use of system
     calls to block for events.

   <hr>
   @section LNetDLD-lspec Logical Specification
   - @ref LNetDLD-lspec-comps
   - @ref LNetDLD-lspec-ep
   - @ref LNetDLD-lspec-tm-start
   - @ref LNetDLD-lspec-tm-stop
   - @ref LNetDLD-lspec-bev-sync
   - @ref LNetDLD-lspec-tm-thread
   - @ref LNetDLD-lspec-buf-nbd
   - @ref LNetDLD-lspec-buf-op
   - @ref LNetDLD-lspec-state
   - @ref LNetDLD-lspec-thread
   - @ref LNetDLD-lspec-numa

   @subsection LNetDLD-lspec-comps Component Overview
   The focus of the LNet transport is the implementation of the asynchronous
   semantics required by the Mero Networking layer.  I/O is performed by an
   underlying "core" layer, which does the actual interaction with the Lustre
   LNet kernel module.  The core layer permits the LNet transport code to be
   written in an address space agnostic fashion, as it offers the same
   interface in both user and kernel space.

   The relationship between the various components of the LNet transport and
   the networking layer is illustrated in the following UML diagram.
   @image html "../../net/lnet/lnet_xo.png" "LNet Transport Objects"
   <!-- PNG image width is 800 -->

   @subsection LNetDLD-lspec-ep End Point Support
   The transport defines the following structure for the internal
   representation of a struct m0_net_end_point.
   @code
   struct nlx_xo_ep {
       uint64_t                xe_magic;
       struct m0_net_end_point xe_ep;
       struct nlx_core_ep_addr xe_core;
       char                    xe_addr[M0_NET_LNET_XEP_ADDR_LEN];
   };
   @endcode
   The length of the structure depends on the length of the string
   representation of the address, which must be saved in the @c xe_addr array.
   The address of the @c xe_ep field is returned as the external representation.

   The end point data structure is not associated internally with any LNet
   kernel resources.

   The transport does not support dynamic addressing: i.e. the @c addr
   parameter can never be NULL in the m0_net_end_point_create() subroutine.
   However, it supports the dynamic assignment of transfer machine identifiers
   as described in the HLD, but only for the @c addr parameter of the
   m0_net_tm_start() subroutine.

   A linked list of all end point objects created for a transfer machine is
   maintained in the m0_net_transfer_mc::ntm_end_points list.  Objects are
   added to this list as a result of the application invoking the
   m0_net_end_point_create() subroutine, or as a side effect of receiving a
   message.  Access to this list is protected by the transfer machine mutex.


   @subsection LNetDLD-lspec-tm-start Transfer Machine Start
   The m0_net_tm_start() subroutine is used to start a transfer machine, which
   results in a call to nlx_xo_tm_start().  The subroutine decodes the end
   point address using the nlx_core_ep_addr_decode() subroutine. It then starts
   the background event processing thread with the desired processor
   affinity. The thread will complete the transfer machine start up and deliver
   its state change event.

   The event processing thread will call the nlx_core_tm_start() subroutine to
   create the internal LNet EQ associated with the transfer machine.  This call
   also validates the transfer machine's address, and assigns a dynamic
   transfer machine identifier if needed.  It will then post a state change
   callback to transition the transfer machine to its normal operational state,
   or fail it if any error is encountered.


   @subsection LNetDLD-lspec-tm-stop Transfer Machine Termination
   Termination of a transfer machine is requested through the m0_net_tm_stop()
   subroutine, which results in a call to nlx_xo_tm_stop(). The latter ensures
   that the transfer machine's thread wakes up by signaling on the
   nlx_xo_transfer_mc::xtm_ev_cond condition variable.

   When terminating a transfer machine the application has a choice of draining
   current operations or aborting such activity.  If the latter choice is made,
   then the transport must first cancel all operations.

   Regardless, the transfer machine's event processing thread completes the
   termination process.  It waits until all buffer queues are empty and any
   ongoing synchronous network buffer delivery has completed, then invokes the
   nlx_core_tm_stop() subroutine to free the LNet EQ and other resources
   associated with the transfer machine.  It then posts the transfer machine
   state change event and terminates itself.  See @ref LNetDLD-lspec-tm-thread
   for further detail.

   @subsection LNetDLD-lspec-bev-sync Synchronous Network Buffer Event Delivery

   The transport supports the optional synchronous network buffer event
   delivery as required by the HLD.  The default asynchronous delivery of
   buffer events is done by the @ref LNetDLD-lspec-tm-thread.  Synchronous
   delivery must be enabled before the transfer machine is started, and is
   indicated by the value of the m0_net_transfer_mc::ntm_bev_auto_deliver
   value being @c false.

   The nlx_xo_bev_deliver_sync() transport operation is invoked to disable the
   automatic delivery of buffer events. The subroutine simply returns without
   error, and the invoking m0_net_buffer_event_deliver_synchronously()
   subroutine will then set the value of
   m0_net_transfer_mc::ntm_bev_auto_deliver value to @c false.

   The nlx_xo_bev_pending() transport operation is invoked from the
   m0_net_buffer_event_pending() subroutine to determine if there are pending
   network buffer events.  It invokes the nlx_core_buf_event_wait() subroutine
   with a timeout of 0 and uses the returned status value to determine if
   events are present or not.

   The nlx_xo_bev_notify() transport operation is invoked from the
   m0_net_buffer_event_notify() subroutine.  It sets the
   nlx_xo_transfer_mc::xtm_ev_chan value to the specified wait channel, and
   signals on the nlx_xo_transfer_mc::xtm_ev_cond condition variable to wake up
   the event processing thread.

   The nlx_xo_bev_deliver_all() transport operation is invoked from the
   m0_net_buffer_event_deliver_all() subroutine.  It attempts to deliver all
   pending events.  The transfer machine lock is held across the call to the
   nlx_core_buf_event_get() subroutine to serialize "consumers" of the
   circular buffer event queue, but is released during event delivery.  The
   m0_net_transfer_mc::ntm_callback_counter field is incremented across the
   call to prevent premature termination when operating outside of the
   protection of the transfer machine mutex.  This is illustrated in the
   following pseudo-code for nlx_xo_bev_deliver_all():
   @code
   int rc;
   bool delivered_events = false;
   M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
   tm->ntm_callback_counter++;
   do { // consume all pending events
        struct nlx_core_buffer_event lcbe;
	struct m0_net_buffer_event nbev;
	rc = nlx_core_buf_event_get(lctm, &lcbe);
	if (rc == 0) {
	  // create end point objects as needed
        }
	m0_mutex_unlock(&tm->ntm_mutex); // release lock
	if (rc == 0) {
	     nbe = ... // convert the event
	     m0_net_buffer_event_post(&nbev);
	     delivered_events = true;
        }
	m0_mutex_lock(&tm->ntm_mutex); // re-acquire lock
   } while (rc == 0);
   tm->ntm_callback_counter--;
   if (delivered_events)
        m0_chan_broadcast(&tm->ntm_chan);
   @endcode

   @subsection LNetDLD-lspec-tm-thread Transfer Machine Event Processing Thread
   The default behavior of a transfer machine is to automatically deliver
   buffer events from the Core API's event queue to the application.  The Core
   API guarantees that LNet operation completion events will result in buffer
   events being enqueued in the order the API receives them, and, in
   particular, that multiple buffer events for any given receive buffer will be
   ordered.  This is very important for the transport, because it has to ensure
   that a receive buffer operation is not prematurely flagged as dequeued.

   The transport uses exactly one event processing thread to process buffer
   events from the Core API.  This has the following advantages:
   - The implementation is simple.
   - It implicitly race-free with respect to receive buffer events.

   Applications are not expected to spend much time in the event callback, so
   this simple approach is acceptable.

   The application can establish specific processor affiliation for the event
   processing thread with the m0_net_tm_confine() subroutine @em prior to
   starting the transfer machine. This results in a call to the
   nlx_xo_tm_confine() subroutine, which makes a copy of the desired processor
   affinity bitmask in nlx_xo_transfer_mc::xtm_processors.

   In addition to automatic buffer event delivery, the event processing thread
   performs the following functions:
   - Notify the presence of buffer events when synchronous buffer event
     delivery is enabled
   - Transfer machine state change event posting
   - Buffer operation timeout processing
   - Logging of statistical data

   The functionality of the event processing thread is best illustrated by
   the following pseudo-code:
   @code
   // start the transfer machine in the Core
   rc = nlx_core_tm_start(&tm, lctm);
   if (rc == 0)
       rc = nlx_ep_create(&tmev.nte_ep, tm, &lctm->ctm_addr);
   // deliver a M0_NET_TEV_STATE_CHANGE event to transition the TM to
   // the M0_NET_TM_STARTED or M0_NET_TM_FAILED states
   // Set the transfer machine's end point on success
   m0_net_tm_event_post(&tmev);
   if (rc != 0)
       return; // failure
   // loop forever
   while (1) {
      timeout = ...; // compute next timeout (short if automatic or stopping)
      if (tm->ntm_bev_auto_deliver) {      // automatic delivery
	  rc = nlx_core_buf_event_wait(lctm, timeout);
	  // buffer event processing
	  if (rc == 0) { // did not time out - events pending
	     m0_mutex_lock(&tm->ntm_mutex);
	     nlx_xo_bev_deliver_all(tm);
	     m0_mutex_unlock(&tm->ntm_mutex);
	  }
      } else {                             // application initiated delivery
	     m0_mutex_lock(&tm->ntm_mutex);
	     if (lctm.xtm_ev_chan == NULL)
	        m0_cond_timedwait(lctm->xtm_ev_cond, timeout);
	     if (lctm.xtm_ev_chan != NULL) {
	        rc = nlx_core_buf_event_wait(lctm, timeout);
	        if (rc == 0) {
		    m0_chan_signal(lctm->xtm_chan);
		    lctm.xtm_chan = NULL;
		}
             }
	     m0_mutex_unlock(&tm->ntm_mutex);
      }
      // do buffer operation timeout processing periodically
      ...
      // termination processing
      if (tm->ntm_state == M0_NET_TM_STOPPING) {
            bool must_stop = false;
            m0_mutex_lock(&tm->ntm_mutex);
            if (all_tm_queues_are_empty(tm) && tm->ntm_callback_counter == 0) {
	       nlx_core_tm_stop(lctm);
	       must_stop = true;
            }
            m0_mutex_unlock(&tm->ntm_mutex);
            if (must_stop) {
	       struct m0_net_tm_event tmev;
               // construct a M0_NET_TEV_STATE_CHANGE event to transition
	       // to the M0_NET_TM_STOPPED state.
	       m0_net_tm_event_post(&tmev);
               break;
            }
      }
      // Log statistical data periodically using ADDB
      ...
   }
   @endcode
   (The C++ style comments above are used only because the example is
   embedded in a Doxygen C comment.  C++ comments are not permitted by the
   Mero coding style.)

   A few points to note on the above pseudo-code:
   - The thread blocks in the nlx_core_buf_event_wait() if the default
     automatic buffer event delivery mode is set, or on the
     nlx_xo_transfer_mc::xtm_ev_cond condition variable otherwise. In the
     latter case, it may also block in the nlx_core_buf_event_wait() subroutine
     if the condition variable is signaled by the nlx_xo_bev_notify()
     subroutine.
   - The transfer machine mutex is obtained across the call to dequeue buffer
     events to serialize with the "other" consumer of the buffer event queue,
     the nlx_xo_buf_add() subroutine that invokes the Core API buffer operation
     initiation subroutines.  This is because these subroutines may allocate
     additional buffer event structures to the queue.
   - The nlx_xo_bev_deliver_all() subroutine processes as many events as it can
     each time around the loop.  The call to the nlx_core_buf_event_wait()
     subroutine in the user space transport is expensive as it makes a device
     driver @c ioctl call internally.
   - The thread is responsible for terminating the transfer machine and
     delivering its termination event.  Termination serializes with concurrent
     invocation of the nlx_xo_bev_deliver_all() subroutine in the case of
     synchronous buffer event delivery.
   - The timeout value can vary depending on the mode of operation. Synchronous
     network delivery is best served by a long timeout value (in the order of a
     minute), at least up to the time that the transfer machine is stopping.
     Automatic buffer event delivery is better served by a short timeout value
     (in the order of a second).  This is because in the user space transport
     the thread would be blocked in an ioctl call in the kernel, and would not
     respond in a lively manner to a shutdown request.  The timeout value is
     also dependent on whether the transfer machine is stopping, the buffer
     operation timeout check period and the statistical recording period.

   @subsection LNetDLD-lspec-buf-nbd Network Buffer Descriptor
   The transport has to define the format of the opaque network buffer
   descriptor returned to the application, to encode the identity of the
   passive buffers.

   The data structure will be defined by the Core API along the following lines:
   @code
   struct nlx_core_buf_desc {
        uint64_t                 cbd_match_bits;
        struct nlx_core_ep_addr  cbd_passive_ep;
        enum m0_net_queue_type   cbd_qtype;
        m0_bcount_t              cbd_size;
   };
   @endcode

   The nlx_core_buf_desc_encode() and nlx_core_buf_desc_decode() subroutines
   are provided by the Core API to generate and process the descriptor.  All
   the descriptor fields are integers, the structure is of fixed length and all
   values are in little-endian format.  No use is made of either the standard
   XDR or the Mero Xcode modules.

   The transport will handle the conversion of the descriptor into its opaque
   over the wire format by simply copying the memory area, as the descriptor is
   inherently portable.


   @subsection LNetDLD-lspec-buf-op Buffer operations
   Buffer operations are initiated through the m0_net_xprt_ops::xo_buf_add()
   operation which points to the nlx_xo_buf_add() subroutine. The subroutine
   will invoke the appropriate Core API buffer initiation operations.

   In passive bulk buffer operations, the transport must first obtain suitable
   match bits for the buffer using the nlx_core_buf_desc_encode() subroutine.
   The transport is responsible for ensuring that the assigned match bits are
   not in use currently; however this step can be ignored with relative safety
   as the match bit space is very large and the match bit counter will only
   wrap around after a very long while.  These match bits should also be
   encoded in the network buffer descriptor that the transport must return.

   In active bulk buffer operations, the size of the active buffer should be
   validated against the size of the passive buffer as given in its network
   buffer descriptor.  The nlx_core_buf_desc_decode() subroutine should be used
   to decode the descriptor.


   @subsection LNetDLD-lspec-state State Specification
   The transport does not introduce its own state model but operates within the
   framework defined by the Mero Networking Module. In general, resources
   are allocated to objects of this module by the underlying Core API, and they
   have to be recovered upon object finalization, and in the particular case of
   the user space transport, upon process termination if the termination was
   not orderly.

   The resources allocated to the following objects are particularly called
   out:

   - m0_net_buffer
   - m0_net_transfer_mc
   - m0_net_domain
   - m0_net_end_point

   Network buffers enqueued with a transfer machine represent operations in
   progress.  Until they get dequeued, the buffers are associated internally
   with LNet kernel module resources (MDs and MEs) allocated on their behalf by
   the Core API.

   The transfer machine is associated with an LNet event queue (EQ).  The EQ
   must be created when the transfer machine is started, and destroyed when the
   transfer machine stops.  The transfer machine operates by default in an
   asynchronous network buffer delivery mode, but can also provide synchronous
   network buffer delivery for locality sensitive applications like the Mero
   request handler.

   Buffers registered with a domain object are potentially associated with LNet
   kernel module resources and, if the transport is in user space, additional
   kernel memory resources as the buffer vector is pinned in
   memory. De-registration of the buffers releases these resources.  The domain
   object of a user space transport is also associated with an open file
   descriptor to the device driver used to communicate with the kernel Core
   API.

   End point structures are exposed externally as struct m0_net_end_point, but
   are allocated and managed internally by the transport with struct nlx_xo_ep.
   They do not use LNet resources, but just transport address space
   memory. Their creation and finalization is protected by the transfer machine
   mutex. They are reference counted, and the application must release all
   references before attempting to finalize a transfer machine.


   @subsection LNetDLD-lspec-thread Threading and Concurrency Model
   The transport inherits the concurrency model of the Mero Networking
   Module. All transport operations are protected by some lock or object state,
   as described in the <a href="https://docs.google.com/a/seagate.com/document/d
/1pDOQXWDZ9t9XDcyXsx4T_aGjFvsyjjvN1ygOtfoXcFg/view">RPC Bulk Transfer Task Plan
</a>.
   The Core API is designed to work with this same locking model.
   The locking order figure is repeated here for convenience:
   @dot
   digraph {
      node [shape=plaintext];
      subgraph cluster_m1 { // represents mutex scope
         // sorted R-L so put mutex name last to align on the left
         rank = same;
	 n1_2 [label="dom_fini()"];  // procedure using mutex
	 n1_1 [label="dom_init()"];
         n1_0 [label="m0_net_mutex"];// mutex name
      }
      subgraph cluster_m2 {
         rank = same;
	 n2_2 [label="tm_fini()"];
         n2_1 [label="tm_init()"];
         n2_4 [label="buf_deregister()"];
	 n2_3 [label="buf_register()"];
         n2_0 [label="nd_mutex"];
      }
      subgraph cluster_m3 {
         rank = same;
	 n3_2 [label="tm_stop()"];
         n3_1 [label="tm_start()"];
	 n3_6 [label="ep_put()"];
	 n3_5 [label="ep_create()"];
	 n3_4 [label="buf_del()"];
	 n3_3 [label="buf_add()"];
         n3_0 [label="ntm_mutex"];
      }
      label="Mutex usage and locking order in the Network Layer";
      n1_0 -> n2_0;  // locking order
      n2_0 -> n3_0;
   }
   @enddot

   The transport only has one thread, its event processing thread.  This thread
   uses the transfer machine lock when serialization is required by the Core
   API, and also when creating or looking up end point objects when processing
   receive buffer events.  Termination of the transfer machine is serialized
   with concurrent invocation of the nlx_xo_bev_deliver_all() subroutine in the
   case of synchronous buffer event delivery by means of the
   m0_net_transfer_mc::ntm_callback_counter field.
   See @ref LNetDLD-lspec-bev-sync and @ref LNetDLD-lspec-tm-thread for
   details.

   @subsection LNetDLD-lspec-numa NUMA optimizations
   The application can establish specific processor affiliation for the event
   processing thread with the m0_net_tm_confine() subroutine prior to starting
   the transfer machine.  Buffer completion events and transfer machine state
   change events will be delivered through callbacks made from this thread.

   Even greater locality of reference is obtained with synchronous network
   buffer event delivery.  The application is able to co-ordinate references to
   network objects and other objects beyond the scope of the network module.

   <hr>
   @section LNetDLD-conformance Conformance
   - @b i.m0.net.xprt.lnet.transport-variable The transport variable
   @c m0_net_lnet_xprt is provided.

   - @b i.m0.net.lnet.buffer-registration Buffer registration is required
   in the network API and results in the corresponding nlx_xo_buf_register()
   subroutine call at the LNet transport layer.  This is where hardware
   optimization can be performed, once LNet provides such APIs.

   - @b i.m0.net.xprt.lnet.end-point-address Mapping of LNet end point
   address is handled in the Core API as described in the @ref
   LNetCoreDLD-fspec "LNet Transport Core API".

   - @b i.m0.net.xprt.lnet.multiple-messages-in-buffer Fields are provided
   in the m0_net_buffer to support multiple message delivery, and the event
   delivery model includes the delivery of buffer events for receive buffers
   that do not always dequeue the buffer.

   - @b i.m0.net.xprt.lnet.dynamic-address-assignment Dynamic transfer
     machine identifier assignment is provided by nlx_core_tm_start().

   - @b i.m0.net.xprt.lnet.processor-affinity The m0_net_tm_confine() API
   is provided and the LNet transport provides the corresponding
   nlx_xo_tm_confine() function.

   - @b i.m0.net.xprt.lnet.user-space The user space implementation of the
   Core API utilizes shared memory and reduces context switches required for
   user-space event processing through the use of a circular queue maintained
   in shared memory and operated upon with atomic operations.

   - @b i.m0.net.synchronous-buffer-event-delivery See @ref
   LNetDLD-lspec-bev-sync and @ref LNetDLD-lspec-tm-thread for details.

   <hr>
   @section LNetDLD-ut Unit Tests
   To control symbol exposure, the transport code is compiled using a single C
   file that includes other C files with static symbols.  Unit testing will
   take advantage of this setup and use conditional renaming of symbols to
   intercept specific internal interfaces.

   The following tests will be performed for the transport operation (xo) layer
   with a fake Core API.  Tests involving the fake Core API ensure that the
   transport operation layer makes the correct calls to the Core API.

   @test Multiple domain creation will be tested.

   @test Buffer registration and deregistration will be tested.

   @test Multiple transfer machine creation will be tested.

   @test Test that the processor affinity bitmask is set in the TM.

   @test The transfer machine state change functionality.

   @test Initiation of buffer operations will be tested.

   @test Delivery of synthetic buffer events will be tested, including multiple
         receive buffer events for a single receive buffer. Both asynchronous
         and synchronous styles of buffer delivery will be tested.

   @test Management of the reference counted end point objects; the addresses
         themselves don't have to valid for these tests.

   @test Encoding and Decoding of the network buffer descriptor will be tested.

   @test Orderly finalization will be tested.

   <hr>
   @section LNetDLD-st System Tests
   @test The @c bulkping system test program will be updated to include support
   for the LNet transport.  This program will be used to test communication
   between end points on the same system and between remote systems.  The
   program will offer the ability to dynamically allocate a transfer machine
   identifier when operating in client mode.

   <hr>
   @section LNetDLD-O Analysis
   In general, the transport operational layer simply routes data too and from
   the Core API; this behavior is analyzed in
   @ref KLNetCoreDLD "LNet Transport Kernel Core DLD".
   <!-- ./linux_kernel/klnet_core.c -->

   An area of concern specific to the transport operations layer is the
   management of end point objects.  In particular, the time taken to search
   the list of end point objects is of O(N) - i.e. a linear search through the
   list, which is proportional to the number of list items.  This search may
   become expensive if the list grows large - items on the list are reference
   counted and it is up to the application to release them, not the transport.

   The internal end point address fields are all numeric and easily lend
   themselves to a hash based strategy (the NID value is the best candidate
   key).  The tricky part of any hashing scheme would be to determine what hash
   function would result in a reasonably even distribution over a set of hash
   buckets; this is not as bad as it sounds, because even in the worst case, it
   would degenerate to the linear search we have at present.

   Ultimately, the choice of whether to use hashing or not depends on what the
   behavior of a Mero server will be like in steady state: if end points are
   released fairly rapidly, the linked list implementation will suffice.  Note
   that since no LNet kernel resources are associated with end point objects,
   this issue is simply related to search performance.


   <hr>
   @section LNetDLD-ref References
   - <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV779386N
tFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>
   - <a href="https://docs.google.com/a/seagate.com/document/d/1pDOQXWDZ9t9XDcyX
sx4T_aGjFvsyjjvN1ygOtfoXcFg/view">RPC Bulk Transfer Task Plan</a>
   - @subpage LNetcqueueDLD "LNet Buffer Event Circular Queue DLD" <!--
     ./bev_cqueue.c -->
   - @subpage KLNetCoreDLD "LNet Transport Kernel Core DLD" <!--
     ./linux_kernel/klnet_core.c -->
   - @subpage ULNetCoreDLD "LNet Transport User Space Core DLD" <!--
     ./ulnet_core.c -->
   - @subpage LNetDRVDLD "LNet Transport Device DLD" <!--
     ./linux_kernel/klnet_drv.c -->

 */

/*
 ******************************************************************************
 End of DLD
 ******************************************************************************
 */

#ifdef __KERNEL__
/* lustre config defines package macros also defined by m0 config */
#undef PACKAGE             /* to avoid conflicts */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION
#include "lustre_config.h" /* required by lnet/types.h */
#include "libcfs/libcfs.h" /* lnet/types.h fails if this is not included */

/* LUSTRE_VERSION_CODE, OBD_OCD_VERSION */
#if M0_LUSTRE_VERSION < 2110
#include <lustre_ver.h>
#else
#include <lustre/lustre_ver.h>
#include <lustre/lustre_idl.h>
#endif

#include <lustre/lustre_idl.h>  /* OBD_OCD_VERSION */
#if LUSTRE_VERSION_CODE > OBD_OCD_VERSION(2, 10, 51, 0)
#include "lnet/lib-types.h"
#else
#include "lnet/types.h"
#endif

#undef PACKAGE             /* suppress lustre specific values */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION
/*
 * Above values were lustre specific, change them again to mero specific.
 * since there are no guard macros around, this will not be a problem.
 */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LNET
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "net/net_internal.h"
#include "net/lnet/lnet_core.h"
#include "net/lnet/lnet_xo.h"
#include "net/lnet/lnet_pvt.h"
#include "mero/magic.h"

#include <asm/byteorder.h>  /* byte swapping macros */

/* debug print support */
#ifdef __KERNEL__
#define NLX_DEBUG
#else
#undef NLX_DEBUG
#endif

#ifdef NLX_DEBUG

struct nlx_debug {
	int _debug_;
};
static struct nlx_debug nlx_debug = {
	._debug_ = 0,
}; /* global debug control */

/* note Linux uses the LP64 standard */
#define NLXP(fmt, ...) M0_LOG(M0_DEBUG, fmt, ## __VA_ARGS__)

#define NLXDBG(ptr, dbg, stmt)						\
do {									\
	if ((ptr)->_debug_ >= (dbg)) {					\
		M0_LOG(M0_DEBUG, "%s: %d:\n", (char*) __FILE__, __LINE__);\
		stmt;							\
	}								\
} while (0)

#define NLXDBGnl(ptr, dbg, stmt)		\
do {						\
	if ((ptr)->_debug_ >= (dbg)) {		\
		stmt;				\
	}					\
} while (0)

#define NLXDBGP(ptr, dbg, fmt, ...)				\
do {								\
	if ((ptr)->_debug_ >= (dbg)) {				\
		M0_LOG(M0_DEBUG, fmt, ## __VA_ARGS__);		\
	}							\
} while (0)

#define NLXDBGPnl(ptr, dbg, fmt, ...)			\
do {							\
	if ((ptr)->_debug_ >= (dbg)) {			\
		M0_LOG(M0_DEBUG, fmt, ## __VA_ARGS__);	\
	}						\
} while (0)

#else /* !NLX_DEBUG */

#define NLXP(fmt, ...)
#define NLXDBG(ptr, dbg, stmt) do { ; } while (0)
#define NLXDBGnl(ptr, dbg, stmt) do { ; } while (0)
#define NLXDBGP(ptr, dbg, fmt, ...) do { ; } while (0)
#define NLXDBGPnl(ptr, dbg, fmt, ...) do { ; } while (0)

#endif /* !NLX_DEBUG */

/*
  To reduce global symbols, yet make the code readable, we
  include other .c files with static symbols into this file.

  Static functions should be declared in the private header file
  so that the order of their definition does not matter.
 */
#include "net/lnet/bev_cqueue.c"
#include "net/lnet/lnet_core.c"
#include "net/lnet/lnet_ioctl.h"
#ifdef __KERNEL__
#include "net/lnet/linux_kernel/klnet_drv.h"
#include "net/lnet/linux_kernel/klnet_core.h"
#include "net/lnet/linux_kernel/kbev_cqueue.c"
#include "net/lnet/linux_kernel/klnet_core.c"
#include "net/lnet/linux_kernel/klnet_drv.c"
/*
 * LUSTRE_SRV_LNET_PID is not available in user space, so assert that the
 * M0 equivalent is valid. Depending on the lustre version, the name may be
 * LUSTRE_SRV_LNET_PID or LNET_PID_LUSTRE.
 */
#ifdef LNET_PID_LUSTRE
M0_BASSERT(M0_NET_LNET_PID == LNET_PID_LUSTRE);
#else
M0_BASSERT(M0_NET_LNET_PID == LUSTRE_SRV_LNET_PID);
#endif
#else
#include "net/lnet/ulnet_core.h"
#include "net/lnet/ulnet_core.c"
#endif
#include "net/lnet/lnet_xo.c"
#include "net/lnet/lnet_ep.c"
#include "net/lnet/lnet_tm.c"

/**
   @addtogroup LNetDFS
   @{
 */

M0_INTERNAL int m0_net_lnet_init(void)
{
	return M0_RC(nlx_core_init());
}

M0_INTERNAL void m0_net_lnet_fini(void)
{
	nlx_core_fini();
}

M0_INTERNAL int m0_net_lnet_ep_addr_net_cmp(const char *addr1,
					    const char *addr2)
{
	const char *cp1;
	const char *cp2;

	cp1 = strchr(addr1, ':');
	cp2 = strchr(addr2, ':');
	if (cp1 == NULL || cp2 == NULL)
		return -1;
	return strncmp(addr1, addr2, min32(cp1 - addr1, cp2 - addr2));
}
M0_EXPORTED(m0_net_lnet_ep_addr_net_cmp);

M0_INTERNAL int m0_net_lnet_ifaces_get(struct m0_net_domain *dom,
				       char *const **addrs)
{
	struct nlx_xo_domain *dp;

	M0_PRE(nlx_dom_invariant(dom));
	dp = dom->nd_xprt_private;
	return nlx_core_nidstrs_get(&dp->xd_core, addrs);
}
M0_EXPORTED(m0_net_lnet_ifaces_get);

M0_INTERNAL void m0_net_lnet_ifaces_put(struct m0_net_domain *dom,
					char *const **addrs)
{
	struct nlx_xo_domain *dp;

	M0_PRE(nlx_dom_invariant(dom));
	dp = dom->nd_xprt_private;
	nlx_core_nidstrs_put(&dp->xd_core, addrs);
}
M0_EXPORTED(m0_net_lnet_ifaces_put);

M0_INTERNAL void m0_net_lnet_dom_set_debug(struct m0_net_domain *dom,
					   unsigned dbg)
{
	struct nlx_xo_domain *dp;

	M0_PRE(dom != NULL);
	m0_mutex_lock(&m0_net_mutex);
	M0_PRE(nlx_dom_invariant(dom));
	dp = dom->nd_xprt_private;
	dp->_debug_ = dbg;
	nlx_core_dom_set_debug(&dp->xd_core, dbg);
	m0_mutex_unlock(&m0_net_mutex);
}
M0_EXPORTED(m0_net_lnet_dom_set_debug);

M0_INTERNAL void m0_net_lnet_tm_set_debug(struct m0_net_transfer_mc *tm,
					  unsigned dbg)
{
	struct nlx_xo_transfer_mc *tp;

	M0_PRE(tm != NULL);
	m0_mutex_lock(&tm->ntm_mutex);
	M0_PRE(nlx_tm_invariant(tm));
	tp = tm->ntm_xprt_private;
	tp->_debug_ = dbg;
	nlx_core_tm_set_debug(&tp->xtm_core, dbg);
	m0_mutex_unlock(&tm->ntm_mutex);
}
M0_EXPORTED(m0_net_lnet_tm_set_debug);

/** @} */ /* LNetDFS */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
