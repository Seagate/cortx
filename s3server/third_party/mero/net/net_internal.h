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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>,
 *                  Carl Braganza <Carl_Braganza@xyratex.com>
 * Original creation date: 04/04/2011
 */

#pragma once

#ifndef __MERO_NET_NET_INTERNAL_H__
#define __MERO_NET_NET_INTERNAL_H__

#include "net/net.h"

/**
   @defgroup net_pvt Network Module Internals
   @ingroup net
   Private interfaces used within the Network module.
   @{
 */

extern struct m0_mutex m0_net_mutex;

/** Validates the value of buffer queue type. */
M0_INTERNAL bool m0_net__qtype_is_valid(enum m0_net_queue_type qt);

/** Validates transfer machine state. */
M0_INTERNAL bool m0_net__tm_state_is_valid(enum m0_net_tm_state ts);

/** TM event invariant. */
M0_INTERNAL bool m0_net__tm_event_invariant(const struct m0_net_tm_event *ev);

/** Validates the TM event type. */
M0_INTERNAL bool m0_net__tm_ev_type_is_valid(enum m0_net_tm_ev_type et);

/** Buffer event invariant. */
M0_INTERNAL bool m0_net__buffer_event_invariant(const struct m0_net_buffer_event
						*ev);

/**
  Buffer checks for a registered buffer.
  Must be called within the domain or transfer machine mutex.
 */
M0_INTERNAL bool m0_net__buffer_invariant(const struct m0_net_buffer *buf);

/**
  Internal version of m0_net_buffer_add() that must be invoked holding the
  TM mutex.
 */
M0_INTERNAL int m0_net__buffer_add(struct m0_net_buffer *buf,
				   struct m0_net_transfer_mc *tm);

/**
  Invariant checks for an end point. No mutex necessary.
  Extra checks if under_tm_mutex set to true.
 */
M0_INTERNAL bool m0_net__ep_invariant(struct m0_net_end_point *ep,
				      struct m0_net_transfer_mc *tm,
				      bool under_tm_mutex);

/**
  Validates tm state.
  Must be called within the domain or transfer machine mutex.
 */
M0_INTERNAL bool m0_net__tm_invariant(const struct m0_net_transfer_mc *tm);

M0_INTERNAL int m0_net__tm_provision_buf(struct m0_net_transfer_mc *tm);

/**
   Internal subroutine to provision the receive queue of a transfer machine
   from its associated buffer pool.

   @pre m0_mutex_is_not_locked(&tm->ntm_mutex) && tm->ntm_callback_counter > 0
   @pre m0_net_buffer_pool_is_not_locked(&tm->ntm_recv_pool))
   @post Length of receive queue >= tm->ntm_recv_queue_min_length &&
                tm->ntm_recv_queue_deficit == 0 ||
         Length of receive queue + tm->ntm_recv_queue_deficit ==
                tm->ntm_recv_queue_min_length
 */
M0_INTERNAL void m0_net__tm_provision_recv_q(struct m0_net_transfer_mc *tm);

/**
   Internal sub variant to get TM statistics from within the TM mutex.

   @pre m0_mutex_is_locked(&tm->ntm_mutex)
 */
M0_INTERNAL int m0_net__tm_stats_get(struct m0_net_transfer_mc *tm,
				     enum m0_net_queue_type qtype,
				     struct m0_net_qstats *qs, bool reset);

/**
 * Common part of post-callback processing for m0_net_tm_event_post() and
 * m0_net_buffer_event_post().
 *
 * Under tm mutex: decrement ref counts, signal waiters
 */
M0_INTERNAL void m0_net__tm_post_callback(struct m0_net_transfer_mc *tm);

/**
 * Cancels all buffers posted to the machine queues.
 *
 * @pre m0_mutex_is_locked(&tm->ntm_mutex)
 */
M0_INTERNAL void m0_net__tm_cancel(struct m0_net_transfer_mc *tm);

/**
   @} net-int
 */

#endif /* __MERO_NET_NET_INTERNAL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
