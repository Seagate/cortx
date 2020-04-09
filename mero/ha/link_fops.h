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
 * Original creation date: 18-Apr-2016
 */

#pragma once

#ifndef __MERO_HA_LINK_FOPS_H__
#define __MERO_HA_LINK_FOPS_H__

#include "lib/types.h"          /* m0_uint128 */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */
#include "fop/fop.h"            /* m0_fop_type */
#include "ha/msg.h"             /* m0_ha_msg */

#include "lib/types_xc.h"       /* m0_uint128_xc */
#include "ha/msg_xc.h"          /* m0_ha_msg_xc */

/**
 * @defgroup ha
 *
 * @{
 */

struct m0_ha_link_tags {
	uint64_t hlt_confirmed;
	uint64_t hlt_delivered;
	uint64_t hlt_next;
	uint64_t hlt_assign;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_link_msg_fop {
	uint64_t               lmf_msg_nr;
	struct m0_ha_msg       lmf_msg;
	struct m0_uint128      lmf_id_local;
	struct m0_uint128      lmf_id_remote;
	struct m0_uint128      lmf_id_connection;
	uint64_t               lmf_out_next;
	uint64_t               lmf_in_delivered;
	/** @see m0_ha_link::hln_req_fop_seq */
	uint64_t               lmf_seq;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_link_msg_rep_fop {
	int32_t                lmr_rc;
	uint64_t               lmr_out_next;
	uint64_t               lmr_in_delivered;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_link_params {
	struct m0_uint128      hlp_id_local;
	struct m0_uint128      hlp_id_remote;
	struct m0_uint128      hlp_id_connection;
	struct m0_ha_link_tags hlp_tags_local;
	struct m0_ha_link_tags hlp_tags_remote;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#define HLTAGS_F "(confirmed=%"PRIu64" delivered=%"PRIu64" next=%"PRIu64" "   \
		 "assign=%"PRIu64")"
#define HLTAGS_P(_tags) (_tags)->hlt_confirmed, (_tags)->hlt_delivered,       \
			(_tags)->hlt_next, (_tags)->hlt_assign

M0_INTERNAL void m0_ha_link_tags_initial(struct m0_ha_link_tags *tags,
                                         bool                    tag_even);
M0_INTERNAL bool m0_ha_link_tags_eq(const struct m0_ha_link_tags *tags1,
                                    const struct m0_ha_link_tags *tags2);
M0_INTERNAL void m0_ha_link_params_invert(struct m0_ha_link_params       *dst,
                                          const struct m0_ha_link_params *src);

extern struct m0_fop_type m0_ha_link_msg_fopt;
extern struct m0_fop_type m0_ha_link_msg_rep_fopt;

M0_INTERNAL int  m0_ha_link_fops_init(void);
M0_INTERNAL void m0_ha_link_fops_fini(void);

/** @} end of ha group */
#endif /* __MERO_HA_LINK_FOPS_H__ */

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
