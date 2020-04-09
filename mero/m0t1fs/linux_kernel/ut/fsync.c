/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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

#include <linux/version.h>                /* LINUX_VERSION_CODE */

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
#include <linux/version.h>
#include "ut/ut.h"                        /* m0_test_suite */
#include "lib/tlist.h"
#include "lib/hash.h"
#include "lib/trace.h"
#include "mdservice/fsync_fops.h"         /* m0_fop_fsync_mds_fopt */
#include "m0t1fs/linux_kernel/m0t1fs.h"   /* m0t1fs_inode */
#include "m0t1fs/linux_kernel/fsync.h"    /* m0t1fs_fsync_interactions */
#include "lib/misc.h"                     /* M0_SET0() */
#include "reqh/reqh_service.h"            /* m0_reqh_service_txid */
/* declared in fsync.c */
extern struct m0t1fs_fsync_interactions  fi;

/* counters to indicate how many times each sub is/was called */
static int                               ut_kernel_fsync_count = 0;
static int                               ut_post_rpc_count = 0;
static int                               ut_wait_for_reply_count = 0;
static int                               ut_fop_fini_count = 0;

/* values for control the behaviour of stub functions */
static int                               ut_post_rpc_delay = 0;
static int                               ut_kernel_fsync_return = -EINVAL;
static int                               ut_post_rpc_early_return = -EINVAL;
static int                               ut_post_rpc_return = -EINVAL;
static int                               ut_wait_for_reply_return = -EINVAL;
static int                               ut_wait_for_reply_remote_return = 0;

/* The reply fop, set in ut_wait_for_reply */
static struct m0_fop                     reply_fop;

/* The reply data */
static struct m0_fop_fsync_rep           reply_data;

/* The fake records that need fsycning */
#define NUM_STRECORDS 10
static struct m0_reqh_service_txid        stx[NUM_STRECORDS];

/* copy of the fsync interactions -
 *	used to restore the original function pointers */
static struct m0t1fs_fsync_interactions  copy;

/* The fake file we try to fsync against */
static struct file                       file;

/* The fake dentry for the file we fsync against */
static struct dentry                     dentry;

/* The fake super_block for the file system we fsync against */
static struct super_block                super_block;

/* A fake m0inode for our fake file/dentry */
static struct m0t1fs_inode               m0inode;

/* A fake super_block for our fake filesystem */
static struct m0t1fs_sb                  csb;

/* A fake serivce context for fsync to send rpc to */
static struct m0_reqh_service_ctx    service;

/* fake connection complying with m0_rpc_session_validate() */
struct m0_rpc_conn conn = { .c_rpc_machine = (void*)1 };

/* Stub functions used to test m0t1fs_fsync */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int ut_kernel_fsync(struct file *file, loff_t start, loff_t end,
                           int datasync)
#else
static int ut_kernel_fsync(struct file *file, struct dentry *dentry,
                           int datasync)
#endif
{
	ut_kernel_fsync_count++;

	return ut_kernel_fsync_return;
}

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

	/* fop/reply are dealt with serially, so we can use the same object
	 * as a reply every time */
	item->ri_reply = &reply_fop.f_item;

	/* Make sure we received a valid reply */
	M0_ASSERT(item != NULL);
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
	ut_kernel_fsync_count = 0;
	ut_post_rpc_count = 0;
	ut_wait_for_reply_count = 0;
	ut_fop_fini_count = 0;

	/* Ensure early/late behaviour does nothing for ut_post_rpc */
	ut_post_rpc_delay = 0;
}

/**
 * (Re-)initialises our fake filesystem of super_block, dentry and inode.
 * These are fed to the functions we test so that their container-of
 * mechanisms find objects of the correct type, with our fake values.
 */
