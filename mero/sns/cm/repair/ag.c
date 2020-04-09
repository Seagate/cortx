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
#include "lib/arith.h"

#include "fid/fid.h"
#include "sns/parity_repair.h"

#include "cm/proxy.h"

#include "sns/cm/cm_utils.h"
#include "sns/cm/repair/ag.h"
#include "sns/cm/cp.h"
#include "sns/cm/file.h"
#include "sns/cm/cm.h"
#include "ioservice/fid_convert.h" /* m0_fid_cob_device_id */

/**
   @addtogroup SNSCMAG

   @{
 */

M0_INTERNAL void m0_sns_cm_acc_cp_init(struct m0_sns_cm_cp *scp,
				       struct m0_sns_cm_ag *ag);

M0_INTERNAL int m0_sns_cm_acc_cp_setup(struct m0_sns_cm_cp *scp,
				       struct m0_fid *tgt_cobfid,
				       uint64_t tgt_cob_index,
				       uint64_t failed_unit_idx,
				       uint64_t data_seg_nr);

M0_INTERNAL int repair_cp_bufvec_split(struct m0_cm_cp *cp);

M0_INTERNAL struct m0_sns_cm_repair_ag *
sag2repairag(const struct m0_sns_cm_ag *sag)
{
	return container_of(sag, struct m0_sns_cm_repair_ag, rag_base);
}

M0_INTERNAL int64_t m0_sns_cm_repair_ag_inbufs(struct m0_sns_cm *scm,
					       struct m0_sns_cm_file_ctx *fctx,
					       const struct m0_cm_ag_id *id)
{
	uint64_t                  nr_cp_bufs;
	uint64_t                  cp_data_seg_nr;
	uint64_t                  nr_acc_bufs;
	int64_t                   nr_in_bufs;
	struct m0_pdclust_layout *pl = m0_layout_to_pdl(fctx->sf_layout);

	cp_data_seg_nr = m0_sns_cm_data_seg_nr(scm, pl);
	/*
	 * Calculate number of buffers required for a copy packet.
	 * This depends on the unit size and the max buffer size.
	 */
	nr_cp_bufs = m0_sns_cm_cp_buf_nr(&scm->sc_ibp.sb_bp, cp_data_seg_nr);
	/* Calculate number of buffers required for incoming copy packets. */
	nr_in_bufs = m0_sns_cm_incoming_reserve_bufs(scm, id);
	if (nr_in_bufs < 0)
		return nr_in_bufs;
	/* Calculate number of buffers required for accumulator copy packets. */
	nr_acc_bufs = nr_cp_bufs * m0_sns_cm_ag_unrepaired_units(scm, fctx,
							id->ai_lo.u_lo, NULL);
	return nr_in_bufs + nr_acc_bufs;
}

static int incr_recover_failure_register(struct m0_sns_cm_repair_ag *rag)
{
	struct m0_cm_cp      *cp;
	struct m0_net_buffer *nbuf_head;
	int                   rc = 0;
	int                   i;

	M0_PRE(rag != NULL);

	for (i = 0; i < rag->rag_base.sag_fnr; ++i) {
		if (!rag->rag_fc[i].fc_is_inuse)
			continue;
		cp = &rag->rag_fc[i].fc_tgt_acc_cp.sc_base;
		m0_cm_cp_bufvec_merge(cp);
		nbuf_head = cp_data_buf_tlist_head(&cp->c_buffers);
		rc = m0_sns_ir_failure_register(&nbuf_head->nb_buffer,
						rag->rag_fc[i].fc_failed_idx,
						&rag->rag_ir);
		if (rc != 0)
			break;
	}

	return M0_RC(rc);
}

static int incr_recover_init(struct m0_sns_cm_repair_ag *rag,
			     struct m0_pdclust_layout *pl)
{
	uint64_t local_cp_nr;
	int      rc;

	M0_PRE(rag != NULL);
	M0_PRE(pl != NULL);

	/*
	 * No need of parity math for N = 1 configuration.
	 */
	if (m0_pdclust_is_replicated(pl))
		return 0;

	M0_SET0(&rag->rag_math);
	M0_SET0(&rag->rag_ir);

	rc = m0_parity_math_init(&rag->rag_math, m0_sns_cm_ag_nr_data_units(pl),
				 m0_sns_cm_ag_nr_parity_units(pl));
	if (rc != 0)
		return M0_RC(rc);

	if (m0_sns_cm_ag_nr_parity_units(pl) == 1)
		return 0;

