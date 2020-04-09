/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/finject.h" /* M0_FI_ENABLED */

#include "fdmi/fdmi.h"
#include "fdmi/source_dock.h"
#include "fdmi/fol_fdmi_src.h"
#include "fdmi/filter.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/module.h"
#include "fop/fop.h"     /* m0_fop_fol_frag */

/**
 * @addtogroup fdmi_fol_src
 *
 * <b>Implementation notes.</b>
 *
 * FDMI needs in-memory representaion of a FOL record to operate on.  So, FOL
 * source will increase backend transaction ref counter (fom->fo_tx.be_tx,
 * using m0_be_tx_get()) to make sure it is not destroyed, and pass
 * m0_fom::fo_tx as a handle to FDMI.  The refcounter will be decremented back
 * once FDMI has completed its processing and all plugins confirm they are
 * done with record.
 *
 * FDMI refc inc/dec will be kept as a separate counter inside m0_be_tx.  This
 * will help prevent/debug cases when FDMI decref calls count does not match
 * incref calls count.  At first, we used transaction lock to protect this
 * counter modification, but it caused deadlocks.  So we switched to using
 * m0_atomic64 instead.  This is OK, since inc/dec operations are never mixed,
 * they are always "in line": N inc operations, followed by N dec operations,
 * so there is no chance of race condition when it decreased to zero, and we
 * initiated tx release operation, and then "someone" decides to increase the
 * counter again.
 *
 * This is implementation of Phase1, which does not need transaction support
 * (that is, we don't need to re-send FDMI records, which would normally
 * happen in case when plugin for example crashes and re-requests FDMI records
 * starting from X in the past).  This assumption results in the following
 * implementation appoach.
 *
 * - We will keep transaction "open" until FDMI reports it has completed
 *   filter processing (m0_fdmi_src::fs_end()).  This
 *   simplifies a lot of stuff -- entire FOL record is available in memory for
 *   our manipulations (get_value and encode calls).
 * - We will NOT count incref/decref calls, and will completely release the
 *   record after m0_fdmi_src::fs_end() call.  (We will
 *   still implement the counter -- to put in a work-around for future
 *   expansion.)
 *
 * @{
 */

/* ------------------------------------------------------------------
 * Fragments handling
 * ------------------------------------------------------------------ */

#define M0_FOL_FRAG_DATA_HANDLER_DECLARE(_opecode, _get_val_func) { \
	.ffh_opecode          = (_opecode),                        \
	.ffh_fol_frag_get_val = (_get_val_func) }

static struct ffs_fol_frag_handler ffs_frag_handler_array[] = {
	M0_FOL_FRAG_DATA_HANDLER_DECLARE(0, NULL)
};

/* ------------------------------------------------------------------
 * List of locked transactions
 * ------------------------------------------------------------------ */

M0_TL_DESCR_DEFINE(ffs_tx, "fdmi fol src tx list", M0_INTERNAL,
		   struct m0_be_tx, t_fdmi_linkage, t_magic,
		   M0_BE_TX_MAGIC, M0_BE_TX_ENGINE_MAGIC);

M0_TL_DEFINE(ffs_tx, M0_INTERNAL, struct m0_be_tx);

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

static struct m0_dtx* ffs_get_dtx(struct m0_fdmi_src_rec *src_rec)
{
	/* This is just wrapper, so no point using ENTRY/LEAVE */

	struct m0_fol_rec *fol_rec;

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
	fol_rec = container_of(src_rec, struct m0_fol_rec, fr_fdmi_rec);
	return container_of(fol_rec, struct m0_dtx, tx_fol_rec);
}

static void be_tx_put_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_be_tx  *be_tx = ast->sa_datum;

	M0_ENTRY("sm_group %p, ast %p (be_tx = %p)", grp, ast, be_tx);
	M0_LOG(M0_DEBUG, "call be_tx_put direct (2)");
	m0_be_tx_put(be_tx);

	M0_LEAVE();
}

