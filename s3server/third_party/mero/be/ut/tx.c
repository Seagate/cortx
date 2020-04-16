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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 12-Jun-2013
 */


/**
 * @addtogroup be
 *
 * * Additional tests
 * - too large tx payload
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/types.h"          /* m0_uint128_eq */
#include "lib/arith.h"          /* m0_rnd64 */
#include "lib/misc.h"           /* M0_BITS */
#include "lib/memory.h"         /* M0_ALLOC_PTR */

#include "ut/ut.h"

#include "be/ut/helper.h"       /* m0_be_ut_backend */

void m0_be_ut_tx_usecase_success(void)
{
	struct m0_be_ut_backend ut_be;
	struct m0_be_ut_seg     ut_seg;
	struct m0_be_seg       *seg;
	struct m0_be_tx_credit  credit = M0_BE_TX_CREDIT_TYPE(uint64_t);
	struct m0_be_tx         tx;
	uint64_t               *data;
	int                     rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, 1 << 20);
	seg = ut_seg.bus_seg;

	m0_be_ut_tx_init(&tx, &ut_be);

	m0_be_tx_prep(&tx, &credit);

	/* m0_be_tx_open_sync() can be used in UT */
	m0_be_tx_open(&tx);
	rc = m0_be_tx_timedwait(&tx, M0_BITS(M0_BTS_ACTIVE, M0_BTS_FAILED),
				M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);

	data = (uint64_t *) (seg->bs_addr + seg->bs_reserved);
	*data = 0x101;
	m0_be_tx_capture(&tx, &M0_BE_REG_PTR(seg, data));

	/* m0_be_tx_close_sync() can be used in UT */
	m0_be_tx_close(&tx);
	rc = m0_be_tx_timedwait(&tx, M0_BITS(M0_BTS_DONE), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	m0_be_tx_fini(&tx);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_tx_usecase_failure(void)
{
	struct m0_be_ut_backend ut_be;
	struct m0_be_tx         tx;
	int                     rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);

	m0_be_ut_tx_init(&tx, &ut_be);

	m0_be_tx_prep(&tx, &m0_be_tx_credit_invalid);

	m0_be_tx_open(&tx);
	rc = m0_be_tx_timedwait(&tx, M0_BITS(M0_BTS_ACTIVE, M0_BTS_FAILED),
				M0_TIME_NEVER);
	M0_UT_ASSERT(rc != 0);

	m0_be_tx_fini(&tx);

	m0_be_ut_backend_fini(&ut_be);
}

static void be_ut_tx_test(size_t nr);

static void be_ut_tx_alloc_init(void **alloc, struct m0_be_seg *seg)
{
	*alloc = seg->bs_addr + seg->bs_reserved;
}

static void be_ut_tx_alloc_fini(void **alloc)
{
	*alloc = NULL;
}

static void *be_ut_tx_alloc(void **alloc, m0_bcount_t size)
{
	void *ptr = *alloc;

	*alloc += size;
	return ptr;
}

void m0_be_ut_tx_states(void)
{
	struct m0_be_ut_backend ut_be;
	struct m0_be_tx_credit  credit = M0_BE_TX_CREDIT_TYPE(uint64_t);
	struct m0_be_ut_seg     ut_seg;
	struct m0_be_seg       *seg;
	struct m0_be_tx         tx;
	uint64_t               *data;
	int                     rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, 1 << 20);
	seg = ut_seg.bus_seg;

	/* test success path */
	m0_be_ut_tx_init(&tx, &ut_be);
	M0_UT_ASSERT(tx.t_sm.sm_rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_PREPARE);

	m0_be_tx_prep(&tx, &credit);
	M0_UT_ASSERT(tx.t_sm.sm_rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_PREPARE);

	/* to check M0_BTS_PLACED state */
	m0_be_tx_get(&tx);

	rc = m0_be_tx_open_sync(&tx);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_ACTIVE);

	data = (uint64_t *) (seg->bs_addr + seg->bs_reserved);
	*data = 0x101;
	m0_be_tx_capture(&tx, &M0_BE_REG_PTR(seg, data));

	m0_be_tx_close(&tx);
	rc = m0_be_tx_timedwait(&tx, M0_BITS(M0_BTS_PLACED), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_PLACED);

	m0_be_tx_put(&tx);
	rc = m0_be_tx_timedwait(&tx, M0_BITS(M0_BTS_DONE), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_DONE);

	m0_be_tx_fini(&tx);

	/* test failure path */
	m0_be_ut_tx_init(&tx, &ut_be);
	M0_UT_ASSERT(tx.t_sm.sm_rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_PREPARE);

	m0_be_tx_prep(&tx, &m0_be_tx_credit_invalid);
	M0_UT_ASSERT(tx.t_sm.sm_rc == 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_PREPARE);

	m0_be_tx_get(&tx);

	rc = m0_be_tx_open_sync(&tx);
	M0_UT_ASSERT(rc != 0);
	M0_UT_ASSERT(m0_be_tx_state(&tx) == M0_BTS_FAILED);

	m0_be_tx_put(&tx);

	m0_be_tx_fini(&tx);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_tx_empty(void)
{
	struct m0_be_ut_backend ut_be;
	struct m0_be_tx         tx;
	int                     rc;
	int                     i;
	struct m0_be_tx_credit  credit[] = {
		M0_BE_TX_CREDIT(0, 0),
		M0_BE_TX_CREDIT(1, sizeof(void *)),
	};

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);

	for (i = 0; i < ARRAY_SIZE(credit); ++i) {
		m0_be_ut_tx_init(&tx, &ut_be);

		m0_be_tx_prep(&tx, &credit[i]);

		rc = m0_be_tx_open_sync(&tx);
		M0_UT_ASSERT(rc == 0);

		m0_be_tx_close_sync(&tx);
		m0_be_tx_fini(&tx);
	}

	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_tx_single(void)
{
	be_ut_tx_test(1);
}

