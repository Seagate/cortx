/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *          James  Morse    <james.s.morse@seagate.com>
 *          Sining Wu       <sining.wu@seagate.com>
 *          Pratik Shinde   <pratik.shinde@seagate.com>
 *
 * 27-Mar-2017: Modified by Sining and Pratik to support SYNC on index and ops.
 * 23-Jun-2015: Modified by Sining from m0t1fs/linux_kernel/fsync.c for object.
 * 11-Apr-2014: Original created for fsync in m0t1fs.
 */

#include "clovis/clovis_addb.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/sync.h"               /* clovis_sync_interactions */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"
#include "lib/finject.h"
#include "mdservice/fsync_fops.h"       /* m0_fop_fsync_mds_fopt */
#include "fop/fom_generic.h"            /* m0_rpc_item_is_generic_reply_fop */
#include "lib/memory.h"                 /* m0_alloc, m0_free */
#include "lib/tlist.h"
#include "lib/hash.h"                   /* m0_htable */
#include "file/file.h"
#include "mero/magic.h"                 /* M0_T1FS_FFW_TLIST_MAGIC? */
#include "pool/pool.h"                  /* pools_common_svc_ctx_tl */

/**
 * Clovis SYNC APIs follow the design described in m0t1fs/linux_kernel/fsync.h
 * and are implemented with the following modifications/extensions.
 *
 * (1) 3 APIs are added to create/initialise SYNC op and to add entities and
 *     operations to sync:
 *     m0_clovis_sync_op_init(): create a new SYNC op.
 *     m0_clovis_sync_entity_add(): Add an entity to SYNC op.
 *     m0_clovis_sync_op_add(): Add an op to SYNC op. The op can only added
 *     if its state is M0_CLOVIS_OS_STABLE or M0_CLOVIS_OS_EXECUTED.
 *
 *     m0t1fs_fsync() and m0t1fs_sync_fs() are changed to
 *     m0_clovis_entity_fsync() and m0_clovis_sync() respectively.
 *
 * (2) Add support to sync index.
 *     A new fop type m0_fop_fsync_cas_fopt is added. See its definition in
 *     cas/cas.c. CAS handles FSYNC fops as IOS and MDS do by setting fom_ops
 *     to &m0_fsync_fom_ops and sm to &m0_fsync_fom_conf.
 *
 * (3) For index UPDATE queries such as PUT and DEL, CAS piggy-backs the latest
 *     txid of an CAS servie in reply fop (via m0_cas_rep::cgr_mod_rep).
 *
 * (4) Clovis maintains a list of {service, txid} pairs for each SYNC op. This
 *     list is constructed/updated when new elements are added to SYNC op.
 *     For a newly added pair {svc, t2}, if there exists a pair {svc, t1} in
 *     the list, these 2 pairs are merged into one {svc, max(t1, t2)}.
 *
 * (5) An SYNC op (and request) may send multiple FSYNC fops to services. The
 *     process of reply fops is: (a) The rpc item callback,
 *     clovis_sync_ri_callback() is called to invoke the fop AST,
 *     clovis_sync_fop_ast, to process the reply fop. (b) Posts SYNC request's
 *     AST, clovis_sync_request_ast, if it is the last reply fop for the SYNC
 *     request. (c) SYNC op's state is moved to STABLE only if every FSYNC fop
 *     is executed successfully.
 *
 * (6) Each entity and op has a list of pending service txid. The pending txid
 *     is updated when any UPDATE fop on entity is replied with latest txid of
 *     a service. The update process is added for dix/cas. Updating txid for cas
 *     is straightforward. For dix, an function pointer
 *     m0_dix_cli::dx_sync_rec_update is added and clovis related information
 *     (entity and op) is passed to dix via m0_dix_req::dr_sync_datum.
 */

static const struct m0_bob_type os_bobtype;
M0_BOB_DEFINE(static, &os_bobtype,  m0_clovis_op_sync);
static const struct m0_bob_type os_bobtype = {
	.bt_name         = "os_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_op_sync, os_magic),
	.bt_magix        = M0_CLOVIS_OS_MAGIC,
	.bt_check        = NULL,
};

M0_TL_DESCR_DEFINE(clovis_sync_target,
		   "Targets to synced for a clovis SYNC request",
                   static, struct clovis_sync_target,
		   srt_tlink, srt_tlink_magic,
		   M0_CLOVIS_SYNC_TGT_TL_MAGIC, M0_CLOVIS_SYNC_TGT_TL_MAGIC);

M0_TL_DEFINE(clovis_sync_target, static, struct clovis_sync_target);

/* TODO: use Clovis-defined magic values */
M0_TL_DESCR_DEFINE(spf, "clovis_sync_fop_wrappers pending fsync-fops",
                   static, struct clovis_sync_fop_wrapper, sfw_tlink,
                   sfw_tlink_magic, M0_T1FS_FFW_TLIST_MAGIC1,
                   M0_T1FS_FFW_TLIST_MAGIC2);

M0_TL_DEFINE(spf, static, struct clovis_sync_fop_wrapper);

