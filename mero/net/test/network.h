/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 03/22/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_NETWORK_H__
#define __MERO_NET_TEST_NETWORK_H__

#include "net/net.h"
#include "net/test/serialize.h"		/* m0_net_test_serialize_op */
#include "net/test/slist.h"		/* m0_net_test_slist */

/**
   @defgroup NetTestNetworkDFS Network
   @ingroup NetTestDFS

   @see
   @ref net-test

   @todo m0_net_test_network_ prefix is too long. rename and align.
   @todo s/uint32_t/size_t/

   @{
 */

enum m0_net_test_network_buf_type {
	M0_NET_TEST_BUF_BULK,	/**< Buffer for the bulk transfers. */
	M0_NET_TEST_BUF_PING,	/**< Buffer for the message transfers. */
};

struct m0_net_test_network_ctx;

/**
   Callback for a network context buffer operations.
   @param ctx Network context.
   @param buf_index Buffer index within network context.
   @param ev Buffer event.
 */
typedef void (*m0_net_test_network_buffer_cb_proc_t)
	(struct m0_net_test_network_ctx	  *ctx,
	 const uint32_t			   buf_index,
	 enum m0_net_queue_type		   q,
	 const struct m0_net_buffer_event *ev);

/** Callbacks for a network context buffers. */
struct m0_net_test_network_buffer_callbacks {
	m0_net_test_network_buffer_cb_proc_t ntnbc_cb[M0_NET_QT_NR];
};

/** Timeouts for each type of transfer machine queue */
struct m0_net_test_network_timeouts {
	m0_time_t ntnt_timeout[M0_NET_QT_NR];
};

/**
 * Net-test network context configuration.
 * This structure is embedded into m0_net_test_network_ctx.
 */
struct m0_net_test_network_cfg {
	/** transfer machine callbacks */
	struct m0_net_tm_callbacks		    ntncfg_tm_cb;
	/** buffer callbacks for every type of network queue */
	struct m0_net_test_network_buffer_callbacks ntncfg_buf_cb;
	/** size of ping buffers */
	m0_bcount_t				    ntncfg_buf_size_ping;
	/** number of ping buffers */
	uint32_t				    ntncfg_buf_ping_nr;
	/** size of bulk buffers */
	m0_bcount_t				    ntncfg_buf_size_bulk;
	/** number of bulk buffers */
	uint32_t				    ntncfg_buf_bulk_nr;
	/** maximum number of endpoints in context */
	uint32_t				    ntncfg_ep_max;
	/**
	 * Timeouts for every type of network buffer.
	 * @see m0_net_test_network_timeouts_never()
	 */
	struct m0_net_test_network_timeouts	    ntncfg_timeouts;
	/** transfer machine should use synchronous event delivery */
	bool					    ntncfg_sync;
};

/**
   Net-test network context structure.
   Contains transfer machine, tm and buffer callbacks, endpoints,
   ping and bulk message buffers.
 */
struct m0_net_test_network_ctx {
	/** Network context configuration. */
	struct m0_net_test_network_cfg		    ntc_cfg;
	/** Network domain. */
	struct m0_net_domain			   *ntc_dom;
	/** Transfer machine. */
	struct m0_net_transfer_mc		   *ntc_tm;
	/** Array of message buffers. Used for message send/recv. */
	struct m0_net_buffer			   *ntc_buf_ping;
	/** Array of buffers for bulk transfer. */
	struct m0_net_buffer			   *ntc_buf_bulk;
	/**
	   Array of pointers to endpoints.
	   Initially this array have no endpoints, but they can
	   be added to this array sequentually, one by one using
	   m0_net_test_network_ep_add().
	   Endpoints are freed in m0_net_test_network_ctx_fini().
	 */
	struct m0_net_end_point			  **ntc_ep;
	/** Current number of endpoints in ntc_ep array. */
	uint32_t				    ntc_ep_nr;
};

