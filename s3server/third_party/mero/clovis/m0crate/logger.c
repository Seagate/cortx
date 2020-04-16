/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Ivan Alekhin <ivan.alekhin@seagate.com>
 * Original creation date: 30-May-2017
 */

#include "clovis/m0crate/logger.h"
#include <string.h>

enum cr_log_level log_level = CLL_INFO;
enum cr_log_level prev_level = CLL_DEBUG;

struct {
	const char *name;
} level_str[] = {
        [CLL_ERROR]	= {"error"},
        [CLL_WARN]	= {"warning"},
        [CLL_INFO]	= {"info"},
        [CLL_TRACE]	= {"trace"},
        [CLL_DEBUG]	= {"dbg"},
};

void cr_log(enum cr_log_level lev, const char *fmt, ...)
{
        va_list va;
	va_start(va, fmt);
	cr_vlog(lev, fmt, va);
	va_end(va);

}

void cr_vlog(enum cr_log_level lev, const char *fmt, va_list args)
{
	if (lev == CLL_SAME) {
		if (prev_level <= log_level) {
			(void) vfprintf(stderr, fmt, args);
		}
	} else {
		if (lev <= log_level) {
			(void) fprintf(stderr, "%s: ", level_str[lev].name);
			(void) vfprintf(stderr, fmt, args);
		}
		prev_level = lev;
	}
}

void cr_set_debug_level(enum cr_log_level level)
{
	log_level = level;
	prev_level = level;
}

void cr_log_ex(enum cr_log_level lev,
	       const char *pre,
	       const char *post,
	       const char *fmt, ...)
{
        va_list va;
	cr_log(lev, "%s", pre);
	va_start(va, fmt);
	cr_vlog(CLL_SAME, fmt, va);
	va_end(va);
	cr_log(CLL_SAME, "%s", post);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
