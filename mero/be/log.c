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
 *                  Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 4-Jul-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/log.h"
#include "be/fmt.h"
#include "be/op.h"              /* m0_be_op */
#include "be/ha.h"              /* m0_be_io_err_send */

#include "lib/arith.h"          /* m0_align */
#include "lib/errno.h"          /* ENOENT */
#include "lib/memory.h"
#include "lib/tlist.h"
#include "lib/ext.h"            /* M0_EXT */
#include "mero/magic.h"
#include "module/instance.h"    /* m0_get */

/**
 * @addtogroup be
 *
 * TODO remove lg_records list because it is not needed anymore.
 * @{
 */

/** BE log record states */
enum {
	LGR_NEW = 1,
	LGR_USED,
	LGR_SCHEDULED,
	LGR_DONE,
	LGR_DISCARDED,
	LGR_FINI,
};

static void be_log_header_update(struct m0_be_log *log);
static int  be_log_header_write(struct m0_be_log            *log,
				struct m0_be_fmt_log_header *log_hdr);

/* m0_be_log::lg_records */
M0_TL_DESCR_DEFINE(record, "be log records", static, struct m0_be_log_record,
		   lgr_linkage, lgr_magic, M0_BE_LOG_RECORD_MAGIC,
		   M0_BE_LOG_RECORD_HEAD_MAGIC);
M0_TL_DEFINE(record, static, struct m0_be_log_record);

static struct m0_be_log *be_log_module2log(struct m0_module *module)
{
	/* XXX bob_of */
	return container_of(module, struct m0_be_log, lg_module);
}

static int be_log_level_enter(struct m0_module *module)
{
	struct m0_be_recovery *rvr;
	struct m0_be_log      *log   = be_log_module2log(module);
	int                    level = module->m_cur + 1;
	int                    rc;

	switch (level) {
	case M0_BE_LOG_LEVEL_INIT:
		log->lg_current          = 0;
		log->lg_discarded        = 0;
		log->lg_reserved         = 0;
		log->lg_free             = 0;
		log->lg_prev_record      = 0;
		log->lg_prev_record_size = 0;
		log->lg_unplaced_exists  = false;
		m0_mutex_init(&log->lg_record_state_lock);
		record_tlist_init(&log->lg_records);
		m0_be_op_init(&log->lg_header_read_op);
		m0_be_op_init(&log->lg_header_write_op);
		return 0;
	case M0_BE_LOG_LEVEL_LOG_SCHED:
		return m0_be_log_sched_init(&log->lg_sched,
		                            &log->lg_cfg.lc_sched_cfg);
	case M0_BE_LOG_LEVEL_LOG_STORE:
		if (log->lg_create_mode) {
			log->lg_cfg.lc_store_cfg.lsc_rbuf_size =
				m0_be_fmt_log_header_size_max(NULL);
			rc = m0_be_log_store_create(&log->lg_store,
						    &log->lg_cfg.lc_store_cfg);
		} else {
			rc = m0_be_log_store_open(&log->lg_store,
						  &log->lg_cfg.lc_store_cfg);
		}
		return rc;
	case M0_BE_LOG_LEVEL_HEADER_PREINIT:
		return m0_be_fmt_log_header_init(&log->lg_header, NULL);
	case M0_BE_LOG_LEVEL_HEADER:
		if (log->lg_create_mode) {
			m0_be_log_header__set(&log->lg_header, 0, 0, 0);
			rc = be_log_header_write(log, &log->lg_header);
		} else {
			rc = m0_be_log_header_read(log, &log->lg_header);
		}
		return rc;
	case M0_BE_LOG_LEVEL_RECOVERY:
		if (!log->lg_create_mode) {
			log->lg_cfg.lc_recovery_cfg.brc_log = log;
			m0_be_recovery_init(&log->lg_recovery,
			                    &log->lg_cfg.lc_recovery_cfg);
			return m0_be_recovery_run(&log->lg_recovery);
		}
		return 0;
	case M0_BE_LOG_LEVEL_ASSIGNS:
		if (!log->lg_create_mode) {
			rvr = &log->lg_recovery;
			m0_be_log_pointers_set(log,
			        m0_be_recovery_current(rvr),
			        m0_be_recovery_discarded(rvr),
			        m0_be_recovery_last_record_pos(rvr),
			        m0_be_recovery_last_record_size(rvr));
		}
		log->lg_free = m0_be_log_store_buf_size(&log->lg_store);
		log->lg_external_lock = log->lg_cfg.lc_lock;
		log->lg_got_space_cb  = log->lg_cfg.lc_got_space_cb;
		return 0;
	default:
		return M0_ERR(-ENOSYS);
	}
}

static void be_log_level_leave(struct m0_module *module)
{
	struct m0_be_log *log   = be_log_module2log(module);
	int               level = module->m_cur;
	int               rc;

	switch (level) {
	case M0_BE_LOG_LEVEL_INIT:
		m0_be_op_fini(&log->lg_header_write_op);
		m0_be_op_fini(&log->lg_header_read_op);
		record_tlist_fini(&log->lg_records);
		m0_mutex_fini(&log->lg_record_state_lock);
		break;
	case M0_BE_LOG_LEVEL_LOG_SCHED:
		m0_be_log_sched_fini(&log->lg_sched);
		break;
	case M0_BE_LOG_LEVEL_LOG_STORE:
		if (log->lg_destroy_mode)
			m0_be_log_store_destroy(&log->lg_store);
		else
			m0_be_log_store_close(&log->lg_store);
		break;
	case M0_BE_LOG_LEVEL_HEADER_PREINIT:
		m0_be_fmt_log_header_fini(&log->lg_header);
		break;
	case M0_BE_LOG_LEVEL_HEADER:
		if (!log->lg_destroy_mode) {
			be_log_header_update(log);
			rc = be_log_header_write(log, &log->lg_header);
			M0_ASSERT_INFO(rc == 0, "rc=%d", rc); /* XXX */
		}
		break;
	case M0_BE_LOG_LEVEL_RECOVERY:
		if (!log->lg_create_mode)
			m0_be_recovery_fini(&log->lg_recovery);
		break;
	case M0_BE_LOG_LEVEL_ASSIGNS:
		break;
	default:
		M0_IMPOSSIBLE("Unexpected m0_module level");
	}
}

