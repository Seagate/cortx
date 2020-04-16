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

#pragma once

#ifndef __MERO_NET_LNET_CORE_H__
#define __MERO_NET_LNET_CORE_H__

/**
   @page LNetCoreDLD-fspec LNet Transport Core API

   - @ref LNetCoreDLD-fspec-ovw
   - @ref LNetCoreDLD-fspec-ds
   - @ref LNetCoreDLD-fspec-subs
   - @ref LNetCore "LNet Transport Core Interfaces"

   @section LNetCoreDLD-fspec-ovw API Overview
   The LNet Transport Core presents an address space agnostic API to the LNet
   Transport layer.  These interfaces are declared in the file
   @ref net/lnet/lnet_core.h.

   The interface is implemented differently in the kernel and in user space.
   The kernel interface interacts directly with LNet; the user space interface
   uses a device driver to communicate with its kernel counterpart and
   uses shared memory to avoid event data copy.

   The Core API offers no callback mechanism.  Instead, the transport must
   poll for events.  Typically this is done on one or more dedicated threads,
   which exhibit the desired processor affiliation required by the higher
   software layers.

   The following sequence diagram illustrates the typical operational flow:
   @msc
   A [label="Application"],
   N [label="Network"],
   x [label="XO Method"],
   t [label="XO Event\nThread"],
   o [label="Core\nOps"],
   e [label="Core\nEvent Queue"],
   L [label="LNet\nOps"],
   l [label="LNet\nCallback"];

   t=>e  [label="Wait"];
   ...;
   A=>N  [label="m0_net_buffer_add()"];
   N=>x  [label="xo_buf_add()"];
   x=>o  [label="nlx_core_buf_op()"];
   o=>L  [label="MD Operation"];
   L>>o;
   o>>x;
   x>>N;
   N>>A;
   ...;
   l=>>e [label="EQ callback"];
   e>>t  [label="Events present"];
   t=>e  [label="Get Event"];
   e>>t  [label="event"];
   N<<=t [label="m0_net_buffer_event_post()"];
   N=>>A [label="callback"];
   t=>e  [label="Get Event"];
   e>>t  [label="empty"];
   t=>e  [label="Wait"];
   ...;
   @endmsc

   @section LNetCoreDLD-fspec-ds API Data Structures
   The API requires that the transport application maintain API defined shared
   data for various network related objects:
   - nlx_core_domain
   - nlx_core_transfer_mc
   - nlx_core_buffer

   The sharing takes place between the transport layer and the core layer.
   This will span the kernel and user address space boundary when using the
   user space transport.

   These shared data structures should be embedded in the transport
   application's own private data.  This requirement results in an
   initialization call pattern that takes a pointer to the standard network
   layer data structure concerned and a pointer to the API's data structure.

   Subsequent calls to the API only pass the API data structure pointer.  The
   API data structure must be eventually finalized.

   @section LNetCoreDLD-fspec-subs Subroutines

   The API subroutines are described in
   @ref LNetCore "LNet Transport Core Interfaces".
   The subroutines are categorized as follows:

   - Initialization, finalization, cancellation and query subroutines:
     - nlx_core_buf_deregister()
     - nlx_core_buf_register()
     - nlx_core_dom_fini()
     - nlx_core_dom_init()
     - nlx_core_get_max_buffer_segment_size()
     - nlx_core_get_max_buffer_size()
     - nlx_core_tm_start()
     - nlx_core_tm_stop()
     .
     These interfaces have names roughly similar to the associated
     m0_net_xprt_ops method from which they are intended to be directly or
     indirectly invoked.  Note that there are no equivalents for the @c
     xo_tm_init(), @c xo_tm_fini() and @c xo_tm_confine() calls.

   - End point address parsing subroutines:
     - nlx_core_ep_addr_decode()
     - nlx_core_ep_addr_encode()

   - Buffer operation related subroutines:
     - nlx_core_buf_active_recv()
     - nlx_core_buf_active_send()
     - nlx_core_buf_del()
     - nlx_core_buf_msg_recv()
     - nlx_core_buf_msg_send()
     - nlx_core_buf_passive_recv()
     - nlx_core_buf_passive_send()
     - nlx_core_buf_desc_encode()
     - nlx_core_buf_desc_decode()
     .
     The buffer operation initiation calls are all invoked in the context of
     the m0_net_buffer_add() subroutine.  All operations are immediately
     initiated in the Lustre LNet kernel module, though results will be
     returned asynchronously through buffer events.

   - Event processing calls:
     - nlx_core_buf_event_wait()
     - nlx_core_buf_event_get()
     .
     The API assumes that only a single transport thread will be used to
     process events.

   Invocation of the buffer operation initiation subroutines and the
   nlx_core_buf_event_get() subroutine should be serialized.

   @see @ref KLNetCoreDLD "LNet Transport Kernel Core DLD"
   @see @ref ULNetCoreDLD "LNet Transport User Space Core DLD"
   @see @ref LNetDRVDLD "LNet Transport Device DLD"

 */

