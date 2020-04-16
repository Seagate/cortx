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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/lq.h"
#include "ut/ut.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "fid/fid.h"            /* M0_FID */
#include "ha/msg.h"             /* m0_ha_msg */
#include "ha/link.h"            /* m0_ha_link_tags_initial */

void m0_ha_ut_lq(void)
{
	struct m0_ha_link_tags  tags;
	struct m0_ha_lq_cfg     lq_cfg;
	struct m0_ha_msg       *msg;
	struct m0_ha_msg       *msg2;
	struct m0_ha_lq        *lq;
	uint64_t                tag;
	uint64_t                tag2;
	bool                    success;

	M0_ALLOC_PTR(lq);
	M0_UT_ASSERT(lq != NULL);
	M0_ALLOC_PTR(msg);
	M0_UT_ASSERT(msg != NULL);
	lq_cfg = (struct m0_ha_lq_cfg){
	};
	m0_ha_lq_init(lq, &lq_cfg);
	m0_ha_link_tags_initial(&tags, false);
	m0_ha_lq_tags_set(lq, &tags);
	*msg = (struct m0_ha_msg){
		.hm_fid            = M0_FID_INIT(1, 2),
		.hm_source_process = M0_FID_INIT(3, 4),
		.hm_source_service = M0_FID_INIT(5, 6),
		.hm_time           = 0,
		.hm_data = {
			.hed_type = M0_HA_MSG_STOB_IOQ,
		},
	};
	M0_UT_ASSERT(!m0_ha_lq_has_next(lq));
	tag = m0_ha_lq_enqueue(lq, msg);
	M0_UT_ASSERT(tag <  m0_ha_lq_tag_assign(lq));
	M0_UT_ASSERT(tag == m0_ha_lq_tag_next(lq));
	M0_UT_ASSERT(m0_ha_lq_has_tag(lq, tag));
	msg2 = m0_ha_lq_msg(lq, tag);
	M0_UT_ASSERT(m0_ha_msg_eq(msg, msg2));
	M0_UT_ASSERT(m0_ha_lq_has_next(lq));
	M0_UT_ASSERT(!m0_ha_lq_is_delivered(lq, tag));

	msg2 = m0_ha_lq_next(lq);
	M0_UT_ASSERT(m0_ha_msg_eq(msg, msg2));
	M0_UT_ASSERT(tag <  m0_ha_lq_tag_next(lq));
	M0_UT_ASSERT(tag == m0_ha_lq_tag_delivered(lq));
	M0_UT_ASSERT(m0_ha_lq_has_tag(lq, tag));
	M0_UT_ASSERT(!m0_ha_lq_has_next(lq));
	M0_UT_ASSERT(!m0_ha_lq_is_delivered(lq, tag));

	success = m0_ha_lq_try_unnext(lq);
	M0_UT_ASSERT(success);
	success = m0_ha_lq_try_unnext(lq);
	M0_UT_ASSERT(!success);
	msg2 = m0_ha_lq_next(lq);
	M0_UT_ASSERT(m0_ha_msg_eq(msg, msg2));

	m0_ha_lq_mark_delivered(lq, tag);
	M0_UT_ASSERT(tag <  m0_ha_lq_tag_delivered(lq));
	M0_UT_ASSERT(tag == m0_ha_lq_tag_confirmed(lq));
	M0_UT_ASSERT(m0_ha_lq_has_tag(lq, tag));
	M0_UT_ASSERT(!m0_ha_lq_has_next(lq));
	M0_UT_ASSERT(m0_ha_lq_is_delivered(lq, tag));

	tag2 = m0_ha_lq_dequeue(lq);
	M0_UT_ASSERT(tag2 == tag);
	M0_UT_ASSERT(tag2 < m0_ha_lq_tag_confirmed(lq));
	M0_UT_ASSERT(m0_ha_lq_has_tag(lq, tag));
	M0_UT_ASSERT(!m0_ha_lq_has_next(lq));
	M0_UT_ASSERT(m0_ha_lq_is_delivered(lq, tag));

	m0_ha_lq_fini(lq);
	m0_free(msg);
	m0_free(lq);
}

/*
 '.' -- delivered message
 'x,y,z' -- not delivered

 1) Construct the following queue:
  + m0_ha_link_tags::hlt_delivered
  |
  V
  x..yyy....zzzzzz
 2) Mark 'y'- and 'z'- messages as delivered
  + m0_ha_link_tags::hlt_delivered
  |
  V
  x...............
 3) Mark 'x'- messages as delivered and move m0_ha_link_tags::hlt_delivered
                  + m0_ha_link_tags::hlt_delivered
                  |
                  V
  ................
 */
