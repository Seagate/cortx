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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 05/08/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/finject.h"      /* M0_FI_ENABLED */
#include "lib/locality.h"     /* m0_locality_get */
#include "lib/lockers.h"
#include "lib/memory.h"
#include "lib/misc.h"         /* M0_SET0 */
#include "lib/rwlock.h"
#include "lib/string.h"       /* m0_strcaseeq */
#include "lib/time.h"
#include "fop/fom.h"
#include "rpc/conn.h"
#include "rpc/rpc_machine_internal.h" /* m0_rpc_chan */
#include "rpc/rpclib.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh_service_internal.h"
#include "rpc/rpc.h"          /* m0_rpc__down_timeout */
#include "rpc/rpc_machine.h"  /* m0_rpc_machine_ep */
#include "mero/magic.h"
#include "conf/obj_ops.h"     /* m0_conf_obj_get */
#include "conf/helpers.h"     /* m0_conf_obj2reqh */
#include "pool/pool.h"        /* m0_pools_common_service_ctx_find */
#include "fid/fid.h"          /* m0_fid_eq */
#include "module/instance.h"  /* m0_get */
#include "conf/ha.h"          /* m0_conf_ha_service_event_post */

/**
   @addtogroup reqhservice
   @{
 */

/**
   static global list of service types.
   Holds struct m0_reqh_service_type instances linked via
   m0_reqh_service_type::rst_linkage.

   @see struct m0_reqh_service_type
 */
static struct m0_tl rstypes;

enum {
	M0_REQH_SVC_RPC_SERVICE_TYPE,
	/**
	 * Timeout for rpc_link connect operation in seconds.
	 * If endpoint is unreachable the timeout allows to disconnect from
	 * reqh service when -ETIMEDOUT is returned.
	 */
	REQH_SVC_CONNECT_TIMEOUT = 1,
};

M0_TL_DESCR_DECLARE(abandoned_svc_ctxs, M0_EXTERN);
M0_TL_DECLARE(abandoned_svc_ctxs, M0_EXTERN, struct m0_reqh_service_ctx);

/** Protects access to list rstypes. */
static struct m0_rwlock rstypes_rwlock;

M0_TL_DESCR_DEFINE(rstypes, "reqh service types", static,
		   struct m0_reqh_service_type, rst_linkage, rst_magix,
		   M0_REQH_SVC_TYPE_MAGIC, M0_REQH_SVC_HEAD_MAGIC);

M0_TL_DEFINE(rstypes, static, struct m0_reqh_service_type);

static struct m0_bob_type rstypes_bob;
M0_BOB_DEFINE(static, &rstypes_bob, m0_reqh_service_type);

static const struct m0_bob_type reqh_svc_ctx = {
	.bt_name         = "m0_reqh_service_ctx",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_reqh_service_ctx,
					   sc_magic),
	.bt_magix        = M0_REQH_SVC_CTX_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(static, &reqh_svc_ctx, m0_reqh_service_ctx);

static struct m0_sm_state_descr service_states[] = {
	[M0_RST_INITIALISING] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Initializing",
		.sd_allowed   = M0_BITS(M0_RST_INITIALISED)
	},
	[M0_RST_INITIALISED] = {
		.sd_name      = "Initialized",
		.sd_allowed   = M0_BITS(M0_RST_STARTING, M0_RST_FAILED)
	},
	[M0_RST_STARTING] = {
		.sd_name      = "Starting",
		.sd_allowed   = M0_BITS(M0_RST_STARTED, M0_RST_FAILED)
	},
	[M0_RST_STARTED] = {
		.sd_name      = "Started",
		.sd_allowed   = M0_BITS(M0_RST_STOPPING)
	},
	[M0_RST_STOPPING] = {
		.sd_name      = "Stopping",
		.sd_allowed   = M0_BITS(M0_RST_STOPPED)
	},
	[M0_RST_STOPPED] = {
		.sd_flags     = M0_SDF_FINAL,
		.sd_name      = "Stopped",
		.sd_allowed   = M0_BITS(M0_RST_STARTING)
	},
	[M0_RST_FAILED] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Failed",
	},
};

const struct m0_sm_conf service_states_conf = {
	.scf_name      = "Service states",
	.scf_nr_states = ARRAY_SIZE(service_states),
	.scf_state     = service_states
};

static void
reqh_service_ctx_destroy_if_abandoned(struct m0_reqh_service_ctx *ctx);


M0_INTERNAL bool m0_reqh_service_invariant(const struct m0_reqh_service *svc)
{
	return _0C(m0_reqh_service_bob_check(svc)) &&
	_0C(M0_IN(svc->rs_sm.sm_state, (M0_RST_INITIALISING, M0_RST_INITIALISED,
				        M0_RST_STARTING, M0_RST_STARTED,
				        M0_RST_STOPPING, M0_RST_STOPPED,
				        M0_RST_FAILED))) &&
	_0C(svc->rs_type != NULL && svc->rs_ops != NULL &&
		(svc->rs_ops->rso_start_async != NULL ||
		 svc->rs_ops->rso_start != NULL)) &&
	_0C(ergo(M0_IN(svc->rs_sm.sm_state, (M0_RST_INITIALISED,
		       M0_RST_STARTING, M0_RST_STARTED, M0_RST_STOPPING,
		       M0_RST_STOPPED, M0_RST_FAILED)),
	     svc->rs_reqh != NULL)) &&
	_0C(ergo(M0_IN(svc->rs_sm.sm_state, (M0_RST_STARTED, M0_RST_STOPPING,
					 M0_RST_STOPPED, M0_RST_FAILED)),
	     m0_reqh_svc_tlist_contains(&svc->rs_reqh->rh_services, svc))) &&
	_0C(ergo(svc->rs_reqh != NULL,
	     M0_IN(m0_reqh_lockers_get(svc->rs_reqh, svc->rs_type->rst_key),
		   (NULL, svc)))) &&
	_0C(svc->rs_level > M0_RS_LEVEL_UNKNOWN);
}
M0_EXPORTED(m0_reqh_service_invariant);

M0_INTERNAL struct m0_reqh_service_type *
m0_reqh_service_type_find(const char *sname)
{
	struct m0_reqh_service_type *t;

	M0_PRE(sname != NULL);

	m0_rwlock_read_lock(&rstypes_rwlock);

	t = m0_tl_find(rstypes, t, &rstypes, m0_streq(t->rst_name, sname));
	if (t != NULL)
		M0_ASSERT(m0_reqh_service_type_bob_check(t));

	m0_rwlock_read_unlock(&rstypes_rwlock);
	return t;
}

