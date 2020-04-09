/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>,
 *                  Mandar Sawant <Mandar_Sawant@xyratex.com>
 * Original creation date: 05/19/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/finject.h"   /* M0_FI_ENABLED */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/atomic.h"
#include "lib/locality.h"
#include "lib/semaphore.h"

#include "mero/magic.h"
#include "addb2/sys.h"
#include "addb2/global.h"
#include "stob/stob.h"
#include "net/net.h"
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "dtm/dtm.h"
#include "rpc/rpc.h"
#include "rpc/item_internal.h" /* m0_rpc_item_is_request */
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"
#include "layout/pdclust.h"
#include "fd/fd.h"             /* m0_fd_fwd_map */
#include "fop/fom_simple.h"
#include "pool/pool.h"
#include "conf/obj.h"          /* M0_CONF_PROCESS_TYPE, m0_conf_fid_type */
#include "conf/confc.h"        /* m0_confc_init */
#include "conf/helpers.h"      /* m0_confc_args */
#include "be/ut/helper.h"

/**
   @addtogroup reqh
   @{
 */

/**
   Tlist descriptor for reqh services.
 */
M0_TL_DESCR_DEFINE(m0_reqh_svc, "reqh service", M0_INTERNAL,
		   struct m0_reqh_service, rs_linkage, rs_magix,
		   M0_REQH_SVC_MAGIC, M0_REQH_SVC_HEAD_MAGIC);

M0_TL_DEFINE(m0_reqh_svc, M0_INTERNAL, struct m0_reqh_service);

static struct m0_bob_type rqsvc_bob;
M0_BOB_DEFINE(M0_INTERNAL, &rqsvc_bob, m0_reqh_service);

/**
   Tlist descriptor for rpc machines.
 */
M0_TL_DESCR_DEFINE(m0_reqh_rpc_mach, "rpc machines", ,
		   struct m0_rpc_machine, rm_rh_linkage, rm_magix,
		   M0_RPC_MACHINE_MAGIC, M0_REQH_RPC_MACH_HEAD_MAGIC);

M0_TL_DEFINE(m0_reqh_rpc_mach, , struct m0_rpc_machine);

M0_LOCKERS_DEFINE(M0_INTERNAL, m0_reqh, rh_lockers);

extern struct m0_reqh_service_type m0_rpc_service_type;

static void __reqh_fini(struct m0_reqh *reqh);

struct disallowed_fop_reply {
	struct m0_fom_simple  ffr_sfom;
	struct m0_fop        *ffr_fop;
	int                   ffr_rc;
};

/**
   Request handler state machine description
 */
static struct m0_sm_state_descr m0_reqh_sm_descr[] = {
	[M0_REQH_ST_INIT] = {
		.sd_flags       = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name        = "Init",
		.sd_allowed     = M0_BITS(M0_REQH_ST_NORMAL,
					  M0_REQH_ST_SVCS_STOP)
	},
	[M0_REQH_ST_NORMAL] = {
		.sd_flags       = 0,
		.sd_name        = "Normal",
		.sd_allowed     = M0_BITS(M0_REQH_ST_DRAIN,
					  M0_REQH_ST_SVCS_STOP)
	},
	[M0_REQH_ST_DRAIN] = {
		.sd_flags       = 0,
		.sd_name        = "Drain",
		.sd_allowed     = M0_BITS(M0_REQH_ST_SVCS_STOP)
	},
	[M0_REQH_ST_SVCS_STOP] = {
		.sd_flags       = 0,
		.sd_name        = "ServicesStop",
		.sd_allowed     = M0_BITS(M0_REQH_ST_STOPPED)
	},
	[M0_REQH_ST_STOPPED] = {
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "Stopped",
		.sd_allowed     = 0
	},
};

/**
   Request handler state machine configuration.
 */
static const struct m0_sm_conf m0_reqh_sm_conf = {
	.scf_name      = "Request Handler States",
	.scf_nr_states = ARRAY_SIZE(m0_reqh_sm_descr),
	.scf_state     = m0_reqh_sm_descr,
};

