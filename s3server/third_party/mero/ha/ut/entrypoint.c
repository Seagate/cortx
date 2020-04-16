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
 *                  Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 28-Apr-2016
 */


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/entrypoint.h"
#include "ut/ut.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/types.h"          /* uint32_t */
#include "lib/string.h"         /* m0_strdup */
#include "lib/arith.h"          /* m0_rnd64 */
#include "lib/misc.h"           /* container_of */
#include "fid/fid.h"            /* m0_fid */
#include "ha/ut/helper.h"       /* m0_ha_ut_rpc_ctx */

enum {
	HA_UT_ENTRYPOINT_USECASE_CONFD_NR = 0x101,
};

struct ha_ut_entrypoint_usecase_ctx {
	struct m0_ha_entrypoint_server   ueus_server;
	struct m0_ha_entrypoint_client   ueus_client;
	enum m0_ha_entrypoint_control    ueus_control;
	uint32_t                         ueus_quorum;
	uint32_t                         ueus_confd_nr;
	struct m0_fid                   *ueus_confd_fids;
	char                           **ueus_confd_endpoints;
	char                            *ueus_rm_ep;
	struct m0_fid                    ueus_rm_fid;
	uint32_t                         ueus_req_count;
	struct m0_clink                  ueus_clink;
};

static void ha_ut_entrypoint_request_arrived(
		 struct m0_ha_entrypoint_server    *hes,
		 const struct m0_ha_entrypoint_req *req,
		 const struct m0_uint128           *req_id)
{
	struct ha_ut_entrypoint_usecase_ctx *uctx;
	struct m0_ha_entrypoint_rep         *rep;

	uctx = container_of(hes, struct ha_ut_entrypoint_usecase_ctx,
			    ueus_server);
	M0_ALLOC_PTR(rep);
	*rep = (struct m0_ha_entrypoint_rep){
		.hae_quorum     = uctx->ueus_quorum,
		.hae_confd_fids = {
			.af_count =  uctx->ueus_confd_nr,
			.af_elems =  uctx->ueus_confd_fids,
		},
		.hae_confd_eps     = (const char **)uctx->ueus_confd_endpoints,
		.hae_active_rm_fid = uctx->ueus_rm_fid,
		.hae_active_rm_ep  = uctx->ueus_rm_ep,
		.hae_control       = uctx->ueus_control,
	};
	++uctx->ueus_req_count;
	m0_ha_entrypoint_server_reply(hes, req_id, rep);
	m0_free(rep);
}

static void
ha_ut_entrypoint_reply_init(struct ha_ut_entrypoint_usecase_ctx *uctx)
{
	uint64_t seed = 42;
	char     buf[0x100];
	int      rc;
	int      i;

	uctx->ueus_control  = M0_HA_ENTRYPOINT_CONSUME;
	uctx->ueus_confd_nr = HA_UT_ENTRYPOINT_USECASE_CONFD_NR;
	uctx->ueus_quorum   = HA_UT_ENTRYPOINT_USECASE_CONFD_NR / 2 + 1;
	M0_ALLOC_ARR(uctx->ueus_confd_fids,      uctx->ueus_confd_nr);
	M0_ALLOC_ARR(uctx->ueus_confd_endpoints, uctx->ueus_confd_nr);
	for (i = 0; i < uctx->ueus_confd_nr; ++i) {
		uctx->ueus_confd_fids[i] = M0_FID_INIT(m0_rnd64(&seed),
						       m0_rnd64(&seed));
		rc = snprintf(buf, ARRAY_SIZE(buf), "confd endpoint %d %"PRIu64,
		              i, m0_rnd64(&seed));
		M0_UT_ASSERT(rc < ARRAY_SIZE(buf));
		uctx->ueus_confd_endpoints[i] = m0_strdup(buf);
	}
	uctx->ueus_rm_ep     = "rm endpoint";
	uctx->ueus_rm_fid    = M0_FID_INIT(m0_rnd64(&seed), m0_rnd64(&seed));
	uctx->ueus_req_count = 0;
}

