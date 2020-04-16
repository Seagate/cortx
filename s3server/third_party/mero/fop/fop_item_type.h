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
 * Original author: Subhash Arya <subhash_arya@xyratex.com>
 * Original creation date: 11/18/2011
 */

#pragma once

#ifndef __MERO_FOP_FOP_ONWIRE_H__
#define __MERO_FOP_FOP_ONWIRE_H__

#include "rpc/rpc_helpers.h"

struct m0_rpc_item;
struct m0_rpc_item_type;

/**
   @addtogroup fop

   This file contains definitions for encoding/decoding a fop type rpc item
   onto a bufvec.
	@see rpc/onwire.h
   @{
 */
/**
   Generic bufvec serialization routine for a fop rpc item type.
   @param item_type Pointer to the item type struct for the item.
   @param item  pointer to the item which is to be serialized.
   @param cur current position of the cursor in the bufvec.
   @retval 0 On success.
   @retval -errno On failure.
*/
M0_INTERNAL int
m0_fop_item_type_default_encode(const struct m0_rpc_item_type *item_type,
				struct m0_rpc_item *item,
				struct m0_bufvec_cursor *cur);

/**
   Generic deserialization routine for a fop rpc item type. Allocates a new rpc
   item and decodes the header and the payload into this item.
   @param item_type Pointer to the item type struct for the item.
   @param item Pointer to the item containing deserialized rpc onwire data and
   payload.
   @param cur current position of the cursor in the bufvec.
   @retval 0 On success.
   @retval -errno if failure.
*/
M0_INTERNAL int
m0_fop_item_type_default_decode(const struct m0_rpc_item_type *item_type,
				struct m0_rpc_item **item_out,
				struct m0_bufvec_cursor *cur);

/**
   Return the onwire size of the item type which is a fop in bytes.
   The onwire size of an item equals = size of (header + payload).
   @param item The rpc item for which the on wire size is to be calculated
   @retval Size of the item in bytes.
*/
M0_INTERNAL m0_bcount_t m0_fop_payload_size(const struct m0_rpc_item *item);

M0_INTERNAL int m0_fop_item_encdec(struct m0_rpc_item *item,
				   struct m0_bufvec_cursor *cur,
				   enum m0_xcode_what  what);

void m0_fop_item_get(struct m0_rpc_item *item);
void m0_fop_item_put(struct m0_rpc_item *item);

/** @} end of fop group */

/* __MERO_FOP_FOP_ONWIRE_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
