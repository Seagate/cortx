/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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
 * Original authors: James Morse   <james_morse@xyratex.com>,
 *                   Juan Gonzalez <juan_gonzalez@xyratex.com>,
 *                   Sining Wu     <sining_wu@xyratex.com>
 * Original creation date: 09-Apr-2014
 */
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_COB
#include "fop/fop.h" /* m0_fom */

#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/trace.h"

#include "mdservice/fsync_foms.h"
#include "mdservice/fsync_fops.h"
#include "ioservice/io_fops.h"     /* m0_fop_fsync_ios_fopt */
#include "cas/cas.h"               /* m0_fop_fsync_cas_fopt */
#include "rpc/rpc_opcodes.h"
#include "ioservice/storage_dev.h" /* m0_storage_dev */
#include "mero/setup.h"            /* m0_cs_storage_devs_get */

#include "fop/fom_generic.h"
#include "be/domain.h" /* m0_be_domain */
#include "be/engine.h" /* m0_be_engine__tx_find */
#include "reqh/reqh.h" /* m0_reqh */


static void fsync_fom_fini(struct m0_fom *fom);
static int fsync_fom_tick(struct m0_fom *fom);
static size_t fsync_fom_locality_get(const struct m0_fom *fom);

/**
 * fsync fom operations
 */
static const struct m0_fom_ops fsync_fom_ops = {
	.fo_fini          = fsync_fom_fini,
	.fo_tick          = fsync_fom_tick,
	.fo_home_locality = fsync_fom_locality_get
};

/**
 * Phase names of an fsync fom
 */
enum fsync_fom_phase {
	M0_FOPH_FSYNC_FOM_START = M0_FOPH_NR + 1,
	M0_FOPH_FSYNC_FOM_WAIT,
	M0_FOPH_FSYNC_DATASYNC,
};

/**
 * Phases of an fsync fom
 */
struct m0_sm_state_descr m0_fsync_fom_phases[] = {
	[M0_FOPH_FSYNC_FOM_START] = {
		.sd_name    = "start",
		.sd_allowed = M0_BITS(M0_FOPH_FSYNC_FOM_WAIT,
				      M0_FOPH_FSYNC_DATASYNC,
				      M0_FOPH_SUCCESS,
				      M0_FOPH_FAILURE)
	},
	[M0_FOPH_FSYNC_FOM_WAIT] = {
		.sd_name    = "wait",
		.sd_allowed = M0_BITS(M0_FOPH_FAILURE,
				      M0_FOPH_FSYNC_FOM_WAIT,
				      M0_FOPH_FSYNC_DATASYNC,
				      M0_FOPH_SUCCESS)
	},
	[M0_FOPH_FSYNC_DATASYNC] = {
		.sd_name    = "datasync",
		.sd_allowed = M0_BITS(M0_FOPH_FAILURE,
				      M0_FOPH_SUCCESS)
	},
};

/**
 * Valid phase transitions for an fsync fom.
 */
static struct m0_sm_trans_descr fsync_fom_phases_trans[] = {
	[ARRAY_SIZE(m0_generic_phases_trans)] =
	{ "tx wait",      M0_FOPH_FSYNC_FOM_START, M0_FOPH_FSYNC_FOM_WAIT},
	{ "start tx=0",   M0_FOPH_FSYNC_FOM_START, M0_FOPH_FSYNC_DATASYNC},
	{ "start failed", M0_FOPH_FSYNC_FOM_START, M0_FOPH_FAILURE},
	{ "no wait",      M0_FOPH_FSYNC_FOM_START, M0_FOPH_SUCCESS},
	{ "wait failed",  M0_FOPH_FSYNC_FOM_WAIT,  M0_FOPH_FAILURE},
	{ "wait more",    M0_FOPH_FSYNC_FOM_WAIT,  M0_FOPH_FSYNC_FOM_WAIT},
	{ "wait done",    M0_FOPH_FSYNC_FOM_WAIT,  M0_FOPH_SUCCESS},
	{ "sync",         M0_FOPH_FSYNC_FOM_WAIT,  M0_FOPH_FSYNC_DATASYNC},
	{ "sync done",    M0_FOPH_FSYNC_DATASYNC,  M0_FOPH_SUCCESS},
	{ "sync failed",  M0_FOPH_FSYNC_DATASYNC,  M0_FOPH_FAILURE},
};

/**
 * This object defines the phases of an fsync fom. It is used when initializing
 * an fsync fom.
 *
 * An fsync fom has two different phases:
 *
 *	- M0_FOPH_FSYNC_FOM_START: This is the initial state for any fsync fom.
 *	In this phase, the fom knows its tick function has not been invoked
 *	before. Only if the fsync request includes the M0_FSYNC_MODE_ACTIVE
 *	mode, the target transaction is forced. In any case, after the first
 *	tick the machine's phase changes to M0_FOPH_FSYNC_FOM_WAIT.
 *
 *	- M0_FOPH_FSYNC_FOM_WAIT: The fom waits on the target transaction until
 *	its state changes to M0_BTS_LOGGED.
 */
