/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 17-Sep-2012
 */
#pragma once

#ifndef __MERO_CONF_UT_COMMON_H__
#define __MERO_CONF_UT_COMMON_H__

#include "conf/confc.h"  /* m0_confc_ctx */
#include "lib/chan.h"    /* m0_clink */

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:*"

extern struct m0_conf_cache m0_conf_ut_cache;
extern struct m0_sm_group   m0_conf_ut_grp;
extern struct m0_net_xprt  *m0_conf_ut_xprt;

struct m0_conf_ut_waiter {
	struct m0_confc_ctx w_ctx;
	struct m0_clink     w_clink;
};

M0_INTERNAL void m0_conf_ut_waiter_init(struct m0_conf_ut_waiter *w,
					struct m0_confc *confc);
M0_INTERNAL void m0_conf_ut_waiter_fini(struct m0_conf_ut_waiter *w);
M0_INTERNAL int m0_conf_ut_waiter_wait(struct m0_conf_ut_waiter *w,
				       struct m0_conf_obj **result);

M0_INTERNAL int m0_conf_ut_ast_thread_init(void);
M0_INTERNAL int m0_conf_ut_ast_thread_fini(void);

M0_INTERNAL int m0_conf_ut_cache_init(void);
M0_INTERNAL int m0_conf_ut_cache_fini(void);

#ifndef __KERNEL__
M0_INTERNAL void m0_conf_ut_cache_from_file(struct m0_conf_cache *cache,
					    const char *path);
#endif

#endif /* __MERO_CONF_UT_COMMON_H__ */
