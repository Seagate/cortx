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
#include "be/fmt_xc.h"

#include "lib/buf.h"            /* m0_buf */
#include "lib/vec.h"            /* M0_BUFVEC_INIT_BUF */
#include "lib/misc.h"           /* container_of */
#include "lib/errno.h"          /* ENOMEM */
#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "xcode/xcode.h"        /* m0_xcode_ctx */

/**
 * @addtogroup be
 *
 * @{
 */

static void *disabled_xcode_alloc(struct m0_xcode_cursor *it, size_t nob)
{
	M0_IMPOSSIBLE("Xcode allocate shouldn't be called from be/fmt/decode");
}

static void disabled_xcode_free(struct m0_xcode_cursor *it)
{
	M0_IMPOSSIBLE("Xcode free shouldn't be called from be/fmt/decode");
}

static m0_bcount_t be_fmt_xcoded_size(struct m0_xcode_type *type,
				      void                 *object)
{
	struct m0_xcode_ctx ctx;
	struct m0_xcode_obj obj = M0_XCODE_OBJ(type, object);

	m0_xcode_ctx_init(&ctx, &obj);
	ctx.xcx_alloc = disabled_xcode_alloc;
	ctx.xcx_free = disabled_xcode_free;

	return m0_xcode_length(&ctx);
}

static int be_fmt_encode(struct m0_xcode_type    *type,
			 void                    *object,
			 struct m0_bufvec_cursor *cur)
{
	struct m0_xcode_ctx ctx;
	struct m0_xcode_obj obj = M0_XCODE_OBJ(type, object);
	int                 rc;

	m0_xcode_ctx_init(&ctx, &obj);

	ctx.xcx_alloc = disabled_xcode_alloc;
	ctx.xcx_free = disabled_xcode_free;

	ctx.xcx_buf = *cur;
	rc = m0_xcode_encode(&ctx);
	if (rc == 0)
		*cur = ctx.xcx_buf;

	return rc;
}

struct m0_fmt_xcode_ctx {
	struct m0_xcode_ctx                 ctx;
	const struct m0_be_fmt_decode_cfg  *cfg;
	int                                 err;
};

static void *be_fmt_alloc(struct m0_xcode_cursor *it, size_t nob)
{
	struct m0_xcode_ctx *xctx =
		container_of(it, struct m0_xcode_ctx, xcx_it);
	struct m0_fmt_xcode_ctx *fctx =
		container_of(xctx, struct m0_fmt_xcode_ctx, ctx);

	uint64_t group_size_max = fctx->cfg->dc_group_size_max;

	struct m0_xcode_cursor_frame *top = m0_xcode_cursor_top(it);
	bool err;

	fctx->err = 0;

	if (M0_IN(top->s_obj.xo_type, (m0_be_fmt_content_header_reg_xc,
				       m0_be_fmt_content_header_tx_xc,
				       m0_buf_xc)))
		err = nob < top->s_obj.xo_type->xct_sizeof ||
		      nob >= group_size_max;
	else if (M0_IN(top->s_obj.xo_type, (m0_be_fmt_cblock_xc,
					    m0_be_fmt_group_xc,
					    m0_be_fmt_log_header_xc,
					    m0_be_fmt_log_record_header_xc,
					    m0_be_fmt_log_record_footer_xc,
					    m0_be_fmt_log_store_header_xc)))
		err = nob != top->s_obj.xo_type->xct_sizeof ||
		      nob >= group_size_max ||
		      top->s_obj.xo_type->xct_sizeof >= group_size_max;
	else {
		err = nob >= group_size_max;
		fctx->err = EPROTO;
	}

	if (err)
		M0_LOG(M0_WARN, "type: %s, sizeof: %lu, nob: %lu",
		       top->s_obj.xo_type->xct_name,
		       top->s_obj.xo_type->xct_sizeof,
		       nob);

	return err ? NULL : m0_xcode_alloc(it, nob);
}

