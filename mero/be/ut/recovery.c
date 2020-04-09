/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 27-Mar-2015
 */

#include "be/io.h"
#include "be/log.h"
#include "be/recovery.h"
#include "lib/memory.h"
#include "stob/domain.h"
#include "stob/stob.h"
#include "ut/stob.h"
#include "ut/ut.h"

enum {
	BE_UT_RECOVERY_LOG_SIZE            = 1024 * 1024,
	BE_UT_RECOVERY_LOG_RESERVE_SIZE    = 4096,
	BE_UT_RECOVERY_LOG_LIO_SIZE        = 1500,
	BE_UT_RECOVERY_LOG_STOB_DOMAIN_KEY = 100,
	BE_UT_RECOVERY_LOG_STOB_KEY        = 42,
	BE_UT_RECOVERY_LOG_RBUF_NR         = 8,
};

const char *be_ut_recovery_log_sdom_location   = "linuxstob:./log";
const char *be_ut_recovery_log_sdom_init_cfg   = "directio=true";
const char *be_ut_recovery_log_sdom_create_cfg = "";

struct be_ut_recovery_ctx {
	struct m0_be_log         burc_log;
	struct m0_mutex          burc_lock;
	struct m0_stob_domain   *burc_sdom;
	struct m0_be_log_record *burc_records;
};

static void be_ut_log_got_space_cb(struct m0_be_log *log)
{
}

static void be_ut_recovery_log_cfg_set(struct m0_be_log_cfg  *log_cfg,
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
			.lsc_size            = BE_UT_RECOVERY_LOG_SIZE,
			.lsc_stob_create_cfg = NULL,
			.lsc_rbuf_nr         = BE_UT_RECOVERY_LOG_RBUF_NR,
		},
		.lc_got_space_cb = &be_ut_log_got_space_cb,
		.lc_lock         = lock,
	};
	m0_stob_id_make(0, BE_UT_RECOVERY_LOG_STOB_KEY,
	                m0_stob_domain_id_get(sdom),
	                &log_cfg->lc_store_cfg.lsc_stob_id);
}

