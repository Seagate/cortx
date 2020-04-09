/*
 *  linux/drivers/block/loop.c
 *
 *  Written by Theodore Ts'o, 3/29/93
 *
 * Copyright 1993 by Theodore Ts'o.  Redistribution of this file is
 * permitted under the GNU General Public License.
 *
 * DES encryption plus some minor changes by Werner Almesberger, 30-MAY-1993
 * more DES encryption plus IDEA encryption by Nicholas J. Leon, June 20, 1996
 *
 * Modularized and updated for 1.1.16 kernel - Mitch Dsouza 28th May 1994
 * Adapted for 1.3.59 kernel - Andries Brouwer, 1 Feb 1996
 *
 * Fixed do_loop_request() re-entrancy - Vincent.Renardias@waw.com Mar 20, 1997
 *
 * Added devfs support - Richard Gooch <rgooch@atnf.csiro.au> 16-Jan-1998
 *
 * Handle sparse backing files correctly - Kenn Humborg, Jun 28, 1998
 *
 * Loadable modules and other fixes by AK, 1998
 *
 * Make real block number available to downstream transfer functions, enables
 * CBC (and relatives) mode encryption requiring unique IVs per data block.
 * Reed H. Petty, rhp@draper.net
 *
 * Maximum number of loop devices now dynamic via max_loop module parameter.
 * Russell Kroll <rkroll@exploits.org> 19990701
 *
 * Maximum number of loop devices when compiled-in now selectable by passing
 * max_loop=<1-255> to the kernel on boot.
 * Erik I. Bolsø, <eriki@himolde.no>, Oct 31, 1999
 *
 * Completely rewrite request handling to be make_request_fn style and
 * non blocking, pushing work to a helper thread. Lots of fixes from
 * Al Viro too.
 * Jens Axboe <axboe@suse.de>, Nov 2000
 *
 * Support up to 256 loop devices
 * Heinz Mauelshagen <mge@sistina.com>, Feb 2002
 *
 * Support for falling back on the write file operation when the address space
 * operations write_begin is not available on the backing filesystem.
 * Anton Altaparmakov, 16 Feb 2005
 *
 * Still To Fix:
 * - Advisory locking is ignored here.
 * - Should use an own CAP_* category instead of CAP_SYS_ADMIN
 *
 */