M0_INTERNAL int
m0_reqh_service_allocate(struct m0_reqh_service **out,
			 const struct m0_reqh_service_type *stype,
			 struct m0_reqh_context *rctx)
{
	int rc;

	M0_ENTRY();
	M0_PRE(out != NULL && stype != NULL);

	rc = stype->rst_ops->rsto_service_allocate(out, stype);
	if (rc == 0) {
		struct m0_reqh_service *service = *out;
		service->rs_type = stype;
		service->rs_reqh_ctx = rctx;
		m0_reqh_service_bob_init(service);
		if (service->rs_level == M0_RS_LEVEL_UNKNOWN)
			service->rs_level = stype->rst_level;
		M0_POST(m0_reqh_service_invariant(service));
	}
	return M0_RC(rc);
}

static void reqh_service_ha_event(struct m0_reqh_service     *service,
                                  enum m0_reqh_service_state  state)
{
	static const enum m0_conf_ha_service_event state2event[] = {
		[M0_RST_STARTING] = M0_CONF_HA_SERVICE_STARTING,
		[M0_RST_STARTED]  = M0_CONF_HA_SERVICE_STARTED,
		[M0_RST_STOPPING] = M0_CONF_HA_SERVICE_STOPPING,
		[M0_RST_STOPPED]  = M0_CONF_HA_SERVICE_STOPPED,
		[M0_RST_FAILED]   = M0_CONF_HA_SERVICE_FAILED,
	};

	if (!M0_IN(state, (M0_RST_STARTING, M0_RST_STARTED,
	                   M0_RST_STOPPING, M0_RST_STOPPED, M0_RST_FAILED)))
		return;
	if (m0_get()->i_ha == NULL || m0_get()->i_ha_link == NULL) {
		M0_LOG(M0_DEBUG, "can't report service HA event=%d "
		       "service_fid="FID_F, state2event[state],
		       FID_P(&service->rs_service_fid));
		return;
	}
	m0_conf_ha_service_event_post(m0_get()->i_ha, m0_get()->i_ha_link,
	                              &service->rs_reqh->rh_fid,
	                              &service->rs_service_fid,
	                              &service->rs_service_fid,
	                              m0_process(),
	                              state2event[state],
	                              service->rs_type->rst_typecode);
}

static void reqh_service_state_set(struct m0_reqh_service *service,
				   enum m0_reqh_service_state state)
{
	m0_sm_group_lock(&service->rs_reqh->rh_sm_grp);
	m0_sm_state_set(&service->rs_sm, state);
	m0_sm_group_unlock(&service->rs_reqh->rh_sm_grp);
	reqh_service_ha_event(service, state);
}

static void reqh_service_starting_common(struct m0_reqh *reqh,
					 struct m0_reqh_service *service,
					 unsigned key)
{
	reqh_service_state_set(service, M0_RST_STARTING);

	/*
	 * NOTE: The key is required to be set before 'rso_start'
	 * as some services can call m0_fom_init() directly in
	 * their service start, m0_fom_init() finds the service
	 * given reqh, using this key
	 */
	M0_ASSERT(m0_reqh_lockers_is_empty(reqh, key));
	m0_reqh_lockers_set(reqh, key, service);
	M0_LOG(M0_DEBUG, "key init for reqh=%p, key=%d", reqh, key);
}

static void reqh_service_failed_common(struct m0_reqh *reqh,
				       struct m0_reqh_service *service,
				       unsigned key)
{
	if (!m0_reqh_lockers_is_empty(reqh, key))
		m0_reqh_lockers_clear(reqh, key);
	reqh_service_state_set(service, M0_RST_FAILED);
}

M0_INTERNAL int
m0_reqh_service_start_async(struct m0_reqh_service_start_async_ctx *asc)
{
	int                     rc;
	unsigned                key;
	struct m0_reqh         *reqh;
	struct m0_reqh_service *service;

	M0_PRE(asc != NULL && asc->sac_service != NULL && asc->sac_fom != NULL);
	service = asc->sac_service;
	M0_PRE(m0_reqh_service_bob_check(service));
	reqh = service->rs_reqh;
	key = service->rs_type->rst_key;

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_service_invariant(service));
	M0_PRE(m0_reqh_service_state_get(service) == M0_RST_INITIALISED);
	M0_PRE(service->rs_ops->rso_start_async != NULL);
	reqh_service_starting_common(reqh, service, key);
	M0_POST(m0_reqh_service_invariant(service));
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	rc = service->rs_ops->rso_start_async(asc);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	if (rc == 0)
		M0_POST(m0_reqh_service_invariant(service));
	else
		reqh_service_failed_common(reqh, service, key);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	return M0_RC(rc);
}

static void reqh_service_started_common(struct m0_reqh *reqh,
					struct m0_reqh_service *service)
{
	reqh_service_state_set(service, M0_RST_STARTED);
}

M0_INTERNAL void m0_reqh_service_started(struct m0_reqh_service *service)
{
	struct m0_reqh *reqh;

	M0_PRE(m0_reqh_service_bob_check(service));
	reqh = service->rs_reqh;

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_service_invariant(service));
	M0_PRE(m0_reqh_service_state_get(service) == M0_RST_STARTING);
	reqh_service_started_common(reqh, service);
	M0_POST(m0_reqh_service_invariant(service));
	m0_rwlock_write_unlock(&reqh->rh_rwlock);
}

M0_INTERNAL void m0_reqh_service_failed(struct m0_reqh_service *service)
{
	unsigned        key;
	struct m0_reqh *reqh;

	M0_PRE(m0_reqh_service_bob_check(service));
	reqh = service->rs_reqh;
	key = service->rs_type->rst_key;

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_service_invariant(service));
	M0_ASSERT(M0_IN(m0_reqh_service_state_get(service),
			(M0_RST_STARTING, M0_RST_INITIALISED)));
	reqh_service_failed_common(reqh, service, key);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);
}

M0_INTERNAL int m0_reqh_service_start(struct m0_reqh_service *service)
{
	int             rc;
	unsigned        key;
	struct m0_reqh *reqh;

	M0_PRE(m0_reqh_service_bob_check(service));
	reqh = service->rs_reqh;
	key = service->rs_type->rst_key;

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_service_invariant(service));
	M0_PRE(service->rs_ops->rso_start != NULL);
	reqh_service_starting_common(reqh, service, key);
	M0_POST(m0_reqh_service_invariant(service));
	M0_POST(m0_reqh_lockers_get(reqh, key) == service);
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	rc = service->rs_ops->rso_start(service);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	if (rc == 0)
		reqh_service_started_common(reqh, service);
	else
		reqh_service_failed_common(reqh, service, key);
	M0_POST(ergo(rc == 0, m0_reqh_service_invariant(service)));
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	return M0_RC(rc);
}

