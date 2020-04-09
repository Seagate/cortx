/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 04/01/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SM
#include "lib/trace.h"              /* m0_console_printf */

#include "lib/errno.h"              /* ESRCH */
#include "lib/misc.h"               /* M0_SET0, M0_IS0 */
#include "lib/mutex.h"
#include "lib/thread.h"             /* m0_thread_self */
#include "lib/arith.h"              /* m0_is_po2 */
#include "lib/memory.h"
#include "lib/finject.h"
#include "lib/locality.h"           /* m0_locality_data_alloc */
#include "addb2/addb2.h"
#include "addb2/identifier.h"
#include "sm/sm.h"

/**
   @addtogroup sm
   @{
*/

/**
 * An end-of-queue marker.
 *
 * All fork queues end with this pointer. This marker is used instead of NULL to
 * make (ast->sa_next == NULL) equivalent to "the ast is not in a fork queue".
 *
 * Compare with lib/queue.c:EOQ.
 */
static struct m0_sm_ast eoq;

M0_INTERNAL void m0_sm_group_init(struct m0_sm_group *grp)
{
	M0_SET0(grp);
	grp->s_forkq = &eoq;
	m0_mutex_init(&grp->s_lock);
	/* add grp->s_clink to otherwise unused grp->s_chan, because m0_chan
	   code assumes that a clink is always associated with a channel. */
	m0_chan_init(&grp->s_chan, &grp->s_lock);
	m0_clink_init(&grp->s_clink, NULL);
	m0_clink_add_lock(&grp->s_chan, &grp->s_clink);
}

M0_INTERNAL void m0_sm_group_fini(struct m0_sm_group *grp)
{
	M0_PRE(grp->s_forkq == &eoq);

	if (m0_clink_is_armed(&grp->s_clink))
		m0_clink_del_lock(&grp->s_clink);
	m0_clink_fini(&grp->s_clink);
	m0_chan_fini_lock(&grp->s_chan);
	m0_mutex_fini(&grp->s_lock);
}

static void _sm_group_lock(struct m0_sm_group *grp)
{
	m0_mutex_lock(&grp->s_lock);
	M0_PRE(grp->s_nesting == 0);
	grp->s_nesting = 1;
}

M0_INTERNAL void m0_sm_group_lock(struct m0_sm_group *grp)
{
	_sm_group_lock(grp);
	m0_sm_asts_run(grp);
}

static void _sm_group_unlock(struct m0_sm_group *grp)
{
	M0_PRE(grp->s_nesting == 1);
	grp->s_nesting = 0;
	m0_mutex_unlock(&grp->s_lock);
}

M0_INTERNAL void m0_sm_group_unlock(struct m0_sm_group *grp)
{
	m0_sm_asts_run(grp);
	_sm_group_unlock(grp);
}

static bool grp_is_locked(const struct m0_sm_group *grp)
{
	return m0_mutex_is_locked(&grp->s_lock);
}

M0_INTERNAL bool m0_sm_group_is_locked(const struct m0_sm_group *grp)
{
	return grp_is_locked(grp);
}

M0_INTERNAL void m0_sm_group_lock_rec(struct m0_sm_group *grp, bool runast)
{
	if (grp->s_lock.m_owner == m0_thread_self())
		++grp->s_nesting;
	else {
		_sm_group_lock(grp);
		if (runast)
			m0_sm_asts_run(grp);
	}
}

M0_INTERNAL void m0_sm_group_unlock_rec(struct m0_sm_group *grp, bool runast)
{
	M0_PRE(grp->s_lock.m_owner == m0_thread_self());
	if (grp->s_nesting > 1)
		--grp->s_nesting;
	else {
		_sm_group_unlock(grp);
		if (runast)
			m0_sm_asts_run(grp);
	}
}

M0_INTERNAL void m0_sm_ast_post(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	M0_PRE(ast->sa_cb != NULL);
	M0_PRE(ast->sa_next == NULL);

	do {
		ast->sa_next = grp->s_forkq;
		M0_LOG(M0_DEBUG, "grp=%p forkq_push: %p -> %p",
		       grp, ast, ast->sa_next);
	} while (!M0_ATOMIC64_CAS(&grp->s_forkq, ast->sa_next, ast));
	m0_clink_signal(&grp->s_clink);
}

