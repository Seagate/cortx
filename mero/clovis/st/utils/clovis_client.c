/* -*- C -*- */
/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original author: Rajanikant Chirmade <rajanikant.chirmade@seagate.com>
 * Original creation date: 29-Aug-2018
 *
 * Subsequent Modification: Abhishek Saha <abhishek.saha@seagate.com>
 * Modification Date: 31-Dec-2108
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>

#include "lib/string.h"
#include "conf/obj.h"
#include "fid/fid.h"
#include "lib/memory.h"
#include "lib/trace.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "clovis/st/utils/clovis_helper.h"
#include "clovis/clovis_internal.h"

enum { CLOVIS_CMD_SIZE = 128 };

/* Clovis parameters */

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;

extern struct m0_addb_ctx m0_clovis_addb_ctx;

static void help(void)
{
	m0_console_printf("Help:\n");
	m0_console_printf("touch   OBJ_ID\n");
	m0_console_printf("write   OBJ_ID SRC_FILE BLOCK_SIZE BLOCK_COUNT\n");
	m0_console_printf("read    OBJ_ID DEST_FILE BLOCK_SIZE BLOCK_COUNT\n");
	m0_console_printf("delete  OBJ_ID\n");
	m0_console_printf("help\n");
	m0_console_printf("quit\n");
}

#define CLOVIS_GET_ARG(arg, cmd, saveptr)       \
	arg = strtok_r(cmd, "\n ", saveptr);   \
	if (arg == NULL) {                      \
		help();                         \
		continue;                       \
	}

#define CLOVIS_GET_COMMON_ARG(arg, fname, saveptr, id, \
			      block_size, block_count) \
	CLOVIS_GET_ARG(arg, NULL, &saveptr);           \
	clovis_obj_id_sscanf(arg, &id);                \
	CLOVIS_GET_ARG(arg, NULL, &saveptr);           \
	fname = strdup(arg);                           \
	CLOVIS_GET_ARG(arg, NULL, &saveptr);           \
	block_size = atoi(arg);                        \
	CLOVIS_GET_ARG(arg, NULL, &saveptr);           \
	block_count = atoi(arg);

static void c0client_usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]...\n"
"Launches Clovis client.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local          ADDR        local endpoint address\n"
"  -H, --ha             ADDR        HA endpoint address\n"
"  -p, --profile        FID         profile FID\n"
"  -P, --process        FID         process FID\n"
"  -L, --layout-id      INT         layout ID, range: [1-14]\n"
"  -e, --enable-locks               enables acquiring and releasing RW locks "
                                   "before and after performing IO.\n"
"  -r, --read-verify                verify parity after reading the data\n"
"  -S, --msg_size       INT         Max RPC msg size 64k i.e 65536\n"
                                   "Note: this should match with m0d's current "
                                   "rpc msg size\n"
"  -q, --min_queue      INT         Minimum length of the receive queue i.e 16\n"
"  -h, --help                       shows this help text and exit\n"
, prog_name);
}

int main(int argc, char **argv)
{
	struct clovis_utility_param  params;
	struct m0_fid                fid;
	int                          rc;
	char                        *arg;
	char                        *saveptr;
	char                        *cmd;
	char                        *fname = NULL;
	struct m0_uint128            id = M0_CLOVIS_ID_APP;
	int                          block_size;
	int                          block_count;

	clovis_utility_args_init(argc, argv, &params,
			         &dix_conf, &clovis_conf, &c0client_usage);

	rc = clovis_init(&clovis_conf, &clovis_container, &clovis_instance);
	if (rc < 0) {
		fprintf(stderr, "clovis_init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	memset(&fid, 0, sizeof fid);

	cmd = m0_alloc(CLOVIS_CMD_SIZE);
	if (cmd == NULL) {
		M0_ERR(-ENOMEM);
		goto cleanup;
	}

	do {
		m0_console_printf("c0clovis >>");
		arg = fgets(cmd, CLOVIS_CMD_SIZE, stdin);
		if (arg == NULL)
			continue;
		CLOVIS_GET_ARG(arg, cmd, &saveptr);
		if (strcmp(arg, "read") == 0) {
			CLOVIS_GET_COMMON_ARG(arg, fname, saveptr, id,
					      block_size, block_count);
			if (!clovis_entity_id_is_valid(&id))
				continue;
			rc = clovis_read(&clovis_container, id, fname,
					 block_size, block_count,
					 params.cup_take_locks);
		} else if (strcmp(arg, "write") == 0) {
			CLOVIS_GET_COMMON_ARG(arg, fname, saveptr, id,
					      block_size, block_count);
			if (!clovis_entity_id_is_valid(&id))
				continue;
			rc = clovis_write(&clovis_container, fname, id,
					  block_size, block_count, false,
					  params.cup_take_locks);
		} else if (strcmp(arg, "touch") == 0) {
			CLOVIS_GET_ARG(arg, NULL, &saveptr);
			clovis_obj_id_sscanf(arg, &id);
			if (!clovis_entity_id_is_valid(&id))
				continue;
			rc = clovis_touch(&clovis_container, id,
					  params.cup_take_locks);
		} else if (strcmp(arg, "delete") == 0) {
			CLOVIS_GET_ARG(arg, NULL, &saveptr);
			clovis_obj_id_sscanf(arg, &id);
			if (!clovis_entity_id_is_valid(&id))
				continue;
			rc = clovis_unlink(&clovis_container, id,
					   params.cup_take_locks);
		} else if (strcmp(arg, "help") == 0)
			help();
		else
			help();
		if (rc < 0) {
			fprintf(stderr, "IO failed! rc = %d\n", rc);
		}
	} while (arg == NULL || strcmp(arg, "quit") != 0);

	m0_free(cmd);
	if (fname != NULL)
		free(fname);

cleanup:
	/* Clean-up */
	clovis_fini(clovis_instance);

	return 0;
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
