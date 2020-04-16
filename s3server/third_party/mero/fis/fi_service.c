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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 06-Feb-2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"           /* M0_ALLOC_PTR */
#include "fis/fi_command_fops.h"
#include "fis/fi_service.h"

/**
 * @page fis-lspec Fault Injection Service.
 *
 * - @subpage fis-lspec-command
 * - @subpage fis-lspec-command-fops
 * - @subpage fis-lspec-command-fom
 *
 * <b>Fault Injection Service: objectives and use</b>
 *
 * Fault Injection Service is intended to provide fault injection functionality
 * at run time. To be enabled the service must appear in configuration database
 * with service type M0_CST_FIS under the process which is the subject for fault
 * injection. Additionally, command line parameter '-j' must be specified for
 * the process to unlock the service start. This "double-lock" mechanism is
 * intended to protect cluster from accidental FIS start.
 *
 * @see mero/st/m0d-fatal
 *
 * - @ref fis-dlspec "Detailed Logical Specification"
 *
 */

/**
 * @defgroup fis-dlspec FIS Internals
 *
 * @{
 */

static int fis_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype);

static int fis_start(struct m0_reqh_service *service)
{
	M0_ENTRY();
	/**
	 * @note FI command fops become acceptable only in case fault injection
	 * service appears in configuration database and started normal way.
	 */
	m0_fi_command_fop_init();
	return M0_RC(0);
}

static void fis_stop(struct m0_reqh_service *service)
{
	M0_ENTRY();
	m0_fi_command_fop_fini();
	M0_LEAVE();
}

static void fis_fini(struct m0_reqh_service *service)
{
	/* Nothing to finalise here. */
}

static const struct m0_reqh_service_type_ops fis_type_ops = {
	.rsto_service_allocate = fis_allocate
};

static const struct m0_reqh_service_ops fis_ops = {
	.rso_start = fis_start,
	.rso_stop  = fis_stop,
	.rso_fini  = fis_fini,
};

struct m0_reqh_service_type m0_fis_type = {
	.rst_name     = FI_SERVICE_NAME,
	.rst_ops      = &fis_type_ops,
	.rst_level    = M0_RPC_SVC_LEVEL,
	.rst_typecode = M0_CST_FIS,
};

static const struct m0_bob_type fis_bob = {
	.bt_name = FI_SERVICE_NAME,
	.bt_magix_offset = offsetof(struct m0_reqh_fi_service, fis_magic),
	.bt_magix = M0_FI_SERVICE_MAGIC,
	.bt_check = NULL
};

M0_BOB_DEFINE(M0_INTERNAL, &fis_bob, m0_reqh_fi_service);

/**
 * Allocates @ref m0_reqh_fi_service instance and initialises it as BOB. Exposes
 * standard @ref m0_reqh_service interface outside for registration with REQH.
 */
static int fis_allocate(struct m0_reqh_service **service,
			const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_fi_service *fis;

	M0_ENTRY();
	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(fis);
	if (fis == NULL)
		return M0_ERR(-ENOMEM);
	m0_reqh_fi_service_bob_init(fis);
	*service = &fis->fis_svc;
	(*service)->rs_ops = &fis_ops;
	return M0_RC(0);
}

M0_INTERNAL int m0_fis_register(void)
{
	M0_ENTRY();
	m0_reqh_service_type_register(&m0_fis_type);
	return M0_RC(0);
}

M0_INTERNAL void m0_fis_unregister(void)
{
	M0_ENTRY();
	m0_reqh_service_type_unregister(&m0_fis_type);
	M0_LEAVE();
}

/** @} end fis-dlspec */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
