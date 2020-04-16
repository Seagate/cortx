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
 * Original author: Huang Hua <Hua_Huang@xyratex.com>
 * Original creation date: 06/19/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_POOL
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/misc.h"        /* M0_IN */
#include "lib/memory.h"
#include "fop/fop.h"
#include "net/lnet/lnet.h"
#include "rpc/rpc.h"
#include "rpc/rpclib.h"
#include "lib/getopts.h"
#include "mero/init.h"
#include "pool/pool.h"
#include "pool/pool_fops.h"
#include "module/instance.h"  /* m0 */

static struct m0_net_domain     cl_ndom;
static struct m0_rpc_client_ctx cl_ctx;
static struct m0_fid            cl_process_fid = M0_FID_TINIT('r', 0, 1);

enum {
	MAX_RPCS_IN_FLIGHT = 10,
	MAX_DEV_NR         = 100,
	MAX_SERVERS        = 1024
};

static const char *cl_ep_addr;
static const char *srv_ep_addr[MAX_SERVERS];
static struct m0_fid device_fid_arr[MAX_DEV_NR];
static int64_t     device_state_arr[MAX_DEV_NR];
static int         di = 0;
static int         ds = 0;
static uint32_t    dev_nr = 0;

struct rpc_ctx {
	struct m0_rpc_conn    ctx_conn;
	struct m0_rpc_session ctx_session;
	const char           *ctx_sep;
};

static int poolmach_client_init(void)
{
	int rc;

	rc = m0_net_domain_init(&cl_ndom, &m0_net_lnet_xprt);
	if (rc != 0)
		return M0_ERR(rc);

	cl_ctx.rcx_net_dom            = &cl_ndom;
	cl_ctx.rcx_local_addr         = cl_ep_addr;
	cl_ctx.rcx_remote_addr        = srv_ep_addr[0];
	cl_ctx.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT;
	cl_ctx.rcx_fid                = &cl_process_fid;

	rc = m0_rpc_client_start(&cl_ctx);
	if (rc != 0)
		m0_net_domain_fini(&cl_ndom);
	return M0_RC(rc);
}

static void poolmach_client_fini(void)
{
	int rc;

	rc = m0_rpc_client_stop(&cl_ctx);
	if (rc != 0)
		M0_LOG(M0_DEBUG, "Failed to stop client");

	m0_net_domain_fini(&cl_ndom);
}

static int poolmach_rpc_ctx_init(struct rpc_ctx *ctx, const char *sep)
{
	ctx->ctx_sep = sep;
	return m0_rpc_client_find_connect(&ctx->ctx_conn, &ctx->ctx_session,
					  &cl_ctx.rcx_rpc_machine,
					  ctx->ctx_sep, M0_CST_IOS,
					  MAX_RPCS_IN_FLIGHT,
			  m0_time_from_now(M0_RPCLIB_UTIL_CONN_TIMEOUT, 0));
}

static void poolmach_rpc_ctx_fini(struct rpc_ctx *ctx)
{
	int rc;

	rc = m0_rpc_session_destroy(&ctx->ctx_session, M0_TIME_NEVER);
	if (rc != 0)
		M0_LOG(M0_DEBUG, "Failed to destroy session to %s",
		       ctx->ctx_sep);
	rc = m0_rpc_conn_destroy(&ctx->ctx_conn, M0_TIME_NEVER);
	if (rc != 0)
		M0_LOG(M0_DEBUG, "Failed to destroy connection to %s",
		       ctx->ctx_sep);
}

static struct m0_mutex poolmach_wait_mutex;
static struct m0_chan  poolmach_wait;
static int32_t         srv_cnt = 0;

extern struct m0_fop_type trigger_fop_fopt;

static void print_help(void)
{
	fprintf(stdout,
"-O Q(uery) or S(et): Query device state or Set device state\n"
"-I device_fid\n"
"[-s device_state]: if Set, the device state. The states supported are:\n"
"    0:    M0_PNDS_ONLINE\n"
"    1:    M0_PNDS_FAILED\n"
"    2:    M0_PNDS_OFFLINE\n"
"-C Client_end_point\n"
"-S Server_end_point [-S Server_end_point ]: max number is %d\n", MAX_SERVERS);
}

