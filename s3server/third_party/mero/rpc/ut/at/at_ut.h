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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 25-May-2016
 */

#pragma once

#ifndef __MERO_RPC_UT_AT_AT_UT_H__
#define __MERO_RPC_UT_AT_AT_UT_H__

#include "xcode/xcode_attr.h"
#include "lib/types.h"
#include "rpc/at.h"
#include "rpc/at_xc.h"

/* import */
struct m0_rpc_machine;

enum {
	DATA_PATTERN     = 0x0a,
	INLINE_LEN       = 70,
	INBULK_THRESHOLD = 4096,
	INBULK_LEN       = 32 * INBULK_THRESHOLD,
};

enum {
	/* Tests oriented on sending data to server. */
	AT_TEST_INLINE_SEND,
	AT_TEST_INBULK_SEND,

	/*
	 * Tests oriented on receiving data from server.
	 * Note, AT_TEST_INLINE_RECV should be the first.
	 */
	AT_TEST_INLINE_RECV,
	AT_TEST_INLINE_RECV_UNK,
	AT_TEST_INBULK_RECV_UNK,
	AT_TEST_INBULK_RECV,
};

M0_INTERNAL void atut__bufdata_alloc(struct m0_buf *buf, size_t size,
				     struct m0_rpc_machine *rmach);

struct atut__req {
	uint32_t             arq_test_id;
	struct m0_rpc_at_buf arq_buf;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct atut__rep {
	uint32_t             arp_rc;
	struct m0_rpc_at_buf arp_buf;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MERO_RPC_UT_AT_AT_UT_H__ */

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
