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
 * Original creation date: 29-May-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/seg.h"             /* m0_be_seg */

#include "lib/types.h"          /* uint64_t */
#include "lib/arith.h"          /* m0_rnd64 */
#include "lib/thread.h"         /* M0_THREAD_INIT */
#include "lib/semaphore.h"      /* m0_semaphore */
#include "lib/misc.h"           /* m0_forall */
#include "lib/memory.h"         /* M0_ALLOC_PTR */

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "ut/stob.h"            /* m0_ut_stob_linux_get */
#include "be/ut/helper.h"       /* m0_be_ut_seg_helper */

enum {
	BE_UT_SEG_SIZE    = 0x20000,
	BE_UT_SEG_IO_ITER = 0x400,
	BE_UT_SEG_IO_OFFS = 0x10000,
	BE_UT_SEG_IO_SIZE = 0x10000,
};
M0_BASSERT(BE_UT_SEG_IO_OFFS + BE_UT_SEG_IO_SIZE <= BE_UT_SEG_SIZE);

M0_INTERNAL void m0_be_ut_seg_open_close(void)
{
	struct m0_be_ut_seg ut_seg;

	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_SEG_SIZE);
	m0_be_ut_seg_fini(&ut_seg);
}

static void be_ut_seg_rand_reg(struct m0_be_reg *reg,
			       void *seg_addr,
			       m0_bindex_t *offset,
			       m0_bcount_t *size,
			       uint64_t *seed)
{
	*size   = m0_rnd64(seed) % (BE_UT_SEG_IO_SIZE / 2) + 1;
	*offset = m0_rnd64(seed) % (BE_UT_SEG_IO_SIZE / 2 - 1);
	reg->br_addr = seg_addr + BE_UT_SEG_IO_OFFS + *offset;
	reg->br_size = *size;
}

M0_INTERNAL void m0_be_ut_seg_io(void)
{
	struct m0_be_ut_seg ut_seg;
	struct m0_be_seg   *seg;
	struct m0_be_reg    reg;
	struct m0_be_reg    reg_check;
	m0_bindex_t         offset;
	m0_bcount_t         size;
	static char         pre[BE_UT_SEG_IO_SIZE];
	static char         post[BE_UT_SEG_IO_SIZE];
	static char         rand[BE_UT_SEG_IO_SIZE];
	uint64_t            seed = 0;
	int                 rc;
	int                 i;
	int                 j;
	int                 cmp;

	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_SEG_SIZE);
	seg = ut_seg.bus_seg;
	reg_check = M0_BE_REG(seg, BE_UT_SEG_IO_SIZE,
			      seg->bs_addr + BE_UT_SEG_IO_OFFS);
	for (i = 0; i < BE_UT_SEG_IO_ITER; ++i) {
		be_ut_seg_rand_reg(&reg, seg->bs_addr, &offset, &size, &seed);
		reg.br_seg = seg;
		for (j = 0; j < reg.br_size; ++j)
			rand[j] = m0_rnd64(&seed) & 0xFF;

		/* read segment before write operation */
		rc = m0_be_seg__read(&reg_check, pre);
		M0_UT_ASSERT(rc == 0);
		/* write */
		rc = m0_be_seg__write(&reg, rand);
		M0_UT_ASSERT(rc == 0);
		/* and read to check if it was written */
		rc = m0_be_seg__read(&reg_check, post);
		M0_UT_ASSERT(rc == 0);
		/* reload segment to test I/O operations in open()/close() */
		m0_be_seg_close(seg);
		rc = m0_be_seg_open(seg);
		M0_UT_ASSERT(rc == 0);

		for (j = 0; j < size; ++j)
			pre[j + offset] = rand[j];

		M0_CASSERT(ARRAY_SIZE(pre) == ARRAY_SIZE(post));
		/*
		 * check if data was written to stob
		 * just after write operation
		 */
		cmp = memcmp(pre, post, ARRAY_SIZE(pre));
		M0_UT_ASSERT(cmp == 0);
		/* compare segment contents before and after reload */
		cmp = memcmp(post, reg_check.br_addr, reg_check.br_size);
		M0_UT_ASSERT(cmp == 0);
	}
	m0_be_ut_seg_fini(&ut_seg);
}

