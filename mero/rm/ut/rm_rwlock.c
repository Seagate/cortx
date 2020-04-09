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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 03/27/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/types.h"
#include "lib/chan.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/time.h"
#include "lib/vec.h"
#include "fop/fom_generic.h"
#include "fid/fid.h"
#include "ut/ut.h"

#include "rm/rm.h"
#include "rm/rm_internal.h"
#include "rm/rm_rwlock.h"  /* m0_rw_lockable */
#include "rm/ut/rmut.h"

#define OWNER(srv_id) rm_ctxs[srv_id].rc_test_data.rd_owner
#define INREQ(srv_id) (&rm_ctxs[srv_id].rc_test_data.rd_in)

extern const struct m0_rm_resource_type_ops rwlockable_type_ops;

static enum rm_server      test_servers_nr;
static struct m0_semaphore conflict_sem;

static void rwlockable_rtype_set(struct rm_ut_data *self)
{
	struct m0_rm_resource_type *rt;
	int                         rc;

	M0_ALLOC_PTR(rt);
	M0_UT_ASSERT(rt != NULL);
	rt->rt_id = M0_RM_RWLOCKABLE_RT;
	rt->rt_ops = &rwlockable_type_ops;
	rc = m0_rm_type_register(&self->rd_dom, rt);
	M0_UT_ASSERT(rc == 0);
	self->rd_rt = rt;
}

static void rwlockable_rtype_unset(struct rm_ut_data *self)
{
	m0_rm_type_deregister(self->rd_rt);
	m0_free0(&self->rd_rt);
}

static void rwlockable_res_set(struct rm_ut_data *self)
{
	struct m0_rw_lockable *lockable;

	M0_ALLOC_PTR(lockable);
	M0_UT_ASSERT(lockable != NULL);
	m0_fid_set(&self->rd_fid, 1, 0);
	m0_rw_lockable_init(lockable, &self->rd_fid, &self->rd_dom);
	self->rd_res = &lockable->rwl_resource;
}

static void rwlockable_res_unset(struct rm_ut_data *self)
{
	struct m0_rw_lockable *lockable;

	lockable = container_of(self->rd_res, struct m0_rw_lockable,
		                rwl_resource);
	m0_rw_lockable_fini(lockable);
	m0_free(lockable);
	self->rd_res = NULL;
}

static void rwlock_owner_set(struct rm_ut_data *self)
{
	struct m0_rm_owner    *owner;
	struct m0_rw_lockable *lockable;
	struct m0_fid          fid = M0_FID_TINIT(M0_RM_OWNER_FT, 1,
					          (uint64_t)self);

	M0_ALLOC_PTR(owner);
	M0_UT_ASSERT(owner != NULL);
	lockable = container_of(self->rd_res, struct m0_rw_lockable,
			        rwl_resource);
	m0_rm_rwlock_owner_init(owner, &fid, lockable, NULL);
	self->rd_owner = owner;
}

static void rwlock_owner_unset(struct rm_ut_data *self)
{
	M0_UT_ASSERT(owner_state(self->rd_owner) == ROS_FINAL);
	m0_rm_rwlock_owner_fini(self->rd_owner);
	m0_free0(&self->rd_owner);
}

const static struct rm_ut_data_ops rwlock_ut_data_ops = {
	.rtype_set = rwlockable_rtype_set,
	.rtype_unset = rwlockable_rtype_unset,
	.resource_set = rwlockable_res_set,
	.resource_unset = rwlockable_res_unset,
	.owner_set = rwlock_owner_set,
	.owner_unset = rwlock_owner_unset,
	.credit_datum_set = NULL
};

static void rwlock_incoming_complete(struct m0_rm_incoming *in, int32_t rc)
{
	/* Do nothing */
	return;
}

static void rwlock_incoming_conflict(struct m0_rm_incoming *in)
{
	m0_semaphore_up(&conflict_sem);
}

const struct m0_rm_incoming_ops rwlock_incoming_ops = {
	.rio_complete = rwlock_incoming_complete,
	.rio_conflict = rwlock_incoming_conflict
};

static void rwlock_build_hierarchy(void)
{
	rm_ctxs[SERVER_1].creditor_id   = SERVER_INVALID;
	rm_ctxs[SERVER_1].debtor_id[0]  = SERVER_2;
	rm_ctxs[SERVER_1].debtor_id[1]  = SERVER_3;
	rm_ctxs[SERVER_1].rc_debtors_nr = 2;

	rm_ctxs[SERVER_2].creditor_id   = SERVER_1;
	rm_ctxs[SERVER_2].debtor_id[0]  = SERVER_INVALID;
	rm_ctxs[SERVER_2].rc_debtors_nr = 1;

	rm_ctxs[SERVER_3].creditor_id   = SERVER_1;
	rm_ctxs[SERVER_3].debtor_id[0]  = SERVER_INVALID;
	rm_ctxs[SERVER_3].rc_debtors_nr = 1;
}