M0_INTERNAL void
m0_reqh_service_prepare_to_stop(struct m0_reqh_service *service)
{
	struct m0_reqh *reqh;
	bool            run_method = false;

	M0_PRE(m0_reqh_service_bob_check(service));
	reqh = service->rs_reqh;

	M0_LOG(M0_DEBUG, "Preparing to stop %s [%d] (%d)",
	       service->rs_type->rst_name,
	       service->rs_level, service->rs_sm.sm_state);

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_PRE(m0_reqh_service_invariant(service));
	M0_ASSERT(M0_IN(service->rs_sm.sm_state, (M0_RST_STARTED,
						  M0_RST_STOPPING)));
	if (service->rs_sm.sm_state == M0_RST_STARTED) {
		reqh_service_state_set(service, M0_RST_STOPPING);
		M0_ASSERT(m0_reqh_service_invariant(service));
		run_method = true;
	}
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	if (run_method && service->rs_ops->rso_prepare_to_stop != NULL)
		service->rs_ops->rso_prepare_to_stop(service);
}

M0_INTERNAL void m0_reqh_service_stop(struct m0_reqh_service *service)
{
	struct m0_reqh *reqh;
	unsigned        key;

	M0_PRE(m0_reqh_service_bob_check(service));
	M0_PRE(m0_fom_domain_is_idle_for(service));
	reqh = service->rs_reqh;
	key = service->rs_type->rst_key;

	m0_rwlock_write_lock(&reqh->rh_rwlock);
	M0_ASSERT(m0_reqh_service_invariant(service));
	M0_ASSERT(service->rs_sm.sm_state == M0_RST_STOPPING);
	reqh_service_state_set(service, M0_RST_STOPPED);
	M0_ASSERT(m0_reqh_service_invariant(service));
	m0_rwlock_write_unlock(&reqh->rh_rwlock);

	service->rs_ops->rso_stop(service);
	/*
	 * Wait again, in case ->rso_stop() launched more foms. E.g., rpcservice
	 * starts reverse connection disconnection at this point.
	 */
	m0_reqh_idle_wait_for(reqh, service);
	m0_reqh_lockers_clear(reqh, key);
}

M0_INTERNAL void m0_reqh_service_init(struct m0_reqh_service *service,
				      struct m0_reqh         *reqh,
				      const struct m0_fid    *fid)
{
	M0_PRE(service != NULL && reqh != NULL &&
		service->rs_sm.sm_state == M0_RST_INITIALISING);
	/* Currently fid may be NULL */
	M0_PRE(fid == NULL || m0_fid_is_valid(fid));

	m0_sm_init(&service->rs_sm, &service_states_conf, M0_RST_INITIALISING,
		   &reqh->rh_sm_grp);

	if (fid != NULL)
		service->rs_service_fid = *fid;
	service->rs_reqh = reqh;
	m0_mutex_init(&service->rs_mutex);
	reqh_service_state_set(service, M0_RST_INITIALISED);

	/*
	 * We want to track these services externally so add them to the list
	 * just as soon as they enter the M0_RST_INITIALISED state.
	 * They will be left on the list until they get fini'd.
	 */
	m0_reqh_svc_tlink_init_at(service, &reqh->rh_services);
	service->rs_fom_key = m0_locality_lockers_allot();
	M0_POST(!m0_buf_is_set(&service->rs_ss_param));
	M0_POST(m0_reqh_service_invariant(service));
}

M0_INTERNAL void m0_reqh_service_fini(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL && m0_reqh_service_bob_check(service));

	M0_ASSERT(m0_fom_domain_is_idle_for(service));
	m0_locality_lockers_free(service->rs_fom_key);
	m0_reqh_svc_tlink_del_fini(service);
	m0_reqh_service_bob_fini(service);
	m0_sm_group_lock(&service->rs_reqh->rh_sm_grp);
	m0_sm_fini(&service->rs_sm);
	m0_sm_group_unlock(&service->rs_reqh->rh_sm_grp);
	m0_mutex_fini(&service->rs_mutex);
	m0_buf_free(&service->rs_ss_param);
	service->rs_ops->rso_fini(service);
}

int m0_reqh_service_type_register(struct m0_reqh_service_type *rstype)
{
	M0_PRE(rstype != NULL);
	M0_PRE(!m0_reqh_service_is_registered(rstype->rst_name));

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EINVAL);

	m0_reqh_service_type_bob_init(rstype);
	m0_rwlock_write_lock(&rstypes_rwlock);
	rstype->rst_key = m0_reqh_lockers_allot();
	rstypes_tlink_init_at_tail(rstype, &rstypes);
	m0_rwlock_write_unlock(&rstypes_rwlock);

	return 0;
}

void m0_reqh_service_type_unregister(struct m0_reqh_service_type *rstype)
{
	M0_PRE(rstype != NULL && m0_reqh_service_type_bob_check(rstype));

	rstypes_tlink_del_fini(rstype);
	m0_reqh_lockers_free(rstype->rst_key);
	m0_reqh_service_type_bob_fini(rstype);
}

M0_INTERNAL int m0_reqh_service_types_length(void)
{
	return rstypes_tlist_length(&rstypes);
}

M0_INTERNAL void m0_reqh_service_list_print(void)
{
	struct m0_reqh_service_type *stype;

	m0_tl_for(rstypes, &rstypes, stype) {
		M0_ASSERT(m0_reqh_service_type_bob_check(stype));
		m0_console_printf(" %s\n", stype->rst_name);
	} m0_tl_endfor;
}

M0_INTERNAL bool m0_reqh_service_is_registered(const char *sname)
{
	return m0_tl_exists(rstypes, stype, &rstypes,
			    m0_strcaseeq(stype->rst_name, sname));
}

M0_INTERNAL int m0_reqh_service_types_init(void)
{
	rstypes_tlist_init(&rstypes);
	m0_bob_type_tlist_init(&rstypes_bob, &rstypes_tl);
	m0_rwlock_init(&rstypes_rwlock);

	return 0;
}
M0_EXPORTED(m0_reqh_service_types_init);

M0_INTERNAL void m0_reqh_service_types_fini(void)
{
	rstypes_tlist_fini(&rstypes);
	m0_rwlock_fini(&rstypes_rwlock);
}
M0_EXPORTED(m0_reqh_service_types_fini);