/**
 * @page m0loop-dld The new m0loop device driver DLD
 *
 * - @ref m0loop-dld-ovw
 * - @ref m0loop-dld-req
 * - @ref m0loop-dld-highlights
 * - @ref m0loop-dld-dep
 * - @ref m0loop-dld-fspec
 * - @ref m0loop-dld-lspec
 *    - @ref m0loop-dld-lspec-thread
 * - @ref m0loop-dld-conformance
 * - @ref m0loop-dld-ut
 * - @ref m0loop-dld-st
 * - @ref m0loop-dld-O
 * - @ref m0loop-dld-ref
 * - @ref m0loop-dld-plan
 *
 * <hr>
 * @section m0loop-dld-ovw Overview
 *
 * M0loop is <a href="http://en.wikipedia.org/wiki/Loop_device">loop
 * block device</a> Linux driver for Mero m0t1fs.
 *
 * M0loop driver was based on <a
 * href="http://lxr.linux.no/linux+v2.6.32/drivers/block/loop.c">
 * standard Linux loop device driver</a>, so it is also GPL-licensed.
 *
 * This document describes only the changes we will make to the standard
 * loop device driver for m0loop.
 *
 * The problem with standard loop driver is that it copies data between
 * pages (see transfer_none()). For writing it tries to use address
 * space operations write_begin and write_end (which must be implemented
 * by the file system) to directly manipulate with the pages in system
 * cache and thus avoid one excess data copy. With m0t1fs this won't
 * work, because it does not use system cache. Thus, it is needed to
 * invent some other mechanism to avoid data copying and remove the use
 * of transfer_none() from the data path altogether.
 *
 * <hr>
 * @section m0loop-dld-req Requirements
 *
 * - @b r.m0loop.map
 *   M0loop should represent a m0t1fs file as the block device.
 * - @b r.m0loop.nocopy
 *   M0loop should avoid the copying of the data between pages.
 * - @b r.m0loop.bulk
 *   M0loop should efficiently use m0t1fs interface. In particular,
 *   it should call m0t1fs read/write functions with as much data as
 *   possible in a one call.
 * - @b r.m0loop.losetup
 *   M0loop should be manageable with the standard losetup utility.
 *
 * <hr>
 * @section m0loop-dld-highlights Design Highlights
 *
 * For all the bio segments, we directly call m0t1fs
 * aio_read/aio_write() functions with the iovecs array argument, where
 * each vector points directly to the pages data. Thus, we avoid data
 * copying.
 *
 * It appears, from empirical investigation, that Linux does not make
 * segments larger than 4K (one page) and does not pass more than one 4K
 * segment in the bio request structure; this results in separate bio
 * requests for each 4K buffer. Calling aio_read/aio_write() each time
 * just for each 4K buffer is not very effective.
 *
 * To solve this problem, we aggregate the segments (actually - the
 * pages) which belongs to one particular read/write operation from
 * all available bio requests in the list into iov array and call
 * aio_read/aio_write() with all of them.
 *
 * <hr>
 * @section m0loop-dld-dep Dependencies
 *
 * The ext4 file system which is likely to be used with m0loop
 * block device has a maximum block size limit of 4K. For optimal
 * performance we suppose such a 4K block size configuration of ext4
 * file system and inform the kernel that our sector size is also 4K
 * (with blk_queue_logical_block_size()). That will be the minimum
 * segment size kernel give us in a bio request. This means that m0t1fs
 * must be able to handle 4K size buffers, possibly with help of
 * read-modify-write (RMW) feature (in case stripe size is greater than
 * 4K).
 *
 * The size of one iovec from m0loop is limited to 4K (one page).
 * So in order to work effectively with m0loop driver m0t1fs should
 * effectively handle all the iov elements in the array passed into the
 * aio_read/aio_write() calls. In particular, it should accumulate the
 * pages across all iovecs (when needed) in the same read or write call
 * and do not limit its I/O size by the size of some one iovec.
 *
 * For example, if stripe unit size is 16K (four 4K pages), m0t1fs
 * should not handle each iovec from iov array separately, but it
 * should try to get all available iovecs from the same request to
 * form as close to 16K (stripe unit size) buffer as possible for its
 * I/O.
 *
 * <hr>
 * @section m0loop-dld-fspec Functional Specification
 *
 * M0loop driver represents a m0t1fs file as a /dev/m0loopN block
 * device in the system. The same way as for standard loop device,
 * losetup(8) utility is used to configure such an association between
 * a file and particular /dev/m0loopN block device instance. The
 * following losetup(8) options are supported for now:
 *
 *   - setup loop device:
 *     losetup /dev/m0loopN m0t1fs_file
 *   - delete loop device:
 *     losetup -d /dev/m0loopN
 *   - get info:
 *     losetup /dev/m0loopN
 *
 * <hr>
 * @section m0loop-dld-lspec Logical Specification
 *
 * In this section we are going to describe mainly how the segments
 * from bio requests are aggregated in iovecs. This is the core logic
 * customization to the standard loop driver we provide in m0loop.
 *
 * Internally, m0loop driver works with the same kernel interfaces as
 * standard loop driver. For example, from block layer it takes bio
 * request structures from which the buffers are referenced. Struct file
 * is used to call the file system operations on a mapped file
 * (aio_read/aio_write). For block device request queue configuration,
 * the same kernel blk_queue_*() API is used. The same ioctls are used
 * by losetup(8) utility (like LOOP_SET_FD) to configure the association
 * between a file and block device.
 *
 * For all the bio segments, we directly call m0t1fs
 * aio_read/aio_write() functions with the iovecs array argument, where
 * each vector points directly to the pages data from the segments.
 * Thus, we avoid data copying inside m0loop driver.
 *
 * The m0loop driver (as well as the standard one) handles bio requests
 * asynchronously, i.e. when kernel calls loop_make_request(), it
 * just add bio request into lo->lo_bio_list queue and wakes up the
 * loop_thread. When the latter starts running, it handles all the
 * bio requests available in the queue one by one. In the stable
 * running system it is more likely that there will be several bio
 * requests in the queue before loop_thread will start to handle
 * them. This allows us to analyze several bio requests available in
 * the queue and aggregate the relevant contiguous segments for one
 * specific read/write file operation into correspondent iovecs for
 * aio_read/aio_write() call. Here is the sequence diagram which shows
 * this process on several CPUs host:
 *
 * @msc
 * requestor,loop_thread,m0t1fs;
 *
 * |||;
 * requestor box requestor [label = "loop_add_bio()"];
 * requestor -> loop_thread [label = "wake_up(lo)"];
 * requestor box requestor [label = "loop_add_bio()"];
 * requestor -> loop_thread [label = "wake_up(lo)"];
 * ...;
 * --- [label = " loop thread scheduled, two threads may run in parallel "];
 * requestor box requestor [label = "loop_add_bio()"],
 * loop_thread box loop_thread [label = "accumulate_bios(): accumulate bio
 *                                       segments into iovecs array"];
 * requestor -> loop_thread [label = "wake_up(lo)"];
 * requestor box requestor [label = "loop_add_bio()"],
 * loop_thread => m0t1fs [label = "aio_read/aio_write()"];
 * requestor -> loop_thread [label = "wake_up(lo)"],
 * loop_thread box loop_thread [label = "accumulate_bios(): accumulate bio
 *                                       segments into iovecs array"];
 * loop_thread => m0t1fs [label = "aio_read/aio_write()"];
 * ...;
 * loop_thread => loop_thread [label = "wait_event(lo, !bio_list_empty())"];
 * @endmsc
 *
 * accumulate_bios() does the main job of contiguous bio segments
 * aggregation. It takes the bio requests from the queue head and fills
 * iovecs array from the bio segments until any of the following
 * conditions happen:
 *
 *   - the position in the file where the data should be read/written
 *     changed;
 *   - bio request changed from read to write (or vise versa);
 *   - the number of segments exceed the size of iovecs array.
 *
 * As soon as any on these conditions happen, we stop aggregating the
 * segments and return from accumulate_bios() routine with number of
 * accumulated segments and with the list of accumulated bio requests.
 * The list will be needed later when we will call bio_end() for each
 * bio with the status of I/O operation.
 *
 * Aggregated segments in iovecs array are passed to do_iov_filebacked()
 * function which is just convenient wrapper for aio_read/aio_write()
 * calls.
 *
 * @subsection m0loop-dld-lspec-thread Threading and Concurrency
 *
 * As it can be seen by now, there are two threads involved in m0loop.
 * The requestor thread adds bio requests to the lo_bio_list queue and
 * the loop_thread handles these requests.
 *
 * If there are no more bio requests to handle in the queue, loop_thread
 * just sleeps waiting for event from the requestor thread. The latter
 * wakes it up after adding a new bio request to the queue.
 *
 * lo_bio_list queue should be protected with the spin lock, because two
 * threads can manipulate it concurrently (see lo->lo_lock). The lo_lock
 * spin lock is provided by the standard loop driver code and we do not
 * change its usage.
 *
 * There is also lo_ctl_mutex in m0loop, which protects the management
 * interface to the driver. We also do not touch this code.
 *
 * @subsection m0loop-dld-lspec-losetup Losetup(8) Utility Support
 *
 * The support of losetup(8) utility is provided just by the code
 * inherited from standard loop device driver, which we do not change.
 *
 * <hr>
 * @section m0loop-dld-conformance Conformance
 *
 * - @b i.m0loop.map
 *   M0loop represents a m0t1fs file as the block device by inheriting
 *   the code from the standard loop device driver.
 * - @b i.m0loop.nocopy
 *   M0loop avoids copying of the data between pages by calling directly
 *   aio_read/aio_write() system call implemented by m0t1fs with the
 *   iovecs which points directly to the data pages.
 * - @b i.m0loop.bulk
 *   M0loop implements aggregation of segments from different bio requests
 *   in iovecs array.
 * - @b i.m0loop.losetup
 *   M0loop is manageable with the standard losetup utility mainly by
 *   inheriting the code from the standard loop device driver.
 *
 * <hr>
 * @section m0loop-dld-ut Unit Tests
 *
 * The following UTs could be implemented for accumulate_bios()
 * routine.
 *
 * Basic functionality tests:
 *
 *   - One bio request (in the queue) with one segment. One element in
 *     iovecs array is returned with correct file pos and I/O size.
 *   - One bio request with two segments. Two elements in iovecs array
 *     are returned with correct file pos and summary I/O size.
 *   - Two bio requests (for the same read/write operation), one segment
 *     each, for contiguous file region. Two elements in iovecs array
 *     are returned with correct file pos and summary I/O size.
 *
 * Exception cases tests:
 *
 *   - Two bio requests (one segment each) but for non-contiguous file
 *     regions. Two calls are expected with one element in iovecs array
 *     returned each time.
 *   - Two bio requests for contiguous file region, but for different
 *     operations: one for read, another for write. Two calls are
 *     expected with one element in iovecs array returned each time.
 *
 * Iovecs array boundary (IOV_ARR_SIZE) tests (contiguous file region):
 *
 *   - IOV_ARR_SIZE bio requests in the list (one segment each).
 *     IOV_ARR_SIZE elements in iovecs array are returned in one call.
 *   - (IOV_ARR_SIZE + 1) bio requests in the list. Two calls are
 *     expected: one with IOV_ARR_SIZE elements in iovecs array
 *     returned, another with one element returned.
 *   - (IOV_ARR_SIZE - 1) bio requests one segment each and one bio
 *     request with two segments. Two calls are expected: one with
 *     (IOV_ARR_SIZE - 1) elements in iovecs array returned, another
 *     with two elements returned.
 *
 * UT should fake upper (bio_request) interface by constructing
 * correspondent bio requests in the list by itself.
 *
 * <hr>
 * @section m0loop-dld-st System Tests
 *
 * The following system testing should pass for m0loop:
 *
 *   - create m0loopN block device on a m0t1fs file with losetup utility;
 *   - mkfs.ext4 with 4K block size (lower block sizes are not supported);
 *   - mount ext4 file system on m0loop device;
 *   - run ext4 file system benchmark (like iozone);
 *   - umount ext4 file system on m0loop device;
 *   - delete m0loopN block device from m0t1fs file with losetup utility.
 *
 * <hr>
 * @section m0loop-dld-O Analysis
 *
 * M0loop does not take any additional resources compared to standard
 * loop driver. Similar to loop, it does not make any memory allocations
 * during normal workflow. Except spin lock (lo_lock), which just
 * protects the bio requests queue between the requestor and loop threads,
 * no any other locks are taken.
 *
 * Upon each particular /dev/m0loopN block device instance bind to
 * the file with losetup(8) utility, iovecs array of IOV_ARR_SIZE
 * size is dynamically allocated. This is the only memory footprint
 * increase in respect to standard loop driver.
 *
 * There are no excessive CPU calculations. Similar to standard loop
 * driver, there is only one loop, where we traverse the bio requests
 * queue in the loop thread.
 *
 * Bio requests are gathered in the queue and eventually the loop
 * thread processes all of them - exactly the same way standard loop
 * driver does. No any additional delay for the bio resources is caused
 * here.
 *
 * It could be possible to add a timer into loop thread to possibly
 * wait for additional bio requests to form a complete stripe size I/O
 * buffers and bypass additional read-modify-write logic in m0t1fs to
 * optimize overall performance. But it is questionable for now, if
 * we gain something from this. First, timer resource should be taken
 * probably every time on each loop thread run. Second, it is not clear
 * whether m0loop is authorized to cause any additional delay in the
 * I/O path and for how long.
 *
 * <hr>
 * @section m0loop-dld-ref References
 *
 * - <a href="http://en.wikipedia.org/wiki/Loop_device">Loop block device</a>
 *   On Wikipedia
 * - <a href="http://lxr.linux.no/linux+v2.6.32/drivers/block/loop.c">
 *   Standard Linux loop device driver</a>, the base source code.
 * - <a href="http://www.makelinux.net/ldd3/chp-16-sect-3">Request
 *   Processing</a> section in LDD3 book to get into some Linux block
 *   device drivers internals.
 * - <a href="http://goo.gl/WzDPt">M0loop investigations log</a>
 *
 * <hr>
 * @section m0loop-dld-plan Implementation Plan
 *
 * The working prototype for m0loop is already implemented along with
 * correspondent changes in m0t1fs (refer to m0loop branch). This was done
 * as a part of experimentations and investigation (prepare) part of this
 * task.
 *
 * As for today, the work on read-modify-write in m0t1fs is ongoing,
 * which may implement the needed changes in m0t1fs (confirmed by Anand).
 *
 * After prototype code is inspected and becomes the real code, it must
 * be carefully tested. Before RMW is ready, the only testing option -
 * it is with 4K m0t1fs stripe size.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/init.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/loop.h>
#include <linux/compat.h>
#include <linux/suspend.h>
#include <linux/freezer.h>
#include <linux/writeback.h>
#include <linux/buffer_head.h>		/* for invalidate_bdev() */
#include <linux/completion.h>
#include <linux/highmem.h>
#include <linux/kthread.h>
#include <linux/splice.h>

