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
 * Original creation date: 11-Apr-2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS

#include "lib/trace.h"
#include "lib/vec.h"
#include "lib/misc.h"    /* M0_IN */
#include "lib/memory.h"
#include "sm/sm.h"
#include "fid/fid.h"     /* m0_fid */
#include "rpc/item.h"
#include "rpc/rpc.h"     /* m0_rpc_post */
#include "rpc/session.h" /* m0_rpc_session */
#include "rpc/conn.h"    /* m0_rpc_conn */
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "cas/cas.h"
#include "cas/cas_xc.h"
#include "cas/client.h"
#include "lib/finject.h"
#include "cas/cas_addb2.h"

/**
 * @addtogroup cas-client
 * @{
 */

/**
 * Iterator to walk over responses for CUR request.
 *
 * On every iteration two values are updated:
 * - Response record (creq_niter::cni_rep).
 * - Request record (creq_niter::cni_req) containing starting key related to
 *   response record.
 *
 * Usually there are several response records for one request record.
 * See m0_cas_rec::cr_rc for more information about CUR reply format.
 */
struct creq_niter {
	/** Current iteration values accessible by user. */
	struct m0_cas_rec  *cni_req;
	struct m0_cas_rec  *cni_rep;

	/** Private fields. */
	struct m0_cas_recv *cni_reqv;
	struct m0_cas_recv *cni_repv;
	uint64_t            cni_req_i;
	uint64_t            cni_rep_i;
	uint64_t            cni_kpos;
};

#define CASREQ_FOP_DATA(fop) ((struct m0_cas_op *)m0_fop_data(fop))

static void cas_req_replied_cb(struct m0_rpc_item *item);

static const struct m0_rpc_item_ops cas_item_ops = {
	.rio_sent    = NULL,
	.rio_replied = cas_req_replied_cb
};

static void creq_asmbl_replied_cb(struct m0_rpc_item *item);

static const struct m0_rpc_item_ops asmbl_item_ops = {
	.rio_sent    = NULL,
	.rio_replied = creq_asmbl_replied_cb
};

static struct m0_sm_state_descr cas_req_states[] = {
	[CASREQ_INIT] = {
		.sd_flags     = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(CASREQ_SENT, CASREQ_FRAGM_SENT)
	},
	[CASREQ_SENT] = {
		.sd_name      = "request-sent",
		.sd_allowed   = M0_BITS(CASREQ_FINAL, CASREQ_FAILURE,
					CASREQ_ASSEMBLY),
	},
	[CASREQ_FRAGM_SENT] = {
		.sd_name      = "request-fragment-sent",
		.sd_allowed   = M0_BITS(CASREQ_FINAL, CASREQ_FAILURE,
					CASREQ_ASSEMBLY),
	},
	[CASREQ_ASSEMBLY] = {
		.sd_name      = "assembly",
		.sd_allowed   = M0_BITS(CASREQ_FRAGM_SENT, CASREQ_FINAL,
					CASREQ_FAILURE),
	},
	[CASREQ_FINAL] = {
		.sd_name      = "final",
		.sd_flags     = M0_SDF_TERMINAL,
	},
	[CASREQ_FAILURE] = {
		.sd_name      = "failure",
		.sd_flags     = M0_SDF_TERMINAL | M0_SDF_FAILURE
	}
};

static struct m0_sm_trans_descr cas_req_trans[] = {
	{ "send-over-rpc",       CASREQ_INIT,       CASREQ_SENT       },
	{ "send-fragm-over-rpc", CASREQ_INIT,       CASREQ_FRAGM_SENT },
	{ "rpc-failure",         CASREQ_SENT,       CASREQ_FAILURE    },
	{ "assembly",            CASREQ_SENT,       CASREQ_ASSEMBLY   },
	{ "req-processed",       CASREQ_SENT,       CASREQ_FINAL      },
	{ "rpc-failure",         CASREQ_FRAGM_SENT, CASREQ_FAILURE    },
	{ "fragm-assembly",      CASREQ_FRAGM_SENT, CASREQ_ASSEMBLY   },
	{ "req-processed",       CASREQ_FRAGM_SENT, CASREQ_FINAL      },
	{ "assembly-fail",       CASREQ_ASSEMBLY,   CASREQ_FAILURE    },
	{ "assembly-done",       CASREQ_ASSEMBLY,   CASREQ_FINAL      },
	{ "assembly-fragm",      CASREQ_ASSEMBLY,   CASREQ_FRAGM_SENT },
};

struct m0_sm_conf cas_req_sm_conf = {
	.scf_name      = "cas_req",
	.scf_nr_states = ARRAY_SIZE(cas_req_states),
	.scf_state     = cas_req_states,
	.scf_trans_nr  = ARRAY_SIZE(cas_req_trans),
	.scf_trans     = cas_req_trans
};

static int cas_req_fragmentation(struct m0_cas_req *req);
static int cas_req_fragment_continue(struct m0_cas_req *req,
				     struct m0_cas_op  *op);
static void creq_recv_fini(struct m0_cas_recv *recv, bool op_is_meta);

static void cas_to_rpc_map(const struct m0_cas_req  *creq,
			   const struct m0_rpc_item *item)
{
	uint64_t cid = m0_sm_id_get(&creq->ccr_sm);
	uint64_t iid = m0_sm_id_get(&item->ri_sm);
	M0_ADDB2_ADD(M0_AVI_CAS_TO_RPC, cid, iid);
}


static bool fid_is_meta(struct m0_fid *fid)
{
	M0_PRE(fid != NULL);
	return m0_fid_eq(fid, &m0_cas_meta_fid);
}

static int creq_op_alloc(uint64_t           recs_nr,
			 struct m0_cas_op **out)
{
	struct m0_cas_op  *op;
	struct m0_cas_rec *rec;

	if (M0_FI_ENABLED("cas_alloc_fail"))
		return M0_ERR(-ENOMEM);

	M0_ALLOC_PTR(op);
	M0_ALLOC_ARR(rec, recs_nr);
	if (op == NULL || rec == NULL) {
		m0_free(op);
		m0_free(rec);
		return M0_ERR(-ENOMEM);
	} else {
		op->cg_rec.cr_nr = recs_nr;
		op->cg_rec.cr_rec = rec;
		*out = op;
	}
	return M0_RC(0);
}

static void creq_op_free(struct m0_cas_op *op)
{
	if (op != NULL) {
		m0_free(op->cg_rec.cr_rec);
		m0_free(op);
	}
}

M0_INTERNAL void m0_cas_req_init(struct m0_cas_req     *req,
				 struct m0_rpc_session *sess,
				 struct m0_sm_group    *grp)
{
	M0_ENTRY();
	M0_PRE(sess != NULL);
	M0_PRE(M0_IS0(req));
	req->ccr_sess = sess;
	m0_sm_init(&req->ccr_sm, &cas_req_sm_conf, CASREQ_INIT, grp);
	m0_sm_addb2_counter_init(&req->ccr_sm);
	M0_LEAVE();
}

static struct m0_rpc_conn *creq_rpc_conn(const struct m0_cas_req *req)
{
	return req->ccr_sess->s_conn;
}

static struct m0_rpc_machine *creq_rpc_mach(const struct m0_cas_req *req)
{
	return creq_rpc_conn(req)->c_rpc_machine;
}

static struct m0_sm_group *cas_req_smgrp(const struct m0_cas_req *req)
{
	return req->ccr_sm.sm_grp;
}

M0_INTERNAL void m0_cas_req_lock(struct m0_cas_req *req)
{
	M0_ENTRY();
	m0_sm_group_lock(cas_req_smgrp(req));
}

M0_INTERNAL void m0_cas_req_unlock(struct m0_cas_req *req)
{
	M0_ENTRY();
	m0_sm_group_unlock(cas_req_smgrp(req));
}

M0_INTERNAL bool m0_cas_req_is_locked(const struct m0_cas_req *req)
{
	return m0_mutex_is_locked(&cas_req_smgrp(req)->s_lock);
}

static void cas_req_state_set(struct m0_cas_req     *req,
			      enum m0_cas_req_state  state)
{
	M0_LOG(M0_DEBUG, "CAS req: %p, state change:[%s -> %s]\n",
	       req, m0_sm_state_name(&req->ccr_sm, req->ccr_sm.sm_state),
	       m0_sm_state_name(&req->ccr_sm, state));
	m0_sm_state_set(&req->ccr_sm, state);
}

