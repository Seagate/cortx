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
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 9-Apr-2013
 */


#pragma once

#ifndef __MERO_RPC_REV_CONN_H__
#define __MERO_RPC_REV_CONN_H__

#include "lib/chan.h"
#include "lib/tlist.h"
#include "rpc/link.h"

/**
   @defgroup rev_conn Reverse connection

   @{
 */

enum {
	M0_REV_CONN_TIMEOUT            = 5,
	M0_REV_CONN_MAX_RPCS_IN_FLIGHT = 1,
};

struct m0_reverse_connection {
	struct m0_rpc_link  rcf_rlink;
	struct m0_tlink     rcf_linkage;
	/* signalled when connection is terminated */
	struct m0_clink     rcf_disc_wait;
	uint64_t            rcf_magic;
};

/** @} end of rev_conn group */

#endif /* __MERO_RPC_REV_CONN_H__ */


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