/* SPTI -> Service to Pending Transaction Id list */
M0_TL_DESCR_DEFINE(spti, "m0_reqh_service_txid pending list", M0_INTERNAL,
			struct m0_reqh_service_txid,
			stx_tlink, stx_link_magic,
			M0_CLOVIS_INSTANCE_PTI_MAGIC,
			M0_CLOVIS_INSTANCE_PTI_MAGIC);

M0_TL_DEFINE(spti, M0_INTERNAL, struct m0_reqh_service_txid);

/**
 * Ugly abstraction of clovis_sync interactions with wider mero code
 * - purely to facilitate unit testing.
 */
static struct clovis_sync_interactions si = {
	.si_post_rpc       = &m0_rpc_post,
	.si_wait_for_reply = &m0_rpc_item_wait_for_reply,
	/* fini is for requests, allocated in a bigger structure */
	.si_fop_fini       = &m0_fop_fini,
	/* put is for replies, allocated by a lower layer */
	.si_fop_put        = &m0_fop_put_lock,
};

/**
 * Cleans-up a fop.
 */
static void clovis_sync_fop_cleanup(struct m0_ref *ref)
{
	struct m0_fop                  *fop;
	struct clovis_sync_fop_wrapper *sfw;

	M0_ENTRY();
	M0_PRE(si.si_fop_fini != NULL);

	fop = M0_AMB(fop, ref, f_ref);
	si.si_fop_fini(fop);

	sfw = M0_AMB(sfw, fop, sfw_fop);
	m0_free(sfw);

	M0_LEAVE("clovis_sync_fop_cleanup");
}

static void clovis_sync_request_done_locked(struct clovis_sync_request *sreq)
{
	struct m0_clovis_op      *op;
	struct m0_clovis_op_sync *os;

	os = sreq->sr_op_sync;
	op = &os->os_oc.oc_op;

	/* Updates SYNC op's state. */
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	m0_clovis_op_executed(op);

	/*
	 * Currently, an SYNC request is considered success only if all services
	 * involved persist requested txid successfully.
	 * TODO: in some cases an SYNC request can be considered success even
	 * some FSYNC fops fail. For example, when an application writs
	 * to an N+K object, it is not necessary to wait for all N+K services.
	 * N services returnning successfully should be considered success for
	 * an SYNC request.
	 */
	/* A server reply will cause the op to complete stably,
	 * with return code being reported in op->op_rc */
	op->op_rc = sreq->sr_rc;
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_STABLE);
	m0_clovis_op_stable(op);
}

static void clovis_sync_request_done(struct clovis_sync_request *sreq)
{
	struct m0_sm_group *op_grp;

	op_grp = &sreq->sr_op_sync->os_oc.oc_op.op_sm_group;
	m0_sm_group_lock(op_grp);
	clovis_sync_request_done_locked(sreq);
	m0_sm_group_unlock(op_grp);
}

/*
 * AST callback for completion of an SYNC request (all fops have been replied).
 * The AST is in the sm group of SYNC op.
 */
static void clovis_sync_request_ast(struct m0_sm_group *grp,
				    struct m0_sm_ast *ast)
{
	struct clovis_sync_request *sreq;

	M0_ENTRY();

	M0_PRE(ast != NULL);
	M0_PRE(grp != NULL);
	M0_PRE(m0_sm_group_is_locked(grp));

	sreq = M0_AMB(sreq, ast, sr_ast);
	clovis_sync_request_done(sreq);

	M0_LEAVE();
}

/**
 * It's called after the FSYNC reply fop is received and processed. Posts SYNC
 * request's AST if it is the last reply fop for the SYNC request.
 */
static void clovis_sync_fop_done(struct clovis_sync_fop_wrapper *sfw, int rc)
{
	struct m0_clovis_op_sync       *os;
	struct clovis_sync_request     *sreq;

	M0_ENTRY();

	sreq = sfw->sfw_req;
	os = sreq->sr_op_sync;

	m0_mutex_lock(&sreq->sr_fops_lock);
	sreq->sr_nr_fops--;
	spf_tlist_del(sfw);
	if (sreq->sr_nr_fops == 0) {
		/* All fops for this SYNC request have been replied. */
		sreq->sr_rc = sreq->sr_rc?:rc;
		m0_sm_ast_post(os->os_sm_grp, &sreq->sr_ast);
	}
	m0_mutex_unlock(&sreq->sr_fops_lock);

	M0_LEAVE();
}

/**
 * Update stx (m0_reqh_service_txid) of an entity or op after receiving
 * FSYNC FOP reply from service.
 */
