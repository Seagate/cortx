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
 * Original creation date: 25-Jun-2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "lib/errno.h"
#include "rpc/onwire.h"
#include "rpc/onwire_xc.h"
#include "rpc/item.h"          /* m0_rpc_item_header2 */
#include "rpc/rpc_helpers.h"
#include "xcode/xcode.h"       /* M0_XCODE_OBJ */

/**
 * @addtogroup rpc
 * @{
 */

#define ITEM_HEAD1_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_header1_xc, ptr)
#define ITEM_HEAD2_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_header2_xc, ptr)
#define ITEM_FOOTER_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_item_footer_xc, ptr)

M0_INTERNAL int m0_rpc_item_header1_encdec(struct m0_rpc_item_header1 *ioh,
					   struct m0_bufvec_cursor    *cur,
					   enum m0_xcode_what          what)
{
	M0_ENTRY("item header1: %p", ioh);
	return M0_RC(m0_xcode_encdec(&ITEM_HEAD1_XCODE_OBJ(ioh), cur, what));
}

M0_INTERNAL int m0_rpc_item_header2_encdec(struct m0_rpc_item_header2 *ioh,
					   struct m0_bufvec_cursor    *cur,
					   enum m0_xcode_what          what)
{
	M0_ENTRY("item header2: %p", ioh);
	return M0_RC(m0_xcode_encdec(&ITEM_HEAD2_XCODE_OBJ(ioh), cur, what));
}

M0_INTERNAL int m0_rpc_item_footer_encdec(struct m0_rpc_item_footer *iof,
					  struct m0_bufvec_cursor   *cur,
					  enum m0_xcode_what         what)
{
	M0_ENTRY("item footer: %p", iof);
	return M0_RC(m0_xcode_encdec(&ITEM_FOOTER_XCODE_OBJ(iof), cur, what));
}

#undef M0_TRACE_SUBSYSTEM

/** @} */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
