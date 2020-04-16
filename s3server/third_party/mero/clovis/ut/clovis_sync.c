/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original authors: James Morse   <james_morse@xyratex.com>,
 *                   Juan Gonzalez <juan_gonzalez@xyratex.com>,
 *                   Sining Wu     <sining_wu@xyratex.com>
 * Original creation date: 17-Apr-2014
 */

/*
 * Production code : m0t1fs/linux_kernel/f{ile,sync}.c
 * UT code         : m0t1fs/linux_kernel/ut/fsync.c
 *
 * The production code is added this way to build mero kernel module.
 * Both production code and UT code are part of Mero kernel module
 * but production code file is not directly added to Makefile as a file
 * that needs to be compiled.
 * Instead, UT code is added in Makefile for mero kernel module build
 * which subsequently adds production code file to mero kernel module.
 */

#include "ut/ut.h"                        /* m0_test_suite */
#include "lib/tlist.h"
#include "lib/hash.h"
#include "mdservice/fsync_fops.h"         /* m0_fop_fsync_mds_fopt */
#include "lib/misc.h"                     /* M0_SET0() */
#include "reqh/reqh_service.h"            /* m0_reqh_service_txid */

#include "clovis/ut/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/sync.h"                 /* clovis_sync_interactions */
#include "clovis/clovis_sync.c"

struct m0_ut_suite                       ut_suite_clovis_sync;

/* counters to indicate how many times each sub is/was called */
static int                               ut_post_rpc_count = 0;
static int                               ut_wait_for_reply_count = 0;
static int                               ut_fop_fini_count = 0;

/* values for control the behaviour of stub functions */
static int                               ut_post_rpc_delay = 0;
static int                               ut_post_rpc_early_return = -EINVAL;
static int                               ut_post_rpc_return = -EINVAL;
static int                               ut_wait_for_reply_return = -EINVAL;
static int                               ut_wait_for_reply_remote_return = 0;

/* The reply fop, set in ut_wait_for_reply */
static struct m0_fop                     reply_fop;

/* The reply data */
static struct m0_fop_fsync_rep           reply_data;

/* The fake sync request and target. */
static struct clovis_sync_request         sreq;
static struct clovis_sync_target          stgt;

/* The fake records that need fsycning */
#define NUM_STRECORDS 10
static struct m0_reqh_service_txid        stx[NUM_STRECORDS];
static struct m0_reqh_service_txid        sreq_stx[NUM_STRECORDS];

/* copy of the fsync interactions -
 *	used to restore the original function pointers */
static struct clovis_sync_interactions    copy;

/* The fake object we try to fsync against */
static struct m0_clovis_obj               obj;

/* The fake clovis instance we fsync against */
static struct m0_clovis                   cinst;

/* The fake clovis realm */
static struct m0_clovis_realm             realm;

/* A fake rpc connection. */
static struct m0_rpc_conn conn = { .c_rpc_machine = (void*)1 };

/* A fake serivce context for fsync to send rpc to */
static struct m0_reqh_service_ctx         service;

static int ut_post_rpc(struct m0_rpc_item *item)
{
	ut_post_rpc_count++;

	/* Check this item came in with the 'right' session */
	M0_UT_ASSERT(item->ri_session == &service.sc_rlink.rlk_sess);

	/* After a certain point, change our return value */
	if (ut_post_rpc_count < ut_post_rpc_delay)
		return ut_post_rpc_early_return;
	else
		return ut_post_rpc_return;
}

static int ut_wait_for_reply(struct m0_rpc_item *item, m0_time_t timeout)
{
	struct m0_fop       *request_fop;
	struct m0_fop_fsync *request_data;

	ut_wait_for_reply_count++;

	/* Make sure we received a valid reply */
	M0_ASSERT(item != NULL);

	/* fop/reply are dealt with serially, so we can use the same object
	 * as a reply every time */
	item->ri_reply = &reply_fop.f_item;

	request_fop = m0_rpc_item_to_fop(item);
	M0_ASSERT(request_fop != NULL);
	request_data = m0_fop_data(request_fop);
	M0_UT_ASSERT(request_data != NULL);
	reply_data.ffr_rc = ut_wait_for_reply_remote_return;
	reply_data.ffr_be_remid = request_data->ff_be_remid;

	reply_fop.f_data.fd_data = &reply_data;

	return ut_wait_for_reply_return;
}

