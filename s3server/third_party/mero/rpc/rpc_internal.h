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
 * Original author: Amit Jambure <amit_jambure@xyratex.com>
 * Original creation date: 10/31/2012
 */

#pragma once

#ifndef __MERO_RPC_INT_H__
#define __MERO_RPC_INT_H__

#include "rpc/conn_internal.h"
#include "rpc/session_internal.h"
#include "rpc/item_internal.h"
#include "rpc/rpc_machine_internal.h"
#include "rpc/formation2_internal.h"
#include "rpc/packet_internal.h"
#include "rpc/session_fops_xc.h"
#include "rpc/session_fops.h"
#include "rpc/session_foms.h"
#include "rpc/onwire.h"
#include "rpc/onwire_xc.h"
#include "rpc/rpc.h"

/**
 * @addtogroup rpc
 * @{
 */

/**
   Initialises all the session related fop types
 */
M0_INTERNAL int m0_rpc_session_module_init(void);

/**
   Finalises all session realted fop types
 */
M0_INTERNAL void m0_rpc_session_module_fini(void);

/**
   Called for each received item.
 */
M0_INTERNAL void rpc_worker_thread_fn(struct m0_rpc_machine *machine);

/**
   Helper routine, internal to rpc module.
   Sets up and posts rpc-item representing @fop.
 */
M0_INTERNAL int m0_rpc__fop_post(struct m0_fop *fop,
				 struct m0_rpc_session *session,
				 const struct m0_rpc_item_ops *ops,
				 m0_time_t abs_timeout);

/**
   Posts rpc item while having rpc machine already locked.
 */
M0_INTERNAL int m0_rpc__post_locked(struct m0_rpc_item *item);

/**
   Temporary routine to place fop in a global queue, from where it can be
   selected for execution.
 */
M0_INTERNAL int m0_rpc_item_dispatch(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_oneway_item_post_locked(const struct m0_rpc_conn *conn,
						struct m0_rpc_item *item);

M0_INTERNAL uint64_t m0_rpc_id_generate(void);

M0_INTERNAL int m0_rpc_service_start(struct m0_reqh *reqh);
M0_INTERNAL void m0_rpc_service_stop(struct m0_reqh *reqh);

M0_TL_DESCR_DECLARE(item_source, M0_EXTERN);
M0_TL_DECLARE(item_source, M0_INTERNAL, struct m0_rpc_item_source);

static inline struct m0_rpc_conn *item2conn(const struct m0_rpc_item *item)
{
	return item->ri_session->s_conn;
}

/** @} */

#endif /* __MERO_RPC_INT_H__ */
