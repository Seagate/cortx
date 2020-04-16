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

#include "lib/uuid.h"
#include "lib/atomic.h"
#include "lib/errno.h"
#include "lib/time.h"
#include "lib/string.h"      /* isxdigit */
#include "lib/misc.h"        /* ARRAY_SIZE, m0_strtou64 */

#ifdef __KERNEL__
#  include <linux/random.h>  /* get_random_uuid */
#  include <linux/uuid.h>    /* generate_random_uuid */
#else
#  include <uuid/uuid.h>     /* generate_uuid */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

M0_BASSERT(sizeof (struct m0_uint128) == sizeof (char[16]));
#ifndef __KERNEL__
M0_BASSERT(sizeof (struct m0_uint128) == sizeof (uuid_t));
#endif

M0_INTERNAL void m0_uuid_generate(struct m0_uint128 *u)
{
#ifdef __KERNEL__
	generate_random_uuid((unsigned char *)u);
#else
	uuid_generate((unsigned char *)u);
#endif
}

/**
   Convert the leading hex string of a specified length to binary.
   The length and characters are enforced.
   The maximum length is 16 hex chars.
 */
static int parse_hex(const char *str, int len, uint64_t *val)
{
	int i;
	char buf[17];

	M0_ASSERT(len < ARRAY_SIZE(buf));
	for (i = 0; i < len; ++i) {
		if (*str == '\0')
			return M0_ERR(-EINVAL);
		if (!isxdigit(*str))
			return M0_ERR(-EINVAL);
		buf[i] = *str++;
	}
	buf[i] = '\0';
	*val = m0_strtou64(buf, NULL, 16);

	return 0;
}

M0_INTERNAL int m0_uuid_parse(const char *str, struct m0_uint128 *val)
{
	uint64_t h1;
	uint64_t h2;
	uint64_t h3;
	uint64_t h4;
	uint64_t h5;

	val->u_hi = val->u_lo = 0;
	if (parse_hex(&str[0],   8, &h1) < 0 || str[8]  != '-' ||
	    parse_hex(&str[9],   4, &h2) < 0 || str[13] != '-' ||
	    parse_hex(&str[14],  4, &h3) < 0 || str[18] != '-' ||
	    parse_hex(&str[19],  4, &h4) < 0 || str[23] != '-' ||
	    parse_hex(&str[24], 12, &h5) < 0 || str[36] != '\0')
		return M0_ERR(-EINVAL);
	val->u_hi = h1 << 32 | h2 << 16 | h3;
	val->u_lo = h4 << 48 | h5;
	/* no validation of adherence to standard version formats */
	return 0;
}
M0_EXPORTED(m0_uuid_parse);

M0_INTERNAL void m0_uuid_format(const struct m0_uint128 *val,
				char *buf, size_t len)
{
	static const char *fmt = "%08x-%04x-%04x-%04x-%012lx";
	uint32_t h1;
	uint32_t h2;
	uint32_t h3;
	uint32_t h4;
	uint64_t h5;

	M0_ASSERT(len > M0_UUID_STRLEN);
	h1 = val->u_hi >> 32;
	h2 = (val->u_hi >> 16) & 0xffff;
	h3 = val->u_hi & 0xffff;
	h4 = val->u_lo >> 48;
	h5 = val->u_lo & 0xffffffffffff;
	sprintf(buf, fmt, h1, h2, h3, h4, h5);
}

M0_INTERNAL struct m0_uint128 m0_node_uuid;

/**
 * Initialize the node UUID.
 */
int m0_node_uuid_init(void)
{
	char               buf[M0_UUID_STRLEN + 1];
	struct m0_uint128 *uuid = &m0_node_uuid;
	int                rc;

	M0_SET0(uuid);
	rc = m0_node_uuid_string_get(buf);
	if (rc == 0) {
		rc = m0_uuid_parse(buf, uuid);
		if (rc == 0) {
			m0_uuid_format(uuid, buf, ARRAY_SIZE(buf));
			M0_LOG(M0_NOTICE, "Node uuid: %s", (char *)buf);
		} else
			M0_LOG(M0_ERROR, "Unable to parse node UUID string");
	} else
		M0_LOG(M0_ERROR, "Unable to fetch node UUID string");
	return M0_RC(rc);
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
