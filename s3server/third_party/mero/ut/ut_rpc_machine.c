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
 * Original creation date: 18-Apr-2013
 */

#include "ut/ut.h"
#include "ut/be.h"
#include "ut/ut_rpc_machine.h"

static void ut_reqh_and_stuff_init(struct m0_ut_rpc_mach_ctx *ctx);

static void buf_dummy(struct m0_net_buffer_pool *bp)
{
}

static const struct m0_net_buffer_pool_ops buf_ops = {
	.nbpo_below_threshold = buf_dummy,
	.nbpo_not_empty       = buf_dummy
};

M0_INTERNAL void m0_ut_rpc_mach_init_and_add(struct m0_ut_rpc_mach_ctx *ctx)
{
	enum { UT_BUF_NR = 8, UT_TM_NR = 2 };
	int rc;

	ctx->rmc_xprt = &m0_net_lnet_xprt;
	rc = m0_net_domain_init(&ctx->rmc_net_dom, ctx->rmc_xprt);
	M0_ASSERT(rc == 0);

	ctx->rmc_bufpool.nbp_ops = &buf_ops;
	rc = m0_rpc_net_buffer_pool_setup(&ctx->rmc_net_dom, &ctx->rmc_bufpool,
					  UT_BUF_NR, UT_TM_NR);
	M0_ASSERT(rc == 0);

	ut_reqh_and_stuff_init(ctx);

	m0_reqh_start(&ctx->rmc_reqh);
	rc = m0_rpc_machine_init(&ctx->rmc_rpc, &ctx->rmc_net_dom,
				 ctx->rmc_ep_addr, &ctx->rmc_reqh,
				 &ctx->rmc_bufpool, M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_ASSERT(rc == 0);
}

#ifndef __KERNEL__

static char *ut_reqh_location_get(void)
{
	static const size_t  str_len = 100;
	char		    *str = m0_alloc(str_len);
	static uint64_t	     start = 10000;

	M0_ASSERT(str != NULL);
	snprintf(str, str_len, "linuxstob:./ut_reqh%"PRIu64, start++);
	return str;
}

static void ut_reqh_and_stuff_init(struct m0_ut_rpc_mach_ctx *ctx)
{
	struct m0_sm_group     *grp;
	struct m0_be_seg       *seg;
	struct m0_be_tx		tx;
	struct m0_be_tx_credit	cred = {};
	char		       *location;
	int			rc;
	/*
	 * Instead of using m0d and dealing with network, database and
	 * other subsystems, request handler is initialised in a 'special way'.
	 * This allows it to operate in a 'limited mode' which is enough for
	 * this test.
	 */
	rc = M0_REQH_INIT(&ctx->rmc_reqh,
			  .rhia_mdstore = &ctx->rmc_mdstore,
			  .rhia_fid     = &g_process_fid,
		);
	M0_ASSERT(rc == 0);

	ctx->rmc_ut_be.but_dom_cfg.bc_engine.bec_reqh = &ctx->rmc_reqh;
	location = ut_reqh_location_get();
	ctx->rmc_ut_be.but_stob_domain_location = location;
	rc = m0_be_ut_backend_init_cfg(&ctx->rmc_ut_be, NULL, true);
	M0_ASSERT(rc == 0);
	m0_be_ut_seg_init(&ctx->rmc_ut_seg, &ctx->rmc_ut_be, 1 << 20);
	seg = ctx->rmc_ut_seg.bus_seg;
	grp = m0_be_ut_backend_sm_group_lookup(&ctx->rmc_ut_be);

	rc = m0_reqh_be_init(&ctx->rmc_reqh, seg);
	M0_ASSERT(rc == 0);

	rc = m0_mdstore_init(&ctx->rmc_mdstore, seg, 0);
	M0_ASSERT(rc == -ENOENT);
	rc = m0_mdstore_create(&ctx->rmc_mdstore, grp, &ctx->rmc_cob_id,
			       &ctx->rmc_ut_be.but_dom, seg);
        M0_ASSERT(rc == 0);

	m0_cob_tx_credit(ctx->rmc_mdstore.md_dom, M0_COB_OP_DOMAIN_MKFS,
			 &cred);
	m0_ut_be_tx_begin(&tx, &ctx->rmc_ut_be, &cred);
	rc = m0_cob_domain_mkfs(ctx->rmc_mdstore.md_dom,
				&M0_MDSERVICE_SLASH_FID, &tx);
	m0_ut_be_tx_end(&tx);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ut_rpc_mach_fini(struct m0_ut_rpc_mach_ctx *ctx)
{
	struct m0_sm_group *grp;
	int                 rc;

	m0_reqh_idle_wait(&ctx->rmc_reqh);
	m0_reqh_pre_storage_fini_svcs_stop(&ctx->rmc_reqh);
	M0_ASSERT(m0_reqh_state_get(&ctx->rmc_reqh) == M0_REQH_ST_STOPPED);
	grp = m0_be_ut_backend_sm_group_lookup(&ctx->rmc_ut_be);
	rc = m0_mdstore_destroy(&ctx->rmc_mdstore, grp,
				&ctx->rmc_ut_be.but_dom);
	M0_ASSERT(rc == 0);

	m0_be_ut_seg_fini(&ctx->rmc_ut_seg);
	m0_be_ut_backend_fini(&ctx->rmc_ut_be);
	m0_be_domain_cleanup_by_location(
		 ctx->rmc_ut_be.but_stob_domain_location);
	m0_free(ctx->rmc_ut_be.but_stob_domain_location);
	m0_reqh_post_storage_fini_svcs_stop(&ctx->rmc_reqh);
	m0_rpc_machine_fini(&ctx->rmc_rpc);
	m0_reqh_fini(&ctx->rmc_reqh);

	m0_rpc_net_buffer_pool_cleanup(&ctx->rmc_bufpool);
	m0_net_domain_fini(&ctx->rmc_net_dom);
}

#else /* __KERNEL__ */

static void ut_reqh_and_stuff_init(struct m0_ut_rpc_mach_ctx *ctx)
{
	int rc;
	/*
	 * Instead of using m0d and dealing with network, database and
	 * other subsystems, request handler is initialised in a 'special way'.
	 * This allows it to operate in a 'limited mode' which is enough for
	 * this test.
	 */
	rc = M0_REQH_INIT(&ctx->rmc_reqh,
			  .rhia_dtm     = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_fid     = &g_process_fid,
		);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ut_rpc_mach_fini(struct m0_ut_rpc_mach_ctx *ctx)
{
	m0_reqh_services_terminate(&ctx->rmc_reqh);
	m0_rpc_machine_fini(&ctx->rmc_rpc);
	m0_reqh_fini(&ctx->rmc_reqh);
}

#endif

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
