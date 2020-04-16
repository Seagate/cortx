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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 21-Jun-2013
 */

#include <linux/kernel.h>    /* pr_info */
#include <linux/debugfs.h>   /* debugfs_create_dir */
#include <linux/module.h>    /* THIS_MODULE */
#include <linux/uaccess.h>   /* strncpy_from_user */
#include <linux/string.h>    /* strncmp */
#include <linux/ctype.h>     /* isprint */
#include <linux/delay.h>     /* msleep */
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>      /* poll_table */
#include <linux/version.h>

#include "lib/mutex.h"       /* m0_mutex */
#include "lib/time.h"        /* m0_time_now */
#include "lib/misc.h"        /* M0_SET_ARR0 */
#include "lib/trace.h"
#include "lib/trace_internal.h"
#include "lib/linux_kernel/trace.h"
#include "utils/linux_kernel/m0ctl_internal.h"
#include "utils/linux_kernel/trace_debugfs.h"


/**
 * @addtogroup m0ctl
 *
 * @{
 */

static struct dentry  *trc_dir;
static const char      trc_dir_name[] = "trace";


typedef int (*trc_write_actor_t)(const char *level_str);

static ssize_t trc_write_helper(struct file *file, const char __user *ubuf,
			        size_t ubuf_size, loff_t *ppos,
				trc_write_actor_t actor)
{
	int          rc;
	ssize_t      ret_size = ubuf_size;
	static char  buf[256];

	if (ubuf_size > sizeof buf - 1)
		return -EINVAL;

	if (strncpy_from_user(buf, ubuf, ubuf_size) < 0)
		return -EFAULT;
	buf[ubuf_size] = '\0';

	/*
	 * usually debugfs files are written with `echo` command which appends a
	 * newline character at the end of string, so we need to remove it if it
	 * present
	 */
	if (buf[ubuf_size - 1] == '\n') {
		buf[ubuf_size - 1] = 0;
		ubuf_size--;
	}

	pr_info(KBUILD_MODNAME ": %s (%s) command '%s'\n",
		file->f_path.dentry->d_name.name,
		file->f_path.dentry->d_iname,
		buf);

	rc = actor(buf);
	if (rc < 0)
		return rc;

	/* ignore the rest of the buffer */
	*ppos += ret_size;

	return ret_size;
}

/******************************* immediate_mask *******************************/

static bool trc_immediate_mask_is_opened = false;

static int trc_immediate_mask_open(struct inode *i, struct file *f)
{
	if (trc_immediate_mask_is_opened)
		return -EBUSY;

	trc_immediate_mask_is_opened = true;

	return 0;
}

static int trc_immediate_mask_release(struct inode *i, struct file *f)
{
	trc_immediate_mask_is_opened = false;
	return 0;
}

static size_t buf_add(char *buf, size_t buf_size, size_t buf_used,
		      const char *fmt, ...)
{
	size_t  n = 0;
	va_list args;

	va_start(args, fmt);
	n += vsnprintf(buf + buf_used,
		       buf_used >= buf_size ? 0 : buf_size - buf_used,
		       fmt, args);
	va_end(args);

	return n;
}

static ssize_t trc_immediate_mask_read(struct file *file, char __user *ubuf,
			      size_t ubuf_size, loff_t *ppos)
{
	int          i;
	ssize_t      ret_size = 0;
	uint64_t     subsys;
	const char  *subsys_name;

	static char  buf[4096];
	size_t       buf_used = 0;

	/* indicate EOF if we are not at the beginning of file */
	if (*ppos != 0)
		return 0;

	buf[0] = '\0';

	for (i = 0; i < sizeof m0_trace_immediate_mask * CHAR_BIT; ++i) {
		subsys = m0_trace_immediate_mask & (1UL << i);
		if (subsys != 0) {
			subsys_name = m0_trace_subsys_name(subsys);
			if (subsys_name == NULL)
				continue;
			buf_used += buf_add(buf, sizeof buf, buf_used,
					    "%s ", subsys_name);
			if (buf_used >= sizeof buf) {
				buf_used = sizeof buf - 2;
				break;
			}
		}
	}

	buf[buf_used++] = '\n';
	buf[buf_used++] = '\0';
	ret_size = min(ubuf_size, buf_used);

	return simple_read_from_buffer(ubuf, ubuf_size, ppos, buf, ret_size);
}

