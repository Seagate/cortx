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
 * Original author: Igor Vartanov
 * Original author: Egor Nikulenkov
 * Original creation date: 03-Mar-2015
 */

#pragma once

#ifndef __MERO_CONF_RCONFC_H__
#define __MERO_CONF_RCONFC_H__

#include "lib/mutex.h"
#include "lib/chan.h"
#include "lib/tlist.h"
#include "fop/fop.h"    /* m0_fop */
#include "sm/sm.h"
#include "conf/confc.h"
#include "ha/entrypoint_fops.h" /* m0_ha_entrypoint_rep */

struct m0_rconfc;
struct m0_rpc_machine;
struct m0_rpc_item;

/**
 * @page rconfc-fspec Redundant Configuration Client (rconfc)
 *
 * Redundant configuration client library -- rconfc -- provides an interface for
 * Mero applications working in cluster with multiple configuration servers
 * (confd).
 *
 * Rconfc supplements confc functionality and processes situations when cluster
 * dynamically changes configuration or loses connection to some of confd
 * servers.
 *
 * - @ref rconfc-fspec-data
 * - @ref rconfc-fspec-sub
 * - @ref rconfc-fspec-routines
 * - @ref rconfc_dfspec "Detailed Functional Specification"
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-fspec-data Data Structures
 * - m0_rconfc --- an instance of redundant configuration client.
 *
 * m0_rconfc structure includes a dedicated m0_confc instance
 * m0_rconfc::rc_confc, which is intended for conventional configuration
 * reading.
 *
 * Beside of that, it contains two lists of confc instances, the one used for
 * communication with multiple confd servers in cluster environment during
 * configuration version election, and the other one used for keeping list of
 * discovered confd servers that run the same version that has won in the most
 * recent election.
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-fspec-sub Subroutines
 *
 * - m0_rconfc_init() initialises rconfc internals.
 *
 * - m0_rconfc_start() carries out configuration version election. During
 *   election all known confd servers are polled for a number of version they
 *   run. The elected version, i.e. the version having the quorum, is used in
 *   further configuration reading operation. In case quorum is reached,
 *   m0_rconfc connects the dedicated m0_rconfc::rc_confc to one of the confd
 *   servers from active list.
 *
 * - m0_rconfc_start_sync() is synchronous version of m0_rconfc_start().
 *
 * - m0_rconfc_stop() disconnects dedicated confc and release other internal
 *   resources.
 *
 * - m0_rconfc_stop_sync() is synchronous version of m0_rconfc_stop().
 *
 * - m0_rconfc_fini() finalises rconfc instance.
 *
 * - m0_rconfc_ver_max_read() reports the maximum configuration version number
 *   the confd servers responded with during the most recent version election.
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-fspec-routines Initialisation, finalisation and reading data
 *
 * To access configuration data the configuration consumer is allowed to follow
 * two different scenarios:
 *
 * - reading with standalone configuration client (confc)
 * - reading with confc exposed from redundant configuration client (rconfc)
 *
 * In the first case the consumer reads data from particular confd server
 * with no regard to whether its version reached quorum or not.
 *
 * In the second case the consumer is guaranteed to read data from configuration
 * that reached the quorum in the cluster, i.e. the most recent consistent
 * one. As well, the consumer is guaranteed to be notified about the fact of
 * configuration change, to let accommodate with the change if required. In
 * addition, the ability to switch among confd servers in case of network error
 * transparently for the consumer sufficiently strengthens the reading
 * reliability, and due to this, cluster robustness in whole.
 *
 * In the latter case the consumer initialises and starts m0_rconfc instance
 * instead of m0_confc, and performs all further reading via
 * m0_rconfc::rc_confc.
 *
 * Also, the consumer using rconfc doesn't provide any confd addresses
 * explicitly. The list of confd servers and other related information is
 * centralised and is maintained by HA service. Rconfc queries this information
 * on startup.
 *
 * Example:
 *
 * @code
 * #include "conf/rconfc.h"
 * #include "conf/obj.h"
 *
 * struct m0_sm_group    *grp = ...;
 * struct m0_rpc_machine *rpcm = ...;
 * struct m0_rconfc       rconfc;
 *
 * void conf_exp_cb(struct m0_rconfc *rconfc)
 * {
 *         ...   clean up all local copies of configuration data    ...
 *         ... close all m0_conf_obj instances retained by consumer ...
 * }
 *
 * startup(const struct m0_fid *profile, ...)
 * {
 *         rc = m0_rconfc_init(&rconfc, grp, rpcm, conf_exp_cb);
 *         if (rc == 0) {
 *              rc = m0_rconfc_start_sync(&rconfc);
 *              if (rc != 0)
 *                      m0_rconfc_stop_sync(&rconfc);
 *         }
 *         ...
 * }
 *
 * ... Access configuration objects, using confc interfaces. ...
 *
 * reading(...)
 * {
 *        struct m0_conf_obj *obj;
 *        struct m0_confc    *confc = &rconfc.rc_confc;
 *        int                 rc;
 *        rc = m0_confc_open_sync(&obj, confc->cc_root, ... );
 *        ... read object data ...
 *        m0_confc_close(obj);
 * }
 *
 * shutdown(...)
 * {
 *         m0_rconfc_stop_sync(&rconfc);
 *         m0_rconfc_fini(&rconfc);
 * }
 * @endcode
 *
 * For asynchronous rconfc start/stop please see details of
 * m0_rconfc_start_sync(), m0_rconfc_stop_sync() implementation or documentation
 * for m0_rconfc_start(), m0_rconfc_stop().
 *
 * @note Consumer is allowed to use any standard approach for opening
 * configuration and traversing directories in accordance with confc
 * specification. Using rconfc puts no additional limitation other than
 * stipulated by proper m0_rconfc_init().
 *
 * @see @ref rconfc_dfspec "Detailed Functional Specification"
 */

