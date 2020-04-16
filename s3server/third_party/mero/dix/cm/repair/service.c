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
 * Original creation date: 15-Aug-2016
 */


/**
 * @addtogroup DIXCM
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "reqh/reqh_service.h"
#include "cm/cm.h"
#include "cm/cp.h"
#include "dix/cm/cm.h"
#include "dix/cm/service.h"
#include "rpc/rpc_opcodes.h"

M0_INTERNAL void m0_dix_cm_repair_cpx_init(void);
M0_INTERNAL void m0_dix_cm_repair_cpx_fini(void);

/** Copy machine service type operations.*/
static int dix_repair_svc_allocate(struct m0_reqh_service **service,
				   const struct m0_reqh_service_type *stype);

static const struct m0_reqh_service_type_ops dix_repair_svc_type_ops = {
	.rsto_service_allocate = dix_repair_svc_allocate
};

M0_DIX_CM_TYPE_DECLARE(dix_repair, M0_CM_DIX_REP_OPCODE,
		       &dix_repair_svc_type_ops, "M0_CST_DIX_REP",
		       M0_CST_DIX_REP);

/** Copy machine service operations.*/
static int dix_repair_svc_start(struct m0_reqh_service *service);
static void dix_repair_svc_stop(struct m0_reqh_service *service);

static const struct m0_reqh_service_ops dix_repair_svc_ops = {
	.rso_start       = dix_repair_svc_start,
	.rso_start_async = m0_reqh_service_async_start_simple,
	.rso_stop        = dix_repair_svc_stop,
	.rso_fini        = m0_dix_cm_svc_fini
};

extern const struct m0_cm_ops       dix_repair_ops;
extern const struct m0_fom_type_ops dix_repair_cp_fom_type_ops;

/**
 * Allocates and initialises REP copy machine.
 * This allocates struct m0_dix_cm and invokes m0_cm_init() to initialise
 * m0_dix_cm::rc_base.
 */
static int dix_repair_svc_allocate(struct m0_reqh_service **service,
				   const struct m0_reqh_service_type *stype)
{
	M0_ENTRY("stype: %p", stype);
	M0_PRE(service != NULL && stype != NULL);
	return M0_RC(m0_dix_cm_svc_allocate(service, stype, &dix_repair_svc_ops,
					    &dix_repair_ops, &dix_repair_dcmt));
}

static int dix_repair_svc_start(struct m0_reqh_service *service)
{
	int rc;

	rc = m0_dix_cm_svc_start(service);
	if (rc == 0) {
		m0_cm_cp_init(&dix_repair_cmt, &dix_repair_cp_fom_type_ops);
		m0_dix_cm_repair_cpx_init();
		m0_dix_repair_sw_onwire_fop_init();
		m0_dix_cm_repair_trigger_fop_init();
	}
	return M0_RC(rc);
}

static void dix_repair_svc_stop(struct m0_reqh_service *service)
{
	m0_dix_cm_svc_stop(service);
	m0_dix_cm_repair_cpx_fini();
	m0_dix_repair_sw_onwire_fop_fini();
	m0_dix_cm_repair_trigger_fop_fini();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of DIXCM group */

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