M0_INTERNAL void m0_sm_asts_run(struct m0_sm_group *grp)
{
	struct m0_sm_ast *ast;

	M0_PRE(grp_is_locked(grp));

	while (1) {
		do {
			ast = grp->s_forkq;
			M0_LOG(M0_DEBUG, "grp=%p forkq_pop: %p <- %p",
			       grp, ast, ast->sa_next);
		} while (ast != &eoq &&
		       !M0_ATOMIC64_CAS(&grp->s_forkq, ast, ast->sa_next));

		if (ast == &eoq)
			break;
		M0_ASSERT(ast->sa_next != NULL);

		ast->sa_next = NULL;
		M0_ASSERT(!m0_is_poisoned(ast->sa_cb));
		if (grp->s_addb2 == NULL) {
			ast->sa_cb(grp, ast);
		} else {
			M0_ADDB2_HIST(grp->s_addb2->ga_forq,
				      &grp->s_addb2->ga_forq_hist,
				      m0_ptr_wrap(ast->sa_cb),
				      ast->sa_cb(grp, ast));
		}
	}
}

M0_INTERNAL void m0_sm_ast_cancel(struct m0_sm_group *grp,
				  struct m0_sm_ast *ast)
{
	M0_PRE(grp_is_locked(grp));
	/*
	 * Concurrency: this function runs under the group lock and the only
	 * other possible concurrent fork queue activity is addition of the new
	 * asts at the head of the queue (m0_sm_ast_post()).
	 *
	 * Hence, the queue head is handled specially, with CAS. The rest of the
	 * queue is scanned normally.
	 */
	if (ast->sa_next == NULL)
		return ; /* not in the queue. */

	if (ast == grp->s_forkq /* cheap check first */ &&
	    M0_ATOMIC64_CAS(&grp->s_forkq, ast, ast->sa_next))
		; /* deleted the head. */
	else {
		struct m0_sm_ast *prev;
		/*
		 * This loop is safe.
		 *
		 * On the first iteration, grp->s_forkq can be changed
		 * concurrently by m0_sm_ast_post(), but "prev" is still a valid
		 * queue element, because removal from the queue is under the
		 * lock. Newly inserted head elements are not scanned.
		 *
		 * On the iterations after the first, immutable portion of the
		 * queue is scanned.
		 */
		prev = grp->s_forkq;
		while (prev->sa_next != ast)
			prev = prev->sa_next;
		prev->sa_next = ast->sa_next;
	}
	ast->sa_next = NULL;
}

static void sm_lock(struct m0_sm *mach)
{
	m0_sm_group_lock(mach->sm_grp);
}

static void sm_unlock(struct m0_sm *mach)
{
	m0_sm_group_unlock(mach->sm_grp);
}

static bool sm_is_locked(const struct m0_sm *mach)
{
	return grp_is_locked(mach->sm_grp);
}

static bool state_is_valid(const struct m0_sm_conf *conf, uint32_t state)
{
	return
		state < conf->scf_nr_states &&
		conf->scf_state[state].sd_name != NULL;
}

static const struct m0_sm_state_descr *state_get(const struct m0_sm *mach,
						 uint32_t state)
{
	M0_PRE(state_is_valid(mach->sm_conf, state));
	return &mach->sm_conf->scf_state[state];
}

static const struct m0_sm_state_descr *sm_state(const struct m0_sm *mach)
{
	return state_get(mach, mach->sm_state);
}

/**
 * Weaker form of state machine invariant, that doesn't check that the group
 * lock is held. Used in m0_sm_init() and m0_sm_fini().
 */
M0_INTERNAL bool sm_invariant0(const struct m0_sm *mach)
{
	const struct m0_sm_state_descr *sd = sm_state(mach);

	return _0C(ergo(sd->sd_invariant != NULL, sd->sd_invariant(mach)));
}

M0_INTERNAL bool m0_sm_invariant(const struct m0_sm *mach)
{
	if (mach->sm_invariant_chk_off)
		return true;
	return _0C(sm_is_locked(mach)) && sm_invariant0(mach);
}

