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
 * Original creation date: 03/08/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/misc.h"
#include "lib/trace.h"
#include "lib/finject.h"
#include "lib/hash.h"

#include "cob/cob.h"
#include "mero/setup.h"
#include "ioservice/io_service.h"
#include "conf/helpers.h"          /* m0_conf_service_get */

#include "pool/pool.h"
#include "sns/parity_repair.h"
#include "sns/cm/ag.h"
#include "sns/cm/iter.h"
#include "sns/cm/cm_utils.h"
#include "sns/cm/file.h"
#include "ioservice/fid_convert.h" /* m0_fid_cob_device_id */
#include "rpc/rpc_machine.h"       /* m0_rpc_machine_ep */
#include "fd/fd.h"                 /* m0_fd_fwd_map */

/**
   @addtogroup SNSCM

   @{
 */

/* Start of UT specific code. */
enum {
	SNS_DEFAULT_FILE_SIZE = 1 << 17,
	SNS_DEFAULT_N = 3,
	SNS_DEFAULT_K = 1,
	SNS_DEFAULT_P = 5,
	SNS_DEFAULT_UNIT_SIZE = 4096,
	SNS_DEFAULT_LAYOUT_ID = 1
};

M0_INTERNAL int
m0_sns_cm_ut_file_size_layout(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_sns_cm             *scm    = fctx->sf_scm;
	struct m0_layout_linear_attr  lattr;
	struct m0_pdclust_attr        plattr;
	struct m0_pdclust_layout     *pl = 0;
	struct m0_layout_linear_enum *le;
	struct m0_pool               *p;
	struct m0_pool_version       *pver;
	struct m0_reqh               *reqh;
	uint64_t                      lid;
	int                           rc = 0;

	reqh = scm->sc_base.cm_service.rs_reqh;
	p = pools_tlist_head(&reqh->rh_pools->pc_pools);
	pver = pool_version_tlist_head(&p->po_vers);
	lid = m0_hash(m0_fid_hash(&pver->pv_id) + SNS_DEFAULT_LAYOUT_ID);
	fctx->sf_pm = &pver->pv_mach;
	fctx->sf_layout = m0_layout_find(&reqh->rh_ldom, lid);
	if (fctx->sf_layout != NULL)
		goto out;
	lattr.lla_nr = SNS_DEFAULT_P;
	lattr.lla_A  = 1;
	lattr.lla_B  = 1;
	rc = m0_linear_enum_build(&reqh->rh_ldom, &lattr, &le);
	if (rc == 0) {
		lid                 = SNS_DEFAULT_LAYOUT_ID;
		plattr.pa_N         = SNS_DEFAULT_N;
		plattr.pa_K         = SNS_DEFAULT_K;
		plattr.pa_P         = SNS_DEFAULT_P;
		plattr.pa_unit_size = SNS_DEFAULT_UNIT_SIZE;
		m0_uint128_init(&plattr.pa_seed, "upjumpandpumpim,");
		rc = m0_pdclust_build(&reqh->rh_ldom, lid, &plattr,
				&le->lle_base, &pl);
		if (rc != 0) {
			m0_layout_enum_fini(&le->lle_base);
			return M0_RC(rc);
		}
	}
	fctx->sf_layout = m0_pdl_to_layout(pl);
out:
	fctx->sf_attr.ca_size = SNS_DEFAULT_FILE_SIZE;

	return M0_RC(rc);
}

/* End of UT specific code. */

M0_INTERNAL void m0_sns_cm_unit2cobfid(struct m0_sns_cm_file_ctx *fctx,
				       const struct m0_pdclust_src_addr *sa,
				       struct m0_pdclust_tgt_addr *ta,
				       struct m0_fid *cfid_out)
{
	m0_sns_cm_file_fwd_map(fctx, sa, ta);
	m0_poolmach_gob2cob(fctx->sf_pm, &fctx->sf_fid, ta->ta_obj, cfid_out);
}

