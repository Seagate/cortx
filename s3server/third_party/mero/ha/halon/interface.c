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
 * Original creation date: 5-May-2016
 */

/**
 * @addtogroup ha
 *
 * TODO log m0_halon_interface_entrypoint_reply() parameters without SIGSEGV
 * TODO handle const/non-const m0_halon_interface_entrypoint_reply() parameters
 * TODO fix 80 chars in line
 * TODO replace m0_halon_interface_internal with m0_halon_interface
 * TODO fix a race between m0_ha_flush() and m0_ha_disconnect() (rpc can send
 *      notification between these calls).
 * TODO refactor common code in accessors
 * TODO change M0_WARN to M0_INFO and make M0_INFO visible
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/halon/interface.h"

#include <stdlib.h>             /* calloc */

#include "lib/types.h"          /* m0_bcount_t */
#include "lib/misc.h"           /* M0_IS0 */
#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/bob.h"            /* M0_BOB_DEFINE */
#include "lib/errno.h"          /* ENOSYS */
#include "lib/string.h"         /* strcmp */
#include "lib/uuid.h"           /* m0_node_uuid_string_set */
#include "lib/thread.h"         /* m0_process */

#include "net/net.h"            /* M0_NET_TM_RECV_QUEUE_DEF_LEN */
#include "net/lnet/lnet.h"      /* m0_net_lnet_xprt */
#include "net/buffer_pool.h"    /* m0_net_buffer_pool */
#include "fid/fid.h"            /* m0_fid */
#include "module/instance.h"    /* m0 */
#include "reqh/reqh.h"          /* m0_reqh */
#include "reqh/reqh_service.h"  /* m0_reqh_service_setup */
#include "rpc/rpc.h"            /* m0_rpc_bufs_nr */
#include "rpc/rpc_machine.h"    /* m0_rpc_machine */
#include "sm/sm.h"              /* m0_sm */
#include "rm/rm_service.h"      /* m0_rms_type */
#include "spiel/spiel.h"        /* m0_spiel */
#include "cm/cm.h"              /* m0_sns_cm_repair_trigger_fop_init */
#include "conf/ha.h"            /* m0_conf_ha_service_event_post */
#include "conf/obj.h"           /* M0_CONF_PROCESS_TYPE */

#include "mero/init.h"          /* m0_init */
#include "mero/magic.h"         /* M0_HALON_INTERFACE_MAGIC */
#include "mero/version.h"       /* m0_build_info_get */

#include "ha/msg.h"             /* m0_ha_msg_debug_print */
#include "ha/ha.h"              /* m0_ha */
#include "ha/entrypoint_fops.h" /* m0_ha_entrypoint_req */
#include "ha/dispatcher.h"      /* m0_ha_dispatcher */
#include "ha/note.h"            /* M0_HA_NVEC_SET */


enum {
	HALON_INTERFACE_EP_BUF        = 0x40,
	HALON_INTERFACE_NVEC_SIZE_MAX = 0x1000,
};

struct m0_halon_interface_cfg {
	const char      *hic_build_git_rev_id;
	const char      *hic_build_configure_opts;
	bool             hic_disable_compat_check;
	char            *hic_local_rpc_endpoint;
	struct m0_fid    hic_process_fid;
	struct m0_fid    hic_ha_service_fid;
	struct m0_fid    hic_rm_service_fid;
	void           (*hic_entrypoint_request_cb)
		(struct m0_halon_interface         *hi,
		 const struct m0_uint128           *req_id,
		 const char                        *remote_rpc_endpoint,
		 const struct m0_fid               *process_fid,
		 const char                        *git_rev_id,
		 uint64_t                           pid,
		 bool                               first_request);
	void           (*hic_msg_received_cb)
		(struct m0_halon_interface *hi,
		 struct m0_ha_link         *hl,
		 const struct m0_ha_msg    *msg,
		 uint64_t                   tag);
	void           (*hic_msg_is_delivered_cb)
		(struct m0_halon_interface *hi,
		 struct m0_ha_link         *hl,
		 uint64_t                   tag);
	void           (*hic_msg_is_not_delivered_cb)
		(struct m0_halon_interface *hi,
		 struct m0_ha_link         *hl,
		 uint64_t                   tag);
	void           (*hic_link_connected_cb)
		(struct m0_halon_interface *hi,
		 const struct m0_uint128   *req_id,
		 struct m0_ha_link         *link);
	void           (*hic_link_reused_cb)
		(struct m0_halon_interface *hi,
		 const struct m0_uint128   *req_id,
		 struct m0_ha_link         *link);
	void           (*hic_link_absent_cb)
		(struct m0_halon_interface *hi,
		 const struct m0_uint128   *req_id);
	void           (*hic_link_is_disconnecting_cb)
		(struct m0_halon_interface *hi,
		 struct m0_ha_link         *link);
	void           (*hic_link_disconnected_cb)
		(struct m0_halon_interface *hi,
		 struct m0_ha_link         *link);

	uint32_t         hic_tm_nr;
	uint32_t         hic_bufs_nr;
	uint32_t         hic_colour;
	m0_bcount_t      hic_max_msg_size;
	uint32_t         hic_queue_len;
	struct m0_ha_cfg hic_ha_cfg;
	struct m0_ha_dispatcher_cfg hic_dispatcher_cfg;
	bool             hic_log_entrypoint;
	bool             hic_log_link;
	bool             hic_log_msg;
};

enum m0_halon_interface_level {
	M0_HALON_INTERFACE_LEVEL_ASSIGNS,
	M0_HALON_INTERFACE_LEVEL_NET_DOMAIN,
	M0_HALON_INTERFACE_LEVEL_NET_BUFFER_POOL,
	M0_HALON_INTERFACE_LEVEL_REQH_INIT,
	M0_HALON_INTERFACE_LEVEL_REQH_START,
	M0_HALON_INTERFACE_LEVEL_RPC_MACHINE,
	M0_HALON_INTERFACE_LEVEL_HA_INIT,
	M0_HALON_INTERFACE_LEVEL_DISPATCHER,
	M0_HALON_INTERFACE_LEVEL_HA_START,
	M0_HALON_INTERFACE_LEVEL_HA_CONNECT,
	M0_HALON_INTERFACE_LEVEL_INSTANCE_SET,
	M0_HALON_INTERFACE_LEVEL_EVENTS_STARTING,
	M0_HALON_INTERFACE_LEVEL_RM_SETUP,
	M0_HALON_INTERFACE_LEVEL_SPIEL_INIT,
	M0_HALON_INTERFACE_LEVEL_SNS_CM_TRIGGER_FOPS,
	M0_HALON_INTERFACE_LEVEL_EVENTS_STARTED,
	M0_HALON_INTERFACE_LEVEL_STARTED,
};