M0_INTERNAL bool m0_reqh_invariant(const struct m0_reqh *reqh)
{
	return	_0C(reqh != NULL) &&
		_0C(ergo(M0_IN(reqh->rh_sm.sm_state,(M0_REQH_ST_INIT,
						 M0_REQH_ST_NORMAL)),
		     reqh->rh_mdstore != NULL)) &&
		_0C(m0_fom_domain_invariant(m0_fom_dom()));
}

M0_INTERNAL int m0_reqh_mdpool_layout_build(struct m0_reqh *reqh)
{
	struct m0_pools_common *pc = reqh->rh_pools;
	struct m0_pool_version *pv;
	struct m0_layout       *layout;
	int                     rc;
	uint64_t                lid;

	M0_ENTRY();
	M0_PRE(_0C(pc != NULL) && _0C(pc->pc_md_pool != NULL));

	pv = pool_version_tlist_head(&pc->pc_md_pool->po_vers);
	lid = m0_pool_version2layout_id(&pv->pv_id, M0_DEFAULT_LAYOUT_ID);
	layout = m0_layout_find(&reqh->rh_ldom, lid);
	if (layout == NULL)
		return M0_RC(-EINVAL);

	rc = m0_layout_instance_build(layout, &pv->pv_id,
				      &pc->pc_md_pool_linst);
	m0_layout_put(layout);

        return M0_RC(rc);
}

M0_INTERNAL void m0_reqh_layouts_cleanup(struct m0_reqh *reqh)
{
	if (reqh->rh_pools->pc_md_pool_linst != NULL)
		m0_layout_instance_fini(reqh->rh_pools->pc_md_pool_linst);
	m0_layout_domain_cleanup(&reqh->rh_ldom);
}

M0_INTERNAL struct m0_rpc_session *
m0_reqh_mdpool_service_index_to_session(const struct m0_reqh *reqh,
				        const struct m0_fid *gob_fid,
				        uint32_t index)
{
	struct m0_reqh_service_ctx    *ctx;
	struct m0_rpc_session         *session;
	struct m0_pdclust_instance    *pi;
	struct m0_pdclust_src_addr     src;
	struct m0_pdclust_tgt_addr     tgt;
	uint64_t                       mds_nr;
	struct m0_pool_version        *md_pv;
	const struct m0_pools_common  *pc = reqh->rh_pools;
	uint32_t                       idx;

	M0_ENTRY();

	M0_LOG(M0_INFO, "index=%d redundancy =%d", index,
			(unsigned int)pc->pc_md_redundancy);
	M0_PRE(index < pc->pc_md_redundancy);

	md_pv = pool_version_tlist_head(&pc->pc_md_pool->po_vers);
	mds_nr = md_pv->pv_attr.pa_P;
	M0_LOG(M0_DEBUG, "number of mdservices =%d", (unsigned int)mds_nr);
	M0_ASSERT(pc->pc_md_redundancy <=
		  (md_pv->pv_attr.pa_N + 2 * md_pv->pv_attr.pa_K));
	M0_ASSERT(index <= mds_nr);

	pi = m0_layout_instance_to_pdi(pc->pc_md_pool_linst);
	src.sa_group = m0_fid_hash(gob_fid);
	src.sa_unit = index;

	m0_mutex_lock(&pi->pi_mutex);
	m0_fd_fwd_map(pi, &src, &tgt);
	m0_mutex_unlock(&pi->pi_mutex);
	M0_ASSERT(tgt.ta_obj < mds_nr);
	idx = md_pv->pv_mach.pm_state->pst_devices_array[tgt.ta_obj].
		pd_sdev_idx;
	ctx = md_pv->pv_pc->pc_dev2svc[idx].pds_ctx;
	M0_ASSERT(ctx != NULL);
	M0_ASSERT(ctx->sc_type == M0_CST_IOS);
	session = &ctx->sc_rlink.rlk_sess;

	M0_LOG(M0_DEBUG, "device index %d id %d -> ctx=%p session=%p", idx,
			(unsigned int)tgt.ta_obj, ctx, session);
	M0_ASSERT(session->s_conn != NULL);
	M0_LEAVE();
	return session;
}

