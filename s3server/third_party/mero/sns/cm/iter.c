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
 * Original creation date: 10/08/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/finject.h"

#include "cob/cob.h"
#include "mdstore/mdstore.h"
#include "reqh/reqh.h"
#include "ioservice/io_service.h"

#include "cm/proxy.h"
#include "sns/parity_repair.h"
#include "sns/cm/cm.h"
#include "sns/cm/cp.h"
#include "sns/cm/ag.h"
#include "sns/cm/cm_utils.h"
#include "sns/cm/file.h"
#include "ioservice/fid_convert.h" /* m0_fid_gob_make */

/**
  @addtogroup SNSCM

  @{
*/

enum {
        SNS_REPAIR_ITER_MAGIX = 0x33BAADF00DCAFE77,
};

static const struct m0_bob_type iter_bob = {
	.bt_name = "sns cm data iterator",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_sns_cm_iter, si_magix),
	.bt_magix = SNS_REPAIR_ITER_MAGIX,
	.bt_check = NULL
};

M0_BOB_DEFINE(static, &iter_bob, m0_sns_cm_iter);

enum cm_data_iter_phase {
	ITPH_INIT,
	/**
	 * Iterator is in this phase when m0_cm:cm_ops::cmo_data_next() is first
	 * invoked as part of m0_cm_start() and m0_cm_cp_pump_start(), from
	 * m0_cm_data_next(). This starts the sns repair data iterator and sets
	 * the iterator to first local data unit of a parity group from a GOB
	 * (file) that needs repair.
	 */
	ITPH_IDLE,
	/**
	 * Iterator is in this phase until all the local data units of a
	 * parity group are serviced (i.e. copy packets are created).
	 */
	ITPH_COB_NEXT,
	/**
	 * Iterator transitions to this phase to select next parity group
	 * that needs to be repaired, and has local data units.
	 */
	ITPH_GROUP_NEXT,
	/**
	 * Iterator transitions to this phase in-order to select next GOB that
	 * needs repair.
	 */
	ITPH_FID_NEXT,
	/** Iterator tries to acquire the async rm file lock in this phase. */
	ITPH_FID_LOCK,
	/** Iterator waits for the rm file lock to be acquired in this phase. */
	ITPH_FID_LOCK_WAIT,
	/** Fetch file attributes and layout. */
	ITPH_FID_ATTR_LAYOUT,
	/**
	 * Once next local data unit of parity group needing repair is calculated
	 * along with its corresponding COB fid, the pre allocated copy packet
	 * by the copy machine pump FOM is populated with required details.
	 * @see struct m0_sns_cm_cp
	 */
	ITPH_CP_SETUP,
	/**
	 * Once the aggregation group is created and initialised, we need to
	 * acquire buffers for accumulator copy packet in the aggregation group
	 * falure contexts. This operation may block.
	 * @see m0_sns_cm_ag::sag_fc
	 * @see struct m0_sns_cm_ag_failure_ctx
	 */
	ITPH_AG_SETUP,
	/**
	 * Iterator is finalised after the sns repair operation is complete.
	 * This is done as part of m0_cm_stop().
	 */
	ITPH_FINI,
	ITPH_NR
};

M0_INTERNAL struct m0_sns_cm *it2sns(struct m0_sns_cm_iter *it)
{
	return container_of(it, struct m0_sns_cm, sc_it);
}

/**
 * Returns current iterator phase.
 */
static enum cm_data_iter_phase iter_phase(const struct m0_sns_cm_iter *it)
{
	return it->si_sm.sm_state;
}

/**
 * Sets iterator phase.
 */
static void iter_phase_set(struct m0_sns_cm_iter *it, int phase)
{
	m0_sm_state_set(&it->si_sm, phase);
}

static bool
iter_layout_invariant(enum cm_data_iter_phase phase,
                      const struct m0_sns_cm_iter_file_ctx *ifc)
{
	struct m0_pdclust_layout *pl =
				 m0_layout_to_pdl(ifc->ifc_fctx->sf_layout);

	return ergo(M0_IN(phase, (ITPH_COB_NEXT, ITPH_GROUP_NEXT,
				  ITPH_CP_SETUP)),
		    pl != NULL &&
		    ifc->ifc_fctx->sf_pi != NULL && ifc->ifc_upg != 0 &&
		    ifc->ifc_dpupg != 0 && m0_fid_is_set(&ifc->ifc_gfid)) &&
	       ergo(M0_IN(phase, (ITPH_CP_SETUP)), ifc->ifc_group_last >= 0 &&
		    m0_fid_is_set(&ifc->ifc_cob_fid) &&
		    ifc->ifc_sa.sa_group <= ifc->ifc_group_last &&
		    ifc->ifc_sa.sa_unit <= ifc->ifc_upg &&
		    ifc->ifc_ta.ta_obj <= m0_pdclust_P(pl));
}