static void cas_req_reply_fini(struct m0_cas_req *req)
{
	struct m0_cas_recv *recv = &req->ccr_reply.cgr_rep;
	uint64_t            i;

	for (i = 0; i < recv->cr_nr; i++) {
		m0_rpc_at_fini(&recv->cr_rec[i].cr_key);
		m0_rpc_at_fini(&recv->cr_rec[i].cr_val);
	}
	m0_free(req->ccr_reply.cgr_rep.cr_rec);
}

static void cas_req_fini(struct m0_cas_req *req)
{
	uint32_t cur_state = req->ccr_sm.sm_state;

	M0_ENTRY();
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE(M0_IN(cur_state, (CASREQ_INIT, CASREQ_FINAL, CASREQ_FAILURE)));
	if (cur_state == CASREQ_FAILURE) {
		if (req->ccr_reply_item != NULL)
			m0_rpc_item_put_lock(req->ccr_reply_item);
		if (req->ccr_fop != NULL) {
			req->ccr_fop->f_data.fd_data = NULL;
			m0_fop_put_lock(req->ccr_fop);
		}
	}
	if (req->ccr_req_op != NULL) {
		/* Restore records vector for proper freeing. */
		req->ccr_req_op->cg_rec = req->ccr_rec_orig;
		creq_recv_fini(&req->ccr_rec_orig, req->ccr_is_meta);
		creq_op_free(req->ccr_req_op);
	}
	cas_req_reply_fini(req);
	m0_free(req->ccr_asmbl_ikeys);
	m0_sm_fini(&req->ccr_sm);
	M0_LEAVE();
}

M0_INTERNAL void m0_cas_req_fini(struct m0_cas_req *req)
{
	M0_PRE(m0_cas_req_is_locked(req));
	cas_req_fini(req);
	M0_SET0(req);
}

M0_INTERNAL void m0_cas_req_fini_lock(struct m0_cas_req *req)
{
	M0_ENTRY();
	m0_cas_req_lock(req);
	cas_req_fini(req);
	m0_cas_req_unlock(req);
	M0_SET0(req);
	M0_LEAVE();
}

/**
 * Assigns pointers to record key/value to NULL, so they are not
 * deallocated during RPC AT buffer finalisation. User is responsible for
 * their deallocation.
 */
static void creq_kv_hold_down(struct m0_cas_rec *rec)
{
	struct m0_rpc_at_buf *key = &rec->cr_key;
	struct m0_rpc_at_buf *val = &rec->cr_val;

	M0_ENTRY();
	M0_PRE(!m0_rpc_at_is_set(key) ||
	       M0_IN(key->ab_type, (M0_RPC_AT_INLINE, M0_RPC_AT_BULK_SEND)));
	if (m0_rpc_at_is_set(key))
		m0_rpc_at_detach(key);
	if (m0_rpc_at_is_set(val))
		m0_rpc_at_detach(val);
	M0_LEAVE();
}

/**
 * Finalises CAS record vector that is intended to be sent as part of the
 * CAS request.
 */
static void creq_recv_fini(struct m0_cas_recv *recv, bool op_is_meta)
{
	struct m0_cas_rec *rec;
	struct m0_cas_kv  *kv;
	uint64_t           i;
	uint64_t           k;

	for (i = 0; i < recv->cr_nr; i++) {
		rec = &recv->cr_rec[i];
		/*
		 * CAS client does not copy keys/values provided by user if
		 * it works with non-meta index, otherwise it encodes keys
		 * and places them in buffers allocated by itself.
		 * Save keys/values in memory in the first case, free in
		 * the second.
		 */
		if (!op_is_meta)
			creq_kv_hold_down(rec);
		m0_rpc_at_fini(&rec->cr_key);
		m0_rpc_at_fini(&rec->cr_val);
		for (k = 0; k < rec->cr_kv_bufs.cv_nr; k++) {
			kv = &rec->cr_kv_bufs.cv_rec[k];
			m0_rpc_at_fini(&kv->ck_key);
			m0_rpc_at_fini(&kv->ck_val);
		}
	}

}

static void creq_fop_destroy(struct m0_cas_req *req)
{
	struct m0_fop *fop = req->ccr_fop;

	fop->f_data.fd_data = NULL;
	m0_fop_fini(fop);
	m0_free(fop);
	req->ccr_fop = NULL;
}

static void creq_fop_release(struct m0_ref *ref)
{
	struct m0_fop *fop;

	M0_ENTRY();
	M0_PRE(ref != NULL);
	fop = container_of(ref, struct m0_fop, f_ref);
	m0_fop_fini(fop);
	m0_free(fop);
	M0_LEAVE();
}

static void creq_asmbl_fop_release(struct m0_ref *ref)
{
	struct m0_fop    *fop;
	struct m0_cas_op *op;

	M0_ENTRY();
	M0_PRE(ref != NULL);
	fop = container_of(ref, struct m0_fop, f_ref);
	op = CASREQ_FOP_DATA(fop);
	creq_recv_fini(&op->cg_rec, fid_is_meta(&op->cg_id.ci_fid));
	m0_fop_fini(fop);
	M0_LEAVE();
}

static int creq_fop_create(struct m0_cas_req  *req,
			   struct m0_fop_type *ftype,
			   struct m0_cas_op   *op)
{
	struct m0_fop *fop;

	M0_ALLOC_PTR(fop);
	if (fop == NULL)
		return M0_ERR(-ENOMEM);

	m0_fop_init(fop, ftype, (void *)op, creq_fop_release);
	fop->f_opaque = req;
	req->ccr_fop = fop;
	req->ccr_ftype = ftype;

	return 0;
}

static int creq_fop_create_and_prepare(struct m0_cas_req     *req,
				       struct m0_fop_type    *ftype,
				       struct m0_cas_op      *op,
				       enum m0_cas_req_state *next_state)
{
	int rc;

	rc = creq_fop_create(req, ftype, op);
	if (rc == 0) {
		*next_state = CASREQ_SENT;
		/*
		 * Check whether original fop payload does not exceed
		 * max rpc item payload.
		 */
		if (m0_rpc_item_max_payload_exceeded(
			    &req->ccr_fop->f_item,
			    req->ccr_sess)) {
			*next_state = CASREQ_FRAGM_SENT;
			rc = cas_req_fragmentation(req);
		}
		if (rc != 0)
			creq_fop_destroy(req);
	}
	return M0_RC(rc);
}

static struct m0_cas_req *item_to_cas_req(struct m0_rpc_item *item)
{
	struct m0_fop *fop = M0_AMB(fop, item, f_item);
	return (struct m0_cas_req *)fop->f_opaque;
}

static struct m0_rpc_item *cas_req_to_item(const struct m0_cas_req *req)
{
	return &req->ccr_fop->f_item;
}

static struct m0_cas_rep *cas_rep(struct m0_rpc_item *reply)
{
	return m0_fop_data(m0_rpc_item_to_fop(reply));
}

M0_INTERNAL int m0_cas_req_generic_rc(const struct m0_cas_req *req)
{
	struct m0_fop      *req_fop = req->ccr_fop;
	struct m0_rpc_item *reply;
	int                 rc;

	M0_PRE(M0_IN(req->ccr_sm.sm_state, (CASREQ_FINAL, CASREQ_FAILURE)));

	reply = req_fop != NULL ? cas_req_to_item(req)->ri_reply : NULL;

	rc = req->ccr_sm.sm_rc;
	if (rc == 0 && reply != NULL)
		rc = m0_rpc_item_generic_reply_rc(reply);
	if (rc == 0)
		rc = req->ccr_reply.cgr_rc;

	return M0_RC(rc);
}

static bool cas_rep_val_is_valid(struct m0_rpc_at_buf *val,
				 struct m0_fid *idx_fid)
{
	return m0_rpc_at_is_set(val) == !fid_is_meta(idx_fid);
}

static int cas_rep__validate(const struct m0_fop_type *ftype,
			     struct m0_cas_op         *op,
			     struct m0_cas_rep        *rep)
{
	struct m0_cas_rec *rec;
	uint64_t           sum;
	uint64_t           i;

