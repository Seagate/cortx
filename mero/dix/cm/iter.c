/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 23-Aug-2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include <unistd.h>
#include "lib/trace.h"

#include "lib/memory.h"
#include "sm/sm.h"
#include "fop/fom.h"
#include "dix/cm/cm.h"   /* m0_dix_cm_type */

#include "pool/pool.h"
#include "pool/pool_machine.h"
#include "reqh/reqh.h"
#include "lib/finject.h"

#include "dix/fid_convert.h"
#include "dix/cm/iter.h"

/**
 * @addtogroup DIXCM
 *
 * @{
 */

enum dix_cm_iter_phase {
	DIX_ITER_INIT    = M0_FOM_PHASE_INIT,
	DIX_ITER_FINAL   = M0_FOM_PHASE_FINISH,
	DIX_ITER_STARTED = M0_FOM_PHASE_NR,
	DIX_ITER_DEL_LOCK,
	DIX_ITER_CTIDX_START,
	DIX_ITER_CTIDX_REPOS,
	DIX_ITER_CTIDX_NEXT,
	DIX_ITER_NEXT_CCTG,
	DIX_ITER_META_LOCK,
	DIX_ITER_CCTG_LOOKUP,
	DIX_ITER_CCTG_START,
	DIX_ITER_CCTG_CONT,
	DIX_ITER_CCTG_CUR_NEXT,
	DIX_ITER_NEXT_KEY,
	DIX_ITER_IDLE_START,
	DIX_ITER_DEL_TX_OPENED,
	DIX_ITER_DEL_TX_WAIT,
	DIX_ITER_DEL_TX_DONE,
	DIX_ITER_IDLE_FIN,
	DIX_ITER_CCTG_CHECK,
	DIX_ITER_EOF,
	DIX_ITER_FAILURE,
};

static struct m0_sm_state_descr dix_cm_iter_phases[] = {
	[DIX_ITER_INIT] = {
		.sd_flags     = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(DIX_ITER_STARTED)
	},
	[DIX_ITER_STARTED] = {
		.sd_name      = "started",
		.sd_allowed   = M0_BITS(DIX_ITER_DEL_LOCK, DIX_ITER_EOF)
	},
	[DIX_ITER_DEL_LOCK] = {
		.sd_name      = "self-lock",
		.sd_allowed   = M0_BITS(DIX_ITER_CTIDX_START)
	},
	[DIX_ITER_CTIDX_START] = {
		.sd_name      = "ctidx-first-key",
		.sd_allowed   = M0_BITS(DIX_ITER_NEXT_CCTG, DIX_ITER_FAILURE)
	},
	[DIX_ITER_CTIDX_REPOS] = {
		.sd_name      = "ctidx-reposition",
		.sd_allowed   = M0_BITS(DIX_ITER_NEXT_CCTG, DIX_ITER_FAILURE)
	},
	[DIX_ITER_CTIDX_NEXT] = {
		.sd_name      = "ctidx-next",
		.sd_allowed   = M0_BITS(DIX_ITER_NEXT_CCTG, DIX_ITER_FAILURE)
	},
	[DIX_ITER_NEXT_CCTG] = {
		.sd_name      = "next-cctg",
		.sd_allowed   = M0_BITS(DIX_ITER_META_LOCK,
					DIX_ITER_EOF,
					DIX_ITER_FAILURE)
	},
	[DIX_ITER_META_LOCK] = {
		.sd_name      = "meta-lock",
		.sd_allowed   = M0_BITS(DIX_ITER_CCTG_LOOKUP, DIX_ITER_FAILURE)
	},
	[DIX_ITER_CCTG_LOOKUP] = {
		.sd_name      = "cctg-meta-lookup",
		.sd_allowed   = M0_BITS(DIX_ITER_CCTG_START,
					DIX_ITER_CCTG_CONT,
					DIX_ITER_CTIDX_REPOS,
					DIX_ITER_FAILURE)
	},
	[DIX_ITER_CCTG_START] = {
		.sd_name      = "cctg-first-record",
		.sd_allowed   = M0_BITS(DIX_ITER_NEXT_KEY, DIX_ITER_FAILURE)
	},
	[DIX_ITER_CCTG_CUR_NEXT] = {
		.sd_name      = "cctg-cur-next",
		.sd_allowed   = M0_BITS(DIX_ITER_NEXT_KEY, DIX_ITER_FAILURE)
	},
	[DIX_ITER_NEXT_KEY] = {
		.sd_name      = "next-key",
		.sd_allowed   = M0_BITS(DIX_ITER_IDLE_START,
					DIX_ITER_CTIDX_NEXT,
					DIX_ITER_CCTG_CUR_NEXT,
					DIX_ITER_FAILURE)
	},
	[DIX_ITER_IDLE_START] = {
		.sd_name      = "idle-start",
		.sd_allowed   = M0_BITS(DIX_ITER_IDLE_FIN,
					DIX_ITER_DEL_TX_OPENED)
	},
	[DIX_ITER_DEL_TX_OPENED] = {
		.sd_name      = "del_tx_opened",
		.sd_allowed   = M0_BITS(DIX_ITER_DEL_TX_WAIT, DIX_ITER_FAILURE)
	},
	[DIX_ITER_DEL_TX_WAIT] = {
		.sd_name      = "del_tx_wait",
		.sd_allowed   = M0_BITS(DIX_ITER_DEL_TX_DONE, DIX_ITER_FAILURE)
	},
	[DIX_ITER_DEL_TX_DONE] = {
		.sd_name      = "del_tx_done",
		.sd_allowed   = M0_BITS(DIX_ITER_IDLE_FIN, DIX_ITER_FAILURE)
	},
	[DIX_ITER_IDLE_FIN] = {
		.sd_name      = "idle-fin",
		.sd_allowed   = M0_BITS(DIX_ITER_CCTG_CHECK, DIX_ITER_EOF)
	},
	[DIX_ITER_CCTG_CHECK] = {
		.sd_name      = "del-check",
		.sd_allowed   = M0_BITS(DIX_ITER_CCTG_CONT,
					DIX_ITER_CTIDX_REPOS)
	},
	[DIX_ITER_CCTG_CONT] = {
		.sd_name      = "cctg-continue",
		.sd_allowed   = M0_BITS(DIX_ITER_NEXT_KEY, DIX_ITER_FAILURE)
	},
	[DIX_ITER_EOF] = {
		.sd_name      = "end-of-iterator",
		.sd_allowed   = M0_BITS(DIX_ITER_FINAL)
	},
	[DIX_ITER_FINAL] = {
		.sd_name      = "final",
		.sd_flags     = M0_SDF_TERMINAL,
	},
	[DIX_ITER_FAILURE] = {
		.sd_name      = "failure",
		.sd_allowed   = M0_BITS(DIX_ITER_FINAL),
		.sd_flags     = M0_SDF_FAILURE
	}
};

/** @todo Revise this array. */
static struct m0_sm_trans_descr dix_cm_iter_trans[] = {
	{ "start",            DIX_ITER_INIT,        DIX_ITER_STARTED     },
	{ "get-first-key",    DIX_ITER_STARTED,     DIX_ITER_CTIDX_NEXT  },
	{ "next-cctg",        DIX_ITER_IDLE_START,  DIX_ITER_CCTG_CUR_NEXT },
	{ "no-more-indices",  DIX_ITER_IDLE_START,  DIX_ITER_EOF         },
	{ "next-key-fetched", DIX_ITER_NEXT_KEY,    DIX_ITER_IDLE_START  },
	{ "index-eof",        DIX_ITER_NEXT_KEY,    DIX_ITER_NEXT_CCTG   },
	{ "failure",          DIX_ITER_NEXT_KEY,    DIX_ITER_FAILURE     },
	{ "get-first-key",    DIX_ITER_NEXT_CCTG,   DIX_ITER_CCTG_LOOKUP },
	{ "no-more-indices",  DIX_ITER_NEXT_CCTG,   DIX_ITER_EOF         },
	{ "failure",          DIX_ITER_NEXT_CCTG,   DIX_ITER_FAILURE     },
	{ "get-first-key",    DIX_ITER_CCTG_LOOKUP, DIX_ITER_NEXT_KEY    },
	{ "cctg-not-found",   DIX_ITER_CCTG_LOOKUP, DIX_ITER_FAILURE     },
	{ "finalise",         DIX_ITER_EOF,         DIX_ITER_FINAL       },
	{ "finalise",         DIX_ITER_FAILURE,     DIX_ITER_FINAL       },
};

