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

#ifndef __MERO_BE_LOG_STORE_H__
#define __MERO_BE_LOG_STORE_H__

#include "lib/buf.h"            /* m0_buf */
#include "lib/types.h"          /* m0_bcount_t */
#include "module/module.h"      /* m0_module */
#include "stob/stob.h"          /* m0_stob_id */

#include "be/fmt.h"             /* m0_be_fmt_log_store_header */

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

struct m0_be_log_io;
struct m0_be_io;
struct m0_be_io_credit;
struct m0_be_op;
struct m0_stob;

/**
 * Log store provides an interface to an infinite persistent storage. It also
 * has an interface for redundant buffer I/O. Although it is impossible to have
 * a trully unlimited storage, it is possible to have some abstractions over
 * finite storage that allows I/O within some I/O window.
 *
 * Definitions
 * - I/O window - range for which it is guaranteed that read I/O will give the
 *   same data as was previously written by write I/O. For any two I/O windows
 *   [offset1, offset1 + length1) and [offset2, offset2 + length2):
 *   (offset1 <= offset2) => ((offset1 + length1) <= (offset2 + length2));
 * - redundant buffer - a buffer that has multiple copies on persistent storage;
 * - backing storage - persistent storage for I/O;
 * - absolute offset - offset within infinite storage;
 * - real offset - offset within backing storage. It is a value used for backing
 *   storage I/O.
 *
 * Highlights
 * - log store uses stobs as a backing store.
 *
 * Log store hides such knowledge as number of stobs, fragmentation, redundant
 * buffers and their positions.
 *
 * @verbatim
 *
 * |<------------------------------ fsh_size ------------------------------>|
 * |                                                                        |
 * +-----------+--------------------------------+---------------------------+
 * | Log store | Redundant buffers              | Circular buffer with data |
 * |  header   | (contain copies of log header) |                           |
 * +-----------+--------------------------------+---------------------------+
 *             |<------- fsh_rbuf_size -------->|<----- fsh_cbuf_size ----->|
 *             |                                |
 *       fsh_rbuf_offset                  fsh_cbuf_offset
 *
 * @endverbatim
 *
 * Interface:
 * - open()/close()/create()/destroy()
 * - bshift() to provide block shift for memory buffers for I/O;
 * - m0_be_log_io recalculations
 *   - m0_be_log_store_io_credit()
 *   - m0_be_log_store_io_translate()
 *   - m0_be_log_store_io_window()
 *   - m0_be_log_store_io_discard()
 * - interface for redundant buffer I/O
 *   - see m0_be_log_store_header_encode() doc for the explanation
 *
 * Limitations
 * - infinite persistent storage is actually limited by M0_BINDEX_MAX, so
 *   interface provides I/O for range [0, M0_BINDEX_MAX];
 * - current implementation uses one stob as a backing storage.
 *
 * Future directions
 * - interface for log store expand/shrink.
 *
 */

enum {
	M0_BE_LOG_STORE_LEVEL_ASSIGNS,
	M0_BE_LOG_STORE_LEVEL_STOB_DOMAIN,
	M0_BE_LOG_STORE_LEVEL_STOB_FIND,
	M0_BE_LOG_STORE_LEVEL_STOB_LOCATE,
	M0_BE_LOG_STORE_LEVEL_STOB_CREATE,
	M0_BE_LOG_STORE_LEVEL_ZERO,
	M0_BE_LOG_STORE_LEVEL_LS_HEADER_INIT,
	M0_BE_LOG_STORE_LEVEL_LS_HEADER_BUF_ALLOC,
	M0_BE_LOG_STORE_LEVEL_HEADER_CREATE,
	M0_BE_LOG_STORE_LEVEL_HEADER_ENCODE,
	M0_BE_LOG_STORE_LEVEL_HEADER_IO,
	M0_BE_LOG_STORE_LEVEL_HEADER_DECODE,
	M0_BE_LOG_STORE_LEVEL_RBUF_ARR_ALLOC,
	M0_BE_LOG_STORE_LEVEL_RBUF_INIT,
	M0_BE_LOG_STORE_LEVEL_RBUF_ASSIGN,
	M0_BE_LOG_STORE_LEVEL_READY,
};

