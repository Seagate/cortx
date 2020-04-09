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
   @page KLNetCoreDLD LNet Transport Kernel Core DLD

   - @ref KLNetCoreDLD-ovw
   - @ref KLNetCoreDLD-def
   - @ref KLNetCoreDLD-req
   - @ref KLNetCoreDLD-depends
   - @ref KLNetCoreDLD-highlights
   - Functional Specification
        - @ref LNetCoreDLD-fspec "LNet Transport Core API"<!-- ./lnet_core.h -->
        - @ref KLNetCore "Core Kernel Interface"         <!-- ./klnet_core.h -->
   - @ref KLNetCoreDLD-lspec
      - @ref KLNetCoreDLD-lspec-comps
      - @ref KLNetCoreDLD-lspec-userspace
      - @ref KLNetCoreDLD-lspec-match-bits
      - @ref KLNetCoreDLD-lspec-tm-list
      - @ref KLNetCoreDLD-lspec-bevq
      - @ref KLNetCoreDLD-lspec-lnet-init
      - @ref KLNetCoreDLD-lspec-reg
      - @ref KLNetCoreDLD-lspec-tm-res
      - @ref KLNetCoreDLD-lspec-buf-res
      - @ref KLNetCoreDLD-lspec-ev
      - @ref KLNetCoreDLD-lspec-recv
      - @ref KLNetCoreDLD-lspec-send
      - @ref KLNetCoreDLD-lspec-passive
      - @ref KLNetCoreDLD-lspec-active
      - @ref KLNetCoreDLD-lspec-lnet-cancel
      - @ref KLNetCoreDLD-lspec-state
      - @ref KLNetCoreDLD-lspec-thread
      - @ref KLNetCoreDLD-lspec-numa
   - @ref KLNetCoreDLD-conformance
   - @ref KLNetCoreDLD-ut
   - @ref KLNetCoreDLD-st
   - @ref KLNetCoreDLD-O
   - @ref KLNetCoreDLD-ref

   <hr>
   @section KLNetCoreDLD-ovw Overview
   The LNet Transport is built over an address space agnostic "core" I/O
   interface.  This document describes the kernel implementation of this
   interface, which directly interacts with the Lustre LNet kernel module.

   <hr>
   @section KLNetCoreDLD-def Definitions
   Refer to <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV
