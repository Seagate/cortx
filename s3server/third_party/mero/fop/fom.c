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
 * Original author: Mandar Sawant <Mandar_Sawant@xyratex.com>
 * Original creation date: 05/04/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FOP
#include "lib/trace.h"

#include "lib/misc.h"
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/locality.h"
#include "lib/processor.h"
#include "lib/time.h"
#include "lib/timer.h"
#include "lib/arith.h"
#include "lib/uuid.h"                 /* m0_node_uuid */
#include "lib/semaphore.h"            /* m0_semaphore */
#include "lib/finject.h"              /* M0_FI_ENABLED */
#include "addb2/net.h"
#include "addb2/addb2.h"
#include "addb2/storage.h"
#include "addb2/identifier.h"
#include "addb2/sys.h"
#include "addb2/global.h"
#include "mero/magic.h"
#include "fop/fop.h"
#include "fop/fom_long_lock.h"
#include "module/instance.h"          /* m0_get */
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "sm/sm.h"
#include "rpc/rpc_machine.h"
#include "rpc/rpc_opcodes.h"
#include "fdmi/fol_fdmi_src.h"

/**
 * @addtogroup fom
 *
 * <b>Locality internals</b>
 *
 * A locality has 4 groups of threads associated with it:
 *
 *     - a handler thread: m0_fom_locality::fl_handler. This thread executes
 *       main handler loop (loc_handler_thread()), where it waits until there
 *       are foms in runqueue and executes their phase transitions. This thread
 *       keeps group lock (m0_fom_locality::fl_group::s_lock) all the
 *       time. As a result, if phase transitions do not block, group lock is
 *       rarely touched;
 *
 *     - blocked threads: threads that executed a m0_fom_block_enter(), but not
 *       yet a matching m0_fom_block_leave() call as part of phase transition;
 *
 *     - unblocking threads: threads that are executing a m0_fom_block_leave()
 *       call and trying to re-acquire the group lock to complete phase
 *       transition.
 *
 *     - idle threads: these are waiting on m0_fom_locality::fl_idle to
 *       become the new handler thread when the previous handler is blocked.
 *
 * Transitions between thread groups are as following:
 *
 *     - the handler thread and a pool of idle threads are created when the
 *       locality is initialised;
 *
 *     - as part of m0_fom_block_leave() call, the blocked thread increments
 *       m0_fom_locality::fl_unblocking counter and acquires the group
 *       lock. When the group lock is acquired, the thread makes itself the
 *       handler thread;
 *
 *     - on each iteration of the main handler thread loop, the handler checks
 *       m0_fom_locality::fl_unblocking. If the counter is greater than 0, the
 *       handler releases the group lock and makes itself idle;
 *
 *     - as part of m0_fom_block_enter() call, current handler thread releases
 *       the group lock and checks m0_fom_locality::fl_unblocking. If the
 *       counter is 0, the handler wakes up one of idle threads (creating one if
 *       none exists), making it the new handler thread.
 *
 * @verbatim
 *
 *      INIT----------------------->IDLE
 *            loc_thr_create()      | ^
 *             ^                    | |
 *             .                    | |
 *             .     signal(fl_idle)| |(unblocking > 0)
 *             .        ^           | |       ^
 *             .        .           | |       .
 *             .        .           | |       .
 *           m0_fom_block_enter()   V |       .
 *        +-----------------------HANDLER     .
 *        |                          ^        .
 *        V                          |        .
 *     BLOCKED                  UNBLOCKING    .
 *        |                          ^        .
 *        |                          |        .
 *        +--------------------------+        .
 *           m0_fom_block_leave()             .
 *                    .                       .
 *                    .........................
 *
 * @endverbatim
 *
 * In the diagram above, dotted arrow means that the state transition causes a
 * change making state transition for *other* thread possible.
 *
 * All threads are linked into m0_fom_locality::fl_threads. This list is used
 * mostly for debugging and invariant checking purposes, but also for thread
 * finalisation (loc_fini()).
 *
 * Thread state transitions, associated lists and counters are protected by
 * the group mutex.
 *
 * @{
 */

enum {
	LOC_IDLE_NR = 1,
	HUNG_FOP_SEC_PERIOD   = 5,
	HUNG_FOP_TIME_SEC_MAX = 2*60,
};

/**
 * Locality thread states. Used for correctness checking.
 */
enum loc_thread_state {
	HANDLER = 1,
	BLOCKED,
	UNBLOCKING,
	IDLE
};

/**
 * Locality thread.
 *
 * Instances of this structure are allocated by loc_thr_create() and freed by
 * loc_thr_fini(). At any time m0_loc_thread::lt_linkage is in
 * m0_fom_locality::fl_threads list and m0_loc_thread::lt_clink is registered
 * with some channel.
 *
 * ->lt_linkage is protected by the locality's group lock. ->lt_state is updated
 * under the group lock, so it can be used by invariants. Other fields are only
 * accessed by the current thread and require no locking.
 */
struct m0_loc_thread {
	enum loc_thread_state   lt_state;
	struct m0_thread        lt_thread;
	struct m0_tlink         lt_linkage;
	struct m0_fom_locality *lt_loc;
	struct m0_clink         lt_clink;
	uint64_t                lt_magix;
};

struct fom_wait_data {
	struct m0_clink  wd_clink;
	uint64_t         wd_phases;
	struct m0_fom   *wd_fom;
	bool             wd_completed;
	int              wd_rc;
};

M0_TL_DESCR_DEFINE(thr, "fom thread", static, struct m0_loc_thread, lt_linkage,
		   lt_magix, M0_FOM_THREAD_MAGIC, M0_FOM_THREAD_HEAD_MAGIC);
M0_TL_DEFINE(thr, static, struct m0_loc_thread);

M0_TL_DESCR_DEFINE(runq, "runq fom", static, struct m0_fom, fo_linkage,
		   fo_magic, M0_FOM_MAGIC, M0_FOM_RUNQ_MAGIC);
M0_TL_DEFINE(runq, static, struct m0_fom);

M0_TL_DESCR_DEFINE(wail, "wail fom", static, struct m0_fom, fo_linkage,
		   fo_magic, M0_FOM_MAGIC, M0_FOM_WAIL_MAGIC);
M0_TL_DEFINE(wail, static, struct m0_fom);

static bool fom_wait_time_is_out(const struct m0_fom_domain *dom,
				 const struct m0_fom *fom);
static int loc_thr_create(struct m0_fom_locality *loc);

static void hung_foms_notify(struct m0_locality_chore *chore,
			     struct m0_locality *loc, void *place);

static struct m0_sm_conf fom_states_conf0;
M0_INTERNAL struct m0_sm_conf fom_states_conf;

/**
 * Fom domain operations.
 * @todo Support fom timeout functionality.
 */
static struct m0_fom_domain_ops m0_fom_dom_ops = {
	.fdo_time_is_out = fom_wait_time_is_out
};

/**
 * Chore which detects long-living foms.
 */
static const struct m0_locality_chore_ops hung_foms_chore_ops = {
	.co_tick = hung_foms_notify
};

static void group_lock(struct m0_fom_locality *loc)
{
	m0_sm_group_lock(&loc->fl_group);
}

static void group_unlock(struct m0_fom_locality *loc)
{
	m0_sm_group_unlock(&loc->fl_group);
}

M0_INTERNAL bool m0_fom_group_is_locked(const struct m0_fom *fom)
{
	return m0_mutex_is_locked(&fom->fo_loc->fl_group.s_lock);
}

static bool is_in_runq(const struct m0_fom *fom)
{
	return runq_tlist_contains(&fom->fo_loc->fl_runq, fom);
}

static bool is_in_wail(const struct m0_fom *fom)
{
	return wail_tlist_contains(&fom->fo_loc->fl_wail, fom);
}

static bool thread_invariant(const struct m0_loc_thread *t)
{
	struct m0_fom_locality *loc = t->lt_loc;

	return
		_0C(M0_IN(t->lt_state, (HANDLER, BLOCKED, UNBLOCKING, IDLE))) &&
		_0C((loc->fl_handler == t) == (t->lt_state == HANDLER)) &&
		_0C(ergo(t->lt_state == UNBLOCKING,
		     m0_atomic64_get(&loc->fl_unblocking) > 0));
}