static void ffs_tx_inc_refc(struct m0_be_tx *be_tx, int64_t *counter)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	int64_t                cnt;

	M0_ENTRY("be_tx %p", be_tx);

	M0_ASSERT(be_tx != NULL);

	if (m0_atomic64_get(&be_tx->t_fdmi_ref) == 0) {
		/**
		 * Value = 0 means this call happened during record
		 * posting. Execution context is well-defined, all
		 * locks already acquired, no need to use AST.
		 */
		M0_LOG(M0_INFO, "first incref for a be_tx_get %p", be_tx);
		m0_be_tx_get(be_tx);
		m0_mutex_lock(&m->fdm_s.fdms_ffs_locked_tx_lock);
		ffs_tx_tlink_init_at_tail(be_tx,
			&m->fdm_s.fdms_ffs_locked_tx_list);
		m0_mutex_unlock(&m->fdm_s.fdms_ffs_locked_tx_lock);
	}

	cnt = m0_atomic64_add_return(&be_tx->t_fdmi_ref, 1);
	M0_ASSERT(cnt > 0);

	if (counter != NULL)
		*counter = cnt;
	M0_LEAVE("counter = %"PRIi64, cnt);
}

static void ffs_tx_dec_refc(struct m0_be_tx *be_tx, int64_t *counter)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	int64_t                cnt;

	M0_ENTRY("be_tx %p, counter ptr %p", be_tx, counter);

	M0_ASSERT(be_tx != NULL);

	cnt = m0_atomic64_sub_return(&be_tx->t_fdmi_ref, 1);
	M0_ASSERT(cnt >= 0);

	if (counter != NULL)
		*counter = cnt;

	if (cnt == 0) {
		m0_mutex_lock(&m->fdm_s.fdms_ffs_locked_tx_lock);
		ffs_tx_tlink_del_fini(be_tx);
		m0_mutex_unlock(&m->fdm_s.fdms_ffs_locked_tx_lock);
		M0_LOG(M0_DEBUG, "call be_tx_put CB");
		be_tx->t_fdmi_put_ast.sa_cb    = be_tx_put_ast_cb;
		be_tx->t_fdmi_put_ast.sa_datum = be_tx;
		m0_sm_ast_post(be_tx->t_sm.sm_grp, &be_tx->t_fdmi_put_ast);
		M0_LOG(M0_DEBUG, "last decref for a be_tx %p "
		       "(ast callback posted)", be_tx);
	}
	M0_LEAVE("counter = %"PRIi64, cnt);
}

#if 0
/* Will only be used in Phase2, when we introduce proper handling of
 * transactions. */
static void ffs_rec_get(struct m0_uint128 *fdmi_rec_id)
{
	M0_ENTRY("fdmi_rec_id: " U128X_F, U128_P(fdmi_rec_id));

	...

	ffs_tx_inc_refc(&entry->fsim_tx->tx_betx, NULL);

	M0_LEAVE();
}
#endif

static void ffs_rec_put(struct m0_fdmi_src_rec	*src_rec,
			int64_t           	*counter)
{
	struct m0_dtx *dtx;
	int64_t cnt;

	M0_ENTRY("src_rec %p, counter %p", src_rec, counter);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	dtx = ffs_get_dtx(src_rec);
	M0_ASSERT(dtx != NULL);

	ffs_tx_dec_refc(&dtx->tx_betx, &cnt);
	if (counter != NULL)
		*counter = cnt;

	M0_LEAVE("counter = %"PRIi64, cnt);
}

/* ------------------------------------------------------------------
 * FOL source interface implementation
 * ------------------------------------------------------------------ */

static int ffs_op_node_eval(struct m0_fdmi_src_rec	*src_rec,
			    struct m0_fdmi_flt_var_node *value_desc,
			    struct m0_fdmi_flt_operand  *value)
{
	struct m0_dtx          *dtx;
	struct m0_fol_rec      *fol_rec;
	uint64_t                opcode;
	struct m0_fol_frag     *rfrag;
	struct m0_fop_fol_frag *rp;
	int                     rc;

	M0_ENTRY("src_rec %p, value desc %p, value %p",
		 src_rec, value_desc, value);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
	M0_ASSERT(value_desc != NULL && value != NULL);

	dtx = ffs_get_dtx(src_rec);
	M0_ASSERT(dtx != NULL);

	fol_rec = &dtx->tx_fol_rec;

