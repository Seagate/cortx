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
 * Original creation date: 01-Feb-2013
 */


/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM

#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/errno.h"                  /* EPROTO */
#include "lib/misc.h"                   /* M0_IN */
#include "mero/magic.h"

#include "fop/fop.h"

#include "dtm/dtm_internal.h"
#include "dtm/nucleus.h"
#include "dtm/operation.h"
#include "dtm/history.h"
#include "dtm/remote.h"
#include "dtm/update.h"

M0_INTERNAL void m0_dtm_update_init(struct m0_dtm_update *update,
				    struct m0_dtm_history *history,
				    struct m0_dtm_oper *oper,
				    const struct m0_dtm_update_data *data)
{
	M0_PRE(ergo(data->da_label != 0,
		    m0_tl_forall(oper, upd, &oper->oprt_op.op_ups,
				 upd->upd_up.up_state == M0_DOS_LIMBO &&
				 upd->upd_label != data->da_label)));
	M0_PRE(!(history->h_hi.hi_flags & M0_DHF_CLOSED));
	M0_PRE(m0_dtm_oper_invariant(oper));
	m0_dtm_up_init(&update->upd_up, &history->h_hi, &oper->oprt_op,
		       data->da_rule, data->da_ver, data->da_orig_ver);
	update->upd_label = data->da_label;
	M0_PRE(m0_dtm_oper_invariant(oper));
	M0_POST(m0_dtm_update_invariant(update));
}

M0_INTERNAL bool m0_dtm_update_invariant(const struct m0_dtm_update *update)
{
	const struct m0_dtm_up          *up     = &update->upd_up;
	enum m0_dtm_state                state  = up->up_state;
	const struct m0_dtm_update_comm *comm   = &update->upd_comm;
	enum m0_dtm_update_comm_state    cstate = comm->uc_state;
	const struct m0_fop             *body   = comm->uc_body;
	struct m0_dtm_remote            *rem    = UPDATE_REM(update);

	return
		m0_dtm_up_invariant(up) &&
		_0C(M0_IN(cstate, (M0_DUX_NEW,
				   M0_DUX_INFLIGHT, M0_DUX_REPLIED))) &&
		_0C(ergo(rem == NULL, body == NULL)) &&
		_0C(ergo(body == NULL, cstate == M0_DUX_NEW)) &&
		_0C(ergo(state > M0_DOS_INPROGRESS,
			 body == NULL || cstate > M0_DUX_NEW)) &&
		_0C(ergo(cstate == M0_DUX_REPLIED,
			 body->f_item.ri_reply != NULL)) &&
		_0C(ergo(rem != NULL, comm->uc_instance <= rem->re_instance));
}

M0_INTERNAL bool m0_dtm_update_is_user(const struct m0_dtm_update *update)
{
	M0_PRE(m0_dtm_update_invariant(update));
	return update->upd_label >= M0_DTM_USER_UPDATE_BASE;
}

M0_INTERNAL void m0_dtm_update_pack(const struct m0_dtm_update *update,
				    struct m0_dtm_update_descr *updd)
{
	const struct m0_dtm_up *up      = &update->upd_up;
	struct m0_dtm_history  *history = UPDATE_HISTORY(update);

	M0_PRE(m0_dtm_update_invariant(update));
	M0_PRE(update->upd_up.up_state >= M0_DOS_FUTURE);
	*updd = (struct m0_dtm_update_descr) {
		.udd_utype = update->upd_ops->updo_type->updtt_id,
		.udd_data  = {
			.da_label    = update->upd_label,
			.da_rule     = up->up_rule,
			.da_ver      = up->up_ver,
			.da_orig_ver = up->up_orig_ver
		}
	};
	m0_dtm_history_pack(history, &updd->udd_id);
}

