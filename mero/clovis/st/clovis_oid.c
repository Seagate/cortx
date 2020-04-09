/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 17-Nov-2014
 */

/**
 * A simple implementation of extent-based oid allocator for ST.
 * (1) It supports single oid and range oid allocation and free.
 * (2) FID extents are linked in a ordered list and performance is
 *     not our focus for this version
 * (3) Simple allocation policy: the first found free oids first
 *     if there is no free object ID, we simply return with an error
 *     and we don't wait for others to release object IDs.
 */

#include "clovis/clovis.h"
#include "clovis/st/clovis_st_misc.h"

#ifdef __KERNEL__

#include <linux/module.h>

#else

#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>             /* for pthread_mutex_xxx */

static pthread_mutex_t oid_extent_list_lock;

#endif

struct clovis_oid_extent {
	uint64_t                  oe_start;
	uint64_t                  oe_range;
	struct clovis_oid_extent *oe_prev;
	struct clovis_oid_extent *oe_next;
};

/*
 * List head of extents. It contains all free object IDs when
 * the st framework is up and will split/merge into multiple
 * extents when the ST is running.
 */
static struct clovis_oid_extent oid_extent_lh = {
	.oe_start = 0x100000UL,
	.oe_range = 0xfffffffffffffeffUL,
	.oe_prev  = &oid_extent_lh,
	.oe_next  = &oid_extent_lh
};

enum {
	CLOVIS_MIN_APP_OID = 0x100000UL
};

static void oid_extent_lock(void)
{
#ifndef __KERNEL__
	pthread_mutex_lock(&oid_extent_list_lock);
#endif
}

static void oid_extent_unlock(void)
{
#ifndef __KERNEL__
	pthread_mutex_unlock(&oid_extent_list_lock);
#endif
}

static int oid_extent_is_overlapped(uint64_t start1, uint64_t range1,
				    uint64_t start2, uint64_t range2)
{

	if (start1 > start2 && start1 < start2 + range2)
		return 1;

	if (start2 > start1 && start2 < start1 + range1)
		return 1;

	return 0;
}

static void oid_extent_delete(struct clovis_oid_extent *oe)
{
	struct clovis_oid_extent *prev;
	struct clovis_oid_extent *next;

	/*
	 * If someone is trying to delete list head, this means
	 * we run out of free object IDs. Treat this differently.
	 */
	if (oe == &oid_extent_lh) {
		oe->oe_range = 0x0UL;
		oe->oe_next  = &oid_extent_lh;
		oe->oe_prev  = &oid_extent_lh;
		return;
	}

	/* Be careful for the extents next to the head*/
	prev = oe->oe_prev;
	next = oe->oe_next;
	prev->oe_next = next;
	next->oe_prev = prev;

	/* remember to free memory*/
	mem_free(oe);
}

static void oid_extent_split(struct clovis_oid_extent *oe,
			     uint64_t wanted)
{
	uint64_t allocated;

	allocated = (oe->oe_range < wanted)?oe->oe_range:wanted;

	oe->oe_start += allocated;
	oe->oe_range -= allocated;

	if (oe->oe_range == 0)
		oid_extent_delete(oe);
}

static void oid_extent_merge(struct clovis_oid_extent *oe,
			     uint64_t s_oid, uint64_t nr_oids,
			     int direction)
{
	struct clovis_oid_extent *prev;
	struct clovis_oid_extent *next;

	/* Merge with extent in front of me*/
	if (direction == 0) {
		oe->oe_range += nr_oids;

		next = oe->oe_next;
		if (next != &oid_extent_lh
		    && (next->oe_start == oe->oe_start + oe->oe_range))
		{
			oe->oe_range += next->oe_range;
			oid_extent_delete(next);
		}

		return;
	}

	/* Merge with extent behind me*/
	oe->oe_start = s_oid;
	oe->oe_range += nr_oids;

	prev = oe->oe_prev;
	if (prev != &oid_extent_lh
	    && (s_oid == prev->oe_start + prev->oe_range))
	{
		prev->oe_range += oe->oe_range;
		oid_extent_delete(oe);
	}

	return;
}

/**
 * Insert an extent in head
 */