#include "net/lnet/lnet.h"
#include "net/lnet/lnet_core_types.h"

/**
   @defgroup LNetCore LNet Transport Core Interfaces
   @ingroup LNetDFS

   The internal, address space agnostic I/O API used by the LNet transport.
   See @ref LNetCoreDLD-fspec "LNet Transport Core API" for organizational
   details and @ref LNetDLD "LNet Transport DLD" for details of the
   Mero Network transport for LNet.

   @{
 */

/**
   Allocates and initializes the network domain's private field for use by LNet.
   @param dom The network domain pointer.
   @param lcdom The private data pointer for the domain to be initialized.
 */
static int nlx_core_dom_init(struct m0_net_domain *dom,
			     struct nlx_core_domain *lcdom);

/**
   Releases LNet transport resources related to the domain.
 */
static void nlx_core_dom_fini(struct nlx_core_domain *lcdom);

/**
   Gets the maximum buffer size (counting all segments).
 */
static m0_bcount_t nlx_core_get_max_buffer_size(struct nlx_core_domain *lcdom);

/**
   Gets the maximum size of a buffer segment.
 */
static m0_bcount_t nlx_core_get_max_buffer_segment_size(
						struct nlx_core_domain *lcdom);

/**
   Gets the maximum number of buffer segments.
 */
static int32_t nlx_core_get_max_buffer_segments(struct nlx_core_domain *lcdom);

/**
   Registers a network buffer.  In user space this results in the buffer memory
   getting pinned.
   The subroutine allocates private data to associate with the network buffer.
   @param lcdom The domain private data to be initialized.
   @param buffer_id Value to set in the cb_buffer_id field.
   @param bvec Buffer vector with core address space pointers.
   @param lcbuf The core private data pointer for the buffer.
 */
static int nlx_core_buf_register(struct nlx_core_domain *lcdom,
				 nlx_core_opaque_ptr_t buffer_id,
				 const struct m0_bufvec *bvec,
				 struct nlx_core_buffer *lcbuf);

/**
   Deregisters the buffer.
   @param lcdom The domain private data.
   @param lcbuf The buffer private data.
 */
static void nlx_core_buf_deregister(struct nlx_core_domain *lcdom,
				    struct nlx_core_buffer *lcbuf);

/**
   Enqueues a buffer for message reception. Multiple messages may be received
   into the buffer, space permitting, up to the configured maximum.

   The invoker should ensure that the subroutine is not invoked concurrently
   with any of the other buffer operation initiation subroutines or the
   nlx_core_buf_event_get() subroutine.

   The invoker should provision sufficient buffer event structures prior to
   the call, using the nlx_core_bevq_provision() subroutine.

   @param lcdom The domain private data.
   @param lctm  Transfer machine private data.
   @param lcbuf Buffer private data.
   @pre lcbuf->cb_length is valid
   @pre lcbuf->cb_min_receive_size is valid
   @pre lcbuf->cb_max_operations > 0
   @see nlx_core_bevq_provision()
 */
static int nlx_core_buf_msg_recv(struct nlx_core_domain *lcdom,
				 struct nlx_core_transfer_mc *lctm,
				 struct nlx_core_buffer *lcbuf);

