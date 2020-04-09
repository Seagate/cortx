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
 * Original creation date: 13-Aug-2013
 */

/**
 * @addtogroup ut
 *
 * @{
 */

#include "ut/be.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ut/ut.h"         /* M0_UT_ASSERT */
#include "be/ut/helper.h"  /* m0_be_ut_backend */
#include "be/op.h"         /* M0_BE_OP_SYNC */
#include "lib/misc.h"      /* M0_BITS */
#include "lib/memory.h"    /* M0_ALLOC_PTR */

#include "reqh/reqh.h"

M0_INTERNAL void
m0_ut_backend_init(struct m0_be_ut_backend *be, struct m0_be_ut_seg *seg)
{
	m0_be_ut_backend_init(be);
	m0_be_ut_seg_init(seg, be, 1 << 20 /* 1 MB */);
}

M0_INTERNAL void
m0_ut_backend_fini(struct m0_be_ut_backend *be, struct m0_be_ut_seg *seg)
{
	m0_be_ut_seg_fini(seg);
	m0_be_ut_backend_fini(be);
}

M0_INTERNAL void m0_ut_be_tx_begin(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred)
{
	int rc;

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ut_be_tx_begin2(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred,
				   m0_bcount_t payload_cred)
{
	int rc;

	m0_be_ut_tx_init(tx, ut_be);
	m0_be_tx_prep(tx, cred);
	m0_be_tx_payload_prep(tx, payload_cred);
	rc = m0_be_tx_open_sync(tx);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void m0_ut_be_tx_end(struct m0_be_tx *tx)
{
	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
