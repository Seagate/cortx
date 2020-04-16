/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 15-Feb-2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RM
#include "lib/trace.h"
#include "lib/string.h"
#include "rm/rm.h"
#include "rm/rm_rwlock.h"
#include "rpc/rpclib.h"

struct wlock_ctx {
	struct m0_rpc_machine     *wlc_rmach;     /**< rpc machine            */
	struct m0_rpc_conn         wlc_conn;      /**< rpc connection         */
	struct m0_rpc_session      wlc_sess;      /**< rpc session            */
	char                      *wlc_rm_addr;   /**< HA-reported RM address */
	struct m0_fid              wlc_rm_fid;    /**< HA-reported RM fid     */
	struct m0_rw_lockable      wlc_rwlock;    /**< lockable resource      */
	struct m0_rm_owner         wlc_owner;     /**< local owner-borrower   */
	struct m0_fid              wlc_owner_fid; /**< owner fid              */
	struct m0_rm_remote        wlc_creditor;  /**< remote creditor        */
	struct m0_rm_incoming      wlc_req;       /**< request to wait on     */
	/** semaphore to wait until request is completed */
	struct m0_semaphore        wlc_sem;
	/**
	 * Write resource domain. Needs to be separate from global read domain
	 * used by @ref rconfc instances. (see m0_rwlockable_read_domain())
	 */
	struct m0_rm_domain        wlc_dom;
	/**
	 * Write resource type. Needs to be registered with the write resource
	 * domain.
	 */
	struct m0_rm_resource_type wlc_rt;
	/** result code of write lock request */
	int32_t                    wlc_rc;
} wlx;

static void write_lock_complete(struct m0_rm_incoming *in,
				int32_t                rc)
{
	M0_ENTRY("incoming %p, rc %d", in, rc);
	wlx.wlc_rc = rc;
	m0_semaphore_up(&wlx.wlc_sem);
	M0_LEAVE();
}

static void write_lock_conflict(struct m0_rm_incoming *in)
{
	/* Do nothing */
}

static struct m0_rm_incoming_ops ri_ops = {
	.rio_complete = write_lock_complete,
	.rio_conflict = write_lock_conflict,
};

static int wlock_ctx_create(struct m0_rpc_machine *rpc_mach, const char *rm_ep)
{
	int rc;

	wlx.wlc_rmach = rpc_mach;
	m0_rwlockable_domain_type_init(&wlx.wlc_dom, &wlx.wlc_rt);
	m0_rw_lockable_init(&wlx.wlc_rwlock, &M0_RWLOCK_FID, &wlx.wlc_dom);
	m0_fid_tgenerate(&wlx.wlc_owner_fid, M0_RM_OWNER_FT);
	m0_rm_rwlock_owner_init(&wlx.wlc_owner, &wlx.wlc_owner_fid,
				&wlx.wlc_rwlock, NULL);
	wlx.wlc_rm_addr = m0_strdup(rm_ep);
	rc = m0_semaphore_init(&wlx.wlc_sem, 0);
	return M0_RC(rc);
}

static int wlock_ctx_connect(struct wlock_ctx *wlx)
{
	enum { MAX_RPCS_IN_FLIGHT = 15 };

	M0_PRE(wlx != NULL);
	return m0_rpc_client_connect(&wlx->wlc_conn, &wlx->wlc_sess,
				     wlx->wlc_rmach, wlx->wlc_rm_addr, NULL,
				     MAX_RPCS_IN_FLIGHT, M0_TIME_NEVER);
}

static void wlock_ctx_creditor_setup(struct wlock_ctx *wlx)
{
	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	m0_rm_remote_init(&wlx->wlc_creditor, wlx->wlc_owner.ro_resource);
	wlx->wlc_creditor.rem_session = &wlx->wlc_sess;
	m0_rm_owner_creditor_reset(&wlx->wlc_owner, &wlx->wlc_creditor);
	M0_LEAVE();
}