void m0_be_ut_tx_several(void)
{
	be_ut_tx_test(2);
}

struct be_ut_complex {
	uint32_t real;
	uint32_t imag;
};

struct be_ut_tx_x {
	struct m0_be_tx        tx;
	struct m0_be_tx_credit cred;
	m0_bcount_t            size;
	void                  *data;
	const union {
		struct m0_uint128    u128;
		struct be_ut_complex complex;
	} captured;
};

enum { SHIFT = 0 };

static void
be_ut_transact(struct be_ut_tx_x *x, struct m0_be_seg *seg, void **alloc)
{
	int rc;

	m0_be_tx_prep(&x->tx, &x->cred);

	rc = m0_be_tx_open_sync(&x->tx);
	M0_UT_ASSERT(rc == 0);

	x->data = be_ut_tx_alloc(alloc, x->size);

	/* Dirty the memory. */
	M0_CASSERT(sizeof(struct m0_uint128) != sizeof(int));
	M0_CASSERT(sizeof(struct m0_uint128) != sizeof(struct be_ut_complex));
	M0_ASSERT(M0_IN(x->size, (sizeof(struct m0_uint128),
				  sizeof(struct be_ut_complex))));
	memcpy(x->data, &x->captured, x->size);

	/* Capture dirty memory. */
	m0_be_tx_capture(&x->tx, &M0_BE_REG(seg, x->size, x->data));

	m0_be_tx_close(&x->tx);
}

/**
 * @param nr  Number of transactions to use.
 */
static void be_ut_tx_test(size_t nr)
{
	struct m0_be_ut_backend ut_be;
	struct m0_be_ut_seg     ut_seg;
	void                   *alloc;
	struct be_ut_tx_x      *x;
	struct be_ut_tx_x       xs[] = {
		{
			.size          = sizeof(struct m0_uint128),
			.captured.u128 = M0_UINT128(0xdeadd00d8badf00d,
						    0x5ca1ab1e7e1eca57)
		},
		{
			.size             = sizeof(struct be_ut_complex),
			.captured.complex = { .real = 18, .imag = 4 }
		},
		{ .size = 0 } /* terminator */
	};

	M0_PRE(0 < nr && nr < ARRAY_SIZE(xs));
	xs[nr].size = 0;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, 1 << 20);
	be_ut_tx_alloc_init(&alloc, ut_seg.bus_seg);

	for (x = xs; x->size != 0; ++x) {
		m0_be_ut_tx_init(&x->tx, &ut_be);
		m0_be_tx_get(&x->tx);
		x->cred = M0_BE_TX_CREDIT(1, x->size);
	}

	for (x = xs; x->size != 0; ++x)
		be_ut_transact(x, ut_seg.bus_seg, &alloc);

	/* Wait for transactions to become persistent. */
	for (x = xs; x->size != 0; ++x) {
		int rc = m0_be_tx_timedwait(&x->tx, M0_BITS(M0_BTS_PLACED),
					    M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);
		m0_be_tx_put(&x->tx);
	}
	for (x = xs; x->size != 0; ++x) {
		int rc = m0_be_tx_timedwait(&x->tx, M0_BITS(M0_BTS_DONE),
					    M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);
		m0_be_tx_fini(&x->tx);
	}

	be_ut_tx_alloc_fini(&alloc);
	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);

}

static void be_ut_tx_do_force (struct be_ut_tx_x *xs, size_t nr)
{
	int                      i;
	int                      nr_grps;
	struct be_ut_tx_x       *x;
	struct m0_be_tx_group  **grps;

	M0_ALLOC_ARR(grps, nr);
	M0_UT_ASSERT(grps != NULL);

	/*
	 * Find all tx groups.
	 *
	 * Note: only 1 tx group is supported at this moment.
	 */
	nr_grps = 0;
	for (x = xs; x->size != 0; ++x) {
		for (i = 0; i < nr_grps; i++) {
			if (x->tx.t_group == grps[i]) break;
		}

		if (i == nr_grps) {
			grps[i] = x->tx.t_group;
			nr_grps ++;
		}
	}

	/*
	 * Note: we will force only one tx per group.
	 */
	for (i = 0, x = xs; x->size != 0; ) {
		m0_be_tx_force(&x->tx);
		i++;

		/* Skip those txs belong to the same group. */
		while (x->tx.t_group != grps[i] && x->size != 0)
			x++;
	}

	m0_free(grps);
}

