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
 * Original author: Nikita_Danilov <Nikita_Danilov@xyratex.com>
 *		    Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 06/27/2012
 */

#pragma once

#ifndef __MERO_RPC_BULK_H__
#define __MERO_RPC_BULK_H__

/**
   @addtogroup rpc

   @{

   @section bulkclientDFSrpcbulk RPC layer abstraction over bulk IO.

   @addtogroup bulkclientDFS
   @{

   Detailed Level Design for bulk IO interface from rpc layer.
   Mero rpc layer, network layer and the underlying transport are
   supposed to constitute a zero-copy path for data IO.
   In order to do this, rpc layer needs to provide support for
   bulk interface exported by network layer which gives the capability
   to bundle IO buffers together and send/receive these buffer descriptors
   on demand. The underlying transport should have the capabilities
   to provide zero-copy path (e.g. RDMA).
   There are 2 major use cases here - read IO and write IO in which
   bulk interface is needed.
   The bulk IO interface from network layer provides abstractions like
   - m0_net_buffer (a generic buffer identified at network layer) and
   - m0_net_buf_desc (an identifier to point to a m0_net_buffer).

   Whenever data buffers are encountered in rpc layer, rpc layer
   (especially formation sub-component) is supposed to take care of
   segregating these rpc items and register m0_net_buffers where
   data buffers are encountered (during write request and read reply)
   and buffer descriptors are copied into rpc items after registering
   net buffers.
   These descriptors are sent to the other side which asks for
   buffers identified by the supplied buffer descriptors.

   Sequence of events in case of write IO call for rpc layer.
   Assumptions
   - Write request call has IO buffers associated with it.
   - Underlying transport supports zero-copy.

   @msc
   client,crpc,srpc,server;

   client=>crpc [ label = "Incoming write request" ];
   client=>crpc [ label = "Adds pages to rpc bulk" ];
   crpc=>crpc [ label = "Populates net buf desc from IO fop" ];
   crpc=>crpc [ label = "net buffer enqueued to
		M0_NET_QT_PASSIVE_BULK_SEND queue" ];
   crpc=>srpc [ label = "Sends fop over wire" ];
   srpc=>server [ label = "IO fop submitted to ioservice" ];
   server=>srpc [ label = "Adds pages to rpc bulk" ];
   server=>srpc [ label = "Net buffer enqueued to M0_NET_QT_ACTIVE_BULK_RECV
		  queue" ];
   server=>srpc [ label = "Start zero copy" ];
   srpc=>crpc [ label = "Start zero copy" ];
   crpc=>srpc [ label = "Zero copy complete" ];
   srpc=>server [ label = "Zero copy complete" ];
   server=>server [ label = "Dispatch IO request" ];
   server=>server [ label = "IO request complete" ];
   server=>srpc [ label = "Send reply fop" ];
   srpc=>crpc [ label = "Send reply fop" ];
   crpc=>client [ label = "Reply received" ];

   @endmsc

   Sequence of events in case of read IO call for rpc layer.
   Assumptions
   - Read request fop has IO buffers associated with it. These buffers are
     actually empty, they contain no user data. These buffers are replaced
     by m0_net_buf_desc and packed with rpc.
   - Read reply fop consists of number of bytes read.
   - Underlying transport supports zero-copy.

   @msc
   client,crpc,srpc,server;

   client=>crpc [ label = "Incoming read request" ];
   client=>crpc [ label = "Adds pages to rpc bulk" ];
   crpc=>crpc [ label = "Populates net buf desc from IO fop" ];
   crpc=>crpc [ label = "net buffer enqueued to
		M0_NET_QT_PASSIVE_BULK_RECV queue" ];
   crpc=>srpc [ label = "Sends fop over wire" ];
   srpc=>server [ label = "IO fop submitted to ioservice" ];
   server=>srpc [ label = "Adds pages to rpc bulk" ];
   server=>srpc [ label = "Net buffer enqueued to M0_NET_QT_ACTIVE_BULK_SEND
		  queue" ];
   server=>server [ label = "Dispatch IO request" ];
   server=>server [ label = "IO request complete" ];
   server=>srpc [ label = "Start zero copy" ];
   srpc=>crpc [ label = "Start zero copy" ];
   crpc=>srpc [ label = "Zero copy complete" ];
   srpc=>server [ label = "Zero copy complete" ];
   srpc=>crpc [ label = "Send reply fop" ];
   crpc=>client [ label = "Reply received" ];

   @endmsc
 */

