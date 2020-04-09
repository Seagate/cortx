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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 02/23/2012
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/errno.h"
#include "lib/trace.h"
#include "lib/tlist.h"
#include "lib/time.h"     /* m0_time_from_now */
#include "lib/bob.h"
#include "lib/memory.h"   /* m0_alloc() */
#include "rpc/link.h"
#include "rpc/service.h"
#include "rpc/rev_conn.h"
#include "rpc/rpc_internal.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "ha/note_fops.h"  /* m0_ha_state_fop_init */

/**
   @addtogroup rpc_service

   @{
 */

M0_TL_DESCR_DEFINE(rev_conn, "Reverse Connections", static,
		   struct m0_reverse_connection, rcf_linkage, rcf_magic,
		   M0_RM_REV_CONN_LIST_MAGIC, M0_RM_REV_CONN_LIST_HEAD_MAGIC);
M0_TL_DEFINE(rev_conn, static, struct m0_reverse_connection);

static const struct m0_bob_type rpc_svc_bob = {
	.bt_name = "rpc service",
	.bt_magix_offset = offsetof(struct m0_rpc_service, rps_magix),
	.bt_magix = M0_RPC_SERVICE_MAGIC,
	.bt_check = NULL
};
M0_BOB_DEFINE(M0_INTERNAL, &rpc_svc_bob, m0_rpc_service);

static int rpc_service_start(struct m0_reqh_service *service)
{
	struct m0_rpc_service *svc;

	svc = bob_of(service, struct m0_rpc_service, rps_svc, &rpc_svc_bob);
	rev_conn_tlist_init(&svc->rps_rev_conns);

	return 0;
}

static void rpc_service_stop(struct m0_reqh_service *service)
{
	struct m0_rpc_service *svc;

	svc = bob_of(service, struct m0_rpc_service, rps_svc, &rpc_svc_bob);
	m0_rpc_service_reverse_session_put(service);
	rev_conn_tlist_fini(&svc->rps_rev_conns);
}

static void rpc_service_fini(struct m0_reqh_service *service)
{
	struct m0_rpc_service *svc;

	svc = bob_of(service, struct m0_rpc_service, rps_svc, &rpc_svc_bob);
	m0_rpc_service_bob_fini(svc);
	m0_free(svc);
}

static int
rpc_service_fop_accept(struct m0_reqh_service *service, struct m0_fop *fop)
{
	return 0;
}

static const struct m0_reqh_service_ops rpc_ops = {
	.rso_start      = rpc_service_start,
	.rso_stop       = rpc_service_stop,
	.rso_fini       = rpc_service_fini,
	.rso_fop_accept = rpc_service_fop_accept
};

static int rpc_service_allocate(struct m0_reqh_service **service,
				const struct m0_reqh_service_type *stype)
{
	struct m0_rpc_service *svc;

	M0_PRE(stype != NULL && service != NULL);

	M0_ALLOC_PTR(svc);
	if (svc == NULL)
		return M0_ERR(-ENOMEM);

	m0_rpc_service_bob_init(svc);
	svc->rps_svc.rs_ops = &rpc_ops;
	*service = &svc->rps_svc;
	return 0;
}

static const struct m0_reqh_service_type_ops rpc_service_type_ops = {
	.rsto_service_allocate = rpc_service_allocate
};

struct m0_reqh_service_type m0_rpc_service_type = {
	.rst_name       = "rpcservice",
	.rst_ops        = &rpc_service_type_ops,
	.rst_level      = M0_RPC_SVC_LEVEL,
	.rst_keep_alive = true,
};
M0_EXPORTED(m0_rpc_service_type);

M0_INTERNAL int m0_rpc_service_register(void)
{
	m0_ha_state_fop_init();
	return m0_reqh_service_type_register(&m0_rpc_service_type);
}

M0_INTERNAL void m0_rpc_service_unregister(void)
{
	m0_reqh_service_type_unregister(&m0_rpc_service_type);
	m0_ha_state_fop_fini();
}

