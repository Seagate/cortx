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
 * Original creation date: 18-Aug-2016
 */


/**
 * @addtogroup DIXCM
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "dix/cm/cp.h"
#include "dix/cm/cm.h"
#include "lib/trace.h"
#include "cm/repreb/cm.h"
#include "layout/pdclust.h"

M0_INTERNAL
int m0_dix_rebalance_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop *fop,
					 void (*fop_release)(struct m0_ref *),
					 uint64_t proxy_id, const char *local_ep,
					 const struct m0_cm_sw *sw,
					 const struct m0_cm_sw *out_interval);

M0_INTERNAL int m0_dix_cm_ag_alloc(struct m0_cm *cm,
				   const struct m0_cm_ag_id *id,
				   bool has_incoming,
				   struct m0_cm_aggr_group **out);

static int dix_rebalance_cm_prepare(struct m0_cm *cm)
{
	struct m0_dix_cm *dcm = cm2dix(cm);

	M0_ENTRY("cm: %p", cm);
	M0_PRE(M0_IN(dcm->dcm_op, (CM_OP_REBALANCE, CM_OP_REBALANCE_RESUME)));
	return 0;
}

static struct m0_cm_cp* dix_rebalance_cm_cp_alloc(struct m0_cm *cm)
{
	struct m0_cm_cp *cp;

	cp = m0_dix_cm_cp_alloc(cm);
	if (cp != NULL)
		cp->c_ops = &m0_dix_cm_rebalance_cp_ops;
	return cp;
}

static void dix_rebalance_cm_stop(struct m0_cm *cm)
{
	struct m0_dix_cm *dcm = cm2dix(cm);

	M0_ENTRY();
	M0_PRE(M0_IN(dcm->dcm_op, (CM_OP_REBALANCE, CM_OP_REBALANCE_RESUME)));
	m0_dix_cm_stop(cm);
	M0_LEAVE();
}

/** Copy machine operations. */
const struct m0_cm_ops dix_rebalance_ops = {
	.cmo_setup               = m0_dix_cm_setup,
	.cmo_prepare             = dix_rebalance_cm_prepare,
	.cmo_start               = m0_dix_cm_start,
	.cmo_ag_alloc            = m0_dix_cm_ag_alloc,
	.cmo_cp_alloc            = dix_rebalance_cm_cp_alloc,
	.cmo_data_next           = m0_dix_cm_data_next,
	.cmo_ag_next             = m0_dix_cm_ag_next,
	.cmo_get_space_for       = m0_dix_get_space_for,
	.cmo_sw_onwire_fop_setup = m0_dix_rebalance_sw_onwire_fop_setup,
	.cmo_is_peer             = m0_dix_is_peer,
	.cmo_stop                = dix_rebalance_cm_stop,
	.cmo_fini                = m0_dix_cm_fini
};

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
