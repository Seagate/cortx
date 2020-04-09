/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 04/12/2011
 */

#include <linux/version.h>    /* LINUX_VERSION_CODE */
#include <linux/module.h>

#include "lib/thread.h"       /* M0_THREAD_INIT */
#include "ut/ut.h"            /* m0_ut_add */
#include "module/instance.h"  /* m0 */
#include "ut/module.h"        /* m0_ut_module */

MODULE_AUTHOR("Xyratex International");
MODULE_DESCRIPTION("Mero Unit Test Module");
MODULE_LICENSE("GPL"); /* should match license of m0mero.ko module */

static char *tests;
module_param(tests, charp, S_IRUGO);
MODULE_PARM_DESC(tests, " list of tests to run in format"
		 " 'suite[:test][,suite[:test]]'");

static char *exclude;
module_param(exclude, charp, S_IRUGO);
MODULE_PARM_DESC(exclude, " list of tests to exclude in format"
		 " 'suite[:test][,suite[:test]]'");

/* sort test suites in alphabetic order */
extern struct m0_ut_suite m0_klibm0_ut; /* test lib first */
extern struct m0_ut_suite addb2_base_ut;
extern struct m0_ut_suite addb2_consumer_ut;
extern struct m0_ut_suite addb2_hist_ut;
extern struct m0_ut_suite addb2_sys_ut;
extern struct m0_ut_suite be_ut;
extern struct m0_ut_suite buffer_pool_ut;
extern struct m0_ut_suite bulkio_client_ut;
extern struct m0_ut_suite m0_net_bulk_if_ut;
extern struct m0_ut_suite m0_net_bulk_mem_ut;
extern struct m0_ut_suite m0_net_lnet_ut;
extern struct m0_ut_suite m0_net_test_ut;
extern struct m0_ut_suite m0_net_tm_prov_ut;
extern struct m0_ut_suite conn_ut;
extern struct m0_ut_suite dtm_dtx_ut;
extern struct m0_ut_suite dtm_nucleus_ut;
extern struct m0_ut_suite dtm_transmit_ut;
extern struct m0_ut_suite failure_domains_tree_ut;
extern struct m0_ut_suite failure_domains_ut;
extern struct m0_ut_suite file_io_ut;
extern struct m0_ut_suite fom_timedwait_ut;
extern struct m0_ut_suite frm_ut;
extern struct m0_ut_suite ha_ut;
extern struct m0_ut_suite layout_ut;
extern struct m0_ut_suite ms_fom_ut;
extern struct m0_ut_suite packet_encdec_ut;
extern struct m0_ut_suite parity_math_ut;
extern struct m0_ut_suite parity_math_ssse3_ut;
extern struct m0_ut_suite reqh_service_ut;
extern struct m0_ut_suite rpc_mc_ut;
extern struct m0_ut_suite rm_ut;
extern struct m0_ut_suite session_ut;
extern struct m0_ut_suite sm_ut;
extern struct m0_ut_suite stob_ut;
extern struct m0_ut_suite xcode_ut;
extern struct m0_ut_suite di_ut;

//extern struct m0_ut_suite m0_loop_ut; /* m0loop driver */

static struct m0_thread ut_thread;

static void tests_add(struct m0_ut_module *m)
{
	/*
	 * set last argument to 'false' to disable test,
	 * it will automatically print a warning to console
	 */

	/* sort test suites in alphabetic order */
	m0_ut_add(m, &m0_klibm0_ut, true);  /* test lib first */
	m0_ut_add(m, &addb2_base_ut, true);
	m0_ut_add(m, &addb2_consumer_ut, true);
	m0_ut_add(m, &addb2_hist_ut, true);
	m0_ut_add(m, &addb2_sys_ut, true);
	m0_ut_add(m, &di_ut, true);
	m0_ut_add(m, &file_io_ut, true);
	m0_ut_add(m, &be_ut, true);
	m0_ut_add(m, &buffer_pool_ut, true);
	m0_ut_add(m, &bulkio_client_ut, true);
	/*m0_ut_add(m, &m0_loop_ut);*/  /* disabled at compile time because
					   it's not ported to * Centos7 */
	m0_ut_add(m, &m0_net_bulk_if_ut, true);
	m0_ut_add(m, &m0_net_bulk_mem_ut, true);
	m0_ut_add(m, &m0_net_lnet_ut, true);
	m0_ut_add(m, &m0_net_test_ut, true);
	m0_ut_add(m, &m0_net_tm_prov_ut, true);

	m0_ut_add(m, &conn_ut, true);
	m0_ut_add(m, &dtm_nucleus_ut, true);
	m0_ut_add(m, &dtm_transmit_ut, true);
	m0_ut_add(m, &dtm_dtx_ut, true);
	m0_ut_add(m, &failure_domains_tree_ut, true);
	m0_ut_add(m, &failure_domains_ut, true);
	m0_ut_add(m, &fom_timedwait_ut, true);
	m0_ut_add(m, &frm_ut, true);
	m0_ut_add(m, &ha_ut, true);
	m0_ut_add(m, &layout_ut, true);
	m0_ut_add(m, &ms_fom_ut, true);
	m0_ut_add(m, &packet_encdec_ut, true);
	m0_ut_add(m, &parity_math_ut, true);
	m0_ut_add(m, &reqh_service_ut, true);
	m0_ut_add(m, &rm_ut, true);
	m0_ut_add(m, &rpc_mc_ut, true);
	m0_ut_add(m, &session_ut, true);
	m0_ut_add(m, &sm_ut, true);
	m0_ut_add(m, &stob_ut, true);
	m0_ut_add(m, &xcode_ut, true);
}

static void run_kernel_ut(int _)
{
	printk(KERN_INFO "Mero Kernel Unit Test\n");
	m0_ut_run();
}

static int __init m0_ut_module_init(void)
{
	static struct m0 instance;
	struct m0_ut_module *ut;
	int                  rc;

	M0_THREAD_ENTER;
	m0_instance_setup(&instance);
	(void)m0_ut_module_type.mt_create(&instance);
	ut = instance.i_moddata[M0_MODULE_UT];

	if (tests != NULL && exclude != NULL)
		return EINVAL; /* only one of the lists should be provided */

	ut->ut_exclude = (exclude != NULL);
	ut->ut_tests = ut->ut_exclude ? exclude : tests;

	tests_add(ut);

	rc = m0_ut_init(&instance);
	if (rc != 0)
		/*
		 * We still need to raise m0 instance to M0_LEVEL_INST_ONCE,
		 * otherwise an attempt to unload m0kut kernel module will
		 * result in kernel panic.
		 */
		m0_module_init(&instance.i_self, M0_LEVEL_INST_ONCE);
	rc = M0_THREAD_INIT(&ut_thread, int, NULL, &run_kernel_ut, 0, "m0kut");
	M0_ASSERT(rc == 0);
	return rc;
}

static void __exit m0_ut_module_fini(void)
{
	M0_THREAD_ENTER;
	m0_thread_join(&ut_thread);
	m0_ut_fini();
}

module_init(m0_ut_module_init)
module_exit(m0_ut_module_fini)

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
