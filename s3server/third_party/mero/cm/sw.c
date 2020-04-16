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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 06/07/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM

#include "lib/trace.h"
#include "lib/time.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/locality.h"

#include "fop/fop.h"
#include "rpc/rpc.h"
#include "mero/setup.h" /* CS_MAX_EP_ADDR_LEN */

#include "cm/proxy.h"
#include "cm/sw.h"
#include "cm/cm.h"

/**
   @addtogroup CMSW

   @{
 */

M0_INTERNAL bool m0_cm_sw_is_set(const struct m0_cm_sw *sw)
{
	return m0_cm_ag_id_is_set(&sw->sw_lo) ||
		m0_cm_ag_id_is_set(&sw->sw_hi);
}

M0_INTERNAL bool m0_cm_sw_cmp(const struct m0_cm_sw *sw0, const struct m0_cm_sw *sw1)
{
	return m0_cm_ag_id_cmp(&sw0->sw_lo, &sw1->sw_lo) ?:
		m0_cm_ag_id_cmp(&sw0->sw_hi, &sw1->sw_hi);
}

M0_INTERNAL void m0_cm_sw_set(struct m0_cm_sw *dst,
			      const struct m0_cm_ag_id *lo,
			      const struct m0_cm_ag_id *hi)
{
	M0_PRE(dst != NULL && lo != NULL && hi != NULL);

	m0_cm_ag_id_copy(&dst->sw_lo, lo);
	m0_cm_ag_id_copy(&dst->sw_hi, hi);
}

M0_INTERNAL void m0_cm_sw_copy(struct m0_cm_sw *dst,
			       const struct m0_cm_sw *src)
{
	M0_PRE(dst != NULL && src != NULL);

	m0_cm_ag_id_copy(&dst->sw_lo, &src->sw_lo);
	m0_cm_ag_id_copy(&dst->sw_hi, &src->sw_hi);
}

M0_INTERNAL int m0_cm_sw_onwire_init(struct m0_cm *cm, struct m0_cm_sw_onwire *sw_onwire,
				     uint64_t proxy_id, const char *ep,
				     const struct m0_cm_sw *sw,
				     const struct m0_cm_sw *out_interval)
{
	M0_PRE(m0_cm_is_locked(cm));
	M0_PRE(sw_onwire != NULL && ep != NULL && sw != NULL);

	M0_ENTRY("cm %p, done %d", cm, (int)cm->cm_done);
	m0_cm_sw_copy(&sw_onwire->swo_in_interval, sw);
	m0_cm_sw_copy(&sw_onwire->swo_out_interval, out_interval);
	sw_onwire->swo_cm_ep.ep_size = CS_MAX_EP_ADDR_LEN;
	sw_onwire->swo_cm_epoch = cm->cm_epoch;
	sw_onwire->swo_sender_id = proxy_id;
	M0_ALLOC_ARR(sw_onwire->swo_cm_ep.ep, CS_MAX_EP_ADDR_LEN);
	if (sw_onwire->swo_cm_ep.ep == NULL )
		return M0_ERR(-ENOMEM);
	strncpy(sw_onwire->swo_cm_ep.ep, ep, CS_MAX_EP_ADDR_LEN);
	if (m0_cm_state_get(cm) == M0_CMS_FAIL)
		sw_onwire->swo_cm_status = M0_PX_FAILED;
	else if (m0_cm_state_get(cm) == M0_CMS_READY)
			sw_onwire->swo_cm_status = M0_PX_READY;
	else if ((!m0_cm_cp_pump_is_complete(&cm->cm_cp_pump) ||
		 !cm->cm_sw_update.swu_is_complete) &&
		 m0_cm_state_get(cm) == M0_CMS_ACTIVE)
			sw_onwire->swo_cm_status = M0_PX_ACTIVE;
	else if (m0_cm_cp_pump_is_complete(&cm->cm_cp_pump) &&
		 cm->cm_sw_update.swu_is_complete &&
		 !m0_cm_aggr_group_tlists_are_empty(cm))
			sw_onwire->swo_cm_status = M0_PX_COMPLETE;
	else
		sw_onwire->swo_cm_status = M0_PX_STOP;

	return 0;
}

M0_INTERNAL int m0_cm_sw_local_update(struct m0_cm *cm)
{
	int rc;

	M0_ENTRY("cm: %p", cm);
	M0_PRE(cm != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	rc = m0_cm_ag_advance(cm);

	return M0_RC(rc);
}

M0_INTERNAL int m0_cm_sw_remote_update(struct m0_cm *cm)
{
	struct m0_cm_proxy *pxy;
	struct m0_cm_sw     in_interval;
	struct m0_cm_sw     out_interval;
	bool                start = false;
	int                 rc = 0;
	M0_ENTRY();

	M0_PRE(cm != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	m0_cm_ag_in_interval(cm, &in_interval);
	/* We reset in_interval::sw_hi in M0_CMS_READY state to
	 * struct m0_cm::cm_sw_last_updated_hi, read from persistent
	 * store, saved from previous sns operation which could have
	 * been quiesced and in-order to resume from the same.
	 */
	m0_cm_ag_id_copy(&in_interval.sw_hi, &cm->cm_sw_last_updated_hi);
	if (m0_cm_state_get(cm) == M0_CMS_READY) {
		start = true;
	}
	m0_cm_ag_out_interval(cm, &out_interval);
	m0_tl_for(proxy, &cm->cm_proxies, pxy) {
		pxy->px_send_final_update = cm->cm_done;
		ID_LOG("proxy last updated",
				&pxy->px_last_sw_onwire_sent.sw_hi);
		if ((start || pxy->px_send_final_update || cm->cm_quiesce ||
		    cm->cm_abort ||
		    !m0_cm_proxy_is_updated(pxy, &in_interval)) &&
		    pxy->px_update_rc == 0) {
			rc = m0_cm_proxy_remote_update(pxy, &in_interval,
						       &out_interval);
			if (rc != 0)
				break;
		}
	} m0_tl_endfor;

	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} CMSW */
/*
*  Local variables:
*  c-indentation-style: "K&R"
*  c-basic-offset: 8
*  tab-width: 8
*  fill-column: 80
*  scroll-step: 1
*  End:
*/
