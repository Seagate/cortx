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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/memory.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "rpc/rpc_opcodes.h"
#include "net/net.h"
#include "net/lnet/lnet.h"      /* m0_net_lnet_xprt */
#include "net/buffer_pool.h"
#include "ut/ut.h"
#include "rpc/rpc.h"
#include "rpc/rpclib.h"
#include "rpc/at.h"
#include "rpc/ut/at/at_ut.h"
#include "rpc/ut/at/at_ut_xc.h"

struct atut_reqh {
	struct m0_net_domain      aur_net_dom;
	struct m0_net_buffer_pool aur_buf_pool;
	struct m0_reqh            aur_reqh;
	struct m0_rpc_machine     aur_rmachine;
};

struct atut_clctx {
	struct m0_net_domain     acl_ndom;
	struct m0_rpc_client_ctx acl_rpc_ctx;
};

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:2"

static struct atut_reqh        atreqh;
static struct atut_clctx       at_cctx;
static struct m0_reqh_service *atsvc;
static struct m0_fop_type      atbuf_req_fopt;
static struct m0_fop_type      atbuf_rep_fopt;
static const struct m0_fom_ops atfom_ops;

/*************************************************/
/*                RPC AT UT service              */
/*************************************************/

static int  atsvc_start(struct m0_reqh_service *svc);
static void atsvc_stop(struct m0_reqh_service *svc);
static void atsvc_fini(struct m0_reqh_service *svc);

static const struct m0_reqh_service_ops atsvc_ops = {
	.rso_start_async = &m0_reqh_service_async_start_simple,
	.rso_start       = &atsvc_start,
	.rso_stop        = &atsvc_stop,
	.rso_fini        = &atsvc_fini
};

static int atsvc_start(struct m0_reqh_service *svc)
{
	return 0;
}

static void atsvc_stop(struct m0_reqh_service *svc)
{
}

static void atsvc_fini(struct m0_reqh_service *svc)
{
	m0_free(svc);
}

static int atsvc_type_allocate(struct m0_reqh_service            **svc,
			       const struct m0_reqh_service_type  *stype)
{
	M0_ALLOC_PTR(*svc);
	M0_ASSERT(*svc != NULL);
	(*svc)->rs_type = stype;
	(*svc)->rs_ops = &atsvc_ops;
	return 0;
}

static const struct m0_reqh_service_type_ops atsvc_type_ops = {
	.rsto_service_allocate = &atsvc_type_allocate
};

static struct m0_reqh_service_type atsvc_type = {
	.rst_name     = "at_ut",
	.rst_ops      = &atsvc_type_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_DS1
};

/*************************************************/
/*                     Helpers                   */
/*************************************************/

static struct m0_fop *g_reqfop;

static void req_fini(void)
{
	m0_fop_put_lock(g_reqfop);
	g_reqfop = NULL;
}

static void req_send(struct atut__req  *req,
		     struct atut__rep **rep)
{
	int rc;

	M0_UT_ASSERT(g_reqfop == NULL);
	M0_ALLOC_PTR(g_reqfop);
	M0_UT_ASSERT(g_reqfop != NULL);
	m0_fop_init(g_reqfop, &atbuf_req_fopt, req, m0_fop_release);
	rc = m0_rpc_post_sync(g_reqfop, &at_cctx.acl_rpc_ctx.rcx_session, NULL,
			      M0_TIME_IMMEDIATELY);
	M0_ASSERT(rc == 0);
	*rep = (struct atut__rep *)m0_fop_data(
			m0_rpc_item_to_fop(g_reqfop->f_item.ri_reply));
}

M0_INTERNAL void atut__bufdata_alloc(struct m0_buf *buf, size_t size,
				     struct m0_rpc_machine *rmach)
{
	int rc;

	M0_UT_ASSERT(rmach->rm_bulk_cutoff == INBULK_THRESHOLD);
	if (size < INBULK_THRESHOLD) {
		rc = m0_buf_alloc(buf, size);
		M0_UT_ASSERT(rc == 0);
	} else {
		buf->b_addr = m0_alloc_aligned(size, PAGE_SHIFT);
		M0_UT_ASSERT(buf->b_addr != NULL);
		buf->b_nob = size;
	}
	memset(buf->b_addr, DATA_PATTERN, size);
}

static struct m0_rpc_conn *client_conn(void)
{
	return &at_cctx.acl_rpc_ctx.rcx_connection;
}

/*************************************************/
/*                    FOM/FOP                    */
/*************************************************/