/**
 * Stub for fop_fini, frees any memory allocated to a fop, without the
 * rpc_item reference counting getting upset that the fop was never
 * sent/received.
 */
static void ut_fop_fini(struct m0_fop *fop)
{
	ut_fop_fini_count++;

	if (fop->f_data.fd_data != &reply_data && fop->f_data.fd_data != NULL)
		m0_xcode_free_obj(&M0_FOP_XCODE_OBJ(fop));
	fop->f_data.fd_data = NULL;
}

/**
 * Resets the stub counters.
 */
static void ut_reset_stub_counters(void)
{
	ut_post_rpc_count = 0;
	ut_wait_for_reply_count = 0;
	ut_fop_fini_count = 0;

	/* Ensure early/late behaviour does nothing for ut_post_rpc */
	ut_post_rpc_delay = 0;
}

/**
 * (Re-)initialises our fake Clovis instance.
 * These are fed to the functions we test so that their container-of
 * mechanisms find objects of the correct type, with our fake values.
 */
static void fake_clovis_setup(void)
{
	int i = 0;

	M0_SET0(&obj);
	M0_SET0(&realm);
	M0_SET0(&service);
	M0_SET0(&stx);

	realm.re_instance = &cinst;
	obj.ob_entity.en_realm = &realm;
	obj.ob_entity.en_type = M0_CLOVIS_ET_OBJ;
	m0_mutex_init(&obj.ob_entity.en_pending_tx_lock);
	spti_tlist_init(&obj.ob_entity.en_pending_tx);

	m0_mutex_init(&service.sc_max_pending_tx_lock);
	service.sc_type = M0_CST_IOS;
	service.sc_rlink.rlk_sess.s_sm.sm_state = M0_RPC_SESSION_IDLE;
	service.sc_rlink.rlk_sess.s_conn = &conn;

	/* Add some records that need syncing
	 * This creates @10 records, all for the same service that need
	 * syncing. This would never happen in reality.
	 * clovis_sync_record_update will only ever update the first service it
	 * finds, assuming that there are no duplicates.
	 */
	for (i = 0; i < NUM_STRECORDS; i++) {
		m0_tlink_init(&spti_tl, &stx[i]);
		stx[i].stx_tri.tri_txid = 3;
		stx[i].stx_tri.tri_locality = 7;
		stx[i].stx_service_ctx = &service;
		spti_tlist_add(&obj.ob_entity.en_pending_tx, &stx[i]);
	}

	/* Add the object and build stx list in the sync list.*/
	M0_SET0(&sreq);
	spf_tlist_init(&sreq.sr_fops);
	spti_tlist_init(&sreq.sr_stxs);
	clovis_sync_target_tlist_init(&sreq.sr_targets);
	m0_mutex_init(&sreq.sr_fops_lock);

	stgt.srt_type  = CLOVIS_SYNC_ENTITY;
	stgt.u.srt_ent = &obj.ob_entity;
	clovis_sync_target_tlink_init_at(&stgt, &sreq.sr_targets);

	for (i = 0; i < NUM_STRECORDS; i++) {
		m0_tlink_init(&spti_tl, &sreq_stx[i]);
		sreq_stx[i].stx_tri.tri_txid = 3;
		sreq_stx[i].stx_tri.tri_locality = 7;
		sreq_stx[i].stx_service_ctx = &service;
		spti_tlist_add(&sreq.sr_stxs, &sreq_stx[i]);
	}
}

/**
 * Tests the clovis_sync_request_fop_send function.
 */
