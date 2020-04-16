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

#include "lib/memory.h"
#include "lib/string.h"

#include "fop/fop.h"
#include "rpc/rpc.h"
#include "rpc/rpc_opcodes.h"

#include "cm/cm.h"
#include "cm/repreb/sw_onwire_fop.h"
#include "cm/repreb/sw_onwire_fop_xc.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM
#include "lib/trace.h"

/**
   @addtogroup XXX

   @{
 */

extern const struct m0_sm_conf m0_cm_repreb_sw_onwire_conf;

M0_INTERNAL
void m0_cm_repreb_sw_onwire_fop_init(struct m0_fop_type *ft,
				     const struct m0_fom_type_ops *fomt_ops,
				     enum M0_RPC_OPCODES op,
				     const char *name,
				     const struct m0_xcode_type *xt,
				     uint64_t rpc_flags,
				     struct m0_cm_type *cmt)
{
	M0_FOP_TYPE_INIT(ft,
			 .name      = name,
			 .opcode    = op,
			 .xt        = xt,
			 .rpc_flags = rpc_flags,
			 .fom_ops   = fomt_ops,
			 .sm        = &m0_cm_repreb_sw_onwire_conf,
			 .svc_type  = &cmt->ct_stype);
}

M0_INTERNAL void m0_cm_repreb_sw_onwire_fop_fini(struct m0_fop_type *ft)
{
	m0_fop_type_fini(ft);
}

M0_INTERNAL int
m0_cm_repreb_sw_onwire_fop_setup(struct m0_cm *cm, struct m0_fop_type *ft,
				 struct m0_fop *fop,
				 void (*fop_release)(struct m0_ref *),
				 uint64_t proxy_id, const char *local_ep,
				 const struct m0_cm_sw *sw,
				 const struct m0_cm_sw *out_interval)
{
	struct m0_cm_sw_onwire *swo_fop;
	int                     rc = 0;

	M0_PRE(cm != NULL && sw != NULL && local_ep != NULL);

	m0_fop_init(fop, ft, NULL, fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc  != 0) {
		m0_fop_fini(fop);
		return M0_RC(rc);
	}
	swo_fop = m0_fop_data(fop);
	rc = m0_cm_sw_onwire_init(cm, swo_fop, proxy_id, local_ep, sw,
				  out_interval);

	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} XXX */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
