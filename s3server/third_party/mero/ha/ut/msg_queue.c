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


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/msg_queue.h"
#include "ut/ut.h"

#include "lib/misc.h"    /* M0_SET0 */
#include "lib/arith.h"   /* m0_rnd64 */
#include "lib/memory.h"  /* M0_ALLOC_ARR */

enum {
	HA_UT_MSG_QUEUE_NR       = 0x1000,
};

void m0_ha_ut_msg_queue(void)
{
	struct m0_ha_msg_queue      mq = {};
	struct m0_ha_msg_queue_cfg  mq_cfg = {};
	struct m0_ha_msg_qitem     *qitem;
	int                         i;
	uint64_t                    seed = 42;
	uint64_t                   *tags;

	m0_ha_msg_queue_init(&mq, &mq_cfg);
	m0_ha_msg_queue_fini(&mq);

	M0_SET0(&mq);
	m0_ha_msg_queue_init(&mq, &mq_cfg);
	qitem = m0_ha_msg_queue_dequeue(&mq);
	M0_UT_ASSERT(qitem == NULL);
	M0_ALLOC_ARR(tags, HA_UT_MSG_QUEUE_NR);
	M0_UT_ASSERT(tags != NULL);
	for (i = 0; i < HA_UT_MSG_QUEUE_NR; ++i) {
		qitem = m0_ha_msg_queue_alloc(&mq);
		M0_UT_ASSERT(qitem != NULL);
		tags[i] = m0_rnd64(&seed);
		qitem->hmq_msg.hm_tag = tags[i];
		m0_ha_msg_queue_enqueue(&mq, qitem);
	}
	for (i = 0; i < HA_UT_MSG_QUEUE_NR; ++i) {
		qitem = m0_ha_msg_queue_dequeue(&mq);
		M0_UT_ASSERT(qitem != NULL);
		M0_UT_ASSERT(m0_ha_msg_tag(&qitem->hmq_msg) == tags[i]);
		m0_ha_msg_queue_free(&mq, qitem);
	}
	m0_free(tags);
	qitem = m0_ha_msg_queue_dequeue(&mq);
	M0_UT_ASSERT(qitem == NULL);
	m0_ha_msg_queue_fini(&mq);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
