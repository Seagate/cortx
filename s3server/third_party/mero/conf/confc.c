/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 30-Jan-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/confc.h"
#include "conf/cache.h"
#include "conf/obj_ops.h"     /* m0_conf_obj_find */
#include "conf/preload.h"     /* m0_confstr_parse */
#include "conf/fop.h"         /* m0_conf_fetch_fopt */
#include "mero/magic.h"       /* M0_CONFC_MAGIC, M0_CONFC_CTX_MAGIC */
#include "fop/fom_generic.h"  /* m0_rpc_item_is_generic_reply_fop */
#include "rpc/rpc.h"          /* m0_rpc_post */
#include "fid/fid.h"          /* m0_fid_is_set */
#include "rpc/rpclib.h"       /* m0_rpc_client_connect */
#include "rpc/rpc_machine.h"  /* m0_rpc_machine_lock */
#include "lib/arith.h"        /* M0_CNT_INC, M0_CNT_DEC */
#include "lib/misc.h"         /* M0_IN */
#include "lib/errno.h"        /* ENOMEM, EPROTO */
#include "lib/memory.h"       /* M0_ALLOC_ARR, m0_free */
#include "lib/finject.h"

/**
 * @page confc-lspec confc Internals
 *
 * - @ref confc-lspec-state
 *   - @ref confc-lspec-state-initial
 *   - @ref confc-lspec-state-check
 *   - @ref confc-lspec-state-wait-reply
 *   - @ref confc-lspec-state-wait-status
 *   - @ref confc-lspec-state-grow-cache
 *   - @ref confc-lspec-state-terminal
 *   - @ref confc-lspec-state-failure
 * - @ref confc-lspec-walk
 * - @ref confc-lspec-grow
 * - @ref confc-lspec-thread
 * - @ref confc_dlspec "Detailed Logical Specification"
 *
 * <hr> <!------------------------------------------------------------>
 * @section confc-lspec-state State Specification
 *
 * A state machine is embedded into m0_confc_ctx structure as its @ref
 * m0_confc_ctx::fc_mach "fc_mach" member.  m0_confc_ctx_init()
 * initialises the state machine and sets its state to S_INITIAL.
 *
 * @dot
 * digraph confc_ctx_states {
 *     node [fontsize=9];
 *     edge [fontsize=9];
 *     S_INITIAL  [style=filled, fillcolor=lightgrey];
 *     S_TERMINAL [style=filled, fillcolor=lightgrey, label="S_TERMINAL"];
 *     S_FAILURE  [style=filled, fillcolor=lightgrey, label="S_FAILURE"];
 *
 *     S_CHECK [label="{ check_st_in() }\nS_CHECK"];
 *     S_WAIT_REPLY [label="{ wait_reply_st_in() }\nS_WAIT_REPLY"];
 *     S_WAIT_STATUS;
 *     S_GROW_CACHE [label="{ grow_cache_st_in() }\nS_GROW_CACHE"];
 *
 *     S_INITIAL -> S_CHECK;
 *     S_INITIAL -> S_FAILURE [label="invalid confc_open() args"];
 *     S_CHECK -> S_TERMINAL [label=
 *   "path target is reached\nand it is M0_CS_READY"];
 *     S_CHECK -> S_FAILURE [label="error"];
 *     S_CHECK -> S_WAIT_REPLY [label=
 *   "M0_CS_MISSING object\nis reached\n{ set status to M0_CS_LOADING;\nsend request }"];
 *     S_WAIT_REPLY -> S_FAILURE [label="timeout\nor\nrc != 0"];
 *     S_WAIT_REPLY -> S_GROW_CACHE [label="response received,\nrc == 0"];
 *     S_WAIT_REPLY -> S_SKIP_CONFD [label="network error,\nneed next confd"];
 *     S_WAIT_REPLY -> S_RETRY_CONFD [label="confd not yet started,\nrc == -EAGAIN"];
 *     S_RETRY_CONFD -> S_CHECK [label="repost the configuration request\n"];
 *     S_SKIP_CONFD -> S_CHECK [label="successfully reconnected\nto next confd"];
 *     S_SKIP_CONFD -> S_FAILURE [label="no confd to\nconnect anymore"];
 *     S_GROW_CACHE -> S_FAILURE [label="error"];
 *     S_GROW_CACHE -> S_CHECK [label="done"];
 *     S_CHECK -> S_WAIT_STATUS [label=
 *   "M0_CS_LOADING object\nis reached\n{ m0_clink_add(&obj->co_chan) }"];
 *     S_WAIT_STATUS -> S_CHECK [label="\"status updated\" event"];
 * }
 * @enddot
 *
 * @subsection confc-lspec-state-initial S_INITIAL
 *
 * Summary: m0_confc_ctx has just been initialised.
 *
 * m0_confc_open() populates m0_confc_ctx::fc_path array (path_copy())
 * and posts an AST to m0_confc::cc_group.
 *
 * @note  m0_sm_ast_post() signals group's clink. Current design of
 *        confc assumes that some thread will respond to this event by
 *        calling m0_sm_asts_run().
 *
 * When the AST, posted by m0_confc_open(), is run, it moves a state
 * machine (m0_confc_ctx::fc_mach) to S_CHECK state.
 *
 * @subsection confc-lspec-state-check S_CHECK
 *
 * Summary: Traversing the path, checking whether the requested
 * configuration object is accessible.
 *
 * When S_CHECK state is entered, check_st_in() callback is invoked.
 * It calls path_walk() and, depending on the value returned by this
 * call, moves the state machine to another state:
 *
@verbatim
+--------------------+-----------------+
| path_walk() result |   next state    |
+--------------------+-----------------+
|    M0_CS_READY     |  S_TERMINAL     |
|    M0_CS_MISSING   |  S_WAIT_REPLY   |
|    M0_CS_LOADING   |  S_WAIT_STATUS  |
|         < 0        |  S_FAILURE      |
+--------------------+-----------------+
@endverbatim
 *
 * The algorithm of path_walk() is described below (see @ref
 * confc-lspec-walk).
 *
 * @subsection confc-lspec-state-wait-reply S_WAIT_REPLY
 *
 * Summary: Waiting for confd's reply to arrive.
 *
 * When a state machine is about to enter S_WAIT_REPLY state,
 * wait_reply_st_in() callback is executed. This callback sends
 * configuration request (m0_confc_ctx::fc_rpc_item) to the confd,
 * using m0_rpc_post().
 *
 * The state machine remains in S_WAIT_REPLY state until a reply from confd
 * arrives. This event triggers on_replied() callback.  If m0_rpc_item::ri_error
 * is non-zero, on_replied() posts an AST that will eventually move the state
 * machine to S_FAILURE state, in case the confc is not coupled with rconfc.  If
 * ->ri_error is zero, on_replied() increments rpc item's reference counter
 * (m0_rpc_item_get()) and posts an AST, scheduling transition to S_GROW_CACHE
 * state.
 *
 * In case confc is coupled with rconfc, and ->ri_error is found non-zero in
 * on_replied(), the state machine is transitioned from S_WAIT_REPLY to
 * S_SKIP_CONFD by posting AST to let rconfc reconnect the confc it is in
 * control of to some other confd server from rconfc's active list.
 *
 * @subsection confc-lspec-state-wait-status S_WAIT_STATUS
 *
 * Summary: Waiting for an object to be filled by another
 * configuration request.
 *
 * A state machine in S_WAIT_STATUS state remains idle until the
 * channel (m0_conf_obj::co_chan) that m0_confc_ctx::fc_clink is
 * registered with is signaled.  Such an event triggers
 * on_object_updated() callback, which de-registers the clink and
 * posts an AST that will eventually move the state machine to S_CHECK
 * state.
 *
 * @note  Object's channel (m0_conf_obj::co_chan) is signaled
 *        (m0_chan_broadcast()) when
 *        -#
 *          object_enrich() completes loading of configuration data
 *          into this object and changes its status to M0_CS_READY
 *          (loading succeeded) or M0_CS_MISSING (loading failed);
 *        -#
 *           the object is closed and its number of references becomes
 *           zero.  (This case is not applicable to S_WAIT_STATUS
 *           state.)
 *
 * @subsection confc-lspec-state-grow-cache S_SKIP_CONFD
 *
 * Summary: Skipping current confd the confc is connected to and reconnecting to
 * some other confd server running the same configuration version.
 *
 * m0_confc::cc_gops::go_skip() is called to let the confc be reconnected. When
 * reconnection succeeded, the state machine is transitioned to S_CHECK state to
 * repeat the last missing object reading. In case of reconnection failure due
 * to having no more responsive confd servers in the active list, the state
 * machine is transitioned to S_FAILURE state.
 *
 * @subsection confc-lspec-state-grow-cache S_GROW_CACHE
 *
 * Summary: Applying configuration data contained in confd's reply.
 *
 * When a state machine is entering S_GROW_CACHE state,
 * grow_cache_st_in() callback is invoked.  If the error code
 * contained in confd's response (m0_conf_fetch_resp::fr_rc) is zero,
 * the callback calls cache_grow() function (see @ref
 * confc-lspec-grow below).  The callback "releases" rpc item by
 * calling m0_rpc_item_put().  If ->fr_rc == 0 and cache_grow()
 * succeeds, grow_cache_st_in() moves the state machine to S_CHECK
 * state, otherwise --- to S_FAILURE state.
 *
 * @subsection confc-lspec-state-terminal S_TERMINAL
 *
 * Summary: Configuration retrieval succeeded.
 *
 * @subsection confc-lspec-state-failure S_FAILURE
 *
 * Summary: Configuration retrieval failed.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confc-lspec-walk Walking the DAG
 *
 * path_walk() begins with locking the confc cache (m0_confc::cc_lock);
 * it unlocks the cache before returning.
 *
 * The function "moves" along the DAG of cached configuration objects,
 * starting at m0_confc_ctx::fc_origin object and following
 * m0_confc_ctx::fc_path.  Next object is found by calling
 * m0_conf_obj_ops::coo_lookup() with current object and path
 * component as parameters.  The iteration continues until
 * ->coo_lookup() fails, or a stub is met, or the end of path is
 * reached.
 *
 * path_walk_complete() applies the results of path walking:
 * increments reference counter of M0_CS_READY object, allocates and
 * fills m0_conf_fetch request for M0_CS_MISSING object, or registers
 * clink with the channel of M0_CS_LOADING object.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confc-lspec-grow Growing the cache
 *
 * cache_grow() locks the cache (m0_confc::cc_cache) and unlocks
 * before returning.  The function performs the following operations
 * for every object descriptor (m0_confx_obj, defined in
 * conf/onwire.ff):
 *   -#
 *      m0_conf_obj_find(): tries to find an object with the same
 *      identity (type and id) in the registry of cached objects.
 *      If the object is not found, a stub is created and added to the
 *      cache.
 *   -#
 *      object_enrich(): compares cached object with the descriptor
 *      received from the confd.  If a discrepancy is found
 *      (!m0_conf_obj_match()), the function returns an error code.
 *
 *      If there is no discrepancy, and the cached object is a stub,
 *      object_enrich() fills the cached object with configuration data
 *      (m0_conf_obj_fill()) and signals object's channel.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confc-lspec-thread Threading and Concurrency Model
 *
 * There are as many state machines in operation as there are
 * unfinished m0_confc_open*() requests.
 *
 * At most one state transition (m0_sm_state_descr::sd_in()) can be
 * running at any given time.  Synchronization of state transitions is
 * achieved by using m0_sm_group (m0_confc::cc_group).
 *
 * m0_confc instance and confc cache are protected from concurrent
 * modifications by m0_confc::cc_lock mutex, aka cache lock.  Group
 * lock (m0_confc::cc_group::s_lock) cannot be used for this purpose,
 * because it does not prevent the application from modifying the
 * cache with m0_confc_close().
 *
 * If a function needs both locks -- group lock and cache lock -- for
 * its operation, group lock must be acquired first.  Note, that the
 * "function" here cannot be something invoked from an AST callback,
 * because otherwise it would deadlock on the group mutex.
 *
 * A user managing the state machine group (m0_confc::cc_group) is
 * responsible for making sure m0_sm_asts_run() is called when
 * m0_sm_group::s_clink is signaled.  See @ref sm (search for
 * `"ast" thread'.)
 */

