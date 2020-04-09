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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 17-Jun-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/tx_group.h"

#include "lib/misc.h"        /* M0_SET0 */
#include "lib/errno.h"       /* ENOSPC */
#include "lib/memory.h"      /* M0_ALLOC_PTR */

#include "be/tx_internal.h"  /* m0_be_tx__reg_area */
#include "be/domain.h"       /* m0_be_domain_seg */
#include "be/engine.h"       /* m0_be_engine__tx_group_open */
#include "be/addb2.h"        /* M0_AVI_BE_TX_TO_GROUP */

/**
 * @addtogroup be
 *
 * @{
 */

/** A wrapper for a transaction that is being recovered. */
struct be_recovering_tx {
	struct m0_be_tx rtx_tx;
	/** This clink is notified when rtx_tx state is changed. */
	struct m0_clink rtx_open_wait;
	struct m0_be_op rtx_op_open;
	struct m0_be_op rtx_op_gc;
	struct m0_tlink rtx_link;
	uint64_t        rtx_magic;
};

/** A list of transactions that are currently being recovered. */
M0_TL_DESCR_DEFINE(rtxs, "m0_be_tx_group::tg_txs_recovering", static,
		   struct be_recovering_tx, rtx_link, rtx_magic,
		   M0_BE_TX_MAGIC, M0_BE_TX_GROUP_MAGIC);
M0_TL_DEFINE(rtxs, static, struct be_recovering_tx);

M0_TL_DESCR_DEFINE(grp, "m0_be_tx_group::tg_txs", M0_INTERNAL,
		   struct m0_be_tx, t_group_linkage, t_magic,
		   M0_BE_TX_MAGIC, M0_BE_TX_GROUP_MAGIC);
M0_TL_DEFINE(grp, M0_INTERNAL, struct m0_be_tx);

M0_INTERNAL void m0_be_tx_group_stable(struct m0_be_tx_group *gr)
{
	M0_ENTRY();
	m0_be_tx_group_fom_stable(&gr->tg_fom);
	M0_LEAVE();
}

M0_INTERNAL struct m0_sm_group *
m0_be_tx_group__sm_group(struct m0_be_tx_group *gr)
{
	return m0_be_tx_group_fom__sm_group(&gr->tg_fom);
}

M0_INTERNAL bool m0_be_tx_group_is_recovering(struct m0_be_tx_group *gr)
{
	return gr->tg_recovering;
}

static void be_tx_group_reg_area_gather(struct m0_be_tx_group *gr)
{
	struct m0_be_reg_area  *ra;
	struct m0_be_tx_credit  used;
	struct m0_be_tx_credit  prepared;
	struct m0_be_tx_credit  captured;
	struct m0_be_tx        *tx;

	M0_LOG(M0_DEBUG, "gr=%p tx_nr=%zu", gr, m0_be_tx_group_tx_nr(gr));
	/* XXX check if it's the right place */
	if (m0_be_tx_group_tx_nr(gr) > 0) {
		M0_BE_TX_GROUP_TX_FORALL(gr, tx) {
			ra = m0_be_tx__reg_area(tx);
			m0_be_reg_area_merger_add(&gr->tg_merger, ra);

			m0_be_reg_area_prepared(ra, &prepared);
			m0_be_reg_area_captured(ra, &captured);
			m0_be_reg_area_used(ra, &used);
			M0_LOG(M0_DEBUG, "tx=%p t_prepared="BETXCR_F" "
			       "t_payload_prepared=%lu captured="BETXCR_F" "
			       "used="BETXCR_F" t_payload.b_nob=%"PRIu64,
			       tx, BETXCR_P(&prepared), tx->t_payload_prepared,
			       BETXCR_P(&captured), BETXCR_P(&used),
			       tx->t_payload.b_nob);
		} M0_BE_TX_GROUP_TX_ENDFOR;
		m0_be_reg_area_merger_merge_to(&gr->tg_merger, &gr->tg_reg_area);
	}
	m0_be_reg_area_optimize(&gr->tg_reg_area);
}