enum {
	BE_UT_SEG_THREAD_NR     = 0x10,
	BE_UT_SEG_PER_THREAD    = 0x10,
	BE_UT_SEG_MULTIPLE_SIZE = 0x10000,
};

static void be_ut_seg_thread_func(struct m0_semaphore *barrier)
{
	struct m0_be_ut_seg ut_seg;
	int                 i;

	m0_semaphore_down(barrier);
	for (i = 0; i < BE_UT_SEG_PER_THREAD; ++i) {
		m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_SEG_MULTIPLE_SIZE);
		m0_be_ut_seg_fini(&ut_seg);
	}
}

void m0_be_ut_seg_multiple(void)
{
	static struct m0_thread threads[BE_UT_SEG_THREAD_NR];
	struct m0_semaphore     barrier;
	bool                    rc_bool;

	m0_semaphore_init(&barrier, 0);
	rc_bool = m0_forall(i, ARRAY_SIZE(threads),
			    M0_THREAD_INIT(&threads[i], struct m0_semaphore *,
					   NULL, &be_ut_seg_thread_func,
					   &barrier, "#%dbe-seg-ut", i) == 0);
	M0_UT_ASSERT(rc_bool);
	m0_forall(i, ARRAY_SIZE(threads), m0_semaphore_up(&barrier), true);
	rc_bool = m0_forall(i, ARRAY_SIZE(threads),
			    m0_thread_join(&threads[i]) == 0);
	M0_UT_ASSERT(rc_bool);
	m0_forall(i, ARRAY_SIZE(threads), m0_thread_fini(&threads[i]), true);
	m0_semaphore_fini(&barrier);
}

/*
 * How to test really large segment:
 * 1. Select segment size you want to test:
 * 1.a Set BE_UT_SEG_LARGE_SIZE to the needed value.
 * 1.b Machine should have at least BE_UT_SEG_LARGE_SIZE virtual memory
 *     available (RAM + swap). Use dd to some file (or just use disk or
 *     partititon) + mkswap + swapon to add virtual memory if needed.
 * 2. Change "stob = " at the beginning of m0_be_ut_seg_large() to point to
 *    some large file that will be used as backing store. You can skip this
 *    step if the filesystem with "ut-sandbox" has enough free space.
 * 3. Run the test. It will take some time. Use iostat, vmstat, blktrace,
 *    iotop or dstat to check if I/O actually happens.
 */
enum {
	/*
	 * It should be > 4GiB, but devvm with small memory will have
	 * a problem with this UT. So it is set to a small value.
	 * It may be increased after paged implemented.
	 */
	BE_UT_SEG_LARGE_SIZE = 1ULL << 27,        /* 128 MiB */
	/* BE_UT_SEG_LARGE_SIZE = 1ULL << 34, */  /* 16 GiB */
	/* Each step-th byte will be overwritten in the test. */
	BE_UT_SEG_LARGE_STEP = 512,
	/* Block size for stob I/O in the test */
	BE_UT_SEG_LARGE_IO_BLOCK = 1ULL << 24,
};

static void be_ut_seg_large_mem(struct m0_be_seg *seg,
                                char              byte,
                                m0_bcount_t       size,
                                bool              check)
{
	m0_bindex_t i;
	char       *addr = seg->bs_addr;

	for (i = 0; i < size; i += BE_UT_SEG_LARGE_STEP) {
		if (i < seg->bs_reserved)
			continue;
		if (check)
			M0_UT_ASSERT(addr[i] == byte);
		else
			addr[i] = byte;
	}
}

static void be_ut_seg_large_block_io(struct m0_be_seg *seg,
                                     m0_bindex_t       offset,
                                     m0_bcount_t       block_size,
                                     char             *block,
                                     bool              read)
{
	struct m0_be_reg reg;

	reg = M0_BE_REG(seg, block_size, seg->bs_addr + offset);
	if (read)
		m0_be_seg__read(&reg, block);
	else
		m0_be_seg__write(&reg, block);
}

/*
 * Checks bytes of backing storage with step or writes bytes with some step
 * to the backing storage.
 *
 * @note It does RMW in "write" case. UT with large (~16GB) segment will take
 * a very long time (estimated ~1h on devvm with SSD).
 */
