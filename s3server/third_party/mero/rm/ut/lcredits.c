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
 * Original author: Rajesh Bhalerao <rajesh_bhalerao@xyratex.com>
 * Original creation date: 07/19/2012
 */
#include "lib/types.h"            /* uint64_t */
#include "lib/chan.h"
#include "lib/misc.h"
#include "ut/ut.h"
#include "lib/ub.h"

#include "rm/rm.h"
#include "rm/rm_internal.h"      /* pi_tlist_length */
#include "rm/ut/rings.h"

static struct m0_chan  complete_chan;
static struct m0_chan  conflict_chan;
static struct m0_mutex conflict_mutex;

extern bool res_tlist_contains(const struct m0_tl *list,
			       const struct m0_rm_resource *res);

static void lcredits_in_complete(struct m0_rm_incoming *in, int32_t rc)
{
        M0_UT_ASSERT(in != NULL);
        m0_chan_broadcast_lock(&complete_chan);
}

static void lcredits_in_conflict(struct m0_rm_incoming *in)
{
	M0_UT_ASSERT(in != NULL);
        m0_chan_broadcast_lock(&conflict_chan);
}

const struct m0_rm_incoming_ops lcredits_incoming_ops = {
        .rio_complete = lcredits_in_complete,
        .rio_conflict = lcredits_in_conflict
};

static void local_credits_init(void)
{
	rings_utdata_ops_set(&rm_test_data);
	rm_utdata_init(&rm_test_data, OBJ_OWNER);
	rm_test_owner_capital_raise(rm_test_data.rd_owner,
				    &rm_test_data.rd_credit);
	M0_SET0(&rm_test_data.rd_in);
	m0_chan_init(&complete_chan, &rm_test_data.rd_rt->rt_lock);
	m0_mutex_init(&conflict_mutex);
	m0_chan_init(&conflict_chan, &conflict_mutex);
}

static void local_credits_fini(void)
{
	m0_chan_fini_lock(&conflict_chan);
	m0_mutex_fini(&conflict_mutex);
	m0_chan_fini_lock(&complete_chan);
	rm_utdata_owner_windup_fini(&rm_test_data);
}

static void credits_pinned_number_test(enum m0_rm_incoming_flags flags)
{
	struct m0_rm_incoming next_in;

	/*
	 * Test verifies that no excessive credits are pinned if several
	 * cached/held credits fully satisfies incoming request by their own.
	 *
	 * The situation is simulated when owner has C0, C1 credits both fully
	 * satisfying incoming request for credit C2.
	 * Owner: Held->[C1], Cached->[C0]
	 * in->rin_want: C2
	 * diff(C2,C1) == 0 and diff(C2,C0) == 0
	 *
	 * Test checks that only one credit is used to satisfy incoming request.
	 */

	/* Move one cached credit to held list */
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);
	rm_test_data.rd_in.rin_want.cr_datum = NENYA;
	rm_test_data.rd_in.rin_ops = &rings_incoming_ops;
	m0_rm_credit_get(&rm_test_data.rd_in);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == 0);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_SUCCESS);

	/* Ask for new credit and test number of pinned credits */
	M0_SET0(&next_in);
	m0_rm_incoming_init(&next_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RINGS_RIP, flags);
	next_in.rin_want.cr_datum = ANY_RING;
	next_in.rin_ops = &rings_incoming_ops;

	m0_rm_credit_get(&next_in);
	M0_UT_ASSERT(next_in.rin_rc == 0);
	M0_UT_ASSERT(next_in.rin_sm.sm_state == RI_SUCCESS);
	M0_UT_ASSERT(pi_tlist_length(&next_in.rin_pins) == 1);

	m0_rm_credit_put(&rm_test_data.rd_in);
	m0_rm_credit_put(&next_in);

	m0_rm_incoming_fini(&rm_test_data.rd_in);
	m0_rm_incoming_fini(&next_in);
}

