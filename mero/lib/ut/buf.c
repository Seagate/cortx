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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 08/20/2012
 */

#include "ut/ut.h"
#include "lib/buf.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/finject.h"
#include "lib/string.h"  /* m0_streq, m0_strings_free */
#include "lib/errno.h"   /* ENOENT */

static void bufs_test(void);
static void buf_cmp_test(void);

static bool bit_is_set(int bits, int index)
{
	return (bool)(bits & (1 << index));
}

void m0_ut_lib_buf_test(void)
{
	struct m0_buf copy = M0_BUF_INIT0;
	static int    d0[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	static char  *d1 = "1234567890";
	static char  *d2 = "123";
	char         *s;
	bool          equal;
	int           k;
	int           j;
	int           rc;
	struct {
		int           equality_mask; /* equality to self is implied */
		struct m0_buf buf;
	} test[] = {
		[0] = { (1 << 1), M0_BUF_INIT(strlen(d1), d1) },
		[1] = { (1 << 0), M0_BUF_INITS(d1) },
		[2] = { (1 << 4), M0_BUF_INITS(d2) },
		[3] = { (1 << 6) | (1 << 7), M0_BUF_INIT(sizeof(d0), d0) },
		[4] = { (1 << 2), M0_BUF_INIT(strlen(d2), d1) },
		[5] = { 0, M0_BUF_INIT(sizeof(d0) - 1, d0) },

		/* [6] and [7] are placeholders and will be overwriten with
		 * m0_buf_init() */
		[6] = { (1 << 3) | (1 << 7), M0_BUF_INIT0 },
		[7] = { (1 << 3) | (1 << 6), M0_BUF_INIT0 },
	};

#ifdef ENABLE_FAULT_INJECTION
	struct m0_buf inj_copy = M0_BUF_INIT0;
	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_buf_copy(&inj_copy, &test[0].buf);
	M0_UT_ASSERT(rc == -ENOMEM);
#endif

	m0_buf_init(&test[6].buf, d0, sizeof(d0));
	m0_buf_init(&test[7].buf, d0, sizeof(d0));

	for (k = 0; k < ARRAY_SIZE(test); ++k) {
		rc = m0_buf_copy(&copy, &test[k].buf);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_buf_eq(&copy, &test[k].buf));
		m0_buf_free(&copy);
	}

	for (k = 0; k < ARRAY_SIZE(test); ++k) {
		for (j = 0; j < ARRAY_SIZE(test); ++j) {
			if (j == k)
				continue;
			equal = m0_buf_eq(&test[j].buf, &test[k].buf);
			M0_UT_ASSERT(equal == bit_is_set(test[j].equality_mask,
							 k));
		}
	}

	copy = M0_BUF_INIT(0, (void *)13);
	M0_UT_ASSERT(m0_buf_eq(&M0_BUF_INIT0, &copy));
	M0_UT_ASSERT(m0_buf_streq(&M0_BUF_INIT0, ""));
	M0_UT_ASSERT(m0_buf_streq(&copy, ""));
	s = m0_buf_strdup(&copy);
	M0_UT_ASSERT(s != NULL && *s == '\0');
	m0_free(s);

	bufs_test();
	buf_cmp_test();
}
M0_EXPORTED(m0_ut_lib_buf_test);

static void bufs_test(void)
{
	struct m0_bufs bufs;
	const char    *strs[] = { "", "1", "two", NULL };
	const char   **strs_new = NULL;
	int            rc;

	rc = m0_bufs_from_strings(&bufs, strs);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(bufs.ab_count == 3);
	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(strs) - 1,
			       m0_buf_streq(&bufs.ab_elems[i], strs[i])));

	rc = m0_bufs_to_strings(&strs_new, &bufs);
	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(strs) - 1,
			       m0_streq(strs[i], strs_new[i]) &&
			       strs[i] != strs_new[i]));
	M0_UT_ASSERT(strs_new[ARRAY_SIZE(strs) - 1] == NULL);

	M0_UT_ASSERT(m0_bufs_streq(&bufs, strs));
	++*(char *)bufs.ab_elems[2].b_addr;
	M0_UT_ASSERT(!m0_bufs_streq(&bufs, strs));
	--*(char *)bufs.ab_elems[2].b_addr;
	M0_UT_ASSERT(m0_bufs_streq(&bufs, strs));

	m0_strings_free(strs_new);
	m0_bufs_free(&bufs);
	M0_UT_ASSERT(bufs.ab_count == 0 && bufs.ab_elems == NULL);
}

static void buf_cmp_test(void)
{
	struct m0_buf buf;

	M0_UT_ASSERT(m0_buf_cmp(&M0_BUF_INIT0, &M0_BUF_INIT0) == 0);
	M0_UT_ASSERT(m0_buf_cmp(&M0_BUF_INITS("equal"),
				&M0_BUF_INITS("equal")) == 0);
	M0_UT_ASSERT(m0_buf_cmp(&M0_BUF_INITS("1"),
				&M0_BUF_INITS("2")) < 0);
	M0_UT_ASSERT(m0_buf_cmp(&M0_BUF_INITS("prefix"),
				&M0_BUF_INITS("prefixsuffix")) < 0);
	M0_UT_ASSERT(m0_buf_cmp(&M0_BUF_INITS("prefixsuffix"),
				&M0_BUF_INITS("prefix")) > 0);

	buf = M0_BUF_INITS("single");
	M0_UT_ASSERT(m0_buf_cmp(&buf, &buf) == 0);
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
