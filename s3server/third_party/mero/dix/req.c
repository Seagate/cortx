/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 15-Aug-2016
 */


/**
 * @addtogroup dix
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/buf.h"
#include "lib/vec.h"
#include "lib/finject.h"
#include "conf/schema.h" /* M0_CST_CAS */
#include "sm/sm.h"
#include "pool/pool.h"   /* m0_pool_version_find */
#include "cas/client.h"
#include "cas/cas.h"     /* m0_dix_fid_type */
#include "dix/layout.h"
#include "dix/meta.h"
#include "dix/req.h"
#include "dix/client.h"
#include "dix/client_internal.h" /* m0_dix_pver */
#include "dix/fid_convert.h"
#include "dix/dix_addb.h"

static struct m0_sm_state_descr dix_req_states[] = {
	[DIXREQ_INIT] = {
		.sd_flags     = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(DIXREQ_DISCOVERY_DONE,
					DIXREQ_FAILURE,
					DIXREQ_LAYOUT_DISCOVERY,
					DIXREQ_LID_DISCOVERY)
	},
	[DIXREQ_LAYOUT_DISCOVERY] = {
		.sd_name      = "layout-discovery",
		.sd_allowed   = M0_BITS(DIXREQ_DISCOVERY_DONE,
					DIXREQ_LID_DISCOVERY,
					DIXREQ_FINAL,
					DIXREQ_FAILURE)
	},
	[DIXREQ_LID_DISCOVERY] = {
		.sd_name      = "lid-discovery",
		.sd_allowed   = M0_BITS(DIXREQ_DISCOVERY_DONE,
					DIXREQ_FINAL,
					DIXREQ_FAILURE)
	},
	[DIXREQ_DISCOVERY_DONE] = {
		.sd_name      = "discovery-done",
		.sd_allowed   = M0_BITS(DIXREQ_INPROGRESS,
					DIXREQ_META_UPDATE,
					DIXREQ_FAILURE)
	},
	[DIXREQ_META_UPDATE] = {
		.sd_name      = "idxop-meta-update",
		.sd_allowed   = M0_BITS(DIXREQ_INPROGRESS, DIXREQ_FINAL,
					DIXREQ_FAILURE)
	},
	[DIXREQ_INPROGRESS] = {
		.sd_name      = "in-progress",
		.sd_allowed   = M0_BITS(DIXREQ_GET_RESEND, DIXREQ_FINAL,
					DIXREQ_DEL_PHASE2, DIXREQ_FAILURE)
	},
	[DIXREQ_GET_RESEND] = {
		.sd_name      = "resend-get-req",
		.sd_allowed   = M0_BITS(DIXREQ_INPROGRESS, DIXREQ_FAILURE)
	},
	[DIXREQ_DEL_PHASE2] = {
		.sd_name      = "delete-phase2",
		.sd_allowed   = M0_BITS(DIXREQ_FINAL, DIXREQ_FAILURE)
	},
	[DIXREQ_FINAL] = {
		.sd_name      = "final",
		.sd_flags     = M0_SDF_TERMINAL,
	},
	[DIXREQ_FAILURE] = {
		.sd_name      = "failure",
		.sd_flags     = M0_SDF_TERMINAL | M0_SDF_FAILURE
	}
};

/** @todo Check it. */
static struct m0_sm_trans_descr dix_req_trans[] = {
	{ "layouts-known", DIXREQ_INIT,             DIXREQ_DISCOVERY_DONE   },
	{ "find-layouts",  DIXREQ_INIT,             DIXREQ_LAYOUT_DISCOVERY },
	{ "resolve-lids",  DIXREQ_INIT,             DIXREQ_LID_DISCOVERY    },
	{ "copy-fail",     DIXREQ_INIT,             DIXREQ_FAILURE          },
	{ "layouts-found", DIXREQ_LAYOUT_DISCOVERY, DIXREQ_DISCOVERY_DONE   },
	{ "lid-exists",    DIXREQ_LAYOUT_DISCOVERY, DIXREQ_LID_DISCOVERY    },
	{ "not-found",     DIXREQ_LAYOUT_DISCOVERY, DIXREQ_FAILURE          },
	{ "desc-final",    DIXREQ_LAYOUT_DISCOVERY, DIXREQ_FINAL            },
	{ "lids-resolved", DIXREQ_LID_DISCOVERY,    DIXREQ_DISCOVERY_DONE   },
	{ "not-resolved",  DIXREQ_LID_DISCOVERY,    DIXREQ_FAILURE          },
	{ "lid-final",     DIXREQ_LID_DISCOVERY,    DIXREQ_FINAL            },
	{ "create/delete", DIXREQ_DISCOVERY_DONE,   DIXREQ_META_UPDATE      },
	{ "execute",       DIXREQ_DISCOVERY_DONE,   DIXREQ_INPROGRESS       },
	{ "failure",       DIXREQ_DISCOVERY_DONE,   DIXREQ_FAILURE          },
	{ "meta-updated",  DIXREQ_META_UPDATE,      DIXREQ_INPROGRESS       },
	{ "crow-or-fail",  DIXREQ_META_UPDATE,      DIXREQ_FINAL            },
	{ "update-fail",   DIXREQ_META_UPDATE,      DIXREQ_FAILURE          },
	{ "get-req-fail",  DIXREQ_INPROGRESS,       DIXREQ_GET_RESEND       },
	{ "rpc-failure",   DIXREQ_INPROGRESS,       DIXREQ_FAILURE          },
	{ "req-processed", DIXREQ_INPROGRESS,       DIXREQ_FINAL            },
	{ "inp-del-ph2",   DIXREQ_INPROGRESS,       DIXREQ_DEL_PHASE2       },
	{ "del-ph2-final", DIXREQ_DEL_PHASE2,       DIXREQ_FINAL            },
	{ "del-ph2-fail",  DIXREQ_DEL_PHASE2,       DIXREQ_FAILURE          },
	{ "all-cctg-fail", DIXREQ_GET_RESEND,       DIXREQ_FAILURE          },
	{ "resend",        DIXREQ_GET_RESEND,       DIXREQ_INPROGRESS       },
};

struct m0_sm_conf dix_req_sm_conf = {
	.scf_name      = "dix_req",
	.scf_nr_states = ARRAY_SIZE(dix_req_states),
	.scf_state     = dix_req_states,
	.scf_trans_nr  = ARRAY_SIZE(dix_req_trans),
	.scf_trans     = dix_req_trans
};

M0_TL_DESCR_DEFINE(cas_rop, "cas record operations",
		   M0_INTERNAL, struct m0_dix_cas_rop, crp_linkage, crp_magix,
		   M0_DIX_ROP_MAGIC, M0_DIX_ROP_HEAD_MAGIC);
M0_TL_DEFINE(cas_rop, M0_INTERNAL, struct m0_dix_cas_rop);

static void dix_idxop(struct m0_dix_req *req);
static void dix_rop(struct m0_dix_req *req);
static void dix_rop_units_set(struct m0_dix_req *req);
static int dix_cas_rops_alloc(struct m0_dix_req *req);
static int dix_cas_rops_fill(struct m0_dix_req *req);
static int dix_cas_rops_send(struct m0_dix_req *req);
static void dix_ldescr_resolve(struct m0_dix_req *req);
static void dix_discovery_completed(struct m0_dix_req *req);
static int dix_idxop_reqs_send(struct m0_dix_req *req);
static void dix_discovery(struct m0_dix_req *req);

static int dix_id_layouts_nr(struct m0_dix_req *req);
static int dix_unknown_layouts_nr(struct m0_dix_req *req);
static uint32_t dix_rop_tgt_iter_max(struct m0_dix_req    *req,
				     struct m0_dix_rec_op *rec_op);


static bool dix_req_is_idxop(const struct m0_dix_req *req)
{
	return M0_IN(req->dr_type, (DIX_CREATE, DIX_DELETE, DIX_CCTGS_LOOKUP));
}

static struct m0_sm_group *dix_req_smgrp(const struct m0_dix_req *req)
{
	return req->dr_sm.sm_grp;
}

static void dix_to_cas_map(const struct m0_dix_req *dreq,
			   const struct m0_cas_req *creq)
{
	uint64_t did = m0_sm_id_get(&dreq->dr_sm);
	uint64_t cid = m0_sm_id_get(&creq->ccr_sm);
	M0_ADDB2_ADD(M0_AVI_DIX_TO_CAS, did, cid);
}


M0_INTERNAL void m0_dix_req_lock(struct m0_dix_req *req)
{
	M0_ENTRY();
	m0_sm_group_lock(dix_req_smgrp(req));
}

M0_INTERNAL void m0_dix_req_unlock(struct m0_dix_req *req)
{
	M0_ENTRY();
	m0_sm_group_unlock(dix_req_smgrp(req));
}

M0_INTERNAL bool m0_dix_req_is_locked(const struct m0_dix_req *req)
{
	return m0_sm_group_is_locked(dix_req_smgrp(req));
}

M0_INTERNAL int m0_dix_req_wait(struct m0_dix_req *req, uint64_t states,
				m0_time_t to)
{
	M0_ENTRY();
	M0_PRE(m0_dix_req_is_locked(req));
	return M0_RC(m0_sm_timedwait(&req->dr_sm, states, to));
}

static void dix_req_init(struct m0_dix_req  *req,
			 struct m0_dix_cli  *cli,
			 struct m0_sm_group *grp,
			 bool                meta)
{
	M0_SET0(req);
	req->dr_cli = cli;
	req->dr_is_meta = meta;
	m0_sm_init(&req->dr_sm, &dix_req_sm_conf, DIXREQ_INIT, grp);
	m0_sm_addb2_counter_init(&req->dr_sm);
}

M0_INTERNAL void m0_dix_mreq_init(struct m0_dix_req  *req,
				  struct m0_dix_cli  *cli,
				  struct m0_sm_group *grp)
{
	dix_req_init(req, cli, grp, true);
}

M0_INTERNAL void m0_dix_req_init(struct m0_dix_req  *req,
				 struct m0_dix_cli  *cli,
				 struct m0_sm_group *grp)
{
	dix_req_init(req, cli, grp, false);
}

static enum m0_dix_req_state dix_req_state(const struct m0_dix_req *req)
{
	return req->dr_sm.sm_state;
}

static void dix_req_state_set(struct m0_dix_req     *req,
			      enum m0_dix_req_state  state)
{
	M0_LOG(M0_DEBUG, "DIX req: %p, state change:[%s -> %s]\n",
	       req, m0_sm_state_name(&req->dr_sm, req->dr_sm.sm_state),
	       m0_sm_state_name(&req->dr_sm, state));
	m0_sm_state_set(&req->dr_sm, state);
}

static void dix_req_failure(struct m0_dix_req *req, int32_t rc)
{
	M0_PRE(rc != 0);
	m0_sm_fail(&req->dr_sm, DIXREQ_FAILURE, rc);
}

static int dix_type_layouts_nr(struct m0_dix_req    *req,
			       enum dix_layout_type  type)
{
	return m0_count(i, req->dr_indices_nr,
		req->dr_indices[i].dd_layout.dl_type == type);
}

static int dix_resolved_nr(struct m0_dix_req *req)
{
	return dix_type_layouts_nr(req, DIX_LTYPE_DESCR);
}

static int dix_id_layouts_nr(struct m0_dix_req *req)
{
	return dix_type_layouts_nr(req, DIX_LTYPE_ID);
}

static int dix_unknown_layouts_nr(struct m0_dix_req *req)
{
	return dix_type_layouts_nr(req, DIX_LTYPE_UNKNOWN);
}

static void dix_to_mdix_map(const struct m0_dix_req *req,
			    const struct m0_dix_meta_req *mreq)
{
	uint64_t rid = m0_sm_id_get(&req->dr_sm);
	uint64_t mid = m0_sm_id_get(&mreq->dmr_req.dr_sm);
	M0_ADDB2_ADD(M0_AVI_DIX_TO_MDIX, rid, mid);
}

