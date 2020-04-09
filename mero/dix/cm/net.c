/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>
 * Original creation date: 25/08/2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "lib/trace.h"

#include "lib/memory.h"

#include "cm/proxy.h"
#include "dix/cm/cm.h"
#include "dix/cm/cp.h"
#include "dix/cm/dix_cp_onwire.h"

#include "fop/fop.h"
#include "fop/fom.h"
#include "net/net.h"
#include "rpc/item.h"
#include "rpc/rpclib.h"
#include "rpc/session.h"
#include "rpc/conn.h"
#include "dix/fid_convert.h"

/**
 * @addtogroup DIXCMCP
 * @{
 */

static void dix_cp_reply_received(struct m0_rpc_item *item);

/*
 * Over-ridden rpc item ops, required to send notification to the copy packet
 * send phase that reply has been received and the copy packet can be finalised.
 */
static const struct m0_rpc_item_ops dix_cp_item_ops = {
	.rio_replied = dix_cp_reply_received,
};

/* Converts in-memory copy packet structure to onwire copy packet structure. */
static int dixcp_to_dixcpx(struct m0_dix_cm_cp *dix_cp,
			   struct m0_dix_cpx   *dix_cpx)
{
	struct m0_cm_cp *cp;
	int              rc;

	M0_PRE(dix_cp != NULL);
	M0_PRE(dix_cpx != NULL);

	cp = &dix_cp->dc_base;

	dix_cpx->dcx_ctg_fid = dix_cp->dc_ctg_fid;
	dix_cpx->dcx_ctg_op_flags = dix_cp->dc_ctg_op_flags;
	dix_cpx->dcx_cp.cpx_prio = cp->c_prio;
	dix_cpx->dcx_phase = M0_CCP_SEND;
	m0_cm_ag_id_copy(&dix_cpx->dcx_cp.cpx_ag_id, &cp->c_ag->cag_id);
	m0_bitmap_onwire_init(&dix_cpx->dcx_cp.cpx_bm, 0);

	m0_rpc_at_init(&dix_cpx->dcx_ab_key);
	m0_rpc_at_init(&dix_cpx->dcx_ab_val);

	rc = m0_rpc_at_add(&dix_cpx->dcx_ab_key, &dix_cp->dc_key,
			   cp->c_cm_proxy->px_conn);
	if (rc == 0)
		rc = m0_rpc_at_add(&dix_cpx->dcx_ab_val, &dix_cp->dc_val,
				   cp->c_cm_proxy->px_conn);
	if (rc == 0) {
		/* Now it's up to dix_cpx to free buffers. */
		M0_SET0(&dix_cp->dc_key);
		M0_SET0(&dix_cp->dc_val);
	}

	return rc;
}

static void dix_cp_reply_received(struct m0_rpc_item *req_item)
{
	struct m0_fop       *req_fop;
	struct m0_dix_cm_cp *dix_cp;
	struct m0_rpc_item  *rep_item;
	struct m0_cm_cp_fop *cp_fop;
	struct m0_fop       *rep_fop;
	struct m0_cpx_reply *cpx_rep;

	M0_ENTRY();
	req_fop = m0_rpc_item_to_fop(req_item);
	cp_fop = M0_AMB(cp_fop, req_fop, cf_fop);
	dix_cp = cp2dixcp(cp_fop->cf_cp);
	M0_LOG(M0_DEBUG, "cm %p, cp %p", cpfom2cm(&dix_cp->dc_base.c_fom),
	       &dix_cp->dc_base);
	rep_item = req_item->ri_reply;
	if (!m0_rpc_item_error(req_item) && rep_item != NULL) {
		if (m0_rpc_item_is_generic_reply_fop(rep_item)) {
			dix_cp->dc_base.c_rc =
				m0_rpc_item_generic_reply_rc(rep_item);
		} else {
			rep_fop = m0_rpc_item_to_fop(rep_item);
			cpx_rep = m0_fop_data(rep_fop);
			dix_cp->dc_base.c_rc = cpx_rep->cr_rc;
		}
	} else
		dix_cp->dc_base.c_rc = m0_rpc_item_error(req_item);

	m0_fom_wakeup(&dix_cp->dc_base.c_fom);
	M0_LEAVE("%d", dix_cp->dc_base.c_rc);
}

static void dix_cp_fop_release(struct m0_ref *ref)
{
	struct m0_cm_cp_fop *cp_fop;
	struct m0_fop       *fop = M0_AMB(fop, ref, f_ref);
	struct m0_dix_cpx   *dix_cpx = m0_fop_data(fop);

	cp_fop = M0_AMB(cp_fop, fop, cf_fop);
	M0_ASSERT(cp_fop != NULL);
	m0_rpc_at_fini(&dix_cpx->dcx_ab_key);
	m0_rpc_at_fini(&dix_cpx->dcx_ab_val);
	m0_fop_fini(fop);
	m0_free(cp_fop);
}

