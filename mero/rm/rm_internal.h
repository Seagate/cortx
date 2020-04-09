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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 09/15/2011
 */
#pragma once

#ifndef __MERO_RM_RM_INTERNAL_H__
#define __MERO_RM_RM_INTERNAL_H__

#include "lib/bob.h"
#include "lib/cookie.h"
#include "rm/rm.h"

enum m0_file_credit {
	RM_FILE_LOCK = 1
};

/** Credit values for RW lock requests */
enum m0_rwlock_credit {
	RM_RW_READ_LOCK  = 1,
	RM_RW_WRITE_LOCK = ~0ULL
};

enum {
	WAIT_TRY_FLAGS = RIF_LOCAL_WAIT | RIF_LOCAL_TRY,
};

/**
 * Created as a result of remote request which is either BORROW or REVOKE
 * (and CANCEL in future).
 */
struct m0_rm_remote_incoming {
	struct m0_rm_incoming ri_incoming;
	/**
	 * Cookie of local owner sent by the remote end.
	 * This is used for locality determination.
	 */
	struct m0_cookie      ri_owner_cookie;
	/**
	 * Cookie of remote owner sent by the remote end.
	 */
	struct m0_cookie      ri_rem_owner_cookie;
	struct m0_cookie      ri_loan_cookie;
	struct m0_uint128     ri_group_id;
};

/**
 * Sticks a tracking pin on @credit. When @credit is released, the all incoming
 * requests that stuck pins into it are notified.
 */
M0_INTERNAL int m0_rm_pin_add(struct m0_rm_incoming *in,
                              struct m0_rm_credit   *credit,
                              uint32_t               flags);

/**
 * Check if credit that was granted by remote owner should be reserved by some
 * incoming request that waits for outgoing request completion. If yes, then
 * reserve 'to_cache' credit and force other requests to wait for reservation
 * cancel.
 */
M0_INTERNAL int granted_maybe_reserve(struct m0_rm_credit *granted,
				      struct m0_rm_credit *to_cache);

/**
 * @name RM FOP interface.
 *
 * Functions and data-structures used for interaction between RM core and RM
 * fops (and fom)s.
 */

/** @{ */

/**
 * Moves credited credits from "owned" to "sublet" list.
 *
 * This is called by borrow fom code after successful processing of
 * bor->bi_incoming.
 *
 * On success, this call transfers ownership of bor->bi_loan to the owner.
 *
 * @pre bor->bi_loan is initialised.
 * @pre m0_mutex_is_locked(&owner->ro_lock)
 * @pre in->rin_state == RI_SUCCESS
 * @pre in->rin_type == RIT_BORROW
 * @pre loan->rl_credit.ri_owner == owner
 *
 * where "owner", "loan" and "in" are respective attributes of "bor".
 */
M0_INTERNAL int m0_rm_borrow_commit(struct m0_rm_remote_incoming *bor);

/**
 * Removes revoked credits from "owned" and borrowed lists.
 *
 * Called by revoke fom code to complete processing of revoke.
 *
 * @pre m0_mutex_is_locked(&owner->ro_lock)
 * @pre in->rin_state == RI_SUCCESS
 * @pre in->rin_type == RIT_REVOKE
 */
M0_INTERNAL int m0_rm_revoke_commit(struct m0_rm_remote_incoming *rvk);

/**
 * Adds borrowed credit to the "borrowed" and "owned" lists.
 *
 * This is called on receiving a reply to an outgoing BORROW fop.
 *
 * This function transfers ownership of the supplied "loan" structure to the
 * owner.
 */
M0_INTERNAL int m0_rm_borrow_done(struct m0_rm_outgoing *out,
				  struct m0_rm_loan *loan);

/**
 * Moves revoked credit from "sublet" to "owned" list.
 *
 * This is called on receiving a reply to an outgoing REVOKE fop.
 */
int m0_rm_revoke_done(struct m0_rm_outgoing *out);

/**
 * Returns the owner locally managing the credits for a given resource.
 *
 * This is used on the creditor side to identify an owner against which an
 * incoming BORROW request is to be processed.
 */
int m0_rm_resource_owner_find(const struct m0_rm_resource *resource,
			      struct m0_rm_owner **owner);

/**
 * Constructs and sends out an outgoing request.
 *
 * Allocates m0_rm_outgoing, adds a pin from "in" to the outgoing request.
 * Constructs and sends an outgoing request fop. Arranges
 * m0_rm_outgoing_complete() to be called on fop reply or timeout.
 *
 */
M0_INTERNAL int m0_rm_request_out(enum m0_rm_outgoing_type otype,
				  struct m0_rm_incoming *in,
				  struct m0_rm_loan *loan,
				  struct m0_rm_credit *credit,
				  struct m0_rm_remote *remote);

M0_INTERNAL void m0_rm_outgoing_send(struct m0_rm_outgoing *outgoing);

/**
 * Initialises the fields of for incoming structure.
 * This creates an incoming request with an empty m0_rm_incoming::rin_want
 * credit.
 *
 * @param out - outgoing (remote) credit request structure
 * @param req_type - outgoing request type
 * @see m0_rm_outgoing_fini
 */
M0_INTERNAL int m0_rm_outgoing_init(struct m0_rm_outgoing    *out,
				    enum m0_rm_outgoing_type  req_type,
				    struct m0_rm_remote      *other,
				    struct m0_rm_credit      *credit);