	/** @todo Phase 2: STUB: For now, we will not analyze filter, we just
	 * return FOL op code -- always. */

	rfrag = m0_rec_frag_tlist_head(&fol_rec->fr_frags);
	M0_ASSERT(rfrag != NULL);

	/**
	 * TODO: Q: (question to FOP/FOL owners) I could not find a better way
	 * to assert that this frag is of m0_fop_fol_frag_type, than to use this
	 * hack (referencing internal _ops structure). Looks like they are
	 * ALWAYS of this type?...  Now that there is NO indication of frag
	 * type whatsoever?... */
	M0_ASSERT(rfrag->rp_ops->rpo_type == &m0_fop_fol_frag_type);

	rp = rfrag->rp_data;
	M0_ASSERT(rp != NULL);
	opcode = rp->ffrp_fop_code;
	m0_fdmi_flt_uint_opnd_fill(value, opcode);
	rc = 0;

	return M0_RC(rc);
}

static void ffs_op_get(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec %p", src_rec);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

#if 0
	/* Proper transactional handling is for phase 2. */
	ffs_rec_get(fdmi_rec_id);
#endif

	M0_LEAVE();
}

static void ffs_op_put(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec %p", src_rec);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

#if 0
	/* Proper transactional handling is for phase 2. */
	ffs_rec_put(fdmi_rec_id, NULL, NULL);
#endif

	M0_LEAVE();
}

static int ffs_op_encode(struct m0_fdmi_src_rec *src_rec,
			 struct m0_buf          *buf)
{
	struct m0_dtx      *dtx;
	struct m0_fol_rec  *fol_rec;
	struct m0_buf       local_buf = {};
	int                 rc;

	M0_ASSERT(buf != NULL);
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
	M0_ASSERT(buf->b_addr == NULL && buf->b_nob == 0);

	M0_ENTRY("src_rec %p, cur " BUF_F, src_rec, BUF_P(buf));

	dtx = ffs_get_dtx(src_rec);
	M0_ASSERT(dtx != NULL);

	fol_rec = &dtx->tx_fol_rec;

	/**
	 * @todo Q: (for FOL owners) FOL record does not provide API call to
	 * calculate record size when encoded.  For now, I'll do double
	 * allocation.  Alloc internal buf of max size, then encode, then
	 * alloc with correct size, then copy, then dealloc inernal buf.  Can
	 * be done properly once FOL record owner exports needed api call.
	 */
	rc = m0_buf_alloc(&local_buf, FOL_REC_MAXSIZE);
	if (rc != 0) {
		return M0_ERR_INFO(rc, "Failed to allocate internal buffer "
				   "for encoded FOL FDMI record.");
	}

	rc = m0_fol_rec_encode(fol_rec, &local_buf);
	if (rc != 0) {
		M0_LOG(M0_ERROR,
		       "Failed to encoded FOL FDMI record.");
		goto done;
	}

	rc = m0_buf_alloc(buf, fol_rec->fr_header.rh_data_len);
	if (rc != 0) {
		M0_LOG(M0_ERROR,
		       "Failed to allocate encoded FOL FDMI record.");
		goto done;
	}
	memcpy(buf->b_addr, local_buf.b_addr, buf->b_nob);

	if (M0_FI_ENABLED("fail_in_final"))
		rc = -EINVAL;

done:
	/* Finalization */
	if (local_buf.b_addr != NULL)
		m0_buf_free(&local_buf);
	/* On-Error cleanup. */
	if (rc < 0) {
		if (buf->b_addr != NULL)
			m0_buf_free(buf);
	}

	return M0_RC(rc);
}

