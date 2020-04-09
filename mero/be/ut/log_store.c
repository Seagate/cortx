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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "be/log_store.h"

#include "be/op.h"              /* M0_BE_OP_SYNC */
#include "be/log_sched.h"       /* m0_be_log_io */

#include "lib/misc.h"           /* M0_SET0 */
#include "lib/errno.h"          /* EINVAL */
#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/string.h"         /* m0_strdup */
#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "ut/stob.h"            /* m0_ut_stob_linux_get */
#include "be/op.h"              /* M0_BE_OP_SYNC */

#include "stob/domain.h"        /* m0_stob_domain_create */
#include "stob/stob.h"          /* m0_stob_id_make */

#if 0
/* DEAD CODE BEGIN */
struct m0_be_log_store_io {
	struct m0_be_log_store *lsi_ls;
	struct m0_be_io        *lsi_io;
	struct m0_be_io        *lsi_io_cblock;
	m0_bindex_t             lsi_pos;
	m0_bindex_t             lsi_end;
};
/* DEAD CODE END */

enum {
	BE_UT_LOG_STOR_SIZE  = 0x100,
	BE_UT_LOG_STOR_STEP  = 0xF,
	BE_UT_LOG_STOR_ITER  = 0x200,
	BE_UT_LOG_STOR_CR_NR = 0x7,
};

static char        be_ut_log_store_pre[BE_UT_LOG_STOR_SIZE];
static char        be_ut_log_store_post[BE_UT_LOG_STOR_SIZE];
static uint64_t    be_ut_log_store_seed;
static m0_bindex_t be_ut_log_store_pos;

/* this random may have non-uniform distribution */
static int be_ut_log_store_rand(int mod)
{
	return m0_rnd64(&be_ut_log_store_seed) % mod;
}

static void
be_ut_log_store_io_read(struct m0_be_log_store *ls, char *buf, m0_bcount_t size)
{
	struct m0_be_io bio = {};
	int             rc;

	rc = m0_be_io_init(&bio);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_io_allocate(&bio, &M0_BE_IO_CREDIT(1, size, 1));
	M0_UT_ASSERT(rc == 0);
	m0_be_io_add(&bio, ls->ls_stob, buf, 0, size);
	m0_be_io_configure(&bio, SIO_READ);

	rc = M0_BE_OP_SYNC(op, m0_be_io_launch(&bio, &op));
	M0_UT_ASSERT(rc == 0);

	m0_be_io_deallocate(&bio);
	m0_be_io_fini(&bio);
}

static void
be_ut_log_store_rand_cr(struct m0_be_io_credit *cr, m0_bcount_t size)
{
	int         buf[BE_UT_LOG_STOR_CR_NR];
	m0_bcount_t i;

	M0_SET0(cr);
	M0_SET_ARR0(buf);
	for (i = 0; i < size; ++i)
		++buf[be_ut_log_store_rand(ARRAY_SIZE(buf))];
	for (i = 0; i < ARRAY_SIZE(buf); ++i) {
		if (buf[i] != 0)
			m0_be_io_credit_add(cr, &M0_BE_IO_CREDIT(1, buf[i], 0));
	}
	/* wrap credit */
	m0_be_io_credit_add(cr, &M0_BE_IO_CREDIT(1, 0, 0));
}

static void be_ut_log_store_io_write_sync(struct m0_be_io *bio)
{
	int rc;

	m0_be_io_configure(bio, SIO_WRITE);
	rc = M0_BE_OP_SYNC(op, m0_be_io_launch(bio, &op));
	M0_UT_ASSERT(rc == 0);
}

