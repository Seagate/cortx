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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/tx_group_format.h"
#include "be/log.h"
#include "be/tx_regmap.h"    /* m0_be_reg_area_used */
#include "be/tx_internal.h"  /* m0_be_tx__reg_area */
#include "be/engine.h"       /* m0_be_engine__reg_area_tagged_remove */
#include "be/pd.h"           /* m0_be_pd_io */
#include "be/log_discard.h"  /* m0_be_log_discard_item */
#include "be/domain.h"       /* m0_be_domain_log */
#include "lib/memory.h"      /* m0_alloc */
#include "lib/misc.h"        /* M0_SET0 */
#include "lib/errno.h"       /* ENOMEM */

#include "module/instance.h" /* m0_get */

/**
 * @addtogroup be
 *
 * @{
 */

#define gft_fmt_group_choose(gft) (gft->gft_fmt_group_decoded != NULL ? \
				   gft->gft_fmt_group_decoded :         \
				   &gft->gft_fmt_group)

enum {
	GFT_GROUP_IO    = 0,
	GFT_GROUP_CB_IO = 1,
};

static struct m0_be_group_format *
be_group_format_module2gft(struct m0_module *module)
{
	/* XXX bob_of */
	return container_of(module, struct m0_be_group_format, gft_module);
}

static int be_group_format_level_enter(struct m0_module *module)
{
	struct m0_be_group_format  *gft = be_group_format_module2gft(module);
	m0_bcount_t                 size_group;
	m0_bcount_t                 size_cblock;
	int                         level = module->m_cur + 1;

	M0_LOG(M0_DEBUG, "gft=%p level=%d", gft, level);

	switch (level) {
	case M0_BE_GROUP_FORMAT_LEVEL_ASSIGNS:
		gft->gft_log                = gft->gft_cfg.gfc_log;
		gft->gft_fmt_group_decoded  = NULL;
		gft->gft_fmt_cblock_decoded = NULL;
		return 0;
	case M0_BE_GROUP_FORMAT_LEVEL_OP_INIT:
		m0_be_op_init(&gft->gft_pd_io_get);
		m0_be_op_init(&gft->gft_log_discard_get);
		m0_be_op_init(&gft->gft_all_get);
		return 0;
	case M0_BE_GROUP_FORMAT_LEVEL_FMT_GROUP_INIT:
		return m0_be_fmt_group_init(&gft->gft_fmt_group,
					    &gft->gft_cfg.gfc_fmt_cfg);
	case M0_BE_GROUP_FORMAT_LEVEL_FMT_CBLOCK_INIT:
		return m0_be_fmt_cblock_init(&gft->gft_fmt_cblock, NULL);
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_INIT:
		m0_be_log_record_init(&gft->gft_log_record, gft->gft_log);
		return 0;
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_GROUP:
		size_group = m0_be_fmt_group_size_max(&gft->gft_cfg.gfc_fmt_cfg);
		return m0_be_log_record_io_create(&gft->gft_log_record,
						  size_group);
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_CBLOCK:
		size_cblock = m0_be_fmt_cblock_size_max(NULL);
		return m0_be_log_record_io_create(&gft->gft_log_record,
						  size_cblock);
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ITER_INIT:
		return m0_be_log_record_iter_init(&gft->gft_log_record_iter);
	case M0_BE_GROUP_FORMAT_LEVEL_INITED:
		return 0;
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ALLOCATE:
		return m0_be_log_record_allocate(&gft->gft_log_record);
	case M0_BE_GROUP_FORMAT_LEVEL_ALLOCATED:
		return 0;
	default:
		return M0_ERR(-ENOSYS);
	}
}

