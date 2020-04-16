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
 * Original creation date: 05/07/2010
 */

#include "ut/ut.h"
#include "lib/queue.h"
#include "lib/assert.h"

struct qt {
	struct m0_queue_link t_linkage;
	int                  t_val;
};

enum {
	NR = 255
};

void test_queue(void)
{
	int i;
	int sum0;
	int sum1;

	struct m0_queue  q;
	static struct qt t[NR]; /* static to reduce kernel stack consumption. */

	for (sum0 = i = 0; i < ARRAY_SIZE(t); ++i) {
		t[i].t_val = i;
		sum0 += i;
	};

	m0_queue_init(&q);
	M0_UT_ASSERT(m0_queue_is_empty(&q));
	M0_UT_ASSERT(m0_queue_get(&q) == NULL);
	M0_UT_ASSERT(m0_queue_length(&q) == 0);

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		m0_queue_put(&q, &t[i].t_linkage);
		M0_UT_ASSERT(!m0_queue_is_empty(&q));
		M0_UT_ASSERT(m0_queue_link_is_in(&t[i].t_linkage));
		M0_UT_ASSERT(m0_queue_length(&q) == i + 1);
	}
	M0_UT_ASSERT(m0_queue_length(&q) == ARRAY_SIZE(t));

	for (i = 0; i < ARRAY_SIZE(t); ++i)
		M0_UT_ASSERT(m0_queue_contains(&q, &t[i].t_linkage));

	for (sum1 = i = 0; i < ARRAY_SIZE(t); ++i) {
		struct m0_queue_link *ql;
		struct qt            *qt;

		ql = m0_queue_get(&q);
		M0_UT_ASSERT(ql != NULL);
		qt = container_of(ql, struct qt, t_linkage);
		M0_UT_ASSERT(&t[0] <= qt && qt < &t[NR]);
		M0_UT_ASSERT(qt->t_val == i);
		sum1 += qt->t_val;
	}
	M0_UT_ASSERT(sum0 == sum1);
	M0_UT_ASSERT(m0_queue_get(&q) == NULL);
	M0_UT_ASSERT(m0_queue_is_empty(&q));
	M0_UT_ASSERT(m0_queue_length(&q) == 0);
	for (i = 0; i < ARRAY_SIZE(t); ++i)
		M0_UT_ASSERT(!m0_queue_link_is_in(&t[i].t_linkage));

	m0_queue_fini(&q);
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
