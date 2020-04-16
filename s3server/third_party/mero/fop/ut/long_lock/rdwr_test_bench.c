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
 * Original creation date: 08/06/2012
 */

#include "ut/ut.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/mutex.h"
#include "reqh/reqh.h"
#include "fop/fom_long_lock.h"

M0_TL_DESCR_DECLARE(m0_lll, M0_EXTERN);
M0_TL_DECLARE(m0_lll, M0_INTERNAL, struct m0_long_lock_link);

enum tb_request_type {
	RQ_READ,
	RQ_WRITE,
	RQ_WAKE_UP,
	RQ_LAST
};

enum tb_request_phase {
	/* See comment on PH_REQ_LOCK value in fom_rdwr_state() function */
	PH_REQ_LOCK = M0_FOM_PHASE_INIT,
	PH_GOT_LOCK = M0_FOPH_NR + 1,
};

struct test_min_max {
	size_t min;
	size_t max;
};

struct test_request {
	enum tb_request_type tr_type;
	/* Expected count of waiters */
	size_t tr_waiters;
	/**
	 * Expected count of owners. As far, test is being run in multithreaded
	 * environment, concurrent FOMs can pop an owner from a queue in an
	 * arbitary order. That's the reason why struct test_min_max is used.
	 */
	struct test_min_max tr_owners;
};

static struct m0_fom      *sleeper;
static struct m0_chan      chan[RDWR_REQUEST_MAX];
static struct m0_clink	   clink[RDWR_REQUEST_MAX];
static struct m0_long_lock long_lock;

/**
 * a. Checks that multiple readers can hold the read lock concurrently, but
 * writers (more than one) get blocked.
 */
static bool readers_check(struct m0_long_lock *lock)
{
	struct m0_long_lock_link *head;
	bool result;

	m0_mutex_lock(&lock->l_lock);

	head = m0_lll_tlist_head(&lock->l_waiters);
	result = m0_tl_forall(m0_lll, l, &lock->l_owners,
			      l->lll_lock_type == M0_LONG_LOCK_READER) &&
		ergo(head != NULL, head->lll_lock_type == M0_LONG_LOCK_WRITER);

	m0_mutex_unlock(&lock->l_lock);

	return result;
}

/**
 * b. Only one writer at a time can hold the write lock. All other contenders
 * wait.
 */
static bool writer_check(struct m0_long_lock *lock)
{
	struct m0_long_lock_link *head;
	bool result;

	m0_mutex_lock(&lock->l_lock);

	head = m0_lll_tlist_head(&lock->l_owners);
	result = head != NULL &&
		head->lll_lock_type == M0_LONG_LOCK_WRITER &&
		m0_lll_tlist_length(&lock->l_owners) == 1;

	m0_mutex_unlock(&lock->l_lock);

	return result;
}

/**
 * Checks expected readers and writers against actual.
 */
static bool lock_check(struct m0_long_lock *lock, enum tb_request_type type,
		       size_t owners_min, size_t owners_max, size_t waiters)
{
	bool result;
	size_t owners_len;

	m0_mutex_lock(&lock->l_lock);

	owners_len = m0_lll_tlist_length(&lock->l_owners);
	result = owners_min <= owners_len && owners_len <= owners_max &&
		m0_lll_tlist_length(&lock->l_waiters) == waiters &&

		(type == RQ_WRITE) ? lock->l_state == M0_LONG_LOCK_WR_LOCKED :
		(type == RQ_READ)  ? lock->l_state == M0_LONG_LOCK_RD_LOCKED :
		false;

	m0_mutex_unlock(&lock->l_lock);

	return result;
}

static int fom_rdwr_tick(struct m0_fom *fom)
{
	struct fom_rdwr	*request;
	int		 rq_type;
	int		 rq_seqn;
	int		 result;

	request = container_of(fom, struct fom_rdwr, fr_gen);
	M0_UT_ASSERT(request != NULL);
	rq_type = request->fr_req->tr_type;
	rq_seqn = request->fr_seqn;

	/*
	 * To pacify M0_PRE(M0_IN(m0_fom_phase(fom), (M0_FOPH_INIT,
	 * M0_FOPH_FAILURE))) precondition in m0_fom_queue(), special processing
	 * order of FOM phases is used.
	 *
	 * Do NOT use this code as a template for the general purpose. It's
	 * designed for tesing of m0_long_lock ONLY!
	 */
	if (m0_fom_phase(fom) == PH_REQ_LOCK) {
		if (rq_seqn == 0)
			sleeper = fom;

		switch (rq_type) {
		case RQ_READ:
			result = M0_FOM_LONG_LOCK_RETURN(
					m0_long_read_lock(&long_lock,
							  &request->fr_link,
							  PH_GOT_LOCK));
			M0_UT_ASSERT((result == M0_FSO_AGAIN)
				     == (rq_seqn == 0));
			result = M0_FSO_WAIT;
			break;
		case RQ_WRITE:
			result = M0_FOM_LONG_LOCK_RETURN(
					m0_long_write_lock(&long_lock,
							   &request->fr_link,
							   PH_GOT_LOCK));
			M0_UT_ASSERT((result == M0_FSO_AGAIN)
				     == (rq_seqn == 0));
			result = M0_FSO_WAIT;
			break;
		case RQ_WAKE_UP:
		default:
			m0_fom_wakeup(sleeper);
			m0_fom_phase_set(fom, PH_GOT_LOCK);
			result = M0_FSO_AGAIN;
		}

		/* notify, fom ready */
		m0_chan_signal_lock(&chan[rq_seqn]);
	} else if (m0_fom_phase(fom) == PH_GOT_LOCK) {
		M0_UT_ASSERT(ergo(M0_IN(rq_type, (RQ_READ, RQ_WRITE)),
				  lock_check(&long_lock, rq_type,
					     request->fr_req->tr_owners.min,
					     request->fr_req->tr_owners.max,
					     request->fr_req->tr_waiters)));

		switch (rq_type) {
		case RQ_READ:
			M0_UT_ASSERT(readers_check(&long_lock));
			M0_UT_ASSERT(m0_long_is_read_locked(&long_lock, fom));
			m0_long_read_unlock(&long_lock, &request->fr_link);
			break;
		case RQ_WRITE:
			M0_UT_ASSERT(writer_check(&long_lock));
			M0_UT_ASSERT(m0_long_is_write_locked(&long_lock, fom));
			m0_long_write_unlock(&long_lock, &request->fr_link);
			break;
		case RQ_WAKE_UP:
		default:
			;
		}

		/* notify, fom ready */
		m0_chan_signal_lock(&chan[rq_seqn]);
		m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
		result = M0_FSO_WAIT;
        } else {
		M0_IMPOSSIBLE("");
		result = 0;
	}

	return result;
}

