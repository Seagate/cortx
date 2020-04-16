/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"
#include "lib/string.h"

#include "ut/ut.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "fdmi/fdmi.h"
#include "fdmi/service.h"
#include "mdstore/mdstore.h"
#include "rpc/rpc_machine_internal.h"
#include "rpc/packet_internal.h"  /* m0_rpc_packet_traverse_items */
#include "rpc/item_internal.h"    /* m0_rpc_item_failed */
#include "rpc/session_internal.h"
#include "rpc/conn_internal.h"

#include "fdmi/ut/sd_common.h"

struct fdmi_sd_ut_ctx              g_sd_ut;
static struct m0_mdstore           md;

/* ------------------------------------------------------------------
 * This stuff is needed to initialize m0_mero (g_sd_ut.mero).
 * ------------------------------------------------------------------ */

#define LOG_FILE_NAME "fdmi_sd_ut.errlog"

static struct m0_net_xprt *sd_ut_xprts[] = {
	&m0_net_lnet_xprt,
};
static FILE               *sd_ut_lfile;

/* ------------------------------------------------------------------
 * Forward declarations.`
 * ------------------------------------------------------------------ */

M0_INTERNAL struct m0_rpc_chan *rpc_chan_get(struct m0_rpc_machine *machine,
					     struct m0_net_end_point *dest_ep,
					     uint64_t max_rpcs_in_flight);

M0_INTERNAL void rpc_chan_put(struct m0_rpc_chan *chan);

/* ------------------------------------------------------------------
 * Other helpers.
 * ------------------------------------------------------------------ */

void fdmi_serv_start_ut(const struct m0_filterc_ops *filterc_ops)
{
	int rc;
	struct m0_reqh_fdmi_svc_params	*fdms_start_params;
	struct m0_reqh_context 		*reqh_ctx = &g_sd_ut.mero.cc_reqh_ctx;
	struct m0_reqh			*reqh     = &reqh_ctx->rc_reqh;

	M0_ENTRY();
	M0_LOG(M0_DEBUG, ">> fdmi_serv_start_ut ");

	memset(reqh, 0, sizeof(struct m0_reqh));

	sd_ut_lfile = fopen(LOG_FILE_NAME, "w+");
	M0_UT_ASSERT(sd_ut_lfile != NULL);

	rc = m0_cs_init(&g_sd_ut.mero, sd_ut_xprts, ARRAY_SIZE(sd_ut_xprts),
			sd_ut_lfile, false);
	M0_UT_ASSERT(rc == 0);

	rc = M0_REQH_INIT(reqh,
			.rhia_fid = &g_process_fid,
			.rhia_dtm = (void *)1,
			.rhia_mdstore = &md);
	M0_UT_ASSERT(rc == 0);

	rc = m0_reqh_service_allocate(&g_sd_ut.fdmi_service, &m0_fdmi_service_type, NULL);
	M0_UT_ASSERT(rc == 0);

	m0_reqh_service_init(g_sd_ut.fdmi_service, reqh, NULL);

	/* Patch filterc instance used by source dock FOM */
	M0_ALLOC_PTR(fdms_start_params);
	M0_ASSERT(fdms_start_params != NULL);
	if (filterc_ops != NULL) {
		fdms_start_params->filterc_ops = filterc_ops;
	}

	m0_buf_init(&g_sd_ut.fdmi_service->rs_ss_param,
		    fdms_start_params,
		    sizeof(struct m0_reqh_fdmi_svc_params));

	m0_reqh_rpc_mach_tlink_init_at_tail(
			&g_sd_ut.rpc_machine,
		       	&reqh->rh_rpc_machines);

	m0_reqh_service_start(g_sd_ut.fdmi_service);

	m0_reqh_start(reqh);

	M0_LOG(M0_DEBUG, "<< fdmi_serv_start_ut ");
	M0_LEAVE();
}

void fdmi_serv_stop_ut(void)
{
	struct m0_reqh *reqh = &g_sd_ut.mero.cc_reqh_ctx.rc_reqh;

	M0_ENTRY();

	M0_LOG(M0_DEBUG, ">>fdmi_serv_stop_ut ");

	m0_reqh_rpc_mach_tlist_pop(&reqh->rh_rpc_machines);

	m0_reqh_shutdown_wait(reqh);
	m0_reqh_services_terminate(reqh);
	m0_reqh_fini(reqh);

	m0_cs_fini(&g_sd_ut.mero);
	M0_SET0(&g_sd_ut);
	fclose(sd_ut_lfile);
	sd_ut_lfile = NULL;

	M0_LOG(M0_DEBUG, "<< fdmi_serv_stop_ut ");
	M0_LEAVE();
}

/* ------------------------------------------------------------------
 * Start/stop unit test.
 * ------------------------------------------------------------------ */

static void test_item_done(struct m0_rpc_packet *packet M0_UNUSED,
			   struct m0_rpc_item *item, int rc)
{
	m0_rpc_item_failed(item, rc);
}

void fdmi_ut_packet_send_failed(struct m0_rpc_machine *mach,
	                        struct m0_rpc_packet *p)
{

	M0_ENTRY("mach %p, p %p", mach, p);

	m0_rpc_machine_lock(mach);

	m0_rpc_packet_traverse_items(p, test_item_done, -EINVAL);
	m0_rpc_frm_packet_done(p);
	m0_rpc_packet_discard(p);

