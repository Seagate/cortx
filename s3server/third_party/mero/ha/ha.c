/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 26-Apr-2016
 */


/**
 * @addtogroup ha
 *
 * TODO remove m0_ha_outgoing_session()
 * TODO handle memory allocation errors
 * TODO handle all errors
 * TODO add magics for ha_links
 * TODO take hlcc_rpc_service_fid for incoming connections from HA
 * TODO deal with locking in ha_link_incoming_find()
 * TODO s/container_of/bob_of/g
 * TODO deal with race: m0_ha_connect() with entrypoint client state
 * TODO use ml_name everywhere in ha/
 * TODO enable process fid assert in m0_ha_process_failed()
 * TODO check fid types
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/ha.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/errno.h"          /* ENOMEM */
#include "lib/misc.h"           /* M0_IS0 */
#include "lib/semaphore.h"      /* m0_semaphore */
#include "lib/string.h"         /* m0_strdup */

#include "module/instance.h"    /* m0_get */
#include "module/module.h"      /* m0_module */

#include "ha/link.h"            /* m0_ha_link */
#include "ha/link_service.h"    /* m0_ha_link_service_init */
#include "ha/entrypoint.h"      /* m0_ha_entrypoint_rep */

#include "conf/obj.h"            /* M0_CONF_PROCESS_TYPE */

enum {
	HA_MAX_RPCS_IN_FLIGHT = 2,
	HA_DISCONNECT_TIMEOUT = 5,
	HA_RESEND_INTERVAL    = 1,
	HA_RECONNECT_INTERVAL = 1,
	HA_NR_SENT_MAX        = 10,
};

struct m0_ha_module {
	struct m0_module hmo_module;
};

enum ha_link_ctx_type {
	HLX_INCOMING,
	HLX_OUTGOING,
};

struct ha_link_ctx {
	struct m0_ha_link      hlx_link;
	/* clink for events */
	struct m0_clink        hlx_clink;
	struct m0_ha          *hlx_ha;
	struct m0_tlink        hlx_tlink;
	uint64_t               hlx_magic;
	enum ha_link_ctx_type  hlx_type;
	struct m0_clink        hlx_stop_clink;
	struct m0_semaphore    hlx_stop_sem;
	struct m0_fid          hlx_process_fid;
	/* protected by m0_ha::h_lock */
	bool                   hlx_disconnecting;
};

M0_TL_DESCR_DEFINE(ha_links, "m0_ha::h_links_{incoming,outgoing}", static,
		   struct ha_link_ctx, hlx_tlink, hlx_magic,
		   7, 8);
M0_TL_DEFINE(ha_links, static, struct ha_link_ctx);

static bool ha_link_event_cb(struct m0_clink *clink)
{
	enum m0_ha_link_state  state;
	struct ha_link_ctx    *hlx;
	struct m0_ha_link     *hl;
	struct m0_ha_msg      *msg;
	struct m0_ha          *ha;
	uint64_t               tag;

	hlx   = container_of(clink, struct ha_link_ctx, hlx_clink);
	hl    = &hlx->hlx_link;
	ha    = hlx->hlx_ha;
	state = m0_ha_link_state_get(hl);
	M0_ENTRY("hlx=%p hl=%p ha=%p state=%d state_name=%s",
		 hlx, hl, ha, state, m0_ha_link_state_name(state));
	switch (state) {
	case M0_HA_LINK_STATE_RECV:
		while ((msg = m0_ha_link_recv(hl, &tag)) != NULL)
			ha->h_cfg.hcf_ops.hao_msg_received(ha, hl, msg, tag);
		break;
	case M0_HA_LINK_STATE_DELIVERY:
		while ((tag = m0_ha_link_delivered_consume(hl)) !=
		       M0_HA_MSG_TAG_INVALID) {
			ha->h_cfg.hcf_ops.hao_msg_is_delivered(ha, hl, tag);
		}
		while ((tag = m0_ha_link_not_delivered_consume(hl)) !=
		       M0_HA_MSG_TAG_INVALID) {
			ha->h_cfg.hcf_ops.hao_msg_is_not_delivered(ha, hl, tag);
		}
		break;
	case M0_HA_LINK_STATE_LINK_FAILED:
		if (hl == ha->h_link) {
			/*
			 * Outgoing HA link reconnect via requesting
			 * an entrypoint.
			 */
			m0_ha_entrypoint_client_request(
						&ha->h_entrypoint_client);
		} else {
			/* XXX */
		}
		break;
	case M0_HA_LINK_STATE_DISCONNECTING:
		if (hlx == ha->h_link_ctx) {
			m0_ha_link_stop(&hlx->hlx_link, &hlx->hlx_stop_clink);
		} else {
			ha->h_cfg.hcf_ops.
				hao_link_is_disconnecting(ha, &hlx->hlx_link);
		}
		break;
	case M0_HA_LINK_STATE_STOP:
		if (hlx != ha->h_link_ctx) {
			ha->h_cfg.hcf_ops.hao_link_disconnected(ha,
								&hlx->hlx_link);
		}
		break;
	default:
		/* nothing to do here */
		break;
	}
	M0_LEAVE();
	return true;
}

static struct ha_link_ctx *
ha_link_incoming_find(struct m0_ha                   *ha,
                      const struct m0_ha_link_params *lp)
{
	M0_PRE(m0_mutex_is_locked(&ha->h_lock));

	return m0_tl_find(ha_links, hlx, &ha->h_links_incoming,
	  m0_uint128_eq(&hlx->hlx_link.hln_conn_cfg.hlcc_params.hlp_id_local,
	                &lp->hlp_id_remote) &&
	  m0_uint128_eq(&hlx->hlx_link.hln_conn_cfg.hlcc_params.hlp_id_remote,
	                &lp->hlp_id_local));
}

