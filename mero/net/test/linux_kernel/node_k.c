/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 4/10/2012
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "net/test/node.h"

MODULE_AUTHOR("Xyratex International");
MODULE_DESCRIPTION("Mero Network Benchmark Module");
MODULE_LICENSE("proprietary");

static char	    *addr = NULL;
static char	    *addr_console = NULL;
static unsigned long timeout = 3;

module_param(addr, charp, S_IRUGO);
MODULE_PARM_DESC(addr, "endpoint address for node commands");

module_param(addr_console, charp, S_IRUGO);
MODULE_PARM_DESC(addr_console, "endpoint address for console commands");

module_param(timeout, ulong, S_IRUGO);
MODULE_PARM_DESC(timeout, "command send timeout, seconds");

static int __init m0_net_test_module_init(void)
{
	struct m0_net_test_node_cfg cfg = {
		.ntnc_addr	   = addr,
		.ntnc_addr_console = addr_console,
		.ntnc_send_timeout = M0_MKTIME(timeout, 0),
	};
	M0_THREAD_ENTER;
	return m0_net_test_node_module_initfini(&cfg);
}

static void __exit m0_net_test_module_fini(void)
{
	M0_THREAD_ENTER;
	m0_net_test_node_module_initfini(NULL);
}

module_init(m0_net_test_module_init)
module_exit(m0_net_test_module_fini)

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