static bool iter_invariant(const struct m0_sns_cm_iter *it)
{
	return it != NULL && m0_sns_cm_iter_bob_check(it) &&
	       it->si_cp != NULL &&
	       ergo(it->si_fc.ifc_fctx != NULL &&
		    it->si_fc.ifc_fctx->sf_layout != NULL,
		    iter_layout_invariant(iter_phase(it), &it->si_fc));
}

/**
 * Calculates COB fid for m0_sns_cm_iter_file_ctx::ifc_sa.
 * Saves calculated struct m0_pdclust_tgt_addr in
 * m0_sns_cm_iter_file_ctx::ifc_ta.
 */
static void unit_to_cobfid(struct m0_sns_cm_iter_file_ctx *ifc,
			   struct m0_fid *cob_fid_out)
{
	struct m0_pdclust_src_addr  *sa;
	struct m0_pdclust_tgt_addr  *ta;

	sa = &ifc->ifc_sa;
	ta = &ifc->ifc_ta;
	ifc->ifc_cob_is_spare_unit = m0_sns_cm_unit_is_spare(ifc->ifc_fctx,
							     sa->sa_group,
							     sa->sa_unit);
	m0_sns_cm_unit2cobfid(ifc->ifc_fctx, sa, ta, cob_fid_out);
}

/* Uses name space iterator. */
M0_INTERNAL int __fid_next(struct m0_sns_cm_iter *it, struct m0_fid *fid_next)
{
	struct m0_sns_cm_iter_file_ctx *ifc = &it->si_fc;
	struct m0_cob_nsrec            *nsrec;
	struct m0_pool_version         *pv;
	struct m0_sns_cm               *scm = it2sns(it);
	struct m0_reqh                 *reqh = m0_sns_cm2reqh(scm);
	int                             rc;

	M0_ENTRY("it = %p", it);

	rc = m0_cob_ns_iter_next(&it->si_cns_it, fid_next, &nsrec);
	if (rc == 0) {
		pv = m0_pool_version_find(reqh->rh_pools, &nsrec->cnr_pver);
		ifc->ifc_pm = &pv->pv_mach;
	}

	return M0_RC(rc);
}

static int __file_context_init(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm          *scm = it2sns(it);
	struct m0_cm              *cm = &scm->sc_base;
	struct m0_pdclust_layout  *pl;
	struct m0_cm_ag_id        *out_last;
	struct m0_sns_cm_file_ctx *fctx = it->si_fc.ifc_fctx;

	pl = m0_layout_to_pdl(it->si_fc.ifc_fctx->sf_layout);
	if (pl == NULL)
		return M0_RC(M0_FSO_WAIT);

	/*
	 * We need only the number of parity units equivalent
	 * to the number of failures.
	 */
	it->si_fc.ifc_dpupg = m0_sns_cm_ag_nr_data_units(pl) +
				m0_sns_cm_ag_nr_parity_units(pl);
	it->si_fc.ifc_upg = m0_sns_cm_ag_size(pl);
	M0_CNT_INC(it->si_total_files);
	out_last = &cm->cm_last_processed_out;
	if (m0_cm_ag_id_is_set(out_last)) {
		it->si_fc.ifc_sa.sa_group = agid2group(out_last);
	 } else
		it->si_fc.ifc_sa.sa_group = 0;

	it->si_fc.ifc_sa.sa_unit = 0;
	it->si_ag = NULL;
	M0_SET0(out_last);
	it->si_fc.ifc_group_last = fctx->sf_max_group;

	return M0_RC(0);
}

static int group__next(struct m0_sns_cm_iter *it)
{
	int rc;

	rc = __file_context_init(it);
	if (rc == 0)
		iter_phase_set(it, ITPH_GROUP_NEXT);
	return M0_RC(rc);
}

