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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 09/28/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#ifndef __KERNEL__
#  include <errno.h>  /* errno */
#  include <stdio.h>  /* fopen, fclose */
#endif

#include "lib/misc.h"
#include "lib/types.h"
#include "lib/memory.h"
#include "lib/assert.h"
#include "rpc/rpc.h"
#include "net/net.h"
#include "net/lnet/lnet.h"
#include "fop/fop.h"
#include "fop/fom_generic.h" /* m0_rpc_item_generic_reply_rc */
#include "rpc/rpclib.h"
#include "conf/helpers.h"    /* m0_conf_service_locate */

#ifndef __KERNEL__
#  include "reqh/reqh.h"
#  include "reqh/reqh_service.h"
#  include "mero/setup.h"
#endif

#ifndef __KERNEL__
int m0_rpc_server_start(struct m0_rpc_server_ctx *sctx)
{
	int rc;

	M0_ENTRY("server_ctx: %p", sctx);
	M0_PRE(sctx->rsx_argv != NULL && sctx->rsx_argc > 0);

	/* Open error log file */
	sctx->rsx_log_file = fopen(sctx->rsx_log_file_name, "w+");
	if (sctx->rsx_log_file == NULL)
		return M0_ERR_INFO(errno, "Open of error log file");

	rc = m0_cs_init(&sctx->rsx_mero_ctx, sctx->rsx_xprts,
			sctx->rsx_xprts_nr, sctx->rsx_log_file, true);
	M0_LOG(M0_DEBUG, "cs_init: rc=%d", rc);
	if (rc != 0)
		goto fclose;

	rc = m0_cs_setup_env(&sctx->rsx_mero_ctx, sctx->rsx_argc,
			     sctx->rsx_argv);
	M0_LOG(M0_DEBUG, "cs_setup_env: rc=%d", rc);
	if (rc != 0)
		goto error;

	rc = m0_cs_start(&sctx->rsx_mero_ctx);
	if (rc == 0)
		return M0_RC(0);

error:
	m0_cs_fini(&sctx->rsx_mero_ctx);
fclose:
	fclose(sctx->rsx_log_file);
	return M0_RC(rc);
}

void m0_rpc_server_stop(struct m0_rpc_server_ctx *sctx)
{
	M0_ENTRY("server_ctx: %p", sctx);

	m0_cs_fini(&sctx->rsx_mero_ctx);
	fclose(sctx->rsx_log_file);

	M0_LEAVE();
}

M0_INTERNAL struct m0_rpc_machine *
m0_rpc_server_ctx_get_rmachine(struct m0_rpc_server_ctx *sctx)
{
	return m0_mero_to_rmach(&sctx->rsx_mero_ctx);
}
#endif /* !__KERNEL__ */

M0_INTERNAL int m0_rpc_client_connect(struct m0_rpc_conn    *conn,
				      struct m0_rpc_session *session,
				      struct m0_rpc_machine *rpc_mach,
				      const char            *remote_addr,
				      struct m0_fid         *svc_fid,
				      uint64_t               max_rpcs_in_flight,
				      m0_time_t              abs_timeout)
{
	struct m0_net_end_point *ep;
	int                      rc;

	M0_ENTRY("conn=%p session=%p rpc_mach=%p remote_addr=%s",
	         conn, session, rpc_mach, remote_addr);

	rc = m0_net_end_point_create(&ep, &rpc_mach->rm_tm, remote_addr);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_rpc_conn_create(conn, svc_fid, ep, rpc_mach, max_rpcs_in_flight,
				abs_timeout);
	m0_net_end_point_put(ep);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_rpc_session_create(session, conn, abs_timeout);
	if (rc != 0)
		(void)m0_rpc_conn_destroy(conn, abs_timeout);

	return M0_RC(rc);
}

M0_INTERNAL int
m0_rpc_client_find_connect(struct m0_rpc_conn       *conn,
			   struct m0_rpc_session    *session,
			   struct m0_rpc_machine    *rpc_mach,
			   const char               *remote_addr,
			   enum m0_conf_service_type stype,
			   uint64_t                  max_rpcs_in_flight,
			   m0_time_t                 abs_timeout)
{
	struct m0_conf_obj *svc_obj = NULL;
	struct m0_fid      *svc_fid = NULL;
	int                 rc;

	M0_ENTRY();
	M0_PRE(rpc_mach != NULL);

	rc = m0_confc_service_find(m0_reqh2confc(rpc_mach->rm_reqh), stype,
				   remote_addr, &svc_obj);
	if (rc != 0)
		return M0_ERR(rc);
	if (svc_obj != NULL)
		svc_fid = &svc_obj->co_id;
	return M0_RC(m0_rpc_client_connect(conn, session, rpc_mach, remote_addr,
					   svc_fid, max_rpcs_in_flight,
					   abs_timeout));
}