	local_cp_nr = rag->rag_base.sag_base.cag_cp_local_nr;
	rc = m0_sns_ir_init(&rag->rag_math, local_cp_nr, &rag->rag_ir);
	if (rc != 0) {
		m0_parity_math_fini(&rag->rag_math);
		return M0_RC(rc);
	}

	rc = incr_recover_failure_register(rag);
	if (rc != 0)
		goto err;

	rc = m0_sns_ir_mat_compute(&rag->rag_ir);
	if (rc != 0)
		goto err;

	return M0_RC(rc);
err:
	m0_sns_ir_fini(&rag->rag_ir);
	m0_parity_math_fini(&rag->rag_math);
	return M0_RC(rc);
}

static void incr_recover_fini(struct m0_sns_cm_repair_ag *rag)
{
	m0_sns_ir_fini(&rag->rag_ir);
	m0_parity_math_fini(&rag->rag_math);
}

/**
 * Calculates remaining of the unused reserved buffers for a given
 * @rag aggregation group.
 */
static uint32_t ag_in_remaining_bufs(struct m0_sns_cm_repair_ag *rag)
{
	struct m0_sns_cm_ag       *sag = &rag->rag_base;
	struct m0_cm_aggr_group   *ag = &sag->sag_base;
	struct m0_sns_cm          *scm = cm2sns(ag->cag_cm);
	struct m0_pdclust_layout  *pl = m0_layout_to_pdl(sag->sag_fctx->sf_layout);
	uint32_t                   nr_cp_bufs;
	uint32_t                   nr_incoming_freed;

	nr_cp_bufs = m0_sns_cm_cp_buf_nr(&scm->sc_ibp.sb_bp,
					 m0_sns_cm_data_seg_nr(scm, pl));
	nr_incoming_freed = ag->cag_freed_cp_nr -
				(sag->sag_cp_created_nr + rag->rag_acc_freed);
	return (sag->sag_incoming_cp_nr - nr_incoming_freed) * nr_cp_bufs;
}

static void acc_check_fini(struct m0_sns_cm_repair_ag *rag)
{
	struct m0_cm_cp           *cp;
	struct m0_sns_cm_ag       *sag = &rag->rag_base;
	struct m0_cm_aggr_group   *ag = &sag->sag_base;
	struct m0_sns_cm          *scm = cm2sns(ag->cag_cm);
	struct m0_pdclust_layout  *pl = m0_layout_to_pdl(sag->sag_fctx->sf_layout);
	uint32_t                   nr_cp_bufs;
	uint32_t                   unused_cps = 0;
	uint32_t                   nr_bufs;
	uint32_t                   nr_rem_bufs;
	int                        i;

	M0_PRE(rag != NULL);

	nr_cp_bufs = m0_sns_cm_cp_buf_nr(&scm->sc_ibp.sb_bp,
					 m0_sns_cm_data_seg_nr(scm, pl));
	for (i = 0; i < sag->sag_fnr; ++i) {
		if (!rag->rag_fc[i].fc_is_inuse) {
			M0_CNT_INC(unused_cps);
			continue;
		}
		cp = &rag->rag_fc[i].fc_tgt_acc_cp.sc_base;
		if (cp->c_buf_nr > 0) {
			repair_cp_bufvec_split(cp);
			cp->c_ops->co_free(cp);
		}
	}
	/* While reserving incoming buffers, we consider all the
	 * accumulators, in-order to avoid buffer leakage, normalize
	 * the reservation if any of the accumulators is not in use.
	 */
	if (ag->cag_has_incoming) {
		nr_rem_bufs = ag_in_remaining_bufs(rag);
		nr_bufs = unused_cps * nr_cp_bufs + nr_rem_bufs;
		m0_sns_cm_cancel_reservation(scm, nr_bufs);
	}
}

static void repair_ag_fini(struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_ag        *sag;
	struct m0_sns_cm_repair_ag *rag;

	M0_ENTRY();
	M0_PRE(ag != NULL);

	sag = ag2snsag(ag);
	rag = sag2repairag(sag);
	M0_ASSERT(ag->cag_fini_ast.sa_next == NULL);

	/* In-case the aggregation group is being forcefully finalised
	 * (e.g. quiesce or abort), we need to release accumulator copy packet
	 * buffers.
	 */
	acc_check_fini(rag);
	incr_recover_fini(rag);
	m0_sns_cm_ag_fini(sag);
	m0_free(rag->rag_fc);
	m0_free(rag);

	M0_LEAVE();
}

