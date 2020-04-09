/* -*- C -*- */
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
 * https://www.seagate.com/contacts
 *
 * Original author: Jean-Philippe Bernardy <jean-philippe.bernardy@tweag.io>
 * Original creation date: 15 Feb 2016
 * Subsequent modifications: Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
 * Date of modifications: 28 Nov 2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ISCS
#include "lib/trace.h"

#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/rpc_machine.h"
#include "iscservice/isc.h"
#include "iscservice/isc_service.h"
#include "lib/hash.h"
#include "lib/memory.h"
#include "lib/chan.h"
#include "lib/finject.h"

static void isc_comp_cleanup(struct m0_fom *fom, int rc, int next_phase);
static int comp_ref_get(struct m0_htable *comp_ht, struct m0_isc_comp_req *req);
static void comp_ref_put(struct m0_htable *comp_ht, struct m0_isc_comp *comp);

/*
 * Represents phases associated with a fom associated with
 * the remote invocation of a registered computation.
 */
enum m0_isc_fom_phases {
	/** Locates the computation in hash table. */
	ISC_PREPARE = M0_FOPH_TYPE_SPECIFIC,
	/** Fetches arguments for computation over network. */
	ISC_LOAD_ARGS,
	/** Copies the fetched arguments to local buffer. */
	ISC_ARGS_LOAD_DONE,
	/** Launches the computation. */
	ISC_COMP_LAUNCH,
	/** Returns the result to caller of the computation. */
	ISC_RET_VAL,
	/** Ensures that returned value is sent over network. */
	ISC_RET_VAL_DONE,
	/** Finalizes relevant data structures. */
	ISC_DONE,
};

struct m0_sm_state_descr isc_fom_phases[] = {
	[ISC_PREPARE] = {
		.sd_name = "prepare",
		.sd_allowed = M0_BITS(ISC_LOAD_ARGS, M0_FOPH_FAILURE)
	},
	[ISC_LOAD_ARGS] = {
		.sd_name = "load-arguments",
		.sd_allowed = M0_BITS(ISC_ARGS_LOAD_DONE)
	},
	[ISC_ARGS_LOAD_DONE] = {
		.sd_name = "args-load-done",
		.sd_allowed = M0_BITS(ISC_COMP_LAUNCH, M0_FOPH_FAILURE)
	},
	[ISC_COMP_LAUNCH] = {
		.sd_name = "comp-launch",
		.sd_allowed = M0_BITS(ISC_RET_VAL)
	},
	[ISC_RET_VAL] = {
		.sd_name = "ret-val",
		.sd_allowed = M0_BITS(ISC_RET_VAL_DONE)
	},
	[ISC_RET_VAL_DONE] = {
		.sd_name = "ret-val-done",
		.sd_allowed = M0_BITS(ISC_DONE, M0_FOPH_FAILURE)
	},
	[ISC_DONE] = {
		.sd_name = "done",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS)
	}
};

static size_t fom_home_locality(const struct m0_fom *fom)
{
	struct m0_isc_comp_req *comp_req = M0_AMB(comp_req, fom, icr_fom);

	return m0_fid_hash(&comp_req->icr_comp_fid);
}

static struct m0_rpc_machine *fom_rmach(const struct m0_fom *fom)
{
	return fom->fo_fop->f_item.ri_session->s_conn->c_rpc_machine;
}

static bool is_alignment_required(struct m0_isc_comp_req *comp_req)
{
	struct m0_fop_isc     *fop_isc;
	struct m0_rpc_machine *rmach;

	if (comp_req->icr_req_type == M0_ICRT_LOCAL)
		return false;

	fop_isc = m0_fop_data(comp_req->icr_fom.fo_fop);
	rmach = fom_rmach(&comp_req->icr_fom);
		/* Caller is expecting bulk-io. */
	return (fop_isc->fi_ret.ab_type == M0_RPC_AT_BULK_RECV  ||
		/* Reply size is too large for inline buffer. */
		comp_req->icr_result.b_nob > rmach->rm_bulk_cutoff) &&
	        !m0_is_aligned((uint64_t)comp_req->icr_result.b_addr,
			       M0_0VEC_ALIGN);
}

