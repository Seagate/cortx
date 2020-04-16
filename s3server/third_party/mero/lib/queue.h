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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 05/07/2011
 */

#pragma once

#ifndef __MERO_LIB_QUEUE_H__
#define __MERO_LIB_QUEUE_H__

#include "lib/types.h"

/**
   @defgroup queue Queue

   FIFO queue. Should be pretty self-explanatory.

   @{
 */

struct m0_queue_link;

/**
   A queue of elements.
 */
struct m0_queue {
	/** Oldest element in the queue (first to be returned). */
	struct m0_queue_link *q_head;
	/** Youngest (last added) element in the queue. */
	struct m0_queue_link *q_tail;
};

/**
   An element in a queue.
 */
struct m0_queue_link {
	struct m0_queue_link *ql_next;
};

/**
   Static queue initializer. Assign this to a variable of type struct m0_queue
   to initialize empty queue.
 */
extern const struct m0_queue M0_QUEUE_INIT;

M0_INTERNAL void m0_queue_init(struct m0_queue *q);
M0_INTERNAL void m0_queue_fini(struct m0_queue *q);
M0_INTERNAL bool m0_queue_is_empty(const struct m0_queue *q);

M0_INTERNAL void m0_queue_link_init(struct m0_queue_link *ql);
M0_INTERNAL void m0_queue_link_fini(struct m0_queue_link *ql);
M0_INTERNAL bool m0_queue_link_is_in(const struct m0_queue_link *ql);
M0_INTERNAL bool m0_queue_contains(const struct m0_queue *q,
				   const struct m0_queue_link *ql);
M0_INTERNAL size_t m0_queue_length(const struct m0_queue *q);

/**
   Returns queue head or NULL if queue is empty.
 */
M0_INTERNAL struct m0_queue_link *m0_queue_get(struct m0_queue *q);
M0_INTERNAL void m0_queue_put(struct m0_queue *q, struct m0_queue_link *ql);

M0_INTERNAL bool m0_queue_invariant(const struct m0_queue *q);

/** @} end of queue group */
#endif /* __MERO_LIB_QUEUE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
