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
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 15-Sep-2014
 */

#include "lib/vec.h"
#include "lib/types.h"
#include "lib/memory.h"               /* m0_alloc, m0_free */
#include "lib/errno.h"                /* ENOMEM */
#include "sm/sm.h"
#include "layout/layout.h"            /* m0_layout_instance_to_enum */
#include "ioservice/fid_convert.h"    /* M0_FID_GOB_CONTAINER_MASK */

#include "clovis/clovis_addb.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_layout.h"
#include "clovis/sync.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"
#include "lib/finject.h"
/**
 * Bob definitions
 */
const struct m0_bob_type oc_bobtype;
const struct m0_bob_type oo_bobtype;
const struct m0_bob_type op_bobtype;

M0_BOB_DEFINE(M0_INTERNAL, &oc_bobtype, m0_clovis_op_common);
M0_BOB_DEFINE(M0_INTERNAL, &oo_bobtype, m0_clovis_op_obj);
M0_BOB_DEFINE(M0_INTERNAL, &op_bobtype, m0_clovis_op);

const struct m0_bob_type oc_bobtype = {
	.bt_name         = "op_common_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_op_common, oc_magic),
	.bt_magix        = M0_CLOVIS_OC_MAGIC,
	.bt_check        = NULL,
};

const struct m0_bob_type oo_bobtype = {
	.bt_name         = "clovis_op_obj_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_op_obj, oo_magic),
	.bt_magix        = M0_CLOVIS_OO_MAGIC,
	.bt_check        = NULL,
};

const struct m0_bob_type op_bobtype = {
	.bt_name         = "clovis_op_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_op, op_magic),
	.bt_magix        = M0_CLOVIS_OP_MAGIC,
	.bt_check        = NULL,
};

const struct m0_bob_type ar_bobtype;
M0_BOB_DEFINE(M0_INTERNAL, &ar_bobtype,  m0_clovis_ast_rc);
const struct m0_bob_type ar_bobtype = {
	.bt_name         = "ar_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_ast_rc, ar_magic),
	.bt_magix        = M0_CLOVIS_AST_RC_MAGIC,
	.bt_check        = NULL,
};

/**
 * Pre-defined identifier of the over-arching realm.
 */
const struct m0_uint128 M0_CLOVIS_UBER_REALM = {0UL, 1ULL};
M0_EXPORTED(M0_CLOVIS_UBER_REALM);

/**
 * First identifier the application is allowed to use.
 * The first 0x100000 ids are reserved for use by clovis.
 */
const struct m0_uint128 M0_CLOVIS_ID_APP = { 0ULL, 0x100000ULL };
M0_EXPORTED(M0_CLOVIS_ID_APP);

enum { MAX_OPCODE = 256 };
static uint64_t opcount[MAX_OPCODE];

/**
 * State machine phases for clovis operations.
 * Note: sd_flags of OS_EXECUTED is set to M0_SDF_TERMINAL to allow an
 * APP to wait on this state
 */
struct m0_sm_state_descr clovis_op_phases[] = {
	[M0_CLOVIS_OS_INITIALISED] = {
		.sd_flags = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name = "initialised",
		.sd_allowed = M0_BITS(M0_CLOVIS_OS_LAUNCHED,
				      M0_CLOVIS_OS_FAILED),
	},
	[M0_CLOVIS_OS_LAUNCHED] = {
		.sd_name = "launched",
		.sd_allowed = M0_BITS(M0_CLOVIS_OS_EXECUTED,
				      M0_CLOVIS_OS_FAILED),
	},
	[M0_CLOVIS_OS_EXECUTED] = {
		.sd_name = "executed",
		.sd_allowed = M0_BITS(M0_CLOVIS_OS_STABLE,
				      M0_CLOVIS_OS_FAILED),
	},
	[M0_CLOVIS_OS_STABLE] = {
		.sd_flags = M0_SDF_FINAL | M0_SDF_TERMINAL,
		.sd_name = "stable",
	},
	[M0_CLOVIS_OS_FAILED] = {
		.sd_flags = M0_SDF_FINAL | M0_SDF_TERMINAL | M0_SDF_FAILURE,
		.sd_name = "failed",
	},
};