static int be_fmt_decode(struct m0_xcode_type               *type,
			 void                              **object,
			 struct m0_bufvec_cursor            *cur,
			 const struct m0_be_fmt_decode_cfg  *cfg)
{
	struct m0_fmt_xcode_ctx  fctx;
	struct m0_xcode_ctx     *ctx = &fctx.ctx;
	struct m0_xcode_obj      obj = M0_XCODE_OBJ(type, NULL);
	int                      rc;

	m0_xcode_ctx_init(ctx, &obj);
	fctx.cfg = cfg;
	ctx->xcx_iter     = cfg->dc_iter;
	ctx->xcx_iter_end = cfg->dc_iter_end;

	/*
	 * xcode doesn't want to use default m0_xcode_alloc() (m0_alloc())
	 * if this field is not set but it uses m0_free() if xcx_free is not
	 * set. It's just some dark magic. It will be fixed some day. I hope..
	 */
	ctx->xcx_alloc = be_fmt_alloc;

	ctx->xcx_buf = *cur;
	rc = m0_xcode_decode(ctx);

	if (rc == -ENOMEM)
		rc = -fctx.err;

	if (rc == 0)
		*cur = ctx->xcx_buf;

	*object = rc == 0 ? m0_xcode_ctx_top(ctx) : NULL;

	return rc;
}

static void be_fmt_decoded_free(struct m0_xcode_type *type,
				void                 *object)
{
	struct m0_xcode_obj obj = M0_XCODE_OBJ(type, object);

	m0_xcode_free_obj(&obj);
}

/* -------------------------------------------------------------------------- */

static bool m0_be_fmt_group__invariant(struct m0_be_fmt_group *fg)
{
	const struct m0_be_fmt_content_header_txs *ht =
		&fg->fg_content_header.fch_txs;
	const struct m0_be_fmt_content_payloads   *cp =
		&fg->fg_content.fmc_payloads;
	const struct m0_be_fmt_group_cfg          *cfg = (void *)fg->fg_cfg;

	return _0C(ht->cht_nr  < cfg->fgc_tx_nr_max) &&
		_0C(cp->fcp_nr < cfg->fgc_tx_nr_max) &&
		_0C(fg->fg_header.fgh_tx_nr < cfg->fgc_tx_nr_max) &&
		_0C(ht->cht_nr == cp->fcp_nr) &&
		_0C(ht->cht_nr == fg->fg_header.fgh_tx_nr);
}

static void be_fmt_content_bufs_init(struct m0_be_fmt_group *fg)
{
	const struct m0_be_fmt_group_cfg  *cfg = (void *)fg->fg_cfg;
	struct m0_be_fmt_content_payloads *payloads =
		&fg->fg_content.fmc_payloads;
	struct m0_be_fmt_content_reg_area *reg_area =
		&fg->fg_content.fmc_reg_area;
	int                               i;

	for (i = 0; i < payloads->fcp_nr; ++i)
		m0_buf_init(&payloads->fcp_payload[i], NULL,
			    cfg->fgc_payload_size_max);

	for (i = 0; i < reg_area->cra_nr; ++i)
		m0_buf_init(&reg_area->cra_reg[i], NULL,
			    cfg->fgc_reg_size_max);
}

M0_INTERNAL void m0_be_fmt_type_trace_end(const struct m0_xcode_cursor *it)
{
	M0_LOG(M0_DEBUG, "last");
}

M0_INTERNAL int m0_be_fmt_type_trace(const struct m0_xcode_cursor *it)
{
	struct m0_xcode_obj *obj;
	void                *ptr;
	m0_bcount_t          size;

	M0_PRE(it != NULL);

	obj  = &m0_xcode_cursor_top((struct m0_xcode_cursor *)it)->s_obj;
	ptr  = obj->xo_ptr;
	size = obj->xo_type->xct_sizeof;

	M0_LOG(M0_DEBUG, "name: %s, type:%d, depth:%d, ptr: %p, size: %lu",
	       obj->xo_type->xct_name,
	       obj->xo_type->xct_aggr,
	       it->xcu_depth,
	       ptr,
	       size);

	return 0;
}

