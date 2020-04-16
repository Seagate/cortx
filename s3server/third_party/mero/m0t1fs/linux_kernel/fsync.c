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
 *                  Juan Gonzalez <juan_gonzalez@xyratex.com>,
 *                  Sining Wu     <sining_wu@xyratex.com>
 * Original creation date: 11-Apr-2014
 */

#include <linux/version.h>      /* LINUX_VERSION_CODE */
#include <linux/fs.h>           /* struct file_operations */
#include <linux/mount.h>        /* struct vfsmount (f_path.mnt) */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"
#include "mdservice/fsync_fops.h"       /* m0_fop_fsync_mds_fopt */
#include "fop/fom_generic.h"            /* m0_rpc_item_is_generic_reply_fop */
#include "lib/memory.h"                 /* m0_alloc, m0_free */
#include "lib/tlist.h"
#include "lib/hash.h"                   /* m0_htable */
#include "file/file.h"
#include "mero/magic.h"                 /* M0_T1FS_FFW_TLIST_MAGIC? */
#include "m0t1fs/linux_kernel/m0t1fs.h" /* m0t1fs_sb */

#include "layout/pdclust.h"             /* M0_PUT_*, m0_layout_to_pdl,
					 * m0_pdclust_instance_map */
#include "m0t1fs/linux_kernel/file_internal.h"

#include "m0t1fs/linux_kernel/fsync.h"  /* m0t1fs_fsync_interactions */

M0_TL_DESCR_DEFINE(fpf, "m0t1fs_fsync_fop_wrappers pending fsync-fops",
                   static, struct m0t1fs_fsync_fop_wrapper, ffw_tlink,
                   ffw_tlink_magic, M0_T1FS_FFW_TLIST_MAGIC1,
                   M0_T1FS_FFW_TLIST_MAGIC2);

M0_TL_DEFINE(fpf, static, struct m0t1fs_fsync_fop_wrapper);

/**
 * Ugly abstraction of m0t1fs_fsync interactions with wider mero code
 * - purely to facilitate unit testing
 */
struct m0t1fs_fsync_interactions fi = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	.kernel_fsync   = generic_file_fsync,
#else
	.kernel_fsync   = &simple_fsync,
#endif
	.post_rpc       = &m0_rpc_post,
	.wait_for_reply = &m0_rpc_item_wait_for_reply,
	/* fini is for requests, allocated in a bigger structure */
	.fop_fini       = &m0_fop_fini,
	/* put is for replies, allocated by a lower layer */
	.fop_put        = &m0_fop_put_lock,
};


/**
 * Cleans-up a fop.
 */
static void m0t1fs_fsync_fop_cleanup(struct m0_ref *ref)
{
	struct m0_fop                   *fop;
	struct m0t1fs_fsync_fop_wrapper *ffw;

	M0_ENTRY();
	M0_PRE(fi.fop_fini != NULL);

	fop = container_of(ref, struct m0_fop, f_ref);
	fi.fop_fini(fop);

	ffw = container_of(fop, struct m0t1fs_fsync_fop_wrapper, ffw_fop);
	m0_free(ffw);

	M0_LEAVE();
}


/**
 * Creates and sends an fsync fop from the provided m0_reqh_service_txid.
 * Allocates and returns the fop wrapper at @ffw_out on success,
 * which is freed on the last m0_fop_put().
 */
int m0t1fs_fsync_request_create(struct m0_reqh_service_txid      *stx,
                                struct m0t1fs_fsync_fop_wrapper **ffw_out,
                                enum m0_fsync_mode                mode)
{
	int                              rc;
	struct m0_fop                   *fop;
	struct m0_rpc_item              *item;
	struct m0_fop_fsync             *ffd;
	struct m0_fop_type              *fopt;
	struct m0t1fs_fsync_fop_wrapper *ffw;

	M0_ENTRY();

