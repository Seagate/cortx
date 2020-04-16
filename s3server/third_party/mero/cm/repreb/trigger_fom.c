/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 16-Aug-2016
 */


/**
 * @addtogroup CM
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM
#include "cm/cm.h"
#include "cm/repreb/trigger_fom.h"
#include "cm/repreb/trigger_fop.h"
#include "cm/repreb/cm.h"
#include "rpc/rpc.h"
#include "fop/fom.h"
#include "fop/fop.h"
#include "fop/fop_item_type.h"
#include "lib/trace.h"
#include "lib/finject.h"

static int trigger_fom_tick(struct m0_fom *fom);
static void trigger_fom_fini(struct m0_fom *fom);
static size_t trigger_fom_home_locality(const struct m0_fom *fom);
static int prepare(struct m0_fom *fom);
static int ready(struct m0_fom *fom);
static int start(struct m0_fom *fom);

static const struct m0_fom_ops trigger_fom_ops = {
	.fo_fini          = trigger_fom_fini,
	.fo_tick          = trigger_fom_tick,
	.fo_home_locality = trigger_fom_home_locality
};

struct m0_sm_state_descr m0_trigger_phases[] = {
	[M0_TPH_PREPARE] = {
		.sd_name      = "Prepare copy machine",
		.sd_allowed   = M0_BITS(M0_TPH_READY, M0_FOPH_INIT,
					M0_FOPH_FAILURE, M0_FOPH_SUCCESS,
					M0_FOPH_FINISH)
	},
	[M0_TPH_READY] = {
		.sd_name      = "Send ready fops",
		.sd_allowed   = M0_BITS(M0_TPH_START, M0_FOPH_FAILURE)
	},
	[M0_TPH_START] = {
		.sd_name      = "Start repair/rebalance",
		.sd_allowed   = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE)
	},
};

const struct m0_sm_conf m0_trigger_conf = {
	.scf_name      = "Trigger",
	.scf_nr_states = ARRAY_SIZE(m0_trigger_phases),
	.scf_state     = m0_trigger_phases
};

static int (*trig_action[]) (struct m0_fom *) = {
	[M0_TPH_PREPARE] = prepare,
	[M0_TPH_READY]   = ready,
	[M0_TPH_START]   = start,
};

M0_INTERNAL int m0_trigger_fom_create(struct m0_trigger_fom *tfom,
				      struct m0_fop         *fop,
				      struct m0_reqh        *reqh)
{
	struct m0_fom      *fom = &tfom->tf_fom;
	struct m0_fop      *rep_fop;
	uint32_t            op;
	struct m0_fop_type *ft;

	M0_PRE(fop != NULL);
	M0_PRE(tfom != NULL);
	op = m0_fop_opcode(fop);
	ft = tfom->tf_ops->fto_type(op);
	rep_fop = m0_fop_reply_alloc(fop, ft);
	if (rep_fop == NULL)
		return M0_ERR(-ENOMEM);
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &trigger_fom_ops,
		    fop, rep_fop, reqh);
	return M0_RC(0);
}

static void trigger_fom_fini(struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	m0_fom_fini(fom);
}

static size_t trigger_fom_home_locality(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	return m0_fop_opcode(fom->fo_fop);
}

static struct m0_cm *trig2cm(const struct m0_fom *fom)
{
	return container_of(fom->fo_service, struct m0_cm, cm_service);
}

static struct m0_trigger_fom *trig2tfom(const struct m0_fom *fom)
{
	return container_of(fom, struct m0_trigger_fom, tf_fom);
}

static void trigger_rep_set(struct m0_fom *fom)
{
	struct m0_fop          *rfop = fom->fo_rep_fop;
	struct trigger_rep_fop *trep = m0_fop_data(rfop);
	struct m0_cm           *cm   = trig2cm(fom);

	trep->rc = cm->cm_mach.sm_rc != 0 ? cm->cm_mach.sm_rc : m0_fom_rc(fom);
	fom->fo_rep_fop = rfop;
}

static int trigger_fom_tick(struct m0_fom *fom)
{
	struct trigger_fop *treq = m0_fop_data(fom->fo_fop);
	struct m0_cm       *cm;
	enum m0_cm_state    cm_state;
	int                 rc = 0;

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		cm = trig2cm(fom);
		if (m0_fom_phase(fom) == M0_FOPH_INIT &&
						treq->op != CM_OP_INVALID) {
			m0_cm_lock(cm);
			cm_state = m0_cm_state_get(cm);
			m0_cm_unlock(cm);
			/*
			 * Run TPH_PREPARE phase before generic phases. This is
			 * required to prevent dependency between trigger_fom's
			 * and m0_cm_sw_update fom's transactions.
			 */
			if (M0_IN(cm_state, (M0_CMS_IDLE, M0_CMS_STOP,
					     M0_CMS_FAIL)) ||
			    M0_IN(treq->op, (CM_OP_REPAIR_QUIESCE,
					     CM_OP_REBALANCE_QUIESCE,
					     CM_OP_REPAIR_ABORT,
					     CM_OP_REBALANCE_ABORT,
					     CM_OP_REPAIR_STATUS,
					     CM_OP_REBALANCE_STATUS))) {
				m0_fom_phase_set(fom, M0_TPH_PREPARE);
				return M0_FSO_AGAIN;
			}
		}
		rc = m0_fom_tick_generic(fom);
	} else
		rc = trig_action[m0_fom_phase(fom)](fom);
	if (rc < 0) {
		if (rc == -EBUSY)
			m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
		else
			m0_fom_phase_move(fom, rc, M0_FOPH_FAILURE);
		trigger_rep_set(fom);
		rc = M0_FSO_AGAIN;
	}

	return M0_RC(rc);
}