static void rwlock_utinit(void)
{
	uint32_t i;

	/* Maximum 3 servers for this test */
	test_servers_nr = 3;

	for (i = 0; i < test_servers_nr; ++i)
		rm_ctx_init(&rm_ctxs[i]);

	rm_ctxs_conf_init(rm_ctxs, test_servers_nr);
	rwlock_build_hierarchy();
	for (i = 0; i < test_servers_nr; ++i) {
		rm_ctxs[i].rc_test_data.rd_ops = &rwlock_ut_data_ops;
		rm_ctx_server_start(i);
	}

	creditor_cookie_setup(SERVER_2, SERVER_1);
	creditor_cookie_setup(SERVER_3, SERVER_1);
}

static void rwlock_servers_windup(void)
{
	int i;

	for (i = 0; i < test_servers_nr; ++i)
		rm_ctx_server_owner_windup(i);
}

static void rwlock_servers_disc_fini(void)
{
	int i;

	/* Disconnect the servers */
	for (i = 0; i < test_servers_nr; ++i)
		rm_ctx_server_stop(i);
	rm_ctxs_conf_fini(rm_ctxs, test_servers_nr);
	/* Finalise the servers */
	for (i = 0; i < test_servers_nr; ++i)
		rm_ctx_fini(&rm_ctxs[i]);
}

static void rwlock_utfini(void)
{
	rwlock_servers_windup();
	rwlock_servers_disc_fini();
}

static void rwlock_acquire_nowait(enum rm_server              srv,
				  struct m0_rm_incoming      *in,
				  enum m0_rm_incoming_flags   flags,
				  enum m0_rm_rwlock_req_type  req_type)
{
	m0_rm_rwlock_req_init(in, OWNER(srv), &rwlock_incoming_ops,
			      flags, req_type);
	m0_rm_credit_get(in);

}

static void rwlock_req_wait(enum rm_server         srv,
			    struct m0_rm_incoming *in)
{
	int rc;

	m0_rm_owner_lock(OWNER(srv));
	rc = m0_sm_timedwait(&in->rin_sm,
			     M0_BITS(RI_SUCCESS, RI_FAILURE),
			     M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	m0_rm_owner_unlock(OWNER(srv));
}

static void rwlock_acquire(enum rm_server              srv,
			   struct m0_rm_incoming      *in,
			   enum m0_rm_incoming_flags   flags,
			   enum m0_rm_rwlock_req_type  req_type)
{
	rwlock_acquire_nowait(srv, in, flags, req_type);
	rwlock_req_wait(srv, in);
	M0_UT_ASSERT(in->rin_rc == 0);
	M0_UT_ASSERT(incoming_state(in) == RI_SUCCESS);
}

static void rwlock_release(struct m0_rm_incoming *in)
{
	m0_rm_credit_put(in);
	m0_rm_rwlock_req_fini(in);
}

static void rwlock_encdec_test()
{
	struct m0_rm_resource                *dec_res;
	struct m0_bufvec                      bufvec;
	struct m0_bufvec_cursor               cur;
	const struct m0_rm_resource_type_ops *rwlock_ops;
	struct m0_rm_resource                *resource;
	int                                   rc;

	rwlock_utinit();
	m0_bufvec_alloc(&bufvec, 1, sizeof(struct m0_fid));
	rwlock_ops = rm_ctxs[SERVER_1].rc_test_data.rd_rt->rt_ops;
	resource   = rm_ctxs[SERVER_1].rc_test_data.rd_res;

	/* Encode the resource from the data-set */
	m0_bufvec_cursor_init(&cur, &bufvec);
	rc = rwlock_ops->rto_encode(&cur, resource);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_cursor_init(&cur, &bufvec);
	rc = rwlock_ops->rto_decode(&cur, &dec_res);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dec_res != NULL);
	M0_UT_ASSERT(rwlock_ops->rto_eq(resource, dec_res));
	m0_rm_resource_free(dec_res);

	m0_bufvec_free(&bufvec);
	rwlock_utfini();
}

void rwlock_read_test(void)
{
	rwlock_utinit();

	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK);
	credits_are_equal(SERVER_1, RCL_HELD,   0);

	rwlock_acquire(SERVER_1, INREQ(SERVER_1), 0, RM_RWLOCK_READ);

	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK - 1);
	credits_are_equal(SERVER_1, RCL_HELD,   RM_RW_READ_LOCK);

	rwlock_release(INREQ(SERVER_1));

	rwlock_utfini();
}

