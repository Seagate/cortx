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
 * Original creation date: 7-Apr-2016
 */


/**
 * @addtogroup ha
 *
 * TODO bob_of in ha_link_rpc_wait_cb
 * TODO handle rpc failures in incoming fom (for reply)
 * TODO use m0_module in m0_ha_link
 * TODO reconsider localities for incoming/outgoing foms
 * TODO handle rpc error in HA_LINK_OUTGOING_STATE_CONNECTING state
 * TODO m0_ha_link_cb_disconnecting() and m0_ha_link_cb_reused() - copy-paste
 *
 * * m0_ha_link outgoing fom state machine
 *
 * @verbatim
 *
 *              INIT  FINISH
 *                 v  ^
 *     RPC_LINK_INIT  RPC_LINK_FINI
 *                 v  ^
 *             NOT_CONNECTED
 *                 v  ^
 *           CONNECT  DISCONNECTING
 *                 v  ^
 * INCOMING_REGISTER  INCOMING_DEREGISTER
 *                 v  ^
 *                 v  INCOMING_QUIESCE_WAIT
 *                 v  ^
 *                 v  INCOMING_QUIESCE
 *                 v  ^
 *        CONNECTING  DISCONNECT
 *                 v  ^
 *                 IDLE <------+
 *                  v          |
 *                 SEND        |
 *                  v          |
 *             WAIT_REPLY      |
 *                  v          |
 *             WAIT_RELEASE >--+
 *
 * @endverbatim
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/link.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/errno.h"          /* ENOMEM */
#include "lib/tlist.h"          /* M0_TL_DESCR_DEFINE */
#include "lib/types.h"          /* m0_uint128 */
#include "lib/misc.h"           /* container_of */
#include "lib/time.h"           /* m0_time_from_now */

#include "sm/sm.h"              /* m0_sm_state_descr */
#include "rpc/rpc.h"            /* m0_rpc_reply_post */
#include "rpc/rpc_opcodes.h"    /* M0_HA_LINK_OUTGOING_OPCODE */

#include "fop/fom_generic.h"    /* M0_FOPH_FINISH */

#include "ha/link_fops.h"       /* m0_ha_link_msg_fopt */
#include "ha/link_service.h"    /* m0_ha_link_service_register */


enum {
	/**
	 * If incoming HA link fop arrives and the link it's supposed to arrive
	 * to is not there the code emits a warning. The problem here is that it
	 * might be a case when the link is going to be created shortly after,
	 * so the warning is a false positive. This parameter allows to suppress
	 * a number of first messages for the link.
	 *
	 * The normal case:
	 * @verbatim
	 * remote                               local
	 *   <--------------------------------- sends entrypoint request
	 * makes local and remote
	 * link parameters
	 * creates the link
	 * sends the entrypoint reply ----------->
	 *                                      creates the link
	 * sends the first message -------------->
	 *                                      the message arrives,
	 *                                      everything is fine, no warning.
	 * @endverbatim
	 *
	 * If the first message from remote is sent before local creates the
	 * link, then the message arrives for non-existent link which leads
	 * to the false positive warning.
	 */
	HA_LINK_SUPPRESS_START_NR = 3,
};

static struct m0_sm_state_descr ha_link_sm_states[] = {
	[M0_HA_LINK_STATE_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "M0_HA_LINK_STATE_INIT",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_FINI,
		                      M0_HA_LINK_STATE_START),
	},
	[M0_HA_LINK_STATE_FINI] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "M0_HA_LINK_STATE_FINI",
		.sd_allowed = 0,
	},
	[M0_HA_LINK_STATE_START] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_START",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_STOP,
		                      M0_HA_LINK_STATE_IDLE),
	},
	[M0_HA_LINK_STATE_STOP] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_STOP",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_FINI),
	},
	[M0_HA_LINK_STATE_IDLE] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_IDLE",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_STOP,
		                      M0_HA_LINK_STATE_RECV,
		                      M0_HA_LINK_STATE_DELIVERY,
		                      M0_HA_LINK_STATE_RPC_FAILED,
		                      M0_HA_LINK_STATE_LINK_FAILED,
		                      M0_HA_LINK_STATE_LINK_REUSED,
		                      M0_HA_LINK_STATE_DISCONNECTING),
	},
	[M0_HA_LINK_STATE_RECV] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_RECV",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_IDLE),
	},
	[M0_HA_LINK_STATE_DELIVERY] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_DELIVERY",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_IDLE),
	},
	[M0_HA_LINK_STATE_RPC_FAILED] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_RPC_FAILED",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_IDLE),
	},
	[M0_HA_LINK_STATE_LINK_FAILED] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_LINK_FAILED",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_IDLE),
	},
	[M0_HA_LINK_STATE_LINK_REUSED] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_LINK_REUSED",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_IDLE),
	},
	[M0_HA_LINK_STATE_DISCONNECTING] = {
		.sd_flags   = 0,
		.sd_name    = "M0_HA_LINK_STATE_DISCONNECTING",
		.sd_allowed = M0_BITS(M0_HA_LINK_STATE_IDLE),
	},
};

M0_BASSERT(ARRAY_SIZE(ha_link_sm_states) == M0_HA_LINK_STATE_NR);

static struct m0_sm_conf ha_link_sm_conf = {
	.scf_name      = "m0_ha_link::hln_sm",
	.scf_nr_states = ARRAY_SIZE(ha_link_sm_states),
	.scf_state     = ha_link_sm_states,
};

static struct m0_fom_type ha_link_outgoing_fom_type;
extern const struct m0_fom_ops ha_link_outgoing_fom_ops;

static void ha_link_outgoing_fom_wakeup(struct m0_ha_link *hl);

static bool ha_link_rpc_wait_cb(struct m0_clink *clink)
{
	struct m0_ha_link *hl;

	M0_ENTRY();
	hl = container_of(clink, struct m0_ha_link, hln_rpc_wait);
	m0_mutex_lock(&hl->hln_lock);
	hl->hln_rpc_event_occurred = true;
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE();
	return true;
}

static bool ha_link_quiesce_wait_cb(struct m0_clink *clink)
{
	struct m0_ha_link *hl;

	M0_ENTRY();
	hl = container_of(clink, struct m0_ha_link, hln_quiesce_wait);
	m0_mutex_lock(&hl->hln_lock);
	hl->hln_quiesced = true;
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE();
	return true;
}

M0_INTERNAL int m0_ha_link_init(struct m0_ha_link     *hl,
				struct m0_ha_link_cfg *hl_cfg)
{
	int rc;

	M0_PRE(M0_IS0(hl));

	M0_ENTRY("hl=%p hlc_reqh=%p hlc_reqh_service=%p hlc_rpc_machine=%p",
	         hl, hl_cfg->hlc_reqh, hl_cfg->hlc_reqh_service,
	         hl_cfg->hlc_rpc_machine);
	hl->hln_cfg = *hl_cfg;
	m0_mutex_init(&hl->hln_lock);
	m0_ha_lq_init(&hl->hln_q_in, &hl->hln_cfg.hlq_q_cfg_in);
	m0_ha_lq_init(&hl->hln_q_out, &hl->hln_cfg.hlq_q_cfg_out);
	m0_fom_init(&hl->hln_fom, &ha_link_outgoing_fom_type,
	            &ha_link_outgoing_fom_ops, NULL, NULL,
	            hl->hln_cfg.hlc_reqh);
	rc = m0_semaphore_init(&hl->hln_stop_cond, 0);
	M0_ASSERT(rc == 0);
	m0_mutex_init(&hl->hln_stop_chan_lock);
	m0_chan_init(&hl->hln_stop_chan, &hl->hln_stop_chan_lock);
	m0_sm_group_init(&hl->hln_sm_group);
	m0_sm_init(&hl->hln_sm, &ha_link_sm_conf, M0_HA_LINK_STATE_INIT,
		   &hl->hln_sm_group);
	m0_clink_init(&hl->hln_rpc_wait, &ha_link_rpc_wait_cb);
	m0_clink_init(&hl->hln_quiesce_wait, &ha_link_quiesce_wait_cb);
	m0_mutex_init(&hl->hln_quiesce_chan_lock);
	m0_chan_init(&hl->hln_quiesce_chan, &hl->hln_quiesce_chan_lock);
	m0_clink_add_lock(&hl->hln_quiesce_chan, &hl->hln_quiesce_wait);
	m0_sm_timer_init(&hl->hln_reconnect_wait_timer);
	hl->hln_rpc_wait.cl_is_oneshot = true;
	hl->hln_reconnect = false;
	hl->hln_reconnect_wait = false;
	hl->hln_reconnect_cfg_is_set = false;
	hl->hln_waking_up = false;
	hl->hln_fom_is_stopping = false;
	hl->hln_fom_enable_wakeup = true;
	hl->hln_no_new_delivered = false;
	hl->hln_req_fop_seq = 0;
	return M0_RC(0);
}

M0_INTERNAL void m0_ha_link_fini(struct m0_ha_link *hl)
{
	M0_ENTRY("hl=%p", hl);
	m0_sm_timer_fini(&hl->hln_reconnect_wait_timer);
	m0_clink_del_lock(&hl->hln_quiesce_wait);
	m0_chan_fini_lock(&hl->hln_quiesce_chan);
	m0_mutex_fini(&hl->hln_quiesce_chan_lock);
	m0_clink_fini(&hl->hln_quiesce_wait);
	m0_clink_fini(&hl->hln_rpc_wait);
	m0_sm_group_lock(&hl->hln_sm_group);
	m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_FINI);
	m0_sm_fini(&hl->hln_sm);
	m0_sm_group_unlock(&hl->hln_sm_group);
	m0_sm_group_fini(&hl->hln_sm_group);
	m0_mutex_lock(&hl->hln_stop_chan_lock);
	m0_chan_fini(&hl->hln_stop_chan);
	m0_mutex_unlock(&hl->hln_stop_chan_lock);
	m0_mutex_fini(&hl->hln_stop_chan_lock);
	m0_semaphore_fini(&hl->hln_stop_cond);
	m0_ha_lq_fini(&hl->hln_q_out);
	m0_ha_lq_fini(&hl->hln_q_in);
	m0_mutex_fini(&hl->hln_lock);
	M0_LEAVE();
}

