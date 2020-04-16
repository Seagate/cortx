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
 *                   Ankit Yadav     <ankit.yadav@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 30-Oct-2014
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "lib/memory.h"    /* m0_free() */
#include "lib/trace.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/st/utils/clovis_helper.h"

/* Currently Clovis can write at max 100 blocks in
 * a single request. This will change in future. */
enum { CLOVIS_MAX_BLOCK_COUNT = 100 };

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;

static void copy_thread_launch(struct clovis_copy_mt_args *args)
{
	int index;

	/* lock ensures that each thread writes on different object id */
	m0_mutex_lock(&args->cma_mutex);
	index = args->cma_index;
	args->cma_utility->cup_id = args->cma_ids[index];
	args->cma_index++;
	m0_mutex_unlock(&args->cma_mutex);
	args->cma_rc[index] = clovis_write(&clovis_container,
					   args->cma_utility->cup_file,
					   args->cma_utility->cup_id,
					   args->cma_utility->cup_block_size,
					   args->cma_utility->cup_block_count,
					   false, false);
}

static void copy_mt_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]... SOURCE\n"
"Copy SOURCE to MERO (Multithreaded: One thread per object).\n"
"Designed to dump large amount of data into Mero"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR      local endpoint address\n"
"  -H, --ha             ADDR      HA endpoint address\n"
"  -p, --profile        FID       profile FID\n"
"  -P, --process        FID       process FID\n"
"  -o, --object         FID       ID of the first mero object\n"
"  -n, --n_obj          INT       No of objects to write\n"
"  -s, --block-size     INT       block size multiple of 4k in bytes or with " \
				 "suffix b/k/m/g. Ex: 1k=1024, 1m=1024*1024\n"
"  -c, --block-count    INT       number of blocks to copy, can give with " \
				 "suffix b/k/m/g/K/M/G. Ex: 1k=1024, " \
				 "1m=1024*1024, 1K=1000 1M=1000*1000\n"
"  -L, --layout-id      INT       layout ID, range: [1-14]\n"
"  -u, --update-mode              updates the exisiting object with data\n"
"  -r, --read-verify              verify parity after reading the data\n"
"  -S, --msg_size       INT       Max RPC msg size 64k i.e 65536\n"
                                 "Note: this should match with m0d's current "
                                 "rpc msg size\n"
"  -q, --min_queue      INT       Minimum length of the receive queue i.e 16\n"
"  -h, --help                     shows this help text and exit\n"
, prog_name);
}

int main(int argc, char **argv)
{
	struct clovis_utility_param  copy_mt_params;
	struct clovis_copy_mt_args   copy_mt_args;
	struct m0_thread            *copy_th = NULL;
	int                          i = 0;
	int                          rc;
	int                          obj_nr;

	clovis_utility_args_init(argc, argv, &copy_mt_params,
				 &dix_conf, &clovis_conf, &copy_mt_usage);

	rc = clovis_init(&clovis_conf, &clovis_container, &clovis_instance);
	if (rc < 0) {
		fprintf(stderr, "clovis_init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	if (argv[optind] != NULL)
		copy_mt_params.cup_file = strdup(argv[optind]);

	copy_mt_args.cma_index = 0;
	copy_mt_args.cma_utility = &copy_mt_params;
	m0_mutex_init(&copy_mt_args.cma_mutex);
	obj_nr = copy_mt_params.cup_n_obj;

	M0_ALLOC_ARR(copy_th, obj_nr);
	M0_ALLOC_ARR(copy_mt_args.cma_ids, obj_nr);
	M0_ALLOC_ARR(copy_mt_args.cma_rc, obj_nr);

	for (i = 0; i < obj_nr; ++i) {
		copy_mt_args.cma_ids[i] = copy_mt_params.cup_id;
		copy_mt_args.cma_ids[i].u_lo = copy_mt_params.cup_id.u_lo + i;
	}

	/* launch one thread per object */
	for (i = 0; i < obj_nr; ++i)
		M0_THREAD_INIT(&copy_th[i], struct clovis_copy_mt_args *,
		               NULL, &copy_thread_launch, &copy_mt_args,
		               "Writer");

	for (i = 0; i < obj_nr; ++i) {
		m0_thread_join(&copy_th[i]);
		m0_thread_fini(&copy_th[i]);
		if (copy_mt_args.cma_rc[i] != 0) {
			fprintf(stderr, "clovis_copy failed for object Id: %lu,"
					" rc = %d\n",
					 copy_mt_args.cma_ids[i].u_lo,
					 copy_mt_args.cma_rc[i]);
			/*
			 * If any write fails, c0cp_mt operation is
			 * considered as unsuccessful.
			 */
			rc = -EIO;
		}
	}

	/* Clean-up */
	m0_mutex_fini(&copy_mt_args.cma_mutex);
	clovis_fini(clovis_instance);
	m0_free(copy_mt_args.cma_rc);
	m0_free(copy_mt_args.cma_ids);
	m0_free(copy_th);
	return M0_RC(rc);
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