enum m0_halon_interface_state {
	M0_HALON_INTERFACE_STATE_UNINITIALISED,
	M0_HALON_INTERFACE_STATE_INITIALISED,
	M0_HALON_INTERFACE_STATE_WORKING,
	M0_HALON_INTERFACE_STATE_FINALISED,
};

struct m0_halon_interface_internal {
	struct m0_halon_interface     *hii_hi;
	struct m0                      hii_instance;
	struct m0_halon_interface_cfg  hii_cfg;
	struct m0_module               hii_module;
	struct m0_net_domain           hii_net_domain;
	struct m0_net_buffer_pool      hii_net_buffer_pool;
	struct m0_reqh                 hii_reqh;
	struct m0_rpc_machine          hii_rpc_machine;
	struct m0_ha                   hii_ha;
	struct m0_ha_dispatcher        hii_dispatcher;
	struct m0_sm_group             hii_sm_group;
	struct m0_sm                   hii_sm;
	uint64_t                       hii_magix;
	struct m0_ha_link             *hii_outgoing_link;
	struct m0_reqh_service        *hii_rm_service;
	struct m0_spiel                hii_spiel;
	uint64_t                       hii_nvec_size;
	struct m0_ha_note              hii_nvec[HALON_INTERFACE_NVEC_SIZE_MAX];
};

static const struct m0_bob_type halon_interface_bob_type = {
	.bt_name         = "halon interface",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_halon_interface_internal,
	                                   hii_magix),
	.bt_magix        = M0_HALON_INTERFACE_MAGIC,
};
M0_BOB_DEFINE(static, &halon_interface_bob_type, m0_halon_interface_internal);

static bool
halon_interface_is_compatible(struct m0_halon_interface *hi,
                              const char                *build_git_rev_id,
                              const char                *build_configure_opts,
                              bool                       disable_compat_check)
{
	const struct m0_build_info *bi = m0_build_info_get();

	M0_ENTRY("build_git_rev_id=%s build_configure_opts=%s "
	         "disable_compat_check=%d", build_git_rev_id,
		 build_configure_opts, !!disable_compat_check);
	if (disable_compat_check)
		return true;
	if (!m0_streq(bi->bi_git_rev_id, build_git_rev_id)) {
		M0_LOG(M0_ERROR, "The loaded mero library (%s) "
		       "is not the expected one (%s)", bi->bi_git_rev_id,
		       build_git_rev_id);
		return false;
	}
	if (!m0_streq(bi->bi_configure_opts, build_configure_opts)) {
		M0_LOG(M0_ERROR, "The configuration options of the loaded "
		       "mero library (%s) do not match the expected ones (%s)",
		       bi->bi_configure_opts, build_configure_opts);
		return false;
	}
	return true;
}

static void
halon_interface_parse_debug_options(struct m0_halon_interface *hi,
                                    const char                *debug_options)
{
	struct m0_halon_interface_cfg *hi_cfg = &hi->hif_internal->hii_cfg;

	M0_ENTRY("hi=%p debug_options=%s", hi, debug_options);

	if (debug_options == NULL)
		return;
	hi_cfg->hic_log_entrypoint = strstr(debug_options, "log-entrypoint")
									!= NULL;
	hi_cfg->hic_log_link       = strstr(debug_options, "log-link") != NULL;
	hi_cfg->hic_log_msg        = strstr(debug_options, "log-msg") != NULL;
}

static struct m0_halon_interface_internal *
halon_interface_ha2hii(struct m0_ha *ha)
{
	struct m0_halon_interface_internal *hii;

	hii = bob_of(ha, struct m0_halon_interface_internal, hii_ha,
	             &halon_interface_bob_type);
	M0_ASSERT(m0_get() == &hii->hii_instance);
	return hii;
}

static void
halon_interface_process_failure_check(struct m0_halon_interface_internal *hii,
                                      struct m0_ha_msg                   *msg)
{
	struct m0_ha_msg_nvec *nvec;
	struct m0_ha_note     *note;
	uint64_t               i;
	uint64_t               j;

	if (m0_ha_msg_type_get(msg) != M0_HA_MSG_NVEC)
		return;
	nvec = &msg->hm_data.u.hed_nvec;
	if (nvec->hmnv_type != M0_HA_NVEC_SET)
		return;
	for (i = 0; i < nvec->hmnv_nr; ++i) {
		note = &nvec->hmnv_arr.hmna_arr[i];
		if (m0_fid_tget(&note->no_id) ==
		    M0_CONF_PROCESS_TYPE.cot_ftype.ft_id) {
			/*
			 * XXX Detect HA state duplications here and ignore the
			 * note if HA state for the process doesn't change.
			 * This allows Hare to be simple and always send as many
			 * HA states as it wants without worrying about sending
			 * each state only once.
			 *
			 * The duplicate detection code has to be removed after
			 * Hare can send states in a proper way.
			 */
			for (j = 0; j < hii->hii_nvec_size; ++j) {
				if (m0_fid_eq(&note->no_id,
				              &hii->hii_nvec[j].no_id))
					break;
			}
			if (j == hii->hii_nvec_size) {
				M0_ASSERT_INFO(hii->hii_nvec_size <
				               ARRAY_SIZE(hii->hii_nvec),
					       "Currently we don't support "
					       "more than %zu processes "
					       "connected to a single "
					       "m0_halon_interface.",
					       ARRAY_SIZE(hii->hii_nvec));
				hii->hii_nvec[hii->hii_nvec_size] =
					(struct m0_ha_note){
						.no_id    = note->no_id,
						.no_state = M0_NC_UNKNOWN,
				};
				++hii->hii_nvec_size;
			}
			/*
			 * Ignore process state if it hasn't changed and the
			 * hmnv_ignore_same_state flag is set.
			 */
			if (note->no_state == hii->hii_nvec[j].no_state &&
			    nvec->hmnv_ignore_same_state)
				continue;
			if (note->no_state == M0_NC_FAILED &&
			    j == hii->hii_nvec_size - 1) {
				M0_LOG(M0_DEBUG, "Ignoring M0_NC_FAILED as "
				       "it's the 1st notification "
				       "for no_id="FID_F, FID_P(&note->no_id));
			}
			if (note->no_state == M0_NC_FAILED &&
			    j != hii->hii_nvec_size - 1) {
				M0_LOG(M0_DEBUG, "no_id="FID_F,
				       FID_P(&note->no_id));
				if (hii->hii_cfg.hic_log_link) {
					M0_LOG(M0_WARN, "no_id="FID_F,
					       FID_P(&note->no_id));
				}
				m0_ha_process_failed(&hii->hii_ha,
						     &note->no_id);
			}
			hii->hii_nvec[j].no_state = note->no_state;
		}
	}
}

