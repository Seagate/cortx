/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 26-Sep-2015
 */

#pragma once

#ifndef __MERO_UT_MISC_H__
#define __MERO_UT_MISC_H__

#include "lib/types.h"  /* uint64_t */
#include "lib/misc.h"   /* M0_SRC_PATH */

/**
 * @defgroup ut
 *
 * @{
 */

/**
 * Returns absolute path to given file in ut/ directory.
 * M0_UT_DIR is defined in ut/Makefile.sub.
 */
#define M0_UT_PATH(name) M0_SRC_PATH("ut/" name)

#define M0_UT_CONF_PROFILE     "<0x7000000000000001:0>"
#define M0_UT_CONF_PROFILE_BAD "<0x7000000000000000:999>" /* non-existent */
#define M0_UT_CONF_PROCESS     "<0x7200000000000001:5>"

/**
 * Random shuffles an array.
 * Uses seed parameter as the seed for RNG.
 *
 * @note It uses an UT-grade RNG.
 */
M0_INTERNAL void m0_ut_random_shuffle(uint64_t *arr,
				      uint64_t  nr,
				      uint64_t *seed);

/**
 * Gives an array with random values with the given sum.
 *
 * @pre nr > 0
 * @post m0_reduce(i, nr, 0, + arr[i]) == sum
 *
 * @note It uses an UT-grade RNG.
 */
M0_INTERNAL void m0_ut_random_arr_with_sum(uint64_t *arr,
					   uint64_t  nr,
					   uint64_t  sum,
					   uint64_t *seed);

/** @} end of ut group */
#endif /* __MERO_UT_MISC_H__ */

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
