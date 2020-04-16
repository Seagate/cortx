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
 * Original creation date: 03/27/2013
 */
#include "lib/types.h"            /* uint64_t */
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
#include "rm/rm_fops.h"
#include "file/file.h"
#include "rm/ut/rmut.h"

/* Import */
extern const struct m0_rm_resource_type_ops file_lock_type_ops;

/*
 * Hierarchy description:
 * SERVER_1 is downward debtor for SERVER_2.
 * SERVER_2 is upward creditor for SERVER_1.
 */

enum {
	CLNT_THR_NR = 10,
	SRV_THR_NR = 10
};

static struct m0_thread clnt_thr[CLNT_THR_NR];
static struct m0_thread srv_thr[SRV_THR_NR];
static int clnt_counter;
static int srv_counter;

/* Maximum test servers for this testcase */
static enum rm_server  test_servers_nr;
static struct m0_clink tests_clink[LOCK_TESTS_NR];

/*
 * m0_file_lock_type_register registers a single type
 * This test needs two object hierarchies. Hence we need one more file type
 * object.
 */
static void fl_rtype_set(struct rm_ut_data *self)
{
	struct m0_rm_resource_type *rt;
	int			    rc;

	M0_ALLOC_PTR(rt);
	M0_UT_ASSERT(rt != NULL);
	rt->rt_id = M0_RM_FLOCK_RT;
	rt->rt_ops = &file_lock_type_ops;
	rc = m0_rm_type_register(&self->rd_dom, rt);
	M0_UT_ASSERT(rc == 0);
	self->rd_rt = rt;
}

static void fl_rtype_unset(struct rm_ut_data *self)
{
	m0_rm_type_deregister(self->rd_rt);
	m0_free0(&self->rd_rt);
}

static void fl_res_set(struct rm_ut_data *self)
{
	struct m0_file *flock;

	M0_ALLOC_PTR(flock);
	M0_UT_ASSERT(flock != NULL);
	m0_fid_set(&self->rd_fid, 1, 0);
	m0_file_init(flock, &self->rd_fid, &self->rd_dom, 0);
	self->rd_res = &flock->fi_res;
}

static void fl_res_unset(struct rm_ut_data *self)
{
	struct m0_file *flock;

	flock = container_of(self->rd_res, struct m0_file, fi_res);
	m0_file_fini(flock);
	m0_free(flock);
	self->rd_res = NULL;
}

static void fl_owner_set(struct rm_ut_data *self)
{
	struct m0_rm_owner *owner;
	struct m0_file     *flock;

	M0_ALLOC_PTR(owner);
	M0_UT_ASSERT(owner != NULL);
	flock = container_of(self->rd_res, struct m0_file, fi_res);
	m0_file_owner_init(owner, &m0_rm_no_group, flock, NULL);
	self->rd_owner = owner;
}

static void fl_owner_unset(struct rm_ut_data *self)
{
	int rc;

	m0_rm_owner_windup(self->rd_owner);
	rc = m0_rm_owner_timedwait(self->rd_owner, M0_BITS(ROS_FINAL),
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(owner_state(self->rd_owner) == ROS_FINAL);
	m0_file_owner_fini(self->rd_owner);
	m0_free0(&self->rd_owner);
}

static void fl_datum_set(struct rm_ut_data *self)
{
	self->rd_credit.cr_datum = RM_FILE_LOCK;
}

const static struct rm_ut_data_ops fl_srv_ut_data_ops = {
	.rtype_set = fl_rtype_set,
	.rtype_unset = fl_rtype_unset,
	.resource_set = fl_res_set,
	.resource_unset = fl_res_unset,
	.owner_set = fl_owner_set,
	.owner_unset = fl_owner_unset,
	.credit_datum_set = fl_datum_set
};

const static struct rm_ut_data_ops fl_client_ut_data_ops = {
	.rtype_set = fl_rtype_set,
	.rtype_unset = fl_rtype_unset,
	.resource_set = fl_res_set,
	.resource_unset = fl_res_unset,
	.owner_set = fl_owner_set,
	.owner_unset = fl_owner_unset,
	.credit_datum_set = fl_datum_set
};

void flock_client_utdata_ops_set(struct rm_ut_data *data)
{
	data->rd_ops = &fl_client_ut_data_ops;
}

void flock_srv_utdata_ops_set(struct rm_ut_data *data)
{
	data->rd_ops = &fl_srv_ut_data_ops;
}

static void file_encdec_test(struct rm_ut_data *utdata)
{
	struct m0_file        *decfile;
	struct m0_rm_resource *dec_res;
	m0_bcount_t             buf_count = 16;
	int                     rc;
	char                    caddr[16];
	void                   *addr = &caddr[0];
	struct m0_bufvec        bufvec = M0_BUFVEC_INIT_BUF(&addr, &buf_count);
	struct m0_bufvec_cursor cur;

	/* Encode the resource from the data-set */
	m0_bufvec_cursor_init(&cur, &bufvec);
	rc = utdata->rd_rt->rt_ops->rto_encode(&cur, utdata->rd_res);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_cursor_init(&cur, &bufvec);
	rc = utdata->rd_rt->rt_ops->rto_decode(&cur, &dec_res);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dec_res != NULL);
	decfile = container_of(dec_res, struct m0_file, fi_res);

	M0_UT_ASSERT(utdata->rd_rt->rt_ops->rto_eq(utdata->rd_res,
						   &decfile->fi_res));
	m0_rm_resource_free(dec_res);
}