/**
 * @defgroup rconfc_dfspec Redundant Configuration Client (rconfc)
 * @brief Detailed Functional Specification.
 *
 * @see @ref rconfc-fspec "Functional Specification"
 *
 * @{
 */

enum m0_rconfc_state {
	M0_RCS_INIT,
	M0_RCS_ENTRYPOINT_WAIT,
	M0_RCS_ENTRYPOINT_CONSUME,
	M0_RCS_CREDITOR_SETUP,
	M0_RCS_GET_RLOCK,
	M0_RCS_VERSION_ELECT,
	M0_RCS_IDLE,
	M0_RCS_RLOCK_CONFLICT,
	M0_RCS_CONDUCTOR_DRAIN,
	M0_RCS_CONDUCTOR_DISCONNECT,
	M0_RCS_STOPPING,
	M0_RCS_FAILURE,
	M0_RCS_FINAL
};

struct rconfc_load_ctx {
	struct m0_sm_group rx_grp;
	struct rconfc_load_ast {
		bool             run;
		struct m0_thread thread;
	}                  rx_ast;
	int                rx_rc;
};

typedef void (*m0_rconfc_cb_t)(struct m0_rconfc *rconfc);

/**
 * Redundant configuration client.
 */
struct m0_rconfc {
	/**
	 * A dedicated confc instance initialised during m0_rconfc_start() in
	 * case the read lock is successfully acquired. Initially connected to a
	 * confd of the elected version, an element of m0_rconfc::rc_active
	 * list. Later, it may be reconnected on the fly to another confd server
	 * of the same version in case previous confd communication failed, or
	 * version re-election occurred.
	 *
	 * Consumer is expected to use m0_rconfc::rc_confc instance for reading
	 * configuration data done any standard way in accordance with m0_confc
	 * specification. However, any explicit initialisation and finalisation
	 * of the instance must be avoided, as m0_rconfc is fully responsible
	 * for that part.
	 */
	struct m0_confc           rc_confc;
	/**
	 * Rconfc state machine.
	 *
	 * @note rc_ha_update_cl links to the rc_sm.sm_chan which is used
	 * as argument for the m0_conf_confc_ha_update_async() call.
	 * It is convenient as we don't need additional channel and mutex
	 * structures just to get the notification about confc_ha_update
	 * completion. The drawback of this approach is that the threads
	 * waiting for the SM state change (like at m0_rconfc_start_wait())
	 * will be awaken needlessly. But this seems to be harmless.
	 */
	struct m0_sm              rc_sm;
	/**
	 * Version number the quorum was reached for. Read-only. Value
	 * M0_CONF_VER_UNKNOWN indicates that the latest version election failed
	 * or never was carried out.
	 */
	uint64_t                  rc_ver;
	/**
	 * The minimum number of confc-s that must have the same version of
	 * configuration in order for this version to be elected. Read-only.
	 */
	uint32_t                  rc_quorum;

	/* Private part. Consumer is not welcomed to access the data below. */

	/**
	 * Rconfc expiration callback. Installed during m0_rconfc_init(), but is
	 * allowed to be re-set later if required, on a locked rconfc instance.
	 *
	 * The callback is executed being under rconfc lock.
	 *
	 * Rconfc expiration callback is called when rconfc election happens.
	 * The callee side must close all configuration objects currently held
	 * and invalidate locally stored configuration data copies if any.
	 *
	 * @note No configuration reading is granted during the call because of
	 * rconfc locked state. Therefore, configuration reader should schedule
	 * reading for some time in future. The callback will also be called
	 * when reaching quorum is not possible. In this case rconfc->rc_ver ==
	 * M0_CONF_VER_UNKNOWN.
	 */
	m0_rconfc_cb_t            rc_expired_cb;

