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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 17-Mar-2015
 */


/**
 * @addtogroup addb2
 *
 * SYSTEM interface (implementation)
 * ---------------------------------
 *
 * A sys object (m0_addb2_sys) has 2 functions:
 *
 *     - it is a factory and a cache of addb2 machines and
 *
 *     - it binds addb2 machines that it creates to network and storage
 *       back-ends.
 *
 * Cached addb2 machines are stored in m0_addb2_sys::sy_pool, machines handed to
 * the users are remembered in m0_addb2_sys::sy_granted.
 *
 * If the pool is about to overflow (grow larger than
 * m0_addb2_sys::sy_conf::co_pool_max) when a machine is returned to it (by a
 * call to m0_addb2_sys_put()), the machine is finalised. Because machine
 * finalisation is asynchronous, additional lists are used for this:
 *
 *     - m0_addb2_sys::sy_moribund: m0_addb2_mach_stop() has been on called on a
 *       machine, but the machine is not yet stopped;
 *
 *     - m0_addb2_sys::sy_deathrow: the machine is completely stopped,
 *       m0_addb2_mach_ops::apo_idle (set to sys_idle()) has been called. The
 *       machine will be finalised on the next sys AST (see sys_balance()).
 *
 * All back-end processing is done in the AST context. The AST is posted to the
 * current locality by sys_post().
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/misc.h"                   /* M0_IS0, M0_AMB */
#include "lib/arith.h"                  /* M0_CNT_DEC, M0_CNT_INC */
#include "lib/errno.h"                  /* ENOMEM */
#include "lib/memory.h"                 /* M0_ALLOC_PTR, m0_free */
#include "lib/finject.h"
#include "lib/locality.h"
#include "lib/trace.h"
#include "lib/thread.h"

#include "pool/pool.h"                  /* pools_common_svc_ctx_tl */
#include "module/instance.h"            /* m0_get */

#include "addb2/addb2.h"
#include "addb2/storage.h"
#include "addb2/net.h"
#include "addb2/identifier.h"
#include "addb2/internal.h"
#include "addb2/sys.h"

/**
 * Sys object.
 */
struct m0_addb2_sys {
	/**
	 * Configuration for the machines created by this sys object.
	 */
	struct m0_addb2_config   sy_conf;
	/**
	 * Lock for all fields of this structure, except for ->sy_queue,
	 * ->sy_queued and ->sy_astwait.
	 */
	struct m0_mutex          sy_lock;
	/**
	 * Storage back-end.
	 */
	struct m0_addb2_storage *sy_stor;
	/**
	 * Network back-end.
	 */
	struct m0_addb2_net     *sy_net;
	/**
	 * Addb2 trace queue.
	 *
	 * Addb2 traces are queued on this list (via
	 * m0_addb2_trace_obj::o_linkage, tr_tlist) by m0_addb2_sys_submit() and
	 * de-qeued in the AST context by sys_balance().
	 */
	struct m0_tl             sy_queue;
	/**
	 * Length of ->sy_queue.
	 */
	m0_bcount_t              sy_queued;
	/**
	 * This lock protects for ->sy_queue, ->sy_queued and ->sy_astwait. A
	 * separate lock is needed to make it possible to call
	 * m0_addb2_sys_submit() under other locks. ->sy_qlock nests within
	 * ->sy_lock.
	 */
	struct m0_mutex          sy_qlock;
	struct m0_sm_ast         sy_ast;
	struct m0_sm_ast_wait    sy_astwait;
	/**
	 * Cached machines.
	 */
	struct m0_tl             sy_pool;
	/**
	 * Machines handed off to users.
	 */
	struct m0_tl             sy_granted;
	/**
	 * Machines being stopped.
	 */
	struct m0_tl             sy_moribund;
	/**
	 * Stopped machines, waiting for finalisation.
	 */
	struct m0_tl             sy_deathrow;
	/**
	 * Total number of machines: pooled, granted, moribund and deathrowed.
	 */
	m0_bcount_t              sy_total;
	/**
	 * Semaphore to wait until back-end stops.
	 */
	struct m0_semaphore      sy_wait;
	/**
	 * List of counters registered via m0_addb2_sys_counter_add().
	 */
	struct m0_list           sy_counters;
	/**
	 * Addb2 machine that was associated with the current thread before
	 * sys_lock() call, which see for the explanation.
	 */
	struct m0_addb2_mach    *sy_stash;
};

