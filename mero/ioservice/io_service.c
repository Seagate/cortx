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
 * Original author: Rajanikant Chirmade <Rajanikant_Chirmade@xyratex.com>
 * Original creation date: 11/02/2011
 */

/**
   @addtogroup DLD_bulk_server_fspec_ios_operations
   @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/tlist.h"
#include "lib/locality.h"
#include "lib/misc.h"
#include "mero/magic.h"
#include "rpc/rpc.h"
#include "rpc/rpclib.h"
#include "net/buffer_pool.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"
#include "ioservice/io_fops.h"
#include "ioservice/io_service.h"
#include "ioservice/ios_start_sm.h"
#include "pool/pool.h"
#include "net/lnet/lnet.h"
#include "mdservice/md_fops.h"
#include "layout/layout.h"
#include "layout/pdclust.h"
#include "mdservice/fsync_fops.h"
#include "module/instance.h"       /* m0_get */
#include "ioservice/fid_convert.h" /* m0_fid_convert_gob2cob */

M0_TL_DESCR_DEFINE(bufferpools, "rpc machines associated with reqh",
		   M0_INTERNAL,
                   struct m0_rios_buffer_pool, rios_bp_linkage, rios_bp_magic,
                   M0_IOS_BUFFER_POOL_MAGIC, M0_IOS_BUFFER_POOL_HEAD_MAGIC);
M0_TL_DEFINE(bufferpools, M0_INTERNAL, struct m0_rios_buffer_pool);


/**
 * These values are supposed to be fetched from configuration cache. Since
 * configuration cache module is not available, these values are defined as
 * a static variable.
 * @see m0_ios_net_buffer_pool_size_set()
 */
static uint32_t ios_net_buffer_pool_size = 32;

/**
 * Key for ios mds connection.
 */
static unsigned ios_mds_conn_key = 0;

static int ios_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype);
static void ios_fini(struct m0_reqh_service *service);

static int ios_start(struct m0_reqh_service *service);
static int ios_start_async(struct m0_reqh_service_start_async_ctx *asc);
static bool ios_start_async_cb(struct m0_clink *clink);
static void ios_prepare_to_stop(struct m0_reqh_service *service);
static void ios_stop(struct m0_reqh_service *service);

static void buffer_pool_not_empty(struct m0_net_buffer_pool *bp);
static void buffer_pool_low(struct m0_net_buffer_pool *bp);

/**
 * I/O Service type operations.
 */
static const struct m0_reqh_service_type_ops ios_type_ops = {
	.rsto_service_allocate = ios_allocate
};

/**
 * I/O Service operations.
 */
static const struct m0_reqh_service_ops ios_ops = {
	.rso_start           = ios_start,
	.rso_start_async     = ios_start_async,
	.rso_prepare_to_stop = ios_prepare_to_stop,
	.rso_stop            = ios_stop,
	.rso_fini            = ios_fini
};

/**
 * Buffer pool operations.
 */
struct m0_net_buffer_pool_ops buffer_pool_ops = {
	.nbpo_not_empty       = buffer_pool_not_empty,
	.nbpo_below_threshold = buffer_pool_low,
};

struct m0_reqh_service_type m0_ios_type = {
	.rst_name     = "M0_CST_IOS",
	.rst_ops      = &ios_type_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_IOS,
};

/**
 * Buffer pool operation function. This function gets called when buffer pool
 * becomes non empty.
 * It sends signal to FOM waiting for network buffer.
 *
 * @param bp buffer pool pointer.
 * @pre bp != NULL
 */
static void buffer_pool_not_empty(struct m0_net_buffer_pool *bp)
{
        struct m0_rios_buffer_pool *buffer_desc;

	M0_PRE(bp != NULL);

	buffer_desc = container_of(bp, struct m0_rios_buffer_pool, rios_bp);
	m0_chan_signal(&buffer_desc->rios_bp_wait);
}

/**
 * Buffer pool operation function.
 * This function gets called when network buffer availability hits
 * lower threshold.
 *
 * @param bp buffer pool pointer.
 * @pre bp != NULL
 */
static void buffer_pool_low(struct m0_net_buffer_pool *bp)
{
	/*
	 * Currently ioservice is ignoring this signal.
	 * But in future io_service may grow
	 * buffer pool depending on some policy.
	 */
}

/**
 * Registers I/O service with mero node.
 * Mero setup calls this function.
 */