static void
halon_interface_entrypoint_request_cb(struct m0_ha                      *ha,
                                      const struct m0_ha_entrypoint_req *req,
                                      const struct m0_uint128           *req_id)
{
	struct m0_halon_interface_internal *hii;

	hii = bob_of(ha, struct m0_halon_interface_internal, hii_ha,
	             &halon_interface_bob_type);
	M0_ENTRY("hi=%p req=%p req_id="U128X_F" remote_rpc_endpoint=%s "
	         "process_fid="FID_F,
	         hii->hii_hi, req, U128_P(req_id), req->heq_rpc_endpoint,
	         FID_P(&req->heq_process_fid));
	M0_LOG(M0_DEBUG, "git_rev_id=%s generation=%"PRIu64" pid=%"PRIu64,
	       req->heq_git_rev_id, req->heq_generation, req->heq_pid);
	if (hii->hii_cfg.hic_log_entrypoint) {
		M0_LOG(M0_WARN, "req_id="U128X_F" remote_rpc_endpoint=%s "
	         "process_fid="FID_F" git_rev_id=%s generation=%"PRIu64" "
	         "pid=%"PRIu64, U128_P(req_id), req->heq_rpc_endpoint,
	         FID_P(&req->heq_process_fid), req->heq_git_rev_id,
		 req->heq_generation, req->heq_pid);
	}
	hii->hii_cfg.hic_entrypoint_request_cb(hii->hii_hi, req_id,
	                                       req->heq_rpc_endpoint,
	                                       &req->heq_process_fid,
	                                       req->heq_git_rev_id,
	                                       req->heq_pid,
	                                       req->heq_first_request);
	M0_LEAVE();
}

static void
halon_interface_entrypoint_replied_cb(struct m0_ha                *ha,
                                      struct m0_ha_entrypoint_rep *rep)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);

	M0_ENTRY("hii=%p ha=%p rep=%p", hii, ha, rep);
	/*
	 * Nothing to do here for now.
	 * In the future some default handler should be called.
	 */
	M0_LEAVE();
}

static void halon_interface_msg_received_cb(struct m0_ha      *ha,
                                            struct m0_ha_link *hl,
                                            struct m0_ha_msg  *msg,
                                            uint64_t           tag)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hi=%p ha=%p hl=%p ep=%s msg=%p epoch=%"PRIu64" tag=%"PRIu64,
		 hii->hii_hi, &hii->hii_ha, hl, ep, msg, msg->hm_epoch, tag);
	m0_ha_msg_debug_print(msg, __func__);
	if (hl != hii->hii_outgoing_link) {
		if (hii->hii_cfg.hic_log_msg) {
			M0_LOG(M0_WARN, "hl=%p ep=%s epoch=%"PRIu64" "
			       "tag=%"PRIu64" type=%"PRIu64,
			       hl, ep, msg->hm_epoch, tag,
			       msg->hm_data.hed_type);
		}
		hii->hii_cfg.hic_msg_received_cb(hii->hii_hi, hl, msg, tag);
	} else {
		m0_ha_dispatcher_handle(&hii->hii_dispatcher, ha, hl, msg, tag);
		halon_interface_process_failure_check(hii, msg);
		m0_ha_delivered(ha, hl, msg);
	}
	M0_LEAVE("hi=%p ha=%p hl=%p ep=%s msg=%p epoch=%"PRIu64" tag=%"PRIu64,
		 hii->hii_hi, &hii->hii_ha, hl, ep, msg, msg->hm_epoch, tag);
}

static void halon_interface_msg_is_delivered_cb(struct m0_ha      *ha,
                                                struct m0_ha_link *hl,
                                                uint64_t           tag)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hii=%p ha=%p hl=%p ep=%s tag=%"PRIu64, hii, ha, hl, ep, tag);
	if (hl != hii->hii_outgoing_link) {
		if (hii->hii_cfg.hic_log_msg)
			M0_LOG(M0_WARN, "hl=%p ep=%s tag=%"PRIu64, hl, ep, tag);
		hii->hii_cfg.hic_msg_is_delivered_cb(hii->hii_hi, hl, tag);
	}
	M0_LEAVE("hii=%p ha=%p hl=%p ep=%s tag=%"PRIu64, hii, ha, hl, ep, tag);
}

static void halon_interface_msg_is_not_delivered_cb(struct m0_ha      *ha,
						    struct m0_ha_link *hl,
						    uint64_t           tag)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hii=%p ha=%p hl=%p ep=%s tag=%"PRIu64, hii, ha, hl, ep, tag);
	if (hl != hii->hii_outgoing_link) {
		if (hii->hii_cfg.hic_log_msg)
			M0_LOG(M0_WARN, "hl=%p ep=%s tag=%"PRIu64, hl, ep, tag);
		hii->hii_cfg.hic_msg_is_not_delivered_cb(hii->hii_hi, hl, tag);
	}
	M0_LEAVE("hii=%p ha=%p hl=%p ep=%s tag=%"PRIu64, hii, ha, hl, ep, tag);
}

static void halon_interface_link_connected_cb(struct m0_ha            *ha,
                                              const struct m0_uint128 *req_id,
                                              struct m0_ha_link       *hl)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hii=%p ha=%p req_id="U128X_F" hl=%p ep=%s", hii, ha,
		 U128_P(req_id), hl, ep);
	if (hii->hii_cfg.hic_log_link) {
		M0_LOG(M0_WARN, "req_id="U128X_F" hl=%p ep=%s",
		       U128_P(req_id), hl, ep);
	}
	hii->hii_cfg.hic_link_connected_cb(hii->hii_hi, req_id, hl);
	M0_LEAVE("hii=%p ha=%p req_id="U128X_F" hl=%p ep=%s", hii, ha,
		 U128_P(req_id), hl, ep);
}

static void halon_interface_link_reused_cb(struct m0_ha            *ha,
                                           const struct m0_uint128 *req_id,
                                           struct m0_ha_link       *hl)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hii=%p ha=%p req_id="U128X_F" hl=%p ep=%s", hii, ha,
		 U128_P(req_id), hl, ep);
	if (hii->hii_cfg.hic_log_link) {
		M0_LOG(M0_WARN, "req_id="U128X_F" hl=%p ep=%s",
		       U128_P(req_id), hl, ep);
	}
	hii->hii_cfg.hic_link_reused_cb(hii->hii_hi, req_id, hl);
	M0_LEAVE("hii=%p ha=%p req_id="U128X_F" hl=%p ep=%s", hii, ha,
		 U128_P(req_id), hl, ep);
}