static void sys_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast);
static void sys_post(struct m0_addb2_sys *sys);
static void sys_idle(struct m0_addb2_mach *mach);
static void sys_lock(struct m0_addb2_sys *sys);
static void sys_unlock(struct m0_addb2_sys *sys);
static void sys_qlock(struct m0_addb2_sys *sys);
static void sys_qunlock(struct m0_addb2_sys *sys);
static void sys_balance(struct m0_addb2_sys *sys);
static bool sys_invariant(const struct m0_addb2_sys *sys);
static bool sys_queue_invariant(const struct m0_addb2_sys *sys);
static int  sys_submit(struct m0_addb2_mach *mach,
		       struct m0_addb2_trace_obj  *obj);
static void net_idle(struct m0_addb2_net *net, void *datum);
static void net_stop(struct m0_addb2_sys *sys);
static void stor_stop(struct m0_addb2_sys *sys);
static m0_bcount_t sys_size(const struct m0_addb2_sys *sys);

static const struct m0_addb2_mach_ops sys_mach_ops;
static const struct m0_addb2_storage_ops sys_stor_ops;

int m0_addb2_sys_init(struct m0_addb2_sys **out,
		      const struct m0_addb2_config *conf)
{
	struct m0_addb2_sys *sys;
	int                  result;

	M0_PRE(conf->co_buffer_min <= conf->co_buffer_max);
	M0_PRE(conf->co_pool_min <= conf->co_pool_max);
	M0_PRE(conf->co_buffer_size % sizeof(uint64_t) == 0);

	M0_ALLOC_PTR(sys);
	if (sys != NULL) {
		sys->sy_conf = *conf;
		sys->sy_ast.sa_cb = &sys_ast;
		m0_mutex_init(&sys->sy_lock);
		m0_mutex_init(&sys->sy_qlock);
		m0_sm_ast_wait_init(&sys->sy_astwait, &sys->sy_qlock);
		/**
		 * `addb2' subsystem is initialised before locales are.
		 * Disable posting of ASTs until m0_addb2_sys_sm_start()
		 * is called.
		 *
		 * @see m0_sm_ast_wait_post(), m0_addb2_sys_sm_start()
		 */
		sys->sy_astwait.aw_allowed = false;
		tr_tlist_init(&sys->sy_queue);
		mach_tlist_init(&sys->sy_pool);
		mach_tlist_init(&sys->sy_granted);
		mach_tlist_init(&sys->sy_moribund);
		mach_tlist_init(&sys->sy_deathrow);
		m0_list_init(&sys->sy_counters);
		*out = sys;
		result = 0;
	} else
		result = M0_ERR(-ENOMEM);
	return result;
}

