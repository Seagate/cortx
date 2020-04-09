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
#include "lib/memory.h" /* m0_free() */
#include "lib/misc.h"

#include "fop/fom.h"
#include "reqh/reqh.h"
#include "dix/cm/cp.h"
#include "dix/cm/cm.h"
#include "dix/cm/dix_cp_onwire.h"
#include "dix/fid_convert.h"

/**
  @addtogroup DIXCMCP

  @{
*/
extern struct m0_fop_type m0_dix_repair_cpx_reply_fopt;
extern struct m0_fop_type m0_dix_rebalance_cpx_reply_fopt;

M0_INTERNAL int m0_dix_cm_cp_sw_check(struct m0_cm_cp *cp);
M0_INTERNAL int m0_dix_cm_cp_send_wait(struct m0_cm_cp *cp);

M0_INTERNAL int m0_dix_cm_repair_cp_send(struct m0_cm_cp *cp);
M0_INTERNAL int m0_dix_cm_rebalance_cp_send(struct m0_cm_cp *cp);


M0_INTERNAL struct m0_dix_cm_cp *cp2dixcp(const struct m0_cm_cp *cp)
{
	return container_of(cp, struct m0_dix_cm_cp, dc_base);
}

static struct m0_dix_cm *cp2dixcm(struct m0_cm_cp *cp)
{
	struct m0_cm *cm = M0_AMB(cm, cp->c_fom.fo_service, cm_service);

	return cm2dix(cm);
}

M0_INTERNAL bool m0_dix_cm_cp_invariant(const struct m0_cm_cp *cp)
{
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);

	return m0_fom_phase(&cp->c_fom) < M0_CCP_NR &&
	       ergo(m0_fom_phase(&cp->c_fom) > M0_CCP_INIT,
		    m0_fid_is_valid(&dix_cp->dc_ctg_fid));
}

/**
 * Uses device id to select a request handler locality for copy packet FOM.
 */
M0_INTERNAL uint64_t m0_dix_cm_cp_home_loc_helper(const struct m0_cm_cp *cp)
{
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);
	struct m0_fop       *fop = cp->c_fom.fo_fop;
	struct m0_dix_cpx   *dix_cpx = NULL;

	M0_PRE(m0_fom_phase(&cp->c_fom) != M0_CCP_FINI);

	if (fop != NULL)
		dix_cpx = m0_fop_data(fop);

	return m0_dix_fid_cctg_device_id(dix_cpx != NULL ?
					 &dix_cpx->dcx_ctg_fid :
					 &dix_cp->dc_ctg_fid);
}

M0_INTERNAL struct m0_cm_cp *m0_dix_cm_cp_alloc(struct m0_cm *cm)
{
	struct m0_dix_cm_cp *dcp;

	M0_ALLOC_PTR(dcp);
	if (dcp == NULL)
		return NULL;
	return &dcp->dc_base;
}

static void dix_cm_cp_dtx_fini(struct m0_cm_cp *cp)
{
	struct m0_fom *fom = &cp->c_fom;

	m0_dtx_fini(&fom->fo_tx);
	M0_SET0(m0_fom_tx(fom));
}

static int dix_cm_cp_dtx_failure(struct m0_cm_cp *cp)
{
	struct m0_fom   *fom = &cp->c_fom;
	struct m0_be_tx *tx = m0_fom_tx(fom);

	dix_cm_cp_dtx_fini(cp);
	return tx->t_sm.sm_rc;
}

static int dix_cm_cp_incoming_kv(struct m0_rpc_at_buf *ab_key,
				 struct m0_rpc_at_buf *ab_val,
				 struct m0_buf        *key,
				 struct m0_buf        *val)
{
	int rc;

	M0_PRE(m0_rpc_at_is_set(ab_key));
	M0_PRE(m0_rpc_at_is_set(ab_val));
	rc = m0_rpc_at_get(ab_key, key);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_rpc_at_get(ab_val, val);
	return M0_RC(rc);
}

/**
 * Converts onwire copy packet structure to in-memory copy packet structure.
 */
static void dixcpx_to_dixcp(const struct m0_dix_cpx *dix_cpx,
			    struct m0_dix_cm_cp     *dix_cp)
{
	M0_PRE(dix_cp != NULL);
	M0_PRE(dix_cpx != NULL);

