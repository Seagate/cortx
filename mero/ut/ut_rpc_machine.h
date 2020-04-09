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
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 18-Apr-2013
 */


#pragma once

#ifndef __MERO_UT_UT_RPC_MACHINE_H__
#define __MERO_UT_UT_RPC_MACHINE_H__

#include "cob/cob.h"
#include "net/lnet/lnet.h"
#include "net/buffer_pool.h"
#include "mdstore/mdstore.h"
#include "reqh/reqh.h"
#include "rpc/rpc.h"
#include "rpc/rpc_machine.h"
#include "be/ut/helper.h"

struct m0_ut_rpc_mach_ctx {
	const char                *rmc_ep_addr;
	struct m0_rpc_machine      rmc_rpc;
	struct m0_be_ut_backend    rmc_ut_be;
	struct m0_be_ut_seg        rmc_ut_seg;
	struct m0_cob_domain_id    rmc_cob_id;
	struct m0_mdstore          rmc_mdstore;
	struct m0_cob_domain       rmc_cob_dom;
	struct m0_net_domain       rmc_net_dom;
	struct m0_net_buffer_pool  rmc_bufpool;
	struct m0_net_xprt        *rmc_xprt;
	struct m0_reqh             rmc_reqh;
};

M0_INTERNAL void m0_ut_rpc_mach_init_and_add(struct m0_ut_rpc_mach_ctx *ctx);

M0_INTERNAL void m0_ut_rpc_mach_fini(struct m0_ut_rpc_mach_ctx *ctx);

#endif /* __MERO_UT_UT_RPC_MACHINE_H__ */


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