static void
be_ut_log_store_io_check(struct m0_be_log_store *ls, m0_bcount_t size)
{
	struct m0_be_log_store_io lsi;
	struct m0_be_io_credit    io_cr_log;
	struct m0_be_io_credit    io_cr_log_cblock;
	struct m0_be_io           io_log = {};
	struct m0_be_io           io_log_cblock = {};
	m0_bcount_t               cblock_size;
	m0_bcount_t               data_size;
	int                       cmp;
	int                       rc;
	int                       i;
	char                      rbuf[BE_UT_LOG_STOR_SIZE];

	M0_PRE(size <= ARRAY_SIZE(rbuf));

	for (i = 0; i < size; ++i)
		rbuf[i] = be_ut_log_store_rand(0x100);

	cblock_size = be_ut_log_store_rand(size - 1) + 1;
	data_size   = size - cblock_size;
	M0_ASSERT(cblock_size > 0);
	M0_ASSERT(data_size > 0);

	be_ut_log_store_rand_cr(&io_cr_log, data_size);
	m0_be_log_store_cblock_io_credit(&io_cr_log_cblock, cblock_size);

	rc = m0_be_io_init(&io_log);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_io_allocate(&io_log, &io_cr_log);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_io_init(&io_log_cblock);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_io_allocate(&io_log_cblock, &io_cr_log_cblock);
	M0_UT_ASSERT(rc == 0);

	m0_be_log_store_io_init(&lsi, ls, &io_log, &io_log_cblock, size);

	/* save */
	be_ut_log_store_io_read(ls, be_ut_log_store_pre,
			       ARRAY_SIZE(be_ut_log_store_pre));
	/* log storage io */
	m0_be_log_store_io_add(&lsi, rbuf, data_size);
	m0_be_log_store_io_add_cblock(&lsi, &rbuf[data_size], cblock_size);
	m0_be_log_store_io_sort(&lsi);
	m0_be_log_store_io_fini(&lsi);
	be_ut_log_store_io_write_sync(&io_log);
	be_ut_log_store_io_write_sync(&io_log_cblock);
	/* do operation in saved memory representation of the log storage */
	for (i = 0; i < size; ++i) {
		be_ut_log_store_pre[(be_ut_log_store_pos + i) %
				   BE_UT_LOG_STOR_SIZE] = rbuf[i];
	}
	be_ut_log_store_pos += size;
	/* check if it was done in log */
	be_ut_log_store_io_read(ls, be_ut_log_store_post,
			       ARRAY_SIZE(be_ut_log_store_post));
	cmp = memcmp(be_ut_log_store_pre, be_ut_log_store_post, size);
	M0_UT_ASSERT(cmp == 0);

	m0_be_io_deallocate(&io_log_cblock);
	m0_be_io_fini(&io_log_cblock);
	m0_be_io_deallocate(&io_log);
	m0_be_io_fini(&io_log);
}

static void be_ut_log_store_io_check_nop(struct m0_be_log_store *ls,
					m0_bcount_t size)
{
	struct m0_be_log_store_io lsi = {};
	struct m0_be_io           io_log = {};
	struct m0_be_io           io_log_cblock = {};
	int                       rc;

	rc = m0_be_io_init(&io_log);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_io_allocate(&io_log, &M0_BE_IO_CREDIT(1, 1, 1));
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_io_init(&io_log_cblock);
	M0_UT_ASSERT(rc == 0);
	rc = m0_be_io_allocate(&io_log_cblock, &M0_BE_IO_CREDIT(1, 1, 1));
	M0_UT_ASSERT(rc == 0);

	m0_be_log_store_io_init(&lsi, ls, &io_log, &io_log_cblock, size);
	m0_be_log_store_io_fini(&lsi);

	m0_be_io_deallocate(&io_log_cblock);
	m0_be_io_fini(&io_log_cblock);
	m0_be_io_deallocate(&io_log);
	m0_be_io_fini(&io_log);
}

static void be_ut_log_store(bool fake_io)
{
	struct m0_be_log_store  ls;
	struct m0_stob         *stob;
	const m0_bcount_t       log_size = BE_UT_LOG_STOR_SIZE;
	m0_bcount_t             used;
	m0_bcount_t             step;
	int                     rc;
	int                     i;

	stob = m0_ut_stob_linux_get();
	M0_SET0(&ls);
	m0_be_log_store_init(&ls, stob);

	rc = m0_be_log_store_create(&ls, log_size);
	M0_UT_ASSERT(rc == 0);

	used = 0;
	be_ut_log_store_seed = 0;
	be_ut_log_store_pos = 0;
	for (step = 2; step <= BE_UT_LOG_STOR_STEP; ++step) {
		for (i = 0; i < BE_UT_LOG_STOR_ITER; ++i) {
			M0_UT_ASSERT(0 <= used && used <= BE_UT_LOG_STOR_SIZE);
			if (used + step <= log_size) {
				rc = m0_be_log_store_reserve(&ls, step);
				M0_UT_ASSERT(rc == 0);
				used += step;
				if (fake_io) {
					be_ut_log_store_io_check_nop(&ls, step);
				} else {
					be_ut_log_store_io_check(&ls, step);
				}
			} else {
				rc = m0_be_log_store_reserve(&ls, step);
				M0_UT_ASSERT(rc != 0);
				m0_be_log_store_discard(&ls, step);
				used -= step;
			}
		}
	}
	m0_be_log_store_discard(&ls, used);

	m0_be_log_store_destroy(&ls);
	m0_be_log_store_fini(&ls);
	m0_ut_stob_put(stob, true);
}
#endif

