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
 * Original creation date: 8-Aug-2016
 */


/**
 * @addtogroup dix
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"

#include "lib/arith.h"        /* min64 */
#include "lib/memory.h"       /* M0_ALLOC_ARR */
#include "pool/pool.h"        /* m0_pool_version */
#include "lib/errno.h"
#include "dix/client.h"
#include "dix/req.h"
#include "lib/finject.h"

#define NOENT (-ENOENT)
#define PROCESSING_IS_DONE (-ENOKEY)

static void sc_result_add(struct m0_dix_next_sort_ctx  *key_ctx,
			  uint32_t                      cidx,
			  struct m0_dix_next_resultset *rs,
			  uint32_t                      key_id,
			  struct m0_cas_next_reply     *rep);

static int sc_rep_cmp(const struct m0_cas_next_reply *a,
		      const struct m0_cas_next_reply *b)
{
	if (a == NULL && b == NULL)
		return 0;
	if (a == NULL)
		return -1;
	if (b == NULL)
		return 1;
	return memcmp(a->cnp_key.b_addr, b->cnp_key.b_addr,
		      min64(a->cnp_key.b_nob, b->cnp_key.b_nob)) ?:
		M0_3WAY(a->cnp_key.b_nob, b->cnp_key.b_nob);
}

static bool sc_rep_le(const struct m0_cas_next_reply *a,
		      const struct m0_cas_next_reply *b)
{
	return sc_rep_cmp(a, b) < 0;
}

static bool sc_rep_eq(const struct m0_cas_next_reply *a,
		      const struct m0_cas_next_reply *b)
{
	return sc_rep_cmp(a, b) == 0;
}

static void sc_next(struct m0_dix_next_sort_ctx *ctx)
{
	ctx->sc_pos++;
	if (ctx->sc_pos >= ctx->sc_reps_nr)
		ctx->sc_done = true;
}

static int sc_rep_get(struct m0_dix_next_sort_ctx  *ctx,
		      struct m0_cas_next_reply    **rep)
{
	*rep = NULL;
	if (ctx->sc_reps_nr != 0 &&
	    (ctx->sc_done || ctx->sc_pos >= ctx->sc_reps_nr))
		return PROCESSING_IS_DONE;
	if (ctx->sc_stop || ctx->sc_reps_nr == 0)
		return NOENT;
	*rep = &ctx->sc_reps[ctx->sc_pos];
	if ((*rep)->cnp_rc == NOENT) {
		ctx->sc_stop = true;
		return NOENT;
	}
	return 0;
}

/**
 * Sets sorting context current position to the first record retrieved for
 * 'key_idx' starting key.
 */
static int sc_key_pos_set(struct m0_dix_next_sort_ctx *ctx,
			  uint32_t                     key_idx,
			  const uint32_t              *recs_nr)
{
	uint32_t                  pos = 0;
	uint32_t                  start_pos = 0;
	uint32_t                  i;
	struct m0_cas_next_reply *rep;

	if (ctx->sc_reps_nr == 0) {
		ctx->sc_stop = true;
		return 0;
	}

	for (i = 0; i < key_idx && pos < ctx->sc_reps_nr; i++) {
		rep = &ctx->sc_reps[pos];
		start_pos += recs_nr[i];
		/** @todo: Why it is so? */
		if (rep->cnp_rc == NOENT) {
			pos++;
			continue;
		}
		for (; pos < start_pos && pos < ctx->sc_reps_nr; pos++);
	}
	ctx->sc_pos = pos;
	ctx->sc_stop = false;
	return ctx->sc_pos;
}

/**
 * Searches for the minimal value in all sort contexts.
 *
 * After minimal value is found, all sort contexts current positions are moved
 * to the first value that is bigger than found minimal value.
 *
 * Function out values:
 * m0_cas_next_reply *rep - minimal value for all sort contexts
 * m0_dix_next_sort_ctx *ret_ctx - sort context which contains "rep"
 * ret_idx - number of rep in cas_next_rep array
 *
 * Returns true if for current starting key there are no more records in all
 * sorting contexts.
 */