/**
   Initialize net-test network context.
   Allocate ping and bulk buffers.
   @param ctx net-test network context structure.
   @param cfg net-test network context configuration. Function will make
	      make a copy of this structure in ctx, so there is no need to
	      keep cfg valid until m0_net_test_network_ctx_fini().
   @param tm_addr transfer machine address (example: "0@lo:12345:42:1024")
   @note if cfg.ntncfg_sync parameter is set, then
   m0_net_buffer_event_deliver_synchronously() will be called for transfer
   machine and m0_net_buffer_event_deliver_all() should be used for buffer
   event delivery.
   @see m0_net_test_network_ctx
   @see m0_net_test_network_cfg
   @pre ctx     != NULL
   @pre cfg	!= NULL
   @pre tm_addr != NULL
   @post m0_net_test_network_ctx_invariant(ctx)
   @return 0 (success)
   @return -ECONNREFUSED m0_net_tm_start() failed.
   @return -errno (failire)
   @todo create configuration structure instead a lot of parameters
 */
int m0_net_test_network_ctx_init(struct m0_net_test_network_ctx *ctx,
				 struct m0_net_test_network_cfg *cfg,
				 const char *tm_addr);
/** Finalize net-test network context */
void m0_net_test_network_ctx_fini(struct m0_net_test_network_ctx *ctx);
/** Invariant for net-test network context */
bool m0_net_test_network_ctx_invariant(struct m0_net_test_network_ctx *ctx);

/**
   Add endpoint to m0_net_test_network_ctx structure.
   @return endpoint number.
   @return -E2BIG ctx->ntc_ep already contains maximum number of endpoints.
   @return -errno (if failure)
   @pre m0_net_test_network_ctx_invariant(ctx)
   @pre ep_addr != NULL
 */
int m0_net_test_network_ep_add(struct m0_net_test_network_ctx *ctx,
			       const char *ep_addr);

/**
   Add endpoints to m0_net_test_network_ctx structure.
   If some endpoint addition fails, then no endpoints will be added to
   network context and all added endpoints will be m0_net_end_point_put()'ed.
   @return -E2BIG ctx->ntc_ep already contains maximum number of endpoints.
   @pre m0_net_test_network_ctx_invariant(ctx)
   @pre m0_net_test_slist_invariant(eps)
   @post m0_net_test_network_ctx_invariant(ctx)
 */
int m0_net_test_network_ep_add_slist(struct m0_net_test_network_ctx *ctx,
				     const struct m0_net_test_slist *eps);

/**
   Add message buffer to network messages send queue.
   @param ctx Net-test network context.
   @param buf_ping_index Index of buffer in ctx->ntc_buf_ping array.
   @param ep_index Entry point index in ctx->ntc_ep array. Entry points
	           should be added to this array prior to calling this
		   function using m0_net_test_network_ep_add().
		   Message will be sent to this endpoint.
 */
int m0_net_test_network_msg_send(struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_ping_index,
				 uint32_t ep_index);

/**
   Add message buffer to network messages send queue.
   Use struct m0_net_end_point instead of endpoint index.
   @see m0_net_test_network_msg_send()
 */
int m0_net_test_network_msg_send_ep(struct m0_net_test_network_ctx *ctx,
				    uint32_t buf_ping_index,
				    struct m0_net_end_point *ep);

/**
   Add message to network messages receive queue.
   @see @ref m0_net_test_network_msg_send()
 */
int m0_net_test_network_msg_recv(struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_ping_index);

/**
   Add bulk buffer to bulk passive/active send/recv queue.
   @param ctx Net-test network context.
   @param buf_bulk_index Index of buffer in ctx->ntc_buf_bulk array.
   @param ep_index Entry point index in ctx->ntc_ep array. Makes sense
		   only for passive send/recv queue.
   @param q Queue type. Should be one of bulk queue types.
 */
int m0_net_test_network_bulk_enqueue(struct m0_net_test_network_ctx *ctx,
				     int32_t buf_bulk_index,
				     int32_t ep_index,
				     enum m0_net_queue_type q);
/**
   Remove network buffer from queue.
   @see m0_net_buffer_del()
   @param ctx Net-test network context.
   @param buf_index Index of buffer in ctx->ntc_buf_bulk or ntc->buf_ping
		    arrays, depending on buf_type parameter.
   @param buf_type Buffer type.
*/
void m0_net_test_network_buffer_dequeue(struct m0_net_test_network_ctx *ctx,
					enum m0_net_test_network_buf_type
					buf_type,
					int32_t buf_index);

