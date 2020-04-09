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
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 09/20/2012
 */

#pragma once

#ifndef __MERO_RPC_HELPERS_H__
#define __MERO_RPC_HELPERS_H__

#include "xcode/xcode.h"  /* m0_xcode_what */

struct m0_rpc_item_header2;

/**
 * @addtogroup rpc
 * @{
 */

M0_INTERNAL int m0_rpc_item_header2_encdec(struct m0_rpc_item_header2 *ioh,
					   struct m0_bufvec_cursor    *cur,
					   enum m0_xcode_what          what);

/** @} */

#endif /* __MERO_RPC_HELPERS_H__ */
