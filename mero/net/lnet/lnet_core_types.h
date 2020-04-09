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
 * Original creation date: 4/04/2012
 */

#pragma once

#ifndef __MERO_NET_LNET_TYPES_H__
#define __MERO_NET_LNET_CORE_TYPES_H__

#include "net/net.h"  /* m0_net_queue_type */

/* forward references */
struct nlx_core_bev_link;
struct nlx_core_bev_cqueue;
struct nlx_core_buffer;
struct nlx_core_buffer_event;
struct nlx_core_domain;
struct nlx_core_ep_addr;
struct nlx_core_transfer_mc;
struct nlx_core_buf_desc;
struct page;

/**
   @addtogroup LNetCore
   @{
 */

/**
   Opaque type wide enough to represent an address in any address space.
 */
typedef uint64_t nlx_core_opaque_ptr_t;
M0_BASSERT(sizeof(nlx_core_opaque_ptr_t) >= sizeof(void *));

/**
   This structure defines the fields in an LNet transport end point address.
   It is packed to minimize the network descriptor size.
 */
struct nlx_core_ep_addr {
	uint64_t cepa_nid;    /**< The LNet Network Identifier */
	uint32_t cepa_pid;    /**< The LNet Process Identifier */
	uint32_t cepa_portal; /**< The LNet Portal Number */
	uint32_t cepa_tmid;   /**< The Transfer Machine Identifier */
} __attribute__((__packed__));

/* Match bit related definitions */
enum {
	/** Number of bits used for TM identifier */
	M0_NET_LNET_TMID_BITS      = 12,
	/** Shift to the TMID position (52) */
	M0_NET_LNET_TMID_SHIFT     = 64 - M0_NET_LNET_TMID_BITS,
	/** Max TM identifier is 2^12-1 (4095) */
	M0_NET_LNET_TMID_MAX       = (1 << M0_NET_LNET_TMID_BITS) - 1,
	/** Invalid value used for dynamic addressing */
	M0_NET_LNET_TMID_INVALID   = M0_NET_LNET_TMID_MAX+1,
	/** Number of bits used for buffer identification (52) */
	M0_NET_LNET_BUFFER_ID_BITS = 64 - M0_NET_LNET_TMID_BITS,
	/** Minimum buffer match bit counter value */
	M0_NET_LNET_BUFFER_ID_MIN  = 1,
	/** Maximum buffer match bit counter value: 2^52-1 (0xfffffffffffff) */
	M0_NET_LNET_BUFFER_ID_MAX  = (1ULL << M0_NET_LNET_BUFFER_ID_BITS) - 1,
	/** Buffer match bit mask */
	M0_NET_LNET_BUFFER_ID_MASK = M0_NET_LNET_BUFFER_ID_MAX,
};
M0_BASSERT(M0_NET_LNET_TMID_BITS + M0_NET_LNET_BUFFER_ID_BITS <= 64);

/**
 * A kernel memory location, in terms of page and offset.
 */
struct nlx_core_kmem_loc {
	union {
		struct {
			/** Page containing the object. */
			struct page *kl_page;
			/** Offset of the object in the page. */
			uint32_t     kl_offset;
		} __attribute__((__packed__));
		uint32_t     kl_data[3];
	};
	/** A checksum of the page and offset, to detect corruption. */
	uint32_t     kl_checksum;
};
M0_BASSERT(sizeof(((struct nlx_core_kmem_loc*) NULL)->kl_page) +
	   sizeof(((struct nlx_core_kmem_loc*) NULL)->kl_offset) ==
	   sizeof(((struct nlx_core_kmem_loc*) NULL)->kl_data));

enum {
	/** Maximum size of an LNET NID string, same as LNET_NIDSTR_SIZE */
	M0_NET_LNET_NIDSTR_SIZE = 32,
};

/**
   Buffer events are linked in the buffer queue using this structure. It is
   designed to be operated upon from either kernel or user space with a single
   producer and single consumer.
 */
struct nlx_core_bev_link {
	/**
	   Self pointer in the consumer (transport) address space.
	 */
	nlx_core_opaque_ptr_t cbl_c_self;

	/**
	   Pointer to the next element in the consumer address space.
	 */
	nlx_core_opaque_ptr_t cbl_c_next;

	/**
	   Self reference in the producer (kernel).
	   The producer reference is kept in the form of a nlx_core_kmem_loc
	   so that queue elements do not all need to be mapped.
	 */
	struct nlx_core_kmem_loc cbl_p_self_loc;

	/**
	   Reference to the next element in the producer.
	   The next reference is kept in the form of a nlx_core_kmem_loc so
	   that queue elements do not all need to be mapped.
	 */
	struct nlx_core_kmem_loc cbl_p_next_loc;
};

/**
   Buffer event queue, operable from either kernel and user space
   with a single producer and single consumer.
 */
struct nlx_core_bev_cqueue {
	/** Number of elements currently in the queue. */
	size_t                 cbcq_nr;

	/** Number of elements in the queue that can be consumed. */
	struct m0_atomic64     cbcq_count;

	/**
	   The consumer removes elements from this anchor.
	   The consumer pointer value is in the address space of the
	   consumer (transport).
	 */
	nlx_core_opaque_ptr_t cbcq_consumer;

	/**
	   The producer adds links to this anchor.
	   The producer reference is kept in the form of a nlx_core_kmem_loc
	   so that queue elements do not all need to be mapped.
	 */
	struct nlx_core_kmem_loc cbcq_producer_loc;
};

