/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 11-Feb-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SPIEL
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/finject.h"       /* M0_FI_ENABLED */
#include "lib/memory.h"        /* m0_alloc, M0_ALLOC_ARR */
#include "lib/string.h"        /* m0_strdup, m0_strings_dup */
#include "lib/tlist.h"
#include "fid/fid.h"
#include "conf/cache.h"
#include "conf/flip_fop.h"
#include "conf/load_fop.h"
#include "conf/obj.h"
#include "conf/onwire_xc.h"    /* m0_confx_xc */
#include "conf/preload.h"      /* m0_confx_free, m0_confx_to_string */
#include "conf/obj_ops.h"      /* m0_conf_obj_find */
#include "conf/dir.h"          /* m0_conf_dir_new */
#include "conf/helpers.h"      /* m0_conf_pvers */
#include "rm/rm_rwlock.h"      /* m0_rw_lockable */
#include "rpc/link.h"
#include "rpc/rpclib.h"        /* m0_rpc_client_connect */
#include "ioservice/fid_convert.h" /* M0_FID_DEVICE_ID_MAX */
#include "spiel/spiel.h"
#include "spiel/conf_mgmt.h"
#include "spiel/spiel_internal.h"
#ifndef __KERNEL__
#  include <stdio.h>           /* FILE, fopen */
#endif

/**
 * @addtogroup spiel-api-fspec-intr
 * @{
 */

M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);

struct spiel_conf_param {
	const struct m0_fid            *scp_fid;
	const struct m0_conf_obj_type  *scp_type;
	struct m0_conf_obj            **scp_obj;
};

static int spiel_rwlockable_write_domain_init(struct m0_spiel_wlock_ctx *wlx)
{
	return m0_rwlockable_domain_type_init(&wlx->wlc_dom, &wlx->wlc_rt);
}

static void spiel_rwlockable_write_domain_fini(struct m0_spiel_wlock_ctx *wlx)
{
	m0_rwlockable_domain_type_fini(&wlx->wlc_dom, &wlx->wlc_rt);
}

#define SPIEL_CONF_CHECK(cache, ...)                                     \
	spiel_conf_parameter_check(cache, (struct spiel_conf_param []) { \
						__VA_ARGS__,             \
						{ NULL, NULL, NULL }, });

/**
 * Overrides configuration version number originally set during
 * m0_spiel_tx_open() and stored in root object.
 */
static int spiel_root_ver_update(struct m0_spiel_tx *tx, uint64_t verno)
{
	int                  rc;
	struct m0_conf_obj  *obj;
	struct m0_conf_root *root;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = m0_conf_obj_find(&tx->spt_cache, &M0_CONF_ROOT_FID, &obj);
	if (obj == NULL || obj->co_status == M0_CS_MISSING)
		rc = M0_ERR(-ENOENT);
	if (rc == 0) {
		root = M0_CONF_CAST(obj, m0_conf_root);
		root->rt_verno = verno;
	}
	m0_mutex_unlock(&tx->spt_lock);

	return M0_RC(rc);
}

void m0_spiel_tx_open(struct m0_spiel *spiel, struct m0_spiel_tx *tx)
{
	struct m0_mutex    *lock = &tx->spt_lock;
	struct m0_conf_obj *obj;
	int                 rc;

	M0_ENTRY("tx=%p", tx);
	M0_PRE(tx != NULL);

	tx->spt_spiel = spiel;
	tx->spt_buffer = NULL;

	m0_mutex_init(lock);
	m0_conf_cache_init(&tx->spt_cache, lock);

	/* Create root object. */
	m0_mutex_lock(lock);
	rc = m0_conf_obj_find(&tx->spt_cache, &M0_CONF_ROOT_FID, &obj);
	m0_mutex_unlock(lock);
	M0_ASSERT(rc == 0); /* XXX Error handling is for cowards. */

	M0_POST(obj->co_status == M0_CS_MISSING);
	M0_LEAVE();
}
M0_EXPORTED(m0_spiel_tx_open);

/**
 *  Check cache for completeness:
 *  each element has state M0_CS_READY and
 *  has real parent (if last need by obj type)
 */
int m0_spiel_tx_validate(struct m0_spiel_tx *tx)
{
	struct m0_conf_obj   *obj;
	struct m0_conf_obj   *obj_parent;
	struct m0_conf_cache *cache = &tx->spt_cache;

	M0_ENTRY();

	m0_tl_for(m0_conf_cache, &cache->ca_registry, obj) {
		if (m0_conf_obj_type(obj) != &M0_CONF_DIR_TYPE) {
			/* Check status*/
			if (obj->co_status != M0_CS_READY)
				return M0_ERR(-EBUSY);

			if (m0_conf_obj_type(obj) != &M0_CONF_ROOT_TYPE) {
				/* Check parent */
				if (obj->co_parent == NULL)
					return M0_ERR(-ENOENT);
				/* Check parent in cache */
				obj_parent = m0_conf_cache_lookup(cache,
							&obj->co_parent->co_id);
				if (obj_parent == NULL ||
				    obj_parent != obj->co_parent)
					return M0_ERR(-ENOENT);
			}
		}
	} m0_tl_endfor;

	return M0_RC(0);
}
M0_EXPORTED(m0_spiel_tx_validate);

/**
 * Frees Spiel context without completing the transaction.
 */
void m0_spiel_tx_close(struct m0_spiel_tx *tx)
{
	M0_ENTRY("tx=%p", tx);
	m0_conf_cache_fini(&tx->spt_cache);
	m0_mutex_fini(&tx->spt_lock);
	M0_LEAVE();
}
M0_EXPORTED(m0_spiel_tx_close);

static bool spiel_load_cmd_invariant(struct m0_spiel_load_command *cmd)
{
	return _0C(cmd != NULL) &&
	       _0C(cmd->slc_load_fop.f_type == &m0_fop_conf_load_fopt);
}

static uint64_t spiel_root_conf_version(struct m0_spiel_tx *tx)
{
	int                 rc;
	uint64_t            verno;
	struct m0_conf_obj *obj;

	M0_ENTRY("tx=%p", tx);

	m0_mutex_lock(&tx->spt_lock);
	rc = m0_conf_obj_find(&tx->spt_cache, &M0_CONF_ROOT_FID, &obj);
	M0_ASSERT(_0C(rc == 0) && _0C(obj != NULL));
	verno = M0_CONF_CAST(obj, m0_conf_root)->rt_verno;
	m0_mutex_unlock(&tx->spt_lock);

	return verno;
}
/**
 * Finalizes a FOP for Spiel Load command.
 * @pre spiel_load_cmd_invariant(spiel_cmd)
 */
static void spiel_load_fop_fini(struct m0_spiel_load_command *spiel_cmd)
{
	M0_PRE(spiel_load_cmd_invariant(spiel_cmd));
	m0_rpc_bulk_buflist_empty(&spiel_cmd->slc_rbulk);
	m0_rpc_bulk_fini(&spiel_cmd->slc_rbulk);
	m0_fop_fini(&spiel_cmd->slc_load_fop);
}

/**
 * Release a FOP for Spiel Load command.
 * @pre spiel_load_cmd_invariant(spiel_cmd)
 */
static void spiel_load_fop_release(struct m0_ref *ref)
{
	struct m0_fop                *fop = M0_AMB(fop, ref, f_ref);
	struct m0_spiel_load_command *spiel_cmd = M0_AMB(spiel_cmd, fop,
							 slc_load_fop);
	spiel_load_fop_fini(spiel_cmd);
}

/**
 * Initializes a FOP for Spiel Load command.
 * @pre spiel_cmd != NULL.
 * @pre tx != NULL.
 * @post spiel_load_cmd_invariant(spiel_cmd)
 */
static int spiel_load_fop_init(struct m0_spiel_load_command *spiel_cmd,
			       struct m0_spiel_tx           *tx)
{
	int                      rc;
	struct m0_fop_conf_load *conf_fop;

	M0_ENTRY();
	M0_PRE(spiel_cmd != NULL);
	M0_PRE(tx != NULL);

	m0_fop_init(&spiel_cmd->slc_load_fop, &m0_fop_conf_load_fopt, NULL,
		    spiel_load_fop_release);
	rc = m0_fop_data_alloc(&spiel_cmd->slc_load_fop);
	if (rc == 0) {
		/* Fill Spiel Conf FOP specific data*/
		conf_fop = m0_conf_fop_to_load_fop(&spiel_cmd->slc_load_fop);
		conf_fop->clf_version = spiel_root_conf_version(tx);
		conf_fop->clf_tx_id = (uint64_t)tx;
		m0_rpc_bulk_init(&spiel_cmd->slc_rbulk);
		M0_POST(spiel_load_cmd_invariant(spiel_cmd));
	}
	return M0_RC(rc);
}

