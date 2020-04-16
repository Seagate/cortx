/* -*- C -*- */
/*
 * COPYRIGHT 2014 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 30-Oct-2014
 *
 * Subsequent modification: Abhishek Saha <abhishek.saha@seagate.com>
 * Modification date: 31-Dec-2018
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>

#include "lib/string.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "clovis/st/utils/clovis_helper.h"

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;

static void touch_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]...\n"
"Create empty MERO object.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR        local endpoint address\n"
"  -H, --ha             ADDR        HA endpoint address\n"
"  -p, --profile        FID         profile FID\n"
"  -P, --process        FID         process FID\n"
"  -o, --object         FID         ID of the mero object\n"
                                   "Object id should larger than M0_CLOVIS_ID_APP\n"
                                   "The first 0x100000 ids are reserved for use by clovis.\n"
"  -L, --layout-id      INT         layout ID, range:[1-14]\n"
"  -n, --n_obj          INT         number of objects to touch\n"
"  -S, --msg_size       INT         Max RPC msg size 64k i.e 65536\n"
				   "Note: this should match with m0d's current "
				   "rpc msg size\n"
"  -q, --min_queue      INT         Minimum length of the receive queue i.e 16\n"
"  -e, --enable-locks               enables acquiring and releasing RW locks "
                                   "before and after performing IO.\n"
"  -h, --help                       shows this help text and exit\n"
, prog_name);
}


int main(int argc, char **argv)
{
	struct clovis_utility_param touch_params;
	struct m0_uint128           b_id = M0_CLOVIS_ID_APP;
	int                         i = 0;
	int                         rc;

	clovis_utility_args_init(argc, argv, &touch_params,
				 &dix_conf,&clovis_conf, &touch_usage);

	rc = clovis_init(&clovis_conf, &clovis_container, &clovis_instance);
	if (rc < 0) {
		fprintf(stderr, "clovis_init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	/* Setting up base object id to object id received */
	b_id.u_lo = touch_params.cup_id.u_lo;
	for (i = 0; i < touch_params.cup_n_obj; ++i) {
		touch_params.cup_id.u_lo = b_id.u_lo + i;
		rc = clovis_touch(&clovis_container, touch_params.cup_id,
				   touch_params.cup_take_locks);
		if (rc != 0) {
			fprintf(stderr, "Failed to touch  obj id: %lu, rc: %d\n",
				touch_params.cup_id.u_lo, rc);
		}
	}

	clovis_fini(clovis_instance);

	return rc;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
