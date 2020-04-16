/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 07/15/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_POOL
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/assert.h"
#include "lib/hash.h"      /* m0_hash */
#include "lib/string.h"    /* m0_streq */
#include "conf/confc.h"    /* m0_confc_from_obj */
#include "conf/schema.h"   /* M0_CST_IOS, M0_CST_MDS */
#include "conf/diter.h"    /* m0_conf_diter_next_sync */
#include "conf/obj_ops.h"  /* M0_CONF_DIRNEXT */
#include "conf/pvers.h"    /* m0_conf_pver_find_by_fid */
#include "conf/cache.h"    /* m0_conf_cache_contains */
#include "conf/helpers.h"  /* m0_confc_root_open() */
#include "reqh/reqh_service.h" /* m0_reqh_service_ctx */
#include "reqh/reqh.h"
#include "rpc/rpc_machine.h"    /* m0_rpc_machine_ep */
#include "ha/entrypoint.h"      /* m0_ha_entrypoint_client */
#include "ha/ha.h"              /* m0_ha */
#include "module/instance.h"    /* m0 */
#include "pool/pool.h"
#include "pool/pool_fops.h"
#include "fd/fd.h"              /* m0_fd_tile_build, m0_fd_tree_build */
#ifndef __KERNEL__
#  include "mero/setup.h"
#else
#  include "m0t1fs/linux_kernel/m0t1fs.h"  /* m0t1fs_sb */
#endif
#include "lib/finject.h" /* M0_FI_ENABLED */

/**
   @page pool_mach DLD of Pool Machine

   - @ref pool_mach-ovw
   - @ref pool_mach-def
   - @ref pool_mach-req
   - @ref pool_mach-depends
   - @ref pool_mach-fspec
   - @ref pool_mach-lspec
      - @ref pool_mach-lspec-comps
      - @ref pool_mach-lspec-ds
   - @ref pool_mach-conformance
   - @ref pool_mach-ut
   - @ref pool_mach-st
   - @ref pool_mach-O
   - @ref pool_mach-ref

   <hr>
   @section pool_mach-ovw Overview
   When system initializes the first time, pool machine state is configured
   from confc & confd. When system restarts, ioservice/clients rebuld pool
   machine state on HA system events for respective pool devices.

   Every pool version has its own pool machine state stored in memory and it
   rebuild using HA system events for respective pool devices on node restarts
   They are independent. Pool machine state transitions are triggered by pool
   machine events. Pool machine events, e.g. device failure event, SNS
   completion event, are delivered to all ioservices. So, these pool machine
   states are updated and the final pool machine states are identical on all
   nodes.

   <hr>
   @section pool_mach-def Definitions
   @see poolmach for failure vectors, events, etc.

   <hr>
   @section pool_mach-req Requirements
   The following requirements should be meet:
   - @b R.DLD.P All pool machine states should be rebuild using HA events.
                When system restarts again, these states are rebuilt from
                HA events for pool devices.
   <hr>
   @section pool_mach-depends Dependencies
   FOP.
   DTM.
   Pool.
   Pool Machine.

   <hr>
   @section pool_mach-fspec Functional Specification
   Pool Machine states, including failure vectors, event lists, spare slots
   usages, etc. are stored in memory and get rebuild using HA events for
   pool devices on node restarts.

   Every pool version has a pool machine. Pool machine update events are
   delivered to all ioservices. So all the pool machine there get state
   transitions according to these events.

   HA component broadcasts every event to all the pool machines in the cluster
   using ha note interface. HA components can also use m0poolmach utility to
   query the current status of pool machine.

   HA component or Administrator may trigger SNS repair and/or SNS rebalance
   operation using m0repair utility. HA component or Administrator decide
   when to trigger these operations upon various conditions, like failure
   type, the elapsed time from failure detected, etc.

   <hr>
   @section pool_mach-lspec Logical Specification

   - @ref pool_mach-lspec-comps
   - @ref pool_mach-lspec-ds
   - @ref pool_mach-lspec-if

   @subsection pool_mach-lspec-comps Component Overview
   Pool machine global information, like number of devices and number of nodes in
   this pool, current read and write version numbers are stored in in-memory data
   structures.

   Every pool machine contains an array of struct m0_pooldev. Every instance of
   struct m0_pooldev represents a device in the pool and maintains the
   corresponding device state. This device informatin is refreshed on
   configuration updates.  Similarly, the node in the pool is represented by
   struct m0_poolnode. This is also refreshed on configuration update.

   @subsection pool_mach-lspec-ds Data Structures
   The data structures of failure vector, failure vector version number,
   event, event list are in @ref poolmach module.

   The pool state, state of node, state of device, events are defined in code.

   @subsection pool_mach-lspec-if Interfaces
   The failure vector and version number operations are designed and listed
   in @ref poolmach.

   <hr>
   @section pool_mach-conformance Conformance
   - @b I.DLD.P All pool machine states are rebuild using HA event for pool
                devices on node restarts.

   <hr>
   @section pool_mach-ut Unit Tests
   Unit test will cover the following case:
   - init BE storage.
   - updates to BE.
   - load from BE.
   - Sending updates to replicas.
   - Replicas handles updates from master.

   <hr>
   @section pool_mach-st System Tests
   Pool machine and its replicas works well when new pool machine events happen.
   Pool machine works well when system re-starts.

   <hr>
   @section pool_mach-O Analysis
   N/A

   <hr>
   @section pool_mach-ref References
   - @ref cm
   - @ref agents
   - @ref poolmach
   - @ref DB
 */

/**
   @addtogroup pool

   @{
 */

enum {
	/**
	 * Status "Device not found in Pool machine"
	 */
	POOL_DEVICE_INDEX_INVALID = -1
};

/**
 * tlist descriptor for list of m0_reqh_service_ctx objects placed
 * in m0_pools_common::pc_svc_ctxs list using sc_link.
 */
M0_TL_DESCR_DEFINE(pools_common_svc_ctx, "Service contexts", M0_INTERNAL,
		   struct m0_reqh_service_ctx, sc_link, sc_magic,
		   M0_REQH_SVC_CTX_MAGIC, M0_POOL_SVC_CTX_HEAD_MAGIC);

M0_TL_DEFINE(pools_common_svc_ctx, M0_INTERNAL, struct m0_reqh_service_ctx);

/**
 * tlist descriptor for list of m0_pool objects placed
 * in m0t1fs_sb::csb_pools list using sc_link.
 */
M0_TL_DESCR_DEFINE(pools, "pools", M0_INTERNAL,
		   struct m0_pool, po_linkage, po_magic,
		   M0_POOL_MAGIC, M0_POOLS_HEAD_MAGIC);

M0_TL_DEFINE(pools, M0_INTERNAL, struct m0_pool);

M0_TL_DESCR_DEFINE(pool_version, "pool versions", M0_INTERNAL,
		   struct m0_pool_version, pv_linkage, pv_magic,
		   M0_POOL_VERSION_MAGIC, M0_POOL_VERSION_HEAD_MAGIC);

M0_TL_DEFINE(pool_version, M0_INTERNAL, struct m0_pool_version);

M0_TL_DESCR_DEFINE(pool_failed_devs, "pool failed devices", M0_INTERNAL,
		   struct m0_pooldev, pd_fail_linkage, pd_magic,
		   M0_POOL_DEV_MAGIC, M0_POOL_DEVICE_HEAD_MAGIC);
M0_TL_DEFINE(pool_failed_devs, M0_INTERNAL, struct m0_pooldev);

static const struct m0_bob_type pver_bob = {
	.bt_name         = "m0_pool_version",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_pool_version, pv_magic),
	.bt_magix        = M0_POOL_VERSION_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(static, &pver_bob, m0_pool_version);

static int pools_common_refresh_locked(struct m0_pools_common *pc);
static int pools_common__dev2ios_build(struct m0_pools_common *pc);
static int pool_version_get_locked(struct m0_pools_common  *pc,
				   const struct m0_fid     *pool,
				   struct m0_pool_version **pv);
static void pool_version__layouts_evict(struct m0_pool_version *pv,
					struct m0_layout_domain *ldom);

static struct m0_pool *pool_find(struct m0_pools_common *pc,
				 const struct m0_fid *pool)
{
	struct m0_pool *ret;

	M0_ENTRY("pool="FID_F, FID_P(pool));

	/*
	 * XXX TODO:
	 * Lock pools common before accessing/updating, at other places.
	 * Merge m0_pool_find() and pool_find().
	 */
	ret = m0_tl_find(pools, p, &pc->pc_pools, m0_fid_eq(&p->po_id, pool));
	M0_LEAVE("%sfound", ret == NULL ? "not " : "");
	return ret;
}

M0_INTERNAL struct m0_pool *m0_pool_find(struct m0_pools_common *pc,
					 const struct m0_fid *pool)
{
	/*
	 * XXX TODO:
	 * Enable this assert once the pools common locking is fixed in the
	 * pools common update paths.
	 */
	/*M0_PRE(m0_pools_is_locked(pc));*/

	return pool_find(pc, pool);
}

static void pool__layouts_evict(struct m0_pool *pool,
				struct m0_layout_domain *ldom);

M0_INTERNAL int m0_pools_init(void)
{

#ifndef __KERNEL__
	return m0_poolmach_fop_init();
#else
	return 0;
#endif
}

M0_INTERNAL void m0_pools_fini(void)
{
#ifndef __KERNEL__
	m0_poolmach_fop_fini();
#endif
}