M0_INTERNAL struct m0_sm_conf m0_fsync_fom_conf = {
	.scf_name      = "fsync",
	.scf_nr_states = ARRAY_SIZE(m0_fsync_fom_phases),
	.scf_state     = m0_fsync_fom_phases,
	.scf_trans_nr  = ARRAY_SIZE(fsync_fom_phases_trans),
	.scf_trans     = fsync_fom_phases_trans,
};

/**
 * Gets the transaction an fsync fop request is targeted to.
 *
 * @param fom fom associated to the fsync fop request being processed.
 * @param txid ID of the target transaction.
 * @return A pointer to the target transaction or NULL if the operation failed.
 * @remark This function acquires a reference on the target transaction. Call
 * m0_be_tx_put once you are done with the tx, so it can be eventually
 * completed.
 */
static struct m0_be_tx *fsync_target_tx_get(struct m0_fom *fom,
					    uint64_t       txid)
{
	struct m0_be_engine *en;

	M0_PRE(fom != NULL);
	en = &m0_fom_reqh(fom)->rh_beseg->bs_domain->bd_engine;
	return m0_be_engine__tx_find(en, txid);
}

/**
 * Controls the simple logic of the fsync fom's state machine.
 * If the target transaction is already logged, the processing of the fsync
 * fop is finished.
 * If, on the contrary, the target transaction has not been logged yet, the
 * fsync fom performs the following actions depending on the phase of the
 * fsync state machine:
 *
 * - M0_FOPH_FSYNC_FOM_START: If the fop request includes the M0_BT_ACTIVE
 *   mode flag, the target transaction gets forced. The fom's phase then
 *   changes to M0_FOPH_FSYNC_FOM_WAIT.
 * - M0_FOPH_FSYNC_FOM_WAIT: The fsync fom registers itself to be notified when
 *   the state of the target transaction changes and then waits. Whenever the
 *   target transaction gets finally logged, the fsync fom gets woken up so
 *   it can complete the processing of the fsync request and send the
 *   corresponding fsync reply.
 *
 * @param fom fsync fom processing the fsync FOP request.
 * @return Always M0_FSO_WAIT. Note that if the processing of the fsync fop
 * is considered completed, the phase of the fsync fom is set to
 * M0_FOPH_FSYNC_FOM_FINISH. In this later case, a return code is included
 * in the fsync fop reply.
 */
static int fsync_fom_tick(struct m0_fom *fom)
{
	/* return value included in the fsync reply */
	int                             rc;
	struct m0_reqh_context         *rctx;
	struct m0_fop_fsync_rep        *rep;
	struct m0_fop_fsync            *req;
	struct m0_be_tx                *target_tx;
	int                             phase;
	uint64_t                        txid;

	M0_ENTRY();

	M0_PRE(fom != NULL);

	req = m0_fop_data(fom->fo_fop);
	M0_ASSERT(req != NULL);
	rep = m0_fop_data(fom->fo_rep_fop);
	M0_ASSERT(rep != NULL);

	M0_ASSERT(M0_IN(req->ff_fsync_mode,
			(M0_FSYNC_MODE_ACTIVE, M0_FSYNC_MODE_PASSIVE)));

	txid = req->ff_be_remid.tri_txid;
	M0_LOG(M0_DEBUG, "Target tx:%lu\n", txid);
	rctx = container_of(m0_fom_reqh(fom), struct m0_reqh_context, rc_reqh);


	phase = m0_fom_phase(fom);

	if (phase < M0_FOPH_NR) {
		M0_LEAVE("fom is in a generic phase");
		return m0_fom_tick_generic(fom);
	}

	M0_ASSERT(M0_IN(phase,
			(M0_FOPH_FSYNC_FOM_START,
			 M0_FOPH_FSYNC_FOM_WAIT,
			 M0_FOPH_FSYNC_DATASYNC)));

	if (phase == M0_FOPH_FSYNC_DATASYNC) {
		struct m0_storage_devs *sdevs = m0_cs_storage_devs_get();

		m0_fom_block_enter(fom);

		m0_storage_devs_lock(sdevs);
		rc = m0_storage_devs_fdatasync(sdevs);
		m0_storage_devs_unlock(sdevs);

		m0_fom_block_leave(fom);

		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);

		return M0_FSO_AGAIN;
	}

	/* we can get the target tx because we're at the same locality */
	target_tx = fsync_target_tx_get(fom, txid);
	if (target_tx == NULL) {
		/*
		 * XXX: when we finally get a last committed tx from the
		 * engine, use it to return an error if the value received
		 * is greater than the value in the engine(ahead in the future).
		 */
		M0_LOG(M0_DEBUG, "tx not found:%lu\n", txid);
		rc = 0;
		goto fsync_done;
	}

	/* if the target tx has been logged, we're done */
	if (m0_be_tx_state(target_tx) >= M0_BTS_LOGGED) {
		M0_LOG(M0_DEBUG, "TX has been logged:%lu\n", target_tx->t_id);
		m0_be_tx_put(target_tx);
		rc = 0;
		goto fsync_done;
	}

	/* we must wait on the target tx */
	switch (phase) {
	case M0_FOPH_FSYNC_FOM_START:
		/* maybe the tx must be forced */
		if (req->ff_fsync_mode == M0_FSYNC_MODE_ACTIVE) {
			M0_LOG(M0_DEBUG, "force tx:%lu\n", target_tx->t_id);
			m0_be_tx_force(target_tx);
		}

		m0_fom_phase_set(fom, M0_FOPH_FSYNC_FOM_WAIT);
		M0_LOG(M0_DEBUG, "fom is now waiting for the target tx\n");
		/* no 'break' so the following code gets executed */

	case M0_FOPH_FSYNC_FOM_WAIT:
		m0_fom_wait_on(fom, &target_tx->t_sm.sm_chan, &fom->fo_cb);
		break;

	default:
		M0_IMPOSSIBLE("wrong  phase");
		break;
	}

	/* release the reference so the tx can be moved to M0_BTS_DONE */
	m0_be_tx_put(target_tx);

	M0_LEAVE("wait for the target tx to be logged");
	return M0_FSO_WAIT;

