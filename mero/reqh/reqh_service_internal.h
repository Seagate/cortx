/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 13-Mar-2017
 */

#pragma once

#ifndef __MERO_REQH_REQH_SERVICE_INTERNAL_H__
#define __MERO_REQH_REQH_SERVICE_INTERNAL_H__

#include "reqh/reqh_service.h"

enum {
	M0_RSC_OFFLINE,
	M0_RSC_ONLINE,
	M0_RSC_CONNECTING,
	M0_RSC_DISCONNECTING,
	M0_RSC_CANCELLED,
};

enum {
	/* Is set after `sc_rlink` initialisation. */
	M0_RSC_RLINK_INITED,
	/*
	 * Is set when m0_reqh_service_connect() is called. Remains set
	 * after m0_reqh_service_disconnect().
	 */
	M0_RSC_RLINK_CONNECT,
	M0_RSC_RLINK_DISCONNECT,
	/* Is set when rpc session can not be cancelled immediately. */
	M0_RSC_RLINK_CANCEL,
	/* Is set when urgent link termination requested. */
	M0_RSC_RLINK_ABORT,
};

#define CTX_STATE(ctx) (ctx)->sc_sm.sm_state

static inline void reqh_service_ctx_sm_lock(struct m0_reqh_service_ctx *ctx)
{
	m0_sm_group_lock(&ctx->sc_sm_grp);
}

static inline void reqh_service_ctx_sm_unlock(struct m0_reqh_service_ctx *ctx)
{
	m0_sm_group_unlock(&ctx->sc_sm_grp);
}

static inline bool
reqh_service_ctx_sm_is_locked(const struct m0_reqh_service_ctx *ctx)
{
	return m0_sm_group_is_locked(&ctx->sc_sm_grp);
}

static inline void
reqh_service_ctx_state_move(struct m0_reqh_service_ctx *ctx, int  state)
{
	m0_sm_state_set(&ctx->sc_sm, state);
}

static inline void
reqh_service_ctx_flag_set(struct m0_reqh_service_ctx *ctx, int  flag)
{
	ctx->sc_conn_flags |= M0_BITS(flag);
}

static inline void
reqh_service_ctx_flag_clear(struct m0_reqh_service_ctx *ctx, int  flag)
{
	ctx->sc_conn_flags &= ~M0_BITS(flag);
}

static inline bool
reqh_service_ctx_flag_is_set(const struct m0_reqh_service_ctx *ctx, int flag)
{
	return !!(ctx->sc_conn_flags & M0_BITS(flag));
}

#endif /* __MERO_REQH_REQH_SERVICE_INTERNAL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
