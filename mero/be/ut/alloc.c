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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 3-Jun-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/alloc.h"

#include "lib/memory.h"         /* m0_addr_is_aligned */
#include "lib/misc.h"           /* M0_SET_ARR0 */
#include "lib/thread.h"         /* m0_thread */
#include "lib/arith.h"          /* m0_rnd64 */
#include "lib/finject.h"        /* m0_fi_enable_once */

#include "ut/ut.h"              /* M0_UT_ASSERT */

#include "be/ut/helper.h"       /* m0_be_ut_backend */
#include "be/op.h"              /* m0_be_op */
#include "be/alloc_internal.h"  /* be_alloc_chunk */

enum {
	BE_UT_ALLOC_SEG_SIZE = 0x40000,
	BE_UT_ALLOC_SIZE     = 0x80,
	BE_UT_ALLOC_SHIFT    = 13,
	BE_UT_ALLOC_PTR_NR   = 0x20,
	BE_UT_ALLOC_NR       = 0x800,
	BE_UT_ALLOC_MT_NR    = 0x100,
	BE_UT_ALLOC_THR_NR   = 0x4,
};

struct be_ut_alloc_thread_state {
	struct m0_thread ats_thread;
	/** pointers array for this thread */
	void            *ats_ptr[BE_UT_ALLOC_PTR_NR];
	/** number of interations for this thread */
	int              ats_nr;
};

static struct m0_be_ut_backend         be_ut_alloc_backend;
static struct m0_be_ut_seg             be_ut_alloc_seg;
static struct be_ut_alloc_thread_state be_ut_ts[BE_UT_ALLOC_THR_NR];

M0_INTERNAL void m0_be_ut_alloc_init_fini(void)
{
	struct m0_be_ut_seg     ut_seg = {};
	struct m0_be_seg       *seg;
	struct m0_be_allocator *a;
	int                     rc;

	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_ALLOC_SEG_SIZE);
	seg = ut_seg.bus_seg;
	a = m0_be_seg_allocator(seg);
	rc = m0_be_allocator_init(a, seg);
	M0_UT_ASSERT(rc == 0);
	m0_be_allocator_fini(a);
	m0_be_ut_seg_fini(&ut_seg);
}

M0_INTERNAL void m0_be_ut_alloc_create_destroy(void)
{
	struct m0_be_ut_seg ut_seg;

	m0_be_ut_backend_init(&be_ut_alloc_backend);
	m0_be_ut_seg_init(&ut_seg, &be_ut_alloc_backend, BE_UT_ALLOC_SEG_SIZE);

	m0_be_ut_seg_allocator_init(&ut_seg, &be_ut_alloc_backend);

	m0_be_ut_seg_allocator_fini(&ut_seg, &be_ut_alloc_backend);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&be_ut_alloc_backend);
	M0_SET0(&be_ut_alloc_backend);
}

static void be_ut_alloc_ptr_handle(struct m0_be_allocator  *a,
				   void                   **p,
				   uint64_t                *seed)
{
	struct m0_be_ut_backend *ut_be = &be_ut_alloc_backend;
	m0_bcount_t              size;
	unsigned                 shift;

	size  = m0_rnd64(seed) % BE_UT_ALLOC_SIZE + 1;
	shift = m0_rnd64(seed) % BE_UT_ALLOC_SHIFT;

	if (*p == NULL) {
		M0_BE_UT_TRANSACT(ut_be, tx, cred,
			  (m0_be_allocator_credit(a, M0_BAO_ALLOC_ALIGNED,
						 size, shift, &cred),
			   m0_be_alloc_stats_credit(a, &cred)),
			  (M0_BE_OP_SYNC(op,
				 m0_be_alloc_aligned(a, tx, &op, p, size,
						     shift,
						     M0_BITS(M0_BAP_NORMAL))),
			   m0_be_alloc_stats_capture(a, tx)));
		M0_UT_ASSERT(*p != NULL);
		M0_UT_ASSERT(m0_addr_is_aligned(*p, shift));
	} else {
		M0_BE_UT_TRANSACT(ut_be, tx, cred,
			  (m0_be_allocator_credit(a, M0_BAO_FREE_ALIGNED,
						 size, shift, &cred),
			   m0_be_alloc_stats_credit(a, &cred)),
			  (M0_BE_OP_SYNC(op,
					m0_be_free_aligned(a, tx, &op, *p)),
			   m0_be_alloc_stats_capture(a, tx)));
		*p = NULL;
	}
}

