/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 23/07/2012
 */

#pragma once

#ifndef __MERO_MDSERVICE_MD_SERVICE_H__
#define __MERO_MDSERVICE_MD_SERVICE_H__

#include "reqh/reqh_service.h"   /* m0_reqh_service */

/**
 * @defgroup mdservice MD Service Operations
 * @see @ref reqh
 *
 * MD Service defines service operation vector -
 * - MD Service operation @ref m0_mds_start()<br>
 *   Initiate buffer_pool and register I/O FOP with service
 * - MD Service operation @ref m0_mds_stop()<br>
 *   Free buffer_pool and unregister I/O FOP with service
 * - MD Service operation @ref m0_mds_fini()<br>
 *   Free MD Service instance.
 *
 *  @{
 */

/**
 * Structure contains generic service structure and
 * service specific information.
 */
struct m0_reqh_md_service {
	/** Generic reqh service object */
	struct m0_reqh_service	rmds_gen;
	/** Magic to check io service object */
	uint64_t		rmds_magic;
};

M0_INTERNAL void m0_mds_unregister(void);
M0_INTERNAL int m0_mds_register(void);

/** @} end of mdservice */

#endif /* __MERO_MDSERVICE_MD_SERVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
