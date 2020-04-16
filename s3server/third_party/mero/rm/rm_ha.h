/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 1-Oct-2015
 */

#pragma once

#ifndef __MERO_RM_RM_HA_H__
#define __MERO_RM_RM_HA_H__

#include "lib/chan.h"   /* m0_clink */
#include "sm/sm.h"
#include "conf/confc.h" /* m0_confc_ctx, m0_confc */
#include "conf/diter.h" /* m0_conf_diter */

/**
 * @defgroup rm-ha HA notifications support in RM.
 *
 * Local RM owners communicate with remote RM owners operating in context of RM
 * service located on remote network node. HA can make decision about RM service
 * failure and notify all nodes about it. Locally, acceptance of such
 * notification means HA state change of corresponding configuration object in
 * confc instance to M0_NC_FAILED. Also, HA can make decision that RM service is
 * recovered froom failure and is online again. Local RM owners should track
 * these HA state changes and take appropriate actions on them.
 *
 * Local RM owners create a subscription to HA notifications about remote RM
 * services. Effectively, subscription means attaching clink to corresponding RM
 * service configuration object channel (m0_conf_obj::co_chan). Subscription can
 * be made synchronously through m0_rm_ha_subscribe_sync() function, or
 * asynchronously using m0_rm_ha_subscriber structure and related functions.
 * RM service configuration object is searched by user-provided remote endpoint
 * in user-provided confc instance.
 *
 * There are two types of remote owners in relation to local owners: debtors and
 * creditors. They have different subscription policies.
 *
 * Subscription to debtor's HA state change is created when the first request
 * from the remote is received (@see rfom_debtor_subscribe()). RPC endpoint is
 * extracted from RPC connection. This subscription is done automatically inside
 * RM code. Result of subscription doesn't affect overall result of incoming
 * request processing.
 *
 * Subscription to creditor's HA state change isn't created automatically by RM
 * code. The reason is that creditor remote structure initialised and managed
 * by RM user and provided to m0_rm_owner_init(). So RM user should create
 * subscription manually. Not all users are interested in tracking creditor
 * failures (assume short-lived local owner that is created to make a few credit
 * requests). Also, confc can be not ready at the point of local RM owner
 * creation (for example, rconfc requests read lock when confc is not ready
 * yet). If local RM owner observes creditor failure notified by HA, then it
 * invokes "self-windup" procedure ending up in ROS_DEAD_CREDITOR or
 * ROS_INSOLVENT state. The reason for such behaviour is that RM owner can't
 * functioning properly without creditor. RM user can detect that fact by
 * monitoring owner state and react properly. For example, switch to another
 * creditor using m0_rm_owner_creditor_reset(). Also local RM owner can recover
 * from ROS_DEAD_CREDITOR back to ROS_ACTIVE if HA state of creditor returns
 * back to M0_NC_ONLINE.
 *
 * Concrete actions taken upon remote owner HA state changes are implemented in
 * rm_remote_death_ast()/rm_remote_online_ast() functions.
 *
 * @note Several remote RM owners can co-exist in one remote RM service. In this
 * case several subscriptions (by the number of owners) will be created instead
 * of one. It's a little overhead, but it eases implementation.
 *
 * @{
 */

enum m0_rm_ha_subscriber_state {
	RM_HA_SBSCR_INIT,
	RM_HA_SBSCR_FS_OPEN,
	RM_HA_SBSCR_DITER_SEARCH,
	RM_HA_SBSCR_FAILURE,
	RM_HA_SBSCR_FINAL
};

/**
 * Structure to store information related to subscription.
 * It is provided by user and used during subscription/unsubscription.
 */
struct m0_rm_ha_tracker {
	/**
	 * Clink to be attached to conf object which death is to be informed by
	 * means of HA notification.
	 */
	struct m0_clink         rht_clink;
	/**
	 * Clink attaches to m0_reqh::rh_conf_cache_exp and tracks
	 * "configuration expired" event. When it is fired, the tracker must
	 * unsubscribe from m0_conf_obj::co_ha_chan. It is possible to enter a
	 * dangerous time slice on unsubscription when remote death is not
	 * monitored while resource credits still held. This is going to be
	 * solved later by differential conf delivery.
	 */
	struct m0_clink          rht_conf_exp;
	/**
	 * Once subscribed to remote's death notification from HA, the remote
	 * endpoint is kept until unsubscription.
	 */
	char                   *rht_ep;
	/**
	 * Last known state of the remote owner.
	 */
	enum m0_ha_obj_state    rht_state;
};