enum {
	BE_UT_LOG_STORE_STOB_DOMAIN_KEY = 0x100,
	BE_UT_LOG_STORE_STOB_KEY_BEGIN  = 0x42,
	BE_UT_LOG_STORE_SIZE            = 0x40000,
	BE_UT_LOG_STORE_NR              = 0x10,
	BE_UT_LOG_STORE_RBUF_NR         = 0x8,
	BE_UT_LOG_STORE_RBUF_SIZE       = 0x456,
};

#define BE_UT_LOG_STORE_SDOM_INIT_CFG "directio=true"

static const char *be_ut_log_store_sdom_location   = "linuxstob:./log_store";
static const char *be_ut_log_store_sdom_init_cfg   =
						BE_UT_LOG_STORE_SDOM_INIT_CFG;
static const char *be_ut_log_store_sdom_create_cfg = "";

static struct m0_be_log_store_cfg be_ut_log_store_cfg = {
	/* temporary solution BEGIN */
	.lsc_stob_domain_location   = "linuxstob:./log_store-tmp",
	.lsc_stob_domain_init_cfg   = BE_UT_LOG_STORE_SDOM_INIT_CFG,
	.lsc_stob_domain_key        = 0x1000,
	.lsc_stob_domain_create_cfg = NULL,
	/* temporary solution END */
	.lsc_size            = BE_UT_LOG_STORE_SIZE,
	.lsc_stob_create_cfg = NULL,
	.lsc_rbuf_nr         = BE_UT_LOG_STORE_RBUF_NR,
	.lsc_rbuf_size       = BE_UT_LOG_STORE_RBUF_SIZE,
};
#undef BE_UT_LOG_STORE_SDOM_INIT_CFG

static void be_ut_log_store_stob_domain_init(struct m0_stob_domain **sdom)
{
	int rc;

	rc = m0_stob_domain_destroy_location(be_ut_log_store_sdom_location);
	M0_UT_ASSERT(rc == 0);
	rc = m0_stob_domain_create(be_ut_log_store_sdom_location,
				   be_ut_log_store_sdom_init_cfg,
				   BE_UT_LOG_STORE_STOB_DOMAIN_KEY,
				   be_ut_log_store_sdom_create_cfg,
				   sdom);
	M0_UT_ASSERT(rc == 0);
}

static void be_ut_log_store_stob_domain_fini(struct m0_stob_domain *sdom)
{
	int rc;

	rc = m0_stob_domain_destroy(sdom);
	M0_UT_ASSERT(rc == 0);
}

