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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 16-Mar-2015
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/memory.h"        /* M0_ALLOC_PTR */
#include "lib/errno.h"
#include "lib/locality.h"      /* m0_locality0_get */
#include "lib/misc.h"          /* M0_QUOTE */
#include "lib/string.h"
#include "lib/finject.h"
#include "ut/ut.h"
#include "conf/confd_stob.h"
#include "conf/flip_fom.h"
#include "conf/flip_fop.h"
#include "conf/load_fom.h"
#include "conf/load_fop.h"
#include "reqh/reqh.h"         /* m0_reqh */
#include "fop/fop.h"           /* m0_fop */
#include "fop/fom.h"           /* m0_fom */
#include "rpc/rpc_machine.h"   /* m0_rpc_machine */
#include "sm/sm.h"
#include "spiel/ut/spiel_ut_common.h"

#define STR_LEN 1000

/**
 * Request handler context with all necessary structures.
 *
 */
struct m0_conf_ut_reqh {
	struct m0_net_domain      cur_net_dom;
	struct m0_net_buffer_pool cur_buf_pool;
	struct m0_reqh            cur_reqh;
	struct m0_rpc_machine     cur_rmachine;
};

struct m0_conf_ut_reqh  *conf_reqh;


extern const struct m0_fom_ops conf_load_fom_ops;
extern const struct m0_fom_ops conf_flip_fom_ops;

static void conf_fop_ut_release(struct m0_ref *ref)
{
	struct m0_fop *fop;

	fop = M0_AMB(fop, ref, f_ref);
	m0_free(fop);
}

/**
 * load-fom-create-fail
 *
 * Test fail create Load FOM
 */
static void conf_load_fom_create_fail(void)
{
	int             rc;
	struct m0_fop  *fop;
	struct m0_fom  *fom;
	struct m0_reqh *reqh;

	M0_ALLOC_PTR(fop);
	M0_UT_ASSERT(fop != NULL);
	M0_ALLOC_PTR(fom);
	M0_UT_ASSERT(fom != NULL);
	M0_ALLOC_PTR(reqh);
	M0_UT_ASSERT(reqh != NULL);

	m0_fop_init(fop, &m0_fop_conf_load_fopt, NULL,
		    conf_fop_ut_release);
	fop->f_item.ri_rmachine = &conf_reqh->cur_rmachine;

	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_conf_load_fom_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 1, 1);
	rc = m0_conf_load_fom_create(fop, &fom, reqh);
	m0_fi_disable("m0_alloc", "fail_allocation");
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_free(fop);
	m0_free(fom);
	m0_free(reqh);
}

/**
 * flip-fom-create-fail
 *
 * Test fail create Flip FOM
 */
static void conf_flip_fom_create_fail(void)
{
	int             rc;
	struct m0_fop  *fop;
	struct m0_fom  *fom;
	struct m0_reqh *reqh;

	M0_ALLOC_PTR(fop);
	M0_ALLOC_PTR(fom);
	M0_ALLOC_PTR(reqh);

	m0_fop_init(fop, &m0_fop_conf_flip_fopt, NULL,
		    conf_fop_ut_release);
	fop->f_item.ri_rmachine = &conf_reqh->cur_rmachine;

	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_conf_flip_fom_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 1, 1);
	rc = m0_conf_flip_fom_create(fop, &fom, reqh);
	m0_fi_disable("m0_alloc", "fail_allocation");
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_free(fop);
	m0_free(fom);
	m0_free(reqh);
}

/**
 * confd-save-load test
 *
 * Test to save and load confd configure file with confd STOB
 */
static void conf_save_load(void)
{
	struct m0_stob *stob;
	char            linux_location[] = "linuxstob:./__s";
	struct m0_fid   confd_fid = M0_CONFD_FID(1, 2, 12345);
	int             rc;
	char           *str_write;
	char           *str_read;
	int             i;

	rc = m0_confd_stob_init(&stob, linux_location, &confd_fid);
	M0_UT_ASSERT(rc == 0);

	str_write = m0_alloc_aligned(STR_LEN, m0_stob_block_shift(stob));
	for (i = 0; i < STR_LEN; ++i)
		str_write[i] = '0' + (i % 10);
	str_write[STR_LEN - 1] = 0;

	rc = m0_confd_stob_write(stob, str_write);
	M0_UT_ASSERT(rc == 0);

	rc = m0_confd_stob_read(stob, &str_read);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(str_write, str_read) == 0);

	m0_free_aligned(str_read, STR_LEN, m0_stob_block_shift(stob));
	m0_free_aligned(str_write, STR_LEN, m0_stob_block_shift(stob));

	rc = m0_stob_domain_destroy_location(linux_location);
	M0_UT_ASSERT(rc == 0);

	m0_confd_stob_fini(stob);
}

/**
 * confd-save-load-fail test
 *
 * Test to save and load confd configure file with confd STOB
 */