M0_INTERNAL int
m0_reqh_init(struct m0_reqh *reqh, const struct m0_reqh_init_args *reqh_args)
{
	int rc = 0;

	M0_ENTRY("%p", reqh);
	M0_PRE(reqh_args->rhia_fid != NULL);
	M0_PRE(m0_conf_fid_type(reqh_args->rhia_fid) == &M0_CONF_PROCESS_TYPE);
	M0_PRE(m0_fid_is_valid(reqh_args->rhia_fid));

	reqh->rh_dtm     = reqh_args->rhia_dtm;
	reqh->rh_beseg   = reqh_args->rhia_db;
	reqh->rh_mdstore = reqh_args->rhia_mdstore;
	reqh->rh_pools   = reqh_args->rhia_pc;
	reqh->rh_oostore = false;
	reqh->rh_fid     = *reqh_args->rhia_fid;

	m0_fol_init(&reqh->rh_fol);
	m0_ha_domain_init(&reqh->rh_hadom, M0_HA_EPOCH_NONE);
	m0_rwlock_init(&reqh->rh_rwlock);
	m0_reqh_lockers_init(reqh);

	m0_reqh_svc_tlist_init(&reqh->rh_services);
	m0_reqh_rpc_mach_tlist_init(&reqh->rh_rpc_machines);
	m0_sm_group_init(&reqh->rh_sm_grp);
	m0_sm_init(&reqh->rh_sm, &m0_reqh_sm_conf, M0_REQH_ST_INIT,
		   &reqh->rh_sm_grp);

	m0_mutex_init(&reqh->rh_guard);
	m0_mutex_init(&reqh->rh_guard_async);
	m0_chan_init(&reqh->rh_conf_cache_exp, &reqh->rh_guard);
	m0_chan_init(&reqh->rh_conf_cache_ready, &reqh->rh_guard);
	m0_chan_init(&reqh->rh_conf_cache_ready_async, &reqh->rh_guard_async);

	if (reqh->rh_beseg != NULL) {
		rc = m0_reqh_be_init(reqh, reqh->rh_beseg);
		if (rc != 0)
			__reqh_fini(reqh);
	}
	return M0_RC(rc);
}

