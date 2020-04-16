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
 * Original author: Igor Vartanov
 * Original creation date: 01-Apr-2015
 */

#pragma once

#ifndef __MERO_CONF_RCONFC_INTERNAL_H__
#define __MERO_CONF_RCONFC_INTERNAL_H__

#include "fid/fid.h"       /* m0_fid */
#include "fop/fom.h"       /* m0_fom */
#include "rm/rm_rwlock.h"  /* m0_rm_rw_lock */

/**
   @addtogroup rconfc_dlspec
   @{
*/

enum confc_state {
	CONFC_IDLE,       /**< The confd is connected to with no reading      */
	CONFC_ARMED,      /**< Reading version from the confd has started     */
	CONFC_OPEN,       /**< The confd was used to engage conductor         */
	CONFC_FAILED,     /**< Reading failure registered with the confd      */
	CONFC_DEAD,       /**< The confd has failed to establish connection   */
};

/* -------------- Read lock context ----------------- */
struct m0_rconfc;

struct rlock_ctx {
	struct m0_rconfc          *rlc_parent;    /**< back link to parent    */
	struct m0_rpc_machine     *rlc_rmach;     /**< rpc machine            */
	struct m0_rpc_conn         rlc_conn;      /**< rpc connection         */
	struct m0_rpc_session      rlc_sess;      /**< rpc session            */
	char                      *rlc_rm_addr;   /**< HA-reported RM address */
	struct m0_fid              rlc_rm_fid;    /**< HA-reported RM fid     */
	bool                       rlc_online;    /**< is RM connected        */
	struct m0_rw_lockable      rlc_rwlock;    /**< lockable resource      */
	struct m0_rm_owner         rlc_owner;     /**< local owner-borrower   */
	struct m0_fid              rlc_owner_fid; /**< owner fid              */
	struct m0_rm_remote        rlc_creditor;  /**< remote creditor        */
	struct m0_rm_incoming      rlc_req;       /**< request to wait on     */
	struct m0_clink            rlc_clink;     /**< waiter for owner windup*/
	struct m0_rm_domain        rlc_dom;       /**< read domain            */
	struct m0_rm_resource_type rlc_rt;        /**< read resource type     */
	m0_time_t                  rlc_timeout;   /**< connection timeout, ns */
};

/* -------------- Quorum calculation context ----------------- */

/*
 * Version Item.  Keeps version number along with counter.
 */
struct ver_item {
	uint64_t vi_ver;
	uint32_t vi_count;
};

enum {
	VERSION_ITEMS_TOTAL_MAX = 256
};

/*
 * Version Accumulator. Keeps already found version items along with counter and
 * total confd count.
 */
struct ver_accm {
	struct ver_item  va_items[VERSION_ITEMS_TOTAL_MAX];
	int              va_count;
	int              va_total;
};

/* --------------   Rconfc internals   ----------------- */

/**
 * Elementary linkage container for placing single confc to herd and active
 * lists
 */
struct rconfc_link {
	struct m0_confc      rl_confc;         /**< confc instance           */
	struct m0_rconfc    *rl_rconfc;        /**< back link to owner       */
	struct m0_confc_ctx  rl_cctx;          /**< confc read context       */
	struct m0_clink      rl_clink;         /**< confc read wait clink    */
	struct m0_tlink      rl_herd;          /**< link to herd list        */
	struct m0_tlink      rl_active;        /**< link to active list      */
	uint64_t             rl_magic;         /**< confc link magic         */
	char                *rl_confd_addr;    /**< confd peer address       */
	struct m0_fid        rl_confd_fid;     /**< confd service fid        */
	struct m0_clink      rl_ha_clink;      /**< confd death clink        */
	int                  rl_rc;            /**< confc result             */
	int                  rl_state;         /**< current state            */
	bool                 rl_preserve;      /**< preserve on herd update  */
	struct m0_fom        rl_fom;           /**< disconnection FOM        */
	struct m0_clink      rl_fom_clink;     /**< disconnection wait clink */
	bool                 rl_fom_queued;    /**< if FOM already in queue  */
	/* XXX: intended for UT only */
	void (*rl_on_state_cb)(struct rconfc_link *lnk);
};

/*
 * made accessible for UT suites
 */
M0_INTERNAL struct rconfc_link *rconfc_herd_find(struct m0_rconfc *rconfc,
						 const char       *addr);

/**
   @}
 */

#endif /* __MERO_CONF_RCONFC_INTERNAL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
