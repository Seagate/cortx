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
 * Original author: Rajesh Bhalerao <Rajesh_Bhalerao@xyratex.com>
 * Original creation date: 07/18/2011
 */

#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RM
#include "lib/errno.h"
#include "lib/list.h"
#include "lib/memory.h"
#include "lib/misc.h"   /* M0_IN */
#include "lib/trace.h"
#include "lib/finject.h"
#include "fop/fom_generic.h"
#include "rpc/service.h"
#include "rpc/rpc.h"
#include "reqh/reqh.h"
#include "conf/confc.h"     /* m0_confc */

#include "rm/rm_fops.h"
#include "rm/rm_foms.h"
#include "rm/rm_service.h"

/**
   @addtogroup rm

   This file includes data structures and function used by RM:fop layer.

   @{
 */

/**
 * Forward declaration
 */
static int   borrow_fom_create(struct m0_fop *fop, struct m0_fom **out,
			       struct m0_reqh *reqh);
static void   borrow_fom_fini(struct m0_fom *fom);
static int    revoke_fom_create(struct m0_fop *fop, struct m0_fom **out,
				struct m0_reqh *reqh);
static void   revoke_fom_fini(struct m0_fom *fom);
static int    cancel_fom_create(struct m0_fop *fop, struct m0_fom **out,
				struct m0_reqh *reqh);
static void   cancel_fom_fini(struct m0_fom *fom);
static int    borrow_fom_tick(struct m0_fom *);
static int    revoke_fom_tick(struct m0_fom *);
static int    cancel_process(struct m0_fom *fom);
static int    cancel_fom_tick(struct m0_fom *);
static size_t locality(const struct m0_fom *fom);

static void remote_incoming_complete(struct m0_rm_incoming *in, int32_t rc);
static void remote_incoming_conflict(struct m0_rm_incoming *in);

enum fop_request_type {
	FRT_BORROW = M0_RIT_BORROW,
	FRT_REVOKE = M0_RIT_REVOKE,
	FRT_CANCEL,
};

/*
 * As part of of incoming_complete(), call remote_incoming complete.
 * This will call request specific functions.
 */
static struct m0_rm_incoming_ops remote_incoming_ops = {
	.rio_complete = remote_incoming_complete,
	.rio_conflict = remote_incoming_conflict,
};

/*
 * Borrow FOM ops.
 */
static struct m0_fom_ops rm_fom_borrow_ops = {
	.fo_fini          = borrow_fom_fini,
	.fo_tick          = borrow_fom_tick,
	.fo_home_locality = locality,
};

const struct m0_fom_type_ops rm_borrow_fom_type_ops = {
	.fto_create = borrow_fom_create,
};

/*
 * Revoke FOM ops.
 */
static struct m0_fom_ops rm_fom_revoke_ops = {
	.fo_fini          = revoke_fom_fini,
	.fo_tick          = revoke_fom_tick,
	.fo_home_locality = locality,
};

const struct m0_fom_type_ops rm_revoke_fom_type_ops = {
	.fto_create = revoke_fom_create,
};

/*
 * Cancel FOM ops.
 */
static struct m0_fom_ops rm_fom_cancel_ops = {
	.fo_fini          = cancel_fom_fini,
	.fo_tick          = cancel_fom_tick,
	.fo_home_locality = locality,
};

const struct m0_fom_type_ops rm_cancel_fom_type_ops = {
	.fto_create = cancel_fom_create,
};

struct m0_sm_state_descr rm_req_phases[] = {
	[FOPH_RM_REQ_START] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "RM_Request_Begin",
		.sd_allowed = M0_BITS(FOPH_RM_REQ_CREDIT_GET,
				      FOPH_RM_REQ_DEBTOR_SUBSCRIBE,
				      FOPH_RM_REQ_FINISH)
	},
	[FOPH_RM_REQ_CREDIT_GET] = {
		.sd_name    = "RM_Credit_Get",
		.sd_allowed = M0_BITS(FOPH_RM_REQ_WAIT)
	},
	[FOPH_RM_REQ_WAIT] = {
		.sd_name    = "RM_Request_Wait",
		.sd_allowed = M0_BITS(FOPH_RM_REQ_FINISH)
	},
	[FOPH_RM_REQ_FINISH] = {
		.sd_flags   = M0_SDF_FINAL,
		.sd_name    = "RM_Request_Completion",
		.sd_allowed = 0
	},
	[FOPH_RM_REQ_DEBTOR_SUBSCRIBE] = {
		.sd_name    = "Debtor_Subscribe",
		.sd_allowed = M0_BITS(FOPH_RM_REQ_CREDIT_GET)
	},
};

