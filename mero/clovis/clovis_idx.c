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
 * Original author: Sining Wu       <sining.wu@seagate.com>
 * Revision:        Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 24-Aug-2015
 *
 */

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_addb.h"
#include "clovis/clovis_idx.h"
#include "clovis/sync.h"

#include "lib/errno.h"
#include "lib/finject.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

static struct m0_clovis_idx_service
clovis_idx_services[M0_CLOVIS_IDX_MAX_SERVICE_ID];

static void clovis_idx_op_cb_launch(struct m0_clovis_op_common *oc);
static void clovis_idx_op_cb_fini(struct m0_clovis_op_common *oc);
static void clovis_idx_op_cb_free(struct m0_clovis_op_common *oc);

const struct m0_bob_type oi_bobtype;
M0_BOB_DEFINE(M0_INTERNAL, &oi_bobtype,  m0_clovis_op_idx);
const struct m0_bob_type oi_bobtype = {
	.bt_name         = "oi_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_op_idx, oi_magic),
	.bt_magix        = M0_CLOVIS_OI_MAGIC,
	.bt_check        = NULL,
};

/**
 * Returns the clovis instance associated to an index operation.
 *
 * @param oi index operation pointing to the instance.
 * @return a pointer to the clovis instance associated to the entity.
 */
static struct m0_clovis *
clovis_oi_instance(struct m0_clovis_op_idx *oi)
{
	M0_PRE(oi != NULL);

	return m0_clovis__entity_instance(oi->oi_oc.oc_op.op_entity);
}

/**
 * Checks an index operation is not malformed or corrupted.
 *
 * @param oi index operation to be checked.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
M0_INTERNAL bool m0_clovis__idx_op_invariant(struct m0_clovis_op_idx *oi)
{
	return M0_RC(oi != NULL &&
		     m0_clovis_op_idx_bob_check(oi) &&
		     oi->oi_oc.oc_op.op_size >= sizeof *oi &&
		     m0_clovis_ast_rc_bob_check(&oi->oi_ar) &&
		     m0_clovis_op_common_bob_check(&oi->oi_oc));
}

/**
 * Sets an index operation. Allocates the operation if it has not been pre-
 * allocated.
 *
 * @param entity Entity of the obj the operation is targeted to.
 * @param op Operation to be prepared.
 * @param opcode Specific operation code.
 * @return 0 if the operation completes successfully or an error code
 * otherwise.
 */
static int clovis_idx_op_init(struct m0_clovis_idx *idx, int opcode,
			      struct m0_bufvec *keys, struct m0_bufvec *vals,
			      int32_t *rcs, uint32_t flags,
			      struct m0_clovis_op *op)
{
	int                         rc;
	struct m0_clovis_op_common *oc;
	struct m0_clovis_op_idx    *oi;
	struct m0_clovis_entity    *entity;
	struct m0_locality         *locality;

	M0_ENTRY();

	M0_PRE(idx != NULL);
	M0_PRE(op != NULL);

	/* Initialise the operation's generic part. */
	entity = &idx->in_entity;
	op->op_code = opcode;
	rc = m0_clovis_op_init(op, &clovis_op_conf, entity);
	if (rc != 0)
		return M0_ERR(rc);
	/*
	 * Init m0_clovis_op_common part.
	 * bob_init()'s haven't been called yet: we use M0_AMB().
	 */
	oc = M0_AMB(oc, op, oc_op);
	m0_clovis_op_common_bob_init(oc);
	oc->oc_cb_launch = clovis_idx_op_cb_launch;
	oc->oc_cb_fini   = clovis_idx_op_cb_fini;
	oc->oc_cb_free   = clovis_idx_op_cb_free;

	/* Init the m0_clovis_op_idx part. */
	oi = M0_AMB(oi, oc, oi_oc);
	oi->oi_idx  = idx;
	oi->oi_keys = keys;
	oi->oi_vals = vals;
	oi->oi_rcs  = rcs;
	oi->oi_flags = flags;

	locality = m0_clovis__locality_pick(clovis_oi_instance(oi));
	M0_ASSERT(locality != NULL);
	oi->oi_sm_grp = locality->lo_grp;
	M0_SET0(&oi->oi_ar);

	m0_clovis_op_idx_bob_init(oi);
	m0_clovis_ast_rc_bob_init(&oi->oi_ar);

	return M0_RC(0);
}

/**
 * Completes an index operation by moving the state of all its state machines
 * to successful states.
 */