M0_INTERNAL int m0_ios_register(void)
{
	int rc;

	rc = m0_ioservice_fop_init();
	if (rc != 0)
		return M0_ERR_INFO(rc, "Unable to initialize fops");
	m0_reqh_service_type_register(&m0_ios_type);
	m0_get()->i_ios_cdom_key = m0_reqh_lockers_allot();
	ios_mds_conn_key = m0_reqh_lockers_allot();
	return M0_RC(rc);
}

/**
 * Unregisters I/O service from mero node.
 */
M0_INTERNAL void m0_ios_unregister(void)
{
	m0_reqh_lockers_free(ios_mds_conn_key);
	m0_reqh_lockers_free(m0_get()->i_ios_cdom_key);

	m0_reqh_service_type_unregister(&m0_ios_type);
	m0_ioservice_fop_fini();
}

M0_INTERNAL bool m0_reqh_io_service_invariant(const struct m0_reqh_io_service
					      *rios)
{
	return rios->rios_magic == M0_IOS_REQH_SVC_MAGIC;
}

/**
 * Create & initialise instance of buffer pool per domain.
 * 1. This function scans rpc_machines from request handler
 *    and creates buffer pool instance for each network domain.
 *    It also creates color map for transfer machines for
 *    respective domain.
 * 2. Initialises all buffer pools and make provision for
 *    configured number of buffers for each buffer pool.
 *
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
M0_INTERNAL int m0_ios_create_buffer_pool(struct m0_reqh_service *service)
{
	int                         nbuffs;
	int                         colours;
	int                         rc = 0;
	struct m0_rpc_machine      *rpcmach;
	struct m0_reqh_io_service  *serv_obj;
	m0_bcount_t                 segment_size;
	uint32_t                    segments_nr;
	struct m0_reqh             *reqh;

	serv_obj = container_of(service, struct m0_reqh_io_service, rios_gen);
	M0_ASSERT(m0_reqh_io_service_invariant(serv_obj));

	reqh = service->rs_reqh;
	m0_rwlock_read_lock(&reqh->rh_rwlock);
	m0_tl_for(m0_reqh_rpc_mach, &reqh->rh_rpc_machines, rpcmach) {
		M0_ASSERT(m0_rpc_machine_bob_check(rpcmach));
		struct m0_rios_buffer_pool *newbp;
		/*
		 * Check buffer pool for network domain of rpc_machine
		 */
		if (m0_tl_exists(bufferpools, bp, &serv_obj->rios_buffer_pools,
				 bp->rios_ndom == rpcmach->rm_tm.ntm_dom))
			continue;

		/* Buffer pool for network domain not found, create one */
		M0_ALLOC_PTR(newbp);
		if (newbp == NULL)
			return M0_ERR(-ENOMEM);

		newbp->rios_ndom = rpcmach->rm_tm.ntm_dom;
		newbp->rios_bp_magic = M0_IOS_BUFFER_POOL_MAGIC;

		colours = m0_list_length(&newbp->rios_ndom->nd_tms);

		segment_size = m0_rpc_max_seg_size(newbp->rios_ndom);
		segments_nr  = m0_rpc_max_segs_nr(newbp->rios_ndom);

		M0_LOG(M0_DEBUG, "ios segments_nr=%d", segments_nr);
		rc = m0_net_buffer_pool_init(&newbp->rios_bp,
					      newbp->rios_ndom,
					      M0_NET_BUFFER_POOL_THRESHOLD,
					      segments_nr, segment_size,
					      colours, M0_0VEC_SHIFT,
					      /* dont_dump */true);
		if (rc != 0) {
			m0_free(newbp);
			break;
		}

		newbp->rios_bp.nbp_ops = &buffer_pool_ops;
		/*
		 * Initialise channel for sending availability of buffers
		 * with buffer pool to I/O FOMs.
		 */
		m0_chan_init(&newbp->rios_bp_wait, &newbp->rios_bp.nbp_mutex);

		/* Pre-allocate network buffers */
		m0_net_buffer_pool_lock(&newbp->rios_bp);
		nbuffs = m0_net_buffer_pool_provision(&newbp->rios_bp,
						      ios_net_buffer_pool_size);
		m0_net_buffer_pool_unlock(&newbp->rios_bp);
		if (nbuffs < ios_net_buffer_pool_size) {
			rc = -ENOMEM;
			m0_chan_fini_lock(&newbp->rios_bp_wait);
			m0_net_buffer_pool_fini(&newbp->rios_bp);
			m0_free(newbp);
			break;
		}

		bufferpools_tlink_init(newbp);
		bufferpools_tlist_add(&serv_obj->rios_buffer_pools, newbp);

	} m0_tl_endfor; /* rpc_machines */
	m0_rwlock_read_unlock(&reqh->rh_rwlock);

	return M0_RC(rc);
}

