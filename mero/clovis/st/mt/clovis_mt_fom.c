/* -*- C -*- */
/*
 * COPYRIGHT 2018 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Anatoliy Bilenko <Anatoliy.Bilenko@seagate.com>
 * Original creation date: 01-09-2018
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#include "clovis/clovis.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"
#include "lib/trace.h"

#include "fid/fid.h"
#include "fop/fom_simple.h" /* m0_fom_simple */
#include "lib/misc.h"       /* M0_SRC_PATH */

static int clovis_st_mt_init(void)
{
	return 0;
}

static int clovis_st_mt_fini(void)
{
	return 0;
}

enum {
	/* CMT_PUTS        = 256, Reduces UT run time */
	CMT_PUTS        = 64,
	CMT_GETS        = CMT_PUTS,
	CMT_DELS        = CMT_PUTS,
	CMT_IDXS        = 64,
	CMT_IDXD        = CMT_IDXS,

	/* CMT_BATCH       = 32, Reduces UT run time */
	CMT_BATCH       = 16,
	CMT_BATCH_OFF   = 1024,
	CMT_FOM_PER_LOC = 2,
	CMT_IDXS_NR     = CMT_IDXS,

	/* CMT_KV_SZ       = 128, Reduces UT run time */
	CMT_KV_SZ       = 10,
};
M0_BASSERT(CMT_PUTS >= CMT_IDXS);

enum {
	REQ_CREATE,
	REQ_DELETE,
	REQ_LOOKUP,
	REQ_NEXT,
	REQ_GET,
	REQ_PUT,
	REQ_DEL
};

struct clovis_st_fom_ctx {
	struct m0_fom_simple      sfc_fom_s;
	struct m0_semaphore       sfc_done;
	int                       sfc_i;
	bool                      sfc_fired;
	struct m0_clovis_op      *sfc_op;
	struct m0_clovis         *sfc_cli;
	struct clovis_st_mt_ctxt *sfc_ctx;

	struct m0_bufvec          sfc_keys;
	struct m0_bufvec          sfc_vals;
	int                       sfc_rcs[CMT_IDXS_NR];
	int64_t                   sfc_ci;
};

struct clovis_st_mt_ctxt {
	struct m0_clovis_container  csc_realm;
	struct m0_clovis           *csc_cli;
	struct m0_clovis_idx        csc_index[CMT_IDXS_NR];
	struct m0_fid               csc_ifid[CMT_IDXS_NR];

	struct m0_reqh             *csc_reqh;
	struct clovis_st_fom_ctx   *csc_fctx;
	uint64_t                    csc_foms_nr;
	uint64_t                    csc_loc_nr;
	struct m0_atomic64          csc_counter;
};

/* XXX copy-paste from lib/ut/locality.c */
static void fom_simple_svc_start(struct m0_reqh *reqh)
{
	struct m0_reqh_service_type *stype;
	struct m0_reqh_service      *service;
	int                          rc;

	stype = m0_reqh_service_type_find("simple-fom-service");
	M0_ASSERT(stype != NULL);
	rc = m0_reqh_service_allocate(&service, stype, NULL);
	M0_ASSERT(rc == 0);
	m0_reqh_service_init(service, reqh,
			     &M0_FID_INIT(0xdeadb00f, 0xb00fdead));
	rc = m0_reqh_service_start(service);
	M0_ASSERT(rc == 0);
	M0_POST(m0_reqh_service_invariant(service));
}

static void clovis_st_op_lock(struct m0_clovis_op *op)
{
	m0_sm_group_lock(op->op_sm.sm_grp);
}

static void clovis_st_op_unlock(struct m0_clovis_op *op)
{
	m0_sm_group_unlock(op->op_sm.sm_grp);
}

static int clovis_st_op_tick_ret(struct m0_clovis_op *op,
				 struct m0_fom       *fom)
{
	enum m0_fom_phase_outcome ret = M0_FSO_AGAIN;

	clovis_st_op_lock(op);

	if (op->op_sm.sm_state < M0_CLOVIS_OS_STABLE) {
		ret = M0_FSO_WAIT;
		m0_fom_wait_on(fom, &op->op_sm.sm_chan, &fom->fo_cb);
	}

	clovis_st_op_unlock(op);

	return ret;
}

