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
 * Original creation date: 12-Apr-2016
 */

/**
 * @addtogroup cas
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS
#include "lib/trace.h"

#include "rpc/rpclib.h"                /* m0_rpc_server_ctx */
#include "lib/finject.h"
#include "lib/memory.h"
#include "ut/misc.h"                   /* M0_UT_PATH */
#include "ut/ut.h"
#include "cas/client.h"
#include "cas/ctg_store.h"             /* m0_ctg_recs_nr */
#include "lib/finject.h"

#define SERVER_LOG_FILE_NAME       "cas_server.log"
#define IFID(x, y) M0_FID_TINIT('i', (x), (y))

extern const struct m0_tl_descr ndoms_descr;

enum {
	/**
	 * @todo Greater number of indices produces -E2BIG error in idx-deleteN
	 * test case.
	 */
	COUNT = 24,
	COUNT_TREE = 10,
	COUNT_VAL_BYTES = 4096,
	COUNT_META_ENTRIES = 3
};

enum idx_operation {
	IDX_CREATE,
	IDX_DELETE
};

M0_BASSERT(COUNT % 2 == 0);

struct async_wait {
	struct m0_clink     aw_clink;
	struct m0_semaphore aw_cb_wait;
	bool                aw_done;
};

/* Client context */
struct cl_ctx {
	/* Client network domain.*/
	struct m0_net_domain     cl_ndom;
	/* Client rpc context.*/
	struct m0_rpc_client_ctx cl_rpc_ctx;
	struct async_wait        cl_wait;
};

enum { MAX_RPCS_IN_FLIGHT = 10 };
/* Configures mero environment with given parameters. */
static char *cas_startup_cmd[] = { "m0d", "-T", "linux",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
                                "-e", "lnet:0@lo:12345:34:1",
                                "-H", "0@lo:12345:34:1",
				"-w", "10", "-F",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_SRC_PATH("cas/ut/conf.xc")};

static const char         *cdbnames[] = { "cas1" };
static const char      *cl_ep_addrs[] = { "0@lo:12345:34:2" };
static const char     *srv_ep_addrs[] = { "0@lo:12345:34:1" };
static struct m0_net_xprt *cs_xprts[] = { &m0_net_lnet_xprt };

static struct cl_ctx            casc_ut_cctx;
static struct m0_rpc_server_ctx casc_ut_sctx = {
		.rsx_xprts            = cs_xprts,
		.rsx_xprts_nr         = ARRAY_SIZE(cs_xprts),
		.rsx_argv             = cas_startup_cmd,
		.rsx_argc             = ARRAY_SIZE(cas_startup_cmd),
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
};

static int bufvec_empty_alloc(struct m0_bufvec *bufvec,
			      uint32_t          num_segs)
{
	M0_UT_ASSERT(num_segs > 0);
	bufvec->ov_buf = NULL;
	bufvec->ov_vec.v_nr = num_segs;
	M0_ALLOC_ARR(bufvec->ov_vec.v_count, num_segs);
	M0_UT_ASSERT(bufvec->ov_vec.v_count != NULL);
	M0_ALLOC_ARR(bufvec->ov_buf, num_segs);
	M0_UT_ASSERT(bufvec->ov_buf != NULL);
	return 0;
}

static void value_create(int size, int num, char *buf)
{
	int j;

	if (size == sizeof(uint64_t))
		*(uint64_t *)buf = num;
	else {
		M0_UT_ASSERT(size > num);
		for (j = 1; j <= num + 1; j++)
			*(char *)(buf + size - j) = 0xff & j;
		memset(buf, 0, size - 1 - num);
	}
}

static void vals_create(int count, int size, struct m0_bufvec *vals)
{
	int i;
	int rc;

	M0_PRE(vals != NULL);
	rc = m0_bufvec_alloc_aligned(vals, count, size, PAGE_SHIFT);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < count; i++)
		value_create(size, i, vals->ov_buf[i]);
}

static void vals_mix_create(int count, int large_size,
			   struct m0_bufvec *vals)
{
	int rc;
	int i;

	rc = bufvec_empty_alloc(vals, count);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(vals->ov_vec.v_nr == count);
	for (i = 0; i < count; i++) {
		vals->ov_vec.v_count[i] = i % 2 ? large_size :
						  sizeof(uint64_t);
		vals->ov_buf[i] = m0_alloc(vals->ov_vec.v_count[i]);
		M0_UT_ASSERT(vals->ov_buf[i] != NULL);
		value_create(vals->ov_vec.v_count[i], i, vals->ov_buf[i]);
	}
}

static int cas_client_init(struct cl_ctx *cctx, const char *cl_ep_addr,
			   const char *srv_ep_addr, const char* dbname,
			   struct m0_net_xprt *xprt)
{
	int                       rc;
	struct m0_rpc_client_ctx *cl_rpc_ctx;

	M0_PRE(cctx != NULL && cl_ep_addr != NULL && srv_ep_addr != NULL &&
	       dbname != NULL && xprt != NULL);

	rc = m0_net_domain_init(&cctx->cl_ndom, xprt);
	M0_UT_ASSERT(rc == 0);

	m0_semaphore_init(&cctx->cl_wait.aw_cb_wait, 0);
	cctx->cl_wait.aw_done          = false;
	cl_rpc_ctx = &cctx->cl_rpc_ctx;

	cl_rpc_ctx->rcx_net_dom            = &cctx->cl_ndom;
	cl_rpc_ctx->rcx_local_addr         = cl_ep_addr;
	cl_rpc_ctx->rcx_remote_addr        = srv_ep_addr;
	cl_rpc_ctx->rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT;
	cl_rpc_ctx->rcx_fid                = &g_process_fid;

	m0_fi_enable_once("m0_rpc_machine_init", "bulk_cutoff_4K");
	rc = m0_rpc_client_start(cl_rpc_ctx);
	M0_UT_ASSERT(rc == 0);

	return rc;
}