void m0_be_ut_log_store_create_simple(void)
{
	/**
	 * @todo use ls_cfg_create and ls_cfg_open to test that
	 * create parameters aren't used in open()
	 */
	struct m0_be_log_store_cfg ls_cfg = be_ut_log_store_cfg;
	struct m0_be_log_store     ls;
	struct m0_stob_domain     *sdom;
	int                        rc;

	be_ut_log_store_stob_domain_init(&sdom);

	m0_stob_id_make(0, BE_UT_LOG_STORE_STOB_KEY_BEGIN,
	                m0_stob_domain_id_get(sdom), &ls_cfg.lsc_stob_id);

	M0_SET0(&ls);
	M0_LOG(M0_DEBUG, "open() should fail for non-existent log_store");
	rc = m0_be_log_store_open(&ls, &ls_cfg);
	M0_UT_ASSERT(rc != 0);

	M0_SET0(&ls);
	M0_LOG(M0_DEBUG, "create() should succeed for non-existent log_store");
	rc = m0_be_log_store_create(&ls, &ls_cfg);
	M0_UT_ASSERT(rc == 0);
	M0_LOG(M0_DEBUG, "destroy() after successful create()");
	m0_be_log_store_destroy(&ls);

	M0_SET0(&ls);
	M0_LOG(M0_DEBUG, "create() + close()");
	rc = m0_be_log_store_create(&ls, &ls_cfg);
	M0_UT_ASSERT(rc == 0);
	m0_be_log_store_close(&ls);

#if 0
	M0_SET0(&ls);
	M0_LOG(M0_DEBUG, "create() should fail after successful create()");
	rc = m0_be_log_store_create(&ls, &ls_cfg);
	M0_UT_ASSERT(rc != 0);
#endif

	M0_SET0(&ls);
	M0_LOG(M0_DEBUG, "open() + close()");
	rc = m0_be_log_store_open(&ls, &ls_cfg);
	M0_UT_ASSERT(rc == 0);
	m0_be_log_store_close(&ls);

	M0_SET0(&ls);
	M0_LOG(M0_DEBUG, "open() + destroy()");
	rc = m0_be_log_store_open(&ls, &ls_cfg);
	M0_UT_ASSERT(rc == 0);
	m0_be_log_store_destroy(&ls);

	M0_SET0(&ls);
	M0_LOG(M0_DEBUG, "open() should fail after destroy()");
	rc = m0_be_log_store_open(&ls, &ls_cfg);
	M0_UT_ASSERT(rc != 0);

	be_ut_log_store_stob_domain_fini(sdom);
}

void m0_be_ut_log_store_create_random(void)
{
	struct m0_be_log_store_cfg ls_cfg[BE_UT_LOG_STORE_NR];
	struct m0_be_log_store     ls[BE_UT_LOG_STORE_NR]     = {};
	struct m0_stob_domain     *sdom;
	char                      *location;
	int                        rc;
	int                        i;

	be_ut_log_store_stob_domain_init(&sdom);

	for (i = 0; i < ARRAY_SIZE(ls_cfg); ++i) {
		ls_cfg[i] = be_ut_log_store_cfg;
		m0_stob_id_make(0, BE_UT_LOG_STORE_STOB_KEY_BEGIN + i,
		                m0_stob_domain_id_get(sdom),
		                &ls_cfg[i].lsc_stob_id);
		/* temporary solution BEGIN */
		location = m0_strdup(ls_cfg[i].lsc_stob_domain_location);
		M0_ASSERT(i <= 'Z' - 'A');
		location[strlen(location) - 1] = 'A' + i; /* XXX dirty hack */
		ls_cfg[i].lsc_stob_domain_location = location;
		ls_cfg[i].lsc_stob_domain_key += i;
		/* temporary solution END */
	}

	for (i = 0; i < ARRAY_SIZE(ls); ++i) {
		rc = m0_be_log_store_create(&ls[i], &ls_cfg[i]);
		M0_UT_ASSERT(rc == 0);
	}
	for (i = 0; i < ARRAY_SIZE(ls); ++i)
		m0_be_log_store_destroy(&ls[i]);

	/* temporary solution BEGIN */
	for (i = 0; i < ARRAY_SIZE(ls_cfg); ++i)
		m0_free(ls_cfg[i].lsc_stob_domain_location);
	/* temporary solution END */

	be_ut_log_store_stob_domain_fini(sdom);
}

static void
be_ut_log_store_test(void (*func)(struct m0_be_log_store *ls,
				  bool                    first_run))
{
	struct m0_be_log_store_cfg ls_cfg = be_ut_log_store_cfg;
	struct m0_be_log_store     ls     = {};
	struct m0_stob_domain     *sdom;
	int                        rc;
	int                        i;

	be_ut_log_store_stob_domain_init(&sdom);

	m0_stob_id_make(0, BE_UT_LOG_STORE_STOB_KEY_BEGIN,
	                m0_stob_domain_id_get(sdom), &ls_cfg.lsc_stob_id);

	rc = m0_be_log_store_create(&ls, &ls_cfg);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < 2; ++i) {
		func(&ls, i == 0);
		m0_be_log_store_close(&ls);
		m0_be_log_store_open(&ls, &ls_cfg);
	}

	m0_be_log_store_destroy(&ls);

	be_ut_log_store_stob_domain_fini(sdom);
}