static void fake_fs_setup(void)
{
	int i = 0;

	M0_SET0(&file);
	M0_SET0(&dentry);
	M0_SET0(&m0inode);
	M0_SET0(&super_block);
	M0_SET0(&csb);
	M0_SET0(&service);
	M0_SET0(&stx);

	/* Assemble the file so that m0t1fs_fsync believes it */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
	file.f_path.dentry = &dentry;
#else
	file.f_dentry = &dentry;
#endif
	dentry.d_inode = &m0inode.ci_inode;
	super_block.s_fs_info = &csb;
	m0inode.ci_inode.i_sb = &super_block;

	m0_mutex_init(&m0inode.ci_pending_tx_lock);
	ispti_tlist_init(&m0inode.ci_pending_tx);

	m0_mutex_init(&service.sc_max_pending_tx_lock);
	service.sc_type = M0_CST_IOS;
	service.sc_rlink.rlk_sess.s_conn = &conn;
	service.sc_rlink.rlk_sess.s_sm.sm_state = M0_RPC_SESSION_IDLE;

	/* Add some records that need fsyncing
	 * This creates @10 records, all for the same service that need
	 * syncing. This would never happen in reality.
	 * m0t1fs_fsync_record_update will only ever update the first service it
	 * finds, assuming that there are no duplicates.
	 */
	for (i = 0; i < NUM_STRECORDS; i++) {
		m0_tlink_init(&ispti_tl, &stx[i]);
		stx[i].stx_tri.tri_txid = 3;
		stx[i].stx_tri.tri_locality = 7;
		stx[i].stx_service_ctx = &service;
		ispti_tlist_add(&m0inode.ci_pending_tx, &stx[i]);
	}
}


/**
 * Tests the m0t1fs_fsync_request_create function.
 */
static void test_m0t1fs_fsync_request_create(void)
{
	int                              rv;
	struct m0t1fs_fsync_fop_wrapper *ffw;
	struct m0_reqh_service_txid      stx;
	struct m0_fop_fsync             *ffd;

	fake_fs_setup();

	M0_SET0(&stx);
	M0_SET0(&copy);

	/* Copy the fsync_interactions struct so that we can restore it to
	 * default values later */
	copy = fi;

	/* we need to override post rpc and fop_fini */
	fi.post_rpc = &ut_post_rpc;
	fi.fop_fini = &ut_fop_fini;
	fi.fop_put = &ut_fop_fini;

	/* Test the values get packed into the fop correctly */
	ut_reset_stub_counters();
	ut_post_rpc_return = 0;
	stx.stx_service_ctx = &service;
	stx.stx_tri.tri_txid = 4000ULL;
	stx.stx_tri.tri_locality = 11;
	rv = m0t1fs_fsync_request_create(&stx, &ffw, M0_FSYNC_MODE_ACTIVE);
	M0_UT_ASSERT(rv == 0);
	ffd = m0_fop_data(&ffw->ffw_fop);
	M0_UT_ASSERT(ffd != NULL);
	M0_UT_ASSERT(ffd->ff_be_remid.tri_txid == stx.stx_tri.tri_txid);
	M0_UT_ASSERT(ffd->ff_be_remid.tri_locality == stx.stx_tri.tri_locality);
	M0_UT_ASSERT(ffd->ff_fsync_mode == M0_FSYNC_MODE_ACTIVE);
	M0_UT_ASSERT(ut_fop_fini_count == 0);

	/* reset anything that got initalised */
	m0_fop_fini(&ffw->ffw_fop);
	m0_free(ffw);
	M0_SET0(&stx);

	/* cause post_rpc to fail */
	ut_reset_stub_counters();
	ut_post_rpc_return = -EINVAL;
	stx.stx_service_ctx = &service;
	stx.stx_tri.tri_txid = 4000ULL;
	stx.stx_tri.tri_locality = 99;
	rv = m0t1fs_fsync_request_create(&stx, &ffw, M0_FSYNC_MODE_ACTIVE);
	M0_UT_ASSERT(rv == ut_post_rpc_return);
	M0_UT_ASSERT(ut_fop_fini_count == 1);

	/* Restore the fsync_interactions struct */
	fi = copy;
}

int default_txid = 42;
int default_locality = 7;

void
test_m0t1fs_fsync_reply_process_init(struct m0t1fs_fsync_fop_wrapper   *ffw,
				     struct m0_reqh_service_txid *stx)
{
	struct m0_fop_fsync *ffd;

	/* Initialise the fops */
	m0_fop_init(&ffw->ffw_fop, &m0_fop_fsync_mds_fopt, NULL, &m0_fop_release);
	m0_fop_data_alloc(&ffw->ffw_fop);
	ffd = m0_fop_data(&ffw->ffw_fop);
	M0_UT_ASSERT(ffd != NULL);
	ffd->ff_be_remid.tri_txid = default_txid;
	ffd->ff_be_remid.tri_locality = default_locality;
	ffd->ff_fsync_mode = M0_FSYNC_MODE_ACTIVE;
	stx->stx_tri.tri_txid = default_txid;
	stx->stx_tri.tri_locality = default_locality;
	stx->stx_service_ctx = &service;
	reply_data.ffr_rc = 0;
	reply_data.ffr_be_remid.tri_txid = default_txid;
	reply_data.ffr_be_remid.tri_locality = default_locality;
	ffw->ffw_stx = stx;
}


