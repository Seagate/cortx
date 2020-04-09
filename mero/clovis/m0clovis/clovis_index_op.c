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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 28-Apr-2016
 */


/**
 * @addtogroup clovis
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/assert.h"        /* M0_ASSERT */
#include "lib/memory.h"        /* M0_ALLOC_ARR */
#include "lib/time.h"          /* M0_TIME_NEVER */
#include "lib/errno.h"
#include "lib/trace.h"         /* M0_ERR */
#include "clovis_index_op.h"
#include "clovis/clovis.h"
#include "clovis_index.h"
#include "cas/cas.h"           /* m0_dix_fid_type */
#include "fid/fid.h"           /* m0_fid_tassume */

static int per_item_rcs_analyse(int32_t *rcs, int cnt)
{
	int i;
	int rc = 0;

	for (i = 0; i < cnt; i++)
		if (rcs[i] != 0) {
			m0_console_printf("rcs[%d]: %d\n", i, rcs[i]);
			rc = rcs[i];
		}
	return M0_RC(rc);
}

static int index_op_tail(struct m0_clovis_entity *ce,
			 struct m0_clovis_op *op, int rc,
			 int *sm_rc)
{
	if (rc == 0) {
		m0_clovis_op_launch(&op, 1);
		rc = m0_clovis_op_wait(op,
				    M0_BITS(M0_CLOVIS_OS_FAILED,
					    M0_CLOVIS_OS_STABLE),
				    M0_TIME_NEVER);
		m0_console_printf("operation rc: %i\n", op->op_rc);
		if (sm_rc != NULL)
			/* Save retcodes. */
			*sm_rc = op->op_rc;
	} else
		m0_console_printf("operation rc: %i\n", rc);
	m0_clovis_op_fini(op);
	m0_clovis_op_free(op);
	m0_clovis_entity_fini(ce);
	return M0_RC(rc);
}

int clovis_index_create(struct m0_clovis_realm *parent, struct m0_fid_arr *fids)
{
	int i;
	int rc = 0;

	M0_PRE(fids != NULL && fids->af_count != 0);

	for(i = 0; rc == 0 && i < fids->af_count; ++i) {
		struct m0_clovis_op   *op  = NULL;
		struct m0_clovis_idx   idx;

		m0_fid_tassume(&fids->af_elems[i], &m0_dix_fid_type);
		m0_clovis_idx_init(&idx, parent,
				   (struct m0_uint128 *)&fids->af_elems[i]);
		rc = m0_clovis_entity_create(NULL, &idx.in_entity, &op);
		rc = index_op_tail(&idx.in_entity, op, rc, NULL);
	}
	return M0_RC(rc);
}

int clovis_index_drop(struct m0_clovis_realm *parent, struct m0_fid_arr *fids)
{
	int i;
	int rc = 0;

	M0_PRE(fids != NULL && fids->af_count != 0);

	for(i = 0; rc == 0 && i < fids->af_count; ++i) {
		struct m0_clovis_idx   idx;
		struct m0_clovis_op   *op = NULL;

		m0_fid_tassume(&fids->af_elems[i], &m0_dix_fid_type);
		m0_clovis_idx_init(&idx, parent,
				   (struct m0_uint128 *)&fids->af_elems[i]);
		rc = m0_clovis_entity_open(&idx.in_entity, &op) ?:
		     m0_clovis_entity_delete(&idx.in_entity, &op) ?:
		     index_op_tail(&idx.in_entity, op, rc, NULL);
	}
	return M0_RC(rc);
}

int clovis_index_list(struct m0_clovis_realm *parent,
		      struct m0_fid          *fid,
		      int                     cnt,
		      struct m0_bufvec       *keys)
{
	struct m0_clovis_idx  idx;
	struct m0_clovis_op  *op = NULL;
	int32_t              *rcs;
	int                   rc;

	M0_PRE(cnt != 0);
	M0_PRE(fid != NULL);
	M0_ALLOC_ARR(rcs, cnt);
	rc = m0_bufvec_alloc(keys, cnt, sizeof(struct m0_fid));
	if (rc != 0 || rcs == NULL) {
		m0_free(rcs);
		return M0_ERR(rc);
	}
	m0_fid_tassume(fid, &m0_dix_fid_type);
	m0_clovis_idx_init(&idx, parent, (struct m0_uint128 *)fid);
	rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_LIST, keys, NULL,
			      rcs, 0, &op);
	rc = index_op_tail(&idx.in_entity, op, rc, NULL);
	m0_free(rcs);
	return M0_RC(rc);
}

int clovis_index_lookup(struct m0_clovis_realm *parent,
		        struct m0_fid_arr      *fids,
		        struct m0_bufvec       *rets)
{
	int  i;
	int  rc = 0;

	M0_PRE(fids != NULL);
	M0_PRE(fids->af_count != 0);
	M0_PRE(rets != NULL);
	M0_PRE(rets->ov_vec.v_nr == 0);

	rc = m0_bufvec_alloc(rets, fids->af_count, sizeof(rc));
	/* Check that indices exist. */
	for(i = 0; rc == 0 && i < fids->af_count; ++i) {
		struct m0_clovis_idx  idx;
		struct m0_clovis_op  *op = NULL;

		m0_fid_tassume(&fids->af_elems[i], &m0_dix_fid_type);
		m0_clovis_idx_init(&idx, parent,
				   (struct m0_uint128 *)&fids->af_elems[i]);
		rc = m0_clovis_idx_op(&idx, M0_CLOVIS_IC_LOOKUP, NULL, NULL,
				      NULL, 0, &op);
		rc = index_op_tail(&idx.in_entity, op, rc,
				   (int *)rets->ov_buf[i]);
	}
	return M0_RC(rc);
}

