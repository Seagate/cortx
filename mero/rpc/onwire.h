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
 * Original author: Subhash Arya <subhash_arya@xyratex.com>
 * Original creation date: 06/25/2011
 */

#pragma once

#ifndef __MERO_RPC_ONWIRE_H__
#define __MERO_RPC_ONWIRE_H__

#include "lib/types.h"        /* uint64_t */
#include "lib/types_xc.h"     /* m0_uint128_xc */
#include "lib/cookie.h"
#include "lib/cookie_xc.h"
#include "xcode/xcode_attr.h" /* M0_XCA_RECORD */
#include "format/format.h"    /* struct m0_format_header */
#include "format/format_xc.h"

/**
 * @addtogroup rpc
 * @{
 */

enum {
	M0_RPC_VERSION_1 = 1,
};

enum {
	/*
	 * Version of RPC packet
	 */

	M0_RPC_PACKET_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_RPC_PACKET_FORMAT_VERSION */
	/*M0_RPC_PACKET_FORMAT_VERSION_2,*/
	/*M0_RPC_PACKET_FORMAT_VERSION_3,*/

	/**
	 * Current version, should point to the latest
	 * M0_RPC_PACKET_FORMAT_VERSION_*
	 */
	M0_RPC_PACKET_FORMAT_VERSION = M0_RPC_PACKET_FORMAT_VERSION_1,

	/*
	 * Version of RPC item
	 */

	M0_RPC_ITEM_FORMAT_VERSION_1 = 1,
	M0_RPC_ITEM_FORMAT_VERSION   = M0_RPC_ITEM_FORMAT_VERSION_1,
};

struct m0_rpc_packet_onwire_header {
	struct m0_format_header poh_header;
	/* Version */
	uint32_t                poh_version;
	/** Number of RPC items in packet */
	uint32_t                poh_nr_items;
	uint64_t                poh_magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_packet_onwire_footer {
	struct m0_format_footer pof_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_item_header1 {
	struct m0_format_header ioh_header;
	/** Item opcode */
	uint32_t                ioh_opcode;
	/** Item flags, taken from enum m0_rpc_item_flags. */
	uint32_t                ioh_flags;
	/** HA epoch transferred by the item. */
	uint64_t                ioh_ha_epoch;
	uint64_t                ioh_magic;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_item_header2 {
	struct m0_uint128 osr_uuid;
	uint64_t          osr_sender_id;
	uint64_t          osr_session_id;
	uint64_t          osr_xid;
	struct m0_cookie  osr_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rpc_item_footer {
	struct m0_format_footer iof_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL int m0_rpc_item_header1_encdec(struct m0_rpc_item_header1 *ioh,
					   struct m0_bufvec_cursor *cur,
					   enum m0_xcode_what what);
M0_INTERNAL int m0_rpc_item_footer_encdec (struct m0_rpc_item_footer *iof,
					   struct m0_bufvec_cursor *cur,
					   enum m0_xcode_what what);

/** @}  End of rpc group */

#endif /* __MERO_RPC_ONWIRE_H__ */