M0_INTERNAL int m0_be_fmt_group_init(struct m0_be_fmt_group            *fg,
				     const struct m0_be_fmt_group_cfg  *cfg)
{
	struct m0_be_fmt_content_header *cheader = &fg->fg_content_header;
	struct m0_be_fmt_content        *content = &fg->fg_content;

	fg->fg_cfg = (uint64_t)cfg;

	cheader->fch_txs.cht_nr      = cfg->fgc_tx_nr_max;
	cheader->fch_reg_area.chr_nr = cfg->fgc_reg_nr_max;
	content->fmc_payloads.fcp_nr = cfg->fgc_tx_nr_max;
	content->fmc_reg_area.cra_nr = cfg->fgc_reg_nr_max;

	M0_PRE(cheader->fch_txs.cht_tx == NULL);
	M0_PRE(cheader->fch_reg_area.chr_reg == NULL);
	M0_PRE(content->fmc_payloads.fcp_payload == NULL);
	M0_PRE(content->fmc_reg_area.cra_reg == NULL);

	M0_ALLOC_ARR(cheader->fch_txs.cht_tx, cfg->fgc_tx_nr_max);
	if (cheader->fch_txs.cht_tx == NULL)
		goto enomem;

	M0_ALLOC_ARR(cheader->fch_reg_area.chr_reg, cfg->fgc_reg_nr_max);
	if (cheader->fch_reg_area.chr_reg == NULL)
		goto enomem;

	M0_ALLOC_ARR(content->fmc_payloads.fcp_payload, cfg->fgc_tx_nr_max);
	if (content->fmc_payloads.fcp_payload == NULL)
		goto enomem;

	M0_ALLOC_ARR(content->fmc_reg_area.cra_reg, cfg->fgc_reg_nr_max);
	if (content->fmc_reg_area.cra_reg == NULL)
		goto enomem;

	be_fmt_content_bufs_init(fg);

	cheader->fch_txs.cht_nr      = 0;
	cheader->fch_reg_area.chr_nr = 0;
	content->fmc_payloads.fcp_nr = 0;
	content->fmc_reg_area.cra_nr = 0;
	fg->fg_header.fgh_reg_nr     = 0;
	fg->fg_header.fgh_tx_nr      = 0;

	return 0;
enomem:
	m0_be_fmt_group_fini(fg);
	return -ENOMEM;
}

M0_INTERNAL void m0_be_fmt_group_fini(struct m0_be_fmt_group *fg)
{
	struct m0_be_fmt_content_header *cheader = &fg->fg_content_header;
	struct m0_be_fmt_content        *content = &fg->fg_content;

	m0_free(content->fmc_reg_area.cra_reg);
	m0_free(content->fmc_payloads.fcp_payload);
	m0_free(cheader->fch_reg_area.chr_reg);
	m0_free(cheader->fch_txs.cht_tx);
}

M0_INTERNAL void m0_be_fmt_group_reset(struct m0_be_fmt_group *fg)
{
	struct m0_be_fmt_content_header  *cheader = &fg->fg_content_header;
	struct m0_be_fmt_content         *content = &fg->fg_content;

	cheader->fch_txs.cht_nr      = 0;
	cheader->fch_reg_area.chr_nr = 0;
	content->fmc_payloads.fcp_nr = 0;
	content->fmc_reg_area.cra_nr = 0;
	fg->fg_header.fgh_reg_nr     = 0;
	fg->fg_header.fgh_tx_nr      = 0;


	M0_ASSERT(cheader->fch_txs.cht_tx != NULL);
	M0_ASSERT(cheader->fch_reg_area.chr_reg != NULL);
	M0_ASSERT(content->fmc_payloads.fcp_payload != NULL);
	M0_ASSERT(content->fmc_reg_area.cra_reg != NULL);

	be_fmt_content_bufs_init(fg);
}

