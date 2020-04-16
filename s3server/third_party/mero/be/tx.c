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
 * Original creation date: 29-May-2013
 */

#include <stddef.h>             /* ptrdiff_t */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/tx.h"
#include "be/tx_group.h"
#include "be/tx_internal.h"

#include "lib/errno.h"          /* ENOMEM */
#include "lib/misc.h"           /* M0_BITS */
#include "lib/arith.h"          /* M0_CNT_INC */
#include "lib/memory.h"                 /* m0_alloc_nz */
#include "fol/fol.h"            /* m0_fol_rec_encode() */

#include "be/op.h"              /* m0_be_op */
#include "be/domain.h"          /* m0_be_domain_engine */
#include "be/engine.h"          /* m0_be_engine__tx_state_set */
#include "be/addb2.h"           /* M0_AVI_BE_TX_STATE, M0_AVI_BE_TX_COUNTER */
#include "be/fmt.h"             /* m0_be_fmt_tx */

/**
 * @addtogroup be
 *
 * @{
 */

static bool be_tx_state_invariant(const struct m0_sm *mach)
{
	return m0_be_tx__invariant(
		container_of(mach, const struct m0_be_tx, t_sm));
}

static bool be_tx_is_locked(const struct m0_be_tx *tx);

#define BE_TX_LOCKED_AT_STATE(tx, states)                               \
({                                                                      \
	const struct m0_be_tx *__tx = (tx);                             \
									\
	_0C(be_tx_is_locked(__tx)) && m0_be_tx__invariant(__tx) &&      \
		_0C(M0_IN(m0_be_tx_state(__tx), states));               \
})

/**
 * M0_BTS_NR items in array is enough, but sometimes gcc issues the following
 * warning:
 *
 * @code
 * $ m0 rebuild
 * ...skip...
 *   CC     mero/mero_libmero_altogether_la-mero_altogether_user.lo
 * In file included from mero/mero_altogether_user.c:23:
 * .../be/tx_group.c: In function ‘m0_be_tx_group__tx_state_post’:
 * .../be/tx.c:84: error: array subscript is above array bounds
 * .../be/tx.c:84: error: array subscript is above array bounds
 * @endcode
 *
 * @todo Find out why M0_BTS_NR + 1 is enough and M0_BTS_NR isn't.
 */
static const ptrdiff_t be_tx_ast_offset[M0_BTS_NR + 1] = {
	[M0_BTS_GROUPING] = offsetof(struct m0_be_tx, t_ast_grouping),
	[M0_BTS_ACTIVE]  = offsetof(struct m0_be_tx, t_ast_active),
	[M0_BTS_FAILED]  = offsetof(struct m0_be_tx, t_ast_failed),
	[M0_BTS_LOGGED]  = offsetof(struct m0_be_tx, t_ast_logged),
	[M0_BTS_PLACED]  = offsetof(struct m0_be_tx, t_ast_placed),
	[M0_BTS_DONE]    = offsetof(struct m0_be_tx, t_ast_done)
};

static void be_tx_state_move_ast(struct m0_be_tx *tx,
				 enum m0_be_tx_state state);

static void be_tx_ast_cb(struct m0_sm_group *sm_group, struct m0_sm_ast *ast)
{
	enum m0_be_tx_state state = (enum m0_be_tx_state)ast->sa_datum;
	struct m0_be_tx    *tx    = ((void *)ast) - be_tx_ast_offset[state];

	M0_PRE(IS_IN_ARRAY(state, be_tx_ast_offset));
	M0_PRE(be_tx_ast_offset[state] != 0);
	be_tx_state_move_ast(tx, state);
}

static struct m0_sm_ast *
be_tx_ast(struct m0_be_tx *tx, enum m0_be_tx_state state)
{
	M0_PRE(IS_IN_ARRAY(state, be_tx_ast_offset));
	M0_PRE(be_tx_ast_offset[state] != 0);
	return ((void *)tx) + be_tx_ast_offset[state];
}

