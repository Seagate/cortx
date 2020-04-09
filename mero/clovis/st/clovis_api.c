/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 20-Nov-2014
 */

/*
 * Wrappers for each m0_clovis_xxx in which we keep track of
 * the resources used.
 */

#include "clovis/clovis.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"

void clovis_st_container_init(struct m0_clovis_container *con,
			      struct m0_clovis_realm     *parent,
			      const struct m0_uint128    *id,
			      struct m0_clovis           *instance)
{
	m0_clovis_container_init(con, parent, id, instance);
}

void clovis_st_obj_init(struct m0_clovis_obj *obj,
		        struct m0_clovis_realm  *parent,
		        const struct m0_uint128 *id, uint64_t layout_id)
{
	m0_clovis_obj_init(obj, parent, id, layout_id);
	clovis_st_mark_entity(&obj->ob_entity);
}

void clovis_st_obj_fini(struct m0_clovis_obj *obj)
{
	m0_clovis_obj_fini(obj);
	clovis_st_unmark_entity(&obj->ob_entity);
}

void clovis_st_idx_init(struct m0_clovis_idx *idx,
			struct m0_clovis_realm  *parent,
			const struct m0_uint128 *id)
{
	m0_clovis_idx_init(idx, parent, id);
	clovis_st_mark_entity(&idx->in_entity);
}

void clovis_st_idx_fini(struct m0_clovis_idx *idx)
{
	m0_clovis_idx_fini(idx);
	clovis_st_unmark_entity(&idx->in_entity);
}


int clovis_st_entity_create(struct m0_fid *pool,
			    struct m0_clovis_entity *entity,
			    struct m0_clovis_op **op)
{
	int rc;

	rc = m0_clovis_entity_create(NULL, entity, op);
	if (*op != NULL) clovis_st_mark_op(*op);

	return rc;
}

int clovis_st_entity_delete(struct m0_clovis_entity *entity,
			    struct m0_clovis_op **op)
{
	int rc;

	rc = m0_clovis_entity_delete(entity, op);
	if (*op != NULL) clovis_st_mark_op(*op);

	return rc;
}

void clovis_st_entity_fini(struct m0_clovis_entity *entity)
{
	m0_clovis_entity_fini(entity);
	clovis_st_unmark_entity(entity);
}

void clovis_st_obj_op(struct m0_clovis_obj       *obj,
		      enum m0_clovis_obj_opcode   opcode,
		      struct m0_indexvec         *ext,
		      struct m0_bufvec           *data,
		      struct m0_bufvec           *attr,
		      uint64_t                    mask,
		      struct m0_clovis_op       **op)
{
	m0_clovis_obj_op(obj, opcode, ext, data, attr, mask, op);
	if (*op != NULL) clovis_st_mark_op(*op);
}

int clovis_st_idx_op(struct m0_clovis_idx       *idx,
		     enum m0_clovis_idx_opcode   opcode,
		     struct m0_bufvec           *keys,
		     struct m0_bufvec           *vals,
		     int                        *rcs,
		     int                         flag,
		     struct m0_clovis_op       **op)
{
	int rc;

	rc = m0_clovis_idx_op(idx, opcode, keys, vals, rcs, flag, op);
	if (*op != NULL) clovis_st_mark_op(*op);

	return rc;
}

void clovis_st_op_launch(struct m0_clovis_op **op, uint32_t nr)
{
	/* nothing to record, call m0_xxx directly */
	m0_clovis_op_launch(op, nr);
	return;
}

int32_t clovis_st_op_wait(struct m0_clovis_op *op, uint64_t bits, m0_time_t to)
{
	/* nothing to record, call m0_xxx directly */
	return m0_clovis_op_wait(op, bits, to);
}

void clovis_st_op_fini(struct m0_clovis_op *op)
{
	m0_clovis_op_fini(op);
}

void clovis_st_op_free(struct m0_clovis_op *op)
{
	m0_clovis_op_free(op);
	clovis_st_unmark_op(op);
}

void clovis_st_entity_open(struct m0_clovis_entity *entity)
{
	struct m0_clovis_op *ops[1] = {NULL};
	int                  rc;

	m0_clovis_entity_open(entity, &ops[0]);
	if (ops[0] != NULL) clovis_st_mark_op(ops[0]);
	clovis_st_op_launch(ops, 1);
	rc = clovis_st_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       m0_time_from_now(5, 0));
	if ( rc == -ETIMEDOUT) {
		m0_clovis_op_cancel(ops, 1);

		m0_clovis_op_wait(ops[0],
				  M0_BITS(M0_CLOVIS_OS_FAILED,
				          M0_CLOVIS_OS_STABLE),
				  M0_TIME_NEVER);
	}
	clovis_st_op_fini(ops[0]);
	clovis_st_op_free(ops[0]);
}

void clovis_st_idx_open(struct m0_clovis_entity *entity)
{
	struct m0_clovis_op *ops[1] = {NULL};

	m0_clovis_entity_open(entity, &ops[0]);
}

int clovis_st_layout_op(struct m0_clovis_obj *obj,
			enum m0_clovis_entity_opcode opcode,
			struct m0_clovis_layout *layout,
			struct m0_clovis_op **op)
{
	int rc;

	rc = m0_clovis_layout_op(obj, opcode, layout, op);
	if (*op != NULL) clovis_st_mark_op(*op);

	return rc;
}
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
