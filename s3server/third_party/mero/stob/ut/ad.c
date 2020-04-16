/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 08/24/2010
 */

#include "lib/arith.h"		/* min64u */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/memory.h"
#include "lib/ub.h"
#include "lib/assert.h"
#include "ut/stob.h"		/* m0_ut_stob_create */
#include "ut/ut.h"

#include "dtm/dtm.h"		/* m0_dtx */
#include "stob/ad.h"		/* m0_stob_ad_cfg_make */
#include "stob/ad_private.h"	/* stob_ad_domain2ad */
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/stob.h"
#include "balloc/balloc.h"

#include "be/ut/helper.h"
#include "lib/errno.h"

/**
   @addtogroup stob
   @{
 */

enum {
	NR                     = 4,
	MIN_BUF_SIZE           = 4096,
	MIN_BUF_SIZE_IN_BLOCKS = 4,
	SEG_SIZE               = 1 << 24,
};

static struct m0_stob_domain *dom_back;
static struct m0_stob_domain *dom_fore;
static struct m0_stob *obj_back;
static struct m0_stob *obj_fore;
static struct m0_stob_io io;
static m0_bcount_t user_vc[NR];
static m0_bcount_t stob_vc[NR];
static char *user_buf[NR];
static char *read_buf[NR];
static char *zero_buf[NR];
static char *user_bufs[NR];
static char *read_bufs[NR];
static m0_bindex_t stob_vi[NR];
static struct m0_clink clink;
static struct m0_dtx g_tx;
struct m0_be_ut_backend ut_be;
struct m0_be_ut_seg ut_seg;
static uint32_t block_shift;
static uint32_t buf_size;

struct mock_balloc {
	m0_bindex_t         mb_next;
	struct m0_ad_balloc mb_ballroom;
};

void m0_stob_ut_ad_init(struct m0_be_ut_backend *ut_be,
			struct m0_be_ut_seg     *ut_seg,
			bool use_small_credits)
{
	struct m0_be_domain_cfg cfg = {};
	int			rc;

	M0_SET0(ut_be);
	M0_SET0(ut_seg);

	m0_be_ut_backend_cfg_default(&cfg);
	if (use_small_credits) {
		/* Reduce maximum credit size of transaction
		 * to exercise transaction breaking code in
		 * stob_ad_punch_credit()
		 */
		cfg.bc_engine.bec_tx_size_max =
			M0_BE_TX_CREDIT(1 << 18, 1 << 21);
	}
	rc = m0_be_ut_backend_init_cfg(ut_be, &cfg, true);
	M0_UT_ASSERT(rc == 0);
	m0_be_ut_seg_init(ut_seg, ut_be, SEG_SIZE);
}

void m0_stob_ut_ad_fini(struct m0_be_ut_backend *ut_be,
			struct m0_be_ut_seg     *ut_seg)
{
	m0_be_ut_seg_fini(ut_seg);
	m0_be_ut_backend_fini(ut_be);
}

static struct mock_balloc *b2mock(struct m0_ad_balloc *ballroom)
{
	return container_of(ballroom, struct mock_balloc, mb_ballroom);
}

static int mock_balloc_init(struct m0_ad_balloc *ballroom,
			    struct m0_be_seg *seg,
			    uint32_t bshift,
			    m0_bindex_t container_size,
			    m0_bcount_t groupsize,
			    m0_bcount_t spare_reserve)
{
	return 0;
}

static void mock_balloc_fini(struct m0_ad_balloc *ballroom)
{
}

static int mock_balloc_alloc(struct m0_ad_balloc *ballroom, struct m0_dtx *dtx,
			     m0_bcount_t count, struct m0_ext *out,
			     uint64_t alloc_type)
{
	struct mock_balloc *mb = b2mock(ballroom);
	m0_bcount_t giveout;

	giveout = min64u(count, 500000);
	out->e_start = mb->mb_next;
	out->e_end   = mb->mb_next + giveout;
	m0_ext_init(out);
	mb->mb_next += giveout + 1;
	/* printf("allocated %8lx/%8lx bytes: [%8lx .. %8lx)\n",
	   giveout, count,
	       out->e_start, out->e_end); */
	return 0;
}