static bool ha_link_stop_cb(struct m0_clink *clink)
{
	struct ha_link_ctx *hlx;

	hlx   = container_of(clink, struct ha_link_ctx, hlx_stop_clink);
	m0_semaphore_up(&hlx->hlx_stop_sem);
	return true;
}

static int ha_link_ctx_init(struct m0_ha                     *ha,
                            struct ha_link_ctx               *hlx,
                            struct m0_ha_link_cfg            *hl_cfg,
                            const struct m0_ha_link_conn_cfg *hl_conn_cfg,
                            const struct m0_fid              *process_fid,
                            enum ha_link_ctx_type             hlx_type)
{
	struct m0_ha_link *hl = &hlx->hlx_link;
	int                rc;

	M0_ENTRY("ha=%p hlx=%p hlx_type=%d", ha, hlx, hlx_type);

	M0_PRE(M0_IN(hlx_type, (HLX_INCOMING, HLX_OUTGOING)));
	M0_PRE(equi(hlx_type == HLX_INCOMING, hl_conn_cfg != NULL));

	rc = m0_ha_link_init(hl, hl_cfg);
	M0_ASSERT(rc == 0);
	rc = m0_semaphore_init(&hlx->hlx_stop_sem, 0);
	M0_ASSERT(rc == 0);     /* XXX */
	m0_clink_init(&hlx->hlx_clink, &ha_link_event_cb);
	m0_clink_add_lock(m0_ha_link_chan(hl), &hlx->hlx_clink);
	m0_clink_init(&hlx->hlx_stop_clink, &ha_link_stop_cb);
	hlx->hlx_stop_clink.cl_is_oneshot = true;
	hlx->hlx_disconnecting = hlx_type == HLX_OUTGOING;
	hlx->hlx_process_fid   = *process_fid;

	hlx->hlx_ha   = ha;
	hlx->hlx_type = hlx_type;
	m0_mutex_lock(&ha->h_lock);
	M0_ASSERT(ergo(hlx_type == HLX_INCOMING,
	               ha_link_incoming_find(ha, &hl_conn_cfg->hlcc_params) ==
	               NULL));
	ha_links_tlink_init_at_tail(hlx, hlx_type == HLX_INCOMING ?
				    &ha->h_links_incoming :
				    &ha->h_links_outgoing);
	m0_mutex_unlock(&ha->h_lock);

	return M0_RC(0);
}

static void ha_link_ctx_fini(struct m0_ha *ha, struct ha_link_ctx *hlx)
{
	M0_ENTRY("ha=%p hlx=%p", ha, hlx);

	M0_PRE(m0_mutex_is_locked(&ha->h_lock));
	M0_PRE_EX(ergo(hlx->hlx_type == HLX_INCOMING,
		       ha_links_tlist_contains(&ha->h_links_stopping, hlx)));

	ha_links_tlink_del_fini(hlx);

	M0_ASSERT(hlx->hlx_disconnecting);
	m0_clink_fini(&hlx->hlx_stop_clink);
	m0_clink_del_lock(&hlx->hlx_clink);
	m0_clink_fini(&hlx->hlx_clink);
	m0_semaphore_fini(&hlx->hlx_stop_sem);
	m0_ha_link_fini(&hlx->hlx_link);
	M0_LEAVE();
}

static uint64_t ha_generation_next(struct m0_ha *ha)
{
	uint64_t generation;

	m0_mutex_lock(&ha->h_lock);
	generation = ha->h_generation_counter++;
	m0_mutex_unlock(&ha->h_lock);
	return generation;
}

static void
ha_request_received_cb(struct m0_ha_entrypoint_server    *hes,
                       const struct m0_ha_entrypoint_req *req,
                       const struct m0_uint128           *req_id)
{
	struct m0_ha *ha;

	ha = container_of(hes, struct m0_ha, h_entrypoint_server);
	M0_ENTRY("ha=%p hes=%p req=%p", ha, hes, req);
	ha->h_cfg.hcf_ops.hao_entrypoint_request(ha, req, req_id);
}

static void ha_link_conn_cfg_make(struct m0_ha_link_conn_cfg *hl_conn_cfg,
                                  const char                 *rpc_endpoint)
{
	*hl_conn_cfg = (struct m0_ha_link_conn_cfg) {
		.hlcc_rpc_service_fid    = M0_FID0,
		.hlcc_rpc_endpoint       = rpc_endpoint,
		.hlcc_max_rpcs_in_flight = HA_MAX_RPCS_IN_FLIGHT,
		.hlcc_connect_timeout    = M0_TIME_NEVER,
		.hlcc_disconnect_timeout = M0_MKTIME(HA_DISCONNECT_TIMEOUT, 0),
		.hlcc_resend_interval    = M0_MKTIME(HA_RESEND_INTERVAL, 0),
		.hlcc_reconnect_interval = M0_MKTIME(HA_RECONNECT_INTERVAL, 0),
		.hlcc_nr_sent_max        = HA_NR_SENT_MAX,
	};
}