enum atfom_phase {
	AT_LOAD = M0_FOPH_TYPE_SPECIFIC,
	AT_LOAD_DONE,
	AT_REP_PREP,
};

static struct m0_sm_state_descr atfom_phases[] = {
	[AT_LOAD] = {
		.sd_name    = "load",
		.sd_allowed = M0_BITS(AT_LOAD_DONE)
	},
	[AT_LOAD_DONE] = {
		.sd_name    = "load-done",
		.sd_allowed = M0_BITS(AT_REP_PREP, M0_FOPH_SUCCESS)
	},
	[AT_REP_PREP] = {
		.sd_name    = "prepare-reply",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS)
	},
};

static size_t atfom_home_locality(const struct m0_fom *fom)
{
	return 1;
}

static bool atdata_is_correct(const struct m0_buf *buf, uint32_t len)
{
	return buf->b_nob == len &&
	       m0_forall(i, buf->b_nob,
			 ((char *)buf->b_addr)[i] == DATA_PATTERN);
}


static bool atbuf_check(const struct m0_rpc_at_buf *ab,
			uint32_t                    len,
			enum m0_rpc_at_type         type)
{
	bool                 ret = true;
	const struct m0_buf *buf;

	if (ab->ab_type != type)
		return false;

	switch (ab->ab_type) {
	case M0_RPC_AT_EMPTY:
		M0_UT_ASSERT(len == 0);
		break;
	case M0_RPC_AT_INLINE:
		buf = &ab->u.ab_buf;
		ret = atdata_is_correct(buf, len);
		break;
	case M0_RPC_AT_BULK_SEND:
		ret = ab->u.ab_send.bdd_used == len;
		break;
	case M0_RPC_AT_BULK_RECV:
		ret = ab->u.ab_recv.bdd_used == len;
		break;
	case M0_RPC_AT_BULK_REP:
		ret = ab->u.ab_rep.abr_len == len;
		break;
	default:
		M0_IMPOSSIBLE("Invalid AT type");
	}
	return ret;
}

static void reqbuf_check(uint32_t test_id, const struct m0_buf *buf, int rc)
{
	switch (test_id) {
	case AT_TEST_INLINE_SEND:
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(atdata_is_correct(buf, INLINE_LEN));
		break;
	case AT_TEST_INBULK_SEND:
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(atdata_is_correct(buf, INBULK_LEN));
		break;
	case AT_TEST_INLINE_RECV:
	case AT_TEST_INLINE_RECV_UNK:
	case AT_TEST_INBULK_RECV_UNK:
	case AT_TEST_INBULK_RECV:
		M0_UT_ASSERT(rc == -EPROTO);
		break;
	default:
		M0_IMPOSSIBLE("Unknown test id");
	}
}

static void load_check(uint32_t test_id, const struct m0_rpc_at_buf *ab,
		       int result)
{
	switch (test_id) {
	case AT_TEST_INLINE_SEND:
		M0_UT_ASSERT(result == M0_FSO_AGAIN);
		M0_UT_ASSERT(atbuf_check(ab, INLINE_LEN, M0_RPC_AT_INLINE));
		break;
	case AT_TEST_INBULK_SEND:
		M0_UT_ASSERT(result == M0_FSO_WAIT);
		M0_UT_ASSERT(atbuf_check(ab, INBULK_LEN, M0_RPC_AT_BULK_SEND));
		break;
	case AT_TEST_INLINE_RECV:
	case AT_TEST_INLINE_RECV_UNK:
	case AT_TEST_INBULK_RECV_UNK:
		M0_UT_ASSERT(result == M0_FSO_AGAIN);
		M0_UT_ASSERT(atbuf_check(ab, 0, M0_RPC_AT_EMPTY));
		break;
	case AT_TEST_INBULK_RECV:
		M0_UT_ASSERT(result == M0_FSO_AGAIN);
		M0_UT_ASSERT(atbuf_check(ab, INBULK_LEN, M0_RPC_AT_BULK_RECV));
		break;
	default:
		M0_IMPOSSIBLE("unknown test id");
	}
}

static void repbuf_fill(uint32_t test_id, struct m0_buf *buf)
{
	struct m0_rpc_machine *rmach;

	rmach = &at_cctx.acl_rpc_ctx.rcx_rpc_machine;

	switch (test_id) {
	case AT_TEST_INLINE_SEND:
	case AT_TEST_INBULK_SEND:
		*buf = M0_BUF_INIT0;
		break;
	case AT_TEST_INLINE_RECV:
	case AT_TEST_INLINE_RECV_UNK:
		atut__bufdata_alloc(buf, INLINE_LEN, rmach);
		break;
	case AT_TEST_INBULK_RECV_UNK:
	case AT_TEST_INBULK_RECV:
		atut__bufdata_alloc(buf, INBULK_LEN, rmach);
		break;
	default:
		M0_IMPOSSIBLE("Unknown test id");
	}
}

