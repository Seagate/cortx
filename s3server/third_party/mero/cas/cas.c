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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 5-May-2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS

#include "lib/trace.h"
#include "lib/finject.h"     /* M0_FI_ENABLED */
#include "fid/fid.h"         /* m0_fid_type_register */
#include "fop/fop.h"
#include "rpc/rpc_opcodes.h"
#include "cas/cas.h"
#include "cas/cas_xc.h"
#include "mdservice/fsync_foms.h"       /* m0_fsync_fom_conf */
#include "mdservice/fsync_fops.h"       /* m0_fsync_fom_ops */
#include "mdservice/fsync_fops_xc.h"    /* m0_fop_fsync_xc */
#include "cas/client.h"                 /* m0_cas_sm_conf_init */

struct m0_fom_type_ops;
struct m0_sm_conf;
struct m0_reqh_service_type;

/**
 * @addtogroup cas
 *
 * @{
 */

M0_INTERNAL struct m0_fop_type cas_get_fopt;
M0_INTERNAL struct m0_fop_type cas_put_fopt;
M0_INTERNAL struct m0_fop_type cas_del_fopt;
M0_INTERNAL struct m0_fop_type cas_cur_fopt;
M0_INTERNAL struct m0_fop_type cas_rep_fopt;
M0_INTERNAL struct m0_fop_type cas_gc_fopt;
struct m0_fop_type m0_fop_fsync_cas_fopt;

static int cas_fops_init(const struct m0_sm_conf           *sm_conf,
			 const struct m0_fom_type_ops      *fom_ops,
			 const struct m0_reqh_service_type *svctype)
{
	M0_FOP_TYPE_INIT(&cas_get_fopt,
			 .name      = "cas-get",
			 .opcode    = M0_CAS_GET_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_put_fopt,
			 .name      = "cas-put",
			 .opcode    = M0_CAS_PUT_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST |
				      M0_RPC_ITEM_TYPE_MUTABO,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_del_fopt,
			 .name      = "cas-del",
			 .opcode    = M0_CAS_DEL_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST |
				      M0_RPC_ITEM_TYPE_MUTABO,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_cur_fopt,
			 .name      = "cas-cur",
			 .opcode    = M0_CAS_CUR_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_rep_fopt,
			 .name      = "cas-rep",
			 .opcode    = M0_CAS_REP_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .xt        = m0_cas_rep_xc,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&cas_gc_fopt,
			 .name      = "cas-gc-wait",
			 .opcode    = M0_CAS_GCW_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .xt        = m0_cas_op_xc,
			 .fom_ops   = fom_ops,
			 .sm        = sm_conf,
			 .svc_type  = svctype);
	M0_FOP_TYPE_INIT(&m0_fop_fsync_cas_fopt,
			 .name      = "fsync-cas",
			 .opcode    = M0_FSYNC_CAS_OPCODE,
			 .xt        = m0_fop_fsync_xc,
#ifndef __KERNEL__
			 .svc_type  = svctype,
			 .sm        = &m0_fsync_fom_conf,
			 .fom_ops   = &m0_fsync_fom_ops,
#endif
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	return  m0_fop_type_addb2_instrument(&cas_get_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_put_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_del_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_cur_fopt) ?:
		m0_fop_type_addb2_instrument(&cas_gc_fopt)?:
		m0_fop_type_addb2_instrument(&m0_fop_fsync_cas_fopt);
}

static void cas_fops_fini(void)
{
	m0_fop_type_addb2_deinstrument(&cas_gc_fopt);
	m0_fop_type_addb2_deinstrument(&cas_cur_fopt);
	m0_fop_type_addb2_deinstrument(&cas_del_fopt);
	m0_fop_type_addb2_deinstrument(&cas_put_fopt);
	m0_fop_type_addb2_deinstrument(&cas_get_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_fsync_cas_fopt);
	m0_fop_type_fini(&cas_gc_fopt);
	m0_fop_type_fini(&cas_rep_fopt);
	m0_fop_type_fini(&cas_cur_fopt);
	m0_fop_type_fini(&cas_del_fopt);
	m0_fop_type_fini(&cas_put_fopt);
	m0_fop_type_fini(&cas_get_fopt);
	m0_fop_type_fini(&m0_fop_fsync_cas_fopt);
}

/**
 * FID of the meta-index. It has the smallest possible FID in order to be always
 * the first during iteration over existing indices.
 */
M0_INTERNAL const struct m0_fid m0_cas_meta_fid = M0_FID_TINIT('i', 0, 0);

/**
 * FID of the catalogue-index index.
 */
M0_INTERNAL const struct m0_fid m0_cas_ctidx_fid = M0_FID_TINIT('i', 0, 1);

/**
 * FID of the "dead index" catalogue (used to collect deleted indices).
 */
M0_INTERNAL const struct m0_fid m0_cas_dead_index_fid = M0_FID_TINIT('i', 0, 2);

M0_INTERNAL const struct m0_fid_type m0_cas_index_fid_type = {
	.ft_id   = 'i',
	.ft_name = "cas-index"
};

M0_INTERNAL const struct m0_fid_type m0_cctg_fid_type = {
	.ft_id   = 'T',
	.ft_name = "component-catalogue"
};

M0_INTERNAL const struct m0_fid_type m0_dix_fid_type = {
	.ft_id   = 'x',
	.ft_name = "distributed-index"
};

M0_INTERNAL int m0_cas_module_init(void)
{
	struct m0_sm_conf            *sm_conf;
	const struct m0_fom_type_ops *fom_ops;
	struct m0_reqh_service_type  *svctype;

	m0_fid_type_register(&m0_cas_index_fid_type);
	m0_fid_type_register(&m0_cctg_fid_type);
	m0_fid_type_register(&m0_dix_fid_type);
	m0_cas_svc_init();
	m0_cas_svc_fop_args(&sm_conf, &fom_ops, &svctype);
	return cas_fops_init(sm_conf, fom_ops, svctype) ?:
		m0_cas_sm_conf_init();
}

M0_INTERNAL void m0_cas_module_fini(void)
{
	m0_cas_sm_conf_fini();
	cas_fops_fini();
	m0_cas_svc_fini();
	m0_fid_type_unregister(&m0_cas_index_fid_type);
	m0_fid_type_unregister(&m0_cctg_fid_type);
	m0_fid_type_unregister(&m0_dix_fid_type);
}

M0_INTERNAL void m0_cas_id_fini(struct m0_cas_id *cid)
{
	M0_PRE(cid != NULL);

	if (m0_fid_type_getfid(&cid->ci_fid) == &m0_cctg_fid_type)
		m0_dix_ldesc_fini(&cid->ci_layout.u.dl_desc);
	M0_SET0(cid);
}

M0_INTERNAL bool m0_cas_id_invariant(const struct m0_cas_id *cid)
{
	return _0C(cid != NULL) &&
	       _0C(M0_IN(m0_fid_type_getfid(&cid->ci_fid),
		      (&m0_cas_index_fid_type, &m0_cctg_fid_type,
		       &m0_dix_fid_type))) &&
	       _0C(M0_IN(cid->ci_layout.dl_type, (DIX_LTYPE_UNKNOWN,
				       DIX_LTYPE_ID, DIX_LTYPE_DESCR))) &&
	       _0C(ergo(m0_fid_type_getfid(&cid->ci_fid) ==
				       &m0_cas_index_fid_type,
			M0_IS0(&cid->ci_layout)));
}

M0_INTERNAL bool cas_in_ut(void)
{
	return M0_FI_ENABLED("ut");
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of cas group */

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
