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
 * Original creation date: 18-Apr-2013
 */


/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM

#include "lib/trace.h"
#include "lib/misc.h"         /* M0_IN */
#include "rpc/rpc.h"
#include "rpc/rpc_opcodes.h"  /* M0_DTM_NOTIFICATION_OPCODE */
#include "fop/fop.h"
#include "reqh/reqh.h"        /* m0_reqh_fop_handle */

#include "dtm/dtm_internal.h"
#include "dtm/history.h"
#include "dtm/remote.h"
#include "dtm/remote_xc.h"

enum rem_rpc_notification {
	R_PERSISTENT = 1,
	R_FIXED      = 2,
	R_RESET      = 3,
	R_UNDO       = 4
};

static const struct m0_dtm_remote_ops rem_rpc_ops;
static const struct m0_dtm_remote_ops rem_local_ops;
static void rem_rpc_notify(struct m0_dtm_remote *rem,
			   const struct m0_dtm_history *history,
			   m0_dtm_ver_t ver, enum rem_rpc_notification opcode);
static int rem_rpc_deliver(struct m0_rpc_machine *mach,
			   struct m0_rpc_item *item);

static struct m0_fop_type rem_rpc_fopt;
static const struct m0_fop_type_ops rem_rpc_ftype_ops;
static struct m0_rpc_item_type_ops rem_rpc_itype_ops;
static const struct m0_rpc_item_ops rem_rpc_item_sender_ops;
static const struct m0_rpc_item_ops rem_rpc_item_redo_ops;

M0_INTERNAL void m0_dtm_remote_add(struct m0_dtm_remote *rem,
				   struct m0_dtm_oper *oper,
				   struct m0_dtm_history *history,
				   struct m0_dtm_update *update)
{
	m0_dtm_fol_remote_add(&rem->re_fol, oper);
}

M0_INTERNAL void m0_dtm_remote_init(struct m0_dtm_remote *remote,
				    struct m0_uint128 *id,
				    struct m0_dtm *local)
{
	M0_PRE(!m0_uint128_eq(id, &local->d_id));
	remote->re_id = *id;
	m0_dtm_fol_remote_init(&remote->re_fol, local, remote);
}

M0_INTERNAL void m0_dtm_remote_fini(struct m0_dtm_remote *remote)
{
	m0_dtm_fol_remote_fini(&remote->re_fol);
}

M0_INTERNAL void m0_dtm_rpc_remote_init(struct m0_dtm_rpc_remote *remote,
					struct m0_uint128 *id,
					struct m0_dtm *local,
					struct m0_rpc_conn *conn)
{
	m0_dtm_remote_init(&remote->rpr_rem, id, local);
	remote->rpr_conn       = conn;
	remote->rpr_rem.re_ops = &rem_rpc_ops;
}

M0_INTERNAL void m0_dtm_rpc_remote_fini(struct m0_dtm_rpc_remote *remote)
{
	m0_dtm_remote_fini(&remote->rpr_rem);
}

static void rem_rpc_persistent(struct m0_dtm_remote *rem,
			       struct m0_dtm_history *history)
{
	rem_rpc_notify(rem, history,
		       update_ver(history->h_persistent), R_PERSISTENT);
}

static void rem_rpc_fixed(struct m0_dtm_remote *rem,
			  struct m0_dtm_history *history)
{
	rem_rpc_notify(rem, history, 0, R_FIXED);
}

static void rem_rpc_reset(struct m0_dtm_remote *rem,
			  struct m0_dtm_history *history)
{
	rem_rpc_notify(rem, history, history->h_hi.hi_ver, R_RESET);
}

static void rem_rpc_undo(struct m0_dtm_remote *rem,
			 struct m0_dtm_history *history, m0_dtm_ver_t upto)
{
	rem_rpc_notify(rem, history, upto, R_UNDO);
}

M0_EXTERN struct m0_rpc_session *m0_rpc_conn_session0(const struct m0_rpc_conn
						      *conn);