static void oid_extent_insert_head(uint64_t s_oid, uint64_t nr_oids)
{
	uint64_t                  a;
	struct clovis_oid_extent *lhp;
	struct clovis_oid_extent *new_oe;

	lhp = &oid_extent_lh;

	/* if all object IDs have been allocated */
	if (lhp->oe_range == 0) {
		lhp->oe_start = s_oid;
		lhp->oe_range = nr_oids;
		return;
	}

	/* can we merge? */
	if (lhp->oe_start == s_oid + nr_oids
	    || (lhp->oe_start + lhp->oe_range) == s_oid)
	{
		lhp->oe_start =
		    (lhp->oe_start < s_oid)?lhp->oe_start:s_oid;
		lhp->oe_range = lhp->oe_range + nr_oids;

		return;
	}

	/* Overlapping is not allowed */
	if (oid_extent_is_overlapped(
		lhp->oe_start, lhp->oe_range, s_oid, nr_oids))
		return;

	/* Allocate memory and initialise extent */
	new_oe = mem_alloc(sizeof *new_oe);
	if (new_oe == NULL)
		return;

	new_oe->oe_start = s_oid;
	new_oe->oe_range = nr_oids;

	/* Ensure that extent list is in increasing order */
	if (lhp->oe_start > new_oe->oe_start) {
		a = lhp->oe_start;
		lhp->oe_start = new_oe->oe_start;
		new_oe->oe_start = a;

		a = lhp->oe_range;
		lhp->oe_range = new_oe->oe_range;
		new_oe->oe_range =a;
	}

	new_oe->oe_prev  = lhp;
	new_oe->oe_next  = lhp;
	lhp->oe_prev     = new_oe;
	lhp->oe_next     = new_oe;
}

/**
 * Insert oe to a place where:
 * a. s_oid > prev->oe_start + prev->oe_range
 *  b. oe->oe_start > s_oid + nr_oids
 *
 */
static void oid_extent_insert(uint64_t s_oid, uint64_t nr_oids)
{
	struct clovis_oid_extent *lhp;
	struct clovis_oid_extent *prev;
	struct clovis_oid_extent *oe;
	struct clovis_oid_extent *new_oe;

	oid_extent_lock();
	lhp = &oid_extent_lh;

	/*
	 * In some cases released oids are to insert in head.
	 * A. Only one extent (head).
	 * B. s_oid is smaller than the current oid in head.
	 */
	if ((lhp->oe_prev == lhp && lhp->oe_next == lhp)
	    || s_oid < lhp->oe_start)
	{
		oid_extent_insert_head(s_oid, nr_oids);
		goto UNLOCK_EXIT;
	}

	/* Search for the right place*/
	prev = lhp;
	oe = lhp->oe_next;
	while (oe != lhp) {
		if (s_oid >= prev->oe_start + prev->oe_range
                    && oe->oe_start >= s_oid + nr_oids)
			break;

		prev = oe;
		oe = oe->oe_next;
	}

	/* Can we merge? It is possible to merge 3 extents*/
	if (s_oid == prev->oe_start + prev->oe_range) {
		oid_extent_merge(prev, s_oid, nr_oids, 0);
		goto UNLOCK_EXIT;
	} else if (oe->oe_start == s_oid + nr_oids) {
		oid_extent_merge(oe, s_oid, nr_oids, 1);
		goto UNLOCK_EXIT;
	}

	/* Allocate a new extent */
	new_oe = (struct clovis_oid_extent *)
		 mem_alloc(sizeof *new_oe);
	if (new_oe == NULL)
		goto UNLOCK_EXIT;

	new_oe->oe_start = s_oid;
	new_oe->oe_range = nr_oids;
	new_oe->oe_prev  = prev;
	new_oe->oe_next  = oe;
	oe->oe_prev      = new_oe;
	prev->oe_next    = new_oe;

UNLOCK_EXIT:
	oid_extent_unlock();
}

static void oid_fill(struct m0_uint128 *oids, uint64_t s_id, int nr_id)
{
	int i;

	for (i = 0; i < nr_id; i++) {
		oids[i].u_hi = 0x0UL;
		oids[i].u_lo = CLOVIS_MIN_APP_OID + s_id + i;
	}
}

