/* -*- C -*- */
/*
 * COPYRIGHT 2018 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 16-Jan-2019
 */

/**
 * @addtogroup m0d
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D
#include "lib/trace.h"

#include "cas/client.h"
#include "conf/confc.h"         /* m0_confc_close */
#include "conf/helpers.h"       /* m0_confc_root_open */
#include "dix/req.h"            /* m0_dix */
#include "dix/fid_convert.h"    /* m0_dix_fid_convert_dix2cctg */
#include "dix/meta.h"           /* m0_dix_root_fid */
#include "lib/buf.h"
#include "lib/locality.h"       /* m0_locality0_get */
#include "lib/misc.h"           /* M0_SET */
#include "lib/vec.h"            /* m0_bufvec_empty_alloc */
#include "mero/setup.h"         /* m0_mero */
#include "mero/setup_internal.h"/* cs_service_init */
#include "pool/pool.h"          /* m0_pools_common, m0_pool_version */
#include "reqh/reqh.h"          /* m0_reqh2confc */
#include "rpc/link.h"           /* m0_rpc_link */
#include "sm/sm.h"              /* m0_sm_timedwait */

static void cs_dix_cas_id_make(struct m0_cas_id *cid,
			       struct m0_dix    *index,
			       uint32_t          sdev_idx)
{
	struct m0_fid cctg_fid;
	int           rc;

	m0_dix_fid_convert_dix2cctg(&index->dd_fid, &cctg_fid, sdev_idx);
	cid->ci_fid = cctg_fid;
	M0_ASSERT(index->dd_layout.dl_type == DIX_LTYPE_DESCR);
	cid->ci_layout.dl_type = index->dd_layout.dl_type;
	rc = m0_dix_ldesc_copy(&cid->ci_layout.u.dl_desc,
			       &index->dd_layout.u.dl_desc);
	M0_ASSERT(rc == 0); /* XXX */
}

static int cs_dix_create_sync(struct m0_dix      *index,
			      uint32_t            sdev_idx,
			      struct m0_rpc_link *link)
{
	struct m0_cas_req req;
	struct m0_cas_id  cid;
	int               rc;

	M0_ENTRY("sdev_idx=%"PRIu32, sdev_idx);

	M0_SET0(&cid);
	cs_dix_cas_id_make(&cid, index, sdev_idx);

	M0_SET0(&req);
	m0_cas_req_init(&req, &link->rlk_sess,
			m0_locality0_get()->lo_grp /* XXX */);

	/* XXX Locks sm group of locality0 */
	m0_cas_req_lock(&req);
	rc = m0_cas_index_create(&req, &cid, 1, NULL /* XXX */);
	M0_ASSERT(rc == 0);
	rc = m0_sm_timedwait(&req.ccr_sm, M0_BITS(CASREQ_FINAL, CASREQ_FAILURE),
			     M0_TIME_NEVER);
	M0_ASSERT(M0_IN(rc, (0, -ESRCH)));
	m0_cas_req_unlock(&req);

	rc = m0_cas_req_generic_rc(&req);
	M0_LOG(M0_DEBUG, "m0_cas_index_create() finished with rc=%d", rc);

	m0_cas_req_fini_lock(&req);
	m0_cas_id_fini(&cid);

	return M0_RC(rc);
}

static int cs_dix_keys_vals_init(struct m0_bufvec    *keys,
				 struct m0_bufvec    *vals,
				 const char          *key,
				 const struct m0_fid *fid,
				 struct m0_dix_ldesc *dld)
{
	m0_bcount_t klen;
	int         rc;

	rc = m0_bufvec_empty_alloc(keys, 1) ?:
	     m0_bufvec_empty_alloc(vals, 1);
	M0_ASSERT(rc == 0);

	rc = m0_dix__meta_val_enc(fid, dld, 1, vals);
	M0_ASSERT(rc == 0);

	klen = strlen(key);
	keys->ov_buf[0] = m0_alloc(klen);
	M0_ASSERT(keys->ov_buf[0] != NULL);
	keys->ov_vec.v_count[0] = klen;
	memcpy(keys->ov_buf[0], key, klen);

	return M0_RC(rc);
}

static void cs_dix_keys_vals_fini(struct m0_bufvec *keys,
				  struct m0_bufvec *vals)
{
	m0_bufvec_free(keys);
	m0_bufvec_free(vals);
}

static int cs_dix_put_sync(struct m0_dix      *index,
			   uint32_t            sdev_idx,
			   struct m0_bufvec   *keys,
			   struct m0_bufvec   *vals,
			   struct m0_rpc_link *link)
{
	struct m0_cas_req req;
	struct m0_cas_id  cid;
	int               rc;

