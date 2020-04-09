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

#include <unistd.h>    /* daemon */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D
#include "lib/trace.h"

#include <sys/resource.h>
#include "lib/bitmap.h"
#include "mero/process_attr.h"
#include "module/instance.h"  /* m0 */

/**
   @addtogroup m0d
   @{
 */

static int reqh_memlimit_set(uint resource, uint64_t limit)
{
	int           rc;
	struct rlimit rl;

	M0_ENTRY();

	rc = -getrlimit(resource, &rl);
	if (rc !=0)
		return M0_ERR(rc);
	rl.rlim_cur = limit;
	rc = -setrlimit(resource, &rl);

	return M0_RC(rc);
}

int m0_cs_memory_limits_setup(struct m0 *instance)
{
	int                  rc;
	struct m0_proc_attr *proc_attr = &instance->i_proc_attr;

	M0_ENTRY();

	rc = proc_attr->pca_memlimit_as != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
		reqh_memlimit_set(RLIMIT_AS, proc_attr->pca_memlimit_as) : 0;
	rc = rc == 0 &&
	     proc_attr->pca_memlimit_rss != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
	     reqh_memlimit_set(RLIMIT_RSS, proc_attr->pca_memlimit_rss) : rc;
	rc = rc == 0 &&
	    proc_attr->pca_memlimit_stack != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
	    reqh_memlimit_set(RLIMIT_STACK, proc_attr->pca_memlimit_stack) : rc;
	rc = rc == 0 &&
	   proc_attr->pca_memlimit_memlock != M0_PROCESS_ATTRIBUTE_NO_MEMLIMIT ?
	   reqh_memlimit_set(RLIMIT_MEMLOCK, proc_attr->pca_memlimit_memlock) :
	   rc;
	return M0_RC(rc);
}

/** @} endgroup m0d */
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
