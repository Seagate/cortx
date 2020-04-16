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
 * Original author: Alexey Lyashkov <Alexey_Lyashkov@xyratex.com>,
 *                  Nikita Danilov <Nikita_Danilov@xyratex.com>,
 *                  Carl Braganza <Carl_Braganza@xyratex.com>
 * Original creation date: 04/01/2010
 */

#pragma once

#ifndef __MERO_NET_NET_H__
#define __MERO_NET_NET_H__

#include <stdarg.h>

#include "lib/rwlock.h"
#include "lib/list.h"
#include "lib/tlist.h"
#include "lib/queue.h"
#include "lib/refs.h"
#include "lib/chan.h"
#include "lib/cond.h"
#include "lib/mutex.h"
#include "lib/time.h"
#include "lib/thread.h"
#include "lib/vec.h"
#include "net/net_otw_types.h"
#include "net/net_otw_types_xc.h"
#include "sm/sm.h"

/**
   @defgroup net Networking

   @brief The networking module provides an asynchronous, event-based message
   passing service, with support for asynchronous bulk data transfer (if used
   with an RDMA capable transport).

   Major data-types in M0 networking are:
   @li Network buffer (m0_net_buffer);
   @li Network buffer descriptor (m0_net_buf_desc);
   @li Network buffer event (m0_net_buffer_event);
   @li Network domain (m0_net_domain);
   @li Network end point (m0_net_end_point);
   @li Network transfer machine (m0_net_transfer_mc);
   @li Network transfer machine event (m0_net_tm_event);
   @li Network transport (m0_net_xprt);

   See <a href="https://docs.google.com/a/seagate.com/document/d/1pDOQXWDZ9t9XDc
yXsx4T_aGjFvsyjjvN1ygOtfoXcFg/edit?hl=en#">RPC Bulk Transfer Task Plan</a>
   for details on the design and use of this API.  If you are writing a
   transport, then the document is the reference for the internal threading and
   serialization model.

   See <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV77938
6NtFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>
   for additional details on the design and use of this API.

   @{

 */

/* import */
struct m0_bitmap;

/* export */
struct m0_net_xprt;
struct m0_net_xprt_ops;
struct m0_net_domain;
struct m0_net_transfer_mc;
struct m0_net_tm_event;
struct m0_net_tm_callbacks;
struct m0_net_end_point;
struct m0_net_buffer;
struct m0_net_buf_desc;
struct m0_net_buffer_event;
struct m0_net_buffer_callbacks;
struct m0_net_qstats;

/**
   Constructor for the network library
 */
M0_INTERNAL int m0_net_init(void);

/**
   Destructor for the network library.
   Releases all allocated resources.
 */
M0_INTERNAL void m0_net_fini(void);

enum {
	/**
	   Default minimum number of receive queue buffers for automatic
	   provisioning.
	 */
	M0_NET_TM_RECV_QUEUE_DEF_LEN = 2,
};

/**
   Network transport (e.g. lnet).
 */
struct m0_net_xprt {
	const char                   *nx_name;
	const struct m0_net_xprt_ops *nx_ops;
};

/**
   Network transport operations. The network domain mutex must be
   held to invoke these methods, unless explicitly stated otherwise.
 */
struct m0_net_xprt_ops {
	/**
	   Initialises transport specific part of a domain (e.g., start threads,
	   initialise portals).
	   Only the m0_net_mutex is held across this call.
	 */
	int  (*xo_dom_init)(struct m0_net_xprt *xprt,
			    struct m0_net_domain *dom);
	/**
	   Finalises transport resources in a domain.
	   Only the m0_net_mutex is held across this call.
	 */
	void (*xo_dom_fini)(struct m0_net_domain *dom);

	/**
	   Performs transport level initialization of the transfer machine.

	   All fields will be initialized at this time, specifically:

	       - ntm_dom
	       - ntm_xprt_private - Initialized to NULL. The method can
	         set its own value in the structure.

	   @retval 0 (success)
	   @retval -errno (failure)
	   @see m0_net_tm_init()
	 */
	int (*xo_tm_init)(struct m0_net_transfer_mc *tm);

	/**
	   Optional method to set the processor affinity for the threads of
	   a transfer machine.
	   The transfer machine must be initialized but not yet started.
	   @param processors Processor bitmap.
	   @retval -ENOSYS  No affinity support available. Implied by a
	   missing method.
	 */
	int (*xo_tm_confine)(struct m0_net_transfer_mc *tm,
			     const struct m0_bitmap *processors);

	/**
	   Initiates the startup of the (initialized) transfer machine.
	   A completion event should be posted when started, using a different
	   thread.
	   <b>Serialized using the transfer machine mutex.</b>

	   The following fields are of special interest to this method:
	       - ntm_dom
	       - ntm_xprt_private

	   @param addr Address (network end-point string representation) of the
	          transfer machine. The method should not reference this string
	          after it returns.

	   @see m0_net_tm_start()
	 */
	int (*xo_tm_start)(struct m0_net_transfer_mc *tm, const char *addr);

	/**
	   Initiates the shutdown of a transfer machine, cancelling any
	   pending startup.
	   No incoming messages should be accepted.  Pending operations should
	   drain or be cancelled if requested.
	   A completion event should be posted when stopped, using a different
	   thread.
	   <b>Serialized using the transfer machine mutex.</b>
	   @param tm   Transfer machine pointer.
	   @param cancel Pending outbound operations should be cancelled
	   immediately.
	   @retval 0 (success)
	   @retval -errno (failure)
	   @see m0_net_tm_stop()
	 */
	int (*xo_tm_stop)(struct m0_net_transfer_mc *tm, bool cancel);

	/**
	   Releases resources associated with a transfer machine.
	   The transfer machine will be in the stopped state.

	   The following fields are of special interest to this method:

	       - ntm_dom
	       - ntm_xprt_private - The method should free any
	         allocated memory tracked by this pointer.
	   @see m0_net_tm_fini()
	 */
	void (*xo_tm_fini)(struct m0_net_transfer_mc *tm);

	/**
	   Creates an end point with a specific address.
	   @param epp     Returned end point data structure.
	   @param addr    Address string.  Could be NULL to
			  indicate dynamic addressing.
			  Do not reference the string after return.

	   @see m0_net_end_point_create()
	 */
	int (*xo_end_point_create)(struct m0_net_end_point **epp,
				   struct m0_net_transfer_mc *tm,
				   const char *addr);

	/**
	   Registers the buffer for use with a transfer machine in
	   the manner indicated by the m0_net_buffer.nb_qtype value.
	   @see m0_net_buffer_register()
	 */
	int (*xo_buf_register)(struct m0_net_buffer *nb);

	/**
	   Deregisters the buffer from the transfer machine.
	   @see m0_net_buffer_deregister()
	 */
	void (*xo_buf_deregister)(struct m0_net_buffer *nb);

	/**
	   Initiates an operation on a buffer on the transfer machine's
	   queues.

	   In the case of buffers added to the M0_NET_QT_ACTIVE_BULK_RECV or
	   M0_NET_QT_ACTIVE_BULK_SEND queues, the method should validate that
	   the buffer size or data length meet the size requirements encoded
	   within the network buffer descriptor m0_net_buffer::nb_desc.

	   In the case of the buffers added to the M0_NET_QT_PASSIVE_BULK_RECV
	   or M0_NET_QT_PASSIVE_BULK_SEND queues, the method should set the
	   network buffer descriptor in the specified buffer
	   (m0_net_buffer::nb_desc).

	   The M0_NET_BUF_IN_USE flag will be cleared before invoking the
	   method. This allows the transport to use this flag to defer
	   operations until later, which is useful if buffers are added during
	   transfer machine state transitions.

	   The M0_NET_BUF_QUEUED flag and the nb_add_time field
	   will be set prior to calling the method.

	   <b>Serialized using the transfer machine mutex.</b>

	   @pre nb->nb_tm != NULL
	   @see m0_net_buffer_add(), struct m0_net_buffer
	 */
	int (*xo_buf_add)(struct m0_net_buffer *nb);