const struct m0_sm_conf borrow_sm_conf = {
	.scf_name      = "Borrow fom",
	.scf_nr_states = ARRAY_SIZE(rm_req_phases),
	.scf_state     = rm_req_phases
};

const struct m0_sm_conf canoke_sm_conf = {
	.scf_name      = "Canoke fom",
	.scf_nr_states = ARRAY_SIZE(rm_req_phases),
	.scf_state     = rm_req_phases
};

static void remote_incoming_complete(struct m0_rm_incoming *in, int32_t rc)
{
	struct m0_rm_remote_incoming *rem_in;
	struct rm_request_fom	     *rqfom;

	rem_in = container_of(in, struct m0_rm_remote_incoming, ri_incoming);
	rqfom = container_of(rem_in, struct rm_request_fom, rf_in);
	M0_ASSERT(M0_IN(m0_fom_phase(&rqfom->rf_fom),
			(FOPH_RM_REQ_START, FOPH_RM_REQ_WAIT)));

	switch (in->rin_type) {
	case FRT_BORROW:
		rc = rc ?: m0_rm_borrow_commit(rem_in);
		break;
	case FRT_REVOKE:
		rc = rc ?: m0_rm_revoke_commit(rem_in);
		break;
	default:
		M0_IMPOSSIBLE("Unrecognized RM request");
		break;
	}

	/*
	 * Override the rc.
	 */
	in->rin_rc = rc;
	/*
	 * FOM may still executes, but for sure will be
	 * in the waiting state when wakeup AST is called.
	 */
	m0_fom_wakeup(&rqfom->rf_fom);
}

static void remote_incoming_conflict(struct m0_rm_incoming *in)
{
	/* Do nothing */
}

/*
 * Generic RM request-FOM constructor.
 */
static int request_fom_create(enum m0_rm_incoming_type type,
			      struct m0_fop *fop, struct m0_fom **out,
			      struct m0_reqh *reqh)
{
	struct rm_request_fom *rqfom;
	struct m0_fop_type    *fopt    = NULL;
	struct m0_fom_ops     *fom_ops = NULL;
	struct m0_fop	      *reply_fop;

	M0_ENTRY("creating FOM for request: %d", type);

	M0_PRE(fop != NULL);
	M0_PRE(fop->f_type != NULL);
	M0_PRE(out != NULL);

	M0_ALLOC_PTR(rqfom);
	if (M0_FI_ENABLED("fom_alloc_failure"))
		m0_free0(&rqfom);
	if (rqfom == NULL)
		return M0_ERR(-ENOMEM);

	switch (type) {
	case FRT_BORROW:
		fopt = &m0_rm_fop_borrow_rep_fopt;
		fom_ops = &rm_fom_borrow_ops;
		break;
	case FRT_REVOKE:
		fopt = &m0_rm_fop_revoke_rep_fopt;
		fom_ops = &rm_fom_revoke_ops;
		break;
	case FRT_CANCEL:
		fopt = &m0_fop_generic_reply_fopt;
		fom_ops = &rm_fom_cancel_ops;
		break;
	default:
		M0_IMPOSSIBLE("Unrecognised RM request");
		break;
	}

	reply_fop = m0_fop_reply_alloc(fop, fopt);
	if (reply_fop == NULL) {
		m0_free(rqfom);
		return M0_ERR(-ENOMEM);
	}

	m0_fom_init(&rqfom->rf_fom, &fop->f_type->ft_fom_type,
		    fom_ops, fop, reply_fop, reqh);
	*out = &rqfom->rf_fom;
	return M0_RC(0);
}

/*
 * Generic RM request-FOM destructor.
 */
static void request_fom_fini(struct m0_fom *fom)
{
	struct rm_request_fom *rfom;

	M0_PRE(fom != NULL);

	rfom = container_of(fom, struct rm_request_fom, rf_fom);
	m0_fom_fini(fom);
	m0_free(rfom);
}