void m0_addb2_sys_fini(struct m0_addb2_sys *sys)
{
	struct m0_addb2_mach      *m;
	struct m0_addb2_trace_obj *to;

	/*
	 * sys object finalisation is a delicate job, because we have to deal
	 * with various asynchronous activities: concurrent ASTs and back-end
	 * finalisations.
	 */

	m0_addb2_sys_sm_stop(sys);
	m0_tl_for(mach, &sys->sy_pool, m) {
		mach_tlist_move_tail(&sys->sy_moribund, m);
		m0_addb2_mach_stop(m);
	} m0_tl_endfor;
	mach_tlist_fini(&sys->sy_pool);
	/* to keep invariant happy, no concurrency at this point. */
	sys_lock(sys);
	sys_balance(sys);
	sys_unlock(sys);
	if (sys->sy_queued > 0)
		M0_LOG(M0_NOTICE, "Records lost: %"PRIi64"/%zi.",
		       sys->sy_queued, tr_tlist_length(&sys->sy_queue));
	m0_tl_teardown(tr, &sys->sy_queue, to) {
		/*
		 * Update the counter *before* calling m0_addb2_trace_done(),
		 * because it might invoke sys_invariant() via sys_idle().
		 */
		sys->sy_queued -= to->o_tr.tr_nr;
		m0_addb2_trace_done(&to->o_tr);
	}
	m0_tl_for(mach, &sys->sy_moribund, m) {
		m0_addb2_mach_wait(m);
	} m0_tl_endfor;
	mach_tlist_fini(&sys->sy_moribund);
	sys_lock(sys);
	sys_balance(sys);
	mach_tlist_fini(&sys->sy_deathrow);
	m0_tl_for(mach, &sys->sy_granted, m) {
		/* Print still granted machines, there should be none! */
		m0_addb2__mach_print(m);
	} m0_tl_endfor;
	M0_ASSERT_INFO(sys->sy_total == 0, "%"PRIi64, sys->sy_total);
	mach_tlist_fini(&sys->sy_granted);
	net_stop(sys);
	stor_stop(sys);
	sys_qlock(sys);
	m0_sm_ast_wait_fini(&sys->sy_astwait);
	sys_qunlock(sys);
	sys_unlock(sys);
	tr_tlist_fini(&sys->sy_queue);
	m0_mutex_fini(&sys->sy_qlock);
	m0_mutex_fini(&sys->sy_lock);
	/* Do not finalise &sys->sy_counters: can be non-empty. */
	m0_free(sys);
}

struct m0_addb2_mach *m0_addb2_sys_get(struct m0_addb2_sys *sys)
{
	struct m0_addb2_mach *m;

	sys_lock(sys);
	m = mach_tlist_pop(&sys->sy_pool);
	if (m == NULL) {
		if (sys->sy_total < sys->sy_conf.co_pool_max) {
			m = m0_addb2_mach_init(&sys_mach_ops, sys);
			if (m != NULL)
				M0_CNT_INC(sys->sy_total);
			else
				M0_LOG(M0_WARN, "Init: %"PRId64".",
				       sys->sy_total);
		} else
			M0_LOG(M0_WARN, "Limit: %"PRId64".", sys->sy_total);
	}
	if (m != NULL)
		mach_tlist_add(&sys->sy_granted, m);
	sys_unlock(sys);
	return m;
}

void m0_addb2_sys_put(struct m0_addb2_sys *sys, struct m0_addb2_mach *m)
{
	bool kill;

	sys_lock(sys);
	M0_PRE(mach_tlist_contains(&sys->sy_granted, m));
	kill = sys->sy_total > sys->sy_conf.co_pool_min;
	if (kill)
		mach_tlist_move_tail(&sys->sy_moribund, m);
	else
		mach_tlist_move(&sys->sy_pool, m);
	sys_balance(sys);
	sys_unlock(sys);
	if (kill)
		m0_addb2_mach_stop(m);
}

int m0_addb2_sys_net_start(struct m0_addb2_sys *sys)
{
	M0_PRE(sys->sy_net == NULL);

	sys_lock(sys);
	sys->sy_net = m0_addb2_net_init();
	sys_unlock(sys);
	return sys->sy_net != NULL ? 0 : M0_ERR(-ENOMEM);
}

void m0_addb2_sys_net_stop(struct m0_addb2_sys *sys)
{
	sys_lock(sys);
	net_stop(sys);
	sys_unlock(sys);
}

int m0_addb2_sys_net_start_with(struct m0_addb2_sys *sys, struct m0_tl *head)
{
	struct m0_reqh_service_ctx *service;
	struct m0_rpc_conn         *conn;
	int                         result;

	if (sys->sy_net == NULL) {
		result = m0_addb2_sys_net_start(sys);
		if (result != 0)
			return M0_ERR(result);
	}

	m0_tl_for(pools_common_svc_ctx, head, service) {
		if (service->sc_type == M0_CST_ADDB2) {
			conn = &service->sc_rlink.rlk_conn;
			sys_lock(sys);
			result = m0_addb2_net_add(sys->sy_net, conn);
			sys_unlock(sys);
			if (result != 0) {
				m0_addb2_sys_net_stop(sys);
				break;
			}
		}
	} m0_tl_endfor;
	return result;
}

