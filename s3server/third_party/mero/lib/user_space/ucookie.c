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
 * Original author: Nachiket Sahasrabudhe <Nachiket_Sahasrabudhe@xyratex.com>
 * Original creation date: 19/07/2012
 */

#include <setjmp.h>        /* setjmp() and longjmp() */

#include "lib/thread.h"
#include "lib/misc.h"      /* M0_SET0 */
#include "lib/errno.h"     /* errno */
#include "lib/assert.h"    /* m0_panic */

/**
   @addtogroup cookie
   @{
 */

static const struct m0_panic_ctx signal_panic = {
	.pc_expr   = "fatal signal delivered",
	.pc_func   = "unknown",
	.pc_file   = "unknown",
	.pc_lineno = 0,
	.pc_fmt    = "signo: %i"
};

/**
 * Signal handler for SIGSEGV.
 */
static void sigsegv(int sig)
{
	struct m0_thread_tls *tls = m0_thread_tls();
	jmp_buf              *buf = tls == NULL ? NULL : tls->tls_arch.tat_jmp;

	if (buf == NULL)
		m0_panic(&signal_panic, sig);
	else
		longjmp(*buf, 1);
}

/**
 * Checks the validity of an address by dereferencing the same. Occurrence of
 * an error in case of an invalid address gets handled by the
 * function sigsegv().
 */
M0_INTERNAL bool m0_arch_addr_is_sane(const void *addr)
{
	jmp_buf           buf;
	jmp_buf         **tls = &m0_thread_tls()->tls_arch.tat_jmp;
	volatile uint64_t dummy M0_UNUSED;
	int               ret;
	bool              result = false;

	*tls = &buf;
	ret = setjmp(buf);
	if (ret == 0) {
		dummy = *(uint64_t *)addr;
		result = true;
	}
	*tls = NULL;

	return result;
}

/** Sets the signal handler for SIGSEGV to sigsegv() function. */
M0_INTERNAL int m0_arch_cookie_global_init(void)
{
	int              ret;
	struct sigaction sa_sigsegv = {
		.sa_handler = sigsegv,
		.sa_flags   = SA_NODEFER
	};

	ret = sigemptyset(&sa_sigsegv.sa_mask) ?:
		sigaction(SIGSEGV, &sa_sigsegv, NULL);
	if (ret != 0) {
		M0_ASSERT(ret == -1);
		return -errno;
	}
	return 0;
}

/** Sets the signal handler for SIGSEGV to the default handler. */
M0_INTERNAL void m0_arch_cookie_global_fini(void)
{
	int              ret;
	struct sigaction sa_sigsegv = { .sa_handler = SIG_DFL };

	ret = sigemptyset(&sa_sigsegv.sa_mask) ?:
		sigaction(SIGSEGV, &sa_sigsegv, NULL);
	M0_ASSERT(ret == 0);
}

/** @} end of cookie group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
