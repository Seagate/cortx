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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 02/29/2012
 */

#include <linux/kernel.h>    /* pr_info */

#ifdef ENABLE_FAULT_INJECTION

#include <linux/debugfs.h>   /* debugfs_create_dir */
#include <linux/kernel.h>    /* kstrtoul */
#include <linux/module.h>    /* THIS_MODULE */
#include <linux/seq_file.h>  /* seq_read */
#include <linux/uaccess.h>   /* strncpy_from_user */
#include <linux/string.h>    /* strncmp */
#include <linux/ctype.h>     /* isprint */
#include <linux/slab.h>      /* kfree */

#include "lib/thread.h"      /* M0_THREAD_INIT */
#include "lib/mutex.h"       /* m0_mutex */
#include "lib/time.h"        /* m0_time_now */
#include "lib/misc.h"        /* M0_SET_ARR0 */
#include "lib/string.h"      /* m0_strdup */
#include "lib/finject.h"
#include "lib/finject_internal.h"
#include "utils/linux_kernel/m0ctl_internal.h"
#include "utils/linux_kernel/finject_debugfs.h"


/**
 * @addtogroup m0ctl
 *
 * @{
 *
 * Fault injection control interface.
 *
 * @li mero/finject_stat   Provides information about all registered fault
 *                            points.
 *
 * @li mero/finject_ctl    Allows to change state of existing fault points
 *                            (enable/disable).
 *
 * finject_ctl accepts commands in the following format:
 *
 * @verbatim
 *
 *     COMMAND = ACTION function_name fp_tag [ ACTION_ARGUMENTS ]
 *
 *     ACTION = enable | disable
 *
 *     ACTION_ARGUMENTS = FP_TYPE [ FP_DATA ]
 *
 *     FP_TYPE = always | oneshot | random | off_n_on_m
 *
 *     FP_DATA = integer { integer }
 *
 * @endverbatim
 *
 * Here some examples:
 *
 * @verbatim
 *
 *     enable m0_alloc fake_failure oneshot
 *     enable m0_alloc fake_failure random 30
 *     enable m0_rpc_conn_start fake_success always
 *     enable m0_net_buffer_del need_fail off_n_on_m 2 5
 *
 *     disable m0_alloc fake_failure
 *     disable m0_net_buffer_del need_fail
 *
 * @endverbatim
 *
 * The easiest way to send a command is to use `echo`:
 *
 * @verbatim
 *
 *     $ echo 'enable m0_init need_fail always' > /sys/kernel/debug/mero/finject_ctl
 *
 * @endverbatim
 */

static struct dentry  *fi_dir;
static const char      fi_dir_name[] = "finject";
static bool            fi_ctl_is_opened = false;


static void *fi_stat_start(struct seq_file *seq, loff_t *pos)
{
	if (*pos == 0)
		return SEQ_START_TOKEN;
	else if (*pos >= m0_fi_states_get_free_idx())
		/* indicate beyond end of file position */
		return NULL;


	return (void*)(m0_fi_states_get() + *pos);
}

static void *fi_stat_next(struct seq_file *seq, void *v, loff_t *pos)
{
	loff_t cur_pos = (*pos)++;

	if (cur_pos >= m0_fi_states_get_free_idx())
		/* indicate end of sequence */
		return NULL;

	return (void*)(m0_fi_states_get() + cur_pos);
}

static void fi_stat_stop(struct seq_file *seq, void *v)
{
}

