/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Subhash Arya <subhash_arya@xyratex.com>
 *                  Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 12/09/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"
#include "lib/finject.h"
#include "lib/locality.h"

#include "ioservice/io_service.h"
#include "ioservice/fid_convert.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"

#include "sns/cm/cm.h"
#include "sns/cm/cm_utils.h"
#include "sns/cm/file.h"
#include "pool/pool.h"

/**
   @addtogroup SNSCMFILE

   @{
 */

#define _AST2FCTX(ast, field) \
	container_of(ast, struct m0_sns_cm_file_ctx, field)

const struct m0_uint128 m0_rm_sns_cm_group = M0_UINT128(0, 2);
extern const struct m0_rm_resource_type_ops file_lock_type_ops;

static int _layout_fetch(struct m0_sns_cm_file_ctx *fctx);
static int _fid_layout_instance(struct m0_sns_cm_file_ctx *fctx);

M0_INTERNAL void m0_sns_cm_fctx_lock(struct m0_sns_cm_file_ctx *fctx)
{
	m0_mutex_lock(&fctx->sf_lock);
}

M0_INTERNAL void m0_sns_cm_fctx_unlock(struct m0_sns_cm_file_ctx *fctx)
{
	m0_mutex_unlock(&fctx->sf_lock);
}

static uint64_t sns_cm_fctx_hash_func(const struct m0_htable *htable,
				      const void *k)
{
	const struct m0_fid *fid = (struct m0_fid *)k;
	const uint64_t       key = fid->f_key;

	return key % htable->h_bucket_nr;
}

static bool sns_cm_fctx_key_eq(const void *key1, const void *key2)
{
	return m0_fid_eq((struct m0_fid *)key1, (struct m0_fid *)key2);
}

M0_HT_DESCR_DEFINE(m0_scmfctx, "Hash of files used by sns cm", M0_INTERNAL,
		   struct m0_sns_cm_file_ctx, sf_sc_link, sf_magic,
		   M0_SNS_CM_FILE_CTX_MAGIC, M0_SNS_CM_MAGIC,
		   sf_fid, sns_cm_fctx_hash_func,
		   sns_cm_fctx_key_eq);

M0_HT_DEFINE(m0_scmfctx, M0_INTERNAL, struct m0_sns_cm_file_ctx,
	     struct m0_fid);

static struct m0_sm_state_descr fctx_sd[M0_SCFS_NR] = {
	[M0_SCFS_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "file ctx init",
		.sd_allowed = M0_BITS(M0_SCFS_LOCK_WAIT, M0_SCFS_LOCKED)
	},
	[M0_SCFS_LOCK_WAIT] = {
		.sd_flags   = 0,
		.sd_name    = "file lock wait",
		.sd_allowed = M0_BITS(M0_SCFS_LOCKED, M0_SCFS_FINI)
	},
	[M0_SCFS_LOCKED] = {
		.sd_flags = 0,
		.sd_name  = "file locked",
		.sd_allowed = M0_BITS(M0_SCFS_ATTR_FETCH,
				      M0_SCFS_LAYOUT_FETCHED,
				      M0_SCFS_FINI)
	},
	[M0_SCFS_ATTR_FETCH] = {
		.sd_flags = 0,
		.sd_name  = "file attr fetch",
		.sd_allowed = M0_BITS(M0_SCFS_ATTR_FETCHED, M0_SCFS_FINI)
	},
	[M0_SCFS_ATTR_FETCHED] = {
		.sd_flags   = 0,
		.sd_name    = "file attr fetched",
		.sd_allowed = M0_BITS(M0_SCFS_LAYOUT_FETCH, M0_SCFS_FINI)
	},
	[M0_SCFS_LAYOUT_FETCH] = {
		.sd_flags = 0,
		.sd_name  = "file layout fetch",
		.sd_allowed = M0_BITS(M0_SCFS_LAYOUT_FETCHED, M0_SCFS_FINI)
	},
	[M0_SCFS_LAYOUT_FETCHED] = {
		.sd_flags   = 0,
		.sd_name    = "file ctx layout fetched",
		.sd_allowed = M0_BITS(M0_SCFS_FINI)
	},
	[M0_SCFS_FINI] = {
		.sd_flags = M0_SDF_TERMINAL,
		.sd_name  = "file ctx fini",
		.sd_allowed = 0
	}
};

