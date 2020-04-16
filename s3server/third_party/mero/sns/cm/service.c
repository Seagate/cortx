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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 16/04/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/trace.h"
#include "lib/misc.h"
#include "lib/finject.h"

#include "reqh/reqh_service.h"
#include "sns/cm/cm.h"
#include "mero/setup.h"
#include "sns/cm/sns_cp_onwire.h"

/**
  @addtogroup SNSCMSVC

  @{
*/

M0_INTERNAL int
m0_sns_cm_svc_allocate(struct m0_reqh_service **service,
		       const struct m0_reqh_service_type *stype,
		       const struct m0_reqh_service_ops *svc_ops,
		       const struct m0_cm_ops *cm_ops)
{
	struct m0_sns_cm *sns_cm;
	struct m0_cm     *cm;
	int               rc;

	M0_ENTRY("stype: %p", stype);
	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(sns_cm);
	if (sns_cm == NULL)
		return M0_RC(-ENOMEM);

	cm = &sns_cm->sc_base;
	*service = &cm->cm_service;
	(*service)->rs_ops = svc_ops;

	rc = m0_cm_init(cm, container_of(stype, struct m0_cm_type, ct_stype),
			cm_ops);
	if (rc != 0)
		m0_free(sns_cm);

	M0_LOG(M0_DEBUG, "sns_cm: %p service: %p", sns_cm, *service);
	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_cm_svc_start(struct m0_reqh_service *service)
{
	struct m0_cm                *cm;
	int                          rc;
	struct cs_endpoint_and_xprt *ep;
	struct m0_reqh_context      *rctx;
	struct m0_mero              *mero;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = container_of(service, struct m0_cm, cm_service);
	rc = m0_cm_setup(cm);
	if (rc != 0)
		return M0_RC(rc);

	/* The following shows how to retrieve ioservice endpoints list.
	 * Copy machine can establish connections to all ioservices,
	 * and build a map for "cob_id" -> "sesssion of ioservice" with
	 * the same algorithm on m0t1fs client.
	 */
	rctx = service->rs_reqh_ctx;
	mero = rctx->rc_mero;
	m0_tl_for(cs_eps, &mero->cc_ios_eps, ep) {
		M0_ASSERT(cs_endpoint_and_xprt_bob_check(ep));
		M0_LOG(M0_DEBUG, "pool_width=%d, "
				 "ioservice xprt:endpoints: %s:%s",
				 mero->cc_pool_width,
				 ep->ex_xprt, ep->ex_endpoint);
	} m0_tl_endfor;

	M0_LEAVE();
	return M0_RC(rc);
}

M0_INTERNAL void m0_sns_cm_svc_stop(struct m0_reqh_service *service)
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

M0_INTERNAL void m0_sns_cm_svc_fini(struct m0_reqh_service *service)
{
	struct m0_cm     *cm;
	struct m0_sns_cm *sns_cm;

	M0_ENTRY("service: %p", service);
	M0_PRE(service != NULL);

	cm = container_of(service, struct m0_cm, cm_service);
	sns_cm = cm2sns(cm);
	m0_free(sns_cm);

	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} SNSCMSVC */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
