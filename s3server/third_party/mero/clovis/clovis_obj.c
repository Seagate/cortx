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
 *                   Abhishek Saha   <abhishek.saha@seagate.com>
 * Original creation date: 5-Oct-2014
 */

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_layout.h"
#include "clovis/clovis_idx.h"
#include "clovis/sync.h"

#include "lib/errno.h"
#include "fid/fid.h"               /* m0_fid */
#include "lib/locality.h"          /* m0_locality_here() */
#include "lib/misc.h"              /* m0_strtou64() */
#include "ioservice/fid_convert.h" /* m0_fid_convert_ */
#include "layout/layout.h"         /* m0_lid_to_unit_map */
#include "conf/helpers.h"          /* m0_confc_root_open */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"
#include "lib/finject.h"


M0_INTERNAL struct m0_clovis*
m0_clovis__oo_instance(struct m0_clovis_op_obj *oo)
{
	M0_PRE(oo != NULL);

	return m0_clovis__entity_instance(oo->oo_oc.oc_op.op_entity);
}

M0_INTERNAL struct m0_clovis_obj*
m0_clovis__obj_entity(struct m0_clovis_entity *entity)
{
	struct m0_clovis_obj *obj;

	M0_PRE(entity != NULL);

	return M0_AMB(obj, entity, ob_entity);
}

/**
 * Checks the common part of an operation is not malformed or corrupted.
 *
 * @param oc operation common to be checked.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
static bool clovis_op_common_invariant(struct m0_clovis_op_common *oc)
{
	return M0_RC(oc != NULL &&
		     m0_clovis_op_common_bob_check(oc));
}

M0_INTERNAL bool m0_clovis_op_obj_ast_rc_invariant(struct m0_clovis_ast_rc *ar)
{
	return M0_RC(ar != NULL &&
		     m0_clovis_ast_rc_bob_check(ar));
}

M0_INTERNAL bool m0_clovis_op_obj_invariant(struct m0_clovis_op_obj *oo)
{
	return M0_RC(oo != NULL &&
		     m0_clovis_op_obj_bob_check(oo) &&
		     oo->oo_oc.oc_op.op_size >= sizeof *oo &&
		     m0_clovis_op_obj_ast_rc_invariant(&oo->oo_ar) &&
		     clovis_op_common_invariant(&oo->oo_oc));
}

/**
 * Checks an object operation is not malformed or corrupted.
 *
 * @param oo object operation to be checked.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
static bool clovis_obj_op_obj_invariant(struct m0_clovis_op_obj *oo)
{
	bool rc;

	/* Don't use a m0_clovis__xxx_instance, as they assert */
	if(oo == NULL)
		rc = false;
	else if(oo->oo_oc.oc_op.op_entity == NULL)
		rc = false;
	else if(oo->oo_oc.oc_op.op_entity->en_realm == NULL)
		rc = false;
	else if(oo->oo_oc.oc_op.op_entity->en_realm->re_instance == NULL)
		rc = false;
	else
		rc = true;

	return M0_RC(rc);
}

static bool clovis_obj_layout_id_invariant(uint64_t layout_id)
{

	int      ltype = M0_CLOVIS_OBJ_LAYOUT_TYPE(layout_id);
	uint64_t lid = M0_CLOVIS_OBJ_LAYOUT_ID(layout_id);

	return M0_RC(M0_IN(ltype, (M0_CLOVIS_LT_PDCLUST,
				   M0_CLOVIS_LT_COMPOSITE,
				   M0_CLOVIS_LT_CAPTURE)) &&
		     lid > 0 && lid < m0_lid_to_unit_map_nr);
}

M0_INTERNAL uint64_t m0_clovis__obj_lid(struct m0_clovis_obj *obj)
{
	uint64_t lid;

	M0_ENTRY();
	M0_PRE(obj != NULL);
	M0_PRE(clovis_obj_layout_id_invariant(obj->ob_attr.oa_layout_id));
	lid = M0_CLOVIS_OBJ_LAYOUT_ID(obj->ob_attr.oa_layout_id);
	M0_LEAVE();
	return lid;
}

M0_INTERNAL enum m0_clovis_layout_type
m0_clovis__obj_layout_type(struct m0_clovis_obj *obj)
{
	int type;

	M0_ENTRY();
	M0_PRE(obj != NULL);
	M0_PRE(clovis_obj_layout_id_invariant(obj->ob_attr.oa_layout_id));
	type = M0_CLOVIS_OBJ_LAYOUT_TYPE(obj->ob_attr.oa_layout_id);
	M0_LEAVE();
	return type;
}

