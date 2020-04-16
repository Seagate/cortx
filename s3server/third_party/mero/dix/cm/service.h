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

#pragma once

#ifndef __MERO_DIX_CM_SERVICE_H__
#define __MERO_DIX_CM_SERVICE_H__

/**
 * @defgroup dixcm
 *
 * @{
 */

/**
 * Allocates DIX copy machine (service context is embedded into copy machine
 * context), initialises embedded base copy machine, sets up embedded service
 * context and return its pointer.
 *
 * @param[out] service DIX CM service.
 * @param[in]  stype   DIX CM service type.
 * @param[in]  svc_ops DIX CM service operation list.
 * @param[in]  cm_ops  Copy machine operation list.
 * @param[in]  dcmt    DIX copy machine type.
 *
 * @ret 0 on succes or negative error code.
 */
M0_INTERNAL int
m0_dix_cm_svc_allocate(struct m0_reqh_service **service,
		       const struct m0_reqh_service_type *stype,
		       const struct m0_reqh_service_ops *svc_ops,
		       const struct m0_cm_ops *cm_ops,
		       struct m0_dix_cm_type  *dcmt);

/**
 * Sets up a DIX copy machine served by @service.
 *
 * @param service DIX CM service.
 *
 * @ret 0 on succes or negative error code.
 */
M0_INTERNAL int m0_dix_cm_svc_start(struct m0_reqh_service *service);

/**
 * Finalises DIX copy machine and embedded base copy machine.
 *
 * @param service DIX CM service.
 */
M0_INTERNAL void m0_dix_cm_svc_stop(struct m0_reqh_service *service);

/** @} end of dixcm group */
#endif /* __MERO_DIX_CM_SERVICE_H__ */

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