static int ha_link_conn_cfg_copy(struct m0_ha_link_conn_cfg       *dst,
                                 const struct m0_ha_link_conn_cfg *src)
{
	char *ep = m0_strdup(src->hlcc_rpc_endpoint);

	if (ep == NULL)
		return M0_ERR_INFO(-ENOMEM, "%s", src->hlcc_rpc_endpoint);
	*dst = *src;
	dst->hlcc_rpc_endpoint = ep;
	return M0_RC(0);
}

static void ha_link_conn_cfg_free(struct m0_ha_link_conn_cfg *hl_conn_cfg)
{
	/* This value is allocated in ha_link_conn_cfg_copy() */
	m0_free((char *)hl_conn_cfg->hlcc_rpc_endpoint);
}

static void ha_link_tags_apply(struct m0_ha_link              *hl,
                               const struct m0_ha_link_params *lp)
{
	const struct m0_ha_link_tags *tags_in_new  = &lp->hlp_tags_remote;
	const struct m0_ha_link_tags *tags_out_new = &lp->hlp_tags_local;
	struct m0_ha_link_tags        tags_in;
	struct m0_ha_link_tags        tags_out;
	bool                          done;

	M0_PRE(m0_mutex_is_locked(&hl->hln_lock));

	m0_ha_lq_tags_get(&hl->hln_q_out, &tags_out);
	m0_ha_lq_tags_get(&hl->hln_q_in,  &tags_in);
	M0_LOG(M0_DEBUG, "hl=%p     out="HLTAGS_F, hl, HLTAGS_P(&tags_out));
	M0_LOG(M0_DEBUG, "hl=%p new out="HLTAGS_F, hl, HLTAGS_P(tags_out_new));
	M0_LOG(M0_DEBUG, "hl=%p      in="HLTAGS_F, hl, HLTAGS_P(&tags_in));
	M0_LOG(M0_DEBUG, "hl=%p  new in="HLTAGS_F, hl, HLTAGS_P(tags_in_new));

	/* Move 'next' tag to 'delivered'. Leave all other tags intact */
	while (m0_ha_lq_tag_next(&hl->hln_q_out) >
	       m0_ha_lq_tag_delivered(&hl->hln_q_out)) {
		done = m0_ha_lq_try_unnext(&hl->hln_q_out);
		M0_ASSERT(done);
	}
	M0_ASSERT_INFO(m0_ha_lq_tag_next(&hl->hln_q_out) ==
		       m0_ha_lq_tag_delivered(&hl->hln_q_out),
		       "m0_ha_lq_tag_next(&hl->hln_q_out)=%"PRIu64" "
		       "m0_ha_lq_tag_delivered(&hl->hln_q_out)=%"PRIu64,
		       m0_ha_lq_tag_next(&hl->hln_q_out),
		       m0_ha_lq_tag_delivered(&hl->hln_q_out));

	m0_ha_lq_tags_get(&hl->hln_q_out, &tags_out);
	M0_ASSERT_INFO(m0_ha_lq_tag_delivered(&hl->hln_q_out) >=
		       tags_out_new->hlt_confirmed,
		       "m0_ha_lq_tag_delivered(&hl->hln_q_out)=%"PRIu64" "
		       "tags_out_new->hlt_confirmed=%"PRIu64,
		       m0_ha_lq_tag_delivered(&hl->hln_q_out),
		       tags_out_new->hlt_confirmed);
	M0_ASSERT_INFO(m0_ha_lq_tag_delivered(&hl->hln_q_out) ==
		       tags_out_new->hlt_delivered,
		       "m0_ha_lq_tag_delivered(&hl->hln_q_out)=%"PRIu64" "
		       "tags_out_new->hlt_delivered=%"PRIu64,
		       m0_ha_lq_tag_delivered(&hl->hln_q_out),
		       tags_out_new->hlt_delivered);
	M0_ASSERT_INFO(m0_ha_lq_tag_next(&hl->hln_q_out) ==
		       tags_out_new->hlt_next,
		       "m0_ha_lq_tag_next(&hl->hln_q_out)=%"PRIu64" "
		       "tags_out_new->hlt_next=%"PRIu64,
		       m0_ha_lq_tag_next(&hl->hln_q_out),
		       tags_out_new->hlt_next);
	M0_ASSERT_INFO(m0_ha_lq_tag_assign(&hl->hln_q_out) >=
		       tags_out_new->hlt_assign,
		       "m0_ha_lq_tag_assign(&hl->hln_q_out)=%"PRIu64" "
		       "tags_out_new->hlt_assign=%"PRIu64,
		       m0_ha_lq_tag_assign(&hl->hln_q_out),
		       tags_out_new->hlt_assign);
	M0_ASSERT_INFO(m0_ha_link_tags_eq(&tags_in, tags_in_new),
		       "tags_in="HLTAGS_F" tags_in_new="HLTAGS_F,
		       HLTAGS_P(&tags_in), HLTAGS_P(tags_in_new));

	m0_ha_lq_tags_get(&hl->hln_q_out, &tags_out);
	m0_ha_lq_tags_get(&hl->hln_q_in,  &tags_in);
	M0_LEAVE("hl=%p out="HLTAGS_F" in="HLTAGS_F,
		 hl, HLTAGS_P(&tags_out), HLTAGS_P(&tags_in));
}

M0_INTERNAL void m0_ha_link_start(struct m0_ha_link          *hl,
                                  struct m0_ha_link_conn_cfg *hl_conn_cfg)
{
	struct m0_ha_link_params *lp;
	int rc;

