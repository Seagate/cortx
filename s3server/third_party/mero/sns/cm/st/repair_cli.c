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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 *                  Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 10/31/2012
 */

#include "lib/assert.h"
#include "lib/memory.h"
#include "fop/fop.h"
#include "net/lnet/lnet.h"
#include "rpc/rpc.h"
#include "rpc/rpclib.h"
#include "repair_cli.h"

static struct m0_fid            process_fid = M0_FID_TINIT('r', 0, 1);
static struct m0_net_domain     cl_ndom;
static struct m0_rpc_client_ctx cl_ctx = {
	.rcx_net_dom            = &cl_ndom,
	.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
	.rcx_fid                = &process_fid,
};

const char *cl_ep_addr;
const char *srv_ep_addr[MAX_SERVERS];

M0_INTERNAL int repair_client_init(void)
{
	int rc;

	rc = m0_net_domain_init(&cl_ndom, &m0_net_lnet_xprt);
	if (rc == 0) {
		cl_ctx.rcx_local_addr  = cl_ep_addr;
		cl_ctx.rcx_remote_addr = srv_ep_addr[0];

		rc = m0_rpc_client_start(&cl_ctx);
	}
	return rc;
}

M0_INTERNAL void repair_client_fini(void)
{
	m0_rpc_client_stop(&cl_ctx);

	m0_net_domain_fini(&cl_ndom);
}

M0_INTERNAL int repair_rpc_ctx_init(struct rpc_ctx *ctx, const char *sep)
{
	return m0_rpc_client_connect(&ctx->ctx_conn,
				     &ctx->ctx_session,
				     &cl_ctx.rcx_rpc_machine, sep,
				     NULL, MAX_RPCS_IN_FLIGHT,
			     m0_time_from_now(M0_RPCLIB_UTIL_CONN_TIMEOUT, 0));
}

M0_INTERNAL void repair_rpc_ctx_fini(struct rpc_ctx *ctx)
{
	if (ctx->ctx_rc != 0)
		return;
	m0_rpc_session_destroy(&ctx->ctx_session, M0_TIME_NEVER);
	m0_rpc_conn_destroy(&ctx->ctx_conn, M0_TIME_NEVER);
}

M0_INTERNAL int repair_rpc_post(struct m0_fop *fop,
				struct m0_rpc_session *session,
				const struct m0_rpc_item_ops *ri_ops,
				m0_time_t  deadline)
{
	struct m0_rpc_item *item;

	M0_PRE(fop != NULL);
	M0_PRE(session != NULL);

	item              = &fop->f_item;
	item->ri_ops      = ri_ops;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = deadline;
	item->ri_resend_interval = M0_TIME_NEVER;

	return m0_rpc_post(item);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
