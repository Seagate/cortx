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

#pragma once

#ifndef __MERO_UT_BE_H__
#define __MERO_UT_BE_H__

#include "lib/types.h"  /* m0_bcount_t */

/* import */
struct m0_be_tx_credit;
struct m0_be_tx;
struct m0_be_seg;
struct m0_be_ut_backend;
struct m0_be_ut_seg;
struct m0_reqh;
struct m0_sm_group;

/**
 * @addtogroup ut
 *
 * BE helper functions that unit tests of other (non-BE) Mero subsystems
 * are allowed to use.
 *
 * The API declared in be/ut/helper.h is supposed to be used by BE unit
 * tests only. The UTs of other subsystems should not #include that file
 * (though they do, hee hee hee).
 *
 * @{
 */

M0_INTERNAL void m0_ut_backend_init(struct m0_be_ut_backend *be,
				    struct m0_be_ut_seg *seg);

M0_INTERNAL void m0_ut_backend_fini(struct m0_be_ut_backend *be,
				    struct m0_be_ut_seg *seg);

/** Initialises, prepares, and opens the transaction. */
M0_INTERNAL void m0_ut_be_tx_begin(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred);

M0_INTERNAL void m0_ut_be_tx_begin2(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred,
				   m0_bcount_t payload_cred);

/** Closes the transaction and waits for its completion. */
M0_INTERNAL void m0_ut_be_tx_end(struct m0_be_tx *tx);

/** @} end of ut group */
#endif /* __MERO_UT_BE_H__ */

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
