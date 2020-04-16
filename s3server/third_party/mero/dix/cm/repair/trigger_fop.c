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
 * Original creation date: 17-Aug-2016
 */


/**
 * @addtogroup DIXCM
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "cm/repreb/trigger_fop.h"
#include "cm/repreb/trigger_fop_xc.h"
#include "cm/repreb/trigger_fom.h"
#include "dix/cm/cm.h"
#include "dix/cm/trigger_fom.h"
#include "dix/cm/trigger_fop.h"
#include "rpc/item.h"

/**
 * Finalises start, quiesce, status, abort repair trigger FOP and
 * corresponding reply FOP types.
 *
 * @see m0_cm_trigger_fop_fini()
 */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_fini(void)
{
	m0_cm_trigger_fop_fini(&m0_dix_repair_trigger_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_trigger_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_quiesce_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_quiesce_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_status_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_status_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_abort_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_repair_abort_rep_fopt);
}

/**
 * Initialises start, quiesce, status, abort repair trigger FOP and
 * corresponding reply FOP types.
 *
 * @see m0_cm_trigger_fop_init()
 */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_init(void)
{
	m0_cm_trigger_fop_init(&m0_dix_repair_trigger_fopt,
			       M0_DIX_REPAIR_TRIGGER_OPCODE,
			       "dix repair trigger",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_trigger_rep_fopt,
			       M0_DIX_REPAIR_TRIGGER_REP_OPCODE,
			       "dix repair trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_dix_repair_quiesce_fopt,
			       M0_DIX_REPAIR_QUIESCE_OPCODE,
			       "dix repair quiesce trigger",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_quiesce_rep_fopt,
			       M0_DIX_REPAIR_QUIESCE_REP_OPCODE,
			       "dix repair quiesce trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_dix_repair_status_fopt,
			       M0_DIX_REPAIR_STATUS_OPCODE,
			       "dix repair status",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_status_rep_fopt,
			       M0_DIX_REPAIR_STATUS_REP_OPCODE,
			       "dix repair status reply",
			       m0_status_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_abort_fopt,
			       M0_DIX_REPAIR_ABORT_OPCODE,
			       "dix repair abort",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_repair_abort_rep_fopt,
			       M0_DIX_REPAIR_ABORT_REP_OPCODE,
			       "dix repair abort reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_repair_cmt,
			       &m0_dix_trigger_fom_type_ops);
}



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
