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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 09/28/2011
 */

#pragma once

#ifndef __MERO_RPC_RPCLIB_H__
#define __MERO_RPC_RPCLIB_H__

#ifndef __KERNEL__
#  include <stdio.h>          /* FILE */
#  include "mero/setup.h"     /* m0_mero */
#endif
#include "reqh/reqh.h"
#include "cob/cob.h"          /* m0_cob_domain */
#include "net/net.h"          /* m0_net_end_point */
#include "net/buffer_pool.h"
#include "rpc/rpc.h"

struct m0_fop;
struct m0_net_xprt;
struct m0_net_domain;

enum {
	/**
	 * Maximum number of retries (m0_rpc_item::ri_nr_sent_max)
	 * for m0_rpc_post_sync() and m0_rpc_post_with_timeout_sync().
	 */
	M0_RPCLIB_MAX_RETRIES = 60,

	/**
	 * Timeout in seconds that is used by standalone utilities, such as
	 * m0console, m0repair, to connect to a service.
	 */
	M0_RPCLIB_UTIL_CONN_TIMEOUT = 20, /* seconds */
};

#ifndef __KERNEL__

struct m0_reqh;
struct m0_reqh_service_type;

/**
 * RPC server context structure.
 *
 * Contains all required data to initialize an RPC server,
 * using mero-setup API.
 */
struct m0_rpc_server_ctx {
	/** a pointer to array of transports, which can be used by server */
	struct m0_net_xprt          **rsx_xprts;
	/** number of transports in array */
	int                           rsx_xprts_nr;

	/**
	 * ARGV-like array of CLI options to configure mero-setup, which is
	 * passed to m0_cs_setup_env()
	 */
	char                        **rsx_argv;
	/** number of elements in rsx_argv array */
	int                           rsx_argc;

	const char                   *rsx_log_file_name;

	/** an embedded mero context structure */
	struct m0_mero                rsx_mero_ctx;

	/**
	 * this is an internal variable, which is used by m0_rpc_server_stop()
	 * to close log file; it should not be initialized by a caller
	 */
	FILE                         *rsx_log_file;
};

/**
  Starts server's rpc machine.

  @param sctx  Initialized rpc context structure.
*/
int m0_rpc_server_start(struct m0_rpc_server_ctx *sctx);

/**
  Stops RPC server.

  @param sctx  Initialized rpc context structure.
*/
void m0_rpc_server_stop(struct m0_rpc_server_ctx *sctx);

M0_INTERNAL struct m0_rpc_machine *
m0_rpc_server_ctx_get_rmachine(struct m0_rpc_server_ctx *sctx);

#endif /* !__KERNEL__ */

/**
 * RPC client context structure.
 *
 * Contains all required data to initialize an RPC client and connect to server.
 */
struct m0_rpc_client_ctx {
	/*
	 * Input parameters.
	 *
	 * They are initialised and filled by a caller of m0_rpc_client_start().
	 */

	/**
	 * A pointer to net domain struct which will be initialized and used by
	 * m0_rpc_client_start()
	 */
	struct m0_net_domain      *rcx_net_dom;

	/** Transport specific local address (client's address) */
	const char                *rcx_local_addr;

	/** Transport specific remote address (server's address) */
	const char                *rcx_remote_addr;

	uint64_t		   rcx_max_rpcs_in_flight;

	/* -------------------------------------------------------------
	 * Output parameters.
	 *
	 * They are initialised and filled by m0_rpc_client_start().
	 */

	struct m0_reqh             rcx_reqh;
	struct m0_rpc_machine	   rcx_rpc_machine;
	struct m0_rpc_conn	   rcx_connection;
	struct m0_rpc_session	   rcx_session;

	/** Buffer pool used to provision TM receive queue. */
	struct m0_net_buffer_pool  rcx_buffer_pool;

	/** Minimum number of buffers in TM receive queue. */
        uint32_t		   rcx_recv_queue_min_length;

	/** Maximum RPC recive buffer size. */
        uint32_t		   rcx_max_rpc_msg_size;

	/** timeout value to establish the client-server connection */
        m0_time_t		   rcx_abs_timeout;
	/** Process FID */
	struct m0_fid             *rcx_fid;
};