static ssize_t trc_immediate_mask_write(struct file *file,
					const char __user *ubuf,
					size_t ubuf_size, loff_t *ppos)
{
	return trc_write_helper(file, ubuf, ubuf_size, ppos,
				m0_trace_set_immediate_mask);
}

static const struct file_operations trc_immediate_mask_fops = {
	.owner    = THIS_MODULE,
	.open     = trc_immediate_mask_open,
	.release  = trc_immediate_mask_release,
	.read     = trc_immediate_mask_read,
	.write    = trc_immediate_mask_write,
};

/******************************* level ****************************************/

static bool trc_level_is_opened = false;

static int trc_level_open(struct inode *i, struct file *f)
{
	if (trc_level_is_opened)
		return -EBUSY;

	trc_level_is_opened = true;

	return 0;
}

static int trc_level_release(struct inode *i, struct file *f)
{
	trc_level_is_opened = false;
	return 0;
}

static ssize_t trc_level_read(struct file *file, char __user *ubuf,
			      size_t ubuf_size, loff_t *ppos)
{
	int          i;
	ssize_t      ret_size = 0;
	uint32_t     level;
	const char  *level_name;

	static char  buf[256];
	size_t       buf_used = 0;

	/* indicate EOF if we are not at the beginning of file */
	if (*ppos != 0)
		return 0;

	buf[0] = '\0';

	for (i = 0; i < sizeof m0_trace_level * CHAR_BIT; ++i) {
		level = m0_trace_level & (1 << i);
		if (level != M0_NONE) {
			level_name = m0_trace_level_name(level);
			buf_used += buf_add(buf, sizeof buf, buf_used,
					    "%s ", level_name);
			if (buf_used >= sizeof buf) {
				buf_used = sizeof buf - 2;
				break;
			}
		}
	}

	buf[buf_used++] = '\n';
	buf[buf_used++] = '\0';
	ret_size = min(ubuf_size, buf_used);

	return simple_read_from_buffer(ubuf, ubuf_size, ppos, buf, ret_size);
}

static ssize_t trc_level_write(struct file *file, const char __user *ubuf,
			       size_t ubuf_size, loff_t *ppos)
{
	return trc_write_helper(file, ubuf, ubuf_size, ppos, m0_trace_set_level);
}

static const struct file_operations trc_level_fops = {
	.owner    = THIS_MODULE,
	.open     = trc_level_open,
	.release  = trc_level_release,
	.read     = trc_level_read,
	.write    = trc_level_write,
};

/******************************* print_context ********************************/

static bool trc_print_context_is_opened = false;

static int trc_print_context_open(struct inode *i, struct file *f)
{
	if (trc_print_context_is_opened)
		return -EBUSY;

	trc_print_context_is_opened = true;

	return 0;
}

static int trc_print_context_release(struct inode *i, struct file *f)
{
	trc_print_context_is_opened = false;
	return 0;
}

static ssize_t trc_print_context_read(struct file *file, char __user *ubuf,
			      size_t ubuf_size, loff_t *ppos)
{
	static char buf[16];
	size_t      ret_size;

	if (m0_trace_print_context == M0_TRACE_PCTX_FUNC)
		strncpy(buf, "func\n", sizeof buf);
	else if (m0_trace_print_context == M0_TRACE_PCTX_SHORT)
		strncpy(buf, "short\n", sizeof buf);
	else if (m0_trace_print_context == M0_TRACE_PCTX_FULL)
		strncpy(buf, "full\n", sizeof buf);
	else if (m0_trace_print_context == M0_TRACE_PCTX_NONE)
		strncpy(buf, "none\n", sizeof buf);
	else
		strncpy(buf, "INVALID\n", sizeof buf);

	ret_size = min(ubuf_size, strlen(buf) + 1);

	return simple_read_from_buffer(ubuf, ubuf_size, ppos, buf, ret_size);
}