static void clovis_sync_fop_stx_update(struct m0_reqh_service_txid *stx,
				       uint64_t txid)
{
	M0_PRE(stx != NULL);

	if (stx->stx_tri.tri_txid <= txid) {
		/*
		 * Our transaction got committed, update the record to be
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
}

/**
 * Processes a reply to an fsync fop.
 */
static int clovis_sync_reply_process(struct clovis_sync_fop_wrapper *sfw)
{
	int                          rc;
	uint64_t                     reply_txid;
	struct m0_fop               *fop;
	struct m0_fop_fsync         *ffd;
	struct m0_fop_fsync_rep     *ffr;
	struct m0_rpc_item          *item;
	struct m0_reqh_service_ctx  *service;
	struct m0_reqh_service_txid *stx;
	struct m0_tl                *pending_tx_tl;
	struct m0_mutex             *pending_tx_lock;
	struct m0_clovis_entity     *ent;
	struct m0_clovis_op         *op;
	struct clovis_sync_request  *sreq;
	struct clovis_sync_target   *tgt;

	M0_ENTRY();

	M0_PRE(sfw != NULL);

	fop  = &sfw->sfw_fop;
	sreq = sfw->sfw_req;
	item = &fop->f_item;
	rc = m0_rpc_item_error(item);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "rpc item error = %d", rc);
		goto out;
	}

	/* Gets the {fop,reply} data */
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
		/* Error (most likely) caused by an ioservice. */
		rc = M0_ERR(-EIO);
		M0_LOG(M0_ERROR, "Committed transaction is smaller "
					    "than that requested.");
		goto out;
	}

	/* Update the stx stored in sfw to avoid sending repeated fops. */
	clovis_sync_fop_stx_update(sfw->sfw_stx, reply_txid);

	/* Update matched stx in all targets of an SYNC request. */
	service = sfw->sfw_stx->stx_service_ctx;
	m0_tl_for(clovis_sync_target, &sreq->sr_targets, tgt) {
		if (tgt->srt_type == CLOVIS_SYNC_ENTITY) {
			ent = tgt->u.srt_ent;
			pending_tx_tl = &ent->en_pending_tx;
			pending_tx_lock = &ent->en_pending_tx_lock;
		} else if (tgt->srt_type == CLOVIS_SYNC_OP) {
			op = tgt->u.srt_op;
			pending_tx_tl = &op->op_pending_tx;
			pending_tx_lock = &op->op_pending_tx_lock;
		} else
			M0_IMPOSSIBLE("SYNC type not supported yet.");

		m0_mutex_lock(pending_tx_lock);
		stx = m0_tl_find(spti, stx, pending_tx_tl,
				 stx->stx_service_ctx == service);
		if (stx != NULL)
			clovis_sync_fop_stx_update(stx, reply_txid);
		m0_mutex_unlock(pending_tx_lock);
	} m0_tl_endfor;

	/* Updates Clovis instance wide stx. */
	m0_mutex_lock(&service->sc_max_pending_tx_lock);
	clovis_sync_fop_stx_update(&service->sc_max_pending_tx, reply_txid);
	m0_mutex_unlock(&service->sc_max_pending_tx_lock);

out:
	return M0_RC(rc);
}

/**
 * AST callback to a FSYNC fop.
 */
static void clovis_sync_fop_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	int                             rc;
	struct clovis_sync_fop_wrapper *sfw;

	M0_ENTRY();

	M0_PRE(grp != NULL);
	M0_PRE(m0_sm_group_is_locked(grp));
	M0_PRE(ast != NULL);

	sfw = M0_AMB(sfw, ast, sfw_ast);
	rc = clovis_sync_reply_process(sfw);
	clovis_sync_fop_done(sfw, rc);

	M0_LEAVE();
}

/**
 * rio_replied RPC callback to be executed whenever a reply to FSYNC fop
 * is received.
 */
static void clovis_sync_rio_replied(struct m0_rpc_item *item)
{
	struct m0_fop                  *fop;
	struct m0_clovis_op_sync       *os;
	struct clovis_sync_fop_wrapper *sfw;

	M0_ENTRY();

	fop = m0_rpc_item_to_fop(item);
	sfw = M0_AMB(sfw, fop, sfw_fop);
	os = sfw->sfw_req->sr_op_sync;

	/* Trigger AST for post processing FSYNC fop. */
	sfw->sfw_ast.sa_cb = &clovis_sync_fop_ast;
	m0_sm_ast_post(os->os_sm_grp, &sfw->sfw_ast);

	M0_LEAVE();
	return;
}

/**
 * RPC callbacks for the posting of FSYNC fops to services.
 */
static const struct m0_rpc_item_ops clovis_sync_ri_ops = {
	.rio_replied = clovis_sync_rio_replied,
};

/**
 * Creates and sends an fsync fop from the provided m0_reqh_service_txid.
 * Allocates and returns the fop wrapper at @sfw_out on success,
 * which is freed on the last m0_fop_put().
 */
static
int clovis_sync_request_fop_send(struct clovis_sync_request      *sreq,
				 struct m0_reqh_service_txid     *stx,
				 enum m0_fsync_mode               mode,
				 bool                             set_ri_ops,
				 struct clovis_sync_fop_wrapper **sfw_out)
{
	int                             rc;
	struct m0_fop                  *fop;
	struct m0_rpc_item             *item;
	struct m0_fop_fsync            *ffd;
	struct m0_fop_type             *fopt;
	struct clovis_sync_fop_wrapper *sfw;

	M0_ENTRY();

	M0_ALLOC_PTR(sfw);
	if (sfw == NULL)
		return M0_ERR(-ENOMEM);

	/* Stores the pending txid reference with the fop */
	sfw->sfw_stx = stx;
	sfw->sfw_req = sreq;

	rc = m0_rpc_session_validate(&stx->stx_service_ctx->sc_rlink.rlk_sess);
	if (rc != 0) {
		m0_free(sfw);
		return M0_ERR_INFO(rc, "Service tx session invalid");
	}

