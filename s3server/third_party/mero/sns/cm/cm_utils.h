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
 * Original creation date: 03/08/2013
 */

#pragma once

#ifndef __MERO_SNS_CM_UTILS_H__
#define __MERO_SNS_CM_UTILS_H__

#include "layout/pdclust.h"

#include "sns/cm/cm.h"

/**
   @addtogroup SNSCM

   @{
*/

struct m0_cob_domain;
struct m0_cm;
struct m0_sns_cm;
struct m0_sns_cm_ag;

/**
 * Returns cob fid for the sa->sa_unit.
 * @see m0_fd_fwd_map
 */
M0_INTERNAL void
m0_sns_cm_unit2cobfid(struct m0_sns_cm_file_ctx *fctx,
		      const struct m0_pdclust_src_addr *sa,
		      struct m0_pdclust_tgt_addr *ta,
		      struct m0_fid *cfid_out);

M0_INTERNAL uint32_t m0_sns_cm_device_index_get(uint64_t group,
						uint64_t unit_number,
						struct m0_sns_cm_file_ctx *fctx);

M0_INTERNAL uint64_t m0_sns_cm_ag_unit2cobindex(struct m0_sns_cm_ag *sag,
						uint64_t unit);

/**
 * Searches for given cob_fid in the local cob domain.
 */
M0_INTERNAL int m0_sns_cm_cob_locate(struct m0_cob_domain *cdom,
				     const struct m0_fid *cob_fid);


/**
 * Calculates number of local data units for a given parity group.
 * This is invoked when new struct m0_sns_cm_ag instance is allocated, from
 * m0_cm_aggr_group_alloc(). This is done in context of sns copy machine data
 * iterator during the latter's ITPH_CP_SETUP phase. Thus we need not calculate
 * the new GOB layout and corresponding pdclust instance, instead used the ones
 * already calculated and save in the iterator, but we take GOB fid and group
 * number as the parameters to this function in-order to perform sanity checks.
 */
M0_INTERNAL uint64_t m0_sns_cm_ag_nr_local_units(struct m0_sns_cm *scm,
						 struct m0_sns_cm_file_ctx *fctx,
						 uint64_t group);


M0_INTERNAL uint64_t m0_sns_cm_ag_nr_global_units(const struct m0_sns_cm_ag *ag,
						  struct m0_pdclust_layout *pl);

M0_INTERNAL uint64_t m0_sns_cm_ag_size(const struct m0_pdclust_layout *pl);

M0_INTERNAL uint64_t m0_sns_cm_ag_nr_data_units(const struct m0_pdclust_layout *pl);

M0_INTERNAL uint64_t m0_sns_cm_ag_nr_parity_units(const struct m0_pdclust_layout *pl);

M0_INTERNAL uint64_t m0_sns_cm_ag_nr_spare_units(const struct m0_pdclust_layout *pl);

M0_INTERNAL int m0_sns_cm_ag_in_cp_units(const struct m0_sns_cm *scm,
					 const struct m0_cm_ag_id *id,
					 struct m0_sns_cm_file_ctx *fctx,
					 uint32_t *in_cp_nr,
					 uint32_t *in_units_nr,
					 struct m0_cm_proxy_in_count *pcount);

M0_INTERNAL bool m0_sns_cm_is_cob_repaired(struct m0_poolmach *pm,
					   uint32_t cob_index);

M0_INTERNAL bool m0_sns_cm_is_cob_repairing(struct m0_poolmach *pm,
					    uint32_t cob_index);

M0_INTERNAL bool m0_sns_cm_is_cob_rebalancing(struct m0_poolmach *pm,
					      uint32_t cob_index);

/**
 * Returns index of spare unit in the parity group, given the failure index
 * in the group.
 */
M0_INTERNAL uint64_t
m0_sns_cm_ag_spare_unit_nr(const struct m0_pdclust_layout *pl,
			   uint64_t fidx);

M0_INTERNAL bool m0_sns_cm_unit_is_spare(struct m0_sns_cm_file_ctx *fctx,
					 uint64_t group_number,
					 uint64_t spare_unit_number);