#include "lib/vec.h"
#include "lib/tlist.h"
#include "lib/mutex.h"
#include "lib/chan.h"
#include "net/net.h"
#include "xcode/xcode.h"          /* M0_XCA_ENUM */

struct m0_rpc_conn;

/**
   Enum to identify the state of bulk operation going on.
 */
enum m0_rpc_bulk_op_states {
	M0_RPC_BULK_OP_INITIATE,
	M0_RPC_BULK_OP_FINISH,
} M0_XCA_ENUM;

/**
   Represents attributes of struct m0_rpc_bulk_buf.
 */
enum {
	/**
	 * The net buffer belonging to struct m0_rpc_bulk_buf is
	 * allocated by rpc bulk APIs.
	 * So it should be deallocated by rpc bulk APIs as well.
	 */
	M0_RPC_BULK_NETBUF_ALLOCATED   = (1 << 0),
	/**
	 * The net buffer belonging to struct m0_rpc_bulk_buf is
	 * registered with net domain by rpc bulk APIs.
	 * So it should be deregistered by rpc bulk APIs as well.
	 */
	M0_RPC_BULK_NETBUF_REGISTERED  = (1 << 1),
	/**
	 * The net buffer has been queued to TM's queue.
	 */
	M0_RPC_BULK_NETBUF_QUEUED      = (1 << 2),

	/**
	 * RPC bulk operation timeout is kept as 60 sec.
	 */
	M0_RPC_BULK_TMO = 60
};

/**
   Represents rpc bulk equivalent of a m0_net_buffer. Contains a net buffer
   pointer, a zero vector which does all the in-memory manipulations
   and a backlink to m0_rpc_bulk structure to report back the status.
 */
struct m0_rpc_bulk_buf {
	/** Magic constant to verify sanity of data. */
	uint64_t              bb_magic;
	/** Net buffer containing IO data. */
	struct m0_net_buffer *bb_nbuf;
	/** Zero vector pointing to user data. */
	struct m0_0vec        bb_zerovec;
	/** Linkage into list of m0_rpc_bulk_buf hanging off
	    m0_rpc_bulk::rb_buflist. */
	struct m0_tlink       bb_link;
	/** Back link to parent m0_rpc_bulk structure. */
	struct m0_rpc_bulk   *bb_rbulk;
	/** Flags bearing attributes of m0_rpc_bulk_buf structure. */
	uint64_t              bb_flags;
};

/**
   Adds a m0_rpc_bulk_buf structure to the list of such structures in a
   m0_rpc_bulk structure.
   @param segs_nr Number of segments needed in new m0_rpc_bulk_buf
   structure.
   @param length Number of bytes in the buffer. If 0, computed as total size of
   databufs.
   @param netdom The m0_net_domain structure to which new m0_rpc_bulk_buf
   structure will belong to. It is primarily used to keep a check on
   thresholds like max_seg_size, max_buf_size and max_number_of_segs.
   @param nb Net buf pointer if user wants to use preallocated network
   buffer. (nb == NULL) implies that net buffer should be allocated by
   m0_rpc_bulk_buf_add().
   @param out Out parameter through which newly created m0_rpc_bulk_buf
   structure is returned back to the caller.
   Users need not remove the m0_rpc_bulk_buf structures manually.
   These structures are removed by rpc bulk callback.
   @see rpc_bulk_buf_cb().
   @pre rbulk != NULL && segs_nr != 0.
   @post (rc == 0 && *out != NULL) || rc != 0.
   @see m0_rpc_bulk.
 */
M0_INTERNAL int m0_rpc_bulk_buf_add(struct m0_rpc_bulk *rbulk,
				    uint32_t segs_nr, m0_bcount_t length,
				    struct m0_net_domain *netdom,
				    struct m0_net_buffer *nb,
				    struct m0_rpc_bulk_buf **out);

