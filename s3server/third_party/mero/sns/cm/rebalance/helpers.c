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

#include "fid/fid.h"

#include "cm/proxy.h"

#include "sns/parity_repair.h"
#include "sns/cm/cm_utils.h"
#include "sns/cm/ag.h"
#include "sns/cm/file.h"
#include "pool/pool.h"

M0_INTERNAL int m0_sns_cm_rebalance_ag_setup(struct m0_sns_cm_ag *sag,
					     struct m0_pdclust_layout *pl);

static bool is_spare_relevant(const struct m0_sns_cm *scm,
			      struct m0_sns_cm_file_ctx *fctx,
			      uint64_t group, uint32_t spare,
			      uint32_t *incoming_unit);

static int cob_to_proxy(struct m0_fid *cobfid, const struct m0_cm *cm,
			struct m0_poolmach *pm, struct m0_cm_proxy **pxy)
{
	struct m0_conf_obj *s;
	struct m0_cm_proxy *p;
	const char         *ep;
	int                 rc;

	M0_PRE(m0_fid_is_set(cobfid));
	M0_PRE(cm != NULL && pm != NULL);

	ep = m0_sns_cm_tgt_ep(cm, pm->pm_pver, cobfid, &s);
	p = m0_tl_find(proxy, p, &cm->cm_proxies,
		       m0_streq(ep, p->px_endpoint));
	m0_confc_close(s);
	if (M0_IN(p->px_status, (M0_PX_STOP, M0_PX_FAILED))) {
		rc = p->px_status == M0_PX_STOP ? -ESHUTDOWN : -EHOSTDOWN;
		return M0_ERR(rc);
	}
	*pxy = p;

	return 0;
}

static int rebalance_ag_in_cp_units(const struct m0_sns_cm *scm,
				    const struct m0_cm_ag_id *id,
				    struct m0_sns_cm_file_ctx *fctx,
				    uint32_t *in_cp_nr,
				    uint32_t *in_units_nr,
				    struct m0_cm_proxy_in_count *pcount)
{
	struct m0_fid		    cobfid;
	struct m0_fid		    gfid;
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_cm_proxy         *pxy;
	const struct m0_cm         *cm;
	struct m0_pdclust_layout   *pl;
	struct m0_poolmach         *pm;
	uint32_t                    incps = 0;
	uint32_t                    tgt_unit;
	uint32_t                    tgt_unit_prev;
	uint64_t                    unit;
	uint64_t                    upg;
	int                         rc = 0;

	M0_ENTRY();

	M0_SET0(&sa);
	cm = &scm->sc_base;
	agid2fid(id, &gfid);
	sa.sa_group = agid2group(id);
	pl = m0_layout_to_pdl(fctx->sf_layout);
	pm = fctx->sf_pm;
	upg = m0_sns_cm_ag_size(pl);
	for (unit = 0; unit < upg; ++unit) {
		sa.sa_unit = unit;
		M0_SET0(&ta);
		M0_SET0(&cobfid);
		M0_ASSERT(pm != NULL);
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (!scm->sc_helpers->sch_is_cob_failed(pm, ta.ta_obj))
			continue;
                if (!m0_sns_cm_is_local_cob(cm, pm->pm_pver, &cobfid))
                        continue;
		if (m0_pdclust_unit_classify(pl, unit) == M0_PUT_SPARE) {
			if (is_spare_relevant(scm, fctx, sa.sa_group, unit,
					      &tgt_unit)) {
				if (pcount != NULL) {
					M0_SET0(&ta);
					M0_SET0(&cobfid);
					sa.sa_unit = tgt_unit;
					m0_sns_cm_unit2cobfid(fctx, &sa, &ta,
							      &cobfid);
					rc = cob_to_proxy(&cobfid, cm, pm, &pxy);
					if (rc != 0)
						return M0_ERR(rc);
					M0_CNT_INC(pcount->p_count[pxy->px_id]);
				}
				M0_CNT_INC(incps);
			}
			continue;
		}
		m0_sns_cm_fctx_lock(fctx);
		rc = m0_sns_repair_spare_map(pm, &gfid, pl, fctx->sf_pi,
					     sa.sa_group, unit, &tgt_unit,
					     &tgt_unit_prev);
		m0_sns_cm_fctx_unlock(fctx);
                if (rc != 0)
                        return M0_ERR(rc);
		M0_SET0(&ta);
		M0_SET0(&cobfid);
                sa.sa_unit = tgt_unit;
                m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
                if (!m0_sns_cm_is_local_cob(cm, pm->pm_pver, &cobfid)) {
			if (pcount != NULL) {
				rc = cob_to_proxy(&cobfid, cm, pm, &pxy);
				if (rc != 0)
					return M0_ERR(rc);
				M0_CNT_INC(pcount->p_count[pxy->px_id]);
			}
			M0_CNT_INC(incps);
		}
	}