	/**
	   Cancels an operation involving a buffer.
	   The method should cancel the operation involving use of the
	   buffer, as described by the value of the m0_net_buffer.nb_qtype
	   field.
	   The M0_NET_BUF_CANCELLED flag should be set in buffers whose
	   operations get cancelled, so m0_net_buffer_event_post() can
	   enforce the right error status.
	   <b>Serialized using the transfer machine mutex.</b>

	   @pre nb->nb_tm != NULL
	   @pre m0_net__qtype_is_valid(nb->nb_qtype)
	   @see m0_net_buffer_del()
	 */
	void (*xo_buf_del)(struct m0_net_buffer *nb);

	/**
	   Invoked by the m0_net_buffer_event_deliver_synchronously()
	   subroutine to request the transport to disable automatic delivery
	   of buffer events. The method is optional and need not be specified
	   if this support is not available.
	   If supported, then the xo_bev_deliver_all() and the xo_bev_pending()
	   operations must be provided.
	   @see m0_net_buffer_event_deliver_synchronously()
	 */
	int  (*xo_bev_deliver_sync)(struct m0_net_transfer_mc *tm);

	/**
	   Invokes m0_net_buffer_event_post() for all pending events.

	   Invoked by the m0_net_buffer_event_deliver_all()
	   subroutine. Optional if the synchronous buffer event delivery
	   feature is not supported.

	   As buffer event delivery takes place without holding the transfer
	   machine mutex, the transport should protect the invocation of this
	   subroutine from synchronous termination of the transfer machine.
	 */
	void (*xo_bev_deliver_all)(struct m0_net_transfer_mc *tm);

	/**
	   Returns true, iff there are pending events.

	   Invoked by the m0_net_buffer_event_pending() subroutine.  Optional
	   if the synchronous buffer event delivery feature is not supported.
	 */
	bool (*xo_bev_pending)(struct m0_net_transfer_mc *tm);

	/**
	   Arranges for the given channel to be signalled when new event
	   arrives.

	   Invoked by the m0_net_buffer_event_notify() subroutine. Optional if
	   the synchronous buffer event delivery feature is not supported.
	 */
	void (*xo_bev_notify)(struct m0_net_transfer_mc *tm,
			      struct m0_chan *chan);

	/**
	   Retrieves the maximum buffer size (includes all segments).
	   @retval size    Returns the maximum buffer size.
	   @see m0_net_domain_get_max_buffer_size()
	 */
	m0_bcount_t (*xo_get_max_buffer_size)(const struct m0_net_domain *dom);

	/**
	   Retrieves the maximum buffer segment size.
	   @retval size    Returns the maximum segment size.
	   @see m0_net_domain_get_max_buffer_segment_size()
	 */
	m0_bcount_t (*xo_get_max_buffer_segment_size)(const struct m0_net_domain
						      *dom);

	/**
	   Retrieves the maximum number of buffer segments.
	   @retval num_segs Returns the maximum number of buffer segments.
	   @see m0_net_domain_get_max_buffer_segments()
	 */
	int32_t (*xo_get_max_buffer_segments)(const struct m0_net_domain *dom);

	/**
	   Retrieves the buffer descriptor size.
	 */
	m0_bcount_t (*xo_get_max_buffer_desc_size)(const struct m0_net_domain
						   *dom);
};

/**
   A collection of network resources.
 */
struct m0_net_domain {
	/**
	   This mutex is used to protect the resources associated with
	   a network domain.
	 */
	struct m0_mutex     nd_mutex;

	/** List of m0_net_buffer structures registered with the domain. */
	struct m0_list      nd_registered_bufs;

	/**
	   List of m0_net_transfer_mc structures. Machines are linked here
	   through ntm_dom_linkage::ntm_dom_linkage.
	 */
	struct m0_list      nd_tms;

	/** Transport private domain data. */
	void               *nd_xprt_private;

	/** This domain's transport. */
	struct m0_net_xprt *nd_xprt;

	/** Linkage for invoking application. */
	struct m0_tlink     nd_app_linkage;

	/** Maximum number of segments in a net buffer.
         * This value is retrieved from xo_get_max_buffer_segments() via
         * m0_net_domain_get_max_buffer_segments()
         */
	uint32_t            nd_get_max_buffer_segments;

	/** Maximum segment size in a net buffer.
         * This value is retrieved from xo_get_max_buffer_segment_size() via
         * m0_net_domain_get_max_buffer_segment_size()
         */
	m0_bcount_t         nd_get_max_buffer_segment_size;

	/** Maximum size of a net buffer.
         * This value is retrieved from xo_get_max_buffer_size() via
         * m0_net_domain_get_max_buffer_size()
         */
	m0_bcount_t         nd_get_max_buffer_size;

	/** Maximum net buffer descriptor size.
         * This value is retrieved from xo_get_max_buffer_desc_size() via
         * m0_net_domain_get_max_buffer_desc_size()
         */
	m0_bcount_t         nd_get_max_buffer_desc_size;

	uint64_t            nd_magix;
};

/**
   Initialises a domain.
   @pre dom->nd_xprt == NULL
 */
int m0_net_domain_init(struct m0_net_domain *dom, struct m0_net_xprt *xprt);

/**
   Releases resources related to a domain.
   @pre All end points, registered buffers and transfer machines released.
   @param dom Domain pointer.
 */
void m0_net_domain_fini(struct m0_net_domain *dom);

/**
   Returns the the maximum buffer size allowed for the domain.
   This includes all segments.
 */
M0_INTERNAL m0_bcount_t m0_net_domain_get_max_buffer_size(struct m0_net_domain
							  *dom);

/**
   Returns the maximum buffer segment size allowed for the domain.
 */
M0_INTERNAL m0_bcount_t m0_net_domain_get_max_buffer_segment_size(struct
								  m0_net_domain
								  *dom);

/**
   Returns the size of m0_net_buf_desc for a given net domain.

   @retval size Size of m0_net_buf_desc for the transport associated
   with given net domain.
 */
M0_INTERNAL m0_bcount_t m0_net_domain_get_max_buffer_desc_size(struct
							       m0_net_domain
							       *dom);

/**
   Returns the maximum number of buffer segments for the domain.
 */
M0_INTERNAL int32_t m0_net_domain_get_max_buffer_segments(struct m0_net_domain
							  *dom);

/**
   This represents an addressable network end point. Memory for this data
   structure is managed by the network transport component and is associated
   with the transfer machine that created the structure.

   Multiple entities may reference and use the data structure at the same time,
   so a reference count is maintained within it to determine when it is safe to
   release the structure.

   Transports should embed this data structure in their private end point
   structures, and provide the release() method required to free them.
   The release() method, which is called with the transfer machine mutex
   locked, should remove the data structure from the transfer machine
   ntm_end_points list.
 */
struct m0_net_end_point {
	/** Magic number. */
	uint64_t                   nep_magix;
	/** Keeps track of usage. */
	struct m0_ref              nep_ref;
	/** Pointer to transfer machine. */
	struct m0_net_transfer_mc *nep_tm;
	/**
	   Linkage in the transfer machine list,
	   m0_net_transfer_mc::ntm_end_points.
	 */
	struct m0_tlink            nep_tm_linkage;
	/**
	   Transport specific printable representation of the
	   end point address.
	 */
	const char                *nep_addr;
};

