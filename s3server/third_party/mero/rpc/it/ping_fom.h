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
 * Original author: Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 07/07/2011
 */

#pragma once

#ifndef __MERO_RPC_IT_PING_FOM_H__
#define __MERO_RPC_IT_PING_FOM_H__

#include "rpc/it/ping_fop.h"

/**
 * Object encompassing FOM for ping
 * operation and necessary context data
 */
struct m0_fom_ping {
	/** Generic m0_fom object. */
        struct m0_fom                    fp_gen;
	/** FOP associated with this FOM. */
        struct m0_fop			*fp_fop;
};

/**
 * <b> State Transition function for "ping" operation
 *     that executes on data server. </b>
 *  - Send reply FOP to client.
 */
M0_INTERNAL int m0_fom_ping_state(struct m0_fom *fom);
M0_INTERNAL size_t m0_fom_ping_home_locality(const struct m0_fom *fom);
M0_INTERNAL void m0_fop_ping_fom_fini(struct m0_fom *fom);

/* __MERO_RPC_IT_PING_FOM_H__ */
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