static const struct m0_sm_conf fctx_sm_conf = {
	.scf_name      = "sm: fctx_conf",
	.scf_nr_states = ARRAY_SIZE(fctx_sd),
	.scf_state     = fctx_sd
};

M0_INTERNAL int m0_sns_cm_fctx_state_get(struct m0_sns_cm_file_ctx *fctx)
{
	return fctx->sf_sm.sm_state;
}

static void _fctx_status_set(struct m0_sns_cm_file_ctx *fctx, int state)
{
	m0_sm_state_set(&fctx->sf_sm, state);
}

static void __fctx_ast_post(struct m0_sns_cm_file_ctx *fctx,
			    struct m0_sm_ast *ast)
{
	m0_sm_ast_post(fctx->sf_group, ast);
}

static void _fctx_fini(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_sns_cm_file_ctx *fctx = _AST2FCTX(ast, sf_fini_ast);

	if (!m0_sns_cm2reqh(fctx->sf_scm)->rh_oostore) {
		m0_clink_del_lock(&fctx->sf_fini_clink);
		m0_file_owner_fini(&fctx->sf_owner);
		m0_rm_remote_fini(&fctx->sf_creditor);
		m0_file_fini(&fctx->sf_file);
	}
	m0_clink_fini(&fctx->sf_fini_clink);
	m0_mutex_fini(&fctx->sf_lock);
	_fctx_status_set(fctx, M0_SCFS_FINI);
	m0_sm_fini(&fctx->sf_sm);
	m0_free(fctx);
}

static void __sns_cm_fctx_cleanup(struct m0_sns_cm_file_ctx *fctx)
{
	if (fctx->sf_pi != NULL)
		m0_layout_instance_fini(&fctx->sf_pi->pi_base);
	if (fctx->sf_layout != NULL)
		m0_layout_put(fctx->sf_layout);
	m0_scmfctx_htable_del(&fctx->sf_scm->sc_file_ctx, fctx);
	m0_sns_cm_fctx_fini(fctx);
}

M0_INTERNAL void m0_sns_cm_fctx_cleanup(struct m0_sns_cm *scm)
{
	struct m0_sns_cm_file_ctx *fctx;

	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	m0_htable_for(m0_scmfctx, fctx, &scm->sc_file_ctx) {
		__sns_cm_fctx_cleanup(fctx);
	} m0_htable_endfor;
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);
}

static void sns_cm_fctx_release(struct m0_ref *ref)
{
	struct m0_sns_cm_file_ctx *fctx;

	M0_PRE(ref != NULL);

	fctx = container_of(ref, struct m0_sns_cm_file_ctx, sf_ref);
	__sns_cm_fctx_cleanup(fctx);
}

M0_INTERNAL void m0_sns_cm_flock_resource_set(struct m0_sns_cm *scm)
{
        scm->sc_rm_ctx.rc_rt.rt_id = M0_RM_FLOCK_RT;
        scm->sc_rm_ctx.rc_rt.rt_ops = &file_lock_type_ops;
}

static void sns_cm_fctx_rm_init(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_sns_cm_rm_ctx *rm_ctx;

	M0_PRE(fctx != NULL && fctx->sf_scm != NULL);

	rm_ctx = &fctx->sf_scm->sc_rm_ctx;
	m0_file_init(&fctx->sf_file, &fctx->sf_fid, &rm_ctx->rc_dom,
		     M0_DI_NONE);
	m0_rm_remote_init(&fctx->sf_creditor, &fctx->sf_file.fi_res);
	fctx->sf_creditor.rem_session =
		m0_pools_common_active_rm_session(rm_ctx->rc_pc);
	M0_ASSERT(fctx->sf_creditor.rem_session != NULL);
	fctx->sf_creditor.rem_state = REM_SERVICE_LOCATED;
	m0_file_owner_init(&fctx->sf_owner, &m0_rm_sns_cm_group, &fctx->sf_file,
			   &fctx->sf_creditor);
	m0_rm_incoming_init(&fctx->sf_rin, &fctx->sf_owner, M0_RIT_LOCAL,
			    RIP_NONE, RIF_MAY_BORROW | RIF_MAY_REVOKE);
}