static int isc_comp_launch(struct m0_isc_comp_req *comp_req, struct m0_fom *fom,
			   int next_phase)
{
	struct m0_isc_comp *comp;
	struct m0_buf       buf;
	int                 result;
	int                 rc;

	comp = m0_cookie_of(&comp_req->icr_cookie, struct m0_isc_comp, ic_gen);
	/* Launching is expected only after initialisation of the cookie. */
	M0_ASSERT(comp != NULL);
	result = comp->ic_op(&comp_req->icr_args, &comp_req->icr_result,
			     &comp_req->icr_comp_data, &rc);
	M0_ASSERT(M0_IN(result, (M0_FSO_AGAIN, M0_FSO_WAIT)));
	/*
	 * In case a re-invocation of a computation is necessary, phase
	 * is not changed.
	 */
	if (rc == -EAGAIN)
		return result;
	/*
	 * In case result is conveyed using rpc bulk adjust the
	 * memory alignment of result.
	 */
	if (is_alignment_required(comp_req)) {
		M0_SET0(&buf);
		rc = m0_buf_copy_aligned(&buf, &comp_req->icr_result,
					 M0_0VEC_SHIFT);
		m0_buf_free(&comp_req->icr_result);
		if (rc == 0)
			comp_req->icr_result = buf;
	}

	if (M0_FI_ENABLED("oom")) {
		m0_buf_free(&comp_req->icr_result);
		rc = M0_ERR(-ENOMEM);
	}
	m0_fom_phase_set(fom, next_phase);
	/* Override only if computation was successful. */
	comp_req->icr_rc = comp_req->icr_rc ?: rc;
	return result;
}

static void isc_comp_prepare(struct m0_isc_comp_req *comp_req,
			     struct m0_fom *fom,
			     int next_phase0, int next_phase1)
{
	struct m0_htable *comp_ht = m0_isc_htable_get();
	int               rc;

	m0_hbucket_lock(comp_ht, &comp_req->icr_comp_fid);
	rc = comp_ref_get(comp_ht, comp_req);
	m0_hbucket_unlock(comp_ht, &comp_req->icr_comp_fid);

	if (rc == 0)
		m0_fom_phase_set(fom, next_phase0);
	else {
		m0_fom_phase_move(fom, M0_ERR(rc), next_phase1);
		comp_req->icr_rc = rc;
	}
}

static int isc_fom_tick(struct m0_fom *fom)
{
	struct m0_isc_comp_req *comp_req  = M0_AMB(comp_req, fom, icr_fom);
	struct m0_fop_isc      *fop_isc   = m0_fop_data(fom->fo_fop);
	struct m0_fop_isc_rep  *reply_isc = m0_fop_data(fom->fo_rep_fop);
	struct m0_buf          *comp_args = &comp_req->icr_args;
	struct m0_buf          *comp_res  = &comp_req->icr_result;
	int                     phase     = m0_fom_phase(fom);
	int                     rc        = 0;
	int                     result    = M0_FSO_AGAIN;

	M0_ENTRY("fom %p phase %d", fom, m0_fom_phase(fom));

	switch (phase) {
	case M0_FOPH_INIT ... M0_FOPH_NR - 1:
		result = m0_fom_tick_generic(fom);
		break;
	case ISC_PREPARE:
		isc_comp_prepare(comp_req, fom, ISC_LOAD_ARGS, M0_FOPH_FAILURE);
		reply_isc->fir_rc = comp_req->icr_rc;
		break;
	case ISC_LOAD_ARGS:
		result = m0_rpc_at_load(&fop_isc->fi_args, fom,
				        ISC_ARGS_LOAD_DONE);
		reply_isc->fir_comp_cookie = comp_req->icr_cookie;
		break;
	case ISC_ARGS_LOAD_DONE:
		rc = m0_rpc_at_get(&fop_isc->fi_args, comp_args);
		if (rc == 0)
			m0_fom_phase_set(fom, ISC_COMP_LAUNCH);
		else
			isc_comp_cleanup(fom, M0_ERR(rc), M0_FOPH_FAILURE);
		break;
	case ISC_COMP_LAUNCH:
		result = isc_comp_launch(comp_req, fom, ISC_RET_VAL);
		reply_isc->fir_rc = comp_req->icr_rc;
		break;
	case ISC_RET_VAL:
		m0_rpc_at_init(&reply_isc->fir_ret);
		result =
		  m0_rpc_at_reply(&fop_isc->fi_ret, &reply_isc->fir_ret,
				  comp_res, fom, ISC_RET_VAL_DONE);
		break;
	case ISC_RET_VAL_DONE:
		rc = m0_rpc_at_reply_rc(&reply_isc->fir_ret);
		/*
		 * Ignore the case when a successful computation does not
		 * have a buffer to send.
		 */
		if (M0_IN(rc, (0, -ENODATA)))
			m0_fom_phase_set(fom, ISC_DONE);
		else
			isc_comp_cleanup(fom, M0_ERR(rc), M0_FOPH_FAILURE);
		break;
	case ISC_DONE:
		isc_comp_cleanup(fom, 0, M0_FOPH_SUCCESS);
		break;
	}
	return result;
}

