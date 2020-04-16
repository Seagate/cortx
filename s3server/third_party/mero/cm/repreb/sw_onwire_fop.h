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
 * Original creation date: 06/07/2013
 */

#pragma once

#ifndef __MERO_CM_REPREB_SW_ONWIRE_FOP_H__
#define __MERO_CM_REPREB_SW_ONWIRE_FOP_H__

#include "xcode/xcode_attr.h"
#include "rpc/rpc_opcodes.h"

#include "cm/cm.h"
#include "cm/sw.h"
#include "cm/sw_xc.h"

/**
   @defgroup XXX Repair/re-balance sliding window
   @ingroup XXX

   @{
 */

struct m0_cm_repreb_sw {
	struct m0_cm_sw_onwire swo_base;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Initialises sliding window FOP type. */
M0_INTERNAL
void m0_cm_repreb_sw_onwire_fop_init(struct m0_fop_type *ft,
				     const struct m0_fom_type_ops *fomt_ops,
				     enum M0_RPC_OPCODES op,
				     const char *name,
				     const struct m0_xcode_type *xt,
				     uint64_t rpc_flags,
				     struct m0_cm_type *cmt);

/** Finalises sliding window FOP type. */
M0_INTERNAL void m0_cm_repreb_sw_onwire_fop_fini(struct m0_fop_type *ft);

/**
 * Allocates sliding window FOP data and initialises sliding window FOP.
 *
 * @see m0_cm_sw_onwire_init()
 */
M0_INTERNAL int
m0_cm_repreb_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop_type *ft,
				 struct m0_fop *fop,
				 void (*fop_release)(struct m0_ref *),
				 uint64_t proxy_id, const char *local_ep,
				 const struct m0_cm_sw *sw,
				 const struct m0_cm_sw *out_interval);

extern struct m0_fop_type m0_cm_repreb_sw_fopt;

/** @} XXX */

#endif /* __MERO_CM_REPREB_SW_ONWIRE_FOP_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