779386NtFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>

   <hr>
   @section KLNetCoreDLD-req Requirements
   - @b r.m0.net.lnet.buffer-registration Provide support for
     hardware optimization through buffer pre-registration.

   - @b r.m0.net.xprt.lnet.end-point-address The implementation
     should support the mapping of end point address to LNet address
     as described in the Refinement section of the HLD.

   - @b r.m0.net.xprt.lnet.multiple-messages-in-buffer Provide
     support for this feature as described in the HLD.

   - @b r.m0.net.xprt.lnet.dynamic-address-assignment Provide
     support for dynamic address assignment as described in the HLD.

   - @b r.m0.net.xprt.lnet.user-space The implementation must
     accommodate the needs of the user space LNet transport.

   - @b r.m0.net.xprt.lnet.user.no-gpl The implementation must not expose
     the user space transport to GPL interfaces.


   <hr>
   @section KLNetCoreDLD-depends Dependencies
   - <b>LNet API</b> headers are required to build the module.
   The Xyratex Lustre source package must be installed on the build
   machine (RPM @c lustre-source version 2.0 or greater).
   - <b>Xyratex Lustre run time</b>
   - @b r.m0.lib.atomic.interoperable-kernel-user-support The @ref LNetcqueueDLD
   "Buffer Event Circular Queue" <!-- ../bev_cqueue.c -->
   provides a shared data structure for efficiently passing event notifications
   from the Core layer to the LNet transport layer.
   - @b r.net.xprt.lnet.growable-event-queue The @ref LNetcqueueDLD
   "Buffer Event Circular Queue" <!-- ../bev_cqueue.c -->
   provides a way to expand the event queue as new buffers are queued with a
   transfer machine, ensuring no events are lost.

   <hr>
   @section KLNetCoreDLD-highlights Design Highlights
   - The Core API is an address space agnostic I/O interface intended for use
     by the Mero Networking LNet transport operation layer in either user
     space or kernel space.

   - Efficient support for the user space transports is provided by use of
     cross-address space tolerant data structures in shared memory.

   - The Core API does not expose any LNet symbols.

   - Each transfer machine is internally assigned one LNet event queue for all
     its LNet buffer operations.

   - Pre-allocation of buffer event space to guarantee that buffer operation
     results can be returned.

   - The notification of the completion of a buffer operation to the transport
     layer is decoupled from the LNet callback that provided this notification
     to the core module.

   - The number of messages that can be delivered into a single receive buffer
     is bounded to support pre-allocation of memory to hold the buffer event
     payload.

   - Buffer completion event notification is provided via a semaphore.  The
     design guarantees delivery of events in the order received from LNet.  In
     particular, the multiple possible events delivered for a single receive
     buffer will be ordered.

   <hr>
   @section KLNetCoreDLD-lspec Logical Specification

   - @ref KLNetCoreDLD-lspec-comps
   - @ref KLNetCoreDLD-lspec-userspace
   - @ref KLNetCoreDLD-lspec-match-bits
   - @ref KLNetCoreDLD-lspec-tm-list
   - @ref KLNetCoreDLD-lspec-bevq
   - @ref KLNetCoreDLD-lspec-lnet-init
   - @ref KLNetCoreDLD-lspec-reg
   - @ref KLNetCoreDLD-lspec-tm-res
   - @ref KLNetCoreDLD-lspec-buf-res
   - @ref KLNetCoreDLD-lspec-ev
   - @ref KLNetCoreDLD-lspec-recv
   - @ref KLNetCoreDLD-lspec-send
   - @ref KLNetCoreDLD-lspec-passive
   - @ref KLNetCoreDLD-lspec-active
   - @ref KLNetCoreDLD-lspec-lnet-cancel
   - @ref KLNetCoreDLD-lspec-state
   - @ref KLNetCoreDLD-lspec-thread
   - @ref KLNetCoreDLD-lspec-numa

   @subsection KLNetCoreDLD-lspec-comps Component Overview
   The relationship between the various objects in the components of the LNet
   transport and the networking layer is illustrated in the following UML
   diagram.  @image html "../../net/lnet/lnet_xo.png" "LNet Transport Objects"

   The Core layer in the kernel has no sub-components but interfaces directly
   with the Lustre LNet module in the kernel.


   @subsection KLNetCoreDLD-lspec-userspace Support for User Space Transports

   The kernel Core module is designed to support user space transports with the
   use of shared memory.  It does not directly provide a mechanism to
   communicate with the user space transport, but expects that the user space
   Core module will provide a device driver to communicate between user and
   kernel space, manage the sharing of core data structures, and interface
   between the kernel and user space implementations of the Core API.

   The common Core data structures are designed to support such communication
   efficiently:

   - The core data structures are organized with a distinction between the
     common directly shareable portions, and private areas for kernel and user
     space data.  This allows each address space to place pointer values of its
     address space in private regions associated with the shared data
     structures.

   - An address space opaque pointer type is provided to safely save pointer
     values in shared memory locations where necessary.

   - The single producer, single consumer circular buffer event queue shared
     between the transport and the core layer in the kernel is designed to work
     with the producer and consumer potentially in different address spaces.
     This is described in further detail in @ref KLNetCoreDLD-lspec-bevq.


  @subsection KLNetCoreDLD-lspec-match-bits Match Bits for Buffer Identification

   The kernel Core module will maintain a unsigned integer counter per transfer
   machine, to generate unique match bits for passive bulk buffers associated
   with that transfer machine.  The upper 12 match bits are reserved by the HLD
   to represent the transfer machine identifier. Therefore the counter is
   (64-12)=52 bits wide. The value of 0 is reserved for unsolicited
   receive messages, so the counter range is [1,0xfffffffffffff]. It is
   initialized to 1 and will wrap back to 1 when it reaches its upper bound.

   The transport uses the nlx_core_buf_passive_recv() or the
   nlx_core_buf_passive_send() subroutines to stage passive buffers.  Prior
   to initiating these operations, the transport should use the
   nlx_core_buf_desc_encode() subroutine to generate new match bits for
   the passive buffer.  The match bit counter will repeat over time, though
   after a very long while.  It is the transport's responsibility to ensure
   that all of the passive buffers associated with a given transfer machine
   have unique match bits.  The match bits should be encoded into the network
   buffer descriptor associated with the passive buffer.


   @subsection KLNetCoreDLD-lspec-tm-list Transfer Machine Uniqueness

   The kernel Core module must ensure that all transfer machines on the host
   have unique transfer machine identifiers for a given NID/PID/Portal,
   regardless of the transport instance or network domain context in which
   these transfer machines are created.  To support this, the ::nlx_kcore_tms
   list threads through all the kernel Core's per-TM private data
   structures. This list is private to the kernel Core, and is protected by the
   ::nlx_kcore_mutex.

   The same list helps in assigning dynamic transfer machine identifiers.  The
   highest available value at the upper bound of the transfer machine
   identifier space is assigned dynamically.  The logic takes into account the
   NID, PID and portal number of the new transfer machine when looking for an
   available transfer machine identifier.  A single pass over the list is
   required to search for an available transfer machine identifier.


   @subsection KLNetCoreDLD-lspec-bevq The Buffer Event Queue

   The kernel Core receives notification of the completion of a buffer
   operation through an LNet callback.  The completion status is not directly
   conveyed to the transport, because the transport layer may have processor
   affinity constraints that are not met by the LNet callback thread; indeed,
   LNet does not even state if this callback is in a schedulable context.

   Instead, the kernel Core module decouples the delivery of buffer operation
   completion to the transport from the LNet callback context by copying the
   result to an intermediate buffer event queue.  The Core API provides the
   nlx_core_buf_event_wait() subroutine that the transport can use to poll for
   the presence of buffer events, and the nlx_core_buf_event_get() subroutine
   to recover the payload of the next available buffer event. See @ref
   KLNetCoreDLD-lspec-ev for further details on these subroutines.

   There is another advantage to this indirect delivery: to address the
   requirement to efficiently support a user space transport, the Core module
   keeps this queue in memory shared between the transport and the Core,
   eliminating the need for a user space transport to make an @c ioctl call to
   fetch the buffer event payload.  The only @c ioctl call needed for a user
   space transport is to block waiting for buffer events to appear in the
   shared memory queue.

   It is critical for proper operation, that there be an available buffer event
   structure when the LNet callback is invoked, or else the event cannot be
   delivered and will be lost.  As the event queue is in shared memory, it is
   not possible, let alone desirable, to allocate a new buffer event structure
   in the callback context.

   The Core API guarantees the delivery of buffer operation completion status
   by maintaining a "pool" of free buffer event structures for this purpose.
   It does so by keeping count of the total number of buffer event structures
   required to satisfy all outstanding operations, and adding additional such
   structures to the "pool" if necessary, when a new buffer operation is
   initiated.  Likewise, the count is decremented for each buffer event
   delivered to the transport.  Most buffers operations only need a single
   buffer event structure in which to return their operation result, but
   receive buffers may need more, depending on the individually
   configurable maximum number of messages that could be received in each
   receive buffer.

   The pool and queue potentially span the kernel and user address spaces.
   There are two cases around the use of these data structures:

   - Normal queue operation involves a single @a producer, in the kernel Core
     callback subroutine, and a single @a consumer, in the Core API
     nlx_core_buf_event_get() subroutine, which may be invoked either in
     the kernel or in user space.

   - The allocation of new buffer event structures to the "pool" is always done
     by the Core API buffer operation initiation subroutines invoked by the
     transport.  The user space implementation of the Core API would have to
     arrange for these new structures to get mapped into the kernel at this
     time.

   The kernel Core module combines both the free "pool" and the result queue
   into a single data structure: a circular, single producer, single consumer
   buffer event queue.  Details on this event queue are covered in the
   @ref LNetcqueueDLD "LNet Buffer Event Circular Queue DLD."

   The design makes a critical simplifying assumption, in that the transport
   will use exactly one thread to process events.  This assumption implicitly
   serializes the delivery of the events associated with any given receive
   buffer, thus the last event which unlinks the buffer is guaranteed to be
   delivered after other events associated with that same buffer operation.


   @subsection KLNetCoreDLD-lspec-lnet-init LNet Initialization and Finalization

   No initialization and finalization logic is required for LNet in the kernel
   for the following reasons:

   - Use of the LNet kernel module is reference counted by the kernel.
   - The LNetInit() subroutine is automatically called when then LNet kernel
     module is loaded, and cannot be called multiple times.


   @subsection KLNetCoreDLD-lspec-reg LNet Buffer Registration

   No hardware optimization support is defined in the LNet API at this time but
   the nlx_core_buf_register() subroutine serves as a placeholder where any
   such optimizations could be made in the future.  The
   nlx_core_buf_deregister() subroutine would be used to release any allocated
   resources.

   During buffer registration, the kernel Core API will translate the
   m0_net_bufvec into the nlx_kcore_buffer::kb_kiov field of the buffer private
   data.

   The kernel implementation of the Core API does not increment the page count
   of the buffer pages.  The supposition here is that the buffers are allocated
   by Mero file system clients, and the Core API has no business imposing
   memory management policy beneath such a client.


   @subsection KLNetCoreDLD-lspec-tm-res LNet Transfer Machine Resources

   A transfer machine is associated with the following LNet resources:
   - An Event Queue (EQ).  This is represented by the
   nlx_kcore_transfer_mc::ktm_eqh handle.

   The nlx_core_tm_start() subroutine creates the event handle.  The
   nlx_core_tm_stop() subroutine releases the handle.
   See @ref KLNetCoreDLD-lspec-ev for more details on event processing.


   @subsection KLNetCoreDLD-lspec-buf-res LNet Buffer Resources

   A network buffer is associated with a Memory Descriptor (MD).  This is
   represented by the nlx_kcore_buffer::kb_mdh handle.  There may be a Match
   Entry (ME) associated with this MD for some operations, but when created, it
   is set up to unlink automatically when the MD is unlinked so it is not
   explicitly tracked.

   All the buffer operation initiation subroutines of the kernel Core API
   create such MDs.  Although an MD is set up to explicitly unlink upon
   completion, the value is saved in case an operation needs to be
   cancelled.

   All MDs are associated with the EQ of the transfer machine
   (nlx_kcore_transfer_mc::ktm_eqh).


   @subsection KLNetCoreDLD-lspec-ev LNet Event Callback Processing

   LNet event queues are used with an event callback subroutine to avoid event
   loss.  The callback subroutine overhead is fairly minimal, as it only copies
   out the event payload and arranges for subsequent asynchronous delivery.
   This, coupled with the fact that the circular buffer used works optimally
   with a single producer and single consumer resulted in the decision to use
   just one LNet EQ per transfer machine (nlx_kcore_transfer_mc::ktm_eqh).

   The EQ is created in the call to the nlx_core_tm_start() subroutine, and
   is freed in the call to the nlx_core_tm_stop() subroutine.

   LNet requires that the callback subroutine be re-entrant and non-blocking,
   and not make any LNet API calls. Given that the circular queue assumes a
   single producer and single consumer, a spin lock is used to serialize access
   of the two EQs to the circular queue.

   The event callback requires that the MD @c user_ptr field be set up to the
   address of the nlx_kcore_buffer data structure.  Note that if an event
   has the @c unlinked field set then this will be the last event that LNet will
   post for the related operation, and the @c user_ptr field will be valid, so
   the callback can safely de-reference the field to determine the correct
   queue.

   The callback subroutine does the following:

   -# It will ignore @c LNET_EVENT_SEND events delivered as a result of a @c
      LNetGet() call if the @c unlinked field of the event is not set. If the
      @c unlinked field is set, the event could either be an out-of-order SEND
      (terminating a REPLY/SEND sequence), or the piggy-backed UNLINK on an
      in-order SEND.  The two cases are distinguished by explicitly tracking
      the receipt of an out-of-order REPLY (in nlx_kcore_buffer::kb_ooo_reply).
      An out-of-order SEND will be treated as though it is the terminating @c
      LNET_EVENT_REPLY event of a SEND/REPLY sequence.
   -# It will not create an event in the circular queue for @c LNET_EVENT_REPLY
      events that do not have their @c unlinked field set.  They indicate an
      out-of-sequence REPLY/SEND combination, and LNet will issue a valid SEND
      event subsequently.  However, the receipt of such an REPLY will be
      remembered in nlx_kcore_buffer::kb_ooo_reply, and its payload in the
      other "ooo" fields, so that when the out-of-order SEND arrives, this data
      can be used to generate the circular queue event.
   -# It will ignore @c LNET_EVENT_ACK events.
   -# It obtains the nlx_kcore_transfer_mc::ktm_bevq_lock spin lock.
   -# The bev_cqueue_pnext() subroutine is then used to locate the next buffer
      event structure in the circular buffer event queue which will be used to
      return the result.
   -# It copies the event payload from the LNet event to the buffer event
      structure.  This includes the value of the @c unlinked field of the
      event, which must be copied to the nlx_core_buffer_event::cbe_unlinked
      field.  For @c LNET_EVENT_UNLINK events, a @c -ECANCELED value is
      written to the nlx_core_buffer_event::cbe_status field and the
      nlx_core_buffer_event::cbe_unlinked field set to true.
      For @c LNET_EVENT_PUT events corresponding to unsolicited message
      delivery, the sender's TMID and Portal are encoded
      in the hdr_data.  These values are decoded into the
      nlx_core_buffer_event::cbe_sender, along with the initiator's NID and PID.
      The nlx_core_buffer_event::cbe_sender is not set for other events.
   -# It invokes the bev_cqueue_put() subroutine to "produce" the event in the
      circular queue.
   -# It releases the nlx_kcore_transfer_mc::ktm_bevq_lock spin lock.
   -# It signals the nlx_kcore_transfer_mc::ktm_sem semaphore with the
      m0_semaphore_up() subroutine.

   The (single) transport layer event handler thread blocks on the Core
   transfer machine semaphore in the Core API nlx_core_buf_event_wait()
   subroutine which uses the m0_semaphore_timeddown() subroutine internally to
   wait on the semaphore.  When the Core API subroutine returns with an
   indication of the presence of events, the event handler thread consumes all
   the pending events with multiple calls to the Core API
   nlx_core_buf_event_get() subroutine, which uses the bev_cqueue_get()
   subroutine internally to get the next buffer event.  Then the event handler
   thread repeats the call to the nlx_core_buf_event_wait() subroutine to once
   again block for additional events.

   In the case of the user space transport, the blocking on the semaphore is
   done indirectly by the user space Core API's device driver in the kernel.
   It is required by the HLD that as many events as possible be consumed before
   the next context switch to the kernel must be made.  To support this, the
   kernel Core nlx_core_buf_event_wait() subroutine takes a few additional
   steps to minimize the chance of returning when the queue is empty.  After it
   obtains the semaphore with the m0_semaphore_timeddown() subroutine (i.e. the
   @em P operation succeeds), it attempts to clear the semaphore count by
   repeatedly calling the m0_semaphore_trydown() subroutine until it fails.  It
   then checks the circular queue, and only if not empty will it return.  This
   is illustrated with the following pseudo-code:
   @code
       do {
          rc = m0_semaphore_timeddown(&sem, &timeout);
          if (rc < 0)
              break; // timed out
          while (m0_semaphore_trydown(&sem))
              ; // exhaust the semaphore
       } while (bev_cqueue_is_empty(&q)); // loop if empty
   @endcode
   (The C++ style comments are used because of doxygen only - they are not
   permitted by the Mero style guide.)

   @subsection KLNetCoreDLD-lspec-recv LNet Receiving Unsolicited Messages

   -# Create an ME with @c LNetMEAttach() for the transfer machine and specify
      the portal, match and ignore bits. All receive buffers for a given TM
      will use a match bit value equal to the TM identifier in the higher order
      bits and zeros for the other bits.  No ignore bits are set. The ME should
      be set up to unlink automatically as it will be used for all receive
      buffers of this transfer machine.  The ME entry should be positioned at
      the end of the portal match list.  There is no need to retain the ME
      handle beyond the subsequent @c LNetMDAttach() call.
   -# Create and attach an MD to the ME using @c LNetMDAttach().
      The MD is set up to unlink automatically.
      Save the MD handle in the nlx_kcore_buffer::kb_mdh field.
      Set up the fields of the @c lnet_md_t argument as follows:
      - Set the @c eq_handle to identify the EQ associated with the transfer
        machine (nlx_kcore_transfer_mc::ktm_eqh).
      - Set the address of the nlx_kcore_buffer in the @c user_ptr field.
      - Pass in the KIOV from the nlx_kcore_buffer::kb_kiov.
      - Set the @c threshold value to the nlx_kcore_buffer::kb_max_recv_msgs
        value.
      - Set the @c max_size value to the nlx_kcore_buffer::kb_min_recv_size
        value.
      - Set the @c LNET_MD_OP_PUT, @c LNET_MD_MAX_SIZE and @c LNET_MD_KIOV
        flags in the @c options field.
   -# When a message arrives, an @c LNET_EVENT_PUT event will be delivered to
      the event queue, and will be processed as described in
      @ref KLNetCoreDLD-lspec-ev.


   @subsection KLNetCoreDLD-lspec-send LNet Sending Messages

   -# Create an MD using @c LNetMDBind() with each invocation of the
      nlx_core_buf_msg_send() subroutine.
      The MD is set up to unlink automatically.
      Save the MD handle in the nlx_kcore_buffer::kb_mdh field.
      Set up the fields of the @c lnet_md_t argument as follows:
      - Set the @c eq_handle to identify the EQ associated with the transfer
        machine (nlx_kcore_transfer_mc::ktm_eqh).
      - Set the address of the nlx_kcore_buffer in the @c user_ptr field.
      - Pass in the KIOV from the nlx_kcore_buffer::kb_kiov.
        The number of entries in the KIOV and the length field in the last
        element of the vector must be adjusted to reflect the desired byte
        count.
      - Set the @c LNET_MD_KIOV flag in the @c options field.
   -# Use the @c LNetPut() subroutine to send the MD to the destination.  The
      match bits must set to the destination TM identifier in the higher order
      bits and zeros for the other bits. The hdr_data must be set to a value
      encoding the TMID (in the upper bits, like the match bits) and the portal
      (in the lower bits). No acknowledgment should be requested.
   -# When the message is sent, an @c LNET_EVENT_SEND event will be delivered
      to the event queue, and processed as described in
      @ref KLNetCoreDLD-lspec-ev.
      @note The event does not indicate if the recipient was able to save the
      data, but merely that it left the host.

   @subsection KLNetCoreDLD-lspec-passive LNet Staging Passive Bulk Buffers

   -# Prior to invoking the nlx_core_buf_passive_recv() or the
      nlx_core_buf_passive_send() subroutines, the transport should use the
      nlx_core_buf_desc_encode() subroutine to assign unique match bits to
      the passive buffer. See @ref KLNetCoreDLD-lspec-match-bits for details.
      The match bits should be encoded into the network buffer descriptor and
      independently conveyed to the remote active transport.
      The network descriptor also encodes the number of bytes to be transferred.
   -# Create an ME using @c LNetMEAttach(). Specify the portal and match_id
      fields as appropriate for the transfer machine.  The buffer's match bits
      are obtained from the nlx_core_buffer::cb_match_bits field.  No ignore
      bits are set. The ME should be set up to unlink automatically, so there
      is no need to save the handle for later use.  The ME should be positioned
      at the end of the portal match list.
   -# Create and attach an MD to the ME using @c LNetMDAttach() with each
      invocation of the nlx_core_buf_passive_recv() or the
      nlx_core_buf_passive_send() subroutines.
      The MD is set up to unlink automatically.
      Save the MD handle in the nlx_kcore_buffer::kb_mdh field.
      Set up the fields of the @c lnet_md_t argument as follows:
      - Set the @c eq_handle to identify the EQ associated with the transfer
        machine (nlx_kcore_transfer_mc::ktm_eqh).
      - Set the address of the nlx_kcore_buffer in the @c user_ptr field.
      - Pass in the KIOV from the nlx_kcore_buffer::kb_kiov.
      - Set the @c LNET_MD_KIOV flag in the @c options field, along with either
        the @c LNET_MD_OP_PUT or the @c LNET_MD_OP_GET flag according to the
        direction of data transfer.
   -# When the bulk data transfer completes, either an @c LNET_EVENT_PUT or an
      @c LNET_EVENT_GET event will be delivered to the event queue, and will be
      processed as described in @ref KLNetCoreDLD-lspec-ev.


   @subsection KLNetCoreDLD-lspec-active LNet Active Bulk Read or Write

   -# Prior to invoking the nlx_core_buf_active_recv() or
      nlx_core_buf_active_send() subroutines, the
      transport should put the match bits of the remote passive buffer into the
      nlx_core_buffer::cb_match_bits field. The destination address of the
      remote transfer machine with the passive buffer should be set in the
      nlx_core_buffer::cb_addr field.
   -# Create an MD using @c LNetMDBind() with each invocation of the
      nlx_core_buf_active_recv() or nlx_core_buf_active_send() subroutines.
      The MD is set up to unlink automatically.
      Save the MD handle in the nlx_kcore_buffer::kb_mdh field.
      Set up the fields of the @c lnet_md_t argument as follows:
      - Set the @c eq_handle to identify the EQ associated with
        the transfer machine (nlx_kcore_transfer_mc::ktm_eqh).
      - Set the address of the nlx_kcore_buffer in the @c user_ptr field.
      - Pass in the KIOV from the nlx_kcore_buffer::kb_kiov.
        The number of entries in the KIOV and the length field in the last
        element of the vector must be adjusted to reflect the desired byte
        count.
      - Set the @c LNET_MD_KIOV flag in the @c options field.
      - In case of an active read, which uses @c LNetGet(), set the threshold
        value to 2 to accommodate both the SEND and the REPLY events. Otherwise
	set it to 1.
   -# Use the @c LNetGet() subroutine to initiate the active read or the @c
      LNetPut() subroutine to initiate the active write. The @c hdr_data is set
      to 0 in the case of @c LNetPut(). No acknowledgment should be requested.
      In the case of an @c LNetGet(), the field used to track out-of-order
      REPLY events (nlx_kcore_buffer::kb_ooo_reply) should be cleared before
      the operation is initiated.
   -# When a response to the @c LNetGet() or @c LNetPut() call completes, an @c
      LNET_EVENT_SEND event will be delivered to the event queue and should
      typically be ignored in the case of @c LNetGet().
      See @ref KLNetCoreDLD-lspec-ev for details.
   -# When the bulk data transfer for @c LNetGet() completes, an
      @c LNET_EVENT_REPLY event will be delivered to the event queue, and will
      be processed as described in @ref KLNetCoreDLD-lspec-ev.

      @note LNet does not guarantee the order of the SEND and REPLY events
      associated with the @c LNetGet() operation.  Also note that in the case
      of an @c LNetGet() operation, the SEND event does not indicate if the
      recipient was able to save the data, but merely that the request left the
      host.


   @subsection KLNetCoreDLD-lspec-lnet-cancel LNet Canceling Operations

   The kernel Core module provides no timeout capability.  The transport may
   initiate a cancel operation using the nlx_core_buf_del() subroutine.

   This will result in an @c LNetMDUnlink() subroutine call being issued for
   the buffer MD saved in the nlx_kcore_buffer::kb_mdh field.
   Cancellation may or may not take place - it depends upon whether the
   operation has started, and there is a race condition in making this call and
   concurrent delivery of an event associated with the MD.

   Assuming success, the next event delivered for the buffer concerned will
   either be a @c LNET_EVENT_UNLINK event or the @c unlinked field will be set
   in the next completion event for the buffer.  The events will be processed
   as described in @ref KLNetCoreDLD-lspec-ev.

   LNet properly handles the race condition between the automatic unlink of the
   MD and a call to @c LNetMDUnlink().


   @subsection KLNetCoreDLD-lspec-state State Specification
   - The kernel Core module relies on the networking data structures to maintain
   the linkage between the data structures used by the Core module. It
   maintains no lists through data structures itself.  As such, these lists can
   only be navigated by the Core API subroutines invoked by the transport (the
   "upper" layer) and not by the Core module's LNet callback subroutine (the
   "lower" layer).

   - The kernel Core API maintains a count of the total number of buffer event
   structures needed.  This should be tested by the Core API's transfer machine
   invariant subroutine before returning from any buffer operation initiation
   call, and before returning from the nlx_core_buf_event_get() subroutine.

   - The kernel Core layer module depends on the LNet module in the kernel at
   run time. This dependency is captured by the Linux kernel module support that
   reference counts the usage of dependent modules.

   - The kernel Core layer modules explicitly tracks the events received for @c
   LNetGet() calls, in the nlx_kcore_buffer data structure associated with the
   call.  This is because there are two events (SEND and REPLY) that are
   returned for this operation, and LNet does not guarantee their order of
   arrival, and the event processing logic is set up such that a circular
   buffer event must be created only upon receipt of the last operation event.
   Complicating the issue is that a cancellation response could be piggy-backed
   onto an in-order SEND.  See @ref KLNetCoreDLD-lspec-ev and @ref
   KLNetCoreDLD-lspec-active for details.


   @subsection KLNetCoreDLD-lspec-thread Threading and Concurrency Model
   -# Generally speaking, API calls within the transport address space
      are protected by the serialization of the Mero Networking layer,
      typically the transfer machine mutex or the domain mutex.
      The nlx_core_buf_desc_encode() subroutine, for example, is fully
      protected by the transfer machine mutex held across the
      m0_net_buffer_add() subroutine call, so implicitly protects the match bit
      counter in the kernel Core's per TM private data.
   -# The Mero Networking layer serialization does not always suffice, as
      the kernel Core module has to support concurrent multiple transport
      instances in kernel and user space.  Fortunately, the LNet API
      intrinsically provides considerable serialization support to the Core, as
      transfer machines are defined by the HLD to have disjoint addresses.
   -# Enforcement of the disjoint address semantics are protected by the
      kernel Core's ::nlx_kcore_mutex lock.  The nlx_core_tm_start() and
      nlx_core_tm_stop() subroutines use this mutex internally for serialization
      and operation on the ::nlx_kcore_tms list threaded through the kernel
      Core's per-TM private data.
   -# The kernel Core module registers a callback subroutine with the
      LNet EQ defined per transfer machine. LNet requires that this subroutine
      be reentrant and non-blocking.  The circular buffer event queue accessed
      from the callback requires a single producer, so the
      nlx_kcore_transfer_mc::ktm_bevq_lock spin lock is used to serialize its
      use across possible concurrent invocations.  The time spent in the lock
      is minimal.
   -# The Core API does not support callbacks to indicate completion of an
      asynchronous buffer operation.  Instead, the transport application must
      invoke the nlx_core_buf_event_wait() subroutine to block waiting for
      buffer events.  Internally this call waits on the
      nlx_kcore_transfer_mc::ktm_sem semaphore.  The semaphore is
      incremented each time an event is added to the buffer event queue.
   -# The event payload is actually delivered via a per transfer machine
      single producer, single consumer, lock-free circular buffer event queue.
      The only requirement for failure free operation is to ensure that there
      are sufficient event structures pre-allocated to the queue, plus one more
      to support the circular semantics.  Multiple events may be dequeued
      between each call to the nlx_core_buf_event_wait() subroutine.  Each such
      event is fetched by a call to the nlx_core_buf_event_get() subroutine,
      until the queue is exhausted.  Note that the queue exists in memory
      shared between the transport and the kernel Core; the transport could be
      in the kernel or in user space.
   -# The API assumes that only a single transport thread will handle event
      processing.  This is a critical assumption in the support for multiple
      messages in a single receive buffer, as it implicitly serializes the
      delivery of the events associated with any given receive buffer, thus the
      last event which unlinks the buffer is guaranteed to be delivered last.
   -# The Mero LNet transport driver releases all kernel resources
      associated with a user space domain when the device is released (the final
      close).  It must not release buffer event objects or transfer machines
      while the LNet EQ callback requires them.  The Kernel Core LNet EQ
      callback, nlx_kcore_eq_cb(), resets the association between a buffer and
      a transfer machine and increments the nlx_kcore_transfer_mc::ktm_sem
      semaphore while holding the nlx_kcore_transfer_mc::ktm_bevq_lock, and the
      callback never refers to either object after releasing the lock.  The
      driver layer holds this lock as well while verifying that a buffer is not
      associated with a transfer machine, and, outside the lock, decrements the
      semaphore to wait for buffers to be unlinked by LNet (the device is being
      released, so no other thread will be decrementing the semaphore).  This
      assures the buffer event objects and the transfer machine will remain
      until the final LNet event is delivered.
   -# LNet properly handles the race condition between the automatic unlink
      of the MD and a call to @c LNetMDUnlink().

   @subsection KLNetCoreDLD-lspec-numa NUMA optimizations
   The LNet transport will initiate calls to the API on threads that may have
   specific process affinity assigned.

   LNet offers no direct NUMA optimizations.  In particular, event callbacks
   cannot be constrained to have any specific processor affinity.  The API
   compensates for this lack of support by providing a level of indirection in
   event delivery: its callback handler simply copies the LNet event payload to
   an event delivery queue and notifies a transport event processing thread of
   the presence of the event. (See @ref KLNetCoreDLD-lspec-bevq above). The
   transport event processing threads can be constrained to have any desired
   processor affinity.

   <hr>
   @section KLNetCoreDLD-conformance Conformance
   - @b i.m0.net.lnet.buffer-registration See @ref KLNetCoreDLD-lspec-reg.

   - @b i.m0.net.xprt.lnet.end-point-address The nlx_core_ep_addr_encode()
     and nlx_core_ep_addr_decode() provide this functionality.

   - @b i.m0.net.xprt.lnet.multiple-messages-in-buffer See @ref
     KLNetCoreDLD-lspec-recv.

   - @b i.m0.net.xprt.lnet.dynamic-address-assignment See @ref
     KLNetCoreDLD-lspec-tm-list.

   - @b i.m0.net.xprt.lnet.user-space See @ref KLNetCoreDLD-lspec-userspace.

   - @b i.m0.net.xprt.lnet.user.no-gpl See the @ref LNetCoreDLD-fspec
     "Functional Specification"; no LNet headers are exposed by the Core API.

   <hr>
   @section KLNetCoreDLD-ut Unit Tests
   The testing strategy is 2 pronged:
   - Tests with a fake LNet API.  These tests will intercept the LNet
     subroutine calls.  The real LNet data structures will be used by the Core
     API.
   - Tests with the real LNet API using the TCP loop back address.  These tests
     will use the TCP loop back address.  LNet on the test machine must be
     configured with the @c "tcp" network.

   @test The correct sequence of LNet operations are issued for each type
         of buffer operation with a fake LNet API.

   @test The callback subroutine properly delivers events to the buffer
         event queue, including single and multiple events for receive buffers
         with a fake LNet API.

   @test The dynamic assignment of transfer machine identifiers with a fake LNet
         API.

   @test Test the parsing of LNet addresses with the real LNet API.

   @test Test each type of buffer operation, including single and multiple
         events for receive buffers with the real LNet API.

   <hr>
   @section KLNetCoreDLD-st System Tests
   System testing will be performed as part of the transport operation system
   test.

   <hr>
   @section KLNetCoreDLD-O Analysis
   - Dynamic transfer machine identifier assignment is proportional to the
   number of transfer machines defined on the server, including kernel and all
   process space LNet transport instances.
   - The time taken to process an LNet event callback is in constant time.
   - The time taken for the transport to dequeue a pending buffer event
   depends upon the operating system scheduler.  The algorithmic
   processing involved is in constant time.
   - The time taken to register a buffer is in constant time.  The reference
     count of the buffer pages is not incremented, so there are no VM subsystem
     imposed delays.
   - The time taken to process outbound buffer operations is unpredictable,
   and depends, at the minimum, on current system load, other LNet users,
   and on the network load.

   <hr>
   @section KLNetCoreDLD-ref References
   - <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV779386N
tFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>
   - The LNet API.

 */

/*
 ******************************************************************************
 End of DLD
 ******************************************************************************
 */

#include "lib/mutex.h"
#include "net/lnet/linux_kernel/klnet_core.h"

/* LNet API, LNET_NIDSTR_SIZE */
#if M0_LUSTRE_VERSION < 2110
#include <lnet/lnet.h>
#else
#include <lnet/nidstr.h>
#endif

#if LUSTRE_VERSION_CODE >= OBD_OCD_VERSION(2, 7, 51, 0)
#include <lnet/api.h>
#include <lnet/lib-types.h>
#endif

/* include local files */
#include "net/lnet/linux_kernel/klnet_vec.c"
#include "net/lnet/linux_kernel/klnet_utils.c"

/**
   @addtogroup KLNetCore
   @{
 */

/**
   Kernel core lock.
   Provides serialization across the nlx_kcore_tms list.
 */
static struct m0_mutex nlx_kcore_mutex;

/** List of all transfer machines. Protected by nlx_kcore_mutex. */
static struct m0_tl nlx_kcore_tms;

/** NID strings of LNIs. */
static char **nlx_kcore_lni_nidstrs;
/** The count of non-NULL entries in nlx_kcore_lni_nidstrs.
    @note Caching this count and the string list will no longer be possible
    if and when dynamic NI addition/removal is supported by LNet.
 */
