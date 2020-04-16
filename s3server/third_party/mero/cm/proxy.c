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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/trace.h"
#include "lib/time.h"
#include "lib/misc.h"
#include "lib/locality.h"

#include "rpc/rpc.h"
#include "rpc/session.h"
#include "mero/magic.h"
#include "mero/setup.h" /* CS_MAX_EP_ADDR_LEN */
#include "fop/fom.h"

#include "cm/cm.h"
#include "cm/cp.h"
#include "cm/proxy.h"
#include "cm/ag.h"

/**
   @addtogroup CMPROXY

   @{
 */

enum {
	PROXY_WAIT = 1
};

M0_TL_DESCR_DEFINE(proxy, "copy machine proxy", M0_INTERNAL,
		   struct m0_cm_proxy, px_linkage, px_magic,
		   CM_PROXY_LINK_MAGIC, CM_PROXY_HEAD_MAGIC);

M0_TL_DEFINE(proxy, M0_INTERNAL, struct m0_cm_proxy);

M0_TL_DESCR_DEFINE(proxy_fail, "copy machine proxy", M0_INTERNAL,
		   struct m0_cm_proxy, px_fail_linkage, px_magic,
		   CM_PROXY_LINK_MAGIC, CM_PROXY_HEAD_MAGIC);

M0_TL_DEFINE(proxy_fail, M0_INTERNAL, struct m0_cm_proxy);

M0_TL_DESCR_DEFINE(proxy_cp, "pending copy packets", M0_INTERNAL,
		   struct m0_cm_cp, c_cm_proxy_linkage, c_magix,
		   CM_CP_MAGIX, CM_PROXY_CP_HEAD_MAGIX);

M0_TL_DEFINE(proxy_cp, M0_INTERNAL, struct m0_cm_cp);

static const struct m0_bob_type proxy_bob = {
	.bt_name = "cm proxy",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_cm_proxy, px_magic),
	.bt_magix = CM_PROXY_LINK_MAGIC,
	.bt_check = NULL
};

M0_BOB_DEFINE(static, &proxy_bob, m0_cm_proxy);

static bool cm_proxy_invariant(const struct m0_cm_proxy *pxy)
{
	/**
	 * @todo : Add checks for pxy::px_id when uid mechanism is implemented.
	 */
	return _0C(pxy != NULL) &&  _0C(m0_cm_proxy_bob_check(pxy)) &&
	       _0C(m0_cm_is_locked(pxy->px_cm)) &&
	       _0C(pxy->px_endpoint != NULL);
}

M0_INTERNAL int m0_cm_proxy_init(struct m0_cm_proxy *proxy, uint64_t px_id,
				 struct m0_cm_ag_id *lo, struct m0_cm_ag_id *hi,
				 const char *endpoint)
{
	M0_PRE(proxy != NULL && lo != NULL && hi != NULL && endpoint != NULL);

	m0_cm_proxy_bob_init(proxy);
	proxy_tlink_init(proxy);
	proxy_fail_tlink_init(proxy);
	m0_mutex_init(&proxy->px_mutex);
	proxy_cp_tlist_init(&proxy->px_pending_cps);
	proxy->px_id = px_id;
	proxy->px_sw.sw_lo = *lo;
	proxy->px_sw.sw_hi = *hi;
	proxy->px_endpoint = endpoint;
	proxy->px_is_done = false;
	proxy->px_epoch = 0;
	return 0;
}

M0_INTERNAL void m0_cm_proxy_add(struct m0_cm *cm, struct m0_cm_proxy *pxy)
{
	M0_ENTRY("cm: %p proxy: %p", cm, pxy);
	M0_PRE(m0_cm_is_locked(cm));
	M0_PRE(!proxy_tlink_is_in(pxy));
	pxy->px_cm = cm;
	proxy_tlist_add_tail(&cm->cm_proxies, pxy);
	M0_CNT_INC(cm->cm_proxy_active_nr);
	M0_CNT_INC(cm->cm_proxy_nr);
	M0_ASSERT(proxy_tlink_is_in(pxy));
	M0_POST(cm_proxy_invariant(pxy));
	M0_LEAVE();
}