	/**
	 * Rconfc idle callback. Initially unset. Allowed to be
	 * installed later if required, on a locked rconfc instance.
	 *
	 * The callback is executed being under rconfc lock.
	 *
	 * Rconfc ready callback is called when m0_rconfc::rc_sm is switched to
	 * M0_RCS_IDLE state. Callback indicates that configuration is available
	 * for reading using m0_rconfc::rc_confc.
	 *
	 * @note No configuration reading is possible during the callback
	 * execution due to the locked state, and the reading should be
	 * scheduled for some time in future.
	 */
	m0_rconfc_cb_t            rc_ready_cb;

	/**
	 * Rconfc callback called when M0_RCS_FAILURE state reached. Initially
	 * unset. Allowed to be installed after start if required, on a locked
	 * rconfc instance.
	 *
	 * @note Do not set m0_rconfc::rc_fatal_cb directly, use
	 * m0_rconfc_fatal_cb_set() instead.
	 *
	 * Rconfc is expected to start without the callback set up. This lets us
	 * distinguish a failure during mero setup from a failure at runtime,
	 * and handle those appropriately.
	 *
	 * @note Rconfc fatal callback is called when m0_rconfc::rc_sm is
	 * switched to M0_RCS_FAILURE. This makes impossible to read
	 * configuration from m0_rconfc::rc_confc::cc_cache during the callback
	 * execution. Besides, the cache starts to be drained right before the
	 * call, so no part of the cache is guaranteed to remain available after
	 * call completion.
	 */
	m0_rconfc_cb_t            rc_fatal_cb;

	/** RPC machine the rconfc to work on. */
	struct m0_rpc_machine    *rc_rmach;
	/**
	 * Gating operations. Intended to control m0_rconfc::rc_confc ability to
	 * perform configuration reading operations.
	 */
	struct m0_confc_gate_ops  rc_gops;
	/** AST to run functions in context of rconfc sm group. */
	struct m0_sm_ast          rc_ast;
	/** Additional data to be used inside AST (i.e. failure error code). */
	int                       rc_datum;
	/** AST to be posted when user requests rconfc to stop. */
	struct m0_sm_ast          rc_stop_ast;
	/** A list of confc instances used during election. */
	struct m0_tl              rc_herd;
	/** A list of confd servers running the elected version. */
	struct m0_tl              rc_active;
	/** Clink to track unpinned conf objects during confc cache drop */
	struct m0_clink           rc_unpinned_cl;
	/** Clink to track ha entrypoint state changes */
        struct m0_clink           rc_ha_entrypoint_cl;
	/** Clink to track confc context state transition on rconfc_herd_fini */
        struct m0_clink           rc_herd_cl;
	/**
	 * Clink to track the finish of m0_conf_confc_ha_update_async()
	 * on phony confc.
	 */
        struct m0_clink           rc_ha_update_cl;
	/** nvec for m0_conf_confc_ha_update_async(). */
	struct m0_ha_nvec         rc_nvec;