enum m0_be_log_store_io_type {
	M0_BE_LOG_STORE_IO_READ,
	M0_BE_LOG_STORE_IO_WRITE,
};

struct m0_be_log_store_cfg {
	/*
	 * Backing store stob id.
	 * It's used in m0_be_log_store_create() and m0_be_log_store_open().
	 */
	struct m0_stob_id lsc_stob_id;
	/**
	 * Backing store stob domain location.
	 * Temporary solution until paged implemented.
	 *
	 * The problem is that m0_stob_domain has per-domain direct I/O
	 * configuration. m0_be_log_store can (and should) use stobs with
	 * direct I/O enabled, but m0_be_seg can't use such stobs.
	 */
	char             *lsc_stob_domain_location;
	/**
	 * 2nd parameter for m0_stob_domain_init() and m0_stob_domain_create().
	 */
	const char       *lsc_stob_domain_init_cfg;

	/*
	 * The following fields are used in m0_be_log_store_create() only.
	 * They are saved in log store header (m0_be_fmt_log_store_header)
	 * (except lsc_stob_domain_key and lsc_stob_domain_create_cfg).
	 */

	/**
	 * Key for stob domain with lsc_stob_domain_location location.
	 *
	 * Temporary solution.
	 */
	uint64_t          lsc_stob_domain_key;
	/**
	 * 4rd parameter for m0_stob_domain_create() for stob domain with
	 * lsc_stob_domain_location location.
	 *
	 * Temporary solution.
	 */
	const char       *lsc_stob_domain_create_cfg;

	/** Total size of backing stob. */
	m0_bcount_t       lsc_size;
	/** m0_stob_create() 3rd parameter for the backing store stob. */
	const char       *lsc_stob_create_cfg;
	/**
	 * Don't zero stob after creation. It avoids unnecessary I/O when
	 * the stob is already zeroed.
	 */
	bool              lsc_stob_dont_zero;
	/** Number of redundant buffers. */
	unsigned          lsc_rbuf_nr;
	/** Size of redundant buffer. */
	m0_bcount_t       lsc_rbuf_size;
};

struct m0_be_log_store {
	struct m0_be_log_store_cfg        ls_cfg;
	bool                              ls_create_mode;
	bool                              ls_destroy_mode;
	bool                              ls_stob_destroyed;
	struct m0_module                  ls_module;

	struct m0_stob                   *ls_stob;
	/*
	 * Temporary solution.
	 * @see m0_be_log_store_cfg::lsc_stob_domain_location
	 */
	struct m0_stob_domain            *ls_stob_domain;

	/* Log store header. */
	struct m0_be_fmt_log_store_header ls_header;
	/* Buffer for encoded m0_be_log_store.ls_header. */
	struct m0_buf                     ls_header_buf;
	/**
	 * Maximum discarded offset after the last m0_be_log_store_open()
	 * or m0_be_log_store_create().
	 *
	 * It will have meaning and persistence after multiple stobs as
	 * backing storage implemented.
	 */
	m0_bindex_t                       ls_offset_discarded;

	struct m0_be_log_io              *ls_rbuf_write_lio;
	struct m0_be_op                  *ls_rbuf_write_op;
	struct m0_buf                     ls_rbuf_write_buf;
	struct m0_be_log_io              *ls_rbuf_read_lio;
	struct m0_be_op                  *ls_rbuf_read_op;
	struct m0_buf                    *ls_rbuf_read_buf;
};

M0_INTERNAL bool m0_be_log_store__invariant(struct m0_be_log_store *ls);

M0_INTERNAL int  m0_be_log_store_open(struct m0_be_log_store     *ls,
				      struct m0_be_log_store_cfg *ls_cfg);
M0_INTERNAL void m0_be_log_store_close(struct m0_be_log_store *ls);