/**
 * Endpoint lists. Used for the transfer machine ntm_end_points list.
 */
M0_TL_DESCR_DECLARE(m0_nep, M0_EXTERN);
M0_TL_DECLARE(m0_nep, M0_INTERNAL, struct m0_net_end_point);

/**
   Allocates an end point data structure representing the desired
   end point and sets its reference count to 1,
   or increments the reference count of an existing matching data structure.
   The data structure is linked to the transfer machine.
   The invoker should call the m0_net_end_point_put() when the
   data structure is no longer needed.
   @param epp Pointer to a pointer to the data structure which will be
   set upon return.  The reference count of the returned data structure
   will be at least 1.
   @param tm  Transfer machine pointer.  The transfer machine must be in
   the started state.
   @param addr String describing the end point address in a transport specific
   manner.  The format of this address string is the same as the printable
   representation form stored in the end point nep_addr field.  It is optional,
   and if NULL, the transport may support assignment of an end point with a
   dynamic address; however this is not guaranteed.
   The address string, if specified, is not referenced again after return from
   this subroutine (i.e., the allocated end point gets the copy).

   @see m0_net_end_point_get(), m0_net_end_point_put()
   @pre tm->ntm_state == M0_NET_TM_STARTED
   @post (*epp)->nep_ref->ref_cnt >= 1 && (*epp)->nep_addr != NULL &&
   (*epp)->nep_tm == tm
 */
M0_INTERNAL int m0_net_end_point_create(struct m0_net_end_point **epp,
					struct m0_net_transfer_mc *tm,
					const char *addr);

/**
   Increments the reference count of an end point data structure.
   This is used to safely point to the structure in a different context -
   when done, the reference count should be decremented by a call to
   m0_net_end_point_put().

   @pre ep->nep_ref->ref_cnt > 0
 */
M0_INTERNAL void m0_net_end_point_get(struct m0_net_end_point *ep);

/**
   Decrements the reference count of an end point data structure.
   The structure will be released when the count goes to 0.
   @param ep End point data structure pointer.
   Do not dereference this pointer after this call.
   @pre ep->nep_ref->ref_cnt >= 1
   @note The transfer machine mutex will be obtained internally to synchronize
   the transport provided release() method in case the end point gets released.
 */
void m0_net_end_point_put(struct m0_net_end_point *ep);

/**
    This enumeration describes the types of logical queues in a transfer
    machine.

    @note We use the term "queue" here to imply that the order of addition
          matters; in reality, while it *may* matter, external factors
          have a *much* larger influence on the actual order in which
          buffer operations complete; we're not implying FIFO semantics here!

    Consider:

    - The underlying transport manages message buffers. Since there is a great
      deal of concurrency and latency involved with network communication, and
      message sizes can vary, a completed (sent or received) message buffer is
      not necessarily the first that was added to its "queue" for that purpose.

    - The upper protocol layers of the <i>remote</i> end points are responsible
      for initiating bulk data transfer operations, which ultimately determines
      when passive buffers complete in <i>this</i> process.  The remote upper
      protocol layers can, and probably will, reorder requests into an optimal
      order for themselves, which does not necessarily correspond to the order
      in which the passive bulk data buffers were added.

    The fact of the matter is that the transfer machine itself
    is really only interested in tracking buffer existence and uses
    lists and not queues internally.
 */
enum m0_net_queue_type {
	/** Queue with buffers to receive messages. */
	M0_NET_QT_MSG_RECV = 0,

	/** Queue with buffers with messages to send. */
	M0_NET_QT_MSG_SEND,

	/**
	   Queue with buffers awaiting completion of
	   remotely initiated bulk data send operations
	   that will read from these buffers.
	 */
	M0_NET_QT_PASSIVE_BULK_RECV,

	/**
	   Queue with buffers awaiting completion of
	   remotely initiated bulk data receive operations
	   that will write to these buffers.
	 */
	M0_NET_QT_PASSIVE_BULK_SEND,

	/**
	   Queue with buffers awaiting completion of
	   locally initiated bulk data receive operations
	   that will read from passive buffers.
	 */
	M0_NET_QT_ACTIVE_BULK_RECV,

	/**
	   Queue with buffers awaiting completion of
	   locally initiated bulk data send operations
	   to passive buffers.
	 */
	M0_NET_QT_ACTIVE_BULK_SEND,

	M0_NET_QT_NR
};

/** A transfer machine can be in one of the following states. */
enum m0_net_tm_state {
	M0_NET_TM_UNDEFINED = 0, /**< Undefined, prior to initialization */
	M0_NET_TM_INITIALIZED,   /**< Initialized */
	M0_NET_TM_STARTING,      /**< Startup in progress */
	M0_NET_TM_STARTED,       /**< Active */
	M0_NET_TM_STOPPING,      /**< Shutdown in progress */
	M0_NET_TM_STOPPED,       /**< Stopped */
	M0_NET_TM_FAILED         /**< Failed TM, must be fini'd */
};

/** Transfer machine event types. */
enum m0_net_tm_ev_type {
	M0_NET_TEV_ERROR = 0,      /**< General error */
	M0_NET_TEV_STATE_CHANGE,   /**< Transfer machine state change event */
	M0_NET_TEV_DIAGNOSTIC,     /**< Diagnostic event */
	M0_NET_TEV_NR
};

/**
   Data structure used to provide asynchronous notification of
   significant events, such as the completion of buffer operations,
   transfer machine state changes and general errors.

   All events have the following fields set:
   - nte_type
   - nte_tm
   - nte_time
   - nte_status

   The nte_type field should be referenced to determine the type of
   event, and which other fields of this structure get set:

   - M0_NET_TEV_ERROR provides error notification, out of the context of
     any buffer operation completion, or a transfer machine state change.
     No additional fields are set.

   - M0_NET_TEV_STATE_CHANGE provides notification of a transfer machine
     state change.
     The nte_next_state field describes the destination state.
     Refer to the nte_status field to determine if the operation succeeded.
     The nte_ep field is set if the next state is M0_NET_TM_STARTED; the
     value is used to set the ntm_ep field of the transfer machine.

   - M0_NET_TEV_DIAGNOSTIC provides diagnostic information.
     The nte_payload field may point to transport specific data.
     The API does not require nor specify how a transport produces
     diagnostic information, but does require that diagnostic events
     not be produced unless explicitly requested.

   This data structure is typically allocated on the stack of the thread
   that invokes the m0_net_tm_event_post() subroutine.  Applications
   should not attempt to save a reference to it from their callback
   functions.

   @see m0_net_tm_event_post() for details on event delivery concurrency.
 */
struct m0_net_tm_event {
	/**
	   Indicates the type of event.
	   Other fields get set depending on the value of this field.
	 */
	enum m0_net_tm_ev_type     nte_type;

	/**
	   Transfer machine pointer.
	 */
	struct m0_net_transfer_mc *nte_tm;

	/**
	   Time the event is posted.
	 */
	m0_time_t                  nte_time;

	/**
	   Status or error code associated with the event.

	   In all event types other than M0_NET_TEV_DIAGNOSTIC, a 0 in this
	   field implies successful completion, and a negative error number
	   is used to indicate the reasons for failure.
	   The following errors are well defined:
		- <b>-ENOBUFS</b> This indicates that the transfer machine
		lost messages due to a lack of receive buffers.

	   Diagnostic events are free to make any use of this field.
	 */
	int32_t                    nte_status;

	/**
	   Valid only if the nte_type is M0_NET_TEV_STATE_CHANGE.

	   The next state of the transfer machine is set in this field.
	   Any associated error condition defined by the nte_status field.
	 */
	enum m0_net_tm_state       nte_next_state;