fsync_done:
	rep->ffr_rc = rc;
	/*
	 * XXX:As an optimization, ffr_be_remid should contain the
	 * ID of the last committed tx (retrieved from the be engine)
	 */
	rep->ffr_be_remid.tri_txid = txid;

	if (!rctx->rc_disable_direct_io)
		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS,
				    M0_FOPH_FAILURE);
	else
		m0_fom_phase_moveif(fom, rc, M0_FOPH_FSYNC_DATASYNC,
				    M0_FOPH_FAILURE);

	M0_LEAVE("fsync fop request processed");

	/* some other generic phases will be executed now */
	return M0_FSO_AGAIN;
}

/**
 * Returns the locality of an fsync fom. This affects the processor where the
 * fom is to be processed.
 * @param fom fom whose locality is to be retrieved.
 * @return A locality ID.
 */
static size_t fsync_fom_locality_get(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_fop != NULL);

	return ((struct m0_fop_fsync *)
		m0_fop_data(fom->fo_fop))->ff_be_remid.tri_locality;
}

/**
 * Finalizes and releases an fsync fom.
 * @param fom fsync fom to be released.
 */
static void fsync_fom_fini(struct m0_fom *fom)
{
	M0_PRE(fom != NULL);
	m0_fom_fini(fom);
}

/**
 * Creates a new fsync fom for a given fsync fop request.
 */
M0_INTERNAL int m0_fsync_req_fom_create(struct m0_fop  *fop,
					struct m0_fom **out,
					struct m0_reqh *reqh)
{
	struct m0_fop   *rep_fop;
	struct m0_fom   *fom;

	M0_ENTRY();

	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);
	M0_PRE(M0_IN(fop->f_type,
		     (&m0_fop_fsync_mds_fopt, &m0_fop_fsync_ios_fopt,
		      &m0_fop_fsync_cas_fopt)));
	M0_PRE(M0_IN(m0_fop_opcode(fop),
		     (M0_FSYNC_MDS_OPCODE, M0_FSYNC_IOS_OPCODE,
		      M0_FSYNC_CAS_OPCODE)));

	/* allocate the fom */
	M0_ALLOC_PTR(fom);
	if (fom == NULL) {
		M0_LEAVE("unable to allocate fom");
		return M0_ERR_INFO(-ENOMEM, "unable to allocate fom");
	}

	/* create the fsync reply */
	rep_fop = m0_fop_reply_alloc(fop, &m0_fop_fsync_rep_fopt);
	if (rep_fop == NULL) {
		m0_free(fom);
		M0_LEAVE("unable to allocate fop reply");
		return M0_ERR_INFO(-ENOMEM, "unable to allocate fop reply");
	}

	/* init the fom */
	m0_fom_init(fom, &fop->f_type->ft_fom_type,
		    &fsync_fom_ops, fop, rep_fop, reqh);
	*out = fom;

	M0_LEAVE();
	return M0_RC(0);
}

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