static void clovis_idx_op_complete(struct m0_clovis_op_idx *oi)
{
	struct m0_clovis_op *op;
	struct m0_sm_group  *op_grp;
	struct m0_sm_group  *en_grp;

	M0_ENTRY();

	M0_PRE(oi != NULL);
	op = &oi->oi_oc.oc_op;
	op_grp = &op->op_sm_group;
	en_grp = &op->op_entity->en_sm_group;
	m0_sm_group_lock(en_grp);

	if (op->op_code == M0_CLOVIS_EO_CREATE)
		m0_sm_move(&op->op_entity->en_sm, 0, M0_CLOVIS_ES_OPEN);
	else if (op->op_code == M0_CLOVIS_EO_DELETE)
		m0_sm_move(&op->op_entity->en_sm, 0, M0_CLOVIS_ES_INIT);
	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	m0_clovis_op_executed(op);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_STABLE);
	m0_clovis_op_stable(op);
	m0_sm_group_unlock(op_grp);

	M0_LEAVE();
}

/**
 * AST callback to complete a whole index operation.
 *
 * @param grp group the AST is executed in.
 * @param ast callback being executed.
 */
M0_INTERNAL void clovis_idx_op_ast_complete(struct m0_sm_group *grp,
					    struct m0_sm_ast *ast)
{
	struct m0_clovis_op_idx *oi;
	struct m0_clovis_ast_rc *ar;

	M0_ENTRY();

	M0_PRE(grp != NULL);
	M0_PRE(m0_sm_group_is_locked(grp));
	M0_PRE(ast != NULL);

	ar = bob_of(ast,  struct m0_clovis_ast_rc, ar_ast, &ar_bobtype);
	oi = bob_of(ar, struct m0_clovis_op_idx, oi_ar, &oi_bobtype);
	clovis_idx_op_complete(oi);

	M0_LEAVE();
}

/**
 * Fails a whole index operation and moves the state of its state machines to
 * failed or error states.
 *
 * @param oi index operation to be failed.
 * @param rc error code that explains why the operation is being failed.
 */
static void clovis_idx_op_fail(struct m0_clovis_op_idx *oi, int rc)
{
	struct m0_clovis_op *op;
	struct m0_sm_group  *op_grp;
	struct m0_sm_group  *en_grp;

	M0_ENTRY();

	M0_PRE(oi != NULL);

	op = &oi->oi_oc.oc_op;

	op_grp = &op->op_sm_group;
	en_grp = &op->op_entity->en_sm_group;

	m0_sm_group_lock(en_grp);

	if (op->op_code == M0_CLOVIS_EO_CREATE)
		m0_sm_move(&op->op_entity->en_sm, 0, M0_CLOVIS_ES_OPEN);
	else if (op->op_code == M0_CLOVIS_EO_DELETE)
		m0_sm_move(&op->op_entity->en_sm, 0, M0_CLOVIS_ES_INIT);

	m0_sm_group_unlock(en_grp);

	m0_sm_group_lock(op_grp);
	op->op_rc = rc;
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	m0_clovis_op_executed(op);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_STABLE);
	m0_clovis_op_stable(op);
	m0_sm_group_unlock(op_grp);

	M0_LEAVE();
}

/**
 * AST callback to fail a whole index operation.
 *
 * @param grp group the AST is executed in.
 * @param ast callback being executed.
 */
M0_INTERNAL void clovis_idx_op_ast_fail(struct m0_sm_group *grp,
					struct m0_sm_ast *ast)
{
	struct m0_clovis_op_idx *oi;
	struct m0_clovis_ast_rc *ar;

	M0_ENTRY();

	M0_PRE(grp != NULL);
	M0_PRE(m0_sm_group_is_locked(grp));
	M0_PRE(ast != NULL);

	ar = bob_of(ast, struct m0_clovis_ast_rc, ar_ast, &ar_bobtype);
	oi = bob_of(ar, struct m0_clovis_op_idx, oi_ar, &oi_bobtype);
	clovis_idx_op_fail(oi, ar->ar_rc);

	M0_LEAVE();
}

/**
 * Callback for an index operation being finalised.
 *
 * @param oc The common callback struct for the operation being finialised.
 */
static void clovis_idx_op_cb_fini(struct m0_clovis_op_common *oc)
{
	struct m0_clovis_op_idx *oi;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	M0_PRE(oc->oc_op.op_size >= sizeof *oi);

	oi = bob_of(oc, struct m0_clovis_op_idx, oi_oc, &oi_bobtype);
	M0_PRE(m0_clovis__idx_op_invariant(oi));

	m0_clovis_op_common_bob_fini(&oi->oi_oc);
	m0_clovis_ast_rc_bob_fini(&oi->oi_ar);
	m0_clovis_op_idx_bob_fini(oi);

	M0_LEAVE();
}

