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
 * Original creation date: 16-Aug-2016
 */

#pragma once

#ifndef __MERO_CM_REPREB_CM_H__
#define __MERO_CM_REPREB_CM_H__

/**
 * @defgroup XXX
 *
 * @{
 */

/**
 * Operation that copy machine is carrying out.
 */
enum m0_cm_op {
	CM_OP_INVALID           = 0,
	CM_OP_REPAIR,
	CM_OP_REBALANCE,
	CM_OP_REPAIR_QUIESCE,
	CM_OP_REBALANCE_QUIESCE,
	CM_OP_REPAIR_RESUME,
	CM_OP_REBALANCE_RESUME,
	CM_OP_REPAIR_STATUS,
	CM_OP_REBALANCE_STATUS,
	CM_OP_REPAIR_ABORT,
	CM_OP_REBALANCE_ABORT
};

/**
 * Repair/re-balance copy machine status
 */
enum m0_cm_status {
	CM_STATUS_INVALID = 0,
	CM_STATUS_IDLE    = 1,
	CM_STATUS_STARTED = 2,
	CM_STATUS_FAILED  = 3,
	CM_STATUS_PAUSED  = 4,
	CM_STATUS_NR,
};

/** @} end of XXX group */
#endif /* __MERO_CM_REPREB_CM_H__ */

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