static void _write_lock_get(struct wlock_ctx *wlx)
{
	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	m0_rm_rwlock_req_init(&wlx->wlc_req, &wlx->wlc_owner, &ri_ops,
			      RIF_MAY_BORROW | RIF_MAY_REVOKE | RIF_LOCAL_WAIT |
			      RIF_RESERVE, RM_RWLOCK_WRITE);
	m0_rm_credit_get(&wlx->wlc_req);
	M0_LEAVE();
}

static void wlock_ctx_destroy(struct wlock_ctx *wlx)
{
	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	M0_LOG(M0_DEBUG, "owner wound up");
	m0_rm_rwlock_owner_fini(&wlx->wlc_owner);
	m0_rw_lockable_fini(&wlx->wlc_rwlock);
	m0_rwlockable_domain_type_fini(&wlx->wlc_dom, &wlx->wlc_rt);
	M0_LEAVE();
}

static void wlock_ctx_disconnect(struct wlock_ctx *wlx)
{
	int rc;

	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(_0C(wlx != NULL) && _0C(!M0_IS0(&wlx->wlc_sess)));
	rc = m0_rpc_session_destroy(&wlx->wlc_sess, M0_TIME_NEVER);
	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to destroy wlock session");
	rc = m0_rpc_conn_destroy(&wlx->wlc_conn, M0_TIME_NEVER);
	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to destroy wlock connection");
	M0_LEAVE();

}

static void wlock_ctx_owner_windup(struct wlock_ctx *wlx)
{
	int rc;

	M0_ENTRY("wlx=%p", wlx);
	m0_rm_owner_windup(&wlx->wlc_owner);
	rc = m0_rm_owner_timedwait(&wlx->wlc_owner,
				   M0_BITS(ROS_FINAL, ROS_INSOLVENT),
				   M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	M0_LEAVE();
}

static void wlock_ctx_creditor_unset(struct wlock_ctx *wlx)
{
	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	m0_rm_remote_fini(&wlx->wlc_creditor);
	M0_SET0(&wlx->wlc_creditor);
	wlx->wlc_owner.ro_creditor = NULL;
	M0_LEAVE();
}

static void _write_lock_put(struct wlock_ctx *wlx)
{
	struct m0_rm_incoming *req;

	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	req = &wlx->wlc_req;
	m0_rm_credit_put(req);
	m0_rm_incoming_fini(req);
	wlock_ctx_owner_windup(wlx);
	wlock_ctx_creditor_unset(wlx);
	M0_LEAVE();
}

M0_INTERNAL void rm_write_lock_put()
{
	_write_lock_put(&wlx);
	wlock_ctx_destroy(&wlx);
	wlock_ctx_disconnect(&wlx);
	m0_free0(&wlx.wlc_rm_addr);
	M0_LEAVE();
}

M0_INTERNAL int rm_write_lock_get(struct m0_rpc_machine *rpc_mach,
				  const char *rm_ep)
{
	int rc;

	rc = wlock_ctx_create(rpc_mach, rm_ep);
	if (rc != 0) {
		M0_ERR(rc);
		goto fail;
	}
	rc = wlock_ctx_connect(&wlx);
	if (rc != 0) {
		M0_ERR_INFO(rc, "ep=%s", rm_ep);
		goto ctx_free;
	}
	wlock_ctx_creditor_setup(&wlx);
	_write_lock_get(&wlx);
	m0_semaphore_down(&wlx.wlc_sem);
	rc = wlx.wlc_rc;
	if (rc != 0) {
		M0_ERR(rc);
		goto ctx_destroy;
	}
	return M0_RC(rc);
ctx_destroy:
	wlock_ctx_owner_windup(&wlx);
	wlock_ctx_creditor_unset(&wlx);
	wlock_ctx_destroy(&wlx);
	wlock_ctx_disconnect(&wlx);
ctx_free:
	m0_free0(&wlx.wlc_rm_addr);
fail:
	return M0_ERR(rc);
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
