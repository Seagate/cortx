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
 * Original creation date: 22-Jan-2013
 */

/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM

#include "lib/bob.h"
#include "lib/misc.h"           /* M0_IN */
#include "lib/arith.h"          /* min_check, max_check */
#include "lib/tlist.h"
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/trace.h"

#include "mero/magic.h"
#include "dtm/nucleus.h"
#include "dtm/dtm_internal.h"

M0_TL_DESCR_DEFINE(hi, "nucleus hi updates", M0_INTERNAL, struct m0_dtm_up,
		   up_hi_linkage, up_magix,
		   M0_DTM_UP_MAGIX, M0_DTM_HI_MAGIX);
M0_TL_DEFINE(hi, M0_INTERNAL, struct m0_dtm_up);

M0_TL_DESCR_DEFINE(op, "nucleus operation updates", M0_INTERNAL, struct m0_dtm_up,
		   up_op_linkage, up_magix,
		   M0_DTM_UP_MAGIX, M0_DTM_OP_MAGIX);
M0_TL_DEFINE(op, M0_INTERNAL, struct m0_dtm_up);

const static struct m0_bob_type hi_bob;
const static struct m0_bob_type op_bob;
static struct m0_bob_type up_bob;

M0_BOB_DEFINE(static, &hi_bob, m0_dtm_hi);
M0_BOB_DEFINE(static, &up_bob, m0_dtm_up);
M0_BOB_DEFINE(static, &op_bob, m0_dtm_op);

static void advance_hi(struct m0_dtm_hi *hi);
static void advance_op(struct m0_dtm_op *op);
static void up_ready  (struct m0_dtm_up *up);
static void up_insert (struct m0_dtm_up *up);
static int  op_cmp    (const struct m0_dtm_op *op);
static void up_del    (struct m0_dtm_up *up);
static int  up_cmp    (const struct m0_dtm_up *up, m0_dtm_ver_t hver);
static void up_fini   (struct m0_dtm_up *up);

enum m0_dtm_ver_cmp {
	LATE  = -1,
	READY =  0,
	EARLY = +1,
	MISER
};

M0_INTERNAL void m0_dtm_op_init(struct m0_dtm_op *op, struct m0_dtm_nu *nu)
{
	m0_dtm_op_bob_init(op);
	op->op_nu    = nu;
	op_tlist_init(&op->op_ups);
}

M0_INTERNAL void m0_dtm_op_close(const struct m0_dtm_op *op)
{
	M0_PRE(m0_dtm_op_invariant(op));

	up_for(op, up) {
		M0_PRE(up->up_state == M0_DOS_LIMBO);
		hi_tlist_add(&up->up_hi->hi_ups, up);
		up->up_state = M0_DOS_FUTURE;
	} up_endfor;
	advance_try(op);
	M0_POST(m0_dtm_op_invariant(op));
}

M0_INTERNAL void m0_dtm_op_prepared(const struct m0_dtm_op *op)
{
	M0_PRE(m0_dtm_op_invariant(op));

	up_for(op, up) {
		up_prepared(up);
	} up_endfor;
	advance_try(op);
	M0_POST(m0_dtm_op_invariant(op));
}

M0_INTERNAL void m0_dtm_op_done(const struct m0_dtm_op *op)
{
	M0_PRE(m0_dtm_op_invariant(op));

	up_for(op, up) {
		M0_PRE(up->up_state == M0_DOS_INPROGRESS);
		up->up_state = M0_DOS_VOLATILE;
	} up_endfor;
	M0_POST(m0_dtm_op_invariant(op));
}

static void op_del(struct m0_dtm_op *op)
{
	up_for(op, up) {
		M0_PRE(M0_IN(up->up_state, (M0_DOS_FUTURE, M0_DOS_LIMBO)));
		up_del(up);
		up->up_state = M0_DOS_LIMBO;
	} up_endfor;
}

