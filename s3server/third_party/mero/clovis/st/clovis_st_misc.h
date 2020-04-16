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
 * Original creation date: 20-Oct-2014
 */

#pragma once

#ifndef __MERO_CLOVIS_ST_MISC_H__
#define __MERO_CLOVIS_ST_MISC_H__

#ifdef __KERNEL__

# include <linux/kernel.h>
# include <linux/ctype.h>
# include <linux/string.h>
# include <linux/types.h>

#define str_dup(s) kstrdup((s), GFP_KERNEL)

/*
 * syslog(8) will trim leading spaces of each kernel log line, so we need to use
 * a non-space character at the beginning of each line to preserve formatting
 */

#define LOG_PREFIX "."

#else

# include <ctype.h>
# include <stdio.h>
# include <stdint.h>
# include <stdlib.h>
# include <string.h>

#define str_dup(s) strdup((s))

#define LOG_PREFIX

#endif /* __KERNEL__ */

#define str_eq(a, b) (strcmp((a), (b)) == 0)

enum {
	TIME_ONE_SECOND = 1000000000ULL,
	TIME_ONE_MSEC   = TIME_ONE_SECOND / 1000
};

/**
 * Helper functions
 */
pid_t get_tid(void);
void console_printf(const char *fmt, ...);
uint32_t generate_random(uint32_t max);

uint64_t time_now(void);
uint64_t time_from_now(uint64_t secs, uint64_t ns);
uint64_t time_seconds(const uint64_t time);
uint64_t time_nanoseconds(const uint64_t time);

void *mem_alloc(size_t size);
void  mem_free(void *p);

#define MEM_ALLOC_ARR(arr, nr)  ((arr) = mem_alloc((nr) * sizeof ((arr)[0])))
#define MEM_ALLOC_PTR(arr) MEM_ALLOC_ARR(arr, 1)

#endif /* __MERO_CLOVIS_ST_MISC_H__ */