static int spiel_load_fop_create(struct m0_spiel_tx           *tx,
				 struct m0_spiel_load_command *spiel_cmd)
{
	int                     rc;
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_net_domain   *nd;
	int                     i;
	int                     segs_nr;
	m0_bcount_t             seg_size;
	m0_bcount_t             size = strlen(tx->spt_buffer)+1;

	rc = spiel_load_fop_init(spiel_cmd, tx);
	if (rc != 0)
		return M0_ERR(rc);

	/* Fill RPC Bulk part of Spiel FOM */
	nd = spiel_rmachine(tx->spt_spiel)->rm_tm.ntm_dom;
	seg_size = m0_net_domain_get_max_buffer_segment_size(nd);
	/*
	 * Calculate number of segments for given data size.
	 * Segments number is rounded up.
	 */
	segs_nr = (size + seg_size - 1) / seg_size;
	rc = m0_rpc_bulk_buf_add(&spiel_cmd->slc_rbulk, segs_nr, size, nd,
				 NULL, &rbuf);
	for (i = 0; i < segs_nr; ++i) {
		m0_rpc_bulk_buf_databuf_add(rbuf,
				    tx->spt_buffer + i*seg_size,
				    min_check(size, seg_size), i, nd);
		size -= seg_size;
	}
	rbuf->bb_nbuf->nb_qtype = M0_NET_QT_PASSIVE_BULK_SEND;
	return rc;
}

static int spiel_load_fop_send(struct m0_spiel_tx           *tx,
			       struct m0_spiel_load_command *spiel_cmd)
{
	int                          rc;
	struct m0_fop_conf_load     *load_fop;
	struct m0_fop               *rep;
	struct m0_fop_conf_load_rep *load_rep;

	M0_ENTRY();
	rc = spiel_load_fop_create(tx, spiel_cmd);
	if (rc != 0)
		return M0_ERR(rc);

	load_fop = m0_conf_fop_to_load_fop(&spiel_cmd->slc_load_fop);
	rc = m0_rpc_bulk_store(&spiel_cmd->slc_rbulk,
			       &spiel_cmd->slc_connect,
			       &load_fop->clf_desc,
			       &m0_rpc__buf_bulk_cb);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_rpc_post_sync(&spiel_cmd->slc_load_fop,
			      &spiel_cmd->slc_session,
			      NULL,
			      0/*M0_TIME_NEVER*/);
	rep = rc == 0 ? m0_rpc_item_to_fop(
				spiel_cmd->slc_load_fop.f_item.ri_reply)
			: NULL;
	load_rep = rep != NULL ? m0_conf_fop_to_load_fop_rep(rep) : NULL;

	if (load_rep != NULL) {
		rc = load_rep->clfr_rc;
		spiel_cmd->slc_version = load_rep->clfr_version;
	} else {
		rc = M0_ERR(-ENOENT);
	}
	m0_fop_put_lock(&spiel_cmd->slc_load_fop);
	return M0_RC(rc);
}

static void spiel_flip_fop_release(struct m0_ref *ref)
{
	m0_fop_fini(container_of(ref, struct m0_fop, f_ref));
}

static int spiel_flip_fop_send(struct m0_spiel_tx           *tx,
			       struct m0_spiel_load_command *spiel_cmd)
{
	int                          rc;
	struct m0_fop_conf_flip     *flip_fop;
	struct m0_fop               *rep;
	struct m0_fop_conf_flip_rep *flip_rep;

	M0_ENTRY();
	M0_PRE(spiel_cmd != NULL);

	m0_fop_init(&spiel_cmd->slc_flip_fop, &m0_fop_conf_flip_fopt, NULL,
		    spiel_flip_fop_release);
	rc = m0_fop_data_alloc(&spiel_cmd->slc_flip_fop);
	if (rc == 0) {
		flip_fop = m0_conf_fop_to_flip_fop(&spiel_cmd->slc_flip_fop);
		flip_fop->cff_prev_version = spiel_cmd->slc_version;
		flip_fop->cff_next_version = spiel_root_conf_version(tx);
		flip_fop->cff_tx_id = (uint64_t)tx;
		rc = m0_rpc_post_sync(&spiel_cmd->slc_flip_fop,
				      &spiel_cmd->slc_session, NULL, 0);
	}
	rep = rc == 0 ? m0_rpc_item_to_fop(
				spiel_cmd->slc_flip_fop.f_item.ri_reply)
			: NULL;
	flip_rep = rep != NULL ? m0_conf_fop_to_flip_fop_rep(rep) : NULL;
	rc = flip_rep != NULL ? flip_rep->cffr_rc : M0_ERR(-ENOENT);
	m0_fop_put_lock(&spiel_cmd->slc_flip_fop);
	return M0_RC(rc);
}

static int wlock_ctx_semaphore_init(struct m0_spiel_wlock_ctx *wlx)
{
	return m0_semaphore_init(&wlx->wlc_sem, 0);
}

static void wlock_ctx_semaphore_up(struct m0_spiel_wlock_ctx *wlx)
{
	m0_semaphore_up(&wlx->wlc_sem);
}

static int wlock_ctx_create(struct m0_spiel *spl)
{
	struct m0_spiel_wlock_ctx *wlx;
	int                        rc;

	M0_ENTRY();
	M0_PRE(spl->spl_wlock_ctx == NULL);

	M0_ALLOC_PTR(wlx);
	if (wlx == NULL)
		return M0_ERR(-ENOMEM);

	wlx->wlc_rmach = spiel_rmachine(spl);
	spiel_rwlockable_write_domain_init(wlx);
	m0_rw_lockable_init(&wlx->wlc_rwlock, &M0_RWLOCK_FID, &wlx->wlc_dom);
	m0_fid_tgenerate(&wlx->wlc_owner_fid, M0_RM_OWNER_FT);
	m0_rm_rwlock_owner_init(&wlx->wlc_owner, &wlx->wlc_owner_fid,
				&wlx->wlc_rwlock, NULL);
	rc = m0_rconfc_rm_endpoint(&spl->spl_rconfc, &wlx->wlc_rm_addr) ?:
		wlock_ctx_semaphore_init(wlx);
	if (rc != 0) {
		m0_free(wlx);
		return M0_ERR(rc);
	}
	spl->spl_wlock_ctx = wlx;
	return M0_RC(0);
}

static void wlock_ctx_destroy(struct m0_spiel_wlock_ctx *wlx)
{
	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);

	m0_rm_rwlock_owner_fini(&wlx->wlc_owner);
	m0_rw_lockable_fini(&wlx->wlc_rwlock);
	spiel_rwlockable_write_domain_fini(wlx);
	M0_LEAVE();
}

static int wlock_ctx_connect(struct m0_spiel_wlock_ctx *wlx)
{
	enum { MAX_RPCS_IN_FLIGHT = 15 };

	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	return M0_RC(m0_rpc_client_connect(&wlx->wlc_conn, &wlx->wlc_sess,
					   wlx->wlc_rmach, wlx->wlc_rm_addr,
					   NULL, MAX_RPCS_IN_FLIGHT,
					   M0_TIME_NEVER));
}

static void wlock_ctx_disconnect(struct m0_spiel_wlock_ctx *wlx)
{
	int rc;

	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(_0C(wlx != NULL) && _0C(!M0_IS0(&wlx->wlc_sess)));
	rc = m0_rpc_session_destroy(&wlx->wlc_sess, M0_TIME_NEVER);
	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to destroy wlock session; rc=%d", rc);
	rc = m0_rpc_conn_destroy(&wlx->wlc_conn, M0_TIME_NEVER);
	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to destroy wlock connection; rc=%d",
		       rc);
	M0_LEAVE();
}

static void spiel_tx_write_lock_complete(struct m0_rm_incoming *in,
					 int32_t                rc)
{
	struct m0_spiel_wlock_ctx *wlx = M0_AMB(wlx, in, wlc_req);

	M0_ENTRY("in=%p rc=%d", in, rc);
	wlx->wlc_rc = rc;
	wlock_ctx_semaphore_up(wlx);
	M0_LEAVE();
}

static void spiel_tx_write_lock_conflict(struct m0_rm_incoming *in)
{
	/* Do nothing */
}

static struct m0_rm_incoming_ops spiel_tx_ri_ops = {
	.rio_complete = spiel_tx_write_lock_complete,
	.rio_conflict = spiel_tx_write_lock_conflict,
};