/* be sure to change be_tx_state_move_ast if change be_tx_states */
static struct m0_sm_state_descr be_tx_states[M0_BTS_NR] = {
	[M0_BTS_PREPARE] = {
		.sd_flags = M0_SDF_INITIAL,
		.sd_name = "prepare",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = M0_BITS(M0_BTS_OPENING, M0_BTS_FAILED),
	},
	[M0_BTS_OPENING] = {
		.sd_name = "opening",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = M0_BITS(M0_BTS_GROUPING, M0_BTS_FAILED),
	},
	[M0_BTS_GROUPING] = {
		.sd_name = "grouping",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = M0_BITS(M0_BTS_ACTIVE),
	},
	[M0_BTS_FAILED] = {
		.sd_flags =  M0_SDF_TERMINAL | M0_SDF_FAILURE,
		.sd_name = "failed",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = 0,
	},
	[M0_BTS_ACTIVE] = {
		.sd_name = "active",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = M0_BITS(M0_BTS_CLOSED),
	},
	[M0_BTS_CLOSED] = {
		.sd_name = "closed",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = M0_BITS(M0_BTS_LOGGED),
	},
	[M0_BTS_LOGGED] = {
		.sd_name = "logged",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = M0_BITS(M0_BTS_PLACED),
	},
	[M0_BTS_PLACED] = {
		.sd_name = "placed",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = M0_BITS(M0_BTS_DONE),
	},
	[M0_BTS_DONE] = {
		.sd_flags = M0_SDF_TERMINAL,
		.sd_name = "done",
		.sd_invariant = be_tx_state_invariant,
		.sd_allowed = 0,
	},
};

static struct m0_sm_trans_descr be_tx_sm_trans[] = {
	{ "opening",        M0_BTS_PREPARE,  M0_BTS_OPENING  },
	{ "prepare-failed", M0_BTS_PREPARE,  M0_BTS_FAILED   },
	{ "grouping",       M0_BTS_OPENING,  M0_BTS_GROUPING },
	{ "open-failed",    M0_BTS_OPENING,  M0_BTS_FAILED   },
	{ "activated",      M0_BTS_GROUPING, M0_BTS_ACTIVE   },
	{ "closed",         M0_BTS_ACTIVE,   M0_BTS_CLOSED   },
	{ "logged",         M0_BTS_CLOSED,   M0_BTS_LOGGED   },
	{ "placed",         M0_BTS_LOGGED,   M0_BTS_PLACED   },
	{ "done",           M0_BTS_PLACED,   M0_BTS_DONE     }
};

struct m0_sm_conf be_tx_sm_conf = {
	.scf_name      = "m0_be_tx::t_sm",
	.scf_nr_states = ARRAY_SIZE(be_tx_states),
	.scf_state     = be_tx_states,
	.scf_trans_nr  = ARRAY_SIZE(be_tx_sm_trans),
	.scf_trans     = be_tx_sm_trans
};

static void be_tx_state_move(struct m0_be_tx     *tx,
			     enum m0_be_tx_state  state,
			     int                  rc);

M0_INTERNAL void m0_be_tx_init(struct m0_be_tx     *tx,
			       uint64_t             tid,
			       struct m0_be_domain *dom,
			       struct m0_sm_group  *sm_group,
			       m0_be_tx_cb_t        persistent,
			       m0_be_tx_cb_t        discarded,
			       void               (*filler)(struct m0_be_tx *tx,
							    void *payload),
			       void                *datum)
{
	enum m0_be_tx_state state;

	M0_PRE(M0_IS0(tx));

	*tx = (struct m0_be_tx){
		.t_id               = tid,
		.t_engine           = m0_be_domain_engine(dom),
		.t_dom              = dom,
		.t_persistent       = persistent,
		.t_discarded        = discarded,
		.t_filler           = filler,
		.t_datum            = datum,
		.t_payload_prepared = 0,
		.t_fast             = false,
		.t_gc_enabled       = false,
		.t_gc_free          = NULL,
		.t_gc_param         = NULL,
		.t_exclusive        = false,
		.t_recovering       = false,
	};

	m0_sm_init(&tx->t_sm, &be_tx_sm_conf, M0_BTS_PREPARE, sm_group);
	m0_sm_addb2_counter_init(&tx->t_sm);

	for (state = 0; state < ARRAY_SIZE(be_tx_ast_offset); ++state) {
		if (be_tx_ast_offset[state] != 0) {
			*be_tx_ast(tx, state) = (struct m0_sm_ast) {
				.sa_cb    = be_tx_ast_cb,
				.sa_datum = (void *) state,
			};
		}
	}

	m0_be_engine__tx_init(tx->t_engine, tx, M0_BTS_PREPARE);
	m0_be_tx_get(tx);

	M0_POST(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE)));
}