static void
ha_ut_entrypoint_reply_fini(struct ha_ut_entrypoint_usecase_ctx *uctx)
{
	int i;

	for (i = 0; i < uctx->ueus_confd_nr; ++i) {
		m0_free(uctx->ueus_confd_endpoints[i]);
	}
	m0_free(uctx->ueus_confd_endpoints);
	m0_free(uctx->ueus_confd_fids);
}

static void
ha_ut_entrypoint_reply_check(struct ha_ut_entrypoint_usecase_ctx *uctx,
			     struct m0_ha_entrypoint_rep         *rep)
{
	int i;

	M0_UT_ASSERT(rep->hae_control == uctx->ueus_control);
	M0_UT_ASSERT(rep->hae_quorum == uctx->ueus_quorum);
	M0_UT_ASSERT(rep->hae_confd_fids.af_count == uctx->ueus_confd_nr);
	for (i = 0; i < uctx->ueus_confd_nr; ++i) {
		M0_UT_ASSERT(m0_fid_eq(&rep->hae_confd_fids.af_elems[i],
		                       &uctx->ueus_confd_fids[i]));
		M0_UT_ASSERT(m0_streq(rep->hae_confd_eps[i],
		                      uctx->ueus_confd_endpoints[i]));
	}
	M0_UT_ASSERT(m0_streq(rep->hae_active_rm_ep, uctx->ueus_rm_ep));
	M0_UT_ASSERT(m0_fid_eq(&rep->hae_active_rm_fid, &uctx->ueus_rm_fid));
}

static void
ha_ut_entrypoint_server_start(struct ha_ut_entrypoint_usecase_ctx *uctx,
			      struct m0_ha_ut_rpc_ctx             *ctx,
			      struct m0_ha_entrypoint_server      *esr)
{
	struct m0_ha_entrypoint_server_cfg   esr_cfg;
	int                                  rc;

	esr_cfg = (struct m0_ha_entrypoint_server_cfg){
		.hesc_reqh             = &ctx->hurc_reqh,
		.hesc_rpc_machine      = &ctx->hurc_rpc_machine,
		.hesc_request_received = &ha_ut_entrypoint_request_arrived,
	};
	rc = m0_ha_entrypoint_server_init(esr, &esr_cfg);
	M0_UT_ASSERT(rc == 0);
	m0_ha_entrypoint_server_start(esr);
}

static void
ha_ut_entrypoint_server_stop(struct ha_ut_entrypoint_usecase_ctx *uctx,
			     struct m0_ha_entrypoint_server      *esr)
{
	m0_ha_entrypoint_server_stop(esr);
	m0_ha_entrypoint_server_fini(esr);
}

static void
ha_ut_entrypoint_client_start(struct ha_ut_entrypoint_usecase_ctx *uctx,
			      struct m0_ha_ut_rpc_ctx             *ctx,
			      struct m0_ha_entrypoint_client      *ecl)
{
	struct m0_ha_entrypoint_client_cfg  ecl_cfg;
	int                                 rc;

	ecl_cfg = (struct m0_ha_entrypoint_client_cfg){
		.hecc_reqh        = &ctx->hurc_reqh,
		.hecc_rpc_machine = &ctx->hurc_rpc_machine,
		.hecc_process_fid = M0_FID_INIT(0, 0),
	};
	rc = m0_ha_entrypoint_client_init(ecl,
			ctx->hurc_rpc_machine.rm_tm.ntm_ep->nep_addr, &ecl_cfg);
	M0_UT_ASSERT(rc == 0);
	m0_ha_entrypoint_client_start_sync(ecl);
}

static void
ha_ut_entrypoint_client_stop(struct ha_ut_entrypoint_usecase_ctx *uctx,
			     struct m0_ha_entrypoint_client      *ecl)
{
	m0_ha_entrypoint_client_stop(ecl);
	m0_ha_entrypoint_client_fini(ecl);
}