static bool conf_invariant(const struct m0_sm_conf *conf)
{
	uint32_t i;
	uint64_t mask;

	if (conf->scf_nr_states >= sizeof(conf->scf_state[0].sd_allowed) * 8) {
		m0_failed_condition = "wrong states_nr";
		return false;
	}

	for (i = 0, mask = 0; i < conf->scf_nr_states; ++i) {
		if (state_is_valid(conf, i))
			mask |= M0_BITS(i);
	}

	for (i = 0; i < conf->scf_nr_states; ++i) {
		if (mask & M0_BITS(i)) {
			const struct m0_sm_state_descr *sd;

			sd = &conf->scf_state[i];
			if (sd->sd_flags & ~(M0_SDF_INITIAL|M0_SDF_FINAL|
					     M0_SDF_FAILURE|M0_SDF_TERMINAL)) {
				m0_failed_condition = "odd sd_flags";
				return false;
			}
			if ((sd->sd_flags & M0_SDF_TERMINAL) &&
			    sd->sd_allowed != 0) {
				m0_failed_condition = "terminal sd_allowed";
				return false;
			}
			if (sd->sd_allowed & ~mask) {
				m0_failed_condition = "odd sd_allowed";
				return false;
			}
		}
	}
	return true;
}

M0_INTERNAL void m0_sm_init(struct m0_sm *mach, const struct m0_sm_conf *conf,
			    uint32_t state, struct m0_sm_group *grp)
{
	M0_PRE(conf_invariant(conf));
	M0_PRE(conf->scf_state[state].sd_flags & M0_SDF_INITIAL);

	mach->sm_state       = state;
	mach->sm_conf        = conf;
	mach->sm_grp         = grp;
	mach->sm_rc          = 0;
	mach->sm_id          = m0_dummy_id_generate();
	mach->sm_state_epoch = m0_time_now();
	mach->sm_addb2_stats = NULL;
	mach->sm_invariant_chk_off = false;
	m0_chan_init(&mach->sm_chan, &grp->s_lock);
	M0_POST(sm_invariant0(mach));
}

M0_INTERNAL void m0_sm_fini(struct m0_sm *mach)
{
	M0_ASSERT(sm_invariant0(mach));
	M0_PRE(sm_state(mach)->sd_flags & (M0_SDF_TERMINAL | M0_SDF_FINAL));
	m0_chan_fini(&mach->sm_chan);
}

M0_INTERNAL void (*m0_sm__conf_init)(const struct m0_sm_conf *conf) = NULL;

M0_INTERNAL void m0_sm_conf_init(struct m0_sm_conf *conf)
{
	uint32_t i;
	uint32_t from;
	uint32_t to;

	M0_PRE(!m0_sm_conf_is_initialized(conf));
	M0_PRE(conf->scf_trans_nr > 0);

	M0_ASSERT(conf->scf_nr_states < M0_SM_MAX_STATES);

	if (m0_sm__conf_init != NULL)
		m0_sm__conf_init(conf);

	for (i = 0; i < conf->scf_nr_states; ++i)
		for (to = 0; to < conf->scf_nr_states; ++to)
			conf->scf_state[i].sd_trans[to] = ~0;

	for (i = 0; i < conf->scf_trans_nr; ++i) {
		from = conf->scf_trans[i].td_src;
		to = conf->scf_trans[i].td_tgt;
		M0_ASSERT(conf->scf_state[from].sd_allowed & M0_BITS(to));
		conf->scf_state[from].sd_trans[to] = i;
	}

	for (i = 0; i < conf->scf_nr_states; ++i)
		for (to = 0; to < conf->scf_nr_states; ++to)
			M0_ASSERT(ergo(conf->scf_state[i].sd_allowed &
				       M0_BITS(to),
				       conf->scf_state[i].sd_trans[to] != ~0));

	conf->scf_magic = M0_SM_CONF_MAGIC;

	M0_POST(m0_sm_conf_is_initialized(conf));
}

M0_INTERNAL void m0_sm_conf_fini(struct m0_sm_conf *conf)
{
	M0_PRE(conf->scf_magic == M0_SM_CONF_MAGIC);
	conf->scf_magic = 0;
}

M0_INTERNAL bool m0_sm_conf_is_initialized(const struct m0_sm_conf *conf)
{
	return conf->scf_magic == M0_SM_CONF_MAGIC;
}

M0_INTERNAL int m0_sm_timedwait(struct m0_sm *mach, uint64_t states,
				m0_time_t deadline)
{
	struct m0_clink waiter;
	int             result;

	M0_ASSERT(m0_sm_invariant(mach));

	result = 0;
	m0_clink_init(&waiter, NULL);

	m0_clink_add(&mach->sm_chan, &waiter);
	while (result == 0 && (M0_BITS(mach->sm_state) & states) == 0) {
		M0_ASSERT(m0_sm_invariant(mach));
		if (sm_state(mach)->sd_flags & M0_SDF_TERMINAL)
			result = -ESRCH;
		else {
			sm_unlock(mach);
			if (!m0_chan_timedwait(&waiter, deadline))
				result = -ETIMEDOUT;
			sm_lock(mach);
		}
	}
	m0_clink_del(&waiter);
	m0_clink_fini(&waiter);
	M0_ASSERT(m0_sm_invariant(mach));
	return result;
}