M0_INTERNAL bool m0_fom_domain_invariant(const struct m0_fom_domain *dom)
{
	size_t cpu_max = m0_processor_nr_max();
	return
		_0C(dom != NULL && dom->fd_localities != NULL) &&
		_0C(dom->fd_localities_nr <= cpu_max) &&
		_0C(m0_forall(i, dom->fd_localities_nr,
			     dom->fd_localities[i] != NULL)) &&
		_0C(dom->fd_ops != NULL);
}

M0_INTERNAL bool m0_locality_invariant(const struct m0_fom_locality *loc)
{
	return
		_0C(loc != NULL && loc->fl_dom != NULL) &&
		_0C(m0_mutex_is_locked(&loc->fl_group.s_lock)) &&
		_0C(M0_CHECK_EX(m0_tlist_invariant(&runq_tl, &loc->fl_runq))) &&
		_0C(M0_CHECK_EX(m0_tlist_invariant(&wail_tl, &loc->fl_wail))) &&
		_0C(m0_tl_forall(thr, t, &loc->fl_threads,
			     t->lt_loc == loc && thread_invariant(t))) &&
		_0C(ergo(loc->fl_handler != NULL,
		     thr_tlist_contains(&loc->fl_threads, loc->fl_handler))) &&
		_0C(M0_CHECK_EX(m0_tl_forall(runq, fom, &loc->fl_runq,
					 fom->fo_loc == loc))) &&
		_0C(M0_CHECK_EX(m0_tl_forall(wail, fom, &loc->fl_wail,
					 fom->fo_loc == loc)));
}

M0_INTERNAL struct m0_reqh *m0_fom_reqh(const struct m0_fom *fom)
{
	return fom->fo_service->rs_reqh;
}

static inline enum m0_fom_state fom_state(const struct m0_fom *fom)
{
	return fom->fo_sm_state.sm_state;
}

static inline void fom_state_set(struct m0_fom *fom, enum m0_fom_state state)
{
	m0_sm_state_set(&fom->fo_sm_state, state);
}

static bool fom_is_blocked(const struct m0_fom *fom)
{
	return
		fom_state(fom) == M0_FOS_RUNNING &&
		M0_IN(fom->fo_thread->lt_state, (BLOCKED, UNBLOCKING));
}

/* Returns fom from state machine m0_fom::fo_sm_state */
static inline struct m0_fom *sm2fom(struct m0_sm *sm)
{
	return container_of(sm, struct m0_fom, fo_sm_state);
}

M0_INTERNAL bool m0_fom_invariant(const struct m0_fom *fom)
{
	return
		_0C(fom != NULL) && _0C(fom->fo_loc != NULL) &&
		_0C(fom->fo_type != NULL) && _0C(fom->fo_ops != NULL) &&

		_0C(m0_fom_group_is_locked(fom)) &&

		/* fom magic is the same in runq and wail tlists,
		 * so we can use either one here.
		 * @todo replace this with bob_check() */
		M0_CHECK_EX(m0_tlink_invariant(&runq_tl, fom)) &&

		_0C(M0_IN(fom_state(fom), (M0_FOS_READY, M0_FOS_WAITING,
					   M0_FOS_RUNNING, M0_FOS_INIT))) &&
		_0C((fom_state(fom) == M0_FOS_READY) == is_in_runq(fom)) &&
		M0_CHECK_EX(_0C((fom_state(fom) == M0_FOS_WAITING) ==
				is_in_wail(fom))) &&
		_0C(ergo(fom->fo_thread != NULL,
			 fom_state(fom) == M0_FOS_RUNNING)) &&
		_0C(ergo(fom->fo_pending != NULL,
		    (fom_state(fom) == M0_FOS_READY || fom_is_blocked(fom)))) &&
		_0C(ergo(fom->fo_cb.fc_state != M0_FCS_DONE,
			 fom_state(fom) == M0_FOS_WAITING)) &&
		_0C(fom->fo_service->rs_type == fom->fo_type->ft_rstype);
}

/*
 * TODO: replace with corresponding HA handler when it's integrated
 */
static bool hung_fom_notify(const struct m0_fom *fom)
{
	m0_time_t diff;

	if (M0_IN(fom->fo_type->ft_id, (M0_BE_TX_GROUP_OPCODE,
					M0_ADDB_FOP_OPCODE,
					M0_HA_LINK_OUTGOING_OPCODE,
					M0_FDMI_SOURCE_DOCK_OPCODE)))
	    return true;

	diff = m0_time_sub(m0_time_now(), fom->fo_sm_state.sm_state_epoch);
	if (m0_time_seconds(diff) > HUNG_FOP_TIME_SEC_MAX) {
		M0_LOG(M0_WARN, "FOP HUNG[" TIME_F " seconds in processing]: "
		       "fom=%p, fop %p[%u] phase: %s", TIME_P(diff), fom,
		       &fom->fo_fop,
		       fom->fo_fop == NULL ? 0 : m0_fop_opcode(fom->fo_fop),
		       m0_fom_phase_name(fom, m0_fom_phase(fom)));
		if (fom->fo_ops->fo_hung_notify != NULL)
			fom->fo_ops->fo_hung_notify(fom);
	}

	return true; /* by convention of m0_tl_forall */
}

static bool fom_wait_time_is_out(const struct m0_fom_domain *dom,
				 const struct m0_fom *fom)
{
	return hung_fom_notify(fom);
}

/**
 * Enqueues fom into locality runq list and increments
 * number of items in runq, m0_fom_locality::fl_runq_nr.
 * This function is invoked when a new fom is submitted for
 * execution or a waiting fom is re-scheduled for processing.
 *
 * @post m0_fom_invariant(fom)
 */
static void fom_ready(struct m0_fom *fom)
{
	struct m0_fom_locality *loc;
	bool                    empty;

	fom_state_set(fom, M0_FOS_READY);
	loc = fom->fo_loc;
	empty = runq_tlist_is_empty(&loc->fl_runq);
	runq_tlist_add_tail(&loc->fl_runq, fom);
	M0_CNT_INC(loc->fl_runq_nr);
	m0_addb2_hist_mod(&loc->fl_runq_counter, loc->fl_runq_nr);
	if (empty)
		m0_chan_signal(&loc->fl_runrun);
	M0_POST(m0_fom_invariant(fom));
}

M0_INTERNAL void m0_fom_ready(struct m0_fom *fom)
{
	struct m0_fom_locality *loc = fom->fo_loc;

	M0_PRE(m0_fom_invariant(fom));

	wail_tlist_del(fom);
	M0_CNT_DEC(loc->fl_wail_nr);
	m0_addb2_hist_mod(&loc->fl_wail_counter, loc->fl_wail_nr);
	fom_ready(fom);
}

static void readyit(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_fom *fom = container_of(ast, struct m0_fom, fo_cb.fc_ast);

	m0_fom_ready(fom);
}

static void fom_addb2_push(struct m0_fom *fom)
{
	M0_ADDB2_PUSH(M0_AVI_FOM, (uint64_t)fom, fom->fo_type->ft_id,
		      fom->fo_transitions, fom->fo_sm_phase.sm_state);
}