M0_INTERNAL int
m0_reqh_be_init(struct m0_reqh *reqh, struct m0_be_seg *seg)
{
	int rc = 0;

	M0_PRE(seg != NULL);

	M0_ENTRY();

	rc = m0_layout_domain_init(&reqh->rh_ldom);
	if (rc == 0) {
		rc = m0_layout_standard_types_register(&reqh->rh_ldom);
		if (rc != 0)
			m0_layout_domain_fini(&reqh->rh_ldom);
		reqh->rh_beseg = seg;
		M0_POST(m0_reqh_invariant(reqh));
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_reqh_be_fini(struct m0_reqh *reqh)
{
	if (reqh->rh_beseg != NULL) {
		m0_layout_standard_types_unregister(&reqh->rh_ldom);
		m0_layout_domain_fini(&reqh->rh_ldom);
		reqh->rh_beseg = NULL;
	}
}

static void __reqh_fini(struct m0_reqh *reqh)
{
	m0_sm_group_lock(&reqh->rh_sm_grp);
	m0_sm_fini(&reqh->rh_sm);
	m0_sm_group_unlock(&reqh->rh_sm_grp);
	m0_sm_group_fini(&reqh->rh_sm_grp);
	m0_reqh_svc_tlist_fini(&reqh->rh_services);
	m0_reqh_rpc_mach_tlist_fini(&reqh->rh_rpc_machines);
	m0_reqh_lockers_fini(reqh);
	m0_rwlock_fini(&reqh->rh_rwlock);
	m0_ha_domain_fini(&reqh->rh_hadom);
	m0_fol_fini(&reqh->rh_fol);

	m0_mutex_lock(&reqh->rh_guard);
	m0_chan_fini(&reqh->rh_conf_cache_exp);
	m0_chan_fini(&reqh->rh_conf_cache_ready);
	m0_mutex_unlock(&reqh->rh_guard);
	m0_chan_fini_lock(&reqh->rh_conf_cache_ready_async);
	m0_mutex_fini(&reqh->rh_guard);
	m0_mutex_fini(&reqh->rh_guard_async);
}

M0_INTERNAL void m0_reqh_fini(struct m0_reqh *reqh)
{
	M0_ENTRY();
	m0_reqh_be_fini(reqh);
	__reqh_fini(reqh);
	M0_LEAVE();
}

M0_INTERNAL void m0_reqhs_fini(void)
{
	m0_reqh_service_types_fini();
}

M0_INTERNAL int m0_reqhs_init(void)
{
	m0_reqh_service_types_init();
	m0_bob_type_tlist_init(&rqsvc_bob, &m0_reqh_svc_tl);
	return 0;
}

#ifndef __KERNEL__
M0_INTERNAL int m0_reqh_addb2_init(struct m0_reqh *reqh, const char *location,
				   uint64_t key, bool mkfs, bool force)
{
	struct m0_addb2_sys *sys  = m0_fom_dom()->fd_addb2_sys;
	struct m0_addb2_sys *gsys = m0_addb2_global_get();
	int                  result;

	/**
	 * @todo replace size constant (128GB)  with a value from confc.
	 */
	result = m0_addb2_sys_stor_start(sys, location, key, mkfs, force,
					 128ULL << 30);
	if (result == 0) {
		result = m0_addb2_sys_net_start(sys);
		if (result == 0) {
			m0_addb2_sys_attach(gsys, sys);
			m0_addb2_sys_sm_start(sys);
			m0_addb2_sys_sm_start(gsys);
		} else
			m0_addb2_sys_stor_stop(sys);
	}
	return result;
}

#else /* !__KERNEL__ */
M0_INTERNAL int m0_reqh_addb2_init(struct m0_reqh *reqh, const char *location,
				   uint64_t key, bool mkfs, bool force)
{
	struct m0_addb2_sys *sys = m0_fom_dom()->fd_addb2_sys;
	int                  result;

	result = m0_addb2_sys_net_start(sys);
	if (result == 0)
		m0_addb2_sys_sm_start(sys);
	return result;
}
#endif

M0_INTERNAL void m0_reqh_addb2_fini(struct m0_reqh *reqh)
{
	struct m0_addb2_sys *sys  = m0_fom_dom()->fd_addb2_sys;

#ifndef __KERNEL__
	struct m0_addb2_sys *gsys = m0_addb2_global_get();

	m0_addb2_sys_detach(gsys);
	m0_addb2_sys_sm_stop(gsys);
#endif
	m0_addb2_sys_sm_stop(sys);
	m0_addb2_sys_net_stop(sys);
	m0_addb2_sys_stor_stop(sys);
}

M0_INTERNAL int m0_reqh_state_get(struct m0_reqh *reqh)
{
	M0_PRE(m0_reqh_invariant(reqh));

	return reqh->rh_sm.sm_state;
}

static void reqh_state_set(struct m0_reqh *reqh,
			   enum m0_reqh_states state)
{
	m0_sm_group_lock(&reqh->rh_sm_grp);
	m0_sm_state_set(&reqh->rh_sm, state);
	m0_sm_group_unlock(&reqh->rh_sm_grp);
}

M0_INTERNAL int m0_reqh_services_state_count(struct m0_reqh *reqh, int state)
{
	int                     cnt = 0;
	struct m0_reqh_service *svc;

	M0_PRE(reqh != NULL);
	M0_PRE(M0_IN(m0_reqh_state_get(reqh), (M0_REQH_ST_NORMAL,
					       M0_REQH_ST_DRAIN,
					       M0_REQH_ST_SVCS_STOP)));
	m0_rwlock_read_lock(&reqh->rh_rwlock);
	m0_tl_for(m0_reqh_svc, &reqh->rh_services, svc) {
		if (m0_reqh_service_state_get(svc) == state)
			++cnt;
	} m0_tl_endfor;
	m0_rwlock_read_unlock(&reqh->rh_rwlock);
	return cnt;
}

extern struct m0_reqh_service_type m0_ha_entrypoint_service_type; /* XXX !!! */
extern struct m0_reqh_service_type m0_ha_link_service_type; /* XXX !!! */
#ifndef __KERNEL__
M0_EXTERN struct m0_reqh_service_type m0_cas_service_type; /* XXX !!! */
#endif

M0_INTERNAL int m0_reqh_fop_allow(struct m0_reqh *reqh, struct m0_fop *fop)
{
	int                                rh_st;
	int                                svc_st;
	struct m0_reqh_service            *svc;
	const struct m0_reqh_service_type *stype;

	M0_ENTRY();
	M0_PRE(reqh != NULL);
	M0_PRE(fop != NULL && fop->f_type != NULL);

	stype = fop->f_type->ft_fom_type.ft_rstype;
	if (stype == NULL)
		return M0_ERR(-ENOSYS);

	svc = m0_reqh_service_find(stype, reqh);
	if (svc == NULL)
		return M0_ERR(-ECONNREFUSED);

	rh_st = m0_reqh_state_get(reqh);
	if (rh_st == M0_REQH_ST_INIT) {
		/*
		 * Allow rpc connection fops from other services during
		 * startup.
		 */
		if (svc != NULL && M0_IN(stype, (&m0_rpc_service_type,
#ifndef __KERNEL__
						 &m0_cas_service_type,
#endif
		                                 &m0_ha_link_service_type,
		                                 &m0_ha_entrypoint_service_type)))
			return M0_RC(0);
		return M0_ERR(-EAGAIN);
	}
	if (rh_st == M0_REQH_ST_STOPPED)
		return M0_ERR(-ESHUTDOWN);

	M0_ASSERT(svc->rs_ops != NULL);
	svc_st = m0_reqh_service_state_get(svc);

	switch (rh_st) {
	case M0_REQH_ST_NORMAL:
		if (svc_st == M0_RST_STARTED)
			return M0_RC(0);
		if (svc_st == M0_RST_STARTING)
			return M0_ERR(-EAGAIN);
		if (svc_st == M0_RST_STOPPING &&
		    svc->rs_ops->rso_fop_accept != NULL)
			return (*svc->rs_ops->rso_fop_accept)(svc, fop);
		return M0_ERR(-ESHUTDOWN);
	case M0_REQH_ST_DRAIN:
		if (M0_IN(svc_st, (M0_RST_STARTED, M0_RST_STOPPING)) &&
		    svc->rs_ops->rso_fop_accept != NULL)
			return (*svc->rs_ops->rso_fop_accept)(svc, fop);
		return M0_ERR(-ESHUTDOWN);
	case M0_REQH_ST_SVCS_STOP:
		return M0_ERR(-ESHUTDOWN);
	default:
		return M0_ERR(-ENOSYS);
	}
}

static int disallowed_fop_tick(struct m0_fom *fom, void *data, int *phase)
{
	struct m0_fop               *fop;
	struct disallowed_fop_reply *reply = data;
	static const char            msg[] = "No service running.";

	fop = m0_fop_reply_alloc(reply->ffr_fop, &m0_fop_generic_reply_fopt);
	if (fop != NULL) {
		struct m0_fop_generic_reply *rep = m0_fop_data(fop);

		rep->gr_rc = reply->ffr_rc;
		rep->gr_msg.s_buf = m0_alloc(sizeof msg);
		if (rep->gr_msg.s_buf != NULL) {
			rep->gr_msg.s_len = sizeof msg;
			memcpy(rep->gr_msg.s_buf, msg, rep->gr_msg.s_len);
		}
		m0_rpc_reply_post(&reply->ffr_fop->f_item, &fop->f_item);
	}
	return -1;
}

static void disallowed_fop_free(struct m0_fom_simple *sfom)
{
	m0_free(container_of(sfom, struct disallowed_fop_reply, ffr_sfom));
}

static void fop_disallowed(struct m0_reqh *reqh,
			   struct m0_fop  *req_fop,
			   int             rc)
{
	struct disallowed_fop_reply *reply;

	M0_PRE(rc != 0);
	M0_PRE(req_fop != NULL);

	M0_ALLOC_PTR(reply);
	if (reply == NULL)
		return;

	m0_fop_get(req_fop);
	reply->ffr_fop = req_fop;
	reply->ffr_rc = rc;
	M0_FOM_SIMPLE_POST(&reply->ffr_sfom, reqh, NULL, disallowed_fop_tick,
			   disallowed_fop_free, reply, M0_FOM_SIMPLE_HERE);

}

M0_INTERNAL int m0_reqh_fop_handle(struct m0_reqh *reqh, struct m0_fop *fop)
{
	struct m0_fom *fom;
	int            rc;

	M0_ENTRY("%p", reqh);
	M0_PRE(reqh != NULL);
	M0_PRE(fop != NULL);

	m0_rwlock_read_lock(&reqh->rh_rwlock);

	rc = m0_reqh_fop_allow(reqh, fop);
	if (rc != 0) {
		m0_rwlock_read_unlock(&reqh->rh_rwlock);
		M0_LOG(M0_WARN, "fop \"%s\"@%p disallowed: %i.",
		       m0_fop_name(fop), fop, rc);
		if (m0_rpc_item_is_request(&fop->f_item)) {
			struct m0_reqh_service_type *rst =
				m0_reqh_service_type_find("simple-fom-service");
			if (rst != NULL &&
			    m0_reqh_service_find(rst, reqh) == NULL)
				return M0_ERR_INFO(-ESHUTDOWN,
						   "Service shutdown.");
			fop_disallowed(reqh, fop, M0_ERR(rc));
		}
		/*
		 * Note :
		 *      Since client will receive generic reply for
		 *      this error, for RPC layer this fop is accepted.
		 */
		return M0_RC(0);
	}

	M0_ASSERT(fop->f_type != NULL);
	M0_ASSERT(fop->f_type->ft_fom_type.ft_ops != NULL);
	M0_ASSERT(fop->f_type->ft_fom_type.ft_ops->fto_create != NULL);

	rc = fop->f_type->ft_fom_type.ft_ops->fto_create(fop, &fom, reqh);
	if (rc == 0)
		m0_fom_queue(fom);

	m0_rwlock_read_unlock(&reqh->rh_rwlock);
	return M0_RC(rc);
}

M0_INTERNAL void m0_reqh_idle_wait_for(struct m0_reqh *reqh,
				       struct m0_reqh_service *service)
{
	struct m0_clink clink;

	M0_PRE(m0_reqh_service_invariant(service));

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&reqh->rh_sm_grp.s_chan, &clink);
	while (!m0_fom_domain_is_idle_for(service))
		m0_chan_wait(&clink);
	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);
}