static int index_op(struct m0_clovis_realm    *parent,
		    struct m0_fid             *fid,
		    enum m0_clovis_idx_opcode  opcode,
		    struct m0_bufvec          *keys,
		    struct m0_bufvec          *vals)
{
	struct m0_clovis_idx  idx;
	struct m0_clovis_op  *op = NULL;
	int32_t              *rcs;
	int                   rc;

	M0_ASSERT(keys != NULL);
	M0_ASSERT(keys->ov_vec.v_nr != 0);
	M0_ALLOC_ARR(rcs, keys->ov_vec.v_nr);
	if (rcs == NULL)
		return M0_ERR(-ENOMEM);

	m0_fid_tassume(fid, &m0_dix_fid_type);
	m0_clovis_idx_init(&idx, parent, (struct m0_uint128 *)fid);
	rc = m0_clovis_idx_op(&idx, opcode, keys, vals, rcs, 0, &op);
	rc = index_op_tail(&idx.in_entity, op, rc, NULL);
	/*
	 * Don't analyse per-item codes for NEXT, because usually user gets
	 * -ENOENT in 'rcs' since he requests more entries than exist.
	 */
	if (opcode != M0_CLOVIS_IC_NEXT)
	     rc = per_item_rcs_analyse(rcs, keys->ov_vec.v_nr);
	m0_free(rcs);
	return M0_RC(rc);
}

int clovis_index_put(struct m0_clovis_realm *parent,
		     struct m0_fid_arr      *fids,
		     struct m0_bufvec       *keys,
		     struct m0_bufvec       *vals)
{
	int rc = 0;
	int i;

	M0_PRE(fids != NULL && fids->af_count != 0);
	M0_PRE(keys != NULL);
	M0_PRE(vals != NULL);

	for (i = 0; i < fids->af_count && rc == 0; i++)
		rc = index_op(parent, &fids->af_elems[i],
			      M0_CLOVIS_IC_PUT, keys, vals);

	return M0_RC(rc);
}

int clovis_index_del(struct m0_clovis_realm *parent,
		     struct m0_fid_arr      *fids,
		     struct m0_bufvec       *keys)
{
	int rc = 0;
	int i;

	M0_PRE(fids != NULL && fids->af_count != 0);
	M0_PRE(keys != NULL);

	for (i = 0; i < fids->af_count && rc == 0; i++)
		rc = index_op(parent, &fids->af_elems[i],
			      M0_CLOVIS_IC_DEL, keys, NULL);

	return M0_RC(rc);
}

int clovis_index_get(struct m0_clovis_realm *parent,
		     struct m0_fid          *fid,
		     struct m0_bufvec       *keys,
		     struct m0_bufvec       *vals)
{
	int rc;
	int keys_nr;

	M0_PRE(fid != NULL);
	M0_PRE(keys != NULL);
	M0_PRE(vals != NULL && vals->ov_vec.v_nr == 0);

	/* Allocate vals entity without buffers. */
	keys_nr = keys->ov_vec.v_nr;
	rc = m0_bufvec_empty_alloc(vals, keys_nr) ?:
	     index_op(parent, fid, M0_CLOVIS_IC_GET, keys, vals) ?:
	     m0_exists(i, keys_nr, vals->ov_buf[i] == NULL) ?
				M0_ERR(-ENODATA) : 0;

	return M0_RC(rc);
}

int clovis_index_next(struct m0_clovis_realm *parent,
		      struct m0_fid          *fid,
		      struct m0_bufvec       *keys,
		      int                     cnt,
		      struct m0_bufvec       *vals)
{
	int   rc;
	void *startkey;
	int   startkey_size;

	M0_PRE(fid != NULL);
	M0_PRE(cnt != 0);
	M0_PRE(keys != NULL && keys->ov_vec.v_nr == 1);
	M0_PRE(vals != NULL && vals->ov_vec.v_nr == 0);

	/* Allocate array for VALs. */
	rc = m0_bufvec_empty_alloc(vals, cnt);
	/* Allocate array for KEYs, reuse first buffer. */
	if (rc == 0) {
		startkey = m0_alloc(keys->ov_vec.v_count[0]);
		if (startkey == NULL)
			goto fail;
		startkey_size = keys->ov_vec.v_count[0];
		memcpy(startkey, keys->ov_buf[0], keys->ov_vec.v_count[0]);
		m0_bufvec_free(keys);
		rc = m0_bufvec_empty_alloc(keys, cnt);
		if (rc != 0)
			goto fail;
		keys->ov_buf[0] = startkey;
		keys->ov_vec.v_count[0] = startkey_size;
	}
	if (rc == 0)
		rc = index_op(parent, fid, M0_CLOVIS_IC_NEXT, keys, vals);

	return M0_RC(rc);
fail:
	rc = M0_ERR(-ENOMEM);
	m0_bufvec_free(vals);
	m0_bufvec_free(keys);
	m0_free(startkey);
	return M0_ERR(rc);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
