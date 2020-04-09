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
 * Original author       : Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 08/03/2011
 * Revision              : Manish Honap <Manish_Honap@xyratex.com>
 * Revision date         : 07/31/2012
 */

#include "fop/fop.h"             /* m0_fop_xcode_length */
#include "fop/fom_generic.h"     /* m0_generic_conf */
#include "rpc/rpc_opcodes.h"     /* M0_CONS_FOP_DEVICE_OPCODE */

#include "console/console_fom.h" /* FOMs defs */
#include "console/console_fop.h" /* FOPs defs */
#include "console/console_fop_xc.h" /* FOP memory layout */

/**
   @addtogroup console
   @{
*/

extern struct m0_reqh_service_type m0_rpc_service_type;

struct m0_fop_type m0_cons_fop_device_fopt;
struct m0_fop_type m0_cons_fop_reply_fopt;
struct m0_fop_type m0_cons_fop_test_fopt;

M0_INTERNAL void m0_console_fop_fini(void)
{
	m0_fop_type_fini(&m0_cons_fop_device_fopt);
	m0_fop_type_fini(&m0_cons_fop_reply_fopt);
	m0_fop_type_fini(&m0_cons_fop_test_fopt);
}

extern const struct m0_fom_type_ops m0_cons_fom_device_type_ops;

M0_INTERNAL int m0_console_fop_init(void)
{
	M0_FOP_TYPE_INIT(&m0_cons_fop_device_fopt,
			 .name      = "Device Failed",
			 .opcode    = M0_CONS_FOP_DEVICE_OPCODE,
			 .xt        = m0_cons_fop_device_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .sm        = &m0_generic_conf,
			 .fom_ops   = &m0_console_fom_type_device_ops,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_cons_fop_reply_fopt,
			 .name      = "Console Reply",
			 .opcode    = M0_CONS_FOP_REPLY_OPCODE,
			 .xt        = m0_cons_fop_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_cons_fop_test_fopt,
			 .name      = "Console Test",
			 .opcode    = M0_CONS_TEST,
			 .xt        = m0_cons_fop_test_xc,
			 .sm        = &m0_generic_conf,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &m0_console_fom_type_test_ops,
			 .svc_type  = &m0_rpc_service_type);
	return 0;
}

/** @} end of console */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