/**
   Enqueues a buffer for message transmission.

   The invoker should ensure that the subroutine is not invoked concurrently
   with any of the other buffer operation initiation subroutines or the
   nlx_core_buf_event_get() subroutine.

   The invoker should provision sufficient buffer event structures prior to
   the call, using the nlx_core_bevq_provision() subroutine.

   @param lcdom The domain private data.
   @param lctm  Transfer machine private data.
   @param lcbuf Buffer private data.
   @pre lcbuf->cb_length is valid
   @pre lcbuf->cb_addr is valid
   @pre lcbuf->cb_max_operations == 1
   @see nlx_core_bevq_provision()
 */
static int nlx_core_buf_msg_send(struct nlx_core_domain *lcdom,
				 struct nlx_core_transfer_mc *lctm,
				 struct nlx_core_buffer *lcbuf);

/**
   Enqueues a buffer for active bulk receive.
   The cb_match_bits field should be set to the value of the match bits of the
   remote passive buffer.
   The cb_addr field should be set with the end point address of the
   transfer machine with the passive buffer.

   The invoker should ensure that the subroutine is not invoked concurrently
   with any of the other buffer operation initiation subroutines or the
   nlx_core_buf_event_get() subroutine.

   The invoker should provision sufficient buffer event structures prior to
   the call, using the nlx_core_bevq_provision() subroutine.

   @param lcdom The domain private data.
   @param lctm  Transfer machine private data.
   @param lcbuf Buffer private data.
   @pre The buffer is queued on the specified transfer machine.
   @pre lcbuf->cb_length is valid
   @pre lcbuf->cb_match_bits != 0
   @pre lcbuf->cb_addr is valid
   @pre lcbuf->cb_max_operations == 1
   @see nlx_core_bevq_provision()
 */
static int nlx_core_buf_active_recv(struct nlx_core_domain *lcdom,
				    struct nlx_core_transfer_mc *lctm,
				    struct nlx_core_buffer *lcbuf);

/**
   Enqueues a buffer for active bulk send.
   See nlx_core_buf_active_recv() for how the buffer is to be initialized.

   The invoker should ensure that the subroutine is not invoked concurrently
   with any of the other buffer operation initiation subroutines or the
   nlx_core_buf_event_get() subroutine.

   The invoker should provision sufficient buffer event structures prior to
   the call, using the nlx_core_bevq_provision() subroutine.

   @param lcdom The domain private data.
   @param lctm  Transfer machine private data.
   @param lcbuf Buffer private data.
   @pre The buffer is queued on the specified transfer machine.
   @pre lcbuf->cb_length is valid
   @pre lcbuf->cb_match_bits != 0
   @pre lcbuf->cb_addr is valid
   @pre lcbuf->cb_max_operations == 1
   @see nlx_core_bevq_provision()
 */
static int nlx_core_buf_active_send(struct nlx_core_domain *lcdom,
				    struct nlx_core_transfer_mc *lctm,
				    struct nlx_core_buffer *lcbuf);

/**
   This subroutine generates new match bits for the given buffer's
   cb_match_bits field.

   It is intended to be used by the transport prior to invoking passive buffer
   operations.  The reason it is not combined with the passive operation
   subroutines is that the core API does not guarantee unique match bits.  The
   match bit counter will wrap over time, though, being a very large counter,
   it would take considerable time before it does wrap.

   @param lctm  Transfer machine private data.
   @param lcbuf The buffer private data.
   @param cbd Descriptor structure to be filled in.
   @pre The buffer is queued on the specified transfer machine on one of the
   passive bulk queues.
   @see nlx_core_buf_desc_decode()
 */
static void nlx_core_buf_desc_encode(struct nlx_core_transfer_mc *lctm,
				     struct nlx_core_buffer *lcbuf,
				     struct nlx_core_buf_desc *cbd);