M0_INTERNAL struct m0_reqh_service *
m0_reqh_service_find(const struct m0_reqh_service_type *st,
		     const struct m0_reqh              *reqh)
{
	struct m0_reqh_service *service;

	M0_PRE(st != NULL && reqh != NULL);
	service = m0_reqh_lockers_get(reqh, st->rst_key);
	M0_POST(ergo(service != NULL, service->rs_type == st));
	return service;
}
M0_EXPORTED(m0_reqh_service_find);

M0_INTERNAL struct m0_reqh_service *
m0_reqh_service_lookup(const struct m0_reqh *reqh, const struct m0_fid *fid)
{
	M0_PRE(reqh != NULL);
	M0_PRE(fid != NULL);

	return m0_tl_find(m0_reqh_svc, s, &reqh->rh_services,
			  m0_fid_eq(fid, &s->rs_service_fid));
}

M0_INTERNAL int m0_reqh_service_state_get(const struct m0_reqh_service *s)
{
	return s->rs_sm.sm_state;
}

M0_INTERNAL int m0_reqh_service_setup(struct m0_reqh_service     **out,
				      struct m0_reqh_service_type *stype,
				      struct m0_reqh              *reqh,
				      struct m0_reqh_context      *rctx,
				      const struct m0_fid         *fid)
{
	int result;

	M0_PRE(m0_reqh_service_find(stype, reqh) == NULL);
	M0_ENTRY();

	result = m0_reqh_service_allocate(out, stype, rctx);
	if (result == 0) {
		struct m0_reqh_service *svc = *out;

		m0_reqh_service_init(svc, reqh, fid);
		result = m0_reqh_service_start(svc);
		if (result != 0)
			m0_reqh_service_fini(svc);
	}
	return M0_RC(result);
}

M0_INTERNAL void m0_reqh_service_quit(struct m0_reqh_service *svc)
{
	if (svc != NULL && svc->rs_sm.sm_state == M0_RST_STARTED) {
		M0_ASSERT(m0_reqh_service_find(svc->rs_type,
					       svc->rs_reqh) == svc);
		m0_reqh_service_prepare_to_stop(svc);
		m0_reqh_idle_wait_for(svc->rs_reqh, svc);
		m0_reqh_service_stop(svc);
		m0_reqh_service_fini(svc);
	}
}

int
m0_reqh_service_async_start_simple(struct m0_reqh_service_start_async_ctx *asc)
{
	M0_ENTRY();
	M0_PRE(m0_reqh_service_state_get(asc->sac_service) == M0_RST_STARTING);

	asc->sac_rc = asc->sac_service->rs_ops->rso_start(asc->sac_service);
	m0_fom_wakeup(asc->sac_fom);
	return M0_RC(asc->sac_rc);
}
M0_EXPORTED(m0_reqh_service_async_start_simple);

static bool service_type_is_valid(enum m0_conf_service_type t)
{
	return 0 < t && t < M0_CST_NR;
}

/**
 * State diagram.
 *
 * @verbatim
 *
 *  ,-------> M0_RSC_OFFLINE
 *  |                |
 *  |  error         v
 *  |`------ M0_RSC_CONNECTING
 *  |                |
 *  |                | success
 *  |                v
 *  |          M0_RSC_ONLINE -------> M0_RSC_CANCELLED
 *  |                |                       |
 *  |                v                       |
 *  `------ M0_RSC_DISCONNECTING <-----------'
 *
 * @endverbatim
 *
 * M0_RSC_OFFLINE is initial and final state. After m0_reqh_service_connect()
 * is called the service context transits to M0_RSC_CONNECTING every time it
 * reaches M0_RSC_OFFLINE until m0_reqh_service_disconnect() is called.
 *
 * Service context transits to M0_RSC_ONLINE when `sc_rlink` is established and
 * `sc_rlink.rlk_sess` can be used.
 *
 * In M0_RSC_CONNECTING and M0_RSC_DISCONNECTING states `sc_rlink` must not be
 * accessed. Exception is reqh_service_ctx_ast_cb() callback, which is called
 * after `rpc_link` receives "operation completed" notification.  Therefore,
 * all operations with `sc_rlink` are deferred and performed inside
 * `reqh_service_ctx_ast_cb()` in these states.
 *
 * Rpc session can be cancelled only in M0_RSC_ONLINE state. If service becomes
 * unavailable in other state, service context keeps trying to connect until
 * `sc_rlink` is established. Cancelled session is always reconnected.
 */

static struct m0_sm_state_descr service_ctx_states[] = {
	[M0_RSC_OFFLINE] = {
		.sd_flags     = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name      = "M0_RSC_OFFLINE",
		.sd_allowed   = M0_BITS(M0_RSC_CONNECTING)
	},
	[M0_RSC_ONLINE] = {
		.sd_name      = "M0_RSC_ONLINE",
		.sd_allowed   = M0_BITS(M0_RSC_DISCONNECTING,
					M0_RSC_CANCELLED)
	},
	[M0_RSC_CONNECTING] = {
		.sd_name      = "M0_RSC_CONNECTING",
		.sd_allowed   = M0_BITS(M0_RSC_ONLINE,
					M0_RSC_OFFLINE)
	},
	[M0_RSC_DISCONNECTING] = {
		.sd_name      = "M0_RSC_DISCONNECTING",
		.sd_allowed   = M0_BITS(M0_RSC_OFFLINE)
	},
	[M0_RSC_CANCELLED] = {
		.sd_name      = "M0_RSC_CANCELLED",
		.sd_allowed   = M0_BITS(M0_RSC_DISCONNECTING)
	},
};

static const struct m0_sm_conf service_ctx_states_conf = {
	.scf_name      = "Service ctx connection states",
	.scf_nr_states = ARRAY_SIZE(service_ctx_states),
	.scf_state     = service_ctx_states
};

static bool reqh_service_context_invariant(const struct m0_reqh_service_ctx *ctx)
{
	return _0C(ctx != NULL) && _0C(m0_reqh_service_ctx_bob_check(ctx)) &&
	       _0C(m0_fid_is_set(&ctx->sc_fid)) &&
	       _0C(service_type_is_valid(ctx->sc_type));
}

M0_INTERNAL void m0_reqh_service_ctx_subscribe(struct m0_reqh_service_ctx *ctx)
{
	m0_conf_obj_get(ctx->sc_service);
	m0_clink_add(&ctx->sc_service->co_ha_chan, &ctx->sc_svc_event);
	m0_conf_obj_get(ctx->sc_process);
	m0_clink_add(&ctx->sc_process->co_ha_chan, &ctx->sc_process_event);
}