static void be_ut_alloc_thread(int index)
{
	struct be_ut_alloc_thread_state *ts = &be_ut_ts[index];
	struct m0_be_allocator          *a;
	uint64_t                         seed = index;
	int                              i;
	int                              j;

	a = m0_be_seg_allocator(be_ut_alloc_seg.bus_seg);
	M0_UT_ASSERT(a != NULL);
	M0_SET_ARR0(ts->ats_ptr);
	for (j = 0; j < ts->ats_nr; ++j) {
		i = m0_rnd64(&seed) % ARRAY_SIZE(ts->ats_ptr);
		be_ut_alloc_ptr_handle(a, &ts->ats_ptr[i], &seed);
	}
	for (i = 0; i < BE_UT_ALLOC_PTR_NR; ++i) {
		if (ts->ats_ptr[i] != NULL) {
			be_ut_alloc_ptr_handle(a, &ts->ats_ptr[i], &seed);
		}
	}
	m0_be_ut_backend_thread_exit(&be_ut_alloc_backend);
}

static void be_ut_alloc_mt(int nr)
{
	struct m0_be_ut_backend *ut_be  = &be_ut_alloc_backend;
	struct m0_be_ut_seg     *ut_seg = &be_ut_alloc_seg;
	int                      rc;
	int                      i;

	M0_SET_ARR0(be_ut_ts);
	for (i = 0; i < nr; ++i) {
		be_ut_ts[i].ats_nr = nr == 1 ? BE_UT_ALLOC_NR :
					       BE_UT_ALLOC_MT_NR;
	}

	m0_be_ut_backend_init(ut_be);
	m0_be_ut_seg_init(ut_seg, ut_be, BE_UT_ALLOC_SEG_SIZE);
	m0_be_ut_seg_allocator_init(ut_seg, ut_be);
	for (i = 0; i < nr; ++i) {
		rc = M0_THREAD_INIT(&be_ut_ts[i].ats_thread, int, NULL,
				    &be_ut_alloc_thread, i,
				    "#%dbe_ut_alloc", i);
		M0_UT_ASSERT(rc == 0);
	}
	for (i = 0; i < nr; ++i) {
		m0_thread_join(&be_ut_ts[i].ats_thread);
		m0_thread_fini(&be_ut_ts[i].ats_thread);
	}
	m0_be_ut_seg_allocator_fini(ut_seg, ut_be);
	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);
	M0_SET0(ut_be);
}

M0_INTERNAL void m0_be_ut_alloc_multiple(void)
{
	be_ut_alloc_mt(1);
}

M0_INTERNAL void m0_be_ut_alloc_concurrent(void)
{
	be_ut_alloc_mt(BE_UT_ALLOC_THR_NR);
}

static void be_ut_alloc_credit_log(struct m0_be_allocator  *a,
				   enum m0_be_allocator_op  optype,
				   const char              *optype_str,
				   m0_bcount_t              size,
				   unsigned                 shift)
{
	struct m0_be_tx_credit cred = {};

	m0_be_allocator_credit(a, optype, size, shift, &cred);
	M0_LOG(M0_INFO,
	       "m0_be_allocator_credit(): "
	       "optype = %d (%s), size = %lu, shift = %d, credit = "BETXCR_F,
	       optype, optype_str, size, shift, BETXCR_P(&cred));
}