static const struct m0_modlev be_log_levels[] = {
	[M0_BE_LOG_LEVEL_INIT] = {
		.ml_name  = "M0_BE_LOG_LEVEL_INIT",
		.ml_enter = be_log_level_enter,
		.ml_leave = be_log_level_leave,
	},
	[M0_BE_LOG_LEVEL_LOG_SCHED] = {
		.ml_name  = "M0_BE_LOG_LEVEL_LOG_SCHED",
		.ml_enter = be_log_level_enter,
		.ml_leave = be_log_level_leave,
	},
	[M0_BE_LOG_LEVEL_LOG_STORE] = {
		.ml_name  = "M0_BE_LOG_LEVEL_LOG_STORE",
		.ml_enter = be_log_level_enter,
		.ml_leave = be_log_level_leave,
	},
	[M0_BE_LOG_LEVEL_HEADER_PREINIT] = {
		.ml_name  = "M0_BE_LOG_LEVEL_HEADER_PREINIT",
		.ml_enter = be_log_level_enter,
		.ml_leave = be_log_level_leave,
	},
	[M0_BE_LOG_LEVEL_HEADER] = {
		.ml_name  = "M0_BE_LOG_LEVEL_HEADER",
		.ml_enter = be_log_level_enter,
		.ml_leave = be_log_level_leave,
	},
	[M0_BE_LOG_LEVEL_RECOVERY] = {
		.ml_name  = "M0_BE_LOG_LEVEL_RECOVERY",
		.ml_enter = be_log_level_enter,
		.ml_leave = be_log_level_leave,
	},
	[M0_BE_LOG_LEVEL_ASSIGNS] = {
		.ml_name  = "M0_BE_LOG_LEVEL_ASSIGNS",
		.ml_enter = be_log_level_enter,
		.ml_leave = be_log_level_leave,
	},
	[M0_BE_LOG_LEVEL_READY] = {
		.ml_name = "fully initialized",
	},
};

M0_INTERNAL void m0_be_log_module_setup(struct m0_be_log     *log,
				        struct m0_be_log_cfg *lg_cfg,
				        bool                  create_mode)
{
	log->lg_cfg = *lg_cfg;
	log->lg_create_mode = create_mode;

	m0_module_setup(&log->lg_module, "m0_be_log",
			be_log_levels, ARRAY_SIZE(be_log_levels), m0_get());
}

static void be_log_module_fini(struct m0_be_log *log,
			       bool              destroy_mode)
{
	M0_PRE(log->lg_reserved == 0);
	M0_PRE(ergo(!log->lg_unplaced_exists,
		    log->lg_current == log->lg_discarded));
	M0_PRE(ergo(log->lg_unplaced_exists,
		    log->lg_discarded == log->lg_unplaced_pos));

	log->lg_destroy_mode = destroy_mode;
	m0_module_fini(&log->lg_module, M0_MODLEV_NONE);
}

static int be_log_module_init(struct m0_be_log     *log,
			      struct m0_be_log_cfg *log_cfg,
			      bool                  create_mode)
{
	int rc;

	m0_be_log_module_setup(log, log_cfg, create_mode);
	rc = m0_module_init(&log->lg_module, M0_BE_LOG_LEVEL_READY);
	if (rc != 0)
		be_log_module_fini(log, create_mode);
	return rc;
}

M0_INTERNAL bool m0_be_log__invariant(struct m0_be_log *log)
{
	return _0C(m0_mutex_is_locked(log->lg_external_lock)) &&
	       _0C(log->lg_discarded <= log->lg_current) &&
	       _0C(ergo(log->lg_unplaced_exists,
			log->lg_discarded <= log->lg_unplaced_pos));
}

M0_INTERNAL int m0_be_log_open(struct m0_be_log     *log,
			       struct m0_be_log_cfg *log_cfg)
{
	return be_log_module_init(log, log_cfg, false);
}

M0_INTERNAL void m0_be_log_close(struct m0_be_log *log)
{
	be_log_module_fini(log, false);
}

M0_INTERNAL int m0_be_log_create(struct m0_be_log     *log,
				 struct m0_be_log_cfg *log_cfg)
{
	return be_log_module_init(log, log_cfg, true);
}

M0_INTERNAL void m0_be_log_destroy(struct m0_be_log *log)
{
	be_log_module_fini(log, true);
}

M0_INTERNAL void m0_be_log_pointers_set(struct m0_be_log *log,
					m0_bindex_t       current_ptr,
					m0_bindex_t       discarded_ptr,
					m0_bindex_t       last_record_pos,
					m0_bcount_t       last_record_size)
{
	/* TODO PRE: log hasn't been used */

	log->lg_current          = current_ptr;
	log->lg_discarded        = discarded_ptr;
	log->lg_prev_record      = last_record_pos;
	log->lg_prev_record_size = last_record_size;

	M0_LOG(M0_DEBUG, "lg_current=%"PRIu64" lg_discarded=%"PRIu64" "
	       "lg_prev_record=%"PRIu64" lg_prev_record_size=%"PRIu64,
	       log->lg_current, log->lg_discarded,
	       log->lg_prev_record, log->lg_prev_record_size);
}

static void be_log_header_io(struct m0_be_log             *log,
			     enum m0_be_log_store_io_type  io_type,
			     struct m0_be_op              *op)
{
	struct m0_be_log_io *lio;
	struct m0_be_op     *io_op;
	unsigned             iter;