static unsigned int nlx_kcore_lni_nr;
/** Reference counter for nlx_kcore_lni_nidstrs. */
static struct m0_atomic64 nlx_kcore_lni_refcount;

M0_TL_DESCR_DEFINE(tms, "nlx tms", static, struct nlx_kcore_transfer_mc,
		   ktm_tm_linkage, ktm_magic, M0_NET_LNET_KCORE_TM_MAGIC,
		   M0_NET_LNET_KCORE_TMS_MAGIC);
M0_TL_DEFINE(tms, static, struct nlx_kcore_transfer_mc);

M0_TL_DESCR_DEFINE(drv_tms, "drv tms", static, struct nlx_kcore_transfer_mc,
		   ktm_drv_linkage, ktm_magic, M0_NET_LNET_KCORE_TM_MAGIC,
		   M0_NET_LNET_DEV_TMS_MAGIC);
M0_TL_DEFINE(drv_tms, static, struct nlx_kcore_transfer_mc);

M0_TL_DESCR_DEFINE(drv_bufs, "drv bufs", static, struct nlx_kcore_buffer,
		   kb_drv_linkage, kb_magic, M0_NET_LNET_KCORE_BUF_MAGIC,
		   M0_NET_LNET_DEV_BUFS_MAGIC);
M0_TL_DEFINE(drv_bufs, static, struct nlx_kcore_buffer);

M0_TL_DESCR_DEFINE(drv_bevs, "drv bevs", static, struct nlx_kcore_buffer_event,
		   kbe_drv_linkage, kbe_magic, M0_NET_LNET_KCORE_BEV_MAGIC,
		   M0_NET_LNET_DEV_BEVS_MAGIC);
M0_TL_DEFINE(drv_bevs, static, struct nlx_kcore_buffer_event);

/* assert the equivalence of LNet and Mero data types */
M0_BASSERT(sizeof(__u64) == sizeof(uint64_t));

/** Unit test intercept support.
   Conventions to use:
   - All such subs must be declared in headers.
   - A macro named for the subroutine, but with the "NLX" portion of the prefix
   in capitals, should be used to call the subroutine via this intercept
   vector.
   - UT should restore the vector upon completion. It is not declared
   const so that the UTs can modify it.
 */
struct nlx_kcore_interceptable_subs {
	int (*_nlx_kcore_LNetMDAttach)(struct nlx_kcore_transfer_mc *kctm,
				       struct nlx_core_buffer *lcbuf,
				       struct nlx_kcore_buffer *kcb,
				       lnet_md_t *umd);
	int (*_nlx_kcore_LNetPut)(struct nlx_kcore_transfer_mc *kctm,
				  struct nlx_core_buffer *lcbuf,
				  struct nlx_kcore_buffer *kcb,
				  lnet_md_t *umd);
	int (*_nlx_kcore_LNetGet)(struct nlx_kcore_transfer_mc *kctm,
				  struct nlx_core_buffer *lcbuf,
				  struct nlx_kcore_buffer *kcb,
				  lnet_md_t *umd);
};
static struct nlx_kcore_interceptable_subs nlx_kcore_iv = {
#define _NLXIS(s) ._##s = s

	_NLXIS(nlx_kcore_LNetMDAttach),
	_NLXIS(nlx_kcore_LNetPut),
	_NLXIS(nlx_kcore_LNetGet),

#undef _NLXI
};

#define NLX_kcore_LNetMDAttach(ktm, lcbuf, kb, umd)	\
	(*nlx_kcore_iv._nlx_kcore_LNetMDAttach)(ktm, lcbuf, kb, umd)
#define NLX_kcore_LNetPut(ktm, lcbuf, kb, umd)		\
	(*nlx_kcore_iv._nlx_kcore_LNetPut)(ktm, lcbuf, kb, umd)
#define NLX_kcore_LNetGet(ktm, lcbuf, kb, umd)		\
	(*nlx_kcore_iv._nlx_kcore_LNetGet)(ktm, lcbuf, kb, umd)

/**
   KCore domain invariant.
   @note Unlike other kernel core object invariants, the reference to the
   nlx_core_domain is allowed to be NULL, because initialization of the
   nlx_kcore_domain in the driver is split between the open and M0_LNET_DOM_INIT
   ioctl request.
 */
static bool nlx_kcore_domain_invariant(const struct nlx_kcore_domain *kd)
{
	return kd != NULL && kd->kd_magic == M0_NET_LNET_KCORE_DOM_MAGIC &&
	       (nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc) ||
		nlx_core_kmem_loc_invariant(&kd->kd_cd_loc));
}

/**
   KCore buffer invariant.
   @note Shouldn't require the mutex as it is called from nlx_kcore_eq_cb.
 */
static bool nlx_kcore_buffer_invariant(const struct nlx_kcore_buffer *kcb)
{
	return kcb != NULL && kcb->kb_magic == M0_NET_LNET_KCORE_BUF_MAGIC &&
	       nlx_core_kmem_loc_invariant(&kcb->kb_cb_loc);
}

/**
   KCore buffer event invariant.
 */
static bool nlx_kcore_buffer_event_invariant(
				      const struct nlx_kcore_buffer_event *kbe)
{
	return kbe != NULL && kbe->kbe_magic == M0_NET_LNET_KCORE_BEV_MAGIC &&
	       nlx_core_kmem_loc_invariant(&kbe->kbe_bev_loc);
}

/**
   KCore tm invariant.
   @note Shouldn't require the mutex as it is called from nlx_kcore_eq_cb.
 */
static bool nlx_kcore_tm_invariant(const struct nlx_kcore_transfer_mc *kctm)
{
	return kctm != NULL && kctm->ktm_magic == M0_NET_LNET_KCORE_TM_MAGIC &&
	       nlx_core_kmem_loc_invariant(&kctm->ktm_ctm_loc);
}

/**
   Tests if the specified address is in use by a running TM.
   @note the nlx_kcore_mutex must be locked by the caller
 */
static bool nlx_kcore_addr_in_use(struct nlx_core_ep_addr *cepa)
{
	M0_PRE(m0_mutex_is_locked(&nlx_kcore_mutex));

	return m0_tl_exists(tms, scan, &nlx_kcore_tms,
			    nlx_core_ep_eq(&scan->ktm_addr, cepa));
}

/**
   Find an unused tmid.
   @note The nlx_kcore_mutex must be locked by the caller
   @param cepa The NID, PID and Portal are used to filter the ::nlx_kcore_tms.
   @return The largest available tmid, or -EADDRNOTAVAIL if none exists.
 */
static int nlx_kcore_max_tmid_find(struct nlx_core_ep_addr *cepa)
{
	int tmid = M0_NET_LNET_TMID_MAX;
	struct nlx_kcore_transfer_mc *scan;
	struct nlx_core_ep_addr *scanaddr;
	M0_PRE(m0_mutex_is_locked(&nlx_kcore_mutex));

	/* list is in descending order by tmid */
	m0_tl_for(tms, &nlx_kcore_tms, scan) {
		scanaddr = &scan->ktm_addr;
		if (scanaddr->cepa_nid == cepa->cepa_nid &&
		    scanaddr->cepa_pid == cepa->cepa_pid &&
		    scanaddr->cepa_portal == cepa->cepa_portal) {
			if (scanaddr->cepa_tmid == tmid)
				--tmid;
			else if (scanaddr->cepa_tmid < tmid)
				break;
		}
	} m0_tl_endfor;
	return tmid >= 0 ? tmid : M0_ERR(-EADDRNOTAVAIL);
}