#include <asm/uaccess.h>

#include "lib/misc.h"   /* M0_SET0 */
#include "lib/errno.h"
#include "m0t1fs/linux_kernel/m0t1fs.h"
#include "m0t1fs/linux_kernel/m0loop_internal.h"

static LIST_HEAD(loop_devices);
static DEFINE_MUTEX(loop_devices_mutex);

static int max_part;
static int part_shift;
static int M0LOOP_MAJOR;

static loff_t get_loop_size(struct loop_device *lo, struct file *file)
{
	loff_t size, offset, loopsize;

	/* Compute loopsize in bytes */
	size = i_size_read(file->f_mapping->host);
	offset = lo->lo_offset;
	loopsize = size - offset;
	if (lo->lo_sizelimit > 0 && lo->lo_sizelimit < loopsize)
		loopsize = lo->lo_sizelimit;

	/*
	 * Unfortunately, if we want to do I/O on the device,
	 * the number of 512-byte sectors has to fit into a sector_t.
	 */
	return loopsize >> 9;
}

static int
figure_loop_size(struct loop_device *lo)
{
	loff_t size = get_loop_size(lo, lo->lo_backing_file);
	sector_t x = (sector_t)size;

	if (unlikely((loff_t)x != size))
		return -EFBIG;

	set_capacity(lo->lo_disk, x);
	return 0;
}

/*
 * wait_on_retry_sync_kiocb:
 *   this routine was just copy/pasted from kernel's
 *   src/fs/read_write.c file. Looks like there is
 *   no other choice nowadays.
 */
static void wait_on_retry_sync_kiocb(struct kiocb *iocb)
{
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (!kiocbIsKicked(iocb))
		schedule();
	else
		kiocbClearKicked(iocb);
	__set_current_state(TASK_RUNNING);
}

/*
 * do_iov_filebacked: do vectored I/O over backed file
 *
 *   - op : operation (read or write);
 *   - n  : number of vectors in iovecs array;
 *   - pos: position in file to read from or write to;
 *   - size: number of bytes.
 *
 * Returns: 0 on success or -EIO (ready to be feed for bio_end()).
 */
static int do_iov_filebacked(struct loop_device *lo, unsigned long op, int n,
			     loff_t pos, unsigned size)
{
	struct file *file = lo->lo_backing_file;
	ssize_t ret;
	struct kiocb kiocb;
	ssize_t (*aio_rw) (struct kiocb *, const struct iovec *,
	                   unsigned long, loff_t);
	mm_segment_t old_fs = get_fs();

	init_sync_kiocb(&kiocb, file);
	kiocb.ki_nbytes = kiocb.ki_left = size;
	kiocb.ki_pos = pos;

	aio_rw = (op == WRITE) ? file->f_op->aio_write :
	                         file->f_op->aio_read;
	set_fs(get_ds());
	for (;;) {
		ret = aio_rw(&kiocb, lo->key_data, n, pos);
		if (ret != -EIOCBRETRY)
			break;
		wait_on_retry_sync_kiocb(&kiocb);
	}
	if (ret == -EIOCBQUEUED)
		ret = wait_on_sync_kiocb(&kiocb);
	set_fs(old_fs);

	if (likely(size == ret))
		return 0;

	return -EIO;
}

static int loop_make_request(struct request_queue *q, struct bio *old_bio)
{
	struct loop_device *lo = q->queuedata;
	int rw = bio_rw(old_bio);

	if (rw == READA)
		rw = READ;

	BUG_ON(!lo || (rw != READ && rw != WRITE));

	spin_lock_irq(&lo->lo_lock);
	if (lo->lo_state != Lo_bound)
		goto out;
	if (unlikely(rw == WRITE && (lo->lo_flags & LO_FLAGS_READ_ONLY)))
		goto out;
	bio_list_add(&lo->lo_bio_list, old_bio);
	wake_up(&lo->lo_event);
	spin_unlock_irq(&lo->lo_lock);
	return 0;

out:
	spin_unlock_irq(&lo->lo_lock);
	bio_io_error(old_bio);
	return 0;
}

/*
 * kick off io on the underlying address space
 */
static void loop_unplug(struct request_queue *q)
{
	struct loop_device *lo = q->queuedata;

	queue_flag_clear_unlocked(QUEUE_FLAG_PLUGGED, q);
	blk_run_address_space(lo->lo_backing_file->f_mapping);
}

struct switch_request {
	struct file *file;
	struct completion wait;
};

static void do_loop_switch(struct loop_device *, struct switch_request *);

/*
 * accumulate_bios: accumulate contiguous bio requests
 *
 * Accumulate contiguous (in respect to file I/O operation) bio requests
 * into iovecs array for aio_{read,write}() call.
 *
 * - bios  : accumulated bio reqs list (out);
 * - iovecs: accumulated iovecs array (out);
 * - ppos  : I/O start position in file (out);
 * - psize : accumulated I/O size (out).
 *
 * Returns: number of iovecs array elements accumulated.
 */