M0_INTERNAL void m0_be_ut_alloc_info(void)
{
	struct m0_be_allocator *a;
	struct m0_be_ut_seg     ut_seg;
	m0_bcount_t             size;
	unsigned                shift;

	m0_be_ut_backend_init(&be_ut_alloc_backend);
	m0_be_ut_seg_init(&ut_seg, &be_ut_alloc_backend, BE_UT_ALLOC_SEG_SIZE);
	m0_be_ut_seg_allocator_init(&ut_seg, &be_ut_alloc_backend);
	a = m0_be_seg_allocator(ut_seg.bus_seg);

	be_ut_alloc_credit_log(a, M0_BAO_CREATE,       "create", 0, 0);
	be_ut_alloc_credit_log(a, M0_BAO_DESTROY,      "destroy", 0, 0);
	be_ut_alloc_credit_log(a, M0_BAO_FREE,         "free", 0, 0);
	be_ut_alloc_credit_log(a, M0_BAO_FREE_ALIGNED, "free_aligned", 0, 0);

	for (size = 1; size <= 0x1000; size *= 4)
		be_ut_alloc_credit_log(a, M0_BAO_ALLOC, "alloc", size, 0);
	for (shift = 0; shift <= 12; shift += 1) {
		be_ut_alloc_credit_log(a, M0_BAO_ALLOC_ALIGNED, "alloc_aligned",
				       0x100, shift);
	}

	m0_be_ut_seg_allocator_fini(&ut_seg, &be_ut_alloc_backend);
	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&be_ut_alloc_backend);
	M0_SET0(&be_ut_alloc_backend);
}

/* segment and memory allocation sizes to test */
enum {
	BE_UT_OOM_SEG_START     = 0x1900,
	BE_UT_OOM_SEG_STEP      = 0x42,
	BE_UT_OOM_SEG_STEP_NR   = 0x4,
	BE_UT_OOM_ALLOC_START   = 0x1,
	BE_UT_OOM_ALLOC_STEP    = 0x1,
	BE_UT_OOM_ALLOC_STEP_NR = 0x4,
};

static void be_ut_alloc_oom_case(struct m0_be_allocator *a,
				 m0_bcount_t             alloc_size)
{
	unsigned   ptrs_nr_max = a->ba_seg->bs_size / alloc_size + 1;
	unsigned   ptrs_nr     = 0;
	unsigned   i;
	void     **ptrs;

	M0_ALLOC_ARR(ptrs, ptrs_nr_max);
	M0_UT_ASSERT(ptrs != NULL);

	do {
		M0_UT_ASSERT(ptrs_nr < ptrs_nr_max);
		M0_BE_UT_TRANSACT(&be_ut_alloc_backend, tx, cred,
		  m0_be_allocator_credit(a, M0_BAO_ALLOC, alloc_size, 0, &cred),
		  M0_BE_OP_SYNC(op, m0_be_alloc(a, tx, &op,
						&ptrs[ptrs_nr], alloc_size)));
	} while (ptrs[ptrs_nr++] != NULL);

	M0_UT_ASSERT(ptrs_nr > 1);

	for (i = 0; i < ptrs_nr; ++i) {
		M0_BE_UT_TRANSACT(&be_ut_alloc_backend, tx, cred,
			  m0_be_allocator_credit(a, M0_BAO_FREE, 0, 0, &cred),
			  M0_BE_OP_SYNC(op, m0_be_free(a, tx, &op, ptrs[i])));
	}

	m0_free(ptrs);
}

M0_INTERNAL void m0_be_ut_alloc_oom(void)
{
	struct m0_be_allocator *a;
	struct m0_be_ut_seg     ut_seg;
	int                     seg_size_start;
	int                     seg_step;
	int                     alloc_step;

	m0_be_ut_backend_init(&be_ut_alloc_backend);

	m0_be_ut_seg_init(&ut_seg, &be_ut_alloc_backend, 0x10000);
	seg_size_start = m0_be_seg_reserved(ut_seg.bus_seg) +
			 BE_UT_OOM_SEG_START;
	m0_be_ut_seg_fini(&ut_seg);

	for (seg_step = 0; seg_step < BE_UT_OOM_SEG_STEP_NR; ++seg_step) {
		m0_be_ut_seg_init(&ut_seg, &be_ut_alloc_backend,
				  m0_round_up(seg_size_start +
					      seg_step * BE_UT_OOM_SEG_STEP,
					      PAGE_SIZE));
		m0_be_ut_seg_allocator_init(&ut_seg, &be_ut_alloc_backend);
		a = m0_be_seg_allocator(ut_seg.bus_seg);

		for (alloc_step = 0; alloc_step < BE_UT_OOM_ALLOC_STEP_NR;
		     ++alloc_step) {
			be_ut_alloc_oom_case(a, BE_UT_OOM_ALLOC_START +
					     alloc_step * BE_UT_OOM_ALLOC_STEP);
		}
		m0_be_ut_seg_allocator_fini(&ut_seg, &be_ut_alloc_backend);
		m0_be_ut_seg_fini(&ut_seg);
	}
	m0_be_ut_backend_fini(&be_ut_alloc_backend);
	M0_SET0(&be_ut_alloc_backend);
}