M0_INTERNAL void m0_clovis__obj_attr_set(struct m0_clovis_obj *obj,
					 struct m0_fid pver,
					 uint64_t layout_id)
{
	M0_ENTRY();
	M0_PRE(obj != NULL);
	M0_PRE(clovis_obj_layout_id_invariant(layout_id));
	obj->ob_attr.oa_layout_id = layout_id;
	obj->ob_attr.oa_pver = pver;
	M0_LEAVE();
}

M0_INTERNAL struct m0_fid m0_clovis__obj_pver(struct m0_clovis_obj *obj)
{
	return obj->ob_attr.oa_pver;
}

M0_INTERNAL bool
m0_clovis__obj_pool_version_is_valid(const struct m0_clovis_obj *obj)
{
	struct m0_clovis *cinst;

	M0_PRE(obj != NULL);
	cinst = m0_clovis__entity_instance(&obj->ob_entity);
	return m0_conf_fid_is_valid(&obj->ob_attr.oa_pver) && (
	       m0_pool_version_find(&cinst->m0c_pools_common,
		                    &obj->ob_attr.oa_pver) != NULL);
}

/**
 * Cancels all the fops that are sent during launch operation
 *
 * @param oc operation being launched. Note the operation is of type
 * m0_clovis_op_common although it has to be allocated as a
 * m0_clovis_op_obj.
 */
static void clovis_obj_namei_cb_cancel(struct m0_clovis_op_common *oc)
{
	int                      rc;
	struct m0_clovis_op_obj *oo;
	struct m0_clovis_op     *op;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	op = &oc->oc_op;
	M0_PRE(op->op_entity != NULL);
	M0_PRE(m0_sm_group_is_locked(&op->op_sm_group));
	M0_PRE(M0_IN(op->op_code, (M0_CLOVIS_EO_CREATE,
				   M0_CLOVIS_EO_DELETE,
				   M0_CLOVIS_EO_OPEN)));

	oo = bob_of(oc, struct m0_clovis_op_obj, oo_oc, &oo_bobtype);
	M0_PRE(clovis_obj_op_obj_invariant(oo));

	rc = m0_clovis__obj_namei_cancel(op);
	op->op_rc = rc;
	M0_LEAVE();
}

/**
 * 'launch entry' on the vtable for obj namespace manipulation. This clovis
 * callback gets invoked when launching a create/delete object operation.
 *
 * @param oc operation being launched. Note the operation is of type
 * m0_clovis_op_common although it has to be allocated as a
 * m0_clovis_op_obj.
 */
static void clovis_obj_namei_cb_launch(struct m0_clovis_op_common *oc)
{
	int                      rc;
	struct m0_clovis_op_obj *oo;
	struct m0_clovis_op     *op;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	op = &oc->oc_op;
	M0_PRE(op->op_entity != NULL);
	M0_PRE(m0_uint128_cmp(&M0_CLOVIS_ID_APP,
			      &op->op_entity->en_id) < 0);
	M0_PRE(M0_IN(op->op_code, (M0_CLOVIS_EO_CREATE,
				   M0_CLOVIS_EO_DELETE,
				   M0_CLOVIS_EO_OPEN)));
	M0_PRE(M0_IN(op->op_entity->en_sm.sm_state, (M0_CLOVIS_ES_INIT,
						     M0_CLOVIS_ES_OPEN)));
	M0_PRE(m0_sm_group_is_locked(&op->op_sm_group));
	M0_ASSERT(clovis_entity_invariant_full(op->op_entity));

	oo = bob_of(oc, struct m0_clovis_op_obj, oo_oc, &oo_bobtype);
	M0_PRE(clovis_obj_op_obj_invariant(oo));

	/* Move to a different state and call the control function. */
	m0_sm_group_lock(&op->op_entity->en_sm_group);
	switch (op->op_code) {
		case M0_CLOVIS_EO_CREATE:
			m0_sm_move(&op->op_entity->en_sm, 0,
				   M0_CLOVIS_ES_CREATING);
			break;
		case M0_CLOVIS_EO_DELETE:
			m0_sm_move(&op->op_entity->en_sm, 0,
				   M0_CLOVIS_ES_DELETING);
			break;
		case M0_CLOVIS_EO_OPEN:
			m0_sm_move(&op->op_entity->en_sm, 0,
				   M0_CLOVIS_ES_OPENING);
			break;
		default:
			M0_IMPOSSIBLE("Management operation not implemented");
	}
	m0_sm_group_unlock(&op->op_entity->en_sm_group);

	rc = m0_clovis__obj_namei_send(oo);
	if (rc == 0)
		m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_LAUNCHED);

	M0_LEAVE();
}

