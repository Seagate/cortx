/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 20-Mar-2015
 */

#include "lib/finject.h" /* M0_FI_ENABLED */
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/types.h"
#include "lib/refs.h"
#include "sm/sm.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/item.h"
#include "sss/process_fops.h"
#include "sss/process_fops_xc.h"
#include "sss/ss_svc.h"
#ifndef __KERNEL__
#include <unistd.h>
#endif

struct m0_fop_type m0_fop_process_fopt;
struct m0_fop_type m0_fop_process_rep_fopt;
struct m0_fop_type m0_fop_process_svc_list_rep_fopt;

extern struct m0_sm_state_descr     ss_process_fom_phases[];
extern struct m0_sm_conf            ss_process_fom_conf;
extern const struct m0_fom_type_ops ss_process_fom_type_ops;
const struct m0_fop_type_ops        ss_process_fop_type_ops;
const struct m0_fop_type_ops        ss_process_svc_list_fop_type_ops;

static const struct m0_rpc_item_type_ops ss_process_item_type_ops = {
	M0_FOP_DEFAULT_ITEM_TYPE_OPS,
};

M0_INTERNAL int m0_ss_process_fops_init(void)
{
#ifndef __KERNEL__
	m0_sm_conf_extend(m0_generic_conf.scf_state,
			  ss_process_fom_phases,
			  m0_generic_conf.scf_nr_states);
#endif
	m0_fop_process_fopt.ft_magix = 0;
	m0_fop_process_rep_fopt.ft_magix = 0;
	m0_fop_process_svc_list_rep_fopt.ft_magix = 0;

	M0_FOP_TYPE_INIT(&m0_fop_process_fopt,
			 .name      = "Process fop",
			 .opcode    = M0_SSS_PROCESS_REQ_OPCODE,
			 .xt        = m0_ss_process_req_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &ss_process_fop_type_ops,
			 .fom_ops   = &ss_process_fom_type_ops,
			 .sm        = &ss_process_fom_conf,
			 .svc_type  = &m0_ss_svc_type,
			 .rpc_ops   = &ss_process_item_type_ops);

	M0_FOP_TYPE_INIT(&m0_fop_process_rep_fopt,
			 .name      = "Process reply fop",
			 .opcode    = M0_SSS_PROCESS_REP_OPCODE,
			 .xt        = m0_ss_process_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fop_ops   = &ss_process_fop_type_ops,
			 .fom_ops   = &ss_process_fom_type_ops,
			 .sm        = &ss_process_fom_conf,
			 .svc_type  = &m0_ss_svc_type,
			 .rpc_ops   = &ss_process_item_type_ops);

	M0_FOP_TYPE_INIT(&m0_fop_process_svc_list_rep_fopt,
			 .name      = "Process services list reply fop",
			 .opcode    = M0_SSS_PROCESS_SVC_LIST_REP_OPCODE,
			 .xt        = m0_ss_process_svc_list_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fop_ops   = &ss_process_svc_list_fop_type_ops,
			 .fom_ops   = &ss_process_fom_type_ops,
			 .sm        = &ss_process_fom_conf,
			 .svc_type  = &m0_ss_svc_type,
			 .rpc_ops   = &ss_process_item_type_ops);
	return 0;
}

M0_INTERNAL void m0_ss_process_fops_fini(void)
{
	m0_fop_type_fini(&m0_fop_process_fopt);
	m0_fop_type_fini(&m0_fop_process_rep_fopt);
	m0_fop_type_fini(&m0_fop_process_svc_list_rep_fopt);
}

static bool ss_fop_is_process_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_process_rep_fopt;
}

static bool ss_fop_is_process_svc_list_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_process_svc_list_rep_fopt;
}


M0_INTERNAL void m0_ss_process_stop_fop_release(struct m0_ref *ref)
{
#ifndef __KERNEL__
	int            pid = getpid();
#endif
	struct m0_fop *fop = container_of(ref, struct m0_fop, f_ref);

	M0_PRE(fop != NULL);
	M0_PRE(ss_fop_is_process_rep(fop));

	m0_fop_fini(fop);
	m0_free(fop);

#ifndef __KERNEL__
	if (!M0_FI_ENABLED("no_kill"))
		kill(pid, SIGQUIT);
#endif
}

M0_INTERNAL struct m0_fop *m0_ss_process_fop_create(struct m0_rpc_machine *mach,
						    uint32_t               cmd,
						    const struct m0_fid   *fid)
{
	struct m0_fop            *fop;
	struct m0_ss_process_req *req;

	M0_PRE(mach != NULL);
	M0_PRE(cmd < M0_PROCESS_NR);

	fop = m0_fop_alloc(&m0_fop_process_fopt, NULL, mach);
	if (fop == NULL)
		return NULL;

	req = m0_ss_fop_process_req(fop);
	req->ssp_cmd = cmd;
	req->ssp_id = *fid;

	return fop;
}

M0_INTERNAL bool m0_ss_fop_is_process_req(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_process_fopt;
}

M0_INTERNAL struct m0_ss_process_req *m0_ss_fop_process_req(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_ss_fop_is_process_req(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL struct m0_ss_process_rep *m0_ss_fop_process_rep(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(ss_fop_is_process_rep(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL struct m0_ss_process_svc_list_rep *
			m0_ss_fop_process_svc_list_rep(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(ss_fop_is_process_svc_list_rep(fop));
	return m0_fop_data(fop);
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
