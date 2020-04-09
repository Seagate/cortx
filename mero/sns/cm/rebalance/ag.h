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

#ifndef __MERO_SNS_CM_REBALANCE_AG_H__
#define __MERO_SNS_CM_REBALANCE_AG_H__

#include "sns/cm/ag.h"

/**
   @defgroup SNSCMAG SNS copy machine aggregation group
   @ingroup SNSCM

   @{
 */

struct m0_sns_cm_rebalance_ag {
	/** Base aggregation group. */
	struct m0_sns_cm_ag  rag_base;
};


/**
 * Allocates and initializes aggregation group for the given m0_cm_ag_id.
 * Caller is responsible to lock the copy machine before calling this function.
 * @pre m0_cm_is_locked(cm) == true
 */
M0_INTERNAL int m0_sns_cm_rebalance_ag_alloc(struct m0_cm *cm,
					     const struct m0_cm_ag_id *id,
					     bool has_incoming,
					     struct m0_cm_aggr_group **out);

/** @} SNSCMAG */

#endif /* __MERO_SNS_CM_REBALANCE_AG_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