M0_INTERNAL void m0_reqh_idle_wait(struct m0_reqh *reqh)
{
	struct m0_reqh_service *service;

	M0_PRE(reqh != NULL);

	m0_tl_for(m0_reqh_svc, &reqh->rh_services, service) {
		M0_ASSERT(m0_reqh_service_invariant(service));
		if (service->rs_level < M0_RS_LEVEL_BEFORE_NORMAL)
			continue;
		m0_reqh_idle_wait_for(reqh, service);
	} m0_tl_endfor;
}

M0_INTERNAL void m0_reqh_services_prepare_to_stop(struct m0_reqh *reqh,
						  unsigned        level)
{
	struct m0_reqh_service *service;

	M0_LOG(M0_DEBUG, "-- Preparing to stop at level [%d] --", level);
	m0_tl_for(m0_reqh_svc, &reqh->rh_services, service) {
		M0_ASSERT(m0_reqh_service_invariant(service));
		if (service->rs_level < level ||
		    !M0_IN(m0_reqh_service_state_get(service),
			   (M0_RST_STARTED, M0_RST_STOPPING)))
			continue;
		m0_reqh_service_prepare_to_stop(service);
	} m0_tl_endfor;
}

M0_INTERNAL void m0_reqh_shutdown(struct m0_reqh *reqh)
{
	M0_PRE(reqh != NULL);
	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_invariant(reqh));
	M0_PRE(m0_reqh_state_get(reqh) == M0_REQH_ST_NORMAL);
	reqh_state_set(reqh, M0_REQH_ST_DRAIN);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);
	m0_reqh_services_prepare_to_stop(reqh, M0_RS_LEVEL_EARLY);
}

