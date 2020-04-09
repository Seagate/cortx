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
 * Original creation date: 24-Jun-2016
 */


/**
 * @addtogroup dix
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/vec.h"
#include "pool/pool.h"
#include "fid/fid.h"
#include "dix/imask.h"
#include "dix/layout.h"
#include "dix/client.h"
#include "dix/client_internal.h"
#include "dix/meta.h"

#define DFID(x, y) M0_FID_TINIT('x', (x), (y))

M0_INTERNAL const struct m0_fid m0_dix_root_fid   = DFID(0,1);
M0_INTERNAL const struct m0_fid m0_dix_layout_fid = DFID(0,2);
M0_INTERNAL const struct m0_fid m0_dix_ldescr_fid = DFID(0,3);

enum {
	DIX_META_INDICES_ROOT   = 0,
	DIX_META_INDICES_LAYOUT = 1,
	DIX_META_INDICES_DESCR  = 2,
        DIX_META_INDICES_NR     = 3
};

static struct m0_dix_cli *meta_req_cli(const struct m0_dix_meta_req *req)
{
	return req->dmr_req.dr_cli;
}

static int dix_mreq_rc(const struct m0_dix_req *req)
{
	return M0_RC(m0_dix_req_rc(req));
}

/**
 * Serialize iname into keys and pair <FID,layout descriptor> into keys
 * and vals bufvecs. Is used for insert the new pair of <key,val> into root
 * and read val by key from root (for read case we should fill only keys
 * parameter)
 */
static int dix_root_add(struct m0_bufvec          *keys,
			struct m0_bufvec          *vals,
			uint32_t                   idx,
			const char                *iname,
			const struct m0_fid       *ifid,
			const struct m0_dix_ldesc *idesc)
{
	m0_bcount_t      klen;
	struct m0_bufvec val;
	int              rc;

	M0_PRE(keys != NULL);
	M0_PRE(iname != NULL);
	M0_PRE((vals != NULL) == (ifid != NULL));
	M0_PRE((vals != NULL) == (idesc != NULL));
	klen = strlen(iname);
	if (vals != NULL) {
		rc = m0_dix__meta_val_enc(ifid, idesc, 1, &val);
		if (rc != 0)
			return M0_ERR(rc);
	}
	keys->ov_buf[idx] = m0_alloc(klen);
	if (keys->ov_buf[idx] == NULL) {
		if (vals != NULL)
			m0_bufvec_free(&val);
		return M0_ERR(-ENOMEM);
	}
	if (vals != NULL) {
		vals->ov_vec.v_count[idx] = val.ov_vec.v_count[0];
		vals->ov_buf[idx] = val.ov_buf[0];
		m0_free(val.ov_buf);
		m0_free(val.ov_vec.v_count);
	}
	keys->ov_vec.v_count[idx] = klen;
	memcpy(keys->ov_buf[idx], iname, klen);
	return 0;
}

static int dix_root_put(struct m0_dix_cli         *cli,
			struct m0_sm_group        *grp,
			const struct m0_dix_ldesc *dld_layout,
			const struct m0_dix_ldesc *dld_ldescr)
{
	struct m0_dix_req dreq;
	struct m0_dix     root = {};
	struct m0_bufvec  keys = {};
	struct m0_bufvec  vals;
	int               rc;

	rc = m0_dix__root_set(cli, &root) ?:
	     m0_bufvec_empty_alloc(&keys, 2) ?:
	     m0_bufvec_empty_alloc(&vals, 2);
	if (rc != 0) {
		m0_dix_fini(&root);
		m0_bufvec_free(&keys);
		return M0_ERR(rc);
	}
	rc = dix_root_add(&keys, &vals, 0, "layout", &m0_dix_layout_fid,
			   dld_layout) ?:
	     dix_root_add(&keys, &vals, 1, "layout-descr", &m0_dix_ldescr_fid,
			   dld_ldescr);
	if (rc != 0)
		goto bvecs_free;
	m0_dix_mreq_init(&dreq, cli, grp);
	m0_dix_req_lock(&dreq);
	rc = m0_dix_put(&dreq, &root, &keys, &vals, NULL, 0) ?:
	     m0_dix_req_wait(&dreq, M0_BITS(DIXREQ_FINAL, DIXREQ_FAILURE),
			      M0_TIME_NEVER) ?:
	     dix_mreq_rc(&dreq);
	m0_dix_req_unlock(&dreq);
	m0_dix_req_fini_lock(&dreq);
bvecs_free:
	m0_dix_fini(&root);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	return M0_RC(rc);
}