	M0_PRE(m0_be_log_sched_is_locked(&log->lg_sched));

	lio = m0_be_log_store_rbuf_io_first(&log->lg_store, io_type,
					    &io_op, &iter);
	/* log should have at least one header */
	do {
		/*
		 * It is safe to add io_op to op set here.
		 * log_sched can't finish I/O when it's locked.
		 */
		m0_be_op_set_add(op, io_op);
		m0_be_log_sched_add(&log->lg_sched, lio, io_op);
		lio = m0_be_log_store_rbuf_io_next(&log->lg_store, io_type,
						   &io_op, &iter);
	} while (lio != NULL);
}

static void be_log_header_io_sync(struct m0_be_log             *log,
				  enum m0_be_log_store_io_type  io_type)
{
	struct m0_be_op *op;

	M0_PRE(M0_IN(io_type, (M0_BE_LOG_STORE_IO_WRITE,
			       M0_BE_LOG_STORE_IO_READ)));
	op = io_type == M0_BE_LOG_STORE_IO_WRITE ? &log->lg_header_write_op :
						   &log->lg_header_read_op;
	m0_be_op_reset(op);
	m0_be_log_sched_lock(&log->lg_sched);
	be_log_header_io(log, io_type, op);
	m0_be_log_sched_unlock(&log->lg_sched);
	m0_be_op_wait(op);
}

static m0_bcount_t be_log_record_header_size(void)
{
	struct m0_be_fmt_log_record_header_cfg cfg = {
		.lrhc_io_nr_max = M0_BE_LOG_RECORD_IO_NR_MAX,
	};

	return m0_be_fmt_log_record_header_size_max(&cfg);
}

static m0_bcount_t be_log_record_footer_size(void)
{
	return m0_be_fmt_log_record_footer_size_max(NULL);
}

M0_INTERNAL m0_bcount_t m0_be_log_reserved_size(struct m0_be_log *log,
						m0_bcount_t      *lio_size,
						int               lio_nr)
{
	m0_bcount_t size = 0;
	m0_bcount_t part;
	uint64_t    alignment = 1ULL << m0_be_log_bshift(log);
	int         i;

	M0_PRE(lio_nr > 0);

	for (i = 0; i < lio_nr; ++i) {
		part = lio_size[i];
		if (i == 0)
			part += be_log_record_header_size();
		if (i == lio_nr - 1)
			part += be_log_record_footer_size();
		size += m0_align(part, alignment);
	}
	return size;
}

static void be_log_record_io_done_cb(struct m0_be_op *op, void *param)
{
	struct m0_be_log_record *record = param;
	struct m0_be_log        *log    = record->lgr_log;

	m0_mutex_lock(&log->lg_record_state_lock);
	record->lgr_state = LGR_DONE;
	m0_mutex_unlock(&log->lg_record_state_lock);
}

M0_INTERNAL void m0_be_log_record_init(struct m0_be_log_record *record,
				       struct m0_be_log *log)
{
	int i;

	record->lgr_log          = log;
	record->lgr_size         = 0;
	record->lgr_state        = LGR_NEW;
	record->lgr_need_discard = false;

	record->lgr_io_nr = 0;
	for (i = 0; i < M0_BE_LOG_RECORD_IO_NR_MAX; ++i) {
		record->lgr_io[i] = NULL;
		record->lgr_op[i] = NULL;
	}
	m0_be_op_init(&record->lgr_record_op);
	m0_be_op_callback_set(&record->lgr_record_op, &be_log_record_io_done_cb,
	                      record, M0_BOS_DONE);
}

M0_INTERNAL void m0_be_log_record_fini(struct m0_be_log_record *record)
{
	int i;

	M0_PRE(M0_IN(record->lgr_state, (LGR_NEW, LGR_DONE)));

	if (record->lgr_need_discard && record->lgr_state == LGR_DONE)
		record_tlink_del_fini(record);

	m0_be_op_fini(&record->lgr_record_op);
	for (i = 0; i < record->lgr_io_nr; ++i) {
		m0_be_op_fini(record->lgr_op[i]);
		m0_free(record->lgr_op[i]);
		m0_be_log_io_deallocate(record->lgr_io[i]);
		m0_be_log_io_fini(record->lgr_io[i]);
		m0_free(record->lgr_io[i]);
	}
	record->lgr_io_nr = 0;

	record->lgr_state = LGR_FINI;
	record->lgr_log   = NULL;
}

M0_INTERNAL void m0_be_log_record_reset(struct m0_be_log_record *record)
{
	int i;

	M0_PRE(M0_IN(record->lgr_state, (LGR_NEW, LGR_DONE)));

	/*
	 * With delayed discard records can't be removed from the list
	 * in m0_be_log_record_discard().
	 */
	if (record->lgr_need_discard && record->lgr_state == LGR_DONE) {
		/*
		 * XXX the lock shouldn't be taken here. Usually it's engine
		 * lock and it should be taken somewhere above in the call
		 * stack.
		 */
		m0_mutex_lock(record->lgr_log->lg_external_lock);
		record_tlink_del_fini(record);
		m0_mutex_unlock(record->lgr_log->lg_external_lock);
	}
	m0_be_fmt_log_record_header_reset(&record->lgr_header);
	m0_be_fmt_log_record_footer_reset(&record->lgr_footer);
	m0_be_op_reset(&record->lgr_record_op);
	for (i = 0; i < record->lgr_io_nr; ++i) {
		m0_be_log_io_reset(record->lgr_io[i]);
		m0_be_op_reset(record->lgr_op[i]);
	}
	record->lgr_state        = LGR_NEW;
	record->lgr_need_discard = false;
	record->lgr_write_header = false;
}

