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
 * Original creation date: 24-Jul-2016
 */

/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/lq.h"

#include "lib/misc.h"           /* M0_IS0 */
#include "lib/assert.h"         /* M0_PRE */

#include "ha/msg.h"             /* M0_HA_MSG_TAG_UNKNOWN */

static bool ha_lq_tags_invariant(const struct m0_ha_link_tags *tags)
{
	return tags->hlt_confirmed % 2 == tags->hlt_delivered % 2 &&
	       tags->hlt_confirmed % 2 == tags->hlt_next        % 2 &&
	       tags->hlt_confirmed % 2 == tags->hlt_assign      % 2 &&
	       tags->hlt_confirmed   <= tags->hlt_delivered &&
	       tags->hlt_delivered <= tags->hlt_next &&
	       tags->hlt_next        <= tags->hlt_assign &&
	       equi(tags->hlt_confirmed == 0, tags->hlt_delivered == 0) &&
	       equi(tags->hlt_confirmed == 0, tags->hlt_next == 0) &&
	       equi(tags->hlt_confirmed == 0, tags->hlt_assign == 0);

}

M0_INTERNAL bool m0_ha_lq_invariant(const struct m0_ha_lq *lq)
{
	return ha_lq_tags_invariant(&lq->hlq_tags);
}

M0_INTERNAL void m0_ha_lq_init(struct m0_ha_lq           *lq,
                               const struct m0_ha_lq_cfg *lq_cfg)
{
	M0_PRE(M0_IS0(lq));
	lq->hlq_cfg = *lq_cfg;
	m0_ha_msg_queue_init(&lq->hlq_mq, &lq->hlq_cfg.hlqc_msg_queue_cfg);
	M0_POST(m0_ha_lq_invariant(lq));
}

M0_INTERNAL void m0_ha_lq_fini(struct m0_ha_lq *lq)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	M0_ASSERT_INFO(_0C(lq->hlq_tags.hlt_confirmed ==
			   lq->hlq_tags.hlt_delivered) &&
	               _0C(lq->hlq_tags.hlt_confirmed ==
			   lq->hlq_tags.hlt_next) &&
	               _0C(lq->hlq_tags.hlt_confirmed ==
			   lq->hlq_tags.hlt_assign),
	               "lq->hlq_tags="HLTAGS_F, HLTAGS_P(&lq->hlq_tags));
	m0_ha_msg_queue_fini(&lq->hlq_mq);
}

M0_INTERNAL void m0_ha_lq_tags_get(const struct m0_ha_lq  *lq,
                                   struct m0_ha_link_tags *tags)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	*tags = lq->hlq_tags;
}

M0_INTERNAL void m0_ha_lq_tags_set(struct m0_ha_lq              *lq,
                                   const struct m0_ha_link_tags *tags)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	M0_ENTRY("lq->hlq_tags="HLTAGS_F" tags="HLTAGS_F,
	         HLTAGS_P(&lq->hlq_tags), HLTAGS_P(tags));
	if (lq->hlq_tags.hlt_confirmed == 0) {
		lq->hlq_tags = *tags;
	} else {
		M0_IMPOSSIBLE("can only set tags just after init()");
	}
	M0_POST(m0_ha_lq_invariant(lq));
}

M0_INTERNAL bool m0_ha_lq_has_tag(const struct m0_ha_lq *lq, uint64_t tag)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	M0_PRE(lq->hlq_tags.hlt_confirmed > 0);

	return tag % 2 == lq->hlq_tags.hlt_confirmed % 2;
}

M0_INTERNAL struct m0_ha_msg *m0_ha_lq_msg(struct m0_ha_lq *lq, uint64_t tag)
{
	struct m0_ha_msg_qitem *qitem;

	M0_PRE(m0_ha_lq_invariant(lq));
	M0_PRE(m0_ha_lq_has_tag(lq, tag));
	qitem = m0_ha_msg_queue_find(&lq->hlq_mq, tag);
	return qitem == NULL ? NULL : &qitem->hmq_msg;
}

M0_INTERNAL struct m0_ha_msg *m0_ha_lq_msg_next(struct m0_ha_lq *lq,
						const struct m0_ha_msg *cur)
{
	struct m0_ha_msg_qitem *qitem =
		container_of(cur, struct m0_ha_msg_qitem, hmq_msg);

	M0_PRE(m0_ha_lq_invariant(lq));
	qitem = m0_ha_msg_queue_next(&lq->hlq_mq, qitem);
	return qitem == NULL ? NULL : &qitem->hmq_msg;
}

M0_INTERNAL struct m0_ha_msg *m0_ha_lq_msg_prev(struct m0_ha_lq *lq,
						const struct m0_ha_msg *cur)
{
	struct m0_ha_msg_qitem *qitem =
		container_of(cur, struct m0_ha_msg_qitem, hmq_msg);

	M0_PRE(m0_ha_lq_invariant(lq));
	qitem = m0_ha_msg_queue_prev(&lq->hlq_mq, qitem);
	return qitem == NULL ? NULL : &qitem->hmq_msg;
}

M0_INTERNAL bool m0_ha_lq_has_next(const struct m0_ha_lq *lq)
{
	M0_PRE(m0_ha_lq_invariant(lq));

	return lq->hlq_tags.hlt_next < lq->hlq_tags.hlt_assign;
}

M0_INTERNAL bool m0_ha_lq_is_delivered(const struct m0_ha_lq *lq, uint64_t tag)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	M0_PRE(m0_ha_lq_has_tag(lq, tag));

	return tag < lq->hlq_tags.hlt_delivered;
}

