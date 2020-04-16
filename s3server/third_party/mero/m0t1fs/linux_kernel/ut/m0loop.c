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
 * Original author: Andriy Tkachuk <andriy_tkachuk@xyratex.com>
 * Original creation date: 08/29/2012
 */

#include <linux/bio.h>
#include <linux/loop.h>

#include "ut/ut.h"
#include "m0t1fs/linux_kernel/m0loop_internal.h"


static void loop_dev_init(struct loop_device *lo)
{
	spin_lock_init(&lo->lo_lock);
	bio_list_init(&lo->lo_bio_list);
	lo->lo_offset = 0;
}

static struct iovec iovecs[IOV_ARR_SIZE];

/*
 * Basic functionality tests:
 *
 *   - One bio request (in the queue) with one segment. One element in
 *     iovecs array is returned with correct file pos and I/O size.
 */
static void accum_bios_basic1(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	bio = bio_alloc(GFP_KERNEL, 1);
	BUG_ON(bio == NULL);

	bio->bi_bdev = (void*)1;
	bio->bi_vcnt = 1;
	bio->bi_size = PAGE_SIZE;

	bio_list_add(&lo.lo_bio_list, bio);

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 1);
	BUG_ON(pos != 0);
	BUG_ON(size != PAGE_SIZE);

	bio_put(bio);
}

/*
 *   - One bio request with two segments. Two elements in iovecs array
 *     are returned with correct file pos and summary I/O size.
 */
static void accum_bios_basic2(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	bio = bio_alloc(GFP_KERNEL, 2);
	BUG_ON(bio == NULL);

	bio->bi_bdev = (void*)1;
	bio->bi_vcnt = 2;
	bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

	bio_list_add(&lo.lo_bio_list, bio);

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 2);
	BUG_ON(pos != 0);
	BUG_ON(size != bio->bi_vcnt * PAGE_SIZE);

	bio_put(bio);
}

/*
 *   - Two bio requests (for the same read/write operation), one segment
 *     each, for contiguous file region. Two elements in iovecs array
 *     are returned with correct file pos and summary I/O size.
 */
static void accum_bios_basic3(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;
	int i;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	for (i = 0; i < 2; ++i) {
		bio = bio_alloc(GFP_KERNEL, 1);
		BUG_ON(bio == NULL);

		bio->bi_bdev = (void*)1;
		bio->bi_sector = PAGE_SIZE / 512 * i;
		bio->bi_vcnt = 1;
		bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

		bio_list_add(&lo.lo_bio_list, bio);
	}

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 2);
	BUG_ON(pos != 0);
	BUG_ON(size != 2*PAGE_SIZE);

	BUG_ON(bio_list_size(&bios) != 2);
	while (!bio_list_empty(&bios))
		bio_put(bio_list_pop(&bios));
}

/*
 * Exception cases tests:
 *
 *   - Two bio requests (one segment each) but for non-contiguous file
 *     regions. Two calls are expected with one element in iovecs array
 *     returned each time.
 */
static void accum_bios_except1(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;
	int i;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	for (i = 0; i < 2; ++i) {
		bio = bio_alloc(GFP_KERNEL, 1);
		BUG_ON(bio == NULL);

		bio->bi_bdev = (void*)1;
		bio->bi_sector = 0;
		bio->bi_vcnt = 1;
		bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

		bio_list_add(&lo.lo_bio_list, bio);
	}

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 1);
	BUG_ON(pos != 0);
	BUG_ON(size != PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 1);
	BUG_ON(bio_list_size(&bios) != 1);

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 1);
	BUG_ON(pos != 0);
	BUG_ON(size != PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 0);
	BUG_ON(bio_list_size(&bios) != 2);

	while (!bio_list_empty(&bios))
		bio_put(bio_list_pop(&bios));
}

/*
 *   - Two bio requests for contiguous file region, but for different
 *     operations: one for read, another for write. Two calls are
 *     expected with one element in iovecs array returned each time.
 */
static void accum_bios_except2(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;
	int i;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	for (i = 0; i < 2; ++i) {
		bio = bio_alloc(GFP_KERNEL, 1);
		BUG_ON(bio == NULL);

		bio->bi_bdev = (void*)1;
		bio->bi_sector = PAGE_SIZE / 512 * i;
		bio->bi_rw = i;
		bio->bi_vcnt = 1;
		bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

		bio_list_add(&lo.lo_bio_list, bio);
	}

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 1);
	BUG_ON(pos != 0);
	BUG_ON(size != PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 1);
	BUG_ON(bio_list_size(&bios) != 1);

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 1);
	BUG_ON(pos != PAGE_SIZE);
	BUG_ON(size != PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 0);
	BUG_ON(bio_list_size(&bios) != 2);

	while (!bio_list_empty(&bios))
		bio_put(bio_list_pop(&bios));
}

/*
 * Iovecs array boundary (IOV_ARR_SIZE) tests (contiguous file region):
 *
 *   - IOV_ARR_SIZE bio requests in the list (one segment each).
 *     IOV_ARR_SIZE elements in iovecs array are returned in one call.
 */
