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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>,
 *                  Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 04-Mar-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/ub.h"         /* m0_ub_set */
#include "lib/misc.h"       /* M0_IN, M0_BITS */
#include "lib/string.h"     /* strlen, m0_strdup */
#include "lib/memory.h"     /* m0_free */
#include "fop/fop.h"        /* m0_fop_alloc */
#include "net/bulk_mem.h"   /* m0_net_bulk_mem_xprt */
#include "net/lnet/lnet.h"  /* m0_net_lnet_xprt */
#include "ut/cs_service.h"  /* m0_cs_default_stypes */
#include "ut/misc.h"        /* M0_UT_PATH */
#include "rpc/rpclib.h"     /* m0_rpc_server_ctx, m0_rpc_client_ctx */
#include "rpc/session.h"    /* m0_rpc_session_timedwait */
#include "rpc/ub/fops.h"

/* ----------------------------------------------------------------
 * CLI arguments
 * ---------------------------------------------------------------- */

/* X(name, defval, max) */
#define ARGS                     \
	X(nr_conns,    2, 10000) \
	X(nr_msgs,  1000,  5000) \
	X(msg_len,    32,  8192)

struct args {
#define X(name, defval, max)  unsigned int a_ ## name;
	ARGS
#undef X
};

static struct args g_args;

/** Assigns default values to the arguments. */
static void args_init(struct args *args)
{
#define X(name, defval, max)  args->a_ ## name = defval;
	ARGS
#undef X
}

static int args_check_limits(const struct args *args)
{
#define X(name, defval, max) \
	&& 0 < args->a_ ## name && args->a_ ## name <= max

	if (true ARGS)
		return 0;
#undef X

	fprintf(stderr, "Value is out of bounds\n");
	return -EINVAL;
}

struct match {
	const char   *m_pattern;
	unsigned int *m_dest;
};

static bool token_matches(const char *token, const struct match *tbl)
{
	for (; tbl->m_pattern != NULL; ++tbl) {
		int rc = sscanf(token, tbl->m_pattern, tbl->m_dest);
		if (rc == 1)
			return true;
	}
	return false;
}

