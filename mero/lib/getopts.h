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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 05/19/2012
 */

#pragma once

#ifndef __MERO_LIB_GETOPTS_H__
#define __MERO_LIB_GETOPTS_H__

#include "lib/types.h"	/* m0_bcount_t */
#include "lib/time.h"	/* m0_time_t */

#ifndef __KERNEL__
#include "lib/user_space/getopts.h"
#endif

/**
   @addtogroup getopts
   @{
 */
extern const char M0_GETOPTS_DECIMAL_POINT;

/**
   Convert numerical argument, followed by a optional multiplier suffix, to an
   uint64_t value.  The numerical argument is expected in the format that
   strtoull(..., 0) can parse. The multiplier suffix should be a char
   from "bkmgKMG" string. The char matches factor which will be
   multiplied by numerical part of argument.

   Suffix char matches:
   - @b b = 512
   - @b k = 1024
   - @b m = 1024 * 1024
   - @b g = 1024 * 1024 * 1024
   - @b K = 1000
   - @b M = 1000 * 1000
   - @b G = 1000 * 1000 * 1000
 */
M0_INTERNAL int m0_bcount_get(const char *arg, m0_bcount_t *out);

/**
   Convert numerical argument, followed by a optional multiplier suffix, to an
   m0_time_t value.  The numerical argument is expected in the format
   "[integer].[integer]" or just "integer", where [integer] is optional integer
   value in format that strtoull(..., 10) can parse, and at least one integer
   should be present in the numerical argument. The multiplier suffix matches
   unit of time and should be a string from the following list.

   Suffix string matches:
   - empty string = a second
   - @b s = a second
   - @b ms = millisecond = 1/1000 of a second
   - @b us = microsecond = 1/1000'000 of a second
   - @b ns = nanosecond  = 1/1000'000'000 of a second

   @note M0_GETOPTS_DECIMAL_POINT is used as decimal point in numerical
   argument to this function.
 */
M0_INTERNAL int m0_time_get(const char *arg, m0_time_t * out);

/** @} end of getopts group */

/* __MERO_LIB_GETOPTS_H__ */
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