static const struct m0_sm_conf dix_cm_iter_sm_conf = {
	.scf_name      = "dix_cm_iter",
	.scf_nr_states = ARRAY_SIZE(dix_cm_iter_phases),
	.scf_state     = dix_cm_iter_phases,
	.scf_trans_nr  = ARRAY_SIZE(dix_cm_iter_trans),
	.scf_trans     = dix_cm_iter_trans
};

static int dix_cm_iter_buf_copy(struct m0_buf *dst,
				struct m0_buf *src,
				m0_bcount_t    cutoff)
{
	M0_PRE(dst->b_nob == 0 && dst->b_addr == NULL);

	return src->b_nob >= cutoff ?
		m0_buf_copy_aligned(dst, src, PAGE_SHIFT) :
		m0_buf_copy(dst, src);
}

static uint64_t dix_cm_iter_fom_locality(const struct m0_fom *fom)
{
	return fom->fo_type->ft_id;
}

static bool dix_cm_iter_meta_clink_cb(struct m0_clink *cl)
{
	struct m0_dix_cm_iter *iter = M0_AMB(iter, cl, di_meta_clink);

	iter->di_meta_modified = true;
	return false;
}

static void dix_cm_iter_init(struct m0_dix_cm_iter *iter)
{
	struct m0_fom *fom = &iter->di_fom;

	iter->di_tgts = NULL;
	iter->di_tgts_cur = 0;
	iter->di_tgts_nr = 0;

	M0_SET0(&iter->di_key);
	M0_SET0(&iter->di_val);

	iter->di_processed_recs_nr = 0;
	iter->di_cctg_processed_recs_nr = 0;

	m0_ctg_op_init(&iter->di_ctidx_op, fom, COF_SLANT);
	m0_ctg_cursor_init(&iter->di_ctidx_op, m0_ctg_ctidx());
	m0_long_lock_link_init(&iter->di_lock_link, fom,
			       &iter->di_lock_addb2);
	m0_long_lock_link_init(&iter->di_meta_lock_link, fom,
			       &iter->di_meta_lock_addb2);
	m0_long_lock_link_init(&iter->di_del_lock_link, fom,
			       &iter->di_del_lock_addb2);

	/* Subscribe to meta catalogue modifications. */
	m0_clink_init(&iter->di_meta_clink, dix_cm_iter_meta_clink_cb);
	m0_clink_add_lock(&m0_ctg_meta()->cc_chan.bch_chan,
			  &iter->di_meta_clink);
}

static void dix_cm_iter_dtx_fini(struct m0_dix_cm_iter *iter)
{
	struct m0_fom *fom = &iter->di_fom;

	m0_dtx_fini(&fom->fo_tx);
	M0_SET0(m0_fom_tx(fom));
}

static void dix_cm_iter_tgts_fini(struct m0_dix_cm_iter *iter)
{
	m0_free(iter->di_tgts);
	iter->di_tgts = NULL;
	iter->di_tgts_cur = 0;
	iter->di_tgts_nr = 0;
}

static void dix_cm_iter_fini(struct m0_dix_cm_iter *iter)
{
	if (iter->di_cctg != NULL)
		m0_long_unlock(m0_ctg_lock(iter->di_cctg), &iter->di_lock_link);
	m0_long_unlock(m0_ctg_lock(m0_ctg_ctidx()), &iter->di_meta_lock_link);
	m0_long_unlock(m0_ctg_lock(m0_ctg_meta()), &iter->di_meta_lock_link);
	m0_long_unlock(m0_ctg_del_lock(), &iter->di_del_lock_link);
	m0_buf_free(&iter->di_prev_key);
	m0_buf_free(&iter->di_key);
	m0_buf_free(&iter->di_val);
	dix_cm_iter_tgts_fini(iter);
	m0_long_lock_link_fini(&iter->di_del_lock_link);
	m0_long_lock_link_fini(&iter->di_meta_lock_link);
	m0_long_lock_link_fini(&iter->di_lock_link);
	m0_ctg_cursor_fini(&iter->di_ctidx_op);
	m0_ctg_op_fini(&iter->di_ctidx_op);
	m0_clink_del_lock(&iter->di_meta_clink);
	m0_clink_fini(&iter->di_meta_clink);
}

static int dix_cm_iter_failure(struct m0_dix_cm_iter *iter, int rc)
{
	M0_PRE(rc < 0);
	m0_fom_phase_move(&iter->di_fom, rc, DIX_ITER_FAILURE);
	m0_chan_broadcast_lock(&iter->di_completed);
	return M0_FSO_WAIT;
}

static int dix_cm_iter_eof(struct m0_dix_cm_iter *iter)
{
	m0_fom_phase_set(&iter->di_fom, DIX_ITER_EOF);
	m0_chan_broadcast_lock(&iter->di_completed);
	return M0_FSO_WAIT;
}

static int dix_cm_iter_idle(struct m0_dix_cm_iter *iter)
{
	m0_fom_phase_set(&iter->di_fom, DIX_ITER_IDLE_START);
	m0_chan_broadcast_lock(&iter->di_completed);
	return M0_FSO_WAIT;
}

static int dix_cm_iter_dtx_failure(struct m0_dix_cm_iter *iter)
{
	struct m0_fom   *fom = &iter->di_fom;
	struct m0_be_tx *tx = m0_fom_tx(fom);

	dix_cm_iter_dtx_fini(iter);
	return dix_cm_iter_failure(iter, tx->t_sm.sm_rc);
}

static struct m0_poolmach *dix_cm_pm_get(struct m0_dix_cm     *dix_cm,
					 struct m0_dix_layout *layout)
{
	struct m0_reqh         *reqh;
	struct m0_pool_version *pver;

	M0_PRE(dix_cm != NULL);
	M0_PRE(layout != NULL);

	reqh = dix_cm->dcm_base.cm_service.rs_reqh;
	pver = m0_pool_version_find(reqh->rh_pools,
				    &layout->u.dl_desc.ld_pver);
	return &pver->pv_mach;
}

static int dix_cm_layout_iter_init(struct m0_dix_layout_iter *iter,
				   const struct m0_fid       *index,
				   struct m0_dix_layout      *layout,
				   struct m0_dix_cm          *dix_cm,
				   struct m0_buf             *key)
{
	struct m0_reqh          *reqh;
	struct m0_layout_domain *ldom;
	struct m0_pool_version  *pver;

	M0_PRE(index != NULL);
	M0_PRE(layout != NULL);
	M0_PRE(dix_cm != NULL);
	M0_PRE(key != NULL);

	reqh = dix_cm->dcm_base.cm_service.rs_reqh;
	ldom = &reqh->rh_ldom;
	pver = m0_pool_version_find(reqh->rh_pools,
				    &layout->u.dl_desc.ld_pver);
	return m0_dix_layout_iter_init(iter, index, ldom, pver,
				       &layout->u.dl_desc, key);
}


