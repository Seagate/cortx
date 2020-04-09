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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RM
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/finject.h"
#include "rpc/item.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/rpc.h"
#include "rm/rm.h"
#include "rm/rm_fops.h"
#include "rm/rm_foms.h"
#include "rm/rm_fops_xc.h"
#include "rm/rm_service.h"
#include "rm/rm_internal.h"

/*
 * Data structures.
 */
/*
 * Tracking structure for outgoing request.
 */
struct rm_out {
	struct m0_rm_outgoing ou_req;
	struct m0_sm_ast      ou_ast;
	struct m0_fop	      ou_fop;
};

/**
 * Forward declaration.
 */
static void rm_reply_process(struct m0_rpc_item *item);
static void rm_borrow_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast);
static void rm_revoke_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast);
static void rm_cancel_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast);

static const struct m0_rpc_item_ops rm_request_rpc_ops = {
	.rio_replied = rm_reply_process,
};

/**
 * FOP definitions for resource-credit borrow request and reply.
 */
struct m0_fop_type m0_rm_fop_borrow_fopt;
struct m0_fop_type m0_rm_fop_borrow_rep_fopt;
extern struct m0_sm_state_descr rm_req_phases[];
extern struct m0_reqh_service_type m0_rpc_service_type;

/**
 * FOP definitions for resource-credit revoke request.
 */
struct m0_fop_type m0_rm_fop_revoke_fopt;
struct m0_fop_type m0_rm_fop_revoke_rep_fopt;

/**
 * FOP definitions for resource-credit cancel request.
 */
struct m0_fop_type m0_rm_fop_cancel_fopt;

/*
 * Extern FOM params
 */
extern const struct m0_fom_type_ops rm_borrow_fom_type_ops;
extern const struct m0_sm_conf      borrow_sm_conf;

extern const struct m0_fom_type_ops rm_revoke_fom_type_ops;
extern const struct m0_fom_type_ops rm_cancel_fom_type_ops;
extern const struct m0_sm_conf      canoke_sm_conf;

/*
 * Allocate and initialise remote request tracking structure.
 */
static int rm_out_create(struct rm_out           **out,
			 enum m0_rm_outgoing_type  otype,
			 struct m0_rm_remote      *other,
			 struct m0_rm_credit      *credit)
{
	struct rm_out *outreq;
	int            rc;

	M0_ENTRY();
	M0_PRE(out != NULL);
	M0_PRE(other != NULL);

	M0_ALLOC_PTR(outreq);
	if (outreq == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}
	rc = m0_rm_outgoing_init(&outreq->ou_req, otype, other, credit);
	if (rc != 0)
		m0_free(outreq);
	*out = outreq;
out:
	return M0_RC(rc);
}

/*
 * De-allocates the remote request tracking structure.
 */
static void rm_out_release(struct rm_out *out)
{
	m0_free(out);
}

static void rm_fop_release(struct m0_ref *ref)
{
	struct m0_fop *fop;
	struct rm_out *out;

	M0_ENTRY();
	M0_PRE(ref != NULL);
	fop = container_of(ref, struct m0_fop, f_ref);
	out = container_of(fop, struct rm_out, ou_fop);

	m0_fop_fini(fop);
	rm_out_release(out);
	M0_LEAVE();
}

static void rm_out_destroy(struct rm_out *out)
{
	M0_ENTRY();
	m0_rm_outgoing_fini(&out->ou_req);
	rm_out_release(out);
	M0_LEAVE();
}

