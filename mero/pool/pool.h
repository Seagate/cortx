/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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

#pragma once

#ifndef __MERO_POOL_POOL_H__
#define __MERO_POOL_POOL_H__

#include "format/format.h"     /* m0_format_header */
#include "lib/chan.h"          /* m0_clink */
#include "lib/rwlock.h"
#include "lib/tlist.h"
#include "lib/tlist_xc.h"
#include "fd/fd.h"             /* m0_fd_tile */
#include "reqh/reqh_service.h" /* m0_reqh_service_ctx */
#include "conf/obj.h"
#include "layout/pdclust.h"    /* m0_pdclust_attr */
#include "pool/pool_machine.h"
#include "pool/pool_machine_xc.h"
#include "pool/policy.h"       /* m0_pver_policy_code */

/**
   @defgroup pool Storage pools.

   @{
 */

/* import */
struct m0_io_req;

/* export */
struct m0_pool;
struct m0_pool_spare_usage;
struct m0_confc_update_state;

enum {
	PM_DEFAULT_NR_NODES = 10,
	PM_DEFAULT_NR_DEV = 80,
	PM_DEFAULT_MAX_NODE_FAILURES = 1,
	PM_DEFAULT_MAX_DEV_FAILURES = 80,
	POOL_MAX_RPC_NR_IN_FLIGHT = 100
};

enum map_type {
	IOS = 1,
	MDS
};

enum {
	PV_SNS_DIRTY = 1 << 0,
};

struct m0_pool_device_to_service {
	/** Service context for the target disk. */
	struct m0_reqh_service_ctx *pds_ctx;

	/** Sdev fid associated with the service context. */
	struct m0_fid               pds_sdev_fid;
};

struct m0_pool {
	struct m0_fid          po_id;

	/** List of pool versions in this pool. */
	struct m0_tl           po_vers;

	/** Linkage into list of pools. */
	struct m0_tlink        po_linkage;

	/**
	 * List of failed devices in the pool.
	 * @see m0_pool::pd_fail_linkage
	 */
	struct m0_tl           po_failed_devices;

	/** Pool version selection policy. */
	struct m0_pver_policy *po_pver_policy;
	uint64_t               po_magic;
};

/**
 * Pool version is the subset of devices from the filesystem.
 * Pool version is associated with a pool machine and contains
 * a device to ioservice map.
 */
struct m0_pool_version {
	struct m0_fid                pv_id;

	/** pool version ditry flag.*/
	bool                         pv_is_dirty;

	/** Is pool version present in the conf cache? */
	bool                         pv_is_stale;

	/** Layout attributes associated with this pool version. */
	struct m0_pdclust_attr       pv_attr;

	/** Total number of nodes in this pool version. */
	uint32_t                     pv_nr_nodes;

	/** Base pool of this pool version. */
	struct m0_pool              *pv_pool;

	struct m0_pools_common      *pv_pc;

	/** Pool machine associated with this pool version. */
	struct m0_poolmach           pv_mach;

	/* The fault tolerant tile associated with the pool version. */
	struct m0_fd_tile            pv_fd_tile;

	/* Failure domains tree associated with the pool version. */
	struct m0_fd_tree            pv_fd_tree;
	/** The tolerance vector associated with the pool version. */
	uint32_t                     pv_fd_tol_vec[M0_CONF_PVER_HEIGHT];

	uint32_t                     pv_sns_flags;

	/**
	 * Linkage into list of pool versions.
	 * @see struct m0_pool::po_vers
	 */
	struct m0_tlink              pv_linkage;

	/** M0_POOL_VERSION_MAGIC */
	uint64_t                     pv_magic;
};

/**
 * Contains resources that are shared among the pools in the filesystem.
 * In-memory references for members should be used under pc_mutex lock as
 * due to configuration expiration they may be updated, and long term users
 * should subscribe with rconfc update channels.
 */
struct m0_pools_common {
	struct m0_tl                      pc_pools;

	struct m0_confc                  *pc_confc;

	struct m0_rpc_machine            *pc_rmach;