/** Fetches file attributes and layout for GOB. */
static int iter_fid_attr_layout(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm_file_ctx *fctx = it->si_fc.ifc_fctx;
	int                        rc;

	M0_PRE(fctx != NULL);

	fctx->sf_pm = it->si_fc.ifc_pm;
	rc = m0_sns_cm_file_attr_and_layout(fctx);
	if (rc == 0) {
		M0_ASSERT(fctx->sf_pi != NULL);
		rc = group__next(it);
		M0_ASSERT(fctx->sf_pi != NULL);
	}
	if (rc == -EAGAIN) {
		m0_sns_cm_file_attr_and_layout_wait(fctx, it->si_fom);
		return M0_RC(M0_FSO_WAIT);
	}

	if (rc == -ENOENT) {
		iter_phase_set(it, ITPH_FID_NEXT);
		rc = 0;
	}

	return M0_RC(rc);
}

static int iter_fid_lock(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm *scm;
	struct m0_fid    *fid;
	int		  rc = 0;

	M0_ENTRY();
	M0_PRE(it != NULL);

	scm = it2sns(it);
	fid = &it->si_fc.ifc_gfid;
	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	rc = m0_sns_cm_file_lock(scm, fid, &it->si_fc.ifc_fctx);
	if (rc == 0)
		iter_phase_set(it, ITPH_FID_ATTR_LAYOUT);
	if (rc == -EAGAIN) {
		iter_phase_set(it, ITPH_FID_LOCK_WAIT);
		rc = 0;
	}
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);
	return M0_RC(rc);
}

static int iter_fid_lock_wait(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm          *scm;
	int		           rc = 0;
	struct m0_sns_cm_file_ctx *fctx;

	M0_ENTRY();
	M0_PRE(it != NULL);

	scm = it2sns(it);
	fctx = it->si_fc.ifc_fctx;
	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	rc = m0_sns_cm_file_lock_wait(fctx, it->si_fom);
	if (rc == -EAGAIN) {
		rc = M0_FSO_WAIT;
		goto end;
	}
	if (rc == 0)
		iter_phase_set(it, ITPH_FID_ATTR_LAYOUT);

end:
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);
	return M0_RC(rc);
}

/** Fetches next GOB fid. */
static int iter_fid_next(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm_iter_file_ctx  *ifc = &it->si_fc;
	struct m0_fid                    fid_next;
	int                              rc;
	M0_ENTRY("it = %p", it);

	m0_fid_gob_make(&fid_next, 0, 0);
	ifc->ifc_fctx = NULL;

	/* Get current GOB fid saved in the iterator. */
	do {
		rc = __fid_next(it, &fid_next);
	} while (rc == 0 && (m0_fid_eq(&fid_next, &M0_COB_ROOT_FID)     ||
			     m0_fid_eq(&fid_next, &M0_MDSERVICE_SLASH_FID)));
	if (rc == 0) {
		/* Save next GOB fid in the iterator. */
		ifc->ifc_gfid = fid_next;
	}
	/* fini old layout instance and put old layout */
	if (ifc->ifc_fctx != NULL && ifc->ifc_fctx->sf_layout != NULL) {
		if (M0_FI_ENABLED("ut_fid_next"))
			m0_layout_put(ifc->ifc_fctx->sf_layout);
	}
	if (rc == -ENOENT) {
		M0_LOG(M0_DEBUG, "no more data: returning -ENODATA last fid"
		       FID_F, FID_P(&ifc->ifc_gfid));
		return M0_RC(-ENODATA);
	}

	iter_phase_set(it, ITPH_FID_LOCK);
	return M0_RC(rc);
}

static bool __has_incoming(struct m0_sns_cm *scm,
			   struct m0_sns_cm_file_ctx *fctx, uint64_t group)
{
	struct m0_cm_ag_id agid;

	M0_PRE(scm != NULL && fctx != NULL);

	m0_sns_cm_ag_agid_setup(&fctx->sf_fid, group, &agid);
	M0_LOG(M0_DEBUG, "agid [%lu] [%lu] [%lu] [%lu]",
	       agid.ai_hi.u_hi, agid.ai_hi.u_lo,
	       agid.ai_lo.u_hi, agid.ai_lo.u_lo);
	return m0_sns_cm_ag_is_relevant(scm, fctx, &agid);
}


