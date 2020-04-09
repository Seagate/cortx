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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 22-Jun-2015
 */


/**
 * @addtogroup be
 *
 * Known issues
 * - error handling using M0_ASSERT().
 * - there is no way to run m0_be_ut_backend_init() as if mkfs was already done.
 *   The function always does mkfs if it can't start BE without mkfs.
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/assert.h"         /* M0_ASSERT */
#include "lib/types.h"          /* UINT64_MAX */
#include "lib/time.h"           /* m0_time_now */
#include "lib/misc.h"           /* M0_SET0 */

#include "be/ut/helper.h"       /* m0_be_ut_backend */
#include "be/tool/common.h"     /* m0_betool_m0_init */

#include <stdlib.h>
#include <stdio.h>              /* snprintf */

enum {
	BETOOL_ST_SEG_SIZE      = 0x4000000,  /* 64MiB */
	BETOOL_ST_TX_STEP       = 0x400000,   /* 4MiB */
	BETOOL_ST_CAPTURE_STEP  = 0x20000,    /* 128KiB */
	BETOOL_ST_CAPTURE_BLOCK = 0x1F000,    /* 124KiB */
};

int m0_betool_st_mkfs(void)
{
	struct m0_be_ut_backend  ut_be = {};
	struct m0_be_seg        *seg;

	m0_betool_m0_init();
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_backend_seg_add2(&ut_be, BETOOL_ST_SEG_SIZE, false, NULL,
				  &seg);
	m0_be_ut_backend_fini(&ut_be);
	m0_betool_m0_fini();
	return 0;
}

static void betool_st_data_check(struct m0_be_ut_backend *ut_be,
				 struct m0_be_seg        *seg,
				 uint64_t                *fill)
{
	uint64_t  start;
	uint64_t  pos;
	uint64_t  tx_value;
	uint64_t *tx_values;
	uint64_t *value;
	int       tx_values_nr;
	int       i;
	int       j;
	int       jump_nr;
	int       tx_first;
	int       tx_last;

	M0_LOG(M0_DEBUG, "check that value is the same for each capture block");
	start = m0_round_up(m0_be_seg_reserved(seg), BETOOL_ST_TX_STEP);
	tx_values_nr = BETOOL_ST_SEG_SIZE / BETOOL_ST_TX_STEP;
	M0_ALLOC_ARR(tx_values, tx_values_nr);
	M0_ASSERT(tx_values != NULL);
	for (pos = start; pos < BETOOL_ST_SEG_SIZE; pos += BETOOL_ST_TX_STEP) {
		value = seg->bs_addr + pos;
		tx_value = *value;
		tx_values[pos / BETOOL_ST_TX_STEP] = tx_value;
		M0_LOG(M0_ALWAYS, "tx_value=%lu", tx_value);
		for (i = 0; i < BETOOL_ST_TX_STEP;
		     i += BETOOL_ST_CAPTURE_STEP) {
			value = seg->bs_addr + pos + i;
			for (j = 0; j < BETOOL_ST_CAPTURE_BLOCK /
					sizeof *value; ++j) {
				if (value[j] != tx_value) {
					M0_LOG(M0_ALWAYS, "step offset = %d "
					       "capture block index = %d "
					       "value[j]=%lu tx_value = %lu",
					       i, j, value[j], tx_value);
					M0_IMPOSSIBLE("data mismatch");
				}
			}
		}
	}
	*fill = 0;
	jump_nr = 0;
	tx_first = tx_values_nr;
	tx_last = -1;
	for (i = 0; i < tx_values_nr; ++i) {
		if (tx_values[i] != 0) {
			tx_first = i;
			break;
		}
	}
	for (i = tx_values_nr - 1; i >= 0; --i) {
		if (tx_values[i] != 0) {
			tx_last = i;
			break;
		}
	}
	M0_LOG(M0_ALWAYS, "tx_first=%d tx_last=%d", tx_first, tx_last);
	for (i = tx_first; i <= tx_last; ++i) {
		if (i > 0 && tx_values[i - 1] > tx_values[i]) {
			M0_LOG(M0_ALWAYS, "jump from %lu to %lu",
			       *fill, tx_values[i]);
				if (*fill != 0)
					++jump_nr;
				M0_ASSERT(jump_nr <= 1);
		}
		*fill = max_check(tx_values[i], *fill);
	}
	++*fill;
	m0_free(tx_values);
}

enum betool_st_event {
	BETOOL_ST_TIME_BEGIN,
	BETOOL_ST_TIME_TX_OPEN,
	BETOOL_ST_TIME_SEG_FILL,
	BETOOL_ST_TIME_TX_CAPTURE,
	BETOOL_ST_TIME_TX_CLOSE_WAIT,
	BETOOL_ST_TIME_END,
	BETOOL_ST_TIME_NR,
};

static const char *betool_st_event_descr[] = {
	"begin",
	"tx open",
	"fill",
	"tx capture",
	"tx close&wait",
	"end",
};
M0_BASSERT(ARRAY_SIZE(betool_st_event_descr) == BETOOL_ST_TIME_NR);

static void betool_st_event_time(m0_time_t            *time,
				 enum betool_st_event  event)
{
	time[event] = m0_time_now();
}

