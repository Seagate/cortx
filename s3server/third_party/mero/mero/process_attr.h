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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 2-Jul-2015
 */
#pragma once

#ifndef __MERO_MERO_PROCESS_H__
#define __MERO_MERO_PROCESS_H__

#include "lib/bitmap.h"        /* m0_bitmap */

struct m0;

/**
   @addtogroup m0d

   For reconfigure (see @ref m0_ss_process_req) some process attributes
   save into @ref m0 instance and apply on initialize module phase.

   @{
 */

enum {
	/**
	   Default value of Process attribute. Attribute not reply if set to
	   this value.
	*/
	M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT = 0
};

/**
   Define Process attribute structure, which contains information on
   core mask and memory limits. This attribute reply on reconfigure Mero.
*/
struct m0_proc_attr {
	/** Available cores mask */
	struct m0_bitmap pca_core_mask;
	/** Memory limits */
	uint64_t         pca_memlimit_as;
	uint64_t         pca_memlimit_rss;
	uint64_t         pca_memlimit_stack;
	uint64_t         pca_memlimit_memlock;
};

/**
 * Set memory limits.
 *
 * For each value of memory limit with non-default value from m0_proc_attr
 * use pair getrlimit and setrlimit.
 *
 * @return error code
 */
int m0_cs_memory_limits_setup(struct m0 *instance);

/** @}  */

#endif /* __MERO_MERO_PROCESS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
