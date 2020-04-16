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


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/ut/helper.h"
#include "ut/ut.h"

#include "lib/types.h"          /* uint32_t */
#include "fid/fid.h"            /* m0_fid */
#include "net/lnet/lnet.h"      /* m0_net_lnet_xprt */
#include "rpc/rpclib.h"         /* m0_rpc_client_connect */


M0_INTERNAL void m0_ha_ut_rpc_ctx_init(struct m0_ha_ut_rpc_ctx *ctx)
{
	struct m0_fid  process_fid = M0_FID_TINIT('r', 0, 1);
	const uint32_t tms_nr      = 1;
	const uint32_t bufs_nr     =
		m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, tms_nr);
	const char    *ep          = "0@lo:12345:42:100";
	int            rc;

	rc = m0_net_domain_init(&ctx->hurc_net_domain, &m0_net_lnet_xprt);
	M0_ASSERT(rc == 0);
	rc = m0_rpc_net_buffer_pool_setup(&ctx->hurc_net_domain,
					  &ctx->hurc_buffer_pool,
					  bufs_nr, tms_nr);
	M0_ASSERT(rc == 0);
	rc = M0_REQH_INIT(&ctx->hurc_reqh,
			  .rhia_dtm          = (void*)1,
			  .rhia_mdstore      = (void*)1,
			  .rhia_fid          = &process_fid);
	M0_ASSERT(rc == 0);
	m0_reqh_start(&ctx->hurc_reqh);
	rc = m0_rpc_machine_init(&ctx->hurc_rpc_machine,
				 &ctx->hurc_net_domain, ep,
				 &ctx->hurc_reqh,
				 &ctx->hurc_buffer_pool,
				 M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ha_ut_rpc_ctx_fini(struct m0_ha_ut_rpc_ctx *ctx)
{
	m0_reqh_shutdown_wait(&ctx->hurc_reqh);
	m0_rpc_machine_fini(&ctx->hurc_rpc_machine);
	m0_reqh_services_terminate(&ctx->hurc_reqh);
	m0_reqh_fini(&ctx->hurc_reqh);
	m0_rpc_net_buffer_pool_cleanup(&ctx->hurc_buffer_pool);
	m0_net_domain_fini(&ctx->hurc_net_domain);
}

M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_init(struct m0_ha_ut_rpc_session_ctx *sctx,
                              struct m0_ha_ut_rpc_ctx         *ctx)
{
	int rc;

	rc = m0_rpc_client_connect(&sctx->husc_conn, &sctx->husc_session,
	                           &ctx->hurc_rpc_machine,
	                           m0_rpc_machine_ep(&ctx->hurc_rpc_machine),
				   NULL, M0_HA_UT_MAX_RPCS_IN_FLIGHT,
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
}

M0_INTERNAL void
m0_ha_ut_rpc_session_ctx_fini(struct m0_ha_ut_rpc_session_ctx *sctx)
{
	int rc;

	rc = m0_rpc_session_destroy(&sctx->husc_session, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_conn_destroy(&sctx->husc_conn, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