	/**
	   End point pointer to be used to set the value of the ntm_ep
	   field when the state changes to M0_NET_TM_STARTED.
	*/
	struct m0_net_end_point   *nte_ep;

	/**
	   Valid only if the nte_type is M0_NET_TEV_STATE_DIAGNOSTIC.

	   Transports may use this to point to internal data; they
	   could also choose to embed the event data structure
	   in a transport specific structure appropriate to the event.
	   Either approach would be of use to a diagnostic application.
	 */
	void                      *nte_payload;
};

/**
   Callbacks associated with a transfer machine.
   Multiple transfer machines can reference an instance of this structure.
 */
struct m0_net_tm_callbacks {
	/**
	   Event callback.
	   @param ev Pointer to the transfer machine event. The pointer
	   is not valid after return from the subroutine.
	 */
	void (*ntc_event_cb)(const struct m0_net_tm_event *ev);
};

/**
   Statistical data maintained for each transfer machine queue.
   It is up to the higher level layers to retrieve the data and
   reset the statistical counters.
 */
struct m0_net_qstats {
	/**
	   The number of add operations performed.
	 */
	uint64_t        nqs_num_adds;

	/**
	   The number of del operations performed.
	 */
	uint64_t        nqs_num_dels;

	/**
	   The number of successful events posted on buffers in the queue.
	 */
	uint64_t        nqs_num_s_events;

	/**
	   The number of failure events posted on buffers in the queue.

	    In the case of the M0_NET_QT_MSG_RECV queue the failure
	    counter is also incremented when auto-provisioning fails, with an
	    increment equal to the number of buffers required to fill the
	    queue to its minimal level.
	 */
	uint64_t        nqs_num_f_events;

	/**
	    The total of time spent in the queue by all buffers
	    measured from when they were added to the queue
	    to the time their completion event got posted.
	 */
	m0_time_t       nqs_time_in_queue;

	/**
	   The total number of bytes processed by buffers in the queue.
	   Computed at completion.
	 */
	uint64_t        nqs_total_bytes;

	/**
	   The maximum number of bytes processed in a single
	   buffer in the queue.
	   Computed at completion.
	 */
	uint64_t        nqs_max_bytes;
};

/**
   This data structure tracks message buffers and supports callbacks to notify
   the application of changes in state associated with these buffers.
 */
struct m0_net_transfer_mc {
	/**
	   Pointer to application callbacks. Should be set before
	   initialization.
	 */
	const struct m0_net_tm_callbacks *ntm_callbacks;

	/** Specifies the transfer machine state. */
	enum m0_net_tm_state        ntm_state;

	struct m0_sm_group          ntm_group;

	/**
	   Mutex associated with the transfer machine.
	   The mutex is used when the transfer machine state is in
	   the bounds:
	   M0_NET_TM_INITIALIZED < state < M0_NET_TM_STOPPED.

	   Most transfer machine operations are protected by this mutex. The
	   presence of this mutex in the transfer machine provides a tighter
	   locus of memory accesses to the data structures associated with the
	   operation of a single transfer machine, than would occur were the
	   domain mutex used.  It also reduces the memory access overlaps
	   between individual transfer machines. Transports could use this
	   memory access pattern to provide processor-affinity support for
	   buffer operation on a per-transfer-machine-per-processor basis, by
	   invoking the buffer operation callbacks on the same processor used
	   to submit the buffer operation.
	 */
#define ntm_mutex ntm_group.s_lock

	/**
	   Callback activity is tracked by this counter.
	   It is incremented by m0_net_tm_post_event() before invoking
	   a callback, and decremented when it returns.

	   This counter is used to guarantee that a transfer machine is not
	   finalised while callbacks for it are executing.
	 */
	uint32_t                    ntm_callback_counter;

	/** Network domain pointer */
	struct m0_net_domain       *ntm_dom;

	/** List of m0_net_end_point structures. Managed by the transport. */
	struct m0_tl                ntm_end_points;

	/**
	   End point associated with this transfer machine.

	   Messages sent from this transfer machine appear to have originated
	   from this end point.

	   It is created internally with the address provided in the call to
	   m0_net_tm_start(). The field is set only upon successful start of
	   the transfer machine. The field is cleared during fini.
	 */
	struct m0_net_end_point    *ntm_ep;

	/**
	   Waiters for event notifications. They do not get copies
	   of the event.
	 */
	struct m0_chan              ntm_chan;

	/** Lists of m0_net_buffer structures by queue type. */
	struct m0_tl		    ntm_q[M0_NET_QT_NR];

	/** Statistics maintained per logical queue. */
	struct m0_net_qstats        ntm_qstats[M0_NET_QT_NR];

	/** Domain linkage (m0_net_domain::nd_tms). */
	struct m0_list_link         ntm_dom_linkage;

	/** Transport private data. */
	void                       *ntm_xprt_private;

	/**
	   True iff automatic delivery of buffer events will take place.
	 */
	bool                        ntm_bev_auto_deliver;

	/**
	   The buffer pool to use for automatic receive queue provisioning.
	 */
	struct m0_net_buffer_pool  *ntm_recv_pool;

	/**
	   Callbacks structure for automatically allocated receive queue
	   buffers.
	 */
	const struct m0_net_buffer_callbacks *ntm_recv_pool_callbacks;

	/**
	   Minimum queue length for the receive queue when provisioning
	   automatically.  The default value is ::M0_NET_TM_RECV_QUEUE_DEF_LEN.
	 */
	uint32_t                    ntm_recv_queue_min_length;

	/**
	   Atomic variable tracking the number of buffers needed for the
	   receive queue when automatically provisioning and out of buffers.
	 */
	struct m0_atomic64          ntm_recv_queue_deficit;

	/**
	   The color assigned to the transfer machine for locality
	   support when provisioning from a buffer pool.
	   The value is initialized to @c ~0.
	 */
	uint32_t                    ntm_pool_colour;

	/**
	   Minimum remaining size in a buffer in TM receive queue to allow reuse
	   for multiple messages.
	 */
	m0_bcount_t		    ntm_recv_queue_min_recv_size;

	/**
	   Maximum number of messages that may be received in the buffer in
	   TM Receive queue.
	 */
	uint32_t		    ntm_recv_queue_max_recv_msgs;
};

/**
   Initializes a transfer machine.

   Prior to invocation the following fields should be set:

   - m0_net_transfer_mc.ntm_state should be set to M0_NET_TM_UNDEFINED.
     Zeroing the entire structure has the side effect of setting this value.

   - m0_net_transfer_mc.ntm_callbacks should point to a properly initialized
     struct m0_net_tm_callbacks data structure.

   All fields in the structure other then the above will be set to their
   appropriate initial values.

   @param dom     Network domain pointer.

   @post tm->ntm_bev_auto_deliver is set.
   @post (tm->ntm_pool_colour == M0_NET_BUFFER_POOL_ANY_COLOR &&
          tm->ntm_recv_pool_queue_min_length == M0_NET_TM_RECV_QUEUE_DEF_LEN)
 */
M0_INTERNAL int m0_net_tm_init(struct m0_net_transfer_mc *tm,
			       struct m0_net_domain      *dom);

/**
   Finalizes a transfer machine, releasing any associated
   transport specific resources.

   All application references to end points associated with this transfer
   machine should be released prior to this call.

   @pre
   M0_IN(tm->ntm_state, (M0_NET_TM_STOPPED,
                         M0_NET_TM_FAILED, M0_NET_TM_INITIALIZED)) &&
   ((m0_nep_tlist_is_empty(&tm->ntm_end_points) && tm->ntm_ep == NULL) ||
    (m0_nep_tlist_length(&tm->ntm_end_points) == 1 &&
     m0_nep_tlist_contains(&tm->ntm_end_points, tm->ntm_ep) &&
     m0_atomic64_get(tm->ntm_ep->nep_ref.ref_cnt) == 1))
 */