static void sns_cm_fctx_rm_fini(struct m0_sns_cm_file_ctx *fctx)
{
	m0_file_unlock(&fctx->sf_rin);
	m0_rm_owner_lock(&fctx->sf_owner);
	m0_clink_add(&fctx->sf_owner.ro_sm.sm_chan, &fctx->sf_fini_clink);
	m0_rm_owner_unlock(&fctx->sf_owner);
	m0_rm_owner_windup(&fctx->sf_owner);
}

static bool fctx_fini_clink_cb(struct m0_clink *link)
{
	struct m0_sns_cm_file_ctx *fctx = M0_AMB(fctx, link, sf_fini_clink);
	if (fctx->sf_owner.ro_sm.sm_state == ROS_FINAL) {
		__fctx_ast_post(fctx, &fctx->sf_fini_ast);
	}

	return true;
}

M0_INTERNAL int m0_sns_cm_fctx_init(struct m0_sns_cm *scm,
				    const struct m0_fid *fid,
				    struct m0_sns_cm_file_ctx **sc_fctx)
{
	struct m0_sns_cm_file_ctx *fctx;

	M0_ENTRY();
	M0_PRE(sc_fctx != NULL || scm != NULL || fid!= NULL);
	M0_PRE(!m0_fid_eq(fid, &M0_COB_ROOT_FID) ||
	       !m0_fid_eq(fid, &M0_COB_ROOT_FID) ||
	       !m0_fid_eq(fid, &M0_MDSERVICE_SLASH_FID));
	M0_PRE(m0_mutex_is_locked(&scm->sc_file_ctx_mutex));
	M0_PRE(m0_sns_cm_fid_is_valid(scm, fid));

	M0_ALLOC_PTR(fctx);
	if (fctx == NULL)
		return M0_ERR(-ENOMEM);
	m0_fid_set(&fctx->sf_fid, fid->f_container, fid->f_key);

	fctx->sf_pd = NULL;
	fctx->sf_layout = NULL;
	fctx->sf_pi = NULL;
	fctx->sf_nr_ios_visited = 0;
	fctx->sf_max_group = 0;
	m0_scmfctx_tlink_init(fctx);
	m0_ref_init(&fctx->sf_ref, 1, sns_cm_fctx_release);
	m0_sm_init(&fctx->sf_sm, &fctx_sm_conf, M0_SCFS_INIT,
		   &scm->sc_base.cm_sm_group);
	m0_clink_init(&fctx->sf_fini_clink, fctx_fini_clink_cb);
	m0_mutex_init(&fctx->sf_lock);
	fctx->sf_group = &scm->sc_base.cm_sm_group;
	fctx->sf_scm = scm;
	if (!m0_sns_cm2reqh(scm)->rh_oostore)
		sns_cm_fctx_rm_init(fctx);
	*sc_fctx = fctx;

	return M0_RC(0);
}

M0_INTERNAL void m0_sns_cm_fctx_fini(struct m0_sns_cm_file_ctx *fctx)
{
	M0_PRE(fctx != NULL);

	m0_scmfctx_tlink_fini(fctx);

	fctx->sf_fini_ast.sa_cb = _fctx_fini;
	/* In non-oostore mode we use resource manager to acquire file lock
	 * across the nodes. We wait until file lock resource is finalised
	 * before finalising fctx.
	 * In oostore mode we do not use file lock resource.
	 */
	if (!m0_sns_cm2reqh(fctx->sf_scm)->rh_oostore)
		sns_cm_fctx_rm_fini(fctx);
	else
		__fctx_ast_post(fctx, &fctx->sf_fini_ast);
}