static void reply_check(uint32_t test_id, const struct m0_rpc_at_buf *ab,
			int result)
{
	switch (test_id) {
	case AT_TEST_INLINE_RECV:
	case AT_TEST_INLINE_RECV_UNK:
		M0_UT_ASSERT(result == M0_FSO_AGAIN);
		M0_UT_ASSERT(atbuf_check(ab, INLINE_LEN, M0_RPC_AT_INLINE));
		break;
	case AT_TEST_INBULK_RECV_UNK:
		M0_UT_ASSERT(result == M0_FSO_AGAIN);
		M0_UT_ASSERT(atbuf_check(ab, INBULK_LEN, M0_RPC_AT_BULK_REP));
		break;
	case AT_TEST_INBULK_RECV:
		M0_UT_ASSERT(result == M0_FSO_WAIT);
		M0_UT_ASSERT(atbuf_check(ab, INBULK_LEN, M0_RPC_AT_BULK_REP));
		break;
	default:
		M0_IMPOSSIBLE("Unknown test id");
	}
}

static void reply_rc_check(uint32_t test_id, const struct m0_rpc_at_buf *ab,
			   int rc)
{
	switch (test_id) {
	case AT_TEST_INLINE_RECV:
	case AT_TEST_INLINE_RECV_UNK:
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(atbuf_check(ab, INLINE_LEN, M0_RPC_AT_INLINE));
		break;
	case AT_TEST_INBULK_RECV_UNK:
		M0_UT_ASSERT(rc == -ENOMSG);
		M0_UT_ASSERT(atbuf_check(ab, INBULK_LEN, M0_RPC_AT_BULK_REP));
		break;
	case AT_TEST_INBULK_RECV:
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(atbuf_check(ab, INBULK_LEN, M0_RPC_AT_BULK_REP));
		break;
	default:
		M0_IMPOSSIBLE("Unknown test id");
	}
}

bool reply_is_necessary(uint32_t test_id)
{
	return test_id >= AT_TEST_INLINE_RECV;
}

static int atfom_tick(struct m0_fom *fom0)
{
	struct atut__req *req     = m0_fop_data(fom0->fo_fop);
	struct atut__rep *rep     = m0_fop_data(fom0->fo_rep_fop);
	struct m0_buf     req_buf = M0_BUF_INIT0;
	struct m0_buf     rep_buf = M0_BUF_INIT0;
	int               phase   = m0_fom_phase(fom0);
	int               result  = M0_FSO_AGAIN;
	uint32_t          test_id = req->arq_test_id;
	int               rc;

	switch (phase) {
	case M0_FOPH_INIT...M0_FOPH_NR - 1:
		result = m0_fom_tick_generic(fom0);
		break;
	case AT_LOAD:
		result = m0_rpc_at_load(&req->arq_buf, fom0, AT_LOAD_DONE);
		load_check(test_id, &req->arq_buf, result);
		break;
	case AT_LOAD_DONE:
		rc = m0_rpc_at_get(&req->arq_buf, &req_buf);
		reqbuf_check(test_id, &req_buf, rc);
		m0_rpc_at_init(&rep->arp_buf);
		repbuf_fill(test_id, &rep_buf);
		if (reply_is_necessary(test_id)) {
			result = m0_rpc_at_reply(&req->arq_buf, &rep->arp_buf,
						 &rep_buf, fom0, AT_REP_PREP);
			reply_check(test_id, &rep->arp_buf, result);
		} else {
			rep->arp_rc = 0;
			m0_fom_phase_set(fom0, M0_FOPH_SUCCESS);
		}
		break;
	case AT_REP_PREP:
		rc = m0_rpc_at_reply_rc(&rep->arp_buf);
		reply_rc_check(test_id, &rep->arp_buf, rc);
		rep->arp_rc = rc;
		m0_fom_phase_set(fom0, M0_FOPH_SUCCESS);
		break;
	}
	return result;
}

static void atfom_fini(struct m0_fom *fom0)
{
	struct atut__req *req = m0_fop_data(fom0->fo_fop);

	m0_rpc_at_fini(&req->arq_buf);
	m0_fom_fini(fom0);
	m0_free(fom0);
}

