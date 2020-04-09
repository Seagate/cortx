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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 02/24/2015
 */
#pragma once

#ifndef __MERO_SPIEL_UT_SPIEL_UT_COMMON_H__
#define __MERO_SPIEL_UT_SPIEL_UT_COMMON_H__

#include "net/net.h"          /* m0_net_domain */
#include "net/buffer_pool.h"  /* m0_net_buffer_pool */
#include "reqh/reqh.h"        /* m0_reqh */
#include "rpc/rpc_machine.h"  /* m0_rpc_machine */
#include "rpc/rpclib.h"       /* m0_rpc_server_ctx */
#include "rm/rm_service.h"    /* m0_rms_type */

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:*"

struct m0_spiel;

extern const char *confd_addr[];
extern const char *rm_addr;

/**
 * Request handler context with all necessary structures.
 *
 * Field sur_reqh can be passed to m0_spiel_start() function
 */
struct m0_spiel_ut_reqh {
	struct m0_net_domain      sur_net_dom;
	struct m0_net_buffer_pool sur_buf_pool;
	struct m0_reqh            sur_reqh;
	struct m0_rpc_machine     sur_rmachine;
	struct m0_rpc_server_ctx  sur_confd_srv;
};

M0_INTERNAL int m0_spiel__ut_reqh_init(struct m0_spiel_ut_reqh *spl_reqh,
				       const char              *ep_addr);

M0_INTERNAL void m0_spiel__ut_reqh_fini(struct m0_spiel_ut_reqh *spl_reqh);

M0_INTERNAL int m0_spiel__ut_rpc_server_start(struct m0_rpc_server_ctx *rpc_srv,
					const char               *confd_ep,
					const char               *confdb_path);

M0_INTERNAL void m0_spiel__ut_rpc_server_stop(
					struct m0_rpc_server_ctx *rpc_srv);

M0_INTERNAL void m0_spiel__ut_init(struct m0_spiel *spiel,
				   const char      *confd_path,
				   bool             cmd_iface);

M0_INTERNAL void m0_spiel__ut_fini(struct m0_spiel *spiel,
				   bool             cmd_iface);

#endif /* __MERO_SPIEL_UT_SPIEL_UT_COMMON_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