	if (ftype == &cas_cur_fopt) {
		sum = 0;
		for (i = 0; i < op->cg_rec.cr_nr; i++)
			sum += op->cg_rec.cr_rec[i].cr_rc;
		if (rep->cgr_rep.cr_nr > sum)
			return M0_ERR(-EPROTO);
		for (i = 0; i < rep->cgr_rep.cr_nr; i++) {
			rec = &rep->cgr_rep.cr_rec[i];
			if ((int32_t)rec->cr_rc > 0 &&
			    (!m0_rpc_at_is_set(&rec->cr_key) ||
			     !cas_rep_val_is_valid(&rec->cr_val,
						   &op->cg_id.ci_fid)))
				rec->cr_rc = M0_ERR(-EPROTO);
		}
	} else {
		M0_ASSERT(M0_IN(ftype, (&cas_get_fopt, &cas_put_fopt,
					&cas_del_fopt)));
		/*
		 * CAS service guarantees equal number of records in request and
		 * response for GET,PUT, DEL operations.  Otherwise, it's not
		 * possible to match requested records with the ones in reply,
		 * because keys in reply are absent.
		 */
		if (op->cg_rec.cr_nr != rep->cgr_rep.cr_nr)
			return M0_ERR(-EPROTO);
		/*
		 * Successful GET reply for ordinary index should always contain
		 * non-empty value.
		 */
		if (ftype == &cas_get_fopt)
			for (i = 0; i < rep->cgr_rep.cr_nr; i++) {
				rec = &rep->cgr_rep.cr_rec[i];
				if (rec->cr_rc == 0 &&
				    !cas_rep_val_is_valid(&rec->cr_val,
							  &op->cg_id.ci_fid))
					rec->cr_rc = M0_ERR(-EPROTO);
			}
	}
	return M0_RC(0);
}

static int cas_rep_validate(const struct m0_cas_req *req)
{
	const struct m0_fop *rfop = req->ccr_fop;
	struct m0_cas_op    *op = m0_fop_data(rfop);
	struct m0_cas_rep   *rep = cas_rep(cas_req_to_item(req)->ri_reply);

	return rep->cgr_rc ?: cas_rep__validate(rfop->f_type, op, rep);
}

static void cas_req_failure(struct m0_cas_req *req, int32_t rc)
{
	M0_PRE(rc != 0);
	m0_sm_fail(&req->ccr_sm, CASREQ_FAILURE, rc);
}

static void cas_req_failure_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_cas_req *req = container_of(ast, struct m0_cas_req,
					      ccr_failure_ast);
	int32_t rc = (long)ast->sa_datum;

	M0_PRE(rc != 0);
	cas_req_failure(req, M0_ERR(rc));
}

static void cas_req_failure_ast_post(struct m0_cas_req *req, int32_t rc)
{
	M0_ENTRY();
	req->ccr_failure_ast.sa_cb = cas_req_failure_ast;
	req->ccr_failure_ast.sa_datum = (void *)(long)rc;
	m0_sm_ast_post(cas_req_smgrp(req), &req->ccr_failure_ast);
	M0_LEAVE();
}

static void creq_item_prepare(const struct m0_cas_req      *req,
			      struct m0_rpc_item           *item,
			      const struct m0_rpc_item_ops *ops)
{
	item->ri_rmachine = creq_rpc_mach(req);
	item->ri_ops      = ops;
	item->ri_session  = req->ccr_sess;
	item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = M0_TIME_IMMEDIATELY;
}

static void cas_fop_send(struct m0_cas_req *req)
{
	struct m0_cas_op   *op = m0_fop_data(req->ccr_fop);
	struct m0_rpc_item *item;
	int                 rc;

	M0_ENTRY();
	M0_PRE(m0_cas_req_is_locked(req));
	req->ccr_sent_recs_nr += op->cg_rec.cr_nr;
	item = cas_req_to_item(req);
	creq_item_prepare(req, item, &cas_item_ops);
	rc = m0_rpc_post(item);
	cas_to_rpc_map(req, item);
	M0_LOG(M0_NOTICE, "RPC post returned %d", rc);
}

static int creq_kv_buf_add(const struct m0_cas_req *req,
			   const struct m0_bufvec  *kv,
			   uint32_t                 idx,
			   struct m0_rpc_at_buf    *buf)
{
	M0_PRE(req != NULL);
	M0_PRE(kv  != NULL);
	M0_PRE(buf != NULL);
	M0_PRE(idx < kv->ov_vec.v_nr);

	return m0_rpc_at_add(buf, &M0_BUF_INIT(kv->ov_vec.v_count[idx],
					       kv->ov_buf[idx]),
			     creq_rpc_conn(req));
}

static void creq_asmbl_fop_init(struct m0_cas_req  *req,
				struct m0_fop_type *ftype,
				struct m0_cas_op   *op)
{
	m0_fop_init(&req->ccr_asmbl_fop, ftype, (void *)op,
		    creq_asmbl_fop_release);
}

/**
 * Fills outgoing record 'rec' that is a part of an assembly FOP.
 *
 * Key/value data is assigned from original GET request. Indices of records in
 * original and assembly requests may be different, 'idx' is an index of a
 * record in assembly request and 'orig_idx' is an index of a record in original
 * GET request that should be re-requested.
 */
static int greq_asmbl_add(struct m0_cas_req *req,
			  struct m0_cas_rec *rec,
			  uint64_t           idx,
			  uint64_t           orig_idx,
			  uint64_t           vlen)
{
	int rc;

	m0_rpc_at_init(&rec->cr_key);
	m0_rpc_at_init(&rec->cr_val);
	rc = creq_kv_buf_add(req, req->ccr_keys, orig_idx, &rec->cr_key) ?:
	     m0_rpc_at_recv(&rec->cr_val, creq_rpc_conn(req), vlen, true);
	if (rc == 0) {
		req->ccr_asmbl_ikeys[idx] = orig_idx;
	} else {
		m0_rpc_at_detach(&rec->cr_key);
		m0_rpc_at_fini(&rec->cr_key);
		m0_rpc_at_fini(&rec->cr_val);
	}
	return M0_RC(rc);
}

/**
 * Returns number of records that should be re-requested via assembly request.
 */
static uint64_t greq_asmbl_count(const struct m0_cas_req *req)
{
	const struct m0_fop  *rfop = req->ccr_fop;
	struct m0_cas_op     *req_op = m0_fop_data(rfop);
	struct m0_cas_rep    *rep = cas_rep(cas_req_to_item(req)->ri_reply);
	struct m0_cas_rec    *rcvd;
	struct m0_cas_rec    *sent;
	struct m0_buf         buf;
	uint64_t              len;
	uint64_t              ret = 0;
	uint64_t              i;
	int                   rc;

	M0_PRE(rfop->f_type == &cas_get_fopt);

	for (i = 0; i < rep->cgr_rep.cr_nr; i++) {
		rcvd = &rep->cgr_rep.cr_rec[i];
		sent = &req_op->cg_rec.cr_rec[i];
		rc = m0_rpc_at_rep_get(&sent->cr_val, &rcvd->cr_val, &buf);
		if (rc != 0 && m0_rpc_at_rep_is_bulk(&rcvd->cr_val, &len))
			ret++;
	}
	return ret;
}

static int greq_asmbl_fill(struct m0_cas_req *req, struct m0_cas_op *op)
{
	const struct m0_fop *rfop = req->ccr_fop;
	struct m0_cas_op    *req_op = m0_fop_data(rfop);
	struct m0_cas_rep   *rep = cas_rep(cas_req_to_item(req)->ri_reply);
	struct m0_cas_rec   *rcvd;
	struct m0_cas_rec   *sent;
	struct m0_buf        buf;
	struct m0_cas_recv  *recv;
	uint64_t             len;
	uint64_t             i;
	uint64_t             k = 0;
	int                  rc = 0;

	M0_PRE(op != NULL);
	M0_PRE(rfop->f_type == &cas_get_fopt);

	recv = &op->cg_rec;
	op->cg_id = req_op->cg_id;

	for (i = 0; i < rep->cgr_rep.cr_nr; i++) {
		rcvd = &rep->cgr_rep.cr_rec[i];
		sent = &req_op->cg_rec.cr_rec[i];
		rc = m0_rpc_at_rep_get(&sent->cr_val, &rcvd->cr_val, &buf);
		if (rc != 0 && m0_rpc_at_rep_is_bulk(&rcvd->cr_val, &len)) {
			M0_PRE(k < recv->cr_nr);
			rc = greq_asmbl_add(req, &recv->cr_rec[k], k, i, len);
			if (rc != 0)
				goto err;
			k++;
		}
	}

err:
	if (rc != 0) {
		/* Finalise all already initialised records. */
		recv->cr_nr = k;
		creq_recv_fini(recv, fid_is_meta(&op->cg_id.ci_fid));
	}
	return M0_RC(rc);
}

