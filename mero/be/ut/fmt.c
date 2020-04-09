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
 * Original author: Anatoliy Bilenko <anatoliy.bilenko@seagate.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 16-Dec-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/fmt.h"

#include "lib/misc.h"
#include "lib/buf.h"    /* m0_buf */
#include "lib/errno.h"  /* EPROTO */
#include "lib/memory.h" /* m0_alloc */
#include "lib/thread.h" /* LAMBDA */

#include "ut/ut.h"
#include "ut/misc.h"    /* m0_ut_random_shuffle */

/**
 * @addtogroup be
 *
 * @{
 */

/*
 * encode
 * decode
 *      valid
 *      invalid
 *      change random bytes
 *      size > valid size
 *      size < valid size
 *      size = 0
 */
void m0_be_ut_fmt_log_header(void)
{
	struct m0_be_fmt_log_header  lh = {};
	struct m0_be_fmt_log_header *lh_decoded;
	m0_bcount_t                  size;
	struct m0_buf                encoded;
	struct m0_bufvec             bvec_encoded;
	struct m0_bufvec_cursor      cur_encoded;
	bool                         equal;
	int                          rc;

	bvec_encoded = M0_BUFVEC_INIT_BUF(&encoded.b_addr, &encoded.b_nob);
	/* encode + decode */
	rc = m0_be_fmt_log_header_init(&lh, NULL);
	M0_UT_ASSERT(rc == 0);
	size = m0_be_fmt_log_header_size(&lh);
	m0_buf_init(&encoded, m0_alloc(size), size);

	lh.flh_group_size = 0x10;
	lh.flh_group_lsn  = 0x20;

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_log_header_encode(&lh, &cur_encoded);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_log_header_decode(&lh_decoded, &cur_encoded,
					 M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc == 0);

	equal = memcmp(&lh, lh_decoded, sizeof lh) == 0;
	M0_UT_ASSERT(equal);

	m0_be_fmt_log_header_decoded_free(lh_decoded);
	m0_be_fmt_log_header_fini(&lh);
	m0_buf_free(&encoded);

	/* decode when buf size is greater than encoded size */
	m0_buf_init(&encoded, m0_alloc(size + 1), size + 1);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_log_header_decode(&lh_decoded, &cur_encoded,
					 M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc == 0);

	m0_be_fmt_log_header_decoded_free(lh_decoded);
	m0_buf_free(&encoded);

	/* decode when buf size is less than encoded size */
	m0_buf_init(&encoded, m0_alloc(size - 1), size - 1);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_log_header_decode(&lh_decoded, &cur_encoded,
					 M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc != 0);

	m0_be_fmt_log_header_decoded_free(lh_decoded);
	m0_buf_free(&encoded);

	/* decode when buf size is 0 */
	m0_buf_init(&encoded, NULL, 0);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_log_header_decode(&lh_decoded, &cur_encoded,
					 M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc != 0);

	m0_be_fmt_log_header_decoded_free(lh_decoded);
	m0_buf_free(&encoded);
}

/*
 * encode
 * decode
 *      valid
 *      invalid
 *      change random bytes
 *      size > valid size
 *      size < valid size
 *      size = 0
 */