/**
 * 'free entry' on the operations vector for index operations.
 *
 * @param oc operation being freed.
 */
static void clovis_idx_op_cb_free(struct m0_clovis_op_common *oc)
{
	struct m0_clovis_op_idx *oi;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	M0_PRE((oc->oc_op.op_size >= sizeof *oi));

	/* By now, fini() has been called and bob_of cannot be used */
	oi = M0_AMB(oi, oc, oi_oc);
	m0_free(oi);

	M0_LEAVE();
}

/**
 * Callback for an index operation being launched.
 * @param oc The common callback struct for the operation being launched.
 */
static void clovis_idx_op_cb_launch(struct m0_clovis_op_common *oc)
{
	int                             rc;
	struct m0_clovis               *m0c;
	struct m0_clovis_op            *op;
	struct m0_clovis_op_idx        *oi;
	struct m0_clovis_idx_query_ops *query_ops;

	int (*query) (struct m0_clovis_op_idx *);

	M0_ENTRY();

	M0_PRE(oc != NULL);
	op = &oc->oc_op;
	oi = bob_of(oc, struct m0_clovis_op_idx, oi_oc, &oi_bobtype);

	m0c = clovis_oi_instance(oi);
	query_ops = m0c->m0c_idx_svc_ctx.isc_service->is_query_ops;
	M0_ASSERT(query_ops != NULL);

	/* No harm to set the operation state here. */
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_LAUNCHED);

	/* Move to a different state and call the control function. */
	m0_sm_group_lock(&op->op_entity->en_sm_group);

	switch (op->op_code) {
	case M0_CLOVIS_EO_CREATE:
		m0_sm_move(&op->op_entity->en_sm, 0,
			   M0_CLOVIS_ES_CREATING);
		query = query_ops->iqo_namei_create;
		break;
	case M0_CLOVIS_EO_DELETE:
		m0_sm_move(&op->op_entity->en_sm, 0,
			   M0_CLOVIS_ES_DELETING);
		query = query_ops->iqo_namei_delete;
		break;
	case M0_CLOVIS_IC_GET:
		query = query_ops->iqo_get;
		break;
	case M0_CLOVIS_IC_PUT:
		query = query_ops->iqo_put;
		break;
	case M0_CLOVIS_IC_DEL:
		query = query_ops->iqo_del;
		break;
	case M0_CLOVIS_IC_NEXT:
		query = query_ops->iqo_next;
		break;
	case M0_CLOVIS_IC_LOOKUP:
		query = query_ops->iqo_namei_lookup;
		break;
	case M0_CLOVIS_IC_LIST:
		query = query_ops->iqo_namei_list;
		break;
	default:
		M0_IMPOSSIBLE("Management operation not implemented");
	}
	M0_ASSERT(query != NULL);

	m0_sm_group_unlock(&op->op_entity->en_sm_group);
	/*
	 * Returned value of query operations:
	 *  = 0: the query is executed synchronously and returns successfully.
	 *  < 0: the query fails.
	 *  = 1: the driver successes in launching the query asynchronously.
	*/
	rc = query(oi);
	oi->oi_ar.ar_rc = rc;
	if (rc < 0) {
		oi->oi_ar.ar_ast.sa_cb = &clovis_idx_op_ast_fail;
		m0_sm_ast_post(oi->oi_sm_grp, &oi->oi_ar.ar_ast);

	} else if (rc == 0) {
		oi->oi_ar.ar_ast.sa_cb = &clovis_idx_op_ast_complete;
		m0_sm_ast_post(oi->oi_sm_grp, &oi->oi_ar.ar_ast);
	}

	M0_LEAVE();
}

int m0_clovis_idx_op(struct m0_clovis_idx       *idx,
		     enum m0_clovis_idx_opcode   opcode,
		     struct m0_bufvec           *keys,
		     struct m0_bufvec           *vals,
		     int32_t                    *rcs,
		     uint32_t                    flags,
		     struct m0_clovis_op       **op)
{
	int rc;

	M0_ENTRY();

