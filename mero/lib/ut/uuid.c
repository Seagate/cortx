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
 * Original author: Carl Braganza <carl_braganza@xyratex.com>
 * Original creation date: 10/04/2012
 */

#include "lib/string.h"
#include "lib/uuid.h"
#include "lib/errno.h"     /* ENOENT */
#include "ut/ut.h"

static char *nil_uuid = "00000000-0000-0000-0000-000000000000"; /* nil UUID */
static char *uuid1    = "abcdef01-2345-6789-abcd-ef0123456789"; /* lc */
static char *uuid2    = "98765432-10AB-CDEF-FEDC-BA0123456789"; /* uc */
static char *bad1     = "bad1";
static char *bad_uuids_len_ok[] = { /* len ok in all cases */
	"abcdef0101-2345-6789-abcd-0123456789", /* field lengths wrong */
	"0123456-1234-12345-1234-123456789abc", /* field lengths wrong */
	"X176543-10ab-0Xdef-fedc-0xa123456789", /* invalid chr @front */
	"9876543M-10ab-cdef-fedc-ba0123456789", /* invalid chr in str */
	"987650x4-10ab-cdef-fedc-ba0123456789", /* 0x in str  */
	"0x765432-10ab-0Xde-fedc-0xa123456789", /* 0x and 0X @front */
};
static char *bad_uuids_short[] = { /* short in one field */
	"abcdef1-2345-6789-abcd-ef0123456789",
	"abcdef01-234-6789-abcd-ef0123456789",
	"abcdef01-2345-678-abcd-ef0123456789",
	"abcdef01-2345-6789-abc-ef0123456789",
	"abcdef01-2345-6789-abcd-ef012345678",
};
static char *bad_uuids_long[] = { /* long in one field */
	"abcdef012-2345-6789-abcd-ef0123456789",
	"abcdef01-23456-6789-abcd-ef0123456789",
	"abcdef01-2345-6789a-abcd-ef0123456789",
	"abcdef01-2345-6789-abcde-ef0123456789",
	"abcdef01-2345-6789-abcd-ef0123456789a",
};

static bool test_identity_op(const char *str)
{
	struct m0_uint128 u1;
	struct m0_uint128 u2;
	char buf[M0_UUID_STRLEN+1];
	int rc;

	rc = m0_uuid_parse(str, &u1);
	if (rc != 0)
		return false;
	m0_uuid_format(&u1, buf, ARRAY_SIZE(buf));
	rc = m0_uuid_parse(buf, &u2);
	if (rc != 0)
		return false;
	return (u1.u_hi == u2.u_hi) && (u1.u_lo == u2.u_lo);
}

struct m0_uint128 uuid[1000];
void m0_test_lib_uuid(void)
{
	struct m0_uint128 u;
	int rc;
	int i;
	int j;

	rc = m0_uuid_parse(nil_uuid, &u);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(u.u_hi == 0);
	M0_UT_ASSERT(u.u_lo == 0);
	M0_UT_ASSERT(test_identity_op(nil_uuid));

	rc = m0_uuid_parse(uuid1, &u);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(u.u_hi == 0xabcdef0123456789);
	M0_UT_ASSERT(u.u_lo == 0xabcdef0123456789);
	M0_UT_ASSERT(test_identity_op(uuid1));

	rc = m0_uuid_parse(uuid2, &u);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(u.u_hi == 0x9876543210abcdef);
	M0_UT_ASSERT(u.u_lo == 0xfedcba0123456789);
	M0_UT_ASSERT(test_identity_op(uuid2));

	rc = m0_uuid_parse(bad1, &u);
	M0_UT_ASSERT(rc == -EINVAL);

	for (i = 0; i < ARRAY_SIZE(bad_uuids_len_ok); ++i) {
		M0_UT_ASSERT(strlen(bad_uuids_len_ok[i]) == M0_UUID_STRLEN);
		rc = m0_uuid_parse(bad_uuids_len_ok[i], &u);
		M0_UT_ASSERT(rc == -EINVAL);
	}

	for (i = 0; i < ARRAY_SIZE(bad_uuids_short); ++i) {
		M0_UT_ASSERT(strlen(bad_uuids_short[i]) < M0_UUID_STRLEN);
		rc = m0_uuid_parse(bad_uuids_short[i], &u);
		M0_UT_ASSERT(rc == -EINVAL);
	}

	for (i = 0; i < ARRAY_SIZE(bad_uuids_long); ++i) {
		M0_UT_ASSERT(strlen(bad_uuids_long[i]) > M0_UUID_STRLEN);
		rc = m0_uuid_parse(bad_uuids_long[i], &u);
		M0_UT_ASSERT(rc == -EINVAL);
	}

	for (i = 0; i < ARRAY_SIZE(uuid); ++i)
		m0_uuid_generate(&uuid[i]);
	for (i = 0; i < ARRAY_SIZE(uuid); ++i)
		for (j = i + 1; j < ARRAY_SIZE(uuid); ++j)
			M0_UT_ASSERT(m0_uint128_cmp(&uuid[i], &uuid[j]) != 0);
}
M0_EXPORTED(m0_test_lib_uuid);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
