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

#include "be/log_store.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"          /* M0_LOG */
#include "lib/errno.h"          /* ENOMEM */
#include "lib/memory.h"         /* m0_alloc */
#include "lib/misc.h"           /* M0_SET0 */

#include "be/fmt.h"             /* m0_be_fmt_log_header */
#include "be/log.h"             /* m0_be_log_io */
#include "be/op.h"              /* m0_be_op */
#include "be/io.h"              /* m0_be_io */

#include "module/instance.h"    /* m0_get */

#include "stob/stob.h"          /* m0_stob_find */
#include "stob/domain.h"        /* m0_stob_domain_create */

/**
 * @addtogroup be
 *
 * @{
 */

enum {
	M0_BE_LOG_STORE_WRITE_SIZE_MAX = 4 * 1024 * 1024,
};

static int be_log_store_buf_alloc_aligned(struct m0_be_log_store *ls,
					  struct m0_buf          *buf,
					  m0_bcount_t             size)
{
	unsigned shift = m0_be_log_store_bshift(ls);

	buf->b_nob  = size;
	size        = m0_align(size, 1ULL << shift);
	buf->b_addr = m0_alloc_aligned(size, shift);

	return buf->b_addr == NULL ? -ENOMEM : 0;
}

static void be_log_store_buf_free_aligned(struct m0_be_log_store *ls,
					  struct m0_buf          *buf)
{
	unsigned shift = m0_be_log_store_bshift(ls);

	m0_free_aligned(buf->b_addr, m0_align(buf->b_nob, 1ULL << shift),
			shift);
}

M0_INTERNAL bool m0_be_log_store__invariant(struct m0_be_log_store *ls)
{
	return true;
}

static int be_log_store_zero(struct m0_be_log_store *ls, m0_bcount_t ls_size)
{
	m0_bindex_t pos;
	m0_bcount_t size;
	uint32_t    bshift;
	void       *zero;
	int         rc;

	bshift = m0_stob_block_shift(ls->ls_stob);
	zero   = m0_alloc_aligned(M0_BE_LOG_STORE_WRITE_SIZE_MAX, bshift);
	rc     = zero == NULL ? -ENOMEM : 0;
	for (pos = 0; rc == 0 && pos < ls_size;
	     pos += M0_BE_LOG_STORE_WRITE_SIZE_MAX) {
		size = min64(ls_size - pos, M0_BE_LOG_STORE_WRITE_SIZE_MAX);
		rc   = m0_be_io_single(ls->ls_stob, SIO_WRITE, zero, pos, size);
	}
	m0_free_aligned(zero, M0_BE_LOG_STORE_WRITE_SIZE_MAX, bshift);

	return rc;
}

static struct m0_be_log_store *
be_log_store_module2store(struct m0_module *module)
{
	/* XXX bob_of */
	return container_of(module, struct m0_be_log_store, ls_module);
}

static bool
be_log_store_header_validate(struct m0_be_fmt_log_store_header *header,
			     uint64_t                           alignment)
{
	M0_LOG(M0_DEBUG, "log store header begin");
	M0_LOG(M0_DEBUG, "size = %"PRIu64,           header->fsh_size);
	M0_LOG(M0_DEBUG, "rbuf_offset = %"PRIu64,    header->fsh_rbuf_offset);
	M0_LOG(M0_DEBUG, "rbuf_nr = %u",             header->fsh_rbuf_nr);
	M0_LOG(M0_DEBUG, "rbuf_size = %"PRIu64,      header->fsh_rbuf_size);
	M0_LOG(M0_DEBUG, "rbuf_size_aligned = %"PRIu64,
	       header->fsh_rbuf_size_aligned);
	M0_LOG(M0_DEBUG, "cbuf_offset = %"PRIu64,    header->fsh_cbuf_offset);
	M0_LOG(M0_DEBUG, "cbuf_size = %"PRIu64,      header->fsh_cbuf_size);
	M0_LOG(M0_DEBUG, "log store header end");

	return header->fsh_size > 0 &&
	       header->fsh_rbuf_nr > 0 &&
	       header->fsh_cbuf_offset >=
	       header->fsh_rbuf_offset + header->fsh_rbuf_nr *
					 header->fsh_rbuf_size_aligned &&
	       header->fsh_cbuf_offset +
	       header->fsh_cbuf_size <= header->fsh_size &&
	       m0_is_aligned(header->fsh_rbuf_offset, alignment) &&
	       m0_is_aligned(header->fsh_rbuf_size_aligned, alignment) &&
	       m0_is_aligned(header->fsh_cbuf_offset, alignment);
}