static void state_set(struct m0_sm *mach, int state, int32_t rc)
{
	const struct m0_sm_state_descr *sd;

	mach->sm_rc = rc;
	/*
	 * Iterate over a possible chain of state transitions.
	 *
	 * State machine invariant can be temporarily violated because ->sm_rc
	 * is set before ->sm_state is updated and, similarly, ->sd_in() might
	 * set ->sm_rc before the next state is entered. In any case, the
	 * invariant is restored the moment->sm_state is updated and must hold
	 * on the loop termination.
	 */
	do {
		struct m0_sm_addb2_stats *stats = mach->sm_addb2_stats;
		uint32_t                  trans;

		sd = sm_state(mach);
		trans = sd->sd_trans[state];
		M0_ASSERT_INFO(sd->sd_allowed & M0_BITS(state), "%s: %s -> %s",
			       mach->sm_conf->scf_name, sd->sd_name,
			       state_get(mach, state)->sd_name);
		if (sd->sd_ex != NULL)
			sd->sd_ex(mach);

		/* Update statistics (if enabled). */
		if (stats != NULL) {
			m0_time_t now = m0_time_now();
			m0_time_t delta;

			delta = m0_time_sub(now, mach->sm_state_epoch) >> 10;
			M0_ASSERT(stats->as_nr == 0 || trans < stats->as_nr);
			if (stats->as_id != 0) {
				M0_ADDB2_ADD(stats->as_id, m0_sm_id_get(mach),
					     trans, state);
			}
			if (stats->as_nr > 0)
				m0_addb2_hist_mod(&stats->as_hist[trans],
						  delta);
			mach->sm_state_epoch = now;
		}
		mach->sm_state = state;
		M0_ASSERT(m0_sm_invariant(mach));
		sd = sm_state(mach);
		state = sd->sd_in != NULL ? sd->sd_in(mach) : -1;
		m0_chan_broadcast(&mach->sm_chan);
	} while (state >= 0);

	M0_POST(m0_sm_invariant(mach));
}

M0_INTERNAL void m0_sm_fail(struct m0_sm *mach, int fail_state, int32_t rc)
{
	M0_PRE(rc != 0);
	M0_PRE(m0_sm_invariant(mach));
	M0_PRE(mach->sm_rc == 0);
	M0_PRE(state_get(mach, fail_state)->sd_flags & M0_SDF_FAILURE);

	state_set(mach, fail_state, rc);
}

void m0_sm_state_set(struct m0_sm *mach, int state)
{
	M0_PRE(m0_sm_invariant(mach));
	state_set(mach, state, 0);
}
M0_EXPORTED(m0_sm_state_set);

M0_INTERNAL void m0_sm_move(struct m0_sm *mach, int32_t rc, int state)
{
	rc == 0 ? m0_sm_state_set(mach, state) : m0_sm_fail(mach, state, rc);
}

/**
 * m0_sm_timer state machine
 *
 * @verbatim
 *                              INIT
 *                                 |
 *                         +-----+ | m0_sm_timer_start()
 *          sm_timer_top() |     | |
 *                         |     V V
 *                         +----ARMED
 *                               | |
 *          m0_sm_timer_cancel() | | sm_timer_bottom()
 *                               | |
 *                               V V
 *                               DONE
 * @endverbatim
 *
 */
enum timer_state {
	INIT,
	ARMED,
	DONE
};

/**
    Timer call-back for a state machine timer.

    @see m0_sm_timer_start().
*/
static unsigned long sm_timer_top(unsigned long data)
{
	struct m0_sm_timer *timer = (void *)data;

	M0_PRE(M0_IN(timer->tr_state, (ARMED, DONE)));
	/*
	 * no synchronisation or memory barriers are needed here: it's OK to
	 * occasionally post the AST when the timer is already cancelled,
	 * because the ast call-back, synchronised with the cancellation by
	 * the group lock, will sort this out.
	 */
	if (timer->tr_state == ARMED)
		m0_sm_ast_post(timer->tr_grp, &timer->tr_ast);
	return 0;
}

static void timer_done(struct m0_sm_timer *timer)
{
	M0_ASSERT(timer->tr_state == ARMED);

	timer->tr_state = DONE;
	m0_timer_stop(&timer->tr_timer);
}