static void wlock_ctx_creditor_setup(struct m0_spiel_wlock_ctx *wlx)
{
	struct m0_rm_owner  *owner;
	struct m0_rm_remote *creditor;

	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	owner = &wlx->wlc_owner;
	creditor = &wlx->wlc_creditor;
	m0_rm_remote_init(creditor, owner->ro_resource);
	creditor->rem_session = &wlx->wlc_sess;
	m0_rm_owner_creditor_reset(owner, creditor);
	M0_LEAVE();
}

static void _spiel_tx_write_lock_get(struct m0_spiel_wlock_ctx *wlx)
{
	struct m0_rm_incoming *req;

	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	req = &wlx->wlc_req;
	m0_rm_rwlock_req_init(req, &wlx->wlc_owner, &spiel_tx_ri_ops,
			      RIF_MAY_BORROW | RIF_MAY_REVOKE | RIF_LOCAL_WAIT |
			      RIF_RESERVE, RM_RWLOCK_WRITE);
	m0_rm_credit_get(req);
	M0_LEAVE();
}

static void wlock_ctx_creditor_unset(struct m0_spiel_wlock_ctx *wlx)
{
	M0_ENTRY("wlx=%p", wlx);
	M0_PRE(wlx != NULL);
	m0_rm_remote_fini(&wlx->wlc_creditor);
	M0_SET0(&wlx->wlc_creditor);
	wlx->wlc_owner.ro_creditor = NULL;
	M0_LEAVE();
}

static void wlock_ctx_semaphore_down(struct m0_spiel_wlock_ctx *wlx)
{
	M0_ENTRY("wlx=%p", wlx);
	m0_semaphore_down(&wlx->wlc_sem);
	M0_LEAVE();
}

static void wlock_ctx_owner_windup(struct m0_spiel_wlock_ctx *wlx)
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

static int spiel_tx_write_lock_get(struct m0_spiel_tx *tx)
{
	struct m0_spiel           *spl;
	struct m0_spiel_wlock_ctx *wlx;
	int                        rc;

	M0_ENTRY("tx=%p", tx);
	M0_PRE(tx != NULL);

	spl = tx->spt_spiel;
	rc = wlock_ctx_create(spl);
	if (rc != 0)
		return M0_ERR(rc);
	wlx = spl->spl_wlock_ctx;
	rc = wlock_ctx_connect(wlx);
	if (rc != 0)
		goto err;
	wlock_ctx_creditor_setup(wlx);
	if (M0_FI_ENABLED("borrow-request-failure")) {
		wlx->wlc_rc = -EIO;  /* simulate MERO-2364 */
		goto wlock_err;
	}
	_spiel_tx_write_lock_get(wlx);
	wlock_ctx_semaphore_down(wlx);
	if (wlx->wlc_rc == 0)
		return M0_RC(0);
wlock_err:
	rc = wlx->wlc_rc;
	wlock_ctx_owner_windup(wlx);
	wlock_ctx_creditor_unset(wlx);
	wlock_ctx_destroy(wlx);
	wlock_ctx_disconnect(wlx);
err:
	m0_free(wlx->wlc_rm_addr);
	m0_free0(&spl->spl_wlock_ctx);
	return M0_ERR(rc);
}

static void _spiel_tx_write_lock_put(struct m0_spiel_wlock_ctx *wlx)
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

static void spiel_tx_write_lock_put(struct m0_spiel_tx *tx)
{
	struct m0_spiel_wlock_ctx *wlx;

	M0_ENTRY("tx=%p", tx);
	M0_PRE(_0C(tx != NULL) && _0C(tx->spt_spiel != NULL));
	wlx = tx->spt_spiel->spl_wlock_ctx;
	_spiel_tx_write_lock_put(wlx);
	wlock_ctx_destroy(wlx);
	wlock_ctx_disconnect(wlx);
	m0_free0(&wlx->wlc_rm_addr);
	m0_free0(&tx->spt_spiel->spl_wlock_ctx);
	M0_LEAVE();
}

int m0_spiel_tx_commit_forced(struct m0_spiel_tx *tx,
			      bool                forced,
			      uint64_t            ver_forced,
			      uint32_t           *rquorum)
{
	enum { MAX_RPCS_IN_FLIGHT = 2 };
	int                            rc;
	struct m0_spiel_load_command  *spiel_cmd   = NULL;
	const char                   **confd_eps   = NULL;
	uint32_t                       confd_count = 0;
	uint32_t                       quorum      = 0;
	uint32_t                       idx;
	uint64_t                       rconfc_ver;

	M0_ENTRY();
	rc = m0_spiel_rconfc_start(tx->spt_spiel, NULL);
	if (rc != 0)
		goto rconfc_fail;
	/*
	 * in case ver_forced value is other than M0_CONF_VER_UNKNOWN, override
	 * transaction version number with ver_forced, otherwise leave the one
	 * intact
	 */
	if (ver_forced != M0_CONF_VER_UNKNOWN) {
		rc = spiel_root_ver_update(tx, ver_forced);
		M0_ASSERT(rc == 0);
	} else {
		rconfc_ver = m0_rconfc_ver_max_read(&tx->spt_spiel->spl_rconfc);
		if (rconfc_ver != M0_CONF_VER_UNKNOWN) {
			++rconfc_ver;
		} else {
			/*
			 * Version number may be unknown due to cluster quorum
			 * issues at runtime, so need to return error code to
			 * client.
			 */
			rc = -ENODATA;
			goto tx_fini;
		}
		rc = spiel_root_ver_update(tx, rconfc_ver);
		M0_ASSERT(rc == 0);
	}

	rc = m0_spiel_tx_validate(tx) ?:
	     m0_conf_cache_to_string(&tx->spt_cache, &tx->spt_buffer, false);
	if (M0_FI_ENABLED("encode_fail"))
		rc = -ENOMEM;
	if (rc != 0)
		goto tx_fini;

	confd_count = m0_rconfc_confd_endpoints(&tx->spt_spiel->spl_rconfc,
						&confd_eps);
	if  (confd_count == 0) {
		rc = M0_ERR(-ENODATA);
		goto tx_fini;
	}
	if (!M0_FI_ENABLED("cmd_alloc_fail"))
		M0_ALLOC_ARR(spiel_cmd, confd_count);
	if (spiel_cmd == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto tx_fini;
	}
	for (idx = 0; idx < confd_count; ++idx) {
		spiel_cmd[idx].slc_status =
			m0_rpc_client_connect(&spiel_cmd[idx].slc_connect,
					      &spiel_cmd[idx].slc_session,
					      spiel_rmachine(tx->spt_spiel),
					      confd_eps[idx], NULL,
					      MAX_RPCS_IN_FLIGHT,
					      M0_TIME_NEVER) ?:
			spiel_load_fop_send(tx, &spiel_cmd[idx]);
	}

	quorum = m0_count(idx, confd_count, (spiel_cmd[idx].slc_status == 0));
	/*
	 * Unless forced transaction committing requested, make sure the quorum
	 * at least reached the value specified at m0_spiel_start_quorum(), or
	 * better went beyond it.
	 */
	if (!forced && quorum < tx->spt_spiel->spl_rconfc.rc_quorum) {
		rc = M0_ERR(-ENOENT);
		goto tx_fini;
	}

	quorum = 0;
	rc = spiel_tx_write_lock_get(tx);
	if (rc != 0)
		goto tx_fini;
	for (idx = 0; idx < confd_count; ++idx) {
		if (spiel_cmd[idx].slc_status == 0) {
			rc = spiel_flip_fop_send(tx, &spiel_cmd[idx]);
			if (rc == 0)
				++quorum;
		}
	}
	/* TODO: handle creditor death */
	spiel_tx_write_lock_put(tx);
	rc = !forced && quorum < tx->spt_spiel->spl_rconfc.rc_quorum ?
		M0_ERR(-ENOENT) : 0;

tx_fini:
	m0_spiel_rconfc_stop(tx->spt_spiel);
rconfc_fail:
	m0_strings_free(confd_eps);
	if (tx->spt_buffer != NULL)
		m0_confx_string_free(tx->spt_buffer);

	if (spiel_cmd != NULL) {
		for (idx = 0; idx < confd_count; ++idx) {
			struct m0_rpc_session *ss = &spiel_cmd[idx].slc_session;
			struct m0_rpc_conn    *sc = &spiel_cmd[idx].slc_connect;

			if (!M0_IN(ss->s_sm.sm_state,
				   (M0_RPC_SESSION_FAILED,
				    M0_RPC_SESSION_INITIALISED)))
				m0_rpc_session_destroy(ss, M0_TIME_NEVER);
			if (!M0_IN(sc->c_sm.sm_state,
				   (M0_RPC_CONN_FAILED,
				    M0_RPC_CONN_INITIALISED)))
			    m0_rpc_conn_destroy(sc, M0_TIME_NEVER);
		}
		m0_free(spiel_cmd);
	}

	/* report resultant quorum value reached during committing */
	if (rquorum != NULL)
		*rquorum = quorum;

	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_tx_commit_forced);