static void clovis_st_dix_kv_destroy(struct m0_bufvec *keys,
				     struct m0_bufvec *vals)
{
	m0_bufvec_free(keys);
	m0_bufvec_free(vals);
}

static void clovis_st_kv_alloc_and_fill(struct m0_bufvec *keys,
					struct m0_bufvec *vals,
					uint32_t          first,
					uint32_t          last,
					bool              emptyvals)
{
	uint32_t i;
	uint32_t count = last - first + 1;
	int      rc;

	rc = m0_bufvec_alloc(keys, count, CMT_KV_SZ);
	M0_ASSERT(rc == 0);
	rc = emptyvals ?
		m0_bufvec_empty_alloc(vals, count) :
		m0_bufvec_alloc(vals, count, CMT_KV_SZ);
	M0_ASSERT(rc == 0);
	for (i = 0; i < count; i++) {
		sprintf(keys->ov_buf[i], "%0*"PRIu32, CMT_KV_SZ - 1, first + i);
		if (!emptyvals)
			sprintf(vals->ov_buf[i], "%0*"PRIu32, CMT_KV_SZ - 1,
				first + i);
	}
}

static void clovis_st_vals_check(struct m0_bufvec   *keys,
				 struct m0_bufvec   *vals,
				 uint32_t            first,
				 uint32_t            last)
{
	uint32_t i;
	uint32_t count = last - first + 1;
	char *key;
	char *val;

	M0_ASSERT(vals->ov_vec.v_nr == keys->ov_vec.v_nr);
	M0_ASSERT(keys->ov_vec.v_nr == count);
	for (i = 0; i < count; i++) {
		char buf[CMT_KV_SZ];
		key = (char *)keys->ov_buf[i];
		val = (char *)vals->ov_buf[i];
		sprintf(buf, "%0*"PRIu32, CMT_KV_SZ - 1, first + i);
		M0_ASSERT(memcmp(key, buf, CMT_KV_SZ) == 0);
		M0_ASSERT(memcmp(val, buf, CMT_KV_SZ) == 0);
	}
}

static void clovis_st_ifid_fill(struct m0_fid *ifid, int i)
{
	*ifid = M0_FID_TINIT(m0_dix_fid_type.ft_id, 2, i);
}

static int clovis_st_common_tick(struct m0_fom *fom, void *data, int *phase,
				 int rqtype)
{
	struct clovis_st_fom_ctx *fctx = data;
	struct m0_clovis_idx     *idxs = fctx->sfc_ctx->csc_index;
	struct m0_fid            *ifid = fctx->sfc_ctx->csc_ifid;
	struct m0_bufvec         *keys = &fctx->sfc_keys;
	struct m0_bufvec         *vals = &fctx->sfc_vals;
	int                      *rcs  = fctx->sfc_rcs;
	int64_t                   ci;
	int                       rc;

	M0_LOG(M0_DEBUG, "i=%d fired=%d rqtype=%d",
	       fctx->sfc_i, !!fctx->sfc_fired, rqtype);
	if (!fctx->sfc_fired) {
		ci = m0_atomic64_sub_return(&fctx->sfc_ctx->csc_counter, 1);
		M0_LOG(M0_DEBUG, "fom=%p rq=%d ci=%d", fom, rqtype, (int)ci);
		if (ci < 0)
			return -1;

		fctx->sfc_ci = ci;
		fctx->sfc_fired = true;
		M0_SET0(keys);
		M0_SET0(vals);
		clovis_st_kv_alloc_and_fill(keys, vals,
					    (int)ci * CMT_BATCH_OFF,
					    (int)ci * CMT_BATCH_OFF + CMT_BATCH,
					    M0_IN(rqtype, (REQ_GET, REQ_DEL)));
		switch(rqtype) {
		case REQ_CREATE:
			clovis_st_ifid_fill(&ifid[ci], ci);
			m0_clovis_idx_init(&idxs[ci],
					   &fctx->sfc_ctx->csc_realm.co_realm,
					   (struct m0_uint128 *)&ifid[ci]);
			rc = m0_clovis_entity_create(NULL, &idxs[ci].in_entity,
						     &fctx->sfc_op);
			break;
		case REQ_DELETE:
			rc = m0_clovis_entity_delete(&idxs[ci].in_entity,
						     &fctx->sfc_op);
			break;
		case REQ_PUT:
			rc = m0_clovis_idx_op(&idxs[ci % CMT_IDXS_NR],
					      M0_CLOVIS_IC_PUT,
					      keys, vals, rcs, 0,
					      &fctx->sfc_op);
			break;
		case REQ_DEL:
			rc = m0_clovis_idx_op(&idxs[ci % CMT_IDXS_NR],
					      M0_CLOVIS_IC_DEL,
					      keys, NULL, rcs, 0,
					      &fctx->sfc_op);
			break;
		case REQ_GET:
			rc = m0_clovis_idx_op(&idxs[ci % CMT_IDXS_NR],
					      M0_CLOVIS_IC_GET,
					      keys, vals, rcs, 0,
					      &fctx->sfc_op);
			break;
		default:
			M0_IMPOSSIBLE("");
		}
		M0_ASSERT(rc == 0);
		m0_clovis_op_launch(&fctx->sfc_op, 1);
	}
	rc = clovis_st_op_tick_ret(fctx->sfc_op, fom);
	if (rc == M0_FSO_WAIT)
		return M0_RC(rc);

	switch(rqtype) {
	case REQ_DELETE:
	case REQ_CREATE:
		break;
	case REQ_GET:
		ci = fctx->sfc_ci;
		clovis_st_vals_check(keys, vals, (int)ci * CMT_BATCH_OFF,
				     (int)ci * CMT_BATCH_OFF + CMT_BATCH);
	default:
		M0_ASSERT(m0_forall(i, CMT_IDXS_NR, rcs[i] == 0));
	}
	m0_clovis_op_fini(fctx->sfc_op);
	m0_free0(&fctx->sfc_op);

	clovis_st_dix_kv_destroy(keys, vals);
	fctx->sfc_fired = false;

	return M0_FSO_AGAIN;
}

