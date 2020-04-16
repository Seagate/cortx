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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 *                  Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 2-Jul-2013
 */

#include "be/io.h"
#include "be/log.h"
#include "lib/chan.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "stob/domain.h"
#include "stob/stob.h"
#include "ut/stob.h"
#include "ut/threads.h"
#include "ut/ut.h"

enum {
	BE_UT_LOG_SIZE            = 1024 * 1024,
	/* We try to reserve more than actual record's size */
	BE_UT_LOG_RESERVE_SIZE    = 4096 * 3,
	BE_UT_LOG_LIO_SIZE        = 1600,
	BE_UT_LOG_THREAD_NR       = 24,
	BE_UT_LOG_STOB_DOMAIN_KEY = 100,
	BE_UT_LOG_STOB_KEY        = 42,
};

const char *be_ut_log_sdom_location   = "linuxstob:./log";
const char *be_ut_log_sdom_init_cfg   = "directio=true";
const char *be_ut_log_sdom_create_cfg = "";

struct m0_stob_domain *be_ut_log_stob_domain;

static void be_ut_log_got_space_cb(struct m0_be_log *log)
{
	/* TODO make some locking mechanism, so threads can repeat reservation */
}

static void be_ut_log_cfg_set(struct m0_be_log_cfg *log_cfg,
			      struct m0_mutex      *lock)
{
	*log_cfg = (struct m0_be_log_cfg){
		.lc_store_cfg = {
			/* temporary solution BEGIN */
			.lsc_stob_domain_location   = "linuxstob:./log_store-tmp",
			.lsc_stob_domain_init_cfg   = "directio=true",
			.lsc_stob_domain_key        = 0x1000,
			.lsc_stob_domain_create_cfg = NULL,
			/* temporary solution END */
			.lsc_size            = BE_UT_LOG_SIZE,
			.lsc_stob_create_cfg = NULL,
			.lsc_rbuf_nr         = 3,
		},
		.lc_got_space_cb = &be_ut_log_got_space_cb,
		.lc_lock         = lock,
	};
	m0_stob_id_make(0, BE_UT_LOG_STOB_KEY,
	                m0_stob_domain_id_get(be_ut_log_stob_domain),
	                &log_cfg->lc_store_cfg.lsc_stob_id);
}