static void cas_client_fini(struct cl_ctx *cctx)
{
	int rc;

	rc = m0_rpc_client_stop(&cctx->cl_rpc_ctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&cctx->cl_ndom);
	m0_semaphore_fini(&cctx->cl_wait.aw_cb_wait);
}

static void casc_ut_init(struct m0_rpc_server_ctx *sctx,
			 struct cl_ctx            *cctx)
{
	int rc;

	M0_SET0(&sctx->rsx_mero_ctx);
	m0_fi_enable_once("m0_rpc_machine_init", "bulk_cutoff_4K");
	rc = m0_rpc_server_start(sctx);
	M0_UT_ASSERT(rc == 0);
	rc = cas_client_init(cctx, cl_ep_addrs[0],
			      srv_ep_addrs[0], cdbnames[0],
			      cs_xprts[0]);
	M0_UT_ASSERT(rc == 0);
}

static void casc_ut_fini(struct m0_rpc_server_ctx *sctx,
			 struct cl_ctx            *cctx)
{
	cas_client_fini(cctx);
	m0_rpc_server_stop(sctx);
}

static bool casc_chan_cb(struct m0_clink *clink)
{
	struct async_wait *aw = container_of(clink, struct async_wait,
					     aw_clink);
	struct m0_sm      *sm = container_of(clink->cl_chan, struct m0_sm,
					     sm_chan);

	if (sm->sm_state == CASREQ_FINAL) {
		aw->aw_done = true;
		m0_semaphore_up(&aw->aw_cb_wait);
	}
	return true;
}

static int ut_idx_crdel_wrp(enum idx_operation       op,
			    struct cl_ctx           *cctx,
			    const struct m0_fid     *ids,
			    uint64_t                 ids_nr,
			    m0_chan_cb_t             cb,
			    struct m0_cas_rec_reply *rep,
			    uint32_t                 flags)
{
	struct m0_cas_req       req;
	struct m0_cas_id       *cids;
	struct m0_chan         *chan;
	int                     rc;
	uint64_t                i;

	/* create cas ids by passed fids */
	M0_ALLOC_ARR(cids, ids_nr);
	if (cids == NULL)
		return M0_ERR(-ENOMEM);
	m0_forall(i, ids_nr, cids[i].ci_fid = ids[i], true);

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, cb);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	if (op == IDX_CREATE)
		rc = m0_cas_index_create(&req, cids, ids_nr, NULL);
	else
		rc = m0_cas_index_delete(&req, cids, ids_nr, NULL, flags);
	/* wait results */
	if (rc == 0) {
		if (cb != NULL) {
			m0_cas_req_unlock(&req);
			m0_semaphore_timeddown(&cctx->cl_wait.aw_cb_wait,
					       m0_time_from_now(5, 0));
			M0_UT_ASSERT(cctx->cl_wait.aw_done);
			cctx->cl_wait.aw_done = false;
			m0_cas_req_lock(&req);
		}
		else
			m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL),
					M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			M0_UT_ASSERT(m0_cas_req_nr(&req) == ids_nr);
			for (i = 0; i < ids_nr; i++)
				if (op == IDX_CREATE)
					m0_cas_index_create_rep(&req, i,
								&rep[i]);
				else
					m0_cas_index_delete_rep(&req, i,
								&rep[i]);
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	m0_free(cids);
	return rc;
}

static int ut_idx_create_async(struct cl_ctx            *cctx,
			       const struct m0_fid      *ids,
			       uint64_t                  ids_nr,
			       m0_chan_cb_t              cb,
			       struct m0_cas_rec_reply *rep)
{
	M0_UT_ASSERT(cb != NULL);
	return ut_idx_crdel_wrp(IDX_CREATE, cctx, ids, ids_nr, cb, rep, 0);
}

static int ut_idx_create(struct cl_ctx            *cctx,
			 const struct m0_fid      *ids,
			 uint64_t                  ids_nr,
			 struct m0_cas_rec_reply *rep)
{
	return ut_idx_crdel_wrp(IDX_CREATE, cctx, ids, ids_nr, NULL, rep, 0);
}

static int ut_lookup_idx(struct cl_ctx           *cctx,
			 const struct m0_fid     *ids,
			 uint64_t                 ids_nr,
			 struct m0_cas_rec_reply *rep)
{
	struct m0_cas_req  req;
	struct m0_cas_id  *cids;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* create cas ids by passed fids */
	M0_ALLOC_ARR(cids, ids_nr);
	if (cids == NULL)
		return M0_ERR(-ENOMEM);
	m0_forall(i, ids_nr, cids[i].ci_fid = ids[i], true);

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_index_lookup(&req, cids, ids_nr);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0)
			for (i = 0; i < ids_nr; i++)
				m0_cas_index_lookup_rep(&req, i, &rep[i]);
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	m0_free(cids);
	return rc;
}

static int ut_idx_flagged_delete(struct cl_ctx           *cctx,
				 const struct m0_fid     *ids,
				 uint64_t                 ids_nr,
				 struct m0_cas_rec_reply *rep,
				 uint32_t                 flags)
{
	return ut_idx_crdel_wrp(IDX_DELETE, cctx, ids, ids_nr, NULL,
				rep, flags);
}

static int ut_idx_delete(struct cl_ctx           *cctx,
			 const struct m0_fid     *ids,
			 uint64_t                 ids_nr,
			 struct m0_cas_rec_reply *rep)
{
	return ut_idx_flagged_delete(cctx, ids, ids_nr, rep, 0);
}