	/**
	  List of m0_reqh_service_ctx objects hanging using sc_link.
	  tlist descriptor: svc_ctx_tl
	  */
	struct m0_tl                      pc_svc_ctxs;

	/**
	  Array of pools_common_svc_ctx_tlist_length() valid elements.
	  The array size is same as the total number of service contexts,
	  pc_mds_map[i] points to m0_reqh_service_ctx of mdservice whose
	  index is i.
	  */
	struct m0_reqh_service_ctx      **pc_mds_map;

	/** RM service context */
	struct m0_reqh_service_ctx       *pc_rm_ctx;

	/**
	 * Each ith element in the array gives the total number of services
	 * of its corresponding type, e.g. element at M0_CST_MDS gives number
	 * of meta-data services in the filesystem.
	 */
	uint64_t                          pc_nr_svcs[M0_CST_NR];

	/**
	 * Total number of devices across all the pools.
	 * Only devices used by IOS or CAS services are accounted.
	 */
	uint32_t                          pc_nr_devices;

	/**
	 * An array of size of pc_nr_devices.
	 * Maps device to IOS/CAS service: dev_idx -> (service_ctx, sdev_fid)
	 * Each pc_dev2svc[i] entry points to instance of
	 * struct m0_reqh_service_ctx which has established rpc connections
	 * with the given service endpoints.
	 * @todo Check whether concurrency needs to be handled after
	 * MERO-1498 is in master.
	 */
	struct m0_pool_device_to_service *pc_dev2svc;

	/** Metadata redundancy count. */
	uint32_t                          pc_md_redundancy;
	/** Pool of ioservices used to store meta data cobs. */
	struct m0_pool                   *pc_md_pool;
	/** Layout instance of the mdpool. */
	struct m0_layout_instance        *pc_md_pool_linst;

	struct m0_ha_entrypoint_client   *pc_ha_ecl;
	struct m0_clink                   pc_ha_clink;
	struct m0_mutex                   pc_rm_lock;
	/** XXX: used only for Clovis UTs now and should be dropped. */
	struct m0_pool_version           *pc_cur_pver;
	struct m0_mutex                   pc_mutex;

	/**
	 * Service contexts that were obsoleted by configuration updates.
	 *
	 * List of m0_reqh_service_ctx-s, linked through .sc_link field.
	 */
	struct m0_tl                      pc_abandoned_svc_ctxs;
	/** Listener for configuration expiration event from rconfc. */
	struct m0_clink                   pc_conf_exp;
	/**
	 * Listener for configuration ready event from rconfc to be run
	 * asynchronously.
	 */
	struct m0_clink                   pc_conf_ready_async;
	/** Pool of cas services used to store dix. */
	struct m0_pool                   *pc_dix_pool;
};

M0_TL_DESCR_DECLARE(pools_common_svc_ctx, M0_EXTERN);
M0_TL_DECLARE(pools_common_svc_ctx, M0_EXTERN, struct m0_reqh_service_ctx);

M0_TL_DESCR_DECLARE(pool_version, M0_EXTERN);
M0_TL_DECLARE(pool_version, M0_EXTERN, struct m0_pool_version);

M0_TL_DESCR_DECLARE(pools, M0_EXTERN);
M0_TL_DECLARE(pools, M0_EXTERN, struct m0_pool);

M0_TL_DESCR_DECLARE(pool_failed_devs, M0_EXTERN);
M0_TL_DECLARE(pool_failed_devs, M0_EXTERN, struct m0_pooldev);

M0_INTERNAL int m0_pools_common_init(struct m0_pools_common *pc,
				      struct m0_rpc_machine *rmach);

/* This internal API is only used by pool implementation and m0t1fs UT. */
M0_INTERNAL int m0__pools_common_init(struct m0_pools_common *pc,
				      struct m0_rpc_machine *rmach,
				      struct m0_conf_root *root);

M0_INTERNAL void m0_pools_common_fini(struct m0_pools_common *pc);

M0_INTERNAL bool m0_pools_common_conf_ready_async_cb(struct m0_clink *clink);

M0_INTERNAL int m0_pools_service_ctx_create(struct m0_pools_common *pc);
M0_INTERNAL void m0_pools_service_ctx_destroy(struct m0_pools_common *pc);