static void addb2_introduce(struct m0_fom *fom)
{
	struct m0_rpc_item             *req;
	static struct m0_sm_addb2_stats phase_stats = {
		.as_id = M0_AVI_PHASE,
		.as_nr = 0
	};
	uint64_t sender_id = 0;
	uint64_t item_sm_id = 0;
	uint64_t phase_sm_id = 0;
	uint64_t state_sm_id = 0;

	if (!m0_sm_addb2_counter_init(&fom->fo_sm_phase))
		fom->fo_sm_phase.sm_addb2_stats = &phase_stats;
	if (!m0_sm_addb2_counter_init(&fom->fo_sm_state))
		fom->fo_sm_state.sm_addb2_stats =
			m0_locality_data(fom_states_conf.scf_addb2_key - 1);

	req = fom->fo_fop != NULL ? &fom->fo_fop->f_item : NULL;

	fom_addb2_push(fom);

	if (req != NULL && req->ri_session != NULL) {
		sender_id = req->ri_session->s_conn->c_sender_id;
		item_sm_id = m0_sm_id_get(&req->ri_sm);
		phase_sm_id = m0_sm_id_get(&fom->fo_sm_phase);
		state_sm_id = m0_sm_id_get(&fom->fo_sm_state);
	}

	M0_ADDB2_ADD(M0_AVI_FOM_DESCR,
		     FID_P(&fom->fo_service->rs_service_fid),
		     /*
		      * Session can be NULL for connection and session
		      * establishing fops.
		      */
		     sender_id,
		     req != NULL ? req->ri_type->rit_opcode : 0,
		     fom->fo_rep_fop != NULL ?
			     fom->fo_rep_fop->f_item.ri_type->rit_opcode : 0,
		     fom->fo_local,
		     item_sm_id,
		     phase_sm_id,
		     state_sm_id);
	if (fom->fo_ops->fo_addb2_descr != NULL)
		fom->fo_ops->fo_addb2_descr(fom);
	m0_addb2_pop(M0_AVI_FOM);
}

static void queueit(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_fom *fom = container_of(ast, struct m0_fom, fo_cb.fc_ast);

	M0_PRE(m0_fom_invariant(fom));
	M0_PRE(m0_fom_phase(fom) == M0_FOM_PHASE_INIT);

	addb2_introduce(fom);
	m0_fom_locality_inc(fom);
	fom_ready(fom);
}

static void thr_addb2_enter(struct m0_loc_thread *thr,
			    struct m0_fom_locality *loc)
{
	m0_addb2_global_thread_leave();
	M0_ASSERT(m0_thread_tls()->tls_addb2_mach == NULL);
	m0_thread_tls()->tls_addb2_mach = loc->fl_addb2_mach;
	m0_addb2_push(M0_AVI_THREAD, M0_ADDB2_OBJ(&thr->lt_thread.t_h));
}

static void thr_addb2_leave(struct m0_loc_thread *thr,
			    struct m0_fom_locality *loc)
{
	M0_PRE(m0_thread_tls()->tls_addb2_mach == loc->fl_addb2_mach);
	m0_addb2_pop(M0_AVI_THREAD);
	m0_thread_tls()->tls_addb2_mach = NULL;
	m0_addb2_global_thread_enter();
}

M0_INTERNAL void m0_fom_wakeup(struct m0_fom *fom)
{
	fom->fo_cb.fc_ast.sa_cb = readyit;
	m0_sm_ast_post(&fom->fo_loc->fl_group, &fom->fo_cb.fc_ast);
}

M0_INTERNAL void m0_fom_block_enter(struct m0_fom *fom)
{
	struct m0_fom_locality *loc;
	struct m0_loc_thread   *thr;

	M0_PRE(m0_fom_invariant(fom));
	M0_PRE(fom_state(fom) == M0_FOS_RUNNING);
	M0_PRE(!fom_is_blocked(fom));

	loc = fom->fo_loc;
	thr = fom->fo_thread;

	M0_PRE(thr->lt_state == HANDLER);
	M0_PRE(thr == loc->fl_handler);

	/*
	 * If there are unblocking threads, trying to complete
	 * m0_fom_block_leave() call, do nothing and release the group lock. One
	 * of these threads would grab it and become the handler.
	 *
	 * Otherwise, wake up one idle thread, creating it if necessary.
	 *
	 * Note that loc->fl_unblocking can change under us, but:
	 *
	 *     - it cannot become 0 if it wasn't, because it is decremented
	 *       under the group lock and,
	 *
	 *     - if it were increased after we check it, nothing bad would
	 *       happen: an extra idle thread wakeup is harmless.
	 */
	if (m0_atomic64_get(&loc->fl_unblocking) == 0) {
		if (!m0_chan_has_waiters(&loc->fl_idle))
			loc_thr_create(loc);
		m0_chan_signal(&loc->fl_idle);
	}

	thr->lt_state = BLOCKED;
	loc->fl_handler = NULL;
	M0_ASSERT(m0_locality_invariant(loc));
	m0_addb2_pop(M0_AVI_FOM);
	thr_addb2_leave(thr, loc);
	group_unlock(loc);
}

M0_INTERNAL void m0_fom_block_leave(struct m0_fom *fom)
{
	struct m0_fom_locality *loc;
	struct m0_loc_thread   *thr;

	loc = fom->fo_loc;
	thr = fom->fo_thread;

	M0_PRE(thr->lt_state == BLOCKED);
	/*
	 * Signal the handler that there is a thread that wants to unblock, just
	 * in case the handler is sleeping on empty runqueue.
	 *
	 * It is enough to do this only when loc->fl_unblocking increments from
	 * 0 to 1, because the handler won't go to sleep until
	 * loc->fl_unblocking drops to 0.
	 */
	if (m0_atomic64_add_return(&loc->fl_unblocking, 1) == 1)
		m0_clink_signal(&loc->fl_group.s_clink);
	/*
	 * lt_state must be changed after fl_unblocking increment
	 * to avoid panics at thread_invariant(). Also, we rely
	 * on the following fact from the Linux kernel here:
	 *
	 *  - RMW operations that have a return value are fully ordered;
	 *
	 * (See https://www.kernel.org/doc/Documentation/atomic_t.txt)
	 */
	thr->lt_state = UNBLOCKING;

	group_lock(loc);
	thr_addb2_enter(thr, loc);
	fom_addb2_push(fom);
	M0_ASSERT(m0_locality_invariant(loc));
	M0_ASSERT(fom_is_blocked(fom));
	M0_ASSERT(loc->fl_handler == NULL);
	loc->fl_handler = thr;
	thr->lt_state = HANDLER;
	m0_atomic64_dec(&loc->fl_unblocking);
	M0_ASSERT(m0_locality_invariant(loc));
}

M0_INTERNAL void m0_fom_queue(struct m0_fom *fom)
{
	struct m0_fom_domain *dom;
	size_t                loc_idx;

	M0_PRE(fom != NULL);

	dom = m0_fom_dom();
	loc_idx = fom->fo_ops->fo_home_locality(fom) % dom->fd_localities_nr;
	M0_ASSERT(loc_idx < dom->fd_localities_nr);
	fom->fo_loc = dom->fd_localities[loc_idx];
	m0_fom_sm_init(fom);
	fom->fo_cb.fc_ast.sa_cb = &queueit;
	m0_sm_ast_post(&fom->fo_loc->fl_group, &fom->fo_cb.fc_ast);
}

/**
 * Puts fom on locality wait list if fom performs a blocking operation, this
 * releases the handler thread to start executing another fom from the runq,
 * thus making the reqh non blocking.
 *
 * Fom state is changed to M0_FOS_WAITING.  m0_fom_locality::fl_group.s_lock
 * should be held before putting fom on the locality wait list.
 *
 * This function is invoked from fom_exec(), if the fom is performing a blocking
 * operation and m0_fom_ops::fo_tick() returns M0_FSO_WAIT.
 *
 * @post m0_fom_invariant(fom)
 */
static void fom_wait(struct m0_fom *fom)
{
	struct m0_fom_locality *loc;

	fom_state_set(fom, M0_FOS_WAITING);
	loc = fom->fo_loc;
	wail_tlist_add_tail(&loc->fl_wail, fom);
	M0_CNT_INC(loc->fl_wail_nr);
	m0_addb2_hist_mod(&loc->fl_wail_counter, loc->fl_wail_nr);
	M0_POST(m0_fom_invariant(fom));
}

static bool fom_wait_is_completed(const struct fom_wait_data *wd)
{
	return (M0_BITS(m0_fom_phase(wd->wd_fom)) &
		(wd->wd_phases | M0_BITS(M0_FOM_PHASE_FINISH)));
}

static int fom_wait_rc(const struct fom_wait_data *wd)
{
	M0_ASSERT(fom_wait_is_completed(wd));
	return m0_fom_phase(wd->wd_fom) == M0_FOM_PHASE_FINISH ?
			M0_ERR(-ESRCH) : 0;
}