void m0_be_ut_fmt_cblock(void)
{
	struct m0_be_fmt_cblock  cb = {};
	struct m0_be_fmt_cblock *cb_decoded;
	m0_bcount_t              size;
	struct m0_buf            encoded;
	struct m0_bufvec         bvec_encoded;
	struct m0_bufvec_cursor  cur_encoded;
	bool                     equal;
	int                      rc;

	bvec_encoded = M0_BUFVEC_INIT_BUF(&encoded.b_addr, &encoded.b_nob);
	/* encode + decode */
	rc = m0_be_fmt_cblock_init(&cb, NULL);
	M0_UT_ASSERT(rc == 0);
	size = m0_be_fmt_cblock_size(&cb);
	m0_buf_init(&encoded, m0_alloc(size), size);

	cb.gcb_lsn   = 0x01;
	cb.gcb_magic = 0xcafebabe;
	cb.gcb_size  = 128;
	cb.gcb_tx_nr = 10;

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_cblock_encode(&cb, &cur_encoded);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_cblock_decode(&cb_decoded, &cur_encoded,
				     M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc == 0);

	equal = memcmp(&cb, cb_decoded, sizeof cb) == 0;
	M0_UT_ASSERT(equal);

	m0_be_fmt_cblock_decoded_free(cb_decoded);
	m0_be_fmt_cblock_fini(&cb);
	m0_buf_free(&encoded);

	/* decode when buf size is greater than encoded size */
	m0_buf_init(&encoded, m0_alloc(size + 1), size + 1);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_cblock_decode(&cb_decoded, &cur_encoded,
				     M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc == 0);

	m0_be_fmt_cblock_decoded_free(cb_decoded);
	m0_buf_free(&encoded);

	/* decode when buf size is less than encoded size */
	m0_buf_init(&encoded, m0_alloc(size - 1), size - 1);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_cblock_decode(&cb_decoded, &cur_encoded,
				     M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc != 0);

	m0_be_fmt_cblock_decoded_free(cb_decoded);
	m0_buf_free(&encoded);

	/* decode when buf size is 0 */
	m0_buf_init(&encoded, NULL, 0);

	m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
	rc = m0_be_fmt_cblock_decode(&cb_decoded, &cur_encoded,
				     M0_BE_FMT_DECODE_CFG_DEFAULT);
	M0_UT_ASSERT(rc != 0);

	m0_be_fmt_cblock_decoded_free(cb_decoded);
	m0_buf_free(&encoded);
}

static bool fmt_group_eq(struct m0_be_fmt_group *left,
			 struct m0_be_fmt_group *right)
{
	uint32_t rnr = left->fg_content_header.fch_reg_area.chr_nr;
	uint32_t tnr = left->fg_content_header.fch_txs.cht_nr;
	uint32_t pnr = left->fg_content.fmc_payloads.fcp_nr;

	struct m0_be_fmt_content_header_reg_area *lra =
		&left->fg_content_header.fch_reg_area;
	struct m0_be_fmt_content_header_reg_area *rra =
		&right->fg_content_header.fch_reg_area;
	struct m0_be_fmt_content_header_txs      *ltxs =
		&left->fg_content_header.fch_txs;
	struct m0_be_fmt_content_header_txs      *rtxs =
		&right->fg_content_header.fch_txs;
	struct m0_buf                            *lps =
		left->fg_content.fmc_payloads.fcp_payload;
	struct m0_buf                            *rps =
		right->fg_content.fmc_payloads.fcp_payload;
	struct m0_buf                            *lreg =
		left->fg_content.fmc_reg_area.cra_reg;
	struct m0_buf                            *rreg =
		right->fg_content.fmc_reg_area.cra_reg;

	return memcmp(&left->fg_header, &right->fg_header,
	              sizeof left->fg_header) == 0 &&
	       left->fg_content_header.fch_reg_area.chr_nr ==
	       right->fg_content_header.fch_reg_area.chr_nr &&
	       left->fg_content_header.fch_txs.cht_nr ==
	       right->fg_content_header.fch_txs.cht_nr &&
	       m0_forall(i, rnr, memcmp(&lra->chr_reg[i], &rra->chr_reg[i],
	                                sizeof lra->chr_reg[i]) == 0) &&
	       m0_forall(i, tnr, memcmp(&ltxs->cht_tx[i], &rtxs->cht_tx[i],
	                                sizeof ltxs->cht_tx[i]) == 0) &&
	       left->fg_content.fmc_payloads.fcp_nr ==
	       right->fg_content.fmc_payloads.fcp_nr &&
	       left->fg_content.fmc_reg_area.cra_nr ==
	       right->fg_content.fmc_reg_area.cra_nr &&
	       m0_forall(i, pnr, m0_buf_eq(&lps[i], &rps[i])) &&
	       m0_forall(i, rnr, m0_buf_eq(&lreg[i], &rreg[i])) &&
	       m0_be_fmt_group_size(left) == m0_be_fmt_group_size(right);
}

