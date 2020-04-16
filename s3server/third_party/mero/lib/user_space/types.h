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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 08/04/2010
 */

#pragma once

#ifndef __MERO_LIB_USER_SPACE_TYPES_H__
#define __MERO_LIB_USER_SPACE_TYPES_H__

/* See 7.18.2 Limits of specified-width integer types in C99 */
/* This is needed because gccxml compiles it in C++ mode. */
#ifdef __cplusplus
#ifndef __STDC_LIMIT_MACROS
#  define  __STDC_LIMIT_MACROS 1
#endif
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <inttypes.h>   /* PRId64, PRIu64, ... */
#include <limits.h>     /* INT_MAX */

/* __MERO_LIB_USER_SPACE_TYPES_H__ */
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
