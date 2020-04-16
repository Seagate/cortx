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
 * Original creation date: 12/09/2012
 * Revision: Anup Barve <anup_barve@xyratex.com>
 * Revision date: 08/05/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/finject.h"

#include "fid/fid.h"
#include "ioservice/io_service.h" /* m0_ios_cdom_get */
#include "reqh/reqh.h"
#include "sns/parity_repair.h"

#include "sns/cm/cm_utils.h"
#include "sns/cm/ag.h"
#include "sns/cm/cp.h"
#include "sns/cm/cm.h"
#include "sns/cm/iter.h"
#include "sns/cm/file.h"
#include "cm/proxy.h"

/**
   @addtogroup SNSCMAG

   @{
 */

enum ag_iter_state {
	AIS_FID_NEXT,
	AIS_FID_LOCK,
	AIS_FID_ATTR,
	AIS_GROUP_NEXT,
	AIS_FINI,
	AIS_NR
};

static struct m0_sm_state_descr ai_sd[AIS_NR] = {
	[AIS_FID_LOCK] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "ag iter fid lock",
		.sd_allowed = M0_BITS(AIS_FID_ATTR, AIS_FID_NEXT, AIS_FINI)
	},
	[AIS_FID_NEXT] = {
		.sd_flags   = 0,
		.sd_name    = "ag iter fid next",
		.sd_allowed = M0_BITS(AIS_FID_LOCK, AIS_FINI)
	},
	[AIS_FID_ATTR] = {
		.sd_flags   = 0,
		.sd_name    = "ag iter fid attr",
		.sd_allowed = M0_BITS(AIS_GROUP_NEXT, AIS_FID_LOCK,
				      AIS_FID_NEXT, AIS_FINI)
	},
	[AIS_GROUP_NEXT] = {
		.sd_flags   = 0,
		.sd_name   = "ag iter group next",
		.sd_allowed = M0_BITS(AIS_FID_NEXT, AIS_FID_LOCK, AIS_FINI)
	},
	[AIS_FINI] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "ag iter fini",
		.sd_allowed = 0
	}
};

static const struct m0_sm_conf ai_sm_conf = {
	.scf_name      = "sm: ai_conf",
	.scf_nr_states = ARRAY_SIZE(ai_sd),
	.scf_state     = ai_sd
};

static enum ag_iter_state ai_state(struct m0_sns_cm_ag_iter *ai)
{
	return ai->ai_sm.sm_state;
}

static struct m0_sns_cm *ai2sns(struct m0_sns_cm_ag_iter *ai)
{
	return container_of(ai, struct m0_sns_cm, sc_ag_it);
}

static void ai_state_set(struct m0_sns_cm_ag_iter *ai, int state)
{
	m0_sm_state_set(&ai->ai_sm, state);
}

static bool _is_fid_valid(struct m0_sns_cm_ag_iter *ai, struct m0_fid *fid)
{
	struct m0_fid         fid_out = {0, 0};
	struct m0_sns_cm     *scm = ai2sns(ai);
	struct m0_cob_domain *cdom = scm->sc_cob_dom;
	struct m0_cob_nsrec  *nsrec;
	int                   rc;

	if (!m0_sns_cm_fid_is_valid(scm, fid))
		return false;
	rc = m0_cob_ns_rec_of(&cdom->cd_namespace, fid, &fid_out, &nsrec);
	if (rc == 0 && m0_fid_eq(fid, &fid_out))
		return true;
	return false;
}