static void be_tx_group_payload_gather(struct m0_be_tx_group *gr)
{
	/*
	 * In the future tx payload will be set via callbacks.
	 * This function will execute these callbacks to gather payload for
	 * each transaction from the group.
	 *
	 * Currently tx payload is filled before tx close, so this
	 * function does nothing.
	 */
}

/*
 * Fill m0_be_tx_group_format with data from the group.
 */
static void be_tx_group_deconstruct(struct m0_be_tx_group *gr)
{
	struct m0_be_fmt_tx  ftx;
	struct m0_be_reg_d  *rd;
	struct m0_be_tx     *tx;

	/* TODO add other fields */

	M0_BE_TX_GROUP_TX_FORALL(gr, tx) {
		m0_be_tx_deconstruct(tx, &ftx);
		m0_be_group_format_tx_add(&gr->tg_od, &ftx);
	} M0_BE_TX_GROUP_TX_ENDFOR;

	M0_BE_REG_AREA_FORALL(&gr->tg_reg_area, rd) {
		m0_be_group_format_reg_log_add(&gr->tg_od, rd);
	};
}

M0_INTERNAL void m0_be_tx_group_close(struct m0_be_tx_group *gr)
{
	struct m0_be_tx *tx;
	m0_bcount_t      size_reserved = 0;

	M0_ENTRY("gr=%p recovering=%d", gr, (int)gr->tg_recovering);

	if (!gr->tg_recovering) {
		be_tx_group_reg_area_gather(gr);
		be_tx_group_payload_gather(gr);
		be_tx_group_deconstruct(gr);
		M0_BE_TX_GROUP_TX_FORALL(gr, tx) {
			size_reserved += tx->t_log_reserved_size;
		} M0_BE_TX_GROUP_TX_ENDFOR;
		m0_be_group_format_log_use(&gr->tg_od, size_reserved);
	}
	m0_be_tx_group_fom_handle(&gr->tg_fom);

	M0_LEAVE();
}

M0_INTERNAL void m0_be_tx_group_reset(struct m0_be_tx_group *gr)
{
	M0_PRE(grp_tlist_is_empty(&gr->tg_txs));
	M0_PRE(rtxs_tlist_is_empty(&gr->tg_txs_recovering));
	M0_PRE(gr->tg_nr_unclosed == 0);
	M0_PRE(gr->tg_nr_unstable == 0);

	M0_SET0(&gr->tg_used);
	M0_SET0(&gr->tg_log_reserved);
	gr->tg_payload_prepared = 0;
	gr->tg_recovering       = false;
	m0_be_reg_area_reset(&gr->tg_reg_area);
	m0_be_reg_area_merger_reset(&gr->tg_merger);
	m0_be_group_format_reset(&gr->tg_od);
	m0_be_tx_group_fom_reset(&gr->tg_fom);
}

M0_INTERNAL void m0_be_tx_group_prepare(struct m0_be_tx_group *gr,
                                        struct m0_be_op       *op)
{
	m0_be_group_format_prepare(&gr->tg_od, op);
}

M0_INTERNAL int m0_be_tx_group_init(struct m0_be_tx_group     *gr,
				    struct m0_be_tx_group_cfg *gr_cfg)
{
	int rc;
	int i;