int main(int argc, char *argv[])
{
	static struct m0 instance;

	struct rpc_ctx *ctxs;
	const char     *op = NULL;
	m0_time_t       start;
	m0_time_t       delta;
	int             rc;
	int             i;
	int             j;

	rc = m0_init(&instance);
	if (rc != 0) {
		fprintf(stderr, "Cannot init Mero: %d\n", rc);
		return M0_ERR(rc);
	}
	rc = M0_GETOPTS("poolmach", argc, argv,
			M0_STRINGARG('O',
				     "Q(uery) or S(et)",
				     LAMBDA(void, (const char *str) {
						op = str;
						if (!M0_IN(op[0], ('Q', 'q',
								   'S', 's')))
							rc = -EINVAL;
					})),
			M0_FORMATARG('N', "Number of devices", "%u", &dev_nr),
			M0_STRINGARG('I', "device fid",
				     LAMBDA(void, (const char *str)
					    {
						m0_fid_sscanf(str,
							   &device_fid_arr[di]);
						M0_CNT_INC(di);
						M0_ASSERT(di <= MAX_DEV_NR);
					    })),
			M0_NUMBERARG('s', "device state",
				     LAMBDA(void, (int64_t device_state)
					    {
						device_state_arr[ds] =
								device_state;
						M0_CNT_INC(ds);
						M0_ASSERT(di <= MAX_DEV_NR);
					    })),
			M0_STRINGARG('C', "Client endpoint",
				     LAMBDA(void, (const char *str) {
						cl_ep_addr = str;
					})),
			M0_STRINGARG('S', "Server endpoint",
				     LAMBDA(void, (const char *str) {
					    srv_ep_addr[srv_cnt] = str;
					    ++srv_cnt;
					    M0_ASSERT(srv_cnt < MAX_SERVERS);
					})),
			);
	if (rc != 0) {
		print_help();
		return M0_ERR(rc);
	}

	if (op == NULL          || dev_nr == 0  ||
	    dev_nr > MAX_DEV_NR || cl_ep_addr == NULL  || srv_cnt == 0 ||
	    di != dev_nr        || ((op[0] == 'S'||op[0] == 's') && ds!=dev_nr)
	   ) {
		print_help();
		fprintf(stderr, "Insane arguments: op=%s cl_ep=%s "
				"dev_nr=%d di=%d ds=%d srv_cnt=%d\n",
				op, cl_ep_addr, dev_nr, di, ds, srv_cnt);
		return M0_ERR(-EINVAL);
	}

	for (i = 0; i < dev_nr; ++i) {
		if (device_state_arr[i] < M0_PNDS_UNKNOWN ||
		    device_state_arr[i] > M0_PNDS_NR) {
			fprintf(stderr, "invalid device state: %lld\n",
				(long long)device_state_arr[i]);
			return M0_ERR(-EINVAL);
		}
	}

	rc = poolmach_client_init();
	if (rc != 0) {
		fprintf(stderr, "Cannot init client: %d\n", rc);
		return M0_ERR(rc);
	}

	m0_mutex_init(&poolmach_wait_mutex);
	m0_chan_init(&poolmach_wait, &poolmach_wait_mutex);

	M0_ALLOC_ARR(ctxs, srv_cnt);
	if (ctxs == NULL) {
		fprintf(stderr, "Not enough memory. srv count = %d\n", srv_cnt);
		return M0_ERR(-ENOMEM);
	}

	for (i = 1; i < srv_cnt; ++i) {
		/* connection to srv_ep_addr[0] is establish in
		 * poolmach_client_init() already */
		rc = poolmach_rpc_ctx_init(&ctxs[i], srv_ep_addr[i]);
		if (rc != 0) {
			fprintf(stderr, "Cannot init rpc ctx = %d\n", rc);
			return M0_ERR(rc);
		}
	}

	m0_mutex_lock(&poolmach_wait_mutex);
	m0_mutex_unlock(&poolmach_wait_mutex);
	start = m0_time_now();
	for (i = 0; i < srv_cnt; ++i) {
		struct m0_fop         *req;
		struct m0_rpc_session *session;

		session = i == 0 ? &cl_ctx.rcx_session : &ctxs[i].ctx_session;

		if (op[0] == 'Q' || op[0] == 'q') {
			struct m0_fop_poolmach_query *query_fop;

			req = m0_fop_alloc_at(session,
					      &m0_fop_poolmach_query_fopt);
			if (req == NULL) {
				fprintf(stderr, "Not enough memory for fop\n");
				return M0_ERR(-ENOMEM);
			}

			query_fop = m0_fop_data(req);
			query_fop->fpq_type  = M0_POOL_DEVICE;
			M0_ALLOC_ARR(query_fop->fpq_dev_idx.fpx_fid, dev_nr);
			query_fop->fpq_dev_idx.fpx_nr = dev_nr;
			for (j = 0; j < dev_nr; ++j)
				query_fop->fpq_dev_idx.fpx_fid[j] =
					device_fid_arr[j];
		} else {
			struct m0_fop_poolmach_set   *set_fop;

			req = m0_fop_alloc_at(session,
					      &m0_fop_poolmach_set_fopt);
			if (req == NULL) {
				fprintf(stderr, "Not enough memory for fop\n");
				return M0_ERR(-ENOMEM);
			}

			set_fop = m0_fop_data(req);
			set_fop->fps_type  = M0_POOL_DEVICE;
			M0_ALLOC_ARR(set_fop->fps_dev_info.fpi_dev, dev_nr);
			set_fop->fps_dev_info.fpi_nr = dev_nr;
			for (j = 0; j < dev_nr; ++j) {
				set_fop->fps_dev_info.fpi_dev[j].fpd_fid =
					device_fid_arr[j];
				set_fop->fps_dev_info.fpi_dev[j].fpd_state =
					device_state_arr[j];
			}
		}

		fprintf(stderr, "sending/posting to %s\n", srv_ep_addr[i]);
		rc = m0_rpc_post_sync(req, session, NULL, 0);
		if (rc != 0) {
			m0_fop_put_lock(req);
			return M0_ERR(rc);
		}
		if (op[0] == 'Q' || op[0] == 'q') {
			struct m0_fop_poolmach_query_rep *query_fop_rep;
			struct m0_fop *rep;
			int            i;

			rep = m0_rpc_item_to_fop(req->f_item.ri_reply);
			query_fop_rep = m0_fop_data(rep);
			for (i = 0; i < dev_nr; ++i) {
				fprintf(stderr,
					"Query: fid = "FID_F" state= %d rc = %d\n",
					FID_P(&query_fop_rep->fqr_dev_info.
					      fpi_dev[i].fpd_fid),
					(int)query_fop_rep->fqr_dev_info.
					fpi_dev[i].fpd_state,
					(int)query_fop_rep->fqr_rc);
			}
		} else {
			struct m0_fop_poolmach_set_rep *set_fop_rep;
			struct m0_fop *rep;
			rep = m0_rpc_item_to_fop(req->f_item.ri_reply);
			set_fop_rep = m0_fop_data(rep);
			fprintf(stderr, "Set got reply: rc = %d\n",
					(int)set_fop_rep->fps_rc);
		}
		m0_fop_put_lock(req);
		if (rc != 0)
			return M0_ERR(rc);
	}
	delta = m0_time_sub(m0_time_now(), start);
	printf("Time: %lu.%2.2lu sec\n", (unsigned long)m0_time_seconds(delta),
			(unsigned long)m0_time_nanoseconds(delta) * 100 /
			M0_TIME_ONE_SECOND);
	for (i = 1; i < srv_cnt; ++i)
		poolmach_rpc_ctx_fini(&ctxs[i]);
	poolmach_client_fini();
	m0_fini();

	return M0_RC(rc);
}

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