static bool ha_entrypoint_state_cb(struct m0_clink *clink)
{
	enum m0_ha_entrypoint_client_state  state;
	struct m0_ha_entrypoint_client     *ecl;
	struct m0_ha_entrypoint_req        *req;
	struct m0_ha_entrypoint_rep        *rep;
	struct m0_ha_link_conn_cfg          hl_conn_cfg;
	struct m0_ha_link                  *hl;
	struct m0_ha                       *ha;
	bool                                consumed = true;

	M0_ENTRY();

	ha    = container_of(clink, struct m0_ha, h_clink);
	ecl   = &ha->h_entrypoint_client;
	state = m0_ha_entrypoint_client_state_get(ecl);
	req   = &ha->h_entrypoint_client.ecl_req;
	rep   = &ha->h_entrypoint_client.ecl_rep;
	M0_LOG(M0_DEBUG, "ha=%p ecl=%p state=%d", ha, ecl, state);

	switch (state) {
	case M0_HEC_AVAILABLE:
		M0_LOG(M0_DEBUG, "ha=%p hae_contol=%d", ha, rep->hae_control);
		M0_ASSERT_INFO(!rep->hae_disconnected_previously,
		               "HA has already decided that this process has "
		               "failed. There is no good reason to continue "
		               "doing something, and there is no code yet "
		               "to handle graceful shutdown in this case. "
		               "Let's just terminate the process and let HA "
		               "do it's job.");
		M0_ASSERT(equi(!rep->hae_link_do_reconnect,
		               m0_ha_cookie_is_eq(&ha->h_cookie_remote,
		                                  &m0_ha_cookie_no_record) ||
		               m0_ha_cookie_is_eq(&ha->h_cookie_remote,
		                                  &rep->hae_cookie_actual)));
		ha->h_cookie_remote = rep->hae_cookie_actual;
		ha_link_conn_cfg_make(&hl_conn_cfg, ha->h_cfg.hcf_addr);
		hl_conn_cfg.hlcc_params = rep->hae_link_params;
		hl = &ha->h_link_ctx->hlx_link;
		if (req->heq_first_request) {
			M0_ASSERT(!rep->hae_link_do_reconnect);
			m0_ha_link_start(hl, &hl_conn_cfg);
			ha->h_link_started = true;
			req->heq_first_request = false;
		} else {
			if (rep->hae_link_do_reconnect) {
				m0_ha_link_reconnect_end(hl, &hl_conn_cfg);
			} else {
				m0_ha_link_reconnect_cancel(hl);
			}
		}
		if (rep->hae_control == M0_HA_ENTRYPOINT_QUERY) {
			/*
			 * XXX: it looks like delay belongs here, or maybe
			 * m0_ha_entrypoint_client_request() itself should
			 * implement deferred request somehow.
			 */
			m0_ha_entrypoint_client_request(ecl);
		} else {
			ha->h_cfg.hcf_ops.hao_entrypoint_replied(ha, rep);
			consumed = false;
		}
		break;
	case M0_HEC_UNAVAILABLE:
		m0_ha_entrypoint_client_request(ecl);
		break;
	case M0_HEC_FILL:
		req->heq_generation      = ha_generation_next(ha);
		req->heq_cookie_expected = ha->h_cookie_remote;
		if (!req->heq_first_request) {
			m0_ha_link_reconnect_begin(&ha->h_link_ctx->hlx_link,
						   &req->heq_link_params);
		}
		break;
	default:
		break;
	}
	M0_LEAVE();
	return consumed;
}

/* tentative definition, isn't possible in C++ */
static const struct m0_modlev ha_levels[];