	if (stx->stx_service_ctx->sc_type == M0_CST_MDS)
		fopt = &m0_fop_fsync_mds_fopt;
	else if (stx->stx_service_ctx->sc_type == M0_CST_IOS)
		fopt = &m0_fop_fsync_ios_fopt;
	else if (stx->stx_service_ctx->sc_type == M0_CST_CAS)
		fopt = &m0_fop_fsync_cas_fopt;
	else
		M0_IMPOSSIBLE("invalid service type:%d",
			      stx->stx_service_ctx->sc_type);

	fop = &sfw->sfw_fop;
	m0_fop_init(fop, fopt, NULL, &clovis_sync_fop_cleanup);
	rc = m0_fop_data_alloc(fop);
	if (rc != 0) {
		m0_free(sfw);
		return M0_ERR_INFO(rc, "Allocating sync fop data failed.");
	}

	ffd = m0_fop_data(fop);
	ffd->ff_be_remid = stx->stx_tri;
	ffd->ff_fsync_mode = mode;

	/*
	 *  Posts the rpc_item directly so that this is asyncronous.
	 *  Prepare the fop as an rpc item
	 */
	item = &fop->f_item;
	item->ri_ops = (set_ri_ops == true)?&clovis_sync_ri_ops:NULL;
	item->ri_session = &stx->stx_service_ctx->sc_rlink.rlk_sess;
	item->ri_prio = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = 0;
	item->ri_nr_sent_max = CLOVIS_RPC_MAX_RETRIES;
	item->ri_resend_interval = CLOVIS_RPC_RESEND_INTERVAL;

	rc = si.si_post_rpc(item);
	if (rc != 0) {
		si.si_fop_fini(fop);
		return M0_ERR_INFO(rc, "Calling m0_rpc_post() failed.");
	}

	*sfw_out = sfw;
	return M0_RC(0);
}

/*
 * If wait_after_launch == true, caller of this function will wait for
 * the FSYNC reply fops.
 */
static int clovis_sync_request_launch(struct clovis_sync_request *sreq,
				      enum m0_fsync_mode mode,
				      bool wait_after_launch)
{
	int                             rc;
	int                             saved_error = 0;
	struct m0_reqh_service_txid    *iter;
	struct clovis_sync_fop_wrapper *sfw = NULL;
	struct m0_tl                   *stx_tl;

	M0_ENTRY();

	if (M0_FI_ENABLED("launch_failed"))
		return M0_ERR(-EAGAIN);

	/*
	 * Finds the services with pending transactions for each entry,
	 * send an fsync fop. This is the fop sending loop.
	 */
	m0_mutex_lock(&sreq->sr_fops_lock);
	stx_tl = &sreq->sr_stxs;
	m0_tl_for(spti, stx_tl, iter) {
		/*
		 * Sends an fsync fop for
		 * iter->stx_maximum_txid to iter->stx_service_ctx
		 */

		/* Checks if this service has any pending transactions. */
		if (iter->stx_tri.tri_txid == 0)
			continue;

		/* Creates and sends an FSYNC fop. */
		rc = clovis_sync_request_fop_send(sreq, iter, mode,
						  !wait_after_launch, &sfw);
		if (rc != 0) {
			saved_error = rc;
			break;
		} else {
			/* Add to list of pending fops  */
			M0_CNT_INC(sreq->sr_nr_fops);
			spf_tlink_init_at(sfw, &sreq->sr_fops);
		}
	} m0_tl_endfor;

	/*
	 * How to handle those fops which have been sent?
	 * (1) Cancel them by calling m0_rpc_item_cancel,
	 * (2) Simply treat them as 'normal' fsync fops and let rpc item
	 *     callback to handle the replies.
	 * As m0_rpc_item_cancel() only cancels rpc item locally, fops are still
	 * executed in service side, clovis will choose the option 2 to allow
	 * clovis to update its records on FSYNC.
	 *
	 * Returns saved_error directly only if no fops are sent, otherwise the
	 * error is stored in clovis_sync_request::sr_rc.
	 */
	if (sreq->sr_nr_fops == 0) {
		if (saved_error == 0)
			/*
			 * It may happen when there are no pending txid. For
			 * example, sync op is launched even before
			 * a write request gets replies and sets txid.
			 */
			rc = -EAGAIN;
		else
			rc = M0_ERR(saved_error);
	} else {
		sreq->sr_rc = saved_error != 0 ? M0_ERR(saved_error) :
				saved_error;
		rc = 0;
	}
	m0_mutex_unlock(&sreq->sr_fops_lock);
	return M0_RC(rc);
}

/**
 * Waits for a reply to an fsync fop and process it.
 * Cleans-up the fop allocated in clovis_sync_request_create.
 */
static int clovis_sync_reply_wait(struct clovis_sync_fop_wrapper *sfw)
{
	int                 rc;
	struct m0_rpc_item *item;
	struct m0_fop      *fop;

	M0_ENTRY();

	fop = &sfw->sfw_fop;
	item = &fop->f_item;

	rc = si.si_wait_for_reply(item, m0_time_from_now(CLOVIS_RPC_TIMEOUT, 0));
	if (rc != 0)
		goto out;

	rc = clovis_sync_reply_process(sfw);

out:
	si.si_fop_put(fop);

	return M0_RC(rc);
}