/**
 * Delete instances of buffer pool.
 * It go through buffer pool list and delete the instance.
 *
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
M0_INTERNAL void m0_ios_delete_buffer_pool(struct m0_reqh_service *service)
{
	struct m0_reqh_io_service  *serv_obj;
	struct m0_rios_buffer_pool *bp;

	M0_PRE(service != NULL);

	serv_obj = container_of(service, struct m0_reqh_io_service, rios_gen);
	M0_ASSERT(m0_reqh_io_service_invariant(serv_obj));

	m0_tl_for(bufferpools, &serv_obj->rios_buffer_pools, bp) {

		M0_ASSERT(bp != NULL);

		m0_chan_fini_lock(&bp->rios_bp_wait);
		bufferpools_tlink_del_fini(bp);
		m0_net_buffer_pool_fini(&bp->rios_bp);
		m0_free(bp);

	} m0_tl_endfor; /* bufferpools */

	bufferpools_tlist_fini(&serv_obj->rios_buffer_pools);
}

/**
 * Allocates and initiates I/O Service instance.
 * This operation allocates & initiates service instance with its operation
 * vector.
 *
 * @param stype service type
 * @param service pointer to service instance.
 *
 * @pre stype != NULL && service != NULL
 */
static int ios_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_io_service *ios;

	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(ios);
	if (ios == NULL)
		return M0_ERR(-ENOMEM);

	bufferpools_tlist_init(&ios->rios_buffer_pools);
	ios->rios_magic = M0_IOS_REQH_SVC_MAGIC;

	*service = &ios->rios_gen;
	(*service)->rs_ops = &ios_ops;

	return 0;
}

/**
 * Finalise I/O Service instance.
 * This operation finalises service instance and de-allocate it.
 *
 * @param service pointer to service instance.
 *
 * @pre service != NULL
 */
static void ios_fini(struct m0_reqh_service *service)
{
	struct m0_reqh_io_service *serv_obj;

	M0_PRE(service != NULL);

	serv_obj = container_of(service, struct m0_reqh_io_service, rios_gen);
	M0_ASSERT(m0_reqh_io_service_invariant(serv_obj));

	m0_free(serv_obj);
}

static int ios_start(struct m0_reqh_service *service)
{
	int                        rc;
	struct m0_reqh_io_service *iosvc;
	struct m0_sm_group        *sm_grp = m0_locality0_get()->lo_grp;

	M0_PRE(service != NULL);

	iosvc = container_of(service, struct m0_reqh_io_service, rios_gen);
	rc = m0_ios_start_sm_init(&iosvc->rios_sm, &iosvc->rios_gen, sm_grp);
	if (rc != 0)
		return M0_ERR(rc);

	m0_ios_start_sm_exec(&iosvc->rios_sm);
	m0_ios_start_lock(&iosvc->rios_sm);
	rc = m0_sm_timedwait(&iosvc->rios_sm.ism_sm,
			     M0_BITS(M0_IOS_START_COMPLETE,
				     M0_IOS_START_FAILURE),
			     M0_TIME_NEVER);
	m0_ios_start_unlock(&iosvc->rios_sm);
	rc = rc ?: iosvc->rios_sm.ism_sm.sm_rc;
	iosvc->rios_cdom = (rc == 0) ? iosvc->rios_sm.ism_dom : NULL;
	m0_sm_group_lock(sm_grp);
	m0_ios_start_sm_fini(&iosvc->rios_sm);
	m0_sm_group_unlock(sm_grp);
	M0_LOG(M0_DEBUG, "io cob domain %p", iosvc->rios_cdom);
	return M0_RC(rc);
}

static int ios_start_async(struct m0_reqh_service_start_async_ctx *asc)
{
	struct m0_reqh_service    *service = asc->sac_service;
	struct m0_reqh_io_service *serv_obj;
	struct m0_sm_group        *grp = m0_locality0_get()->lo_grp;

	M0_ENTRY();
	M0_PRE(service != NULL);
	M0_PRE(m0_reqh_service_state_get(service) == M0_RST_STARTING);

	serv_obj = container_of(service, struct m0_reqh_io_service, rios_gen);

	asc->sac_rc = m0_ios_start_sm_init(&serv_obj->rios_sm, service, grp);
	if (asc->sac_rc != 0) {
		m0_fom_wakeup(asc->sac_fom);
		return M0_ERR(asc->sac_rc);
	}

	serv_obj->rios_fom = asc->sac_fom;
	m0_clink_init(&serv_obj->rios_clink, ios_start_async_cb);
	m0_clink_add_lock(&serv_obj->rios_sm.ism_sm.sm_chan,
			  &serv_obj->rios_clink);
	m0_ios_start_sm_exec(&serv_obj->rios_sm);
	return M0_RC(0);
}