static bool fom_wait_cb(struct m0_clink *clink)
{
	struct fom_wait_data *wd = M0_AMB(wd, clink, wd_clink);

	if (fom_wait_is_completed(wd)) {
		wd->wd_rc = fom_wait_rc(wd);
		/*
		 * Detach clink from FOM phase SM channel to not get
		 * assertion on FOM finalisation.
		 */
		m0_clink_del(&wd->wd_clink);
		/* That will signal on semaphore in m0_fom_timedwait(). */
		return false;
	}
	return true;
}

static int fom_wait_init(void *data)
{
	struct fom_wait_data *wd = data;

	/*
	 * FOM may be already in finish state, it worth checking it here to not
	 * wait deadline.
	 */
	wd->wd_completed = fom_wait_is_completed(wd);
	if (!wd->wd_completed)
		m0_clink_add(&wd->wd_fom->fo_sm_phase.sm_chan, &wd->wd_clink);
	return wd->wd_completed ? fom_wait_rc(wd) : 0;
}

static int fom_wait_fini(void *data)
{
	struct fom_wait_data *wd = data;

	if (m0_clink_is_armed(&wd->wd_clink))
		m0_clink_del(&wd->wd_clink);
	return 0;
}

M0_INTERNAL int m0_fom_timedwait(struct m0_fom *fom, uint64_t phases,
				 m0_time_t deadline)
{
	struct fom_wait_data  wd;
	struct m0_locality   *loc = &fom->fo_loc->fl_locality;
	int                   result;

	wd.wd_phases = phases;
	wd.wd_fom = fom;
	m0_clink_init(&wd.wd_clink, &fom_wait_cb);
	result = m0_locality_call(loc, &fom_wait_init, &wd);
	if (!wd.wd_completed) {
		result = m0_chan_timedwait(&wd.wd_clink, deadline) ?
			 wd.wd_rc : M0_ERR(-ETIMEDOUT);
		m0_locality_call(loc, &fom_wait_fini, &wd);
	}
	m0_clink_fini(&wd.wd_clink);
	return result;
}

/**
 * Helper function advancing a fom call-back from ARMED to DONE state.
 */
static void cb_done(struct m0_fom_callback *cb)
{
	struct m0_clink *clink = &cb->fc_clink;

	M0_PRE(cb->fc_state == M0_FCS_ARMED);

	M0_ASSERT(!m0_clink_is_armed(clink));
	cb->fc_state = M0_FCS_DONE;

	M0_POST(m0_fom_invariant(cb->fc_fom));
}

/**
 * Helper to execute the bottom half of a fom call-back.
 */
static void cb_run(struct m0_fom_callback *cb)
{
	M0_PRE(m0_fom_invariant(cb->fc_fom));

	cb_done(cb);
	cb->fc_bottom(cb);
}

static void *cb_next(struct m0_fom_callback *cb)
{
	return cb->fc_ast.sa_next;
}

/**
 * Invokes fom phase transition method, which transitions fom
 * through various phases of its execution without blocking.
 * @post m0_fom_invariant(fom)
 */
static void fom_exec(struct m0_fom *fom)
{
	int			rc;
	struct m0_fom_locality *loc;

	loc = fom->fo_loc;
	fom->fo_thread = loc->fl_handler;
	fom_state_set(fom, M0_FOS_RUNNING);
	do {
		M0_ASSERT(m0_fom_invariant(fom));
		M0_ASSERT(m0_fom_phase(fom) != M0_FOM_PHASE_FINISH);
		rc = fom->fo_ops->fo_tick(fom);
		if (FOM_PHASE_DEBUG) {
			fom->fo_log[fom->fo_transitions %
				    ARRAY_SIZE(fom->fo_log)] =
				m0_fom_phase(fom);
		}
		/*
		 * (rc == M0_FSO_AGAIN) means that next phase transition is
		 * possible. Current policy is to execute the transition
		 * immediately. Alternative is to put the fom on the runqueue
		 * and select "the best" fom from the runqueue.
		 */
		fom->fo_transitions++;
	} while (rc == M0_FSO_AGAIN);

	fom->fo_thread = NULL;

	M0_ASSERT(rc == M0_FSO_WAIT);
	M0_ASSERT(m0_fom_group_is_locked(fom));

	if (m0_fom_phase(fom) == M0_FOM_PHASE_FINISH) {
		/*
		 * Finish fom itself.
		 */
		fom->fo_ops->fo_fini(fom);
		/*
		 * Don't touch the fom after this point.
		 */
	} else {
		struct m0_fom_callback *cb;

		fom_wait(fom);
		/*
		 * If there are pending call-backs, execute them, until one of
		 * them wakes the fom up. Don't bother to optimize moving
		 * between queues: this is a rare case.
		 *
		 * Note: call-backs are executed in LIFO order.
		 */
		M0_ADDB2_PUSH(M0_AVI_FOM_CB);
		while ((cb = fom->fo_pending) != NULL) {
			fom->fo_pending = cb_next(cb);
			cb_run(cb);
			/*
			 * call-back is not allowed to destroy a fom.
			 */
			M0_ASSERT(m0_fom_phase(fom) != M0_FOM_PHASE_FINISH);
			if (fom_state(fom) != M0_FOS_WAITING)
				break;
		}
		m0_addb2_pop(M0_AVI_FOM_CB);
		M0_ASSERT(m0_fom_invariant(fom));
	}
}

/**
 * Dequeues a fom from runq list of the locality.
 *
 * @retval m0_fom if queue is not empty, NULL otherwise
 */
static struct m0_fom *fom_dequeue(struct m0_fom_locality *loc)
{
	struct m0_fom *fom;

	fom = runq_tlist_pop(&loc->fl_runq);
	if (fom != NULL) {
		M0_ASSERT(fom->fo_loc == loc);
		M0_CNT_DEC(loc->fl_runq_nr);
		m0_addb2_hist_mod(&loc->fl_runq_counter, loc->fl_runq_nr);
	}
	return fom;
}

/**
 * Locality handler thread. See the "Locality internals" section.
 */
static void loc_handler_thread(struct m0_loc_thread *th)
{
	struct m0_clink        *clink  = &th->lt_clink;
	struct m0_fom_locality *loc    = th->lt_loc;

	while (1) {
		/*
		 * start idle, wait for work to do. The clink was registered
		 * with &loc->fl_idle by loc_thr_create().
		 */
		M0_ASSERT(th->lt_state == IDLE);
		m0_chan_wait(clink);

		/* become the handler thread */
		group_lock(loc);
		M0_ASSERT(loc->fl_handler == NULL);
		loc->fl_handler = th;
		th->lt_state = HANDLER;
		thr_addb2_enter(th, loc);

		/*
		 * re-initialise the clink and arrange for it to receive group
		 * AST notifications and runrun wakeups.
		 */
		m0_clink_del(clink);
		m0_clink_fini(clink);
		m0_clink_init(clink, NULL);
		m0_clink_attach(clink, &loc->fl_group.s_clink, NULL);
		m0_clink_add(&loc->fl_runrun, clink);

		/*
		 * main handler loop.
		 *
		 * This loop terminates when the locality is finalised
		 * (loc->fl_shutdown) or this thread should go back to the idle
		 * state.
		 */
		while (1) {
			struct m0_fom *fom;
			M0_ASSERT(m0_locality_invariant(loc));

			m0_addb2_force(M0_MKTIME(5, 0));
			/*
			 * All foms that have been queued up in runqueue will be
			 * executed, clearing their accumulation in semaphore.
			 */
			m0_semaphore_drain(&clink->cl_group->cl_wait);
			/*
			 * Check for a blocked thread that tries to unblock and
			 * complete a phase transition.
			 */
			if (m0_atomic64_get(&loc->fl_unblocking) > 0)
				/*
				 * Idle ourselves. The unblocking thread (first
				 * to grab the group lock in case there are
				 * many), becomes the new handler.
				 */
				break;
			M0_ADDB2_IN(M0_AVI_AST, m0_sm_asts_run(&loc->fl_group));
			M0_ADDB2_IN(M0_AVI_CHORE,
				    m0_locality_chores_run(&loc->fl_locality));
			fom = fom_dequeue(loc);
			if (fom != NULL) {
				fom_addb2_push(fom);
				fom_exec(fom);
				m0_addb2_pop(M0_AVI_FOM);
			} else if (loc->fl_shutdown)
				break;
			else
				/*
				 * Yes, sleep with the lock held. Knock on
				 * &loc->fl_runrun or &loc->fl_group.s_clink to
				 * wake.
				 */
				m0_chan_wait(clink);
		}
		loc->fl_handler = NULL;
		th->lt_state = IDLE;
		m0_clink_del(clink);
		m0_clink_fini(clink);
		m0_clink_init(&th->lt_clink, NULL);
		m0_clink_add(&loc->fl_idle, &th->lt_clink);
		thr_addb2_leave(th, loc);
		group_unlock(loc);
		if (loc->fl_shutdown)
			break;
	}
}