	/** Quorum calculation context. */
	void                     *rc_qctx;
	/** Read lock context. */
	void                     *rc_rlock_ctx;
	/** Indicates whether user requests rconfc stopping. */
	bool                      rc_stopping;
	/**
	 * Indicates whether read lock conflict was observed and
	 * should be processed once rconfc is idle.
	 */
	bool                      rc_rlock_conflict;
	/**
	 * Confc instance artificially filled with objects having fids of
	 * current confd and top-level RM services got from HA. Artificial
	 * nature is because of filling its cache non-standard way at the time
	 * when conventional configuration reading is impossible due to required
	 * conf version number remaining unknown until election procedure is
	 * done.
	 *
	 * This instance is added to HA clients list and serves HA notifications
	 * about remote confd and RM service deaths. The intention is to
	 * automatically keep herd list and top-level RM endpoint address up to
	 * date during the entire rconfc instance life cycle.
	 *
	 * @attention Never use this confc for real configuration reading. No
	 *            RPC communication is possible with the instance due to the
	 *            way of its initialisation.
	 */
	struct m0_confc           rc_phony;
	/**
	 * Local configuration database string to be passed to rc_confc during
	 * its initialisation. Initially NULL-ed it can be set prior to running
	 * m0_rconfc_start(). Being set it indicates that rconfc is to be
	 * running mode compatible with confd, i.e. having cache pre-loaded.
	 *
	 * @see m0_rconfc_is_preloaded()
	 *
	 * @note rconfc takes care of freeing this string with m0_free() during
	 * finalisation.
	 */
	char                     *rc_local_conf;
	/** Profile FID rconfc to work on. M0_FID0 if not present. */
	struct m0_fid             rc_profile;
	/**
	 * Context for full conf loading.
	 *
	 * @note Currently many parts of Mero rely on full configuration
	 * availability at operation time.
	 */
	struct rconfc_load_ctx    rc_rx;
	/** AST used for full conf load during rconfc life */
	struct m0_sm_ast          rc_load_ast;
	/** AST used for full conf load finalisation */
	struct m0_sm_ast          rc_load_fini_ast;
	/**
	 * The mutex is to protect m0_rconfc::rc_herd_chan.
	 *
	 * As well, rconfc_link::rl_fom_queued value is accessed by rconfc
	 * code strictly under the lock.
	 */
	struct m0_mutex           rc_herd_lock;
	/**
	 * The channel is to announce herd link finalisation completion. Herd
	 * must be safe to finalise only when having no links which FOMs are
	 * already queued for finalisation.
	 */
	struct m0_chan            rc_herd_chan;
	/** Clink for asynchronous conductor connection/disconnection. */
	struct m0_clink           rc_conductor_clink;
	/** HA ENTRYPOINT reply local copy */
	struct m0_ha_entrypoint_rep rc_ha_entrypoint_rep;
	/**
	 * Retries made to obtain ENTRYPOINT information sufficient for
	 * successfull rconfc start.
	 */
	uint32_t                    rc_ha_entrypoint_retries;
	/**
	 * The state, which m0_rconfc::rc_sm was in prior to entering
	 * M0_RCS_FAILURE state. Rconfc start may fail due to external causes
	 * unrelated to rconfc internal workings, e.g. start timeout expired.
	 */
	uint32_t                    rc_sm_state_on_abort;
};

/**
 * Initialise redundant configuration client instance.
 *
 * If initialisation fails, then finalisation is done internally. No explicit
 * m0_rconfc_fini() is needed.
 *
 * @param rconfc     - rconfc instance
 * @param profile    - makes sense for clients only, otherwise - &M0_FID0.
 * @param sm_group   - state machine group to be used with confc.
 *                     Opening conf objects later in context of this SM group is
 *                     prohibited, so providing locality SM group is a
 *                     bad choice. Use locality0 (m0_locality0_get()) or
 *                     some dedicated SM group.
 * @param rmach      - RPC machine to be used to communicate with confd
 * @param exp_cb     - callback, a "configuration just expired" event
 * @param ready_cb   - rconfc is ready for reading configuration
 */
M0_INTERNAL int m0_rconfc_init(struct m0_rconfc      *rconfc,
			       const struct m0_fid   *profile,
			       struct m0_sm_group    *sm_group,
			       struct m0_rpc_machine *rmach,
			       m0_rconfc_cb_t         expired_cb,
			       m0_rconfc_cb_t         ready_cb);

/**
 * Rconfc starts with obtaining all necessary information (cluster "entry
 * point") from HA service.
 *
 * Rconfc continues with election, where allocated confc instances poll
 * corresponding confd for configuration version number they currently run. At
 * the same time confd availability is tested.
 *
 * Election ends when some version has a quorum. At the same time the active
 * list is populated with rconfc_link instances which confc points to confd of
 * the newly elected version.
 *
 * Function is asynchronous, user can wait on rconfc->rc_sm.sm_chan until
 * rconfc->rc_sm.sm_state in (M0_RCS_IDLE, M0_RCS_FAILURE). M0_RCS_FAILURE state
 * means that start failed, return code can be obtained from
 * rconfc->rc_sm.sm_rc.
 *
 * @note Even with unsuccessful startup rconfc instance requires for
 * explicit m0_rconfc_stop(). The behavior is to provide an ability to call
 * m0_rconfc_ver_max_read() even after unsuccessful version election.
 *
 * When m0_rconfc::rc_local_conf is not NULL during start, m0_rconfc::rc_confc
 * is pre-loaded from the local configuration string. Such rconfc instance does
 * not participate in standard rconfc activity like holding read lock, version
 * election etc., and being pre-loaded it can only be stopped and finalised.
 *
 * @note if rconfc starts successfully and m0_rconfc::rc_ready_cb is not NULL,
 * then the callback is invoked.
 *
 * @see m0_rconfc_is_preloaded()
 */
M0_INTERNAL int m0_rconfc_start(struct m0_rconfc *rconfc);