/**
   Add the transfer machine to the ::nlx_kcore_tms.  The list is kept in
   descending order sorted by nlx_core_ep_addr::cepa_tmid.
   @note the nlx_kcore_mutex must be locked by the caller
 */
static void nlx_kcore_tms_list_add(struct nlx_kcore_transfer_mc *kctm)
{
	struct nlx_kcore_transfer_mc *scan;
	struct nlx_core_ep_addr *scanaddr;
	struct nlx_core_ep_addr *cepa = &kctm->ktm_addr;
	M0_PRE(m0_mutex_is_locked(&nlx_kcore_mutex));

	m0_tl_for(tms, &nlx_kcore_tms, scan) {
		scanaddr = &scan->ktm_addr;
		if (scanaddr->cepa_tmid <= cepa->cepa_tmid) {
			tms_tlist_add_before(scan, kctm);
			return;
		}
	} m0_tl_endfor;
	tms_tlist_add_tail(&nlx_kcore_tms, kctm);
}

/**
   Callback for the LNet Event Queue.
   It must be re-entrant, and not make any calls to the LNet API.
   It must not perform non-atomic operations, such as locking a mutex.
   An atomic spin lock is used to synchronize access to the
   nlx_core_transfer_mc::nlx_core_bev_cqueue.
 */
static void nlx_kcore_eq_cb(lnet_event_t *event)
{
	struct nlx_kcore_buffer *kbp;
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_core_transfer_mc *ctm;
	struct nlx_core_bev_link *ql;
	struct nlx_core_buffer_event *bev;
	m0_time_t now = m0_time_now();
	bool is_unlinked = false;
	unsigned mlength;
	unsigned offset;
	int status;

	M0_PRE(event != NULL);
	if (event->type == LNET_EVENT_ACK) {
		/* we do not use ACK */
		NLXDBG(&nlx_debug, 1,
		       nlx_kprint_lnet_event("nlx_kcore_eq_cb: filtered ACK",
					     event));
		return;
	}
	kbp = event->md.user_ptr;
	M0_ASSERT(nlx_kcore_buffer_invariant(kbp));
	ktm = kbp->kb_ktm;
	M0_ASSERT(nlx_kcore_tm_invariant(ktm));

	NLXDBGP(ktm, 1, "\t%p: eq_cb: %p %s U:%d S:%d T:%d buf:%lx\n",
		ktm, event, nlx_kcore_lnet_event_type_to_string(event->type),
		event->unlinked, event->status, event->md.threshold,
		(unsigned long) kbp->kb_buffer_id);
	NLXDBG(ktm, 2, nlx_kprint_lnet_event("eq_cb", event));
	NLXDBG(ktm, 3, nlx_kprint_kcore_tm("eq_cb", ktm));

	if (event->unlinked != 0) {
		LNetInvalidateMDHandle(&kbp->kb_mdh); /* Invalid use, but safe */
		/* kbp->kb_ktm = NULL set below */
		is_unlinked = true;
	}
	status  = event->status;
	mlength = event->mlength;
	offset  = event->offset;

	if (event->type == LNET_EVENT_SEND &&
	    kbp->kb_qtype == M0_NET_QT_ACTIVE_BULK_RECV) {
		/* An LNetGet related event, normally ignored */
		if (!is_unlinked) {
			NLXDBGP(ktm, 1, "\t%p: ignored LNetGet() SEND\n", ktm);
			return;
		}
		/* An out-of-order SEND, or
		   cancellation notification piggy-backed onto an in-order SEND.
		   The only way to distinguish is from the value of the
		   event->kb_ooo_reply field.
		 */
		NLXDBGP(ktm, 1,
			"\t%p: LNetGet() SEND with unlinked: thr:%d ooo:%d\n",
			ktm, event->md.threshold, (int) kbp->kb_ooo_reply);
		if (status == 0) {
			if (!kbp->kb_ooo_reply)
				status = -ECANCELED;
			else {  /* from earlier REPLY */
				mlength = kbp->kb_ooo_mlength;
				offset  = kbp->kb_ooo_offset;
				status  = kbp->kb_ooo_status;
			}
		}
	} else if (event->type == LNET_EVENT_UNLINK) {/* see nlx_core_buf_del */
		M0_ASSERT(is_unlinked);
		status = -ECANCELED;
	} else if (!is_unlinked) {
		NLXDBGP(ktm, 1, "\t%p: eq_cb: %p %s !unlinked Q=%d\n", ktm,
			event, nlx_kcore_lnet_event_type_to_string(event->type),
			kbp->kb_qtype);
		/* We may get REPLY before SEND, so ignore such events,
		   but save the significant values for when the SEND arrives.
		 */
		if (kbp->kb_qtype == M0_NET_QT_ACTIVE_BULK_RECV) {
			kbp->kb_ooo_reply   = true;
			kbp->kb_ooo_mlength = mlength;
			kbp->kb_ooo_offset  = offset;
			kbp->kb_ooo_status  = status;
			return;
		}
		/* we don't expect anything other than receive messages */
		M0_ASSERT(kbp->kb_qtype == M0_NET_QT_MSG_RECV);
	}

	spin_lock(&ktm->ktm_bevq_lock);
	ctm = nlx_kcore_core_tm_map_atomic(ktm);
	ql = bev_cqueue_pnext(&ctm->ctm_bevq);
	bev = container_of(ql, struct nlx_core_buffer_event, cbe_tm_link);
	bev->cbe_buffer_id = kbp->kb_buffer_id;
	bev->cbe_time      = m0_time_sub(now, kbp->kb_add_time);
	bev->cbe_status    = status;
	bev->cbe_length    = mlength;
	bev->cbe_offset    = offset;
	bev->cbe_unlinked  = is_unlinked;
	if (event->hdr_data != 0) {
		bev->cbe_sender.cepa_nid = event->initiator.nid;
		bev->cbe_sender.cepa_pid = event->initiator.pid;
		nlx_kcore_hdr_data_decode(event->hdr_data,
					  &bev->cbe_sender.cepa_portal,
					  &bev->cbe_sender.cepa_tmid);
	} else
		M0_SET0(&bev->cbe_sender);

	/* Reset in spinlock to synchronize with driver nlx_dev_tm_cleanup() */
	if (is_unlinked)
		kbp->kb_ktm = NULL;
	bev_cqueue_put(&ctm->ctm_bevq, ql);
	nlx_kcore_core_tm_unmap_atomic(ctm);
	spin_unlock(&ktm->ktm_bevq_lock);

	wake_up(&ktm->ktm_wq);
}

/**
   Set a memory location reference, including checksum.
   @pre off < PAGE_SIZE && ergo(pg == NULL, off == 0)
   @param loc Location to set.
   @param pg Pointer to page object.
   @param off Offset within the page.
 */
static void nlx_core_kmem_loc_set(struct nlx_core_kmem_loc *loc,
				  struct page *pg, uint32_t off)
{
	M0_PRE(off < PAGE_SIZE && ergo(pg == NULL, off == 0));
	M0_PRE(loc != NULL);

	loc->kl_page = pg;
	loc->kl_offset = off;
	loc->kl_checksum = nlx_core_kmem_loc_checksum(loc);
}

M0_INTERNAL void *nlx_core_mem_alloc(size_t size, unsigned shift)
{
	return m0_alloc(size);
}

M0_INTERNAL void nlx_core_mem_free(void *data, size_t size, unsigned shift)
{
	m0_free(data);
}

/**
   Initializes the core private data given a previously initialized
   kernel core private data object.
   @see nlx_kcore_kcore_dom_init()
   @pre nlx_kcore_domain_invariant(kd) &&
   !nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc)
   @param kd Kernel core private data pointer.
   @param cd Core private data pointer.
 */
static int nlx_kcore_core_dom_init(struct nlx_kcore_domain *kd,
				   struct nlx_core_domain *cd)
{
	M0_PRE(kd != NULL && cd != NULL);
	M0_PRE(nlx_kcore_domain_invariant(kd));
	M0_PRE(!nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc));
	cd->cd_kpvt = kd;
	return 0;
}

M0_INTERNAL int nlx_core_dom_init(struct m0_net_domain *dom,
				  struct nlx_core_domain *cd)
{
	struct nlx_kcore_domain *kd;
	int rc;

	M0_PRE(dom != NULL && cd != NULL);
	NLX_ALLOC_PTR(kd);
	if (kd == NULL)
		return M0_ERR(-ENOMEM);
	rc = nlx_kcore_kcore_dom_init(kd);
	if (rc != 0)
		goto fail_free_kd;
	nlx_core_kmem_loc_set(&kd->kd_cd_loc, virt_to_page(cd),
			      NLX_PAGE_OFFSET((unsigned long) cd));
	rc = nlx_kcore_core_dom_init(kd, cd);
	if (rc == 0)
		return 0;

	/* failed */
	nlx_kcore_kcore_dom_fini(kd);
fail_free_kd:
	m0_free(kd);
	M0_ASSERT(rc != 0);
	return M0_RC(rc);
}

/**
   Finilizes the core private data associated with a kernel core
   private data object.
   @param kd Kernel core private data pointer.
   @param cd Core private data pointer.
 */
static void nlx_kcore_core_dom_fini(struct nlx_kcore_domain *kd,
				    struct nlx_core_domain *cd)
{
	M0_PRE(cd != NULL && cd->cd_kpvt == kd);
	cd->cd_kpvt = NULL;
}

M0_INTERNAL void nlx_core_dom_fini(struct nlx_core_domain *cd)
{
	struct nlx_kcore_domain *kd;

	M0_PRE(cd != NULL);
	kd = cd->cd_kpvt;
	M0_PRE(nlx_kcore_domain_invariant(kd));
	nlx_kcore_core_dom_fini(kd, cd);
	nlx_core_kmem_loc_set(&kd->kd_cd_loc, NULL, 0);
	nlx_kcore_kcore_dom_fini(kd);
	m0_free(kd);
}

M0_INTERNAL m0_bcount_t nlx_core_get_max_buffer_size(struct nlx_core_domain
						     *lcdom)
{
	return LNET_MAX_PAYLOAD;
}

M0_INTERNAL m0_bcount_t nlx_core_get_max_buffer_segment_size(struct
							     nlx_core_domain
							     *lcdom)
{
	/* PAGE_SIZE limit applies only when LNET_MD_KIOV has been set in
	 * lnet_md_t::options. There's no such limit in MD fragment size when
	 * LNET_MD_IOVEC is set.  DLD calls for only LNET_MD_KIOV to be used.
	 */
	return PAGE_SIZE;
}

M0_INTERNAL int32_t nlx_core_get_max_buffer_segments(struct nlx_core_domain
						     *lcdom)
{
	return LNET_MAX_IOV;
}