/**
 * Tests m0_be_tx_force().
 * @param nr  Number of transactions to use.
 */
static void be_ut_tx_force(size_t nr)
{
	struct m0_be_ut_backend ut_be;
	struct m0_be_ut_seg     ut_seg;
	void                   *alloc;
	struct be_ut_tx_x      *x;
	struct be_ut_tx_x       xs[] = {
		{
			.size          = sizeof(struct m0_uint128),
			.captured.u128 = M0_UINT128(0xdeadd00d8badf00d,
						    0x5ca1ab1e7e1eca57)
		},
		{
			.size             = sizeof(struct be_ut_complex),
			.captured.complex = { .real = 18, .imag = 4 }
		},
		{ .size = 0 } /* terminator */
	};

	M0_PRE(0 < nr && nr < ARRAY_SIZE(xs));
	xs[nr].size = 0;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, 1 << 20);
	be_ut_tx_alloc_init(&alloc, ut_seg.bus_seg);

	for (x = xs; x->size != 0; ++x) {
		m0_be_ut_tx_init(&x->tx, &ut_be);
		m0_be_tx_get(&x->tx);
		x->cred = M0_BE_TX_CREDIT(1, x->size);
	}

	for (x = xs; x->size != 0; ++x)
		be_ut_transact(x, ut_seg.bus_seg, &alloc);

#if 0 /* XXX */
	/* Wait for transactions to become GROUPED. */
	for (x = xs; x->size != 0; ++x) {
		int rc = m0_be_tx_timedwait(&x->tx, M0_BITS(M0_BTS_GROUPED),
					    M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);
	}
#endif

	/* Force all txs to be persistent */
	be_ut_tx_do_force(xs, nr);

	/* Wait for transactions to become persistent. */
	for (x = xs; x->size != 0; ++x) {
		int rc = m0_be_tx_timedwait(&x->tx, M0_BITS(M0_BTS_PLACED),
					    M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);
		m0_be_tx_put(&x->tx);
	}

	for (x = xs; x->size != 0; ++x) {
		int rc = m0_be_tx_timedwait(&x->tx, M0_BITS(M0_BTS_DONE),
					    M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);
		m0_be_tx_fini(&x->tx);
	}

	be_ut_tx_alloc_fini(&alloc);
	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);

}

void m0_be_ut_tx_force(void)
{
	be_ut_tx_force(2);
}


/** constants for backend UT for transaction persistence */
enum {
	BE_UT_TX_P_SEG_SIZE     = 0x10000,
	BE_UT_TX_P_TX_NR        = 0x100,
	BE_UT_TX_P_REG_NR       = 0x10,
	BE_UT_TX_P_REG_SIZE_MAX = 0x100,
};

static void be_ut_tx_reg_rand(struct m0_be_reg *reg,
			      struct m0_be_seg *seg,
			      uint64_t         *seed)
{
	uintptr_t addr;

	reg->br_seg = seg;
	reg->br_size = m0_rnd64(seed) % BE_UT_TX_P_REG_SIZE_MAX + 1;
	addr = m0_rnd64(seed) %
	       (seg->bs_size - reg->br_size - seg->bs_reserved);
	reg->br_addr = seg->bs_addr + seg->bs_reserved + addr;
}

static void be_ut_tx_reg_rand_fill(struct m0_be_reg *reg, uint64_t *seed)
{
	int i;

	for (i = 0; i < reg->br_size; ++i)
		((char *) reg->br_addr)[i] = m0_rnd64(seed) & 0xFF;
}

