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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 *                  Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 *                  Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 17-Jun-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/tx_group_fom.h"

#include "lib/misc.h"        /* M0_BITS */
#include "rpc/rpc_opcodes.h" /* M0_BE_TX_GROUP_OPCODE */

#include "be/tx_group.h"
#include "be/tx_service.h"   /* m0_be_txs_stype */

/**
 * @addtogroup be
 * @{
 */

static struct m0_be_tx_group_fom *fom2tx_group_fom(const struct m0_fom *fom);
static void tx_group_fom_fini(struct m0_fom *fom);

/* ------------------------------------------------------------------
 * State definitions
 * ------------------------------------------------------------------ */

/**
 * Phases of tx_group fom.
 *
 * @verbatim
 *
 *       INIT ----> FAILED ----------.
 *        |                          |
 *        v                          v
 * ,---> OPEN ----> STOPPING ----> FINISH
 * |      |
 * |      v
 * |    LOGGING ----> RECONSTRUCT
 * |      |                |
 * |      |                v
 * |      |             TX_OPEN
 * |      |                |
 * |      |                v
 * |      |             TX_CLOSE
 * |      |                |
 * |      v                v
 * |    PLACING <------ REAPPLY
 * |      |
 * |      v
 * |    PLACED
 * |      |
 * |      v
 * |  STABILIZING
 * |      |
 * |      v
 * |    STABLE ----- TX_GC_WAIT
 * |      |             |
 * |      v             |
 * `--- RESET <---------/
 *
 * @endverbatim
 */
enum tx_group_fom_state {
	TGS_INIT   = M0_FOM_PHASE_INIT,
	TGS_FINISH = M0_FOM_PHASE_FINISH,
	/**
	 * tx_group gets populated with transactions, that are added with
	 * tx_group_add().
	 */
	TGS_OPEN   = M0_FOM_PHASE_NR,
	/** Log stobio is in progress. */
	TGS_PREPARE,            /* XXX rename it */
	TGS_LOGGING,            /* XXX rename it */
	/** Group is reconstructing */
	TGS_RECONSTRUCT,        /* XXX rename it? */
	TGS_TX_OPEN,            /* XXX rename it? */
	TGS_TX_CLOSE,           /* XXX rename it? */
	TGS_REAPPLY,            /* XXX rename it? */
	/** In-place (segment) stobio is in progress. */
	TGS_PLACING,
	TGS_PLACED,
	/** Waiting for transactions to stabilize. */
	TGS_STABILIZING,
	/**
	 * m0_be_tx_stable() has been called for all transactions of the
	 * tx_group.
	 */
	TGS_STABLE,
	TGS_TX_GC_WAIT,
	TGS_RESET,
	TGS_STOPPING,
	TGS_FAILED,
	TGS_NR
};

static struct m0_sm_state_descr tx_group_fom_states[TGS_NR] = {
#define _S(name, flags, allowed)      \
	[name] = {                    \
		.sd_flags   = flags,  \
		.sd_name    = #name,  \
		.sd_allowed = allowed \
	}

	_S(TGS_INIT,   M0_SDF_INITIAL, M0_BITS(TGS_OPEN, TGS_FAILED)),
	_S(TGS_FINISH, M0_SDF_TERMINAL, 0),
	_S(TGS_FAILED, M0_SDF_FAILURE, M0_BITS(TGS_FINISH)),
	_S(TGS_STOPPING,    0, M0_BITS(TGS_FINISH)),
	_S(TGS_OPEN,        0, M0_BITS(TGS_PREPARE, TGS_STOPPING)),
	_S(TGS_PREPARE,     0, M0_BITS(TGS_LOGGING)),
	_S(TGS_LOGGING,     0, M0_BITS(TGS_PLACING, TGS_RECONSTRUCT)),
	_S(TGS_RECONSTRUCT, 0, M0_BITS(TGS_TX_OPEN)),
	_S(TGS_TX_OPEN,     0, M0_BITS(TGS_TX_CLOSE)),
	_S(TGS_TX_CLOSE,    0, M0_BITS(TGS_REAPPLY)),
	_S(TGS_REAPPLY,     0, M0_BITS(TGS_PLACING)),
	_S(TGS_PLACING,     0, M0_BITS(TGS_PLACED)),
	_S(TGS_PLACED,      0, M0_BITS(TGS_STABILIZING)),
	_S(TGS_STABILIZING, 0, M0_BITS(TGS_STABLE)),
	_S(TGS_STABLE,      0, M0_BITS(TGS_RESET, TGS_TX_GC_WAIT)),
	_S(TGS_TX_GC_WAIT,  0, M0_BITS(TGS_RESET)),
	_S(TGS_RESET,       0, M0_BITS(TGS_OPEN)),
#undef _S
};

