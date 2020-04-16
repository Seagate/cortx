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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 01-Sep-2012
 */

#pragma once

#ifndef __MERO_LIB_STRING_H__
#define __MERO_LIB_STRING_H__

/*
 * Define standard string manipulation functions (strcat, strlen, strcmp, &c.)
 * together with sprintf(3) and snprintf(3).
 * Also pick up support for strtoul(3) and variants, and ctype macros.
 */

#define m0_streq(a, b) (strcmp((a), (b)) == 0)
#define m0_strcaseeq(a, b) (strcasecmp((a), (b)) == 0)

#ifndef __KERNEL__
# include <ctype.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

#define m0_strdup(s) strdup((s))
#define m0_asprintf(s, fmt, ...)                          \
	({                                                \
		int __nr;                                 \
		char **__s = (s);                         \
		__nr = asprintf(__s, (fmt), __VA_ARGS__); \
		if (__nr <= 0)                            \
			*__s = NULL;                      \
	})

#else
# include <linux/ctype.h>
# include <linux/kernel.h>
# include <linux/string.h>

#define m0_strdup(s) kstrdup((s), GFP_KERNEL)
#define m0_asprintf(s, fmt, ...) \
	({ *(s) = kasprintf(GFP_ATOMIC, (fmt), __VA_ARGS__); })

static inline char *strerror(int errnum)
{
	return "strerror() is not supported in kernel";
}
#endif /* __KERNEL__ */

#include "lib/types.h"

struct m0_fop_str {
	uint32_t s_len;
	uint8_t *s_buf;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * Converts m0_bcount_t number into a reduced string representation, calculating
 * a magnitude and representing it as standard suffix like "Ki", "Mi", "Gi" etc.
 * So, for example, 87654321 becomes "83 Mi".
 */
const char *m0_bcount_with_suffix(char *buf, size_t size, m0_bcount_t c);

M0_INTERNAL void m0_strings_free(const char **arr);

M0_INTERNAL const char **m0_strings_dup(const char **src);

M0_INTERNAL char *
m0_vsnprintf(char *buf, size_t buflen, const char *format, ...)
	__attribute__((format (printf, 3, 4)));

/** Returns true iff `str' starts with the specified `prefix'. */
M0_INTERNAL bool m0_startswith(const char *prefix, const char *str);

#endif /* __MERO_LIB_STRING_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