static void cached_credits_test(enum m0_rm_incoming_flags flags)
{
	struct m0_rm_incoming next_in;

	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);

	rm_test_data.rd_in.rin_want.cr_datum = NENYA;
	rm_test_data.rd_in.rin_ops = &rings_incoming_ops;
	/*
	 * 1. Test obtaining cached credit.
	 */
	m0_rm_credit_get(&rm_test_data.rd_in);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == 0);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_SUCCESS);

	M0_SET0(&next_in);
	m0_rm_incoming_init(&next_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);
	next_in.rin_want.cr_datum = VILYA;
	next_in.rin_ops = &rings_incoming_ops;

	/*
	 * 2. Test obtaining another cached credit.
	 */
	m0_rm_credit_get(&next_in);
	M0_UT_ASSERT(next_in.rin_rc == 0);
	M0_UT_ASSERT(next_in.rin_sm.sm_state == RI_SUCCESS);

	m0_rm_credit_put(&rm_test_data.rd_in);
	m0_rm_credit_put(&next_in);

	m0_rm_incoming_fini(&rm_test_data.rd_in);
	m0_rm_incoming_fini(&next_in);
}

static void held_credits_test(enum m0_rm_incoming_flags flags)
{
	struct m0_rm_incoming next_in;
	struct m0_clink       clink;

	M0_SET0(&rm_test_data.rd_in);
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);

	rm_test_data.rd_in.rin_want.cr_datum = NENYA;
	rm_test_data.rd_in.rin_ops = &lcredits_incoming_ops;

	m0_rm_credit_get(&rm_test_data.rd_in);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == 0);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_SUCCESS);

	M0_SET0(&next_in);
	m0_rm_incoming_init(&next_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);
	next_in.rin_want.cr_datum = NENYA;
	next_in.rin_ops = &lcredits_incoming_ops;

	/*
	 * 1. Try to obtain conflicting held credit.
	 */
	m0_rm_credit_get(&next_in);
	M0_UT_ASSERT(ergo(flags == 0,
			  next_in.rin_sm.sm_state == RI_SUCCESS &&
			  next_in.rin_rc == 0));
	M0_UT_ASSERT(ergo(flags & RIF_LOCAL_WAIT,
			  next_in.rin_sm.sm_state == RI_WAIT));
	M0_UT_ASSERT(ergo(flags & RIF_LOCAL_TRY,
			  next_in.rin_sm.sm_state == RI_FAILURE));

	if (flags & RIF_LOCAL_WAIT) {
		m0_clink_init(&clink, NULL);
		m0_clink_add_lock(&complete_chan, &clink);
	}

	/* First caller releases the credit */
	m0_rm_credit_put(&rm_test_data.rd_in);

	/*
	 * 2. If the flag is RIF_LOCAL_WAIT, check if we get the credit
	 *    after the first caller releases it.
	 */
	if (flags & RIF_LOCAL_WAIT) {
		M0_UT_ASSERT(m0_chan_timedwait(&clink, M0_TIME_NEVER));
		M0_UT_ASSERT(next_in.rin_rc == 0);
		M0_UT_ASSERT(next_in.rin_sm.sm_state == RI_SUCCESS);
		m0_rm_credit_put(&next_in);
		m0_clink_del_lock(&clink);
		m0_clink_fini(&clink);
	} else if (flags == 0 || !(flags & RIF_LOCAL_TRY)) {
		m0_rm_credit_put(&next_in);
	}

	m0_rm_incoming_fini(&rm_test_data.rd_in);
	m0_rm_incoming_fini(&next_in);
}

static void held_non_conflicting_test(enum m0_rm_incoming_flags flags)
{
	struct m0_rm_incoming any_in;

	M0_SET0(&rm_test_data.rd_in);
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);

	rm_test_data.rd_in.rin_want.cr_datum = ALLRINGS;
	rm_test_data.rd_in.rin_ops = &lcredits_incoming_ops;

	m0_rm_credit_get(&rm_test_data.rd_in);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == 0);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_SUCCESS);

	M0_SET0(&any_in);
	m0_rm_incoming_init(&any_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RINGS_RIP, flags);
	any_in.rin_want.cr_datum = ANY_RING;
	any_in.rin_ops = &lcredits_incoming_ops;

	/*
	 * Try to obtain non-conflicting held credit.
	 */
	m0_rm_credit_get(&any_in);
	M0_UT_ASSERT(any_in.rin_rc == 0);
	M0_UT_ASSERT(any_in.rin_sm.sm_state == RI_SUCCESS);

	m0_rm_credit_put(&rm_test_data.rd_in);
	m0_rm_credit_put(&any_in);

	m0_rm_incoming_fini(&rm_test_data.rd_in);
	m0_rm_incoming_fini(&any_in);

}