static int clovis_st_get_tick(struct m0_fom *fom, void *data, int *phase)
{
	return clovis_st_common_tick(fom, data, phase, REQ_GET);
}

static int clovis_st_put_tick(struct m0_fom *fom, void *data, int *phase)
{
	return clovis_st_common_tick(fom, data, phase, REQ_PUT);
}

static int clovis_st_del_tick(struct m0_fom *fom, void *data, int *phase)
{
	return clovis_st_common_tick(fom, data, phase, REQ_DEL);
}

static int clovis_st_idx_create_tick(struct m0_fom *fom, void *data, int *phase)
{
	return clovis_st_common_tick(fom, data, phase, REQ_CREATE);
}

static int clovis_st_idx_delete_tick(struct m0_fom *fom, void *data, int *phase)
{
	return clovis_st_common_tick(fom, data, phase, REQ_DELETE);
}

static void clovis_st_fom_free(struct m0_fom_simple *fom_s)
{
	struct clovis_st_fom_ctx *fctx;

	fctx = container_of(fom_s, struct clovis_st_fom_ctx, sfc_fom_s);
	m0_semaphore_up(&fctx->sfc_done);
}

static void clovis_st_foms_run(struct clovis_st_mt_ctxt *ctx,
			    int (*tick)(struct m0_fom *, void *, int *))
{
	struct clovis_st_fom_ctx *fctx;
	struct m0_fom_simple     *fom_s;
	int                       foms_to_run;
	int                       i;

	foms_to_run = ctx->csc_foms_nr;
	for (i = 0; i < foms_to_run; ++i) {
		fctx = &ctx->csc_fctx[i];
		fom_s = &fctx->sfc_fom_s;
		fctx->sfc_i = i;
		fctx->sfc_fired = false;
		M0_SET0(fom_s);
		m0_fom_simple_post(fom_s, ctx->csc_reqh, NULL,
		                   tick, &clovis_st_fom_free, fctx, i);
	}
	for (i = 0; i < foms_to_run; ++i)
		m0_semaphore_down(&ctx->csc_fctx[i].sfc_done);
}