static size_t locality(const struct m0_fom *fom)
{
	struct rm_request_fom *rfom;

	rfom = container_of(fom, struct rm_request_fom, rf_fom);
	return (size_t)(rfom->rf_in.ri_owner_cookie.co_generation >> 32);
}

/*
 * This function will fill reply FOP data.
 * However, it will not set the return code.
 *
 * @see reply_err_set()
 */
static int reply_prepare(const enum m0_rm_incoming_type type,
			 struct m0_fom *fom)
{
	struct m0_rm_fop_borrow_rep *breply_fop;
	struct m0_rm_fop_revoke_rep *rreply_fop;
	struct rm_request_fom       *rfom;
	struct m0_rm_loan           *loan;
	int                          rc = 0;

	M0_ENTRY("reply for fom: %p", fom);
	rfom = container_of(fom, struct rm_request_fom, rf_fom);

	switch (type) {
	case FRT_BORROW:
		breply_fop = m0_fop_data(fom->fo_rep_fop);
		breply_fop->br_loan.lo_cookie = rfom->rf_in.ri_loan_cookie;

		/*
		 * Get the loan pointer from the cookie to process the reply.
		 * It's safe to access loan as this get called when
		 * m0_credit_get() succeeds. Hence the loan cookie is valid.
		 */
		loan = m0_cookie_of(&rfom->rf_in.ri_loan_cookie,
				    struct m0_rm_loan, rl_id);

		M0_ASSERT(loan != NULL);
		m0_cookie_init(&breply_fop->br_creditor_cookie,
				&loan->rl_credit.cr_owner->ro_id);
		/*
		 * Memory for the buffer is allocated by the function.
		 */
		rc = m0_rm_credit_encode(&loan->rl_credit,
					 &breply_fop->br_credit.cr_opaque);
		break;
	case FRT_REVOKE:
		rreply_fop = m0_fop_data(fom->fo_rep_fop);
		rreply_fop->rr_debtor_cookie = rfom->rf_in.ri_rem_owner_cookie;
		break;
	default:
		break;
	}
	return M0_RC(rc);
}

/*
 * Set RM reply FOP error code.
 */
static void reply_err_set(enum m0_rm_incoming_type type,
			 struct m0_fom *fom, int rc)
{
	struct m0_rm_fop_borrow_rep *bfop;
	struct m0_rm_fop_revoke_rep *rfop;

	M0_ENTRY("reply for fom: %p type: %d error: %d", fom, type, rc);

	switch (type) {
	case FRT_BORROW:
		bfop = m0_fop_data(fom->fo_rep_fop);
		bfop->br_rc = rc;
		break;
	case FRT_REVOKE:
		rfop = m0_fop_data(fom->fo_rep_fop);
		rfop->rr_rc = rc;
		break;
	case FRT_CANCEL:
		break;
	default:
		M0_IMPOSSIBLE("Unrecognized RM request");
	}
	M0_LEAVE();
}

M0_INTERNAL int m0_rm_reverse_session_get(struct m0_rm_remote_incoming *rem_in,
					  struct m0_rm_remote          *remote)
{
	int                     rc = 0;
	struct rm_request_fom  *rfom;
	struct m0_fom          *fom;
	struct m0_reqh_service *service;
	struct m0_rpc_item     *item;

	M0_PRE(_0C(rem_in != NULL) && _0C(remote != NULL));
	M0_ENTRY("remote incoming %p remote %p", rem_in, remote);
	rfom = container_of(rem_in, struct rm_request_fom, rf_in);
	fom  = &rfom->rf_fom;
	item = &fom->fo_fop->f_item;
	if (item->ri_session != NULL) {
		service = m0_reqh_rpc_service_find(fom->fo_service->rs_reqh);
		M0_ASSERT(service != NULL);
		remote->rem_session =
			m0_rpc_service_reverse_session_lookup(service, item);
		if (remote->rem_session == NULL) {
			remote->rem_rev_sess_clink.cl_is_oneshot = true;
			rc = m0_rpc_service_reverse_session_get(
				service, item, &remote->rem_rev_sess_clink,
				&remote->rem_session);
		}
	}
	return M0_RC(rc);
}

/*
 * Build an remote-incoming structure using remote request information.
 */