static void be_ut_log_init(struct m0_be_log *log, struct m0_mutex *lock)
{
	struct m0_be_log_cfg log_cfg;
	int                  rc;

	rc = m0_stob_domain_create(be_ut_log_sdom_location,
				   be_ut_log_sdom_init_cfg,
				   BE_UT_LOG_STOB_DOMAIN_KEY,
				   be_ut_log_sdom_create_cfg,
				   &be_ut_log_stob_domain);
	M0_UT_ASSERT(rc == 0);
	be_ut_log_cfg_set(&log_cfg, lock);
	rc = m0_be_log_create(log, &log_cfg);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_log_fini(struct m0_be_log *log)
{
	int rc;

	m0_be_log_destroy(log);
	rc = m0_stob_domain_destroy(be_ut_log_stob_domain);
	M0_UT_ASSERT(rc == 0);
}

static int be_ut_log_open(struct m0_be_log *log, struct m0_mutex *lock)
{
	struct m0_be_log_cfg log_cfg;

	M0_SET0(log);
	be_ut_log_cfg_set(&log_cfg, lock);
	return m0_be_log_open(log, &log_cfg);
}

struct be_ut_log_thread_ctx {
	int                     bult_index;
	int                    *bult_order;
	char                   *bult_data;
	struct m0_atomic64     *bult_atom;
	struct m0_mutex        *bult_lock;
	struct m0_be_log       *bult_log;
	struct m0_be_log_record bult_record;
	struct m0_be_op         bult_op;
	bool                    bult_discard;
	int                     bult_lio_nr;
	m0_bcount_t             bult_lio_size;
	m0_bcount_t             bult_reserve_size;
};

static void be_ut_log_multi_thread(struct be_ut_log_thread_ctx *ctx)
{
	struct m0_be_log_record *record = &ctx->bult_record;
	struct m0_be_log        *log    = ctx->bult_log;
	struct m0_mutex         *lock   = ctx->bult_lock;
	struct m0_be_op         *op     = &ctx->bult_op;
	struct m0_bufvec        *bvec;
	int                      lio_nr       = ctx->bult_lio_nr;
	m0_bcount_t              lio_size     = ctx->bult_lio_size;
	m0_bcount_t              reserve_size = ctx->bult_reserve_size;
	int64_t                  index;
	int                      rc;
	int                      i;

	m0_mutex_lock(lock);
	rc = m0_be_log_reserve(log, reserve_size);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_unlock(lock);
	for (i = 0; i < lio_nr; ++i)
		m0_be_log_record_io_size_set(record, i, lio_size);
	m0_mutex_lock(lock);
	index = m0_atomic64_add_return(ctx->bult_atom, 1) - 1;
	ctx->bult_order[index] = ctx->bult_index;
	m0_be_log_record_io_prepare(record, SIO_WRITE, reserve_size);
	m0_mutex_unlock(lock);

	for (i = 0; i < lio_nr; ++i) {
		bvec = m0_be_log_record_io_bufvec(record, i);
		M0_UT_ASSERT(bvec->ov_vec.v_nr == 1 &&
			     bvec->ov_vec.v_count[0] == lio_size);
		memcpy(bvec->ov_buf[0], ctx->bult_data, lio_size);
	}

	m0_be_log_record_io_launch(record, op);
	m0_be_op_wait(op);

	m0_mutex_lock(lock);
	if (ctx->bult_discard)
		m0_be_log_record_discard(record->lgr_log, record->lgr_size);
	else
		m0_be_log_record_skip_discard(record);
	m0_mutex_unlock(lock);
}

M0_UT_THREADS_DEFINE(be_ut_log, be_ut_log_multi_thread);

static void be_ut_log_multi_ut(int thread_nr, bool discard,
			       int lio_nr, m0_bcount_t lio_size)
{
	struct m0_be_log              log  = {};
	struct m0_mutex               lock = {};
	struct be_ut_log_thread_ctx  *ctxs;
	struct m0_be_log_record      *record;
	struct m0_be_log_record_iter *iters;
	struct m0_bufvec             *bvec;
	struct m0_atomic64            atom;
	m0_bcount_t                   reserve_size;
	int                          *order;
	int                           rc;
	int                           c;
	int                           i;
	int                           j;

	m0_mutex_init(&lock);
	be_ut_log_init(&log, &lock);

	M0_ALLOC_ARR(ctxs, thread_nr);
	M0_UT_ASSERT(ctxs != NULL);
	M0_ALLOC_ARR(order, thread_nr);
	M0_UT_ASSERT(order != NULL);
	m0_atomic64_set(&atom, 0);
	reserve_size = m0_round_up(lio_size, 1ULL << m0_be_log_bshift(&log));

	/* Preallocation */

	for (i = 0; i < thread_nr; ++i) {
		ctxs[i] = (struct be_ut_log_thread_ctx){
			.bult_index        = i,
			.bult_order        = order,
			.bult_atom         = &atom,
			.bult_log          = &log,
			.bult_lock         = &lock,
			.bult_discard      = discard,
			.bult_lio_nr       = lio_nr,
			.bult_lio_size     = lio_size,
			.bult_reserve_size = reserve_size * lio_nr,
		};
		m0_be_op_init(&ctxs[i].bult_op);
		ctxs[i].bult_data = m0_alloc(lio_size);
		M0_UT_ASSERT(ctxs[i].bult_data != NULL);
		c = i % 62;
		c = c < 10 ? c + '0' :
		    c < 36 ? c - 10 + 'a' :
			     c - 36 + 'A';
		memset(ctxs[i].bult_data, c, lio_size);

		record = &ctxs[i].bult_record;
		m0_be_log_record_init(record, &log);
		for (j = 0; j < lio_nr; ++j) {
			rc = m0_be_log_record_io_create(record, lio_size);
			M0_UT_ASSERT(rc == 0);
		}
		rc = m0_be_log_record_allocate(record);
		M0_UT_ASSERT(rc == 0);
	}

	/* Start writing */

	M0_UT_THREADS_START(be_ut_log, thread_nr, ctxs);
	M0_UT_THREADS_STOP(be_ut_log);

	/* With delayed discard all records must be discarded in order
	 * of writing to log. Therefore, discard all records here.
	 */
	m0_mutex_lock(&lock);
	for (i = 0; i < thread_nr; ++i) {
		record = &ctxs[i].bult_record;
		if (discard) {
			m0_be_log_record_discard(record->lgr_log,
						  record->lgr_size);
		} else {
			m0_be_log_record_skip_discard(record);
		}
	}
	m0_mutex_unlock(&lock);

	/* Check written records */

	M0_ALLOC_ARR(iters, thread_nr + 1);
	M0_UT_ASSERT(iters != NULL);
	M0_ALLOC_PTR(record);
	M0_UT_ASSERT(record != NULL);

	m0_be_log_record_init(record, &log);
	for (i = 0; i < lio_nr; ++i) {
		rc = m0_be_log_record_io_create(record, lio_size);
		M0_UT_ASSERT(rc == 0);
	}
	rc = m0_be_log_record_allocate(record);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < thread_nr; ++i) {
		m0_be_log_record_iter_init(&iters[i]);
		rc = i == 0 ? m0_be_log_record_initial(&log, &iters[i]) :
			    m0_be_log_record_next(&log, &iters[i-1], &iters[i]);
		M0_UT_ASSERT(rc == 0);
		m0_be_log_record_reset(record);
		m0_be_log_record_assign(record, &iters[i], false);
		m0_mutex_lock(&lock);
		m0_be_log_record_io_prepare(record, SIO_READ, 0);
		m0_mutex_unlock(&lock);

		rc = M0_BE_OP_SYNC_RET(op,
				       m0_be_log_record_io_launch(record, &op),
				       bo_sm.sm_rc);
		M0_UT_ASSERT(rc == 0);
		for (j = 0; j < lio_nr; ++j) {
			bvec = m0_be_log_record_io_bufvec(record, j);
			M0_UT_ASSERT(bvec->ov_vec.v_nr == 1 &&
				     bvec->ov_vec.v_count[0] == lio_size);
			M0_UT_ASSERT(memcmp(ctxs[order[i]].bult_data,
				     bvec->ov_buf[0],
				     lio_size) == 0);
		}
	}
	m0_be_log_record_deallocate(record);
	m0_be_log_record_fini(record);
	m0_free(record);
	/* log must contain exactly thread_nr records */
	rc = m0_be_log_record_next(&log, &iters[thread_nr-1], &iters[thread_nr]);
	M0_UT_ASSERT(rc != 0);

	/* Finalisation */

	for (i = 0; i < thread_nr; ++i) {
		m0_be_log_record_iter_fini(&iters[i]);
		m0_free(ctxs[i].bult_data);
		m0_be_op_fini(&ctxs[i].bult_op);
		m0_be_log_record_deallocate(&ctxs[i].bult_record);
		m0_be_log_record_fini(&ctxs[i].bult_record);
	}
	m0_free(iters);
	m0_free(order);
	m0_free(ctxs);

	be_ut_log_fini(&log);
	m0_mutex_fini(&lock);
}