static void failures_test(void)
{
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, RIF_LOCAL_WAIT);

	rm_test_data.rd_in.rin_ops = &rings_incoming_ops;
	rm_test_data.rd_in.rin_want.cr_datum = INVALID_RING;

	/*
	 * 1. Test - m0_rm_credit_get() with invalid credit (value) fails.
	 */
	m0_rm_credit_get(&rm_test_data.rd_in);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_FAILURE);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == -ESRCH);

	/*
	 * 2. Test - credit_get fails when owner is not in ROS_ACTIVE state.
	 */
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, RIF_LOCAL_WAIT);
	rm_test_data.rd_in.rin_ops = &rings_incoming_ops;
	rm_test_data.rd_in.rin_want.cr_datum = INVALID_RING;
	rm_test_data.rd_owner->ro_sm.sm_state = ROS_FINALISING;
	m0_rm_credit_get(&rm_test_data.rd_in);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == -EAGAIN);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_FAILURE);
	rm_test_data.rd_owner->ro_sm.sm_state = ROS_ACTIVE;
}

static void reserved_credit_get_test(enum m0_rm_incoming_flags    flags,
				     enum m0_rm_owner_owned_state type)
{
	struct m0_rm_incoming in1;
	struct m0_rm_incoming in2;
	struct m0_clink       complete_clink;
	struct m0_clink       conflict_clink;

	/*
	 * Test checks that if local credits pinned with M0_RPF_BARRIER are
	 * suitable for incoming request, then incoming request is set to
	 * RI_WAIT state until these credits are unpinned. The only exception is
	 * incoming request with RIF_LOCAL_TRY flag. In that case -EBUSY is
	 * returned immediately.
	 */
	m0_clink_init(&complete_clink, NULL);
	m0_clink_add_lock(&complete_chan, &complete_clink);
	m0_clink_init(&conflict_clink, NULL);
	m0_clink_add_lock(&conflict_chan, &conflict_clink);

	/* Hold NENYA */
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, 0);
	rm_test_data.rd_in.rin_want.cr_datum = NENYA;
	rm_test_data.rd_in.rin_ops = &lcredits_incoming_ops;
	m0_rm_credit_get(&rm_test_data.rd_in);
	m0_chan_wait(&complete_clink);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == 0);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_SUCCESS);

	/*
	 * Get credits with RIF_RESERVE flag. Since NENYA is already
	 * held and RIF_LOCAL_WAIT is set, request is not satisfied immediately,
	 * but requested credits are pinned with M0_RPF_BARRIER.
	 */
	M0_SET0(&in1);
	m0_rm_incoming_init(&in1, rm_test_data.rd_owner, M0_RIT_LOCAL,
			    RIP_NONE, RIF_RESERVE | RIF_LOCAL_WAIT);
	in1.rin_want.cr_datum = NARYA | NENYA;
	in1.rin_ops = &lcredits_incoming_ops;
	m0_rm_credit_get(&in1);
	M0_UT_ASSERT(in1.rin_rc == 0);
	M0_UT_ASSERT(in1.rin_sm.sm_state == RI_WAIT);

	/*
	 * Get credit pinned with M0_RPF_BARRIER.
	 * Request can't be satisfied immediately.
	 */
	M0_SET0(&in2);
	m0_rm_incoming_init(&in2, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, flags);
	in2.rin_want.cr_datum = (type == OWOS_CACHED) ? NARYA : NENYA;
	in2.rin_ops = &lcredits_incoming_ops;
	m0_rm_credit_get(&in2);
	if (flags & RIF_LOCAL_TRY)
		M0_UT_ASSERT(in2.rin_sm.sm_state == RI_FAILURE &&
			     in2.rin_rc == -EBUSY);
	else
		M0_UT_ASSERT(in2.rin_sm.sm_state == RI_WAIT &&
			     in2.rin_rc == 0);

	/* Release NENYA credit, so in1 can be satisfied */
	M0_UT_ASSERT(m0_chan_timedwait(&conflict_clink,
				       m0_time_from_now(10, 0)));
	m0_rm_credit_put(&rm_test_data.rd_in);
	M0_UT_ASSERT(m0_chan_timedwait(&complete_clink,
				       m0_time_from_now(10, 0)));

	/* Release credits for in1, so in2 can be satisfied */
	m0_rm_credit_put(&in1);
	if (!(flags & RIF_LOCAL_TRY)) {
		M0_UT_ASSERT(m0_chan_timedwait(&complete_clink,
					       m0_time_from_now(10, 0)));
		m0_rm_credit_put(&in2);
	}

	m0_rm_incoming_fini(&rm_test_data.rd_in);
	m0_rm_incoming_fini(&in1);
	m0_rm_incoming_fini(&in2);
	m0_clink_del_lock(&complete_clink);
	m0_clink_fini(&complete_clink);
	m0_clink_del_lock(&conflict_clink);
	m0_clink_fini(&conflict_clink);
}