M0_INTERNAL void
m0_reqh_service_ctx_unsubscribe(struct m0_reqh_service_ctx *ctx)
{
	if (ctx->sc_svc_event.cl_chan != NULL) {
		m0_clink_cleanup(&ctx->sc_svc_event);
		ctx->sc_svc_event.cl_chan = NULL;
		m0_confc_close(ctx->sc_service);
		ctx->sc_service = NULL;
	}
	if (ctx->sc_process_event.cl_chan != NULL) {
		m0_clink_cleanup(&ctx->sc_process_event);
		ctx->sc_process_event.cl_chan = NULL;
		m0_confc_close(ctx->sc_process);
		ctx->sc_process = NULL;
	}
}

static void reqh_service_connect_locked(struct m0_reqh_service_ctx *ctx,
					m0_time_t                   deadline)
{
	M0_PRE(M0_IN(CTX_STATE(ctx), (M0_RSC_OFFLINE,
				      M0_RSC_CANCELLED)));

	m0_rpc_link_reset(&ctx->sc_rlink);
	reqh_service_ctx_state_move(ctx, M0_RSC_CONNECTING);
	m0_rpc_link_connect_async(&ctx->sc_rlink, deadline,
				  &ctx->sc_rlink_wait);
}

M0_INTERNAL void m0_reqh_service_connect(struct m0_reqh_service_ctx *ctx)
{
	struct m0_conf_obj *obj = ctx->sc_service;

	M0_ENTRY("ctx=%p '%s' Connect to service '%s' type=%s", ctx,
		 m0_rpc_machine_ep(ctx->sc_rlink.rlk_conn.c_rpc_machine),
		 m0_rpc_link_end_point(&ctx->sc_rlink),
		 m0_conf_service_type2str(ctx->sc_type));
	M0_PRE(reqh_service_context_invariant(ctx));
	M0_PRE(m0_conf_cache_is_locked(obj->co_cache));
	M0_PRE(reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_INITED));

	reqh_service_ctx_sm_lock(ctx);
	M0_ASSERT(CTX_STATE(ctx) == M0_RSC_OFFLINE);
	reqh_service_ctx_flag_set(ctx, M0_RSC_RLINK_CONNECT);
	/*
	 * Set M0_RSC_RLINK_CANCEL according to service conf object.  This
	 * simulates behaviour of service_event_handler() for events that are
	 * lost before subscription.
	 */
	if (obj->co_ha_state == M0_NC_FAILED)
		reqh_service_ctx_flag_set(ctx, M0_RSC_RLINK_CANCEL);
	reqh_service_connect_locked(ctx,
		m0_time_from_now(REQH_SVC_CONNECT_TIMEOUT, 0));
	m0_reqh_service_ctx_subscribe(ctx);
	reqh_service_ctx_sm_unlock(ctx);
	M0_LEAVE();
}

M0_INTERNAL bool
m0_reqh_service_ctx_is_connected(const struct m0_reqh_service_ctx *ctx)
{
	return reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_CONNECT);
}

static void
reqh_service_disconnect_locked(struct m0_reqh_service_ctx *ctx)
{
	m0_time_t timeout;

	/*
	 * Not required to wait for reply on session/connection termination
	 * if process is died otherwise wait for some timeout.
	 */
	timeout = CTX_STATE(ctx) == M0_RSC_CANCELLED ?
		  M0_TIME_IMMEDIATELY : m0_rpc__down_timeout();
	reqh_service_ctx_state_move(ctx, M0_RSC_DISCONNECTING);
	m0_rpc_link_disconnect_async(&ctx->sc_rlink, timeout,
				     &ctx->sc_rlink_wait);
}

M0_INTERNAL void m0_reqh_service_disconnect(struct m0_reqh_service_ctx *ctx)
{
	M0_ENTRY("Disconnecting from service '%s'",
		 m0_rpc_link_end_point(&ctx->sc_rlink));
	M0_PRE(reqh_service_context_invariant(ctx));

	m0_reqh_service_ctx_unsubscribe(ctx);
	reqh_service_ctx_sm_lock(ctx);
	reqh_service_ctx_flag_set(ctx, M0_RSC_RLINK_DISCONNECT);
	reqh_service_ctx_flag_clear(ctx, M0_RSC_RLINK_CANCEL);
	/*
	 * 'ING states will be handled in reqh_service_ctx_ast_cb().
	 * Offline state does not require disconnection either.
	 */
	if (M0_IN(CTX_STATE(ctx), (M0_RSC_ONLINE, M0_RSC_CANCELLED)))
		reqh_service_disconnect_locked(ctx);
	reqh_service_ctx_sm_unlock(ctx);
	M0_LEAVE();
}

static int reqh_service_ctx_state_wait(struct m0_reqh_service_ctx *ctx,
				       int                         state)
{
	int rc;

	M0_PRE(M0_IN(state, (M0_RSC_ONLINE, M0_RSC_OFFLINE)));

	reqh_service_ctx_sm_lock(ctx);
	rc = m0_sm_timedwait(&ctx->sc_sm, M0_BITS(state), M0_TIME_NEVER);
	M0_ASSERT_INFO(rc == 0, "rc = %d", rc);
	M0_ASSERT(CTX_STATE(ctx) == state);
	M0_ASSERT(ergo(state == M0_RSC_ONLINE,
		       m0_rpc_link_is_connected(&ctx->sc_rlink)));
	M0_ASSERT(ergo(state == M0_RSC_OFFLINE,
		       !m0_rpc_link_is_connected(&ctx->sc_rlink)));
	rc = ctx->sc_rlink.rlk_rc;
	reqh_service_ctx_sm_unlock(ctx);

	return rc;
}

M0_INTERNAL void m0_reqh_service_connect_wait(struct m0_reqh_service_ctx *ctx)
{

	if (ctx->sc_service->co_ha_state == M0_NC_ONLINE) {
		int rc = reqh_service_ctx_state_wait(ctx, M0_RSC_ONLINE);
		M0_ASSERT(rc == 0);
	}
}

M0_INTERNAL int m0_reqh_service_disconnect_wait(struct m0_reqh_service_ctx *ctx)
{
	return M0_RC(reqh_service_ctx_state_wait(ctx, M0_RSC_OFFLINE));
}

static void reqh_service_reconnect_locked(struct m0_reqh_service_ctx *ctx,
					  const char                 *addr)
{
	M0_PRE(addr != NULL &&
	       strcmp(addr, m0_rpc_link_end_point(&ctx->sc_rlink)) == 0);

	if (M0_IN(CTX_STATE(ctx), (M0_RSC_DISCONNECTING,
				   M0_RSC_CONNECTING))) {
		/* 'ING states reach ONLINE eventually */
		reqh_service_ctx_flag_clear(ctx, M0_RSC_RLINK_CANCEL);
		return;
	}
	reqh_service_disconnect_locked(ctx);
}