static int ai_group_next(struct m0_sns_cm_ag_iter *ai)
{
	struct m0_cm_ag_id        ag_id = {};
	struct m0_sns_cm         *scm = ai2sns(ai);
	struct m0_cm             *cm = &scm->sc_base;
	struct m0_cm_aggr_group  *ag;
	struct m0_fom            *fom;
	struct m0_sns_cm_file_ctx *fctx = ai->ai_fctx;
	struct m0_pool_version    *pver;
	uint64_t                  group_last = ai->ai_group_last;
	uint64_t                  group = agid2group(&ai->ai_id_curr);
	uint64_t                  i;
	size_t                    nr_bufs;
	int                       rc = 0;

	/* Move to next file if pool version is dirty already. */
	pver = fctx->sf_pm->pm_pver;
	if (m0_sns_cm_pver_is_dirty(pver))
		goto fid_next;

	if (m0_cm_ag_id_is_set(&ai->ai_id_curr))
		++group;
	fom = &fctx->sf_scm->sc_base.cm_sw_update.swu_fom;
	for (i = group; i <= group_last; ++i) {
		m0_sns_cm_ag_agid_setup(&ai->ai_fid, i, &ag_id);
		if (!m0_sns_cm_ag_is_relevant(scm, fctx, &ag_id))
			continue;
		ag = m0_cm_aggr_group_locate(cm, &ag_id, true);
		if (ag != NULL)
			continue;
		rc = cm->cm_ops->cmo_get_space_for(cm, &ag_id, &nr_bufs);
		if (rc == -ENOSPC) {
			m0_sns_cm_buf_wait(&scm->sc_ibp, fom);
			rc = -ENOBUFS;
		}
		if (rc == 0) {
			m0_sns_cm_reserve_space(scm, nr_bufs);
			rc = m0_cm_aggr_group_alloc(cm, &ag_id, true, &ag);
			if (rc != 0)
				m0_sns_cm_cancel_reservation(scm, nr_bufs);
			if (rc == 0)
				ai->ai_id_next = ag_id;
		}
		if (rc < 0) {
			if (M0_IN(rc, (-ENOMEM, -ENOBUFS)))
				return M0_RC(rc);
			else if (M0_IN(rc,  (-ENOENT, -ESHUTDOWN))) {
				rc = 0;
				continue;
			} else {
				m0_sns_cm_pver_dirty_set(pver);
				rc = 0;
				goto fid_next;
			}
		}
	}

fid_next:
	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	m0_sns_cm_file_unlock(scm, &ai->ai_fid);
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);
	ai_state_set(ai, AIS_FID_NEXT);

	return M0_RC(rc);
}

static int ai_fid_attr(struct m0_sns_cm_ag_iter *ai)
{
	struct m0_sns_cm_file_ctx *fctx = ai->ai_fctx;
	struct m0_fom             *fom;
	int                        rc;

	fctx->sf_pm = ai->ai_pm;
	rc = m0_sns_cm_file_attr_and_layout(fctx);
	if (rc == -EAGAIN) {
		fom = &fctx->sf_scm->sc_base.cm_sw_update.swu_fom;
		m0_sns_cm_file_attr_and_layout_wait(fctx, fom);
		return M0_RC(M0_FSO_WAIT);
	}
	if (rc == 0) {
		ai->ai_group_last = fctx->sf_max_group;
		ai_state_set(ai, AIS_GROUP_NEXT);
	}
	if (rc == -ENOENT) {
		ai_state_set(ai, AIS_FID_NEXT);
		rc = 0;
	}

	return M0_RC(rc);
}

static int __file_lock(struct m0_sns_cm *scm, const struct m0_fid *fid,
                       struct m0_sns_cm_file_ctx **fctx)
{
	struct m0_fom *fom;
	int            rc;

	fom = &scm->sc_base.cm_sw_update.swu_fom;
	rc = m0_sns_cm_file_lock(scm, fid, fctx);
	if (rc == -EAGAIN) {
		M0_ASSERT(*fctx != NULL);
		rc = m0_sns_cm_file_lock_wait(*fctx, fom);
	}

	return M0_RC(rc);
}

static int ai_pm_set(struct m0_sns_cm_ag_iter *ai, struct m0_fid *pv_id)
{
	struct m0_sns_cm       *scm = ai2sns(ai);
	struct m0_reqh         *reqh = m0_sns_cm2reqh(scm);
	struct m0_pool_version *pv;
	struct m0_fid          *pver_id;
	struct m0_cob_nsrec    *nsrec;
	struct m0_fid           fid = {0, 0};
	int                     rc = 0;

	pver_id = pv_id;
	if (pver_id == NULL) {
		rc = m0_cob_ns_rec_of(&scm->sc_cob_dom->cd_namespace,
				      &ai->ai_fid, &fid, &nsrec);
		if (rc == 0)
			pver_id = &nsrec->cnr_pver;
	}
	if (rc == 0) {
		pv = m0_pool_version_find(reqh->rh_pools, pver_id);
		ai->ai_pm = &pv->pv_mach;
	}

	return M0_RC(rc);
}

static int ai_fid_lock(struct m0_sns_cm_ag_iter *ai)
{
	struct m0_sns_cm *scm = ai2sns(ai);
	int               rc = 0;

	if (!_is_fid_valid(ai, &ai->ai_fid)) {
		ai_state_set(ai, AIS_FID_NEXT);
		return M0_RC(0);
	}

	if (ai->ai_pm == NULL)
		rc = ai_pm_set(ai, NULL);

	if (rc == 0) {
		m0_mutex_lock(&scm->sc_file_ctx_mutex);
		rc = __file_lock(scm, &ai->ai_fid, &ai->ai_fctx);
		m0_mutex_unlock(&scm->sc_file_ctx_mutex);
	}

	if (rc == 0)
		ai_state_set(ai, AIS_FID_ATTR);

	return M0_RC(rc);
}

