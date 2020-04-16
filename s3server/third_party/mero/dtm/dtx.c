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
#include "lib/memory.h"
#include "lib/errno.h"              /* ENOMEM, ENOSYS */

#include "dtm/catalogue.h"
#include "dtm/dtm_internal.h"
#include "dtm/history.h"
#include "dtm/dtm.h"
#include "dtm/dtx.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "lib/trace.h"

struct m0_dtm_dtx_party {
	struct m0_dtm_dtx     *pa_dtx;
	struct m0_dtm_controlh pa_ch;
};

static const struct m0_dtm_history_ops dtx_ops;
static const struct m0_dtm_history_ops dtx_srv_ops;
static struct m0_dtm_controlh *dtx_get(struct m0_dtm_dtx *dtx,
				       struct m0_dtm_remote *rem);
static struct m0_dtm_history *dtx_srv_alloc(struct m0_dtm *dtm,
					    const struct m0_uint128 *id,
					    void *datum);
static struct m0_dtm_history *pa_history(struct m0_dtm_dtx_party *pa);

M0_INTERNAL int m0_dtm_dtx_init(struct m0_dtm_dtx *dtx,
				const struct m0_uint128 *id,
				struct m0_dtm *dtm, uint32_t nr_max)
{
	dtx->dt_id = *id;
	dtx->dt_dtm = dtm;
	dtx->dt_nr_max = nr_max;
	dtx->dt_nr = dtx->dt_nr_fixed = 0;
	M0_ALLOC_ARR(dtx->dt_party, nr_max);
	return dtx->dt_party == NULL ? -ENOMEM : 0;
}

M0_INTERNAL void m0_dtm_dtx_fini(struct m0_dtm_dtx *dtx)
{
	if (dtx->dt_party != NULL) {
		uint32_t i;

		for (i = 0; i < dtx->dt_nr; ++i)
			m0_dtm_controlh_fini(&dtx->dt_party[i].pa_ch);
		m0_free(dtx->dt_party);
	}
}

M0_INTERNAL void m0_dtm_dtx_add(struct m0_dtm_dtx *dtx,
				struct m0_dtm_oper *oper)
{
	oper_for(oper, i) {
		if (oper_update_unique(oper, i))
			m0_dtm_controlh_add(dtx_get(dtx, UPDATE_REM(i)),
					    oper);
	} oper_endfor;
}

M0_INTERNAL void m0_dtm_dtx_close(struct m0_dtm_dtx *dtx)
{
	uint32_t i;

	for (i = 0; i < dtx->dt_nr; ++i) {
		struct m0_dtm_history *history = pa_history(&dtx->dt_party[i]);
		struct m0_dtm_update  *update;

		update = up_update(history_latest(history));
		M0_ASSERT(update != NULL);
		m0_dtm_controlh_fuse_close(update);
		M0_ASSERT(m0_dtm_history_invariant(history));
	}
}

static void dtx_noop(void *unused)
{}

static int dtx_find(struct m0_dtm *dtm, const struct m0_dtm_history_type *ht,
		    const struct m0_uint128 *id,
		    struct m0_dtm_history **out)
{
	M0_IMPOSSIBLE("Looking for dtx?");
	return M0_ERR(-ENOSYS);
}

static const struct m0_dtm_history_type_ops dtx_htype_ops = {
	.hito_find = &dtx_find
};

enum {
	M0_DTM_HTYPE_DTX     = 8,
	M0_DTM_HTYPE_DTX_SRV = 9
};

M0_INTERNAL const struct m0_dtm_history_type m0_dtm_dtx_htype = {
	.hit_id     = M0_DTM_HTYPE_DTX,
	.hit_rem_id = M0_DTM_HTYPE_DTX_SRV,
	.hit_name   = "dtx-party",
	.hit_ops    = &dtx_htype_ops
};

static const struct m0_uint128 *dtx_id(const struct m0_dtm_history *history)
{
	struct m0_dtm_dtx_party *pa;

	pa = M0_AMB(pa, history, pa_ch.ch_history);
	return &pa->pa_dtx->dt_id;
}