M0_INTERNAL const char *m0_pool_dev_state_to_str(enum m0_pool_nd_state state)
{
	static const char *names[M0_PNDS_NR] = {
		[M0_PNDS_UNKNOWN] = "unknown",
		[M0_PNDS_ONLINE] = "online",
		[M0_PNDS_FAILED] = "failed",
		[M0_PNDS_OFFLINE] = "offline",
		[M0_PNDS_SNS_REPAIRING] = "repairing",
		[M0_PNDS_SNS_REPAIRED] = "repaired",
		[M0_PNDS_SNS_REBALANCING] = "rebalancing"
	};

	M0_PRE(IS_IN_ARRAY(state, names));
	M0_ASSERT(m0_forall(i, ARRAY_SIZE(names), names[i] != NULL));
	return names[state];
}

M0_INTERNAL int m0_pool_init(struct m0_pool *pool, const struct m0_fid *id,
			     enum m0_pver_policy_code pver_policy)
{
	struct m0_pver_policy_type *ppt;

	M0_ENTRY();
	pool->po_id = *id;
	pools_tlink_init(pool);
	pool_version_tlist_init(&pool->po_vers);
	pool_failed_devs_tlist_init(&pool->po_failed_devices);
	ppt = m0_pver_policy_type_find(pver_policy);
	M0_ASSERT(ppt != NULL);
	return M0_RC(ppt->ppt_ops->ppto_create(&pool->po_pver_policy));
}

M0_INTERNAL void m0_pool_fini(struct m0_pool *pool)
{
	pools_tlink_fini(pool);
	pool_version_tlist_fini(&pool->po_vers);
	pool_failed_devs_tlist_fini(&pool->po_failed_devices);
	pool->po_pver_policy->pp_ops->ppo_fini(pool->po_pver_policy);
}

static bool pools_common_invariant(const struct m0_pools_common *pc)
{
	return _0C(pc != NULL) && _0C(pc->pc_confc != NULL);
}

static bool pool_version_invariant(const struct m0_pool_version *pv)
{
	return _0C(pv != NULL) && _0C(m0_pool_version_bob_check(pv)) &&
	       _0C(m0_fid_is_set(&pv->pv_id)) && _0C(pv->pv_pool != NULL);
}

static bool obj_is_service(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE;
}

static bool is_mds(const struct m0_conf_obj *obj)
{
	return obj_is_service(obj) &&
	       M0_CONF_CAST(obj, m0_conf_service)->cs_type == M0_CST_MDS;
}

/**
 * Fills pc->pc_mds_map[] with service contexts (m0_reqh_service_ctx) of
 * the meta-data services run on the node associated with `ctrl`.
 */
static int mds_map_fill(struct m0_pools_common *pc,
			const struct m0_conf_controller *ctrl)
{
	struct m0_conf_diter        it;
	struct m0_reqh_service_ctx *ctx;
	struct m0_conf_service     *svc;
	uint64_t                    idx = 0;
	int                         rc;

	M0_ENTRY("ctrl="FID_F" node="FID_F, FID_P(&ctrl->cc_obj.co_id),
		 FID_P(&ctrl->cc_node->cn_obj.co_id));

	rc = m0_conf_diter_init(&it, pc->pc_confc, &ctrl->cc_node->cn_obj,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		return M0_ERR(rc);

	while ((rc = m0_conf_diter_next_sync(&it, is_mds)) == M0_CONF_DIRNEXT) {
		svc = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_service);
		ctx = m0_tl_find(pools_common_svc_ctx, ctx, &pc->pc_svc_ctxs,
				 m0_fid_eq(&svc->cs_obj.co_id, &ctx->sc_fid));
		pc->pc_mds_map[idx++] = ctx;
		M0_LOG(M0_DEBUG, "mds index:%"PRIu64", no. of mds:%"PRIu64,
		       idx, pc->pc_nr_svcs[M0_CST_MDS]);
		M0_ASSERT(idx <= pc->pc_nr_svcs[M0_CST_MDS]);
	}
	m0_conf_diter_fini(&it);
	return M0_RC(rc);
}

static bool obj_is_controllerv(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE &&
	       m0_conf_obj_type(M0_CONF_CAST(obj, m0_conf_objv)->cv_real) ==
	       &M0_CONF_CONTROLLER_TYPE;
}

static int pool_mds_map_init(struct m0_pools_common *pc)
{
	struct m0_conf_obj   *mdpool;
	struct m0_conf_objv  *objv;
	struct m0_conf_root  *root;
	struct m0_conf_diter  it;
	int                   rc;

	M0_ENTRY();
	M0_PRE(!pools_common_svc_ctx_tlist_is_empty(&pc->pc_svc_ctxs));
	M0_PRE(pc->pc_mds_map != NULL);

	root = M0_CONF_CAST(pc->pc_confc->cc_root, m0_conf_root);
	rc = m0_confc_open_sync(&mdpool, &root->rt_obj,
				M0_CONF_ROOT_POOLS_FID, root->rt_mdpool);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_conf_diter_init(&it, pc->pc_confc, mdpool,
				M0_CONF_POOL_PVERS_FID,
				M0_CONF_PVER_SITEVS_FID,
				M0_CONF_SITEV_RACKVS_FID,
				M0_CONF_RACKV_ENCLVS_FID,
				M0_CONF_ENCLV_CTRLVS_FID);
	if (rc != 0)
		goto end;

	while ((rc = m0_conf_diter_next_sync(&it, obj_is_controllerv)) ==
		M0_CONF_DIRNEXT) {
		objv = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_objv);
		/*
		 * XXX BUG: mds_map_fill() overwrites pc->pc_mds_map[].
		 *
		 * The bug will bite on cluster configurations that have
		 * several controller-v objects in the MD pool subtree.
		 */
		rc = mds_map_fill(pc, M0_CONF_CAST(objv->cv_real,
						   m0_conf_controller));
		if (rc != 0)
			break;
	}
	m0_conf_diter_fini(&it);
end:
	m0_confc_close(mdpool);
	return M0_RC(rc);
}

static bool obj_is_ios_cas_diskv(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE &&
	       m0_disk_is_of_type(M0_CONF_CAST(obj, m0_conf_objv)->cv_real,
				  M0_BITS(M0_CST_IOS, M0_CST_CAS));
}

M0_INTERNAL int m0_pool_version_device_map_init(struct m0_pool_version *pv,
						struct m0_conf_pver *pver,
						struct m0_pools_common *pc)
{
	struct m0_conf_diter              it;
	struct m0_conf_drive             *disk;
	struct m0_conf_sdev              *sdev;
	struct m0_conf_service           *svc;
	struct m0_reqh_service_ctx       *ctx;
	struct m0_pool_device_to_service *dev;
	uint32_t                          nr_sdevs = 0;
	int                               rc;

	M0_ENTRY();
	M0_PRE(pc != NULL && pc->pc_dev2svc != NULL);
	M0_PRE(!pools_common_svc_ctx_tlist_is_empty(&pc->pc_svc_ctxs));

	rc = m0_conf_diter_init(&it, pc->pc_confc, &pver->pv_obj,
				M0_CONF_PVER_SITEVS_FID,
				M0_CONF_SITEV_RACKVS_FID,
				M0_CONF_RACKV_ENCLVS_FID,
				M0_CONF_ENCLV_CTRLVS_FID,
				M0_CONF_CTRLV_DRIVEVS_FID);
	if (rc != 0)
		return M0_ERR(rc);
	/*
	 * XXX TODO: Replace m0_conf_diter_next_sync() with
	 * m0_conf_diter_next().
	 */
	while ((rc = m0_conf_diter_next_sync(&it, obj_is_ios_cas_diskv)) ==
	       M0_CONF_DIRNEXT) {
		/*
		 * Assign helper pointers.
		 */
		disk = M0_CONF_CAST(M0_CONF_CAST(m0_conf_diter_result(&it),
						 m0_conf_objv)->cv_real,
				    m0_conf_drive);
		sdev = disk->ck_sdev;
		svc = M0_CONF_CAST(m0_conf_obj_grandparent(&sdev->sd_obj),
				   m0_conf_service);
		/*
		 * Find a m0_reqh_service_ctx that corresponds to `svc'.
		 */
		ctx = m0_tl_find(pools_common_svc_ctx, ctx, &pc->pc_svc_ctxs,
				 m0_fid_eq(&ctx->sc_fid, &svc->cs_obj.co_id) &&
				 ctx->sc_type == svc->cs_type);
		M0_ASSERT(ctx != NULL);

		M0_LOG(M0_DEBUG, "dev_idx=%d service="FID_F" disk="FID_F
		       " sdev_fid="FID_F, sdev->sd_dev_idx,
		       FID_P(&svc->cs_obj.co_id), FID_P(&disk->ck_obj.co_id),
		       FID_P(&sdev->sd_obj.co_id));
		M0_ASSERT(sdev->sd_dev_idx < pc->pc_nr_devices);
		/*
		 * Set "(m0_reqh_service_ctx, sdev_fid)" tuple, associated
		 * with this dev_idx, or make sure it is set to correct
		 * values.
		 */
		dev = &pc->pc_dev2svc[sdev->sd_dev_idx];
		if (dev->pds_ctx == NULL) {
			dev->pds_sdev_fid = sdev->sd_obj.co_id;
			dev->pds_ctx = ctx;
		} else {
			M0_ASSERT(m0_fid_eq(&dev->pds_sdev_fid,
					    &sdev->sd_obj.co_id));
			M0_ASSERT(dev->pds_ctx == ctx);
		}
		M0_CNT_INC(nr_sdevs);
	}

	m0_conf_diter_fini(&it);
	M0_POST(nr_sdevs <= pc->pc_nr_devices && nr_sdevs == pv->pv_attr.pa_P);
	return M0_RC(rc);
}