	M0_ALLOC_PTR(ffw);
	if (ffw == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_rpc_session_validate(&stx->stx_service_ctx->sc_rlink.rlk_sess);
	if (rc != 0) {
		m0_free(ffw);
		return M0_ERR(rc);
	}

	if (stx->stx_service_ctx->sc_type == M0_CST_MDS)
		fopt = &m0_fop_fsync_mds_fopt;
	else if (stx->stx_service_ctx->sc_type == M0_CST_IOS)
		fopt = &m0_fop_fsync_ios_fopt;
	else
		M0_IMPOSSIBLE("invalid service type:%d",
			      stx->stx_service_ctx->sc_type);

	/* store the pending txid reference with the fop */
	ffw->ffw_stx = stx;

	fop = &ffw->ffw_fop;
	m0_fop_init(fop, fopt, NULL, &m0t1fs_fsync_fop_cleanup);
	rc = m0_fop_data_alloc(fop);
	if (rc != 0) {
		m0_free(ffw);
		return M0_ERR_INFO(rc, "Allocating fsync fop data failed.");
	}

	ffd = m0_fop_data(fop);

	ffd->ff_be_remid = stx->stx_tri;
	ffd->ff_fsync_mode = mode;

	/*
	 *  We post the rpc_item directly so that this is asyncronous.
	 *  Prepare the fop as an rpc item
	 */
	item = &fop->f_item;
	item->ri_session         = &stx->stx_service_ctx->sc_rlink.rlk_sess;
	item->ri_prio            = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline        = 0;
	item->ri_nr_sent_max     = M0T1FS_RPC_MAX_RETRIES;
	item->ri_resend_interval = M0T1FS_RPC_RESEND_INTERVAL;

	rc = fi.post_rpc(item);
	if (rc != 0) {
		fi.fop_put(fop);
		return M0_ERR_INFO(rc, "Calling m0_rpc_post() failed.");
	}

	*ffw_out = ffw;
	M0_LEAVE();
	return 0;
}

static void fsync_stx_update(struct m0_reqh_service_txid *stx, uint64_t txid,
			     struct m0_mutex *lock)
{
	M0_PRE(stx != NULL);
	M0_PRE(lock != NULL);

	m0_mutex_lock(lock);
	if (stx->stx_tri.tri_txid <= txid) {
		/*
		 * Our transaction got committed update the record to be
		 * ignored in the future.
		 */
		M0_SET0(&stx->stx_tri);
	}
	/*
	 * Else the stx_maximum_txid got increased while we were waiting, it
	 * is acceptable for fsync to return as long as the
	 * correct-at-time-of-sending txn was committed (which the caller
	 * should assert).
	 */
	m0_mutex_unlock(lock);
}


/**
 * Waits for a reply to an fsync fop and process it.
 * Cleans-up the fop allocated in m0t1fs_fsync_request_create.
 *
 * inode may be NULL if the reply is only likely to touch the super block.
 * csb may be NULL, iff inode is specified.
 *
 */
int m0t1fs_fsync_reply_process(struct m0t1fs_sb                *csb,
                               struct m0t1fs_inode             *inode,
                               struct m0t1fs_fsync_fop_wrapper *ffw)
{
	int                         rc;
	uint64_t                    reply_txid;
	struct m0_fop              *fop;
	struct m0_rpc_item         *item;
	struct m0_fop_fsync        *ffd;
	struct m0_fop_fsync_rep    *ffr;
	struct m0_reqh_service_ctx *service;

	M0_ENTRY();

	if (csb == NULL)
		csb = m0inode_to_sb(inode);
	M0_PRE(csb != NULL);

	fop = &ffw->ffw_fop;
	item = &fop->f_item;

	rc = fi.wait_for_reply(item, m0_time_from_now(M0T1FS_RPC_TIMEOUT, 0));
	if (rc != 0)
		goto out;

	/* get the {fop,reply} data */
	ffd = m0_fop_data(fop);
	M0_ASSERT(ffd != NULL);
	ffr = m0_fop_data(m0_rpc_item_to_fop(item->ri_reply));
	M0_ASSERT(ffr != NULL);

	rc = ffr->ffr_rc;
	if (rc != 0) {
		M0_LOG(M0_ERROR, "reply rc=%d", rc);
		goto out;
	}

	/* Is this a valid reply to our request */
	reply_txid = ffr->ffr_be_remid.tri_txid;
	if (reply_txid < ffd->ff_be_remid.tri_txid) {
		/* invalid reply, network 'garbage'? */
		rc = -EPROTO;
		M0_LOG(M0_ERROR, "Commited transaction is smaller "
					    "than that requested.");
		goto out;
	}

	if (inode != NULL)
		fsync_stx_update(ffw->ffw_stx, reply_txid,
				 &inode->ci_pending_tx_lock);

	service = ffw->ffw_stx->stx_service_ctx;

	/*
	 * check the super block too, super block txid_record
	 * is embedded in the m0_reqh_service_ctx struct
	 */
	fsync_stx_update(&service->sc_max_pending_tx, reply_txid,
			 &service->sc_max_pending_tx_lock);
out:
	fi.fop_put(fop);

