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
 * Original author: Nathan Rutman <Nathan_Rutman@xyratex.com>
 *		    Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 12/08/2010
 */

#pragma once

#ifndef __MERO_LIB_USER_SPACE_TIME_H__
#define __MERO_LIB_USER_SPACE_TIME_H__

#include <time.h>

/**
 * Clock sources for m0_time_now(). @see m0_time_now()
 * @note Be sure to change m0_semaphore and m0_timer implementations
 * after changing CLOCK_SOURCES list.
 * @see man 3p clock_gettime
 * @see timer_posix_set(), m0_semaphore_timeddown(), m0_time_now(),
 *	m0_time_to_realtime().
 */
enum CLOCK_SOURCES {
	M0_CLOCK_SOURCE_REALTIME = CLOCK_REALTIME,
	M0_CLOCK_SOURCE_MONOTONIC = CLOCK_MONOTONIC,
	/** @note POSIX timers on Linux don't support this clock source */
	M0_CLOCK_SOURCE_MONOTONIC_RAW = CLOCK_MONOTONIC_RAW,
	/** gettimeofday(). All others clock sources use clock_gettime() */
	M0_CLOCK_SOURCE_GTOD,
	/** CLOCK_REALTIME + CLOCK_MONOTONIC combination.
	 *  @see m0_utime_init() */
	M0_CLOCK_SOURCE_REALTIME_MONOTONIC,
};

/* __MERO_LIB_USER_SPACE_TIME_H__ */
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