static int ai_fid_next(struct m0_sns_cm_ag_iter *ai)
{
	struct m0_fid        fid = {0, 0};
	struct m0_fid        fid_curr = ai->ai_fid;
	struct m0_sns_cm    *scm = ai2sns(ai);
	struct m0_cob_nsrec *nsrec;
	int                  rc = 0;

	do {
		M0_CNT_INC(fid_curr.f_key);
		rc = m0_cob_ns_rec_of(&scm->sc_cob_dom->cd_namespace,
				      &fid_curr, &fid, &nsrec);
		fid_curr = fid;
	} while (rc == 0 &&
		 (m0_fid_eq(&fid, &M0_COB_ROOT_FID) ||
		  m0_fid_eq(&fid, &M0_MDSERVICE_SLASH_FID)));

	if (rc == 0) {
		M0_SET0(&ai->ai_id_curr);
		ai->ai_fid = fid;
		rc = ai_pm_set(ai, &nsrec->cnr_pver);
		if (rc == 0)
			ai_state_set(ai, AIS_FID_LOCK);
	}

	if (rc == -ENOENT)
		rc = -ENODATA;

	return M0_RC(rc);
}

static int (*ai_action[])(struct m0_sns_cm_ag_iter *ai) = {
	[AIS_FID_NEXT]   = ai_fid_next,
	[AIS_FID_LOCK]   = ai_fid_lock,
	[AIS_FID_ATTR]   = ai_fid_attr,
	[AIS_GROUP_NEXT] = ai_group_next,
};

M0_INTERNAL int m0_sns_cm_ag__next(struct m0_sns_cm *scm,
				   const struct m0_cm_ag_id *id_curr,
				   struct m0_cm_ag_id *id_next)
{
	struct m0_sns_cm_ag_iter  *ai = &scm->sc_ag_it;
	struct m0_cm              *cm = &scm->sc_base;
	struct m0_sns_cm_file_ctx *fctx;
	struct m0_fid              fid;
	int                        rc;

	ai->ai_id_curr = *id_curr;
	agid2fid(&ai->ai_id_curr, &fid);
	fctx = ai->ai_fctx;
	/*
	 * Reset ai->ai_id_curr if ag_next iterator has reached to higher fid
	 * through AIS_FID_NEXT than @id_curr in-order to start processing from
	 */
	if (m0_fid_cmp(&ai->ai_fid, &fid) > 0)
		M0_SET0(&ai->ai_id_curr);
	if (m0_fid_cmp(&ai->ai_fid, &fid) < 0) {
		if (fctx != NULL &&
		    m0_sns_cm_fctx_state_get(fctx) >= M0_SCFS_LOCK_WAIT) {
			m0_mutex_lock(&scm->sc_file_ctx_mutex);
			m0_sns_cm_file_unlock(scm, &ai->ai_fid);
			m0_mutex_unlock(&scm->sc_file_ctx_mutex);
		}
		ai->ai_fid = fid;
		if (ai_state(ai) != AIS_FID_LOCK)
			ai_state_set(ai, AIS_FID_LOCK);
	}

	do {
		if ((cm->cm_quiesce || cm->cm_abort) && (M0_IN(ai_state(ai),
							 (AIS_FID_LOCK,
							  AIS_FID_NEXT, AIS_GROUP_NEXT)))) {
			M0_LOG(M0_DEBUG, "%lu: Got %s cmd", cm->cm_id,
					 cm->cm_quiesce ? "QUIESCE" : "ABORT");
			return M0_RC(-ENODATA);
		}
		rc = ai_action[ai_state(ai)](ai);
	} while (rc == 0);

