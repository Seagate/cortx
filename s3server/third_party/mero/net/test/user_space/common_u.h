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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 09/15/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_USER_SPACE_COMMON_U_H__
#define __MERO_NET_TEST_USER_SPACE_COMMON_U_H__

#include <stdio.h>		/* printf */

#include "lib/time.h"		/* m0_time_t */

/**
   @defgroup NetTestUCommonDFS Common user-space routines
   @ingroup NetTestDFS

   @see @ref net-test

   @{
 */

extern bool m0_net_test_u_printf_verbose;

#define M0_VERBOSEFLAGARG M0_FLAGARG('v', "Verbose output",		      \
				     &m0_net_test_u_printf_verbose)
#define M0_IFLISTARG(pflag) M0_FLAGARG('l', "List available LNET interfaces", \
				       pflag)

char *m0_net_test_u_str_copy(const char *str);
void m0_net_test_u_str_free(char *str);
/** perror */
void m0_net_test_u_print_error(const char *s, int code);
void m0_net_test_u_print_s(const char *fmt, const char *str);
void m0_net_test_u_print_time(char *name, m0_time_t time);
void m0_net_test_u_lnet_info(void);
void m0_net_test_u_print_bsize(double bsize);

int m0_net_test_u_printf(const char *fmt, ...);
int m0_net_test_u_printf_v(const char *fmt, ...);

/**
   @} end of NetTestUCommonDFS group
 */

#endif /* __MERO_NET_TEST_USER_SPACE_COMMON_U_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