#define CFG(tx_nr_max, reg_nr_max, payload_sz_max, ra_sz_max)   \
	(const struct m0_be_fmt_group_cfg) {                    \
		.fgc_tx_nr_max         = tx_nr_max,             \
		.fgc_reg_nr_max        = reg_nr_max,            \
		.fgc_payload_size_max  = payload_sz_max,        \
		.fgc_reg_size_max = ra_sz_max,			\
	}
#define REG(size, addr, buf)                    \
	(const struct m0_be_fmt_reg) {          \
		.fr_size = size,                \
		.fr_addr = addr,                \
		.fr_buf  = buf                  \
	}
#define TX(p_addr, p_nob, id)                           \
	(const struct m0_be_fmt_tx) {                   \
		.bft_payload = {                        \
			.b_addr = p_addr,               \
			.b_nob = p_nob,                 \
		},                                      \
		.bft_id = id,                           \
	}

static void be_fmt_group_populate(struct m0_be_fmt_group *group,
				  void                   *payload,
				  size_t                  payload_size,
				  size_t                  TX_NR_MAX,
				  size_t                  REG_NR_MAX)
{
	int i;

	for (i = 0; i < TX_NR_MAX; ++i)
		m0_be_fmt_group_tx_add(group, i == 0 ?
				       &TX(payload, payload_size, i) :
				       &TX(   NULL,            0, i));

	for (i = 0; i < REG_NR_MAX; ++i)
		m0_be_fmt_group_reg_add(group, &REG(payload_size,
						    (void *) 0x400000000128ULL,
						    payload));
}

struct m0_be_ut_fmt_group_test {
	struct m0_be_fmt_group_cfg cfg;
	bool                       buf_lack;
	bool                       last;
	void (*group_spoil)(struct m0_be_ut_fmt_group_test *,
			    struct m0_be_fmt_group *);
	void (*encoded_spoil)(struct m0_be_ut_fmt_group_test *,
			      struct m0_buf *);
};

static void spoil_nr(struct m0_be_ut_fmt_group_test *t, struct m0_buf *b)
{
	int   i;
	uint8_t *addr = (uint8_t *)b->b_addr;
	/* make sure one byte can hold the size */
	M0_UT_ASSERT((uint8_t) t->cfg.fgc_tx_nr_max == t->cfg.fgc_tx_nr_max);
	for (i = 0; i < b->b_nob; i++) {
		if (addr[i] == t->cfg.fgc_tx_nr_max) {
			/* corrupt size, make it bigger */
			M0_LOG(M0_DEBUG, "corrupt starting from %p", &addr[i]);
			addr[i+1] = 0xDE;
			addr[i+2] = 0xAD;
			addr[i+3] = 0xBE;
			addr[i+4] = 0xAF;
		}
	}
}