static void args_help(void)
{
	fprintf(stderr, "Expecting a comma-separated list of parameter"
		" specifications:\n");
#define X(name, defval, max) \
	fprintf(stderr, "  %s=NUM\t(default = %u, ulimit = %u)\n", \
		#name, defval, max);
	ARGS
#undef X
}

static int args_parse(const char *src, struct args *dest)
{
	if (src == NULL || *src == 0)
		return 0;

	char *s;
	char *token;
	const struct match match_tbl[] = {
#define X(name, defval, max) { #name "=%u", &dest->a_ ## name },
		ARGS
#undef X
		{ NULL, NULL }
	};

	s = strdupa(src);
	if (s == NULL)
		return -ENOMEM;

	while ((token = strsep(&s, ",")) != NULL) {
		if (!token_matches(token, match_tbl)) {
			fprintf(stderr, "Unable to parse `%s'\n", token);
			args_help();
			return -EINVAL;
		}
	}
	return args_check_limits(dest);
}

#undef ARGS

/* ----------------------------------------------------------------
 * RPC client and server definitions
 * ---------------------------------------------------------------- */

enum {
	CLIENT_COB_DOM_ID  = 16,
	MAX_RPCS_IN_FLIGHT = 10, /* XXX CONFIGUREME */
	MAX_RETRIES        = 500,
	MIN_RECV_QUEUE_LEN = 200
};

/* #define UB_USE_LNET_XPORT */
#ifdef UB_USE_LNET_XPORT
#  define CLIENT_ENDPOINT_FMT  "0@lo:12345:34:%d"
#  define SERVER_ENDPOINT_ADDR "0@lo:12345:32:1"
#  define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR
static struct m0_net_xprt *g_xprt = &m0_net_lnet_xprt;
#else
#  define CLIENT_ENDPOINT_FMT  "127.0.0.1:%d"
#  define SERVER_ENDPOINT_ADDR "127.0.0.1:1"
#  define SERVER_ENDPOINT      "bulk-mem:" SERVER_ENDPOINT_ADDR
static struct m0_net_xprt *g_xprt = &m0_net_bulk_mem_xprt;
#endif

struct ub_rpc_client {
	struct m0_rpc_client_ctx rc_ctx;
	struct m0_net_domain     rc_net_dom;
	struct m0_cob_domain     rc_cob_dom;
};

static struct ub_rpc_client *g_clients;

M0_BASSERT(MIN_RECV_QUEUE_LEN == 200);

#define NAME(ext) "rpc-ub" ext
static char *g_argv[] = {
	NAME(""), "-Q", "200" /* MIN_RECV_QUEUE_LEN */, "-w", "10",
	"-T", "AD", "-D", NAME(".db"), "-S", NAME(".stob"),
	"-A", "linuxstob:"NAME(".addb-stob"),
	"-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
	"-f", M0_UT_CONF_PROCESS,
	"-c", M0_UT_PATH("conf.xc")
};

static struct m0_rpc_server_ctx g_sctx = {
	.rsx_xprts            = &g_xprt,
	.rsx_xprts_nr         = 1,
	.rsx_argv             = g_argv,
	.rsx_argc             = ARRAY_SIZE(g_argv),
	.rsx_log_file_name    = NAME(".log")
};
#undef NAME

static void ub_item_replied(struct m0_rpc_item *item);

static const struct m0_rpc_item_ops ub_item_ops = {
	.rio_replied = ub_item_replied
};

/* ----------------------------------------------------------------
 * RPC client and server operations
 * ---------------------------------------------------------------- */

static void _client_start(struct ub_rpc_client *client, uint32_t cob_dom_id,
			  const char *ep)
{
	int rc;
	struct m0_fid process_fid = M0_FID_TINIT('r', 2, 1);

	rc = m0_net_domain_init(&client->rc_net_dom, g_xprt);
	M0_ASSERT(rc == 0);

	client->rc_ctx = (struct m0_rpc_client_ctx){
		.rcx_net_dom               = &client->rc_net_dom,
		.rcx_local_addr            = m0_strdup(ep),
		.rcx_remote_addr           = SERVER_ENDPOINT_ADDR,
		.rcx_max_rpcs_in_flight    = MAX_RPCS_IN_FLIGHT,
		.rcx_recv_queue_min_length = MIN_RECV_QUEUE_LEN,
		.rcx_fid                   = &process_fid,
	};
	rc = m0_rpc_client_start(&client->rc_ctx);
	if (rc != 0)
		M0_LOG(M0_FATAL, "rc=%d", rc);
	M0_ASSERT(rc == 0);
}

static void _client_stop(struct ub_rpc_client *client)
{
	int rc;

	rc = m0_rpc_client_stop(&client->rc_ctx);
	M0_ASSERT(rc == 0);
	m0_free((void *)client->rc_ctx.rcx_local_addr);
	m0_net_domain_fini(&client->rc_net_dom);
}

static int _start(const char *opts)
{
	int  i;
	int  rc;
	char ep[40];

	args_init(&g_args);
	rc = args_parse(opts, &g_args) ?: m0_rpc_server_start(&g_sctx);
	if (rc != 0)
		return rc;

	M0_ALLOC_ARR(g_clients, g_args.a_nr_conns);
	if (g_clients == NULL) {
		rc = -ENOMEM;
		goto err;
	}

	for (i = 0; i < g_args.a_nr_conns; ++i) {
		snprintf(ep, sizeof ep, CLIENT_ENDPOINT_FMT,
			 2 + i); /* 1 is server EP, so we start from 2 */
		_client_start(&g_clients[i], CLIENT_COB_DOM_ID + i, ep);
	}

	m0_rpc_ub_fops_init();
	return 0;
err:
	m0_rpc_server_stop(&g_sctx);
	return rc;
}

static void _stop(void)
{
	int i;

	for (i = 0; i < g_args.a_nr_conns; ++i)
		_client_stop(&g_clients[i]);
	m0_free(g_clients);
	m0_rpc_server_stop(&g_sctx);
	m0_rpc_ub_fops_fini();
}

static void ub_item_replied(struct m0_rpc_item *item)
{
	struct ub_resp *resp;
	struct ub_req  *req;
	int32_t         err;

	err = m0_rpc_item_error(item);
	if (err != 0)
		M0_LOG(M0_FATAL, "err=%d", err);
	M0_UB_ASSERT(err == 0);

	req  = m0_fop_data(m0_rpc_item_to_fop(item));
	resp = m0_fop_data(m0_rpc_item_to_fop(item->ri_reply));
	M0_UB_ASSERT(resp->ur_seqn == req->uq_seqn);
	M0_UB_ASSERT(m0_buf_eq(&resp->ur_data, &req->uq_data));
}

static void fop_send(struct m0_rpc_session *session, size_t msg_id)
{
	struct m0_fop      *fop;
	struct ub_req      *req;
	struct m0_rpc_item *item;
	char               *data;
	int                 rc;

	M0_PRE(g_args.a_msg_len > 0);

	fop = m0_fop_alloc_at(session, &m0_rpc_ub_req_fopt);
	M0_UB_ASSERT(fop != NULL);

	req = m0_fop_data(fop);
	req->uq_seqn = msg_id;
	M0_ALLOC_ARR(data, g_args.a_msg_len);
	M0_UB_ASSERT(data != NULL);
	/* `data' will be freed by rpc layer (see m0_fop_fini()) */
	m0_buf_init(&req->uq_data, data, g_args.a_msg_len);

	item = &fop->f_item;
	item->ri_nr_sent_max = MAX_RETRIES;
	item->ri_ops         = &ub_item_ops;
	item->ri_session     = session;
	item->ri_deadline    = m0_time_from_now(1, 0);
	item->ri_prio        = M0_RPC_ITEM_PRIO_MID; /* XXX CONFIGUREME */

	rc = m0_rpc_post(item);
	M0_UB_ASSERT(rc == 0);
	m0_fop_put_lock(fop);
}

/* ----------------------------------------------------------------
 * Benchmark
 * ---------------------------------------------------------------- */

static struct m0_rpc_session *_session(unsigned int i)
{
	M0_PRE(i < g_args.a_nr_conns);
	return &g_clients[i].rc_ctx.rcx_session;
}

static void run(int iter M0_UNUSED)
{
	int n;
	int k;
	int rc;

	M0_PRE(g_args.a_nr_msgs > 0 && g_args.a_nr_conns > 0);

	/* @todo: For some reason the following error may occur here:
	   mero: NOTICE : [rpc/slot.c:584:m0_rpc_slot_reply_received] < rc=-71.
	   Needs investigation!
	 */
	for (n = 0; n < g_args.a_nr_msgs; ++n) {
		for (k = 0; k < g_args.a_nr_conns; ++k)
			fop_send(_session(k), n);
	}

	for (k = 0; k < g_args.a_nr_conns; ++k) {
		rc = m0_rpc_session_timedwait(_session(k),
					      M0_BITS(M0_RPC_SESSION_IDLE,
						      M0_RPC_SESSION_FAILED),
					      M0_TIME_NEVER);
		M0_UB_ASSERT(rc == 0);
	}
}

struct m0_ub_set m0_rpc_ub = {
	.us_name = "rpc-ub",
	.us_init = _start,
	.us_fini = _stop,
	.us_run  = {
		{ .ub_name  = "run",
		  .ub_iter  = 1,
		  .ub_round = run },
		{ .ub_name = NULL }  /* terminator */
	}
};

#undef M0_TRACE_SUBSYSTEM