enum {
	BE_UT_LOG_STORE_IO_WINDOW_STEP    = 0x1,
	BE_UT_LOG_STORE_IO_WINDOW_STEP_NR = 0x100000,
};

/* Check I/O windows ordering. */
static void be_ut_log_store_io_window(struct m0_be_log_store *ls,
				      bool                    first_run)
{
	m0_bindex_t offset  = 0;
	m0_bcount_t length;
	m0_bindex_t offset1 = 0;
	m0_bcount_t length1 = 0;
	int         rc;
	int         i;

	for (i = 0; i < BE_UT_LOG_STORE_IO_WINDOW_STEP_NR; ++i) {
		rc = m0_be_log_store_io_window(ls, offset, &length);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(length != 0);
		M0_UT_ASSERT(length1 + offset1 <= length + offset);
		offset1 = offset;
		length1 = length;

		offset += BE_UT_LOG_STORE_IO_WINDOW_STEP;
	}
}

void m0_be_ut_log_store_io_window(void)
{
	be_ut_log_store_test(&be_ut_log_store_io_window);
}

enum {
	BE_UT_LOG_STORE_IO_DISCARD_STEP    = 0x11,
	BE_UT_LOG_STORE_IO_DISCARD_STEP_NR = 0x10000,
};

static void be_ut_log_store_io_discard(struct m0_be_log_store *ls,
				       bool                    first_run)
{
	m0_bindex_t offset = 0;
	m0_bcount_t length;
	int         rc;
	int         i;

	for (i = 0; i < BE_UT_LOG_STORE_IO_DISCARD_STEP_NR; ++i) {
		rc = m0_be_log_store_io_window(ls, offset, &length);
		M0_UT_ASSERT(rc == 0);
		M0_BE_OP_SYNC(op, m0_be_log_store_io_discard(ls, offset, &op));
		rc = m0_be_log_store_io_window(ls, offset, &length);
		M0_UT_ASSERT(M0_IN(rc, (0, -EINVAL)));
		M0_UT_ASSERT(ergo(rc == 0, length != 0));

		offset += BE_UT_LOG_STORE_IO_DISCARD_STEP;
	}
}

void m0_be_ut_log_store_io_discard(void)
{
	be_ut_log_store_test(&be_ut_log_store_io_discard);
}

enum {
	BE_UT_LOG_STORE_IO_TRANSLATE_STEP    = 0x1,
	BE_UT_LOG_STORE_IO_TRANSLATE_STEP_NR = 0x400,
	BE_UT_LOG_STORE_IO_NR                = 0x10,
};

static void be_ut_log_store_length_generate(m0_bcount_t *io_length,
					    unsigned     nr,
					    uint64_t    *seed)
{
	m0_bcount_t delta;
	unsigned    i;
	unsigned    index;

	for (i = 1; i < nr; ++i) {
		/* find a number to split */
		do {
			index = m0_rnd64(seed) % i;
		} while (io_length[index] < 2);
		/* split it into two non-zero numbers */
		delta = m0_rnd64(seed) % (io_length[index] - 1) + 1;
		io_length[index] -= delta;
		io_length[i]      = delta;
		M0_UT_ASSERT(io_length[index] > 0);
		M0_UT_ASSERT(io_length[i]     > 0);
	}
}

static void be_ut_log_store_io_intersect_check(struct m0_be_log_io **lio,
					       unsigned              nr)
{
	struct m0_be_io *bio1;
	struct m0_be_io *bio2;
	bool             intersect;
	int              i;
	int              j;

	for (i = 0; i < nr; ++i)
		for (j = i + 1; j < nr; ++j) {
			bio1 = m0_be_log_io_be_io(lio[i]);
			bio2 = m0_be_log_io_be_io(lio[j]);
			intersect = m0_be_io_intersect(bio1, bio2);
			M0_UT_ASSERT(!intersect);
		}
}