static void be_ut_log_record_init_write_one(struct m0_be_log_record *record,
					    struct m0_be_log        *log,
					    struct m0_mutex         *lock,
					    struct m0_be_op         *op)
{
	m0_bcount_t reserve_size;
	m0_bcount_t size;
	int         rc;

	size = BE_UT_LOG_LIO_SIZE;
	reserve_size = m0_be_log_reserved_size(log, &size, 1);
	m0_be_log_record_init(record, log);
	m0_be_op_init(op);
	rc = m0_be_log_record_io_create(record, BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_allocate(record);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_lock(lock);
	rc = m0_be_log_reserve(log, reserve_size);
	M0_UT_ASSERT(rc == 0);
	m0_be_log_record_io_size_set(record, 0, BE_UT_LOG_LIO_SIZE);
	m0_be_log_record_io_prepare(record, SIO_WRITE, reserve_size);
	m0_mutex_unlock(lock);

	m0_be_log_record_io_launch(record, op);
}

static void be_ut_log_record_wait_fini_one(struct m0_be_log_record *record,
					   struct m0_mutex         *lock,
					   struct m0_be_op         *op,
					   bool                     discard)
{
	m0_be_op_wait(op);
	m0_mutex_lock(lock);
	if (discard)
		m0_be_log_record_discard(record->lgr_log, record->lgr_size);
	else
		m0_be_log_record_skip_discard(record);
	m0_mutex_unlock(lock);
	m0_be_log_record_deallocate(record);
	/* record can be non-discarded here, in this case finalisation must be
	 * protected with external lock.
	 */
	m0_mutex_lock(lock);
	m0_be_log_record_fini(record);
	m0_mutex_unlock(lock);
	m0_be_op_fini(op);
}

/* Writes a record to the log and returns position/size of the record. */
static void be_ut_log_record_write_sync(struct m0_be_log *log,
					struct m0_mutex  *lock,
					m0_bindex_t      *index,
					m0_bcount_t      *size)
{
	struct m0_be_log_record record = {};
	struct m0_be_op         op     = {};

	be_ut_log_record_init_write_one(&record, log, lock, &op);
	/* read-only access to record's fields when it's scheduled */
	*index = record.lgr_position;
	*size  = record.lgr_size;
	be_ut_log_record_wait_fini_one(&record, lock, &op, true);
}

static void be_ut_log_curr_pos_check(struct m0_be_log *log,
				     m0_bindex_t       pos)
{
	struct m0_be_log_record_iter iter = {};
	int                          rc;

	rc = m0_be_log_record_iter_init(&iter);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_initial(log, &iter);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(iter.lri_header.lrh_pos == pos);
	m0_be_log_record_iter_fini(&iter);
}

static void be_ut_log_header_repair_test(int header_nr,
					 int new_nr,
					 m0_bindex_t lsn_old,
					 m0_bcount_t size_old,
					 m0_bindex_t lsn_new,
					 m0_bcount_t size_new,
					 int valid_index)
{
	struct m0_be_fmt_log_header **hdrs;
	struct m0_be_fmt_log_header   valid = {};
	bool                          need_repair;
	int                           rc;
	int                           i;

	M0_PRE(new_nr <= header_nr);
	M0_PRE(valid_index < header_nr);

	M0_ALLOC_ARR(hdrs, header_nr);
	M0_UT_ASSERT(hdrs != NULL);
	for (i = 0; i < header_nr; ++i) {
		M0_ALLOC_PTR(hdrs[i]);
		M0_UT_ASSERT(hdrs[i] != NULL);
		rc = m0_be_fmt_log_header_init(hdrs[i], NULL);
		M0_UT_ASSERT(rc == 0);
		if (i < new_nr)
			m0_be_log_header__set(hdrs[i], 0, lsn_new, size_new);
		else
			m0_be_log_header__set(hdrs[i], 0, lsn_old, size_old);
	}
	rc = m0_be_fmt_log_header_init(&valid, NULL);
	M0_UT_ASSERT(rc == 0);

	need_repair = m0_be_log_header__repair(hdrs, header_nr, &valid);
	M0_UT_ASSERT(equi(new_nr % header_nr == 0, !need_repair));
	M0_UT_ASSERT(m0_be_log_header__is_eq(&valid, hdrs[valid_index]));

	for (i = 0; i < header_nr; ++i) {
		m0_be_fmt_log_header_fini(hdrs[i]);
		m0_free(hdrs[i]);
	}
	m0_free(hdrs);
	m0_be_fmt_log_header_fini(&valid);
}

void m0_be_ut_log_multi(void)
{
	/* Write records in parallel. Records consume size less than log
	 * capacity.
	 */
	be_ut_log_multi_ut(BE_UT_LOG_THREAD_NR, true, 2, BE_UT_LOG_LIO_SIZE);

	/* Write records in parallel, but don't discard them. */
	be_ut_log_multi_ut(BE_UT_LOG_THREAD_NR, false, 2, BE_UT_LOG_LIO_SIZE);
}

/** @see be_ut_recovery_iter_count() */
static void be_ut_log_recover_and_discard(struct m0_be_log *log,
                                          struct m0_mutex  *lock)
{
	struct m0_be_log_record_iter iter   = {};
	struct m0_be_log_record      record = {};
	int                          rc;