static bool __group_skip(struct m0_sns_cm_iter *it, uint64_t group)
{
	struct m0_sns_cm_iter_file_ctx *ifc = &it->si_fc;
	struct m0_sns_cm_file_ctx      *fctx = ifc->ifc_fctx;
	struct m0_fid                   cobfid;
	struct m0_pdclust_src_addr      sa;
	struct m0_pdclust_tgt_addr      ta;
	int                             i;
	struct m0_poolmach             *pm = fctx->sf_pm;
 	struct m0_sns_cm               *scm = it2sns(it);

	M0_ENTRY("it: %p group: %lu", it, (unsigned long)group);

	for (i = 0; i < ifc->ifc_upg; ++i) {
		sa.sa_unit = i;
		sa.sa_group = group;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (scm->sc_helpers->sch_is_cob_failed(pm, ta.ta_obj) &&
		    !m0_sns_cm_is_cob_repaired(pm, ta.ta_obj) &&
		    !m0_sns_cm_unit_is_spare(fctx, group, sa.sa_unit))
			return false;
	}

	return true;
}


static int __group_alloc(struct m0_sns_cm *scm, struct m0_fid *gfid,
			 uint64_t group, struct m0_pdclust_layout *pl,
			 bool has_incoming, struct m0_cm_aggr_group **ag)
{
	struct m0_cm        *cm = &scm->sc_base;
	struct m0_cm_ag_id   agid;
	size_t               nr_bufs;
	int                  rc = 0;

	m0_sns_cm_ag_agid_setup(gfid, group, &agid);
	/*
	 * Allocate new aggregation group for the given aggregation
	 * group identifier.
	 * Check if the aggregation group has incoming copy packets, if
	 * yes, check if the aggregation group was already created and
	 * processed through sliding window.
	 * Thus if sliding_window_lo < agid < sliding_window_hi then the
	 * group was already processed and we proceed to next group.
	 */
	if (has_incoming) {
		if (m0_cm_ag_id_cmp(&agid,
				    &cm->cm_sw_last_updated_hi) <= 0) {
			*ag = m0_cm_aggr_group_locate(cm, &agid, has_incoming);
			/* SNS repair abort or quiesce ca lead to frozen
			 * aggregation groups. It is possible that the
			 * aggregation group was detected frozen in such
			 * incident and was finalised. In regular sns operation
			 * this situation is not expected.
			 */
			if (*ag == NULL) {
				M0_LOG(M0_DEBUG, "group "M0_AG_F" not found",
					M0_AG_P(&agid));
				rc = -ENOENT;
			}
			goto out;
		}
	}

	if (has_incoming) {
		rc = cm->cm_ops->cmo_get_space_for(cm, &agid, &nr_bufs);
		if (rc == 0)
			m0_sns_cm_reserve_space(scm, nr_bufs);
		M0_LOG(M0_DEBUG, "agid [%lu] [%lu] [%lu] [%lu]",
		       agid.ai_hi.u_hi, agid.ai_hi.u_lo,
		       agid.ai_lo.u_hi, agid.ai_lo.u_lo);
	}
	if (rc == 0) {
		rc = m0_cm_aggr_group_alloc(cm, &agid, has_incoming, ag);
		if (rc != 0 && has_incoming)
			m0_sns_cm_cancel_reservation(scm, nr_bufs);
	}

out:
	if (*ag != NULL && has_incoming) {
		if ((*ag)->cag_cp_local_nr > 0 &&
		    !aggr_grps_out_tlink_is_in(*ag))
			m0_cm_aggr_group_add(cm, *ag, false);
	}
	return M0_RC(rc);
}

/**
 * Finds parity group having units belonging to the failed container.
 * This iterates through each parity group of the file, and its units.
 * A COB id is calculated for each unit and checked if ti belongs to the
 * failed container, if yes then the group is selected for processing.
 * This is invoked from ITPH_GROUP_NEXT phase.
 */