/**
 * 'free entry' on the vtable for obj namespace manipulation. This callback
 * gets invoked when freeing an operation.
 *
 * @param oc operation being freed. Note the operation is of type
 * m0_clovis_op_common although it has to be allocated as a
 * m0_clovis_op_obj.
 */
static void clovis_obj_namei_cb_free(struct m0_clovis_op_common *oc)
{
	struct m0_clovis_op_obj *oo;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	M0_PRE((oc->oc_op.op_size >= sizeof *oo));

	/* By now, fini() has been called and bob_of cannot be used */
	oo = M0_AMB(oo, oc, oo_oc);
	M0_PRE(clovis_obj_op_obj_invariant(oo));

	m0_free(oo);

	M0_LEAVE();
}

/**
 * 'op fini entry' on the vtable for entities. This clovis callback gets invoked
 * when a create/delete operation on an object gets finalised.
 *
 * @param oc operation being finalised. Note the operation is of type
 * m0_clovis_op_common although it has to be allocated as a
 * m0_clovis_op_obj.
 */
static void clovis_obj_namei_cb_fini(struct m0_clovis_op_common *oc)
{
	struct m0_clovis_op_obj *oo;
	struct m0_clovis        *cinst;

	M0_ENTRY();
	M0_PRE(oc != NULL);
	M0_PRE(oc->oc_op.op_size >= sizeof *oo);

	oo = bob_of(oc, struct m0_clovis_op_obj, oo_oc, &oo_bobtype);
	M0_PRE(m0_clovis_op_obj_invariant(oo));

	cinst = m0_clovis__oo_instance(oo);
	M0_ASSERT(cinst != NULL);

	if (oo->oo_layout_instance != NULL) {
		m0_layout_instance_fini(oo->oo_layout_instance);
		oo->oo_layout_instance = NULL;
	}

	m0_clovis_op_common_bob_fini(&oo->oo_oc);
	m0_clovis_ast_rc_bob_fini(&oo->oo_ar);
	m0_clovis_op_obj_bob_fini(oo);

	M0_SET0(&oo->oo_fid);

#ifdef CLOVIS_FOR_M0T1FS
	M0_SET0(&oo->oo_pfid);
	m0_buf_free(&oo->oo_name);
#endif

	M0_LEAVE();
}

M0_INTERNAL int
m0_clovis__obj_pool_version_get(struct m0_clovis_obj *obj,
				struct m0_pool_version **pv)
{
	int               rc;
	struct m0_clovis *cinst;
	struct m0_fid    *pool;

	M0_ENTRY();

	cinst = m0_clovis__obj_instance(obj);

	if (pv == NULL)
		return M0_ERR(-ENOENT);

	if (M0_FI_ENABLED("fake_pool_version")) {
		*pv = cinst->m0c_pools_common.pc_cur_pver;
		return 0;
	}

	/** Validate the cached pool version. */
	if (m0_clovis__obj_pool_version_is_valid(obj)) {
		*pv = m0_pool_version_find(&cinst->m0c_pools_common,
					   &obj->ob_attr.oa_pver);
		rc = (*pv != NULL)? 0 : -ENOENT;
	} else {
		pool = m0_fid_is_set(&obj->ob_attr.oa_pool)?
		       &obj->ob_attr.oa_pool : NULL;
		rc = m0_pool_version_get(&cinst->m0c_pools_common, pool, pv);
		if (rc != 0)
			return M0_ERR(rc);
		M0_ASSERT(*pv != NULL);
		obj->ob_attr.oa_pver = (*pv)->pv_id;
	}

	return M0_RC(rc);
}

M0_INTERNAL uint64_t
m0_clovis__obj_layout_id_get(struct m0_clovis_op_obj *oo)
{
	uint64_t              lid;
	struct m0_clovis_obj *obj;

	M0_ENTRY();

	if (M0_FI_ENABLED("fake_obj_layout_id"))
		return 1;

	obj = M0_AMB(obj, oo->oo_oc.oc_op.op_entity, ob_entity);
	M0_ASSERT(clovis_obj_layout_id_invariant(obj->ob_attr.oa_layout_id));
	lid = M0_CLOVIS_OBJ_LAYOUT_ID(obj->ob_attr.oa_layout_id);

	M0_LEAVE();
	return lid;
}