M0_INTERNAL void
m0_reqh_service_cancel_reconnect(struct m0_reqh_service_ctx *ctx)
{
	M0_ENTRY("Reconnecting to service '%s'",
		 m0_rpc_link_end_point(&ctx->sc_rlink));
	reqh_service_ctx_sm_lock(ctx);
	M0_PRE(reqh_service_context_invariant(ctx));
	M0_PRE(m0_reqh_service_ctx_is_connected(ctx));
	M0_PRE(!reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_DISCONNECT));
	if (CTX_STATE(ctx) == M0_RSC_ONLINE) {
		m0_rpc_session_cancel(&ctx->sc_rlink.rlk_sess);
		reqh_service_disconnect_locked(ctx);
	}
	reqh_service_ctx_sm_unlock(ctx);
	M0_LEAVE();
}

static void reqh_service_session_cancel(struct m0_reqh_service_ctx *ctx)
{
	M0_PRE(reqh_service_ctx_sm_is_locked(ctx));
	M0_PRE(M0_IN(CTX_STATE(ctx), (M0_RSC_ONLINE,
				      M0_RSC_CONNECTING,
				      M0_RSC_DISCONNECTING)));
	M0_PRE(!reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_CANCEL));

	if (CTX_STATE(ctx) == M0_RSC_ONLINE) {
		m0_rpc_session_cancel(&ctx->sc_rlink.rlk_sess);
		reqh_service_ctx_state_move(ctx, M0_RSC_CANCELLED);
	} else {
		reqh_service_ctx_flag_set(ctx, M0_RSC_RLINK_CANCEL);
	}
}

static bool reqh_service_ctx_is_cancelled(struct m0_reqh_service_ctx *ctx)
{
	M0_PRE(reqh_service_ctx_sm_is_locked(ctx));

	return reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_CANCEL) ||
	       CTX_STATE(ctx) == M0_RSC_CANCELLED;
}

M0_INTERNAL void m0_reqh_service_ctxs_shutdown_prepare(struct m0_reqh *reqh)
{
	struct m0_reqh_service_ctx *ctx;

	M0_ENTRY();
	/*
	 * Step 1: Flag every context for abortion.
	 *
	 * The idea is to make any reconnecting service context to encounter the
	 * flag and go offline before real context destruction occurs.
	 */
	m0_tl_for(pools_common_svc_ctx, &reqh->rh_pools->pc_svc_ctxs, ctx) {
		M0_ASSERT(!reqh_service_ctx_sm_is_locked(ctx));
		/*
		 * Need to prevent context from being activated by HA
		 * notification, service or process related.
		 */
		m0_reqh_service_ctx_unsubscribe(ctx);
		reqh_service_ctx_sm_lock(ctx);
		/*
		 * Context is unconditionally flagged to abort. In case of
		 * connection broken due to network reasons, the context will be
		 * put offline and prevented from further connection.
		 *
		 * This way yet still incomplete context is commanded to go
		 * offline.  Presently offline context does not require any
		 * special attention.
		 */
		reqh_service_ctx_flag_set(ctx, M0_RSC_RLINK_ABORT);
		reqh_service_ctx_sm_unlock(ctx);
	} m0_tl_endfor;

	/*
	 * Step 2: Spot a still connecting context and wait for fom activity.
	 *
	 * The idea is to let not a context with dormant fom go for destruction.
	 * The rpc link fom activity has to occur first to make sure abortion
	 * flag goes in effect. Besides the current one, any other reconnecting
	 * context may be offlined in background because of abortion flag
	 * already installed on previous step.
	 */
	m0_tl_for(pools_common_svc_ctx, &reqh->rh_pools->pc_svc_ctxs, ctx) {
		bool connecting;

		M0_ASSERT(!reqh_service_ctx_sm_is_locked(ctx));
		reqh_service_ctx_sm_lock(ctx);
		connecting = CTX_STATE(ctx) == M0_RSC_CONNECTING;
		M0_LOG(M0_DEBUG, "[%d] %s (%d)", CTX_STATE(ctx),
		       m0_rpc_link_end_point(&ctx->sc_rlink), ctx->sc_type);
		if (connecting) {
			/*
			 * .rlk_rc must not be 0 to meet M0_RSC_OFFLINE
			 * conditions.
			 * */
			ctx->sc_rlink.rlk_rc = M0_ERR(-EPERM);
			reqh_service_ctx_flag_clear(ctx, M0_RSC_RLINK_CONNECT);
			/*
			 * From this moment we rely on rpc link timeout to wake
			 * up the link fom to be ultimately aborted by the flag
			 * specification.
			 */
		}
		reqh_service_ctx_sm_unlock(ctx);
		/* Do waiting outside the sm lock. */
		if (connecting) {
			m0_clink_add_lock(&ctx->sc_rlink.rlk_wait,
					  &ctx->sc_rlink_abort);
			/*
			 * While we were leaving the sm lock the fom activity
			 * might happen alright, thus timed waiting.
			 */
			m0_chan_timedwait(&ctx->sc_rlink_abort,
				m0_time_from_now(REQH_SVC_CONNECT_TIMEOUT, 0));
			m0_clink_del_lock(&ctx->sc_rlink_abort);
		}
	} m0_tl_endfor;
	M0_LEAVE();
}

M0_INTERNAL void m0_reqh_service_ctx_fini(struct m0_reqh_service_ctx *ctx)
{
	M0_ENTRY();

	M0_PRE(reqh_service_context_invariant(ctx));

	reqh_service_ctx_sm_lock(ctx);
	m0_sm_fini(&ctx->sc_sm);
	reqh_service_ctx_sm_unlock(ctx);
	m0_sm_group_fini(&ctx->sc_sm_grp);
	m0_clink_fini(&ctx->sc_rlink_wait);
	m0_clink_fini(&ctx->sc_rlink_abort);
	m0_clink_fini(&ctx->sc_process_event);
	m0_clink_fini(&ctx->sc_svc_event);
	m0_mutex_fini(&ctx->sc_max_pending_tx_lock);
	m0_reqh_service_ctx_bob_fini(ctx);
	if (reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_INITED))
		m0_rpc_link_fini(&ctx->sc_rlink);

	M0_LEAVE();
}

static void reqh_service_ctx_ast_cb(struct m0_sm_group *grp,
				    struct m0_sm_ast   *ast)
{
	struct m0_reqh_service_ctx *ctx = M0_AMB(ctx, ast, sc_rlink_ast);
	bool                        connected;
	bool                        disconnect;

