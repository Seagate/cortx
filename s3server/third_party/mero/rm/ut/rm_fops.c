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
 * Original creation date: 07/23/2012
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "ut/ut.h"
#include "rm/rm.h"
#include "rm/rm_fops.h"
#include "rm/ut/rmut.h"
#include "rm/ut/rings.h"
#include "rm/rm_internal.h"      /* m0_rm_outgoing_fini */
#include "rm/rm_fops.c"          /* To access static APIs. */

static struct m0_rm_loan *test_loan;
static struct m0_rm_remote remote;
static struct m0_rpc_machine ut_rm_mach;

static void post_borrow_validate(int err);
static void borrow_reply_populate(struct m0_rm_fop_borrow_rep *breply,
				  int err);
static void post_borrow_cleanup(struct m0_rpc_item *item, int err);
static void borrow_fop_validate(struct m0_rm_fop_borrow *bfop);
static void post_revoke_validate(int err);
static void revoke_fop_validate(struct m0_rm_fop_revoke *rfop);
static void post_revoke_cleanup(struct m0_rpc_item *item, int err);
static void revoke_reply_populate(struct m0_fop_generic_reply *rreply,
				  int err);

/*
 *****************
 * Common test functions for test cases in this file.
 ******************
 */

/*
 * Prepare parameters (request) for testing RM-FOP-send functions.
 */
static void request_param_init(enum m0_rm_incoming_flags flags)
{
	M0_SET0(&rm_test_data.rd_in);
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);

	m0_rm_credit_init(&rm_test_data.rd_in.rin_want, rm_test_data.rd_owner);
	rm_test_data.rd_in.rin_want.cr_datum = NENYA;
	rm_test_data.rd_in.rin_ops = &rings_incoming_ops;

	M0_SET0(&rm_test_data.rd_credit);
	m0_rm_credit_init(&rm_test_data.rd_credit, rm_test_data.rd_owner);
	rm_test_data.rd_credit.cr_datum = NENYA;
	rm_test_data.rd_credit.cr_ops = &rings_credit_ops;

	M0_ALLOC_PTR(rm_test_data.rd_owner->ro_creditor);
	M0_UT_ASSERT(rm_test_data.rd_owner->ro_creditor != NULL);
	m0_rm_remote_init(rm_test_data.rd_owner->ro_creditor,
			  rm_test_data.rd_owner->ro_resource);
	m0_cookie_init(&rm_test_data.rd_owner->ro_creditor->rem_cookie,
		       &rm_test_data.rd_owner->ro_id);
	M0_ALLOC_PTR(test_loan);
	M0_UT_ASSERT(test_loan != NULL);
	m0_cookie_init(&test_loan->rl_cookie, &test_loan->rl_id);
	remote.rem_state = REM_FREED;
	M0_SET0(&remote);
	m0_rm_remote_init(&remote, rm_test_data.rd_res);
	m0_cookie_init(&remote.rem_cookie, &rm_test_data.rd_owner->ro_id);
	m0_rm_loan_init(test_loan, &rm_test_data.rd_credit, &remote);
	m0_cookie_init(&test_loan->rl_cookie, &test_loan->rl_id);
}

/*
 * Finalise parameters (request) parameters for testing RM-FOP-send functions.
 */
static void request_param_fini(void)
{
	if (test_loan != NULL)
		m0_rm_loan_fini(test_loan);
	m0_rm_remote_fini(&remote);
	m0_free(test_loan);
	m0_rm_remote_fini(rm_test_data.rd_owner->ro_creditor);
	m0_free0(&rm_test_data.rd_owner->ro_creditor);
}

/*
 * Validate local wait/try flags for RM FOP
 */
static void wait_try_flags_validate(uint64_t orig_flags,
				    uint64_t rm_fop_flags)
{
	if (orig_flags & RIF_LOCAL_WAIT) {
		M0_UT_ASSERT(rm_fop_flags & RIF_LOCAL_WAIT);
		M0_UT_ASSERT(!(rm_fop_flags & RIF_LOCAL_TRY));
	} else if (orig_flags & RIF_LOCAL_TRY) {
		M0_UT_ASSERT(!(rm_fop_flags & RIF_LOCAL_WAIT));
		M0_UT_ASSERT(rm_fop_flags & RIF_LOCAL_TRY);
	} else {
		M0_UT_ASSERT(rm_fop_flags & RIF_LOCAL_WAIT);
		M0_UT_ASSERT(!(rm_fop_flags & RIF_LOCAL_TRY));
	}
}

/*
 * Validate FOP and other data structures after RM-FOP-send function is called.
 */