/**
 * @defgroup confc_dlspec confc Internals
 *
 * @see @ref conf, @ref confc-lspec "Logical Specification of confc"
 *
 * @{
 */

/* ------------------------------------------------------------------
 * State definitions
 * ------------------------------------------------------------------ */

static int check_st_in(struct m0_sm *mach);       /* S_CHECK */
static int wait_reply_st_in(struct m0_sm *mach);  /* S_WAIT_REPLY */
static int skip_confd_st_in(struct m0_sm *mach);  /* S_SKIP_CONFD */
static int retry_confd_st_in(struct m0_sm *mach); /* S_RETRY_CONFD */
static int grow_cache_st_in(struct m0_sm *mach);  /* S_GROW_CACHE */
static int failure_st_in(struct m0_sm *mach);     /* S_FAILURE */

static bool check_st_invariant(const struct m0_sm *mach);    /* S_CHECK */
static bool failure_st_invariant(const struct m0_sm *mach);  /* S_FAILURE */
static bool terminal_st_invariant(const struct m0_sm *mach); /* S_TERMINAL */

/** States of m0_confc_ctx::fc_mach. */
enum confc_ctx_state { S_INITIAL, S_CHECK, S_WAIT_REPLY, S_WAIT_STATUS,
		       S_RETRY_CONFD, S_SKIP_CONFD, S_GROW_CACHE, S_FAILURE,
		       S_TERMINAL, S_NR };

static struct m0_sm_state_descr confc_ctx_states[S_NR] = {
	[S_INITIAL] = {
		.sd_flags     = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name      = "S_INITIAL",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_CHECK, S_FAILURE)
	},
	[S_CHECK] = {
		.sd_flags     = 0,
		.sd_name      = "S_CHECK",
		.sd_in        = check_st_in,
		.sd_ex        = NULL,
		.sd_invariant = check_st_invariant,
		.sd_allowed   = M0_BITS(S_WAIT_REPLY, S_WAIT_STATUS, S_TERMINAL,
					S_FAILURE)
	},
	[S_WAIT_REPLY] = {
		.sd_flags     = 0,
		.sd_name      = "S_WAIT_REPLY",
		.sd_in        = wait_reply_st_in,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_GROW_CACHE, S_RETRY_CONFD, S_FAILURE,
					S_SKIP_CONFD)
	},
	[S_WAIT_STATUS] = {
		.sd_flags     = 0,
		.sd_name      = "S_WAIT_STATUS",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_CHECK)
	},
	[S_RETRY_CONFD] = {
		.sd_flags     = 0,
		.sd_name      = "S_RETRY_CONFD",
		.sd_in        = retry_confd_st_in,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_CHECK)
	},
	[S_SKIP_CONFD] = {
		.sd_flags     = 0,
		.sd_name      = "S_SKIP_CONFD",
		.sd_in        = skip_confd_st_in,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_CHECK, S_FAILURE)
	},
	[S_GROW_CACHE] = {
		.sd_flags     = 0,
		.sd_name      = "S_GROW_CACHE",
		.sd_in        = grow_cache_st_in,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_CHECK, S_FAILURE)
	},
	[S_FAILURE] = {
		.sd_flags     = M0_SDF_FAILURE | M0_SDF_FINAL,
		.sd_name      = "S_FAILURE",
		.sd_in        = failure_st_in,
		.sd_ex        = NULL,
		.sd_invariant = failure_st_invariant,
		.sd_allowed   = 0
	},
	[S_TERMINAL] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "S_TERMINAL",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = terminal_st_invariant,
		.sd_allowed   = 0
	}
};

static const struct m0_sm_conf confc_ctx_states_conf = {
	.scf_name      = "states of m0_confc_ctx::fc_mach",
	.scf_nr_states = S_NR,
	.scf_state     = confc_ctx_states
};

/* ------------------------------------------------------------------
 * Bob types and invariants
 * ------------------------------------------------------------------ */

static bool _confc_check(const void *bob);
static bool _ctx_check(const void *bob);

static const struct m0_bob_type confc_bob = {
	.bt_name         = "m0_confc",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_confc, cc_magic),
	.bt_magix        = M0_CONFC_MAGIC,
	.bt_check        = _confc_check
};
M0_BOB_DEFINE(static, &confc_bob, m0_confc);

static const struct m0_bob_type ctx_bob = {
	.bt_name         = "m0_confc_ctx",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_confc_ctx, fc_magic),
	.bt_magix        = M0_CONFC_CTX_MAGIC,
	.bt_check        = _ctx_check
};
M0_BOB_DEFINE(static, &ctx_bob, m0_confc_ctx);

M0_INTERNAL bool m0_confc_invariant(const struct m0_confc *confc)
{
	return m0_confc_bob_check(confc);
}