/**
   Performs common kernel core tasks related to registering a network
   buffer.  The nlx_kcore_buffer::kb_kiov is @b not set.
   @param kd Kernel core private domain pointer.
   @param buffer_id Value to set in the cb_buffer_id field.
   @param cb The core private data pointer for the buffer.
   @param kb Kernel core private buffer pointer.
 */
static int nlx_kcore_buf_register(struct nlx_kcore_domain *kd,
				  nlx_core_opaque_ptr_t buffer_id,
				  struct nlx_core_buffer *cb,
				  struct nlx_kcore_buffer *kb)
{
	M0_PRE(nlx_kcore_domain_invariant(kd));
	drv_bufs_tlink_init(kb);
	kb->kb_ktm           = NULL;
	kb->kb_buffer_id     = buffer_id;
	kb->kb_kiov          = NULL;
	kb->kb_kiov_len      = 0;
	kb->kb_kiov_orig_len = 0;
	LNetInvalidateMDHandle(&kb->kb_mdh);
	kb->kb_ooo_reply     = false;
	kb->kb_ooo_mlength   = 0;
	kb->kb_ooo_status    = 0;
	kb->kb_ooo_offset    = 0;

	cb->cb_kpvt          = kb;
	cb->cb_buffer_id     = buffer_id;
	cb->cb_magic         = M0_NET_LNET_CORE_BUF_MAGIC;

	M0_POST(nlx_kcore_buffer_invariant(kb));
	return 0;
}

/**
   Performs common kernel core tasks related to de-registering a buffer.
   @param cb The core private data pointer for the buffer.
   @param kb Kernel core private buffer pointer.
 */
static void nlx_kcore_buf_deregister(struct nlx_core_buffer *cb,
				     struct nlx_kcore_buffer *kb)
{
	M0_PRE(nlx_kcore_buffer_invariant(kb));
	M0_PRE(LNetMDHandleIsInvalid(kb->kb_mdh));
	drv_bufs_tlink_fini(kb);
	kb->kb_magic = 0;
	m0_free(kb->kb_kiov);
	cb->cb_buffer_id = 0;
	cb->cb_kpvt = NULL;
	cb->cb_magic = 0;
}

M0_INTERNAL int nlx_core_buf_register(struct nlx_core_domain *cd,
				      nlx_core_opaque_ptr_t buffer_id,
				      const struct m0_bufvec *bvec,
				      struct nlx_core_buffer *cb)
{
	int rc;
	struct nlx_kcore_buffer *kb;
	struct nlx_kcore_domain *kd;

	M0_PRE(cb != NULL && cb->cb_kpvt == NULL);
	M0_PRE(cd != NULL);
	kd = cd->cd_kpvt;
	NLX_ALLOC_PTR(kb);
	if (kb == NULL)
		return M0_ERR(-ENOMEM);
	nlx_core_kmem_loc_set(&kb->kb_cb_loc, virt_to_page(cb),
			      NLX_PAGE_OFFSET((unsigned long) cb));
	rc = nlx_kcore_buf_register(kd, buffer_id, cb, kb);
	if (rc != 0)
		goto fail_free_kb;
	rc = nlx_kcore_buffer_kla_to_kiov(kb, bvec);
	if (rc != 0)
		goto fail_buf_registered;
	M0_ASSERT(kb->kb_kiov != NULL && kb->kb_kiov_len > 0);
	M0_POST(nlx_kcore_buffer_invariant(cb->cb_kpvt));
	return 0;

fail_buf_registered:
	nlx_kcore_buf_deregister(cb, kb);
fail_free_kb:
	m0_free(kb);
	M0_ASSERT(rc != 0);
	return M0_RC(rc);
}

M0_INTERNAL void nlx_core_buf_deregister(struct nlx_core_domain *cd,
					 struct nlx_core_buffer *cb)
{
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;
	nlx_kcore_buf_deregister(cb, kb);
	m0_free(kb);
}

/**
   Performs kernel core tasks relating to adding a buffer to
   the message receive queue.
   @param ktm The kernel transfer machine private data.
   @param cb The buffer private data.
   @param kb The kernel buffer private data.
 */
static int nlx_kcore_buf_msg_recv(struct nlx_kcore_transfer_mc *ktm,
				  struct nlx_core_buffer *cb,
				  struct nlx_kcore_buffer *kb)
{
	lnet_md_t umd;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(nlx_core_buffer_invariant(cb));
	M0_PRE(nlx_kcore_buffer_invariant(kb));
	M0_PRE(cb->cb_qtype == M0_NET_QT_MSG_RECV);
	M0_PRE(cb->cb_length > 0);
	M0_PRE(cb->cb_min_receive_size <= cb->cb_length);
	M0_PRE(cb->cb_max_operations > 0);

	nlx_kcore_umd_init(ktm, cb, kb, cb->cb_max_operations,
			   cb->cb_min_receive_size, LNET_MD_OP_PUT,
			   false, &umd);
	cb->cb_match_bits =
		nlx_core_match_bits_encode(ktm->ktm_addr.cepa_tmid, 0);
	cb->cb_addr = ktm->ktm_addr;
	rc = NLX_kcore_LNetMDAttach(ktm, cb, kb, &umd);
	return M0_RC(rc);
}

M0_INTERNAL int nlx_core_buf_msg_recv(struct nlx_core_domain *cd,	/* not used */
				      struct nlx_core_transfer_mc *ctm,
				      struct nlx_core_buffer *cb)
{
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;
	return nlx_kcore_buf_msg_recv(ktm, cb, kb);
}

/**
   Performs kernel core tasks relating to adding a buffer to
   the message receive queue.
   @param ktm The kernel transfer machine private data.
   @param cb The buffer private data.
   @param kb The kernel buffer private data.
 */
static int nlx_kcore_buf_msg_send(struct nlx_kcore_transfer_mc *ktm,
				  struct nlx_core_buffer *cb,
				  struct nlx_kcore_buffer *kb)
{
	lnet_md_t umd;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(nlx_core_buffer_invariant(cb));
	M0_PRE(nlx_kcore_buffer_invariant(kb));
	M0_PRE(cb->cb_qtype == M0_NET_QT_MSG_SEND);
	M0_PRE(cb->cb_length > 0);
	M0_PRE(cb->cb_max_operations == 1);

	nlx_kcore_umd_init(ktm, cb, kb, 1, 0, 0, false, &umd);
	nlx_kcore_kiov_adjust_length(ktm, kb, &umd, cb->cb_length);
	cb->cb_match_bits =
		nlx_core_match_bits_encode(cb->cb_addr.cepa_tmid, 0);
	rc = NLX_kcore_LNetPut(ktm, cb, kb, &umd);
	nlx_kcore_kiov_restore_length(kb);
	return M0_RC(rc);
}

M0_INTERNAL int nlx_core_buf_msg_send(struct nlx_core_domain *cd,	/* not used */
				      struct nlx_core_transfer_mc *ctm,
				      struct nlx_core_buffer *cb)
{
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;
	return nlx_kcore_buf_msg_send(ktm, cb, kb);
}

/**
   Performs kernel core tasks relating to adding a buffer to
   the bulk active receive queue.
   @param ktm The kernel transfer machine private data.
   @param cb The buffer private data.
   @param kb The kernel buffer private data.
 */
static int nlx_kcore_buf_active_recv(struct nlx_kcore_transfer_mc *ktm,
				     struct nlx_core_buffer *cb,
				     struct nlx_kcore_buffer *kb)
{
	uint32_t tmid;
	uint64_t counter;
	lnet_md_t umd;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(nlx_core_buffer_invariant(cb));
	M0_PRE(nlx_kcore_buffer_invariant(kb));
	M0_PRE(cb->cb_qtype == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_PRE(cb->cb_length > 0);
	M0_PRE(cb->cb_max_operations == 1);

	M0_PRE(cb->cb_match_bits > 0);
	nlx_core_match_bits_decode(cb->cb_match_bits, &tmid, &counter);
	M0_PRE(tmid == cb->cb_addr.cepa_tmid);
	M0_PRE(counter >= M0_NET_LNET_BUFFER_ID_MIN);
	M0_PRE(counter <= M0_NET_LNET_BUFFER_ID_MAX);

	nlx_kcore_umd_init(ktm, cb, kb, 1, 0, 0, true, &umd);
	nlx_kcore_kiov_adjust_length(ktm, kb, &umd, cb->cb_length);
	rc = NLX_kcore_LNetGet(ktm, cb, kb, &umd);
	nlx_kcore_kiov_restore_length(kb);
	return M0_RC(rc);
}

M0_INTERNAL int nlx_core_buf_active_recv(struct nlx_core_domain *cd,	/* not used */
					 struct nlx_core_transfer_mc *ctm,
					 struct nlx_core_buffer *cb)
{
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;
	return nlx_kcore_buf_active_recv(ktm, cb, kb);
}

/**
   Performs kernel core tasks relating to adding a buffer to
   the bulk active send queue.
   @param ktm The kernel transfer machine private data.
   @param cb The buffer private data.
   @param kb The kernel buffer private data.
 */
static int nlx_kcore_buf_active_send(struct nlx_kcore_transfer_mc *ktm,
				     struct nlx_core_buffer *cb,
				     struct nlx_kcore_buffer *kb)
{
	uint32_t tmid;
	uint64_t counter;
	lnet_md_t umd;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(nlx_core_buffer_invariant(cb));
	M0_PRE(nlx_kcore_buffer_invariant(kb));
	M0_PRE(cb->cb_qtype == M0_NET_QT_ACTIVE_BULK_SEND);
	M0_PRE(cb->cb_length > 0);
	M0_PRE(cb->cb_max_operations == 1);

	M0_PRE(cb->cb_match_bits > 0);
	nlx_core_match_bits_decode(cb->cb_match_bits, &tmid, &counter);
	M0_PRE(tmid == cb->cb_addr.cepa_tmid);
	M0_PRE(counter >= M0_NET_LNET_BUFFER_ID_MIN);
	M0_PRE(counter <= M0_NET_LNET_BUFFER_ID_MAX);

