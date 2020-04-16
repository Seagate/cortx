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
 * Original author: Huang Hua <hua_huang@xyratex.com>
 * Original creation date: 09/02/2010
 */

#include <stdlib.h>       /* srand, rand */
#include <errno.h>
#include <sys/time.h>
#include <err.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BALLOC
#include "lib/trace.h"
#include "lib/arith.h"    /* M0_3WAY, m0_uint128 */
#include "lib/misc.h"     /* M0_SET0 */
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/thread.h"
#include "lib/getopts.h"
#include "dtm/dtm.h"      /* m0_dtx */
#include "mero/magic.h"
#include "ut/ut.h"
#include "ut/be.h"
#include "balloc/balloc.h"
#include "be/ut/helper.h"
#include "stob/ad.h"      /* m0_stob_ad_spares_calc */

#define BALLOC_DBNAME "./__balloc_db"

#define GROUP_SIZE (BALLOC_DEF_CONTAINER_SIZE / (BALLOC_DEF_BLOCKS_PER_GROUP * \
						 (1 << BALLOC_DEF_BLOCK_SHIFT)))

#define BALLOC_DEBUG

static const int    MAX     = 10;
static m0_bcount_t  prev_free_blocks;
m0_bcount_t	   *prev_group_info_free_blocks;

enum balloc_invariant_enum {
	INVAR_ALLOC,
	INVAR_FREE,
};

bool balloc_ut_invariant(struct m0_balloc *mero_balloc,
			 struct m0_ext alloc_ext,
			 int balloc_invariant_flag)
{
	m0_bcount_t len = m0_ext_length(&alloc_ext);
	m0_bcount_t group;

	group = alloc_ext.e_start >> mero_balloc->cb_sb.bsb_gsbits;

	if (mero_balloc->cb_sb.bsb_magic != M0_BALLOC_SB_MAGIC)
		return false;

	switch (balloc_invariant_flag) {
	    case INVAR_ALLOC:
		 prev_free_blocks		    -= len;
		 prev_group_info_free_blocks[group] -= len;
		 break;
	    case INVAR_FREE:
		 prev_free_blocks		    += len;
		 prev_group_info_free_blocks[group] += len;
		 break;
	    default:
		 return false;
	}

	return mero_balloc->cb_group_info[group].bgi_normal.bzp_freeblocks ==
		prev_group_info_free_blocks[group] &&
		mero_balloc->cb_sb.bsb_freeblocks ==
		prev_free_blocks;
}