static ssize_t trc_print_context_write(struct file *file, const char __user *ubuf,
			       size_t ubuf_size, loff_t *ppos)
{
	return trc_write_helper(file, ubuf, ubuf_size, ppos,
				m0_trace_set_print_context);
}

static const struct file_operations trc_print_context_fops = {
	.owner    = THIS_MODULE,
	.open     = trc_print_context_open,
	.release  = trc_print_context_release,
	.read     = trc_print_context_read,
	.write    = trc_print_context_write,
};

/******************************* stat ****************************************/

static bool trc_stat_is_opened = false;

static int trc_stat_open(struct inode *i, struct file *f)
{
	if (trc_stat_is_opened)
		return -EBUSY;

	trc_stat_is_opened = true;

	return 0;
}

static int trc_stat_release(struct inode *i, struct file *f)
{
	trc_stat_is_opened = false;
	return 0;
}

static const char *bytes_to_human_str(uint64_t bytes)
{
	static char buf[256];
	uint64_t    integer;
	uint32_t    reminder;
	const char *units;

	buf[0] = '\0';

	if (bytes / 1024 / 1024 / 1024) { /* > 1 gigabyte */
		integer = bytes / 1024 / 1024 / 1024;
		reminder = bytes % (1024 * 1024 * 1024) / 10000000;
		units = "Gi";
	} else if (bytes / 1024 / 1024) { /* > 1 megabyte */
		integer = bytes / 1024 / 1024;
		reminder = bytes % (1024 * 1024) / 10000;
		units = "Mi";
	} else if (bytes / 1024) {  /* > 1 kilobyte */
		integer = bytes / 1024;
		reminder = bytes % 1024 / 10;
		units = "Ki";
	} else {
		integer = bytes;
		reminder = 0;
		units = "B";
		/* just return an empty string for now, to avoid duplication */
		return buf;
	}

	snprintf(buf, sizeof buf, "%llu.%02u%s", integer, reminder, units);
	buf[sizeof buf - 1] = '\0';

	return buf;
}