M0_INTERNAL void m0_reqh_shutdown_wait(struct m0_reqh *reqh)
{
	m0_reqh_shutdown(reqh);
	m0_reqh_idle_wait(reqh);
}

static void __reqh_svcs_stop(struct m0_reqh *reqh, unsigned level)
{
	struct m0_reqh_service *service;

	m0_tl_for(m0_reqh_svc, &reqh->rh_services, service) {
		M0_ASSERT(m0_reqh_service_invariant(service));
		if (service->rs_level < level)
			continue;
		if (M0_IN(m0_reqh_service_state_get(service),
			  (M0_RST_STARTED, M0_RST_STOPPING))) {
			m0_reqh_service_prepare_to_stop(service);
			m0_reqh_idle_wait_for(reqh, service);
			m0_reqh_service_stop(service);
		}
		M0_LOG(M0_DEBUG, "service=%s level=%d srvlev=%d",
		       service->rs_type->rst_name, level, service->rs_level);
		m0_reqh_service_fini(service);
		if (service == reqh->rh_rpc_service)
			reqh->rh_rpc_service = NULL;
	} m0_tl_endfor;
}

M0_INTERNAL void m0_reqh_services_terminate(struct m0_reqh *reqh)
{
	m0_reqh_pre_storage_fini_svcs_stop(reqh);
	m0_reqh_post_storage_fini_svcs_stop(reqh);
}