extern const struct m0_rm_incoming_ops file_lock_incoming_ops;
static int __sns_cm_file_lock(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_sns_cm *scm;
	M0_ENTRY();

	M0_PRE(fctx != NULL);
	M0_PRE(m0_sns_cm_fid_is_valid(fctx->sf_scm, &fctx->sf_fid));
	M0_PRE(fctx->sf_owner.ro_resource == &fctx->sf_file.fi_res);

	scm = fctx->sf_scm;
	M0_ASSERT(scm != NULL);
	M0_ASSERT(m0_sns_cm_fctx_locate(scm, &fctx->sf_fid) == NULL);
	M0_ASSERT(m0_mutex_is_locked(&scm->sc_file_ctx_mutex));

	M0_LOG(M0_DEBUG, "Lock file for FID : "FID_F, FID_P(&fctx->sf_fid));
	_fctx_status_set(fctx, M0_SCFS_LOCK_WAIT);
	m0_scmfctx_htable_add(&scm->sc_file_ctx, fctx);
	/* XXX: Ideally m0_file_lock should be called, but
	 * it internally calls m0_rm_incoming_init(). This has already been
	 * called here in m0_sns_cm_file_lock_init().
	 * m0_file_lock(&fctx->sf_owner, &fctx->sf_rin);
	 */
	fctx->sf_rin.rin_want.cr_datum    = RM_FILE_LOCK;
	fctx->sf_rin.rin_want.cr_group_id = m0_rm_sns_cm_group;
	fctx->sf_rin.rin_ops              = &file_lock_incoming_ops;

	m0_rm_credit_get(&fctx->sf_rin);

	M0_POST(m0_sns_cm_fctx_locate(scm, &fctx->sf_fid) == fctx);
	return M0_RC(-EAGAIN);
}

static int __sns_cm_file_oo_lock(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_sns_cm *scm;
	M0_ENTRY();

	M0_PRE(fctx != NULL);
	M0_PRE(m0_sns_cm_fid_is_valid(fctx->sf_scm, &fctx->sf_fid));

	scm = fctx->sf_scm;
	M0_ASSERT(scm != NULL);
	M0_ASSERT(m0_sns_cm_fctx_locate(scm, &fctx->sf_fid) == NULL);
	M0_ASSERT(m0_mutex_is_locked(&scm->sc_file_ctx_mutex));

	M0_LOG(M0_DEBUG, "Lock file for FID : "FID_F, FID_P(&fctx->sf_fid));
	m0_scmfctx_htable_add(&scm->sc_file_ctx, fctx);
	_fctx_status_set(fctx, M0_SCFS_LOCKED);

	M0_POST(m0_sns_cm_fctx_locate(scm, &fctx->sf_fid) == fctx);
	return M0_RC(0);
}

static void __sns_cm_file_unlock(struct m0_sns_cm_file_ctx *fctx)
{
	M0_PRE(fctx != NULL);
	M0_PRE(m0_sns_cm_fid_is_valid(fctx->sf_scm, &fctx->sf_fid));
	M0_PRE(ergo(!m0_sns_cm2reqh(fctx->sf_scm)->rh_oostore,
		    fctx->sf_owner.ro_resource == &fctx->sf_file.fi_res));
	M0_PRE(m0_mutex_is_locked(&fctx->sf_scm->sc_file_ctx_mutex));

	m0_ref_put(&fctx->sf_ref);
	M0_LOG(M0_DEBUG, "Unlock file for FID : "FID_F, FID_P(&fctx->sf_fid));
}

M0_INTERNAL int
m0_sns_cm_file_lock_wait(struct m0_sns_cm_file_ctx *fctx, struct m0_fom *fom)
{
	struct m0_chan *rm_chan;
	uint32_t        state;

	M0_ENTRY();

	M0_PRE(fctx != NULL && fctx->sf_scm != NULL && fom != NULL);
	M0_PRE(m0_cm_is_locked(&fctx->sf_scm->sc_base));
	M0_PRE(m0_mutex_is_locked(&fctx->sf_scm->sc_file_ctx_mutex));
	M0_PRE(m0_sns_cm_fid_is_valid(fctx->sf_scm, &fctx->sf_fid));
	M0_PRE(m0_sns_cm_fctx_locate(fctx->sf_scm, &fctx->sf_fid) != NULL);

	m0_rm_owner_lock(&fctx->sf_owner);
	state = fctx->sf_rin.rin_sm.sm_state;
	if (state == RI_SUCCESS ||  state == RI_FAILURE) {
		m0_rm_owner_unlock(&fctx->sf_owner);
		if (state == RI_FAILURE) {
			m0_ref_put(&fctx->sf_ref);
			return M0_ERR_INFO(-EFAULT,
					   "Failed to lock file "FID_F,
					   FID_P(&fctx->sf_fid));
		} else {
			if (m0_sns_cm_fctx_state_get(fctx) == M0_SCFS_LOCK_WAIT)
				_fctx_status_set(fctx, M0_SCFS_LOCKED);
			return M0_RC(0);
		}
	}
	rm_chan = &fctx->sf_rin.rin_sm.sm_chan;
	m0_fom_wait_on(fom, rm_chan, &fom->fo_cb);
	m0_rm_owner_unlock(&fctx->sf_owner);
	return M0_RC(-EAGAIN);
}