int m0_addb2_sys_stor_start(struct m0_addb2_sys *sys, const char *location,
			    uint64_t key, bool mkfs, bool force,
			    m0_bcount_t size)
{
	M0_PRE(sys->sy_stor == NULL);

	sys_lock(sys);
	sys->sy_stor = m0_addb2_storage_init(location, key, mkfs, force,
					     &sys_stor_ops, size, sys);
	sys_unlock(sys);
	return sys->sy_stor != NULL ? 0 : M0_ERR(-ENOMEM);
}

void m0_addb2_sys_stor_stop(struct m0_addb2_sys *sys)
{
	sys_lock(sys);
	stor_stop(sys);
	sys_unlock(sys);
}

void m0_addb2_sys_sm_start(struct m0_addb2_sys *sys)
{
	sys_qlock(sys);
	sys->sy_astwait.aw_allowed = true;
	sys_qunlock(sys);
}

void m0_addb2_sys_sm_stop(struct m0_addb2_sys *sys)
{
	sys_qlock(sys);
	m0_sm_ast_wait(&sys->sy_astwait);
	M0_ASSERT(sys->sy_ast.sa_next == NULL);
	sys_qunlock(sys);
}

int m0_addb2_sys_submit(struct m0_addb2_sys *sys,
			struct m0_addb2_trace_obj *obj)
{
	sys_qlock(sys);
	if (sys->sy_queued + obj->o_tr.tr_nr <= sys->sy_conf.co_queue_max) {
		sys->sy_queued += obj->o_tr.tr_nr;
		tr_tlink_init_at_tail(obj, &sys->sy_queue);
		sys_post(sys);
	} else {
		M0_LOG(M0_DEBUG, "Queue overflow.");
		obj = NULL;
	}
	sys_qunlock(sys);
	return obj != NULL;
}

void m0_addb2_sys_attach(struct m0_addb2_sys *sys, struct m0_addb2_sys *src)
{
	sys_lock(sys);
	sys->sy_net  = src->sy_net;
	sys->sy_stor = src->sy_stor;
	sys_unlock(sys);
}

void m0_addb2_sys_detach(struct m0_addb2_sys *sys)
{
	sys_lock(sys);
	sys_balance(sys);
	sys->sy_net  = NULL;
	sys->sy_stor = NULL;
	sys_unlock(sys);
}

void m0_addb2_sys_counter_add(struct m0_addb2_sys *sys,
			      struct m0_addb2_counter *counter, uint64_t id)
{
	counter->co_sensor.s_id = id;
	sys_lock(sys);
	m0_list_add_tail(&sys->sy_counters,
			 &counter->co_sensor.s_linkage.t_link);
	sys_unlock(sys);
}

/**
 * Main back-end processing function.
 *
 * This is called in the AST context (but also directly by m0_addb2_sys_fini()).
 *
 * Submits queued traces and finalises stopped machines.
 */
static void sys_balance(struct m0_addb2_sys *sys)
{
	struct m0_addb2_trace_obj *obj;
	struct m0_addb2_mach      *m;

	M0_PRE(sys_invariant(sys));
	if (sys->sy_stor != NULL || sys->sy_net != NULL) {
		sys_qlock(sys);
		while ((obj = tr_tlist_pop(&sys->sy_queue)) != NULL) {
			sys->sy_queued -= obj->o_tr.tr_nr;
			sys_qunlock(sys);
			if (m0_get()->i_disable_addb2_storage ||
			    (sys->sy_stor != NULL ?
			     m0_addb2_storage_submit(sys->sy_stor, obj) :
			     m0_addb2_net_submit(sys->sy_net, obj)) == 0)
				m0_addb2_trace_done(&obj->o_tr);
			sys_qlock(sys);
		}
		sys_qunlock(sys);
	}
	m0_tl_teardown(mach, &sys->sy_deathrow, m) {
		m0_addb2_mach_fini(m);
		M0_CNT_DEC(sys->sy_total);
	}
	M0_POST(sys_invariant(sys));
}

void (*m0_addb2__sys_submit_trap)(struct m0_addb2_sys *sys,
				  struct m0_addb2_trace_obj *obj) = NULL;