static bool ctx_invariant(const struct m0_confc_ctx *ctx)
{
	M0_PRE(ctx != NULL);
	return m0_confc_ctx_bob_check(ctx);
}

/* ------------------------------------------------------------------
 * m0_confc
 * ------------------------------------------------------------------ */
static int confc_cache_preload(struct m0_confc *confc, const char *local_conf);
static int connect_to_confd(struct m0_confc *confc, const char *confd_addr,
			    struct m0_rpc_machine *rpc_mach);
static void disconnect_from_confd(struct m0_confc *confc);
static void confc_lock(struct m0_confc *confc);
static void confc_unlock(struct m0_confc *confc);
static bool confc_is_locked(const struct m0_confc *confc);
static void clink_cleanup_fini(struct m0_clink *link);

M0_INTERNAL bool m0_confc_is_inited(const struct m0_confc *confc)
{
	return confc->cc_group != NULL;
}

M0_INTERNAL bool m0_confc_is_online(const struct m0_confc *confc)
{
	return confc->cc_rlink.rlk_conn.c_rpc_machine != NULL;
}

static bool not_empty(const char *s)
{
	return s != NULL && *s != '\0';
}

static int confc_cache_create(struct m0_confc *confc,
			      const char *local_conf)
{
	int rc;

	M0_ENTRY("confc=%p", confc);
	M0_PRE(confc_is_locked(confc));

	m0_conf_cache_init(&confc->cc_cache, &confc->cc_lock);

	/* Create stub for root object */
	rc = m0_conf_obj_find(&confc->cc_cache, &M0_CONF_ROOT_FID,
	                      &confc->cc_root);
	if (rc != 0)
		return M0_ERR(rc);

	if (not_empty(local_conf))
		rc = confc_cache_preload(confc, local_conf);
	return M0_RC(rc);
}