static int atfom_create(struct m0_fop *fop,
			struct m0_fom **out, struct m0_reqh *reqh)
{
	struct m0_fop *repfop;

	M0_ALLOC_PTR(*out);
	M0_ASSERT(*out != NULL);
	repfop = m0_fop_reply_alloc(fop, &atbuf_rep_fopt);
	M0_ASSERT(repfop != NULL);
	m0_fom_init(*out, &fop->f_type->ft_fom_type, &atfom_ops,
		    fop, repfop, reqh);
	return 0;
}

static struct m0_sm_conf at_sm_conf = {
	.scf_name      = "atfom",
	.scf_nr_states = ARRAY_SIZE(atfom_phases),
	.scf_state     = atfom_phases,
};

static const struct m0_fom_ops atfom_ops = {
	.fo_tick          = atfom_tick,
	.fo_home_locality = atfom_home_locality,
	.fo_fini          = atfom_fini
};

static const struct m0_fom_type_ops atfom_type_ops = {
	.fto_create = atfom_create
};

static void at_fops_init(void)
{
	m0_sm_conf_extend(m0_generic_conf.scf_state, atfom_phases,
			  m0_generic_conf.scf_nr_states);
	m0_xc_rpc_ut_at_at_ut_init();

	M0_FOP_TYPE_INIT(&atbuf_req_fopt,
			 .name       = "atbuf-req",
			 .opcode     = M0_UT_RPC_AT_REQ_OPCODE,
			 .rpc_flags  = M0_RPC_ITEM_TYPE_REQUEST,
			 .xt         = atut__req_xc,
			 .fom_ops    = &atfom_type_ops,
			 .sm         = &at_sm_conf,
			 .svc_type   = &atsvc_type);
	M0_FOP_TYPE_INIT(&atbuf_rep_fopt,
			 .name       = "atbuf-rep",
			 .opcode     = M0_UT_RPC_AT_REP_OPCODE,
			 .rpc_flags  = M0_RPC_ITEM_TYPE_REPLY,
			 .xt         = atut__rep_xc,
			 .svc_type   = &atsvc_type);
}

static void at_fops_fini(void)
{
	m0_fop_type_fini(&atbuf_req_fopt);
	m0_fop_type_fini(&atbuf_rep_fopt);
	m0_xc_rpc_ut_at_at_ut_fini();
}

/*************************************************/
/*         Test initialisation/finalisation      */
/*************************************************/

static void client_start(void)
{
	struct m0_rpc_client_ctx *cl_rpc_ctx = &at_cctx.acl_rpc_ctx;
	int                       rc;

	rc = m0_net_domain_init(&at_cctx.acl_ndom, &m0_net_lnet_xprt);
	M0_UT_ASSERT(rc == 0);

	cl_rpc_ctx->rcx_net_dom            = &at_cctx.acl_ndom;
	cl_rpc_ctx->rcx_local_addr         = CLIENT_ENDPOINT_ADDR;
	cl_rpc_ctx->rcx_remote_addr        = SERVER_ENDPOINT_ADDR;
	cl_rpc_ctx->rcx_max_rpcs_in_flight = 10;
	cl_rpc_ctx->rcx_fid                = &g_process_fid;

	rc = m0_rpc_client_start(cl_rpc_ctx);
	M0_UT_ASSERT(rc == 0);
	cl_rpc_ctx->rcx_rpc_machine.rm_bulk_cutoff = INBULK_THRESHOLD;
}

static void client_stop(void)
{
	int rc;

	rc = m0_rpc_client_stop(&at_cctx.acl_rpc_ctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&at_cctx.acl_ndom);
}

static void reqh_start(void)
{
	struct m0_reqh *reqh = &atreqh.aur_reqh;
	int             rc;

	rc = m0_reqh_service_allocate(&atsvc, &atsvc_type, NULL);
	M0_UT_ASSERT(rc == 0);
	at_fops_init();
	m0_reqh_service_init(atsvc, reqh, NULL);
	m0_reqh_service_start(atsvc);
	m0_reqh_start(reqh);
}

static void reqh_stop(void)
{
	struct m0_reqh *reqh = &atreqh.aur_reqh;

	m0_reqh_service_prepare_to_stop(atsvc);
	m0_reqh_idle_wait_for(reqh, atsvc);
	m0_reqh_service_stop(atsvc);
	m0_reqh_service_fini(atsvc);
	at_fops_fini();
}