static void be_ut_log_store_io_translate(struct m0_be_log_store *ls,
					 bool                    first_run)
{
	struct m0_be_io_credit   iocred = {};
	struct m0_be_log_io     *lio;
	struct m0_be_log_io    **lio_arr;
	struct m0_be_io         *bio;
	unsigned char           *field;
	m0_bcount_t              io_length[BE_UT_LOG_STORE_IO_NR];
	m0_bindex_t              io_offset[BE_UT_LOG_STORE_IO_NR];
	m0_bindex_t              offset = 0;
	m0_bcount_t              length;
	m0_bcount_t              length1;
	m0_bcount_t              field_length;
	m0_bcount_t              size;
	uint32_t                 bshift = m0_be_log_store_bshift(ls);
	uint64_t                 alignment = 1ULL << bshift;
	uint64_t                 seed = 42;
	int                      rc;
	int                      i;
	int                      j;

	M0_ALLOC_ARR(lio, BE_UT_LOG_STORE_IO_NR);
	M0_UT_ASSERT(lio != NULL);
	M0_ALLOC_ARR(lio_arr, BE_UT_LOG_STORE_IO_NR);
	M0_UT_ASSERT(lio_arr != NULL);
	for (i = 0; i < BE_UT_LOG_STORE_IO_NR; ++i)
		lio_arr[i] = &lio[i];
	rc = m0_be_log_store_io_window(ls, 0, &field_length);
	M0_UT_ASSERT(rc == 0);
	field = m0_alloc_aligned(field_length, bshift);
	for (i = 0; i < field_length; ++i)
		field[i] = m0_rnd64(&seed) & 0xFF;
	m0_be_log_store_io_credit(ls, &iocred);
	m0_be_io_credit_add(&iocred, &M0_BE_IO_CREDIT(1, field_length, 0));
	for (i = 0; i < BE_UT_LOG_STORE_IO_NR; ++i) {
		rc = m0_be_log_io_init(&lio[i]);
		M0_UT_ASSERT(rc == 0);
		rc = m0_be_log_io_allocate(&lio[i], &iocred, bshift);
		M0_UT_ASSERT(rc == 0);
	}
	for (i = 0; i < BE_UT_LOG_STORE_IO_TRANSLATE_STEP_NR; ++i) {
		rc = m0_be_log_store_io_window(ls, offset, &length);
		M0_UT_ASSERT(rc == 0);
		length = m0_round_down(length, alignment);
		M0_UT_ASSERT(length > 0);
		/*
		 * Temporary solution. If log store can expand then field_length
		 * should be increased here (field allocated size should be
		 * increased too).
		 */
		M0_UT_ASSERT(length <= field_length);

		io_length[0] = length / alignment;
		be_ut_log_store_length_generate(&io_length[0],
						ARRAY_SIZE(io_length), &seed);
		for (j = 0; j < ARRAY_SIZE(io_length); ++j)
			io_length[j] *= alignment;
		length1 = m0_reduce(k, ARRAY_SIZE(io_length),
				    0, + io_length[k]);
		M0_UT_ASSERT(length == length1);

		io_offset[0] = 0;
		for (j = 1; j < ARRAY_SIZE(io_length); ++j)
			io_offset[j] = io_offset[j - 1] + io_length[j - 1];

		for (j = 0; j < ARRAY_SIZE(io_length); ++j) {
			m0_be_log_io_reset(&lio[j]);
			bio = m0_be_log_io_be_io(&lio[j]);
			m0_be_io_add_nostob(bio, field, 0, io_length[j]);
		}
		for (j = 0; j < ARRAY_SIZE(io_length); ++j) {
			bio = m0_be_log_io_be_io(&lio[j]);
			m0_be_log_store_io_translate(ls, offset + io_offset[j],
						     bio);
		}
		for (j = 0; j < ARRAY_SIZE(io_length); ++j) {
			size = m0_be_io_size(m0_be_log_io_be_io(&lio[j]));
			M0_UT_ASSERT(size == io_length[j]);
		}
		be_ut_log_store_io_intersect_check(lio_arr,
						   BE_UT_LOG_STORE_IO_NR);

		M0_BE_OP_SYNC(op, m0_be_log_store_io_discard(ls, offset, &op));
		rc = m0_be_log_store_io_window(ls, offset, &length);
		M0_UT_ASSERT(M0_IN(rc, (0, -EINVAL)));
		M0_UT_ASSERT(ergo(rc == 0, length != 0));

		offset += BE_UT_LOG_STORE_IO_TRANSLATE_STEP * alignment;
	}
	for (i = 0; i < BE_UT_LOG_STORE_IO_NR; ++i) {
		m0_be_log_io_deallocate(&lio[i]);
		m0_be_log_io_fini(&lio[i]);
	}
	m0_free_aligned(field, field_length, bshift);
	m0_free(lio_arr);
	m0_free(lio);
}