	gr->tg_cfg = *gr_cfg;
	gr->tg_cfg.tgc_format = (struct m0_be_group_format_cfg){
		.gfc_fmt_cfg = {
			.fgc_tx_nr_max	      = gr_cfg->tgc_tx_nr_max,
			.fgc_reg_nr_max	      = gr_cfg->tgc_size_max.tc_reg_nr,
			.fgc_payload_size_max = gr_cfg->tgc_payload_max,
			.fgc_reg_size_max    = gr_cfg->tgc_size_max.tc_reg_size,
			.fgc_seg_nr_max	      = gr_cfg->tgc_seg_nr_max,
		},
		.gfc_log = gr_cfg->tgc_log,
		.gfc_log_discard = gr_cfg->tgc_log_discard,
		.gfc_pd = gr_cfg->tgc_pd,
	};
	/* XXX temporary block begin */
	gr->tg_size             = gr_cfg->tgc_size_max;
	gr->tg_payload_prepared = 0;
	gr->tg_log              = gr_cfg->tgc_log;
	gr->tg_domain           = gr_cfg->tgc_domain;
	gr->tg_engine           = gr_cfg->tgc_engine;
	/* XXX temporary block end */
	grp_tlist_init(&gr->tg_txs);
	rtxs_tlist_init(&gr->tg_txs_recovering);
	m0_be_tx_group_fom_init(&gr->tg_fom, gr, gr->tg_cfg.tgc_reqh);
	rc = m0_be_group_format_init(&gr->tg_od, &gr->tg_cfg.tgc_format,
				     gr, gr->tg_log);
	M0_ASSERT(rc == 0);	/* XXX */
	rc = m0_be_reg_area_init(&gr->tg_reg_area, &gr->tg_cfg.tgc_size_max,
				 M0_BE_REG_AREA_DATA_NOCOPY);
	M0_ASSERT(rc == 0);	/* XXX */
	rc = m0_be_reg_area_merger_init(&gr->tg_merger, gr_cfg->tgc_tx_nr_max);
	M0_ASSERT(rc == 0);     /* XXX */
	M0_ALLOC_ARR(gr->tg_rtxs, gr->tg_cfg.tgc_tx_nr_max);
	M0_ASSERT(gr->tg_rtxs != NULL); /* XXX */
	for (i = 0; i < gr->tg_cfg.tgc_tx_nr_max; ++i) {
		m0_be_op_init(&gr->tg_rtxs[i].rtx_op_open);
		m0_be_op_init(&gr->tg_rtxs[i].rtx_op_gc);
	}
	return 0;
}

M0_INTERNAL bool m0_be_tx_group__invariant(struct m0_be_tx_group *gr)
{
	return m0_be_tx_group_tx_nr(gr) <= gr->tg_cfg.tgc_tx_nr_max;
}

M0_INTERNAL void m0_be_tx_group_fini(struct m0_be_tx_group *gr)
{
	int i;

	for (i = 0; i < gr->tg_cfg.tgc_tx_nr_max; ++i) {
		m0_be_op_fini(&gr->tg_rtxs[i].rtx_op_gc);
		m0_be_op_fini(&gr->tg_rtxs[i].rtx_op_open);
	}
	m0_free(gr->tg_rtxs);
	m0_be_reg_area_merger_fini(&gr->tg_merger);
	m0_be_reg_area_fini(&gr->tg_reg_area);
	m0_be_tx_group_fom_fini(&gr->tg_fom);
	rtxs_tlist_fini(&gr->tg_txs_recovering);
	grp_tlist_fini(&gr->tg_txs);
}

static void be_tx_group_tx_to_gr_map(const struct m0_be_tx       *tx,
				     const struct m0_be_tx_group *gr)
{
	uint64_t tid = m0_sm_id_get(&tx->t_sm);
	uint64_t gid = m0_sm_id_get(&gr->tg_fom.tgf_gen.fo_sm_phase);

	M0_ADDB2_ADD(M0_AVI_BE_TX_TO_GROUP, tid, gid);
}

static void be_tx_group_tx_add(struct m0_be_tx_group *gr, struct m0_be_tx *tx)
{
	M0_LOG(M0_DEBUG, "tx=%p group=%p", tx, gr);
	grp_tlink_init_at_tail(tx, &gr->tg_txs);
	M0_CNT_INC(gr->tg_nr_unclosed);
	M0_CNT_INC(gr->tg_nr_unstable);
	be_tx_group_tx_to_gr_map(tx, gr);
}

M0_INTERNAL int m0_be_tx_group_tx_add(struct m0_be_tx_group *gr,
				      struct m0_be_tx       *tx)
{
	struct m0_be_tx_credit group_used = gr->tg_used;
	int                    rc;