static int incoming_prepare(enum m0_rm_incoming_type type, struct m0_fom *fom)
{
	struct m0_rm_fop_borrow    *bfop;
	struct m0_rm_fop_revoke    *rfop;
	struct m0_rm_fop_req	   *basefop = NULL;
	struct m0_rm_incoming	   *in;
	struct m0_rm_owner	   *owner;
	struct rm_request_fom	   *rfom;
	struct m0_buf		   *buf;
	enum m0_rm_incoming_policy  policy;
	uint64_t		    flags;
	int			    rc = 0;

	M0_ENTRY("prepare remote incoming request for fom: %p request: %d",
		 fom, type);

	rfom = container_of(fom, struct rm_request_fom, rf_fom);
	switch (type) {
	case FRT_BORROW:
		bfop = m0_fop_data(fom->fo_fop);
		basefop = &bfop->bo_base;
		/* Remote owner (requester) cookie */
		rfom->rf_in.ri_rem_owner_cookie = basefop->rrq_owner.ow_cookie;
		/*
		 * Populate the owner cookie for creditor (local)
		 * This is used later by locality().
		 */
		/* Possibly M0_COOKIE_NULL */
		rfom->rf_in.ri_owner_cookie = bfop->bo_creditor.ow_cookie;
		rfom->rf_in.ri_group_id = bfop->bo_group_id;
		break;

	case FRT_REVOKE:
		rfop = m0_fop_data(fom->fo_fop);
		basefop = &rfop->fr_base;
		/*
		 * Populate the owner cookie for debtor (local)
		 * This server is debtor; hence it received REVOKE request.
		 * This is used later by locality().
		 */
		rfom->rf_in.ri_owner_cookie = basefop->rrq_owner.ow_cookie;
		/*
		 * Populate the loan cookie.
		 */
		rfom->rf_in.ri_loan_cookie = rfop->fr_loan.lo_cookie;
		break;

	default:
		M0_IMPOSSIBLE("Unrecognized RM request");
		break;
	}
	policy = basefop->rrq_policy;
	flags = basefop->rrq_flags;
	buf = &basefop->rrq_credit.cr_opaque;
	in = &rfom->rf_in.ri_incoming;
	owner = m0_cookie_of(&rfom->rf_in.ri_owner_cookie,
			     struct m0_rm_owner, ro_id);
	/*
	 * Owner is NULL, create owner for given resource.
	 * Resource description is provided in basefop->rrq_owner.ow_resource
	 */
	if (owner == NULL) {
		/* Owner cannot be NULL for a revoke request */
		M0_ASSERT(type != (enum m0_rm_incoming_type)FRT_REVOKE);
		rc = m0_rm_svc_owner_create(fom->fo_service, &owner,
					    &basefop->rrq_owner.ow_resource);
		if (rc != 0) {
			m0_free(owner);
			return M0_RC(rc);
		}
	}
	/*
	 * Owner may be on its way of finalising, so no further credit operation
	 * makes sense with the owner, thus incoming request to be just rejected
	 *
	 * When not rejected, m0_rm_incoming finalisation is going to suffer
	 * from possible race in destruction of state machine lock object done
	 * in concurrent thread.
	 */
	if (owner_state(owner) > ROS_ACTIVE) {
		M0_LOG(M0_DEBUG, "Owner bad state: %d", owner_state(owner));
		return M0_ERR(-ESTALE);
	}
	m0_rm_incoming_init(in, owner, type, policy, flags);
	in->rin_ops = &remote_incoming_ops;
	in->rin_reserve.rrp_time = basefop->rrq_orig_time;
	in->rin_reserve.rrp_owner = basefop->rrq_orig_owner;
	in->rin_reserve.rrp_seq = basefop->rrq_orig_seq;
	rc = m0_rm_credit_decode(&in->rin_want, buf);
	if (rc != 0)
		m0_rm_incoming_fini(in);
	in->rin_want.cr_group_id = rfom->rf_in.ri_group_id;

	return M0_RC(rc);
}

static int remote_create(struct m0_rm_remote          **rem,
			 struct m0_rm_remote_incoming  *rem_in)
{
	struct m0_cookie      *cookie = &rem_in->ri_rem_owner_cookie;
	struct m0_rm_resource *res = incoming_to_resource(&rem_in->ri_incoming);
	struct m0_rm_remote   *other;
	int                    rc;