void
call_m0t1fs_fsync_reply_process(struct m0t1fs_sb                *input_csb,
                                struct m0t1fs_inode             *input_m0inode,
                                struct m0t1fs_fsync_fop_wrapper *input_ffw,
                                int                              expect_return,
                                int                              expect_ut_wait_for_reply_count,
                                int                              expect_ut_fop_fini_count,
                                uint64_t                         expect_txid,
                                size_t                           expect_locality)
{
	int rv;

	rv = m0t1fs_fsync_reply_process(input_csb, input_m0inode, input_ffw);
	M0_UT_ASSERT(rv == expect_return);
	M0_UT_ASSERT(ut_wait_for_reply_count == expect_ut_wait_for_reply_count);
	M0_UT_ASSERT(ut_fop_fini_count == expect_ut_fop_fini_count);
	M0_UT_ASSERT(input_ffw->ffw_stx->stx_tri.tri_txid == expect_txid);
	M0_UT_ASSERT(input_ffw->ffw_stx->stx_tri.tri_locality ==
		     expect_locality);
}

/**
 * Tests the m0t1fs_fsync_reply_process function.
 */
void test_m0t1fs_fsync_reply_process(void)
{
	struct m0_reqh_service_txid     *iter;
	struct m0t1fs_fsync_fop_wrapper  ffw;
	struct m0_reqh_service_txid      stx;
	struct m0_fop_fsync             *ffd;

	fake_fs_setup();

	M0_SET0(&ffw);
	M0_SET0(&stx);
	M0_SET0(&ffd);
	M0_SET0(&copy);

	/* Copy the fsync_interactions struct so that we can restore it to
	 * default values later */
	copy = fi;

	/* we need to override wait for reply and fop_fini */
	fi.wait_for_reply = &ut_wait_for_reply;
	fi.fop_fini = &ut_fop_fini;
	fi.fop_put = &ut_fop_fini;

	/* Initialise the fops */
	test_m0t1fs_fsync_reply_process_init(&ffw, &stx);

	/* wait for reply fails */
	ut_reset_stub_counters();
	ut_wait_for_reply_return = -EINVAL;
	ut_wait_for_reply_remote_return = 0;
	call_m0t1fs_fsync_reply_process(NULL, &m0inode, &ffw,
	                                ut_wait_for_reply_return,
	                                1, 1, default_txid, default_locality);

	/* remote fop fails */
	test_m0t1fs_fsync_reply_process_init(&ffw, &stx);
	ut_reset_stub_counters();
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = -EINVAL;
	call_m0t1fs_fsync_reply_process(NULL, &m0inode, &ffw,
	                                ut_wait_for_reply_remote_return,
	                                1, 1, default_txid, default_locality);

	/* inode:transaction-to-sync increased while fop was in flight */
	/* super block record should still be updated - this can happen
	 * when the inode has been updated, and we raced to get the super_block
	 * lock before the thread updating the inode */
	test_m0t1fs_fsync_reply_process_init(&ffw, &stx);
	ffd = m0_fop_data(&ffw.ffw_fop);
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
	call_m0t1fs_fsync_reply_process(NULL, &m0inode, &ffw, 0,
	                                1, 1, 50, 2);
	/* Check superblock record was updated */
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	M0_UT_ASSERT(iter->stx_tri.tri_txid == 0);
	M0_UT_ASSERT(iter->stx_tri.tri_locality == 0);
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);

	/* super-block:transaction-to-sync increased while fop was in flight */
	/* inode record should still be updated - this can happen
	 * when another file was modified */
	test_m0t1fs_fsync_reply_process_init(&ffw, &stx);
	ffd = m0_fop_data(&ffw.ffw_fop);
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
	call_m0t1fs_fsync_reply_process(NULL, &m0inode, &ffw, 0,
	                                1, 1, 0, 0);
	/* Check superblock record was not updated */
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	M0_UT_ASSERT(iter->stx_tri.tri_txid != 0);
	M0_UT_ASSERT(iter->stx_tri.tri_locality != 0);
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);

	/* transaction-to-sync set to 0 on success */
	test_m0t1fs_fsync_reply_process_init(&ffw, &stx);
	ffd = m0_fop_data(&ffw.ffw_fop);
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
	call_m0t1fs_fsync_reply_process(NULL, &m0inode, &ffw, 0,
	                                1, 1, 0, 0);
	/* Check superblock record was updated */
	m0_mutex_lock(&service.sc_max_pending_tx_lock);
	iter = &service.sc_max_pending_tx;
	M0_UT_ASSERT(iter->stx_tri.tri_txid == 0);
	M0_UT_ASSERT(iter->stx_tri.tri_locality == 0);
	m0_mutex_unlock(&service.sc_max_pending_tx_lock);

	/* Restore the fsync_interactions struct */
	fi = copy;
}