static ssize_t trc_stat_read(struct file *file, char __user *ubuf,
			      size_t ubuf_size, loff_t *ppos)
{
	static char  buf[4096];
	const size_t buf_size = sizeof buf - 1; /* reserve last byte for '\0' */
	size_t       buf_used = 0;
	ssize_t      ret_size = 0;
	const struct m0_trace_stats *stats = m0_trace_get_stats();
	uint64_t     logbuf_pos;
	uint32_t     logbuf_size;
	uint64_t     total_rec_num;
	uint32_t     rec_per_sec;
	uint32_t     bytes_per_sec;
	uint32_t     avg_rec_per_sec;
	uint32_t     avg_bytes_per_sec;
	uint32_t     max_rec_per_sec;
	uint32_t     max_bytes_per_sec;
	uint32_t     avg_rec_size;
	uint32_t     max_rec_size;

	/* indicate EOF if we are not at the beginning of file */
	if (*ppos != 0)
		return 0;

	buf[0] = '\0';

	m0_trace_stats_update(0);
	logbuf_pos        = m0_trace_logbuf_pos_get();
	logbuf_size       = m0_trace_logbuf_size_get();
	total_rec_num     = m0_atomic64_get(&stats->trs_rec_total);
	rec_per_sec       = stats->trs_rec_per_sec;
	bytes_per_sec     = stats->trs_bytes_per_sec;
	avg_rec_per_sec   = stats->trs_avg_rec_per_sec;
	avg_bytes_per_sec = stats->trs_avg_bytes_per_sec;
	max_rec_per_sec   = stats->trs_max_rec_per_sec;
	max_bytes_per_sec = stats->trs_max_bytes_per_sec;
	avg_rec_size      = stats->trs_avg_rec_size;
	max_rec_size      = stats->trs_max_rec_size;

	buf_used += buf_add(buf, buf_size, buf_used,
			    "buffer address:       0x%p\n",
			    m0_trace_logbuf_get());

	buf_used += buf_add(buf, buf_size, buf_used,
			    "buffer size:          %-12u  %s\n",
			    logbuf_size, bytes_to_human_str(logbuf_size));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "buffer abs pos:       %-12llu  %s\n",
			    logbuf_pos, bytes_to_human_str(logbuf_pos));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "total rec num:        %-12llu  %s\n",
			    total_rec_num, bytes_to_human_str(total_rec_num));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "records per sec:      %-12u  %s\n",
			    rec_per_sec, bytes_to_human_str(rec_per_sec));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "bytes per sec:        %-12u  %s\n",
			    bytes_per_sec, bytes_to_human_str(bytes_per_sec));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "avg records per sec:  %-12u  %s\n",
			    avg_rec_per_sec,
			     bytes_to_human_str(avg_rec_per_sec));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "avg bytes per sec:    %-12u  %s\n",
			    avg_bytes_per_sec,
			     bytes_to_human_str(avg_bytes_per_sec));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "max records per sec:  %-12u  %s\n",
			    max_rec_per_sec,
			     bytes_to_human_str(max_rec_per_sec));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "max bytes per sec:    %-12u  %s\n",
			    max_bytes_per_sec,
			    bytes_to_human_str(max_bytes_per_sec));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "avg record size:      %-12u  %s\n",
			    avg_rec_size, bytes_to_human_str(avg_rec_size));

	buf_used += buf_add(buf, buf_size, buf_used,
			    "max record size:      %-12u  %s\n",
			    max_rec_size, bytes_to_human_str(max_rec_size));

	if (buf_used >= sizeof buf)
		buf_used = buf_size - 1;
	buf[buf_used++] = '\0';
	ret_size = min(ubuf_size, buf_used);

	return simple_read_from_buffer(ubuf, ubuf_size, ppos, buf, ret_size);
}

static const struct file_operations trc_stat_fops = {
	.owner    = THIS_MODULE,
	.open     = trc_stat_open,
	.release  = trc_stat_release,
	.read     = trc_stat_read,
};

/******************************* records **************************************/

static bool trc_records_is_opened = false;
static const struct m0_trace_rec_header *trc_records_last_trh;

static int trc_records_open(struct inode *i, struct file *f)
{
	if (trc_records_is_opened)
		return -EBUSY;

	trc_records_is_opened = true;
	trc_records_last_trh = NULL;

	return 0;
}

static int trc_records_release(struct inode *i, struct file *f)
{
	trc_records_is_opened = false;
	return 0;
}

static const struct m0_trace_rec_header*
find_next_recornd(const struct m0_trace_rec_header *curtrh)
{
	const char *begptr = (char*)m0_trace_logbuf_get();
	const char *endptr = begptr + m0_trace_logbuf_size_get();
	const char *curptr = begptr + m0_trace_logbuf_pos_get() %
				      m0_trace_logbuf_size_get();

	const char *p = (char*)curtrh + curtrh->trh_record_size;

	if (p > curptr) {
		for (; p < endptr; p += M0_TRACE_REC_ALIGN)
			if (*((uint64_t*)p) == M0_TRACE_MAGIC)
				return (const struct m0_trace_rec_header*)p;
		p = begptr;
	}

	for (; p < curptr; p += M0_TRACE_REC_ALIGN)
		if (*((uint64_t*)p) == M0_TRACE_MAGIC)
			return (const struct m0_trace_rec_header*)p;

	/* p == curptr, it means there is no new trace records in log buffer */
	return NULL;
}