/**
 * Textual descriptions for the valid state machine transitions.
 */
struct m0_sm_trans_descr clovis_op_trans[] = {
	{"launching", M0_CLOVIS_OS_INITIALISED, M0_CLOVIS_OS_LAUNCHED},
	{"failed-to-launch", M0_CLOVIS_OS_INITIALISED, M0_CLOVIS_OS_FAILED},
	{"operation-executed", M0_CLOVIS_OS_LAUNCHED, M0_CLOVIS_OS_EXECUTED},
	{"operation-failed", M0_CLOVIS_OS_LAUNCHED, M0_CLOVIS_OS_FAILED},
	{"transaction-stable", M0_CLOVIS_OS_EXECUTED, M0_CLOVIS_OS_STABLE},
	{"transaction-failed", M0_CLOVIS_OS_EXECUTED, M0_CLOVIS_OS_FAILED},
};

/**
 * Configuration structure for the clovis operation state machine.
 */
struct m0_sm_conf clovis_op_conf = {
	.scf_name = "clovis-op-conf",
	.scf_nr_states = ARRAY_SIZE(clovis_op_phases),
	.scf_state = clovis_op_phases,
	.scf_trans = clovis_op_trans,
	.scf_trans_nr = ARRAY_SIZE(clovis_op_trans),
};

/**
 * State machine phases for clovis entities.
 */
struct m0_sm_state_descr clovis_entity_phases[] = {
	[M0_CLOVIS_ES_INIT] = {
		.sd_flags = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name = "init",
		.sd_allowed = M0_BITS(M0_CLOVIS_ES_CREATING,
				      M0_CLOVIS_ES_OPENING),
	},
	[M0_CLOVIS_ES_CREATING] = {
		.sd_name = "creating",
		.sd_allowed = M0_BITS(M0_CLOVIS_ES_OPEN, M0_CLOVIS_ES_FAILED),
	},
	[M0_CLOVIS_ES_DELETING] = {
		.sd_name = "deleting",
		.sd_allowed = M0_BITS(M0_CLOVIS_ES_INIT, M0_CLOVIS_ES_FAILED),
	},
	[M0_CLOVIS_ES_OPENING] = {
		.sd_name = "opening",
		.sd_allowed = M0_BITS(M0_CLOVIS_ES_OPEN, M0_CLOVIS_ES_FAILED),
	},
	[M0_CLOVIS_ES_OPEN] = {
		.sd_name = "open",
		.sd_allowed = M0_BITS(M0_CLOVIS_ES_DELETING,
				      M0_CLOVIS_ES_CLOSING,
				      M0_CLOVIS_ES_FAILED),
	},
	[M0_CLOVIS_ES_CLOSING] = {
		.sd_name = "closing",
		.sd_allowed = M0_BITS(M0_CLOVIS_ES_INIT, M0_CLOVIS_ES_FAILED),
	},
	[M0_CLOVIS_ES_FAILED] = {
		.sd_flags = M0_SDF_FINAL | M0_SDF_TERMINAL | M0_SDF_FAILURE,
		.sd_name = "failed",
	},
};

/**
 * Textual descriptions for the valid state machine transitions.
 */
struct m0_sm_trans_descr clovis_entity_trans[] = {
	{"creating", M0_CLOVIS_ES_INIT, M0_CLOVIS_ES_CREATING},
	{"deleting", M0_CLOVIS_ES_OPEN, M0_CLOVIS_ES_DELETING},
	{"opening", M0_CLOVIS_ES_INIT, M0_CLOVIS_ES_OPENING},
	{"finished-creating", M0_CLOVIS_ES_CREATING, M0_CLOVIS_ES_OPEN},
	{"finished-deleting", M0_CLOVIS_ES_DELETING, M0_CLOVIS_ES_INIT},
	{"open", M0_CLOVIS_ES_OPENING, M0_CLOVIS_ES_OPEN},
	{"failed-to-create", M0_CLOVIS_ES_CREATING, M0_CLOVIS_ES_FAILED},
	{"failed-to-delete", M0_CLOVIS_ES_DELETING, M0_CLOVIS_ES_FAILED},
	{"failed-to-open", M0_CLOVIS_ES_OPENING, M0_CLOVIS_ES_FAILED},
	{"closing", M0_CLOVIS_ES_OPEN, M0_CLOVIS_ES_CLOSING},
	{"failed-to-open", M0_CLOVIS_ES_OPEN, M0_CLOVIS_ES_FAILED},
	{"finished-closing", M0_CLOVIS_ES_CLOSING, M0_CLOVIS_ES_INIT},
	{"failed-to-close", M0_CLOVIS_ES_CLOSING, M0_CLOVIS_ES_FAILED},
};