M0_INTERNAL void m0_cm_proxy_del(struct m0_cm *cm, struct m0_cm_proxy *pxy)
{
	M0_ENTRY("cm: %p proxy: %p", cm, pxy);
	M0_PRE(m0_cm_is_locked(cm));
	M0_PRE(proxy_tlink_is_in(pxy));
	if (proxy_fail_tlink_is_in(pxy))
		proxy_fail_tlist_del(pxy);
	proxy_fail_tlink_fini(pxy);
	proxy_tlink_del_fini(pxy);
	M0_CNT_DEC(cm->cm_proxy_nr);
	M0_ASSERT(!proxy_tlink_is_in(pxy));
	M0_POST(cm_proxy_invariant(pxy));
	M0_LEAVE();
}

M0_INTERNAL void m0_cm_proxy_cp_add(struct m0_cm_proxy *pxy,
				    struct m0_cm_cp *cp)
{
	M0_ENTRY("proxy: %p cp: %p ep: %s", pxy, cp, pxy->px_endpoint);
	M0_PRE(m0_cm_proxy_is_locked(pxy));
	M0_PRE(!proxy_cp_tlink_is_in(cp));

	proxy_cp_tlist_add_tail(&pxy->px_pending_cps, cp);
	ID_LOG("proxy ag_id", &cp->c_ag->cag_id);
	M0_POST(proxy_cp_tlink_is_in(cp));
	M0_LEAVE();
}

static void cm_proxy_cp_del(struct m0_cm_proxy *pxy,
			    struct m0_cm_cp *cp)
{
	M0_PRE(m0_cm_proxy_is_locked(pxy));
	M0_PRE(proxy_cp_tlink_is_in(cp));
	proxy_cp_tlist_del(cp);
	M0_POST(!proxy_cp_tlink_is_in(cp));
}

M0_INTERNAL struct m0_cm_proxy *m0_cm_proxy_locate(struct m0_cm *cm,
						   const char *addr)
{
	struct m0_net_transfer_mc *tm;
	struct m0_net_end_point   *ep;
	struct m0_cm_proxy        *pxy;
	/*
	 * Proxy address string (pxy->px_endpoint) cannot be directly compared
	 * with supplied address string, because the same end-point might have
	 * different address strings.
	 *
	 * Instantiate the endpoints and compare them directly.
	 */
	tm = &m0_reqh_rpc_mach_tlist_head
		(&cm->cm_service.rs_reqh->rh_rpc_machines)->rm_tm;
	if (tm == NULL || m0_net_end_point_create(&ep, tm, addr) != 0)
		return NULL;

	m0_tl_for(proxy, &cm->cm_proxies, pxy) {
		struct m0_net_end_point *scan;
		if (m0_net_end_point_create(&scan, tm, pxy->px_endpoint) != 0)
			continue;
		m0_net_end_point_put(scan); /* OK to put before comparison. */
		if (scan == ep)
			break;
	} m0_tl_endfor;
	m0_net_end_point_put(ep);
	return pxy;
}

static void __wake_up_pending_cps(struct m0_cm_proxy *pxy)
{
	struct m0_cm_cp *cp;

	m0_tl_for(proxy_cp, &pxy->px_pending_cps, cp) {
		cm_proxy_cp_del(pxy, cp);
		/* wakeup pending copy packet foms */
		m0_fom_wakeup(&cp->c_fom);
	} m0_tl_endfor;

}

static bool epoch_check(struct m0_cm_proxy *pxy, m0_time_t px_epoch)
{
	if (px_epoch != pxy->px_epoch) {
		M0_LOG(M0_WARN, "Mismatch Epoch,"
		       "current: %llu" "received: %llu",
		       (unsigned long long)pxy->px_epoch,
		       (unsigned long long)px_epoch);

		return false;
	}

	return true;
}

static void proxy_done(struct m0_cm_proxy *proxy)
{
	struct m0_cm *cm = proxy->px_cm;

	if (!proxy->px_is_done) {
		proxy->px_is_done = true;
		m0_cm_notify(cm);
	}
	m0_cm_complete_notify(cm);
}

/**
 * Updates @pxy sliding window with given [@lo, @hi], latest outgoing
 * aggregation group proccessed with @last_out and proxy status with
 * @px_status.
 */
static void _sw_update(struct m0_cm_proxy *pxy, struct m0_cm_sw *in_interval,
		       struct m0_cm_sw *out_interval, uint32_t px_status)
{
	ID_LOG("proxy lo", &pxy->px_sw.sw_lo);
	ID_LOG("proxy hi", &pxy->px_sw.sw_hi);

