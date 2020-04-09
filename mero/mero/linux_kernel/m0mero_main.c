/* -*- C -*- */
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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 10/13/2011
 * Modified by : Dima Chumak <dmitriy_chumak@xyratex.com>
 * Modification date: 7-Aug-2013
 */

#include <linux/kernel.h>  /* printk */
#include <linux/module.h>  /* THIS_MODULE */
#include <linux/init.h>    /* module_init */
#include <linux/version.h>

#include "lib/list.h"
#include "lib/thread.h"
#include "mero/init.h"
#include "mero/version.h"
#include "mero/linux_kernel/module.h"
#include "module/instance.h"  /* m0 */

M0_INTERNAL int __init mero_init(void)
{
	static struct m0     instance;
	const struct module *m;
	M0_THREAD_ENTER;

	m = m0_mero_ko_get_module();
	pr_info("mero: init\n");
	m0_build_info_print();
	pr_info("mero: module address: 0x%p\n", m);
	pr_info("mero: module core address: 0x%p\n", M0_MERO_KO_BASE(m));
	pr_info("mero: module core size: %u\n", M0_MERO_KO_SIZE(m));

	return m0_init(&instance);
}

M0_INTERNAL void __exit mero_exit(void)
{
	M0_THREAD_ENTER;
	pr_info("mero: cleanup\n");
	m0_fini();
}

module_init(mero_init);
module_exit(mero_exit);

MODULE_AUTHOR("Xyratex International");
MODULE_DESCRIPTION("Mero Library");
MODULE_LICENSE("GPL");

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