M0_INTERNAL int m0_sns_cm_file_lock(struct m0_sns_cm *scm,
				    const struct m0_fid *fid,
				    struct m0_sns_cm_file_ctx **out)
{
	struct m0_sns_cm_file_ctx *fctx;
	struct m0_fid              f = *fid;
	int                        rc;

	M0_ENTRY();

	M0_PRE(scm != NULL || fid != NULL);
	M0_PRE(m0_cm_is_locked(&scm->sc_base));
	M0_PRE(m0_sns_cm_fid_is_valid(scm, fid));
	M0_PRE(m0_mutex_is_locked(&scm->sc_file_ctx_mutex));

	fctx = m0_scmfctx_htable_lookup(&scm->sc_file_ctx, &f);
	if ( fctx != NULL) {
		*out = fctx;
		m0_ref_get(&fctx->sf_ref);
		M0_LOG(M0_DEBUG, "fid: "FID_F, FID_P(fid));
		if (m0_sns_cm_fctx_state_get(fctx) >= M0_SCFS_LOCKED) {
			return M0_RC(0);
		}
		if (m0_sns_cm_fctx_state_get(fctx) == M0_SCFS_LOCK_WAIT)
			return M0_RC(-EAGAIN);
	}
	rc = m0_sns_cm_fctx_init(scm, fid, &fctx);
	if (rc == 0) {
		rc = !m0_sns_cm2reqh(scm)->rh_oostore ?
			__sns_cm_file_lock(fctx) :
			__sns_cm_file_oo_lock(fctx);
		*out = fctx;
	}
	return M0_RC(rc);
}

M0_INTERNAL struct m0_sns_cm_file_ctx *
m0_sns_cm_fctx_locate(struct m0_sns_cm *scm, struct m0_fid *fid)
{
	struct m0_sns_cm_file_ctx *fctx;

	M0_ENTRY("sns cm %p, fid %p="FID_F, scm, fid, FID_P(fid));
	M0_PRE(scm != NULL && fid != NULL);
	M0_PRE(m0_mutex_is_locked(&scm->sc_file_ctx_mutex));

	fctx = m0_scmfctx_htable_lookup(&scm->sc_file_ctx, fid);
	M0_ASSERT(ergo(fctx != NULL, m0_fid_cmp(fid, &fctx->sf_fid) == 0));

	M0_LEAVE();
	return fctx;
}

M0_INTERNAL struct m0_sns_cm_file_ctx *
m0_sns_cm_fctx_get(struct m0_sns_cm *scm, const struct m0_cm_ag_id *id)
{
	struct m0_fid             *fid;
	struct m0_sns_cm_file_ctx *fctx;

	if (M0_FI_ENABLED("do_nothing"))
		return NULL;

	M0_PRE(scm != NULL && id != NULL);

	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	fid = (struct m0_fid *)&id->ai_hi;
	M0_ASSERT(m0_sns_cm_fid_is_valid(scm, fid));
	fctx = m0_sns_cm_fctx_locate(scm, fid);
	M0_ASSERT(fctx != NULL);
	M0_CNT_INC(fctx->sf_ag_nr);
	M0_LOG(M0_DEBUG, "ag nr: %lu, FID :"FID_F, fctx->sf_ag_nr,
			FID_P(fid));
	m0_ref_get(&fctx->sf_ref);
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);

	return fctx;
}