static int mock_balloc_free(struct m0_ad_balloc *ballroom, struct m0_dtx *dtx,
			    struct m0_ext *ext)
{
	/* printf("freed     %8lx bytes: [%8lx .. %8lx)\n", m0_ext_length(ext),
	       ext->e_start, ext->e_end); */
	return 0;
}

static const struct m0_ad_balloc_ops mock_balloc_ops = {
	.bo_init  = mock_balloc_init,
	.bo_fini  = mock_balloc_fini,
	.bo_alloc = mock_balloc_alloc,
	.bo_free  = mock_balloc_free,
};

struct mock_balloc mb = {
	.mb_next = 0,
	.mb_ballroom = {
		.ab_ops = &mock_balloc_ops
	}
};

static void init_vecs()
{
	int i;

	for (i = 0; i < NR; ++i) {
		user_bufs[i] = m0_stob_addr_pack(user_buf[i], block_shift);
		read_bufs[i] = m0_stob_addr_pack(read_buf[i], block_shift);
		user_vc[i] = buf_size >> block_shift;
		stob_vc[i] = buf_size >> block_shift;
		stob_vi[i] = (buf_size * (2 * i + 1)) >> block_shift;
		memset(user_buf[i], ('a' + i)|1, buf_size);
	}
}

static int test_ad_init(bool use_small_credits)
{
	char             *dom_cfg;
	char             *dom_init_cfg;
	int               i;
	int               rc;
	struct m0_stob_id stob_id;

	rc = m0_stob_domain_create("linuxstob:./__s", "directio=true",
				    0xc0de, NULL, &dom_back);
	M0_ASSERT(rc == 0);

	m0_stob_id_make(0, 0xba5e, &dom_back->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, &obj_back);
	M0_ASSERT(rc == 0);
	rc = m0_stob_locate(obj_back);
	M0_ASSERT(rc == 0);
	rc = m0_ut_stob_create(obj_back, NULL, NULL);
	M0_ASSERT(rc == 0);

	m0_stob_ut_ad_init(&ut_be, &ut_seg, use_small_credits);

	m0_stob_ad_cfg_make(&dom_cfg, ut_seg.bus_seg,
			    m0_stob_id_get(obj_back), 0);
	M0_UT_ASSERT(dom_cfg != NULL);
	m0_stob_ad_init_cfg_make(&dom_init_cfg, &ut_be.but_dom);
	M0_UT_ASSERT(dom_init_cfg != NULL);

	rc = m0_stob_domain_create("adstob:ad", dom_init_cfg, 0xad,
				   dom_cfg, &dom_fore);
	M0_ASSERT(rc == 0);
	m0_free(dom_cfg);
	m0_free(dom_init_cfg);

	m0_stob_id_make(0, 0xd15c, &dom_fore->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, &obj_fore);
	M0_ASSERT(rc == 0);
	rc = m0_stob_locate(obj_fore);
	M0_ASSERT(rc == 0);
	rc = m0_ut_stob_create(obj_fore, NULL, &ut_be.but_dom);
	M0_ASSERT(rc == 0);

	block_shift = m0_stob_block_shift(obj_fore);
	/* buf_size is chosen so it would be at least MIN_BUF_SIZE in bytes
	 * or it would consist of at least MIN_BUF_SIZE_IN_BLOCKS blocks */
	buf_size = max_check(MIN_BUF_SIZE,
			     (1 << block_shift) * MIN_BUF_SIZE_IN_BLOCKS);

	for (i = 0; i < ARRAY_SIZE(user_buf); ++i) {
		user_buf[i] = m0_alloc_aligned(buf_size, block_shift);
		M0_ASSERT(user_buf[i] != NULL);
	}

	for (i = 0; i < ARRAY_SIZE(read_buf); ++i) {
		read_buf[i] = m0_alloc_aligned(buf_size, block_shift);
		M0_ASSERT(read_buf[i] != NULL);
	}

	for (i = 0; i < ARRAY_SIZE(zero_buf); ++i) {
		zero_buf[i] = m0_alloc_aligned(buf_size, block_shift);
		M0_ASSERT(zero_buf[i] != NULL);
	}

	init_vecs();

	return rc;
}