void m0_be_ut_fmt_group_size_max(void)
{
	struct m0_be_fmt_group_cfg cfg;
	struct m0_be_fmt_group     fg;
	m0_bcount_t                size;
	m0_bcount_t                size_max;
	uint64_t                   max;
	uint64_t                   reg_area_size;
	void                      *buf;
	int                        rc;
	int                        i;
	int                        j;

	uint64_t reg_sizes1[] = { 1024, 1024, 1024 };
	uint64_t reg_sizes2[] = { 2048, 512,  512  };
	uint64_t reg_sizes3[] = { 3072, 0,    0    };
	uint64_t reg_sizes4[] = { 1, 2, 3, 4, 5, 6 };
	struct {
		uint64_t  tx_nr;
		uint64_t  reg_nr;
		uint64_t  payload_size;
		uint64_t *reg_sizes;
	} tests[] = {
		{ 3, ARRAY_SIZE(reg_sizes1), 1024, reg_sizes1 },
		{ 4, ARRAY_SIZE(reg_sizes2), 100,  reg_sizes2 },
		{ 5, ARRAY_SIZE(reg_sizes3), 1,    reg_sizes3 },
		{ 1, ARRAY_SIZE(reg_sizes4), 99,   reg_sizes4 },
		{ 0, 0, 0, NULL },
	};

	for (i = 0; i < ARRAY_SIZE(tests); ++i) {
		reg_area_size = 0;
		max           = tests[i].payload_size;
		for (j = 0; j < tests[i].reg_nr; ++j) {
			reg_area_size += tests[i].reg_sizes[j];
			max = max64u(max, tests[i].reg_sizes[j]);
		}
		cfg = CFG(tests[i].tx_nr, tests[i].reg_nr,
			  tests[i].payload_size * tests[i].tx_nr,
			  reg_area_size);

		buf = max == 0 ? NULL : m0_alloc(max);
		M0_UT_ASSERT(ergo(max != 0, buf != NULL));

		M0_SET0(&fg);
		rc = m0_be_fmt_group_init(&fg, &cfg);
		M0_UT_ASSERT(rc == 0);
		for (j = 0; j < tests[i].tx_nr; ++j) {
			m0_be_fmt_group_tx_add(&fg, &TX(buf,
						tests[i].payload_size, 0));
		}
		for (j = 0; j < tests[i].reg_nr; ++j) {
			m0_be_fmt_group_reg_add(&fg, &REG(tests[i].reg_sizes[j],
							  NULL, buf));
		}
		size_max = m0_be_fmt_group_size_max(&cfg);
		size     = m0_be_fmt_group_size(&fg);
		M0_UT_ASSERT(size == size_max);
		m0_be_fmt_group_fini(&fg);
		m0_free(buf);
	}
}

enum {
	BE_UT_FMT_GROUP_SIZE_MAX_RND_NR   = 0x40,
	BE_UT_FMT_GROUP_SIZE_MAX_RND_ITER = 0x40,
};

static m0_bcount_t
be_ut_fmt_group_size_max_check(struct m0_be_fmt_group_cfg *fg_cfg,
                               uint64_t                   *seed)
{
	struct m0_be_fmt_group *fg;
	m0_bcount_t             size;
	m0_bcount_t             size_max;
	uint64_t               *payload_size = NULL;
	uint64_t               *reg_size = NULL;
	int                     rc;
	uint64_t                i;

	if ((fg_cfg->fgc_tx_nr_max == 0 && fg_cfg->fgc_payload_size_max > 0) ||
	    (fg_cfg->fgc_reg_nr_max == 0 && fg_cfg->fgc_reg_size_max > 0))
		return M0_BCOUNT_MAX;

	M0_ALLOC_PTR(fg);
	M0_UT_ASSERT(fg != NULL);
	if (fg_cfg->fgc_tx_nr_max > 0) {
		M0_ALLOC_ARR(payload_size, fg_cfg->fgc_tx_nr_max);
		M0_UT_ASSERT(payload_size != NULL);
		m0_ut_random_arr_with_sum(payload_size, fg_cfg->fgc_tx_nr_max,
		                          fg_cfg->fgc_payload_size_max, seed);
		m0_ut_random_shuffle(payload_size, fg_cfg->fgc_tx_nr_max, seed);
		M0_ASSERT(m0_reduce(i, fg_cfg->fgc_tx_nr_max,
				    0, + payload_size[i]) ==
		          fg_cfg->fgc_payload_size_max);
	}
	if (fg_cfg->fgc_reg_nr_max > 0) {
		M0_ALLOC_ARR(reg_size, fg_cfg->fgc_reg_nr_max);
		M0_UT_ASSERT(reg_size != NULL);
		m0_ut_random_arr_with_sum(reg_size, fg_cfg->fgc_reg_nr_max,
		                          fg_cfg->fgc_reg_size_max, seed);
		m0_ut_random_shuffle(reg_size, fg_cfg->fgc_reg_nr_max, seed);
		M0_ASSERT(m0_reduce(i, fg_cfg->fgc_reg_nr_max,
				    0, + reg_size[i]) ==
		          fg_cfg->fgc_reg_size_max);
	}
	rc = m0_be_fmt_group_init(fg, fg_cfg);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < fg_cfg->fgc_tx_nr_max; ++i)
		m0_be_fmt_group_tx_add(fg, &TX(NULL, payload_size[i], 0));
	for (i = 0; i < fg_cfg->fgc_reg_nr_max; ++i)
		m0_be_fmt_group_reg_add(fg, &REG(reg_size[i], NULL, NULL));
	size = m0_be_fmt_group_size(fg);
	size_max = m0_be_fmt_group_size_max(fg_cfg);
	M0_ASSERT_INFO(size == size_max, "size=%"PRIu64" size_max=%"PRIu64,
	               size, size_max);
	m0_be_fmt_group_fini(fg);
	m0_free(reg_size);
	m0_free(payload_size);
	m0_free(fg);
	return size_max;
}