static void halon_interface_link_absent_cb(struct m0_ha            *ha,
                                           const struct m0_uint128 *req_id)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	M0_ENTRY("hii=%p ha=%p req_id="U128X_F, hii, ha, U128_P(req_id));
	if (hii->hii_cfg.hic_log_link) {
		M0_LOG(M0_WARN, "req_id="U128X_F" ep=%s", U128_P(req_id), ep);
	}
	hii->hii_cfg.hic_link_absent_cb(hii->hii_hi, req_id);
	M0_LEAVE("hii=%p ha=%p req_id="U128X_F, hii, ha, U128_P(req_id));
}

static void halon_interface_link_is_disconnecting_cb(struct m0_ha      *ha,
						     struct m0_ha_link *hl)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hii=%p ha=%p hl=%p ep=%s", hii, ha, hl, ep);
	if (hii->hii_cfg.hic_log_link)
		M0_LOG(M0_WARN, "hl=%p ep=%s", hl, ep);
	hii->hii_cfg.hic_link_is_disconnecting_cb(hii->hii_hi, hl);
	M0_LEAVE("hii=%p ha=%p hl=%p ep=%s", hii, ha, hl, ep);
}

static void halon_interface_link_disconnected_cb(struct m0_ha      *ha,
                                                 struct m0_ha_link *hl)
{
	struct m0_halon_interface_internal *hii = halon_interface_ha2hii(ha);
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hii=%p ha=%p hl=%p ep=%s", hii, ha, hl, ep);
	if (hii->hii_cfg.hic_log_link)
		M0_LOG(M0_WARN, "hl=%p ep=%s", hl, ep);
	hii->hii_cfg.hic_link_disconnected_cb(hii->hii_hi, hl);
	M0_LEAVE("hii=%p ha=%p hl=%p ep=%s", hii, ha, hl, ep);
}

static const struct m0_ha_ops halon_interface_ha_ops = {
	.hao_entrypoint_request    = &halon_interface_entrypoint_request_cb,
	.hao_entrypoint_replied    = &halon_interface_entrypoint_replied_cb,
	.hao_msg_received          = &halon_interface_msg_received_cb,
	.hao_msg_is_delivered      = &halon_interface_msg_is_delivered_cb,
	.hao_msg_is_not_delivered  = &halon_interface_msg_is_not_delivered_cb,
	.hao_link_connected        = &halon_interface_link_connected_cb,
	.hao_link_reused           = &halon_interface_link_reused_cb,
	.hao_link_absent           = &halon_interface_link_absent_cb,
	.hao_link_is_disconnecting = &halon_interface_link_is_disconnecting_cb,
	.hao_link_disconnected     = &halon_interface_link_disconnected_cb,
};

static struct m0_sm_state_descr halon_interface_states[] = {
	[M0_HALON_INTERFACE_STATE_UNINITIALISED] = {
		.sd_flags = M0_SDF_INITIAL,
		.sd_name = "M0_HALON_INTERFACE_STATE_UNINITIALISED",
		.sd_allowed = M0_BITS(M0_HALON_INTERFACE_STATE_INITIALISED),
	},
	[M0_HALON_INTERFACE_STATE_INITIALISED] = {
		.sd_name = "M0_HALON_INTERFACE_STATE_INITIALISED",
		.sd_allowed = M0_BITS(M0_HALON_INTERFACE_STATE_WORKING,
		                      M0_HALON_INTERFACE_STATE_FINALISED),
	},
	[M0_HALON_INTERFACE_STATE_WORKING] = {
		.sd_name = "M0_HALON_INTERFACE_STATE_WORKING",
		.sd_allowed = M0_BITS(M0_HALON_INTERFACE_STATE_INITIALISED),
	},
	[M0_HALON_INTERFACE_STATE_FINALISED] = {
		.sd_flags = M0_SDF_TERMINAL,
		.sd_name = "M0_HALON_INTERFACE_STATE_FINALISED",
		.sd_allowed = 0,
	},
};

static struct m0_sm_conf halon_interface_sm_conf = {
	.scf_name      = "m0_halon_interface_internal::hii_sm",
	.scf_nr_states = ARRAY_SIZE(halon_interface_states),
	.scf_state     = halon_interface_states,
};

int m0_halon_interface_init(struct m0_halon_interface **hi_out,
                            const char                 *build_git_rev_id,
                            const char                 *build_configure_opts,
                            const char                 *debug_options,
                            const char                 *node_uuid)
{
	struct m0_halon_interface_internal *hii;
	bool                                disable_compat_check;
	int                                 rc;

	disable_compat_check = debug_options != NULL &&
		strstr(debug_options, "disable-compatibility-check") != NULL;
	if (!halon_interface_is_compatible(NULL, build_git_rev_id,
	                                   build_configure_opts,
					   disable_compat_check))
		return M0_ERR(-EINVAL);

	/* M0_ALLOC_PTR() can't be used before m0_init() */
	*hi_out = calloc(1, sizeof **hi_out);
	hii = calloc(1, sizeof *hii);
	if (*hi_out == NULL || hii == NULL) {
		free(*hi_out);
		free(hii);
		return M0_ERR(-ENOMEM);
	}
	(*hi_out)->hif_internal = hii;
	(*hi_out)->hif_internal->hii_hi = *hi_out;
	m0_halon_interface_internal_bob_init(hii);
	m0_node_uuid_string_set(node_uuid);
	rc = m0_init(&hii->hii_instance);
	if (rc != 0) {
		free(hii);
		return M0_ERR(rc);
	}
	halon_interface_parse_debug_options(*hi_out, debug_options);
	m0_sm_group_init(&hii->hii_sm_group);
	m0_sm_init(&hii->hii_sm, &halon_interface_sm_conf,
		   M0_HALON_INTERFACE_STATE_UNINITIALISED, &hii->hii_sm_group);
	m0_sm_group_lock(&hii->hii_sm_group);
	m0_sm_state_set(&hii->hii_sm, M0_HALON_INTERFACE_STATE_INITIALISED);
	m0_sm_group_unlock(&hii->hii_sm_group);
	return M0_RC(0);
}

void m0_halon_interface_fini(struct m0_halon_interface *hi)
{
	struct m0_halon_interface_internal *hii;

	M0_ENTRY("hi=%p", hi);

	hii = hi->hif_internal;
	m0_sm_group_lock(&hii->hii_sm_group);
	m0_sm_state_set(&hii->hii_sm, M0_HALON_INTERFACE_STATE_FINALISED);
	m0_sm_fini(&hii->hii_sm);
	m0_sm_group_unlock(&hii->hii_sm_group);
	m0_sm_group_fini(&hii->hii_sm_group);
	m0_fini();
	m0_halon_interface_internal_bob_fini(hii);
	free(hii);
	M0_LEAVE();
}