M0_INTERNAL m0_bcount_t m0_be_fmt_group_size(struct m0_be_fmt_group *fg)
{
	return be_fmt_xcoded_size(m0_be_fmt_group_xc, fg);
}

M0_INTERNAL m0_bcount_t
m0_be_fmt_group_size_max(const struct m0_be_fmt_group_cfg *cfg)
{
	return sizeof(struct m0_be_fmt_group_header) +
	       /* m0_be_fmt_content_header */
	       M0_MEMBER_SIZE(struct m0_be_fmt_content_header_txs, cht_nr) +
	       sizeof(struct m0_be_fmt_content_header_tx) *
					cfg->fgc_tx_nr_max +
	       M0_MEMBER_SIZE(struct m0_be_fmt_content_header_reg_area,
			      chr_nr) +
	       sizeof(struct m0_be_fmt_content_header_reg) *
					cfg->fgc_reg_nr_max +
	       /* m0_be_fmt_content */
	       M0_MEMBER_SIZE(struct m0_be_fmt_content_payloads, fcp_nr) +
	       (sizeof(struct m0_buf) - M0_MEMBER_SIZE(struct m0_buf, b_addr)) *
					cfg->fgc_tx_nr_max +
	       cfg->fgc_payload_size_max +
	       M0_MEMBER_SIZE(struct m0_be_fmt_content_reg_area, cra_nr) +
	       (sizeof(struct m0_buf) - M0_MEMBER_SIZE(struct m0_buf, b_addr)) *
					cfg->fgc_reg_nr_max +
	       cfg->fgc_reg_size_max +
	       M0_MEMBER_SIZE(struct m0_be_fmt_group, fg_cfg);
}

M0_INTERNAL int m0_be_fmt_group_encode(struct m0_be_fmt_group  *fg,
				       struct m0_bufvec_cursor *cur)
{
	fg->fg_header.fgh_size = m0_be_fmt_group_size(fg);

	return be_fmt_encode(m0_be_fmt_group_xc, fg, cur);
}

M0_INTERNAL int m0_be_fmt_group_decode(struct m0_be_fmt_group            **fg,
				       struct m0_bufvec_cursor            *cur,
				       const struct m0_be_fmt_decode_cfg  *cfg)
{
	return be_fmt_decode(m0_be_fmt_group_xc, (void **)fg, cur, cfg);
}

M0_INTERNAL void m0_be_fmt_group_decoded_free(struct m0_be_fmt_group *fg)
{
	be_fmt_decoded_free(m0_be_fmt_group_xc, fg);
}

M0_INTERNAL void m0_be_fmt_group_reg_add(struct m0_be_fmt_group     *fg,
					 const struct m0_be_fmt_reg *freg)
{
	struct m0_be_fmt_content_header_reg_area *hra;
	struct m0_be_fmt_content_reg_area        *ra;
	const struct m0_be_fmt_group_cfg         *cfg = (void *)fg->fg_cfg;

	ra  = &fg->fg_content.fmc_reg_area;
	hra = &fg->fg_content_header.fch_reg_area;

	M0_ASSERT(ra->cra_nr  < cfg->fgc_reg_nr_max);
	M0_ASSERT(hra->chr_nr < cfg->fgc_reg_nr_max);
	M0_ASSERT(fg->fg_header.fgh_reg_nr < cfg->fgc_reg_nr_max);
	M0_ASSERT(hra->chr_nr == ra->cra_nr);
	M0_ASSERT(hra->chr_nr == fg->fg_header.fgh_reg_nr);
	M0_ASSERT(freg->fr_size <= cfg->fgc_reg_size_max);

	ra->cra_reg[ra->cra_nr++] = M0_BUF_INIT(freg->fr_size, freg->fr_buf);
	hra->chr_reg[hra->chr_nr++] = (struct m0_be_fmt_content_header_reg) {
					.chg_size = freg->fr_size,
					.chg_addr = (uint64_t) freg->fr_addr,
					};
	fg->fg_header.fgh_reg_nr++;
}

