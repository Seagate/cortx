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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 *                  Subhash Arya  <subhash_arya@xyratex.com>
 * Original creation date: 03/08/2013
 */

#pragma once

#ifndef __MERO_CM_PROXY_H__
#define __MERO_CM_PROXY_H__

#include "lib/chan.h"

#include "fop/fom_simple.h"
#include "rpc/conn.h"
#include "rpc/session.h"
#include "sm/sm.h"
#include "conf/obj.h"

#include "cm/sw.h"
#include "cm/ag.h"

/**
   @defgroup CMPROXY copy machine proxy
   @ingroup CM

   @{
*/

struct m0_cm_cp;

enum m0_proxy_state {
	M0_PX_INIT,
	M0_PX_READY,
	M0_PX_ACTIVE,
	M0_PX_COMPLETE,
	M0_PX_STOP,
	M0_PX_FAILED
};

/**
 * Represents remote replica and stores its details including its sliding
 * window.
 */
struct m0_cm_proxy {
	/** Remote replica's identifier. */
	uint64_t                px_id;

	m0_time_t               px_epoch;

	/** Remote replica's sliding window. */
	struct m0_cm_sw         px_sw;

	/** Last local sliding window update sent to this replica. */
	struct m0_cm_sw         px_last_sw_onwire_sent;

	/**
	 * Identifier of the last aggregation group received from this proxy
	 * having outgoing copy packets.
	 */
	struct m0_cm_sw         px_out_interval;

	/**
	 * Identifier of last aggregation group sent to this proxy
	 * having outgoing copy packets.
	 */
	struct m0_cm_ag_id      px_last_out_sent;

	struct m0_sm_ast        px_sw_onwire_ast;

	struct m0_sm_ast        px_fail_ast;

	struct m0_sm_ast        px_online_ast;

	enum m0_proxy_state     px_status;

	bool                    px_is_done;

	uint64_t                px_nr_updates_posted;

	/** 0 if sw update was successfull. */
	int                     px_update_rc;

	uint32_t                px_updates_pending;

	/** Back reference to local copy machine. */
	struct m0_cm           *px_cm;

	/**
	 * Pending list of copy packets to be forwarded to the remote
	 * replica.
	 * @see m0_cm_cp::c_proxy_linkage
	 */
	struct m0_tl            px_pending_cps;

	struct m0_mutex         px_mutex;

	struct m0_rpc_conn     *px_conn;

	struct m0_rpc_session  *px_session;

	const char             *px_endpoint;

	/**
	 * Linkage into copy machine proxy list.
	 * @see struct m0_cm::cm_proxies
	 */
	struct m0_tlink        px_linkage;

	struct m0_tlink        px_fail_linkage;

	/**
	 * Listens for an event on io service's configuration object's
	 * HA channel.
	 * Used to update proxy status in the clink callback on HA
	 * notification.
	 */
	struct m0_clink        px_ha_link;

	/**
	 * If true, post a one final update to remote copy machine corresponding
	 * to this proxy.
	 * True when copy machine has completed processing all its aggregation
	 * groups.
	 */
	bool                   px_send_final_update;

	uint64_t               px_magic;
};

/**
 * Incoming copy packet counter from every m0_cm_proxy
 */
struct m0_cm_proxy_in_count {
	/** Number of proxies. */
	uint32_t  p_nr;
	/** Array of number of copy packets from each proxy. */
	uint32_t *p_count;
};

/**
 * Sliding window update fop context for a remote replica proxy.
 * @see m0_cm_proxy_remote_update()
 */
struct m0_cm_proxy_sw_onwire {
	struct m0_fop       pso_fop;
	/**
	 * Remote copy machine replica proxy to which the sliding window
	 * update FOP is to be sent (i.e. m0_cm_proxy_sw_onwire_fop::psu_fop).
	 */
	struct m0_cm_proxy *pso_proxy;
};

M0_INTERNAL int m0_cm_proxy_init(struct m0_cm_proxy *pxy, uint64_t px_id,
				 struct m0_cm_ag_id *lo, struct m0_cm_ag_id *hi,
				 const char *endpoint);

M0_INTERNAL void m0_cm_proxy_add(struct m0_cm *cm, struct m0_cm_proxy *pxy);

M0_INTERNAL void m0_cm_proxy_del(struct m0_cm *cm, struct m0_cm_proxy *pxy);

M0_INTERNAL struct m0_cm_proxy *m0_cm_proxy_locate(struct m0_cm *cm,
						   const char *ep);

M0_INTERNAL int m0_cm_proxy_update(struct m0_cm_proxy *pxy,
				   struct m0_cm_sw *in_interval,
				   struct m0_cm_sw *out_interval,
				   uint32_t px_status,
				   m0_time_t px_epoch);

M0_INTERNAL int m0_cm_proxy_remote_update(struct m0_cm_proxy *proxy,
					  struct m0_cm_sw *in_interval,
					  struct m0_cm_sw *out_interval);

M0_INTERNAL void m0_cm_proxy_cp_add(struct m0_cm_proxy *pxy,
				    struct m0_cm_cp *cp);

M0_INTERNAL uint64_t m0_cm_proxy_nr(struct m0_cm *cm);

M0_INTERNAL bool m0_cm_proxy_agid_is_in_sw(struct m0_cm_proxy *pxy,
					   struct m0_cm_ag_id *id);

M0_INTERNAL void m0_cm_proxy_fini(struct m0_cm_proxy *pxy);

M0_INTERNAL bool m0_cm_proxy_is_done(const struct m0_cm_proxy *pxy);
M0_INTERNAL void m0_cm_proxy_pending_cps_wakeup(struct m0_cm *cm);

M0_INTERNAL void m0_cm_proxy_event_handle_register(struct m0_cm_proxy *pxy,
						   struct m0_conf_obj *svc_obj);

M0_INTERNAL bool m0_cm_proxy_is_locked(struct m0_cm_proxy *pxy);
M0_INTERNAL void m0_cm_proxy_lock(struct m0_cm_proxy *pxy);
M0_INTERNAL void m0_cm_proxy_unlock(struct m0_cm_proxy *pxy);

M0_INTERNAL bool m0_cm_proxies_ready(const struct m0_cm *cm);

M0_INTERNAL int m0_cm_proxy_in_count_alloc(struct m0_cm_proxy_in_count *pcount,
					   uint32_t nr_proxies);
M0_INTERNAL void m0_cm_proxy_in_count_free(struct m0_cm_proxy_in_count *pcount);

M0_INTERNAL bool m0_cm_proxy_is_updated(struct m0_cm_proxy *proxy,
					struct m0_cm_sw *in_interval);

M0_INTERNAL void m0_cm_proxies_sent_reset(struct m0_cm *cm);

M0_TL_DESCR_DECLARE(proxy, M0_EXTERN);
M0_TL_DECLARE(proxy, M0_INTERNAL, struct m0_cm_proxy);

M0_TL_DESCR_DECLARE(proxy_fail, M0_EXTERN);
M0_TL_DECLARE(proxy_fail, M0_INTERNAL, struct m0_cm_proxy);

M0_TL_DESCR_DECLARE(proxy_cp, M0_EXTERN);
M0_TL_DECLARE(proxy_cp, M0_INTERNAL, struct m0_cm_cp);

/** @} endgroup CMPROXY */

/* __MERO_CM_PROXY_H__ */

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