static int ut_idx_list(struct cl_ctx             *cctx,
		       const struct m0_fid       *start_fid,
		       uint64_t                   ids_nr,
		       uint64_t                  *rep_count,
		       struct m0_cas_ilist_reply *rep)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_index_list(&req, start_fid, ids_nr, 0);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			*rep_count = m0_cas_req_nr(&req);
			for (i = 0; i < *rep_count; i++)
				m0_cas_index_list_rep(&req, i, &rep[i]);
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static int ut_rec_put(struct cl_ctx           *cctx,
		      struct m0_cas_id        *index,
		      const struct m0_bufvec  *keys,
		      const struct m0_bufvec  *values,
		      struct m0_cas_rec_reply *rep,
		      uint32_t                 flags)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_put(&req, index, keys, values, NULL, flags);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0)
			for (i = 0; i < keys->ov_vec.v_nr; i++)
				m0_cas_put_rep(&req, i, &rep[i]);
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static void ut_get_rep_clear(struct m0_cas_get_reply *rep, uint32_t nr)
{
	uint32_t i;

	for (i = 0; i < nr; i++)
		m0_free(rep[i].cge_val.b_addr);
}

static int ut_rec_get(struct cl_ctx           *cctx,
		      struct m0_cas_id        *index,
		      const struct m0_bufvec  *keys,
		      struct m0_cas_get_reply *rep)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_get(&req, index, keys);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			M0_UT_ASSERT(m0_cas_req_nr(&req) == keys->ov_vec.v_nr);
			for (i = 0; i < keys->ov_vec.v_nr; i++) {
				m0_cas_get_rep(&req, i, &rep[i]);
				/*
				 * Lock value in memory, because it will be
				 * deallocated after m0_cas_req_fini().
				 */
				if (rep[i].cge_rc == 0)
					m0_cas_rep_mlock(&req, i);
			}
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static void ut_next_rep_clear(struct m0_cas_next_reply *rep, uint64_t nr)
{
	uint64_t i;

	for (i = 0; i < nr; i++) {
		m0_free(rep[i].cnp_key.b_addr);
		m0_free(rep[i].cnp_val.b_addr);
		M0_SET0(&rep[i]);
	}
}

static int ut_next_rec(struct cl_ctx            *cctx,
		       struct m0_cas_id         *index,
		       struct m0_bufvec         *start_keys,
		       uint32_t                 *recs_nr,
		       struct m0_cas_next_reply *rep,
		       uint64_t                 *count,
		       uint32_t                  flags)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_next(&req, index, start_keys, recs_nr, flags);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			*count = m0_cas_req_nr(&req);
			for (i = 0; i < *count; i++) {
				m0_cas_next_rep(&req, i, &rep[i]);
				/*
				 * Lock key/value in memory, because they will
				 * be deallocated after m0_cas_req_fini().
				 */
				if (rep[i].cnp_rc == 0)
					m0_cas_rep_mlock(&req, i);
			}
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static int ut_rec_del(struct cl_ctx           *cctx,
		      struct m0_cas_id        *index,
		      struct m0_bufvec        *keys,
		      struct m0_cas_rec_reply *rep)
{
	struct m0_cas_req  req;
	struct m0_chan    *chan;
	int                rc;
	uint64_t           i;

	/* start operation */
	M0_SET0(&req);
	m0_cas_req_init(&req, &cctx->cl_rpc_ctx.rcx_session,
			m0_locality0_get()->lo_grp);
	chan = &req.ccr_sm.sm_chan;
	M0_UT_ASSERT(chan != NULL);
	m0_clink_init(&cctx->cl_wait.aw_clink, NULL);
	m0_clink_add_lock(chan, &cctx->cl_wait.aw_clink);

	m0_cas_req_lock(&req);
	rc = m0_cas_del(&req, index, keys, NULL, 0);
	if (rc == 0) {
		/* wait results */
		m0_cas_req_wait(&req, M0_BITS(CASREQ_FINAL), M0_TIME_NEVER);
		rc = m0_cas_req_generic_rc(&req);
		if (rc == 0) {
			M0_UT_ASSERT(m0_cas_req_nr(&req) == keys->ov_vec.v_nr);
			for (i = 0; i < keys->ov_vec.v_nr; i++)
				m0_cas_del_rep(&req, i, rep);
		}
	}
	m0_cas_req_unlock(&req);
	m0_clink_del_lock(&cctx->cl_wait.aw_clink);
	m0_cas_req_fini_lock(&req);
	m0_clink_fini(&cctx->cl_wait.aw_clink);
	return rc;
}