	dix_cp->dc_ctg_fid = dix_cpx->dcx_ctg_fid;
	dix_cp->dc_ctg_op_flags = dix_cpx->dcx_ctg_op_flags;

	dix_cp->dc_base.c_prio = dix_cpx->dcx_cp.cpx_prio;

	m0_bitmap_init(&dix_cp->dc_base.c_xform_cp_indices, 1);

	dix_cp->dc_is_local = false;

	dix_cp->dc_base.c_buf_nr = 0;
	dix_cp->dc_base.c_data_seg_nr = 0;
}

static void dix_cm_cp_reply_send(struct m0_cm_cp    *cp,
				 struct m0_fop_type *ft,
				 int                 rc)
{
	struct m0_fop       *rfop = cp->c_fom.fo_rep_fop;
	struct m0_cpx_reply *cpx_rep;

	M0_ENTRY("cm: %p, cp: %p, ft:'%s', rc: %d",
		 cpfom2cm(&cp2dixcp(cp)->dc_base.c_fom), cp, ft->ft_name, rc);
	cpx_rep = m0_fop_data(cp->c_fom.fo_fop);
	cpx_rep->cr_rc = rc;
	m0_rpc_reply_post(&cp->c_fom.fo_fop->f_item, &rfop->f_item);
}

static int dix_cm_fom_tx_wait(struct m0_fom *fom)
{
	m0_fom_wait_on(fom, &m0_fom_tx(fom)->t_sm.sm_chan, &fom->fo_cb);
	return M0_FSO_WAIT;
}

M0_INTERNAL int m0_dix_cm_cp_init(struct m0_cm_cp *cp)
{
	struct m0_dix_cpx *dix_cpx = NULL;
	int                next_phase;
	int                rc = 0;

	M0_ENTRY();
	M0_PRE(m0_fom_phase(&cp->c_fom) == M0_CCP_INIT);

	if (cp->c_fom.fo_fop != NULL) {
		struct m0_dix_cm_cp     *dix_cp = cp2dixcp(cp);
		struct m0_cm_aggr_group *ag;
		struct m0_cm            *cm;

		dix_cpx = m0_fop_data(cp->c_fom.fo_fop);
		dixcpx_to_dixcp(dix_cpx, dix_cp);

		/*
		 * Setup dummy aggregation group and add copy packet here,
		 * generic mechanism needs it.
		 */
		cm = cpfom2cm(&dix_cp->dc_base.c_fom);
		m0_cm_lock(cm);
		rc = m0_cm_aggr_group_alloc(cm, &dix_cpx->dcx_cp.cpx_ag_id,
					    true, &ag);
		m0_cm_unlock(cm);

		if (rc == 0) {
			m0_cm_ag_cp_add(ag, &dix_cp->dc_base);

			dix_cp->dc_phase_transmit = DCM_PT_KEY;
			dix_cp->dc_ctg = NULL;
			m0_long_lock_link_init(&dix_cp->dc_meta_lock,
					       &cp->c_fom,
					       &dix_cp->dc_meta_lock_addb2);
			m0_long_lock_link_init(&dix_cp->dc_ctg_lock, &cp->c_fom,
					       &dix_cp->dc_ctg_lock_addb2);
			m0_fom_phase_set(&cp->c_fom, dix_cpx->dcx_phase);
		}
	}

	if (rc == 0) {
		M0_ASSERT(M0_IN(m0_fom_phase(&cp->c_fom),
				(M0_CCP_INIT, M0_CCP_SEND)));
		if (m0_fom_phase(&cp->c_fom) == M0_CCP_INIT)
			next_phase = M0_CCP_SEND;
		else
			next_phase = M0_CCP_RECV_INIT;
	} else
		next_phase = M0_CCP_FAIL;
	m0_fom_phase_set(&cp->c_fom, next_phase);
	M0_LOG(M0_DEBUG, "Next phase is: %d", next_phase);
	return M0_RC(M0_FSO_AGAIN);
}