	M0_PRE(idx != NULL);
	M0_PRE(M0_IN(opcode, (M0_CLOVIS_IC_LOOKUP, M0_CLOVIS_IC_LIST,
			      M0_CLOVIS_IC_GET, M0_CLOVIS_IC_PUT,
			      M0_CLOVIS_IC_DEL, M0_CLOVIS_IC_NEXT)));
	M0_PRE(M0_IN(opcode, (M0_CLOVIS_IC_DEL,
			      M0_CLOVIS_IC_LOOKUP,
			      M0_CLOVIS_IC_LIST)) == (vals == NULL));
	M0_PRE(ergo(opcode != M0_CLOVIS_IC_LOOKUP, keys != NULL));
	M0_PRE(ergo(vals != NULL,
		    keys->ov_vec.v_nr == vals->ov_vec.v_nr));
	M0_PRE(ergo(opcode == M0_CLOVIS_IC_LIST,
		    m0_forall(i, keys->ov_vec.v_nr,
			      keys->ov_vec.v_count[i] ==
			      sizeof(struct m0_uint128))));
	M0_PRE(op != NULL);
	M0_PRE(ergo(flags == M0_OIF_SYNC_WAIT,
		    M0_IN(opcode, (M0_CLOVIS_IC_PUT, M0_CLOVIS_IC_DEL))));

	rc = m0_clovis_op_get(op, sizeof(struct m0_clovis_op_idx));
	if (rc == 0) {
		M0_ASSERT(*op != NULL);
		rc = clovis_idx_op_init(idx, opcode, keys, vals, rcs, flags,
					*op);
	}

	return M0_RC(rc);
}
M0_EXPORTED(m0_clovis_idx_op);

/**
 * Sets an entity operation to create or delete an index.
 *
 * @param entity entity to be modified.
 * @param op pointer to the operation being set.
 * @param opcode M0_CLOVIS_EO_CREATE or M0_CLOVIS_EO_DELETE.
 * @return 0 if the function succeeds or an error code otherwise.
 */
M0_INTERNAL int m0_clovis_idx_op_namei(struct m0_clovis_entity *entity,
				       struct m0_clovis_op **op,
				       enum m0_clovis_entity_opcode opcode)
{
	int                   rc;
	struct m0_clovis_idx *idx;

	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_PRE(op != NULL);

	rc = m0_clovis_op_get(op, sizeof(struct m0_clovis_op_idx));
	if (rc == 0) {
		M0_ASSERT(*op != NULL);
		idx = M0_AMB(idx, entity, in_entity);
		rc = clovis_idx_op_init(idx, opcode, NULL, NULL, NULL, 0,
					*op);
	}

	return M0_RC(rc);
}

void m0_clovis_idx_init(struct m0_clovis_idx    *idx,
			struct m0_clovis_realm  *parent,
			const struct m0_uint128 *id)
{
	M0_ENTRY();

	M0_PRE(idx != NULL);
	M0_PRE(parent != NULL);
	M0_PRE(id != NULL);

	/* Initalise the entity */
	m0_clovis_entity_init(&idx->in_entity, parent, id, M0_CLOVIS_ET_IDX);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_idx_init);

void m0_clovis_idx_fini(struct m0_clovis_idx *idx)
{
	M0_ENTRY();

	M0_PRE(idx != NULL);
	m0_clovis_entity_fini(&idx->in_entity);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_idx_fini);

M0_INTERNAL void m0_clovis_idx_service_config(struct m0_clovis *m0c,
		 			      int svc_id, void *svc_conf)
{
	struct m0_clovis_idx_service     *service;
	struct m0_clovis_idx_service_ctx *ctx;

	M0_PRE(m0c != NULL);
	M0_PRE(svc_id >= 0 && svc_id < M0_CLOVIS_IDX_MAX_SERVICE_ID);

	service = &clovis_idx_services[svc_id];
	ctx = &m0c->m0c_idx_svc_ctx;
	ctx->isc_service  = service;
	ctx->isc_svc_conf = svc_conf;
	ctx->isc_svc_inst = NULL;
}

M0_INTERNAL void m0_clovis_idx_service_register(int svc_id,
				struct m0_clovis_idx_service_ops *sops,
				struct m0_clovis_idx_query_ops   *qops)
{
	struct m0_clovis_idx_service *service;

	M0_PRE(svc_id >= 0 && svc_id < M0_CLOVIS_IDX_MAX_SERVICE_ID);

	service = &clovis_idx_services[svc_id];
	service->is_svc_ops = sops;
	service->is_query_ops = qops;
}

M0_INTERNAL void m0_clovis_idx_services_register(void)
{
	m0_clovis_idx_mock_register();
#ifdef CLOVIS_IDX_STORE_CASS
	m0_clovis_idx_cass_register();
#endif
	m0_clovis_idx_dix_register();
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
