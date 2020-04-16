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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 *                  Amit Jambure <amit_jambure@xyratex.com>
 * Metadata       : Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 05/04/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"

#include <linux/module.h>
#include <linux/init.h>

#include "m0t1fs/linux_kernel/m0t1fs.h"
#include "lib/memory.h"
#include "fid/fid.h"
#include "ioservice/io_fops.h"
#include "mdservice/md_fops.h"
#include "rpc/rpclib.h"
#include "rm/rm.h"
#include "net/lnet/lnet_core_types.h"
#include "mdservice/fsync_fops.h"
#include "sss/process_fops.h"            /* m0_ss_process_fops_init */
#include "ha/note_fops.h"
#include "addb2/global.h"
#include "addb2/sys.h"

struct m0_bitmap    m0t1fs_client_ep_tmid;
struct m0_mutex     m0t1fs_mutex;
struct m0_semaphore m0t1fs_cpus_sem;

static struct file_system_type m0t1fs_fs_type = {
	.owner        = THIS_MODULE,
	.name         = "m0t1fs",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	.mount        = m0t1fs_mount,
#else
	.get_sb       = m0t1fs_get_sb,
#endif
	.kill_sb      = m0t1fs_kill_sb,
	.fs_flags     = FS_BINARY_MOUNTDATA
};

M0_INTERNAL int m0t1fs_init(void)
{
	struct m0_addb2_sys *sys = m0_addb2_global_get();
	int                  rc;
	int                  cpus;

	M0_ENTRY();
	m0_mutex_init(&m0t1fs_mutex);
	/*
	 * [1 - M0_NET_LNET_TMID_MAX / 2] for clients.
	 * [M0_NET_LNET_TMID_MAX / 2 - M0_NET_LNET_TMID_MAX] for server ep.
	 */
	rc = m0_bitmap_init(&m0t1fs_client_ep_tmid, M0_NET_LNET_TMID_MAX / 2);
	if (rc != 0)
		goto out;
	m0_bitmap_set(&m0t1fs_client_ep_tmid, 0, true);

	rc = m0_mdservice_fsync_fop_init(NULL);
	if (rc != 0)
		goto bitmap_fini;

	rc = m0_ioservice_fop_init();
	if (rc != 0)
		goto fsync_fini;

	rc = m0_mdservice_fop_init();
	if (rc != 0)
		goto ioservice_fop_fini;

	rc = m0_ss_process_fops_init();
	if (rc != 0)
		goto mdservice_fop_fini;

	rc = m0t1fs_inode_cache_init();
	if (rc != 0)
		goto process_fops_fini;

	m0_addb2_sys_sm_start(sys);
	rc = m0_addb2_sys_net_start(sys);
	if (rc != 0)
		goto icache_fini;

	rc = register_filesystem(&m0t1fs_fs_type);
	if (rc != 0)
		goto addb2_fini;
	/*
	 * Limit the number of concurrent parity calculations
	 * to avoid starving other threads (especially LNet) out.
	 *
	 * Note: the exact threshold number may come from configuration
	 * database later where it can be specified per-node.
	 */
	cpus = (num_online_cpus() / 2) ?: 1;
	printk(KERN_INFO "mero: max CPUs for parity calcs: %d\n", cpus);
	m0_semaphore_init(&m0t1fs_cpus_sem, cpus);
	return M0_RC(0);

addb2_fini:
	m0_addb2_sys_net_stop(sys);
	m0_addb2_sys_sm_stop(sys);
icache_fini:
	m0t1fs_inode_cache_fini();
process_fops_fini:
	m0_ss_process_fops_fini();
mdservice_fop_fini:
	m0_mdservice_fop_fini();
ioservice_fop_fini:
	m0_ioservice_fop_fini();
fsync_fini:
	m0_mdservice_fsync_fop_fini();
bitmap_fini:
	m0_bitmap_fini(&m0t1fs_client_ep_tmid);
	m0_mutex_fini(&m0t1fs_mutex);
out:
	return M0_ERR(rc);
}

M0_INTERNAL void m0t1fs_fini(void)
{
	struct m0_addb2_sys *sys;
	M0_THREAD_ENTER;
	M0_ENTRY();

	sys = m0_addb2_global_get();

	(void)unregister_filesystem(&m0t1fs_fs_type);

	m0_addb2_sys_net_stop(sys);
	m0_addb2_sys_sm_stop(sys);
	m0t1fs_inode_cache_fini();
	m0_ha_state_fop_fini();
	m0_mdservice_fop_fini();
	m0_ioservice_fop_fini();
	m0_mdservice_fsync_fop_fini();
	m0_bitmap_fini(&m0t1fs_client_ep_tmid);
	m0_mutex_fini(&m0t1fs_mutex);
	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM
