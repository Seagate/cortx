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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>,
 * Original creation date: 03/23/2011
 */

#pragma once

#ifndef __MERO_NET_NET_OTW_TYPES_H__
#define __MERO_NET_NET_OTW_TYPES_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

/**
 @addtogroup net
 @{
 */

struct m0_net_buf_desc {
	uint32_t  nbd_len;
	uint8_t  *nbd_data;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * In order to provide support for partially filled network buffers this
 * structure can be used. bdd_used stores how much data a network buffer
 * contains. rpc bulk fills this value.
 */
struct m0_net_buf_desc_data {
	struct m0_net_buf_desc bdd_desc;
	uint64_t               bdd_used;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MERO_NET_NET_OTW_TYPES_H__ */

/** @} end of net group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