void m0_be_ut_tx_persistence(void)
{
	static struct m0_be_reg regs[BE_UT_TX_P_REG_NR];
	struct m0_be_ut_backend ut_be;
	struct m0_be_tx_credit  credit;
	struct m0_be_ut_seg     ut_seg;
	struct m0_be_seg       *seg;
	struct m0_be_tx         tx;
	uint64_t                seed = 0;
	int                     i;
	int                     j;
	int                     rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_TX_P_SEG_SIZE);
	seg = ut_seg.bus_seg;

	for (j = 0; j < BE_UT_TX_P_TX_NR; ++j) {
		M0_SET0(&tx);
		m0_be_ut_tx_init(&tx, &ut_be);

		for (i = 0; i < ARRAY_SIZE(regs); ++i)
			be_ut_tx_reg_rand(&regs[i], seg, &seed);

		M0_SET0(&credit);
		for (i = 0; i < ARRAY_SIZE(regs); ++i) {
			m0_be_tx_credit_add(&credit, &M0_BE_TX_CREDIT(
						    1, regs[i].br_size));
		}

		m0_be_tx_prep(&tx, &credit);

		rc = m0_be_tx_open_sync(&tx);
		M0_UT_ASSERT(rc == 0);

		for (i = 0; i < ARRAY_SIZE(regs); ++i) {
			be_ut_tx_reg_rand_fill(&regs[i], &seed);
			m0_be_tx_capture(&tx, &regs[i]);
		}

		m0_be_tx_get(&tx);
		m0_be_tx_close(&tx);
		rc = m0_be_tx_timedwait(&tx, M0_BITS(M0_BTS_PLACED),
					M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);

		m0_be_tx_put(&tx);
		rc = m0_be_tx_timedwait(&tx, M0_BITS(M0_BTS_DONE),
					M0_TIME_NEVER);
		M0_UT_ASSERT(rc == 0);
		m0_be_tx_fini(&tx);
	}

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

enum {
	BE_UT_TX_F_SEG_SIZE  = 0x20000,
	BE_UT_TX_F_TX_NR     = 0x100,
	BE_UT_TX_F_TX_CONCUR = 0x80,
	BE_UT_TX_F_INIT      = 1,
	BE_UT_TX_F_CAPTURE   = 2,
};

struct be_ut_tx_fast {
	struct m0_be_tx      txf_tx;
	struct m0_clink      txf_clink;
	struct m0_semaphore  txf_sem;
	struct m0_semaphore *txf_global_sem;
	int                  txf_state;
};

static bool be_ut_tx_fast_cb(struct m0_clink *clink)
{
	struct be_ut_tx_fast *txf;

	txf = container_of(clink, struct be_ut_tx_fast, txf_clink);
	M0_ASSERT(m0_be_tx_state(&txf->txf_tx) != M0_BTS_FAILED);
	if (m0_be_tx_state(&txf->txf_tx) == M0_BTS_ACTIVE) {
		m0_clink_del(clink);
		txf->txf_state = BE_UT_TX_F_CAPTURE;
		m0_semaphore_up(&txf->txf_sem);
		m0_semaphore_up(txf->txf_global_sem);
	}
	return false;
}

static void be_ut_tx_fast_gc_free(struct m0_be_tx *tx, void *param)
{
	struct be_ut_tx_fast *txf = param;

	txf->txf_state = BE_UT_TX_F_INIT;
	m0_semaphore_up(&txf->txf_sem);
	m0_semaphore_up(txf->txf_global_sem);
}

void m0_be_ut_tx_fast(void)
{
	struct m0_be_ut_backend  ut_be;
	struct be_ut_tx_fast    *txf;
	struct m0_semaphore      global_sem;
	struct m0_be_ut_seg      ut_seg;
	struct m0_sm_group      *grp = NULL;
	struct m0_be_seg        *seg;
	struct m0_be_reg         reg;
	struct m0_be_tx         *tx;
	int                      nr_init;
	int                      nr_closed;
	int                      i;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_TX_F_SEG_SIZE);
	seg = ut_seg.bus_seg;
	M0_ALLOC_ARR(txf, BE_UT_TX_F_TX_CONCUR);
	m0_semaphore_init(&global_sem, BE_UT_TX_F_TX_CONCUR);
	for (i = 0; i < BE_UT_TX_F_TX_CONCUR; ++i) {
		m0_semaphore_init(&txf[i].txf_sem, 1);
		m0_clink_init(&txf[i].txf_clink, &be_ut_tx_fast_cb);
		txf[i].txf_state = BE_UT_TX_F_INIT;
		txf[i].txf_global_sem = &global_sem;
	}
	reg = M0_BE_REG(seg, 1, seg->bs_addr + seg->bs_reserved);
	nr_init = 0;
	nr_closed = 0;
	while (!(nr_closed == BE_UT_TX_F_TX_NR &&
	         nr_init   == BE_UT_TX_F_TX_NR + BE_UT_TX_F_TX_CONCUR)) {
		if (grp != NULL)
			m0_sm_group_unlock(grp);
		m0_semaphore_down(&global_sem);
		if (grp != NULL)
			m0_sm_group_lock(grp);
		for (i = 0; i < BE_UT_TX_F_TX_CONCUR; ++i) {
			if (m0_semaphore_trydown(&txf[i].txf_sem))
				break;
		}
		M0_UT_ASSERT(i != BE_UT_TX_F_TX_CONCUR);
		tx = &txf[i].txf_tx;
		switch (txf[i].txf_state) {
		case BE_UT_TX_F_INIT:
			if (nr_init++ >= BE_UT_TX_F_TX_NR)
				break;
			M0_SET0(tx);
			m0_be_ut_tx_init(tx, &ut_be);
			grp = tx->t_sm.sm_grp;
			m0_be_tx_gc_enable(tx, &be_ut_tx_fast_gc_free, &txf[i]);
			m0_clink_add(&tx->t_sm.sm_chan, &txf[i].txf_clink);
			m0_be_tx_prep(tx, &M0_BE_TX_CREDIT(1, reg.br_size));
			m0_be_tx_open(tx);
			break;
		case BE_UT_TX_F_CAPTURE:
			m0_be_tx_capture(tx, &reg);
			++reg.br_addr;
			m0_be_tx_close(tx);
			++nr_closed;
			break;
		default:
			M0_IMPOSSIBLE("invalid state %d", txf[i].txf_state);
		}
	}
	for (i = 0; i < BE_UT_TX_F_TX_CONCUR; ++i) {
		m0_clink_fini(&txf[i].txf_clink);
		m0_semaphore_fini(&txf[i].txf_sem);
	}
	m0_semaphore_fini(&global_sem);
	m0_free(txf);
	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

