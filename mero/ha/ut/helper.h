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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 4-May-2016
 */

#pragma once

#ifndef __MERO_HA_UT_HELPER_H__
#define __MERO_HA_UT_HELPER_H__

/**
 * @defgroup ha
 *
 * @{
 */

#include "net/net.h"            /* m0_net_domain */
#include "net/buffer_pool.h"    /* m0_net_buffer_pool */
#include "reqh/reqh.h"          /* m0_reqh */
#include "rpc/rpc.h"            /* m0_rpc_machine_init */
#include "rpc/conn.h"           /* m0_rpc_conn */
#include "rpc/session.h"        /* m0_rpc_session */

enum {
	M0_HA_UT_MAX_RPCS_IN_FLIGHT = 2,
};

struct m0_ha_ut_rpc_ctx {
	struct m0_net_domain      hurc_net_domain;
	struct m0_net_buffer_pool hurc_buffer_pool;
	struct m0_reqh            hurc_reqh;
	struct m0_rpc_machine     hurc_rpc_machine;
};

struct m0_ha_ut_rpc_session_ctx {
	struct m0_rpc_conn    husc_conn;
	struct m0_rpc_session husc_session;
};

M0_INTERNAL void m0_ha_ut_rpc_ctx_init(struct m0_ha_ut_rpc_ctx *ctx);
M0_INTERNAL void m0_ha_ut_rpc_ctx_fini(struct m0_ha_ut_rpc_ctx *ctx);

M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_init(struct m0_ha_ut_rpc_session_ctx *sctx,
                              struct m0_ha_ut_rpc_ctx         *ctx);
M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_fini(struct m0_ha_ut_rpc_session_ctx *sctx);

/** @} end of ha group */
#endif /* __MERO_HA_UT_HELPER_H__ */

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