static void idx_create(void)
{
	struct m0_cas_rec_reply rep       = { 0 };
	const struct m0_fid     ifid      = IFID(2, 3);
	const struct m0_fid     ifid_fake = IFID(2, 4);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid_fake, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_create_fail(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOMEM);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_create_a(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create_async(&casc_ut_cctx, &ifid, 1, casc_chan_cb, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_create_n(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_fid           ifid[COUNT];
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete_fail(void)
{
	struct m0_cas_rec_reply rep  = { 0 };
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOMEM);
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete_non_exist(void)
{
	struct m0_cas_rec_reply rep  = {};
	const struct m0_fid     ifid = IFID(2, 3);
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Try to remove non-existent index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, &rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == -ENOENT);

	/* Try to remove non-existent index with CROW flag. */
	rc = ut_idx_flagged_delete(&casc_ut_cctx, &ifid, 1, &rep, COF_CROW);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep.crr_rc == 0);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_delete_n(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_fid           ifid[COUNT];
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices*/
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == -ENOENT));

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_tree_insert(void)
{
	struct m0_cas_get_reply rep[COUNT_TREE];
	struct m0_cas_rec_reply rec_rep[COUNT_TREE];
	struct m0_fid           ifid[COUNT_TREE];
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;
	int                     i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* initialize data */
	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT_TREE);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, COUNT_TREE, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT_TREE, rec_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rec_rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rec_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rec_rep[i].crr_rc == 0));

	/* insert several records into each index */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values,
				rec_rep, 0);
		M0_UT_ASSERT(rc == 0);
	}
	/* get all data */
	m0_forall(i, COUNT_TREE, *(uint64_t*)values.ov_buf[i] = 0, true);
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_get(&casc_ut_cctx, &index, &keys, rep);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_forall(j, COUNT_TREE,
			       *(uint64_t*)rep[j].cge_val.b_addr == j * j));
		ut_get_rep_clear(rep, COUNT_TREE);
	}
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_tree_delete(void)
{
	struct m0_cas_rec_reply rep[COUNT_TREE];
	struct m0_fid           ifid[COUNT_TREE];
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;
	int                     i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* initialize data */
	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT_TREE);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, COUNT_TREE, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	/* insert several records into each index */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
		M0_UT_ASSERT(rc == 0);
	}

	/* delete all trees */
	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == -ENOENT));

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_tree_delete_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT_TREE];
	struct m0_cas_get_reply get_rep[COUNT_TREE];
	struct m0_fid           ifid[COUNT_TREE];
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;
	int                     i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* initialize data */
	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT_TREE, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT_TREE);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, COUNT_TREE, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));
	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	/* insert several records into each index */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
		M0_UT_ASSERT(rc == 0);
	}

	/* delete all trees */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == -ENOMEM);

	rc = ut_lookup_idx(&casc_ut_cctx, ifid, COUNT_TREE, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT_TREE, rep[i].crr_rc == 0));

	/* get all data */
	m0_forall(i, COUNT_TREE, *(uint64_t*)values.ov_buf[i] = 0, true);
	for (i = 0; i < COUNT_TREE; i++) {
		index.ci_fid = ifid[i];
		rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_forall(j, COUNT_TREE,
			       *(uint64_t*)get_rep[j].cge_val.b_addr == j * j));
		ut_get_rep_clear(get_rep, COUNT_TREE);
	}

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_list(void)
{
	struct m0_cas_rec_reply   rep[COUNT];
	struct m0_cas_ilist_reply rep_list[COUNT + COUNT_META_ENTRIES + 1];
	struct m0_fid             ifid[COUNT];
	uint64_t                  rep_count;
	int                       rc;
	int                       i;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* Create several indices. */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/* Get list of indices from start. */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT);
	M0_UT_ASSERT(m0_forall(i, COUNT, m0_fid_eq(&rep_list[i].clr_fid,
						   &ifid[i])));
	/* Get list of indices from another position. */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[COUNT / 2], COUNT,
			 &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= COUNT / 2 + 1); /* 1 for -ENOENT record */
	M0_UT_ASSERT(m0_forall(i, COUNT / 2,
				rep_list[i].clr_rc == 0 &&
				m0_fid_eq(&rep_list[i].clr_fid,
					  &ifid[i + COUNT / 2])));
	M0_UT_ASSERT(rep_list[COUNT / 2].clr_rc == -ENOENT);
	/**
	 * Get list of indices from the end. Should contain two records:
	 * the last index and -ENOENT record.
	 */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[COUNT - 1], COUNT,
			 &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= 2);
	M0_UT_ASSERT(m0_fid_eq(&rep_list[0].clr_fid, &ifid[COUNT - 1]));
	M0_UT_ASSERT(rep_list[1].clr_rc == -ENOENT);

	/* Get list of indices from start (provide m0_cas_meta_fid). */
	rc = ut_idx_list(&casc_ut_cctx, &m0_cas_meta_fid,
			 /* meta, catalogue-index, dead-index and -ENOENT */
			 COUNT + COUNT_META_ENTRIES + 1,
			 &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT + COUNT_META_ENTRIES + 1);
	M0_UT_ASSERT(m0_fid_eq(&rep_list[0].clr_fid, &m0_cas_meta_fid));
	M0_UT_ASSERT(m0_fid_eq(&rep_list[1].clr_fid, &m0_cas_ctidx_fid));
	M0_UT_ASSERT(m0_fid_eq(&rep_list[2].clr_fid, &m0_cas_dead_index_fid));
	for (i = COUNT_META_ENTRIES; i < COUNT + COUNT_META_ENTRIES; i++)
		M0_UT_ASSERT(m0_fid_eq(&rep_list[i].clr_fid,
				       &ifid[i-COUNT_META_ENTRIES]));
	M0_UT_ASSERT(rep_list[COUNT + COUNT_META_ENTRIES].clr_rc == -ENOENT);

	/* Delete all indices. */
	rc = ut_idx_delete(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(j, COUNT, rep[j].crr_rc == 0));
	/* Get list - should be empty. */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= 1);
	M0_UT_ASSERT(rep_list[0].clr_rc == -ENOENT);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void idx_list_fail(void)
{
	struct m0_cas_rec_reply   rep[COUNT];
	struct m0_cas_ilist_reply rep_list[COUNT];
	struct m0_fid             ifid[COUNT];
	uint64_t                  rep_count;
	int                       rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	m0_forall(i, COUNT, ifid[i] = IFID(2, 3 + i), true);
	/* create several indices */
	rc = ut_idx_create(&casc_ut_cctx, ifid, COUNT, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/* get list of indices from start */
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT);
	M0_UT_ASSERT(m0_forall(i, COUNT, m0_fid_eq(&rep_list[i].clr_fid,
						   &ifid[i])));
	/* get failed cases for list */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_idx_list(&casc_ut_cctx, &ifid[0], COUNT, &rep_count, rep_list);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT);
	M0_UT_ASSERT(m0_forall(i, COUNT, i == 0 ?
					 rep_list[i].clr_rc == -ENOMEM :
					 rep_list[i].clr_rc == -EPROTO));

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static bool next_rep_equals(const struct m0_cas_next_reply *rep,
			    void                           *key,
			    void                           *val)
{
	return memcmp(rep->cnp_key.b_addr, key, rep->cnp_key.b_nob) == 0 &&
	       memcmp(rep->cnp_val.b_addr, val, rep->cnp_val.b_nob) == 0;
}