void m0_be_ut_log_store_io_translate(void)
{
	be_ut_log_store_test(&be_ut_log_store_io_translate);
}

static void be_ut_log_store_rbuf(struct m0_be_log_store *ls,
				 bool                    first_run)
{
	enum m0_be_log_store_io_type   io_type;
	struct m0_be_log_io           *lio;
	struct m0_be_log_io          **lio_arr;
	struct m0_be_log_io           *lio_r;
	struct m0_be_log_io           *lio_w;
	struct m0_be_log_io           *lio_w0;
	unsigned                       nr;
	unsigned                       nr_lio_read = 0;
	unsigned                       nr_lio_write;
	unsigned                       iter;
	unsigned                       iter_r;
	unsigned                       iter_w;
	int                            i;
	bool                           eq;

	/* number of redundant buffer IOs is the same for read and write */
	for (i = 0; i < 2; ++i) {
		io_type = i == 0 ?
			  M0_BE_LOG_STORE_IO_READ : M0_BE_LOG_STORE_IO_WRITE;
		nr = 0;
		for (lio = m0_be_log_store_rbuf_io_first(ls, io_type,
							 NULL, &iter);
		     lio != NULL;
		     lio = m0_be_log_store_rbuf_io_next(ls, io_type,
							NULL, &iter))
			++nr;
		if (i == 0)
			nr_lio_read = nr;
		else
			nr_lio_write = nr;
	}
	M0_UT_ASSERT(nr_lio_read == nr_lio_write);
	M0_UT_ASSERT(nr_lio_read == BE_UT_LOG_STORE_RBUF_NR);
	/* read and write IOs point to the same place in backing store */
	lio_r = m0_be_log_store_rbuf_io_first(ls, M0_BE_LOG_STORE_IO_READ,
					      NULL, &iter_r);
	lio_w = m0_be_log_store_rbuf_io_first(ls, M0_BE_LOG_STORE_IO_WRITE,
					      NULL, &iter_w);
	do {
		eq = m0_be_io_offset_stob_is_eq(m0_be_log_io_be_io(lio_r),
						m0_be_log_io_be_io(lio_w));
		M0_UT_ASSERT(eq);
		lio_r = m0_be_log_store_rbuf_io_next(ls,
						      M0_BE_LOG_STORE_IO_READ,
						      NULL, &iter_r);
		lio_w = m0_be_log_store_rbuf_io_next(ls,
						      M0_BE_LOG_STORE_IO_WRITE,
						      NULL, &iter_w);
	} while (lio_r != NULL || lio_w != NULL);
	M0_UT_ASSERT(lio_r == NULL && lio_w == NULL);
	/* all write IOs are done from the same place in memory */
	io_type = M0_BE_LOG_STORE_IO_WRITE;
	lio_w0 = m0_be_log_store_rbuf_io_first(ls, io_type, NULL, &iter_w);
	while (1) {
		lio_w = m0_be_log_store_rbuf_io_next(ls, io_type,
						     NULL, &iter_w);
		if (lio_w == NULL)
			break;
		eq = m0_be_io_ptr_user_is_eq(m0_be_log_io_be_io(lio_w0),
					     m0_be_log_io_be_io(lio_w));
		M0_UT_ASSERT(eq);
	}
	/* all read I/Os doesn't intersect in backing storage */
	M0_ALLOC_ARR(lio_arr, nr_lio_read);
	M0_UT_ASSERT(lio_arr != NULL);
	io_type = M0_BE_LOG_STORE_IO_READ;
	i = 0;
	for (lio = m0_be_log_store_rbuf_io_first(ls, io_type, NULL, &iter_r);
	     lio != NULL;
	     lio = m0_be_log_store_rbuf_io_next(ls, io_type, NULL, &iter_r)) {
		lio_arr[i++] = lio;
	}
	M0_UT_ASSERT(i == nr_lio_read);
	be_ut_log_store_io_intersect_check(lio_arr, nr_lio_read);
	m0_free(lio_arr);
}

void m0_be_ut_log_store_rbuf(void)
{
	be_ut_log_store_test(&be_ut_log_store_rbuf);
}

/* @todo test rbuf and cbuf intersections on a backing storage */

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
