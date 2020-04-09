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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 14-Apr-2015
 */

#include "ut/ut.h"
#include "lib/finject.h"
#include "lib/memory.h"
#include "lib/trace.h"
#include "reqh/reqh.h"
#include "rpc/session.h"
#include "rpc/rpc_opcodes.h"
#include "net/lnet/lnet.h"
#include "rpc/link.h"
#include "rpc/rpc.h"
#include "fop/fop.h"
#include "sss/ss_fops.h"
#include "ha/ut/helper.h"

#include "rpc/ut/clnt_srv_ctx.c"   /* sctx, cctx. NOTE: This is .c file */

enum {
	RLUT_MAX_RPCS_IN_FLIGHT = 10,
	RLUT_CONN_TIMEOUT       = 4, /* seconds */
	RLUT_SESS_TIMEOUT       = 2, /* seconds */
};

struct m0_clink clink;

static void rlut_init(struct m0_net_domain      *net_dom,
		      struct m0_net_buffer_pool *buf_pool,
		      struct m0_reqh            *reqh,
		      struct m0_rpc_machine     *rmachine)
{
	const char               *client_ep = "0@lo:12345:34:1";
	/* unreacheable remote EP */
	struct m0_net_xprt       *xprt = &m0_net_lnet_xprt;
	int                       rc;

	enum {
		NR_TMS = 1,
	};

	M0_SET0(net_dom);
	M0_SET0(buf_pool);
	M0_SET0(reqh);
	M0_SET0(rmachine);

	rc = m0_net_domain_init(net_dom, xprt);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_net_buffer_pool_setup(net_dom,
					  buf_pool,
					  m0_rpc_bufs_nr(
					     M0_NET_TM_RECV_QUEUE_DEF_LEN,
					     NR_TMS),
					  NR_TMS);
	M0_UT_ASSERT(rc == 0);

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_start(reqh);

	rc = m0_rpc_machine_init(rmachine, net_dom, client_ep, reqh, buf_pool,
				 M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_UT_ASSERT(rc == 0);
}

static void rlut_fini(struct m0_net_domain      *net_dom,
		      struct m0_net_buffer_pool *buf_pool,
		      struct m0_reqh            *reqh,
		      struct m0_rpc_machine     *rmachine)
{
	m0_reqh_services_terminate(reqh);
	m0_rpc_machine_fini(rmachine);
	m0_reqh_fini(reqh);
	m0_rpc_net_buffer_pool_cleanup(buf_pool);
	m0_net_domain_fini(net_dom);
}

static void rlut_remote_unreachable(void)
{
	struct m0_net_domain      net_dom;
	struct m0_net_buffer_pool buf_pool;
	struct m0_reqh            reqh;
	struct m0_rpc_machine     rmachine;
	struct m0_rpc_link       *rlink;
	/* unreacheable remote EP */
	const char               *remote_ep = "0@lo:12345:35:1";
	int                       rc;

	rlut_init(&net_dom, &buf_pool, &reqh, &rmachine);

	/* RPC link structure is too big to be allocated on stack */
	M0_ALLOC_PTR(rlink);
	M0_UT_ASSERT(rlink != NULL);

	rc = m0_rpc_link_init(rlink, &rmachine, NULL, remote_ep,
			      MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_CONN_TIMEOUT, 0));
	M0_UT_ASSERT(rc != 0 && !m0_rpc_link_is_connected(rlink));
	m0_rpc_link_fini(rlink);
	m0_free(rlink);

	rlut_fini(&net_dom, &buf_pool, &reqh, &rmachine);
}

static void rlut_reconnect(void)
{
	struct m0_ha_ut_rpc_ctx *rpc_ctx;
	struct m0_rpc_machine   *mach;
	struct m0_rpc_link      *rlink;
	const char              *remote_ep;
	int                      rc;
	int                      i;

	M0_ALLOC_PTR(rpc_ctx);
	M0_UT_ASSERT(rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(rpc_ctx);
	mach = &rpc_ctx->hurc_rpc_machine;
	remote_ep = m0_rpc_machine_ep(mach);

	M0_ALLOC_PTR(rlink);
	M0_UT_ASSERT(rlink != NULL);
	rc = m0_rpc_link_init(rlink, mach, NULL, remote_ep,
			      RLUT_MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);

	/* Reconnect after disconnect */
	for (i = 0; i < 2; ++i) {
		rc = m0_rpc_link_connect_sync(rlink,
					m0_time_from_now(RLUT_CONN_TIMEOUT, 0));
		M0_UT_ASSERT(rc == 0 && m0_rpc_link_is_connected(rlink));
		rc = m0_rpc_link_disconnect_sync(rlink,
					m0_time_from_now(RLUT_CONN_TIMEOUT, 0));
		M0_UT_ASSERT(rc == 0 && !m0_rpc_link_is_connected(rlink));
		m0_rpc_link_reset(rlink);
	}

	/* Reconnect after fail */
	m0_fi_enable_once("m0_rpc_conn_establish", "fake_error");
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_CONN_TIMEOUT, 0));
	M0_UT_ASSERT(rc != 0 && !m0_rpc_link_is_connected(rlink));
	m0_rpc_link_reset(rlink);
	m0_fi_disable("m0_rpc_conn_establish", "fake_error");
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_CONN_TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0 && m0_rpc_link_is_connected(rlink));
	rc = m0_rpc_link_disconnect_sync(rlink,
					m0_time_from_now(RLUT_CONN_TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0 && !m0_rpc_link_is_connected(rlink));

	m0_rpc_link_fini(rlink);
	m0_free(rlink);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
}