static int test_ad_fini(void)
{
	int i;

	m0_stob_put(obj_fore);
	m0_stob_domain_destroy(dom_fore);
	m0_stob_put(obj_back);
	m0_stob_domain_destroy(dom_back);

	m0_stob_ut_ad_fini(&ut_be, &ut_seg);

	for (i = 0; i < ARRAY_SIZE(user_buf); ++i)
		m0_free(user_buf[i]);

	for (i = 0; i < ARRAY_SIZE(read_buf); ++i)
		m0_free(read_buf[i]);

	for (i = 0; i < ARRAY_SIZE(zero_buf); ++i)
		m0_free(zero_buf[i]);

	return 0;
}

static void test_write(int nr, struct m0_dtx *tx)
{
	struct m0_sm_group *grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	struct m0_fol_frag *fol_frag;
	bool		    is_local_tx = false;
	int		    rc;

	/* @Note: This Fol record part object is not freed and shows as leak,
	 * as it is passed as embedded object in other places.
	 */
	M0_ALLOC_PTR(fol_frag);
	M0_UB_ASSERT(fol_frag != NULL);

	m0_stob_io_init(&io);

	io.si_opcode = SIO_WRITE;
	io.si_flags  = 0;
	io.si_fol_frag = fol_frag;
	io.si_user.ov_vec.v_nr = nr;
	io.si_user.ov_vec.v_count = user_vc;
	io.si_user.ov_buf = (void **)user_bufs;

	io.si_stob.iv_vec.v_nr = nr;
	io.si_stob.iv_vec.v_count = stob_vc;
	io.si_stob.iv_index = stob_vi;
	rc = m0_stob_io_private_setup(&io, obj_fore);
	M0_UT_ASSERT(rc == 0);
	m0_stob_ad_balloc_set(&io, M0_BALLOC_NORMAL_ZONE);

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&io.si_wait, &clink);

	if (tx == NULL) {
		tx = &g_tx;
		M0_SET0(tx);
		m0_dtx_init(tx, &ut_be.but_dom, grp);
		is_local_tx = true;
	}
	m0_stob_io_credit(&io, dom_fore, &tx->tx_betx_cred);
	rc = m0_dtx_open_sync(tx);
	M0_ASSERT(rc == 0);

	rc = m0_stob_io_prepare_and_launch(&io, obj_fore, tx, NULL);
	M0_ASSERT(rc == 0);

	if (is_local_tx) {
		rc = m0_dtx_done_sync(tx);
		M0_ASSERT(rc == 0);
		m0_dtx_fini(tx);
	}

	m0_chan_wait(&clink);

	M0_ASSERT(io.si_rc == 0);
	M0_ASSERT(io.si_count == (buf_size * nr) >> block_shift);

	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);

	m0_stob_io_fini(&io);
}

static void test_read(int nr)
{
	int rc;

	m0_stob_io_init(&io);

	io.si_opcode = SIO_READ;
	io.si_flags  = 0;
	io.si_user.ov_vec.v_nr = nr;
	io.si_user.ov_vec.v_count = user_vc;
	io.si_user.ov_buf = (void **)read_bufs;

	io.si_stob.iv_vec.v_nr = nr;
	io.si_stob.iv_vec.v_count = stob_vc;
	io.si_stob.iv_index = stob_vi;

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&io.si_wait, &clink);

	rc = m0_stob_io_prepare_and_launch(&io, obj_fore, &g_tx, NULL);
	M0_ASSERT(rc == 0);

	m0_chan_wait(&clink);

	M0_ASSERT(io.si_rc == 0);
	M0_ASSERT(io.si_count == (buf_size * nr) >> block_shift);

	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);

	m0_stob_io_fini(&io);
}