/**
 * Lookup the first available set of oids and allocate
 *
 * @param oids: array for returned object IDs
 * @param nr_oids: number of wanted object IDs
 * @return: < 0, no free
 */
static uint64_t oid_alloc(struct m0_uint128 *oids, uint64_t nr_oids)
{
	uint64_t                  nr_allocated;
	uint64_t                  nr_wanted;
	struct clovis_oid_extent *oe;
	struct clovis_oid_extent *next;
	struct clovis_oid_extent *lhp;

	/* lock the extent list first */
	oid_extent_lock();
	lhp = &oid_extent_lh;

	/* scan the whole list*/
	nr_allocated = 0;
	nr_wanted = nr_oids;
	oe = lhp;

	do {
		next = oe->oe_next;

		/* This exent has enough free IDs*/
		if (oe->oe_range > nr_wanted) {
			oid_fill(oids + nr_allocated, oe->oe_start, nr_wanted);
			oid_extent_split(oe, nr_wanted);
			nr_allocated += nr_wanted;
			nr_wanted -= 0;

			break;
		}

		/* This extent has less free IDs than required */
		oid_fill(oids + nr_allocated, oe->oe_start, oe->oe_range);
		nr_allocated += oe->oe_range;
		nr_wanted -= oe->oe_range;
		oid_extent_delete(oe);

		oe = next;
	} while (oe != lhp && nr_wanted != 0);

	/* unlock */
	oid_extent_unlock();

	return nr_allocated;
}

/**
 * Allocate a single object id
 *
 * @param oid: returned object ID
 * @return: == 0 ok, < 0 can't allocate free oid
 */
int clovis_oid_get(struct m0_uint128 *oid)
{
	uint64_t nr_allocated;

	nr_allocated = oid_alloc(oid, 1);
	if (nr_allocated == 0)
		return -EAGAIN;

	return 0;
}

void clovis_oid_put(struct m0_uint128 oid)
{
	oid_extent_insert(oid.u_lo, 1);
}

/**
 * Get a number of object IDs in one call
 *
 * @param oids: returned object IDs
 * @param nr_oids: the number of object IDs wanted
 * @retun: == 0, there is no free object IDs available.
 *          > 0, the allocated number of object IDs
 */
uint64_t clovis_oid_get_many(struct m0_uint128 *oids, uint64_t nr_oids)
{
	uint64_t nr_allocated;

	nr_allocated = oid_alloc(oids, nr_oids);
	if (nr_allocated == 0)
		return 0;

	return nr_allocated;
}

void clovis_oid_put_many(struct m0_uint128 *oids, uint64_t nr_oids)
{
	uint64_t i;
	uint64_t j;

	for (i = 0; i < nr_oids; ) {
		/* look for those oids are next to each other */
		for (j = i + 1; j < nr_oids; j++) {
			if (oids[j].u_hi != oids[j-1].u_hi
			    || oids[j].u_lo != oids[j-1].u_lo + 1)
				break;
		}

		oid_extent_insert(oids[i].u_lo, j - i);
		i = j;
	}
}

int clovis_oid_allocator_init(void)
{
	uint64_t offset;

	/*
	 * Set the values of extent list head: the oe_start is
	 * offseted with a random number in order to enable multiple runs
	 * of ST (this is different from setting the number of rounds in
	 * one single ST run although they can achieve the same thing).
	 *
	 * We use the value of tv_usec in struct timeval returned by
	 * time_now(). [generate_random() isn't used as random() creates
	 * pseudo random number]
	 */
	offset = time_now() % 0xffffUL;

	oid_extent_lh.oe_start = CLOVIS_MIN_APP_OID + offset; //0xffffUL * offset;
	oid_extent_lh.oe_range = 0xffffffffffffffffUL - oid_extent_lh.oe_start;
	oid_extent_lh.oe_prev  = &oid_extent_lh;
	oid_extent_lh.oe_next  = &oid_extent_lh;

#ifdef __KERNEL__
	return 0;
#else
	return pthread_mutex_init(&oid_extent_list_lock, NULL);
#endif
}

int clovis_oid_allocator_fini(void)
{
#ifdef __KERNEL__
	return 0;
#else
	return pthread_mutex_destroy(&oid_extent_list_lock);
#endif
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