static void isc_fom_fini(struct m0_fom *fom)
{
	struct m0_isc_comp_req *comp_req = M0_AMB(comp_req, fom, icr_fom);
	struct m0_fop_isc      *fop_isc  = m0_fop_data(fom->fo_fop);

	m0_rpc_at_fini(&fop_isc->fi_args);
	m0_fom_fini(fom);
	m0_free(comp_req);
}

static const struct m0_fom_ops isc_fom_ops = {
	.fo_tick          = &isc_fom_tick,
	.fo_home_locality = &fom_home_locality,
	.fo_fini          = &isc_fom_fini
};

static int isc_fom_create(struct m0_fop *fop, struct m0_fom **out,
			  struct m0_reqh *reqh)
{
	struct m0_isc_comp_req *comp_req;
	struct m0_fom          *fom;
	struct m0_fop_isc      *fop_isc = m0_fop_data(fop);
	struct m0_fop          *reply_isc;
	struct m0_buf           in_buf;

	M0_PRE(fop != NULL && fop->f_type != NULL && out != NULL);

	M0_ALLOC_PTR(comp_req);
	if (comp_req == NULL)
		return M0_ERR(-ENOMEM);
	reply_isc = m0_fop_reply_alloc(fop, &m0_fop_isc_rep_fopt);
	if (reply_isc == NULL) {
		m0_free(comp_req);
		return M0_ERR(-ENOMEM);
	}
	m0_buf_init(&in_buf, NULL, 0);
	m0_isc_comp_req_init(comp_req, &in_buf, &fop_isc->fi_comp_id,
			     &fop_isc->fi_comp_cookie, M0_ICRT_REMOTE, reqh);
	fom = &comp_req->icr_fom;
	*out = fom;
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &isc_fom_ops, fop,
		    reply_isc, reqh);
	return M0_RC(0);
}

static void isc_comp_cleanup(struct m0_fom *fom, int rc, int next_phase)
{
	struct m0_isc_comp_req *comp_req = M0_AMB(comp_req, fom, icr_fom);
	struct m0_htable       *comp_ht = m0_isc_htable_get();
	struct m0_isc_comp     *comp;
	struct m0_fop_isc_rep  *reply_isc;

	comp = m0_cookie_of(&comp_req->icr_cookie, struct m0_isc_comp, ic_gen);
	M0_ASSERT(comp != NULL);
	m0_hbucket_lock(comp_ht, &comp_req->icr_comp_fid);
	comp_ref_put(comp_ht, comp);
	m0_hbucket_unlock(comp_ht, &comp_req->icr_comp_fid);
	/*
	 * Override the result of computation only if
	 * it has completed successfully.
	 */
	comp_req->icr_rc = comp_req->icr_rc ?: rc;
	if (comp_req->icr_req_type == M0_ICRT_REMOTE) {
		reply_isc = m0_fop_data(fom->fo_rep_fop);
		reply_isc->fir_rc = comp_req->icr_rc;
	}
	m0_fom_phase_move(fom, rc, next_phase);
}

static int comp_ref_get(struct m0_htable *comp_ht,
			struct m0_isc_comp_req *comp_req)
{
	struct m0_cookie   *cookie = &comp_req->icr_cookie;
	struct m0_fid      *fid    = &comp_req->icr_comp_fid;
	struct m0_isc_comp *comp;

