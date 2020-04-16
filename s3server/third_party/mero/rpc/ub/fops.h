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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 07-Mar-2013
 */

#pragma once

#ifndef __MERO_RPC_UB_FOPS_H__
#define __MERO_RPC_UB_FOPS_H__

#include "xcode/xcode.h"
#include "lib/buf_xc.h"

/** RPC UB request. */
struct ub_req {
	uint64_t      uq_seqn; /**< Sequential number. */
	struct m0_buf uq_data; /**< Data buffer. */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** RPC UB response. */
struct ub_resp {
	int32_t       ur_rc;
	uint64_t      ur_seqn; /**< Sequential number. */
	struct m0_buf ur_data; /**< Data buffer. */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

extern struct m0_fop_type m0_rpc_ub_req_fopt;
extern struct m0_fop_type m0_rpc_ub_resp_fopt;

M0_INTERNAL void m0_rpc_ub_fops_init(void);
M0_INTERNAL void m0_rpc_ub_fops_fini(void);

#endif /* __MERO_RPC_UB_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