void rwlock_write_test(void)
{
	rwlock_utinit();
	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK);
	credits_are_equal(SERVER_1, RCL_HELD,   0);

	rwlock_acquire(SERVER_1, INREQ(SERVER_1), 0, RM_RWLOCK_WRITE);

	credits_are_equal(SERVER_1, RCL_CACHED, 0);
	credits_are_equal(SERVER_1, RCL_HELD,   RM_RW_WRITE_LOCK);

	rwlock_release(INREQ(SERVER_1));

	rwlock_utfini();
}

void rwlock_read_read_hold_test(void)
{
	rwlock_utinit();
	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK);
	credits_are_equal(SERVER_1, RCL_HELD,   0);
	credits_are_equal(SERVER_2, RCL_CACHED, 0);
	credits_are_equal(SERVER_2, RCL_HELD,   0);

	rwlock_acquire(SERVER_1, INREQ(SERVER_1), 0, RM_RWLOCK_READ);
	rwlock_acquire(SERVER_2, INREQ(SERVER_2), RIF_MAY_BORROW,
		       RM_RWLOCK_READ);

	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK - 2);
	credits_are_equal(SERVER_1, RCL_HELD,   RM_RW_READ_LOCK);
	credits_are_equal(SERVER_2, RCL_CACHED, 0);
	credits_are_equal(SERVER_2, RCL_HELD,   RM_RW_READ_LOCK);

	rwlock_release(INREQ(SERVER_2));
	rwlock_release(INREQ(SERVER_1));

	rwlock_utfini();
}

void rwlock_read_write_hold_test(void)
{
	rwlock_utinit();
	m0_semaphore_init(&conflict_sem, 0);

	rwlock_acquire(SERVER_1, INREQ(SERVER_1), 0, RM_RWLOCK_WRITE);
	credits_are_equal(SERVER_1, RCL_HELD, RM_RW_WRITE_LOCK);

	rwlock_acquire_nowait(SERVER_2, INREQ(SERVER_2), RIF_MAY_BORROW,
			      RM_RWLOCK_READ);
	M0_UT_ASSERT(incoming_state(INREQ(SERVER_2)) == RI_WAIT);

	m0_semaphore_timeddown(&conflict_sem, m0_time_from_now(60, 0));
	rwlock_release(INREQ(SERVER_1));

	rwlock_req_wait(SERVER_2, INREQ(SERVER_2));
	M0_UT_ASSERT(INREQ(SERVER_2)->rin_rc == 0);

	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK - 1);
	credits_are_equal(SERVER_1, RCL_HELD,   0);
	credits_are_equal(SERVER_2, RCL_CACHED, 0);
	credits_are_equal(SERVER_2, RCL_HELD,   RM_RW_READ_LOCK);

	rwlock_release(INREQ(SERVER_2));

	m0_semaphore_fini(&conflict_sem);
	rwlock_utfini();
}

void rwlock_write_read_hold_test1(void)
{
	rwlock_utinit();
	m0_semaphore_init(&conflict_sem, 0);

	rwlock_acquire(SERVER_1, INREQ(SERVER_1), 0, RM_RWLOCK_READ);
	credits_are_equal(SERVER_1, RCL_HELD, RM_RW_READ_LOCK);

	rwlock_acquire_nowait(SERVER_2, INREQ(SERVER_2), RIF_MAY_BORROW,
			      RM_RWLOCK_WRITE);
	M0_UT_ASSERT(incoming_state(INREQ(SERVER_2)) == RI_WAIT);

	m0_semaphore_timeddown(&conflict_sem, m0_time_from_now(60, 0));
	rwlock_release(INREQ(SERVER_1));

	rwlock_req_wait(SERVER_2, INREQ(SERVER_2));
	M0_UT_ASSERT(INREQ(SERVER_2)->rin_rc == 0);

	credits_are_equal(SERVER_1, RCL_CACHED, 0);
	credits_are_equal(SERVER_1, RCL_HELD,   0);
	credits_are_equal(SERVER_2, RCL_CACHED, 0);
	credits_are_equal(SERVER_2, RCL_HELD,   RM_RW_WRITE_LOCK);

	rwlock_release(INREQ(SERVER_2));

	m0_semaphore_fini(&conflict_sem);
	rwlock_utfini();
}