	nlx_kcore_umd_init(ktm, cb, kb, 1, 0, 0, false, &umd);
	nlx_kcore_kiov_adjust_length(ktm, kb, &umd, cb->cb_length);
	rc = NLX_kcore_LNetPut(ktm, cb, kb, &umd);
	nlx_kcore_kiov_restore_length(kb);
	return M0_RC(rc);
}

M0_INTERNAL int nlx_core_buf_active_send(struct nlx_core_domain *cd,	/* not used */
					 struct nlx_core_transfer_mc *ctm,
					 struct nlx_core_buffer *cb)
{
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;
	return nlx_kcore_buf_active_send(ktm, cb, kb);
}

/**
   Performs kernel core tasks relating to adding a buffer to
   the bulk passive receive queue.
   @param ktm The kernel transfer machine private data.
   @param cb The buffer private data.
   @param kb The kernel buffer private data.
 */
static int nlx_kcore_buf_passive_recv(struct nlx_kcore_transfer_mc *ktm,
				      struct nlx_core_buffer *cb,
				      struct nlx_kcore_buffer *kb)
{
	uint32_t tmid;
	uint64_t counter;
	lnet_md_t umd;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(nlx_core_buffer_invariant(cb));
	M0_PRE(nlx_kcore_buffer_invariant(kb));
	M0_PRE(cb->cb_qtype == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_PRE(cb->cb_length > 0);
	M0_PRE(cb->cb_max_operations == 1);
	M0_PRE(cb->cb_match_bits > 0);

	nlx_core_match_bits_decode(cb->cb_match_bits, &tmid, &counter);
	M0_PRE(tmid == ktm->ktm_addr.cepa_tmid);
	M0_PRE(counter >= M0_NET_LNET_BUFFER_ID_MIN);
	M0_PRE(counter <= M0_NET_LNET_BUFFER_ID_MAX);

	nlx_kcore_umd_init(ktm, cb, kb, 1, 0, LNET_MD_OP_PUT, false, &umd);
	cb->cb_addr = ktm->ktm_addr;
	rc = NLX_kcore_LNetMDAttach(ktm, cb, kb, &umd);
	return M0_RC(rc);
}

M0_INTERNAL int nlx_core_buf_passive_recv(struct nlx_core_domain *cd,	/* not used */
					  struct nlx_core_transfer_mc *ctm,
					  struct nlx_core_buffer *cb)
{
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;
	return nlx_kcore_buf_passive_recv(ktm, cb, kb);
}

/**
   Performs kernel core tasks relating to adding a buffer to
   the bulk passive send queue.
   @param ktm The kernel transfer machine private data.
   @param cb The buffer private data.
   @param kb The kernel buffer private data.
 */
static int nlx_kcore_buf_passive_send(struct nlx_kcore_transfer_mc *ktm,
				      struct nlx_core_buffer *cb,
				      struct nlx_kcore_buffer *kb)
{
	uint32_t tmid;
	uint64_t counter;
	lnet_md_t umd;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(nlx_core_buffer_invariant(cb));
	M0_PRE(nlx_kcore_buffer_invariant(kb));
	M0_PRE(cb->cb_qtype == M0_NET_QT_PASSIVE_BULK_SEND);
	M0_PRE(cb->cb_length > 0);
	M0_PRE(cb->cb_max_operations == 1);
	M0_PRE(cb->cb_match_bits > 0);

	nlx_core_match_bits_decode(cb->cb_match_bits, &tmid, &counter);
	M0_PRE(tmid == ktm->ktm_addr.cepa_tmid);
	M0_PRE(counter >= M0_NET_LNET_BUFFER_ID_MIN);
	M0_PRE(counter <= M0_NET_LNET_BUFFER_ID_MAX);

	nlx_kcore_umd_init(ktm, cb, kb, 1, 0, LNET_MD_OP_GET, false, &umd);
	nlx_kcore_kiov_adjust_length(ktm, kb, &umd, cb->cb_length);
	cb->cb_addr = ktm->ktm_addr;
	rc = NLX_kcore_LNetMDAttach(ktm, cb, kb, &umd);
	nlx_kcore_kiov_restore_length(kb);
	return M0_RC(rc);
}

M0_INTERNAL int nlx_core_buf_passive_send(struct nlx_core_domain *cd,	/* not used */
					  struct nlx_core_transfer_mc *ctm,
					  struct nlx_core_buffer *cb)
{
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;
	return nlx_kcore_buf_passive_send(ktm, cb, kb);
}

M0_INTERNAL int nlx_core_buf_del(struct nlx_core_domain *cd,	/* not used */
				 struct nlx_core_transfer_mc *ctm,
				 struct nlx_core_buffer *cb)
{
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	M0_PRE(nlx_core_buffer_invariant(cb));
	kb = cb->cb_kpvt;

	/* Subtle: Cancelling the MD associated with the buffer
	   could result in a LNet UNLINK event if the buffer operation is
	   terminated by LNet.
	   The unlink bit is also set in other LNet events but does not
	   signify cancel in those cases.
	 */
	return nlx_kcore_LNetMDUnlink(ktm, kb);
}

/**
   Performs common kernel core tasks to wait for buffer events.
   @param ctm The transfer machine private data.
   @param ktm The kernel transfer machine private data.
   @param timeout Absolute time at which to stop waiting.
 */
static int nlx_kcore_buf_event_wait(struct nlx_core_transfer_mc *ctm,
				    struct nlx_kcore_transfer_mc *ktm,
				    m0_time_t timeout)
{
	int             rc;
	struct timespec ts;
	m0_time_t       now;

	M0_PRE(nlx_core_tm_invariant(ctm));
	M0_PRE(nlx_kcore_tm_invariant(ktm));

	if (!bev_cqueue_is_empty(&ctm->ctm_bevq))
		return M0_RC(0);
	else if (timeout == 0)
		return M0_RC(-ETIMEDOUT);

	now = m0_time_now();
	timeout = timeout > now ? m0_time_sub(timeout, now) : 0;
	ts.tv_sec  = m0_time_seconds(timeout);
	ts.tv_nsec = m0_time_nanoseconds(timeout);

	rc = wait_event_interruptible_timeout(ktm->ktm_wq,
		!bev_cqueue_is_empty(&ctm->ctm_bevq), timespec_to_jiffies(&ts));

	return M0_RC(rc == 0 ? -ETIMEDOUT : rc < 0 ? rc : 0);
}

M0_INTERNAL int nlx_core_buf_event_wait(struct nlx_core_domain *cd,
					struct nlx_core_transfer_mc *ctm,
					m0_time_t timeout)
{
	struct nlx_kcore_transfer_mc *ktm;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	return nlx_kcore_buf_event_wait(ctm, ktm, timeout);
}

/**
   Decodes a NID string into a NID.
   @param nidstr the string to be decoded.
   @param nid On success, the resulting NID is returned here.
   @retval -EINVAL the NID string could not be decoded.
 */
static int nlx_kcore_nidstr_decode(const char *nidstr, uint64_t *nid)
{
	*nid = libcfs_str2nid(nidstr);
	if (*nid == LNET_NID_ANY)
		return M0_ERR_INFO(-EINVAL, "nidstr=%s", nidstr);
	return 0;
}

M0_INTERNAL int nlx_core_nidstr_decode(struct nlx_core_domain *lcdom,	/* not used */
				       const char *nidstr, uint64_t * nid)
{
	return nlx_kcore_nidstr_decode(nidstr, nid);
}

/**
   Encode a NID into its string representation.
   @param nid The NID to be converted.
   @param nidstr On success, the string form is set here.
 */
static int nlx_kcore_nidstr_encode(uint64_t nid,
				   char nidstr[M0_NET_LNET_NIDSTR_SIZE])
{
	const char *cp = libcfs_nid2str(nid);

	M0_ASSERT(cp != NULL && *cp != 0);
	strncpy(nidstr, cp, M0_NET_LNET_NIDSTR_SIZE - 1);
	nidstr[M0_NET_LNET_NIDSTR_SIZE - 1] = 0;
	return 0;
}

M0_INTERNAL int nlx_core_nidstr_encode(struct nlx_core_domain *lcdom,	/* not used */
				       uint64_t nid,
				       char nidstr[M0_NET_LNET_NIDSTR_SIZE])
{
	return nlx_kcore_nidstr_encode(nid, nidstr);
}

/**
   Gets a list of strings corresponding to the local LNET network interfaces.
   The returned array must be released using nlx_core_nidstrs_put().
   @param nidary A NULL-terminated (like argv) array of NID strings is returned.
 */
static int nlx_kcore_nidstrs_get(char * const **nidary)
{
	M0_PRE(nlx_kcore_lni_nidstrs != NULL);
	*nidary = nlx_kcore_lni_nidstrs;
	m0_atomic64_inc(&nlx_kcore_lni_refcount);
	return 0;
}

M0_INTERNAL int nlx_core_nidstrs_get(struct nlx_core_domain *lcdom,
				     char *const **nidary)
{
	return nlx_kcore_nidstrs_get(nidary);
}

/**
   Kernel helper to release the strings returned by nlx_kcore_nidstrs_get().
 */
M0_INTERNAL void nlx_kcore_nidstrs_put(char *const **nidary)
{
	M0_PRE(*nidary == nlx_kcore_lni_nidstrs);
	M0_PRE(m0_atomic64_get(&nlx_kcore_lni_refcount) > 0);
	m0_atomic64_dec(&nlx_kcore_lni_refcount);
	*nidary = NULL;
}

M0_INTERNAL void nlx_core_nidstrs_put(struct nlx_core_domain *lcdom,
				      char *const **nidary)
{
	nlx_kcore_nidstrs_put(nidary);
}

M0_INTERNAL int nlx_core_new_blessed_bev(struct nlx_core_domain *cd,
					 struct nlx_core_transfer_mc *ctm,
					 struct nlx_core_buffer_event **bevp)
{
	struct nlx_core_buffer_event *bev;
	struct nlx_kcore_transfer_mc *ktm;

	ktm = ctm->ctm_kpvt;
	M0_ASSERT(nlx_kcore_tm_invariant(ktm));

	NLX_ALLOC_ALIGNED_PTR(bev);
	if (bev == NULL) {
		*bevp = NULL;
		return M0_ERR(-ENOMEM);
	}
	bev_link_bless(&bev->cbe_tm_link, virt_to_page(&bev->cbe_tm_link));
	*bevp = bev;
	return 0;
}

/**
   Performs kernel core tasks relating to stopping a transfer machine.
   Kernel resources are released.
   @param ctm The transfer machine private data.
   @param ktm The kernel transfer machine private data.
 */
static void nlx_kcore_tm_stop(struct nlx_core_transfer_mc *ctm,
			      struct nlx_kcore_transfer_mc *ktm)
{
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(drv_bevs_tlist_is_empty(&ktm->ktm_drv_bevs));

	rc = LNetEQFree(ktm->ktm_eqh);
	M0_ASSERT(rc == 0);

	m0_mutex_lock(&nlx_kcore_mutex);
	tms_tlist_del(ktm);
	m0_mutex_unlock(&nlx_kcore_mutex);
	drv_bevs_tlist_fini(&ktm->ktm_drv_bevs);
	drv_tms_tlink_fini(ktm);
	tms_tlink_fini(ktm);
	ktm->ktm_magic = 0;
	/* allow kernel cleanup even if core is invalid */
	if (nlx_core_tm_invariant(ctm))
		ctm->ctm_kpvt = NULL;
}

M0_INTERNAL void nlx_core_tm_stop(struct nlx_core_domain *cd,
				  struct nlx_core_transfer_mc *ctm)
{
	struct nlx_kcore_transfer_mc *ktm;

	M0_PRE(nlx_core_tm_invariant(ctm));
	ktm = ctm->ctm_kpvt;
	nlx_kcore_tm_stop(ctm, ktm);
	bev_cqueue_fini(&ctm->ctm_bevq, nlx_core_bev_free_cb);
	m0_free(ktm);
}

/**
   Performs kernel core tasks related to starting a transfer machine.
   Internally this results in the creation of the LNet EQ associated
   with the transfer machine.
   @param kd The kernel domain for this transfer machine.
   @param ctm The transfer machine private data to be initialized.
   The nlx_core_transfer_mc::ctm_addr must be set by the caller.  If the
   lcpea_tmid field value is M0_NET_LNET_TMID_INVALID then a transfer
   machine identifier is dynamically assigned to the transfer machine
   and the nlx_core_transfer_mc::ctm_addr is modified in place.
   @param ktm The kernel transfer machine private data to be initialized.
 */
static int nlx_kcore_tm_start(struct nlx_kcore_domain      *kd,
			      struct nlx_core_transfer_mc  *ctm,
			      struct nlx_kcore_transfer_mc *ktm)
{
	struct nlx_core_ep_addr *cepa;
	lnet_process_id_t id;
	int rc;
	int i;

