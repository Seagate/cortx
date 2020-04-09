/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original creation date: 15-Oct-2014
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include "clovis/clovis.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_assert.h"

//MODULE_AUTHOR("Seagate");
MODULE_AUTHOR("Xyratex International");
MODULE_DESCRIPTION("Clovis System Tests");
MODULE_LICENSE("proprietary");

enum clovis_idx_service {
	IDX_MERO = 1,
	IDX_CASS,
};

/* Module parameters */
static char                    *clovis_local_addr;
static char                    *clovis_ha_addr;
static char                    *clovis_prof;
static char                    *clovis_proc_fid;
static char                    *clovis_tests;
static int                      clovis_index_service;
static struct m0_clovis_config  clovis_conf;
static struct m0_idx_dix_config dix_conf;

module_param(clovis_local_addr, charp, S_IRUGO);
MODULE_PARM_DESC(clovis_local_addr, "Clovis Local Address");

module_param(clovis_ha_addr, charp, S_IRUGO);
MODULE_PARM_DESC(clovis_ha_addr, "Clovis HA Address");

module_param(clovis_prof, charp, S_IRUGO);
MODULE_PARM_DESC(clovis_prof, "Clovis Profile Opt");

module_param(clovis_proc_fid, charp, S_IRUGO);
MODULE_PARM_DESC(clovis_proc_fid, "Clovis Process FID for rmservice");

module_param(clovis_index_service, int, S_IRUGO);
MODULE_PARM_DESC(clovis_index_service, "Clovis index service");

module_param(clovis_tests, charp, S_IRUGO);
MODULE_PARM_DESC(clovis_tests, "Clovis ST tests");

static int clovis_st_init_instance(void)
{
	int		  rc;
	struct m0_clovis *instance = NULL;

	clovis_conf.cc_is_oostore            = true;
	clovis_conf.cc_is_read_verify        = false;
	clovis_conf.cc_local_addr            = clovis_local_addr;
	clovis_conf.cc_ha_addr               = clovis_ha_addr;
	clovis_conf.cc_profile               = clovis_prof;
	clovis_conf.cc_process_fid           = clovis_proc_fid;
	clovis_conf.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	/* TODO: Clovis ST for index APIs are disabled in kerne mode.
	 * Mero KVS need to implement a new feature demanded by MERO-2210
	 * System tests for Index will be enabled again after that feature
	 * is implemented in KVS backend.
	 */
	clovis_conf.cc_idx_service_id = M0_CLOVIS_IDX_DIX;
	dix_conf.kc_create_meta = false;
	clovis_conf.cc_idx_service_conf = &dix_conf;

	rc = m0_clovis_init(&instance, &clovis_conf, true);
	if (rc != 0)
		goto exit;

	clovis_st_set_instance(instance);

exit:
	return rc;
}

static void clovis_st_fini_instance(void)
{
	struct m0_clovis *instance;

	instance = clovis_st_get_instance();
	m0_clovis_fini(instance, true);
}

static int __init clovis_st_module_init(void)
{
	int rc;

	M0_CLOVIS_THREAD_ENTER;

	/* Initilises Clovis ST. */
	clovis_st_init();
	clovis_st_add_suites();

	/*
	 * Set tests to be run. If clovis_tests == NULL, all ST will
	 * be executed.
	 */
	clovis_st_set_tests(clovis_tests);

	/* Currently, all threads share the same instance. */
	rc = clovis_st_init_instance();
	if (rc < 0) {
		printk(KERN_INFO"clovis_init failed!\n");
		return rc;
	}

	/*
	 * Start worker threads.
	 */
	clovis_st_set_nr_workers(1);
	clovis_st_cleaner_init();
	return clovis_st_start_workers();
}

static void __exit clovis_st_module_fini(void)
{
	M0_CLOVIS_THREAD_ENTER;
	clovis_st_stop_workers();
	clovis_st_cleaner_fini();
	clovis_st_fini_instance();
	clovis_st_fini();
}

module_init(clovis_st_module_init)
module_exit(clovis_st_module_fini)

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
