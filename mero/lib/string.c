/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dima Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 21-Aug-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include "lib/types.h"        /* m0_bcount_t */
#include "lib/memory.h"       /* M0_ALLOC_ARR */
#include "lib/misc.h"         /* ARRAY_SIZE */
#include "lib/finject.h"      /* M0_FI_ENABLED */
#include "lib/string.h"

const char *m0_bcount_with_suffix(char *buf, size_t size, m0_bcount_t c)
{
	static const char *suffix[] = {
		"", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi"
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(suffix) - 1 && c >= 1024; ++i, c /= 1024)
		;
	snprintf(buf, size, "%3" PRId64 " %s", c, suffix[i]);
	return buf;
}

M0_INTERNAL void m0_strings_free(const char **arr)
{
	if (arr != NULL) {
		const char **p;
		for (p = arr; *p != NULL; ++p)
			m0_free((void *)*p);
		m0_free0(&arr);
	}
}

M0_INTERNAL const char **m0_strings_dup(const char **src)
{
	int     i;
	int     n;
	char  **dest;

	for (n = 0; src[n] != NULL; ++n)
		; /* counting */

	M0_ALLOC_ARR(dest, n + 1);
	if (dest == NULL)
		return NULL;

	for (i = 0; i < n; ++i) {
		if (!M0_FI_ENABLED("strdup_failed")) {
			dest[i] = m0_strdup(src[i]);
		}

		if (dest[i] == NULL) {
			m0_strings_free((const char **)dest);
			return NULL;
		}
	}
	dest[n] = NULL; /* end of list */

	return (const char **)dest;
}

M0_INTERNAL char *
m0_vsnprintf(char *buf, size_t buflen, const char *format, ...)
{
	va_list ap;
	int     n;

	va_start(ap, format);
	n = vsnprintf(buf, buflen, format, ap);
	va_end(ap);
	M0_ASSERT(n > 0);
	if ((size_t)n >= buflen)
		M0_LOG(M0_WARN, "Output was truncated");
	return buf;
}

M0_INTERNAL bool m0_startswith(const char *prefix, const char *str)
{
	size_t len_pre = strlen(prefix);

	return len_pre <= strlen(str) && strncmp(prefix, str, len_pre) == 0;
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