void m0_ha_ut_entrypoint_usecase(void)
{
	struct ha_ut_entrypoint_usecase_ctx *uctx;
	struct m0_ha_entrypoint_server      *esr;
	struct m0_ha_entrypoint_client      *ecl;
	struct m0_ha_ut_rpc_ctx             *ctx;
	struct m0_ha_entrypoint_rep         *rep;

	M0_ALLOC_PTR(uctx);
	M0_ALLOC_PTR(ctx);
	M0_ALLOC_PTR(ecl);
	m0_ha_ut_rpc_ctx_init(ctx);

	ha_ut_entrypoint_reply_init(uctx);

	esr = &uctx->ueus_server;
	ha_ut_entrypoint_server_start(uctx, ctx, esr);

	ha_ut_entrypoint_client_start(uctx, ctx, ecl);
	rep = &ecl->ecl_rep;

	ha_ut_entrypoint_reply_check(uctx, rep);
	M0_UT_ASSERT(uctx->ueus_req_count == 1);

	ha_ut_entrypoint_client_stop(uctx, ecl);
	ha_ut_entrypoint_server_stop(uctx, esr);
	M0_UT_ASSERT(uctx->ueus_req_count == 1);

	ha_ut_entrypoint_reply_fini(uctx);

	m0_ha_ut_rpc_ctx_fini(ctx);
	m0_free(ecl);
	m0_free(ctx);
	m0_free(uctx);
}

static bool ha_ut_entrypoint_client_cb(struct m0_clink *clink)
{
	struct ha_ut_entrypoint_usecase_ctx *uctx =
		container_of(clink, struct ha_ut_entrypoint_usecase_ctx,
			     ueus_clink);
	struct m0_ha_entrypoint_client      *ecl = &uctx->ueus_client;
	struct m0_ha_entrypoint_rep         *rep = &ecl->ecl_rep;
	enum m0_ha_entrypoint_client_state   state;

	state = m0_ha_entrypoint_client_state_get(ecl);
	if (state == M0_HEC_UNAVAILABLE)
		M0_SET0(rep);
	if (state == M0_HEC_AVAILABLE)
		ha_ut_entrypoint_reply_check(uctx, rep);

	return state != M0_HEC_AVAILABLE;
}

void m0_ha_ut_entrypoint_client(void)
{
	struct ha_ut_entrypoint_usecase_ctx *uctx;
	struct m0_ha_entrypoint_server      *esr;
	struct m0_ha_entrypoint_client      *ecl;
	struct m0_ha_ut_rpc_ctx             *ctx;
	struct m0_ha_entrypoint_rep         *rep;

	M0_ALLOC_PTR(uctx);
	M0_ALLOC_PTR(ctx);
	m0_ha_ut_rpc_ctx_init(ctx);

	ha_ut_entrypoint_reply_init(uctx);

	esr = &uctx->ueus_server;
	ha_ut_entrypoint_server_start(uctx, ctx, esr);

	ecl = &uctx->ueus_client;
	ha_ut_entrypoint_client_start(uctx, ctx, ecl);
	rep = &ecl->ecl_rep;
	ha_ut_entrypoint_reply_check(uctx, rep);
	M0_UT_ASSERT(uctx->ueus_req_count == 1);

	m0_clink_init(&uctx->ueus_clink, ha_ut_entrypoint_client_cb);
	m0_clink_add_lock(m0_ha_entrypoint_client_chan(ecl), &uctx->ueus_clink);
	m0_ha_entrypoint_client_request(ecl);
	m0_chan_wait(&uctx->ueus_clink);
	m0_clink_del_lock(&uctx->ueus_clink);
	m0_clink_fini(&uctx->ueus_clink);
	M0_UT_ASSERT(uctx->ueus_req_count == 2);

	/* starts successfully after stop */
	m0_ha_entrypoint_client_stop(ecl);
	M0_SET0(rep);
	m0_ha_entrypoint_client_start_sync(ecl);
	ha_ut_entrypoint_reply_check(uctx, rep);
	M0_UT_ASSERT(uctx->ueus_req_count == 3);

	ha_ut_entrypoint_client_stop(uctx, ecl);
	ha_ut_entrypoint_server_stop(uctx, esr);
	M0_UT_ASSERT(uctx->ueus_req_count == 3);

	ha_ut_entrypoint_reply_fini(uctx);

	m0_ha_ut_rpc_ctx_fini(ctx);
	m0_free(ctx);
	m0_free(uctx);
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
