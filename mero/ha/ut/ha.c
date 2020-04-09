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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 26-Apr-2016
 */


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/ha.h"
#include "ut/ut.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/mutex.h"          /* m0_mutex */
#include "lib/chan.h"           /* m0_clink */

#include "ha/entrypoint.h"      /* M0_HEC_AVAILABLE */
#include "ha/ut/helper.h"       /* m0_ha_ut_rpc_ctx */

struct ha_ut_usecase_ctx {
	struct m0_mutex    huc_lock;
	struct m0_ha       huc_ha;
	struct m0_ha_link *huc_link_in1;
	struct m0_ha_link *huc_link_in2;
	struct m0_ha_link *huc_link_out;
	int                huc_entrypoint_request_nr;
	int                huc_entrypoint_replied_nr;
	int                huc_msg_received_nr;
	int                huc_msg_is_delivered_nr;
	int                huc_msg_is_not_delivered_nr;
	int                huc_link_connected_nr;
	int                huc_link_reused_nr;
	int                huc_link_is_disconnecting_nr;
	int                huc_link_disconnected_nr;
};

struct ha_ut_wait_ctx {
	struct m0_ha                   *huw_ha;
	struct m0_ha_entrypoint_client *huw_entrypoint_client;
	struct m0_clink                 huw_clink;
};

static struct ha_ut_usecase_ctx *ha_ut_ha2usecase_ctx(struct m0_ha *ha)
{
	return container_of(ha, struct ha_ut_usecase_ctx, huc_ha);
}

static void ha_ut_usecase_link_check(struct ha_ut_usecase_ctx *huc,
                                     struct m0_ha_link        *hl)
{
	m0_mutex_lock(&huc->huc_lock);
	M0_UT_ASSERT(huc->huc_link_in1 == NULL ||
		     huc->huc_link_in2 == NULL ||
		     M0_IN(hl, (huc->huc_link_in1, huc->huc_link_in2, NULL)));
	if (huc->huc_link_in1 == NULL) {
		huc->huc_link_in1 = hl;
	} else if (huc->huc_link_in1 != hl && huc->huc_link_in2 != hl) {
		M0_UT_ASSERT(huc->huc_link_in2 == NULL);
		huc->huc_link_in2 = hl;
	}
	m0_mutex_unlock(&huc->huc_lock);
}

static void ha_ut_usecase_entrypoint_request
				(struct m0_ha                      *ha,
				 const struct m0_ha_entrypoint_req *req,
				 const struct m0_uint128           *req_id)
{
	struct m0_ha_entrypoint_rep  rep;
	struct ha_ut_usecase_ctx    *huc = ha_ut_ha2usecase_ctx(ha);
	struct m0_ha_link           *hl;

	M0_UT_ASSERT(huc->huc_entrypoint_request_nr == 0);
	++huc->huc_entrypoint_request_nr;

	M0_UT_ASSERT(m0_fid_eq(&M0_FID_TINIT('r', 1, 2),
			       &req->heq_process_fid));
	rep = (struct m0_ha_entrypoint_rep){
		.hae_quorum        = 1,
		.hae_confd_fids    = {},
		.hae_confd_eps     = NULL,
		.hae_active_rm_fid = M0_FID_INIT(5, 6),
		.hae_active_rm_ep  = NULL,
		.hae_control       = M0_HA_ENTRYPOINT_CONSUME,
	};
	m0_ha_entrypoint_reply(ha, req_id, &rep, &hl);
	ha_ut_usecase_link_check(huc, hl);
}

static void ha_ut_usecase_entrypoint_replied(struct m0_ha                *ha,
                                             struct m0_ha_entrypoint_rep *rep)
{
	struct ha_ut_usecase_ctx    *huc = ha_ut_ha2usecase_ctx(ha);

	M0_UT_ASSERT(huc->huc_entrypoint_replied_nr == 0);
	++huc->huc_entrypoint_replied_nr;

	M0_UT_ASSERT(rep->hae_control == M0_HA_ENTRYPOINT_CONSUME);
	M0_UT_ASSERT(rep->hae_quorum == 1);
	M0_UT_ASSERT(m0_fid_eq(&M0_FID_INIT(5, 6), &rep->hae_active_rm_fid));
}