static int prepare(struct m0_fom *fom)
{
	struct m0_cm          *cm = trig2cm(fom);
	struct trigger_fop    *treq = m0_fop_data(fom->fo_fop);
	enum m0_cm_state       cm_state;
	struct m0_trigger_fom *tfom = trig2tfom(fom);
	int                    rc;

	if (M0_IN(treq->op, (CM_OP_REPAIR_QUIESCE, CM_OP_REBALANCE_QUIESCE))) {
		/* Set quiesce flag to running copy machine and quit. */
		cm->cm_quiesce = true;
		trigger_rep_set(fom);
		m0_rpc_reply_post(m0_fop_to_rpc_item(fom->fo_fop),
				  m0_fop_to_rpc_item(fom->fo_rep_fop));
		m0_fom_phase_set(fom, M0_FOPH_FINISH);
		return M0_FSO_WAIT;
	}

	if (M0_IN(treq->op, (CM_OP_REPAIR_ABORT, CM_OP_REBALANCE_ABORT))) {
		/* Set abort flag. */
		m0_cm_lock(cm);
		/* Its an explicit abort command, no need to transition cm
		 * to failed state.*/
		m0_cm_abort(cm, 0);
		m0_cm_unlock(cm);
		M0_LOG(M0_DEBUG, "GOT ABORT cmd");
		trigger_rep_set(fom);
		m0_rpc_reply_post(m0_fop_to_rpc_item(fom->fo_fop),
				  m0_fop_to_rpc_item(fom->fo_rep_fop));
		m0_fom_phase_set(fom, M0_FOPH_FINISH);
		return M0_FSO_WAIT;
	}

	m0_cm_lock(cm);
	cm_state = m0_cm_state_get(cm);
	m0_cm_unlock(cm);

	if (M0_IN(treq->op, (CM_OP_REPAIR_STATUS, CM_OP_REBALANCE_STATUS))) {
		struct m0_fop            *rfop = fom->fo_rep_fop;
		struct m0_status_rep_fop *trep = m0_fop_data(rfop);
		enum m0_cm_status         cm_status;

		/* Send back status and progress. */
		M0_LOG(M0_DEBUG, "sending back status for %d: cm state=%d",
				 treq->op, cm_state);
		switch (cm_state) {
			case M0_CMS_IDLE:
			case M0_CMS_STOP:
				if (cm->cm_quiesce)
					cm_status = CM_STATUS_PAUSED;
				else
					cm_status = CM_STATUS_IDLE;
				break;
			case M0_CMS_PREPARE:
			case M0_CMS_READY:
			case M0_CMS_ACTIVE:
				cm_status = CM_STATUS_STARTED;
				break;
			case M0_CMS_FAIL:
				/* Wait for cleanup */
				if (cm->cm_done && cm->cm_proxy_nr == 0 &&
				    cm->cm_sw_update.swu_is_complete)
					cm_status = CM_STATUS_FAILED;
				else
					cm_status = CM_STATUS_STARTED;
				break;
			case M0_CMS_FINI:
			case M0_CMS_INIT:
			default:
				cm_status = CM_STATUS_INVALID;
				break;
		}
		trep->ssr_state = cm_status;
		trep->ssr_progress = tfom->tf_ops->fto_progress(fom, false);
		m0_rpc_reply_post(m0_fop_to_rpc_item(fom->fo_fop),
				  m0_fop_to_rpc_item(fom->fo_rep_fop));
		m0_fom_phase_set(fom, M0_FOPH_FINISH);
		return M0_FSO_WAIT;
	}

	rc = M0_FSO_AGAIN;
	tfom->tf_ops->fto_prepare(fom);
	if (M0_IN(cm_state, (M0_CMS_IDLE, M0_CMS_STOP, M0_CMS_FAIL))) {
		tfom->tf_ops->fto_progress(fom, true);
		m0_cm_wait(cm, fom);
		rc = m0_cm_prepare(cm);
		if (rc == 0) {
			m0_fom_phase_set(fom, M0_FOPH_INIT);
			rc = M0_FSO_WAIT;
		} else {
			m0_cm_wait_cancel(cm, fom);
		}
	} else if (M0_IN(cm_state, (M0_CMS_READY, M0_CMS_ACTIVE))) {
		/* CM is busy. */
		rc = -EBUSY;
		M0_LOG(M0_WARN, "CM is still active, state: %d", cm_state);
	} else
		m0_fom_phase_set(fom, M0_TPH_READY);
	M0_LOG(M0_DEBUG, "got trigger: prepare");
	return M0_RC(rc);
}

