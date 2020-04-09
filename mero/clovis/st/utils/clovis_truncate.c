/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author:  Abhishek Saha <abhishek.saha@seagate.com>
 * Original creation date: 17-Jul-2019
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>

#include "lib/string.h"
#include "lib/trace.h"
#include "lib/getopts.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "clovis/st/utils/clovis_helper.h"

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;

extern struct m0_addb_ctx m0_clovis_addb_ctx;

static void trunc_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]...\n"
"Truncate MERO object to a given size.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR      local endpoint address\n"
"  -H, --ha             ADDR      HA endpoint address\n"
"  -p, --profile        FID       profile FID\n"
"  -P, --process        FID       process FID\n"
"  -o, --object         FID       ID of the mero object\n"
                                 "Object id should larger than M0_CLOVIS_ID_APP\n"
                                 "The first 0x100000 ids are reserved for use by clovis. \n"
"  -s, --block-size     INT       block size multiple of 4k in bytes or with " \
				 "suffix b/k/m/g. Ex: 1k=1024, 1m=1024*1024\n"
"  -c, --block-count    INT       number of blocks to copy, can give with " \
                                 "suffix b/k/m/g/K/M/G. Ex: 1k=1024, " \
                                 "1m=1024*1024, 1K=1000 1M=1000*1000\n"
"  -t, --trunc-len      INT       number of blocks to punch out,"
				 "can give with suffix b/k/m/g/K/M/G\n"
"  -L, --layout-id      INT       layout ID, range: [1-14]\n"
"  -e, --enable-locks              enables acquiring and releasing RW locks "
                                  "before and after performing IO.\n"
"  -S, --msg_size       INT       Max RPC msg size 64k i.e 65536\n"
				 "Note: this should match with m0d's current "
				 "rpc msg size\n"
"  -q, --min_queue      INT       Minimum length of the receive queue i.e 16\n"
"  -h, --help                      shows this help text and exit\n"
, prog_name);
}

int main(int argc, char **argv)
{
	int                         rc;
	struct clovis_utility_param trunc_params;

	clovis_utility_args_init(argc, argv, &trunc_params, &dix_conf,
				&clovis_conf, &trunc_usage);

	rc = clovis_init(&clovis_conf, &clovis_container, &clovis_instance);
	if (rc < 0) {
		fprintf(stderr, "clovis_init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	rc = clovis_truncate(&clovis_container, trunc_params.cup_id,
			      trunc_params.cup_block_size,
			      trunc_params.cup_block_count,
			      trunc_params.cup_trunc_len,
			      trunc_params.cup_take_locks);
	if (rc < 0) {
		fprintf(stderr, "clovis_truncate failed! rc = %d\n", rc);
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