enum {
	BE_UT_TX_C_THREAD_NR     = 0x10,
	BE_UT_TX_C_TX_PER_THREAD = 0x40,
};

struct be_ut_tx_thread_state {
	struct m0_thread         tts_thread;
	struct m0_be_ut_backend *tts_ut_be;
	bool                     tts_exclusive;
};

static void be_ut_tx_run_tx_helper(struct be_ut_tx_thread_state *state,
				   struct m0_be_tx              *tx,
				   bool                          exclusive)
{
	M0_SET0(tx);
	m0_be_ut_tx_init(tx, state->tts_ut_be);

	if (exclusive)
		m0_be_tx_exclusive_open_sync(tx);
	else
		m0_be_tx_open_sync(tx);

	m0_be_tx_close_sync(tx);
	m0_be_tx_fini(tx);
}

static void be_ut_tx_thread(struct be_ut_tx_thread_state *state)
{
	struct m0_be_tx tx;
	int             i;

	if (state->tts_exclusive)
		be_ut_tx_run_tx_helper(state, &tx, true);
	else
		for (i = 0; i < BE_UT_TX_C_TX_PER_THREAD; ++i)
			be_ut_tx_run_tx_helper(state, &tx, false);

	m0_be_ut_backend_thread_exit(state->tts_ut_be);
}

void m0_be_ut_tx_concurrent_helper(bool exclusive)
{
	static struct be_ut_tx_thread_state threads[BE_UT_TX_C_THREAD_NR];
	struct m0_be_ut_backend             ut_be;
	int                                 i;
	int                                 rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);

	for (i = 0; i < ARRAY_SIZE(threads); ++i) {
		threads[i].tts_ut_be     = &ut_be;
		threads[i].tts_exclusive = !exclusive ? false :
			(i == ARRAY_SIZE(threads) / 2 ||
			 i == ARRAY_SIZE(threads) / 4 ||
			 i == ARRAY_SIZE(threads) * 3 / 4);
		rc = M0_THREAD_INIT(&threads[i].tts_thread,
				    struct be_ut_tx_thread_state *, NULL,
				    &be_ut_tx_thread, &threads[i],
				    "#%dbe_ut_tx", i);
		M0_UT_ASSERT(rc == 0);
	}
	for (i = 0; i < ARRAY_SIZE(threads); ++i) {
		rc = m0_thread_join(&threads[i].tts_thread);
		M0_UT_ASSERT(rc == 0);
		m0_thread_fini(&threads[i].tts_thread);
	}

	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_tx_concurrent(void)
{
	m0_be_ut_tx_concurrent_helper(false);
}

void m0_be_ut_tx_concurrent_excl(void)
{
	m0_be_ut_tx_concurrent_helper(true);
}

enum {
	BE_UT_TX_CAPTURING_SEG_SIZE = 0x10000,
	BE_UT_TX_CAPTURING_TX_NR    = 0x10,
	BE_UT_TX_CAPTURING_NR       = 0x100,
	BE_UT_TX_CAPTURING_RANGE    = 0x20,
};

M0_BASSERT(BE_UT_TX_CAPTURING_RANGE >= sizeof(uint64_t));

void m0_be_ut_tx_capturing(void)
{
	struct m0_be_ut_backend  ut_be;
	struct m0_be_tx_credit   cred = M0_BE_TX_CREDIT_TYPE(uint64_t);
	struct m0_be_ut_seg      ut_seg;
	struct m0_be_seg        *seg;
	struct m0_be_tx          tx;
	uint64_t                 seed = 0;
	uint64_t                *ptr;
	int                      i;
	int                      j;
	int                      rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_TX_CAPTURING_SEG_SIZE);
	seg = ut_seg.bus_seg;

	m0_be_tx_credit_mul(&cred, BE_UT_TX_CAPTURING_NR);
	for (i = 0; i < BE_UT_TX_CAPTURING_TX_NR; ++i) {
		m0_be_ut_tx_init(&tx, &ut_be);
		m0_be_tx_prep(&tx, &cred);
		rc = m0_be_tx_open_sync(&tx);
		M0_UT_ASSERT(rc == 0);

		for (j = 0; j < BE_UT_TX_CAPTURING_NR; ++j) {
			ptr = seg->bs_addr + m0_be_seg_reserved(seg) +
			      m0_rnd64(&seed) %
			      (BE_UT_TX_CAPTURING_RANGE - sizeof *ptr);
			*ptr = m0_rnd64(&seed);
			m0_be_tx_capture(&tx, &M0_BE_REG_PTR(seg, ptr));
		}

		m0_be_tx_close_sync(&tx);
		m0_be_tx_fini(&tx);
	}
	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