M0_INTERNAL int m0_pool_version_init(struct m0_pool_version *pv,
				     const struct m0_fid *id,
				     struct m0_pool *pool,
				     uint32_t pool_width,
				     uint32_t nr_nodes,
				     uint32_t nr_data,
				     uint32_t nr_failures)
{
	int rc;

	M0_ENTRY("pver id:"FID_F"N:%d K:%d P:%d", FID_P(id), nr_data,
			nr_failures, pool_width);
	pv->pv_id = *id;
	pv->pv_attr.pa_N = nr_data;
	pv->pv_attr.pa_K = nr_failures;
	pv->pv_attr.pa_P = pool_width;
	pv->pv_pool = pool;
	pv->pv_nr_nodes = nr_nodes;

	rc = m0_poolmach_init(&pv->pv_mach, pv, pv->pv_nr_nodes,
			      pv->pv_attr.pa_P, pv->pv_nr_nodes,
			      pv->pv_attr.pa_K);
	m0_pool_version_bob_init(pv);
	pool_version_tlink_init(pv);
	pv->pv_is_dirty = false;
	pv->pv_is_stale = false;

	M0_POST(pool_version_invariant(pv));
	return M0_RC(rc);
}

M0_INTERNAL struct m0_pool_version *
m0_pool_clean_pver_find(const struct m0_pool *pool)
{
	struct m0_pool_version *pver;

	m0_tl_for (pool_version, &pool->po_vers, pver) {
		if (!pver->pv_is_dirty)
			return pver;
	} m0_tl_endfor;
	return NULL;
}

M0_INTERNAL struct m0_pool_version *
m0_pool_version_lookup(const struct m0_pools_common *pc,
		       const struct m0_fid *id)
{
	struct m0_pool         *pool;
	struct m0_pool_version *pver;

	M0_ENTRY(FID_F, FID_P(id));

	m0_tl_for (pools, &pc->pc_pools, pool) {
		pver = m0_tl_find(pool_version, pv, &pool->po_vers,
				  m0_fid_eq(&pv->pv_id, id));
		if (pver != NULL)
			return pver;
	} m0_tl_endfor;
	return NULL;
}

M0_INTERNAL struct m0_pool_version *
m0_pool_version_find(struct m0_pools_common *pc, const struct m0_fid *id)
{
	struct m0_pool_version *pv;
	struct m0_conf_root    *root;
	struct m0_conf_pver    *pver = NULL;
	int                     rc;

	M0_ENTRY(FID_F, FID_P(id));
	M0_PRE(pc != NULL);

	m0_mutex_lock(&pc->pc_mutex);
	pv = m0_pool_version_lookup(pc, id);
	if (pv != NULL) {
		rc = 0;
		goto end;
	}
	rc = m0_confc_root_open(pc->pc_confc, &root);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Cannot open root object");
		goto end;
	}
	rc = m0_conf_pver_find_by_fid(id, root, &pver) ?:
		m0_pool_version_append(pc, pver, &pv);
	m0_confc_close(&root->rt_obj);
end:
	m0_mutex_unlock(&pc->pc_mutex);
	return rc == 0 ? pv : NULL;
}

static bool is_md_pool(const struct m0_pools_common *pc,
		       const struct m0_pool *pool)
{
	return (pc->pc_md_pool != NULL &&
		m0_fid_eq(&pool->po_id, &pc->pc_md_pool->po_id));
}

static bool is_dix_pool(const struct m0_pools_common *pc,
			const struct m0_pool *pool)
{
	return (pc->pc_dix_pool != NULL &&
		m0_fid_eq(&pool->po_id, &pc->pc_dix_pool->po_id));
}

static int pool_version_get_locked(struct m0_pools_common  *pc,
				   const struct m0_fid     *hint,
				   struct m0_pool_version **pv)
{
	int                     rc = -ENOENT;
	struct m0_pool         *pool;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(&pc->pc_mutex));

	if (pv == NULL)
		return M0_RC(-EINVAL);

	m0_tl_for(pools, &pc->pc_pools, pool) {
		if (is_md_pool(pc, pool) || is_dix_pool(pc, pool))
			continue;
		if (hint != NULL && !m0_fid_eq(&pool->po_id, hint))
			continue;
		rc = pool->po_pver_policy->pp_ops->ppo_get(pc, pool, pv);
		if (rc == 0 || hint != NULL)
			break;
		/*
		 * Couldn't find pver in this pool.
		 * No worries, let's try another one.
		 */
	} m0_tl_endfor;

	return M0_RC(rc);
}

M0_INTERNAL int
m0_pool_version_get(struct m0_pools_common  *pc,
		    const struct m0_fid     *pool,
		    struct m0_pool_version **pv)
{
	int rc;

	M0_ENTRY();
	m0_mutex_lock(&pc->pc_mutex);
	rc = pool_version_get_locked(pc, pool, pv);
	m0_mutex_unlock(&pc->pc_mutex);
	return M0_RC(rc);
}

static int _nodes_count(struct m0_conf_pver *pver, uint32_t *nodes)
{
	struct m0_conf_diter  it;
	struct m0_confc      *confc;
	uint32_t              nr_nodes = 0;
	int                   rc;

	confc = m0_confc_from_obj(&pver->pv_obj);
	rc = m0_conf_diter_init(&it, confc, &pver->pv_obj,
				M0_CONF_PVER_SITEVS_FID,
				M0_CONF_SITEV_RACKVS_FID,
				M0_CONF_RACKV_ENCLVS_FID,
				M0_CONF_ENCLV_CTRLVS_FID,
				M0_CONF_CTRLV_DRIVEVS_FID);
	if (rc != 0)
		return M0_ERR(rc);

	/*
	 * XXX TODO: Replace m0_conf_diter_next_sync() with
	 * m0_conf_diter_next().
	 */
	while ((rc = m0_conf_diter_next_sync(&it, obj_is_controllerv)) ==
		M0_CONF_DIRNEXT) {
		/* We filter only controllerv objects. */
		M0_CNT_INC(nr_nodes);
	}

	*nodes = nr_nodes;
	m0_conf_diter_fini(&it);

	return M0_RC(rc);
}

M0_INTERNAL int m0_pool_version_init_by_conf(struct m0_pool_version *pv,
					     struct m0_conf_pver *pver,
					     struct m0_pool *pool,
					     struct m0_pools_common *pc)
{
	uint32_t nodes = 0;
	uint32_t failure_level;
	int      rc;

	M0_ENTRY();
	M0_PRE(pv != NULL && pver != NULL && pool != NULL && pc != NULL);

	rc = _nodes_count(pver, &nodes);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_pool_version_init(pv, &pver->pv_obj.co_id, pool,
				  pver->pv_u.subtree.pvs_attr.pa_P, nodes,
				  pver->pv_u.subtree.pvs_attr.pa_N,
				  pver->pv_u.subtree.pvs_attr.pa_K) ?:
	     m0_pool_version_device_map_init(pv, pver, pc) ?:
	     m0_poolmach_init_by_conf(&pv->pv_mach, pver) ?:
	     m0_poolmach_spare_build(&pv->pv_mach, pool, pver->pv_kind);
	if (rc == 0) {
		pv->pv_pc = pc;
		pv->pv_mach.pm_pver = pv;
		memcpy(pv->pv_fd_tol_vec, pver->pv_u.subtree.pvs_tolerance,
		       sizeof(pver->pv_u.subtree.pvs_tolerance));
		rc = m0_fd_tile_build(pver, pv, &failure_level) ?:
			m0_fd_tree_build(pver, &pv->pv_fd_tree);
		if (rc == 0)
			pool_version_tlist_add_tail(&pool->po_vers, pv);
	}

	M0_POST(pool_version_invariant(pv));
	return M0_RC(rc);
}

M0_INTERNAL void m0_pool_version_fini(struct m0_pool_version *pv)
{
	M0_ENTRY();
	M0_PRE(pool_version_invariant(pv));

	pool_version_tlink_fini(pv);
	m0_pool_version_bob_fini(pv);
	m0_poolmach_fini(&pv->pv_mach);
	pv->pv_mach.pm_pver = NULL;
	m0_fd_tile_destroy(&pv->pv_fd_tile);
	m0_fd_tree_destroy(&pv->pv_fd_tree);

	M0_LEAVE();
}

M0_INTERNAL void m0_pool_versions_fini(struct m0_pool *pool)
{
	struct m0_pool_version *pv;

	m0_tl_teardown(pool_version, &pool->po_vers, pv) {
		m0_pool_version_fini(pv);
		m0_free(pv);
	}
}

M0_INTERNAL void m0_pool_versions_stale_mark(struct m0_pools_common *pc,
					     struct m0_confc_update_state *s)
{
	struct m0_pool         *pool;
	struct m0_pool_version *pver;
	struct m0_conf_cache   *cache = &pc->pc_confc->cc_cache;

	M0_PRE(m0_mutex_is_locked(&s->cus_lock));

	if (pc->pc_confc == NULL)
		return;
	m0_mutex_lock(&pc->pc_mutex);
	m0_tl_for(pools, &pc->pc_pools, pool) {
		m0_tl_for(pool_version, &pool->po_vers, pver) {
			if (!m0_conf_cache_contains(cache, &pver->pv_id))
				pver->pv_is_stale = true;
		} m0_tl_endfor;
	} m0_tl_endfor;
	m0_mutex_unlock(&pc->pc_mutex);
}

M0_INTERNAL struct m0_pool_version *
m0_pool_version_md_get(const struct m0_pools_common *pc)
{
	M0_PRE(pc != NULL);

	return pool_version_tlist_head(&pc->pc_md_pool->po_vers);
}