/**
 * Finalises the fields of
 * @param out
 * @see m0_rm_outgoing_init
 */
M0_INTERNAL void m0_rm_outgoing_fini(struct m0_rm_outgoing *out);

/**
 * Initialises the loan
 */
M0_INTERNAL int m0_rm_loan_init(struct m0_rm_loan         *loan,
				const struct m0_rm_credit *credit,
				struct m0_rm_remote       *creditor);

/**
 * Finalise the lona. Release ref count of remote owner.
 */
M0_INTERNAL void m0_rm_loan_fini(struct m0_rm_loan *loan);

/**
 * Allocates and initialises the loan
 *
 * @param loan - On success, this will contain an allocated and initialised
 *               loan strucutre
 * @param credit - the credits for which loan is being allocated/created.
 */
M0_INTERNAL int m0_rm_loan_alloc(struct m0_rm_loan         **loan,
				 const struct m0_rm_credit  *credit,
				 struct m0_rm_remote        *creditor);

/**
 * Debits (removes) a loan (borrowed or sublet) from a given list.
 */
M0_INTERNAL int m0_rm_owner_loan_debit(struct m0_rm_owner *owner,
				       struct m0_rm_loan  *paid_loan,
				       struct m0_tl       *list);
/**
 * Pays back the loan. Removes the sublet loan and refreshes the credit cache.
 */
M0_INTERNAL int m0_rm_loan_settle(struct m0_rm_owner *owner,
				  struct m0_rm_loan  *loan);

/**
 * Called when an outgoing request completes (possibly with an error, like a
 * timeout).
 */
M0_INTERNAL void m0_rm_outgoing_complete(struct m0_rm_outgoing *og);

/**
 * Establish a reverse session to facilitate sending revoke requests
 */
M0_INTERNAL int
m0_rm_reverse_session_get(struct m0_rm_remote_incoming *rem_in,
			  struct m0_rm_remote          *remote);

/** @} end of rm-fop interface. */

/**
 * @name RM lists.
 */

/** @{ */

M0_TL_DESCR_DECLARE(res, extern);
M0_TL_DECLARE(res, M0_INTERNAL, struct m0_rm_resource);

M0_TL_DESCR_DECLARE(m0_rm_ur, extern);
M0_TL_DECLARE(m0_rm_ur, M0_INTERNAL, struct m0_rm_credit);

M0_TL_DESCR_DECLARE(m0_remotes, extern);
M0_TL_DECLARE(m0_remotes, M0_INTERNAL, struct m0_rm_remote);

M0_TL_DESCR_DECLARE(m0_owners, extern);
M0_TL_DECLARE(m0_owners, M0_INTERNAL, struct m0_rm_owner);

M0_TL_DESCR_DECLARE(pr, extern);
M0_TL_DECLARE(pr, M0_INTERNAL, struct m0_rm_pin);

M0_TL_DESCR_DECLARE(pi, extern);
M0_TL_DECLARE(pi, M0_INTERNAL, struct m0_rm_pin);

M0_EXTERN const struct m0_bob_type loan_bob;
M0_BOB_DECLARE(M0_INTERNAL, m0_rm_loan);

/**
 * Execute "expr" against all credits lists in a given owner.
 */
#define RM_OWNER_LISTS_FOR(owner, expr)					\
({									\
	struct m0_rm_owner *__o = (owner);				\
	int                 __i;					\
	int                 __j;					\
									\
	(expr)(&__o->ro_borrowed);					\
	(expr)(&__o->ro_sublet);					\
									\
	for (__i = 0; __i < ARRAY_SIZE(__o->ro_owned); __i++)		\
		(expr)(&__o->ro_owned[__i]);				\
									\
	for (__i = 0; __i < ARRAY_SIZE(__o->ro_incoming); __i++) {	\
		for (__j = 0; __j < ARRAY_SIZE(__o->ro_incoming[__i]); __j++) \
			(expr)(&__o->ro_incoming[__i][__j]);		\
	}								\
									\
	for (__i = 0; __i < ARRAY_SIZE(__o->ro_outgoing); __i++)	\
		(expr)(&__o->ro_outgoing[__i]);				\
})

static inline enum m0_rm_incoming_state
incoming_state(const struct m0_rm_incoming *in)
{
	return (enum m0_rm_incoming_state)in->rin_sm.sm_state;
}

static inline struct m0_rm_resource *
incoming_to_resource(struct m0_rm_incoming *in)
{
	return in->rin_want.cr_owner->ro_resource;
}

static inline enum m0_rm_owner_state
owner_state(const struct m0_rm_owner *owner)
{
	return (enum m0_rm_owner_state)owner->ro_sm.sm_state;
}

static inline struct m0_sm_group *
resource_grp(const struct m0_rm_resource *res)
{
	return &res->r_type->rt_sm_grp;
}

static inline struct m0_sm_group *
owner_grp(const struct m0_rm_owner *owner)
{
	return resource_grp(owner->ro_resource);
}

/** @} end of RM lists. */

M0_INTERNAL struct m0_rm_remote *
m0_rm_remote_find(struct m0_rm_remote_incoming *rem_in);

/* __MERO_RM_RM_INTERNAL_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
