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
 * Original creation date: 07/03/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"
#include "lib/misc.h"

#include "sns/parity_repair.h"
#include "sns/cm/cm_utils.h"
#include "sns/cm/file.h"
#include "sns/cm/ag.h"
#include "cm/proxy.h"
#include "pool/pool_machine.h"

M0_INTERNAL int m0_sns_cm_repair_ag_setup(struct m0_sns_cm_ag *sag,
					  struct m0_pdclust_layout *pl);

static int repair_ag_in_cp_units(const struct m0_sns_cm *scm,
				 const struct m0_cm_ag_id *id,
				 struct m0_sns_cm_file_ctx *fctx,
				 uint32_t *in_cp_nr,
				 uint32_t *in_units_nr,
				 struct m0_cm_proxy_in_count *pcount)
{
	const struct m0_cm         *cm = &scm->sc_base;
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_fid               cobfid;
	struct m0_fid               spare_cob;
	struct m0_fid               gfid;
	struct m0_poolmach         *pm;
	struct m0_cm_proxy         *pxy;
	struct m0_conf_obj         *svc;
	struct m0_bitmap            proxy_map;
	const char                 *ep;
	struct m0_pdclust_layout   *pl;
	bool                        is_failed;
	uint32_t                    incps = 0;
	uint32_t                    inunits = 0;
	uint32_t                    local_spares = 0;
	uint32_t                    tgt_unit_prev;
	uint64_t                    unit;
	int                         rc = 0;
	int                         i;

	M0_ENTRY();
	M0_PRE(scm != NULL && id != NULL && fctx != NULL);
	M0_PRE(in_cp_nr != NULL && in_units_nr != NULL);

	agid2fid(id, &fctx->sf_fid);
	sa.sa_group = agid2group(id);
	pm = fctx->sf_pm;
	pl = m0_layout_to_pdl(fctx->sf_layout);
	rc = m0_bitmap_init(&proxy_map, scm->sc_base.cm_proxy_nr);
	if (rc != 0)
		return M0_ERR(rc);
	for (unit = 0; unit < m0_sns_cm_ag_size(pl); ++unit) {
		sa.sa_unit = unit;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (m0_sns_cm_unit_is_spare(fctx, sa.sa_group, unit))
			continue;

		is_failed = scm->sc_helpers->sch_is_cob_failed(pm, ta.ta_obj);
		/* Count number of spares corresponding to the failures
		 * on a node. This is required to calculate exact number of
		 * incoming copy packets.
		 */
		if (is_failed && !m0_sns_cm_is_cob_repaired(pm, ta.ta_obj)) {
			m0_sns_cm_fctx_lock(fctx);
			rc = m0_sns_repair_spare_map(pm, &gfid, pl, fctx->sf_pi,
						     sa.sa_group, unit,
						     (unsigned *)&sa.sa_unit,
						     &tgt_unit_prev);
			m0_sns_cm_fctx_unlock(fctx);
			if (rc != 0) {
				m0_bitmap_fini(&proxy_map);
				return M0_ERR(rc);
			}

			m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &spare_cob);
			if (m0_sns_cm_unit_is_spare(fctx, sa.sa_group,
						    sa.sa_unit) &&
			    m0_sns_cm_is_local_cob(cm, pm->pm_pver,
						   &spare_cob)) {
				M0_CNT_INC(local_spares);
			}
		}
		if (!is_failed &&
		    !m0_sns_cm_is_local_cob(cm, pm->pm_pver, &cobfid)) {
			ep = m0_sns_cm_tgt_ep(cm, pm->pm_pver, &cobfid, &svc);
			pxy = m0_tl_find(proxy, pxy, &cm->cm_proxies,
					 m0_streq(ep, pxy->px_endpoint));
			m0_confc_close(svc);
			if (M0_IN(pxy->px_status, (M0_PX_STOP, M0_PX_FAILED))) {
				rc = pxy->px_status == M0_PX_STOP ? -ESHUTDOWN :
								     -EHOSTDOWN;
				m0_bitmap_fini(&proxy_map);
				return M0_ERR_INFO(rc, " %s", pxy->px_endpoint);
			}

			if (!m0_bitmap_get(&proxy_map, pxy->px_id)) {
				m0_bitmap_set(&proxy_map, pxy->px_id, true);
				M0_CNT_INC(incps);
			}
			M0_CNT_INC(inunits);
		}
	}

	/* set incoming copy packets count per proxy based on local spares */
	if (pcount != NULL) {
		for (i = 0; i < pcount->p_nr; ++i) {
			if (m0_bitmap_get(&proxy_map, i))
				pcount->p_count[i] = local_spares;
		}
	}
	*in_cp_nr = incps * local_spares;
	*in_units_nr = inunits;
	m0_bitmap_fini(&proxy_map);

	M0_LEAVE();
	return M0_RC(rc);
}

