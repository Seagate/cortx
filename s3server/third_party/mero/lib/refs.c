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

#include "lib/refs.h"

void m0_ref_init(struct m0_ref *ref, int init_num,
		void (*release) (struct m0_ref *ref))
{
	m0_atomic64_set(&ref->ref_cnt, init_num);
	ref->release = release;
	m0_mb();
}

M0_INTERNAL void m0_ref_get(struct m0_ref *ref)
{
	m0_atomic64_inc(&ref->ref_cnt);
	m0_mb();
}

M0_INTERNAL void m0_ref_put(struct m0_ref *ref)
{
	if (m0_atomic64_dec_and_test(&ref->ref_cnt))
		ref->release(ref);
}

M0_INTERNAL int64_t m0_ref_read(const struct m0_ref *ref)
{
	return m0_atomic64_get(&ref->ref_cnt);
}