M0_INTERNAL void m0_dtm_update_unpack(struct m0_dtm_update *update,
				      const struct m0_dtm_update_descr *updd)
{
	struct m0_dtm_up *up = &update->upd_up;

	M0_PRE(update->upd_label == updd->udd_data.da_label);
	M0_PRE(M0_IN(update->upd_up.up_state, (M0_DOS_INPROGRESS,
					       M0_DOS_PREPARE)));
	M0_PRE(UPDATE_HISTORY(update)->h_ops->hio_type->hit_id ==
	       updd->udd_id.hid_htype);
	M0_PRE(m0_dtm_update_matches_descr(update, updd));

	up->up_rule     = updd->udd_data.da_rule;
	up->up_ver      = updd->udd_data.da_ver;
	up->up_orig_ver = updd->udd_data.da_orig_ver;

	M0_POST(m0_dtm_update_invariant(update));
}

M0_INTERNAL int m0_dtm_update_build(struct m0_dtm_update *update,
				    struct m0_dtm_oper *oper,
				    const struct m0_dtm_update_descr *updd)
{
	struct m0_dtm_history            *history;
	struct m0_dtm                    *dtm = nu_dtm(oper->oprt_op.op_nu);
	int                               result;

	result = m0_dtm_history_unpack(dtm, &updd->udd_id, &history);
	if (result == 0) {
		if (m0_tl_exists(oper, scan, &oper->oprt_op.op_ups,
				 scan->upd_label == updd->udd_data.da_label))
			return M0_ERR_INFO(-EPROTO, "%i", updd->udd_data.da_label);
		m0_dtm_update_init(update, history, oper, &updd->udd_data);
		result = history->h_ops->hio_update(history, updd->udd_utype,
						    update);
	}
	M0_POST(ergo(result == 0, m0_dtm_update_invariant(update)));
	return result;
}

M0_INTERNAL bool
m0_dtm_update_matches_descr(const struct m0_dtm_update *update,
			    const struct m0_dtm_update_descr *updd)
{
	const struct m0_dtm_up *up = &update->upd_up;

	return
		up->up_rule == updd->udd_data.da_rule &&
		M0_IN(up->up_ver,      (0, updd->udd_data.da_ver)) &&
		M0_IN(up->up_orig_ver, (0, updd->udd_data.da_orig_ver));
}

M0_INTERNAL bool
m0_dtm_descr_matches_update(const struct m0_dtm_update *update,
			    const struct m0_dtm_update_descr *updd)
{
	const struct m0_dtm_up *up = &update->upd_up;

	return
		up->up_rule == updd->udd_data.da_rule &&
		M0_IN(updd->udd_data.da_ver,      (0, up->up_ver)) &&
		M0_IN(updd->udd_data.da_orig_ver, (0, up->up_orig_ver));
}

M0_INTERNAL void m0_dtm_update_list_init(struct m0_tl *list)
{
	oper_tlist_init(list);
}

M0_INTERNAL void m0_dtm_update_list_fini(struct m0_tl *list)
{
	struct m0_dtm_update *leftover;

	m0_tl_for(oper, list, leftover) {
		oper_tlist_del(leftover);
	} m0_tl_endfor;
	oper_tlist_fini(list);
}

M0_INTERNAL void m0_dtm_update_reint(struct m0_dtm_update *update)
{
	M0_PRE(update->upd_up.up_state == M0_DOS_INPROGRESS);
	M0_PRE(update->upd_comm.uc_body != NULL);
	M0_PRE(update->upd_comm.uc_state == M0_DUX_NEW);

	history_lock(UPDATE_HISTORY(update));
	update_reint(update);
	history_unlock(UPDATE_HISTORY(update));
}

static const struct m0_rpc_item_ops dtm_update_item_ops;

M0_INTERNAL void m0_dtm_comm_set(struct m0_dtm_update *update,
				 struct m0_fop *fop)
{
	struct m0_dtm_update_comm *comm = &update->upd_comm;

	M0_PRE(update->upd_up.up_state < M0_DOS_INPROGRESS);
	M0_PRE(comm->uc_state == M0_DUX_NEW);
	M0_PRE(comm->uc_body == NULL);
	M0_PRE(fop->f_item.ri_ops == NULL);
	M0_PRE(fop->f_opaque == NULL);

	comm->uc_body = fop;
	fop->f_item.ri_ops = &dtm_update_item_ops;
	fop->f_opaque = update;
}

