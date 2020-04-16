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
 * Original author:
 * Original creation date:
 */

#pragma once

#ifndef __MERO_RPC_CONN_POOL_H__
#define __MERO_RPC_CONN_POOL_H__

#include "rpc/conn.h"
#include "rpc/session.h"
#include "rpc/rpc_machine.h"
#include "rpc/link.h"

struct m0_rpc_conn_pool;

struct m0_rpc_conn_pool_item {
	struct m0_rpc_link       cpi_rpc_link;
	struct m0_chan           cpi_chan;
	struct m0_clink          cpi_clink;
	int                      cpi_users_nr;
	bool                     cpi_connecting; /**< connect is in progress */
	struct m0_tlink          cpi_linkage;    /**< linkage for cp_items */
	struct m0_rpc_conn_pool *cpi_pool;       /**< conn pool ref */
	uint64_t                 cpi_magic;
};

struct m0_rpc_conn_pool {
	struct m0_tl             cp_items;
	struct m0_rpc_machine   *cp_rpc_mach;
	struct m0_mutex          cp_mutex;
	struct m0_mutex          cp_ch_mutex;
	m0_time_t                cp_timeout;
	uint64_t                 cp_max_rpcs_in_flight;
};

M0_INTERNAL int m0_rpc_conn_pool_init(
	struct m0_rpc_conn_pool *pool,
	struct m0_rpc_machine   *rpc_mach,
	m0_time_t                conn_timeout,
	uint64_t                 max_rpcs_in_flight);

M0_INTERNAL void m0_rpc_conn_pool_fini(struct m0_rpc_conn_pool *pool);

M0_INTERNAL int m0_rpc_conn_pool_get_sync(
		struct m0_rpc_conn_pool *pool,
		const char              *remote_ep,
		struct m0_rpc_session   **session);

/**
 * @todo Potential race if connection is established before
 * clink is added to session channel.
 */
M0_INTERNAL int m0_rpc_conn_pool_get_async(
		struct m0_rpc_conn_pool *pool,
		const char              *remote_ep,
		struct m0_rpc_session   **session);

M0_INTERNAL void m0_rpc_conn_pool_put(
		struct m0_rpc_conn_pool *pool,
		struct m0_rpc_session   *session);

M0_INTERNAL
struct m0_chan *m0_rpc_conn_pool_session_chan(struct m0_rpc_session *session);

/**
 * @todo Unprotected access to ->sm_state in this function.
 */
M0_INTERNAL bool m0_rpc_conn_pool_session_established(
		struct m0_rpc_session *session);

#endif /* __MERO_RPC_CONN_POOL_H__ */

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