static int be_log_store_rbuf_alloc(struct m0_be_log_store *ls,
				   struct m0_buf          *buf)
{
	return be_log_store_buf_alloc_aligned(ls, buf,
				   ls->ls_header.fsh_rbuf_size_aligned);
}

static void be_log_store_rbuf_free(struct m0_be_log_store *ls,
				   struct m0_buf          *buf)
{
	be_log_store_buf_free_aligned(ls, buf);
}

static int be_log_store_rbuf_init_fini_index(struct m0_be_log_store *ls,
					     int                     i,
					     bool                    init)
{
	struct m0_be_io_credit iocred;
	uint32_t               bshift;
	int                    rc = 0;

	if (!init)
		goto fini;

	rc = be_log_store_rbuf_alloc(ls, &ls->ls_rbuf_read_buf[i]);
	if (rc != 0)
		goto err;
	rc = m0_be_log_io_init(&ls->ls_rbuf_write_lio[i]);
	if (rc != 0)
		goto buf_free;
	rc = m0_be_log_io_init(&ls->ls_rbuf_read_lio[i]);
	if (rc != 0)
		goto iow_fini;
	iocred = M0_BE_IO_CREDIT(1, ls->ls_header.fsh_rbuf_size_aligned, 1);
	bshift = m0_be_log_store_bshift(ls);
	rc = m0_be_log_io_allocate(&ls->ls_rbuf_write_lio[i], &iocred, bshift);
	if (rc != 0)
		goto ior_fini;
	rc = m0_be_log_io_allocate(&ls->ls_rbuf_read_lio[i], &iocred, bshift);
	if (rc != 0)
		goto iow_dealloc;
	m0_be_op_init(&ls->ls_rbuf_write_op[i]);
	m0_be_op_init(&ls->ls_rbuf_read_op[i]);
	return 0;

fini:
	m0_be_op_fini(&ls->ls_rbuf_read_op[i]);
	m0_be_op_fini(&ls->ls_rbuf_write_op[i]);
	m0_be_log_io_deallocate(&ls->ls_rbuf_read_lio[i]);
iow_dealloc:
	m0_be_log_io_deallocate(&ls->ls_rbuf_write_lio[i]);
ior_fini:
	m0_be_log_io_fini(&ls->ls_rbuf_read_lio[i]);
iow_fini:
	m0_be_log_io_fini(&ls->ls_rbuf_write_lio[i]);
buf_free:
	be_log_store_rbuf_free(ls, &ls->ls_rbuf_read_buf[i]);
err:
	return rc;
}

static void be_log_store_rbuf_fini_nr(struct m0_be_log_store *ls,
				      int                     nr)
{
	int i;

	for (i = nr - 1; i >= 0; --i)
		be_log_store_rbuf_init_fini_index(ls, i, false);
}

static void be_log_store_rbuf_fini(struct m0_be_log_store *ls)
{
	be_log_store_rbuf_fini_nr(ls, ls->ls_header.fsh_rbuf_nr);
	be_log_store_rbuf_free(ls, &ls->ls_rbuf_write_buf);
}

static int be_log_store_rbuf_init(struct m0_be_log_store *ls)
{
	int i;
	int rc = 0;

	for (i = 0; i < ls->ls_header.fsh_rbuf_nr; ++i) {
		rc = be_log_store_rbuf_init_fini_index(ls, i, true);
		if (rc != 0)
			break;
	}
	rc = rc ?: be_log_store_rbuf_alloc(ls, &ls->ls_rbuf_write_buf);
	if (rc != 0)
		be_log_store_rbuf_fini_nr(ls, i);
	return rc;
}

static void be_log_store_rbuf_arr_free(struct m0_be_log_store *ls)
{
	m0_free(ls->ls_rbuf_write_lio);
	m0_free(ls->ls_rbuf_write_op);
	m0_free(ls->ls_rbuf_read_lio);
	m0_free(ls->ls_rbuf_read_op);
	m0_free(ls->ls_rbuf_read_buf);
}