static uint32_t repair_ag_inactive_acc_nr(struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_ag        *sag = ag2snsag(ag);
	struct m0_sns_cm_repair_ag *rag = sag2repairag(sag);
	uint32_t                    inactive_acc_nr = 0;
	int                         i;

	for (i = 0; i < sag->sag_fnr; ++i) {
		if (rag->rag_fc[i].fc_is_inuse && !rag->rag_fc[i].fc_is_active)
			M0_CNT_INC(inactive_acc_nr);
	}

	if (inactive_acc_nr >= rag->rag_acc_freed)
		return inactive_acc_nr - rag->rag_acc_freed;
	else
		return rag->rag_acc_freed - inactive_acc_nr;
}

static bool repair_ag_can_fini(const struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_ag        *sag = ag2snsag(ag);
	struct m0_sns_cm_repair_ag *rag = sag2repairag(sag);
	uint32_t                    inactive_accs = 0;

	if (ag->cag_is_frozen || ag->cag_rc != 0) {
		inactive_accs = repair_ag_inactive_acc_nr(&sag->sag_base);
		return ag->cag_ref == inactive_accs;
	}

	return (rag->rag_acc_inuse_nr == rag->rag_acc_freed) &&
	       (ag->cag_transformed_cp_nr ==
		(ag->cag_freed_cp_nr - rag->rag_acc_freed));
}

static const struct m0_cm_aggr_group_ops sns_cm_repair_ag_ops = {
	.cago_ag_can_fini       = repair_ag_can_fini,
	.cago_fini              = repair_ag_fini,
	.cago_local_cp_nr       = m0_sns_cm_ag_local_cp_nr,
	.cago_has_incoming_from = m0_sns_cm_ag_has_incoming_from,
	.cago_is_frozen_on      = m0_sns_cm_ag_is_frozen_on
};

static uint64_t repair_ag_target_unit(struct m0_sns_cm_ag *sag,
				      struct m0_pdclust_layout *pl,
				      struct m0_pdclust_instance *pi,
				      uint64_t fdev, uint64_t funit)
{
        struct m0_poolmach *pm;
        struct m0_fid       gfid;
        uint64_t            group;
        uint32_t            tgt_unit;
        uint32_t            tgt_unit_prev;
        int                 rc;

	pm = sag->sag_fctx->sf_pm;

        agid2fid(&sag->sag_base.cag_id, &gfid);
        group = agid2group(&sag->sag_base.cag_id);
	m0_sns_cm_fctx_lock(sag->sag_fctx);
        rc = m0_sns_repair_spare_map(pm, &gfid, pl, pi,
			group, funit, &tgt_unit, &tgt_unit_prev);
	m0_sns_cm_fctx_unlock(sag->sag_fctx);
        if (rc != 0)
                tgt_unit = ~0;

        return tgt_unit;
}

static int repair_ag_failure_ctxs_setup(struct m0_sns_cm_repair_ag *rag,
					const struct m0_bitmap *fmap,
					struct m0_pdclust_layout *pl)
{
	struct m0_sns_cm_ag                    *sag = &rag->rag_base;
	struct m0_sns_cm_file_ctx              *fctx;
	struct m0_cm                           *cm = snsag2cm(sag);
	struct m0_sns_cm_repair_ag_failure_ctx *rag_fc;
	struct m0_fid                           fid;
	struct m0_fid                           cobfid;
	struct m0_fid                          *tgt_cobfid;
	struct m0_pdclust_instance             *pi = sag->sag_fctx->sf_pi;
	bool                                    local;
	enum m0_pool_nd_state                   state_out;
	uint64_t                                tgt_unit;
	uint64_t                                fidx = 0;
	uint64_t                                group;
	uint64_t                                data_unit_id_out;
	int                                     i;
	int                                     rc = 0;
	struct m0_poolmach                     *pm;
	uint32_t                                index;

	M0_PRE(fmap != NULL);
	M0_PRE(fmap->b_nr == m0_sns_cm_ag_size(pl));