static void dix_layout_find_ast_cb(struct m0_sm_group *grp,
				   struct m0_sm_ast   *ast)
{
	struct m0_dix_req      *req = ast->sa_datum;
	struct m0_dix_meta_req *meta_req = req->dr_meta_req;
	struct m0_dix_ldesc    *ldesc;
	enum m0_dix_req_state   state = dix_req_state(req);
	bool                    idx_op = dix_req_is_idxop(req);
	uint32_t                i;
	uint32_t                k;
	int                     rc;
	int                     rc2;

	M0_ENTRY("req %p", req);
	M0_PRE(M0_IN(state, (DIXREQ_LAYOUT_DISCOVERY, DIXREQ_LID_DISCOVERY)));
	rc = m0_dix_meta_generic_rc(meta_req);
	if (rc == 0) {
		M0_ASSERT(ergo(!idx_op, m0_dix_meta_req_nr(meta_req) == 1));
		for (i = 0, k = 0; rc == 0 && i < req->dr_indices_nr; i++) {
			switch(req->dr_indices[i].dd_layout.dl_type) {
			case DIX_LTYPE_UNKNOWN:
				M0_ASSERT(state == DIXREQ_LAYOUT_DISCOVERY);
				rc2 = m0_dix_layout_rep_get(meta_req, k,
					      &req->dr_indices[k].dd_layout);
				break;
			case DIX_LTYPE_ID:
				M0_ASSERT(state == DIXREQ_LID_DISCOVERY);
				ldesc = &req->dr_indices[k].dd_layout.u.dl_desc;
				rc2 = m0_dix_ldescr_rep_get(meta_req, k, ldesc);
				break;
			default:
				/*
				 * Note, that CAS requests are not sent for
				 * layouts with DIX_LTYPE_DESCR.
				 */
				M0_IMPOSSIBLE("Impossible layout type %d",
				      req->dr_indices[i].dd_layout.dl_type);
				break;
			}
			if (rc2 != 0) {
				/*
				 * Treat getting layout error as a fatal error
				 * for record operation, since the request is
				 * executed against only one index.
				 */
				if (!idx_op)
					rc = rc2;
				else
					req->dr_items[k].dxi_rc = rc2;
			}
			k++;
		}
		/* All replies for the meta request should be used. */
		M0_ASSERT(k == m0_dix_meta_req_nr(meta_req));
	}
	m0_dix_meta_req_fini(meta_req);
	m0_free0(&req->dr_meta_req);

	if (rc == 0) {
		/*
		 * Stop request processing if there are no items which can
		 * potentially succeed.
		 */
		if (!m0_exists(i, req->dr_items_nr,
			       req->dr_items[i].dxi_rc == 0)) {
			dix_req_state_set(req, DIXREQ_FINAL);
		/*
		 * If there are layouts identified by id then it's
		 * necessary to resolve id to layout descriptors. Check for
		 * state to avoid resolving layout ids in a loop.
		 */
		} else if (dix_id_layouts_nr(req) > 0 &&
			   dix_req_state(req) != DIXREQ_LID_DISCOVERY) {
			dix_ldescr_resolve(req);
		} else {
			/*
			 * All (or at least some) layout descriptors are
			 * obtained successfully.
			 */
			dix_discovery_completed(req);
		}
	} else {
		dix_req_failure(req, rc);
	}
}

static bool dix_layout_find_clink_cb(struct m0_clink *cl)
{
	struct m0_dix_req *req = M0_AMB(req, cl, dr_clink);

	m0_clink_del(cl);
	m0_clink_fini(cl);
	req->dr_ast.sa_cb = dix_layout_find_ast_cb;
	req->dr_ast.sa_datum = req;
	m0_sm_ast_post(dix_req_smgrp(req), &req->dr_ast);
	return true;
}

static void dix_layout_find(struct m0_dix_req *req)
{
	struct m0_dix_meta_req *meta_req;
	struct m0_fid          *fids;
	struct m0_dix          *indices;
	uint32_t                i;
	uint32_t                k;
	uint32_t                unknown_nr;
	int                     rc;

	indices = req->dr_indices;
	unknown_nr = dix_unknown_layouts_nr(req);

	M0_PRE(unknown_nr > 0);
	M0_ALLOC_PTR(req->dr_meta_req);
	M0_ALLOC_ARR(fids, unknown_nr);
	if (fids == NULL || req->dr_meta_req == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto err;
	}
	meta_req = req->dr_meta_req;
	for (i = 0, k = 0; i < req->dr_indices_nr; i++)
		if (indices[i].dd_layout.dl_type == DIX_LTYPE_UNKNOWN)
			fids[k++] = req->dr_indices[i].dd_fid;
	m0_dix_meta_req_init(meta_req, req->dr_cli, dix_req_smgrp(req));
	dix_to_mdix_map(req, meta_req);
	m0_clink_init(&req->dr_clink, dix_layout_find_clink_cb);
	m0_clink_add_lock(&meta_req->dmr_chan, &req->dr_clink);
	/* Start loading layouts from CAS. */
	rc = m0_dix_layout_get(meta_req, fids, unknown_nr);
	if (rc != 0) {
		m0_clink_del_lock(&req->dr_clink);
		m0_clink_fini(&req->dr_clink);
		m0_dix_meta_req_fini(meta_req);
	}

err:
	if (rc != 0) {
		m0_free0(&req->dr_meta_req);
		dix_req_failure(req, rc);
	} else {
		dix_req_state_set(req, DIXREQ_LAYOUT_DISCOVERY);
	}
	m0_free(fids);
}

static int dix_indices_copy(struct m0_dix       **dst_indices,
			    const struct m0_dix  *src_indices,
			    uint32_t              indices_nr)
{
	struct m0_dix *dst;
	uint32_t       i;
	int            rc = 0;

	M0_PRE(dst_indices != NULL);
	M0_PRE(src_indices != NULL);
	M0_PRE(indices_nr != 0);
	M0_ALLOC_ARR(dst, indices_nr);
	if (dst == NULL)
		return M0_ERR(-ENOMEM);
	for (i = 0; i < indices_nr; i++) {
		rc = m0_dix_copy(&dst[i], &src_indices[i]);
		if (rc != 0)
			break;
	}
	if (rc != 0) {
		for (i = 0; i < indices_nr; i++)
			m0_dix_fini(&dst[i]);
		m0_free(dst);
		return M0_ERR(rc);
	}
	*dst_indices = dst;
	return 0;
}

static int dix_req_indices_copy(struct m0_dix_req   *req,
				const struct m0_dix *indices,
				uint32_t             indices_nr)
{
	int rc;

	M0_PRE(indices != NULL);
	M0_PRE(indices_nr != 0);
	rc = dix_indices_copy(&req->dr_indices, indices, indices_nr);
	if (rc == 0)
		req->dr_indices_nr = indices_nr;
	return rc;
}

static struct m0_pool_version *dix_pver_find(const struct m0_dix_req *req,
					     const struct m0_fid     *pver_fid)
{
	return m0_pool_version_find(req->dr_cli->dx_pc, pver_fid);
}

static void dix_idxop_ctx_free(struct m0_dix_idxop_ctx *idxop)
{
	uint32_t i;

	for (i = 0; i < idxop->dcd_idxop_reqs_nr; i++)
		m0_free(idxop->dcd_idxop_reqs[i].dcr_creqs);
	m0_free0(&idxop->dcd_idxop_reqs);
}

static void dix_idxop_item_rc_update(struct m0_dix_item          *ditem,
				     struct m0_dix_req           *req,
				     const struct m0_dix_cas_req *creq)
{
	struct m0_cas_rec_reply  crep;
	const struct m0_cas_req *cas_req;
	int                      rc;

	if (ditem->dxi_rc == 0) {
		cas_req = &creq->ds_creq;
		rc = creq->ds_rc ?:
		     m0_cas_req_generic_rc(cas_req);
		if (rc == 0) {
			switch (req->dr_type) {
			case DIX_CREATE:
				m0_cas_index_create_rep(cas_req, 0, &crep);
				break;
			case DIX_DELETE:
				m0_cas_index_delete_rep(cas_req, 0, &crep);
				break;
			case DIX_CCTGS_LOOKUP:
				m0_cas_index_lookup_rep(cas_req, 0, &crep);
				break;
			default:
				M0_IMPOSSIBLE("Unknown type %u", req->dr_type);
			}
			rc = crep.crr_rc;
			/*
			 * It is OK to get -ENOENT during 2nd phase, because
			 * catalogue can be deleted on 1st phase.
			 */
			if (dix_req_state(req) == DIXREQ_DEL_PHASE2 &&
			    rc == -ENOENT)
				rc = 0;
		}
		ditem->dxi_rc = rc;
	}
}

static void dix_idxop_completed(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_dix_req       *req = ast->sa_datum;
	struct m0_dix_idxop_ctx *idxop_ctx = &req->dr_idxop;
	struct m0_dix_idxop_req *idxop_req;
	struct m0_dix_item      *ditem;
	struct m0_dix_cas_req   *creq;
	bool                     del_phase2 = false;
	uint32_t                 i;
	uint64_t                 j;
	int                      rc;

	(void)grp;
	M0_ENTRY("req %p", req);
	for (i = 0; i < idxop_ctx->dcd_idxop_reqs_nr; i++) {
		idxop_req = &idxop_ctx->dcd_idxop_reqs[i];
		M0_ASSERT(idxop_req->dcr_index_no < req->dr_items_nr);
		ditem = &req->dr_items[idxop_req->dcr_index_no];
		for (j = 0; j < idxop_req->dcr_creqs_nr; j++) {
			creq = &idxop_req->dcr_creqs[j];
			dix_idxop_item_rc_update(ditem, req, creq);
			if (ditem->dxi_rc == 0 && idxop_req->dcr_del_phase2) {
				ditem->dxi_del_phase2 = true;
				del_phase2 = true;
			}
			m0_cas_req_fini(&creq->ds_creq);
		}
	}
	dix_idxop_ctx_free(idxop_ctx);
	if (del_phase2) {
		dix_req_state_set(req, DIXREQ_DEL_PHASE2);
		rc = dix_idxop_reqs_send(req);
		if (rc != 0)
			dix_req_failure(req, M0_ERR(rc));
	} else {
		dix_req_state_set(req, DIXREQ_FINAL);
	}
	M0_LEAVE();
}

static bool dix_idxop_clink_cb(struct m0_clink *cl)
{
	struct m0_dix_cas_req   *creq = container_of(cl, struct m0_dix_cas_req,
						     ds_clink);
	uint32_t                 state = creq->ds_creq.ccr_sm.sm_state;
	struct m0_dix_idxop_ctx *idxop;
	struct m0_dix_req       *dreq;

	if (M0_IN(state, (CASREQ_FINAL, CASREQ_FAILURE))) {
		dreq = creq->ds_parent;

		/* Update txid records in Clovis. */
		if (dreq->dr_cli->dx_sync_rec_update != NULL)
			dreq->dr_cli->dx_sync_rec_update(
				dreq, creq->ds_creq.ccr_sess,
				&creq->ds_creq.ccr_remid);

		m0_clink_del(cl);
		m0_clink_fini(cl);
		idxop = &creq->ds_parent->dr_idxop;
		idxop->dcd_completed_nr++;
		M0_PRE(idxop->dcd_completed_nr <= idxop->dcd_cas_reqs_nr);
		if (idxop->dcd_completed_nr == idxop->dcd_cas_reqs_nr) {
			idxop->dcd_ast.sa_cb = dix_idxop_completed;
			idxop->dcd_ast.sa_datum = dreq;
			m0_sm_ast_post(dix_req_smgrp(dreq), &idxop->dcd_ast);
		}
	}
	return true;
}