enum {
	BE_UT_TX_GC_SEG_SIZE         = 0x10000,
	BE_UT_TX_GC_TX_NR            = 0x100,
	BE_UT_TX_GC_RAND_DENOMINATOR = 0x5,
};

struct be_ut_gc_test {
	struct m0_be_tx     *bugc_tx;
	bool                 bugc_gc_enabled;
	struct m0_clink      bugc_clink;
	long                *bugc_ringbuf;
	int                 *bugc_rb_pos;
	struct m0_semaphore *bugc_rb_ready;
	struct m0_mutex     *bugc_rb_lock;
	long                 bugc_index;
	struct m0_semaphore *bugc_gc_done;
};

static struct be_ut_gc_test be_ut_gc_tests[BE_UT_TX_GC_TX_NR];

static bool be_ut_tx_gc_cb(struct m0_clink *clink)
{
	struct be_ut_gc_test *test;

	test = container_of(clink, struct be_ut_gc_test, bugc_clink);
	M0_ASSERT(m0_be_tx_state(test->bugc_tx) != M0_BTS_FAILED);
	if (m0_be_tx_state(test->bugc_tx) == M0_BTS_ACTIVE) {
		m0_clink_del(clink);
		m0_mutex_lock(test->bugc_rb_lock);
		test->bugc_ringbuf[*test->bugc_rb_pos] = test->bugc_index;
		++*test->bugc_rb_pos;
		m0_semaphore_up(test->bugc_rb_ready);
		m0_mutex_unlock(test->bugc_rb_lock);
	}
	return false;
}

static void be_ut_tx_gc_free(struct m0_be_tx *tx, void *param)
{
	int i;
	int index = -1;

	for (i = 0; i < BE_UT_TX_GC_TX_NR; ++i) {
		if (be_ut_gc_tests[i].bugc_tx == tx) {
			M0_UT_ASSERT(index == -1);
			index = i;
		}
	}
	M0_UT_ASSERT(&be_ut_gc_tests[index] == param);
	M0_UT_ASSERT(be_ut_gc_tests[index].bugc_gc_enabled);
	M0_UT_ASSERT(be_ut_gc_tests[index].bugc_tx != NULL);
	m0_free(tx);
	be_ut_gc_tests[index].bugc_tx = NULL;
	m0_semaphore_up(be_ut_gc_tests[index].bugc_gc_done);
}

static void be_ut_tx_gc_free_tx_failed(struct m0_be_tx *tx, void *param)
{
	M0_UT_ASSERT(false);
}

