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
 * Original creation date: 04-Aug-2010
 */

#pragma once

#ifndef __MERO_LIB_USER_SPACE_MISC_H__
#define __MERO_LIB_USER_SPACE_MISC_H__

#ifndef offsetof
#define offsetof(typ,memb) __builtin_offsetof(typ, memb)
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr)-(char *)(&((type *)0)->member)))
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/**
 * size of array
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) ((sizeof (a)) / (sizeof (a)[0]))
#endif

#define M0_EXPORTED(s)

/**
 * Resolve hostname.
 */
M0_INTERNAL int m0_host_resolve(const char *name, char *buf, size_t bufsiz);

/**
 * Print performance counters: getrusage(), /proc/self/io.
 *
 * All errors (can't open file etc.) are silently ignored.
 */
M0_INTERNAL void m0_performance_counters(char *buf, size_t buf_len);

#endif /* __MERO_LIB_USER_SPACE_MISC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