static void rm_req_fop_validate(enum m0_rm_incoming_type reqtype)
{
	struct m0_rm_fop_borrow *bfop;
	struct m0_rm_fop_revoke *rfop;
	struct m0_rm_pin	*pin;
	struct m0_rm_loan	*loan;
	struct m0_rm_outgoing	*og;
	struct rm_out		*oreq;
	uint32_t                 pins_nr = 0;

	m0_tl_for(pi, &rm_test_data.rd_in.rin_pins, pin) {

		M0_UT_ASSERT(pin->rp_flags == M0_RPF_TRACK);
		loan = bob_of(pin->rp_credit, struct m0_rm_loan,
			      rl_credit, &loan_bob);
		og = container_of(loan, struct m0_rm_outgoing, rog_want);
		oreq = container_of(og, struct rm_out, ou_req);

		switch (reqtype) {
		case M0_RIT_BORROW:
			bfop = m0_fop_data(&oreq->ou_fop);
			borrow_fop_validate(bfop);
			break;
		case M0_RIT_REVOKE:
			rfop = m0_fop_data(&oreq->ou_fop);
			revoke_fop_validate(rfop);
			break;
		default:
			break;
		}
		m0_rm_ur_tlist_del(pin->rp_credit);
		pi_tlink_del_fini(pin);
		pr_tlink_del_fini(pin);
		m0_free(pin);
		m0_rm_outgoing_fini(&oreq->ou_req);
		rm_fop_release(&oreq->ou_fop.f_ref);
		++pins_nr;
	} m0_tl_endfor;
	M0_UT_ASSERT(pins_nr == 1);
}

/*
 * Create reply for each FOP.
 */
static struct m0_rpc_item *rm_reply_create(enum m0_rm_incoming_type reqtype,
					   int err)
{
	struct m0_fop_generic_reply *rreply;
	struct m0_rm_fop_borrow_rep *breply;
	struct m0_fop_type          *fopt = NULL;
	struct m0_fop		    *fop;
	struct m0_rm_pin	    *pin;
	struct m0_rm_loan	    *loan;
	struct m0_rm_outgoing	    *og;
	struct m0_rpc_item	    *item = NULL;
	struct m0_rm_owner	    *owner;
	struct rm_out		    *oreq;
	uint32_t		     pins_nr = 0;

	m0_tl_for(pi, &rm_test_data.rd_in.rin_pins, pin) {

		M0_UT_ASSERT(pin->rp_flags == M0_RPF_TRACK);
		loan = bob_of(pin->rp_credit, struct m0_rm_loan,
			      rl_credit, &loan_bob);
		og = container_of(loan, struct m0_rm_outgoing, rog_want);
		oreq = container_of(og, struct rm_out, ou_req);

		switch (reqtype) {
		case M0_RIT_BORROW:
			fopt = &m0_rm_fop_borrow_rep_fopt;
			break;
		case M0_RIT_REVOKE:
			fopt = &m0_fop_generic_reply_fopt;
			break;
		default:
			break;
		}
		fop = m0_fop_alloc(fopt, NULL, &ut_rm_mach);
		M0_UT_ASSERT(fop != NULL);
		item = &oreq->ou_fop.f_item;
		switch (reqtype) {
		case M0_RIT_BORROW:
			breply = m0_fop_data(fop);
			borrow_reply_populate(breply, err);
			item->ri_reply = &fop->f_item;
			item->ri_rmachine = &ut_rm_mach;
			break;
		case M0_RIT_REVOKE:
			rreply = m0_fop_data(fop);
			revoke_reply_populate(rreply, err);
			item->ri_reply = &fop->f_item;
			item->ri_rmachine = &ut_rm_mach;
			break;
		default:
			break;
		}
		owner = og->rog_want.rl_credit.cr_owner;
		/* Delete the pin so that owner_balance() does not process it */
		m0_rm_owner_lock(owner);
		pi_tlink_del_fini(pin);
		pr_tlink_del_fini(pin);
		m0_rm_owner_unlock(owner);
		m0_free(pin);
		++pins_nr;
	} m0_tl_endfor;
	M0_UT_ASSERT(pins_nr == 1);

	return item;
}

/*
 * Test a reply FOP.
 */
