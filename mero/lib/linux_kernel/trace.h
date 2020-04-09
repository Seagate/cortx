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
 * Original author: Dima Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 5-Jul-2013
 */


#pragma once

#ifndef __MERO_LIB_LINUX_KERNEL_TRACE_H__
#define __MERO_LIB_LINUX_KERNEL_TRACE_H__

#include "lib/atomic.h"
#include "lib/types.h"


/**
 * @addtogroup trace
 *
 * @{
 */

/** Trace statistics */
struct m0_trace_stats {
	/** total number of trace records generated since program start */
	struct m0_atomic64  trs_rec_total;
	/** number of trace records generated withing last second */
	uint32_t            trs_rec_per_sec;
	/** amount of trace data generated withing last second, in bytes */
	uint32_t            trs_bytes_per_sec;
	/** average observed value of trs_rec_per_sec */
	uint32_t            trs_avg_rec_per_sec;
	/** average observed value of trs_bytes_per_sec */
	uint32_t            trs_avg_bytes_per_sec;
	/** maximum observed value of trs_rec_per_sec */
	uint32_t            trs_max_rec_per_sec;
	/** maximum observed value of trs_bytes_per_sec */
	uint32_t            trs_max_bytes_per_sec;
	/** average observed value of trace record size */
	uint32_t            trs_avg_rec_size;
	/** maximum observed value of trace record size */
	uint32_t            trs_max_rec_size;
};

M0_INTERNAL const struct m0_trace_stats *m0_trace_get_stats(void);

/** @} end of trace group */

#endif /* __MERO_LIB_LINUX_KERNEL_TRACE_H__ */


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
