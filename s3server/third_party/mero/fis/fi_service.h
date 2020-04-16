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

#pragma once

#ifndef __MERO_FIS_FI_SERVICE_H__
#define __MERO_FIS_FI_SERVICE_H__

#include "reqh/reqh_service.h"

/**
 * @page fis-dld Fault Injection at run time DLD
 * - @subpage fis-fspec "Functional Specification"
 * - @subpage fis-lspec "Logical Specification"
 *
 * Typically used in UT suites, the fault injection functionality can be used in
 * system tests as well. In order to control fault injections a service of a
 * special M0_CST_FIS type is enabled and started in mero instance allowing it
 * accept respective FOPs. From now on any cluster participant is able to post a
 * particular FOP to the mero instance that is to control its fault injection
 * point states based on received information.
 *
 * @note Fault injection mechanisms take effect only in debug builds when
 * appropriate build configuration parameters were applied. And in release
 * builds FI appears disabled, so even with FI service up and running the posted
 * commands are going to cause no effect despite reported success code.
 */

/**
 * @page fis-fspec Fault Injection Service functions.
 * - @subpage fis-fspec-command
 *
 * FI service is registered with REQH by calling m0_fis_register(). The
 * registration occurs during mero instance initialisation. The registration
 * makes FIS service type be available globally in mero instance.
 *
 * During mero finalisation FIS service type is unregistered by calling
 * m0_fis_unregister().
 *
 * @note FIS related FOPs are not registered along with the service type (see
 * fis_start() for the details of FOPs registration).
 */

/**
 * @defgroup fis-dfspec Fault Injection Service (FIS)
 * @brief Detailed Functional Specification.
 *
 * @{
 */
#define FI_SERVICE_NAME "M0_CST_FIS"

/** Service structure to be registered with REQH. */
struct m0_reqh_fi_service {
	/** Request handler service representation */
	struct m0_reqh_service fis_svc;

	/** fis_magic == M0_FI_SERVICE_MAGIC */
	uint64_t               fis_magic;
};

M0_INTERNAL int m0_fis_register(void);
M0_INTERNAL void m0_fis_unregister(void);

/** @} fis-dfspec */
#endif /* __MERO_FIS_FI_SERVICE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