static void service_ctxs_destroy(struct m0_pools_common *pc)
{
	struct m0_reqh_service_ctx *ctx;
	int                         rc;
	int                         i;

	M0_ENTRY();

	/* Disconnect from all services asynchronously. */
	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		if (m0_reqh_service_ctx_is_connected(ctx))
			m0_reqh_service_disconnect(ctx);
	} m0_tl_endfor;

	m0_tl_teardown(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		if (m0_reqh_service_ctx_is_connected(ctx)) {
			rc = m0_reqh_service_disconnect_wait(ctx);
			M0_ASSERT_INFO(M0_IN(rc, (0, -ECANCELED, -ETIMEDOUT,
						  -EINVAL, -EHOSTUNREACH,
						  -ECONNREFUSED)),
				       "rc=%d", rc);
		}
		m0_reqh_service_ctx_destroy(ctx);
	}
	M0_POST(pools_common_svc_ctx_tlist_is_empty(&pc->pc_svc_ctxs));

	for (i = 0; i < pc->pc_nr_devices; ++i)
		pc->pc_dev2svc[i].pds_ctx = NULL;

	M0_LEAVE();
}

static bool reqh_svc_ctx_is_in_pools(struct m0_pools_common *pc,
				     struct m0_conf_service *cs)
{
	return m0_tl_find(pools_common_svc_ctx, ctx, &pc->pc_svc_ctxs,
			  m0_fid_eq(&cs->cs_obj.co_id, &ctx->sc_fid)) != NULL;
}

/**
 * Creates service contexts from given struct m0_conf_service.
 * Creates service context for each endpoint in m0_conf_service::cs_endpoints.
 */
static int __service_ctx_create(struct m0_pools_common *pc,
				struct m0_conf_service *cs,
				bool services_connect)
{
	struct m0_reqh_service_ctx  *ctx;
	const char                 **endpoint;
	bool                         already_in;
	int                          rc = 0;

	M0_PRE(m0_conf_service_type_is_valid(cs->cs_type));
	M0_PRE((pc->pc_rmach != NULL) == services_connect);

	for (endpoint = cs->cs_endpoints; *endpoint != NULL; ++endpoint) {
		M0_ASSERT_INFO(endpoint == cs->cs_endpoints,
		   "Only single endpoint per service is supported for now");
		already_in = reqh_svc_ctx_is_in_pools(pc, cs);
		M0_LOG(M0_DEBUG, "%s svc:"FID_F" type:%d ep:%s",
		       already_in ? "unchanged" : "new",
		       FID_P(&cs->cs_obj.co_id),
		       (int)cs->cs_type, *endpoint);
		if (already_in)
			continue;
		rc = m0_reqh_service_ctx_create(&cs->cs_obj, cs->cs_type,
						pc->pc_rmach, *endpoint,
						POOL_MAX_RPC_NR_IN_FLIGHT,
						&ctx);
		if (rc != 0)
			return M0_ERR(rc);
		ctx->sc_pc = pc;
		pools_common_svc_ctx_tlink_init_at_tail(ctx, &pc->pc_svc_ctxs);
		if (services_connect) {
			/*
			 * m0_reqh_service_ctx handles current HA state and
			 * further state changes.
			 */
			m0_conf_cache_lock(cs->cs_obj.co_cache);
			m0_reqh_service_connect(ctx);
			m0_conf_cache_unlock(cs->cs_obj.co_cache);
		}
	}
	M0_CNT_INC(pc->pc_nr_svcs[cs->cs_type]);
	return M0_RC(rc);
}

static bool is_local_rms(const struct m0_conf_service *svc)
{
	const struct m0_conf_process *proc;
	struct m0_rpc_machine        *mach;
	const char                   *local_ep;

	if (svc->cs_type != M0_CST_RMS)
		return false;
	proc = M0_CONF_CAST(m0_conf_obj_grandparent(&svc->cs_obj),
			    m0_conf_process);
	mach = m0_reqh_rpc_mach_tlist_head(
		&m0_conf_obj2reqh(&svc->cs_obj)->rh_rpc_machines);
	local_ep = m0_rpc_machine_ep(mach);
	M0_LOG(M0_DEBUG, "local_ep=%s proc_ep=%s svc_type=%d process="FID_F
	       "service="FID_F, local_ep, proc->pc_endpoint, svc->cs_type,
	       FID_P(&proc->pc_obj.co_id), FID_P(&svc->cs_obj.co_id));
	return m0_streq(local_ep, proc->pc_endpoint);
}

static int active_rm_ctx_create(struct m0_pools_common *pc,
				bool                    service_connect)
{
	struct m0_conf_service *svc;
	struct m0_fid           active_rm;
	int                     rc = 0;

	active_rm = pc->pc_ha_ecl->ecl_rep.hae_active_rm_fid;
	if (m0_fid_is_set(&active_rm)) {
		rc = m0_conf_service_get(pc->pc_confc, &active_rm, &svc);
		if (rc != 0)
			return M0_ERR(rc);
		rc = __service_ctx_create(pc, svc, service_connect);
		m0_confc_close(&svc->cs_obj);
	}
	return M0_RC(rc);
}

static struct m0_reqh_service_ctx *
active_rm_ctx_find(struct m0_pools_common *pc)
{
	return m0_pools_common_service_ctx_find(pc,
				&pc->pc_ha_ecl->ecl_rep.hae_active_rm_fid,
				M0_CST_RMS);
}

/**
 * @todo : This needs to be converted to process (m0d) context since
 *         it connects to specific process endpoint.
 */
static int service_ctxs_create(struct m0_pools_common *pc,
			       bool service_connect)
{
	struct m0_conf_diter    it;
	struct m0_conf_service *svc;
	bool                    rm_is_set;
	int                     rc;

	M0_ENTRY();

	rc = active_rm_ctx_create(pc, service_connect);
	if (rc != 0)
		return M0_ERR(rc);
	rm_is_set = active_rm_ctx_find(pc) != NULL;

	rc = M0_FI_ENABLED("diter_fail") ? -ENOMEM :
		m0_conf_diter_init(&it, pc->pc_confc, pc->pc_confc->cc_root,
				   M0_CONF_ROOT_NODES_FID,
				   M0_CONF_NODE_PROCESSES_FID,
				   M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0) {
		if (rm_is_set)
			service_ctxs_destroy(pc);
		return M0_ERR(rc);
	}
	/*
	 * XXX TODO: Replace m0_conf_diter_next_sync() with
	 * m0_conf_diter_next().
	 */
	while ((rc = m0_conf_diter_next_sync(&it, obj_is_service)) ==
		M0_CONF_DIRNEXT) {
		svc = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_service);
		/*
		 * Connection to confd is managed by configuration client.
		 * confd is already connected in m0_confc_init() to the
		 * endpoint provided by the command line argument -C.
		 *
		 * Connection to RMS is skipped here except local RMS
		 * if local configuration preloaded. Nodes will only
		 * use RM services returned by HA entrypoint.
		 *
		 * FI services need no service context either.
		 */
		if ((!rm_is_set && is_local_rms(svc)) ||
		    !M0_IN(svc->cs_type, (M0_CST_CONFD, M0_CST_RMS, M0_CST_HA,
					  M0_CST_FIS))) {
			rc = __service_ctx_create(pc, svc, service_connect);
			if (rc != 0)
				break;
		}
	}
	m0_conf_diter_fini(&it);
	if (rc != 0)
		service_ctxs_destroy(pc);
	return M0_RC(rc);
}

static bool service_ctx_ha_entrypoint_cb(struct m0_clink *clink)
{
	struct m0_pools_common             *pc = M0_AMB(pc, clink, pc_ha_clink);
	struct m0_ha_entrypoint_client     *ecl = pc->pc_ha_ecl;
	struct m0_ha_entrypoint_rep        *rep = &ecl->ecl_rep;
	enum m0_ha_entrypoint_client_state  state;

	state = m0_ha_entrypoint_client_state_get(ecl);
	if (state == M0_HEC_AVAILABLE &&
	    rep->hae_control != M0_HA_ENTRYPOINT_QUERY &&
	    m0_fid_is_set(&rep->hae_active_rm_fid) &&
	    (pc->pc_rm_ctx == NULL || !m0_fid_eq(&pc->pc_rm_ctx->sc_fid,
						 &rep->hae_active_rm_fid))) {
		m0_mutex_lock(&pc->pc_rm_lock);
		pc->pc_rm_ctx = active_rm_ctx_find(pc);
		M0_LOG(M0_DEBUG, "new RM="FID_F" pc_rm_ctx=%p",
		       FID_P(&rep->hae_active_rm_fid),
		       pc->pc_rm_ctx);
		/*
		 * If pc_rm_ctx == NULL - it will be set at
		 * m0_pools_common_conf_ready_async_cb() when
		 * the configuration is ready. The latter is
		 * required for active_rm_ctx_create().
		 */
		m0_mutex_unlock(&pc->pc_rm_lock);
	}
	return true;
}

M0_INTERNAL struct m0_rpc_session *
m0_pools_common_active_rm_session(struct m0_pools_common *pc)
{
	struct m0_rpc_session *sess = NULL;

	m0_mutex_lock(&pc->pc_rm_lock);
	if (pc->pc_rm_ctx != NULL)
		sess = &pc->pc_rm_ctx->sc_rlink.rlk_sess;
	m0_mutex_unlock(&pc->pc_rm_lock);

	return sess;
}