static int dix_idxop_pver_analyse(struct m0_dix_idxop_req *idxop_req,
				  struct m0_dix_req       *dreq,
				  uint64_t                *creqs_nr)
{
	struct m0_pool_version     *pver = idxop_req->dcr_pver;
	struct m0_poolmach         *pm   = &pver->pv_mach;
	struct m0_pools_common     *pc   = pver->pv_pc;
	struct m0_pooldev          *sdev;
	enum m0_pool_nd_state       state;
	struct m0_reqh_service_ctx *cas_svc;
	enum dix_req_type           type = dreq->dr_type;
	uint32_t                    i;
	int                         rc = 0;

	M0_PRE(M0_IN(type, (DIX_CREATE, DIX_DELETE, DIX_CCTGS_LOOKUP)));
	M0_PRE(ergo(type == DIX_CREATE, (dreq->dr_flags & COF_CROW) == 0));

	*creqs_nr = 0;
	for (i = 0; i < pm->pm_state->pst_nr_devices; i++) {
		sdev = &pm->pm_state->pst_devices_array[i];
		cas_svc = pc->pc_dev2svc[sdev->pd_sdev_idx].pds_ctx;
		if (cas_svc->sc_type != M0_CST_CAS) {
			rc = M0_ERR_INFO(-EINVAL, "Incorrect service type %d",
					 cas_svc->sc_type);
			break;
		}

		state = sdev->pd_state;
		/*
		 * It's impossible to create all component catalogues if some
		 * disk is not available (online or rebalancing). Also, it's
		 * impossible to check consistently that all component
		 * catalogues present if some disk is not online.
		 */
		if ((state != M0_PNDS_ONLINE && type == DIX_CCTGS_LOOKUP) ||
		    (!M0_IN(state, (M0_PNDS_ONLINE, M0_PNDS_SNS_REBALANCING)) &&
		     type == DIX_CREATE)) {
			rc = M0_ERR(-EIO);
			break;
		}

		/*
		 * Two-phase component catalogue removal is necessary if
		 * repair/re-balance is in progress.
		 * See dix/client.h, "Operation in degraded mode".
		 */
		if (type == DIX_DELETE &&
		    dix_req_state(dreq) != DIXREQ_DEL_PHASE2 &&
		    M0_IN(state, (M0_PNDS_SNS_REPAIRING,
				  M0_PNDS_SNS_REBALANCING)))
			idxop_req->dcr_del_phase2 = true;

		/*
		 * Send CAS requests only on online or rebalancing drives.
		 * Actually, only DIX_DELETE only can send on devices
		 * selectively. DIX_CREATE, DIX_CCTGS_LOOKUP send whether to all
		 * drives in a pool or to none.
		 */
		if (M0_IN(state, (M0_PNDS_ONLINE, M0_PNDS_SNS_REBALANCING)))
			(*creqs_nr)++;
	}

	if (rc == 0 && *creqs_nr == 0)
		rc = M0_ERR(-EIO);

	if (rc != 0)
		*creqs_nr = 0;
	M0_POST(rc == 0 ? *creqs_nr > 0 : *creqs_nr == 0);
	return rc;
}

static int dix_idxop_req_send(struct m0_dix_idxop_req *idxop_req,
			      struct m0_dix_req       *dreq,
			      uint64_t                *reqs_acc)
{
	struct m0_pool_version     *pver = idxop_req->dcr_pver;
	struct m0_poolmach         *pm   = &pver->pv_mach;
	struct m0_pools_common     *pc   = pver->pv_pc;
	struct m0_dix_cas_req      *creq;
	struct m0_pooldev          *sdev;
	uint32_t                    sdev_idx;
	struct m0_fid               cctg_fid;
	struct m0_reqh_service_ctx *cas_svc;
	uint32_t                    i;
	uint32_t                    k;
	struct m0_dix              *index;
	struct m0_cas_id            cid;
	uint32_t                    flags = dreq->dr_flags;
	uint64_t                    creqs_nr;
	int                         rc;

	m0_rwlock_read_lock(&pm->pm_lock);
	rc = dix_idxop_pver_analyse(idxop_req, dreq, &creqs_nr);
	if (rc != 0)
		goto pmach_unlock;
	M0_ALLOC_ARR(idxop_req->dcr_creqs, creqs_nr);
	if (idxop_req->dcr_creqs == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto pmach_unlock;
	}
	idxop_req->dcr_creqs_nr = creqs_nr;
	k = 0;
	for (i = 0; i < pm->pm_state->pst_nr_devices; i++) {
		sdev = &pver->pv_mach.pm_state->pst_devices_array[i];
		if (!M0_IN(sdev->pd_state, (M0_PNDS_ONLINE,
					    M0_PNDS_SNS_REBALANCING)))
			continue;
		sdev_idx = sdev->pd_sdev_idx;
		creq = &idxop_req->dcr_creqs[k++];
		creq->ds_parent = dreq;
		cas_svc = pc->pc_dev2svc[sdev_idx].pds_ctx;
		M0_ASSERT(cas_svc->sc_type == M0_CST_CAS);
		m0_cas_req_init(&creq->ds_creq, &cas_svc->sc_rlink.rlk_sess,
				dix_req_smgrp(dreq));
		dix_to_cas_map(dreq, &creq->ds_creq);
		m0_clink_init(&creq->ds_clink, dix_idxop_clink_cb);
		m0_clink_add(&creq->ds_creq.ccr_sm.sm_chan, &creq->ds_clink);
		index = &dreq->dr_indices[idxop_req->dcr_index_no];
		m0_dix_fid_convert_dix2cctg(&index->dd_fid, &cctg_fid,
					    sdev_idx);
		cid.ci_fid = cctg_fid;
		M0_ASSERT(index->dd_layout.dl_type == DIX_LTYPE_DESCR);
		cid.ci_layout.dl_type = index->dd_layout.dl_type;
		rc = m0_dix_ldesc_copy(&cid.ci_layout.u.dl_desc,
				       &index->dd_layout.u.dl_desc);
		if (rc == 0) {
			switch (dreq->dr_type) {
			case DIX_CREATE:
				rc = m0_cas_index_create(&creq->ds_creq, &cid,
							 1, dreq->dr_dtx);
				break;
			case DIX_DELETE:
				if (idxop_req->dcr_del_phase2)
					flags |= COF_DEL_LOCK;
				rc = m0_cas_index_delete(&creq->ds_creq, &cid,
							 1, dreq->dr_dtx,
							 flags);
				break;
			case DIX_CCTGS_LOOKUP:
				rc = m0_cas_index_lookup(&creq->ds_creq, &cid,
							 1);
				break;
			default:
				M0_IMPOSSIBLE("Unknown type %u", dreq->dr_type);
			}
		}
		m0_cas_id_fini(&cid);
		if (rc != 0) {
			creq->ds_rc = M0_ERR(rc);
			m0_clink_del(&creq->ds_clink);
			m0_clink_fini(&creq->ds_clink);
			/*
			 * index->dd_layout.u.dl_desc will be finalised in
			 * m0_dix_req_fini().
			 */
		} else {
			(*reqs_acc)++;
		}
	}
	M0_ASSERT(k == creqs_nr);
pmach_unlock:
	m0_rwlock_read_unlock(&pm->pm_lock);
	return M0_RC(rc);
}

static void dix_idxop_meta_update_ast_cb(struct m0_sm_group *grp,
					 struct m0_sm_ast   *ast)
{
	struct m0_dix_req      *req = ast->sa_datum;
	struct m0_dix_meta_req *meta_req = req->dr_meta_req;
	struct m0_dix_item     *item;
	uint64_t                fids_nr;
	int                     i;
	int                     k;
	/*
	 * Inidicates whether request processing should be continued.
	 * The request processing is stopped if all items in the user input
	 * vector are failed or CROW is requested for CREATE operation.
	 */
	bool                    cont = false;
	bool                    crow = !!(req->dr_flags & COF_CROW);
	int                     rc;

	fids_nr = m0_count(i, req->dr_items_nr, req->dr_items[i].dxi_rc == 0);
	M0_ASSERT(fids_nr > 0);
	rc = m0_dix_meta_generic_rc(meta_req);
	if (rc == 0) {
		k = 0;
		for (i = 0; i < req->dr_items_nr; i++) {
			item = &req->dr_items[i];
			if (item->dxi_rc == 0) {
				item->dxi_rc = m0_dix_meta_item_rc(meta_req, k);
				cont = cont || item->dxi_rc == 0;
				k++;
			}
		}
		M0_ASSERT(k == fids_nr);
		if (!cont)
			M0_LOG(M0_ERROR, "All items are failed");
		/*
		 * If CROW is requested for CREATE operation, then component
		 * catalogues shouldn't be created => no CAS create requests
		 * should be sent.
		 */
		cont = cont && !(req->dr_type == DIX_CREATE && crow);
		if (cont)
			rc = dix_idxop_reqs_send(req);
	}

	m0_dix_meta_req_fini(meta_req);
	m0_free0(&req->dr_meta_req);
	if (rc == 0)
		dix_req_state_set(req, !cont ?
				DIXREQ_FINAL : DIXREQ_INPROGRESS);
	else
		dix_req_failure(req, rc);
}

static bool dix_idxop_meta_update_clink_cb(struct m0_clink *cl)
{
	struct m0_dix_req *req = container_of(cl, struct m0_dix_req, dr_clink);

	/*
	 * Sining: no need to update SYNC records in Clovis from this callback
	 * as it is invoked in meta.c::dix_meta_op_done_cb() and reply fops
	 * have been processed to update SYNC records in dix_cas_rop_clink_cb()
	 * before it.
	 */

	m0_clink_del(cl);
	m0_clink_fini(cl);
	req->dr_ast.sa_cb = dix_idxop_meta_update_ast_cb;
	req->dr_ast.sa_datum = req;
	m0_sm_ast_post(dix_req_smgrp(req), &req->dr_ast);
	return true;
}

static int dix_idxop_meta_update(struct m0_dix_req *req)
{
	struct m0_dix_meta_req *meta_req;
	struct m0_fid          *fids;
	struct m0_dix_layout   *layouts = NULL;
	uint32_t                fids_nr;
	bool                    create = req->dr_type == DIX_CREATE;
	uint64_t                i;
	uint64_t                k;
	int                     rc;

	M0_ENTRY();
	M0_PRE(M0_IN(req->dr_type, (DIX_CREATE, DIX_DELETE)));
	M0_ASSERT(req->dr_indices_nr == req->dr_items_nr);
	fids_nr = m0_count(i, req->dr_items_nr, req->dr_items[i].dxi_rc == 0);
	M0_ASSERT(fids_nr > 0);
	M0_ALLOC_PTR(req->dr_meta_req);
	M0_ALLOC_ARR(fids, fids_nr);
	if (create)
		M0_ALLOC_ARR(layouts, fids_nr);
	if (fids == NULL || (create && layouts == NULL) ||
	    req->dr_meta_req == NULL) {
		m0_free(fids);
		m0_free(layouts);
		m0_free(req->dr_meta_req);
		return M0_ERR(-ENOMEM);
	}

	meta_req = req->dr_meta_req;
	k = 0;
	for (i = 0; i < req->dr_items_nr; i++) {
		if (req->dr_items[i].dxi_rc == 0) {
			if (create) {
				fids[k] = req->dr_orig_indices[i].dd_fid;
				layouts[k] = req->dr_orig_indices[i].dd_layout;
			} else {
				fids[k] = req->dr_indices[i].dd_fid;
			}
			k++;
		}
	}
	M0_ASSERT(k == fids_nr);

	m0_dix_meta_req_init(meta_req, req->dr_cli, dix_req_smgrp(req));
	dix_to_mdix_map(req, meta_req);
	/* Pass down the SYNC datum. */
	meta_req->dmr_req.dr_sync_datum = req->dr_sync_datum;
	m0_clink_init(&req->dr_clink, dix_idxop_meta_update_clink_cb);
	m0_clink_add_lock(&meta_req->dmr_chan, &req->dr_clink);
	rc = create ?
	     m0_dix_layout_put(meta_req, fids, layouts, fids_nr, 0) :
	     m0_dix_layout_del(meta_req, fids, fids_nr);
	if (rc != 0) {
		m0_clink_del_lock(&req->dr_clink);
		m0_clink_fini(&req->dr_clink);
		m0_dix_meta_req_fini(meta_req);
		m0_free0(&req->dr_meta_req);
	}
	m0_free(layouts);
	m0_free(fids);
	return M0_RC(rc);
}

/**
 * Determines whether item should be sent in dix_idxop_reqs_send().
 *
 * Item should be sent if there was no failure for it on previous steps
 * (i.e. during discovery) or if it should be sent during second phase of
 * delete operation (see "Operation in degraded mode" in dix/client.h).
 */