/**
 * Tests the m0t1fs_fsync_record_update function.
 */
void test_m0t1fs_fsync_record_update(void)
{
	struct m0_reqh_service_txid *iter;
	struct m0_be_tx_remid        btr;

	fake_fs_setup();

	/* test the inode record is updated */
	btr.tri_txid = 50;
	btr.tri_locality = 3;
	m0t1fs_fsync_record_update(&service, NULL, &m0inode, &btr);
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
	m0t1fs_fsync_record_update(&service, NULL, &m0inode, &btr);
	M0_UT_ASSERT(iter->stx_tri.tri_txid == 999ULL);
	M0_UT_ASSERT(iter->stx_tri.tri_locality == 18);
}

void call_m0t1fs_fsync_core(struct m0t1fs_inode *input_m0inode,
                            int input_flag, int expect_return,
                            int expect_ut_post_rpc_count,
                            int expect_ut_wait_for_reply_count,
                            int expect_ut_fop_fini_count)
{
	int rv;

	rv = m0t1fs_fsync_core(input_m0inode, input_flag);
	M0_UT_ASSERT(rv == expect_return);
	M0_UT_ASSERT(ut_post_rpc_count == expect_ut_post_rpc_count);
	M0_UT_ASSERT(ut_wait_for_reply_count == expect_ut_wait_for_reply_count);
	M0_UT_ASSERT(ut_fop_fini_count == expect_ut_fop_fini_count);
}

/**
 * Tests the m0t1fs_fsync_core function.
 */
