/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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
 * Original authors: James Morse   <james_morse@xyratex.com>,
 *                   Juan Gonzalez <juan_gonzalez@xyratex.com>,
 *                   Sining Wu     <sining_wu@xyratex.com>
 * Original creation date: 09-Apr-2014
 */
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_COB
#include "mdservice/fsync_foms.h"
#include "mdservice/fsync_fops.h"
#include "mdservice/fsync_fops_xc.h"
#include "rpc/rpc_opcodes.h"
#include "lib/trace.h"
#include "fop/fom_generic.h" /* m0_generic_conf */

/**
 * FOP type for the fsync fop. This type is set by this module (fsync_fops.c)
 */
struct m0_fop_type m0_fop_fsync_mds_fopt;

/**
 * FOP type for the fsync fop reply. This type is set by this module
 * (fsync_fops.c)
 */
struct m0_fop_type m0_fop_fsync_rep_fopt;

/**
 * Releases the fsync fop request type and the fsync fop reply type.
 */
M0_INTERNAL void m0_mdservice_fsync_fop_fini(void)
{
	m0_fop_type_fini(&m0_fop_fsync_mds_fopt);

	m0_fop_type_fini(&m0_fop_fsync_rep_fopt);

#ifndef __KERNEL__
	m0_sm_conf_fini(&m0_fsync_fom_conf);
#endif
}
M0_EXPORTED(m0_mdservice_fsync_fop_fini);

#ifndef __KERNEL__

/**
 * Operations for the fom. Mainly the function invoked to create the right fom
 * for an fsync fop.
 */
const struct m0_fom_type_ops m0_fsync_fom_ops = {
	.fto_create = m0_fsync_req_fom_create
};


static void fop_init(struct m0_reqh_service_type * svct)
{
	M0_ASSERT(svct != NULL);

	/* extend m0_fsync_fom_conf with the generic phases */
	m0_sm_conf_extend(m0_generic_conf.scf_state, m0_fsync_fom_phases,
			  m0_generic_conf.scf_nr_states);

	m0_sm_conf_trans_extend(&m0_generic_conf, &m0_fsync_fom_conf);

	m0_sm_conf_init(&m0_fsync_fom_conf);

	M0_FOP_TYPE_INIT(&m0_fop_fsync_mds_fopt,
			 .name      = "fsync-mds request",
			 .opcode    = M0_FSYNC_MDS_OPCODE,
			 .xt        = m0_fop_fsync_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &m0_fsync_fom_ops,
			 /*
			  * The fsync fopt is initialize with the svct of the
			  * first service calling this function.
			  */
			 .svc_type  = svct,
			 .sm        = &m0_fsync_fom_conf);
}

#else /* __KERNEL__ */

static void fop_init(__attribute__((unused)) struct m0_reqh_service_type * svct)
{
	M0_FOP_TYPE_INIT(&m0_fop_fsync_mds_fopt,
			 .name      = "fsync-mds request",
			 .opcode    = M0_FSYNC_MDS_OPCODE,
			 .xt        = m0_fop_fsync_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
}

M0_EXPORTED(m0_mdservice_fsync_fop_init);

#endif


/**
 * Initializes the fsync fop request type and the fsync fop reply type
 * so the caller can use them to understand and handle fsync fops.
 */
M0_INTERNAL int m0_mdservice_fsync_fop_init(struct m0_reqh_service_type * svct)
{
	fop_init(svct);

	M0_FOP_TYPE_INIT(&m0_fop_fsync_rep_fopt,
			 .name      = "fsync reply",
			 .opcode    = M0_FSYNC_MDS_REP_OPCODE,
			 .xt        = m0_fop_fsync_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	return M0_RC(0);
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
