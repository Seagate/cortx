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

#pragma once

#ifndef __MERO_BE_FMT_H__
#define __MERO_BE_FMT_H__

#include "lib/types.h"          /* m0_bcount_t */
#include "lib/buf_xc.h"         /* m0_buf_xc */
#include "lib/assert.h"		/* M0_BASSERT */

/**
 * @defgroup be
 *
 * Interface
 *
 * * Overview
 * be/fmt is an abstraction on top of xcode. It makes xcoding in BE simpler.
 *
 * ** There are 3 top-level objects in this file:
 * - m0_be_fmt_log_header
 * - m0_be_fmt_group
 * - m0_be_fmt_cblock
 *
 * ** There are 8 functions for each top-level object:
 * - init()
 * - fini()
 * - reset()
 * - size()
 * - size_max()
 * - encode()
 * - decode()
 * - decoded_free()
 *
 * * Design highlighs
 * - intermediate structures like m0_be_fmt_tx and m0_be_fmt_reg are used
 *   in accessors to hide actual object placement inside xcoded structures;
 * - m0_be_fmt_<object_name>_{init,fini,...}() may be empty, but they are added
 *   to unify interface.
 *
 * * Typical use cases
 *
 * ** Encoding
 * - init()
 * - size() - determine buffer size
 * - encode() - use buffer with size returned on previous step
 * - fini()
 *
 * ** Decoding
 * - decode()
 * - decoded_free() - after using decoded objects to free memory allocated
 *   on decode() step
 *
 * ** Encoding with preallocation
 * - initialization stage
 *   - init() - allocates all structures needed for maximal number of elements.
 *   - size_max() - finds out maximum buffer size needed for encoding
 * - working cycle (as many times as needed)
 *   - reset() - moves objects to their initial state
 *   - "structure_part"_add() (optional) fills structures with data to encode;
 *     examples: m0_be_fmt_group_tx_add(), m0_be_fmt_group_reg_add()
 *   - size() (optional) - determine encoded size
 *   - encode() - to encode
 * - finalisation stage
 *   - fini() - to free all structures allocated in init() (if any)
 *
 * * Memory allocation policy
 *
 * Interface is designed to provide encode() and mutator functions to be free
 * of allocations/deallocations.
 *
 * ** General
 *
 * - reset()
 * - size()
 * - size_max()
 * - encode()
 * - "structure_part"_add() (mutators)
 * - init()
 * - fini()
 * - decode()
 * - decoded_free() - same as decode().
 *   - they can have any number of m0_alloc() or m0_free() calls.
 *
 * ** Decoding
 * Memory is allocated using m0_alloc() wherever it is needed. There is no
 * optimisation like preallocation.
 *
 * ** Encoding
 *
 * *** Fixed size structures
 * m0_be_fmt_log_header and m0_be_fmt_cblock have fixed size, so
 * their init() (fini()) functions don't have m0_alloc() (m0_free()) calls.
 *
 * *** Variable size structures
 * m0_be_fmt_group may have different encoded size depending on number of
 * elements (tx, regions etc.).
 * Memory for maximum number of elements is allocated in init() function.
 *
 * @{
 */

struct m0_buf;
struct m0_be_group_format;
struct m0_be_fmt_log_record_header;