M0_INTERNAL uint32_t m0_sns_cm_device_index_get(uint64_t group,
						uint64_t unit_number,
						struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_pdclust_src_addr sa;
	struct m0_pdclust_tgt_addr ta;

	M0_SET0(&sa);
	M0_SET0(&ta);

	sa.sa_group = group;
	sa.sa_unit = unit_number;
	m0_sns_cm_file_fwd_map(fctx, &sa, &ta);
	M0_LEAVE("index:%d", (int)ta.ta_obj);
	return ta.ta_obj;
}

M0_INTERNAL uint64_t m0_sns_cm_ag_unit2cobindex(struct m0_sns_cm_ag *sag,
						uint64_t unit)
{
	struct m0_pdclust_src_addr sa;
	struct m0_pdclust_tgt_addr ta;
	struct m0_sns_cm_file_ctx *fctx = sag->sag_fctx;

	sa.sa_group = agid2group(&sag->sag_base.cag_id);
	sa.sa_unit  = unit;
	m0_sns_cm_file_fwd_map(fctx, &sa, &ta);
	return ta.ta_frame *
		m0_pdclust_unit_size(m0_layout_to_pdl(fctx->sf_layout));
}

M0_INTERNAL int m0_sns_cm_cob_locate(struct m0_cob_domain *cdom,
				     const struct m0_fid *cob_fid)
{
	struct m0_cob        *cob;
	struct m0_cob_oikey   oikey;
	int                   rc;

	M0_ENTRY("dom=%p cob="FID_F, cdom, FID_P(cob_fid));

	m0_cob_oikey_make(&oikey, cob_fid, 0);
	rc = m0_cob_locate(cdom, &oikey, M0_CA_NSKEY_FREE, &cob);
	if (rc == 0) {
		M0_ASSERT(m0_fid_eq(cob_fid, m0_cob_fid(cob)));
		m0_cob_put(cob);
	}

	return M0_RC(rc);
}

M0_INTERNAL uint64_t m0_sns_cm_ag_nr_local_units(struct m0_sns_cm *scm,
						 struct m0_sns_cm_file_ctx *fctx,
						 uint64_t group)
{
	struct m0_pdclust_src_addr       sa;
	struct m0_pdclust_tgt_addr       ta;
	struct m0_fid                    cobfid;
	uint64_t                         nrlu = 0;
	struct m0_poolmach              *pm;
	struct m0_pdclust_layout        *pl;
	enum m0_sns_cm_local_unit_type  ut;
	int                              i;
	int                              start;
	int                              end;

	M0_ENTRY();

	M0_PRE(scm != NULL && fctx != NULL);

	pl = m0_layout_to_pdl(fctx->sf_layout);
	start = m0_sns_cm_ag_unit_start(scm, pl);
	end = m0_sns_cm_ag_unit_end(scm, pl);
	sa.sa_group = group;
	pm = fctx->sf_pm;
	for (i = start; i < end; ++i) {
		sa.sa_unit = i;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		ut = m0_sns_cm_local_unit_type_get(fctx, sa.sa_group, sa.sa_unit);
		if (M0_IN(ut, (M0_SNS_CM_UNIT_LOCAL, M0_SNS_CM_UNIT_HOLE_EOF)) &&
		    !scm->sc_helpers->sch_is_cob_failed(pm, ta.ta_obj) &&
		    !m0_sns_cm_unit_is_spare(fctx, group, i))
			M0_CNT_INC(nrlu);
	}
	M0_LEAVE("number of local units = %lu", nrlu);

	return nrlu;
}

M0_INTERNAL
uint64_t m0_sns_cm_ag_nr_global_units(const struct m0_sns_cm_ag *sag,
		struct m0_pdclust_layout *pl)
{
	return m0_pdclust_N(pl) + m0_pdclust_K(pl);
}

M0_INTERNAL
uint64_t m0_sns_cm_ag_nr_data_units(const struct m0_pdclust_layout *pl)
{
	return m0_pdclust_N(pl);
}

M0_INTERNAL
uint64_t m0_sns_cm_ag_nr_parity_units(const struct m0_pdclust_layout *pl)
{
	return m0_pdclust_K(pl);
}

