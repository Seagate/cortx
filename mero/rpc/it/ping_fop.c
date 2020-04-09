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
 * Original author: Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 07/07/2011
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "fop/fom.h"
#include "fop/fop.h"
#include "rpc/it/ping_fop.h"
#include "rpc/it/ping_fop_xc.h"
#include "rpc/it/ping_fom.h"
#include "lib/errno.h"
#include "rpc/rpc.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"

struct m0_fop_type m0_fop_ping_fopt;
struct m0_fop_type m0_fop_ping_rep_fopt;

M0_INTERNAL void m0_ping_fop_fini(void)
{
	m0_fop_type_fini(&m0_fop_ping_rep_fopt);
	m0_fop_type_fini(&m0_fop_ping_fopt);
	m0_xc_rpc_it_ping_fop_fini();
}

extern const struct m0_fom_type_ops m0_fom_ping_type_ops;
extern struct m0_reqh_service_type m0_rpc_service_type;

M0_INTERNAL void m0_ping_fop_init(void)
{
	m0_xc_rpc_it_ping_fop_init();
	M0_FOP_TYPE_INIT(&m0_fop_ping_fopt,
			 .name      = "Ping fop",
			 .opcode    = M0_RPC_PING_OPCODE,
			 .xt        = m0_fop_ping_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &m0_fom_ping_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_fop_ping_rep_fopt,
			 .name      = "Ping fop reply",
			 .opcode    = M0_RPC_PING_REPLY_OPCODE,
			 .xt        = m0_fop_ping_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_rpc_service_type);
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