static void conf_save_load_fail(void)
{
	struct m0_stob *stob;
	char            linux_location[] = "linuxstob:./__s";
	char            fail_location[] = "fail_location";
	struct m0_fid   confd_fid = M0_CONFD_FID(1, 2, 12345);
	int             rc;
	char           *str_write;
	char           *str_read;
	int             i;

	/* Test create fail*/
	rc = m0_confd_stob_init(&stob, fail_location, &confd_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_confd_stob_init(&stob, linux_location, &confd_fid);
	M0_UT_ASSERT(rc == 0);

	/* test save fail */
	str_write = m0_alloc_aligned(STR_LEN, m0_stob_block_shift(stob));
	for (i = 0; i < STR_LEN; ++i)
		str_write[i] = '0' + (i % 10);
	str_write[STR_LEN - 1] = 0;

	rc = m0_confd_stob_write(stob, str_write);
	M0_UT_ASSERT(rc == 0);

	/* test load fail */
	m0_fi_enable_once("m0_alloc_aligned", "fail_allocation");
	rc = m0_confd_stob_read(stob, &str_read);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 0, 1);
	rc = m0_confd_stob_read(stob, &str_read);
	m0_fi_disable("m0_alloc", "fail_allocation");
	M0_UT_ASSERT(rc == -ENOMEM);

	rc = m0_confd_stob_read(stob, &str_read);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strcmp(str_write, str_read) == 0);

	/* Finish */
	m0_free_aligned(str_read, STR_LEN, m0_stob_block_shift(stob));
	m0_free_aligned(str_write, STR_LEN, m0_stob_block_shift(stob));

	rc = m0_stob_domain_destroy_location(linux_location);
	M0_UT_ASSERT(rc == 0);

	m0_confd_stob_fini(stob);
}

/**
 * Init reqh, rpc_machine, etc for tests
 */
static int conf_ut_reqh_init(struct m0_conf_ut_reqh *conf_reqh,
			     const char             *ep_addr)
{
	struct m0_net_xprt *xprt = &m0_net_lnet_xprt;
	enum { NR_TMS = 1 };
	int rc;

	M0_SET0(conf_reqh);
	rc = m0_net_domain_init(&conf_reqh->cur_net_dom, xprt);
	if (rc != 0)
		return rc;

	rc = m0_rpc_net_buffer_pool_setup(&conf_reqh->cur_net_dom,
					  &conf_reqh->cur_buf_pool,
					  m0_rpc_bufs_nr(
					     M0_NET_TM_RECV_QUEUE_DEF_LEN,
					     NR_TMS),
					  NR_TMS);
	if (rc != 0)
		goto net;

	rc = M0_REQH_INIT(&conf_reqh->cur_reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &g_process_fid);
	if (rc != 0)
		goto buf_pool;
	m0_reqh_start(&conf_reqh->cur_reqh);

	rc = m0_rpc_machine_init(&conf_reqh->cur_rmachine,
				 &conf_reqh->cur_net_dom, ep_addr,
				 &conf_reqh->cur_reqh, &conf_reqh->cur_buf_pool,
				 M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);

	if (rc == 0) {
		return 0;
	}
buf_pool:
	m0_rpc_net_buffer_pool_cleanup(&conf_reqh->cur_buf_pool);
net:
	m0_net_domain_fini(&conf_reqh->cur_net_dom);
	return rc;
}

/**
 * Finit reqh, rpc_machine, ect for tests
 */
static void conf_ut_reqh_fini(struct m0_conf_ut_reqh *conf_reqh)
{
	m0_reqh_services_terminate(&conf_reqh->cur_reqh);
	m0_rpc_machine_fini(&conf_reqh->cur_rmachine);
	m0_reqh_fini(&conf_reqh->cur_reqh);
	m0_rpc_net_buffer_pool_cleanup(&conf_reqh->cur_buf_pool);
	m0_net_domain_fini(&conf_reqh->cur_net_dom);
}

/**
 * spiel-conf-init
 */
static int conf_load_ut_init()
{
	int         rc;
	const char *ep = SERVER_ENDPOINT_ADDR;

	M0_ALLOC_PTR(conf_reqh);
	rc = conf_ut_reqh_init(conf_reqh, ep);
	M0_UT_ASSERT(rc == 0);
	return 0;
}

/**
 *  spiel-conf-init
 */
static int conf_load_ut_fini()
{
	conf_ut_reqh_fini(conf_reqh);
	m0_free(conf_reqh);
	return 0;
}

struct m0_ut_suite conf_load_ut = {
	.ts_name = "conf-load-ut",
	.ts_init = conf_load_ut_init,
	.ts_fini = conf_load_ut_fini,
	.ts_tests = {
		{ "load-fom-create-fail",  conf_load_fom_create_fail   },
		{ "flip-fom-create-fail",  conf_flip_fom_create_fail   },
		{ "confd-save-load",       conf_save_load              },
		{ "confd-save-load-fail",  conf_save_load_fail         },
		{ NULL, NULL },
	},
};
M0_EXPORTED(conf_load_ut);

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