int accumulate_bios(struct loop_device *lo, struct bio_list *bios,
		    struct iovec *iovecs, loff_t *ppos, unsigned *psize)
{
	int i;
	int iov_idx = 0;
	unsigned long op = READ; /* init this to avoid gcc warning */
	loff_t pos;
	loff_t next_pos = 0;
	struct bio *bio;
	struct iovec *iov;
	struct bio_vec *bvec;

	while (!bio_list_empty(&lo->lo_bio_list)) {

		bio = bio_list_peek(&lo->lo_bio_list);

		if (unlikely(!bio->bi_bdev))
			break;

		pos = ((loff_t) bio->bi_sector << 9) + lo->lo_offset;
		if (iov_idx == 0) {
			*ppos = pos;
			*psize = 0;
			op = bio_rw(bio);
		}

		/* Contiguous && same operation */
		if ((next_pos != 0 && pos != next_pos) ||
		    bio_rw(bio) != op)
			break;

		/* Fit into iovecs array */
		BUG_ON(bio->bi_vcnt > IOV_ARR_SIZE);
		if (iov_idx + bio->bi_vcnt > IOV_ARR_SIZE)
			break;

		/* Ok, take it out */
		spin_lock_irq(&lo->lo_lock);
		bio_list_pop(&lo->lo_bio_list);
		spin_unlock_irq(&lo->lo_lock);

		bio_for_each_segment(bvec, bio, i) {
			iov = &iovecs[iov_idx++];
			iov->iov_base = page_address(bvec->bv_page) +
					bvec->bv_offset;
			iov->iov_len = bvec->bv_len;
		}

		bio_list_add(bios, bio);
		next_pos = pos + bio->bi_size;
		*psize += bio->bi_size;
		/*printk(KERN_DEBUG "m0loop%d: accum: "
		       "op=%d idx=%d bio=%p pos=%d size=%d\n", lo->lo_number,
		       (int)op, iov_idx, bio, (int)pos, *psize);*/
	}

	printk(KERN_DEBUG "m0loop%d: accum: op=%u n=%d pos=%lu size=%d\n",
	       lo->lo_number, (int)op, iov_idx, (unsigned long)*ppos, *psize);

	return iov_idx;
}

/*
 * worker thread that handles reads/writes to file backed loop devices,
 * to avoid blocking in our make_request_fn. it also does loop decrypting
 * on reads for block backed loop, as that is too heavy to do from
 * b_end_io context where irqs may be disabled.
 *
 * Loop explanation:  loop_clr_fd() sets lo_state to Lo_rundown before
 * calling kthread_stop().  Therefore once kthread_should_stop() is
 * true, make_request will not place any more requests.  Therefore
 * once kthread_should_stop() is true and lo_bio is NULL, we are
 * done with the loop.
 */
static int loop_thread(void *data)
{
	struct loop_device *lo = data;
	struct bio *bio;
	int iovecs_nr;
	struct bio_list bios;
	loff_t pos;
	unsigned size;

	while (!kthread_should_stop() || !bio_list_empty(&lo->lo_bio_list)) {

		wait_event_interruptible(lo->lo_event,
				!bio_list_empty(&lo->lo_bio_list) ||
				kthread_should_stop());

		bio_list_init(&bios);

		iovecs_nr = accumulate_bios(lo, &bios, lo->key_data,
		                            &pos, &size);

		bio = bio_list_peek(&bios);
		if (bio != NULL) {
			int ret = do_iov_filebacked(lo, bio_rw(bio), iovecs_nr,
			                            pos, size);
			while (!bio_list_empty(&bios))
				bio_endio(bio_list_pop(&bios), ret);
		}

		bio = bio_list_peek(&lo->lo_bio_list);
		if (unlikely(bio && !bio->bi_bdev)) {
			spin_lock_irq(&lo->lo_lock);
			bio_list_pop(&lo->lo_bio_list);
			spin_unlock_irq(&lo->lo_lock);
			do_loop_switch(lo, bio->bi_private);
			bio_put(bio);
		}
	}

	return 0;
}

/*
 * loop_switch performs the hard work of switching a backing store.
 * First it needs to flush existing IO, it does this by sending a magic
 * BIO down the pipe. The completion of this BIO does the actual switch.
 */
static int loop_switch(struct loop_device *lo, struct file *file)
{
	struct switch_request w;
	struct bio *bio = bio_alloc(GFP_KERNEL, 0);
	if (!bio)
		return -ENOMEM;
	init_completion(&w.wait);
	w.file = file;
	bio->bi_private = &w;
	bio->bi_bdev = NULL;
	loop_make_request(lo->lo_queue, bio);
	wait_for_completion(&w.wait);
	return 0;
}

/*
 * Helper to flush the IOs in loop, but keeping loop thread running
 */
static int loop_flush(struct loop_device *lo)
{
	/* loop not yet configured, no running thread, nothing to flush */
	if (!lo->lo_thread)
		return 0;

	return loop_switch(lo, NULL);
}

/*
 * Do the actual switch; called from the BIO completion routine
 */
static void do_loop_switch(struct loop_device *lo, struct switch_request *p)
{
	struct file *file = p->file;
	struct file *old_file = lo->lo_backing_file;
	struct address_space *mapping;

	/* if no new file, only flush of queued bios requested */
	if (!file)
		goto out;

	mapping = file->f_mapping;
	mapping_set_gfp_mask(old_file->f_mapping, lo->old_gfp_mask);
	lo->lo_backing_file = file;
	lo->lo_blocksize = S_ISBLK(mapping->host->i_mode) ?
		mapping->host->i_bdev->bd_block_size : PAGE_SIZE;
	lo->old_gfp_mask = mapping_gfp_mask(mapping);
	mapping_set_gfp_mask(mapping, lo->old_gfp_mask & ~(__GFP_IO|__GFP_FS));
out:
	complete(&p->wait);
}


/*
 * loop_change_fd switched the backing store of a loopback device to
 * a new file. This is useful for operating system installers to free up
 * the original file and in High Availability environments to switch to
 * an alternative location for the content in case of server meltdown.
 * This can only work if the loop device is used read-only, and if the
 * new backing store is the same size and type as the old backing store.
 */
static int loop_change_fd(struct loop_device *lo, struct block_device *bdev,
			  unsigned int arg)
{
	struct file	*file, *old_file;
	struct inode	*inode;
	int		error;

	error = -ENXIO;
	if (lo->lo_state != Lo_bound)
		goto out;

	/* the loop device has to be read-only */
	error = -EINVAL;
	if (!(lo->lo_flags & LO_FLAGS_READ_ONLY))
		goto out;

	error = -EBADF;
	file = fget(arg);
	if (!file)
		goto out;

	inode = file->f_mapping->host;
	old_file = lo->lo_backing_file;

	error = -EINVAL;

	if (!S_ISREG(inode->i_mode) && !S_ISBLK(inode->i_mode))
		goto out_putf;

	/* size of the new backing store needs to be the same */
	if (get_loop_size(lo, file) != get_loop_size(lo, old_file))
		goto out_putf;

	/* and ... switch */
	error = loop_switch(lo, file);
	if (error)
		goto out_putf;

	fput(old_file);
	if (max_part > 0)
		ioctl_by_bdev(bdev, BLKRRPART, 0);
	return 0;

 out_putf:
	fput(file);
 out:
	return error;
}

static inline int is_loop_device(struct file *file)
{
	struct inode *i = file->f_mapping->host;

	return i && S_ISBLK(i->i_mode) && MAJOR(i->i_rdev) == M0LOOP_MAJOR;
}