static bool sc_min_val_get(struct m0_dix_next_sort_ctx_arr  *ctxarr,
			   struct m0_cas_next_reply        **rep,
			   struct m0_dix_next_sort_ctx     **ret_ctx,
			   uint32_t                         *ret_idx)
{
	uint32_t                     ctx_id;
	uint32_t                     done_cnt  = 0;
	uint32_t                     nokey_cnt = 0;
	struct m0_dix_next_sort_ctx *ctx;
	struct m0_dix_next_sort_ctx *ctx_arr;
	struct m0_cas_next_reply    *min       = NULL;
	struct m0_cas_next_reply    *val;
	int                          rc;

	*ret_ctx = NULL;
	ctx_arr  = ctxarr->sca_ctx;
	/* Find minimal value in sort contexts. */
	for (ctx_id = 0; ctx_id < ctxarr->sca_nr; ctx_id++) {
		ctx = &ctx_arr[ctx_id];
		rc = sc_rep_get(ctx, &val);
		if (rc == NOENT) {
			nokey_cnt++;
			continue;
		}
		if (rc == PROCESSING_IS_DONE) {
			done_cnt++;
			continue;
		}
		if (sc_rep_eq(NULL, min) || sc_rep_le(val, min)) {
			min = val;
			*ret_ctx = ctx;
			*ret_idx = ctx->sc_pos;
		}
	}
	*rep = min;
	if (done_cnt == ctxarr->sca_nr || nokey_cnt == ctxarr->sca_nr)
		return true;

	/* Advance positions (if necessary) in all sort contexts. */
	for (ctx_id = 0; ctx_id < ctxarr->sca_nr; ctx_id++) {
		ctx = &ctx_arr[ctx_id];
		if (ctx->sc_stop)
			continue;
		sc_rep_get(ctx, &val);
		if (val == NULL || sc_rep_eq(val, min))
			sc_next(ctx);
	}
	return false;
}

static int dix_rs_vals_alloc(struct m0_dix_next_resultset *rs,
			     uint32_t key_idx, uint32_t nr)
{
	M0_ASSERT(rs != NULL);
	M0_ASSERT(rs->nrs_res != NULL);
	M0_ASSERT(key_idx < rs->nrs_res_nr);

	rs->nrs_res[key_idx].drs_nr = nr;
	M0_ALLOC_ARR(rs->nrs_res[key_idx].drs_reps,
		     rs->nrs_res[key_idx].drs_nr);
	if (rs->nrs_res[key_idx].drs_reps == NULL)
		return M0_ERR(-ENOMEM);
	return 0;
}

/**
 * Loads all CAS replies for NEXT request in sorting contexts.
 *
 * There is exactly one sorting context for one CAS reply. One CAS reply carries
 * retrieved records for all starting keys in a linear array from one component
 * catalogue.
 */
static int dix_data_load(struct m0_dix_req            *req,
			 struct m0_dix_next_resultset *rs)
{
	struct m0_dix_cas_rop       *cas_rop;
	struct m0_cas_req           *creq;
	struct m0_dix_rop_ctx       *rop = req->dr_rop;
	struct m0_dix_next_sort_ctx *ctx;
	uint32_t                     ctx_id = 0;
	struct m0_dix_next_sort_ctx *ctxs;
	uint32_t                     i;

