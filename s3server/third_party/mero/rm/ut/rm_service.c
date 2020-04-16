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
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 20-Mar-2013
 */

#include "rm/rm_service.h"
#include "net/lnet/lnet.h" /* m0_net_lnet_xprt */
#include "rm/ut/rmut.h"    /* rm_ctx */
#include "rpc/rpclib.h"    /* m0_rpc_server_ctx */
#include "ut/misc.h"       /* M0_UT_PATH */
#include "ut/ut.h"

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR
#define SERVER_DB            "server_db"
#define SERVER_STOB          "server_stob"
#define SERVER_ADDB_STOB     "linuxstob:server_addb_stob"
#define SERVER_LOG           "rmserver.log"

static char *server_argv[] = {
	"rm-ut", "-T", "linux", "-D", SERVER_DB,
	"-S", SERVER_STOB, "-A", SERVER_ADDB_STOB,
	"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
	"-f", M0_UT_CONF_PROCESS,
	"-c", M0_UT_PATH("conf.xc")
};

extern struct m0_reqh_service_type m0_rms_type;

static struct m0_net_xprt *xprt        = &m0_net_lnet_xprt;
static struct rm_ctx  *server_ctx  = &rm_ctxs[SERVER_1];
static struct rm_ctx  *client_ctx  = &rm_ctxs[SERVER_2];
static struct m0_clink     tests_clink[TEST_NR];
extern void flock_client_utdata_ops_set(struct rm_ut_data *data);

static struct m0_rpc_server_ctx sctx = {
	.rsx_xprts            = &xprt,
	.rsx_xprts_nr         = 1,
	.rsx_argv             = server_argv,
	.rsx_argc             = ARRAY_SIZE(server_argv),
	.rsx_log_file_name    = SERVER_LOG,
};

static void rm_service_start(struct m0_rpc_server_ctx *sctx)
{
	int result;

	result = m0_rpc_server_start(sctx);
	M0_UT_ASSERT(result == 0);
}

static void rm_service_stop(struct m0_rpc_server_ctx *sctx)
{
	m0_rpc_server_stop(sctx);
}

static void rm_svc_server(const int tid)
{
	rm_service_start(&sctx);

	/* Signal client that server is now up and running */
	m0_chan_signal_lock(&rm_ut_tests_chan);
	/* Stay alive till client runs its test cases */
	m0_chan_wait(&tests_clink[SERVER_2]);

	rm_service_stop(&sctx);
	/* Tell client that I am done */
	m0_chan_signal_lock(&rm_ut_tests_chan);
}