int m0_spiel_tx_commit(struct m0_spiel_tx  *tx)
{
	return m0_spiel_tx_commit_forced(tx, false, M0_CONF_VER_UNKNOWN, NULL);
}
M0_EXPORTED(m0_spiel_tx_commit);

/**
 * Create m0_conf_dir objects for m0_conf_root
 */
static int spiel_root_dirs_create(struct m0_conf_root *r)
{
	struct m0_conf_obj *obj = &r->rt_obj;

	return M0_RC(m0_conf_dir_new(obj, &M0_CONF_ROOT_NODES_FID,
				     &M0_CONF_NODE_TYPE, NULL, &r->rt_nodes) ?:
		     m0_conf_dir_new(obj, &M0_CONF_ROOT_POOLS_FID,
				     &M0_CONF_POOL_TYPE, NULL, &r->rt_pools) ?:
		     m0_conf_dir_new(obj, &M0_CONF_ROOT_SITES_FID,
				     &M0_CONF_SITE_TYPE, NULL, &r->rt_sites) ?:
		     m0_conf_dir_new(obj, &M0_CONF_ROOT_PROFILES_FID,
				     &M0_CONF_PROFILE_TYPE, NULL,
				     &r->rt_profiles) ?:
		     m0_conf_dir_new(obj, &M0_CONF_ROOT_FDMI_FLT_GRPS_FID,
				     &M0_CONF_FDMI_FLT_GRP_TYPE, NULL,
				     &r->rt_fdmi_flt_grps));
}

/** Create m0_conf_dir objects for m0_conf_node. */
static int spiel_node_dirs_create(struct m0_conf_node *node)
{
	return M0_RC(m0_conf_dir_new(&node->cn_obj,
				     &M0_CONF_NODE_PROCESSES_FID,
				     &M0_CONF_PROCESS_TYPE, NULL,
				     &node->cn_processes));
}

/** Create m0_conf_dir objects for m0_conf_process. */
static int spiel_process_dirs_create(struct m0_conf_process *process)
{
	return M0_RC(m0_conf_dir_new(&process->pc_obj,
				     &M0_CONF_PROCESS_SERVICES_FID,
				     &M0_CONF_SERVICE_TYPE, NULL,
				     &process->pc_services));
}

/** Create m0_conf_dir objects for m0_conf_service. */
static int spiel_service_dirs_create(struct m0_conf_service *service)
{
	return M0_RC(m0_conf_dir_new(&service->cs_obj,
				     &M0_CONF_SERVICE_SDEVS_FID,
				     &M0_CONF_SDEV_TYPE, NULL,
				     &service->cs_sdevs));
}

/** Create m0_conf_dir objects for m0_conf_pool. */
static int spiel_pool_dirs_create(struct m0_conf_pool *pool)
{
	return M0_RC(m0_conf_dir_new(&pool->pl_obj,
				     &M0_CONF_POOL_PVERS_FID,
				     &M0_CONF_PVER_TYPE, NULL,
				     &pool->pl_pvers));
}

/**
 * Create m0_conf_dir objects for m0_conf_site
 */
static int spiel_site_dirs_create(struct m0_conf_site  *site)
{
	return M0_RC(m0_conf_dir_new(&site->ct_obj,
				     &M0_CONF_SITE_RACKS_FID,
				     &M0_CONF_RACK_TYPE, NULL,
				     &site->ct_racks));
}

/**
 * Create m0_conf_dir objects for m0_conf_rack
 */
static int spiel_rack_dirs_create(struct m0_conf_rack  *rack)
{
	return M0_RC(m0_conf_dir_new(&rack->cr_obj,
				     &M0_CONF_RACK_ENCLS_FID,
				     &M0_CONF_ENCLOSURE_TYPE, NULL,
				     &rack->cr_encls));
}

/** Create m0_conf_dir objects for m0_conf_enclosure. */
static int spiel_enclosure_dirs_create(struct m0_conf_enclosure *enclosure)
{
	return M0_RC(m0_conf_dir_new(&enclosure->ce_obj,
				     &M0_CONF_ENCLOSURE_CTRLS_FID,
				     &M0_CONF_CONTROLLER_TYPE, NULL,
				     &enclosure->ce_ctrls));
}

/** Create m0_conf_dir objects for m0_conf_controller. */
static int spiel_controller_dirs_create(struct m0_conf_controller *controller)
{
	return M0_RC(m0_conf_dir_new(&controller->cc_obj,
				     &M0_CONF_CONTROLLER_DRIVES_FID,
				     &M0_CONF_DRIVE_TYPE, NULL,
				     &controller->cc_drives));
}

/** Create m0_conf_dir objects for m0_conf_pver. */
static int spiel_pver_dirs_create(struct m0_conf_pver *pver)
{
	return M0_RC(m0_conf_dir_new(&pver->pv_obj,
				     &M0_CONF_PVER_SITEVS_FID,
				     &M0_CONF_OBJV_TYPE, NULL,
				     &pver->pv_u.subtree.pvs_sitevs));
}

/**
 * Create m0_conf_dir objects for site version
 */
static int spiel_sitev_dirs_create(struct m0_conf_objv  *objv)
{
	return M0_RC(m0_conf_dir_new(&objv->cv_obj,
				     &M0_CONF_SITEV_RACKVS_FID,
				     &M0_CONF_OBJV_TYPE, NULL,
				     &objv->cv_children));
}

/**
 * Create m0_conf_dir objects for rack version
 */
static int spiel_rackv_dirs_create(struct m0_conf_objv  *objv)
{
	return M0_RC(m0_conf_dir_new(&objv->cv_obj,
				     &M0_CONF_RACKV_ENCLVS_FID,
				     &M0_CONF_OBJV_TYPE, NULL,
				     &objv->cv_children));
}

/** Create m0_conf_dir objects for enclosure version. */
static int spiel_enclosurev_dirs_create(struct m0_conf_objv *objv)
{
	return M0_RC(m0_conf_dir_new(&objv->cv_obj,
				     &M0_CONF_ENCLV_CTRLVS_FID,
				     &M0_CONF_OBJV_TYPE, NULL,
				     &objv->cv_children));
}

/** Create m0_conf_dir objects for controller version. */
static int spiel_controllerv_dirs_create(struct m0_conf_objv *objv)
{
	return M0_RC(m0_conf_dir_new(&objv->cv_obj,
				     &M0_CONF_CTRLV_DRIVEVS_FID,
				     &M0_CONF_OBJV_TYPE, NULL,
				     &objv->cv_children));
}

static int spiel_conf_parameter_check(struct m0_conf_cache    *cache,
				      struct spiel_conf_param *parameters)
{
	struct spiel_conf_param  *param;
	const struct m0_conf_obj *obj;
	int                       rc;

	M0_PRE(cache != NULL);
	M0_PRE(parameters != NULL);

	for (param = parameters; param->scp_type != NULL; ++param) {
		if (param->scp_fid == NULL ||
		    m0_conf_fid_type(param->scp_fid) != param->scp_type)
			return M0_ERR(-EINVAL);
		rc = m0_conf_obj_find(cache, param->scp_fid, param->scp_obj);
		if (rc != 0)
			return M0_ERR(rc);
	}
	obj = *parameters->scp_obj;
	if (obj->co_status == M0_CS_MISSING) {
		M0_LOG(M0_INFO, FID_F": OK", FID_P(&obj->co_id));
		return M0_RC(0);
	}
	M0_ASSERT(obj->co_status == M0_CS_READY);
	return M0_ERR_INFO(-EEXIST, FID_F": Not a stub", FID_P(&obj->co_id));
}

