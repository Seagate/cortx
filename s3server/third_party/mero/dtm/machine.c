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
 * Original creation date: 22-Jul-2013
 */

/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM

#include "lib/trace.h"
#include "lib/assert.h"

#include "dtm/dtm.h"
#include "dtm/remote.h"
#include "dtm/nucleus.h"
#include "dtm/history.h"
#include "dtm/dtm_internal.h"
#include "dtm/machine.h"

static void history_balance(struct m0_dtm_history *history);
static void history_excite(struct m0_dtm_history *history);
static void history_calm(struct m0_dtm_history *history);
static void undo_done(struct m0_dtm_update *update);
static void sibling_undo      (struct m0_dtm_history *history,
			       struct m0_dtm_op *op);
static void sibling_persistent(struct m0_dtm_history *history,
			       struct m0_dtm_op *op);
static void sibling_reset     (struct m0_dtm_history *history,
			       struct m0_dtm_op *op);

M0_INTERNAL void m0_dtm_balance(struct m0_dtm *dtm)
{
	while (!exc_tlist_is_empty(&dtm->d_excited)) {
		struct m0_dtm_history *history;

		m0_tl_for(exc, &dtm->d_excited, history) {
			history_balance(history);
		} m0_tl_endfor;
	}
}

M0_INTERNAL void m0_dtm_history_balance(struct m0_dtm_history *history)
{
	history_balance(history);
	m0_dtm_balance(HISTORY_DTM(history));
}

M0_INTERNAL void m0_dtm_undo_done(struct m0_dtm_update *update)
{
	undo_done(update);
	m0_dtm_history_balance(UPDATE_HISTORY(update));
}

static void history_balance(struct m0_dtm_history *history)
{
	bool                  more = false;
	struct m0_dtm_up     *up;
	struct m0_dtm_update *update;

	M0_PRE(m0_dtm_history_invariant(history));

	if (history->h_undo != NULL) {
		up = history_latest(history);
		update = up_update(up);
		M0_ASSERT(up != NULL);
		sibling_undo(history, up->up_op);
		if (update->upd_ops->updo_undo != NULL)
			update->upd_ops->updo_undo(update);
		else {
			undo_done(update);
			more = true;
		}
	}
	if (history->h_persistent != NULL) {
		up = UPDATE_UP(history->h_persistent);
		do {
			if (up->up_state >= M0_DOS_PERSISTENT)
				break;
			up->up_state = M0_DOS_PERSISTENT;
			sibling_persistent(history, up->up_op);
			up = m0_dtm_up_prior(up);
		} while (up != NULL);
		if (up_ver(up) != update_ver(history->h_persistent))
			history->h_ops->hio_persistent(history);
		if ((history->h_hi.hi_flags & M0_DHF_CLOSED) &&
		    m0_dtm_up_later(&history->h_persistent->upd_up) == NULL)
			history->h_ops->hio_fixed(history);
		history->h_persistent = NULL;
	}
	if (history->h_reset != NULL) {
		up = UPDATE_UP(history->h_reset);
		do {
			if (up->up_state < M0_DOS_INPROGRESS)
				break;
			up->up_state = M0_DOS_INPROGRESS;
			sibling_reset(history, up->up_op);
			up = m0_dtm_up_later(up);
		} while (up != NULL);
		history->h_known =
			up_update(m0_dtm_up_prior(UPDATE_UP(history->h_reset)));
		history->h_reint = history->h_reset;
		history->h_reset = NULL;
		more = true;
	}
	if (history->h_reint != NULL) {
		up = UPDATE_UP(history->h_reint);
		do {
			if (up->up_state < M0_DOS_INPROGRESS)
				break;
			update_reint(up_update(up));
			up = m0_dtm_up_later(up);
		} while (up != NULL);
		history->h_reint = NULL;
	}
	more ? history_excite(history) : history_calm(history);
	M0_POST(m0_dtm_history_invariant(history));
}

static void sibling_undo(struct m0_dtm_history *history, struct m0_dtm_op *op)
{
	up_for(op, up) {
		struct m0_dtm_history *other  = UP_HISTORY(up);
		struct m0_dtm_update  *update = up_update(up);

		M0_ASSERT(up == hi_latest(up->up_hi));
		M0_ASSERT(up->up_state >= M0_DOS_VOLATILE);

		if (other != history && other->h_epoch < history->h_epoch &&
		    (other->h_undo == NULL || update_is_earlier(update,
							   other->h_undo))) {
			other->h_undo = update;
			other->h_epoch = history->h_epoch;
			history_excite(other);
		}
	} up_endfor;
}

static void sibling_persistent(struct m0_dtm_history *history,
			       struct m0_dtm_op *op)
{
	up_for(op, up) {
		struct m0_dtm_history *other  = UP_HISTORY(up);
		struct m0_dtm_update  *update = up_update(up);

		if (up->up_state < M0_DOS_PERSISTENT && other != history &&
		    other->h_rem == history->h_rem &&
		    (other->h_persistent == NULL ||
		     update_is_earlier(other->h_persistent, update))) {
			other->h_persistent = update;
			history_excite(other);
		}
	} up_endfor;
}

static void sibling_reset(struct m0_dtm_history *history, struct m0_dtm_op *op)
{
	up_for(op, up) {
		struct m0_dtm_history *other  = UP_HISTORY(up);
		struct m0_dtm_update  *update = up_update(up);

		if (other != history && other->h_rem == history->h_rem &&
		    other->h_epoch < history->h_epoch &&
		    (other->h_reset == NULL ||
		     update_is_earlier(update, other->h_reset))) {
			other->h_reset = update;
			other->h_epoch = history->h_epoch;
			history_excite(other);
		}
	} up_endfor;
}

static void undo_done(struct m0_dtm_update *update)
{
	struct m0_dtm_up      *up      = UPDATE_UP(update);
	struct m0_dtm_history *history = UP_HISTORY(up);
	struct m0_dtm_up      *prior   = m0_dtm_up_prior(up);
	m0_dtm_ver_t           pver    = up_ver(prior);

	M0_PRE(m0_dtm_history_invariant(history));
	M0_PRE(up == history_latest(history));
	M0_ASSERT(history->h_persistent == NULL);
	M0_ASSERT(history->h_reint == NULL);
	M0_ASSERT(history->h_known == NULL);
	M0_ASSERT(history->h_reset == NULL);

	if (history->h_max_ver == up->up_ver)
		history->h_max_ver = pver;
	if (history->h_hi.hi_ver == up->up_ver)
		history->h_hi.hi_ver = pver;
	if (history->h_undo == update)
		history->h_undo = NULL;

	M0_POST(m0_dtm_history_invariant(history));
}

static void dtm_excitement_start(struct m0_dtm *dtm)
{
}

static void history_excite(struct m0_dtm_history *history)
{
	struct m0_dtm *dtm     = HISTORY_DTM(history);
	struct m0_tl  *excited = &dtm->d_excited;

	if (!exc_tlink_is_in(history)) {
		bool empty = exc_tlist_is_empty(excited);

		exc_tlist_add_tail(excited, history);
		if (empty)
			dtm_excitement_start(dtm);
	}
}

static void history_calm(struct m0_dtm_history *history)
{
	exc_tlist_remove(history);
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