M0_INTERNAL void m0_reqh_pre_storage_fini_svcs_stop(struct m0_reqh *reqh)
{
	M0_PRE(reqh != NULL);
	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_invariant(reqh));
	M0_PRE(M0_IN(m0_reqh_state_get(reqh),
		     (M0_REQH_ST_DRAIN, M0_REQH_ST_NORMAL, M0_REQH_ST_INIT)));

	reqh_state_set(reqh, M0_REQH_ST_SVCS_STOP);
	M0_PRE(m0_reqh_state_get(reqh) == M0_REQH_ST_SVCS_STOP);

	m0_rwlock_write_unlock(&reqh->rh_rwlock);
	__reqh_svcs_stop(reqh, M0_RS_LEVEL_NORMAL);
	__reqh_svcs_stop(reqh, M0_RS_LEVEL_BEFORE_NORMAL + 1);
	__reqh_svcs_stop(reqh, M0_RS_LEVEL_BEFORE_NORMAL);
	__reqh_svcs_stop(reqh, M0_RS_LEVEL_BEFORE_NORMAL - 1);
	__reqh_svcs_stop(reqh, M0_RS_LEVEL_BEFORE_NORMAL - 2);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	reqh_state_set(reqh, M0_REQH_ST_STOPPED);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);
}

M0_INTERNAL void m0_reqh_post_storage_fini_svcs_stop(struct m0_reqh *reqh)
{
	M0_PRE(m0_reqh_state_get(reqh) == M0_REQH_ST_STOPPED);
	__reqh_svcs_stop(reqh, M0_RS_LEVEL_EARLY);
	__reqh_svcs_stop(reqh, M0_RS_LEVEL_EARLIEST);
}

M0_INTERNAL void m0_reqh_start(struct m0_reqh *reqh)
{
	M0_PRE(reqh != NULL);
	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_invariant(reqh));

	M0_PRE(m0_reqh_state_get(reqh) == M0_REQH_ST_INIT);
	reqh_state_set(reqh, M0_REQH_ST_NORMAL);

	m0_rwlock_write_unlock(&reqh->rh_rwlock);
}

M0_INTERNAL uint64_t m0_reqh_nr_localities(const struct m0_reqh *reqh)
{
	M0_PRE(m0_reqh_invariant(reqh));
	return m0_fom_dom()->fd_localities_nr;
}

M0_INTERNAL int m0_reqh_conf_setup(struct m0_reqh *reqh,
				   struct m0_confc_args *args)
{
	struct m0_rconfc *rconfc = &reqh->rh_rconfc;
	struct m0_fid     profile;
	int               rc;

	M0_PRE(args->ca_group != NULL && args->ca_rmach != NULL);

	rc = m0_fid_sscanf(args->ca_profile, &profile);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Cannot parse profile `%s'",
				   args->ca_profile);

	rc = m0_rconfc_init(rconfc, &profile, args->ca_group, args->ca_rmach,
			    m0_confc_expired_cb, m0_confc_ready_cb);
	if (rc == 0 && args->ca_confstr != NULL) {
		rconfc->rc_local_conf = m0_strdup(args->ca_confstr);
		if (rconfc->rc_local_conf == NULL)
			return M0_ERR(-ENOMEM);
	}
	return M0_RC(rc);
}

M0_INTERNAL struct m0_confc *m0_reqh2confc(struct m0_reqh *reqh)
{
	return &reqh->rh_rconfc.rc_confc;
}

M0_INTERNAL struct m0_fid *m0_reqh2profile(struct m0_reqh *reqh)
{
	return &reqh->rh_rconfc.rc_profile;
}

#undef M0_TRACE_SUBSYSTEM
/** @} endgroup reqh */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