static struct m0_reqh_service_ctx *
service_ctx_find_by_type(const struct m0_pools_common *pc,
			 enum m0_conf_service_type     type)
{
	return m0_tl_find(pools_common_svc_ctx, ctx, &pc->pc_svc_ctxs,
			  ctx->sc_type == type);
}

M0_INTERNAL struct m0_reqh_service_ctx *
m0_pools_common_service_ctx_find(const struct m0_pools_common *pc,
				 const struct m0_fid *id,
				 enum m0_conf_service_type type)
{
	return m0_tl_find(pools_common_svc_ctx, ctx, &pc->pc_svc_ctxs,
			  m0_fid_eq(id, &ctx->sc_fid) && ctx->sc_type == type);
}

/**
 * Callback called on configuration expiration detected by rconfc. In the course
 * of handling all existing reqh service contexts are unsubscribed from HA
 * notifications. This lets conf cache be drained.
 *
 * @note The context connection state remains unchanged.
 */
static bool pools_common_conf_expired_cb(struct m0_clink *clink)
{
	struct m0_pools_common     *pc = M0_AMB(pc, clink, pc_conf_exp);
	struct m0_reqh_service_ctx *ctx;

	M0_ENTRY("pc %p", pc);

	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		m0_reqh_service_ctx_unsubscribe(ctx);
	} m0_tl_endfor;
#ifndef __KERNEL__
	/*
	 * The async callback for pools_common shall be called after
	 * configuration expired and new configuration ready events. For this
	 * reason the clink isn't added to m0_reqh::rh_conf_cache_ready_async
	 * chan in m0_pools_common_init(), because it might be triggered when
	 * rconfc starts at the first time.
	 */
	m0_clink_cleanup(&pc->pc_conf_ready_async);
	/*
	 * Note: m0_reqh::rh_conf_cache_ready_async and
	 * m0_reqh::rh_conf_cache_exp are protected with different mutexes, but
	 * deadlock is impossible due to the fact that broadcast on
	 * rh_conf_cache_ready_async happens only atfer broadcasting on
	 * rh_conf_cache_ready chan which is protected with same mutex as
	 * rh_conf_cache_exp. Thus, here the sequence of locks is constant:
	 * 1. When conf expired:
	 * m0_mutex_lock(m0_reqh::rh_guard
	 * m0_mutex_lock(m0_reqh::rh_guard_async
	 * m0_mutex_unlock(m0_reqh::rh_guard_async
	 * m0_mutex_unlock(m0_reqh::rh_guard
	 * 2. When conf ready
	 * m0_mutex_lock(m0_reqh::rh_guard
	 * m0_mutex_unlock(m0_reqh::rh_guard
	 * m0_mutex_lock(m0_reqh::rh_guard_async
	 * m0_mutex_unlock(m0_reqh::rh_guard_async
	 */
	m0_clink_add_lock(
			&m0_confc2reqh(pc->pc_confc)->rh_conf_cache_ready_async,
			&pc->pc_conf_ready_async);
#endif
	M0_LEAVE();
	return true;
}

/**
 * Service context is matching to current configuration only when 1) respective
 * service object is found in conf, i.e. not NULL, 2) belongs to the process
 * with originally known and not changed fid, and 3) has its rpc connection
 * endpoint in the conf.
 */
static bool reqh_service_ctx_is_matching(const struct m0_reqh_service_ctx *ctx,
					 const struct m0_conf_obj         *svc)
{
	struct m0_conf_obj *proc;
	const char         *ep = m0_rpc_link_end_point(&ctx->sc_rlink);

	M0_PRE(svc != NULL); /* service found in conf */
	proc = m0_conf_obj_grandparent(svc);
	return m0_fid_eq(&proc->co_id, &ctx->sc_fid_process) &&
		m0_conf_service_ep_is_known(svc, ep);
}

/**
 * Moves reqh service context from m0_pools_common::pc_svc_ctx list to
 * m0_reqh::rh_abandoned_svc_ctxs list. Once abandoned the context appears
 * unlinked from rconfc events and instructed to disconnect itself
 * asynchronously.
 */
static void reqh_service_ctx_abandon(struct m0_reqh_service_ctx *ctx)
{
	/*
	 * The context is unsubscribed by now. When configuration updates,
	 * pools_common_conf_ready_cb() is always called after
	 * pools_common_conf_expired_cb():
	 *
	 * read lock conflict
	 *  \_ pools_common_conf_expired_cb()
	 *      \_ m0_reqh_service_ctx_unsubscribe() <-- context clinks cleanup
	 *
	 * ... new conf version distribution ...
	 *
	 * read lock acquisition && full conf load
	 *  \_ pools_common_conf_ready_cb()
	 *      \_ pools_common__ctx_subscribe_or_abandon()
	 *          \_ reqh_service_ctx_abandon()    <-- YOU ARE HERE
	 */
	M0_PRE(ctx->sc_svc_event.cl_chan == NULL);
	M0_PRE(ctx->sc_process_event.cl_chan == NULL);

	/* Move the context to the list of abandoned ones. */
	M0_PRE(ctx->sc_pc != NULL);
	pools_common_svc_ctx_tlink_del_fini(ctx);
	pools_common_svc_ctx_tlink_init_at_tail(ctx, &ctx->sc_pc->
						pc_abandoned_svc_ctxs);
	/*
	 * The context found disappeared from conf, so go start disconnecting it
	 * asynchronously if needed.
	 */
	if (m0_reqh_service_ctx_is_connected(ctx))
		m0_reqh_service_disconnect(ctx);
	/*
	 * The context is to be physically destroyed later when rpc link
	 * disconnection is confirmed in reqh_service_ctx_ast_cb().
	 */
}

/**
 * Every previously existing reqh service context must be checked for being
 * known to new conf and remained unchanged. During this check the unchanged
 * service context is subscribed to HA notifications. Otherwise the context is
 * abandoned and ultimately instructed to disconnect.
 */
static void pools_common__ctx_subscribe_or_abandon(struct m0_pools_common *pc)
{
	struct m0_conf_cache       *cache = &pc->pc_confc->cc_cache;
	struct m0_conf_obj         *obj;
	struct m0_reqh_service_ctx *ctx;

	M0_ENTRY("pc %p", pc);

	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		m0_conf_cache_lock(cache);
		obj = m0_conf_cache_lookup(cache, &ctx->sc_fid);
		if (obj != NULL && reqh_service_ctx_is_matching(ctx, obj)) {
			ctx->sc_service = obj;
			ctx->sc_process = m0_conf_obj_grandparent(obj);
			if (!m0_clink_is_armed(&ctx->sc_svc_event) &&
			    !m0_clink_is_armed(&ctx->sc_process_event))
				m0_reqh_service_ctx_subscribe(ctx);
		} else {
			reqh_service_ctx_abandon(ctx);
		}
		m0_conf_cache_unlock(cache);
	} m0_tl_endfor;

	M0_LEAVE();
}

static void pools_common__md_pool_cleanup(struct m0_pools_common *pc)
{
	if (pc->pc_md_pool_linst != NULL)
		m0_layout_instance_fini(pc->pc_md_pool_linst);
	pc->pc_md_pool_linst = NULL;
	pc->pc_md_pool = NULL;
}

static void pools_common__pools_update_or_cleanup(struct m0_pools_common *pc)
{
	struct m0_pool         *pool;
	struct m0_pool_version *pver;
	struct m0_conf_cache   *cache = &pc->pc_confc->cc_cache;
	struct m0_reqh         *reqh = m0_confc2reqh(pc->pc_confc);
	bool                    in_conf;

	m0_tl_for(pools, &pc->pc_pools, pool) {
		in_conf = m0_conf_cache_contains(cache, &pool->po_id);
		M0_LOG(M0_DEBUG, "pool %p "FID_F" is %sknown", pool,
		       FID_P(&pool->po_id), in_conf ? "":"not ");

		if (!in_conf) {
			if (pc->pc_md_pool == pool)
				pools_common__md_pool_cleanup(pc);
			if (pc->pc_dix_pool != NULL && pc->pc_dix_pool == pool)
				pc->pc_dix_pool = NULL;
			/* cleanup */
			pools_tlink_del_fini(pool);
			pool__layouts_evict(pool, &reqh->rh_ldom);
			m0_pool_versions_fini(pool);
			m0_pool_fini(pool);
			m0_free(pool);
		} else {
			m0_tl_for(pool_version, &pool->po_vers, pver) {
				if (!m0_conf_cache_contains(cache,
							    &pver->pv_id)) {
					pool_version_tlink_del_fini(pver);
					pool_version__layouts_evict(pver,
								&reqh->rh_ldom);
					m0_pool_version_fini(pver);
					m0_free(pver);
				}
			} m0_tl_endfor;
		}
	} m0_tl_endfor;
}

static void pool__layouts_evict(struct m0_pool *pool,
				struct m0_layout_domain *ldom)
{

	struct m0_pool_version *pver;

	m0_tl_for(pool_version, &pool->po_vers, pver) {
		pool_version__layouts_evict(pver, ldom);
	} m0_tl_endfor;

}

static void pool_version__layouts_evict(struct m0_pool_version *pv,
					struct m0_layout_domain *ldom)
{
	struct m0_pdclust_attr *pa = &pv->pv_attr;
	uint64_t                layout_id;
	struct m0_layout       *layout;
	int                     i;

