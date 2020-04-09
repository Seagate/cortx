/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Ivan Alekhin <ivan.alekhin@seagate.com>
 * Original creation date: 30-May-2017
 */

#pragma once

#ifndef __MERO_CLOVIS_M0CRATE_LOGGER_H__
#define __MERO_CLOVIS_M0CRATE_LOGGER_H__

#include <stdarg.h>       /* va_list */
#include <stdio.h>        /* vfprintf(), stderr */

/**
 * @defgroup crate_logger
 *
 * @{
 */

enum cr_log_level {
	CLL_ERROR	= 0,
	CLL_WARN	= 1,
	CLL_INFO	= 2,
	CLL_TRACE	= 3,
	CLL_DEBUG	= 4,
	CLL_SAME	= -1,
};
void cr_log(enum cr_log_level lev, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void cr_log_ex(enum cr_log_level lev,
	       const char *pre,
	       const char *post,
	       const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void cr_vlog(enum cr_log_level lev, const char *fmt, va_list args);
void cr_set_debug_level(enum cr_log_level level);

#define crlog(level, ...) cr_log_ex(level, LOG_PREFIX, "\n", __VA_ARGS__)

/** @} end of crate_logger group */
#endif /* __MERO_CLOVIS_M0CRATE_LOGGER_H__ */

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