	M0_ENTRY("hl=%p hlp_id_local="U128X_F" hlp_id_remote="U128X_F" "
	         "hlp_id_connection="U128X_F, hl,
	         U128_P(&hl_conn_cfg->hlcc_params.hlp_id_local),
	         U128_P(&hl_conn_cfg->hlcc_params.hlp_id_remote),
	         U128_P(&hl_conn_cfg->hlcc_params.hlp_id_connection));
	M0_LOG(M0_DEBUG, "hlcc_rpc_service_fid="FID_F" "
	       "hlcc_rpc_endpoint=%s hlcc_max_rpcs_in_flight=%"PRIu64,
	       FID_P(&hl_conn_cfg->hlcc_rpc_service_fid),
	       (const char *)hl_conn_cfg->hlcc_rpc_endpoint,
	       hl_conn_cfg->hlcc_max_rpcs_in_flight);
	rc = ha_link_conn_cfg_copy(&hl->hln_conn_cfg, hl_conn_cfg);
	M0_ASSERT(rc == 0);     /* XXX */
	m0_mutex_lock(&hl->hln_lock);
	lp = &hl->hln_conn_cfg.hlcc_params;
	m0_ha_lq_tags_set(&hl->hln_q_out, &lp->hlp_tags_local);
	m0_ha_lq_tags_set(&hl->hln_q_in,  &lp->hlp_tags_remote);
	hl->hln_cb_disconnecting = false;
	hl->hln_cb_reused        = false;
	m0_mutex_unlock(&hl->hln_lock);
	m0_fom_queue(&hl->hln_fom);
	hl->hln_fom_locality = &hl->hln_fom.fo_loc->fl_locality;
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_link_stop(struct m0_ha_link *hl, struct m0_clink *clink)
{
	M0_ENTRY("hl=%p", hl);
	M0_PRE(clink->cl_is_oneshot);
	m0_clink_add_lock(&hl->hln_stop_chan, clink);
	m0_semaphore_up(&hl->hln_stop_cond);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_link_reconnect_begin(struct m0_ha_link        *hl,
                                            struct m0_ha_link_params *lp)
{
	m0_mutex_lock(&hl->hln_lock);
	*lp = hl->hln_conn_cfg.hlcc_params;
	m0_ha_lq_tags_get(&hl->hln_q_out, &lp->hlp_tags_local);
	m0_ha_lq_tags_get(&hl->hln_q_in,  &lp->hlp_tags_remote);
	m0_mutex_unlock(&hl->hln_lock);
	/* TODO refactor it somehow */
	M0_LOG(M0_DEBUG, "hl=%p id_local="U128X_F" id_remote="U128X_F" "
	       "id_connection="U128X_F, hl, U128_P(&lp->hlp_id_local),
	       U128_P(&lp->hlp_id_remote), U128_P(&lp->hlp_id_connection));
	M0_LOG(M0_DEBUG, "hl=%p  hlp_tags_local="HLTAGS_F,
	       hl, HLTAGS_P(&lp->hlp_tags_local));
	M0_LOG(M0_DEBUG, "hl=%p hlp_tags_remote="HLTAGS_F,
	       hl, HLTAGS_P(&lp->hlp_tags_remote));
}

M0_INTERNAL void
m0_ha_link_reconnect_end(struct m0_ha_link                *hl,
                         const struct m0_ha_link_conn_cfg *hl_conn_cfg)
{
	const struct m0_ha_link_params *lp = &hl_conn_cfg->hlcc_params;
	int                             rc;

	/* TODO refactor the M0_LOG of hl_conn_cfg */
	M0_ENTRY("hl=%p hlcc_rpc_service_fid="FID_F" hlcc_rpc_endpoint=%s",
		 hl, FID_P(&hl_conn_cfg->hlcc_rpc_service_fid),
		 hl_conn_cfg->hlcc_rpc_endpoint);
	M0_ENTRY("hl=%p hlcc_max_rpcs_in_flight=%"PRIu64" "
	         "hlcc_connect_timeout=%"PRIu64" "
	         "hlcc_disconnect_timeout=%"PRIu64" "
	         "hlcc_resend_interval=%"PRIu64" "
	         "hlcc_nr_sent_max=%"PRIu64,
	         hl, hl_conn_cfg->hlcc_max_rpcs_in_flight,
	         hl_conn_cfg->hlcc_connect_timeout,
	         hl_conn_cfg->hlcc_disconnect_timeout,
	         hl_conn_cfg->hlcc_resend_interval,
	         hl_conn_cfg->hlcc_nr_sent_max);
	M0_LOG(M0_DEBUG, "hl=%p id_local="U128X_F" id_remote="U128X_F" "
	       "id_connection="U128X_F, hl, U128_P(&lp->hlp_id_local),
	       U128_P(&lp->hlp_id_remote), U128_P(&lp->hlp_id_connection));
	M0_LOG(M0_DEBUG, "hl=%p  hlp_tags_local="HLTAGS_F,
	       hl, HLTAGS_P(&lp->hlp_tags_local));
	M0_LOG(M0_DEBUG, "hl=%p hlp_tags_remote="HLTAGS_F,
	       hl, HLTAGS_P(&lp->hlp_tags_remote));
	m0_mutex_lock(&hl->hln_lock);
	if (hl->hln_reconnect_cfg_is_set)
		ha_link_conn_cfg_free(&hl->hln_conn_reconnect_cfg);
	rc = ha_link_conn_cfg_copy(&hl->hln_conn_reconnect_cfg, hl_conn_cfg);
	M0_ASSERT(rc == 0);     /* XXX */
	hl->hln_reconnect_cfg_is_set = true;
	hl->hln_reconnect = true;
	m0_mutex_unlock(&hl->hln_lock);
	M0_LEAVE("hl=%p", hl);
}

M0_INTERNAL void m0_ha_link_reconnect_cancel(struct m0_ha_link *hl)
{
	M0_ENTRY("hl=%p", hl);
	M0_LEAVE();
}

M0_INTERNAL void
m0_ha_link_reconnect_params(const struct m0_ha_link_params *lp_alive,
			    struct m0_ha_link_params       *lp_alive_new,
			    struct m0_ha_link_params       *lp_dead_new,
			    const struct m0_uint128        *id_alive,
			    const struct m0_uint128        *id_dead,
			    const struct m0_uint128        *id_connection)
{
	const struct m0_ha_link_tags *tags_local  = &lp_alive->hlp_tags_local;
	const struct m0_ha_link_tags *tags_remote = &lp_alive->hlp_tags_remote;

	*lp_alive_new = (struct m0_ha_link_params){
		.hlp_id_local      = *id_alive,
		.hlp_id_remote     = *id_dead,
		.hlp_id_connection = *id_connection,
		.hlp_tags_local    = {
			.hlt_confirmed   = tags_local->hlt_confirmed,
			.hlt_delivered = tags_local->hlt_delivered,
			.hlt_next        = tags_local->hlt_delivered,
			.hlt_assign      = tags_local->hlt_assign,
		},
		.hlp_tags_remote   = {
			.hlt_confirmed   = tags_remote->hlt_confirmed,
			.hlt_delivered = tags_remote->hlt_delivered,
			.hlt_next        = tags_remote->hlt_next,
			.hlt_assign      = tags_remote->hlt_assign,
		},
	};
	*lp_dead_new = (struct m0_ha_link_params){
		.hlp_id_local      = *id_dead,
		.hlp_id_remote     = *id_alive,
		.hlp_id_connection = *id_connection,
		.hlp_tags_local    = {
			.hlt_confirmed   = tags_remote->hlt_delivered,
			.hlt_delivered = tags_remote->hlt_delivered,
			.hlt_next        = tags_remote->hlt_next,
			.hlt_assign      = tags_remote->hlt_next,
		},
		.hlp_tags_remote   = {
			.hlt_confirmed   = tags_local->hlt_delivered,
			.hlt_delivered = tags_local->hlt_delivered,
			.hlt_next        = tags_local->hlt_delivered,
			.hlt_assign      = tags_local->hlt_delivered,
		},
	};
}

M0_INTERNAL struct m0_chan *m0_ha_link_chan(struct m0_ha_link *hl)
{
	return &hl->hln_sm.sm_chan;
}

M0_INTERNAL enum m0_ha_link_state m0_ha_link_state_get(struct m0_ha_link *hl)
{
	M0_PRE(m0_sm_group_is_locked(&hl->hln_sm_group));
	return hl->hln_sm.sm_state;
}

M0_INTERNAL const char *m0_ha_link_state_name(enum m0_ha_link_state state)
{
	M0_PRE(_0C(state >= M0_HA_LINK_STATE_INIT) &&
	       _0C(state < M0_HA_LINK_STATE_NR));
	return ha_link_sm_states[state].sd_name;
}

enum ha_link_send_type {
	HA_LINK_SEND_QUERY,
	HA_LINK_SEND_POST,
	HA_LINK_SEND_REPLY,
};

M0_INTERNAL void m0_ha_link_send(struct m0_ha_link      *hl,
                                 const struct m0_ha_msg *msg,
                                 uint64_t               *tag)
{
	M0_ENTRY("hl=%p msg=%p", hl, msg);
	m0_mutex_lock(&hl->hln_lock);
	*tag = m0_ha_lq_enqueue(&hl->hln_q_out, msg);
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE("hl=%p msg=%p tag=%"PRIu64, hl, msg, *tag);
}

M0_INTERNAL struct m0_ha_msg *m0_ha_link_recv(struct m0_ha_link *hl,
					      uint64_t          *tag)
{
	struct m0_ha_msg *msg;

	m0_mutex_lock(&hl->hln_lock);
	msg = m0_ha_lq_next(&hl->hln_q_in);
	if (msg != NULL)
		*tag = m0_ha_msg_tag(msg);
	m0_mutex_unlock(&hl->hln_lock);

	M0_LOG(M0_DEBUG, "hl=%p msg=%p tag=%"PRIu64,
	       hl, msg, msg == NULL ? M0_HA_MSG_TAG_UNKNOWN : *tag);
	return msg;
}

M0_INTERNAL void m0_ha_link_delivered(struct m0_ha_link *hl,
				      struct m0_ha_msg  *msg)
{
	M0_ENTRY("hl=%p msg=%p tag=%"PRIu64, hl, msg, m0_ha_msg_tag(msg));
	m0_mutex_lock(&hl->hln_lock);
	m0_ha_lq_mark_delivered(&hl->hln_q_in, m0_ha_msg_tag(msg));
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE("hl=%p msg=%p tag=%"PRIu64, hl, msg, m0_ha_msg_tag(msg));
}

M0_INTERNAL bool m0_ha_link_msg_is_delivered(struct m0_ha_link *hl,
					     uint64_t           tag)
{
	bool delivered;

	m0_mutex_lock(&hl->hln_lock);
	delivered = m0_ha_lq_is_delivered(&hl->hln_q_out, tag);
	m0_mutex_unlock(&hl->hln_lock);
	return delivered;
}

M0_INTERNAL uint64_t m0_ha_link_delivered_consume(struct m0_ha_link *hl)
{
	uint64_t tag;

	m0_mutex_lock(&hl->hln_lock);
	tag = hl->hln_no_new_delivered ? M0_HA_MSG_TAG_INVALID :
	      m0_ha_lq_dequeue(&hl->hln_q_out);
	m0_mutex_unlock(&hl->hln_lock);
	M0_LOG(M0_DEBUG, "hl=%p tag=%"PRIu64, hl, tag);
	return tag;
}

M0_INTERNAL uint64_t m0_ha_link_not_delivered_consume(struct m0_ha_link *hl)
{
	uint64_t tag;

	m0_mutex_lock(&hl->hln_lock);
	tag = !hl->hln_no_new_delivered ? M0_HA_MSG_TAG_INVALID :
	      m0_ha_lq_dequeue(&hl->hln_q_out);
	m0_mutex_unlock(&hl->hln_lock);
	M0_LOG(M0_DEBUG, "hl=%p tag=%"PRIu64, hl, tag);
	return tag;
}

struct ha_link_wait_ctx {
	struct m0_ha_link  *hwc_hl;
	uint64_t            hwc_tag;
	struct m0_clink     hwc_clink;
	struct m0_semaphore hwc_sem;
	bool                hwc_check_disable;
};

static void ha_link_wait(struct ha_link_wait_ctx *wait_ctx,
					  bool (*check)(struct m0_clink *clink))
{
	int rc;

	m0_clink_init(&wait_ctx->hwc_clink, check);
	rc = m0_semaphore_init(&wait_ctx->hwc_sem, 0);
	M0_ASSERT(rc == 0);     /* XXX */
	m0_clink_add_lock(m0_ha_link_chan(wait_ctx->hwc_hl),
			  &wait_ctx->hwc_clink);
	check(&wait_ctx->hwc_clink);
	m0_semaphore_down(&wait_ctx->hwc_sem);
	m0_clink_del_lock(&wait_ctx->hwc_clink);
	m0_semaphore_fini(&wait_ctx->hwc_sem);
	m0_clink_fini(&wait_ctx->hwc_clink);
}

static bool ha_link_wait_delivery_check(struct m0_clink *clink)
{
	struct ha_link_wait_ctx *wait_ctx;

	/* XXX bob_of */
	wait_ctx = container_of(clink, struct ha_link_wait_ctx, hwc_clink);
	if (!wait_ctx->hwc_check_disable &&
	    m0_ha_link_msg_is_delivered(wait_ctx->hwc_hl, wait_ctx->hwc_tag)) {
		wait_ctx->hwc_check_disable = true;
		m0_semaphore_up(&wait_ctx->hwc_sem);
	}
	return false;
}

M0_INTERNAL void m0_ha_link_wait_delivery(struct m0_ha_link *hl, uint64_t tag)
{
	bool                    delivered;
	struct ha_link_wait_ctx wait_ctx = {
		.hwc_hl            = hl,
		.hwc_tag           = tag,
		.hwc_check_disable = false,
	};

	M0_ENTRY("hl=%p tag=%"PRIu64, hl, tag);
	ha_link_wait(&wait_ctx, &ha_link_wait_delivery_check);
	delivered = m0_ha_link_msg_is_delivered(hl, tag);
	M0_ASSERT(delivered);
	M0_LEAVE("hl=%p tag=%"PRIu64, hl, tag);
}

static bool ha_link_wait_arrival_check(struct m0_clink *clink)
{
	struct ha_link_wait_ctx *wait_ctx;
	bool                     arrived;
	struct m0_ha_link       *hl;

	/* XXX bob_of */
	wait_ctx = container_of(clink, struct ha_link_wait_ctx, hwc_clink);
	hl = wait_ctx->hwc_hl;
	M0_ENTRY("hl=%p", hl);
	if (!wait_ctx->hwc_check_disable) {
		m0_mutex_lock(&hl->hln_lock);
		arrived = m0_ha_lq_has_next(&hl->hln_q_in);
		m0_mutex_unlock(&hl->hln_lock);
		if (arrived) {
			wait_ctx->hwc_check_disable = true;
			m0_semaphore_up(&wait_ctx->hwc_sem);
		}
		M0_LOG(M0_DEBUG, "hl=%p arrived=%d", hl, !!arrived);
	}
	M0_LEAVE("hl=%p", hl);
	return false;
}

M0_INTERNAL void m0_ha_link_wait_arrival(struct m0_ha_link *hl)
{
	bool                    arrived;
	struct ha_link_wait_ctx wait_ctx = {
		.hwc_hl            = hl,
		.hwc_check_disable = false,
	};

	M0_ENTRY("hl=%p", hl);
	ha_link_wait(&wait_ctx, &ha_link_wait_arrival_check);
	m0_mutex_lock(&hl->hln_lock);
	arrived = m0_ha_lq_has_next(&hl->hln_q_in);
	m0_mutex_unlock(&hl->hln_lock);
	M0_ASSERT(arrived);
	M0_LEAVE("hl=%p", hl);
}


static bool ha_link_wait_confirmation_check(struct m0_clink *clink)
{
	struct ha_link_wait_ctx *wait_ctx;
	bool                     delivered_notified;
	struct m0_ha_link       *hl;

	/* XXX bob_of */
	wait_ctx = container_of(clink, struct ha_link_wait_ctx, hwc_clink);
	hl = wait_ctx->hwc_hl;
	M0_ENTRY("hl=%p", hl);
	if (!wait_ctx->hwc_check_disable) {
		m0_mutex_lock(&hl->hln_lock);
		delivered_notified = wait_ctx->hwc_tag <
				     m0_ha_lq_tag_confirmed(&hl->hln_q_in);
		m0_mutex_unlock(&hl->hln_lock);
		if (delivered_notified) {
			wait_ctx->hwc_check_disable = true;
			m0_semaphore_up(&wait_ctx->hwc_sem);
		}
		M0_LOG(M0_DEBUG, "hl=%p delivered_notified=%d",
		       hl, !!delivered_notified);
	}
	M0_LEAVE("hl=%p", hl);
	return false;
}

M0_INTERNAL void m0_ha_link_wait_confirmation(struct m0_ha_link *hl,
                                              uint64_t           tag)
{
	struct ha_link_wait_ctx wait_ctx = {
		.hwc_hl            = hl,
		.hwc_tag           = tag,
		.hwc_check_disable = false,
	};

	M0_ENTRY("hl=%p tag=%"PRIu64, hl, tag);
	ha_link_wait(&wait_ctx, &ha_link_wait_confirmation_check);
	M0_LEAVE("hl=%p", hl);
}

M0_INTERNAL void m0_ha_link_flush(struct m0_ha_link *hl)
{
	uint64_t tag_out_assign;
	uint64_t tag_in_assign;

	M0_ENTRY("hl=%p", hl);

	m0_mutex_lock(&hl->hln_lock);
	tag_out_assign = m0_ha_lq_tag_assign(&hl->hln_q_out);
	tag_in_assign  = m0_ha_lq_tag_assign(&hl->hln_q_in);
	m0_mutex_unlock(&hl->hln_lock);

	if (!M0_IN(tag_out_assign, (1, 2)))
		m0_ha_link_wait_delivery(hl, tag_out_assign - 2);
	if (!M0_IN(tag_in_assign, (1, 2)))
		m0_ha_link_wait_confirmation(hl, tag_in_assign - 2);
	M0_LEAVE("hl=%p tag_out_assign=%"PRIu64" tag_in_assign=%"PRIu64,
		 hl, tag_out_assign, tag_in_assign);
}

M0_INTERNAL void m0_ha_link_cb_disconnecting(struct m0_ha_link *hl)
{
	M0_ENTRY("hl=%p", hl);
	m0_mutex_lock(&hl->hln_lock);
	M0_ASSERT(!hl->hln_cb_disconnecting);
	hl->hln_cb_disconnecting = true;
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE("hl=%p", hl);
}

M0_INTERNAL void m0_ha_link_cb_reused(struct m0_ha_link *hl)
{
	M0_ENTRY("hl=%p", hl);
	m0_mutex_lock(&hl->hln_lock);
	M0_ASSERT(!hl->hln_cb_reused);
	hl->hln_cb_reused = true;
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE("hl=%p", hl);
}

static void ha_link_tags_update(struct m0_ha_link *hl,
                                uint64_t           out_next,
                                uint64_t           in_delivered)
{
	struct m0_ha_link_tags tags_out;
	struct m0_ha_link_tags tags_in;
	uint64_t               delivered;

	M0_ENTRY("hl=%p out_next=%"PRIu64" in_delivered=%"PRIu64,
	         hl, out_next, in_delivered);
	M0_PRE(m0_mutex_is_locked(&hl->hln_lock));
	m0_ha_lq_tags_get(&hl->hln_q_out, &tags_out);
	m0_ha_lq_tags_get(&hl->hln_q_in,  &tags_in);
	M0_LOG(M0_DEBUG, "hl=%p out="HLTAGS_F, hl, HLTAGS_P(&tags_out));
	M0_LOG(M0_DEBUG, "hl=%p  in="HLTAGS_F, hl, HLTAGS_P(&tags_in));

	while (m0_ha_lq_tag_next(&hl->hln_q_out) < in_delivered)
		(void)m0_ha_lq_next(&hl->hln_q_out);

	delivered = m0_ha_lq_tag_delivered(&hl->hln_q_out);
	while (delivered < in_delivered) {
		m0_ha_lq_mark_delivered(&hl->hln_q_out, delivered);
		delivered += 2;
	}

	m0_ha_lq_tags_get(&hl->hln_q_out, &tags_out);
	m0_ha_lq_tags_get(&hl->hln_q_in,  &tags_in);
	M0_LOG(M0_DEBUG, "hl=%p out="HLTAGS_F, hl, HLTAGS_P(&tags_out));
	M0_LOG(M0_DEBUG, "hl=%p  in="HLTAGS_F, hl, HLTAGS_P(&tags_in));
}

static void ha_link_tags_in_out(struct m0_ha_link *hl,
                                uint64_t          *out_next,
                                uint64_t          *in_delivered)
{
	M0_PRE(m0_mutex_is_locked(&hl->hln_lock));

	*out_next     = m0_ha_lq_tag_next(&hl->hln_q_out);
	*in_delivered = m0_ha_lq_tag_delivered(&hl->hln_q_in);
	M0_LOG(M0_DEBUG, "out_next=%"PRIu64" in_delivered=%"PRIu64,
	       *out_next, *in_delivered);
}

static void ha_link_msg_received(struct m0_ha_link      *hl,
                                 const struct m0_ha_msg *msg)
{
	M0_ENTRY("hl=%p tag=%"PRIu64" type=%d",
		 hl, m0_ha_msg_tag(msg), m0_ha_msg_type_get(msg));
	M0_PRE(m0_mutex_is_locked(&hl->hln_lock));
	if (!M0_IN(m0_ha_msg_tag(msg), (M0_HA_MSG_TAG_UNKNOWN,
	                                m0_ha_lq_tag_assign(&hl->hln_q_in)))) {
		M0_LOG(M0_WARN, "dropping out-of-order message: hl=%p "
		       "tag=%"PRIu64" hed_type=%d",
		       hl, m0_ha_msg_tag(msg), m0_ha_msg_type_get(msg));
	} else {
		m0_ha_lq_enqueue(&hl->hln_q_in, msg);
	}
}

static void ha_link_msg_recv_or_delivery_broadcast(struct m0_ha_link *hl)
{
	uint64_t in_next;
	uint64_t out_confirmed;
	uint64_t tag_recv;
	uint64_t tag_delivery;

	m0_mutex_lock(&hl->hln_lock);
	in_next = m0_ha_lq_tag_next(&hl->hln_q_in);
	tag_recv = in_next > 2 ? in_next :
		   in_next == m0_ha_lq_tag_assign(&hl->hln_q_in) ? 0 : in_next;
	out_confirmed = m0_ha_lq_tag_confirmed(&hl->hln_q_out);
	tag_delivery = out_confirmed > 2 ? out_confirmed :
		       out_confirmed == m0_ha_lq_tag_delivered(&hl->hln_q_out) ?
		       0 : out_confirmed;
	m0_mutex_unlock(&hl->hln_lock);

	m0_sm_group_lock(&hl->hln_sm_group);
	M0_LOG(M0_DEBUG, "hln_tag_broadcast_recv=%"PRIu64" tag_recv=%"PRIu64,
	       hl->hln_tag_broadcast_recv, tag_recv);
	M0_LOG(M0_DEBUG, "hln_tag_broadcast_delivery=%"PRIu64" "
	       "tag_delivery=%"PRIu64,
	       hl->hln_tag_broadcast_delivery, tag_delivery);
	if (hl->hln_tag_broadcast_recv <= tag_recv) {
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_RECV);
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_IDLE);
		hl->hln_tag_broadcast_recv = tag_recv;
	}
	if (hl->hln_tag_broadcast_delivery <= tag_delivery) {
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_DELIVERY);
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_IDLE);
		hl->hln_tag_broadcast_delivery = tag_delivery;
	}
	m0_sm_group_unlock(&hl->hln_sm_group);
}