	return M0_RC(rc);
}


/**
 * m0t1fs fsync core sends an fsync-fop to a list of services, then blocks,
 * waiting for replies. This is implemented as two loops.
 * The 'fop sending loop', generates and posts fops, adding them to a list
 * of pending fops. This is all done while holding the
 * m0t1fs_indode::ci_service_pending_txid_map_lock. The 'reply receiving loop'
 * works over the list of pending fops, waiting for a reply for each one.
 * It acquires the m0t1fs_indode::ci_service_pending_txid_map_lock only
 * when necessary.
 */
int m0t1fs_fsync_core(struct m0t1fs_inode *inode, enum m0_fsync_mode mode)
{
	int                              rc;
	int                              saved_error = 0;
	struct m0_tl                     pending_fops;
	struct m0_reqh_service_txid     *iter;
	struct m0t1fs_fsync_fop_wrapper *ffw;

	M0_ENTRY();

	M0_PRE(inode != NULL);
	M0_PRE(fi.kernel_fsync != NULL);
	M0_PRE(fi.post_rpc != NULL);
	M0_PRE(fi.wait_for_reply != NULL);
	M0_PRE(fi.fop_fini != NULL);

	m0_tlist_init(&fpf_tl, &pending_fops);

	/*
	 * find the inode's list services with pending transactions
	 * for each entry, send an fsync fop.
	 * This is the fop sending loop.
	 */
	m0_mutex_lock(&inode->ci_pending_tx_lock);
	m0_tl_for(ispti, &inode->ci_pending_tx, iter) {
		/*
		 * send an fsync fop for
		 * iter->stx_maximum_txid to iter->stx_service_ctx
		 */

		/* Check if this service has any pending transactions. */
		if (iter->stx_tri.tri_txid == 0)
			continue;

		/* Create and send a request */
		rc = m0t1fs_fsync_request_create(iter, &ffw, mode);
		if (rc != 0) {
			saved_error = rc;
			break;
		} else
			/* Add to list of pending fops  */
			fpf_tlink_init_at(ffw, &pending_fops);
	} m0_tl_endfor;
	m0_mutex_unlock(&inode->ci_pending_tx_lock);

	/*
	 * At this point we may have sent some fops, but stopped when one
	 * failed - collect all the replies before returning.
	 */

	/* This is the fop-reply receiving loop. */
	m0_tl_teardown(fpf, &pending_fops, ffw) {
		/* Get and process the reply. */
		rc = m0t1fs_fsync_reply_process(NULL, inode, ffw);
		saved_error = saved_error ? : rc;
	}

	M0_LEAVE();
	return saved_error;
}


/**
 * Entry point for fsync, calls m0t1fs_fsync_core with mode=active
 *
 * parameter dentry is unused as we don't need it - it is removed in later
 * kernel versions
 *
 * parameter datasync is unused, as we only ever sync data. To sync metadata
 * would require a sync against the super-block, as all metadata currently
 * lives in container-zero.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
int m0t1fs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
#else
int m0t1fs_fsync(struct file *file, struct dentry *dentry, int datasync)
#endif
{
	int                  rc;
	struct m0t1fs_inode *inode;

	M0_THREAD_ENTER;
	M0_ENTRY();

	M0_PRE(file != NULL);
	inode = m0t1fs_file_to_m0inode(file);
	M0_PRE(inode != NULL);

	/*
	 * push any relevant changes we don't know about through m0t1fs_aio
	 * This call will block until all the data is sent to the server, and
	 * we have uptodate pending transaction-ids.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	rc = fi.kernel_fsync(file, start, end, datasync);
#else
	rc = fi.kernel_fsync(file, dentry, datasync);
#endif
	if (rc != 0) {
		/**
		 * Failure
		 * @todo: Generate some addb here.
		 */
		return M0_ERR_INFO(rc, "Simple_fsync returned error.");
	}

	M0_LEAVE();
	return m0t1fs_fsync_core(inode, M0_FSYNC_MODE_ACTIVE);
}


/**
 * Update a m0_reqh_service_txid with the specified be_tx_remid
 * if the struct m0_be_tx_remid::tri_txid > the stored value
 * inode may be NULL if the update has no associated inode.
 * csb may be NULL, iff inode is specified.
 */
void m0t1fs_fsync_record_update(struct m0_reqh_service_ctx *service,
                                struct m0t1fs_sb              *csb,
                                struct m0t1fs_inode           *inode,
                                struct m0_be_tx_remid         *btr)
{
	struct m0_reqh_service_txid *stx = NULL;

	M0_ENTRY();