	M0_ENTRY("gr=%p tx=%p t_prepared="BETXCR_F" t_payload_prepared=%lu "
	         "tg_used="BETXCR_F" tg_payload_prepared=%lu group_tx_nr=%zu",
	         gr, tx, BETXCR_P(&tx->t_prepared), tx->t_payload_prepared,
	         BETXCR_P(&gr->tg_used), gr->tg_payload_prepared,
	         m0_be_tx_group_tx_nr(gr));
	M0_PRE(m0_be_tx_group__invariant(gr));
	M0_PRE(equi(m0_be_tx__is_recovering(tx),
		    m0_be_tx_group_is_recovering(gr)));

	if (m0_be_tx__is_recovering(tx)) {
		be_tx_group_tx_add(gr, tx);
		rc = 0; /* XXX check for ENOSPC */
	} else {
		m0_be_tx_credit_add(&group_used, &tx->t_prepared);

		if (m0_be_tx_group_tx_nr(gr) == gr->tg_cfg.tgc_tx_nr_max) {
			rc = -EXFULL;
		} else if (!m0_be_tx_credit_le(&group_used, &gr->tg_size) ||
		           gr->tg_payload_prepared + tx->t_payload_prepared >
		           gr->tg_cfg.tgc_payload_max) {
			rc = -ENOSPC;
		} else {
			be_tx_group_tx_add(gr, tx);
			gr->tg_used              = group_used;
			gr->tg_payload_prepared += tx->t_payload_prepared;
			rc = 0;
		}
	}
	M0_POST(m0_be_tx_group__invariant(gr));
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_tx_group_tx_closed(struct m0_be_tx_group *gr,
                                          struct m0_be_tx       *tx)
{
	struct m0_be_tx_credit tx_prepared;
	struct m0_be_tx_credit tx_captured;
	m0_bcount_t            log_used;
	m0_bcount_t            log_unused;
	uint64_t               tx_sm_id;

	if (m0_be_tx__is_recovering(tx))
		return;

	m0_be_reg_area_prepared(m0_be_tx__reg_area(tx), &tx_prepared);
	m0_be_reg_area_captured(m0_be_tx__reg_area(tx), &tx_captured);
	M0_ASSERT(m0_be_tx_credit_le(&tx_prepared, &gr->tg_used));
	M0_ASSERT(m0_be_tx_credit_le(&tx_captured, &tx_prepared));
	M0_ASSERT(tx->t_payload_prepared <= gr->tg_payload_prepared);
	M0_ASSERT(tx->t_payload.b_nob    <= tx->t_payload_prepared);
	M0_LOG(M0_DEBUG, "gr=%p tx=%p tx_prepared="BETXCR_F" "
	       "t_payload_prepared=%"PRIu64" tg_used="BETXCR_F" "
	       "tg_payload_prepared=%"PRIu64, gr, tx, BETXCR_P(&tx_prepared),
	       tx->t_payload_prepared, BETXCR_P(&gr->tg_used),
	       gr->tg_payload_prepared);

	m0_be_tx_credit_sub(&gr->tg_used, &tx_prepared);
	m0_be_tx_credit_add(&gr->tg_used, &tx_captured);
	gr->tg_payload_prepared -= tx->t_payload_prepared;
	gr->tg_payload_prepared += tx->t_payload.b_nob;
	M0_LOG(M0_DEBUG, "gr=%p tx=%p tx_captured="BETXCR_F" "
	       "t_payload.b_nob=%"PRIu64" tg_used="BETXCR_F" "
	       "tg_payload_prepared=%"PRIu64, gr, tx, BETXCR_P(&tx_captured),
	       tx->t_payload.b_nob, BETXCR_P(&gr->tg_used),
	       gr->tg_payload_prepared);

