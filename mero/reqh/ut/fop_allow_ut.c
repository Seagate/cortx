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
 * Original author: Rajanikant Chirmade <Rajnaikant_Chirmade@xyratex.com>
 * Original creation date: 28-July-2014
 */

#include "reqh/ut/service_xc.h"      /* m0_xc_reqh_ut_service_init */
#include "rpc/rpc_opcodes.h"         /* M0_REQH_UT_ALLOW_OPCODE */
#include "sss/ss_fops.h"             /* m0_sss_req */
#include "ut/ut.h"

#include "rpc/ut/clnt_srv_ctx.c"
#include "reqh/ut/reqhut_fom.c"

static int fom_tick(struct m0_fom *fom);

static char *ut_server_argv[] = {
	"rpclib_ut", "-T", "AD", "-D", SERVER_DB_NAME,
	"-f", M0_UT_CONF_PROCESS,
	"-S", SERVER_STOB_NAME, "-A", SERVER_ADDB_STOB_NAME,
	"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
	"-c", M0_UT_PATH("conf.xc")
};

struct m0_reqh_service_type *ut_stypes[] = {
	&ds1_service_type,
};

static const struct m0_fid ut_fid = {
	.f_container = 0x7300000000000001,
	.f_key       = 26
};

static const struct m0_fom_ops ut_fom_ops = {
	.fo_fini = reqhut_fom_fini,
	.fo_tick = fom_tick,
	.fo_home_locality = reqhut_find_fom_home_locality
};

static int fom_create(struct m0_fop  *fop,
		      struct m0_fom **out,
		      struct m0_reqh *reqh)
{
	struct m0_fom *fom;
	struct m0_fop *rfop;

	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);

	M0_ALLOC_PTR(fom);
	if (fom == NULL)
		return -ENOMEM;

	rfop = m0_fop_reply_alloc(fop, &m0_fop_generic_reply_fopt);
	M0_UT_ASSERT(rfop != NULL);

	m0_fom_init(fom, &fop->f_type->ft_fom_type, &ut_fom_ops,
		    fop, rfop, reqh);

	*out = fom;

	return 0;
}

static const struct m0_fom_type_ops ut_fom_type_ops = {
	.fto_create = &fom_create
};

static struct m0_fop_type m0_reqhut_allow_fopt;

static int m0_reqhut_fop_init(void)
{
	m0_xc_reqh_ut_service_init();
	m0_reqhut_dummy_xc->xct_flags = M0_XCODE_TYPE_FLAG_DOM_RPC;
	M0_FOP_TYPE_INIT(&m0_reqhut_allow_fopt,
			 .name      = "Reqh unit test",
			 .opcode    = M0_REQH_UT_ALLOW_OPCODE,
			 .xt        = m0_reqhut_dummy_xc,
			 .fom_ops   = &ut_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .svc_type  = &ds1_service_type);
	return 0;
}

static void m0_reqhut_fop_fini(void)
{
	m0_fop_type_fini(&m0_reqhut_allow_fopt);
	m0_xc_reqh_ut_service_fini();
}

static int fom_tick(struct m0_fom *fom)
{
	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static int send_fop()
{
	struct m0_fop *fop;
	int            rc;

	fop = m0_fop_alloc(&m0_reqhut_allow_fopt, NULL, &cctx.rcx_rpc_machine);
	M0_UT_ASSERT(fop != NULL);

	rc = m0_rpc_post_sync(fop, &cctx.rcx_session, NULL, 0);
	m0_fop_put_lock(fop);

	return rc;
}

static struct m0_fop *ut_ssfop_alloc(const char *name, uint32_t cmd)
{
	struct m0_fop     *fop;
	struct m0_sss_req *ss_fop;

	M0_ALLOC_PTR(fop);
	M0_UT_ASSERT(fop != NULL);

	M0_ALLOC_PTR(ss_fop);
	M0_UT_ASSERT(ss_fop != NULL);

	m0_buf_init(&ss_fop->ss_name, (void *)name, strlen(name));
	ss_fop->ss_cmd = cmd;
	ss_fop->ss_id  = ut_fid;

	m0_fop_init(fop, &m0_fop_ss_fopt, (void *)ss_fop, m0_ss_fop_release);

	return fop;

}

static void ut_sss_req(const char *name, uint32_t cmd, int expected_rc,
		      int expected_state)
{
	int                 rc;
	struct m0_fop      *fop;
	struct m0_fop      *rfop;
	struct m0_rpc_item *item;
	struct m0_sss_rep  *ss_rfop;

	fop = ut_ssfop_alloc(name, cmd);
	item = &fop->f_item;

	rc = m0_rpc_post_sync(fop, &cctx.rcx_session, NULL, 0);
	M0_UT_ASSERT(rc == 0);

	rfop  = m0_rpc_item_to_fop(item->ri_reply);
	M0_UT_ASSERT(rfop != NULL);

	ss_rfop = m0_fop_data(rfop);
	M0_UT_ASSERT(ss_rfop->ssr_rc == expected_rc);
	if (expected_state != 0)
		M0_UT_ASSERT(ss_rfop->ssr_state == expected_state);
	m0_fop_put_lock(fop);
}

static void fop_allow_test(void)
{
	int rc;

	sctx.rsx_argv = ut_server_argv;
	sctx.rsx_argc = ARRAY_SIZE(ut_server_argv);

	rc = m0_reqhut_fop_init();
	M0_UT_ASSERT(rc == 0);

	start_rpc_client_and_server();
	ut_sss_req(ds1_service_type.rst_name, M0_SERVICE_STATUS, 0,
		   M0_RST_STARTED);
	ut_sss_req(ds1_service_type.rst_name, M0_SERVICE_QUIESCE, 0,
		   M0_RST_STOPPING);
	ut_sss_req(ds1_service_type.rst_name, M0_SERVICE_STOP, 0,
		   M0_RST_STOPPED);

	rc = send_fop();
	M0_UT_ASSERT(rc == -ECONNREFUSED);

	/* Test reposting of the fop in case of failure. */
	rc = send_fop();
	M0_UT_ASSERT(rc == -ECONNREFUSED);

	ut_sss_req(ds1_service_type.rst_name,
		   M0_SERVICE_INIT, 0,  M0_RST_INITIALISED);
	ut_sss_req(ds1_service_type.rst_name,
		   M0_SERVICE_START, 0,  M0_RST_STARTED);
	rc = send_fop();
	M0_UT_ASSERT(rc == 0);

	m0_reqhut_fop_fini();
	stop_rpc_client_and_server();
}

struct m0_ut_suite reqh_fop_allow_ut = {
	.ts_name  = "reqh-fop-allow-ut",
	.ts_tests = {
		{ "reqh-fop-allow", fop_allow_test },
		{ NULL, NULL }
	}
};
M0_EXPORTED(reqh_fop_allow_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