M0_INTERNAL void m0_be_tx_fini(struct m0_be_tx *tx)
{
	enum m0_be_tx_state state;

	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_DONE, M0_BTS_FAILED)));
	M0_PRE(tx->t_ref == 0);

	m0_be_engine__tx_fini(tx->t_engine, tx);

	for (state = 0; state < ARRAY_SIZE(be_tx_ast_offset); ++state) {
		if (be_tx_ast_offset[state] != 0)
			m0_sm_ast_cancel(tx->t_sm.sm_grp, be_tx_ast(tx, state));
	}
	/*
	 * Note: m0_sm_fini() will call be_tx_state_invariant(), so
	 * m0_be_tx::t_reg_area should be finalized after m0_be_tx::t_sm.
	 */
	m0_sm_fini(&tx->t_sm);
	m0_be_reg_area_fini(&tx->t_reg_area);
	m0_free(tx->t_payload.b_addr);
}

M0_INTERNAL void m0_be_tx_prep(struct m0_be_tx              *tx,
			       const struct m0_be_tx_credit *credit)
{
	M0_ENTRY("tx=%p credit="BETXCR_F, tx, BETXCR_P(credit));
	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE)));

	m0_be_tx_credit_add(&tx->t_prepared, credit);

	M0_POST(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE)));
}

M0_INTERNAL void m0_be_tx_payload_prep(struct m0_be_tx *tx, m0_bcount_t size)
{
	M0_ENTRY("tx=%p size=%"PRIu64, tx, size);
	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE)));

	tx->t_payload_prepared += size;

	M0_POST(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE)));
}

M0_INTERNAL void m0_be_tx_open(struct m0_be_tx *tx)
{
	M0_ENTRY("tx=%p t_prepared="BETXCR_F" t_payload_prepared=%"PRIu64,
		 tx, BETXCR_P(&tx->t_prepared), tx->t_payload_prepared);
	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE)));

	if (m0_be_tx_credit_eq(&tx->t_prepared, &m0_be_tx_credit_invalid)) {
		M0_LOG(M0_NOTICE, "tx=%p t_prepared="BETXCR_F,
		       tx, BETXCR_P(&tx->t_prepared));
		be_tx_state_move(tx, M0_BTS_FAILED, -EINVAL);
	} else {
		be_tx_state_move(tx, M0_BTS_OPENING, 0);
	}

	M0_POST(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_OPENING, M0_BTS_FAILED)));
	M0_LEAVE();
}

static void be_tx_make_reg_d(struct m0_be_tx        *tx,
                             struct m0_be_reg_d     *rd,
                             const struct m0_be_reg *reg)
{
	struct m0_be_seg *seg;

	/* TODO cache seg if the performance impact is significant */
	seg = m0_be_domain_seg_by_addr(tx->t_dom, reg->br_addr);
	*rd = M0_BE_REG_D(M0_BE_REG(reg->br_seg == NULL ? seg : reg->br_seg,
				    reg->br_size, reg->br_addr), NULL);
	M0_POST(m0_be_reg__invariant(&rd->rd_reg));
}

M0_INTERNAL void m0_be_tx_capture(struct m0_be_tx        *tx,
				  const struct m0_be_reg *reg)
{
	struct m0_be_reg_d rd;

	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_ACTIVE)));

	be_tx_make_reg_d(tx, &rd, reg);
	rd.rd_gen_idx = m0_be_reg_gen_idx(reg);
	m0_be_reg_area_capture(&tx->t_reg_area, &rd);
}

M0_INTERNAL void
m0_be_tx_uncapture(struct m0_be_tx *tx, const struct m0_be_reg *reg)
{
	struct m0_be_reg_d rd;

	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_ACTIVE)));

	be_tx_make_reg_d(tx, &rd, reg);
	m0_be_reg_area_uncapture(&tx->t_reg_area, &rd);
}