static int be_log_store_level_enter(struct m0_module *module)
{
	struct m0_be_fmt_log_store_header *header;
	struct m0_buf          *buf;
	enum m0_stob_io_opcode  opcode;
	struct m0_be_log_store *ls    = be_log_store_module2store(module);
	int                     level = module->m_cur + 1;
	struct m0_stob_id      *stob_id = &ls->ls_cfg.lsc_stob_id;
	m0_bcount_t             size;
	m0_bcount_t             header_size;
	uint32_t                shift;
	uint64_t                alignment;
	int                     rc;

	switch (level) {
	case M0_BE_LOG_STORE_LEVEL_ASSIGNS:
		ls->ls_stob_destroyed   = false;
		ls->ls_offset_discarded = 0;
		return 0;
	case M0_BE_LOG_STORE_LEVEL_STOB_DOMAIN:
		if (ls->ls_create_mode) {
			/* Destroy stob domain if exists. */
			rc = m0_stob_domain_init(
					ls->ls_cfg.lsc_stob_domain_location,
					ls->ls_cfg.lsc_stob_domain_init_cfg,
					&ls->ls_stob_domain);
			if (rc == 0)
				rc = m0_stob_domain_destroy(ls->ls_stob_domain);
			else if (rc == -ENOENT)
				rc = 0;
			return rc ?: m0_stob_domain_create(
					ls->ls_cfg.lsc_stob_domain_location,
					ls->ls_cfg.lsc_stob_domain_init_cfg,
					ls->ls_cfg.lsc_stob_domain_key,
					ls->ls_cfg.lsc_stob_domain_create_cfg,
					&ls->ls_stob_domain);
		}
		return m0_stob_domain_init(ls->ls_cfg.lsc_stob_domain_location,
					   ls->ls_cfg.lsc_stob_domain_init_cfg,
					   &ls->ls_stob_domain);
	case M0_BE_LOG_STORE_LEVEL_STOB_FIND:
		/*
		 * As BE log is no longer in 0types, it's configuration
		 * contains only stob domain location and the stob fid.
		 * si_domain_fid is set here after the stob domain is
		 * initialised.
		 *
		 * m0_be_log_store may not need a separate domain after paged
		 * is implemented, see the explanation in
		 * be_domain_level_enter()::M0_BE_DOMAIN_LEVEL_LOG_CONFIGURE.
		 *
		 * XXX TODO remote this after paged is implemented.
		 */
		/* temporary solution BEGIN */
		stob_id->si_domain_fid =
			*m0_stob_domain_id_get(ls->ls_stob_domain);
		/* temporary solution END */
		return m0_stob_find(stob_id, &ls->ls_stob);
	case M0_BE_LOG_STORE_LEVEL_STOB_LOCATE:
		if (m0_stob_state_get(ls->ls_stob) == CSS_UNKNOWN)
		       return m0_stob_locate(ls->ls_stob);
		return 0;
	case M0_BE_LOG_STORE_LEVEL_STOB_CREATE:
		if (ls->ls_create_mode) {
			return m0_stob_create(ls->ls_stob, NULL,
					      ls->ls_cfg.lsc_stob_create_cfg);
		}
		return m0_stob_state_get(ls->ls_stob) == CSS_EXISTS ?
		       0 : M0_ERR(-ENOENT);
	case M0_BE_LOG_STORE_LEVEL_ZERO:
		if (ls->ls_create_mode) {
			M0_ASSERT(ergo(ls->ls_cfg.lsc_stob_create_cfg != NULL,
				       !ls->ls_cfg.lsc_stob_dont_zero));
			return ls->ls_cfg.lsc_stob_dont_zero ? 0 :
			       be_log_store_zero(ls, ls->ls_cfg.lsc_size);
		}
		return 0;
	case M0_BE_LOG_STORE_LEVEL_LS_HEADER_INIT:
		return m0_be_fmt_log_store_header_init(&ls->ls_header,
						       NULL);
	case M0_BE_LOG_STORE_LEVEL_LS_HEADER_BUF_ALLOC:
		shift = m0_be_log_store_bshift(ls);
		size  = m0_be_fmt_log_store_header_size_max(NULL);
		size  = m0_align(size, 1ULL << shift);
		return be_log_store_buf_alloc_aligned(ls,
						      &ls->ls_header_buf,
						      size);
	case M0_BE_LOG_STORE_LEVEL_HEADER_CREATE:
		if (!ls->ls_create_mode)
			return 0;
		shift     = m0_be_log_store_bshift(ls);
		alignment = 1ULL << shift;
		size = m0_be_fmt_log_store_header_size_max(NULL);
		header_size = m0_align(size, alignment);

		header = &ls->ls_header;
		header->fsh_size              = ls->ls_cfg.lsc_size;
		header->fsh_rbuf_offset       = header_size;
		header->fsh_rbuf_nr           = ls->ls_cfg.lsc_rbuf_nr;
		header->fsh_rbuf_size         = ls->ls_cfg.lsc_rbuf_size;
		header->fsh_rbuf_size_aligned = m0_align(header->fsh_rbuf_size,
							 alignment);
		header->fsh_cbuf_offset = header->fsh_rbuf_offset +
					  header->fsh_rbuf_nr *
					  header->fsh_rbuf_size_aligned;
		header->fsh_cbuf_size = header->fsh_size -
					header->fsh_cbuf_offset;
		M0_ASSERT(be_log_store_header_validate(header, alignment));
		return 0;
	case M0_BE_LOG_STORE_LEVEL_HEADER_ENCODE:
		if (!ls->ls_create_mode)
			return 0;
		return m0_be_fmt_log_store_header_encode_buf(
			     &ls->ls_header, &ls->ls_header_buf);
	case M0_BE_LOG_STORE_LEVEL_HEADER_IO:
		buf    = &ls->ls_header_buf;
		opcode = ls->ls_create_mode ? SIO_WRITE : SIO_READ;
		return m0_be_io_single(ls->ls_stob, opcode,
				       buf->b_addr, 0, buf->b_nob);
	case M0_BE_LOG_STORE_LEVEL_HEADER_DECODE:
		if (ls->ls_create_mode)
			return 0;
		rc = m0_be_fmt_log_store_header_decode_buf(&header,
					   &ls->ls_header_buf,
					   M0_BE_FMT_DECODE_CFG_DEFAULT);
		if (rc == 0) {
			ls->ls_header = *header;
			m0_be_fmt_log_store_header_decoded_free(header);

			shift     = m0_be_log_store_bshift(ls);
			alignment = 1ULL << shift;
			header    = &ls->ls_header;
			if (!be_log_store_header_validate(header, alignment))
				rc = M0_ERR(-EINVAL);
		}
		return rc;
	case M0_BE_LOG_STORE_LEVEL_RBUF_ARR_ALLOC:
		M0_ALLOC_ARR(ls->ls_rbuf_write_lio, ls->ls_header.fsh_rbuf_nr);
		M0_ALLOC_ARR(ls->ls_rbuf_write_op,  ls->ls_header.fsh_rbuf_nr);
		M0_ALLOC_ARR(ls->ls_rbuf_read_lio,  ls->ls_header.fsh_rbuf_nr);
		M0_ALLOC_ARR(ls->ls_rbuf_read_op,   ls->ls_header.fsh_rbuf_nr);
		M0_ALLOC_ARR(ls->ls_rbuf_read_buf,  ls->ls_header.fsh_rbuf_nr);
		if (ls->ls_rbuf_write_lio == NULL ||
		    ls->ls_rbuf_write_op  == NULL ||
		    ls->ls_rbuf_read_lio  == NULL ||
		    ls->ls_rbuf_read_op   == NULL ||
		    ls->ls_rbuf_read_buf  == NULL) {
			be_log_store_rbuf_arr_free(ls);
			return M0_ERR(-ENOMEM);
		}
		return 0;
	case M0_BE_LOG_STORE_LEVEL_RBUF_INIT:
		return be_log_store_rbuf_init(ls);
	case M0_BE_LOG_STORE_LEVEL_RBUF_ASSIGN:
		m0_be_log_store_rbuf_io_reset(ls, M0_BE_LOG_STORE_IO_READ);
		m0_be_log_store_rbuf_io_reset(ls, M0_BE_LOG_STORE_IO_WRITE);
		return 0;
	default:
		return M0_ERR(-ENOSYS);
	}
}