M0_INTERNAL void m0_net_tm_fini(struct m0_net_transfer_mc *tm);

/**
   Sets the processor affinity of the threads of a transfer machine.
   The transfer machine must be initialized but not yet started.

   Support for this operation is transport specific.
   @param processors Processor bitmap.  The bit map is not referenced
          internally after the subroutine returns.

   @retval -ENOSYS  No affinity support available in the transport.

   @pre tm->ntm_state == M0_NET_TM_INITIALIZED

   @see @ref Processor "Processor API"
   @see @ref bitmap "Bitmap API"
 */
M0_INTERNAL int m0_net_tm_confine(struct m0_net_transfer_mc *tm,
				  const struct m0_bitmap *processors);

/**
   Starts a transfer machine.

   The subroutine does not block the invoker. Instead the state is
   immediately changed to M0_NET_TM_STARTING, and an event will be
   posted to indicate when the transfer machine has transitioned to
   the M0_NET_TM_STARTED state.

   @note It is possible that the state change event be posted before this
   subroutine returns.
   It is guaranteed that the event will be posted on a different thread.

   @param tm  Transfer machine pointer.

   @param addr End point address to associate with the transfer machine.  May
          be null if dynamic addressing is supported by the transport.  The end
          point is created internally and made visible by the ntm_ep field only
          if the start operation succeeds.

   @pre tm->ntm_state == M0_NET_TM_INITIALIZED

   @see m0_net_end_point_create()
 */
M0_INTERNAL int m0_net_tm_start(struct m0_net_transfer_mc *tm,
				const char *addr);

/**
   Initiates the shutdown of a transfer machine.  New messages will
   not be accepted and new end points cannot be created.
   Pending operations will be completed or aborted as desired.

   All end point references must be released by the application prior
   to invocation. The only end point reference that may exist is that of
   this transfer machine itself, and that will be released during fini.

   The subroutine does not block the invoker.  Instead the state is
   immediately changed to M0_NET_TM_STOPPING, and an event will be
   posted to indicate when the transfer machine has transitioned to
   the M0_NET_TM_STOPPED state.

   @note It is possible that the state change event be posted before this
   subroutine returns.
   It is guaranteed that the event will be posted on a different thread.

   @pre M0_IN(tm->ntm_state, (M0_NET_TM_INITIALIZED,
                              M0_NET_TM_STARTING, M0_NET_TM_STARTED))

   @param abort Cancel pending operations. Support for this option is
   transport specific.
 */
M0_INTERNAL int m0_net_tm_stop(struct m0_net_transfer_mc *tm, bool abort);

/**
   Retrieves transfer machine statistics for all or for a single logical queue,
   optionally resetting the data.  The operation is performed atomically
   with respect to on-going transfer machine activity.

   @param qtype Logical queue identifier of the queue concerned. Specify
          M0_NET_QT_NR instead if all the queues are to be considered.

   @param qs Returned statistical data. May be NULL if only a reset operation
          is desired.  Otherwise should point to a single m0_net_qstats data
          structure if the value of <b>qtype</b> is not M0_NET_QT_NR, or else
          should point to an array of M0_NET_QT_NR such structures in which to
          return the statistical data on all the queues.

   @param reset Ttrue iff the associated statistics data should be reset at the
          same time.

   @pre tm->ntm_state >= M0_NET_TM_INITIALIZED
 */
M0_INTERNAL int m0_net_tm_stats_get(struct m0_net_transfer_mc *tm,
				    enum m0_net_queue_type qtype,
				    struct m0_net_qstats *qs, bool reset);

/**
   A transfer machine is notified of non-buffer related events of interest
   with this subroutine.
   Typically, the subroutine is invoked by the transport associated with
   the transfer machine.

   The event data structure is not referenced from elsewhere after this
   subroutine returns, so may be allocated on the stack of the calling thread.

   Multiple concurrent events may be delivered for a given transfer machine.

   The subroutine will also signal to all waiters on the
   m0_net_transfer_mc::ntm_chan field after delivery of the callback.

   The invoking process should be aware that the callback subroutine could
   end up making re-entrant calls to the transport layer.

   @param ev Event pointer. The m0_net_tm_event::nte_tm field identifies the
   transfer machine.

   @see m0_net_tm_buffer_post()
 */
M0_INTERNAL void m0_net_tm_event_post(const struct m0_net_tm_event *ev);

/**
   Associates a buffer pool color with a transfer machine. This helps
   establish an association between a network buffer and the transfer machine
   when provisioning from a buffer pool, which can considerably improve the
   spatial and temporal locality of future provisioning calls from the buffer
   pool.

   Automatically provisioned receive queue network buffers will be allocated
   with the specified color. The application can also use this color when
   provisioning buffers for this transfer machine in other network buffer pool
   use cases.

   If this function is not called, the transfer machine's color is initialized
   to @c ~0.

   @pre M0_IN(tm->ntm_state, (M0_NET_TM_INITIALIZED, M0_NET_TM_STARTED))

   @see m0_net_tm_colour_get(), m0_net_tm_pool_attach()
 */
M0_INTERNAL void m0_net_tm_colour_set(struct m0_net_transfer_mc *tm,
				      uint32_t colour);

/**
   Returns the buffer pool color associated with a transfer machine.
   @see m0_net_tm_colour_set()
 */
M0_INTERNAL uint32_t m0_net_tm_colour_get(struct m0_net_transfer_mc *tm);

/**
   Enables the automatic provisioning of network buffers to the receive
   queue of the transfer machine from the specified network buffer pool.

   Provisioning takes place at the following times:
   - Upon transfer machine startup
   - Prior to delivery of a de-queueud receive message buffer
   - When buffers are returned to an exhausted network buffer pool and there
     are transfer machines that can be re-provisioned from that pool. This
     requires that the application invoke the
     m0_net_domain_buffer_pool_not_empty() subroutine from the pool's not-empty
     callback.
   - When the minimum length of the receive buffer queue is modified.
   @param bufpool Pointer to a network buffer pool.
   @param callbacks Pointer to the callbacks to be set in the provisioned
   network buffer.
   @param min_recv_size Minimum remaining size in a buffer in TM receive queue
   to allow reuse for multiple messages.
   @param max_recv_msgs Maximum number of messages that may be received in the
   buffer in TM receive queue.
   @param min_recv_queue_len Minimum nuber of buffers in TM receive queue.

   @pre tm != NULL
   @pre tm->ntm_state == M0_NET_TM_INITIALIZED
   @pre bufpool != NULL
   @pre callbacks != NULL
   @pre callbacks->nbc_cb[M0_NET_QT_MSG_RECV] != NULL
   @pre min_recv_size > 0
   @pre max_recv_msgs > 0

   @see m0_net_tm_colour_set(), m0_net_domain_buffer_pool_not_empty(),
        m0_net_tm_pool_length_set()
 */
M0_INTERNAL int m0_net_tm_pool_attach(struct m0_net_transfer_mc *tm,
				      struct m0_net_buffer_pool *bufpool,
				      const struct m0_net_buffer_callbacks
				      *callbacks, m0_bcount_t min_recv_size,
				      uint32_t max_recv_msgs,
				      uint32_t min_recv_queue_len);

/**
   Sets the minimum number of network buffers that should be present on the
   receive queue of the transfer machine. If the number falls below this
   value and automatic provisioning is enabled, then additional buffers are
   provisioned as needed.
   Invoking this subroutine may trigger provisioning.
   @param len Minimum receive queue length. The default value is
   M0_NET_TM_RECV_QUEUE_DEF_LEN.
   @see m0_net_tm_pool_attach()
 */