static bool ios_start_async_cb(struct m0_clink *clink)
{
	struct m0_reqh_io_service *iosvc;
	struct m0_ios_start_sm    *ios_sm;
	int                        rc;

	iosvc = container_of(clink, struct m0_reqh_io_service, rios_clink);
	ios_sm = &iosvc->rios_sm;
	if (M0_IN(ios_sm->ism_sm.sm_state,
		   (M0_IOS_START_COMPLETE, M0_IOS_START_FAILURE))) {
		m0_clink_del(clink);
		m0_clink_fini(clink);
		rc = ios_sm->ism_sm.sm_rc;
		iosvc->rios_cdom = (rc == 0) ? ios_sm->ism_dom : NULL;
		m0_ios_start_sm_fini(ios_sm);
		m0_fom_wakeup(iosvc->rios_fom);
	}
	return true;
}

static void ios_prepare_to_stop(struct m0_reqh_service *service)
{
	M0_LOG(M0_DEBUG, "ioservice PREPARE ......");
	m0_ios_mds_conn_fini(service->rs_reqh);
	M0_LOG(M0_DEBUG, "ioservice PREPARE STOPPED");
}

static void ios_stop(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);

	m0_ios_delete_buffer_pool(service);
	m0_ios_cdom_fini(service->rs_reqh);
	m0_reqh_lockers_clear(service->rs_reqh, m0_get()->i_ios_cdom_key);
	M0_LOG(M0_DEBUG, "ioservice STOPPED");
}

/**
 * @todo: This function is used by copy machine module, but not used by IO
 * service.
 * Corresponding ticket: MERO-1190.
 */
M0_INTERNAL void m0_ios_cdom_get(struct m0_reqh *reqh,
				 struct m0_cob_domain **out)
{
	M0_PRE(reqh != NULL);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	*out = m0_reqh_lockers_get(reqh, m0_get()->i_ios_cdom_key);
	M0_ASSERT(*out != NULL);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);
}

M0_INTERNAL void m0_ios_cdom_fini(struct m0_reqh *reqh)
{
	struct m0_cob_domain *cdom;

	M0_PRE(reqh != NULL);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	cdom = m0_reqh_lockers_get(reqh, m0_get()->i_ios_cdom_key);
	m0_cob_domain_fini(cdom);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);
}

enum {
	RPC_TIMEOUT          = 8, /* seconds */
	MAX_NR_RPC_IN_FLIGHT = 100,
};

M0_TL_DESCR_DECLARE(cs_eps, extern);
M0_TL_DECLARE(cs_eps, M0_INTERNAL, struct cs_endpoint_and_xprt);

static int m0_ios_mds_conn_init(struct m0_reqh             *reqh,
				struct m0_ios_mds_conn_map *conn_map)
{
	struct m0_mero              *mero;
	struct m0_rpc_machine       *rpc_machine;
	const char                  *srv_ep_addr;
	struct cs_endpoint_and_xprt *ep;
	int                          rc = 0;
	struct m0_ios_mds_conn      *conn;
	M0_ENTRY();

	M0_PRE(reqh != NULL);
	mero = m0_cs_ctx_get(reqh);
	rpc_machine = m0_reqh_rpc_mach_tlist_head(&reqh->rh_rpc_machines);
	M0_ASSERT(mero != NULL);
	M0_ASSERT(rpc_machine != NULL);
	conn_map->imc_nr = 0;