void barrier_on_barrier_test(void)
{
	struct m0_rm_incoming in1;
	struct m0_rm_incoming in2;
	struct m0_clink       complete_clink;
	struct m0_clink       conflict_clink;

	/*
	 * Test checks that incoming requests with RIF_RESERVE flag are
	 * processed in order they are issued.
	 */
	m0_clink_init(&complete_clink, NULL);
	m0_clink_add_lock(&complete_chan, &complete_clink);
	m0_clink_init(&conflict_clink, NULL);
	m0_clink_add_lock(&conflict_chan, &conflict_clink);

	/* Hold NENYA */
	m0_rm_incoming_init(&rm_test_data.rd_in, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, 0);
	rm_test_data.rd_in.rin_want.cr_datum = NENYA;
	rm_test_data.rd_in.rin_ops = &lcredits_incoming_ops;
	m0_rm_credit_get(&rm_test_data.rd_in);
	m0_chan_wait(&complete_clink);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_rc == 0);
	M0_UT_ASSERT(rm_test_data.rd_in.rin_sm.sm_state == RI_SUCCESS);

	/* First request with RIF_RESERVE for NENYA */
	M0_SET0(&in1);
	m0_rm_incoming_init(&in1, rm_test_data.rd_owner, M0_RIT_LOCAL,
				RIP_NONE, RIF_RESERVE | RIF_LOCAL_WAIT);
	in1.rin_want.cr_datum = NENYA;
	in1.rin_ops = &lcredits_incoming_ops;
	m0_rm_credit_get(&in1);
	M0_UT_ASSERT(in1.rin_rc == 0);
	M0_UT_ASSERT(in1.rin_sm.sm_state == RI_WAIT);

	/* Second request with RIF_RESERVE for NENYA */
	M0_SET0(&in2);
	m0_rm_incoming_init(&in2, rm_test_data.rd_owner,
			    M0_RIT_LOCAL, RIP_NONE, RIF_RESERVE);
	in2.rin_want.cr_datum = NENYA;
	in2.rin_ops = &lcredits_incoming_ops;
	m0_rm_credit_get(&in2);
	M0_UT_ASSERT(in2.rin_sm.sm_state == RI_WAIT);
	M0_UT_ASSERT(in2.rin_rc == 0);

	/* Release NENYA */
	M0_UT_ASSERT(m0_chan_timedwait(&conflict_clink,
				       m0_time_from_now(10, 0)));
	m0_rm_credit_put(&rm_test_data.rd_in);

	/* First request with RIF_RESERVE is satisfied */
	M0_UT_ASSERT(m0_chan_timedwait(&complete_clink,
				       m0_time_from_now(10, 0)));
	m0_rm_credit_put(&in1);

	/* Second request with RIF_RESERVE is satisfied */
	M0_UT_ASSERT(m0_chan_timedwait(&complete_clink,
				       m0_time_from_now(10, 0)));
	m0_rm_credit_put(&in2);

	m0_rm_incoming_fini(&rm_test_data.rd_in);
	m0_rm_incoming_fini(&in1);
	m0_rm_incoming_fini(&in2);
	m0_clink_del_lock(&complete_clink);
	m0_clink_fini(&complete_clink);
	m0_clink_del_lock(&conflict_clink);
	m0_clink_fini(&conflict_clink);
}

void local_credits_test(void)
{
	int      i;
	uint64_t flags[] = {0, RIF_LOCAL_WAIT, RIF_LOCAL_TRY, RIF_RESERVE,
	                    RIF_RESERVE | RIF_LOCAL_WAIT,
			    RIF_RESERVE | RIF_LOCAL_TRY};

	local_credits_init();
	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		cached_credits_test(flags[i]);
		held_credits_test(flags[i]);
		held_non_conflicting_test(flags[i]);
		credits_pinned_number_test(flags[i]);
		reserved_credit_get_test(flags[i], OWOS_CACHED);
		reserved_credit_get_test(flags[i], OWOS_HELD);
	}
	barrier_on_barrier_test();
	failures_test();
	local_credits_fini();
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