M0_INTERNAL uint32_t m0_be_fmt_group_reg_nr(const struct m0_be_fmt_group *fg)
{
	return fg->fg_header.fgh_reg_nr;
}

M0_INTERNAL void m0_be_fmt_group_reg_by_id(const struct m0_be_fmt_group *fg,
					   uint32_t                      index,
					   struct m0_be_fmt_reg         *freg)
{
	const struct m0_be_fmt_content_header_reg_area *hra;
	const struct m0_be_fmt_content_reg_area        *ra;

	M0_PRE(index < m0_be_fmt_group_reg_nr(fg));

	ra  = &fg->fg_content.fmc_reg_area;
	hra = &fg->fg_content_header.fch_reg_area;

	*freg = M0_BE_FMT_REG(hra->chr_reg[index].chg_size,
			      (void *)hra->chr_reg[index].chg_addr,
			      ra->cra_reg[index].b_addr);
}

M0_INTERNAL void m0_be_fmt_group_tx_add(struct m0_be_fmt_group    *fg,
					const struct m0_be_fmt_tx *ftx)
{
	struct m0_be_fmt_content_header_txs *ht;
	struct m0_be_fmt_content_payloads   *cp;
	const struct m0_be_fmt_group_cfg    *cfg = (void *)fg->fg_cfg;

	ht = &fg->fg_content_header.fch_txs;
	cp = &fg->fg_content.fmc_payloads;

	M0_ASSERT(m0_be_fmt_group__invariant(fg));
	M0_ASSERT(m0_reduce(i, ht->cht_nr, 0,
			    + ht->cht_tx[i].chx_payload_size) +
	          ftx->bft_payload.b_nob <= cfg->fgc_payload_size_max);

	ht->cht_tx[ht->cht_nr++]  = (struct m0_be_fmt_content_header_tx) {
		.chx_tx_id        = ftx->bft_id,
		.chx_payload_size = ftx->bft_payload.b_nob,
	};

	cp->fcp_payload[cp->fcp_nr++] = ftx->bft_payload;
	fg->fg_header.fgh_tx_nr++;
}

M0_INTERNAL uint32_t m0_be_fmt_group_tx_nr(const struct m0_be_fmt_group *fg)
{
	return fg->fg_header.fgh_tx_nr;
}

M0_INTERNAL void m0_be_fmt_group_tx_by_id(const struct m0_be_fmt_group *fg,
					  uint32_t                      index,
					  struct m0_be_fmt_tx          *ftx)
{
	const struct m0_be_fmt_content_header_txs *ht;
	const struct m0_be_fmt_content_payloads   *cp;

	M0_PRE(index < m0_be_fmt_group_tx_nr(fg));

	ht = &fg->fg_content_header.fch_txs;
	cp = &fg->fg_content.fmc_payloads;

	*ftx = M0_BE_FMT_TX(cp->fcp_payload[index],
			    ht->cht_tx[index].chx_tx_id);
}

M0_INTERNAL struct m0_be_fmt_group_info *
m0_be_fmt_group_info_get(struct m0_be_fmt_group *fg)
{
	return &fg->fg_header.fgh_info;
}

