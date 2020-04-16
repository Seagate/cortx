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
 * Original creation date: 27-Jan-2013
 */


/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM

#include "lib/trace.h"
#include "lib/misc.h"               /* M0_IN */
#include "lib/errno.h"
#include "lib/memory.h"

#include "dtm/dtm_internal.h"
#include "dtm/history.h"
#include "dtm/update.h"
#include "dtm/remote.h"
#include "dtm/operation.h"
#include "dtm/dtm.h"

M0_INTERNAL void m0_dtm_oper_init(struct m0_dtm_oper *oper, struct m0_dtm *dtm,
				  struct m0_tl *uu)
{
	struct m0_dtm_update *update;

	m0_dtm_op_init(&oper->oprt_op, &dtm->d_nu);
	m0_dtm_update_list_init(&oper->oprt_uu);
	if (uu != NULL) {
		m0_tl_for(oper, uu, update) {
			oper_tlist_move_tail(&oper->oprt_uu, update);
		} m0_tl_endfor;
	}
}

M0_INTERNAL void m0_dtm_oper_fini(struct m0_dtm_oper *oper)
{
	struct m0_dtm *dtm = nu_dtm(oper->oprt_op.op_nu);

	m0_dtm_update_list_fini(&oper->oprt_uu);
	dtm_lock(dtm);
	M0_PRE(m0_dtm_oper_invariant(oper));
	m0_dtm_op_fini(&oper->oprt_op);
	dtm_unlock(dtm);
}

M0_INTERNAL bool m0_dtm_oper_invariant(const struct m0_dtm_oper *oper)
{
	const struct m0_dtm_op *op = &oper->oprt_op;
	return
		m0_dtm_op_invariant(op) &&
		m0_tl_forall(oper, u0, &op->op_ups,
		   m0_tl_forall(oper, u1, &op->op_ups,
			ergo(u0->upd_label == u1->upd_label,
			     u0 == u1 || (!(oper->oprt_flags & M0_DOF_CLOSED) &&
					  u0->upd_label == 0)))) &&
		(oper->oprt_flags & M0_DOF_CLOSED) ==
		!op_state(op, M0_DOS_LIMBO);
}

M0_INTERNAL void m0_dtm_oper_close(struct m0_dtm_oper *oper)
{
	uint32_t max_label = 0;

	oper_for(oper, update) {
		struct m0_dtm_history *history;

		history = UPDATE_HISTORY(update);
		if (history->h_rem != NULL &&
		    oper_update_unique(oper, update)) {
			m0_dtm_remote_add(history->h_rem, oper, history, update);
		}
	} oper_endfor;
	oper_lock(oper);
	M0_PRE(!(oper->oprt_flags & M0_DOF_CLOSED));
	M0_PRE(m0_dtm_oper_invariant(oper));
	oper_for(oper, update) {
		if (update->upd_label < M0_DTM_USER_UPDATE_BASE) {
			max_label = max32u(max_label, update->upd_label);
			if (m0_dtm_controlh_update_is_close(update))
				history_close(UPDATE_HISTORY(update));
		}
	} oper_endfor;
	oper_for(oper, update) {
		struct m0_dtm_history *history;

		history = UPDATE_HISTORY(update);
		if (update->upd_label == 0)
			update->upd_label = ++max_label;
		M0_ASSERT(max_label < M0_DTM_USER_UPDATE_BASE);
		history->h_max_ver = max64u(history->h_max_ver,
					    update->upd_up.up_ver);
	} oper_endfor;
	m0_dtm_op_close(&oper->oprt_op);
	oper->oprt_flags |= M0_DOF_CLOSED;
	M0_PRE(m0_dtm_oper_invariant(oper));
	oper_unlock(oper);
}

M0_INTERNAL void m0_dtm_oper_prepared(const struct m0_dtm_oper *oper,
				      const struct m0_dtm_remote *rem)
{
	M0_PRE(oper->oprt_flags & M0_DOF_CLOSED);
	oper_lock(oper);
	M0_PRE(m0_dtm_oper_invariant(oper));
	up_for(&oper->oprt_op, up) {
		struct m0_dtm_history *history;

		history = UP_HISTORY(up);
		M0_PRE(up->up_state >= M0_DOS_PREPARE);
		if (history->h_rem == rem) {
			up_prepared(up);
			history->h_max_ver = max64u(history->h_max_ver,
						    up->up_ver);
		}
	} up_endfor;
	advance_try(&oper->oprt_op);
	M0_POST(m0_dtm_oper_invariant(oper));
	oper_unlock(oper);
}

M0_INTERNAL void m0_dtm_oper_done(const struct m0_dtm_oper *oper,
				  const struct m0_dtm_remote *rem)
{
	M0_PRE(oper->oprt_flags & M0_DOF_CLOSED);
	oper_lock(oper);
	M0_PRE(m0_dtm_oper_invariant(oper));
	up_for(&oper->oprt_op, up) {
		M0_PRE(up->up_state >= M0_DOS_PREPARE);
		if (UP_HISTORY(up)->h_rem == rem) {
			M0_PRE(up->up_state == M0_DOS_INPROGRESS);
			up->up_state = M0_DOS_VOLATILE;
		}
	} up_endfor;
	M0_POST(m0_dtm_oper_invariant(oper));
	oper_unlock(oper);
}

M0_INTERNAL void m0_dtm_oper_pack(struct m0_dtm_oper *oper,
				  const struct m0_dtm_remote *rem,
				  struct m0_dtm_oper_descr *ode)
{
	uint32_t idx = 0;

