#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "lib/misc.h"
#include "ut/misc.h"
#include "ut/ut.h"
#include "iscservice/isc.h"
#include "iscservice/isc_service.h"
#include "iscservice/ut/common.h" /* cc_block_init */
#include "lib/finject.h"
#include "rpc/rpclib.h"           /* m0_rpc_server_start */
#include "rpc/ut/at/at_ut.h"      /* atut__bufdata_alloc */

#include <stdio.h>

#define SERVER_ENDPOINT_ADDR "lnet:0@lo:12345:34:1"
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:2"
#define F_CONT 0x12345
#define BARRIER_CNT 2

static struct m0_reqh_service *iscs;
static struct m0_rpc_server_ctx isc_ut_sctx;
static struct m0_rpc_client_ctx isc_ut_cctx;
static struct m0_net_xprt *xprt = &m0_net_lnet_xprt;
static struct m0_net_domain isc_ut_client_ndom;
static uint32_t cc_type;
static const char *SERVER_LOGFILE = "isc_ut.log";
char  *isc_ut_server_args[] = { "m0d", "-T", "LINUX",
				"-D", "sr_db", "-S", "sr_stob",
				"-A", "linuxstob:sr_addb_stob",
				"-f", M0_UT_CONF_PROCESS,
				"-w", "10",
				"-F",
				"-G", SERVER_ENDPOINT_ADDR,
				"-e", SERVER_ENDPOINT_ADDR,
//				"-P", M0_UT_CONF_PROFILE,
				"-c", M0_UT_PATH("conf.xc")};
struct remote_invoke_var {
	struct m0_fop_isc   riv_req;
	struct m0_buf       riv_data;
	struct m0_buf       riv_recv_buf;
	struct m0_mutex     riv_guard;
	struct m0_chan      riv_chan;
} remote_call_info;

static void isc_item_cb(struct m0_rpc_item *item)
{
	struct m0_fop *req_fop;
	struct m0_fop *repl_fop;

	req_fop = m0_rpc_item_to_fop(item);
	repl_fop = m0_rpc_item_to_fop(item->ri_reply);
	/*
	 * References guarantee that fops are not released
	 * before the reply is processed.
	 */
	m0_fop_get(req_fop);
	m0_fop_get(repl_fop);
	m0_chan_signal_lock(&remote_call_info.riv_chan);
}

static const struct m0_rpc_item_ops isc_item_ops = {
	.rio_replied = isc_item_cb,
};

/*
 * Classification of function types based on signature.
 */
enum funct_type {
	/* Neither receives i/p buffer nor returns anything. */
	FT_NEITHER_IO,
	/* Receives no i/p buffer, returns o/p buffer. */
	FT_NO_INPUT,
	/* Receives i/p buffer, returns nothing. */
	FT_NO_OUTPUT,
	/* Receives i/p buffer and returns o/p buffer. */
	FT_BOTH_IO
};

/*
 * A parameter that decides whether all threads use the same fid for
 * registering a computation or a different fid.
 */
enum concc_type {
	CT_SAME_FID,
	CT_DIFF_FID,
};

/* A parameter for the choice of m0_rpc_at buffer. */
enum buf_type {
	BT_INLINE,
	BT_INBULK,
};

struct visitor_entry {
	struct m0_mutex   ve_mutex;
	struct m0_chan    ve_chan;
	volatile uint32_t ve_count;
	uint32_t          ve_wait_count;
} vis_ent;

static int ret_codes[] =
	{-ENOENT, -EINVAL, -ENOMEM, -EPROTO, -EPERM, -ENOMSG, 0};

struct comp_req_aux {
	int (*cra_comp)(struct m0_buf *arg_in,
		        struct m0_buf *args_out,
		        struct m0_isc_comp_private *comp_data, int *rc);
	char *cra_name;
} cra;

char *fixed_str = "abcdefgh";

static void fid_get(const char *f_name, struct m0_fid *fid)
{
	uint32_t f_key = m0_full_name_hash((const unsigned char*)f_name,
					    strlen(f_name));
	m0_fid_set(fid, F_CONT, f_key);
}

