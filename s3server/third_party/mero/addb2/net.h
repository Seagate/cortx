/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 07-Feb-2015
 */

#pragma once

#ifndef __MERO_ADDB2_NET_H__
#define __MERO_ADDB2_NET_H__

/**
 * @defgroup addb2
 *
 * Addb2 network component
 * -----------------------
 *
 * Addb2 network engine (m0_addb2_net) accepts abb2 trace buffers,
 * m0_addb2_trace (wrapped in trace buffer objects m0_addb2_trace_obj) for
 * transmission over network to a remote addb2 service.
 *
 * Traces are opportunistically piggy-backed to the outgoing rpc packets.
 *
 * On receiving a trace, addb2 service submits the trace for storage, along with
 * other traces produced normally on the remote node.
 *
 * @{
 */
/* import */
struct m0_rpc_conn;
struct m0_addb2_trace_obj;

/* export */
struct m0_addb2_net;

/**
 * Allocates and initialises a network engine.
 */
M0_INTERNAL struct m0_addb2_net *m0_addb2_net_init(void);
M0_INTERNAL void m0_addb2_net_fini  (struct m0_addb2_net *net);

/**
 * Adds the connection to the engine.
 *
 * Pending traces, submitted to the engine, will be piggy-backed to the packets
 * outgoing through this connection, when the packets have enough free space.
 *
 * Typically, this function will be called when configuration in confc is
 * scanned during initialisation or after configuration change.
 *
 * @see m0_addb2_net_del().
 */
M0_INTERNAL int  m0_addb2_net_add   (struct m0_addb2_net *net,
				     struct m0_rpc_conn *conn);
/**
 * Deletes the connection.
 *
 * @see m0_addb2_net_add().
 */
M0_INTERNAL void m0_addb2_net_del   (struct m0_addb2_net *net,
				     struct m0_rpc_conn *conn);
/**
 * Submits a trace for network.
 *
 * The trace will be opportunistically send over any connection added to the
 * network machine.
 */
M0_INTERNAL int  m0_addb2_net_submit(struct m0_addb2_net *net,
				     struct m0_addb2_trace_obj *obj);
/**
 * If necessary, triggers an attempt to send some pending traces.
 *
 * This function should be called periodically to guarantee that traces are sent
 * out even if there is no other outgoing message to which the traces can be
 * piggy-backed.
 *
 * Caller must guarantee that connections are neither added nor deleted, while
 * this call is in progress.
 */
M0_INTERNAL void m0_addb2_net_tick  (struct m0_addb2_net *net);

/**
 * Initiates the network machine stopping.
 *
 * When there are no more pending traces, the provided call-back is invoked.
 */
M0_INTERNAL void m0_addb2_net_stop  (struct m0_addb2_net *net,
				     void (*callback)(struct m0_addb2_net *,
						      void *),
				     void *datum);
M0_INTERNAL int  m0_addb2_net_module_init(void);
M0_INTERNAL void m0_addb2_net_module_fini(void);

/** @} end of addb2 group */
#endif /* __MERO_ADDB2_NET_H__ */

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