M0_INTERNAL int m0_confc_reconnect(struct m0_confc       *confc,
				   struct m0_rpc_machine *rpc_mach,
				   const char            *confd_addr)
{
	int rc;

	M0_ENTRY("confc = %p, confd_addr = %s", confc, confd_addr);
	confc_lock(confc);
	if (m0_confc_is_online(confc))
		disconnect_from_confd(confc);
	M0_ASSERT(!m0_confc_is_online(confc));
	if (confd_addr == NULL)
		/*
		 * Just want to disconnect confc in rconfc_conductor_drained()
		 * Don't return the error because log messages confuse.
		 */
		rc = 0;
	else if (*confd_addr == '\0')
		rc = M0_ERR(-EINVAL);
	else
		rc = connect_to_confd(confc, confd_addr, rpc_mach);
	M0_POST(ergo(rc == 0,
		     not_empty(confd_addr) == m0_confc_is_online(confc)));
	M0_POST(m0_confc_invariant(confc));
	confc_unlock(confc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_confc_init_wait(struct m0_confc       *confc,
				   struct m0_sm_group    *sm_group,
				   const char            *confd_addr,
				   struct m0_rpc_machine *rpc_mach,
				   const char            *local_conf,
				   uint64_t               timeout_ns)
{
	int rc;

	M0_ENTRY("confc=%p", confc);
	M0_PRE(sm_group != NULL);
	M0_PRE(ergo(not_empty(confd_addr), rpc_mach != NULL));
	M0_LOG(M0_DEBUG, "confd=%s lconf=%s", confd_addr, local_conf);

	m0_mutex_init(&confc->cc_lock);
	confc_lock(confc);
	rc = confc_cache_create(confc, local_conf);
	confc_unlock(confc);
	if (rc != 0)
		goto err;

	confc->cc_rpc_timeout = timeout_ns;
	M0_SET0(m0_confc2conn(confc));
	M0_ASSERT(!m0_confc_is_online(confc));
	if (not_empty(confd_addr))
		rc = connect_to_confd(confc, confd_addr, rpc_mach);

	if (rc == 0) {
		confc->cc_group = sm_group;
		confc->cc_nr_ctx = 0;
		m0_confc_bob_init(confc);
		m0_mutex_init(&confc->cc_unatt_guard);
		m0_chan_init(&confc->cc_unattached, &confc->cc_unatt_guard);
		confc->cc_gops = NULL;
		m0_clink_init(&confc->cc_drain, NULL);

		M0_POST(not_empty(confd_addr) == m0_confc_is_online(confc));
		M0_POST(m0_confc_invariant(confc));
		return M0_RC(0);
	}
err:
	confc->cc_root = NULL;
	m0_conf_cache_fini(&confc->cc_cache);
	m0_mutex_fini(&confc->cc_lock);

	return M0_ERR_INFO(rc, "confc=%p confd_addr=%s", confc, confd_addr);
}

M0_INTERNAL int m0_confc_init(struct m0_confc       *confc,
			      struct m0_sm_group    *sm_group,
			      const char            *confd_addr,
			      struct m0_rpc_machine *rpc_mach,
			      const char            *local_conf)
{
	return m0_confc_init_wait(confc, sm_group, confd_addr, rpc_mach,
				  local_conf, M0_TIME_NEVER);
}

M0_INTERNAL void m0_confc_fini(struct m0_confc *confc)
{
	M0_ENTRY("confc=%p", confc);
	M0_PRE(confc->cc_nr_ctx == 0);

	clink_cleanup_fini(&confc->cc_drain);
	m0_chan_fini_lock(&confc->cc_unattached);
	m0_mutex_fini(&confc->cc_unatt_guard);
	confc->cc_gops = NULL;
	m0_confc_bob_fini(confc); /* performs _confc_check() */

	if (m0_confc_is_online(confc))
		disconnect_from_confd(confc);

	confc->cc_group = NULL;
	confc->cc_root = NULL;
	m0_conf_cache_fini(&confc->cc_cache);
	m0_mutex_fini(&confc->cc_lock);

	M0_LEAVE();
}

M0_INTERNAL struct m0_confc *m0_confc_from_obj(const struct m0_conf_obj *obj)
{
	return bob_of(obj->co_cache, struct m0_confc, cc_cache, &confc_bob);
}

M0_INTERNAL void m0_confc_gate_ops_set(struct m0_confc          *confc,
				       struct m0_confc_gate_ops *gops)
{
	M0_PRE(confc != NULL);
	M0_PRE(gops == NULL || gops->go_check != NULL);
	clink_cleanup_fini(&confc->cc_drain);
	confc->cc_gops = gops;
	if (gops != NULL && gops->go_drain != NULL) {
		m0_clink_init(&confc->cc_drain, confc->cc_gops->go_drain);
		m0_clink_add_lock(&confc->cc_unattached, &confc->cc_drain);
	}
}

static bool _confc_check(const void *bob)
{
	const struct m0_confc *confc = bob;
	return
		_0C(confc->cc_group != NULL) &&
		_0C(confc->cc_root == NULL ||
		    m0_conf_obj_invariant(confc->cc_root));
}

static void clink_cleanup_fini(struct m0_clink *link)
{
	if (link->cl_chan != NULL) {
		M0_PRE(m0_chan_is_locked(link->cl_chan));
		if (m0_clink_is_armed(link)) {
			m0_clink_del(link);
			m0_clink_fini(link);
			M0_SET0(link);
		}
	}
}

/* ------------------------------------------------------------------
 * m0_confc_ctx
 * ------------------------------------------------------------------ */

static bool on_object_updated(struct m0_clink *link);
static bool request_check(const struct m0_confc_ctx *ctx);
static bool eop(const struct m0_fid *buf);
static void confc_group_lock(const struct m0_confc *confc);
static void confc_group_unlock(const struct m0_confc *confc);
static bool confc_group_is_locked(const struct m0_confc *confc);

M0_INTERNAL int
m0_confc_ctx_init(struct m0_confc_ctx *ctx, struct m0_confc *confc)
{
	bool ok;

	M0_ENTRY("ctx=%p", ctx);
	M0_PRE(m0_confc_invariant(confc));

	confc_lock(confc);
	ok = confc->cc_gops == NULL ||
		/**
		 * This call may block.
		 * @see rconfc_gate_check()
		 */
		confc->cc_gops->go_check(confc);
	if (ok) {
		M0_LOG(M0_DEBUG, "ctx=%p confc=%p nr_ctx: %"PRIu32" -> %"PRIu32,
		       ctx, confc, confc->cc_nr_ctx, confc->cc_nr_ctx + 1);
		M0_CNT_INC(confc->cc_nr_ctx); /* attach to m0_confc */
	}
	confc_unlock(confc);
	if (!ok)
		return M0_ERR_INFO(-EPERM, "Conf reading is not allowed"
				   " [ctx=%p]", ctx);

	*ctx = (struct m0_confc_ctx){ .fc_confc = confc };
	m0_sm_init(&ctx->fc_mach, &confc_ctx_states_conf, S_INITIAL,
		   confc->cc_group);
	ctx->fc_ast.sa_datum = &ctx->fc_ast_datum;
	m0_clink_init(&ctx->fc_clink, on_object_updated);
	m0_confc_ctx_bob_init(ctx);

	M0_POST(ctx_invariant(ctx));
	M0_LEAVE("ctx=%p", ctx);
	return 0;
}

M0_INTERNAL void m0_confc_ctx_fini_locked(struct m0_confc_ctx *ctx)
{
	struct m0_confc *confc = ctx->fc_confc;

	M0_ENTRY("ctx=%p", ctx);
	M0_PRE(ctx_invariant(ctx));
	M0_PRE(confc_group_is_locked(confc));

	m0_clink_fini(&ctx->fc_clink);
	ctx->fc_origin = NULL;

	confc_lock(confc);
	if (ctx->fc_result != NULL) {
		M0_ASSERT(ctx->fc_mach.sm_state == S_TERMINAL);
		m0_conf_obj_put(ctx->fc_result);
	}
	M0_LOG(M0_DEBUG, "ctx=%p confc=%p nr_ctx: %"PRIu32" -> %"PRIu32,
	       ctx, confc, confc->cc_nr_ctx, confc->cc_nr_ctx - 1);
	M0_CNT_DEC(confc->cc_nr_ctx); /* detach from m0_confc */
	if (confc->cc_nr_ctx == 0)
		m0_chan_signal_lock(&confc->cc_unattached);
	confc_unlock(confc);

	m0_sm_fini(&ctx->fc_mach);

	if (ctx->fc_rpc_item != NULL) {
		m0_rpc_item_put_lock(ctx->fc_rpc_item);
		ctx->fc_rpc_item = NULL;
	}

	m0_confc_ctx_bob_fini(ctx);
	ctx->fc_confc = NULL;

	M0_LEAVE();
}

M0_INTERNAL void m0_confc_ctx_fini(struct m0_confc_ctx *ctx)
{
	struct m0_confc *confc = ctx->fc_confc;

	M0_ENTRY();
	confc_group_lock(confc); /* needed for m0_sm_fini() */
	m0_confc_ctx_fini_locked(ctx);
	confc_group_unlock(confc);
	M0_LEAVE();
}

static bool _ctx_check(const void *bob)
{
	const struct m0_confc_ctx *ctx = bob;
	const struct m0_sm        *mach = &ctx->fc_mach;

	return  _0C(m0_confc_invariant(ctx->fc_confc)) &&
		_0C(ctx->fc_ast.sa_datum == &ctx->fc_ast_datum) &&
		_0C(ctx->fc_clink.cl_cb == on_object_updated) &&
		_0C(ergo(ctx->fc_rpc_item != NULL, request_check(ctx))) &&
		_0C(ergo(mach->sm_state == S_TERMINAL, mach->sm_rc == 0)) &&
		_0C(ergo(mach->sm_state == S_FAILURE, mach->sm_rc < 0)) &&
		_0C(ergo(ctx->fc_origin != NULL,
			 m0_conf_obj_invariant(ctx->fc_origin)));
}

M0_INTERNAL bool m0_confc_ctx_is_completed(const struct m0_confc_ctx *ctx)
{
	M0_PRE(confc_group_is_locked(ctx->fc_confc));
	M0_PRE(ctx_invariant(ctx));
	return M0_IN(ctx->fc_mach.sm_state, (S_TERMINAL, S_FAILURE));
}

M0_INTERNAL bool m0_confc_ctx_is_completed_lock(const struct m0_confc_ctx *ctx)
{
	bool res;

	confc_group_lock(ctx->fc_confc);
	res = m0_confc_ctx_is_completed(ctx);
	confc_group_unlock(ctx->fc_confc);
	return res;
}

M0_INTERNAL int32_t m0_confc_ctx_error(const struct m0_confc_ctx *ctx)
{
	M0_PRE(m0_confc_ctx_is_completed(ctx));
	return ctx->fc_mach.sm_rc;
}

M0_INTERNAL int32_t m0_confc_ctx_error_lock(const struct m0_confc_ctx *ctx)
{
	M0_PRE(m0_confc_ctx_is_completed_lock(ctx));
	return ctx->fc_mach.sm_rc;
}

M0_INTERNAL struct m0_conf_obj *m0_confc_ctx_result(struct m0_confc_ctx *ctx)
{
	struct m0_conf_obj *res = ctx->fc_result;

	M0_ENTRY("ctx=%p", ctx);
	M0_PRE(ctx_invariant(ctx));
	M0_PRE(ctx->fc_mach.sm_state == S_TERMINAL);
	M0_PRE(res != NULL && m0_conf_obj_invariant(res));

	ctx->fc_result = NULL;

	M0_LEAVE("retval=%p", res);
	return res;
}

/* ----------------------------------------------------------------
 * sm_waiter
 * ---------------------------------------------------------------- */

struct sm_waiter {
	struct m0_confc_ctx w_ctx;
	struct m0_clink     w_clink;
};

/** Filters out intermediate state transitions of m0_confc_ctx::fc_mach. */
static bool sm__filter(struct m0_clink *link)
{
	return !m0_confc_ctx_is_completed(&container_of(link, struct sm_waiter,
							w_clink)->w_ctx);
}

static int sm_waiter_init(struct sm_waiter *w, struct m0_confc *confc)
{
	int rc = m0_confc_ctx_init(&w->w_ctx, confc);
	if (rc == 0) {
		m0_clink_init(&w->w_clink, sm__filter);
		m0_clink_add_lock(&w->w_ctx.fc_mach.sm_chan, &w->w_clink);
	}
	return M0_RC(rc);
}

static void sm_waiter_fini(struct sm_waiter *w)
{
	m0_clink_del_lock(&w->w_clink);
	m0_clink_fini(&w->w_clink);
	m0_confc_ctx_fini(&w->w_ctx);
}

static int sm_waiter_wait(struct sm_waiter *w, struct m0_conf_obj **result)
{
	int rc;

	M0_ENTRY();

	/*
	 * The context may be changed by AST callback, and it is possible that
	 * the one is not in invariant state when checking for completion. To
	 * prevent this, we need to ensure that the confc is locked.
	 */
	while (!m0_confc_ctx_is_completed_lock(&w->w_ctx))
		m0_chan_wait(&w->w_clink);
	rc = m0_confc_ctx_error_lock(&w->w_ctx);
	if (rc == 0)
		*result = m0_confc_ctx_result(&w->w_ctx);

	return M0_RC(rc);
}

/* ------------------------------------------------------------------
 * open/close
 * ------------------------------------------------------------------ */

static void ast_state_set(struct m0_sm_ast *ast, enum confc_ctx_state state);
static void ast_fail(struct m0_sm_ast *ast, int rc);
static int path_copy(const struct m0_fid *src, struct m0_fid *dest,
		     size_t dest_sz);

M0_INTERNAL void m0_confc__open(struct m0_confc_ctx *ctx,
				struct m0_conf_obj  *origin,
				const struct m0_fid *path)
{
	int rc = 0;

	M0_ENTRY("ctx=%p origin=%p", ctx, origin);
	M0_PRE(ctx_invariant(ctx) && ctx->fc_mach.sm_state == S_INITIAL);
	M0_PRE(ctx->fc_origin == NULL && eop(ctx->fc_path));

	if (origin == NULL) {
		ctx->fc_origin = ctx->fc_confc->cc_root;
	} else if (M0_FI_ENABLED("invalid-origin") ||
		   unlikely(origin->co_cache != &ctx->fc_confc->cc_cache)) {
		/*
		 * `origin' may be freed at this point.
		 *
		 * Possible scenario (MERO-2363):
		 *
		 * -- Let confc->cc_root = A.
		 * m0_confc_root_open
		 *  \_ m0_confc_open_sync (origin=A)
		 *      \_ m0_confc__open_sync
		 *          \_ sm_waiter_init
		 *          |   \_ m0_confc_ctx_init
		 *          |       \_ rconfc_gate_check -- blocks until
		 *          |                            -- conf cache is ready
		 * ------------------------------------------------------------
		 * -- Another thread calls _confc_cache_clean(). The function
		 * -- frees all conf objects - including 'A'! - and changes
		 * -- the value of confc->cc_root pointer.
		 * ------------------------------------------------------------
		 *          \_ m0_confc__open (origin=A)
		 * -- Since `A' has already been freed, origin->co_cache will
		 * -- be garbage.
		 */
		rc = M0_ERR_INFO(-EAGAIN, "Invalid origin: %p", origin);
	} else {
		M0_PRE(m0_conf_obj_invariant(origin));
		ctx->fc_origin = origin;
	}
	rc = rc ?: path_copy(path, ctx->fc_path, ARRAY_SIZE(ctx->fc_path));
	if (rc == 0)
		ast_state_set(&ctx->fc_ast, S_CHECK);
	else
		ast_fail(&ctx->fc_ast, rc);
	M0_LEAVE();
}

M0_INTERNAL int m0_confc__open_sync(struct m0_conf_obj **result,
				    struct m0_conf_obj  *origin,
				    const struct m0_fid *path)
{
	struct sm_waiter w;
	int              rc;

	M0_ENTRY();
	M0_PRE(origin != NULL);
	M0_PRE(m0_conf_obj_invariant(origin));

	rc = sm_waiter_init(&w, m0_confc_from_obj(origin));
	if (rc != 0)
		return M0_ERR(rc);
	m0_confc__open(&w.w_ctx, origin, path);
	rc = sm_waiter_wait(&w, result);
	sm_waiter_fini(&w);

	M0_POST(ergo(rc == 0, (*result)->co_status == M0_CS_READY));
	return M0_RC(rc);
}

M0_INTERNAL void m0_confc_close(struct m0_conf_obj *obj)
{
	if (obj != NULL) {
		confc_lock(m0_confc_from_obj(obj));
		m0_conf_obj_put(obj);
		confc_unlock(m0_confc_from_obj(obj));
	}
}

M0_INTERNAL void m0_confc_open_by_fid(struct m0_confc_ctx *ctx,
				      const struct m0_fid *fid)
{
	struct m0_conf_obj *obj;
	int                 rc;

	rc = m0_conf_obj_find_lock(&ctx->fc_confc->cc_cache, fid, &obj);
	if (rc == 0)
		m0_confc_open(ctx, obj, M0_FID0);
	else
		ast_fail(&ctx->fc_ast, rc);
}

M0_INTERNAL int m0_confc_open_by_fid_sync(struct m0_confc      *confc,
					  const struct m0_fid  *fid,
					  struct m0_conf_obj  **result)
{
	struct sm_waiter w;
	int              rc;

	M0_ENTRY();

	rc = sm_waiter_init(&w, confc);
	if (rc != 0)
		return M0_ERR(rc);
	m0_confc_open_by_fid(&w.w_ctx, fid);
	rc = sm_waiter_wait(&w, result);
	sm_waiter_fini(&w);

	M0_POST(ergo(rc == 0, (*result)->co_status == M0_CS_READY));
	return M0_RC(rc);
}

/**
 * Copies path from `src' to `dest'.
 *
 * @retval 0        Success.
 * @retval -E2BIG   `src' path is too long.
 */
static int
path_copy(const struct m0_fid *src, struct m0_fid *dest, size_t dest_sz)
{
	size_t i;

	M0_ENTRY();

	for (i = 0; i < dest_sz && !eop(&src[i]); ++i)
		dest[i] = src[i];

	if (i == dest_sz)
		return M0_ERR(-E2BIG);
	dest[i] = (struct m0_fid)M0_FID0; /* terminate the path */

	return M0_RC(0);
}

/* ------------------------------------------------------------------
 * readdir
 * ------------------------------------------------------------------ */

M0_INTERNAL int m0_confc_readdir(struct m0_confc_ctx *ctx,
				 struct m0_conf_obj  *dir,
				 struct m0_conf_obj **pptr)
{
	int rc;

	M0_ENTRY("ctx=%p dir=%p *pptr=%p", ctx, dir, *pptr);
	M0_PRE(m0_conf_obj_type(dir) == &M0_CONF_DIR_TYPE &&
	       dir->co_cache == &ctx->fc_confc->cc_cache);

	confc_lock(m0_confc_from_obj(dir));
	rc = dir->co_ops->coo_readdir(dir, pptr);
	if (rc == M0_CONF_DIRMISS) {
		/*
		 * Request {origin, [relation, entry]} from confd, where
		 *   origin is dir's parent,
		 *   relation is how dir is referred to by the origin,
		 *   entry is the first missing entry in the dir.
		 */
		struct m0_fid path[] = {
			M0_CONF_CAST(dir, m0_conf_dir)->cd_relfid,
			(*pptr)->co_id,
			M0_FID0
		};
		m0_confc__open(ctx, dir->co_parent, path);
	}
	confc_unlock(m0_confc_from_obj(dir));
	return M0_RC(rc);
}

M0_INTERNAL int m0_confc_readdir_sync(struct m0_conf_obj *dir,
				      struct m0_conf_obj **pptr)
{
	struct sm_waiter w;
	int              rc;

	M0_ENTRY("dir=%p *pptr=%p", dir, *pptr);

	rc = sm_waiter_init(&w, m0_confc_from_obj(dir));
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_confc_readdir(&w.w_ctx, dir, pptr);
	if (rc == M0_CONF_DIRMISS)
		rc = sm_waiter_wait(&w, pptr) ?:
			(*pptr == NULL ? M0_CONF_DIREND : M0_CONF_DIRNEXT);
	else
		M0_ASSERT(M0_IN(rc, (M0_CONF_DIRNEXT, M0_CONF_DIREND)));
	sm_waiter_fini(&w);
	return M0_RC(rc);
}

/* ------------------------------------------------------------------
 * Casts
 * ------------------------------------------------------------------ */

M0_INTERNAL struct m0_rpc_conn *m0_confc2conn(struct m0_confc *confc)
{
	return &confc->cc_rlink.rlk_conn;
}

M0_INTERNAL struct m0_rpc_session *m0_confc2sess(struct m0_confc *confc)
{
	return &confc->cc_rlink.rlk_sess;
}

static struct m0_confc_ctx *mach_to_ctx(struct m0_sm *mach)
{
	return bob_of(mach, struct m0_confc_ctx, fc_mach, &ctx_bob);
}

static const struct m0_confc_ctx *const_mach_to_ctx(const struct m0_sm *mach)
{
	return bob_of(mach, const struct m0_confc_ctx, fc_mach, &ctx_bob);
}

static struct m0_confc_ctx *ast_to_ctx(struct m0_sm_ast *ast)
{
	return bob_of(ast, struct m0_confc_ctx, fc_ast, &ctx_bob);
}

/* ------------------------------------------------------------------
 * State transitions
 *
 * Note, that *_st_in() functions don't need to assert that the group
 * lock is being hold.  This check is part of state machine invariant
 * (m0_sm_invariant()), which is asserted when a state is entered (and
 * left).
 * ------------------------------------------------------------------ */

static int path_walk(struct m0_confc_ctx *ctx);
static int cache_grow(struct m0_confc *confc,
		      const struct m0_conf_fetch_resp *resp);
static struct m0_confc_ctx *item_to_ctx(const struct m0_rpc_item *item);
static uint64_t *confc_cache_ver(struct m0_confc_ctx *ctx);

/** Actions to perform on entering S_CHECK state. */
static int check_st_in(struct m0_sm *mach)
{
	static const int next_state[] = {
		[M0_CS_MISSING] = S_WAIT_REPLY,
		[M0_CS_LOADING] = S_WAIT_STATUS,
		[M0_CS_READY]   = S_TERMINAL
	};
	int rc;
	struct m0_confc_ctx *ctx = mach_to_ctx(mach);

	M0_ENTRY("mach=%p ctx=%p", mach, ctx);

	rc = path_walk(ctx);
	if (rc < 0) {
		mach->sm_rc = rc;
		M0_LEAVE("retval=S_FAILURE");
		return S_FAILURE;
	}

	M0_ASSERT(IS_IN_ARRAY(rc, next_state));
	M0_LEAVE("retval=%d", next_state[rc]);
	return next_state[rc];
}

/** Actions to perform on entering S_WAIT_REPLY state. */
static int wait_reply_st_in(struct m0_sm *mach)
{
	struct m0_confc_ctx *ctx = mach_to_ctx(mach);
	int                  rc;

	M0_ENTRY("mach=%p ctx=%p", mach, ctx);
	M0_PRE(ctx->fc_rpc_item != NULL);

	rc = m0_rpc_post(ctx->fc_rpc_item);
	if (rc == 0)
		return M0_RC(-1);
	mach->sm_rc = rc;
	M0_LEAVE("retval=S_FAILURE");
	return S_FAILURE;
}

/**
 * Change status of origin object of the configuration request from
 * M0_CS_LOADING to M0_CS_MISSING.
 */
static void conf_obj_status_reset(struct m0_confc_ctx *ctx)
{
	struct m0_rpc_item   *item = ctx->fc_rpc_item;
	struct m0_confc      *confc = ctx->fc_confc;
	struct m0_conf_obj   *obj;
	struct m0_conf_fetch *req;

	req = m0_fop_data(m0_rpc_item_to_fop(item));
	confc_lock(confc);
	obj = m0_conf_cache_lookup(&confc->cc_cache, &req->f_origin);
	confc_unlock(confc);
	M0_ASSERT(obj->co_status == M0_CS_LOADING);
	obj->co_status = M0_CS_MISSING;
}

/** Actions to perform on entering S_RETRY_CONFD state. */
static int retry_confd_st_in(struct m0_sm *mach)
{
	struct m0_confc_ctx *ctx = mach_to_ctx(mach);

	M0_ENTRY("mach=%p ctx=%p", mach, ctx);

	conf_obj_status_reset(ctx);
	/* end up with rpc item which is not needed anymore */
	m0_rpc_item_put_lock(ctx->fc_rpc_item);
	ctx->fc_rpc_item = NULL;

	/*
	 * Retry with the last missing object
	 */
	M0_LEAVE("retval=S_CHECK");
	return S_CHECK;
}

/** Actions to perform on entering S_SKIP_CONFD state. */
static int skip_confd_st_in(struct m0_sm *mach)
{
	struct m0_confc_ctx *ctx = mach_to_ctx(mach);
	int                  rc;

	M0_ENTRY("mach=%p ctx=%p", mach, ctx);
	M0_PRE(ctx->fc_confc->cc_gops != NULL &&
	       ctx->fc_confc->cc_gops->go_skip != NULL);

	/* ask rconfc to skip current confd and connect confc to other one */
	rc = ctx->fc_confc->cc_gops->go_skip(ctx->fc_confc);
	if (M0_FI_ENABLED("force_reconnect_success"))
		rc = 0;
	if (rc == 0)
		conf_obj_status_reset(ctx);
	/* end up with rpc item which is not needed anymore */
	m0_rpc_item_put_lock(ctx->fc_rpc_item);
	ctx->fc_rpc_item = NULL;

	mach->sm_rc = rc;
	M0_LEAVE("rc=%d retval=%s", rc, rc == 0 ? "S_CHECK" : "S_FAILURE");
	return rc == 0 ? S_CHECK : S_FAILURE;
}

/** Actions to perform on entering S_GROW_CACHE state. */
static int grow_cache_st_in(struct m0_sm *mach)
{
	struct m0_conf_fetch_resp *resp;
	int                        rc;
	struct m0_confc_ctx       *ctx   = mach_to_ctx(mach);
	struct m0_rpc_item        *item  = ctx->fc_rpc_item;
	struct m0_rpc_machine     *rmach = item->ri_rmachine;

	M0_ENTRY("mach=%p ctx=%p", mach, ctx);
	M0_PRE(item != NULL && item->ri_error == 0 && item->ri_reply != NULL &&
	       rmach != NULL);

	resp = m0_fop_data(m0_rpc_item_to_fop(item->ri_reply));
	rc = resp->fr_rc;
	if (*confc_cache_ver(ctx) == M0_CONF_VER_UNKNOWN)
		/* the very first fetch occurred */
		*confc_cache_ver(ctx) = resp->fr_ver;
	else if (*confc_cache_ver(ctx) != resp->fr_ver)
		rc = M0_ERR(-EPROTO);

	if (rc == 0)
		rc = cache_grow(ctx->fc_confc, resp);

	m0_rpc_machine_lock(rmach);
	/* Let the rpc layer free memory allocated for response. */
	m0_rpc_item_put(item->ri_reply);
	/* The item has been consumed and is not needed any more. */
	m0_rpc_item_put(item);
	m0_rpc_machine_unlock(rmach);
	ctx->fc_rpc_item = NULL;

	mach->sm_rc = rc;
	M0_LEAVE("rc=%d retval=%s", rc, rc == 0 ? "S_CHECK" : "S_FAILURE");
	return rc == 0 ? S_CHECK : S_FAILURE;
}

/** Actions to perform on entering S_FAILURE state. */
static int failure_st_in(struct m0_sm *mach)
{
	struct m0_confc_ctx *ctx = mach_to_ctx(mach);
	M0_ENTRY("mach=%p ctx=%p", mach, ctx);

	/*
	 * Unset M0_CS_LOADING object with path_walk() in order for
	 * m0_confc_fini() not to fail:
	 *    m0_confc_fini
	 *     \_ m0_conf_cache_fini
	 *         \_ _obj_del
	 *             \_ m0_conf_obj_delete
	 *                 \_ M0_PRE(obj->co_status != M0_CS_LOADING)
	 *
	 * ctx->fc_origin may be NULL; see "Invalid origin" in m0_confc__open().
	 */
	if (likely(ctx->fc_origin != NULL))
		(void)path_walk(ctx);

	return M0_RC(-1);
}

/** Handles `RPC replied' event (i.e. response arrival or an error). */
static void on_replied(struct m0_rpc_item *item)
{
	struct m0_confc_ctx *ctx = item_to_ctx(item);
	int                  rc;

	M0_ENTRY("item=%p ctx=%p", item, ctx);
	M0_PRE(ctx_invariant(ctx));

	rc = m0_rpc_item_error(item);
	if (M0_FI_ENABLED("fail_rpc_reply"))
		rc = M0_ERR(-EPERM);
	if (rc == 0) {
		m0_rpc_item_get(item->ri_reply);
		ast_state_set(&ctx->fc_ast, S_GROW_CACHE);
	} else {
		/*
		 * See if the confc is a 'conductor' governed by rconfc. In case
		 * it is, try to switch to other confd running the same version.
		 */
		if (ctx->fc_confc->cc_gops != NULL &&
		    ctx->fc_confc->cc_gops->go_skip != NULL)
			ast_state_set(&ctx->fc_ast, S_SKIP_CONFD);
		else if (rc == -EAGAIN)
			ast_state_set(&ctx->fc_ast, S_RETRY_CONFD);
		else
			ast_fail(&ctx->fc_ast, rc);
	}
	M0_LEAVE("rc=%d", rc);
}

/** Handles `object loading completed' and `object unpinned' events. */
static bool on_object_updated(struct m0_clink *link)
{
	struct m0_confc_ctx *ctx = bob_of(link->cl_group, struct m0_confc_ctx,
					  fc_clink, &ctx_bob);

	M0_ENTRY();
	M0_PRE(confc_is_locked(ctx->fc_confc));

	m0_clink_del(&ctx->fc_clink);
	ast_state_set(&ctx->fc_ast, S_CHECK);

	M0_LEAVE();
	return true; /* event is consumed */
}

static bool check_st_invariant(const struct m0_sm *mach)
{
	const struct m0_confc_ctx *ctx = const_mach_to_ctx(mach);
	return mach->sm_rc == 0 && ctx->fc_result == NULL &&
		ctx_invariant(ctx);
}

static bool failure_st_invariant(const struct m0_sm *mach)
{
	const struct m0_confc_ctx *ctx = const_mach_to_ctx(mach);
	return ctx->fc_result == NULL && ctx->fc_mach.sm_rc < 0;
}

static bool terminal_st_invariant(const struct m0_sm *mach)
{
	/* We do not check m0_confc_ctx::fc_result, because it may
	 * have been unset by m0_confc_ctx_result(). */
	return mach->sm_rc == 0;
}

static uint64_t *confc_cache_ver(struct m0_confc_ctx *ctx)
{
	return &ctx->fc_confc->cc_cache.ca_ver;
}

/* ------------------------------------------------------------------
 * Walkies
 *
 *         They're "Techno Trousers". Ex-NASA. Fantastic for walkies!
 * ------------------------------------------------------------------ */

static int path_walk_complete(struct m0_confc_ctx *ctx, struct m0_conf_obj *obj,
			      size_t ri);
static int request_create(struct m0_confc_ctx *ctx,
			  const struct m0_conf_obj *orig, size_t ri);
static bool confc_group_is_locked(const struct m0_confc *confc);

/** Last path element? */
static bool eop(const struct m0_fid *id)
{
	return !m0_fid_is_set(id);
}

/**
 * Follows the path, checking statuses of met objects.
 *
 * @retval M0_CS_READY    Path target is reachable.
 *
 * @retval M0_CS_MISSING  One of the intermediate objects or the target
 *                        itself is M0_CS_MISSING.
 *                        path_walk_complete(), which is called from
 *                        path_walk(), changes status of this object
 *                        to M0_CS_LOADING and fills ctx->fc_req.
 *
 * @retval M0_CS_LOADING  Neither path target nor missing objects can
 *                        be reached because of M0_CS_LOADING object
 *                        blocking the path.  If ctx->fc_mach is in
 *                        S_FAILURE state, path_walk_complete() reverts
 *                        status of this object to M0_CS_MISSING and
 *                        signals object's channel.  Otherwise
 *                        path_walk_complete() registers ctx->fc_clink
 *                        with the channel of loading object.
 *
 * @retval -ENOENT        ctx->fc_path refers to a nonexistent object.
 * @retval -ENOMEM        Request allocation failed.
 *
 * @see @ref confc-lspec-state
 */
static int path_walk(struct m0_confc_ctx *ctx)
{
	struct m0_conf_obj *obj;
	size_t              ri;
	int                 rc;

	M0_ENTRY("ctx=%p", ctx);
	M0_PRE(confc_group_is_locked(ctx->fc_confc));
	M0_PRE(m0_conf_obj_invariant(ctx->fc_origin));
	M0_PRE(M0_IN(ctx->fc_mach.sm_state, (S_CHECK, S_FAILURE)));

	confc_lock(ctx->fc_confc);

	for (rc = 0, obj = ctx->fc_origin, ri = 0;
	     rc == 0 && obj->co_status == M0_CS_READY &&
		     !eop(&ctx->fc_path[ri]);
	     ++ri)
		rc = obj->co_ops->coo_lookup(obj, &ctx->fc_path[ri], &obj);

	if (rc == 0)
		rc = path_walk_complete(ctx, obj, ri);
	else
		M0_ASSERT(rc == -ENOENT);

	confc_unlock(ctx->fc_confc);

	M0_POST(confc_group_is_locked(ctx->fc_confc));
	return M0_RC(rc);
}

/**
 * Applies the results of path walking.
 *
 * @returns original status of the reached configuration object.
 *
 * @param ctx  Configuration retrieval context.
 * @param obj  The object reached by a path walk.
 * @param ri   The position in ctx->fc_path[] where the remaining (not
 *             visited) path components start.
 */
static int
path_walk_complete(struct m0_confc_ctx *ctx, struct m0_conf_obj *obj, size_t ri)
{
	int rc;

	M0_ENTRY("ctx=%p obj=%p ri=%zu", ctx, obj, ri);
	M0_PRE(confc_group_is_locked(ctx->fc_confc));
	M0_PRE(confc_is_locked(ctx->fc_confc));

	switch (obj->co_status) {
	case M0_CS_READY:
		M0_ASSERT(eop(&ctx->fc_path[ri]));

		m0_conf_obj_get(obj);
		ctx->fc_result = obj;

		M0_POST(m0_conf_obj_invariant(ctx->fc_result));
		M0_LEAVE("retval=M0_CS_READY");
		return M0_CS_READY;

	case M0_CS_MISSING:
		obj->co_status = M0_CS_LOADING;
		if (m0_conf_obj_type(obj) == &M0_CONF_DIR_TYPE) {
			/*
			 * Directory objects don't travel over the
			 * network.  Query the parent object.
			 */
			M0_ASSERT(obj->co_parent != NULL);
			obj = obj->co_parent;
			M0_CNT_DEC(ri);
		}
		rc = request_create(ctx, obj, ri);
		if (rc == 0) {
			M0_LEAVE("retval=M0_CS_MISSING");
			return M0_CS_MISSING;
		}
		return M0_RC(rc);

	case M0_CS_LOADING:
		if (ctx->fc_mach.sm_state == S_FAILURE) {
			obj->co_status = M0_CS_MISSING;
			m0_chan_broadcast(&obj->co_chan);
			break;
		}
		m0_clink_add(&obj->co_chan, &ctx->fc_clink);
		M0_LEAVE("retval=M0_CS_LOADING");
		return M0_CS_LOADING;

	default:
		M0_IMPOSSIBLE("Invalid object status");
	}
	return M0_RC(-1);
}

/* ------------------------------------------------------------------
 * AST
 * ------------------------------------------------------------------ */

static void _state_set(struct m0_sm_group *grp M0_UNUSED, struct m0_sm_ast *ast)
{
	int state = *(int *)ast->sa_datum;
	M0_PRE(M0_IN(state, (S_INITIAL, S_CHECK, S_WAIT_REPLY, S_WAIT_STATUS,
			     S_RETRY_CONFD, S_SKIP_CONFD, S_GROW_CACHE,
                             /* note the absence of S_FAILURE */
			     S_TERMINAL)));

	m0_sm_state_set(&ast_to_ctx(ast)->fc_mach, state);
}

static void _fail(struct m0_sm_group *grp M0_UNUSED, struct m0_sm_ast *ast)
{
	m0_sm_fail(&ast_to_ctx(ast)->fc_mach, S_FAILURE, *(int *)ast->sa_datum);
}

static void _ast_post(struct m0_sm_ast *ast,
		      void (*cb)(struct m0_sm_group *, struct m0_sm_ast *),
		      int datum)
{
	struct m0_confc_ctx *ctx = ast_to_ctx(ast);

	ast->sa_cb = cb;
	M0_ASSERT(ast->sa_datum == &ctx->fc_ast_datum);
	ctx->fc_ast_datum = datum;

	m0_sm_ast_post(ctx->fc_confc->cc_group, ast);
}

/** Posts an AST that will advance the state machine to given state. */
static void ast_state_set(struct m0_sm_ast *ast, enum confc_ctx_state state)
{
	_ast_post(ast, _state_set, state);
}

/** Posts an AST that will move the state machine to S_FAILURE state. */
static void ast_fail(struct m0_sm_ast *ast, int rc)
{
	_ast_post(ast, _fail, rc);
}

/* ------------------------------------------------------------------
 * Configuration cache management
 * ------------------------------------------------------------------ */

static int object_enrich(struct m0_conf_obj *dest,
			 const struct m0_confx_obj *src,
			 struct m0_confc *confc)
{
	int rc;

	M0_ENTRY();
	M0_PRE(m0_conf_obj_type(dest) == m0_conf_objx_type(src));
	M0_PRE(confc_is_locked(confc));
	M0_PRE(dest->co_cache == &confc->cc_cache);

	if (!m0_conf_obj_match(dest, src))
		return M0_ERR_INFO(-EPROTO, "Conflict of incoming and cached "
				   "configuration data: src="FID_F" dest="FID_F,
				   FID_P(&src->xo_u.u_header.ch_id),
				   FID_P(&dest->co_id));
	if (dest->co_status == M0_CS_READY)
		return M0_RC(0); /* do nothing */

	rc = m0_conf_obj_fill(dest, src);
	M0_ASSERT(dest->co_status == (rc == 0 ? M0_CS_READY : M0_CS_MISSING));
	m0_chan_broadcast(&dest->co_chan);

	return M0_RC(rc);
}

static int
cached_obj_update(struct m0_confc *confc, const struct m0_confx_obj *flat)
{
	struct m0_conf_obj *obj;

	M0_ENTRY("confc=%p", confc);
	return M0_RC(m0_conf_obj_find(&confc->cc_cache, m0_conf_objx_fid(flat),
				   &obj) ?: object_enrich(obj, flat, confc));
}

/** Adds objects, described by a configuration string, to the cache. */
static int confc_cache_preload(struct m0_confc *confc, const char *local_conf)
{
	struct m0_confx *enc;
	uint32_t         i;
	int              rc;

	M0_ENTRY();
	M0_PRE(confc_is_locked(confc));

	rc = m0_confstr_parse(local_conf, &enc);
	if (rc == 0) {
		for (i = 0; i < enc->cx_nr && rc == 0; ++i)
			rc = cached_obj_update(confc, M0_CONFX_AT(enc, i));
		m0_confx_free(enc);
	}
	return M0_RC(rc);
}

/**
 * Adds new objects, contained in confd's response, to the confc
 * configuration cache.
 *
 * @pre  resp->fr_rc == 0
 */
static int
cache_grow(struct m0_confc *confc, const struct m0_conf_fetch_resp *resp)
{
	struct m0_confx_obj *flat;
	uint32_t             i;
	int                  rc = 0;

	M0_ENTRY();
	M0_PRE(resp->fr_rc == 0);
	M0_PRE(confc_group_is_locked(confc));

	confc_lock(confc);
	for (i = 0; i < resp->fr_data.cx_nr; ++i) {
		flat = M0_CONFX_AT(&resp->fr_data, i);

		if (!m0_conf_fid_is_valid(m0_conf_objx_fid(flat))) {
			M0_LOG(M0_ERROR, "Invalid m0_confx_obj received");
			rc = -EPROTO;
			break;
		}

		rc = cached_obj_update(confc, flat);
		if (rc != 0)
			break;
	}
	confc_unlock(confc);
	return M0_RC(rc);
}

/* ------------------------------------------------------------------
 * Networking
 * ------------------------------------------------------------------ */

static inline m0_time_t confc_deadline(const struct m0_confc *confc)
{
	return confc->cc_rpc_timeout == M0_TIME_NEVER ? M0_TIME_NEVER :
		m0_time_from_now(0, confc->cc_rpc_timeout);
}

static int connect_to_confd(struct m0_confc *confc, const char *confd_addr,
			    struct m0_rpc_machine *rpc_mach)
{
	enum { MAX_RPCS_IN_FLIGHT = 2 };
	int rc;

	M0_ENTRY();
	M0_PRE(not_empty(confd_addr) && rpc_mach != NULL);
	/*
	 * The service fid passed to function below is NULL and hence the
	 * link fom won't try to subscribe for ha callback on configuration
	 * object for service, thus not needing a conf cache lock either.
	 * This is needed because the caller of this function could be
	 * holding a conf cache lock at this point.
	 */
	rc = m0_rpc_link_init(&confc->cc_rlink, rpc_mach, NULL, confd_addr,
			      MAX_RPCS_IN_FLIGHT);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_rpc_link_connect_sync(&confc->cc_rlink, confc_deadline(confc));
	if (rc != 0)
		m0_rpc_link_fini(&confc->cc_rlink);

	M0_POST((rc == 0) == confc->cc_rlink.rlk_connected);
	M0_POST(rc != 0 || m0_confc_is_online(confc));
	return M0_RC(rc);
}

static void disconnect_from_confd(struct m0_confc *confc)
{
	M0_ENTRY();
	M0_PRE(m0_confc_is_online(confc));
	M0_PRE(m0_confc2sess(confc)->s_conn == m0_confc2conn(confc));

	(void)m0_rpc_link_disconnect_sync(&confc->cc_rlink,
					  confc_deadline(confc));
	m0_rpc_link_fini(&confc->cc_rlink);
	M0_LEAVE();
}

/**
 * Wrapper structure, enclosing m0_fop and a pointer to m0_confc_ctx.
 *
 * This structure serves two purposes:
 *
 * 1. It lets rpc layer own the fops created by confc.
 *
 *    The fop is freed by rpc layer during rpc session destruction,
 *    initiated by m0_confc_fini().
 *
 *    We cannot embed m0_fop into a m0_confc_ctx, because m0_confc_ctx
 *    may leave the scope before m0_confc_fini() is called.  In this
 *    case the pointer to rpc item, known to rpc session, would be
 *    invalidated. Session destruction would fail.
 *
 * 2. It allows on_replied() function to obtain m0_confc_ctx object,
 *    given m0_rpc_item.  See item_to_ctx().
 */
struct confc_fop {
	/*
	 * m0_fop must be the first member of confc_fop, so that
	 * m0_fop_release() frees the whole confc_fop object.
	 */
	struct m0_fop        cf_fop;
	struct m0_confc_ctx *cf_ctx;
};

static struct m0_confc_ctx *item_to_ctx(const struct m0_rpc_item *item)
{
	M0_PRE(item != NULL);
	/* XXX TODO use bob_of() */
	return container_of(m0_rpc_item_to_fop(item), struct confc_fop,
			    cf_fop)->cf_ctx;
}

static void confc_fop_release(struct m0_ref *ref)
{
	M0_ENTRY();
	M0_PRE(ref != NULL);

	/*
	 * The memory, pointed to by ->f_path.ab_elems of
	 * m0_conf_fetch, has never been allocated by xcode.
	 * Nevertheless, m0_xcode_free() will try to free this memory,
	 * because `m0_bufs' structure is recognized by xcode's
	 * allocp() as one of the cases requiring dynamic memory
	 * allocation.
	 *
	 * We zero m0_conf_fetch object so that m0_xcode_free() does
	 * not segfault.
	 */
	M0_SET0((struct m0_conf_fetch *)m0_fop_data(
			container_of(ref, struct m0_fop, f_ref)));

	m0_fop_release(ref);

	M0_LEAVE();
}

static struct confc_fop *confc_fop_alloc(struct m0_confc_ctx *ctx)
{
	struct confc_fop *p;
	int               rc;

	M0_ALLOC_PTR(p);
	if (p == NULL)
		return NULL;

	m0_fop_init(&p->cf_fop, &m0_conf_fetch_fopt, NULL, confc_fop_release);
	rc = m0_fop_data_alloc(&p->cf_fop);
	if (rc != 0) {
		m0_free(p);
		return NULL;
	}

	p->cf_ctx = ctx;
	return p;
}

static const struct m0_rpc_item_ops confc_item_ops = {
	.rio_replied = on_replied,
};

/**
 * Allocates and fills configuration fetch request fop.
 *
 * Upon success, confc_fop_alloc() sets m0_confc_ctx::fc_rpc_item
 * field.
 *
 * @param ctx   Configuration retrieval context.
 * @param orig  Origin of the path being sent to confd.
 * @param ri    Starting position (in ctx->fc_path[]) of the path to be
 *              sent to confd.
 *
 * @pre  m0_confc_is_online(ctx->fc_confc)
 */
static int request_create(struct m0_confc_ctx *ctx,
			  const struct m0_conf_obj *orig, size_t ri)
{
	struct confc_fop     *p;
	struct m0_rpc_item   *item;
	struct m0_conf_fetch *req;
	uint32_t              len;

	M0_ENTRY("ctx=%p orig=%p ri=%zu", ctx, orig, ri);
	M0_PRE(ctx_invariant(ctx) && m0_confc_is_online(ctx->fc_confc) &&
	       ctx->fc_rpc_item == NULL);

	p = confc_fop_alloc(ctx);
	if (p == NULL)
		return M0_ERR(-ENOMEM);

	/* Setup rpc item. */
	item = &p->cf_fop.f_item;
	item->ri_ops = &confc_item_ops;
	item->ri_session = m0_confc2sess(ctx->fc_confc);

	/* Setup payload. */
	req = m0_fop_data(&p->cf_fop);
	req->f_origin = orig->co_id;

	for (len = 0; !eop(&ctx->fc_path[ri + len]); ++len)
		; /* measure path length */
	req->f_path.af_count = len;
	req->f_path.af_elems = len == 0 ? NULL : &ctx->fc_path[ri];

	ctx->fc_rpc_item = item;

	M0_POST(ctx_invariant(ctx));
	return M0_RC(0);
}

static bool request_check(const struct m0_confc_ctx *ctx)
{
	const struct m0_conf_fetch *req;
	const struct m0_rpc_item   *item = ctx->fc_rpc_item;

	M0_PRE(item != NULL && m0_confc_is_online(ctx->fc_confc));

	req = m0_fop_data(m0_rpc_item_to_fop(item));

	return  m0_conf_fid_is_valid(&req->f_origin) &&
		m0_fid_is_set(&req->f_origin) &&
		equi(req->f_path.af_count == 0, req->f_path.af_elems == NULL) &&
		item->ri_type == &m0_conf_fetch_fopt.ft_rpc_item_type &&
		item->ri_ops != NULL &&
		item->ri_ops->rio_replied == on_replied &&
		item->ri_session == m0_confc2sess(ctx->fc_confc) &&
		item_to_ctx(item) == ctx;
}

/* ------------------------------------------------------------------
 * Locking
 * ------------------------------------------------------------------ */

static void confc_group_lock(const struct m0_confc *confc)
{
	m0_mutex_lock(&confc->cc_group->s_lock);
}

static void confc_group_unlock(const struct m0_confc *confc)
{
	m0_mutex_unlock(&confc->cc_group->s_lock);
}

static bool confc_group_is_locked(const struct m0_confc *confc)
{
	return m0_mutex_is_locked(&confc->cc_group->s_lock);
}

static void confc_lock(struct m0_confc *confc)
{
	m0_mutex_lock(&confc->cc_lock);
}

static void confc_unlock(struct m0_confc *confc)
{
	m0_mutex_unlock(&confc->cc_lock);
}

static bool confc_is_locked(const struct m0_confc *confc)
{
	return m0_mutex_is_locked(&confc->cc_lock);
}

/** @} confc_dlspec */
#undef M0_TRACE_SUBSYSTEM
