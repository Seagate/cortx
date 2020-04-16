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
 * Original creation date: 16/04/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/memory.h"

#include "fop/fop.h"
#include "reqh/reqh.h"
#include "conf/obj_ops.h"     /* m0_conf_obj_find_lock */

#include "sns/cm/cm_utils.h"
#include "sns/cm/iter.h"
#include "sns/cm/cm.h"
#include "sns/cm/cp.h"
#include "sns/cm/file.h"
#include "sns/cm/rebalance/ag.h"

/* Import */
struct m0_cm_sw;

extern const struct m0_sns_cm_helpers rebalance_helpers;
extern const struct m0_cm_cp_ops m0_sns_cm_rebalance_cp_ops;

M0_INTERNAL int
m0_sns_cm_rebalance_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop *fop,
					void (*fop_release)(struct m0_ref *),
					uint64_t proxy_id, const char *local_ep,
					const struct m0_cm_sw *sw,
					const struct m0_cm_sw *out_interval);

static struct m0_cm_cp *rebalance_cm_cp_alloc(struct m0_cm *cm)
{
	struct m0_sns_cm_cp *scp;

	M0_ALLOC_PTR(scp);
	if (scp == NULL)
		return NULL;
	scp->sc_base.c_ops = &m0_sns_cm_rebalance_cp_ops;
	return &scp->sc_base;
}

static int rebalance_cm_prepare(struct m0_cm *cm)
{
	struct m0_sns_cm *scm = cm2sns(cm);
	int               rc;

	M0_ENTRY("cm: %p", cm);
	M0_PRE(scm->sc_op == CM_OP_REBALANCE);

	scm->sc_helpers = &rebalance_helpers;
	rc = m0_sns_cm_fail_dev_log(cm, M0_PNDS_SNS_REBALANCING);
	if (rc == 0)
		rc = m0_sns_cm_prepare(cm);

	return M0_RC(rc);

}

static void rebalance_cm_stop(struct m0_cm *cm)
{
	struct m0_sns_cm       *scm = cm2sns(cm);
	struct m0_pools_common *pc = cm->cm_service.rs_reqh->rh_pools;
	struct m0_pool         *pool;
	struct m0_pooldev      *pd;
	struct m0_conf_obj     *disk_obj;
	struct m0_conf_drive   *disk;
	struct m0_conf_cache   *cc;
	struct m0_ha_nvec       nvec;
	enum m0_ha_obj_state    dstate;
	enum m0_ha_obj_state    pstate;
	int                     i = 0;
	int                     rc;

	M0_ENTRY();
	M0_ASSERT(scm->sc_op == CM_OP_REBALANCE);

	cc = &pc->pc_confc->cc_cache;
	m0_tl_for (pools, &pc->pc_pools, pool) {
		/* Skip mdpool, since only io pools are rebalanced. */
		if (m0_fid_eq(&pc->pc_md_pool->po_id, &pool->po_id) ||
		    (pc->pc_dix_pool != NULL &&
		     m0_fid_eq(&pc->pc_dix_pool->po_id, &pool->po_id)))
			continue;
		rc = m0_sns_cm_pool_ha_nvec_alloc(pool, M0_PNDS_SNS_REBALANCING,
						  &nvec);
		if (rc != 0) {
			M0_LOG(M0_DEBUG, "HA note allocation for pool"FID_F
					"failed with rc: %d",
					FID_P(&pool->po_id), rc);
			if (rc == -ENOENT)
				continue;
			M0_LOG(M0_ERROR, "HA note allocation failed with rc: %d", rc);
			goto out;
		}
		pstate = M0_NC_ONLINE;
		m0_tl_for(pool_failed_devs, &pool->po_failed_devices, pd) {
			if (pd->pd_state == M0_PNDS_SNS_REBALANCING) {
				dstate = M0_NC_ONLINE;
				rc = m0_conf_obj_find_lock(cc, &pd->pd_id,
							   &disk_obj);
				M0_ASSERT(rc == 0);
				disk = M0_CONF_CAST(disk_obj, m0_conf_drive);
				M0_ASSERT(disk != NULL);
				if (m0_sns_cm_disk_has_dirty_pver(cm, disk,
								  true) ||
				    m0_cm_is_dirty(cm)) {
					dstate = M0_NC_REBALANCE;
					pstate = M0_NC_REBALANCE;
				}
				nvec.nv_note[i].no_id = disk_obj->co_id;
				nvec.nv_note[i].no_state = dstate;
				M0_CNT_INC(i);
			}
		} m0_tl_endfor;
		/* Set pool ha note. */
		nvec.nv_note[i].no_id = pool->po_id;
		nvec.nv_note[i].no_state = pstate;
		m0_ha_local_state_set(&nvec);
		m0_free(nvec.nv_note);
		i = 0;
	} m0_tl_endfor;

out:
	m0_sns_cm_stop(cm);
	M0_LEAVE();
}

/**
 * Returns true iff the copy machine has enough space to receive all
 * the copy packets from the given relevant group "id".
 * Reserves buffers from incoming buffer pool struct m0_sns_cm::sc_ibp
 * corresponding to all the incoming copy packets.
 * e.g. sns repair copy machine checks if the incoming buffer pool has
 * enough free buffers to receive all the remote units corresponding
 * to a parity group.
 */
static int rebalance_cm_get_space_for(struct m0_cm *cm,
				      const struct m0_cm_ag_id *id,
				      size_t *count)
{
	struct m0_sns_cm          *scm = cm2sns(cm);
	struct m0_pdclust_layout  *pl;
	struct m0_fid              fid;
	struct m0_sns_cm_file_ctx *fctx;
	int64_t                    tot_inbufs;

	M0_PRE(cm != NULL && id != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	agid2fid(id, &fid);
	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	fctx = m0_sns_cm_fctx_locate(scm, &fid);
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);
	M0_ASSERT(fctx != NULL);
	pl = m0_layout_to_pdl(fctx->sf_layout);
	tot_inbufs = m0_sns_cm_incoming_reserve_bufs(scm, id);
	if (tot_inbufs <= 0)
		return -ENOENT;
	*count = tot_inbufs;
	return m0_sns_cm_has_space_for(scm, pl, tot_inbufs);
}

/** Copy machine operations. */
const struct m0_cm_ops sns_rebalance_ops = {
	.cmo_setup               = m0_sns_cm_setup,
	.cmo_prepare             = rebalance_cm_prepare,
	.cmo_start               = m0_sns_cm_start,
	.cmo_ag_alloc            = m0_sns_cm_rebalance_ag_alloc,
	.cmo_cp_alloc            = rebalance_cm_cp_alloc,
	.cmo_data_next           = m0_sns_cm_iter_next,
	.cmo_ag_next             = m0_sns_cm_ag_next,
	.cmo_get_space_for       = rebalance_cm_get_space_for,
	.cmo_sw_onwire_fop_setup = m0_sns_cm_rebalance_sw_onwire_fop_setup,
	.cmo_is_peer             = m0_sns_is_peer,
	.cmo_ha_msg		 = m0_sns_cm_ha_msg,
	.cmo_stop                = rebalance_cm_stop,
	.cmo_fini                = m0_sns_cm_fini
};

#undef M0_TRACE_SUBSYSTEM

/** @} SNSCM */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