static void test_m0t1fs_fsync_core(void)
{
	int i;

	fake_fs_setup();

	M0_SET0(&reply_fop);
	M0_SET0(&reply_data);
	M0_SET0(&copy);

	/* Copy the fsync_interactions struct so that we can restore it to
	 * default values later */
	copy = fi;

	/* Load the stubs over the top of the fsync_interactions struct */
	fi.post_rpc = &ut_post_rpc;
	fi.wait_for_reply = &ut_wait_for_reply;
	fi.fop_fini = &ut_fop_fini;
	fi.fop_put = &ut_fop_fini;

	/* Cause fop sending to fail at the first attempt - check an error
	 * is returned and that we don't wait for a reply to the failed fop */
	ut_reset_stub_counters();
	ut_post_rpc_return = -EINVAL;
	ut_post_rpc_early_return = -EINVAL;
	call_m0t1fs_fsync_core(&m0inode, M0_FSYNC_MODE_ACTIVE,
	                       ut_post_rpc_return, 1, 0,1);

	/* Cause fop sending to fail after a few have been sent - check those
	 * that were sent correctly have their replies processed */
	ut_reset_stub_counters();
	ut_post_rpc_early_return = 0;
	ut_post_rpc_delay = 4;
	ut_post_rpc_return = -EINVAL;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_m0t1fs_fsync_core(&m0inode, M0_FSYNC_MODE_ACTIVE,
	                       ut_post_rpc_return, ut_post_rpc_delay,
	                       ut_post_rpc_delay-1, ut_post_rpc_delay);
	/* reset replies */
	for (i = 0; i < NUM_STRECORDS; i++)
		stx[i].stx_tri.tri_txid = 3;

	/* Cause a remote fop to fail - test the error is propogated */
	ut_reset_stub_counters();
	ut_post_rpc_early_return = 0;
	ut_post_rpc_delay = 2;
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = -EINVAL;
	call_m0t1fs_fsync_core(&m0inode, M0_FSYNC_MODE_ACTIVE,
	                       ut_wait_for_reply_remote_return,
	                       NUM_STRECORDS, NUM_STRECORDS, NUM_STRECORDS);
	/* reset replies */
	for (i = 0; i < NUM_STRECORDS; i++)
		stx[i].stx_tri.tri_txid = 3;

	/* Send replies for the versions requested - test records are updated
	 so that repeated calls to fsync have no effect */
	ut_reset_stub_counters();
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_m0t1fs_fsync_core(&m0inode, M0_FSYNC_MODE_ACTIVE, 0,
	                       NUM_STRECORDS, NUM_STRECORDS, NUM_STRECORDS);
	/* test the records were updated */
	for (i = 0; i < NUM_STRECORDS; i++)
		M0_UT_ASSERT(stx[i].stx_tri.tri_txid == 0);

	/* Don't reset replies ! */

	/* Test a repeated call to fsync causes no fops to be sent */
	ut_reset_stub_counters();
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
	call_m0t1fs_fsync_core(&m0inode, M0_FSYNC_MODE_ACTIVE, 0, 0, 0, 0);

	/* Restore the fsync_interactions struct */
	fi = copy;

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
void call_m0t1fs_fsync(struct file *input_file, loff_t start, loff_t end,
                       int input_datamode, int expect_return,
                       int expect_ut_kernel_fsync_count,
                       int expect_ut_post_rpc_count,
                       int expect_ut_fop_fini_count)
#else
void call_m0t1fs_fsync(struct file *input_file, struct dentry *input_dentry,
                       int input_datamode, int expect_return,
                       int expect_ut_kernel_fsync_count,
                       int expect_ut_post_rpc_count,
                       int expect_ut_fop_fini_count)
#endif
{
	int rv;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	rv = m0t1fs_fsync(input_file, start, end, input_datamode);
#else
	rv = m0t1fs_fsync(input_file, input_dentry, input_datamode);
#endif
	M0_UT_ASSERT(rv == expect_return);
	M0_UT_ASSERT(ut_kernel_fsync_count == expect_ut_kernel_fsync_count);
	M0_UT_ASSERT(ut_post_rpc_count == expect_ut_post_rpc_count);
	M0_UT_ASSERT(ut_fop_fini_count == expect_ut_fop_fini_count);
}

void fsync_test(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	/*
	 * fsync range in bytes, a whole file can be specified as [0, -1]
	 * see documentation of filemap_write_and_wait_range() in kernel
	 */
	loff_t start = 0;
	loff_t end   = -1;
#endif
	int    i;

	/* test our component pieces work first */
	test_m0t1fs_fsync_request_create();
	test_m0t1fs_fsync_reply_process();
	test_m0t1fs_fsync_record_update();
	test_m0t1fs_fsync_core();

	M0_SET0(&copy);

	/* Copy the fsync_interactions struct so that we can restore it to
	 * default values later */
	copy = fi;

	fake_fs_setup();
	fi.kernel_fsync = &ut_kernel_fsync;
	fi.post_rpc = &ut_post_rpc;
	fi.wait_for_reply = &ut_wait_for_reply;
	fi.fop_fini = &ut_fop_fini;
	fi.fop_put = &ut_fop_fini;

	/* Cause kernel simple_fsync to fail - check an error is
	 * returned and that no fops are sent */
	ut_reset_stub_counters();
	ut_kernel_fsync_return = -EIO;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	call_m0t1fs_fsync(&file, start, end, 0, ut_kernel_fsync_return, 1, 0, 0);
#else
	call_m0t1fs_fsync(&file, &dentry, 0, ut_kernel_fsync_return, 1, 0, 0);
#endif
	/* Check normal operation works */
	ut_reset_stub_counters();
	ut_kernel_fsync_return = 0;
	ut_post_rpc_return = 0;
	ut_wait_for_reply_return = 0;
	ut_wait_for_reply_remote_return = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	call_m0t1fs_fsync(&file, start, end, 0, 0, 1, NUM_STRECORDS,
			  NUM_STRECORDS);
#else
	call_m0t1fs_fsync(&file, &dentry, 0, 0, 1, NUM_STRECORDS,
	                  NUM_STRECORDS);
#endif
	/* test the records were updated */
	for (i = 0; i < NUM_STRECORDS; i++)
		M0_UT_ASSERT(stx[i].stx_tri.tri_txid == 0);

	/* Restore the fsync_interactions struct */
	fi = copy;
}
