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

#include "fop/fop.h"

#include "sns/cm/cm.h"
#include "sns/cm/cp.h"
#include "rpc/rpc_opcodes.h"

M0_INTERNAL void m0_sns_cpx_init(struct m0_fop_type *ft,
				 const struct m0_fom_type_ops *fomt_ops,
				 enum M0_RPC_OPCODES op,
				 const char *name,
				 const struct m0_xcode_type *xt,
				 uint64_t rpc_flags, struct m0_cm_type *cmt)
{
	M0_FOP_TYPE_INIT(ft,
			 .name      = name,
			 .opcode    = op,
			 .xt        = xt,
			 .rpc_flags = rpc_flags,
			 .fom_ops   = fomt_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &cmt->ct_stype);
}

M0_INTERNAL void m0_sns_cpx_fini(struct m0_fop_type *ft)
{
	m0_fop_type_fini(ft);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