static int fop_common_fill(struct rm_out         *outreq,
			   struct m0_rm_incoming *in,
			   struct m0_rm_credit   *credit,
			   struct m0_cookie      *cookie,
			   struct m0_fop_type    *fopt,
			   size_t                 offset,
			   void                 **data)
{
	struct m0_rm_fop_req *req;
	struct m0_fop        *fop;
	int                   rc;

	fop = &outreq->ou_fop;
	m0_fop_init(fop, fopt, NULL, rm_fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc == 0) {
		*data  = m0_fop_data(fop);
		req = (struct m0_rm_fop_req *) (char *)*data + offset;
		req->rrq_policy = in->rin_policy;
		req->rrq_flags = in->rin_flags;
		req->rrq_orig_time = in->rin_reserve.rrp_time;
		req->rrq_orig_owner = in->rin_reserve.rrp_owner;
		req->rrq_orig_seq = in->rin_reserve.rrp_seq;
		/*
		 * Set RIF_LOCAL_WAIT for remote requests if none of the
		 * RIF_LOCAL_WAIT, RIF_LOCAL_TRY is set, because only local
		 * users may resolve conflicts by some other means.
		 */
		if (!(in->rin_flags & WAIT_TRY_FLAGS))
			req->rrq_flags |= RIF_LOCAL_WAIT;
		req->rrq_owner.ow_cookie = *cookie;
		rc = m0_rm_resource_encode(incoming_to_resource(in),
					   &req->rrq_owner.ow_resource) ?:
			m0_rm_credit_encode(credit,
					    &req->rrq_credit.cr_opaque);
	} else {
		m0_fop_fini(fop);
	}

	return M0_RC(rc);
}

static int borrow_fop_fill(struct rm_out         *outreq,
			   struct m0_rm_incoming *in,
			   struct m0_rm_credit   *credit)
{
	struct m0_rm_fop_borrow *bfop;
	struct m0_cookie         cookie;
	int                      rc;

	M0_ENTRY("creating borrow fop for incoming: %p credit value: %llu",
		 in, (long long unsigned) credit->cr_datum);
	m0_cookie_init(&cookie, &in->rin_want.cr_owner->ro_id);
	rc = fop_common_fill(outreq, in, credit, &cookie,
			     &m0_rm_fop_borrow_fopt,
			     offsetof(struct m0_rm_fop_borrow, bo_base),
			     (void **)&bfop);

	if (rc == 0) {
		struct m0_rm_remote *rem = in->rin_want.cr_owner->ro_creditor;

		/* Copy creditor cookie */
		bfop->bo_creditor.ow_cookie = rem ? rem->rem_cookie :
			M0_COOKIE_NULL;
		bfop->bo_group_id = credit->cr_group_id;
	}
	return M0_RC(rc);
}

static int revoke_fop_fill(struct rm_out         *outreq,
			   struct m0_rm_incoming *in,
			   struct m0_rm_loan     *loan,
			   struct m0_rm_remote   *other,
			   struct m0_rm_credit   *credit)
{
	struct m0_rm_fop_revoke *rfop;
	int			 rc = 0;

	M0_ENTRY("creating revoke fop for incoming: %p credit value: %llu",
		 in, (long long unsigned) credit->cr_datum);

	rc = fop_common_fill(outreq, in, credit, &other->rem_cookie,
			     &m0_rm_fop_revoke_fopt,
			     offsetof(struct m0_rm_fop_revoke, fr_base),
			     (void **)&rfop);
	if (rc == 0)
		rfop->fr_loan.lo_cookie = loan->rl_cookie;

	return M0_RC(rc);
}

static int cancel_fop_fill(struct rm_out     *outreq,
			   struct m0_rm_loan *loan)
{
	struct m0_rm_fop_cancel *cfop;
	struct m0_fop           *fop;
	int                      rc;

	M0_ENTRY("creating cancel fop for credit value: %llu",
		 (long long unsigned) loan->rl_credit.cr_datum);

	fop = &outreq->ou_fop;
	m0_fop_init(fop, &m0_rm_fop_cancel_fopt, NULL, rm_fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc == 0) {
		cfop = m0_fop_data(fop);
		cfop->fc_loan.lo_cookie = loan->rl_cookie;
		cfop->fc_creditor_cookie = loan->rl_other->rem_cookie;
	} else {
		m0_fop_fini(fop);
	}
	return M0_RC(rc);
}

static void outreq_fini(struct rm_out *outreq, int rc)
{
	outreq->ou_req.rog_rc = rc;
	m0_rm_outgoing_complete(&outreq->ou_req);
	m0_fop_put_lock(&outreq->ou_fop);
}

