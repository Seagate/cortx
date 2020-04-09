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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 30-Oct-2014
 *
 * Subsequent Modification: Abhishek Saha <abhishek.saha@seagate.com>
 * Modification Date : 31-Dec-2018
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

#include "conf/obj.h"
#include "fid/fid.h"
#include "lib/trace.h"
#include "lib/string.h"
#include "lib/getopts.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "clovis/st/utils/clovis_helper.h"

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;

extern struct m0_addb_ctx m0_clovis_addb_ctx;

static void copy_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]... SOURCE\n"
"Copy SOURCE to MERO.\n"
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
"  -L, --layout-id      INT       layout ID, range: [1-14]\n"
"  -e, --enable-locks             enables acquiring and releasing RW locks "
                                 "before and after performing IO.\n"
"  -u, --update-mode              updates the exisiting object with data\n"
"  -S, --msg_size       INT       Max RPC msg size 64k i.e 65536\n"
                                 "Note: this should match with m0d's current "
                                 "rpc msg size\n"
"  -q, --min_queue      INT       Minimum length of the receive queue i.e 16\n"
"  -h, --help                     shows this help text and exit\n"
, prog_name);
}

int main(int argc, char **argv)
{
	struct clovis_utility_param  cp_param;
	int                          rc;

	clovis_utility_args_init(argc, argv, &cp_param,
			         &dix_conf, &clovis_conf, &copy_usage);

	rc = clovis_init(&clovis_conf, &clovis_container, &clovis_instance);
	if (rc < 0) {
		fprintf(stderr, "clovis_init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	if (argv[optind] != NULL)
		cp_param.cup_file = strdup(argv[optind]);

	rc = clovis_write(&clovis_container, cp_param.cup_file,
			  cp_param.cup_id, cp_param.cup_block_size,
			  cp_param.cup_block_count, cp_param.cup_update_mode,
			  cp_param.cup_take_locks);
	if (rc < 0) {
		fprintf(stderr, "clovis_write failed! rc = %d\n", rc);
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
