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
 * Original author: Atsuro Hoshino <atsuro_hoshino@xyratex.com>
 * Original creation date: 02-Sep-2013
 */

#include "fop/fom_generic.h"
#include "rpc/rpc.h"
#include "rpc/rpc_opcodes.h"

#include "ha/note_fops.h"
#include "ha/note_fops_xc.h"
#include "ha/note_xc.h"

extern struct m0_reqh_service_type m0_rpc_service_type;

struct m0_fop_type m0_ha_state_get_fopt;
struct m0_fop_type m0_ha_state_get_rep_fopt;
struct m0_fop_type m0_ha_state_set_fopt;

M0_INTERNAL void m0_ha_state_fop_fini(void)
{
	m0_fop_type_fini(&m0_ha_state_get_fopt);
	m0_fop_type_fini(&m0_ha_state_get_rep_fopt);
	m0_fop_type_fini(&m0_ha_state_set_fopt);
}

M0_INTERNAL int m0_ha_state_fop_init(void)
{
	M0_FOP_TYPE_INIT(&m0_ha_state_get_fopt,
			 .name      = "HA State Get",
			 .opcode    = M0_HA_NOTE_GET_OPCODE,
			 .xt        = m0_ha_nvec_xc,
			 .fom_ops   = m0_ha_state_get_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_ha_state_get_rep_fopt,
			 .name      = "HA State Get Reply",
			 .opcode    = M0_HA_NOTE_GET_REP_OPCODE,
			 .xt        = m0_ha_state_fop_xc,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_ha_state_set_fopt,
			 .name      = "HA State Set",
			 .opcode    = M0_HA_NOTE_SET_OPCODE,
			 .xt        = m0_ha_nvec_xc,
			 .fom_ops   = m0_ha_state_set_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	return 0;
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
