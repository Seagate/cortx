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
 * Original creation date: 27-Jan-2013
 */


/**
 * @addtogroup dtm
 *
 * @{
 */

#include "lib/assert.h"
#include "lib/locality.h"

#include "dtm/update.h"
#include "dtm/ltx.h"

static void ltx_persistent_hook(const struct m0_be_tx *tx);
static const struct m0_dtm_history_ops ltx_ops;
static const struct m0_uint128 ltxid = M0_UINT128(0x10ca1, 0x10ca1);

M0_INTERNAL void m0_dtm_ltx_init(struct m0_dtm_ltx *ltx, struct m0_dtm *dtm,
				 struct m0_be_domain *dom)
{
	m0_dtm_controlh_init(&ltx->lx_ch, dtm);
	ltx->lx_ch.ch_history.h_ops = &ltx_ops;
	ltx->lx_ch.ch_history.h_hi.hi_flags |= M0_DHF_OWNED;
	ltx->lx_ch.ch_history.h_rem = NULL;
	ltx->lx_dom = dom;
}

M0_INTERNAL void m0_dtm_ltx_open(struct m0_dtm_ltx *ltx)
{
	m0_be_tx_init(&ltx->lx_tx, 0, ltx->lx_dom, m0_locality_here()->lo_grp,
		      &ltx_persistent_hook, NULL, NULL, NULL);
}

M0_INTERNAL void m0_dtm_ltx_close(struct m0_dtm_ltx *ltx)
{
	m0_dtm_controlh_close(&ltx->lx_ch);
	m0_dtm_oper_done(&ltx->lx_ch.ch_clop, NULL);
	m0_be_tx_close(&ltx->lx_tx);
}

M0_INTERNAL void m0_dtm_ltx_fini(struct m0_dtm_ltx *ltx)
{
	m0_dtm_controlh_fini(&ltx->lx_ch);
	m0_be_tx_fini(&ltx->lx_tx);
}

M0_INTERNAL void m0_dtm_ltx_add(struct m0_dtm_ltx *ltx,
				struct m0_dtm_oper *oper)
{
	m0_dtm_controlh_add(&ltx->lx_ch, oper);
}

static void ltx_persistent_hook(const struct m0_be_tx *tx)
{
	struct m0_dtm_ltx *ltx = M0_AMB(ltx, tx, lx_tx);
	M0_ASSERT(ltx->lx_ch.ch_history.h_hi.hi_flags & M0_DHF_CLOSED);
	m0_dtm_history_persistent(&ltx->lx_ch.ch_history, ~0ULL);
}

static int ltx_find(struct m0_dtm *dtm, const struct m0_dtm_history_type *ht,
		    const struct m0_uint128 *id,
		    struct m0_dtm_history **out)
{
	M0_IMPOSSIBLE("Looking for ltx?");
	return 0;
}

static const struct m0_dtm_history_type_ops ltx_htype_ops = {
	.hito_find = ltx_find
};

enum {
	M0_DTM_HTYPE_LTX = 7
};

M0_INTERNAL const struct m0_dtm_history_type m0_dtm_ltx_htype = {
	.hit_id   = M0_DTM_HTYPE_LTX,
	.hit_name = "local transaction",
	.hit_ops  = &ltx_htype_ops
};

static const struct m0_uint128 *ltx_id(const struct m0_dtm_history *history)
{
	return &ltxid;
}

static void ltx_noop(struct m0_dtm_history *history)
{
}

static const struct m0_dtm_history_ops ltx_ops = {
	.hio_type       = &m0_dtm_ltx_htype,
	.hio_id         = &ltx_id,
	.hio_persistent = &ltx_noop,
	.hio_fixed      = &ltx_noop,
	.hio_update     = &m0_dtm_controlh_update
};


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