struct ha_link_incoming_fom {
	struct m0_ha_link *hli_hl;
	struct m0_fom      hli_fom;
};

static struct ha_link_incoming_fom *
ha_link_incoming_fom_container(struct m0_fom *fom)
{
	/* TODO bob_of */
	return container_of(fom, struct ha_link_incoming_fom, hli_fom);
}

static int ha_link_incoming_fom_tick(struct m0_fom *fom)
{
	struct m0_ha_link_msg_fop     *req_fop;
	struct m0_ha_link_msg_rep_fop *rep_fop;
	struct ha_link_incoming_fom   *hli;
	struct m0_ha_link             *hl;
	struct m0_uint128              id_connection;
	struct m0_ha_msg              *msg;
	const char                    *ep;

	req_fop = m0_fop_data(fom->fo_fop);
	rep_fop = m0_fop_data(fom->fo_rep_fop);
	hli = ha_link_incoming_fom_container(fom);
	ep = m0_rpc_conn_addr(
	                 m0_fop_to_rpc_item(fom->fo_fop)->ri_session->s_conn);
	M0_ENTRY("fom=%p req_fop=%p rep_fop=%p ep=%s",
		 fom, req_fop, rep_fop, ep);
	M0_LOG(M0_DEBUG, "ep=%p lmf_msg_nr=%"PRIu64" lmf_id_remote="U128X_F" "
	       "lmf_id_local="U128X_F" lmf_id_connection="U128X_F,
	       ep, req_fop->lmf_msg_nr, U128_P(&req_fop->lmf_id_remote),
	       U128_P(&req_fop->lmf_id_local),
	       U128_P(&req_fop->lmf_id_connection));

	hl = m0_ha_link_service_find_get(fom->fo_service,
					 &req_fop->lmf_id_remote,
	                                 &id_connection);
	hli->hli_hl = hl;
	M0_LOG(M0_DEBUG, "fom=%p hl=%p", fom, hl);
	if (req_fop->lmf_msg_nr != 0) {
		msg = &req_fop->lmf_msg;
		M0_LOG(M0_DEBUG, "ep=%s lmf_id_remote="U128X_F" "
		       "hm_fid="FID_F" hed_type=%d tag=%"PRIu64,
		       ep, U128_P(&req_fop->lmf_id_remote), FID_P(&msg->hm_fid),
		       m0_ha_msg_type_get(msg), m0_ha_msg_tag(msg));
	}
	if (hl == NULL) {
		/*
		 * The first M0_LOG() parameter must be a compile-time
		 * constant.
		 */
		if (req_fop->lmf_seq <= HA_LINK_SUPPRESS_START_NR) {
			M0_LOG(M0_DEBUG, "no such link: ep=%s "
			       "lmf_id_remote="U128X_F" lmf_seq=%"PRIu64,
			       ep, U128_P(&req_fop->lmf_id_remote),
			       req_fop->lmf_seq);
		} else {
			M0_LOG(M0_WARN, "no such link: ep=%s "
			       "lmf_id_remote="U128X_F" lmf_seq=%"PRIu64,
			       ep, U128_P(&req_fop->lmf_id_remote),
			       req_fop->lmf_seq);
		}
		rep_fop->lmr_rc = -ENOLINK;
	} else if (!m0_uint128_eq(&id_connection,
				  &req_fop->lmf_id_connection)) {
		/* link exists but connection ID doesn't match */
		M0_LOG(M0_WARN, "connection ID doesn't match: "
		       "id_connection="U128X_F" lmf_id_connection="U128X_F,
		       U128_P(&id_connection),
		       U128_P(&req_fop->lmf_id_connection));
		rep_fop->lmr_rc = -EBADSLT;
	} else {
		m0_mutex_lock(&hl->hln_lock);
		if (req_fop->lmf_msg_nr != 0)
			ha_link_msg_received(hl, &req_fop->lmf_msg);
		if (!hl->hln_no_new_delivered) {
			ha_link_tags_update(hl, req_fop->lmf_out_next,
			                    req_fop->lmf_in_delivered);
		}
		ha_link_tags_in_out(hl, &rep_fop->lmr_out_next,
		                    &rep_fop->lmr_in_delivered);
		m0_mutex_unlock(&hl->hln_lock);
		ha_link_msg_recv_or_delivery_broadcast(hl);
		rep_fop->lmr_rc = 0;
	}
	M0_LOG(M0_DEBUG, "hl=%p lmr_rc=%"PRIu32, hl, rep_fop->lmr_rc);

        m0_rpc_reply_post(m0_fop_to_rpc_item(fom->fo_fop),
                          m0_fop_to_rpc_item(fom->fo_rep_fop));
        m0_fom_phase_set(fom, M0_FOPH_FINISH);
        return M0_FSO_WAIT;
}

