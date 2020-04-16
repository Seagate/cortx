/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#pragma once

#ifndef __MERO_FDMI_FDMI_SERVICE_H__
#define __MERO_FDMI_FDMI_SERVICE_H__

#include "reqh/reqh_service.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/plugin_dock_internal.h"
#include "fdmi/filterc.h"
/**
 * @addtogroup fdmi_main
 * @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
 * @see @ref reqh
 *
 * @{
 *
 * FDMI service runs as a part of Mero instance. FDMI service stores context
 * data for both FDMI source dock and FDMI plugin dock. FDMI service is
 * initialized and started on Mero instance start up, FDMI Source dock and FDMI
 * plugin dock are managed separately, and specific API is provided for this
 * purposes.
 *
 */

struct m0_reqh_fdmi_svc_params {
	/* FilterC operations can be patched by UT */
	const struct m0_filterc_ops *filterc_ops;
};

/**
 * Structure contains generic service structure and
 * service specific information.
 */
struct m0_reqh_fdmi_service {
	/** Generic reqh service object */
	struct m0_reqh_service  rfdms_gen;

	/**
	 * @todo Temporary field to indicate
	 * whether source dock was successfully started. (phase 2)
	 */
	bool                    rfdms_src_dock_inited;

	/** Magic to check fdmi service object */
	uint64_t                rfdms_magic;
};

M0_INTERNAL void m0_fdms_unregister(void);
M0_INTERNAL int m0_fdms_register(void);

/** @} end of addtogroup fdmi_main */

#endif /* __MERO_FDMI_FDMI_SERVICE_H__ */
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