/**
    AST call-back for a timer.

    @see m0_sm_timer_start().
*/
static void sm_timer_bottom(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_sm_timer *tr = container_of(ast, struct m0_sm_timer, tr_ast);

	M0_PRE(grp_is_locked(tr->tr_grp));
	M0_ASSERT(tr->tr_state == ARMED);

	timer_done(tr);
	tr->tr_cb(tr);
}

M0_INTERNAL void m0_sm_timer_init(struct m0_sm_timer *timer)
{
	M0_SET0(timer);
	timer->tr_state     = INIT;
	timer->tr_ast.sa_cb = sm_timer_bottom;
}

M0_INTERNAL void m0_sm_timer_fini(struct m0_sm_timer *timer)
{
	M0_PRE(M0_IN(timer->tr_state, (INIT, DONE)));
	M0_PRE(timer->tr_ast.sa_next == NULL);

	if (timer->tr_state == DONE) {
		M0_ASSERT(!m0_timer_is_started(&timer->tr_timer));
		m0_timer_fini(&timer->tr_timer);
	}
}

M0_INTERNAL int m0_sm_timer_start(struct m0_sm_timer *timer,
				  struct m0_sm_group *group,
				  void (*cb)(struct m0_sm_timer *),
				  m0_time_t deadline)
{
	int result;

	M0_PRE(grp_is_locked(group));
	M0_PRE(timer->tr_state == INIT);
	M0_PRE(cb != NULL);

	/*
	 * This is how timer is implemented:
	 *
	 *    - a timer is armed (with sm_timer_top() call-back);
	 *
	 *    - when the timer fires off, an AST to the state machine group is
	 *      posted from the timer call-back;
	 *
	 *    - the AST invokes user-supplied call-back.
	 */

	result = m0_timer_init(&timer->tr_timer, M0_TIMER_HARD, NULL,
			       sm_timer_top, (unsigned long)timer);
	if (result == 0) {
		timer->tr_state = ARMED;
		timer->tr_grp   = group;
		timer->tr_cb    = cb;
		m0_timer_start(&timer->tr_timer, deadline);
	}
	return result;
}

M0_INTERNAL void m0_sm_timer_cancel(struct m0_sm_timer *timer)
{
	M0_PRE(grp_is_locked(timer->tr_grp));
	M0_PRE(M0_IN(timer->tr_state, (ARMED, DONE)));

	if (timer->tr_state == ARMED) {
		timer_done(timer);
		/*
		 * Once timer_done() returned, the timer call-back
		 * (sm_timer_top()) is guaranteed to be never executed, so the
		 * ast won't be posted. Hence, it is safe to remove it, if it
		 * is here.
		 */
		m0_sm_ast_cancel(timer->tr_grp, &timer->tr_ast);
	}
	M0_POST(timer->tr_ast.sa_next == NULL);
}

M0_INTERNAL bool m0_sm_timer_is_armed(const struct m0_sm_timer *timer)
{
	return timer->tr_state == ARMED;
}

/**
    AST call-back for a timeout.

    @see m0_sm_timeout_arm().
*/
static void timeout_ast(struct m0_sm_timer *timer)
{
	struct m0_sm         *mach = timer->tr_ast.sa_mach;
	struct m0_sm_timeout *to   = container_of(timer,
						  struct m0_sm_timeout,
						  st_timer);
	M0_ASSERT(m0_sm_invariant(mach));
	m0_sm_state_set(mach, to->st_state);
}

/**
   Cancels a timeout, if necessary.

   This is called if a state transition happened before the timeout expired.

   @see m0_sm_timeout_arm().
 */
static bool sm_timeout_cancel(struct m0_clink *link)
{
	struct m0_sm_timeout *to   = container_of(link, struct m0_sm_timeout,
						  st_clink);
	struct m0_sm         *mach = to->st_timer.tr_ast.sa_mach;

	M0_ASSERT(m0_sm_invariant(mach));
	if (!(M0_BITS(mach->sm_state) & to->st_bitmask))
		m0_sm_timer_cancel(&to->st_timer);
	return true;
}

M0_INTERNAL void m0_sm_timeout_init(struct m0_sm_timeout *to)
{
	M0_SET0(to);
	m0_sm_timer_init(&to->st_timer);
	m0_clink_init(&to->st_clink, sm_timeout_cancel);
}