static void ha_link_incoming_fom_fini(struct m0_fom *fom)
{
	struct ha_link_incoming_fom *hli = ha_link_incoming_fom_container(fom);

	m0_fom_fini(fom);
	if (hli->hli_hl != NULL)
		m0_ha_link_service_put(hli->hli_fom.fo_service, hli->hli_hl);
	m0_free(hli);
}

static size_t ha_link_incoming_fom_locality(const struct m0_fom *fom)
{
	return 0;
}

const struct m0_fom_ops ha_link_incoming_fom_ops = {
	.fo_tick          = &ha_link_incoming_fom_tick,
	.fo_fini          = &ha_link_incoming_fom_fini,
	.fo_home_locality = &ha_link_incoming_fom_locality,
};

static int ha_link_incoming_fom_create(struct m0_fop   *fop,
                                       struct m0_fom  **m,
                                       struct m0_reqh  *reqh)
{
	struct ha_link_incoming_fom   *hli;
	struct m0_fom                 *fom;
	struct m0_ha_link_msg_rep_fop *reply;

	M0_PRE(fop != NULL);
	M0_PRE(m != NULL);

	M0_ALLOC_PTR(hli);
	if (hli == NULL)
		return M0_ERR(-ENOMEM);
	fom = &hli->hli_fom;

	M0_ALLOC_PTR(reply);
	if (reply == NULL) {
		m0_free(hli);
		return M0_ERR(-ENOMEM);
	}

	fom->fo_rep_fop = m0_fop_alloc(&m0_ha_link_msg_rep_fopt,
				       reply, m0_fop_rpc_machine(fop));
	if (fom->fo_rep_fop == NULL) {
		m0_free(reply);
		m0_free(hli);
		return M0_ERR(-ENOMEM);
	}

	m0_fom_init(fom, &fop->f_type->ft_fom_type, &ha_link_incoming_fom_ops,
		    fop, fom->fo_rep_fop, reqh);

	*m = fom;
	return M0_RC(0);
}

const struct m0_fom_type_ops m0_ha_link_incoming_fom_type_ops = {
	.fto_create = &ha_link_incoming_fom_create,
};

enum ha_link_outgoing_fom_state {
	HA_LINK_OUTGOING_STATE_INIT   = M0_FOM_PHASE_INIT,
	HA_LINK_OUTGOING_STATE_FINISH = M0_FOM_PHASE_FINISH,
	HA_LINK_OUTGOING_STATE_INCOMING_REGISTER,
	HA_LINK_OUTGOING_STATE_INCOMING_DEREGISTER,
	HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE,
	HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE_WAIT,
	HA_LINK_OUTGOING_STATE_RPC_LINK_INIT,
	HA_LINK_OUTGOING_STATE_RPC_LINK_FINI,
	HA_LINK_OUTGOING_STATE_NOT_CONNECTED,
	HA_LINK_OUTGOING_STATE_CONNECT,
	HA_LINK_OUTGOING_STATE_CONNECTING,
	HA_LINK_OUTGOING_STATE_DISCONNECT,
	HA_LINK_OUTGOING_STATE_DISCONNECTING,
	HA_LINK_OUTGOING_STATE_IDLE,
	HA_LINK_OUTGOING_STATE_SEND,
	HA_LINK_OUTGOING_STATE_WAIT_REPLY,
	HA_LINK_OUTGOING_STATE_WAIT_RELEASE,
	HA_LINK_OUTGOING_STATE_NR,
};