	for (i = M0_DEFAULT_LAYOUT_ID; i < m0_lid_to_unit_map_nr; ++i) {
		pa->pa_unit_size = m0_lid_to_unit_map[i];
		m0_uint128_init(&pa->pa_seed, M0_PDCLUST_SEED);
		layout_id = m0_pool_version2layout_id(&pv->pv_id, i);
		layout = m0_layout_find(ldom, layout_id);
		if (layout != NULL) {
			m0_layout_put(layout);
			/*
			 * Assumes that all referees of layout have put the
			 * reference.
			 */
			M0_ASSERT(m0_ref_read(&layout->l_ref) == 1);
			m0_layout_put(layout);
		}
	}
}

static int pools_common__update_by_conf(struct m0_pools_common *pc)
{
	int              rc;
	struct m0_reqh  *reqh = m0_confc2reqh(pc->pc_confc);
	struct m0_fid   *pfid =	m0_reqh2profile(reqh);

	M0_ENTRY("pc=%p", pc);

	if (m0_fid_eq(pfid, &M0_FID0))
		pfid = NULL;

	if (pc->pc_md_pool_linst != NULL)
		pools_common__md_pool_cleanup(pc);
	rc = pools_common_refresh_locked(pc) ?:
		pools_common__dev2ios_build(pc) ?:
		m0_pools_setup(pc, pfid, NULL, NULL) ?:
		m0_pool_versions_setup(pc) ?:
		m0_reqh_mdpool_layout_build(reqh);

	return M0_RC(rc);
}

/**
 * Callback called when new configuration is ready.
 */
M0_INTERNAL bool m0_pools_common_conf_ready_async_cb(struct m0_clink *clink)
{
	struct m0_pools_common *pc = M0_AMB(pc, clink, pc_conf_ready_async);
	int                     rc;

	M0_ENTRY("pc %p", pc);
	M0_PRE(pools_common_invariant(pc));

	m0_mutex_lock(&pc->pc_rm_lock);
	if (pc->pc_rm_ctx == NULL) {
		/*
		 * active_rm_ctx_create() calls m0_conf_service_get()
		 * which requires configuration to be ready. Therefore,
		 * we have it here instead of service_ctx_ha_entrypoint_cb().
		 */
		rc = active_rm_ctx_create(pc, true);
		M0_ASSERT(rc == 0);
		pc->pc_rm_ctx = active_rm_ctx_find(pc);
		M0_LOG(M0_DEBUG, "created new pc_rm_ctx=%p", pc->pc_rm_ctx);
	}
	m0_mutex_unlock(&pc->pc_rm_lock);
	M0_ASSERT(pc->pc_rm_ctx != NULL);

	m0_mutex_lock(&pc->pc_mutex);

	pools_common__ctx_subscribe_or_abandon(pc);
	/*
	 * prepare for refreshing pools common:
	 * - zero counters
	 * - free runtime maps
	 */
	M0_SET_ARR0(pc->pc_nr_svcs);
	m0_free0(&pc->pc_mds_map);
	m0_free0(&pc->pc_dev2svc);
	pc->pc_nr_devices = 0;
	/* cleanup outdated pools if any */
	pools_common__pools_update_or_cleanup(pc);
	/* add missing service contexts and re-build mds map */
	rc = pools_common__update_by_conf(pc);
	/**
	 * @todo XXX: See if we could do anything with the failed update
	 * here. But so far we just cross fingers and hope it succeeds.
	 */
	M0_POST(rc == 0);
	M0_POST(pools_common_invariant(pc));
	m0_mutex_unlock(&pc->pc_mutex);
	M0_LEAVE();
	return true;
}

static int pools_common__dev2ios_build(struct m0_pools_common *pc)
{
	int rc;

	M0_ENTRY();
	rc = m0_conf_devices_count(pc->pc_confc,
				   M0_BITS(M0_CST_IOS, M0_CST_CAS),
				   &pc->pc_nr_devices);
	if (rc != 0)
		return M0_ERR(rc);
	M0_LOG(M0_DEBUG, "io/cas services device count: %u", pc->pc_nr_devices);
	M0_ALLOC_ARR(pc->pc_dev2svc, pc->pc_nr_devices);
	if (pc->pc_dev2svc == NULL)
		return M0_ERR(-ENOMEM);
	return M0_RC(0);
}

M0_INTERNAL int m0_pools_common_init(struct m0_pools_common *pc,
				     struct m0_rpc_machine *rmach)
{
	struct m0_confc     *confc;
	struct m0_conf_root *root;

	M0_ENTRY();
	confc = m0_reqh2confc(rmach->rm_reqh);
	root = M0_CONF_CAST(confc->cc_root, m0_conf_root);
	return M0_RC(m0__pools_common_init(pc, rmach, root));
}

M0_INTERNAL int m0__pools_common_init(struct m0_pools_common *pc,
				      struct m0_rpc_machine *rmach,
				      struct m0_conf_root *root)
{
	struct m0_reqh *reqh;
	int             rc;

	M0_ENTRY();
	M0_PRE(pc != NULL);
	M0_PRE(rmach == NULL ||
	       m0_reqh2confc(rmach->rm_reqh)->cc_root == &root->rt_obj);

	*pc = (struct m0_pools_common) {
		.pc_rmach         = rmach,
		.pc_md_redundancy = root->rt_mdredundancy,
		.pc_confc         = m0_confc_from_obj(&root->rt_obj),
		.pc_cur_pver      = NULL,
		.pc_md_pool       = NULL,
		.pc_dix_pool      = NULL
	};

	rc = pools_common__dev2ios_build(pc);
	if (rc != 0) {
		/* We want pools_common_invariant(pc) to fail. */
		pc->pc_confc = NULL;
		return M0_ERR(rc);
	}
	m0_mutex_init(&pc->pc_mutex);
	pools_common_svc_ctx_tlist_init(&pc->pc_abandoned_svc_ctxs);
	pools_common_svc_ctx_tlist_init(&pc->pc_svc_ctxs);
	pools_tlist_init(&pc->pc_pools);
	pc->pc_ha_ecl = &m0_get()->i_ha->h_entrypoint_client;
	m0_mutex_init(&pc->pc_rm_lock);
	m0_clink_init(&pc->pc_ha_clink, service_ctx_ha_entrypoint_cb);
	m0_clink_init(&pc->pc_conf_exp, pools_common_conf_expired_cb);
	reqh = m0_confc2reqh(pc->pc_confc);
	m0_clink_add_lock(&reqh->rh_conf_cache_exp, &pc->pc_conf_exp);
#ifndef __KERNEL__
	/*
	 * m0_pools_common_conf_ready_async_cb() is called directly by
	 * m0t1fs_ref_put_lock() in m0t1fs code. No need to register the
	 * callback in kernel space.
	 * @see m0t1fs_ref_put_lock()
	 */
	m0_clink_init(&pc->pc_conf_ready_async,
		      m0_pools_common_conf_ready_async_cb);
#endif
	M0_POST(pools_common_invariant(pc));
	return M0_RC(0);
}

/**
 * Refreshing m0_pools_common implies:
 * - creating service ctx that m0_pools_common::pc_svc_ctxs list is missing
 * - re-building m0_pools_common::pc_mds_map
 * - finding special contexts critical for operations with pools
 */
static int pools_common_refresh_locked(struct m0_pools_common *pc)
{
	int rc;

	M0_PRE(m0_mutex_is_locked(&pc->pc_mutex));
	M0_PRE(pc->pc_mds_map == NULL);

	rc = service_ctxs_create(pc, pc->pc_rmach != NULL);
	if (rc != 0)
		return M0_ERR(rc);

	M0_ALLOC_ARR(pc->pc_mds_map,
		     pools_common_svc_ctx_tlist_length(&pc->pc_svc_ctxs));
	rc = pc->pc_mds_map == NULL ? M0_ERR(-ENOMEM) :
		pool_mds_map_init(pc);
	if (rc != 0)
		goto err;
	pc->pc_rm_ctx = service_ctx_find_by_type(pc, M0_CST_RMS);
	if (pc->pc_rm_ctx == NULL) {
		rc = M0_ERR_INFO(-ENOENT, "The mandatory rmservice is missing."
				 "Make sure this is specified in the conf db.");
		goto err;
	}
	M0_POST(pools_common_invariant(pc));
	return M0_RC(0);
err:
	m0_free0(&pc->pc_mds_map);
	service_ctxs_destroy(pc);
	return M0_ERR(rc);
}

static int pools_common_refresh(struct m0_pools_common *pc)
{
	int rc;

	m0_mutex_lock(&pc->pc_mutex);
	rc = pools_common_refresh_locked(pc);
	m0_mutex_unlock(&pc->pc_mutex);
	return M0_RC(rc);
}

M0_INTERNAL int m0_pools_service_ctx_create(struct m0_pools_common *pc)
{
	int rc;

	M0_ENTRY();
	M0_PRE(pc != NULL);

	rc = pools_common_refresh(pc);
	if (rc == 0)
		m0_clink_add_lock(m0_ha_entrypoint_client_chan(pc->pc_ha_ecl),
				  &pc->pc_ha_clink);
	return M0_RC(rc);
}

M0_INTERNAL void m0_pools_common_fini(struct m0_pools_common *pc)
{
	M0_ENTRY();
	M0_PRE(pools_common_invariant(pc));

	m0_clink_fini(&pc->pc_ha_clink);
	m0_mutex_fini(&pc->pc_rm_lock);
	pools_common_svc_ctx_tlist_fini(&pc->pc_abandoned_svc_ctxs);
	pools_common_svc_ctx_tlist_fini(&pc->pc_svc_ctxs);
	pools_tlist_fini(&pc->pc_pools);
	m0_free0(&pc->pc_dev2svc);
	m0_clink_cleanup(&pc->pc_conf_exp);
	m0_clink_fini(&pc->pc_conf_exp);
#ifndef __KERNEL__
	m0_clink_cleanup(&pc->pc_conf_ready_async);
	m0_clink_fini(&pc->pc_conf_ready_async);
#endif
	m0_mutex_fini(&pc->pc_mutex);
	/*
	 * We want pools_common_invariant(pc) to fail after
	 * m0_pools_common_fini().
	 */
	pc->pc_confc = NULL;
	M0_LEAVE();
}

