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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 07/02/2012
 */

#include "lib/errno.h"		/* ENOMEM */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/memory.h"		/* M0_ALLOC_ARR */

#include "net/test/ringbuf.h"

/**
   @defgroup NetTestRingbufInternals Ringbuf
   @ingroup NetTestInternals

   @{
 */

int m0_net_test_ringbuf_init(struct m0_net_test_ringbuf *rb, size_t size)
{
	M0_PRE(rb != NULL);
	M0_PRE(size != 0);

	rb->ntr_size = size;
	m0_atomic64_set(&rb->ntr_start, 0);
	m0_atomic64_set(&rb->ntr_end, 0);
	M0_ALLOC_ARR(rb->ntr_buf, rb->ntr_size);

	if (rb->ntr_buf != NULL)
		M0_ASSERT(m0_net_test_ringbuf_invariant(rb));

	return rb->ntr_buf == NULL ? -ENOMEM : 0;
}

void m0_net_test_ringbuf_fini(struct m0_net_test_ringbuf *rb)
{
	M0_PRE(m0_net_test_ringbuf_invariant(rb));

	m0_free(rb->ntr_buf);
	M0_SET0(rb);
}

bool m0_net_test_ringbuf_invariant(const struct m0_net_test_ringbuf *rb)
{
	int64_t start;
	int64_t end;

	if (rb == NULL || rb->ntr_buf == NULL)
		return false;

	start = m0_atomic64_get(&rb->ntr_start);
	end   = m0_atomic64_get(&rb->ntr_end);
	if (start > end)
		return false;
	if (end - start > rb->ntr_size)
		return false;
	return true;
}

void m0_net_test_ringbuf_push(struct m0_net_test_ringbuf *rb, size_t value)
{
	int64_t index;

	M0_PRE(m0_net_test_ringbuf_invariant(rb));
	index = m0_atomic64_add_return(&rb->ntr_end, 1) - 1;
	M0_ASSERT(m0_net_test_ringbuf_invariant(rb));

	rb->ntr_buf[index % rb->ntr_size] = value;
}

size_t m0_net_test_ringbuf_pop(struct m0_net_test_ringbuf *rb)
{
	int64_t index;

	M0_PRE(m0_net_test_ringbuf_invariant(rb));
	index = m0_atomic64_add_return(&rb->ntr_start, 1) - 1;
	M0_ASSERT(m0_net_test_ringbuf_invariant(rb));

	return rb->ntr_buf[index % rb->ntr_size];
}

bool m0_net_test_ringbuf_is_empty(struct m0_net_test_ringbuf *rb)
{
	M0_PRE(m0_net_test_ringbuf_invariant(rb));

	return m0_atomic64_get(&rb->ntr_end) == m0_atomic64_get(&rb->ntr_start);
}

size_t m0_net_test_ringbuf_nr(struct m0_net_test_ringbuf *rb)
{
	M0_PRE(m0_net_test_ringbuf_invariant(rb));

	return m0_atomic64_get(&rb->ntr_end) - m0_atomic64_get(&rb->ntr_start);
}

/**
   @} end of NetTestRingbufInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
