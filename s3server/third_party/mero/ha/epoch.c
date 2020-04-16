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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 02-May-2013
 */


/**
 * @addtogroup ha High availability
 *
 * @{
 */

#include "lib/misc.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"
#include "lib/errno.h"               /* ENOMEM */
#include "lib/memory.h"
#include "module/instance.h"
#include "mero/magic.h"
#include "rpc/rpc_machine.h"
#include "rpc/item.h"
#include "reqh/reqh.h"

#include "ha/epoch.h"

/**
 * HA client record, an item of ha_global::hg_clients list.
 *
 * HA acceptance procedure is expected to apply notification vector to each and
 * every confc instance globally known to HA via client records.
 *
 * @see m0_ha_state_accept()
 */
struct ha_client {
	struct m0_confc *hc_confc;           /**< confc to be updated by HA   */
	struct m0_ref    hc_ref;             /**< client reference counter    */
	struct m0_tlink  hc_link;            /**< standard linkage to list    */
	uint64_t         hc_magic;           /**< standard magic number       */
};

/**
 * HA global context. Intended for keeping list of clients to be notified via
 * confc instances registered with the context. Besides, keeps RPC session to be
 * used for communication with HA.
 */
struct ha_global {
	struct m0_tl    hg_clients;   /**< HA clients to be updated    */
	struct m0_mutex hg_guard;     /**< contexts list protection    */
};

M0_TL_DESCR_DEFINE(hg_client, "ha global clients list", static,
		   struct ha_client, hc_link, hc_magic,
		   M0_HA_CLIENT_MAGIC, M0_HA_CLIENT_HEAD_MAGIC);
M0_TL_DEFINE(hg_client, static, struct ha_client);

M0_TL_DESCR_DEFINE(ham, "ha epoch monitor", static,
		   struct m0_ha_epoch_monitor, hem_linkage, hem_magix,
		   M0_HA_EPOCH_MONITOR_MAGIC, M0_HA_DOMAIN_MAGIC);
M0_TL_DEFINE(ham, static, struct m0_ha_epoch_monitor);
M0_INTERNAL const uint64_t M0_HA_EPOCH_NONE = 0ULL;

static int default_mon_future(struct m0_ha_epoch_monitor *self,
			      uint64_t epoch, const struct m0_rpc_item *item)
{
	return M0_HEO_OBEY;
}

M0_INTERNAL void m0_ha_domain_init(struct m0_ha_domain *dom, uint64_t epoch)
{
	dom->hdo_epoch = epoch;
	m0_rwlock_init(&dom->hdo_lock);
	ham_tlist_init(&dom->hdo_monitors);
	dom->hdo_default_mon = (struct m0_ha_epoch_monitor) {
		.hem_future = default_mon_future
	};
	m0_ha_domain_monitor_add(dom, &dom->hdo_default_mon);
}

M0_INTERNAL void m0_ha_domain_fini(struct m0_ha_domain *dom)
{
	m0_ha_domain_monitor_del(dom, &dom->hdo_default_mon);
	ham_tlist_fini(&dom->hdo_monitors);
	m0_rwlock_fini(&dom->hdo_lock);
}

M0_INTERNAL void m0_ha_domain_monitor_add(struct m0_ha_domain *dom,
					struct m0_ha_epoch_monitor *mon)
{
	m0_rwlock_write_lock(&dom->hdo_lock);
	ham_tlink_init_at(mon, &dom->hdo_monitors);
	m0_rwlock_write_unlock(&dom->hdo_lock);
}

M0_INTERNAL void m0_ha_domain_monitor_del(struct m0_ha_domain *dom,
					struct m0_ha_epoch_monitor *mon)
{
	m0_rwlock_write_lock(&dom->hdo_lock);
	ham_tlist_del(mon);
	m0_rwlock_write_unlock(&dom->hdo_lock);
}

M0_INTERNAL uint64_t m0_ha_domain_get_read(struct m0_ha_domain *dom)
{
	m0_rwlock_read_lock(&dom->hdo_lock);
	return dom->hdo_epoch;
}

M0_INTERNAL void m0_ha_domain_put_read(struct m0_ha_domain *dom)
{
	m0_rwlock_read_unlock(&dom->hdo_lock);
}

M0_INTERNAL uint64_t m0_ha_domain_get_write(struct m0_ha_domain *dom)
{
	m0_rwlock_write_lock(&dom->hdo_lock);
	return dom->hdo_epoch;
}

M0_INTERNAL void m0_ha_domain_put_write(struct m0_ha_domain *dom,
					uint64_t epoch)
{
	M0_PRE(epoch >= dom->hdo_epoch);
	dom->hdo_epoch = epoch;
	m0_rwlock_write_unlock(&dom->hdo_lock);
}

M0_INTERNAL int m0_ha_global_init(void)
{
	struct ha_global *hg;

	M0_ALLOC_PTR(hg);
	if (hg == NULL)
		return M0_ERR(-ENOMEM);
	m0_get()->i_moddata[M0_MODULE_HA] = hg;
	m0_mutex_init(&hg->hg_guard);
	hg_client_tlist_init(&hg->hg_clients);
	return M0_RC(0);
}