/**
 * Clovis sends an fsync-fop to a list of services, then blocks,
 * waiting for replies. This is implemented as two loops.
 * The 'fop sending loop', generates and posts fops, adding them to a list
 * of pending fops. This is all done while holding the
 * m0_clovis_entity::en_pending_tx_lock. The 'reply receiving loop'
 * works over the list of pending fops, waiting for a reply for each one.
 * It acquires the m0_clovis_obj::ob_pending_tx_map_lock only
 * when necessary.
 */
static int clovis_sync_request_launch_and_wait(struct clovis_sync_request *sreq,
					       enum m0_fsync_mode mode)
{
	int                             rc;
	int                             saved_error;
	struct clovis_sync_fop_wrapper *sfw;

	M0_ENTRY();

	M0_PRE(sreq != NULL);

	/*
	 * After calling clovis_sync_core() we may have sent some fops,
	 * but stopped when one failed - collect all the replies before
	 * returning.
	 */
	rc = clovis_sync_request_launch(sreq, mode, true);
	if (rc != 0)
		return M0_ERR(rc);

	/* This is the fop-reply receiving loop. */
	saved_error = sreq->sr_rc;
	m0_tl_teardown(spf, &sreq->sr_fops, sfw) {
		/* Get and process the reply. */
		rc = clovis_sync_reply_wait(sfw);
		saved_error = saved_error ? : rc;
	}

	return M0_RC(saved_error);
}

static int clovis_sync_request_target_add(struct clovis_sync_request *sreq,
					  int type, void *target)
{
	struct m0_clovis_entity    *ent;
	struct m0_clovis_op        *op;
	struct clovis_sync_target  *stgt;

	M0_ENTRY();

	M0_ALLOC_PTR(stgt);
	if (stgt == NULL)
		return M0_ERR(-ENOMEM);

	switch(type) {
	case CLOVIS_SYNC_ENTITY:
		ent = (struct m0_clovis_entity *)target;
		M0_ASSERT(M0_IN(ent->en_type,
				(M0_CLOVIS_ET_OBJ, M0_CLOVIS_ET_IDX)));
		stgt->srt_type = CLOVIS_SYNC_ENTITY;
		stgt->u.srt_ent = ent;
		break;
	case CLOVIS_SYNC_OP:
		/*
		 * Only those ops which have been executed and received txid
		 * can be sync-ed in current version.
		 */
		op = (struct m0_clovis_op *)target;
		stgt->srt_type = CLOVIS_SYNC_OP;
		stgt->u.srt_op = op;
		break;
	case CLOVIS_SYNC_INSTANCE:
		stgt->srt_type = CLOVIS_SYNC_INSTANCE;
		break;
	default:
		M0_IMPOSSIBLE("Unknow type for SYNC request.");
		break;
	}
	clovis_sync_target_tlink_init_at(stgt, &sreq->sr_targets);

	return M0_RC(0);
}

static int clovis_sync_request_stx_add(struct clovis_sync_request *sreq,
				       struct m0_tl *pending_tx_tl,
				       struct m0_mutex *pending_tx_lock)
{
	int                          i = 0;
	int                          rc = 0;
	int			     nr_saved_stxs = 0;
	struct m0_reqh_service_txid *saved_stxs;
	struct m0_reqh_service_txid *stx = NULL;
	struct m0_reqh_service_txid *iter;

	m0_mutex_lock(pending_tx_lock);

	M0_ALLOC_ARR(saved_stxs, spti_tlist_length(pending_tx_tl));
	if (saved_stxs == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto error;
	}

	m0_tl_for(spti, pending_tx_tl, iter) {
		/* Find the record for this service */
		stx = m0_tl_find(spti, stx, &sreq->sr_stxs,
				 stx->stx_service_ctx == iter->stx_service_ctx);
		if (stx != NULL) {
			saved_stxs[nr_saved_stxs].stx_service_ctx =
							stx->stx_service_ctx;
			saved_stxs[nr_saved_stxs].stx_tri = stx->stx_tri;
			if (iter->stx_tri.tri_txid > stx->stx_tri.tri_txid)
				stx->stx_tri = iter->stx_tri;
		} else {
			/* Not found - add a new stx. */
			M0_ALLOC_PTR(stx);
			if (stx == NULL) {
				rc = M0_ERR(-ENOMEM);
				goto undo;
			}
			stx->stx_service_ctx = iter->stx_service_ctx;
			stx->stx_tri = iter->stx_tri;
			spti_tlink_init_at(stx, &sreq->sr_stxs);
		}
		nr_saved_stxs++;
	} m0_tl_endfor;