static void be_log_store_level_leave(struct m0_module *module)
{
	struct m0_be_log_store *ls = be_log_store_module2store(module);
	int                     level = module->m_cur;
	int                     rc;

	switch (level) {
	case M0_BE_LOG_STORE_LEVEL_ASSIGNS:
		break;
	case M0_BE_LOG_STORE_LEVEL_STOB_DOMAIN:
		if (ls->ls_destroy_mode) {
			rc = m0_stob_domain_destroy(ls->ls_stob_domain);
			M0_ASSERT_INFO(rc == 0, "rc = %d", rc); /* XXX */
		} else {
			m0_stob_domain_fini(ls->ls_stob_domain);
		}
		break;
	case M0_BE_LOG_STORE_LEVEL_STOB_FIND:
		if (!ls->ls_stob_destroyed)
			m0_stob_put(ls->ls_stob);
		break;
	case M0_BE_LOG_STORE_LEVEL_STOB_LOCATE:
		break;
	case M0_BE_LOG_STORE_LEVEL_STOB_CREATE:
		if (ls->ls_destroy_mode) {
			rc = m0_stob_destroy(ls->ls_stob, NULL);
			M0_ASSERT_INFO(rc == 0, "rc = %d", rc); /* XXX */
			ls->ls_stob_destroyed = true;
		}
		break;
	case M0_BE_LOG_STORE_LEVEL_ZERO:
		break;
	case M0_BE_LOG_STORE_LEVEL_LS_HEADER_INIT:
		m0_be_fmt_log_store_header_fini(&ls->ls_header);
		break;
	case M0_BE_LOG_STORE_LEVEL_LS_HEADER_BUF_ALLOC:
		be_log_store_buf_free_aligned(ls, &ls->ls_header_buf);
		break;
	case M0_BE_LOG_STORE_LEVEL_HEADER_CREATE:
	case M0_BE_LOG_STORE_LEVEL_HEADER_ENCODE:
	case M0_BE_LOG_STORE_LEVEL_HEADER_IO:
	case M0_BE_LOG_STORE_LEVEL_HEADER_DECODE:
		break;
	case M0_BE_LOG_STORE_LEVEL_RBUF_ARR_ALLOC:
		be_log_store_rbuf_arr_free(ls);
		break;
	case M0_BE_LOG_STORE_LEVEL_RBUF_INIT:
		be_log_store_rbuf_fini(ls);
		break;
	case M0_BE_LOG_STORE_LEVEL_RBUF_ASSIGN:
		break;
	default:
		M0_IMPOSSIBLE("Unexpected m0_module level");
	}
}