M0_INTERNAL void m0_ha_global_fini(void)
{
	struct ha_global *hg = m0_get()->i_moddata[M0_MODULE_HA];

	M0_PRE(hg != NULL);
	hg_client_tlist_fini(&hg->hg_clients);
	m0_mutex_fini(&hg->hg_guard);
	m0_free0(&hg);
}

static inline void ha_global_lock(struct ha_global *hg)
{
	m0_mutex_lock(&hg->hg_guard);
}

static inline void ha_global_unlock(struct ha_global *hg)
{
	m0_mutex_unlock(&hg->hg_guard);
}

/**
 * @todo: Is it possible to move {m0}_ha_client_* functions to ha/note.[ch]
 * files? Seems that functionality is related to HA notifications.
 */
/** ha_client::hc_ref release callback */
static void ha_client_release(struct m0_ref *ref)
{
	struct ha_client *client;

	client = container_of(ref, struct ha_client, hc_ref);
	hg_client_tlink_del_fini(client);
	m0_free(client);
}

M0_INTERNAL int m0_ha_client_add(struct m0_confc *confc)
{
	struct ha_global *hg = m0_get()->i_moddata[M0_MODULE_HA];
	struct ha_client *client;
	int               rc;

	M0_ENTRY();
	M0_PRE(hg != NULL);
	/*
	 * Only properly initialised confc allowed to register here. Otherwise
	 * early HA notifications acceptance to cause a crash on locking cache
	 * in ha_state_accept()!
	 */
	M0_PRE(confc->cc_cache.ca_lock != NULL);

	ha_global_lock(hg);
	client = m0_tl_find(hg_client, item, &hg->hg_clients,
			    item->hc_confc == confc);
	if (client == NULL) {
		M0_ALLOC_PTR(client);
		if (client == NULL) {
			rc = -ENOMEM;
			goto err;
		}
		client->hc_confc = confc;
		hg_client_tlink_init_at_tail(client, &hg->hg_clients);
		m0_ref_init(&client->hc_ref, 1, ha_client_release);
	} else {
		m0_ref_get(&client->hc_ref);
		rc = -EALREADY;
		goto err;
	}
	ha_global_unlock(hg);
	return M0_RC(0);

err:
	ha_global_unlock(hg);
	return M0_ERR(rc);

}

M0_INTERNAL int m0_ha_client_del(struct m0_confc *confc)
{
	struct ha_global *hg = m0_get()->i_moddata[M0_MODULE_HA];
	struct ha_client *client;
	int               rc = 0;

	M0_ENTRY();
	M0_PRE(hg != NULL);
	ha_global_lock(hg);
	client = m0_tl_find(hg_client, item, &hg->hg_clients,
			    item->hc_confc == confc);
 	if (client != NULL)
		m0_ref_put(&client->hc_ref);
	else
		rc = -ENOENT;
	ha_global_unlock(hg);
	return M0_RC(rc);
}

M0_INTERNAL void m0_ha_clients_iterate(m0_ha_client_cb_t iter,
				       const void       *data,
				       uint64_t          data2)
{
	struct ha_global *hg = m0_get()->i_moddata[M0_MODULE_HA];
	struct ha_client *client;

	M0_PRE(hg != NULL);
	ha_global_lock(hg);
	m0_tl_for(hg_client, &hg->hg_clients, client) {
		iter(client->hc_confc, data, data2);
	} m0_tl_endfor;
	ha_global_unlock(hg);
}

M0_INTERNAL int m0_ha_epoch_check(const struct m0_rpc_item *item)
{
	struct m0_ha_domain             *ha_dom;
	uint64_t                         item_epoch = item->ri_ha_epoch;
	uint64_t                         epoch;
	struct m0_ha_epoch_monitor      *mon;
	int                              rc = 0;

	ha_dom = &item->ri_rmachine->rm_reqh->rh_hadom;
	M0_LOG(M0_DEBUG, "mine=%lu rcvd=%lu",
				(unsigned long)ha_dom->hdo_epoch,
				(unsigned long)item_epoch);
	if (item_epoch == ha_dom->hdo_epoch)
		return 0;

	epoch = m0_ha_domain_get_write(ha_dom);

	/*
	 * Domain epoch could be changed before we took the lock
	 * with m0_ha_domain_get_write(), so let's check it again.
	 */
	if (epoch == item_epoch)
		goto out;

	m0_tl_for(ham, &ha_dom->hdo_monitors, mon) {
		if (item_epoch > epoch && mon->hem_future != NULL)
			rc = mon->hem_future(mon, epoch, item);
		else if (mon->hem_past != NULL)
			rc = mon->hem_past(mon, epoch, item);
		else
			continue;

		if (rc == M0_HEO_CONTINUE) {
			continue;
		} else if (rc == M0_HEO_OK) {
			break;
		} else if (rc == M0_HEO_OBEY) {
			M0_LOG(M0_DEBUG, "old=%lu new=%lu",
						(unsigned long)epoch,
						(unsigned long)item_epoch);
			epoch = item_epoch;
			break;
		} else if (M0_IN(rc, (M0_HEO_DROP, M0_HEO_ERROR))) {
			rc = 1;
			break;
		}
	} m0_tl_endfor;

out:
	m0_ha_domain_put_write(ha_dom, epoch);

	return M0_RC(rc);
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