	M0_PRE(oper->oprt_flags & M0_DOF_CLOSED);
	oper_lock(oper);
	M0_PRE(m0_dtm_oper_invariant(oper));
	oper_for(oper, update) {
		M0_ASSERT(HISTORY_DTM(&rem->re_fol.rfo_ch.ch_history) ==
			  HISTORY_DTM(UPDATE_HISTORY(update)));
		if (UPDATE_REM(update) == rem) {
			M0_ASSERT(idx < ode->od_updates.ou_nr);
			m0_dtm_update_pack(update,
					   &ode->od_updates.ou_update[idx++]);
		}
	} oper_endfor;
	ode->od_updates.ou_nr = idx;
	oper_unlock(oper);
}

M0_INTERNAL int m0_dtm_oper_build(struct m0_dtm_oper *oper, struct m0_tl *uu,
				  const struct m0_dtm_oper_descr *ode)
{
	uint32_t              i;
	int                   result;

	M0_PRE(!(oper->oprt_flags & M0_DOF_CLOSED));
	oper_lock(oper);
	M0_PRE(m0_dtm_oper_invariant(oper));
	for (result = 0, i = 0; i < ode->od_updates.ou_nr; ++i) {
		struct m0_dtm_update_descr *ud = &ode->od_updates.ou_update[i];
		struct m0_dtm_update       *update;

		update = oper_tlist_pop(uu);
		M0_ASSERT(update != NULL);
		result = m0_dtm_update_build(update, oper, ud);
		if (result != 0)
			break;
	}
	M0_POST(ergo(result == 0, m0_dtm_oper_invariant(oper)));
	oper_unlock(oper);
	if (result != 0)
		m0_dtm_oper_fini(oper);
	return result;
}

M0_INTERNAL void m0_dtm_reply_pack(const struct m0_dtm_oper *oper,
				   const struct m0_dtm_oper_descr *request,
				   struct m0_dtm_oper_descr *reply)
{
	uint32_t i;
	uint32_t j;

	M0_PRE(oper->oprt_flags & M0_DOF_CLOSED);
	oper_lock(oper);
	M0_PRE(m0_dtm_oper_invariant(oper));
	for (j = 0, i = 0; i < request->od_updates.ou_nr; ++i) {
		struct m0_dtm_update_descr *ud;
		const struct m0_dtm_update *update;

		ud = &request->od_updates.ou_update[i];
		update = m0_dtm_oper_get(oper, ud->udd_data.da_label);
		M0_ASSERT(update != NULL);
		M0_ASSERT(update->upd_up.up_state >= M0_DOS_VOLATILE);
		M0_ASSERT(m0_dtm_descr_matches_update(update, ud));
		M0_ASSERT(j < reply->od_updates.ou_nr);
		m0_dtm_update_pack(update, &reply->od_updates.ou_update[j++]);
	}
	reply->od_updates.ou_nr = j;
	M0_POST(m0_dtm_oper_invariant(oper));
	oper_unlock(oper);
}

M0_INTERNAL void m0_dtm_reply_unpack(struct m0_dtm_oper *oper,
				     const struct m0_dtm_oper_descr *reply)
{
	uint32_t i;

	M0_PRE(oper->oprt_flags & M0_DOF_CLOSED);
	oper_lock(oper);
	M0_PRE(m0_dtm_oper_invariant(oper));
	for (i = 0; i < reply->od_updates.ou_nr; ++i) {
		struct m0_dtm_update_descr *ud;
		struct m0_dtm_update       *update;

		ud = &reply->od_updates.ou_update[i];
		update = m0_dtm_oper_get(oper, ud->udd_data.da_label);
		M0_ASSERT(update != NULL); /* -EPROTO */
		M0_PRE(M0_IN(update->upd_up.up_state, (M0_DOS_INPROGRESS,
						       M0_DOS_PREPARE)));
		m0_dtm_update_unpack(update, ud);
	}
	M0_POST(m0_dtm_oper_invariant(oper));
	oper_unlock(oper);
}

M0_INTERNAL struct m0_dtm_update *m0_dtm_oper_get(const struct m0_dtm_oper *oper,
						  uint32_t label)
{
	M0_PRE(m0_dtm_oper_invariant(oper));
	oper_for(oper, update) {
		if (update->upd_label == label)
			return update;
	} oper_endfor;
	return NULL;
}

M0_INTERNAL bool oper_update_unique(const struct m0_dtm_oper *oper,
				    const struct m0_dtm_update *update)
{
	oper_for(oper, scan) {
		if (scan == update)
			return true;
		if (UPDATE_REM(scan) == UPDATE_REM(update))
			return false;
	} oper_endfor;
	M0_IMPOSSIBLE("Missing update.");
	return false;
}

M0_INTERNAL void oper_lock(const struct m0_dtm_oper *oper)
{
	nu_lock(oper->oprt_op.op_nu);
}

M0_INTERNAL void oper_unlock(const struct m0_dtm_oper *oper)
{
	nu_unlock(oper->oprt_op.op_nu);
}

M0_INTERNAL void oper_print(const struct m0_dtm_oper *oper)
{
	M0_LOG(M0_FATAL, "oper flags: %lx uu: %u",
	       (unsigned long)oper->oprt_flags,
	       (unsigned)oper_tlist_length(&oper->oprt_uu));
	oper_for(oper, update) {
		update_print(update);
	} oper_endfor;
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