static bool dix_item_should_be_sent(const struct m0_dix_req *req, uint32_t i)
{
	return dix_req_state(req) == DIXREQ_DEL_PHASE2 ?
			req->dr_items[i].dxi_del_phase2 :
			req->dr_items[i].dxi_rc == 0;
}

static int dix_idxop_reqs_send(struct m0_dix_req *req)
{
	struct m0_dix           *indices = req->dr_indices;
	struct m0_dix_idxop_ctx *idxop   = &req->dr_idxop;
	struct m0_dix_idxop_req *idxop_req;
	struct m0_dix_layout    *layout;
	uint32_t                 reqs_nr;
	uint32_t                 i;
	uint32_t                 k;
	uint64_t                 cas_nr = 0;
	int                      rc;

	M0_ENTRY();
	M0_PRE(M0_IN(dix_req_state(req), (DIXREQ_DISCOVERY_DONE,
					  DIXREQ_META_UPDATE,
					  DIXREQ_DEL_PHASE2)));
	reqs_nr = m0_count(i, req->dr_items_nr,
			   dix_item_should_be_sent(req, i));
	M0_PRE(reqs_nr > 0);
	M0_SET0(idxop);
	M0_ALLOC_ARR(idxop->dcd_idxop_reqs, reqs_nr);
	if (idxop->dcd_idxop_reqs == NULL)
		return M0_ERR(-ENOMEM);
	idxop->dcd_idxop_reqs_nr = reqs_nr;
	for (i = 0, k = 0; i < req->dr_items_nr; i++) {
		if (dix_item_should_be_sent(req, i)) {
			layout = &indices[i].dd_layout;
			M0_PRE(layout->dl_type == DIX_LTYPE_DESCR);
			idxop_req = &idxop->dcd_idxop_reqs[k++];
			idxop_req->dcr_index_no = i;
			idxop_req->dcr_pver = dix_pver_find(req,
						&layout->u.dl_desc.ld_pver);
			rc = dix_idxop_req_send(idxop_req, req, &cas_nr);
			if (rc != 0)
				req->dr_items[i].dxi_rc = M0_ERR(rc);
		}
	}
	if (cas_nr == 0) {
		dix_idxop_ctx_free(idxop);
		return M0_ERR(-EIO);
	} else {
		idxop->dcd_completed_nr = 0;
		idxop->dcd_cas_reqs_nr = cas_nr;
	}
	return M0_RC(0);
}

static void dix_idxop(struct m0_dix_req *req)
{
	enum m0_dix_req_state next_state = DIXREQ_INVALID;
	int                   rc;

	M0_PRE(dix_resolved_nr(req) > 0);
	/*
	 * Put/delete ordinary indices layouts in 'layout' meta-index.
	 */
	if (!req->dr_is_meta &&
	    M0_IN(req->dr_type, (DIX_CREATE, DIX_DELETE))) {
		rc = dix_idxop_meta_update(req);
		next_state = DIXREQ_META_UPDATE;
	} else {
		rc = dix_idxop_reqs_send(req);
		next_state = DIXREQ_INPROGRESS;
	}

	if (rc == 0)
		dix_req_state_set(req, next_state);
	else
		dix_req_failure(req, M0_ERR(rc));
}

M0_INTERNAL int m0_dix_create(struct m0_dix_req   *req,
			      const struct m0_dix *indices,
			      uint32_t             indices_nr,
			      struct m0_dtx       *dtx,
			      uint32_t             flags)
{
	int rc;

	M0_ENTRY();
	/*
	 * User should provide layouts of each index to create in the form of
	 * layout descriptor or layout id.
	 */
	M0_PRE(m0_forall(i, indices_nr,
	       indices[i].dd_layout.dl_type != DIX_LTYPE_UNKNOWN));
	M0_PRE(ergo(req->dr_is_meta, dix_id_layouts_nr(req) == 0));
	M0_PRE(M0_IN(flags, (0, COF_CROW)));
	req->dr_dtx = dtx;
	/*
	 * Save indices identifiers in two arrays. Indices identifiers in
	 * req->dr_indices will be overwritten once layout ids (if any) are
	 * resolved into layout descriptors. Record in 'layout' index should be
	 * inserted with the layout requested by a user, not the resolved one.
	 */
	rc = dix_req_indices_copy(req, indices, indices_nr) ?:
	     dix_indices_copy(&req->dr_orig_indices, indices, indices_nr);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ALLOC_ARR(req->dr_items, indices_nr);
	if (req->dr_items == NULL)
		/*
		 * Cleanup of req->dr_indices and req->dr_orig_indices will be
		 * done in m0_dix_req_fini().
		 */
		return M0_ERR(-ENOMEM);
	req->dr_items_nr = indices_nr;
	req->dr_type = DIX_CREATE;
	req->dr_flags = flags;
	dix_discovery(req);
	return M0_RC(0);
}

static void dix_ldescr_resolve(struct m0_dix_req *req)
{
	struct m0_dix          *indices  = req->dr_indices;
	uint32_t                id_nr    = dix_id_layouts_nr(req);
	struct m0_dix_meta_req *meta_req;
	uint64_t               *lids;
	int                     i;
	int                     k;
	int                     rc;

	/*
	 * If layout descriptors have DIX_LTYPE_ID, then they should be loaded
	 * via m0_dix_ldescr_get() in order to find out actual layout.
	 */
	M0_ALLOC_PTR(req->dr_meta_req);
	M0_ALLOC_ARR(lids, id_nr);
	if (lids == NULL || req->dr_meta_req == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto err;
	}
	for (i = 0, k = 0; i < req->dr_indices_nr; i++)
		if (indices[i].dd_layout.dl_type == DIX_LTYPE_ID)
			lids[k++] = indices[i].dd_layout.u.dl_id;
	meta_req = req->dr_meta_req;
	m0_dix_meta_req_init(meta_req, req->dr_cli, dix_req_smgrp(req));
	dix_to_mdix_map(req, meta_req);
	m0_clink_init(&req->dr_clink, dix_layout_find_clink_cb);
	m0_clink_add_lock(&meta_req->dmr_chan, &req->dr_clink);
	/* Start loading layout descriptors from CAS. */
	rc = m0_dix_ldescr_get(meta_req, lids, id_nr);
	if (rc != 0) {
		m0_clink_del_lock(&req->dr_clink);
		m0_clink_fini(&req->dr_clink);
		m0_dix_meta_req_fini(meta_req);
	}

err:
	if (rc != 0) {
		m0_free0(&req->dr_meta_req);
		dix_req_failure(req, rc);
	} else {
		dix_req_state_set(req, DIXREQ_LID_DISCOVERY);
	}
	m0_free(lids);
}

static void addb2_add_dix_req_attrs(const struct m0_dix_req *req)
{
	uint64_t sm_id = m0_sm_id_get(&req->dr_sm);

	M0_ADDB2_ADD(M0_AVI_ATTR, sm_id,
		     M0_AVI_DIX_REQ_ATTR_IS_META, req->dr_is_meta);
	M0_ADDB2_ADD(M0_AVI_ATTR, sm_id,
		     M0_AVI_DIX_REQ_ATTR_REQ_TYPE, req->dr_type);
	M0_ADDB2_ADD(M0_AVI_ATTR, sm_id,
		     M0_AVI_DIX_REQ_ATTR_ITEMS_NR, req->dr_items_nr);
	M0_ADDB2_ADD(M0_AVI_ATTR, sm_id,
		     M0_AVI_DIX_REQ_ATTR_INDICES_NR, req->dr_indices_nr);
	if (req->dr_keys != NULL)
		M0_ADDB2_ADD(M0_AVI_ATTR, sm_id, M0_AVI_DIX_REQ_ATTR_KEYS_NR,
			     req->dr_keys->ov_vec.v_nr);
	if (req->dr_vals != NULL)
		M0_ADDB2_ADD(M0_AVI_ATTR, sm_id, M0_AVI_DIX_REQ_ATTR_VALS_NR,
			     req->dr_vals->ov_vec.v_nr);
}

static void dix_discovery_completed(struct m0_dix_req *req)
{
	dix_req_state_set(req, DIXREQ_DISCOVERY_DONE);
	addb2_add_dix_req_attrs(req);
	/*
	 * All layouts have been resolved, all types are DIX_LTYPE_DESCR,
	 * perform dix operation.
	 */
	switch (req->dr_type) {
	case DIX_CREATE:
	case DIX_DELETE:
	case DIX_CCTGS_LOOKUP:
		dix_idxop(req);
		break;
	case DIX_GET:
	case DIX_PUT:
	case DIX_DEL:
	case DIX_NEXT:
		dix_rop(req);
		break;
	default:
		M0_IMPOSSIBLE("Unknown request type %u", req->dr_type);
	}
}

static void dix_discovery_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_dix_req *req = container_of(ast, struct m0_dix_req, dr_ast);

	(void)grp;
	if (dix_unknown_layouts_nr(req) > 0)
		dix_layout_find(req);
	else if (dix_id_layouts_nr(req) > 0)
		dix_ldescr_resolve(req);
	else
		dix_discovery_completed(req);
}

static void dix_discovery(struct m0_dix_req *req)
{
	/* Entry point to start discovering and resolving layout descriptors. */
	req->dr_ast.sa_cb = dix_discovery_ast;
	req->dr_ast.sa_datum = req;
	m0_sm_ast_post(dix_req_smgrp(req), &req->dr_ast);
}