static void be_group_format_level_leave(struct m0_module *module)
{
	struct m0_be_group_format *gft = be_group_format_module2gft(module);
	int                        level = module->m_cur;

	M0_LOG(M0_DEBUG, "gft=%p level=%d", gft, level);

	switch (level) {
	case M0_BE_GROUP_FORMAT_LEVEL_ASSIGNS:
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_OP_INIT:
		m0_be_op_fini(&gft->gft_all_get);
		m0_be_op_fini(&gft->gft_log_discard_get);
		m0_be_op_fini(&gft->gft_pd_io_get);
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_FMT_GROUP_INIT:
		m0_be_fmt_group_fini(&gft->gft_fmt_group);
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_FMT_CBLOCK_INIT:
		m0_be_fmt_cblock_fini(&gft->gft_fmt_cblock);
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_INIT:
		m0_be_log_record_fini(&gft->gft_log_record);
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_GROUP:
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_CBLOCK:
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ITER_INIT:
		m0_be_log_record_iter_fini(&gft->gft_log_record_iter);
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_INITED:
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ALLOCATE:
		m0_be_log_record_deallocate(&gft->gft_log_record);
		break;
	case M0_BE_GROUP_FORMAT_LEVEL_ALLOCATED:
		if (gft->gft_fmt_group_decoded != NULL) {
			m0_be_fmt_group_decoded_free(gft->gft_fmt_group_decoded);
		}
		if (gft->gft_fmt_cblock_decoded != NULL) {
			m0_be_fmt_cblock_decoded_free(
						gft->gft_fmt_cblock_decoded);
		}
		if (gft->gft_pd_io != NULL)
			m0_be_pd_io_put(gft->gft_cfg.gfc_pd, gft->gft_pd_io);
		break;
	default:
		M0_IMPOSSIBLE("Unexpected m0_module level");
	}
}

M0_INTERNAL void m0_be_group_format_reset(struct m0_be_group_format *gft)
{
	m0_be_fmt_group_reset(&gft->gft_fmt_group);
	m0_be_fmt_cblock_reset(&gft->gft_fmt_cblock);
	m0_be_log_record_reset(&gft->gft_log_record);
	if (gft->gft_pd_io != NULL) {
		m0_be_pd_io_put(gft->gft_cfg.gfc_pd, gft->gft_pd_io);
		gft->gft_pd_io = NULL;
	}
	m0_be_op_reset(&gft->gft_pd_io_get);
	m0_be_op_reset(&gft->gft_log_discard_get);
	m0_be_op_reset(&gft->gft_all_get);

	if (gft->gft_fmt_group_decoded != NULL) {
		m0_time_t time = m0_time_now();
		m0_be_fmt_group_decoded_free(gft->gft_fmt_group_decoded);
		M0_LOG(M0_DEBUG, "time=%lu",
		       (unsigned long)m0_time_now() - time);
		gft->gft_fmt_group_decoded = NULL;
	}
	if (gft->gft_fmt_cblock_decoded != NULL) {
		m0_be_fmt_cblock_decoded_free(
				gft->gft_fmt_cblock_decoded);
		gft->gft_fmt_cblock_decoded = NULL;
	}
}

static const struct m0_modlev be_group_format_levels[] = {
	[M0_BE_GROUP_FORMAT_LEVEL_ASSIGNS] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_ASSIGNS",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_OP_INIT] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_OP_INIT",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_FMT_GROUP_INIT] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_FMT_GROUP_INIT",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_FMT_CBLOCK_INIT] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_FMT_CBLOCK_INIT",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_INIT] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_INIT",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_GROUP] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_GROUP",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_CBLOCK] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_CBLOCK",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ITER_INIT] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ITER_INIT",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_INITED] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_INITED",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ALLOCATE] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ALLOCATE",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
	[M0_BE_GROUP_FORMAT_LEVEL_ALLOCATED] = {
		.ml_name  = "M0_BE_GROUP_FORMAT_LEVEL_ALLOCATED",
		.ml_enter = be_group_format_level_enter,
		.ml_leave = be_group_format_level_leave,
	},
};

M0_INTERNAL void
m0_be_group_format_module_setup(struct m0_be_group_format     *gft,
				struct m0_be_group_format_cfg *gft_cfg)
{
	gft->gft_cfg = *gft_cfg;
	m0_module_setup(&gft->gft_module, "m0_be_group_format",
			be_group_format_levels,
			ARRAY_SIZE(be_group_format_levels),
			m0_get());
}

static void be_group_format_module_fini(struct m0_be_group_format *gft,
					bool                       deallocate)
{
	m0_module_fini(&gft->gft_module,
		       deallocate ? M0_BE_GROUP_FORMAT_LEVEL_INITED :
				    M0_MODLEV_NONE);
}

static int be_group_format_module_init(struct m0_be_group_format     *gft,
				       struct m0_be_group_format_cfg *gft_cfg,
				       bool                           allocate)
{
	int rc;

	if (!allocate)
		m0_be_group_format_module_setup(gft, gft_cfg);
	rc = m0_module_init(&gft->gft_module,
			    allocate ? M0_BE_GROUP_FORMAT_LEVEL_ALLOCATED :
				       M0_BE_GROUP_FORMAT_LEVEL_INITED);
	if (rc != 0)
		be_group_format_module_fini(gft, allocate);
	return rc;
}

