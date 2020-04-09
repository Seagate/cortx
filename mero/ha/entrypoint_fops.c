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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 18-May-2016
 */


/**
 * @addtogroup ha
 *
 * TODO handle memory allocation errors;
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/entrypoint_fops.h"
#include "ha/entrypoint_fops_xc.h"

#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/string.h"         /* memcpy */

#include "fop/fop.h"            /* M0_FOP_TYPE_INIT */
#include "rpc/rpc_opcodes.h"    /* M0_HA_ENTRYPOINT_REQ_OPCODE */
#include "rpc/item.h"           /* M0_RPC_ITEM_TYPE_REQUEST */

#include "ha/entrypoint.h"      /* m0_ha_entrypoint_service_type */

struct m0_fop_type m0_ha_entrypoint_req_fopt;
struct m0_fop_type m0_ha_entrypoint_rep_fopt;

M0_INTERNAL void m0_ha_entrypoint_fops_init(void)
{
	M0_FOP_TYPE_INIT(&m0_ha_entrypoint_req_fopt,
			 .name      = "HA Cluster Entry Point Get Req",
			 .opcode    = M0_HA_ENTRYPOINT_REQ_OPCODE,
			 .xt        = m0_ha_entrypoint_req_fop_xc,
			 .fom_ops   = &m0_ha_entrypoint_fom_type_ops,
			 .sm        = &m0_ha_entrypoint_server_fom_states_conf,
			 .svc_type  = &m0_ha_entrypoint_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_ha_entrypoint_rep_fopt,
			 .name      = "HA Cluster Entry Point Get Reply",
			 .opcode    = M0_HA_ENTRYPOINT_REP_OPCODE,
			 .xt        = m0_ha_entrypoint_rep_fop_xc,
			 .svc_type  = &m0_ha_entrypoint_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
}

M0_INTERNAL void m0_ha_entrypoint_fops_fini(void)
{
	m0_fop_type_fini(&m0_ha_entrypoint_rep_fopt);
	m0_fop_type_fini(&m0_ha_entrypoint_req_fopt);
}

M0_INTERNAL int
m0_ha_entrypoint_req2fop(const struct m0_ha_entrypoint_req *req,
                         struct m0_ha_entrypoint_req_fop   *req_fop)
{
	char *git_rev_id_dup = m0_strdup(req->heq_git_rev_id);

	if (git_rev_id_dup == NULL)
		return M0_ERR(-ENOMEM);
	*req_fop = (struct m0_ha_entrypoint_req_fop){
		.erf_first_request   = req->heq_first_request ? 1 : 0,
		.erf_generation      = req->heq_generation,
		.erf_process_fid     = req->heq_process_fid,
		.erf_link_params     = req->heq_link_params,
		.erf_git_rev_id      = M0_BUF_INITS(git_rev_id_dup),
		.erf_pid             = req->heq_pid,
	};
	m0_ha_cookie_to_xc(&req->heq_cookie_expected,
			   &req_fop->erf_cookie_expected);
	return 0;
}

M0_INTERNAL int
m0_ha_entrypoint_fop2req(const struct m0_ha_entrypoint_req_fop *req_fop,
                         const char                            *rpc_endpoint,
                         struct m0_ha_entrypoint_req           *req)
{
	char *rpc_endpoint_dup = m0_strdup(rpc_endpoint);
	char *git_rev_id_dup   = m0_buf_strdup(&req_fop->erf_git_rev_id);

	if (rpc_endpoint_dup == NULL || git_rev_id_dup == NULL) {
		m0_free(rpc_endpoint_dup);
		m0_free(git_rev_id_dup);
		return M0_ERR(-ENOMEM);
	}
	*req = (struct m0_ha_entrypoint_req){
		.heq_first_request   = req_fop->erf_first_request != 0,
		.heq_generation      = req_fop->erf_generation,
		.heq_process_fid     = req_fop->erf_process_fid,
		.heq_rpc_endpoint    = rpc_endpoint_dup,
		.heq_link_params     = req_fop->erf_link_params,
		.heq_git_rev_id      = git_rev_id_dup,
		.heq_pid             = req_fop->erf_pid,
	};
	m0_ha_cookie_from_xc(&req->heq_cookie_expected,
	                     &req_fop->erf_cookie_expected);
	return M0_RC(0);
}

M0_INTERNAL int
m0_ha_entrypoint_fop2rep(const struct m0_ha_entrypoint_rep_fop *rep_fop,
                         struct m0_ha_entrypoint_rep           *rep)
{
	uint32_t i;
	int      rc;

	*rep = (struct m0_ha_entrypoint_rep){
		.hae_quorum        = rep_fop->hbp_quorum,
		.hae_confd_fids    = {
			.af_count = rep_fop->hbp_confd_fids.af_count,
		},
		.hae_active_rm_fid = rep_fop->hbp_active_rm_fid,
		.hae_active_rm_ep  = m0_buf_strdup(&rep_fop->hbp_active_rm_ep),
		.hae_link_params   = rep_fop->hbp_link_params,
		.hae_link_do_reconnect       = !!rep_fop->hbp_link_do_reconnect,
		.hae_disconnected_previously =
			!!rep_fop->hbp_disconnected_previously,
		.hae_control       = rep_fop->hbp_control,
	};
	M0_ASSERT(rep->hae_active_rm_ep != NULL);
	M0_ASSERT(rep->hae_confd_fids.af_count ==
		  rep_fop->hbp_confd_eps.ab_count);
	rc = m0_bufs_to_strings(&rep->hae_confd_eps, &rep_fop->hbp_confd_eps);
	M0_ASSERT(rc == 0);
	M0_ALLOC_ARR(rep->hae_confd_fids.af_elems,
		     rep->hae_confd_fids.af_count);
	M0_ASSERT(rep->hae_confd_fids.af_elems != NULL);
	for (i = 0; i < rep->hae_confd_fids.af_count; ++i) {
		rep->hae_confd_fids.af_elems[i] =
			rep_fop->hbp_confd_fids.af_elems[i];
	}
	m0_ha_cookie_from_xc(&rep->hae_cookie_actual,
	                     &rep_fop->hbp_cookie_actual);
	return M0_RC(0);
}

M0_INTERNAL int
m0_ha_entrypoint_rep2fop(const struct m0_ha_entrypoint_rep *rep,
                         struct m0_ha_entrypoint_rep_fop   *rep_fop)
{
	uint32_t  i;
	char     *rm_ep;
	int       rc;

	rm_ep = rep->hae_active_rm_ep == NULL ?
		m0_strdup("") : m0_strdup(rep->hae_active_rm_ep);
	*rep_fop = (struct m0_ha_entrypoint_rep_fop){
		.hbp_quorum            = rep->hae_quorum,
		.hbp_confd_fids        = {
			.af_count = rep->hae_confd_fids.af_count,
		},
		.hbp_confd_eps         = {
			.ab_count = rep->hae_confd_fids.af_count,
		},
		.hbp_active_rm_fid     = rep->hae_active_rm_fid,
		.hbp_active_rm_ep      = M0_BUF_INITS(rm_ep),
		.hbp_link_params       = rep->hae_link_params,
		.hbp_link_do_reconnect = !!rep->hae_link_do_reconnect,
		.hbp_disconnected_previously =
			!!rep->hae_disconnected_previously,
		.hbp_control           = rep->hae_control,
	};
	M0_ALLOC_ARR(rep_fop->hbp_confd_eps.ab_elems,
		     rep_fop->hbp_confd_eps.ab_count);
	M0_ASSERT(rep_fop->hbp_confd_eps.ab_elems != NULL);
	for (i = 0; i < rep_fop->hbp_confd_eps.ab_count; ++i) {
		rc = m0_buf_copy(&rep_fop->hbp_confd_eps.ab_elems[i],
				 &M0_BUF_INITS((char *)rep->hae_confd_eps[i]));
		M0_ASSERT(rc == 0);
	}
	M0_ALLOC_ARR(rep_fop->hbp_confd_fids.af_elems,
		     rep_fop->hbp_confd_fids.af_count);
	M0_ASSERT(rep_fop->hbp_confd_fids.af_elems != NULL);
	for (i = 0; i < rep_fop->hbp_confd_fids.af_count; ++i) {
		rep_fop->hbp_confd_fids.af_elems[i] =
			rep->hae_confd_fids.af_elems[i];
	}
	m0_ha_cookie_to_xc(&rep->hae_cookie_actual,
	                   &rep_fop->hbp_cookie_actual);
	return 0;
}

M0_INTERNAL void m0_ha_entrypoint_rep_free(struct m0_ha_entrypoint_rep *rep)
{
	m0_free0(&rep->hae_confd_fids.af_elems);
	m0_strings_free(rep->hae_confd_eps);
	m0_free0(&rep->hae_active_rm_ep);
}

M0_INTERNAL void m0_ha_entrypoint_req_free(struct m0_ha_entrypoint_req *req)
{
	m0_free(req->heq_rpc_endpoint);
	/* It should be allocated by m0_alloc() in m0_ha_entrypoint_fop2req */
	m0_free((char *)req->heq_git_rev_id);
}

M0_INTERNAL int  m0_ha_entrypoint_rep_copy(struct m0_ha_entrypoint_rep *to,
					   struct m0_ha_entrypoint_rep *from)
{
	int rc;

	if (from->hae_confd_fids.af_count == 0 || from->hae_confd_eps == NULL)
		return M0_RC(-EINVAL);

	*to = (struct m0_ha_entrypoint_rep) {
		.hae_quorum        = from->hae_quorum,
		.hae_active_rm_fid = from->hae_active_rm_fid,
		.hae_link_params   = from->hae_link_params,
		.hae_active_rm_ep  = m0_strdup(from->hae_active_rm_ep),
		.hae_control       = from->hae_control,
	};
	if (to->hae_active_rm_ep == NULL) {
		rc = -ENOMEM;
		goto out;
	}

	rc = m0_fid_arr_copy(&to->hae_confd_fids, &from->hae_confd_fids);
	if (rc != 0)
		goto out;

	to->hae_confd_eps = m0_strings_dup(from->hae_confd_eps);
	if (to->hae_confd_eps == NULL) {
		rc = -ENOMEM;
		goto out;
	}

	return M0_RC(0);
out:
	m0_ha_entrypoint_rep_free(to);
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
