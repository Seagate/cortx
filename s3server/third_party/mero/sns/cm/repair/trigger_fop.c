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

#include "fop/fop.h"
#include "fop/fop_item_type.h"

#include "sns/cm/cm.h"
#include "sns/cm/trigger_fop.h"
#include "cm/repreb/trigger_fop.h"
#include "cm/repreb/trigger_fop_xc.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

/*
 * Implements a simplistic sns repair trigger FOM for corresponding trigger FOP.
 * This is solely for testing purpose and a separate trigger FOP/FOM will be
 * implemented later, which would be similar to this one.
 */

extern struct m0_cm_type sns_repair_cmt;
extern const struct m0_fom_type_ops m0_sns_trigger_fom_type_ops;

M0_INTERNAL void m0_sns_cm_repair_trigger_fop_fini(void)
{
	m0_cm_trigger_fop_fini(&m0_sns_repair_trigger_fopt);
	m0_cm_trigger_fop_fini(&m0_sns_repair_trigger_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_sns_repair_quiesce_fopt);
	m0_cm_trigger_fop_fini(&m0_sns_repair_quiesce_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_sns_repair_status_fopt);
	m0_cm_trigger_fop_fini(&m0_sns_repair_status_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_sns_repair_abort_fopt);
	m0_cm_trigger_fop_fini(&m0_sns_repair_abort_rep_fopt);
}

M0_INTERNAL void m0_sns_cm_repair_trigger_fop_init(void)
{
	m0_cm_trigger_fop_init(&m0_sns_repair_trigger_fopt,
			       M0_SNS_REPAIR_TRIGGER_OPCODE,
			       "sns repair trigger",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_sns_repair_trigger_rep_fopt,
			       M0_SNS_REPAIR_TRIGGER_REP_OPCODE,
			       "sns repair trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_sns_repair_quiesce_fopt,
			       M0_SNS_REPAIR_QUIESCE_OPCODE,
			       "sns repair quiesce trigger",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_sns_repair_quiesce_rep_fopt,
			       M0_SNS_REPAIR_QUIESCE_REP_OPCODE,
			       "sns repair quiesce trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_sns_repair_status_fopt,
			       M0_SNS_REPAIR_STATUS_OPCODE,
			       "sns repair status",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_sns_repair_status_rep_fopt,
			       M0_SNS_REPAIR_STATUS_REP_OPCODE,
			       "sns repair status reply",
			       m0_status_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_sns_repair_abort_fopt,
			       M0_SNS_REPAIR_ABORT_OPCODE,
			       "sns repair abort",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_sns_repair_abort_rep_fopt,
			       M0_SNS_REPAIR_ABORT_REP_OPCODE,
			       "sns repair abort reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &sns_repair_cmt,
			       &m0_sns_trigger_fom_type_ops);
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