	if (m0_cm_sw_cmp(in_interval, &pxy->px_sw) > 0)
		m0_cm_sw_copy(&pxy->px_sw, in_interval);
	if (m0_cm_sw_cmp(out_interval, &pxy->px_out_interval) > 0)
		m0_cm_sw_copy(&pxy->px_out_interval, out_interval);
	pxy->px_status = px_status;
	__wake_up_pending_cps(pxy);
	M0_ASSERT(cm_proxy_invariant(pxy));
}

static int px_ready(struct m0_cm_proxy *p, struct m0_cm_sw *in_interval,
		    struct m0_cm_sw *out_interval, m0_time_t px_epoch,
		    uint32_t px_status)
{
	struct m0_cm       *cm = p->px_cm;
	struct m0_cm_ag_id  hi;
	int                 rc = 0;

	if (p->px_epoch == 0 && m0_cm_state_get(cm) == M0_CMS_READY) {
		p->px_epoch = px_epoch;
		p->px_status = px_status;
		hi = in_interval->sw_hi;
		/*
		 * Here we select the minimum of the sliding window
		 * starting point provided by each remote copy machine,
		 * from which this copy machine will start in-order to
		 * keep all the copy machines in sync.
		 */
		   if (m0_cm_ag_id_cmp(&hi, &cm->cm_sw_last_updated_hi) < 0) {
				cm->cm_sw_last_updated_hi = hi;
		}
		M0_CNT_INC(cm->cm_nr_proxy_updated);
		rc = 0;
	} else if (m0_cm_state_get(cm) < M0_CMS_READY)
		rc = -EINVAL;

	return M0_RC(rc);
}

static int px_active(struct m0_cm_proxy *p, struct m0_cm_sw *in_interval,
		     struct m0_cm_sw *out_interval, m0_time_t px_epoch,
		     uint32_t px_status)
{
	_sw_update(p, in_interval, out_interval, px_status);
	/* TODO This is expensive during M0_CMS_CTIVE phase but needed to
	 * handle cleanup in case of copy machine failures during active
	 * phase. Try to find another alternative.
	 */
	m0_cm_frozen_ag_cleanup(p->px_cm, p);
	return 0;
}

static int px_complete(struct m0_cm_proxy *p, struct m0_cm_sw *in_interval,
		       struct m0_cm_sw *out_interval, m0_time_t px_epoch,
		       uint32_t px_status)
{
	_sw_update(p, in_interval, out_interval, px_status);
	m0_cm_frozen_ag_cleanup(p->px_cm, p);
	return 0;
}

static int px_stop_fail(struct m0_cm_proxy *p, struct m0_cm_sw *in_interval,
			struct m0_cm_sw *out_interval, m0_time_t px_epoch,
			uint32_t px_status)
{
	_sw_update(p, in_interval, out_interval, px_status);
	m0_cm_frozen_ag_cleanup(p->px_cm, p);
	proxy_done(p);
	return 0;
}

static int (*px_action[])(struct m0_cm_proxy *px, struct m0_cm_sw *in_interval,
			  struct m0_cm_sw *out_interval, m0_time_t px_epoch,
			  uint32_t px_status) = {
	[M0_PX_READY] = px_ready,
	[M0_PX_ACTIVE] = px_active,
	[M0_PX_COMPLETE] = px_complete,
	[M0_PX_STOP] = px_stop_fail,
	[M0_PX_FAILED] = px_stop_fail
};