/**
 * Init function for a locality thread. Confines the thread to the locality
 * core.
 */
static int loc_thr_init(struct m0_loc_thread *th)
{
	return m0_thread_confine(&th->lt_thread, &th->lt_loc->fl_processors);
}

static void loc_thr_fini(struct m0_loc_thread *th)
{
	M0_PRE(m0_mutex_is_locked(&th->lt_loc->fl_group.s_lock));
	M0_PRE(th->lt_state == IDLE);
	m0_clink_del(&th->lt_clink);
	m0_clink_fini(&th->lt_clink);
	m0_thread_fini(&th->lt_thread);
	thr_tlink_del_fini(th);
	m0_free(th);
}

static int loc_thr_create(struct m0_fom_locality *loc)
{
	struct m0_loc_thread *thr;
	int                   res;

	M0_PRE(m0_mutex_is_locked(&loc->fl_group.s_lock));

	M0_ENTRY("%p", loc);

	M0_ALLOC_PTR(thr);
	if (thr == NULL)
		return M0_ERR(-ENOMEM);
	thr->lt_state      = IDLE;
	thr->lt_magix      = M0_FOM_THREAD_MAGIC;
	thr->lt_loc        = loc;
	thr_tlink_init_at_tail(thr, &loc->fl_threads);

	m0_clink_init(&thr->lt_clink, NULL);
	m0_clink_add(&loc->fl_idle, &thr->lt_clink);

	res = M0_THREAD_INIT(&thr->lt_thread, struct m0_loc_thread *,
			     loc_thr_init, &loc_handler_thread, thr,
			     "m0_loc_thread");
	if (res != 0)
		loc_thr_fini(thr);
	return M0_RC(res);
}

static void loc_addb2_fini(struct m0_fom_locality *loc)
{
	struct m0_addb2_mach *orig = m0_thread_tls()->tls_addb2_mach;

	m0_thread_tls()->tls_addb2_mach = loc->fl_addb2_mach;
	m0_addb2_pop(M0_AVI_LOCALITY);
	m0_addb2_pop(M0_AVI_PID);
	m0_addb2_pop(M0_AVI_NODE);
	m0_thread_tls()->tls_addb2_mach = orig;
	m0_addb2_sys_put(loc->fl_dom->fd_addb2_sys, loc->fl_addb2_mach);
}

/**
 * Finalises a given locality.
 */
static void loc_fini(struct m0_fom_locality *loc)
{
	struct m0_loc_thread *th;

	loc->fl_shutdown = true;
	m0_clink_signal(&loc->fl_group.s_clink);

	group_lock(loc);
	m0_chan_broadcast(&loc->fl_runrun);
	m0_chan_broadcast(&loc->fl_idle);
	while ((th = thr_tlist_head(&loc->fl_threads)) != NULL) {
		group_unlock(loc);
		m0_thread_join(&th->lt_thread);
		group_lock(loc);
		loc_thr_fini(th);
	}
	group_unlock(loc);

	runq_tlist_fini(&loc->fl_runq);
	M0_ASSERT(loc->fl_runq_nr == 0);
	wail_tlist_fini(&loc->fl_wail);
	M0_ASSERT(loc->fl_wail_nr == 0);
	thr_tlist_fini(&loc->fl_threads);
	M0_ASSERT(m0_atomic64_get(&loc->fl_unblocking) == 0);
	m0_chan_fini_lock(&loc->fl_idle);
	m0_chan_fini_lock(&loc->fl_runrun);
	m0_sm_group_fini(&loc->fl_group);
	m0_bitmap_fini(&loc->fl_processors);
	loc_addb2_fini(loc);
	m0_locality_fini(&loc->fl_locality);
}

/**
 * Initialises a locality in fom domain.  Creates and adds threads to locality,
 * every thread is confined to the cpus represented by the
 * m0_fom_locality::fl_processors, this is done in the locality thread init
 * function (loc_thr_init()).
 *
 * A pool of LOC_IDLE_NR idle threads is created together with a handler thread.
 *
 * @see loc_thr_create()
 * @see loc_thr_init()
 *
 * @param loc     m0_fom_locality to be initialised
 * @param idx     index of locality within fom domain
 */
static int loc_init(struct m0_fom_locality *loc, struct m0_fom_domain *dom,
		    size_t idx)
{
	int                   res;
	struct m0_addb2_mach *orig = m0_thread_tls()->tls_addb2_mach;

	M0_PRE(loc != NULL);

	M0_ENTRY();

	loc->fl_dom = dom;
	loc->fl_addb2_mach = m0_addb2_sys_get(dom->fd_addb2_sys);
	if (loc->fl_addb2_mach == NULL) {
		res = M0_ERR(-ENOMEM);
		goto err;
	}

	runq_tlist_init(&loc->fl_runq);
	loc->fl_runq_nr = 0;
	wail_tlist_init(&loc->fl_wail);
	loc->fl_wail_nr = 0;
	loc->fl_idx = idx;
	m0_thread_tls()->tls_addb2_mach = loc->fl_addb2_mach;
	m0_addb2_push(M0_AVI_NODE, M0_ADDB2_OBJ(&m0_node_uuid));
	M0_ADDB2_PUSH(M0_AVI_PID, m0_pid());
	M0_ADDB2_PUSH(M0_AVI_LOCALITY, loc->fl_idx);
	m0_addb2_clock_add(&loc->fl_clock, M0_AVI_CLOCK, -1);
	m0_addb2_hist_add(&loc->fl_fom_active,   1, 30, M0_AVI_FOM_ACTIVE, -1);
	m0_addb2_hist_add(&loc->fl_runq_counter, 1, 30, M0_AVI_RUNQ, -1);
	m0_addb2_hist_add(&loc->fl_wail_counter, 1, 30, M0_AVI_WAIL, -1);
	m0_addb2_hist_add_auto(&loc->fl_grp_addb2.ga_forq_hist, 1000,
			       M0_AVI_LOCALITY_FORQ, -1);
	m0_addb2_hist_add_auto(&loc->fl_chan_addb2.ca_wait_hist, 1000,
			       M0_AVI_LOCALITY_CHAN_WAIT, -1);
	m0_addb2_hist_add_auto(&loc->fl_chan_addb2.ca_cb_hist, 1000,
			       M0_AVI_LOCALITY_CHAN_CB, -1);
	m0_addb2_hist_add_auto(&loc->fl_chan_addb2.ca_queue_hist, 1000,
			       M0_AVI_LOCALITY_CHAN_QUEUE, -1);
	loc->fl_grp_addb2.ga_forq = M0_AVI_LOCALITY_FORQ_DURATION;
	m0_thread_tls()->tls_addb2_mach = orig;

	m0_locality_init(&loc->fl_locality,
			 &loc->fl_group, loc->fl_dom, loc->fl_idx);
	m0_sm_group_init(&loc->fl_group);
	loc->fl_group.s_addb2 = &loc->fl_grp_addb2;
	m0_chan_init(&loc->fl_runrun, &loc->fl_group.s_lock);
	loc->fl_runrun.ch_addb2 = &loc->fl_chan_addb2;
	thr_tlist_init(&loc->fl_threads);
	m0_atomic64_set(&loc->fl_unblocking, 0);
	m0_chan_init(&loc->fl_idle, &loc->fl_group.s_lock);

	res = m0_bitmap_init(&loc->fl_processors, dom->fd_localities_nr);
	if (res == 0) {
		int i;

		m0_bitmap_set(&loc->fl_processors, idx, true);
		/* create a pool of idle threads plus the handler thread. */
		group_lock(loc);
		for (i = 0; i < LOC_IDLE_NR + 1; ++i) {
			res = loc_thr_create(loc);
			if (res != 0)
				break;
		}
		group_unlock(loc);
		/*
		 * All threads created above are blocked at
		 * loc_handler_thread()::m0_chan_wait(clink). One thread
		 * per-locality is woken at the end of m0_fom_domain_init().
		 */
	}
	if (res != 0)
		loc_fini(loc);
	return M0_RC(res);
err:
	return M0_ERR(res);
}