M0_INTERNAL int
m0_clovis__obj_layout_instance_build(struct m0_clovis *cinst,
				     const uint64_t layout_id,
				     const struct m0_fid *fid,
				     struct m0_layout_instance **linst)
{
	int                     rc = 0;
	struct m0_layout       *layout;

	M0_PRE(cinst != NULL);
	M0_PRE(linst != NULL);
	M0_PRE(fid != NULL);

	/*
	 * All the layouts should already be generated on startup and added
	 * to the list unless wrong layout_id is used.
	 */
	layout = m0_layout_find(&cinst->m0c_reqh.rh_ldom, layout_id);
	if (layout == NULL) {
		rc = -EINVAL;
		goto out;
	}

	*linst = NULL;
	rc = m0_layout_instance_build(layout, fid, linst);
	m0_layout_put(layout);

out:
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

#ifdef CLOVIS_FOR_M0T1FS
/**
 * Generates a name for an object from its fid.
 *
 * @param name buffer where the name is stored.
 * @param name_len length of the name buffer.
 * @param fid fid of the object.
 * @return 0 if the name was correctly generated or -EINVAL otherwise.
 */
int clovis_obj_fid_make_name(char *name, size_t name_len,
			     const struct m0_fid *fid)
{
	int rc;

	M0_PRE(name != NULL);
	M0_PRE(name_len > 0);
	M0_PRE(fid != NULL);

	rc = snprintf(name, name_len, "%"PRIx64":%"PRIx64, FID_P(fid));
	M0_ASSERT(rc >= 0 && rc < name_len);
	return M0_RC(0);
}
#endif

/**
 * Initialises a m0_clovis_obj namespace operation. The operation is intended
 * to manage in the backend the object the provided entity is associated to.
 *
 * @param entity in-memory representation of the object's entity.
 * @param op operation being set. The operation must have been allocated as a
 * m0_clovis_op_obj.
 * @return 0 if the function completes successfully or an error code otherwise.
 */
static int clovis_obj_namei_op_init(struct m0_clovis_entity *entity,
				    struct m0_clovis_op *op)
{
	int                         rc;
	char                       *obj_name;
	uint64_t                    obj_key;
	uint64_t                    obj_container;
	uint64_t                    lid;
	struct m0_clovis_op_obj    *oo;
	struct m0_clovis_op_common *oc;
	struct m0_layout_instance  *linst;
	struct m0_clovis           *cinst;

	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_PRE(op != NULL);

	if (M0_FI_ENABLED("fake_msg_size")) {
		rc = M0_ERR(-EMSGSIZE);
		goto error;
	}

	if (op->op_size < sizeof *oo) {
		rc = M0_ERR(-EMSGSIZE);
		goto error;
	}

