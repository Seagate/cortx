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
 * Original author: Igor Perelyotov <igor.m.perelyotov@seagate.com>
 * Original creation date: 04-Mar-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "sss/ss_fops.h"       /* m0_sss_req */
#include "sss/process_fops.h"  /* m0_ss_process_req */
#include "sss/device_fops.h"   /* m0_sss_device_fop */
#include "net/lnet/lnet.h"     /* m0_net_lnet_xprt */
#include "rpc/rpclib.h"        /* m0_rpc_server_ctx */
#include "lib/finject.h"
#include "ut/misc.h"           /* M0_UT_PATH */
#include "ut/ut.h"
#include "mero/version.h"      /* m0_build_info_get */

#define SERVER_DB_NAME        "sss_ut_server.db"
#define SERVER_STOB_NAME      "sss_ut_server.stob"
#define SERVER_ADDB_STOB_NAME "linuxstob:sss_ut_server.addb_stob"
#define SERVER_LOG_NAME       "sss_ut_server.log"
#define SERVER_ENDPOINT_ADDR  "0@lo:12345:34:1"
#define SERVER_ENDPOINT       "lnet:" SERVER_ENDPOINT_ADDR

#define CLIENT_DB_NAME        "sss_ut_client.db"
#define CLIENT_ENDPOINT_ADDR  "0@lo:12345:34:*"

enum {
	MAX_RPCS_IN_FLIGHT = 1,
};

static const struct m0_fid ut_fid = {
	.f_container = 0x7300000000000001,
	.f_key       = 10
};

static struct m0_net_domain    client_net_dom;
static struct m0_net_xprt     *xprt = &m0_net_lnet_xprt;

static char *server_argv[] = {
	"sss_ut", "-T", "AD", "-D", SERVER_DB_NAME,
	"-S", SERVER_STOB_NAME, "-A", SERVER_ADDB_STOB_NAME,
	"-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR, "-w", "10",
	"-f", M0_UT_CONF_PROCESS,
	"-c", M0_UT_PATH("conf.xc")
};

static struct m0_rpc_server_ctx sctx = {
	.rsx_xprts         = &xprt,
	.rsx_xprts_nr      = 1,
	.rsx_argv          = server_argv,
	.rsx_argc          = ARRAY_SIZE(server_argv),
	.rsx_log_file_name = SERVER_LOG_NAME,
};

static struct m0_rpc_client_ctx cctx = {
	.rcx_net_dom            = &client_net_dom,
	.rcx_local_addr         = CLIENT_ENDPOINT_ADDR,
	.rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
	.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
	.rcx_fid                = &g_process_fid,
};

extern const struct m0_fom_type_ops ss_process_fom_type_ops;
extern struct m0_fop_type m0_fop_process_fopt;

static void rpc_client_and_server_start(void)
{
	int rc;

	rc = m0_net_domain_init(&client_net_dom, xprt);
	M0_ASSERT(rc == 0);
#if 0
	/* Test case: memory error on service allocate */
	m0_fi_enable("ss_svc_rsto_service_allocate", "fail_allocation");
	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc != 0);
	m0_fi_disable("ss_svc_rsto_service_allocate", "fail_allocation");
	m0_rpc_server_stop(&sctx);
#endif
	/* Normal start */
	rc = m0_rpc_server_start(&sctx);
	M0_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&cctx);
	M0_ASSERT(rc == 0);
}

static void rpc_client_and_server_stop(void)
{
	int rc;
	M0_LOG(M0_DEBUG, "stop");
	rc = m0_rpc_client_stop(&cctx);
	M0_ASSERT(rc == 0);
	m0_rpc_server_stop(&sctx);
	m0_net_domain_fini(&client_net_dom);
}

static struct m0_fop *sss_ut_fop_alloc(const char *name, uint32_t cmd)
{
	struct m0_fop     *fop;
	struct m0_sss_req *ss_fop;

	M0_ALLOC_PTR(fop);
	M0_ASSERT(fop != NULL);

	M0_ALLOC_PTR(ss_fop);
	M0_ASSERT(ss_fop != NULL);

	m0_buf_init(&ss_fop->ss_name, (void *)name, strlen(name));
	ss_fop->ss_cmd = cmd;
	ss_fop->ss_id  = ut_fid;