	m0_tl_for(cs_eps, &mero->cc_mds_eps, ep) {
		M0_ASSERT(cs_endpoint_and_xprt_bob_check(ep));
		M0_ASSERT(ep->ex_scrbuf != NULL);
		srv_ep_addr = ep->ex_endpoint;
		M0_LOG(M0_DEBUG, "Ios connecting to mds %s", srv_ep_addr);

		M0_ALLOC_PTR(conn);
		if (conn == NULL)
			return M0_RC(-ENOMEM);

		rc = m0_rpc_client_find_connect(&conn->imc_conn,
						&conn->imc_session,
						rpc_machine, srv_ep_addr,
						M0_CST_MDS,
						MAX_NR_RPC_IN_FLIGHT,
						M0_TIME_NEVER);
		if (rc == 0) {
			conn->imc_connected = true;
			M0_LOG(M0_DEBUG, "Ios connected to mds %s",
					 srv_ep_addr);
		} else {
			conn->imc_connected = false;
			M0_LOG(M0_ERROR, "Ios could not connect to mds %s: "
					 "rc = %d",
					 srv_ep_addr, rc);
		}
		M0_LOG(M0_DEBUG, "ios connected to mds: conn=%p ep=%s rc=%d "
				 "index=%d", conn, srv_ep_addr, rc,
				 conn_map->imc_nr);
		conn_map->imc_map[conn_map->imc_nr ++] = conn;
		M0_ASSERT(conn_map->imc_nr <= M0T1FS_MAX_NR_MDS);
	} m0_tl_endfor;
	return M0_RC(rc);
}

/* Assumes that reqh->rh_rwlock is locked for writing. */
static int ios_mds_conn_get_locked(struct m0_reqh              *reqh,
				   struct m0_ios_mds_conn_map **out,
				   bool                        *new)
{
	M0_PRE(ios_mds_conn_key != 0);

	*new = false;
	*out = m0_reqh_lockers_get(reqh, ios_mds_conn_key);
	if (*out != NULL)
		return 0;

	M0_ALLOC_PTR(*out);
	if (*out == NULL)
		return M0_ERR(-ENOMEM);
	*new = true;

	m0_reqh_lockers_set(reqh, ios_mds_conn_key, *out);
	M0_LOG(M0_DEBUG, "key init for reqh=%p, key=%d", reqh,
	       ios_mds_conn_key);
	return 0;
}

/**
 * Gets ioservice to mdservice connection. If it is newly allocated, establish
 * the connection.
 *
 * @param out the connection is returned here.
 *
 * @note This is a block operation in service.
 *       m0_fom_block_enter()/m0_fom_block_leave() must be used to notify fom.
 */
static int m0_ios_mds_conn_get(struct m0_reqh              *reqh,
			       struct m0_ios_mds_conn_map **out)
{
	int  rc;
	bool new;

	M0_ENTRY("reqh %p", reqh);
	M0_PRE(reqh != NULL);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	rc = ios_mds_conn_get_locked(reqh, out, &new);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	if (new) {
		M0_ASSERT(rc == 0);
		rc = m0_ios_mds_conn_init(reqh, *out);
	}
	return M0_RC(rc);
}

static struct m0_ios_mds_conn *
m0_ios_mds_conn_map_hash(const struct m0_ios_mds_conn_map *imc_map,
			 const struct m0_fid *gfid)
{
	struct m0_fid fid = *gfid;
	unsigned int  hash;

	m0_fid_tchange(&fid, 0);
	hash = m0_fid_hash(&fid);
	M0_LOG(M0_DEBUG, "%d nr=%d" FID_F,
			 hash % imc_map->imc_nr,
			 imc_map->imc_nr, FID_P(gfid));
	return imc_map->imc_map[hash % imc_map->imc_nr];
}

/**
 * Terminates and clears the ioservice to mdservice connection.
 */
M0_INTERNAL void m0_ios_mds_conn_fini(struct m0_reqh *reqh)
{
	struct m0_ios_mds_conn_map *imc_map;
	struct m0_ios_mds_conn     *imc;
	int                         rc;
	M0_PRE(reqh != NULL);
	M0_PRE(ios_mds_conn_key != 0);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	imc_map = m0_reqh_lockers_get(reqh, ios_mds_conn_key);
	if (imc_map != NULL)
		m0_reqh_lockers_clear(reqh, ios_mds_conn_key);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	while (imc_map != NULL && imc_map->imc_nr > 0) {
		imc = imc_map->imc_map[--imc_map->imc_nr];
		M0_LOG(M0_DEBUG, "imc conn fini in reqh = %p, imc = %p",
				 reqh, imc);
		if (imc != NULL && imc->imc_connected) {
			M0_LOG(M0_DEBUG, "destroy session for %p", imc);
			rc = m0_rpc_session_destroy(&imc->imc_session,
						    M0_TIME_NEVER);
			if (rc != 0)
				M0_LOG(M0_ERROR,
				       "Failed to terminate session %d", rc);

			M0_LOG(M0_DEBUG, "destroy conn for %p", imc);
			rc = m0_rpc_conn_destroy(&imc->imc_conn, M0_TIME_NEVER);
			if (rc != 0)
				M0_LOG(M0_ERROR,
				       "Failed to terminate connection %d", rc);
		}
		m0_free(imc); /* free(NULL) is OK */
	}
	m0_free(imc_map); /* free(NULL) is OK */
}