static void ut_clovis_test_clovis_sync_request_fop_send(void)
{
	int                             rv;
	struct clovis_sync_fop_wrapper *sfw;
	struct m0_reqh_service_txid     stx;
	struct m0_fop_fsync            *ffd;

	fake_clovis_setup();

	M0_SET0(&stx);
	M0_SET0(&copy);

	/* Copy the fsync_interactions struct so that we can restore it to
	 * default values later */
	copy = si;

	/* we need to override post rpc and fop_fini */
	si.si_post_rpc = &ut_post_rpc;
	si.si_fop_fini = &ut_fop_fini;
	si.si_fop_put = &ut_fop_fini;

	/* Test the values get packed into the fop correctly */
	ut_reset_stub_counters();
	ut_post_rpc_return = 0;
	stx.stx_service_ctx = &service;
	stx.stx_tri.tri_txid = 4000ULL;
	stx.stx_tri.tri_locality = 11;
	rv = clovis_sync_request_fop_send(NULL, &stx,
					  M0_FSYNC_MODE_ACTIVE, false, &sfw);
	M0_UT_ASSERT(rv == 0);
	ffd = m0_fop_data(&sfw->sfw_fop);
	M0_UT_ASSERT(ffd != NULL);
	M0_UT_ASSERT(ffd->ff_be_remid.tri_txid == stx.stx_tri.tri_txid);
	M0_UT_ASSERT(ffd->ff_be_remid.tri_locality == stx.stx_tri.tri_locality);
	M0_UT_ASSERT(ffd->ff_fsync_mode == M0_FSYNC_MODE_ACTIVE);
	M0_UT_ASSERT(ut_fop_fini_count == 0);

	/* reset anything that got initalised */
	m0_fop_fini(&sfw->sfw_fop);
	m0_free(sfw);
	M0_SET0(&stx);

	/* cause post_rpc to fail */
	ut_reset_stub_counters();
	ut_post_rpc_return = -EINVAL;
	stx.stx_service_ctx = &service;
	stx.stx_tri.tri_txid = 4000ULL;
	stx.stx_tri.tri_locality = 99;
	rv = clovis_sync_request_fop_send(NULL, &stx,
					  M0_FSYNC_MODE_ACTIVE, false, &sfw);
	M0_UT_ASSERT(rv == ut_post_rpc_return);
	M0_UT_ASSERT(ut_fop_fini_count == 1);

	/* Restore the fsync_interactions struct */
	si = copy;
}

int clovis_default_txid = 42;
int clovis_default_locality = 7;

void
test_clovis_sync_reply_wait_init(
			struct clovis_sync_fop_wrapper *sfw,
			struct m0_reqh_service_txid *stx)
{
	struct m0_fop_fsync *ffd;

	/* Initialise the fops */
	m0_fop_init(&sfw->sfw_fop, &m0_fop_fsync_mds_fopt, NULL, &m0_fop_release);
	m0_fop_data_alloc(&sfw->sfw_fop);
	ffd = m0_fop_data(&sfw->sfw_fop);
	M0_UT_ASSERT(ffd != NULL);
	ffd->ff_be_remid.tri_txid = clovis_default_txid;
	ffd->ff_be_remid.tri_locality = clovis_default_locality;
	ffd->ff_fsync_mode = M0_FSYNC_MODE_ACTIVE;
	stx->stx_tri.tri_txid = clovis_default_txid;
	stx->stx_tri.tri_locality = clovis_default_locality;
	stx->stx_service_ctx = &service;
	reply_data.ffr_rc = 0;
	reply_data.ffr_be_remid.tri_txid = clovis_default_txid;
	reply_data.ffr_be_remid.tri_locality = clovis_default_locality;
	sfw->sfw_stx = stx;
}


void
call_clovis_sync_reply_wait(struct m0_clovis                *input_cinst,
			    struct m0_clovis_obj            *input_obj,
			    struct clovis_sync_fop_wrapper *input_sfw,
			    int                              expect_return,
			    int                              expect_ut_wait_for_reply_count,
			    int                              expect_ut_fop_fini_count,
			    uint64_t                         expect_txid,
			    size_t                           expect_locality)
{
	int rv;

	input_sfw->sfw_req = &sreq;
	rv = clovis_sync_reply_wait(input_sfw);
	M0_UT_ASSERT(rv == expect_return);
	M0_UT_ASSERT(ut_wait_for_reply_count == expect_ut_wait_for_reply_count);
	M0_UT_ASSERT(ut_fop_fini_count == expect_ut_fop_fini_count);
	M0_UT_ASSERT(input_sfw->sfw_stx->stx_tri.tri_txid == expect_txid);
	M0_UT_ASSERT(input_sfw->sfw_stx->stx_tri.tri_locality ==
		     expect_locality);
}

/**
 * Tests the clovis_sync_reply_wait function.
 */