struct m0_be_fmt_group_info {
	/* there is nothing we can do with unknown field. */
	uint64_t gi_unknown;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_group_header {
	struct m0_be_fmt_group_info fgh_info;
	uint64_t                    fgh_lsn;
	uint64_t                    fgh_size;
	uint64_t                    fgh_tx_nr;
	uint64_t                    fgh_reg_nr;
	uint64_t                    fgh_magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_content_header_tx {
	uint64_t    chx_tx_id;
	m0_bcount_t chx_payload_size;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_content_header_txs {
	uint32_t                            cht_nr;
	struct m0_be_fmt_content_header_tx *cht_tx;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);

struct m0_be_fmt_content_header_reg {
	m0_bcount_t  chg_size;
	uint64_t     chg_addr; /* has to be (void *) */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_content_header_reg_area {
	uint32_t                             chr_nr;
	struct m0_be_fmt_content_header_reg *chr_reg;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);

struct m0_be_fmt_content_header {
	struct m0_be_fmt_content_header_txs      fch_txs;
	struct m0_be_fmt_content_header_reg_area fch_reg_area;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_content_payloads {
	uint32_t       fcp_nr;
	struct m0_buf *fcp_payload;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);

struct m0_be_fmt_content_reg_area {
	uint32_t       cra_nr;
	struct m0_buf *cra_reg;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);

struct m0_be_fmt_content {
	struct m0_be_fmt_content_payloads fmc_payloads;
	struct m0_be_fmt_content_reg_area fmc_reg_area;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/* -------------------------------------------------------------------------- */

struct m0_be_fmt_log_store_header_cfg {
	int fshc_unused;
};

struct m0_be_fmt_log_store_header {
	/* total usable size of the stob */
	m0_bcount_t fsh_size;
	/* redundant buffer configuration */
	m0_bindex_t fsh_rbuf_offset;
	unsigned    fsh_rbuf_nr;
	m0_bcount_t fsh_rbuf_size;
	m0_bcount_t fsh_rbuf_size_aligned;
	/* circular buffer configuration */
	m0_bindex_t fsh_cbuf_offset;
	m0_bcount_t fsh_cbuf_size;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_group_cfg;

struct m0_be_fmt_log_header {
	uint64_t    flh_serial;
	m0_bindex_t flh_discarded;
	m0_bindex_t flh_group_lsn;
	m0_bcount_t flh_group_size;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_group {
	struct m0_be_fmt_group_header      fg_header;
	struct m0_be_fmt_content_header    fg_content_header;
	struct m0_be_fmt_content           fg_content;
	/**
	 * uint64_t is used here to prevent xcoding of this field.
	 *
	 * It is a pointer to m0_be_fmt_group_cfg and it is set in
	 * m0_be_fmt_group_init().
	 */
	uint64_t                           fg_cfg;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

M0_BASSERT(sizeof(((struct m0_be_fmt_group *)NULL)->fg_cfg) ==
	   sizeof(struct m0_be_fmt_group_cfg *));

struct m0_be_fmt_cblock {
	uint64_t gcb_lsn;
	uint64_t gcb_size;
	uint64_t gcb_tx_nr;
	uint64_t gcb_magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_group_cfg {
	uint64_t fgc_tx_nr_max;
	uint64_t fgc_reg_nr_max;
	uint64_t fgc_payload_size_max;
	uint64_t fgc_reg_size_max;
	uint64_t fgc_seg_nr_max;
};

struct m0_be_fmt_log_record_footer {
	m0_bindex_t lrf_pos;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_be_fmt_log_record_header_cfg {
	uint64_t lrhc_io_nr_max;
};

struct m0_be_fmt_log_record_header_io_size {
	uint32_t     lrhs_nr;
	m0_bcount_t *lrhs_size;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);

struct m0_be_fmt_log_record_header {
	m0_bindex_t                            lrh_pos;
	m0_bcount_t                            lrh_size;
	m0_bindex_t                            lrh_discarded;
	m0_bindex_t                            lrh_prev_pos;
	m0_bindex_t                            lrh_prev_size;
	uint64_t                               lrh_io_nr_max;
	struct m0_be_fmt_log_record_header_io_size lrh_io_size;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

#define BFLRH_F "(pos=%"PRIu64" size=%"PRIu64" discarded=%"PRIu64" " \
		"prev_pos=%"PRIu64" prev_size=%"PRIu64" io_nr_max=%"PRIu64")"
#define BFLRH_P(h) (h)->lrh_pos, (h)->lrh_size, (h)->lrh_discarded, \
		   (h)->lrh_prev_pos, (h)->lrh_prev_size,           \
		   (h)->lrh_io_nr_max

/**
 * Format decode function config. Used to check decoded values against various
 * configuration parameters.
 */
struct m0_be_fmt_decode_cfg {
	uint64_t   dc_group_size_max;
	int      (*dc_iter)(const struct m0_xcode_cursor *it);
	void     (*dc_iter_end)(const struct m0_xcode_cursor *it);
};

/**
 * Trace function for fmt types, when types are iterated generates log output.
 */
M0_INTERNAL int  m0_be_fmt_type_trace(const struct m0_xcode_cursor *it);
M0_INTERNAL void m0_be_fmt_type_trace_end(const struct m0_xcode_cursor *it);

#define M0_BE_FMT_DECODE_CFG_DEFAULT                      \
	(&(const struct m0_be_fmt_decode_cfg) {           \
		.dc_group_size_max =  1 << 24,            \
		.dc_iter = NULL,                          \
		.dc_iter_end = NULL,                      \
	})

#define M0_BE_FMT_DECODE_CFG_DEFAULT_WITH_TRACE           \
	(&(const struct m0_be_fmt_decode_cfg) {           \
		.dc_group_size_max =  1 << 24,            \
		.dc_iter = m0_be_fmt_type_trace,          \
		.dc_iter_end  = m0_be_fmt_type_trace_end, \
	})

/* functional interfaces */
#define M0_BE_FMT_DECLARE(name)                                                \
struct m0_be_fmt_##name;                                                       \
struct m0_be_fmt_##name##_cfg;                                                 \
M0_INTERNAL int                                                                \
m0_be_fmt_##name##_init(struct m0_be_fmt_##name              *obj,             \
			const struct m0_be_fmt_##name##_cfg *cfg);             \
M0_INTERNAL void m0_be_fmt_##name##_fini(struct m0_be_fmt_##name *obj);        \
M0_INTERNAL void m0_be_fmt_##name##_reset(struct m0_be_fmt_##name *obj);       \
M0_INTERNAL m0_bcount_t                                                        \
m0_be_fmt_##name##_size(struct m0_be_fmt_##name *obj);                         \
M0_INTERNAL m0_bcount_t                                                        \
m0_be_fmt_##name##_size_max(const struct m0_be_fmt_##name##_cfg *cfg);         \
M0_INTERNAL int m0_be_fmt_##name##_encode(struct m0_be_fmt_##name  *obj,       \
					  struct m0_bufvec_cursor  *cur);      \
M0_INTERNAL int m0_be_fmt_##name##_encode_buf(struct m0_be_fmt_##name  *obj,   \
					      struct m0_buf            *buf);  \
M0_INTERNAL int                                                                \
m0_be_fmt_##name##_decode(struct m0_be_fmt_##name          **obj,              \
			  struct m0_bufvec_cursor           *cur,              \
			  const struct m0_be_fmt_decode_cfg *cfg);             \
M0_INTERNAL int                                                                \
m0_be_fmt_##name##_decode_buf(struct m0_be_fmt_##name          **obj,          \
			      struct m0_buf                     *buf,          \
			      const struct m0_be_fmt_decode_cfg *cfg);         \
M0_INTERNAL void m0_be_fmt_##name##_decoded_free(struct m0_be_fmt_##name *obj)

/* -------------------------------------------------------------------------- */

M0_BE_FMT_DECLARE(group);
M0_BE_FMT_DECLARE(log_store_header);
M0_BE_FMT_DECLARE(log_record_footer);
M0_BE_FMT_DECLARE(log_header);

/* Iterative interface to add tx from group and group reg_area into
 * m0_be_fmt_group
 */

struct m0_be_fmt_reg {
	m0_bcount_t  fr_size;
	void        *fr_addr;
	void        *fr_buf;
};

#define M0_BE_FMT_REG(size, addr, buf) (struct m0_be_fmt_reg){  \
	.fr_size = (size),                                      \
	.fr_addr = (addr),                                      \
	.fr_buf  = (buf),                                       \
}

M0_INTERNAL void m0_be_fmt_group_reg_add(struct m0_be_fmt_group     *fg,
					 const struct m0_be_fmt_reg *freg);

M0_INTERNAL uint32_t m0_be_fmt_group_reg_nr(const struct m0_be_fmt_group *fg);
M0_INTERNAL void m0_be_fmt_group_reg_by_id(const struct m0_be_fmt_group *fg,
					   uint32_t                      index,
					   struct m0_be_fmt_reg         *freg);

struct m0_be_fmt_tx {
	struct m0_buf  bft_payload;
	uint64_t       bft_id;
};

#define M0_BE_FMT_TX(payload, id) (struct m0_be_fmt_tx){        \
	.bft_payload = (payload),                               \
	.bft_id      = (id),                                    \
}

M0_INTERNAL void m0_be_fmt_group_tx_add(struct m0_be_fmt_group    *fg,
					const struct m0_be_fmt_tx *ftx);
M0_INTERNAL uint32_t m0_be_fmt_group_tx_nr(const struct m0_be_fmt_group *fg);
M0_INTERNAL void m0_be_fmt_group_tx_by_id(const struct m0_be_fmt_group *fg,
					  uint32_t                      index,
					  struct m0_be_fmt_tx          *ftx);

M0_INTERNAL struct m0_be_fmt_group_info *
m0_be_fmt_group_info_get(struct m0_be_fmt_group *fg);

M0_INTERNAL bool m0_be_fmt_group_sanity_check(struct m0_be_fmt_group *fg);

M0_BE_FMT_DECLARE(cblock);
M0_BE_FMT_DECLARE(log_record_header);

M0_INTERNAL void
m0_be_fmt_log_record_header_io_size_add(struct m0_be_fmt_log_record_header *obj,
					m0_bcount_t                       size);
#undef M0_BE_FMT_DECLARE

/** @} end of be group */
#endif /* __MERO_BE_FMT_H__ */

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