	log_used = m0_be_group_format_log_reserved_size(
			    gr->tg_cfg.tgc_log, &tx_captured,
			    tx->t_payload.b_nob);
	M0_ASSERT(tx->t_log_reserved_size >= log_used);

	tx_sm_id = m0_sm_id_get(&tx->t_sm);
	M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id, M0_AVI_BE_TX_ATTR_LOG_RESERVED_SZ,
		     tx->t_log_reserved_size);
	M0_ADDB2_ADD(M0_AVI_ATTR, tx_sm_id, M0_AVI_BE_TX_ATTR_LOG_USED,
		     log_used);

	log_unused = tx->t_log_reserved_size - log_used;
	tx->t_log_reserved_size = log_used;
	m0_be_log_unreserve(gr->tg_cfg.tgc_log, log_unused);
}

M0_INTERNAL void m0_be_tx_group_tx_del(struct m0_be_tx_group *gr,
				       struct m0_be_tx *tx)
{
	M0_LOG(M0_DEBUG, "tx=%p gr=%p", tx, gr);
	grp_tlink_del_fini(tx);
	be_tx_group_tx_to_gr_map(tx, gr);
}

M0_INTERNAL size_t m0_be_tx_group_tx_nr(struct m0_be_tx_group *gr)
{
	return grp_tlist_length(&gr->tg_txs);
}

M0_INTERNAL void m0_be_tx_group_open(struct m0_be_tx_group *gr)
{
	m0_be_engine__tx_group_ready(gr->tg_engine, gr);
}

M0_INTERNAL int m0_be_tx_group_start(struct m0_be_tx_group *gr)
{
	return m0_be_tx_group_fom_start(&gr->tg_fom);
}

M0_INTERNAL void m0_be_tx_group_stop(struct m0_be_tx_group *gr)
{
	m0_be_tx_group_fom_stop(&gr->tg_fom);
}

M0_INTERNAL int m0_be_tx_group__allocate(struct m0_be_tx_group *gr)
{
	return m0_be_group_format_allocate(&gr->tg_od);
}

M0_INTERNAL void m0_be_tx_group__deallocate(struct m0_be_tx_group *gr)
{
	m0_be_group_format_fini(&gr->tg_od);
}

M0_INTERNAL void m0_be_tx_group_seg_place_prepare(struct m0_be_tx_group *gr)
{
	struct m0_be_reg_d *rd;

	if (!gr->tg_recovering) {
		M0_BE_REG_AREA_FORALL(&gr->tg_reg_area, rd)
			m0_be_group_format_reg_seg_add(&gr->tg_od, rd);
	}
	m0_be_group_format_seg_place_prepare(&gr->tg_od);
}

M0_INTERNAL void m0_be_tx_group_seg_place(struct m0_be_tx_group *gr,
					  struct m0_be_op       *op)
{
	m0_be_group_format_seg_place(&gr->tg_od, op);
}

M0_INTERNAL void m0_be_tx_group_encode(struct m0_be_tx_group *gr)
{
	m0_be_group_format_encode(&gr->tg_od);
}

M0_INTERNAL void m0_be_tx_group_log_write(struct m0_be_tx_group *gr,
					  struct m0_be_op       *op)
{
	m0_be_group_format_log_write(&gr->tg_od, op);
}

M0_INTERNAL void
m0_be_tx_group__tx_state_post(struct m0_be_tx_group *gr,
			      enum m0_be_tx_state    state,
			      bool                   del_tx_from_group)
{
	struct m0_be_tx *tx;

	M0_ENTRY("gr=%p state=%s group_tx_nr=%zd",
		 gr, m0_be_tx_state_name(state), m0_be_tx_group_tx_nr(gr));

	M0_ASSERT(m0_be_tx_group_tx_nr(gr) > 0);

	M0_BE_TX_GROUP_TX_FORALL(gr, tx) {
		if (del_tx_from_group)
			m0_be_tx_group_tx_del(gr, tx);
		m0_be_tx__state_post(tx, state);
	} M0_BE_TX_GROUP_TX_ENDFOR;

	M0_LEAVE();
}

