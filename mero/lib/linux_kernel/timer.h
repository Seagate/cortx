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
 * Original author: Huang Hua <hua_huang@xyratex.com>
 *                  Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 03/04/2011
 */

#pragma once

#ifndef __MERO_LIB_LINUX_KERNEL_TIMER_H__
#define __MERO_LIB_LINUX_KERNEL_TIMER_H__

#include <linux/timer.h>	/* timer_list */

#include "lib/time.h"		/* m0_time_t */

/**
   @addtogroup timer

   <b>Linux kernel timer.</a>
   @{
 */

struct m0_timer {
	/** Timer type: M0_TIMER_SOFT or M0_TIMER_HARD. */
	enum m0_timer_type  t_type;
	/** Timer triggers this callback. */
	m0_timer_callback_t t_callback;
	/** User data. It is passed to m0_timer::t_callback(). */
	unsigned long	    t_data;
	/** Expire time in future of this timer. */
	m0_time_t	    t_expire;
	/** Timer state.  Used in state changes checking. */
	enum m0_timer_state t_state;

	/** Kernel timer. */
	struct timer_list t_timer;
};

M0_EXTERN const struct m0_timer_operations m0_timer_ops[];

/** @} end of timer group */

/* __MERO_LIB_LINUX_KERNEL_TIMER_H__ */
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