static void test_punch(int nr)
{
	struct m0_sm_group *grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	struct m0_dtx      *tx;
	struct m0_indexvec  range;
	struct m0_indexvec  want;
	struct m0_indexvec  got;
	int                 rc;
	int                 i;
	bool                credit_is_enough = false;
	uint32_t            idx = 0;

	rc = m0_indexvec_alloc(&range, nr);
	M0_ASSERT(rc == 0);
	rc = m0_indexvec_alloc(&want, 1);
	M0_ASSERT(rc == 0);
	rc = m0_indexvec_alloc(&got, 1);
	M0_ASSERT(rc == 0);

	for (i = 0; i < nr; ++i) {
		range.iv_vec.v_count[i] = stob_vc[i];
		range.iv_index[i] = stob_vi[i];
	}

start_again:
	tx = &g_tx;
	M0_SET0(tx);
	m0_dtx_init(tx, &ut_be.but_dom, grp);

	want.iv_vec.v_count[0] = range.iv_vec.v_count[idx];
	want.iv_index[0] = range.iv_index[idx];
	rc = m0_stob_punch_credit(obj_fore, &want, &got,
				  &tx->tx_betx_cred);
	M0_ASSERT(rc == 0);
	range.iv_index[idx] += got.iv_vec.v_count[0];
	range.iv_vec.v_count[idx] -= got.iv_vec.v_count[0];
	if (range.iv_vec.v_count[idx] == 0) {
		idx++;
		if (idx == nr)
			credit_is_enough = true;
	}
	rc = m0_dtx_open_sync(tx);
	M0_ASSERT(rc == 0);
	rc = obj_fore->so_ops->sop_punch(obj_fore, &got, &g_tx);
	M0_ASSERT(rc == 0);
	rc = m0_dtx_done_sync(tx);
	M0_ASSERT(rc == 0);
	m0_dtx_fini(tx);

	if (!credit_is_enough)
		goto start_again;

	m0_indexvec_free(&got);
	m0_indexvec_free(&want);
	m0_indexvec_free(&range);

	for (i = 0; i < nr; i++) {
		struct m0_stob_ad_domain *adom;
		struct m0_be_emap_cursor  it;
		struct m0_ext            *ext;
		adom = stob_ad_domain2ad(m0_stob_dom_get(obj_fore));
		rc = stob_ad_cursor(adom, obj_fore, stob_vi[i], &it);
		M0_ASSERT(rc == 0);
		ext = &it.ec_seg.ee_ext;
		M0_ASSERT(ext->e_start <= stob_vi[i]);
		M0_ASSERT(ext->e_end   >= stob_vi[i] + stob_vc[i]);
		M0_ASSERT(it.ec_seg.ee_val == AET_HOLE);
		m0_be_emap_close(&it);
	}
}

static void test_ad_rw_unordered()
{
	int i;

	/* Unorderd write requests */
	init_vecs();
	for (i = NR/2; i < NR; ++i) {
		stob_vi[i-(NR/2)] = (buf_size * (i + 1)) >> block_shift;
		memset(user_buf[i-(NR/2)], ('a' + i)|1, buf_size);
	}
	test_write(NR/2, NULL);

	init_vecs();
	for (i = 0; i < NR/2; ++i) {
		stob_vi[i] = (buf_size * (i + 1)) >> block_shift;
		memset(user_buf[i], ('a' + i)|1, buf_size);
	}
	test_write(NR/2, NULL);

	init_vecs();
	for (i = 0; i < NR; ++i) {
		stob_vi[i] = (buf_size * (i + 1)) >> block_shift;
		memset(user_buf[i], ('a' + i)|1, buf_size);
	}

	/* This generates unordered offsets for back stob io */
	test_read(NR);
	for (i = 0; i < NR; ++i)
		M0_ASSERT(memcmp(user_buf[i], read_buf[i], buf_size) == 0);
}

