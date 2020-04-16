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
 * Original author: Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
 * Original creation date: 22-June-2016
 */


#pragma once

#ifndef __MERO_HA_FAILVEC_H__
#define __MERO_HA_FAILVEC_H__


#include "lib/tlist.h"         /* m0_tl */
#include "lib/mutex.h"         /* m0_mutex */
#include "ha/dispatcher.h"     /* m0_mero_ha_handler */
#include "lib/cookie.h"        /* m0_cookie */
#include "ha/note.h"           /* M0_HA_STATE_UPDATE_LIMIT */
#include "xcode/xcode_attr.h"

#include "ha/note_xc.h"
#include "lib/cookie_xc.h"

/* Forward declarations. */
struct m0_cookie;
struct m0_poolmach;
struct m0_fid;
struct m0_chan;
struct m0_ha_dispatcher;

struct m0_ha_msg_failure_vec_req {
	struct m0_fid    mfq_pool;
	struct m0_cookie mfq_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_msg_failure_vec_rep {
	struct m0_fid               mfp_pool;
	struct m0_cookie            mfp_cookie;
	uint32_t                    mfp_nr;
	struct m0_ha_msg_nvec_array mfp_vec;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_fvec_handler {
	struct m0_tl             hfh_fvreq;
	struct m0_mutex          hfh_lock;
	struct m0_ha_handler     hfh_handler;
	struct m0_ha_dispatcher *hfh_dispatcher;
};

M0_INTERNAL int m0_ha_fvec_handler_init(struct m0_ha_fvec_handler *hfh,
					struct m0_ha_dispatcher *hd);

M0_INTERNAL void m0_ha_fvec_handler_fini(struct m0_ha_fvec_handler *hfh);

M0_INTERNAL int m0_ha_fvec_handler_add(struct m0_ha_fvec_handler *hfh,
                                       const struct m0_fid *pool_fid,
				       struct m0_poolmach *pool_mach,
				       struct m0_chan *chan,
				       struct m0_cookie *cookie);
/** Sends an HA message to store/fetch the failure vector from HA. */
M0_INTERNAL int m0_ha_msg_fvec_send(const struct m0_fid *pool_fid,
				    const struct m0_cookie *req_cookie,
				    struct m0_ha_link *hl,
				    uint32_t type);

/** Handles requests to fetch failure vector. */
M0_INTERNAL void m0_ha_fvec_req_handler(struct m0_ha_fvec_handler *hfh,
					const struct m0_ha_msg *msg,
					struct m0_ha_link *hl);

/** Applies the received failure vector to respective pool-machine. */
M0_INTERNAL void m0_ha_fvec_rep_handler(struct m0_ha_fvec_handler *hfh,
					const struct m0_ha_msg *msg);

/** Requests for the failure vector for the given pool machine. */
M0_INTERNAL int m0_ha_failvec_fetch(const struct m0_fid *pool_fid,
				    struct m0_poolmach *pmach,
				    struct m0_chan *chan);

#endif /*  __MERO_HA_FAILVEC_H__ */

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