static bool greq_asmbl_post(struct m0_cas_req *req)
{
	struct m0_cas_op   *op = NULL;
	struct m0_rpc_item *item = &req->ccr_asmbl_fop.f_item;
	uint64_t            asmbl_count;
	bool                ret = false;
	int                 rc = 0;

	asmbl_count = greq_asmbl_count(req);
	if (asmbl_count > 0) {
		M0_ALLOC_ARR(req->ccr_asmbl_ikeys, asmbl_count);
		if (req->ccr_asmbl_ikeys == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto err;
		}
		rc = creq_op_alloc(asmbl_count, &op) ?:
		     greq_asmbl_fill(req, op);
		if (rc == 0) {
			creq_asmbl_fop_init(req, &cas_get_fopt, op);
			creq_item_prepare(req, item, &asmbl_item_ops);
			rc = m0_rpc_post(item);
			cas_to_rpc_map(req, item);
			if (rc != 0)
				m0_fop_put_lock(&req->ccr_asmbl_fop);
			else
				ret = true;
		}
	}
err:
	if (rc != 0) {
		m0_free(req->ccr_asmbl_ikeys);
		creq_op_free(op);
	}
	return ret;
}

static bool creq_niter_invariant(struct creq_niter *it)
{
	return it->cni_req_i <= it->cni_reqv->cr_nr &&
	       it->cni_rep_i <= it->cni_repv->cr_nr;
}

static void creq_niter_init(struct creq_niter *it,
			    struct m0_cas_op  *op,
			    struct m0_cas_rep *rep)
{
	it->cni_reqv  = &op->cg_rec;
	it->cni_repv  = &rep->cgr_rep;
	it->cni_req_i = 0;
	it->cni_rep_i = 0;
	it->cni_kpos  = -1;
	it->cni_req   = NULL;
	it->cni_rep   = NULL;
}

static int creq_niter_next(struct creq_niter *it)
{
	int rc = 0;

	M0_PRE(creq_niter_invariant(it));
	if (it->cni_rep_i == it->cni_repv->cr_nr)
		rc = -ENOENT;

	if (rc == 0) {
		it->cni_rep = &it->cni_repv->cr_rec[it->cni_rep_i++];
		it->cni_kpos++;
		if (it->cni_rep->cr_rc == 1 ||
		    ((int32_t)it->cni_rep->cr_rc <= 0 && it->cni_req_i == 0) ||
		    it->cni_kpos == it->cni_req->cr_rc) {
			/** @todo Validate it. */
			M0_PRE(it->cni_req_i < it->cni_reqv->cr_nr);
			it->cni_req = &it->cni_reqv->cr_rec[it->cni_req_i];
			it->cni_req_i++;
			it->cni_kpos = 0;
		}
	}

	M0_POST(creq_niter_invariant(it));
	return M0_RC(rc);
}

static void creq_niter_fini(struct creq_niter *it)
{
	M0_SET0(it);
}

/**
 * Sets starting keys and allocates space for key/values AT buffers expected in
 * reply.
 */
static int nreq_asmbl_prep(struct m0_cas_req *req, struct m0_cas_op *op)
{
	struct m0_cas_op  *orig = m0_fop_data(req->ccr_fop);
	struct m0_cas_rec *rec;
	uint64_t           i;
	int                rc;

	M0_PRE(op->cg_rec.cr_nr == orig->cg_rec.cr_nr);
	op->cg_id = orig->cg_id;
	for (i = 0; i < orig->cg_rec.cr_nr; i++) {
		rec = &op->cg_rec.cr_rec[i];
		M0_ASSERT(M0_IS0(rec));
		rec->cr_rc = orig->cg_rec.cr_rec[i].cr_rc;
		m0_rpc_at_init(&rec->cr_key);
		rc = creq_kv_buf_add(req, req->ccr_keys, i, &rec->cr_key);
		if (rc != 0)
			goto err;
		M0_ALLOC_ARR(rec->cr_kv_bufs.cv_rec, rec->cr_rc);
		if (op->cg_rec.cr_rec[i].cr_kv_bufs.cv_rec == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto err;
		}
	}
err:
	if (rc != 0) {
		/* Finalise all already initialised records. */
		op->cg_rec.cr_nr = i + 1;
		creq_recv_fini(&op->cg_rec, fid_is_meta(&op->cg_id.ci_fid));
	}
	return M0_RC(rc);
}

static int nreq_asmbl_fill(struct m0_cas_req *req, struct m0_cas_op *op)
{
	const struct m0_fop  *rfop = req->ccr_fop;
	struct m0_cas_rep    *reply = cas_rep(cas_req_to_item(req)->ri_reply);
	struct m0_cas_rec    *rep;
	struct m0_cas_rec    *rec;
	struct m0_rpc_at_buf *key;
	struct m0_rpc_at_buf *val;
	struct creq_niter     iter;
	uint64_t              klen;
	uint64_t              vlen;
	bool                  bulk_key;
	bool                  bulk_val;
	int                   rc = 0;
	uint64_t              i;

	M0_PRE(rfop->f_type == &cas_cur_fopt);

	rc = nreq_asmbl_prep(req, op);
	if (rc != 0)
		return M0_ERR(rc);
	/*
	 * 'op' is a copy of original request relative to starting keys,
	 * so iterator will iterate over 'op' the same way as with original
	 * request.
	 */
	creq_niter_init(&iter, op, reply);
	while (creq_niter_next(&iter) != -ENOENT) {
		rep = iter.cni_rep;
		rec = iter.cni_req;
		i = rec->cr_kv_bufs.cv_nr;
		bulk_key = m0_rpc_at_rep_is_bulk(&rep->cr_key, &klen);
		bulk_val = m0_rpc_at_rep_is_bulk(&rep->cr_val, &vlen);
		key = &rec->cr_kv_bufs.cv_rec[i].ck_key;
		val = &rec->cr_kv_bufs.cv_rec[i].ck_val;
		m0_rpc_at_init(key);
		m0_rpc_at_init(val);
		rc = m0_rpc_at_recv(val, creq_rpc_conn(req), vlen, bulk_val) ?:
		     m0_rpc_at_recv(key, creq_rpc_conn(req), klen, bulk_key);
		if (rc == 0) {
			rec->cr_kv_bufs.cv_nr++;
		} else {
			m0_rpc_at_fini(key);
			m0_rpc_at_fini(val);
			break;
		}
	}
	creq_niter_fini(&iter);

	if (rc != 0)
		creq_recv_fini(&op->cg_rec, fid_is_meta(&op->cg_id.ci_fid));
	return M0_RC(rc);
}

static bool nreq_asmbl_post(struct m0_cas_req *req)
{
	const struct m0_fop *rfop = req->ccr_fop;
	struct m0_cas_op    *req_op = m0_fop_data(rfop);
	struct m0_cas_rep   *rep = cas_rep(cas_req_to_item(req)->ri_reply);
	struct m0_rpc_item  *item = &req->ccr_asmbl_fop.f_item;
	struct m0_cas_rec   *rcvd;
	struct m0_cas_rec   *sent;
	bool                 bulk = false;
	struct m0_buf        buf;
	struct m0_cas_op    *op;
	int                  rc;
	uint64_t             len;
	struct creq_niter    iter;

	creq_niter_init(&iter, req_op, rep);
	while ((rc = creq_niter_next(&iter)) != -ENOENT) {
		rcvd = iter.cni_rep;
		sent = iter.cni_req;
		M0_ASSERT(sent->cr_kv_bufs.cv_nr == 0);
		rc = m0_rpc_at_rep_get(NULL, &rcvd->cr_key, &buf) ?:
		     m0_rpc_at_rep_get(NULL, &rcvd->cr_val, &buf);
		if (rc != 0 && !bulk)
			bulk = m0_rpc_at_rep_is_bulk(&rcvd->cr_key, &len) ||
			       m0_rpc_at_rep_is_bulk(&rcvd->cr_val, &len);
	}
	creq_niter_fini(&iter);

	/*
	 * If at least one key/value requires bulk transmission, then
	 * resend the whole request, requesting bulk transmission as necessary.
	 */
	if (bulk) {
		rc = creq_op_alloc(req_op->cg_rec.cr_nr, &op);
		if (rc == 0) {
			rc = nreq_asmbl_fill(req, op);
			if (rc == 0) {
				creq_asmbl_fop_init(req, &cas_cur_fopt, op);
				creq_item_prepare(req, item, &asmbl_item_ops);
				rc = m0_rpc_post(item);
				cas_to_rpc_map(req, item);
				if (rc != 0)
					m0_fop_put_lock(&req->ccr_asmbl_fop);
			} else {
				creq_op_free(op);
				bulk = false;
			}
		}
	}
	return bulk;
}

static void creq_rep_override(struct m0_cas_rec *orig,
			      struct m0_cas_rec *new)
{
	M0_ENTRY();
	m0_rpc_at_fini(&orig->cr_key);
	m0_rpc_at_fini(&orig->cr_val);
	*orig = *new;
	/*
	 * Key/value data buffers are now attached to both records.
	 * Detach buffers from 'new' record to avoid double free.
	 */
	m0_rpc_at_detach(&new->cr_key);
	m0_rpc_at_detach(&new->cr_val);
	M0_LEAVE();
}