/**
 * Context for asynchronous subscription to HA notifications.
 * Subscriber is responsible to locate RM service in conf database, fill
 * provided m0_rm_ha_tracker structure and attach clink in this tracker to found
 * RM service configuration object channel.
 */
struct m0_rm_ha_subscriber {
	struct m0_sm             rhs_sm;       /**< State machine */
	struct m0_confc         *rhs_confc;    /**< User-provided confc */
	struct m0_rm_ha_tracker *rhs_tracker;  /**< User-provided tracker */
	struct m0_confc_ctx      rhs_cctx;     /**< Confc reading context */
	struct m0_conf_obj      *rhs_dir_root; /**< FS object (root of diter) */
	struct m0_conf_diter     rhs_diter;    /**< Directory iterator */
	struct m0_clink          rhs_clink;    /**< Clink tracking rhs_cctx */
	struct m0_sm_ast         rhs_ast;      /**< AST to schedule sm action */
};

/**
 * Represents an HA event associated with a remote owner. Events reported by HA
 * are enqueued in m0_rm_resource_type::rt_ha_events.
 */
struct m0_rm_ha_event {
	/** Linkage in m0_rm_resource_type::rt_ha_events. */
	struct m0_queue_link     rhe_link;
	/** HA state of remote owner. */
	enum m0_ha_obj_state     rhe_state;
	/** Tracker associated with a remote owner. */
	struct m0_rm_ha_tracker *rhe_tracker;
};

/**
 * Initialise RM HA tracker structure.
 * @param cb   Used to initialise internal clink. This clink will be attached to
 *             RM service configuration object when subscription is done.
 */
M0_INTERNAL void m0_rm_ha_tracker_init(struct m0_rm_ha_tracker *tracker,
				       m0_chan_cb_t             cb);
/**
 * Finalise RM HA tracker.
 * @pre tracker is not subscribed.
 */
M0_INTERNAL void m0_rm_ha_tracker_fini(struct m0_rm_ha_tracker *tracker);

/**
 * Initialise RM HA subscriber.
 *
 * @param grp     sm group to execute asynchronous operations during
 *                subscription
 * @param confc   confc to search for remote RM service configuration object
 * @param rem_ep  RPC endpoint of the remote RM service. Serves as search
 *                criteria
 * @param tracker initialised tracker structure
 */
M0_INTERNAL int m0_rm_ha_subscriber_init(struct m0_rm_ha_subscriber *sbscr,
					 struct m0_sm_group         *grp,
					 struct m0_confc            *confc,
					 const char                 *rem_ep,
					 struct m0_rm_ha_tracker    *tracker);

/**
 * Start asynchronous subscription process.
 *
 * User should wait until m0_rm_ha_subscriber::rhs_sm is in one of
 * (RM_HA_SBSCR_FINAL, RM_HA_SBSCR_FAILURE) states.
 * If subscriber state machine ends up in RM_HA_SBSCR_FINAL state, then
 * subscription is successful and RM HA tracker clink is attached to found RM
 * service configuration object.
 */
M0_INTERNAL void m0_rm_ha_subscribe(struct m0_rm_ha_subscriber *sbscr);

/**
 * Finalise RM HA subscriber.
 */
M0_INTERNAL void m0_rm_ha_subscriber_fini(struct m0_rm_ha_subscriber *sbscr);

/**
 * Subscribe to HA notifications about RM service failure synchronously.
 * Arguments have the same meaning as in m0_rm_ha_subscriber_init().
 */
M0_INTERNAL int m0_rm_ha_subscribe_sync(struct m0_confc         *confc,
					const char              *rem_ep,
					struct m0_rm_ha_tracker *tracker);

/**
 * Removes remote's subscription to HA notifications.
 * It is safe to call it even if subscription wasn't done before or failed.
 *
 * @pre Confc with object being tracked should be locked.
 */
M0_INTERNAL void m0_rm_ha_unsubscribe(struct m0_rm_ha_tracker *tracker);

/**
 * Locked version of m0_rm_ha_unsubscribe().
 */
M0_INTERNAL void m0_rm_ha_unsubscribe_lock(struct m0_rm_ha_tracker *tracker);

/** @} end of rm group */
#endif /* __MERO_RM_RM_HA_H__ */

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
