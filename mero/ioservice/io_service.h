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
 * Original author: Rajanikant Chirmade <Rajanikant_Chirmade@xyratex.com>
 * Original creation date: 11/02/2011
 */

#pragma once

#ifndef __MERO_IOSERVICE_IO_SERVICE_H__
#define __MERO_IOSERVICE_IO_SERVICE_H__

/**
 * @defgroup DLD_bulk_server_fspec_ioservice_operations I/O Service Operations
 * @see @ref DLD-bulk-server
 * @see @ref reqh
 *
 * I/O Service initialization and operations controlled by request handler.
 *
 * I/O Service defines service type operation vector -
 * - I/O Service type operation @ref m0_ioservice_alloc_and_init()<br>
 *   Request handler uses this service type operation to allocate and
 *   and initiate service instance.
 *
 * I/O Service defines service operation vector -
 * - I/O Service operation @ref m0_ioservice_start()<br>
 *   Initiate buffer_pool and register I/O FOP with service
 * - I/O Service operation @ref m0_ioservice_stop()<br>
 *   Free buffer_pool and unregister I/O FOP with service
 * - I/O Service operation @ref m0_ioservice_fini)<br>
 *   Free I/O Service instance.
 *
 * State transition diagram for I/O Service will be available at @ref reqh
 *
 *  @{
 */
#include "net/buffer_pool.h"
#include "reqh/reqh_service.h"
#include "lib/chan.h"
#include "lib/tlist.h"
#include "cob/cob.h"
#include "layout/layout.h"
#include "rpc/conn.h"
#include "rpc/session.h"
#include "ioservice/ios_start_sm.h" /* m0_ios_start_sm */
#include "pool/pool_machine.h"      /* struct m0_poolmach_versions */

struct m0_fom;

M0_INTERNAL int m0_ios_register(void);
M0_INTERNAL void m0_ios_unregister(void);

/**
 * Data structure represents list of buffer pool per network domain.
 */
struct m0_rios_buffer_pool {
        /** Pointer to Network buffer pool. */
        struct m0_net_buffer_pool    rios_bp;
        /** Pointer to net domain owner of this buffer pool */
        struct m0_net_domain        *rios_ndom;
        /** Buffer pool wait channel. */
        struct m0_chan               rios_bp_wait;
        /** Linkage into netowrk buffer pool list */
        struct m0_tlink              rios_bp_linkage;
        /** Magic */
        uint64_t                     rios_bp_magic;
};

/**
 * Structure contains generic service structure and
 * service specific information.
 */
struct m0_reqh_io_service {
	/** Generic reqh service object */
	struct m0_reqh_service       rios_gen;
	/** Buffer pools belongs to this services */
	struct m0_tl                 rios_buffer_pools;
	/** Cob domain for ioservice. */
	struct m0_cob_domain         *rios_cdom;

	/**
	 * rpc client to metadata & management service.
	 * This is stored in reqh as a key. So other services, like "sns_repair"
	 * can also use this to contact mds to get layout for files, get
	 * attributes for files, etc.
	 */
	struct m0_rpc_client_ctx    *rios_mds_rpc_ctx;
	struct m0_net_domain         rios_cl_ndom;

	/** SM for start service */
	struct m0_ios_start_sm       rios_sm;
	/** Clink of SM for start service */
	struct m0_clink              rios_clink;

	/** FOM to be notified about asynchronous start completion */
	struct m0_fom               *rios_fom;
	/** magic to check io service object */
	uint64_t                     rios_magic;
};

M0_INTERNAL bool m0_reqh_io_service_invariant(const struct m0_reqh_io_service
					      *rios);

M0_INTERNAL void m0_ios_cdom_get(struct m0_reqh        *reqh,
				 struct m0_cob_domain **out);

M0_INTERNAL void m0_ios_cdom_fini(struct m0_reqh *reqh);

struct m0_ios_mds_conn {
	struct m0_rpc_conn    imc_conn;
	struct m0_rpc_session imc_session;
	bool                  imc_connected;
};

enum {
	M0T1FS_MAX_NR_MDS = 1024
};

struct m0_ios_mds_conn_map {
	struct m0_ios_mds_conn *imc_map[M0T1FS_MAX_NR_MDS];
	uint32_t                imc_nr;
};

M0_INTERNAL void m0_ios_mds_conn_fini(struct m0_reqh *reqh);

M0_INTERNAL int m0_ios_mds_getattr(struct m0_reqh *reqh,
				   const struct m0_fid *gfid,
				   struct m0_cob_attr *attr);
M0_INTERNAL int m0_ios_getattr(struct m0_reqh *reqh,
			       const struct m0_fid *gfid,
			       uint64_t index,
			       struct m0_cob_attr *attr);

M0_INTERNAL int m0_ios_mds_getattr_async(struct m0_reqh *reqh,
				         const struct m0_fid *gfid,
					 struct m0_cob_attr  *attr,
					 void (*cb)(void *arg, int rc),
					 void *arg);
M0_INTERNAL int m0_ios_getattr_async(struct m0_reqh *reqh,
				     const struct m0_fid *gfid,
				     struct m0_cob_attr  *attr,
				     uint64_t index,
				     void (*cb)(void *arg, int rc),
				     void *arg);
M0_INTERNAL int m0_ios_cob_getattr_async(const struct m0_fid *gfid,
					 struct m0_cob_attr *attr,
					 uint64_t cob_idx,
					 struct m0_pool_version *pv,
					 void (*cb)(void *arg, int rc),
					 void *arg);

/**
 * Sets default values for buf_nr for m0_net_buffer_pool_provision() in
 * ioservice.
 *
 * @note It sets a static variable, so this change is persistent across
 * m0_init()/m0_fini().
 * @see ios_net_buffer_pool_size
 */
M0_INTERNAL void m0_ios_net_buffer_pool_size_set(uint32_t buffer_pool_size);

/** @} end of io_service */

#endif /* __MERO_IOSERVICE_IO_SERVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