M0_INTERNAL int m0_pool_init(struct m0_pool *pool, const struct m0_fid *id,
			     enum m0_pver_policy_code pver_policy);
M0_INTERNAL void m0_pool_fini(struct m0_pool *pool);

/**
 * Initialises pool version from configuration data.
 */
M0_INTERNAL int m0_pool_version_init_by_conf(struct m0_pool_version *pv,
					     struct m0_conf_pver *pver,
					     struct m0_pool *pool,
					     struct m0_pools_common *pc);

M0_INTERNAL int m0_pool_version_init(struct m0_pool_version *pv,
				     const struct m0_fid *id,
				     struct m0_pool *pool,
				     uint32_t pool_width,
				     uint32_t nodes,
				     uint32_t nr_data,
				     uint32_t nr_failures);

/**
 * Gets pool version from in-memory list of pools (pc->pc_pools).
 *
 * @param pool - if not NULL, get the version of it, otherwise,
 *               select the pool as per internally defined policy.
 *
 * @see m0_conf_pver_get(), m0_conf_pver_find()
 */
M0_INTERNAL int m0_pool_version_get(struct m0_pools_common  *pc,
				    const struct m0_fid     *pool,
				    struct m0_pool_version **pv);

M0_INTERNAL struct m0_pool_version *
m0_pool_version_lookup(const struct m0_pools_common *pc,
		       const struct m0_fid          *id);

M0_INTERNAL struct m0_pool_version *
m0_pool_version_find(struct m0_pools_common *pc, const struct m0_fid *id);

M0_INTERNAL void m0_pool_version_fini(struct m0_pool_version *pv);

M0_INTERNAL int m0_pool_versions_init_by_conf(struct m0_pool *pool,
					      struct m0_pools_common *pc,
					      const struct m0_conf_pool *cp,
					      struct m0_sm_group *sm_grp,
					      struct m0_dtm *dtm);

M0_INTERNAL void m0_pool_versions_fini(struct m0_pool *pool);

M0_INTERNAL void m0_pool_versions_stale_mark(struct m0_pools_common *pc,
					     struct m0_confc_update_state *s);
M0_INTERNAL struct m0_pool_version *
m0_pool_version_md_get(const struct m0_pools_common *pc);

M0_INTERNAL int m0_pools_init(void);
M0_INTERNAL void m0_pools_fini(void);

/**
 * Setups pools at @pc. If @profile is NULL - use all
 * pools available in configuration. Otherwise, only
 * those that belongs to the specified @profile.
 * @note clients should specify @profile.
 */
M0_INTERNAL int m0_pools_setup(struct m0_pools_common *pc,
			       const struct m0_fid    *profile,
			       struct m0_sm_group     *sm_grp,
			       struct m0_dtm          *dtm);

M0_INTERNAL void m0_pools_destroy(struct m0_pools_common *pc);

M0_INTERNAL int m0_pool_versions_setup(struct m0_pools_common *pc);

/**
 * Appends in-memory pool version to pool versions list.
 * It dynamically generate new pool version using base pool version
 * and current failed set.
 */
M0_INTERNAL int m0_pool_version_append(struct m0_pools_common *pc,
				       struct m0_conf_pver *pver,
				       struct m0_pool_version **pv);

M0_INTERNAL void m0_pool_versions_destroy(struct m0_pools_common *pc);

M0_INTERNAL struct m0_pool_version *
m0_pool_clean_pver_find(const struct m0_pool *pool);

/** Generates layout id from pool version fid */
M0_INTERNAL uint64_t
m0_pool_version2layout_id(const struct m0_fid *pv_fid, uint64_t lid);

/**
 * Creates service contexts from given struct m0_conf_service.
 * Creates service context for each endpoint in m0_conf_service::cs_endpoints.
 */
M0_INTERNAL struct m0_rpc_session *
m0_pools_common_active_rm_session(struct m0_pools_common *pc);

M0_INTERNAL struct m0_reqh_service_ctx *
m0_pools_common_service_ctx_find(const struct m0_pools_common *pc,
				 const struct m0_fid *id,
				 enum m0_conf_service_type type);
