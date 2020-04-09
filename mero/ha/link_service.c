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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 15-May-2016
 */


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/link_service.h"

#include "lib/tlist.h"          /* M0_TL_DESCR_DEFINE */
#include "lib/misc.h"           /* container_of */
#include "lib/types.h"          /* m0_uint128_eq */
#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/mutex.h"          /* m0_mutex */

#include "mero/magic.h"         /* M0_HA_LINK_SERVICE_MAGIC */
#include "reqh/reqh_service.h"  /* m0_reqh_service */

#include "ha/link.h"            /* m0_ha_link */

M0_TL_DESCR_DEFINE(ha_link_svc, "ha_link_service::hls_links", static,
		   struct m0_ha_link, hln_service_link, hln_service_magic,
		   M0_HA_LINK_SERVICE_LINK_MAGIC,
		   M0_HA_LINK_SERVICE_HEAD_MAGIC);
M0_TL_DEFINE(ha_link_svc, static, struct m0_ha_link);

struct ha_link_service {
	struct m0_reqh_service  hls_service;
	struct m0_reqh         *hls_reqh;
	struct m0_tl            hls_links;
	struct m0_tl            hls_quiescing;
	struct m0_mutex         hls_lock;
	uint64_t                hls_magic;
};

static const struct m0_bob_type ha_link_service_bob_type = {
	.bt_name         = "m0_ha_link_service",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct ha_link_service, hls_magic),
	.bt_magix        = M0_HALON_INTERFACE_MAGIC,
};
/*
 * "static inline" because with just "static" ha_link_service_bob_check()
 * is not used, but it's declared => compiler emits a warning.
 */
M0_BOB_DEFINE(static inline, &ha_link_service_bob_type, ha_link_service);

static struct ha_link_service *
ha_link_service_container(struct m0_reqh_service *service)
{
	return bob_of(service, struct ha_link_service, hls_service,
		      &ha_link_service_bob_type);
}

static void ha_link_service_init(struct m0_reqh_service *service)
{
	struct ha_link_service *hl_service = ha_link_service_container(service);

	M0_ENTRY("service=%p hl_service=%p", service, hl_service);
	m0_mutex_init(&hl_service->hls_lock);
	ha_link_svc_tlist_init(&hl_service->hls_links);
	ha_link_svc_tlist_init(&hl_service->hls_quiescing);
	M0_LEAVE();
}

static void ha_link_service_fini(struct m0_reqh_service *service)
{
	struct ha_link_service *hl_service = ha_link_service_container(service);

	M0_ENTRY("service=%p hl_service=%p", service, hl_service);
	ha_link_svc_tlist_fini(&hl_service->hls_quiescing);
	ha_link_svc_tlist_fini(&hl_service->hls_links);
	m0_mutex_fini(&hl_service->hls_lock);
	ha_link_service_bob_fini(hl_service);
	/* allocated in ha_link_service_allocate() */
	m0_free(hl_service);
	M0_LEAVE();
}

static int ha_link_service_start(struct m0_reqh_service *service)
{
	M0_ENTRY();
	return M0_RC(0);
}

static int ha_link_service_fop_accept(struct m0_reqh_service *service,
				      struct m0_fop *fop)
{
	M0_ENTRY();
	return M0_RC(0);
}

static void ha_link_service_stop(struct m0_reqh_service *service)
{
	M0_ENTRY();
	M0_LEAVE();
}

static struct m0_ha_link *
ha_link_service_find(struct ha_link_service  *hl_service,
                     const struct m0_uint128 *link_id,
                     struct m0_uint128       *connection_id)
{
	struct m0_ha_link *hl;

	M0_ENTRY("hl_service=%p link_id="U128X_F, hl_service, U128_P(link_id));
	M0_PRE(m0_mutex_is_locked(&hl_service->hls_lock));
	hl = m0_tl_find(ha_link_svc, ha_link, &hl_service->hls_links,
		m0_uint128_eq(&ha_link->hln_service_link_id, link_id));
	if (connection_id != NULL)
		*connection_id = hl == NULL ? M0_UINT128(0, 0) :
				 hl->hln_service_connection_id;
	M0_LEAVE("hl=%p link_id="U128X_F" connection_id="U128X_F, hl,
	         U128_P(link_id), U128_P(connection_id == NULL ?
	                                 &M0_UINT128(0, 0) : connection_id));
	return hl;
}

static void ha_link_service_get(struct ha_link_service *hl_service,
                                struct m0_ha_link      *hl)
{
	M0_PRE(hl != NULL);
	M0_ENTRY("hl_service=%p hl=%p hln_service_ref_counter=%"PRIu64,
		 hl_service, hl, hl->hln_service_ref_counter);
	M0_PRE(m0_mutex_is_locked(&hl_service->hls_lock));
	M0_PRE(!hl->hln_service_quiescing);
	M0_PRE(!hl->hln_service_released);

	++hl->hln_service_ref_counter;
}

M0_INTERNAL struct m0_ha_link *
m0_ha_link_service_find_get(struct m0_reqh_service  *service,
                            const struct m0_uint128 *link_id,
                            struct m0_uint128       *connection_id)
{
	struct ha_link_service *hl_service = ha_link_service_container(service);
	struct m0_ha_link      *hl;

	M0_ENTRY("service=%p hl_service=%p link_id="U128X_F,
	         service, hl_service, U128_P(link_id));
	m0_mutex_lock(&hl_service->hls_lock);
	hl = ha_link_service_find(hl_service, link_id, connection_id);
	if (hl != NULL)
		ha_link_service_get(hl_service, hl);
	m0_mutex_unlock(&hl_service->hls_lock);
	M0_LEAVE("service=%p hl_service=%p link_id="U128X_F" hl=%p ",
	         service, hl_service, U128_P(link_id), hl);
	return hl;
}