M0_INTERNAL void m0_rm_outgoing_send(struct m0_rm_outgoing *outgoing)
{
	struct rm_out      *outreq;
	struct m0_rpc_item *item;
	int                 rc;

	M0_ENTRY("outgoing: %p", outgoing);
	M0_PRE(outgoing->rog_sent == false);

	outreq = M0_AMB(outreq, outgoing, ou_req);
	M0_ASSERT(outreq->ou_ast.sa_cb == NULL);
	M0_ASSERT(outreq->ou_fop.f_item.ri_session == NULL);

	switch (outgoing->rog_type) {
	case M0_ROT_BORROW:
		outreq->ou_ast.sa_cb = &rm_borrow_ast;
		break;
	case M0_ROT_REVOKE:
		outreq->ou_ast.sa_cb = &rm_revoke_ast;
		break;
	case M0_ROT_CANCEL:
		outreq->ou_ast.sa_cb = &rm_cancel_ast;
		break;
	default:
		break;
	}

	item = &outreq->ou_fop.f_item;
	item->ri_session = outgoing->rog_want.rl_other->rem_session;
	item->ri_ops     = &rm_request_rpc_ops;

	M0_LOG(M0_DEBUG, "%p[%u] sending request:%p over session: %p",
	       item, item->ri_type->rit_opcode, outreq, item->ri_session);

	if (M0_FI_ENABLED("no-rpc"))
		return;

	rc = m0_rpc_post(item);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "%p[%u], request %p could not be posted, "
		       "rc %d", item, item->ri_type->rit_opcode, outgoing, rc);
		m0_sm_ast_cancel(
			owner_grp(outreq->ou_req.rog_want.rl_credit.cr_owner),
			&outreq->ou_ast);
		outreq_fini(outreq, rc);
		return;
	}
	outgoing->rog_sent = true;
	M0_LEAVE();
}

static void outgoing_queue(enum m0_rm_outgoing_type  otype,
			   struct m0_rm_owner       *owner,
			   struct rm_out            *outreq,
			   struct m0_rm_incoming    *in,
			   struct m0_rm_remote      *other)
{
	M0_PRE(owner != NULL);

	if (in != NULL)
		m0_rm_pin_add(in, &outreq->ou_req.rog_want.rl_credit,
			      M0_RPF_TRACK);

	m0_rm_ur_tlist_add(&owner->ro_outgoing[OQS_GROUND],
			   &outreq->ou_req.rog_want.rl_credit);
	/*
	 * It is possible that remote session is not yet established
	 * when revoke request should be sent.
	 * In this case let's wait till session is established and
	 * send revoke request from rev_session_clink_cb callback.
	 *
	 * The race is possible if m0_clink_is_armed() return false, but
	 * rev_session_clink_cb() call is not finished yet. In that case
	 * outgoing request could be sent twice.
	 * Flag m0_rm_outgoing::rog_sent is used to prevent that.
	 */
	if (otype != M0_ROT_REVOKE ||
	    !m0_clink_is_armed(&other->rem_rev_sess_clink))
		m0_rm_outgoing_send(&outreq->ou_req);
}

M0_INTERNAL int m0_rm_request_out(enum m0_rm_outgoing_type otype,
				  struct m0_rm_incoming   *in,
				  struct m0_rm_loan       *loan,
				  struct m0_rm_credit     *credit,
				  struct m0_rm_remote     *other)
{
	struct rm_out *outreq;
	int            rc;

	M0_ENTRY("sending request type: %d for incoming: %p credit value: %llu",
		 otype, in, (long long unsigned) credit->cr_datum);
	M0_PRE(M0_IN(otype, (M0_ROT_BORROW, M0_ROT_REVOKE, M0_ROT_CANCEL)));
	M0_PRE(ergo(M0_IN(otype, (M0_ROT_REVOKE, M0_ROT_CANCEL)),
		    loan != NULL));

	rc = rm_out_create(&outreq, otype, other, credit);
	if (rc != 0)
		return M0_ERR(rc);

	if (loan != NULL)
		outreq->ou_req.rog_want.rl_cookie = loan->rl_cookie;

	switch (otype) {
	case M0_ROT_BORROW:
		rc = borrow_fop_fill(outreq, in, credit);
		break;
	case M0_ROT_REVOKE:
		rc = revoke_fop_fill(outreq, in, loan, other, credit);
		break;
	case M0_ROT_CANCEL:
		rc = cancel_fop_fill(outreq, loan);
		break;
	default:
		rc = -EINVAL;
		M0_IMPOSSIBLE("Unrecognized RM request");
	}