static int __group_next(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm               *scm = it2sns(it);
	struct m0_sns_cm_iter_file_ctx *ifc;
	struct m0_sns_cm_file_ctx      *fctx;
	struct m0_pdclust_src_addr     *sa;
	struct m0_fid                  *gfid;
	struct m0_pdclust_layout       *pl;
	uint64_t                        group;
	uint64_t                        nrlu = 0;
	bool                            has_incoming = false;
	int                             rc = 0;
	struct m0_poolmach             *pm;

	M0_ENTRY("it = %p", it);

	ifc = &it->si_fc;
	fctx = ifc->ifc_fctx;
	pl = m0_layout_to_pdl(fctx->sf_layout);
	sa = &ifc->ifc_sa;
	gfid = &ifc->ifc_gfid;
	if (it->si_ag != NULL)
		m0_cm_ag_put(it->si_ag);
	it->si_ag = NULL;
	pm = fctx->sf_pm;
	if (m0_sns_cm_pver_is_dirty(pm->pm_pver))
		goto fid_next;
	for (group = sa->sa_group; group <= ifc->ifc_group_last; ++group) {
		if (__group_skip(it, group))
			continue;
		has_incoming = __has_incoming(scm, ifc->ifc_fctx, group);
		if (!has_incoming)
			nrlu = m0_sns_cm_ag_nr_local_units(scm, ifc->ifc_fctx,
							   group);
		if (has_incoming || nrlu > 0) {
			rc = __group_alloc(scm, gfid, group, pl, has_incoming,
					   &it->si_ag);
			if (rc == -ENOENT) {
				rc = 0;
				continue;
			}
			if (it->si_ag != NULL)
				m0_cm_ag_get(it->si_ag);
			if (rc != 0) {
				if (rc == -ENOBUFS)
					iter_phase_set(it, ITPH_AG_SETUP);
				if (rc == -EINVAL) {
					m0_sns_cm_pver_dirty_set(pm->pm_pver);
					rc = 0;
					goto fid_next;
				}
				if (M0_IN(rc, (-ENOENT, -ESHUTDOWN))) {
					rc = 0;
					continue;
				}
			}
			ifc->ifc_sa.sa_group = group;
			ifc->ifc_sa.sa_unit = 0;
			if (rc == 0)
				iter_phase_set(it, ITPH_COB_NEXT);
			goto out;
		}
	}

fid_next:
	/* Put the reference on the file lock taken in ITPH_FID_NEXT phase. */
	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	m0_sns_cm_file_unlock(scm, gfid);
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);
	it->si_fc.ifc_fctx = NULL;
	iter_phase_set(it, ITPH_FID_NEXT);
out:
	return M0_RC(rc);
}

/**
 * Finds the next parity group to process.
 * @note This operation may block while fetching the file size, as part of file
 * attributes.
 */
static int iter_group_next(struct m0_sns_cm_iter *it)
{
	return __group_next(it);
}

/**
 * Configures aggregation group, acquires buffers for accumulator copy packet
 * in the aggregation group failure contexts.
 *
 * @see struct m0_sns_cm_ag::sag_fc
 * @see struct m0_sns_cm_ag_failure_ctx
 * @see m0_sns_cm_ag_setup()
 */
static int iter_ag_setup(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm          *scm = it2sns(it);
	struct m0_cm_aggr_group   *ag;
	struct m0_sns_cm_ag       *sag;
	struct m0_sns_cm_file_ctx *fctx;
	struct m0_pdclust_layout  *pl;
	int                        rc;

	fctx = it->si_fc.ifc_fctx;
	pl = m0_layout_to_pdl(fctx->sf_layout);
	ag = it->si_ag;
	M0_ASSERT(ag != NULL);
	sag = ag2snsag(ag);
	rc = scm->sc_helpers->sch_ag_setup(sag, pl);
	if (rc == 0)
		iter_phase_set(it, ITPH_COB_NEXT);

	return M0_RC(rc);
}

static bool unit_has_data(struct m0_sns_cm *scm, uint32_t unit)
{
	struct m0_sns_cm_iter_file_ctx *ifc = &scm->sc_it.si_fc;
	struct m0_pdclust_layout       *pl;
	enum m0_cm_op                   op = scm->sc_op;

	pl = m0_layout_to_pdl(ifc->ifc_fctx->sf_layout);
	switch(op) {
	case CM_OP_REPAIR:
		return !ifc->ifc_cob_is_spare_unit;
	case CM_OP_REBALANCE:
		if (m0_pdclust_unit_classify(pl, unit) == M0_PUT_SPARE)
			return !ifc->ifc_cob_is_spare_unit;
		break;
	default:
		M0_IMPOSSIBLE("Bad operation");
	}

	return false;
}

/**
 * Configures the given copy packet with aggregation group and stob details.
 */
