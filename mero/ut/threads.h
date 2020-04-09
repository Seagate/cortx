/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 4-Mar-2014
 */

#pragma once

#ifndef __MERO_UT_THREADS_H__
#define __MERO_UT_THREADS_H__

#include "lib/types.h"	/* size_t */

/**
 * @defgroup ut
 *
 * Multithreaded UT helpers.
 *
 * @{
 */

struct m0_thread;

struct m0_ut_threads_descr {
	void		 (*utd_thread_func)(void *param);
	struct m0_thread  *utd_thread;
	int		   utd_thread_nr;
};

#define M0_UT_THREADS_DEFINE(name, thread_func)				\
static struct m0_ut_threads_descr ut_threads_descr_##name = {		\
	.utd_thread_func = (void (*)(void *))thread_func,		\
};

#define M0_UT_THREADS_START(name, thread_nr, param_array)		\
	m0_ut_threads_start(&ut_threads_descr_##name, thread_nr,	\
			    param_array, sizeof(param_array[0]))

#define M0_UT_THREADS_STOP(name)					\
	m0_ut_threads_stop(&ut_threads_descr_##name)

M0_INTERNAL void m0_ut_threads_start(struct m0_ut_threads_descr *descr,
				     int			 thread_nr,
				     void			*param_array,
				     size_t			 param_size);
M0_INTERNAL void m0_ut_threads_stop(struct m0_ut_threads_descr *descr);


/** @} end of ut group */
#endif /* __MERO_UT_THREADS_H__ */

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
