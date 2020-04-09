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
 * Original creation date: 4-Jul-2013
 */

#include <string.h>

#include "be/tx_group_format.h"

#include "ut/stob.h"
#include "ut/ut.h"
#include "be/ut/helper.h"
#include "be/seg.h"
#include "be/op.h"              /* M0_BE_OP_SYNC */
#include "be/log_discard.h"     /* m0_be_log_discard */
#include "be/pd.h"              /* m0_be_pd */
#include "lib/arith.h"
#include "lib/memory.h"
#include "stob/stob.h"
#include "stob/domain.h"

struct be_ut_tgf_tx {
	m0_bcount_t   tgft_payload_size;
	struct m0_buf tgft_payload;
	uint64_t      tgft_id;
};

struct be_ut_tgf_reg {
	m0_bcount_t tgfr_size;
	void       *tgfr_seg_addr;
	void       *tgfr_buf;
};

struct be_ut_tgf_group {
	struct m0_be_group_format_cfg tgfg_cfg;
	struct m0_be_group_format     tgfg_gft;
	int                           tgfg_tx_nr;
	struct be_ut_tgf_tx          *tgfg_txs;
	int                           tgfg_reg_nr;
	struct be_ut_tgf_reg         *tgfg_regs;
};

struct be_ut_tgf_ctx {
	struct m0_be_log         tgfc_log;
	struct m0_mutex          tgfc_lock;
	struct m0_be_log_discard tgfc_log_discard;
	struct m0_be_log_discard_cfg tgfc_log_discard_cfg;
	struct m0_be_pd          tgfc_pd;
	struct m0_be_pd_cfg      tgfc_pd_cfg;
	struct m0_stob_domain   *tgfc_sdom;
	struct m0_stob          *tgfc_seg_stob;
	struct m0_be_seg         tgfc_seg;
	void                    *tgfc_seg_addr;
	int                      tgfc_group_nr;
	struct be_ut_tgf_group  *tgfc_groups;
};

/* TODO move initialisation of log to be ut helper */
enum {
	BE_UT_TGF_SEG_ADDR            = 0x300000000000ULL,
	BE_UT_TGF_SEG_SIZE            = 1024 * 1024,

	BE_UT_TGF_LOG_SIZE            = 1024 * 1024,
	BE_UT_TGF_LOG_STOB_DOMAIN_KEY = 100,
	BE_UT_TGF_LOG_STOB_KEY        = 42,
	BE_UT_TGF_LOG_RBUF_NR         = 8,

	BE_UT_TGF_SEG_NR_MAX          = 256,

	BE_UT_TGF_MAGIC               = 0xfffe,
};

static const char *be_ut_tgf_log_sdom_location   = "linuxstob:./log";
static const char *be_ut_tgf_log_sdom_init_cfg   = "directio=true";
static const char *be_ut_tgf_log_sdom_create_cfg = "";
static bool        be_ut_tgf_do_discard;

static void be_ut_tgf_log_got_space_cb(struct m0_be_log *log)
{
}

static void be_ut_tgf_log_cfg_set(struct m0_be_log_cfg  *log_cfg,
				  struct m0_stob_domain *sdom,
				  struct m0_mutex       *lock)
{
	*log_cfg = (struct m0_be_log_cfg){
		.lc_store_cfg = {
			/* temporary solution BEGIN */
			.lsc_stob_domain_location   = "linuxstob:./log_store-tmp",
			.lsc_stob_domain_init_cfg   = "directio=true",
			.lsc_stob_domain_key        = 0x1000,
			.lsc_stob_domain_create_cfg = NULL,
			/* temporary solution END */
			.lsc_size            = BE_UT_TGF_LOG_SIZE,
			.lsc_stob_create_cfg = NULL,
			.lsc_rbuf_nr         = BE_UT_TGF_LOG_RBUF_NR,
		},
		.lc_got_space_cb = &be_ut_tgf_log_got_space_cb,
		.lc_lock         = lock,
	};
	m0_stob_id_make(0, BE_UT_TGF_LOG_STOB_KEY,
	                m0_stob_domain_id_get(sdom),
			&log_cfg->lc_store_cfg.lsc_stob_id);
}