static void nreq_asmbl_accept(struct m0_cas_req *req)
{
	struct m0_fop      *fop = &req->ccr_asmbl_fop;
	struct m0_rpc_item *item = &fop->f_item;
	struct m0_cas_rep  *crep = cas_rep(cas_req_to_item(req)->ri_reply);
	struct m0_cas_rep  *rep = m0_fop_data(m0_rpc_item_to_fop(
							item->ri_reply));
	struct m0_cas_rec  *rcvd;
	struct m0_cas_rec  *sent;
	struct m0_cas_op   *op = m0_fop_data(fop);
	struct m0_cas_kv   *kv;
	struct creq_niter   iter;
	uint64_t            i;
	int                 rc;

	i = 0;
	creq_niter_init(&iter, op, rep);
	while (creq_niter_next(&iter) != -ENOENT) {
		rcvd = iter.cni_rep;
		sent = iter.cni_req;
		if ((int32_t)rcvd->cr_rc > 0) {
			/** @todo validate it */
			M0_PRE(rcvd->cr_rc <= sent->cr_kv_bufs.cv_nr);
			kv = &sent->cr_kv_bufs.cv_rec[rcvd->cr_rc - 1];
			rc = m0_rpc_at_rep2inline(&kv->ck_key, &rcvd->cr_key) ?:
			     m0_rpc_at_rep2inline(&kv->ck_val, &rcvd->cr_val);
			if (rc == 0)
				creq_rep_override(&crep->cgr_rep.cr_rec[i],
						  rcvd);
		}
		i++;
	}
	creq_niter_fini(&iter);
}

static void greq_asmbl_accept(struct m0_cas_req *req)
{
	struct m0_fop      *fop = &req->ccr_asmbl_fop;
	struct m0_rpc_item *item = &fop->f_item;
	struct m0_cas_rep  *crep = cas_rep(cas_req_to_item(req)->ri_reply);
	struct m0_cas_rep  *rep = m0_fop_data(m0_rpc_item_to_fop(
							item->ri_reply));
	struct m0_cas_rec  *rec;
	struct m0_cas_op   *op = m0_fop_data(fop);
	uint64_t            i;
	uint64_t            orig_i;
	int                 rc;

	for (i = 0; i < rep->cgr_rep.cr_nr; i++) {
		rec = &rep->cgr_rep.cr_rec[i];
		if (rec->cr_rc == 0) {
			rc = m0_rpc_at_rep2inline(&op->cg_rec.cr_rec[i].cr_val,
						  &rec->cr_val);
			if (rc == 0) {
				orig_i = req->ccr_asmbl_ikeys[i];
				creq_rep_override(&crep->cgr_rep.cr_rec[orig_i],
						  rec);
			}
		}
	}
}

/**
 * Gets transacation identifier information from returned reply fop
 * of an `UPDATE` op.
 */
static void cas_req_fsync_remid_copy(struct m0_cas_req *req)
{
	struct m0_cas_rep *rep;

	M0_PRE(req != NULL);
	M0_PRE(req->ccr_fop != NULL);

	rep = cas_rep(cas_req_to_item(req)->ri_reply);
	M0_ASSERT(rep != NULL);

	req->ccr_remid = rep->cgr_mod_rep.fmr_remid;
}

static int cas_req_reply_handle(struct m0_cas_req *req,
				bool              *fragm_continue)
{
	struct m0_cas_rep  *reply = &req->ccr_reply;
	struct m0_cas_rep  *rcvd_reply = cas_rep(req->ccr_reply_item);
	struct m0_fop      *req_fop = req->ccr_fop;
	struct m0_cas_op   *op = m0_fop_data(req_fop);
	uint64_t            i;
	uint64_t            reply_seed;
	int                 rc = 0;

	M0_ASSERT(req_fop->f_type == req->ccr_ftype);
	M0_ASSERT(req->ccr_sent_recs_nr <= req->ccr_rec_orig.cr_nr);
	M0_ASSERT(reply->cgr_rep.cr_nr + rcvd_reply->cgr_rep.cr_nr <=
		  req->ccr_max_replies_nr);
	*fragm_continue = false;

	/* Copy tx remid before fop and rpc item in `req` are set to NULL*/
	cas_req_fsync_remid_copy(req);
	/*
	 * Place reply buffers locally (without copying of actual data), zero
	 * them in reply fop to avoid their freeing during reply fop destroying.
	 */
	reply_seed = reply->cgr_rep.cr_nr;
	for (i = 0; i < rcvd_reply->cgr_rep.cr_nr; i++) {
		reply->cgr_rep.cr_rec[reply_seed + i] =
			rcvd_reply->cgr_rep.cr_rec[i];
		/* Detach buffers to avoid double-freeing of received data. */
		creq_kv_hold_down(&rcvd_reply->cgr_rep.cr_rec[i]);
	}
	reply->cgr_rep.cr_nr += rcvd_reply->cgr_rep.cr_nr;
	/* Roger, reply item is not needed anymore. */
	m0_rpc_item_put_lock(req->ccr_reply_item);
	req->ccr_reply_item = NULL;
	/*
	 * Null fop data pointer to avoid cas_op freeing during fop finalisation
	 * as cas_op is going to be reused for the rest fragments sending.
	 */
	req_fop->f_data.fd_data = NULL;
	m0_fop_put_lock(req_fop);
	req->ccr_fop = NULL;
	if (req->ccr_sent_recs_nr < req->ccr_rec_orig.cr_nr) {
		/* Continue fragmentation. */
		rc = cas_req_fragment_continue(req, op);
		if (rc == 0)
			*fragm_continue = true;
	}
	return rc;
}

static void creq_asmbl_replied_ast(struct m0_sm_group *grp,
				   struct m0_sm_ast   *ast)
{
	struct m0_cas_req  *req   = container_of(ast, struct m0_cas_req,
						 ccr_replied_ast);
	struct m0_fop      *fop   = &req->ccr_asmbl_fop;
	struct m0_rpc_item *item  = &fop->f_item;
	struct m0_rpc_item *reply = item->ri_reply;
	bool                fragm_continue;
	int                 rc;

	rc = m0_rpc_item_error(item) ?:
	     cas_rep(reply)->cgr_rc ?:
	     cas_rep__validate(fop->f_type, m0_fop_data(fop), cas_rep(reply));
	if (rc == 0) {
		M0_ASSERT(M0_IN(fop->f_type, (&cas_get_fopt, &cas_cur_fopt)));
		if (fop->f_type == &cas_get_fopt)
			greq_asmbl_accept(req);
		else
			nreq_asmbl_accept(req);
	}
	/*
	 * On assembly request error, just continue request processing.
	 * All records that were requested via assembly request already
	 * have error status code.
	 */
	rc = cas_req_reply_handle(req, &fragm_continue);
	if (rc == 0)
		cas_req_state_set(req, !fragm_continue ?
				  CASREQ_FINAL : CASREQ_FRAGM_SENT);
	else
		cas_req_failure(req, M0_ERR(rc));

	m0_rpc_item_put_lock(item);
}

static void creq_asmbl_replied_cb(struct m0_rpc_item *item)
{
	struct m0_cas_req *req = container_of(m0_rpc_item_to_fop(item),
					      struct m0_cas_req,
					      ccr_asmbl_fop);

	M0_ENTRY();
	req->ccr_replied_ast.sa_cb = creq_asmbl_replied_ast;
	m0_sm_ast_post(cas_req_smgrp(req), &req->ccr_replied_ast);
	M0_LEAVE();
}

static void cas_req_replied_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_cas_req  *req = container_of(ast, struct m0_cas_req,
					       ccr_replied_ast);
	struct m0_fop_type *req_fop_type = req->ccr_fop->f_type;
	bool                assembly_wait = false;
	bool                suppress_err_msg;
	bool                fragm_continue;
	int                 rc;

	req->ccr_reply.cgr_rc = cas_rep(req->ccr_reply_item)->cgr_rc;
	rc = cas_rep_validate(req);
	if (rc == 0) {
		if (M0_IN(req_fop_type, (&cas_cur_fopt, &cas_get_fopt)) &&
		    !req->ccr_is_meta) {
			assembly_wait = (req_fop_type == &cas_get_fopt) ?
				greq_asmbl_post(req) : nreq_asmbl_post(req);
			if (assembly_wait)
				cas_req_state_set(req, CASREQ_ASSEMBLY);
		}
		if (!assembly_wait) {
			rc = cas_req_reply_handle(req, &fragm_continue);
			if (rc == 0 && !fragm_continue)
				cas_req_state_set(req, CASREQ_FINAL);
		}
	}
	if (rc != 0) {
		/*
		 * For now CROW flag is commonly used for PUT operations. In
		 * this case indices are physically created only on the nodes
		 * where keys are presented. But NEXT queries are sent to all
		 * the nodes, so some of them may return -ENOENT (index does
		 * not exist). It is not critical, suppress these errors not
		 * to irritate the user.
		 */
		suppress_err_msg = !req->ccr_is_meta &&
			req_fop_type == &cas_cur_fopt && rc == -ENOENT;
		cas_req_failure(req, suppress_err_msg ? rc : M0_ERR(rc));
	}
}