M0_INTERNAL void m0_dtm_op_del(struct m0_dtm_op *op)
{
	M0_PRE(m0_dtm_op_invariant(op));
	op_del(op);
	M0_POST(m0_dtm_op_invariant(op));
}

M0_INTERNAL void m0_dtm_op_fini(struct m0_dtm_op *op)
{
	M0_PRE(m0_dtm_op_invariant(op));
	up_for(op, up) {
		up_fini(up);
	} up_endfor;
	op_tlist_fini(&op->op_ups);
	m0_dtm_op_bob_fini(op);
}

M0_INTERNAL void advance_try(const struct m0_dtm_op *op)
{
	up_for(op, up) {
		advance_hi(up->up_hi);
	} up_endfor;
}

static void advance_hi(struct m0_dtm_hi *hi)
{
	struct m0_dtm_up *last = NULL;
	hi_for(hi, up) {
		if (up->up_state > M0_DOS_FUTURE)
			break;
		last = up;
	} hi_endfor;
	while (last != NULL) {
		/* because advance_op() can call op_del(). */
		struct m0_dtm_up *prev = hi_tlist_prev(&hi->hi_ups, last);

		M0_ASSERT(last->up_state == M0_DOS_FUTURE);
		advance_op(last->up_op);
		last = prev;
	}
}

static void advance_op(struct m0_dtm_op *op)
{
	const struct m0_dtm_op_ops *op_ops = op->op_ops;

	M0_PRE(op_state(op, M0_DOS_FUTURE));

	if (m0_tl_forall(op, up, &op->op_ups,
			 !(up->up_hi->hi_flags & M0_DHF_BUSY))) {
		switch (op_cmp(op)) {
		case READY:
			up_for(op, up) {
				up_ready(up);
				up->up_state = M0_DOS_PREPARE;
				up->up_hi->hi_flags |= M0_DHF_BUSY;
			} up_endfor;
			op_ops->doo_ready(op);
			break;
		case EARLY:
			break;
		case LATE:
			op_del(op);
			op_ops->doo_late(op);
			break;
		case MISER:
			op_del(op);
			op_ops->doo_miser(op);
			break;
		default:
			M0_IMPOSSIBLE("Wrong op_cmp().");
		}
	}
}

static void up_ready(struct m0_dtm_up *up)
{
	struct m0_dtm_hi *hi = up->up_hi;

	if (up->up_hi->hi_flags & M0_DHF_OWNED) {
		if (up->up_orig_ver == 0)
			up->up_orig_ver = hi->hi_ver;
		M0_ASSERT(up->up_orig_ver == hi->hi_ver);
		if (up->up_ver == 0) {
			if (up->up_rule == M0_DUR_NOT)
				up->up_ver = up->up_orig_ver;
			else
				up->up_ver = up->up_orig_ver + 1;
		}
	}
	hi_tlist_del(up);
	up_insert(up);
}

M0_INTERNAL void up_prepared(struct m0_dtm_up *up)
{
	struct m0_dtm_hi *hi = up->up_hi;

	M0_PRE(up->up_state == M0_DOS_PREPARE);
	M0_ASSERT(hi->hi_flags & M0_DHF_BUSY);
	M0_ASSERT(ergo(hi->hi_flags & M0_DHF_OWNED, up->up_ver != 0));
	if (up->up_ver != 0)
		hi->hi_ver = up->up_ver;
	hi->hi_flags &= ~M0_DHF_BUSY;
	up->up_state = M0_DOS_INPROGRESS;
}

static int op_cmp(const struct m0_dtm_op *op)
{
	enum m0_dtm_ver_cmp min = EARLY;
	enum m0_dtm_ver_cmp max = LATE;

	/*
	 * Invariants can be violated at the entry: operation can be late,
	 * misordered, etc.
	 */

	/*
	 * max  ^
	 *      |
	 * EARLY|MISER EARLY EARLY
	 * READY|MISER READY   X
	 * LATE |LATE    X     X
	 * -----+-----+-----+--------> min
	 *       LATE  READY EARLY
	 */
	up_for(op, up) {
		enum m0_dtm_ver_cmp check;

		check = up_cmp(up, up->up_hi->hi_ver);
		min = min_check(min, check);
		max = max_check(max, check);
	} up_endfor;
	return
		min == LATE  && max == LATE  ? LATE  :
		min == READY && max == READY ? READY :
		min >= READY && max == EARLY ? EARLY : MISER;
}