static int iter_cp_setup(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm               *scm = it2sns(it);
	struct m0_pdclust_layout       *pl;
	struct m0_sns_cm_iter_file_ctx *ifc;
	struct m0_sns_cm_file_ctx      *fctx;
	struct m0_sns_cm_cp            *scp;
	struct m0_sns_cm_ag            *sag;
	bool                            has_data;
	uint64_t                        group;
	uint64_t                        stob_offset;
	uint64_t                        cp_data_seg_nr;
	uint64_t                        ag_cp_idx;
	int                             rc = 0;

	M0_ENTRY("it = %p", it);

	ifc = &it->si_fc;
	fctx = ifc->ifc_fctx;
	pl = m0_layout_to_pdl(fctx->sf_layout);
	group = ifc->ifc_sa.sa_group;
	has_data = unit_has_data(scm, ifc->ifc_sa.sa_unit - 1);
	if (!has_data)
		goto out;
	stob_offset = ifc->ifc_ta.ta_frame *
		      m0_pdclust_unit_size(pl);
	scp = it->si_cp;
	if (scp->sc_base.c_ag == NULL)
		m0_cm_ag_cp_add(it->si_ag, &scp->sc_base);
	sag = ag2snsag(scp->sc_base.c_ag);
	scp->sc_is_local = true;
	cp_data_seg_nr = m0_sns_cm_data_seg_nr(scm, pl);
	/*
	 * ifc->ifc_sa.sa_unit has gotten one index ahead. Hence actual
	 * index of the copy packet is (ifc->ifc_sa.sa_unit - 1).
	 * see iter_cob_next().
	 */
	ag_cp_idx = ifc->ifc_sa.sa_unit - 1;
	/*
	 * If the aggregation group unit to be read is a spare unit
	 * containing data then map the spare unit to its corresponding
	 * failed data/parity unit in the aggregation group @ag.
	 * This is required to mark the appropriate data/parity unit of
	 * which this spare contains data.
	 */

	if (m0_pdclust_unit_classify(pl, ag_cp_idx) == M0_PUT_SPARE) {
		m0_sns_cm_fctx_lock(fctx);
		rc = m0_sns_repair_data_map(fctx->sf_pm, pl,
					    fctx->sf_pi, group,
					    ag_cp_idx, &ag_cp_idx);
		m0_sns_cm_fctx_unlock(fctx);
		if (rc != 0)
			return M0_RC(rc);
	}
	rc = m0_sns_cm_cp_setup(scp, &ifc->ifc_cob_fid, stob_offset,
				cp_data_seg_nr, ~0, ag_cp_idx);
	if (rc < 0)
		return M0_RC(rc);

	M0_CNT_INC(sag->sag_cp_created_nr);
	rc = M0_FSO_AGAIN;
out:
	iter_phase_set(it, ITPH_COB_NEXT);
	return M0_RC(rc);
}

/**
 * Finds next local COB corresponding to a unit in the parity group to perform
 * read/write. For each unit in the given parity group, it calculates its
 * corresponding COB fid, and checks if the COB is local. If no local COB is
 * found for a given parity group after iterating through all its units, next
 * parity group is calculated, else the pre-allocated copy packet is populated
 * with required stob details details.
 * @see iter_cp_setup()
 * @note cob_next returns COB fid only for local data and parity units in a
 * parity group.
 */
static int iter_cob_next(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm_iter_file_ctx *ifc;
	struct m0_fid                  *cob_fid;
	struct m0_pdclust_src_addr     *sa;
	struct m0_sns_cm               *scm;
	enum m0_sns_cm_local_unit_type  ut;

	M0_ENTRY("it=%p", it);

	ifc = &it->si_fc;
	sa = &ifc->ifc_sa;
	scm = it2sns(it);
	cob_fid = &ifc->ifc_cob_fid;
	it->si_cp->sc_is_hole_eof = false;

	do {
		if (sa->sa_unit >= ifc->ifc_upg) {
			++sa->sa_group;
			iter_phase_set(it, ITPH_GROUP_NEXT);
			return M0_RC(0);
		}
		/*
		 * Calculate COB fid corresponding to the unit and advance
		 * scm->sc_it.si_src::sa_unit to next unit in the parity
		 * group. If this is the last unit in the parity group then
		 * proceed to next parity group in the GOB.
		 */
		unit_to_cobfid(ifc, cob_fid);
		ut = m0_sns_cm_local_unit_type_get(ifc->ifc_fctx, sa->sa_group,
						   sa->sa_unit);
		++sa->sa_unit;
	} while (ut == M0_SNS_CM_UNIT_INVALID ||
		 scm->sc_helpers->sch_is_cob_failed(ifc->ifc_fctx->sf_pm,
				                    ifc->ifc_ta.ta_obj));

	if (ut == M0_SNS_CM_UNIT_HOLE_EOF)
		it->si_cp->sc_is_hole_eof = true;

	if (M0_IN(ut, (M0_SNS_CM_UNIT_LOCAL, M0_SNS_CM_UNIT_HOLE_EOF)))
		iter_phase_set(it, ITPH_CP_SETUP);

	return M0_RC(0);
}