M0_INTERNAL void m0_be_tx_group_recovery_prepare(struct m0_be_tx_group *gr,
						 struct m0_be_log      *log)
{
	m0_be_group_format_recovery_prepare(&gr->tg_od, log);
	m0_be_tx_group_fom_recovery_prepare(&gr->tg_fom);
	gr->tg_recovering = true;
}

M0_INTERNAL void m0_be_tx_group_log_read(struct m0_be_tx_group *gr,
					 struct m0_be_op       *op)
{
	return m0_be_group_format_log_read(&gr->tg_od, op);
}

M0_INTERNAL int m0_be_tx_group_decode(struct m0_be_tx_group *gr)
{
	return m0_be_group_format_decode(&gr->tg_od);
}

static void be_tx_group_reconstruct_reg_area(struct m0_be_tx_group *gr)
{
	struct m0_be_group_format *gft = &gr->tg_od;
	struct m0_be_seg          *seg;
	struct m0_be_reg_d         rd;
	uint32_t                   reg_nr;
	uint32_t                   i;

	reg_nr = m0_be_group_format_reg_nr(gft);
	for (i = 0; i < reg_nr; ++i) {
		m0_be_group_format_reg_get(gft, i, &rd);
		seg = m0_be_domain_seg(gr->tg_domain, rd.rd_reg.br_addr);
		/*
		 * It is possible that seg is already removed from seg0.
		 * In this case the safest way is to ignore such regions.
		 * TODO add some code to prevent ABA problem (another segment
		 * is added at the same address as previous is removed).
		 */
		if (seg != NULL) {
			m0_be_reg_area_capture(&gr->tg_reg_area, &rd);
			rd.rd_reg.br_seg = seg;
			m0_be_group_format_reg_seg_add(&gr->tg_od, &rd);
		}
	}
}

static struct be_recovering_tx *
tx2tx_group_recovering_tx(struct m0_be_tx *tx)
{
	return container_of(tx, struct be_recovering_tx, rtx_tx);
}

static bool be_tx_group_recovering_tx_open(struct m0_clink *clink)
{
	struct be_recovering_tx *rtx;

	rtx = container_of(clink, struct be_recovering_tx, rtx_open_wait);
	if (m0_be_tx_state(&rtx->rtx_tx) == M0_BTS_ACTIVE) {
		m0_clink_del(clink);
		m0_be_op_done(&rtx->rtx_op_open);
	}
	return false;
}

static void be_tx_group_recovering_gc(struct m0_be_tx *tx, void *param)
{
	struct be_recovering_tx *rtx;

	rtx = tx2tx_group_recovering_tx(tx);
	rtxs_tlink_del_fini(rtx);
	m0_clink_fini(&rtx->rtx_open_wait);
	m0_be_op_done(&rtx->rtx_op_gc);
}

/**
 * Highlighs:
 * - transactions are reconstructed in the given sm group;
 * - transactions are allocated using m0_alloc();
 * - transactions are freed using m0_be_tx_gc_enable().
 */
static void be_tx_group_reconstruct_transactions(struct m0_be_tx_group *gr,
						 struct m0_sm_group    *sm_grp)
{
	struct be_recovering_tx   *rtx;
	struct m0_be_group_format *gft = &gr->tg_od;
	struct m0_be_fmt_tx        ftx;
	struct m0_be_tx           *tx;
	uint32_t                   tx_nr;
	uint32_t                   i;