static int fi_stat_show(struct seq_file *seq, void *v)
{
	const struct m0_fi_fpoint_state *state = v;
	struct m0_fi_fpoint_state_info   si;

	/* print header */
	if (SEQ_START_TOKEN == v) {
		seq_puts(seq, m0_fi_states_headline[0]);
		seq_puts(seq, m0_fi_states_headline[1]);
		return 0;
	}

	/* skip disabled states */
	/* TODO: add an option to control this in runtime through debugfs */
	/*if (!fi_state_enabled(state))
		return SEQ_SKIP;*/

	m0_fi_states_get_state_info(state, &si);

	seq_printf(seq, m0_fi_states_print_format,
			si.si_idx, si.si_enb, si.si_total_hit_cnt,
			si.si_total_trigger_cnt, si.si_hit_cnt,
			si.si_trigger_cnt, si.si_type, si.si_data,
			si.si_module, si.si_file, si.si_line_num,
			si.si_func, si.si_tag);

	return 0;
}

static const struct seq_operations fi_stat_sops = {
	.start = fi_stat_start,
	.stop  = fi_stat_stop,
	.next  = fi_stat_next,
	.show  = fi_stat_show,
};

static int fi_stat_open(struct inode *i, struct file *f)
{
	int rc = 0;

	rc = seq_open(f, &fi_stat_sops);

	if (rc == 0) {
		struct seq_file *sf = f->private_data;
		sf->private = i->i_private;
	}

	return rc;
}

