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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 8-May-2016
 */

/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "lib/assert.h"                 /* M0_ASSERT */
#include "lib/misc.h"                   /* NULL */
#include "fid/fid.h"                    /* M0_FID_TINIT */
#include "ha/halon/interface.h"         /* m0_halon_interface */

int main(int argc, char *argv[])
{
	struct m0_halon_interface *hi;
	int rc;

	rc = m0_halon_interface_init(&hi, "", "", NULL, NULL);
	M0_ASSERT(rc == 0);
	rc = m0_halon_interface_start(hi, "0@lo:12345:42:100",
	                              &M0_FID_TINIT('r', 1, 1),
	                              &M0_FID_TINIT('s', 1, 1),
	                              &M0_FID_TINIT('s', 1, 2),
				      NULL, NULL, NULL, NULL, NULL,
	                              NULL, NULL, NULL, NULL);
	M0_ASSERT(rc == 0);
	m0_halon_interface_stop(hi);
	m0_halon_interface_fini(hi);
	return 0;
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of ha group */

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