static int sys_submit(struct m0_addb2_mach *m, struct m0_addb2_trace_obj *obj)
{
	struct m0_addb2_sys *sys = m0_addb2_mach_cookie(m);

	if (M0_FI_ENABLED("trap") && m0_addb2__sys_submit_trap != NULL)
		m0_addb2__sys_submit_trap(sys, obj);
	return m0_addb2_sys_submit(sys, obj);
}

static void sys_post(struct m0_addb2_sys *sys)
{
	M0_PRE(m0_mutex_is_locked(&sys->sy_qlock));
	/*
	 * m0_sm_ast_wait_post() checks for ->aw_allowed, but we have to check
	 * it before calling m0_locality_here(), because m0_addb2_global_fini()
	 * is called after m0_localities_fini().
	 */
	if (sys->sy_astwait.aw_allowed && sys->sy_ast.sa_next == NULL)
		m0_sm_ast_wait_post(&sys->sy_astwait,
				    m0_locality_here()->lo_grp, &sys->sy_ast);
}

/**
 * Implementation of m0_addb2_mach_ops::apo_idle().
 *
 * Called by IMPLEMENTATION when the machine is stopped. Moves the machine from
 * moribund and deathrow lists and post an AST to finalise the machine.
 *
 * @see sys_mach_ops
 */
static void sys_idle(struct m0_addb2_mach *m)
{
	struct m0_addb2_sys *sys = m0_addb2_mach_cookie(m);

	sys_lock(sys);
	M0_PRE(mach_tlist_contains(&sys->sy_moribund, m));
	mach_tlist_move(&sys->sy_deathrow, m);
	sys_qlock(sys);
	sys_post(sys);
	sys_qunlock(sys);
	sys_unlock(sys);
}

static void net_stop(struct m0_addb2_sys *sys)
{
	M0_PRE(m0_mutex_is_locked(&sys->sy_lock));
	sys_balance(sys);
	if (sys->sy_net != NULL) {
		m0_semaphore_init(&sys->sy_wait, 0);
		m0_addb2_net_stop(sys->sy_net, &net_idle, sys);
		m0_semaphore_down(&sys->sy_wait);
		m0_addb2_net_fini(sys->sy_net);
		m0_semaphore_fini(&sys->sy_wait);
		sys->sy_net = NULL;
	}
}

static void stor_stop(struct m0_addb2_sys *sys)
{
	M0_PRE(m0_mutex_is_locked(&sys->sy_lock));
	sys_balance(sys);
	if (sys->sy_stor != NULL) {
		m0_semaphore_init(&sys->sy_wait, 0);
		m0_addb2_storage_stop(sys->sy_stor);
		sys_unlock(sys);
		m0_semaphore_down(&sys->sy_wait);
		sys_lock(sys);
		m0_addb2_storage_fini(sys->sy_stor);
		m0_semaphore_fini(&sys->sy_wait);
		sys->sy_stor = NULL;
	}
}

void (*m0_addb2__sys_ast_trap)(struct m0_addb2_sys *sys) = NULL;

/**
 * Sys AST call-back.
 */
static void sys_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_addb2_sys *sys = M0_AMB(sys, ast, sy_ast);

	if (M0_FI_ENABLED("trap") && m0_addb2__sys_ast_trap != NULL)
		m0_addb2__sys_ast_trap(sys);
	sys_lock(sys);
	/*
	 * Add counters one by one, trying to distribute them across localities.
	 */
	if (!m0_list_is_empty(&sys->sy_counters)) {
		struct m0_addb2_counter *counter;
		uint64_t                 id;

		counter = M0_AMB(counter, m0_list_first(&sys->sy_counters),
				 co_sensor.s_linkage.t_link);
		id = counter->co_sensor.s_id;
		m0_list_del(&counter->co_sensor.s_linkage.t_link);
		M0_SET0(counter);
		m0_addb2_counter_add(counter, id, -1);
	}
	sys_balance(sys);
	sys_qlock(sys);
	m0_sm_ast_wait_signal(&sys->sy_astwait);
	sys_qunlock(sys);
	sys_unlock(sys);
}

M0_INTERNAL bool m0_addb2_net__is_not_locked(const struct m0_addb2_net *net);
M0_INTERNAL bool
m0_addb2_storage__is_not_locked(const struct m0_addb2_storage *stor);