static void be_ut_seg_large_stob(struct m0_be_seg *seg,
                                 char              byte,
                                 m0_bcount_t       size,
                                 bool              check)
{
	m0_bindex_t  i;
	m0_bindex_t  block_begin = M0_BINDEX_MAX;
	m0_bindex_t  block_begin_new;
	m0_bcount_t  block_size = BE_UT_SEG_LARGE_IO_BLOCK;
	m0_bcount_t  size_left = 0;
	char        *block;

	block = m0_alloc(block_size);
	M0_UT_ASSERT(block != NULL);
	for (i = 0; i < size; i += BE_UT_SEG_LARGE_STEP) {
		if (i < seg->bs_reserved)
			continue;
		/* Do block I/O if block was changed after previous I/O. */
		block_begin_new = m0_round_down(i, block_size);
		size_left = min_check(block_size, size - block_begin_new);
		if (block_begin_new != block_begin) {
			if (!check && block_begin != M0_BINDEX_MAX) {
				/* W from RMW is here */
				be_ut_seg_large_block_io(seg, block_begin,
				                         block_size, block,
							 false);
			}
			be_ut_seg_large_block_io(seg, block_begin_new,
			                         size_left, block, true);
			block_begin = block_begin_new;
		}
		if (check) {
			M0_UT_ASSERT(block[i - block_begin] == byte);
		} else {
			block[i - block_begin] = byte;
		}
	}
	/* last W from RMW is here */
	if (size_left > 0) {
		be_ut_seg_large_block_io(seg, block_begin, size_left,
		                         block, false);
	}
	m0_free(block);
}