static void dix_meta_indices_fini(struct m0_dix *meta)
{
	int i;

	for (i = 0; i < DIX_META_INDICES_NR; i++)
		m0_dix_fini(&meta[i]);
}

static int dix_meta_indices_init(struct m0_dix        *indices,
				 struct m0_dix_cli    *cli,
				 struct m0_dix_ldesc  *dld_layout,
				 struct m0_dix_ldesc  *dld_ldescr)
{
	int rc;

	rc = m0_dix__root_set(cli, &indices[DIX_META_INDICES_ROOT]);
	if (rc != 0)
		return M0_ERR(rc);

	if (dld_layout != NULL) {
		indices[1].dd_fid = m0_dix_layout_fid;
		rc = m0_dix_desc_set(&indices[DIX_META_INDICES_LAYOUT],
				     dld_layout);
	}
	else {
		rc = m0_dix__layout_set(cli, &indices[DIX_META_INDICES_LAYOUT]);
	}
	if (rc != 0)
		goto err;

	if (dld_ldescr != NULL) {
		indices[2].dd_fid = m0_dix_ldescr_fid;
		rc = m0_dix_desc_set(&indices[DIX_META_INDICES_DESCR],
				     dld_ldescr);
	}
	else {
		rc = m0_dix__ldescr_set(cli, &indices[DIX_META_INDICES_DESCR]);
	}
	if (rc != 0)
		goto err;
	M0_ASSERT(rc == 0);
	return 0;
err:
	dix_meta_indices_fini(indices);
	return M0_ERR(rc);
}

static int dix_meta_create(struct m0_dix_cli   *cli,
			   struct m0_sm_group  *grp,
			   struct m0_dix_ldesc *dld_layout,
			   struct m0_dix_ldesc *dld_ldescr)
{
	struct m0_dix_req dreq;
	struct m0_dix     dix[DIX_META_INDICES_NR] = {};
	int               rc;

	M0_ENTRY();
	rc = dix_meta_indices_init(dix, cli, dld_layout, dld_ldescr);
	if (rc != 0)
		return M0_ERR(rc);
	m0_dix_mreq_init(&dreq, cli, grp);
	m0_dix_req_lock(&dreq);
	rc = m0_dix_create(&dreq, dix, ARRAY_SIZE(dix), NULL, 0) ?:
	     m0_dix_req_wait(&dreq, M0_BITS(DIXREQ_FINAL, DIXREQ_FAILURE),
			      M0_TIME_NEVER) ?:
	     dix_mreq_rc(&dreq);
	m0_dix_req_unlock(&dreq);
	m0_dix_req_fini_lock(&dreq);
	dix_meta_indices_fini(dix);
	return M0_RC(rc);
}

static int dix_meta_delete(struct m0_dix_cli   *cli,
			   struct m0_sm_group  *grp,
			   struct m0_dix_ldesc *dld_layout,
			   struct m0_dix_ldesc *dld_ldescr)
{
	struct m0_dix_req dreq;
	struct m0_dix     dix[DIX_META_INDICES_NR] = {};
	int               rc;

	M0_ENTRY();
	rc = dix_meta_indices_init(dix, cli, dld_layout, dld_ldescr);
	if (rc != 0)
		return M0_ERR(rc);
	m0_dix_mreq_init(&dreq, cli, grp);
	m0_dix_req_lock(&dreq);
	rc = m0_dix_delete(&dreq, dix, ARRAY_SIZE(dix), NULL, 0) ?:
	     m0_dix_req_wait(&dreq, M0_BITS(DIXREQ_FINAL, DIXREQ_FAILURE),
			      M0_TIME_NEVER) ?:
	     dix_mreq_rc(&dreq);
	m0_dix_req_unlock(&dreq);
	m0_dix_req_fini_lock(&dreq);
	dix_meta_indices_fini(dix);
	return M0_RC(rc);
}

