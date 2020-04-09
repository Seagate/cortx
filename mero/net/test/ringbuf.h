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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 07/02/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_RINGBUF_H__
#define __MERO_NET_TEST_RINGBUF_H__

#include "lib/types.h"	/* size_t */
#include "lib/atomic.h"	/* m0_atomic64 */

/**
   @defgroup NetTestRingbufDFS Ringbuf
   @ingroup NetTestDFS

   @{
 */

/**
   Circular FIFO buffer with size_t elements.
   @note m0_net_test_ringbuf.ntr_start and m0_net_test_ringbuf.ntr_end are
   absolute indices.
 */
struct m0_net_test_ringbuf {
	size_t		    ntr_size;	/**< Maximum number of elements */
	size_t		   *ntr_buf;	/**< Ringbuf array */
	struct m0_atomic64  ntr_start;	/**< Start pointer */
	struct m0_atomic64  ntr_end;	/**< End pointer */
};

/**
   Initialize ring buffer.
   @param rb ring buffer
   @param size maximum number of elements.
 */
int m0_net_test_ringbuf_init(struct m0_net_test_ringbuf *rb, size_t size);

/**
   Finalize ring buffer.
   @pre m0_net_test_ringbuf_invariant(rb)
 */
void m0_net_test_ringbuf_fini(struct m0_net_test_ringbuf *rb);
/** Ring buffer invariant. */
bool m0_net_test_ringbuf_invariant(const struct m0_net_test_ringbuf *rb);

/**
   Push item to the ring buffer.
   @pre m0_net_test_ringbuf_invariant(rb)
   @post m0_net_test_ringbuf_invariant(rb)
 */
void m0_net_test_ringbuf_push(struct m0_net_test_ringbuf *rb, size_t value);

/**
   Pop item from the ring buffer.
   @pre m0_net_test_ringbuf_invariant(rb)
   @post m0_net_test_ringbuf_invariant(rb)
 */
size_t m0_net_test_ringbuf_pop(struct m0_net_test_ringbuf *rb);

/**
   Is ring buffer empty?
   Useful with MPSC/SPSC access pattern.
   @pre m0_net_test_ringbuf_invariant(rb)
 */
bool m0_net_test_ringbuf_is_empty(struct m0_net_test_ringbuf *rb);

/**
   Get current number of elements in the ring buffer.
   @note This function is not thread-safe.
   @pre m0_net_test_ringbuf_invariant(rb)
 */
size_t m0_net_test_ringbuf_nr(struct m0_net_test_ringbuf *rb);

/**
   @} end of NetTestRingbufDFS group
 */

#endif /*  __MERO_NET_TEST_RINGBUF_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