extern uint32_t m0_rpc__filter_opcode[4];

static void rlut_remote_delay(void)
{
	struct m0_ha_ut_rpc_ctx *rpc_ctx;
	struct m0_rpc_machine   *mach;
	struct m0_rpc_link      *rlink;
	const char              *remote_ep;
	int                      rc;

	M0_ALLOC_PTR(rpc_ctx);
	M0_UT_ASSERT(rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(rpc_ctx);
	mach = &rpc_ctx->hurc_rpc_machine;
	remote_ep = m0_rpc_machine_ep(mach);

	M0_ALLOC_PTR(rlink);
	M0_UT_ASSERT(rlink != NULL);
	/* delay on session establishing */
	rc = m0_rpc_link_init(rlink, mach, NULL, remote_ep,
			      RLUT_MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable("item_received_fi", "drop_opcode");
	m0_rpc__filter_opcode[0] = M0_RPC_SESSION_ESTABLISH_REP_OPCODE;
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc != 0 && !m0_rpc_link_is_connected(rlink));
	m0_rpc__filter_opcode[0] = 0;
	m0_fi_disable("item_received_fi", "drop_opcode");
	m0_rpc_link_fini(rlink);
	/* delay on session establishing and connection termination */
	M0_SET0(rlink);
	m0_fi_enable("item_received_fi", "drop_opcode");
	m0_rpc__filter_opcode[0] = M0_RPC_SESSION_ESTABLISH_REP_OPCODE;
	m0_rpc__filter_opcode[1] = M0_RPC_CONN_TERMINATE_REP_OPCODE;
	rc = m0_rpc_link_init(rlink, mach, NULL, remote_ep,
			      RLUT_MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc != 0 && !m0_rpc_link_is_connected(rlink));
	m0_rpc__filter_opcode[0] = 0;
	m0_rpc__filter_opcode[1] = 0;
	m0_fi_disable("item_received_fi", "drop_opcode");
	m0_rpc_link_fini(rlink);
	/* delay on session termination */
	M0_SET0(rlink);
	m0_fi_enable("item_received_fi", "drop_opcode");
	m0_rpc__filter_opcode[0] = M0_RPC_SESSION_TERMINATE_REP_OPCODE;
	rc = m0_rpc_link_init(rlink, mach, NULL, remote_ep,
			      RLUT_MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0 && m0_rpc_link_is_connected(rlink));
	rc = m0_rpc_link_disconnect_sync(rlink,
					m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc == -ETIMEDOUT && !m0_rpc_link_is_connected(rlink));
	m0_rpc__filter_opcode[0] = 0;
	m0_fi_disable("item_received_fi", "drop_opcode");
	m0_rpc_link_fini(rlink);
	/* delay on connection termination */
	M0_SET0(rlink);
	rc = m0_rpc_link_init(rlink, mach, NULL, remote_ep,
			      RLUT_MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0 && m0_rpc_link_is_connected(rlink));
	m0_fi_enable("item_received_fi", "drop_opcode");
	m0_rpc__filter_opcode[0] = M0_RPC_CONN_TERMINATE_REP_OPCODE;
	rc = m0_rpc_link_disconnect_sync(rlink,
					m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc == -ETIMEDOUT && !m0_rpc_link_is_connected(rlink));
	m0_rpc__filter_opcode[0] = 0;
	m0_fi_disable("item_received_fi", "drop_opcode");
	m0_rpc_link_fini(rlink);

	m0_free(rlink);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
}

static void rlut_reset(void)
{
	struct m0_ha_ut_rpc_ctx *rpc_ctx;
	struct m0_rpc_machine   *mach;
	struct m0_rpc_link      *rlink;
	const char              *remote_ep;
	int                      rc;

	M0_ALLOC_PTR(rpc_ctx);
	M0_UT_ASSERT(rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(rpc_ctx);
	mach = &rpc_ctx->hurc_rpc_machine;
	remote_ep = m0_rpc_machine_ep(mach);

	M0_ALLOC_PTR(rlink);
	M0_UT_ASSERT(rlink != NULL);
	rc = m0_rpc_link_init(rlink, mach, NULL, remote_ep,
			      RLUT_MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);

	/* delay on session establishing */
	m0_fi_enable("item_received_fi", "drop_opcode");
	m0_rpc__filter_opcode[0] = M0_RPC_SESSION_ESTABLISH_REP_OPCODE;
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc != 0 && !m0_rpc_link_is_connected(rlink));
	m0_rpc__filter_opcode[0] = 0;
	m0_fi_disable("item_received_fi", "drop_opcode");

	/* normal connect */
	m0_rpc_link_reset(rlink);
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0 && m0_rpc_link_is_connected(rlink));

	/* delay on session termination */
	m0_fi_enable("item_received_fi", "drop_opcode");
	m0_rpc__filter_opcode[0] = M0_RPC_SESSION_TERMINATE_REP_OPCODE;
	rc = m0_rpc_link_disconnect_sync(rlink,
					m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc != 0 && !m0_rpc_link_is_connected(rlink));
	m0_rpc__filter_opcode[0] = 0;
	m0_fi_disable("item_received_fi", "drop_opcode");

	/* normal connect/disconnect */
	m0_rpc_link_reset(rlink);
	rc = m0_rpc_link_connect_sync(rlink,
				      m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0 && m0_rpc_link_is_connected(rlink));
	rc = m0_rpc_link_disconnect_sync(rlink,
					m0_time_from_now(RLUT_SESS_TIMEOUT, 0));
	M0_UT_ASSERT(rc == 0 && !m0_rpc_link_is_connected(rlink));

	m0_rpc_link_fini(rlink);
	m0_free(rlink);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
}