	comp = m0_cookie_of(cookie, struct m0_isc_comp, ic_gen);
	if (comp == NULL)
		comp = m0_isc_htable_lookup(comp_ht, fid);
	if (comp == NULL) {
		*cookie = M0_COOKIE_NULL;
		return M0_ERR_INFO(-ENOENT, "Computation not found");
	}
	if (comp->ic_reg_state == M0_ICS_UNREGISTERED ||
	    M0_FI_ENABLED("unregister")) {
		*cookie = M0_COOKIE_NULL;
		return M0_ERR_INFO(-EINVAL, "Computation is already"
				   "unregistered");
	}
	m0_cookie_init(cookie, &comp->ic_gen);
	M0_CNT_INC(comp->ic_ref_count);

	return 0;
}

static void comp_cleanup(struct m0_htable *comp_ht, struct m0_isc_comp *comp)
{
	m0_isc_htable_del(comp_ht, comp);
	m0_isc_tlink_fini(comp);
	m0_free(comp->ic_name);
	m0_free(comp);
}

static void comp_ref_put(struct m0_htable *comp_ht, struct m0_isc_comp *comp)
{
	M0_CNT_DEC(comp->ic_ref_count);
	if (comp->ic_ref_count == 0 &&
	    comp->ic_reg_state == M0_ICS_UNREGISTERED)
		comp_cleanup(comp_ht, comp);
}

const struct m0_fom_type_ops m0_fom_isc_type_ops = {
	.fto_create = isc_fom_create
};

struct m0_sm_conf isc_sm_conf = {
	.scf_name      = "isc-fom",
	.scf_nr_states = ARRAY_SIZE(isc_fom_phases),
	.scf_state     = isc_fom_phases,
};

enum exec_fom_phases {
	EP_PREPARE = M0_FOM_PHASE_INIT,
	EP_FINI    = M0_FOM_PHASE_FINISH,
	EP_EXEC,
	EP_FAIL,
	EP_COMPLETE,
};

struct m0_sm_state_descr comp_exec_fom_states[] = {
	[EP_PREPARE] = {
		.sd_flags = M0_SDF_INITIAL,
		.sd_name = "prepare",
		.sd_allowed = M0_BITS(EP_EXEC, EP_FAIL, EP_COMPLETE)
	},
	[EP_EXEC] = {
		.sd_name = "launch-computation",
		.sd_allowed = M0_BITS(EP_COMPLETE)
	},
	[EP_COMPLETE] = {
		.sd_name = "computation-completes",
		.sd_allowed = M0_BITS(EP_FINI)
	},
	[EP_FAIL] = {
		.sd_flags = M0_SDF_FAILURE,
		.sd_name = "failure-in-launch",
		.sd_allowed = M0_BITS(EP_FINI),
	},
	[EP_FINI] = {
		.sd_flags =  M0_SDF_TERMINAL,
		.sd_name = "computation-finalize",
		.sd_allowed = 0
	}
};

static const struct m0_sm_conf comp_exec_fom_states_conf = {
	.scf_name      = "computation-execution-fom",
	.scf_nr_states = ARRAY_SIZE(comp_exec_fom_states),
	.scf_state     = comp_exec_fom_states,
};

static struct m0_fom_type comp_exec_fom_type;
static const struct m0_fom_type_ops comp_exec_fom_type_ops = {
	.fto_create = NULL,
};

static int ce_fom_tick(struct m0_fom *fom)
{
	struct m0_isc_comp_req *comp_req = M0_AMB(comp_req, fom, icr_fom);
	int                     phase    = m0_fom_phase(fom);
	int                     result   = M0_FSO_AGAIN;

	switch (phase) {
	case EP_PREPARE:
		isc_comp_prepare(comp_req, fom, EP_EXEC, EP_FAIL);
		break;
	case EP_EXEC:
		result = isc_comp_launch(comp_req, fom, EP_COMPLETE);
		break;
	case EP_FAIL:
		m0_fom_phase_set(fom, EP_FINI);
		result = M0_FSO_WAIT;
		break;
	case EP_COMPLETE:
		isc_comp_cleanup(fom, 0, EP_FINI);
		result = M0_FSO_WAIT;
		break;
	}
	return result;
}

