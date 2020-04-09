/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Abhishek Saha <abhishek.saha@seagate.com>
 * Original creation date: 26-Feb-2019
 */

/**
 * @addtogroup clovis
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS

#include "lib/trace.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/io.h"
#include "ioservice/fid_convert.h"

/** Initialises the rm_lock_ctx */
static void clovis_rm_ctx_init(struct m0_clovis_rm_lock_ctx *ctx,
			       struct m0_clovis *m0c,
			       struct m0_fid *fid,
			       const struct m0_uint128 *group);

/** Finalizes the rm_lock_ctx */
static void clovis_rm_ctx_fini(struct m0_ref *ref);

/** Initialises lock request */
static void clovis_rm_lock_req_init(struct m0_clink *clink,
				    struct m0_rm_owner *owner,
				    const struct m0_uint128 *group,
				    struct m0_clovis_rm_lock_req *req);

/** Finalizes lock request */
static void clovis_rm_lock_req_fini(struct m0_clovis_rm_lock_req *req);

/** Lock request completion callback */
static void obj_lock_incoming_complete(struct m0_rm_incoming *in, int32_t rc);

/** Lock request conflict callback */
static void obj_lock_incoming_conflict(struct m0_rm_incoming *in);

const struct m0_rm_incoming_ops obj_lock_incoming_ops = {
	.rio_complete = obj_lock_incoming_complete,
	.rio_conflict = obj_lock_incoming_conflict
};

static bool rm_key_eq(const void *key1, const void *key2)
{
	return m0_fid_eq(key1, key2);
}

static uint64_t rm_hash_func(const struct m0_htable *htable, const void *k)
{
	return m0_fid_hash(k) % htable->h_bucket_nr;
}

M0_HT_DESCR_DEFINE(rm_ctx, "Hash-table for RM locks", M0_INTERNAL,
		   struct m0_clovis_rm_lock_ctx, rmc_hlink, rmc_magic,
		   M0_CLOVIS_RM_MAGIC, M0_CLOVIS_RM_HEAD_MAGIC,
		   rmc_key, rm_hash_func, rm_key_eq);

M0_HT_DEFINE(rm_ctx, M0_INTERNAL, struct m0_clovis_rm_lock_ctx, struct m0_fid);

int m0_clovis_obj_lock_init(struct m0_clovis_obj *obj,
			    const struct m0_uint128 *group)
{
	struct m0_fid                 fid;
	struct m0_clovis_rm_lock_ctx *ctx;
	struct m0_clovis_entity      *ent = &obj->ob_entity;
	struct m0_clovis             *m0c = ent->en_realm->re_instance;

	M0_ENTRY();
	M0_PRE(obj != NULL);

	m0_fid_gob_make(&fid, ent->en_id.u_hi, ent->en_id.u_lo);
	M0_LOG(M0_INFO, FID_F, FID_P(&fid));
	group = group == NULL ? &m0_rm_no_group : group;
	rm_ctx_hbucket_lock(&m0c->m0c_rm_ctxs, &fid);
	ctx = rm_ctx_htable_lookup(&m0c->m0c_rm_ctxs, &fid);
	if (ctx != NULL)
		m0_ref_get(&ctx->rmc_ref);
	else {
		M0_ALLOC_PTR(ctx);
		if (ctx == NULL) {
			rm_ctx_hbucket_unlock(&m0c->m0c_rm_ctxs, &fid);
			return M0_RC(-ENOMEM);
		}
		clovis_rm_ctx_init(ctx, m0c, &fid, group);
		rm_ctx_htable_add(&m0c->m0c_rm_ctxs, ctx);
	}
	m0_cookie_init(&obj->ob_cookie, &ctx->rmc_gen);
	rm_ctx_hbucket_unlock(&m0c->m0c_rm_ctxs, &fid);

	return M0_RC(0);
}
M0_EXPORTED(m0_clovis_obj_lock_init);

static void clovis_rm_ctx_init(struct m0_clovis_rm_lock_ctx *ctx,
			       struct m0_clovis *m0c,
			       struct m0_fid *fid,
			       const struct m0_uint128 *group)
{
	struct m0_rm_domain    *rdom;
	struct m0_pools_common *pc = &m0c->m0c_pools_common;

	M0_ENTRY();

	rdom = clovis_rm_domain_get(m0c);
	M0_ASSERT(rdom != NULL);

	ctx->rmc_magic = M0_CLOVIS_RM_MAGIC;
	ctx->rmc_htable = &m0c->m0c_rm_ctxs;
	ctx->rmc_key = *fid;
	m0_cookie_new(&ctx->rmc_gen);
	rm_ctx_tlink_init(ctx);
	m0_ref_init(&ctx->rmc_ref, 1, clovis_rm_ctx_fini);
	m0_file_init(&ctx->rmc_file, &ctx->rmc_key, rdom, M0_DI_DEFAULT_TYPE);
	m0_rm_remote_init(&ctx->rmc_creditor, &ctx->rmc_file.fi_res);
	ctx->rmc_creditor.rem_session = m0_pools_common_active_rm_session(pc);
	M0_ASSERT(ctx->rmc_creditor.rem_session != NULL);
	ctx->rmc_creditor.rem_state = REM_SERVICE_LOCATED;
	m0_file_owner_init(&ctx->rmc_owner, group, &ctx->rmc_file,
			   &ctx->rmc_creditor);

	M0_LEAVE();
}