enum {
	/** Minimum number of buffer event entries in the queue. */
	M0_NET_LNET_BEVQ_MIN_SIZE  = 2,
	/** Number of reserved buffer event entries in the queue.
	    The entry pointed to by the consumer is owned by the consumer and
	    thus cannot be used by the producer.
	    It will eventually be used when the pointers advance.
	 */
	M0_NET_LNET_BEVQ_NUM_RESERVED = 1,
};

/**
   This structure describes a buffer event. It is very similar to
   struct m0_net_buffer_event.
 */
struct nlx_core_buffer_event {
	/** Linkage in the TM buffer event queue */
	struct nlx_core_bev_link     cbe_tm_link;

	/**
	    This value is set by the kernel Core module's LNet event handler,
	    and is copied from the nlx_core_buffer::cb_buffer_id
	    field. The value is a pointer to the m0_net_buffer structure in the
	    transport address space.
	 */
	nlx_core_opaque_ptr_t        cbe_buffer_id;

	/** Event timestamp, relative to the buffer add time */
	m0_time_t                    cbe_time;

	/** Status code (-errno). 0 is success */
	int32_t                      cbe_status;

	/** Length of data in the buffer */
	m0_bcount_t                  cbe_length;

	/** Offset of start of the data in the buffer. (Receive only) */
	m0_bindex_t                  cbe_offset;

	/** Address of the other end point.  (unsolicited Receive only)  */
	struct nlx_core_ep_addr      cbe_sender;

	/** True if the buffer is no longer in use */
        bool                         cbe_unlinked;

	/** Core kernel space private. */
	void                        *cbe_kpvt;
};

/**
   Core domain data.  The transport layer should embed this in its private data.
 */
struct nlx_core_domain {
	void    *cd_upvt; /**< Core user space private */
	void    *cd_kpvt; /**< Core kernel space private */
	unsigned _debug_;
};

/**
   Core transfer machine data.  The transport layer should embed this in its
   private data.
 */
struct nlx_core_transfer_mc {
	uint64_t                   ctm_magic;

	/** The transfer machine address. */
	struct nlx_core_ep_addr    ctm_addr;

	/**
	   Buffer completion event queue.  The queue is shared between the
	   transport address space and the kernel.
	 */
	struct nlx_core_bev_cqueue ctm_bevq;

	/**
	   Count of bevq entries needed. Incremented by each nlx_xo_buf_add()
	   operation (not necessarily by 1), and decremented when the
	   buffer is unlinked by LNet, in nlx_xo_bev_deliver_all().
	 */
	size_t                     ctm_bev_needed;

	/** Match bit counter.
	    Range [M0_NET_LNET_BUFFER_ID_MIN, M0_NET_LNET_BUFFER_ID_MAX].
	*/
	uint64_t                   ctm_mb_counter;

	void                      *ctm_upvt; /**< Core user space private */
	void                      *ctm_kpvt; /**< Core kernel space private */

	unsigned                   _debug_;
};

/**
   Core buffer data.  The transport layer should embed this in its private data.
 */
struct nlx_core_buffer {
	uint64_t                cb_magic;

	/**
	   The address of the m0_net_buffer structure in the transport address
	   space. The value is set by the nlx_core_buffer_register()
	   subroutine.
	 */
	nlx_core_opaque_ptr_t   cb_buffer_id;

	/**
	   The buffer queue type - copied from m0_net_buffer::nb_qtype
	   when the buffer operation is initiated.
	 */
        enum m0_net_queue_type  cb_qtype;

	/**
	   The length of data involved in the operation.
	   Note this is less than or equal to the buffer length.
	 */
	m0_bcount_t             cb_length;

	/**
	   Value from nb_min_receive_size for receive queue buffers only.
	 */
	m0_bcount_t             cb_min_receive_size;

	/**
	   Value from nb_max_receive_msgs for receive queue buffers.
	   Set to 1 in other cases.
	   The value is used for the threshold field of an lnet_md_t, and
	   specifies the number of internal buffer event structures that
	   have to be provisioned to accommodate the expected result
	   notifications.
	 */
	uint32_t                cb_max_operations;

	/**
	   The match bits for a passive bulk buffer, including the TMID field.
	   They should be set using the nlx_core_buf_desc_encode()
	   subroutine.

	   The field is also used in an active buffer to describe the match
	   bits of the remote passive buffer.

	   The field is set automatically for receive buffers.
	 */
	uint64_t                cb_match_bits;

	/**
	   The address of the destination transfer machine is set in this field
	   for buffers on the M0_NET_QT_MSG_SEND queue.

	   The address of the remote passive transfer machine is set in this
	   field for buffers on the M0_NET_QT_ACTIVE_BULK_SEND or
	   M0_NET_QT_ACTIVE_BULK_RECV queues.
	 */
	struct nlx_core_ep_addr cb_addr;

	void                   *cb_upvt; /**< Core user space private */
	void                   *cb_kpvt; /**< Core kernel space private */
};

/**
   The LNet transport's Network Buffer Descriptor format.
   The external form is the opaque m0_net_buf_desc.
   All fields are stored in little-endian order, and the structure is
   copied as-is to the external opaque form.
 */
struct nlx_core_buf_desc {
	union {
		struct {
			/** Match bits of the passive buffer */
			uint64_t                 cbd_match_bits;

			/** Passive TM's end point */
			struct nlx_core_ep_addr  cbd_passive_ep;

			/** Passive buffer queue type (enum m0_net_queue_type)
			    expressed here explicitly as a 32 bit number.
			*/
			uint32_t                 cbd_qtype;

			/** Passive buffer size */
			m0_bcount_t              cbd_size;
		};
		uint64_t         cbd_data[5];
	};
	uint64_t         cbd_checksum;
};

/** @} */ /* LNetCore */

#endif /* __MERO_NET_LNET_CORE_TYPES_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