static int dix_cm_tgt_to_unit(uint64_t  tgt,
			      uint64_t *group_tgts,
			      uint64_t  group_tgts_nr,
			      uint64_t *unit)
{
	uint64_t i;
	int      rc = 0;

	M0_PRE(group_tgts != NULL);
	M0_PRE(unit != NULL);

	*unit = 0;

	for (i = 0; i < group_tgts_nr; i++) {
		if (group_tgts[i] == tgt) {
			*unit = i;
			break;
		}
	}

	if (i == group_tgts_nr)
		rc = M0_ERR(-ENOENT);

	return M0_RC(rc);
}

static
bool dix_cm_repair_spare_has_data(struct m0_dix_layout_iter *iter,
				  struct m0_poolmach        *pm,
				  uint64_t                  *group_tgts,
				  uint64_t                   group_tgts_nr,
				  uint64_t                   spare_id)
{
	uint64_t                    spare_id_loc;
	uint64_t                    unit;
	uint64_t                    device_index;
	struct m0_pool_spare_usage *spare_usage_array;
	bool                        has_data = false;
	int                         rc;

	if (spare_id == 0)
		return false;

	spare_usage_array = pm->pm_state->pst_spare_usage_array;
	spare_id_loc = spare_id;
	device_index = spare_usage_array[spare_id_loc].psu_device_index;

	/*
	 * Go backward through spare usage array starting from the previous unit
	 * relative to passed spare unit to check whether passed spare unit
	 * contains data as a result of previous data reconstructions
	 * (a.g. resolve "failures chain").
	 */
	do {
		M0_CNT_DEC(spare_id_loc);

		/* Skip device that is not in the current "failure chain". */
		unit = spare_id_loc + m0_dix_liter_N(iter) +
			m0_dix_liter_K(iter);
		if (group_tgts[unit] != device_index)
			continue;

		/*
		 * Convert previously failed device to unit number inside of
		 * parity group.
		 */
		device_index = spare_usage_array[spare_id_loc].
			psu_device_index;
		M0_ASSERT(device_index != POOL_PM_SPARE_SLOT_UNUSED);

		rc = dix_cm_tgt_to_unit(device_index,
					group_tgts,
					group_tgts_nr,
					&unit);
		/*
		 * It may occur that previously failed device is outside of
		 * current parity group, break current failure chain in this
		 * case.
		 */
		M0_ASSERT(rc == 0 || rc == -ENOENT);
		if (rc == -ENOENT)
			break;
		/*
		 * Go further if previously failed device serves spare unit
		 * until:
		 *  - failed device is mapped to data or parity unit;
		 *  - all previuos spare units of parity group are checked.
		 */
		if (m0_dix_liter_unit_classify(iter, unit) != M0_PUT_SPARE) {
			/*
			 * Device that serves data unit or parity unit is failed
			 * initially, set has_data to true if it was repaired,
			 * leave as false otherwise and break the loop.
			 */
			if (spare_usage_array[spare_id_loc].psu_device_state !=
			    M0_PNDS_SNS_REPAIRING)
				has_data = true;
			break;
		}
	} while (spare_id_loc > 0);

	return has_data;
}

static int dix_cm_rebalance_tgts_get(struct m0_dix_layout_iter *iter,
				     struct m0_poolmach        *pm,
				     uint64_t                  *group_tgts,
				     uint64_t                   group_tgts_nr,
				     uint64_t                   spare_id,
				     uint64_t                 **tgts,
				     uint64_t                  *tgts_nr)
{
	uint32_t                    N;
	uint32_t                    K;
	uint64_t                    unit;
	uint64_t                    tgt_tmp;
	uint64_t                    device_index;
	uint64_t                    spare_id_loc;
	struct m0_pool_spare_usage *spare_usage_array;
	bool                        is_set = false;
	bool                        has_data = false;
	bool                        is_first_cycle = true;
	int                         rc = 0;

	*tgts = NULL;
	*tgts_nr = 0;

	spare_usage_array = pm->pm_state->pst_spare_usage_array;

	spare_id_loc = spare_id;
	N = m0_dix_liter_N(iter);
	K = m0_dix_liter_K(iter);

	do {
		if (!is_first_cycle) {
			M0_CNT_DEC(spare_id_loc);
			unit = spare_id_loc + N + K;
			if (group_tgts[unit] != device_index) {
				continue;
			}
		} else
			is_first_cycle = false;
		/*
		 * Convert previously failed device to unit number inside of
		 * parity group.
		 */
		device_index = spare_usage_array[spare_id_loc].
			psu_device_index;
		/*
		 * Skip unused slots as they may occur as a result of
		 * successive repair/rebalance processes.
		 */
		if (device_index == POOL_PM_SPARE_SLOT_UNUSED)
			continue;
		rc = dix_cm_tgt_to_unit(device_index,
					group_tgts,
					group_tgts_nr,
					&unit);
		/*
		 * It may occur that previously failed device is outside of
		 * current parity group, break current failure chain in this
		 * case.
		 */
		M0_ASSERT(rc == 0 || rc == -ENOENT);
		if (rc == -ENOENT)
			break;
		/*
		 * We are looking for the last rebalancing device in failure
		 * chain, full failure chain should be checked.
		 */
		if (spare_usage_array[spare_id_loc].psu_device_state ==
		    M0_PNDS_SNS_REBALANCING) {
			/*
			 * Rebalancing device found during checking of failure
			 * chain, set it as target candidate.
			 */
			tgt_tmp = device_index;
			is_set = true;
		}
		if (m0_dix_liter_unit_classify(iter, unit) != M0_PUT_SPARE) {
			/*
			 * Non-spare slot achieved during checking of failure
			 * chain, it means that served locally spare contains
			 * real data.
			 */
			has_data = true;
			break;
		}
	} while (spare_id_loc > 0);

	if (is_set && has_data) {
		M0_ALLOC_ARR(*tgts, 1);
		if (*tgts == NULL)
			rc = M0_ERR(-ENOMEM);
		else {
			*tgts[0] = tgt_tmp;
			*tgts_nr = 1;
		}
	}

	return M0_RC(rc);
}