void m0_be_ut_tx_gc(void)
{
	struct m0_be_ut_backend  ut_be;
	struct be_ut_gc_test    *test;
	struct m0_semaphore      rb_ready;
	struct m0_semaphore      gc_done;
	struct m0_be_ut_seg      ut_seg;
	struct m0_sm_group      *grp;
	struct m0_be_seg        *seg;
	struct m0_mutex          rb_lock;
	struct m0_be_tx         *tx;
	uint64_t                 seed = 0;
	long                    *array;
	long                    *ringbuf;
	bool                     gc_enabled;
	int                      gc_enabled_nr = 0;
	int                      rb_pos = 0;
	int                      i;
	int                      rc;

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_TX_CAPTURING_SEG_SIZE);
	seg = ut_seg.bus_seg;
	array = seg->bs_addr + m0_be_seg_reserved(seg);
	M0_ALLOC_ARR(ringbuf, BE_UT_TX_GC_TX_NR);
	M0_UT_ASSERT(ringbuf != NULL);
	m0_semaphore_init(&rb_ready, 0);
	m0_semaphore_init(&gc_done, 0);
	m0_mutex_init(&rb_lock);

	for (i = 0; i < BE_UT_TX_GC_TX_NR; ++i) {
		M0_ALLOC_PTR(tx);
		M0_UT_ASSERT(tx != NULL);

		be_ut_gc_tests[i] = (struct be_ut_gc_test){
			.bugc_tx         = tx,
			.bugc_gc_enabled = false,
			.bugc_ringbuf    = ringbuf,
			.bugc_rb_pos     = &rb_pos,
			.bugc_rb_ready   = &rb_ready,
			.bugc_rb_lock    = &rb_lock,
			.bugc_index      = i,
			.bugc_gc_done    = &gc_done,
		};
		m0_clink_init(&be_ut_gc_tests[i].bugc_clink, &be_ut_tx_gc_cb);

		gc_enabled = m0_rnd64(&seed) % BE_UT_TX_GC_RAND_DENOMINATOR;
		be_ut_gc_tests[i].bugc_gc_enabled = gc_enabled;
		tx = be_ut_gc_tests[i].bugc_tx;
		gc_enabled_nr += gc_enabled;

		m0_be_ut_tx_init(tx, &ut_be);
		m0_be_tx_prep(tx, &M0_BE_TX_CREDIT_PTR(&array[i]));
		m0_clink_add(&tx->t_sm.sm_chan, &be_ut_gc_tests[i].bugc_clink);
		m0_be_tx_open(tx);
		grp = tx->t_sm.sm_grp;

		if (gc_enabled) {
			m0_be_tx_gc_enable(tx, &be_ut_tx_gc_free,
					   &be_ut_gc_tests[i]);
		}
	}
	/*
	 * Check that at least one transaction has gc enabled
	 * and at least one doesn't.
	 */
	M0_UT_ASSERT(gc_enabled_nr > 0);
	M0_UT_ASSERT(gc_enabled_nr < BE_UT_TX_GC_TX_NR);

	for (i = 0; i < BE_UT_TX_GC_TX_NR; ++i) {
		m0_sm_group_unlock(grp);
		m0_semaphore_down(&rb_ready);
		m0_sm_group_lock(grp);

		m0_mutex_lock(&rb_lock);
		test = &be_ut_gc_tests[ringbuf[i]];
		tx   = test->bugc_tx;
		m0_mutex_unlock(&rb_lock);

		if (m0_rnd64(&seed) % BE_UT_TX_GC_RAND_DENOMINATOR)
			m0_be_tx_capture(tx, &M0_BE_REG_PTR(seg, &array[i]));
		m0_be_tx_close(tx);
	}
	m0_sm_group_unlock(grp);
	for (i = 0; i < gc_enabled_nr; ++i)
		m0_semaphore_down(&gc_done);
	m0_sm_group_lock(grp);
	/*
	 * Finalise and free all tx that weren't GCed.
	 */
	for (i = 0; i < BE_UT_TX_GC_TX_NR; ++i) {
		test = &be_ut_gc_tests[i];
		tx   = test->bugc_tx;
		M0_UT_ASSERT(equi(test->bugc_gc_enabled, tx == NULL));
		if (tx != NULL) {
			m0_be_tx_timedwait(tx, M0_BITS(M0_BTS_DONE),
					   M0_TIME_NEVER);
			m0_be_tx_fini(tx);
			m0_free(tx);
		}
	}
	M0_UT_ASSERT(!m0_semaphore_trydown(&rb_ready));
	M0_UT_ASSERT(!m0_semaphore_trydown(&gc_done));
	/*
	 * Check "m0_be_tx_open() failed with gc enabled" case.
	 */
	M0_ALLOC_PTR(tx);
	M0_UT_ASSERT(tx != NULL);
	m0_be_ut_tx_init(tx, &ut_be);
	m0_be_tx_prep(tx, &m0_be_tx_credit_invalid);
	m0_be_tx_gc_enable(tx, &be_ut_tx_gc_free_tx_failed, tx);
	rc = m0_be_tx_open_sync(tx);
	M0_UT_ASSERT(rc != 0);
	m0_be_tx_fini(tx);
	m0_free(tx);

	m0_mutex_fini(&rb_lock);
	m0_semaphore_fini(&gc_done);
	m0_semaphore_fini(&rb_ready);
	m0_free(ringbuf);
	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

enum {
	BE_UT_TX_PAYLOAD_SEG_SIZE = 0x10000,
	BE_UT_TX_PAYLOAD_TEST_NR  = 0x100,
	BE_UT_TX_PAYLOAD_SIZE_MAX = 0x10000,
};

struct be_ut_tx_payload_test {
	/* tx to test */
	struct m0_be_tx   tpt_tx;
	/* tx payload credit */
	m0_bcount_t       tpt_credit;
	/* how many bytes to fill in the test */
	m0_bcount_t       tpt_fill;
	/* segment for capturing */
	struct m0_be_seg *tpt_seg;
	/* number of bytes to capture */
	m0_bcount_t       tpt_capture;
	/* offset for capturing in seg (starting with bs_reserved) */
	m0_bcount_t       tpt_offset;
};

#define TX_PAYLOAD_TEST(credit, fill, capture, offset) {        \
	.tpt_credit = (credit),                                 \
	.tpt_fill = (fill),                                     \
	.tpt_capture = (capture),                               \
	.tpt_offset = (offset),                                 \
}

static uint64_t be_ut_tx_payload_seed;

static void be_ut_tx_buf_fill_random(char *data, m0_bcount_t size)
{
	int i;

	for (i = 0; i < size; ++i)
		data[i] = m0_rnd64(&be_ut_tx_payload_seed) & 0xFF;
}