void m0_ha_ut_lq_mark_delivered(void)
{
	enum { MARK_DELIVERED_LQ_SIZE = 100 };
	struct m0_ha_link_tags  link_tags;
	struct m0_ha_lq_cfg     lq_cfg;
	struct m0_ha_msg       *msg;
	struct m0_ha_lq        *lq;
	uint64_t                tag;
	uint32_t                i;
	uint32_t                lq_size = MARK_DELIVERED_LQ_SIZE;
	uint64_t                tags[MARK_DELIVERED_LQ_SIZE];
	M0_CASSERT(MARK_DELIVERED_LQ_SIZE >= 100);
	M0_CASSERT(MARK_DELIVERED_LQ_SIZE % 2 == 0);


	M0_ALLOC_PTR(lq);
	M0_UT_ASSERT(lq != NULL);
	M0_ALLOC_PTR(msg);
	M0_UT_ASSERT(msg != NULL);
	lq_cfg = (struct m0_ha_lq_cfg){
	};
	m0_ha_lq_init(lq, &lq_cfg);
	m0_ha_link_tags_initial(&link_tags, false);
	m0_ha_lq_tags_set(lq, &link_tags);

	/*  1) Construct lq */
	for (i = 0; i < lq_size; ++i) {
		*msg = (struct m0_ha_msg){
			.hm_fid            = M0_FID_INIT(i, i + 1),
			.hm_source_process = M0_FID_INIT(3, 4),
			.hm_source_service = M0_FID_INIT(5, 6),
			.hm_time           = 0,
			.hm_data = {
				.hed_type = M0_HA_MSG_STOB_IOQ,
			},
		};
		M0_UT_ASSERT(!m0_ha_lq_has_next(lq));
		tag = m0_ha_lq_enqueue(lq, msg);
		M0_UT_ASSERT(tag <  m0_ha_lq_tag_assign(lq));
		M0_UT_ASSERT(tag == m0_ha_lq_tag_next(lq));
		M0_UT_ASSERT(m0_ha_lq_has_tag(lq, tag));
		M0_UT_ASSERT(m0_ha_lq_has_next(lq));
		M0_UT_ASSERT(!m0_ha_lq_is_delivered(lq, tag));

		(void)m0_ha_lq_next(lq);
		M0_UT_ASSERT(tag <  m0_ha_lq_tag_next(lq));
		M0_UT_ASSERT(m0_ha_lq_has_tag(lq, tag));
		M0_UT_ASSERT(!m0_ha_lq_has_next(lq));
		M0_UT_ASSERT(!m0_ha_lq_is_delivered(lq, tag));

		tags[i] = tag;
		if (!((i >= lq_size/10 && i < lq_size/10 + lq_size/20) ||
		      (i >= lq_size/4  && i < lq_size/4  + lq_size/10) ||
		      (i >= lq_size/2  && i < lq_size/2  + lq_size/5)))
			m0_ha_lq_mark_delivered(lq, tag);

		M0_ASSERT(ergo(i < lq_size/10,
			       tag == m0_ha_lq_tag_delivered(lq)-2));
		M0_ASSERT(ergo(i > lq_size/10,
			       tag != m0_ha_lq_tag_delivered(lq)-2));
	}

	/* 2) Mark some messages as delivered */
	for (i = 0; i < lq_size; ++i) {
		tag = tags[i];
		M0_UT_ASSERT(m0_ha_lq_tag_delivered(lq) == tags[lq_size/10]);

		if ((i >= lq_size/4  && i < lq_size/4  + lq_size/10) ||
		    (i >= lq_size/2 && i < lq_size/2 + lq_size/5))
			m0_ha_lq_mark_delivered(lq, tag);

		M0_UT_ASSERT(m0_ha_lq_tag_delivered(lq) == tags[lq_size/10]);
	}

	/*  3) Mark all messages as delivered and move hlt_delivered */
	for (i = 0; i < lq_size; ++i) {
		tag = tags[i];

		M0_ASSERT(ergo(i < lq_size/10, tags[lq_size/10] ==
					       m0_ha_lq_tag_delivered(lq)));
		M0_ASSERT(ergo(i >= lq_size/10 && i < lq_size/4 - lq_size/10,
			       m0_ha_lq_tag_delivered(lq) == tags[i]));
		M0_ASSERT(ergo(i >= lq_size/4 - lq_size/10,
			       m0_ha_lq_tag_delivered(lq)-2 ==
			       tags[lq_size-1]));

		if ((i >= lq_size/10 && i < lq_size/10 + lq_size/20))
			m0_ha_lq_mark_delivered(lq, tag);

		M0_ASSERT(ergo(i < lq_size/10, tags[lq_size/10] ==
					       m0_ha_lq_tag_delivered(lq)));
		M0_ASSERT(ergo(i >= lq_size/10 && i < lq_size/4 - lq_size/10 - 1,
			       m0_ha_lq_tag_delivered(lq)-2 == tags[i]));
		M0_ASSERT(ergo(i >= lq_size/4 - lq_size/10 - 1,
			       m0_ha_lq_tag_delivered(lq)-2 ==
			       tags[lq_size-1]));
	}

	for (i = 0; i < lq_size; ++i) {
		tag = m0_ha_lq_dequeue(lq);
		M0_UT_ASSERT(m0_ha_lq_has_tag(lq, tag));
		M0_UT_ASSERT(!m0_ha_lq_has_next(lq));
		M0_UT_ASSERT(m0_ha_lq_is_delivered(lq, tag));
	}

	m0_ha_lq_fini(lq);
	m0_free(msg);
	m0_free(lq);
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
