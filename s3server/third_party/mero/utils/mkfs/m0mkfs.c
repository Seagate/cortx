/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 20 Mar 2014
 */

#include <stdio.h>            /* fprintf */
#include <unistd.h>           /* pause */
#include <signal.h>           /* sigaction */
#include <sys/time.h>
#include <sys/resource.h>

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"         /* M0_SET0 */
#include "lib/uuid.h"         /* m0_node_uuid_string_set */

#include "mero/setup.h"
#include "mero/init.h"
#include "mero/version.h"
#include "module/instance.h"  /* m0 */
#include "net/lnet/lnet.h"
#include "reqh/reqh_service.h"

/**
   @addtogroup m0mkfs
   @{

   Abstract.

   This is mkfs stage0 tool. It prepares storage in a way m0d did before
   and allows to run m0d after that.

   Currently mkfs does the following:
   - prepare stob for IO;
   - prepare stob for addb;
   - prepare metadata and create initial metadata structures, such as:
     * root cob;
     * hierarachy root;
     * initial metadata tables.
   - segmap is created in normal file and saved in working dir. It then
     can be found by m0d in startup time and used;

   Options.

   Mkfs fully reuses startup code from mero/setup.c with tiny modificatios
   in order to support "erase" mode, that is, when mkfs is run on top of
   existing stobs and structures that have already been used by m0d.

   This is why m0_init() and m0_cs_init() and m0_cs_setup_env() are used
   same way as they used by m0d. The difference is that m0_cs_setup_env()
   has "mkfs" argument set to true and services and fops handling is not
   started for mkfs path.

   That said, that both, mkfs and m0d share the same set of options such
   as endpoints, bufer sizes, etc. Obviously, not all of them needed for
   mkfs as they are clearly m0d start options and not all of them needed
   by m0d as they may be applied to mkfs once, saved and reussd by m0d
   in startup time. This set of options that will not change between runs
   and needs to be saved by mkfs to some storare structures.

   This structure that we can use for saving things like that is called
   seg0. Integration with seg0 is part of stage1 work that will be started
   as soon as stage0 is finished.

   Anticipating stage1 work.

   - seg0 integration;
   - save those m0d start options, that don't change between runs, to seg0;
   - stop sharing options between m0d and mkfs. Each of them should
     handle only minimaly need set of them.
 */

/**
   Represents various network transports supported
   by a particular node in a cluster.
 */
static struct m0_net_xprt *cs_xprts[] = {
	&m0_net_lnet_xprt
};

M0_INTERNAL int main(int argc, char **argv)
{
	int              rc;
	int              i;
	struct m0_mero   mero_ctx;
	static struct m0 instance;
	struct rlimit    rlim = {10240, 10240};
	char            *uuid = NULL;

	if (argc > 1 &&
	    (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
		m0_build_info_print();
		exit(EXIT_SUCCESS);
	}

	rc = setrlimit(RLIMIT_NOFILE, &rlim);
	if (rc != 0) {
		fprintf(stderr, "\n Failed to setrlimit\n");
		goto out;
	}

	for (i = 0; i < argc; ++i)
		if (m0_streq("-u", argv[i])) {
			uuid = argv[i + 1];
			break;
		}
	m0_node_uuid_string_set(uuid);

	errno = 0;
	M0_SET0(&mero_ctx);
	rc = m0_init(&instance);
	if (rc != 0) {
		fprintf(stderr, "\n Failed to initialise Mero \n");
		goto out;
	}

	rc = m0_cs_init(&mero_ctx, cs_xprts, ARRAY_SIZE(cs_xprts), stderr, true);
	if (rc != 0) {
		fprintf(stderr, "\n Failed to initialise Mero \n");
		goto cleanup;
	}

        rc = m0_cs_setup_env(&mero_ctx, argc, argv);
	m0_cs_fini(&mero_ctx);
cleanup:
	m0_fini();
out:
	errno = rc < 0 ? -rc : rc;
	return errno;
}

/** @} endgroup m0mkfs */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