M0_INTERNAL void m0_dtm_hi_init(struct m0_dtm_hi *hi, struct m0_dtm_nu *nu)
{
	M0_SET0(hi);
	m0_dtm_hi_bob_init(hi);
	hi->hi_nu  = nu;
	hi->hi_ver = 1;
	hi_tlist_init(&hi->hi_ups);
}

M0_INTERNAL void m0_dtm_hi_fini(struct m0_dtm_hi *hi)
{
	hi_for(hi, up) {
		up_fini(up);
	} hi_endfor;
	hi_tlist_fini(&hi->hi_ups);
	m0_dtm_hi_bob_fini(hi);
}

M0_INTERNAL void m0_dtm_up_init(struct m0_dtm_up *up, struct m0_dtm_hi *hi,
				struct m0_dtm_op *op, enum m0_dtm_up_rule rule,
				m0_dtm_ver_t ver, m0_dtm_ver_t orig_ver)
{
	M0_PRE(m0_dtm_op_invariant(op));
	M0_PRE(m0_dtm_hi_invariant(hi));
	M0_PRE(op_state(op, M0_DOS_LIMBO));

	m0_dtm_up_bob_init(up);
	op_tlink_init_at(up, &op->op_ups);
	hi_tlink_init(up);
	up->up_hi       = hi;
	up->up_op       = op;
	up->up_rule     = rule;
	up->up_ver      = ver;
	up->up_orig_ver = orig_ver;
	M0_POST(m0_dtm_up_invariant(up));
	M0_POST(m0_dtm_op_invariant(op));
	M0_POST(m0_dtm_hi_invariant(hi));
}

static void up_fini(struct m0_dtm_up *up)
{
	if (hi_tlink_is_in(up))
		hi_tlink_del_fini(up);
	op_tlink_del_fini(up);
	m0_dtm_up_bob_fini(up);
}

static void up_insert(struct m0_dtm_up *up)
{
	struct m0_dtm_hi *hi = up->up_hi;

	hi_for(hi, scan) {
		if (scan->up_state > M0_DOS_FUTURE) {
			hi_tlist_add_before(scan, up);
			return;
		}
	} hi_endfor;
	hi_tlist_add_tail(&hi->hi_ups, up);
}

static void up_del(struct m0_dtm_up *up)
{
	M0_PRE(m0_dtm_up_invariant(up));
	M0_PRE(up->up_state < M0_DOS_STABLE);

	if (hi_tlink_is_in(up))
		hi_tlink_del_fini(up);
}

static int up_cmp(const struct m0_dtm_up *up, m0_dtm_ver_t hver)
{
	m0_dtm_ver_t uver = up->up_ver;
	/*
	 * Possible invariant violations on entry: the update is not in the
	 * history, even when the operation is not in LIMBO.
	 */
	M0_PRE(hver != 0);

	if (uver == 0)
		return READY;
	if (up->up_orig_ver != 0)
		return M0_3WAY(up->up_orig_ver, hver);

	switch (up->up_rule) {
	case M0_DUR_INC:
		return M0_3WAY(uver, hver + 1);
	case M0_DUR_SET:
		return uver <= hver ? LATE : READY;
	case M0_DUR_NOT:
		return M0_3WAY(uver, hver);
	case M0_DUR_APP:
	default:
		M0_IMPOSSIBLE("Impossible rule.");
		return 0;
	}
}

M0_INTERNAL void m0_dtm_nu_init(struct m0_dtm_nu *nu)
{
	m0_mutex_init(&nu->nu_lock);
}

