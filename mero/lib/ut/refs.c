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
 * Original author: Alexey Lyashkov <Alexey_Lyashkov@xyratex.com>
 * Original creation date: 04/08/2010
 */

#include "ut/ut.h"
#include "lib/memory.h"
#include "lib/refs.h"

struct test_struct {
	struct m0_ref	ref;
};

static int free_done;

void test_destructor(struct m0_ref *r)
{
	struct test_struct *t;

	t = container_of(r, struct test_struct, ref);

	m0_free(t);
	free_done = 1;
}

void test_refs(void)
{
	struct test_struct *t;

	t = m0_alloc(sizeof(struct test_struct));
	M0_UT_ASSERT(t != NULL);

	free_done = 0;
	m0_ref_init(&t->ref, 1, test_destructor);

	M0_UT_ASSERT(m0_ref_read(&t->ref) == 1);
	m0_ref_get(&t->ref);
	M0_UT_ASSERT(m0_ref_read(&t->ref) == 2);
	m0_ref_put(&t->ref);
	M0_UT_ASSERT(m0_ref_read(&t->ref) == 1);
	m0_ref_put(&t->ref);

	M0_UT_ASSERT(free_done);
}
