/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 01-Dec-2012
 */
#pragma once
#ifndef __MERO_CONF_UT_RPC_HELPERS_H__
#define __MERO_CONF_UT_RPC_HELPERS_H__

struct m0_net_xprt;
struct m0_rpc_machine;

/** Initializes and start reqh service of passed type. */
M0_INTERNAL int m0_ut_rpc_service_start(struct m0_reqh_service **service,
				const struct m0_reqh_service_type *type);

/** Initialises net and rpc layers, performs m0_rpc_machine_init(). */
M0_INTERNAL int m0_ut_rpc_machine_start(struct m0_rpc_machine *mach,
					struct m0_net_xprt *xprt,
					const char *ep_addr);

/** Performs m0_rpc_machine_fini(), finalises rpc and net layers. */
M0_INTERNAL void m0_ut_rpc_machine_stop(struct m0_rpc_machine *mach);

#endif /* __MERO_CONF_UT_RPC_HELPERS_H__ */
