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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 30-Aug-2016
 */
#pragma once
#ifndef __MERO_BE_HA_H__
#define __MERO_BE_HA_H__

#include "lib/types.h"  /* uint8_t */

/**
 * @defgroup be-ha
 *
 * @{
 */

enum m0_be_location {
	M0_BE_LOC_NONE,
	M0_BE_LOC_LOG,
	M0_BE_LOC_SEGMENT_1,
	M0_BE_LOC_SEGMENT_2
};

/**
 * BE I/O error.
 *
 * Payload of m0_ha_msg, which Mero sends to HA in case of BE I/O error.
 */
struct m0_be_io_err {
	uint32_t ber_errcode; /* `int' is not xcodeable */
	uint8_t  ber_location;   /**< @see m0_be_location for values */
	uint8_t  ber_io_opcode;  /**< @see m0_stob_io_opcode for values */
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

/**
 * Sends HA notification about BE I/O error.
 *
 * @note The function never returns.
 */
M0_INTERNAL void m0_be_io_err_send(uint32_t errcode, uint8_t location,
				   uint8_t io_opcode);

/** @} be-ha */
#endif /* __MERO_BE_HA_H__ */
