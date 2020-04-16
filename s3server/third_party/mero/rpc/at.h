/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 16-May-2016
 */

#pragma once

#ifndef __MERO_RPC_AT_H__
#define __MERO_RPC_AT_H__

#include "lib/types.h"
#include "lib/buf.h"              /* m0_buf */
#include "lib/buf_xc.h"           /* m0_buf_xc */
#include "net/net_otw_types.h"    /* m0_net_buf_desc_data */
#include "net/net_otw_types_xc.h" /* m0_net_buf_desc_data */
#include "xcode/xcode_attr.h"
#include "lib/assert.h"           /* M0_BASSERT */


/**
 * @defgroup rpc-at
 *
 * RPC adaptive transmission (AT) is an interface to send/receive buffers as a
 * part of RPC item. AT implementation transfers the contents of the buffer to
 * the receiver either:
 * - inline, i.e., by including the contents of the buffer into the RPC item
 *   body, or
 * - inbulk, by constructing a network buffer representing the memory buffer,
 *   transmitting the network buffer descriptor in the RPC item and using bulk
 *   (RDMA) to transfer the contents of the buffer.
 * Implementation hides actual transfer method from RPC parties as much as
 * possible.
 *
 * Two roles are clearly distinguished:
 * - client, the originator of buffer transmission via RPC AT mechanism. Client
 *   can issue two types of requests: send buffer to server and request buffer
 *   from server.
 * - server, which handles client requests. Server-side AT interface is
 *   fom-oriented.
 *
 * Client to server buffer transmission
 * ------------------------------------
 *
 * Client is able to attach m0_buf buffer (exactly one) to an RPC AT buffer
 * (m0_rpc_at_buf). After that RPC AT buffer can be sent as part of RPC item.
 * Actual buffer transfer method depends on:
 * - buffer alignment. Only page-aligned buffers are suitable for inbulk.
 * - buffer size. Every RPC machine has a parameter that determines the cutoff
 *   size after which inbulk is used.
 *
 * Client code to initialise AT buffer:
 * @code
 * m0_rpc_at_init(ab);
 * rc = m0_rpc_at_add(ab, user_buf, rpc_mach);
 * if (rc == 0)
 *         // Send RPC item containing ab.
 *         m0_rpc_post_sync(fop, ...);
 * m0_rpc_at_fini(ab);
 * @endcode
 *
 * Note, that AT buffer finalisation should be done after reply from server is
 * received.
 *
 * Server code to load AT buffer received in RPC item:
 * @code
 * foo_tick(fom) {
 *         switch (fom_phase(fom)) {
 *         ...
 *         case PHASE_X:
 *                 ...
 *                 return m0_rpc_at_load(&fop->ab, fom, PHASE_Y);
 *         case PHASE_Y:
 *                 struct m0_buf buf;
 *
 *                 rc = m0_rpc_at_get(&fop->ab, &buf);
 *                 // Process buf.
 *                 ...
 *                 m0_rpc_at_fini(&fop->ab);
 *         }
 * }
 * @endcode
 *
 * Note, that initialisation of AT buffer is not required.
 *
 *
 * Buffer reception from server
 * ----------------------------
 *
 * Client is able to request buffer from server by sending AT buffer of special
 * type as part of RPC request. Identification of requested buffer on server
 * side is specific to RPC item type. Server sends requested buffer by embedding
 * AT buffer as part of reply RPC item.
 *
 * Client specifies in request the preferred way of buffer transmission: inline
 * or inbulk. For inbulk transmission client allocates incoming net buffer of
 * sufficient size and sends its descriptor in request. Client uses empty buffer
 * (of type M0_RPC_AT_EMPTY) to specify that inline transmission is preferred.
 *
 * Server chooses the way of buffer transmission using the following algorithm:
 * - If inline is requested and is possible (buffer is not too big), than inline
 *   is used.
 * - If inbulk is requested and received net buffer descriptor has sufficient
 *   size, then inbulk is used. AT buffer in reply contains status of RPC bulk
 *   transmission and requested buffer length.
 * - If inbulk is required, but client requested inline or inbulk with
 *   insufficient size, then AT buffer in server reply contains error code and
 *   size of the buffer. Client can use this information to re-request buffer
 *   using inbulk with sufficient size of net buffer.
 *
 * @todo Another method of buffer transmission may be needed: "best". When
 * client specifies this method it has to provide network buffer as with in-bulk
 * case. The server sends the reply inline if possible, uses the netbuffer
 * otherwise. This would allow return of large records (via in-bulk), while
 * avoiding in-bulk overhead for small records.
 *
 * Client code example:
 * @code
 * int buf_request(uint32_t len, bool force, struct m0_buf *out, bool *rep_bulk)
 * {
 *         // Initialise RPC item.
 *         ...
 *         m0_rpc_at_init(ab);
 *         rc = m0_rpc_at_recv(ab, rmach, len, force);
 *         if (rc == 0) {
 *                 rc = m0_rpc_post_sync(fop, ...);
 *                 if (rc == 0) {
 *                         // rep_ab is a pointer to AT buffer in RPC reply.
 *                         ...
 *                         rc = m0_rpc_at_rep_get(ab, rep_ab, out);
 *                         if (rc != 0)
 *                                 *rep_bulk = m0_rpc_at_rep_is_bulk(rep_ab,
 *                                                               &out.b_nob);
 *                 }
 *         }
 *         m0_rpc_at_fini(ab);
 *         // Finalise RPC item.
 *         ...
 *         return rc;
 * }
 *
 * rep_bulk = false;
 * // Assume client doesn't have any idea about buffer size.
 * rc = buf_request(M0_RPC_AT_UNKNOWN_LEN, false, &buf, &rep_bulk);
 * if (rc != 0 && rep_bulk) {
 *         // Re-request buffer, now forcing bulk.
 *         len = buf->b.nob;
 *         buf = M0_BUF_INIT0;
 *         rc = buf_request(len, true, &buf, &rep_bulk);
 * }
 * @endcode
 *
 * Server code example:
 * @code
 * foo_tick(fom) {
 *         switch (fom_phase(fom)) {
 *         ...
 *         case PHASE_X:
 *                 ...
 *                 m0_rpc_at_init(&fom->fo_rep_fop->rep_ab);
 *                 return m0_rpc_at_reply(&fom->fo_fop->ab,
 *                                        &fom->fo_rep_fop->rep_ab,
 *                                        buf, fom, PHASE_Y);
 *         case PHASE_Y:
 *                 // Check status. At this point requested buffer is
 *                 // inlined at reply FOP or sent via RPC bulk to client.
 *                 rc = m0_rpc_at_reply_rc(&fom->fo_rep_fop->rep_ab);
 *         }
 * }
 * @endcode
 *
 * Note, that server shouldn't finalise RPC AT buffer that is used as the reply
 * to the client request. The memory will be automatically de-allocated when
 * a corresponding reply FOP is finalised.
 *
 * Forcing RPC bulk method on client side is required in case if maximum RPC
 * item size or inbulk threshold are configured differently on server and
 * client. For example, client may have higher inbulk threshold and would prefer
 * to use inline reception method, but server will return an error because of
 * lower inbulk threshold.
 *
 * @todo: Perform negotiation on inbulk threshold during connection
 * establishment?
 *
 * References:
 * - RPC AT Requirements
 * https://docs.google.com/document/d/1rHXYdb0sVNA7HEiPJh7YGMiJHM9aw8NOTpKdtn_Cpe4/edit
 *
 * @{
 */