/**
   Serialize or deserialize bulk buffer network transport descriptor
   to/from ping buffer.
   @see @ref net-test-fspec-usecases-bd
   @param op Serialization operation.
   @param ctx Net-test network context.
   @param buf_bulk_index Bulk buffer index. Buffer descriptor will be taken
			 from this buffer for serialization and set for this
			 buffer after deserialization.
   @param buf_ping_index Ping buffer index. This buffer will be used as
			 container to serialized buffer descriptors.
   @param offset Offset in the ping buffer to serialize/deserialize
		 network descriptor. Should have value 0 for the first
		 descriptor when serializing/deserializing.
   @return length of serialized/deserialized buffer descriptor.
   @pre op == M0_NET_TEST_SERIALIZE || op == M0_NET_TEST_DESERIALIZE
   @pre ctx != NULL
   @pre buf_bulk_index < ctx->ntc_cfg.ntncfg_buf_bulk_nr
   @pre buf_ping_index < ctx->ntc_cfg.ntncfg_buf_ping_nr
   @todo possible security vulnerability because bounds are not checked
 */
m0_bcount_t
m0_net_test_network_bd_serialize(enum m0_net_test_serialize_op op,
				 struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_bulk_index,
				 uint32_t buf_ping_index,
				 m0_bcount_t offset);

/**
   Get number of stored network buffer descriptors in ping buffer.
   @see @ref net-test-fspec-usecases-bd
   @see m0_net_test_network_bd_serialize()
   @pre ctx != NULL
   @pre buf_ping_index < ctx->ntc_cfg.ntncfg_buf_ping_nr
 */
size_t m0_net_test_network_bd_nr(struct m0_net_test_network_ctx *ctx,
				 uint32_t buf_ping_index);

/**
   Decrease number of network buffer descriptors in ping buffer.
   This function simply decreases number of network buffer descriptors
   stored in ping buffer.
   @see @ref net-test-fspec-usecases-bd
   @see m0_net_test_network_bd_serialize()
 */
void m0_net_test_network_bd_nr_dec(struct m0_net_test_network_ctx *ctx,
				   uint32_t buf_ping_index);

/**
   Accessor to buffers in net-test network context.
 */
struct m0_net_buffer *
m0_net_test_network_buf(struct m0_net_test_network_ctx *ctx,
			enum m0_net_test_network_buf_type buf_type,
			uint32_t buf_index);

/**
   Resize network buffer.
   Calls m0_net_buffer_deregister()/m0_net_buffer_register().
 */
int m0_net_test_network_buf_resize(struct m0_net_test_network_ctx *ctx,
				   enum m0_net_test_network_buf_type buf_type,
				   uint32_t buf_index,
				   m0_bcount_t new_size);

/**
   Fill entire buffer m0_bufvec with char ch.
   Useful for unit tests.
 */
void m0_net_test_network_buf_fill(struct m0_net_test_network_ctx *ctx,
				  enum m0_net_test_network_buf_type buf_type,
				  uint32_t buf_index,
				  uint8_t fill);

/** Accessor for endpoints by index. */
struct m0_net_end_point *
m0_net_test_network_ep(struct m0_net_test_network_ctx *ctx, size_t ep_index);

/**
   Search for ep_addr in m0_net_test_network_ctx.ntc_ep
   This function have time complexity
   of O(number of endpoints in the network context).
   @return >= 0 endpoint index
   @return -1 endpoint not found
 */
ssize_t m0_net_test_network_ep_search(struct m0_net_test_network_ctx *ctx,
				      const char *ep_addr);

/**
   Return m0_net_test_network_timeouts, filled with M0_TIME_NEVER.
   Useful because M0_TIME_NEVER is declared as "extern const".
 */
struct m0_net_test_network_timeouts m0_net_test_network_timeouts_never(void);

/**
   @} end of NetTestNetworkDFS group
 */

#endif /*  __MERO_NET_TEST_NETWORK_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