/**
   AD unit-test.
 */
static void test_ad(void)
{
	int i;

	for (i = 1; i <= NR; ++i)
		test_write(i, NULL);

	for (i = 1; i <= NR; ++i) {
		int j;
		test_read(i);
		for (j = 0; j < i; ++j)
			M0_ASSERT(memcmp(user_buf[j], read_buf[j], buf_size) == 0);
	}
}

/**
   PUNCH test.
 */
static void punch_test(void)
{
	int i;

	for (i = 1; i <= NR; ++i) {
		int j;
		test_punch(i);
		test_read(i);
		for (j = 0; j < i; ++j)
			M0_ASSERT(memcmp(zero_buf[j], read_buf[j], buf_size) == 0);
	}
}

static void test_ad_undo(void)
{
	struct m0_sm_group *grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	struct m0_fol_frag *rfrag;
	struct m0_dtx       tx = {};
	int                 rc;

	M0_SET0(&g_tx);
	m0_dtx_init(&g_tx, &ut_be.but_dom, grp);
	memset(user_buf[0], 'a', buf_size);
	test_write(1, &g_tx);
	rc = m0_dtx_done_sync(&g_tx);
	M0_ASSERT(rc == 0);

	test_read(1);
	M0_ASSERT(memcmp(user_buf[0], read_buf[0], buf_size) == 0);

	rfrag = m0_rec_frag_tlist_head(&g_tx.tx_fol_rec.fr_frags);
	M0_ASSERT(rfrag != NULL);

	/* Write new data in stob */
	m0_dtx_init(&tx, &ut_be.but_dom, grp);
	rfrag->rp_ops->rpo_undo_credit(rfrag, &tx.tx_betx_cred);
	memset(user_buf[0], 'b', buf_size);
	test_write(1, &tx);

	/* Do the undo operation. */
	rc = rfrag->rp_ops->rpo_undo(rfrag, &tx.tx_betx);
	M0_UT_ASSERT(rc == 0);

	rc = m0_dtx_done_sync(&tx);
	M0_ASSERT(rc == 0);
	m0_dtx_fini(&tx);

	m0_dtx_fini(&g_tx);

	test_read(1);
	M0_ASSERT(memcmp(user_buf[0], read_buf[0], buf_size) != 0);

}

void m0_stob_ut_adieu_ad(void)
{
	int rc;

	rc = test_ad_init(false);
	M0_ASSERT(rc == 0);
	test_ad();
	test_ad_rw_unordered();
	test_ad_undo();
	rc = test_ad_fini();
	M0_ASSERT(rc == 0);

	rc = test_ad_init(true);
	M0_ASSERT(rc == 0);
	punch_test();
	rc = test_ad_fini();
	M0_ASSERT(rc == 0);
}

static void ub_write(int i)
{
	test_write(NR - 1, NULL);
}

static void ub_read(int i)
{
	test_read(NR - 1);
}

static int ub_init(const char *opts M0_UNUSED)
{
	return test_ad_init(false);
}

static void ub_fini(void)
{
	(void)test_ad_fini();
}

enum { UB_ITER = 100 };

struct m0_ub_set m0_ad_ub = {
	.us_name = "ad-ub",
	.us_init = ub_init,
	.us_fini = ub_fini,
	.us_run  = {
		{ .ub_name = "write-prime",
		  .ub_iter = 1,
		  .ub_round = ub_write },

		{ .ub_name = "write",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_write },

		{ .ub_name = "read",
		  .ub_iter = UB_ITER,
		  .ub_round = ub_read },

		{ .ub_name = NULL }
	}
};

/** @} end group stob */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