	m0_fop_init(fop, &m0_fop_ss_fopt, (void *)ss_fop, m0_ss_fop_release);

	return fop;
}

static void sss_ut_req(uint32_t cmd,
		       int32_t  expected_rc,
		       uint32_t expected_state)
{
	int                 rc;
	struct m0_fop      *fop;
	struct m0_fop      *rfop;
	struct m0_rpc_item *item;
	struct m0_sss_rep  *ss_rfop;

	fop = sss_ut_fop_alloc("M0_CST_MDS", cmd);
	item = &fop->f_item;
	rc = m0_rpc_post_sync(fop, &cctx.rcx_session, NULL, 0);
	M0_UT_ASSERT(rc == 0);

	rfop = m0_rpc_item_to_fop(item->ri_reply);
	M0_UT_ASSERT(rfop != NULL);

	ss_rfop = m0_fop_data(rfop);
	M0_UT_ASSERT(ss_rfop->ssr_rc == expected_rc);

	if (expected_state != 0)
		M0_UT_ASSERT(ss_rfop->ssr_state == expected_state);

	m0_fop_put_lock(fop);
}

static int sss_ut_init(void)
{
	M0_ENTRY();
	rpc_client_and_server_start();
	M0_LEAVE();
	return M0_RC(0);
}

static int sss_ut_fini(void)
{
	M0_ENTRY();
	rpc_client_and_server_stop();
	M0_LEAVE();
	return M0_RC(0);
}

static void sss_commands_test(void)
{
	/* quiesce and stop */
	sss_ut_req(M0_SERVICE_STATUS, 0, M0_RST_STARTED);
	sss_ut_req(M0_SERVICE_QUIESCE, 0, M0_RST_STOPPING);
	sss_ut_req(M0_SERVICE_STOP, 0, M0_RST_STOPPED);

	/* init */
	sss_ut_req(M0_SERVICE_STATUS, -ENOENT, 0);
	sss_ut_req(M0_SERVICE_INIT, 0, M0_RST_INITIALISED);
	sss_ut_req(M0_SERVICE_STATUS, 0, M0_RST_INITIALISED);

	/* start */
	sss_ut_req(M0_SERVICE_START, 0, M0_RST_STARTED);
	sss_ut_req(M0_SERVICE_STATUS, 0, M0_RST_STARTED);

	/* health */
	/* health is not implemented for mdservice now */
	sss_ut_req(M0_SERVICE_HEALTH, M0_HEALTH_UNKNOWN, M0_RST_STARTED);

	/* quiesce */
	sss_ut_req(M0_SERVICE_QUIESCE, 0, M0_RST_STOPPING);
	sss_ut_req(M0_SERVICE_STATUS, 0, M0_RST_STOPPING);

	/* stop */
	sss_ut_req(M0_SERVICE_STOP, 0, M0_RST_STOPPED);
	sss_ut_req(M0_SERVICE_STATUS, -ENOENT, 0);
}

static struct m0_fop *ut_sss_process_create_req(uint32_t cmd)
{
	struct m0_fop            *fop;
	struct m0_ss_process_req *process_fop_req;

	M0_ALLOC_PTR(fop);
	M0_UT_ASSERT(fop != NULL);
	M0_ALLOC_PTR(process_fop_req);
	M0_UT_ASSERT(process_fop_req != NULL);

	process_fop_req->ssp_cmd = cmd;
	process_fop_req->ssp_id = M0_FID_TINIT('r', 1, 5);
	m0_fop_init(fop,
		    &m0_fop_process_fopt,
		    (void *)process_fop_req,
		    m0_fop_release);

	return fop;
}

static void ut_sss_process_param_set(struct m0_fop *fop, const char *name)
{
	struct m0_ss_process_req *req = m0_fop_data(fop);

	if (name != NULL)
		req->ssp_param = M0_BUF_INIT_CONST(strlen(name) + 1, name);
	else
		req->ssp_param = M0_BUF_INIT0;
}

/**
 * Processes commands test
 */