static void reply_test(enum m0_rm_incoming_type reqtype, int err)
{
	struct m0_rpc_item  *item;
	struct m0_rm_remote *other;
	int                  rc;

	request_param_init(RIF_LOCAL_WAIT);

	m0_fi_enable_once("m0_rm_outgoing_send", "no-rpc");
	switch (reqtype) {
	case M0_RIT_BORROW:
		rm_test_data.rd_in.rin_flags |= RIF_MAY_BORROW;
		other = rm_test_data.rd_owner->ro_creditor;
		rc = m0_rm_request_out(M0_ROT_BORROW, &rm_test_data.rd_in, NULL,
				       &rm_test_data.rd_credit, other);
		M0_UT_ASSERT(rc == 0);
		item = rm_reply_create(M0_RIT_BORROW, err);
		rm_reply_process(item);
		/* Lock and unlock the owner to run AST */
		m0_rm_owner_lock(rm_test_data.rd_owner);
		m0_rm_owner_unlock(rm_test_data.rd_owner);
		post_borrow_validate(err);
		post_borrow_cleanup(item, err);
		break;
	case M0_RIT_REVOKE:
		rm_test_data.rd_in.rin_flags |= RIF_MAY_REVOKE;
		rc = m0_rm_request_out(M0_ROT_REVOKE, &rm_test_data.rd_in,
				       test_loan, &test_loan->rl_credit,
				       &remote);
		M0_UT_ASSERT(rc == 0);
		item = rm_reply_create(M0_RIT_REVOKE, err);
		m0_rm_owner_lock(rm_test_data.rd_owner);
		m0_rm_ur_tlist_add(&rm_test_data.rd_owner->ro_sublet,
				   &test_loan->rl_credit);
		m0_rm_owner_unlock(rm_test_data.rd_owner);
		rm_reply_process(item);
		/* Lock and unlock the owner to run AST */
		m0_rm_owner_lock(rm_test_data.rd_owner);
		m0_rm_owner_unlock(rm_test_data.rd_owner);
		post_revoke_validate(err);
		post_revoke_cleanup(item, err);
		/*
		 * When revoke succeeds, loan is de-allocated through
		 * revoke_reply(). If revoke fails, post_revoke_cleanup(),
		 * de-allocates the loan. Set test_loan to NULL. Otherwise,
		 * it will result in double free().
		 */
		test_loan = NULL;
		break;
	default:
		M0_IMPOSSIBLE("Invalid RM-FOM type");
	}

	request_param_fini();
}

/*
 * Test a request FOP.
 */
static void request_test(enum m0_rm_incoming_type reqtype)
{
	struct m0_rm_credit       rest;
	enum m0_rm_incoming_flags flags[] = {0, RIF_LOCAL_TRY, RIF_LOCAL_WAIT};
	int                       rc;
	int                       i;

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		request_param_init(flags[i]);

		m0_fi_enable_once("m0_rm_outgoing_send", "no-rpc");
		m0_rm_credit_init(&rest, rm_test_data.rd_owner);
		rc = rest.cr_ops->cro_copy(&rest, &rm_test_data.rd_credit);
		M0_UT_ASSERT(rc == 0);
		switch (reqtype) {
		case M0_RIT_BORROW:
			rm_test_data.rd_in.rin_flags |= RIF_MAY_BORROW;
			rc = m0_rm_request_out(M0_ROT_BORROW,
					       &rm_test_data.rd_in, NULL,
					       &rest, &remote);
			break;
		case M0_RIT_REVOKE:
			rm_test_data.rd_in.rin_flags |= RIF_MAY_REVOKE;
			rc = m0_rm_request_out(M0_ROT_REVOKE,
					       &rm_test_data.rd_in,
					       test_loan, &rest, &remote);
			break;
		default:
			M0_IMPOSSIBLE("Invalid RM-FOM type");
		}
		M0_UT_ASSERT(rc == 0);
		m0_rm_credit_fini(&rest);

		rm_req_fop_validate(reqtype);
		request_param_fini();
	}
}

/*
 *****************
 * RM Borrow-FOP test functions
 ******************
 */
/*
 * Validates the owner lists after receiving BORROW-reply.
 */
static void post_borrow_validate(int err)
{
	bool got_credit;

	m0_rm_owner_lock(rm_test_data.rd_owner);
	got_credit = !m0_rm_ur_tlist_is_empty(
			&rm_test_data.rd_owner->ro_borrowed) &&
		    !m0_rm_ur_tlist_is_empty(
			&rm_test_data.rd_owner->ro_owned[OWOS_CACHED]);
	m0_rm_owner_unlock(rm_test_data.rd_owner);
	M0_UT_ASSERT(ergo(err, !got_credit));
	M0_UT_ASSERT(ergo(err == 0, got_credit));
}

static void borrow_reply_populate(struct m0_rm_fop_borrow_rep *breply,
				  int err)
{
	int rc;

	breply->br_rc = err;

	if (err == 0) {
		rc = m0_rm_credit_encode(&rm_test_data.rd_credit,
					&breply->br_credit.cr_opaque);
		M0_UT_ASSERT(rc == 0);
	}
}