M0_INTERNAL void m0_dtm_update_link(struct m0_tl *list,
				    struct m0_dtm_update *update, uint32_t nr)
{
	while (nr-- != 0)
		oper_tlink_init_at(update++, list);
}

M0_INTERNAL struct m0_dtm_update *up_update(struct m0_dtm_up *up)
{
	return up != NULL ?
		container_of(up, struct m0_dtm_update, upd_up) : NULL;
}

M0_INTERNAL bool update_is_earlier(struct m0_dtm_update *update0,
				   struct m0_dtm_update *update1)
{
	return up_is_earlier(UPDATE_UP(update0), UPDATE_UP(update1));
}

M0_INTERNAL void update_reint(struct m0_dtm_update *update)
{
	struct m0_dtm_update_comm *comm    = &update->upd_comm;
	struct m0_dtm_history     *history = UPDATE_HISTORY(update);
	struct m0_dtm_remote      *rem     = history->h_rem;

	M0_PRE(update->upd_up.up_state == M0_DOS_INPROGRESS);

	if (comm->uc_body == NULL)
		return;
	if (comm->uc_state == M0_DUX_NEW)
		rem->re_ops->reo_send(rem, update);
	else if (comm->uc_state == M0_DUX_INFLIGHT &&
		 comm->uc_instance == history->h_rem->re_instance)
		;
	else
		rem->re_ops->reo_resend(rem, update);

	comm->uc_state    = M0_DUX_INFLIGHT;
	comm->uc_instance = history->h_rem->re_instance;
}

static void dtm_update_replied(struct m0_rpc_item *item)
{
	struct m0_fop             *fop    = m0_rpc_item_to_fop(item);
	struct m0_dtm_update      *update = fop->f_opaque;
	struct m0_dtm_update_comm *comm   = &update->upd_comm;

	M0_PRE(m0_dtm_update_invariant(update));
	M0_PRE(fop == comm->uc_body);
	M0_PRE(comm->uc_state == M0_DUX_INFLIGHT);

	/* XXX TODO Handle rpc errors. */

	if (!(comm->uc_flags & M0_DUCF_REPLIED_CALLED)) {
		if (update->upd_ops->updto_replied != NULL)
			update->upd_ops->updto_replied(update);
		comm->uc_flags |= M0_DUCF_REPLIED_CALLED;
	}
}

static const struct m0_rpc_item_ops dtm_update_item_ops = {
	.rio_replied = &dtm_update_replied
};

M0_TL_DESCR_DEFINE(history, "dtm history updates", M0_INTERNAL,
		   struct m0_dtm_update,
		   upd_up.up_hi_linkage, upd_up.up_magix,
		   M0_DTM_UP_MAGIX, M0_DTM_HI_MAGIX);
M0_TL_DEFINE(history, M0_INTERNAL, struct m0_dtm_update);

M0_TL_DESCR_DEFINE(oper, "dtm operation updates", M0_INTERNAL,
		   struct m0_dtm_update,
		   upd_up.up_op_linkage, upd_up.up_magix,
		   M0_DTM_UP_MAGIX, M0_DTM_OP_MAGIX);
M0_TL_DEFINE(oper, M0_INTERNAL, struct m0_dtm_update);

M0_INTERNAL void update_print_internal(const struct m0_dtm_update *update,
				       bool history)
{
	static const char state_name[] = "LFpIVPS??????????????????";
	static const char rule_name[] = "ISNA?????????????????????";
	char buf[100];

	history_print_header(UPDATE_HISTORY(update), buf);
	M0_LOG(M0_FATAL, "\tupdate: %s@%p/%p: %s",
	       update->upd_ops->updo_type->updtt_name,
	       update, update->upd_up.up_op, history ? &buf[0] : "");
	M0_LOG(M0_FATAL, "\t\tstate: %c label: %lx "
	       "rule: %c ver: %lu orig: %lu",
	       state_name[update->upd_up.up_state],
	       (unsigned long)update->upd_label,
	       rule_name[update->upd_up.up_rule],
	       (unsigned long)update->upd_up.up_ver,
	       (unsigned long)update->upd_up.up_orig_ver);
}

M0_INTERNAL void update_print(const struct m0_dtm_update *update)
{
	update_print_internal(update, true);
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