M0_INTERNAL
uint64_t m0_sns_cm_ag_nr_spare_units(const struct m0_pdclust_layout *pl)
{
	return m0_pdclust_K(pl);
}

M0_INTERNAL uint64_t m0_sns_cm_ag_size(const struct m0_pdclust_layout *pl)
{
	return m0_sns_cm_ag_nr_data_units(pl) + 2 *
		m0_sns_cm_ag_nr_parity_units(pl);
}

M0_INTERNAL bool m0_sns_cm_is_cob_repaired(struct m0_poolmach *pm,
					   uint32_t cob_index)
{
	enum m0_pool_nd_state state_out = 0;
	M0_PRE(pm != NULL);

	m0_poolmach_device_state(pm, cob_index, &state_out);
	return state_out == M0_PNDS_SNS_REPAIRED;
}

M0_INTERNAL bool m0_sns_cm_is_cob_repairing(struct m0_poolmach *pm,
					    uint32_t cob_index)
{
	enum m0_pool_nd_state state_out = 0;
	M0_PRE(pm != NULL);

	m0_poolmach_device_state(pm, cob_index, &state_out);
	return state_out == M0_PNDS_SNS_REPAIRING;
}

M0_INTERNAL bool m0_sns_cm_is_cob_rebalancing(struct m0_poolmach *pm,
					      uint32_t cob_index)
{
	enum m0_pool_nd_state state_out = 0;
	M0_PRE(pm != NULL);

	m0_poolmach_device_state(pm, cob_index, &state_out);

	return state_out == M0_PNDS_SNS_REBALANCING;
}

M0_INTERNAL bool m0_sns_cm_unit_is_spare(struct m0_sns_cm_file_ctx *fctx,
					 uint64_t group_nr, uint64_t spare_nr)
{
	uint64_t                    data_unit_id_out;
	struct m0_fid               cobfid;
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_poolmach         *pm;
	struct m0_pdclust_instance *pi;
	struct m0_pdclust_layout   *pl;
	enum m0_pool_nd_state       state_out = 0;
	bool                        result = true;
	int                         rc;

	M0_ENTRY("index:%d", (int)spare_nr);

	pl = m0_layout_to_pdl(fctx->sf_layout);
	pi = fctx->sf_pi;
	pm = fctx->sf_pm;
	if (m0_pdclust_unit_classify(pl, spare_nr) == M0_PUT_SPARE) {
		/*
		 * Firstly, check if the device corresponding to the given spare
		 * unit is already repaired. If yes then the spare unit is empty
		 * or the data is already moved. So we need not repair the spare
		 * unit.
		 */
		M0_SET0(&sa);
		M0_SET0(&ta);
		sa.sa_unit = spare_nr;
		sa.sa_group = group_nr;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		rc = m0_poolmach_device_state(pm, ta.ta_obj, &state_out);
		M0_ASSERT(rc == 0);
		if (state_out == M0_PNDS_SNS_REPAIRED) {
			M0_LOG(M0_DEBUG, "repaired index:%d", (int)ta.ta_obj);
			goto out;
		}
		/*
		 * Failed spare unit may contain data of previously failed data
		 * unit from the parity group. Reverse map the spare unit to the
		 * repaired data/parity unit from the parity group.
		 * If we fail to map the spare unit to any of the previously
		 * failed data unit, means the spare is empty.
		 * We lock fctx to protect pdclust instance tile cache.
		 */
		m0_sns_cm_fctx_lock(fctx);
		rc = m0_sns_repair_data_map(pm, pl, pi,
					    group_nr, spare_nr,
					    &data_unit_id_out);
		m0_sns_cm_fctx_unlock(fctx);
		if (rc == -ENOENT) {
			M0_LOG(M0_DEBUG, "empty spare index:%d", (int)spare_nr);
			goto out;
		}
		M0_SET0(&sa);
		M0_SET0(&ta);

		sa.sa_unit = data_unit_id_out;
		sa.sa_group = group_nr;

		/*
		 * The mechanism to reverse map the spare unit to any of the
		 * previously failed data unit is generic and based to device
		 * failure information from the pool machine.
		 * Thus if the device hosting the reverse mapped data unit for
		 * the given spare is in M0_PNDS_SNS_REPAIRED state, means the
		 * given spare unit contains data and needs to be repaired, else
		 * it is empty.
		 */
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (m0_poolmach_device_is_in_spare_usage_array(pm, ta.ta_obj)) {
			rc = m0_poolmach_device_state(pm, ta.ta_obj,
					              &state_out);
			M0_ASSERT(rc == 0);
			if (!M0_IN(state_out, (M0_PNDS_SNS_REPAIRED,
					       M0_PNDS_SNS_REBALANCING)))
				goto out;
		}
	}
	result = false;
out:
	return result;
}