static int loop_set_fd(struct loop_device *lo, fmode_t mode,
		       struct block_device *bdev, unsigned int arg)
{
	struct file	*file, *f;
	struct inode	*inode;
	struct address_space *mapping;
	unsigned lo_blocksize;
	int		lo_flags = 0;
	int		error;
	loff_t		size;

	/* This is safe, since we have a reference from open(). */
	__module_get(THIS_MODULE);

	error = -EBADF;
	file = fget(arg);
	if (!file)
		goto out;

	error = -EBUSY;
	if (lo->lo_state != Lo_unbound)
		goto out_putf;

	/* Avoid recursion */
	f = file;
	while (is_loop_device(f)) {
		struct loop_device *l;

		if (f->f_mapping->host->i_bdev == bdev)
			goto out_putf;

		l = f->f_mapping->host->i_bdev->bd_disk->private_data;
		if (l->lo_state == Lo_unbound) {
			error = -EINVAL;
			goto out_putf;
		}
		f = l->lo_backing_file;
	}

	mapping = file->f_mapping;
	inode = mapping->host;
	error = -EINVAL;
        if (!S_ISREG(inode->i_mode) && !S_ISBLK(inode->i_mode))
                goto out_putf;

        if (!(file->f_mode & FMODE_WRITE) || !(mode & FMODE_WRITE) ||
            !file->f_op->write)
                lo_flags |= LO_FLAGS_READ_ONLY;

        lo_blocksize = S_ISBLK(inode->i_mode) ?
                inode->i_bdev->bd_block_size : PAGE_SIZE;

	/* m0t1fs must have it */
	if (!file->f_op->aio_read || !file->f_op->aio_write) {
		error = -EINVAL;
		goto out_putf;
	}

	size = get_loop_size(lo, file);

	if ((loff_t)(sector_t)size != size) {
		error = -EFBIG;
		goto out_putf;
	}

	if (!(mode & FMODE_WRITE))
		lo_flags |= LO_FLAGS_READ_ONLY;

	set_device_ro(bdev, (lo_flags & LO_FLAGS_READ_ONLY) != 0);

	lo->lo_blocksize = lo_blocksize;
	lo->lo_device = bdev;
	lo->lo_flags = lo_flags;
	lo->lo_backing_file = file;
	lo->transfer = NULL;
	lo->ioctl = NULL;
	lo->lo_sizelimit = 0;
	lo->old_gfp_mask = mapping_gfp_mask(mapping);
	mapping_set_gfp_mask(mapping, lo->old_gfp_mask & ~(__GFP_IO|__GFP_FS));

	bio_list_init(&lo->lo_bio_list);

	/*
	 * set queue make_request_fn, and add limits based on lower level
	 * device
	 */
	blk_queue_make_request(lo->lo_queue, loop_make_request);
	lo->lo_queue->queuedata = lo;
	lo->lo_queue->unplug_fn = loop_unplug;

	if (!(lo_flags & LO_FLAGS_READ_ONLY) && file->f_op->fsync)
		blkdev_issue_flush(bdev, NULL);

	set_capacity(lo->lo_disk, size);
	bd_set_size(bdev, size << 9);
        /* let user-space know about the new size */
        kobject_uevent(&disk_to_dev(bdev->bd_disk)->kobj, KOBJ_CHANGE);

	set_blocksize(bdev, lo_blocksize);

	blk_queue_bounce_limit(lo->lo_queue, BLK_BOUNCE_ANY);
	blk_queue_logical_block_size(lo->lo_queue, PAGE_SIZE);

	/*
	 * Encryption is never used for m0loop, so we can
	 * reuse lo->key_data pointer for iovecs array.
	 */
	lo->key_data = kzalloc(IOV_ARR_SIZE * sizeof(struct iovec),
	                       GFP_KERNEL);
	if (lo->key_data == NULL) {
		error = -ENOMEM;
		goto out_clr;
	}

	lo->lo_thread = kthread_create(loop_thread, lo, "m0loop%d",
						lo->lo_number);
	if (IS_ERR(lo->lo_thread)) {
		error = PTR_ERR(lo->lo_thread);
		goto out_clr;
	}
	lo->lo_state = Lo_bound;
	wake_up_process(lo->lo_thread);
	if (max_part > 0)
		ioctl_by_bdev(bdev, BLKRRPART, 0);
	return 0;

out_clr:
	lo->lo_thread = NULL;
	lo->lo_device = NULL;
	lo->lo_backing_file = NULL;
	lo->lo_flags = 0;
	set_capacity(lo->lo_disk, 0);
	invalidate_bdev(bdev);
	bd_set_size(bdev, 0);
	mapping_set_gfp_mask(mapping, lo->old_gfp_mask);
	lo->lo_state = Lo_unbound;
 out_putf:
	fput(file);
 out:
	/* This is safe: open() is still holding a reference. */
	module_put(THIS_MODULE);
	return error;
}

static int loop_clr_fd(struct loop_device *lo, struct block_device *bdev)
{
	struct file *filp = lo->lo_backing_file;
	gfp_t gfp = lo->old_gfp_mask;

	if (lo->lo_state != Lo_bound)
		return -ENXIO;

	if (lo->lo_refcnt > 1)	/* we needed one fd for the ioctl */
		return -EBUSY;

        /*
         * If we've explicitly asked to tear down the loop device,
         * and it has an elevated reference count, set it for auto-teardown when
         * the last reference goes away. This stops $!~#$@ udev from
         * preventing teardown because it decided that it needs to run blkid on
         * the loopback device whenever they appear. xfstests is notorious for
         * failing tests because blkid via udev races with a losetup
         * <dev>/do something like mkfs/losetup -d <dev> causing the losetup -d
         * command to fail with EBUSY.
         */
        if (lo->lo_refcnt > 1) {
                lo->lo_flags |= LO_FLAGS_AUTOCLEAR;
                mutex_unlock(&lo->lo_ctl_mutex);
                return 0;
        }

	if (filp == NULL)
		return -EINVAL;

	spin_lock_irq(&lo->lo_lock);
	lo->lo_state = Lo_rundown;
	spin_unlock_irq(&lo->lo_lock);

	kthread_stop(lo->lo_thread);

	lo->lo_queue->unplug_fn = NULL;
	lo->lo_backing_file = NULL;

	lo->transfer = NULL;
	lo->ioctl = NULL;
	lo->lo_device = NULL;
	lo->lo_encryption = NULL;
	lo->lo_offset = 0;
	lo->lo_sizelimit = 0;
	lo->lo_encrypt_key_size = 0;
	lo->lo_flags = 0;
	lo->lo_thread = NULL;
	memset(lo->lo_encrypt_key, 0, LO_KEY_SIZE);
	memset(lo->lo_crypt_name, 0, LO_NAME_SIZE);
	memset(lo->lo_file_name, 0, LO_NAME_SIZE);
	if (bdev)
		invalidate_bdev(bdev);
	set_capacity(lo->lo_disk, 0);
	if (bdev)
		bd_set_size(bdev, 0);
	mapping_set_gfp_mask(filp->f_mapping, gfp);
	kfree(lo->key_data);
	lo->lo_state = Lo_unbound;
	/* This is safe: open() is still holding a reference. */
	module_put(THIS_MODULE);
	if (max_part > 0 && bdev)
		ioctl_by_bdev(bdev, BLKRRPART, 0);
        lo->lo_flags = 0;
	mutex_unlock(&lo->lo_ctl_mutex);
	/*
	 * Need not hold lo_ctl_mutex to fput backing file.
	 * Calling fput holding lo_ctl_mutex triggers a circular
	 * lock dependency possibility warning as fput can take
	 * bd_mutex which is usually taken before lo_ctl_mutex.
	 */
	fput(filp);
	return 0;
}