static int dix_cm_repair_tgts_get(struct m0_dix_layout_iter *iter,
				  struct m0_poolmach        *pm,
				  uint64_t                 **tgts,
				  uint64_t                  *tgts_nr)
{
	uint32_t                    i;
	uint64_t                    unit;
	uint64_t                    units_nr;
	uint64_t                    spare_id;
	uint64_t                    tgt_tmp;
	uint64_t                   *group_tgts;
	uint64_t                    group_tgts_nr;
	uint64_t                    device_index;
	uint32_t                    device_state;
	uint64_t                   *tgts_loc;
	uint64_t                    tgts_nr_loc;
	uint32_t                    max_device_failures;
	struct m0_pool_spare_usage *spare_usage_array;
	enum m0_pool_nd_state       state;
	int                         rc = 0;

	M0_PRE(iter != NULL);
	M0_PRE(pm != NULL);
	M0_PRE(tgts != NULL);
	M0_PRE(tgts_nr != NULL);

	*tgts = NULL;
	*tgts_nr = 0;

	if (M0_FI_ENABLED("single_target")) {
		*tgts_nr = 1;
		M0_ALLOC_ARR(*tgts, *tgts_nr);
		(*tgts)[0] = 1;
		return 0;
	}

	m0_dix_layout_iter_reset(iter);

	/*
	 * Firstly get all target devices of parity group that the key belongs
	 * to, it is needed to skip failed target devices that are not
	 * considered in scope of current parity group.
	 */
	group_tgts_nr = m0_dix_liter_W(iter);
	M0_ALLOC_ARR(group_tgts, group_tgts_nr);
	if (group_tgts == NULL)
		return M0_ERR(-ENOMEM);

	unit = 0;
	units_nr = group_tgts_nr;

	while (unit < units_nr) {
		m0_dix_layout_iter_next(iter, &tgt_tmp);
		group_tgts[unit] = tgt_tmp;
		M0_CNT_INC(unit);
	}

	/*
	 * Allocate memory for resulting target devices, precise number of
	 * targets is not determined at this stage, so allocate array of length
	 * that equals to the maximum number of device failures (i.e. count of
	 * all spare units in parity group). It guarantees that such number of
	 * array elements is enough to store resulting targets. Actual number of
	 * resulting targets will be set at the end of the algo, set it to 0
	 * initially.
	 */
	tgts_nr_loc = 0;
	M0_ALLOC_ARR(tgts_loc, pm->pm_state->pst_max_device_failures);
	if (tgts_loc == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}

	m0_rwlock_read_lock(&pm->pm_lock);

	spare_usage_array = pm->pm_state->pst_spare_usage_array;
	max_device_failures = pm->pm_state->pst_max_device_failures;

	/*
	 * Go through the spare usage array to determine target devices where
	 * current key-value record should be reconstructed.
	 */
	for (i = 0; i < max_device_failures; i++) {
		spare_id = i;
		device_index = spare_usage_array[spare_id].psu_device_index;
		device_state = spare_usage_array[spare_id].psu_device_state;

		/* Unused slot is achieved, does not make sense to continue. */
		if (device_index == POOL_PM_SPARE_SLOT_UNUSED)
			break;
		/*
		 * Skip device if it is not under repairing or it is not in
		 * the devices set of current parity group.
		 */
		if (device_state != M0_PNDS_SNS_REPAIRING ||
		    !m0_exists(j, group_tgts_nr, group_tgts[j] == device_index))
			continue;

		/* Get target device where current spare unit is stored. */
		tgt_tmp = group_tgts[spare_id + m0_dix_liter_N(iter) +
				     m0_dix_liter_K(iter)];
		/* Skip spare unit if it is not online. */
		state = pm->pm_state->pst_devices_array[tgt_tmp].pd_state;
		if (state != M0_PNDS_ONLINE)
			continue;

		rc = dix_cm_tgt_to_unit(device_index,
					group_tgts,
					group_tgts_nr,
					&unit);
		/* Must be true, check the logic otherwise. */
		M0_ASSERT(rc == 0);
		/* Skip empty spare unit. */
		if (m0_dix_liter_unit_classify(iter, unit) == M0_PUT_SPARE &&
		    !dix_cm_repair_spare_has_data(iter, pm, group_tgts,
						  group_tgts_nr, spare_id))
			continue;

		M0_ASSERT(!m0_exists(j, tgts_nr_loc, tgts_loc[j] == tgt_tmp));
		tgts_loc[tgts_nr_loc] = tgt_tmp;
		M0_CNT_INC(tgts_nr_loc);
	}

	m0_rwlock_read_unlock(&pm->pm_lock);
out:
	m0_free(group_tgts);
	if (rc == 0 && tgts_nr_loc > 0) {
		*tgts = tgts_loc;
		*tgts_nr = tgts_nr_loc;
	} else
		m0_free(tgts_loc);

	return M0_RC(rc);
}

static void parity_group_print(struct m0_dix_layout_iter *iter,
			       struct m0_poolmach        *pm,
			       uint64_t                   local_device,
			       struct m0_buf             *key)
{
	uint64_t               unit;
	uint64_t               units_nr;
	uint64_t               tgt;
	struct m0_pooldev     *global_dev;
	enum m0_pool_nd_state  state;

	m0_dix_layout_iter_reset(iter);

	unit = 0;
	units_nr = m0_dix_liter_W(iter);

	M0_LOG(M0_DEBUG, "Parity group info for key %"PRIu64
	       " (pool dev_id/global dev_id):",
	       be64toh(*(uint64_t *)key->b_addr));
	M0_LOG(M0_DEBUG, "=============================================");
	while (unit < units_nr) {
		m0_dix_layout_iter_next(iter, &tgt);
		M0_ASSERT(tgt < pm->pm_state->pst_nr_devices);
		state = pm->pm_state->pst_devices_array[tgt].pd_state;
		global_dev = m0_dix_tgt2sdev(&iter->dit_linst, tgt);
		M0_LOG(M0_DEBUG,
			"%"PRIu64"%s: dev_id: %"PRIu64"/%u, %s, %s%s",
			unit,
			units_nr > 10 ? (unit < 10 ? " " : "") : "",
			tgt,
			global_dev->pd_sdev_idx,
			unit < m0_dix_liter_N(iter) ?
			"  data" :
			unit < m0_dix_liter_N(iter) + m0_dix_liter_K(iter) ?
			"parity" :
			" spare",
			m0_pool_dev_state_to_str(state),
			local_device == global_dev->pd_sdev_idx ?
						", (local)" : "");
		M0_CNT_INC(unit);
	}
	M0_LOG(M0_DEBUG, "=============================================");
}

static void spare_usage_print(struct m0_dix_layout_iter *iter,
			      struct m0_poolmach        *pm)
{
	uint32_t                    i;
	uint64_t                    tgt;
	struct m0_pooldev          *global_dev;
	uint32_t                    max_device_failures;
	struct m0_pool_spare_usage *spare_usage_array;
	enum m0_pool_nd_state       state;

	m0_dix_layout_iter_goto(iter, m0_dix_liter_N(iter) +
				m0_dix_liter_K(iter));

	spare_usage_array = pm->pm_state->pst_spare_usage_array;
	max_device_failures = pm->pm_state->pst_max_device_failures;

	M0_LOG(M0_DEBUG,
	       "Spare usage array info (pool dev_id/global dev_id):");
	M0_LOG(M0_DEBUG, "=============================================");
	for (i = 0; i < max_device_failures; i++) {
		tgt = spare_usage_array[i].psu_device_index;
		if (tgt != POOL_PM_SPARE_SLOT_UNUSED) {
			state = spare_usage_array[i].psu_device_state;
			global_dev = m0_dix_tgt2sdev(&iter->dit_linst, tgt);
			M0_LOG(M0_DEBUG,
			       "%u: dev_id: %"PRIu64"/%u, %s, served by:",
			       i,
			       tgt,
			       global_dev->pd_sdev_idx,
			       m0_pool_dev_state_to_str(state));
		} else
			M0_LOG(M0_DEBUG, "%u: slot unused, served by:",
						i);

		m0_dix_layout_iter_next(iter, &tgt);
		M0_ASSERT(tgt < pm->pm_state->pst_nr_devices);
		state = pm->pm_state->pst_devices_array[tgt].pd_state;
		global_dev = m0_dix_tgt2sdev(&iter->dit_linst, tgt);
		M0_LOG(M0_DEBUG,
		       "%s  dev_id: %"PRIu64"/%u, %s",
		       max_device_failures > 10 ? "  " : " ",
		       tgt,
		       global_dev->pd_sdev_idx,
		       m0_pool_dev_state_to_str(state));
	}
	M0_LOG(M0_DEBUG, "=============================================");
}

static void tgts_print(struct m0_dix_layout_iter *iter,
		       uint64_t                  *tgts,
		       uint64_t                   tgts_nr,
		       struct m0_buf             *key)
{
	struct m0_pooldev *global_dev;
	uint64_t           i;

	m0_dix_layout_iter_reset(iter);