M0_INTERNAL
uint64_t m0_sns_cm_ag_spare_unit_nr(const struct m0_pdclust_layout *pl,
				    uint64_t fidx)
{
        return m0_sns_cm_ag_nr_data_units(pl) +
		m0_sns_cm_ag_nr_parity_units(pl) + fidx;
}

M0_INTERNAL uint64_t m0_sns_cm_ag_unit_start(const struct m0_sns_cm *scm,
					     const struct m0_pdclust_layout *pl)
{
        return scm->sc_helpers->sch_ag_unit_start(pl);
}

M0_INTERNAL uint64_t m0_sns_cm_ag_unit_end(const struct m0_sns_cm *scm,
					   const struct m0_pdclust_layout *pl)
{
	return scm->sc_helpers->sch_ag_unit_end(pl);
}

M0_INTERNAL int m0_sns_cm_ag_tgt_unit2cob(struct m0_sns_cm_ag *sag,
					  uint64_t tgt_unit,
					  struct m0_fid *cobfid)
{
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_sns_cm_file_ctx  *fctx = sag->sag_fctx;

	agid2fid(&sag->sag_base.cag_id, &fctx->sf_fid);
	sa.sa_group = agid2group(&sag->sag_base.cag_id);
	sa.sa_unit  = tgt_unit;
	m0_sns_cm_unit2cobfid(fctx, &sa, &ta, cobfid);

	return 0;
}

static const char *local_ep(const struct m0_cm *cm)
{
	struct m0_reqh        *rh = cm->cm_service.rs_reqh;
	struct m0_rpc_machine *rm = m0_cm_rpc_machine_find(rh);

	return m0_rpc_machine_ep(rm);
}

/**
 * Gets IO service which given cob is associated with.
 *
 * @note  The returned conf object has to be m0_confc_close()d eventually.
 *
 * @post  retval->cs_type == M0_CST_IOS
 * @post  retval->cs_endpoints[0] != NULL
 */
static struct m0_conf_service *
cm_conf_service_get(const struct m0_cm *cm,
		    const struct m0_pool_version *pv,
		    const struct m0_fid *cob_fid)
{
	struct m0_confc        *confc = m0_reqh2confc(cm->cm_service.rs_reqh);
	uint32_t                dev_idx;
	struct m0_fid           svc_fid;
	struct m0_conf_service *svc = 0;
	int                     rc;

	dev_idx = m0_fid_cob_device_id(cob_fid);
	svc_fid = pv->pv_pc->pc_dev2svc[dev_idx].pds_ctx->sc_fid;
	rc = m0_conf_service_get(confc, &svc_fid, &svc);
	M0_ASSERT(rc == 0); /* Error checking is for sissies. */

	M0_POST(svc->cs_type == M0_CST_IOS);
	M0_POST(svc->cs_endpoints[0] != NULL);
	return svc;
}

M0_INTERNAL const char *m0_sns_cm_tgt_ep(const struct m0_cm *cm,
					 const struct m0_pool_version *pv,
					 const struct m0_fid *cob_fid,
					 struct m0_conf_obj **hostage)
{
	struct m0_conf_service *svc;

	*hostage = NULL; /* prevents m0_confc_close(<garbage>) in FI case */
	if (M0_FI_ENABLED("local-ep"))
		return local_ep(cm);
	svc = cm_conf_service_get(cm, pv, cob_fid);
	*hostage = &svc->cs_obj;
	return svc->cs_endpoints[0];
}

