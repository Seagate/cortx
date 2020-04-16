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


#pragma once

#ifndef __MERO_BE_TX_GROUP_ONDISK_H__
#define __MERO_BE_TX_GROUP_ONDISK_H__

#include "be/io.h"              /* m0_be_io */
#include "be/tx_regmap.h"       /* m0_be_reg_area */
#include "be/fmt.h"             /* m0_be_fmt_group */
#include "be/log.h"             /* m0_be_log_io */
#include "be/tx_credit.h"       /* m0_be_tx_credit */

/**
 * @defgroup be Meta-data back-end
 *
 * * Overview
 * m0_be_tx_group_format is responsible for group encoding/decoding for log I/O
 * and for group log I/O and seg I/O.
 *
 * * Typical use cases
 *
 * ** Initialisation phase
 * - init()
 * - allocate()
 *
 * ** Common begin of the loop
 * - reset()
 * - prepare()
 *
 * ** Normal operation (middle of the loop)
 * - tx_add()
 * - reg_add()
 * - group_info()
 * - log_use()
 * - encode()
 * - log_write()
 *
 * ** Recovery operation (middle of the loop)
 * - recovery_prepare()
 * - log_read()
 * - decode()
 * - group_info()
 * - tx_nr(), tx_get()
 * - reg_nr(), reg_get()
 *
 * ** Common end of the loop
 * - seg_place_prepare()
 * - seg_place()
 * - log_discard()
 *
 * ** Finalisation phase
 * - deallocate
 * - fini
 *
 * @{
 */

struct m0_be_tx_group;
struct m0_be_log;

typedef void (*m0_be_group_format_reg_area_rebuild_t)
	(struct m0_be_reg_area *ra,
	 struct m0_be_reg_area *ra_new,
	 void                  *param);

enum {
	M0_BE_GROUP_FORMAT_LEVEL_ASSIGNS,
	M0_BE_GROUP_FORMAT_LEVEL_OP_INIT,
	M0_BE_GROUP_FORMAT_LEVEL_FMT_GROUP_INIT,
	M0_BE_GROUP_FORMAT_LEVEL_FMT_CBLOCK_INIT,
	M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_INIT,
	M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_GROUP,
	M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_IO_CREATE_CBLOCK,
	M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ITER_INIT,
	M0_BE_GROUP_FORMAT_LEVEL_INITED,
	M0_BE_GROUP_FORMAT_LEVEL_LOG_RECORD_ALLOCATE,
	M0_BE_GROUP_FORMAT_LEVEL_ALLOCATED,
};

struct m0_be_group_format_cfg {
	struct m0_be_fmt_group_cfg  gfc_fmt_cfg;
	struct m0_be_log           *gfc_log;
	struct m0_be_log_discard   *gfc_log_discard;
	struct m0_be_pd            *gfc_pd;
};

struct m0_be_group_format {
	struct m0_be_group_format_cfg  gft_cfg;
	struct m0_module               gft_module;

	struct m0_be_fmt_group         gft_fmt_group;
	struct m0_be_fmt_cblock        gft_fmt_cblock;
	struct m0_be_fmt_group        *gft_fmt_group_decoded;
	struct m0_be_fmt_cblock       *gft_fmt_cblock_decoded;

	struct m0_be_log              *gft_log;
	struct m0_be_log_record_iter   gft_log_record_iter;
	struct m0_be_log_record        gft_log_record;

	struct m0_be_pd_io            *gft_pd_io;
	struct m0_be_log_discard_item *gft_log_discard_item;
	struct m0_ext                  gft_ext;
	struct m0_be_op                gft_pd_io_op;
	struct m0_be_op                gft_tmp_op;
	/** is used in m0_be_group_format_prepare() */
	struct m0_be_op                gft_pd_io_get;
	/** is used in m0_be_group_format_prepare() */
	struct m0_be_op                gft_log_discard_get;
	/** hack because m0_be_op_tick_ret() needs M0_BOS_ACTIVE state */
	struct m0_be_op                gft_all_get;
};