	*in_cp_nr = *in_units_nr = incps;

	M0_LEAVE("incps=%u", (unsigned)incps);
        return M0_RC(rc);
}

static uint64_t rebalance_ag_unit_start(const struct m0_pdclust_layout *pl)
{
	return m0_sns_cm_ag_spare_unit_nr(pl, 0);
}

static uint64_t rebalance_ag_unit_end(const struct m0_pdclust_layout *pl)
{
	return m0_sns_cm_ag_size(pl);
}

static int _tgt_check_and_change(struct m0_sns_cm_ag *sag, struct m0_poolmach *pm,
				 struct m0_pdclust_layout *pl,
				 uint64_t data_unit, uint32_t old_tgt_dev,
				 uint64_t *new_tgt_unit)
{
	uint64_t                   group = agid2group(&sag->sag_base.cag_id);
	uint32_t                   spare;
	uint32_t                   spare_prev;
	struct m0_sns_cm_file_ctx *fctx;
	int                        rc;

	fctx = sag->sag_fctx;
	m0_sns_cm_fctx_lock(fctx);
	rc = m0_sns_repair_spare_rebalancing(pm, &fctx->sf_fid, pl, fctx->sf_pi,
				     group, data_unit, &spare,
				     &spare_prev);
	m0_sns_cm_fctx_unlock(fctx);
	if (rc == 0)
		*new_tgt_unit = spare;

	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_rebalance_tgt_info(struct m0_sns_cm_ag *sag,
					     struct m0_sns_cm_cp *scp)
{
	struct m0_pdclust_layout  *pl;
	struct m0_fid              cobfid;
	struct m0_sns_cm_file_ctx *fctx;
	struct m0_poolmach        *pm;
	uint64_t                   group = agid2group(&sag->sag_base.cag_id);
	uint64_t                   data_unit;
	uint64_t                   offset;
	uint32_t                   dev_idx;
	int                        rc = 0;

	fctx = sag->sag_fctx;
	pm = fctx->sf_pm;
	pl = m0_layout_to_pdl(fctx->sf_layout);
	data_unit = scp->sc_base.c_ag_cp_idx;
	dev_idx = m0_sns_cm_device_index_get(group, data_unit, fctx);
	if (!m0_sns_cm_is_cob_rebalancing(pm, dev_idx)) {
		rc = _tgt_check_and_change(sag, pm, pl, data_unit, dev_idx,
					   &data_unit);
		if (rc != 0) {
			M0_LOG(M0_DEBUG, "Target device %u not rebalancing,"
					 "no other rebalancing target found.",
					 (unsigned)dev_idx);
			return M0_RC(rc);
		}
		scp->sc_base.c_ag_cp_idx = data_unit;
	}

	rc = m0_sns_cm_ag_tgt_unit2cob(sag, scp->sc_base.c_ag_cp_idx, &cobfid);
	if (rc == 0) {
		offset = m0_sns_cm_ag_unit2cobindex(sag,
						    scp->sc_base.c_ag_cp_idx);
		/*
		 * Change the target cobfid and offset of the copy
		 * packet to write the data from spare unit back to
		 * previously failed data unit but now available data/
		 * parity unit in the aggregation group.
		 */
		m0_sns_cm_cp_tgt_info_fill(scp, &cobfid, offset,
					   scp->sc_base.c_ag_cp_idx);
	}

	return M0_RC(rc);
}

static bool is_spare_relevant(const struct m0_sns_cm *scm,
			      struct m0_sns_cm_file_ctx *fctx,
			      uint64_t group, uint32_t spare,
			      uint32_t *incoming_unit)
{
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_fid               cobfid;
	uint32_t                    dev_idx;
	uint64_t                    data;
	uint32_t                    spare_prev;
	struct m0_poolmach         *pm;
	struct m0_pdclust_layout   *pl;
	struct m0_pdclust_instance *pi;
	int                         rc;

	pl = m0_layout_to_pdl(fctx->sf_layout);
	pi = fctx->sf_pi;
	pm = fctx->sf_pm;
	m0_sns_cm_fctx_lock(fctx);
	rc = m0_sns_repair_data_map(pm, pl, pi, group, spare, &data);
	m0_sns_cm_fctx_unlock(fctx);
	if (rc == 0) {
		dev_idx = m0_sns_cm_device_index_get(group, data, fctx);
		if (m0_sns_cm_is_cob_repaired(pm, dev_idx)) {
			m0_sns_cm_fctx_lock(fctx);
			rc = m0_sns_repair_spare_map(pm, &fctx->sf_fid, pl, pi,
						     group, data, &spare,
						     &spare_prev);
			m0_sns_cm_fctx_unlock(fctx);
			if (rc == 0) {
				sa.sa_unit = spare;
				sa.sa_group = group;
				m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
				if (!m0_sns_cm_is_local_cob(&scm->sc_base,
							    pm->pm_pver,
							    &cobfid)) {
					*incoming_unit = spare;
					return true;
				}
			}
		}
	}
	return false;
}

static bool rebalance_ag_is_relevant(struct m0_sns_cm *scm,
				     struct m0_sns_cm_file_ctx *fctx,
				     uint64_t group)
{
	uint32_t                    spare;
	uint32_t                    spare_prev;
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_fid               cobfid;
	uint64_t                    upg;
	uint32_t                    i;
	uint32_t                    tunit;
	uint32_t                    in_unit;
	struct m0_poolmach         *pm;
	struct m0_pdclust_layout   *pl;
	int                         rc;

	M0_PRE(fctx != NULL);

	pl = m0_layout_to_pdl(fctx->sf_layout);
	upg = m0_sns_cm_ag_size(pl);
	sa.sa_group = group;
	pm = fctx->sf_pm;
	for (i = 0; i < upg; ++i) {
		sa.sa_unit = i;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (m0_sns_cm_is_cob_rebalancing(pm, ta.ta_obj) &&
		    m0_sns_cm_is_local_cob(&scm->sc_base, pm->pm_pver,
					   &cobfid)) {
			tunit = sa.sa_unit;
			if (m0_pdclust_unit_classify(pl, tunit) ==
					M0_PUT_SPARE) {
				if (!is_spare_relevant(scm, fctx, group, tunit,
						       &in_unit))
					continue;
			}
			m0_sns_cm_fctx_lock(fctx);
			rc = m0_sns_repair_spare_map(pm, &fctx->sf_fid,
					pl, fctx->sf_pi, group, tunit,
					&spare, &spare_prev);
			m0_sns_cm_fctx_unlock(fctx);
			if (rc != 0)
				return false;
			tunit = spare;
			sa.sa_unit = tunit;
			m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
			if (!m0_sns_cm_is_local_cob(&scm->sc_base, pm->pm_pver,
						&cobfid))
				return true;
		}
	}

	return false;
}

static int
rebalance_cob_locate(struct m0_sns_cm *scm, struct m0_cob_domain *cdom,
                     struct m0_poolmach *pm, const struct m0_fid *cob_fid)
{
	return m0_sns_cm_cob_locate(cdom, cob_fid);
}

static bool rebalance_is_cob_failed(struct m0_poolmach *pm,
				    uint32_t cob_index)
{
	enum m0_pool_nd_state state_out = 0;
	M0_PRE(pm != NULL);

	m0_poolmach_device_state(pm, cob_index, &state_out);
	return !M0_IN(state_out, (M0_PNDS_ONLINE, M0_PNDS_OFFLINE,
		      M0_PNDS_SNS_REPAIRING));
}

const struct m0_sns_cm_helpers rebalance_helpers = {
	.sch_ag_in_cp_units  = rebalance_ag_in_cp_units,
	.sch_ag_unit_start   = rebalance_ag_unit_start,
	.sch_ag_unit_end     = rebalance_ag_unit_end,
	.sch_ag_is_relevant  = rebalance_ag_is_relevant,
	.sch_ag_setup        = m0_sns_cm_rebalance_ag_setup,
	.sch_cob_locate      = rebalance_cob_locate,
	.sch_is_cob_failed   = rebalance_is_cob_failed
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
