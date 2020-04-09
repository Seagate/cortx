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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 05/06/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/misc.h"            /* M0_CAT */
#include "lib/trace_internal.h"  /* m0_trace_file_path_get */
#include "mero/version.h"        /* m0_build_info */

/**
   @addtogroup assert

   @{
*/

/**
 * Panic function.
 */
void m0_panic(const struct m0_panic_ctx *ctx, ...)
{
	static int repanic = 0;
	va_list    ap;
	const struct m0_build_info *bi = m0_build_info_get();

	if (repanic++ == 0) {
		M0_LOG(M0_FATAL, "panic: %s at %s() (%s:%i) %s [git: %s] %s",
		       ctx->pc_expr, ctx->pc_func, ctx->pc_file, ctx->pc_lineno,
		       m0_failed_condition ?: "", bi->bi_git_describe,
		       m0_trace_file_path_get());
		va_start(ap, ctx);
		m0_arch_panic(ctx, ap);
		va_end(ap);
	} else {
		/* The death of God left the angels in a strange position. */
		while (true) {
			;
		}
	}
}
M0_EXPORTED(m0_panic);

M0_INTERNAL void m0_panic_only(const struct m0_panic_ctx *ctx, ...)
{
	va_list ap;

	va_start(ap, ctx);
	m0_arch_panic(ctx, ap);
	va_end(ap);
}

void m0_backtrace(void)
{
	m0_arch_backtrace();
}
M0_EXPORTED(m0_backtrace);

M0_INTERNAL void m0__assertion_hook(void)
{
}
M0_EXPORTED(m0__assertion_hook);

/** @} end of assert group */
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