static void wait_lock(enum rm_server srv_id)
{
	struct m0_rm_incoming *in    = &rm_ctxs[srv_id].rc_test_data.rd_in;
	struct m0_rm_owner    *owner = rm_ctxs[srv_id].rc_test_data.rd_owner;
	int		       rc;

	m0_file_lock(owner, in);
	m0_rm_owner_lock(owner);
	rc = m0_sm_timedwait(&in->rin_sm,
			     M0_BITS(RI_SUCCESS, RI_FAILURE),
			     M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	m0_rm_owner_unlock(owner);
	M0_UT_ASSERT(in->rin_rc == 0);
	m0_file_unlock(in);
}

void test_verify(enum flock_tests test_id)
{
	struct m0_rm_owner *clnt = rm_ctxs[SERVER_1].rc_test_data.rd_owner;
	struct m0_rm_owner *srv  = rm_ctxs[SERVER_2].rc_test_data.rd_owner;
	struct m0_sm_group *smgrp;
	bool                validation_lock = false;

	switch(test_id) {
	case LOCK_ON_CLIENT_TEST:
		M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(&srv->ro_sublet));
		M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(&clnt->ro_borrowed));
		M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(
				&clnt->ro_owned[OWOS_CACHED]));
		break;
	case LOCK_ON_SERVER_TEST:
		M0_UT_ASSERT(m0_rm_ur_tlist_is_empty(&srv->ro_sublet));
		M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(
				&srv->ro_owned[OWOS_CACHED]));
		M0_UT_ASSERT(m0_rm_ur_tlist_is_empty(&clnt->ro_borrowed));
		break;
	case DISTRIBUTED_LOCK_TEST:
		smgrp = owner_grp(srv);
		if (!m0_mutex_is_locked(&smgrp->s_lock)) {
			validation_lock = true;
			m0_rm_owner_lock(srv);
		}
		M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(&srv->ro_sublet) ||
			     !m0_rm_ur_tlist_is_empty(
                                &srv->ro_owned[OWOS_CACHED]) ||
			     !m0_rm_ur_tlist_is_empty(
                                &srv->ro_owned[OWOS_HELD]));
		if (validation_lock)
			m0_rm_owner_unlock(srv);
		break;
	default:
		break;
	}
}