/**
   This subroutine decodes the buffer descriptor and copies the values into the
   given core buffer private data.  It is the inverse operation of the
   nlx_core_buf_desc_encode().

   It does the following:
   - The descriptor is validated.
   - The cb_addr field and cb_match_bits fields are set from the descriptor,
     providing the address of the passive buffer.
   - The operation being performed (SEND or RECV) is validated against the
     descriptor.
   - The active buffer length is validated against the passive buffer.
   - The size of the active transfer is set in the cb_length field.

   @param lctm  Transfer machine private data.
   @param lcbuf The buffer private data with cb_length set to the buffer size.
   @param cbd Descriptor structure to be filled in.
   @retval -EINVAL Invalid descriptor
   @retval -EPERM  Invalid operation
   @retval -EFBIG  Buffer too small
   @pre The buffer is queued on the specified transfer machine on one of the
   active bulk queues.
   @see nlx_core_buf_desc_encode()
 */
static int nlx_core_buf_desc_decode(struct nlx_core_transfer_mc *lctm,
				    struct nlx_core_buffer *lcbuf,
				    struct nlx_core_buf_desc *cbd);

/**
   Enqueues a buffer for passive bulk receive.
   The match bits for the passive buffer should be set in the buffer with the
   nlx_core_buf_desc_encode() subroutine before this call.
   It is guaranteed that the buffer can be remotely accessed when the
   subroutine returns.

   The invoker should ensure that the subroutine is not invoked concurrently
   with any of the other buffer operation initiation subroutines or the
   nlx_core_buf_event_get() subroutine.

   The invoker should provision sufficient buffer event structures prior to
   the call, using the nlx_core_bevq_provision() subroutine.

   @param lcdom The domain private data.
   @param lctm  Transfer machine private data.
   @param lcbuf Buffer private data.
   @pre The buffer is queued on the specified transfer machine.
   @pre lcbuf->cb_length is valid
   @pre lcbuf->cb_match_bits != 0
   @pre lcbuf->cb_max_operations == 1
   @see nlx_core_bevq_provision()
 */
static int nlx_core_buf_passive_recv(struct nlx_core_domain *lcdom,
				     struct nlx_core_transfer_mc *lctm,
				     struct nlx_core_buffer *lcbuf);

/**
   Enqueues a buffer for passive bulk send.
   See nlx_core_buf_passive_recv() for how the buffer is to be initialized.
   It is guaranteed that the buffer can be remotely accessed when the
   subroutine returns.

   The invoker should ensure that the subroutine is not invoked concurrently
   with any of the other buffer operation initiation subroutines or the
   nlx_core_buf_event_get() subroutine.

   The invoker should provision sufficient buffer event structures prior to
   the call, using the nlx_core_bevq_provision() subroutine.

   @param lcdom The private data pointer for the domain.
   @param lctm  Transfer machine private data.
   @param lcbuf Buffer private data.
   @pre The buffer is queued on the specified transfer machine.
   @pre lcbuf->cb_length is valid
   @pre lcbuf->cb_match_bits != 0
   @pre lcbuf->cb_max_operations == 1
   @see nlx_core_bevq_provision()
 */
static int nlx_core_buf_passive_send(struct nlx_core_domain *lcdom,
				     struct nlx_core_transfer_mc *lctm,
				     struct nlx_core_buffer *lcbuf);

/**
   Cancels a buffer operation if possible.
   @param lcdom The domain private data.
   @param lctm  Transfer machine private data.
   @param lcbuf Buffer private data.
   @pre The buffer is queued on the specified transfer machine.
 */
static int nlx_core_buf_del(struct nlx_core_domain *lcdom,
			    struct nlx_core_transfer_mc *lctm,
			    struct nlx_core_buffer *lcbuf);

/**
   Waits for buffer events, or the timeout.
   @param lcdom Domain pointer.
   @param lctm Transfer machine private data.
   @param timeout Absolute time at which to stop waiting.  A value of 0
   indicates that the subroutine should not wait.
   @retval 0 Events present.
   @retval -ETIMEDOUT Timed out before events arrived.
 */
static int nlx_core_buf_event_wait(struct nlx_core_domain *lcdom,
				   struct nlx_core_transfer_mc *lctm,
				   m0_time_t timeout);

