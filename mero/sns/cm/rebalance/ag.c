/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 12/09/2012
 * Revision: Anup Barve <anup_barve@xyratex.com>
 * Revision date: 08/05/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"

#include "fid/fid.h"
#include "sns/parity_repair.h"

#include "cm/proxy.h"
#include "sns/cm/cm_utils.h"
#include "sns/cm/rebalance/ag.h"
#include "sns/cm/cp.h"
#include "sns/cm/cm.h"
#include "sns/cm/iter.h"
#include "sns/cm/file.h"

/**
   @addtogroup SNSCMAG

   @{
 */

M0_INTERNAL struct m0_sns_cm_rebalance_ag *
sag2rebalanceag(const struct m0_sns_cm_ag *sag)
{
	return container_of(sag, struct m0_sns_cm_rebalance_ag, rag_base);
}

static void rebalance_ag_fini(struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_rebalance_ag *rag;
	struct m0_sns_cm_ag           *sag;
	struct m0_sns_cm              *scm;
	struct m0_pdclust_layout      *pl;
	uint32_t                       nr_cp_bufs;
	uint32_t                       unused_cps;
	uint32_t                       nr_bufs;


	M0_ENTRY();
	M0_PRE(ag != NULL);

	sag = ag2snsag(ag);
	rag = sag2rebalanceag(sag);
	scm = cm2sns(ag->cag_cm);
	pl = m0_layout_to_pdl(sag->sag_fctx->sf_layout);
	nr_cp_bufs = m0_sns_cm_cp_buf_nr(&scm->sc_ibp.sb_bp,
					 m0_sns_cm_data_seg_nr(scm, pl));
	if (ag->cag_has_incoming) {
		unused_cps = sag->sag_not_coming * sag->sag_local_tgts_nr;
		nr_bufs = unused_cps * nr_cp_bufs;
		m0_sns_cm_cancel_reservation(scm, nr_bufs);
	}
	m0_sns_cm_ag_fini(sag);
	m0_free(rag);

	M0_LEAVE();
}

static bool rebalance_ag_can_fini(const struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_ag *sag = ag2snsag(ag);

	M0_PRE(ag != NULL);

	if (ag->cag_is_frozen || ag->cag_rc != 0)
		return ag->cag_ref == 0;
        if (ag->cag_has_incoming) {
		return ag->cag_freed_cp_nr == sag->sag_incoming_cp_nr +
					      ag->cag_cp_local_nr;
        } else
		return ag->cag_freed_cp_nr == ag->cag_cp_local_nr;
}

static const struct m0_cm_aggr_group_ops sns_cm_rebalance_ag_ops = {
	.cago_ag_can_fini       = rebalance_ag_can_fini,
	.cago_fini              = rebalance_ag_fini,
	.cago_local_cp_nr       = m0_sns_cm_ag_local_cp_nr,
	.cago_has_incoming_from = m0_sns_cm_ag_has_incoming_from,
	.cago_is_frozen_on      = m0_sns_cm_ag_is_frozen_on
};

M0_INTERNAL int m0_sns_cm_rebalance_ag_alloc(struct m0_cm *cm,
					     const struct m0_cm_ag_id *id,
					     bool has_incoming,
					     struct m0_cm_aggr_group **out)
{
	struct m0_sns_cm_rebalance_ag *rag;
	struct m0_sns_cm_ag           *sag;
	int                            rc = 0;

	M0_ENTRY("scm: %p, ag id:%p", cm, id);
	M0_PRE(cm != NULL && id != NULL && out != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	/* Allocate new aggregation group. */
	M0_ALLOC_PTR(rag);
	if (rag == NULL)
		return M0_ERR(-ENOMEM);
	sag = &rag->rag_base;
        rc = m0_sns_cm_ag_init(&rag->rag_base, cm, id, &sns_cm_rebalance_ag_ops,
                               has_incoming);
        if (rc != 0) {
                m0_free(rag);
                return M0_RC(rc);
        }

	sag->sag_local_tgts_nr = sag->sag_incoming_cp_nr;
	*out = &sag->sag_base;
	M0_LEAVE("ag: %p", &sag->sag_base);
	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_rebalance_ag_setup(struct m0_sns_cm_ag *sag,
					     struct m0_pdclust_layout *pl)
{
	return 0;
}

/** @} SNSCMAG */
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