static void next_common(struct m0_bufvec *keys,
			struct m0_bufvec *values,
			uint32_t          flags)
{
	struct m0_cas_rec_reply   rep[COUNT];
	struct m0_cas_next_reply  next_rep[COUNT];
	const struct m0_fid       ifid = IFID(2, 3);
	struct m0_cas_id          index = {};
	struct m0_bufvec          start_key;
	bool                      slant;
	bool                      exclude_start_key;
	uint32_t                  recs_nr;
	uint64_t                  rep_count;
	int                       rc;

	slant = flags & COF_SLANT;
	exclude_start_key = flags & COF_EXCLUDE_START_KEY;

	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	rc = m0_bufvec_alloc(&start_key, 1, keys->ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);

	/* perform next for all records */
	recs_nr = COUNT;
	value_create(start_key.ov_vec.v_count[0], 0, start_key.ov_buf[0]);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == recs_nr);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	if (!exclude_start_key || slant)
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i],
						       values->ov_buf[i])));
	else
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i + 1],
						       values->ov_buf[i + 1])));
	ut_next_rep_clear(next_rep, rep_count);

	/* perform next for small rep */
	recs_nr = COUNT / 2;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT / 2);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	if (!exclude_start_key || slant)
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i],
						       values->ov_buf[i])));
	else
		M0_UT_ASSERT(m0_forall(i, rep_count,
				       next_rep_equals(&next_rep[i],
						       keys->ov_buf[i + 1],
						       values->ov_buf[i + 1])));
	ut_next_rep_clear(next_rep, rep_count);

	/* perform next for half records */
	value_create(start_key.ov_vec.v_count[0],
		     !slant ? COUNT / 2 : COUNT / 2 + 1,
		     start_key.ov_buf[0]);
	recs_nr = COUNT;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count <= recs_nr);
	M0_UT_ASSERT(m0_forall(i, COUNT / 2, next_rep[i].cnp_rc == 0));
	if (!exclude_start_key)
		M0_UT_ASSERT(
			m0_forall(i, COUNT / 2,
				  next_rep_equals(
					  &next_rep[i],
					  keys->ov_buf[COUNT / 2 + i],
					  values->ov_buf[COUNT / 2 + i])));
	else
		M0_UT_ASSERT(
			m0_forall(i, COUNT / 2,
				  next_rep_equals(
					  &next_rep[i],
					  keys->ov_buf[COUNT / 2 + i + 1],
					  values->ov_buf[COUNT / 2 + i + 1])));
	M0_UT_ASSERT(next_rep[COUNT / 2].cnp_rc == -ENOENT);
	ut_next_rep_clear(next_rep, rep_count);

	/* perform next for empty result set */
	value_create(start_key.ov_vec.v_count[0],
		     !slant ? COUNT : COUNT + 1,
		     start_key.ov_buf[0]);
	recs_nr = COUNT;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr,
			 next_rep, &rep_count, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count >= 1);
	M0_UT_ASSERT(next_rep[0].cnp_rc == -ENOENT);
	ut_next_rep_clear(next_rep, rep_count);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&start_key);
}

static void next(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Usual case. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	/* Call next_common() with 'slant' disabled. */
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* 'Slant' case. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	/*
	 * It is required by next_common() function to fill
	 * keys/values using shift 1.
	 */
	m0_forall(i, keys.ov_vec.v_nr,
		  (*(uint64_t*)keys.ov_buf[i]   = i + 1,
		   *(uint64_t*)values.ov_buf[i] = (i + 1) * (i + 1),
		   true));
	/* Call next_common() with 'slant' enabled. */
	next_common(&keys, &values, COF_SLANT);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* 'First key exclude' case. */
	rc = m0_bufvec_alloc(&keys, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT + 1);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	/* Call next_common() with 'first key exclude' enabled. */
	next_common(&keys, &values, COF_EXCLUDE_START_KEY);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* 'First key exclude' with 'slant' case. */
	rc = m0_bufvec_alloc(&keys, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT + 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT + 1);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr,
		  (*(uint64_t*)keys.ov_buf[i]   = i + 1,
		   *(uint64_t*)values.ov_buf[i] = (i + 1) * (i + 1),
		   true));
	/* Call next_common() with 'first key exclude' and 'slant' enabled. */
	next_common(&keys, &values, COF_SLANT | COF_EXCLUDE_START_KEY);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					true));
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	next_common(&keys, &values, 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_fail(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_bufvec         start_key;
	uint32_t                 recs_nr;
	uint64_t                 rep_count;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	/* insert index and records */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/* clear result set */
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = 0,
					*(uint64_t*)values.ov_buf[i] = 0,
					true));
	/* perform next for all records */
	recs_nr = COUNT;
	rc = m0_bufvec_alloc(&start_key, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	*(uint64_t *)start_key.ov_buf[0] = 0;
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr,
			 next_rep, &rep_count, 0);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_bufvec_free(&start_key);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_multi_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         start_keys;
	uint32_t                 recs_nr[3];
	uint64_t                 rep_count;
	int                      rc;
	int                      i;

	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/*
	 * Perform next for three keys: first, middle and last.
	 */
	rc = m0_bufvec_alloc(&start_keys, 3, keys->ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	value_create(start_keys.ov_vec.v_count[0], 0, start_keys.ov_buf[0]);
	value_create(start_keys.ov_vec.v_count[1], COUNT / 2,
		     start_keys.ov_buf[1]);
	value_create(start_keys.ov_vec.v_count[2], COUNT,
		     start_keys.ov_buf[2]);
	recs_nr[0] = COUNT / 2 - 1;
	recs_nr[1] = COUNT / 2 - 1;
	recs_nr[2] = 1;
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_keys, recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == COUNT - 1);
	M0_UT_ASSERT(m0_forall(i, COUNT - 2, next_rep[i].cnp_rc == 0));
	M0_UT_ASSERT(next_rep[COUNT - 2].cnp_rc == -ENOENT);
	M0_UT_ASSERT(m0_forall(i, COUNT / 2 - 1,
			       next_rep_equals(&next_rep[i],
					       keys->ov_buf[i],
					       values->ov_buf[i])));
	for (i = COUNT / 2 - 1; i < COUNT - 2; i++) {
		M0_UT_ASSERT(next_rep_equals(&next_rep[i],
					     keys->ov_buf[i + 1],
					     values->ov_buf[i + 1]));
	}
	ut_next_rep_clear(next_rep, rep_count);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&start_keys);
}

