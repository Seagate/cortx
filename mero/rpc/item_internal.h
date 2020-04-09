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

#ifndef __MERO_RPC_ITEM_INT_H__
#define __MERO_RPC_ITEM_INT_H__

#include "rpc/item.h"

struct m0_fid;

/**
   @addtogroup rpc

   @{
 */

/** Initialises global the rpc item state including types list and lock */
M0_INTERNAL int m0_rpc_item_module_init(void);

/**
  Finalizes and destroys the global rpc item state including type list by
  traversing the list and deleting and finalizing each element.
*/
M0_INTERNAL void m0_rpc_item_module_fini(void);

M0_INTERNAL bool m0_rpc_item_is_update(const struct m0_rpc_item *item);
M0_INTERNAL bool m0_rpc_item_is_oneway(const struct m0_rpc_item *item);
M0_INTERNAL bool m0_rpc_item_is_request(const struct m0_rpc_item *item);
M0_INTERNAL bool m0_rpc_item_is_reply(const struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_xid_assign(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_sm_init(struct m0_rpc_item *item,
				     enum m0_rpc_item_dir dir);
M0_INTERNAL void m0_rpc_item_sm_fini(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_change_state(struct m0_rpc_item *item,
					  enum m0_rpc_item_state state);
M0_INTERNAL void m0_rpc_item_failed(struct m0_rpc_item *item, int32_t rc);

M0_INTERNAL int m0_rpc_item_timer_start(struct m0_rpc_item *item);
M0_INTERNAL void m0_rpc_item_timer_stop(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_ha_timer_stop(struct m0_rpc_item *item);

M0_INTERNAL void m0_rpc_item_send(struct m0_rpc_item *item);

M0_INTERNAL const char *item_kind(const struct m0_rpc_item *item);
M0_INTERNAL const char *item_state_name(const struct m0_rpc_item *item);

M0_INTERNAL int m0_rpc_item_received(struct m0_rpc_item *item,
				     struct m0_rpc_machine *machine);

M0_INTERNAL void m0_rpc_item_process_reply(struct m0_rpc_item *req,
					   struct m0_rpc_item *reply);

M0_INTERNAL void m0_rpc_item_send_reply(struct m0_rpc_item *req,
					struct m0_rpc_item *reply);

M0_INTERNAL void m0_rpc_item_replied_invoke(struct m0_rpc_item *item);

/** @} */
#endif /* __MERO_RPC_ITEM_INT_H__ */
