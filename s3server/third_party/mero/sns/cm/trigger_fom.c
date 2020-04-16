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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"

#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"

#include "cm/cm.h"
#include "cm/repreb/trigger_fom.h"
#include "cm/repreb/trigger_fop.h"
#include "sns/cm/trigger_fop.h"
#include "sns/cm/cm.h"
#include "sns/cm/repair/ag.h"

/*
 * Implements a simplistic sns repair trigger FOM for corresponding trigger FOP.
 * This is solely for testing purpose and a separate trigger FOP/FOM will be
 * implemented later, which would be similar to this one.
 */


static int sns_trigger_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh);
static struct m0_fop_type *sns_fop_type(uint32_t op);
static uint64_t sns_progress(struct m0_fom *fom, bool reinit_counter);
static void sns_prepare(struct m0_fom *fom);

static const struct m0_fom_trigger_ops sns_trigger_ops = {
	.fto_type     = sns_fop_type,
	.fto_progress = sns_progress,
	.fto_prepare  = sns_prepare
};

const struct m0_fom_type_ops m0_sns_trigger_fom_type_ops = {
	.fto_create = sns_trigger_fom_create,
};

static int sns_trigger_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	struct m0_trigger_fom *tfom;
	int                    rc;

	M0_ALLOC_PTR(tfom);
	if (tfom == NULL)
		return M0_ERR(-ENOMEM);
	tfom->tf_ops = &sns_trigger_ops;
	rc = m0_trigger_fom_create(tfom, fop, reqh);
	if (rc != 0) {
		m0_free(tfom);
		return M0_ERR(rc);
	}
	*out = &tfom->tf_fom;
	return 0;
}

static struct m0_fop_type *sns_fop_type(uint32_t op)
{
	struct m0_fop_type *sns_fop_type[] = {
		[M0_SNS_REPAIR_TRIGGER_OPCODE] =
			&m0_sns_repair_trigger_rep_fopt,
		[M0_SNS_REPAIR_QUIESCE_OPCODE] =
			&m0_sns_repair_quiesce_rep_fopt,
		[M0_SNS_REPAIR_STATUS_OPCODE] =
			&m0_sns_repair_status_rep_fopt,
		[M0_SNS_REBALANCE_TRIGGER_OPCODE] =
			&m0_sns_rebalance_trigger_rep_fopt,
		[M0_SNS_REBALANCE_QUIESCE_OPCODE] =
			&m0_sns_rebalance_quiesce_rep_fopt,
		[M0_SNS_REBALANCE_STATUS_OPCODE] =
			&m0_sns_rebalance_status_rep_fopt,
		[M0_SNS_REPAIR_ABORT_OPCODE] =
			&m0_sns_repair_abort_rep_fopt,
		[M0_SNS_REBALANCE_ABORT_OPCODE] =
			&m0_sns_rebalance_abort_rep_fopt,
	};
	M0_ASSERT(IS_IN_ARRAY(op, sns_fop_type));
	return sns_fop_type[op];
}

static void print__ag(const struct m0_tl_descr *descr, const struct m0_tl *head)
{
	struct m0_cm_aggr_group *ag;

	m0_tlist_for(descr, head, ag) {
		M0_LOG(M0_DEBUG, M0_AG_F, M0_AG_P(&ag->cag_id));
		M0_LOG(M0_DEBUG, " freed=%u local_cp=%u transformed=%u ref=%u",
			(unsigned)ag->cag_freed_cp_nr, (unsigned)ag->cag_cp_local_nr,
			(unsigned)ag->cag_transformed_cp_nr, (unsigned)ag->cag_ref);
	} m0_tlist_endfor;
}

static void print_ag(struct m0_cm *cm)
{
	print__ag(&aggr_grps_in_tl, &cm->cm_aggr_grps_in);
	print__ag(&aggr_grps_out_tl, &cm->cm_aggr_grps_out);
}

static uint64_t sns_progress(struct m0_fom *fom, bool reinit_counter)
{
	static uint64_t  progress = 0;
	struct m0_cm    *cm       = container_of(fom->fo_service,
						 struct m0_cm, cm_service);

	M0_PRE(cm != NULL);
	if (reinit_counter)
		progress = 0;
	/* For debugging purpose. */
	if (progress > 10)
		print_ag(cm);
	return progress++;
}

static void sns_prepare(struct m0_fom *fom)
{
	struct m0_cm       *cm   = M0_AMB(cm, fom->fo_service, cm_service);
	struct m0_sns_cm   *scm  = cm2sns(cm);
	struct trigger_fop *treq = m0_fop_data(fom->fo_fop);

	M0_PRE(scm != NULL);
	M0_PRE(treq != NULL);
	M0_PRE(M0_IN(treq->op, (CM_OP_REPAIR, CM_OP_REPAIR_RESUME,
				CM_OP_REBALANCE, CM_OP_REBALANCE_RESUME)));

	if (M0_IN(treq->op, (CM_OP_REPAIR, CM_OP_REBALANCE))) {
		cm->cm_reset = true;
		scm->sc_op = treq->op;
	} else
		scm->sc_op = treq->op == CM_OP_REPAIR_RESUME ? CM_OP_REPAIR :
			     CM_OP_REBALANCE;
}

#undef M0_TRACE_SUBSYSTEM
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