int m0_spiel_root_add(struct m0_spiel_tx   *tx,
		      const struct m0_fid  *rootfid,
		      const struct m0_fid  *mdpool,
		      const struct m0_fid  *imeta_pver,
		      uint32_t              mdredundancy,
		      const char          **params)
{
	int                  rc;
	struct m0_conf_obj  *root_obj;
	struct m0_conf_root *root;
	struct m0_conf_obj  *pool;
	struct m0_conf_obj  *pver;

	M0_ENTRY();
	if (M0_IN(NULL, (rootfid, mdpool, imeta_pver, params)))
		return M0_ERR(-EPROTO);

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(
		&tx->spt_cache,
		/*
		 * Note: currently there are no constraints on the type of
		 * `rootfid'.
		 */
		{mdpool, &M0_CONF_POOL_TYPE, &pool},
		{&M0_CONF_ROOT_FID, &M0_CONF_ROOT_TYPE, &root_obj});
	if (rc == 0 && m0_fid_is_set(imeta_pver))
		rc = SPIEL_CONF_CHECK(&tx->spt_cache,
				      {imeta_pver, &M0_CONF_PVER_TYPE, &pver});
	if (rc != 0)
		goto out;

	M0_PRE(root_obj->co_status == M0_CS_MISSING);
	root = M0_CONF_CAST(root_obj, m0_conf_root);

	root->rt_params = m0_strings_dup(params);
	if (root->rt_params == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}
	root->rt_rootfid      = *rootfid;
	root->rt_mdpool       = *mdpool;
	root->rt_imeta_pver   = *imeta_pver;
	root->rt_mdredundancy = mdredundancy;
	/*
	 * Real version number will be set during transaction commit in
	 * one of two ways:
	 *
	 * 1. Spiel client may provide version number explicitly when
	 *    calling m0_spiel_tx_commit_forced().
	 *
	 * 2. In case client does not provide any valid version number,
	 *    current maximum version number known among confd services
	 *    will be fetched by rconfc. This value will be incremented
	 *    and used as version number of the currently populated
	 *    conf DB.
	 */
	root->rt_verno = M0_CONF_VER_TEMP;

	rc = spiel_root_dirs_create(root);
	if (rc != 0) {
		m0_free0(&root->rt_params);
		goto out;
	}
	root_obj->co_status = M0_CS_READY;
	M0_POST(m0_conf_obj_invariant(root_obj));
out:
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_root_add);

int m0_spiel_node_add(struct m0_spiel_tx  *tx,
		      const struct m0_fid *fid,
		      uint32_t             memsize,
		      uint32_t             nr_cpu,
		      uint64_t             last_state,
		      uint64_t             flags)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_node *node;
	struct m0_conf_root *root;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_NODE_TYPE, &obj },
			      {&M0_CONF_ROOT_FID, &M0_CONF_ROOT_TYPE,
			       &obj_parent});
	if (rc != 0)
		goto fail;

	node = M0_CONF_CAST(obj, m0_conf_node);
	node->cn_memsize = memsize;
	node->cn_nr_cpu = nr_cpu;
	node->cn_last_state = last_state;
	node->cn_flags = flags;

	rc = spiel_node_dirs_create(node);
	if (rc != 0)
		goto fail;
	root = M0_CONF_CAST(obj_parent, m0_conf_root);
	if (root->rt_nodes == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_root_dirs_create(root);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(root->rt_nodes, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_node_add);

static bool spiel_cores_is_valid(const struct m0_bitmap *cores)
{
	return cores != NULL && m0_exists(i, cores->b_nr,
					  cores->b_words[i] != 0);
}

int m0_spiel_process_add(struct m0_spiel_tx  *tx,
			 const struct m0_fid *fid,
			 const struct m0_fid *parent,
			 struct m0_bitmap    *cores,
			 uint64_t             memlimit_as,
			 uint64_t             memlimit_rss,
			 uint64_t             memlimit_stack,
			 uint64_t             memlimit_memlock,
			 const char          *endpoint)
{
	int                     rc;
	struct m0_conf_obj     *obj = NULL;
	struct m0_conf_process *process;
	struct m0_conf_obj     *obj_parent;
	struct m0_conf_node    *node;

	M0_ENTRY();
	if (!spiel_cores_is_valid(cores) || endpoint == NULL)
		return M0_ERR(-EINVAL);
	m0_mutex_lock(&tx->spt_lock);

	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_PROCESS_TYPE, &obj },
			      {parent, &M0_CONF_NODE_TYPE, &obj_parent});
	if (rc != 0)
		goto fail;

	process = M0_CONF_CAST(obj, m0_conf_process);
	rc = m0_bitmap_init(&process->pc_cores, cores->b_nr);
	if (rc != 0)
		goto fail;
	/* XXX FIXME: process->pc_cores.b_words will leak in case of error */
	m0_bitmap_copy(&process->pc_cores, cores);
	process->pc_memlimit_as      = memlimit_as;
	process->pc_memlimit_rss     = memlimit_rss;
	process->pc_memlimit_stack   = memlimit_stack;
	process->pc_memlimit_memlock = memlimit_memlock;
	process->pc_endpoint         = m0_strdup(endpoint);
	if (process->pc_endpoint == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto fail;
	}
	/* XXX FIXME: process->pc_endpoint will leak in case of error */

	rc = spiel_process_dirs_create(process);
	if (rc != 0)
		goto fail;
	node = M0_CONF_CAST(obj_parent, m0_conf_node);
	if (node->cn_processes == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_node_dirs_create(node);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(node->cn_processes, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_process_add);

int m0_spiel_service_add(struct m0_spiel_tx                 *tx,
			 const struct m0_fid                *fid,
			 const struct m0_fid                *parent,
			 const struct m0_spiel_service_info *service_info)
{
	int                     rc;
	struct m0_conf_obj     *obj = NULL;
	struct m0_conf_service *service;
	struct m0_conf_obj     *obj_parent;
	struct m0_conf_process *process;

	M0_ENTRY();
	if (service_info == NULL ||
	    !m0_conf_service_type_is_valid(service_info->svi_type))
		return M0_ERR(-EINVAL);
	m0_mutex_lock(&tx->spt_lock);

	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_SERVICE_TYPE, &obj },
			      {parent, &M0_CONF_PROCESS_TYPE, &obj_parent});
	if (rc != 0)
		goto fail;

	service = M0_CONF_CAST(obj, m0_conf_service);
	service->cs_type = service_info->svi_type;
	service->cs_endpoints = m0_strings_dup(service_info->svi_endpoints);
	if (service->cs_endpoints == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto fail;
	}

	rc = spiel_service_dirs_create(service);
	if (rc != 0)
		goto fail;
	process = M0_CONF_CAST(obj_parent, m0_conf_process);
	if (process->pc_services == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_process_dirs_create(process);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(process->pc_services, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_service_add);

int m0_spiel_device_add(struct m0_spiel_tx                        *tx,
			const struct m0_fid                       *fid,
			const struct m0_fid                       *parent,
			const struct m0_fid                       *drive,
		        uint32_t                                   dev_idx,
			enum m0_cfg_storage_device_interface_type  iface,
			enum m0_cfg_storage_device_media_type      media,
			uint32_t                                   bsize,
			uint64_t                                   size,
			uint64_t                                   last_state,
			uint64_t                                   flags,
			const char                                *filename)
{
	int                     rc;
	struct m0_conf_obj     *obj = NULL;
	struct m0_conf_sdev    *device;
	struct m0_conf_obj     *svc_obj;
	struct m0_conf_obj     *drv_obj;
	struct m0_conf_service *service;
	struct m0_conf_drive   *drv;

	M0_ENTRY();
	if (dev_idx > M0_FID_DEVICE_ID_MAX ||
	    !M0_CFG_SDEV_INTERFACE_TYPE_IS_VALID(iface) ||
	    !M0_CFG_SDEV_MEDIA_TYPE_IS_VALID(media) ||
	    filename == NULL)
		return M0_ERR(-EINVAL);

	m0_mutex_lock(&tx->spt_lock);

	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_SDEV_TYPE, &obj },
			      {parent, &M0_CONF_SERVICE_TYPE, &svc_obj},
			      {drive, &M0_CONF_DRIVE_TYPE, &drv_obj});
	if (rc != 0)
		goto fail;

	device = M0_CONF_CAST(obj, m0_conf_sdev);
	device->sd_dev_idx = dev_idx;
	device->sd_iface = iface;
	device->sd_media = media;
	device->sd_bsize = bsize;
	device->sd_size = size;
	device->sd_last_state = last_state;
	device->sd_flags = flags;
	device->sd_filename = m0_strdup(filename);
	if (device->sd_filename == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto fail;
	}
	/* XXX FIXME: device->sd_filename will leak in case of error */

	service = M0_CONF_CAST(svc_obj, m0_conf_service);
	if (service->cs_sdevs == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_service_dirs_create(service);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(service->cs_sdevs, obj);
	drv = M0_CONF_CAST(drv_obj, m0_conf_drive);
	if (drv->ck_sdev != NULL) {
		m0_conf_dir_del(service->cs_sdevs, obj);
		rc = M0_ERR(-EINVAL);
		goto fail;
	}
	drv->ck_sdev = device;
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_device_add);

