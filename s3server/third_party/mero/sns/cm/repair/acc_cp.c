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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 13/02/2013
 */

#ifndef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#endif
#include "lib/trace.h"
#include "lib/memory.h" /* m0_free() */
#include "lib/misc.h"

#include "fop/fom.h"
#include "cob/cob.h"
#include "reqh/reqh.h"

#include "sns/cm/cp.h"
#include "sns/cm/cm.h"
#include "sns/cm/repair/ag.h"
#include "sns/cm/cm_utils.h"

/**
  Implements accumulator copy packet for an aggregation group.
  Accumulator copy packet is initialised when an aggregation group is created.
  Data buffers for accumulator copy packets are pre-acquired in context of the
  sns copy machine iterator. The number of accumulator copy packets in an
  aggregation group is equivalent to the total number of failed units in an
  aggregation group. In case of multiple accumulator copy packets, an
  accumulator copy packet is chosen based on the index of failed unit in an
  aggregation group to be recovered.

  @addtogroup SNSCMCP
  @{
*/

M0_INTERNAL int m0_sns_cm_repair_cp_xform(struct m0_cm_cp *cp);
M0_INTERNAL int m0_sns_cm_repair_cp_send(struct m0_cm_cp *cp);
M0_INTERNAL int m0_sns_cm_repair_cp_recv_wait(struct m0_cm_cp *cp);

static void acc_cp_free(struct m0_cm_cp *cp)
{
	struct m0_sns_cm_repair_ag             *rag = sag2repairag(ag2snsag(cp->c_ag));
	struct m0_sns_cm_repair_ag_failure_ctx *fc;

	M0_PRE(cp != NULL);

	m0_sns_cm_cp_buf_release(cp);
	fc = container_of(cp2snscp(cp), struct m0_sns_cm_repair_ag_failure_ctx,
			  fc_tgt_acc_cp);
	if (fc->fc_is_inuse && fc->fc_is_active) {
		M0_CNT_INC(rag->rag_acc_freed);
		fc->fc_is_active = false;
		m0_cm_ag_cp_del(cp->c_ag, cp);
	}
}

static int acc_cp_fini(struct m0_cm_cp *cp)
{

	return 0;
}

const struct m0_cm_cp_ops m0_sns_cm_acc_cp_ops = {
	.co_action = {
		[M0_CCP_INIT]          = &m0_sns_cm_cp_init,
		[M0_CCP_READ]          = &m0_sns_cm_cp_read,
		[M0_CCP_WRITE_PRE]     = &m0_sns_cm_cp_write_pre,
		[M0_CCP_WRITE]         = &m0_sns_cm_cp_write,
		[M0_CCP_IO_WAIT]       = &m0_sns_cm_cp_io_wait,
		[M0_CCP_XFORM]         = &m0_sns_cm_repair_cp_xform,
		[M0_CCP_SW_CHECK]      = &m0_sns_cm_cp_sw_check,
		[M0_CCP_SEND]          = &m0_sns_cm_repair_cp_send,
		[M0_CCP_SEND_WAIT]     = &m0_sns_cm_cp_send_wait,
		[M0_CCP_RECV_INIT]     = &m0_sns_cm_cp_recv_init,
		[M0_CCP_RECV_WAIT]     = &m0_sns_cm_cp_recv_wait,
		[M0_CCP_FAIL]          = &m0_sns_cm_cp_fail,
		/* To satisfy the m0_cm_cp_invariant() */
		[M0_CCP_FINI]          = &acc_cp_fini,
	},
	.co_action_nr            = M0_CCP_NR,
	.co_phase_next           = &m0_sns_cm_cp_phase_next,
	.co_invariant            = &m0_sns_cm_cp_invariant,
	.co_home_loc_helper      = &cp_home_loc_helper,
	.co_complete             = &m0_sns_cm_cp_complete,
	.co_free                 = &acc_cp_free,
};

/**
 * Initialises accumulator copy packet and its corresponding FOM.
 */
M0_INTERNAL void m0_sns_cm_acc_cp_init(struct m0_sns_cm_cp *scp,
				       struct m0_sns_cm_ag *sag)
{
	struct m0_cm_cp *cp = &scp->sc_base;

        M0_PRE(scp != NULL);

	m0_cm_ag_cp_add(&sag->sag_base, cp);
	cp->c_ops = &m0_sns_cm_acc_cp_ops;
	/*
	 * Initialise the bitmap representing the copy packets
	 * which will be transformed into the resultant copy
	 * packet.
	 */
	m0_bitmap_init(&scp->sc_base.c_xform_cp_indices,
		       sag->sag_base.cag_cp_global_nr);
        m0_cm_cp_fom_init(sag->sag_base.cag_cm, cp, NULL, NULL);
}

/**
 * Configures accumulator copy packet and acquires data buffers.
 */
M0_INTERNAL int m0_sns_cm_acc_cp_setup(struct m0_sns_cm_cp *scp,
				       struct m0_fid *tgt_cobfid,
				       uint64_t tgt_cob_index,
				       uint64_t failed_unit_idx,
				       uint64_t data_seg_nr)
{
	struct m0_sns_cm_ag                    *sag = ag2snsag(scp->sc_base.c_ag);
	struct m0_cm                           *cm = sag->sag_base.cag_cm;
	struct m0_sns_cm_repair_ag_failure_ctx *rag_fc;

        M0_PRE(scp != NULL && sag != NULL);
	M0_PRE(m0_cm_is_locked(cm));

	if (!sag->sag_base.cag_has_incoming)
		scp->sc_is_local = true;
	scp->sc_is_acc = true;
	rag_fc = M0_AMB(rag_fc, scp, fc_tgt_acc_cp);
	return m0_sns_cm_cp_setup(scp, tgt_cobfid, tgt_cob_index, data_seg_nr,
				  failed_unit_idx, rag_fc->fc_tgt_idx);
}

/** @} SNSCMCP */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