M0_INTERNAL int m0_cm_proxy_update(struct m0_cm_proxy *pxy,
				   struct m0_cm_sw *in_interval,
				   struct m0_cm_sw *out_interval,
				   uint32_t px_status,
				   m0_time_t px_epoch)
{
	struct m0_cm *cm;
	int           rc;

	M0_ENTRY("proxy: %p ep: %s", pxy, pxy->px_endpoint);
	M0_PRE(pxy != NULL && in_interval != NULL && out_interval != NULL);
	M0_PRE(px_status >= pxy->px_status);

	m0_cm_proxy_lock(pxy);
	cm = pxy->px_cm;
	M0_ASSERT(m0_cm_is_locked(cm));
        M0_LOG(M0_DEBUG, "Recvd from :%s status: %u curr_status: %u "
			 "nr_updates: %u", pxy->px_endpoint, px_status,
			 pxy->px_status, (unsigned)cm->cm_nr_proxy_updated);

	if (pxy->px_status != M0_PX_INIT && !epoch_check(pxy, px_epoch)) {
		m0_cm_proxy_unlock(pxy);
		return -EINVAL;
	}

	if (px_status >= M0_PX_COMPLETE &&
	    pxy->px_status < M0_PX_COMPLETE) {
		/*
		 * Got a fresh "complete(fail,stop)" state - need to
		 * decrease counter
		 */
		M0_LOG(M0_DEBUG, "Decrease proxy_nr (current nr %"
		       PRIu64") cm %p, pxy %p",
		       cm->cm_proxy_active_nr, cm, pxy);
		M0_CNT_DEC(cm->cm_proxy_active_nr);
	} else if (pxy->px_status >= M0_PX_COMPLETE &&
		   px_status < M0_PX_COMPLETE) {
		M0_LOG(M0_DEBUG, "Increase proxy_nr (current nr %"
		       PRIu64") cm %p, pxy %p",
		       cm->cm_proxy_active_nr, cm, pxy);
		M0_CNT_INC(cm->cm_proxy_active_nr);
	}

	rc = px_action[px_status](pxy, in_interval, out_interval, px_epoch, px_status);
	if (m0_cm_state_get(cm) == M0_CMS_READY && m0_cm_proxies_ready(cm))
			m0_chan_broadcast(&cm->cm_proxy_init_wait);
	/*
	 * All proxies finished processing, i.e. all proxies are in
	 * COMPLETE/STOP/FAIL state.
	 */
	if (cm->cm_proxy_active_nr == 0) {
		M0_LOG(M0_DEBUG, "No more active proxies in cm %p", cm);
		m0_cm_notify(cm);
	}
	m0_cm_proxy_unlock(pxy);

	return M0_RC(rc);
}

M0_INTERNAL bool m0_cm_proxy_is_updated(struct m0_cm_proxy *proxy,
					struct m0_cm_sw *in_interval)
{
	return m0_cm_ag_id_cmp(&in_interval->sw_hi,
			       &proxy->px_last_sw_onwire_sent.sw_hi) <= 0;
}

static void proxy_sw_onwire_ast_cb(struct m0_sm_group *grp,
				   struct m0_sm_ast *ast)
{
	struct m0_cm_proxy *proxy = container_of(ast, struct m0_cm_proxy,
						      px_sw_onwire_ast);
	struct m0_cm       *cm = proxy->px_cm;
	struct m0_cm_sw     in_interval;
	struct m0_cm_sw     out_interval;

	M0_ASSERT(cm_proxy_invariant(proxy));

	m0_cm_ag_in_interval(cm, &in_interval);
	if (m0_cm_ag_id_is_set(&cm->cm_sw_last_updated_hi))
		m0_cm_ag_id_copy(&in_interval.sw_hi,
				 &cm->cm_sw_last_updated_hi);
	m0_cm_ag_out_interval(cm, &out_interval);
	M0_LOG(M0_DEBUG, "proxy ep: %s, cm->cm_aggr_grps_in_nr %lu"
			 " pending updates: %u", proxy->px_endpoint,
			 cm->cm_aggr_grps_in_nr, proxy->px_updates_pending);
	ID_LOG("proxy last updated hi", &proxy->px_last_sw_onwire_sent.sw_hi);

	/*
	 * We check if updates posted are greater than 0 and decrement as
	 * there could be a case of update resend while a reply is already
	 * on wire and a proxy may receive multiple replies for an update.
	 */
	if (proxy->px_nr_updates_posted > 0)
		M0_CNT_DEC(proxy->px_nr_updates_posted);

        if (proxy->px_update_rc != 0 || proxy->px_send_final_update ||
	    (proxy->px_updates_pending > 0 &&
	     (!m0_cm_proxy_is_updated(proxy, &in_interval) || cm->cm_abort ||
	     cm->cm_quiesce))) {
		if (proxy->px_update_rc == -ECANCELED ||
		     (M0_IN(proxy->px_status, (M0_PX_FAILED, M0_PX_STOP)) &&
		      !proxy->px_send_final_update))
			proxy->px_updates_pending = 0;
		else
			m0_cm_proxy_remote_update(proxy, &in_interval, &out_interval);
	}

