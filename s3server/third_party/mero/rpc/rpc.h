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
 * Original author: Nikita_Danilov <Nikita_Danilov@xyratex.com>
 *		    Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 04/28/2011
 */

/**
   @defgroup rpc RPC
   @page rpc-layer-core-dld RPC layer core DLD
   @section Overview
   RPC layer core is used to transmit rpc items and groups of them.

   @see https://docs.google.com/a/xyratex.com/Doc?docid=0AQaCw6YRYSVSZGZmMz
V6NzJfMTljbTZ3anhjbg&hl=en

   @{
*/

#pragma once

#ifndef __MERO_RPC_RPCCORE_H__
#define __MERO_RPC_RPCCORE_H__

#include "lib/arith.h"                /* max32u */
#include "rpc/rpc_machine.h"
#include "rpc/conn.h"
#include "rpc/session.h"
#include "rpc/item.h"
#include "rpc/bulk.h"
#include "rpc/rpc_helpers.h"
#include "net/buffer_pool.h"
#include "rpc/item.h"        /* m0_rpc_item_onwire_header_size */
#include "rpc/item_source.h"

/* imports */

M0_INTERNAL int m0_rpc_init(void);
M0_INTERNAL void m0_rpc_fini(void);

/**
  Posts an item to the rpc layer.

  The rpc layer will try to send the item out not later than
  item->ri_deadline and with priority of item->ri_priority.

  Operation timeout is controlled by item->ri_resend_interval and
  item->ri_nr_sent_max. By default their values are set to 1 second and
  UINT64_MAX respectively. RPC resends the request every ->ri_resend_interval
  seconds, until a reply is received. item->ri_resend_interval and
  item->ri_nr_sent_max are "public" fields that user can set before
  posting an item.

  After successful call to m0_rpc_post(), user should not attempt to directly
  free the item. Instead reference on the item should be dropped.
  @see m0_fop_put()
  @see m0_rpc_item_type_ops::rito_item_put

  Callbacks:

  @see m0_rpc_item_ops::rio_sent
  @see m0_rpc_item_ops::rio_replied

  If item is successfully placed on network, then rio_sent() is called.
  If RPC layer failed to place item on network, then rio_sent() is called with
     item->ri_error set to non-zero error code.
  When reply to item is received, rio_replied() callback is called with
     item->ri_reply pointing to reply item and item->ri_error == 0.
  If request's operation timeout is passed or receiver reported failure, then
     rio_replied() is called with item->ri_error set to non-zero error code and
     item->ri_reply == NULL.

  To wait until either reply is received or request is failed use
  m0_rpc_item_wait_for_reply().

  @pre item->ri_session != NULL
  @pre M0_IN(session_state(item->ri_session), (M0_RPC_SESSION_IDLE,
					       M0_RPC_SESSION_BUSY))
  @pre m0_rpc_item_is_request(item) && !m0_rpc_item_is_bound(item)
  @pre m0_rpc_item_size(item) <=
          m0_rpc_session_get_max_item_size(item->ri_session)
*/
M0_INTERNAL int m0_rpc_post(struct m0_rpc_item *item);

/**
  Posts reply item on the same session on which the request item is received.

  RPC items are reference counted, so do not directly free request or reply
  items.

  @see m0_fop_put()
  @see m0_rpc_item_type_ops::rito_item_put
 */
void m0_rpc_reply_post(struct m0_rpc_item *request, struct m0_rpc_item *reply);

M0_INTERNAL void m0_rpc_oneway_item_post(const struct m0_rpc_conn *conn,
					 struct m0_rpc_item *item);

/**
   Create a buffer pool per net domain which to be shared by TM's in it.
   @pre ndom != NULL && app_pool != NULL
   @pre bufs_nr != 0
 */
M0_INTERNAL int m0_rpc_net_buffer_pool_setup(struct m0_net_domain *ndom,
					     struct m0_net_buffer_pool
					     *app_pool, uint32_t bufs_nr,
					     uint32_t tm_nr);

void m0_rpc_net_buffer_pool_cleanup(struct m0_net_buffer_pool *app_pool);

/**
 * Calculates the total number of buffers needed in network domain for
 * receive buffer pool.
 * @param len total Length of the TM's in a network domain
 * @param tms_nr    Number of TM's in the network domain
 */
M0_INTERNAL uint32_t m0_rpc_bufs_nr(uint32_t len, uint32_t tms_nr);

/** Returns the maximum segment size of receive pool of network domain. */
M0_INTERNAL m0_bcount_t m0_rpc_max_seg_size(struct m0_net_domain *ndom);

/** Returns the maximum number of segments of receive pool of network domain. */
M0_INTERNAL uint32_t m0_rpc_max_segs_nr(struct m0_net_domain *ndom);

/** Returns the maximum RPC message size in the network domain. */
M0_INTERNAL m0_bcount_t m0_rpc_max_msg_size(struct m0_net_domain *ndom,
					    m0_bcount_t rpc_size);

/**
 * Returns the maximum number of messages that can be received in a buffer
 * of network domain for a specific maximum receive message size.
 */
M0_INTERNAL uint32_t m0_rpc_max_recv_msgs(struct m0_net_domain *ndom,
					  m0_bcount_t rpc_size);

M0_INTERNAL m0_time_t m0_rpc__down_timeout(void);

/** @} end group rpc */

#endif /* __MERO_RPC_RPCCORE_H__  */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