int isc_ut_server_start(void)
{
	int rc = 0;

	M0_SET0(&isc_ut_sctx);
	isc_ut_sctx.rsx_xprts         = &xprt;
	isc_ut_sctx.rsx_xprts_nr      = 1;
	isc_ut_sctx.rsx_argv          = isc_ut_server_args;
	isc_ut_sctx.rsx_argc          = ARRAY_SIZE(isc_ut_server_args);
	isc_ut_sctx.rsx_log_file_name = SERVER_LOGFILE;

	rc = m0_rpc_server_start(&isc_ut_sctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_reqh_service_allocate(&iscs, &m0_iscs_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(iscs, m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx),
						    NULL);
	rc = m0_reqh_service_start(iscs);
	M0_UT_ASSERT(rc == 0);
	return rc;
}

static void isc_ut_server_stop(void)
{
	m0_reqh_service_prepare_to_stop(iscs);
	m0_reqh_service_stop(iscs);
	m0_reqh_service_fini(iscs);
	m0_rpc_server_stop(&isc_ut_sctx);
}

static void isc_ut_client_start(void)
{
	int rc;

	M0_SET0(&isc_ut_cctx);
	rc = m0_net_domain_init(&isc_ut_client_ndom, &m0_net_lnet_xprt);
	M0_UT_ASSERT(rc == 0);
	isc_ut_cctx.rcx_remote_addr = SERVER_ENDPOINT_ADDR;
	isc_ut_cctx.rcx_remote_addr = "0@lo:12345:34:1";
	isc_ut_cctx.rcx_max_rpcs_in_flight = 10;
	isc_ut_cctx.rcx_recv_queue_min_length = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	isc_ut_cctx.rcx_max_rpc_msg_size = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	isc_ut_cctx.rcx_local_addr = CLIENT_ENDPOINT_ADDR;
	isc_ut_cctx.rcx_net_dom = &isc_ut_client_ndom;
	isc_ut_cctx.rcx_fid = &g_process_fid;

	rc = m0_rpc_client_start(&isc_ut_cctx);
	M0_UT_ASSERT(rc == 0);
	isc_ut_cctx.rcx_rpc_machine.rm_bulk_cutoff = INBULK_THRESHOLD;
}

static void isc_ut_client_stop()
{
	int rc;

	rc = m0_rpc_client_stop(&isc_ut_cctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&isc_ut_client_ndom);
}

static int null_computation(struct m0_buf *in, struct m0_buf *out,
			    struct m0_isc_comp_private *comp_data, int *rc)
{
	*rc = 0;
	return M0_FSO_AGAIN;
}

static struct m0_rpc_machine *fom_rmach(const struct m0_fom *fom)
{
	return fom->fo_fop->f_item.ri_session->s_conn->c_rpc_machine;
}

/* Increments ASCII value of each character by one. */
static int string_update(struct m0_buf *in, struct m0_buf *out,
		         struct m0_isc_comp_private *comp_data, int *rc)
{
	struct m0_rpc_machine *rmach;
	uint32_t               i;
	uint64_t               size;
	char                  *instr;
	char                  *outstr;

	if (M0_FI_ENABLED("comp_error")) {
		*rc = -EPERM;
		return M0_FSO_AGAIN;
	}
	if (M0_FI_ENABLED("at_mismatch")) {
		rmach = fom_rmach(comp_data->icp_fom);
		size = rmach->rm_bulk_cutoff * 2;
	} else
		size = in->b_nob;
	*rc = m0_buf_alloc(out, size);
	if (*rc != 0)
		return M0_FSO_AGAIN;
	instr = (char *)in->b_addr;
	outstr = (char *)out->b_addr;
	for (i = 0; i < in->b_nob; ++i) {
		outstr[i] = (instr[i] + 1) % CHAR_MAX;
	}
	return M0_FSO_AGAIN;
}

/* Compares input string with predefined string. */
static int strguess(struct m0_buf *in, struct m0_buf *out,
		    struct m0_isc_comp_private *comp_data, int *rc)
{
	char *out_str;

	if (!m0_buf_streq(in, fixed_str)) {
		out_str = m0_strdup(fixed_str);
		if (out_str != NULL)
			m0_buf_init(out, (void *)out_str, strlen(out_str));
		else
			*rc = -ENOMEM;
		*rc = -EINVAL;
	} else
		*rc = 0;
	return M0_FSO_AGAIN;
}

static void comp_launch(void *args)
{
	struct thr_args         *thr_args = args;
	struct m0_isc_comp_req  *comp_req = thr_args->ta_data;
	int                      rc;

	/* Wait till the last thread reaches here. */
	m0_semaphore_up(thr_args->ta_barrier);
	rc = m0_isc_comp_req_exec_sync(comp_req);
	M0_UT_ASSERT(rc == 0);
	m0_isc_comp_req_fini(comp_req);
}

static void vis_entry_init(uint32_t barrier_cnt)
{
	m0_mutex_init(&vis_ent.ve_mutex);
	m0_chan_init(&vis_ent.ve_chan, &vis_ent.ve_mutex);
	vis_ent.ve_count = 0;
	vis_ent.ve_wait_count = barrier_cnt;
}

static void vis_entry_fini(void)
{
	m0_chan_fini_lock(&vis_ent.ve_chan);
	m0_mutex_fini(&vis_ent.ve_mutex);
	M0_SET0(&vis_ent);
}

/*
 * Puts calling fom on wait till predefined number of threads visit the
 * function.
 */
static int barrier(struct m0_buf *in, struct m0_buf *out,
		   struct m0_isc_comp_private *comp_data, int *rc)
{
	int result;

	m0_mutex_lock(&vis_ent.ve_mutex);
	if (vis_ent.ve_count < vis_ent.ve_wait_count - 1) {
		m0_fom_wait_on(comp_data->icp_fom, &vis_ent.ve_chan,
			       &comp_data->icp_fom->fo_cb);
		result = M0_FSO_WAIT;
		*rc = -EAGAIN;
		++vis_ent.ve_count;
	} else {
		m0_chan_broadcast(&vis_ent.ve_chan);
		result = M0_FSO_AGAIN;
		*rc = 0;
		if (vis_ent.ve_count <= vis_ent.ve_wait_count - 1)
			++vis_ent.ve_count;
	}
	m0_mutex_unlock(&vis_ent.ve_mutex);
	return result;
}

static void local_invocation(struct m0_isc_comp_req *comp_req,
			     struct m0_fid *fid, int exp_rc)
{
	struct m0_buf    comp_arg = M0_BUF_INIT0;
	struct m0_reqh  *reqh;
	struct m0_cookie comp_cookie;
	int              rc;

	M0_SET0(&comp_cookie);
	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	m0_isc_comp_req_init(comp_req, &comp_arg, fid, &comp_cookie,
			     M0_ICRT_LOCAL, reqh);
	rc = m0_isc_comp_req_exec_sync(comp_req);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(comp_req->icr_rc == exp_rc);
	m0_isc_comp_req_fini(comp_req);
}

static void comp_req_init(void *arg, int tid)
{
	struct m0_isc_comp_req  *comp_req = arg;
	struct m0_fid            fid;
	struct m0_reqh          *reqh;
	struct m0_reqh_service  *svc_isc;
	struct m0_buf            comp_arg = M0_BUF_INIT0;
	int                      rc;
	int                      exp_rc;

	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	svc_isc = m0_reqh_service_find(&m0_iscs_type, reqh);
	M0_UT_ASSERT(svc_isc != NULL);
	if (cc_type == CT_DIFF_FID) {
		m0_fid_set(&fid, 0x1234, tid);
		exp_rc = 0;
	} else {
		fid_get(cra.cra_name, &fid);
		exp_rc = -EEXIST;
	}
	rc = m0_isc_comp_register(cra.cra_comp, cra.cra_name, &fid);
	M0_UT_ASSERT(rc == exp_rc);
	m0_isc_comp_req_init(comp_req, &comp_arg, &fid, &comp_req->icr_cookie,
			     M0_ICRT_LOCAL, reqh);
}

/*
 * Tests the local invocation of a computation. Also tests the concurrent
 * invocation.
 */
static void test_comp_launch(void)
{
	struct m0_reqh_service *svc_isc;
	struct m0_reqh         *reqh;
	struct m0_fid           fid;
	struct m0_isc_comp_req  comp_req;
	struct cnc_cntrl_block  cc_block;
	int                     rc;

	M0_SET0(&comp_req);
	M0_SET0(&fid);

	rc = isc_ut_server_start();
	M0_UT_ASSERT(rc == 0);
	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	svc_isc =
	  m0_reqh_service_find(&m0_iscs_type, reqh);
	M0_UT_ASSERT(svc_isc != NULL);
	fid_get("null_computation", &fid);
	/* Test a local invocation of computation. */
	rc = m0_isc_comp_register(null_computation, "null_computation", &fid);
	M0_UT_ASSERT(rc == 0);
	local_invocation(&comp_req, &fid, 0);
	m0_isc_comp_unregister(&fid);

	m0_mutex_init(&vis_ent.ve_mutex);
	m0_chan_init(&vis_ent.ve_chan, &vis_ent.ve_mutex);

	/* Set the global parameters for comp_req_init(). */
	cra.cra_name = m0_strdup("barrier");
	cra.cra_comp = barrier;
	/* Test concurrent invocation of different fids */
	vis_entry_init(THR_NR);
	cc_type = CT_DIFF_FID;
	cc_block_init(&cc_block, sizeof comp_req, comp_req_init);
	cc_block_launch(&cc_block, comp_launch);
	M0_UT_ASSERT(vis_ent.ve_count == THR_NR);
	vis_entry_fini();

	/* Test concurrent invocation of a same fid. */
	vis_entry_init(THR_NR);
	fid_get("barrier", &fid);
	rc = m0_isc_comp_register(barrier, "barrier", &fid);
	M0_UT_ASSERT(rc == 0);
	cc_type = CT_SAME_FID;
	cc_block_init(&cc_block, sizeof comp_req, comp_req_init);
	cc_block_launch(&cc_block, comp_launch);
	M0_UT_ASSERT(vis_ent.ve_count == THR_NR);
	vis_entry_fini();

	/* Test error in computation. */
	fid_get("string_update", &fid);
	rc = m0_isc_comp_register(string_update, "string_update", &fid);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable("string_update", "comp_error");
	local_invocation(&comp_req, &fid, -EPERM);
	m0_fi_disable("string_update", "comp_error");

	isc_ut_server_stop();
}

static void test_local_err_path(void)
{
	struct m0_reqh_service *svc_isc;
	struct m0_reqh         *reqh;
	struct m0_fid           fid;
	struct m0_isc_comp_req  comp_req;
	int                     rc;

	M0_SET0(&comp_req);
	M0_SET0(&fid);

	rc = isc_ut_server_start();
	M0_UT_ASSERT(rc == 0);
	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	svc_isc =
	  m0_reqh_service_find(&m0_iscs_type, reqh);
	M0_UT_ASSERT(svc_isc != NULL);
	fid_get("null_computation", &fid);

	/* Test a local invocation of computation. */
	rc = m0_isc_comp_register(null_computation, "null_computation", &fid);
	M0_UT_ASSERT(rc == 0);
	local_invocation(&comp_req, &fid, 0);
	m0_isc_comp_unregister(&fid);

	/* Test a computation that's absent. */
	local_invocation(&comp_req, &fid, -ENOENT);

	/* Test error in computation. */
	fid_get("string_update", &fid);
	rc = m0_isc_comp_register(string_update, "string_update", &fid);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable("string_update", "comp_error");
	local_invocation(&comp_req, &fid, -EPERM);
	m0_fi_disable("string_update", "comp_error");

	/* Test error in buffer allocation. */
	m0_fi_enable("isc_comp_launch", "oom");
	local_invocation(&comp_req, &fid, -ENOMEM);
	m0_fi_disable("isc_comp_launch", "oom");


	isc_ut_server_stop();
}

/*
 * Tests probing of registration state of a computation when:
 *  -computation is registered.
 *  -computation is unregistered while its instance is running.
 *  -computation is not present with the isc service.
 */
static void test_comp_state(void)
{
	struct m0_reqh_service *svc_isc;
	struct m0_reqh         *reqh;
	struct m0_fid           fid;
	struct m0_clink         waiter;
	struct m0_isc_comp_req  comp_req;
	int                     state;
	int                     rc;

	M0_SET0(&comp_req);
	rc = isc_ut_server_start();
	M0_UT_ASSERT(rc == 0);
	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	svc_isc =
	  m0_reqh_service_find(&m0_iscs_type, reqh);
	M0_UT_ASSERT(svc_isc != NULL);

	vis_entry_init(BARRIER_CNT);
	fid_get("barrier", &fid);
	/* Check state of a computation that's absent. */
	state = m0_isc_comp_state_probe(&fid);
	M0_UT_ASSERT(state == -ENOENT);
	rc = m0_isc_comp_register(barrier, "barrier", &fid);
	M0_UT_ASSERT(rc == 0);
	cc_type = CT_SAME_FID;
	/* Set the global parameters for comp_req_init(). */
	cra.cra_name = m0_strdup("barrier");
	cra.cra_comp = barrier;
	comp_req_init(&comp_req, 0);
	m0_clink_init(&waiter, NULL);
	m0_clink_add_lock(&comp_req.icr_chan, &waiter);
	waiter.cl_is_oneshot = true;

	rc = m0_isc_comp_req_exec(&comp_req);
	M0_UT_ASSERT(rc == 0);
	/* Spin till the computation is launched. */
	while (vis_ent.ve_count == 0);
	/* Check the state of a registered computation. */
	state = m0_isc_comp_state_probe(&fid);
	M0_UT_ASSERT(state == M0_ICS_REGISTERED);

	/* Unregister while a computation is running. */
	m0_isc_comp_unregister(&fid);
	state = m0_isc_comp_state_probe(&fid);
	M0_UT_ASSERT(state == M0_ICS_UNREGISTERED);
	/*
	 * Try registering a computation that's scheduled for
	 * unregister.
	 */
	rc = m0_isc_comp_register(barrier, "barrier", &fid);
	M0_UT_ASSERT(rc == -EEXIST);
	/* Invoke the fom pending on channel. */
	m0_mutex_lock(&vis_ent.ve_mutex);
	vis_ent.ve_count++;
	m0_chan_broadcast(&vis_ent.ve_chan);
	m0_mutex_unlock(&vis_ent.ve_mutex);

	/* Wait for the computation to finish. */
	m0_chan_wait(&waiter);
	/* Computation shall now be unavailable. */
	state = m0_isc_comp_state_probe(&fid);
	M0_UT_ASSERT(state == -ENOENT);

	M0_UT_ASSERT(vis_ent.ve_count == BARRIER_CNT);
	vis_entry_fini();
	isc_ut_server_stop();
}

enum fop_processing_phase {
	FPP_SEND,
	FPP_REPLY_PROCESS,
	FPP_COMPLETE,
	FPP_INVALID,
};

static uint32_t remote_invocation_async(int exp_rc, uint32_t buf_type,
					uint32_t phase, struct m0_fop **arg_fop)
{
	struct m0_fop        *reply_fop;
	struct m0_fop_isc_rep repl;
	struct m0_fop_isc    *req;
	struct m0_buf        *recv_buf;
	struct m0_isc_comp   *comp;
	int                   rc;

	switch (phase) {
	case FPP_SEND:
		arg_fop[0] = m0_alloc(sizeof arg_fop[0][0]);
		M0_UT_ASSERT(arg_fop[0] != NULL);
		m0_fop_init(arg_fop[0], &m0_fop_isc_fopt,
			    &remote_call_info.riv_req, m0_fop_release);
		arg_fop[0]->f_item.ri_ops = &isc_item_ops;
		arg_fop[0]->f_item.ri_session = &isc_ut_cctx.rcx_session;
		rc = m0_rpc_post(&arg_fop[0]->f_item);
		M0_UT_ASSERT(rc == 0);
		return FPP_REPLY_PROCESS;
	case FPP_REPLY_PROCESS:
		reply_fop = m0_rpc_item_to_fop(arg_fop[0]->f_item.ri_reply);
		repl = *(struct m0_fop_isc_rep *)m0_fop_data(reply_fop);
		M0_UT_ASSERT(repl.fir_rc == exp_rc);
		req = m0_fop_data(arg_fop[0]);
		recv_buf = &remote_call_info.riv_recv_buf;
		if (repl.fir_rc == 0) {
			rc = m0_rpc_at_rep_get(&req->fi_ret,
					       &repl.fir_ret, recv_buf);
			M0_UT_ASSERT(rc == 0);
			/* Ensure that a valid cookie is returned. */
			comp = m0_cookie_of(&repl.fir_comp_cookie,
					    struct m0_isc_comp, ic_gen);
			M0_UT_ASSERT(comp != NULL);
			M0_UT_ASSERT(m0_fid_eq(&comp->ic_fid,
					       &req->fi_comp_id));
		}
		m0_fop_put0_lock(arg_fop[0]);
		m0_fop_put0_lock(reply_fop);
		return FPP_COMPLETE;
	}
	return FPP_INVALID;
}

static void req_fop_prepare(uint32_t buf_type, struct m0_fid *fid,
			    uint32_t f_type)
{
	struct m0_fop_isc     *fop_isc;
	struct m0_buf         *data;
	struct m0_rpc_machine *rmach;
	char                  *in_buf;
	uint32_t               size;
	int                    rc;

	M0_SET0(&remote_call_info);
	fop_isc = &remote_call_info.riv_req;
	data = &remote_call_info.riv_data;
	*data = M0_BUF_INIT0;
	size = 0;
	rmach = &isc_ut_cctx.rcx_rpc_machine;
	fop_isc->fi_comp_id = *fid;
	m0_rpc_at_init(&fop_isc->fi_args);
	switch (f_type) {
	case FT_NEITHER_IO:
	case FT_NO_INPUT:
		break;
	case FT_NO_OUTPUT:
		in_buf    = m0_strdup(fixed_str);
		M0_UT_ASSERT(in_buf != NULL);
		m0_buf_init(data, (void *)in_buf, strlen(fixed_str));
		break;
	case FT_BOTH_IO:
		size = buf_type == BT_INLINE ? INLINE_LEN : INBULK_LEN;
		atut__bufdata_alloc(data, size, rmach);
		memset(data->b_addr, 'a', data->b_nob);
		break;
	default:
		M0_UT_ASSERT(false);
	}
	rc = m0_rpc_at_add(&fop_isc->fi_args, data,
			   &isc_ut_cctx.rcx_connection);
	M0_UT_ASSERT(rc == 0);
	m0_rpc_at_init(&fop_isc->fi_ret);
	if (M0_FI_ENABLED("at_mismatch")) {
		rc = m0_rpc_at_recv(&fop_isc->fi_ret,
				    &isc_ut_cctx.rcx_connection,
				    M0_RPC_AT_UNKNOWN_LEN, false);
	} else {
		rc = m0_rpc_at_recv(&fop_isc->fi_ret,
				    &isc_ut_cctx.rcx_connection,
				    size, false);
		M0_UT_ASSERT(ergo(size == INBULK_LEN,
			     (fop_isc->fi_ret.ab_type == M0_RPC_AT_BULK_RECV)));
	}
	M0_UT_ASSERT(rc == 0);
}

/* Sets the conditions required to invoke the particular error path. */
static void ret_codes_precond(int exp_rc, uint32_t buf_type, struct m0_fid *fid)
{
	int rc;

	switch (exp_rc) {
	case -EINVAL:
		m0_fi_enable("comp_ref_get", "unregister");
		remote_call_info.riv_req.fi_comp_id = *fid;
		break;
	case -ENOENT:
		remote_call_info.riv_req.fi_comp_id = *fid;
		m0_isc_comp_unregister(fid);
		break;
	case -ENOMEM:
		req_fop_prepare(buf_type, fid, FT_BOTH_IO);
		m0_fi_enable("isc_comp_launch", "oom");
		break;
	case -EPROTO:
		M0_SET0(&remote_call_info);
		remote_call_info.riv_req.fi_comp_id = *fid;
		break;
	case -EPERM:
		req_fop_prepare(buf_type, fid, FT_BOTH_IO);
		m0_fi_enable("string_update", "comp_error");
		break;
	case -ENOMSG:
		m0_fi_enable("req_fop_prepare", "at_mismatch");
		req_fop_prepare(buf_type, fid, FT_BOTH_IO);
		m0_fi_enable("string_update", "at_mismatch");
		break;
	case 0:
		req_fop_prepare(buf_type, fid, FT_BOTH_IO);
		rc = m0_isc_comp_register(string_update, "string_update", fid);
		M0_UT_ASSERT(M0_IN(rc, (0, -EEXIST)));
		break;
	default:
		break;
	}
}

/* Undoes the conditions set during ret_codes_precond(). */
static void ret_codes_postcond(int exp_rc, void *arg)
{
	struct m0_fid *fid;
	int            rc;

	switch (exp_rc) {
	case -EINVAL:
		m0_fi_disable("comp_ref_get", "unregister");
		break;
	case -ENOENT:
		fid = (struct m0_fid *)arg;
		rc = m0_isc_comp_register(string_update, "string_update", fid);
		M0_UT_ASSERT(rc == 0);
		break;
	case -ENOMEM:
		fid = (struct m0_fid *)arg;
		m0_fi_disable("isc_comp_launch", "oom");
		break;
	case -EPERM:
		m0_fi_disable("string_update", "comp_error");
		break;
	case -ENOMSG:
		m0_fi_disable("req_fop_prepare", "at_mismatch");
		m0_fi_disable("string_update", "at_mismatch");
		break;
	default:
		break;
	}
	m0_rpc_at_fini(&remote_call_info.riv_req.fi_args);
	m0_rpc_at_fini(&remote_call_info.riv_req.fi_ret);
}

static void remote_invocation(int exp_rc, uint32_t f_type)
{
	struct m0_fop         arg_fop;
	struct m0_fop        *reply_fop;
	struct m0_fop_isc_rep repl;
	struct m0_fop_isc    *req;
	struct m0_buf        *recv_buf;
	struct m0_isc_comp   *comp;
	int                   ret_rc;
	int                   rc;
	int                   i;

	M0_SET0(&arg_fop);

	m0_fop_init(&arg_fop, &m0_fop_isc_fopt, &remote_call_info.riv_req,
		    m0_fop_release);
	rc = m0_rpc_post_sync(&arg_fop, &isc_ut_cctx.rcx_session, NULL,
			      M0_TIME_IMMEDIATELY);
	M0_UT_ASSERT(rc == 0);
	reply_fop = m0_rpc_item_to_fop(arg_fop.f_item.ri_reply);
	repl = *(struct m0_fop_isc_rep *)m0_fop_data(reply_fop);
	M0_UT_ASSERT(repl.fir_rc == exp_rc);
	req = m0_fop_data(&arg_fop);
	recv_buf = &remote_call_info.riv_recv_buf;
	ret_rc = m0_rpc_at_rep_get(&req->fi_ret, &repl.fir_ret, recv_buf);
	/* Ensure that a valid cookie is returned. */
	comp = m0_cookie_of(&repl.fir_comp_cookie, struct m0_isc_comp, ic_gen);
	if (!M0_IN(repl.fir_rc, (-ENOENT, -EINVAL))) {
		M0_UT_ASSERT(comp != NULL);
		M0_UT_ASSERT(m0_fid_eq(&comp->ic_fid, &req->fi_comp_id));
	} else if (f_type != FT_NO_INPUT)
		M0_UT_ASSERT(comp == NULL);
	switch (f_type) {
	case FT_NEITHER_IO:
	case FT_NO_OUTPUT:
		M0_UT_ASSERT(ret_rc == 0);
		break;
	case FT_NO_INPUT:
		M0_UT_ASSERT(ret_rc == 0);
		M0_UT_ASSERT(m0_buf_streq(recv_buf, fixed_str));
		break;
	case FT_BOTH_IO:
		for (i = 0; i < recv_buf->b_nob; ++i) {
			M0_UT_ASSERT(((uint8_t *)recv_buf->b_addr)[i] ==
				       'b');
		}
		break;
	}
	m0_rpc_at_fini(&repl.fir_ret);
}

static uint32_t expected_output(uint32_t f_type)
{
	switch (f_type) {
	case FT_NEITHER_IO:
	case FT_NO_OUTPUT:
	case FT_BOTH_IO:
		return 0;
	case FT_NO_INPUT:
		return -EINVAL;
	default:
		M0_UT_ASSERT(false);
	}
	return 0;
}

static void comp_remote_invoke(struct comp_req_aux *cra, uint32_t f_type)
{
	struct m0_fid fid;
	int           rc;
	int           exp_rc;

	exp_rc = expected_output(f_type);
	fid_get(cra->cra_name, &fid);
	rc = m0_isc_comp_register(cra->cra_comp, cra->cra_name, &fid);
	M0_UT_ASSERT(rc == 0);
	req_fop_prepare(BT_INBULK, &fid, f_type);
	remote_invocation(exp_rc, f_type);
	m0_isc_comp_unregister(&fid);
}

/*
 * Tests four possible signatures of a computation. See @funct_type.
 */
static void test_comp_signature(void)
{
	struct m0_reqh         *reqh;
	struct m0_reqh_service *svc_isc;
	int                     rc;

	rc = isc_ut_server_start();
	M0_UT_ASSERT(rc == 0);
	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	svc_isc = m0_reqh_service_find(&m0_iscs_type, reqh);
	M0_UT_ASSERT(svc_isc != NULL);
	isc_ut_client_start();
	/*  no-i/p and no-o/p. */
	cra.cra_name = m0_strdup("null_computation");
	cra.cra_comp = null_computation;
	comp_remote_invoke(&cra, FT_NEITHER_IO);

	/* i/p and no-o/p */
	cra.cra_name = m0_strdup("strguess");
	cra.cra_comp = strguess;
	comp_remote_invoke(&cra, FT_NO_OUTPUT);

	/* no-i/p and o/p */
	cra.cra_name = m0_strdup("strguess");
	cra.cra_comp = strguess;
	comp_remote_invoke(&cra, FT_NO_INPUT);

	/* Both i/p and o/p. */
	cra.cra_name = m0_strdup("string_update");
	cra.cra_comp = string_update;
	comp_remote_invoke(&cra, FT_BOTH_IO);

	isc_ut_client_stop();
	isc_ut_server_stop();
}

/* Tests the case when isc-fom is made to wait by computation. */
static void test_remote_waiting(void)
{
	struct m0_reqh         *reqh;
	struct m0_reqh_service *svc_isc;
	struct m0_isc_comp_req  comp_req;
	struct m0_fid           fid;
	struct m0_fop          *arg_fop;
	struct m0_clink         waiter;
	uint32_t                phase;
	int                     rc;

	rc = isc_ut_server_start();
	M0_UT_ASSERT(rc == 0);
	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	svc_isc = m0_reqh_service_find(&m0_iscs_type, reqh);
	M0_UT_ASSERT(svc_isc != NULL);
	fid_get("barrier", &fid);
	cra.cra_name = m0_strdup("barrier");
	cra.cra_comp = barrier;
	rc = m0_isc_comp_register(barrier, "barrier", &fid);
	M0_UT_ASSERT(rc == 0);
	isc_ut_client_start();
	vis_entry_init(BARRIER_CNT);
	req_fop_prepare(BT_INLINE, &fid, FT_NEITHER_IO);
	/* Prepare to wait for reply fop. */
	m0_mutex_init(&remote_call_info.riv_guard);
	m0_chan_init(&remote_call_info.riv_chan, &remote_call_info.riv_guard);
	m0_clink_init(&waiter, NULL);
	m0_clink_add_lock(&remote_call_info.riv_chan, &waiter);
	waiter.cl_is_oneshot = true;
	phase = remote_invocation_async(0, BT_INLINE, FPP_SEND,
					&arg_fop);
	/* Spin till the fom is executed. */
	while (vis_ent.ve_count == 0);
	M0_UT_ASSERT(vis_ent.ve_count == 1);

	/* Launch a local computation to release the waiting fom. */
	comp_req_init(&comp_req, 0);
	rc = m0_isc_comp_req_exec_sync(&comp_req);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(vis_ent.ve_count == BARRIER_CNT);

	/* Wait for reply fop. */
	m0_chan_wait(&waiter);
	phase = remote_invocation_async(0, BT_INLINE, phase, &arg_fop);
	isc_ut_client_stop();
	isc_ut_server_stop();
}

/*
 * Tests error path associated with the remote invocation of a
 * computation. Following error-paths are exercised during the test:
 *  -when computation is not registered (-ENOENT)
 *  -when computation is scheduled to unregister (-EINVAL)
 *  -insufficient memory (-ENOMEM)
 *  -when m0_rpc_at buffer is not initialised properly (-EPROTO)
 *  -when a computation returns an error (-EPERM)
 *  -when caller does not anticipate response via bulk-io.(-ENOMSG)
 *  -when all parameters are sane (0)
 */
static void test_remote_err_path(void)
{
	struct m0_reqh         *reqh;
	struct m0_reqh_service *svc_isc;
	struct m0_fid           fid;
	int                     rc;
	int                     i;
	int                     j;

	rc = isc_ut_server_start();
	M0_UT_ASSERT(rc == 0);
	reqh = m0_cs_reqh_get(&isc_ut_sctx.rsx_mero_ctx);
	svc_isc = m0_reqh_service_find(&m0_iscs_type, reqh);
	M0_UT_ASSERT(svc_isc != NULL);
	fid_get("string_update", &fid);
	rc = m0_isc_comp_register(string_update, "string_update", &fid);
	M0_UT_ASSERT(rc == 0);
	isc_ut_client_start();
	for (j = BT_INLINE; j < BT_INBULK + 1; ++j) {
		for (i = 0; i < ARRAY_SIZE(ret_codes); ++i) {
			ret_codes_precond(ret_codes[i], j, &fid);
			remote_invocation(ret_codes[i], FT_BOTH_IO);
			ret_codes_postcond(ret_codes[i], &fid);
		}
	}
	isc_ut_client_stop();
	isc_ut_server_stop();
}

struct m0_ut_suite isc_service_ut = {
	.ts_name  = "isc-service-ut",
	.ts_init  = NULL,
	.ts_fini  = NULL,
	.ts_tests = {
		{"comp-launch", test_comp_launch, "Nachiket"},
		{"local-error-path", test_local_err_path, "Nachiket"},
		{"comp-state", test_comp_state, "Nachiket"},
		{"remote-comp-signature", test_comp_signature, "Nachiket"},
		{"remote-waiting", test_remote_waiting, "Nachiket"},
		{"remote-error-path", test_remote_err_path, "Nachiket"},
		{NULL, NULL}
	}
};