	m0_be_log_record_init(&record, log);
	rc = m0_be_log_record_io_create(&record, BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_allocate(&record);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_iter_init(&iter);
	M0_UT_ASSERT(rc == 0);
	while (m0_be_log_recovery_record_available(log)) {
		m0_be_log_recovery_record_get(log, &iter);
		m0_be_log_record_assign(&record, &iter, true);
		m0_mutex_lock(lock);
		m0_be_log_record_io_prepare(&record, SIO_READ, 0);
		m0_mutex_unlock(lock);
		rc = M0_BE_OP_SYNC_RET(op,
				       m0_be_log_record_io_launch(&record, &op),
				       bo_sm.sm_rc);
		M0_UT_ASSERT(rc == 0);
		m0_mutex_lock(lock);
		m0_be_log_record_discard(log, record.lgr_size);
		m0_mutex_unlock(lock);
		m0_be_log_record_reset(&record);
	}
	m0_be_log_record_iter_fini(&iter);
	m0_be_log_record_deallocate(&record);
	m0_be_log_record_fini(&record);
}

/*
 * Check m0_be_log_close() in case when at least 1 record is finalised
 * without discarding.
 *
 * XXX temporary changed due to recovery integration.
 * Please change back after log_discard become a part of log.
 */
void m0_be_ut_log_unplaced(void)
{
	struct m0_be_log        log  = {};
	struct m0_mutex         lock = {};
	struct m0_be_log_record records[4];
	struct m0_be_op         ops[4];
	m0_bindex_t             pos;
	int                     i;

	m0_mutex_init(&lock);
	be_ut_log_init(&log, &lock);

	/* 1th non-discarded */

	memset(records, 0, sizeof(records));
	memset(ops, 0, sizeof(ops));
	for (i = 0; i < 4; ++i) {
		be_ut_log_record_init_write_one(&records[i], &log,
						&lock, &ops[i]);
	}
	pos = records[3].lgr_position;
	/*
	be_ut_log_record_wait_fini_one(&records[0], &lock, &ops[0], true);
	be_ut_log_record_wait_fini_one(&records[1], &lock, &ops[1], false);
	be_ut_log_record_wait_fini_one(&records[2], &lock, &ops[2], true);
	be_ut_log_record_wait_fini_one(&records[3], &lock, &ops[3], false);
	*/
	be_ut_log_record_wait_fini_one(&records[0], &lock, &ops[0], true);
	be_ut_log_record_wait_fini_one(&records[1], &lock, &ops[1], true);
	be_ut_log_record_wait_fini_one(&records[2], &lock, &ops[2], true);
	be_ut_log_record_wait_fini_one(&records[3], &lock, &ops[3], false);

	m0_be_log_close(&log);
	be_ut_log_open(&log, &lock);
	be_ut_log_curr_pos_check(&log, pos);
	be_ut_log_recover_and_discard(&log, &lock);

	/* 0th non-discarded */

	memset(records, 0, sizeof(records));
	memset(ops, 0, sizeof(ops));
	for (i = 0; i < 3; ++i) {
		be_ut_log_record_init_write_one(&records[i], &log,
						&lock, &ops[i]);
	}
	pos = records[1].lgr_position;
	/*
	 * After log_discard is part of log it can be reverted back.
	 */
	/*
	be_ut_log_record_wait_fini_one(&records[0], &lock, &ops[0], false);
	be_ut_log_record_wait_fini_one(&records[1], &lock, &ops[1], true);
	be_ut_log_record_wait_fini_one(&records[2], &lock, &ops[2], false);
	*/
	be_ut_log_record_wait_fini_one(&records[0], &lock, &ops[0], true);
	be_ut_log_record_wait_fini_one(&records[1], &lock, &ops[1], false);
	be_ut_log_record_wait_fini_one(&records[2], &lock, &ops[2], false);

	m0_be_log_close(&log);
	be_ut_log_open(&log, &lock);
	be_ut_log_curr_pos_check(&log, pos);
	be_ut_log_recover_and_discard(&log, &lock);

	/* all 3 non-discarded */

	memset(records, 0, sizeof(records));
	memset(ops, 0, sizeof(ops));
	for (i = 0; i < 3; ++i) {
		be_ut_log_record_init_write_one(&records[i], &log,
						&lock, &ops[i]);
	}
	pos = records[0].lgr_position;
	be_ut_log_record_wait_fini_one(&records[0], &lock, &ops[0], false);
	be_ut_log_record_wait_fini_one(&records[1], &lock, &ops[1], false);
	be_ut_log_record_wait_fini_one(&records[2], &lock, &ops[2], false);

	m0_be_log_close(&log);
	be_ut_log_open(&log, &lock);
	be_ut_log_curr_pos_check(&log, pos);
	be_ut_log_recover_and_discard(&log, &lock);

	be_ut_log_fini(&log);
	m0_mutex_fini(&lock);
}

void m0_be_ut_log_header(void)
{
	struct m0_be_fmt_log_header header  = {};
	struct m0_be_log            log     = {};
	struct m0_mutex             lock    = {};
	m0_bindex_t                 index;
	m0_bcount_t                 size;
	int                         i;
	int                         rc;

	m0_mutex_init(&lock);
	be_ut_log_init(&log, &lock);
	rc = m0_be_fmt_log_header_init(&header, NULL);
	M0_UT_ASSERT(rc == 0);

	/* Check of correct work. */

	rc = m0_be_log_header_read(&log, &header);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(header.flh_group_lsn  == 0 &&
		     header.flh_group_size == 0);

	for (i = 0; i < BE_UT_LOG_THREAD_NR; ++i) {
		be_ut_log_record_write_sync(&log, &lock, &index, &size);
		if (i == 0) {
			/* log header must contain position of a valid record
			 * after first write.
			 */
			rc = m0_be_log_header_read(&log, &header);
			M0_UT_ASSERT(rc == 0);
			M0_UT_ASSERT(header.flh_group_lsn  == 0 &&
				     header.flh_group_size == size);
		}
	}

	/* log writes header during closing */
	m0_be_log_close(&log);
	rc = be_ut_log_open(&log, &lock);
	M0_UT_ASSERT(rc == 0);

	rc = m0_be_log_header_read(&log, &header);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(header.flh_group_lsn  == index &&
		     header.flh_group_size == size);

	/* m0_be_log_header__repair() check */

	be_ut_log_header_repair_test(3, 0, index + 4096, size + 4096,
				     index, size, 0);
	be_ut_log_header_repair_test(3, 3, index + 4096, size + 4096,
				     index, size, 0);
	be_ut_log_header_repair_test(3, 1, index + 4096, size + 4096,
				     index, size, 2);
	be_ut_log_header_repair_test(3, 2, index + 4096, size + 4096,
				     index, size, 0);

	m0_be_fmt_log_header_fini(&header);
	be_ut_log_fini(&log);
	m0_mutex_fini(&lock);
}

/* Check guarantees of api that ain't checked by the rest tests. */
void m0_be_ut_log_api(void)
{
	struct m0_be_log             log  = {};
	struct m0_mutex              lock = {};
	struct m0_be_log_record_iter iter = {};
	m0_bcount_t                  unit;
	int                          rc;

	m0_mutex_init(&lock);
	be_ut_log_init(&log, &lock);

	/* m0_be_log_record_initial() on empty log */

	rc = m0_be_log_record_iter_init(&iter);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_initial(&log, &iter);
	M0_UT_ASSERT(rc == -ENOENT);
	m0_be_log_record_iter_fini(&iter);

	/* m0_be_log_unreserve() check */

	unit = 1 << m0_be_log_bshift(&log);
	m0_mutex_lock(&lock);
	m0_be_log_reserve(&log, unit);
	m0_be_log_reserve(&log, unit);
	m0_be_log_unreserve(&log, 2 * unit);
	m0_be_log_reserve(&log, 2 * unit);
	m0_be_log_unreserve(&log, unit);
	m0_be_log_unreserve(&log, unit);
	m0_mutex_unlock(&lock);
	/* log checks pointers during finalisation */
	m0_be_log_close(&log);
	rc = be_ut_log_open(&log, &lock);
	M0_UT_ASSERT(rc == 0);

	/* end of tests */

	be_ut_log_fini(&log);
	m0_mutex_fini(&lock);
}

/* Simple UT shows example of log usage. */
void m0_be_ut_log_user(void)
{
	struct m0_be_log_record_iter iter   = {};
	struct m0_be_log_record      record = {};
	struct m0_be_log             log    = {};
	struct m0_mutex              lock   = {};
	struct m0_bufvec            *bvec;
	m0_bcount_t                  size[2];
	m0_bcount_t                  reserved_size;
	int                          rc;

	m0_mutex_init(&lock);
	be_ut_log_init(&log, &lock);

	/* Check that actual record's size is less than we will reserve */
	size[0] = size[1] = BE_UT_LOG_LIO_SIZE;
	reserved_size = m0_be_log_reserved_size(&log, size, 2);
	M0_UT_ASSERT(reserved_size < BE_UT_LOG_RESERVE_SIZE);

	/* Write */

	m0_be_log_record_init(&record, &log);
	rc = m0_be_log_record_io_create(&record, BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_io_create(&record, BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_allocate(&record);
	M0_UT_ASSERT(rc == 0);

	m0_mutex_lock(&lock);
	rc = m0_be_log_reserve(&log, BE_UT_LOG_RESERVE_SIZE);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_unlock(&lock);

	m0_be_log_record_io_size_set(&record, 0, BE_UT_LOG_LIO_SIZE);
	m0_be_log_record_io_size_set(&record, 1, BE_UT_LOG_LIO_SIZE);
	m0_mutex_lock(&lock);
	m0_be_log_record_io_prepare(&record, SIO_WRITE, BE_UT_LOG_RESERVE_SIZE);
	m0_mutex_unlock(&lock);

	/* fill bufvec */
	bvec = m0_be_log_record_io_bufvec(&record, 0);
	M0_UT_ASSERT(bvec->ov_vec.v_nr == 1 &&
		     bvec->ov_vec.v_count[0] == BE_UT_LOG_LIO_SIZE);
	strncpy(bvec->ov_buf[0], "lio1", BE_UT_LOG_LIO_SIZE);
	bvec = m0_be_log_record_io_bufvec(&record, 1);
	M0_UT_ASSERT(bvec->ov_vec.v_nr == 1 &&
		     bvec->ov_vec.v_count[0] == BE_UT_LOG_LIO_SIZE);
	strncpy(bvec->ov_buf[0], "lio2", BE_UT_LOG_LIO_SIZE);

	rc = M0_BE_OP_SYNC_RET(op,
			       m0_be_log_record_io_launch(&record, &op),
			       bo_sm.sm_rc);
	M0_UT_ASSERT(rc == 0);

	m0_mutex_lock(&lock);
	m0_be_log_record_discard(&log, record.lgr_size);
	m0_mutex_unlock(&lock);
	m0_be_log_record_deallocate(&record);
	m0_be_log_record_fini(&record);

	/* Read */

	record = (struct m0_be_log_record){};
	m0_be_log_record_init(&record, &log);
	rc = m0_be_log_record_io_create(&record, BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_io_create(&record, BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_allocate(&record);
	M0_UT_ASSERT(rc == 0);

	rc = m0_be_log_record_iter_init(&iter);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_initial(&log, &iter);
	M0_UT_ASSERT(rc == 0);
	m0_be_log_record_assign(&record, &iter, false);

	m0_mutex_lock(&lock);
	m0_be_log_record_io_prepare(&record, SIO_READ, 0);
	m0_mutex_unlock(&lock);

	rc = M0_BE_OP_SYNC_RET(op,
			       m0_be_log_record_io_launch(&record, &op),
			       bo_sm.sm_rc);
	M0_UT_ASSERT(rc == 0);

	bvec = m0_be_log_record_io_bufvec(&record, 0);
	M0_UT_ASSERT(bvec->ov_vec.v_nr == 1 &&
		     bvec->ov_vec.v_count[0] == BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(memcmp("lio1", bvec->ov_buf[0], strlen("lio1")) == 0);
	bvec = m0_be_log_record_io_bufvec(&record, 1);
	M0_UT_ASSERT(bvec->ov_vec.v_nr == 1 &&
		     bvec->ov_vec.v_count[0] == BE_UT_LOG_LIO_SIZE);
	M0_UT_ASSERT(memcmp("lio2", bvec->ov_buf[0], strlen("lio2")) == 0);

	m0_be_log_record_iter_fini(&iter);
	m0_be_log_record_deallocate(&record);
	m0_be_log_record_fini(&record);

	be_ut_log_fini(&log);
	m0_mutex_fini(&lock);
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