	ctxs = rs->nrs_sctx_arr.sca_ctx;
	m0_tl_for(cas_rop, &rop->dg_cas_reqs, cas_rop) {
		ctx             = &ctxs[ctx_id++];
		creq            = &cas_rop->crp_creq;
		ctx->sc_creq    = creq;
		ctx->sc_reps_nr = m0_cas_req_nr(creq);
		if (ctx->sc_reps_nr == 0)
			continue;
		M0_ALLOC_ARR(ctx->sc_reps, ctx->sc_reps_nr);
		if (ctx->sc_reps == NULL) {
			/* Free already allocated reps. */
			for (i = 0; i < ctx_id; i++)
				m0_free(ctxs[i].sc_reps);
			return M0_ERR(-ENOMEM);
		}
		for (i = 0; i < ctx->sc_reps_nr; i++)
			m0_cas_next_rep(creq, i, &ctx->sc_reps[i]);
	} m0_tl_endfor;
	M0_ASSERT(ctx_id == rs->nrs_sctx_arr.sca_nr);
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_next_result_prepare(struct m0_dix_req *req)
{
	struct m0_cas_next_reply        *rep;
	struct m0_cas_next_reply        *last_rep = NULL;
	struct m0_dix_next_sort_ctx_arr *ctx_arr;
	struct m0_dix_next_sort_ctx     *ctxs;
	uint32_t                         i;
	uint32_t                         key_id;
	uint32_t                         ctx_id;
	const uint32_t                  *recs_nr;
	uint64_t                         start_keys_nr;
	struct m0_dix_next_resultset    *rs;
	uint32_t                         ctxs_nr;
	bool                             done = false;
	uint32_t                         rc = 0;

	recs_nr       = req->dr_recs_nr;
	start_keys_nr = req->dr_items_nr;
	rs            = &req->dr_rs;
	if (!M0_FI_ENABLED("mock_data_load")) {
		ctxs_nr = req->dr_rop->dg_cas_reqs_nr;
		rc = m0_dix_rs_init(rs, start_keys_nr, ctxs_nr);
	} else
		ctxs_nr = rs->nrs_sctx_arr.sca_nr;
	if (rc != 0)
		goto end;
	for (i = 0; rc == 0 && i < start_keys_nr; i++)
		rc = dix_rs_vals_alloc(rs, i, recs_nr[i]);
	if (rc != 0)
		goto end;
	ctx_arr = &rs->nrs_sctx_arr;
	/*
	 * Initialise all contexts and load all results from cas_rop into sort
	 * contexts.
	 */
	ctxs = ctx_arr->sca_ctx;
	if (!M0_FI_ENABLED("mock_data_load"))
		rc = dix_data_load(req, rs);
	if (rc != 0)
		goto end;
	/* Scan all results and merge-sort values into resultset. */
	for (key_id = 0; !done && rc == 0 && key_id < start_keys_nr; key_id++) {
		uint32_t                     cidx    = 0;
		struct m0_dix_next_sort_ctx *key_ctx = NULL;

		/* Setup key position for all contexts. */
		for (ctx_id = 0; ctx_id < ctxs_nr; ctx_id++)
			sc_key_pos_set(&ctxs[ctx_id], key_id, recs_nr);
		i = 0;
		while (rc == 0 && i < recs_nr[key_id]) {
			if ((done = sc_min_val_get(ctx_arr, &rep, &key_ctx,
						   &cidx)))
				break;
			if (rep != NULL &&
			   (i == 0 || !sc_rep_eq(last_rep, rep))) {
				sc_result_add(key_ctx, cidx, rs, key_id, rep);
				last_rep = rep;
				i++;
			}
		}
	}
	/* Free all creqs. We don't need any data from them. */
	if (!M0_FI_ENABLED("mock_data_load"))
		for (ctx_id = 0; ctx_id < ctxs_nr; ctx_id++)
			m0_cas_req_fini(ctxs[ctx_id].sc_creq);
	return rc;
end:
	if (!M0_FI_ENABLED("mock_data_load"))
		m0_dix_rs_fini(rs);
	return rc;
}

static int sc_init(struct m0_dix_next_sort_ctx_arr *ctx_arr, uint32_t nr)
{
	ctx_arr->sca_nr = nr;
	M0_ALLOC_ARR(ctx_arr->sca_ctx, ctx_arr->sca_nr);
	if (ctx_arr->sca_ctx == NULL)
		return M0_ERR(-ENOMEM);
	return 0;
}

static void sc_fini(struct m0_dix_next_sort_ctx_arr *ctx_arr)
{
	uint32_t i;

	for (i = 0; i < ctx_arr->sca_nr; i++)
		m0_free(ctx_arr->sca_ctx[i].sc_reps);
	m0_free(ctx_arr->sca_ctx);
}

M0_INTERNAL int m0_dix_rs_init(struct m0_dix_next_resultset *rs,
			       uint32_t                      start_keys_nr,
			       uint32_t                      sctx_nr)
{
	int rc;

	rs->nrs_res_nr = start_keys_nr;
	M0_ALLOC_ARR(rs->nrs_res, start_keys_nr);
	if (rs->nrs_res == NULL)
		return M0_ERR(-ENOMEM);
	rc = sc_init(&rs->nrs_sctx_arr, sctx_nr);
	return rc;
}

M0_INTERNAL void m0_dix_rs_fini(struct m0_dix_next_resultset *rs)
{
	struct m0_dix_next_results *res;
	int                         i;
	int                         j;

	M0_ASSERT(rs != NULL);
	if (rs->nrs_res != NULL) {
		for (i = 0; i < rs->nrs_res_nr; i++) {
			/*
			 * Free result keys and values, cause all reps are
			 * mlocked in sc_result_add(). All other keys and values
			 * have been destroyed by m0_cas_req_fini() at the end
			 * of m0_dix_next_result_prepare().
			 */
			res = &rs->nrs_res[i];
			if (!M0_FI_ENABLED("mock_data_load"))
				for (j = 0; j < rs->nrs_res[i].drs_pos; j++) {
					m0_buf_free(&res->drs_reps[j]->cnp_key);
					m0_buf_free(&res->drs_reps[j]->cnp_val);
				}
			m0_free(res->drs_reps);
		}
		m0_free(rs->nrs_res);
	}
	sc_fini(&rs->nrs_sctx_arr);
}

/*
 * Key_ctx and cidx (cas key index) are required to hold record in memory.
 * Key/value pair is deallocated in m0_dix_req_fini()->m0_dix_rs_fini().
 */
static void sc_result_add(struct m0_dix_next_sort_ctx  *key_ctx,
			  uint32_t                      cidx,
			  struct m0_dix_next_resultset *rs,
			  uint32_t                      key_id,
			  struct m0_cas_next_reply     *rep)
{
	struct m0_cas_next_reply   **reps;
	struct m0_dix_next_results  *res;

	M0_ASSERT(key_ctx != NULL);
	M0_ASSERT(rs->nrs_res[key_id].drs_pos < rs->nrs_res[key_id].drs_nr);
	M0_ASSERT(key_id < rs->nrs_res_nr);
	/*
	 * Value will be freed at m0_dix_req_fini().
	 * Pointers to keys and vals are stored in next_resultset.
	 * Position sc_pos (returned from sc_min_val_get()) is equal to position
	 * in cas_rep array, that's why we can use cidx for mlock.
	 */
	if (!M0_FI_ENABLED("mock_data_load"))
		m0_cas_rep_mlock(key_ctx->sc_creq, cidx);
	res  = &rs->nrs_res[key_id];
	reps = res->drs_reps;
	reps[res->drs_pos++] = rep;
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
