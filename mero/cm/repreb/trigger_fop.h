/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 17-Aug-2016
 */

#pragma once

#ifndef __MERO_CM_REPREB_TRIGGER_FOP_H__
#define __MERO_CM_REPREB_TRIGGER_FOP_H__

/**
 * @defgroup CM
 *
 * @{
 */
#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "rpc/rpc_opcodes.h"  /* M0_RPC_OPCODES */

struct m0_fom_type_ops;
struct m0_cm_type;
struct m0_fop_type;
struct m0_xcode_type;

struct failure_data {
	uint32_t  fd_nr;
	uint64_t *fd_index;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * Simplistic implementation of repair trigger fop for testing purposes
 * only.
 */
struct trigger_fop {
	uint32_t op;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct trigger_rep_fop {
	int32_t rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_status_rep_fop {
	int32_t  ssr_rc;
	uint32_t ssr_state;
	uint64_t ssr_progress;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


/** Initialises copy machine trigger FOP type. */
M0_INTERNAL void m0_cm_trigger_fop_init(struct m0_fop_type *ft,
					enum M0_RPC_OPCODES op,
					const char *name,
					const struct m0_xcode_type *xt,
					uint64_t rpc_flags,
					struct m0_cm_type *cmt,
					const struct m0_fom_type_ops *ops);

/** Finalises copy machine trigger FOP type. */
M0_INTERNAL void m0_cm_trigger_fop_fini(struct m0_fop_type *ft);

/** @} end of CM group */
#endif /* __MERO_CM_REPREB_TRIGGER_FOP_H__ */

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
