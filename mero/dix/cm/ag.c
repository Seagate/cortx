/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>
 * Original creation date: 25/08/2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"

#include "fid/fid.h"
#include "dix/cm/cp.h"
#include "dix/cm/cm.h"
#include "cm/proxy.h"

/**
   @addtogroup DIXCMAG

   @{
 */

static uint64_t dix_cp_ag_local_cp_nr(const struct m0_cm_aggr_group *ag)
{
	/* There is always one aggregation group per copy packet. */
	return 1;
}

static void dix_cp_ag_fini(struct m0_cm_aggr_group *ag)
{
	M0_PRE(ag != NULL);
	M0_ENTRY("ag %p", ag);
	m0_cm_aggr_group_fini(ag);
	m0_free(ag);
	M0_LEAVE();
}

static bool dix_cp_ag_can_fini(const struct m0_cm_aggr_group *ag)
{
	return true;
}

static bool dix_cm_ag_is_frozen_on(struct m0_cm_aggr_group *ag,
				   struct m0_cm_proxy *pxy)
{
	/** @todo What to return here? */
	return false;
}

static const struct m0_cm_aggr_group_ops dix_cm_ag_ops = {
	.cago_local_cp_nr  = &dix_cp_ag_local_cp_nr,
	.cago_fini         = &dix_cp_ag_fini,
	.cago_ag_can_fini  = &dix_cp_ag_can_fini,
	.cago_is_frozen_on = &dix_cm_ag_is_frozen_on
};

/**
 * Allocates and initialises DIX CM aggregation group.
 *
 * @param[in]  cm           Base copy machine.
 * @param[in]  id           Aggregation group ID.
 * @param[in]  has_incoming Shows whether incoming copy packets for this
 *                          aggregation group expected.
 * @param[out] out          Allocated an initialised aggregation group.
 *
 * @ret 0 on success or -ENOMEM.
 *
 * @see m0_cm_aggr_group_init()
 */
M0_INTERNAL int m0_dix_cm_ag_alloc(struct m0_cm *cm,
				   const struct m0_cm_ag_id *id,
				   bool has_incoming,
				   struct m0_cm_aggr_group **out)
{
	struct m0_cm_aggr_group *ag = NULL;

	M0_ENTRY("cm: %p, ag id:%p", cm, id);
	M0_PRE(cm != NULL && id != NULL && out != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	*out = NULL;

	/* Allocate new aggregation group. */
	M0_ALLOC_PTR(ag);
	if (ag == NULL)
		return M0_ERR(-ENOMEM);

	m0_cm_aggr_group_init(ag, cm, id, has_incoming, &dix_cm_ag_ops);
	*out = ag;

	M0_LEAVE("ag: %p", ag);
	return M0_RC(0);
}

/** @} DIXCMAG */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