M0_INTERNAL int m0_dix_cm_cp_fail(struct m0_cm_cp    *cp,
				  struct m0_fop_type *ft)
{
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);
	struct m0_cas_ctg   *meta   = m0_ctg_meta();

	M0_ENTRY("cp: %p, ft: %s", cp, ft->ft_name);
	M0_PRE(m0_fom_phase(&cp->c_fom) == M0_CCP_FAIL);

	m0_long_unlock(m0_ctg_lock(meta), &dix_cp->dc_meta_lock);
	if (dix_cp->dc_ctg != NULL)
		m0_long_unlock(m0_ctg_lock(dix_cp->dc_ctg), &dix_cp->dc_ctg_lock);
	cp->c_rc = m0_fom_rc(&cp->c_fom);
	dix_cm_cp_reply_send(cp, ft, cp->c_rc);
	m0_fom_phase_move(&cp->c_fom, 0, M0_CCP_FINI);
	M0_LEAVE("ret %d, cp_rc %d", M0_FSO_WAIT, cp->c_rc);
	return M0_RC(M0_FSO_WAIT);
}

static int dix_cm_repair_cp_fail(struct m0_cm_cp *cp)
{
	return m0_dix_cm_cp_fail(cp, &m0_dix_repair_cpx_reply_fopt);
}

static int dix_cm_rebalance_cp_fail(struct m0_cm_cp *cp)
{
	return m0_dix_cm_cp_fail(cp, &m0_dix_rebalance_cpx_reply_fopt);
}

M0_INTERNAL void m0_dix_cm_cp_complete(struct m0_cm_cp *cp)
{
	struct m0_dix_cm       *dcm    = cp2dixcm(cp);
	struct m0_dix_cm_cp    *dix_cp = cp2dixcp(cp);
	struct m0_dix_cm_stats *dcs    = m0_locality_data(dcm->dcm_stats_key);
	size_t                  size;

	M0_PRE(m0_cm_cp_invariant(cp));

	/* Collect stats for the current locality. */
	size = dix_cp->dc_key.b_nob + dix_cp->dc_val.b_nob;
	if (cp->c_io_op == M0_CM_CP_READ)
		dcs->dcs_read_size += size;
	else
		dcs->dcs_write_size += size;
	M0_LEAVE("io_op %d, size %ld", cp->c_io_op, size);
}

M0_INTERNAL void m0_dix_cm_cp_free(struct m0_cm_cp *cp)
{
	struct m0_dix_cm_cp  *dix_cp = cp2dixcp(cp);

	if (cp->c_ag != NULL)
		m0_cm_ag_cp_del(cp->c_ag, cp);
	m0_free(dix_cp);
}

M0_INTERNAL int m0_dix_cm_cp_fini(struct m0_cm_cp *cp)
{
	struct m0_dix_cm_cp  *dix_cp = cp2dixcp(cp);
	struct m0_dix_cm     *dcm = cp2dixcm(cp);
	struct m0_cm_cp_pump *pump = &dcm->dcm_base.cm_cp_pump;

	M0_ENTRY();
	if (dix_cp->dc_is_local) {
		M0_ASSERT(dcm->dcm_cp_in_progress);
		dcm->dcm_cp_in_progress = false;
		m0_fom_wakeup(&pump->p_fom);
	} else {
		m0_long_lock_link_fini(&dix_cp->dc_meta_lock);
		m0_long_lock_link_fini(&dix_cp->dc_ctg_lock);
	}
	return M0_RC(0);
}

M0_INTERNAL void m0_dix_cm_cp_tgt_info_fill(struct m0_dix_cm_cp *dix_cp,
					    const struct m0_fid *cctg_fid)
{
	dix_cp->dc_ctg_fid = *cctg_fid;
}

M0_INTERNAL void m0_dix_cm_cp_setup(struct m0_dix_cm_cp *dix_cp,
					    const struct m0_fid *cctg_fid,
					    uint64_t failed_unit_index)
{
	M0_PRE(dix_cp != NULL && dix_cp->dc_base.c_ag != NULL);

	m0_dix_cm_cp_tgt_info_fill(dix_cp, cctg_fid);
	m0_bitmap_init(&dix_cp->dc_base.c_xform_cp_indices, 1);
}