static ssize_t trc_records_read(struct file *file, char __user *ubuf,
			        size_t ubuf_size, loff_t *ppos)
{
	long         rc;
	static char  buf[16 * 1024]; /* 16 KB */
	void        *tr_body;
	loff_t       fake_pos = 0;

	const struct m0_trace_rec_header        *trh;

	buf[0] = '\0';

	while (true) {
		if (trc_records_last_trh == NULL)
			trh = m0_trace_last_record_get();
		else
			trh = find_next_recornd(trc_records_last_trh);

		if (trh != NULL && trh != trc_records_last_trh) {
			trc_records_last_trh = trh;
			tr_body = (char*)trh + m0_align(sizeof *trh,
							M0_TRACE_REC_ALIGN);
			break;
		}

		rc = msleep_interruptible(100);
		if (rc != 0)
			/* got a signal, return EOF */
			return 0;
	}

	rc = m0_trace_record_print_yaml(buf, sizeof buf, trh, tr_body, true);
	if (rc != 0) {
		pr_warn(KBUILD_MODNAME ": internal buffer is to small to hold"
			" trace record\n");
		/* return empty string: there is only '\0' in the buffer */
		return simple_read_from_buffer(ubuf, ubuf_size, &fake_pos,
					       buf, 1);
	}

	return simple_read_from_buffer(ubuf, ubuf_size, &fake_pos,
				       buf, strlen(buf));
}

static const struct file_operations trc_records_fops = {
	.owner    = THIS_MODULE,
	.open     = trc_records_open,
	.release  = trc_records_release,
	.read     = trc_records_read,
};

/******************************* buffer **************************************/

static int trc_buffer_open(struct inode *i, struct file *f)
{
	return 0;
}

static int trc_buffer_release(struct inode *i, struct file *f)
{
	return 0;
}

static ssize_t trc_buffer_read(struct file *file, char __user *ubuf,
			        size_t ubuf_size, loff_t *ppos)
{
	const void *logbuf_header = m0_trace_logbuf_header_get();
	uint32_t    logbuf_size = M0_TRACE_BUF_HEADER_SIZE +
				  m0_trace_logbuf_size_get();

	return simple_read_from_buffer(ubuf, ubuf_size, ppos, logbuf_header,
				       logbuf_size);
}

static void trc_buffer_mmap_close(struct vm_area_struct *vma)
{
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
static int trc_buffer_mmap_fault(struct vm_fault *vmf)
#else
static int trc_buffer_mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
#endif
{
	const void  *logbuf_header = m0_trace_logbuf_header_get();
	pgoff_t      pgoff = vmf->pgoff;
	struct page *page;

	page = vmalloc_to_page(logbuf_header + (pgoff << PAGE_SHIFT));
	if (!page)
		return VM_FAULT_SIGBUS;

	get_page(page);
	vmf->page = page;

	return 0;
}

static const struct vm_operations_struct trc_buffer_mmap_ops = {
	.fault = trc_buffer_mmap_fault,
	.close = trc_buffer_mmap_close,
};

static int trc_buffer_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long  length = vma->vm_end - vma->vm_start;
	uint32_t       logbuf_size = M0_TRACE_BUF_HEADER_SIZE +
				     m0_trace_logbuf_size_get();

	if (length + (vma->vm_pgoff << PAGE_SHIFT) > logbuf_size)
		return -EINVAL;

	vma->vm_ops = &trc_buffer_mmap_ops;
	vma->vm_flags |= VM_DONTEXPAND;

	return 0;
}

static unsigned int trc_buffer_poll(struct file *filp, poll_table *pt)
{
	long            rc;
	const char     *curpos;
	const uint32_t  idle_timeo = 100; /* in msec */

	static uint32_t    timeo;
	static const char *oldpos;

	curpos = (char*)m0_trace_logbuf_get() +
			m0_trace_logbuf_pos_get() % m0_trace_logbuf_size_get();

	/* init static vars */
	if (oldpos == NULL) {
		oldpos = curpos;
		timeo = idle_timeo;
	}

	while (curpos == oldpos) {
		rc = msleep_interruptible(timeo);
		if (rc != 0)
			/* got a signal, return "no data" */
			return 0;

		curpos = (char*)m0_trace_logbuf_get() +
			 m0_trace_logbuf_pos_get() % m0_trace_logbuf_size_get();

		if (++timeo > idle_timeo)
			timeo = idle_timeo;
	}
	oldpos = curpos;
	timeo /= 2;

	return (unsigned int)(POLLIN | POLLRDNORM);
}