/**
 * Destroys all service contexts kept in m0_pools_common::pc_abandoned_svc_ctxs
 * list.
 */
static void abandoned_svc_ctxs_cleanup(struct m0_pools_common *pc)
{
	struct m0_reqh_service_ctx *ctx;

	if (!m0_confc_is_inited(pc->pc_confc)) {
		/*
		 * rconfc seems failed to start or never tried to, so no
		 * configuration was read, so no conf updates were possible, so
		 * there was no condition to abandon any context, thus must be
		 * nothing to clean here.
		 */
		M0_ASSERT(pools_common_svc_ctx_tlist_is_empty(
				  &pc->pc_abandoned_svc_ctxs));
		return;
	}

	m0_tl_teardown(pools_common_svc_ctx, &pc->pc_abandoned_svc_ctxs, ctx) {
		/*
		 * Any abandoned context was initially set for disconnection
		 * (see reqh_service_ctx_abandon())
		 *
		 * So just wait here for disconnection completion if required.
		 */
		m0_reqh_service_disconnect_wait(ctx);
		m0_reqh_service_ctx_destroy(ctx);
	}
}

M0_INTERNAL void
m0_pools_common_service_ctx_connect_sync(struct m0_pools_common *pc)
{
	struct m0_reqh_service_ctx *ctx;

	m0_tl_for (pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		m0_reqh_service_connect_wait(ctx);
	} m0_tl_endfor;
}

M0_INTERNAL void m0_pools_service_ctx_destroy(struct m0_pools_common *pc)
{
	M0_ENTRY();
	M0_PRE(pools_common_invariant(pc));
	/* m0_cs_fini() calls this function even without m0_cs_start() */
	if (m0_clink_is_armed(&pc->pc_ha_clink))
		m0_clink_del_lock(&pc->pc_ha_clink);
	service_ctxs_destroy(pc);
	m0_free0(&pc->pc_mds_map);
	abandoned_svc_ctxs_cleanup(pc);
	M0_LEAVE();
}

static bool is_actual_pver(const struct m0_conf_obj *obj)
{
	/**
	 * @todo XXX filter only actual pool versions till formulaic
	 *           pool version creation in place.
	 */
	return m0_conf_obj_type(obj) == &M0_CONF_PVER_TYPE &&
		M0_CONF_CAST(obj, m0_conf_pver)->pv_kind == M0_CONF_PVER_ACTUAL;
}

M0_INTERNAL int m0_pool_versions_setup(struct m0_pools_common *pc)
{
	struct m0_confc        *confc;
	struct m0_conf_diter    it;
	struct m0_pool_version *pver;
	struct m0_conf_pver    *pver_obj;
	struct m0_fid          *pool_id;
	struct m0_pool         *pool;
	struct m0_reqh         *reqh = m0_confc2reqh(pc->pc_confc);
	int                     rc;

	M0_ENTRY();

	confc = pc->pc_confc;
	rc = M0_FI_ENABLED("diter_fail") ? -ENOMEM :
		m0_conf_diter_init(&it, confc, confc->cc_root,
				   M0_CONF_ROOT_POOLS_FID,
				   M0_CONF_POOL_PVERS_FID);
	if (rc != 0)
		return M0_ERR(rc);

	while ((rc = m0_conf_diter_next_sync(&it, is_actual_pver)) ==
	       M0_CONF_DIRNEXT) {
		pver_obj = M0_CONF_CAST(m0_conf_diter_result(&it),
					m0_conf_pver);
		pool_id = &m0_conf_obj_grandparent(&pver_obj->pv_obj)->co_id;
		pool = m0_tl_find(pools, pool, &pc->pc_pools,
				  m0_fid_eq(&pool->po_id, pool_id));
		if (pool == NULL)
			continue;
		pver = m0_tl_find(pool_version, pver, &pool->po_vers,
				  m0_fid_eq(&pver_obj->pv_obj.co_id,
					    &pver->pv_id));
		M0_LOG(M0_DEBUG, "%spver:"FID_F, pver != NULL ? "! ":"",
		       FID_P(&pver_obj->pv_obj.co_id));
		if (pver != NULL) {
			/*
			 * Version is already in pool, so we must be in pools
			 * refreshing cycle.
			 */
			rc = m0_pool_version_device_map_init(pver, pver_obj, pc);
			if (rc != 0)
				break;
			continue;
		}
		M0_ALLOC_PTR(pver);
		if (pver == NULL) {
			rc = M0_ERR(-ENOMEM);
			break;
		}
		rc = m0_pool_version_init_by_conf(pver, pver_obj, pool, pc) ?:
		     m0_layout_init_by_pver(&reqh->rh_ldom, pver, NULL);
		if (rc != 0)
			break;
	}

	m0_conf_diter_fini(&it);
	if (rc != 0)
		m0_pools_destroy(pc);

	return M0_RC(rc);
}

static int pool_from_virtual_pver(const struct m0_conf_pver *virtual,
				  struct m0_confc           *confc,
				  struct m0_conf_pool      **out)
{
	struct m0_conf_root       *root;
	const struct m0_conf_pver *fpver;
	int                        rc;

	M0_ENTRY("virtual="FID_F, FID_P(&virtual->pv_obj.co_id));
	M0_PRE(virtual->pv_kind == M0_CONF_PVER_VIRTUAL);

	rc = m0_confc_root_open(confc, &root);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Cannot open root object");
	rc = m0_conf_pver_formulaic_from_virtual(virtual, root, &fpver);
	if (rc == 0)
		*out = M0_CONF_CAST(m0_conf_obj_grandparent(&fpver->pv_obj),
				    m0_conf_pool);
	m0_confc_close(&root->rt_obj);
	return M0_RC(rc);
}

M0_INTERNAL int m0_pool_version_append(struct m0_pools_common  *pc,
				       struct m0_conf_pver     *pver,
				       struct m0_pool_version **pv)
{
	struct m0_conf_pool *cp;
	struct m0_pool      *p;
	int                  rc;

	M0_ENTRY();

	if (pver->pv_kind == M0_CONF_PVER_ACTUAL) {
		cp = M0_CONF_CAST(m0_conf_obj_grandparent(&pver->pv_obj),
				  m0_conf_pool);
	} else {
		rc = pool_from_virtual_pver(pver, pc->pc_confc, &cp);
		if (rc != 0)
			return M0_ERR(rc);
	}
	p = pool_find(pc, &cp->pl_obj.co_id);
	M0_ASSERT(p != NULL);