	M0_LOG(M0_DEBUG, "Targets for key %"PRIu64
	       " (pool dev_id/global dev_id):",
	       be64toh(*(uint64_t *)key->b_addr));
	M0_LOG(M0_DEBUG, "=============================================");
	for (i = 0; i < tgts_nr; i++) {
		global_dev = m0_dix_tgt2sdev(&iter->dit_linst, tgts[i]);
		M0_LOG(M0_DEBUG,
		       "%"PRIu64"%s: dev_id: %"PRIu64"/%u",
		       i + 1,
		       tgts_nr > 10 ? (i < 10 ? " " : "") : "",
		       tgts[i],
		       global_dev->pd_sdev_idx);
	}
	M0_LOG(M0_DEBUG, "=============================================");
}

static bool dix_cm_is_repair_coordinator(struct m0_dix_layout_iter *iter,
					 struct m0_poolmach        *pm,
					 uint64_t                   local_dev)
{
	uint64_t              unit;
	uint64_t              units_nr;
	uint64_t              tgt;
	struct m0_pooldev    *sdev;
	bool                  coord = false;

	M0_PRE(iter != NULL);
	M0_PRE(pm != NULL);

	if (M0_FI_ENABLED("always_coordinator"))
		return true;

	m0_dix_layout_iter_reset(iter);

	unit = 0;
	/*
	 * Consider only data and parity units. If the first alive unit is spare
	 * then we can not reconstruct parity.
	 */
	units_nr = m0_dix_liter_N(iter) + m0_dix_liter_K(iter);

	while (unit < units_nr) {
		m0_dix_layout_iter_next(iter, &tgt);

		M0_ASSERT(tgt < pm->pm_state->pst_nr_devices);

		sdev = &pm->pm_state->pst_devices_array[tgt];
		if (sdev->pd_state != M0_PNDS_ONLINE)
			unit++;
		else {
			coord = sdev->pd_sdev_idx == local_dev;
			break;
		}
	}
	return coord;
}

static
int dix_cm_iter_rebalance_tgts_get(struct m0_dix_cm_iter     *iter,
				   struct m0_dix_layout_iter *liter,
				   struct m0_poolmach        *pm,
				   uint64_t                   local_device,
				   struct m0_buf             *key,
				   bool                      *is_coordinator,
				   uint64_t                 **tgts,
				   uint64_t                  *tgts_nr)
{
	uint64_t           unit;
	uint64_t           tgt;
	uint64_t           tgt_tmp;
	uint64_t           units_nr;
	uint64_t          *group_tgts;
	uint64_t           group_tgts_nr;
	uint32_t           N;
	uint32_t           K;
	struct m0_pooldev *sdev = NULL;
	int                rc = 0;

	*is_coordinator = false;
	*tgts = NULL;
	*tgts_nr = 0;

	m0_dix_layout_iter_reset(liter);

	group_tgts_nr = m0_dix_liter_W(liter);
	M0_ALLOC_ARR(group_tgts, group_tgts_nr);
	if (group_tgts == NULL)
		return M0_ERR(-ENOMEM);

	units_nr = group_tgts_nr;

	for (unit = 0; unit < units_nr; unit++) {
		m0_dix_layout_iter_next(liter, &tgt_tmp);
		group_tgts[unit] = tgt_tmp;
	}

	m0_dix_layout_iter_reset(liter);

	for (unit = 0; unit < units_nr; unit++) {
		m0_dix_layout_iter_next(liter, &tgt);
		M0_ASSERT(tgt < pm->pm_state->pst_nr_devices);
		sdev = &pm->pm_state->pst_devices_array[tgt];
		if (sdev->pd_sdev_idx == local_device)
			break;
	}
	M0_ASSERT(unit != units_nr);
	M0_ASSERT(sdev != NULL);
	M0_ASSERT(sdev->pd_sdev_idx == local_device);

	N = m0_dix_liter_N(liter);
	K = m0_dix_liter_K(liter);

	if (sdev->pd_state == M0_PNDS_ONLINE &&
	    m0_dix_liter_unit_classify(liter, unit) == M0_PUT_SPARE &&
	    pm->pm_state->pst_spare_usage_array[unit - N - K].
	    psu_device_index != POOL_PM_SPARE_SLOT_UNUSED) {
		rc = dix_cm_rebalance_tgts_get(liter, pm,
					       group_tgts, group_tgts_nr,
					       unit - N - K,
					       tgts, tgts_nr);
		M0_ASSERT(*tgts_nr == 0 || *tgts_nr == 1);
		if (rc == 0 && *tgts_nr > 0) {
			M0_ASSERT(*tgts != NULL);
			*is_coordinator = true;
		}
	}

	if (rc == 0)
		M0_LOG(M0_DEBUG, "Coordinator? %d, key %"PRIu64,
		       (int)*is_coordinator,
		       be64toh(*(uint64_t *)key->b_addr));
	m0_free(group_tgts);
	return M0_RC(rc);
}

static
int dix_cm_iter_repair_tgts_get(struct m0_dix_cm_iter     *iter,
				struct m0_dix_layout_iter *liter,
				struct m0_poolmach        *pm,
				uint64_t                   local_device,
				struct m0_buf             *key,
				bool                      *is_coordinator,
				uint64_t                 **tgts,
				uint64_t                  *tgts_nr)
{
	int rc = 0;

	*is_coordinator = dix_cm_is_repair_coordinator(liter, pm, local_device);
	M0_LOG(M0_DEBUG, "Coordinator? %d, key %"PRIu64, (int)*is_coordinator,
	       be64toh(*(uint64_t *)key->b_addr));

	if (*is_coordinator)
		rc = dix_cm_repair_tgts_get(liter, pm, tgts, tgts_nr);

	return M0_RC(rc);
}

static int dix_cm_iter_next_key(struct m0_dix_cm_iter *iter,
				struct m0_buf         *key,
				struct m0_buf         *val,
				bool                  *is_coordinator)
{
	struct m0_poolmach        *pm;
	struct m0_fid              dix_fid;
	struct m0_dix_layout       layout = {};
	struct m0_dix_layout_iter  layout_iter;
	uint64_t                   local_device;
	struct m0_dix_cm          *dix_cm;
	uint64_t                  *tgts = NULL;
	uint64_t                   tgts_nr = 0;
	int                        rc = 0;

	M0_ENTRY();

	dix_cm = container_of(iter, struct m0_dix_cm, dcm_it);
	m0_dix_fid_convert_cctg2dix(&iter->di_cctg_fid, &dix_fid);
	layout.dl_type = DIX_LTYPE_DESCR;
	layout.u.dl_desc = iter->di_ldesc;
	rc = dix_cm_layout_iter_init(&layout_iter, &dix_fid, &layout, dix_cm,
				     key);
	if (rc != 0)
		return M0_ERR(rc);

	pm = dix_cm_pm_get(dix_cm, &layout);
	local_device = m0_dix_fid_cctg_device_id(&iter->di_cctg_fid);

	if (M0_FI_ENABLED("print_parity_group")) {
		parity_group_print(&layout_iter,
				   pm, local_device, key);
	}
	if (M0_FI_ENABLED("print_spare_usage")) {
		spare_usage_print(&layout_iter, pm);
	}

	M0_ASSERT(M0_IN(dix_cm->dcm_type, (&dix_repair_dcmt,
					   &dix_rebalance_dcmt)));

	rc = (dix_cm->dcm_type == &dix_repair_dcmt ?
	      &dix_cm_iter_repair_tgts_get : &dix_cm_iter_rebalance_tgts_get)
		(iter, &layout_iter, pm, local_device, key, is_coordinator,
		 &tgts, &tgts_nr);

	if (rc == 0 && *is_coordinator && tgts_nr > 0) {
		M0_ASSERT(tgts != NULL);

		if (M0_FI_ENABLED("print_targets"))
			tgts_print(&layout_iter, tgts, tgts_nr, key);

		iter->di_tgts = tgts;
		iter->di_tgts_cur = 0;
		iter->di_tgts_nr = tgts_nr;

		rc = dix_cm_iter_buf_copy(&iter->di_key, key,
					  iter->di_cutoff) ?:
		     dix_cm_iter_buf_copy(&iter->di_val, val, iter->di_cutoff);
		if (rc != 0)
			m0_buf_free(&iter->di_key);
	}
	m0_dix_layout_iter_fini(&layout_iter);

	return M0_RC(rc);
}