static int ffs_op_decode(struct m0_buf *buf, void **handle)
{
	struct m0_fol_rec *fol_rec = 0;
	int                rc = 0;

	M0_ASSERT(buf != NULL && buf->b_addr != NULL && handle != NULL);

	M0_ENTRY("buf " BUF_F ", handle %p", BUF_P(buf), handle);

	M0_ALLOC_PTR(fol_rec);
	if (fol_rec == NULL) {
		M0_LOG(M0_ERROR, "failed to allocate m0_fol_rec object");
		rc = -ENOMEM;
		goto done;
	}
	m0_fol_rec_init(fol_rec, NULL);

	rc = m0_fol_rec_decode(fol_rec, buf);
	if (rc < 0)
		goto done;

	*handle = fol_rec;

	if (M0_FI_ENABLED("fail_in_final"))
		rc = -EINVAL;

done:
	if (rc < 0) {
		if (fol_rec != NULL) {
			m0_fol_rec_fini(fol_rec);
			m0_free0(&fol_rec);
		}
		*handle = NULL;
	}

	return M0_RC(rc);
}

static void ffs_op_begin(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec %p", src_rec);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	/**
	 * No need to do anything on this event for FOL Source.  Call to
	 * ffs_tx_inc_refc done in m0_fol_fdmi_post_record below will make sure
	 * the data is already in memory and available for fast access at the
	 * moment of this call.
	 */

	(void)src_rec;

	M0_LEAVE();
}

static void ffs_op_end(struct m0_fdmi_src_rec *src_rec)
{
	int64_t counter;

	M0_ENTRY("src_rec %p", src_rec);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	/* Note: in Phase 2, we'll probably need to handle two different cases
	 * differently.  Case#1 is get/put backend transaction, to make sure
	 * it's held in RAM up to the moment FDMI runs all filters.  Case#2 is
	 * some kind of counter which protects FOL entry from destruction
	 * before we get all 'decref' callbacks from FDMI source dock.  Right
	 * now, I ignore incref/decref altogether, since Phase 1 does not
	 * support transactions and FDMI records re-sending.  And I do this
	 * decref here to release transaction.  In Phase 2, this may need
	 * rework. */
	counter = 1;
	ffs_rec_put(src_rec, &counter); /* This call is a pair to the
					 * first call, inc_refc, done in
					 * post_record. */
	M0_ASSERT(counter == 0); /* Valid until FDMI phase 2; in phase 2 we
				  * add transaction handling, and
				  * incref/decref will start working, and
				  * counter will be non-zero here if any
				  * filters matched.  See ffs_op_get. */

	M0_LEAVE();
}

/* ------------------------------------------------------------------
 * Init/fini
 * ------------------------------------------------------------------ */

M0_INTERNAL int m0_fol_fdmi_src_init(void)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	int                    rc;

	M0_ENTRY();

	M0_ASSERT(m->fdm_s.fdms_ffs_ctx.ffsc_src == NULL);
	m->fdm_s.fdms_ffs_ctx.ffsc_magic = M0_FOL_FDMI_SRC_CTX_MAGIC;

	rc = m0_fdmi_source_alloc(M0_FDMI_REC_TYPE_FOL,
		&m->fdm_s.fdms_ffs_ctx.ffsc_src);
	if (m->fdm_s.fdms_ffs_ctx.ffsc_src == NULL)
		return M0_RC(-ENOMEM);

	ffs_tx_tlist_init(&m->fdm_s.fdms_ffs_locked_tx_list);
	m0_mutex_init(&m->fdm_s.fdms_ffs_locked_tx_lock);

	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_node_eval  = ffs_op_node_eval;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_get        = ffs_op_get;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_put        = ffs_op_put;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_begin      = ffs_op_begin;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_end        = ffs_op_end;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_encode     = ffs_op_encode;
	m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_decode     = ffs_op_decode;

	rc = m0_fdmi_source_register(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed to register FDMI FOL source.");
		goto error_free_src;
	}

	m->fdm_s.fdms_ffs_ctx.ffsc_frag_handler_vector = ffs_frag_handler_array;
	m->fdm_s.fdms_ffs_ctx.ffsc_handler_number      =
		ARRAY_SIZE(ffs_frag_handler_array);
	return M0_RC(rc);