/*
 * Compose HW core mask with preset mask from instance
 */
static void core_mask_apply(struct m0_bitmap *onln_cpu_map)
{
	struct m0_bitmap *cores;
	int               i;

	cores = &m0_get()->i_proc_attr.pca_core_mask;

	if (m0_bitmap_set_nr(cores) == 0)
		return;

	for (i = 0; i < cores->b_nr && i < onln_cpu_map->b_nr; ++i)
		if (!m0_bitmap_get(cores, i))
			m0_bitmap_set(onln_cpu_map, i, false);
}

static void hung_foms_notify(struct m0_locality_chore *chore,
			     struct m0_locality *loc, void *place)
{
	struct m0_fom_locality *floc = container_of(loc, struct m0_fom_locality,
						    fl_locality);
	const struct m0_fom_domain *dom = floc->fl_dom;

	(void)m0_tl_forall(runq, fom, &floc->fl_runq,
			   dom->fd_ops->fdo_time_is_out(dom, fom));
	(void)m0_tl_forall(wail, fom, &floc->fl_wail,
			   dom->fd_ops->fdo_time_is_out(dom, fom));
}

M0_INTERNAL int m0_fom_domain_init(struct m0_fom_domain **out)
{
	struct m0_fom_domain   *dom;
	struct m0_fom_locality *loc;
	int                     result;
	size_t                  cpu_max;
	size_t                  cpu_nr;
	size_t                  i;
	struct m0_bitmap        cpu_map;

	M0_ENTRY();

	cpu_max = m0_processor_nr_max();
	result = m0_bitmap_init(&cpu_map, cpu_max);
	if (result != 0)
		return M0_ERR(result);

	m0_processors_online(&cpu_map);
	core_mask_apply(&cpu_map);
	cpu_nr = m0_bitmap_set_nr(&cpu_map);

	M0_ALLOC_PTR(dom);
	if (dom == NULL) {
		m0_bitmap_fini(&cpu_map);
		return M0_ERR(-ENOMEM);
	}
	dom->fd_ops = &m0_fom_dom_ops;

	result = m0_addb2_sys_init(&dom->fd_addb2_sys,
				   &(struct m0_addb2_config) {
					   .co_queue_max = 1024 * 1024,
					   .co_pool_min  = cpu_nr,
					   .co_pool_max  = cpu_nr
				   });
	if (result == 0) {
		M0_ALLOC_ARR(dom->fd_localities, cpu_nr);
		if (dom->fd_localities != NULL) {
			dom->fd_localities_nr = cpu_nr;
			for (i = 0; i < cpu_nr; ++i) {
				/* Do not support holes in cpu mask. */
				M0_ASSERT(m0_bitmap_get(&cpu_map, i));
				M0_ALLOC_PTR(loc);
				if (loc != NULL) {
					result = loc_init(loc, dom, i);
					if (result == 0)
						dom->fd_localities[i] = loc;
					else
						m0_free(loc);
				} else
					result = M0_ERR(-ENOMEM);
				if (result != 0)
					break;
			}
			if (result == 0) {
				m0_locality_dom_set(dom);
				/* Wake up handler threads. */
				for (i = 0; i < cpu_nr; ++i) {
					loc = dom->fd_localities[i];
					group_lock(loc);
					m0_chan_signal(&loc->fl_idle);
					group_unlock(loc);
				}

				m0_locality_chore_init(&dom->fd_hung_foms_chore,
				       &hung_foms_chore_ops,
				       NULL,
				       M0_MKTIME(HUNG_FOP_SEC_PERIOD, 0),
				       0);
			}
		} else
			result = M0_ERR(-ENOMEM);
	}
	m0_bitmap_fini(&cpu_map);
	if (result == 0)
		*out = dom;
	else {
		*out = NULL;
		m0_fom_domain_fini(dom);
	}
	return result;
}

M0_INTERNAL void m0_fom_domain_fini(struct m0_fom_domain *dom)
{
	int i;

	m0_locality_chore_fini(&dom->fd_hung_foms_chore);
	if (dom->fd_localities != NULL) {
		for (i = dom->fd_localities_nr - 1; i >= 0; --i) {
			if (dom->fd_localities[i] != NULL)
				loc_fini(dom->fd_localities[i]);
		}
		m0_locality_dom_unset(dom);
		for (i = 0; i < dom->fd_localities_nr; i++)
			m0_free(dom->fd_localities[i]);
		m0_free(dom->fd_localities);
	}
	if (dom->fd_addb2_sys != NULL)
		m0_addb2_sys_fini(dom->fd_addb2_sys);
	m0_free(dom);
}

static bool is_loc_locker_empty(struct m0_fom_locality *loc, uint32_t key)
{
	return m0_locality_lockers_is_empty(&loc->fl_locality, key);
}

M0_INTERNAL bool m0_fom_domain_is_idle_for(const struct m0_reqh_service *svc)
{
	struct m0_fom_domain *dom = m0_fom_dom();
	return m0_forall(i, dom->fd_localities_nr,
			 is_loc_locker_empty(dom->fd_localities[i],
					     svc->rs_fom_key));
}

M0_INTERNAL bool m0_fom_domain_is_idle(const struct m0_fom_domain *dom)
{
	return m0_forall(i, dom->fd_localities_nr,
			 dom->fd_localities[i]->fl_foms == 0);
}

M0_INTERNAL void m0_fom_locality_inc(struct m0_fom *fom)
{
	unsigned                key = fom->fo_service->rs_fom_key;
	struct m0_fom_locality *loc = fom->fo_loc;
	uint64_t                cnt;

	M0_ASSERT(key != 0);
	cnt = (uint64_t)m0_locality_lockers_get(&loc->fl_locality, key);
	M0_CNT_INC(cnt);
	M0_CNT_INC(loc->fl_foms);
	m0_addb2_hist_mod(&loc->fl_fom_active, loc->fl_foms);
	m0_locality_lockers_set(&loc->fl_locality, key, (void *)cnt);
}

M0_INTERNAL bool m0_fom_locality_dec(struct m0_fom *fom)
{
	unsigned                key = fom->fo_service->rs_fom_key;
	struct m0_fom_locality *loc = fom->fo_loc;
	uint64_t                cnt;

	M0_ASSERT(key != 0);
	cnt = (uint64_t)m0_locality_lockers_get(&loc->fl_locality, key);
	M0_CNT_DEC(cnt);
	M0_CNT_DEC(loc->fl_foms);
	m0_locality_lockers_set(&loc->fl_locality, key, (void *)cnt);
	m0_addb2_hist_mod(&loc->fl_fom_active, loc->fl_foms);
	return cnt == 0;
}