static uint64_t repair_ag_unit_start(const struct m0_pdclust_layout *pl)
{
	return 0;
}

static uint64_t repair_ag_unit_end(const struct m0_pdclust_layout *pl)
{
	return m0_sns_cm_ag_size(pl);
}

static bool repair_ag_is_relevant(struct m0_sns_cm *scm,
				  struct m0_sns_cm_file_ctx *fctx,
				  uint64_t group)
{
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_fid               cobfid;
	uint64_t                    data_unit;
	uint32_t                    N;
	uint32_t                    K;
	uint32_t                    j;
	uint32_t                    spare;
	struct m0_pdclust_layout   *pl;
	struct m0_poolmach         *pm;
	int                         rc;

	M0_ENTRY();
	M0_PRE(fctx != NULL);

	pm = fctx->sf_pm;
	pl = m0_layout_to_pdl(fctx->sf_layout);
	N = m0_sns_cm_ag_nr_data_units(pl);
	K = m0_sns_cm_ag_nr_parity_units(pl);
	sa.sa_group = group;
	for (j = N + K; j < N + 2 * K; ++j) {
		sa.sa_unit = j;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (scm->sc_helpers->sch_is_cob_failed(pm, ta.ta_obj))
			continue;
		if (!m0_sns_cm_is_local_cob(&scm->sc_base, pm->pm_pver, &cobfid))
			continue;
		spare = j;
		do {
			m0_sns_cm_fctx_lock(fctx);
			rc = m0_sns_repair_data_map(pm, pl, fctx->sf_pi, group,
						    spare, &data_unit);
			m0_sns_cm_fctx_unlock(fctx);
			if (rc != 0)
				break;
			spare = data_unit;
		} while (m0_sns_cm_unit_is_spare(fctx, group, data_unit));

		if (rc != 0)
			continue;
		sa.sa_unit = data_unit;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (m0_sns_cm_is_cob_repairing(pm, ta.ta_obj))
			return true;
	}

	return M0_RC(false);
}

static int
repair_cob_locate(struct m0_sns_cm *scm, struct m0_cob_domain *cdom,
                  struct m0_poolmach *pm, const struct m0_fid *cob_fid)
{
	int rc;

	rc = m0_sns_cm_cob_locate(cdom, cob_fid);
	if (rc == -ENOENT) {
		if (m0_sns_cm_is_local_cob(&scm->sc_base, pm->pm_pver, cob_fid))
			rc = -ENODEV;
	}

	return rc;
}

static bool repair_is_cob_failed(struct m0_poolmach *pm,
			         uint32_t cob_index)
{
	enum m0_pool_nd_state state_out = 0;
	M0_PRE(pm != NULL);

	m0_poolmach_device_state(pm, cob_index, &state_out);
	/* Checking for M0_PNDS_SNS_REBALANCING for the case where the
 	   repaired device fails during rebalance, and a new repair is
	   triggered. The cob is not marked as failed, since it was already
	   repaired */
	return !M0_IN(state_out, (M0_PNDS_ONLINE, M0_PNDS_OFFLINE,
		      M0_PNDS_SNS_REBALANCING));
}

const struct m0_sns_cm_helpers repair_helpers = {
	.sch_ag_in_cp_units  = repair_ag_in_cp_units,
	.sch_ag_unit_start   = repair_ag_unit_start,
	.sch_ag_unit_end     = repair_ag_unit_end,
	.sch_ag_is_relevant  = repair_ag_is_relevant,
	.sch_ag_setup        = m0_sns_cm_repair_ag_setup,
	.sch_cob_locate      = repair_cob_locate,
	.sch_is_cob_failed   = repair_is_cob_failed
};

#undef M0_TRACE_SUBSYSTEM

/** @} endgroup SNSCM */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