/**
   Adds a data buffer to zero vector referred to by rpc bulk structure.
   @param rbulk rpc bulk structure to which data buffer will be added.
   @param buf User space buffer starting address.
   @param count Number of bytes in user space buffer.
   @param index Index of target object to which io is targeted.
   @param netdom Net domain to which the net buffer from m0_rpc_bulk_buf
   belongs.
   @pre buf != NULL && count != 0 && netdom != NULL &&
   rpc_bulk_invariant(rbulk).
   @post rpc_bulk_invariant(rbulk).
 */
M0_INTERNAL int m0_rpc_bulk_buf_databuf_add(struct m0_rpc_bulk_buf *rbuf,
					    void *buf,
					    m0_bcount_t count,
					    m0_bindex_t index,
					    struct m0_net_domain *netdom);

/**
   An abstract data structure that avails bulk transport for io operations.
   End users will register the IO vectors using this structure and bulk
   transfer apis will take care of doing the data transfer in zero-copy
   fashion.
   These APIs are primarily used by another in-memory structure m0_io_fop.
   @see m0_io_fop.
   @note Passive entities engaging in bulk transfer do not block for
   m0_rpc_bulk callback. Only active entities are blocked since they
   can not proceed until bulk transfer is complete.
   @see rpc_bulk_buf_cb().
 */
struct m0_rpc_bulk {
	/** Magic to verify sanity of struct m0_rpc_bulk. */
	uint64_t        rb_magic;
	/** Mutex to protect access on list rb_buflist. */
	struct m0_mutex rb_mutex;
	/**
	 * List of m0_rpc_bulk_buf structures linked through
	 * m0_rpc_bulk_buf::rb_link.
	 */
	struct m0_tl    rb_buflist;
	/** Channel to wait on rpc bulk to complete the io. */
	struct m0_chan  rb_chan;
	/** Number of bytes read/written through this structure. */
	m0_bcount_t     rb_bytes;
	/**
	 * Return value of operations like addition of buffers to transfer
	 * machine and zero-copy operation. This field is updated by
	 * net buffer send/receive callbacks.
	 */
	int32_t         rb_rc;
	uint64_t        rb_id;
};

/**
   Initializes a rpc bulk structure.
   @param rbulk rpc bulk structure to be initialized.
   @pre rbulk != NULL.
   @post rpc_bulk_invariant(rbulk).
 */
M0_INTERNAL void m0_rpc_bulk_init(struct m0_rpc_bulk *rbulk);

/**
   Removes all m0_rpc_bulk_buf structures from list of such structures in
   m0_rpc_bulk structure and deallocates it.
   @pre rbulk != NULL.
   @post rpcbulk_tlist_length(&rbulk->rb_buflist) == 0.
 */
M0_INTERNAL void m0_rpc_bulk_buflist_empty(struct m0_rpc_bulk *rbulk);

/**
   Finalizes the rpc bulk structure.
   @pre rbulk != NULL && rpc_bulk_invariant(rbulk).
 */
M0_INTERNAL void m0_rpc_bulk_fini(struct m0_rpc_bulk *rbulk);

/**
   Enum to identify the type of bulk operation going on.
 */
enum m0_rpc_bulk_op_type {
	/**
	 * Store the net buf descriptors from net buffers to io fops.
	 * Typically used by bulk client.
	 */
	M0_RPC_BULK_STORE = (1 << 0),
	/**
	 * Load the net buf descriptors from io fops to destination
	 * net buffers.
	 * Typically used by bulk server.
	 */
	M0_RPC_BULK_LOAD  = (1 << 1),
} M0_XCA_ENUM;

/**
   Assigns queue type for buffers maintained in rbulk->rb_buflist from
   argument q.
   @param rbulk m0_rpc_bulk structure containing list of m0_rpc_bulk_buf
   structures whose net buffers queue type has to be assigned.
   @param q Queue type for m0_net_buffer structures.
   @pre rbulk != NULL && !m0_tlist_is_empty(rbulk->rb_buflist) &&
   m0_mutex_is_locked(&rbulk->rb_mutex) &&
   q == M0_NET_QT_PASSIVE_BULK_RECV || q == M0_NET_QT_PASSIVE_BULK_SEND ||
   q == M0_NET_QT_ACTIVE_BULK_RECV  || q == M0_NET_QT_ACTIVE_BULK_SEND.
 */