M0_INTERNAL void m0_dtm_nu_fini(struct m0_dtm_nu *nu)
{
	m0_mutex_fini(&nu->nu_lock);
}

M0_INTERNAL struct m0_dtm_up *m0_dtm_up_prior(struct m0_dtm_up *up)
{
	return hi_tlist_next(&up->up_hi->hi_ups, up);
}

M0_INTERNAL struct m0_dtm_up *m0_dtm_up_later(struct m0_dtm_up *up)
{
	return hi_tlist_prev(&up->up_hi->hi_ups, up);
}

M0_INTERNAL struct m0_dtm_up *hi_latest(struct m0_dtm_hi *hi)
{
	return hi_tlist_head(&hi->hi_ups);
}

M0_INTERNAL struct m0_dtm_up *hi_earliest(struct m0_dtm_hi *hi)
{
	return hi_tlist_tail(&hi->hi_ups);
}


M0_INTERNAL struct m0_dtm_up *hi_find(struct m0_dtm_hi *hi, m0_dtm_ver_t ver)
{
	hi_for(hi, up) {
		if (up->up_ver <= ver)
			return up;
	} hi_endfor;
	return NULL;
}

M0_INTERNAL m0_dtm_ver_t up_ver(const struct m0_dtm_up *up)
{
	return up != NULL ? up->up_ver : 0;
}

M0_INTERNAL bool up_is_earlier(struct m0_dtm_up *up0, struct m0_dtm_up *up1)
{
	M0_PRE(up0 != NULL);
	M0_PRE(up1 != NULL);
	M0_PRE(up0->up_hi == up1->up_hi);

	if (up0->up_ver < up1->up_ver)
		return true;
	while (1) {
		up0 = m0_dtm_up_later(up0);
		if (up0 == NULL)
			return false;
		if (up1 == up0)
			return true;
	}
}


static bool op_is_locked(const struct m0_dtm_op *op)
{
	return m0_mutex_is_locked(&op->op_nu->nu_lock);
}

static bool up_pair_invariant(const struct m0_dtm_up *up,
			      const struct m0_dtm_up *next);

M0_INTERNAL bool m0_dtm_hi_invariant(const struct m0_dtm_hi *hi)
{
	unsigned prepare = 0;
	return
		_0C(m0_dtm_hi_bob_check(hi)) &&
		m0_tl_forall(hi, up, &hi->hi_ups,
		      m0_dtm_up_invariant(up) &&
		      up_pair_invariant(up, hi_tlist_next(&hi->hi_ups, up)) &&
		      ergo(up->up_state == M0_DOS_PREPARE, ++prepare + 1)) &&
		prepare == !!(hi->hi_flags & M0_DHF_BUSY);
}

M0_INTERNAL bool m0_dtm_up_invariant(const struct m0_dtm_up *up)
{
	return
		_0C(m0_dtm_up_bob_check(up)) &&
		_0C(0 <= up->up_state && up->up_state < M0_DOS_NR) &&
		_0C(0 <= up->up_rule && up->up_rule < M0_DUR_NR) &&
		_0C(up->up_hi != NULL && up->up_op != NULL) &&
		_0C(up->up_ver >= up->up_orig_ver) &&
		_0C(ergo(up->up_ver == up->up_orig_ver,
			 up->up_ver == 0 || up->up_rule == M0_DUR_NOT)) &&
		_0C(ergo(up->up_ver != 0,
			 ergo(up->up_rule == M0_DUR_INC,
			      up->up_ver == up->up_orig_ver + 1))) &&
		_0C(ergo(up->up_rule == M0_DUR_NOT,
			 up->up_ver == up->up_orig_ver)) &&
		_0C(ergo(up->up_state > M0_DOS_INPROGRESS,
			 up->up_orig_ver != 0)) &&
		_0C(hi_tlist_contains(&up->up_hi->hi_ups, up) ==
					(up->up_state != M0_DOS_LIMBO)) &&
		_0C(op_tlist_contains(&up->up_op->op_ups, up));
}