M0_INTERNAL int m0_sm_timeout_arm(struct m0_sm *mach, struct m0_sm_timeout *to,
				  m0_time_t timeout, int state,
				  uint64_t bitmask)
{
	int                 result;
	struct m0_sm_timer *tr = &to->st_timer;

	M0_PRE(m0_sm_invariant(mach));
	M0_PRE(tr->tr_state == INIT);
	M0_PRE(!(sm_state(mach)->sd_flags & M0_SDF_TERMINAL));
	M0_PRE(sm_state(mach)->sd_allowed & M0_BITS(state));
	M0_PRE(m0_forall(i, mach->sm_conf->scf_nr_states,
			 ergo(M0_BITS(i) & bitmask,
			      state_get(mach, i)->sd_allowed & M0_BITS(state))));
	if (M0_FI_ENABLED("failed"))
		return M0_ERR(-EINVAL);

	/*
	 * @todo to->st_clink remains registered with mach->sm_chan even after
	 * the timer expires or is cancelled. This does no harm, but should be
	 * fixed when the support for channels with external locks lands.
	 */
	to->st_state       = state;
	to->st_bitmask     = bitmask;
	tr->tr_ast.sa_mach = mach;
	result = m0_sm_timer_start(tr, mach->sm_grp, timeout_ast, timeout);
	if (result == 0)
		m0_clink_add(&mach->sm_chan, &to->st_clink);
	return result;
}

M0_INTERNAL void m0_sm_timeout_fini(struct m0_sm_timeout *to)
{
	m0_sm_timer_fini(&to->st_timer);
	if (m0_clink_is_armed(&to->st_clink))
		m0_clink_del(&to->st_clink);
	m0_clink_fini(&to->st_clink);
}

M0_INTERNAL bool m0_sm_timeout_is_armed(const struct m0_sm_timeout *to)
{
	return m0_sm_timer_is_armed(&to->st_timer);
}

static bool trans_exists(const struct m0_sm_conf *conf,
			 uint32_t src, uint32_t tgt)
{
	return m0_exists(i, conf->scf_trans_nr,
			 conf->scf_trans[i].td_src == src &&
			 conf->scf_trans[i].td_tgt == tgt);
}

M0_INTERNAL void m0_sm_conf_trans_extend(const struct m0_sm_conf *base,
					 struct m0_sm_conf *sub)
{
	uint32_t i;
	uint32_t j = 0;

	M0_PRE(conf_invariant(base));

	for (i = 0; i < base->scf_trans_nr; i++) {
		const struct m0_sm_trans_descr *b = &base->scf_trans[i];

		if (!trans_exists(sub, b->td_src, b->td_tgt)) {
			/* Find the next empty slot. */
			for (; j < sub->scf_trans_nr; j++) {
				if (sub->scf_trans[j].td_src == 0 &&
				    sub->scf_trans[j].td_tgt == 0)
					break;
			}
			M0_ASSERT(j < sub->scf_trans_nr);
			M0_ASSERT(sub->scf_trans[j].td_cause == NULL);
			sub->scf_trans[j++] = *b;
		}
	}

	/*
	 * Make non-empty transitions in sub to be contiguous. Copy remaining
	 * transitions, skipping the empty ones.
	 */
	for (i = j; i < sub->scf_trans_nr; i++)
		if (sub->scf_trans[i].td_src != 0 ||
		    sub->scf_trans[i].td_tgt != 0)
			sub->scf_trans[j++] = sub->scf_trans[i];
	sub->scf_trans_nr = j;

	M0_POST(conf_invariant(sub));
}

M0_INTERNAL void m0_sm_conf_extend(const struct m0_sm_state_descr *base,
				   struct m0_sm_state_descr *sub, uint32_t nr)
{
	uint32_t i;

	for (i = 0; i < nr; ++i) {
		if (sub[i].sd_name == NULL && base[i].sd_name != NULL)
			sub[i] = base[i];
	}
}

M0_INTERNAL const char *m0_sm_conf_state_name(const struct m0_sm_conf *conf,
					      int state)
{
	return state_is_valid(conf, state) ?
	       conf->scf_state[state].sd_name : "invalid";
}

M0_INTERNAL const char *m0_sm_state_name(const struct m0_sm *mach, int state)
{
	return m0_sm_conf_state_name(mach->sm_conf, state);
}

struct sm_call {
	void               *sc_input;
	int               (*sc_call)(void *);
	int                 sc_output;
	struct m0_semaphore sc_wait;
};