M0_INTERNAL int m0_be_group_format_init(struct m0_be_group_format     *gft,
					struct m0_be_group_format_cfg *gft_cfg,
					struct m0_be_tx_group         *group,
					struct m0_be_log              *log)
{
	gft_cfg->gfc_log = log;
	return be_group_format_module_init(gft, gft_cfg, false);
}

M0_INTERNAL void m0_be_group_format_fini(struct m0_be_group_format *gft)
{
	 be_group_format_module_fini(gft, false);
}

M0_INTERNAL bool m0_be_group_format__invariant(struct m0_be_group_format *go)
{
	return true; /* XXX TODO */
}

M0_INTERNAL int m0_be_group_format_allocate(struct m0_be_group_format *gft)
{
	return be_group_format_module_init(gft, NULL, true);
}

M0_INTERNAL void m0_be_group_format_deallocate(struct m0_be_group_format *gft)
{
	 be_group_format_module_fini(gft, true);
}

M0_INTERNAL void m0_be_group_format_prepare(struct m0_be_group_format *gft,
                                            struct m0_be_op           *op)
{
	m0_be_op_set_add(op, &gft->gft_pd_io_get);
	m0_be_op_set_add(op, &gft->gft_log_discard_get);
	m0_be_op_set_add(op, &gft->gft_all_get);
	m0_be_op_active(&gft->gft_all_get);
	m0_be_log_discard_item_get(gft->gft_cfg.gfc_log_discard,
	                           &gft->gft_log_discard_get,
	                           &gft->gft_log_discard_item);
	m0_be_pd_io_get(gft->gft_cfg.gfc_pd, &gft->gft_pd_io,
	                &gft->gft_pd_io_get);
	m0_be_op_done(&gft->gft_all_get);
}

M0_INTERNAL void m0_be_group_format_encode(struct m0_be_group_format *gft)
{
	struct m0_bufvec_cursor  cur;
	struct m0_bufvec        *bvec;
	int                      rc;

	bvec = m0_be_log_record_io_bufvec(&gft->gft_log_record, GFT_GROUP_IO);
	m0_bufvec_cursor_init(&cur, bvec);
	rc = m0_be_fmt_group_encode(&gft->gft_fmt_group, &cur);
	M0_ASSERT_INFO(rc == 0, "due to preallocated buffers "
		       "encode can't fail here: rc = %d", rc);
	bvec = m0_be_log_record_io_bufvec(&gft->gft_log_record, GFT_GROUP_CB_IO);
	m0_bufvec_cursor_init(&cur, bvec);
	rc = m0_be_fmt_cblock_encode(&gft->gft_fmt_cblock, &cur);
	M0_ASSERT_INFO(rc == 0, "due to preallocated buffers "
		       "encode can't fail here: rc = %d", rc);
}

M0_INTERNAL int m0_be_group_format_decode(struct m0_be_group_format *gft)
{
	struct m0_bufvec_cursor  cur;
	struct m0_bufvec        *bvec;
	int                      rc;

	M0_PRE(gft->gft_fmt_group_decoded == NULL);
	M0_PRE(gft->gft_fmt_cblock_decoded == NULL);

	bvec = m0_be_log_record_io_bufvec(&gft->gft_log_record, GFT_GROUP_IO);
	m0_bufvec_cursor_init(&cur, bvec);
	rc = m0_be_fmt_group_decode(&gft->gft_fmt_group_decoded, &cur,
				    M0_BE_FMT_DECODE_CFG_DEFAULT);

	bvec = m0_be_log_record_io_bufvec(&gft->gft_log_record, GFT_GROUP_CB_IO);
	m0_bufvec_cursor_init(&cur, bvec);
	rc = rc ?: m0_be_fmt_cblock_decode(&gft->gft_fmt_cblock_decoded,
					   &cur, M0_BE_FMT_DECODE_CFG_DEFAULT);
	return rc;
}

M0_INTERNAL void m0_be_group_format_reg_log_add(struct m0_be_group_format *gft,
						const struct m0_be_reg_d  *rd)
{
	struct m0_be_fmt_reg freg;

	freg = M0_BE_FMT_REG(rd->rd_reg.br_size, rd->rd_reg.br_addr,
			     rd->rd_buf);
	m0_be_fmt_group_reg_add(&gft->gft_fmt_group, &freg);
}