static const struct m0_modlev be_log_store_levels[] = {
	[M0_BE_LOG_STORE_LEVEL_ASSIGNS] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_ASSIGNS",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_STOB_DOMAIN] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_STOB_DOMAIN",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_STOB_FIND] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_STOB_FIND",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_STOB_LOCATE] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_STOB_LOCATE",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_STOB_CREATE] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_STOB_CREATE",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_ZERO] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_ZERO",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_LS_HEADER_INIT] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_LS_HEADER_INIT",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_LS_HEADER_BUF_ALLOC] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_LS_HEADER_BUF_ALLOC",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_HEADER_CREATE] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_HEADER_CREATE",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_HEADER_ENCODE] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_HEADER_ENCODE",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_HEADER_IO] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_HEADER_IO",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_HEADER_DECODE] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_HEADER_DECODE",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_RBUF_ARR_ALLOC] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_RBUF_ARR_ALLOC",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_RBUF_INIT] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_RBUF_INIT",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_RBUF_ASSIGN] = {
		.ml_name  = "M0_BE_LOG_STORE_LEVEL_RBUF_ASSIGN",
		.ml_enter = be_log_store_level_enter,
		.ml_leave = be_log_store_level_leave,
	},
	[M0_BE_LOG_STORE_LEVEL_READY] = {
		.ml_name = "fully initialized",
	},
};

M0_INTERNAL void
m0_be_log_store_module_setup(struct m0_be_log_store     *ls,
			     struct m0_be_log_store_cfg *ls_cfg,
			     bool                        create_mode)
{
	M0_ENTRY("lsc_stob_id="STOB_ID_F" lsc_size=%"PRIu64,
		 STOB_ID_P(&ls_cfg->lsc_stob_id), ls_cfg->lsc_size);

	ls->ls_cfg = *ls_cfg;
	ls->ls_create_mode = create_mode;

	m0_module_setup(&ls->ls_module, "m0_be_log_store",
			be_log_store_levels, ARRAY_SIZE(be_log_store_levels),
			m0_get());
}