const static struct m0_sm_conf tx_group_fom_conf = {
	.scf_name      = "m0_be_tx_group_fom",
	.scf_nr_states = ARRAY_SIZE(tx_group_fom_states),
	.scf_state     = tx_group_fom_states,
};

static int tx_group_fom_tick(struct m0_fom *fom)
{
	enum tx_group_fom_state    phase = m0_fom_phase(fom);
	struct m0_be_tx_group_fom *m     = fom2tx_group_fom(fom);
	struct m0_be_tx_group     *gr    = m->tgf_group;
	struct m0_be_op           *op    = &m->tgf_op;
	int                        rc;

	M0_ENTRY("tx_group_fom=%p group=%p phase=%s", m, gr,
		 m0_fom_phase_name(fom, phase));

	switch (phase) {
	case TGS_INIT:
		rc = m0_be_tx_group__allocate(gr);
		m0_fom_phase_move(fom, rc, rc == 0 ? TGS_OPEN : TGS_FAILED);
		m0_semaphore_up(&m->tgf_start_sem);
		return M0_FSO_WAIT;
	case TGS_OPEN:
		if (m->tgf_stopping) {
			m0_fom_phase_set(fom, TGS_STOPPING);
			return M0_FSO_AGAIN;
		}
		return M0_FSO_WAIT;
	case TGS_PREPARE:
		m0_be_op_reset(op);
		m0_be_tx_group_prepare(gr, op);
		return m0_be_op_tick_ret(op, fom, TGS_LOGGING);
	case TGS_LOGGING:
		m0_be_op_reset(op);
		if (m->tgf_recovery_mode) {
			m0_be_tx_group_log_read(gr, op);
			return m0_be_op_tick_ret(op, fom, TGS_RECONSTRUCT);
		} else {
			m0_be_tx_group_encode(gr);
			m0_be_tx_group_log_write(gr, op);
			return m0_be_op_tick_ret(op, fom, TGS_PLACING);
		}
	case TGS_RECONSTRUCT:
		rc = m0_be_tx_group_decode(gr);
		M0_ASSERT_INFO(rc == 0, "rc = %d", rc); /* XXX notify engine */
		rc = m0_be_tx_group_reconstruct(gr,
						&m->tgf_gen.fo_loc->fl_group);
		M0_ASSERT_INFO(rc == 0, "rc = %d", rc); /* XXX notify engine */
		m0_fom_phase_set(fom, TGS_TX_OPEN);
		return M0_FSO_AGAIN;
	case TGS_TX_OPEN:
		m0_be_op_reset(op);
		m0_be_tx_group_reconstruct_tx_open(gr, op);
		return m0_be_op_tick_ret(op, fom, TGS_TX_CLOSE);
	case TGS_TX_CLOSE:
		m0_be_op_reset(&m->tgf_op_gc);
		/* m0_be_op_tick_ret() for the op is in TGS_TX_GC_WAIT phase */
		m0_be_tx_group_reconstruct_tx_close(gr, &m->tgf_op_gc);
		m0_fom_phase_set(fom, TGS_REAPPLY);
		return M0_FSO_AGAIN;
	case TGS_REAPPLY:
		m0_be_op_reset(op);
		rc = m0_be_tx_group_reapply(gr, op);
		M0_ASSERT_INFO(rc == 0, "rc = %d", rc); /* XXX notify engine */
		return m0_be_op_tick_ret(op, fom, TGS_PLACING);
	case TGS_PLACING:
		m0_be_tx_group__tx_state_post(gr, M0_BTS_LOGGED, false);
		m0_be_op_reset(op);
		m0_be_tx_group_seg_place_prepare(gr);
		m0_be_tx_group_seg_place(gr, op);
		return m0_be_op_tick_ret(op, fom, TGS_PLACED);
	case TGS_PLACED:
		m0_be_tx_group__tx_state_post(gr, M0_BTS_PLACED, true);
		m0_fom_phase_set(fom, TGS_STABILIZING);
		return M0_FSO_AGAIN;
	case TGS_STABILIZING:
		if (m->tgf_stable) {
			m0_fom_phase_set(fom, TGS_STABLE);
			return M0_FSO_AGAIN;
		}
		return M0_FSO_WAIT;
	case TGS_STABLE:
		m0_fom_phase_set(fom, m->tgf_recovery_mode ?
				 TGS_TX_GC_WAIT : TGS_RESET);
		return M0_FSO_AGAIN;
	case TGS_TX_GC_WAIT:
		return m0_be_op_tick_ret(&m->tgf_op_gc, fom, TGS_RESET);
	case TGS_RESET:
		m0_be_tx_group_reset(gr);
		m0_be_tx_group_open(gr);
		m0_fom_phase_set(fom, TGS_OPEN);
		return M0_FSO_AGAIN;
	case TGS_STOPPING:
		m0_be_tx_group__deallocate(gr);
		m0_fom_phase_set(fom, TGS_FINISH);
		return M0_FSO_WAIT;
	case TGS_FAILED:
		m0_fom_phase_set(fom, TGS_FINISH);
		return M0_FSO_WAIT;
	default:
		M0_IMPOSSIBLE("Invalid phase: %d", phase);
	}

	M0_LEAVE();
	return M0_FSO_WAIT;
}