static void clovis_st_foms_init(struct clovis_st_mt_ctxt *ctx)
{
	struct m0_reqh           *reqh = ctx->csc_reqh;
	struct clovis_st_fom_ctx *fctx;
	uint64_t                  loc_nr;
	uint64_t                  foms_nr;
	int                       i;

	M0_PRE(ctx->csc_reqh != NULL);
	M0_PRE(ctx->csc_cli  != NULL);

	loc_nr = m0_reqh_nr_localities(reqh);
	foms_nr = loc_nr * CMT_FOM_PER_LOC;

	M0_ALLOC_ARR(ctx->csc_fctx, foms_nr);
	M0_ASSERT(ctx->csc_fctx != NULL);
	ctx->csc_foms_nr = foms_nr;
	ctx->csc_loc_nr  = loc_nr;

	for (i = 0; i < foms_nr; ++i) {
		fctx = &ctx->csc_fctx[i];
		fctx->sfc_cli = ctx->csc_cli;
		fctx->sfc_ctx = ctx;
		m0_semaphore_init(&fctx->sfc_done, 0);
	}
}

void clovis_st_mt_inst(struct m0_clovis *clovis)
{
	static struct clovis_st_mt_ctxt ctx = {};

	m0_clovis_container_init(&ctx.csc_realm, NULL,
				 &M0_CLOVIS_UBER_REALM,
				 clovis);

	ctx.csc_reqh  = &clovis->m0c_reqh;
	ctx.csc_cli   = clovis;
	fom_simple_svc_start(ctx.csc_reqh);
	clovis_st_foms_init(&ctx);

	m0_atomic64_set(&ctx.csc_counter, CMT_IDXS);
	clovis_st_foms_run(&ctx, clovis_st_idx_create_tick);

	m0_atomic64_set(&ctx.csc_counter, CMT_PUTS);
	clovis_st_foms_run(&ctx, clovis_st_put_tick);

	m0_atomic64_set(&ctx.csc_counter, CMT_GETS);
	clovis_st_foms_run(&ctx, clovis_st_get_tick);

	m0_atomic64_set(&ctx.csc_counter, CMT_DELS);
	clovis_st_foms_run(&ctx, clovis_st_del_tick);

	m0_atomic64_set(&ctx.csc_counter, CMT_IDXD);
	clovis_st_foms_run(&ctx, clovis_st_idx_delete_tick);
}

void clovis_st_lsfid_inst(struct m0_clovis *clovis,
			  void (*print)(struct m0_fid*))
{
	enum { BATCH_SZ = 128 };

	struct m0_fid              ifid0;
	struct m0_bufvec           keys;
	struct m0_clovis_op       *op = NULL;
	struct m0_clovis_idx       idx0;
	struct m0_clovis_container realm;
	int32_t                    rcs[BATCH_SZ];
	int                        i;
	int                        rc;

	m0_clovis_container_init(&realm, NULL, &M0_CLOVIS_UBER_REALM, clovis);
	ifid0 = M0_FID_TINIT(m0_dix_fid_type.ft_id, 0, 0);
	rc = m0_bufvec_alloc(&keys, BATCH_SZ, sizeof(struct m0_fid));
	M0_ASSERT(rc == 0);

	m0_clovis_idx_init(&idx0, &realm.co_realm, (struct m0_uint128 *)&ifid0);
	rc = m0_clovis_idx_op(&idx0, M0_CLOVIS_IC_LIST, &keys, NULL, rcs, 0,
			      &op);
	M0_ASSERT(rc == 0);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_STABLE), M0_TIME_NEVER);
	M0_ASSERT(rc == 0);

	m0_clovis_op_fini(op);
	m0_free0(&op);
	m0_clovis_idx_fini(&idx0);

	for (i = 0; i < BATCH_SZ; ++i) {
		if (rcs[i] == 0) {
			struct m0_fid *fid = keys.ov_buf[i];

			M0_ASSERT(keys.ov_vec.v_count[i] ==
				  sizeof(struct m0_fid));
			M0_ASSERT(m0_fid_is_set(fid));
			M0_LOG(M0_DEBUG, FID_F, FID_P(fid));
			if (print != NULL)
				print(fid);
		}
	}

	m0_bufvec_free(&keys);
}

void clovis_st_mt(void)
{
	clovis_st_mt_inst(clovis_st_get_instance());
}

void clovis_st_lsfid(void)
{
	clovis_st_lsfid_inst(clovis_st_get_instance(), NULL);
}

struct clovis_st_suite st_suite_clovis_mt = {
	.ss_name = "clovis_st_mt",
	.ss_init = clovis_st_mt_init,
	.ss_fini = clovis_st_mt_fini,
	.ss_tests = {
		{ "clovis_st_mt",    clovis_st_mt    },
		{ "clovis_st_lsfid", clovis_st_lsfid },
		{ NULL, NULL },
	}
};

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