M0_INTERNAL int m0_be_group_format_init(struct m0_be_group_format     *gft,
					struct m0_be_group_format_cfg *gft_cfg,
					struct m0_be_tx_group         *group,
					struct m0_be_log              *log);
M0_INTERNAL void m0_be_group_format_fini(struct m0_be_group_format *gft);
M0_INTERNAL bool m0_be_group_format__invariant(struct m0_be_group_format *go);

M0_INTERNAL void m0_be_group_format_reset(struct m0_be_group_format *gft);

M0_INTERNAL int  m0_be_group_format_allocate(struct m0_be_group_format *gft);
M0_INTERNAL void m0_be_group_format_deallocate(struct m0_be_group_format *gft);

M0_INTERNAL void
m0_be_group_format_module_setup(struct m0_be_group_format     *gft,
				struct m0_be_group_format_cfg *gft_cfg);

M0_INTERNAL void m0_be_group_format_prepare(struct m0_be_group_format *gft,
                                            struct m0_be_op           *op);
M0_INTERNAL void m0_be_group_format_encode(struct m0_be_group_format *gft);
M0_INTERNAL int  m0_be_group_format_decode(struct m0_be_group_format *gft);

M0_INTERNAL void m0_be_group_format_reg_log_add(struct m0_be_group_format *gft,
						const struct m0_be_reg_d  *rd);
M0_INTERNAL void m0_be_group_format_reg_seg_add(struct m0_be_group_format *gft,
						const struct m0_be_reg_d  *rd);
M0_INTERNAL uint32_t
m0_be_group_format_reg_nr(const struct m0_be_group_format *gft);
M0_INTERNAL void
m0_be_group_format_reg_get(const struct m0_be_group_format *gft,
			   uint32_t                         index,
			   struct m0_be_reg_d              *rd);

M0_INTERNAL void m0_be_group_format_tx_add(struct m0_be_group_format *gft,
					   struct m0_be_fmt_tx       *ftx);
M0_INTERNAL uint32_t
m0_be_group_format_tx_nr(const struct m0_be_group_format *gft);
M0_INTERNAL void
m0_be_group_format_tx_get(const struct m0_be_group_format *gft,
			  uint32_t                         index,
			  struct m0_be_fmt_tx             *ftx);

M0_INTERNAL struct m0_be_fmt_group_info *
m0_be_group_format_group_info(struct m0_be_group_format *gft);

/* static method */
M0_INTERNAL m0_bcount_t
m0_be_group_format_log_reserved_size(struct m0_be_log       *log,
				     struct m0_be_tx_credit *cred,
				     m0_bcount_t             cred_payload);
M0_INTERNAL void
m0_be_group_format_log_use(struct m0_be_group_format *gft,
			   m0_bcount_t                size_reserved);
M0_INTERNAL void
m0_be_group_format_recovery_prepare(struct m0_be_group_format *gft,
				    struct m0_be_log          *log);

M0_INTERNAL void m0_be_group_format_log_write(struct m0_be_group_format *gft,
					      struct m0_be_op           *op);
M0_INTERNAL void m0_be_group_format_log_read(struct m0_be_group_format *gft,
					     struct m0_be_op           *op);

M0_INTERNAL void
m0_be_group_format_seg_place_prepare(struct m0_be_group_format *gft);
M0_INTERNAL void m0_be_group_format_seg_place(struct m0_be_group_format *gft,
					      struct m0_be_op           *op);

/* move to engine? */
M0_INTERNAL void m0_be_group_format_discard(struct m0_be_log_discard      *ld,
                                            struct m0_be_log_discard_item *ldi);


M0_INTERNAL void
m0_be_group_format_seg_io_credit(struct m0_be_group_format_cfg *gft_cfg,
                                 struct m0_be_io_credit        *io_cred);

/** @} end of be group */

#endif /* __MERO_BE_TX_GROUP_ONDISK_H__ */

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