static void addb2_add_tx_attrs(const struct m0_be_tx *tx)
{
	uint64_t tx_sm_id = m0_sm_id_get(&tx->t_sm);

        M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id,
		     M0_AVI_BE_TX_ATTR_PAYLOAD_NOB,
                     tx->t_payload.b_nob);
        M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id,
		     M0_AVI_BE_TX_ATTR_PAYLOAD_PREP,
		     tx->t_payload_prepared);
        M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id,
		     M0_AVI_BE_TX_ATTR_RA_AREA_USED,
		     tx->t_reg_area.bra_area_used);
        M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id,
		     M0_AVI_BE_TX_ATTR_RA_PREP_TC_REG_NR,
		     tx->t_reg_area.bra_prepared.tc_reg_nr);
        M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id,
		     M0_AVI_BE_TX_ATTR_RA_PREP_TC_REG_SIZE,
		     tx->t_reg_area.bra_prepared.tc_reg_size);
        M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id,
		     M0_AVI_BE_TX_ATTR_RA_CAPT_TC_REG_NR,
		     tx->t_reg_area.bra_captured.tc_reg_nr);
        M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id,
		     M0_AVI_BE_TX_ATTR_RA_CAPT_TC_REG_SIZE,
		     tx->t_reg_area.bra_captured.tc_reg_size);
}

M0_INTERNAL void m0_be_tx_close(struct m0_be_tx *tx)
{
	M0_ENTRY("tx=%p", tx);
	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_ACTIVE)));

	addb2_add_tx_attrs(tx);
	be_tx_state_move(tx, M0_BTS_CLOSED, 0);

	M0_LEAVE();
}

M0_INTERNAL void m0_be_tx_get(struct m0_be_tx *tx)
{
	M0_ENTRY("tx=%p t_ref=%"PRIu32" state=%s",
		 tx, tx->t_ref, m0_be_tx_state_name(m0_be_tx_state(tx)));
	M0_PRE(be_tx_is_locked(tx));
	M0_PRE(!M0_IN(m0_be_tx_state(tx), (M0_BTS_FAILED, M0_BTS_DONE)));

	M0_CNT_INC(tx->t_ref);
}

M0_INTERNAL void m0_be_tx_put(struct m0_be_tx *tx)
{
	M0_ENTRY("tx=%p t_ref=%"PRIu32" state=%s",
		 tx, tx->t_ref, m0_be_tx_state_name(m0_be_tx_state(tx)));
	M0_PRE(be_tx_is_locked(tx));

	M0_CNT_DEC(tx->t_ref);
	if (tx->t_ref == 0 && m0_be_tx_state(tx) != M0_BTS_FAILED)
		m0_be_tx__state_post(tx, M0_BTS_DONE);
}

M0_INTERNAL int m0_be_tx_timedwait(struct m0_be_tx *tx,
				   uint64_t         states,
				   m0_time_t        deadline)
{
	int rc;

	M0_ENTRY("tx=%p state=%s states=%"PRIu64" deadline=%"PRIu64,
		 tx, m0_be_tx_state_name(m0_be_tx_state(tx)), states, deadline);
	M0_PRE(be_tx_is_locked(tx));
	M0_PRE(ergo(tx->t_gc_enabled,
		    M0_IN(m0_be_tx_state(tx), (M0_BTS_PREPARE, M0_BTS_OPENING,
					       M0_BTS_ACTIVE, M0_BTS_FAILED))));

	rc = m0_sm_timedwait(&tx->t_sm, states, deadline);
	M0_ASSERT_INFO(M0_IN(rc, (0, -ETIMEDOUT)), "rc=%d", rc);
	return M0_RC(rc == 0 ? tx->t_sm.sm_rc : rc);
}

M0_INTERNAL enum m0_be_tx_state m0_be_tx_state(const struct m0_be_tx *tx)
{
	return tx->t_sm.sm_state;
}

M0_INTERNAL const char *m0_be_tx_state_name(enum m0_be_tx_state state)
{
	return m0_sm_conf_state_name(&be_tx_sm_conf, state);
}