static void betool_st_event_time_print(m0_time_t  *time,
				       const char *info,
				       uint64_t    fill)
{
	char   buf[0x100];
	size_t len = sizeof buf;
	int    idx = 0;
	int    printed;
	int    i;

	printed = snprintf(buf, ARRAY_SIZE(buf), "%s fill: %lu", info, fill);
	for (i = 1; i < BETOOL_ST_TIME_NR; ++i) {
		len -= printed;
		idx += printed;
		M0_ASSERT(buf[idx] == '\0');
		printed = snprintf(&buf[idx], len, " %s: +%luus",
				   betool_st_event_descr[i],
				   (time[i] - time[0]) / 1000);
		M0_ASSERT(printed >= 0);
	}
	M0_LOG(M0_ALWAYS, "%s", (const char *)buf);
}

/* pass fill_end = UINT64_MAX for the infinite loop */
static void betool_st_data_write(struct m0_be_ut_backend *ut_be,
				 struct m0_be_seg        *seg,
				 uint64_t                 fill,
				 uint64_t                 fill_end)
{
	struct m0_be_tx_credit  cred;
	struct m0_be_reg        reg;
	struct m0_be_tx        *tx;
	m0_time_t              *time;
	uint64_t                start;
	uint64_t                tx_start;
	uint64_t                ringbuf_steps;
	uint64_t               *value;
	int                     step_nr;
	int                     rc;
	int                     i;
	int                     j;

	M0_ALLOC_PTR(tx);
	M0_ASSERT(tx != NULL);
	M0_ALLOC_ARR(time, BETOOL_ST_TIME_NR);
	M0_ASSERT(time != NULL);
	step_nr = BETOOL_ST_TX_STEP / BETOOL_ST_CAPTURE_STEP;
	cred    = M0_BE_TX_CREDIT(step_nr, BETOOL_ST_CAPTURE_BLOCK * step_nr);
	start   = m0_round_up(m0_be_seg_reserved(seg), BETOOL_ST_TX_STEP);
	ringbuf_steps = (BETOOL_ST_SEG_SIZE - start) / BETOOL_ST_TX_STEP;
	M0_LOG(M0_ALWAYS, "cred = "BETXCR_F, BETXCR_P(&cred));
	for (; fill < fill_end; ++fill) {
		betool_st_event_time(time, BETOOL_ST_TIME_BEGIN);

		tx_start = start + BETOOL_ST_TX_STEP * (fill % ringbuf_steps);
		M0_SET0(tx);
		m0_be_ut_tx_init(tx, ut_be);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
		betool_st_event_time(time, BETOOL_ST_TIME_TX_OPEN);

		for (i = 0; i < BETOOL_ST_TX_STEP;
		     i += BETOOL_ST_CAPTURE_STEP) {
			value = seg->bs_addr + tx_start + i;
			for (j = 0;
			     j < BETOOL_ST_CAPTURE_BLOCK / sizeof *value; ++j)
				value[j] = fill;
		}
		betool_st_event_time(time, BETOOL_ST_TIME_SEG_FILL);

		for (i = 0; i < BETOOL_ST_TX_STEP;
		     i += BETOOL_ST_CAPTURE_STEP) {
			reg = M0_BE_REG(seg, BETOOL_ST_CAPTURE_BLOCK,
					seg->bs_addr + tx_start + i);
			m0_be_tx_capture(tx, &reg);
		}
		betool_st_event_time(time, BETOOL_ST_TIME_TX_CAPTURE);

		m0_be_tx_close_sync(tx);
		betool_st_event_time(time, BETOOL_ST_TIME_TX_CLOSE_WAIT);

		m0_be_tx_fini(tx);
		betool_st_event_time(time, BETOOL_ST_TIME_END);
		betool_st_event_time_print(time, "cycle", fill);
	}
	m0_free(time);
	m0_free(tx);
}

int m0_betool_st_run(void)
{
	struct m0_be_ut_backend ut_be = {};
	struct m0_be_domain_cfg cfg = {};
	struct m0_be_seg        *seg;
	uint64_t                 fill_start;

	m0_betool_m0_init();
	m0_be_ut_backend_cfg_default(&cfg);
	M0_LOG(M0_ALWAYS, "recovering...");
	m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_LOG(M0_ALWAYS, "recovered.");
	seg = m0_be_domain_seg_first(&ut_be.but_dom);
	M0_LOG(M0_ALWAYS, "segment with addr=%p and size=%lu found",
	       seg->bs_addr, seg->bs_size);
	M0_ASSERT_INFO(seg->bs_size == BETOOL_ST_SEG_SIZE,
		       "seg->bs_size=%lu BETOOL_ST_SEG_SIZE=%d",
		       seg->bs_size, BETOOL_ST_SEG_SIZE);
	betool_st_data_check(&ut_be, seg, &fill_start);
	betool_st_data_write(&ut_be, seg, fill_start, UINT64_MAX);
	// betool_st_data_write(&ut_be, seg, fill_start, 10000);
	m0_be_ut_backend_fini(&ut_be);
	m0_betool_m0_fini();
	return 0;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
