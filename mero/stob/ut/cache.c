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
 * Original creation date: 3-Mar-2014
 */

#include "stob/cache.h"		/* m0_stob_cache */

#include "lib/memory.h"		/* M0_ALLOC_PTR */
#include "lib/thread.h"		/* M0_THREAD_INIT */
#include "lib/arith.h"		/* m0_rnd64 */

#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "ut/threads.h"		/* M0_UT_THREADS_DEFINE */

#include "stob/stob.h"		/* m0_stob */
#include "stob/stob_internal.h"	/* m0_stob__key_set */

enum {
	STOB_UT_CACHE_THREAD_NR = 0x20,
	STOB_UT_CACHE_ITER_NR	= 0x2000,
	STOB_UT_CACHE_STOB_NR	= 0x40,
	STOB_UT_CACHE_COLD_SIZE	= 0x10,
};

struct stob_ut_cache_ctx {
	int		 suc_index;
	size_t		 suc_idle_size;
	size_t		 suc_iter_nr;
};

static struct m0_stob stob_ut_cache_stobs[STOB_UT_CACHE_STOB_NR];
static struct m0_stob_cache stob_ut_cache;

static void stob_ut_cache_thread(struct stob_ut_cache_ctx *ctx)
{
	struct m0_stob_cache *cache = &stob_ut_cache;
	struct m0_stob	     *stob;
	struct m0_stob	     *found;
	struct m0_stob	     *found2;
	uint64_t	      state = ctx->suc_index;
	int		      i;
	long		      j;

	for (i = 0; i < STOB_UT_CACHE_ITER_NR; ++i) {
		/* select random stob */
		j = m0_rnd64(&state) % ARRAY_SIZE(stob_ut_cache_stobs);
		stob = &stob_ut_cache_stobs[j];
		/* add to cache if it hasn't been added yet */
		/* delete if it has already been added */
		m0_stob_cache_lock(cache);
		found = m0_stob_cache_lookup(cache, m0_stob_fid_get(stob));
		if (found == NULL) {
			m0_stob_cache_add(cache, stob);
		} else {
			m0_stob_cache_idle(cache, stob);
		}
		found2 = m0_stob_cache_lookup(cache, m0_stob_fid_get(stob));
		/*
		 * If stob was in the cache before and idle_size > 0
		 * then second lookup will bring stob back to busy cache.
		 * It is not what we need, so push it back to the idle cache
		 * in this case.
		 */
		if (found != NULL && found2 != NULL)
			m0_stob_cache_idle(cache, stob);
		m0_stob_cache_unlock(cache);
		M0_UT_ASSERT(ergo(found == NULL, found2 != NULL));
		M0_UT_ASSERT(M0_IN(stob, (found, found2)));
	}
}

static void stob_ut_cache_evict_cb(struct m0_stob_cache *cache,
				   struct m0_stob *stob)
{
	/* XXX check that it is called */
}

M0_UT_THREADS_DEFINE(stob_cache, stob_ut_cache_thread);

static void stob_ut_cache_test(size_t thread_nr,
			       size_t iter_nr,
			       size_t idle_size)
{
	struct stob_ut_cache_ctx *ctxs;
	struct m0_stob		 *stob;
	const struct m0_fid      *stob_fid;
	size_t			  i;
	int			  rc;
	uint64_t		  state = 0;

	M0_ALLOC_ARR(ctxs, thread_nr);
	M0_UT_ASSERT(ctxs != NULL);

	M0_SET0(&stob_ut_cache);
	M0_SET_ARR0(stob_ut_cache_stobs);

	rc = m0_stob_cache_init(&stob_ut_cache, idle_size,
				&stob_ut_cache_evict_cb);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < ARRAY_SIZE(stob_ut_cache_stobs); ++i)
		stob_ut_cache_stobs[i].so_id.si_fid.f_key = m0_rnd64(&state);

	for (i = 0; i < thread_nr; ++i) {
		ctxs[i] = (struct stob_ut_cache_ctx){
			.suc_index     = i,
			.suc_idle_size = idle_size,
			.suc_iter_nr   = iter_nr,
		};
	}
	M0_UT_THREADS_START(stob_cache, thread_nr, ctxs);
	M0_UT_THREADS_STOP(stob_cache);

	/* clear stob cache */
	m0_stob_cache_lock(&stob_ut_cache);
	for (i = 0; i < ARRAY_SIZE(stob_ut_cache_stobs); ++i) {
		stob_fid = m0_stob_fid_get(&stob_ut_cache_stobs[i]);
		stob = m0_stob_cache_lookup(&stob_ut_cache, stob_fid);
		if (stob != NULL)
			m0_stob_cache_idle(&stob_ut_cache, stob);
	}
	m0_stob_cache_unlock(&stob_ut_cache);

	m0_stob_cache_fini(&stob_ut_cache);
	m0_free(ctxs);
}

void m0_stob_ut_cache(void)
{
	stob_ut_cache_test(STOB_UT_CACHE_THREAD_NR, STOB_UT_CACHE_ITER_NR,
			   STOB_UT_CACHE_COLD_SIZE);
}

void m0_stob_ut_cache_idle_size0(void)
{
	stob_ut_cache_test(STOB_UT_CACHE_THREAD_NR, STOB_UT_CACHE_ITER_NR, 0);
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
