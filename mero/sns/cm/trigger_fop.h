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
 * Original creation date: 09/11/2011
 */

#pragma once

#ifndef __MERO_SNS_CM_TRIGGER_FOP_H__
#define __MERO_SNS_CM_TRIGGER_FOP_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "rpc/rpc_opcodes.h"

#include "cm/cm.h"

extern struct m0_fop_type m0_sns_repair_trigger_fopt;
extern struct m0_fop_type m0_sns_repair_quiesce_fopt;
extern struct m0_fop_type m0_sns_repair_status_fopt;
extern struct m0_fop_type m0_sns_rebalance_trigger_fopt;
extern struct m0_fop_type m0_sns_rebalance_quiesce_fopt;
extern struct m0_fop_type m0_sns_rebalance_status_fopt;
extern struct m0_fop_type m0_sns_repair_abort_fopt;
extern struct m0_fop_type m0_sns_rebalance_abort_fopt;

extern struct m0_fop_type m0_sns_repair_trigger_rep_fopt;
extern struct m0_fop_type m0_sns_repair_quiesce_rep_fopt;
extern struct m0_fop_type m0_sns_repair_status_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_trigger_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_quiesce_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_status_rep_fopt;
extern struct m0_fop_type m0_sns_repair_abort_rep_fopt;
extern struct m0_fop_type m0_sns_rebalance_abort_rep_fopt;


M0_INTERNAL int m0_sns_cm_trigger_fop_alloc(struct m0_rpc_machine  *mach,
					    uint32_t                op,
					    struct m0_fop         **fop);
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