void m0_be_ut_seg_large(void)
{
	struct m0_be_seg *seg;
	struct m0_stob   *stob;
	m0_bcount_t       size;
	void             *addr;
	int               rc;

	M0_ALLOC_PTR(seg);
	M0_UT_ASSERT(seg != NULL);
	stob = m0_ut_stob_linux_get();
	/* stob = m0_ut_stob_linux_create("/dev/sdc1"); */
	M0_UT_ASSERT(stob != NULL);

	size = BE_UT_SEG_LARGE_SIZE;
	addr = m0_be_ut_seg_allocate_addr(size);

	m0_be_seg_init(seg, stob, NULL, M0_BE_SEG_FAKE_ID);
	rc = m0_be_seg_create(seg, size, addr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_seg_open(seg);
	M0_UT_ASSERT(rc == 0);

	M0_LOG(M0_DEBUG, "check if in-memory segment data is initially zeroed");
	be_ut_seg_large_mem(seg, 0, size, true);
	M0_LOG(M0_DEBUG, "check if backing store is initially zeroed");
	be_ut_seg_large_stob(seg, 0, size, true);
	M0_LOG(M0_DEBUG, "write 1 to in-memory segment data");
	be_ut_seg_large_mem(seg, 1, size, false);
	M0_LOG(M0_DEBUG, "check 1 in in-memory segment data");
	be_ut_seg_large_mem(seg, 1, size, true);
	M0_LOG(M0_DEBUG, "check if backing store is still zeroed");
	be_ut_seg_large_stob(seg, 0, size, true);
	M0_LOG(M0_DEBUG, "write 2 to the backing store");
	be_ut_seg_large_stob(seg, 2, size, false);
	M0_LOG(M0_DEBUG, "check 1 in in-memory segment data");
	be_ut_seg_large_mem(seg, 1, size, true);
	M0_LOG(M0_DEBUG, "check 2 in the backing store");
	be_ut_seg_large_stob(seg, 2, size, true);
	M0_LOG(M0_DEBUG, "write 3 to in-memory segment data");
	be_ut_seg_large_mem(seg, 3, size, false);
	M0_LOG(M0_DEBUG, "check 3 in in-memory segment data");
	be_ut_seg_large_mem(seg, 3, size, true);
	M0_LOG(M0_DEBUG, "check 2 in the backing store");
	be_ut_seg_large_stob(seg, 2, size, true);

	m0_be_seg_close(seg);
	rc = m0_be_seg_destroy(seg);
	M0_UT_ASSERT(rc == 0);
	m0_be_seg_fini(seg);

	m0_ut_stob_put(stob, true);
	m0_free(seg);
}

void m0_be_ut_seg_large_multiple(void)
{
	struct m0_be_seg_geom geom[] = {
		{
			.sg_size = 0ULL,
			.sg_addr = NULL,
			.sg_offset = 0ULL,
			.sg_id = 1,
		},
		{
			.sg_size = 0ULL,
			.sg_addr = NULL,
			.sg_offset = 0ULL,
			.sg_id = 1,
		},
		{
			.sg_size = 0ULL,
			.sg_addr = NULL,
			.sg_offset = 0ULL,
			.sg_id = 1,
		},
		M0_BE_SEG_GEOM0,
	};
	int i;
	m0_bcount_t       size;
	m0_bcount_t       offset = 0;
	void             *addr;
	int               rc;
	struct m0_be_seg *seg[ARRAY_SIZE(geom) - 1];
	struct m0_stob   *stob;

	size = BE_UT_SEG_LARGE_SIZE;

	for (i = 0; !m0_be_seg_geom_eq(&geom[i], &M0_BE_SEG_GEOM0); ++i) {
		addr = m0_be_ut_seg_allocate_addr(size);
		geom[i] = (struct m0_be_seg_geom) {
			.sg_size = size,
			.sg_addr = addr,
			.sg_offset = offset,
			.sg_id = i+0x111,
		},
		offset += size;
	}

	stob = m0_ut_stob_linux_get();
	M0_UT_ASSERT(stob != NULL);
	rc = m0_be_seg_create_multiple(stob, geom);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < ARRAY_SIZE(geom) - 1; ++i) {
		M0_ALLOC_PTR(seg[i]);
		M0_UT_ASSERT(seg[i] != NULL);
		m0_be_seg_init(seg[i], stob, NULL, geom[i].sg_id);
		M0_UT_ASSERT(rc == 0);
		rc = m0_be_seg_open(seg[i]);
		M0_UT_ASSERT(rc == 0);
	}

	for (i = 0; i < ARRAY_SIZE(geom) - 1; ++i) {
		M0_LOG(M0_DEBUG, "check if in-memory segment data is initially"
		       " zeroed");
		be_ut_seg_large_mem(seg[i], 0, size, true);
		M0_LOG(M0_DEBUG, "check if backing store is initially zeroed");
		be_ut_seg_large_stob(seg[i], 0, size, true);
		M0_LOG(M0_DEBUG, "write 1 to in-memory segment data");
		be_ut_seg_large_mem(seg[i], 1, size, false);
		M0_LOG(M0_DEBUG, "check 1 in in-memory segment data");
		be_ut_seg_large_mem(seg[i], 1, size, true);
		M0_LOG(M0_DEBUG, "check if backing store is still zeroed");
		be_ut_seg_large_stob(seg[i], 0, size, true);
		M0_LOG(M0_DEBUG, "write 2 to the backing store");
		be_ut_seg_large_stob(seg[i], 2, size, false);
		M0_LOG(M0_DEBUG, "check 1 in in-memory segment data");
		be_ut_seg_large_mem(seg[i], 1, size, true);
		M0_LOG(M0_DEBUG, "check 2 in the backing store");
		be_ut_seg_large_stob(seg[i], 2, size, true);
		M0_LOG(M0_DEBUG, "write 3 to in-memory segment data");
		be_ut_seg_large_mem(seg[i], 3, size, false);
		M0_LOG(M0_DEBUG, "check 3 in in-memory segment data");
		be_ut_seg_large_mem(seg[i], 3, size, true);
		M0_LOG(M0_DEBUG, "check 2 in the backing store");
		be_ut_seg_large_stob(seg[i], 2, size, true);
	}

	for (i = 0; i < ARRAY_SIZE(geom) - 1; ++i) {
		m0_be_seg_close(seg[i]);
		rc = m0_be_seg_destroy(seg[i]);
		M0_UT_ASSERT(rc == 0);
		m0_be_seg_fini(seg[i]);
		m0_free(seg[i]);
	}

	m0_ut_stob_put(stob, true);
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