static const struct file_operations trc_buffer_fops = {
	.owner    = THIS_MODULE,
	.open     = trc_buffer_open,
	.release  = trc_buffer_release,
	.read     = trc_buffer_read,
	.mmap     = trc_buffer_mmap,
	.poll     = trc_buffer_poll,
};

/******************************* init/fini ************************************/

int trc_dfs_init(void)
{
	struct dentry     *trc_level_file;
	static const char  trc_level_name[] = "level";
	struct dentry     *trc_immediate_mask_file;
	static const char  trc_immediate_mask_name[] = "immediate_mask";
	struct dentry     *trc_print_context_file;
	static const char  trc_print_context_name[] = "print_context";
	struct dentry     *trc_stat_file;
	static const char  trc_stat_name[] = "stat";
	struct dentry     *trc_records_file;
	static const char  trc_records_name[] = "records";
	struct dentry     *trc_buffer_file;
	static const char  trc_buffer_name[] = "buffer";
	int                rc = 0;

	trc_dir = debugfs_create_dir(trc_dir_name, dfs_root_dir);
	if (trc_dir == NULL) {
		pr_err(KBUILD_MODNAME ": can't create debugfs dir '%s/%s'\n",
		       dfs_root_name, trc_dir_name);
		rc = -EPERM;
		goto out;
	}

	trc_level_file = debugfs_create_file(trc_level_name, S_IRUSR | S_IWUSR,
					     trc_dir, NULL, &trc_level_fops);
	if (trc_level_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, trc_dir_name,
			trc_level_name);
		rc = -EPERM;
		goto err;
	}

	trc_immediate_mask_file = debugfs_create_file(trc_immediate_mask_name,
					S_IRUSR | S_IWUSR, trc_dir, NULL,
					&trc_immediate_mask_fops);
	if (trc_immediate_mask_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, trc_dir_name,
			trc_immediate_mask_name);
		rc = -EPERM;
		goto err;
	}

	trc_print_context_file = debugfs_create_file(trc_print_context_name,
					S_IRUSR | S_IWUSR, trc_dir, NULL,
					&trc_print_context_fops);
	if (trc_print_context_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, trc_dir_name,
			trc_print_context_name);
		rc = -EPERM;
		goto err;
	}

	trc_stat_file = debugfs_create_file(trc_stat_name, S_IRUSR, trc_dir,
					NULL, &trc_stat_fops);
	if (trc_stat_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, trc_dir_name,
			trc_stat_name);
		rc = -EPERM;
		goto err;
	}

	trc_records_file = debugfs_create_file(trc_records_name, S_IRUSR,
					       trc_dir, NULL,
					       &trc_records_fops);
	if (trc_records_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, trc_dir_name,
			trc_records_name);
		rc = -EPERM;
		goto err;
	}

	trc_buffer_file = debugfs_create_file(trc_buffer_name, S_IRUSR,
					      trc_dir, NULL,
					      &trc_buffer_fops);
	if (trc_buffer_file == NULL) {
		pr_err(KBUILD_MODNAME ": failed to create debugfs file"
			" '%s/%s/%s'\n", dfs_root_name, trc_dir_name,
			trc_buffer_name);
		rc = -EPERM;
		goto err;
	}
out:
	return rc;
err:
	debugfs_remove_recursive(trc_dir);
	trc_dir = 0;
	return rc;
}

void trc_dfs_cleanup(void)
{
	debugfs_remove_recursive(trc_dir);
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