static void rem_rpc_send(struct m0_dtm_remote *rem,
			 struct m0_dtm_update *update)
{
	struct m0_rpc_item *item = &update->upd_comm.uc_body->f_item;

	M0_PRE(update->upd_comm.uc_body != NULL);
	M0_PRE(update->upd_up.up_state == M0_DOS_INPROGRESS);
	m0_rpc_post(item);
}

static void rem_rpc_resend(struct m0_dtm_remote *rem,
			   struct m0_dtm_update *update)
{
	struct m0_rpc_item *item = &update->upd_comm.uc_body->f_item;

	M0_PRE(update->upd_comm.uc_body != NULL);
	M0_PRE(M0_IN(update->upd_up.up_state, (M0_DOS_INPROGRESS,
					       M0_DOS_VOLATILE,
					       M0_DOS_PERSISTENT)));
	m0_rpc_item_cancel_init(item);
	/* add 1/100 second deadline to give a chance to build a better rpc. */
	item->ri_deadline = m0_time_from_now(0, 10000000);
	m0_rpc_post(item);
}

static const struct m0_dtm_remote_ops rem_rpc_ops = {
	.reo_persistent = &rem_rpc_persistent,
	.reo_fixed      = &rem_rpc_fixed,
	.reo_reset      = &rem_rpc_reset,
	.reo_send       = &rem_rpc_send,
	.reo_resend     = &rem_rpc_resend,
	.reo_undo       = &rem_rpc_undo
};

static void notice_pack(struct m0_dtm_notice *notice,
			const struct m0_dtm_history *history,
			m0_dtm_ver_t ver, enum rem_rpc_notification opcode)
{
	notice->dno_opcode = opcode;
	notice->dno_ver    = ver;
	m0_dtm_history_pack(history, &notice->dno_id);
}

static void rem_rpc_notify(struct m0_dtm_remote *rem,
			   const struct m0_dtm_history *history,
			   m0_dtm_ver_t ver, enum rem_rpc_notification opcode)
{
	struct m0_dtm_rpc_remote *rpr;
	struct m0_fop            *fop;

	M0_PRE(rem->re_ops == &rem_rpc_ops);
	rpr = M0_AMB(rpr, rem, rpr_rem);
	fop = m0_fop_alloc(&rem_rpc_fopt, NULL, rpr->rpr_conn->c_rpc_machine);
	if (fop != NULL) {
		struct m0_rpc_item *item;

		notice_pack(m0_fop_data(fop), history, ver, opcode);
		item              = &fop->f_item;
		item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
		item->ri_deadline = 0;
		item->ri_ops      = &rem_rpc_item_sender_ops;
		m0_rpc_oneway_item_post(rpr->rpr_conn, item);
	}
}

M0_INTERNAL int m0_dtm_remote_global_init(void)
{
	rem_rpc_itype_ops = m0_fop_default_item_type_ops;
	rem_rpc_itype_ops.rito_deliver = &rem_rpc_deliver;
	M0_FOP_TYPE_INIT(&rem_rpc_fopt,
			 .name      = "dtm notice",
			 .opcode    = M0_DTM_NOTIFICATION_OPCODE,
			 .xt        = m0_dtm_notice_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_ONEWAY,
			 .fop_ops   = &rem_rpc_ftype_ops,
			 .rpc_ops   = &rem_rpc_itype_ops);
	return 0;
}

M0_INTERNAL void m0_dtm_remote_global_fini(void)
{
	m0_fop_type_fini(&rem_rpc_fopt);
}