static void tx_group_fom_fini(struct m0_fom *fom)
{
	struct m0_be_tx_group_fom *m = fom2tx_group_fom(fom);

	m0_fom_fini(fom);
	m0_semaphore_up(&m->tgf_finish_sem);
}

static size_t tx_group_fom_locality(const struct m0_fom *fom)
{
	return 0; /* XXX TODO: reconsider */
}

static const struct m0_fom_ops tx_group_fom_ops = {
	.fo_fini          = tx_group_fom_fini,
	.fo_tick          = tx_group_fom_tick,
	.fo_home_locality = tx_group_fom_locality
};

static struct m0_fom_type tx_group_fom_type;

static const struct m0_fom_type_ops tx_group_fom_type_ops = {
	.fto_create = NULL
};

static struct m0_be_tx_group_fom *fom2tx_group_fom(const struct m0_fom *fom)
{
	/* XXX TODO bob_of() */
	return container_of(fom, struct m0_be_tx_group_fom, tgf_gen);
}

static void be_tx_group_fom_handle(struct m0_sm_group *gr,
				   struct m0_sm_ast   *ast)
{
	struct m0_be_tx_group_fom *m = M0_AMB(m, ast, tgf_ast_handle);

	M0_LOG(M0_DEBUG, "m=%p, tx_nr=%zu", m,
	       m0_be_tx_group_tx_nr(m->tgf_group));

	m0_fom_phase_set(&m->tgf_gen, TGS_PREPARE);
	m0_fom_ready(&m->tgf_gen);
}

/*
 * Wakes up tx_group_fom iff it is waiting.
 * It is possible that multiple fom wakeup asts are posted through different
 * code paths. Thus we avoid waking up of already running FOM.
 */
static void be_tx_group_fom_iff_waiting_wakeup(struct m0_fom *fom)
{
	M0_ENTRY();
	if (m0_fom_is_waiting(fom)) {
		M0_LOG(M0_DEBUG, "waking up");
		m0_fom_ready(fom);
	}
	M0_LEAVE();
}

static void be_tx_group_fom_stable(struct m0_sm_group *_, struct m0_sm_ast *ast)
{
	struct m0_be_tx_group_fom *m = M0_AMB(m, ast, tgf_ast_stable);

	M0_ENTRY();

	m->tgf_stable = true;
	be_tx_group_fom_iff_waiting_wakeup(&m->tgf_gen);
	M0_LEAVE();
}