/**
 * Locks the sys object.
 *
 * @see sys_unlock.
 */
static void sys_lock(struct m0_addb2_sys *sys)
{
	struct m0_addb2_mach *cur = m0_thread_tls()->tls_addb2_mach;

	/*
	 * Assert lock ordering: queue lock, net lock and storage lock nest
	 * within sys lock.
	 */
	M0_PRE(m0_mutex_is_not_locked(&sys->sy_qlock));
	M0_PRE(ergo(sys->sy_net != NULL,
		    m0_addb2_net__is_not_locked(sys->sy_net)));
	M0_PRE(ergo(sys->sy_stor != NULL,
		    m0_addb2_storage__is_not_locked(sys->sy_stor)));
	/*
	 * Clear the addb2 machine, associated with the current
	 * thread. This avoids addb2 re-entrancy and dead-locks. Do this
	 * outside of the lock, just in case m0_mutex_lock() makes addb2
	 * calls.
	 *
	 * The machine is restored in sys_unlock().
	 */
	m0_thread_tls()->tls_addb2_mach = NULL;
	m0_mutex_lock(&sys->sy_lock);
	M0_ASSERT(sys->sy_stash == NULL);
	sys->sy_stash = cur;
	M0_ASSERT(sys_invariant(sys));
}

static void sys_unlock(struct m0_addb2_sys *sys)
{
	struct m0_addb2_mach *cur = sys->sy_stash;

	M0_ASSERT(sys_invariant(sys));

	sys->sy_stash = NULL;
	m0_mutex_unlock(&sys->sy_lock);
	m0_thread_tls()->tls_addb2_mach = cur;
}

/**
 * Locks the queue.
 *
 * @see sys_qunlock.
 */
static void sys_qlock(struct m0_addb2_sys *sys)
{
	m0_mutex_lock(&sys->sy_qlock);
	M0_ASSERT(sys_queue_invariant(sys));
}

/**
 * Unlocks the queue.
 *
 * @see sys_qlock.
 */
static void sys_qunlock(struct m0_addb2_sys *sys)
{
	M0_ASSERT(sys_queue_invariant(sys));
	m0_mutex_unlock(&sys->sy_qlock);
}

static void net_idle(struct m0_addb2_net *net, void *datum)
{
	struct m0_addb2_sys *sys = datum;
	m0_semaphore_up(&sys->sy_wait);
}

static void stor_idle(struct m0_addb2_storage *stor)
{
	struct m0_addb2_sys *sys = m0_addb2_storage_cookie(stor);
	m0_semaphore_up(&sys->sy_wait);
}

static m0_bcount_t sys_size(const struct m0_addb2_sys *sys)
{
	return  mach_tlist_length(&sys->sy_pool) +
		mach_tlist_length(&sys->sy_granted) +
		mach_tlist_length(&sys->sy_moribund) +
		mach_tlist_length(&sys->sy_deathrow);
}

static const struct m0_addb2_mach_ops sys_mach_ops = {
	.apo_submit = &sys_submit,
	.apo_idle   = &sys_idle
};

static const struct m0_addb2_storage_ops sys_stor_ops = {
	.sto_idle = &stor_idle
};

static bool sys_invariant(const struct m0_addb2_sys *sys)
{
	return  _0C(m0_mutex_is_locked(&sys->sy_lock)) &&
		_0C(m0_thread_tls()->tls_addb2_mach == NULL) && /* sys_lock() */
		_0C(M0_CHECK_EX(sys_size(sys) == sys->sy_total)) &&
		_0C(sys->sy_total <= sys->sy_conf.co_pool_max);
}

static bool sys_queue_invariant(const struct m0_addb2_sys *sys)
{
	return  _0C(m0_mutex_is_locked(&sys->sy_qlock)) &&
		_0C(sys->sy_queued <= sys->sy_conf.co_queue_max) &&
		_0C(M0_CHECK_EX(sys->sy_queued == m0_tl_reduce(tr, t,
						       &sys->sy_queue,
						       0, + t->o_tr.tr_nr)));
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