static void cas_req_replied_cb(struct m0_rpc_item *item)
{
	struct m0_cas_req *req = item_to_cas_req(item);

	M0_ENTRY();
	if (M0_FI_ENABLED("send-failure"))
		item->ri_error = -ENOTCONN;
	if (item->ri_error == 0) {
		M0_ASSERT(item->ri_reply != NULL);
		req->ccr_reply_item = item->ri_reply;
		/*
		 * Get additional reference to reply item to copy reply buffers
		 * in replied ast call.
		 */
		m0_rpc_item_get(item->ri_reply);
		req->ccr_replied_ast.sa_cb = cas_req_replied_ast;
		m0_sm_ast_post(cas_req_smgrp(req), &req->ccr_replied_ast);
	} else
		cas_req_failure_ast_post(req, item->ri_error);
	M0_LEAVE();
}

static int cas_index_op_prepare(const struct m0_cas_req  *req,
				const struct m0_cas_id   *cids,
				uint64_t                  cids_nr,
				bool                      recv_val,
				uint32_t                  flags,
				struct m0_cas_op        **out)
{
	struct m0_cas_op  *op;
	struct m0_cas_rec *rec;
	int                rc;
	uint64_t           i;

	M0_ENTRY();

	rc = creq_op_alloc(cids_nr, &op);
	if (rc != 0)
		return M0_ERR(rc);
	op->cg_id.ci_fid = m0_cas_meta_fid;
	op->cg_flags = flags;
	rec = op->cg_rec.cr_rec;
	for (i = 0; i < cids_nr; i++) {
		struct m0_buf buf;

		m0_rpc_at_init(&rec[i].cr_key);
		/* Xcode the key to get continuous buffer for sending. */
		rc = m0_xcode_obj_enc_to_buf(
			&M0_XCODE_OBJ(m0_cas_id_xc,
				      /*
				       * Cast to avoid 'discard const' compile
				       * error, in fact cas id element is not
				       * changed during encoding.
				       */
				      (struct m0_cas_id *)&cids[i]),
			&buf.b_addr, &buf.b_nob);
		if (rc == 0) {
			rc = m0_rpc_at_add(&rec[i].cr_key,
					   &buf,
					   creq_rpc_conn(req));
			if (rc != 0)
				m0_buf_free(&buf);
			else if (recv_val) {
				m0_rpc_at_init(&rec[i].cr_val);
				rc = m0_rpc_at_recv(&rec[i].cr_val,
						    creq_rpc_conn(req),
						    sizeof(struct m0_fid),
						    false);
			}
		}
		if (rc != 0)
			break;
	}

	if (rc != 0) {
		op->cg_rec.cr_nr = i + 1;
		creq_recv_fini(&op->cg_rec, fid_is_meta(&op->cg_id.ci_fid));
		creq_op_free(op);
		return M0_ERR(rc);
	}
	*out = op;
	return M0_RC(rc);
}

static void addb2_add_cas_req_attrs(const struct m0_cas_req *req)
{
	uint64_t sm_id = m0_sm_id_get(&req->ccr_sm);

	M0_ADDB2_ADD(M0_AVI_ATTR, sm_id,
		     M0_AVI_CAS_REQ_ATTR_IS_META, !!req->ccr_is_meta);
	if (req->ccr_req_op != NULL)
		M0_ADDB2_ADD(M0_AVI_ATTR, sm_id, M0_AVI_CAS_REQ_ATTR_REC_NR,
			     req->ccr_req_op->cg_rec.cr_nr);
}

static int cas_index_req_prepare(struct m0_cas_req       *req,
				 const struct m0_cas_id  *cids,
				 uint64_t                 cids_nr,
				 uint64_t                 max_replies_nr,
				 bool                     recv_val,
				 uint32_t                 flags,
				 struct m0_cas_op       **op)
{
	struct m0_cas_recv *reply_recv;
	int                 rc;

	reply_recv = &req->ccr_reply.cgr_rep;
	M0_ALLOC_ARR(reply_recv->cr_rec, max_replies_nr);
	if (reply_recv->cr_rec == NULL)
		return M0_ERR(-ENOMEM);
	/* Set to 0 initially, will be increased when reply is received. */
	reply_recv->cr_nr = 0;
	req->ccr_max_replies_nr = max_replies_nr;
	req->ccr_is_meta = true;
	rc = cas_index_op_prepare(req, cids, cids_nr, recv_val, flags, op);
	if (rc == 0) {
		req->ccr_rec_orig = (*op)->cg_rec;
		req->ccr_req_op = *op;
		addb2_add_cas_req_attrs(req);
	}
	if (rc != 0) {
		m0_free(reply_recv->cr_rec);
		reply_recv->cr_nr = 0;
		reply_recv->cr_rec = NULL;
	}

	return M0_RC(rc);

}

M0_INTERNAL uint64_t m0_cas_req_nr(const struct m0_cas_req *req)
{
	return req->ccr_reply.cgr_rep.cr_nr;
}

M0_INTERNAL int m0_cas_req_wait(struct m0_cas_req *req, uint64_t states,
				m0_time_t to)
{
	M0_ENTRY();
	M0_PRE(m0_cas_req_is_locked(req));
	return M0_RC(m0_sm_timedwait(&req->ccr_sm, states, to));
}

M0_INTERNAL int m0_cas_index_create(struct m0_cas_req      *req,
				    const struct m0_cas_id *cids,
				    uint64_t                cids_nr,
				    struct m0_dtx          *dtx)
{
	struct m0_cas_op      *op;
	enum m0_cas_req_state  next_state;
	int                    rc;

	M0_ENTRY();
	M0_PRE(req->ccr_sess != NULL);
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE(m0_forall(i, cids_nr, m0_cas_id_invariant(&cids[i])));
	(void)dtx;
	rc = cas_index_req_prepare(req, cids, cids_nr, cids_nr, false, 0, &op);
	if (rc != 0)
		return M0_ERR(rc);
	rc = creq_fop_create_and_prepare(req, &cas_put_fopt, op, &next_state);
	if (rc == 0) {
		cas_fop_send(req);
		cas_req_state_set(req, next_state);
	}
	return M0_RC(rc);
}

static void cas_rep_copy(const struct m0_cas_req *req,
			 uint64_t                 idx,
			 struct m0_cas_rec_reply *rep)
{
	const struct m0_cas_recv *recv = &req->ccr_reply.cgr_rep;

	M0_ASSERT(idx < m0_cas_req_nr(req));
	rep->crr_rc = recv->cr_rec[idx].cr_rc;
	rep->crr_hint = recv->cr_rec[idx].cr_hint;
}

M0_INTERNAL void m0_cas_index_create_rep(const struct m0_cas_req *req,
					 uint64_t                 idx,
					 struct m0_cas_rec_reply *rep)
{
	M0_ENTRY();
	cas_rep_copy(req, idx, rep);
	M0_LEAVE();
}

