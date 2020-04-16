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
 * Original creation date: 28-Jun-2012
 */

#pragma once

#ifndef __MERO_SETUP_INTERNAL_H__
#define __MERO_SETUP_INTERNAL_H__

#include "mero/setup.h"

/* import */
struct m0_storage_devs;

/**
   @addtogroup m0d
   @{
 */

/** Represents list of buffer pools in the mero context. */
struct cs_buffer_pool {
	/** Network buffer pool object. */
	struct m0_net_buffer_pool cs_buffer_pool;
	/** Linkage into network buffer pool list. */
	struct m0_tlink           cs_bp_linkage;
	uint64_t                  cs_bp_magic;
};

M0_INTERNAL int cs_service_init(const char *name, struct m0_reqh_context *rctx,
				struct m0_reqh *reqh, struct m0_fid *fid);
M0_INTERNAL void cs_service_fini(struct m0_reqh_service *service);

/** Uses confc API to generate CLI arguments, understood by _args_parse(). */
M0_INTERNAL int cs_conf_to_args(struct cs_args *dest, struct m0_conf_root *r);

M0_INTERNAL int cs_conf_storage_init(struct cs_stobs        *stob,
				     struct m0_storage_devs *devs,
				     bool                    force);

M0_INTERNAL int cs_conf_device_reopen(struct m0_poolmach *pm,
				      struct cs_stobs *stob, uint32_t dev_id);

M0_INTERNAL int cs_conf_services_init(struct m0_mero *cctx);

/** @} endgroup m0d */
#endif /* __MERO_SETUP_INTERNAL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