	m0_free(saved_stxs);
	m0_mutex_unlock(pending_tx_lock);
	return M0_RC(0);

undo:
	m0_tl_for(spti, pending_tx_tl, iter) {
		stx = m0_tl_find(spti, stx, &sreq->sr_stxs,
				 stx->stx_service_ctx == iter->stx_service_ctx);
		M0_ASSERT(stx != NULL);

		if (saved_stxs[i].stx_service_ctx == NULL)
			/* It is a newly created stx, so simply remove it. */
			spti_tlist_del(stx);
		else if (stx->stx_tri.tri_txid >
			saved_stxs[i].stx_tri.tri_txid) {
			/* Retores the old value. */
			M0_ASSERT(stx->stx_service_ctx ==
				  saved_stxs[i].stx_service_ctx);
			stx->stx_tri = saved_stxs[i].stx_tri;
		}

		i++;
		if ( i == nr_saved_stxs)
			break;
	} m0_tl_endfor;

error:
	m0_free(saved_stxs);
	m0_mutex_unlock(pending_tx_lock);
	return M0_RC(rc);
}

static void clovis_sync_pending_stx_update(struct m0_reqh_service_ctx *service,
					   struct m0_mutex *pending_tx_lock,
					   struct m0_tl *pending_tx,
					   struct m0_be_tx_remid *btr)
{
	struct m0_reqh_service_txid *stx;

	/*
	  * TODO: replace this O(N) search with something better.
	  * Embbed the struct m0_reqh_service_txid in a list of
	  * 'services for this inode'? See RB1667
	  */
	/* Find the record for this service */
	m0_mutex_lock(pending_tx_lock);
	stx = m0_tl_find(spti, stx, pending_tx,
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

			spti_tlink_init_at(stx, pending_tx);
		}
	}
	m0_mutex_unlock(pending_tx_lock);
}

/**
 * Updates a m0_reqh_service_txid with the specified be_tx_remid
 * if the struct m0_be_tx_remid::tri_txid > the stored value
 * obj may be NULL if the update has no associated inode.
 */
void clovis_sync_record_update(struct m0_reqh_service_ctx *service,
                                struct m0_clovis_entity    *ent,
				struct m0_clovis_op        *op,
                                struct m0_be_tx_remid      *btr)
{
	struct m0_reqh_service_txid *stx = NULL;

	M0_ENTRY();

	M0_PRE(service != NULL);

	/* Updates pending transaction number in the entity. */
	if ( ent!= NULL)
		clovis_sync_pending_stx_update(
			service, &ent->en_pending_tx_lock,
			&ent->en_pending_tx, btr);

	/* Updates pending transaction number in the op. */
	if ( op!= NULL)
		clovis_sync_pending_stx_update(
			service, &op->op_pending_tx_lock,
			&op->op_pending_tx, btr);

	/* update pending transaction number in the Clovis instance */
	m0_mutex_lock(&service->sc_max_pending_tx_lock);
	stx = &service->sc_max_pending_tx;
	/* update the value from the reply_fop */
	if (btr->tri_txid > stx->stx_tri.tri_txid) {
		stx->stx_service_ctx = service;
		stx->stx_tri = *btr;
	}
	m0_mutex_unlock(&service->sc_max_pending_tx_lock);

	M0_LEAVE("Clovis sync record updated.");
}

/**----------------------------------------------------------------------------*
 *                           Clovis SYNC APIS                                  *
 *-----------------------------------------------------------------------------*/
/**
 * Checks an SYNC operation is not malformed or corrupted.
 */
static bool clovis_sync_op_invariant(struct m0_clovis_op_sync *os)
{
	return M0_RC(os != NULL &&
		     m0_clovis_op_sync_bob_check(os) &&
		     os->os_oc.oc_op.op_size >= sizeof *os &&
		     m0_clovis_ast_rc_bob_check(&os->os_ar) &&
		     m0_clovis_op_common_bob_check(&os->os_oc));
}

/**
 * Callback for an SYNC operation being finalised.
 */
static void clovis_sync_op_cb_fini(struct m0_clovis_op_common *oc)
{
	struct m0_clovis_op_sync *os;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	M0_PRE(oc->oc_op.op_size >= sizeof *os);

	os = bob_of(oc, struct m0_clovis_op_sync, os_oc, &os_bobtype);
	M0_PRE(clovis_sync_op_invariant(os));
	m0_clovis_op_common_bob_fini(&os->os_oc);
	m0_clovis_ast_rc_bob_fini(&os->os_ar);
	m0_clovis_op_sync_bob_fini(os);

	M0_LEAVE();
}

/**
 * 'free entry' on the operations vector for SYNC operations.
 */
static void clovis_sync_op_cb_free(struct m0_clovis_op_common *oc)
{
	struct m0_clovis_op_sync    *os;
	struct clovis_sync_target   *tgt;
	struct m0_reqh_service_txid *stx = NULL;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	M0_PRE((oc->oc_op.op_size >= sizeof *os));

	/* By now, fini() has been called and bob_of cannot be used */
	os = M0_AMB(os, oc, os_oc);
	m0_tl_teardown(spti, &os->os_req->sr_stxs, stx)
		m0_free(stx);
	m0_tl_teardown(clovis_sync_target, &os->os_req->sr_targets, tgt)
		m0_free(tgt);
	m0_free(os->os_req);
	m0_free(os);

	M0_LEAVE();
}

/**
 * Callback for an SYNC operation being launched.
 */