M0_INTERNAL int m0_cas_index_delete(struct m0_cas_req      *req,
				    const struct m0_cas_id *cids,
				    uint64_t                cids_nr,
				    struct m0_dtx          *dtx,
				    uint32_t                flags)
{
	struct m0_cas_op      *op;
	enum m0_cas_req_state  next_state;
	int                    rc;

	M0_ENTRY();
	M0_PRE(req->ccr_sess != NULL);
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE(m0_forall(i, cids_nr, m0_cas_id_invariant(&cids[i])));
	M0_PRE((flags & ~(COF_CROW | COF_DEL_LOCK)) == 0);
	(void)dtx;
	rc = cas_index_req_prepare(req, cids, cids_nr, cids_nr, false, flags,
				   &op);
	if (rc != 0)
		return M0_ERR(rc);
	rc = creq_fop_create_and_prepare(req, &cas_del_fopt, op, &next_state);
	if (rc == 0) {
		cas_fop_send(req);
		cas_req_state_set(req, CASREQ_SENT);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_cas_index_delete_rep(const struct m0_cas_req *req,
					 uint64_t                 idx,
					 struct m0_cas_rec_reply *rep)
{
	M0_ENTRY();
	cas_rep_copy(req, idx, rep);
	M0_LEAVE();
}

M0_INTERNAL int m0_cas_index_lookup(struct m0_cas_req      *req,
				    const struct m0_cas_id *cids,
				    uint64_t                cids_nr)
{
	struct m0_cas_op      *op;
	enum m0_cas_req_state  next_state;
	int                    rc;

	M0_ENTRY();
	M0_PRE(req->ccr_sess != NULL);
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE(m0_forall(i, cids_nr, m0_cas_id_invariant(&cids[i])));
	rc = cas_index_req_prepare(req, cids, cids_nr, cids_nr, true, 0, &op);
	if (rc != 0)
		return M0_ERR(rc);
	rc = creq_fop_create_and_prepare(req, &cas_get_fopt, op, &next_state);
	if (rc == 0) {
		cas_fop_send(req);
		cas_req_state_set(req, next_state);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_cas_index_lookup_rep(const struct m0_cas_req *req,
					 uint64_t                 idx,
					 struct m0_cas_rec_reply *rep)
{
	M0_ENTRY();
	cas_rep_copy(req, idx, rep);
	M0_LEAVE();
}

M0_INTERNAL int m0_cas_index_list(struct m0_cas_req   *req,
				  const struct m0_fid *start_fid,
				  uint32_t             indices_nr,
				  uint32_t             flags)
{
	struct m0_cas_op      *op;
	struct m0_cas_id       cid = { .ci_fid = *start_fid };
	enum m0_cas_req_state  next_state;
	int                    rc;

	M0_ENTRY();
	M0_PRE(start_fid != NULL);
	M0_PRE(req->ccr_sess != NULL);
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE((flags & ~(COF_SLANT | COF_EXCLUDE_START_KEY)) == 0);

	rc = cas_index_req_prepare(req, &cid, 1, indices_nr, false, flags, &op);
	if (rc != 0)
		return M0_ERR(rc);
	op->cg_rec.cr_rec[0].cr_rc = indices_nr;
	rc = creq_fop_create_and_prepare(req, &cas_cur_fopt, op, &next_state);
	if (rc == 0) {
		cas_fop_send(req);
		cas_req_state_set(req, next_state);
	}
	return M0_RC(rc);
}

static int cas_next_rc(int64_t service_rc)
{
	int rc;

	/*
	 * Zero return code means some error on service side.
	 * Service place sequence number of record starting from 1 in cr_rc on
	 * success.
	 */
	if (service_rc == 0)
		/*
		 * Don't use M0_ERR() here to not pollute trace log.
		 * Service places zero return code in all records following the
		 * record having negative return code. It can happen in a
		 * totally valid case when client requests more records than
		 * available in a catalogue.
		 */
		rc = -EPROTO;
	else if (service_rc < 0)
		rc = service_rc;
	else
		rc = 0;
	return M0_RC(rc);
}

M0_INTERNAL void m0_cas_index_list_rep(struct m0_cas_req         *req,
				       uint32_t                   idx,
				       struct m0_cas_ilist_reply *rep)
{
	struct m0_cas_recv *recv = &req->ccr_reply.cgr_rep;
	struct m0_cas_rec  *rec;
	struct m0_buf       fid;

	M0_ENTRY();
	M0_PRE(idx < m0_cas_req_nr(req));

	rec = &recv->cr_rec[idx];
	rep->clr_rc = cas_next_rc(rec->cr_rc) ?:
		      m0_rpc_at_rep_get(NULL, &rec->cr_key, &fid);
	if (rep->clr_rc == 0) {
		rep->clr_fid = *(struct m0_fid *)fid.b_addr;
		rep->clr_hint = recv->cr_rec[idx].cr_hint;
	}
	M0_LEAVE();
}

static int cas_req_fragmentation(struct m0_cas_req *req)
{
	struct m0_cas_op *op = m0_fop_data(req->ccr_fop);
	int               rc = 0;

	if (M0_FI_ENABLED("fragm_error"))
		return -E2BIG;

	M0_PRE(req->ccr_rec_orig.cr_nr != 0 &&
	       req->ccr_rec_orig.cr_rec != NULL);
	M0_PRE(req->ccr_sent_recs_nr < req->ccr_rec_orig.cr_nr);
	/*
	 * Flush records counter and reset records pointer to the first record
	 * to be sent.
	 */
	op->cg_rec.cr_nr = 0;
	op->cg_rec.cr_rec = &req->ccr_rec_orig.cr_rec[req->ccr_sent_recs_nr];
	do {
		op->cg_rec.cr_nr++;
		if (m0_rpc_item_max_payload_exceeded(&req->ccr_fop->f_item,
						     req->ccr_sess)) {
			/*
			 * Found the number of records when item payload exceeds
			 * max rpc item payload, chose previous number of
			 * records (current - 1) for sending.
			 */
			op->cg_rec.cr_nr--;
			break;
		}
	} while (req->ccr_sent_recs_nr + op->cg_rec.cr_nr <
		 req->ccr_rec_orig.cr_nr);
	if (op->cg_rec.cr_nr == 0)
		rc = -E2BIG; /* Almost impossible case. */
	if (rc != 0)
		/*
		 * Restore original records vector in case of error for proper
		 * finalisation of data structures.
		 */
		op->cg_rec = req->ccr_rec_orig;

	return M0_RC(rc);
}

static int cas_req_fragment_continue(struct m0_cas_req  *req,
				     struct m0_cas_op   *op)
{
	int rc;

	rc = creq_fop_create(req, req->ccr_ftype, op);
	if (rc != 0)
		return M0_ERR(rc);
	rc = cas_req_fragmentation(req);
	if (rc == 0)
		cas_fop_send(req);
	else
		creq_fop_destroy(req);

	return M0_RC(rc);
}

static int cas_records_op_prepare(const struct m0_cas_req  *req,
				  const struct m0_cas_id   *index,
				  const struct m0_bufvec   *keys,
				  const struct m0_bufvec   *values,
				  uint32_t                  flags,
				  struct m0_cas_op        **out)
{
	struct m0_cas_op   *op;
	struct m0_cas_rec  *rec;
	uint32_t            keys_nr = keys->ov_vec.v_nr;
	uint64_t            i;
	int                 rc;

	M0_ENTRY();
	rc = creq_op_alloc(keys_nr, &op);
	if (rc != 0)
		return M0_ERR(rc);
	op->cg_id = *index;
	rec = op->cg_rec.cr_rec;
	for (i = 0; i < keys_nr; i++) {
		m0_rpc_at_init(&rec[i].cr_key);
		rc = creq_kv_buf_add(req, keys, i, &rec[i].cr_key);
		if (rc == 0 && values != NULL) {
			m0_rpc_at_init(&rec[i].cr_val);
			rc = creq_kv_buf_add(req, values, i, &rec[i].cr_val);
		}
		if (rc != 0)
			break;
	}
	if (rc != 0) {
		op->cg_rec.cr_nr = i + 1;
		creq_recv_fini(&op->cg_rec, fid_is_meta(&op->cg_id.ci_fid));
		creq_op_free(op);
		return M0_ERR(rc);
	}
	op->cg_flags = flags;
	*out = op;
	return M0_RC(rc);
}

static int cas_req_prep(struct m0_cas_req       *req,
			const struct m0_cas_id  *index,
			const struct m0_bufvec  *keys,
			const struct m0_bufvec  *values,
			uint64_t                 max_replies_nr,
			uint32_t                 flags,
			struct m0_cas_op       **op)
{
	struct m0_cas_recv *reply_recv;
	int                 rc;

	reply_recv = &req->ccr_reply.cgr_rep;
	M0_ALLOC_ARR(reply_recv->cr_rec, max_replies_nr);
	if (reply_recv->cr_rec == NULL)
		return M0_ERR(-ENOMEM);
	/* Set to 0 initially, will be increased when reply is received. */
	reply_recv->cr_nr = 0;
	req->ccr_max_replies_nr = max_replies_nr;
	req->ccr_is_meta = false;
	rc = cas_records_op_prepare(req, index, keys, values, flags, op);
	if (rc == 0) {
		req->ccr_rec_orig = (*op)->cg_rec;
		req->ccr_req_op = *op;
		addb2_add_cas_req_attrs(req);
	}
	if (rc != 0) {
		m0_free(reply_recv->cr_rec);
		reply_recv->cr_nr = 0;
		reply_recv->cr_rec = NULL;
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_cas_put(struct m0_cas_req      *req,
			   struct m0_cas_id       *index,
			   const struct m0_bufvec *keys,
			   const struct m0_bufvec *values,
			   struct m0_dtx          *dtx,
			   uint32_t                flags)
{
	struct m0_cas_op      *op;
	enum m0_cas_req_state  next_state;
	int                    rc;

	M0_ENTRY();
	M0_PRE(keys != NULL);
	M0_PRE(values != NULL);
	M0_PRE(keys->ov_vec.v_nr == values->ov_vec.v_nr);
	M0_PRE(m0_cas_req_is_locked(req));
	/* Create and overwrite flags can't be specified together. */
	M0_PRE(!(flags & COF_CREATE) || !(flags & COF_OVERWRITE));
	/* Only create, overwrite, crow and sync_wait flags are allowed. */
	M0_PRE((flags &
		~(COF_CREATE | COF_OVERWRITE | COF_CROW | COF_SYNC_WAIT)) == 0);
	M0_PRE(m0_cas_id_invariant(index));

	(void)dtx;
	rc = cas_req_prep(req, index, keys, values, keys->ov_vec.v_nr, flags,
			  &op);
	if (rc != 0)
		return M0_ERR(rc);
	rc = creq_fop_create_and_prepare(req, &cas_put_fopt, op, &next_state);
	if (rc == 0) {
		cas_fop_send(req);
		cas_req_state_set(req, next_state);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_cas_put_rep(struct m0_cas_req       *req,
				uint64_t                 idx,
				struct m0_cas_rec_reply *rep)
{
	M0_ENTRY();
	M0_PRE(req->ccr_ftype == &cas_put_fopt);
	cas_rep_copy(req, idx, rep);
	M0_LEAVE();
}

M0_INTERNAL int m0_cas_get(struct m0_cas_req      *req,
			   struct m0_cas_id       *index,
			   const struct m0_bufvec *keys)
{
	struct m0_cas_op      *op;
	int                    rc;
	struct m0_rpc_at_buf  *ab;
	enum m0_cas_req_state  next_state;
	uint32_t               i;

	M0_ENTRY();
	M0_PRE(keys != NULL);
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE(m0_cas_id_invariant(index));

	rc = cas_req_prep(req, index, keys, NULL, keys->ov_vec.v_nr, 0, &op);
	if (rc != 0)
		return M0_ERR(rc);
	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		ab = &op->cg_rec.cr_rec[i].cr_val;
		m0_rpc_at_init(ab);
		rc = m0_rpc_at_recv(ab, creq_rpc_conn(req),
				    M0_RPC_AT_UNKNOWN_LEN, false);
		if (rc != 0) {
			m0_rpc_at_fini(ab);
			break;
		}
	}
	if (rc != 0) {
		op->cg_rec.cr_nr = i;
		creq_recv_fini(&op->cg_rec, fid_is_meta(&op->cg_id.ci_fid));
		creq_op_free(op);
	} else {
		req->ccr_keys = keys;
		rc = creq_fop_create_and_prepare(req, &cas_get_fopt, op,
						 &next_state);
		if (rc == 0) {
			cas_fop_send(req);
			cas_req_state_set(req, next_state);
		}
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_cas_get_rep(const struct m0_cas_req *req,
				uint64_t                 idx,
				struct m0_cas_get_reply *rep)
{
	const struct m0_cas_rep *cas_rep = &req->ccr_reply;
	struct m0_cas_rec       *sent;
	struct m0_cas_rec       *rcvd;

	M0_ENTRY();
	M0_PRE(idx < m0_cas_req_nr(req));
	M0_PRE(req->ccr_ftype == &cas_get_fopt);
	rcvd = &cas_rep->cgr_rep.cr_rec[idx];
	sent = &req->ccr_rec_orig.cr_rec[idx];
	rep->cge_rc = rcvd->cr_rc;
	if (rep->cge_rc == 0)
	      m0_rpc_at_rep_get(&sent->cr_val, &rcvd->cr_val, &rep->cge_val);
	M0_LEAVE();
}

M0_INTERNAL int m0_cas_next(struct m0_cas_req *req,
			    struct m0_cas_id  *index,
			    struct m0_bufvec  *start_keys,
			    uint32_t          *recs_nr,
			    uint32_t           flags)
{
	struct m0_cas_op      *op;
	enum m0_cas_req_state  next_state;
	uint64_t               max_replies_nr = 0;
	uint32_t               i;
	int                    rc;

	M0_ENTRY();
	M0_PRE(start_keys != NULL);
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE(m0_cas_id_invariant(index));
	/* Only slant and exclude start key flags are allowed. */
	M0_PRE((flags & ~(COF_SLANT | COF_EXCLUDE_START_KEY)) == 0);

	for (i = 0; i < start_keys->ov_vec.v_nr; i++)
		max_replies_nr += recs_nr[i];
	rc = cas_req_prep(req, index, start_keys, NULL, max_replies_nr, flags,
			  &op);
	if (rc != 0)
		return M0_ERR(rc);
	for (i = 0; i < start_keys->ov_vec.v_nr; i++)
		op->cg_rec.cr_rec[i].cr_rc = recs_nr[i];
	req->ccr_keys = start_keys;
	rc = creq_fop_create_and_prepare(req, &cas_cur_fopt, op,
					 &next_state);
	if (rc == 0) {
		cas_fop_send(req);
		cas_req_state_set(req, next_state);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_cas_rep_mlock(const struct m0_cas_req *req,
				  uint64_t                 idx)
{
	const struct m0_cas_rep *cas_rep = &req->ccr_reply;

	M0_PRE(M0_IN(req->ccr_ftype, (&cas_get_fopt, &cas_cur_fopt)));
	creq_kv_hold_down(&cas_rep->cgr_rep.cr_rec[idx]);
}

M0_INTERNAL void m0_cas_next_rep(const struct m0_cas_req  *req,
				 uint32_t                  idx,
				 struct m0_cas_next_reply *rep)
{
	const struct m0_cas_rep *cas_rep = &req->ccr_reply;
	struct m0_cas_rec       *rcvd;

	M0_ENTRY();
	M0_PRE(idx < m0_cas_req_nr(req));
	M0_PRE(req->ccr_ftype == &cas_cur_fopt);
	rcvd = &cas_rep->cgr_rep.cr_rec[idx];
	rep->cnp_rc = cas_next_rc(rcvd->cr_rc) ?:
		      m0_rpc_at_rep_get(NULL, &rcvd->cr_key, &rep->cnp_key) ?:
		      m0_rpc_at_rep_get(NULL, &rcvd->cr_val, &rep->cnp_val);
}

M0_INTERNAL int m0_cas_del(struct m0_cas_req *req,
			   struct m0_cas_id  *index,
			   struct m0_bufvec  *keys,
			   struct m0_dtx     *dtx,
			   uint32_t           flags)
{
	struct m0_cas_op      *op;
	enum m0_cas_req_state  next_state;
	int                    rc;

	M0_ENTRY();
	M0_PRE(keys != NULL);
	M0_PRE(m0_cas_req_is_locked(req));
	M0_PRE(m0_cas_id_invariant(index));
	M0_PRE(M0_IN(flags, (0, COF_DEL_LOCK, COF_SYNC_WAIT)));

	(void)dtx;
	rc = cas_req_prep(req, index, keys, NULL, keys->ov_vec.v_nr, flags,
			  &op);
	if (rc != 0)
		return M0_ERR(rc);
	rc = creq_fop_create_and_prepare(req, &cas_del_fopt, op, &next_state);
	if (rc == 0) {
		cas_fop_send(req);
		cas_req_state_set(req, next_state);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_cas_del_rep(struct m0_cas_req       *req,
				uint64_t                 idx,
				struct m0_cas_rec_reply *rep)
{
	M0_ENTRY();
	M0_PRE(req->ccr_ftype == &cas_del_fopt);
	cas_rep_copy(req, idx, rep);
	M0_LEAVE();
}

M0_INTERNAL int  m0_cas_sm_conf_init(void)
{
	m0_sm_conf_init(&cas_req_sm_conf);
	return m0_sm_addb2_init(&cas_req_sm_conf,
				M0_AVI_CAS_SM_REQ,
				M0_AVI_CAS_SM_REQ_COUNTER);
}

M0_INTERNAL void m0_cas_sm_conf_fini(void)
{
	m0_sm_addb2_fini(&cas_req_sm_conf);
	m0_sm_conf_fini(&cas_req_sm_conf);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of cas-client group */

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