static void be_ut_tgf_log_init(struct be_ut_tgf_ctx *ctx)
{
	struct m0_be_log_cfg log_cfg;
	int                  rc;

	m0_mutex_init(&ctx->tgfc_lock);
	rc = m0_stob_domain_create(be_ut_tgf_log_sdom_location,
				   be_ut_tgf_log_sdom_init_cfg,
				   BE_UT_TGF_LOG_STOB_DOMAIN_KEY,
				   be_ut_tgf_log_sdom_create_cfg,
				   &ctx->tgfc_sdom);
	M0_UT_ASSERT(rc == 0);
	be_ut_tgf_log_cfg_set(&log_cfg, ctx->tgfc_sdom, &ctx->tgfc_lock);
	rc = m0_be_log_create(&ctx->tgfc_log, &log_cfg);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_tgf_log_fini(struct be_ut_tgf_ctx *ctx)
{
	int rc;

	m0_be_log_destroy(&ctx->tgfc_log);
	rc = m0_stob_domain_destroy(ctx->tgfc_sdom);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_fini(&ctx->tgfc_lock);
}

static void be_ut_tgf_log_open(struct be_ut_tgf_ctx *ctx)
{
	struct m0_be_log_cfg log_cfg;
	int                  rc;

	M0_SET0(&ctx->tgfc_log);
	be_ut_tgf_log_cfg_set(&log_cfg, ctx->tgfc_sdom, &ctx->tgfc_lock);
	rc = m0_be_log_open(&ctx->tgfc_log, &log_cfg);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_tgf_log_close(struct be_ut_tgf_ctx *ctx)
{
	m0_be_log_close(&ctx->tgfc_log);
}

static void be_ut_tgf_seg_init(struct be_ut_tgf_ctx *ctx)
{
	struct m0_be_seg *seg = &ctx->tgfc_seg;
	int               rc;

	ctx->tgfc_seg_stob = m0_ut_stob_linux_get();
	M0_UT_ASSERT(ctx->tgfc_seg_stob != NULL);
	m0_be_seg_init(seg, ctx->tgfc_seg_stob, NULL, M0_BE_SEG_FAKE_ID);
	rc = m0_be_seg_create(seg, BE_UT_TGF_SEG_SIZE,
			      (void *)BE_UT_TGF_SEG_ADDR);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_seg_open(seg);
	M0_UT_ASSERT(rc == 0);
	ctx->tgfc_seg_addr = (char*)seg->bs_addr + seg->bs_reserved;
}

static void be_ut_tgf_seg_fini(struct be_ut_tgf_ctx *ctx)
{
	int rc;

	m0_be_seg_close(&ctx->tgfc_seg);
	rc = m0_be_seg_destroy(&ctx->tgfc_seg);
	M0_UT_ASSERT(rc == 0);
	m0_be_seg_fini(&ctx->tgfc_seg);
	m0_ut_stob_put(ctx->tgfc_seg_stob, true);
}

static void be_ut_tgf_rnd_fill(void *buf, size_t size)
{
	static uint64_t seed;
	size_t          i;

	for (i = 0; i < size; ++i) {
		((unsigned char *)buf)[i] = (m0_rnd64(&seed) >> 16) & 0xffULL;
	}
}

static void be_ut_tgf_buf_init(struct be_ut_tgf_ctx   *ctx,
			       struct be_ut_tgf_group *group)
{
	struct be_ut_tgf_reg *reg;
	struct be_ut_tgf_tx  *tx;
	struct m0_be_seg     *seg         = &ctx->tgfc_seg;
	void                 *addr        = ctx->tgfc_seg_addr;
	m0_bcount_t           payload_max = 0;
	m0_bcount_t           ra_size_max = 0;
	int                   rc;
	int                   i;

	for (i = 0; i < group->tgfg_tx_nr; ++i) {
		tx = &group->tgfg_txs[i];
		rc = m0_buf_alloc(&tx->tgft_payload, tx->tgft_payload_size);
		M0_UT_ASSERT(rc == 0);
		payload_max += tx->tgft_payload_size;
		be_ut_tgf_rnd_fill(tx->tgft_payload.b_addr,
				   tx->tgft_payload.b_nob);
	}
	for (i = 0; i < group->tgfg_reg_nr; ++i) {
		reg = &group->tgfg_regs[i];
		reg->tgfr_buf = m0_alloc(reg->tgfr_size);
		M0_UT_ASSERT(reg->tgfr_buf != NULL);
		reg->tgfr_seg_addr = addr;
		addr               = (char *)addr + reg->tgfr_size;
		ra_size_max       += reg->tgfr_size;
		be_ut_tgf_rnd_fill(reg->tgfr_buf, reg->tgfr_size);
	}
	M0_UT_ASSERT((char*)addr - (char*)seg->bs_addr <= seg->bs_size);
	ctx->tgfc_seg_addr = addr;

	group->tgfg_cfg = (struct m0_be_group_format_cfg) {
		.gfc_fmt_cfg = {
			.fgc_tx_nr_max         = group->tgfg_tx_nr,
			.fgc_reg_nr_max        = group->tgfg_reg_nr,
			.fgc_payload_size_max  = payload_max,
			.fgc_reg_size_max      = ra_size_max,
			.fgc_seg_nr_max        = BE_UT_TGF_SEG_NR_MAX,
		},
		.gfc_log         = &ctx->tgfc_log,
		.gfc_log_discard = &ctx->tgfc_log_discard,
		.gfc_pd          = &ctx->tgfc_pd,
	};
}

static void be_ut_tgf_buf_fini(struct be_ut_tgf_ctx   *ctx,
			       struct be_ut_tgf_group *group)
{
	int i;

	for (i = 0; i < group->tgfg_tx_nr; ++i) {
		m0_buf_free(&group->tgfg_txs[i].tgft_payload);
	}
	for (i = 0; i < group->tgfg_reg_nr; ++i) {
		m0_free(group->tgfg_regs[i].tgfr_buf);
	}
}

static void be_ut_tgf_group_init(struct be_ut_tgf_ctx   *ctx,
				 struct be_ut_tgf_group *group)
{
	int rc;

	M0_SET0(&group->tgfg_gft);
	rc = m0_be_group_format_init(&group->tgfg_gft, &group->tgfg_cfg, NULL,
				     &ctx->tgfc_log);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_group_format_allocate(&group->tgfg_gft);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_tgf_group_fini(struct be_ut_tgf_ctx   *ctx,
				 struct be_ut_tgf_group *group)
{
	m0_be_group_format_deallocate(&group->tgfg_gft);
	m0_be_group_format_fini(&group->tgfg_gft);
}

static void be_ut_tgf_group_write(struct be_ut_tgf_ctx   *ctx,
				  struct be_ut_tgf_group *group,
				  bool                    seg_write)
{
	struct be_ut_tgf_tx         *tx;
	struct m0_be_fmt_tx          ftx;
	struct be_ut_tgf_reg        *reg;
	struct m0_be_reg_d           regd;
	struct m0_be_fmt_group_info *info;
	struct m0_be_seg            *seg = &ctx->tgfc_seg;
	struct m0_be_group_format   *gft = &group->tgfg_gft;
	struct m0_be_fmt_group_cfg  *cfg = &group->tgfg_cfg.gfc_fmt_cfg;
	m0_bcount_t                  reserved_size;
	int                          rc;
	int                          i;

	reserved_size  = m0_be_group_format_log_reserved_size(&ctx->tgfc_log,
				&M0_BE_TX_CREDIT(group->tgfg_reg_nr,
						 cfg->fgc_reg_size_max),
				cfg->fgc_payload_size_max);
	reserved_size *= group->tgfg_tx_nr;
	m0_mutex_lock(&ctx->tgfc_lock);
	rc = m0_be_log_reserve(&ctx->tgfc_log, reserved_size);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_unlock(&ctx->tgfc_lock);

	m0_be_group_format_reset(gft);
	M0_BE_OP_SYNC(op, m0_be_group_format_prepare(gft, &op));
	for (i = 0; i < group->tgfg_tx_nr; ++i) {
		tx  = &group->tgfg_txs[i];
		ftx = M0_BE_FMT_TX(tx->tgft_payload, tx->tgft_id);
		m0_be_group_format_tx_add(gft, &ftx);
	}
	for (i = 0; i < group->tgfg_reg_nr; ++i) {
		reg  = &group->tgfg_regs[i];
		regd = M0_BE_REG_D(M0_BE_REG(seg, reg->tgfr_size,
					     reg->tgfr_seg_addr),
				   reg->tgfr_buf);
		m0_be_group_format_reg_log_add(gft, &regd);
		m0_be_group_format_reg_seg_add(gft, &regd);
	}
	info = m0_be_group_format_group_info(gft);
	info->gi_unknown = BE_UT_TGF_MAGIC;

	m0_mutex_lock(&ctx->tgfc_lock);
	m0_be_group_format_log_use(gft, reserved_size);
	m0_mutex_unlock(&ctx->tgfc_lock);
	m0_be_group_format_encode(gft);
	rc = M0_BE_OP_SYNC_RET(op,
			       m0_be_group_format_log_write(gft, &op),
			       bo_sm.sm_rc);
	M0_UT_ASSERT(rc == 0);

	m0_mutex_lock(&ctx->tgfc_lock);
	m0_be_log_record_skip_discard(&gft->gft_log_record);
	m0_mutex_unlock(&ctx->tgfc_lock);

	if (seg_write) {
		m0_be_group_format_seg_place_prepare(gft);
		rc = M0_BE_OP_SYNC_RET(op,
				       m0_be_group_format_seg_place(gft, &op),
				       bo_sm.sm_rc);
		M0_UT_ASSERT(rc == 0);
	}
}

static void be_ut_tgf_group_read_check(struct be_ut_tgf_ctx   *ctx,
				       struct be_ut_tgf_group *group,
				       bool                    seg_check)
{
	struct be_ut_tgf_tx         *tx;
	struct m0_be_fmt_tx          ftx;
	struct be_ut_tgf_reg        *reg;
	struct m0_be_reg_d           regd;
	struct m0_be_fmt_group_info *info;
	struct m0_be_group_format   *gft = &group->tgfg_gft;
	bool                         available;
	int                          rc;
	int                          nr;
	int                          i;

	m0_be_group_format_reset(gft);
	M0_BE_OP_SYNC(op, m0_be_group_format_prepare(gft, &op));
	available = m0_be_log_recovery_record_available(&ctx->tgfc_log);
	M0_UT_ASSERT(available);
	m0_mutex_lock(&ctx->tgfc_lock);
	m0_be_group_format_recovery_prepare(gft, &ctx->tgfc_log);
	m0_mutex_unlock(&ctx->tgfc_lock);
	rc = M0_BE_OP_SYNC_RET(op,
			       m0_be_group_format_log_read(gft, &op),
			       bo_sm.sm_rc);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_group_format_decode(gft);
	M0_UT_ASSERT(rc == 0);
	info = m0_be_group_format_group_info(gft);
	M0_UT_ASSERT(info->gi_unknown == BE_UT_TGF_MAGIC);
	nr = m0_be_group_format_tx_nr(gft);
	M0_UT_ASSERT(nr == group->tgfg_tx_nr);
	for (i = 0; i < nr; ++i) {
		tx  = &group->tgfg_txs[i];
		ftx = (struct m0_be_fmt_tx){};
		m0_be_group_format_tx_get(gft, i, &ftx);
		M0_UT_ASSERT(ftx.bft_id == tx->tgft_id);
		M0_UT_ASSERT(ftx.bft_payload.b_nob == tx->tgft_payload_size);
		M0_UT_ASSERT(memcmp(ftx.bft_payload.b_addr,
				    tx->tgft_payload.b_addr,
				    tx->tgft_payload_size) == 0);
	}
	nr = m0_be_group_format_reg_nr(gft);
	M0_UT_ASSERT(nr == group->tgfg_reg_nr);
	for (i = 0; i < nr; ++i) {
		reg  = &group->tgfg_regs[i];
		regd = (struct m0_be_reg_d){};
		m0_be_group_format_reg_get(gft, i, &regd);
		M0_UT_ASSERT(regd.rd_reg.br_size == reg->tgfr_size);
		M0_UT_ASSERT(regd.rd_reg.br_addr == reg->tgfr_seg_addr);
		M0_UT_ASSERT(memcmp(regd.rd_buf, reg->tgfr_buf,
				    reg->tgfr_size) == 0);
	}

	m0_mutex_lock(&ctx->tgfc_lock);
	// XXX m0_be_group_format_log_discard(gft);
	m0_mutex_unlock(&ctx->tgfc_lock);

	if (seg_check) {
		m0_be_seg_close(&ctx->tgfc_seg);
		m0_be_seg_fini(&ctx->tgfc_seg);
		M0_SET0(&ctx->tgfc_seg);
		m0_be_seg_init(&ctx->tgfc_seg, ctx->tgfc_seg_stob, NULL,
			       M0_BE_SEG_FAKE_ID);
		rc = m0_be_seg_open(&ctx->tgfc_seg);
		M0_UT_ASSERT(rc == 0);
		nr = m0_be_group_format_reg_nr(gft);
		for (i = 0; i < nr; ++i) {
			reg = &group->tgfg_regs[i];
			M0_UT_ASSERT(memcmp(reg->tgfr_seg_addr, reg->tgfr_buf,
					    reg->tgfr_size) == 0);
		}
	}
	m0_be_group_format_seg_place_prepare(gft);
	rc = M0_BE_OP_SYNC_RET(op,
	                       m0_be_group_format_seg_place(gft, &op),
	                       bo_sm.sm_rc);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_tgf_ldsc_sync(struct m0_be_log_discard      *ld,
                                struct m0_be_op               *op,
                                struct m0_be_log_discard_item *ldi)
{
	m0_be_op_active(op);
	m0_be_op_done(op);
}

static void be_ut_tgf_ldsc_discard(struct m0_be_log_discard      *ld,
                                   struct m0_be_log_discard_item *ldi)
{
	if (be_ut_tgf_do_discard)
		m0_be_group_format_discard(ld, ldi);
}

static void be_ut_tgf_test(int group_nr, struct be_ut_tgf_group *groups)
{
	struct m0_be_group_format_cfg gfc_cfg;
	struct be_ut_tgf_ctx          ctx;
	int                           rc;
	int                           i;

	ctx = (struct be_ut_tgf_ctx) {
		.tgfc_group_nr = group_nr,
		.tgfc_groups   = groups,
		.tgfc_log_discard_cfg = {
			.ldsc_items_max         = group_nr,
			.ldsc_items_threshold   = group_nr,
			.ldsc_items_pending_max = group_nr,
			.ldsc_loc               = m0_locality0_get(),
			.ldsc_sync_timeout      = M0_TIME_ONE_SECOND,
			.ldsc_sync              = &be_ut_tgf_ldsc_sync,
			.ldsc_discard           = &be_ut_tgf_ldsc_discard,
		},
		.tgfc_pd_cfg = {
			.bpdc_seg_io_nr = group_nr,
			.bpdc_sched = {
				.bisc_pos_start = 0,
			}
		},
	};

	be_ut_tgf_log_init(&ctx);
	be_ut_tgf_seg_init(&ctx);

	for (i = 0; i < group_nr; ++i)
		be_ut_tgf_buf_init(&ctx, &groups[i]);

	gfc_cfg = groups[0].tgfg_cfg;
	for (i = 1; i < group_nr; ++i) {
		gfc_cfg.gfc_fmt_cfg.fgc_reg_nr_max =
		     max_check(gfc_cfg.gfc_fmt_cfg.fgc_reg_nr_max,
		               groups[i].tgfg_cfg.gfc_fmt_cfg.fgc_reg_nr_max);
		gfc_cfg.gfc_fmt_cfg.fgc_reg_size_max =
		     max_check(gfc_cfg.gfc_fmt_cfg.fgc_reg_size_max,
		               groups[i].tgfg_cfg.gfc_fmt_cfg.fgc_reg_size_max);
		gfc_cfg.gfc_fmt_cfg.fgc_seg_nr_max =
		     max_check(gfc_cfg.gfc_fmt_cfg.fgc_seg_nr_max,
		               groups[i].tgfg_cfg.gfc_fmt_cfg.fgc_seg_nr_max);
	}

	m0_be_group_format_seg_io_credit(&gfc_cfg,
					 &ctx.tgfc_pd_cfg.bpdc_io_credit);
	rc = m0_be_log_discard_init(&ctx.tgfc_log_discard,
	                            &ctx.tgfc_log_discard_cfg);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_pd_init(&ctx.tgfc_pd, &ctx.tgfc_pd_cfg);
	M0_UT_ASSERT(rc == 0);

	be_ut_tgf_do_discard = false;
	for (i = 0; i < group_nr; ++i) {
		be_ut_tgf_group_init(&ctx, &groups[i]);
		be_ut_tgf_group_write(&ctx, &groups[i], true);
		m0_mutex_lock(&ctx.tgfc_lock);
		be_ut_tgf_group_fini(&ctx, &groups[i]);
		m0_mutex_unlock(&ctx.tgfc_lock);
	}
	be_ut_tgf_log_close(&ctx);

	M0_BE_OP_SYNC(op, m0_be_log_discard_flush(&ctx.tgfc_log_discard, &op));
	m0_be_pd_fini(&ctx.tgfc_pd);
	m0_be_log_discard_fini(&ctx.tgfc_log_discard);
	M0_SET0(&ctx.tgfc_log_discard);
	rc = m0_be_log_discard_init(&ctx.tgfc_log_discard,
	                            &ctx.tgfc_log_discard_cfg);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_pd_init(&ctx.tgfc_pd, &ctx.tgfc_pd_cfg);
	M0_UT_ASSERT(rc == 0);

	be_ut_tgf_log_open(&ctx);
	for (i = 0; i < group_nr; ++i) {
		be_ut_tgf_group_init(&ctx, &groups[i]);
	}
	be_ut_tgf_do_discard = true;
	for (i = 0; i < group_nr; ++i) {
		be_ut_tgf_group_read_check(&ctx, &groups[i], true);
		be_ut_tgf_group_fini(&ctx, &groups[i]);
	}

	for (i = 0; i < group_nr; ++i)
		be_ut_tgf_buf_fini(&ctx, &groups[i]);
	M0_BE_OP_SYNC(op, m0_be_log_discard_flush(&ctx.tgfc_log_discard, &op));
	m0_be_pd_fini(&ctx.tgfc_pd);
	m0_be_log_discard_fini(&ctx.tgfc_log_discard);
	be_ut_tgf_log_fini(&ctx);
	be_ut_tgf_seg_fini(&ctx);
}

static struct be_ut_tgf_tx be_ut_tgf_txs0[] = {
	{ .tgft_payload_size = 1024, .tgft_id = 1, },
};
static struct be_ut_tgf_reg be_ut_tgf_regs0[] = {
	{ .tgfr_size = 1024, },
	{ .tgfr_size = 1024, },
	{ .tgfr_size = 1024, },
};
static struct be_ut_tgf_tx be_ut_tgf_txs1[] = {
	{ .tgft_payload_size = 1024, .tgft_id = 1, },
	{ .tgft_payload_size = 513,  .tgft_id = 2, },
	{ .tgft_payload_size = 100,  .tgft_id = 3, },
};
static struct be_ut_tgf_reg be_ut_tgf_regs1[] = {
	{ .tgfr_size = 1024, },
	{ .tgfr_size = 1023, },
};
static struct be_ut_tgf_tx be_ut_tgf_txs2[] = {
	{ .tgft_payload_size = 1000, .tgft_id = 1ULL << 63, },
	{ .tgft_payload_size = 500,  .tgft_id = 1ULL << 62, },
};
static struct be_ut_tgf_reg be_ut_tgf_regs2[] = {
	{ .tgfr_size = 512,  },
	{ .tgfr_size = 100,  },
	{ .tgfr_size = 1000, },
};

static struct be_ut_tgf_group be_ut_tgf_groups[] = {
	{
		.tgfg_tx_nr  = ARRAY_SIZE(be_ut_tgf_txs0),
		.tgfg_txs    = be_ut_tgf_txs0,
		.tgfg_reg_nr = ARRAY_SIZE(be_ut_tgf_regs0),
		.tgfg_regs   = be_ut_tgf_regs0,
	},
	{
		.tgfg_tx_nr  = ARRAY_SIZE(be_ut_tgf_txs1),
		.tgfg_txs    = be_ut_tgf_txs1,
		.tgfg_reg_nr = ARRAY_SIZE(be_ut_tgf_regs1),
		.tgfg_regs   = be_ut_tgf_regs1,
	},
	{
		.tgfg_tx_nr  = ARRAY_SIZE(be_ut_tgf_txs2),
		.tgfg_txs    = be_ut_tgf_txs2,
		.tgfg_reg_nr = ARRAY_SIZE(be_ut_tgf_regs2),
		.tgfg_regs   = be_ut_tgf_regs2,
	},
};

void m0_be_ut_group_format(void)
{
	be_ut_tgf_test(ARRAY_SIZE(be_ut_tgf_groups), be_ut_tgf_groups);
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