static void dlock(enum rm_server srv_id, int n)
{
	struct m0_rm_owner    *owner = rm_ctxs[srv_id].rc_test_data.rd_owner;
	struct m0_rm_incoming  req;
	int		       rc;

	M0_SET0(&req);
	m0_file_lock(owner, &req);
	m0_rm_owner_lock(owner);
	test_verify(DISTRIBUTED_LOCK_TEST);
	rc = m0_sm_timedwait(&req.rin_sm,
			     M0_BITS(RI_SUCCESS, RI_FAILURE),
			     M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(req.rin_rc == 0);
	M0_UT_ASSERT(incoming_state(&req) == RI_SUCCESS);
	m0_rm_owner_unlock(owner);
	if (srv_id == SERVER_1) {
		clnt_counter += n;
		loan_session_set(SERVER_2, SERVER_1);
	} else if (srv_id == SERVER_2) {
		srv_counter += n;
	}
	m0_file_unlock(&req);
	M0_SET0(&req);
}

static void srv_dlock(int n)
{
	dlock(SERVER_2, n);
}

static void srv_dlock_run(void)
{
	int i;
	int sum;
	int rc;

	for (sum = i = 0; i < SRV_THR_NR; ++i) {
		rc = M0_THREAD_INIT(&srv_thr[i], int, NULL,
				    &srv_dlock, i, "srv_dlock");
		M0_UT_ASSERT(rc == 0);
		sum += i;
	}

	for (i = 0; i < SRV_THR_NR; ++i) {
		m0_thread_join(&srv_thr[i]);
		m0_thread_fini(&srv_thr[i]);
	}
	M0_UT_ASSERT(srv_counter == sum);
}

static void clnt_dlock(int n)
{
	/* SERVER_1 acts as a client */
	dlock(SERVER_1, n);
}

static void client_dlock_run(void)
{
	int i;
	int sum;
	int rc;

	/* Server-2 (server) is upward creditor for Server-1 (client) */
	creditor_cookie_setup(SERVER_1, SERVER_2);

	for (sum = i = 0; i < CLNT_THR_NR; ++i) {
		rc = M0_THREAD_INIT(&clnt_thr[i], int, NULL,
				    &clnt_dlock, i, "clnt_dlock");
		M0_UT_ASSERT(rc == 0);
		sum += i;
	}

	for (i = 0; i < CLNT_THR_NR; ++i) {
		m0_thread_join(&clnt_thr[i]);
		m0_thread_fini(&clnt_thr[i]);
	}
	M0_UT_ASSERT(clnt_counter == sum);
}

/* DLD - Test 3 */
static void server_lock_test(void)
{
	/* Take a wait lock on server */
	wait_lock(SERVER_2);
}

/* DLD - Test 2 */
static void testcase2_run(void)
{
	struct m0_rm_incoming *in    = &rm_ctxs[SERVER_1].rc_test_data.rd_in;
	struct m0_rm_owner    *owner = rm_ctxs[SERVER_1].rc_test_data.rd_owner;
	struct m0_rm_incoming  req;

	/* Take the lock */
	m0_file_lock(owner, in);
	M0_UT_ASSERT(incoming_state(in) == RI_SUCCESS);
	M0_UT_ASSERT(m0_rm_ur_tlist_is_empty(&owner->ro_owned[OWOS_CACHED]));
	M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(&owner->ro_owned[OWOS_HELD]));

	/* Recursively try to take the same lock */
	m0_file_lock(owner, &req);
	M0_UT_ASSERT(incoming_state(&req) == RI_WAIT);

	/* Release lock (from the first request) */
	m0_file_unlock(in);

	/* Second lock request should now be successful */
	M0_UT_ASSERT(req.rin_rc == 0);
	M0_UT_ASSERT(incoming_state(&req) == RI_SUCCESS);
	m0_file_unlock(&req);
}

/* DLD - Test 1 */
static void testcase1_run(void)
{
	/* Server-2 (server) is upward creditor for Server-1 (client) */
	creditor_cookie_setup(SERVER_1, SERVER_2);

	/* Take a wait lock on client */
	wait_lock(SERVER_1);

	/*
	 * Set up session pointer on the server. So that this lock can be
	 * revoked by the server.
	 */
	loan_session_set(SERVER_2, SERVER_1);
}

static void client_lock_test(void)
{
	testcase1_run();
	test_verify(LOCK_ON_CLIENT_TEST);
	testcase2_run();
	test_verify(LOCK_ON_CLIENT_TEST);
}

static void client_tests(void)
{
	m0_chan_wait(&tests_clink[LOCK_ON_CLIENT_TEST]);

	/* Test encode/decode for code coverage */
	file_encdec_test(&rm_ctxs[SERVER_1].rc_test_data);

	/* Now start the use-cases */
	client_lock_test();

	/* Begin next test */
	m0_chan_signal_lock(&rm_ut_tests_chan);
	m0_chan_wait(&tests_clink[DISTRIBUTED_LOCK_TEST]);

	client_dlock_run();
}