static void be_tx_state_move_ast(struct m0_be_tx *tx, enum m0_be_tx_state state)
{
	enum m0_be_tx_state tx_state = m0_be_tx_state(tx);

	M0_ENTRY("tx=%p %s -> %s",
	         tx, m0_be_tx_state_name(tx_state), m0_be_tx_state_name(state));

	if (tx_state < M0_BTS_CLOSED || state == tx_state + 1) {
		/*
		 * If we have state transition to M0_BTS_FAILED here
		 * then transaction exceeds engine tx size limit.
		 */
		be_tx_state_move(tx, state,
				 state == M0_BTS_FAILED ? -E2BIG : 0);
	} else {
		while (tx_state < state)
			be_tx_state_move(tx, ++tx_state, 0);
	}
}

static int be_tx_memory_allocate(struct m0_be_tx *tx)
{
	int rc;

	tx->t_payload.b_nob = tx->t_payload_prepared;
	if (tx->t_payload_prepared > 0)
		tx->t_payload.b_addr = m0_alloc_nz(tx->t_payload.b_nob);
	if (tx->t_payload.b_addr == NULL && tx->t_payload.b_nob != 0) {
		rc = -ENOMEM;
		M0_LOG(M0_ERROR, "tx=%p t_payload_prepared=%"PRIu64" rc=%d",
		       tx, tx->t_payload_prepared, rc);
	} else {
		rc = m0_be_reg_area_init(&tx->t_reg_area, &tx->t_prepared,
					 M0_BE_REG_AREA_DATA_COPY);
		if (rc != 0) {
			m0_free0(&tx->t_payload.b_addr);
			M0_LOG(M0_ERROR, "tx=%p t_prepared="BETXCR_F" rc=%d",
			       tx, BETXCR_P(&tx->t_prepared), rc);
		}
	}
	return M0_RC(rc);
}

static void be_tx_gc(struct m0_be_tx *tx)
{
	void (*gc_free)(struct m0_be_tx *, void *param);
	void  *gc_param;

	M0_ENTRY("tx=%p t_gc_free=%p t_gc_param=%p",
		 tx, tx->t_gc_free, tx->t_gc_param);
	gc_free  = tx->t_gc_free;
	gc_param = tx->t_gc_param;
	m0_be_tx_fini(tx);
	if (gc_free != NULL)
		gc_free(tx, gc_param);
	else
		m0_free(tx);
}

static void be_tx_state_move(struct m0_be_tx     *tx,
			     enum m0_be_tx_state  state,
			     int                  rc)
{
	bool tx_is_freed = false;

	M0_ENTRY("tx=%p rc=%d %s -> %s", tx, rc,
		 m0_be_tx_state_name(m0_be_tx_state(tx)),
		 m0_be_tx_state_name(state));

	M0_PRE(m0_be_tx__invariant(tx));
	M0_PRE(be_tx_is_locked(tx));
	M0_PRE(ergo(rc != 0, state == M0_BTS_FAILED));
	M0_PRE(ergo(M0_IN(state, (M0_BTS_PREPARE, M0_BTS_OPENING, M0_BTS_GROUPING,
				  M0_BTS_ACTIVE, M0_BTS_CLOSED, M0_BTS_LOGGED,
				  M0_BTS_PLACED, M0_BTS_DONE)), rc == 0));

	if (state == M0_BTS_ACTIVE) {
		rc = be_tx_memory_allocate(tx);
		if (rc != 0)
			state = M0_BTS_FAILED;
	}

	if (state == M0_BTS_LOGGED && tx->t_persistent != NULL)
		tx->t_persistent(tx);
	if (state == M0_BTS_DONE && tx->t_discarded != NULL)
		tx->t_discarded(tx);

	m0_sm_move(&tx->t_sm, rc, state);
	m0_be_engine__tx_state_set(tx->t_engine, tx, state);

	if (M0_IN(state, (M0_BTS_PLACED, M0_BTS_FAILED)))
		m0_be_tx_put(tx);

	if (state == M0_BTS_DONE && tx->t_gc_enabled) {
		be_tx_gc(tx);
		tx_is_freed = true;
	}

	M0_POST(tx_is_freed || m0_be_tx__invariant(tx));
	M0_LEAVE();
}