static void ce_fom_fini(struct m0_fom *fom)
{
	struct m0_isc_comp_req *comp_req;

	comp_req = M0_AMB(comp_req, fom, icr_fom);
	m0_fom_fini(fom);
	m0_chan_broadcast_lock(&comp_req->icr_chan);
}

static const struct m0_fom_ops comp_exec_fom_ops = {
	.fo_home_locality = fom_home_locality,
	.fo_tick          = ce_fom_tick,
	.fo_fini          = ce_fom_fini
};

M0_INTERNAL void m0_isc_fom_type_init(void)
{
	m0_fom_type_init(&comp_exec_fom_type, M0_ISCSERVICE_EXEC_OPCODE,
			 &comp_exec_fom_type_ops, &m0_iscs_type,
			 &comp_exec_fom_states_conf);
}

static bool isc_comp_req_invariant(const struct m0_isc_comp_req *comp_req)
{
	return _0C(comp_req != NULL) &&
	       _0C(m0_fid_is_set(&comp_req->icr_comp_fid)) &&
	       _0C(comp_req->icr_reqh != NULL) &&
	       _0C(M0_IN(comp_req->icr_req_type, (M0_ICRT_LOCAL,
						  M0_ICRT_REMOTE)));

}

M0_INTERNAL int m0_isc_comp_req_exec(struct m0_isc_comp_req *comp_req)
{
	struct m0_reqh_service *svc_isc;
	int                     rc = 0;

	M0_ENTRY();

	M0_PRE(isc_comp_req_invariant(comp_req));

	svc_isc = m0_reqh_service_find(&m0_iscs_type, comp_req->icr_reqh);
	if (svc_isc == NULL)
		return M0_ERR_INFO(-EINVAL, "Service does not exist");
	m0_fom_init(&comp_req->icr_fom, &comp_exec_fom_type, &comp_exec_fom_ops,
		    NULL, NULL, comp_req->icr_reqh);
	m0_fom_queue(&comp_req->icr_fom);

	M0_LEAVE();
	return M0_RC(rc);
}

M0_INTERNAL int m0_isc_comp_req_exec_sync(struct m0_isc_comp_req *comp_req)
{
	struct m0_clink waiter;
	int             rc;

	M0_ENTRY();

	m0_clink_init(&waiter, NULL);
	m0_clink_add_lock(&comp_req->icr_chan, &waiter);
	waiter.cl_is_oneshot = true;
	rc = m0_isc_comp_req_exec(comp_req);
	if (rc != 0) {
		m0_clink_del_lock(&waiter);
		return M0_RC(rc);
	}
	m0_chan_wait(&waiter);
	M0_LEAVE();
	return M0_RC(rc);
}

M0_INTERNAL void m0_isc_comp_req_init(struct m0_isc_comp_req *comp_req,
				      const struct m0_buf *comp_args,
				      const struct m0_fid *comp_fid,
				      const struct m0_cookie *comp_cookie,
				      enum m0_isc_comp_req_type comp_req_type,
				      struct m0_reqh *reqh)
{
	M0_PRE(comp_req != NULL && comp_args != NULL && comp_fid != NULL &&
	       comp_cookie != NULL && reqh != NULL);

	M0_ENTRY();
	M0_SET0(comp_req);

	m0_buf_copy(&comp_req->icr_args, comp_args);
	comp_req->icr_comp_fid = *comp_fid;
	comp_req->icr_cookie   = *comp_cookie;
	comp_req->icr_reqh     = reqh;
	comp_req->icr_req_type = comp_req_type;
	comp_req->icr_comp_data.icp_fom = &comp_req->icr_fom;
	m0_chan_init(&comp_req->icr_chan, &comp_req->icr_guard);

	M0_POST(isc_comp_req_invariant(comp_req));
	M0_LEAVE();
}

static bool is_request_local(struct m0_isc_comp_req *comp_req)
{
	return comp_req->icr_req_type == M0_ICRT_LOCAL;
}

M0_INTERNAL void m0_isc_comp_req_fini(struct m0_isc_comp_req *comp_req)
{
	M0_PRE(comp_req != NULL);
	M0_ENTRY();

	m0_buf_free(&comp_req->icr_args);
	if (is_request_local(comp_req))
		m0_buf_free(&comp_req->icr_result);
	m0_chan_fini_lock(&comp_req->icr_chan);
	M0_LEAVE();
}

