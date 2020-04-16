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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 04/16/2012
 */

#pragma once

#ifndef __MERO_SNS_CM_AG_H__
#define __MERO_SNS_CM_AG_H__

#include "layout/pdclust.h"
#include "cob/ns_iter.h"

#include "cm/ag.h"
#include "sns/cm/cp.h"
#include "cm/proxy.h"

/**
   @defgroup SNSCMAG SNS copy machine aggregation group
   @ingroup SNSCM

   @{
 */

struct m0_sns_cm;

struct m0_sns_cm_ag {
	/** Base aggregation group. */
	struct m0_cm_aggr_group          sag_base;

	struct m0_sns_cm_file_ctx       *sag_fctx;

	/** Total number of failure units in this aggregation group. */
	uint32_t                         sag_fnr;

	/**
	 * Accounts for number for incoming copy packets for this aggregation
	 * group per struct m0_cm_proxy.
	 */
	struct m0_cm_proxy_in_count      sag_proxy_in_count;

	/** Number of local copy packets created by data iterator. */
	uint32_t                         sag_cp_created_nr;

	/**
	 * Actual number of incoming data/parity units for this aggregation
	 * group.
	 */
	uint32_t                         sag_incoming_units_nr;

	/**
	 * Number incoming copy packets expected for this aggregation
	 * group.
	 */
	uint32_t                         sag_incoming_cp_nr;

	/** Number of outgoing copy packets. */
	uint32_t                         sag_outgoing_nr;

	/** Number of local targets/spares of an aggregation group to be used.*/
	uint32_t                         sag_local_tgts_nr;

	/** Number of incoming copy packets that will no more arrive. */
	uint32_t                         sag_not_coming;

	/** Bitmap of failed units in the aggregation group. */
	struct m0_bitmap                 sag_fmap;
};

/**
 * Incoming aggregation groups iterator.
 * This is used to advance sliding window during sns repair or rebalance.
 */
struct m0_sns_cm_ag_iter {
	/** Iterator state machine. */
	struct m0_sm                 ai_sm;
	/** File of which the parity groups are being iterated. */
	struct m0_fid                ai_fid;
	/** Current incoming aggregation group id. */
	struct m0_cm_ag_id           ai_id_curr;
	/** Next incoming agregation group id. */
	struct m0_cm_ag_id           ai_id_next;
	/** Total number of aggregation groups to be iterated for given file. */
	uint64_t                     ai_group_last;
	/** File context corresponding to file being iterated. */
	struct m0_sns_cm_file_ctx   *ai_fctx;

	struct m0_poolmach          *ai_pm;
};

M0_INTERNAL int m0_sns_cm_ag__next(struct m0_sns_cm *scm,
				   const struct m0_cm_ag_id *id_curr,
				   struct m0_cm_ag_id *id_next);
M0_INTERNAL int m0_sns_cm_ag_iter_init(struct m0_sns_cm_ag_iter *ai);
M0_INTERNAL void m0_sns_cm_ag_iter_fini(struct m0_sns_cm_ag_iter *ai);

/**
 * Initialises given sns specific generic aggregation group.
 * Invokes m0_cm_aggr_group_init().
 */
M0_INTERNAL int m0_sns_cm_ag_init(struct m0_sns_cm_ag *sag,
				  struct m0_cm *cm,
				  const struct m0_cm_ag_id *id,
				  const struct m0_cm_aggr_group_ops *ag_ops,
				  bool has_incoming);

/**
 * Finalises given sns specific generic aggregation group.
 * Invokes m0_cm_aggr_group_fini().
 */
M0_INTERNAL void m0_sns_cm_ag_fini(struct m0_sns_cm_ag *sag);

/**
 * Returns number of copy packets corresponding to the units local to the
 * given node for an aggregation group.
 */
M0_INTERNAL uint64_t m0_sns_cm_ag_local_cp_nr(const struct m0_cm_aggr_group *ag);
M0_INTERNAL bool m0_sns_cm_ag_has_incoming_from(struct m0_cm_aggr_group *ag,
						struct m0_cm_proxy *proxy);

/**
 * Returns true iff aggregation group cannot progress.
 * This can happen during sns repair/rebalance quiesce or abort operation.
 * An aggregation group can be frozen in following cases,
 * case 1: The given proxy @pxy has completed (i.e. there will be no more
 *         outgoing copy packets created by the proxy) and there are still
 *         incoming copy packets expected from the proxy.
 * case 2: There are incoming aggregation groups which have received all
 *         the relevant copy packets from the remote replicas but pump
 *         fom is already stopped so there will be no more local copy
 *         packets created if any.
 */
M0_INTERNAL bool m0_sns_cm_ag_is_frozen_on(struct m0_cm_aggr_group *ag,
					   struct m0_cm_proxy *pxy);

M0_INTERNAL struct m0_sns_cm_ag *ag2snsag(const struct m0_cm_aggr_group *ag);

M0_INTERNAL void agid2fid(const struct m0_cm_ag_id *id,
			  struct m0_fid *fid);

M0_INTERNAL uint64_t agid2group(const struct m0_cm_ag_id *id);

M0_INTERNAL void m0_sns_cm_ag_agid_setup(const struct m0_fid *gob_fid,
					 uint64_t group,
					 struct m0_cm_ag_id *agid);

M0_INTERNAL struct m0_cm *snsag2cm(const struct m0_sns_cm_ag *sag);

M0_INTERNAL bool m0_sns_cm_ag_has_data(struct m0_sns_cm_file_ctx *fctx,
				       uint64_t group);

/** @} SNSCMAG */

#endif /* __MERO_SNS_CM_AG_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