M0_INTERNAL void m0_sns_cm_fctx_put(struct m0_sns_cm *scm,
				    const struct m0_cm_ag_id *id)
{
	struct m0_fid              *fid;
	struct m0_sns_cm_file_ctx  *fctx;

	if (M0_FI_ENABLED("do_nothing"))
		return;

	M0_PRE(scm != NULL && id != NULL);

	fid = (struct m0_fid *)&id->ai_hi;
	m0_mutex_lock(&scm->sc_file_ctx_mutex);
	M0_ASSERT(m0_sns_cm_fid_is_valid(scm, fid));
	fctx = m0_sns_cm_fctx_locate(scm, fid);
	M0_ASSERT(fctx != NULL);
	M0_CNT_DEC(fctx->sf_ag_nr);
	M0_LOG(M0_DEBUG, "ag nr: %lu, FID : <%lx : %lx>", fctx->sf_ag_nr,
			FID_P(fid));
	m0_ref_put(&fctx->sf_ref);
	m0_mutex_unlock(&scm->sc_file_ctx_mutex);
}

M0_INTERNAL void m0_sns_cm_file_unlock(struct m0_sns_cm *scm,
				       struct m0_fid *fid)
{
	struct m0_sns_cm_file_ctx *fctx;

	if (M0_FI_ENABLED("do_nothing"))
		return;

	M0_PRE(scm != NULL && fid != NULL);
	M0_PRE(m0_sns_cm_fid_is_valid(scm, fid));
	M0_PRE(m0_mutex_is_locked(&scm->sc_file_ctx_mutex));

	fctx = m0_sns_cm_fctx_locate(scm, fid);
	M0_ASSERT(fctx != NULL);
	M0_LOG(M0_DEBUG, "File with FID : "FID_F" has %lu AGs",
			FID_P(&fctx->sf_fid), fctx->sf_ag_nr);
	__sns_cm_file_unlock(fctx);
}

static int _attr_fetch(struct m0_sns_cm_file_ctx *fctx);

static uint64_t max_frame(const struct m0_sns_cm_file_ctx *fctx,
                          size_t max_cob_size)
{
	uint64_t                  frame_size;
	uint64_t                  frame;
	struct m0_pdclust_layout *pl;

	pl = m0_layout_to_pdl(fctx->sf_layout);
	frame_size = pl->pl_attr.pa_unit_size;
	if (max_cob_size < frame_size)
		frame = 0;
	else {
		frame = max_cob_size % frame_size ?
			max_cob_size / frame_size + 1 :
			max_cob_size / frame_size;
	}

	return frame;
}

static void _max_group_set(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_pdclust_src_addr sa;
	struct m0_pdclust_tgt_addr ta;
	uint64_t                   cob_size;

	cob_size = fctx->sf_attr.ca_size;
	ta.ta_frame = max_frame(fctx, cob_size);
	ta.ta_obj = fctx->sf_pd->pd_index;
	m0_fd_bwd_map(fctx->sf_pi, &ta, &sa);
	if (fctx->sf_max_group < sa.sa_group)
		fctx->sf_max_group = sa.sa_group;
}

static void _attr_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_sns_cm_file_ctx *fctx = _AST2FCTX(ast, sf_attr_ast);

	fctx->sf_rc = (long)ast->sa_datum;
	if (fctx->sf_rc == 0) {
		_layout_fetch(fctx);
		_fid_layout_instance(fctx);
		_max_group_set(fctx);
	}
	_attr_fetch(fctx);
}

static inline void _attr_cb(void *arg, int rc)
{
	struct m0_sns_cm_file_ctx *fctx = arg;

	fctx->sf_attr_ast.sa_cb = _attr_ast_cb;
	M0_LOG(M0_DEBUG, "rc:%d %"PRIx64" %d", rc, fctx->sf_attr.ca_size,
					       (int)fctx->sf_nr_ios_visited);

	/*
	 * We save file attribute fetch result temporarily and update
	 * m0_sns_cm_file_ctx::sf_rc in ast callback under copy machine lock
	 * in-order to avoid the race between update and reading of
	 * m0_sns_cm_file_ctx::sf_rc.
	 */
	fctx->sf_attr_ast.sa_datum = (void *)(long)rc;
	__fctx_ast_post(fctx, &fctx->sf_attr_ast);
}

