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
 * Original author: Dima Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 7-Aug-2013
 */

#include <linux/kernel.h>    /* pr_info */
#include <linux/debugfs.h>   /* debugfs_create_dir */
#include <linux/module.h>    /* THIS_MODULE */
#include <linux/version.h>

#include "lib/misc.h"        /* bool */
#include "mero/linux_kernel/module.h"
#include "utils/linux_kernel/m0ctl_internal.h"
#include "utils/linux_kernel/trace_debugfs.h"


/**
 * @addtogroup m0ctl
 *
 * @{
 */

static struct dentry  *core_file;
static const char      core_name[] = "core";
static bool            core_file_is_opened = false;


static int core_open(struct inode *i, struct file *f)
{
	if (core_file_is_opened)
		return -EBUSY;

	core_file_is_opened = true;

	return 0;
}

static int core_release(struct inode *i, struct file *f)
{
	core_file_is_opened = false;
	return 0;
}

static ssize_t core_read(struct file *file, char __user *ubuf,
			 size_t ubuf_size, loff_t *ppos)
{
	const struct module *m = m0_mero_ko_get_module();

	return simple_read_from_buffer(ubuf, ubuf_size, ppos,
				       M0_MERO_KO_BASE(m),
				       M0_MERO_KO_SIZE(m));
}

static const struct file_operations core_fops = {
	.owner    = THIS_MODULE,
	.open     = core_open,
	.release  = core_release,
	.read     = core_read,
};

/******************************* init/fini ************************************/

int core_dfs_init(void)
{
	core_file = debugfs_create_file(core_name, S_IRUSR, dfs_root_dir, NULL,
					&core_fops);
	if (core_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
		       " '%s/%s'\n", dfs_root_name, core_name);
		return -EPERM;
	}

	return 0;
}

void core_dfs_cleanup(void)
{
	debugfs_remove(core_file);
}

/** @} end of m0ctl group */

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
