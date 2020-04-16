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

#pragma once

#ifndef __MERO_DIX_CM_TRIGGER_FOP_H__
#define __MERO_DIX_CM_TRIGGER_FOP_H__

/**
 * @defgroup dixcm
 *
 * @{
 */
struct m0_rpc_machine;

extern struct m0_fop_type m0_dix_rebalance_trigger_fopt;
extern struct m0_fop_type m0_dix_rebalance_trigger_rep_fopt;
extern struct m0_fop_type m0_dix_rebalance_quiesce_fopt;
extern struct m0_fop_type m0_dix_rebalance_quiesce_rep_fopt;
extern struct m0_fop_type m0_dix_rebalance_status_fopt;
extern struct m0_fop_type m0_dix_rebalance_status_rep_fopt;
extern struct m0_fop_type m0_dix_rebalance_abort_fopt;
extern struct m0_fop_type m0_dix_rebalance_abort_rep_fopt;

extern struct m0_fop_type m0_dix_repair_trigger_fopt;
extern struct m0_fop_type m0_dix_repair_quiesce_fopt;
extern struct m0_fop_type m0_dix_repair_status_fopt;
extern struct m0_fop_type m0_dix_repair_abort_fopt;
extern struct m0_fop_type m0_dix_repair_trigger_rep_fopt;
extern struct m0_fop_type m0_dix_repair_quiesce_rep_fopt;
extern struct m0_fop_type m0_dix_repair_status_rep_fopt;
extern struct m0_fop_type m0_dix_repair_abort_rep_fopt;


/**
 * Allocates trigger FOP for given operation @op.
 *
 * @param[in]  mach RPC machine to handle FOP RPC item.
 * @param[in]  op   Repair/re-balance control operation.
 * @param[out] fop  Newly created FOP.
 *
 * @ret 0 on success or -ENOMEM.
 */
M0_INTERNAL int m0_dix_cm_trigger_fop_alloc(struct m0_rpc_machine  *mach,
					    uint32_t                op,
					    struct m0_fop         **fop);
/** @} end of dixcm group */
#endif /* __MERO_DIX_CM_TRIGGER_FOP_H__ */

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