M0_INTERNAL bool m0_sns_cm_is_local_cob(const struct m0_cm *cm,
					const struct m0_pool_version *pv,
					const struct m0_fid *cob_fid)
{
	struct m0_conf_obj *svc;
	bool                result;

	if (M0_FI_ENABLED("local-ep"))
		return true;
	result = m0_streq(m0_sns_cm_tgt_ep(cm, pv, cob_fid, &svc),
			  local_ep(cm));
	m0_confc_close(svc);
	return result;
}

M0_INTERNAL size_t m0_sns_cm_ag_unrepaired_units(const struct m0_sns_cm *scm,
						 struct m0_sns_cm_file_ctx *fctx,
						 uint64_t group,
						 struct m0_bitmap *fmap_out)
{
	struct m0_pdclust_src_addr sa;
	struct m0_pdclust_tgt_addr ta;
	struct m0_fid              cobfid;
	uint64_t                   upg;
	uint64_t                   unit;
	size_t                     group_failures = 0;
	struct m0_pdclust_layout  *pl;
	struct m0_poolmach        *pm;
	M0_ENTRY();

	M0_PRE(scm != NULL && fctx != NULL);

	pm = fctx->sf_pm;
	pl = m0_layout_to_pdl(fctx->sf_layout);
	upg = m0_sns_cm_ag_size(pl);
	sa.sa_group = group;
	for (unit = 0; unit < upg; ++unit) {
		if (m0_sns_cm_unit_is_spare(fctx, group, unit))
			continue;
		sa.sa_unit = unit;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cobfid);
		if (scm->sc_helpers->sch_is_cob_failed(pm, ta.ta_obj) &&
		    !m0_sns_cm_is_cob_repaired(pm, ta.ta_obj)) {
			M0_CNT_INC(group_failures);
			if (fmap_out != NULL) {
				M0_ASSERT(fmap_out->b_nr == upg);
				m0_bitmap_set(fmap_out, unit, true);
			}
		}
	}

	M0_LEAVE("number of failures in group = %lu are %u",
		 (unsigned long)group, (unsigned)group_failures);
	return group_failures;
}

M0_INTERNAL bool m0_sns_cm_ag_is_relevant(struct m0_sns_cm *scm,
					  struct m0_sns_cm_file_ctx *fctx,
					  const struct m0_cm_ag_id *id)
{
        struct m0_fid               fid;
        size_t                      group_failures;
        uint64_t                    group;
        bool                        result = false;

	M0_ENTRY();

        agid2fid(id,  &fid);
	group = id->ai_lo.u_lo;
	/* Firstly check if this group has any failed units. */
	group_failures = m0_sns_cm_ag_unrepaired_units(scm, fctx, group, NULL);
	if (group_failures > 0 )
		result = scm->sc_helpers->sch_ag_is_relevant(scm, fctx, group);

        return M0_RC(result);
}

M0_INTERNAL int m0_sns_cm_ag_in_cp_units(const struct m0_sns_cm *scm,
					 const struct m0_cm_ag_id *id,
					 struct m0_sns_cm_file_ctx *fctx,
					 uint32_t *in_cp_nr,
					 uint32_t *in_units_nr,
					 struct m0_cm_proxy_in_count *pcount)
{
	M0_PRE(m0_cm_is_locked(&scm->sc_base));

	return scm->sc_helpers->sch_ag_in_cp_units(scm, id, fctx, in_cp_nr,
						   in_units_nr, pcount);
}

M0_INTERNAL bool m0_sns_cm_fid_is_valid(const struct m0_sns_cm *snscm,
				        const struct m0_fid *fid)
{
        return m0_fid_is_set(fid) && m0_fid_is_valid(fid) &&
	       m0_sns_cm2reqh(snscm)->rh_oostore ? true:
		fid->f_key >= M0_MDSERVICE_START_FID.f_key;
}

M0_INTERNAL struct m0_reqh *m0_sns_cm2reqh(const struct m0_sns_cm *snscm)
{
	return snscm->sc_base.cm_service.rs_reqh;
}