M0_INTERNAL void m0_net_tm_pool_length_set(struct m0_net_transfer_mc *tm,
					   uint32_t len);

/**
   Reprovisions all transfer machines in the network domain of this buffer
   pool, that are associated with the pool.

   The application typically arranges for this subroutine to be called from the
   pool's not-empty callback operation.

   The subroutine should be invoked while holding the pool lock, which is
   normally the case in the pool not-empty callback,
   m0_net_buffer_pool_ops::nbpo_not_empty().

   @param pool A network buffer pool.
   @see m0_net_tm_pool_attach()
 */
M0_INTERNAL void
m0_net_domain_buffer_pool_not_empty(struct m0_net_buffer_pool *pool);

/** Buffer completion events are described by this data structure. */
struct m0_net_buffer_event {
	/** Pointer to the buffer */
	struct m0_net_buffer      *nbe_buffer;

	/** Time the event is posted. */
	m0_time_t                  nbe_time;

	/**
	   Status or error code associated with the event.

	   A 0 in this field implies successful completion, and a negative
	   error number is used to indicate the reasons for failure.

	   The following errors are well defined:

		- -ECANCELED. This is used in buffer release events to indicate
		that the associated buffer operation was cancelled by a call to
		m0_net_buffer_del().

		- -ETIMEDOUT. This is used in buffer release events to indicate
		that the associated buffer operation did not complete before
		the current time exceeded the nb_timeout value.  The support
		for this feature is transport specific.  The nb_timeout value
		is always reset to M0_TIME_NEVER by the time the buffer
		callback is invoked.
	 */
	int32_t                    nbe_status;

	/**
	   Length of the buffer data associated with this event.
	   The field is valid only if the event is posted
	   for the M0_NET_QT_MSG_RECV, M0_NET_QT_PASSIVE_BULK_RECV or
	   M0_NET_QT_ACTIVE_BULK_RECV queues.
	 */
	m0_bcount_t                nbe_length;

	/**
	   Starting offset of the buffer data associated with this event,
	   if the event is posted for the
	   M0_NET_QT_MSG_RECV, M0_NET_QT_PASSIVE_BULK_RECV or
	   M0_NET_QT_ACTIVE_BULK_RECV queues.

	   Provided for future support of multi-delivery buffer transports.
	   Applications should take it into consideration when determining the
	   starting location of the event data in the buffer.
	 */
	m0_bindex_t                nbe_offset;

	/**
	   This field is used only in successful completion of buffers
	   in the received message queue (M0_NET_QT_MSG_RECV).
	   The transport will set the end point to identify the sender
	   of the message before invoking the completion callback on the buffer.

	   The end point will be released when the callback returns, so
	   applications should increment the reference count on the end
	   point with m0_net_end_point_get(), if they wish to dereference
	   the pointer in a different context.
	 */
	struct m0_net_end_point   *nbe_ep;
};

/**
   Buffer callback function pointer type.
   @param ev Pointer to the buffer event. The pointer is not valid after
   the callback returns.
 */
typedef void (*m0_net_buffer_cb_proc_t)(const struct m0_net_buffer_event *ev);

/**
   This data structure contains application callback function pointers
   for buffer completion callbacks, one function per type of buffer queue.

   Applications should provide a pointer to an instance of such a
   structure in the nb_callbacks field of the struct m0_net_buffer.
   Multiple objects can point to a single instance of such a structure.

   @see m0_net_buffer_event_post() for the concurrency semantics.
 */
struct m0_net_buffer_callbacks {
	m0_net_buffer_cb_proc_t nbc_cb[M0_NET_QT_NR];
};

/** Buffer state is tracked using these bitmap flags. */
enum m0_net_buf_flags {
	/** The buffer is registered with the domain. */
	M0_NET_BUF_REGISTERED  = 1 << 0,
	/** The buffer is added to a transfer machine logical queue. */
	M0_NET_BUF_QUEUED      = 1 << 1,
	/** Set when the transport starts using the buffer. */
	M0_NET_BUF_IN_USE      = 1 << 2,
	/** Indicates that the buffer operation has been cancelled. */
	M0_NET_BUF_CANCELLED   = 1 << 3,
	/** Indicates that the buffer operation has timed out. */
	M0_NET_BUF_TIMED_OUT   = 1 << 4,
	/**
	   Set by the transport to indicate that a buffer should not be
	   dequeued in a m0_net_buffer_event_post() call.
	 */
	M0_NET_BUF_RETAIN      = 1 << 5,
};

/**
   This data structure is used to track the memory used for message passing or
   bulk data transfer over the network.

   Support for scatter-gather buffers is provided by use of a m0_bufvec; upper
   layer protocols may impose limitations on the use of scatter-gather,
   especially for message passing.  The transport will impose limitations on
   the number of vector elements and the overall maximum buffer size.

   The invoking application is responsible for the creation of these data
   structures, registering them with the network domain, and providing them to
   a transfer machine for specific operations. As such, memory alignment
   considerations of the encapsulated m0_bufvec are handled by the invoking
   application.

   Once the application initiates an operation on a buffer, it should refrain
   from modifying the buffer until the callback signifying operation
   completion.

   Applications must register buffers with the transport before use, and
   deregister them before shutting down.
 */
struct m0_net_buffer {
	/**
	   Vector pointing to memory associated with this data structure.
	   Initialized by the application prior to registration.
	   It should not be modified until after registration.
	 */
	struct m0_bufvec           nb_buffer;

	/**
	   The actual amount of data to be transferred in the case of adding
	   the buffer to the M0_NET_QT_MSG_SEND, M0_NET_QT_PASSIVE_BULK_SEND or
	   M0_NET_QT_ACTIVE_BULK_SEND queues.

	   The actual amount of valid data received, upon completion of
	   M0_NET_QT_MSG_RECV, M0_NET_QT_PASSIVE_BULK_RECV or
	   M0_NET_QT_ACTIVE_BULK_RECV queue operations, is not set here, but in
	   the nbe_length field of the m0_net_buffer_event instead.
	 */
	m0_bcount_t                nb_length;

	/**
	   The starting offset in the buffer from which the data should
	   be read, in the case of adding a buffer to the
	   M0_NET_QT_MSG_SEND, M0_NET_QT_PASSIVE_BULK_SEND or
	   M0_NET_QT_ACTIVE_BULK_SEND queues.

	   It is transport specific if a non-zero value is supported.
	 */
	m0_bindex_t                nb_offset;

	/**
	   Domain pointer. It is set automatically when the buffer
	   is registered with m0_net_buffer_register().
	   The application should not modify this field.
	 */
	struct m0_net_domain      *nb_dom;

	/**
	   Transfer machine pointer. It is set automatically with
	   every call to m0_net_buffer_add().
	 */
	struct m0_net_transfer_mc *nb_tm;

	/**
	   The application should set this value to identify the logical
	   transfer machine queue before calling m0_net_buffer_add().
	 */
	enum m0_net_queue_type     nb_qtype;

	/**
	    Pointer to application callbacks. Should be set
	    prior to adding the buffer to a transfer machine queue.
	 */
	const struct m0_net_buffer_callbacks *nb_callbacks;

	/**
	   Absolute time by which an operation involving the buffer should
	   stop with failure if not completed.
	   The application should set this field prior to adding the
	   buffer to a transfer machine logical queue.

	   <b>Support for this is transport specific.</b>
	   A value of M0_TIME_NEVER disables the timeout.
	   The value is forced to M0_TIME_NEVER during buffer registration,
	   and reset to the same prior to the invocation of the buffer
	   callback so applications need not bother with this field unless
	   they intend to set a timeout value.

	   Adding a buffer to a logical queue will fail with a -ETIME
	   error code if the specified nb_timeout value is in the past.
	 */
	m0_time_t                  nb_timeout;