/**
 * Synchronous version of m0_rconfc_start() with limited deadline. Use
 * M0_TIME_NEVER value to indicate infinite waiting.
 */
M0_INTERNAL int m0_rconfc_start_wait(struct m0_rconfc *rconfc,
				     uint64_t          timeout_ns);

static inline int m0_rconfc_start_sync(struct m0_rconfc *rconfc)
{
	return m0_rconfc_start_wait(rconfc, M0_TIME_NEVER);
}

/**
 * Finalises dedicated m0_rconfc::rc_confc instance and puts all the acquired
 * resources back.
 *
 * Function is asynchronous, user should wait on rconfc->rc_sm.sm_chan until
 * rconfc->rc_sm.sm_state is M0_RCS_FINAL.
 *
 * @note User is not allowed to call m0_rconfc_start() again on stopped rconfc
 * instance as well as other API. The only calls allowed with stopped instance
 * are m0_rconfc_ver_max_read() and m0_rconfc_fini().
 */
M0_INTERNAL void m0_rconfc_stop(struct m0_rconfc *rconfc);

/**
 * Synchronous version of m0_rconfc_stop().
 */
M0_INTERNAL void m0_rconfc_stop_sync(struct m0_rconfc *rconfc);

/**
 * Finalises rconfc instance.
 */
M0_INTERNAL void m0_rconfc_fini(struct m0_rconfc *rconfc);

M0_INTERNAL void m0_rconfc_lock(struct m0_rconfc *rconfc);
M0_INTERNAL void m0_rconfc_unlock(struct m0_rconfc *rconfc);

/**
 * Maximum version number the herd confc elements gathered from their confd
 * peers.
 *
 * @note Supposed to be called internally, e.g. by spiel during transaction
 * opening.
 *
 * @pre rconfc_state(rconfc) != M0_RCS_INIT
 */
M0_INTERNAL uint64_t m0_rconfc_ver_max_read(struct m0_rconfc *rconfc);

/**
 * Installs rconfc fatal callback.
 *
 * @pre rconfc is locked.
 */
M0_INTERNAL void m0_rconfc_fatal_cb_set(struct m0_rconfc *rconfc,
					m0_rconfc_cb_t    cb);

/**
 * Allocates and fills eps with confd endpoints from m0_rconfc::rc_herd list.
 * Returns number of endpoints or -ENOMEM if memory allocation was failed during
 * duplication of an endpoint.
 *
 * @pre rconfc_state(rconfc) != M0_RCS_INIT
 */
M0_INTERNAL int m0_rconfc_confd_endpoints(struct m0_rconfc   *rconfc,
					  const char       ***eps);

/**
 * Allocates and fills ep with RM endpoint from
 * m0_rconfc::rc_rlock_ctx::rlc_rm_addr. Returns 0 if success or -ENOMEM if
 * memory allocation was failed during duplication of the endpoint.
 *
 * @pre rconfc_state(rconfc) != M0_RCS_INIT
 */
M0_INTERNAL int m0_rconfc_rm_endpoint(struct m0_rconfc *rconfc, char **ep);

/**
 * Returns the fid of active RM obtained by entrypoint request to HA.
 *
 * @pre rconfc_state(rconfc) != M0_RCS_INIT
 */
M0_INTERNAL void m0_rconfc_rm_fid(struct m0_rconfc *rconfc, struct m0_fid *out);

/**
 * When running 'pre-loaded' mode no communication with a herd of confd
 * instances, reaching quorum, getting read lock, blocking read context, etc. is
 * supposed on such rconfc instance. In this case it just shells the pre-filled
 * rc_confc while having all event mechanisms disabled. In this case rconfc
 * remains to be in M0_RCS_INIT state.
 *
 * @note Due to being in M0_RCS_INIT state calling m0_rconfc_ver_max_read(),
 * m0_rconfc_rm_endpoint() and m0_rconfc_confd_endpoints() makes no sense with
 * such rconfc, and therefore, prohibited. Rconfc starting and stopping under
 * the circumstances causes no effect on the internal state machine, which
 * remains in M0_RCS_INIT state during all its life.
 */
M0_INTERNAL bool m0_rconfc_is_preloaded(struct m0_rconfc *rconfc);

/**
 * Indicates whether rconfc in M0_RCS_IDLE state or not, In M0_RCS_IDLE state
 * rconfc holds a read lock and a client is able to read configuration.
 */
M0_INTERNAL bool m0_rconfc_reading_is_allowed(const struct m0_rconfc *rconfc);

/** @} rconfc_dfspec */
#endif /* __MERO_CONF_RCONFC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