	reqh_service_ctx_sm_lock(ctx);
	M0_ENTRY("'%s' ctx of service '%s' state: %d",
		 m0_rpc_machine_ep(ctx->sc_rlink.rlk_conn.c_rpc_machine),
		 m0_rpc_link_end_point(&ctx->sc_rlink), CTX_STATE(ctx));
	M0_PRE(M0_IN(CTX_STATE(ctx), (M0_RSC_CONNECTING,
				      M0_RSC_DISCONNECTING)));

	connected  = m0_rpc_link_is_connected(&ctx->sc_rlink);
	disconnect = reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_DISCONNECT);

	switch (CTX_STATE(ctx)) {
	case M0_RSC_CONNECTING:
		if (connected) {
			M0_ASSERT(ctx->sc_rlink.rlk_rc == 0);
			reqh_service_ctx_state_move(ctx, M0_RSC_ONLINE);
			M0_LOG(M0_DEBUG, "connecting -> online: %s (%d)",
			       m0_rpc_link_end_point(&ctx->sc_rlink),
			       ctx->sc_type);
		} else {
			M0_ASSERT(ctx->sc_rlink.rlk_rc != 0);
			/* Reconnect later. */
			reqh_service_ctx_state_move(ctx, M0_RSC_OFFLINE);
			M0_LOG(M0_DEBUG, "connecting -> offline: %s (%d)",
			       m0_rpc_link_end_point(&ctx->sc_rlink),
			       ctx->sc_type);
		}
		break;

	case M0_RSC_DISCONNECTING:
		M0_ASSERT(!connected);
		reqh_service_ctx_state_move(ctx, M0_RSC_OFFLINE);
		reqh_service_ctx_destroy_if_abandoned(ctx);
		M0_LOG(M0_DEBUG, "disconnecting -> offline: %s (%d)",
		       m0_rpc_link_end_point(&ctx->sc_rlink), ctx->sc_type);
		break;

	default:
		M0_IMPOSSIBLE("Invalid state: %d", CTX_STATE(ctx));
	}
	M0_LOG(M0_DEBUG, "state: %d", CTX_STATE(ctx));

	/* Cancel only established session. */
	if (reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_CANCEL) &&
	    connected) {
		m0_rpc_session_cancel(&ctx->sc_rlink.rlk_sess);
		reqh_service_ctx_state_move(ctx, M0_RSC_CANCELLED);
		reqh_service_ctx_flag_clear(ctx, M0_RSC_RLINK_CANCEL);
	}
	/*
	 * Reconnect every time `ctx' reaches OFFLINE state until
	 * m0_reqh_service_disconnect() is called unless explicit abortion is
	 * requested.
	 */
	if (CTX_STATE(ctx) == M0_RSC_OFFLINE) {
		if (!reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_ABORT) &&
		    !disconnect)
			reqh_service_connect_locked(ctx,
				m0_time_from_now(REQH_SVC_CONNECT_TIMEOUT, 0));
	}
	/* m0_reqh_service_disconnect() was called during connecting. */
	if (CTX_STATE(ctx) == M0_RSC_ONLINE && disconnect)
		reqh_service_disconnect_locked(ctx);

	reqh_service_ctx_sm_unlock(ctx);
}

static bool reqh_service_ctx_rlink_cb(struct m0_clink *clink)
{
	struct m0_reqh_service_ctx *ctx = M0_AMB(ctx, clink, sc_rlink_wait);
	struct m0_locality         *loc;

	/*
	 * Go out from the chan lock.
	 *
	 * @note m0_reqh_service_connect_wait() and
	 * m0_reqh_service_disconnect_wait() must not be called from a locality
	 * thread without m0_fom_block_enter()/leave(). Otherwise, it can lead
	 * to a deadlock.
	 */
	loc = m0_locality_get(ctx->sc_fid.f_key);
	ctx->sc_rlink_ast.sa_cb = &reqh_service_ctx_ast_cb;
	m0_sm_ast_post(loc->lo_grp, &ctx->sc_rlink_ast);

	return true;
}

/**
 * Connect/Disconnect service context on process event from HA.
 */
static bool process_event_handler(struct m0_clink *clink)
{
	struct m0_reqh_service_ctx *ctx = M0_AMB(ctx, clink, sc_process_event);
	struct m0_conf_obj         *obj = M0_AMB(obj, clink->cl_chan,
						 co_ha_chan);
	struct m0_conf_process     *process;

	M0_ENTRY();
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_PROCESS_TYPE);

	process = M0_CONF_CAST(obj, m0_conf_process);
	reqh_service_ctx_sm_lock(ctx);
	if (!reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_CONNECT)) {
		/* Ignore notifications for offline services. */
		goto exit_unlock;
	}

	switch (obj->co_ha_state) {
	case M0_NC_FAILED:
	case M0_NC_TRANSIENT:
		/*
		 * m0_rpc_post() needs valid session, so service context is not
		 * finalised. Here making service context as inactive, which
		 * will become active again after reconnection when process is
		 * restarted.
		 */
		if (!reqh_service_ctx_is_cancelled(ctx))
			reqh_service_session_cancel(ctx);
		break;
	case M0_NC_ONLINE:
		if (!reqh_service_ctx_is_cancelled(ctx) ||
		    m0_conf_obj_grandparent(obj)->co_ha_state != M0_NC_ONLINE)
			break;
		/*
		 * Process may become online prior to service object.
		 *
		 * Make sure respective service object is known online. In case
		 * it is not, quit and let service_event_handler() do the job.
		 *
		 * Note: until service object gets known online, re-connection
		 * is not possible due to assertions in RPC connection HA
		 * subscription code.
		 */
		if (ctx->sc_service == NULL)
			ctx->sc_service = m0_conf_cache_lookup(obj->co_cache,
							    &ctx->sc_fid);
		M0_ASSERT(ctx->sc_service != NULL);
		if (ctx->sc_service->co_ha_state != M0_NC_ONLINE)
			break;

		reqh_service_reconnect_locked(ctx, process->pc_endpoint);
		break;
	default:
		;
	}
exit_unlock:
	reqh_service_ctx_sm_unlock(ctx);

	M0_LEAVE();
	return true;
}

/**
 * Cancel items for service on service failure event from HA.
 */