	m0_rpc_machine_unlock(mach);
	M0_LEAVE();
}

/** @todo Add prefix */
void prepare_rpc_env(struct test_rpc_env         *env,
	             struct m0_reqh              *reqh,
	             const struct m0_rpc_frm_ops *frm_ops,
		     bool                         sender,
		     struct m0_rpc_conn          *rpc_conn,
		     struct m0_rpc_session       *rpc_session)
{
	enum { TEST_TM_NR = 1 }; /* Number of TMs. */
	int                      rc;
	struct m0_net_xprt      *xprt = &m0_net_lnet_xprt;

	M0_ENTRY();

	M0_ASSERT(rpc_conn != NULL && rpc_session != NULL);

	m0_net_domain_init(&env->tre_net_dom, xprt);

	rc = m0_rpc_net_buffer_pool_setup(
		&env->tre_net_dom, &env->tre_buffer_pool,
		m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, TEST_TM_NR),
		TEST_TM_NR);
	M0_ASSERT(rc == 0);

	env->ep_addr_local  = "0@lo:12345:32:123";
	env->ep_addr_remote = "0@lo:12345:32:123";
	rc = m0_rpc_machine_init(&env->tre_rpc_machine,
			         &env->tre_net_dom,
				 env->ep_addr_local,
				 reqh, &env->tre_buffer_pool, M0_BUFFER_ANY_COLOUR,
				 2048, /* max RPC message size */
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_ASSERT(rc == 0);

	rc = m0_net_end_point_create(&env->tre_ep,
		                     &env->tre_rpc_machine.rm_tm,
				     env->ep_addr_remote);
	M0_ASSERT(rc == 0);

	m0_rpc_machine_lock(&env->tre_rpc_machine);
	env->tre_rpc_chan = rpc_chan_get(&env->tre_rpc_machine, env->tre_ep,
		                         10 /* max packets in flight */);
	env->tre_rpc_chan->rc_frm.f_ops = frm_ops;
	M0_ASSERT(env->tre_rpc_chan != NULL);

	env->tre_conn = rpc_conn;
	if (!sender) {
		rc = m0_rpc_rcv_conn_init(env->tre_conn, env->tre_ep,
		       	&env->tre_rpc_machine, &M0_UINT128(10, 10));
	} else {
		m0_rpc_machine_unlock(&env->tre_rpc_machine);
		rc = m0_rpc_conn_init(env->tre_conn, NULL, env->tre_ep,
		       	&env->tre_rpc_machine, 10);
		m0_rpc_machine_lock(&env->tre_rpc_machine);
	}

	conn_state_set(env->tre_conn, M0_RPC_CONN_ACTIVE);
	env->tre_conn->c_sender_id = 0xFEFE;
	m0_rpc_machine_unlock(&env->tre_rpc_machine);
	M0_ASSERT(rc == 0);

	env->tre_session = rpc_session;
	rc = m0_rpc_session_init(env->tre_session, env->tre_conn);
	env->tre_session->s_session_id = 10;
	m0_rpc_machine_lock(&env->tre_rpc_machine);
	session_state_set(env->tre_session, M0_RPC_SESSION_IDLE);
	m0_rpc_machine_unlock(&env->tre_rpc_machine);
	M0_ASSERT(rc == 0);

	M0_LEAVE();
}

void unprepare_rpc_env(struct test_rpc_env *env)
{
	M0_ENTRY("env %p", env);

	if (session_state(env->tre_session) == M0_RPC_SESSION_BUSY) {
		m0_rpc_machine_lock(&env->tre_rpc_machine);
		session_state_set(env->tre_session, M0_RPC_SESSION_IDLE);
		env->tre_session->s_hold_cnt = 0;
		env->tre_rpc_chan->rc_frm.f_state = FRM_IDLE;
		env->tre_rpc_chan->rc_frm.f_nr_items = 0;
		env->tre_rpc_chan->rc_frm.f_nr_packets_enqed = 0;
		m0_sm_fail(&env->tre_session->s_sm,
			   M0_RPC_SESSION_FAILED, -EBUSY);
		m0_rpc_machine_unlock(&env->tre_rpc_machine);
	} else {
		m0_rpc_machine_lock(&env->tre_rpc_machine);
		session_state_set(env->tre_session, M0_RPC_SESSION_TERMINATED);
		m0_rpc_machine_unlock(&env->tre_rpc_machine);
	}
	m0_rpc_session_fini(env->tre_session);

	m0_rpc_machine_lock(&env->tre_rpc_machine);
	conn_state_set(env->tre_conn, M0_RPC_CONN_TERMINATED);
	m0_rpc_machine_unlock(&env->tre_rpc_machine);
	m0_rpc_conn_fini(env->tre_conn);

	m0_rpc_machine_lock(&env->tre_rpc_machine);
	rpc_chan_put(env->tre_rpc_chan);
	m0_rpc_machine_unlock(&env->tre_rpc_machine);

	m0_net_end_point_put(env->tre_ep);

	m0_rpc_machine_fini(&env->tre_rpc_machine);
	m0_rpc_net_buffer_pool_cleanup(&env->tre_buffer_pool);

	m0_net_domain_fini(&env->tre_net_dom);

	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

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