static void be_ut_fmt_group_size_max_test(struct m0_be_fmt_group_cfg *fg_cfg2,
                                          uint64_t                   *seed)
{
	struct m0_be_fmt_group_cfg  fg_cfg = *fg_cfg2;
	m0_bcount_t                 size_max;
	m0_bcount_t                 size_max_prev;
	uint64_t                   *value[] = {
		&fg_cfg.fgc_tx_nr_max,
		&fg_cfg.fgc_reg_nr_max,
		&fg_cfg.fgc_payload_size_max,
		&fg_cfg.fgc_reg_size_max,
		&fg_cfg.fgc_seg_nr_max,
	};
	int                         i;

	size_max_prev = be_ut_fmt_group_size_max_check(&fg_cfg, seed);
	for (i = 0; i < BE_UT_FMT_GROUP_SIZE_MAX_RND_ITER; ++i) {
		++*value[m0_rnd64(seed) % ARRAY_SIZE(value)];
		size_max = be_ut_fmt_group_size_max_check(&fg_cfg, seed);
		M0_UT_ASSERT(ergo(size_max_prev != M0_BCOUNT_MAX,
				  size_max >= size_max_prev));
		size_max_prev = size_max == M0_BCOUNT_MAX ?
			        size_max_prev : size_max;
	}
}

void m0_be_ut_fmt_group_size_max_rnd(void)
{
	struct m0_be_fmt_group_cfg *fg_cfg;
	uint64_t                    seed = 42;
	int                         i;

	be_ut_fmt_group_size_max_test(&(struct m0_be_fmt_group_cfg){
		.fgc_tx_nr_max        = 0,
		.fgc_reg_nr_max       = 0,
		.fgc_payload_size_max = 0,
		.fgc_reg_size_max     = 0,
		.fgc_seg_nr_max       = 0,
	}, &seed);
	be_ut_fmt_group_size_max_test(&(struct m0_be_fmt_group_cfg){
		.fgc_tx_nr_max        = 0x100,
		.fgc_reg_nr_max       = 0x100,
		.fgc_payload_size_max = 0x100,
		.fgc_reg_size_max     = 0x100,
		.fgc_seg_nr_max       = 0x100,
	}, &seed);

	M0_ALLOC_PTR(fg_cfg);
	M0_UT_ASSERT(fg_cfg != NULL);
	for (i = 0; i < BE_UT_FMT_GROUP_SIZE_MAX_RND_NR; ++i) {
		*fg_cfg = (struct m0_be_fmt_group_cfg){
			.fgc_tx_nr_max        = m0_rnd64(&seed) % 0x10 + 1,
			.fgc_reg_nr_max       = m0_rnd64(&seed) % 0x10 + 1,
			.fgc_payload_size_max = m0_rnd64(&seed) % 0x1000,
			.fgc_reg_size_max     = m0_rnd64(&seed) % 0x1000,
			.fgc_seg_nr_max       = m0_rnd64(&seed) % 0x10 + 1,
		};
		be_ut_fmt_group_size_max_test(fg_cfg, &seed);
	}
	m0_free(fg_cfg);
}