static void post_borrow_cleanup(struct m0_rpc_item *item, int err)
{
	struct m0_rm_credit *credit;
	struct m0_rm_loan   *loan;
	struct rm_out       *outreq;

	outreq = container_of(m0_rpc_item_to_fop(item), struct rm_out, ou_fop);
	loan = &outreq->ou_req.rog_want;
	/*
	 * A borrow error leaves the owner lists unaffected.
	 * If borrow succeeds, the owner lists are updated. Hence they
	 * need to be cleaned-up.
	 */
	if (err)
		return;

	m0_rm_owner_lock(rm_test_data.rd_owner);
	m0_tl_for(m0_rm_ur, &rm_test_data.rd_owner->ro_owned[OWOS_CACHED],
			credit) {
		m0_rm_ur_tlink_del_fini(credit);
		m0_rm_credit_fini(credit);
		m0_free(credit);
	} m0_tl_endfor;

	m0_tl_for(m0_rm_ur, &rm_test_data.rd_owner->ro_borrowed, credit) {
		m0_rm_ur_tlink_del_fini(credit);
		loan = bob_of(credit, struct m0_rm_loan, rl_credit, &loan_bob);
		m0_rm_loan_fini(loan);
		m0_free(loan);
	} m0_tl_endfor;
	m0_rm_owner_unlock(rm_test_data.rd_owner);
}

/*
 * Check if m0_rm_request_out() has filled the FOP correctly
 */
static void borrow_fop_validate(struct m0_rm_fop_borrow *bfop)
{
	struct m0_rm_owner  *owner;
	struct m0_rm_credit  credit;
	int                  rc;

	owner = m0_cookie_of(&bfop->bo_base.rrq_owner.ow_cookie,
			     struct m0_rm_owner, ro_id);

	M0_UT_ASSERT(owner != NULL);
	M0_UT_ASSERT(owner == rm_test_data.rd_owner);

	m0_rm_credit_init(&credit, rm_test_data.rd_owner);
	credit.cr_ops = &rings_credit_ops;
	rc = m0_rm_credit_decode(&credit, &bfop->bo_base.rrq_credit.cr_opaque);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(credit.cr_datum == rm_test_data.rd_credit.cr_datum);
	m0_rm_credit_fini(&credit);

	M0_UT_ASSERT(bfop->bo_base.rrq_policy == RIP_NONE);
	M0_UT_ASSERT(bfop->bo_base.rrq_flags & RIF_MAY_BORROW);
	wait_try_flags_validate(rm_test_data.rd_in.rin_flags,
				bfop->bo_base.rrq_flags);
	m0_buf_free(&bfop->bo_base.rrq_credit.cr_opaque);
}

/*
 * Test function for m0_rm_request_out() for BORROW FOP.
 */
static void borrow_request_test(void)
{
	request_test(M0_RIT_BORROW);
}

/*
 * Test function for borrow_reply().
 */
static void borrow_reply_test(void)
{
	/* 1. Test borrow-success */
	reply_test(M0_RIT_BORROW, 0);

	/* 2. Test borrow-failure */
	reply_test(M0_RIT_BORROW, -ETIMEDOUT);
}

/*
 *****************
 * RM Revoke-FOM test functions
 ******************
 */
/*
 * Validates the owner lists after receiving REVOKE-reply.
 */
static void post_revoke_validate(int err)
{
	bool sublet;
	bool owned;

	m0_rm_owner_lock(rm_test_data.rd_owner);
	sublet = !m0_rm_ur_tlist_is_empty(&rm_test_data.rd_owner->ro_sublet);
	owned = !m0_rm_ur_tlist_is_empty(
			&rm_test_data.rd_owner->ro_owned[OWOS_CACHED]);
	m0_rm_owner_unlock(rm_test_data.rd_owner);

	/* If revoke fails, credit remains in sublet list */
	M0_UT_ASSERT(ergo(err, sublet && !owned));
	/* If revoke succeeds, credit will be the part of cached list*/
	M0_UT_ASSERT(ergo(err == 0, owned && !sublet));
}

/*
 * Check if m0_rm_request_out() has filled the FOP correctly
 */