static void notice_deliver(struct m0_dtm_notice *notice, struct m0_dtm *dtm)
{
	struct m0_dtm_history *history;
	int                    result;

	result = m0_dtm_history_unpack(dtm, &notice->dno_id, &history);
	if (result == 0) {
		switch (notice->dno_opcode) {
		case R_PERSISTENT:
			m0_dtm_history_persistent(history, notice->dno_ver);
			break;
		case R_FIXED:
			break;
		case R_RESET:
			m0_dtm_history_reset(history, notice->dno_ver);
			break;
		case R_UNDO:
			m0_dtm_history_undo(history, notice->dno_ver);
			break;
		default:
			M0_LOG(M0_ERROR, "DTM notice: %i.", notice->dno_opcode);
		}
	} else
		M0_LOG(M0_ERROR, "DTM history: %i.", result);
}

static int rem_rpc_deliver(struct m0_rpc_machine *mach,
			   struct m0_rpc_item *item)
{
	notice_deliver(m0_fop_data(m0_rpc_item_to_fop(item)), mach->rm_dtm);
	return 0;
}

static void rem_rpc_redo_replied(struct m0_rpc_item *item)
{}

static const struct m0_fop_type_ops rem_rpc_ftype_ops = {
	/* nothing */
};

static struct m0_rpc_item_type_ops rem_rpc_itype_ops = {
	/* initialised in m0_dtm_remote_global_init() */
};

static const struct m0_rpc_item_ops rem_rpc_item_sender_ops = {
	/* nothing */
};

static const struct m0_rpc_item_ops rem_rpc_item_redo_ops = {
	.rio_replied = &rem_rpc_redo_replied
};

M0_INTERNAL void m0_dtm_local_remote_init(struct m0_dtm_local_remote *lre,
					  struct m0_uint128 *id,
					  struct m0_dtm *local,
					  struct m0_reqh *reqh)
{
	m0_dtm_remote_init(&lre->lre_rem, id, local);
	lre->lre_rem.re_ops = &rem_local_ops;
	lre->lre_reqh = reqh;
}

M0_INTERNAL void m0_dtm_local_remote_fini(struct m0_dtm_local_remote *lre)
{
	m0_dtm_remote_fini(&lre->lre_rem);
}

static void rem_local_notify(struct m0_dtm_remote *rem,
			     const struct m0_dtm_history *history,
			     m0_dtm_ver_t ver, enum rem_rpc_notification opcode)
{
	struct m0_dtm_notice notice;

	notice_pack(&notice, history, ver, opcode);
	notice_deliver(&notice, HISTORY_DTM(&rem->re_fol.rfo_ch.ch_history));
}

static void rem_local_persistent(struct m0_dtm_remote *rem,
				 struct m0_dtm_history *history)
{
	rem_local_notify(rem, history,
			 update_ver(history->h_persistent), R_PERSISTENT);
}

static void rem_local_fixed(struct m0_dtm_remote *rem,
			    struct m0_dtm_history *history)
{
	rem_local_notify(rem, history, 0, R_FIXED);
}

static void rem_local_reset(struct m0_dtm_remote *rem,
			    struct m0_dtm_history *history)
{
	rem_local_notify(rem, history, history->h_hi.hi_ver, R_RESET);
}

static void rem_local_send(struct m0_dtm_remote *rem,
			   struct m0_dtm_update *update)
{
	struct m0_dtm_local_remote *lre;
	int                         result;

	lre = M0_AMB(lre, rem, lre_rem);
	result = m0_reqh_fop_handle(lre->lre_reqh, update->upd_comm.uc_body);
	if (result != 0)
		M0_LOG(M0_ERROR, "redo: %i.", result);
}

static void rem_local_undo(struct m0_dtm_remote *rem,
			   struct m0_dtm_history *history, m0_dtm_ver_t upto)
{
	rem_local_notify(rem, history, upto, R_UNDO);
}


static const struct m0_dtm_remote_ops rem_local_ops = {
	.reo_persistent = &rem_local_persistent,
	.reo_fixed      = &rem_local_fixed,
	.reo_reset      = &rem_local_reset,
	.reo_send       = &rem_local_send,
	.reo_resend     = &rem_local_send,
	.reo_undo       = &rem_local_undo
};

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