	M0_ALLOC_PTR(other);
	if (other != NULL) {
		m0_rm_remote_init(other, res);
		rc = m0_rm_reverse_session_get(rem_in, other);
		if (rc != 0) {
			m0_free(other);
			return M0_ERR(rc);
		}
		other->rem_state = REM_SERVICE_LOCATED;
		other->rem_cookie = *cookie;
		/* @todo - Figure this out */
		/* other->rem_id = 0; */
		m0_remotes_tlist_add(&res->r_remote, other);
	} else
		rc = M0_ERR(-ENOMEM);
	*rem = other;
	return M0_RC(rc);
}

/**
 * Asynchronously starts subscribing debtor to HA notification. This call makes
 * remote request FOM sm route to diverge and first locate conf object
 * corresponding to the remote object and install clink into the object's
 * channel, and only then continue dealing with credit processing.
 */
static int rfom_debtor_subscribe(struct rm_request_fom *rfom,
				 struct m0_rm_remote   *debtor)
{
	struct m0_confc       *confc;
	struct m0_rpc_item    *item;
	const char            *ep;
	struct m0_fom         *fom;
	int                    rc;

	M0_PRE(rfom != NULL);
	fom = &rfom->rf_fom;
	item = &fom->fo_fop->f_item;
	ep = m0_rpc_item_remote_ep_addr(item);
	M0_ASSERT(ep != NULL);
	/* Already subscribed? Then we are fine. */
	if (m0_clink_is_armed(&debtor->rem_tracker.rht_clink))
		return M0_RC(-EEXIST);
	/* get confc instance HA to be notifying on */
	confc = m0_reqh2confc(item->ri_rmachine->rm_reqh);
	rc = m0_rm_ha_subscriber_init(&rfom->rf_sbscr, &fom->fo_loc->fl_group,
				confc, ep, &debtor->rem_tracker);
	if (rc == 0) {
		m0_fom_phase_set(fom, FOPH_RM_REQ_DEBTOR_SUBSCRIBE);
		m0_fom_wait_on(fom, &rfom->rf_sbscr.rhs_sm.sm_chan,
			       &fom->fo_cb);
		m0_rm_ha_subscribe(&rfom->rf_sbscr);
	}

	return M0_RC(rc);
}

/*
 * Prepare incoming request. Send request for the credits.
 */
static int request_pre_process(struct m0_fom *fom,
			       enum m0_rm_incoming_type type)
{
	struct rm_request_fom *rfom;
	struct m0_rm_remote   *debtor;
	int                    rc;

	M0_ENTRY("pre-processing fom: %p request : %d", fom, type);

	M0_PRE(fom != NULL);
	rfom = container_of(fom, struct rm_request_fom, rf_fom);

	rc = incoming_prepare(type, fom);
	if (rc != 0) {
		/*
		 * This will happen if owner cookie is stale or
		 * copying of credit data fails.
		 */
		goto err;
	}

	if (type == M0_RIT_BORROW) {
		debtor = m0_rm_remote_find(&rfom->rf_in);
		if (debtor == NULL) {
			rc = remote_create(&debtor, &rfom->rf_in);
			if (rc != 0) {
				m0_rm_incoming_fini(&rfom->rf_in.ri_incoming);
				goto err;
			}
		} else if (debtor->rem_dead) {
			/*
			 * There is nobody to reply to, as request originator
			 * has been announced dead to the moment.
			 */
			rc = M0_ERR(-ECANCELED);
			m0_rm_incoming_fini(&rfom->rf_in.ri_incoming);
			goto err;
		}
		/*
		 * Subscribe debtor to HA notifications. If subscription
		 * fails, proceed as usual to the next step.
		 */
		if (!M0_FI_ENABLED("no-subscription")) {
			rc = rfom_debtor_subscribe(rfom, debtor);
			if (rc == 0)
				return M0_RC(M0_FSO_WAIT);
		}
	}