M0_INTERNAL uint64_t m0_ha_lq_tag_assign(const struct m0_ha_lq *lq)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	return lq->hlq_tags.hlt_assign;
}

M0_INTERNAL uint64_t m0_ha_lq_tag_next(const struct m0_ha_lq *lq)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	return lq->hlq_tags.hlt_next;
}

M0_INTERNAL uint64_t m0_ha_lq_tag_delivered(const struct m0_ha_lq *lq)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	return lq->hlq_tags.hlt_delivered;
}

M0_INTERNAL uint64_t m0_ha_lq_tag_confirmed(const struct m0_ha_lq *lq)
{
	M0_PRE(m0_ha_lq_invariant(lq));
	return lq->hlq_tags.hlt_confirmed;
}

M0_INTERNAL uint64_t m0_ha_lq_enqueue(struct m0_ha_lq        *lq,
                                      const struct m0_ha_msg *msg)
{
	struct m0_ha_msg_qitem *qitem;
	uint64_t                tag;

	M0_PRE(m0_ha_lq_invariant(lq));
	qitem = m0_ha_msg_queue_alloc(&lq->hlq_mq);
	M0_ASSERT(qitem != NULL);       /* XXX */
	qitem->hmq_msg = *msg;
	tag = m0_ha_msg_tag(msg);
	if (tag == M0_HA_MSG_TAG_UNKNOWN) {
		qitem->hmq_msg.hm_tag     = lq->hlq_tags.hlt_assign;
		qitem->hmq_delivery_state = M0_HA_MSG_QITEM_NOT_DELIVERED;
	} else {
		M0_ASSERT(tag == lq->hlq_tags.hlt_assign);
	}
	lq->hlq_tags.hlt_assign += 2;
	m0_ha_msg_queue_enqueue(&lq->hlq_mq, qitem);
	M0_POST(m0_ha_lq_invariant(lq));
	return m0_ha_msg_tag(&qitem->hmq_msg);
}

M0_INTERNAL struct m0_ha_msg *m0_ha_lq_next(struct m0_ha_lq *lq)
{
	struct m0_ha_msg *msg;

	M0_PRE(m0_ha_lq_invariant(lq));
	if (lq->hlq_tags.hlt_next == lq->hlq_tags.hlt_assign)
		return NULL;
	msg = m0_ha_lq_msg(lq, lq->hlq_tags.hlt_next);
	lq->hlq_tags.hlt_next += 2;
	M0_POST(m0_ha_lq_invariant(lq));
	return msg;
}

M0_INTERNAL bool m0_ha_lq_try_unnext(struct m0_ha_lq *lq)
{
	bool done;

	M0_PRE(m0_ha_lq_invariant(lq));
	done = lq->hlq_tags.hlt_delivered < lq->hlq_tags.hlt_next;
	if (done)
		lq->hlq_tags.hlt_next -= 2;
	M0_POST(m0_ha_lq_invariant(lq));
	return done;
}

M0_INTERNAL void m0_ha_lq_mark_delivered(struct m0_ha_lq *lq, uint64_t tag)
{
	struct m0_ha_msg       *msg;
	struct m0_ha_msg_qitem *qitem;

	M0_PRE(m0_ha_lq_invariant(lq));

	msg = m0_ha_lq_msg(lq, tag);
	M0_ASSERT(msg != NULL);
	M0_ASSERT(lq->hlq_tags.hlt_delivered <= msg->hm_tag);
	M0_ASSERT(msg->hm_tag <= lq->hlq_tags.hlt_next);

	qitem = m0_ha_msg_queue_find(&lq->hlq_mq, tag);
	qitem->hmq_delivery_state = M0_HA_MSG_QITEM_DELIVERED;

	qitem = m0_ha_msg_queue_find(&lq->hlq_mq, lq->hlq_tags.hlt_delivered);
	M0_ASSERT(qitem != NULL);
	for (; qitem != NULL;
	     qitem = m0_ha_msg_queue_next(&lq->hlq_mq, qitem)) {
		if (lq->hlq_tags.hlt_delivered == lq->hlq_tags.hlt_next ||
		    qitem->hmq_delivery_state == M0_HA_MSG_QITEM_NOT_DELIVERED)
			break;

		lq->hlq_tags.hlt_delivered = m0_ha_msg_tag(&qitem->hmq_msg) + 2;
	}
	M0_POST(m0_ha_lq_invariant(lq));
}

M0_INTERNAL uint64_t m0_ha_lq_dequeue(struct m0_ha_lq *lq)
{
	struct m0_ha_msg_qitem *qitem;
	uint64_t                tag;

	M0_PRE(m0_ha_lq_invariant(lq));
	if (lq->hlq_tags.hlt_confirmed < lq->hlq_tags.hlt_delivered) {
		qitem = m0_ha_msg_queue_dequeue(&lq->hlq_mq);
		M0_ASSERT(qitem != NULL);
		tag = m0_ha_msg_tag(&qitem->hmq_msg);
		m0_ha_msg_queue_free(&lq->hlq_mq, qitem);
		M0_ASSERT(tag == lq->hlq_tags.hlt_confirmed);
		lq->hlq_tags.hlt_confirmed += 2;
	} else {
		tag = M0_HA_MSG_TAG_INVALID;
	}
	M0_POST(m0_ha_lq_invariant(lq));
	return tag;
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