static int
loop_set_status(struct loop_device *lo, const struct loop_info64 *info)
{
	uid_t uid = current_uid();

	if (lo->lo_encrypt_key_size &&
	    lo->lo_key_owner != uid &&
	    !capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (lo->lo_state != Lo_bound)
		return -ENXIO;
	if ((unsigned int) info->lo_encrypt_key_size > LO_KEY_SIZE)
		return -EINVAL;

	if (info->lo_encrypt_type)
		return -ENOTSUPP;

	if (lo->lo_offset != info->lo_offset ||
	    lo->lo_sizelimit != info->lo_sizelimit) {
		lo->lo_offset = info->lo_offset;
		lo->lo_sizelimit = info->lo_sizelimit;
		if (figure_loop_size(lo))
			return -EFBIG;
	}

	memcpy(lo->lo_file_name, info->lo_file_name, LO_NAME_SIZE);
	memcpy(lo->lo_crypt_name, info->lo_crypt_name, LO_NAME_SIZE);
	lo->lo_file_name[LO_NAME_SIZE-1] = 0;
	lo->lo_crypt_name[LO_NAME_SIZE-1] = 0;

	lo->transfer = NULL;
	lo->ioctl = NULL;

	if ((lo->lo_flags & LO_FLAGS_AUTOCLEAR) !=
	     (info->lo_flags & LO_FLAGS_AUTOCLEAR))
		lo->lo_flags ^= LO_FLAGS_AUTOCLEAR;

	lo->lo_encrypt_key_size = info->lo_encrypt_key_size;
	lo->lo_init[0] = info->lo_init[0];
	lo->lo_init[1] = info->lo_init[1];
	if (info->lo_encrypt_key_size) {
		memcpy(lo->lo_encrypt_key, info->lo_encrypt_key,
		       info->lo_encrypt_key_size);
		lo->lo_key_owner = uid;
	}

	return 0;
}

static int
loop_get_status(struct loop_device *lo, struct loop_info64 *info)
{
	struct file *file = lo->lo_backing_file;
	struct kstat stat;
	int error;

	if (lo->lo_state != Lo_bound)
		return -ENXIO;
	error = vfs_getattr(file->f_path.mnt, file->f_path.dentry, &stat);
	if (error)
		return error;
	M0_SET0(info);
	info->lo_number = lo->lo_number;
	info->lo_device = huge_encode_dev(stat.dev);
	info->lo_inode = stat.ino;
	info->lo_rdevice = huge_encode_dev(lo->lo_device ? stat.rdev : stat.dev);
	info->lo_offset = lo->lo_offset;
	info->lo_sizelimit = lo->lo_sizelimit;
	info->lo_flags = lo->lo_flags;
	memcpy(info->lo_file_name, lo->lo_file_name, LO_NAME_SIZE);
	memcpy(info->lo_crypt_name, lo->lo_crypt_name, LO_NAME_SIZE);
	info->lo_encrypt_type =
		lo->lo_encryption ? lo->lo_encryption->number : 0;
	if (lo->lo_encrypt_key_size && capable(CAP_SYS_ADMIN)) {
		info->lo_encrypt_key_size = lo->lo_encrypt_key_size;
		memcpy(info->lo_encrypt_key, lo->lo_encrypt_key,
		       lo->lo_encrypt_key_size);
	}
	return 0;
}

static void
loop_info64_from_old(const struct loop_info *info, struct loop_info64 *info64)
{
	M0_SET0(info64);
	info64->lo_number = info->lo_number;
	info64->lo_device = info->lo_device;
	info64->lo_inode = info->lo_inode;
	info64->lo_rdevice = info->lo_rdevice;
	info64->lo_offset = info->lo_offset;
	info64->lo_sizelimit = 0;
	info64->lo_encrypt_type = info->lo_encrypt_type;
	info64->lo_encrypt_key_size = info->lo_encrypt_key_size;
	info64->lo_flags = info->lo_flags;
	info64->lo_init[0] = info->lo_init[0];
	info64->lo_init[1] = info->lo_init[1];
	if (info->lo_encrypt_type == LO_CRYPT_CRYPTOAPI)
		memcpy(info64->lo_crypt_name, info->lo_name, LO_NAME_SIZE);
	else
		memcpy(info64->lo_file_name, info->lo_name, LO_NAME_SIZE);
	memcpy(info64->lo_encrypt_key, info->lo_encrypt_key, LO_KEY_SIZE);
}

static int
loop_info64_to_old(const struct loop_info64 *info64, struct loop_info *info)
{
	M0_SET0(info);
	info->lo_number = info64->lo_number;
	info->lo_device = info64->lo_device;
	info->lo_inode = info64->lo_inode;
	info->lo_rdevice = info64->lo_rdevice;
	info->lo_offset = info64->lo_offset;
	info->lo_encrypt_type = info64->lo_encrypt_type;
	info->lo_encrypt_key_size = info64->lo_encrypt_key_size;
	info->lo_flags = info64->lo_flags;
	info->lo_init[0] = info64->lo_init[0];
	info->lo_init[1] = info64->lo_init[1];
	if (info->lo_encrypt_type == LO_CRYPT_CRYPTOAPI)
		memcpy(info->lo_name, info64->lo_crypt_name, LO_NAME_SIZE);
	else
		memcpy(info->lo_name, info64->lo_file_name, LO_NAME_SIZE);
	memcpy(info->lo_encrypt_key, info64->lo_encrypt_key, LO_KEY_SIZE);

	/* error in case values were truncated */
	if (info->lo_device != info64->lo_device ||
	    info->lo_rdevice != info64->lo_rdevice ||
	    info->lo_inode != info64->lo_inode ||
	    info->lo_offset != info64->lo_offset)
		return -EOVERFLOW;

	return 0;
}

static int
loop_set_status_old(struct loop_device *lo, const struct loop_info __user *arg)
{
	struct loop_info info;
	struct loop_info64 info64;

	if (copy_from_user(&info, arg, sizeof (struct loop_info)))
		return -EFAULT;
	loop_info64_from_old(&info, &info64);
	return loop_set_status(lo, &info64);
}

static int
loop_set_status64(struct loop_device *lo, const struct loop_info64 __user *arg)
{
	struct loop_info64 info64;

	if (copy_from_user(&info64, arg, sizeof (struct loop_info64)))
		return -EFAULT;
	return loop_set_status(lo, &info64);
}

static int
loop_get_status_old(struct loop_device *lo, struct loop_info __user *arg) {
	struct loop_info info;
	struct loop_info64 info64;
	int err = 0;

	if (!arg)
		err = -EINVAL;
	if (!err)
		err = loop_get_status(lo, &info64);
	if (!err)
		err = loop_info64_to_old(&info64, &info);
	if (!err && copy_to_user(arg, &info, sizeof(info)))
		err = -EFAULT;

	return err;
}

static int
loop_get_status64(struct loop_device *lo, struct loop_info64 __user *arg) {
	struct loop_info64 info64;
	int err = 0;

	if (!arg)
		err = -EINVAL;
	if (!err)
		err = loop_get_status(lo, &info64);
	if (!err && copy_to_user(arg, &info64, sizeof(info64)))
		err = -EFAULT;

	return err;
}

static int loop_set_capacity(struct loop_device *lo, struct block_device *bdev)
{
	int err;
	sector_t sec;
	loff_t sz;

	err = -ENXIO;
	if (unlikely(lo->lo_state != Lo_bound))
		goto out;
	err = figure_loop_size(lo);
	if (unlikely(err))
		goto out;
	sec = get_capacity(lo->lo_disk);
	/* the width of sector_t may be narrow for bit-shift */
	sz = sec;
	sz <<= 9;
	mutex_lock(&bdev->bd_mutex);
	bd_set_size(bdev, sz);
	mutex_unlock(&bdev->bd_mutex);

 out:
	return err;
}

static int lo_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	struct loop_device *lo = bdev->bd_disk->private_data;
	int err;

	mutex_lock_nested(&lo->lo_ctl_mutex, 1);
	switch (cmd) {
	case LOOP_SET_FD:
		err = loop_set_fd(lo, mode, bdev, arg);
		break;
	case LOOP_CHANGE_FD:
		err = loop_change_fd(lo, bdev, arg);
		break;
	case LOOP_CLR_FD:
		/* loop_clr_fd would have unlocked lo_ctl_mutex on success */
		err = loop_clr_fd(lo, bdev);
		if (!err)
			goto out_unlocked;
		break;
	case LOOP_SET_STATUS:
		err = loop_set_status_old(lo, (struct loop_info __user *) arg);
		break;
	case LOOP_GET_STATUS:
		err = loop_get_status_old(lo, (struct loop_info __user *) arg);
		break;
	case LOOP_SET_STATUS64:
		err = loop_set_status64(lo, (struct loop_info64 __user *) arg);
		break;
	case LOOP_GET_STATUS64:
		err = loop_get_status64(lo, (struct loop_info64 __user *) arg);
		break;
	case LOOP_SET_CAPACITY:
		err = -EPERM;
		if ((mode & FMODE_WRITE) || capable(CAP_SYS_ADMIN))
			err = loop_set_capacity(lo, bdev);
		break;
	default:
		err = lo->ioctl ? lo->ioctl(lo, cmd, arg) : -EINVAL;
	}
	mutex_unlock(&lo->lo_ctl_mutex);

out_unlocked:
	printk(KERN_INFO "m0loop%d: ioctl: cmd=0x%x res=%d\n",
	       lo->lo_number, cmd, err);
	return err;
}