static void reqh_fop_handle(struct m0_reqh *reqh,  struct m0_fom *fom)
{
	M0_PRE(reqh != NULL);
	m0_rwlock_read_lock(&reqh->rh_rwlock);
        M0_PRE(m0_reqh_state_get(reqh) == M0_REQH_ST_NORMAL);
	m0_fom_queue(fom);
	m0_rwlock_read_unlock(&reqh->rh_rwlock);
}

static void test_req_handle(struct m0_reqh *reqh,
			    struct test_request *rq, int seqn)
{
	struct m0_fom   *fom;
	struct fom_rdwr *obj;
	int rc;

	rc = rdwr_fom_create(&fom, reqh);
	M0_UT_ASSERT(rc == 0);

	obj = container_of(fom, struct fom_rdwr, fr_gen);
	obj->fr_req  = rq;
	obj->fr_seqn = seqn;

	reqh_fop_handle(reqh, fom);
}

/* c. To make sure that the fairness queue works, lock should sequentially
 * transit from "state to state" listed in the following structure: */

static struct test_request test[3][RDWR_REQUEST_MAX] = {
	[0] = {
		{.tr_type = RQ_READ,  .tr_owners = {1, 1}, .tr_waiters = 8},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 7},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 4},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 4},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 4},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 3},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},

		{.tr_type = RQ_WAKE_UP, .tr_owners = {0, 0}, .tr_waiters = 0},
		{.tr_type = RQ_LAST,    .tr_owners = {0, 0}, .tr_waiters = 0},
	},
	[1] = {
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 9},
		{.tr_type = RQ_READ,  .tr_owners = {1, 1}, .tr_waiters = 8},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 7},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 4},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 4},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 4},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 3},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},

		{.tr_type = RQ_WAKE_UP, .tr_owners = {0, 0}, .tr_waiters = 0},
		{.tr_type = RQ_LAST,    .tr_owners = {0, 0}, .tr_waiters = 0},
	},
	[2] = {
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 9},
		{.tr_type = RQ_READ,  .tr_owners = {1, 1}, .tr_waiters = 8},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 7},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 6},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 5},
		{.tr_type = RQ_READ,  .tr_owners = {1, 1}, .tr_waiters = 4},
		{.tr_type = RQ_WRITE, .tr_owners = {1, 1}, .tr_waiters = 3},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},
		{.tr_type = RQ_READ,  .tr_owners = {1, 3}, .tr_waiters = 0},

		{.tr_type = RQ_WAKE_UP, .tr_owners = {0, 0}, .tr_waiters = 0},
		{.tr_type = RQ_LAST,    .tr_owners = {0, 0}, .tr_waiters = 0},
	},
};

static void rdwr_send_fop(struct m0_reqh **reqh, size_t reqh_nr)
{
	int i;
	int j;

	for (j = 0; j < ARRAY_SIZE(test); ++j) {
		m0_long_lock_init(&long_lock);

		for (i = 0; test[j][i].tr_type != RQ_LAST; ++i) {
			m0_chan_init(&chan[i], &long_lock.l_lock);
			m0_clink_init(&clink[i], NULL);
			m0_clink_add_lock(&chan[i], &clink[i]);

			/* d. Send FOMs from multiple request handlers, where
			 * they can contend for the lock. 'reqh[i % reqh_nr]'
			 * expression allows to send FOMs one by one into each
			 * request handler */
			test_req_handle(reqh[i % reqh_nr], &test[j][i], i);

			/* Wait until the fom completes the first state
			 * transition. This is needed to achieve deterministic
			 * lock acquisition order. */
			m0_chan_wait(&clink[i]);
		}

		/* Wait until all queued foms are processed. */
		for (i = 0; test[j][i].tr_type != RQ_LAST; ++i) {
			m0_chan_wait(&clink[i]);
		}

		/* Cleanup */
		for (i = 0; test[j][i].tr_type != RQ_LAST; ++i) {
			m0_clink_del_lock(&clink[i]);
			m0_chan_fini_lock(&chan[i]);
			m0_clink_fini(&clink[i]);
		}

		m0_long_lock_fini(&long_lock);
	}
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
