/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 10/22/2012
 */

#pragma once

#ifndef __MERO_SNS_CM_UT_CP_COMMON_H__
#define __MERO_SNS_CM_UT_CP_COMMON_H__

#include "ut/ut.h"
#include "sns/cm/cp.h"
#include "sns/cm/ag.h"

extern struct m0_mero sctx;

/* Populates the bufvec with a character value. */
void bv_populate(struct m0_bufvec *b, char data, uint32_t seg_nr,
		 uint32_t seg_size);
void bv_alloc_populate(struct m0_bufvec *b, char data, uint32_t seg_nr,
		       uint32_t seg_size);

/* Compares 2 bufvecs and asserts if not equal. */
void bv_compare(struct m0_bufvec *b1, struct m0_bufvec *b2, uint32_t seg_nr,
		uint32_t seg_size);

void bv_free(struct m0_bufvec *b);

void cp_prepare(struct m0_cm_cp *cp, struct m0_net_buffer *buf,
		uint32_t bv_seg_nr, uint32_t bv_seg_size,
                struct m0_sns_cm_ag *sns_ag, char data,
		struct m0_fom_ops *cp_fom_ops, struct m0_reqh *reqh,
		uint64_t cp_ag_idx, bool is_acc_cp, struct m0_cm *scm);
struct m0_sns_cm *reqh2snscm(struct m0_reqh *reqh);

void layout_gen(struct m0_pdclust_layout **pdlay, struct m0_reqh *reqh);
void layout_destroy(struct m0_pdclust_layout *pdlay);

int cs_init(struct m0_mero *sctx);
int cs_init_with_ad_stob(struct m0_mero *sctx);
void cs_fini(struct m0_mero *sctx);

void pool_mach_transit(struct m0_reqh *reqh, struct m0_poolmach *pm,
			uint64_t fd, enum m0_pool_nd_state state);

#endif /* __MERO_SNS_CM_UT_CP_COMMON_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