M0_INTERNAL bool m0_sns_cm_disk_has_dirty_pver(struct m0_cm *cm,
					       struct m0_conf_drive *disk,
					       bool clear)
{
	struct m0_conf_pver   **conf_pvers;
	struct m0_pool_version *pver;
	struct m0_pools_common *pc = cm->cm_service.rs_reqh->rh_pools;
	uint32_t                k;

	conf_pvers = disk->ck_pvers;
	for (k = 0; conf_pvers[k] != NULL; ++k) {
		pver = m0_pool_version_find(pc, &conf_pvers[k]->pv_obj.co_id);
		if (m0_sns_cm_pver_is_dirty(pver)) {
			M0_LOG(M0_FATAL, "pver %p is dirty, clearing", pver);
			if (clear)
				pver->pv_sns_flags = 0;
			return true;
		}
	}

	return false;
}

M0_INTERNAL bool m0_sns_cm_pver_is_dirty(struct m0_pool_version *pver)
{
	return pver->pv_sns_flags & PV_SNS_DIRTY;
}

M0_INTERNAL void m0_sns_cm_pver_dirty_set(struct m0_pool_version *pver)
{
	pver->pv_sns_flags |= PV_SNS_DIRTY;
}

M0_INTERNAL int m0_sns_cm_pool_ha_nvec_alloc(struct m0_pool *pool,
					     enum m0_pool_nd_state state,
					     struct m0_ha_nvec *nvec)
{
	struct m0_pooldev  *pd;
	struct m0_ha_note  *note;
	uint64_t            nr_devs = 0;

	m0_tl_for(pool_failed_devs, &pool->po_failed_devices, pd) {
		if (pd->pd_state == state)
			M0_CNT_INC(nr_devs);
	} m0_tl_endfor;
	if (nr_devs == 0)
		return M0_ERR(-ENOENT);
	/* We add one more ha note for pool. */
	M0_ALLOC_ARR(note, nr_devs + 1);
	if (note == NULL)
		return M0_ERR(-ENOMEM);
	nvec->nv_nr = nr_devs + 1;
	nvec->nv_note = note;

	return 0;
}

M0_INTERNAL
bool m0_sns_cm_group_has_local_presence(struct m0_sns_cm_file_ctx *fctx,
					uint32_t units_per_group, uint64_t group)
{
	struct m0_pdclust_src_addr  sa;
	struct m0_pdclust_tgt_addr  ta;
	struct m0_fid               cob_fid;
	struct m0_sns_cm           *scm;
	uint32_t                    i;
	int                         rc;

	scm = fctx->sf_scm;
	sa.sa_group = group;
	for (i = 0; i < units_per_group; ++i) {
		sa.sa_unit = i;
		m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cob_fid);
		rc = m0_sns_cm_cob_locate(scm->sc_cob_dom, &cob_fid);
		if (rc == 0)
			return true;
	}

	return false;
}

M0_INTERNAL enum m0_sns_cm_local_unit_type
m0_sns_cm_local_unit_type_get(struct m0_sns_cm_file_ctx *fctx, uint64_t group,
			      uint64_t unit)
{
	struct m0_pdclust_src_addr sa;
	struct m0_pdclust_tgt_addr ta;
	struct m0_fid              cob_fid;
	struct m0_pool_version    *pv;
	struct m0_sns_cm          *scm;
	int                        rc;

	if (group > fctx->sf_max_group)
		return M0_SNS_CM_UNIT_INVALID;
	sa.sa_group = group;
	sa.sa_unit = unit;
	scm = fctx->sf_scm;
	pv = fctx->sf_pm->pm_pver;
	m0_sns_cm_unit2cobfid(fctx, &sa, &ta, &cob_fid);
	rc = m0_sns_cm_cob_locate(scm->sc_cob_dom, &cob_fid);
	if (rc == -ENOENT) {
		if (m0_sns_cm_is_local_cob(&scm->sc_base, pv, &cob_fid))
			return M0_SNS_CM_UNIT_HOLE_EOF;
	}

	return rc == 0 ? M0_SNS_CM_UNIT_LOCAL : M0_SNS_CM_UNIT_INVALID;
}

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
