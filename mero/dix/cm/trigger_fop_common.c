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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 30-Aug-2016
 */

/**
 * @addtogroup dixcm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "lib/trace.h"
#include "fop/fop.h"
#include "fop/fop_item_type.h"
#include "cm/repreb/cm.h"

struct m0_fop_type m0_dix_rebalance_trigger_fopt;
struct m0_fop_type m0_dix_rebalance_trigger_rep_fopt;
struct m0_fop_type m0_dix_rebalance_quiesce_fopt;
struct m0_fop_type m0_dix_rebalance_quiesce_rep_fopt;
struct m0_fop_type m0_dix_rebalance_status_fopt;
struct m0_fop_type m0_dix_rebalance_status_rep_fopt;
struct m0_fop_type m0_dix_rebalance_abort_fopt;
struct m0_fop_type m0_dix_rebalance_abort_rep_fopt;

struct m0_fop_type m0_dix_repair_trigger_fopt;
struct m0_fop_type m0_dix_repair_quiesce_fopt;
struct m0_fop_type m0_dix_repair_status_fopt;
struct m0_fop_type m0_dix_repair_abort_fopt;
struct m0_fop_type m0_dix_repair_trigger_rep_fopt;
struct m0_fop_type m0_dix_repair_quiesce_rep_fopt;
struct m0_fop_type m0_dix_repair_status_rep_fopt;
struct m0_fop_type m0_dix_repair_abort_rep_fopt;

M0_INTERNAL int m0_dix_cm_trigger_fop_alloc(struct m0_rpc_machine  *mach,
					    uint32_t                op,
					    struct m0_fop         **fop)
{
	static struct m0_fop_type *dix_fop_type[] = {
		[CM_OP_REPAIR]           = &m0_dix_repair_trigger_fopt,
		[CM_OP_REPAIR_QUIESCE]   = &m0_dix_repair_quiesce_fopt,
		[CM_OP_REBALANCE]        = &m0_dix_rebalance_trigger_fopt,
		[CM_OP_REBALANCE_QUIESCE]= &m0_dix_rebalance_quiesce_fopt,
		[CM_OP_REPAIR_RESUME]    = &m0_dix_repair_trigger_fopt,
		[CM_OP_REBALANCE_RESUME] = &m0_dix_rebalance_trigger_fopt,
		[CM_OP_REPAIR_STATUS]    = &m0_dix_repair_status_fopt,
		[CM_OP_REBALANCE_STATUS] = &m0_dix_rebalance_status_fopt,
		[CM_OP_REPAIR_ABORT]     = &m0_dix_repair_abort_fopt,
		[CM_OP_REBALANCE_ABORT]  = &m0_dix_rebalance_abort_fopt,
	};
	M0_ENTRY();
	M0_PRE(IS_IN_ARRAY(op, dix_fop_type));

	*fop = m0_fop_alloc(dix_fop_type[op], NULL, mach);
	return *fop == NULL ? M0_ERR(-ENOMEM) : M0_RC(0);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of dixcm group */

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
