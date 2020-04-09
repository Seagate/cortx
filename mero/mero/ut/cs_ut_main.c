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
 * Original creation date: 12/10/2011
 */

#include "mero/setup.c"

#include "net/bulk_mem.h"  /* m0_net_bulk_mem_xprt */
#include "ut/cs_fop.h"     /* CS_UT_SERVICE1 */
#include "ut/misc.h"       /* M0_UT_PATH */
#include "ut/ut.h"
#include "mero/iem.h"
#include "rm/st/wlock_helper.h"

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR

extern const struct m0_tl_descr ndoms_descr;

/* Client context */
struct cl_ctx {
	/* Client network domain.*/
	struct m0_net_domain     cl_ndom;
	/* Client rpc context.*/
	struct m0_rpc_client_ctx cl_ctx;
};

/* Configures mero environment with given parameters. */
static char *cs_ut_service_one_cmd[] = { "m0d", "-T", "linux",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
                                "-e", SERVER_ENDPOINT,
                                "-H", SERVER_ENDPOINT_ADDR,
				"-w", "10",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_services_many_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
				"-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-e", "bulk-mem:127.0.0.1:35678",
                                "-H", SERVER_ENDPOINT_ADDR,
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_opts_jumbled_cmd[] = { "m0d", "-D",
                                "cs_sdb", "-T", "AD",
				"-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-H", SERVER_ENDPOINT_ADDR,
                                "-S", "cs_stob", "-A", "linuxstob:cs_addb_stob",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_dev_stob_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
				"-w", "10",
				"-U",
                                "-e", SERVER_ENDPOINT,
                                "-H", SERVER_ENDPOINT_ADDR,
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_stype_bad_cmd[] = { "m0d", "-T", "asdadd",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_sdb",
				"-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-H", SERVER_ENDPOINT_ADDR,
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_xprt_bad_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_sdb",
				"-w", "10",
                                "-e", "asdasdada:172.18.50.40@o2ib1:34567:2",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_ep_bad_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_sdb", "-w", "10",
                                "-e", "lnet:asdad:asdsd:sadasd",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_service_bad_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_sdb", "-w", "10",
                                "-e", "lnet:172.18.50.40@o2ib1:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_args_bad_cmd[] = { "m0d", "-D", "cs_sdb",
                                "-S", "cs_stob", "-A", "linuxstob:cs_addb_sdb",
				"-w", "10",
                                "-e", "lnet:172.18.50.40@o2ib1:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_buffer_pool_cmd[] = { "m0d", "-T", "linux",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob", "-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-H", SERVER_ENDPOINT_ADDR,
                                /*
				 * -m is temporary set to 32768.
				 * It's required to handle m0_ha_msg transfer.
				 */
                                "-q", "4", "-m", "32768",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_lnet_cmd[] = { "m0d", "-T", "linux",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_sdb",
				"-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-H", SERVER_ENDPOINT_ADDR,
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_lnet_mult_if_cmd[] = { "m0d", "-T", "linux",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
				"-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-e", "lnet:172.18.50.40@tcp:12345:30:101",
                                "-e", "lnet:172.18.50.40@o2ib0:12345:34:101",
                                "-H", SERVER_ENDPOINT_ADDR,
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

/** @todo Remove passing of multiple endpoints to m0d, as is not needed. */

static char *cs_ut_ep_mixed_dup_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
				"-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-e", "lnet:172.18.50.40@tcp:12345:30:101",
                                "-e", "lnet:172.18.50.40@o2ib0:12345:34:101",
                                "-e", "lnet:172.18.50.40@o2ib1:12345:30:101",
                                "-e", "lnet:172.18.50.40@o2ib1:12345:30:101",
                                "-H", SERVER_ENDPOINT_ADDR,
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_lnet_dup_tcp_if_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
				"-w", "10",
                                "-e", SERVER_ENDPOINT,
                                "-e", "lnet:172.18.50.40@tcp:12345:30:101",
                                "-e", "lnet:172.18.50.40@tcp:12345:32:105",
                                "-H", SERVER_ENDPOINT_ADDR,
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *cs_ut_lnet_ep_bad_cmd[] = { "m0d", "-T", "AD",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
				"-w", "10",
                                "-e", "lnet:asdad:asdsd:sadasd",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static const char *cdbnames[] = { "cdb1", "cdb2" };
static const char *cl_ep_addrs[] = { "0@lo:12345:34:2", "127.0.0.1:34569" };
static const char *srv_ep_addrs[] = { SERVER_ENDPOINT_ADDR, "127.0.0.1:35678" };

/*
  Transports used in mero a context.
 */
static struct m0_net_xprt *cs_xprts[] = {
	&m0_net_lnet_xprt,
	&m0_net_bulk_mem_xprt
};

enum { MAX_RPCS_IN_FLIGHT = 10 };

#define SERVER_LOG_FILE_NAME "cs_ut.errlog"

static int cs_ut_client_init(struct cl_ctx *cctx, const char *cl_ep_addr,
			     const char *srv_ep_addr, const char* dbname,
			     struct m0_net_xprt *xprt)
{
	int                       rc;
	struct m0_rpc_client_ctx *cl_ctx;

	M0_PRE(cctx != NULL && cl_ep_addr != NULL && srv_ep_addr != NULL &&
	       dbname != NULL && xprt != NULL);

	rc = m0_net_domain_init(&cctx->cl_ndom, xprt);
	M0_UT_ASSERT(rc == 0);

	cl_ctx = &cctx->cl_ctx;

	cl_ctx->rcx_net_dom            = &cctx->cl_ndom;
	cl_ctx->rcx_local_addr         = cl_ep_addr;
	cl_ctx->rcx_remote_addr        = srv_ep_addr;
	cl_ctx->rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT;
	cl_ctx->rcx_fid                = &g_process_fid;

	rc = m0_rpc_client_start(cl_ctx);
	M0_UT_ASSERT(rc == 0);

	return rc;
}

static void cs_ut_client_fini(struct cl_ctx *cctx)
{
	int rc;

	rc = m0_rpc_client_stop(&cctx->cl_ctx);
	M0_UT_ASSERT(rc == 0);

	m0_net_domain_fini(&cctx->cl_ndom);
}

/** Sends fops to server. */
int m0_cs_ut_send_fops(struct m0_rpc_session *cl_rpc_session, int dstype)
{
	int                    rc;
        uint32_t               i;
        struct m0_fop         *fop[10] = { 0 };
	struct cs_ds1_req_fop *cs_ds1_fop;
	struct cs_ds2_req_fop *cs_ds2_fop;

	M0_PRE(cl_rpc_session != NULL && dstype > 0);

	switch (dstype) {
	case CS_UT_SERVICE1:
		for (i = 0; i < 10; ++i) {
			fop[i] = m0_fop_alloc_at(cl_rpc_session,
						 &cs_ds1_req_fop_fopt);
			cs_ds1_fop = m0_fop_data(fop[i]);
			cs_ds1_fop->csr_value = i;
			rc = m0_rpc_post_sync(fop[i], cl_rpc_session,
					      &cs_ds_req_fop_rpc_item_ops,
					      0 /* deadline */);
			M0_UT_ASSERT(rc == 0);
			m0_fop_put_lock(fop[i]);
		}
		break;
	case CS_UT_SERVICE2:
		for (i = 0; i < 10; ++i) {
			fop[i] = m0_fop_alloc_at(cl_rpc_session,
						 &cs_ds2_req_fop_fopt);
			cs_ds2_fop = m0_fop_data(fop[i]);
			cs_ds2_fop->csr_value = i;
			rc = m0_rpc_post_sync(fop[i], cl_rpc_session,
					      &cs_ds_req_fop_rpc_item_ops,
					      0 /* deadline */);
			M0_UT_ASSERT(rc == 0);
			m0_fop_put_lock(fop[i]);
		}
		break;
	default:
		M0_ASSERT("Invalid service type" == 0);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cs_ut_test_helper_success(struct cl_ctx *cctx, size_t cctx_nr,
				     char *cs_argv[], int cs_argc)
{
	int rc;
	int i;
	int stype;
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts         = cs_xprts,
		.rsx_xprts_nr      = ARRAY_SIZE(cs_xprts),
		.rsx_argv          = cs_argv,
		.rsx_argc          = cs_argc,
		.rsx_log_file_name = SERVER_LOG_FILE_NAME
	};

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < cctx_nr; ++i) {
		rc = cs_ut_client_init(&cctx[i], cl_ep_addrs[i],
					srv_ep_addrs[i], cdbnames[i],
					cs_xprts[i]);
		M0_UT_ASSERT(rc == 0);
	}

	stype = CS_UT_SERVICE1;
	for (i = 0; i < cctx_nr; ++i, ++stype)
		m0_cs_ut_send_fops(&cctx[i].cl_ctx.rcx_session, stype);

	for (i = 0; i < cctx_nr; ++i)
		cs_ut_client_fini(&cctx[i]);

	m0_rpc_server_stop(&sctx);

	return rc;
}

static void cs_ut_test_helper_failure(char *cs_argv[], int cs_argc)
{
	int rc;
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts         = cs_xprts,
		.rsx_xprts_nr      = ARRAY_SIZE(cs_xprts),
		.rsx_argv          = cs_argv,
		.rsx_argc          = cs_argc,
		.rsx_log_file_name = SERVER_LOG_FILE_NAME
	};

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc != 0);

	/*
	 * If, for some reason, m0_rpc_server_start() completed without error,
	 * we need to stop server.
	 */
	if (rc == 0)
		m0_rpc_server_stop(&sctx);
}

static void test_cs_ut_cs_start_err(void)
{
	m0_fi_enable_once("cs_level_enter", "pools_cleanup");
	cs_ut_test_helper_failure(cs_ut_service_one_cmd,
				  ARRAY_SIZE(cs_ut_service_one_cmd));
}

static void test_cs_ut_service_one(void)
{
	struct cl_ctx cctx[1] = {};

	cs_ut_test_helper_success(cctx, ARRAY_SIZE(cctx), cs_ut_service_one_cmd,
				  ARRAY_SIZE(cs_ut_service_one_cmd));
}

static void dev_conf_file_create(void)
{
	FILE *f;
	char  cwd[MAXPATHLEN];
	char *path;

	path = getcwd(cwd, ARRAY_SIZE(cwd));
	M0_UT_ASSERT(path != NULL);

	/* touch d1 d2 */
	f = fopen("d1", "w");
	M0_UT_ASSERT(f != NULL);
	fclose(f);
	f = fopen("d2", "w");
	M0_UT_ASSERT(f != NULL);
	fclose(f);
}

static void test_cs_ut_dev_stob(void)
{
	struct cl_ctx cctx[1] = {};

	dev_conf_file_create();
	cs_ut_test_helper_success(cctx, ARRAY_SIZE(cctx), cs_ut_dev_stob_cmd,
				  ARRAY_SIZE(cs_ut_dev_stob_cmd));
}

static void test_cs_ut_services_many(void)
{
	struct cl_ctx cctx[2] = {};

	cs_ut_test_helper_success(cctx, ARRAY_SIZE(cctx),
				  cs_ut_services_many_cmd,
				  ARRAY_SIZE(cs_ut_services_many_cmd));
}

static void test_cs_ut_opts_jumbled(void)
{
	struct cl_ctx cctx[1] = {};

	cs_ut_test_helper_success(cctx, ARRAY_SIZE(cctx),
				  cs_ut_opts_jumbled_cmd,
				  ARRAY_SIZE(cs_ut_opts_jumbled_cmd));
}

/** Tests m0d failure paths using fault injection. */
static void test_cs_ut_linux_stob_cleanup(void)
{
	dev_conf_file_create();
	m0_fi_enable_once("storage_dev_new", "ad_domain_locate_fail");
	cs_ut_test_helper_failure(cs_ut_dev_stob_cmd,
				  ARRAY_SIZE(cs_ut_dev_stob_cmd));
}

static void test_cs_ut_stype_bad(void)
{
	cs_ut_test_helper_failure(cs_ut_stype_bad_cmd,
				  ARRAY_SIZE(cs_ut_stype_bad_cmd));
}

static void test_cs_ut_xprt_bad(void)
{
	cs_ut_test_helper_failure(cs_ut_xprt_bad_cmd,
				  ARRAY_SIZE(cs_ut_xprt_bad_cmd));
}

static void test_cs_ut_ep_bad(void)
{
	cs_ut_test_helper_failure(cs_ut_ep_bad_cmd,
				  ARRAY_SIZE(cs_ut_ep_bad_cmd));
}

static void test_cs_ut_lnet_ep_bad(void)
{
	cs_ut_test_helper_failure(cs_ut_lnet_ep_bad_cmd,
				  ARRAY_SIZE(cs_ut_lnet_ep_bad_cmd));
}

static void test_cs_ut_lnet_ep_duplicate(void)
{
	/* Duplicate tcp interfaces in a request handler context. */
	cs_ut_test_helper_failure(cs_ut_lnet_dup_tcp_if_cmd,
				  ARRAY_SIZE(cs_ut_lnet_dup_tcp_if_cmd));
}

static void test_cs_ut_lnet_multiple_if(void)
{
	struct m0_mero mero_ctx = {};
	int            rc;

	rc = m0_cs_init(&mero_ctx, cs_xprts, ARRAY_SIZE(cs_xprts), stderr, true);
	M0_UT_ASSERT(rc == 0);

	rc = cs_args_parse(&mero_ctx, ARRAY_SIZE(cs_ut_lnet_mult_if_cmd),
			   cs_ut_lnet_mult_if_cmd);
	M0_UT_ASSERT(rc == 0);

	rc = cs_reqh_ctx_validate(&mero_ctx);
	M0_UT_ASSERT(rc == 0);

	m0_cs_fini(&mero_ctx);
}

static void test_cs_ut_lnet_ep_mixed_dup(void)
{
	struct m0_mero mero_ctx = {};
	FILE          *out;
	int            rc;

	out = fopen("temp", "w");

	rc = m0_cs_init(&mero_ctx, cs_xprts, ARRAY_SIZE(cs_xprts), out, true);
	M0_UT_ASSERT(rc == 0);

	rc = cs_args_parse(&mero_ctx, ARRAY_SIZE(cs_ut_ep_mixed_dup_cmd),
			   cs_ut_ep_mixed_dup_cmd);
	M0_UT_ASSERT(rc == 0);

	rc = cs_reqh_ctx_validate(&mero_ctx);
	M0_UT_ASSERT(rc != 0);

	m0_cs_fini(&mero_ctx);
	fclose(out);
}

static void test_cs_ut_service_bad(void)
{
	cs_ut_test_helper_failure(cs_ut_service_bad_cmd,
				  ARRAY_SIZE(cs_ut_service_bad_cmd));
}

static void test_cs_ut_args_bad(void)
{
	cs_ut_test_helper_failure(cs_ut_args_bad_cmd,
				  ARRAY_SIZE(cs_ut_args_bad_cmd));
}

static void test_cs_ut_buffer_pool(void)
{
	struct cl_ctx cctx[1] = {};

	cs_ut_test_helper_success(cctx, ARRAY_SIZE(cctx), cs_ut_buffer_pool_cmd,
				  ARRAY_SIZE(cs_ut_buffer_pool_cmd));
}

static void test_cs_ut_lnet(void)
{
	struct cl_ctx cctx[1] = {};

	cs_ut_test_helper_success(cctx, ARRAY_SIZE(cctx), cs_ut_lnet_cmd,
				  ARRAY_SIZE(cs_ut_lnet_cmd));
}

static void test_cs_ut_setup_fail(void)
{
	/* m0_pools_common_init() is to fail inside cs_conf_setup() */
	m0_fi_enable_once("m0_conf_devices_count", "diter_fail");
	cs_ut_test_helper_failure(cs_ut_service_one_cmd,
				  ARRAY_SIZE(cs_ut_service_one_cmd));
	/* m0_pools_setup() is to fail inside cs_conf_setup() */
	m0_fi_enable_once("m0_pools_setup", "diter_fail");
	cs_ut_test_helper_failure(cs_ut_service_one_cmd,
				  ARRAY_SIZE(cs_ut_service_one_cmd));

	/* cs_conf_services_init() is to fail inside cs_conf_setup() */
	m0_fi_enable_once("cs_conf_services_init", "diter_fail");
	cs_ut_test_helper_failure(cs_ut_service_one_cmd,
				  ARRAY_SIZE(cs_ut_service_one_cmd));
}

static void test_cs_ut_rconfc_fail(void)
{
	/* Standalone confc instance the dummy HA to work on */
	M0_INTERNAL struct m0_confc *m0_ha_entrypoint_confc_override(void);
	/*
	 * Server configuration making no local conf available. Therefore, mero
	 * instance is to request entrypoint info during rconfc start.
	 */
	static char *setup_conf[] = { "m0d", "-T", "linux",
				      "-D", "cs_sdb", "-S", "cs_stob",
				      "-A", "linuxstob:cs_addb_stob",
				      "-e", SERVER_ENDPOINT,
				      "-H", SERVER_ENDPOINT_ADDR,
				      "-w", "10" };
	struct m0_confc *confc = m0_ha_entrypoint_confc_override();
	char            *confstr;
	int              rc;

	/*
	 * We have to let dummy HA respond with entrypoint info working on a
	 * standalone confc instead of the one returned by m0_reqh2confc(). So
	 * need to pre-load it with standard configuration beforehand.
	 */
	rc = m0_file_read(M0_UT_PATH("conf.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(confc, m0_locality0_get()->lo_grp, NULL, NULL,
			   confstr);
	M0_UT_ASSERT(rc == 0);
	m0_free0(&confstr);
	/* Now wire it up */
	m0_fi_enable("mero_ha_entrypoint_request_cb", "ut_confc");
	/*
	 * m0_rconfc_start_sync() is to fail inside cs_conf_setup() because
	 * dummy HA breaks rconfc operation by reporting no RMS.
	 */
	m0_fi_enable("mero_ha_entrypoint_rep_rm_fill", "no_rms_fid");
	cs_ut_test_helper_failure(setup_conf, ARRAY_SIZE(setup_conf));

	m0_confc_fini(confc);
	m0_fi_disable("mero_ha_entrypoint_rep_rm_fill", "no_rms_fid");
	m0_fi_disable("mero_ha_entrypoint_request_cb", "ut_confc");
}

extern volatile sig_atomic_t gotsignal;

static void cs_ut_term_sig_handler(int signum)
{
	gotsignal = signum;
}

static int cs_ut_register_signal(void)
{
	struct sigaction term_act;
	int              rc;

	gotsignal = 0;
	term_act.sa_handler = cs_ut_term_sig_handler;
	sigemptyset(&term_act.sa_mask);
	term_act.sa_flags = 0;

	rc = sigaction(SIGUSR2, &term_act, NULL);
	M0_UT_ASSERT(rc == 0);
	return rc;
}

static struct m0_rpc_machine *cs_ut_reqh2rmach(struct m0_reqh *reqh)
{
	struct m0_rpc_machine *rmach;

	rmach = m0_reqh_rpc_mach_tlist_head(&reqh->rh_rpc_machines);
	M0_UT_ASSERT(rmach != NULL);
	return rmach;
}

static void cs_ut_write_lock_trigger(struct m0_reqh *reqh)
{
	int rc = rm_write_lock_get(cs_ut_reqh2rmach(reqh),
				   SERVER_ENDPOINT_ADDR);
	M0_UT_ASSERT(rc == 0);
	sleep(2);
	rm_write_lock_put();
}

/**
 * Test fatal signal delivery.
 *
 * Standard m0_rpc_server_start() invokes mero instance running confd, RM and
 * dummy HA. A standalone rconfc is launched and wired to standard
 * cs_rconfc_fatal_cb() (borrowed from server already running). Write lock
 * triggering makes rconfc cancel its read lock. On next read lock arrival fault
 * injection makes rconfc fail, and therefore, rise signal via
 * cs_rconfc_fatal_cb().
 */
static void test_cs_ut_rconfc_fatal(void)
{
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts         = cs_xprts,
		.rsx_xprts_nr      = ARRAY_SIZE(cs_xprts),
		.rsx_argv          = cs_ut_service_one_cmd,
		.rsx_argc          = ARRAY_SIZE(cs_ut_service_one_cmd),
		.rsx_log_file_name = SERVER_LOG_FILE_NAME
	};
	struct m0_reqh *reqh = &sctx.rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	struct m0_rconfc       rconfc;
	int                    rc;

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);
	/* Prepare and launch standalone rconfc */
	rc = m0_rconfc_init(&rconfc, m0_reqh2profile(reqh),
			    m0_locality0_get()->lo_grp,
			    cs_ut_reqh2rmach(reqh), NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	/*
	 * Register SIGUSR2 handler. The signal is to be sent by
	 * cs_rconfc_fatal_cb() and caught by cs_ut_term_sig_handler().
	 */
	rc = cs_ut_register_signal();
	M0_UT_ASSERT(gotsignal == 0);
	M0_UT_ASSERT(rc == 0);
	/* Install fatal callback to the rconfc instance. */
	m0_rconfc_lock(&rconfc);
	m0_rconfc_fatal_cb_set(&rconfc, reqh->rh_rconfc.rc_fatal_cb);
	m0_rconfc_unlock(&rconfc);
	/* Make sure read lock go and come does not cause any issue. */
	cs_ut_write_lock_trigger(reqh);
	sleep(1);
	M0_UT_ASSERT(gotsignal == 0);
	/* Imitate issue with read lock acquisition. */
	m0_fi_enable_once("cs_rconfc_fatal_cb", "ut_signal");
	m0_fi_enable_once("rconfc_read_lock_complete", "rlock_req_failed");
	cs_ut_write_lock_trigger(reqh);
	sleep(1);
	/* Make sure SIGUSR2 has been got. */
	M0_UT_ASSERT(gotsignal == SIGUSR2);
	/* Make sure failed rconfc is able to stop regular way. */
	M0_UT_ASSERT(rconfc.rc_sm.sm_state == M0_RCS_FAILURE);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	m0_rpc_server_stop(&sctx);
}

static void test_cs_ut_iem(void)
{
	M0_MERO_IEM(M0_MERO_IEM_SEVERITY_A_ALERT, M0_MERO_IEM_MODULE_IO,
		M0_MERO_IEM_EVENT_TEST);

	M0_MERO_IEM_DESC(M0_MERO_IEM_SEVERITY_A_ALERT, M0_MERO_IEM_MODULE_IO,
		M0_MERO_IEM_EVENT_TEST,
		"DUMMY Description, Dummy number[%d], Dummy text [%s]",
		__LINE__, __FUNCTION__);

	M0_MERO_IEM_DESC(M0_MERO_IEM_SEVERITY_X_CRITICAL, M0_MERO_IEM_MODULE_OS,
		M0_MERO_IEM_EVENT_IOQ,
		"DUMMY Description, Dummy number[%d], Dummy text [%s]",
		__LINE__, __FUNCTION__);

	M0_MERO_IEM_DESC(M0_MERO_IEM_SEVERITY_E_ERROR, M0_MERO_IEM_MODULE_IO,
		M0_MERO_IEM_EVENT_IOQ,
		"DUMMY Description, Dummy number[%d], Dummy text [%s]",
		__LINE__, __FUNCTION__);

	M0_MERO_IEM_DESC(M0_MERO_IEM_SEVERITY_I_INFORMATIONAL, M0_MERO_IEM_MODULE_OS,
		M0_MERO_IEM_EVENT_FREE_SPACE,
		"DUMMY Description, Dummy number[%d], Dummy text [%s]",
		__LINE__, __FUNCTION__);

	M0_MERO_IEM_DESC(M0_MERO_IEM_SEVERITY_B_DEBUG, M0_MERO_IEM_MODULE_IO,
		M0_MERO_IEM_EVENT_FREE_SPACE,
		"DUMMY Description, Dummy number[%d], Dummy text [%s]",
		__LINE__, __FUNCTION__);
}

struct m0_ut_suite m0d_ut = {
        .ts_name = "m0d-ut",
        .ts_tests = {
                { "cs-single-service", test_cs_ut_service_one},
		{ "cs-multiple-services", test_cs_ut_services_many},
		{ "cs-command-options-jumbled", test_cs_ut_opts_jumbled},
		{ "cs-device-stob", test_cs_ut_dev_stob},
		{ "cs-fail-linux-stob-cleanup", test_cs_ut_linux_stob_cleanup},
		{ "cs-bad-storage-type", test_cs_ut_stype_bad},
		{ "cs-bad-network-xprt", test_cs_ut_xprt_bad},
		{ "cs-bad-network-ep", test_cs_ut_ep_bad},
		{ "cs-bad-service", test_cs_ut_service_bad},
		{ "cs-missing-options", test_cs_ut_args_bad},
		{ "cs-buffer_pool-options", test_cs_ut_buffer_pool},
		{ "cs-bad-lnet-ep", test_cs_ut_lnet_ep_bad},
		{ "cs-duplicate-lnet-ep", test_cs_ut_lnet_ep_duplicate},
		{ "cs-duplicate-lnet-mixed-ep", test_cs_ut_lnet_ep_mixed_dup},
		{ "cs-lnet-multiple-interfaces", test_cs_ut_lnet_multiple_if},
		{ "cs-lnet-options", test_cs_ut_lnet},
		{ "cs-setup-fail", test_cs_ut_setup_fail},
		{ "cs-rconfc-fail", test_cs_ut_rconfc_fail},
		{ "cs-rconfc-fatal", test_cs_ut_rconfc_fatal},
		{ "cs-start-err", test_cs_ut_cs_start_err},
		{ "cs-iem", test_cs_ut_iem},
		{ NULL, NULL },
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