	fctx = sag->sag_fctx;
	pm = fctx->sf_pm;
	agid2fid(&sag->sag_base.cag_id, &fid);
	group = agid2group(&sag->sag_base.cag_id);
	for (i = 0; i < fmap->b_nr; ++i) {
		if (!m0_bitmap_get(fmap, i))
			continue;
		if (m0_sns_cm_unit_is_spare(fctx, group, i))
			continue;
		/*
		 * Check if the failed index is spare, which means that
		 * this is failure after repair has been done atleast
		 * once. This also means that rebalance has not been
		 * invoked yet.
		 */
		if (m0_pdclust_unit_classify(pl, i) == M0_PUT_SPARE) {
			m0_sns_cm_fctx_lock(fctx);
			rc = m0_sns_repair_data_map(pm, pl, pi, group,
						    i, &data_unit_id_out);
			m0_sns_cm_fctx_unlock(fctx);
				if (rc != 0)
					return M0_RC(rc);
			/*
			 * If the mapped data unit @data_unit_id_out
			 * corresponding to spare unit @i is same as the spare
			 * unit @i then ignore and nove to next failed unit in
			 * the parity group.
			 */
			if (data_unit_id_out == i)
				continue;
		} else
			data_unit_id_out = i;

		/*
		 * Move to next failed unit in the parity group if the mapped
		 * data unit @data_unit_id_out is another spare unit in the
		 * aggregation group (i.e N + K < i < N + 2K).
		 */
		if (m0_sns_cm_unit_is_spare(fctx, group,
					    data_unit_id_out))
			continue;
		rc = m0_sns_cm_ag_tgt_unit2cob(sag, data_unit_id_out,
					       &cobfid);
		if (rc != 0)
			return M0_RC(rc);
		index = m0_sns_cm_device_index_get(group, data_unit_id_out, fctx);
		/*
		 * Failed unit is data/parity unit.
		 * Check the device state for the device hosting the failed unit.
		 * If the device state == M0_PNDS_SNS_REPAIRED, means failed
		 * unit @i was already repaired in previous sns repair operation.
		 * Hence move to next failed unit in the aggregation group.
		 */
		if (data_unit_id_out == i) {
			rc = m0_poolmach_device_state(pm, index, &state_out);
			if (rc != 0)
				return M0_RC(rc);
			if (state_out == M0_PNDS_SNS_REPAIRED)
				continue;
		}

		tgt_unit = repair_ag_target_unit(sag, pl, pi, index,
						 data_unit_id_out);
		rag_fc = &rag->rag_fc[fidx];
		tgt_cobfid = &rag_fc->fc_tgt_cobfid;
		rc = m0_sns_cm_ag_tgt_unit2cob(sag, tgt_unit, tgt_cobfid);
		if (rc != 0)
			return M0_RC(rc);
		/*
		 * Number of accumulators allocated == number of failures in an
		 * aggregation group.
		 * So, if the target cob (cob hosting the spare unit) for the
		 * given accumulator is not local and there are no local units
		 * for the given aggregation group then this accumulator is not
		 * used, thus not freed, this holds the aggregation group
		 * finalisation. Thus mark the accumulator if it is used.
		 * (struct m0_sns_cm_repair_ag_failure_ctx::fc_is_inuse) and
		 * also account the number of used accumulators in an
		 * aggregation group,
		 * (struct m0_sns_cm_repair_ag::rag_acc_inuse_nr). Use this
		 * information to finalise an aggregation group.
		 */
		local = m0_sns_cm_is_local_cob(cm, pm->pm_pver, tgt_cobfid);
		if (local || sag->sag_base.cag_cp_local_nr != 0) {
			rag_fc->fc_failed_idx = data_unit_id_out;
			rag_fc->fc_tgt_idx = tgt_unit;
			rag_fc->fc_tgt_cob_index =
				m0_sns_cm_ag_unit2cobindex(sag, tgt_unit);
			M0_CNT_INC(rag->rag_acc_inuse_nr);
			rag_fc->fc_is_inuse = true;
			if (local)
				M0_CNT_INC(sag->sag_local_tgts_nr);
		}
		if (rag_fc->fc_is_inuse &&
		    !m0_sns_cm_is_local_cob(cm, pm->pm_pver,
					    &rag_fc->fc_tgt_cobfid))
			M0_CNT_INC(sag->sag_outgoing_nr);
		++fidx;
	}

	M0_POST(fidx <= sag->sag_fnr);
	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_repair_ag_alloc(struct m0_cm *cm,
					  const struct m0_cm_ag_id *id,
					  bool has_incoming,
					  struct m0_cm_aggr_group **out)
{
	struct m0_sns_cm_repair_ag             *rag;
	struct m0_sns_cm_repair_ag_failure_ctx *rag_fc;
	struct m0_sns_cm_ag                    *sag = NULL;
	struct m0_pdclust_layout               *pl = NULL;
	uint64_t                                f_nr;
	int                                     i;
	int                                     rc = 0;