int m0_rpc_client_start(struct m0_rpc_client_ctx *cctx)
{
	enum { NR_TM = 1 }; /* number of TMs */
	int rc;

	M0_ENTRY("client_ctx: %p", cctx);

	if (cctx->rcx_recv_queue_min_length == 0)
		cctx->rcx_recv_queue_min_length = M0_NET_TM_RECV_QUEUE_DEF_LEN;

	rc = m0_rpc_net_buffer_pool_setup(
		cctx->rcx_net_dom, &cctx->rcx_buffer_pool,
		m0_rpc_bufs_nr(cctx->rcx_recv_queue_min_length, NR_TM),
		NR_TM);
	if (rc != 0)
		return M0_RC(rc);

	M0_SET0(&cctx->rcx_reqh);
	rc = M0_REQH_INIT(&cctx->rcx_reqh,
			  .rhia_dtm     = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_fid     = cctx->rcx_fid,
		);
	if (rc != 0)
		goto err;
	m0_reqh_start(&cctx->rcx_reqh);

	rc = m0_rpc_machine_init(&cctx->rcx_rpc_machine,
				 cctx->rcx_net_dom, cctx->rcx_local_addr,
				 &cctx->rcx_reqh,
				 &cctx->rcx_buffer_pool, M0_BUFFER_ANY_COLOUR,
				 cctx->rcx_max_rpc_msg_size,
				 cctx->rcx_recv_queue_min_length);
	if (rc != 0) {
		m0_reqh_services_terminate(&cctx->rcx_reqh);
		m0_reqh_fini(&cctx->rcx_reqh);
		goto err;
	}

	rc = m0_rpc_client_connect(&cctx->rcx_connection, &cctx->rcx_session,
				   &cctx->rcx_rpc_machine,
				   cctx->rcx_remote_addr, NULL,
				   cctx->rcx_max_rpcs_in_flight,
				   cctx->rcx_abs_timeout?:M0_TIME_NEVER);
	if (rc == 0)
		return M0_RC(0);

	m0_reqh_services_terminate(&cctx->rcx_reqh);
	m0_rpc_machine_fini(&cctx->rcx_rpc_machine);
	m0_reqh_fini(&cctx->rcx_reqh);
err:
	m0_rpc_net_buffer_pool_cleanup(&cctx->rcx_buffer_pool);
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_client_start);


int m0_rpc_client_stop(struct m0_rpc_client_ctx *cctx)
{
	return m0_rpc_client_stop_stats(cctx, NULL);
}
M0_EXPORTED(m0_rpc_client_stop);

int m0_rpc_client_stop_stats(struct m0_rpc_client_ctx *cctx,
			     void (*printout)(struct m0_rpc_machine *))
{
	int rc0;
	int rc1;

	M0_ENTRY("client_ctx: %p", cctx);

	rc0 = m0_rpc_session_destroy(&cctx->rcx_session, M0_TIME_NEVER);
	if (rc0 != 0)
		M0_LOG(M0_ERROR, "Failed to terminate session %d", rc0);

	rc1 = m0_rpc_conn_destroy(&cctx->rcx_connection, M0_TIME_NEVER);
	if (rc1 != 0)
		M0_LOG(M0_ERROR, "Failed to terminate connection %d", rc1);
	if (printout != NULL)
		printout(&cctx->rcx_rpc_machine);
	m0_reqh_services_terminate(&cctx->rcx_reqh);
	m0_rpc_machine_fini(&cctx->rcx_rpc_machine);
	m0_reqh_fini(&cctx->rcx_reqh);
	m0_rpc_net_buffer_pool_cleanup(&cctx->rcx_buffer_pool);

	return M0_RC(rc0 ?: rc1);
}
M0_EXPORTED(m0_rpc_client_stop_stats);

int m0_rpc_post_with_timeout_sync(struct m0_fop                *fop,
				  struct m0_rpc_session        *session,
				  const struct m0_rpc_item_ops *ri_ops,
				  m0_time_t                     deadline,
				  m0_time_t                     timeout)
{
	struct m0_rpc_item *item;
	int                 rc;

	M0_ENTRY("fop: %p, session: %p", fop, session);
	M0_PRE(session != NULL);

	item              = &fop->f_item;
	item->ri_ops      = ri_ops;
	item->ri_session  = session;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = deadline;

	/* Set default value if user hasn't set anything else */
	if (item->ri_nr_sent_max == ~(uint64_t)0)
		item->ri_nr_sent_max = M0_RPCLIB_MAX_RETRIES;

	/*
	 * Add a ref so that the item does not get vanished, say due to
	 * session cancellation, while it is being waited for.
	 */
	m0_fop_get(fop);
	rc = m0_rpc_post(item) ?:
	     m0_rpc_item_wait_for_reply(item, timeout) ?:
	     m0_rpc_item_generic_reply_rc(item->ri_reply);
	m0_fop_put_lock(fop);
	return M0_RC(rc);
}
M0_EXPORTED(m0_rpc_post_with_timeout_sync);

int m0_rpc_post_sync(struct m0_fop                *fop,
		     struct m0_rpc_session        *session,
		     const struct m0_rpc_item_ops *ri_ops,
		     m0_time_t                     deadline)
{
	M0_ENTRY("fop: %p, session: %p", fop, session);
	M0_PRE(session != NULL);

	return m0_rpc_post_with_timeout_sync(fop, session, ri_ops, deadline,
					     M0_TIME_NEVER);
}
M0_EXPORTED(m0_rpc_post_sync);

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