error_free_src:
	m0_fdmi_source_deregister(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m0_fdmi_source_free(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m->fdm_s.fdms_ffs_ctx.ffsc_src = NULL;
	return M0_RC(rc);
}

M0_INTERNAL void m0_fol_fdmi_src_fini(void)
{
	M0_ENTRY();
	m0_fol_fdmi_src_deinit();
	M0_LEAVE();
}

M0_INTERNAL int m0_fol_fdmi_src_deinit(void)
{
	struct m0_fdmi_module   *m = m0_fdmi_module__get();
	struct m0_fdmi_src_ctx  *src_ctx;
	struct m0_be_tx		*be_tx;
	int rc = 0;

	M0_ENTRY();

	M0_PRE(m->fdm_s.fdms_ffs_ctx.ffsc_src != NULL);
	src_ctx = container_of(m->fdm_s.fdms_ffs_ctx.ffsc_src,
			       struct m0_fdmi_src_ctx, fsc_src);

	M0_PRE(src_ctx->fsc_registered);
	M0_PRE(m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_record_post != NULL);

	/**
	 * The deregister below does not call for fs_put/fs_end, so
	 * we'll have to do call m0_be_tx_put explicitly here, over
	 * all transactions we've locked.
	 */
	m0_mutex_lock(&m->fdm_s.fdms_ffs_locked_tx_lock);
	m0_tlist_for(&ffs_tx_tl, &m->fdm_s.fdms_ffs_locked_tx_list, be_tx) {
		ffs_tx_tlink_del_fini(be_tx);
		m0_be_tx_put(be_tx);
		/**
		 * Note we don't reset t_fdmi_ref here, it's a flag
		 * the record is not yet released by plugins.
		 */
	} m0_tlist_endfor;
	m0_mutex_unlock(&m->fdm_s.fdms_ffs_locked_tx_lock);
	m0_mutex_fini(&m->fdm_s.fdms_ffs_locked_tx_lock);

	ffs_tx_tlist_fini(&m->fdm_s.fdms_ffs_locked_tx_list);

	m0_fdmi_source_deregister(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m0_fdmi_source_free(m->fdm_s.fdms_ffs_ctx.ffsc_src);
	m->fdm_s.fdms_ffs_ctx.ffsc_src = NULL;
	return M0_RC(rc);
}

/* ------------------------------------------------------------------
 * Entry point for FOM to start FDMI processing
 * ------------------------------------------------------------------ */

M0_INTERNAL int m0_fol_fdmi_post_record(struct m0_fom *fom)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	struct m0_dtx         *dtx;
	struct m0_be_tx       *be_tx;
	int                    rc;

	M0_ENTRY("fom: %p", fom);

	M0_ASSERT(fom != NULL);
	M0_ASSERT(m->fdm_s.fdms_ffs_ctx.ffsc_src->fs_record_post != NULL);

	/**
	 * There is no "unpost record" method, so we have to prepare
	 * everything that may fail -- before calling to post method.
	 */

	dtx   = &fom->fo_tx;
	be_tx = &fom->fo_tx.tx_betx;

	/** @todo Phase 2: Move inc ref call to FDMI source dock */

	ffs_tx_inc_refc(be_tx, NULL);

	/* Post record. */
	dtx->tx_fol_rec.fr_fdmi_rec.fsr_src  = m->fdm_s.fdms_ffs_ctx.ffsc_src;
	dtx->tx_fol_rec.fr_fdmi_rec.fsr_dryrun = false;
	dtx->tx_fol_rec.fr_fdmi_rec.fsr_data = NULL;

	rc = M0_FDMI_SOURCE_POST_RECORD(&dtx->tx_fol_rec.fr_fdmi_rec);
	if (rc < 0) {
		M0_LOG(M0_ERROR, "Failed to post FDMI record.");
		goto error_post_record;
	} else {
		M0_ENTRY("Posted FDMI rec, src_rec %p, rec id " U128X_F,
			 &dtx->tx_fol_rec.fr_fdmi_rec,
			 U128_P(&dtx->tx_fol_rec.fr_fdmi_rec.fsr_rec_id));
	}

	/* Aftermath. */

	/**
	 * NOTE: IMPORTANT! Do not call anything that may fail here! It is
	 * not possible to un-post the record; anything that may fail, must be
	 * done before the M0_FDMI_SOURCE_POST_RECORD call above.
	 */

	return M0_RC(rc);
error_post_record:
	ffs_tx_dec_refc(be_tx, NULL);
	return M0_RC(rc);
}

/**
 * @} addtogroup fdmi_fol_src
 */

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