int test_balloc_ut_ops(struct m0_be_ut_backend *ut_be, struct m0_be_seg *seg)
{
	struct m0_sm_group     *grp;
	struct m0_balloc       *mero_balloc;
	struct m0_dtx           dtx = {};
	struct m0_be_tx        *tx  = &dtx.tx_betx;
	struct m0_be_tx_credit  cred;
	struct m0_ext           ext[MAX];
	struct m0_ext           tmp   = {};
	m0_bcount_t             count = 539;
	m0_bcount_t             spare_size;
	int                     i     = 0;
	int                     result;
	time_t                  now;

	time(&now);
	srand(now);

	grp = m0_be_ut_backend_sm_group_lookup(ut_be);
	result = m0_balloc_create(0, seg, grp, &mero_balloc);
	M0_UT_ASSERT(result == 0);

	result = mero_balloc->cb_ballroom.ab_ops->bo_init
		(&mero_balloc->cb_ballroom, seg, BALLOC_DEF_BLOCK_SHIFT,
		 BALLOC_DEF_CONTAINER_SIZE, BALLOC_DEF_BLOCKS_PER_GROUP,
		 m0_stob_ad_spares_calc(BALLOC_DEF_BLOCKS_PER_GROUP));

	if (result == 0) {
		prev_free_blocks = mero_balloc->cb_sb.bsb_freeblocks;
		M0_ALLOC_ARR(prev_group_info_free_blocks, GROUP_SIZE);
		for (i = 0; i < GROUP_SIZE; ++i) {
			prev_group_info_free_blocks[i] =
			 mero_balloc->cb_group_info[i].bgi_normal.bzp_freeblocks;
		}

		for (i = 0; i < MAX; ++i) {
			count = rand() % 1500 + 1;

			cred = M0_BE_TX_CREDIT(0, 0);
			mero_balloc->cb_ballroom.ab_ops->bo_alloc_credit(
				    &mero_balloc->cb_ballroom, 1, &cred);
			m0_ut_be_tx_begin(tx, ut_be, &cred);

			/* pass last result as goal. comment out this to turn
			   off goal */
			//tmp.e_start = tmp.e_end;
			result = mero_balloc->cb_ballroom.ab_ops->bo_alloc(
				    &mero_balloc->cb_ballroom, &dtx, count,
				    &tmp, M0_BALLOC_NORMAL_ZONE);
			M0_UT_ASSERT(result == 0);
			if (result < 0) {
				M0_LOG(M0_ERROR, "Error in allocation");
				return result;
			}

			ext[i] = tmp;

			/* The result extent length should be less than
			 * or equal to the requested length. */
			M0_UT_ASSERT(m0_ext_length(&ext[i]) <= count);
			M0_UT_ASSERT(balloc_ut_invariant(mero_balloc, ext[i],
							 INVAR_ALLOC));
			M0_LOG(M0_INFO, "%3d:rc=%d: req=%5d, got=%5d: "
			       "[%08llx,%08llx)=[%8llu,%8llu)",
			       i, result, (int)count,
			       (int)m0_ext_length(&ext[i]),
			       (unsigned long long)ext[i].e_start,
			       (unsigned long long)ext[i].e_end,
			       (unsigned long long)ext[i].e_start,
			       (unsigned long long)ext[i].e_end);
			m0_ut_be_tx_end(tx);
		}

		spare_size =
		  m0_stob_ad_spares_calc(mero_balloc->cb_sb.bsb_groupsize);
		for (i = 0;
		     i < mero_balloc->cb_sb.bsb_groupcount && result == 0;
		     ++i) {
			struct m0_balloc_group_info *grp =
				m0_balloc_gn2info(mero_balloc, i);

			if (grp) {
				m0_balloc_lock_group(grp);
				result = m0_balloc_load_extents(mero_balloc,
								grp);
				if (result == 0)
					m0_balloc_debug_dump_group_extent(
						    "balloc ut", grp);
				m0_balloc_release_extents(grp);
				m0_balloc_unlock_group(grp);
			}
		}

		/* randomize the array */
		for (i = 0; i < MAX; ++i) {
			int a;
			int b;
			a = rand() % MAX;
			b = rand() % MAX;
			M0_SWAP(ext[a], ext[b]);
		}

		for (i = 0; i < MAX && result == 0; ++i) {
			cred = M0_BE_TX_CREDIT(0, 0);
			mero_balloc->cb_ballroom.ab_ops->bo_free_credit(
				    &mero_balloc->cb_ballroom, 1, &cred);
			m0_ut_be_tx_begin(tx, ut_be, &cred);

			result = mero_balloc->cb_ballroom.ab_ops->bo_free(
					    &mero_balloc->cb_ballroom, &dtx,
					    &ext[i]);
			M0_UT_ASSERT(result == 0);
			if (result < 0) {
				M0_LOG(M0_ERROR, "Error during free for size %5d",
					(int)m0_ext_length(&ext[i]));
				return result;
			}

			M0_UT_ASSERT(balloc_ut_invariant(mero_balloc, ext[i],
							 INVAR_FREE));
			M0_LOG(M0_INFO, "%3d:rc=%d: freed=         %5d: "
			       "[%08llx,%08llx)=[%8llu,%8llu)",
			       i, result, (int)m0_ext_length(&ext[i]),
			       (unsigned long long)ext[i].e_start,
			       (unsigned long long)ext[i].e_end,
			       (unsigned long long)ext[i].e_start,
			       (unsigned long long)ext[i].e_end);
			m0_ut_be_tx_end(tx);
		}

		M0_UT_ASSERT(mero_balloc->cb_sb.bsb_freeblocks ==
					prev_free_blocks);
		if (mero_balloc->cb_sb.bsb_freeblocks != prev_free_blocks) {
			M0_LOG(M0_ERROR, "Size mismatch during block reclaim");
			result = -EINVAL;
		}

		for (i = 0;
		     i < mero_balloc->cb_sb.bsb_groupcount && result == 0;
		     ++i) {
			struct m0_balloc_group_info *grp = m0_balloc_gn2info
				(mero_balloc, i);

			if (grp) {
				m0_balloc_lock_group(grp);
				result = m0_balloc_load_extents(mero_balloc,
								grp);
				if (result == 0)
					m0_balloc_debug_dump_group_extent(
						    "balloc ut", grp);
				M0_UT_ASSERT(grp->bgi_normal.bzp_freeblocks ==
					     mero_balloc->cb_sb.bsb_groupsize -
					     spare_size);
				m0_balloc_release_extents(grp);
				m0_balloc_unlock_group(grp);
			}
		}

		mero_balloc->cb_ballroom.ab_ops->bo_fini(
			    &mero_balloc->cb_ballroom);
	}

	m0_free(prev_group_info_free_blocks);

	M0_LOG(M0_INFO, "done. status = %d", result);
	return result;
}

void test_balloc()
{
	struct m0_be_ut_backend	 ut_be;
	struct m0_be_ut_seg	 ut_seg;
	int			 rc;

	M0_SET0(&ut_be);
	/* Init BE */
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1ULL << 24);
	rc = test_balloc_ut_ops(&ut_be, ut_seg.bus_seg);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

struct m0_ut_suite balloc_ut = {
        .ts_name  = "balloc-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
        .ts_tests = {
                { "balloc", test_balloc},
		{ NULL, NULL }
        }
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