int m0_spiel_profile_add(struct m0_spiel_tx *tx, const struct m0_fid *fid)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_root *root;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);

	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_PROFILE_TYPE, &obj},
			      {&M0_CONF_ROOT_FID, &M0_CONF_ROOT_TYPE,
			       &obj_parent});
	if (rc != 0)
		goto fail;

	root = M0_CONF_CAST(obj_parent, m0_conf_root);
	if (root->rt_profiles == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_root_dirs_create(root);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(root->rt_profiles, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_profile_add);

static int spiel_profile_pool_add(struct m0_conf_profile *prof,
				  const struct m0_fid    *pool)
{
	struct m0_fid_arr *fids = &prof->cp_pools;
	struct m0_fid     *tmp;

	M0_ENTRY();

	if (m0_exists(i, fids->af_count, m0_fid_eq(&fids->af_elems[i], pool)))
		return M0_ERR_INFO(-EEXIST, "prof="FID_F" pool="FID_F,
				   FID_P(&prof->cp_obj.co_id), FID_P(pool));

	M0_ALLOC_ARR(tmp, fids->af_count + 1);
	if (tmp == NULL)
		return M0_ERR(-ENOMEM);

	/* Copy existing fids to `tmp` array .. */
	memcpy(tmp, fids->af_elems, fids->af_count * sizeof(fids->af_elems[0]));
	/* .. and add new fid. */
	tmp[fids->af_count] = *pool;

	/* Update prof->cp_pools. */
	m0_free(fids->af_elems);
	fids->af_elems = tmp;
	M0_CNT_INC(fids->af_count);

	return M0_RC(0);
}

int m0_spiel_profile_pool_add(struct m0_spiel_tx  *tx,
			      const struct m0_fid *profile,
			      const struct m0_fid *pool)
{
	int                     rc;
	struct m0_conf_obj     *obj;
	struct m0_conf_profile *prof;

	M0_ENTRY("profile="FID_F" pool="FID_F, FID_P(profile), FID_P(pool));
	m0_mutex_lock(&tx->spt_lock);

	rc = m0_conf_obj_find(&tx->spt_cache, profile, &obj);
	if (rc != 0) {
		M0_ERR(rc);
		goto out;
	}
	if (obj->co_status != M0_CS_READY) {
		rc = M0_ERR_INFO(-EPROTO, "No profile to add pool to."
				 " m0_spiel_profile_add() missing?");
		goto out;
	}
	prof = M0_CONF_CAST(obj, m0_conf_profile);

	rc = spiel_profile_pool_add(prof, pool);
	if (rc != 0) {
		M0_ERR(rc);
		goto out;
	}
	M0_POST(m0_conf_obj_invariant(obj));
out:
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_profile_pool_add);

int m0_spiel_pool_add(struct m0_spiel_tx  *tx,
		      const struct m0_fid *fid,
		      uint32_t             pver_policy)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_pool *pool;
	struct m0_conf_obj  *parent;
	struct m0_conf_root *root;

	M0_ENTRY(FID_F, FID_P(fid));

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_POOL_TYPE, &obj },
			      {&M0_CONF_ROOT_FID, &M0_CONF_ROOT_TYPE, &parent});
	if (rc != 0)
		goto out;

	pool = M0_CONF_CAST(obj, m0_conf_pool);
	pool->pl_pver_policy = pver_policy;
	if (pool->pl_pvers == NULL) {
		rc = spiel_pool_dirs_create(pool);
		if (rc != 0)
			goto out;
	}
	root = M0_CONF_CAST(parent, m0_conf_root);
	if (root->rt_pools == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_root_dirs_create(root);
		if (rc != 0)
			goto out;
	}
	m0_conf_dir_add(root->rt_pools, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
out:
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_pool_add);

int m0_spiel_site_add(struct m0_spiel_tx  *tx,
		      const struct m0_fid *fid)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_site *site;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_root *root;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_SITE_TYPE, &obj },
			      {&M0_CONF_ROOT_FID, &M0_CONF_ROOT_TYPE,
			       &obj_parent});
	if (rc != 0)
		goto fail;

	site = M0_CONF_CAST(obj, m0_conf_site);
	rc = spiel_site_dirs_create(site);
	if (rc != 0)
		goto fail;
	root = M0_CONF_CAST(obj_parent, m0_conf_root);
	if (root->rt_sites == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_root_dirs_create(root);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(root->rt_sites, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_site_add);

int m0_spiel_rack_add(struct m0_spiel_tx  *tx,
		      const struct m0_fid *fid,
		      const struct m0_fid *parent)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_rack *rack;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_site *site;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_RACK_TYPE, &obj },
			      {parent, &M0_CONF_SITE_TYPE, &obj_parent});
	if (rc != 0)
		goto fail;

	rack = M0_CONF_CAST(obj, m0_conf_rack);
	rc = spiel_rack_dirs_create(rack);
	if (rc != 0)
		goto fail;
	site = M0_CONF_CAST(obj_parent, m0_conf_site);
	if (site->ct_racks == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_site_dirs_create(site);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(site->ct_racks, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_rack_add);

int m0_spiel_enclosure_add(struct m0_spiel_tx  *tx,
			   const struct m0_fid *fid,
			   const struct m0_fid *parent)
{
	int                       rc;
	struct m0_conf_obj       *obj = NULL;
	struct m0_conf_enclosure *enclosure;
	struct m0_conf_obj       *obj_parent;
	struct m0_conf_rack      *rack;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_ENCLOSURE_TYPE, &obj },
			      {parent, &M0_CONF_RACK_TYPE, &obj_parent});
	if (rc != 0)
		goto fail;

	enclosure = M0_CONF_CAST(obj, m0_conf_enclosure);
	rc = spiel_enclosure_dirs_create(enclosure);
	if (rc != 0)
		goto fail;
	rack = M0_CONF_CAST(obj_parent, m0_conf_rack);
	if (rack->cr_encls == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_rack_dirs_create(rack);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(rack->cr_encls, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_enclosure_add);

int m0_spiel_controller_add(struct m0_spiel_tx  *tx,
			    const struct m0_fid *fid,
			    const struct m0_fid *parent,
			    const struct m0_fid *node)
{
	int                        rc;
	struct m0_conf_obj        *obj = NULL;
	struct m0_conf_obj        *node_obj;
	struct m0_conf_controller *controller;
	struct m0_conf_obj        *obj_parent;
	struct m0_conf_enclosure  *enclosure;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_CONTROLLER_TYPE, &obj },
			      {parent, &M0_CONF_ENCLOSURE_TYPE, &obj_parent},
			      {node, &M0_CONF_NODE_TYPE, &node_obj});
	if (rc != 0)
		goto fail;

	controller = M0_CONF_CAST(obj, m0_conf_controller);
	controller->cc_node = M0_CONF_CAST(node_obj, m0_conf_node);
	rc = spiel_controller_dirs_create(controller);
	if (rc != 0)
		goto fail;
	enclosure = M0_CONF_CAST(obj_parent, m0_conf_enclosure);
	if (enclosure->ce_ctrls == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_enclosure_dirs_create(enclosure);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(enclosure->ce_ctrls, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_controller_add);

int m0_spiel_drive_add(struct m0_spiel_tx  *tx,
		       const struct m0_fid *fid,
		       const struct m0_fid *parent)
{
	int                        rc;
	struct m0_conf_obj        *obj = NULL;
	struct m0_conf_obj        *obj_parent;
	struct m0_conf_controller *controller;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_DRIVE_TYPE, &obj },
			      {parent, &M0_CONF_CONTROLLER_TYPE, &obj_parent});
	if (rc != 0)
		goto fail;

	controller = M0_CONF_CAST(obj_parent, m0_conf_controller);
	if (controller->cc_drives == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_controller_dirs_create(controller);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(controller->cc_drives, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_drive_add);

int m0_spiel_pver_actual_add(struct m0_spiel_tx           *tx,
			     const struct m0_fid          *fid,
			     const struct m0_fid          *parent,
			     const struct m0_pdclust_attr *attrs,
			     uint32_t                     *tolerance,
			     uint32_t                      tolerance_len)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_pver *pver = NULL;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_pool *pool;

	M0_ENTRY();

	if (tolerance_len != M0_CONF_PVER_HEIGHT)
		return M0_ERR(-EINVAL);
	if (!m0_pdclust_attr_check(attrs))
		return M0_ERR(-EINVAL);

	m0_mutex_lock(&tx->spt_lock);

	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_PVER_TYPE, &obj},
			      {parent, &M0_CONF_POOL_TYPE, &obj_parent});
	if (rc != 0)
		goto fail;

	pver = M0_CONF_CAST(obj, m0_conf_pver);
	pver->pv_kind = M0_CONF_PVER_ACTUAL;
	memcpy(pver->pv_u.subtree.pvs_tolerance, tolerance,
	       M0_CONF_PVER_HEIGHT * sizeof(*tolerance));
	pver->pv_u.subtree.pvs_attr = *attrs;

	rc = spiel_pver_dirs_create(pver);
	if (rc != 0)
		goto fail;
	pool = M0_CONF_CAST(obj_parent, m0_conf_pool);
	if (pool->pl_pvers == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_pool_dirs_create(pool);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(pool->pl_pvers, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_pver_actual_add);

int m0_spiel_pver_formulaic_add(struct m0_spiel_tx  *tx,
				const struct m0_fid *fid,
				const struct m0_fid *parent,
				uint32_t             index,
				const struct m0_fid *base_pver,
				uint32_t            *allowance,
				uint32_t             allowance_len)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_obj  *base_obj = NULL;
	struct m0_conf_pver *pver = NULL;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_pool *pool;

	M0_ENTRY();

	if (allowance_len != M0_CONF_PVER_HEIGHT)
		return M0_ERR(-EINVAL);
	m0_mutex_lock(&tx->spt_lock);

	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_PVER_TYPE, &obj},
			      {base_pver, &M0_CONF_PVER_TYPE, &base_obj},
			      {parent, &M0_CONF_POOL_TYPE, &obj_parent});
	if (rc != 0)
		goto fail;

	pver = M0_CONF_CAST(obj, m0_conf_pver);
	pver->pv_kind = M0_CONF_PVER_FORMULAIC;
	memcpy(pver->pv_u.formulaic.pvf_allowance, allowance,
	       M0_CONF_PVER_HEIGHT * sizeof(*allowance));
	pver->pv_u.formulaic.pvf_id = index;
	pver->pv_u.formulaic.pvf_base = *base_pver;

	pool = M0_CONF_CAST(obj_parent, m0_conf_pool);
	if (pool->pl_pvers == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_pool_dirs_create(pool);
		if (rc != 0)
			goto fail;
	}

	m0_conf_dir_add(pool->pl_pvers, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_pver_formulaic_add);