static void dtx_fixed(struct m0_dtm_history *history)
{
	struct m0_dtm_dtx_party *pa;
	struct m0_dtm_dtx       *dx;

	pa = M0_AMB(pa, history, pa_ch.ch_history);
	dx = pa->pa_dtx;
	M0_ASSERT(dx->dt_nr_fixed < dx->dt_nr);
	if (++dx->dt_nr_fixed == dx->dt_nr) {
	}
}

static const struct m0_dtm_history_ops dtx_ops = {
	.hio_type       = &m0_dtm_dtx_htype,
	.hio_id         = &dtx_id,
	.hio_persistent = (void *)&dtx_noop,
	.hio_fixed      = &dtx_fixed,
	.hio_update     = &m0_dtm_controlh_update
};

static struct m0_dtm_history *pa_history(struct m0_dtm_dtx_party *pa)
{
	return &pa->pa_ch.ch_history;
}

static struct m0_dtm_controlh *dtx_get(struct m0_dtm_dtx *dtx,
				       struct m0_dtm_remote *rem)
{
	uint32_t                 i;
	struct m0_dtm_dtx_party *pa;
	struct m0_dtm_history   *history;

	for (i = 0, pa = dtx->dt_party; i < dtx->dt_nr; ++i, ++pa) {
		if (pa_history(pa)->h_rem == rem)
			return &pa->pa_ch;
	}
	M0_ASSERT(dtx->dt_nr < dtx->dt_nr_max);
	m0_dtm_controlh_init(&pa->pa_ch, dtx->dt_dtm);
	history = pa_history(pa);
	history->h_rem = rem;
	history->h_ops = &dtx_ops;
	history->h_hi.hi_flags |= M0_DHF_OWNED;
	pa->pa_dtx = dtx;
	dtx->dt_nr++;
	return &pa->pa_ch;
}

static int dtx_srv_find(struct m0_dtm *dtm, const struct m0_dtm_history_type *ht,
			const struct m0_uint128 *id,
			struct m0_dtm_history **out)
{
	return m0_dtm_catalogue_find(&dtm->d_cat[M0_DTM_HTYPE_DTX_SRV], dtm,
				     id, &dtx_srv_alloc, NULL, out);
}

static const struct m0_dtm_history_type_ops dtx_srv_htype_ops = {
	.hito_find = &dtx_srv_find
};

M0_INTERNAL const struct m0_dtm_history_type m0_dtm_dtx_srv_htype = {
	.hit_id     = M0_DTM_HTYPE_DTX_SRV,
	.hit_rem_id = M0_DTM_HTYPE_DTX,
	.hit_name   = "distributed transaction service side",
	.hit_ops    = &dtx_srv_htype_ops
};

static const struct m0_uint128 *dtx_srv_id(const struct m0_dtm_history *history)
{
	struct m0_dtm_dtx_srv *dtx;

	dtx = M0_AMB(dtx, history, ds_history);
	return &dtx->ds_id;
}

static const struct m0_dtm_history_ops dtx_srv_ops = {
	.hio_type       = &m0_dtm_dtx_srv_htype,
	.hio_id         = &dtx_srv_id,
	.hio_persistent = (void *)&dtx_noop,
	.hio_fixed      = (void *)&dtx_noop,
	.hio_update     = &m0_dtm_controlh_update
};

static struct m0_dtm_history *dtx_srv_alloc(struct m0_dtm *dtm,
					    const struct m0_uint128 *id,
					    void *datum)
{
	struct m0_dtm_dtx_srv *dtx;
	struct m0_dtm_history *history;

	M0_ALLOC_PTR(dtx);
	if (dtx != NULL) {
		history = &dtx->ds_history;
		dtx->ds_id = *id;
		m0_dtm_history_init(&dtx->ds_history, dtm);
		history->h_ops = &dtx_srv_ops;
	} else
		history = NULL;
	return history;
}

/** @} end of dtm group */

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