static void clovis_sync_op_cb_launch(struct m0_clovis_op_common *oc)
{
	int                         rc = 0;
	struct m0_clovis_op        *op;
	struct m0_clovis_op_sync   *os;
	struct clovis_sync_request *sreq;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	os = bob_of(oc, struct m0_clovis_op_sync, os_oc, &os_bobtype);
	op = &oc->oc_op;

	sreq = os->os_req;
	rc = clovis_sync_request_launch(sreq, os->os_mode, false);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_LAUNCHED);

	if (rc != 0) {
		/*
	 	 * If the SYNC request is not sent to services, update the op's
		 * state here. Note: m0_clovis_op_launch_one() has held the
		 * group lock.
		 */
		sreq->sr_rc = sreq->sr_rc?:rc;
		clovis_sync_request_done_locked(sreq);
	}
}

static void clovis_sync_request_init(struct clovis_sync_request *sreq)
{
	M0_SET0(sreq);
	spf_tlist_init(&sreq->sr_fops);
	spti_tlist_init(&sreq->sr_stxs);
	clovis_sync_target_tlist_init(&sreq->sr_targets);
	m0_mutex_init(&sreq->sr_fops_lock);
}

static int clovis_sync_op_init(struct m0_clovis_op *op)
{
	int                         rc;
	struct m0_clovis_op_common *oc;
	struct m0_clovis_op_sync   *os;
	struct m0_locality         *locality;
	struct clovis_sync_request *sreq;

	M0_ENTRY();

	op->op_code = M0_CLOVIS_EO_SYNC;
	rc = m0_clovis_op_init(op, &clovis_op_conf, NULL);
	if (rc != 0)
		return M0_RC(rc);
	/*
	 * Initialise m0_clovis_op_common part.
	 * bob_init()'s haven't been called yet: we use M0_AMB().
	 */
	oc = M0_AMB(oc, op, oc_op);
	os = M0_AMB(os, oc, os_oc);
	os->os_mode = M0_FSYNC_MODE_ACTIVE;

	m0_clovis_op_common_bob_init(oc);
	oc->oc_cb_launch = clovis_sync_op_cb_launch;
	oc->oc_cb_fini   = clovis_sync_op_cb_fini;
	oc->oc_cb_free   = clovis_sync_op_cb_free;

	/* Allocates and initialises a sync request. */
	M0_ALLOC_PTR(sreq);
	if (sreq == NULL)
		return M0_ERR(-ENOMEM);
	clovis_sync_request_init(sreq);
	sreq->sr_op_sync = os;
	sreq->sr_ast.sa_cb = clovis_sync_request_ast;
	os->os_req = sreq;

	/* Picks a locality thread for this op. */
	locality = m0_clovis__locality_pick(NULL);
	M0_ASSERT(locality != NULL);
	os->os_sm_grp = locality->lo_grp;
	M0_SET0(&os->os_ar);

	m0_clovis_op_sync_bob_init(os);
	m0_clovis_ast_rc_bob_init(&os->os_ar);

	return M0_RC(0);
}

int m0_clovis_sync_op_init(struct m0_clovis_op **sop)
{
	int rc = 0;

	M0_ENTRY();

	rc = m0_clovis_op_alloc(sop, sizeof(struct m0_clovis_op_sync))?:
	     clovis_sync_op_init(*sop);

	return M0_RC(rc);
}
M0_EXPORTED(m0_clovis_sync_op_init);

int m0_clovis_sync_entity_add(struct m0_clovis_op *sop,
			      struct m0_clovis_entity *ent)
{
	int                         rc;
	struct m0_clovis_op_common *oc;
	struct m0_clovis_op_sync   *os;
	struct clovis_sync_request *sreq;

	M0_ENTRY();

	/*
	 * New elements can only be added to the SYNC op
	 * before the op is launched.
	 */
	M0_PRE(sop != NULL);
	M0_PRE(sop->op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);

	oc = bob_of(sop, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	os = bob_of(oc, struct m0_clovis_op_sync, os_oc, &os_bobtype);

	/* Stores the target. */
	sreq = os->os_req;
	M0_ASSERT(sreq != NULL);
	rc = clovis_sync_request_target_add(sreq, CLOVIS_SYNC_ENTITY, ent);
	if (rc != 0)
		return M0_ERR(rc);

	/* Adds service txid (if stx exists in the list, then merge.).*/
	rc = clovis_sync_request_stx_add(sreq, &ent->en_pending_tx,
					 &ent->en_pending_tx_lock);

	return M0_RC(rc);
}
M0_EXPORTED(m0_clovis_sync_entity_add);

int m0_clovis_sync_op_add(struct m0_clovis_op *sop,
			  struct m0_clovis_op *op)
{
	int                         rc;
	struct m0_clovis_op_common *oc;
	struct m0_clovis_op_sync   *os;
	struct clovis_sync_request *sreq;

	M0_ENTRY();