static void
halon_interface_process_event(struct m0_halon_interface_internal *hii,
                              enum m0_conf_ha_process_event       event)
{
	m0_conf_ha_process_event_post(&hii->hii_ha, hii->hii_outgoing_link,
	                              &hii->hii_cfg.hic_process_fid,
	                              m0_process(), event,
	                              M0_CONF_HA_PROCESS_M0D);
}

static void
halon_interface_service_event(struct m0_halon_interface_internal *hii,
                              enum m0_conf_ha_service_event       event)
{
	m0_conf_ha_service_event_post(&hii->hii_ha, hii->hii_outgoing_link,
	                              &hii->hii_cfg.hic_process_fid,
	                              &hii->hii_cfg.hic_ha_service_fid,
	                              &hii->hii_cfg.hic_ha_service_fid,
	                              m0_process(),
	                              event, M0_CST_HA);
}

static const struct m0_modlev halon_interface_levels[];

static int halon_interface_level_enter(struct m0_module *module)
{
	struct m0_halon_interface_internal *hii;
	enum m0_halon_interface_level       level = module->m_cur + 1;

	hii = bob_of(module, struct m0_halon_interface_internal, hii_module,
	             &halon_interface_bob_type);
	M0_ENTRY("hii=%p level=%d %s", hii, level,
		 halon_interface_levels[level].ml_name);
	switch (level) {
	case M0_HALON_INTERFACE_LEVEL_ASSIGNS:
		/*
		 * Zero all data structures initialised later to allow
		 * m0_halon_interface_start() after m0_halon_interface_stop().
		 */
		M0_SET0(&hii->hii_net_domain);
		M0_SET0(&hii->hii_net_buffer_pool);
		M0_SET0(&hii->hii_reqh);
		M0_SET0(&hii->hii_rpc_machine);
		M0_SET0(&hii->hii_ha);
		M0_SET0(&hii->hii_dispatcher);
		hii->hii_outgoing_link = NULL;
		hii->hii_rm_service    = NULL;
		M0_SET0(&hii->hii_spiel);
		hii->hii_nvec_size     = 0;

		hii->hii_cfg.hic_tm_nr        = 1;
		hii->hii_cfg.hic_bufs_nr      = 100;
		hii->hii_cfg.hic_colour       = M0_BUFFER_ANY_COLOUR;
		hii->hii_cfg.hic_max_msg_size = 1UL << 17;
		hii->hii_cfg.hic_queue_len    = 100;
		hii->hii_cfg.hic_ha_cfg = (struct m0_ha_cfg){
			.hcf_ops         = halon_interface_ha_ops,
			.hcf_rpc_machine = &hii->hii_rpc_machine,
			.hcf_addr        = hii->hii_cfg.hic_local_rpc_endpoint,
			.hcf_reqh        = &hii->hii_reqh,
			.hcf_process_fid = hii->hii_cfg.hic_process_fid,
		};
		hii->hii_cfg.hic_dispatcher_cfg = (struct m0_ha_dispatcher_cfg){
			.hdc_enable_note      = true,
			.hdc_enable_keepalive = true,
			.hdc_enable_fvec      = true,
		};
		return M0_RC(0);
	case M0_HALON_INTERFACE_LEVEL_NET_DOMAIN:
		return M0_RC(m0_net_domain_init(&hii->hii_net_domain,
		                                &m0_net_lnet_xprt));
	case M0_HALON_INTERFACE_LEVEL_NET_BUFFER_POOL:
		return M0_RC(m0_rpc_net_buffer_pool_setup(
		                &hii->hii_net_domain, &hii->hii_net_buffer_pool,
		                hii->hii_cfg.hic_bufs_nr,
				hii->hii_cfg.hic_tm_nr));
	case M0_HALON_INTERFACE_LEVEL_REQH_INIT:
		return M0_RC(M0_REQH_INIT(&hii->hii_reqh,
		                          .rhia_dtm          = (void*)1,
		                          .rhia_mdstore      = (void*)1,
		                          .rhia_fid          =
						&hii->hii_cfg.hic_process_fid));
	case M0_HALON_INTERFACE_LEVEL_REQH_START:
		m0_reqh_start(&hii->hii_reqh);
		return M0_RC(0);
	case M0_HALON_INTERFACE_LEVEL_RPC_MACHINE:
		return M0_RC(m0_rpc_machine_init(
		                &hii->hii_rpc_machine, &hii->hii_net_domain,
				 hii->hii_cfg.hic_local_rpc_endpoint,
				&hii->hii_reqh, &hii->hii_net_buffer_pool,
				 hii->hii_cfg.hic_colour,
				 hii->hii_cfg.hic_max_msg_size,
				 hii->hii_cfg.hic_queue_len));
	case M0_HALON_INTERFACE_LEVEL_HA_INIT:
		return M0_RC(m0_ha_init(&hii->hii_ha,
					&hii->hii_cfg.hic_ha_cfg));
	case M0_HALON_INTERFACE_LEVEL_DISPATCHER:
		return M0_RC(m0_ha_dispatcher_init(&hii->hii_dispatcher,
					   &hii->hii_cfg.hic_dispatcher_cfg));
	case M0_HALON_INTERFACE_LEVEL_HA_START:
		return M0_RC(m0_ha_start(&hii->hii_ha));
	case M0_HALON_INTERFACE_LEVEL_HA_CONNECT:
		hii->hii_outgoing_link = m0_ha_connect(&hii->hii_ha);
		M0_LOG(M0_DEBUG, "hii_outgoing_link=%p",
		       hii->hii_outgoing_link);
		return hii->hii_outgoing_link == NULL ? M0_ERR(-EINVAL) :
							M0_RC(0);
	case M0_HALON_INTERFACE_LEVEL_INSTANCE_SET:
		M0_ASSERT(m0_get()->i_ha      == NULL);
		M0_ASSERT(m0_get()->i_ha_link == NULL);
		m0_get()->i_ha      = &hii->hii_ha;
		m0_get()->i_ha_link =  hii->hii_outgoing_link;
		return M0_RC(0);
	case M0_HALON_INTERFACE_LEVEL_EVENTS_STARTING:
		halon_interface_process_event(hii, M0_CONF_HA_PROCESS_STARTING);
		halon_interface_service_event(hii, M0_CONF_HA_SERVICE_STARTING);
		halon_interface_service_event(hii, M0_CONF_HA_SERVICE_STARTED);
		return M0_RC(0);
	case M0_HALON_INTERFACE_LEVEL_RM_SETUP:
		return M0_RC(m0_reqh_service_setup(&hii->hii_rm_service,
		                                   &m0_rms_type,
		                                   &hii->hii_reqh, NULL,
					   &hii->hii_cfg.hic_rm_service_fid));
	case M0_HALON_INTERFACE_LEVEL_SPIEL_INIT:
		return M0_RC(m0_spiel_init(&hii->hii_spiel, &hii->hii_reqh));
	case M0_HALON_INTERFACE_LEVEL_SNS_CM_TRIGGER_FOPS:
		m0_sns_cm_repair_trigger_fop_init();
		m0_sns_cm_rebalance_trigger_fop_init();
		return M0_RC(0);
	case M0_HALON_INTERFACE_LEVEL_EVENTS_STARTED:
		halon_interface_process_event(hii, M0_CONF_HA_PROCESS_STARTED);
		return M0_RC(0);
	case M0_HALON_INTERFACE_LEVEL_STARTED:
		return M0_ERR(-ENOSYS);
	}
	return M0_ERR(-ENOSYS);
}