static void test_flock(struct m0_rm_owner *owner, struct m0_file *file,
		       struct m0_fid *fid, struct m0_rm_remote *creditor,
		       bool unwind)
{
	int                    rc;
	struct m0_rm_incoming  in;

	M0_SET0(owner);
	m0_file_init(file, fid, &rm_test_data.rd_dom, 0);
	m0_file_owner_init(owner, &m0_rm_no_group, file, creditor);
	m0_file_lock(owner, &in);
	m0_rm_owner_lock(owner);
	rc = m0_sm_timedwait(&in.rin_sm,
			     M0_BITS(RI_SUCCESS, RI_FAILURE),
			     M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(in.rin_rc == 0);
	M0_UT_ASSERT(incoming_state(&in) == RI_SUCCESS);

	m0_rm_owner_unlock(owner);
	m0_file_unlock(&in);
	if (unwind) {
		m0_rm_owner_windup(owner);
		rc = m0_rm_owner_timedwait(owner, M0_BITS(ROS_FINAL),
					   M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);
		m0_file_owner_fini(owner);
		m0_file_fini(file);
	}
}

static void rm_client(const int tid)
{
	int                      rc;
	struct m0_rm_resource   *resource;
	struct m0_rm_remote     *creditor;
	struct m0_rm_owner       owner;
	struct m0_file           file1;
	struct m0_file           file2;
	struct m0_fid            fids[] = {{0, 1}, {0, 2}};
	struct m0_reqh_service  *rmservice;

	m0_ut_rpc_mach_init_and_add(&client_ctx->rc_rmach_ctx);
	rc = m0_reqh_service_setup(&rmservice, &m0_rms_type,
				   &client_ctx->rc_rmach_ctx.rmc_reqh,
				   NULL, NULL);
	M0_UT_ASSERT(rc == 0);

	m0_mutex_init(&client_ctx->rc_mutex);
	m0_chan_init(&client_ctx->rc_chan, &client_ctx->rc_mutex);
	m0_clink_init(&client_ctx->rc_clink, NULL);

	/* Start the server */
	rc = M0_THREAD_INIT(&server_ctx->rc_thr, int, NULL, &rm_svc_server, 0,
			    "rm_svc_%d", 0);
	M0_UT_ASSERT(rc == 0);

	/* Wait till server starts */
	m0_chan_wait(&tests_clink[SERVER_1]);

	/* Connect to end point of SERVER_1 */
	rm_ctx_connect(client_ctx, server_ctx);

	M0_SET0(&file1);
	M0_SET0(&file2);
	M0_ALLOC_PTR(creditor);
	M0_UT_ASSERT(creditor != NULL);
	M0_ALLOC_PTR(resource);
	M0_UT_ASSERT(resource != NULL);

	flock_client_utdata_ops_set(&rm_test_data);
	rm_utdata_init(&rm_test_data, OBJ_RES);

	resource->r_type = rm_test_data.rd_rt;

	m0_rm_remote_init(creditor, resource);
	creditor->rem_session = &client_ctx->rc_sess[SERVER_1];
	creditor->rem_state   = REM_SERVICE_LOCATED;

	/*
	 * Test for Cancel
	 * We perform resource owner_finalisation on debtor before rm-service
	 * shuts down. This results in sending cancel request to rm-service.
	 */
	test_flock(&owner, &file1, &fids[0], creditor, true);

	/*
	 * Test the request again for same resource
	 * This test checks for caching of credits on creditor side.
	 * When we ask for the same credit second time, creditor grants
	 * this request from cached credits instead of creating a new
	 * resource (compared to above case where a new resource is
	 * created and creditor is granted a self loan)
	 */
	test_flock(&owner, &file1, &fids[0], creditor, true);

	/*
	 * Test for Revoke
	 * We disconnect from server and stop the rm-service before
	 * performing resource owner finalisation on debtor. In this case
	 * server sends revoke request for file resource.
	 */
	test_flock(&owner, &file2, &fids[1], creditor, false);

	rm_ctx_disconnect(client_ctx, server_ctx);

	/* Tell server to stop */
	m0_chan_signal_lock(&rm_ut_tests_chan);
	/* Wait for server to stop */
	m0_chan_wait(&tests_clink[SERVER_1]);

	m0_rm_owner_windup(&owner);
	rc = m0_rm_owner_timedwait(&owner, M0_BITS(ROS_FINAL), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	m0_file_owner_fini(&owner);
	m0_file_fini(&file2);
	m0_rm_remote_fini(creditor);
	m0_free(resource);
	m0_free(creditor);
	rm_utdata_fini(&rm_test_data, OBJ_RES);
	m0_clink_fini(&client_ctx->rc_clink);
	m0_chan_fini_lock(&client_ctx->rc_chan);
	m0_mutex_fini(&client_ctx->rc_mutex);
	m0_ut_rpc_mach_fini(&client_ctx->rc_rmach_ctx);
}

/*
 * Two threads are started; One server, which runs rm-service and one client
 * which requests resource credits to server.
 *
 * First server is started by using m0_rpc_server_start. rm-service option is
 * provided in rpc server context.
 *
 * Client asks for credit request of rings resource type. A dummy creditor is
 * created which points to session established with rm-service.
 *
 * When borrow request FOP reaches server, server checks that creditor cookie is
 * NULL; It now creates an owner for given resource type and grants this request
 * to client.
 */
void rmsvc(void)
{
	int rc;

	m0_mutex_init(&rm_ut_tests_chan_mutex);
	m0_chan_init(&rm_ut_tests_chan, &rm_ut_tests_chan_mutex);

	for (rc = 0; rc < 2; ++rc) {
		M0_SET0(&rm_ctxs[rc]);
		rm_ctxs[rc].rc_id = rc;
		rm_ctxs[rc].rc_rmach_ctx.rmc_cob_id.id = cob_ids[rc];
		rm_ctxs[rc].rc_rmach_ctx.rmc_ep_addr = serv_addr[rc];
		m0_clink_init(&tests_clink[rc], NULL);
		m0_clink_add_lock(&rm_ut_tests_chan, &tests_clink[rc]);
	}

	/* Start client */
	rc = M0_THREAD_INIT(&client_ctx->rc_thr, int, NULL, &rm_client, 0,
			    "rm_cli_%d", 0);
	M0_UT_ASSERT(rc == 0);

	m0_thread_join(&client_ctx->rc_thr);
	m0_thread_join(&server_ctx->rc_thr);
	m0_thread_fini(&server_ctx->rc_thr);
	m0_thread_fini(&client_ctx->rc_thr);

	for (rc = 0; rc <= 1; ++rc) {
		m0_clink_del_lock(&tests_clink[rc]);
		m0_clink_fini(&tests_clink[rc]);
	}

	m0_chan_fini_lock(&rm_ut_tests_chan);
	m0_mutex_fini(&rm_ut_tests_chan_mutex);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