static void accum_bios_bound1(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;
	int i;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	for (i = 0; i < IOV_ARR_SIZE; ++i) {
		bio = bio_alloc(GFP_KERNEL, 1);
		BUG_ON(bio == NULL);

		bio->bi_bdev = (void*)1;
		bio->bi_sector = PAGE_SIZE / 512 * i;
		bio->bi_vcnt = 1;
		bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

		bio_list_add(&lo.lo_bio_list, bio);
	}

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != IOV_ARR_SIZE);
	BUG_ON(pos != 0);
	BUG_ON(size != IOV_ARR_SIZE * PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 0);
	BUG_ON(bio_list_size(&bios) != IOV_ARR_SIZE);

	while (!bio_list_empty(&bios))
		bio_put(bio_list_pop(&bios));
}

/*
 *   - (IOV_ARR_SIZE + 1) bio requests in the list. Two calls are
 *     expected: one with IOV_ARR_SIZE elements in iovecs array
 *     returned, another with one element returned.
 */
static void accum_bios_bound2(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;
	int i;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	for (i = 0; i < IOV_ARR_SIZE + 1; ++i) {
		bio = bio_alloc(GFP_KERNEL, 1);
		BUG_ON(bio == NULL);

		bio->bi_bdev = (void*)1;
		bio->bi_sector = PAGE_SIZE / 512 * i;
		bio->bi_vcnt = 1;
		bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

		bio_list_add(&lo.lo_bio_list, bio);
	}

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != IOV_ARR_SIZE);
	BUG_ON(pos != 0);
	BUG_ON(size != IOV_ARR_SIZE * PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 1);
	BUG_ON(bio_list_size(&bios) != IOV_ARR_SIZE);

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 1);
	BUG_ON(pos != IOV_ARR_SIZE * PAGE_SIZE);
	BUG_ON(size != PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 0);
	BUG_ON(bio_list_size(&bios) != IOV_ARR_SIZE + 1);

	while (!bio_list_empty(&bios))
		bio_put(bio_list_pop(&bios));
}

/*
 *   - (IOV_ARR_SIZE - 1) bio requests one segment each and one bio
 *     request with two segments. Two calls are expected: one with
 *     (IOV_ARR_SIZE - 1) elements in iovecs array returned, another
 *     with two elements returned.
 */
static void accum_bios_bound3(void)
{
	struct loop_device lo;
	struct bio *bio;
	struct bio_list bios;
	loff_t pos;
	unsigned size;
	unsigned n;
	int i;

	loop_dev_init(&lo);
	bio_list_init(&bios);

	for (i = 0; i < IOV_ARR_SIZE - 1; ++i) {
		bio = bio_alloc(GFP_KERNEL, 1);
		BUG_ON(bio == NULL);

		bio->bi_bdev = (void*)1;
		bio->bi_sector = PAGE_SIZE / 512 * i;
		bio->bi_vcnt = 1;
		bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

		bio_list_add(&lo.lo_bio_list, bio);
	}

	bio = bio_alloc(GFP_KERNEL, 2);
	BUG_ON(bio == NULL);

	bio->bi_bdev = (void*)1;
	bio->bi_sector = PAGE_SIZE / 512 * i;
	bio->bi_vcnt = 2;
	bio->bi_size = bio->bi_vcnt * PAGE_SIZE;

	bio_list_add(&lo.lo_bio_list, bio);

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != IOV_ARR_SIZE - 1);
	BUG_ON(pos != 0);
	BUG_ON(size != (IOV_ARR_SIZE - 1) * PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 1);
	BUG_ON(bio_list_size(&bios) != IOV_ARR_SIZE - 1);

	n = accumulate_bios(&lo, &bios, iovecs, &pos, &size);
	BUG_ON(n != 2);
	BUG_ON(pos != (IOV_ARR_SIZE - 1) * PAGE_SIZE);
	BUG_ON(size != 2 * PAGE_SIZE);
	BUG_ON(bio_list_size(&lo.lo_bio_list) != 0);
	BUG_ON(bio_list_size(&bios) != IOV_ARR_SIZE);

	while (!bio_list_empty(&bios))
		bio_put(bio_list_pop(&bios));
}

struct m0_ut_suite m0_loop_ut = {
	.ts_name = "m0loop-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "accum_bios_basic1", accum_bios_basic1},
		{ "accum_bios_basic2", accum_bios_basic2},
		{ "accum_bios_basic3", accum_bios_basic3},
		{ "accum_bios_except1", accum_bios_except1},
		{ "accum_bios_except2", accum_bios_except2},
		{ "accum_bios_bound1", accum_bios_bound1},
		{ "accum_bios_bound2", accum_bios_bound2},
		{ "accum_bios_bound3", accum_bios_bound3},
		{ NULL, NULL }
	}
};
M0_EXPORTED(m0_loop_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
