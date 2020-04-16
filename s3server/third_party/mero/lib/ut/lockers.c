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
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 9-Jan-2013
 */

#include "lib/lockers.h"
#include "ut/ut.h"

M0_LOCKERS_DECLARE(M0_INTERNAL, bank, 2);

struct bank {
	struct bank_lockers vault;
};

M0_LOCKERS_DEFINE(M0_INTERNAL, bank, vault);

static void bank_init(struct bank *bank)
{
	bank_lockers_init(bank);
}

static void bank_fini(struct bank *bank)
{
	bank_lockers_fini(bank);
}

void test_lockers(void)
{
	int         key;
	int         key1;
	char       *valuable = "Gold";
	char       *asset;
	struct bank federal;
	int         i;

	bank_init(&federal);

	key = bank_lockers_allot();
	M0_UT_ASSERT(key == 0);

	key1 = bank_lockers_allot();
	M0_UT_ASSERT(key != key1);

	M0_UT_ASSERT(bank_lockers_is_empty(&federal, key));
	bank_lockers_set(&federal, key, valuable);
	M0_UT_ASSERT(!bank_lockers_is_empty(&federal, key));

	asset = bank_lockers_get(&federal, key);
	M0_UT_ASSERT(asset == valuable);

	bank_lockers_clear(&federal, key);
	M0_UT_ASSERT(bank_lockers_is_empty(&federal, key));

	bank_fini(&federal);
	for (i = 0; i < 1000; ++i) {
		bank_lockers_free(key1);
		key1 = bank_lockers_allot();
		M0_UT_ASSERT(key != key1);
	}
}
M0_EXPORTED(test_lockers);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