	oc = bob_of(op, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	M0_PRE(clovis_op_common_invariant(oc));
	oo = bob_of(oc, struct m0_clovis_op_obj, oo_oc, &oo_bobtype);
	M0_PRE(m0_clovis_op_obj_invariant(oo));

	cinst = m0_clovis__oo_instance(oo);
	M0_ASSERT(cinst != NULL);

	/* Set the op_common's callbacks. */
	oc->oc_cb_launch = clovis_obj_namei_cb_launch;
	oc->oc_cb_cancel = clovis_obj_namei_cb_cancel;
	oc->oc_cb_fini = clovis_obj_namei_cb_fini;
	oc->oc_cb_free = clovis_obj_namei_cb_free;

	/*
	 * Set the object's fid.
	 *
	 * Something about the fid at current mero implementation (fid_convert.h)
	 * (1) fid is 128 bits long,  global fid and cob fid both use the highest
	 * 8 bits to represent object type and the lowest 96 bits to store object
	 * id. The interpretion of these 96 bits depends on the users. For
	 * example, as the name of fid::f_container suggests, the 32 bits (or
	 * any number of bits) in f_container can be viewed as 'application
	 * container' id, so supporting multiple application containers is
	 * possible in current Mero implementation.
	 *
	 * (2) The difference of global fid and cob fid is in the 24 bits in
	 * fid::f_container. cob fid uses these 24 bits to store device id in a
	 * layout (md is 0, and devices in ioservices ranges from 1 to P).
	 *
	 * (3) Does Clovis need to check if an object's container id matches
	 * the container id inside its fid?
	 */
	obj_container = entity->en_id.u_hi;
	obj_key = entity->en_id.u_lo;
	m0_fid_gob_make(&oo->oo_fid, obj_container, obj_key);

	/* Get a layout instance for the object. */
	lid = m0_pool_version2layout_id(&oo->oo_pver,
				 	m0_clovis__obj_layout_id_get(oo));
	rc = m0_clovis__obj_layout_instance_build(cinst, lid,
						  &oo->oo_fid, &linst);
	if (rc != 0)
		goto error;
	oo->oo_layout_instance = linst;

#ifdef CLOVIS_FOR_M0T1FS
	/* Set the object's parent's fid. */
	if (!m0_fid_is_set(&cinst->m0c_root_fid) ||
	    !m0_fid_is_valid(&cinst->m0c_root_fid)) {
		rc = -EINVAL;
		goto error;
	}
	oo->oo_pfid = cinst->m0c_root_fid;

	/* Generate a valid oo_name. */
	obj_name = m0_alloc(M0_OBJ_NAME_MAX_LEN);
	rc = clovis_obj_fid_make_name(obj_name, M0_OBJ_NAME_MAX_LEN, &oo->oo_fid);
	if (rc != 0)
		goto error;
	m0_buf_init(&oo->oo_name, obj_name, strlen(obj_name));
#endif
	M0_ASSERT(rc == 0);
	return M0_RC(rc);

error:
	M0_ASSERT(rc != 0);
	return M0_ERR(rc);
}

/**
 * Initialises a m0_clovis_op_obj (i.e. an operation on an object).
 *
 * @param oo object operation to be initialised.
 * @return 0 if success or an error code otherwise.
 */
static int clovis_obj_op_obj_init(struct m0_clovis_op_obj *oo)
{
	int                     rc;
	struct m0_locality     *locality;
	struct m0_pool_version *pv;
	struct m0_clovis_obj   *obj;
	struct m0_clovis       *cinst;

	M0_ENTRY();
	M0_PRE(oo != NULL);
	M0_PRE(M0_IN(OP_OBJ2CODE(oo), (M0_CLOVIS_EO_CREATE,
				       M0_CLOVIS_EO_DELETE,
				       M0_CLOVIS_EO_OPEN)));

	/** Get the object's pool version. */
	obj = m0_clovis__obj_entity(oo->oo_oc.oc_op.op_entity);
	if (OP_OBJ2CODE(oo) == M0_CLOVIS_EO_CREATE) {
		rc = m0_clovis__obj_pool_version_get(obj, &pv);
		if (rc != 0)
			return M0_ERR(rc);
		oo->oo_pver = pv->pv_id;
	} else if (OP_OBJ2CODE(oo) == M0_CLOVIS_EO_OPEN) {
		/*
		 * XXX:Not required to assign pool version for operation other
		 *     than OBJECT CREATE.
		 * OBJECT OPEN operation will fetch pool version from meta-data
		 * and cache it to m0_clovis_obj::ob_layout::oa_pver
		 * MERO-2871 will fix and verify this issue separately.
		 */
			cinst = m0_clovis__obj_instance(obj);
			pv = m0_pool_version_md_get(&cinst->m0c_pools_common);
			M0_ASSERT(pv != NULL);
			oo->oo_pver = pv->pv_id;
	} else {
		oo->oo_pver = m0_clovis__obj_pver(obj);
	}
	/** TODO: hash the fid to chose a locality */
	locality = m0_clovis__locality_pick(m0_clovis__oo_instance(oo));
	M0_ASSERT(locality != NULL);
	oo->oo_sm_grp = locality->lo_grp;
	M0_SET0(&oo->oo_ar);

	oo->oo_layout_instance = NULL;
	M0_SET0(&oo->oo_fid);

#ifdef CLOVIS_FOR_M0T1FS
	M0_SET0(&oo->oo_pfid);
	M0_SET0(&oo->oo_name);
#endif

	m0_clovis_op_obj_bob_init(oo);
	m0_clovis_ast_rc_bob_init(&oo->oo_ar);

	M0_POST(m0_clovis_op_obj_invariant(oo));
	return M0_RC(0);
}

/**
 * Prepares a clovis operation to be executed on an object. Does all the
 * generic stuff common to every operation on objects. Also allocates the
 * operation if it has not been pre-allocated.
 *
 * @param entity Entity of the obj the operation is targeted to.
 * @param[out] op Operation to be prepared. The operation might have been
 * pre-allocated. Otherwise the function allocates extra memory.
 * @param opcode Specific operation code.
 * @return 0 if the operation completes successfully or an error code
 * otherwise.
 */
static int clovis_obj_op_prepare(struct m0_clovis_entity *entity,
				 struct m0_clovis_op **op,
				 enum m0_clovis_entity_opcode opcode)
{
	int                         rc;
	bool                        alloced = false;
	struct m0_clovis_op_common *oc;
	struct m0_clovis_op_obj    *oo;

	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_ASSERT(clovis_entity_invariant_full(entity));
	M0_PRE(M0_IN(entity->en_sm.sm_state, (M0_CLOVIS_ES_INIT,
					      M0_CLOVIS_ES_OPEN)));
	M0_PRE(op != NULL);
	/* We may want to gain this lock, check we don't already hold it. */
	M0_PRE(*op == NULL ||
	       !m0_sm_group_is_locked(&(*op)->op_sm_group));

