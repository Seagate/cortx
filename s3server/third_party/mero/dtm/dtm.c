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
 * Original creation date: 04/01/2010
 */

/**
 * @addtogroup dtm Distributed transaction manager
 * @{
 */

#include "lib/misc.h"              /* m0_forall, ARRAY_SIZE */

#include "dtm/nucleus.h"
#include "dtm/fol.h"
#include "dtm/dtm.h"
#include "dtm/dtm_internal.h"
#include "dtm/dtm_update_xc.h"
#include "dtm/update_xc.h"
#include "dtm/operation_xc.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "lib/trace.h"

M0_INTERNAL void m0_dtm_init(struct m0_dtm *dtm, struct m0_uint128 *id)
{
	int i;

	dtm->d_id = *id;
	m0_dtm_nu_init(&dtm->d_nu);
	m0_dtm_history_type_register(dtm, &m0_dtm_fol_htype);
	m0_dtm_fol_init(&dtm->d_fol, dtm);
	exc_tlist_init(&dtm->d_excited);
	for (i = 0; i < ARRAY_SIZE(dtm->d_cat); ++i)
		m0_dtm_catalogue_init(&dtm->d_cat[i]);
}

M0_INTERNAL void m0_dtm_fini(struct m0_dtm *dtm)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dtm->d_cat); ++i)
		m0_dtm_catalogue_fini(&dtm->d_cat[i]);
	exc_tlist_fini(&dtm->d_excited);
	m0_dtm_fol_fini(&dtm->d_fol);
	m0_dtm_history_type_deregister(dtm, &m0_dtm_fol_htype);
	M0_PRE(m0_forall(i, ARRAY_SIZE(dtm->d_htype), dtm->d_htype[i] == NULL));
	m0_dtm_nu_fini(&dtm->d_nu);
}

M0_INTERNAL void m0_dtx_init(struct m0_dtx *tx,
			     struct m0_be_domain *be_domain,
			     struct m0_sm_group  *sm_group)
{
	M0_PRE(be_domain != NULL);

	m0_be_tx_init(&tx->tx_betx, 0, be_domain, sm_group,
		      NULL, NULL, NULL, NULL);
	tx->tx_betx_cred = M0_BE_TX_CREDIT(0, 0);
	tx->tx_state = M0_DTX_INIT;
	m0_fol_rec_init(&tx->tx_fol_rec, NULL);
}

M0_INTERNAL void m0_dtx_prep(struct m0_dtx *tx,
			     const struct m0_be_tx_credit *cred)
{
	m0_be_tx_prep(&tx->tx_betx, cred);
}

M0_INTERNAL void m0_dtx_open(struct m0_dtx *tx)
{
	M0_PRE(tx->tx_state == M0_DTX_INIT);
	M0_PRE(m0_be_tx_state(&tx->tx_betx) == M0_BTS_PREPARE);
	m0_be_tx_prep(&tx->tx_betx, &tx->tx_betx_cred);
	m0_be_tx_open(&tx->tx_betx);
}

M0_INTERNAL void m0_dtx_opened(struct m0_dtx *tx)
{
	M0_PRE(tx->tx_state == M0_DTX_INIT);
	M0_PRE(m0_be_tx_state(&tx->tx_betx) == M0_BTS_ACTIVE);
	tx->tx_state = M0_DTX_OPEN;
}

M0_INTERNAL int m0_dtx_open_sync(struct m0_dtx *tx)
{
	int rc;

	m0_dtx_open(tx);
	rc = m0_be_tx_timedwait(&tx->tx_betx,
				M0_BITS(M0_BTS_ACTIVE, M0_BTS_FAILED),
				M0_TIME_NEVER);
	if (m0_be_tx_state(&tx->tx_betx) == M0_BTS_ACTIVE)
		m0_dtx_opened(tx);

	return M0_RC(rc);
}

M0_INTERNAL void m0_dtx_done(struct m0_dtx *tx)
{
	M0_PRE(M0_IN(tx->tx_state, (M0_DTX_INIT, M0_DTX_OPEN)));
	m0_be_tx_close(&tx->tx_betx);
	tx->tx_state = M0_DTX_DONE;
}

M0_INTERNAL int m0_dtx_done_sync(struct m0_dtx *tx)
{
	int rc;

	m0_dtx_done(tx);
	rc = m0_be_tx_timedwait(&tx->tx_betx, M0_BITS(M0_BTS_DONE),
				M0_TIME_NEVER);
	M0_ASSERT(m0_be_tx_state(&tx->tx_betx) == M0_BTS_DONE);

	return M0_RC(rc);
}

M0_INTERNAL void m0_dtx_fini(struct m0_dtx *tx)
{
	M0_PRE(M0_IN(tx->tx_state, (M0_DTX_INIT, M0_DTX_DONE)));
	m0_be_tx_fini(&tx->tx_betx);
	m0_fol_rec_fini(&tx->tx_fol_rec);
	tx->tx_state = M0_DTX_INVALID;
}

M0_INTERNAL int m0_dtm_global_init(void)
{
	m0_dtm_nuclei_init();
	return m0_dtm_remote_global_init();
}

M0_INTERNAL void m0_dtm_global_fini(void)
{
	m0_dtm_remote_global_fini();
	m0_dtm_nuclei_fini();
}

M0_INTERNAL struct m0_dtm *nu_dtm(struct m0_dtm_nu *nu)
{
	return container_of(nu, struct m0_dtm, d_nu);
}

M0_INTERNAL void dtm_lock(struct m0_dtm *dtm)
{
	nu_lock(&dtm->d_nu);
}

M0_INTERNAL void dtm_unlock(struct m0_dtm *dtm)
{
	nu_unlock(&dtm->d_nu);
}

M0_INTERNAL int m0_dtx_fol_add(struct m0_dtx *tx)
{
	return m0_be_tx_fol_add(&tx->tx_betx, &tx->tx_fol_rec);
}

#undef M0_TRACE_SUBSYSTEM

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