	if (rc == 0) {
		outgoing_queue(otype, credit->cr_owner, outreq, in, other);
	} else {
		M0_LOG(M0_ERROR, "filling fop failed: rc=%d", rc);
		rm_out_destroy(outreq);
	}
	return M0_RC(rc);
}

static void rm_borrow_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct rm_out               *outreq = M0_AMB(outreq, ast, ou_ast);
	const struct m0_rpc_item    *item = &outreq->ou_fop.f_item;
	struct m0_rm_fop_borrow_rep *borrow_reply;
	struct m0_rm_owner          *owner;
	struct m0_rm_loan           *loan = NULL;
	struct m0_rm_credit         *credit;
	struct m0_rm_credit         *bcredit;
	struct m0_buf                buf;
	int                          rc;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(&grp->s_lock));

	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		borrow_reply = m0_fop_data(m0_rpc_item_to_fop(item->ri_reply));
		rc = borrow_reply->br_rc;
	}
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Borrow request %p failed: rc=%d", outreq, rc);
		goto out;
	}
	M0_ASSERT(borrow_reply != NULL);
	bcredit = &outreq->ou_req.rog_want.rl_credit;
	owner   = bcredit->cr_owner;
	if (m0_cookie_is_null(&owner->ro_creditor->rem_cookie))
		owner->ro_creditor->rem_cookie =
			borrow_reply->br_creditor_cookie;
	/* Get the data for a credit from the FOP */
	m0_buf_init(&buf, borrow_reply->br_credit.cr_opaque.b_addr,
		    borrow_reply->br_credit.cr_opaque.b_nob);
	rc = m0_rm_credit_decode(bcredit, &buf);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "credit decode for request %p failed: rc=%d",
		       outreq, rc);
		goto out;
	}
	rc = m0_rm_credit_dup(bcredit, &credit) ?:
		m0_rm_loan_alloc(&loan, bcredit, owner->ro_creditor) ?:
		granted_maybe_reserve(bcredit, credit);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "borrowed loan/credit allocation request %p"
		       " failed: rc=%d", outreq, rc);
		m0_free(loan);
		m0_free(credit);
		goto out;
	}
	M0_LOG(M0_INFO, "borrow request %p successful; credit value: %llu",
	       outreq, (long long unsigned)credit->cr_datum);
	loan->rl_cookie = borrow_reply->br_loan.lo_cookie;
	/* Add loan to the borrowed list. */
	m0_rm_ur_tlist_add(&owner->ro_borrowed, &loan->rl_credit);
	/* Add credit to the CACHED list. */
	m0_rm_ur_tlist_add(&owner->ro_owned[OWOS_CACHED], credit);
out:
	outreq_fini(outreq, rc);
	M0_LEAVE();
}

static void rm_revoke_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct rm_out               *outreq = M0_AMB(outreq, ast, ou_ast);
	const struct m0_rpc_item    *item = &outreq->ou_fop.f_item;
	struct m0_rm_loan           *loan;
	struct m0_rm_fop_revoke_rep *revoke_reply;
	int                rc;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(&grp->s_lock));

	loan = &outreq->ou_req.rog_want;
	if (loan->rl_other->rem_dead) {
		/*
		 * Remote owner is considered failed by HA. All loans are
		 * already revoked in rm_remote_death_ast(). Set result code to
		 * SUCCESS, since loan is already settled and goto out to
		 * not try settle it once more.
		 */
		rc = 0;
		goto out;
	}
	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		revoke_reply = m0_fop_data(m0_rpc_item_to_fop(item->ri_reply));
		rc = revoke_reply->rr_rc;
	}
	if (rc != 0) {
		M0_LOG(M0_ERROR, "revoke request %p failed: rc=%d", outreq, rc);
		goto out;
	}
	rc = m0_rm_loan_settle(loan->rl_credit.cr_owner, loan);
out:
	outreq_fini(outreq, rc);
	M0_LEAVE();
}