static struct m0_sm_state_descr
ha_link_outgoing_fom_states[HA_LINK_OUTGOING_STATE_NR] = {
#define _ST(name, flags, allowed)      \
	[name] = {                    \
		.sd_flags   = flags,  \
		.sd_name    = #name,  \
		.sd_allowed = allowed \
	}
	_ST(HA_LINK_OUTGOING_STATE_INIT, M0_SDF_INITIAL,
	   M0_BITS(HA_LINK_OUTGOING_STATE_RPC_LINK_INIT)),
	_ST(HA_LINK_OUTGOING_STATE_RPC_LINK_INIT, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_NOT_CONNECTED)),
	_ST(HA_LINK_OUTGOING_STATE_RPC_LINK_FINI, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_FINISH)),
	_ST(HA_LINK_OUTGOING_STATE_NOT_CONNECTED, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_CONNECT,
	           HA_LINK_OUTGOING_STATE_RPC_LINK_FINI)),
	_ST(HA_LINK_OUTGOING_STATE_CONNECT, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_INCOMING_REGISTER)),
	_ST(HA_LINK_OUTGOING_STATE_INCOMING_REGISTER, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_CONNECTING)),
	_ST(HA_LINK_OUTGOING_STATE_CONNECTING, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_IDLE)),
	_ST(HA_LINK_OUTGOING_STATE_IDLE, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_SEND,
	           HA_LINK_OUTGOING_STATE_DISCONNECT)),
	_ST(HA_LINK_OUTGOING_STATE_SEND, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_WAIT_REPLY)),
	_ST(HA_LINK_OUTGOING_STATE_WAIT_REPLY, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_WAIT_RELEASE)),
	_ST(HA_LINK_OUTGOING_STATE_WAIT_RELEASE, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_IDLE)),
	_ST(HA_LINK_OUTGOING_STATE_DISCONNECT, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE)),
	_ST(HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE_WAIT)),
	_ST(HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE_WAIT, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_INCOMING_DEREGISTER)),
	_ST(HA_LINK_OUTGOING_STATE_INCOMING_DEREGISTER, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_DISCONNECTING)),
	_ST(HA_LINK_OUTGOING_STATE_DISCONNECTING, 0,
	   M0_BITS(HA_LINK_OUTGOING_STATE_NOT_CONNECTED)),
	_ST(HA_LINK_OUTGOING_STATE_FINISH, M0_SDF_TERMINAL, 0),
#undef _ST
};

const static struct m0_sm_conf ha_link_outgoing_fom_conf = {
	.scf_name      = "ha_link_outgoing_fom",
	.scf_nr_states = ARRAY_SIZE(ha_link_outgoing_fom_states),
	.scf_state     = ha_link_outgoing_fom_states,
};

static void ha_link_outgoing_item_sent(struct m0_rpc_item *item)
{
	struct m0_ha_link *hl;

	/* XXX bob_of */
	hl = container_of(container_of(item, struct m0_fop, f_item),
	                  struct m0_ha_link, hln_outgoing_fop);
	M0_ENTRY("hl=%p item=%p", hl, item);
	M0_LEAVE();
}

static void ha_link_outgoing_item_replied(struct m0_rpc_item *item)
{
	struct m0_ha_link *hl;
	int                rc;

	/* XXX bob_of */
	hl = container_of(container_of(item, struct m0_fop, f_item),
	                  struct m0_ha_link, hln_outgoing_fop);
	M0_ENTRY("hl=%p item=%p ri_error=%"PRIi32, hl, item, item->ri_error);
	m0_mutex_lock(&hl->hln_lock);
	M0_ASSERT(!hl->hln_replied);
	hl->hln_replied = true;
	hl->hln_rpc_rc  = item->ri_error ?:
			  m0_rpc_item_generic_reply_rc(item->ri_reply);
	rc = hl->hln_rpc_rc;
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE("hl=%p item=%p rc=%d", hl, item, rc);
}

const static struct m0_rpc_item_ops ha_link_outgoing_item_ops = {
	.rio_sent    = &ha_link_outgoing_item_sent,
	.rio_replied = &ha_link_outgoing_item_replied,
};

static void ha_link_outgoing_fop_release(struct m0_ref *ref)
{
	struct m0_ha_link *hl;
	struct m0_fop     *fop = container_of(ref, struct m0_fop, f_ref);

	/* XXX bob_of */
	hl = container_of(fop, struct m0_ha_link, hln_outgoing_fop);
	M0_ENTRY("hl=%p fop=%p", hl, fop);
	fop->f_data.fd_data = NULL;
	m0_fop_fini(fop);
	m0_mutex_lock(&hl->hln_lock);
	M0_ASSERT(!hl->hln_released);
	hl->hln_released = true;
	m0_mutex_unlock(&hl->hln_lock);
	ha_link_outgoing_fom_wakeup(hl);
	M0_LEAVE();
}

static int ha_link_outgoing_fop_send(struct m0_ha_link *hl)
{
	struct m0_ha_link_msg_fop *req_fop = &hl->hln_req_fop_data;
	struct m0_ha_link_params  *params;
	struct m0_rpc_item        *item;

	M0_ENTRY("hl=%p", hl);
	M0_SET0(&hl->hln_outgoing_fop);
	M0_SET0(req_fop);
	m0_fop_init(&hl->hln_outgoing_fop, &m0_ha_link_msg_fopt,
	            req_fop, &ha_link_outgoing_fop_release);

	if (hl->hln_msg_to_send == NULL) {
		req_fop->lmf_msg_nr = 0;
	} else {
		req_fop->lmf_msg_nr = 1;
		req_fop->lmf_msg    = *hl->hln_msg_to_send;
	}
	m0_mutex_lock(&hl->hln_lock);
	/* TODO use designated initialiser after m0_ha_msg become small */
	params = &hl->hln_conn_cfg.hlcc_params;
	req_fop->lmf_id_local       = params->hlp_id_local;
	req_fop->lmf_id_remote      = params->hlp_id_remote;
	req_fop->lmf_id_connection  = params->hlp_id_connection;
	req_fop->lmf_seq            = ++hl->hln_req_fop_seq;
	ha_link_tags_in_out(hl, &req_fop->lmf_out_next,
	                    &req_fop->lmf_in_delivered);
	M0_LOG(M0_DEBUG, "lmf_id_remote="U128X_F" lmf_id_local="U128X_F" "
	       "lmf_id_connection="U128X_F" lmf_msg_nr=%"PRIu64" tag=%"PRIu64" "
	       "lmf_seq=%"PRIu64,
	       U128_P(&req_fop->lmf_id_remote),
	       U128_P(&req_fop->lmf_id_local),
	       U128_P(&req_fop->lmf_id_connection),
	       req_fop->lmf_msg_nr,
	       hl->hln_msg_to_send == NULL ?
	       M0_HA_MSG_TAG_UNKNOWN : m0_ha_msg_tag(hl->hln_msg_to_send),
	       req_fop->lmf_seq);
	m0_mutex_unlock(&hl->hln_lock);
	item = m0_fop_to_rpc_item(&hl->hln_outgoing_fop);
	item->ri_prio            = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline        = m0_time_from_now(0, 0);
	item->ri_resend_interval = hl->hln_conn_cfg.hlcc_resend_interval;
	item->ri_nr_sent_max     = hl->hln_conn_cfg.hlcc_nr_sent_max;
	item->ri_ops             = &ha_link_outgoing_item_ops;
	item->ri_session         = &hl->hln_rpc_link.rlk_sess;
	m0_rpc_post(item);
	M0_LEAVE("hl=%p", hl);
	return 0;
}

static int ha_link_outgoing_fop_replied(struct m0_ha_link *hl)
{
	struct m0_ha_link_msg_rep_fop *rep_fop;
	struct m0_ha_link_tags         tags;
	struct m0_rpc_item            *req_item;
	int                            rc;

	M0_PRE(m0_mutex_is_locked(&hl->hln_lock));

	if (hl->hln_rpc_rc == 0) {
		req_item = m0_fop_to_rpc_item(&hl->hln_outgoing_fop);
		rep_fop  = m0_fop_data(m0_rpc_item_to_fop(req_item->ri_reply));
		rc = rep_fop->lmr_rc;
		M0_LOG(M0_DEBUG, "lmr_rc=%"PRIi32, rep_fop->lmr_rc);
	} else {
		rc = hl->hln_rpc_rc;
	}
	if (rc == 0) {
		ha_link_tags_update(hl, rep_fop->lmr_out_next,
		                    rep_fop->lmr_in_delivered);
	} else {
		m0_ha_lq_try_unnext(&hl->hln_q_out);
		m0_ha_lq_tags_get(&hl->hln_q_out, &tags);
		M0_LOG(M0_WARN, "rc=%d hl=%p ep=%s lq_tags="HLTAGS_F,
		       rc, hl, hl->hln_conn_cfg.hlcc_rpc_endpoint,
		       HLTAGS_P(&tags));
	}
	return M0_RC(rc);
}

static bool ha_link_q_in_confirm_all(struct m0_ha_link *hl)
{
	bool confirmed_updated = false;

	M0_PRE(m0_mutex_is_locked(&hl->hln_lock));
	while (m0_ha_lq_dequeue(&hl->hln_q_in) != M0_HA_MSG_TAG_INVALID)
		confirmed_updated = true;
	return confirmed_updated;
}

static void ha_link_cb_disconnecting_reused(struct m0_ha_link *hl)
{
	bool cb_disconnecting;
	bool cb_reused;

	M0_ENTRY("hl=%p", hl);
	m0_mutex_lock(&hl->hln_lock);
	cb_disconnecting = hl->hln_cb_disconnecting;
	cb_reused        = hl->hln_cb_reused;
	hl->hln_cb_disconnecting = false;
	hl->hln_cb_reused        = false;
	m0_mutex_unlock(&hl->hln_lock);
	if (cb_disconnecting) {
		m0_sm_group_lock(&hl->hln_sm_group);
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_DISCONNECTING);
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_IDLE);
		m0_sm_group_unlock(&hl->hln_sm_group);
	}
	if (cb_reused) {
		m0_sm_group_lock(&hl->hln_sm_group);
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_LINK_REUSED);
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_IDLE);
		m0_sm_group_unlock(&hl->hln_sm_group);
	}
	M0_LEAVE("hl=%p cb_disconnecting=%d cb_reused=%d",
	         hl, !!cb_disconnecting, !!cb_reused);
}