static void sm_call_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct sm_call *sc = ast->sa_datum;

	sc->sc_output = sc->sc_call(sc->sc_input);
	m0_semaphore_up(&sc->sc_wait);
}

int m0_sm_group_call(struct m0_sm_group *group, int (*cb)(void *), void *data)
{
	struct m0_sm_ast ast = {};
	struct sm_call   sc  = {
		.sc_input = data,
		.sc_call  = cb
	};
	m0_semaphore_init(&sc.sc_wait, 0);
	ast.sa_datum = &sc;
	ast.sa_cb    = &sm_call_ast;
	m0_sm_ast_post(group, &ast);
	m0_semaphore_down(&sc.sc_wait);
	m0_semaphore_fini(&sc.sc_wait);
	return sc.sc_output;
}

static int sm_addb2_ctor(struct m0_sm_addb2_stats *stats,
			 const struct m0_sm_conf *c)
{
	int i;

	stats->as_id = c->scf_addb2_id;
	stats->as_nr = c->scf_trans_nr;
	for (i = 0; i < stats->as_nr; ++i) {
		m0_addb2_hist_add_auto(&stats->as_hist[i], 1000 /* skip */,
				     /*
				      * index parameter (2) corresponds to
				      * "standard" labels added to the context
				      * of a locality addb2 machine: node, pid
				      * and locality-id.
				      */
				     c->scf_addb2_counter + i, 2);
	}
	return 0;
}

static void sm_addb2_dtor(struct m0_sm_addb2_stats *stats,
			  const struct m0_sm_conf *c)
{
	int i;

	for (i = 0; i < stats->as_nr; ++i)
		m0_addb2_hist_del(&stats->as_hist[i]);
}

M0_INTERNAL int m0_sm_addb2_init(struct m0_sm_conf *conf,
				 uint64_t id, uint64_t counter)
{
	size_t nob;
	int    result;

	M0_PRE(conf->scf_addb2_id == 0);
	M0_PRE(conf->scf_addb2_counter == 0);
	M0_PRE(conf->scf_addb2_key == 0);

	conf->scf_addb2_id      = id;
	conf->scf_addb2_counter = counter;
	nob = sizeof(struct m0_sm_addb2_stats) +
		conf->scf_trans_nr * M0_MEMBER_SIZE(struct m0_sm_addb2_stats,
						    as_hist[0]);
	result = m0_locality_data_alloc(nob, (void *)&sm_addb2_ctor,
					(void *)sm_addb2_dtor, conf);
	if (result >= 0) {
		conf->scf_addb2_key = result + 1;
		result = 0;
	}
	return result;
}

M0_INTERNAL void m0_sm_addb2_fini(struct m0_sm_conf *conf)
{
	if (conf->scf_addb2_key > 0)
		m0_locality_data_free(conf->scf_addb2_key - 1);

	conf->scf_addb2_id = 0;
	conf->scf_addb2_counter = 0;
	conf->scf_addb2_key = 0;
}

static void sm_addb2_counter_init_add(struct m0_sm *sm)
{
	const struct m0_sm_state_descr *sd = sm_state(sm);
	uint32_t state = sm->sm_state;
	uint32_t trans = sd->sd_trans[state];
	uint64_t as_id = sm->sm_addb2_stats->as_id;

	if (as_id != 0)
		M0_ADDB2_ADD(as_id, m0_sm_id_get(sm), trans, state);
}

M0_INTERNAL bool m0_sm_addb2_counter_init(struct m0_sm *sm)
{
	const struct m0_sm_conf *conf  = sm->sm_conf;

	if (conf->scf_addb2_key > 0) {
		sm->sm_addb2_stats = m0_locality_data(conf->scf_addb2_key - 1);
		sm_addb2_counter_init_add(sm);
	}
	return conf->scf_addb2_key > 0;
}

M0_INTERNAL void
m0_sm_ast_wait_init(struct m0_sm_ast_wait *wait, struct m0_mutex *ch_guard)
{
	wait->aw_allowed = true;
	m0_atomic64_set(&wait->aw_active, 0);
	m0_chan_init(&wait->aw_chan, ch_guard);
}

M0_INTERNAL void m0_sm_ast_wait_fini(struct m0_sm_ast_wait *wait)
{
	M0_PRE(m0_atomic64_get(&wait->aw_active) == 0);

	m0_chan_fini(&wait->aw_chan);
}