static void dix_cm_iter_meta_unlock(struct m0_dix_cm_iter *iter)
{
	iter->di_meta_modified = false;
	m0_long_read_unlock(m0_ctg_lock(m0_ctg_meta()),
			    &iter->di_meta_lock_link);
}

static int dix_cm_iter_fom_tick(struct m0_fom *fom)
{
	struct m0_dix_cm_iter *iter = M0_AMB(iter, fom, di_fom);
	struct m0_dix_cm      *dix_cm = M0_AMB(dix_cm, iter, dcm_it);
	uint8_t                min_key = 0;
	struct m0_buf          kbuf = M0_BUF_INIT(sizeof min_key, &min_key);
	struct m0_buf          key = {};
	struct m0_buf          val = {};
	int                    phase = m0_fom_phase(fom);
	int                    result = M0_FSO_AGAIN;
	bool                   is_coordinator = false;
	bool                   is_same_key    = false;
	struct m0_be_tx       *tx;
	int                    rc;

	M0_ENTRY("fom %p, dix_cm %p, phase %d", fom, dix_cm, phase);
	switch (phase) {
	case DIX_ITER_INIT:
		dix_cm_iter_init(iter);
		m0_fom_phase_set(fom, DIX_ITER_STARTED);
		result = M0_FSO_WAIT;
		break;
	case DIX_ITER_STARTED:
		if (iter->di_stop)
			m0_fom_phase_set(fom, DIX_ITER_EOF);
		else
			result = M0_FOM_LONG_LOCK_RETURN(m0_long_read_lock(
						    m0_ctg_lock(m0_ctg_ctidx()),
						       &iter->di_meta_lock_link,
						       DIX_ITER_DEL_LOCK));
		break;
	case DIX_ITER_DEL_LOCK:
		result = M0_FOM_LONG_LOCK_RETURN(m0_long_write_lock(
						       m0_ctg_del_lock(),
						       &iter->di_del_lock_link,
						       DIX_ITER_CTIDX_START));
		break;
	case DIX_ITER_CTIDX_START:
	case DIX_ITER_CTIDX_REPOS:
		if (phase == DIX_ITER_CTIDX_REPOS) {
			m0_ctg_cursor_init(&iter->di_ctidx_op, m0_ctg_ctidx());
			kbuf = M0_BUF_INIT_PTR(&iter->di_cctg_fid);
		}
		result = m0_ctg_cursor_get(&iter->di_ctidx_op, &kbuf,
					   DIX_ITER_NEXT_CCTG);
		break;
	case DIX_ITER_CTIDX_NEXT:
		result = m0_ctg_cursor_next(&iter->di_ctidx_op,
					    DIX_ITER_NEXT_CCTG);
		break;
	case DIX_ITER_NEXT_CCTG:
		rc = m0_ctg_op_rc(&iter->di_ctidx_op);
		if (rc == 0) {
			struct m0_dix_layout *layout;

			m0_ctg_cursor_kv_get(&iter->di_ctidx_op, &key, &val);
			iter->di_cctg_fid = *(struct m0_fid *)key.b_addr;
			layout = (struct m0_dix_layout *)val.b_addr;
			M0_ASSERT(layout->dl_type == DIX_LTYPE_DESCR);
			iter->di_ldesc = layout->u.dl_desc;
			iter->di_cctg_processed_recs_nr = 0;
		}
		m0_long_read_unlock(m0_ctg_lock(m0_ctg_ctidx()),
				    &iter->di_meta_lock_link);
		if (rc == 0) {
			result = M0_FOM_LONG_LOCK_RETURN(m0_long_read_lock(
						m0_ctg_lock(m0_ctg_meta()),
						&iter->di_meta_lock_link,
						DIX_ITER_META_LOCK));
		} else if (rc == -ENOENT) {
			/* No more component catalogues, finish iterator. */
			result = dix_cm_iter_eof(iter);
		} else {
			result = dix_cm_iter_failure(iter, rc);
		}
		break;
	case DIX_ITER_META_LOCK:
		/* Lookup component catalogue in meta. */
		m0_ctg_op_init(&iter->di_ctg_op, fom, 0);
		result = m0_ctg_meta_lookup(&iter->di_ctg_op,
					    &iter->di_cctg_fid,
					    DIX_ITER_CCTG_LOOKUP);
		if (result < 0)
			m0_ctg_op_fini(&iter->di_ctg_op);
		break;
	case DIX_ITER_CCTG_LOOKUP:
		rc = m0_ctg_op_rc(&iter->di_ctg_op);
		if (rc == 0)
			iter->di_cctg = m0_ctg_meta_lookup_result(
							&iter->di_ctg_op);
		else
			m0_ctg_op_fini(&iter->di_ctg_op);
		if (rc == 0) {
			int next_state;

			if (m0_fid_eq(&iter->di_cctg_fid,
				      &iter->di_prev_cctg_fid)) {
				next_state = DIX_ITER_CCTG_CONT;
			} else {
				next_state = DIX_ITER_CCTG_START;
				m0_buf_free(&iter->di_prev_key);
				m0_ctg_op_fini(&iter->di_ctg_op);
			}

			result = M0_FOM_LONG_LOCK_RETURN(
				m0_long_lock(m0_ctg_lock(iter->di_cctg),
					     dix_cm->dcm_type !=
					     &dix_repair_dcmt,
					     &iter->di_lock_link, next_state));
			dix_cm_iter_meta_unlock(iter);
		} else if (rc == -ENOENT && iter->di_meta_modified) {
			/*
			 * Current component catalogue may be deleted by CAS
			 * service when iterator had neither catalogue-index
			 * lock nor meta lock. Go to the next one in this case.
			 */
			dix_cm_iter_meta_unlock(iter);
			result = M0_FOM_LONG_LOCK_RETURN(
				m0_long_read_lock(
					m0_ctg_lock(m0_ctg_ctidx()),
					&iter->di_meta_lock_link,
					DIX_ITER_CTIDX_REPOS));

		} else {
			dix_cm_iter_meta_unlock(iter);
			result = dix_cm_iter_failure(iter, rc);
		}
		break;
	case DIX_ITER_CCTG_START:
		m0_ctg_op_init(&iter->di_ctg_op, fom, COF_SLANT);
		m0_ctg_cursor_init(&iter->di_ctg_op, iter->di_cctg);
		result = m0_ctg_cursor_get(&iter->di_ctg_op, &kbuf,
					   DIX_ITER_NEXT_KEY);
		if (result < 0) {
			m0_ctg_cursor_fini(&iter->di_ctg_op);
			m0_ctg_op_fini(&iter->di_ctg_op);
		}
		break;
	case DIX_ITER_CCTG_CONT:
		m0_ctg_cursor_init(&iter->di_ctg_op, iter->di_cctg);
		result = m0_ctg_cursor_get(&iter->di_ctg_op,
					   &iter->di_prev_key,
					   DIX_ITER_NEXT_KEY);
		if (result < 0) {
			m0_ctg_cursor_fini(&iter->di_ctg_op);
			m0_ctg_op_fini(&iter->di_ctg_op);
		}
		break;
	case DIX_ITER_CCTG_CUR_NEXT:
		M0_PRE(iter->di_tgts == NULL);
		M0_PRE(iter->di_tgts_cur == 0);
		M0_PRE(iter->di_tgts_nr == 0);

		result = m0_ctg_cursor_next(&iter->di_ctg_op,
					    DIX_ITER_NEXT_KEY);
		if (result < 0) {
			m0_ctg_cursor_fini(&iter->di_ctg_op);
			m0_ctg_op_fini(&iter->di_ctg_op);
		}
		break;
	case DIX_ITER_NEXT_KEY:
		rc = m0_ctg_op_rc(&iter->di_ctg_op);
		if (rc == 0) {
			struct m0_buf tmp_key = {};
			struct m0_buf tmp_val = {};

			m0_ctg_cursor_kv_get(&iter->di_ctg_op,
					     &tmp_key,
					     &tmp_val);
			if (!m0_buf_eq(&iter->di_prev_key, &tmp_key)) {
				rc = dix_cm_iter_next_key(iter,
							  &tmp_key,
							  &tmp_val,
							  &is_coordinator);
				m0_buf_free(&iter->di_prev_key);
				M0_CNT_INC(iter->di_processed_recs_nr);
				M0_CNT_INC(iter->di_cctg_processed_recs_nr);
			} else
				is_same_key = true;
		}

		if (rc == 0 && (is_same_key || !is_coordinator ||
				iter->di_tgts_nr == 0)) {
			m0_fom_phase_set(fom, DIX_ITER_CCTG_CUR_NEXT);
			result = M0_FSO_AGAIN;
			break;
		}

		M0_LOG(M0_DEBUG, "%s CM, unlock",
		       dix_cm->dcm_type == &dix_repair_dcmt ? "Repair" :
		       "Re-balance");
		m0_long_unlock(m0_ctg_lock(iter->di_cctg), &iter->di_lock_link);

		if (rc == 0)
			result = dix_cm_iter_idle(iter);
		else if (rc == -ENOENT) {
			/*
			 * End of current catalogue is reached, lets go to the
			 * next one.
			 */
			m0_buf_free(&iter->di_prev_key);
			m0_ctg_cursor_fini(&iter->di_ctg_op);
			m0_ctg_op_fini(&iter->di_ctg_op);
			result = M0_FOM_LONG_LOCK_RETURN(m0_long_read_lock(
						  m0_ctg_lock(m0_ctg_ctidx()),
						  &iter->di_meta_lock_link,
						  DIX_ITER_CTIDX_NEXT));
		} else
			result = dix_cm_iter_failure(iter, rc);
		break;
	case DIX_ITER_IDLE_START:
		M0_ASSERT(iter->di_tgts_cur <= iter->di_tgts_nr);
		if (!iter->di_stop && iter->di_tgts_cur < iter->di_tgts_nr) {
			/*
			 * Not all targets are processed for the same key,
			 * stay on that key.
			 */
			m0_chan_broadcast_lock(&iter->di_completed);
			result = M0_FSO_WAIT;
		} else {
			if (dix_cm->dcm_type == &dix_rebalance_dcmt) {
				struct m0_be_tx_credit *accum =
					m0_fom_tx_credit(fom);
				struct m0_be_seg       *be_seg =
					m0_fom_reqh(fom)->rh_beseg;

				iter->di_ctg_del_op_rc = 0;
				m0_dtx_init(&fom->fo_tx,
					    be_seg->bs_domain,
					    &fom->fo_loc->fl_group);
				m0_ctg_delete_credit(iter->di_cctg,
						     iter->di_key.b_nob,
						     iter->di_val.b_nob,
						     accum);
				m0_dtx_open(&fom->fo_tx);
				tx = m0_fom_tx(fom);
				m0_fom_phase_set(fom, DIX_ITER_DEL_TX_OPENED);
				m0_fom_wait_on(fom, &tx->t_sm.sm_chan,
					       &fom->fo_cb);
				result = M0_FSO_WAIT;
			} else
				m0_fom_phase_set(fom, DIX_ITER_IDLE_FIN);
		}
		break;
	case DIX_ITER_DEL_TX_OPENED:
		M0_ASSERT(dix_cm->dcm_type == &dix_rebalance_dcmt);
		tx = m0_fom_tx(fom);
		if (m0_be_tx_state(tx) != M0_BTS_ACTIVE) {
			if (m0_be_tx_state(tx) == M0_BTS_FAILED)
				result = dix_cm_iter_dtx_failure(iter);
			else {
				m0_fom_wait_on(fom, &tx->t_sm.sm_chan,
					       &fom->fo_cb);
				result = M0_FSO_WAIT;
			}
		} else {
			m0_dtx_opened(&fom->fo_tx);
			m0_ctg_op_init(&iter->di_ctg_del_op, fom, 0);
			result = m0_ctg_delete(
				&iter->di_ctg_del_op,
				iter->di_cctg,
				&iter->di_key,
				DIX_ITER_DEL_TX_WAIT);
		}
		break;
	case DIX_ITER_DEL_TX_WAIT:
		M0_ASSERT(dix_cm->dcm_type == &dix_rebalance_dcmt);
		iter->di_ctg_del_op_rc =
			m0_ctg_op_rc(&iter->di_ctg_del_op);
		m0_ctg_op_fini(&iter->di_ctg_del_op);
		tx = m0_fom_tx(fom);
		if (m0_be_tx_state(tx) == M0_BTS_FAILED) {
			M0_ASSERT(iter->di_ctg_del_op_rc != 0);
			/* @todo: Can not finalise here in active state. */
			fom->fo_tx.tx_state = M0_DTX_DONE;
			result = dix_cm_iter_dtx_failure(iter);
		} else {
			m0_dtx_done(&fom->fo_tx);
			m0_fom_phase_set(fom, DIX_ITER_DEL_TX_DONE);
			m0_fom_wait_on(fom, &tx->t_sm.sm_chan,
				       &fom->fo_cb);
			result = M0_FSO_WAIT;
		}
		break;
	case DIX_ITER_DEL_TX_DONE:
		M0_ASSERT(dix_cm->dcm_type == &dix_rebalance_dcmt);
		tx = m0_fom_tx(fom);
		if (m0_be_tx_state(tx) != M0_BTS_DONE) {
			if (m0_be_tx_state(tx) == M0_BTS_FAILED)
				result = dix_cm_iter_dtx_failure(iter);
			else {
				m0_fom_wait_on(fom, &tx->t_sm.sm_chan,
					       &fom->fo_cb);
				result = M0_FSO_WAIT;
			}
		} else {
			dix_cm_iter_dtx_fini(iter);
			if (iter->di_ctg_del_op_rc != 0)
				result = dix_cm_iter_failure(
					iter, iter->di_ctg_del_op_rc);
			else
				m0_fom_phase_set(fom, DIX_ITER_IDLE_FIN);
		}
		break;
	case DIX_ITER_IDLE_FIN:
		m0_long_write_unlock(m0_ctg_del_lock(),
				     &iter->di_del_lock_link);
		iter->di_prev_key = iter->di_key;
		M0_SET0(&iter->di_key);
		m0_buf_free(&iter->di_val);
		dix_cm_iter_tgts_fini(iter);
		if (iter->di_stop)
			m0_fom_phase_set(fom, DIX_ITER_EOF);
		else
			result = M0_FOM_LONG_LOCK_RETURN(m0_long_write_lock(
						       m0_ctg_del_lock(),
						       &iter->di_del_lock_link,
						       DIX_ITER_CCTG_CHECK));
		break;
	case DIX_ITER_CCTG_CHECK:
		if (!iter->di_meta_modified) {
			m0_ctg_cursor_fini(&iter->di_ctg_op);
			if (dix_cm->dcm_type == &dix_repair_dcmt)
				result = M0_FOM_LONG_LOCK_RETURN(
					m0_long_read_lock(
						m0_ctg_lock(iter->di_cctg),
						&iter->di_lock_link,
						DIX_ITER_CCTG_CONT));
			else
				result = M0_FOM_LONG_LOCK_RETURN(
					m0_long_write_lock(
						m0_ctg_lock(iter->di_cctg),
						&iter->di_lock_link,
						DIX_ITER_CCTG_CONT));
		} else {
			iter->di_prev_cctg_fid = iter->di_cctg_fid;
			m0_ctg_cursor_fini(&iter->di_ctg_op);
			m0_ctg_cursor_fini(&iter->di_ctidx_op);
			result = M0_FOM_LONG_LOCK_RETURN(
				m0_long_read_lock(
					m0_ctg_lock(m0_ctg_ctidx()),
					&iter->di_meta_lock_link,
					DIX_ITER_CTIDX_REPOS));
		}
		break;
	case DIX_ITER_EOF:
	case DIX_ITER_FAILURE:
		if (iter->di_stop) {
			dix_cm_iter_fini(iter);
			m0_fom_phase_set(fom, DIX_ITER_FINAL);
		}
		result = M0_FSO_WAIT;
		break;
	case DIX_ITER_FINAL:
	default:
		M0_IMPOSSIBLE("Incorrect phase %d", phase);
	}
	if (result < 0)
		result = dix_cm_iter_failure(iter, result);
	return M0_RC(result);
}

