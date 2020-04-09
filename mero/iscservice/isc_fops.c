/*
 * COPYRIGHT 2017 SEAGATE TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author: Nachiket Sahasrabuddhe <Nachiket_Sahasrabuddhe@seagate.com>
 * Original creation date: 27 Nov 2017
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "fop/fom.h"
#include "fop/fop.h"
#include "iscservice/isc_fops.h"
#include "iscservice/isc_fops_xc.h"
#include "lib/errno.h"
#include "rpc/rpc.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"

struct m0_fop_type m0_fop_isc_fopt;
struct m0_fop_type m0_fop_isc_rep_fopt;

extern struct m0_reqh_service_type m0_iscs_type;
extern const struct m0_fom_type_ops m0_fom_isc_type_ops;
extern struct m0_sm_state_descr isc_fom_phases[];
extern struct m0_sm_conf isc_sm_conf;

M0_INTERNAL int m0_iscservice_fop_init(void)
{
	m0_sm_conf_extend(m0_generic_conf.scf_state, isc_fom_phases,
			  m0_generic_conf.scf_nr_states);
	M0_FOP_TYPE_INIT(&m0_fop_isc_fopt,
			 .name      = "isc-exec-fop",
			 .opcode    = M0_ISCSERVICE_REQ_OPCODE,
			 .xt        = m0_fop_isc_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &m0_fom_isc_type_ops,
			 .svc_type  = &m0_iscs_type,
			 .sm        = &isc_sm_conf);

	M0_FOP_TYPE_INIT(&m0_fop_isc_rep_fopt,
			 .name      = "isc-fop-reply",
			 .opcode    = M0_ISCSERVICE_REP_OPCODE,
			 .xt        = m0_fop_isc_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_iscs_type
                         );
	return m0_fop_type_addb2_instrument(&m0_fop_isc_fopt) ?:
	       m0_fop_type_addb2_instrument(&m0_fop_isc_rep_fopt);
}

M0_INTERNAL void m0_iscservice_fop_fini(void)
{
	m0_fop_type_addb2_deinstrument(&m0_fop_isc_rep_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_isc_fopt);

	m0_fop_type_fini(&m0_fop_isc_rep_fopt);
	m0_fop_type_fini(&m0_fop_isc_fopt);
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