static void ha_ut_usecase_msg_received(struct m0_ha      *ha,
                                       struct m0_ha_link *hl,
                                       struct m0_ha_msg  *msg,
                                       uint64_t           tag)
{
}

static void ha_ut_usecase_msg_is_delivered(struct m0_ha      *ha,
                                           struct m0_ha_link *hl,
                                           uint64_t           tag)
{
}

static void ha_ut_usecase_msg_is_not_delivered(struct m0_ha      *ha,
                                               struct m0_ha_link *hl,
                                               uint64_t           tag)
{
}

static void ha_ut_usecase_link_connected(struct m0_ha            *ha,
                                         const struct m0_uint128 *req_id,
                                         struct m0_ha_link       *hl)
{
	struct ha_ut_usecase_ctx    *huc = ha_ut_ha2usecase_ctx(ha);

	M0_UT_ASSERT(huc->huc_link_connected_nr == 0);
	++huc->huc_link_connected_nr;
	ha_ut_usecase_link_check(huc, hl);
}

static void ha_ut_usecase_link_reused(struct m0_ha            *ha,
                                      const struct m0_uint128 *req_id,
                                      struct m0_ha_link       *hl)
{
	struct ha_ut_usecase_ctx    *huc = ha_ut_ha2usecase_ctx(ha);

	M0_UT_ASSERT(huc->huc_link_reused_nr == 0);
	++huc->huc_link_reused_nr;
	ha_ut_usecase_link_check(huc, hl);
}

static void ha_ut_usecase_link_is_disconnecting(struct m0_ha      *ha,
                                                struct m0_ha_link *hl)
{
	struct ha_ut_usecase_ctx    *huc = ha_ut_ha2usecase_ctx(ha);

	M0_UT_ASSERT(M0_IN(huc->huc_link_is_disconnecting_nr, (0, 1)));
	++huc->huc_link_is_disconnecting_nr;

	m0_ha_disconnect_incoming(ha, hl);
}

static void ha_ut_usecase_link_disconnected(struct m0_ha      *ha,
                                            struct m0_ha_link *hl)
{
	struct ha_ut_usecase_ctx    *huc = ha_ut_ha2usecase_ctx(ha);

	M0_UT_ASSERT(M0_IN(huc->huc_link_disconnected_nr, (0, 1)));
	++huc->huc_link_disconnected_nr;

	m0_mutex_lock(&huc->huc_lock);
	M0_UT_ASSERT(M0_IN(hl, (huc->huc_link_in1, huc->huc_link_in2)));
	huc->huc_link_in1 = hl == huc->huc_link_in1 ? NULL : huc->huc_link_in1;
	huc->huc_link_in2 = hl == huc->huc_link_in2 ? NULL : huc->huc_link_in2;
	m0_mutex_unlock(&huc->huc_lock);
}

static bool ha_ut_ha_wait_available(struct m0_clink *clink)
{
	struct ha_ut_wait_ctx *huw;

	huw = container_of(clink, struct ha_ut_wait_ctx, huw_clink);
	return huw->huw_entrypoint_client->ecl_sm.sm_state != M0_HEC_AVAILABLE;
}

static const struct m0_ha_ops ha_ut_usecase_ha_ops = {
	.hao_entrypoint_request    = ha_ut_usecase_entrypoint_request,
	.hao_entrypoint_replied    = ha_ut_usecase_entrypoint_replied,
	.hao_msg_received          = ha_ut_usecase_msg_received,
	.hao_msg_is_delivered      = ha_ut_usecase_msg_is_delivered,
	.hao_msg_is_not_delivered  = ha_ut_usecase_msg_is_not_delivered,
	.hao_link_connected        = ha_ut_usecase_link_connected,
	.hao_link_reused           = ha_ut_usecase_link_reused,
	.hao_link_is_disconnecting = ha_ut_usecase_link_is_disconnecting,
	.hao_link_disconnected     = ha_ut_usecase_link_disconnected,
};

