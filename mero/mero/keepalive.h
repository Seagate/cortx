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
 * Original creation date: 17-Jun-2016
 */

#pragma once

#ifndef __MERO_MERO_KEEPALIVE_H__
#define __MERO_MERO_KEEPALIVE_H__

/**
 * @defgroup mero-keepalive
 *
 * @{
 */

#include "lib/types.h"          /* m0_uint128 */
#include "lib/atomic.h"         /* m0_atomic64 */
#include "ha/dispatcher.h"      /* m0_ha_handler */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */

#include "lib/types_xc.h"       /* m0_uint128_xc */

struct m0_ha_msg_keepalive_req {
	struct m0_uint128 kaq_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_msg_keepalive_rep {
	struct m0_uint128 kap_id;
	uint64_t          kap_counter;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_keepalive_handler {
	struct m0_ha_dispatcher *kah_dispatcher;
	struct m0_ha_handler     kah_handler;
	struct m0_atomic64       kah_counter;
};

M0_INTERNAL int
m0_ha_keepalive_handler_init(struct m0_ha_keepalive_handler *ka,
                             struct m0_ha_dispatcher        *hd);
M0_INTERNAL void
m0_ha_keepalive_handler_fini(struct m0_ha_keepalive_handler *ka);

/** @} end of mero-keepalive group */
#endif /* __MERO_MERO_KEEPALIVE_H__ */

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