	M0_ENTRY("sdev_idx=%"PRIu32, sdev_idx);

	M0_SET0(&cid);
	cs_dix_cas_id_make(&cid, index, sdev_idx);

	M0_SET0(&req);
	m0_cas_req_init(&req, &link->rlk_sess,
			m0_locality0_get()->lo_grp /* XXX */);

	m0_cas_req_lock(&req);
	rc = m0_cas_put(&req, &cid, keys, vals, NULL /* XXX */, 0);
	if (rc == 0) {
		rc = m0_sm_timedwait(&req.ccr_sm, M0_BITS(CASREQ_FINAL,
							  CASREQ_FAILURE),
				     M0_TIME_NEVER);
		M0_ASSERT(M0_IN(rc, (0, -ESRCH)));
	}
	m0_cas_req_unlock(&req);

	rc = rc ?: m0_cas_req_generic_rc(&req);
	M0_LOG(M0_DEBUG, "m0_cas_put() for sdev_idx=%"PRIu32" finished with "
			 "rc=%d", sdev_idx, rc);

	m0_cas_req_fini_lock(&req);
	m0_cas_id_fini(&cid);

	return M0_RC(rc);
}

static int cs_dix_put_one(struct m0_dix           *index,
			  const char              *keystr,
			  const struct m0_fid     *fid,
			  struct m0_dix_ldesc     *dld,
			  struct m0_pools_common  *pc,
			  struct m0_pool_version  *pver,
			  struct m0_layout_domain *ldom,
			  struct m0_fid           *cas_fid,
			  struct m0_rpc_link      *link)
{
	struct m0_reqh_service_ctx *cas_svc;
	struct m0_dix_layout_iter   iter;
	struct m0_pooldev          *sdev;
	struct m0_bufvec            keys;
	struct m0_bufvec            vals;
	struct m0_buf               key;
	uint64_t                    tgt;
	uint32_t                    sdev_idx;
	uint32_t                    iter_max;
	uint32_t                    i;
	bool                        is_spare;
	int rc;

	rc = cs_dix_keys_vals_init(&keys, &vals, keystr, fid, dld);
	M0_ASSERT(rc == 0);
	key = M0_BUF_INIT(keys.ov_vec.v_count[0], keys.ov_buf[0]);

	M0_SET0(&iter);
	rc = m0_dix_layout_iter_init(&iter, &index->dd_fid, ldom, pver,
				     &index->dd_layout.u.dl_desc, &key);
	M0_ASSERT(rc == 0);
	iter_max = pver->pv_attr.pa_N + 2 * pver->pv_attr.pa_K;
	m0_dix_layout_iter_reset(&iter);
	for (i = 0; i < iter_max; ++i) {
		is_spare = m0_dix_liter_unit_classify(&iter,
					iter.dit_unit) == M0_PUT_SPARE;
		m0_dix_layout_iter_next(&iter, &tgt);
		M0_ASSERT(tgt < pver->pv_attr.pa_P);
		M0_LOG(M0_DEBUG, "tgt=%"PRIu64" is_spare=%d", tgt, !!is_spare);
		if (is_spare)
			continue;

		sdev = &pver->pv_mach.pm_state->pst_devices_array[tgt];
		sdev_idx = sdev->pd_sdev_idx;
		cas_svc = pc->pc_dev2svc[sdev_idx].pds_ctx;
		M0_ASSERT(cas_svc->sc_type == M0_CST_CAS);
		if (!m0_fid_eq(cas_fid, &cas_svc->sc_fid))
			continue;

		rc = cs_dix_put_sync(index, sdev_idx, &keys, &vals, link);
		M0_ASSERT(rc == 0);
	}

	m0_dix_layout_iter_fini(&iter);
	cs_dix_keys_vals_fini(&keys, &vals);

	return M0_RC(rc);
}

static bool cs_dix_pver_is_valid(struct m0_pools_common *pc,
				 struct m0_pool_version *pver)
{
	struct m0_reqh_service_ctx  *cas_svc;
	struct m0_poolmach          *pm = &pver->pv_mach;
	struct m0_pooldev           *sdev;
	uint32_t                     sdev_idx;
	uint32_t                     i;

	/* `pm' is locked in m0_cs_dix_setup(). */
	for (i = 0; i < pm->pm_state->pst_nr_devices; ++i) {
		sdev = &pm->pm_state->pst_devices_array[i];
		sdev_idx = sdev->pd_sdev_idx;
		cas_svc = pc->pc_dev2svc[sdev_idx].pds_ctx;
		if (cas_svc->sc_type != M0_CST_CAS) {
			return false;
		}
	}
	return true;
}