M0_INTERNAL int m0_dix_cm_cp_dup(struct m0_cm_cp *src, struct m0_cm_cp **dest)
{
	struct m0_dix_cm_cp *dest_dix_cp;
	struct m0_dix_cm_cp *src_dix_cp;
	int                  rc;

	M0_ENTRY();
	rc = m0_cm_cp_dup(src, dest);
	if (rc == 0) {
		dest_dix_cp = cp2dixcp(*dest);
		src_dix_cp = cp2dixcp(src);
		dest_dix_cp->dc_ctg_fid = src_dix_cp->dc_ctg_fid;
		dest_dix_cp->dc_is_local = src_dix_cp->dc_is_local;
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_cm_cp_read(struct m0_cm_cp *cp)
{
	/* All data are already read by DIX iterator. */
	M0_ENTRY();
	m0_fom_phase_set(&cp->c_fom, M0_CCP_IO_WAIT);
	M0_LOG(M0_DEBUG, "set next state to %d", M0_CCP_IO_WAIT);
	return M0_RC(M0_FSO_AGAIN);
}

M0_INTERNAL int m0_dix_cm_cp_xform(struct m0_cm_cp *cp)
{
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);

	M0_ENTRY();
	m0_ctg_op_init(&dix_cp->dc_ctg_op, &cp->c_fom, 0);
	return M0_RC(m0_ctg_meta_lookup(&dix_cp->dc_ctg_op,
					&dix_cp->dc_ctg_fid,
					M0_CCP_WRITE_PRE));
}

M0_INTERNAL int m0_dix_cm_cp_write_pre(struct m0_cm_cp *cp)
{
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);
	struct m0_dix_cpx   *dix_cpx = m0_fop_data(cp->c_fom.fo_fop);
	struct m0_ctg_op    *ctg_op = &dix_cp->dc_ctg_op;
	int                  result = M0_FSO_AGAIN;
	int                  rc;

	M0_ENTRY("cp: %p", cp);

	rc = m0_ctg_op_rc(ctg_op);
	if (rc == 0)
		rc = dix_cm_cp_incoming_kv(&dix_cpx->dcx_ab_key,
					   &dix_cpx->dcx_ab_val,
					   &dix_cp->dc_key,
					   &dix_cp->dc_val);
	if (rc == 0) {
		struct m0_cas_ctg *meta = m0_ctg_meta();

		dix_cp->dc_ctg = m0_ctg_meta_lookup_result(ctg_op);
		M0_ASSERT(dix_cp->dc_ctg != NULL);
		m0_long_read_unlock(m0_ctg_lock(meta),
				    &dix_cp->dc_meta_lock);
		result = m0_long_write_lock(m0_ctg_lock(dix_cp->dc_ctg),
					    &dix_cp->dc_ctg_lock,
					    M0_CCP_TX_OPEN);
		result = M0_FOM_LONG_LOCK_RETURN(result);
	} else {
		M0_LOG(M0_DEBUG, "ctg_op_rc %d move to state %d (fail)",
		       rc, M0_CCP_FAIL);
		m0_fom_phase_move(&cp->c_fom, rc, M0_CCP_FAIL);
	}
	m0_ctg_op_fini(ctg_op);
	return M0_RC(result);
}

M0_INTERNAL int m0_dix_cm_cp_tx_open(struct m0_cm_cp *cp)
{
	struct m0_fom          *fom = &cp->c_fom;
	struct m0_dix_cm_cp    *dix_cp = cp2dixcp(cp);
	struct m0_be_tx_credit *accum = &fom->fo_tx.tx_betx_cred;

	dix_cp->dc_ctg_op_rc = 0;
	m0_dtx_init(&fom->fo_tx, m0_fom_reqh(fom)->rh_beseg->bs_domain,
		    &fom->fo_loc->fl_group);

	m0_ctg_insert_credit(dix_cp->dc_ctg, dix_cp->dc_key.b_nob,
			     dix_cp->dc_val.b_nob, accum);

	m0_dtx_open(&fom->fo_tx);

	m0_fom_phase_set(fom, M0_CCP_WRITE);

	return dix_cm_fom_tx_wait(fom);
}

