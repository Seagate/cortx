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
 * Original author: Rajanikant Chirmade <Rajanikant_Chirmade@xyratex.com>
 * Original creation date: 01/06/2012
 */

#include "ioservice/io_service.c"
#include "net/bulk_mem.h"         /* m0_net_bulk_mem_xprt */
#include "ut/misc.h"              /* M0_UT_PATH */
#include "ut/ut.h"

extern const struct m0_tl_descr bufferpools_tl;

/* Mero setup arguments. */
static char *ios_ut_bp_singledom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-e", "lnet:0@lo:12345:34:1",
				"-H", "0@lo:12345:34:1",
				"-w", "10",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *ios_ut_bp_multidom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-w", "10",
				"-e", "lnet:0@lo:12345:34:1",
				"-e", "bulk-mem:127.0.0.1:35678",
				"-H", "0@lo:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *ios_ut_bp_repeatdom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-w", "10",
				"-e", "lnet:0@lo:12345:34:1",
				"-e", "bulk-mem:127.0.0.1:35678",
				"-e", "bulk-mem:127.0.0.1:35679",
				"-H", "0@lo:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

static char *ios_ut_bp_onerepeatdom_cmd[] = { "m0d", "-T", "AD",
				"-D", "cs_sdb", "-S", "cs_stob",
				"-A", "linuxstob:cs_addb_stob",
				"-w", "10",
				"-e", "lnet:0@lo:12345:34:1",
				"-e", "bulk-mem:127.0.0.1:35678",
				"-e", "bulk-mem:127.0.0.1:35679",
				"-H", "0@lo:12345:34:1",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_UT_PATH("conf.xc")};

/* Transports used in mero context. */
static struct m0_net_xprt *cs_xprts[] = {
	&m0_net_lnet_xprt,
	&m0_net_bulk_mem_xprt
};

#define SERVER_LOG_FILE_NAME "cs_ut.errlog"

static int get_ioservice_buffer_pool_count(struct m0_rpc_server_ctx *sctx)
{
	struct m0_reqh_io_service *serv_obj;
	struct m0_reqh_service    *reqh_ios;
	struct m0_reqh            *reqh;

	reqh     = m0_cs_reqh_get(&sctx->rsx_mero_ctx);
	reqh_ios = m0_reqh_service_find(&m0_ios_type, reqh);
	serv_obj = container_of(reqh_ios, struct m0_reqh_io_service, rios_gen);
	M0_UT_ASSERT(serv_obj != NULL);

	return bufferpools_tlist_length(&serv_obj->rios_buffer_pools);
}

static int check_buffer_pool_per_domain(char *cs_argv[], int cs_argc, int nbp)
{
	int rc;
	int bp_count;
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts            = cs_xprts,
		.rsx_xprts_nr         = ARRAY_SIZE(cs_xprts),
		.rsx_argv             = cs_argv,
		.rsx_argc             = cs_argc,
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
	};

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);

	bp_count = get_ioservice_buffer_pool_count(&sctx);
	M0_UT_ASSERT(bp_count == nbp);

	m0_rpc_server_stop(&sctx);
	return rc;
}

void test_ios_bp_single_dom()
{
	/* It will create single buffer pool (per domain)*/
	check_buffer_pool_per_domain(ios_ut_bp_singledom_cmd,
				     ARRAY_SIZE(ios_ut_bp_singledom_cmd), 1);
}

void test_ios_bp_multi_dom()
{
	/* It will create two buffer pool (per domain) */
	check_buffer_pool_per_domain(ios_ut_bp_multidom_cmd,
				     ARRAY_SIZE(ios_ut_bp_multidom_cmd), 2);
}

void test_ios_bp_repeat_dom()
{
	/* It will create single buffer pool (per domain) */
	check_buffer_pool_per_domain(ios_ut_bp_repeatdom_cmd,
				     ARRAY_SIZE(ios_ut_bp_repeatdom_cmd), 2);
}
void test_ios_bp_onerepeat_dom()
{
	/* It will create two buffer pool (per domain) */
	check_buffer_pool_per_domain(ios_ut_bp_onerepeatdom_cmd,
				     ARRAY_SIZE(ios_ut_bp_onerepeatdom_cmd), 2);
}

struct m0_ut_suite ios_bufferpool_ut = {
        .ts_name = "ios-bufferpool-ut",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "ios-bufferpool-single-domain", test_ios_bp_single_dom},
                { "ios-bufferpool-multiple-domains", test_ios_bp_multi_dom},
                { "ios-bufferpool-repeat-domains", test_ios_bp_repeat_dom},
                { "ios-bufferpool-onerepeat-domain", test_ios_bp_onerepeat_dom},
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