static void be_tx_group_fom_stop(struct m0_sm_group *gr, struct m0_sm_ast *ast)
{
	struct m0_be_tx_group_fom *m = M0_AMB(m, ast, tgf_ast_stop);

	M0_ENTRY();

	m->tgf_stopping = true;
	be_tx_group_fom_iff_waiting_wakeup(&m->tgf_gen);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_tx_group_fom_init(struct m0_be_tx_group_fom *m,
					 struct m0_be_tx_group     *gr,
					 struct m0_reqh            *reqh)
{
	M0_ENTRY();

	m0_fom_init(&m->tgf_gen, &tx_group_fom_type,
		    &tx_group_fom_ops, NULL, NULL, reqh);

	m->tgf_group    = gr;
	m->tgf_reqh     = reqh;
	m->tgf_stable   = false;
	m->tgf_stopping = false;

#define _AST(handler) (struct m0_sm_ast){ .sa_cb = (handler) }
	m->tgf_ast_handle  = _AST(be_tx_group_fom_handle);
	m->tgf_ast_stable  = _AST(be_tx_group_fom_stable);
	m->tgf_ast_stop    = _AST(be_tx_group_fom_stop);
#undef _AST

	m0_semaphore_init(&m->tgf_start_sem, 0);
	m0_semaphore_init(&m->tgf_finish_sem, 0);
	m0_be_op_init(&m->tgf_op);
	m0_be_op_init(&m->tgf_op_gc);

	M0_LEAVE();
}

M0_INTERNAL void m0_be_tx_group_fom_fini(struct m0_be_tx_group_fom *m)
{
	M0_PRE(m0_fom_phase(&m->tgf_gen) == TGS_FINISH);

	m0_be_op_fini(&m->tgf_op_gc);
	m0_be_op_fini(&m->tgf_op);
	m0_semaphore_fini(&m->tgf_start_sem);
	m0_semaphore_fini(&m->tgf_finish_sem);
}

M0_INTERNAL void m0_be_tx_group_fom_reset(struct m0_be_tx_group_fom *m)
{
	m->tgf_recovery_mode = false;
	m->tgf_stable        = false;
}

static void be_tx_group_fom_ast_post(struct m0_be_tx_group_fom *gf,
				     struct m0_sm_ast          *ast)
{
	m0_sm_ast_post(&gf->tgf_gen.fo_loc->fl_group, ast);
}

M0_INTERNAL int m0_be_tx_group_fom_start(struct m0_be_tx_group_fom *gf)
{
	int            rc;
	struct m0_fom *fom = &gf->tgf_gen;

	m0_fom_queue(fom);
	m0_semaphore_down(&gf->tgf_start_sem);
	M0_ASSERT(M0_IN(m0_fom_phase(fom), (TGS_OPEN, TGS_FAILED)));
	rc = m0_fom_rc(fom);
	if (m0_fom_phase(fom) == TGS_FAILED) {
		M0_ASSERT(rc != 0);
		m0_fom_wakeup(fom);
		m0_semaphore_down(&gf->tgf_finish_sem);
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_be_tx_group_fom_stop(struct m0_be_tx_group_fom *gf)
{
	M0_ENTRY();
	be_tx_group_fom_ast_post(gf, &gf->tgf_ast_stop);
	m0_semaphore_down(&gf->tgf_finish_sem);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_tx_group_fom_handle(struct m0_be_tx_group_fom *m)
{
	be_tx_group_fom_ast_post(m, &m->tgf_ast_handle);
}

M0_INTERNAL void m0_be_tx_group_fom_stable(struct m0_be_tx_group_fom *gf)
{
	be_tx_group_fom_ast_post(gf, &gf->tgf_ast_stable);
}

M0_INTERNAL struct m0_sm_group *
m0_be_tx_group_fom__sm_group(struct m0_be_tx_group_fom *m)
{
	return &m->tgf_gen.fo_loc->fl_group;
}

M0_INTERNAL void
m0_be_tx_group_fom_recovery_prepare(struct m0_be_tx_group_fom *m)
{
	m->tgf_recovery_mode = true;
}

M0_INTERNAL void m0_be_tx_group_fom_mod_init(void)
{
	m0_fom_type_init(&tx_group_fom_type, M0_BE_TX_GROUP_OPCODE,
			 &tx_group_fom_type_ops,
			 &m0_be_txs_stype, &tx_group_fom_conf);
}

M0_INTERNAL void m0_be_tx_group_fom_mod_fini(void)
{}

/** @} end of be group */
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
