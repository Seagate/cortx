/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 19-Oct-2016
 */

/**
 * @addtogroup addb2
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB
#include "lib/trace.h"
#include "addb2/histogram.h"
#include "addb2/internal.h"               /* m0_addb2__counter_snapshot */

static const struct m0_addb2_sensor_ops hist_ops;

void m0_addb2_hist_add(struct m0_addb2_hist *hist, int64_t min, int64_t max,
		       uint64_t label, int idx)
{
	struct m0_addb2_counter *c = &hist->hi_counter;

	M0_PRE(M0_IS0(hist));
	M0_PRE(max > min);

	m0_addb2__counter_data_init(&c->co_val);
	hist->hi_data.hd_min = min;
	hist->hi_data.hd_max = max;
	m0_addb2_sensor_add(&c->co_sensor, label, VALUE_MAX_NR, idx, &hist_ops);
}

void m0_addb2_hist_add_auto(struct m0_addb2_hist *hist, int skip,
			    uint64_t label, int idx)
{
	struct m0_addb2_counter *c = &hist->hi_counter;

	M0_PRE(M0_IS0(hist));
	M0_PRE(skip > 0);

	m0_addb2__counter_data_init(&c->co_val);
	hist->hi_skip = skip;
	m0_addb2_sensor_add(&c->co_sensor, label, VALUE_MAX_NR, idx, &hist_ops);
}

void m0_addb2_hist_del(struct m0_addb2_hist *hist)
{
	m0_addb2_sensor_del(&hist->hi_counter.co_sensor);
}

void m0_addb2_hist_mod(struct m0_addb2_hist *hist, int64_t val)
{
	m0_addb2_hist_mod_with(hist, val, 0);
}

void m0_addb2_hist_mod_with(struct m0_addb2_hist *hist,
			    int64_t val, uint64_t datum)
{
	struct m0_addb2_hist_data *hd = &hist->hi_data;

	if (hist->hi_skip > 0) {
		hd->hd_min = min64(hd->hd_min, val);
		hd->hd_max = max64(hd->hd_max, val);
		hist->hi_skip--;
	} else
		hist->hi_data.hd_bucket[m0_addb2_hist_bucket(hist, val)]++;
	m0_addb2_counter_mod_with(&hist->hi_counter, val, datum);
}

int m0_addb2_hist_bucket(const struct m0_addb2_hist *hist, int64_t val)
{
	const struct m0_addb2_hist_data *hd = &hist->hi_data;
	int                              idx;

	if (val < hd->hd_min)
		idx = 0;
	else if (val >= hd->hd_max)
		idx = M0_ADDB2_HIST_BUCKETS - 1;
	else
		idx = (val - hd->hd_min) * (M0_ADDB2_HIST_BUCKETS - 2) /
			(hd->hd_max - hd->hd_min) + 1;
	M0_POST(0 <= idx && idx < M0_ADDB2_HIST_BUCKETS);
	return idx;
}

static void hist_snapshot(struct m0_addb2_sensor *s, uint64_t *area)
{
	struct m0_addb2_hist      *hist = M0_AMB(hist, s, hi_counter.co_sensor);
	struct m0_addb2_hist_data *hd   = &hist->hi_data;
	int                        i;

	m0_addb2__counter_snapshot(s, area);
	area += M0_ADDB2_COUNTER_VALS;
	*(struct m0_addb2_hist_data *)area = *hd;
	for (i = 0; i < ARRAY_SIZE(hd->hd_bucket); ++i)
		hd->hd_bucket[i] = 0;
}

static void hist_fini(struct m0_addb2_sensor *s)
{;}

static const struct m0_addb2_sensor_ops hist_ops = {
	.so_snapshot = &hist_snapshot,
	.so_fini     = &hist_fini
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