static void reqh_init(void)
{
	struct m0_net_xprt *xprt = &m0_net_lnet_xprt;
	int                 rc;

	M0_SET0(&atreqh);
	rc = m0_net_domain_init(&atreqh.aur_net_dom, xprt);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_net_buffer_pool_setup(&atreqh.aur_net_dom,
					  &atreqh.aur_buf_pool,
					  m0_rpc_bufs_nr(
					     M0_NET_TM_RECV_QUEUE_DEF_LEN, 1),
					  1);
	M0_UT_ASSERT(rc == 0);
	rc = M0_REQH_INIT(&atreqh.aur_reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_machine_init(&atreqh.aur_rmachine,
				 &atreqh.aur_net_dom, SERVER_ENDPOINT_ADDR,
				 &atreqh.aur_reqh, &atreqh.aur_buf_pool,
				 M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	atreqh.aur_rmachine.rm_bulk_cutoff = INBULK_THRESHOLD;
	M0_UT_ASSERT(rc == 0);
}

static void reqh_fini(void)
{
	m0_reqh_services_terminate(&atreqh.aur_reqh);
	m0_rpc_machine_fini(&atreqh.aur_rmachine);
	m0_reqh_fini(&atreqh.aur_reqh);
	m0_rpc_net_buffer_pool_cleanup(&atreqh.aur_buf_pool);
	m0_net_domain_fini(&atreqh.aur_net_dom);
}

static void init(void)
{
	reqh_init();
	reqh_start();
	client_start();
}

static void fini(void)
{
	client_stop();
	reqh_stop();
	reqh_fini();
}

/*************************************************/
/*                    Test cases                 */
/*************************************************/

static void init_fini(void)
{
	struct m0_rpc_at_buf ab;

	m0_rpc_at_init(&ab);
	m0_rpc_at_fini(&ab);
}

static void inline_send(void)
{
	struct atut__req      *req;
	struct atut__rep      *rep;
	struct m0_rpc_at_buf  *ab;
	struct m0_buf          data  = M0_BUF_INIT0;
	struct m0_rpc_machine *rmach;
	int                    rc;

	init();
	M0_ALLOC_PTR(req);
	M0_UT_ASSERT(req != NULL);
	req->arq_test_id = AT_TEST_INLINE_SEND;
	ab = &req->arq_buf;
	m0_rpc_at_init(ab);
	rmach = &at_cctx.acl_rpc_ctx.rcx_rpc_machine;
	atut__bufdata_alloc(&data, INLINE_LEN, rmach);
	rc = m0_rpc_at_add(ab, &data, client_conn());
	M0_UT_ASSERT(rc == 0);
	req_send(req, &rep);
	M0_UT_ASSERT(rep->arp_rc == 0);
	M0_UT_ASSERT(!m0_rpc_at_is_set(&rep->arp_buf));
	m0_rpc_at_fini(&req->arq_buf);
	m0_rpc_at_fini(&rep->arp_buf);
	req_fini();
	fini();
}

static void inbulk_send(void)
{
	struct atut__req      *req;
	struct atut__rep      *rep;
	struct m0_rpc_at_buf  *ab;
	struct m0_buf          data = M0_BUF_INIT0;
	struct m0_rpc_machine *rmach;
	int                    rc;

	init();
	M0_ALLOC_PTR(req);
	M0_UT_ASSERT(req != NULL);
	req->arq_test_id = AT_TEST_INBULK_SEND;
	ab = &req->arq_buf;
	m0_rpc_at_init(ab);
	rmach = &at_cctx.acl_rpc_ctx.rcx_rpc_machine;
	atut__bufdata_alloc(&data, INBULK_LEN, rmach);
	rc = m0_rpc_at_add(ab, &data, client_conn());
	M0_UT_ASSERT(rc == 0);
	req_send(req, &rep);
	M0_UT_ASSERT(rep->arp_rc == 0);
	M0_UT_ASSERT(!m0_rpc_at_is_set(&rep->arp_buf));
	m0_rpc_at_fini(&req->arq_buf);
	m0_rpc_at_fini(&rep->arp_buf);
	req_fini();
	fini();
}

static void inline__recv(uint32_t test_id, uint32_t len)
{
	struct atut__req     *req;
	struct atut__rep     *rep;
	struct m0_rpc_at_buf *ab;
	struct m0_buf         data;
	int                   rc;

	init();
	M0_ALLOC_PTR(req);
	M0_UT_ASSERT(req != NULL);
	req->arq_test_id = test_id;
	ab = &req->arq_buf;
	m0_rpc_at_init(ab);
	rc = m0_rpc_at_recv(ab, client_conn(), len, false);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ab->ab_type == M0_RPC_AT_EMPTY);
	req_send(req, &rep);
	M0_UT_ASSERT(rep->arp_rc == 0);
	M0_UT_ASSERT(m0_rpc_at_is_set(&rep->arp_buf));
	rc = m0_rpc_at_rep_get(ab, &rep->arp_buf, &data);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_rpc_at_is_set(&rep->arp_buf));
	M0_UT_ASSERT(atbuf_check(&rep->arp_buf, INLINE_LEN, M0_RPC_AT_INLINE));
	m0_rpc_at_fini(&req->arq_buf);
	m0_rpc_at_fini(&rep->arp_buf);
	req_fini();
	fini();
}

