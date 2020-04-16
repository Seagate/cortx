
 /* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 13-Feb-2015
 */

#pragma once

#ifndef __MERO_SPIEL_FOPS_H__
#define __MERO_SPIEL_FOPS_H__


#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "fid/fid_xc.h"         /* m0_fid_xc */
#include "lib/types_xc.h"       /* m0_uint128_xc */
#include "fop/fop.h"            /* m0_fop */
#include "rpc/bulk.h"           /* m0_rpc_bulk */
#include "net/net_otw_types.h"  /* m0_net_buf_desc_data */

/**
 * @addtogroup spiel-api-fspec-intr
 * @{
 */

/**
 * Spiel write lock context. Intended for taking write lock along transaction
 * commission. When obtaining write lock, all read locks are revoked across
 * entire cluster. Later read locks become obtainable only when write lock
 * appears released explicitly.
 *
 * @note Write lock is held exclusively and cannot be revoked by RM. Only
 * explicit lock cancellation is possible.
 */
struct m0_spiel_wlock_ctx {
	struct m0_rpc_machine     *wlc_rmach;     /**< rpc machine            */
	struct m0_rpc_conn         wlc_conn;      /**< rpc connection         */
	struct m0_rpc_session      wlc_sess;      /**< rpc session            */
	char                      *wlc_rm_addr;   /**< HA-reported RM address */
	struct m0_fid              wlc_rm_fid;    /**< HA-reported RM fid     */
	struct m0_rw_lockable      wlc_rwlock;    /**< lockable resource      */
	struct m0_rm_owner         wlc_owner;     /**< local owner-borrower   */
	struct m0_fid              wlc_owner_fid; /**< owner fid              */
	struct m0_rm_remote        wlc_creditor;  /**< remote creditor        */
	struct m0_rm_incoming      wlc_req;       /**< request to wait on     */
	/** semaphore to wait until request is completed */
	struct m0_semaphore        wlc_sem;
	/**
	 * Write resource domain. Needs to be separate from global read domain
	 * used by @ref rconfc instances. (see m0_rwlockable_read_domain())
	 */
	struct m0_rm_domain        wlc_dom;
	/**
	 * Write resource type. Needs to be registered with the write resource
	 * domain.
	 */
	struct m0_rm_resource_type wlc_rt;
	/** result code of write lock request */
	int32_t                    wlc_rc;
};

/**
 * This data structure is used to associate an Spiel fop with its
 * rpc bulk data. It abstracts the m0_net_buffer and net layer APIs.
 * Client side implementations use this structure to represent
 * conf fops and the associated rpc bulk structures.
 * @see m0_rpc_bulk().
 */
struct m0_spiel_load_command {
	/** Inline fop for a generic Conf Load fop. */
	struct m0_fop         slc_load_fop;
	/** Inline fop for a generic Conf Flip fop. */
	struct m0_fop         slc_flip_fop;
	/** Rpc bulk structure containing zero vector for spiel fop. */
	struct m0_rpc_bulk    slc_rbulk;
	/** Connect */
	struct m0_rpc_conn    slc_connect;
	/** Session */
	struct m0_rpc_session slc_session;
	/* Error status */
	int                   slc_status;
	/** Current Version on Confd */
	int                   slc_version;
};

/** @} */
/* __MERO_SPIEL_FOPS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