static int comp_register(struct m0_htable *comp_ht,
			 int (*ftn)(struct m0_buf *arg_in,
				    struct m0_buf *args_out,
				    struct m0_isc_comp_private
				    *comp_data, int *rc),
			 char *comp_name,
			 const struct m0_fid *fid)
{
	struct m0_isc_comp *comp;

	comp = m0_isc_htable_lookup(comp_ht, fid);
	if (comp != NULL)
		return M0_ERR_INFO(-EEXIST, "Computation %s already registered"
				    " for fid "FID_F, comp->ic_name,
				    FID_P(&comp->ic_fid));
	M0_ALLOC_PTR(comp);
	if (comp == NULL)
		return M0_ERR(-ENOMEM);
	comp->ic_fid  = *fid;
	comp->ic_op   = ftn;
	comp->ic_name = comp_name;
	m0_cookie_new(&comp->ic_gen);
	comp->ic_reg_state = M0_ICS_REGISTERED;
	m0_isc_tlink_init(comp);
	m0_isc_htable_add(comp_ht, comp);
	M0_LOG(M0_INFO, "Computation %s with id "FID_F" registered.",
	       comp->ic_name, FID_P(&comp->ic_fid));
	return 0;
}

M0_INTERNAL int m0_isc_comp_register(int (*ftn)(struct m0_buf *arg_in,
						struct m0_buf *args_out,
						struct m0_isc_comp_private
						*comp_data, int *rc),
				     const char *f_name,
				     const struct m0_fid *ftn_fid)
{
	struct m0_htable *comp_ht = m0_isc_htable_get();
	char             *comp_name;
	int               rc = 0;

	M0_PRE(ftn != NULL && ftn_fid != NULL);
	M0_PRE(m0_htable_is_init(comp_ht));
	M0_ENTRY();

	comp_name = m0_strdup(f_name);
	if (comp_name == NULL)
		return M0_ERR(-ENOMEM);
	m0_hbucket_lock(comp_ht, ftn_fid);
	rc = comp_register(comp_ht, ftn, comp_name, ftn_fid);
	m0_hbucket_unlock(comp_ht, ftn_fid);

	M0_LEAVE();
	return M0_RC(rc);
}

static void comp_unregister(struct m0_htable *comp_ht, const struct m0_fid *fid)
{
	struct m0_isc_comp *comp;

	comp = m0_isc_htable_lookup(comp_ht, fid);
	M0_ASSERT(comp != NULL);

	M0_LOG(M0_INFO, "Computation %s with id "FID_F" unregistered.",
	       comp->ic_name, FID_P(&comp->ic_fid));
	if (comp->ic_ref_count > 0) {
		comp->ic_reg_state = M0_ICS_UNREGISTERED;
		return;
	}
	comp_cleanup(comp_ht, comp);
	return;
}

M0_INTERNAL void m0_isc_comp_unregister(const struct m0_fid *fid)
{
	struct m0_htable *comp_ht = m0_isc_htable_get();
	struct m0_fid     comp_fid = *fid;

	M0_PRE(fid != NULL);
	M0_PRE(m0_htable_is_init(comp_ht));
	M0_ENTRY();

	m0_hbucket_lock(comp_ht, &comp_fid);
	comp_unregister(comp_ht, &comp_fid);
	m0_hbucket_unlock(comp_ht, &comp_fid);

	M0_LEAVE();
}

M0_INTERNAL int m0_isc_comp_state_probe(const struct m0_fid *fid)
{
	struct m0_htable   *comp_ht = m0_isc_htable_get();
	struct m0_isc_comp *comp;
	int                 state;

	M0_PRE(fid != NULL);
	M0_PRE(m0_htable_is_init(comp_ht));
	M0_ENTRY();

	m0_hbucket_lock(comp_ht, fid);
	comp = m0_isc_htable_lookup(comp_ht, fid);
	if (comp == NULL) {
		m0_hbucket_unlock(comp_ht, fid);
		return M0_ERR(-ENOENT);
	}
	state = comp->ic_reg_state;
	m0_hbucket_unlock(comp_ht, fid);
	M0_ASSERT(M0_IN(state, (M0_ICS_REGISTERED, M0_ICS_UNREGISTERED)));

	M0_LEAVE();
	return state;
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
