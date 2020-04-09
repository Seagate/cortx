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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 10-Feb-2017
 */

#include "fis/fi_command_fom.h"    /* m0_fi_command_fom_type_ops */
#include "fis/fi_command_fops.h"
#include "fis/fi_command_xc.h"     /* m0_fi_command_req_xc, m0_fi_command_rep_xc */
#include "fop/fom_generic.h"       /* m0_generic_conf */
#include "fop/fop.h"               /* M0_FOP_TYPE_INIT */
#include "rpc/rpc.h"               /* m0_rpc_service_type */
#include "rpc/rpc_opcodes.h"

/**
 * @page fis-lspec-command-fops Fault Injection Command FOPs.
 *
 * Fault Injection Command FOPs initialised at FIS start and finalised at
 * service stop. FI command is accepted only when the FOPs are initialised.
 */

/**
 * @addtogroup fis-dlspec
 *
 * @{
 */
struct m0_fop_type m0_fi_command_req_fopt;
struct m0_fop_type m0_fi_command_rep_fopt;

M0_INTERNAL void m0_fi_command_fop_init(void)
{
	extern struct m0_reqh_service_type m0_rpc_service_type;

	M0_FOP_TYPE_INIT(&m0_fi_command_req_fopt,
			 .name      = "Fault Injection Command",
			 .opcode    = M0_FI_COMMAND_OPCODE,
			 .xt        = m0_fi_command_req_xc,
			 .sm        = &m0_generic_conf,
			 .fom_ops   = &m0_fi_command_fom_type_ops,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_fi_command_rep_fopt,
			 .name      = "Fault Injection Command reply",
			 .opcode    = M0_FI_COMMAND_REP_OPCODE,
			 .xt        = m0_fi_command_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_rpc_service_type);
}

M0_INTERNAL void m0_fi_command_fop_fini(void)
{
	m0_fop_type_fini(&m0_fi_command_req_fopt);
	m0_fop_type_fini(&m0_fi_command_rep_fopt);
}

/** @} end fis-dlspec */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
