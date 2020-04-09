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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 07-Mar-2013
 */

#include "lib/memory.h"             /* M0_ALLOC_PTR */
#include "lib/ub.h"                 /* M0_UB_ASSERT */
#include "fop/fop.h"                /* m0_fop_type */
#include "fop/fom.h"                /* m0_fom_ops */
#include "fop/fom_generic.h"        /* M0_FOPH_NR */
#include "rpc/rpc_opcodes.h"        /* M0_RPC_UB_REQ_OPCODE */
#include "rpc/ub/fops.h"            /* ub_req, ub_resp */
#include "rpc/ub/fops_xc.h"         /* m0_xc_rpc_ub_fops_init */
#include "ut/cs_service.h"          /* ds1_service_type */

struct m0_fop_type m0_rpc_ub_req_fopt;
struct m0_fop_type m0_rpc_ub_resp_fopt;

/* ----------------------------------------------------------------
 * RPC UB request FOM declarations
 * ---------------------------------------------------------------- */

static int ub_req_fom_create(struct m0_fop *fop, struct m0_fom **m,
			     struct m0_reqh *reqh);
static void ub_req_fom_fini(struct m0_fom *fom);
static int ub_req_fom_tick(struct m0_fom *fom);
static size_t ub_req_fom_home_locality(const struct m0_fom *fom);

static const struct m0_fom_type_ops ub_req_fom_type_ops = {
	.fto_create = ub_req_fom_create
};

static const struct m0_fom_ops ub_req_fom_ops = {
	.fo_fini          = ub_req_fom_fini,
	.fo_tick          = ub_req_fom_tick,
	.fo_home_locality = ub_req_fom_home_locality,
};

/* ----------------------------------------------------------------
 * RPC UB request FOM definitions
 * ---------------------------------------------------------------- */

static int
ub_req_fom_create(struct m0_fop *fop, struct m0_fom **m, struct m0_reqh *reqh)
{
	struct m0_fom *fom;

	M0_ALLOC_PTR(fom);
	M0_ASSERT(fom != NULL);

	m0_fom_init(fom, &m0_rpc_ub_req_fopt.ft_fom_type, &ub_req_fom_ops,
		    fop, NULL, reqh);
	*m = fom;
	return 0;
}

static void ub_req_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
}

static int ub_req_fom_tick(struct m0_fom *fom)
{
	const struct ub_req *req;
	struct ub_resp      *resp;
	int                  rc;

	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	M0_UB_ASSERT(fom->fo_rep_fop == NULL);
	fom->fo_rep_fop = m0_fop_reply_alloc(fom->fo_fop, &m0_rpc_ub_resp_fopt);
	M0_UB_ASSERT(fom->fo_rep_fop != NULL);

	req  = m0_fop_data(fom->fo_fop);
	resp = m0_fop_data(fom->fo_rep_fop);

	resp->ur_seqn = req->uq_seqn;
	rc = m0_buf_copy(&resp->ur_data, &req->uq_data);
	M0_UB_ASSERT(rc == 0);

	m0_fom_phase_move(fom, 0, M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static size_t ub_req_fom_home_locality(const struct m0_fom *fom)
{
	static size_t n = 0;
	return n++;
}

/* ----------------------------------------------------------------
 * RPC UB FOPs initialization/finalization
 * ---------------------------------------------------------------- */

M0_INTERNAL void m0_rpc_ub_fops_init(void)
{
	m0_xc_rpc_ub_fops_init();

	M0_FOP_TYPE_INIT(&m0_rpc_ub_req_fopt,
			 .name      = "RPC UB request",
			 .opcode    = M0_RPC_UB_REQ_OPCODE,
			 .xt        = ub_req_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &ub_req_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &ds1_service_type);
	M0_FOP_TYPE_INIT(&m0_rpc_ub_resp_fopt,
			 .name      = "RPC UB response",
			 .opcode    = M0_RPC_UB_RESP_OPCODE,
			 .xt        = ub_resp_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
}

M0_INTERNAL void m0_rpc_ub_fops_fini(void)
{
	m0_fop_type_fini(&m0_rpc_ub_req_fopt);
	m0_fop_type_fini(&m0_rpc_ub_resp_fopt);
	m0_xc_rpc_ub_fops_fini();
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