/**
 * Configuration structure for the clovis entity state machine.
 */
struct m0_sm_conf clovis_entity_conf = {
	.scf_name = "clovis-entity-conf",
	.scf_nr_states = ARRAY_SIZE(clovis_entity_phases),
	.scf_state = clovis_entity_phases,
	.scf_trans = clovis_entity_trans,
	.scf_trans_nr = ARRAY_SIZE(clovis_entity_trans),
};

/**----------------------------------------------------------------------------*
 *                                Helper functions                             *
 *-----------------------------------------------------------------------------*/

M0_INTERNAL struct m0_clovis *
m0_clovis__entity_instance(const struct m0_clovis_entity *entity)
{
	M0_PRE(entity != NULL);
	M0_PRE(entity->en_realm != NULL);
	M0_PRE(entity->en_realm->re_instance != NULL);

	return entity->en_realm->re_instance;
}

M0_INTERNAL struct m0_clovis *
m0_clovis__op_instance(const struct m0_clovis_op *op)
{
	struct m0_clovis_entity *entity;
	M0_PRE(op != NULL);

	entity = op->op_code == M0_CLOVIS_EO_SYNC ?
		m0_clovis__op_sync_entity(op) : op->op_entity;
	M0_PRE(entity != NULL);

	return m0_clovis__entity_instance(entity);
}

M0_INTERNAL struct m0_clovis_op *
m0_clovis__ioo_to_op(struct m0_clovis_op_io *ioo)
{
	return &ioo->ioo_oo.oo_oc.oc_op;
}

M0_INTERNAL bool
m0_clovis__is_oostore(struct m0_clovis *instance)
{
	return instance->m0c_config->cc_is_oostore;
}

M0_INTERNAL struct m0_clovis *
m0_clovis__obj_instance(const struct m0_clovis_obj *obj)
{
	M0_PRE(obj != NULL);

	return m0_clovis__entity_instance(&obj->ob_entity);
}

/**
 * Pick a locality: Mero and the new locality interface(chore) now uses TLS to
 * store data and these data are set when a "mero" thread is created.
 * An application thread (not the main thread calling m0_init, considering ST
 * multi-threading framework), it doesn't have the same TLS by nature, which
 * causes a problem when it calls mero functions like m0_locality_here/get
 * directly as below.
 *
 * Ensure to use m0_thread_adopt/shun to make a thread (non-)meroism when a
 * thread starts/ends.
 *
 * TODO: more intelligent locality selection policy based on fid and workload.
 */
M0_INTERNAL struct m0_locality *
m0_clovis__locality_pick(struct m0_clovis *cinst)
{
	return  m0_locality_here();
}

M0_INTERNAL bool m0_clovis_entity_type_is_valid(enum m0_clovis_entity_type type)
{
	return M0_IN(type, (M0_CLOVIS_ET_OBJ, M0_CLOVIS_ET_IDX));
}

M0_INTERNAL bool clovis_entity_invariant_full(struct m0_clovis_entity *ent)
{
	bool                rc = false;
	struct m0_sm_group *grp;

	grp = &ent->en_sm_group;
	if (!m0_sm_group_is_locked(grp)) {
		m0_sm_group_lock(grp);
		rc = clovis_entity_invariant_locked(ent);
		m0_sm_group_unlock(grp);
	}
	return M0_RC(rc);
}