static void next_multi(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void next_multi_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					true));
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	next_multi_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void put_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	int                     rc;

	M0_UT_ASSERT(keys != NULL && values != NULL);

	M0_SET_ARR0(rep);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
}

/*
 * Put small Keys and Values.
 */
static void put(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Test fragmented requests.
 */
static void recs_fragm(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	struct m0_cas_next_reply next_rep[60];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_bufvec         start_keys;
	uint64_t                 rep_count;
	uint32_t                 recs_nr[20];
	int                      i;
	int                      j;
	int                      k;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));

	M0_SET_ARR0(rep);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;


	/* Test fragmented PUT. */
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	//m0_fi_enable_once("cas_req_fragmentation", "fragm_error");
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");


	/* Test fragmented GET. */
	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys.ov_vec.v_nr,
		     memcmp(get_rep[i].cge_val.b_addr,
			    values.ov_buf[i],
			    values.ov_vec.v_count[i] ) == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	ut_get_rep_clear(get_rep, keys.ov_vec.v_nr);


	/* Test fragmented NEXT. */
	M0_SET_ARR0(rep);
	M0_SET_ARR0(next_rep);
	rc = m0_bufvec_alloc(&start_keys, 20, keys.ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < 20; i++) {
		value_create(start_keys.ov_vec.v_count[i], i,
			     start_keys.ov_buf[i]);
		recs_nr[i] = 3;
	}
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_keys, recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == 60);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");

	k = 0;
	for (i = 0; i < 20; i++) {
		for (j = 0; j < 3; j++) {
			M0_UT_ASSERT(next_rep_equals(&next_rep[k++],
						     keys.ov_buf[i + j],
						     values.ov_buf[i + j]));
		}
	}
	ut_next_rep_clear(next_rep, rep_count);


	/* Test fragmented DEL. */
	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");


	/* Check selected values - must be empty. */
	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, get_rep[i].cge_rc == -ENOENT));
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	ut_get_rep_clear(get_rep, COUNT);


	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&start_keys);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Test errors during fragmented requests.
 */
static void recs_fragm_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	const struct m0_fid     ifids[2] = {IFID(2, 3), IFID(2, 4)};
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	M0_SET_ARR0(rep);

	/* Create indices. */
	rc = ut_idx_create(&casc_ut_cctx, ifids, 2, rep);
	M0_UT_ASSERT(rc == 0);

	index.ci_fid = ifids[0];

	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	m0_fi_enable_off_n_on_m("cas_req_fragmentation", "fragm_error", 1, 1);
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	m0_fi_disable("cas_req_fragmentation", "fragm_error");
	M0_UT_ASSERT(rc == -E2BIG);

	M0_SET_ARR0(rep);

	index.ci_fid = ifids[1];

	m0_fi_enable_once("m0_rpc_item_max_payload_exceeded",
			  "payload_too_large1");
	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large2",
				10, 1);
	m0_fi_enable_off_n_on_m("cas_req_replied_cb", "send-failure", 1, 1);
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large2");
	m0_fi_disable("cas_req_replied_cb", "send-failure");
	M0_UT_ASSERT(rc == -ENOTCONN);

	/* Remove indices. */
	rc = ut_idx_delete(&casc_ut_cctx, ifids, 2, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put Large Keys and Values.
 */
static void put_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					  true));

	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					  true));
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i,
					  true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	put_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put small Keys and Values with 'create' or 'overwrite' flag.
 */
static void put_save_common(uint32_t flags)
{
	struct m0_cas_rec_reply rep[1];
	struct m0_cas_get_reply grep[1];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint32_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	*(uint64_t*)keys.ov_buf[0] = 1;
	*(uint32_t*)values.ov_buf[0] = 1;

	M0_SET_ARR0(rep);

	/* create index */
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);

	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	m0_bufvec_free(&values);
	/* Allocate value of size greater than size of previous value. */
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);

	*(uint64_t*)values.ov_buf[0] = 2;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == 0);
	if (flags & COF_CREATE)
		M0_UT_ASSERT(*(uint32_t*)grep[0].cge_val.b_addr == 1);
	if (flags & COF_OVERWRITE)
		M0_UT_ASSERT(*(uint64_t*)grep[0].cge_val.b_addr == 2);
	ut_get_rep_clear(grep, 1);

	*(uint64_t*)values.ov_buf[0] = 3;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, flags);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == 0);
	if (flags & COF_CREATE)
		M0_UT_ASSERT(*(uint32_t*)grep[0].cge_val.b_addr == 1);
	if (flags & COF_OVERWRITE)
		M0_UT_ASSERT(*(uint64_t*)grep[0].cge_val.b_addr == 3);
	ut_get_rep_clear(grep, 1);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Put small Keys and Values with 'create' flag.
 */
static void put_create(void)
{
	put_save_common(COF_CREATE);
}

/*
 * Put small Keys and Values with 'overwrite' flag.
 */
static void put_overwrite(void)
{
	put_save_common(COF_OVERWRITE);
}

/*
 * Put small Keys and Values with 'create-on-write' flag.
 */
static void put_crow(void)
{
	struct m0_cas_rec_reply rep[1];
	struct m0_cas_get_reply grep[1];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	*(uint64_t*)keys.ov_buf[0] = 1;
	*(uint64_t*)values.ov_buf[0] = 1;

	M0_SET_ARR0(rep);

	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, COF_CROW);

	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(*(uint64_t*)grep[0].cge_val.b_addr == 1);
	ut_get_rep_clear(grep, 1);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

/*
 * Fail to create catalogue during putting of small Keys and Values with
 * 'create-on-write' flag.
 */
