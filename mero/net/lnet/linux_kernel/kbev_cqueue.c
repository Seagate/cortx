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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 3/6/2012
 */

/**
   @addtogroup bevcqueue
   @{
 */

#include <linux/highmem.h>      /* kmap_atomic */

/**
   Determines the next element in the queue that can be used by the producer.
   This operation causes the page containing the next element to be mapped
   using @c kmap_atomic().
   @note This operation is to be used only by the producer.
   @param q the queue
   @returns a pointer to the next available element in the producer context
   @pre bev_cqueue_invariant(q)
   @post p->cbl_c_self != q->cbcq_consumer
 */
static struct nlx_core_bev_link *bev_cqueue_pnext(
					   const struct nlx_core_bev_cqueue *q)
{
	struct nlx_core_bev_link *p;
	const struct nlx_core_kmem_loc *loc;
	char *ptr;

	M0_PRE(bev_cqueue_invariant(q));
	loc = &q->cbcq_producer_loc;
	M0_PRE(nlx_core_kmem_loc_invariant(loc) && _0C(loc->kl_page != NULL));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	ptr = kmap_atomic(loc->kl_page);
#else
	ptr = kmap_atomic(loc->kl_page, KM_USER1);
#endif
	p = (struct nlx_core_bev_link *) (ptr + loc->kl_offset);
	M0_POST(nlx_core_kmem_loc_invariant(&p->cbl_p_self_loc));
	M0_POST(p->cbl_c_self != q->cbcq_consumer);
	return p;
}

/**
   Puts (produces) an element so it can be consumed.  The caller must first
   call bev_cqueue_pnext() to ensure such an element exists.  The page
   containing the element is unmapped using @c kunmap_atomic().
   @param q the queue
   @param p current element, previously obtained using bev_cqueue_pnext()
   @pre bev_cqueue_invariant(q) && p->cbl_c_self != q->cbcq_consumer
 */
static void bev_cqueue_put(struct nlx_core_bev_cqueue *q,
			   struct nlx_core_bev_link *p)
{

	M0_PRE(bev_cqueue_invariant(q));
	M0_PRE(p->cbl_c_self != q->cbcq_consumer);
	M0_PRE(nlx_core_kmem_loc_eq(&q->cbcq_producer_loc, &p->cbl_p_self_loc));
	q->cbcq_producer_loc = p->cbl_p_next_loc;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	kunmap_atomic(p);
#else
	kunmap_atomic(p, KM_USER1);
#endif
	m0_atomic64_inc(&q->cbcq_count);
}

/**
   Blesses the nlx_core_bev_link of a nlx_core_bev_cqueue element, assigning
   the producer self value.
   @param ql The link to bless, the caller must have already mapped the element
   into the producer address space.
   @param pg The page object corresponding to the link object.
 */
static void bev_link_bless(struct nlx_core_bev_link *ql, struct page *pg)
{
	nlx_core_kmem_loc_set(&ql->cbl_p_self_loc,
			      pg, NLX_PAGE_OFFSET((unsigned long) ql));
}

/** @} */ /* bevcqueue */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