static void be_ut_tx_payload_test_nr(struct m0_be_ut_backend      *ut_be,
                                     struct be_ut_tx_payload_test *test,
                                     size_t                        nr)
{
	struct m0_be_tx_credit  cred;
	struct m0_be_seg       *seg;
	struct m0_be_reg        reg;
	int                     rc;
	int                     i;

	for (i = 0; i < nr; ++i) {
		m0_be_ut_tx_init(&test[i].tpt_tx, ut_be);
		if (test[i].tpt_capture > 0) {
			cred = M0_BE_TX_CREDIT(1, test[i].tpt_capture);
			m0_be_tx_prep(&test[i].tpt_tx, &cred);
		}
		m0_be_tx_payload_prep(&test[i].tpt_tx, test[i].tpt_credit);
		/*
		 * If test hangs here - try to increase BE log size
		 * or decrease BE_UT_TX_PAYLOAD_TX_NR.
		 */
		rc = m0_be_tx_open_sync(&test[i].tpt_tx);
		M0_UT_ASSERT(rc == 0);
		M0_ASSERT(test[i].tpt_fill <= test[i].tpt_credit);
		M0_ASSERT(test[i].tpt_fill <= test[i].tpt_tx.t_payload.b_nob);
		be_ut_tx_buf_fill_random(test[i].tpt_tx.t_payload.b_addr,
					 test[i].tpt_fill);
		test[i].tpt_tx.t_payload.b_nob = test[i].tpt_fill;
		if (test[i].tpt_capture > 0) {
			seg = test[i].tpt_seg;
			reg = M0_BE_REG(seg, test[i].tpt_capture,
			                seg->bs_addr + seg->bs_reserved +
			                test[i].tpt_offset);
			m0_be_tx_capture(&test[i].tpt_tx, &reg);
		}
		m0_be_tx_close_sync(&test[i].tpt_tx);
		m0_be_tx_fini(&test[i].tpt_tx);
	}
}

void m0_be_ut_tx_payload(void)
{
	enum {
		PAYLOAD_SIZE = BE_UT_TX_PAYLOAD_SIZE_MAX,
	};
	struct be_ut_tx_payload_test *tests;
	struct be_ut_tx_payload_test  special_cases[] = {
		/* payload credit, payload fill, capture credit, capture fill */
		TX_PAYLOAD_TEST(0, 0,     0,     0),
		TX_PAYLOAD_TEST(0, 0, 0x100,     0),
		TX_PAYLOAD_TEST(0, 0, 0x100, 0x100),
		TX_PAYLOAD_TEST(1, 0,     0,     0),
		TX_PAYLOAD_TEST(1, 1,     0,     0),
		TX_PAYLOAD_TEST(PAYLOAD_SIZE,                0,     0,     0),
		TX_PAYLOAD_TEST(PAYLOAD_SIZE,                0, 0x100,     0),
		TX_PAYLOAD_TEST(PAYLOAD_SIZE,                0, 0x100, 0x100),
		TX_PAYLOAD_TEST(PAYLOAD_SIZE,     PAYLOAD_SIZE, 0x100, 0x100),
		TX_PAYLOAD_TEST(PAYLOAD_SIZE, PAYLOAD_SIZE / 2, 0x100,     0),
	};
	struct m0_be_ut_backend  ut_be = {};
	struct m0_be_ut_seg      ut_seg = {};
	struct m0_be_seg        *seg;
	m0_bcount_t              size;
	int                      i;

	be_ut_tx_payload_seed = 42;     /* always works like magic */

	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_TX_CAPTURING_SEG_SIZE);
	seg = ut_seg.bus_seg;

	for (i = 0; i < ARRAY_SIZE(special_cases); ++i)
		special_cases[i].tpt_seg = seg;
	be_ut_tx_payload_test_nr(&ut_be, &special_cases[0],
				 ARRAY_SIZE(special_cases));

	M0_ALLOC_ARR(tests, BE_UT_TX_PAYLOAD_TEST_NR);
	for (i = 0; i < BE_UT_TX_PAYLOAD_TEST_NR; ++i) {
		tests[i].tpt_seg = seg;
		size = seg->bs_size - seg->bs_reserved;

		tests[i].tpt_credit  = m0_rnd64(&be_ut_tx_payload_seed) %
				       BE_UT_TX_PAYLOAD_TEST_NR + 1;
		tests[i].tpt_fill    = m0_rnd64(&be_ut_tx_payload_seed) %
				       tests[i].tpt_credit;
		tests[i].tpt_capture = m0_rnd64(&be_ut_tx_payload_seed) % size;
		tests[i].tpt_offset  = m0_rnd64(&be_ut_tx_payload_seed) %
				       (size - tests[i].tpt_capture);
	}
	be_ut_tx_payload_test_nr(&ut_be, tests, BE_UT_TX_PAYLOAD_TEST_NR);
	m0_free(tests);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
}

#undef TX_PAYLOAD_TEST

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
