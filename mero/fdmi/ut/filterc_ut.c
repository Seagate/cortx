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
 * Original author: Yuriy Umanets <yuriy.umanets@seagate.com>
 * Original creation date: 5/5/2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "ut/ut.h"
#include "fdmi/fdmi.h"
#include "fdmi/ut/sd_common.h"  /* M0_FDMI_UT_PATH */

#include "net/lnet/lnet.h"      /* m0_net_lnet_xprt */
#include "rpc/rpclib.h"
#include "ut/cs_service.h"      /* m0_cs_default_stypes */
#include "lib/string.h"         /* m0_strdup */
#include "lib/finject.h"
#include "fdmi/service.h"       /* m0_reqh_fdmi_svc_params */
#include "conf/ut/rpc_helpers.h"
#include "ut/misc.h"
#include <errno.h>

static char                    g_fdmi_data[] = "hello, FDMI";
static struct m0_fdmi_src_rec  g_src_rec;

static struct m0_filterc_ops  *ufc_fco;
static struct m0_reqh_service *ufc_fdmi_service;

static struct m0_net_xprt *m0_fdmi_ut_xprt = &m0_net_lnet_xprt;

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:*"
#define SERVER_ENDPOINT_ADDR  "0@lo:12345:34:1"
#define SERVER_ENDPOINT       "lnet:" SERVER_ENDPOINT_ADDR

#define SERVER_DB_NAME        "fdmi_filterc_ut.db"
#define SERVER_STOB_NAME      "fdmi_filterc_ut.stob"
#define SERVER_ADDB_STOB_NAME "linuxstob:fdmi_filterc_ut.addb_stob"
#define SERVER_LOG_NAME       "fdmi_filterc_ut.log"
#define SERVER_ENDPOINT_ADDR  "0@lo:12345:34:1"

#define CLIENT_DB_NAME        "fdmi_filterc_ut.db"

static struct m0_net_domain    client_net_dom;

static char *server_argv[] = {
	"fdmi_filterc_ut", "-T", "AD", "-D", SERVER_DB_NAME,
	"-S", SERVER_STOB_NAME, "-A", SERVER_ADDB_STOB_NAME,
	"-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR, "-w", "10",
	"-c", M0_SRC_PATH("fdmi/ut/conf.xc")
};

enum {
	MAX_RPCS_IN_FLIGHT = 1,
};

static struct m0_rpc_server_ctx sctx = {
	.rsx_xprts         = &m0_fdmi_ut_xprt,
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

/* ----------------------------------------------------------------
 * FilterC setup/teardown
 * ---------------------------------------------------------------- */

static struct m0_reqh *mero2reqh(struct m0_mero *mero)
{
  	return &mero->cc_reqh_ctx.rc_reqh;
}

static int ut_filterc_start()
{
	struct m0_reqh *reqh = mero2reqh(&sctx.rsx_mero_ctx);
	struct m0_reqh_service_type *stype;
	bool start_service = false;
	int rc = 0;

	stype = m0_reqh_service_type_find("M0_CST_FDMI");
	if (stype == NULL) {
		M0_LOG(M0_ERROR, "FDMI service type is not found.");
		return M0_ERR_INFO(-EINVAL, "Unknown reqh service type: fdmi");
	}

	ufc_fdmi_service = m0_reqh_service_find(stype, reqh);
	if (ufc_fdmi_service == NULL) {
		rc = m0_reqh_service_allocate(&ufc_fdmi_service, &m0_fdmi_service_type, NULL);
		M0_UT_ASSERT(rc == 0);
		m0_reqh_service_init(ufc_fdmi_service, reqh, NULL);
		start_service = true;
	}

	/* Patch filterc instance used by source dock FOM */
	if (ufc_fco != NULL) {
		struct m0_reqh_fdmi_svc_params *fdms_start_params;
		M0_ALLOC_PTR(fdms_start_params);
		M0_ASSERT(fdms_start_params != NULL);
		fdms_start_params->filterc_ops = ufc_fco;

		m0_buf_init(&ufc_fdmi_service->rs_ss_param,
			    fdms_start_params,
			    sizeof(*fdms_start_params));
	}

	if (start_service) {
		rc = m0_reqh_service_start(ufc_fdmi_service);
		M0_UT_ASSERT(rc == 0);
	}
	return M0_RC(rc);
}

/* ----------------------------------------------------------------
 * FilterC Operation replacement
 * ---------------------------------------------------------------- */

static int ut_filterc_fco_start(struct m0_filterc_ctx *ctx,
				struct m0_reqh *reqh)
{
	int rc;