M0_INTERNAL void m0_be_group_format_reg_seg_add(struct m0_be_group_format *gft,
						const struct m0_be_reg_d  *rd)
{
	m0_be_io_add(m0_be_pd_io_be_io(gft->gft_pd_io),
		     rd->rd_reg.br_seg->bs_stob, rd->rd_buf,
		     m0_be_reg_offset(&rd->rd_reg), rd->rd_reg.br_size);
}

M0_INTERNAL uint32_t
m0_be_group_format_reg_nr(const struct m0_be_group_format *gft)
{
	return m0_be_fmt_group_reg_nr(gft_fmt_group_choose(gft));
}

M0_INTERNAL void
m0_be_group_format_reg_get(const struct m0_be_group_format *gft,
			   uint32_t			    index,
			   struct m0_be_reg_d		   *rd)
{
	struct m0_be_fmt_reg freg;

	M0_PRE(gft->gft_fmt_group_decoded != NULL);

	m0_be_fmt_group_reg_by_id(gft->gft_fmt_group_decoded, index, &freg);
	/*
	 * XXX currently segment field in region is always NULL
	 * after group reconstruct. It may be changed in the future.
	 */
	*rd = M0_BE_REG_D(M0_BE_REG(NULL, freg.fr_size, freg.fr_addr),
			  freg.fr_buf);
}

M0_INTERNAL void m0_be_group_format_tx_add(struct m0_be_group_format *gft,
					   struct m0_be_fmt_tx       *ftx)
{
	m0_be_fmt_group_tx_add(&gft->gft_fmt_group, ftx);
}

M0_INTERNAL uint32_t
m0_be_group_format_tx_nr(const struct m0_be_group_format *gft)
{
	return m0_be_fmt_group_tx_nr(gft_fmt_group_choose(gft));
}

M0_INTERNAL void
m0_be_group_format_tx_get(const struct m0_be_group_format *gft,
			  uint32_t                         index,
			  struct m0_be_fmt_tx             *ftx)
{
	M0_PRE(gft->gft_fmt_group_decoded != NULL);

	m0_be_fmt_group_tx_by_id(gft->gft_fmt_group_decoded, index, ftx);
}

M0_INTERNAL struct m0_be_fmt_group_info *
m0_be_group_format_group_info(struct m0_be_group_format *gft)
{
	return m0_be_fmt_group_info_get(gft_fmt_group_choose(gft));
}

M0_INTERNAL m0_bcount_t
m0_be_group_format_log_reserved_size(struct m0_be_log       *log,
				     struct m0_be_tx_credit *cred,
				     m0_bcount_t             cred_payload)
{
	m0_bcount_t                lio_size[2];
	struct m0_be_fmt_group_cfg cfg = {
		.fgc_tx_nr_max = 1,
		.fgc_reg_nr_max = cred->tc_reg_nr,
		.fgc_payload_size_max = cred_payload,
		.fgc_reg_size_max = cred->tc_reg_size,
	};

	lio_size[0] = m0_be_fmt_group_size_max(&cfg);
	lio_size[1] = m0_be_fmt_cblock_size_max(NULL);
	return m0_be_log_reserved_size(log, lio_size, 2);
}

M0_INTERNAL void
m0_be_group_format_log_use(struct m0_be_group_format *gft,
			   m0_bcount_t                size_reserved)
{
	struct m0_be_log_record *record = &gft->gft_log_record;
	m0_bcount_t              size_group;
	m0_bcount_t              size_cblock;

	size_group  = m0_be_fmt_group_size(&gft->gft_fmt_group);
	size_cblock = m0_be_fmt_cblock_size(&gft->gft_fmt_cblock);

	M0_LOG(M0_DEBUG, "size_reserved=%lu size_group=%lu size_cblock=%lu",
	       size_reserved, size_group, size_cblock);

	m0_be_log_record_io_size_set(record, GFT_GROUP_IO, size_group);
	m0_be_log_record_io_size_set(record, GFT_GROUP_CB_IO, size_cblock);
	m0_be_log_record_io_prepare(record, SIO_WRITE, size_reserved);
}

M0_INTERNAL void
m0_be_group_format_recovery_prepare(struct m0_be_group_format *gft,
				    struct m0_be_log          *log)
{
	struct m0_be_log_record *record = &gft->gft_log_record;

	m0_be_log_recovery_record_get(log, &gft->gft_log_record_iter);
	m0_be_log_record_assign(record, &gft->gft_log_record_iter, true);
	m0_be_log_record_io_prepare(record, SIO_READ, 0);
}

