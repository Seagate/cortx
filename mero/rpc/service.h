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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 02/23/2012
 */

#pragma once

#ifndef __MERO_RPC_SERVICE_H__
#define __MERO_RPC_SERVICE_H__

#include "rpc/session.h"
#include "rpc/item.h"
#include "reqh/reqh_service.h"

/**
   @defgroup rpc_service RPC service
   @{
 */

struct m0_rpc_service {
	/** Reqh service representation */
	struct m0_reqh_service rps_svc;
	/** List maintaining reverse connections to clients */
	struct m0_tl           rps_rev_conns;
	/** magic == M0_RPC_SERVICE_MAGIC */
	uint64_t               rps_magix;
};

M0_INTERNAL int m0_rpc_service_register(void);
M0_INTERNAL void m0_rpc_service_unregister(void);

/**
 * Return reverse session to given item.
 *
 * @pre svc != NULL
 * @pre item != NULL && session != NULL
 */
M0_INTERNAL int
m0_rpc_service_reverse_session_get(struct m0_reqh_service   *service,
				   const struct m0_rpc_item *item,
				   struct m0_clink          *clink,
				   struct m0_rpc_session   **session);

M0_INTERNAL void
m0_rpc_service_reverse_session_put(struct m0_reqh_service *service);

M0_INTERNAL int m0_rpc_session_status(struct m0_rpc_session *session);

M0_INTERNAL struct m0_rpc_session *
m0_rpc_service_reverse_session_lookup(struct m0_reqh_service    *service,
				      const struct m0_rpc_item *item);

M0_INTERNAL struct m0_reqh_service *
m0_reqh_rpc_service_find(struct m0_reqh *reqh);

/**
   @} end of rpc_service group
 */
#endif /* __MERO_RPC_SERVICE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