static void halon_interface_level_leave(struct m0_module *module)
{
	struct m0_halon_interface_internal *hii;
	enum m0_halon_interface_level       level = module->m_cur;

	hii = bob_of(module, struct m0_halon_interface_internal, hii_module,
	             &halon_interface_bob_type);
	M0_ENTRY("hii=%p level=%d", hii, level);
	switch (level) {
	case M0_HALON_INTERFACE_LEVEL_ASSIGNS:
		break;
	case M0_HALON_INTERFACE_LEVEL_NET_DOMAIN:
		m0_net_domain_fini(&hii->hii_net_domain);
		break;
	case M0_HALON_INTERFACE_LEVEL_NET_BUFFER_POOL:
		m0_rpc_net_buffer_pool_cleanup(&hii->hii_net_buffer_pool);
		break;
	case M0_HALON_INTERFACE_LEVEL_REQH_INIT:
		m0_reqh_fini(&hii->hii_reqh);
		break;
	case M0_HALON_INTERFACE_LEVEL_REQH_START:
		m0_reqh_services_terminate(&hii->hii_reqh);
		break;
	case M0_HALON_INTERFACE_LEVEL_RPC_MACHINE:
		m0_reqh_shutdown_wait(&hii->hii_reqh);
		m0_rpc_machine_fini(&hii->hii_rpc_machine);
		break;
	case M0_HALON_INTERFACE_LEVEL_HA_INIT:
		m0_ha_fini(&hii->hii_ha);
		break;
	case M0_HALON_INTERFACE_LEVEL_DISPATCHER:
		m0_ha_dispatcher_fini(&hii->hii_dispatcher);
		break;
	case M0_HALON_INTERFACE_LEVEL_HA_START:
		m0_ha_stop(&hii->hii_ha);
		break;
	case M0_HALON_INTERFACE_LEVEL_HA_CONNECT:
		m0_ha_flush(&hii->hii_ha, m0_ha_outgoing_link(&hii->hii_ha));
		m0_ha_disconnect(&hii->hii_ha);
		break;
	case M0_HALON_INTERFACE_LEVEL_INSTANCE_SET:
		M0_ASSERT(m0_get()->i_ha      == &hii->hii_ha);
		M0_ASSERT(m0_get()->i_ha_link ==  hii->hii_outgoing_link);
		m0_get()->i_ha      = NULL;
		m0_get()->i_ha_link = NULL;
		break;
	case M0_HALON_INTERFACE_LEVEL_EVENTS_STARTING:
		halon_interface_service_event(hii, M0_CONF_HA_SERVICE_STOPPING);
		halon_interface_service_event(hii, M0_CONF_HA_SERVICE_STOPPED);
		halon_interface_process_event(hii, M0_CONF_HA_PROCESS_STOPPED);
		break;
	case M0_HALON_INTERFACE_LEVEL_RM_SETUP:
		m0_reqh_service_quit(hii->hii_rm_service);
		break;
	case M0_HALON_INTERFACE_LEVEL_SPIEL_INIT:
		m0_spiel_fini(&hii->hii_spiel);
		break;
	case M0_HALON_INTERFACE_LEVEL_SNS_CM_TRIGGER_FOPS:
		m0_sns_cm_rebalance_trigger_fop_fini();
		m0_sns_cm_repair_trigger_fop_fini();
		break;
	case M0_HALON_INTERFACE_LEVEL_EVENTS_STARTED:
		halon_interface_process_event(hii, M0_CONF_HA_PROCESS_STOPPING);
		break;
	case M0_HALON_INTERFACE_LEVEL_STARTED:
		M0_IMPOSSIBLE("can't be here");
		break;
	}
	M0_LEAVE();
}

static const struct m0_modlev halon_interface_levels[] = {
	[M0_HALON_INTERFACE_LEVEL_ASSIGNS] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_ASSIGNS",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_NET_DOMAIN] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_NET_DOMAIN",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_NET_BUFFER_POOL] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_NET_BUFFER_POOL",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_REQH_INIT] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_REQH_INIT",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_REQH_START] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_REQH_START",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_RPC_MACHINE] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_RPC_MACHINE",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_HA_INIT] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_HA_INIT",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_DISPATCHER] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_DISPATCHER",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_HA_START] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_HA_START",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_HA_CONNECT] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_HA_CONNECT",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_INSTANCE_SET] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_INSTANCE_SET",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_EVENTS_STARTING] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_EVENTS_STARTING",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_RM_SETUP] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_RM_SETUP",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_SPIEL_INIT] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_SPIEL_INIT",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_SNS_CM_TRIGGER_FOPS] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_SNS_CM_TRIGGER_FOPS",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_EVENTS_STARTED] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_EVENTS_STARTED",
		.ml_enter = halon_interface_level_enter,
		.ml_leave = halon_interface_level_leave,
	},
	[M0_HALON_INTERFACE_LEVEL_STARTED] = {
		.ml_name  = "M0_HALON_INTERFACE_LEVEL_STARTED",
	},
};