static void be_log_store_module_fini(struct m0_be_log_store *ls,
				     bool                    destroy_mode)
{
	ls->ls_destroy_mode = destroy_mode;
	m0_module_fini(&ls->ls_module, M0_MODLEV_NONE);
}

static int be_log_store_module_init(struct m0_be_log_store     *ls,
				    struct m0_be_log_store_cfg *ls_cfg,
				    bool                        create_mode)
{
	int rc;

	m0_be_log_store_module_setup(ls, ls_cfg, create_mode);
	rc = m0_module_init(&ls->ls_module, M0_BE_LOG_STORE_LEVEL_READY);
	if (rc != 0)
		be_log_store_module_fini(ls, create_mode);
	return rc;
}

M0_INTERNAL int m0_be_log_store_open(struct m0_be_log_store     *ls,
				     struct m0_be_log_store_cfg *ls_cfg)
{
	return be_log_store_module_init(ls, ls_cfg, false);
}

M0_INTERNAL int m0_be_log_store_create(struct m0_be_log_store     *ls,
				       struct m0_be_log_store_cfg *ls_cfg)
{
	return be_log_store_module_init(ls, ls_cfg, true);
}

M0_INTERNAL void m0_be_log_store_destroy(struct m0_be_log_store *ls)
{
	be_log_store_module_fini(ls, true);
}

M0_INTERNAL void m0_be_log_store_close(struct m0_be_log_store *ls)
{
	be_log_store_module_fini(ls, false);
}

M0_INTERNAL uint32_t m0_be_log_store_bshift(struct m0_be_log_store *ls)
{
	return m0_stob_block_shift(ls->ls_stob);
}

M0_INTERNAL m0_bcount_t m0_be_log_store_buf_size(struct m0_be_log_store *ls)
{
	return ls->ls_header.fsh_cbuf_size;
}

M0_INTERNAL void m0_be_log_store_io_credit(struct m0_be_log_store *ls,
					   struct m0_be_io_credit *accum)
{
	m0_be_io_credit_add(accum, &M0_BE_IO_CREDIT(1, 0, 1));
}

M0_INTERNAL int m0_be_log_store_io_window(struct m0_be_log_store *ls,
					  m0_bindex_t             offset,
					  m0_bcount_t            *length)
{
	if (offset >= ls->ls_offset_discarded) {
		*length = ls->ls_header.fsh_cbuf_size;
		return 0;
	} else {
		return -EINVAL;
	}
}

M0_INTERNAL void m0_be_log_store_io_discard(struct m0_be_log_store *ls,
					    m0_bindex_t             offset,
					    struct m0_be_op        *op)
{
	m0_be_op_active(op);

	ls->ls_offset_discarded = max64u(offset, ls->ls_offset_discarded);

	m0_be_op_done(op);
}

static m0_bindex_t be_log_store_phys_addr(struct m0_be_log_store *ls,
					  m0_bindex_t             position)
{
	return ls->ls_header.fsh_cbuf_offset +
	       position % ls->ls_header.fsh_cbuf_size;
}

M0_INTERNAL void m0_be_log_store_io_translate(struct m0_be_log_store *ls,
					      m0_bindex_t             position,
					      struct m0_be_io        *bio)
{
	m0_bcount_t size = m0_be_io_size(bio);
	m0_bindex_t phys = be_log_store_phys_addr(ls, position);

	/* @note We support I/O to a single stob only. */
	m0_be_io_stob_assign(bio, ls->ls_stob, 0, size);
	m0_be_io_stob_move(bio, ls->ls_stob, phys,
			   ls->ls_header.fsh_cbuf_offset,
			   ls->ls_header.fsh_cbuf_size);
	m0_be_io_vec_pack(bio);
	m0_be_io_sort(bio);
}

M0_INTERNAL struct m0_buf *
m0_be_log_store_rbuf_write_buf(struct m0_be_log_store *ls)
{
	return &ls->ls_rbuf_write_buf;
}

M0_INTERNAL struct m0_buf *
m0_be_log_store_rbuf_read_buf_first(struct m0_be_log_store *ls,
				    unsigned               *iter)
{
	*iter = 0;
	return &ls->ls_rbuf_read_buf[*iter];
}

