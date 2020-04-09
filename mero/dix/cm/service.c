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
 * Original creation date: 19-Aug-2016
 */


/**
 * @addtogroup dixcm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "dix/cm/cm.h"
#include "lib/trace.h"
#include "lib/memory.h"
#include "mero/setup.h"
#include "dix/req.h"

M0_INTERNAL int m0_dix_cm_module_init(void)
{
	return m0_dix_cm_type_register() ?:
		m0_dix_sm_conf_init();
}

M0_INTERNAL void m0_dix_cm_module_fini(void)
{
	m0_dix_sm_conf_fini();
	m0_dix_cm_type_deregister();
}

M0_INTERNAL int
m0_dix_cm_svc_allocate(struct m0_reqh_service **service,
		       const struct m0_reqh_service_type *stype,
		       const struct m0_reqh_service_ops *svc_ops,
		       const struct m0_cm_ops *cm_ops,
		       struct m0_dix_cm_type  *dcmt)
{
	struct m0_dix_cm        *dix_cm;
	struct m0_cm            *cm;
	int                      rc;

	M0_ENTRY("stype: %p", stype);
	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(dix_cm);
	if (dix_cm == NULL)
		return M0_ERR(-ENOMEM);

	dix_cm->dcm_type = dcmt;
	cm = &dix_cm->dcm_base;
	*service = &cm->cm_service;
	(*service)->rs_ops = svc_ops;

	rc = m0_cm_init(cm, container_of(stype, struct m0_cm_type, ct_stype),
			cm_ops);
	if (rc != 0)
		m0_free(dix_cm);
	M0_LOG(M0_DEBUG, "dix_cm: %p service: %p", dix_cm, *service);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_cm_svc_start(struct m0_reqh_service *service)
{
	struct m0_cm *cm;
	int           rc;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = M0_AMB(cm, service, cm_service);
	rc = m0_cm_setup(cm);
	if (rc != 0)
		return M0_ERR(rc);
	return M0_RC(rc);
}

M0_INTERNAL void m0_dix_cm_svc_stop(struct m0_reqh_service *service)
{
	struct m0_cm *cm;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = container_of(service, struct m0_cm, cm_service);
	/*
	 * Finalise the copy machine as the copy machine as the service is
	 * stopped.
	 */
	m0_cm_fini(cm);

	M0_LEAVE();
}

M0_INTERNAL void m0_dix_cm_svc_fini(struct m0_reqh_service *service)
{
	struct m0_cm     *cm;
	struct m0_dix_cm *dix_cm;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = container_of(service, struct m0_cm, cm_service);
	dix_cm = cm2dix(cm);
	m0_free(dix_cm);

	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dixcm group */

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