M0_INTERNAL struct m0_rpc_session *
m0_rpc_service_reverse_session_lookup(struct m0_reqh_service   *service,
				      const struct m0_rpc_item *item)
{

	struct m0_reverse_connection     *revc;
	struct m0_rpc_service            *svc;
	const struct m0_rpc_item_header2 *header;

	M0_PRE(item != NULL && service->rs_type == &m0_rpc_service_type);
	header = &item->ri_header;
	svc    = bob_of(service, struct m0_rpc_service, rps_svc, &rpc_svc_bob);
	if (header->osr_sender_id == SENDER_ID_INVALID)
		revc = m0_tl_find(rev_conn, revc, &svc->rps_rev_conns,
				m0_uint128_cmp(&revc->rcf_rlink.rlk_conn.c_uuid,
					       &header->osr_uuid) == 0);
	else
		revc = m0_tl_find(rev_conn, revc, &svc->rps_rev_conns,
				  revc->rcf_rlink.rlk_conn.c_sender_id ==
				  header->osr_sender_id);
	return revc == NULL ? NULL : &revc->rcf_rlink.rlk_sess;
}

M0_INTERNAL int
m0_rpc_service_reverse_session_get(struct m0_reqh_service   *service,
				   const struct m0_rpc_item *item,
				   struct m0_clink          *clink,
				   struct m0_rpc_session   **session)
{
	int                           rc;
	const char                   *rem_ep;
	struct m0_reverse_connection *revc;
	struct m0_rpc_service        *svc;

	M0_ENTRY();
	M0_PRE(item != NULL && service->rs_type == &m0_rpc_service_type);

	svc    = bob_of(service, struct m0_rpc_service, rps_svc, &rpc_svc_bob);
	rem_ep = m0_rpc_item_remote_ep_addr(item);

	M0_ALLOC_PTR(revc);
	rc = revc == NULL ? -ENOMEM : 0;
	rc = rc ?: m0_rpc_link_init(&revc->rcf_rlink,
				    item->ri_rmachine, NULL, rem_ep,
				    M0_REV_CONN_MAX_RPCS_IN_FLIGHT);
	if (rc == 0) {
		m0_rpc_link_connect_async(&revc->rcf_rlink,
				m0_time_from_now(M0_REV_CONN_TIMEOUT, 0),
				clink);
		*session = &revc->rcf_rlink.rlk_sess;
		rev_conn_tlink_init_at_tail(revc, &svc->rps_rev_conns);
	}
	return M0_RC(rc);
}

M0_INTERNAL void
m0_rpc_service_reverse_session_put(struct m0_reqh_service *service)
{
	struct m0_rpc_service        *svc;
	struct m0_reverse_connection *revc;

	M0_ENTRY();
	M0_PRE(service->rs_type == &m0_rpc_service_type);

	svc = bob_of(service, struct m0_rpc_service, rps_svc, &rpc_svc_bob);
	m0_tl_for(rev_conn, &svc->rps_rev_conns, revc) {
		M0_SET0(&revc->rcf_disc_wait);
		if (revc->rcf_rlink.rlk_connected) {
			m0_clink_init(&revc->rcf_disc_wait, NULL);
			revc->rcf_disc_wait.cl_is_oneshot = true;
			m0_rpc_link_disconnect_async(&revc->rcf_rlink,
				m0_time_from_now(M0_REV_CONN_TIMEOUT, 0),
				&revc->rcf_disc_wait);
		}
	} m0_tlist_endfor;
	m0_tl_teardown(rev_conn, &svc->rps_rev_conns, revc) {
		if (revc->rcf_disc_wait.cl_group != NULL) {
			m0_chan_wait(&revc->rcf_disc_wait);
			m0_clink_fini(&revc->rcf_disc_wait);
		}
		m0_rpc_link_fini(&revc->rcf_rlink);
		rev_conn_tlink_fini(revc);
		m0_free(revc);
	}

	M0_LEAVE();
}

M0_INTERNAL int m0_rpc_session_status(struct m0_rpc_session *session)
{
	return container_of(session, struct m0_rpc_link, rlk_sess)->rlk_rc;
}

M0_INTERNAL struct m0_reqh_service *
m0_reqh_rpc_service_find(struct m0_reqh *reqh)
{
	return m0_reqh_service_find(&m0_rpc_service_type, reqh);
}

M0_INTERNAL int m0_rpc_service_start(struct m0_reqh *reqh)
{
	return reqh->rh_rpc_service == NULL ?
		m0_reqh_service_setup(&reqh->rh_rpc_service,
				      &m0_rpc_service_type, reqh, NULL, NULL) :
		0;
}

M0_INTERNAL void m0_rpc_service_stop(struct m0_reqh *reqh)
{
	if (m0_reqh_rpc_mach_tlist_is_empty(&reqh->rh_rpc_machines)) {
		m0_reqh_service_quit(reqh->rh_rpc_service);
		reqh->rh_rpc_service = NULL;
	}
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of rpc_service group */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