static void ut_sss_process_req(struct m0_fop *fop, int ssr_rc_exptd)
{
	int                       rc;
	struct m0_fop            *rfop;
	struct m0_ss_process_rep *process_fop_resp;
	struct m0_rpc_item       *item;

	item = &fop->f_item;
	rc = m0_rpc_post_sync(fop, &cctx.rcx_session, NULL, 0);
	M0_UT_ASSERT(rc == 0);

	rfop  = m0_rpc_item_to_fop(item->ri_reply);
	M0_UT_ASSERT(rfop != NULL);

	process_fop_resp = m0_fop_data(rfop);
	M0_UT_ASSERT(process_fop_resp->sspr_rc == ssr_rc_exptd);
	ut_sss_process_param_set(fop, NULL);
	m0_fop_put_lock(fop);
}

static void sss_process_quiesce_test(void)
{
	struct m0_fop *fop;

	fop = ut_sss_process_create_req(M0_PROCESS_QUIESCE);
	ut_sss_process_req(fop, 0);
}

static void sss_process_reconfig_test(void)
{
	struct m0_fop *fop;

	m0_fi_enable_once("ss_process_reconfig", "unit_test");
	fop = ut_sss_process_create_req(M0_PROCESS_RECONFIG);
	ut_sss_process_req(fop, 0);
}

static void sss_process_health_test(void)
{
	struct m0_fop *fop;

	fop = ut_sss_process_create_req(M0_PROCESS_HEALTH);
	ut_sss_process_req(fop, M0_HEALTH_GOOD);
}

static void sss_process_stop_test(void)
{
	struct m0_fop *fop;

	m0_fi_enable_once("m0_ss_process_stop_fop_release", "no_kill");
	fop = ut_sss_process_create_req(M0_PROCESS_STOP);
	ut_sss_process_req(fop, 0);
}

static void sss_process_commands_test(void)
{
	sss_process_reconfig_test();
	sss_process_quiesce_test();
	sss_process_health_test();
	sss_process_stop_test();
}

static void sss_process_svc_list_test(void)
{
	int                                rc;
	struct m0_fop                     *fop;
	struct m0_fop                     *rfop;
	struct m0_ss_process_svc_list_rep *process_fop_resp;
	struct m0_rpc_item                *item;

	fop = ut_sss_process_create_req(M0_PROCESS_RUNNING_LIST);
	item = &fop->f_item;
	rc = m0_rpc_post_sync(fop, &cctx.rcx_session, NULL, 0);
	M0_UT_ASSERT(rc == 0);

	rfop  = m0_rpc_item_to_fop(item->ri_reply);
	M0_UT_ASSERT(rfop != NULL);

	process_fop_resp = m0_fop_data(rfop);
	M0_UT_ASSERT(process_fop_resp->sspr_rc == 0);
	rc = process_fop_resp->sspr_rc;
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(process_fop_resp->sspr_services.ab_count > 0);

	m0_fop_put_lock(fop);
}

static void sss_process_lib_load_noent_test(void)
{
	struct m0_fop *fop;

	fop = ut_sss_process_create_req(M0_PROCESS_LIB_LOAD);
	ut_sss_process_param_set(fop, "no-such-library");
	ut_sss_process_req(fop, -EINVAL);
}

static void sss_process_lib_load_libc_test(void)
{
	struct m0_fop *fop;

	fop = ut_sss_process_create_req(M0_PROCESS_LIB_LOAD);
	ut_sss_process_param_set(fop, "/lib64/libc.so.6"); /* OK, fragile. */
	ut_sss_process_req(fop, 0);
}

/**
 * Tests library loading request processing.
 *
 * Loads libtestlib.so library. The "mero_lib_init()" function in this library
 * (sss/ut/testlib.c) is invoked and enables a specific failure injection point,
 * checked by this test.
 */