M0_INTERNAL int m0_dix_cm_cp_send(struct m0_cm_cp *cp, struct m0_fop_type *ft)
{
	struct m0_dix_cm_cp   *dix_cp;
	struct m0_dix_cpx     *dix_cpx;
	struct m0_rpc_session *session;
	struct m0_cm_cp_fop   *cp_fop;
	struct m0_fop         *fop;
	struct m0_rpc_item    *item;
	int                    rc;

	M0_ENTRY();
	M0_PRE(cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_SEND);
	M0_PRE(cp->c_cm_proxy != NULL);

	dix_cp = cp2dixcp(cp);
	M0_ALLOC_PTR(cp_fop);
	if (cp_fop == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}
	fop = &cp_fop->cf_fop;
	m0_fop_init(fop, ft, NULL, dix_cp_fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc != 0) {
		m0_fop_fini(fop);
		m0_free(cp_fop);
		goto out;
	}

	dix_cpx = m0_fop_data(fop);
	M0_PRE(dix_cpx != NULL);
	cp_fop->cf_cp = cp;
	cp->c_ops->co_complete(cp);
	rc = dixcp_to_dixcpx(dix_cp, dix_cpx);
	if (rc != 0) {
		m0_fop_fini(fop);
		m0_free(cp_fop);
		goto out;
	}

	m0_mutex_lock(&cp->c_cm_proxy->px_mutex);
	session = cp->c_cm_proxy->px_session;
	m0_mutex_unlock(&cp->c_cm_proxy->px_mutex);

	item  = m0_fop_to_rpc_item(fop);
	item->ri_ops = &dix_cp_item_ops;
	item->ri_session = session;
	item->ri_prio  = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = 0;

	m0_rpc_post(item);
	m0_fop_put_lock(fop);
out:
	if (rc != 0) {
		M0_LOG(M0_ERROR, "rc=%d", rc);
		m0_buf_free(&dix_cp->dc_key);
		m0_buf_free(&dix_cp->dc_val);
		m0_fom_phase_move(&cp->c_fom, rc, M0_CCP_FAIL);
		return M0_RC(M0_FSO_AGAIN);
	}

	m0_fom_phase_set(&cp->c_fom, M0_CCP_SEND_WAIT);
	return M0_RC(M0_FSO_WAIT);
}

M0_INTERNAL int m0_dix_cm_cp_send_wait(struct m0_cm_cp *cp)
{
	M0_PRE(cp != NULL);

	M0_LOG(M0_DEBUG, "reply rc: %d", cp->c_rc);

	if (cp->c_rc != 0) {
		M0_LOG(M0_ERROR, "rc=%d", cp->c_rc);
		m0_fom_phase_move(&cp->c_fom, cp->c_rc, M0_CCP_FAIL);
		return M0_FSO_AGAIN;
	}
	m0_fom_phase_set(&cp->c_fom, M0_CCP_FINI);
	return M0_FSO_WAIT;
}

M0_INTERNAL int m0_dix_cm_cp_recv_init(struct m0_cm_cp *cp)
{
	struct m0_rpc_at_buf *at_buf  = NULL;
	struct m0_dix_cm_cp  *dix_cp  = cp2dixcp(cp);
	struct m0_dix_cpx    *dix_cpx = m0_fop_data(cp->c_fom.fo_fop);

	M0_PRE(dix_cp->dc_phase_transmit < DCM_PT_NR);
	at_buf = dix_cp->dc_phase_transmit < DCM_PT_VAL ?
		&dix_cpx->dcx_ab_key :
		&dix_cpx->dcx_ab_val;

	return m0_rpc_at_load(at_buf, &cp->c_fom, M0_CCP_RECV_WAIT);
}

M0_INTERNAL int m0_dix_cm_cp_recv_wait(struct m0_cm_cp *cp)
{
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);
	int                  result = M0_FSO_AGAIN;

	M0_PRE(dix_cp->dc_phase_transmit < DCM_PT_NR);
	if (dix_cp->dc_phase_transmit < DCM_PT_VAL) {
		/* Start load value, key has been loaded. */
		dix_cp->dc_phase_transmit++;
		m0_fom_phase_set(&cp->c_fom, M0_CCP_RECV_INIT);
	} else {
		struct m0_cas_ctg *meta = m0_ctg_meta();

		M0_ASSERT(meta != NULL);
		/* Key and value are loaded, lock meta-catalogue. */
		result = m0_long_read_lock(m0_ctg_lock(meta),
					   &dix_cp->dc_meta_lock,
					   M0_CCP_XFORM);
		result = M0_FOM_LONG_LOCK_RETURN(result);
	}
	return result;
}

M0_INTERNAL int m0_dix_cm_cp_sw_check(struct m0_cm_cp *cp)
{
	M0_PRE(cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_SW_CHECK);

	/* In DIX we do not care about sliding window, always ready to send. */
	m0_fom_phase_set(&cp->c_fom, M0_CCP_SEND);
	return M0_FSO_AGAIN;
}

/** @} DIXCMCP */
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