/**
 * Returns starting index of the unit in the aggregation group relevant to
 * the sns copy machine operation.
 * @see m0_sns_cm_op
 */
M0_INTERNAL uint64_t m0_sns_cm_ag_unit_start(const struct m0_sns_cm *scm,
					     const struct m0_pdclust_layout *pl);

/**
 * Returns end index of the unit in the aggregation group relevant to the
 * sns copy machine operation.
 * @see m0_sns_cm_op
 */
M0_INTERNAL uint64_t m0_sns_cm_ag_unit_end(const struct m0_sns_cm *scm,
					   const struct m0_pdclust_layout *pl);

/**
 * Calculates and returns the cobfid for the given group and the target unit
 * of the file (represented by the gobfid).
 */
M0_INTERNAL int m0_sns_cm_ag_tgt_unit2cob(struct m0_sns_cm_ag *sag,
					  uint64_t tgt_unit,
					  struct m0_fid *cobfid);

/**
 * Builds temporary layout and uses default file size for given gob_fid.
 * Only for UT purposes.
 */
M0_INTERNAL int
m0_sns_cm_ut_file_size_layout(struct m0_sns_cm_file_ctx *fctx);

/**
 * Gets endpoint address of the IO service which given cob is associated with.
 *
 * @note  m0_sns_cm_tgt_ep() pins a m0_conf_service object and returns its
 *        reference to caller via `hostage' parameter. The user should
 *        m0_confc_close() this object after using the endpoint string.
 */
M0_INTERNAL const char *m0_sns_cm_tgt_ep(const struct m0_cm *cm,
					 const struct m0_pool_version *pv,
					 const struct m0_fid *gfid,
					 struct m0_conf_obj **hostage);

M0_INTERNAL size_t m0_sns_cm_ag_unrepaired_units(const struct m0_sns_cm *scm,
						 struct m0_sns_cm_file_ctx *fctx,
						 uint64_t group,
						 struct m0_bitmap *fmap_out);

/**
 * Returns true if the given aggregation group corresponding to the id is
 * relevant. Thus if a node hosts the spare unit of the given aggregation group
 * and which is not the failed unit of the group, the group is considered for
 * the repair.
 */
M0_INTERNAL bool m0_sns_cm_ag_is_relevant(struct m0_sns_cm *scm,
					  struct m0_sns_cm_file_ctx *fctx,
					  const struct m0_cm_ag_id *id);

M0_INTERNAL bool
m0_sns_cm_ag_relevant_is_done(const struct m0_cm_aggr_group *ag,
			      uint64_t nr_cps_fini);

M0_INTERNAL bool m0_sns_cm_fid_is_valid(const struct m0_sns_cm *snscm,
				        const struct m0_fid *fid);

M0_INTERNAL struct m0_reqh *m0_sns_cm2reqh(const struct m0_sns_cm *snscm);

M0_INTERNAL bool m0_sns_cm_is_local_cob(const struct m0_cm *cm,
					const struct m0_pool_version *pv,
					const struct m0_fid *cob_fid);

M0_INTERNAL bool m0_sns_cm_disk_has_dirty_pver(struct m0_cm *cm,
					       struct m0_conf_drive *disk,
					       bool clear);
M0_INTERNAL bool m0_sns_cm_pver_is_dirty(struct m0_pool_version *pver);
M0_INTERNAL void m0_sns_cm_pver_dirty_set(struct m0_pool_version *pver);
M0_INTERNAL int m0_sns_cm_pool_ha_nvec_alloc(struct m0_pool *pool,
					     enum m0_pool_nd_state state,
					     struct m0_ha_nvec *nvec);
M0_INTERNAL enum m0_sns_cm_local_unit_type
m0_sns_cm_local_unit_type_get(struct m0_sns_cm_file_ctx *fctx, uint64_t group,
			      uint64_t unit);

/** @} endgroup SNSCM */

/* __MERO_SNS_CM_UTILS_H__ */

#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