	tx_nr = m0_be_group_format_tx_nr(gft);
	for (i = 0; i < tx_nr; ++i) {
		rtx = &gr->tg_rtxs[i];
		/*
		 * rtx->rtx_op_open and rtx->rtx_op_gc are initialised in
		 * m0_be_tx_group_init().
		 */
		M0_SET0(&rtx->rtx_tx);
		M0_SET0(&rtx->rtx_open_wait);
		rtxs_tlink_init_at_tail(rtx, &gr->tg_txs_recovering);
		m0_be_group_format_tx_get(gft, i, &ftx);
		tx = &rtx->rtx_tx;
		m0_be_tx_init(tx, 0, gr->tg_domain,
			      sm_grp, NULL, NULL, NULL, NULL);
		m0_be_tx__group_assign(tx, gr);
		m0_be_tx__recovering_set(tx);
		m0_be_tx_reconstruct(tx, &ftx);
		m0_be_op_reset(&rtx->rtx_op_open);
		m0_be_op_reset(&rtx->rtx_op_gc);
		m0_clink_init(&rtx->rtx_open_wait,
			      &be_tx_group_recovering_tx_open);
		m0_clink_add(&tx->t_sm.sm_chan, &rtx->rtx_open_wait);
		m0_be_tx_gc_enable(tx, &be_tx_group_recovering_gc, NULL);
	}
}

M0_INTERNAL int m0_be_tx_group_reconstruct(struct m0_be_tx_group *gr,
					   struct m0_sm_group    *sm_grp)
{
	be_tx_group_reconstruct_reg_area(gr);
	be_tx_group_reconstruct_transactions(gr, sm_grp);
	return 0; /* XXX no error handling yet. It will be fixed. */
}

M0_INTERNAL void m0_be_tx_group_reconstruct_tx_open(struct m0_be_tx_group *gr,
						    struct m0_be_op       *op)
{
	struct be_recovering_tx *rtx;

	m0_tl_for(rtxs, &gr->tg_txs_recovering, rtx) {
		m0_be_op_set_add(op, &rtx->rtx_op_open);
	} m0_tl_endfor;

	m0_tl_for(rtxs, &gr->tg_txs_recovering, rtx) {
		m0_be_op_active(&rtx->rtx_op_open);
		m0_be_tx_open(&rtx->rtx_tx);
	} m0_tl_endfor;
}

M0_INTERNAL void
m0_be_tx_group_reconstruct_tx_close(struct m0_be_tx_group *gr,
                                    struct m0_be_op       *op_gc)
{
	struct be_recovering_tx *rtx;

	m0_tl_for(rtxs, &gr->tg_txs_recovering, rtx) {
		m0_be_op_set_add(op_gc, &rtx->rtx_op_gc);
	} m0_tl_endfor;

	m0_tl_for(rtxs, &gr->tg_txs_recovering, rtx) {
		m0_be_op_active(&rtx->rtx_op_gc);
		m0_be_tx_close(&rtx->rtx_tx);
	} m0_tl_endfor;
}

/*
 * It will perform actual I/O when paged implemented so op is added
 * to the function parameters list.
 */
M0_INTERNAL int m0_be_tx_group_reapply(struct m0_be_tx_group *gr,
				       struct m0_be_op       *op)
{
	struct m0_be_reg_d *rd;

	m0_be_op_active(op);

	M0_BE_REG_AREA_FORALL(&gr->tg_reg_area, rd) {
		memcpy(rd->rd_reg.br_addr, rd->rd_buf, rd->rd_reg.br_size);
	};

	m0_be_op_done(op);
	return 0;
}

M0_INTERNAL void m0_be_tx_group_discard(struct m0_be_log_discard      *ld,
                                        struct m0_be_log_discard_item *ldi)
{
	m0_be_group_format_discard(ld, ldi);
}

M0_INTERNAL void
m0_be_tx_group_seg_io_credit(struct m0_be_tx_group_cfg *gr_cfg,
                             struct m0_be_io_credit    *io_cred)
{
	struct m0_be_group_format_cfg gft_cfg = {
		.gfc_fmt_cfg = {
			.fgc_reg_nr_max	  = gr_cfg->tgc_size_max.tc_reg_nr,
			.fgc_reg_size_max = gr_cfg->tgc_size_max.tc_reg_size,
			.fgc_seg_nr_max	  = gr_cfg->tgc_seg_nr_max,
		}
	};
	m0_be_group_format_seg_io_credit(&gft_cfg, io_cred);
}

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