/**
   Fetches the next event from the circular buffer event queue.

   The invoker should ensure that the subroutine is not invoked concurrently
   with any of the buffer operation initiation subroutines, or another
   invocation of itself.

   @param lctm Transfer machine private data.
   @param lcbe The next buffer event is returned here.
   @retval true Event returned.
   @retval false No events on the queue.
   @see nlx_core_bevq_provision()
 */
static bool nlx_core_buf_event_get(struct nlx_core_transfer_mc *lctm,
				   struct nlx_core_buffer_event *lcbe);

/**
   Parses an end point address string and convert to internal form.
   A "*" value for the transfer machine identifier results in a value of
   M0_NET_LNET_TMID_INVALID being set.
   @param lcdom Domain pointer.
   @param ep_addr The LNet end point address to decode.
   @param cepa On success, the parsed values are stored here.
 */
static int nlx_core_ep_addr_decode(struct nlx_core_domain *lcdom,
				   const char *ep_addr,
				   struct nlx_core_ep_addr *cepa);

/**
   Constructs the external address string from its internal form.
   A value of M0_NET_LNET_TMID_INVALID for the cepa_tmid field results in
   a "*" being set for that field.
   @param lcdom Domain pointer.
   @param cepa The end point address parameters to encode.
   @param buf The string address is stored in this buffer.
 */
static void nlx_core_ep_addr_encode(struct nlx_core_domain *lcdom,
				    const struct nlx_core_ep_addr *cepa,
				    char buf[M0_NET_LNET_XEP_ADDR_LEN]);

/**
   Gets a list of strings corresponding to the local LNET network interfaces.
   The returned array must be released using nlx_core_nidstrs_put().
   @param lcdom Domain pointer.
   @param nidary A NULL-terminated (like argv) array of NID strings is returned.
 */
static int nlx_core_nidstrs_get(struct nlx_core_domain *lcdom,
				char * const **nidary);

/**
   Releases the string array returned by nlx_core_nidstrs_get().
 */
static void nlx_core_nidstrs_put(struct nlx_core_domain *lcdom,
				 char * const **nidary);

/**
   Starts a transfer machine. Internally this results in
   the creation of the LNet EQ associated with the transfer machine.
   @param lcdom The domain private data.
   @param tm The transfer machine pointer.
   @param lctm The transfer machine private data to be initialized.  The
   nlx_core_transfer_mc::ctm_addr must be set by the caller.  If the
   lcpea_tmid field value is M0_NET_LNET_TMID_INVALID then a transfer machine
   identifier is dynamically assigned to the transfer machine and the
   nlx_core_transfer_mc::ctm_addr is modified in place.
   @note There is no equivalent of the xo_tm_init() subroutine.
   @note This function does not create a m0_net_end_point for the transfer
   machine, because there is no equivalent object at the core layer.
 */
static int nlx_core_tm_start(struct nlx_core_domain *lcdom,
			     struct m0_net_transfer_mc *tm,
			     struct nlx_core_transfer_mc *lctm);

/**
   Stops the transfer machine and release associated resources.  All operations
   must be finalized prior to this call.
   @param lcdom The domain private data.
   @param lctm The transfer machine private data.
   @note There is no equivalent of the xo_tm_fini() subroutine.
 */
static void nlx_core_tm_stop(struct nlx_core_domain *lcdom,
			     struct nlx_core_transfer_mc *lctm);

/**
   Compare two struct nlx_core_ep_addr objects.
 */
static inline bool nlx_core_ep_eq(const struct nlx_core_ep_addr *cep1,
				  const struct nlx_core_ep_addr *cep2)
{
	return cep1->cepa_nid == cep2->cepa_nid &&
		cep1->cepa_pid == cep2->cepa_pid &&
		cep1->cepa_portal == cep2->cepa_portal &&
		cep1->cepa_tmid == cep2->cepa_tmid;
}