static struct m0_fop *ut_fop_alloc(const char *name, uint32_t cmd)
{
	struct m0_fop     *fop;
	struct m0_sss_req *rfop;

	M0_ALLOC_PTR(fop);
	M0_UT_ASSERT(fop != NULL);

	M0_ALLOC_PTR(rfop);
	M0_UT_ASSERT(rfop != NULL);

	m0_buf_init(&rfop->ss_name, (void *)name, strlen(name));
	rfop->ss_cmd = cmd;

	m0_fop_init(fop, &m0_fop_ss_fopt, (void *)rfop, m0_ss_fop_release);

	return fop;

}

static void ut_req(struct m0_rpc_session *sess, const char *name, uint32_t cmd)
{
	int                 rc;
	struct m0_fop      *fop;

	fop = ut_fop_alloc(name, cmd);

	rc = m0_rpc_post_sync(fop, sess, NULL, 0);
	M0_UT_ASSERT(rc == 0);
	m0_fop_put_lock(fop);
}

void rlut_connect_async()
{
	struct m0_rpc_machine *mach;
	struct m0_rpc_link    *rlink;
	const char            *remote_ep;
	int                    rc;

	start_rpc_client_and_server();
	mach = &cctx.rcx_rpc_machine;
	remote_ep = cctx.rcx_remote_addr;

	M0_ALLOC_PTR(rlink);
	M0_UT_ASSERT(rlink != NULL);
	rc = m0_rpc_link_init(rlink, mach, NULL, remote_ep,
			      RLUT_MAX_RPCS_IN_FLIGHT);
	M0_UT_ASSERT(rc == 0);

	m0_clink_init(&clink, NULL);
	clink.cl_is_oneshot = true;

	m0_rpc_link_connect_async(rlink,
				  m0_time_from_now(RLUT_SESS_TIMEOUT, 0),
				  &clink);
	rc = m0_rpc_session_timedwait(&rlink->rlk_sess, M0_BITS(M0_RPC_SESSION_IDLE),
				      M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	ut_req(&rlink->rlk_sess, "M0_CST_IOS", M0_SERVICE_STATUS);

	m0_chan_wait(&clink);
	M0_UT_ASSERT(rc == 0 && m0_rpc_link_is_connected(rlink));

	m0_clink_fini(&clink);

	m0_clink_init(&clink, NULL);
	clink.cl_is_oneshot = true;
	m0_rpc_link_disconnect_async(rlink,
				     m0_time_from_now(RLUT_SESS_TIMEOUT, 0),
				     &clink);
	m0_chan_wait(&clink);
	M0_UT_ASSERT(rc == 0 && !m0_rpc_link_is_connected(rlink));
	m0_clink_fini(&clink);

	m0_rpc_link_fini(rlink);
	m0_free(rlink);
	stop_rpc_client_and_server();
}

struct m0_ut_suite link_lib_ut = {
	.ts_name = "rpc-link-ut",
	.ts_tests = {
		{ "remote-unreacheable", rlut_remote_unreachable },
		{ "reconnect",           rlut_reconnect          },
		{ "remote-delay",        rlut_remote_delay       },
		{ "reset",               rlut_reset              },
		{ "connect-async",       rlut_connect_async      },
		{ NULL, NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