static int ready(struct m0_fom *fom)
{
	struct m0_cm *cm = trig2cm(fom);
	int           rc;

	if (M0_FI_ENABLED("no_wait")) {
		rc = m0_cm_ready(cm);
		if (rc == 0) {
			m0_fom_phase_set(fom, M0_TPH_START);
			rc = M0_FSO_AGAIN;
		}
		return M0_RC(rc);
	}
	if (cm->cm_proxy_nr > 0) {
		m0_cm_lock(cm);
		m0_cm_proxies_init_wait(cm, fom);
		m0_cm_unlock(cm);
		rc = M0_FSO_WAIT;
	} else
		rc = M0_FSO_AGAIN;
	rc = m0_cm_ready(cm) ?: rc;
	if (rc < 0 && rc != -EAGAIN) {
		if (cm->cm_proxy_nr > 0)
			m0_cm_wait_cancel(cm, fom);
		return M0_ERR(rc);
	}
	if (rc == -EAGAIN)
		return M0_FSO_WAIT;

	m0_fom_phase_set(fom, M0_TPH_START);
	M0_LOG(M0_DEBUG, "trigger: ready rc: %d", rc);
	return rc;
}

static int start(struct m0_fom *fom)
{
	struct m0_cm *cm = trig2cm(fom);
	int           rc;

	/*
	 * CM start is potentially blocking operation. For example, DIX
	 * repair/re-balance CM start iterator in separate FOM and waits until
	 * it reaches desired state.
	 */
	m0_fom_block_enter(fom);
	rc = m0_cm_start(cm);
	m0_fom_block_leave(fom);
	if (rc == -EAGAIN) {
		m0_cm_lock(cm);
		m0_cm_proxies_init_wait(cm, fom);
		m0_cm_unlock(cm);
		return M0_FSO_WAIT;
	}
	if (rc != 0)
		return M0_ERR(rc);
	M0_LOG(M0_DEBUG, "trigger: start");
	trigger_rep_set(fom);
	m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

#ifndef __KERNEL__
extern struct m0_sm_state_descr m0_trigger_phases[];
extern const struct m0_sm_conf m0_trigger_conf;
#endif

M0_INTERNAL void m0_cm_trigger_fop_fini(struct m0_fop_type *ft)
{
	m0_fop_type_fini(ft);
}

M0_INTERNAL void m0_cm_trigger_fop_init(struct m0_fop_type *ft,
				        enum M0_RPC_OPCODES op,
				        const char *name,
				        const struct m0_xcode_type *xt,
				        uint64_t rpc_flags,
				        struct m0_cm_type *cmt,
				        const struct m0_fom_type_ops *ops)
{
#ifndef __KERNEL__
	m0_sm_conf_extend(m0_generic_conf.scf_state, m0_trigger_phases,
			  m0_generic_conf.scf_nr_states);
#endif

	M0_FOP_TYPE_INIT(ft,
			 .name      = name,
			 .opcode    = op,
			 .xt        = xt,
#ifndef __KERNEL__
			 .fom_ops   = ops,
			 .svc_type  = &cmt->ct_stype,
			 .sm        = &m0_trigger_conf,
#endif
			 .rpc_flags = rpc_flags);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of CM group */

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