/**
 * Transitions the data iterator (m0_sns_cm::sc_it) to ITPH_FID_NEXT
 * in-order to find the first GOB and parity group that needs repair.
 */
M0_INTERNAL int iter_idle(struct m0_sns_cm_iter *it)
{
	iter_phase_set(it, ITPH_FID_NEXT);

	return 0;
}

static int (*iter_action[])(struct m0_sns_cm_iter *it) = {
	[ITPH_IDLE]                  = iter_idle,
	[ITPH_COB_NEXT]              = iter_cob_next,
	[ITPH_GROUP_NEXT]            = iter_group_next,
	[ITPH_FID_NEXT]              = iter_fid_next,
	[ITPH_FID_LOCK]              = iter_fid_lock,
	[ITPH_FID_LOCK_WAIT]         = iter_fid_lock_wait,
	[ITPH_FID_ATTR_LAYOUT]       = iter_fid_attr_layout,
	[ITPH_CP_SETUP]              = iter_cp_setup,
	[ITPH_AG_SETUP]              = iter_ag_setup,
};

/**
 * Calculates next data object to be re-structured and accordingly populates
 * the given copy packet.
 */
M0_INTERNAL int m0_sns_cm_iter_next(struct m0_cm *cm, struct m0_cm_cp *cp)
{
	struct m0_sns_cm      *scm;
	struct m0_sns_cm_iter *it;
	int                    rc;
	M0_ENTRY("cm = %p, cp = %p", cm, cp);

	M0_PRE(cm != NULL && cp != NULL);

	scm = cm2sns(cm);
	it = &scm->sc_it;
	it->si_cp = cp2snscp(cp);
	do {
		if (cm->cm_quiesce || cm->cm_abort) {
			if (M0_IN(iter_phase(it), (ITPH_FID_NEXT,
						   ITPH_GROUP_NEXT))) {
				M0_LOG(M0_DEBUG,
				       "%lu: Got %s cmd: returning -ENODATA",
					cm->cm_id,
					cm->cm_quiesce ? "QUIESCE" : "ABORT");
				if (iter_phase(it) == ITPH_GROUP_NEXT &&
				    it->si_ag != NULL)
					m0_cm_ag_put(it->si_ag);

				return M0_RC(-ENODATA);
			}
		}
		rc = iter_action[iter_phase(it)](it);
		M0_ASSERT(iter_invariant(it));
	} while (rc == 0);

	if (rc == -ENODATA)
		iter_phase_set(it, ITPH_IDLE);

	if (M0_IN(rc, (-ENOBUFS, -ENOSPC))) {
		m0_sns_cm_buf_wait(rc == -ENOSPC ? &scm->sc_ibp : &scm->sc_obp,
				   it->si_fom);
		rc = -ENOBUFS;
	}

	return M0_RC(rc);
}

