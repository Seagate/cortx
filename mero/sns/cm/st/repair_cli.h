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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 04/11/2013
 */

#pragma once

#ifndef __MERO_SNS_CM_ST_REPAIR_CLI_H__
#define __MERO_SNS_CM_ST_REPAIR_CLI_H__

#include "rpc/rpc.h"

enum {
	MAX_RPCS_IN_FLIGHT = 10,
	MAX_FILES_NR       = 10,
	MAX_SERVERS        = 1024
};

struct rpc_ctx {
	struct m0_rpc_conn    ctx_conn;
	struct m0_rpc_session ctx_session;
	int                   ctx_rc;
};

M0_INTERNAL int  repair_client_init(void);
M0_INTERNAL void repair_client_fini(void);
M0_INTERNAL int repair_rpc_ctx_init(struct rpc_ctx *ctx, const char *sep);
M0_INTERNAL void repair_rpc_ctx_fini(struct rpc_ctx *ctx);
M0_INTERNAL int repair_rpc_post(struct m0_fop *fop,
				struct m0_rpc_session *session,
				const struct m0_rpc_item_ops *ri_ops,
				m0_time_t  deadline);

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