M0_INTERNAL void
m0_be_log_record_assign(struct m0_be_log_record      *record,
			struct m0_be_log_record_iter *iter,
			bool                          need_discard)
{
	struct m0_be_fmt_log_record_header *header = &iter->lri_header;
	struct m0_be_log                   *log    = record->lgr_log;
	int                                 i;

	M0_ENTRY("iter->header=" BFLRH_F, BFLRH_P(header));
	M0_PRE(header->lrh_io_size.lrhs_nr == record->lgr_io_nr);

	record->lgr_last_discarded = header->lrh_discarded;
	record->lgr_position       = header->lrh_pos;
	record->lgr_prev_pos       = header->lrh_prev_pos;
	record->lgr_prev_size      = header->lrh_prev_size;
	record->lgr_size           = header->lrh_size;
	record->lgr_state          = LGR_USED;
	record->lgr_need_discard   = need_discard;
	for (i = 0; i < header->lrh_io_size.lrhs_nr; ++i) {
		m0_be_log_record_io_size_set(record, i,
					     header->lrh_io_size.lrhs_size[i]);
	}
	if (need_discard)
		record_tlink_init_at_tail(record, &log->lg_records);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_log_record_ext(struct m0_be_log_record *record,
                                      struct m0_ext           *ext)
{
	M0_ASSERT(M0_IN(record->lgr_state,
			(LGR_USED, LGR_SCHEDULED, LGR_DONE)));
	*ext = M0_EXT(record->lgr_position,
	              record->lgr_position + record->lgr_size);
}

M0_INTERNAL void m0_be_log_record_skip_discard(struct m0_be_log_record *record)
{
	struct m0_be_log *log = record->lgr_log;

	M0_PRE(record->lgr_need_discard);
	M0_PRE(m0_mutex_is_locked(log->lg_external_lock));

	if (!log->lg_unplaced_exists ||
	    record->lgr_position < log->lg_unplaced_pos) {
		log->lg_unplaced_exists = true;
		log->lg_unplaced_pos    = record->lgr_position;
		log->lg_unplaced_size   = record->lgr_size;
	}
	M0_POST(m0_be_log__invariant(log));
}

M0_INTERNAL void m0_be_log_record_discard(struct m0_be_log *log,
					  m0_bcount_t       size)
{
	M0_PRE(m0_mutex_is_locked(log->lg_external_lock));
	M0_PRE(log->lg_discarded + size <= log->lg_current);
	M0_PRE(ergo(log->lg_unplaced_exists,
		    log->lg_discarded == log->lg_unplaced_pos ||
		    log->lg_discarded + size <= log->lg_unplaced_pos));

	/*
	 * User must guarantee discarding of records in the same order as
	 * they are prepared for I/O.
	 */

	if (log->lg_unplaced_exists &&
	    log->lg_discarded == log->lg_unplaced_pos) {
		size = 0;
	}
	M0_LOG(M0_DEBUG, "%lu", size);
	log->lg_free      += size;
	log->lg_discarded += size;

	M0_POST(m0_be_log__invariant(log));

	log->lg_got_space_cb(log);
}

static void be_log_io_credit(struct m0_be_log       *log,
			     struct m0_be_io_credit *accum)
{
	m0_be_log_store_io_credit(&log->lg_store, accum);
}

M0_INTERNAL int m0_be_log_record_io_create(struct m0_be_log_record *record,
					   m0_bcount_t              size_max)
{
	struct m0_be_io_credit iocred;
	struct m0_be_log_io   *lio;
	struct m0_be_log      *log    = record->lgr_log;
	uint32_t               bshift = m0_be_log_bshift(log);
	int                    index  = record->lgr_io_nr;
	int                    rc     = 0;

	M0_ENTRY("record=%p index=%d size_max=%"PRIu64,
		 record, index, size_max);

	if (index >= M0_BE_LOG_RECORD_IO_NR_MAX)
		return -ERANGE;

	/* We don't know which I/O is the last, therefore reserve footer size to
	 * all I/O.
	 */
	size_max += index == 0 ? be_log_record_header_size() :
				 be_log_record_footer_size();
	size_max  = m0_align(size_max, 1ULL << bshift);
	iocred    = M0_BE_IO_CREDIT(1, size_max, 1);
	be_log_io_credit(log, &iocred);

	M0_ALLOC_PTR(record->lgr_io[index]);
	M0_ALLOC_PTR(record->lgr_op[index]);
	if (record->lgr_io[index] == NULL || record->lgr_op[index] == NULL)
		rc = -ENOMEM;

	lio = record->lgr_io[index];
	rc  = rc ?: m0_be_log_io_init(lio);
	if (rc == 0) {
		rc = m0_be_log_io_allocate(lio, &iocred, bshift);
		if (rc != 0)
			m0_be_log_io_fini(lio);
	}

	if (rc == 0) {
		m0_be_op_init(record->lgr_op[index]);
		lio->lio_record = record;
		++record->lgr_io_nr;
	} else {
		m0_free(record->lgr_io[index]);
		m0_free(record->lgr_op[index]);
	}
	return M0_RC(rc);
}

static int be_log_record_header_init(struct m0_be_fmt_log_record_header *hdr)
{
	struct m0_be_fmt_log_record_header_cfg cfg = {
		.lrhc_io_nr_max = M0_BE_LOG_RECORD_IO_NR_MAX,
	};
	return m0_be_fmt_log_record_header_init(hdr, &cfg);
}

M0_INTERNAL int m0_be_log_record_allocate(struct m0_be_log_record *record)
{
	int rc;

	rc = be_log_record_header_init(&record->lgr_header);
	if (rc != 0)
		return rc;

	rc = m0_be_fmt_log_record_footer_init(&record->lgr_footer, NULL);
	if (rc != 0)
		m0_be_fmt_log_record_header_fini(&record->lgr_header);

	return rc;
}

M0_INTERNAL void m0_be_log_record_deallocate(struct m0_be_log_record *record)
{
	m0_be_fmt_log_record_header_fini(&record->lgr_header);
	m0_be_fmt_log_record_footer_fini(&record->lgr_footer);
}

M0_INTERNAL void m0_be_log_record_io_size_set(struct m0_be_log_record *record,
					      int                      index,
					      m0_bcount_t              size)
{
	struct m0_be_log_io *lio;

	M0_PRE(index < record->lgr_io_nr);

	lio               = record->lgr_io[index];
	lio->lio_buf_size = size;
	lio->lio_buf_addr = lio->lio_buf.b_addr;
	if (index == 0) {
		/*
		 * This is first lio, so we need to reserve a window for
		 * log record header.
		 */
		lio->lio_buf_addr = (char *)lio->lio_buf_addr +
				    be_log_record_header_size();
	}
	lio->lio_bufvec = M0_BUFVEC_INIT_BUF(&lio->lio_buf_addr,
					     &lio->lio_buf_size);
}

M0_INTERNAL void
m0_be_log_record_io_prepare(struct m0_be_log_record *record,
			    enum m0_stob_io_opcode   opcode,
			    m0_bcount_t              size_reserved)
{
	struct m0_bufvec_cursor  cur    = {};
	struct m0_bufvec         bvec;
	struct m0_be_log_io     *lio;
	struct m0_be_log        *log    = record->lgr_log;
	struct m0_buf           *buf;
	m0_bcount_t              size   = 0;
	m0_bcount_t              size_lio;
	m0_bcount_t              size_lio_aligned;
	m0_bcount_t              size_fmt;
	uint32_t                 bshift = m0_be_log_bshift(log);
	uint64_t                 align  = 1ULL << bshift;
	void                    *addr_fmt;
	void                    *addr_zero;
	int                      rc;
	int                      i;

	struct m0_be_fmt_log_record_footer *footer;
	struct m0_be_fmt_log_record_header *header;

	M0_ENTRY("record=%p opcode=%d size_reserved=%lu",
	         record, opcode, size_reserved);

	M0_PRE(m0_mutex_is_locked(log->lg_external_lock));
	M0_PRE(ergo(opcode == SIO_READ, record->lgr_state == LGR_USED));
	M0_PRE(ergo(opcode == SIO_WRITE, record->lgr_state == LGR_NEW));
	M0_PRE(record->lgr_io_nr > 0);

	if (opcode == SIO_WRITE) {
		record->lgr_state          = LGR_USED;
		record->lgr_need_discard   = true;
		record->lgr_position       = log->lg_current;
		record->lgr_prev_pos       = log->lg_prev_record;
		record->lgr_prev_size      = log->lg_prev_record_size;
		record->lgr_last_discarded = log->lg_discarded;
		record_tlink_init_at_tail(record, &log->lg_records);
	}

	for (i = 0; i < record->lgr_io_nr; ++i) {
		lio       = record->lgr_io[i];
		size_lio  = lio->lio_buf_size;
		size_lio += i == 0 ? be_log_record_header_size() : 0;
		addr_zero = (char*)lio->lio_buf.b_addr + size_lio;
		size_lio += i == record->lgr_io_nr - 1 ?
			    be_log_record_footer_size() : 0;

		/* fill padding with 0xCC */
		size_lio_aligned = m0_align(size_lio, align);
		memset(addr_zero, 0xCC, size_lio_aligned - size_lio);
		size_lio = size_lio_aligned;

		m0_be_io_add_nostob(&lio->lio_be_io, lio->lio_buf.b_addr,
				    0, size_lio);
		m0_be_log_store_io_translate(&log->lg_store,
					     record->lgr_position + size,
					     &lio->lio_be_io);
		m0_be_io_configure(&lio->lio_be_io, opcode);
		size += size_lio;
	}

	if (opcode == SIO_WRITE) {
		M0_ASSERT(size <= size_reserved);
		log->lg_current         += size;
		log->lg_free            += size_reserved - size;
		log->lg_reserved        -= size_reserved;
		log->lg_prev_record      = record->lgr_position;
		log->lg_prev_record_size = size;
		record->lgr_size         = size;

		if (size_reserved != size)
			log->lg_got_space_cb(log);

		/* log record header */
		lio    = record->lgr_io[0];
		header = &record->lgr_header;
		for (i = 0; i < record->lgr_io_nr; ++i) {
			m0_be_fmt_log_record_header_io_size_add(header,
					record->lgr_io[i]->lio_buf_size);
		}
		header->lrh_pos       = record->lgr_position;
		header->lrh_size      = size;
		header->lrh_discarded = record->lgr_last_discarded;
		header->lrh_prev_pos  = record->lgr_prev_pos;
		header->lrh_prev_size = record->lgr_prev_size;
		size_fmt = m0_be_fmt_log_record_header_size(header);
		bvec     = M0_BUFVEC_INIT_BUF(&lio->lio_buf.b_addr, &size_fmt);
		m0_bufvec_cursor_init(&cur, &bvec);
		m0_be_fmt_log_record_header_encode(header, &cur);

		/* log record footer */
		lio    = record->lgr_io[record->lgr_io_nr - 1];
		footer = &record->lgr_footer;
		footer->lrf_pos = record->lgr_position;
		size_fmt        = m0_be_fmt_log_record_footer_size(footer);
		addr_fmt        = (char *)lio->lio_buf.b_addr +
				  m0_align(lio->lio_buf_size + size_fmt, align) -
				  size_fmt;
		bvec            = M0_BUFVEC_INIT_BUF(&addr_fmt, &size_fmt);
		m0_bufvec_cursor_init(&cur, &bvec);
		m0_be_fmt_log_record_footer_encode(footer, &cur);
	}

	if (opcode == SIO_WRITE &&
	    m0_be_log_store_overwrites(&log->lg_store, record->lgr_position,
				       size, log->lg_header.flh_group_lsn)) {
		m0_be_log_store_rbuf_io_reset(&log->lg_store,
					      M0_BE_LOG_STORE_IO_WRITE);
		be_log_header_update(log);
		buf = m0_be_log_store_rbuf_write_buf(&log->lg_store);
		rc  = m0_be_fmt_log_header_encode_buf(&log->lg_header, buf);
		M0_ASSERT_INFO(rc == 0, "rc=%d", rc); /* XXX */
		record->lgr_write_header = true;
	}

	/*
	 * Size is part of the log that is not held by records. Notify user
	 * if it less than threshold.
	 */
	size = m0_be_log_store_buf_size(&log->lg_store) -
	       (log->lg_current - log->lg_discarded);
	if (size <= log->lg_cfg.lc_full_threshold &&
	    log->lg_cfg.lc_full_cb != NULL)
		log->lg_cfg.lc_full_cb(log);
}

M0_INTERNAL void m0_be_log_record_io_launch(struct m0_be_log_record *record,
					    struct m0_be_op         *op)
{
	struct m0_be_log *log = record->lgr_log;
	struct m0_be_op   op2 = {};
	int               i;

	record->lgr_state = LGR_SCHEDULED;
	m0_be_log_sched_lock(&log->lg_sched);
	m0_be_op_set_add(op, &record->lgr_record_op);

	/* XXX Move op to ACTIVE state. Workaround for several tx_groups. */
	m0_be_op_init(&op2);
	m0_be_op_set_add(op, &op2);
	m0_be_op_active(&op2);
	m0_be_op_done(&op2);
	m0_be_op_fini(&op2);

	if (record->lgr_write_header) {
		m0_be_op_reset(&log->lg_header_write_op);
		m0_be_op_set_add(&record->lgr_record_op,
				 &log->lg_header_write_op);
		be_log_header_io(log, M0_BE_LOG_STORE_IO_WRITE,
				 &log->lg_header_write_op);
	}
	for (i = 0; i < record->lgr_io_nr; ++i) {
		m0_be_op_set_add(&record->lgr_record_op,
				 record->lgr_op[i]);
		m0_be_log_sched_add(&log->lg_sched,
				    record->lgr_io[i], record->lgr_op[i]);
	}
	m0_be_log_sched_unlock(&log->lg_sched);
}

M0_INTERNAL struct m0_bufvec *
m0_be_log_record_io_bufvec(struct m0_be_log_record *record,
			   int                      index)
{
	M0_PRE(index < record->lgr_io_nr);
	return m0_be_log_io_bufvec(record->lgr_io[index]);
}

M0_INTERNAL int m0_be_log_reserve(struct m0_be_log *log, m0_bcount_t size)
{
	int rc;

	M0_ENTRY("log=%p size=%lu lg_free=%lu", log, size, log->lg_free);

	M0_PRE(m0_be_log__invariant(log));

	if (log->lg_free < size)  {
		rc = -EAGAIN;
	} else {
		log->lg_free     -= size;
		log->lg_reserved += size;
		rc = 0;
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_log_unreserve(struct m0_be_log *log, m0_bcount_t size)
{
	M0_PRE(m0_be_log__invariant(log));
	M0_PRE(log->lg_reserved >= size);

	log->lg_free     += size;
	log->lg_reserved -= size;

	log->lg_got_space_cb(log);
}

M0_INTERNAL uint32_t m0_be_log_bshift(struct m0_be_log *log)
{
	return m0_be_log_store_bshift(&log->lg_store);
}

M0_INTERNAL void m0_be_log_header__set(struct m0_be_fmt_log_header *hdr,
				       m0_bindex_t                  discarded,
				       m0_bindex_t                  lsn,
				       m0_bcount_t                  size)
{
	M0_ENTRY("discarded=%lu lsn=%lu size=%lu", discarded, lsn, size);

	m0_be_fmt_log_header_reset(hdr);
	hdr->flh_discarded  = discarded;
	hdr->flh_group_lsn  = lsn;
	hdr->flh_group_size = size;
}

M0_INTERNAL bool m0_be_log_header__is_eq(struct m0_be_fmt_log_header *hdr1,
					 struct m0_be_fmt_log_header *hdr2)
{
	return hdr1->flh_discarded  == hdr2->flh_discarded &&
	       hdr1->flh_group_lsn  == hdr2->flh_group_lsn &&
	       hdr1->flh_group_size == hdr2->flh_group_size;
}

static void be_log_header_update(struct m0_be_log *log)
{
	struct m0_be_log_record *record;
	m0_bindex_t              index = log->lg_prev_record;
	m0_bcount_t              size  = log->lg_prev_record_size;

	m0_mutex_lock(&log->lg_record_state_lock);
	record = record_tlist_head(&log->lg_records);
	/* this condition also handles the first record in log. */
	if (record != NULL && record->lgr_state != LGR_DONE &&
	    record->lgr_position != 0) {
		index = record->lgr_prev_pos;
		size  = record->lgr_prev_size;
	}
	while (record != NULL && record->lgr_state == LGR_DONE) {
		index  = record->lgr_position;
		size   = record->lgr_size;
		record = record_tlist_next(&log->lg_records, record);
	}
	m0_mutex_unlock(&log->lg_record_state_lock);

	if (log->lg_unplaced_exists && log->lg_unplaced_pos < index) {
		index = log->lg_unplaced_pos;
		size  = log->lg_unplaced_size;
	}
	m0_be_log_header__set(&log->lg_header, log->lg_discarded, index, size);
}

static int be_log_header_write(struct m0_be_log            *log,
			       struct m0_be_fmt_log_header *log_hdr)
{
	struct m0_buf *buf;
	int            rc;

	m0_be_log_store_rbuf_io_reset(&log->lg_store, M0_BE_LOG_STORE_IO_WRITE);
	buf = m0_be_log_store_rbuf_write_buf(&log->lg_store);
	rc  = m0_be_fmt_log_header_encode_buf(log_hdr, buf);
	if (rc == 0)
		be_log_header_io_sync(log, M0_BE_LOG_STORE_IO_WRITE);
	return M0_RC(rc);
}

M0_INTERNAL bool m0_be_log_header__repair(struct m0_be_fmt_log_header **hdrs,
					  int                           nr,
					  struct m0_be_fmt_log_header  *out)
{
	bool need_repair = false;
	int  i           = 0;

	M0_PRE(nr == 3);

	if (!m0_be_log_header__is_eq(hdrs[0], hdrs[1])) {
		i = 2;
		need_repair = true;
	}
	m0_be_log_header__set(out, hdrs[i]->flh_discarded,
				   hdrs[i]->flh_group_lsn,
				   hdrs[i]->flh_group_size);

	return need_repair || !m0_be_log_header__is_eq(hdrs[0], hdrs[2]);
}

M0_INTERNAL int m0_be_log_header_read(struct m0_be_log            *log,
				      struct m0_be_fmt_log_header *log_hdr)
{
	struct m0_be_fmt_log_header *hdrs[3] = {};
	struct m0_buf               *buf;
	unsigned                     iter;
	bool                         need_repair;
	int                          rc = 0;
	int                          i;

	/*
	 * This function always returns a valid log header and restores it
	 * if needed. To achieve this log stores redundant copies of log header.
	 * Recovery algorithm for log header:
	 *   1. Read 3 copies of log header
	 *   2. Compare the log headers
	 *   3. If 1st and 2nd are equal then 1st is valid
	 *   4. Else 3rd header is valid
	 *   5. If all 3 headers are not equal then write valid header
	 *   6. Return valid header to user
	 * Configuration must guarantee that we have at least 3 rbufs for log
	 * headers.
	 * The above algorithm doesn't work in case of stob corruption.
	 * This function recovers log header after multiple fails during
	 * previous recoveries.
	 */

	m0_be_log_store_rbuf_io_reset(&log->lg_store, M0_BE_LOG_STORE_IO_READ);
	be_log_header_io_sync(log, M0_BE_LOG_STORE_IO_READ);
	buf = m0_be_log_store_rbuf_read_buf_first(&log->lg_store, &iter);
	for (i = 0; rc == 0 && buf != NULL && i < ARRAY_SIZE(hdrs); ++i) {
		rc  = m0_be_fmt_log_header_decode_buf(&hdrs[i], buf,
						M0_BE_FMT_DECODE_CFG_DEFAULT);
		buf = m0_be_log_store_rbuf_read_buf_next(&log->lg_store, &iter);
	}
	if (rc == 0) {
		need_repair = m0_be_log_header__repair(hdrs, i, log_hdr);
		if (need_repair)
			be_log_header_write(log, log_hdr);
	}

	for (i = 0; i < ARRAY_SIZE(hdrs); ++i) {
		if (hdrs[i] != NULL)
			m0_be_fmt_log_header_decoded_free(hdrs[i]);
	}
	return rc;
}

static int be_log_read_plain(struct m0_be_log *log,
			     m0_bindex_t       pos,
			     m0_bcount_t       size,
			     void             *out)
{
	struct m0_be_io        bio    = {};
	struct m0_be_io_credit iocred = M0_BE_IO_CREDIT(1, size, 1);
	int                    rc;

	be_log_io_credit(log, &iocred);
	rc = m0_be_io_init(&bio);
	if (rc != 0)
		goto out;
	rc = m0_be_io_allocate(&bio, &iocred);
	if (rc == 0) {
		m0_be_io_add_nostob(&bio, out, 0, size);
		m0_be_log_store_io_translate(&log->lg_store, pos, &bio);
		m0_be_io_configure(&bio, SIO_READ);
		rc = M0_BE_OP_SYNC_RET(op, m0_be_io_launch(&bio, &op),
				       bo_sm.sm_rc);
		m0_be_io_deallocate(&bio);
	}
	m0_be_io_fini(&bio);
out:
	if (rc != 0)
		m0_be_io_err_send(-rc, M0_BE_LOC_LOG, SIO_READ);
	return rc;
}

M0_INTERNAL bool m0_be_fmt_log_record_header__invariant(
				struct m0_be_fmt_log_record_header *header,
				struct m0_be_log                   *log)
{
	m0_bindex_t pos       = header->lrh_pos;
	m0_bcount_t size      = header->lrh_size;
	m0_bindex_t prev      = header->lrh_prev_pos;
	m0_bindex_t discarded = header->lrh_discarded;
	m0_bcount_t hsize     = be_log_record_header_size();
	m0_bcount_t fsize     = be_log_record_footer_size();
	uint64_t    alignment = 1ULL << m0_be_log_bshift(log);

	return ergo(pos == 0, prev == 0 && discarded == 0) &&
	       ergo(pos != 0, prev < pos && discarded <= pos) &&
	       size >= m0_align(hsize + fsize, alignment) &&
//	       pos + size <= log->lg_current &&
	       m0_is_aligned(pos, alignment) &&
	       m0_is_aligned(prev, alignment) &&
	       m0_is_aligned(discarded, alignment) &&
	       m0_is_aligned(size, alignment);
}

static void be_log_record_header_copy(struct m0_be_fmt_log_record_header *dest,
				      struct m0_be_fmt_log_record_header *src)
{
	int i;

	M0_PRE(dest->lrh_io_nr_max >= src->lrh_io_size.lrhs_nr);

	dest->lrh_pos       = src->lrh_pos;
	dest->lrh_size      = src->lrh_size;
	dest->lrh_discarded = src->lrh_discarded;
	dest->lrh_prev_pos  = src->lrh_prev_pos;
	dest->lrh_prev_size = src->lrh_prev_size;
	for (i = 0; i < src->lrh_io_size.lrhs_nr; ++i)
		dest->lrh_io_size.lrhs_size[i] = src->lrh_io_size.lrhs_size[i];
	dest->lrh_io_size.lrhs_nr = src->lrh_io_size.lrhs_nr;
}

static int be_log_record_iter_read(struct m0_be_log             *log,
				   struct m0_be_log_record_iter *iter,
				   m0_bindex_t                   pos)
{
	struct m0_be_fmt_log_record_footer *footer;
	struct m0_be_fmt_log_record_header *header;
	struct m0_bufvec_cursor             cur;
	struct m0_bufvec                    bvec;
	m0_bcount_t                         size_fmt;
	m0_bcount_t                         size;
	uint32_t                            bshift = m0_be_log_bshift(log);
	uint64_t                            align  = 1ULL << bshift;
	char                               *data;
	void                               *addr_fmt;
	int                                 rc;

	M0_PRE(m0_is_aligned(pos, align));

	size_fmt = be_log_record_header_size();
	size     = m0_align(size_fmt + (pos & (align - 1)), align);
	data     = m0_alloc_aligned(size, bshift);
	if (data == NULL)
		return -ENOMEM;
	addr_fmt = data + (pos & (align - 1));
	bvec     = M0_BUFVEC_INIT_BUF(&addr_fmt, &size_fmt);
	m0_bufvec_cursor_init(&cur, &bvec);
	pos &= ~((m0_bindex_t)align - 1);
	rc   = be_log_read_plain(log, pos, size, data);
	rc   = rc ?: m0_be_fmt_log_record_header_decode(&header, &cur,
						M0_BE_FMT_DECODE_CFG_DEFAULT);
	if (rc == -EPROTO)
		rc = -ENOENT;

	m0_free_aligned(data, size, bshift);
	if (rc != 0)
		return rc;

	rc = m0_be_fmt_log_record_header__invariant(header, log) ? 0 : -ENOENT;
	if (rc == 0) {
		size_fmt = be_log_record_footer_size();
		size     = m0_align(size_fmt, align);
		data     = m0_alloc_aligned(size, bshift);
		addr_fmt = data + size - size_fmt;
		bvec     = M0_BUFVEC_INIT_BUF(&addr_fmt, &size_fmt);
		m0_bufvec_cursor_init(&cur, &bvec);
		rc = be_log_read_plain(log, pos + header->lrh_size - size,
				       size, data);
		rc = rc ?: m0_be_fmt_log_record_footer_decode(&footer, &cur,
					      M0_BE_FMT_DECODE_CFG_DEFAULT);
		if (rc == 0) {
			rc = header->lrh_pos == footer->lrf_pos ? 0 : -ENOENT;
			m0_be_fmt_log_record_footer_decoded_free(footer);
		}
		m0_free_aligned(data, size, bshift);
		if (rc == 0)
			be_log_record_header_copy(&iter->lri_header, header);
	}
	m0_be_fmt_log_record_header_decoded_free(header);

	return rc;
}

M0_INTERNAL int m0_be_log_record_iter_init(struct m0_be_log_record_iter *iter)
{
	return be_log_record_header_init(&iter->lri_header);
}

M0_INTERNAL void m0_be_log_record_iter_fini(struct m0_be_log_record_iter *iter)
{
	m0_be_fmt_log_record_header_fini(&iter->lri_header);
}

M0_INTERNAL void m0_be_log_record_iter_copy(struct m0_be_log_record_iter *dest,
					    struct m0_be_log_record_iter *src)
{
	be_log_record_header_copy(&dest->lri_header, &src->lri_header);
}

M0_INTERNAL int m0_be_log_record_initial(struct m0_be_log             *log,
					 struct m0_be_log_record_iter *curr)
{
	m0_bindex_t pos  = log->lg_header.flh_group_lsn;
	m0_bcount_t size = log->lg_header.flh_group_size;
	int         rc;

	rc = (pos == 0 && size == 0) ? -ENOENT : 0;
	rc = rc ?: be_log_record_iter_read(log, curr, pos);
	if (rc == 0 && curr->lri_header.lrh_size != size)
		rc = -EBADF;
	return rc;
}

M0_INTERNAL int m0_be_log_record_next(struct m0_be_log                   *log,
				      const struct m0_be_log_record_iter *curr,
				      struct m0_be_log_record_iter       *next)
{
	int rc = be_log_record_iter_read(log, next, curr->lri_header.lrh_pos +
						    curr->lri_header.lrh_size);
	if (rc == 0 && curr->lri_header.lrh_pos >= next->lri_header.lrh_pos)
		rc = -ENOENT;
	return rc;
}

M0_INTERNAL int m0_be_log_record_prev(struct m0_be_log                   *log,
				      const struct m0_be_log_record_iter *curr,
				      struct m0_be_log_record_iter       *prev)
{
	int rc = be_log_record_iter_read(log, prev,
					 curr->lri_header.lrh_prev_pos);
	if (rc == 0 && curr->lri_header.lrh_pos <= prev->lri_header.lrh_pos)
		rc = -ENOENT;
	return rc;
}

M0_INTERNAL bool
m0_be_log_recovery_record_available(struct m0_be_log *log)
{
	return log->lg_create_mode ? false :
	       m0_be_recovery_log_record_available(&log->lg_recovery);
}

M0_INTERNAL void
m0_be_log_recovery_record_get(struct m0_be_log             *log,
			      struct m0_be_log_record_iter *iter)
{
	m0_be_recovery_log_record_get(&log->lg_recovery, iter);
}

M0_INTERNAL m0_bindex_t
m0_be_log_recovery_discarded(struct m0_be_log *log)
{
	return m0_be_recovery_discarded(&log->lg_recovery);
}

M0_INTERNAL bool m0_be_log_contains_stob(struct m0_be_log        *log,
                                         const struct m0_stob_id *stob_id)
{
	return m0_be_log_store_contains_stob(&log->lg_store, stob_id);
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