M0_INTERNAL void
m0_pools_common_service_ctx_connect_sync(struct m0_pools_common *pc);

/**
 * pool node. Data structure representing a node in a pool.
 *
 * Pool node and pool server are two different views of the same physical
 * entity. A pool node is how a server looks "externally" to other nodes.
 * "struct poolnode" represents a particular server on other servers. E.g.,
 * when a new server is added to the pool, "struct poolnode" is created on
 * every server in the pool. "struct poolserver", on the other hand, represents
 * a server state machine locally on the server where it runs.
 *
 * @see pool server
 */
struct m0_poolnode {
	struct m0_format_header pn_header;
	uint32_t                pn_state M0_XCA_FENUM(m0_pool_nd_state);
	char                    pn_pad[4];
	/** Pool node identity. */
	struct m0_fid           pn_id;
	struct m0_format_footer pn_footer;
};
M0_BASSERT(sizeof(enum m0_pool_nd_state) == 4);

enum m0_poolnode_format_version {
	M0_POOLNODE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_POOLNODE_FORMAT_VERSION */
	/*M0_POOLNODE_FORMAT_VERSION_2,*/
	/*M0_POOLNODE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_POOLNODE_FORMAT_VERSION = M0_POOLNODE_FORMAT_VERSION_1
};

/** Storage device in a pool. */
struct m0_pooldev {
	struct m0_format_header pd_header;
	/** device state (as part of pool machine state). This field is only
	    meaningful when m0_pooldev::pd_node.pn_state is PNS_ONLINE */
	uint32_t                pd_state M0_XCA_FENUM(m0_pool_nd_state);
	char                    pd_pad[4];
	/** pool device identity */
	struct m0_fid           pd_id;
	/** storage device fid */
	struct m0_fid           pd_sdev_fid;
	/** pooldev index in the poolmachine-state device array*/
	uint32_t                pd_index;
	/**
	 * storage device index between 1 to total number of devices in the
	 * filesystem, get from m0_conf_sdev:sd_dev_idx.
	 */
	uint32_t                pd_sdev_idx;

	/** a node this storage device is attached to */
	struct m0_poolnode     *pd_node;
	/** pool machine this pooldev belongs to */
	struct m0_poolmach     *pd_pm;
	/**
	 * Link to receive HA state change notification. This will wait on
	 * disk obj's wait channel i.e. m0_conf_obj::co_ha_chan.
	 */
	struct m0_be_clink      pd_clink;

	/**
	 * Link into list of failed devices in the pool.
	 * @see m0_pool::po_failed_devices.
	 */
	struct m0_tlink         pd_fail_linkage;
	uint64_t                pd_magic;

	struct m0_format_footer pd_footer;
};

enum m0_pooldev_format_version {
	M0_POOLDEV_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_POOLDEV_FORMAT_VERSION */
	/*M0_POOLDEV_FORMAT_VERSION_2,*/
	/*M0_POOLDEV_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_POOLDEV_FORMAT_VERSION = M0_POOLDEV_FORMAT_VERSION_1
};

/**
 * Tracking spare slot usage.
 * If spare slot is not used for repair/rebalance, its :psp_device_index is -1.
 */
struct m0_pool_spare_usage {
	struct m0_format_header psu_header;
	/**
	 * Index of the device from m0_poolmach_state::pst_devices_array in the
	 * pool associated with this spare slot.
	 */
	uint32_t                psu_device_index;

