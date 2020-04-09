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
 * Original author: Igor Perelyotov <igor.m.perelyotov@seagate.com>
 * Original creation date: 18-Aug-2016
 */

#pragma once

#ifndef __MERO_RPC_HA_H__
#define __MERO_RPC_HA_H__

/**
 * @defgroup rpc-ha
 *
 * @{
 */
#include "lib/types.h"          /* uint64_t */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */

struct m0_ha_msg_rpc {
	/** Indicates how many attempts to notify HA were made */
	uint64_t hmr_attempts;
	/** @see m0_ha_obj_state for values */
	uint64_t hmr_state;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MERO_RPC_HA_H__ */