void m0_fom_fini(struct m0_fom *fom)
{
	struct m0_reqh *reqh;

	M0_ENTRY("fom: %p fop %p rep fop %p", fom, fom->fo_fop,
					      fom->fo_rep_fop);
	M0_PRE(m0_fom_phase(fom) == M0_FOM_PHASE_FINISH);
	M0_PRE(fom->fo_pending == NULL);

	reqh = m0_fom_reqh(fom);
	fom_state_set(fom, M0_FOS_FINISH);

	m0_sm_fini(&fom->fo_sm_phase);
	m0_sm_fini(&fom->fo_sm_state);
	runq_tlink_fini(fom);
	m0_fom_callback_fini(&fom->fo_cb);

	if (fom->fo_fop != NULL) {
		M0_LOG(M0_DEBUG, "fom: %p fop %p item %p[%u] rep fop %p",
		       fom, fom->fo_fop, &fom->fo_fop->f_item,
				m0_fop_opcode(fom->fo_fop), fom->fo_rep_fop);
		m0_fop_put_lock(fom->fo_fop);
	}
	if (fom->fo_rep_fop != NULL)
		m0_fop_put_lock(fom->fo_rep_fop);

	/*
	 * Channel lock is taken before decrementing the locality count
	 * because otherwise, a race window is left out because of which
	 * the function m0_reqh_idle_wait_for(), which checks the value of
	 * this locality count to figure out whether request handler is idle,
	 * can exit prematurely. This could result in an attempt to lock or
	 * unlock the channel mutex even after it is finalized by the caller
	 * of m0_reqh_idle_wait_for() function.
	 *
	 * TODO: This being a hot path, instead of taking and releasing a
	 * global lock, fom and service finalisations should synchronise
	 * through an RCU-like mechanism.
	 */
	m0_chan_lock(&reqh->rh_sm_grp.s_chan);
	if (m0_fom_locality_dec(fom))
		m0_chan_broadcast(&reqh->rh_sm_grp.s_chan);
	m0_chan_unlock(&reqh->rh_sm_grp.s_chan);

	M0_LEAVE();
}
M0_EXPORTED(m0_fom_fini);

void m0_fom_init(struct m0_fom *fom, const struct m0_fom_type *fom_type,
		 const struct m0_fom_ops *ops, struct m0_fop *fop,
		 struct m0_fop *reply, struct m0_reqh *reqh)
{
	M0_PRE(fom != NULL);
	M0_PRE(reqh != NULL);

	M0_ENTRY("fom: %p fop %p rep fop %p", fom, fop, reply);

	fom->fo_type	    = fom_type;
	fom->fo_ops	    = ops;
	fom->fo_transitions = 0;
	fom->fo_local	    = false;
	m0_fom_callback_init(&fom->fo_cb);
	runq_tlink_init(fom);

	if (fop != NULL) {
		m0_fop_get(fop);
		M0_LOG(M0_DEBUG, "fom: %p fop %p item %p[%u] rep fop %p",
		       fom, fop, &fop->f_item, m0_fop_opcode(fop), reply);
	}

	fom->fo_fop = fop;

	if (reply != NULL) {
		m0_fop_get(reply);
		fop->f_item.ri_reply = &reply->f_item;
	}
	fom->fo_rep_fop = reply;

	/**
	 * @note The service may be in M0_RST_STARTING state
	 * if the fom was launched on startup
	 */
	fom->fo_service = m0_reqh_service_find(fom_type->ft_rstype, reqh);

	M0_ASSERT(fom->fo_service != NULL);
	M0_LEAVE();
}
M0_EXPORTED(m0_fom_init);

static bool fom_clink_cb(struct m0_clink *link)
{
	struct m0_fom_callback *cb = container_of(link, struct m0_fom_callback,
						  fc_clink);
	M0_PRE(cb->fc_state >= M0_FCS_ARMED);

	if (cb->fc_state == M0_FCS_ARMED &&
	    (cb->fc_top == NULL || !cb->fc_top(cb)))
		m0_sm_ast_post(&cb->fc_fom->fo_loc->fl_group, &cb->fc_ast);

	return true;
}

static void fom_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_fom_callback *cb  = container_of(ast, struct m0_fom_callback,
						   fc_ast);
	struct m0_fom          *fom = cb->fc_fom;

	M0_PRE(m0_fom_invariant(fom));
	M0_PRE(cb->fc_state == M0_FCS_ARMED);

	if (fom_state(fom) == M0_FOS_WAITING)
		cb_run(cb);
	else {
		M0_ASSERT(fom_state(fom) == M0_FOS_READY ||
			  fom_is_blocked(fom));
		/*
		 * Call-back arrived while our fom is in READY state (hanging on
		 * the runqueue, waiting for its turn) or RUNNING state (blocked
		 * between m0_fom_block_enter() and
		 * m0_fom_block_leave()). Instead of executing the call-back
		 * immediately, add it to the stack of pending call-backs for
		 * this fom. The call-back will be executed by fom_exec() when
		 * the fom is about to return to the WAITING state.
		 */
		cb->fc_ast.sa_next = (void *)fom->fo_pending;
		fom->fo_pending = cb;
	}
}

M0_INTERNAL void m0_fom_callback_init(struct m0_fom_callback *cb)
{
	cb->fc_state = M0_FCS_DONE;
	m0_clink_init(&cb->fc_clink, fom_clink_cb);
}

M0_INTERNAL void m0_fom_callback_arm(struct m0_fom *fom, struct m0_chan *chan,
				     struct m0_fom_callback *cb)
{
	M0_PRE(cb->fc_bottom != NULL);
	M0_PRE(cb->fc_state == M0_FCS_DONE);

	cb->fc_fom = fom;

	cb->fc_ast.sa_cb = &fom_ast_cb;
	cb->fc_state = M0_FCS_ARMED;
	m0_mb();
	cb->fc_clink.cl_is_oneshot = true;
	m0_clink_add(chan, &cb->fc_clink);
}

static bool fom_callback_is_armed(const struct m0_fom_callback *cb)
{
	return cb->fc_state == M0_FCS_ARMED;
}

M0_INTERNAL bool m0_fom_is_waiting_on(const struct m0_fom *fom)
{
	return fom_callback_is_armed(&fom->fo_cb);
}

static void fom_ready_cb(struct m0_fom_callback *cb)
{
	m0_fom_ready(cb->fc_fom);
}

M0_INTERNAL void m0_fom_wait_on(struct m0_fom *fom, struct m0_chan *chan,
				struct m0_fom_callback *cb)
{
	cb->fc_bottom = fom_ready_cb;
	m0_fom_callback_arm(fom, chan, cb);
}

M0_INTERNAL void m0_fom_callback_fini(struct m0_fom_callback *cb)
{
	M0_PRE(cb->fc_state == M0_FCS_DONE);
	m0_clink_fini(&cb->fc_clink);
}

static void cb_cancel(struct m0_fom_callback *cb)
{
	struct m0_fom_callback *prev;

	prev = cb->fc_fom->fo_pending;
	while (prev != NULL && cb_next(prev) != cb)
		prev = cb_next(prev);
	if (prev != NULL)
		prev->fc_ast.sa_next = cb_next(cb);
}

M0_INTERNAL void m0_fom_callback_cancel(struct m0_fom_callback *cb)
{
	struct m0_clink *clink = &cb->fc_clink;

	M0_PRE(cb->fc_state >= M0_FCS_ARMED);

	if (cb->fc_state == M0_FCS_ARMED) {
		m0_clink_cleanup_locked(clink);
		cb_done(cb);
		/* Once the clink is finalised, the AST cannot be posted, cancel
		   the AST. */
		m0_sm_ast_cancel(&cb->fc_fom->fo_loc->fl_group, &cb->fc_ast);
		/* Once the AST is cancelled, cb cannot be added to the pending
		   list, cancel cb. */
		cb_cancel(cb);
	}
}

M0_INTERNAL void m0_fom_timeout_init(struct m0_fom_timeout *to)
{
	M0_SET0(to);
	m0_sm_timer_init(&to->to_timer);
	m0_fom_callback_init(&to->to_cb);
}

M0_INTERNAL void m0_fom_timeout_fini(struct m0_fom_timeout *to)
{
	m0_fom_callback_fini(&to->to_cb);
	m0_sm_timer_fini(&to->to_timer);
}

static void fom_timeout_cb(struct m0_sm_timer *timer)
{
	struct m0_fom_timeout  *to = container_of(timer, struct m0_fom_timeout,
						  to_timer);
	struct m0_fom_callback *cb = &to->to_cb;

	cb->fc_state = M0_FCS_ARMED;
	fom_ast_cb(to->to_timer.tr_grp, &cb->fc_ast);
}