M0_INTERNAL void m0_be_tx__state_post(struct m0_be_tx     *tx,
				      enum m0_be_tx_state  state)
{
	/* XXX move to group_fom doc */
	/*
	 * tx_group's fom and tx's sm may belong different sm_groups (e.g.,
	 * they may be processed by different localities).
	 *
	 *             locality
	 *             --------
	 *             sm_group     sm_group    sm_group
	 *                | |            |         | |
	 *                | |            |         | |
	 *      tx_group  | |            |         | |
	 *      --------  | |            |         | |
	 *           fom -' |            |         | |
	 *                  |  tx    tx  |     tx  | |  tx
	 *                  |  --    --  |     --  | |  --
	 *                  `- sm    sm -'     sm -' `- sm
	 *
	 * ->fo_tick() of tx_group's fom shall not assume that sm_group of
	 * tx's sm is locked. In order to advance tx's sm, ->fo_tick()
	 * implementation should post an AST to tx's sm_group.
	 */
	M0_PRE(M0_IN(state, (M0_BTS_GROUPING, M0_BTS_ACTIVE, M0_BTS_FAILED,
			     M0_BTS_LOGGED, M0_BTS_PLACED, M0_BTS_DONE)));
	M0_LOG(M0_DEBUG, "tx=%p state=%s", tx, m0_be_tx_state_name(state));

	m0_sm_ast_post(tx->t_sm.sm_grp, be_tx_ast(tx, state));
}

M0_INTERNAL bool m0_be_tx__invariant(const struct m0_be_tx *tx)
{
	return _0C(m0_be_tx_state(tx) < M0_BTS_NR) &&
	       m0_be_reg_area__invariant(&tx->t_reg_area);
}

static bool be_tx_is_locked(const struct m0_be_tx *tx)
{
	return m0_mutex_is_locked(&tx->t_sm.sm_grp->s_lock);
}

M0_INTERNAL struct m0_be_reg_area *m0_be_tx__reg_area(struct m0_be_tx *tx)
{
	return &tx->t_reg_area;
}

M0_INTERNAL int m0_be_tx_open_sync(struct m0_be_tx *tx)
{
	enum m0_be_tx_state state;
	int		    rc;

	m0_be_tx_open(tx);
	rc = m0_be_tx_timedwait(tx, M0_BITS(M0_BTS_ACTIVE, M0_BTS_FAILED),
				M0_TIME_NEVER);

	state = m0_be_tx_state(tx);
	M0_ASSERT_INFO(equi(rc == 0, state == M0_BTS_ACTIVE) &&
		       equi(rc != 0, state == M0_BTS_FAILED),
		       "tx=%p rc=%d state=%s",
		       tx, rc, m0_be_tx_state_name(state));
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_tx_exclusive_open(struct m0_be_tx *tx)
{
	tx->t_exclusive = true;
	m0_be_tx_open(tx);
}

M0_INTERNAL int m0_be_tx_exclusive_open_sync(struct m0_be_tx *tx)
{
	int rc;

	tx->t_exclusive = true;
	rc = m0_be_tx_open_sync(tx);
	M0_POST(m0_be_engine__exclusive_open_invariant(tx->t_engine, tx));
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_tx_close_sync(struct m0_be_tx *tx)
{
	bool gc_enabled;
	int  rc;

	tx->t_fast	 = true;
	/*
	 * m0_be_tx_timedwait() can't be used on transactions with
	 * GC enabled. So GC is disabled and called manually if it
	 * is needed.
	 */
	gc_enabled	 = tx->t_gc_enabled;
	tx->t_gc_enabled = false;
	m0_be_tx_close(tx);
	rc = m0_be_tx_timedwait(tx, M0_BITS(M0_BTS_DONE), M0_TIME_NEVER);
	M0_ASSERT_INFO(rc == 0, "Transaction can't fail after m0_be_tx_open(): "
		       "rc = %d, tx = %p", rc, tx);
	if (gc_enabled)
		be_tx_gc(tx);
}

M0_INTERNAL bool m0_be_tx__is_fast(struct m0_be_tx *tx)
{
	return tx->t_fast;
}

M0_INTERNAL int m0_be_tx_fol_add(struct m0_be_tx *tx, struct m0_fol_rec *rec)
{
	M0_PRE(be_tx_is_locked(tx));
	M0_PRE(m0_be_tx_state(tx) == M0_BTS_ACTIVE);

	return m0_fol_rec_encode(rec, &tx->t_payload);
}

M0_INTERNAL void m0_be_tx_force(struct m0_be_tx *tx)
{
	M0_PRE(be_tx_is_locked(tx));
	M0_PRE(m0_be_tx_state(tx) >= M0_BTS_CLOSED);

	M0_ENTRY("tx=%p", tx);

	/* let be engine do the dirty part */
	m0_be_engine__tx_force(tx->t_engine, tx);
}

M0_INTERNAL bool m0_be_tx__is_exclusive(const struct m0_be_tx *tx)
{
	return tx->t_exclusive;
}

M0_INTERNAL void m0_be_tx__recovering_set(struct m0_be_tx *tx)
{
	tx->t_recovering = true;
}

M0_INTERNAL bool m0_be_tx__is_recovering(struct m0_be_tx *tx)
{
	return tx->t_recovering;
}

M0_INTERNAL void m0_be_tx_deconstruct(struct m0_be_tx     *tx,
				      struct m0_be_fmt_tx *ftx)
{
	*ftx = M0_BE_FMT_TX(tx->t_payload, tx->t_id);
}

M0_INTERNAL void m0_be_tx_reconstruct(struct m0_be_tx           *tx,
				      const struct m0_be_fmt_tx *ftx)
{
	int rc;

	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE)));
	M0_PRE(m0_be_tx__is_recovering(tx));

	/*
	 * Temporary solution.
	 * In the future it will be no preallocated payload buffer for each tx.
	 * It will be preallocated in the group and will be filled by
	 * user-supplied callbacks.
	 */
	rc = m0_buf_copy(&tx->t_payload, &ftx->bft_payload);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
	tx->t_id = ftx->bft_id;
}