	rc = filterc_def_ops.fco_start(ctx, reqh);
	M0_UT_ASSERT(rc == 0);
	return rc;
}

static void rpc_client_and_server_start(void)
{
	int rc;

	rc = m0_net_domain_init(&client_net_dom, m0_fdmi_ut_xprt);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);
}

static void rpc_client_and_server_stop(void)
{
	int rc;
	rc = m0_rpc_client_stop(&cctx);
	M0_UT_ASSERT(rc == 0);
	m0_rpc_server_stop(&sctx);
	m0_net_domain_fini(&client_net_dom);
}

static struct m0_cond match_cond;
static struct m0_mutex cond_mutex;

static int test_fs_node_eval(
		struct m0_fdmi_src_rec *src_rec,
		struct m0_fdmi_flt_var_node *value_desc,
		struct m0_fdmi_flt_operand *value)
{
	bool matched;

	M0_UT_ASSERT(src_rec == &g_src_rec);
	M0_UT_ASSERT(src_rec->fsr_data == &g_fdmi_data);

	 /**
	  * Configuration root definition for the filter, contains two bool
	  * operands, one of them is true and this what we check here.
	  * However src_rec->fsr_matched is set in fdmi_eval generic code
	  * and this callback is only called for var nodes but is mandatory
	  * for registering custom filter. So basically this is just in case.
	  */
	matched = value->ffo_type == M0_FF_OPND_BOOL &&
		value->ffo_data.fpl_type == M0_FF_OPND_PLD_BOOL &&
		value->ffo_data.fpl_pld.fpl_boolean;
	m0_fdmi_flt_bool_opnd_fill(value, matched);
	src_rec->fsr_matched = matched;
	return 0;
}

static int test_fs_encode(struct m0_fdmi_src_rec *src_rec,
			  struct m0_buf          *buf)
{
	return 0;
}

static void test_fs_get(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
}

static void test_fs_put(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
}

static void test_fs_begin(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
}

static void test_fs_end(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
	m0_mutex_lock(&cond_mutex);
	m0_cond_broadcast(&match_cond);
	m0_mutex_unlock(&cond_mutex);
}

static struct m0_fdmi_src *src_alloc()
{
	struct m0_fdmi_src *src;
	int                 rc;

	rc = m0_fdmi_source_alloc(M0_FDMI_REC_TYPE_TEST, &src);
	M0_UT_ASSERT(rc == 0);

	src->fs_node_eval  = test_fs_node_eval;
	src->fs_get        = test_fs_get;
	src->fs_put        = test_fs_put;
	src->fs_begin      = test_fs_begin;
	src->fs_end        = test_fs_end;
	src->fs_encode     = test_fs_encode;
	return src;
}

static void filterc_connect_to_confd(void)
{
	struct m0_filterc_ops  fc_ops = filterc_def_ops;
	struct m0_fdmi_src    *src = src_alloc();
	int                    rc;
	M0_ENTRY();

	m0_mutex_init(&cond_mutex);
	m0_cond_init(&match_cond, &cond_mutex);

	ufc_fco = &fc_ops;
	fc_ops.fco_start = ut_filterc_fco_start;

	rpc_client_and_server_start();

	rc = ut_filterc_start();
	M0_UT_ASSERT(rc == 0);

	rc = m0_fdmi_source_register(src);
	M0_UT_ASSERT(rc == 0);
	g_src_rec = (struct m0_fdmi_src_rec) {
		/* Don't send to remote ep. */
		.fsr_dryrun  = true,
		.fsr_matched = false,
		.fsr_src     = src,
		.fsr_data    = g_fdmi_data,
	};

	rc = M0_FDMI_SOURCE_POST_RECORD(&g_src_rec);
	M0_UT_ASSERT(rc == 0);

	m0_mutex_lock(&cond_mutex);
	m0_cond_wait(&match_cond);
	m0_mutex_unlock(&cond_mutex);
	M0_UT_ASSERT(g_src_rec.fsr_matched);

	m0_fdmi_source_deregister(src);
	m0_fdmi_source_free(src);

	rpc_client_and_server_stop();
	m0_cond_fini(&match_cond);
	m0_mutex_fini(&cond_mutex);

	M0_LEAVE();
}

struct m0_ut_suite fdmi_filterc_ut = {
	.ts_name = "fdmi-filterc-ut",
	.ts_tests = {
		{ "filterc-connect-to-confd", filterc_connect_to_confd},
		{ NULL, NULL },
	},
};

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
