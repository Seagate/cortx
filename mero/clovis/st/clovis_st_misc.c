/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 29-Oct-2014
 */

/**
 * Helper functions for Clovis ST in both user space and kernel
 */
#ifdef __KERNEL__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/random.h>

#else
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>		/* syscall (gettid)*/
#include <sys/syscall.h>	/* syscall (gettid) */

#endif

#include <stdbool.h>

#include "clovis/st/clovis_st_assert.h"
#include "clovis/st/clovis_st_misc.h"
#include "lib/memory.h"

/*******************************************************************
 *                              Time                               *
 *******************************************************************/

#ifdef __KERNEL__

uint64_t time_now(void)
{
	struct timespec ts;

	getnstimeofday(&ts);
	return ts.tv_sec * TIME_ONE_SECOND + ts.tv_nsec;
}

#else

uint64_t time_now(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * TIME_ONE_SECOND + tv.tv_usec * 1000;
}

#endif /* end of time_now*/

uint64_t time_seconds(const uint64_t time)
{
	return time / TIME_ONE_SECOND;
}

uint64_t time_nanoseconds(const uint64_t time)
{
        return time % TIME_ONE_SECOND;
}

uint64_t time_from_now(uint64_t secs, uint64_t ns)
{
	return time_now() + secs * TIME_ONE_SECOND + ns;
}


/*******************************************************************
 *                             Memory                              *
 *******************************************************************/

void *mem_alloc(size_t size)
{
	void *p;

	p = m0_alloc(size);
	if (p != NULL)
		memset(p, 0, size);

	if (clovis_st_is_cleaner_up() == true)
		clovis_st_mark_ptr(p);

	return p;
}

void mem_free(void *p)
{
	if (clovis_st_is_cleaner_up() == true)
		clovis_st_unmark_ptr(p);

	m0_free(p);
}

/*******************************************************************
 *                             Misc                                *
 *******************************************************************/

#ifdef __KERNEL__
pid_t get_tid(void)
{
	return current->pid;
}

void console_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}

uint32_t generate_random(uint32_t max)
{
	uint32_t randv;

	get_random_bytes(&randv, sizeof(randv));
	return randv % max;
}

#else

pid_t get_tid(void)
{
	return syscall(SYS_gettid);
}

void console_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

uint32_t generate_random(uint32_t max)
{
	return (uint32_t)random()%max;
}

#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