static bool service_event_handler(struct m0_clink *clink)
{
	struct m0_reqh_service_ctx *ctx = M0_AMB(ctx, clink, sc_svc_event);
	struct m0_conf_obj         *obj = M0_AMB(obj, clink->cl_chan,
						 co_ha_chan);
	struct m0_conf_service     *service;
	struct m0_reqh             *reqh = m0_conf_obj2reqh(obj);
	bool                        result = true;

	M0_ENTRY();
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE);

	service = M0_CONF_CAST(obj, m0_conf_service);
	M0_PRE(ctx == m0_pools_common_service_ctx_find(reqh->rh_pools,
						       &obj->co_id,
						       service->cs_type));
	reqh_service_ctx_sm_lock(ctx);
	if (!reqh_service_ctx_flag_is_set(ctx, M0_RSC_RLINK_CONNECT)) {
		/* Ignore notifications for offline services. */
		goto exit_unlock;
	}

	switch (obj->co_ha_state) {
	case M0_NC_TRANSIENT:
		/*
		 * It seems important to do nothing here to let rpc item ha
		 * timeout do its job. When HA really decides on service death,
		 * it notifies with M0_NC_FAILED.
		 */
		break;
	case M0_NC_FAILED:
		if (!reqh_service_ctx_is_cancelled(ctx))
			reqh_service_session_cancel(ctx);
		break;
	case M0_NC_ONLINE:
		/*
		 * Note: Make no assumptions about process HA state, as service
		 * state update may take a lead in the batch updates.
		 */
		if (reqh_service_ctx_is_cancelled(ctx)) {
			reqh_service_reconnect_locked(ctx,
						      service->cs_endpoints[0]);
		}
		break;
	default:
		;
	}
exit_unlock:
	reqh_service_ctx_sm_unlock(ctx);

	M0_LEAVE();
	return result;
}

M0_INTERNAL int m0_reqh_service_ctx_init(struct m0_reqh_service_ctx *ctx,
					 struct m0_conf_obj *svc_obj,
					 enum m0_conf_service_type stype,
					 struct m0_rpc_machine *rmach,
					 const char *addr,
					 uint32_t max_rpc_nr_in_flight)
{
	struct m0_conf_obj *proc_obj = m0_conf_obj_grandparent(svc_obj);
	int                 rc;

	M0_ENTRY();
	M0_LOG(M0_DEBUG, FID_F "%d", FID_P(&svc_obj->co_id), stype);

	M0_SET0(ctx);
	if (rmach != NULL) {
		rc = m0_rpc_link_init(&ctx->sc_rlink, rmach, &svc_obj->co_id,
				      addr, max_rpc_nr_in_flight);
		if (rc != 0)
			return M0_ERR(rc);
		reqh_service_ctx_flag_set(ctx, M0_RSC_RLINK_INITED);
	}
	ctx->sc_fid = svc_obj->co_id;
	ctx->sc_service = svc_obj;
	ctx->sc_process = proc_obj;
	ctx->sc_type = stype;
	ctx->sc_fid_process = proc_obj->co_id;
	m0_reqh_service_ctx_bob_init(ctx);
	m0_mutex_init(&ctx->sc_max_pending_tx_lock);
	m0_clink_init(&ctx->sc_svc_event, service_event_handler);
	m0_clink_init(&ctx->sc_process_event, process_event_handler);
	m0_clink_init(&ctx->sc_rlink_wait, reqh_service_ctx_rlink_cb);
	m0_clink_init(&ctx->sc_rlink_abort, NULL);
	m0_sm_group_init(&ctx->sc_sm_grp);
	m0_sm_init(&ctx->sc_sm, &service_ctx_states_conf,
		   M0_RSC_OFFLINE, &ctx->sc_sm_grp);
	ctx->sc_rlink_wait.cl_is_oneshot = true;

	M0_POST(reqh_service_context_invariant(ctx));
	return M0_RC(0);
}

M0_INTERNAL int m0_reqh_service_ctx_create(struct m0_conf_obj *svc_obj,
					   enum m0_conf_service_type stype,
					   struct m0_rpc_machine *rmach,
					   const char *addr,
					   uint32_t max_rpc_nr_in_flight,
					   struct m0_reqh_service_ctx **out)
{
	int rc;

	M0_PRE(m0_fid_is_set(&svc_obj->co_id));
	M0_PRE(service_type_is_valid(stype));

	M0_ENTRY(FID_F "stype:%d", FID_P(&svc_obj->co_id), stype);
	M0_ALLOC_PTR(*out);
	if (*out == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_reqh_service_ctx_init(*out, svc_obj, stype, rmach, addr,
				      max_rpc_nr_in_flight);
	if (rc != 0)
		m0_free(*out);

	return M0_RC(rc);
}

M0_INTERNAL void
m0_reqh_service_ctx_destroy(struct m0_reqh_service_ctx *ctx)
{
	m0_reqh_service_ctx_fini(ctx);
	m0_free(ctx);
}

M0_INTERNAL struct m0_reqh_service_ctx *
m0_reqh_service_ctx_from_session(struct m0_rpc_session *session)
{
	struct m0_reqh_service_ctx *ret;
	struct m0_rpc_link         *rlink;

	M0_PRE(session != NULL);

	rlink = M0_AMB(rlink, session, rlk_sess);
	ret = M0_AMB(ret, rlink, sc_rlink);

	M0_POST(reqh_service_context_invariant(ret));

	return ret;
}

static void abandoned_ctx_destroy_cb(struct m0_sm_group *grp,
				     struct m0_sm_ast   *ast)
{
	struct m0_reqh_service_ctx *ctx = ast->sa_datum;

	M0_ENTRY("ctx %p "FID_F, ctx, FID_P(&ctx->sc_fid));
	m0_reqh_service_ctx_destroy(ctx);
	M0_LEAVE();
}

/**
 * Destroys service context if found in m0_pools_common::pc_abandoned_svc_ctxs
 * list.
 */
static void
reqh_service_ctx_destroy_if_abandoned(struct m0_reqh_service_ctx *ctx)
{
	struct m0_pools_common *pc  = ctx->sc_pc;
	struct m0_sm_ast       *ast = &ctx->sc_rlink_ast;

	M0_ENTRY("ctx %p", ctx);
	M0_PRE(M0_IN(ctx->sc_rlink.rlk_conn.c_sm.sm_state,
		     (M0_RPC_CONN_TERMINATED, M0_RPC_CONN_FAILED)));

	if (pools_common_svc_ctx_tlist_contains(&pc->pc_abandoned_svc_ctxs,
						ctx)) {
		pools_common_svc_ctx_tlink_del_fini(ctx);
		/*
		 * Escape from being under the context group lock.
		 *
		 * Hijacking sc_rlink_ast here must be surely safe for abandoned
		 * context, as no connection related operation is going to be
		 * done anymore on the context's rpc link.
		 */
		ast->sa_datum = ctx;
		ast->sa_cb = abandoned_ctx_destroy_cb;
		m0_sm_ast_post(m0_locality_get(ctx->sc_fid.f_key)->lo_grp, ast);
	}
	M0_LEAVE();
}

/** @} endgroup reqhservice */
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