/**
   Subroutine to provision additional buffer event entries on the
   buffer event queue if needed.
   It increments the struct nlx_core_transfer_mc::ctm_bev_needed counter
   by the number of LNet events that can be delivered, as indicated by the
   @c need parameter.

   The subroutine is to be used in the consumer address space only, and uses
   a kernel or user space specific allocator subroutine to obtain an
   appropriately blessed entry in the producer space.

   The invoker must lock the transfer machine prior to this call.

   @param lcdom LNet core domain pointer.
   @param lctm Pointer to LNet core TM data structure.
   @param need Number of additional buffer entries required.
   @see nlx_core_new_blessed_bev(), nlx_core_bevq_release()
 */
static int nlx_core_bevq_provision(struct nlx_core_domain *lcdom,
				   struct nlx_core_transfer_mc *lctm,
				   size_t need);

/**
   Subroutine to reduce the needed capacity of the buffer event queue.
   Note: Entries are never actually released from the circular queue until
   termination.

   The subroutine is to be used in the consumer address space only.
   The invoker must lock the transfer machine prior to this call.

   @param lctm Pointer to LNet core TM data structure.
   @param release Number of buffer entries released.
   @see nlx_core_bevq_provision()
 */
static void nlx_core_bevq_release(struct nlx_core_transfer_mc *lctm,
				  size_t release);

/**
   Subroutine to allocate a new buffer event structure initialized
   with the producer space self pointer set.
   This subroutine is defined separately for the kernel and user space.
   @param lcdom LNet core domain pointer.
   @param lctm LNet core transfer machine pointer.
   In the user space transport this must be initialized at least with the
   core device driver file descriptor.
   In kernel space this is not used.
   @param bevp Buffer event return pointer.  The memory must be allocated with
   the NLX_ALLOC_PTR() macro or variant.  It will be freed with the
   NLX_FREE_PTR() macro from the nlx_core_bev_free_cb() subroutine.
   @post bev_cqueue_bless(&bevp->cbe_tm_link) has been invoked.
   @see bev_cqueue_bless()
 */
static int nlx_core_new_blessed_bev(struct nlx_core_domain *lcdom,
				    struct nlx_core_transfer_mc *lctm,
				    struct nlx_core_buffer_event **bevp);

/**
   Allocate zero-filled memory, like m0_alloc().
   In user space, this memory is allocated such that it will not
   cross page boundaries using m0_alloc_aligned().
   @param size Memory size.
   @param shift Alignment, ignored in kernel space.
   @pre size <= PAGE_SIZE
 */
static void *nlx_core_mem_alloc(size_t size, unsigned shift);

/**
   Frees memory allocated by nlx_core_mem_alloc().
 */
static void nlx_core_mem_free(void *data, size_t size, unsigned shift);

static void nlx_core_dom_set_debug(struct nlx_core_domain *lcdom, unsigned dbg);
static void nlx_core_tm_set_debug(struct nlx_core_transfer_mc *lctm,
				  unsigned dbg);

/**
   Round up a number n to the next power of 2, min 1<<3, works for n <= 1<<9.
   If n is a power of 2, returns n.
   Requires a constant input, allowing compile-time computation.
 */
#define NLX_PO2_SHIFT(n)                                                \
	(((n) <= 8) ? 3 : ((n) <= 16) ? 4 : ((n) <= 32) ? 5 :           \
	 ((n) <= 64) ? 6 : ((n) <= 128) ? 7 : ((n) <= 256) ? 8 :        \
	 ((n) <= 512) ? 9 : ((n) / 0))
#define NLX_ALLOC_ALIGNED_PTR(ptr) \
	((ptr) = nlx_core_mem_alloc(sizeof ((ptr)[0]),                  \
				    NLX_PO2_SHIFT(sizeof ((ptr)[0]))))
#define NLX_FREE_ALIGNED_PTR(ptr) \
	nlx_core_mem_free((ptr), sizeof ((ptr)[0]), \
			  NLX_PO2_SHIFT(sizeof ((ptr)[0])))

#define NLX_ALLOC(ptr, len) ({ ptr = m0_alloc(len); })
#define NLX_ALLOC_PTR(ptr) M0_ALLOC_PTR(ptr)
#define NLX_ALLOC_ARR(ptr, nr) M0_ALLOC_ARR(ptr, nr)

/** @} */ /* LNetCore */

#endif /* __MERO_NET_LNET_CORE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