	M0_PRE(service != NULL);
	if (csb == NULL)
		csb = m0inode_to_sb(inode);
	M0_PRE(csb != NULL);

	/* Updates pending transaction number in the inode */
	if (inode != NULL) {
		/*
		  * TODO: replace this O(N) search with something better.
		  * Embbed the struct m0_reqh_service_txid in a list of
		  * 'services for this inode'? See RB1667
		  */
		/* Find the record for this service */
		m0_mutex_lock(&inode->ci_pending_tx_lock);
		stx = m0_tl_find(ispti, stx,
				 &inode->ci_pending_tx,
				 stx->stx_service_ctx == service);

		if (stx != NULL) {
			if (btr->tri_txid > stx->stx_tri.tri_txid)
				stx->stx_tri = *btr;
		} else {
			/*
			 * not found - add a new record
			 */
			M0_ALLOC_PTR(stx);
			if (stx != NULL) {
				stx->stx_service_ctx = service;
				stx->stx_tri = *btr;

				ispti_tlink_init_at(stx, &inode->ci_pending_tx);
			}
		}
		m0_mutex_unlock(&inode->ci_pending_tx_lock);
	}

	/* update pending transaction number in the super block */
	m0_mutex_lock(&service->sc_max_pending_tx_lock);
	stx = &service->sc_max_pending_tx;
	/* update the value from the reply_fop */
	if (btr->tri_txid > stx->stx_tri.tri_txid) {
		stx->stx_service_ctx = service;
		stx->stx_tri = *btr;
	}
	m0_mutex_unlock(&service->sc_max_pending_tx_lock);

	M0_LEAVE("Fsync record updated.");
}


/**
 * Entry point for sync_fs, sends fsync-fops for each pending
 * transaction the super block is aware off. Unlike m0t1fs_fsync_core
 * this function acquires the sc_max_pending_tx_lock for each service,
 * as there is not a larger-granularity lock.
 */
int m0t1fs_sync_fs(struct super_block *sb, int wait)
{
	int                              rc;
	int                              saved_error = 0;
	struct m0_tl                     pending_fops;
	struct m0_reqh_service_txid     *stx;
	struct m0_reqh_service_ctx      *iter;
	struct m0t1fs_fsync_fop_wrapper *ffw;
	struct m0t1fs_sb                *csb;

	M0_THREAD_ENTER;
	M0_ENTRY();

	M0_PRE(fi.kernel_fsync != NULL);
	M0_PRE(fi.post_rpc != NULL);
	M0_PRE(fi.wait_for_reply != NULL);
	M0_PRE(fi.fop_fini != NULL);

	csb = M0T1FS_SB(sb);

	m0_tlist_init(&fpf_tl, &pending_fops);

	/*
	 *  loop over all services associated with this super block,
	 *  send an fsync fop for those with pending transactions
	 *
	 *  fop sending loop
	 */
	m0_tl_for(pools_common_svc_ctx, &csb->csb_pools_common.pc_svc_ctxs,
		  iter) {
		/*
		 * Send an fsync fop for iter->sc_max_pending_txt to iter.
		 */
		m0_mutex_lock(&iter->sc_max_pending_tx_lock);
		stx = &iter->sc_max_pending_tx;

		/*
		 * Check if this service has any pending transactions.
		 * Currently for fsync operations are supported only for
		 * ioservice and mdservice.
		 */
		if (stx->stx_tri.tri_txid == 0 ||
		    !M0_IN(stx->stx_service_ctx->sc_type,
			  (M0_CST_MDS, M0_CST_IOS))) {
			m0_mutex_unlock(&iter->sc_max_pending_tx_lock);
			continue;
		}

		/* Create and send a request */
		rc = m0t1fs_fsync_request_create(stx, &ffw,
		                                 M0_FSYNC_MODE_ACTIVE);
		if (rc != 0) {
			saved_error = rc;
			m0_mutex_unlock(&iter->sc_max_pending_tx_lock);
			break;
		} else {
			/* Add to list of pending fops */
			fpf_tlink_init_at(ffw, &pending_fops);
		}

		m0_mutex_unlock(&iter->sc_max_pending_tx_lock);
	} m0_tl_endfor;

	/*
	 * At this point we may have sent some fops, but stopped when one
	 * failed - collect all the replies before returning
	 */

	/* reply receiving loop */
	m0_tl_teardown(fpf, &pending_fops, ffw) {
		/* get and process the reply */
		rc = m0t1fs_fsync_reply_process(csb, NULL, ffw);
		saved_error = saved_error ? : rc;
	}

	M0_LEAVE();

	return saved_error;
}