M0_INTERNAL void m0_be_group_format_log_write(struct m0_be_group_format *gft,
					      struct m0_be_op           *op)
{
	/* XXX @pre SIO_WRITE is set */
	m0_be_log_record_io_launch(&gft->gft_log_record, op);
}

M0_INTERNAL void m0_be_group_format_log_read(struct m0_be_group_format *gft,
					     struct m0_be_op           *op)
{
	m0_be_log_record_io_launch(&gft->gft_log_record, op);
}

M0_INTERNAL void
m0_be_group_format_seg_place_prepare(struct m0_be_group_format *gft)
{
	/*
	 * Regions are added to m0_be_io for seg I/O in
	 * m0_be_group_format_reg_seg_add().
	 */
	m0_be_log_record_ext(&gft->gft_log_record, &gft->gft_ext);
	m0_be_io_configure(m0_be_pd_io_be_io(gft->gft_pd_io), SIO_WRITE);
	m0_be_log_discard_item_ext_set(gft->gft_log_discard_item,
				       &gft->gft_ext);
	m0_be_log_discard_item_user_data_set(gft->gft_log_discard_item,
					     gft->gft_cfg.gfc_log);
}

static void be_tx_group_format_seg_io_starting(struct m0_be_op *op, void *param)
{
	struct m0_be_group_format *tgf = param;

	m0_be_log_discard_item_starting(tgf->gft_cfg.gfc_log_discard,
	                                tgf->gft_log_discard_item);
}

static void be_tx_group_format_seg_io_finished(struct m0_be_op *op, void *param)
{
	struct m0_be_group_format *tgf = param;

	m0_be_log_discard_item_finished(tgf->gft_cfg.gfc_log_discard,
	                                tgf->gft_log_discard_item);
}

static void be_tx_group_format_seg_io_op_gc(struct m0_be_op *op, void *param)
{
	struct m0_be_group_format *gft = param;

	m0_be_op_done(&gft->gft_tmp_op);
	m0_be_op_fini(&gft->gft_tmp_op);
	m0_be_op_fini(op);
}

M0_INTERNAL void m0_be_group_format_discard(struct m0_be_log_discard      *ld,
                                            struct m0_be_log_discard_item *ldi)
{
	struct m0_be_log *log;
	struct m0_ext    *ext;

	log = m0_be_log_discard_item_user_data(ldi);
	ext = m0_be_log_discard_item_ext(ldi);

	M0_LOG(M0_DEBUG, "log=%p ldi=%p ext="EXT_F, log, ldi, EXT_P(ext));

	m0_mutex_lock(log->lg_cfg.lc_lock);     /* XXX */
	m0_be_log_record_discard(log, ext->e_end - ext->e_start);
	m0_mutex_unlock(log->lg_cfg.lc_lock);   /* XXX */
}

M0_INTERNAL void m0_be_group_format_seg_place(struct m0_be_group_format *gft,
					      struct m0_be_op           *op)
{
	struct m0_be_op *gft_op;

	gft_op = &gft->gft_pd_io_op;
	M0_SET0(gft_op);
	m0_be_op_init(gft_op);
	M0_SET0(&gft->gft_tmp_op);
	m0_be_op_init(&gft->gft_tmp_op);
	m0_be_op_set_add(op, gft_op);
	m0_be_op_set_add(op, &gft->gft_tmp_op);
	m0_be_op_active(&gft->gft_tmp_op);
	m0_be_op_callback_set(gft_op, &be_tx_group_format_seg_io_starting,
	                      gft, M0_BOS_ACTIVE);
	m0_be_op_callback_set(gft_op, &be_tx_group_format_seg_io_finished,
	                      gft, M0_BOS_DONE);
	m0_be_op_callback_set(gft_op, &be_tx_group_format_seg_io_op_gc,
	                      gft, M0_BOS_GC);
	M0_LOG(M0_DEBUG, "seg_place ldi=%p", gft->gft_log_discard_item);
	m0_be_pd_io_add(gft->gft_cfg.gfc_pd, gft->gft_pd_io, &gft->gft_ext,
			gft_op);
}

M0_INTERNAL void
m0_be_group_format_seg_io_credit(struct m0_be_group_format_cfg *gft_cfg,
                                 struct m0_be_io_credit        *io_cred)
{
	struct m0_be_fmt_group_cfg *fmt_cfg = &gft_cfg->gfc_fmt_cfg;

	*io_cred = M0_BE_IO_CREDIT(fmt_cfg->fgc_reg_nr_max,
	                           fmt_cfg->fgc_reg_size_max,
	                           fmt_cfg->fgc_seg_nr_max);
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