	/* Allocate the op if necessary. */
	if (*op == NULL) {
		rc = m0_clovis_op_alloc(op, sizeof *oo);
		if (rc != 0)
			return M0_ERR(rc);
		alloced = true;
	} else {
		size_t cached_size = (*op)->op_size;

		if ((*op)->op_size < sizeof *oo)
			return M0_ERR(-EMSGSIZE);

		/* 0 the pre-allocated operation. */
		memset(*op, 0, cached_size);
		(*op)->op_size = cached_size;
	}
	m0_mutex_init(&(*op)->op_pending_tx_lock);
	spti_tlist_init(&(*op)->op_pending_tx);

	/* Initialise the operation's generic part. */
	(*op)->op_code = opcode;
	rc = m0_clovis_op_init(*op, &clovis_op_conf, entity);
	if (rc != 0)
		goto op_free;

	/* No bob_init()'s have been called yet: we use M0_AMB(). */
	oc = M0_AMB(oc, *op, oc_op);
	m0_clovis_op_common_bob_init(oc);

	/* Init the m0_clovis_op_obj part. */
	oo = M0_AMB(oo, oc, oo_oc);
	rc = clovis_obj_op_obj_init(oo);
	if (rc != 0)
		goto op_fini;

	return M0_RC(0);

op_fini:
	m0_clovis_op_fini(*op);
op_free:
	if (alloced) {
		m0_clovis_op_free(*op);
		*op = NULL;
	}

	return M0_RC(rc);
}

/**
 * Sets a entity operation to modify the object namespace.
 * This type of operation on entities allow creating and deleting entities.
 *
 * @param entity entity to be modified.
 * @param op pointer to the operation being set. The caller might pre-allocate
 * this operation.Otherwise, the function will allocate the required memory.
 * @param opcode M0_CLOVIS_EO_CREATE or M0_CLOVIS_EO_DELETE.
 * @return 0 if the function succeeds or an error code otherwise.
 */
static int clovis_entity_namei_op(struct m0_clovis_entity *entity,
				struct m0_clovis_op **op,
				enum m0_clovis_entity_opcode opcode)
{
	int  rc;
	bool pre_allocated = false;

	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_ASSERT(clovis_entity_invariant_full(entity));
	M0_PRE(M0_IN(entity->en_sm.sm_state, (M0_CLOVIS_ES_INIT,
					      M0_CLOVIS_ES_OPEN)));
	M0_PRE(op != NULL);
	M0_PRE(M0_IN(opcode, (M0_CLOVIS_EO_CREATE,
			      M0_CLOVIS_EO_DELETE,
			      M0_CLOVIS_EO_OPEN)));

	switch (entity->en_type) {
	case M0_CLOVIS_ET_OBJ:
		pre_allocated = (*op != NULL);
		/* Allocate an op on an object and initialise common stuff. */
		rc = clovis_obj_op_prepare(entity, op, opcode);
		if (rc != 0)
			goto error;

		/* Initialise the stuff specific to a obj namespace operation. */
		rc = clovis_obj_namei_op_init(entity, *op);
		if (rc != 0)
			goto op_fini;
		break;
	case M0_CLOVIS_ET_IDX:
		rc = m0_clovis_idx_op_namei(entity, op, opcode);
		break;
	default:
		M0_IMPOSSIBLE("Entity type not yet implemented.");
	}