/**
 * Gets file attributes from mdservice.
 * @param reqh the request handler.
 * @param gfid the global fid of the file.
 * @param attr the returned attributes will be stored here.
 *
 * @note This is a block operation in service.
 *       m0_fom_block_enter()/m0_fom_block_leave() must be used to notify fom.
 */
M0_INTERNAL int m0_ios_mds_getattr(struct m0_reqh *reqh,
				   const struct m0_fid *gfid,
				   struct m0_cob_attr *attr)
{
	struct m0_ios_mds_conn_map *imc_map;
	struct m0_ios_mds_conn     *imc;
	struct m0_fop              *req;
	struct m0_fop              *rep;
	struct m0_fop_getattr      *getattr;
	struct m0_fop_getattr_rep  *getattr_rep;
	struct m0_fop_cob          *req_fop_cob;
	struct m0_fop_cob          *rep_fop_cob;
	int                         rc;

	rc = m0_ios_mds_conn_get(reqh, &imc_map);
	if (rc != 0)
		return M0_RC(rc);

	imc = m0_ios_mds_conn_map_hash(imc_map, gfid);
	if (!imc->imc_connected)
		return M0_ERR(-ENODEV);

	req = m0_fop_alloc_at(&imc->imc_session, &m0_fop_getattr_fopt);
	if (req == NULL)
		return M0_ERR(-ENOMEM);

	getattr = m0_fop_data(req);
	req_fop_cob = &getattr->g_body;
	req_fop_cob->b_tfid = *gfid;

	M0_LOG(M0_DEBUG, "ios getattr for "FID_F, FID_P(gfid));
	rc = m0_rpc_post_sync(req, &imc->imc_session, NULL, 0);
	M0_LOG(M0_DEBUG, "ios getattr for "FID_F" rc: %d", FID_P(gfid), rc);

	if (rc == 0) {
		rep = m0_rpc_item_to_fop(req->f_item.ri_reply);
		getattr_rep = m0_fop_data(rep);
		rep_fop_cob = &getattr_rep->g_body;
		if (rep_fop_cob->b_rc == 0)
			m0_md_cob_wire2mem(attr, rep_fop_cob);
		else
			rc = rep_fop_cob->b_rc;
	}
	m0_fop_put_lock(req);
	return M0_RC(rc);
}

static int _rpc_post(struct m0_fop         *fop,
		     struct m0_rpc_session *session)
{
	struct m0_rpc_item *item;

	M0_PRE(fop != NULL);
	M0_PRE(session != NULL);

	item                     = &fop->f_item;
	item->ri_session         = session;
	item->ri_prio            = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline        = 0;

	return m0_rpc_post(item);
}

struct mds_op {
	struct m0_fop mo_fop;

	void        (*mo_cb)(void *arg, int rc);
	void         *mo_arg;
	/** saved out pointer. returned data will be copied here */
	void         *mo_out;

	/* These arguments are saved in async call and used in callback */
	void         *mo_p1;   /* saved param1 */
	void         *mo_p2;   /* saved param2 */
};

static void mds_op_release(struct m0_ref *ref)
{
	struct mds_op *mds_op;
	struct m0_fop *fop;

	fop = container_of(ref, struct m0_fop, f_ref);
	mds_op = container_of(fop, struct mds_op, mo_fop);
	m0_fop_fini(fop);
	m0_free(mds_op);
}