static void server_tests(void)
{
	/* Now start the tests - wait till all the servers are ready */
	m0_chan_signal_lock(&rm_ut_tests_chan);
	m0_chan_wait(&tests_clink[LOCK_ON_SERVER_TEST]);
	server_lock_test();
	test_verify(LOCK_ON_SERVER_TEST);

	/* Begin next test */
	m0_chan_signal_lock(&rm_ut_tests_chan);
	srv_dlock_run();
}

static void rm_server_start(const int tid)
{
	if (tid >= test_servers_nr)
		return;

	switch(tid) {
	case SERVER_1:
		flock_client_utdata_ops_set(&rm_ctxs[tid].rc_test_data);
		rm_ctx_server_start(tid);
		client_tests();
		break;
	case SERVER_2:
		flock_srv_utdata_ops_set(&rm_ctxs[tid].rc_test_data);
		rm_ctx_server_start(tid);
		server_tests();
		break;
	default:
		break;
	}
}

/*
 * Configure server hierarchy.
 */
static void server_hier_config(void)
{
	rm_ctxs[SERVER_1].creditor_id = SERVER_2;
	rm_ctxs[SERVER_1].debtor_id[0] = SERVER_INVALID;
	rm_ctxs[SERVER_1].rc_debtors_nr = 1;

	rm_ctxs[SERVER_2].creditor_id = SERVER_INVALID;
	rm_ctxs[SERVER_2].debtor_id[0] = SERVER_1;
	rm_ctxs[SERVER_2].rc_debtors_nr = 1;
}

static void flock_utinit(void)
{
	uint32_t i;

	/* Maximum 2 servers for this test */
	test_servers_nr = 2;
	for (i = 0; i < test_servers_nr; ++i)
		rm_ctx_init(&rm_ctxs[i]);

	rm_ctxs_conf_init(rm_ctxs, test_servers_nr);
	server_hier_config();
	m0_mutex_init(&rm_ut_tests_chan_mutex);
	m0_chan_init(&rm_ut_tests_chan, &rm_ut_tests_chan_mutex);
	/* Set up test sync points */
	for (i = 0; i < LOCK_TESTS_NR; ++i) {
		m0_clink_init(&tests_clink[i], NULL);
		m0_clink_add_lock(&rm_ut_tests_chan, &tests_clink[i]);
	}
}

static void flock_utfini(void)
{
	int32_t i;

	/*
	 * Windup the server first, then the client. Trying to stop
	 * client first may put it into INSOLVENT state. This will
	 * finalize UT-RM objects hierarchy.
	 */
	rm_ctx_server_windup(SERVER_2);
	rm_ctx_server_windup(SERVER_1);

	/*
	 * Following loops cannot be combined.
	 * The ops within the loops need sync points. Hence they are separate.
	 */
	/* Disconnect the servers. */
	for (i = test_servers_nr - 1; i >= 0; --i) {
		rm_ctx_server_stop(i);
	}
	rm_ctxs_conf_fini(rm_ctxs, test_servers_nr);
	/*
	 * Finalise the servers. Must be done in the reverse order, so that the
	 * first initialised reqh is finalised last.
	 */
	for (i = test_servers_nr - 1; i >= 0; --i) {
		rm_ctx_fini(&rm_ctxs[i]);
	}
	for (i = 0; i < LOCK_TESTS_NR; ++i) {
		m0_clink_del_lock(&tests_clink[i]);
		m0_clink_fini(&tests_clink[i]);
	}
	m0_chan_fini_lock(&rm_ut_tests_chan);
	m0_mutex_fini(&rm_ut_tests_chan_mutex);
}

void flock_test(void)
{
	int rc;
	int i;

	flock_utinit();
	/* Start RM servers */
	for (i = 0; i < test_servers_nr; ++i) {
		rc = M0_THREAD_INIT(&rm_ctxs[i].rc_thr, int, NULL,
				    &rm_server_start, i, "rm_server_%d", i);
		M0_UT_ASSERT(rc == 0);
	}

	for (i = 0; i < test_servers_nr; ++i) {
		m0_thread_join(&rm_ctxs[i].rc_thr);
		m0_thread_fini(&rm_ctxs[i].rc_thr);
	}
	flock_utfini();
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