static void revoke_fop_validate(struct m0_rm_fop_revoke *rfop)
{
	struct m0_rm_owner *owner;
	struct m0_rm_credit credit;
	struct m0_rm_loan  *loan;
	int                 rc;

	owner = m0_cookie_of(&rfop->fr_base.rrq_owner.ow_cookie,
			     struct m0_rm_owner, ro_id);

	M0_UT_ASSERT(owner != NULL);
	M0_UT_ASSERT(owner == rm_test_data.rd_owner);

	loan = m0_cookie_of(&rfop->fr_loan.lo_cookie, struct m0_rm_loan, rl_id);
	M0_UT_ASSERT(loan != NULL);
	M0_UT_ASSERT(loan == test_loan);

	m0_rm_credit_init(&credit, rm_test_data.rd_owner);
	credit.cr_ops = &rings_credit_ops;
	rc = m0_rm_credit_decode(&credit, &rfop->fr_base.rrq_credit.cr_opaque);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(credit.cr_datum == rm_test_data.rd_credit.cr_datum);
	m0_rm_credit_fini(&credit);

	M0_UT_ASSERT(rfop->fr_base.rrq_policy == RIP_NONE);
	M0_UT_ASSERT(rfop->fr_base.rrq_flags & RIF_MAY_REVOKE);
	wait_try_flags_validate(rm_test_data.rd_in.rin_flags,
				rfop->fr_base.rrq_flags);
	m0_buf_free(&rfop->fr_base.rrq_credit.cr_opaque);
}

static void post_revoke_cleanup(struct m0_rpc_item *item, int err)
{
	struct m0_rm_credit *credit;
	struct m0_rm_loan   *loan;
	struct rm_out       *outreq;

	outreq = container_of(m0_rpc_item_to_fop(item), struct rm_out, ou_fop);
	loan = &outreq->ou_req.rog_want;
	/*
	 * After a successful revoke, sublet credit is transferred to
	 * OWOS_CACHED. Otherwise it remains in the sublet list.
	 * Clean up the lists.
	 */
	m0_rm_owner_lock(rm_test_data.rd_owner);
	if (err == 0) {
		m0_tl_for(m0_rm_ur,
			  &rm_test_data.rd_owner->ro_owned[OWOS_CACHED],
			  credit) {
			m0_rm_ur_tlink_del_fini(credit);
			m0_rm_credit_fini(credit);
			m0_free(credit);
		} m0_tl_endfor;
	} else {
		m0_tl_for(m0_rm_ur, &rm_test_data.rd_owner->ro_sublet, credit) {
			m0_rm_ur_tlink_del_fini(credit);
			loan = bob_of(credit, struct m0_rm_loan,
				      rl_credit, &loan_bob);
			m0_rm_loan_fini(loan);
			m0_free(loan);
		} m0_tl_endfor;
	}
	m0_rm_owner_unlock(rm_test_data.rd_owner);
}

static void revoke_reply_populate(struct m0_fop_generic_reply *rreply,
				  int err)
{
	rreply->gr_rc = err;
}

/*
 * Test function for m0_rm_request_out() for REVOKE FOP.
 */
static void revoke_request_test(void)
{
	request_test(M0_RIT_REVOKE);
}

/*
 * Test function for revoke_reply().
 */
static void revoke_reply_test(void)
{
	/* 1. Test revoke-success */
	reply_test(M0_RIT_REVOKE, 0);

	/* 2. Test revoke-failure */
	reply_test(M0_RIT_REVOKE, -ETIMEDOUT);
}

/*
 *****************
 * Top level test functions
 ******************
 */
static void borrow_fop_funcs_test(void)
{
	/* Initialise hierarchy of RM objects with rings resource */
	rings_utdata_ops_set(&rm_test_data);
	rm_utdata_init(&rm_test_data, OBJ_OWNER);

	/* 1. Test m0_rm_request_out() - sending BORROW FOP */
	borrow_request_test();

	/* 2. Test borrow_reply() - reply for BORROW FOP */
	borrow_reply_test();

	rm_utdata_owner_windup_fini(&rm_test_data);
}

static void revoke_fop_funcs_test(void)
{
	/* Initialise hierarchy of RM objects with rings resource */
	rings_utdata_ops_set(&rm_test_data);
	rm_utdata_init(&rm_test_data, OBJ_OWNER);

	/* 1. Test m0_rm_request_out() - sending REVOKE FOP */
	revoke_request_test();

	/* 2. Test revoke_reply() - reply for REVOKE FOP */
	revoke_reply_test();

	rm_utdata_owner_windup_fini(&rm_test_data);
}

void rm_fop_funcs_test(void)
{
	m0_sm_group_init(&ut_rm_mach.rm_sm_grp);
	borrow_fop_funcs_test();
	revoke_fop_funcs_test();
	m0_sm_group_fini(&ut_rm_mach.rm_sm_grp);
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