void m0_be_ut_fmt_group(void)
{
	int rc;
	int i;
	struct m0_be_ut_fmt_group_test test[] = {
		{ /* main route of the test, shall pass */
			.cfg = CFG(10, 100, 99, 800),
		},
		{ /* main route of the test, shall pass */
			.cfg = CFG(1, 2, 20, 16),
		},
		{ /* simulates the lack of space in the buffer for decode */
			.cfg = CFG(10, 100, 99, 800),
			.buf_lack = true,
		},
		{ /* simulates data corruption after encoding the group */
			.cfg = CFG(10, 100, 99, 800),
			.encoded_spoil = LAMBDA(void, (struct m0_be_ut_fmt_group_test *t,
						       struct m0_buf *b) {
					memset(b->b_addr, 0xCC, b->b_nob / 10);
				}),
		},
		{ /* simulates group data sizes mismatch: tx header size exceeds
		   * reasonable values */
			.cfg = CFG(10, 100, 99, 800),
			.group_spoil = LAMBDA(void, (struct m0_be_ut_fmt_group_test *t,
						     struct m0_be_fmt_group *g){
					g->fg_content_header.fch_txs.cht_nr =
						1 << 10;
				}),

		},
		{ /* simulates group data sizes mismatch: reg area size exceeds
		   * reasonable values */
			.cfg = CFG(10, 100, 99, 800),
			.group_spoil = LAMBDA(void, (struct m0_be_ut_fmt_group_test *t,
						     struct m0_be_fmt_group *g){
				g->fg_content_header.fch_reg_area.chr_nr =
					1 << 10;
				}),

		},
#if 0 /* XXX: crashes in xcode. */
fmt-group  mero:  fb00  FATAL : [lib/assert.c:46:m0_panic] panic: fatal signal delivered at unknown() (unknown:0)  [git: jenkins-OSAINT_mero-294-230-g2d11e2b-dirty]
Mero panic: fatal signal delivered at unknown() unknown:0 (errno: 2) (last failed: none) [git: jenkins-OSAINT_mero-294-230-g2d11e2b-dirty] pid: 76277
Mero panic reason: signo: 11
/.libs/libmero-0.1.0.so(m0_arch_backtrace+0x1f)[0x7f3e679b3431]
/.libs/libmero-0.1.0.so(m0_arch_panic+0x124)[0x7f3e679b3578]
/.libs/libmero-0.1.0.so(m0_panic+0x1e5)[0x7f3e6799e9a8]
/.libs/libmero-0.1.0.so(+0x22b6ee)[0x7f3e679b36ee]
/lib64/libpthread.so.0[0x3593e0f4a0]
/lib64/libc.so.6[0x3593a889e8]
/lib64/libc.so.6(memmove+0x7e)[0x3593a8252e]
/.libs/libmero-0.1.0.so(m0_bufvec_cursor_copy+0xa2)[0x7f3e679ad75b]
/.libs/libmero-0.1.0.so(+0x2d4ccb)[0x7f3e67a5cccb]
/.libs/libmero-0.1.0.so(m0_xcode_encode+0x1d)[0x7f3e67a5cdbc]
/.libs/libmero-0.1.0.so(+0x18ee45)[0x7f3e67916e45]
/.libs/libmero-0.1.0.so(m0_be_fmt_group_encode+0x41)[0x7f3e67917897]
/.libs/libmero-ut.so.0(m0_be_ut_fmt_group+0x3d0)[0x7f3e68699484]
#endif
#if 0
		{ /* simulates group data sizes mismatch: payload size exceeds
		   * reasonable values */
			.cfg = CFG(10, 100, 99, 99),
			.group_spoil = LAMBDA(void, (struct m0_be_ut_fmt_group_test *t,
						     struct m0_be_fmt_group *g){
					g->fg_content.fmc_payloads.fcp_nr =
						1 << 10;
				}),

		},
		{ /* simulates group data sizes mismatch: payload buf size
		   * exceeds reasonable values */
			.cfg = CFG(10, 100, 99, 99),
			.group_spoil = LAMBDA(void, (struct m0_be_ut_fmt_group_test *t,
						     struct m0_be_fmt_group *g){
				g->fg_content.fmc_payloads.fcp_payload[0].b_nob =
					1 << 24;
				}),

		},
		{ /* simulates group data sizes mismatch: reg area size exceeds
		   * reasonable values */
			.cfg = CFG(10, 100, 99, 99),
			.group_spoil = LAMBDA(void, (struct m0_be_ut_fmt_group_test *t,
						     struct m0_be_fmt_group *g){
					g->fg_content.fmc_reg_area.cra_nr =
						1 << 10;
				}),

		},
#endif
		/* The following tests simulate data corruption after encoding
		   the group.  Make sure that even such combination will not
		   crash xcode.
		  */
		{
			.cfg = CFG(0xEA, 100, 99, 800),
			.encoded_spoil = spoil_nr,
		},
		{
			.cfg = CFG(10, 0xEA, 99, 0xEA * 8),
			.encoded_spoil = spoil_nr,
		},
		{
			.cfg = CFG(10, 100, 0xEA, 800),
			.encoded_spoil = spoil_nr,
		},
		{
			.last = true,
		}
	};

	for (i = 0; !test[i].last; ++i) {
		struct m0_be_fmt_group  group = {};
		struct m0_be_fmt_group *group_decoded;
		m0_bcount_t             size;
		struct m0_buf           encoded;
		struct m0_bufvec	bvec_encoded;
		struct m0_bufvec_cursor	cur_encoded;
		bool                    reseted = false;

		M0_LOG(M0_DEBUG, "test: %i, cfg:(%lu, %lu, %lu, %lu)", i,
		       test[i].cfg.fgc_tx_nr_max,
		       test[i].cfg.fgc_reg_nr_max,
		       test[i].cfg.fgc_reg_size_max,
		       test[i].cfg.fgc_payload_size_max);
		bvec_encoded = M0_BUFVEC_INIT_BUF(&encoded.b_addr,
						  &encoded.b_nob);
		rc = m0_be_fmt_group_init(&group, &test[i].cfg);
		M0_UT_ASSERT(rc == 0);

		size = !test[i].buf_lack ?
			m0_be_fmt_group_size_max(&test[i].cfg) :
			m0_be_fmt_group_size(&group) - 1;

		m0_buf_init(&encoded, m0_alloc(size), size);

		while (true) {
			be_fmt_group_populate(&group, &size, sizeof size,
					      test[i].cfg.fgc_tx_nr_max,
					      test[i].cfg.fgc_reg_nr_max);

			if (test[i].group_spoil != NULL)
				test[i].group_spoil(&test[i], &group);

			m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
			rc = m0_be_fmt_group_encode(&group, &cur_encoded);
			if (test[i].buf_lack || test[i].group_spoil != NULL) {
				M0_UT_ASSERT(rc == -EPROTO);
				break;
			}
			M0_UT_ASSERT(rc == 0);

			if (test[i].encoded_spoil != NULL)
				test[i].encoded_spoil(&test[i], &encoded);
			m0_bufvec_cursor_init(&cur_encoded, &bvec_encoded);
			rc = m0_be_fmt_group_decode(
				&group_decoded,
				&cur_encoded,
				M0_BE_FMT_DECODE_CFG_DEFAULT_WITH_TRACE);
			if (test[i].encoded_spoil == NULL)
				M0_UT_ASSERT(rc == 0 &&
					     fmt_group_eq(&group,
							  group_decoded));
			else
				M0_UT_ASSERT(rc != 0 ||
					     !fmt_group_eq(&group,
							   group_decoded));

			m0_be_fmt_group_decoded_free(group_decoded);

			if (reseted)
				break;
			m0_be_fmt_group_reset(&group);
			reseted = true;
		}

		m0_be_fmt_group_fini(&group);
		m0_buf_free(&encoded);
	}
}

/** @} end of be group */

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