M0_INTERNAL bool m0_be_fmt_group_sanity_check(struct m0_be_fmt_group *fg)
{
	const struct m0_be_fmt_group_cfg              *cfg = (void *)fg->fg_cfg;
	const struct m0_be_fmt_content_header_txs      *ht;
	const struct m0_be_fmt_content_payloads        *cp;
	const struct m0_be_fmt_content_reg_area        *ra;
	const struct m0_be_fmt_content_header_reg_area *hra;
	const struct m0_be_fmt_content_header          *cheader;
	const struct m0_be_fmt_content                 *content;


	cheader = &fg->fg_content_header;
	content = &fg->fg_content;

	ra      = &fg->fg_content.fmc_reg_area;
	hra     = &fg->fg_content_header.fch_reg_area;

	cp      = &fg->fg_content.fmc_payloads;
	ht      = &fg->fg_content_header.fch_txs;

	return _0C(ra->cra_nr  < cfg->fgc_reg_nr_max) &&
		_0C(hra->chr_nr < cfg->fgc_reg_nr_max) &&
		_0C(fg->fg_header.fgh_reg_nr < cfg->fgc_reg_nr_max) &&
		_0C(hra->chr_nr == ra->cra_nr) &&
		_0C(hra->chr_nr == fg->fg_header.fgh_reg_nr) &&
		_0C(cheader->fch_txs.cht_tx != NULL) &&
		_0C(cheader->fch_reg_area.chr_reg != NULL) &&
		_0C(content->fmc_payloads.fcp_payload != NULL) &&
		_0C(content->fmc_reg_area.cra_reg != NULL) &&
		_0C(ht->cht_nr  < cfg->fgc_tx_nr_max) &&
		_0C(cp->fcp_nr < cfg->fgc_tx_nr_max) &&
		_0C(fg->fg_header.fgh_tx_nr < cfg->fgc_tx_nr_max) &&
		_0C(ht->cht_nr == cp->fcp_nr) &&
		_0C(ht->cht_nr == fg->fg_header.fgh_tx_nr);
}

/* -------------------------------------------------------------------------- */

#define M0_BE_FMT_DEFINE_INIT_SIMPLE(name)                              \
M0_INTERNAL int                                                         \
m0_be_fmt_##name##_init(struct m0_be_fmt_##name             *obj,       \
			const struct m0_be_fmt_##name##_cfg *cfg)       \
{                                                                       \
	return 0;                                                       \
}                                                                       \
M0_INTERNAL void m0_be_fmt_##name##_fini(struct m0_be_fmt_##name *obj)  \
{}                                                                      \
M0_INTERNAL void m0_be_fmt_##name##_reset(struct m0_be_fmt_##name *obj) \
{}                                                                      \
M0_INTERNAL m0_bcount_t                                                 \
m0_be_fmt_##name##_size_max(const struct m0_be_fmt_##name##_cfg *cfg)   \
{                                                                       \
	return m0_be_fmt_##name##_size(NULL);                           \
}