M0_INTERNAL void m0_be_tx__group_assign(struct m0_be_tx       *tx,
					struct m0_be_tx_group *gr)
{
	if (!m0_be_tx__is_recovering(tx))
		tx->t_group = gr;
}

M0_INTERNAL bool m0_be_should_break(struct m0_be_engine          *eng,
				    const struct m0_be_tx_credit *accum,
				    const struct m0_be_tx_credit *delta)
{
	struct m0_be_tx_credit total = *accum;
	struct m0_be_tx_credit max;

	m0_be_tx_credit_add(&total, delta);
	m0_be_engine_tx_size_max(eng, &max, NULL);
	return !m0_be_tx_credit_le(&total, &max);
}

M0_INTERNAL void m0_be_tx_gc_enable(struct m0_be_tx *tx,
				    void           (*gc_free)(struct m0_be_tx *,
							      void *param),
				    void            *param)
{
	M0_ENTRY("tx=%p gc_free=%p param=%p", tx, gc_free, param);

	M0_PRE(BE_TX_LOCKED_AT_STATE(tx, (M0_BTS_PREPARE, M0_BTS_OPENING,
					  M0_BTS_ACTIVE)));

	tx->t_gc_enabled = true;
	tx->t_gc_free    = gc_free;
	tx->t_gc_param   = param;
}

M0_EXTERN struct m0_sm_conf op_states_conf;
M0_INTERNAL int m0_be_tx_mod_init(void)
{
	m0_sm_conf_init(&be_tx_sm_conf);
	m0_sm_conf_init(&op_states_conf);
	return  m0_sm_addb2_init(&be_tx_sm_conf,
				 M0_AVI_BE_TX_STATE, M0_AVI_BE_TX_COUNTER) ?:
		m0_sm_addb2_init(&op_states_conf, 0, M0_AVI_BE_OP_COUNTER);
}

M0_INTERNAL void m0_be_tx_mod_fini(void)
{
	m0_sm_addb2_fini(&op_states_conf);
	m0_sm_addb2_fini(&be_tx_sm_conf);
	m0_sm_conf_fini(&op_states_conf);
	m0_sm_conf_fini(&be_tx_sm_conf);
}

#undef BE_TX_LOCKED_AT_STATE

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