static void sss_process_lib_load_testlib_test(void)
{
	struct m0_fop *fop;
	char          *sopath;
	int            rc;
	int            i;

	/*
	 * This failure injection can be enabled, because libtestlib.so is also
	 * loaded by other tests.
	 */
	m0_fi_enable(__func__, "loaded");
	m0_fi_disable(__func__, "loaded");
	/*
	 * Convoluted code below because, M0_FI_ENABLED() introduces a static
	 * structure. That is, multiple M0_FI_ENABLED(LABEL) with the same
	 * label are *different* fi points.
	 */
	for (i = 0; i < 2; ++i) {
		M0_UT_ASSERT(!!M0_FI_ENABLED("loaded") == !!i);
		if (i == 0) {
			rc = asprintf(&sopath, "%s/%s",
				      m0_build_info_get()->bi_build_dir,
				      "ut/.libs/libtestlib.so.0.0.0");
			M0_UT_ASSERT(rc >= 0);
			M0_UT_ASSERT(sopath != NULL);
			fop = ut_sss_process_create_req(M0_PROCESS_LIB_LOAD);
			ut_sss_process_param_set(fop, sopath);
			ut_sss_process_req(fop, 0);
		} else
			free(sopath);
	}
}

static void sss_fop_ut_release(struct m0_ref *ref)
{
	struct m0_fop *fop;
	fop = M0_AMB(fop, ref, f_ref);
	m0_free(fop);
}

static void sss_process_fom_create_fail(void)
{
	int             rc;
	struct m0_fop  *fop;
	struct m0_fom  *fom;
	struct m0_reqh *reqh;
	struct m0_ss_process_req *req;

	M0_ALLOC_PTR(fop);
	M0_ALLOC_PTR(req);
	M0_ALLOC_PTR(fom);
	M0_ALLOC_PTR(reqh);

	m0_fop_init(fop, &m0_fop_process_fopt, req,
		    sss_fop_ut_release);
	req->ssp_cmd = 1;

	m0_fi_enable_once("ss_process_fom_create", "fom_alloc_fail");
	rc = ss_process_fom_type_ops.fto_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_once("ss_process_fom_create", "fop_alloc_fail");
	fop->f_item.ri_rmachine = &cctx.rcx_rpc_machine;
	rc = ss_process_fom_type_ops.fto_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_once("ss_process_fom_create", "fop_data_alloc_fail");
	fop->f_item.ri_rmachine = &cctx.rcx_rpc_machine;
	rc = ss_process_fom_type_ops.fto_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fop_fini(fop);
	m0_free(fop);
	m0_free(fom);
	m0_free(reqh);
}

/**
 * device-fom-fail
 *
 * Test fail create SNS Device FOM
 */
static void sss_device_fom_create_fail(void)
{
	int                       rc;
	struct m0_fop            *fop;
	struct m0_fom            *fom;
	struct m0_sss_device_fop *req;
	struct m0_reqh *reqh;

	M0_ALLOC_PTR(fop);
	M0_ALLOC_PTR(req);
	M0_ALLOC_PTR(fom);
	M0_ALLOC_PTR(reqh);

	m0_fop_init(fop, &m0_sss_fop_device_fopt, req, m0_fop_release);
	req->ssd_cmd = M0_DEVICE_ATTACH;

	m0_fi_enable_once("sss_device_fom_create", "fom_alloc_fail");
	fop->f_item.ri_rmachine = &cctx.rcx_rpc_machine;
	rc = fop->f_type->ft_fom_type.ft_ops->fto_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_once("sss_device_fom_create", "fop_alloc_fail");
	fop->f_item.ri_rmachine = &cctx.rcx_rpc_machine;
	rc = fop->f_type->ft_fom_type.ft_ops->fto_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fop_fini(fop);
	m0_free(fop);
	m0_free(fom);
	m0_free(reqh);
}

struct m0_ut_suite sss_ut = {
	.ts_name = "sss-ut",
	.ts_init = sss_ut_init,
	.ts_fini = sss_ut_fini,
	.ts_tests = {
		{ "commands", sss_commands_test },
		{ "process-commands", sss_process_commands_test },
		{ "process-services-list", sss_process_svc_list_test },
		{ "process-fom-create-fail", sss_process_fom_create_fail },
		{ "device-fom-fail", sss_device_fom_create_fail },
		{ "lib-load-noent", sss_process_lib_load_noent_test },
		{ "lib-load-libc", sss_process_lib_load_libc_test },
		{ "lib-testlib", sss_process_lib_load_testlib_test },
		{ NULL, NULL },
	},
};
M0_EXPORTED(sss_ut);

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