M0_INTERNAL int m0_dix_delete(struct m0_dix_req   *req,
			      const struct m0_dix *indices,
			      uint64_t             indices_nr,
			      struct m0_dtx       *dtx,
			      uint32_t             flags)
{
	int rc;

	M0_ENTRY();
	M0_PRE(M0_IN(flags, (0, COF_CROW)));
	req->dr_dtx = dtx;
	rc = dix_req_indices_copy(req, indices, indices_nr);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ALLOC_ARR(req->dr_items, indices_nr);
	if (req->dr_items == NULL)
		return M0_ERR(-ENOMEM);
	req->dr_items_nr = indices_nr;
	req->dr_type = DIX_DELETE;
	req->dr_flags = flags;
	dix_discovery(req);
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_cctgs_lookup(struct m0_dix_req   *req,
				    const struct m0_dix *indices,
				    uint32_t             indices_nr)
{
	int rc;

	M0_ENTRY();
	rc = dix_req_indices_copy(req, indices, indices_nr);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ALLOC_ARR(req->dr_items, indices_nr);
	if (req->dr_items == NULL)
		return M0_ERR(-ENOMEM);
	req->dr_items_nr = indices_nr;
	req->dr_type = DIX_CCTGS_LOOKUP;
	dix_discovery(req);
	return M0_RC(0);
}

static int dix_rec_op_init(struct m0_dix_rec_op   *rec_op,
			   struct m0_dix_req      *req,
			   struct m0_dix_cli      *cli,
			   struct m0_pool_version *pver,
			   struct m0_dix          *dix,
			   struct m0_buf          *key,
			   uint64_t                user_item)
{
	int rc;

	M0_PRE(dix->dd_layout.dl_type == DIX_LTYPE_DESCR);
	rc = m0_dix_layout_iter_init(&rec_op->dgp_iter, &dix->dd_fid,
				     cli->dx_ldom, pver,
				     &dix->dd_layout.u.dl_desc, key);
	if (rc != 0)
		return M0_ERR(rc);
	rec_op->dgp_units_nr = dix_rop_tgt_iter_max(req, rec_op);
	M0_ALLOC_ARR(rec_op->dgp_units, rec_op->dgp_units_nr);
	if (rec_op->dgp_units == NULL) {
		m0_dix_layout_iter_fini(&rec_op->dgp_iter);
		return M0_ERR(rc);
	}
	rec_op->dgp_item = user_item;
	rec_op->dgp_key  = *key;
	return 0;
}

static void dix_rec_op_fini(struct m0_dix_rec_op *rec_op)
{
	m0_dix_layout_iter_fini(&rec_op->dgp_iter);
	m0_free(rec_op->dgp_units);
}

static int dix_cas_rop_alloc(struct m0_dix_req *req, uint32_t sdev,
			     struct m0_dix_cas_rop **cas_rop)
{
	struct m0_dix_rop_ctx *rop = req->dr_rop;

	M0_ALLOC_PTR(*cas_rop);
	if (*cas_rop == NULL)
		return M0_ERR(-ENOMEM);
	(*cas_rop)->crp_parent = req;
	(*cas_rop)->crp_sdev_idx = sdev;
	(*cas_rop)->crp_flags = req->dr_flags;
	cas_rop_tlink_init_at(*cas_rop, &rop->dg_cas_reqs);
	return 0;
}

static void dix_cas_rop_fini(struct m0_dix_cas_rop *cas_rop)
{
	m0_free(cas_rop->crp_attrs);
	m0_bufvec_free2(&cas_rop->crp_keys);
	m0_bufvec_free2(&cas_rop->crp_vals);
	cas_rop_tlink_fini(cas_rop);
}

static void dix_cas_rops_fini(struct m0_tl *cas_rops)
{
	struct m0_dix_cas_rop *cas_rop;

	m0_tl_teardown(cas_rop, cas_rops, cas_rop) {
		dix_cas_rop_fini(cas_rop);
		m0_free(cas_rop);
	}
}

static int dix_rop_ctx_init(struct m0_dix_req      *req,
			    struct m0_dix_rop_ctx  *rop,
			    const struct m0_bufvec *keys,
			    uint64_t               *indices)
{
	struct m0_dix       *dix = &req->dr_indices[0];
	struct m0_dix_ldesc *ldesc;
	uint32_t             keys_nr;
	struct m0_buf        key;
	uint32_t             i;
	int                  rc = 0;

	M0_ENTRY();
	M0_PRE(M0_IS0(rop));
	M0_PRE(req->dr_indices_nr == 1);
	M0_PRE(dix->dd_layout.dl_type == DIX_LTYPE_DESCR);
	M0_PRE(keys != NULL);
	keys_nr = keys->ov_vec.v_nr;
	M0_PRE(keys_nr != 0);
	ldesc = &dix->dd_layout.u.dl_desc;
	rop->dg_pver = dix_pver_find(req, &ldesc->ld_pver);
	M0_ALLOC_ARR(rop->dg_rec_ops, keys_nr);
	M0_ALLOC_ARR(rop->dg_target_rop, rop->dg_pver->pv_attr.pa_P);
	if (rop->dg_rec_ops == NULL || rop->dg_target_rop == NULL)
		return M0_ERR(-ENOMEM);
	for (i = 0; i < keys_nr; i++) {
		key = M0_BUF_INIT(keys->ov_vec.v_count[i], keys->ov_buf[i]);
		rc = dix_rec_op_init(&rop->dg_rec_ops[i], req, req->dr_cli,
				     rop->dg_pver, &req->dr_indices[0], &key,
				     indices == NULL ? i : indices[i]);
		if (rc != 0) {
			for (i = 0; i < rop->dg_rec_ops_nr; i++)
				dix_rec_op_fini(&rop->dg_rec_ops[i]);
			break;
		}
		rop->dg_rec_ops_nr++;
	}
	cas_rop_tlist_init(&rop->dg_cas_reqs);
	return M0_RC(rc);
}

static void dix_rop_ctx_fini(struct m0_dix_rop_ctx *rop)
{
	uint32_t i;

	for (i = 0; i < rop->dg_rec_ops_nr; i++)
		dix_rec_op_fini(&rop->dg_rec_ops[i]);
	m0_free(rop->dg_rec_ops);
	m0_free(rop->dg_target_rop);
	dix_cas_rops_fini(&rop->dg_cas_reqs);
	cas_rop_tlist_fini(&rop->dg_cas_reqs);
	M0_SET0(rop);
}

static void dix__rop(struct m0_dix_req *req, const struct m0_bufvec *keys,
		     uint64_t *indices)
{
	struct m0_dix_rop_ctx *rop;
	uint32_t               keys_nr;
	int                    rc;

	M0_PRE(keys != NULL);
	M0_PRE(req->dr_indices_nr == 1);

	keys_nr = keys->ov_vec.v_nr;
	M0_PRE(keys_nr != 0);
	M0_ALLOC_PTR(rop);
	if (rop == NULL) {
		dix_req_failure(req, M0_ERR(-ENOMEM));
		return;
	}
	rc = dix_rop_ctx_init(req, rop, keys, indices);
	if (rc != 0) {
		dix_req_failure(req, rc);
		return;
	}
	req->dr_rop = rop;
	dix_rop_units_set(req);
	rc = dix_cas_rops_alloc(req) ?:
	     dix_cas_rops_fill(req) ?:
	     dix_cas_rops_send(req);
	if (rc != 0) {
		dix_rop_ctx_fini(rop);
		dix_req_failure(req, rc);
	} else if (dix_req_state(req) != DIXREQ_DEL_PHASE2)
		dix_req_state_set(req, DIXREQ_INPROGRESS);
}

static void dix_rop(struct m0_dix_req *req)
{
	M0_PRE(req->dr_indices_nr == 1);
	M0_PRE(dix_unknown_layouts_nr(req) == 0);
	M0_PRE(req->dr_keys != NULL);

	dix__rop(req, req->dr_keys, NULL);
}

static void dix_item_rc_update(struct m0_dix_req  *req,
			       struct m0_cas_req  *creq,
			       uint64_t            key_idx,
			       struct m0_dix_item *ditem)
{
	struct m0_cas_rec_reply rep;
	struct m0_cas_get_reply get_rep;
	int                     rc;
	enum dix_req_type       rtype = req->dr_type;

	rc = m0_cas_req_generic_rc(creq);
	if (rc == 0) {
		switch (rtype) {
		case DIX_GET:
			m0_cas_get_rep(creq, key_idx, &get_rep);
			rc = get_rep.cge_rc;
			if (rc == 0) {
				ditem->dxi_val = get_rep.cge_val;
				/* Value will be freed at m0_dix_req_fini(). */
				m0_cas_rep_mlock(creq, key_idx);
			}
			break;
		case DIX_PUT:
			m0_cas_put_rep(creq, key_idx, &rep);
			rc = rep.crr_rc;
			break;
		case DIX_DEL:
			m0_cas_del_rep(creq, key_idx, &rep);
			/*
			 * It is possible that repair process didn't copy
			 * replica to spare disk yet. Ignore such an error.
			 */
			if (dix_req_state(req) == DIXREQ_DEL_PHASE2 &&
			    rep.crr_rc == -ENOENT)
				rep.crr_rc = 0;
			rc = rep.crr_rc;
			break;
		default:
			M0_IMPOSSIBLE("Incorrect type %u", rtype);
		}
	}
	ditem->dxi_rc = rc;
}

static bool dix_item_get_has_failed(struct m0_dix_item *item)
{
	return item->dxi_rc != 0 && item->dxi_rc != -ENOENT;
}

static bool dix_item_parity_unit_is_last(const struct m0_dix_req  *req,
					 const struct m0_dix_item *item)
{
	struct m0_pool_version *pver;

	pver = m0_dix_pver(req->dr_cli, &req->dr_indices[0]);
	return item->dxi_pg_unit == pver->pv_attr.pa_N + pver->pv_attr.pa_K - 1;
}

static void dix_get_req_resend(struct m0_dix_req *req)
{
	struct m0_bufvec  keys;
	uint64_t         *indices;
	uint32_t          keys_nr;
	uint32_t          i;
	uint32_t          k = 0;
	int               rc;

	keys_nr = m0_count(i, req->dr_items_nr,
			dix_item_get_has_failed(&req->dr_items[i]) &&
			!dix_item_parity_unit_is_last(req, &req->dr_items[i]));
	if (keys_nr == 0) {
		/*
		 * Some records are not retrieved from both data and parity
		 * units locations.
		 */
		rc = M0_ERR(-EHOSTUNREACH);
		goto end;
	}
	rc = m0_bufvec_empty_alloc(&keys, keys_nr);
	M0_ALLOC_ARR(indices, keys_nr);
	if (rc != 0 || indices == NULL) {
		rc = rc ?: M0_ERR(-ENOMEM);
		goto free;
	}
	for (i = 0; i < req->dr_items_nr; i++) {
		if (!dix_item_get_has_failed(&req->dr_items[i]))
			continue;
		/*
		 * Clear error code in order to update it successfully on
		 * request completion. Otherwise, it wouldn't be overwritten
		 * because it is possible that several CAS requests are sent
		 * for one item and it holds error code for the first failed
		 * request.
		 */
		req->dr_items[i].dxi_rc = 0;
		req->dr_items[i].dxi_pg_unit++;
		keys.ov_vec.v_count[k] = req->dr_keys->ov_vec.v_count[i];
		keys.ov_buf[k] = req->dr_keys->ov_buf[i];
		indices[k] = i;
		k++;
	}

	dix__rop(req, &keys, indices);
free:
	m0_bufvec_free2(&keys);
	m0_free(indices);
end:
	if (rc != 0)
		dix_req_failure(req, M0_ERR(rc));
}

static bool dix_del_phase2_is_needed(const struct m0_dix_rec_op *rec_op)
{
	return m0_exists(i, rec_op->dgp_units_nr,
			 rec_op->dgp_units[i].dpu_del_phase2);
}

static int dix_rop_del_phase2_rop(struct m0_dix_req      *req,
				  struct m0_dix_rop_ctx **out)
{
	struct m0_dix_rop_ctx *cur_rop = req->dr_rop;
	struct m0_dix_rop_ctx *out_rop;
	struct m0_dix_rec_op  *src_rec_op;
	struct m0_dix_rec_op  *dst_rec_op;
	struct m0_bufvec       keys;
	uint64_t              *indices;
	uint32_t               keys_nr;
	uint32_t               i;
	uint32_t               j;
	uint32_t               k = 0;
	int                    rc;

	keys_nr = m0_count(i, cur_rop->dg_rec_ops_nr,
			   dix_del_phase2_is_needed(&cur_rop->dg_rec_ops[i]));

	if (keys_nr == 0)
		return 0;

	rc = m0_bufvec_empty_alloc(&keys, keys_nr);
	M0_ALLOC_ARR(indices, keys_nr);
	M0_ALLOC_PTR(out_rop);
	if (rc != 0 || indices == NULL || out_rop == NULL) {
		rc = M0_ERR(rc ?: -ENOMEM);
		goto free;
	}
	for (i = 0; i < cur_rop->dg_rec_ops_nr; i++) {
		if (!dix_del_phase2_is_needed(&cur_rop->dg_rec_ops[i]))
			continue;
		keys.ov_vec.v_count[k] = req->dr_keys->ov_vec.v_count[i];
		keys.ov_buf[k] = req->dr_keys->ov_buf[i];
		indices[k] = i;
		k++;
	}

	rc = dix_rop_ctx_init(req, out_rop, &keys, indices);
	if (rc != 0)
		goto free;
	k = 0;
	for (i = 0; i < cur_rop->dg_rec_ops_nr; i++) {
		src_rec_op = &cur_rop->dg_rec_ops[i];
		if (!dix_del_phase2_is_needed(src_rec_op))
			continue;
		dst_rec_op = &out_rop->dg_rec_ops[k++];
		M0_ASSERT(src_rec_op->dgp_units_nr == dst_rec_op->dgp_units_nr);
		for (j = 0; j < src_rec_op->dgp_units_nr; j++)
			dst_rec_op->dgp_units[j] = src_rec_op->dgp_units[j];
	}

free:
	m0_bufvec_free2(&keys);
	m0_free(indices);
	if (rc == 0) {
		rc = keys_nr;
		*out = out_rop;
	} else
		m0_free(out_rop);
	return M0_RC(rc);
}

static void dix_rop_del_phase2(struct m0_dix_req *req)
{
	int rc;

	dix_req_state_set(req, DIXREQ_DEL_PHASE2);

	rc = dix_cas_rops_alloc(req) ?:
	     dix_cas_rops_fill(req) ?:
	     dix_cas_rops_send(req);

	if (rc != 0) {
		dix_rop_ctx_fini(req->dr_rop);
		dix_req_failure(req, rc);
	}
}

static void dix_cas_rop_rc_update(struct m0_dix_cas_rop *cas_rop, int rc)
{
	struct m0_dix_req  *req = cas_rop->crp_parent;
	struct m0_dix_item *ditem;
	uint64_t            item_idx;
	uint32_t            i;

	for (i = 0; i < cas_rop->crp_keys_nr; i++) {
		item_idx = cas_rop->crp_attrs[i].cra_item;
		ditem = &req->dr_items[item_idx];
		if (ditem->dxi_rc != 0)
			continue;
		if (rc == 0)
			dix_item_rc_update(req, &cas_rop->crp_creq, i, ditem);
		else
			ditem->dxi_rc = M0_ERR(rc);
	}
}

static void dix_rop_completed(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_dix_req     *req = ast->sa_datum;
	struct m0_dix_rop_ctx *rop = req->dr_rop;
	struct m0_dix_rop_ctx *rop_del_phase2;
	bool                   del_phase2 = false;
	struct m0_dix_cas_rop *cas_rop;

	(void)grp;
	if (req->dr_type == DIX_NEXT)
		m0_dix_next_result_prepare(req);
	else
		m0_tl_for(cas_rop, &rop->dg_cas_reqs, cas_rop) {
			dix_cas_rop_rc_update(cas_rop, 0);
			m0_cas_req_fini(&cas_rop->crp_creq);
		} m0_tl_endfor;

	if (req->dr_type == DIX_DEL &&
	    dix_req_state(req) == DIXREQ_INPROGRESS)
		del_phase2 = dix_rop_del_phase2_rop(req, &rop_del_phase2) > 0;

	dix_rop_ctx_fini(rop);
	if (req->dr_type == DIX_GET &&
	    m0_exists(i, req->dr_items_nr,
		      dix_item_get_has_failed(&req->dr_items[i]))) {
		dix_req_state_set(req, DIXREQ_GET_RESEND);
		dix_get_req_resend(req);
	} else if (req->dr_type == DIX_DEL && del_phase2) {
		m0_free(rop);
		req->dr_rop = rop_del_phase2;
		dix_rop_del_phase2(req);
	} else {
		dix_req_state_set(req, DIXREQ_FINAL);
	}
}

static bool dix_cas_rop_clink_cb(struct m0_clink *cl)
{
	struct m0_dix_cas_rop  *crop = container_of(cl, struct m0_dix_cas_rop,
						    crp_clink);
	uint32_t                state = crop->crp_creq.ccr_sm.sm_state;
	struct m0_dix_rop_ctx  *rop;
	struct m0_dix_req      *dreq;

	if (M0_IN(state, (CASREQ_FINAL, CASREQ_FAILURE))) {
		dreq = crop->crp_parent;

		/*
		 * Update pending transaction number. Note: as
		 * m0_cas_req::ccr_fop is set to NULL in cas_req_reply_handle()
		 * we must get the returned remid before that.
	 	 */
		if (dreq->dr_cli->dx_sync_rec_update != NULL)
			dreq->dr_cli->dx_sync_rec_update(
				dreq, crop->crp_creq.ccr_sess,
				&crop->crp_creq.ccr_remid);


		m0_clink_del(cl);
		m0_clink_fini(cl);
		rop = crop->crp_parent->dr_rop;
		rop->dg_completed_nr++;
		M0_PRE(rop->dg_completed_nr <= rop->dg_cas_reqs_nr);
		if (rop->dg_completed_nr == rop->dg_cas_reqs_nr) {
			rop->dg_ast.sa_cb = dix_rop_completed;
			rop->dg_ast.sa_datum = dreq;
			m0_sm_ast_post(dix_req_smgrp(dreq), &rop->dg_ast);
		}
	}
	return true;
}

static int dix_cas_rops_send(struct m0_dix_req *req)
{
	struct m0_pools_common     *pc = req->dr_cli->dx_pc;
	struct m0_dix_rop_ctx      *rop = req->dr_rop;
	struct m0_dix_cas_rop      *cas_rop;
	struct m0_cas_req          *creq;
	uint32_t                    sdev_idx;
	struct m0_cas_id            cctg_id;
	struct m0_reqh_service_ctx *cas_svc;
	struct m0_dix_layout       *layout = &req->dr_indices[0].dd_layout;
	int                         rc;

	M0_PRE(rop->dg_cas_reqs_nr == 0);
	m0_tl_for(cas_rop, &rop->dg_cas_reqs, cas_rop) {
		sdev_idx = cas_rop->crp_sdev_idx;
		creq = &cas_rop->crp_creq;
		cas_svc = pc->pc_dev2svc[sdev_idx].pds_ctx;
		M0_ASSERT(cas_svc->sc_type == M0_CST_CAS);
		m0_cas_req_init(creq, &cas_svc->sc_rlink.rlk_sess,
				dix_req_smgrp(req));
		dix_to_cas_map(req, creq);
		m0_clink_init(&cas_rop->crp_clink, dix_cas_rop_clink_cb);
		m0_clink_add(&creq->ccr_sm.sm_chan, &cas_rop->crp_clink);
		M0_ASSERT(req->dr_indices_nr == 1);
		m0_dix_fid_convert_dix2cctg(&req->dr_indices[0].dd_fid,
					    &cctg_id.ci_fid, sdev_idx);
		M0_ASSERT(layout->dl_type == DIX_LTYPE_DESCR);
		cctg_id.ci_layout.dl_type = layout->dl_type;
		/** @todo CAS request should copy cctg_id internally. */
		rc = m0_dix_ldesc_copy(&cctg_id.ci_layout.u.dl_desc,
				       &layout->u.dl_desc);
		switch (req->dr_type) {
		case DIX_GET:
			rc = m0_cas_get(creq, &cctg_id, &cas_rop->crp_keys);
			break;
		case DIX_PUT:
			rc = m0_cas_put(creq, &cctg_id, &cas_rop->crp_keys,
					&cas_rop->crp_vals, req->dr_dtx,
					cas_rop->crp_flags);
			break;
		case DIX_DEL:
			rc = m0_cas_del(creq, &cctg_id, &cas_rop->crp_keys,
					req->dr_dtx, cas_rop->crp_flags);
			break;
		case DIX_NEXT:
			rc = m0_cas_next(creq, &cctg_id, &cas_rop->crp_keys,
					 req->dr_recs_nr,
					 cas_rop->crp_flags | COF_SLANT);
			break;
		default:
			M0_IMPOSSIBLE("Unknown req type %u", req->dr_type);
		}
		if (rc != 0) {
			m0_clink_del(&cas_rop->crp_clink);
			m0_clink_fini(&cas_rop->crp_clink);
			m0_cas_req_fini(&cas_rop->crp_creq);
			dix_cas_rop_rc_update(cas_rop, rc);
			cas_rop_tlink_del_fini(cas_rop);
			dix_cas_rop_fini(cas_rop);
			m0_free(cas_rop);
		} else {
			rop->dg_cas_reqs_nr++;
		}
	} m0_tl_endfor;

	if (rop->dg_cas_reqs_nr == 0)
		return M0_ERR(-EFAULT);
	return M0_RC(0);
}

static void dix_rop_tgt_iter_begin(const struct m0_dix_req *req,
				   struct m0_dix_rec_op    *rec_op)
{
	if (req->dr_type == DIX_NEXT)
		rec_op->dgp_next_tgt = 0;
	else
		m0_dix_layout_iter_reset(&rec_op->dgp_iter);
}

static uint32_t dix_rop_tgt_iter_max(struct m0_dix_req    *req,
				     struct m0_dix_rec_op *rec_op)
{
	struct m0_dix_layout_iter *iter = &rec_op->dgp_iter;
	enum dix_req_type          type = req->dr_type;

	M0_ASSERT(M0_IN(type, (DIX_GET, DIX_PUT, DIX_DEL, DIX_NEXT)));
	if (type == DIX_NEXT)
		/*
		 * NEXT operation should be sent to all devices, because the
		 * distribution of keys over devices is unknown. Therefore, all
		 * component catalogues should be queried and returned records
		 * should be merge-sorted.
		 */
		return m0_dix_liter_P(iter);
	else
		return m0_dix_liter_N(iter) + 2 * m0_dix_liter_K(iter);
}

static void dix_rop_tgt_iter_next(const struct m0_dix_req *req,
				  struct m0_dix_rec_op    *rec_op,
				  uint64_t                *target,
				  bool                    *is_spare)
{
	if (req->dr_type != DIX_NEXT) {
		*is_spare = m0_dix_liter_unit_classify(&rec_op->dgp_iter,
				rec_op->dgp_iter.dit_unit) == M0_PUT_SPARE;
		m0_dix_layout_iter_next(&rec_op->dgp_iter, target);
	} else {
		*target = rec_op->dgp_next_tgt++;
		*is_spare = false;
	}
}

static int dix_spare_slot_find(struct m0_poolmach_state *pm_state,
			       uint64_t                  failed_tgt,
			       uint32_t                 *spare_slot)
{
	struct m0_pool_spare_usage *spare_usage_array;
	uint32_t                    i;

	spare_usage_array = pm_state->pst_spare_usage_array;
	for (i = 0; i < pm_state->pst_max_device_failures; i++) {
		if (spare_usage_array[i].psu_device_index == failed_tgt) {
			*spare_slot = i;
			return 0;
		}
	}
	return M0_ERR_INFO(-ENOENT, "No spare slot found for target %"PRIu64,
			   failed_tgt);
}

static struct m0_pool_version *dix_rec_op_pver(struct m0_dix_rec_op *rec_op)
{
	return m0_pdl_to_layout(rec_op->dgp_iter.dit_linst.li_pl)->l_pver;
}

static uint32_t dix_rop_max_failures(struct m0_dix_rop_ctx *rop)
{
	struct m0_pool_version *pver = rop->dg_pver;

	M0_ASSERT(pver != NULL);
	return pver->pv_mach.pm_state->pst_max_device_failures;
}

static uint32_t dix_rec_op_spare_offset(struct m0_dix_rec_op *rec_op)
{
	return m0_dix_liter_spare_offset(&rec_op->dgp_iter);
}

static int dix__spare_target(struct m0_dix_rec_op         *rec_op,
			     const struct m0_dix_pg_unit  *failed_unit,
			     uint32_t                     *spare_slot,
			     struct m0_dix_pg_unit       **spare_unit,
			     bool                          with_data)
{
	struct m0_pool_version    *pver;
	struct m0_poolmach_state  *pm_state;
	struct m0_dix_pg_unit     *spare;
	uint32_t                   slot;
	uint64_t                   spare_offset;
	uint64_t                   tgt;
	int                        rc;

	/*
	 * Pool machine should be locked here. It is done in
	 * dix_rop_units_set().
	 */
	pver = dix_rec_op_pver(rec_op);
	pm_state = pver->pv_mach.pm_state;
	spare_offset = dix_rec_op_spare_offset(rec_op);
	M0_PRE(ergo(with_data, M0_IN(failed_unit->dpu_pd_state,
			(M0_PNDS_SNS_REPAIRED, M0_PNDS_SNS_REBALANCING))));
	tgt = failed_unit->dpu_tgt;
	do {
		rc = dix_spare_slot_find(pm_state, tgt, &slot);
		if (rc != 0)
			return M0_ERR(rc);
		spare = &rec_op->dgp_units[spare_offset + slot];
		if (!spare->dpu_failed) {
			/* Found non-failed spare unit, exit the loop. */
			*spare_unit = spare;
			*spare_slot = slot;
			return M0_RC(0);
		}
		if (with_data && M0_IN(spare->dpu_pd_state,
				(M0_PNDS_FAILED, M0_PNDS_SNS_REPAIRING))) {
			/*
			 * Spare unit with repaired data is requested, but some
			 * spare unit in a chain is not repaired yet.
			 */
			return M0_ERR(-ENODEV);
		}
		tgt = spare->dpu_tgt;
	} while (1);
}

static int dix_spare_target(struct m0_dix_rec_op         *rec_op,
			    const struct m0_dix_pg_unit  *failed_unit,
			    uint32_t                     *spare_slot,
			    struct m0_dix_pg_unit       **spare_unit)
{
	return dix__spare_target(rec_op, failed_unit, spare_slot, spare_unit,
				 false);
}

static int dix_spare_target_with_data(struct m0_dix_rec_op         *rec_op,
				      const struct m0_dix_pg_unit  *failed_unit,
				      uint32_t                     *spare_slot,
				      struct m0_dix_pg_unit       **spare_unit)
{
	return dix__spare_target(rec_op, failed_unit, spare_slot, spare_unit,
				 true);
}

static void dix_online_unit_choose(struct m0_dix_req    *req,
				   struct m0_dix_rec_op *rec_op)
{
	struct m0_dix_pg_unit *pgu;
	uint64_t               start_unit;
	uint64_t               i;
	uint64_t               j;

	M0_ENTRY();
	M0_PRE(req->dr_type == DIX_GET);
	start_unit = req->dr_items[rec_op->dgp_item].dxi_pg_unit;
	M0_ASSERT(start_unit < dix_rec_op_spare_offset(rec_op));
	for (i = 0; i < start_unit; i++)
		rec_op->dgp_units[i].dpu_failed = true;
	for (i = start_unit; i < rec_op->dgp_units_nr; i++) {
		pgu = &rec_op->dgp_units[i];
		if (!pgu->dpu_is_spare && !pgu->dpu_failed)
			break;
	}
	for (j = i + 1; j < rec_op->dgp_units_nr; j++)
		rec_op->dgp_units[j].dpu_failed = true;
}

static void dix_pg_unit_pd_assign(struct m0_dix_pg_unit *pgu,
				  struct m0_pooldev     *pd)
{
	pgu->dpu_tgt      = pd->pd_index;
	pgu->dpu_sdev_idx = pd->pd_sdev_idx;
	pgu->dpu_pd_state = pd->pd_state;
	pgu->dpu_failed   = pool_failed_devs_tlink_is_in(pd);
}

/**
 * Determines targets for the parity group 'unit' with the target device known
 * to be non-online. Record operation units (rec_op->dgp_units[]) for the
 * resulting targets are updated accordingly (usually dpu_is_spare flag is unset
 * to indicate that spare unit target should be used instead of the failed one).
 */
static void dix_rop_failed_unit_tgt(struct m0_dix_req    *req,
				    struct m0_dix_rec_op *rec_op,
				    uint64_t              unit)
{
	struct m0_dix_pg_unit *pgu = &rec_op->dgp_units[unit];
	struct m0_dix_pg_unit *spare;
	uint32_t               spare_offset;
	uint32_t               spare_slot;
	int                    rc;

	M0_ENTRY();
	M0_PRE(dix_req_state(req) != DIXREQ_DEL_PHASE2);
	M0_PRE(pgu->dpu_failed);
	M0_PRE(M0_IN(pgu->dpu_pd_state, (M0_PNDS_FAILED,
					 M0_PNDS_SNS_REPAIRING,
					 M0_PNDS_SNS_REPAIRED,
					 M0_PNDS_SNS_REBALANCING)));
	switch (req->dr_type) {
	case DIX_NEXT:
		/* Do nothing. */
		break;
	case DIX_GET:
		if (M0_IN(pgu->dpu_pd_state, (M0_PNDS_SNS_REPAIRED,
					      M0_PNDS_SNS_REBALANCING))) {
			rc = dix_spare_target_with_data(rec_op, pgu,
					&spare_slot, &spare);
			if (rc == 0) {
				spare->dpu_is_spare = false;
				break;
			}
		}
		break;
	case DIX_PUT:
		if (pgu->dpu_pd_state == M0_PNDS_SNS_REBALANCING)
			pgu->dpu_failed = false;
		rc = dix_spare_target(rec_op, pgu, &spare_slot, &spare);
		if (rc == 0) {
			spare_offset = dix_rec_op_spare_offset(rec_op);
			unit = spare_offset + spare_slot;
			rec_op->dgp_units[unit].dpu_is_spare = false;
		}
		break;
	case DIX_DEL:
		if (pgu->dpu_pd_state == M0_PNDS_FAILED)
			break;
		rc = dix_spare_target(rec_op, pgu, &spare_slot, &spare);
		if (rc != 0)
			break;
		spare_offset = dix_rec_op_spare_offset(rec_op);
		unit = spare_offset + spare_slot;
		if (pgu->dpu_pd_state == M0_PNDS_SNS_REPAIRED) {
			rec_op->dgp_units[unit].dpu_is_spare = false;
		} else if (pgu->dpu_pd_state == M0_PNDS_SNS_REPAIRING) {
			rec_op->dgp_units[unit].dpu_del_phase2 = true;
		} else if (pgu->dpu_pd_state == M0_PNDS_SNS_REBALANCING) {
			rec_op->dgp_units[unit].dpu_is_spare = false;
			pgu->dpu_del_phase2 = true;
		}
		break;
	default:
		M0_IMPOSSIBLE("Invalid request type %d", req->dr_type);
	}
	M0_LEAVE();
}

/**
 * For every unit that is unaccessible (resides on a non-online device)
 * determines corresponding spare unit. Depending on DIX request type and device
 * states, record units are updated in order to skip the failed unit, to use the
 * spare unit or to use both the failed and the spare units.
 *
 * For more information see dix/client.h, "Operation in degraded mode" section.
 */
static void dix_rop_failures_analyse(struct m0_dix_req *req)
{
	struct m0_dix_rop_ctx *rop = req->dr_rop;
	struct m0_dix_rec_op  *rec_op;
	struct m0_dix_pg_unit *unit;
	uint32_t               i;
	uint32_t               j;

	for (i = 0; i < rop->dg_rec_ops_nr; i++) {
		rec_op = &rop->dg_rec_ops[i];
		for (j = 0; j < rec_op->dgp_units_nr; j++) {
			unit = &rec_op->dgp_units[j];
			if (!unit->dpu_is_spare && unit->dpu_failed) {
				rec_op->dgp_failed_devs_nr++;
				dix_rop_failed_unit_tgt(req, rec_op, j);
			}
		}
	}
}

static void dix_rop_units_set(struct m0_dix_req *req)
{
	struct m0_dix_rop_ctx *rop = req->dr_rop;
	struct m0_dix_rec_op  *rec_op;
	struct m0_dix_pg_unit *unit;
	struct m0_pooldev     *pd;
	struct m0_poolmach    *pm = &rop->dg_pver->pv_mach;
	struct m0_pool        *pool = rop->dg_pver->pv_pool;
	uint64_t               tgt;
	uint32_t               i;
	uint32_t               j;

	m0_rwlock_read_lock(&pm->pm_lock);

	/*
	 * Determine destination devices for all records for all units as it
	 * should be without failures in a pool.
	 */
	for (i = 0; i < rop->dg_rec_ops_nr; i++) {
		rec_op = &rop->dg_rec_ops[i];
		dix_rop_tgt_iter_begin(req, rec_op);
		for (j = 0; j < rec_op->dgp_units_nr; j++) {
			unit = &rec_op->dgp_units[j];
			dix_rop_tgt_iter_next(req, rec_op, &tgt,
					      &unit->dpu_is_spare);
			pd = m0_dix_tgt2sdev(&rec_op->dgp_iter.dit_linst, tgt);
			dix_pg_unit_pd_assign(unit, pd);
		}
	}

	/*
	 * Analyse failures in a pool and modify individual units state
	 * in order to send CAS requests to proper destinations. Hold pool
	 * machine lock to get consistent results.
	 */
	if (pm->pm_pver->pv_is_dirty &&
	    !pool_failed_devs_tlist_is_empty(&pool->po_failed_devices))
		dix_rop_failures_analyse(req);

	m0_rwlock_read_unlock(&pm->pm_lock);

	/*
	 * Only one CAS GET request should be sent for every record.
	 * Choose the best destination for every record.
	 */
	if (req->dr_type == DIX_GET) {
		for (i = 0; i < rop->dg_rec_ops_nr; i++)
			dix_online_unit_choose(req, &rop->dg_rec_ops[i]);
	}
}

static bool dix_pg_unit_skip(struct m0_dix_req     *req,
			     struct m0_dix_pg_unit *unit)
{
	if (dix_req_state(req) != DIXREQ_DEL_PHASE2)
		return unit->dpu_failed || unit->dpu_is_spare;
	else
		return !unit->dpu_del_phase2;
}

static int dix_cas_rops_alloc(struct m0_dix_req *req)
{
	struct m0_dix_rop_ctx  *rop = req->dr_rop;
	struct m0_dix_rec_op   *rec_op;
	uint32_t                i;
	uint32_t                j;
	uint32_t                max_failures;
	struct m0_dix_cas_rop **map = rop->dg_target_rop;
	struct m0_dix_cas_rop  *cas_rop;
	struct m0_dix_pg_unit  *unit;
	bool                    del_lock;
	int                     rc = 0;

	M0_ENTRY("req %p", req);
	M0_ASSERT(rop->dg_rec_ops_nr > 0);

	max_failures = dix_rop_max_failures(rop);
	for (i = 0; i < rop->dg_rec_ops_nr; i++) {
		rec_op = &rop->dg_rec_ops[i];
		/*
		 * If 2-phase delete is necessary, then CAS request should be
		 * sent with COF_DEL_LOCK flag in order to prevent possible
		 * concurrency issues with repair/re-balance process.
		 */
		del_lock = (req->dr_type == DIX_DEL &&
			    dix_del_phase2_is_needed(rec_op));
		if (rec_op->dgp_failed_devs_nr > max_failures) {
			req->dr_items[rec_op->dgp_item].dxi_rc = M0_ERR(-EIO);
			/* Skip this record operation. */
			continue;
		}
		for (j = 0; j < rec_op->dgp_units_nr; j++) {
			unit = &rec_op->dgp_units[j];
			if (dix_pg_unit_skip(req, unit))
				continue;
			if (map[unit->dpu_tgt] == NULL) {
				rc = dix_cas_rop_alloc(req, unit->dpu_sdev_idx,
						       &cas_rop);
				if (rc != 0)
					goto end;
				map[unit->dpu_tgt] = cas_rop;
			}
			if (del_lock)
				map[unit->dpu_tgt]->crp_flags |= COF_DEL_LOCK;
			map[unit->dpu_tgt]->crp_keys_nr++;
		}
	}

	/* It is possible that all data units are not available. */
	if (cas_rop_tlist_is_empty(&rop->dg_cas_reqs))
		return M0_ERR(-EIO);

	m0_tl_for(cas_rop, &rop->dg_cas_reqs, cas_rop) {
		M0_ALLOC_ARR(cas_rop->crp_attrs, cas_rop->crp_keys_nr);
		if (cas_rop->crp_attrs == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto end;
		}
		rc = m0_bufvec_empty_alloc(&cas_rop->crp_keys,
					   cas_rop->crp_keys_nr);
		if (rc != 0)
			goto end;
		if (req->dr_type == DIX_PUT) {
			rc = m0_bufvec_empty_alloc(&cas_rop->crp_vals,
						   cas_rop->crp_keys_nr);
			if (rc != 0)
				goto end;
		}
		cas_rop->crp_cur_key = 0;
	} m0_tl_endfor;

end:
	if (rc != 0) {
		dix_cas_rops_fini(&rop->dg_cas_reqs);
		return M0_ERR(rc);
	}
	return M0_RC(0);
}

static int dix_cas_rops_fill(struct m0_dix_req *req)
{
	struct m0_dix_rop_ctx  *rop = req->dr_rop;
	struct m0_dix_cas_rop **map = rop->dg_target_rop;
	struct m0_dix_rec_op   *rec_op;
	uint32_t                j;
	uint32_t                i;
	uint64_t                tgt;
	uint64_t                item;
	struct m0_bufvec       *keys;
	struct m0_bufvec       *vals;
	struct m0_buf          *key;
	uint32_t                idx;
	struct m0_dix_pg_unit  *unit;

	M0_ENTRY("req %p", req);
	for (i = 0; i < rop->dg_rec_ops_nr; i++) {
		rec_op = &rop->dg_rec_ops[i];
		item = rec_op->dgp_item;
		for (j = 0; j < rec_op->dgp_units_nr; j++) {
			unit = &rec_op->dgp_units[j];
			tgt = unit->dpu_tgt;
			if (dix_pg_unit_skip(req, unit))
				continue;
			M0_ASSERT(map[tgt] != NULL);
			keys = &map[tgt]->crp_keys;
			vals = &map[tgt]->crp_vals;
			key = &rec_op->dgp_key;
			idx = map[tgt]->crp_cur_key;
			keys->ov_vec.v_count[idx] = key->b_nob;
			keys->ov_buf[idx]         = key->b_addr;
			if (req->dr_type == DIX_PUT) {
				vals->ov_vec.v_count[idx] =
					req->dr_vals->ov_vec.v_count[item];
				vals->ov_buf[idx] =
					req->dr_vals->ov_buf[item];
			}
			map[tgt]->crp_attrs[idx].cra_item = item;
			map[tgt]->crp_cur_key++;
		}
	}
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_put(struct m0_dix_req      *req,
			   const struct m0_dix    *index,
			   const struct m0_bufvec *keys,
			   const struct m0_bufvec *vals,
			   struct       m0_dtx    *dtx,
			   uint32_t                flags)
{
	uint32_t keys_nr = keys->ov_vec.v_nr;
	int      rc;

	M0_PRE(keys->ov_vec.v_nr == vals->ov_vec.v_nr);
	M0_PRE(keys_nr != 0);
	/* Only overwrite, crow and sync_wait flags are allowed. */
	M0_PRE((flags & ~(COF_OVERWRITE | COF_CROW | COF_SYNC_WAIT)) == 0);
	rc = dix_req_indices_copy(req, index, 1);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ALLOC_ARR(req->dr_items, keys_nr);
	if (req->dr_items == NULL)
		return M0_ERR(-ENOMEM);
	req->dr_items_nr = keys_nr;
	req->dr_keys = keys;
	req->dr_vals = vals;
	req->dr_dtx = dtx;
	req->dr_type = DIX_PUT;
	req->dr_flags = flags;
	dix_discovery(req);
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_get(struct m0_dix_req      *req,
			   const struct m0_dix    *index,
			   const struct m0_bufvec *keys)
{
	uint32_t keys_nr = keys->ov_vec.v_nr;
	int      rc;

	M0_PRE(keys_nr != 0);
	rc = dix_req_indices_copy(req, index, 1);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ALLOC_ARR(req->dr_items, keys_nr);
	if (req->dr_items == NULL)
		return M0_ERR(-ENOMEM);
	req->dr_items_nr = keys_nr;
	req->dr_keys = keys;
	req->dr_type = DIX_GET;
	dix_discovery(req);
	return M0_RC(0);
}

M0_INTERNAL void m0_dix_get_rep(const struct m0_dix_req *req,
				uint64_t                 idx,
				struct m0_dix_get_reply *rep)
{
	M0_PRE(m0_dix_generic_rc(req) == 0);
	M0_PRE(idx < req->dr_items_nr);
	rep->dgr_rc  = req->dr_items[idx].dxi_rc;
	rep->dgr_val = req->dr_items[idx].dxi_val;
}

M0_INTERNAL int m0_dix_del(struct m0_dix_req      *req,
			   const struct m0_dix    *index,
			   const struct m0_bufvec *keys,
			   struct m0_dtx          *dtx,
			   uint32_t                flags)
{
	uint32_t keys_nr = keys->ov_vec.v_nr;
	int      rc;

	M0_PRE(keys_nr != 0);
	/* Only sync_wait flag is allowed. */
	M0_PRE((flags & ~(COF_SYNC_WAIT)) == 0);
	rc = dix_req_indices_copy(req, index, 1);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ALLOC_ARR(req->dr_items, keys_nr);
	if (req->dr_items == NULL)
		return M0_ERR(-ENOMEM);
	req->dr_items_nr = keys_nr;
	req->dr_keys = keys;
	req->dr_dtx = dtx;
	req->dr_type = DIX_DEL;
	req->dr_flags = flags;
	dix_discovery(req);
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_next(struct m0_dix_req      *req,
			    const struct m0_dix    *index,
			    const struct m0_bufvec *start_keys,
			    const uint32_t         *recs_nr,
			    uint32_t                flags)
{
	uint32_t keys_nr = start_keys->ov_vec.v_nr;
	uint32_t i;
	int      rc;

	/* Only slant and exclude start key flags are allowed. */
	M0_PRE((flags & ~(COF_SLANT | COF_EXCLUDE_START_KEY)) == 0);
	M0_PRE(keys_nr != 0);

	rc = dix_req_indices_copy(req, index, 1);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ALLOC_ARR(req->dr_items, keys_nr);
	M0_ALLOC_ARR(req->dr_recs_nr, keys_nr);
	if (req->dr_items == NULL || req->dr_recs_nr == NULL)
		/*
		 * Memory will be deallocated in m0_dix_req_fini() if necessary.
		 */
		return M0_ERR(-ENOMEM);
	req->dr_items_nr = keys_nr;
	req->dr_keys     = start_keys;
	req->dr_type     = DIX_NEXT;
	req->dr_flags    = flags;
	for (i = 0; i < keys_nr; i++)
		req->dr_recs_nr[i] = recs_nr[i];
	dix_discovery(req);
	return 0;
}

M0_INTERNAL void m0_dix_next_rep(const struct m0_dix_req  *req,
				 uint64_t                  key_idx,
				 uint64_t                  val_idx,
				 struct m0_dix_next_reply *rep)
{
	const struct m0_dix_next_resultset  *rs = &req->dr_rs;
	struct m0_dix_next_results          *res;
	struct m0_cas_next_reply           **reps;

	M0_ASSERT(rs != NULL);
	M0_ASSERT(key_idx < rs->nrs_res_nr);
	res  = &rs->nrs_res[key_idx];
	reps = res->drs_reps;
	M0_ASSERT(val_idx < res->drs_pos);
	M0_ASSERT(reps[val_idx]->cnp_rc == 0);
	rep->dnr_key = reps[val_idx]->cnp_key;
	rep->dnr_val = reps[val_idx]->cnp_val;
}

M0_INTERNAL uint32_t m0_dix_next_rep_nr(const struct m0_dix_req *req,
					uint64_t                 key_idx)
{
	M0_ASSERT(key_idx < req->dr_rs.nrs_res_nr);
	return req->dr_rs.nrs_res[key_idx].drs_pos;
}

M0_INTERNAL int m0_dix_item_rc(const struct m0_dix_req *req,
			       uint64_t                 idx)
{
	M0_PRE(m0_dix_generic_rc(req) == 0);
	M0_PRE(idx < m0_dix_req_nr(req));
	return req->dr_items[idx].dxi_rc;
}

M0_INTERNAL int m0_dix_generic_rc(const struct m0_dix_req *req)
{
	M0_PRE(M0_IN(dix_req_state(req), (DIXREQ_FINAL, DIXREQ_FAILURE)));
	return M0_RC(req->dr_sm.sm_rc);
}

M0_INTERNAL int m0_dix_req_rc(const struct m0_dix_req *req)
{
	int rc;
	int i;

	rc = m0_dix_generic_rc(req);
	if (rc == 0)
		for (i = 0; i < m0_dix_req_nr(req); i++) {
			rc = m0_dix_item_rc(req, i);
			if (rc != 0)
				break;
		}
	return M0_RC(rc);
}

M0_INTERNAL uint64_t m0_dix_req_nr(const struct m0_dix_req *req)
{
	return req->dr_items_nr;
}

M0_INTERNAL void m0_dix_get_rep_mlock(struct m0_dix_req *req, uint64_t idx)
{
	M0_PRE(dix_req_state(req) == DIXREQ_FINAL);
	M0_PRE(req->dr_type == DIX_GET);
	M0_PRE(idx < req->dr_items_nr);

	req->dr_items[idx].dxi_key = M0_BUF_INIT0;
	req->dr_items[idx].dxi_val = M0_BUF_INIT0;
}

M0_INTERNAL void m0_dix_next_rep_mlock(struct m0_dix_req *req,
				       uint32_t           key_idx,
				       uint32_t           val_idx)
{
	struct m0_dix_next_resultset  *rs = &req->dr_rs;
	struct m0_dix_next_results    *res;
	struct m0_cas_next_reply     **reps;

	M0_PRE(dix_req_state(req) == DIXREQ_FINAL);
	M0_PRE(req->dr_type == DIX_NEXT);
	M0_PRE(rs != NULL);
	M0_PRE(key_idx < rs->nrs_res_nr);
	res  = &rs->nrs_res[key_idx];
	reps = res->drs_reps;
	M0_PRE(val_idx < res->drs_pos);
	reps[val_idx]->cnp_val = M0_BUF_INIT0;
	reps[val_idx]->cnp_key = M0_BUF_INIT0;
}

static void dix_item_fini(const struct m0_dix_req *req,
			  struct m0_dix_item      *item)
{
	switch(req->dr_type){
	case DIX_NEXT:
		m0_buf_free(&item->dxi_key);
		/* Fall through. */
	case DIX_GET:
		m0_buf_free(&item->dxi_val);
		break;
	default:
		break;
	}
}

M0_INTERNAL void m0_dix_req_fini(struct m0_dix_req *req)
{
	uint32_t i;

	M0_PRE(m0_dix_req_is_locked(req));
	for (i = 0; i < req->dr_indices_nr; i++)
		m0_dix_fini(&req->dr_indices[i]);
	m0_free(req->dr_indices);
	M0_ASSERT((req->dr_orig_indices != NULL) ==
		  (req->dr_type == DIX_CREATE));
	if (req->dr_orig_indices != NULL) {
		for (i = 0; i < req->dr_indices_nr; i++)
			m0_dix_fini(&req->dr_orig_indices[i]);
		m0_free(req->dr_orig_indices);
	}
	for (i = 0; i < req->dr_items_nr; i++)
		dix_item_fini(req, &req->dr_items[i]);
	m0_free(req->dr_items);
	m0_free(req->dr_recs_nr);
	m0_free(req->dr_rop);
	m0_dix_rs_fini(&req->dr_rs);
	m0_sm_fini(&req->dr_sm);
}

M0_INTERNAL void m0_dix_req_fini_lock(struct m0_dix_req *req)
{
	struct m0_sm_group *grp = dix_req_smgrp(req);

	M0_PRE(!m0_dix_req_is_locked(req));
	m0_sm_group_lock(grp);
	m0_dix_req_fini(req);
	m0_sm_group_unlock(grp);
}

M0_INTERNAL int m0_dix_copy(struct m0_dix *dst, const struct m0_dix *src)
{
	*dst = *src;
	if (src->dd_layout.dl_type == DIX_LTYPE_DESCR)
		return m0_dix_ldesc_copy(&dst->dd_layout.u.dl_desc,
					 &src->dd_layout.u.dl_desc);
	return 0;
}

M0_INTERNAL int m0_dix_desc_set(struct m0_dix             *dix,
				const struct m0_dix_ldesc *desc)
{
	dix->dd_layout.dl_type = DIX_LTYPE_DESCR;
	return m0_dix_ldesc_copy(&dix->dd_layout.u.dl_desc, desc);
}

M0_INTERNAL void m0_dix_fini(struct m0_dix *dix)
{
	if (dix->dd_layout.dl_type == DIX_LTYPE_DESCR)
		m0_dix_ldesc_fini(&dix->dd_layout.u.dl_desc);
}

M0_INTERNAL int  m0_dix_sm_conf_init(void)
{
	m0_sm_conf_init(&dix_req_sm_conf);
	return m0_sm_addb2_init(&dix_req_sm_conf,
				M0_AVI_DIX_SM_REQ,
				M0_AVI_DIX_SM_REQ_COUNTER);
}

M0_INTERNAL void m0_dix_sm_conf_fini(void)
{
	m0_sm_addb2_fini(&dix_req_sm_conf);
	m0_sm_conf_fini(&dix_req_sm_conf);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of dix group */

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