static void rm_cancel_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct rm_out       *outreq = M0_AMB(outreq, ast, ou_ast);
	struct m0_rm_owner  *owner;
	struct m0_rm_loan   *loan;
	struct m0_rm_credit *credit;
	int                  rc;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(&grp->s_lock));

	rc = m0_rpc_item_error(&outreq->ou_fop.f_item);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "cancel request %p failed: rc=%d", outreq, rc);
		goto out;
	}
	owner = outreq->ou_req.rog_want.rl_credit.cr_owner;
	loan = &outreq->ou_req.rog_want;
	m0_tl_for (m0_rm_ur, &owner->ro_owned[OWOS_CACHED], credit) {
		if (credit->cr_ops->cro_intersects(credit, &loan->rl_credit))
			m0_rm_ur_tlist_del(credit);
	} m0_tl_endfor;
	rc = m0_rm_owner_loan_debit(owner, loan, &owner->ro_borrowed);
out:
	outreq_fini(outreq, rc);
	M0_LEAVE();
}

static void rm_reply_process(struct m0_rpc_item *item)
{
	struct m0_rm_owner *owner;
	struct rm_out      *outreq;

	M0_ENTRY();
	M0_PRE(item != NULL);

	/*
	 * Note that RPC errors are handled in the corresponding AST callback:
	 * rm_borrow_ast(), rm_revoke_ast(), rm_cancel_ast().
	 */
	M0_ASSERT(ergo(item->ri_error == 0, item->ri_reply != NULL));
	outreq = M0_AMB(outreq, m0_rpc_item_to_fop(item), ou_fop);
	owner = outreq->ou_req.rog_want.rl_credit.cr_owner;

	M0_ASSERT(outreq->ou_ast.sa_cb != NULL);
	m0_sm_ast_post(owner_grp(owner), &outreq->ou_ast);
	M0_LEAVE();
}

M0_INTERNAL void m0_rm_fop_fini(void)
{
	m0_fop_type_fini(&m0_rm_fop_cancel_fopt);
	m0_fop_type_fini(&m0_rm_fop_revoke_rep_fopt);
	m0_fop_type_fini(&m0_rm_fop_revoke_fopt);
	m0_fop_type_fini(&m0_rm_fop_borrow_rep_fopt);
	m0_fop_type_fini(&m0_rm_fop_borrow_fopt);
}
M0_EXPORTED(m0_rm_fop_fini);

/**
 * Initialises RM fops.
 * @see rm_fop_fini()
 */
M0_INTERNAL int m0_rm_fop_init(void)
{
	M0_FOP_TYPE_INIT(&m0_rm_fop_borrow_fopt,
			 .name      = "Credit Borrow",
			 .opcode    = M0_RM_FOP_BORROW,
			 .xt        = m0_rm_fop_borrow_xc,
			 .sm	    = &borrow_sm_conf,
			 .fom_ops   = &rm_borrow_fom_type_ops,
			 .svc_type  = &m0_rms_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_rm_fop_borrow_rep_fopt,
			 .name      = "Credit Borrow Reply",
			 .opcode    = M0_RM_FOP_BORROW_REPLY,
			 .xt        = m0_rm_fop_borrow_rep_xc,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_rm_fop_revoke_fopt,
			 .name      = "Credit Revoke",
			 .opcode    = M0_RM_FOP_REVOKE,
			 .xt        = m0_rm_fop_revoke_xc,
			 .sm	    = &canoke_sm_conf,
			 .fom_ops   = &rm_revoke_fom_type_ops,
			 .svc_type  = &m0_rms_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_rm_fop_revoke_rep_fopt,
			 .name      = "Credit Revoke Reply",
			 .opcode    = M0_RM_FOP_REVOKE_REPLY,
			 .xt        = m0_rm_fop_revoke_rep_xc,
			 .svc_type  = &m0_rpc_service_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_rm_fop_cancel_fopt,
			 .name      = "Credit Return (Cancel)",
			 .opcode    = M0_RM_FOP_CANCEL,
			 .xt        = m0_rm_fop_cancel_xc,
			 .sm	    = &canoke_sm_conf,
			 .fom_ops   = &rm_cancel_fom_type_ops,
			 .svc_type  = &m0_rms_type,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	return 0;
}
M0_EXPORTED(m0_rm_fop_init);

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