M0_INTERNAL struct m0_buf *
m0_be_log_store_rbuf_read_buf_next(struct m0_be_log_store *ls,
				   unsigned               *iter)
{
	return ++*iter == ls->ls_header.fsh_rbuf_nr ?
	       NULL : &ls->ls_rbuf_read_buf[*iter];
}

static struct m0_be_log_io *
be_log_store_rbuf_io_and_op(struct m0_be_log_store        *ls,
			    enum m0_be_log_store_io_type   io_type,
			    struct m0_be_op              **op,
			    unsigned                       index)
{
	M0_PRE(M0_IN(io_type, (M0_BE_LOG_STORE_IO_READ,
			       M0_BE_LOG_STORE_IO_WRITE)));
	if (op != NULL) {
		*op = &(io_type == M0_BE_LOG_STORE_IO_READ ?
		        ls->ls_rbuf_read_op : ls->ls_rbuf_write_op)[index];
	}
	return &(io_type == M0_BE_LOG_STORE_IO_READ ?
		 ls->ls_rbuf_read_lio : ls->ls_rbuf_write_lio)[index];
}

M0_INTERNAL struct m0_be_log_io *
m0_be_log_store_rbuf_io_first(struct m0_be_log_store        *ls,
			      enum m0_be_log_store_io_type   io_type,
			      struct m0_be_op              **op,
			      unsigned                      *iter)
{
	*iter = 0;
	return be_log_store_rbuf_io_and_op(ls, io_type, op, *iter);
}

M0_INTERNAL struct m0_be_log_io *
m0_be_log_store_rbuf_io_next(struct m0_be_log_store        *ls,
			     enum m0_be_log_store_io_type   io_type,
			     struct m0_be_op              **op,
			     unsigned                      *iter)
{
	++*iter;
	if (*iter == ls->ls_header.fsh_rbuf_nr) {
		if (op != NULL)
			*op  = NULL;
		return NULL;
	} else {
	       return be_log_store_rbuf_io_and_op(ls, io_type, op, *iter);
	}
}

M0_INTERNAL void
m0_be_log_store_rbuf_io_reset(struct m0_be_log_store       *ls,
			      enum m0_be_log_store_io_type  io_type)
{
	enum m0_stob_io_opcode opcode;
	struct m0_be_log_io   *lio;
	struct m0_be_io       *bio;
	struct m0_be_op       *op;
	struct m0_buf         *buf;
	m0_bcount_t            size;
	m0_bindex_t            offset;
	int                    i;

	opcode = io_type == M0_BE_LOG_STORE_IO_READ ? SIO_READ : SIO_WRITE;
	size   = ls->ls_header.fsh_rbuf_size_aligned;
	offset = ls->ls_header.fsh_rbuf_offset;
	for (i = 0; i < ls->ls_header.fsh_rbuf_nr; ++i) {
		if (io_type == M0_BE_LOG_STORE_IO_READ) {
			lio = &ls->ls_rbuf_read_lio[i];
			op  = &ls->ls_rbuf_read_op[i];
			buf = &ls->ls_rbuf_read_buf[i];
		} else {
			lio = &ls->ls_rbuf_write_lio[i];
			op  = &ls->ls_rbuf_write_op[i];
			buf = &ls->ls_rbuf_write_buf;
		}
		m0_be_log_io_reset(lio);
		bio = m0_be_log_io_be_io(lio);
		m0_be_io_add(bio, ls->ls_stob, buf->b_addr,
			     offset + size * i, size);
		m0_be_io_configure(bio, opcode);
		m0_be_op_reset(op);
	}
}

M0_INTERNAL bool m0_be_log_store_overwrites(struct m0_be_log_store *ls,
					    m0_bindex_t             index,
					    m0_bcount_t             size,
					    m0_bindex_t             position)
{
	m0_bindex_t end;

	end      = be_log_store_phys_addr(ls, index + size);
	index    = be_log_store_phys_addr(ls, index);
	position = be_log_store_phys_addr(ls, position);

	return (position >= index && position < end) ||
	       (index >= end && (position >= index || position < end));
}

M0_INTERNAL bool
m0_be_log_store_contains_stob(struct m0_be_log_store  *ls,
                              const struct m0_stob_id *stob_id)
{
	return m0_stob_id_eq(stob_id, m0_stob_id_get(ls->ls_stob));
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
