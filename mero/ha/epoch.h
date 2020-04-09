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
 * Original creation date: 29-Apr-2013
 */

#pragma once

#ifndef __MERO_HA_EPOCH_H__
#define __MERO_HA_EPOCH_H__


/**
 * @defgroup ha High availability
 *
 * HA epoch module.
 *
 * High-availability (HA) sub-system defines "an HA epoch" as a maximal interval
 * in the system history through which no failures are agreed upon.
 *
 * (Note that the word "epoch" has other meanings.)
 *
 * When a new failure is detected or previous failure eliminated, the process of
 * developing a consensus about the change in failure state is initiated within
 * the HA sub-system. This process, if successful, culminates in transition to a
 * new epoch.
 *
 * An epoch is identified by "an epoch number", which is a 64-bit integer. A
 * later epoch has greater number.
 *
 * Mero operations that depend on failures are tagged by epoch
 * numbers. Typically, rpc items, sent as part of an operation, carry as a field
 * the number of the epoch in which the operation was initiated. If receiver
 * receives a message from an epoch different from the epoch known to the
 * receiver, it has to handle it specially. Possible options include:
 *
 *     - ignore the epoch number difference and process the operation as usual,
 *
 *     - move the receiver to the item's epoch (this is only possible if the
 *       item's epoch is greater than the receiver epoch, because epoch numbers
 *       must increase monotonically),
 *
 *     - drop the item on the floor,
 *
 *     - send "wrong epoch" reply to the sender.
 *
 * HA notifies the system about transition to the new epoch by broadcasting
 * special item to all nodes. This item is always processed by moving its
 * receivers to the item's epoch.
 *
 * @{
 */

/* import */
#include "lib/types.h"
#include "lib/tlist.h"
#include "lib/rwlock.h"
struct m0_rpc_item;
struct m0_confc;

/* export */
struct m0_ha_domain;

/**
 * Epoch change monitor.
 *
 * Monitors, added to the domain, are called in the order of registration, when
 * an item from an epoch different from the known one is received.
 */
struct m0_ha_epoch_monitor {
	/**
	 * This function is called when an item from a past epoch is
	 * received.
	 */
	int (*hem_past)(struct m0_ha_epoch_monitor *self, uint64_t epoch,
			const struct m0_rpc_item *item);
	/**
	 * This function is called when an item from a future epoch is
	 * received.
	 */
	int (*hem_future)(struct m0_ha_epoch_monitor *self, uint64_t epoch,
			  const struct m0_rpc_item *item);
	/** Linkage into m0_ha_domain::hdo_monitors list. */
	struct m0_tlink      hem_linkage;
	/** Domain this monitor is registered with. */
	struct m0_ha_domain *hem_domain;
	uint64_t             hem_magix;
};

/**
 * HA domain tracks HA epoch.
 *
 * Typically, an instance of HA domain is associated with each request handler.
 */
struct m0_ha_domain {
	/** Known epoch number. */
	uint64_t         hdo_epoch;
	/** Lock protecting epoch number changes and monitors list
	    manipulations. */
	struct m0_rwlock hdo_lock;
	/**
	 * List of monitors.
	 *
	 * @see m0_ha_epoch_monitor
	 */
	struct m0_tl               hdo_monitors;
	struct m0_ha_epoch_monitor hdo_default_mon;
};

/**
 * Possible return values of m0_ha_epoch_monitor::hem_{past,future}().
 */
enum m0_ha_epoch_outcome {
	/**
	 * This monitor made no decision, call the next monitor. If all monitors
	 * return M0_HEO_CONTINUE, M0_HEO_ERROR is assumed.
	 */
	M0_HEO_CONTINUE,
	/**
	 * The item should be processed ignoring the difference in epoch.
	 */
	M0_HEO_OK,
	/**
	 * The receiver should transit to the item's epoch.
	 */
	M0_HEO_OBEY,
	/**
	 * Don't deliver the item.
	 */
	M0_HEO_DROP,
	/**
	 * If the item expects a reply, send "wrong epoch" reply to the
	 * sender. Otherwise as in M0_HEO_DROP.
	 */
	M0_HEO_ERROR
};

