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

#ifndef __MERO_SNS_CM_REPAIR_AG_H__
#define __MERO_SNS_CM_REPAIR_AG_H__


#include "sns/cm/ag.h"
#include "pool/pool_machine.h"

/**
   @defgroup SNSCMAG SNS copy machine aggregation group
   @ingroup SNSCM

   @{
 */

/**
 * Represents a failure context corresponding to an aggregation group.
 * This is populated on creation of the aggregation group.
 */
struct m0_sns_cm_repair_ag_failure_ctx {
	/** Accumulator copy packet for this failure context. */
	struct m0_sns_cm_cp          fc_tgt_acc_cp;

	/** Index of the failed unit in aggregation group. */
	uint32_t                     fc_failed_idx;

	/**
	 * Index of target unit corresponding to the failed unit in aggregation
	 * group.
	 */
	uint32_t                     fc_tgt_idx;

	/*
	 * cob fid containing the target unit for the aggregation
	 * group.
	 */
	struct m0_fid                fc_tgt_cobfid;

	/** Target unit offset within the cob identified by tgt_cobfid. */
	uint64_t                     fc_tgt_cob_index;

	/**
	 * True, if the copy packet fom corresponding to this accumulator is
	 * in-progress.
	 */
	bool                         fc_is_active;

	/** True, if this accumulator is in use. */
	bool                         fc_is_inuse;
};

struct m0_sns_cm_repair_ag {
	/** Base aggregation group. */
	struct m0_sns_cm_ag                     rag_base;

	/**
	 * Number of accumulator copy packets finalised.
	 * This should be equal to sag_fnr.
	 */
	uint32_t                                rag_acc_freed;

	/** Number of accumulators actually in use. */
	uint32_t                                rag_acc_inuse_nr;

	/**
	 * Aggregation group failure context.
	 * Number of failure contexts are equivalent to number of failures in
	 * the aggregation group, i.e. m0_sns_cm_ag::sag_fnr.
	 */
	struct m0_sns_cm_repair_ag_failure_ctx *rag_fc;

	/** Parity math context required for incremental recovery algorithm. */
	struct m0_parity_math                   rag_math;

	/** Incremental recovery context. */
	struct m0_sns_ir                        rag_ir;
};


/**
 * Allocates and initializes aggregation group for the given m0_cm_ag_id.
 * Every sns copy machine aggregation group maintains accumulator copy packets,
 * equivalent to the number of failed units in the aggregation group. During
 * initialisation, the buffers are acquired for the accumulator copy packets
 * from the copy machine buffer pool.
 * Caller is responsible to lock the copy machine before calling this function.
 * @pre m0_cm_is_locked(cm) == true
 */
M0_INTERNAL int m0_sns_cm_repair_ag_alloc(struct m0_cm *cm,
					  const struct m0_cm_ag_id *id,
					  bool has_incoming,
					  struct m0_cm_aggr_group **out);

/*
 * Configures accumulator copy packet, acquires buffer for accumulator copy
 * packet.
 * Increments struct m0_cm_aggr_group::cag_cp_local_nr for newly created
 * accumulator copy packets, so that aggregation group is not finalised before
 * the finalisation of accumulator copy packets.
 *
 * @see m0_sns_cm_acc_cp_setup()
 */
M0_INTERNAL int m0_sns_cm_repair_ag_setup(struct m0_sns_cm_ag *ag,
					  struct m0_pdclust_layout *pl);

/**
 * Returns true if all the local copy packets are transformed in the accumulator
 * copy packet.
 * @see struct m0_sns_cm_repair_ag_failure_ctx::fc_tgt_acc_cp
 */
M0_INTERNAL bool m0_sns_cm_ag_acc_is_full_with(const struct m0_cm_cp *acc,
					       uint64_t nr_cps);

/**
 * Calculates number of buffers required for all the incoming copy packets.
 */
M0_INTERNAL int64_t m0_sns_cm_repair_ag_inbufs(struct m0_sns_cm *scm,
					       struct m0_sns_cm_file_ctx *fctx,
					       const struct m0_cm_ag_id *id);

M0_INTERNAL struct m0_sns_cm_repair_ag *
sag2repairag(const struct m0_sns_cm_ag *sag);

/** @} SNSCMAG */

#endif /* __MERO_SNS_CM_REPAIR_AG_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