void m0_clovis_obj_lock_fini(struct m0_clovis_obj *obj)
{
	struct m0_clovis_rm_lock_ctx *ctx;

	M0_ENTRY();
	M0_PRE(obj != NULL);

	ctx = m0_cookie_of(&obj->ob_cookie, struct m0_clovis_rm_lock_ctx,
			   rmc_gen);
	M0_ASSERT(ctx != NULL);
	rm_ctx_hbucket_lock(ctx->rmc_htable, &ctx->rmc_key);
	if (m0_ref_read(&ctx->rmc_ref) == 1) {
		rm_ctx_htable_del(ctx->rmc_htable, ctx);
		rm_ctx_hbucket_unlock(ctx->rmc_htable, &ctx->rmc_key);
		m0_ref_put(&ctx->rmc_ref);
	} else {
		m0_ref_put(&ctx->rmc_ref);
		rm_ctx_hbucket_unlock(ctx->rmc_htable, &ctx->rmc_key);
	}

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_obj_lock_fini);

static void clovis_rm_ctx_fini(struct m0_ref *ref)
{
	int                           rc;
	struct m0_clovis_rm_lock_ctx *ctx;

	M0_ENTRY();

	ctx = M0_AMB(ctx, ref, rmc_ref);

	m0_rm_owner_windup(&ctx->rmc_owner);
	rc = m0_rm_owner_timedwait(&ctx->rmc_owner,
				   M0_BITS(ROS_FINAL, ROS_INSOLVENT),
				   M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	m0_file_owner_fini(&ctx->rmc_owner);
	m0_rm_remote_fini(&ctx->rmc_creditor);
	m0_file_fini(&ctx->rmc_file);
	rm_ctx_tlink_fini(ctx);
	m0_free(ctx);

	M0_LEAVE();
}

int m0_clovis_obj_lock_get(struct m0_clovis_obj *obj,
			   struct m0_clovis_rm_lock_req *req,
			   const struct m0_uint128 *group,
			   struct m0_clink *clink)
{
	struct m0_clovis_rm_lock_ctx *ctx;

	M0_ENTRY();
	M0_PRE(obj != NULL);
	M0_PRE(req != NULL);
	M0_PRE(clink != NULL);
	M0_ASSERT(clink->cl_is_oneshot);

	ctx = m0_cookie_of(&obj->ob_cookie, struct m0_clovis_rm_lock_ctx,
			   rmc_gen);
	M0_ASSERT(ctx != NULL);
	req->rlr_rc = 0;
	group = group == NULL ? &m0_rm_no_group : group;
	clovis_rm_lock_req_init(clink, &ctx->rmc_owner, group, req);
	m0_rm_credit_get(&req->rlr_in);

	return M0_RC(0);
}
M0_EXPORTED(m0_clovis_obj_lock_get);

int m0_clovis_obj_lock_get_sync(struct m0_clovis_obj *obj,
				struct m0_clovis_rm_lock_req *req,
				const struct m0_uint128 *group)
{
	struct m0_clink clink;

	M0_ENTRY();
	M0_PRE(obj != NULL);
	M0_PRE(req != NULL);

	m0_clink_init(&clink, NULL);
	clink.cl_is_oneshot = true;
	m0_clovis_obj_lock_get(obj, req, group, &clink);
	m0_chan_wait(&clink);
	m0_clink_fini(&clink);

	return M0_RC(req->rlr_rc);
}
M0_EXPORTED(m0_clovis_obj_lock_get_sync);

void m0_clovis_obj_lock_put(struct m0_clovis_rm_lock_req *req)
{
	M0_ENTRY();

	M0_PRE(req != NULL);
	m0_rm_credit_put(&req->rlr_in);
	clovis_rm_lock_req_fini(req);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_obj_lock_put);

static void clovis_rm_lock_req_init(struct m0_clink *clink,
				    struct m0_rm_owner *owner,
				    const struct m0_uint128 *group,
				    struct m0_clovis_rm_lock_req *req)
{
	M0_ENTRY();

	m0_rm_incoming_init(&req->rlr_in, owner, M0_RIT_LOCAL, RIP_NONE,
			    RIF_LOCAL_WAIT | RIF_MAY_BORROW | RIF_MAY_REVOKE);
	req->rlr_in.rin_want.cr_group_id = *group;
	req->rlr_in.rin_want.cr_datum = RM_FILE_LOCK;
	req->rlr_in.rin_ops = &obj_lock_incoming_ops;
	m0_mutex_init(&req->rlr_mutex);
	m0_chan_init(&req->rlr_chan, &req->rlr_mutex);
	m0_clink_add_lock(&req->rlr_chan, clink);

	M0_LEAVE();
}

static void clovis_rm_lock_req_fini(struct m0_clovis_rm_lock_req *req)
{
	M0_ENTRY();

	m0_chan_fini_lock(&req->rlr_chan);
	m0_mutex_fini(&req->rlr_mutex);
	m0_rm_incoming_fini(&req->rlr_in);

	M0_LEAVE();
}

static void obj_lock_incoming_complete(struct m0_rm_incoming *in, int32_t rc)
{
	struct m0_clovis_rm_lock_req *req;

	M0_ENTRY();

	req = M0_AMB(req, in, rlr_in);
	req->rlr_rc = rc;
	/* Signals the thread waiting for the lock to be granted */
	m0_chan_broadcast_lock(&req->rlr_chan);

	M0_LEAVE();
	return;
}

static void obj_lock_incoming_conflict(struct m0_rm_incoming *in)
{
	/* Do nothing */
	return;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of clovis group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