static void put_crow_fail(void)
{
	struct m0_cas_rec_reply rep[1];
	struct m0_cas_get_reply grep[1];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, 1, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	*(uint64_t*)keys.ov_buf[0] = 1;
	*(uint64_t*)values.ov_buf[0] = 1;

	M0_SET_ARR0(rep);

	m0_fi_enable_off_n_on_m("ctg_buf_get", "cas_alloc_fail", 1, 1);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, COF_CROW);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_disable("ctg_buf_get", "cas_alloc_fail");

	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, grep);
	M0_UT_ASSERT(rc == -ENOENT);

	/* Try to lookup non-existent index. */
	rc = ut_lookup_idx(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == -ENOENT);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void put_fail_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == -ENOMEM);

	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void put_fail(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	put_fail_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
}

static void put_bulk_fail(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;

	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	put_fail_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
}

static void upd(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* update several records */
	m0_forall(i, values.ov_vec.v_nr / 3,
		  *(uint64_t*)values.ov_buf[i] = COUNT * COUNT, true);
	keys.ov_vec.v_nr /= 3;
	values.ov_vec.v_nr = keys.ov_vec.v_nr;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	values.ov_vec.v_nr = keys.ov_vec.v_nr = COUNT;
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, i < values.ov_vec.v_nr / 3 ?
				rep[i].crr_rc == -EEXIST : rep[i].crr_rc == 0));

	m0_forall(i, values.ov_vec.v_nr,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	/* check selected values*/
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys.ov_vec.v_nr,
			       *(uint64_t*)get_rep[i].cge_val.b_addr == i * i));
	ut_get_rep_clear(get_rep, keys.ov_vec.v_nr);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	int                     rc;

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* Delete all records */
	rc = ut_rec_del(&casc_ut_cctx, &index, keys, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));
	/* check selected values - must be empty*/
	rc = ut_rec_get(&casc_ut_cctx, &index, keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, get_rep[i].cge_rc == -ENOENT));
	ut_get_rep_clear(get_rep, COUNT);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
}

static void del(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	del_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	/* Bulk keys and  values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	del_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* Delete all records */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	/* check selected values - must be empty*/
	m0_forall(i, values.ov_vec.v_nr,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, get_rep[i].cge_rc == 0));
	ut_get_rep_clear(get_rep, COUNT);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void del_n(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr != 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	/* Delete several records */
	keys.ov_vec.v_nr /= 3;
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep);
	M0_UT_ASSERT(rc == 0);
	/* restore old count value */
	keys.ov_vec.v_nr = COUNT;
	/* check selected values - some records not found*/
	m0_forall(i, values.ov_vec.v_nr,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT,
			       (i < COUNT / 3) ? get_rep[i].cge_rc == -ENOENT :
			       rep[i].crr_rc == 0 &&
			       *(uint64_t*)get_rep[i].cge_val.b_addr == i * i));
	ut_get_rep_clear(get_rep, COUNT);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void null_value(void)
{
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_cas_id         index = {};
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_bufvec         start_key;
	uint32_t                 recs_nr;
	uint64_t                 rep_count;
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_empty_alloc(&values, COUNT);
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, *(uint64_t*)keys.ov_buf[i] = i, true);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;

	/* Insert new records with empty (NULL) values. */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);

	/* Get inserted records through 'GET' request. */
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys.ov_vec.v_nr,
			       (get_rep[i].cge_rc == 0 &&
				get_rep[i].cge_val.b_addr == NULL &&
				get_rep[i].cge_val.b_nob == 0)));

	/* Get inserted records through 'NEXT' request. */
	rc = m0_bufvec_alloc(&start_key, 1, keys.ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	recs_nr = COUNT;
	value_create(start_key.ov_vec.v_count[0], 0, start_key.ov_buf[0]);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep_count == recs_nr);
	M0_UT_ASSERT(m0_forall(i, rep_count, next_rep[i].cnp_rc == 0));
	M0_UT_ASSERT(m0_forall(i, rep_count,
			       next_rep_equals(&next_rep[i],
					       keys.ov_buf[i],
					       values.ov_buf[i])));
	m0_bufvec_free(&start_key);

	/* Delete records. */
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void get_common(struct m0_bufvec *keys, struct m0_bufvec *values)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	int                     rc;

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, keys, values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	/* check selected values */
	rc = ut_rec_get(&casc_ut_cctx, &index, keys, get_rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, keys->ov_vec.v_nr,
		     memcmp(get_rep[i].cge_val.b_addr,
			    values->ov_buf[i],
			    values->ov_vec.v_count[i] ) == 0));
	ut_get_rep_clear(get_rep, keys->ov_vec.v_nr);

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
}

static void get(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void get_bulk(void)
{
	struct m0_bufvec keys;
	struct m0_bufvec values;
	int              rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	/* Bulk keys and values. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk values. */
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i] = i, true));
	vals_create(COUNT, COUNT_VAL_BYTES, &values);
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk keys. */
	vals_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i] = i,
					true));
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	/* Bulk mix. */
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &keys);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, values.ov_vec.v_nr, (*(uint64_t*)values.ov_buf[i]   = i,
					true));
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);

	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					true));
	vals_mix_create(COUNT, COUNT_VAL_BYTES, &values);
	get_common(&keys, &values);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void get_fail(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	struct m0_cas_get_reply get_rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);

	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);

	m0_forall(i, values.ov_vec.v_nr / 3,
		 (*(uint64_t*)values.ov_buf[i] = 0, true));
	/* check selected values */
	m0_fi_enable_once("creq_op_alloc", "cas_alloc_fail");
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void recs_count(void)
{
	struct m0_cas_rec_reply rep[COUNT];
	const struct m0_fid     ifid = IFID(2, 3);
	struct m0_cas_id        index = {};
	struct m0_bufvec        keys;
	struct m0_bufvec        values;
	int                     rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	M0_SET_ARR0(rep);
	M0_UT_ASSERT(m0_ctg_rec_nr() == 0);
	M0_UT_ASSERT(m0_ctg_rec_size() == 0);
	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	index.ci_fid = ifid;
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ctg_rec_nr() == COUNT);
	M0_UT_ASSERT(m0_ctg_rec_size() == COUNT * 2 * sizeof(uint64_t));
	rc = ut_rec_del(&casc_ut_cctx, &index, &keys, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ctg_rec_nr() == 0);
	/* Currently total size of records is not decremented on deletion. */
	M0_UT_ASSERT(m0_ctg_rec_size() == COUNT * 2 * sizeof(uint64_t));

	/*
	 * Check total records size overflow.
	 * The total records size in this case should stick to ~0ULL.
	 */
	m0_fi_enable_off_n_on_m("ctg_state_update", "test_overflow", 1, 1);
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	m0_fi_disable("ctg_state_update", "test_overflow");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_ctg_rec_nr() == COUNT);
	M0_UT_ASSERT(m0_ctg_rec_size() == ~0ULL);
	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}