/**
 * Establishes RPC connection and creates a session.
 *
 * Connection automatically handles HA notifications regarding state of service
 * identified by service object, if provided. In case of service death being
 * announced, all rpc items on the connection get cancelled letting connection
 * close safe.
 *
 * @param[out] conn
 * @param[out] session
 * @param[in]  rpc_mach
 * @param[in]  remote_addr
 * @param[in]  service object, optional, can be NULL
 * @param[in]  max_rpcs_in_flight
 * @param[in]  abs_timeout
 */
M0_INTERNAL int m0_rpc_client_connect(struct m0_rpc_conn    *conn,
				      struct m0_rpc_session *session,
				      struct m0_rpc_machine *rpc_mach,
				      const char            *remote_addr,
				      struct m0_fid         *svc_fid,
				      uint64_t               max_rpcs_in_flight,
				      m0_time_t              abs_timeout);

/**
 * A bit more intelligent version of m0_rpc_client_connect(). To be sure client
 * connects to right service, client side provides service fid (optional, can be
 * NULL) and service type along with the remote address. The call internally
 * makes sure the provided address belongs to service of correct fid and type.
 *
 * However even with service object not found, m0_rpc_client_connect() attempt
 * is ultimately done anyway.
 *
 * @note confc contained by REQH, i.e. accessible by m0_reqh2confc(reqh), is
 * used for service object look-up.
 */
M0_INTERNAL int
m0_rpc_client_find_connect(struct m0_rpc_conn       *conn,
			   struct m0_rpc_session    *session,
			   struct m0_rpc_machine    *rpc_mach,
			   const char               *remote_addr,
			   enum m0_conf_service_type stype,
			   uint64_t                  max_rpcs_in_flight,
			   m0_time_t                 abs_timeout);

/**
 * Starts client's rpc machine.
 *
 * Creates connection to server and establishes an rpc session on top
 * of it.  Created session object can be set in an rpc item and used
 * in m0_rpc_post().
 *
 * @param cctx  Initialised rpc context structure.
 */
int m0_rpc_client_start(struct m0_rpc_client_ctx *cctx);

/**
 * Terminates RPC session and connection with server and finalises
 * client's RPC machine.
 *
 * @param cctx  Initialised rpc context structure.
 */
int m0_rpc_client_stop(struct m0_rpc_client_ctx *cctx);

int m0_rpc_client_stop_stats(struct m0_rpc_client_ctx *cctx,
			     void (*printout)(struct m0_rpc_machine *));

/**
 * Sends a fop (an RPC item, to be precise) and waits for reply.
 *
 * By default, fop is resent after every second until reply is received.
 * To change this behaviour set fop->f_item.ri_nr_sent_max and
 * fop->f_item.ri_resend_interval. They are, maximum number of times
 * fop is sent before failing and interval after which the fop is resent,
 * respectively. Their default values are M0_RPCLIB_MAX_RETRIES and
 * m0_time(1, 0).
 *
 * To simply timeout fop after N seconds set fop->f_item.ri_nr_sent_max
 * to N before calling m0_rpc_post_sync().
 *
 * @param fop       Fop to send.  Presumably, fop->f_item.ri_reply will hold
 *                  the reply upon successful return.
 * @param session   The session to be used for the client call.
 * @param ri_ops    Pointer to RPC item ops structure.
 * @param deadline  Absolute time after which formation should send the
 *		    fop as soon as possible. deadline should be 0 if
 *		    fop shouldn't wait in formation queue and should
 *		    be sent immediately.
 */
int m0_rpc_post_sync(struct m0_fop                *fop,
		     struct m0_rpc_session        *session,
		     const struct m0_rpc_item_ops *ri_ops,
		     m0_time_t                     deadline);

int m0_rpc_post_with_timeout_sync(struct m0_fop                *fop,
				  struct m0_rpc_session        *session,
				  const struct m0_rpc_item_ops *ri_ops,
				  m0_time_t                     deadline,
				  m0_time_t                     timeout);

#endif /* __MERO_RPC_RPCLIB_H__ */
