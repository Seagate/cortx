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
 * Original creation date: 07/05/2012
 */

#include "ut/ut.h"		/* M0_UT_ASSERT */

#include "net/test/ringbuf.h"

enum {
	NET_TEST_RB_SIZE    = 0x1000,
	NET_TEST_RB_LOOP_NR = 0x10,
};

static void ringbuf_push_pop(struct m0_net_test_ringbuf *rb, size_t nr)
{
	size_t i;
	size_t value;
	size_t len;

	M0_PRE(rb != NULL);

	for (i = 0; i < nr; ++i) {
		m0_net_test_ringbuf_push(rb, i);
		len = m0_net_test_ringbuf_nr(rb);
		M0_UT_ASSERT(len == i + 1);
	}
	for (i = 0; i < nr; ++i) {
		value = m0_net_test_ringbuf_pop(rb);
		M0_UT_ASSERT(value == i);
		len = m0_net_test_ringbuf_nr(rb);
		M0_UT_ASSERT(len == nr - i - 1);
	}
}

void m0_net_test_ringbuf_ut(void)
{
	struct m0_net_test_ringbuf rb;
	int			   rc;
	int			   i;
	size_t			   value;
	size_t			   nr;

	/* init */
	rc = m0_net_test_ringbuf_init(&rb, NET_TEST_RB_SIZE);
	M0_UT_ASSERT(rc == 0);
	nr = m0_net_test_ringbuf_nr(&rb);
	M0_UT_ASSERT(nr == 0);
	/* test #1: single value push, single value pop */
	m0_net_test_ringbuf_push(&rb, 42);
	nr = m0_net_test_ringbuf_nr(&rb);
	M0_UT_ASSERT(nr == 1);
	value = m0_net_test_ringbuf_pop(&rb);
	M0_UT_ASSERT(value == 42);
	nr = m0_net_test_ringbuf_nr(&rb);
	M0_UT_ASSERT(nr == 0);
	/* test #2: multiple values push, multiple values pop */
	ringbuf_push_pop(&rb, NET_TEST_RB_SIZE);
	/*
	 * test #3: push and pop (NET_TEST_RB_SIZE - 1) items
	 * NET_TEST_RB_LOOP_NR times
	 */
	for (i = 0; i < NET_TEST_RB_LOOP_NR; ++i)
		ringbuf_push_pop(&rb, NET_TEST_RB_SIZE - 1);
	/* fini */
	m0_net_test_ringbuf_fini(&rb);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