static void reply_too_large(void)
{
	struct m0_bufvec         keys;
	struct m0_bufvec         values;
	struct m0_cas_rec_reply  rep[COUNT];
	struct m0_cas_get_reply  get_rep[COUNT];
	struct m0_cas_next_reply next_rep[COUNT];
	const struct m0_fid      ifid = IFID(2, 3);
	struct m0_bufvec         start_key;
	uint32_t                 recs_nr;
	uint64_t                 rep_count;
	struct m0_cas_id         index = {};
	int                      rc;

	casc_ut_init(&casc_ut_sctx, &casc_ut_cctx);
	rc = m0_bufvec_alloc(&keys, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&values, COUNT, sizeof(uint64_t));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(keys.ov_vec.v_nr == COUNT);
	M0_UT_ASSERT(keys.ov_vec.v_nr == values.ov_vec.v_nr);
	m0_forall(i, keys.ov_vec.v_nr, (*(uint64_t*)keys.ov_buf[i]   = i,
					*(uint64_t*)values.ov_buf[i] = i * i,
					true));
	M0_SET_ARR0(rep);
	M0_SET_ARR0(get_rep);

	rc = ut_idx_create(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rep[0].crr_rc == 0);
	index.ci_fid = ifid;
	/* Insert new records */
	rc = ut_rec_put(&casc_ut_cctx, &index, &keys, &values, rep, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_forall(i, COUNT, rep[i].crr_rc == 0));

	m0_fi_enable_off_n_on_m("m0_rpc_item_max_payload_exceeded",
				"payload_too_large1", 1, 1);
	rc = ut_rec_get(&casc_ut_cctx, &index, &keys, get_rep);
	M0_UT_ASSERT(rc == -E2BIG);

	M0_SET_ARR0(next_rep);
	rc = m0_bufvec_alloc(&start_key, 1, keys.ov_vec.v_count[0]);
	M0_UT_ASSERT(rc == 0);
	/* perform next for all records */
	recs_nr = COUNT;
	value_create(start_key.ov_vec.v_count[0], 0, start_key.ov_buf[0]);
	rc = ut_next_rec(&casc_ut_cctx, &index, &start_key, &recs_nr, next_rep,
			 &rep_count, 0);
	M0_UT_ASSERT(rc == -E2BIG);
	m0_fi_disable("m0_rpc_item_max_payload_exceeded", "payload_too_large1");

	/* Remove index. */
	rc = ut_idx_delete(&casc_ut_cctx, &ifid, 1, rep);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&keys);
	m0_bufvec_free(&values);
	casc_ut_fini(&casc_ut_sctx, &casc_ut_cctx);
}


struct m0_ut_suite cas_client_ut = {
	.ts_name   = "cas-client",
	.ts_owners = "Leonid",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "idx-create",             idx_create,             "Leonid" },
		{ "idx-create-fail",        idx_create_fail,        "Leonid" },
		{ "idx-create-async",       idx_create_a,           "Leonid" },
		{ "idx-delete",             idx_delete,             "Leonid" },
		{ "idx-delete-fail",        idx_delete_fail,        "Leonid" },
		{ "idx-delete-non-exist",   idx_delete_non_exist,   "Sergey" },
		{ "idx-createN",            idx_create_n,           "Leonid" },
		{ "idx-deleteN",            idx_delete_n,           "Leonid" },
		{ "idx-list",               idx_list,               "Leonid" },
		{ "idx-list-fail",          idx_list_fail,          "Leonid" },
		{ "next",                   next,                   "Leonid" },
		{ "next-fail",              next_fail,              "Leonid" },
		{ "next-multi",             next_multi,             "Egor"   },
		{ "next-bulk",              next_bulk,              "Leonid" },
		{ "next-multi-bulk",        next_multi_bulk,        "Leonid" },
		{ "put",                    put,                    "Leonid" },
		{ "put-bulk",               put_bulk,               "Leonid" },
		{ "put-create",             put_create,             "Sergey" },
		{ "put-overwrite",          put_overwrite,          "Sergey" },
		{ "put-crow",               put_crow,               "Sergey" },
		{ "put-fail",               put_fail,               "Leonid" },
		{ "put-bulk-fail",          put_bulk_fail,          "Leonid" },
		{ "put-crow-fail",          put_crow_fail,          "Sergey" },
		{ "get",                    get,                    "Leonid" },
		{ "get-bulk",               get_bulk,               "Leonid" },
		{ "get-fail",               get_fail,               "Leonid" },
		{ "upd",                    upd,                    "Leonid" },
		{ "del",                    del,                    "Leonid" },
		{ "del-bulk",               del_bulk,               "Leonid" },
		{ "del-fail",               del_fail,               "Leonid" },
		{ "delN",                   del_n,                  "Leonid" },
		{ "null-value",             null_value,             "Egor"   },
		{ "idx-tree-insert",        idx_tree_insert,        "Leonid" },
		{ "idx-tree-delete",        idx_tree_delete,        "Leonid" },
		{ "idx-tree-delete-fail",   idx_tree_delete_fail,   "Leonid" },
		{ "recs-count",             recs_count,             "Leonid" },
		{ "reply-too-large",        reply_too_large,        "Sergey" },
		{ "recs-fragm",             recs_fragm,             "Sergey" },
		{ "recs_fragm_fail",        recs_fragm_fail,        "Sergey" },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of cas group */

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