	M0_ENTRY("scm: %p, ag id:%p", cm, id);
	M0_PRE(cm != NULL && id != NULL && out != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	/* Allocate new aggregation group. */
	M0_ALLOC_PTR(rag);
	if (rag == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_sns_cm_ag_init(&rag->rag_base, cm, id, &sns_cm_repair_ag_ops,
			       has_incoming);
	if (rc != 0) {
		m0_free(rag);
		return M0_RC(rc);
	}
	f_nr = rag->rag_base.sag_fnr;
	M0_ALLOC_ARR(rag->rag_fc, f_nr);
	if (rag->rag_fc == NULL) {
		rc =  M0_ERR(-ENOMEM);
		goto cleanup_ag;
	}
	sag = &rag->rag_base;
	pl = m0_layout_to_pdl(sag->sag_fctx->sf_layout);
	/* Set the target cob fid of accumulators for this aggregation group. */
	rc = repair_ag_failure_ctxs_setup(rag, &sag->sag_fmap, pl);
	if (rc != 0)
		goto cleanup_ag;

	/* Initialise the accumulators. */
	for (i = 0; i < sag->sag_fnr; ++i) {
		rag_fc = &rag->rag_fc[i];
		if (rag_fc->fc_is_inuse)
			m0_sns_cm_acc_cp_init(&rag_fc->fc_tgt_acc_cp, sag);
	}

	/* Acquire buffers and setup the accumulators. */
	rc = m0_sns_cm_repair_ag_setup(sag, pl);
	if (rc != 0 && rc != -ENOBUFS)
		goto cleanup_acc;
	*out = &sag->sag_base;
	goto done;

cleanup_acc:
	for (i = 0; i < sag->sag_fnr; ++i) {
		rag_fc = &rag->rag_fc[i];
		if (rag_fc->fc_is_inuse)
			m0_sns_cm_cp_buf_release(&rag_fc->fc_tgt_acc_cp.sc_base);
	}
cleanup_ag:
	M0_LOG(M0_ERROR, "cleaning up group rc: %d", rc);
	m0_cm_aggr_group_fini(&sag->sag_base);
	m0_bitmap_fini(&sag->sag_fmap);
	m0_free(rag->rag_fc);
	m0_free(rag);
done:
	M0_LEAVE("ag: %p", &sag->sag_base);
	M0_ASSERT(rc <= 0);
	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_repair_ag_setup(struct m0_sns_cm_ag *sag,
					  struct m0_pdclust_layout *pl)
{
	struct m0_sns_cm                       *scm;
	struct m0_sns_cm_repair_ag             *rag = sag2repairag(sag);
	struct m0_sns_cm_repair_ag_failure_ctx *rag_fc;
	struct m0_fid                           gfid;
	uint64_t                                cp_data_seg_nr;
	int                                     i;
	int                                     rc = 0;

	M0_PRE(sag != NULL);

	scm = cm2sns(sag->sag_base.cag_cm);
	cp_data_seg_nr = m0_sns_cm_data_seg_nr(scm, pl);
	for (i = 0; i < sag->sag_fnr; ++i) {
		rag_fc = &rag->rag_fc[i];
		if (rag_fc->fc_is_inuse) {
			rc = m0_sns_cm_acc_cp_setup(&rag_fc->fc_tgt_acc_cp,
						    &rag_fc->fc_tgt_cobfid,
						    rag_fc->fc_tgt_cob_index,
						    rag_fc->fc_failed_idx,
						    cp_data_seg_nr);
		}
		if (rc != 0)
			return M0_RC(rc);
	}

	agid2fid(&sag->sag_base.cag_id, &gfid);
	return incr_recover_init(rag, pl);
}

/**
 * Returns true if all the necessary copy packets are transformed or
 * accumulated into the aggregation group accumulator for a given
 * copy machine operation, viz. sns-repair or sns-rebalance.
 */
M0_INTERNAL bool m0_sns_cm_ag_acc_is_full_with(const struct m0_cm_cp *acc,
					       uint64_t nr_cps)
{
	int      i;
	uint64_t xform_cp_nr = 0;

	for (i = 0; i < acc->c_xform_cp_indices.b_nr; ++i) {
		if (m0_bitmap_get(&acc->c_xform_cp_indices, i))
			M0_CNT_INC(xform_cp_nr);
	}

	return xform_cp_nr >= nr_cps;
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