M0_INTERNAL int  m0_be_log_store_create(struct m0_be_log_store     *ls,
					struct m0_be_log_store_cfg *ls_cfg);
M0_INTERNAL void m0_be_log_store_destroy(struct m0_be_log_store *ls);
/* TODO destroy by cfg? */

M0_INTERNAL void
m0_be_log_store_module_setup(struct m0_be_log_store     *ls,
			     struct m0_be_log_store_cfg *ls_cfg,
			     bool                        create_mode);

M0_INTERNAL uint32_t m0_be_log_store_bshift(struct m0_be_log_store *ls);
M0_INTERNAL m0_bcount_t m0_be_log_store_buf_size(struct m0_be_log_store *ls);
M0_INTERNAL void m0_be_log_store_io_credit(struct m0_be_log_store *ls,
					   struct m0_be_io_credit *accum);
/**
 * Re-calculates stob indexes for BE I/O operation. Logically, this function
 * adds position value to the stob indexes and translates them into proper
 * pysical offsets. This operation may add new I/O vector to the bio or even
 * new stob. Therefore, user must allocate additional space in m0_be_io
 * structure.
 *
 * @param position logical offset
 * @todo s/io_prepare/io_translate/g
 */
M0_INTERNAL void m0_be_log_store_io_translate(struct m0_be_log_store *ls,
					      m0_bindex_t             position,
					      struct m0_be_io        *bio);

/**
 * Get I/O window for the given offset.
 *
 * @return -EINVAL information about byte at the given offset was already
 *                 discarded.
 */
M0_INTERNAL int m0_be_log_store_io_window(struct m0_be_log_store *ls,
					  m0_bindex_t             offset,
					  m0_bcount_t            *length);
/**
 * Discards information about range [0, offset).
 *
 * @note It is not persistent in the current implementation, but it still
 * can be used for invariants.
 */
M0_INTERNAL void m0_be_log_store_io_discard(struct m0_be_log_store *ls,
					    m0_bindex_t		    offset,
					    struct m0_be_op	   *op);

M0_INTERNAL bool m0_be_log_store_overwrites(struct m0_be_log_store *ls,
					    m0_bindex_t             index,
					    m0_bcount_t             size,
					    m0_bindex_t             position);

/**
 * @todo Make interface for redundant buffer simplier.
 *
 * m0_be_log_store can provide interface to:
 * - set redundant buffer size and number of redundant buffers
 *   in m0_be_log_store_create();
 * - get bufvec of the redundant buffer;
 * - get all m0_be_log_io to perform I/O for redundant buffer.
 *
 * In this case m0_be_log_store can use any kind of data as a redundant
 * buffer, not just m0_be_log_header.
 */
M0_INTERNAL struct m0_buf *
m0_be_log_store_rbuf_write_buf(struct m0_be_log_store *ls);
M0_INTERNAL struct m0_buf *
m0_be_log_store_rbuf_read_buf_first(struct m0_be_log_store *ls,
				    unsigned               *iter);
M0_INTERNAL struct m0_buf *
m0_be_log_store_rbuf_read_buf_next(struct m0_be_log_store *ls,
				   unsigned               *iter);
M0_INTERNAL struct m0_be_log_io *
m0_be_log_store_rbuf_io_first(struct m0_be_log_store        *ls,
			      enum m0_be_log_store_io_type   io_type,
			      struct m0_be_op              **op,
			      unsigned                      *iter);
M0_INTERNAL struct m0_be_log_io *
m0_be_log_store_rbuf_io_next(struct m0_be_log_store        *ls,
			     enum m0_be_log_store_io_type   io_type,
			     struct m0_be_op              **op,
			     unsigned                      *iter);
M0_INTERNAL void
m0_be_log_store_rbuf_io_reset(struct m0_be_log_store       *ls,
			      enum m0_be_log_store_io_type  io_type);

M0_INTERNAL bool
m0_be_log_store_contains_stob(struct m0_be_log_store  *ls,
                              const struct m0_stob_id *stob_id);
/** @} end of be group */

#endif /* __MERO_BE_LOG_STORE_H__ */


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