/**
 * Entity invariant. Checks type and state machine.
 *
 * @param ent An entity.
 * @return Whether the entities type is valid.
 */
M0_INTERNAL bool clovis_entity_invariant_locked(const struct
						m0_clovis_entity *ent)
{
	return M0_RC(ent != NULL &&
		     m0_sm_group_is_locked(&ent->en_sm_group) &&
		     m0_sm_invariant(&ent->en_sm) &&
		     m0_clovis_entity_type_is_valid(ent->en_type));
}

/**
 * Op invariant. Checks op's bob.
 *
 * @param op An op.
 * @return Whether the entities type is valid.
 */
M0_INTERNAL bool clovis_op_invariant(const struct m0_clovis_op *op)
{
	return M0_RC(op != NULL &&
	       op->op_size >= sizeof(struct m0_clovis_op_common) &&
	       m0_clovis_op_bob_check(op));
}

/**
 * Checks if entiry is valid for NON-SYNC ops.
 */
M0_INTERNAL bool clovis_op_entity_invariant(const struct m0_clovis_op *op)
{
	return (op->op_entity == NULL) == (op->op_code == M0_CLOVIS_EO_SYNC) &&
	       ergo(op->op_entity != NULL,
		    clovis_entity_invariant_full(op->op_entity));
}

/**
 * Check if entity's id is valid.
 */
M0_INTERNAL bool clovis_entity_id_is_valid(const struct m0_uint128 *id)
{
	if (m0_uint128_cmp(&M0_CLOVIS_ID_APP, id) >= 0) {
		M0_LOG(M0_ERROR, "Invalid entity fid detected: "
		       "<%"PRIx64 ":%"PRIx64 ">."
		       "Entity id should larger than M0_CLOVIS_ID_APP.",
		       id->u_hi, id->u_lo);
		return false;
	}

	return true;
}

/**----------------------------------------------------------------------------*
 *                       Entity, Object and Index                              *
 *-----------------------------------------------------------------------------*/

M0_INTERNAL void m0_clovis_entity_init(struct m0_clovis_entity *entity,
				       struct m0_clovis_realm  *parent,
				       const struct m0_uint128 *id,
				       const enum m0_clovis_entity_type type)
{
	struct m0_sm_group *grp;

	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_PRE(parent != NULL);
	M0_PRE(parent->re_instance != NULL);
	M0_PRE(id != NULL);
	M0_PRE(clovis_entity_id_is_valid(id));
	M0_PRE(m0_sm_conf_is_initialized(&clovis_entity_conf));
	M0_PRE(m0_clovis_entity_type_is_valid(type));

	/* initalise the entity */
	entity->en_type = type;
	entity->en_id = *id;
	entity->en_realm = parent;

	/* initalise the state machine */
	grp = &entity->en_sm_group;
	m0_sm_group_init(grp);
	m0_sm_init(&entity->en_sm, &clovis_entity_conf, M0_CLOVIS_ES_INIT, grp);
	m0_mutex_init(&entity->en_pending_tx_lock);
	spti_tlist_init(&entity->en_pending_tx);
	M0_ASSERT(clovis_entity_invariant_full(entity));
	M0_LEAVE();
}

void m0_clovis_obj_init(struct m0_clovis_obj    *obj,
			struct m0_clovis_realm  *parent,
			const struct m0_uint128 *id,
			uint64_t                 layout_id)
{
	M0_ENTRY();

	M0_PRE(obj != NULL);
	M0_PRE(parent != NULL);
	M0_PRE(id != NULL);
	M0_PRE(clovis_entity_id_is_valid(id));
	M0_PRE(M0_IS0(obj));

	/* Initalise the entity */
	m0_clovis_entity_init(&obj->ob_entity, parent, id, M0_CLOVIS_ET_OBJ);

	/* set the blocksize to a reasonable default */
	obj->ob_attr.oa_bshift = CLOVIS_DEFAULT_BUF_SHIFT;
	M0_ASSERT(layout_id > 0 && layout_id < m0_lid_to_unit_map_nr);
	obj->ob_attr.oa_layout_id = layout_id;

#ifdef CLOVIS_OSYNC
	m0_mutex_init(&obj->ob_pending_tx_lock);
	ospti_tlist_init(&obj->ob_pending_tx);
#endif

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_obj_init);