	/**
	   Time at which the buffer was added to its logical queue.
	   Set by the m0_net_buffer_add() subroutine and used to
	   compute the time spent in the queue.
	 */
	m0_time_t                  nb_add_time;

	/**
	   Network transport descriptor.

	   The value is set upon return from m0_net_buffer_add() when the
	   buffer is added to the M0_NET_QT_PASSIVE_BULK_RECV or
	   M0_NET_QT_PASSIVE_BULK_SEND queues.

	   Applications should convey the descriptor to the active side to
	   perform the bulk data transfer. The active side application code
	   should set this value when adding the buffer to the
	   M0_NET_QT_ACTIVE_BULK_RECV or M0_NET_QT_ACTIVE_BULK_SEND queues,
	   using the m0_net_desc_copy() subroutine.

	   In both cases, applications are responsible for freeing the memory
	   used by this descriptor with the m0_net_desc_free() subroutine.
	 */
	struct m0_net_buf_desc     nb_desc;

	/**
	   This field identifies an end point in the associated transfer
	   machine.

	   When sending messages the application should specify the end point
	   of the destination before adding the buffer to the
	   M0_NET_QT_MSG_SEND queue.

	   The field is not used for the bulk cases nor for received messages.
	 */
	struct m0_net_end_point   *nb_ep;

	/**
	   Linkage into one of the transfer machine lists that implement the
	   logical queues.
	   There is only one linkage for all of the queues, as a buffer
	   can only be used for one type of operation at a time.

	   For free pool buffers, it is also used for linkage into
	   m0_net_buffer_pool::nbp_colours[].

	   The application should not modify this field.
	 */
	struct m0_tlink		   nb_tm_linkage;

	/** Linkage into a network buffer pool LRU list. */
	struct m0_tlink		   nb_lru;

	/** Application-specific buffer linkage. */
	struct m0_tlink            nb_extern_linkage;

	/** Magic for network buffer list. */
	uint64_t		   nb_magic;

	/**
	   Linkage into one of the domain list that tracks registered buffers.

	   The application should not modify this field.
	 */
	struct m0_list_link        nb_dom_linkage;

	/**
	   Transport private data associated with the buffer.
	   Will be freed when the buffer is deregistered, if not earlier.

	   The application should not modify this field.
	 */
	void                      *nb_xprt_private;

	/**
	   Application specific private data associated with the buffer.
	   It is populated and used by the end user.
	   It is end user's responsibility to use this field to allocate
	   or deallocate any memory regions stored in this field.

	   It is neither verified by net code nor do the net layer
	   invariants touch it.

	   Current users:

               - rpc/bulk uses this for a pointer to m0_rpc_bulk_buf;
               - net/test uses this for a pointer to m0_net_test_network_ctx.
	 */
	void			  *nb_app_private;

	/**
	   Buffer state is tracked with bitmap flags from
	   enum m0_net_buf_flags.

	   The application should initialize this field to 0 prior
	   to registering the buffer with the domain.

	   The application should not modify these flags again until
	   after de-registration.
	 */
	uint64_t                   nb_flags;

	/**
	   Minimum remaining size in a receive buffer to allow reuse
	   for multiple messages.
	   The value may not be 0 for buffers in the M0_NET_QT_MSG_RECV queue.
	 */
	m0_bcount_t                nb_min_receive_size;

	/**
	   Maximum number of messages that may be received in the buffer.
	   The value may not be 0 for buffers in the M0_NET_QT_MSG_RECV queue.
	 */
	uint32_t                   nb_max_receive_msgs;

	/**
	   Set when a buffer is provisioned from a pool using the
	   m0_net_buffer_pool_get() subroutine call.
	 */
	struct m0_net_buffer_pool *nb_pool;

	/** Counts the number of messages received when on the receive queue. */
	uint32_t                   nb_msgs_received;
};

/**
   Registers a buffer with the domain. The domain could perform some
   optimizations under the covers.

   @param buf Pointer to a buffer. The buffer should have the following fields
   initialized:
   - m0_net_buffer.nb_buffer should be initialized to point to the buffer
   memory regions.
   The buffer's timeout value is initialized to M0_TIME_NEVER upon return.

   @pre buf->nb_flags == 0
   @pre buf->nb_buffer.ov_buf != NULL
   @pre m0_vec_count(&buf->nb_buffer.ov_vec) > 0
   @post ergo(result == 0, buf->nb_flags & M0_NET_BUF_REGISTERED)
   @post ergo(result == 0, buf->nb_timeout == M0_TIME_NEVER)

   @param dom Pointer to the domain.
 */
M0_INTERNAL int m0_net_buffer_register(struct m0_net_buffer *buf,
				       struct m0_net_domain *dom);

/**
   Deregisters a previously registered buffer and releases any transport
   specific resources associated with it. The buffer should not be in use, nor
   should this subroutine be invoked from a callback.

   @pre buf->nb_flags == M0_NET_BUF_REGISTERED
   @pre buf->nb_dom == dom
 */
M0_INTERNAL void m0_net_buffer_deregister(struct m0_net_buffer *buf,
					  struct m0_net_domain *dom);

/**
   Adds a registered buffer to a transfer machine's logical queue specified
   by the m0_net_buffer.nb_qtype value.

   - Buffers added to the M0_NET_QT_MSG_RECV queue are used to receive
     messages.

   - When data is contained in the buffer, as in the case of the
     M0_NET_QT_MSG_SEND, M0_NET_QT_PASSIVE_BULK_SEND and
     M0_NET_QT_ACTIVE_BULK_SEND queues, the application must set the
     m0_net_buffer.nb_length field to the actual length of the data to be
     transferred.

   - Buffers added to the M0_NET_QT_MSG_SEND queue must identify the
     message destination end point with the m0_net_buffer.nb_ep field.

   - Buffers added to the M0_NET_QT_PASSIVE_BULK_RECV or
     M0_NET_QT_PASSIVE_BULK_SEND queues must have their m0_net_buffer.nb_ep
     field set to identify the end point that will initiate the bulk data
     transfer.  Upon return from this subroutine the m0_net_buffer.nb_desc
     field will be set to the network buffer descriptor to be conveyed to said
     end point.

   - Buffers added to the M0_NET_QT_ACTIVE_BULK_RECV or
     M0_NET_QT_ACTIVE_BULK_SEND queues must have their m0_net_buffer.nb_desc
     field set to the network buffer descriptor associated with the passive
     buffer.

   - The callback function pointer for the appropriate queue type
     must be set in nb_callbacks.

   The buffer should not be modified until the operation completion
   callback is invoked for the buffer.

   The buffer completion callback is guaranteed to be invoked on a
   different thread.

   @pre buf->nb_dom == tm->ntm_dom
   @pre tm->ntm_state == M0_NET_TM_STARTED
   @pre m0_net__qtype_is_valid(buf->nb_qtype)
   @pre buf->nb_flags == M0_NET_BUF_REGISTERED
   @pre buf->nb_callbacks->nbc_cb[buf->nb_qtype] != NULL
   @pre ergo(buf->nb_qtype == M0_NET_QT_MSG_RECV,
             buf->nb_min_receive_size != 0 && buf->nb_max_receive_msgs != 0)
   @pre ergo(buf->nb_qtype == M0_NET_QT_MSG_SEND, buf->nb_ep != NULL)
   @pre ergo(M0_IN(buf->nb_qtype, (M0_NET_QT_ACTIVE_BULK_RECV,
                                   M0_NET_QT_ACTIVE_BULK_SEND)),
             buf->nb_desc.nbd_len != 0)
   @pre ergo(M0_IN(buf->nb_qtype, (M0_NET_QT_MSG_SEND ||
                                   M0_NET_QT_PASSIVE_BULK_SEND,
                                   M0_NET_QT_ACTIVE_BULK_SEND)),
             buf->nb_length > 0)

   @retval -ETIME nb_timeout is set to other than M0_TIME_NEVER, and occurs in
           the past. Note that this differs from them buffer timeout error code
           of -ETIMEDOUT.

   @note Receiving a successful buffer completion callback is not a guarantee
   that a data transfer actually took place, but merely an indication that the
   transport reported the operation was successfully executed. See the
   transport documentation for details.
 */