void rwlock_write_read_hold_test2(void)
{
	rwlock_utinit();
	m0_semaphore_init(&conflict_sem, 0);

	rwlock_acquire(SERVER_2, INREQ(SERVER_2), RIF_MAY_BORROW,
		       RM_RWLOCK_READ);
	credits_are_equal(SERVER_2, RCL_HELD, RM_RW_READ_LOCK);

	rwlock_acquire_nowait(SERVER_3, INREQ(SERVER_3),
			      RIF_MAY_BORROW | RIF_MAY_REVOKE,
			      RM_RWLOCK_WRITE);
	M0_UT_ASSERT(incoming_state(INREQ(SERVER_3)) == RI_WAIT);

	m0_semaphore_timeddown(&conflict_sem, m0_time_from_now(60, 0));
	rwlock_release(INREQ(SERVER_2));

	rwlock_req_wait(SERVER_3, INREQ(SERVER_3));
	M0_UT_ASSERT(INREQ(SERVER_3)->rin_rc == 0);

	credits_are_equal(SERVER_2, RCL_CACHED, 0);
	credits_are_equal(SERVER_2, RCL_HELD,   0);
	credits_are_equal(SERVER_3, RCL_CACHED, 0);
	credits_are_equal(SERVER_3, RCL_HELD,   RM_RW_WRITE_LOCK);

	rwlock_release(INREQ(SERVER_3));

	m0_semaphore_fini(&conflict_sem);
	rwlock_utfini();
}

void rwlock_write_write_hold_test(void)
{
	rwlock_utinit();
	m0_semaphore_init(&conflict_sem, 0);

	rwlock_acquire(SERVER_2, INREQ(SERVER_2), RIF_MAY_BORROW,
		       RM_RWLOCK_WRITE);
	credits_are_equal(SERVER_2, RCL_HELD, RM_RW_WRITE_LOCK);

	rwlock_acquire_nowait(SERVER_3, INREQ(SERVER_3),
			      RIF_MAY_BORROW | RIF_MAY_REVOKE, RM_RWLOCK_WRITE);
	M0_UT_ASSERT(incoming_state(INREQ(SERVER_3)) == RI_WAIT);

	m0_semaphore_timeddown(&conflict_sem, m0_time_from_now(60, 0));
	rwlock_release(INREQ(SERVER_2));

	rwlock_req_wait(SERVER_3, INREQ(SERVER_3));
	M0_UT_ASSERT(INREQ(SERVER_3)->rin_rc == 0);

	credits_are_equal(SERVER_2, RCL_CACHED, 0);
	credits_are_equal(SERVER_2, RCL_HELD,   0);
	credits_are_equal(SERVER_3, RCL_CACHED, 0);
	credits_are_equal(SERVER_3, RCL_HELD,   RM_RW_WRITE_LOCK);

	rwlock_release(INREQ(SERVER_3));

	m0_semaphore_fini(&conflict_sem);
	rwlock_utfini();
}

void rwlock_two_read_locks_test(void)
{
	rwlock_utinit();

	rwlock_acquire(SERVER_2, INREQ(SERVER_2), RIF_MAY_BORROW,
		       RM_RWLOCK_READ);
	credits_are_equal(SERVER_2, RCL_HELD, RM_RW_READ_LOCK);

	rwlock_acquire(SERVER_3, INREQ(SERVER_3), RIF_MAY_BORROW,
		       RM_RWLOCK_READ);
	credits_are_equal(SERVER_3, RCL_HELD, RM_RW_READ_LOCK);

	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK - 2);

	/*
	 * Check that read credits are returned to the creditor successfully.
	 */
	rwlock_release(INREQ(SERVER_2));
	rm_ctx_server_owner_windup(SERVER_2);
	rwlock_release(INREQ(SERVER_3));
	rm_ctx_server_owner_windup(SERVER_3);
	credits_are_equal(SERVER_1, RCL_CACHED, RM_RW_WRITE_LOCK);
	rm_ctx_server_owner_windup(SERVER_1);

	rwlock_servers_disc_fini();
}

struct m0_ut_suite rm_rwlock_ut = {
	.ts_name = "rm-rwlock-ut",
	.ts_tests = {
		{ "rwlock-encdec"             , rwlock_encdec_test },
		{ "read-lock"                 , rwlock_read_test },
		{ "read-lock-when-read-hold"  , rwlock_read_read_hold_test },
		{ "read-lock-when-write-hold" , rwlock_read_write_hold_test },
		{ "write-lock"                , rwlock_write_test },
		{ "write-lock-when-read-hold1", rwlock_write_read_hold_test1 },
		{ "write-lock-when-read-hold2", rwlock_write_read_hold_test2 },
		{ "write-lock-when-write-hold", rwlock_write_write_hold_test },
		{ "two-read-locks"            , rwlock_two_read_locks_test },
		{ NULL, NULL }
	}
};

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