M0_INTERNAL void m0_ha_link_service_put(struct m0_reqh_service *service,
                                        struct m0_ha_link      *hl)
{
	struct ha_link_service *hl_service = ha_link_service_container(service);
	uint64_t                ref_counter;

	M0_ENTRY("service=%p hl_service=%p hl=%p", service, hl_service, hl);
	m0_mutex_lock(&hl_service->hls_lock);
	M0_PRE(hl->hln_service_ref_counter > 0);
	--hl->hln_service_ref_counter;
	if (hl->hln_service_ref_counter == 0 && hl->hln_service_quiescing) {
		hl->hln_service_released = true;
		m0_chan_signal_lock(hl->hln_service_release_chan);
	}
	ref_counter = hl->hln_service_ref_counter;
	m0_mutex_unlock(&hl_service->hls_lock);
	M0_LEAVE("service=%p hl_service=%p hl=%p "
	         "hln_service_ref_counter=%"PRIu64,
	         service, hl_service, hl, ref_counter);
}

M0_INTERNAL void m0_ha_link_service_quiesce(struct m0_reqh_service *service,
                                            struct m0_ha_link      *hl,
                                            struct m0_chan         *chan)
{
	struct ha_link_service *hl_service = ha_link_service_container(service);

	M0_ENTRY("service=%p hl_service=%p hl=%p", service, hl_service, hl);
	m0_mutex_lock(&hl_service->hls_lock);
	hl->hln_service_release_chan = chan;
	ha_link_service_get(hl_service, hl);
	ha_link_svc_tlist_move_tail(&hl_service->hls_quiescing, hl);
	hl->hln_service_quiescing = true;
	m0_mutex_unlock(&hl_service->hls_lock);
	m0_ha_link_service_put(service, hl);
	M0_LEAVE("service=%p hl_service=%p hl=%p", service, hl_service, hl);
}

M0_INTERNAL void
m0_ha_link_service_register(struct m0_reqh_service  *service,
                            struct m0_ha_link       *hl,
                            const struct m0_uint128 *link_id,
                            const struct m0_uint128 *connection_id)
{
	struct ha_link_service *hl_service = ha_link_service_container(service);

	M0_ENTRY("service=%p hl=%p hl_service=%p", service, hl, hl_service);
	hl->hln_service_ref_counter   = 0;
	hl->hln_service_link_id       = *link_id;
	hl->hln_service_connection_id = *connection_id;
	hl->hln_service_quiescing     = false;
	hl->hln_service_released      = false;
	m0_mutex_lock(&hl_service->hls_lock);
	M0_PRE(ha_link_service_find(hl_service, link_id, NULL) == NULL);
	ha_link_svc_tlink_init_at_tail(hl, &hl_service->hls_links);
	m0_mutex_unlock(&hl_service->hls_lock);
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_link_service_deregister(struct m0_reqh_service *service,
                                               struct m0_ha_link      *hl)
{
	struct ha_link_service *hl_service = ha_link_service_container(service);

	M0_ENTRY("service=%p hl=%p hl_service=%p", service, hl, hl_service);
	M0_PRE(hl->hln_service_released);
	M0_PRE(hl->hln_service_ref_counter == 0);
	m0_mutex_lock(&hl_service->hls_lock);
	ha_link_svc_tlink_del_fini(hl);
	m0_mutex_unlock(&hl_service->hls_lock);
	M0_LEAVE();
}

static const struct m0_reqh_service_ops ha_link_service_ops = {
	.rso_start      = ha_link_service_start,
	.rso_fop_accept = ha_link_service_fop_accept,
	.rso_stop       = ha_link_service_stop,
	.rso_fini       = ha_link_service_fini,
};

static int ha_link_service_allocate(struct m0_reqh_service            **service,
                                    const struct m0_reqh_service_type  *stype);

static const struct m0_reqh_service_type_ops ha_link_stype_ops = {
	.rsto_service_allocate = ha_link_service_allocate,
};

struct m0_reqh_service_type m0_ha_link_service_type = {
	.rst_name       = "ha-link-service",
	.rst_ops        = &ha_link_stype_ops,
	.rst_level      = M0_HA_LINK_SVC_LEVEL,
	.rst_keep_alive = true,
};

static int ha_link_service_allocate(struct m0_reqh_service            **service,
                                    const struct m0_reqh_service_type  *stype)
{
	struct ha_link_service *hl_service;

	M0_ENTRY();
	M0_PRE(stype == &m0_ha_link_service_type);

	M0_ALLOC_PTR(hl_service);
	if (hl_service == NULL)
		return M0_RC(-ENOMEM);

	ha_link_service_bob_init(hl_service);
	ha_link_service_init(&hl_service->hls_service);
	*service           = &hl_service->hls_service;
	(*service)->rs_ops = &ha_link_service_ops;

	return M0_RC(0);
}

M0_INTERNAL int m0_ha_link_service_init(struct m0_reqh_service **hl_service,
                                        struct m0_reqh          *reqh)
{
	M0_ENTRY("reqh=%p", reqh);
	return M0_RC(m0_reqh_service_setup(hl_service, &m0_ha_link_service_type,
	                                   reqh, NULL, NULL));
}

M0_INTERNAL void m0_ha_link_service_fini(struct m0_reqh_service *hl_service)
{
	M0_ENTRY("hl_service=%p", hl_service);
	m0_reqh_service_quit(hl_service);
	M0_LEAVE();
}

M0_INTERNAL int m0_ha_link_service_mod_init(void)
{
	return m0_reqh_service_type_register(&m0_ha_link_service_type);
}

M0_INTERNAL void m0_ha_link_service_mod_fini(void)
{
	m0_reqh_service_type_unregister(&m0_ha_link_service_type);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