M0_INTERNAL void m0_sm_ast_wait_prepare(struct m0_sm_ast_wait *wait,
					struct m0_clink *clink)
{
	M0_PRE(m0_chan_is_locked(&wait->aw_chan));
	m0_clink_init(clink, NULL);
	/*
	 * Disable further ASTs. Since m0_sm_ast_wait_post() probes for
	 * wait->aw_allowed, it's necessary to ensure that if any AST
	 * gets posted during m0_sm_ast_wait_prepare(), it be waited
	 * for. Memory fencing here and usage of atomic instruction
	 * in m0_sm_ast_wait_prepare() ensures the right ordering of
	 * events that will never miss waiting for a posted AST.
	 */
	wait->aw_allowed = false;
	m0_mb();
	m0_clink_add(&wait->aw_chan, clink);
}

M0_INTERNAL void m0_sm_ast_wait_complete(struct m0_sm_ast_wait *wait,
					 struct m0_clink *clink)
{
	m0_clink_del(clink);
	m0_clink_fini(clink);
}

M0_INTERNAL void m0_sm_ast_wait_loop(struct m0_sm_ast_wait *wait,
				     struct m0_clink *clink)
{
	/* Wait until outstanding ASTs complete. */
	while (m0_atomic64_get(&wait->aw_active) > 0)
		m0_chan_wait(clink);
}

M0_INTERNAL void m0_sm_ast_wait(struct m0_sm_ast_wait *wait)
{
	struct m0_clink clink;

	m0_sm_ast_wait_prepare(wait, &clink);
	m0_chan_unlock(&wait->aw_chan);
	m0_sm_ast_wait_loop(wait, &clink);
	m0_chan_lock(&wait->aw_chan);
	m0_sm_ast_wait_complete(wait, &clink);
}

M0_INTERNAL void m0_sm_ast_wait_post(struct m0_sm_ast_wait *wait,
				     struct m0_sm_group *grp,
				     struct m0_sm_ast *ast)
{
	/*
	 * This function cannot take locks, because it can be called from an
	 * awkward context (timer call-back, etc.). Hence atomic is used and
	 * ordering of accesses is critical.
	 *
	 * m0_sm_ast_wait() sets wait->aw_allowed first and then checks
	 * wait->aw_active. Therefore, the opposite order should be used here to
	 * guarantee that m0_sm_ast_wait() does not return prematurely and
	 * misses an AST.
	 * Since atomic accesses and mutex operations are implicit memory
	 * barriers, explicit barriers are not needed around wait->aw_allowed
	 * accesses.
	 */
	m0_atomic64_inc(&wait->aw_active);
	if (wait->aw_allowed)
		m0_sm_ast_post(grp, ast);
	else
		m0_atomic64_dec(&wait->aw_active);
}

M0_INTERNAL void m0_sm_ast_wait_signal(struct m0_sm_ast_wait *wait)
{
	M0_PRE(m0_chan_is_locked(&wait->aw_chan));

	if (m0_atomic64_dec_and_test(&wait->aw_active))
		m0_chan_signal(&wait->aw_chan);
}

M0_INTERNAL void m0_sm_conf_print(const struct m0_sm_conf *conf)
{
	int i;

	m0_console_printf("digraph \"%s\" {\n", conf->scf_name);
	m0_console_printf("\t\"%s\" [shape=plaintext]\n\n", conf->scf_name);
	for (i = 0; i < conf->scf_nr_states; ++i) {
		const struct m0_sm_state_descr *sd = &conf->scf_state[i];

		if (!state_is_valid(conf, i))
			continue;
		m0_console_printf("\t\"%s\" [shape=%s]\n", sd->sd_name,
			(sd->sd_flags & M0_SDF_INITIAL) ? "circle" :
			(sd->sd_flags & M0_SDF_TERMINAL) ? "doublecircle" :
			(sd->sd_flags & M0_SDF_FAILURE) ? "cds" : "rect");
	}
	m0_console_printf("\n");
	for (i = 0; i < conf->scf_trans_nr; ++i) {
		const struct m0_sm_trans_descr *td = &conf->scf_trans[i];

		m0_console_printf("\t\"%s\" -> \"%s\" [label=\"%s\"]\n",
				  conf->scf_state[td->td_src].sd_name,
				  conf->scf_state[td->td_tgt].sd_name,
				  td->td_cause);
	}
	m0_console_printf("}\n\n");
}

M0_INTERNAL uint64_t m0_sm_id_get(const struct m0_sm *sm)
{
	return sm->sm_id;
}

/** @} end of sm group */
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