static int ha_level_enter(struct m0_module *module)
{
	struct m0_ha_link_cfg  hl_cfg;
	enum m0_ha_level       level = module->m_cur + 1;
	struct m0_ha          *ha;
	char                  *addr;
	int                    rc;

	ha = container_of(module, struct m0_ha, h_module);
	M0_ENTRY("ha=%p level=%d %s", ha, level, ha_levels[level].ml_name);
	switch (level) {
	case M0_HA_LEVEL_ASSIGNS:
		m0_mutex_init(&ha->h_lock);
		ha_links_tlist_init(&ha->h_links_incoming);
		ha_links_tlist_init(&ha->h_links_outgoing);
		ha_links_tlist_init(&ha->h_links_stopping);
		m0_clink_init(&ha->h_clink, ha_entrypoint_state_cb);
		m0_ha_cookie_init(&ha->h_cookie_local);
		m0_ha_cookie_record(&ha->h_cookie_local);
		m0_ha_cookie_init(&ha->h_cookie_remote);
		ha->h_link_id_counter            = 1;
		ha->h_generation_counter         = 1;
		ha->h_link_started               = false;
		ha->h_warn_local_link_disconnect = true;
		return M0_RC(0);
	case M0_HA_LEVEL_ADDR_STRDUP:
		addr = m0_strdup(ha->h_cfg.hcf_addr);
		if (addr == NULL)
			return M0_ERR(-ENOMEM);
		ha->h_cfg.hcf_addr = addr;
		return M0_RC(0);
	case M0_HA_LEVEL_LINK_SERVICE:
		return M0_RC(m0_ha_link_service_init(&ha->h_hl_service,
						     ha->h_cfg.hcf_reqh));
	case M0_HA_LEVEL_ENTRYPOINT_SERVER_INIT:
		ha->h_cfg.hcf_entrypoint_server_cfg =
			(struct m0_ha_entrypoint_server_cfg){
				.hesc_reqh             = ha->h_cfg.hcf_reqh,
				.hesc_request_received =&ha_request_received_cb,
			};
		return M0_RC(m0_ha_entrypoint_server_init(
		                        &ha->h_entrypoint_server,
		                        &ha->h_cfg.hcf_entrypoint_server_cfg));
	case M0_HA_LEVEL_ENTRYPOINT_CLIENT_INIT:
		ha->h_cfg.hcf_entrypoint_client_cfg =
			(struct m0_ha_entrypoint_client_cfg){
				.hecc_reqh        = ha->h_cfg.hcf_reqh,
				.hecc_rpc_machine = ha->h_cfg.hcf_rpc_machine,
				.hecc_process_fid = ha->h_cfg.hcf_process_fid,
			};
		return M0_RC(m0_ha_entrypoint_client_init(
		                         &ha->h_entrypoint_client,
					  ha->h_cfg.hcf_addr,
	                                 &ha->h_cfg.hcf_entrypoint_client_cfg));
	case M0_HA_LEVEL_INIT:
		M0_IMPOSSIBLE("can't be here");
		return M0_ERR(-ENOSYS);
	case M0_HA_LEVEL_ENTRYPOINT_SERVER_START:
		m0_ha_entrypoint_server_start(&ha->h_entrypoint_server);
		return M0_RC(0);
	case M0_HA_LEVEL_INCOMING_LINKS:
		return M0_RC(0);
	case M0_HA_LEVEL_START:
		M0_IMPOSSIBLE("can't be here");
		return M0_ERR(-ENOSYS);
	case M0_HA_LEVEL_LINK_CTX_ALLOC:
		M0_ALLOC_PTR(ha->h_link_ctx);
		if (ha->h_link_ctx == NULL)
			return M0_ERR(-ENOMEM);
		return M0_RC(0);
	case M0_HA_LEVEL_LINK_CTX_INIT:
		hl_cfg = (struct m0_ha_link_cfg){
			.hlc_reqh         = ha->h_cfg.hcf_reqh,
			.hlc_reqh_service = ha->h_hl_service,
			.hlc_rpc_machine  = ha->h_cfg.hcf_rpc_machine,
			.hlq_q_cfg_in     = {},
			.hlq_q_cfg_out    = {},
		};
		rc = ha_link_ctx_init(ha, ha->h_link_ctx, &hl_cfg, NULL,
				      &M0_FID0, HLX_OUTGOING);
		return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
	case M0_HA_LEVEL_ENTRYPOINT_CLIENT_START:
		ha->h_entrypoint_client.ecl_req.heq_first_request = true;
		M0_SET0(&ha->h_entrypoint_client.ecl_req.heq_link_params);
		m0_clink_add_lock(
		        m0_ha_entrypoint_client_chan(&ha->h_entrypoint_client),
			&ha->h_clink);
		m0_ha_entrypoint_client_start(&ha->h_entrypoint_client);
		return M0_RC(0);
	case M0_HA_LEVEL_ENTRYPOINT_CLIENT_WAIT:
		m0_chan_wait(&ha->h_clink);
		return M0_RC(0);
	case M0_HA_LEVEL_LINK_ASSIGN:
		m0_mutex_lock(&ha->h_lock);
		ha->h_link = &ha->h_link_ctx->hlx_link;
		m0_mutex_unlock(&ha->h_lock);
		return M0_RC(0);
	case M0_HA_LEVEL_CONNECT:
		M0_IMPOSSIBLE("can't be here");
		return M0_ERR(-ENOSYS);
	}
	return M0_ERR(-ENOSYS);
}