	/*
	 * New elements can only be added to the SYNC op
	 * before the op is launched.
	 */
	M0_PRE(sop != NULL);
	M0_PRE(sop->op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	M0_PRE(op != NULL);
	M0_PRE(M0_IN(op->op_sm.sm_state,
		     (M0_CLOVIS_OS_STABLE, M0_CLOVIS_OS_EXECUTED)));

	oc = bob_of(sop, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	os = bob_of(oc, struct m0_clovis_op_sync, os_oc, &os_bobtype);

	/* Stores the target. */
	sreq = os->os_req;
	M0_ASSERT(sreq != NULL);
	rc = clovis_sync_request_target_add(sreq, CLOVIS_SYNC_OP, op);
	if (rc != 0)
		return M0_ERR(rc);

	/* Adds service txid (if stx exists in the list, then merge.).*/
	rc = clovis_sync_request_stx_add(sreq, &op->op_pending_tx,
					 &op->op_pending_tx_lock);

	return M0_RC(rc);
}
M0_EXPORTED(m0_clovis_sync_op_add);

/**
 * Entry point for sync, calls clovis_sync_core with mode=active
 */
int m0_clovis_entity_sync(struct m0_clovis_entity *ent)
{
	int                          rc;
	struct clovis_sync_request   sreq;
	struct clovis_sync_target   *tgt;
	struct m0_reqh_service_txid *stx;

	M0_ENTRY();
	M0_PRE(ent != NULL);

	clovis_sync_request_init(&sreq);
	rc = clovis_sync_request_target_add(&sreq, CLOVIS_SYNC_ENTITY, ent);
	if (rc != 0)
		return M0_ERR(rc);

	rc = clovis_sync_request_stx_add(
		&sreq, &ent->en_pending_tx, &ent->en_pending_tx_lock)?:
	     clovis_sync_request_launch_and_wait(&sreq, M0_FSYNC_MODE_ACTIVE);
	m0_tl_teardown(spti, &sreq.sr_stxs, stx)
		m0_free(stx);
	m0_tl_teardown(clovis_sync_target, &sreq.sr_targets, tgt)
		m0_free(tgt);

	return M0_RC(rc);
}
M0_EXPORTED(m0_clovis_entity_sync);

/**
 * Entry point for syncing the all pending tx in the Clovis instance.
 * Unlike clovis_sync_core this function acquires the sc_max_pending_tx_lock
 * for each service, as there is not a larger-granularity lock.
 */
int m0_clovis_sync(struct m0_clovis *m0c, bool wait)
{
	int                             rc;
	int                             saved_error = 0;
	struct m0_reqh_service_txid    *stx;
	struct m0_reqh_service_ctx     *iter;
	struct clovis_sync_request      sreq;
	struct clovis_sync_fop_wrapper *sfw;

	M0_ENTRY();

	M0_PRE(si.si_post_rpc != NULL);
	M0_PRE(si.si_wait_for_reply != NULL);
	M0_PRE(si.si_fop_fini != NULL);

	clovis_sync_request_init(&sreq);

	/*
	 *  loop over all services associated with this super block,
	 *  send an fsync fop for those with pending transactions
	 *
	 *  fop sending loop
	 */
	m0_tl_for(pools_common_svc_ctx, &m0c->m0c_pools_common.pc_svc_ctxs,
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
		rc = clovis_sync_request_fop_send(&sreq, stx,
						  M0_FSYNC_MODE_ACTIVE,
						  true, &sfw);
		if (rc != 0) {
			saved_error = rc;
			m0_mutex_unlock(&iter->sc_max_pending_tx_lock);
			break;
		} else {
			/* Reset the rpc item ops to NULL. */
			sfw->sfw_fop.f_item.ri_ops = NULL;
			/* Add to list of pending fops */
			spf_tlink_init_at(sfw, &sreq.sr_fops);
		}

		m0_mutex_unlock(&iter->sc_max_pending_tx_lock);
	} m0_tl_endfor;

	/*
	 * At this point we may have sent some fops, but stopped when one
	 * failed - collect all the replies before returning
	 */

	/* reply receiving loop */
	m0_tl_teardown(spf, &sreq.sr_fops, sfw) {
		/* get and process the reply */
		rc = clovis_sync_reply_wait(sfw);
		saved_error = saved_error ? : rc;
	}

	M0_LEAVE();
	return (saved_error == 0) ? M0_RC(saved_error): M0_ERR(saved_error);
}
M0_EXPORTED(m0_clovis_sync);

M0_INTERNAL struct m0_clovis_entity *
m0_clovis__op_sync_entity(const struct m0_clovis_op *op)
{
	struct m0_clovis_op_sync   *os;
	struct clovis_sync_target  *stgt;
	struct m0_clovis_op_common *oc;

	M0_PRE(op != NULL);
	M0_PRE(op->op_code == M0_CLOVIS_EO_SYNC);

	oc = bob_of(op, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	M0_PRE(oc != NULL);
	os = M0_AMB(os, oc, os_oc);
	M0_PRE(os != NULL && os->os_req != NULL);

	stgt = clovis_sync_target_tlist_head(&os->os_req->sr_targets);
	M0_PRE(stgt != NULL);
	switch (stgt->srt_type) {
	case CLOVIS_SYNC_ENTITY:
		return stgt->u.srt_ent;
	case CLOVIS_SYNC_OP:
		return stgt->u.srt_op->op_entity;
	case CLOVIS_SYNC_INSTANCE:
		break;
	default:
		M0_IMPOSSIBLE("Unknow type for SYNC request.");
	}

	return NULL;
}

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