static bool dix_meta_op_done_cb(struct m0_clink *clink)
{
	struct m0_dix_meta_req *req = M0_AMB(req, clink, dmr_clink);
	struct m0_sm           *sm  = M0_AMB(sm, clink->cl_chan, sm_chan);

	M0_PRE(req != NULL);
	if (M0_IN(sm->sm_state, (DIXREQ_FINAL, DIXREQ_FAILURE))) {
		m0_clink_del(clink);
		m0_chan_broadcast_lock(&req->dmr_chan);
	}
	return true;
}

M0_INTERNAL void m0_dix_meta_req_init(struct m0_dix_meta_req *req,
				      struct m0_dix_cli      *cli,
				      struct m0_sm_group     *grp)
{
	M0_ENTRY();
	M0_SET0(req);
	m0_dix_mreq_init(&req->dmr_req, cli, grp);
	m0_mutex_init(&req->dmr_wait_mutex);
	m0_chan_init(&req->dmr_chan, &req->dmr_wait_mutex);
	m0_clink_init(&req->dmr_clink, dix_meta_op_done_cb);
	M0_LEAVE();
}

static void dix_meta_req_fini(struct m0_dix_meta_req *req)
{
	M0_ENTRY();
	M0_PRE(req != NULL);
	m0_chan_fini_lock(&req->dmr_chan);
	m0_mutex_fini(&req->dmr_wait_mutex);
	m0_dix_req_fini(&req->dmr_req);
	m0_bufvec_free(&req->dmr_keys);
	m0_bufvec_free(&req->dmr_vals);
	m0_clink_fini(&req->dmr_clink);
	M0_LEAVE();
}

M0_INTERNAL void m0_dix_meta_req_fini(struct m0_dix_meta_req *req)
{
	M0_PRE(m0_dix_req_is_locked(&req->dmr_req));
	dix_meta_req_fini(req);
}

M0_INTERNAL void m0_dix_meta_req_fini_lock(struct m0_dix_meta_req *req)
{
	M0_PRE(!m0_dix_req_is_locked(&req->dmr_req));
	m0_dix_req_lock(&req->dmr_req);
	dix_meta_req_fini(req);
	m0_dix_req_unlock(&req->dmr_req);
}

M0_INTERNAL void m0_dix_meta_lock(struct m0_dix_meta_req *req)
{
	m0_dix_req_lock(&req->dmr_req);
}

M0_INTERNAL void m0_dix_meta_unlock(struct m0_dix_meta_req *req)
{
	m0_dix_req_unlock(&req->dmr_req);
}

M0_INTERNAL int m0_dix_meta_generic_rc(const struct m0_dix_meta_req *req)
{
	return M0_RC(m0_dix_generic_rc(&req->dmr_req));
}

M0_INTERNAL int m0_dix_meta_item_rc(const struct m0_dix_meta_req *req,
				    uint64_t                      idx)
{
	M0_PRE(m0_dix_meta_generic_rc(req) == 0);
	return m0_dix_item_rc(&req->dmr_req, idx);
}

M0_INTERNAL int m0_dix_meta_req_nr(const struct m0_dix_meta_req *req)
{
	return m0_dix_req_nr(&req->dmr_req);
}

M0_INTERNAL int m0_dix_meta_create(struct m0_dix_cli   *cli,
				   struct m0_sm_group  *grp,
				   struct m0_dix_ldesc *dld_layout,
				   struct m0_dix_ldesc *dld_ldescr)
{
	int rc;