	if (m0_cm_state_get(cm) == M0_CMS_READY &&
	    !m0_bitmap_get(&cm->cm_proxy_update_map, proxy->px_id) &&
	    proxy->px_update_rc == 0) {
		M0_CNT_INC(cm->cm_nr_proxy_updated);
		m0_bitmap_set(&cm->cm_proxy_update_map, proxy->px_id, true);
	}

	/* Initial handshake complete, signal waiters to continue further.*/
	if (m0_cm_state_get(cm) == M0_CMS_READY && m0_cm_proxies_ready(cm))
			m0_chan_signal(&cm->cm_proxy_init_wait);

	/*
	 * Handle service/node failure during sns-repair/rebalance.
	 * Cannot send updates to dead proxy, all the aggregation groups,
	 * frozen on that proxy must be destroyed.
	 */
	if (proxy->px_status == M0_PX_FAILED || m0_cm_state_get(cm) == M0_CMS_FAIL ||
	    cm->cm_quiesce || cm->cm_abort) {
		m0_cm_proxy_lock(proxy);
		__wake_up_pending_cps(proxy);
		m0_cm_proxy_unlock(proxy);
		/* Here we have already received notification from HA about
		 * the proxy failure and might receive explicit abort command as well.
		 * So no need to transition cm to FAILED state, just aborting the
		 * operation would suffice.
		 */
		m0_cm_abort(cm, 0);
		m0_cm_frozen_ag_cleanup(cm, proxy);
	}
	if (cm->cm_done || proxy->px_status == M0_PX_FAILED ||
			   m0_cm_state_get(cm) == M0_CMS_FAIL) {
		/* Wake up anyone waiting to handle further process (cleanup/completion). */
		m0_cm_complete_notify(cm);
	}
}

static void proxy_sw_onwire_item_replied_cb(struct m0_rpc_item *req_item)
{
	struct m0_cm_proxy_sw_onwire *swu_fop;
	struct m0_cm_sw_onwire_rep   *sw_rep;
	struct m0_rpc_item           *rep_item;
	struct m0_cm_proxy           *proxy;
	struct m0_fop                *rep_fop;

	M0_ENTRY("%p", req_item);

	swu_fop = M0_AMB(swu_fop, m0_rpc_item_to_fop(req_item), pso_fop);
	proxy = swu_fop->pso_proxy;
	M0_ASSERT(m0_cm_proxy_bob_check(proxy));

	if (req_item->ri_error == 0) {
		rep_item = req_item->ri_reply;
		if (m0_rpc_item_is_generic_reply_fop(rep_item))
			proxy->px_update_rc = m0_rpc_item_generic_reply_rc(rep_item);
                else {
                        rep_fop = m0_rpc_item_to_fop(rep_item);
                        sw_rep = m0_fop_data(rep_fop);
                        proxy->px_update_rc = sw_rep->swr_rc;
                }
        } else
                proxy->px_update_rc = req_item->ri_error;

	proxy->px_sw_onwire_ast.sa_cb = proxy_sw_onwire_ast_cb;
	m0_sm_ast_post(&proxy->px_cm->cm_sm_group, &proxy->px_sw_onwire_ast);

	M0_LEAVE();
}

const struct m0_rpc_item_ops proxy_sw_onwire_item_ops = {
	.rio_replied = proxy_sw_onwire_item_replied_cb
};

static void cm_proxy_sw_onwire_post(struct m0_cm_proxy *proxy,
				    struct m0_fop *fop,
				    const struct m0_rpc_conn *conn)
{
	struct m0_rpc_item *item;

	M0_ENTRY("fop: %p conn: %p", fop, conn);
	M0_PRE(fop != NULL && conn != NULL);

	item              = m0_fop_to_rpc_item(fop);
	item->ri_ops      = &proxy_sw_onwire_item_ops;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_session  = proxy->px_session;
	item->ri_deadline = 0;

	M0_CNT_INC(proxy->px_nr_updates_posted);
	m0_rpc_post(item);
	m0_fop_put_lock(fop);
	M0_LEAVE();
}

static void proxy_sw_onwire_release(struct m0_ref *ref)
{
	struct m0_cm_proxy_sw_onwire  *pso_fop;
	struct m0_fop                     *fop;

	fop = container_of(ref, struct m0_fop, f_ref);
	pso_fop = container_of(fop, struct m0_cm_proxy_sw_onwire, pso_fop);
	M0_ASSERT(pso_fop != NULL);
	m0_fop_fini(fop);
	m0_free(pso_fop);
}

