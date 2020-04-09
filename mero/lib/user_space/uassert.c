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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#ifdef HAVE_BACKTRACE
#  include <execinfo.h>		/* backtrace */
#endif
#include <sys/types.h>		/* waitpid */
#include <sys/wait.h>		/* waitpid */
#include <linux/limits.h>	/* PATH_MAX */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* abort */
#include <unistd.h>		/* fork, sleep */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/trace_internal.h" /* m0_trace_file_path_get */
#include "lib/misc.h"		/* ARRAY_SIZE */
#include "mero/version.h"	/* m0_build_info */

/**
 * @addtogroup assert
 *
 * User space m0_arch_panic() implementation.
 * @{
 */

enum { BACKTRACE_DEPTH_MAX = 256 };

M0_EXTERN char *m0_debugger_args[4];

#ifdef ENABLE_DETAILED_BACKTRACE
static void arch_backtrace_detailed(void)
{
	const char *gdb_path = "/usr/bin/gdb";
	const char *gdb_cmd = "thread apply all bt";
	pid_t	    pid;
	char	    pid_str[32];
	char	    path_str[PATH_MAX];
	ssize_t	    path_str_len;

	fprintf(stderr, "Trying to recover detailed backtrace...\n");
	fflush(stderr);

	snprintf(pid_str, sizeof(pid_str), "%ld", (long) getpid());
	path_str_len = readlink("/proc/self/exe",
				path_str, sizeof(path_str) - 1);
	if (path_str_len == -1)
		return;
	path_str[path_str_len] = '\0';

	pid = fork();
	if (pid > 0) {
		/* parent */
		waitpid(pid, NULL, 0);
	} else if (pid == 0) {
		/* child */
		dup2(STDERR_FILENO, STDOUT_FILENO);
		execl(gdb_path, gdb_path, "-batch", "-nx", "-ex", gdb_cmd,
		      path_str, pid_str, NULL);
		/* if execl() failed */
		_exit(EXIT_SUCCESS);
	} else {
		/* fork() failed, nothing to do */
	}
}
#endif

void m0_arch_backtrace(void)
{
#ifdef HAVE_BACKTRACE
	void	   *trace[BACKTRACE_DEPTH_MAX];
	int	    nr;

	nr = backtrace(trace, ARRAY_SIZE(trace));
	backtrace_symbols_fd(trace, nr, STDERR_FILENO);
#endif
#ifdef ENABLE_DETAILED_BACKTRACE
	arch_backtrace_detailed();
#endif
}

/**
   Simple user space panic function: issue diagnostics to the stderr, flush the
   stream, optionally print the backtrace and abort(3) the program.

   Stack back-trace printing uses GNU extensions to the libc, declared in
   <execinfo.h> header (checked for by ./configure). Object files should be
   compiled with -rdynamic for this to work in the presence of dynamic linking.
 */
M0_INTERNAL void m0_arch_panic(const struct m0_panic_ctx *c, va_list ap)
{
	const struct m0_build_info *bi = m0_build_info_get();

	fprintf(stderr,
		"Mero panic: %s at %s() %s:%i (errno: %i) (last failed: %s)"
		" [git: %s] pid: %u  %s\n",
		c->pc_expr, c->pc_func, c->pc_file, c->pc_lineno, errno,
		m0_failed_condition ?: "none", bi->bi_git_describe,
		(unsigned)getpid(), m0_trace_file_path_get());
	if (c->pc_fmt != NULL) {
		fprintf(stderr, "Mero panic reason: ");
		vfprintf(stderr, c->pc_fmt, ap);
		fprintf(stderr, "\n");
	}
	fflush(stderr);

	m0_arch_backtrace();
	m0_debugger_invoke();

	abort();
}

M0_INTERNAL void m0_debugger_invoke(void)
{
	const char *debugger = m0_debugger_args[0];

	if (debugger == NULL) {
		; /* nothing */
	} else if (!strcmp(debugger, "wait")) {
		fprintf(stderr, "Mero pid %u waits for debugger.\n", getpid());
		fflush(stderr);
		while (1)
			sleep(1);
	} else {
		int rc;

		rc = fork();
		if (rc > 0) {
			/* parent */
			volatile bool stop = true;

			while (stop) {
				;
			}
		} else if (rc == 0) {
			/* child */
			rc = execvp(m0_debugger_args[0], m0_debugger_args);
			M0_IMPOSSIBLE("execvp() failed with rc=%d errno=%d",
				      rc, errno);
		}
	}
}

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
