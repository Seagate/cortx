/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 14-Jan-2014
 */

#include "be/fl.h"

#include "lib/types.h"          /* uint64_t */
#include "lib/arith.h"          /* m0_rnd64 */

#include "ut/ut.h"              /* M0_UT_ASSERT */

#include "be/alloc_internal.h"  /* be_alloc_chunk */
#include "be/ut/helper.h"       /* m0_be_ut_backend */

enum {
	BE_UT_FL_CHUNK_NR  = 0x100,
	BE_UT_FL_ITER      = 0x800,
	BE_UT_FL_SEG_SIZE  = 0x20000,
	BE_UT_FL_OP_PER_TX = 0x10,
	BE_UT_FL_SIZE_MAX  = M0_BE_FL_STEP * (M0_BE_FL_NR + 0x10),
};

static struct m0_be_ut_backend be_ut_fl_backend;

static uint64_t be_ut_fl_rand(uint64_t max, uint64_t *seed)
{
	return m0_rnd64(seed) % max;
}

static m0_bcount_t be_ut_fl_rand_size(uint64_t *seed)
{
	return be_ut_fl_rand(BE_UT_FL_SIZE_MAX, seed);
}

void m0_be_ut_fl(void)
{
	static struct m0_be_ut_backend *ut_be = &be_ut_fl_backend;
	static struct m0_be_ut_seg      ut_seg;
	static struct m0_be_tx          tx;
	struct be_alloc_chunk          *chunks;
	struct m0_be_tx_credit          cred;
	struct m0_be_fl                *fl;
	struct m0_be_seg               *seg;
	uint64_t                        seed = 0;
	void                           *addr;
	int                            *chunks_used;
	int                             i;
	int                             rc;
	uint64_t                        index;

	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(&ut_seg, ut_be, BE_UT_FL_SEG_SIZE);
	seg = ut_seg.bus_seg;

	addr    = seg->bs_addr + m0_be_seg_reserved(seg);
	fl      = (struct m0_be_fl *)addr;
	addr   += sizeof *fl;
	chunks  = (struct be_alloc_chunk *)addr;

	M0_BE_UT_TRANSACT(ut_be, tx, cred,
			  m0_be_fl_credit(NULL, M0_BFL_CREATE, &cred),
			  m0_be_fl_create(fl, tx, seg));

	M0_ALLOC_ARR(chunks_used, BE_UT_FL_CHUNK_NR);
	M0_ASSERT(chunks_used != NULL);

	for (i = 0; i < BE_UT_FL_ITER; ++i) {
		if ((i % BE_UT_FL_OP_PER_TX) == 0) {
			M0_SET0(&tx);
			m0_be_ut_tx_init(&tx, ut_be);

			cred = M0_BE_TX_CREDIT(0, 0);
			/* XXX don't use the largest possible credit */
			m0_be_fl_credit(fl, M0_BFL_ADD, &cred);
			m0_be_tx_credit_mul(&cred, BE_UT_FL_OP_PER_TX);
			m0_be_tx_prep(&tx, &cred);

			rc = m0_be_tx_open_sync(&tx);
			M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
		}
		index = be_ut_fl_rand(BE_UT_FL_CHUNK_NR, &seed);
		if (chunks_used[index]) {
			m0_be_fl_del(fl, &tx, &chunks[index]);
			chunks_used[index] = false;
		} else {
			chunks[index].bac_size = be_ut_fl_rand_size(&seed);
			m0_be_fl_add(fl, &tx, &chunks[index]);
			chunks_used[index] = true;
		}
		if (i + 1 == BE_UT_FL_ITER ||
		    ((i + 1) % BE_UT_FL_OP_PER_TX) == 0) {
			m0_be_tx_close_sync(&tx);
			m0_be_tx_fini(&tx);
		}
	}

	for (i = 0; i < BE_UT_FL_CHUNK_NR; ++i) {
		if (chunks_used[i]) {
			M0_BE_UT_TRANSACT(ut_be, tx, cred,
				  m0_be_fl_credit(NULL, M0_BFL_DEL, &cred),
				  m0_be_fl_del(fl, tx, &chunks[i]));
		}
	}

	m0_free(chunks_used);

	M0_BE_UT_TRANSACT(ut_be, tx, cred,
			  m0_be_fl_credit(NULL, M0_BFL_DESTROY, &cred),
			  m0_be_fl_destroy(fl, tx));

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(ut_be);
}

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