	m0_fom_phase_set(fom, FOPH_RM_REQ_CREDIT_GET);
	/*
	 * Don't try to analyse incoming_state(in), because access to 'in' is
	 * not synchronised here. Return always M0_FSO_WAIT, fom will wakeup
	 * from remote_incoming_complete().
	 */
	return M0_RC(M0_FSO_AGAIN);

err:
	reply_err_set(type, fom, rc);
	m0_rpc_reply_post(&fom->fo_fop->f_item,
			  m0_fop_to_rpc_item(fom->fo_rep_fop));
	m0_fom_phase_set(fom, FOPH_RM_REQ_FINISH);
	return M0_RC(M0_FSO_WAIT);
}

static int request_post_process(struct m0_fom *fom)
{
	struct rm_request_fom *rfom;
	struct m0_rm_incoming *in;
	int		       rc;

	M0_ENTRY("post-processing fom: %p", fom);
	M0_PRE(fom != NULL);

	rfom = container_of(fom, struct rm_request_fom, rf_fom);
	in = &rfom->rf_in.ri_incoming;

	rc = in->rin_rc;
	M0_ASSERT(ergo(rc == 0, incoming_state(in) == RI_SUCCESS));
	M0_ASSERT(ergo(rc != 0, incoming_state(in) == RI_FAILURE));
	/*
	 * Owner is allowed to be in ROS_FINAL state, otherwise its resource
	 * must be set up
	 */
	M0_ASSERT(owner_state(in->rin_want.cr_owner) == ROS_FINAL ||
		  incoming_to_resource(in) != NULL);

	if (incoming_state(in) == RI_SUCCESS) {
		rc = reply_prepare(in->rin_type, fom);
		m0_rm_credit_put(in);
	}

	reply_err_set(in->rin_type, fom, rc);
	m0_rm_incoming_fini(in);

	if (M0_FI_ENABLED("no_rpc_reply_post"))
		return M0_RC(M0_FSO_WAIT);

	m0_rpc_reply_post(&fom->fo_fop->f_item,
			  m0_fop_to_rpc_item(fom->fo_rep_fop));

	return M0_RC(M0_FSO_WAIT);
}

static int request_fom_tick(struct m0_fom           *fom,
			    enum m0_rm_incoming_type type)
{
	struct rm_request_fom *rfom;
	int                    rc = 0;

	M0_ENTRY("running fom: %p for request: %d", fom, type);

	switch (m0_fom_phase(fom)) {
	case FOPH_RM_REQ_START:
		if (type == (enum m0_rm_incoming_type)FRT_CANCEL)
			rc = cancel_process(fom);
		else
			rc = request_pre_process(fom, type);
		break;
	case FOPH_RM_REQ_CREDIT_GET:
		rfom = container_of(fom, struct rm_request_fom, rf_fom);
		m0_fom_phase_set(fom, FOPH_RM_REQ_WAIT);
		m0_rm_credit_get(&rfom->rf_in.ri_incoming);
		rc = M0_FSO_WAIT;
		break;
	case FOPH_RM_REQ_WAIT:
		rc = request_post_process(fom);
		m0_fom_phase_set(fom, FOPH_RM_REQ_FINISH);
		break;
	default:
		M0_IMPOSSIBLE("Unrecognized RM FOM phase");
		break;
	}
	return M0_RC(rc);
}

static int debtor_subscription_check(struct m0_fom *fom)
{
	struct rm_request_fom *rfom;
	int                    rc;

	rfom = container_of(fom, struct rm_request_fom, rf_fom);
	if (M0_IN(rfom->rf_sbscr.rhs_sm.sm_state,
		  (RM_HA_SBSCR_FINAL, RM_HA_SBSCR_FAILURE))) {
		if (rfom->rf_sbscr.rhs_sm.sm_rc != 0) {
			M0_LOG(M0_DEBUG, "Can't subscribe to debtor with ep %s",
			   m0_rm_remote_find(&rfom->rf_in)->rem_tracker.rht_ep);
		}
		m0_rm_ha_subscriber_fini(&rfom->rf_sbscr);
		/* Ignore subscriber return code, procced to getting credit */
		m0_fom_phase_set(fom, FOPH_RM_REQ_CREDIT_GET);
		rc = M0_FSO_AGAIN;
	}
	else {
		m0_fom_wait_on(fom, &rfom->rf_sbscr.rhs_sm.sm_chan,
			       &fom->fo_cb);
		rc = M0_FSO_WAIT;
	}
	return M0_RC(rc);
}