static void ha_link_outgoing_reconnect_timeout(struct m0_sm_timer *timer)
{
	struct m0_ha_link *hl = container_of(timer, struct m0_ha_link,
					     hln_reconnect_wait_timer);

	m0_mutex_lock(&hl->hln_lock);
	hl->hln_reconnect_wait = false;
	m0_mutex_unlock(&hl->hln_lock);

	ha_link_outgoing_fom_wakeup(hl);
}

static int ha_link_outgoing_fom_tick(struct m0_fom *fom)
{
	enum ha_link_outgoing_fom_state  phase;
	struct m0_ha_link               *hl;
	m0_time_t                        abs_timeout;
	bool                             replied;
	bool                             released;
	bool                             stopping;
	bool                             rpc_event_occurred;
	bool                             reconnect;
	bool                             reconnect_wait;
	bool                             quiesced;
	int                              reply_rc;
	int                              rc;

	hl = container_of(fom, struct m0_ha_link, hln_fom); /* XXX bob_of */
	phase = m0_fom_phase(&hl->hln_fom);
	M0_ENTRY("hl=%p phase=%s", hl, m0_fom_phase_name(&hl->hln_fom, phase));

	switch (phase) {
	case HA_LINK_OUTGOING_STATE_INIT:
		hl->hln_msg_to_send      = NULL;
		hl->hln_confirmed_update = false;
		hl->hln_rpc_rc           = 0;
		hl->hln_reply_rc         = 0;
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_RPC_LINK_INIT);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_RPC_LINK_INIT:
		m0_sm_group_lock(&hl->hln_sm_group);
		hl->hln_tag_broadcast_recv     = 0;
		hl->hln_tag_broadcast_delivery = 0;
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_START);
		m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_IDLE);
		m0_sm_group_unlock(&hl->hln_sm_group);
		rc = m0_rpc_link_init(&hl->hln_rpc_link,
		                      hl->hln_cfg.hlc_rpc_machine,
		                      &hl->hln_conn_cfg.hlcc_rpc_service_fid,
		                      hl->hln_conn_cfg.hlcc_rpc_endpoint,
		                      hl->hln_conn_cfg.hlcc_max_rpcs_in_flight);
		M0_ASSERT(rc == 0);     /* XXX handle it */
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_NOT_CONNECTED);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_RPC_LINK_FINI:
		if (m0_sm_timer_is_armed(&hl->hln_reconnect_wait_timer))
			m0_sm_timer_cancel(&hl->hln_reconnect_wait_timer);
		m0_rpc_link_fini(&hl->hln_rpc_link);
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_FINISH);
		return M0_RC(M0_FSO_WAIT);
	case HA_LINK_OUTGOING_STATE_NOT_CONNECTED:
		ha_link_cb_disconnecting_reused(hl);
		m0_mutex_lock(&hl->hln_lock);
		stopping = hl->hln_fom_is_stopping;
		m0_mutex_unlock(&hl->hln_lock);
		if (!stopping) {
			m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_CONNECT);
			return M0_RC(M0_FSO_AGAIN);
		} else {
			m0_mutex_lock(&hl->hln_lock);
			hl->hln_fom_enable_wakeup = false;
			hl->hln_no_new_delivered = true;
			while (m0_ha_lq_next(&hl->hln_q_out) != NULL)
				;
			while (m0_ha_lq_tag_delivered(&hl->hln_q_out) <
			       m0_ha_lq_tag_next(&hl->hln_q_out)) {
				m0_ha_lq_mark_delivered(&hl->hln_q_out,
					m0_ha_lq_tag_delivered(&hl->hln_q_out));
			}
			(void)ha_link_q_in_confirm_all(hl);
			m0_mutex_unlock(&hl->hln_lock);
			ha_link_msg_recv_or_delivery_broadcast(hl);
			m0_sm_group_lock(&hl->hln_sm_group);
			m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_STOP);
			m0_sm_group_unlock(&hl->hln_sm_group);
			m0_mutex_lock(&hl->hln_lock);
			if (hl->hln_reconnect_cfg_is_set) {
				ha_link_conn_cfg_free(
				                &hl->hln_conn_reconnect_cfg);
			}
			ha_link_conn_cfg_free(&hl->hln_conn_cfg);
			m0_mutex_unlock(&hl->hln_lock);
			m0_sm_ast_cancel(hl->hln_fom_locality->lo_grp,
			                 &hl->hln_waking_ast);
			m0_fom_phase_set(fom,
					 HA_LINK_OUTGOING_STATE_RPC_LINK_FINI);
			return M0_RC(M0_FSO_AGAIN);
		}
	case HA_LINK_OUTGOING_STATE_CONNECT:
		m0_mutex_lock(&hl->hln_lock);
		hl->hln_rpc_event_occurred = false;
		m0_mutex_unlock(&hl->hln_lock);
		m0_rpc_link_reset(&hl->hln_rpc_link);
		abs_timeout = m0_time_add(m0_time_now(),
		                          hl->hln_conn_cfg.hlcc_connect_timeout);
		m0_rpc_link_connect_async(&hl->hln_rpc_link, abs_timeout,
		                          &hl->hln_rpc_wait);
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_INCOMING_REGISTER);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_INCOMING_REGISTER:
		m0_mutex_lock(&hl->hln_lock);
		if (hl->hln_reconnect) {
			hl->hln_reconnect = false;
			M0_ASSERT(hl->hln_reconnect_cfg_is_set);
			ha_link_conn_cfg_free(&hl->hln_conn_cfg);
			rc = ha_link_conn_cfg_copy(&hl->hln_conn_cfg,
			                           &hl->hln_conn_reconnect_cfg);
			M0_ASSERT(rc == 0);     /* XXX */
			ha_link_conn_cfg_free(&hl->hln_conn_reconnect_cfg);
			hl->hln_reconnect_cfg_is_set = false;
			ha_link_tags_apply(hl, &hl->hln_conn_cfg.hlcc_params);
		}
		m0_ha_link_service_register(hl->hln_cfg.hlc_reqh_service, hl,
			    &hl->hln_conn_cfg.hlcc_params.hlp_id_local,
			    &hl->hln_conn_cfg.hlcc_params.hlp_id_connection);
		m0_mutex_unlock(&hl->hln_lock);
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_CONNECTING);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_CONNECTING:
		m0_mutex_lock(&hl->hln_lock);
		rpc_event_occurred = hl->hln_rpc_event_occurred;
		m0_mutex_unlock(&hl->hln_lock);
		if (rpc_event_occurred) {
			M0_ASSERT_INFO(hl->hln_rpc_link.rlk_rc == 0,
			               "rlk_rc=%d", hl->hln_rpc_link.rlk_rc);
			m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_IDLE);
			return M0_RC(M0_FSO_AGAIN);
		}
		return M0_RC(M0_FSO_WAIT);
	case HA_LINK_OUTGOING_STATE_DISCONNECT:
		m0_mutex_lock(&hl->hln_lock);
		hl->hln_rpc_event_occurred = false;
		m0_mutex_unlock(&hl->hln_lock);
		abs_timeout = m0_time_add(m0_time_now(),
				  hl->hln_conn_cfg.hlcc_disconnect_timeout);
		m0_rpc_link_disconnect_async(&hl->hln_rpc_link, abs_timeout,
		                             &hl->hln_rpc_wait);
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE:
		m0_mutex_lock(&hl->hln_lock);
		hl->hln_quiesced = false;
		m0_mutex_unlock(&hl->hln_lock);
		m0_ha_link_service_quiesce(hl->hln_cfg.hlc_reqh_service, hl,
		                           &hl->hln_quiesce_chan);
		m0_fom_phase_set(fom,
				 HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE_WAIT);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_INCOMING_QUIESCE_WAIT:
		m0_mutex_lock(&hl->hln_lock);
		quiesced = hl->hln_quiesced;
		m0_mutex_unlock(&hl->hln_lock);
		if (quiesced) {
			m0_fom_phase_set(fom,
				 HA_LINK_OUTGOING_STATE_INCOMING_DEREGISTER);
			return M0_RC(M0_FSO_AGAIN);
		}
		return M0_RC(M0_FSO_WAIT);
	case HA_LINK_OUTGOING_STATE_INCOMING_DEREGISTER:
		m0_ha_link_service_deregister(hl->hln_cfg.hlc_reqh_service, hl);
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_DISCONNECTING);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_DISCONNECTING:
		m0_mutex_lock(&hl->hln_lock);
		rpc_event_occurred = hl->hln_rpc_event_occurred;
		m0_mutex_unlock(&hl->hln_lock);
		if (rpc_event_occurred) {
			if (hl->hln_rpc_link.rlk_rc != 0) {
				M0_LOG(M0_WARN, "rlk_rc=%d endpoint=%s",
				       hl->hln_rpc_link.rlk_rc,
				      m0_rpc_link_end_point(&hl->hln_rpc_link));
			}
			m0_fom_phase_set(fom,
					 HA_LINK_OUTGOING_STATE_NOT_CONNECTED);
			return M0_RC(M0_FSO_AGAIN);
		}
		return M0_RC(M0_FSO_WAIT);
	case HA_LINK_OUTGOING_STATE_IDLE:
		M0_ASSERT(hl->hln_msg_to_send == NULL);
		hl->hln_replied  = false;
		hl->hln_released = false;
		ha_link_cb_disconnecting_reused(hl);
		if (m0_semaphore_trydown(&hl->hln_stop_cond)) {
			M0_LOG(M0_DEBUG, "stop case");
			m0_mutex_lock(&hl->hln_lock);
			hl->hln_fom_is_stopping = true;
			m0_mutex_unlock(&hl->hln_lock);
			ha_link_msg_recv_or_delivery_broadcast(hl);
			m0_fom_phase_set(fom,
					 HA_LINK_OUTGOING_STATE_DISCONNECT);
			return M0_RC(M0_FSO_AGAIN);
		}
		reply_rc = hl->hln_reply_rc;
		if (reply_rc != 0) {
			M0_LOG(M0_DEBUG, "link failed, ha_link reconnect case. "
			       "reply_rc=%d", reply_rc);
			hl->hln_reply_rc = 0;
			m0_sm_group_lock(&hl->hln_sm_group);
			m0_sm_state_set(&hl->hln_sm,
					M0_HA_LINK_STATE_LINK_FAILED);
			m0_sm_state_set(&hl->hln_sm, M0_HA_LINK_STATE_IDLE);
			m0_sm_group_unlock(&hl->hln_sm_group);

			m0_mutex_lock(&hl->hln_lock);
			hl->hln_reconnect_wait = true;
			m0_mutex_unlock(&hl->hln_lock);

			return M0_RC(M0_FSO_AGAIN);
		}
		m0_mutex_lock(&hl->hln_lock);
		reconnect_wait = hl->hln_reconnect_wait;
		m0_mutex_unlock(&hl->hln_lock);
		if (reconnect_wait) {
			time_t rtime = hl->hln_conn_cfg.hlcc_reconnect_interval;
			M0_LOG(M0_DEBUG, "link failed, reconnect wait case");
			if (m0_sm_timer_is_armed(&hl->hln_reconnect_wait_timer))
				return M0_RC(M0_FSO_WAIT);

			M0_LOG(M0_DEBUG, "link failed, reconnect wait case "
			       "armed");
			m0_sm_timer_fini(&hl->hln_reconnect_wait_timer);
			m0_sm_timer_init(&hl->hln_reconnect_wait_timer);
			rc = m0_sm_timer_start(&hl->hln_reconnect_wait_timer,
				       hl->hln_fom_locality->lo_grp,
				       ha_link_outgoing_reconnect_timeout,
				       m0_time_add(m0_time_now(), rtime));
			M0_ASSERT_INFO(rc == 0, "hl->hln_reconnect_wait_timer "
				       "failed to start, rc=%d", rc);
			return M0_RC(M0_FSO_WAIT);
		}
		m0_mutex_lock(&hl->hln_lock);
		reconnect = hl->hln_reconnect;
		m0_mutex_unlock(&hl->hln_lock);
		if (reconnect) {
			M0_LOG(M0_DEBUG, "link reconnect case");
			m0_fom_phase_set(fom,
					 HA_LINK_OUTGOING_STATE_DISCONNECT);
			return M0_RC(M0_FSO_AGAIN);
		}
		m0_mutex_lock(&hl->hln_lock);
		hl->hln_msg_to_send = m0_ha_lq_next(&hl->hln_q_out);
		hl->hln_confirmed_update = ha_link_q_in_confirm_all(hl);
		m0_mutex_unlock(&hl->hln_lock);
		if (hl->hln_msg_to_send != NULL || hl->hln_confirmed_update) {
			m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_SEND);
			return M0_RC(M0_FSO_AGAIN);
		}
		return M0_RC(M0_FSO_WAIT);
	case HA_LINK_OUTGOING_STATE_SEND:
		rc = ha_link_outgoing_fop_send(hl);
		M0_ASSERT(rc == 0);     /* XXX handle it */
		m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_WAIT_REPLY);
		return M0_RC(M0_FSO_AGAIN);
	case HA_LINK_OUTGOING_STATE_WAIT_REPLY:
		m0_mutex_lock(&hl->hln_lock);
		replied = hl->hln_replied;
		m0_mutex_unlock(&hl->hln_lock);
		if (replied) {
			m0_mutex_lock(&hl->hln_lock);
			hl->hln_reply_rc = ha_link_outgoing_fop_replied(hl);
			hl->hln_msg_to_send = NULL;
			m0_mutex_unlock(&hl->hln_lock);
			if (hl->hln_reply_rc == 0)
				hl->hln_confirmed_update = false;
			ha_link_msg_recv_or_delivery_broadcast(hl);
			m0_fop_put_lock(&hl->hln_outgoing_fop);
			m0_fom_phase_set(fom,
					 HA_LINK_OUTGOING_STATE_WAIT_RELEASE);
			return M0_RC(M0_FSO_AGAIN);
		}
		return M0_FSO_WAIT;
	case HA_LINK_OUTGOING_STATE_WAIT_RELEASE:
		m0_mutex_lock(&hl->hln_lock);
		released = hl->hln_released;
		m0_mutex_unlock(&hl->hln_lock);
		if (released) {
			m0_fom_phase_set(fom, HA_LINK_OUTGOING_STATE_IDLE);
			return M0_RC(M0_FSO_AGAIN);
		}
		return M0_FSO_WAIT;
	case HA_LINK_OUTGOING_STATE_FINISH:
	case HA_LINK_OUTGOING_STATE_NR:
		M0_IMPOSSIBLE("");
	}
        return M0_RC(M0_FSO_WAIT);
}