static void dix_cm_iter_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
}

static const struct m0_fom_ops dix_cm_iter_fom_ops = {
	.fo_fini          = dix_cm_iter_fom_fini,
	.fo_tick          = dix_cm_iter_fom_tick,
	.fo_home_locality = dix_cm_iter_fom_locality
};

static const struct m0_fom_type_ops dix_cm_iter_fom_type_ops = {
	.fto_create = NULL
};

M0_INTERNAL void m0_dix_cm_iter_type_register(struct m0_dix_cm_type *dcmt)
{
	uint64_t fom_id;

	fom_id = dcmt->dct_base->ct_fom_id + 4;
	m0_fom_type_init(&dcmt->dct_iter_fomt, fom_id,
			 &dix_cm_iter_fom_type_ops, &dcmt->dct_base->ct_stype,
			 &dix_cm_iter_sm_conf);
}

M0_INTERNAL int m0_dix_cm_iter_start(struct m0_dix_cm_iter *iter,
				     struct m0_dix_cm_type *dcmt,
				     struct m0_reqh        *reqh,
				     m0_bcount_t            rpc_cutoff)
{
	M0_ENTRY("iter = %p", iter);
	M0_PRE(M0_IS0(iter));
	iter->di_cutoff = rpc_cutoff;
	m0_mutex_init(&iter->di_ch_guard);
	m0_chan_init(&iter->di_completed, &iter->di_ch_guard);
	m0_fom_init(&iter->di_fom, &dcmt->dct_iter_fomt, &dix_cm_iter_fom_ops,
		    NULL, NULL, reqh);
	m0_fom_queue(&iter->di_fom);
	m0_fom_timedwait(&iter->di_fom, M0_BITS(DIX_ITER_STARTED),
			 M0_TIME_NEVER);
	return M0_RC(0);
}