M0_INTERNAL int m0_net_buffer_add(struct m0_net_buffer *buf,
				  struct m0_net_transfer_mc *tm);

/**
   Removes a registered buffer from a logical queue, if possible,
   cancelling any operation in progress.

   <b>Cancellation support is provided by the underlying transport.</b> It is
   not guaranteed that actual cancellation of the operation in progress will
   always be supported, and even if it is, there are race conditions in the
   execution of this request and the concurrent invocation of the completion
   callback on the buffer.

   The transport should set the M0_NET_BUF_CANCELLED flag in the buffer if
   the operation has not yet started.  The flag will be cleared by
   m0_net_buffer_event_post().

   The buffer completion callback is guaranteed to be invoked on a
   different thread.

   @pre buf->nb_flags & M0_NET_BUF_REGISTERED

   @retval true (success)
   @retval false (failure, the buffer is not queued)
 */
M0_INTERNAL bool m0_net_buffer_del(struct m0_net_buffer *buf,
				   struct m0_net_transfer_mc *tm);

/**
   A transfer machine is notified of buffer related events with this
   subroutine.

   Typically, the subroutine is invoked by the transport associated with the
   transfer machine.

   The event data structure is not referenced from elsewhere after this
   subroutine returns, so may be allocated on the stack of the calling thread.

   Multiple concurrent events may be delivered for a given buffer, depending
   upon the transport.

   The subroutine will remove a buffer from its queue if the M0_NET_BUF_RETAIN
   flag is @em not set.  It will clear the M0_NET_BUF_QUEUED and
   M0_NET_BUF_IN_USE flags and set the nb_timeout field to M0_TIME_NEVER if the
   buffer is dequeued.  It will always clear the M0_NET_BUF_RETAIN,
   M0_NET_BUF_CANCELLED and M0_NET_BUF_TIMED_OUT flags prior to invoking the
   callback. The M0_NET_BUF_RETAIN flag must not be set if the status indicates
   error.

   If the M0_NET_BUF_CANCELLED flag was set, then the status must be
   -ECANCELED.

   If the M0_NET_BUF_TIMED_OUT flag was set, then the status must be
   -ETIMEDOUT.

   The subroutine will perform a m0_end_point_put() on the ev->nbe_ep, if the
   queue type is M0_NET_QT_MSG_RECV and the nbe_status value is 0, and for the
   M0_NET_QT_MSG_SEND queue to match the m0_end_point_get() made in the
   m0_net_buffer_add() call. Care should be taken by the transport to
   accomodate these adjustments when invoking the subroutine with the
   M0_NET_BUF_RETAIN flag set.

   The subroutine will also signal to all waiters on the
   m0_net_transfer_mc.ntm_chan field after delivery of the callback.

   The invoking process should be aware that the callback subroutine could end
   up making re-entrant calls to the transport layer.

   @param ev Event pointer. The nbe_buffer field identifies the buffer,
   and the buffer's nb_tm field identifies the associated transfer machine.

   @see m0_net_tm_event_post()
 */
M0_INTERNAL void m0_net_buffer_event_post(const struct m0_net_buffer_event *ev);

/**
   Deliver all pending network buffer events, by calling
   m0_net_buffer_event_post() every pending event. Should be called
   periodically by the application if synchronous network buffer event
   processing is enabled.

   @param tm Pointer to a transfer machine which has been set up for
   synchronous network buffer event processing.

   @pre !tm->ntm_bev_auto_deliver

   @see m0_net_buffer_event_deliver_synchronously(),
   m0_net_buffer_event_pending(), m0_net_buffer_event_notify()
 */
M0_INTERNAL void m0_net_buffer_event_deliver_all(struct m0_net_transfer_mc *tm);

/**
   This subroutine disables the automatic delivery of network buffer events.
   Instead, the application should use the m0_net_buffer_event_pending()
   subroutine to check for the presence of events, and the
   m0_net_buffer_event_deliver_all() subroutine to cause pending events to
   be delivered.  The m0_net_buffer_event_notify() subroutine can be used
   to get notified on a wait channel when buffer events arrive.

   Support for this mode of operation is transport specific.

   The subroutine must be invoked before the transfer machine is started.

   @pre tm->ntm_bev_auto_deliver
   @post !tm->ntm_bev_auto_deliver

   @see m0_net_buffer_event_pending(), m0_net_buffer_event_deliver_all(),
   m0_net_buffer_event_notify()
 */
M0_INTERNAL int
m0_net_buffer_event_deliver_synchronously(struct m0_net_transfer_mc *tm);

/**
   This subroutine determines if there are pending network buffer events that
   can be delivered with the m0_net_buffer_event_deliver_all() subroutine.

   @pre !tm->ntm_bev_auto_deliver

   @see m0_net_buffer_event_deliver_synchronously()
 */
M0_INTERNAL bool m0_net_buffer_event_pending(struct m0_net_transfer_mc *tm);

/**
   This subroutine arranges for notification of the arrival of the next network
   buffer event to be signalled on the specified channel. Typically, this
   subroutine is called only when the the m0_net_buffer_event_pending()
   subroutine indicates that there are no events pending. The subroutine does
   not block the invoker.

   @note The subroutine exhibits "monoshot" behavior - it only signals once
   on the specified wait channel.

   @pre !tm->ntm_bev_auto_deliver

   @see m0_net_buffer_event_deliver_synchronously()
 */
M0_INTERNAL void m0_net_buffer_event_notify(struct m0_net_transfer_mc *tm,
					    struct m0_chan *chan);

/** Copies a network buffer descriptor. */
M0_INTERNAL int m0_net_desc_copy(const struct m0_net_buf_desc *from_desc,
				 struct m0_net_buf_desc *to_desc);

/**
   Frees a network buffer descriptor.
   @param desc Specify the network buffer descriptor. Its fields will be
   cleared after this operation.
 */
M0_INTERNAL void m0_net_desc_free(struct m0_net_buf_desc *desc);

/* Descriptor for the tlist of buffers. */
M0_TL_DESCR_DECLARE(m0_net_pool, M0_EXTERN);
M0_TL_DESCR_DECLARE(m0_net_tm, M0_EXTERN);
M0_TL_DECLARE(m0_net_pool, M0_INTERNAL, struct m0_net_buffer);
M0_TL_DECLARE(m0_net_tm, M0_INTERNAL, struct m0_net_buffer);

#ifndef __KERNEL__
/**
 * Checks whether endpoint address is properly formatted.
 *
 * Endpoint address format (ABNF):
 *
 *     endpoint = nid ":12345:" DIGIT+ ":" DIGIT+
 *     ; <network id>:<process id>:<portal number>:<transfer machine id>
 *     ;
 *     nid      = "0@lo" / (ipv4addr  "@" ("tcp" / "o2ib") [DIGIT])
 *     ipv4addr = 1*3DIGIT "." 1*3DIGIT "." 1*3DIGIT "." 1*3DIGIT ; 0..255
 */
M0_INTERNAL bool m0_net_endpoint_is_valid(const char *endpoint);
#endif

/** @} end of networking group */
#endif /* __MERO_NET_NET_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
