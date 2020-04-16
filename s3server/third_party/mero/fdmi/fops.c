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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#include "lib/string.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "fop/fop_item_type.h"
#include "rpc/rpc_opcodes.h"
#include "fdmi/fdmi.h"
#include "fdmi/fops.h"
#include "fdmi/fops_xc.h"
#include "fdmi/service.h"

M0_INTERNAL int m0_fdmi_req_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh);

const struct m0_fop_type_ops m0_fdmi_fop_ops = {
	.fto_undo = NULL,
	.fto_redo = NULL,
};

#ifndef __KERNEL__
extern struct m0_fom_type_ops m0_fdmi_fom_type_ops;
#endif

struct m0_fop_type m0_fop_fdmi_rec_not_fopt;
struct m0_fop_type m0_fop_fdmi_rec_not_rep_fopt;
struct m0_fop_type m0_fop_fdmi_rec_release_fopt;
struct m0_fop_type m0_fop_fdmi_rec_release_rep_fopt;

extern const struct m0_fom_ops      fdmi_rr_fom_ops;
extern const struct m0_fom_type_ops fdmi_rr_fom_type_ops;
#ifndef __KERNEL__
extern const struct m0_sm_conf            fdmi_rr_fom_sm_conf;
extern const struct m0_sm_conf            fdmi_plugin_dock_fom_sm_conf;
#endif

M0_INTERNAL int m0_fdms_fopts_init(void)
{
	M0_FOP_TYPE_INIT(&m0_fop_fdmi_rec_not_fopt,
			 .name      = "FDMI record notification",
			 .opcode    = M0_FDMI_RECORD_NOT_OPCODE,
			 .xt        = m0_fop_fdmi_record_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
#ifndef __KERNEL__
			 .fom_ops   = m0_fdmi__pdock_fom_type_ops_get(),
			 .svc_type  = &m0_fdmi_service_type,
			 .sm        = &fdmi_plugin_dock_fom_sm_conf,
#endif
			 .fop_ops   = &m0_fdmi_fop_ops);

	M0_FOP_TYPE_INIT(&m0_fop_fdmi_rec_release_fopt,
			 .name      = "FDMI record release",
			 .opcode    = M0_FDMI_RECORD_RELEASE_OPCODE,
			 .xt        = m0_fop_fdmi_rec_release_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &m0_fdmi_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &fdmi_rr_fom_type_ops,
			 .svc_type  = &m0_fdmi_service_type,
			 .sm        = &fdmi_rr_fom_sm_conf
#endif
			);


	return 0;
}

M0_INTERNAL int m0_fdms_rep_fopts_init(void)
{
	M0_FOP_TYPE_INIT(&m0_fop_fdmi_rec_not_rep_fopt,
			 .name      = "FDMI record notification reply",
			 .opcode    = M0_FDMI_RECORD_NOT_REP_OPCODE,
			 .xt        = m0_fop_fdmi_record_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	M0_FOP_TYPE_INIT(&m0_fop_fdmi_rec_release_rep_fopt,
			 .name      = "FDMI record release reply",
			 .opcode    = M0_FDMI_RECORD_RELEASE_REP_OPCODE,
			 .xt        = m0_fop_fdmi_rec_release_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .sm        = &m0_generic_conf);

	return 0;
}

M0_INTERNAL int m0_fdms_fop_init(void)
{
        /*
         * Provided by m0gccxml2xcode after parsing fdmi_fops.h
         */
        m0_xc_fdmi_fops_init();

        return	m0_fdms_fopts_init() ?:
		m0_fdms_rep_fopts_init();
}
M0_EXPORTED(m0_fdms_fop_init);

M0_INTERNAL void m0_fdms_fop_fini(void)
{
        m0_fop_type_fini(&m0_fop_fdmi_rec_not_fopt);
        m0_fop_type_fini(&m0_fop_fdmi_rec_release_fopt);

        m0_fop_type_fini(&m0_fop_fdmi_rec_not_rep_fopt);
        m0_fop_type_fini(&m0_fop_fdmi_rec_release_rep_fopt);

        m0_xc_fdmi_fops_fini();
}
M0_EXPORTED(m0_fdms_fop_fini);

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