static void be_ut_recovery_log_init(struct be_ut_recovery_ctx *ctx)
{
	struct m0_be_log_cfg log_cfg;
	int                  rc;

	m0_mutex_init(&ctx->burc_lock);
	rc = m0_stob_domain_create(be_ut_recovery_log_sdom_location,
				   be_ut_recovery_log_sdom_init_cfg,
				   BE_UT_RECOVERY_LOG_STOB_DOMAIN_KEY,
				   be_ut_recovery_log_sdom_create_cfg,
				   &ctx->burc_sdom);
	M0_UT_ASSERT(rc == 0);
	be_ut_recovery_log_cfg_set(&log_cfg, ctx->burc_sdom, &ctx->burc_lock);
	rc = m0_be_log_create(&ctx->burc_log, &log_cfg);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_recovery_log_fini(struct be_ut_recovery_ctx *ctx)
{
	int rc;

	m0_be_log_destroy(&ctx->burc_log);
	rc = m0_stob_domain_destroy(ctx->burc_sdom);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_fini(&ctx->burc_lock);
}

static void be_ut_recovery_log_open(struct be_ut_recovery_ctx *ctx)
{
	struct m0_be_log_cfg log_cfg;
	int                  rc;

	be_ut_recovery_log_cfg_set(&log_cfg, ctx->burc_sdom, &ctx->burc_lock);
	rc = m0_be_log_open(&ctx->burc_log, &log_cfg);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_recovery_log_close(struct be_ut_recovery_ctx *ctx)
{
	m0_be_log_close(&ctx->burc_log);
}

static void be_ut_recovery_log_reopen(struct be_ut_recovery_ctx *ctx)
{
	be_ut_recovery_log_close(ctx);
	M0_SET0(&ctx->burc_log);
	be_ut_recovery_log_open(ctx);
}

static void be_ut_recovery_log_record_init_one(struct m0_be_log_record *record,
					       struct m0_be_log        *log)
{
	int rc;

	m0_be_log_record_init(record, log);
	rc = m0_be_log_record_io_create(record, BE_UT_RECOVERY_LOG_LIO_SIZE);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_log_record_allocate(record);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_recovery_log_record_fini_one(struct m0_be_log_record *record)
{
	m0_be_log_record_deallocate(record);
	m0_be_log_record_fini(record);
}

static void be_ut_recovery_log_fill(struct be_ut_recovery_ctx *ctx,
				    int                        record_nr,
				    int                        discard_nr,
				    bool                       last_incomplete)
{
	struct m0_be_log        *log  = &ctx->burc_log;
	struct m0_mutex         *lock = &ctx->burc_lock;
	struct m0_be_log_record *record;
	int                      i;
	int                      rc;

	M0_ALLOC_ARR(ctx->burc_records, record_nr);
	M0_UT_ASSERT(ctx->burc_records != NULL);

	for (i = 0; i < record_nr; ++i) {
		record = &ctx->burc_records[i];
		be_ut_recovery_log_record_init_one(record, log);
		m0_mutex_lock(lock);
		rc = m0_be_log_reserve(log, BE_UT_RECOVERY_LOG_RESERVE_SIZE);
		M0_UT_ASSERT(rc == 0);
		m0_be_log_record_io_size_set(record, 0,
					     BE_UT_RECOVERY_LOG_LIO_SIZE);
		m0_be_log_record_io_prepare(record, SIO_WRITE,
					    BE_UT_RECOVERY_LOG_RESERVE_SIZE);
		m0_mutex_unlock(lock);
		rc = M0_BE_OP_SYNC_RET(op,
				       m0_be_log_record_io_launch(record, &op),
				       bo_sm.sm_rc);
		M0_UT_ASSERT(rc == 0);

		m0_mutex_lock(lock);
		if (i < discard_nr) {
			m0_be_log_record_discard(record->lgr_log,
						  record->lgr_size);
		} else {
			m0_be_log_record_skip_discard(record);
		}
		m0_mutex_unlock(lock);
	}

	for (i = 0; i < record_nr; ++i) {
		record = &ctx->burc_records[i];
		be_ut_recovery_log_record_fini_one(record);
	}
	m0_free(ctx->burc_records);
}

static int be_ut_recovery_iter_count(struct be_ut_recovery_ctx *ctx)
{
	struct m0_be_log             *log    = &ctx->burc_log;
	struct m0_mutex              *lock   = &ctx->burc_lock;
	struct m0_be_log_record       record = {};
	struct m0_be_log_record_iter  iter   = {};
	int                           count  = 0;
	int                           rc;

	be_ut_recovery_log_record_init_one(&record, log);
	rc = m0_be_log_record_iter_init(&iter);
	M0_UT_ASSERT(rc == 0);
	while (m0_be_log_recovery_record_available(log)) {
		m0_be_log_recovery_record_get(log, &iter);
		++count;

		/* discard this record to move log's pointers */
		m0_be_log_record_assign(&record, &iter, true);
		m0_mutex_lock(lock);
		m0_be_log_record_io_prepare(&record, SIO_READ, 0);
		m0_mutex_unlock(lock);
		rc = M0_BE_OP_SYNC_RET(op,
				       m0_be_log_record_io_launch(&record, &op),
				       bo_sm.sm_rc);
		M0_UT_ASSERT(rc == 0);
		m0_mutex_lock(lock);
		m0_be_log_record_discard(record.lgr_log, record.lgr_size);
		m0_mutex_unlock(lock);
		m0_be_log_record_reset(&record);
	}
	m0_be_log_record_iter_fini(&iter);
	be_ut_recovery_log_record_fini_one(&record);

	return count;
}

void m0_be_ut_recovery(void)
{
	struct be_ut_recovery_ctx ctx = {};
	int                       count;
	int                       nr;

	be_ut_recovery_log_init(&ctx);

	/* empty log */
	count = be_ut_recovery_iter_count(&ctx);
	M0_UT_ASSERT(count == 0);

	be_ut_recovery_log_fill(&ctx, 10, 10, false);
	be_ut_recovery_log_reopen(&ctx);
	count = be_ut_recovery_iter_count(&ctx);
	M0_UT_ASSERT(count == 0);

	be_ut_recovery_log_fill(&ctx, 10, 5, false);
	be_ut_recovery_log_reopen(&ctx);
	count = be_ut_recovery_iter_count(&ctx);
	M0_UT_ASSERT(count == 5);

	nr = BE_UT_RECOVERY_LOG_SIZE / BE_UT_RECOVERY_LOG_RESERVE_SIZE * 2;
	be_ut_recovery_log_fill(&ctx, nr, nr - 5, false);
	be_ut_recovery_log_reopen(&ctx);
	count = be_ut_recovery_iter_count(&ctx);
	M0_UT_ASSERT(count == 5);

	be_ut_recovery_log_reopen(&ctx);
	count = be_ut_recovery_iter_count(&ctx);
	M0_UT_ASSERT(count == 0);

	be_ut_recovery_log_fini(&ctx);
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