static void getattr_rpc_item_reply_cb(struct m0_rpc_item *item)
{
	struct mds_op      *mdsop;
	struct m0_fop      *req;
	struct m0_fop      *rep;
	struct m0_fop_cob  *rep_fop_cob;
	struct m0_cob_attr *attr;
	int                 rc;

	M0_PRE(item != NULL);
	req = m0_rpc_item_to_fop(item);
	mdsop = container_of(req, struct mds_op, mo_fop);
	attr = mdsop->mo_out;

	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		rep = m0_rpc_item_to_fop(item->ri_reply);
		if (m0_is_cob_getattr_fop(req)) {
			struct m0_fop_cob_getattr_reply *getattr_rep;
			getattr_rep = m0_fop_data(rep);
			rep_fop_cob = &getattr_rep->cgr_body;
			rc = getattr_rep->cgr_rc ?: rep_fop_cob->b_rc;
		} else {
			struct m0_fop_getattr_rep *getattr_rep;
			getattr_rep = m0_fop_data(rep);
			rep_fop_cob = &getattr_rep->g_body;
			rc = getattr_rep->g_rc ?: rep_fop_cob->b_rc;
		}
		if (rc == 0)
			m0_md_cob_wire2mem(attr, rep_fop_cob);
	}
	M0_LOG(M0_DEBUG, "ios getattr replied: %d", rc);
	mdsop->mo_cb(mdsop->mo_arg, rc);
}

static const struct m0_rpc_item_ops getattr_fop_rpc_item_ops = {
	.rio_replied = getattr_rpc_item_reply_cb,
};

static void ios_cob_fop_populate(struct m0_fop       *fop,
				 const struct m0_fid *cob_fid,
				 const struct m0_fid *gob_fid,
				 uint32_t             cob_idx,
				 uint32_t             cob_type)
{
	struct m0_fop_cob_common *common;

	M0_PRE(fop != NULL);
	M0_PRE(fop->f_type != NULL);
	M0_PRE(cob_fid != NULL);
	M0_PRE(gob_fid != NULL);

	common = m0_cobfop_common_get(fop);
	M0_ASSERT(common != NULL);

	common->c_gobfid   = *gob_fid;
	common->c_cobfid   = *cob_fid;
	common->c_cob_idx  = cob_idx;
	common->c_cob_type = cob_type;
}

/**
 * Getattr of file from ioservice synchronously.
 */
M0_INTERNAL int m0_ios_getattr(struct m0_reqh *reqh,
			       const struct m0_fid *gfid,
			       uint64_t index,
			       struct m0_cob_attr *attr)
{
	struct m0_fop         *req;
	int                    rc;
	struct m0_fid          md_fid;
	struct m0_rpc_session *rpc_session;
	struct m0_fop_cob     *rep_fop_cob;

	m0_fid_convert_gob2cob(gfid, &md_fid, 0);
	rpc_session = m0_reqh_mdpool_service_index_to_session(reqh,
					gfid, index);
	req = m0_fop_alloc_at(rpc_session, &m0_fop_cob_getattr_fopt);
	if (req == NULL)
		return M0_ERR(-ENOMEM);

	ios_cob_fop_populate(req, &md_fid, gfid, index, M0_COB_MD);
	M0_LOG(M0_DEBUG, "ios getattr for "FID_F, FID_P(gfid));
	rc = m0_rpc_post_sync(req, rpc_session, NULL, 0);
	M0_LOG(M0_DEBUG, "ios getattr sent synchronously: rc = %d", rc);
	if (rc == 0) {
		struct m0_fop                   *rep;
		struct m0_fop_cob_getattr_reply *getattr_rep;

		rep = m0_rpc_item_to_fop(req->f_item.ri_reply);
		getattr_rep = m0_fop_data(rep);
		rep_fop_cob = &getattr_rep->cgr_body;
		if (rep_fop_cob->b_rc == 0)
			m0_md_cob_wire2mem(attr, rep_fop_cob);
		else
			rc = rep_fop_cob->b_rc;
	}
	m0_fop_put_lock(req);
	return M0_RC(rc);
}

static int _ios_cob_getattr_async(struct m0_rpc_session *rpc_session,
				   struct m0_fid *cob_fid,
				   const struct m0_fid *gfid,
				   struct m0_cob_attr *attr,
				   uint32_t index,
				   uint32_t cob_type,
				   void (*cb)(void *arg, int rc),
				   void *arg)
{
	struct mds_op *mdsop;
	struct m0_fop *req;
	int            rc;

	M0_ALLOC_PTR(mdsop);
	if (mdsop == NULL)
		return M0_ERR(-ENOMEM);
	req = &mdsop->mo_fop;
	m0_fop_init(req, &m0_fop_cob_getattr_fopt, NULL, &mds_op_release);
	rc = m0_fop_data_alloc(req);
	if (rc != 0) {
		m0_free(mdsop);
		return M0_RC(rc);
	}
	req->f_item.ri_ops = &getattr_fop_rpc_item_ops;

	mdsop->mo_cb  = cb;
	mdsop->mo_arg = arg;
	mdsop->mo_out = attr;

	ios_cob_fop_populate(req, cob_fid, gfid, index, cob_type);
	M0_LOG(M0_DEBUG, "ios getattr for index:%d"FID_F, (int)index, FID_P(gfid));
	rc = _rpc_post(req, rpc_session);
	M0_LOG(M0_DEBUG, "ios getattr sent asynchronously: rc = %d", rc);

	m0_fop_put_lock(req);

	return M0_RC(rc);
}

