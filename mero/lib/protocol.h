/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy.Bilenko <Anatoliy.Bilenko@seagate.com>
 * Original creation date: 24-May-2017
 */

#pragma once

#ifndef __MERO_LIB_PROTOCOL_H__
#define __MERO_LIB_PROTOCOL_H__

/**
 * @defgroup protocol Protocol
 *
 * @{
 */

#include "lib/types.h"
#include "xcode/xcode_attr.h"

/**
 * Protocol version id string which has to coincide on the sides using protocol
 */
struct m0_protocol_id {
	uint8_t p_id[64];
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** @} end of protocol group */
#endif /* __MERO_LIB_PROTOCOL_H__ */

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