	M0_ALLOC_PTR(*pv);
	if (*pv == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_pool_version_init_by_conf(*pv, pver, p, pc);
	if (rc != 0) {
		m0_free(*pv);
		return M0_ERR(rc);
	}
	rc = m0_layout_init_by_pver(&m0_confc2reqh(pc->pc_confc)->rh_ldom, *pv,
				    NULL);
	if (rc != 0) {
		m0_pool_version_fini(*pv);
		m0_free(*pv);
		return M0_ERR(rc);
	}
	return M0_RC(0);
}

static int
_pool_create(struct m0_pool **out, const struct m0_conf_pool *conf_pool)
{
	int rc;

	M0_ALLOC_PTR(*out);
	if (*out == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_pool_init(*out, &conf_pool->pl_obj.co_id,
			  conf_pool->pl_pver_policy);
	if (rc != 0)
		m0_free0(out);
	return M0_RC(rc);
}

static void
dix_pool_setup(struct m0_pools_common *pc, const struct m0_fid *imeta_pver)
{
	M0_ENTRY("imeta_pver="FID_F, FID_P(imeta_pver));
	if (m0_fid_is_set(imeta_pver)) {
		struct m0_conf_obj  *pver;
		struct m0_conf_pool *pool;
		int                  rc;

		rc = m0_conf_obj_find_lock(&pc->pc_confc->cc_cache, imeta_pver,
					   &pver);
		M0_ASSERT(rc == 0);
		pool = M0_CONF_CAST(m0_conf_obj_grandparent(pver),
				    m0_conf_pool);
		pc->pc_dix_pool = pool_find(pc, &pool->pl_obj.co_id);
		M0_ASSERT(pc->pc_dix_pool != NULL);
		M0_LOG(M0_DEBUG, "imeta_pver="FID_F" -> dix_pool="FID_F,
		       FID_P(imeta_pver), FID_P(&pc->pc_dix_pool->po_id));
	}
	M0_LEAVE();
}

static bool profile_has_pool(const struct m0_conf_profile *profile,
			     const struct m0_fid          *pool)
{
	return m0_exists(i, profile->cp_pools.af_count,
		         m0_fid_eq(&profile->cp_pools.af_elems[i], pool));
}

M0_INTERNAL int m0_pools_setup(struct m0_pools_common *pc,
			       const struct m0_fid    *profile,
			       struct m0_sm_group     *sm_grp,
			       struct m0_dtm          *dtm)
{
	int                     rc;
	struct m0_pool         *pool;
	struct m0_conf_obj     *pool_obj;
	struct m0_conf_root    *root;
	struct m0_conf_profile *prof = NULL;
	struct m0_conf_diter    it;

	M0_ENTRY("profile="FID_F, FID_P(profile ?: &M0_FID0));

	if (profile != NULL) {
		rc = m0_confc_profile_open(pc->pc_confc, profile, &prof);
		if (rc != 0)
			return M0_ERR_INFO(rc, "profile="FID_F, FID_P(profile));
	}
	root = M0_CONF_CAST(pc->pc_confc->cc_root, m0_conf_root);
	rc = M0_FI_ENABLED("diter_fail") ? M0_ERR(-ENOMEM) :
		m0_conf_diter_init(&it, pc->pc_confc, &root->rt_obj,
				   M0_CONF_ROOT_POOLS_FID);
	if (rc != 0) {
		if (prof != NULL)
			m0_confc_close(&prof->cp_obj);
		return M0_ERR(rc);
	}
	while ((rc = m0_conf_diter_next_sync(&it, m0_conf_obj_is_pool)) ==
		M0_CONF_DIRNEXT) {
		pool_obj = m0_conf_diter_result(&it);
		M0_LOG(M0_DEBUG, "pool="FID_F, FID_P(&pool_obj->co_id));
		if (prof != NULL && !profile_has_pool(prof, &pool_obj->co_id))
			continue;
		pool = pool_find(pc, &pool_obj->co_id);
		if (pool != NULL)
			/*
			 * Pool is already in pools common, so we must be in
			 * pools refreshing cycle.
			 */
			continue;
		rc = _pool_create(&pool, M0_CONF_CAST(pool_obj, m0_conf_pool));
		if (rc != 0)
			break;
		pools_tlink_init_at_tail(pool, &pc->pc_pools);
	}
	m0_conf_diter_fini(&it);
	if (prof != NULL)
		m0_confc_close(&prof->cp_obj);
	if (rc != 0) {
		m0_pools_destroy(pc);
		return M0_ERR(rc);
	}
	/* MD pool setup. */
	pc->pc_md_pool = pool_find(pc, &root->rt_mdpool);
	if (pc->pc_md_pool == NULL) {
		m0_pools_destroy(pc);
		return M0_ERR_INFO(-ENOENT, "Cannot find metadata pool "FID_F,
				   FID_P(&root->rt_mdpool));
	}
	M0_LOG(M0_DEBUG, "md_pool="FID_F, FID_P(&root->rt_mdpool));

	dix_pool_setup(pc, &root->rt_imeta_pver);
	return M0_RC(0);
}

M0_INTERNAL void m0_pool_versions_destroy(struct m0_pools_common *pc)
{
	struct m0_pool *p;

	M0_ENTRY();
	m0_tl_for(pools, &pc->pc_pools, p) {
		m0_pool_versions_fini(p);
	} m0_tl_endfor;
	M0_LEAVE();
}

M0_INTERNAL void m0_pools_destroy(struct m0_pools_common *pc)
{
	struct m0_pool *p;

	M0_ENTRY();
	m0_tl_teardown(pools, &pc->pc_pools, p) {
		m0_pool_fini(p);
		m0_free(p);
	}
	M0_LEAVE();
}

M0_INTERNAL uint64_t
m0_pool_version2layout_id(const struct m0_fid *pv_fid, uint64_t lid)
{
	return m0_hash(m0_fid_hash(pv_fid) + lid);
}

M0_INTERNAL uint32_t m0_ha2pm_state_map(enum m0_ha_obj_state hastate)
{
	uint32_t ha2pm_statemap [] = {
		[M0_NC_ONLINE]         = M0_PNDS_ONLINE,
		[M0_NC_FAILED]         = M0_PNDS_FAILED,
		[M0_NC_TRANSIENT]      = M0_PNDS_OFFLINE,
		[M0_NC_REPAIR]         = M0_PNDS_SNS_REPAIRING,
		[M0_NC_REPAIRED]       = M0_PNDS_SNS_REPAIRED,
		[M0_NC_REBALANCE]      = M0_PNDS_SNS_REBALANCING,
	};
	M0_ASSERT (hastate < M0_NC_NR);
	return ha2pm_statemap[hastate];
}

static bool disks_poolmach_state_update_cb(struct m0_clink *cl)
{
	struct m0_poolmach_event  pme;
	struct m0_conf_obj       *obj =
		container_of(cl->cl_chan, struct m0_conf_obj, co_ha_chan);
	struct m0_pooldev        *pdev =
		container_of(cl, struct m0_pooldev, pd_clink.bc_u.clink);

	M0_ENTRY();
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_DRIVE_TYPE);

	pme.pe_type = M0_POOL_DEVICE;
	pme.pe_index = pdev->pd_index;
	pme.pe_state = m0_ha2pm_state_map(obj->co_ha_state);
	M0_LOG(M0_DEBUG, "pe_type=%6s pe_index=%x, pe_state=%10d",
			 pme.pe_type == M0_POOL_DEVICE ? "device":"node",
			 pme.pe_index, pme.pe_state);

	return M0_RC(m0_poolmach_state_transit(pdev->pd_pm, &pme));
}

M0_INTERNAL void m0_pooldev_clink_del(struct m0_clink *cl)
{
	if (M0_FI_ENABLED("do_nothing_for_poolmach-ut")) {
		/*
		 * The poolmach-ut does not add/register clink in pooldev.
		 * So need to skip deleting the links if this is called
		 * during poolmach-ut.
		 * TODO: This is workaround & can be addressed differently.
		 */
		return;
	}
	m0_clink_del_lock(cl);
	m0_clink_fini(cl);
}

M0_INTERNAL void m0_pooldev_clink_add(struct m0_clink *link,
				      struct m0_chan  *chan)
{
	m0_clink_init(link, disks_poolmach_state_update_cb);
	m0_clink_add_lock(chan, link);
}

#ifndef __KERNEL__
/**
 * Find out device ids of the REPAIRED devices in the given pool machine
 * and call m0_mero_stob_reopen() on each of them.
 */
M0_INTERNAL int m0_pool_device_reopen(struct m0_poolmach *pm,
				      struct m0_reqh *reqh)
{
	struct m0_pool_spare_usage *spare_array;
	struct m0_pooldev          *dev_array;
	uint32_t                    dev_id;
	int                         i;
	int                         rc = 0;

	dev_array = pm->pm_state->pst_devices_array;
	spare_array = pm->pm_state->pst_spare_usage_array;
	for (i = 0; spare_array[i].psu_device_index !=
	     POOL_PM_SPARE_SLOT_UNUSED; ++i) {
		dev_id = spare_array[i].psu_device_index;
		if (dev_array[dev_id].pd_state == M0_PNDS_SNS_REPAIRED) {
			rc = m0_mero_stob_reopen(reqh, pm, dev_id);
			if (rc != 0)
				return M0_ERR(rc);
		}
	}
	return M0_RC(rc);
}

static int pool_device_index(const struct m0_poolmach *pm,
			     const struct m0_fid      *fid)
{
	int j;
	return m0_exists(i, pm->pm_state->pst_nr_devices,
			 m0_fid_eq(&pm->pm_state->pst_devices_array[j=i].pd_id,
				   fid)) ? j : POOL_DEVICE_INDEX_INVALID;
}

static void pool_device_state_last_revert(struct m0_pools_common *pc,
					  struct m0_fid          *dev_fid,
					  struct m0_poolmach     *pm_stop)
{
	struct m0_pool         *pool;
	struct m0_pool_version *pver;
	struct m0_poolmach     *pm;
	int                     dev_idx;

	m0_tl_for(pools, &pc->pc_pools, pool) {
		m0_tl_for(pool_version, &pool->po_vers, pver) {
			pm = &pver->pv_mach;
			if (pm == pm_stop)
				return;
			dev_idx = pool_device_index(pm, dev_fid);
			if (dev_idx != POOL_DEVICE_INDEX_INVALID)
				m0_poolmach_state_last_cancel(pm);
		} m0_tl_endfor;
	} m0_tl_endfor;
}

/**
 * Iterate over all pool versions and update corresponding poolmachines
 * containing provided disk.
 */
M0_INTERNAL int m0_pool_device_state_update(struct m0_reqh        *reqh,
					    struct m0_be_tx       *tx,
					    struct m0_fid         *dev_fid,
					    enum m0_pool_nd_state  new_state)
{
	struct m0_pools_common   *pc = reqh->rh_pools;
	struct m0_pool           *pool;
	struct m0_pool_version   *pver;
	struct m0_poolmach       *pm;
	int                       rc;
	int                       dev_idx;
	struct m0_poolmach_event  pme;

	m0_tl_for(pools, &pc->pc_pools, pool) {
		m0_tl_for(pool_version, &pool->po_vers, pver) {
			pm = &pver->pv_mach;
			dev_idx = pool_device_index(pm, dev_fid);
			if (dev_idx != POOL_DEVICE_INDEX_INVALID) {
				pme.pe_type  = M0_POOL_DEVICE;
				pme.pe_index = dev_idx;
				pme.pe_state = new_state;
				rc = m0_poolmach_state_transit(pm, &pme);
				if (rc != 0) {
					pool_device_state_last_revert(pc,
								      dev_fid,
								      pm);
					return M0_ERR(rc);
				}
			}
		} m0_tl_endfor;
	} m0_tl_endfor;

	return M0_RC(0);
}

#endif /* !__KERNEL__ */

M0_INTERNAL void m0_pools_lock(struct m0_pools_common *pc)
{
	m0_mutex_lock(&pc->pc_mutex);
}

M0_INTERNAL void m0_pools_unlock(struct m0_pools_common *pc)
{
	m0_mutex_unlock(&pc->pc_mutex);
}

M0_INTERNAL bool m0_pools_is_locked(struct m0_pools_common *pc)
{
	return m0_mutex_is_locked(&pc->pc_mutex);
}

/** @} end group pool */
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