void m0_ha_ut_ha_usecase(void)
{
	struct ha_ut_usecase_ctx *ctx;
	struct m0_ha_ut_rpc_ctx  *rpc_ctx;
	struct ha_ut_wait_ctx     wait_ctx;
	struct m0_ha_msg         *msg;
	struct m0_ha_cfg          ha_cfg;
	struct m0_clink          *clink;
	struct m0_ha             *ha;
	int                       rc;

	M0_ALLOC_PTR(msg);
	M0_UT_ASSERT(msg != NULL);
	M0_ALLOC_PTR(rpc_ctx);
	M0_UT_ASSERT(rpc_ctx != NULL);
	m0_ha_ut_rpc_ctx_init(rpc_ctx);
	M0_ALLOC_PTR(ctx);
	M0_UT_ASSERT(ctx != NULL);
	m0_mutex_init(&ctx->huc_lock);
	ha = &ctx->huc_ha;

	ha_cfg = (struct m0_ha_cfg){
		.hcf_ops         = ha_ut_usecase_ha_ops,
		.hcf_rpc_machine = &rpc_ctx->hurc_rpc_machine,
		.hcf_reqh        = &rpc_ctx->hurc_reqh,
		.hcf_addr       = m0_rpc_machine_ep(&rpc_ctx->hurc_rpc_machine),
		.hcf_process_fid = M0_FID_TINIT('r', 1, 2),
	};
	rc = m0_ha_init(ha, &ha_cfg);
	M0_UT_ASSERT(rc == 0);
	rc = m0_ha_start(ha);
	M0_UT_ASSERT(rc == 0);

	ctx->huc_link_out = m0_ha_connect(ha);
	M0_UT_ASSERT(ctx->huc_link_out != NULL);
	M0_UT_ASSERT(ctx->huc_entrypoint_request_nr == 1);
	M0_UT_ASSERT(ctx->huc_entrypoint_replied_nr == 1);
	M0_UT_ASSERT(ctx->huc_link_connected_nr     == 1);
	ctx->huc_entrypoint_request_nr = 0;
	ctx->huc_entrypoint_replied_nr = 0;
	ctx->huc_link_connected_nr     = 0;

	/* reconnect: both are alive */
	wait_ctx = (struct ha_ut_wait_ctx){
		.huw_ha                = ha,
		.huw_entrypoint_client = &ha->h_entrypoint_client,
	};
	clink = &wait_ctx.huw_clink;
	m0_clink_init(clink, &ha_ut_ha_wait_available);
	m0_clink_add_lock(m0_ha_entrypoint_client_chan(
	                                        &ha->h_entrypoint_client),
	                  clink);
	m0_ha_entrypoint_client_request(&ha->h_entrypoint_client);
	m0_chan_wait(clink);
	m0_clink_del_lock(clink);
	m0_clink_fini(clink);
	M0_UT_ASSERT(ctx->huc_entrypoint_request_nr == 1);
	M0_UT_ASSERT(ctx->huc_entrypoint_replied_nr == 1);
	M0_UT_ASSERT(ctx->huc_link_reused_nr        == 1);
	ctx->huc_entrypoint_request_nr = 0;
	ctx->huc_entrypoint_replied_nr = 0;
	ctx->huc_link_reused_nr        = 0;

	/* reconnect: m0d reconnect case */
	m0_ha_disconnect(ha);
	ctx->huc_link_out = m0_ha_connect(ha);
	m0_ha_disconnect(ha);
	M0_UT_ASSERT(ctx->huc_entrypoint_request_nr    == 1);
	M0_UT_ASSERT(ctx->huc_entrypoint_replied_nr    == 1);
	M0_UT_ASSERT(ctx->huc_link_connected_nr        == 1);
	M0_UT_ASSERT(ctx->huc_link_reused_nr           == 0);

	m0_ha_stop(ha);
	M0_UT_ASSERT(ctx->huc_link_is_disconnecting_nr == 2);
	M0_UT_ASSERT(ctx->huc_link_disconnected_nr     == 2);
	m0_ha_fini(ha);

	M0_UT_ASSERT(ctx->huc_link_in1 == NULL);
	M0_UT_ASSERT(ctx->huc_link_in2 == NULL);
	m0_mutex_fini(&ctx->huc_lock);
	m0_free(ctx);
	m0_ha_ut_rpc_ctx_fini(rpc_ctx);
	m0_free(rpc_ctx);
	m0_free(msg);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