void ut_clovis_test_clovis_sync_reply_wait(void)
{
	struct m0_reqh_service_txid    *iter;
	struct clovis_sync_fop_wrapper  sfw;
	struct m0_reqh_service_txid     stx;
	struct m0_fop_fsync            *ffd;

	fake_clovis_setup();

	M0_SET0(&sfw);
	M0_SET0(&stx);
	M0_SET0(&ffd);
	M0_SET0(&copy);

	/* Copy the fsync_interactions struct so that we can restore it to
	 * default values later */
	copy = si;

	/* we need to override wait for reply and fop_fini */
	si.si_wait_for_reply = &ut_wait_for_reply;
	si.si_fop_fini = &ut_fop_fini;
	si.si_fop_put = &ut_fop_fini;

	/* Initialise the fops */
	test_clovis_sync_reply_wait_init(&sfw, &stx);

	/* wait for reply fails */
	ut_reset_stub_counters();
	ut_wait_for_reply_return = -EINVAL;
	ut_wait_for_reply_remote_return = 0;
	call_clovis_sync_reply_wait(NULL, &obj, &sfw,
	                                ut_wait_for_reply_return,
	                                1, 1, clovis_default_txid,
					clovis_default_locality);

	/* remote fop fails */
	test_clovis_sync_reply_wait_init(&sfw, &stx);
	ut_reset_stub_counters();
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = -EINVAL;
	call_clovis_sync_reply_wait(NULL, &obj, &sfw,
	                                ut_wait_for_reply_remote_return,
	                                1, 1, clovis_default_txid,
					clovis_default_locality);

	/* inode:transaction-to-sync increased while fop was in flight */
	/* super block record should still be updated - this can happen
	 * when the inode has been updated, and we raced to get the super_block
	 * lock before the thread updating the inode */
	test_clovis_sync_reply_wait_init(&sfw, &stx);
	ffd = m0_fop_data(&sfw.sfw_fop);
	M0_UT_ASSERT(ffd != NULL);
	ffd->ff_be_remid.tri_txid = 42;
	stx.stx_tri.tri_txid = 50;
	stx.stx_tri.tri_locality = 2;
	reply_data.ffr_rc = 0;
	reply_data.ffr_be_remid.tri_txid = 42;
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	iter->stx_tri.tri_txid = 42;
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);
	ut_reset_stub_counters();
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_clovis_sync_reply_wait(NULL, &obj, &sfw, 0,
	                                1, 1, 50, 2);
	/* Check superblock record was updated */
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	M0_UT_ASSERT(iter->stx_tri.tri_txid == 0);
	M0_UT_ASSERT(iter->stx_tri.tri_locality == 0);
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);

	/*
	 * Clovis instance: transaction-to-sync increased while fop
	 * was in flight.
	 */
	/*
	 * As sfw->sfw_stx stores stx that is merged from all involved
	 * targets, clovis updates stx's of targets only, not the one
	 * sfw->sfw_stx.
	 */
	test_clovis_sync_reply_wait_init(&sfw, &stx);
	ffd = m0_fop_data(&sfw.sfw_fop);
	M0_UT_ASSERT(ffd != NULL);
	ffd->ff_be_remid.tri_txid = 42;
	stx.stx_tri.tri_txid = 42;
	stx.stx_tri.tri_locality = 6;
	reply_data.ffr_rc = 0;
	reply_data.ffr_be_remid.tri_txid = 42;
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	iter->stx_tri.tri_txid = 50;
	iter->stx_tri.tri_locality = 81;
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);
	ut_reset_stub_counters();
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_clovis_sync_reply_wait(NULL, &obj, &sfw, 0,
	                                1, 1, 0, 0);
	/* Check superblock record was not updated */
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	M0_UT_ASSERT(iter->stx_tri.tri_txid != 0);
	M0_UT_ASSERT(iter->stx_tri.tri_locality != 0);
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);

	/* transaction-to-sync set to 0 on success */
	test_clovis_sync_reply_wait_init(&sfw, &stx);
	ffd = m0_fop_data(&sfw.sfw_fop);
	M0_UT_ASSERT(ffd != NULL);
	ffd->ff_be_remid.tri_txid = 42;
	stx.stx_tri.tri_txid = 42;
	stx.stx_tri.tri_locality = 12;
	reply_data.ffr_rc = 0;
	reply_data.ffr_be_remid.tri_txid = 42;
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	iter->stx_tri.tri_txid = 42;
	iter->stx_tri.tri_locality = 12;
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);

	ut_reset_stub_counters();
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_clovis_sync_reply_wait(NULL, &obj, &sfw, 0,
	                                1, 1, 0, 0);
	/* Check superblock record was updated */
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	M0_UT_ASSERT(iter->stx_tri.tri_txid == 0);
	M0_UT_ASSERT(iter->stx_tri.tri_locality == 0);
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);

	/* Restore the fsync_interactions struct */
	si = copy;
}