static void ha_link_outgoing_fom_wakeup_ast(struct m0_sm_group *gr,
                                            struct m0_sm_ast   *ast)
{
	struct m0_ha_link *hl = ast->sa_datum;

	M0_ENTRY("hl=%p", hl);
	m0_mutex_lock(&hl->hln_lock);
	hl->hln_waking_up = false;
	m0_mutex_unlock(&hl->hln_lock);
	if (m0_fom_is_waiting(&hl->hln_fom)) {
		M0_LOG(M0_DEBUG, "waking up");
		m0_fom_ready(&hl->hln_fom);
	}
	M0_LEAVE();
}

static void ha_link_outgoing_fom_wakeup(struct m0_ha_link *hl)
{
	M0_ENTRY("hl=%p", hl);
	m0_mutex_lock(&hl->hln_lock);
	if (!hl->hln_waking_up && hl->hln_fom_enable_wakeup) {
		M0_LOG(M0_DEBUG, "posting ast");
		hl->hln_waking_up = true;
		hl->hln_waking_ast = (struct m0_sm_ast){
			.sa_cb    = &ha_link_outgoing_fom_wakeup_ast,
			.sa_datum = hl,
		};
		m0_sm_ast_post(hl->hln_fom_locality->lo_grp,
			       &hl->hln_waking_ast);
	}
	m0_mutex_unlock(&hl->hln_lock);
	M0_LEAVE();
}

static void ha_link_outgoing_fom_fini(struct m0_fom *fom)
{
	struct m0_ha_link *hl;

	hl = container_of(fom, struct m0_ha_link, hln_fom); /* XXX bob_of */
	M0_ENTRY("fom=%p hl=%p", fom, hl);
	m0_fom_fini(fom);
	m0_chan_broadcast_lock(&hl->hln_stop_chan);
	M0_LEAVE();
}

static size_t ha_link_outgoing_fom_locality(const struct m0_fom *fom)
{
	return 0;
}

const struct m0_fom_ops ha_link_outgoing_fom_ops = {
	.fo_tick          = &ha_link_outgoing_fom_tick,
	.fo_fini          = &ha_link_outgoing_fom_fini,
	.fo_home_locality = &ha_link_outgoing_fom_locality,
};

static int ha_link_outgoing_fom_create(struct m0_fop   *fop,
                                       struct m0_fom  **m,
                                       struct m0_reqh  *reqh)
{
	struct m0_fom *fom;

	M0_PRE(fop != NULL);
	M0_PRE(m != NULL);

	M0_ALLOC_PTR(fom);
	if (fom == NULL)
		return M0_ERR(-ENOMEM);

	m0_fom_init(fom, &fop->f_type->ft_fom_type, &ha_link_outgoing_fom_ops,
		    fop, fom->fo_rep_fop, reqh);

	*m = fom;
	return M0_RC(0);
}

const struct m0_fom_type_ops m0_ha_link_outgoing_fom_type_ops = {
	.fto_create = &ha_link_outgoing_fom_create,
};

M0_INTERNAL struct m0_rpc_session *m0_ha_link_rpc_session(struct m0_ha_link *hl)
{
	return &hl->hln_rpc_link.rlk_sess;
}

M0_INTERNAL void m0_ha_link_rpc_endpoint(struct m0_ha_link *hl,
                                         char              *buf,
                                         m0_bcount_t        buf_len)
{
	m0_mutex_lock(&hl->hln_lock);
	strncpy(buf, hl->hln_conn_cfg.hlcc_rpc_endpoint, buf_len);
	buf[buf_len - 1] = 0;
	m0_mutex_unlock(&hl->hln_lock);
}

M0_INTERNAL int m0_ha_link_mod_init(void)
{
	int rc;

	rc = m0_ha_link_fops_init();
	M0_ASSERT(rc == 0);
	m0_fom_type_init(&ha_link_outgoing_fom_type, M0_HA_LINK_OUTGOING_OPCODE,
			 &m0_ha_link_outgoing_fom_type_ops,
			 &m0_ha_link_service_type, &ha_link_outgoing_fom_conf);
	return 0;
}

M0_INTERNAL void m0_ha_link_mod_fini(void)
{
	m0_ha_link_fops_fini();
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
