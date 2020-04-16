/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 01-Dec-2012
 */

#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "rpc/rpc.h"
#include "ut/ut.h"

static struct m0_reqh            g_reqh;
static struct m0_net_domain      g_net_dom;
static struct m0_net_buffer_pool g_buf_pool;

M0_INTERNAL int m0_ut_rpc_service_start(struct m0_reqh_service **service,
					const struct m0_reqh_service_type *type)
{
	int rc;

	rc = m0_reqh_service_allocate(service, type,  NULL);
	if (rc != 0)
		return rc;
	m0_reqh_service_init(*service, &g_reqh, NULL);
	m0_reqh_service_start(*service);
	return 0;
}

/* XXX Code duplication! See m0_ha_ut_rpc_ctx_init(). */
M0_INTERNAL int m0_ut_rpc_machine_start(struct m0_rpc_machine *mach,
					struct m0_net_xprt *xprt,
					const char *ep_addr)
{
	enum { NR_TMS = 1 };
	int rc;

	M0_SET0(&g_reqh);
	M0_SET0(&g_net_dom);
	M0_SET0(&g_buf_pool);
	rc = m0_net_domain_init(&g_net_dom, xprt);
	if (rc != 0)
		return rc;

	rc = m0_rpc_net_buffer_pool_setup(&g_net_dom, &g_buf_pool,
					  m0_rpc_bufs_nr(
						  M0_NET_TM_RECV_QUEUE_DEF_LEN,
						  NR_TMS),
					  NR_TMS);
	if (rc != 0)
		goto net;

	rc = M0_REQH_INIT(&g_reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &g_process_fid,
		);
	if (rc != 0)
		goto buf_pool;
	m0_reqh_start(&g_reqh);

	rc = m0_rpc_machine_init(mach, &g_net_dom, ep_addr, &g_reqh,
				 &g_buf_pool, M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	if (rc == 0) {
		M0_POST(mach->rm_tm.ntm_dom->nd_xprt == xprt);
		return 0;
	}
buf_pool:
	m0_rpc_net_buffer_pool_cleanup(&g_buf_pool);
net:
	m0_net_domain_fini(&g_net_dom);
	return rc;
}

/* XXX Code duplication! See m0_ha_ut_rpc_ctx_fini(). */
M0_INTERNAL void m0_ut_rpc_machine_stop(struct m0_rpc_machine *mach)
{
	m0_reqh_shutdown_wait(&g_reqh);
	m0_rpc_machine_fini(mach);
	m0_reqh_services_terminate(&g_reqh);
	m0_reqh_fini(&g_reqh);
	m0_rpc_net_buffer_pool_cleanup(&g_buf_pool);
	m0_net_domain_fini(&g_net_dom);
}