static int _ios_failed_cob_attr(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_poolmach *pm;
	struct m0_pool     *pool;
	struct m0_pooldev  *pd;
	int                 rc = fctx->sf_rc;

	M0_PRE(fctx->sf_pm != NULL);

	pm = fctx->sf_pm;
	pool = pm->pm_pver->pv_pool;
	pd = fctx->sf_pd;

	if (pd == NULL)
		pd = pool_failed_devs_tlist_head(&pool->po_failed_devices);
	else
		pd = pool_failed_devs_tlist_next(&pool->po_failed_devices, pd);

	if (pd != NULL) {
		fctx->sf_pd = pd;
		rc = m0_ios_cob_getattr_async(&fctx->sf_fid,
					      &fctx->sf_attr, pd->pd_index,
					      pm->pm_pver, &_attr_cb, fctx);
		rc = rc == -ENOMEM ? rc : -EAGAIN;
	}

	return M0_RC(rc);
}

static int _attr_fetch(struct m0_sns_cm_file_ctx *fctx)
{
	M0_PRE(m0_sns_cm_fctx_state_get(fctx) == M0_SCFS_ATTR_FETCH);


	fctx->sf_rc = _ios_failed_cob_attr(fctx);
	/* Transition to M0_SCFS_ATTR_FETCHED in case of success or error. */
	if (fctx->sf_rc != -EAGAIN)
		_fctx_status_set(fctx, M0_SCFS_ATTR_FETCHED);

	return M0_RC(fctx->sf_rc);
}

static int _layout_fetch(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_cm            *cm = &fctx->sf_scm->sc_base;
	struct m0_reqh          *reqh = cm->cm_service.rs_reqh;
	struct m0_layout_domain *ldom = &reqh->rh_ldom;
	struct m0_layout        *l;
	uint64_t                 layout_id;

	M0_PRE(M0_IN(m0_sns_cm_fctx_state_get(fctx), (M0_SCFS_ATTR_FETCH,
						      M0_SCFS_LAYOUT_FETCH)));

	layout_id = m0_pool_version2layout_id(&fctx->sf_attr.ca_pver,
					      fctx->sf_attr.ca_lid);
	l = fctx->sf_layout ?: m0_layout_find(ldom, layout_id);
	if (l == NULL)
		return M0_RC(-ENOENT);
	fctx->sf_layout = l;
	return M0_RC(0);
}

static int _fid_layout_instance(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_layout_instance *li;
	int                        rc;

	rc = m0_layout_instance_build(fctx->sf_layout, &fctx->sf_fid, &li);
	if (rc == 0)
		fctx->sf_pi = m0_layout_instance_to_pdi(li);

	return M0_RC(rc);
}

static struct m0_poolmach *
sns_cm_poolmach_get(struct m0_sns_cm_file_ctx *fctx)
{
	struct m0_reqh         *reqh;
	struct m0_pool_version *pv;
	M0_PRE(fctx != NULL);

	reqh = m0_sns_cm2reqh(fctx->sf_scm);
	pv = m0_pool_version_find(reqh->rh_pools, &fctx->sf_attr.ca_pver);

	if (pv != NULL) {
		return &pv->pv_mach;
	} else {
		M0_LOG(M0_ERROR, "Cannot find pool version for fid="FID_F
				 " pver="FID_F,
				 FID_P(&fctx->sf_fid),
				 FID_P(&fctx->sf_attr.ca_pver));
		return NULL;
	}
}

M0_INTERNAL struct m0_pool_version *
m0_sns_cm_pool_version_get(struct m0_sns_cm_file_ctx *fctx)
{
	M0_PRE(fctx != NULL && fctx->sf_pm != NULL);

	return fctx->sf_pm->pm_pver;
}