void m0_clovis_entity_fini(struct m0_clovis_entity *entity)
{
	struct m0_reqh_service_txid *iter;

	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_PRE(M0_IN(entity->en_sm.sm_state,
		     (M0_CLOVIS_ES_INIT, M0_CLOVIS_ES_OPEN,
		      M0_CLOVIS_ES_FAILED)));
	M0_ASSERT(clovis_entity_invariant_full(entity));

	m0_tl_teardown(spti, &entity->en_pending_tx, iter)
		m0_free0(&iter);
	spti_tlist_fini(&entity->en_pending_tx);
	m0_mutex_fini(&entity->en_pending_tx_lock);
	m0_sm_group_lock(&entity->en_sm_group);
	if (entity->en_sm.sm_state == M0_CLOVIS_ES_OPEN) {
		m0_sm_move(&entity->en_sm, 0, M0_CLOVIS_ES_CLOSING);
		m0_sm_move(&entity->en_sm, 0, M0_CLOVIS_ES_INIT);
	}
	m0_sm_fini(&entity->en_sm);
	m0_sm_group_unlock(&entity->en_sm_group);
	m0_sm_group_fini(&entity->en_sm_group);
	M0_SET0(entity);
	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_entity_fini);

void m0_clovis_obj_fini(struct m0_clovis_obj *obj)
{

	M0_CLOVIS_THREAD_ENTER;

	M0_ENTRY();
	M0_PRE(obj != NULL);

	/* Cleanup layout. */
	if (obj->ob_layout != NULL) {
		m0_clovis__layout_put(obj->ob_layout);
		m0_clovis_layout_free(obj->ob_layout);
		obj->ob_layout = NULL;
	}
	m0_clovis_entity_fini(&obj->ob_entity);
	M0_SET0(obj);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_obj_fini);


/**----------------------------------------------------------------------------*
 *                                Operations                                   *
 *-----------------------------------------------------------------------------*/

/**
 * This executes the optional user-provided callback.
 * Called with the group lock held.
 *
 * @param mach The state machine which has changed to this state.
 * @return -1, stay in this state.
 */
M0_INTERNAL int m0_clovis_op_executed(struct m0_clovis_op *op)
{
	M0_ENTRY();

	M0_ASSERT(clovis_op_invariant(op));

	/* Call the op:executed callback if one is present */
	if (op->op_cbs != NULL && op->op_cbs->oop_executed != NULL)
		op->op_cbs->oop_executed(op);

	return M0_RC(-1);
}

/**
 * This in turn finds and executes the optional user-provided callback.
 * Called with the group lock held.
 *
 * @param mach The state machine which has changed to this state.
 * @return -1, stay in this state.
 */
M0_INTERNAL int m0_clovis_op_stable(struct m0_clovis_op *op)
{
	struct m0_clovis *m0c;

	M0_ENTRY();

	M0_ASSERT(clovis_op_invariant(op));

	m0c = m0_clovis__op_instance(op);
	M0_PRE(m0c != NULL);

	/* Call the op:stable callback if one is present */
	if (op->op_cbs != NULL && op->op_cbs->oop_stable != NULL)
		op->op_cbs->oop_stable(op);
	if (!M0_FI_ENABLED("skip_ongoing_io_ref"))
		m0_clovis__io_ref_put(m0c);

	return M0_RC(-1);
}

/**
 * Callback triggered by the state machine entering the 'FAILED' state.
 * This in turn finds and executes the optional user-provided callback.
 * Called with the group lock held.
 *
 * @param mach The state machine which has changed to this state.
 * @return -1, stay in this state.
 */
M0_INTERNAL int m0_clovis_op_failed(struct m0_clovis_op *op)
{
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_ASSERT(clovis_op_invariant(op));

	m0c = m0_clovis__op_instance(op);
	M0_PRE(m0c != NULL);

	/* Call the op:failed callback if one is present */
	if (op->op_cbs != NULL && op->op_cbs->oop_failed != NULL)
		op->op_cbs->oop_failed(op);

	if (!M0_FI_ENABLED("skip_ongoing_io_ref"))
		m0_clovis__io_ref_put(m0c);

	return M0_RC(-1);
}