M0_INTERNAL int m0_ios_cob_getattr_async(const struct m0_fid *gfid,
					 struct m0_cob_attr *attr,
					 uint64_t cob_idx,
					 struct m0_pool_version *pv,
					 void (*cb)(void *arg, int rc),
					 void *arg)
{
	struct m0_reqh_service_ctx *ctx;
	struct m0_rpc_session      *rpc_session;
	struct m0_fid               cob_fid;
	int                         rc;

	m0_fid_convert_gob2cob(gfid, &cob_fid, cob_idx);
	ctx = pv->pv_pc->pc_dev2svc[cob_idx].pds_ctx;
	rpc_session = &ctx->sc_rlink.rlk_sess;

	rc = _ios_cob_getattr_async(rpc_session, &cob_fid, gfid, attr, cob_idx,
				    M0_COB_IO, cb, arg);

	return M0_RC(rc);
}

/**
 * getattr from ioservice asynchronously.
 */
M0_INTERNAL int m0_ios_getattr_async(struct m0_reqh *reqh,
				     const struct m0_fid *gfid,
				     struct m0_cob_attr *attr,
				     uint64_t index,
				     void (*cb)(void *arg, int rc),
				     void *arg)
{
	int                    rc;
	struct m0_fid          md_fid;
	struct m0_rpc_session *rpc_session;

	m0_fid_convert_gob2cob(gfid, &md_fid, 0);
	rpc_session = m0_reqh_mdpool_service_index_to_session(reqh,
					gfid, index);
	M0_ASSERT(rpc_session != NULL);

	rc = _ios_cob_getattr_async(rpc_session, &md_fid, gfid, attr, index, M0_COB_MD, cb, arg);

	return M0_RC(rc);
}

/**
 * getattr from mdservice asynchronously.
 */
M0_INTERNAL int m0_ios_mds_getattr_async(struct m0_reqh *reqh,
				         const struct m0_fid *gfid,
					 struct m0_cob_attr *attr,
					 void (*cb)(void *arg, int rc),
					 void *arg)
{
	struct m0_ios_mds_conn_map *imc_map;
	struct m0_ios_mds_conn     *imc;
	struct mds_op              *mdsop;
	struct m0_fop              *req;
	struct m0_fop_getattr      *getattr;
	struct m0_fop_cob          *req_fop_cob;
	int                         rc;

	/* This might block on first call. */
	rc = m0_ios_mds_conn_get(reqh, &imc_map);
	if (rc != 0)
		return M0_RC(rc);
	M0_ASSERT(imc_map->imc_nr != 0);
	imc = m0_ios_mds_conn_map_hash(imc_map, gfid);

	if (!imc->imc_connected)
		return M0_ERR(-ENODEV);

	M0_ALLOC_PTR(mdsop);
	if (mdsop == NULL)
		return M0_ERR(-ENOMEM);

	req = &mdsop->mo_fop;
	m0_fop_init(req, &m0_fop_getattr_fopt, NULL, &mds_op_release);
	rc = m0_fop_data_alloc(req);
	if (rc == 0) {
		req->f_item.ri_ops = &getattr_fop_rpc_item_ops;
	} else {
		m0_free(mdsop);
		return M0_RC(rc);
	}

	mdsop->mo_cb  = cb;
	mdsop->mo_arg = arg;
	mdsop->mo_out = attr;

	getattr = m0_fop_data(req);
	req_fop_cob = &getattr->g_body;
	req_fop_cob->b_tfid = *gfid;

	M0_LOG(M0_DEBUG, "ios getattr for "FID_F, FID_P(gfid));
	rc = _rpc_post(req, &imc->imc_session);
	M0_LOG(M0_DEBUG, "ios getattr sent asynchronously: rc = %d", rc);

	m0_fop_put_lock(req);
	return M0_RC(rc);
}

M0_INTERNAL void m0_ios_net_buffer_pool_size_set(uint32_t buffer_pool_size)
{
	ios_net_buffer_pool_size = buffer_pool_size;
}

#undef M0_TRACE_SUBSYSTEM

/** @} endgroup io_service */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