/**
 * Value used when epoch number is unknown or irrelevant.
 */
M0_EXTERN const uint64_t M0_HA_EPOCH_NONE;

/**
 * Initialise the domain, with the given epoch number.
 */
M0_INTERNAL void m0_ha_domain_init(struct m0_ha_domain *dom, uint64_t epoch);
M0_INTERNAL void m0_ha_domain_fini(struct m0_ha_domain *dom);

M0_INTERNAL void m0_ha_domain_monitor_add(struct m0_ha_domain *dom,
					struct m0_ha_epoch_monitor *mon);
M0_INTERNAL void m0_ha_domain_monitor_del(struct m0_ha_domain *dom,
					struct m0_ha_epoch_monitor *mon);

/**
 * Acquires read lock on the domain and returns known epoch.
 *
 * The known epoch cannot be changed until the lock is released by calling
 * m0_ha_domain_put_read().
 */
M0_INTERNAL uint64_t m0_ha_domain_get_read(struct m0_ha_domain *dom);

/**
 * Releases the lock acquired by m0_ha_domain_get_read().
 */
M0_INTERNAL void m0_ha_domain_put_read(struct m0_ha_domain *dom);

/**
 * Acquires the write lock and returns the known epoch.
 *
 * The known epoch can be neither modified nor queried until the lock is
 * released by m0_ha_domain_put_write().
 */
M0_INTERNAL uint64_t m0_ha_domain_get_write(struct m0_ha_domain *dom);

/**
 * Releases the lock acquired by m0_ha_domain_get_write() and sets the new known
 * epoch.
 *
 * HA sub-system uses this function to move to the next epoch.
 *
 * Message receivers use this function to handle M0_HEO_OBEY return from
 * m0_ha_epoch_monitor::hem_future() method.
 *
 * @pre epoch >= dom->hdo_epoch
 */
M0_INTERNAL void m0_ha_domain_put_write(struct m0_ha_domain *dom, uint64_t epoch);

M0_INTERNAL int  m0_ha_global_init(void);
M0_INTERNAL void m0_ha_global_fini(void);

/**
 * Adds new HA client record to HA global context, or increments client's
 * reference counter in case client record already exists.
 *
 * With this registration the client side is allowed to provide any arbitrary
 * confc instance having cache filled with the conf objects which HA state
 * changes the client side wants to be notified of.
 *
 * In its turn, HA is to apply recieved HA notifications to every registered
 * confc instance. (see m0_ha_state_accept() implementation)
 *
 * @pre confc->cc_cache.ca_lock != NULL
 *
 * @note Any client is allowed to register any confc instance that suits
 * client's needs in HA subscriptions, taking no care about the instance
 * origins, i.e. no matter if the instance were already registered previously by
 * some other client or not.
 */
M0_INTERNAL int m0_ha_client_add(struct m0_confc *confc);

/**
 * Decrements HA client record reference counter, and deletes the record from
 * global HA list when the counter reaches zero value.
 */
M0_INTERNAL int m0_ha_client_del(struct m0_confc *confc);

/**
 * Callback used during global HA client list iteration
 */
typedef void (*m0_ha_client_cb_t)(void       *client,
				  const void *data,
				  uint64_t    data2);

/**
 * Global HA client list iteration. Calling back occurs on per-client context
 * basis. Caller provides callback along with data the caller side is to
 * process. Data is to remain non-modified during iteration.
 *
 * @note Global HA context mutex is expected to be locked in the course of
 * client list iteration.
 */
M0_INTERNAL void m0_ha_clients_iterate(m0_ha_client_cb_t iter,
				       const void       *data,
				       uint64_t          data2);

M0_INTERNAL int m0_ha_epoch_check(const struct m0_rpc_item *item);

/** @} end of ha group */

#endif /* __MERO_HA_EPOCH_H__ */

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