M0_INTERNAL void m0_be_ut_alloc_spare(void)
{
	struct {
		bool     do_free;
		uint64_t zonemask;
		bool     should_fail;
		int      fi;
	} scenario[] = {
		{ false, M0_BITS(M0_BAP_NORMAL), false, 0 },
		{ false, M0_BITS(M0_BAP_NORMAL), true , 0 },
		{ true,  M0_BITS(M0_BAP_NORMAL), false, 0 },
		{ false, M0_BITS(M0_BAP_NORMAL), false, 0 },
		{ false, M0_BITS(M0_BAP_REPAIR), false, 0 },
		{ false, M0_BITS(M0_BAP_NORMAL), true,  0 },
		{ true,  M0_BITS(M0_BAP_NORMAL), false, 3 },
		{ true,  M0_BITS(M0_BAP_REPAIR), false, 4 }
	};
	struct m0_be_ut_backend        ut_be = {};
	struct m0_be_ut_seg            ut_seg;
	struct m0_be_allocator        *a;
	m0_bcount_t                    size;
	void                          *ptrs[ARRAY_SIZE(scenario)] = {};
	struct m0_be_allocator_stats   stats_before = {};
	struct m0_be_allocator_stats   stats_after = {};
	int                            i;

	M0_ENTRY();

	m0_be_ut_backend_init(&ut_be);

	/* Reserve 50% for M0_BAP_REPAIR zone. */
	m0_fi_enable_once("be_ut_seg_allocator_initfini", "repair_zone_50");
	m0_be_ut_seg_init(&ut_seg, &ut_be, BE_UT_ALLOC_SEG_SIZE);
	m0_be_ut_seg_allocator_init(&ut_seg, &ut_be);
	a = m0_be_seg_allocator(ut_seg.bus_seg);
	M0_UT_ASSERT(a != NULL);

	size = (BE_UT_ALLOC_SEG_SIZE - m0_be_seg_reserved(a->ba_seg)) / 3;

	for (i = 0 ; i < ARRAY_SIZE(scenario) ; ++i) {
		m0_be_alloc_stats(a, &stats_before);

		if (scenario[i].do_free) {
			M0_LOG(M0_INFO,
			       "ut_alloc_spare #%d do free ptrs[%d] %p",
			       i, scenario[i].fi, ptrs[scenario[i].fi]);

			M0_BE_UT_TRANSACT(&ut_be, tx, cred,
			  m0_be_allocator_credit(a, M0_BAO_FREE, 0, 0, &cred),
			  M0_BE_OP_SYNC(op, m0_be_free(a, tx, &op,
						       ptrs[scenario[i].fi])));
		} else {
			M0_BE_UT_TRANSACT(&ut_be, tx, cred,
			  (m0_be_allocator_credit(a, M0_BAO_ALLOC_ALIGNED,
						  size, BE_UT_ALLOC_SHIFT,
						  &cred),
					   m0_be_alloc_stats_credit(a, &cred)),
					  (M0_BE_OP_SYNC(op,
						 m0_be_alloc_aligned(a, tx, &op,
						     &ptrs[i], size,
						     BE_UT_ALLOC_SHIFT,
						     scenario[i].zonemask)),
					   m0_be_alloc_stats_capture(a, tx)));
			M0_UT_ASSERT(
				(ptrs[i] == NULL) == scenario[i].should_fail);
		}
		m0_be_alloc_stats(a, &stats_after);

		M0_UT_ASSERT(ergo(scenario[i].zonemask & M0_BITS(M0_BAP_REPAIR),
			(stats_before.bas_space_used == stats_after.bas_space_used) &&
			(stats_before.bas_space_free == stats_after.bas_space_free)));
	}

	m0_be_ut_backend_fini(&ut_be);
	M0_SET0(&ut_be);
	M0_LOG(M0_INFO, "m0_be_ut_alloc_spare OK");

	M0_LEAVE();
}


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