static const struct file_operations fi_stat_fops = {
	.owner		= THIS_MODULE,
	.open		= fi_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int fi_ctl_open(struct inode *i, struct file *f)
{
	if (fi_ctl_is_opened)
		return -EBUSY;

	fi_ctl_is_opened = true;

	return 0;
}

static int fi_ctl_release(struct inode *i, struct file *f)
{
	fi_ctl_is_opened = false;
	return 0;
}

static int fi_ctl_process_cmd(int argc, char *argv[])
{
	/* there should be at least "action", "func" and "tag" */
	if (argc < 3) {
		pr_err(KBUILD_MODNAME ": finject_ctl too few arguments\n");
		return -EINVAL;
	}

	if (strcmp(argv[0], "enable") == 0) {
		int rc;
		char *func;
		char *tag;

		/* "enable" also requires an "fp_type" */
		if (argc < 4) {
			pr_err(KBUILD_MODNAME ": finject_ctl too few arguments"
			       " for command '%s'\n", argv[0]);
			return -EINVAL;
		}

		func = m0_strdup(argv[1]);
		if (func == NULL)
			return -ENOMEM;

		rc = m0_fi_add_dyn_id(func);
		if (rc != 0) {
			kfree(func);
			return rc;
		}

		tag = m0_strdup(argv[2]);
		if (tag == NULL)
			return -ENOMEM;

		rc = m0_fi_add_dyn_id(tag);
		if (rc != 0) {
			kfree(tag);
			return rc;
		}

		if (strcmp(argv[3], "always") == 0) {
			if (argc > 4) {
				pr_err(KBUILD_MODNAME ": finject_ctl too many"
				       " arguments for command '%s'\n", argv[0]);
				return -EINVAL;
			}
			m0_fi_enable(func, tag);
		} else if (strcmp(argv[3], "oneshot") == 0) {
			if (argc > 4) {
				pr_err(KBUILD_MODNAME ": finject_ctl too many"
				       " arguments for command '%s'\n", argv[0]);
				return -EINVAL;
			}
			m0_fi_enable_once(func, tag);
		} else if (strcmp(argv[3], "random") == 0) {
			unsigned long p;
			if (argc != 5) {
				pr_err(KBUILD_MODNAME ": finject_ctl incorrect"
				       " number of arguments for FP type '%s'\n",
				       argv[3]);
				return -EINVAL;
			}
			rc = kstrtoul(argv[4], 0, &p);
			if (rc < 0)
				return rc;
			m0_fi_enable_random(func, tag, p);
		} else if (strcmp(argv[3], "off_n_on_m") == 0) {
			unsigned long n;
			unsigned long m;
			if (argc != 6) {
				pr_err(KBUILD_MODNAME ": finject_ctl incorrect"
				       " number of arguments for FP type '%s'\n",
				       argv[3]);
				return -EINVAL;
			}
			rc = kstrtoul(argv[4], 0, &n);
			if (rc < 0)
				return rc;
			rc = kstrtoul(argv[5], 0, &m);
			if (rc < 0)
				return rc;
			m0_fi_enable_off_n_on_m(func, tag, n, m);
		} else {
			pr_err(KBUILD_MODNAME ": finject_ctl: invalid or not"
			       " allowed FP type '%s'\n", argv[3]);
			return -EINVAL;
		}
	} else if (strcmp(argv[0], "disable") == 0) {
		m0_fi_disable(argv[1], argv[2]);
	} else {
		pr_err(KBUILD_MODNAME ": finject_ctl: invalid action '%s'\n",
					argv[0]);
		return -EINVAL;
	}

	return 0;
}

static ssize_t fi_ctl_write(struct file *file, const char __user *user_buf,
			    size_t size, loff_t *ppos)
{
	int       rc;
	int       i;
	ssize_t   ret_size = size;
	char      buf[256];
	int       argc;
	char    **argv;

	M0_THREAD_ENTER;
	if (size > sizeof buf - 1)
		return -EINVAL;

	if (strncpy_from_user(buf, user_buf, size) < 0)
		return -EFAULT;
	buf[size] = 0;

	/*
	 * usually debugfs files are written with `echo` command which appends a
	 * newline character at the end of string, so we need to remove it if it
	 * present
	 */
	if (buf[size - 1] == '\n') {
		buf[size - 1] = 0;
		size--;
	}

	/*
	 * check that buffer contains only printable text data to prevent
	 * user-space from injecting some malicious binary code into kernel in
	 * place of FP identifiers
	 */
	for (i = 0; i < size; ++i)
		if (!isprint(buf[i]))
			return -EINVAL;

	pr_info(KBUILD_MODNAME ": finject_ctl command '%s'\n", buf);

	argv = argv_split(GFP_KERNEL, buf, &argc);
	if (argv == NULL)
		return -ENOMEM;

	rc = fi_ctl_process_cmd(argc, argv);
	argv_free(argv);

	if (rc < 0)
		return rc;

	/* ignore the rest of the buffer, only one command at a time */
	*ppos += ret_size;
	return ret_size;
}

static const struct file_operations fi_ctl_fops = {
	.owner		= THIS_MODULE,
	.open		= fi_ctl_open,
	.release	= fi_ctl_release,
	.write		= fi_ctl_write,
};

int fi_dfs_init(void)
{
	struct dentry     *stat_file;
	static const char  stat_name[] = "stat";
	struct dentry     *ctl_file;
	static const char  ctl_name[] = "ctl";
	int                rc = 0;

	fi_dir = debugfs_create_dir(fi_dir_name, dfs_root_dir);
	if (fi_dir == NULL) {
		pr_err(KBUILD_MODNAME ": can't create debugfs dir '%s/%s'\n",
		       dfs_root_name, fi_dir_name);
		rc = -EPERM;
		goto out;
	}

	stat_file = debugfs_create_file(stat_name, S_IRUSR, fi_dir, NULL,
					&fi_stat_fops);
	if (stat_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, fi_dir_name, stat_name);
		rc = -EPERM;
		goto err;
	}

	ctl_file = debugfs_create_file(ctl_name, S_IWUSR, fi_dir, NULL,
				       &fi_ctl_fops);
	if (ctl_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, fi_dir_name, ctl_name);
		rc = -EPERM;
		goto err;
	}
out:
	return rc;
err:
	debugfs_remove_recursive(fi_dir);
	fi_dir = NULL;
	return rc;
}

void fi_dfs_cleanup(void)
{
	debugfs_remove_recursive(fi_dir);
}

/** @} end of m0ctl group */

#else

int fi_dfs_init(void)
{
	pr_warning(KBUILD_MODNAME ": fault injection is not available, because it"
				" was disabled during build\n");
	return 0;
}

void fi_dfs_cleanup(void)
{
}

#endif /* ENABLE_FAULT_INJECTION */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
