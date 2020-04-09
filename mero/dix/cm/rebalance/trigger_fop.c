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
#include "cm/repreb/trigger_fop.h"
#include "cm/repreb/trigger_fop_xc.h"
#include "cm/repreb/trigger_fom.h"
#include "dix/cm/trigger_fom.h"
#include "dix/cm/trigger_fop.h"
#include "dix/cm/cm.h"
#include "rpc/item.h"

/**
 * Finalises start, quiesce, status, abort re-balance trigger FOP and
 * corresponding reply FOP types.
 *
 * @see m0_cm_trigger_fop_fini()
 */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_fini(void)
{
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_trigger_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_trigger_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_quiesce_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_quiesce_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_status_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_status_rep_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_abort_fopt);
	m0_cm_trigger_fop_fini(&m0_dix_rebalance_abort_rep_fopt);
}

/**
 * Initialises start, quiesce, status, abort re-balance trigger FOP and
 * corresponding reply FOP types.
 *
 * @see m0_cm_trigger_fop_init()
 */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_init(void)
{
	m0_cm_trigger_fop_init(&m0_dix_rebalance_trigger_fopt,
			       M0_DIX_REBALANCE_TRIGGER_OPCODE,
			       "dix rebalance trigger",
			       trigger_fop_xc,
			        M0_RPC_MUTABO_REQ,
			        &dix_rebalance_cmt,
			        &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_rebalance_trigger_rep_fopt,
			       M0_DIX_REBALANCE_TRIGGER_REP_OPCODE,
			       "dix rebalance trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_rebalance_cmt,
			       &m0_dix_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_dix_rebalance_quiesce_fopt,
			       M0_DIX_REBALANCE_QUIESCE_OPCODE,
			       "dix rebalance quiesce trigger",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_rebalance_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_rebalance_quiesce_rep_fopt,
			       M0_DIX_REBALANCE_QUIESCE_REP_OPCODE,
			       "dix rebalance quiesce trigger reply",
			       trigger_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_rebalance_cmt,
			       &m0_dix_trigger_fom_type_ops);

	m0_cm_trigger_fop_init(&m0_dix_rebalance_status_fopt,
			       M0_DIX_REBALANCE_STATUS_OPCODE,
			       "dix rebalance status",
			       trigger_fop_xc,
			       M0_RPC_MUTABO_REQ,
			       &dix_rebalance_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_rebalance_status_rep_fopt,
			       M0_DIX_REBALANCE_STATUS_REP_OPCODE,
			       "dix rebalance status reply",
			       m0_status_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_rebalance_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_rebalance_abort_fopt,
			       M0_DIX_REBALANCE_ABORT_OPCODE,
			       "dix rebalance abort",
			       m0_status_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REQUEST,
			       &dix_rebalance_cmt,
			       &m0_dix_trigger_fom_type_ops);
	m0_cm_trigger_fop_init(&m0_dix_rebalance_abort_rep_fopt,
			       M0_DIX_REBALANCE_ABORT_REP_OPCODE,
			       "dix rebalance abort reply",
			       m0_status_rep_fop_xc,
			       M0_RPC_ITEM_TYPE_REPLY,
			       &dix_rebalance_cmt,
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