	M0_POST(rc == 0);
	M0_POST(*op != NULL);
	m0_sm_group_lock(&(*op)->op_sm_group);
	M0_POST((*op)->op_sm.sm_rc == 0);
	m0_sm_group_unlock(&(*op)->op_sm_group);

	return M0_RC(0);
op_fini:
	m0_clovis_op_fini(*op);
	if (!pre_allocated) {
		m0_clovis_op_free(*op);
		*op = NULL;
	}
error:
	M0_ASSERT(rc != 0);
	return M0_ERR(rc);
}

int m0_clovis_entity_create(struct m0_fid *pool,
			    struct m0_clovis_entity *entity,
			    struct m0_clovis_op **op)
{
	struct m0_clovis_obj *obj;

	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_PRE(op != NULL);

	/* Currently, pool selection is only for objects. */
	if (entity->en_type == M0_CLOVIS_ET_OBJ && pool != NULL) {
		obj = M0_AMB(obj, entity, ob_entity);
		obj->ob_attr.oa_pool = *pool;
	}

	return M0_RC(clovis_entity_namei_op(entity, op, M0_CLOVIS_EO_CREATE));
}
M0_EXPORTED(m0_clovis_entity_create);

int m0_clovis_entity_delete(struct m0_clovis_entity *entity,
			    struct m0_clovis_op **op)
{
	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_PRE(op != NULL);

	return M0_RC(clovis_entity_namei_op(entity, op, M0_CLOVIS_EO_DELETE));
}
M0_EXPORTED(m0_clovis_entity_delete);

uint64_t m0_clovis_obj_unit_size_to_layout_id(int unit_size)
{
	uint64_t i;

	for (i = 0; i < m0_lid_to_unit_map_nr; i++)
		if (m0_lid_to_unit_map[i] == unit_size)
			break;

	if (i == m0_lid_to_unit_map_nr)
		return 0;
	else
		return i;
}
M0_EXPORTED(m0_clovis_obj_unit_size_to_layout_id);

int m0_clovis_obj_layout_id_to_unit_size(uint64_t layout_id)
{
	M0_ASSERT(layout_id > 0 && layout_id < m0_lid_to_unit_map_nr);

	return m0_lid_to_unit_map[layout_id];
}
M0_EXPORTED(m0_clovis_obj_layout_id_to_unit_size);

uint64_t m0_clovis_layout_id(const struct m0_clovis *instance)
{
	uint64_t lid = M0_DEFAULT_LAYOUT_ID;

	M0_ENTRY();
	M0_PRE(instance != NULL);

	/*
	 * TODO:This layout selection is a temporary solution for s3 team
	 * requirement. In future this has to be replaced by more sophisticated
	 * solution.
	 */
	if (instance->m0c_config->cc_layout_id != 0)
		lid = instance->m0c_config->cc_layout_id;

	M0_LEAVE("lid=%"PRIu64, lid);
	return lid;
}
M0_EXPORTED(m0_clovis_layout_id);

enum m0_clovis_layout_type m0_clovis_obj_layout_type(struct m0_clovis_obj *obj)
{
	return M0_CLOVIS_OBJ_LAYOUT_TYPE(obj->ob_attr.oa_layout_id);
}
M0_EXPORTED(m0_clovis_obj_layout_type);

int m0_clovis_entity_open(struct m0_clovis_entity *entity,
			  struct m0_clovis_op **op)
{
	M0_ENTRY();

	M0_PRE(entity != NULL);
	M0_PRE(op != NULL);

	if (entity->en_type == M0_CLOVIS_ET_IDX) {
		/* Since pool version for index entity is got by index query,
		 * move the entity state into OPEN and return success. */
		m0_sm_group_lock(&entity->en_sm_group);
		m0_sm_move(&entity->en_sm, 0, M0_CLOVIS_ES_OPENING);
		m0_sm_move(&entity->en_sm, 0, M0_CLOVIS_ES_OPEN);
		m0_sm_group_unlock(&entity->en_sm_group);
		return M0_RC(0);
	} else
		return M0_RC(clovis_entity_namei_op(entity, op,
						    M0_CLOVIS_EO_OPEN));
}
M0_EXPORTED(m0_clovis_entity_open);

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