static void ha_level_leave(struct m0_module *module)
{
	struct ha_link_ctx *hlx;
	enum m0_ha_level    level = module->m_cur;
	struct m0_ha       *ha;
	struct m0_tl       *list;

	ha = container_of(module, struct m0_ha, h_module);
	M0_ENTRY("ha=%p level=%d %s", ha, level, ha_levels[level].ml_name);
	switch (level) {
	case M0_HA_LEVEL_ASSIGNS:
		m0_ha_cookie_fini(&ha->h_cookie_remote);
		m0_ha_cookie_fini(&ha->h_cookie_local);
		m0_clink_fini(&ha->h_clink);
		ha_links_tlist_fini(&ha->h_links_stopping);
		ha_links_tlist_fini(&ha->h_links_outgoing);
		ha_links_tlist_fini(&ha->h_links_incoming);
		m0_mutex_fini(&ha->h_lock);
		break;
	case M0_HA_LEVEL_ADDR_STRDUP:
		/* it's OK to free allocated in ha_level_enter() char * */
		m0_free((char *)ha->h_cfg.hcf_addr);
		break;
	case M0_HA_LEVEL_LINK_SERVICE:
		m0_ha_link_service_fini(ha->h_hl_service);
		break;
	case M0_HA_LEVEL_ENTRYPOINT_SERVER_INIT:
		m0_ha_entrypoint_server_fini(&ha->h_entrypoint_server);
		break;
	case M0_HA_LEVEL_ENTRYPOINT_CLIENT_INIT:
		m0_ha_entrypoint_client_fini(&ha->h_entrypoint_client);
		break;
	case M0_HA_LEVEL_INIT:
		M0_IMPOSSIBLE("can't be here");
		break;
	case M0_HA_LEVEL_ENTRYPOINT_SERVER_START:
		m0_ha_entrypoint_server_stop(&ha->h_entrypoint_server);
		break;
	case M0_HA_LEVEL_INCOMING_LINKS:
		/*
		 * There is situation when m0_ha_process_failed() disconnects
		 * more than one ha_link. Therefore, we need to empty
		 * h_links_incoming first and then check h_links_stopping.
		 */
		list = &ha->h_links_incoming;
		while (true) {
			m0_mutex_lock(&ha->h_lock);
			hlx = ha_links_tlist_head(list);
			m0_mutex_unlock(&ha->h_lock);
			if (hlx == NULL && list == &ha->h_links_incoming) {
				list = &ha->h_links_stopping;
				continue;
			}
			if (hlx == NULL && list == &ha->h_links_stopping)
				break;
			M0_LOG(M0_DEBUG, "hlx=%p", hlx);
			if (list == &ha->h_links_incoming)
				m0_ha_process_failed(ha, &hlx->hlx_process_fid);
			m0_semaphore_down(&hlx->hlx_stop_sem);
			m0_mutex_lock(&ha->h_lock);
			ha_link_ctx_fini(ha, hlx);
			m0_mutex_unlock(&ha->h_lock);
			m0_free(hlx);
		}
		break;
	case M0_HA_LEVEL_START:
		M0_IMPOSSIBLE("can't be here");
		break;
	case M0_HA_LEVEL_LINK_CTX_ALLOC:
		m0_free(ha->h_link_ctx);
		break;
	case M0_HA_LEVEL_LINK_CTX_INIT:
		ha->h_warn_local_link_disconnect = false;
		/* @see ha_entrypoint_state_cb for m0_ha_link_start() */
		if (ha->h_link_started) {
			m0_ha_link_cb_disconnecting(&ha->h_link_ctx->hlx_link);
			m0_semaphore_down(&ha->h_link_ctx->hlx_stop_sem);
		}
		m0_mutex_lock(&ha->h_lock);
		ha_link_ctx_fini(ha, ha->h_link_ctx);
		m0_mutex_unlock(&ha->h_lock);
		break;
	case M0_HA_LEVEL_ENTRYPOINT_CLIENT_START:
		m0_ha_entrypoint_client_stop(&ha->h_entrypoint_client);
		break;
	case M0_HA_LEVEL_ENTRYPOINT_CLIENT_WAIT:
		m0_clink_del_lock(&ha->h_clink);
		break;
	case M0_HA_LEVEL_LINK_ASSIGN:
		m0_mutex_lock(&ha->h_lock);
		ha->h_link = NULL;
		m0_mutex_unlock(&ha->h_lock);
		break;
	case M0_HA_LEVEL_CONNECT:
		M0_IMPOSSIBLE("can't be here");
		break;
	}
	M0_LEAVE();
}

static const struct m0_modlev ha_levels[] = {
	[M0_HA_LEVEL_ASSIGNS] = {
		.ml_name  = "M0_HA_LEVEL_ASSIGNS",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_ADDR_STRDUP] = {
		.ml_name  = "M0_HA_LEVEL_ADDR_STRDUP",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_LINK_SERVICE] = {
		.ml_name  = "M0_HA_LEVEL_LINK_SERVICE",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_ENTRYPOINT_SERVER_INIT] = {
		.ml_name  = "M0_HA_LEVEL_ENTRYPOINT_SERVER_INIT",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_ENTRYPOINT_CLIENT_INIT] = {
		.ml_name  = "M0_HA_LEVEL_ENTRYPOINT_CLIENT_INIT",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_INIT] = {
		.ml_name  = "M0_HA_LEVEL_INIT",
	},
	[M0_HA_LEVEL_ENTRYPOINT_SERVER_START] = {
		.ml_name  = "M0_HA_LEVEL_ENTRYPOINT_SERVER_START",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_INCOMING_LINKS] = {
		.ml_name  = "M0_HA_LEVEL_INCOMING_LINKS",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_START] = {
		.ml_name  = "M0_HA_LEVEL_START",
	},
	[M0_HA_LEVEL_LINK_CTX_ALLOC] = {
		.ml_name  = "M0_HA_LEVEL_LINK_CTX_ALLOC",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_LINK_CTX_INIT] = {
		.ml_name  = "M0_HA_LEVEL_LINK_CTX_INIT",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_ENTRYPOINT_CLIENT_START] = {
		.ml_name  = "M0_HA_LEVEL_ENTRYPOINT_CLIENT_START",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_ENTRYPOINT_CLIENT_WAIT] = {
		.ml_name  = "M0_HA_LEVEL_ENTRYPOINT_CLIENT_WAIT",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_LINK_ASSIGN] = {
		.ml_name  = "M0_HA_LEVEL_LINK_ASSIGN",
		.ml_enter = ha_level_enter,
		.ml_leave = ha_level_leave,
	},
	[M0_HA_LEVEL_CONNECT] = {
		.ml_name  = "M0_HA_LEVEL_CONNECT",
	},
};

M0_INTERNAL int m0_ha_init(struct m0_ha *ha, struct m0_ha_cfg *ha_cfg)
{
	int rc;

	M0_ENTRY("ha=%p hcf_rpc_machine=%p hcf_reqh=%p",
	         ha, ha_cfg->hcf_rpc_machine, ha_cfg->hcf_reqh);
	M0_PRE(M0_IS0(ha));
	ha->h_cfg = *ha_cfg;
	m0_module_setup(&ha->h_module, "m0_ha_module",
			ha_levels, ARRAY_SIZE(ha_levels), m0_get());
	rc = m0_module_init(&ha->h_module, M0_HA_LEVEL_INIT);
	if (rc != 0) {
		m0_module_fini(&ha->h_module, M0_MODLEV_NONE);
		return M0_ERR(rc);
	}
	return M0_RC(0);
}