	*id_next = ai->ai_id_next;
	if (rc == -EAGAIN)
		rc = M0_FSO_WAIT;

	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_ag_iter_init(struct m0_sns_cm_ag_iter *ai)
{
	struct m0_sns_cm     *scm = ai2sns(ai);

	M0_SET0(ai);
	m0_sm_init(&ai->ai_sm, &ai_sm_conf, AIS_FID_LOCK,
		   &scm->sc_base.cm_sm_group);

	return M0_RC(0);
}

M0_INTERNAL void m0_sns_cm_ag_iter_fini(struct m0_sns_cm_ag_iter *ai)
{
	ai_state_set(ai, AIS_FINI);
	m0_sm_fini(&ai->ai_sm);
}

M0_INTERNAL struct m0_cm *snsag2cm(const struct m0_sns_cm_ag *sag)
{
	return sag->sag_base.cag_cm;
}

M0_INTERNAL struct m0_sns_cm_ag *ag2snsag(const struct m0_cm_aggr_group *ag)
{
	return container_of(ag, struct m0_sns_cm_ag, sag_base);
}

M0_INTERNAL void m0_sns_cm_ag_agid_setup(const struct m0_fid *gob_fid, uint64_t group,
                                         struct m0_cm_ag_id *agid)
{
        agid->ai_hi.u_hi = gob_fid->f_container;
        agid->ai_hi.u_lo = gob_fid->f_key;
        agid->ai_lo.u_hi = 0;
        agid->ai_lo.u_lo = group;
}

M0_INTERNAL void agid2fid(const struct m0_cm_ag_id *id, struct m0_fid *fid)
{
	M0_PRE(id != NULL);
	M0_PRE(fid != NULL);

        m0_fid_set(fid, id->ai_hi.u_hi, id->ai_hi.u_lo);
}

M0_INTERNAL uint64_t agid2group(const struct m0_cm_ag_id *id)
{
	M0_PRE(id != NULL);

	return id->ai_lo.u_lo;
}

M0_INTERNAL uint64_t m0_sns_cm_ag_local_cp_nr(const struct m0_cm_aggr_group *ag)
{
	struct m0_fid              fid;
	struct m0_cm              *cm;
	struct m0_sns_cm          *scm;
	struct m0_sns_cm_ag       *sag = ag2snsag(ag);
	struct m0_sns_cm_file_ctx *fctx = sag->sag_fctx;
	uint64_t                   group;

	M0_ENTRY();
	M0_PRE(ag != NULL);

	agid2fid(&ag->cag_id, &fid);
	group = agid2group(&ag->cag_id);

	cm = ag->cag_cm;
	M0_ASSERT(cm != NULL);
	scm = cm2sns(cm);

	M0_LEAVE();
	return m0_sns_cm_ag_nr_local_units(scm, fctx, group);
}

M0_INTERNAL void m0_sns_cm_ag_fini(struct m0_sns_cm_ag *sag)
{
        struct m0_cm_aggr_group *ag;
        struct m0_cm            *cm;
        struct m0_sns_cm        *scm;

        M0_ENTRY();
        M0_PRE(sag != NULL);

	ag = &sag->sag_base;
        cm = ag->cag_cm;
        M0_ASSERT(cm != NULL);
	scm = cm2sns(cm);
	m0_bitmap_fini(&sag->sag_fmap);
	m0_cm_proxy_in_count_free(&sag->sag_proxy_in_count);
	m0_sns_cm_fctx_put(scm, &ag->cag_id);
	m0_cm_aggr_group_fini_and_progress(ag);
	m0_sns_cm_print_status(scm);
        M0_LEAVE();
}

M0_INTERNAL int m0_sns_cm_ag_init(struct m0_sns_cm_ag *sag,
				  struct m0_cm *cm,
				  const struct m0_cm_ag_id *id,
				  const struct m0_cm_aggr_group_ops *ag_ops,
				  bool has_incoming)
{
	struct m0_sns_cm           *scm = cm2sns(cm);
	struct m0_fid               gfid;
	struct m0_sns_cm_file_ctx  *fctx;
	struct m0_pdclust_layout   *pl;
	uint64_t                    upg;
	uint64_t                    f_nr;
	int                         rc = 0;
	struct m0_poolmach_state   *pm_state;

	M0_ENTRY("scm: %p, ag id:%p", cm, id);
	M0_PRE(sag != NULL && cm != NULL && id != NULL && ag_ops != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	agid2fid(id, &gfid);
	fctx = m0_sns_cm_fctx_get(scm, id);
	M0_ASSERT(fctx != NULL && fctx->sf_layout != NULL);
	pl = m0_layout_to_pdl(fctx->sf_layout);
	upg = m0_sns_cm_ag_size(pl);
	m0_bitmap_init(&sag->sag_fmap, upg);
	if (cm->cm_proxy_nr > 0)
		rc = m0_cm_proxy_in_count_alloc(&sag->sag_proxy_in_count,
						cm->cm_proxy_nr);
	if (rc != 0)
		goto fail;

	sag->sag_fctx = fctx;
	pm_state = fctx->sf_pm->pm_state;
	/* calculate actual failed number of units in this group. */
	f_nr = m0_sns_cm_ag_unrepaired_units(scm, fctx, id->ai_lo.u_lo, &sag->sag_fmap);
	if (f_nr == 0 || f_nr > m0_pdclust_K(pl) ||
	    pm_state->pst_nr_failures > pm_state->pst_max_device_failures ||
	    M0_FI_ENABLED("ag_init_failure")) {
		rc = M0_ERR_INFO(-EINVAL, "nr failures: %u group "M0_AG_F
				   " pst_nr_failures: %u pst_max_device_failures: %u",
				    (unsigned)f_nr, M0_AG_P(id), (unsigned)pm_state->pst_nr_failures,
				    (unsigned)pm_state->pst_max_device_failures);
		goto fail;
	}
	sag->sag_fnr = f_nr;
	if (has_incoming) {
		rc = m0_sns_cm_ag_in_cp_units(scm, id, fctx,
					      &sag->sag_incoming_cp_nr,
					      &sag->sag_incoming_units_nr,
					      &sag->sag_proxy_in_count);
		if (rc != 0)
			goto fail;
	}
	m0_cm_aggr_group_init(&sag->sag_base, cm, id, has_incoming, ag_ops);
	sag->sag_base.cag_cp_global_nr = m0_sns_cm_ag_nr_global_units(sag, pl);
	M0_LEAVE("ag: %p", sag);
	return M0_RC(rc);
fail:
	m0_bitmap_fini(&sag->sag_fmap);
	m0_sns_cm_fctx_put(scm, id);
	m0_cm_proxy_in_count_free(&sag->sag_proxy_in_count);
	return M0_ERR(rc);
}

M0_INTERNAL bool m0_sns_cm_ag_has_incoming_from(struct m0_cm_aggr_group *ag,
						struct m0_cm_proxy *proxy)
{
	struct m0_sns_cm_ag *sag = ag2snsag(ag);
	return sag->sag_proxy_in_count.p_count[proxy->px_id] > 0;
}

static bool ag_id_is_in(const struct m0_cm_ag_id *id, const struct m0_cm_sw *interval)
{
	return m0_cm_ag_id_cmp(id, &interval->sw_lo) >= 0 &&
	       m0_cm_ag_id_cmp(id, &interval->sw_hi) <= 0;
}

M0_INTERNAL bool m0_sns_cm_ag_is_frozen_on(struct m0_cm_aggr_group *ag, struct m0_cm_proxy *pxy)
{
	struct m0_cm        *cm = ag->cag_cm;
	struct m0_sns_cm_ag *sag = ag2snsag(ag);

	M0_ENTRY();
	M0_PRE(m0_cm_ag_is_locked(ag));

	/*
	 * Proxy can be NULL if cleanup is invoked for local node, this can happen
	 * in case of a single node setup.
	 * If proxy is not NULL then find out if there are any incoming copy packets
	 * from the given proxy that is already completed and the copy packets will
	 * no longer be arriving.
	 */
	if (pxy != NULL && ag->cag_ops->cago_has_incoming_from(ag, pxy)) {
		if ((M0_IN(pxy->px_status, (M0_PX_COMPLETE)) &&
		     !ag_id_is_in(&ag->cag_id, &pxy->px_out_interval)) ||
		     M0_IN(pxy->px_status, (M0_PX_STOP, M0_PX_FAILED)) ||
		     m0_cm_ag_id_cmp(&ag->cag_id, &pxy->px_out_interval.sw_lo) < 0) {
			sag->sag_proxy_in_count.p_count[pxy->px_id] = 0;
			M0_CNT_INC(sag->sag_not_coming);
		}
	}

	ag->cag_is_frozen = sag->sag_not_coming > 0;

	if (!ag->cag_is_frozen && m0_cm_cp_pump_is_complete(&cm->cm_cp_pump) &&
	    sag->sag_cp_created_nr != ag->cag_cp_local_nr)
			ag->cag_is_frozen = true;

	return ag->cag_is_frozen;

	M0_LEAVE();
}

M0_INTERNAL bool m0_sns_cm_ag_has_data(struct m0_sns_cm_file_ctx *fctx, uint64_t group)
{
	struct m0_pdclust_layout *pl;
	uint32_t                  nr_max_du;

	nr_max_du = m0_sns_cm_file_data_units(fctx);
	pl = m0_layout_to_pdl(fctx->sf_layout);
	return !m0_sns_cm_file_unit_is_EOF(pl, nr_max_du, group, 0);
}

/** @} SNSCMAG */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