	/** state of the device to use this spare slot */
	uint32_t                psu_device_state M0_XCA_FENUM(m0_pool_nd_state);
	struct m0_format_footer psu_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_pool_spare_usage_format_version {
	M0_POOL_SPARE_USAGE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_POOL_SPARE_USAGE_FORMAT_VERSION */
	/*M0_POOL_SPARE_USAGE_FORMAT_VERSION_2,*/
	/*M0_POOL_SPARE_USAGE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_POOL_SPARE_USAGE_FORMAT_VERSION = M0_POOL_SPARE_USAGE_FORMAT_VERSION_1
};

/** @} end group pool */

/**
   @defgroup servermachine Server machine
   @{
*/

/**
   resource limit

   Data structure to describe the fraction of resource usage limitation:
   0  : resource cannot be used at all.
   100: resource can be used entirely without limitation.
   0 < value < 100: fraction of resources can be used.
*/
struct m0_rlimit {
       int rl_processor_throughput;
       int rl_memory;
       int rl_storage_throughput;
       int rl_network_throughput;
};

/**
   pool server

   Pool server represents a pool node plus its state machines, lives locally on
   the server where it runs.

   @see pool node
*/
struct m0_poolserver {
	struct m0_poolnode      ps_node;
	/* struct m0_persistent_sm ps_mach; */
	struct m0_rlimit	ps_rl_usage; /**< the current resource usage */
};

M0_INTERNAL int m0_poolserver_init(struct m0_poolserver *srv);
M0_INTERNAL void m0_poolserver_fini(struct m0_poolserver *srv);
M0_INTERNAL int m0_poolserver_reset(struct m0_poolserver *srv);
M0_INTERNAL int m0_poolserver_on(struct m0_poolserver *srv);
M0_INTERNAL int m0_poolserver_off(struct m0_poolserver *srv);
M0_INTERNAL int m0_poolserver_io_req(struct m0_poolserver *srv,
				     struct m0_io_req *req);
M0_INTERNAL int m0_poolserver_device_join(struct m0_poolserver *srv,
					  struct m0_pooldev *dev);
M0_INTERNAL int m0_poolserver_device_leave(struct m0_poolserver *srv,
					   struct m0_pooldev *dev);

/**
 * Find out device ids of the REPAIRED devices in the given pool machine
 * and call m0_mero_stob_reopen() on each of them.
 */
M0_INTERNAL int m0_pool_device_reopen(struct m0_poolmach *pm,
				      struct m0_reqh *rs_reqh);

/**
 * Iterate over all pool versions and update corresponding poolmachines
 * containing provided disk. Also updates ios disk state in ios poolmachine.
 */
M0_INTERNAL int m0_pool_device_state_update(struct m0_reqh        *reqh,
					    struct m0_be_tx       *tx,
					    struct m0_fid         *dev_fid,
					    enum m0_pool_nd_state  new_state);

/**
 * State of SNS repair with respect to given global fid.
 * Used during degraded mode write IO.
 * During normal IO, the UNINITIALIZED enum value is used.
 * The next 2 states are used during degraded mode write IO.
 */
enum sns_repair_state {
	/**
	 * Used by IO requests done during healthy state of storage pool.
	 * Initialized to -1 in order to sync it with output of API
	 * m0_sns_cm_fid_repair_done().
	 * */
	SRS_UNINITIALIZED = 1,

	/**
	 * Assumes a distributed lock has been acquired on the associated
	 * global fid and SNS repair is yet to start on given global fid.
	 */
	SRS_REPAIR_NOTDONE,

	/**
	 * Assumes a distributed lock has been acquired on associated
	 * global fid and SNS repair has completed for given fid.
	 */
	SRS_REPAIR_DONE,

	SRS_NR,
};

/**
 * Register clink of pooldev to disk conf object's wait channel
 * to receive HA notifications.
 */
M0_INTERNAL void m0_pooldev_clink_add(struct m0_clink *link,
				      struct m0_chan  *chan);
/**
 * Delete clink of pooldev
 */
M0_INTERNAL void m0_pooldev_clink_del(struct m0_clink *cl);

M0_INTERNAL uint32_t m0_ha2pm_state_map(enum m0_ha_obj_state hastate);

/**
 * Converts numeric device state to string representation.
 */
M0_INTERNAL const char *m0_pool_dev_state_to_str(enum m0_pool_nd_state state);
M0_INTERNAL struct m0_pool *m0_pool_find(struct m0_pools_common *pc,
					 const struct m0_fid *pool);

M0_INTERNAL void m0_pools_lock(struct m0_pools_common *pc);
M0_INTERNAL void m0_pools_unlock(struct m0_pools_common *pc);
M0_INTERNAL bool m0_pools_is_locked(struct m0_pools_common *pc);

/** @} end of servermachine group */
#endif /* __MERO_POOL_POOL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