M0_INTERNAL int m0_cm_proxy_remote_update(struct m0_cm_proxy *proxy,
					  struct m0_cm_sw *in_interval,
					  struct m0_cm_sw *out_interval)
{
	struct m0_cm                 *cm;
	struct m0_rpc_machine        *rmach;
	struct m0_rpc_conn           *conn;
	struct m0_cm_proxy_sw_onwire *sw_fop;
	struct m0_fop                *fop;
	const char                   *ep;
	int                           rc;

	M0_ENTRY("proxy: %p", proxy);
	M0_PRE(proxy != NULL);
	cm = proxy->px_cm;
	M0_PRE(m0_cm_is_locked(cm));

	if (proxy->px_nr_updates_posted > 0) {
		M0_CNT_INC(proxy->px_updates_pending);
		return 0;
	}
	if (proxy->px_send_final_update)
		proxy->px_send_final_update = false;
	M0_ALLOC_PTR(sw_fop);
	if (sw_fop == NULL)
		return M0_ERR(-ENOMEM);
	fop = &sw_fop->pso_fop;
	rmach = proxy->px_conn->c_rpc_machine;
	ep = rmach->rm_tm.ntm_ep->nep_addr;
	conn = proxy->px_conn;
	rc = cm->cm_ops->cmo_sw_onwire_fop_setup(cm, fop,
						 proxy_sw_onwire_release,
						 proxy->px_id, ep, in_interval,
						 out_interval);
	if (rc != 0) {
		m0_fop_put_lock(fop);
		m0_free(sw_fop);
		return M0_ERR(rc);
	}
	sw_fop->pso_proxy = proxy;
	ID_LOG("proxy last updated hi", &proxy->px_last_sw_onwire_sent.sw_hi);

	cm_proxy_sw_onwire_post(proxy, fop, conn);
	m0_cm_sw_copy(&proxy->px_last_sw_onwire_sent, in_interval);

	M0_LOG(M0_DEBUG, "Sending to %s hi: ["M0_AG_F"]",
	       proxy->px_endpoint, M0_AG_P(&in_interval->sw_hi));
	return M0_RC(0);
}

M0_INTERNAL bool m0_cm_proxy_is_done(const struct m0_cm_proxy *pxy)
{
	return pxy->px_is_done && pxy->px_nr_updates_posted == 0 &&
	       pxy->px_sw_onwire_ast.sa_next == NULL;
}

M0_INTERNAL void m0_cm_proxy_fini(struct m0_cm_proxy *pxy)
{
	M0_ENTRY("%p", pxy);
	M0_PRE(pxy != NULL);
	M0_PRE(proxy_cp_tlist_is_empty(&pxy->px_pending_cps));

	proxy_cp_tlist_fini(&pxy->px_pending_cps);
	m0_cm_proxy_bob_fini(pxy);
	if (m0_clink_is_armed(&pxy->px_ha_link)) {
		m0_clink_del_lock(&pxy->px_ha_link);
		m0_clink_fini(&pxy->px_ha_link);
	}
	m0_mutex_fini(&pxy->px_mutex);
	M0_LEAVE();
}

M0_INTERNAL uint64_t m0_cm_proxy_nr(struct m0_cm *cm)
{
	M0_PRE(m0_cm_is_locked(cm));

	return proxy_tlist_length(&cm->cm_proxies);
}

M0_INTERNAL bool m0_cm_proxy_agid_is_in_sw(struct m0_cm_proxy *pxy,
					   struct m0_cm_ag_id *id)
{
	bool result;

	m0_cm_proxy_lock(pxy);
	result =  m0_cm_ag_id_cmp(id, &pxy->px_sw.sw_lo) >= 0 &&
		  m0_cm_ag_id_cmp(id, &pxy->px_sw.sw_hi) <= 0;
	m0_cm_proxy_unlock(pxy);

	return result;
}

M0_INTERNAL void m0_cm_proxy_pending_cps_wakeup(struct m0_cm *cm)
{
	struct m0_cm_proxy *pxy;

	m0_tl_for(proxy, &cm->cm_proxies, pxy) {
		__wake_up_pending_cps(pxy);
	} m0_tl_endfor;
}

