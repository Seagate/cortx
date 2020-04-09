/* -*- C -*- */
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 05/07/2011
 */

#include "lib/queue.h"
#include "lib/assert.h"
#include "lib/misc.h"    /* NULL */

/**
   @addtogroup queue Queue

   When a queue is not empty, last element's m0_queue_link::ql_next is set to
   "end-of-queue" marker (EOQ). This guarantees that an element is in a queue
   iff ql_next is not NULL (see m0_queue_link_is_in()).

   When a queue is empty, its head and tail are set to EOQ. This allows
   iteration over queue elements via loop of the form

   @code
   for (scan = q->q_head; scan != EOQ; scan = scan->ql_next) { ... }
   @endcode

   independently of whether the queue is empty.

   @{
 */

#define EOQ ((struct m0_queue_link *)8)

const struct m0_queue M0_QUEUE_INIT = {
	.q_head = EOQ,
	.q_tail = EOQ
};

M0_INTERNAL void m0_queue_init(struct m0_queue *q)
{
	q->q_head = q->q_tail = EOQ;
	M0_ASSERT(m0_queue_invariant(q));
}

M0_INTERNAL void m0_queue_fini(struct m0_queue *q)
{
	M0_ASSERT(m0_queue_invariant(q));
	M0_ASSERT(m0_queue_is_empty(q));
}

M0_INTERNAL bool m0_queue_is_empty(const struct m0_queue *q)
{
	M0_ASSERT_EX(m0_queue_invariant(q));
	return q->q_head == EOQ;
}

M0_INTERNAL void m0_queue_link_init(struct m0_queue_link *ql)
{
	ql->ql_next = NULL;
}

M0_INTERNAL void m0_queue_link_fini(struct m0_queue_link *ql)
{
	M0_ASSERT(!m0_queue_link_is_in(ql));
}

M0_INTERNAL bool m0_queue_link_is_in(const struct m0_queue_link *ql)
{
	return ql->ql_next != NULL;
}

M0_INTERNAL bool m0_queue_contains(const struct m0_queue *q,
				   const struct m0_queue_link *ql)
{
	struct m0_queue_link *scan;

	M0_ASSERT(m0_queue_invariant(q));
	for (scan = q->q_head; scan != EOQ; scan = scan->ql_next) {
		M0_ASSERT(scan != NULL);
		if (scan == ql)
			return true;
	}
	return false;
}

M0_INTERNAL size_t m0_queue_length(const struct m0_queue *q)
{
	size_t length;
	struct m0_queue_link *scan;

	M0_ASSERT_EX(m0_queue_invariant(q));

	for (length = 0, scan = q->q_head; scan != EOQ; scan = scan->ql_next)
		++length;
	return length;
}

M0_INTERNAL struct m0_queue_link *m0_queue_get(struct m0_queue *q)
{
	struct m0_queue_link *head;

	/* invariant is checked on entry to m0_queue_is_empty() */
	if (m0_queue_is_empty(q))
		head = NULL;
	else {
		head = q->q_head;
		q->q_head = head->ql_next;
		if (q->q_head == EOQ)
			q->q_tail = EOQ;
		head->ql_next = NULL;
	}
	M0_ASSERT_EX(m0_queue_invariant(q));
	return head;

}

M0_INTERNAL void m0_queue_put(struct m0_queue *q, struct m0_queue_link *ql)
{
	/* invariant is checked on entry to m0_queue_is_empty() */
	if (m0_queue_is_empty(q))
		q->q_head = ql;
	else
		q->q_tail->ql_next = ql;
	q->q_tail = ql;
	ql->ql_next = EOQ;
	M0_ASSERT_EX(m0_queue_invariant(q));
}

M0_INTERNAL bool m0_queue_invariant(const struct m0_queue *q)
{
	struct m0_queue_link *scan;

	if ((q->q_head == EOQ) != (q->q_tail == EOQ))
		return false;
	if (q->q_head == NULL || q->q_tail == NULL)
		return false;

	for (scan = q->q_head; scan != EOQ; scan = scan->ql_next) {
		if (scan == NULL || scan == EOQ)
			return false;
	}
	if (q->q_head != EOQ && q->q_tail->ql_next != EOQ)
		return false;
	return true;
}

/** @} end of queue group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