M0_INTERNAL int m0_clovis_op_get(struct m0_clovis_op **op, size_t size)
{
	int rc = 0;

	M0_ENTRY();
	M0_PRE(op != NULL);

	/* Allocate the op if necessary. */
	if (*op == NULL) {
		rc = m0_clovis_op_alloc(op, size);
		if (rc != 0)
			return M0_ERR(rc);
	} else {
		size_t cached_size = (*op)->op_size;

		if ((*op)->op_size < size)
			return M0_ERR(-EMSGSIZE);

		/* 0 the pre-allocated operation. */
		memset(*op, 0, cached_size);
		(*op)->op_size = size;
	}
	m0_mutex_init(&(*op)->op_pending_tx_lock);
	spti_tlist_init(&(*op)->op_pending_tx);
	return M0_RC(0);
}

/**
 * Cancel a launched single operation, called from m0_clovis_op_cancel.
 *
 */
M0_INTERNAL void m0_clovis_op_cancel_one(struct m0_clovis_op *op)
{
	struct m0_clovis_op_common *oc;

	M0_ENTRY();

	M0_PRE(clovis_op_invariant(op));
	M0_PRE(M0_IN(op->op_sm.sm_state, (M0_CLOVIS_OS_LAUNCHED,
					  M0_CLOVIS_OS_EXECUTED,
					  M0_CLOVIS_OS_STABLE,
					  M0_CLOVIS_OS_FAILED)));

	oc = bob_of(op, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	M0_PRE(oc->oc_cb_cancel != NULL);

	m0_sm_group_lock(&op->op_sm_group);

	if (!M0_IN(op->op_sm.sm_state, (M0_CLOVIS_OS_STABLE,
				        M0_CLOVIS_OS_FAILED))) {
		/* cancel only initialized, launched or executed op */
		oc->oc_cb_cancel(oc);
	}

	/* Call the op-type's cancel function */
	m0_sm_group_unlock(&op->op_sm_group);

	M0_LEAVE();
}

void m0_clovis_op_cancel(struct m0_clovis_op **op, uint32_t nr)
{
	int i;
	struct m0_clovis_entity *entity;

	M0_ENTRY();
	M0_PRE(op != NULL);

	for (i = 0; i < nr; i++) {
		entity = op[i]->op_entity;
		if (entity->en_type == M0_CLOVIS_ET_OBJ)
			m0_clovis_op_cancel_one(op[i]);
	}

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_op_cancel);

static void addb2_add_op_attrs(const struct m0_clovis_op *op)
{
	struct m0_clovis_entity *entity;
	uint64_t                 oid;

	M0_PRE(op != NULL);

	oid  = m0_sm_id_get(&op->op_sm);
	entity = op->op_code == M0_CLOVIS_EO_SYNC ?
		m0_clovis__op_sync_entity(op) : op->op_entity;

	if (entity != NULL)
		M0_ADDB2_ADD(M0_AVI_ATTR, oid, M0_AVI_CLOVIS_OP_ATTR_ENTITY_ID,
			     entity->en_type);
	M0_ADDB2_ADD(M0_AVI_ATTR, oid, M0_AVI_CLOVIS_OP_ATTR_CODE,
		     op->op_code);
}

/**
 * Launches a single operation, called from m0_clovis_op_launch.
 *
 * PRE(op != NULL);
 * PRE(op->op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
 * PRE(ergo(op->op_entity != NULL, clovis_entity_invariant_full(op->op_entity));
 * PRE((op->op_size >= sizeof(*oc)));
 *
 * @param op The operation to be launch
 */
M0_INTERNAL void m0_clovis_op_launch_one(struct m0_clovis_op *op)
{
	struct m0_clovis_op_common *oc;
	struct m0_clovis           *m0c;
	int                         rc;

	M0_ENTRY();

	M0_PRE(clovis_op_invariant(op));
	M0_PRE(op->op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	/* SYNC op doesn't have `entity`. */
	M0_PRE(clovis_op_entity_invariant(op));

	oc = bob_of(op, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	M0_PRE(oc->oc_cb_launch != NULL);

	m0c = m0_clovis__op_instance(op);
	M0_PRE(m0c != NULL);

	rc = m0_clovis__io_ref_get(m0c);
	if (rc != 0) {
		m0_sm_group_lock(&op->op_sm_group);
		m0_sm_move(&op->op_sm, rc, M0_CLOVIS_OS_FAILED);
		op->op_rc = rc;
		m0_sm_group_unlock(&op->op_sm_group);
		return;
	}

	addb2_add_op_attrs(op);

	m0_sm_group_lock(&op->op_sm_group);

	/* Call the op-type's launch function */
	oc->oc_cb_launch(oc);
	m0_sm_group_unlock(&op->op_sm_group);

	M0_LEAVE();
}

void m0_clovis_op_launch(struct m0_clovis_op **op, uint32_t nr)
{
	int i;

	M0_ENTRY();
	M0_PRE(op != NULL);

	for (i = 0; i < nr; i++)
		m0_clovis_op_launch_one(op[i]);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_op_launch);

int32_t m0_clovis_op_wait(struct m0_clovis_op *op, uint64_t bits,
			  m0_time_t to)
{
	int32_t   rc;

	M0_ENTRY();

	M0_PRE(clovis_op_invariant(op));
	M0_PRE(bits != 0);
	/* Check bits only contains bits for these states */
	M0_PRE((bits & ~M0_BITS(M0_CLOVIS_OS_LAUNCHED,
				M0_CLOVIS_OS_EXECUTED,
				M0_CLOVIS_OS_STABLE,
				M0_CLOVIS_OS_FAILED)) == 0);

	/* Perform a timed wait */
	m0_sm_group_lock(&op->op_sm_group);
	rc = m0_sm_timedwait(&op->op_sm, bits, to);
	m0_sm_group_unlock(&op->op_sm_group);

	/*
	 * No point checking sm in one of bits states - it may have moved
	 * while we released the locks
	 */
	return M0_RC(rc);
}
M0_EXPORTED(m0_clovis_op_wait);

/**
 * Allocates memory for an operation.
 *
 * @remarks For each m0_clovis_op the implementation maintains some additional
 * data. Therefore, the amount of memory allocated by this function is typically
 * greater than sizeof(struct m0_clovis_op)
 * @param[out] op Pointer to the allocated operation.
 * @param op_size Size of the operation to be allocated. Must be greater than
 * sizeof(struct m0_clovis_op_common).
 * @return 0 if the operation was correctly allocated or -ENOMEM if it not
 * enough memory was available.
 */
M0_INTERNAL int m0_clovis_op_alloc(struct m0_clovis_op **op, size_t op_size)
{
	M0_ENTRY();

	M0_PRE(op != NULL);
	M0_PRE(*op == NULL);
	M0_PRE(op_size >= sizeof(struct m0_clovis_op_common));

	/* Allocate the operation */
	*op = m0_alloc(op_size);
	if (*op == NULL)
		return M0_ERR(-ENOMEM);
	(*op)->op_size = op_size;

	return M0_RC(0);
}

/**
 * Initialises a clovis operation in its most generic form.
 *
 * @param op Operation being initialised.
 * @param conf Configuration for the operation state machine.
 * @param entity Entity the operation is targeted to.
 * @return 0 if the operation succeeds. -EMGSIZE if the operation was
 * pre-allocated with the wrong size. -ENOMEM if it was not possible to
 * allocate memory for the operation.
 */
M0_INTERNAL int m0_clovis_op_init(struct m0_clovis_op *op,
				  const struct m0_sm_conf *conf,
				  struct m0_clovis_entity *entity)
{
	struct m0_sm_group *grp;

	M0_ENTRY();

	if (M0_FI_ENABLED("fail_op_init"))
		return M0_ERR(-EINVAL);

	M0_PRE(op != NULL);
	M0_PRE(conf != NULL);
	M0_PRE(m0_sm_conf_is_initialized(conf));
	M0_ASSERT(ergo(entity != NULL, clovis_entity_invariant_full(entity)));

	/* Initialise the operation. */
	m0_clovis_op_bob_init(op);
	op->op_entity = entity;

	/* XXX initialise a cookie? or wait for launch to do that... */
	/*(*op)->op_gen = ?;*/

	/* Initialise the state machine. */
	grp = &op->op_sm_group;
	m0_sm_group_init(grp);
	m0_sm_init(&op->op_sm, conf, M0_CLOVIS_OS_INITIALISED, grp);
	m0_sm_addb2_counter_init(&op->op_sm);

	M0_ASSERT(IS_IN_ARRAY(op->op_code, opcount));
	op->op_count = opcount[op->op_code]++; /* XXX lock! */

	/* m0_sm_invariant must be checked under sm_group lock. */
	m0_sm_group_lock(grp);
	M0_POST(m0_sm_invariant(&op->op_sm));
	m0_sm_group_unlock(grp);
	m0_mutex_init(&op->op_priv_lock);

	return M0_RC(0);
}

void m0_clovis_op_fini(struct m0_clovis_op *op)
{
	struct m0_clovis_op_common *oc;
	struct m0_sm_group         *grp;

	M0_ENTRY();

	M0_PRE(op != NULL);
	M0_PRE(M0_IN(op->op_sm.sm_state, (M0_CLOVIS_OS_INITIALISED,
					  M0_CLOVIS_OS_STABLE,
					  M0_CLOVIS_OS_FAILED)));
	M0_PRE(op->op_size >= sizeof *oc);

	oc = bob_of(op, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	if (oc->oc_cb_fini != NULL)
		oc->oc_cb_fini(oc);
	m0_mutex_fini(&op->op_priv_lock);

	grp = &op->op_sm_group;
	m0_sm_group_lock(grp);
	m0_sm_fini(&op->op_sm);
	op->op_sm.sm_state = M0_CLOVIS_OS_UNINITIALISED;
	m0_sm_group_unlock(grp);
	m0_sm_group_fini(grp);

	/* Finalise op's bob */
	m0_clovis_op_bob_fini(op);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_op_fini);

void m0_clovis_op_free(struct m0_clovis_op *op)
{
	struct m0_clovis_op_common *oc;
	struct m0_clovis_op_obj    *oo;

	M0_ENTRY();

	M0_PRE(op != NULL);
	M0_PRE(op->op_sm.sm_state == M0_CLOVIS_OS_UNINITIALISED);
	M0_PRE((op->op_size >= sizeof *oc));

	/* bob_fini has been called in op_fini, using M0_AMB() here. */
	oc = M0_AMB(oc, op, oc_op);
	oo = M0_AMB(oo, oc, oo_oc);
	if (oc->oc_cb_free != NULL)
		oc->oc_cb_free(oc);
	else
		m0_free(oo);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_op_free);

void m0_clovis_op_setup(struct m0_clovis_op *op,
			const struct m0_clovis_op_ops *cbs,
			m0_time_t linger)
{
	M0_ENTRY();

	M0_PRE(op != NULL);
	M0_PRE(op->op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);

	op->op_cbs = cbs;
	op->op_linger = linger;

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_op_setup);

void m0_clovis_op_kick(struct m0_clovis_op *op)
{
	M0_ENTRY();

	M0_PRE(op != NULL);
	M0_PRE(op->op_sm.sm_state >= M0_CLOVIS_OS_INITIALISED);

	/** @todo: put pressure on the rpc system to send this rpc message */
	/** @todo: send an fsync-force fop to hurry the placement of this
	 *         transaction */
	/** @todo: could release/acquire the group lock in op_launch, and
	 *         test whether the op is already in launched, allowing
	 *         operations that are to-be-launched to be launched from
	 *         here */

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_op_kick);

int32_t m0_clovis_rc(const struct m0_clovis_op *op)
{
	M0_ENTRY();
	M0_PRE(op != NULL);
	return M0_RC(op->op_rc);
}
M0_EXPORTED(m0_clovis_rc);

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
