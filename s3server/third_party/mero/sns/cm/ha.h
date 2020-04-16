/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Ankit Yadav <ankit.yadav@seagate.com>
 * Original creation date: 21-Jan-2019
 */

#pragma once

#ifndef __MERO_SNS_CM_HA_H__
#define __MERO_SNS_CM_HA_H__

#include "lib/types.h"  /* uint8_t */

/**
 * SNS error.
 *
 * Payload of m0_ha_msg, which Mero sends to HA in case of cm failure.
 */
struct m0_ha_sns_err {
        uint32_t hse_errcode; /* `int` is not xcodeable */
        uint8_t  hse_opcode;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


#endif /* __MERO_SNS_CM_HA_H__ */