#ifdef CONFIG_COMPAT
struct compat_loop_info {
	compat_int_t	lo_number;      /* ioctl r/o */
	compat_dev_t	lo_device;      /* ioctl r/o */
	compat_ulong_t	lo_inode;       /* ioctl r/o */
	compat_dev_t	lo_rdevice;     /* ioctl r/o */
	compat_int_t	lo_offset;
	compat_int_t	lo_encrypt_type;
	compat_int_t	lo_encrypt_key_size;    /* ioctl w/o */
	compat_int_t	lo_flags;       /* ioctl r/o */
	char		lo_name[LO_NAME_SIZE];
	unsigned char	lo_encrypt_key[LO_KEY_SIZE]; /* ioctl w/o */
	compat_ulong_t	lo_init[2];
	char		reserved[4];
};

/*
 * Transfer 32-bit compatibility structure in userspace to 64-bit loop info
 * - noinlined to reduce stack space usage in main part of driver
 */
static noinline int
loop_info64_from_compat(const struct compat_loop_info __user *arg,
			struct loop_info64 *info64)
{
	struct compat_loop_info info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	M0_SET0(info64);
	info64->lo_number = info.lo_number;
	info64->lo_device = info.lo_device;
	info64->lo_inode = info.lo_inode;
	info64->lo_rdevice = info.lo_rdevice;
	info64->lo_offset = info.lo_offset;
	info64->lo_sizelimit = 0;
	info64->lo_encrypt_type = info.lo_encrypt_type;
	info64->lo_encrypt_key_size = info.lo_encrypt_key_size;
	info64->lo_flags = info.lo_flags;
	info64->lo_init[0] = info.lo_init[0];
	info64->lo_init[1] = info.lo_init[1];
	if (info.lo_encrypt_type == LO_CRYPT_CRYPTOAPI)
		memcpy(info64->lo_crypt_name, info.lo_name, LO_NAME_SIZE);
	else
		memcpy(info64->lo_file_name, info.lo_name, LO_NAME_SIZE);
	memcpy(info64->lo_encrypt_key, info.lo_encrypt_key, LO_KEY_SIZE);
	return 0;
}

/*
 * Transfer 64-bit loop info to 32-bit compatibility structure in userspace
 * - noinlined to reduce stack space usage in main part of driver
 */
static noinline int
loop_info64_to_compat(const struct loop_info64 *info64,
		      struct compat_loop_info __user *arg)
{
	struct compat_loop_info info;

	M0_SET0(&info);
	info.lo_number = info64->lo_number;
	info.lo_device = info64->lo_device;
	info.lo_inode = info64->lo_inode;
	info.lo_rdevice = info64->lo_rdevice;
	info.lo_offset = info64->lo_offset;
	info.lo_encrypt_type = info64->lo_encrypt_type;
	info.lo_encrypt_key_size = info64->lo_encrypt_key_size;
	info.lo_flags = info64->lo_flags;
	info.lo_init[0] = info64->lo_init[0];
	info.lo_init[1] = info64->lo_init[1];
	if (info.lo_encrypt_type == LO_CRYPT_CRYPTOAPI)
		memcpy(info.lo_name, info64->lo_crypt_name, LO_NAME_SIZE);
	else
		memcpy(info.lo_name, info64->lo_file_name, LO_NAME_SIZE);
	memcpy(info.lo_encrypt_key, info64->lo_encrypt_key, LO_KEY_SIZE);

	/* error in case values were truncated */
	if (info.lo_device != info64->lo_device ||
	    info.lo_rdevice != info64->lo_rdevice ||
	    info.lo_inode != info64->lo_inode ||
	    info.lo_offset != info64->lo_offset ||
	    info.lo_init[0] != info64->lo_init[0] ||
	    info.lo_init[1] != info64->lo_init[1])
		return -EOVERFLOW;

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

static int
loop_set_status_compat(struct loop_device *lo,
		       const struct compat_loop_info __user *arg)
{
	struct loop_info64 info64;
	int ret;

	ret = loop_info64_from_compat(arg, &info64);
	if (ret < 0)
		return ret;
	return loop_set_status(lo, &info64);
}

static int
loop_get_status_compat(struct loop_device *lo,
		       struct compat_loop_info __user *arg)
{
	struct loop_info64 info64;
	int err = 0;

