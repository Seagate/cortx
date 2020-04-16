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
 * Original creation date: 08/04/2010
 */

#pragma once

#ifndef __MERO_LIB_LINUX_KERNEL_TYPES_H__
#define __MERO_LIB_LINUX_KERNEL_TYPES_H__

#include <linux/types.h>
#include <linux/kernel.h>  /* INT_MAX */

#include "lib/assert.h"

M0_BASSERT(((uint32_t)0) - 1 == ~(uint32_t)0);

#define UINT8_MAX  ((uint8_t)0xff)
#define INT8_MIN   ((int8_t)0x80)
#define INT8_MAX   ((int8_t)0x7f)
#define UINT16_MAX ((uint16_t)0xffff)
#define INT16_MIN  ((int16_t)0x8000)
#define INT16_MAX  ((int16_t)0x7fff)
#define UINT32_MAX ((uint32_t)0xffffffff)
#define INT32_MIN  ((int32_t)0x80000000)
#define INT32_MAX  ((int32_t)0x7fffffff)
#define UINT64_MAX ((uint64_t)0xffffffffffffffff)
#define INT64_MIN  ((int64_t)0x8000000000000000)
#define INT64_MAX  ((int64_t)0x7fffffffffffffff)

M0_BASSERT(INT8_MIN < 0);
M0_BASSERT(INT8_MAX > 0);
M0_BASSERT(INT16_MIN < 0);
M0_BASSERT(INT16_MAX > 0);
M0_BASSERT(INT32_MIN < 0);
M0_BASSERT(INT32_MAX > 0);
M0_BASSERT(INT64_MIN < 0);
M0_BASSERT(INT64_MAX > 0);

#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIi64 "lli"
#define PRIo64 "llo"
#define PRIx64 "llx"
#define SCNx64 "llx"
#define SCNi64 "lli"

#define PRId32 "d"
#define PRIu32 "u"
#define PRIi32 "i"
#define PRIo32 "o"
#define PRIx32 "x"
#define SCNx32 "x"
#define SCNi32 "i"

/* __MERO_LIB_LINUX_KERNEL_TYPES_H__ */
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