M0_INTERNAL int
m0_sns_cm_file_attr_and_layout(struct m0_sns_cm_file_ctx *fctx)
{
	int rc = 0;

	M0_PRE(m0_cm_is_locked(&fctx->sf_scm->sc_base));
	M0_PRE(M0_IN(m0_sns_cm_fctx_state_get(fctx), (M0_SCFS_LOCKED,
						   M0_SCFS_ATTR_FETCH,
						   M0_SCFS_ATTR_FETCHED,
						   M0_SCFS_LAYOUT_FETCH,
						   M0_SCFS_LAYOUT_FETCHED)));

	if (M0_FI_ENABLED("ut_attr_layout")) {
		rc = m0_sns_cm_ut_file_size_layout(fctx);
		if (rc != 0)
			return M0_RC(rc);
		_fctx_status_set(fctx, M0_SCFS_LAYOUT_FETCHED);
		rc = _fid_layout_instance(fctx);
		return M0_RC(rc);
	}

	switch (m0_sns_cm_fctx_state_get(fctx)) {
	case M0_SCFS_LOCKED :
		_fctx_status_set(fctx, M0_SCFS_ATTR_FETCH);
		rc = _attr_fetch(fctx);
		if (rc == -EAGAIN)
			return M0_RC(rc);
	case M0_SCFS_ATTR_FETCHED :
		if (fctx->sf_rc != 0)
			return M0_ERR_INFO(fctx->sf_rc, FID_F,
					   FID_P(&fctx->sf_fid));

		/* populate fctx->sf_pm here */
		fctx->sf_pm = sns_cm_poolmach_get(fctx);
		if (fctx->sf_pm == NULL) {
			fctx->sf_rc = -ENOENT;
			return M0_ERR(fctx->sf_rc);
		}
		break;
	case M0_SCFS_ATTR_FETCH :
	case M0_SCFS_LAYOUT_FETCH :
		rc = -EAGAIN;
		break;
	default :
		rc = -EINVAL;
	}

	return M0_RC(rc);
}

M0_INTERNAL void
m0_sns_cm_file_attr_and_layout_wait(struct m0_sns_cm_file_ctx *fctx,
				    struct m0_fom *fom)
{
	m0_fom_wait_on(fom, &fctx->sf_sm.sm_chan, &fom->fo_cb);
}

M0_INTERNAL void m0_sns_cm_file_fwd_map(struct m0_sns_cm_file_ctx *fctx,
					const struct m0_pdclust_src_addr *sa,
					struct m0_pdclust_tgt_addr *ta)
{
	m0_sns_cm_fctx_lock(fctx);
	m0_fd_fwd_map(fctx->sf_pi, sa, ta);
	m0_sns_cm_fctx_unlock(fctx);
}

M0_INTERNAL void m0_sns_cm_file_bwd_map(struct m0_sns_cm_file_ctx *fctx,
					const struct m0_pdclust_tgt_addr *ta,
					struct m0_pdclust_src_addr *sa)
{
	m0_sns_cm_fctx_lock(fctx);
	m0_fd_bwd_map(fctx->sf_pi, ta, sa);
	m0_sns_cm_fctx_unlock(fctx);
}

M0_INTERNAL uint64_t m0_sns_cm_file_data_units(struct m0_sns_cm_file_ctx *fctx)
{
	size_t                    fsize;
	uint64_t                  nr_total_du = 0;
	struct m0_pdclust_layout *pl = m0_layout_to_pdl(fctx->sf_layout);

	M0_PRE(fctx != NULL);
	M0_ENTRY();

	fsize = fctx->sf_attr.ca_size;
	nr_total_du = fsize / m0_pdclust_unit_size(pl);
	if (fsize % m0_pdclust_unit_size(pl) > 0)
		M0_CNT_INC(nr_total_du);

	M0_LEAVE();
	return nr_total_du;
}

M0_INTERNAL bool m0_sns_cm_file_unit_is_EOF(struct m0_pdclust_layout *pl,
					    uint64_t nr_max_data_units,
					    uint64_t group, uint32_t unit)
{
	uint32_t d;

	M0_PRE(pl != NULL);

	if (m0_pdclust_unit_classify(pl, unit) == M0_PUT_DATA) {
		d = m0_sns_cm_ag_nr_data_units(pl);
		return (group * d + unit + 1) > nr_max_data_units;
	}

	return false;
}

#undef _AST2FCTX

/** @} SNSCMFILE */
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