/**
 * Tests the clovis_sync_record_update function.
 */
void ut_clovis_test_clovis_sync_record_update(void)
{
	struct m0_reqh_service_txid *iter;
	struct m0_be_tx_remid        btr;

	fake_clovis_setup();

	/* test the inode record is updated */
	btr.tri_txid = 50;
	btr.tri_locality = 3;
	clovis_sync_record_update(&service, &obj.ob_entity, NULL, &btr);
	/* test the first record was updated */
	M0_UT_ASSERT(stx[NUM_STRECORDS-1].stx_tri.tri_txid == 50ULL);
	/* check later records were not updated */
	M0_UT_ASSERT(stx[0].stx_tri.tri_txid != 50ULL);
	M0_UT_ASSERT(stx[0].stx_tri.tri_locality != 3);

	/* test the sb record is updated */
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);
	btr.tri_txid = 999;
	btr.tri_locality = 18;
	clovis_sync_record_update(&service, &obj.ob_entity, NULL, &btr);
	M0_UT_ASSERT(iter->stx_tri.tri_txid == 999ULL);
	M0_UT_ASSERT(iter->stx_tri.tri_locality == 18);
}

void call_clovis_sync_request_launch_and_wait(
				struct m0_clovis_obj *input_obj,
				int input_flag, int expect_return,
				int expect_ut_post_rpc_count,
				int expect_ut_wait_for_reply_count,
				int expect_ut_fop_fini_count)
{
	int rv;

	/* Reset sreq. */
	sreq.sr_nr_fops = 0;
	sreq.sr_rc = 0;
	rv = clovis_sync_request_launch_and_wait(&sreq, input_flag);
	M0_UT_ASSERT(rv == expect_return);
	M0_UT_ASSERT(ut_post_rpc_count == expect_ut_post_rpc_count);
	M0_UT_ASSERT(ut_wait_for_reply_count == expect_ut_wait_for_reply_count);
	M0_UT_ASSERT(ut_fop_fini_count == expect_ut_fop_fini_count);
}

/**
 * Tests the clovis_sync_launch_and_wait function.
 */
