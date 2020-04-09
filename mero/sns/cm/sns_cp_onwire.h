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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 02/15/2013
 */

#pragma once

#ifndef __MERO_SNS_CM_CP_ONWIRE_H__
#define __MERO_SNS_CM_CP_ONWIRE_H__

#include "rpc/rpc_opcodes.h"
#include "stob/stob.h"
#include "stob/stob_xc.h"
#include "cm/cp_onwire.h"
#include "cm/cp_onwire_xc.h"

struct m0_cm_type;

/** SNS specific onwire copy packet structure. */
struct m0_sns_cpx {
        /** Base copy packet fields. */
        struct m0_cpx             scx_cp;
        /**
         * Index vectors representing the extent information for the
         * data represented by the copy packet.
         */
        struct m0_io_indexvec_seq scx_ivecs;

        /** Destination stob id. */
        struct m0_stob_id         scx_stob_id;

	uint64_t                  scx_failed_idx;

	/** Copy packet fom phase before sending it onwire. */
	uint32_t                  scx_phase;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** SNS specific onwire copy packet reply structure. */
struct m0_sns_cpx_reply {
	int32_t             scr_rc;
        /** Base copy packet reply fields. */
        struct m0_cpx_reply scr_cp_rep;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL void m0_sns_cpx_init(struct m0_fop_type *ft,
				 const struct m0_fom_type_ops *fomt_ops,
				 enum M0_RPC_OPCODES op,
				 const char *name,
				 const struct m0_xcode_type *xt,
				 uint64_t rpc_flags, struct m0_cm_type *cmt);

M0_INTERNAL void m0_sns_cpx_fini(struct m0_fop_type *ft);

#endif /* __MERO_SNS_CM_CP_ONWIRE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