M0_INTERNAL int m0_dix_cm_cp_write(struct m0_cm_cp *cp)
{
	struct m0_fom       *fom = &cp->c_fom;
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);
	struct m0_dix_cm    *dix_cm = cp2dixcm(cp);
	bool                 repair = dix_cm->dcm_type ==
				      &dix_repair_dcmt;
	struct m0_ctg_op    *ctg_op = &dix_cp->dc_ctg_op;
	struct m0_be_tx     *tx;
	int                  result = M0_FSO_AGAIN;
	int                  rc = 0;

	M0_ENTRY("cp: %p", cp);

	tx = m0_fom_tx(fom);
	if (m0_be_tx_state(tx) != M0_BTS_ACTIVE) {
		if (m0_be_tx_state(tx) == M0_BTS_FAILED)
			rc = dix_cm_cp_dtx_failure(cp);
		else {
			result = dix_cm_fom_tx_wait(fom);
		}
	} else {
		m0_dtx_opened(&fom->fo_tx);
		m0_ctg_op_init(ctg_op, &cp->c_fom,
			       (dix_cp->dc_ctg_op_flags |
				(repair ? COF_RESERVE : 0)));
		cp->c_io_op = M0_CM_CP_WRITE;
		result = m0_ctg_insert(ctg_op, dix_cp->dc_ctg,
				       &dix_cp->dc_key,
				       &dix_cp->dc_val,
				       M0_CCP_TX_DONE);
		if (result < 0)
			rc = result;
	}
	if (rc != 0)
		m0_fom_phase_move(&cp->c_fom, rc, M0_CCP_FAIL);
	return M0_RC(result);
}

M0_INTERNAL int m0_dix_cm_cp_tx_done(struct m0_cm_cp *cp)
{
	struct m0_fom       *fom = &cp->c_fom;
	struct m0_be_tx     *tx = m0_fom_tx(fom);
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);
	struct m0_ctg_op    *ctg_op = &dix_cp->dc_ctg_op;
	int                  result = M0_FSO_AGAIN;
	int                  rc = 0;

	M0_ENTRY("cp=%p, rc %d", cp, m0_ctg_op_rc(ctg_op));

	dix_cp->dc_ctg_op_rc = m0_ctg_op_rc(ctg_op);
	m0_ctg_op_fini(ctg_op);
	if (m0_be_tx_state(tx) == M0_BTS_FAILED) {
		/* @todo: Can not finalise here in active state. */
		fom->fo_tx.tx_state = M0_DTX_DONE;
		rc = dix_cm_cp_dtx_failure(cp);
	} else {
		m0_dtx_done(&fom->fo_tx);
		m0_fom_phase_set(fom, M0_CCP_IO_WAIT);
		result = dix_cm_fom_tx_wait(fom);
	}
	if (rc != 0)
		m0_fom_phase_move(&cp->c_fom, rc, M0_CCP_FAIL);
	return result;
}

M0_INTERNAL int m0_dix_cm_cp_io_wait(struct m0_cm_cp    *cp,
				     struct m0_fop_type *ft)
{
	struct m0_fom       *fom = &cp->c_fom;
	struct m0_be_tx     *tx = m0_fom_tx(fom);
	struct m0_dix_cm_cp *dix_cp = cp2dixcp(cp);
	int                  result = M0_FSO_AGAIN;
	int                  rc = 0;

	M0_ENTRY("cp=%p, ft '%s'", cp, ft->ft_name);

	if (m0_be_tx_state(tx) != M0_BTS_DONE) {
		if (m0_be_tx_state(tx) == M0_BTS_FAILED)
			rc = dix_cm_cp_dtx_failure(cp);
		else
			result = dix_cm_fom_tx_wait(fom);
	} else {
		rc = dix_cp->dc_ctg_op_rc;
		dix_cm_cp_reply_send(cp, ft, rc);
		dix_cm_cp_dtx_fini(cp);
		if (rc == 0) {
			cp->c_ops->co_complete(cp);
			m0_long_write_unlock(m0_ctg_lock(dix_cp->dc_ctg),
					     &dix_cp->dc_ctg_lock);
			M0_LOG(M0_DEBUG, "move to next state CPP_FINI");
			m0_fom_phase_set(fom, M0_CCP_FINI);
			result = M0_FSO_WAIT;
		}
	}
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "move to next state CPP_FAIL");
		m0_fom_phase_move(fom, rc, M0_CCP_FAIL);
	}
	return result;
}