/**
 * This function handles the request to borrow a credit to a resource on
 * a server ("creditor").
 *
 * Prior to borrowing credit remote object (debtor) has to be subscribed to HA
 * notifications about conf object status change to handle debtor death
 * properly. This is done in FOPH_RM_REQ_DEBTOR_SUBSCRIBE phase.
 *
 * @param fom -> fom processing the CREDIT_BORROW request on the server
 *
 */
static int borrow_fom_tick(struct m0_fom *fom)
{
	int rc;

	M0_ENTRY("running fom: %p for request: %d", fom, FRT_BORROW);
	switch (m0_fom_phase(fom)) {
	case FOPH_RM_REQ_DEBTOR_SUBSCRIBE:
		rc = debtor_subscription_check(fom);
		break;
	default:
		rc = request_fom_tick(fom, FRT_BORROW);
	}
	return M0_RC(rc);
}

/**
 * This function handles the request to revoke a credit to a resource on
 * a server ("debtor"). REVOKE is typically issued to the client. In Mero,
 * resources are arranged in hierarchy (chain). Hence a server can receive
 * REVOKE from another server.
 *
 * @param fom -> fom processing the CREDIT_REVOKE request on the server
 *
 */
static int revoke_fom_tick(struct m0_fom *fom)
{
	return request_fom_tick(fom, FRT_REVOKE);
}

static int cancel_process(struct m0_fom *fom)
{
	struct m0_rm_fop_cancel  *cfop;
	struct m0_rm_loan        *loan;
	struct m0_rm_owner       *owner;
	int                       rc = 0;

	cfop = m0_fop_data(fom->fo_fop);

	owner = m0_cookie_of(&cfop->fc_creditor_cookie,
			     struct m0_rm_owner, ro_id);
	/* Creditors are alive as long as RM service is running */
	M0_ASSERT(owner != NULL);
	/* Lock owner to exclude races with processing HA notification */
	m0_rm_owner_lock(owner);
	loan = m0_cookie_of(&cfop->fc_loan.lo_cookie,
			    struct m0_rm_loan, rl_id);
	/*
	 * Loan can be absent if debtor is considered dead and this loan has
	 * already been revoked.
	 */
	if (loan != NULL) {
		M0_ASSERT(loan->rl_other != NULL);
		M0_ASSERT(!loan->rl_other->rem_dead);
		rc = m0_rm_loan_settle(owner, loan);
	} else {
		M0_LOG(M0_WARN, "loan %p is not found!",
				(void *)cfop->fc_loan.lo_cookie.co_addr);
		rc = -ENOENT;
	}
	reply_err_set(FRT_CANCEL, fom, rc);
	m0_rpc_reply_post(&fom->fo_fop->f_item,
			  m0_fop_to_rpc_item(fom->fo_rep_fop));
	m0_rm_owner_unlock(owner);

	m0_fom_phase_set(fom, FOPH_RM_REQ_FINISH);
	return M0_RC(M0_FSO_WAIT);
}

static int cancel_fom_tick(struct m0_fom *fom)
{
	return request_fom_tick(fom, FRT_CANCEL);
}

/*
 * A borrow FOM constructor.
 */
static int borrow_fom_create(struct m0_fop *fop, struct m0_fom **out,
			     struct m0_reqh *reqh)
{
	return request_fom_create(FRT_BORROW, fop, out, reqh);
}

/*
 * A borrow FOM destructor.
 */
static void borrow_fom_fini(struct m0_fom *fom)
{
	request_fom_fini(fom);
}

/*
 * A revoke FOM constructor.
 */
static int revoke_fom_create(struct m0_fop *fop, struct m0_fom **out,
			     struct m0_reqh *reqh)
{
	return request_fom_create(FRT_REVOKE, fop, out, reqh);
}

/*
 * A revoke FOM destructor.
 */
static void revoke_fom_fini(struct m0_fom *fom)
{
	request_fom_fini(fom);
}

/*
 * A cancel FOM constructor.
 */
static int cancel_fom_create(struct m0_fop *fop, struct m0_fom **out,
			     struct m0_reqh *reqh)
{
	return request_fom_create(FRT_CANCEL, fop, out, reqh);
}

/*
 * A cancel FOM destructor.
 */
static void cancel_fom_fini(struct m0_fom *fom)
{
	request_fom_fini(fom);
}

/** @} */

/**
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