int m0_halon_interface_start(struct m0_halon_interface *hi,
                             const char                *local_rpc_endpoint,
                             const struct m0_fid       *process_fid,
                             const struct m0_fid       *ha_service_fid,
                             const struct m0_fid       *rm_service_fid,
                             void                     (*entrypoint_request_cb)
				(struct m0_halon_interface         *hi,
				 const struct m0_uint128           *req_id,
				 const char             *remote_rpc_endpoint,
				 const struct m0_fid    *process_fid,
				 const char             *git_rev_id,
				 uint64_t                pid,
				 bool                    first_request),
			     void                     (*msg_received_cb)
				(struct m0_halon_interface *hi,
				 struct m0_ha_link         *hl,
				 const struct m0_ha_msg    *msg,
				 uint64_t                   tag),
			     void                     (*msg_is_delivered_cb)
				(struct m0_halon_interface *hi,
				 struct m0_ha_link         *hl,
				 uint64_t                   tag),
			     void                     (*msg_is_not_delivered_cb)
				(struct m0_halon_interface *hi,
				 struct m0_ha_link         *hl,
				 uint64_t                   tag),
			     void                    (*link_connected_cb)
			        (struct m0_halon_interface *hi,
				 const struct m0_uint128   *req_id,
			         struct m0_ha_link         *link),
			     void                    (*link_reused_cb)
			        (struct m0_halon_interface *hi,
				 const struct m0_uint128   *req_id,
			         struct m0_ha_link         *link),
			     void                    (*link_absent_cb)
			        (struct m0_halon_interface *hi,
				 const struct m0_uint128   *req_id),
			     void                    (*link_is_disconnecting_cb)
			        (struct m0_halon_interface *hi,
			         struct m0_ha_link         *link),
			     void                     (*link_disconnected_cb)
			        (struct m0_halon_interface *hi,
			         struct m0_ha_link         *link))
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;
	char                               *ep;
	int                                 rc;

	M0_PRE(m0_halon_interface_internal_bob_check(hii));
	M0_PRE(m0_get() == &hii->hii_instance);
	M0_PRE(process_fid    != NULL);
	M0_PRE(rm_service_fid != NULL);

	M0_ENTRY("hi=%p local_rpc_endpoint=%s process_fid="FID_F,
	         hi, local_rpc_endpoint, FID_P(process_fid));
	M0_LOG(M0_DEBUG, "hi=%p ha_service_fid="FID_F" rm_service_fid="FID_F,
		 hi, FID_P(ha_service_fid), FID_P(rm_service_fid));

	ep = m0_strdup(local_rpc_endpoint);
	if (ep == NULL)
		return M0_ERR(-ENOMEM);

	hii->hii_cfg.hic_local_rpc_endpoint       =  ep;
	hii->hii_cfg.hic_process_fid              = *process_fid;
	hii->hii_cfg.hic_ha_service_fid           = *ha_service_fid;
	hii->hii_cfg.hic_rm_service_fid           = *rm_service_fid;
	hii->hii_cfg.hic_entrypoint_request_cb    =  entrypoint_request_cb;
	hii->hii_cfg.hic_msg_received_cb          =  msg_received_cb;
	hii->hii_cfg.hic_msg_is_delivered_cb      =  msg_is_delivered_cb;
	hii->hii_cfg.hic_msg_is_not_delivered_cb  =  msg_is_not_delivered_cb;
	hii->hii_cfg.hic_link_connected_cb        =  link_connected_cb;
	hii->hii_cfg.hic_link_reused_cb           =  link_reused_cb;
	hii->hii_cfg.hic_link_absent_cb           =  link_absent_cb;
	hii->hii_cfg.hic_link_is_disconnecting_cb =  link_is_disconnecting_cb;
	hii->hii_cfg.hic_link_disconnected_cb     =  link_disconnected_cb;

	m0_module_setup(&hii->hii_module, "m0_halon_interface",
			halon_interface_levels,
			ARRAY_SIZE(halon_interface_levels),
			&hii->hii_instance);
	rc = m0_module_init(&hii->hii_module, M0_HALON_INTERFACE_LEVEL_STARTED);
	if (rc != 0) {
		m0_module_fini(&hii->hii_module, M0_MODLEV_NONE);
		m0_free(ep);
		return M0_ERR(rc);
	}
	m0_sm_group_lock(&hii->hii_sm_group);
	m0_sm_state_set(&hii->hii_sm, M0_HALON_INTERFACE_STATE_WORKING);
	m0_sm_group_unlock(&hii->hii_sm_group);
	return M0_RC(rc);
}

void m0_halon_interface_stop(struct m0_halon_interface *hi)
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;

	M0_ASSERT(m0_halon_interface_internal_bob_check(hii));
	M0_ASSERT(m0_get() == &hii->hii_instance);

	M0_ENTRY("hi=%p", hi);

	m0_sm_group_lock(&hii->hii_sm_group);
	m0_sm_state_set(&hii->hii_sm, M0_HALON_INTERFACE_STATE_INITIALISED);
	m0_sm_group_unlock(&hii->hii_sm_group);

	m0_free(hii->hii_cfg.hic_local_rpc_endpoint);
	m0_module_fini(&hii->hii_module, M0_MODLEV_NONE);

	M0_LEAVE();
}

void m0_halon_interface_entrypoint_reply(
                struct m0_halon_interface  *hi,
                const struct m0_uint128    *req_id,
                int                         rc,
                uint32_t                    confd_nr,
                const struct m0_fid        *confd_fid_data,
                const char                **confd_eps_data,
                uint32_t                    confd_quorum,
                const struct m0_fid        *rm_fid,
                const char                 *rm_eps)
{
	struct m0_ha_entrypoint_rep rep;

	M0_ENTRY("hi=%p req_id="U128X_F" rc=%d confd_nr=%"PRIu32" "
	         "confd_quorum=%"PRIu32" rm_fid="FID_F" rm_eps=%s",
		 hi, U128_P(req_id), rc, confd_nr, confd_quorum,
	         FID_P(rm_fid == NULL ? &M0_FID0 : rm_fid), rm_eps);

	if (hi->hif_internal->hii_cfg.hic_log_entrypoint) {
		M0_LOG(M0_WARN, "req_id="U128X_F" rc=%d confd_nr=%"PRIu32" "
	         "confd_quorum=%"PRIu32" rm_fid="FID_F" rm_eps=%s",
		 U128_P(req_id), rc, confd_nr, confd_quorum,
	         FID_P(rm_fid == NULL ? &M0_FID0 : rm_fid), rm_eps);
	}
	rep = (struct m0_ha_entrypoint_rep){
		.hae_quorum        = confd_quorum,
		.hae_confd_fids    = {
			.af_count = confd_nr,
			.af_elems = (struct m0_fid *)confd_fid_data,
		},
		.hae_confd_eps     = confd_eps_data,
		.hae_active_rm_fid = rm_fid == NULL ? M0_FID0 : *rm_fid,
		.hae_active_rm_ep  = (char *)rm_eps,
		.hae_control       = rc == 0 ? M0_HA_ENTRYPOINT_CONSUME :
					       M0_HA_ENTRYPOINT_QUERY,
	};
	m0_ha_entrypoint_reply(&hi->hif_internal->hii_ha, req_id, &rep, NULL);
	M0_LEAVE();
}