static int fom_timeout_start(struct m0_fom_timeout *to,
			     struct m0_fom *fom,
			     void (*cb)(struct m0_fom_callback *),
			     m0_time_t deadline)
{
	to->to_cb.fc_fom    = fom;
	to->to_cb.fc_bottom = cb;
	return m0_sm_timer_start(&to->to_timer, fom->fo_sm_state.sm_grp,
				 fom_timeout_cb, deadline);
}

M0_INTERNAL int m0_fom_timeout_wait_on(struct m0_fom_timeout *to,
				       struct m0_fom *fom,
				       m0_time_t deadline)
{
	return fom_timeout_start(to, fom, fom_ready_cb, deadline);
}

M0_INTERNAL int m0_fom_timeout_arm(struct m0_fom_timeout *to,
				   struct m0_fom *fom,
				   void (*cb)(struct m0_fom_callback *),
				   m0_time_t deadline)
{
	return fom_timeout_start(to, fom, cb, deadline);
}

M0_INTERNAL void m0_fom_timeout_cancel(struct m0_fom_timeout *to)
{
	struct m0_fom_callback *cb = &to->to_cb;
	struct m0_sm_timer     *tr = &to->to_timer;

	if (m0_sm_timer_is_armed(tr) || fom_callback_is_armed(cb)) {
		M0_PRE(m0_fom_invariant(cb->fc_fom));

		m0_sm_timer_cancel(tr);
		m0_fom_callback_cancel(cb);
	}
}

M0_INTERNAL struct m0_fom_type *m0_fom__types[M0_OPCODES_NR];

M0_INTERNAL void m0_fom_type_init(struct m0_fom_type *type, uint64_t id,
				  const struct m0_fom_type_ops *ops,
				  const struct m0_reqh_service_type *svc_type,
				  const struct m0_sm_conf *sm)
{
	M0_PRE(IS_IN_ARRAY(id, m0_fom__types));
	M0_PRE(id > 0);
	M0_PRE(M0_IN(m0_fom__types[id], (NULL, type)));

	if (m0_fom__types[id] == NULL) {
		type->ft_id         = id;
		type->ft_ops        = ops;
		if (sm != NULL)
			type->ft_conf = *sm;
		type->ft_state_conf = fom_states_conf0;
		type->ft_rstype     = svc_type;
		m0_fom__types[id]   = type;
	}
}

static struct m0_sm_state_descr fom_states[] = {
	[M0_FOS_INIT] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Init",
		.sd_allowed   = M0_BITS(M0_FOS_FINISH, M0_FOS_READY)
	},
	[M0_FOS_READY] = {
		.sd_name      = "Ready",
		.sd_allowed   = M0_BITS(M0_FOS_RUNNING)
	},
	[M0_FOS_RUNNING] = {
		.sd_name      = "Running",
		.sd_allowed   = M0_BITS(M0_FOS_READY, M0_FOS_WAITING,
					M0_FOS_FINISH)
	},
	[M0_FOS_WAITING] = {
		.sd_name      = "Waiting",
		.sd_allowed   = M0_BITS(M0_FOS_READY, M0_FOS_FINISH)
	},
	[M0_FOS_FINISH] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Finished",
	}
};

static struct m0_sm_trans_descr fom_trans[M0_FOS_TRANS_NR] = {
	{ "Schedule",  M0_FOS_INIT,     M0_FOS_READY },
	{ "Failed",    M0_FOS_INIT,     M0_FOS_FINISH },
	{ "Run",       M0_FOS_READY,    M0_FOS_RUNNING },
	{ "Yield",     M0_FOS_RUNNING,  M0_FOS_READY },
	{ "Sleep",     M0_FOS_RUNNING,  M0_FOS_WAITING },
	{ "Done",      M0_FOS_RUNNING,  M0_FOS_FINISH },
	{ "Wakeup",    M0_FOS_WAITING,  M0_FOS_READY },
	{ "Terminate", M0_FOS_WAITING,  M0_FOS_FINISH }
};

M0_INTERNAL struct m0_sm_conf fom_states_conf = {
	.scf_name      = "FOM states",
	.scf_nr_states = ARRAY_SIZE(fom_states),
	.scf_state     = fom_states,
	.scf_trans_nr  = ARRAY_SIZE(fom_trans),
	.scf_trans     = fom_trans
};

static struct m0_sm_conf fom_states_conf0;

M0_INTERNAL int m0_foms_init(void)
{
	fom_states_conf0 = fom_states_conf;
	return m0_sm_addb2_init(&fom_states_conf,
				M0_AVI_STATE, M0_AVI_STATE_COUNTER);
}

M0_INTERNAL void m0_foms_fini(void)
{
	m0_sm_addb2_fini(&fom_states_conf);
}

M0_INTERNAL void m0_fom_sm_init(struct m0_fom *fom)
{
	struct m0_sm_group *fom_group;

	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_loc != NULL);

	fom_group = &fom->fo_loc->fl_group;
	m0_sm_init(&fom->fo_sm_phase, &fom->fo_type->ft_conf,
		   M0_FOM_PHASE_INIT, fom_group);
	m0_sm_init(&fom->fo_sm_state, &fom->fo_type->ft_state_conf,
		   M0_FOS_INIT, fom_group);
}

void m0_fom_phase_set(struct m0_fom *fom, int phase)
{
	M0_LOG(M0_DEBUG, "fom=%p, item %p[%u] phase set: %s -> %s", fom,
	       fom->fo_fop == NULL ? NULL : &fom->fo_fop->f_item,
	       fom->fo_fop == NULL ? 0 : m0_fop_opcode(fom->fo_fop),
	       m0_fom_phase_name(fom, m0_fom_phase(fom)),
	       m0_fom_phase_name(fom, phase));
	m0_sm_state_set(&fom->fo_sm_phase, phase);
}
M0_EXPORTED(m0_fom_phase_set);

void m0_fom_phase_move(struct m0_fom *fom, int32_t rc, int phase)
{
	M0_LOG(M0_DEBUG, "fom=%p, item %p[%u] phase set: %s -> %s", fom,
	       fom->fo_fop == NULL ? NULL : &fom->fo_fop->f_item,
	       fom->fo_fop == NULL ? 0 : m0_fop_opcode(fom->fo_fop),
	       m0_fom_phase_name(fom, m0_fom_phase(fom)),
	       m0_fom_phase_name(fom, phase));
	m0_sm_move(&fom->fo_sm_phase, rc, phase);
}
M0_EXPORTED(m0_fom_phase_move);

void m0_fom_phase_moveif(struct m0_fom *fom, int32_t rc, int phase0, int phase1)
{
	m0_fom_phase_move(fom, rc, rc == 0 ? phase0 : phase1);
}
M0_EXPORTED(m0_fom_phase_moveif);

int m0_fom_phase(const struct m0_fom *fom)
{
	return fom->fo_sm_phase.sm_state;
}
M0_EXPORTED(m0_fom_phase);

M0_INTERNAL const char *m0_fom_phase_name(const struct m0_fom *fom, int phase)
{
	return m0_sm_state_name(&fom->fo_sm_phase, phase);
}

M0_INTERNAL int m0_fom_rc(const struct m0_fom *fom)
{
	return fom->fo_sm_phase.sm_rc;
}

M0_INTERNAL bool m0_fom_is_waiting(const struct m0_fom *fom)
{
	return fom_state(fom) == M0_FOS_WAITING && is_in_wail(fom);
}

M0_INTERNAL int m0_fom_fol_rec_add(struct m0_fom *fom)
{
	int rc;

	M0_ENTRY();

	rc = m0_dtx_fol_add(&fom->fo_tx);
	if (rc != 0)
		goto done;

#ifndef __KERNEL__
	rc = m0_fol_fdmi_post_record(fom);
	if (rc != 0)
		goto done;
#endif

done:
	return M0_RC(rc);
}

M0_INTERNAL struct m0_reqh *m0_fom2reqh(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL && fom->fo_service != NULL);
	return fom->fo_service->rs_reqh;
}

#undef M0_TRACE_SUBSYSTEM

/** @} endgroup fom */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