	M0_PRE(cli->dx_sm.sm_state == DIXCLI_BOOTSTRAP);
	rc = dix_meta_create(cli, grp, dld_layout, dld_ldescr);
	if (rc == 0) {
		rc = dix_root_put(cli, grp, dld_layout, dld_ldescr);
		if (rc != 0)
			dix_meta_delete(cli, grp, dld_layout, dld_ldescr);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_meta_check(struct m0_dix_cli  *cli,
				  struct m0_sm_group *grp,
				  bool               *result)
{
	struct m0_dix_req dreq;
	struct m0_dix     indices[DIX_META_INDICES_NR] = {};
	int               rc;

	M0_ENTRY();
	M0_PRE(result != NULL);
	M0_PRE(cli->dx_sm.sm_state == DIXCLI_READY);
	/*
	 * If DIX client is ready, then existence of 'layout' and 'layout-descr'
	 * indices was checked during client startup.
	 */
	rc = dix_meta_indices_init(indices, cli, NULL, NULL);
	if (rc != 0)
		return M0_ERR(rc);
	m0_dix_mreq_init(&dreq, cli, grp);
	m0_dix_req_lock(&dreq);
	rc = m0_dix_cctgs_lookup(&dreq, indices, ARRAY_SIZE(indices)) ?:
	     m0_dix_req_wait(&dreq, M0_BITS(DIXREQ_FINAL, DIXREQ_FAILURE),
			      M0_TIME_NEVER) ?:
	     dix_mreq_rc(&dreq);
	if (rc == -ENOENT) {
		rc = 0;
		*result = false;
	} else if (rc == 0) {
		*result = true;
	}
	m0_dix_req_unlock(&dreq);
	m0_dix_req_fini_lock(&dreq);
	dix_meta_indices_fini(indices);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_root_read(struct m0_dix_meta_req *req)
{
	int                rc;
	struct m0_bufvec  *keys = &req->dmr_keys;
	struct m0_dix_cli *cli = req->dmr_req.dr_cli;
	struct m0_dix      root = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	M0_PRE(M0_IN(cli->dx_sm.sm_state, (DIXCLI_READY,
					   DIXCLI_BOOTSTRAP,
					   DIXCLI_STARTING)));
	rc = m0_dix__root_set(cli, &root) ?:
	     m0_bufvec_empty_alloc(keys, 2);
	if (rc != 0) {
		m0_dix_fini(&root);
		return M0_ERR(rc);
	}
	rc = dix_root_add(keys, NULL, 0, "layout", NULL, NULL) ?:
	     dix_root_add(keys, NULL, 1, "layout-descr", NULL, NULL);
	if (rc != 0)
		goto err;

	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_get(&req->dmr_req, &root, &req->dmr_keys);
	if (rc != 0) {
		m0_clink_del(&req->dmr_clink);
		goto err;
	}
	m0_dix_fini(&root);
	return M0_RC(0);
err:
	m0_dix_fini(&root);
	m0_bufvec_free(keys);
	return M0_ERR(rc);
}

static int dix_layout_from_read_rep(struct m0_dix_meta_req *req,
				    uint64_t                idx,
				    const struct m0_fid    *expected,
				    struct m0_dix_ldesc    *out)
{
	struct m0_bufvec        vals;
	struct m0_dix_get_reply rep;
	struct m0_fid           fid;
	struct m0_dix_ldesc     ldesc = {};
	int                     rc;

	M0_PRE(m0_dix_generic_rc(&req->dmr_req) == 0);
	m0_dix_get_rep(&req->dmr_req, idx, &rep);
	if (rep.dgr_rc != 0)
		return M0_ERR(rep.dgr_rc);
	vals = M0_BUFVEC_INIT_BUF(&rep.dgr_val.b_addr, &rep.dgr_val.b_nob);
	rc = m0_dix__meta_val_dec(&vals, &fid, &ldesc, 1);
	if (rc == 0) {
		if (!m0_fid_eq(&fid, expected))
			rc = M0_ERR(-EPROTO);
		else
			rc = m0_dix_ldesc_copy(out, &ldesc);
		m0_dix_ldesc_fini(&ldesc);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_root_read_rep(struct m0_dix_meta_req *req,
				     struct m0_dix_ldesc    *layout,
				     struct m0_dix_ldesc    *ldescr)
{
	int rc;

	M0_ENTRY();
	/*
	 * Exactly two records are expected in reply: the one for 'layout' index
	 * and an another for 'layout-descr' index. The reply should contain
	 * records in exactly this order ('layout' index first).
	 */
	if (m0_dix_req_nr(&req->dmr_req) != 2)
		return M0_ERR(-EPROTO);
	rc = dix_layout_from_read_rep(req, 0, &m0_dix_layout_fid, layout);
	if (rc == 0) {
		rc = dix_layout_from_read_rep(req, 1, &m0_dix_ldescr_fid,
					      ldescr);
		if (rc != 0)
			m0_dix_ldesc_fini(layout);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_meta_destroy(struct m0_dix_cli  *cli,
				    struct m0_sm_group *grp)
{
	int rc;

	M0_ENTRY();
	M0_PRE(cli->dx_sm.sm_state == DIXCLI_READY);
	rc = dix_meta_delete(cli, grp, NULL, NULL);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_ldescr_put(struct m0_dix_meta_req    *req,
				  const uint64_t            *lid,
				  const struct m0_dix_ldesc *ldesc,
				  uint32_t                   nr)
{
	int           rc;
	struct m0_dix index = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	M0_PRE(ldesc != NULL);
	M0_PRE(lid != NULL);
	rc = m0_dix__ldescr_set(meta_req_cli(req), &index) ?:
	     m0_dix__ldesc_vals_enc(lid, ldesc, nr,
				    &req->dmr_keys, &req->dmr_vals);
	if (rc != 0) {
		m0_dix_fini(&index);
		return M0_ERR(rc);
	}
	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_put(&req->dmr_req, &index, &req->dmr_keys, &req->dmr_vals,
			NULL, 0);
	if (rc != 0)
		m0_clink_del(&req->dmr_clink);
	m0_dix_fini(&index);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_ldescr_get(struct m0_dix_meta_req *req,
				  const uint64_t         *lid,
				  uint32_t                nr)
{
	int           rc;
	struct m0_dix index = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	M0_PRE(lid != NULL);
	rc = m0_dix__ldescr_set(meta_req_cli(req), &index) ?:
	     m0_dix__ldesc_vals_enc(lid, NULL, nr, &req->dmr_keys, NULL);
	if (rc != 0) {
		m0_dix_fini(&index);
		return M0_ERR(rc);
	}
	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_get(&req->dmr_req, &index, &req->dmr_keys);
	if (rc != 0)
		m0_clink_del(&req->dmr_clink);
	m0_dix_fini(&index);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_ldescr_rep_get(struct m0_dix_meta_req *req,
				      uint64_t                idx,
				      struct m0_dix_ldesc    *ldesc)
{
	int                     rc;
	struct m0_dix_get_reply rep;
	struct m0_bufvec        vals;

	M0_ENTRY();
	M0_PRE(m0_dix_meta_generic_rc(req) == 0);
	m0_dix_get_rep(&req->dmr_req, idx, &rep);
	rc = rep.dgr_rc;
	if (rc == 0) {
		vals = M0_BUFVEC_INIT_BUF(&rep.dgr_val.b_addr,
					  &rep.dgr_val.b_nob);
		rc = m0_dix__ldesc_vals_dec(NULL, &vals, NULL, ldesc, 1);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_ldescr_del(struct m0_dix_meta_req *req,
				 const uint64_t         *lid,
				 uint32_t                nr)
{
	int           rc;
	struct m0_dix index = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	M0_PRE(lid != NULL);
	rc = m0_dix__ldescr_set(meta_req_cli(req), &index) ?:
	     m0_dix__ldesc_vals_enc(lid, NULL, nr, &req->dmr_keys, NULL);
	if (rc != 0) {
		m0_dix_fini(&index);
		return M0_ERR(rc);
	}
	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_del(&req->dmr_req, &index, &req->dmr_keys, NULL, 0);
	if (rc != 0)
		m0_clink_del(&req->dmr_clink);
	m0_dix_fini(&index);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_layout_put(struct m0_dix_meta_req     *req,
				  const struct m0_fid        *fid,
				  const struct m0_dix_layout *dlay,
				  uint32_t                    nr,
				  uint32_t                    flags)
{
	int           rc;
	struct m0_dix index = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	M0_PRE(fid != NULL);
	M0_PRE(dlay != NULL);
	rc = m0_dix__layout_set(meta_req_cli(req), &index) ?:
	     m0_dix__layout_vals_enc(fid, dlay, nr,
				     &req->dmr_keys, &req->dmr_vals);
	if (rc != 0) {
		m0_dix_fini(&index);
		return M0_ERR(rc);
	}
	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_put(&req->dmr_req, &index, &req->dmr_keys, &req->dmr_vals,
			NULL, flags);
	if (rc != 0)
		m0_clink_del(&req->dmr_clink);
	m0_dix_fini(&index);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_layout_del(struct m0_dix_meta_req *req,
				  const struct m0_fid    *fid,
				  uint32_t                nr)
{
	int           rc;
	struct m0_dix index = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	rc = m0_dix__layout_set(meta_req_cli(req), &index) ?:
	     m0_dix__layout_vals_enc(fid, NULL, nr, &req->dmr_keys, NULL);
	if (rc != 0) {
		m0_dix_fini(&index);
		return M0_ERR(rc);
	}
	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_del(&req->dmr_req, &index, &req->dmr_keys, NULL, 0);
	if (rc != 0)
		m0_clink_del(&req->dmr_clink);
	m0_dix_fini(&index);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_layout_get(struct m0_dix_meta_req *req,
				  const struct m0_fid    *fid,
				  uint32_t                nr)
{
	int           rc;
	struct m0_dix index = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	M0_PRE(fid != NULL);
	rc = m0_dix__layout_set(meta_req_cli(req), &index) ?:
	     m0_dix__layout_vals_enc(fid, NULL, nr, &req->dmr_keys, NULL);
	if (rc != 0) {
		m0_dix_fini(&index);
		return M0_ERR(rc);
	}
	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_get(&req->dmr_req, &index, &req->dmr_keys);
	if (rc != 0)
		m0_clink_del(&req->dmr_clink);
	m0_dix_fini(&index);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_layout_rep_get(struct m0_dix_meta_req *req,
				      uint64_t                idx,
				      struct m0_dix_layout   *dlay)
{
	int                     rc;
	struct m0_dix_get_reply rep;
	struct m0_bufvec        vals;

	M0_ENTRY();
	m0_dix_get_rep(&req->dmr_req, idx, &rep);
	rc = rep.dgr_rc;
	if (rc == 0 && dlay != NULL) {
		vals = M0_BUFVEC_INIT_BUF(&rep.dgr_val.b_addr,
					  &rep.dgr_val.b_nob);
		rc = m0_dix__layout_vals_dec(NULL, &vals, NULL, dlay, 1);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_index_list(struct m0_dix_meta_req *req,
				  const struct m0_fid    *start_fid,
				  uint32_t                indices_nr)
{
	int               rc;
	struct m0_bufvec *keys = &req->dmr_keys;
	uint32_t          keys_nr = indices_nr;
	struct m0_dix     index = {};

	M0_ENTRY();
	M0_PRE(req != NULL);
	M0_PRE(start_fid != NULL);
	M0_PRE(indices_nr != 0);
	rc = m0_dix__layout_set(meta_req_cli(req), &index) ?:
	     m0_dix__layout_vals_enc(start_fid, NULL, 1, keys, NULL);
	if (rc != 0) {
		m0_dix_fini(&index);
		return M0_ERR(rc);
	}

	m0_clink_add(&req->dmr_req.dr_sm.sm_chan, &req->dmr_clink);
	rc = m0_dix_next(&req->dmr_req, &index, &req->dmr_keys, &keys_nr, 0);
	if (rc != 0) {
		m0_clink_del(&req->dmr_clink);
		m0_bufvec_free(keys);
	}
	m0_dix_fini(&index);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_index_list_rep_nr(struct m0_dix_meta_req *req)
{
	return m0_dix_next_rep_nr(&req->dmr_req, 0);
}

M0_INTERNAL int m0_dix_index_list_rep(struct m0_dix_meta_req *req,
				      uint32_t                idx,
				      struct m0_fid          *fid)
{
	int                      rc;
	struct m0_bufvec         index;
	struct m0_dix_next_reply rep;

	M0_ENTRY();
	m0_dix_next_rep(&req->dmr_req, 0, idx, &rep);
	index = M0_BUFVEC_INIT_BUF(&rep.dnr_key.b_addr,
				   &rep.dnr_key.b_nob);
	rc = m0_dix__layout_vals_dec(&index, NULL, fid, NULL, 1);
	return M0_RC(rc);
}
#undef DFID
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