/*
 * XXX Current implementation doesn't support multiple DIX pools.
 * rt_imeta_pver is used for all 3 meta indices.
 */
M0_INTERNAL int m0_cs_dix_setup(struct m0_mero *cctx)
{
	struct m0_pools_common      *pc    = &cctx->cc_pools_common;
	struct m0_reqh_context      *rctx  = &cctx->cc_reqh_ctx;
	struct m0_rpc_machine       *rmach = m0_mero_to_rmach(cctx);
	struct m0_rpc_machine       *rmach_save;
	const char                  *ep;
	struct m0_reqh_service_type *stype;
	struct m0_reqh_service      *service;
	struct m0_reqh_service_ctx  *cas_svc;
	struct m0_fid               *cas_fid;
	struct m0_rpc_link           link;
	int                          rc;

	struct m0_pool_version      *pver;
	struct m0_poolmach          *pm;
	struct m0_conf_root         *root;
	struct m0_pooldev           *sdev;
	struct m0_fid                root_pver_fid;
	uint32_t                     sdev_idx;
	uint32_t                     i;

	struct m0_dix                index_root;
	struct m0_dix                index1;
	struct m0_dix                index2;
	struct m0_dix_ldesc          dld_root;
	struct m0_dix_ldesc          dlds[2];
	struct m0_dix_ldesc         *dld1 = &dlds[0];
	struct m0_dix_ldesc         *dld2 = &dlds[1];

	struct m0_ext                range[] = {
		{ .e_start = 0, .e_end = IMASK_INF },
	};

	const char *layout_key = "layout";
	const char *ldescr_key = "layout-descr";

	enum { MAX_RPCS_IN_FLIGHT = 2 }; /* XXX */

	M0_ENTRY();

	M0_PRE(cctx->cc_mkfs);
	M0_PRE(rmach != NULL);

	ep = rmach->rm_tm.ntm_ep->nep_addr;

	/* Skip if CAS isn't configured */

	if (rctx->rc_services[M0_CST_CAS] == NULL)
		return M0_RC(0);

	/* Start CAS/FDMI services */

	M0_PRE(rctx->rc_services[M0_CST_CAS] != NULL);

	rc = cs_service_init(rctx->rc_services[M0_CST_CAS], rctx, &rctx->rc_reqh,
			     &rctx->rc_service_fids[M0_CST_CAS]);
	M0_ASSERT(rc == 0);
	M0_LOG(M0_DEBUG, "service: %s" FID_F " cs_service_init: %d",
	       rctx->rc_services[M0_CST_CAS],
	       FID_P(&rctx->rc_service_fids[M0_CST_CAS]), rc);

	/* Start FDMI service even if it's missed in configuration */
	rc = cs_service_init("M0_CST_FDMI", rctx, &rctx->rc_reqh, NULL);
	M0_ASSERT(rc == 0);

	cas_fid = &rctx->rc_service_fids[M0_CST_CAS];

	/* Create service ctxs in pools_common */

	/* XXX hack not to connect to services */
	rmach_save = pc->pc_rmach;
	pc->pc_rmach = NULL;
	rc = m0_pools_service_ctx_create(pc);
	M0_ASSERT(rc == 0);
	pc->pc_rmach = rmach_save;

	rc = m0_pool_versions_setup(pc);
	M0_ASSERT(rc == 0);

	/* Get root pver */

	rc = m0_confc_root_open(m0_reqh2confc(&cctx->cc_reqh_ctx.rc_reqh),
				&root);
	M0_ASSERT(rc == 0);
	root_pver_fid = root->rt_imeta_pver;
	m0_confc_close(&root->rt_obj);

	pver = m0_pool_version_find(&cctx->cc_pools_common, &root_pver_fid);
	M0_ASSERT(pver != NULL);
	pm = &pver->pv_mach;
	m0_rwlock_read_lock(&pm->pm_lock);
	M0_ASSERT(!pver->pv_is_dirty);
	M0_ASSERT(pool_failed_devs_tlist_is_empty(
					&pver->pv_pool->po_failed_devices));

	/* Purpose of this check is to skip UTs with incorrect DIX root pver. */
	if (!cs_dix_pver_is_valid(pc, pver)) {
		M0_LOG(M0_DEBUG, "DIX pver is not valid, skipping DIX init");
		goto exit_fini_pools;
	}

	M0_LOG(M0_DEBUG, "Root pver: P=%"PRIu32" N=%"PRIu32" K=%"PRIu32,
	       pver->pv_attr.pa_P, pver->pv_attr.pa_N, pver->pv_attr.pa_K);

	/* Connect to itself to send CAS fop */

	M0_SET0(&link);
	rc = m0_rpc_link_init(&link, rmach, NULL, ep, MAX_RPCS_IN_FLIGHT);
	M0_ASSERT(rc == 0);
	rc = m0_rpc_link_connect_sync(&link, M0_TIME_NEVER);
	M0_ASSERT(rc == 0);

	/* Init m0_dix objects */

	M0_SET0(&dld_root);
	m0_dix_ldesc_init(&dld_root, range, ARRAY_SIZE(range),
			  HASH_FNC_FNV1, &root_pver_fid);
	M0_SET0(&index_root);
	index_root.dd_fid = m0_dix_root_fid;
	rc = m0_dix_desc_set(&index_root, &dld_root);
	M0_ASSERT(rc == 0);

	M0_SET0(dld1);
	m0_dix_ldesc_init(dld1, range, ARRAY_SIZE(range),
			  HASH_FNC_CITY, &root_pver_fid);
	M0_SET0(&index1);
	index1.dd_fid = m0_dix_layout_fid;
	rc = m0_dix_desc_set(&index1, dld1);
	M0_ASSERT(rc == 0);

	M0_SET0(dld2);
	m0_dix_ldesc_init(dld2, range, ARRAY_SIZE(range),
			  HASH_FNC_CITY, &root_pver_fid);
	M0_SET0(&index2);
	index2.dd_fid = m0_dix_ldescr_fid;
	rc = m0_dix_desc_set(&index2, dld2);
	M0_ASSERT(rc == 0);

	/* Create meta indices */

	/*
	 * Emulate behaviour of dix_idxop_req_send()
	 * and send reqs for all sdevs
	 */
	for (i = 0; i < pm->pm_state->pst_nr_devices; ++i) {
		sdev = &pm->pm_state->pst_devices_array[i];
		M0_ASSERT(M0_IN(sdev->pd_state, (M0_PNDS_ONLINE,
						 M0_PNDS_SNS_REBALANCING)));
		sdev_idx = sdev->pd_sdev_idx;
		cas_svc = pc->pc_dev2svc[sdev_idx].pds_ctx;
		M0_ASSERT(cas_svc->sc_type == M0_CST_CAS);
		if (!m0_fid_eq(cas_fid, &cas_svc->sc_fid))
			continue;

		rc = cs_dix_create_sync(&index_root, sdev_idx, &link);
		M0_ASSERT(rc == 0);
		rc = cs_dix_create_sync(&index1, sdev_idx, &link);
		M0_ASSERT(rc == 0);
		rc = cs_dix_create_sync(&index2, sdev_idx, &link);
		M0_ASSERT(rc == 0);
	}

	/* PUT layout */

	rc = cs_dix_put_one(&index_root, layout_key, &m0_dix_layout_fid,
			    dld1, pc, pver, &rctx->rc_reqh.rh_ldom,
			    cas_fid, &link);
	M0_ASSERT(rc == 0);

	/* PUT ldescr */

	rc = cs_dix_put_one(&index_root, ldescr_key, &m0_dix_ldescr_fid,
			    dld2, pc, pver, &rctx->rc_reqh.rh_ldom,
			    cas_fid, &link);
	M0_ASSERT(rc == 0);

	/* Fini */

	m0_dix_fini(&index2);
	m0_dix_fini(&index1);
	m0_dix_fini(&index_root);
	m0_dix_ldesc_fini(dld2);
	m0_dix_ldesc_fini(dld1);
	m0_dix_ldesc_fini(&dld_root);

	/* Disconnect */

	rc = m0_rpc_link_disconnect_sync(&link, M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	m0_rpc_link_fini(&link);

	/* Destroy service ctxs in pools_common */

exit_fini_pools:
	m0_rwlock_read_unlock(&pm->pm_lock);
	if (rctx->rc_state == RC_INITIALISED)
		m0_reqh_layouts_cleanup(&rctx->rc_reqh);
	m0_pool_versions_destroy(pc);
	m0_pools_service_ctx_destroy(pc);

	/* Stop CAS/FDMI services */

	service = m0_reqh_service_lookup(&rctx->rc_reqh, cas_fid);
	M0_ASSERT(service != NULL);
	cs_service_fini(service);

	stype = m0_reqh_service_type_find("M0_CST_FDMI");
	M0_ASSERT(stype != NULL);
	service = m0_reqh_service_find(stype, &rctx->rc_reqh);
	M0_ASSERT(service != NULL);
	cs_service_fini(service);

	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of m0d group */

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