M0_INTERNAL void m0_rpc_bulk_qtype(struct m0_rpc_bulk *rbulk,
				   enum m0_net_queue_type q);

/**
   Stores the m0_net_buf_desc/s for net buffer/s pointed to by m0_rpc_bulk_buf
   structure/s in the io fop wire format.
   This API is typically invoked by bulk client in a zero-copy buffer
   transfer.
   @param rbulk Rpc bulk structure from whose list of m0_rpc_bulk_buf
   structures, the net buf descriptors of io fops will be populated.
   @param conn The m0_rpc_conn object that represents the rpc connection
   made with receiving node.
   @param to_desc Net buf descriptor from fop which will be populated.
   @pre rbuf != NULL && item != NULL && to_desc != NULL &&
   (rbuf->bb_nbuf & M0_NET_BUF_REGISTERED) &&
   (rbuf->bb_nbuf.nb_qtype == M0_NET_QT_PASSIVE_BULK_RECV ||
    rbuf->bb_nbuf.nb_qtype == M0_NET_QT_PASSIVE_BULK_SEND).
   @post rpc_bulk_invariant(rbulk).
 */
M0_INTERNAL int
m0_rpc_bulk_store(struct m0_rpc_bulk                   *rbulk,
		  const struct m0_rpc_conn             *conn,
		  struct m0_net_buf_desc_data          *to_desc,
		  const struct m0_net_buffer_callbacks *bulk_cb);

/**
   Loads the m0_net_buf_desc/s pointing to net buffer/s contained by
   m0_rpc_bulk_buf structure/s in rbulk->rb_buflist and starts RDMA transfer
   of buffers.
   This API is typically used by bulk server in a zero-copy buffer transfer.
   @param rbulk Rpc bulk structure from whose list of m0_rpc_bulk_buf
   structures, net buffers will be added to transfer machine.
   @param conn The m0_rpc_conn object which represents the rpc connection
   made with receiving node.
   @param from_desc The source net buf descriptor which points to the source
   buffer from which data is copied.
   @pre rbuf != NULL && item != NULL && from_desc != NULL &&
   (rbuf->bb_nbuf & M0_NET_BUF_REGISTERED) &&
   (rbuf->bb_nbuf.nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV ||
    rbuf->bb_nbuf.nb_qtype == M0_NET_QT_ACTIVE_BULK_SEND).
   @post rpc_bulk_invariant(rbulk).
 */
M0_INTERNAL int
m0_rpc_bulk_load(struct m0_rpc_bulk                   *rbulk,
		 const struct m0_rpc_conn             *conn,
		 struct m0_net_buf_desc_data          *from_desc,
		 const struct m0_net_buffer_callbacks *bulk_cb);

/**
 * Does exactly opposite of what m0_rpc_bulk_store() does.
 * Should be called only when m0_rpc_bulk_store() had succeeded
 * earlier and later due to some other condition, bulk transfer
 * did not happen.
 * Now the already stored net buffers need to be
 * deleted from m0_net_transfer_mc so that m0_rpc_bulk object can be
 * finalised.
 * Invocation of this API should be followed by wait on m0_rpc_bulk::rb_chan.
 * @pre  rbulk != NULL && rbulk->rb_rc == 0 && rpc_bulk_invariant(rbulk).
 * @post rpcbulk_tlist_is_empty(&rbulk->rb_buflist).
 */
M0_INTERNAL void m0_rpc_bulk_store_del(struct m0_rpc_bulk *rbulk);

/**
 * Like m0_rpc_bulk_store_del() but deletes unqueued buffers only.
 * Requires rbulk locked by user.
 * @return number of deleted unqueued bufs.
 */
M0_INTERNAL size_t m0_rpc_bulk_store_del_unqueued(struct m0_rpc_bulk *rbulk);

M0_INTERNAL void m0_rpc_bulk_default_cb(const struct m0_net_buffer_event *evt);

extern const struct m0_net_buffer_callbacks m0_rpc__buf_bulk_cb;

M0_INTERNAL size_t m0_rpc_bulk_buf_length(struct m0_rpc_bulk *rbulk);

M0_INTERNAL bool m0_rpc_bulk_is_empty(struct m0_rpc_bulk *rbulk);

/** @} bulkclientDFS end group */

/** @} end of rpc group */

#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
