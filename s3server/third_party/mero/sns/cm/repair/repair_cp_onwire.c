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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 02/15/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"
#include "fop/fop.h"

#include "sns/cm/cm.h"
#include "sns/cm/sns_cp_onwire.h"
#include "sns/cm/sns_cp_onwire_xc.h"

struct m0_fop_type m0_sns_repair_cpx_fopt;
struct m0_fop_type m0_sns_repair_cpx_reply_fopt;
extern struct m0_cm_type sns_repair_cmt;

static int repair_cp_fom_create(struct m0_fop *fop, struct m0_fom **m,
				struct m0_reqh *reqh);

M0_INTERNAL const struct m0_fom_type_ops repair_cp_fom_type_ops = {
        .fto_create = repair_cp_fom_create
};

static int repair_cp_fom_create(struct m0_fop *fop, struct m0_fom **m,
				struct m0_reqh *reqh)
{
	struct m0_fop   *r_fop;
	int              rc;

	r_fop = m0_fop_reply_alloc(fop, &m0_sns_repair_cpx_reply_fopt);
	if (r_fop == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_cm_cp_fom_create(fop, r_fop, m, reqh);

	return M0_RC(rc);
}

M0_INTERNAL void m0_sns_cm_repair_cpx_init(void)
{
	m0_sns_cpx_init(&m0_sns_repair_cpx_fopt,
			&repair_cp_fom_type_ops,
			M0_SNS_CM_REPAIR_CP_OPCODE,
			"SNS Repair copy packet", m0_sns_cpx_xc,
			M0_RPC_MUTABO_REQ,
			&sns_repair_cmt);
	m0_sns_cpx_init(&m0_sns_repair_cpx_reply_fopt,
			&repair_cp_fom_type_ops,
			M0_SNS_CM_REPAIR_CP_REP_OPCODE,
			"SNS Repair copy packet reply",
			m0_sns_cpx_reply_xc, M0_RPC_ITEM_TYPE_REPLY,
			&sns_repair_cmt);
}

M0_INTERNAL void m0_sns_cm_repair_cpx_fini(void)
{
        m0_sns_cpx_fini(&m0_sns_repair_cpx_fopt);
        m0_sns_cpx_fini(&m0_sns_repair_cpx_reply_fopt);
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