static struct m0_sm_state_descr cm_iter_sd[ITPH_NR] = {
	[ITPH_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "iter init",
		.sd_allowed = M0_BITS(ITPH_IDLE, ITPH_FINI)
	},
	[ITPH_IDLE] = {
		.sd_flags   = 0,
		.sd_name    = "iter idle",
		.sd_allowed = M0_BITS(ITPH_FID_NEXT, ITPH_FINI)
	},
	[ITPH_COB_NEXT] = {
		.sd_flags   = 0,
		.sd_name    = "COB next",
		.sd_allowed = M0_BITS(ITPH_GROUP_NEXT, ITPH_CP_SETUP, ITPH_IDLE)
	},
	[ITPH_GROUP_NEXT] = {
		.sd_flags   = 0,
		.sd_name    = "group next",
		.sd_allowed = M0_BITS(ITPH_COB_NEXT, ITPH_AG_SETUP,
				      ITPH_FID_NEXT, ITPH_IDLE)
	},
	[ITPH_FID_NEXT] = {
		.sd_flags   = 0,
		.sd_name    = "FID next",
		.sd_allowed = M0_BITS(ITPH_GROUP_NEXT, ITPH_FID_LOCK,
				      ITPH_IDLE)
	},
	[ITPH_FID_LOCK] = {
		.sd_flags   = 0,
		.sd_name    = "File lock wait",
		.sd_allowed = M0_BITS(ITPH_FID_ATTR_LAYOUT, ITPH_FID_LOCK_WAIT)
	},
	[ITPH_FID_LOCK_WAIT] = {
		.sd_flags   = 0,
		.sd_name    = "File lock wait",
		.sd_allowed = M0_BITS(ITPH_FID_ATTR_LAYOUT)
	},
	[ITPH_FID_ATTR_LAYOUT] = {
		.sd_flags   = 0,
		.sd_name    = "File attr and layout fetch",
		.sd_allowed = M0_BITS(ITPH_GROUP_NEXT, ITPH_FID_NEXT, ITPH_IDLE)
	},
	[ITPH_CP_SETUP] = {
		.sd_flags   = 0,
		.sd_name    = "cp setup",
		.sd_allowed = M0_BITS(ITPH_COB_NEXT)
	},
	[ITPH_AG_SETUP] = {
		.sd_flags   = 0,
		.sd_name    = "ag setup",
		.sd_allowed = M0_BITS(ITPH_COB_NEXT)
	},
	[ITPH_FINI] = {
		.sd_flags = M0_SDF_TERMINAL,
		.sd_name  = "cm iter fini",
		.sd_allowed = 0
	}
};

static const struct m0_sm_conf cm_iter_sm_conf = {
	.scf_name      = "sm: cm_iter_conf",
	.scf_nr_states = ARRAY_SIZE(cm_iter_sd),
	.scf_state     = cm_iter_sd
};

M0_INTERNAL int m0_sns_cm_iter_init(struct m0_sns_cm_iter *it)
{
	struct m0_sns_cm *scm = it2sns(it);
	struct m0_cm     *cm;

	M0_PRE(it != NULL);

	cm = &scm->sc_base;
	m0_sm_init(&it->si_sm, &cm_iter_sm_conf, ITPH_INIT, &cm->cm_sm_group);
	m0_sns_cm_iter_bob_init(it);
	it->si_total_files = 0;
	if (it->si_fom == NULL)
		it->si_fom = &scm->sc_base.cm_cp_pump.p_fom;

	return M0_RC(0);
}

M0_INTERNAL int m0_sns_cm_iter_start(struct m0_sns_cm_iter *it)
{
	struct m0_fid     gfid;
	struct m0_fid     gfid_start;
	struct m0_sns_cm *scm = it2sns(it);
	struct m0_cm     *cm = &scm->sc_base;
	int               rc;

	M0_PRE(it != NULL);
	M0_PRE(M0_IN(iter_phase(it), (ITPH_INIT, ITPH_IDLE)));

	agid2fid(&cm->cm_last_processed_out, &gfid_start);
	m0_fid_gob_make(&gfid, gfid_start.f_container, gfid_start.f_key);
	rc = m0_cob_ns_iter_init(&it->si_cns_it, &gfid, scm->sc_cob_dom);
	if (iter_phase(it) == ITPH_INIT)
		iter_phase_set(it, ITPH_IDLE);

	return M0_RC(rc);
}

M0_INTERNAL void m0_sns_cm_iter_stop(struct m0_sns_cm_iter *it)
{
	M0_PRE(M0_IN(iter_phase(it), (ITPH_INIT, ITPH_IDLE, ITPH_FID_NEXT,
				      ITPH_GROUP_NEXT, ITPH_FID_ATTR_LAYOUT)));

	if (!M0_IN(iter_phase(it), (ITPH_INIT, ITPH_IDLE)))
		iter_phase_set(it, ITPH_IDLE);
	if (iter_phase(it) == ITPH_IDLE) {
		if (it->si_cns_it.cni_cdom != NULL)
			m0_cob_ns_iter_fini(&it->si_cns_it);
		M0_SET0(&it->si_fc);
	}
}

M0_INTERNAL void m0_sns_cm_iter_fini(struct m0_sns_cm_iter *it)
{
	M0_PRE(it != NULL);
	M0_PRE(M0_IN(iter_phase(it), (ITPH_INIT, ITPH_IDLE)));

	iter_phase_set(it, ITPH_FINI);
	m0_sm_fini(&it->si_sm);
	m0_sns_cm_iter_bob_fini(it);
}

/** @} SNSCM */
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
