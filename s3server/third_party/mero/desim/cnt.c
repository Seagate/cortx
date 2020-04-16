/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 */
/*
 * Copyright 2009 ClusterStor.
 *
 * Nikita Danilov.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <math.h>

#include "mero/magic.h"
#include "desim/sim.h"
#include "desim/cnt.h"

/**
   @addtogroup desim desim
   @{
 */

M0_TL_DESCR_DEFINE(cnts, "counters", static, struct cnt,
		   c_linkage, c_magic, M0_DESIM_CNT_MAGIC,
		   M0_DESIM_CNTS_HEAD_MAGIC);
M0_TL_DEFINE(cnts, static, struct cnt);

static struct m0_tl cnts;

M0_INTERNAL void cnt_init(struct cnt *cnt, struct cnt *parent,
			  const char *format, ...)
{
	va_list valist;

	memset(cnt, 0, sizeof *cnt);
	cnt->c_min = ~0ULL;
	cnt->c_max = 0;
	va_start(valist, format);
	sim_name_vaset(&cnt->c_name, format, valist);
	va_end(valist);
	cnt->c_parent = parent;
	cnts_tlink_init_at_tail(cnt, &cnts);
}

M0_INTERNAL void cnt_dump(struct cnt *cnt)
{
	cnt_t  avg;
	double sig;

	if (cnt->c_nr != 0) {
		avg = cnt->c_sum / cnt->c_nr;
		sig = sqrt(cnt->c_sq/cnt->c_nr - avg*avg);
		sim_log(NULL, SLL_INFO, "[%s: %llu (%llu) %llu %llu %f]\n",
			cnt->c_name, avg, cnt->c_nr,
			cnt->c_min, cnt->c_max, sig);
	} else
		sim_log(NULL, SLL_INFO, "[%s: empty]\n", cnt->c_name);
}

M0_INTERNAL void cnt_dump_all(void)
{
	struct cnt *scan;

	m0_tl_for(cnts, &cnts, scan)
		cnt_dump(scan);
	m0_tl_endfor;
}

M0_INTERNAL void cnt_fini(struct cnt *cnt)
{
	if (cnt->c_name != NULL)
		free(cnt->c_name);
	cnts_tlink_del_fini(cnt);
	cnt->c_magic = 0;
}


M0_INTERNAL void cnt_mod(struct cnt *cnt, cnt_t val)
{
	do {
		cnt->c_sum += val;
		cnt->c_nr++;
		cnt->c_sq += val*val;
		if (val > cnt->c_max)
			cnt->c_max = val;
		if (val < cnt->c_min)
			cnt->c_min = val;
	} while ((cnt = cnt->c_parent) != NULL);
}

M0_INTERNAL void cnt_global_init(void)
{
	cnts_tlist_init(&cnts);
}

M0_INTERNAL void cnt_global_fini(void)
{
	struct cnt *scan;

	m0_tl_for(cnts, &cnts, scan)
		cnt_fini(scan);
	m0_tl_endfor;

	cnts_tlist_fini(&cnts);
}

/** @} end of desim group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