static void ut_clovis_test_clovis_sync_request_launch_and_wait(void)
{
	int i;

	fake_clovis_setup();

	M0_SET0(&reply_fop);
	M0_SET0(&reply_data);
	M0_SET0(&copy);

	/* Copy the fsync_interactions struct so that we can restore it to
	 * default values later */
	copy = si;

	/* Load the stubs over the top of the fsync_interactions struct */
	si.si_post_rpc = &ut_post_rpc;
	si.si_wait_for_reply = &ut_wait_for_reply;
	si.si_fop_fini = &ut_fop_fini;
	si.si_fop_put = &ut_fop_fini;

	/* Cause fop sending to fail at the first attempt - check an error
	 * is returned and that we don't wait for a reply to the failed fop */
	ut_reset_stub_counters();
	ut_post_rpc_return = -EINVAL;
	ut_post_rpc_early_return = -EINVAL;
	call_clovis_sync_request_launch_and_wait(
		&obj, M0_FSYNC_MODE_ACTIVE, ut_post_rpc_return, 1, 0, 1);

	/* Cause fop sending to fail after a few have been sent - check those
	 * that were sent correctly have their replies processed */
	ut_reset_stub_counters();
	ut_post_rpc_early_return = 0;
	ut_post_rpc_delay = 4;
	ut_post_rpc_return = -EINVAL;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_clovis_sync_request_launch_and_wait(&obj, M0_FSYNC_MODE_ACTIVE,
	                       ut_post_rpc_return, ut_post_rpc_delay,
	                       ut_post_rpc_delay - 1, ut_post_rpc_delay);
	/* reset replies */
	for (i = 0; i < NUM_STRECORDS; i++)
		sreq_stx[i].stx_tri.tri_txid = 3;

	/* Cause a remote fop to fail - test the error is propogated */
	ut_reset_stub_counters();
	ut_post_rpc_early_return = 0;
	ut_post_rpc_delay = 2;
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = -EINVAL;
	call_clovis_sync_request_launch_and_wait(&obj, M0_FSYNC_MODE_ACTIVE,
	                       ut_wait_for_reply_remote_return,
	                       NUM_STRECORDS, NUM_STRECORDS, NUM_STRECORDS);
	/* reset replies */
	for (i = 0; i < NUM_STRECORDS; i++)
		sreq_stx[i].stx_tri.tri_txid = 3;

	/* Send replies for the versions requested - test records are updated
	 so that repeated calls to fsync have no effect */
	ut_reset_stub_counters();
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_clovis_sync_request_launch_and_wait(&obj, M0_FSYNC_MODE_ACTIVE, 0,
	                       NUM_STRECORDS, NUM_STRECORDS, NUM_STRECORDS);
	/* test the records were updated */
	M0_UT_ASSERT(stx[NUM_STRECORDS - 1].stx_tri.tri_txid == 0);
	/* Don't reset replies ! */

	/* Test a repeated call to fsync causes no fops to be sent */
	ut_reset_stub_counters();
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_clovis_sync_request_launch_and_wait(
		&obj, M0_FSYNC_MODE_ACTIVE, -EAGAIN, 0, 0, 0);
	/* Restore the fsync_interactions struct */
	si = copy;

}

void call_m0_clovis_obj_sync(struct m0_clovis_obj *obj,
			     int expect_return,
			     int expect_ut_post_rpc_count,
			     int expect_ut_fop_fini_count)
{
	int rv;

	rv = m0_clovis_entity_sync(&obj->ob_entity);
	M0_UT_ASSERT(rv == expect_return);
	M0_UT_ASSERT(ut_post_rpc_count == expect_ut_post_rpc_count);
	M0_UT_ASSERT(ut_fop_fini_count == expect_ut_fop_fini_count);
}

void ut_clovis_test_m0_clovis_obj_sync(void)
{
	/*
	 * Copy the fsync_interactions struct so that we can restore it to
	 * default values later.
	 */
	M0_SET0(&copy);
	copy = si;

	/* Setup fake test env */
	fake_clovis_setup();
	si.si_post_rpc = &ut_post_rpc;
	si.si_wait_for_reply = &ut_wait_for_reply;
	si.si_fop_fini = &ut_fop_fini;
	si.si_fop_put = &ut_fop_fini;

	/* Check normal operation works */
	ut_reset_stub_counters();
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_m0_clovis_obj_sync(&obj, 0, 1, 1);

	/* test the records were updated */
	M0_UT_ASSERT(stx[NUM_STRECORDS - 1].stx_tri.tri_txid == 0);

	/* Restore the fsync_interactions struct */
	si = copy;
}

M0_INTERNAL int ut_clovis_sync_init(void)
{
#ifndef __KERNEL__
	ut_clovis_shuffle_test_order(&ut_suite_clovis_sync);
#endif

	m0_clovis_init_io_op();

	return 0;
}

M0_INTERNAL int ut_clovis_sync_fini(void)
{
	return 0;
}

struct m0_ut_suite ut_suite_clovis_sync = {
	.ts_name = "clovis-sync-ut",
	.ts_init = ut_clovis_sync_init,
	.ts_fini = ut_clovis_sync_fini,
	.ts_tests = {

		{ "m0_clovis_obj_sync",
		  &ut_clovis_test_m0_clovis_obj_sync},
		{ "clovis_sync_request_launch_and_wait",
		  &ut_clovis_test_clovis_sync_request_launch_and_wait},
		{ "clovis_sync_record_update",
		  &ut_clovis_test_clovis_sync_record_update},
		{ "clovis_sync_reply_wait",
		  &ut_clovis_test_clovis_sync_reply_wait},
		{ "clovis_sync_request_fop_send",
		  &ut_clovis_test_clovis_sync_request_fop_send},
		{ NULL, NULL },
	}
};
