/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mandar Sawant <Mandar_Sawant@xyratex.com>
 *		    Madhavrao Vemuri <madhav_vemuri@xyratex.com>
 * Original creation date: 07/19/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FOP
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/string.h"
#include "stob/stob.h"
#include "net/net.h"
#include "fop/fop.h"
#include "dtm/dtm.h"
#include "rpc/rpc.h"
#include "rpc/rpc_opcodes.h"    /* M0_REQH_ERROR_REPLY_OPCODE */
#include "rpc/item_internal.h"  /* m0_rpc_item_is_update */
#include "reqh/reqh.h"
#include "fop/fom_generic.h"
#include "fop/fom_generic_xc.h"
#include "addb2/addb2.h"
#include "addb2/identifier.h"

/**
   @addtogroup fom
   @{
 */

struct m0_fop_type m0_fop_generic_reply_fopt;
M0_EXPORTED(m0_fop_generic_reply_fopt);

M0_INTERNAL void m0_fom_generic_fini(void)
{
	m0_fop_type_fini(&m0_fop_generic_reply_fopt);
}

M0_INTERNAL int m0_fom_generic_init(void)
{
	M0_FOP_TYPE_INIT(&m0_fop_generic_reply_fopt,
			 .name      = "generic-reply",
			 .opcode    = M0_REQH_ERROR_REPLY_OPCODE,
			 .xt        = m0_fop_generic_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	return 0;
}

M0_INTERNAL void m0_fom_mod_rep_fill(struct m0_fop_mod_rep *rep,
				     struct m0_fom *fom)
{
	rep->fmr_remid.tri_txid = m0_fom_tx(fom)->t_id;
	rep->fmr_remid.tri_locality = fom->fo_ops->fo_home_locality(fom);
}

bool m0_rpc_item_is_generic_reply_fop(const struct m0_rpc_item *item)
{
	return item->ri_type == &m0_fop_generic_reply_fopt.ft_rpc_item_type;
}
M0_EXPORTED(m0_rpc_item_is_generic_reply_fop);

int32_t m0_rpc_item_generic_reply_rc(const struct m0_rpc_item *reply)
{
	struct m0_fop_generic_reply *reply_fop;
	int32_t                      rc;

	M0_PRE(reply != NULL);
	if (m0_rpc_item_is_generic_reply_fop(reply)) {
		reply_fop = m0_fop_data(m0_rpc_item_to_fop(reply));
		rc = reply_fop->gr_rc;
		if (rc != 0)
			M0_LOG(M0_ERROR, "Receiver reported error: %d \"%s\"",
			       rc,
			       (char*)reply_fop->gr_msg.s_buf ?: strerror(rc));
		return M0_RC(rc);
	} else
		return 0;
}
M0_EXPORTED(m0_rpc_item_generic_reply_rc);

static bool fom_is_update(const struct m0_fom *fom)
{
	return m0_rpc_item_is_update(m0_fop_to_rpc_item(fom->fo_fop));
}

/**
 * Fom phase descriptor structure, helps to transition fom
 * through its standard phases
 */
struct fom_phase_desc {
	/**
	   Perfoms actions corresponding to a particular standard fom
	   phase, as defined.

	   @retval returns M0_FSO_AGAIN, this transitions fom to its next phase

	   @see m0_fom_tick_generic()
	 */
	int (*fpd_action) (struct m0_fom *fom);
	/**
	   Next phase the fom should transition into, after successfully
	   completing the current phase execution.
	 */
	int		   fpd_nextphase;
	/**
	   Fom phase name in user readable format.
	 */
	const char	  *fpd_name;
	/**
	   Bitmap representation of the fom phase.
	   This is used in pre condition checks before executing
	   fom phase action.

	   @see m0_fom_tick_generic()
	 */
	uint64_t	   fpd_pre_phase;
};

/**
 * Begins fom execution, transitions fom to its first
 * standard phase.
 *
 * @see m0_fom_tick_generic()
 *
 * @retval M0_FSO_AGAIN, to execute next fom phase
 */
static int fom_phase_init(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Performs authenticity checks on fop,
 * executed by the fom.
 */
static int fom_authen(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing a blocking operation
 * in M0_FOPH_AUTHENTICATE phase.
 */
static int fom_authen_wait(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Identifies local resources required for fom
 * execution.
 */
static int fom_loc_resource(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing a blocking operation
 * in M0_FOPH_RESOURCE_LOCAL phase.
 */
static int fom_loc_resource_wait(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Identifies distributed resources required for fom execution.
 */
static int fom_dist_resource(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing blocking operation
 * in M0_FOPH_RESOURCE_DISTRIBUTED_PHASE.
 */
static int fom_dist_resource_wait(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Locates and loads filesystem objects affected by
 * fop executed by this fom.
 */
static int fom_obj_check(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing blocking operation
 * in M0_FOPH_OBJECT_CHECK.
 */
static int fom_obj_check_wait(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Performs authorisation checks on behalf of the user,
 * accessing the file system objects affected by
 * the fop.
 */
static int fom_auth(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing a blocking operation
 * M0_FOPH_AUTHORISATION phase.
 */
static int fom_auth_wait(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Initialize fom local transactional context, the fom operations
 * are executed in this context.
 * After fom execution is completed the transaction is committed.
 */
static int fom_tx_init(struct m0_fom *fom)
{
	uint64_t tx_sm_id;
	uint64_t phase_sm_id;

	if (fom_is_update(fom)) {
		m0_dtx_init(&fom->fo_tx, m0_fom_reqh(fom)->rh_beseg->bs_domain,
			    &fom->fo_loc->fl_group);

		phase_sm_id = m0_sm_id_get(&fom->fo_sm_phase);
		tx_sm_id = m0_sm_id_get(&fom->fo_tx.tx_betx.t_sm);
		M0_ADDB2_ADD(M0_AVI_FOM_TO_TX, phase_sm_id, tx_sm_id);
	}
	return M0_FSO_AGAIN;
}

/**
 * Creates fom local transactional context.
 * Add a fol record fragment for the fop to the transaction.
 */
static int fom_tx_open(struct m0_fom *fom)
{
	if (fom_is_update(fom)) {
		struct m0_dtx *dtx = &fom->fo_tx;

		m0_be_tx_payload_prep(m0_fom_tx(fom), FOL_REC_MAXSIZE);
		if (!fom->fo_local) {
			int rc;
			rc = m0_fop_fol_add(fom->fo_fop, fom->fo_rep_fop, dtx);
			if (rc < 0)
				return M0_RC(rc);
		}
		m0_dtx_open(dtx);
	}
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing a blocking operation,
 * issued at the M0_FOPH_TXN_OPEN phase.
 */
static int fom_tx_wait(struct m0_fom *fom)
{
	if (fom_is_update(fom)) {
		struct m0_be_tx *tx = m0_fom_tx(fom);

		M0_ENTRY("fom=%p, tx_state %d", fom, m0_be_tx_state(tx));
		M0_PRE(M0_IN(m0_be_tx_state(tx), (M0_BTS_OPENING,
						  M0_BTS_GROUPING,
						  M0_BTS_ACTIVE,
						  M0_BTS_FAILED)));

		if (m0_be_tx_state(tx) == M0_BTS_FAILED)
			return M0_RC(tx->t_sm.sm_rc);
		else if (M0_IN(m0_be_tx_state(tx), (M0_BTS_OPENING,
						    M0_BTS_GROUPING))) {
			m0_fom_wait_on(fom, &tx->t_sm.sm_chan, &fom->fo_cb);
			M0_LEAVE();
			return M0_FSO_WAIT;
		} else
			m0_dtx_opened(&fom->fo_tx);
	}
	M0_LEAVE();
	return M0_FSO_AGAIN;
}

/**
 * Allocates generic reqh error reply fop and sets the same
 * into fom->fo_rep_fop.
 */
static void generic_reply_build(struct m0_fom *fom)
{
	struct m0_fop               *rfop = fom->fo_rep_fop;
	struct m0_fop_generic_reply *out_fop;

	if (rfop == NULL)
		rfop = m0_fop_reply_alloc(fom->fo_fop,
					  &m0_fop_generic_reply_fopt);
	if (rfop != NULL) {
		fom->fo_rep_fop = rfop;
		M0_PRE(rfop->f_type->ft_xt->xct_child[0].xf_type == &M0_XT_U32);
		out_fop = m0_fop_data(rfop);
		out_fop->gr_rc = m0_fom_rc(fom);
	}
}

/**
 * Handles fom execution failure, if fom fails in one of
 * the standard phases, then we construct a generic error
 * reply fop and assign it to m0_fom::fo_rep_fop, else if
 * fom fails in fop specific operation, then fom should
 * already contain a fop specific error reply provided by
 * fop specific operation.
 */
static int fom_failure(struct m0_fom *fom)
{
	int            rc = m0_fom_rc(fom);
	struct m0_dtx *tx = &fom->fo_tx;

	if (rc != 0) {
		M0_LOG(M0_NOTICE, "fom_rc=%d", rc);
		generic_reply_build(fom);
	}
	/*
	 * If transaction was initialised, but not opened, finalise it.
	 */
	if (tx->tx_state == M0_DTX_INIT) {
		struct m0_be_tx *betx = &tx->tx_betx;
		/**
		 * @todo hack to move be transaction into FAILED state, so that
		 * it can be finalised.
		 */
		if (m0_be_tx_state(betx) == M0_BTS_PREPARE) {
			m0_be_tx_prep(betx, &m0_be_tx_credit_invalid);
			m0_be_tx_open(betx);
		}
		M0_ASSERT(m0_be_tx_state(betx) == M0_BTS_FAILED);
		m0_dtx_fini(tx);
	}
	return M0_FSO_AGAIN;
}

/**
 * Fom execution is successful.
 */
static int fom_success(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Make a FOL transaction record
 */
static int fom_fol_rec_add(struct m0_fom *fom)
{
	if (fom_is_update(fom) &&
	    !fom->fo_local && fom->fo_tx.tx_state == M0_DTX_OPEN) {
		int rc = m0_fom_fol_rec_add(fom);
		if (rc < 0)
			return M0_RC(rc);
	}
	return M0_FSO_AGAIN;
}

/**
 * Commits local fom transactional context if fom
 * execution is successful.
 */
static int fom_tx_commit(struct m0_fom *fom)
{
	struct m0_dtx   *dtx = &fom->fo_tx;
	struct m0_be_tx *tx  = m0_fom_tx(fom);

	if (!fom_is_update(fom))
		;
	else if (dtx->tx_state == M0_DTX_INIT) {
		m0_dtx_fini(dtx);
	} else if (M0_IN(dtx->tx_state, (M0_DTX_OPEN, M0_DTX_DONE))){
		M0_ASSERT(m0_be_tx_state(tx) == M0_BTS_ACTIVE);
		m0_dtx_done(dtx);
	}
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing a blocking operation
 * in M0_FOPH_TXN_COMMIT phase.
 */
M0_INTERNAL int m0_fom_tx_commit_wait(struct m0_fom *fom)
{
	struct m0_dtx   *dtx = &fom->fo_tx;
	struct m0_be_tx *tx = m0_fom_tx(fom);

	if (!fom_is_update(fom))
		;
	else if (dtx->tx_state == M0_DTX_DONE) {
		if (m0_be_tx_state(tx) == M0_BTS_DONE) {
			m0_dtx_fini(&fom->fo_tx);
			return M0_FSO_AGAIN;
		}
		m0_fom_wait_on(fom, &tx->t_sm.sm_chan, &fom->fo_cb);
		return M0_FSO_WAIT;
	}

	return M0_FSO_AGAIN;
}

/**
 * Posts reply fop, if the fom execution was done locally,
 * reply fop is cached until the changes are integrated
 * with the server.
 *
 * @pre fom->fo_rep_fop != NULL
 *
 * @todo Implement write back cache, during which we may perform updates on
 *       local objects and re-integrate with the server later, in that case we
 *       may block while, we caching fop, this requires more additions to the
 *       routine.
 */
static int fom_queue_reply(struct m0_fom *fom)
{
	M0_PRE(fom->fo_rep_fop != NULL);

	M0_LOG(M0_DEBUG, "request %p[%u], reply %p, reply->ri_error %d",
	       m0_fop_to_rpc_item(fom->fo_fop),
	       m0_fop_to_rpc_item(fom->fo_fop)->ri_type->rit_opcode,
	       m0_fop_to_rpc_item(fom->fo_rep_fop),
	       m0_fop_to_rpc_item(fom->fo_rep_fop)->ri_error);
	if (!fom->fo_local)
		m0_rpc_reply_post(m0_fop_to_rpc_item(fom->fo_fop),
				  m0_fop_to_rpc_item(fom->fo_rep_fop));
	return M0_FSO_AGAIN;
}

/**
 * Resumes fom execution after completing a blocking operation
 * in M0_FOPH_QUEUE_REPLY phase.
 */
static int fom_queue_reply_wait(struct m0_fom *fom)
{
	if (fom->fo_tx.tx_state == M0_DTX_INIT) {
		m0_fom_phase_set(fom, M0_FOPH_FINISH);
		return M0_FSO_WAIT;
	}

	return M0_FSO_AGAIN;
}

static int fom_timeout(struct m0_fom *fom)
{
	return M0_FSO_AGAIN;
}

/**
 * Fom phase operations table, this defines a fom_phase_desc object
 * for every generic phase of the fom, containing a function pointer
 * to the phase handler, the next phase fom should transition into
 * and a phase name in user visible format in order to log addb event.
 *
 * @see struct fom_phase_desc
 */
static const struct fom_phase_desc fpd_table[] = {
	[M0_FOPH_INIT] =	      { &fom_phase_init,
					M0_FOPH_AUTHENTICATE, "init",
					M0_BITS(M0_FOPH_INIT) },
	[M0_FOPH_AUTHENTICATE] =      { &fom_authen,
					M0_FOPH_RESOURCE_LOCAL, "authen",
					M0_BITS(M0_FOPH_AUTHENTICATE) },
	[M0_FOPH_AUTHENTICATE_WAIT] = { &fom_authen_wait,
					M0_FOPH_RESOURCE_LOCAL, "authen_wait",
					M0_BITS(M0_FOPH_AUTHENTICATE_WAIT)},
	[M0_FOPH_RESOURCE_LOCAL] =    { &fom_loc_resource,
					M0_FOPH_RESOURCE_DISTRIBUTED,
					"local_resource",
					M0_BITS(M0_FOPH_RESOURCE_LOCAL) },
	[M0_FOPH_RESOURCE_LOCAL_WAIT] = { &fom_loc_resource_wait,
					  M0_FOPH_RESOURCE_DISTRIBUTED,
					  "local_resource_wait",
					 M0_BITS(M0_FOPH_RESOURCE_LOCAL_WAIT) },
	[M0_FOPH_RESOURCE_DISTRIBUTED] = { &fom_dist_resource,
					   M0_FOPH_OBJECT_CHECK,
					   "dist_resource",
					M0_BITS(M0_FOPH_RESOURCE_DISTRIBUTED) },
	[M0_FOPH_RESOURCE_DISTRIBUTED_WAIT] = { &fom_dist_resource_wait,
					      M0_FOPH_OBJECT_CHECK,
					     "dist_resource_wait",
				   M0_BITS(M0_FOPH_RESOURCE_DISTRIBUTED_WAIT) },
	[M0_FOPH_OBJECT_CHECK] =      { &fom_obj_check,
					M0_FOPH_AUTHORISATION, "obj_check",
					M0_BITS(M0_FOPH_OBJECT_CHECK) },
	[M0_FOPH_OBJECT_CHECK_WAIT] = { &fom_obj_check_wait,
					M0_FOPH_AUTHORISATION, "obj_check_wait",
					M0_BITS(M0_FOPH_OBJECT_CHECK_WAIT) },
	[M0_FOPH_AUTHORISATION] =     { &fom_auth, M0_FOPH_TXN_INIT, "auth",
					M0_BITS(M0_FOPH_AUTHORISATION) },
	[M0_FOPH_AUTHORISATION_WAIT] = { &fom_auth_wait, M0_FOPH_TXN_INIT,
					 "auth_wait",
					 M0_BITS(M0_FOPH_AUTHORISATION_WAIT) },
	[M0_FOPH_TXN_INIT] =	      { &fom_tx_init, M0_FOPH_TXN_OPEN,
					"tx_init", M0_BITS(M0_FOPH_TXN_INIT) },
	[M0_FOPH_TXN_OPEN] =	      { &fom_tx_open, M0_FOPH_TXN_WAIT,
					"tx_open", M0_BITS(M0_FOPH_TXN_OPEN) },
	[M0_FOPH_TXN_WAIT] =	      { &fom_tx_wait, M0_FOPH_TYPE_SPECIFIC,
					"tx_wait", M0_BITS(M0_FOPH_TXN_WAIT) },
	[M0_FOPH_SUCCESS] =	      { &fom_success, M0_FOPH_FOL_REC_ADD,
					"success", M0_BITS(M0_FOPH_SUCCESS) },
	[M0_FOPH_FOL_REC_ADD] =       { &fom_fol_rec_add, M0_FOPH_TXN_COMMIT,
					"fol_rec_add",
					M0_BITS(M0_FOPH_FOL_REC_ADD) },
	[M0_FOPH_TXN_COMMIT] =	      { &fom_tx_commit, M0_FOPH_QUEUE_REPLY,
					"tx_commit",
					M0_BITS(M0_FOPH_TXN_COMMIT) },
	[M0_FOPH_QUEUE_REPLY] =       { &fom_queue_reply,
					M0_FOPH_QUEUE_REPLY_WAIT, "queue_reply",
					M0_BITS(M0_FOPH_QUEUE_REPLY) },
	[M0_FOPH_QUEUE_REPLY_WAIT] =  { &fom_queue_reply_wait,
					M0_FOPH_TXN_COMMIT_WAIT,
					"queue_reply_wait",
					M0_BITS(M0_FOPH_QUEUE_REPLY_WAIT) },
	[M0_FOPH_TXN_COMMIT_WAIT] =   { &m0_fom_tx_commit_wait, M0_FOPH_FINISH,
					"tx_commit_wait",
					M0_BITS(M0_FOPH_TXN_COMMIT_WAIT) },
	[M0_FOPH_TIMEOUT] =	      { &fom_timeout, M0_FOPH_FAILURE,
					"timeout", M0_BITS(M0_FOPH_TIMEOUT) },
	[M0_FOPH_FAILURE] =	      { &fom_failure, M0_FOPH_TXN_COMMIT,
					"failure", M0_BITS(M0_FOPH_FAILURE) },
};

/**
 * FOM generic phases, allowed transitions from each phase and their functions
 * are assigned to a state machine descriptor.
 * State name is used to log addb event.
 */
static struct m0_sm_state_descr generic_phases[] = {
	[M0_FOPH_INIT] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(M0_FOPH_AUTHENTICATE, M0_FOPH_FINISH,
					M0_FOPH_SUCCESS, M0_FOPH_FAILURE,
					M0_FOPH_TYPE_SPECIFIC)
	},
	[M0_FOPH_AUTHENTICATE] = {
		.sd_name      = "authen",
		.sd_allowed   = M0_BITS(M0_FOPH_AUTHENTICATE_WAIT,
					M0_FOPH_RESOURCE_LOCAL,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_AUTHENTICATE_WAIT] = {
		.sd_name      = "authen_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_RESOURCE_LOCAL, M0_FOPH_FAILURE)
	},
	[M0_FOPH_RESOURCE_LOCAL] = {
		.sd_name      = "local_resource",
		.sd_allowed   = M0_BITS(M0_FOPH_RESOURCE_LOCAL_WAIT,
					M0_FOPH_RESOURCE_DISTRIBUTED,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_RESOURCE_LOCAL_WAIT] = {
		.sd_name      = "local_resource_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_RESOURCE_DISTRIBUTED,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_RESOURCE_DISTRIBUTED] = {
		.sd_name      = "dist_resource",
		.sd_allowed   = M0_BITS(M0_FOPH_RESOURCE_DISTRIBUTED_WAIT,
					M0_FOPH_OBJECT_CHECK,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_RESOURCE_DISTRIBUTED_WAIT] = {
		.sd_name      = "dist_resource_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_OBJECT_CHECK, M0_FOPH_FAILURE)
	},
	[M0_FOPH_OBJECT_CHECK] = {
		.sd_name      = "obj_check",
		.sd_allowed   = M0_BITS(M0_FOPH_OBJECT_CHECK_WAIT,
					M0_FOPH_AUTHORISATION,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_OBJECT_CHECK_WAIT] = {
		.sd_name      = "obj_check_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_AUTHORISATION, M0_FOPH_FAILURE)
	},
	[M0_FOPH_AUTHORISATION] = {
		.sd_name      = "auth",
		.sd_allowed   = M0_BITS(M0_FOPH_AUTHORISATION_WAIT,
					M0_FOPH_TXN_INIT,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_AUTHORISATION_WAIT] = {
		.sd_name      = "auth_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_INIT, M0_FOPH_FAILURE)
	},
	[M0_FOPH_TXN_INIT] = {
		.sd_name      = "tx_init",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_OPEN, M0_FOPH_FAILURE)
	},
	[M0_FOPH_TXN_OPEN] = {
		.sd_name      = "tx_open",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_WAIT)
	},
	[M0_FOPH_TXN_WAIT] = {
		.sd_name      = "tx_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_FAILURE,
					M0_FOPH_TYPE_SPECIFIC)
	},
	[M0_FOPH_SUCCESS] = {
		.sd_name      = "success",
		.sd_allowed   = M0_BITS(M0_FOPH_FOL_REC_ADD)
	},
	[M0_FOPH_FOL_REC_ADD] = {
		.sd_name      = "fol_rec_add",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_COMMIT)
	},
	[M0_FOPH_TXN_COMMIT] = {
		.sd_name      = "tx_commit",
		.sd_allowed   = M0_BITS(M0_FOPH_QUEUE_REPLY)
	},
	[M0_FOPH_QUEUE_REPLY] = {
		.sd_name      = "queue_reply",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_COMMIT_WAIT,
					M0_FOPH_QUEUE_REPLY_WAIT,
					M0_FOPH_FINISH)
	},
	[M0_FOPH_QUEUE_REPLY_WAIT] = {
		.sd_name      = "queue_reply_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_COMMIT_WAIT,
					M0_FOPH_FINISH)
	},
	[M0_FOPH_TXN_COMMIT_WAIT] = {
		.sd_name      = "tx_commit_wait",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_INIT, M0_FOPH_FINISH)
	},
	[M0_FOPH_TIMEOUT] = {
		.sd_name      = "timeout",
		.sd_allowed   = M0_BITS(M0_FOPH_FAILURE)
	},
	[M0_FOPH_FAILURE] = {
		.sd_flags     = M0_SDF_FAILURE,
		.sd_name      = "failure",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_COMMIT)
	},
	[M0_FOPH_FINISH] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "finish",
	},
	[M0_FOPH_TYPE_SPECIFIC] = {
		.sd_name      = "specific-phase",
		.sd_allowed   = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE,
					M0_FOPH_FINISH)
	}
};

struct m0_sm_trans_descr m0_generic_phases_trans[] = {
	{"initialised",     M0_FOPH_INIT,   M0_FOPH_AUTHENTICATE},
	{"finished",        M0_FOPH_INIT,   M0_FOPH_FINISH},
	{"success",         M0_FOPH_INIT,   M0_FOPH_SUCCESS},
	{"init-failed",	M0_FOPH_INIT,   M0_FOPH_FAILURE},
	{"start-specific-phases", M0_FOPH_INIT, M0_FOPH_TYPE_SPECIFIC},
	{"wait-authentication",
	 M0_FOPH_AUTHENTICATE, M0_FOPH_AUTHENTICATE_WAIT},
	{"authentication-finished",
	 M0_FOPH_AUTHENTICATE, M0_FOPH_RESOURCE_LOCAL},
	{"authentication-failed", M0_FOPH_AUTHENTICATE, M0_FOPH_FAILURE},
	{"authentication-wait-complete",
	 M0_FOPH_AUTHENTICATE_WAIT, M0_FOPH_RESOURCE_LOCAL},
	{"authentication-wait-failed",
	 M0_FOPH_AUTHENTICATE_WAIT, M0_FOPH_FAILURE},
	{"local-resource-wait",
	 M0_FOPH_RESOURCE_LOCAL, M0_FOPH_RESOURCE_LOCAL_WAIT},
	{"local-resourc-complete",
	 M0_FOPH_RESOURCE_LOCAL, M0_FOPH_RESOURCE_DISTRIBUTED},
	{"local-resource-failed",
	 M0_FOPH_RESOURCE_LOCAL, M0_FOPH_FAILURE},
	{"local-resource-wait-complete",
	 M0_FOPH_RESOURCE_LOCAL_WAIT, M0_FOPH_RESOURCE_DISTRIBUTED},
	{"local-resource-wait-failed",
	 M0_FOPH_RESOURCE_LOCAL_WAIT, M0_FOPH_FAILURE},
	{"distributed-resource-wait",
	 M0_FOPH_RESOURCE_DISTRIBUTED, M0_FOPH_RESOURCE_DISTRIBUTED_WAIT},
	{"distributed-resource-complete",
	 M0_FOPH_RESOURCE_DISTRIBUTED, M0_FOPH_OBJECT_CHECK},
	{"distributed-resource-failed",
	 M0_FOPH_RESOURCE_DISTRIBUTED, M0_FOPH_FAILURE},
	{"distributed-resource-wait-complete",
	 M0_FOPH_RESOURCE_DISTRIBUTED_WAIT, M0_FOPH_OBJECT_CHECK},
	{"distributed-resource-wait-failed",
	 M0_FOPH_RESOURCE_DISTRIBUTED_WAIT, M0_FOPH_FAILURE},
	{"object-wait", M0_FOPH_OBJECT_CHECK, M0_FOPH_OBJECT_CHECK_WAIT},
	{"object-wait-complete",  M0_FOPH_OBJECT_CHECK, M0_FOPH_AUTHORISATION},
	{"object-failed", M0_FOPH_OBJECT_CHECK, M0_FOPH_FAILURE},
	{"object-wait-complete",
	 M0_FOPH_OBJECT_CHECK_WAIT, M0_FOPH_AUTHORISATION},
	{"object-wait-failed", M0_FOPH_OBJECT_CHECK_WAIT, M0_FOPH_FAILURE},
	{"authorisation-wait",
	 M0_FOPH_AUTHORISATION, M0_FOPH_AUTHORISATION_WAIT},
	{"authorisation-complete", M0_FOPH_AUTHORISATION, M0_FOPH_TXN_INIT},
	{"autorisation-failed", M0_FOPH_AUTHORISATION, M0_FOPH_FAILURE},
	{"authorisation-wait-complete",
	 M0_FOPH_AUTHORISATION_WAIT, M0_FOPH_TXN_INIT},
	{"authorisation-wait-failed",
	 M0_FOPH_AUTHORISATION_WAIT, M0_FOPH_FAILURE},
	{"tx-initialised", M0_FOPH_TXN_INIT, M0_FOPH_TXN_OPEN},
	{"tx-initialisation-failed", M0_FOPH_TXN_INIT, M0_FOPH_FAILURE},
	{"tx-open-wait", M0_FOPH_TXN_OPEN, M0_FOPH_TXN_WAIT},
	{"tx-open-failed", M0_FOPH_TXN_WAIT, M0_FOPH_FAILURE},
	{"tx-opened",  M0_FOPH_TXN_WAIT, M0_FOPH_TYPE_SPECIFIC},
	{"completed", M0_FOPH_SUCCESS, M0_FOPH_FOL_REC_ADD},
	{"fol-record-added", M0_FOPH_FOL_REC_ADD, M0_FOPH_TXN_COMMIT},
	{"tx-commit-start", M0_FOPH_TXN_COMMIT, M0_FOPH_QUEUE_REPLY},
	{"reply-queue-wait", M0_FOPH_QUEUE_REPLY, M0_FOPH_QUEUE_REPLY_WAIT},
	{"tx-commit-wait", M0_FOPH_QUEUE_REPLY, M0_FOPH_TXN_COMMIT_WAIT},
	{"reply-complete", M0_FOPH_QUEUE_REPLY, M0_FOPH_FINISH},
	{"reply-wait-finished",
	 M0_FOPH_QUEUE_REPLY_WAIT, M0_FOPH_TXN_COMMIT_WAIT},
	{"reply-wait-finished", M0_FOPH_QUEUE_REPLY_WAIT, M0_FOPH_FINISH},
	{"next", M0_FOPH_TXN_COMMIT_WAIT, M0_FOPH_TXN_INIT},
	{"tx-commit-wait-complete", M0_FOPH_TXN_COMMIT_WAIT, M0_FOPH_FINISH},
	{"timeout", M0_FOPH_TIMEOUT, M0_FOPH_FAILURE},
	{"failed", M0_FOPH_FAILURE, M0_FOPH_TXN_COMMIT},
};

M0_BASSERT(ARRAY_SIZE(m0_generic_phases_trans) == M0_FOM_GENERIC_TRANS_NR);

const struct m0_sm_conf m0_generic_conf = {
	.scf_magic     = M0_SM_CONF_MAGIC,
	.scf_name      = "standard-phases",
	.scf_nr_states = ARRAY_SIZE(generic_phases),
	.scf_state     = generic_phases,
	.scf_trans_nr  = ARRAY_SIZE(m0_generic_phases_trans),
	.scf_trans     = m0_generic_phases_trans,
};
M0_EXPORTED(m0_generic_conf);

int m0_fom_tick_generic(struct m0_fom *fom)
{
	int			     rc;
	const struct fom_phase_desc *fpd_phase;

	M0_PRE(fom != NULL);

	fpd_phase = &fpd_table[m0_fom_phase(fom)];

	M0_ENTRY("fom=%p phase=%s", fom,
		m0_fom_phase_name(fom, m0_fom_phase(fom)));

	rc = fpd_phase->fpd_action(fom);
	if (rc < 0) {
		m0_fom_phase_move(fom, rc, M0_FOPH_FAILURE);
		rc = M0_FSO_AGAIN;
	} else if (rc == M0_FSO_AGAIN) {
		/* fpd_action() could change phase */
		fpd_phase = &fpd_table[m0_fom_phase(fom)];
		m0_fom_phase_set(fom, fpd_phase->fpd_nextphase);
	}

	if (m0_fom_phase(fom) == M0_FOPH_FINISH)
		rc = M0_FSO_WAIT;

	M0_LEAVE("fom=%p phase=%s rc=%d", fom,
		m0_fom_phase_name(fom, m0_fom_phase(fom)), rc);
	return M0_RC(rc);
}

/** @} end of fom group */
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
