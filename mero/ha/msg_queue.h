/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 26-Apr-2016
 */

#pragma once

#ifndef __MERO_HA_MSG_QUEUE_H__
#define __MERO_HA_MSG_QUEUE_H__

/**
 * @defgroup ha
 *
 * This structure is added to support basic queue operations and to support
 * m0_ha_msg pre-allocation for OOM cases (when it's needed).
 *
 * User is responsible for locking.
 *
 * @{
 */

#include "lib/types.h"  /* uint64_t */
#include "lib/tlist.h"  /* m0_tlink */
#include "ha/msg.h"     /* m0_ha_msg */

enum m0_ha_msg_qitem_delivery_state {
	M0_HA_MSG_QITEM_NOT_DELIVERED,
	M0_HA_MSG_QITEM_DELIVERED,
};

/** m0_ha_msg_queue item. */
struct m0_ha_msg_qitem {
	struct m0_ha_msg                    hmq_msg;
	enum m0_ha_msg_qitem_delivery_state hmq_delivery_state;
	/** Link for m0_ha_msg_queue::mq_queue */
	struct m0_tlink                     hmq_link;
	uint64_t                            hmq_magic;
};

struct m0_ha_msg_queue_cfg {
	int unused;
};

struct m0_ha_msg_queue {
	struct m0_ha_msg_queue_cfg mq_cfg;
	struct m0_tl               mq_queue;
};

M0_INTERNAL void m0_ha_msg_queue_init(struct m0_ha_msg_queue     *mq,
                                      struct m0_ha_msg_queue_cfg *cfg);
M0_INTERNAL void m0_ha_msg_queue_fini(struct m0_ha_msg_queue *mq);
M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_alloc(struct m0_ha_msg_queue *mq);
M0_INTERNAL void m0_ha_msg_queue_free(struct m0_ha_msg_queue *mq,
                                      struct m0_ha_msg_qitem *qitem);
M0_INTERNAL void m0_ha_msg_queue_enqueue(struct m0_ha_msg_queue *mq,
                                         struct m0_ha_msg_qitem *qitem);
M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_dequeue(struct m0_ha_msg_queue *mq);
M0_INTERNAL void m0_ha_msg_queue_push_front(struct m0_ha_msg_queue *mq,
                                            struct m0_ha_msg_qitem *qitem);
M0_INTERNAL bool m0_ha_msg_queue_is_empty(struct m0_ha_msg_queue *mq);
M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_find(struct m0_ha_msg_queue *mq,
                     uint64_t                tag);
M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_next(struct m0_ha_msg_queue *mq,
		     const struct m0_ha_msg_qitem *cur);
M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_prev(struct m0_ha_msg_queue *mq,
                     const struct m0_ha_msg_qitem *cur);


/** @} end of ha group */
#endif /* __MERO_HA_MSG_QUEUE_H__ */

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