int m0_spiel_site_v_add(struct m0_spiel_tx  *tx,
			const struct m0_fid *fid,
			const struct m0_fid *parent,
			const struct m0_fid *real)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_objv *objv;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_pver *pver;
	struct m0_conf_obj  *real_obj;

	M0_ENTRY();

	real_obj = m0_conf_cache_lookup(&tx->spt_cache, real);
	if (real_obj == NULL)
		return M0_ERR(-ENOENT);

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_OBJV_TYPE, &obj },
			      {parent, &M0_CONF_PVER_TYPE, &obj_parent},
			      {real, &M0_CONF_SITE_TYPE, &real_obj});
	if (rc != 0)
		goto fail;

	objv = M0_CONF_CAST(obj, m0_conf_objv);
	objv->cv_real = real_obj;
	rc = spiel_sitev_dirs_create(objv);
	if (rc != 0)
		goto fail;
	pver = M0_CONF_CAST(obj_parent, m0_conf_pver);
	if (pver->pv_u.subtree.pvs_sitevs == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_pver_dirs_create(pver);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(pver->pv_u.subtree.pvs_sitevs, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_site_v_add);

int m0_spiel_rack_v_add(struct m0_spiel_tx  *tx,
			const struct m0_fid *fid,
			const struct m0_fid *parent,
			const struct m0_fid *real)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_objv *objv;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_objv *objv_parent;
	struct m0_conf_obj  *real_obj;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_OBJV_TYPE, &obj },
			      {parent, &M0_CONF_OBJV_TYPE, &obj_parent},
			      {real, &M0_CONF_RACK_TYPE, &real_obj});
	if (rc != 0)
		goto fail;

	objv = M0_CONF_CAST(obj, m0_conf_objv);
	objv->cv_real = real_obj;
	rc = spiel_rackv_dirs_create(objv);
	if (rc != 0)
		goto fail;
	objv_parent = M0_CONF_CAST(obj_parent, m0_conf_objv);
	if (objv_parent->cv_children == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_sitev_dirs_create(objv_parent);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(objv_parent->cv_children, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_rack_v_add);

int m0_spiel_enclosure_v_add(struct m0_spiel_tx  *tx,
			     const struct m0_fid *fid,
			     const struct m0_fid *parent,
			     const struct m0_fid *real)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_objv *objv;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_objv *objv_parent;
	struct m0_conf_obj  *real_obj;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_OBJV_TYPE, &obj },
			      {parent, &M0_CONF_OBJV_TYPE, &obj_parent},
			      {real, &M0_CONF_ENCLOSURE_TYPE, &real_obj});
	if (rc != 0)
		goto fail;

	objv = M0_CONF_CAST(obj, m0_conf_objv);
	objv->cv_real = real_obj;
	rc = spiel_enclosurev_dirs_create(objv);
	if (rc != 0)
		goto fail;
	objv_parent = M0_CONF_CAST(obj_parent, m0_conf_objv);
	if (objv_parent->cv_children == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_rackv_dirs_create(objv_parent);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(objv_parent->cv_children, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_enclosure_v_add);

int m0_spiel_controller_v_add(struct m0_spiel_tx  *tx,
			      const struct m0_fid *fid,
			      const struct m0_fid *parent,
			      const struct m0_fid *real)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_objv *objv;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_objv *objv_parent;
	struct m0_conf_obj  *real_obj;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_OBJV_TYPE, &obj },
			      {parent, &M0_CONF_OBJV_TYPE, &obj_parent},
			      {real, &M0_CONF_CONTROLLER_TYPE, &real_obj});
	if (rc != 0)
		goto fail;

	objv = M0_CONF_CAST(obj, m0_conf_objv);
	objv->cv_real = real_obj;
	rc = spiel_controllerv_dirs_create(objv);
	if (rc != 0)
		goto fail;
	objv_parent = M0_CONF_CAST(obj_parent, m0_conf_objv);
	if (objv_parent->cv_children == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_enclosurev_dirs_create(objv_parent);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(objv_parent->cv_children, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_controller_v_add);

int m0_spiel_drive_v_add(struct m0_spiel_tx  *tx,
			 const struct m0_fid *fid,
			 const struct m0_fid *parent,
			 const struct m0_fid *real)
{
	int                  rc;
	struct m0_conf_obj  *obj = NULL;
	struct m0_conf_objv *objv;
	struct m0_conf_obj  *obj_parent;
	struct m0_conf_objv *objv_parent;
	struct m0_conf_obj  *real_obj;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	rc = SPIEL_CONF_CHECK(&tx->spt_cache,
			      {fid, &M0_CONF_OBJV_TYPE, &obj },
			      {parent, &M0_CONF_OBJV_TYPE, &obj_parent},
			      {real, &M0_CONF_DRIVE_TYPE, &real_obj});
	if (rc != 0)
		goto fail;

	objv = M0_CONF_CAST(obj, m0_conf_objv);
	objv->cv_real = real_obj;

	objv_parent = M0_CONF_CAST(obj_parent, m0_conf_objv);
	if (objv_parent->cv_children == NULL) {
		/* Parent dir does not exist ==> create it. */
		rc = spiel_controllerv_dirs_create(objv_parent);
		if (rc != 0)
			goto fail;
	}
	m0_conf_dir_add(objv_parent->cv_children, obj);
	obj->co_status = M0_CS_READY;

	M0_POST(m0_conf_obj_invariant(obj));
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(0);
fail:
	if (obj != NULL && rc != -EEXIST)
		m0_conf_cache_del(&tx->spt_cache, obj);
	m0_mutex_unlock(&tx->spt_lock);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_drive_v_add);

static int spiel_pver_add(struct m0_conf_obj **obj_v, struct m0_conf_pver *pver)
{
	struct m0_conf_obj             *obj;
	const struct m0_conf_obj_type  *obj_type;
	struct m0_conf_pver           **pvers;
	struct m0_conf_pver           **pvers_new;
	unsigned                        nr_pvers;

	M0_ENTRY();
	/*
	 * XXX TODO: pver->pv_u.subtree.pvs_tolerance should be validated.
	 * m0_fd_tolerance_check() performs the validation, but it requires
	 * m0_confc, which spiel_pver_add() does not have access to
	 * (tx->spt_cache does not belong to any confc instance).  We might
	 * need to rewrite m0_fd_tolerance_check() to not use confc and work
	 * with m0_conf_cache only.
	 */
	obj = M0_CONF_CAST(*obj_v, m0_conf_objv)->cv_real;
	obj_type = m0_conf_obj_type(obj);
	pvers = m0_conf_pvers(obj);
	for (nr_pvers = 0; pvers != NULL && pvers[nr_pvers] != NULL; ++nr_pvers)
		/* count the elements */;

	/* Element count = old count + new + finish NULL */
	M0_ALLOC_ARR(pvers_new, nr_pvers + 2);
	if (pvers_new == NULL || M0_FI_ENABLED("fail_allocation"))
		return M0_ERR(-ENOMEM);
	memcpy(pvers_new, pvers, nr_pvers * sizeof(*pvers));
	pvers_new[nr_pvers] = pver;

	if (obj_type == &M0_CONF_SITE_TYPE)
		M0_CONF_CAST(obj, m0_conf_site)->ct_pvers = pvers_new;
	else if (obj_type == &M0_CONF_RACK_TYPE)
		M0_CONF_CAST(obj, m0_conf_rack)->cr_pvers = pvers_new;
	else if (obj_type == &M0_CONF_ENCLOSURE_TYPE)
		M0_CONF_CAST(obj, m0_conf_enclosure)->ce_pvers = pvers_new;
	else if (obj_type == &M0_CONF_CONTROLLER_TYPE)
		M0_CONF_CAST(obj, m0_conf_controller)->cc_pvers = pvers_new;
	else if (obj_type == &M0_CONF_DRIVE_TYPE)
		M0_CONF_CAST(obj, m0_conf_drive)->ck_pvers = pvers_new;
	else
		M0_IMPOSSIBLE("");

	m0_free(pvers);
	return M0_RC(0);
}

/**
 * Removes `pver' element from m0_conf_pvers(obj).
 *
 * @note spiel_pver_delete() does not change the size of pvers array.
 * If pvers has pver then after remove it pvers has two NULLs end.
 */
static int spiel_pver_delete(struct m0_conf_obj        *obj,
			     const struct m0_conf_pver *pver)
{
	struct m0_conf_pver **pvers = m0_conf_pvers(obj);
	unsigned              i;
	bool                  found = false;

	M0_ENTRY();
	M0_PRE(pver != NULL);

	if (pvers == NULL)
		return M0_ERR(-ENOENT);

	for (i = 0; pvers[i] != NULL; ++i) {
		if (pvers[i] == pver)
			found = true;
		if (found)
			pvers[i] = pvers[i + 1];
	}
	return M0_RC(found ? 0 : -ENOENT);
}

static int spiel_objv_remove(struct m0_conf_obj  **obj,
			     struct m0_conf_pver  *pver)
{
	struct m0_conf_objv *objv = M0_CONF_CAST(*obj, m0_conf_objv);

	if (objv->cv_children != NULL)
		m0_conf_cache_del((*obj)->co_cache, &objv->cv_children->cd_obj);
	m0_conf_obj_put(*obj);
	m0_conf_dir_tlist_del(*obj);
	m0_conf_cache_del((*obj)->co_cache, *obj);
	*obj = NULL;
	return M0_RC(0);
}

static int spiel_pver_iterator(struct m0_conf_obj  *dir,
			       struct m0_conf_pver *pver,
			       int (*action)(struct m0_conf_obj**,
					     struct m0_conf_pver*))
{
	int                  rc;
	struct m0_conf_obj  *entry;
	struct m0_conf_objv *objv;

	m0_conf_obj_get(dir); /* required by ->coo_readdir() */
	for (entry = NULL; (rc = dir->co_ops->coo_readdir(dir, &entry)) > 0; ) {
		/* All configuration is expected to be available. */
		M0_ASSERT(rc != M0_CONF_DIRMISS);

		objv = M0_CONF_CAST(entry, m0_conf_objv);
		rc = objv->cv_children == NULL ? 0 :
		      spiel_pver_iterator(&objv->cv_children->cd_obj, pver,
					  action);
		rc = rc ?: action(&entry, pver);
		if (rc != 0) {
			m0_conf_obj_put(entry);
			break;
		}
	}
	m0_conf_obj_put(dir);
	return M0_RC(rc);
}

int m0_spiel_pool_version_done(struct m0_spiel_tx  *tx,
			       const struct m0_fid *fid)
{
	int                  rc;
	struct m0_conf_pver *pver;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	pver = M0_CONF_CAST(m0_conf_cache_lookup(&tx->spt_cache, fid),
			    m0_conf_pver);
	M0_ASSERT(pver != NULL);
	rc = spiel_pver_iterator(&pver->pv_u.subtree.pvs_sitevs->cd_obj, pver,
				 &spiel_pver_add);
	if (rc != 0) {
		spiel_pver_iterator(&pver->pv_u.subtree.pvs_sitevs->cd_obj,
				    pver, &spiel_objv_remove);
		/*
		 * TODO: Remove this line once m0_spiel_element_del removes
		 * the object itself and all its m0_conf_dir members.
		 */
		m0_conf_cache_del(&tx->spt_cache,
				  &pver->pv_u.subtree.pvs_sitevs->cd_obj);

		m0_mutex_unlock(&tx->spt_lock);
		m0_spiel_element_del(tx, fid);
	} else {
		m0_mutex_unlock(&tx->spt_lock);
	}
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_pool_version_done);

static void spiel_pver_remove(struct m0_conf_cache *cache,
			      struct m0_conf_pver  *pver)
{
	struct m0_conf_obj *obj;

	M0_ENTRY();
	m0_tl_for (m0_conf_cache, &cache->ca_registry, obj) {
		if (M0_IN(m0_conf_obj_type(obj),
			  (&M0_CONF_SITE_TYPE, &M0_CONF_RACK_TYPE,
			   &M0_CONF_ENCLOSURE_TYPE, &M0_CONF_CONTROLLER_TYPE,
			   &M0_CONF_DRIVE_TYPE)))
			spiel_pver_delete(obj, pver);
	} m0_tl_endfor;
	M0_LEAVE();
}

int m0_spiel_element_del(struct m0_spiel_tx *tx, const struct m0_fid *fid)
{
	int                 rc = 0;
	struct m0_conf_obj *obj;

	M0_ENTRY();

	m0_mutex_lock(&tx->spt_lock);
	obj = m0_conf_cache_lookup(&tx->spt_cache, fid);
	if (obj != NULL) {
		if (m0_conf_obj_type(obj) == &M0_CONF_PVER_TYPE)
			spiel_pver_remove(&tx->spt_cache,
					  M0_CONF_CAST(obj, m0_conf_pver));
		m0_conf_dir_tlist_del(obj);
		m0_conf_cache_del(&tx->spt_cache, obj);
	}
	m0_mutex_unlock(&tx->spt_lock);
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_element_del);

static int spiel_str_to_file(char *str, const char *filename)
{
#ifdef __KERNEL__
	return 0;
#else
	int   rc;
	FILE *file;

	file = fopen(filename, "w+");
	if (file == NULL)
		return -errno;
	rc = fwrite(str, strlen(str), 1, file) == 1 ? 0 : -EINVAL;
	fclose(file);
	return rc;
#endif
}

static int spiel_tx_to_str(struct m0_spiel_tx *tx,
			   uint64_t            ver_forced,
			   char              **str,
			   bool                debug)
{
	M0_ENTRY();
	M0_PRE(ver_forced != M0_CONF_VER_UNKNOWN);

	return M0_RC(spiel_root_ver_update(tx, ver_forced) ?:
		     m0_conf_cache_to_string(&tx->spt_cache, str, debug));
}

int m0_spiel_tx_to_str(struct m0_spiel_tx *tx,
		       uint64_t            ver_forced,
		       char              **str)
{
	return spiel_tx_to_str(tx, ver_forced, str, false);
}
M0_EXPORTED(m0_spiel_tx_to_str);

void m0_spiel_tx_str_free(char *str)
{
	m0_confx_string_free(str);
}
M0_EXPORTED(m0_spiel_tx_str_free);

static int spiel_tx_dump(struct m0_spiel_tx *tx,
			 uint64_t            ver_forced,
			 const char         *filename,
			 bool                debug)
{
	int   rc;
	char *buffer;

	M0_ENTRY();
	rc = spiel_tx_to_str(tx, ver_forced, &buffer, debug);
	if (rc == 0) {
		rc = spiel_str_to_file(buffer, filename);
		m0_spiel_tx_str_free(buffer);
	}
	return M0_RC(rc);
}

int m0_spiel_tx_dump(struct m0_spiel_tx *tx, uint64_t ver_forced,
		     const char *filename)
{
	return M0_RC(spiel_tx_dump(tx, ver_forced, filename, false));
}
M0_EXPORTED(m0_spiel_tx_dump);

int m0_spiel_tx_dump_debug(struct m0_spiel_tx *tx, uint64_t ver_forced,
			   const char *filename)
{
	return M0_RC(spiel_tx_dump(tx, ver_forced, filename, true));
}
M0_EXPORTED(m0_spiel_tx_dump_debug);

/** @} */
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