	if (!arg)
		err = -EINVAL;
	if (!err)
		err = loop_get_status(lo, &info64);
	if (!err)
		err = loop_info64_to_compat(&info64, arg);
	return err;
}

static int lo_compat_ioctl(struct block_device *bdev, fmode_t mode,
			   unsigned int cmd, unsigned long arg)
{
	struct loop_device *lo = bdev->bd_disk->private_data;
	int err;

	switch(cmd) {
	case LOOP_SET_STATUS:
		mutex_lock(&lo->lo_ctl_mutex);
		err = loop_set_status_compat(
			lo, (const struct compat_loop_info __user *) arg);
		mutex_unlock(&lo->lo_ctl_mutex);
		break;
	case LOOP_GET_STATUS:
		mutex_lock(&lo->lo_ctl_mutex);
		err = loop_get_status_compat(
			lo, (struct compat_loop_info __user *) arg);
		mutex_unlock(&lo->lo_ctl_mutex);
		break;
	case LOOP_SET_CAPACITY:
	case LOOP_CLR_FD:
	case LOOP_GET_STATUS64:
	case LOOP_SET_STATUS64:
		arg = (unsigned long) compat_ptr(arg);
	case LOOP_SET_FD:
	case LOOP_CHANGE_FD:
		err = lo_ioctl(bdev, mode, cmd, arg);
		break;
	default:
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}
#endif

static int lo_open(struct block_device *bdev, fmode_t mode)
{
	struct loop_device *lo = bdev->bd_disk->private_data;

	printk(KERN_INFO "m0loop%d: open\n", lo->lo_number);

	mutex_lock(&lo->lo_ctl_mutex);
	lo->lo_refcnt++;
	mutex_unlock(&lo->lo_ctl_mutex);

	return 0;
}

static int lo_release(struct gendisk *disk, fmode_t mode)
{
	struct loop_device *lo = disk->private_data;
	int err;

	mutex_lock(&lo->lo_ctl_mutex);

	if (--lo->lo_refcnt)
		goto out;

	if (lo->lo_flags & LO_FLAGS_AUTOCLEAR) {
		/*
		 * In autoclear mode, stop the loop thread
		 * and remove configuration after last close.
		 */
		err = loop_clr_fd(lo, NULL);
		if (!err)
			goto out_unlocked;
	} else {
		/*
		 * Otherwise keep thread (if running) and config,
		 * but flush possible ongoing bios in thread.
		 */
		loop_flush(lo);
	}

out:
	mutex_unlock(&lo->lo_ctl_mutex);
out_unlocked:
	printk(KERN_INFO "m0loop%d: release\n", lo->lo_number);
	return 0;
}

static const struct block_device_operations lo_fops = {
	.owner =	THIS_MODULE,
	.open =		lo_open,
	.release =	lo_release,
	.ioctl =	lo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =	lo_compat_ioctl,
#endif
};

/*
 * And now the modules code and kernel interface.
 */
static int max_loop;
module_param(max_loop, int, 0);
MODULE_PARM_DESC(max_loop, "Maximum number of loop devices");
module_param(max_part, int, 0);
MODULE_PARM_DESC(max_part, "Maximum number of partitions per loop device");
MODULE_LICENSE("GPL");

int loop_register_transfer(struct loop_func_table *funcs)
{
	return 0;
}

int loop_unregister_transfer(int number)
{
	return 0;
}


static struct loop_device *loop_alloc(int i)
{
	struct loop_device *lo;
	struct gendisk *disk;

	lo = kzalloc(sizeof(*lo), GFP_KERNEL);
	if (!lo)
		goto out;

	spin_lock_init(&lo->lo_lock);

	lo->lo_queue = blk_alloc_queue(GFP_KERNEL);
	if (!lo->lo_queue)
		goto out_free_dev;
	/*
	 * RedHat's blk_throtl_exit() use queue_lock,
	 * so we have to initialize it here to something
	 * (lo_lock seems to be ok) to be able
	 * to unload the module without kernel crash.
	 */
	lo->lo_queue->queue_lock = &lo->lo_lock;

	disk = lo->lo_disk = alloc_disk(1 << part_shift);
	if (!disk)
		goto out_free_queue;

	mutex_init(&lo->lo_ctl_mutex);
	lo->lo_number		= i;
	lo->lo_thread		= NULL;
	init_waitqueue_head(&lo->lo_event);
	disk->major		= M0LOOP_MAJOR;
	disk->first_minor	= i << part_shift;
	disk->fops		= &lo_fops;
	disk->private_data	= lo;
	disk->queue		= lo->lo_queue;
	sprintf(disk->disk_name, "m0loop%d", i);
	return lo;

out_free_queue:
	blk_cleanup_queue(lo->lo_queue);
out_free_dev:
	kfree(lo);
out:
	return NULL;
}

static void loop_free(struct loop_device *lo)
{
	blk_cleanup_queue(lo->lo_queue);
	put_disk(lo->lo_disk);
	list_del(&lo->lo_list);
	kfree(lo);
}

static struct loop_device *loop_init_one(int i)
{
	struct loop_device *lo;

	list_for_each_entry(lo, &loop_devices, lo_list) {
		if (lo->lo_number == i)
			return lo;
	}

	lo = loop_alloc(i);
	if (lo) {
		add_disk(lo->lo_disk);
		list_add_tail(&lo->lo_list, &loop_devices);
	}
	return lo;
}

static void loop_del_one(struct loop_device *lo)
{
	del_gendisk(lo->lo_disk);
	loop_free(lo);
}

static struct kobject *loop_probe(dev_t dev, int *part, void *data)
{
	struct loop_device *lo;
	struct kobject *kobj;

	mutex_lock(&loop_devices_mutex);
	lo = loop_init_one(dev & MINORMASK);
	kobj = lo ? get_disk(lo->lo_disk) : ERR_PTR(-ENOMEM);
	mutex_unlock(&loop_devices_mutex);

	*part = 0;
	return kobj;
}

static int __init loop_init(void)
{
	int i, nr;
	unsigned long range;
	struct loop_device *lo, *next;

	/*
	 * loop module now has a feature to instantiate underlying device
	 * structure on-demand, provided that there is an access dev node.
	 * However, this will not work well with user space tool that doesn't
	 * know about such "feature".  In order to not break any existing
	 * tool, we do the following:
	 *
	 * (1) if max_loop is specified, create that many upfront, and this
	 *     also becomes a hard limit.
	 * (2) if max_loop is not specified, create 8 loop device on module
	 *     load, user can further extend loop device by create dev node
	 *     themselves and have kernel automatically instantiate actual
	 *     device on-demand.
	 */

	part_shift = 0;
	if (max_part > 0)
		part_shift = fls(max_part);

	if (max_loop > 1UL << (MINORBITS - part_shift))
		return -EINVAL;

	if (max_loop) {
		nr = max_loop;
		range = max_loop;
	} else {
		nr = 8;
		range = 1UL << (MINORBITS - part_shift);
	}

	if ((M0LOOP_MAJOR = register_blkdev(0, "m0loop"))) {
		printk("m0loop: major %d\n", M0LOOP_MAJOR);
		if (M0LOOP_MAJOR < 0)
			return -EIO;
	}

	for (i = 0; i < nr; i++) {
		lo = loop_alloc(i);
		if (!lo)
			goto Enomem;
		list_add_tail(&lo->lo_list, &loop_devices);
	}

	/* point of no return */

	list_for_each_entry(lo, &loop_devices, lo_list)
		add_disk(lo->lo_disk);

	blk_register_region(MKDEV(M0LOOP_MAJOR, 0), range,
				  THIS_MODULE, loop_probe, NULL, NULL);

	printk(KERN_INFO "m0loop: module loaded\n");
	return 0;

Enomem:
	printk(KERN_WARNING "m0loop: out of memory\n");

	list_for_each_entry_safe(lo, next, &loop_devices, lo_list)
		loop_free(lo);

	unregister_blkdev(M0LOOP_MAJOR, "m0loop");
	return -ENOMEM;
}

static void __exit loop_exit(void)
{
	unsigned long range;
	struct loop_device *lo, *next;

	range = max_loop ? max_loop :  1UL << (MINORBITS - part_shift);

	list_for_each_entry_safe(lo, next, &loop_devices, lo_list)
		loop_del_one(lo);

	blk_unregister_region(MKDEV(M0LOOP_MAJOR, 0), range);
	unregister_blkdev(M0LOOP_MAJOR, "m0loop");
}

module_init(loop_init);
module_exit(loop_exit);

#ifndef MODULE
static int __init max_loop_setup(char *str)
{
	max_loop = simple_strtol(str, NULL, 0);
	return 1;
}

__setup("max_loop=", max_loop_setup);
#endif
