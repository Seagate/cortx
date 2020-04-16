/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ucontext.h>               /* getcontext */
#include <signal.h>                 /* struct stack */
#include <assert.h>                 /* assert */

#include "lib/getopts.h"
#include "lib/thread.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"

enum clovis_idx_service {
	IDX_MERO = 1,
	IDX_CASS,
};

/* Clovis parameters */
static char                    *clovis_local_addr;
static char                    *clovis_ha_addr;
static char                    *clovis_prof;
static char                    *clovis_proc_fid;
static char                    *clovis_tests;
enum   clovis_idx_service       clovis_index_service;
static struct m0_clovis_config  clovis_conf;

#include "clovis/clovis_internal.h"
#include "sm/sm.h"

static struct m0_idx_dix_config  dix_conf;
static struct m0_idx_cass_config cass_conf;

static int clovis_st_init_instance(void)
{
	int               rc;
	struct m0_clovis *instance = NULL;

	clovis_conf.cc_is_oostore            = true;
	clovis_conf.cc_is_read_verify        = false;
	clovis_conf.cc_local_addr            = clovis_local_addr;
	clovis_conf.cc_ha_addr               = clovis_ha_addr;
	clovis_conf.cc_profile               = clovis_prof;
	clovis_conf.cc_process_fid           = clovis_proc_fid;
	clovis_conf.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	if (clovis_index_service == IDX_MERO) {
		clovis_conf.cc_idx_service_id   = M0_CLOVIS_IDX_DIX;
		/* DIX- Metadata is created by m0dixinit() in ST script. */
		dix_conf.kc_create_meta = false;
		clovis_conf.cc_idx_service_conf = &dix_conf;
	} else if (clovis_index_service == IDX_CASS) {
		/* Cassandra index driver */
		cass_conf.cc_cluster_ep              = "127.0.0.1";
		cass_conf.cc_keyspace                = "clovis_index_keyspace";
		cass_conf.cc_max_column_family_num   = 1;
		clovis_conf.cc_idx_service_id        = M0_CLOVIS_IDX_CASS;
		clovis_conf.cc_idx_service_conf      = &cass_conf;
	} else {
		rc = -EINVAL;
		console_printf("Invalid index service configuration.");
		goto exit;
	}

	rc = m0_clovis_init(&instance, &clovis_conf, true);
	if (rc != 0)
		goto exit;

	clovis_st_set_instance(instance);

exit:
	return rc;
}

static void clovis_st_fini_instance(void)
{
	m0_clovis_fini(clovis_st_get_instance(), true);
}

static int clovis_st_wait_workers(void)
{
	return clovis_st_stop_workers();
}

void clovis_st_usage()
{
	console_printf(
		"Clovis System Test Framework: c0st\n"
		"    -- Note: if -l|-u is used, no Clovis details are needed\n"
		"       otherwise, -m, -h and -p have to be provided\n"
		"Usage: c0st "
		"[-l|-u|-r] [-m local] [-h ha] [-p prof_opt] "
		"[-t tests]\n"
		"    -l                List all tests\n"
		"    -m local          Local(my) end point address\n"
		"    -h ha             HA address \n"
		"    -p prof_opt       Profile options for Clovis\n"
		"    -f proc_fid       Process FID for rmservice@Clovis\n"
		"    -t tests          Only run the specified tests\n"
		"    -I index service  Index service(Cassandra, mock, Mero-KVS)\n"
		"    -r                Run tests in a suite in random order\n"
		"    -u                Print usage\n"
		);
}

void clovis_st_get_opts(int argc, char **argv)
{
	int rc;

	if (argc < 2) {
		clovis_st_usage();
		exit(-1);
	}

	clovis_local_addr = NULL;
	clovis_ha_addr    = NULL;
	clovis_prof       = NULL;
	clovis_proc_fid   = NULL;
	clovis_tests      = NULL;

	rc = M0_GETOPTS("clovis_st", argc, argv,
			M0_HELPARG('?'),
			M0_VOIDARG('i', "more verbose help",
					LAMBDA(void, (void) {
						clovis_st_usage();
						exit(0);
					})),
			M0_VOIDARG('l', "Lists all clovis tests",
					LAMBDA(void, (void) {
						clovis_st_list(true);
						exit(0);
					})),
			M0_STRINGARG('m', "Local endpoint address",
					  LAMBDA(void, (const char *string) {
					       clovis_local_addr = (char*)string;
					  })),
			M0_STRINGARG('h', "HA address",
					  LAMBDA(void, (const char *str) {
					       clovis_ha_addr = (char*)str;
					  })),
			M0_STRINGARG('f', "Process FID",
					  LAMBDA(void, (const char *str) {
					       clovis_proc_fid = (char*)str;
					  })),
			M0_STRINGARG('p', "Profile options for Clovis",
					  LAMBDA(void, (const char *str) {
					       clovis_prof = (char*)str;
					  })),
			M0_NUMBERARG('I', "Index service id",
					  LAMBDA(void, (int64_t service_idx) {
					       clovis_index_service = service_idx;
				          })),
			M0_VOIDARG('r', "Ramdom test mode",
				        LAMBDA(void, (void) {
					       clovis_st_set_test_mode(
							CLOVIS_ST_RAND_MODE);
				        })),
			M0_STRINGARG('t', "Lists clovis tests",
					LAMBDA(void, (const char *str) {
					       clovis_tests = (char*)str;
					})));
	/* some checks */
	if (rc != 0 || clovis_local_addr == NULL || clovis_ha_addr == NULL ||
	    clovis_prof == NULL || clovis_proc_fid == NULL)
	{
		clovis_st_usage();
		exit(0);
	}

	/*
	 * Set tests to be run. If clovis_tests == NULL, all ST will
	 * be executed.
	 */
	clovis_st_set_tests(clovis_tests);
}


int main(int argc, char **argv)
{
	int           rc;
	static struct m0 instance;

	m0_instance_setup(&instance);
	rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_ONCE);
	if (rc != 0) {
		fprintf(stderr, "Cannot init module %i\n", rc);
		return rc;
	}

	/* initialise Clovis ST */
	clovis_st_init();
	clovis_st_add_suites();

	/* Get input parameters */
	clovis_st_get_opts(argc, argv);

	/* currently, all threads share the same instance */
	if (clovis_st_init_instance() < 0) {
		fprintf(stderr, "clovis_init failed!\n");
		return -1;
	}

	/* start worker threads */
	clovis_st_set_nr_workers(1);
	clovis_st_cleaner_init();
	clovis_st_start_workers();

	/* wait till all workers complete */
	rc = clovis_st_wait_workers();
	clovis_st_cleaner_fini();

	/* clean-up */
	clovis_st_fini_instance();
	clovis_st_fini();

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