static void px_fail_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_cm_proxy *pxy = container_of(ast, struct m0_cm_proxy,
					     px_fail_ast);

	m0_cm_proxy_lock(pxy);
	pxy->px_status = M0_PX_FAILED;
	pxy->px_is_done = true;
	if (!proxy_fail_tlink_is_in(pxy))
		proxy_fail_tlist_add_tail(&pxy->px_cm->cm_failed_proxies, pxy);
	__wake_up_pending_cps(pxy);
	m0_cm_proxy_unlock(pxy);
	m0_cm_abort(pxy->px_cm, 0);
	m0_cm_frozen_ag_cleanup(pxy->px_cm, pxy);
	m0_cm_complete_notify(pxy->px_cm);
}

static void px_online_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_cm_proxy *pxy = container_of(ast, struct m0_cm_proxy,
					       px_online_ast);

	/*
	 * Do nothing for now, ongoing sns operation must cleanup and complete
	 * and sns repair/rebalance must be restarted.
	 */
	M0_LOG(M0_DEBUG, "proxy %s is online", pxy->px_endpoint);
}

static bool proxy_clink_cb(struct m0_clink *clink)
{
	struct m0_cm_proxy *pxy = M0_AMB(pxy, clink, px_ha_link);
	struct m0_conf_obj *svc_obj = container_of(clink->cl_chan,
						   struct m0_conf_obj,
						   co_ha_chan);
	struct m0_sm_ast   *ast = NULL;

	M0_PRE(m0_conf_obj_type(svc_obj) == &M0_CONF_SERVICE_TYPE);

	if (M0_IN(svc_obj->co_ha_state, (M0_NC_FAILED, M0_NC_TRANSIENT))) {
		pxy->px_fail_ast.sa_cb = px_fail_ast_cb;
		ast = &pxy->px_fail_ast;
	} else if (svc_obj->co_ha_state == M0_NC_ONLINE &&
		   pxy->px_status == M0_PX_FAILED) {
		pxy->px_online_ast.sa_cb = px_online_ast_cb;
		ast = &pxy->px_online_ast;
	}
	m0_sm_ast_post(&pxy->px_cm->cm_sm_group, ast);

	return true;
}

M0_INTERNAL void m0_cm_proxy_event_handle_register(struct m0_cm_proxy *pxy,
						   struct m0_conf_obj *svc_obj)
{
	m0_clink_init(&pxy->px_ha_link, proxy_clink_cb);
	m0_clink_add_lock(&svc_obj->co_ha_chan, &pxy->px_ha_link);
}

M0_INTERNAL bool m0_cm_proxy_is_locked(struct m0_cm_proxy *pxy)
{
	return m0_mutex_is_locked(&pxy->px_mutex);
}

M0_INTERNAL void m0_cm_proxy_lock(struct m0_cm_proxy *pxy)
{
	m0_mutex_lock(&pxy->px_mutex);
}

M0_INTERNAL void m0_cm_proxy_unlock(struct m0_cm_proxy *pxy)
{
	m0_mutex_unlock(&pxy->px_mutex);
}

M0_INTERNAL bool m0_cm_proxies_ready(const struct m0_cm *cm)
{
	uint32_t nr_failed_proxies;

	M0_PRE(m0_cm_is_locked(cm));

	nr_failed_proxies = proxy_fail_tlist_length(&cm->cm_failed_proxies);
	return cm->cm_nr_proxy_updated == (cm->cm_proxy_nr - nr_failed_proxies) * 2;
}

M0_INTERNAL int m0_cm_proxy_in_count_alloc(struct m0_cm_proxy_in_count *pcount,
					   uint32_t nr_proxies)
{
	M0_PRE(nr_proxies > 0);

	M0_ALLOC_ARR(pcount->p_count, nr_proxies);
	if (pcount->p_count == NULL)
		return -ENOMEM;
	pcount->p_nr = nr_proxies;

	return 0;
}

M0_INTERNAL void m0_cm_proxy_in_count_free(struct m0_cm_proxy_in_count *pcount)
{
	M0_PRE(pcount != NULL);

	m0_free(pcount->p_count);
	pcount->p_count = NULL;
	pcount->p_nr = 0;
}

M0_INTERNAL void m0_cm_proxies_sent_reset(struct m0_cm *cm)
{
	struct m0_cm_proxy *pxy;

	m0_tl_for(proxy, &cm->cm_proxies, pxy) {
		M0_SET0(&pxy->px_last_sw_onwire_sent);
	} m0_tl_endfor;
}

#undef M0_TRACE_SUBSYSTEM

/** @} CMPROXY */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