#define M0_BE_FMT_DEFINE_XCODE(name)                                           \
M0_INTERNAL m0_bcount_t m0_be_fmt_##name##_size(struct m0_be_fmt_##name *obj)  \
{                                                                              \
	return be_fmt_xcoded_size(m0_be_fmt_##name##_xc, obj);                 \
}                                                                              \
M0_INTERNAL int m0_be_fmt_##name##_encode(struct m0_be_fmt_##name *obj,        \
					  struct m0_bufvec_cursor *cur)        \
{                                                                              \
	return be_fmt_encode(m0_be_fmt_##name##_xc, obj, cur);                 \
}                                                                              \
M0_INTERNAL int m0_be_fmt_##name##_encode_buf(struct m0_be_fmt_##name  *obj,   \
					      struct m0_buf            *buf)   \
{                                                                              \
	struct m0_bufvec         bvec;                                         \
	struct m0_bufvec_cursor  cur;                                          \
									       \
	bvec = M0_BUFVEC_INIT_BUF(&buf->b_addr, &buf->b_nob);                  \
	m0_bufvec_cursor_init(&cur, &bvec);                                    \
	return m0_be_fmt_##name##_encode(obj, &cur);                           \
}                                                                              \
M0_INTERNAL int                                                                \
m0_be_fmt_##name##_decode(struct m0_be_fmt_##name           **obj,             \
			  struct m0_bufvec_cursor            *cur,             \
			  const struct m0_be_fmt_decode_cfg  *cfg)             \
{                                                                              \
	return be_fmt_decode(m0_be_fmt_##name##_xc, (void **)obj, cur, cfg);   \
}                                                                              \
M0_INTERNAL int                                                                \
m0_be_fmt_##name##_decode_buf(struct m0_be_fmt_##name          **obj,          \
			      struct m0_buf                     *buf,          \
			      const struct m0_be_fmt_decode_cfg *cfg)          \
{                                                                              \
	struct m0_bufvec         bvec;                                         \
	struct m0_bufvec_cursor  cur;                                          \
									       \
	bvec = M0_BUFVEC_INIT_BUF(&buf->b_addr, &buf->b_nob);                  \
	m0_bufvec_cursor_init(&cur, &bvec);                                    \
	return m0_be_fmt_##name##_decode(obj, &cur, cfg);                      \
}                                                                              \
M0_INTERNAL void m0_be_fmt_##name##_decoded_free(struct m0_be_fmt_##name *obj) \
{                                                                              \
	be_fmt_decoded_free(m0_be_fmt_##name##_xc, obj);                       \
}

/* -------------------------------------------------------------------------- */

M0_INTERNAL int
m0_be_fmt_log_record_header_init(struct m0_be_fmt_log_record_header           *obj,
				 const struct m0_be_fmt_log_record_header_cfg *cfg)
{
	obj->lrh_io_nr_max       = cfg->lrhc_io_nr_max;
	obj->lrh_io_size.lrhs_nr = 0;
	M0_ALLOC_ARR(obj->lrh_io_size.lrhs_size, cfg->lrhc_io_nr_max);

	return obj->lrh_io_size.lrhs_size == NULL ? -ENOMEM : 0;
}

M0_INTERNAL void
m0_be_fmt_log_record_header_fini(struct m0_be_fmt_log_record_header *obj)
{
	m0_free(obj->lrh_io_size.lrhs_size);
}

M0_INTERNAL void
m0_be_fmt_log_record_header_reset(struct m0_be_fmt_log_record_header *obj)
{
	obj->lrh_io_size.lrhs_nr = 0;
}

M0_INTERNAL m0_bcount_t m0_be_fmt_log_record_header_size_max(
			      const struct m0_be_fmt_log_record_header_cfg *cfg)
{
	struct m0_be_fmt_log_record_header obj = {
		.lrh_io_size = {
			.lrhs_nr = cfg->lrhc_io_nr_max,
		},
	};

	return m0_be_fmt_log_record_header_size(&obj);
}

M0_INTERNAL void
m0_be_fmt_log_record_header_io_size_add(struct m0_be_fmt_log_record_header *obj,
					m0_bcount_t                        size)
{
	struct m0_be_fmt_log_record_header_io_size *io_size = &obj->lrh_io_size;

	M0_PRE(io_size->lrhs_nr < obj->lrh_io_nr_max);
	io_size->lrhs_size[io_size->lrhs_nr++] = size;
}

M0_BE_FMT_DEFINE_XCODE(log_record_header);

/* -------------------------------------------------------------------------- */

M0_BE_FMT_DEFINE_INIT_SIMPLE(log_record_footer);
M0_BE_FMT_DEFINE_XCODE(log_record_footer);

M0_BE_FMT_DEFINE_INIT_SIMPLE(log_store_header);
M0_BE_FMT_DEFINE_XCODE(log_store_header);

M0_BE_FMT_DEFINE_INIT_SIMPLE(cblock);
M0_BE_FMT_DEFINE_XCODE(cblock);

M0_BE_FMT_DEFINE_INIT_SIMPLE(log_header);
M0_BE_FMT_DEFINE_XCODE(log_header);

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