static void dix_cm_iter_next_ast_cb(struct m0_sm_group *grp,
				    struct m0_sm_ast   *ast)
{
	struct m0_dix_cm_iter *iter = M0_AMB(iter, ast, di_ast);

	M0_PRE(m0_fom_is_waiting(&iter->di_fom));
	M0_PRE(M0_IN(m0_fom_phase(&iter->di_fom), (DIX_ITER_STARTED,
						   DIX_ITER_IDLE_START)));
	m0_fom_wakeup(&iter->di_fom);
}

M0_INTERNAL void m0_dix_cm_iter_next(struct m0_dix_cm_iter *iter)
{
	iter->di_ast.sa_cb = dix_cm_iter_next_ast_cb;
	iter->di_ast.sa_datum = iter;
	m0_sm_ast_post(&iter->di_fom.fo_loc->fl_group, &iter->di_ast);
}

M0_INTERNAL int m0_dix_cm_iter_get(struct m0_dix_cm_iter *iter,
				   struct m0_buf         *key,
				   struct m0_buf         *val,
				   uint32_t              *sdev_id)
{
	struct m0_dix_cm   *dcm = container_of(iter, struct m0_dix_cm, dcm_it);
	struct m0_poolmach *pm;
	uint64_t            tgt;

	M0_PRE(M0_IN(m0_fom_phase(&iter->di_fom), (DIX_ITER_IDLE_START,
						   DIX_ITER_EOF,
						   DIX_ITER_FAILURE)));
	if (m0_fom_phase(&iter->di_fom) == DIX_ITER_EOF)
		return M0_ERR(-ENODATA);
	else if (m0_fom_phase(&iter->di_fom) == DIX_ITER_FAILURE)
		return M0_ERR(m0_fom_rc(&iter->di_fom));
	else {
		int rc;

		M0_ASSERT(iter->di_tgts_cur < iter->di_tgts_nr);

		rc = dix_cm_iter_buf_copy(key, &iter->di_key,
					  iter->di_cutoff) ?:
		     dix_cm_iter_buf_copy(val, &iter->di_val, iter->di_cutoff);
		if (rc != 0) {
			m0_buf_free(key);
		} else {
			struct m0_dix_layout  layout;
			struct m0_pooldev    *pd;

			layout.dl_type = DIX_LTYPE_DESCR;
			layout.u.dl_desc = iter->di_ldesc;
			pm = dix_cm_pm_get(dcm, &layout);
			M0_ASSERT(pm != NULL);
			tgt = iter->di_tgts[iter->di_tgts_cur];
			pd = &pm->pm_state->pst_devices_array[tgt];
			*sdev_id = pd->pd_sdev_idx;

			M0_CNT_INC(iter->di_tgts_cur);
		}

		return rc;
	}
}

static void dix_cm_iter_stop_ast_cb(struct m0_sm_group *grp,
				    struct m0_sm_ast   *ast)
{
	struct m0_dix_cm_iter *iter = M0_AMB(iter, ast, di_ast);

	iter->di_stop = true;
	/*
	 * Fom is waiting for sure, because AST and FOM state machine execute in
	 * the same state machine group. But fom executor behaviour can be
	 * changed in the future, so let's check fom state anyway.
	 */
	M0_ASSERT(m0_fom_is_waiting(&iter->di_fom));
	m0_fom_wakeup(&iter->di_fom);
}

M0_INTERNAL void m0_dix_cm_iter_stop(struct m0_dix_cm_iter *iter)
{
	M0_ENTRY("iter = %p", iter);
	iter->di_ast.sa_cb = dix_cm_iter_stop_ast_cb;
	iter->di_ast.sa_datum = iter;
	m0_sm_ast_post(&iter->di_fom.fo_loc->fl_group, &iter->di_ast);
	m0_fom_timedwait(&iter->di_fom, M0_BITS(DIX_ITER_FINAL), M0_TIME_NEVER);
	m0_chan_fini_lock(&iter->di_completed);
	m0_mutex_fini(&iter->di_ch_guard);
}

M0_INTERNAL
void m0_dix_cm_iter_cur_pos(struct m0_dix_cm_iter *iter,
			    struct m0_fid         *cctg_fid,
			    uint64_t              *cctg_proc_recs_nr)
{
	*cctg_fid = iter->di_cctg_fid;
	*cctg_proc_recs_nr = iter->di_cctg_processed_recs_nr;
}

M0_INTERNAL
void m0_dix_cm_iter_processed_num(struct m0_dix_cm_iter *iter,
				  uint64_t              *proc_recs_nr)
{
	*proc_recs_nr = iter->di_processed_recs_nr;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of DIXCM group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