void m0_halon_interface_send(struct m0_halon_interface *hi,
                             struct m0_ha_link         *hl,
                             const struct m0_ha_msg    *msg,
                             uint64_t                  *tag)
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	M0_PRE(m0_halon_interface_internal_bob_check(hii));
	M0_PRE(m0_get() == &hii->hii_instance);

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hi=%p ha=%p hl=%p ep=%s msg=%p epoch=%"PRIu64" tag=%p",
		 hi, &hii->hii_ha, hl, ep, msg, msg->hm_epoch, tag);
	m0_ha_msg_debug_print(msg, __func__);
	m0_ha_send(&hii->hii_ha, hl, msg, tag);
	if (hii->hii_cfg.hic_log_msg) {
		M0_LOG(M0_WARN, "hl=%p ep=%s epoch=%"PRIu64" tag=%"PRIu64" "
		       "type=%"PRIu64,
		       hl, ep, msg->hm_epoch, *tag, msg->hm_data.hed_type);
	}
	M0_LEAVE("hi=%p ha=%p hl=%p ep=%s msg=%p epoch=%"PRIu64" tag=%"PRIu64,
		 hi, &hii->hii_ha, hl, ep, msg, msg->hm_epoch, *tag);
}

void m0_halon_interface_delivered(struct m0_halon_interface *hi,
                                  struct m0_ha_link         *hl,
                                  const struct m0_ha_msg    *msg)
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	M0_PRE(m0_halon_interface_internal_bob_check(hii));
	M0_PRE(m0_get() == &hii->hii_instance);

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hi=%p ha=%p hl=%p ep=%s msg=%p epoch=%"PRIu64" tag=%"PRIu64,
		 hi, &hii->hii_ha, hl, ep,
		 msg, msg->hm_epoch, m0_ha_msg_tag(msg));
	if (hii->hii_cfg.hic_log_msg) {
		M0_LOG(M0_WARN, "hl=%p ep=%s epoch=%"PRIu64" tag=%"PRIu64" "
		       "type=%"PRIu64, hl, ep, msg->hm_epoch, m0_ha_msg_tag(msg),
		       msg->hm_data.hed_type);
	}
	/*
	 * Remove 'const' here.
	 *
	 * 'msg' should be the message passed to hic_msg_received_cb().
	 * halon_interface_msg_received_cb() has non-const msg, the function
	 * makes the message const. If the 'const' is removed here then no
	 * harm should be done.
	 */
	m0_ha_delivered(&hii->hii_ha, hl, (struct m0_ha_msg *)msg);
	M0_LEAVE("hi=%p ha=%p hl=%p ep=%s msg=%p epoch=%"PRIu64" tag=%"PRIu64,
		 hi, &hii->hii_ha, hl, ep,
		 msg, msg->hm_epoch, m0_ha_msg_tag(msg));
}

void m0_halon_interface_disconnect(struct m0_halon_interface *hi,
                                   struct m0_ha_link         *hl)
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;
	char                                buf[HALON_INTERFACE_EP_BUF];
	const char                         *ep = &buf[0];

	M0_PRE(m0_halon_interface_internal_bob_check(hii));
	M0_PRE(m0_get() == &hii->hii_instance);

	m0_ha_rpc_endpoint(&hii->hii_ha, hl, buf, ARRAY_SIZE(buf));
	M0_ENTRY("hi=%p ha=%p hl=%p ep=%s", hi, &hii->hii_ha, hl, ep);
	if (hii->hii_cfg.hic_log_link)
		M0_LOG(M0_WARN, "hl=%p ep=%s", hl, ep);
	m0_ha_disconnect_incoming(&hii->hii_ha, hl);
	M0_LEAVE("hi=%p ha=%p hl=%p ep=%s", hi, &hii->hii_ha, hl, ep);
}

static bool halon_interface_is_working(struct m0_halon_interface_internal *hii)
{
	bool working;

	m0_sm_group_lock(&hii->hii_sm_group);
	working = hii->hii_sm.sm_state == M0_HALON_INTERFACE_STATE_WORKING;
	m0_sm_group_unlock(&hii->hii_sm_group);
	return working;
}

struct m0_rpc_machine *
m0_halon_interface_rpc_machine(struct m0_halon_interface *hi)
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;
	struct m0_rpc_machine              *rpc_machine;
	bool                                working;

	M0_PRE(m0_halon_interface_internal_bob_check(hii));
	M0_PRE(m0_get() == &hii->hii_instance);

	rpc_machine = &hii->hii_rpc_machine;
	working = halon_interface_is_working(hii);
	M0_LOG(M0_DEBUG, "hi=%p hii=%p rpc_machine=%p working=%d",
	       hi, hii, rpc_machine, !!working);
	return working ? rpc_machine : NULL;
}

struct m0_reqh *m0_halon_interface_reqh(struct m0_halon_interface *hi)
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;
	struct m0_reqh                     *reqh;
	bool                                working;

	M0_PRE(m0_halon_interface_internal_bob_check(hii));
	M0_PRE(m0_get() == &hii->hii_instance);

	reqh = &hii->hii_reqh;
	working = halon_interface_is_working(hii);
	M0_LOG(M0_DEBUG, "hi=%p hii=%p reqh=%p working=%d",
	       hi, hii, reqh, !!working);
	return working ? reqh : NULL;
}

struct m0_spiel *m0_halon_interface_spiel(struct m0_halon_interface *hi)
{
	struct m0_halon_interface_internal *hii = hi->hif_internal;
	struct m0_spiel                    *spiel;
	bool                                working;

	M0_PRE(m0_halon_interface_internal_bob_check(hii));
	M0_PRE(m0_get() == &hii->hii_instance);

	spiel = &hii->hii_spiel;
	working = halon_interface_is_working(hii);
	M0_LOG(M0_DEBUG, "hi=%p hii=%p spiel=%p working=%d",
	       hi, hii, spiel, !!working);
	return working ? spiel : NULL;
}

M0_INTERNAL int m0_halon_interface_thread_adopt(struct m0_halon_interface *hi,
						struct m0_thread *thread)
{
	struct m0 *mero;
	int        rc;

	M0_PRE(m0_halon_interface_internal_bob_check(hi->hif_internal));

	mero = &hi->hif_internal->hii_instance;
	rc = m0_thread_adopt(thread, mero);

	return M0_RC(rc);
}

M0_INTERNAL void m0_halon_interface_thread_shun(void)
{
	m0_thread_shun();
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