/* Import */
struct m0_fom;
struct m0_rpc_conn;
struct rpc_at_bulk;

enum m0_rpc_at_type {
	M0_RPC_AT_EMPTY     = 0,
	M0_RPC_AT_INLINE    = 1,
	M0_RPC_AT_BULK_SEND = 2,
	M0_RPC_AT_BULK_RECV = 3,
	M0_RPC_AT_BULK_REP  = 4,
	M0_RPC_AT_TYPE_NR
};

enum {
	M0_RPC_AT_UNKNOWN_LEN = 0,
};

/**
 * Sent by server as a reply to client requesting the buffer in case of inbulk.
 */
struct m0_rpc_at_bulk_rep {
	/**
	 * RPC bulk transfer result.
	 * -ENOMSG if inbulk is required, but client requested inline or
	 *  provided net buffer descriptor with insufficient length.
	 */
	int32_t  abr_rc;

	/** Length of the requested buffer. */
	uint64_t abr_len;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_at_extra {
	/* This field is not used, it's neccessary for proper alignment only. */
	struct m0_net_buf_desc_data  abr_desc;
	struct rpc_at_bulk          *abr_bulk;
	struct m0_buf                abr_user_buf;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_at_buf {
	/** Value from enum m0_rpc_at_type. */
	uint32_t ab_type;
	union {
		struct m0_buf               ab_buf
			M0_XCA_TAG("M0_RPC_AT_INLINE");

		struct m0_net_buf_desc_data ab_send
			M0_XCA_TAG("M0_RPC_AT_BULK_SEND");

		struct m0_net_buf_desc_data ab_recv
			M0_XCA_TAG("M0_RPC_AT_BULK_RECV");

		struct m0_rpc_at_bulk_rep   ab_rep
			M0_XCA_TAG("M0_RPC_AT_BULK_REP");

		/**
		 * That's a hack to store back reference to RPC bulk structure
		 * serving inbulk transmission of AT buffer. Don't use a
		 * separate field in order to avoid sending it over the network.
		 *
		 * Back reference presents in RAM due to union layout, but is
		 * not serialised by xcode.
		 */
		struct m0_rpc_at_extra      ab_extra
			M0_XCA_TAG("M0_RPC_AT_TYPE_NR");
	} u;
} M0_XCA_UNION M0_XCA_DOMAIN(rpc);

/* Checks that ab_extra is properly placed in union. */
M0_BASSERT(sizeof((struct m0_rpc_at_buf *) 0)->u ==
	   sizeof(struct m0_rpc_at_extra));
M0_BASSERT(offsetof(struct m0_rpc_at_buf, u.ab_extra.abr_bulk) >=
	   sizeof((struct m0_rpc_at_buf *) 0)->u.ab_buf);
M0_BASSERT(offsetof(struct m0_rpc_at_buf, u.ab_extra.abr_bulk) >=
	   sizeof((struct m0_rpc_at_buf *) 0)->u.ab_send);
M0_BASSERT(offsetof(struct m0_rpc_at_buf, u.ab_extra.abr_bulk) >=
	   sizeof((struct m0_rpc_at_buf *) 0)->u.ab_recv);
M0_BASSERT(offsetof(struct m0_rpc_at_buf, u.ab_extra.abr_bulk) >=
	   sizeof((struct m0_rpc_at_buf *) 0)->u.ab_rep);

/**
 * Initialises AT buffer.
 *
 * Should be called for AT buffer, that is not received from the network, e.g.
 * before m0_rpc_at_add(), m0_rpc_at_recv(), m0_rpc_at_reply() (for out buffer).
 */
M0_INTERNAL void m0_rpc_at_init(struct m0_rpc_at_buf *ab);

/**
 * Finalises AT buffer.
 *
 * Should be called before container RPC item finalisation, but after RPC item
 * is sent over network.
 */
M0_INTERNAL void m0_rpc_at_fini(struct m0_rpc_at_buf *ab);

/**
 * Called once for an AT buffer. If this returns success, then AT buffer
 * can be serialised as part of rpc item. On success, buffer content will be
 * deallocated in m0_rpc_at_fini(), unless m0_rpc_at_detach() is called.
 *
 * @pre conn != NULL
 */
M0_INTERNAL int m0_rpc_at_add(struct m0_rpc_at_buf     *ab,
			      const struct m0_buf      *buf,
			      const struct m0_rpc_conn *conn);

/**
 * Loads AT buffer contents.
 *
 * Fom unconditionally moves to 'next_phase'. Once AT buffer is fully loaded,
 * fom tick function is called.
 *
 * If the buffer is transmitted inline, then there is nothing to do and
 * M0_FSO_AGAIN is returned. Otherwise, asynchronous buffer loading through
 * RPC bulk is started and M0_FSO_WAIT is returned.
 *
 * Returns value from @ref m0_fom_phase_outcome.
 * @see m0_rpc_at_get()
 */
M0_INTERNAL int m0_rpc_at_load(struct m0_rpc_at_buf *ab, struct m0_fom *fom,
			       int next_phase);

/**
 * Initialises user-supplied buffer with AT buffer data.
 *
 * Should be called after m0_rpc_at_load() finished loading. Function doesn't
 * actually copy AT buffer data, but assigns 'buf' fields to point to received
 * data. The data is accessible until 'ab' is not finalised.
 *
 * Returns result of loading invoked by m0_rpc_at_load().
 * @see m0_rpc_at_load()
 */
M0_INTERNAL int m0_rpc_at_get(const struct m0_rpc_at_buf *ab,
			      struct m0_buf              *buf);


/**
 * Prepares AT buffer reception request.
 *
 * @param conn       RPC connection.
 * @param len        Expected length of the buffer, ignored for inline.
 * @param force_bulk Force inbulk transmission method.
 *
 * @pre conn != NULL
 */
M0_INTERNAL int m0_rpc_at_recv(struct m0_rpc_at_buf     *ab,
			       const struct m0_rpc_conn *conn,
			       uint32_t                  len,
			       bool                      force_bulk);

/**
 * Adds user-supplied buffer to AT buffer embedded in server reply.
 *
 * Fom unconditionally moves to 'next_phase'. Once buffer is processed (sent via
 * inbulk or embedded in reply FOP), fom tick function is called.
 *
 * Returns value from @ref m0_fom_phase_outcome.
 * @see m0_rpc_at_reply_rc()
 */
M0_INTERNAL int m0_rpc_at_reply(struct m0_rpc_at_buf *in,
				struct m0_rpc_at_buf *out,
				struct m0_buf        *repbuf,
				struct m0_fom        *fom,
				int                   next_phase);

/**
 * Returns execution result of m0_rpc_at_reply().
 *
 * @see m0_rpc_at_reply()
 */
M0_INTERNAL int m0_rpc_at_reply_rc(struct m0_rpc_at_buf *out);

/**
 * Initialises user-supplied buffer with AT buffer data received as reply.
 *
 * Intended to be called on client side.
 *
 * @see m0_rpc_at_rep_is_bulk
 */
M0_INTERNAL int m0_rpc_at_rep_get(struct m0_rpc_at_buf *sent,
				  struct m0_rpc_at_buf *rcvd,
				  struct m0_buf        *out);

/**
 * Transforms received AT buffer to inline type.
 *
 * Regardless of actual transmission method used, modify received buffer as it
 * was received via inline transmission method. Buffer referred by a 'sent'
 * argument can be safely finalised afterwards.
 */
M0_INTERNAL int m0_rpc_at_rep2inline(struct m0_rpc_at_buf *sent,
				     struct m0_rpc_at_buf *rcvd);

/**
 * Checks whether inbulk was used by server to send AT buffer.
 *
 * Allows client to discover actual buffer length, stored on server.
 * Also, return value 'true' indicates that server requested inbulk method for
 * buffer transmission.
 */
M0_INTERNAL bool m0_rpc_at_rep_is_bulk(const struct m0_rpc_at_buf *rcvd,
				       uint64_t                   *len);

/**
 * Checks whether AT buffer has some associated data.
 *
 * AT buffer is considered to be empty if:
 * - Buffer is initialised, but no data is attached (has type M0_RPC_AT_EMPTY).
 * - Buffer is intented for buffer reception.
 * - Buffer has type M0_RPC_AT_BULK_REP, but no bytes were transmitted via bulk.
 */
M0_INTERNAL bool m0_rpc_at_is_set(const struct m0_rpc_at_buf *ab);

/**
 * Detaches internal data buffer from AT buffer.
 *
 * Transfers data buffer ownership to the user, so user is responsible for its
 * deallocation. For received AT buffer internal data buffer can be obtained via
 * m0_rpc_at_rep_get(). For sent AT buffer it's the buffer provided in
 * m0_rpc_at_add().
 */
M0_INTERNAL void m0_rpc_at_detach(struct m0_rpc_at_buf *ab);

/**
 * Returns length of data attached to buffer.
 *
 * Returned value interpretation depends on buffer type:
 * - M0_RPC_AT_INLINE, M0_RPC_AT_BULK_SEND: length of the attached buffer;
 * - M0_RPC_AT_BULK_RECV: total size of RPC bulk buffers reserved by client;
 * - M0_RPC_AT_BULK_REP: size of buffer set by service regardless of rc.
 */
M0_INTERNAL uint64_t m0_rpc_at_len(const struct m0_rpc_at_buf *ab);

/** @} end of rpc-at group */

#endif /* __MERO_RPC_AT_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