M0_INTERNAL int m0_ha_start(struct m0_ha *ha)
{
	int rc;

	M0_ENTRY("ha=%p", ha);
	rc = m0_module_init(&ha->h_module, M0_HA_LEVEL_START);
	if (rc != 0) {
		m0_module_fini(&ha->h_module, M0_HA_LEVEL_INIT);
		return M0_ERR(rc);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_ha_stop(struct m0_ha *ha)
{
	M0_ENTRY("ha=%p", ha);
	m0_module_fini(&ha->h_module, M0_HA_LEVEL_INIT);
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_fini(struct m0_ha *ha)
{
	M0_ENTRY("ha=%p", ha);

	m0_module_fini(&ha->h_module, M0_MODLEV_NONE);
	M0_LEAVE();
}

M0_INTERNAL struct m0_ha_link *m0_ha_connect(struct m0_ha *ha)
{
	struct m0_ha_link *hl;
	int                rc;

	M0_ENTRY("ha=%p hcf_addr=%s", ha, ha->h_cfg.hcf_addr);
	rc = m0_module_init(&ha->h_module, M0_HA_LEVEL_CONNECT);
	if (rc == 0) {
		m0_mutex_lock(&ha->h_lock);
		hl = ha->h_link;
		m0_mutex_unlock(&ha->h_lock);
	} else {
		m0_module_fini(&ha->h_module, M0_HA_LEVEL_START);
		M0_LOG(M0_ERROR, "rc=%d", rc);
		hl = NULL;
	}
	M0_LEAVE("ha=%p hcf_addr=%s hl=%p", ha, ha->h_cfg.hcf_addr, hl);
	return hl;
}

M0_INTERNAL void m0_ha_disconnect(struct m0_ha *ha)
{
	M0_ENTRY("ha=%p hcf_addr=%s", ha, ha->h_cfg.hcf_addr);
	m0_module_fini(&ha->h_module, M0_HA_LEVEL_START);
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_disconnect_incoming(struct m0_ha      *ha,
                                           struct m0_ha_link *hl)
{
	struct ha_link_ctx *hlx;

	hlx = container_of(hl, struct ha_link_ctx, hlx_link);
	M0_ENTRY("ha=%p hl=%p", ha, hl);
	M0_PRE(hlx->hlx_type == HLX_INCOMING);
	m0_mutex_lock(&ha->h_lock);
	ha_links_tlist_move(&ha->h_links_stopping, hlx);
	m0_mutex_unlock(&ha->h_lock);
	m0_ha_link_stop(&hlx->hlx_link, &hlx->hlx_stop_clink);
	M0_LEAVE();
}

static void ha_link_id_next(struct m0_ha      *ha,
                            struct m0_uint128 *id)
{
	*id = M0_UINT128(0, ha->h_link_id_counter++);
}

static int
ha_link_incoming_create(struct m0_ha                       *ha,
                        const struct m0_ha_entrypoint_req  *req,
                        struct m0_ha_link_conn_cfg         *hl_conn_cfg,
                        struct ha_link_ctx                **hlx_ptr)
{
	struct m0_ha_link_cfg       hl_cfg;
	struct ha_link_ctx         *hlx;
	int                         rc;

	hl_cfg = (struct m0_ha_link_cfg){
		.hlc_reqh         = ha->h_cfg.hcf_reqh,
		.hlc_reqh_service = ha->h_hl_service,
		.hlc_rpc_machine  = ha->h_cfg.hcf_rpc_machine,
		.hlq_q_cfg_in     = {},
		.hlq_q_cfg_out    = {},
	};
	M0_ALLOC_PTR(hlx);
	M0_ASSERT(hlx != NULL); /* XXX */
	rc = ha_link_ctx_init(ha, hlx, &hl_cfg, hl_conn_cfg,
			      &req->heq_process_fid, HLX_INCOMING);
	M0_ASSERT(rc == 0);     /* XXX */
	m0_ha_link_start(&hlx->hlx_link, hl_conn_cfg);
	*hlx_ptr = hlx;
	return 0;
}

static void ha_link_handle(struct m0_ha                       *ha,
                           const struct m0_uint128            *req_id,
                           const struct m0_ha_entrypoint_req  *req,
                           struct m0_ha_entrypoint_rep        *rep,
                           struct m0_ha_link                 **hl_ptr)
{
	struct m0_ha_link_conn_cfg  hl_conn_cfg;
	struct ha_link_ctx         *hlx;
	struct m0_uint128           id_local;
	struct m0_uint128           id_remote;
	struct m0_uint128           id_connection;
	uint64_t                    generation;
	bool                        add_new_link;
	int                         rc;

	m0_mutex_lock(&ha->h_lock);
	hlx = ha_link_incoming_find(ha, &req->heq_link_params);
	m0_mutex_unlock(&ha->h_lock);
	M0_LOG(M0_DEBUG, "hlx=%p", hlx);
	rep->hae_link_do_reconnect       = false;
	rep->hae_disconnected_previously = false;
	if (hlx == NULL) {
		ha_link_conn_cfg_make(&hl_conn_cfg, req->heq_rpc_endpoint);
		generation = ha_generation_next(ha);
		id_connection = M0_UINT128(req->heq_generation, generation);
		ha_link_id_next(ha, &id_local);
		ha_link_id_next(ha, &id_remote);
		if (req->heq_first_request) {
			M0_LOG(M0_DEBUG, "first connection to HA");
			hl_conn_cfg.hlcc_params = (struct m0_ha_link_params){
				.hlp_id_connection = id_connection,
				.hlp_id_local      = id_local,
				.hlp_id_remote     = id_remote,
			};
			m0_ha_link_tags_initial(
			        &hl_conn_cfg.hlcc_params.hlp_tags_local, false);
			m0_ha_link_tags_initial(
			        &hl_conn_cfg.hlcc_params.hlp_tags_remote, true);
			m0_ha_link_params_invert(&rep->hae_link_params,
						 &hl_conn_cfg.hlcc_params);
			add_new_link = true;
		} else if (m0_ha_cookie_is_eq(&ha->h_cookie_local,
		                             &req->heq_cookie_expected)) {
			M0_LOG(M0_DEBUG, "link had been disconnected "
			       "previously, a new one shouldn't be created");
			rep->hae_disconnected_previously = true;
			add_new_link = false;
		} else {
			M0_LOG(M0_DEBUG, "HA has restarted, reconnect case");
			m0_ha_link_reconnect_params(&req->heq_link_params,
			                            &rep->hae_link_params,
			                            &hl_conn_cfg.hlcc_params,
			                            &id_remote, &id_local,
			                            &id_connection);
			rep->hae_link_do_reconnect = true;
			add_new_link = true;
		}
		if (add_new_link) {
			rc = ha_link_incoming_create(ha, req, &hl_conn_cfg, &hlx);
			M0_ASSERT(rc == 0);
			ha->h_cfg.hcf_ops.hao_link_connected(ha, req_id,
			                                     &hlx->hlx_link);
		} else {
			ha->h_cfg.hcf_ops.hao_link_absent(ha, req_id);
		}
	} else {
		if (req->heq_first_request) {
			M0_IMPOSSIBLE("m0d has restarted, new link case. "
			              "Link should already be disconnected.");
		} else {
			M0_LOG(M0_DEBUG, "everyone is alive, link is reused");
			ha->h_cfg.hcf_ops.hao_link_reused(ha, req_id,
							  &hlx->hlx_link);
		}
	}
	rep->hae_cookie_actual = ha->h_cookie_local;
	if (hl_ptr != NULL)
		*hl_ptr = hlx == NULL ? NULL : &hlx->hlx_link;
	M0_LEAVE("ha=%p heq_rpc_endpoint=%s hl=%p",
		 ha, req->heq_rpc_endpoint, &hlx->hlx_link);
}

void m0_ha_entrypoint_reply(struct m0_ha                       *ha,
                            const struct m0_uint128            *req_id,
			    const struct m0_ha_entrypoint_rep  *rep,
			    struct m0_ha_link                 **hl_ptr)
{
	struct m0_ha_entrypoint_rep        rep_copy = *rep;
	const struct m0_ha_entrypoint_req *req;

	M0_ENTRY("ha=%p req_id="U128X_F" rep=%p", ha, U128_P(req_id), rep);
	req = m0_ha_entrypoint_server_request_find(&ha->h_entrypoint_server,
	                                           req_id);
	M0_ASSERT(req != NULL);
	ha_link_handle(ha, req_id, req, &rep_copy, hl_ptr);
	m0_ha_entrypoint_server_reply(&ha->h_entrypoint_server,
				      req_id, &rep_copy);
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_send(struct m0_ha           *ha,
                            struct m0_ha_link      *hl,
                            const struct m0_ha_msg *msg,
                            uint64_t               *tag)
{
	m0_ha_link_send(hl, msg, tag);
}

M0_INTERNAL void m0_ha_delivered(struct m0_ha      *ha,
                                 struct m0_ha_link *hl,
                                 struct m0_ha_msg  *msg)
{
	m0_ha_link_delivered(hl, msg);
}

enum m0_ha_mod_level {
	M0_HA_MOD_LEVEL_ASSIGNS,
	M0_HA_MOD_LEVEL_LINK_SERVICE,
	M0_HA_MOD_LEVEL_LINK,
	M0_HA_MOD_LEVEL_ENTRYPOINT,
	M0_HA_MOD_LEVEL_STARTED,
};

static const struct m0_modlev ha_mod_levels[];

static int ha_mod_level_enter(struct m0_module *module)
{
	enum m0_ha_mod_level  level = module->m_cur + 1;
	struct m0_ha_module  *ha_module;

	ha_module = container_of(module, struct m0_ha_module, hmo_module);
	M0_ENTRY("ha_module=%p level=%d %s", ha_module, level,
		 ha_mod_levels[level].ml_name);
	switch (level) {
	case M0_HA_MOD_LEVEL_ASSIGNS:
		M0_PRE(m0_get()->i_ha_module == NULL);
		m0_get()->i_ha_module = ha_module;
		return M0_RC(0);
	case M0_HA_MOD_LEVEL_LINK_SERVICE:
		return M0_RC(m0_ha_link_mod_init());
	case M0_HA_MOD_LEVEL_LINK:
		return M0_RC(m0_ha_link_service_mod_init());
	case M0_HA_MOD_LEVEL_ENTRYPOINT:
		return M0_RC(m0_ha_entrypoint_mod_init());
	case M0_HA_MOD_LEVEL_STARTED:
		M0_IMPOSSIBLE("can't be here");
		return M0_ERR(-ENOSYS);
	}
	return M0_ERR(-ENOSYS);
}

static void ha_mod_level_leave(struct m0_module *module)
{
	enum m0_ha_mod_level  level = module->m_cur;
	struct m0_ha_module  *ha_module;

	ha_module = container_of(module, struct m0_ha_module, hmo_module);
	M0_ENTRY("ha_module=%p level=%d", ha_module, level);
	switch (level) {
	case M0_HA_MOD_LEVEL_ASSIGNS:
		m0_get()->i_ha_module = NULL;
		break;
	case M0_HA_MOD_LEVEL_LINK_SERVICE:
		m0_ha_link_service_mod_fini();
		break;
	case M0_HA_MOD_LEVEL_LINK:
		m0_ha_link_mod_fini();
		break;
	case M0_HA_MOD_LEVEL_ENTRYPOINT:
		m0_ha_entrypoint_mod_fini();
		break;
	case M0_HA_MOD_LEVEL_STARTED:
		M0_IMPOSSIBLE("can't be here");
		break;
	}
	M0_LEAVE();
}

static const struct m0_modlev ha_mod_levels[] = {
	[M0_HA_MOD_LEVEL_ASSIGNS] = {
		.ml_name  = "M0_HA_MOD_LEVEL_ASSIGNS",
		.ml_enter = ha_mod_level_enter,
		.ml_leave = ha_mod_level_leave,
	},
	[M0_HA_MOD_LEVEL_LINK_SERVICE] = {
		.ml_name  = "M0_HA_MOD_LEVEL_LINK_SERVICE",
		.ml_enter = ha_mod_level_enter,
		.ml_leave = ha_mod_level_leave,
	},
	[M0_HA_MOD_LEVEL_LINK] = {
		.ml_name  = "M0_HA_MOD_LEVEL_LINK",
		.ml_enter = ha_mod_level_enter,
		.ml_leave = ha_mod_level_leave,
	},
	[M0_HA_MOD_LEVEL_ENTRYPOINT] = {
		.ml_name  = "M0_HA_MOD_LEVEL_ENTRYPOINT",
		.ml_enter = ha_mod_level_enter,
		.ml_leave = ha_mod_level_leave,
	},
	[M0_HA_MOD_LEVEL_STARTED] = {
		.ml_name  = "M0_HA_MOD_LEVEL_STARTED",
	},
};

M0_INTERNAL void m0_ha_flush(struct m0_ha      *ha,
			     struct m0_ha_link *hl)
{
	m0_ha_link_flush(hl);
}

M0_INTERNAL void m0_ha_process_failed(struct m0_ha        *ha,
                                      const struct m0_fid *process_fid)
{
	struct ha_link_ctx *hlx;
	bool                disconnecting;

	M0_ENTRY("ha=%p process_fid="FID_F, ha, FID_P(process_fid));
	/*
	 * XXX Temporary disable the assert as the process fids are not always
	 * valid in Mero ST.
	 */
	/*
	M0_PRE(m0_fid_tget(process_fid) ==
	       M0_CONF_PROCESS_TYPE.cot_ftype.ft_id);
	*/
	if (ha->h_warn_local_link_disconnect &&
	    m0_fid_eq(process_fid, &ha->h_cfg.hcf_process_fid))
		M0_LOG(M0_WARN, "disconnecting local link");
	m0_mutex_lock(&ha->h_lock);
	m0_tl_for(ha_links, &ha->h_links_incoming, hlx) {
		if (!m0_fid_eq(process_fid, &hlx->hlx_process_fid))
			continue;
		disconnecting = hlx->hlx_disconnecting;
		hlx->hlx_disconnecting = true;
		if (!disconnecting)
			m0_ha_link_cb_disconnecting(&hlx->hlx_link);
		M0_LOG(M0_DEBUG, "ha=%p process_fid="FID_F" hlx=%p "
		       "disconnecting=%d", ha, FID_P(process_fid), hlx,
		       !!disconnecting);
	} m0_tl_endfor;
	m0_mutex_unlock(&ha->h_lock);
	M0_LEAVE("ha=%p process_fid="FID_F, ha, FID_P(process_fid));
}

M0_INTERNAL struct m0_ha_link *m0_ha_outgoing_link(struct m0_ha *ha)
{
	struct m0_ha_link *hl;

	m0_mutex_lock(&ha->h_lock);
	hl = ha->h_link;
	m0_mutex_unlock(&ha->h_lock);
	return hl;
}

M0_INTERNAL struct m0_rpc_session *m0_ha_outgoing_session(struct m0_ha *ha)
{
	struct ha_link_ctx *hlx;

	hlx = ha_links_tlist_head(&ha->h_links_outgoing);
	return m0_ha_link_rpc_session(&hlx->hlx_link);
}

M0_INTERNAL void m0_ha_rpc_endpoint(struct m0_ha      *ha,
                                    struct m0_ha_link *hl,
                                    char              *buf,
                                    m0_bcount_t        buf_len)
{
	m0_ha_link_rpc_endpoint(hl, buf, buf_len);
}

M0_INTERNAL int m0_ha_mod_init(void)
{
	struct m0_ha_module *ha_module;
	int                  rc;

	M0_ALLOC_PTR(ha_module);
	if (ha_module == NULL)
		return M0_ERR(-ENOMEM);

	m0_module_setup(&ha_module->hmo_module, "m0_ha_mod_module",
			ha_mod_levels, ARRAY_SIZE(ha_mod_levels), m0_get());
	rc = m0_module_init(&ha_module->hmo_module, M0_HA_MOD_LEVEL_STARTED);
	if (rc != 0) {
		m0_module_fini(&ha_module->hmo_module, M0_MODLEV_NONE);
		m0_free(ha_module);
		return M0_ERR(rc);
	}
	return M0_RC(0);
}

M0_INTERNAL void m0_ha_mod_fini(void)
{
	struct m0_ha_module *ha_module = m0_get()->i_ha_module;

	M0_PRE(ha_module != NULL);

	m0_module_fini(&ha_module->hmo_module, M0_MODLEV_NONE);
	m0_free(ha_module);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
