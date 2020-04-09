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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 31-Jan-2013
 */

/**
 * @addtogroup assert
 *
 * @{
 */

#include <linux/kernel.h>         /* pr_emerg */
#include <linux/bug.h>            /* BUG */
#include <linux/string.h>         /* strcmp */
#include <linux/delay.h>          /* mdelay */
#include <linux/kgdb.h>           /* kgdb_breakpoint() */

#include "lib/assert.h"           /* m0_failed_condition */
#include "mero/version.h"         /* m0_build_info */

static int m0_panic_delay_msec = 110;

void m0_arch_backtrace()
{
	dump_stack();
}

M0_INTERNAL void m0_arch_panic(const struct m0_panic_ctx *c, va_list ap)
{
	const struct m0_build_info *bi = m0_build_info_get();

	pr_emerg("Mero panic: %s at %s() %s:%i (last failed: %s) [git: %s]\n",
		 c->pc_expr, c->pc_func, c->pc_file, c->pc_lineno,
		 m0_failed_condition ?: "none", bi->bi_git_describe);
	if (c->pc_fmt != NULL) {
		pr_emerg("Mero panic reason: ");
		vprintk(c->pc_fmt, ap);
		pr_emerg("\n");
	}
	dump_stack();
	/*
	 * Delay BUG() call in in order to allow m0traced process to fetch all
	 * trace records from kernel buffer, before system-wide Kernel Panic is
	 * triggered.
	 */
	mdelay(m0_panic_delay_msec);
	BUG();
}

M0_INTERNAL void m0_debugger_invoke(void)
{
#ifdef CONFIG_KGDB
	kgdb_breakpoint();
#endif
}

/** @} end of assert group */

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
