/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original Author: Madhavrao Vemuri <Madhav_Vemuri@xyratex.com>
 * Original creation date: 07/09/2011
 */

#include <linux/module.h>

#include "rpc_ping.h"
#include "lib/thread.h"

M0_INTERNAL int init_module(void)
{
	M0_THREAD_ENTER;
	return m0_rpc_ping_init();
}

M0_INTERNAL void cleanup_module(void)
{
	M0_THREAD_ENTER;
	m0_rpc_ping_fini();
}

MODULE_AUTHOR("Xyratex");
MODULE_DESCRIPTION("Mero Kernel rpc ping Module");
MODULE_LICENSE("proprietary");

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
