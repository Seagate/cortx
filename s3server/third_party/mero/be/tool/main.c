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
 * Original creation date: 24-May-2015
 */


/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include <stdio.h>      /* printf */
#include <string.h>     /* strcmp */
#include <stdlib.h>             /* EXIT_FAILURE */

#include "lib/string.h"         /* m0_streq */

#include "be/tool/st.h" /* m0_betool_st_mkfs */

static const char *betool_help = ""
"Usage: m0betool [cmd]\n"
"where [cmd] is one from 'st mkfs', 'st run' (without quotes)\n";


int main(int argc, char *argv[])
{
	int rc;

	if (argc == 3 && m0_streq(argv[1], "st") &&
	    (m0_streq(argv[2], "mkfs") || m0_streq(argv[2], "run"))) {
		if (strcmp(argv[2], "mkfs") == 0)
			rc = m0_betool_st_mkfs();
		else
			rc = m0_betool_st_run();
	} else {
		printf("%s", betool_help);
		rc = EXIT_FAILURE;
	}
	return rc;
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