M0_INTERNAL bool m0_dtm_op_invariant(const struct m0_dtm_op *op)
{
	return
		_0C(op_is_locked(op)) &&
		_0C(m0_dtm_op_bob_check(op)) &&
		m0_tl_forall(op, up, &op->op_ups,
			     m0_dtm_up_invariant(up) && _0C(up->up_op == op) &&
			     _0C(up->up_hi->hi_nu == op->op_nu) &&
			     m0_dtm_hi_invariant(up->up_hi)) &&
		_0C(({
			enum m0_dtm_state min = M0_DOS_NR;
			enum m0_dtm_state max = 0;

			up_for(op, up) {
				min = min_check(min, up->up_state);
				max = max_check(max, up->up_state);
			} up_endfor;
			op_tlist_is_empty(&op->op_ups) ||
			   (min == max) || ((min >= M0_DOS_PREPARE) &&
					    (max < M0_DOS_STABLE));
		   })) &&
		_0C(ergo(!op_state(op, M0_DOS_LIMBO),
			 !m0_tl_forall(op, up, &op->op_ups,
				       up->up_rule == M0_DUR_NOT ||
				       up->up_ver == 0)));
}

static bool up_pair_invariant(const struct m0_dtm_up *up,
			      const struct m0_dtm_up *earlier)
{
	return earlier == NULL || up->up_state < M0_DOS_PREPARE ||
		(_0C(up->up_state <= earlier->up_state) &&
		 _0C(ergo((up->up_hi->hi_flags & M0_DHF_OWNED) ||
			  up->up_state > M0_DOS_INPROGRESS,
			  up->up_orig_ver != 0 &&
			  up->up_ver != 0 &&
			  up->up_orig_ver >= earlier->up_ver)) &&
		 _0C(ergo(up->up_hi->hi_flags & M0_DHF_FULL,
			  up->up_orig_ver == earlier->up_ver)));
}

M0_INTERNAL bool op_state(const struct m0_dtm_op *op, enum m0_dtm_state state)
{
	return m0_tl_forall(op, up, &op->op_ups, up->up_state == state);
}

const static struct m0_bob_type hi_bob = {
	.bt_name         = "dtm history",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_dtm_hi, hi_ups.t_magic),
	.bt_magix        = M0_DTM_HI_MAGIX
};

const static struct m0_bob_type op_bob = {
	.bt_name         = "dtm operation",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_dtm_op, op_ups.t_magic),
	.bt_magix        = M0_DTM_OP_MAGIX
};

M0_INTERNAL void m0_dtm_nuclei_init(void)
{
	M0_SET0(&up_bob);
	m0_bob_type_tlist_init(&up_bob, &hi_tl);
}

M0_INTERNAL void m0_dtm_nuclei_fini(void)
{
}

M0_INTERNAL void nu_lock(struct m0_dtm_nu *nu)
{
	m0_mutex_lock(&nu->nu_lock);
}

M0_INTERNAL void nu_unlock(struct m0_dtm_nu *nu)
{
	m0_mutex_unlock(&nu->nu_lock);
}

M0_INTERNAL void up_print(const struct m0_dtm_up *up)
{
	M0_LOG(M0_FATAL, "\tup: s: %3.3i r: %3.3i v: %7.7lu o: %7.7lu",
	       up->up_state, up->up_rule, (unsigned long)up->up_ver,
	       (unsigned long)up->up_orig_ver);
}

M0_INTERNAL void op_print(const struct m0_dtm_op *op)
{
	M0_LOG(M0_FATAL, "op");
	up_for(op, up) {
		up_print(up);
	} up_endfor;
}

M0_INTERNAL void hi_print(const struct m0_dtm_hi *hi)
{
	M0_LOG(M0_FATAL, "hi: f: %3.3lx v: %7.7lu", (unsigned long)hi->hi_flags,
	       (unsigned long)hi->hi_ver);
	hi_for(hi, up) {
		up_print(up);
	} hi_endfor;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm group */

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