static void inline_recv(void)
{
	inline__recv(AT_TEST_INLINE_RECV, INLINE_LEN);
}

static void inline_recv_unk(void)
{
	inline__recv(AT_TEST_INLINE_RECV_UNK, M0_RPC_AT_UNKNOWN_LEN);
}

static void inbulk_recv_unk(void)
{
	struct atut__req     *req;
	struct atut__rep     *rep;
	struct m0_rpc_at_buf *ab;
	struct m0_buf         data;
	int                   rc;

	init();
	M0_ALLOC_PTR(req);
	M0_UT_ASSERT(req != NULL);
	req->arq_test_id = AT_TEST_INBULK_RECV_UNK;
	ab = &req->arq_buf;
	m0_rpc_at_init(ab);
	rc = m0_rpc_at_recv(ab, client_conn(), M0_RPC_AT_UNKNOWN_LEN, false);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ab->ab_type == M0_RPC_AT_EMPTY);
	req_send(req, &rep);
	M0_UT_ASSERT(rep->arp_rc == -ENOMSG);
	M0_UT_ASSERT(m0_rpc_at_is_set(&rep->arp_buf));
	rc = m0_rpc_at_rep_get(ab, &rep->arp_buf, &data);
	M0_UT_ASSERT(rc == -ENOMSG);
	M0_UT_ASSERT(m0_rpc_at_is_set(&rep->arp_buf));
	M0_UT_ASSERT(atbuf_check(&rep->arp_buf, INBULK_LEN,
				 M0_RPC_AT_BULK_REP));
	m0_rpc_at_fini(&req->arq_buf);
	m0_rpc_at_fini(&rep->arp_buf);
	req_fini();
	fini();
}

static void inbulk_recv(void)
{
	struct atut__req     *req;
	struct atut__rep     *rep;
	struct m0_rpc_at_buf *ab;
	struct m0_buf         data;
	int                   rc;

	init();
	M0_ALLOC_PTR(req);
	M0_UT_ASSERT(req != NULL);
	req->arq_test_id = AT_TEST_INBULK_RECV;
	ab = &req->arq_buf;
	m0_rpc_at_init(ab);
	rc = m0_rpc_at_recv(ab, client_conn(), INBULK_LEN, false);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(ab->ab_type == M0_RPC_AT_BULK_RECV);
	req_send(req, &rep);
	M0_UT_ASSERT(rep->arp_rc == 0);
	M0_UT_ASSERT(m0_rpc_at_is_set(&rep->arp_buf));
	rc = m0_rpc_at_rep_get(ab, &rep->arp_buf, &data);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_rpc_at_is_set(&rep->arp_buf));
	M0_UT_ASSERT(atbuf_check(&rep->arp_buf, INBULK_LEN,
				 M0_RPC_AT_BULK_REP));
	M0_UT_ASSERT(atdata_is_correct(&data, INBULK_LEN));
	m0_rpc_at_fini(&req->arq_buf);
	m0_rpc_at_fini(&rep->arp_buf);
	req_fini();
	fini();
}

struct m0_ut_suite rpc_at_ut = {
	.ts_name   = "rpc-at",
	.ts_owners = "Egor",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "init-fini",             init_fini,             "Egor" },
		{ "inline-send",           inline_send,           "Egor" },
		{ "inbulk-send",           inbulk_send,           "Egor" },
		{ "inline-recv",           inline_recv,           "Egor" },
		{ "inline-recv-unk",       inline_recv_unk,       "Egor" },
		{ "inbulk-recv-unk",       inbulk_recv_unk,       "Egor" },
		{ "inbulk-recv",           inbulk_recv,           "Egor" },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

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