	M0_ENTRY();
	M0_PRE(kd != NULL && ctm != NULL && ktm != NULL);
	cepa = &ctm->ctm_addr;

	/*
	 * cepa_nid/cepa_pid must match a local NID/PID.
	 * cepa_portal must be in range.  cepa_tmid is checked below.
	 */
	if (cepa->cepa_portal == LNET_RESERVED_PORTAL ||
	    cepa->cepa_portal >= M0_NET_LNET_MAX_PORTALS) {
		M0_LOG(M0_ERROR, "cepa_portal=%"PRIu32" "
		       "LNET_RESERVED_PORTAL=%d M0_NET_LNET_MAX_PORTALS=%d",
		       cepa->cepa_portal, LNET_RESERVED_PORTAL,
		       M0_NET_LNET_MAX_PORTALS);
		rc = -EINVAL;
		goto fail;
	}
	for (i = 0; i < nlx_kcore_lni_nr; ++i) {
		rc = LNetGetId(i, &id);
		M0_ASSERT(rc == 0);
		if (id.nid == cepa->cepa_nid && id.pid == cepa->cepa_pid)
			break;
	}
	if (i == nlx_kcore_lni_nr) {
		rc = M0_ERR(-ENOENT);
		goto fail;
	}

	rc = LNetEQAlloc(M0_NET_LNET_EQ_SIZE, nlx_kcore_eq_cb, &ktm->ktm_eqh);
	if (rc < 0) {
		M0_LOG(M0_ERROR, "LNetEQAlloc() failed: rc=%d", rc);
		goto fail;
	}

	m0_mutex_lock(&nlx_kcore_mutex);
	if (cepa->cepa_tmid == M0_NET_LNET_TMID_INVALID) {
		rc = nlx_kcore_max_tmid_find(cepa);
		if (rc < 0) {
			m0_mutex_unlock(&nlx_kcore_mutex);
			goto fail_with_eq;
		}
		cepa->cepa_tmid = rc;
	} else if (cepa->cepa_tmid > M0_NET_LNET_TMID_MAX) {
		m0_mutex_unlock(&nlx_kcore_mutex);
		rc = M0_ERR(-EINVAL);
		goto fail_with_eq;
	} else if (nlx_kcore_addr_in_use(cepa)) {
		m0_mutex_unlock(&nlx_kcore_mutex);
		rc = M0_ERR(-EADDRINUSE);
		goto fail_with_eq;
	}
	ktm->ktm_addr = *cepa;
	tms_tlink_init(ktm);
	nlx_kcore_tms_list_add(ktm);
	m0_mutex_unlock(&nlx_kcore_mutex);

	drv_tms_tlink_init(ktm);
	drv_bevs_tlist_init(&ktm->ktm_drv_bevs);
	ctm->ctm_mb_counter = M0_NET_LNET_BUFFER_ID_MIN;
	spin_lock_init(&ktm->ktm_bevq_lock);
	init_waitqueue_head(&ktm->ktm_wq);
	ktm->_debug_ = ctm->_debug_;
	ctm->ctm_kpvt = ktm;
	ctm->ctm_magic = M0_NET_LNET_CORE_TM_MAGIC;
	M0_POST(nlx_kcore_tm_invariant(ktm));
	return 0;

fail_with_eq:
	i = LNetEQFree(ktm->ktm_eqh);
	M0_ASSERT(i == 0);
fail:
	M0_ASSERT(rc != 0);
	return M0_ERR(rc);
}

M0_INTERNAL int nlx_core_tm_start(struct nlx_core_domain *cd,
				  struct m0_net_transfer_mc *tm,
				  struct nlx_core_transfer_mc *ctm)
{
	struct nlx_kcore_domain *kd;
	struct nlx_core_buffer_event *e1;
	struct nlx_core_buffer_event *e2;
	struct nlx_kcore_transfer_mc *ktm;
	int rc;

	M0_PRE(m0_mutex_is_locked(&tm->ntm_mutex));
	M0_PRE(nlx_tm_invariant(tm));
	M0_PRE(cd != NULL);
	kd = cd->cd_kpvt;
	M0_PRE(nlx_kcore_domain_invariant(kd));

	NLX_ALLOC_PTR(ktm);
	if (ktm == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto fail_ktm;
	}

	nlx_core_kmem_loc_set(&ktm->ktm_ctm_loc, virt_to_page(ctm),
			      NLX_PAGE_OFFSET((unsigned long) ctm));
	rc = nlx_kcore_tm_start(kd, ctm, ktm);
	if (rc != 0)
		goto fail_ktm;

	ctm->ctm_upvt = NULL;
	rc = nlx_core_new_blessed_bev(cd, ctm, &e1);
	if (rc == 0)
		rc = nlx_core_new_blessed_bev(cd, ctm, &e2);
	if (rc != 0)
		goto fail_blessed_bev;
	M0_ASSERT(e1 != NULL && e2 != NULL);
	bev_cqueue_init(&ctm->ctm_bevq, &e1->cbe_tm_link, &e2->cbe_tm_link);
	M0_ASSERT(bev_cqueue_is_empty(&ctm->ctm_bevq));
	return 0;

 fail_blessed_bev:
	if (e1 != NULL)
		nlx_core_bev_free_cb(&e1->cbe_tm_link);
	nlx_kcore_tm_stop(ctm, ktm);
 fail_ktm:
	m0_free(ktm);
	M0_ASSERT(rc != 0);
	return M0_RC(rc);
}

static void nlx_core_fini(void)
{
	int rc;
	int i;

	M0_ASSERT(m0_atomic64_get(&nlx_kcore_lni_refcount) == 0);
	nlx_dev_fini();
	if (nlx_kcore_lni_nidstrs != NULL) {
		for (i = 0; nlx_kcore_lni_nidstrs[i] != NULL; ++i)
			m0_free(nlx_kcore_lni_nidstrs[i]);
		m0_free0(&nlx_kcore_lni_nidstrs);
	}
	nlx_kcore_lni_nr = 0;
	tms_tlist_fini(&nlx_kcore_tms);
	m0_mutex_fini(&nlx_kcore_mutex);
	rc = LNetNIFini();
	M0_ASSERT(rc == 0);
}

static int nlx_core_init(void)
{
	int rc;
	int i;
	lnet_process_id_t id;
	const char *nidstr;
	/*
	 * Temporarily reset current->journal_info, because LNetNIInit assumes
	 * it is NULL.
	 */
	struct m0_thread_tls *tls = m0_thread_tls_pop();

	/*
	 * Init LNet with same PID as Lustre would use in case we are first.
	 * Depending on the lustre version, the PID symbol may be called
	 * LUSTRE_SRV_LNET_PID or LNET_PID_LUSTRE.
	 */
#ifdef LNET_PID_LUSTRE
	rc = LNetNIInit(LNET_PID_LUSTRE);
#else
	rc = LNetNIInit(LUSTRE_SRV_LNET_PID);
#endif
	m0_thread_tls_back(tls);
	if (rc < 0)
		return M0_RC(rc);

	m0_mutex_init(&nlx_kcore_mutex);
	tms_tlist_init(&nlx_kcore_tms);

	m0_atomic64_set(&nlx_kcore_lni_refcount, 0);
	for (i = 0, rc = 0; rc != -ENOENT; ++i)
		rc = LNetGetId(i, &id);
	M0_ALLOC_ARR(nlx_kcore_lni_nidstrs, i);
	if (nlx_kcore_lni_nidstrs == NULL) {
		nlx_core_fini();
		return M0_ERR(-ENOMEM);
	}
	nlx_kcore_lni_nr = i - 1;
	for (i = 0; i < nlx_kcore_lni_nr; ++i) {
		rc = LNetGetId(i, &id);
		M0_ASSERT(rc == 0);
		nidstr = libcfs_nid2str(id.nid);
		M0_ASSERT(nidstr != NULL);
		nlx_kcore_lni_nidstrs[i] = m0_alloc(strlen(nidstr) + 1);
		if (nlx_kcore_lni_nidstrs[i] == NULL) {
			nlx_core_fini();
			return M0_ERR(-ENOMEM);
		}
		strcpy(nlx_kcore_lni_nidstrs[i], nidstr);
	}

	rc = nlx_dev_init();
	if (rc != 0)
		nlx_core_fini();

	return M0_RC(rc);
}

static struct nlx_kcore_ops nlx_kcore_def_ops = {
	.ko_dom_init = nlx_kcore_core_dom_init,
	.ko_dom_fini = nlx_kcore_core_dom_fini,
	.ko_buf_register = nlx_kcore_buf_register,
	.ko_buf_deregister = nlx_kcore_buf_deregister,
	.ko_tm_start = nlx_kcore_tm_start,
	.ko_tm_stop = nlx_kcore_tm_stop,
	.ko_buf_msg_recv = nlx_kcore_buf_msg_recv,
	.ko_buf_msg_send = nlx_kcore_buf_msg_send,
	.ko_buf_active_recv = nlx_kcore_buf_active_recv,
	.ko_buf_active_send = nlx_kcore_buf_active_send,
	.ko_buf_passive_recv = nlx_kcore_buf_passive_recv,
	.ko_buf_passive_send = nlx_kcore_buf_passive_send,
	.ko_buf_del = nlx_kcore_LNetMDUnlink,
	.ko_buf_event_wait = nlx_kcore_buf_event_wait,
};

/**
   Initializes the kernel core domain private data object.
   The nlx_kcore_domain::kd_cd_loc is empty on return, denoting the initial
   state of the domain.  The caller must set this field before calling
   nlx_kcore_core_dom_init().
   @see nlx_kcore_core_dom_init()
   @post nlx_kcore_domain_invariant(kd)
   @param kd kernel core private data pointer for the domain to be initialized.
 */
static int nlx_kcore_kcore_dom_init(struct nlx_kcore_domain *kd)
{
	M0_PRE(kd != NULL);
	kd->kd_magic = M0_NET_LNET_KCORE_DOM_MAGIC;
	nlx_core_kmem_loc_set(&kd->kd_cd_loc, NULL, 0);
	m0_mutex_init(&kd->kd_drv_mutex);
	kd->kd_drv_ops = &nlx_kcore_def_ops;
	drv_tms_tlist_init(&kd->kd_drv_tms);
	drv_bufs_tlist_init(&kd->kd_drv_bufs);
	M0_POST(nlx_kcore_domain_invariant(kd));
	return 0;
}

/**
   Finalizes the kernel core domain private data object.
   @param kd kernel core private data pointer for the domain to be finalized.
 */
static void nlx_kcore_kcore_dom_fini(struct nlx_kcore_domain *kd)
{
	M0_PRE(nlx_kcore_domain_invariant(kd));
	M0_PRE(nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc));

	drv_bufs_tlist_fini(&kd->kd_drv_bufs);
	drv_tms_tlist_fini(&kd->kd_drv_tms);
	kd->kd_drv_ops = NULL;
	m0_mutex_fini(&kd->kd_drv_mutex);
}

/** @} */ /* KLNetCore */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