static int dix_cm_repair_cp_io_wait(struct m0_cm_cp *cp)
{
	return m0_dix_cm_cp_io_wait(cp, &m0_dix_repair_cpx_reply_fopt);
}

static int dix_cm_rebalance_cp_io_wait(struct m0_cm_cp *cp)
{
	return m0_dix_cm_cp_io_wait(cp, &m0_dix_rebalance_cpx_reply_fopt);
}

M0_INTERNAL const struct m0_cm_cp_ops m0_dix_cm_repair_cp_ops = {
	.co_action = {
		[M0_CCP_INIT]         = &m0_dix_cm_cp_init,
		[M0_CCP_READ]         = &m0_dix_cm_cp_read,
		[M0_CCP_WRITE_PRE]    = &m0_dix_cm_cp_write_pre,
		[M0_CCP_TX_OPEN]      = &m0_dix_cm_cp_tx_open,
		[M0_CCP_WRITE]        = &m0_dix_cm_cp_write,
		[M0_CCP_TX_DONE]      = &m0_dix_cm_cp_tx_done,
		[M0_CCP_IO_WAIT]      = &dix_cm_repair_cp_io_wait,
		[M0_CCP_XFORM]        = &m0_dix_cm_cp_xform,
		[M0_CCP_SW_CHECK]     = &m0_dix_cm_cp_sw_check,
		[M0_CCP_SEND]         = &m0_dix_cm_repair_cp_send,
		[M0_CCP_SEND_WAIT]    = &m0_dix_cm_cp_send_wait,
		[M0_CCP_RECV_INIT]    = &m0_dix_cm_cp_recv_init,
		[M0_CCP_RECV_WAIT]    = &m0_dix_cm_cp_recv_wait,
		[M0_CCP_FAIL]         = &dix_cm_repair_cp_fail,
		[M0_CCP_FINI]         = &m0_dix_cm_cp_fini,
	},
	.co_action_nr            = M0_CCP_NR,
	.co_phase_next           = NULL,
	.co_invariant            = &m0_dix_cm_cp_invariant,
	.co_home_loc_helper      = &m0_dix_cm_cp_home_loc_helper,
	.co_complete             = &m0_dix_cm_cp_complete,
	.co_free                 = &m0_dix_cm_cp_free,
};

M0_INTERNAL const struct m0_cm_cp_ops m0_dix_cm_rebalance_cp_ops = {
	.co_action = {
		[M0_CCP_INIT]         = &m0_dix_cm_cp_init,
		[M0_CCP_READ]         = &m0_dix_cm_cp_read,
		[M0_CCP_WRITE_PRE]    = &m0_dix_cm_cp_write_pre,
		[M0_CCP_TX_OPEN]      = &m0_dix_cm_cp_tx_open,
		[M0_CCP_WRITE]        = &m0_dix_cm_cp_write,
		[M0_CCP_TX_DONE]      = &m0_dix_cm_cp_tx_done,
		[M0_CCP_IO_WAIT]      = &dix_cm_rebalance_cp_io_wait,
		[M0_CCP_XFORM]        = &m0_dix_cm_cp_xform,
		[M0_CCP_SW_CHECK]     = &m0_dix_cm_cp_sw_check,
		[M0_CCP_SEND]         = &m0_dix_cm_rebalance_cp_send,
		[M0_CCP_SEND_WAIT]    = &m0_dix_cm_cp_send_wait,
		[M0_CCP_RECV_INIT]    = &m0_dix_cm_cp_recv_init,
		[M0_CCP_RECV_WAIT]    = &m0_dix_cm_cp_recv_wait,
		[M0_CCP_FAIL]         = &dix_cm_rebalance_cp_fail,
		[M0_CCP_FINI]         = &m0_dix_cm_cp_fini,
	},
	.co_action_nr            = M0_CCP_NR,
	.co_phase_next           = NULL,
	.co_invariant            = &m0_dix_cm_cp_invariant,
	.co_home_loc_helper      = &m0_dix_cm_cp_home_loc_helper,
	.co_complete             = &m0_dix_cm_cp_complete,
	.co_free                 = &m0_dix_cm_cp_free,
};

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
