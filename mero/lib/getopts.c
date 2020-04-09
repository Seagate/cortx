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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 05/19/2012
 */

#include "lib/getopts.h"

#include "lib/misc.h"           /* strchr, m0_strtou64 */
#include "lib/errno.h"          /* EINVAL */
#include "lib/assert.h"         /* M0_CASSSERT */
#include "lib/types.h"          /* UINT64_MAX */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

const char M0_GETOPTS_DECIMAL_POINT = '.';

M0_INTERNAL int m0_bcount_get(const char *arg, m0_bcount_t *out)
{
	char		 *end = NULL;
	char		 *pos;
	static const char suffix[] = "bkmgKMG";
	int		  rc = 0;

	static const uint64_t multiplier[] = {
		1 << 9,
		1 << 10,
		1 << 20,
		1 << 30,
		1000,
		1000 * 1000,
		1000 * 1000 * 1000
	};

	M0_CASSERT(ARRAY_SIZE(suffix) - 1 == ARRAY_SIZE(multiplier));

	*out = m0_strtou64(arg, &end, 0);

	if (*end != 0 && rc == 0) {
		pos = strchr(suffix, *end);
		if (pos != NULL) {
			if (*out <= M0_BCOUNT_MAX / multiplier[pos - suffix])
				*out *= multiplier[pos - suffix];
			else
				rc = -EOVERFLOW;
		} else
			rc = -EINVAL;
	}
	return rc;
}

M0_INTERNAL int m0_time_get(const char *arg, m0_time_t *out)
{
	char	*end = NULL;
	uint64_t before;	/* before decimal point */
	uint64_t after = 0;	/* after decimal point */
	int	 rc = 0;
	uint64_t unit_mul = 1000000000;
	int	 i;
	uint64_t pow_of_10 = 1;

	static const char *unit[] = {
		"s",
		"ms",
		"us",
		"ns",
	};
	static const uint64_t multiplier[] = {
		1000000000,
		1000000,
		1000,
		1,
	};

	M0_CASSERT(ARRAY_SIZE(unit) == ARRAY_SIZE(multiplier));

	before = m0_strtou64(arg, &end, 10);
	if (*end == M0_GETOPTS_DECIMAL_POINT) {
		arg = ++end;
		after = m0_strtou64(arg, &end, 10);
		for (i = 0; i < end - arg; ++i) {
			pow_of_10 = pow_of_10 >= UINT64_MAX / 10 ? UINT64_MAX :
				    pow_of_10 * 10;
		}
	}
	if (before == UINT64_MAX || after == UINT64_MAX)
		rc = -E2BIG;

	if (rc == 0 && *end != '\0') {
		for (i = 0; i < ARRAY_SIZE(unit); ++i) {
			if (strncmp(end, unit[i], strlen(unit[i]) + 1) == 0) {
				unit_mul = multiplier[i];
				break;
			}
		}
		if (i == ARRAY_SIZE(unit))
			rc = -EINVAL;
	}
	if (rc == 0)
		*out = before * unit_mul + after * unit_mul / pow_of_10;
	return rc;
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
