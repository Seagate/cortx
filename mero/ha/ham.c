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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 15-Sep-2016
 */

/**
 * @addtogroup m0ham
 *
 * m0ham (HA messenger) --- CLI utility for sending/receiving m0_ha_msg.
 *
 * @todo Fix memory leaks:
 * - m0_ha_entrypoint_req_fop::erf_git_rev_id
 * - m0_ha_entrypoint_rep_fop::hbp_active_rm_ep
 * - m0_ha_entrypoint_rep_fop::hbp_confd_fids.af_elems
 * - m0_ha_entrypoint_rep_fop::hbp_confd_eps.ab_elems
 *
 * @todo Implement "Connect/disconnect without m0_ha_msg traffic" scenario.
 * See the comment in ham_link_disconnected().
 *
 * @todo "Server sends, client receives" scenario ==> client crashes.
 *
 * @todo ham_say() entrypoint attributes in ham_entrypoint_replied().
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "module/instance.h"  /* m0 */
#include "mero/init.h"        /* m0_init */
#include "net/net.h"          /* m0_net_domain */
#include "net/lnet/lnet.h"    /* m0_net_lnet_xprt */
#include "net/buffer_pool.h"  /* m0_net_buffer_pool */
#include "reqh/reqh.h"        /* m0_reqh */
#include "rpc/rpc_machine.h"  /* m0_rpc_machine */
#include "rpc/rpc.h"          /* m0_rpc_net_buffer_pool_setup */
#include "ha/ha.h"            /* m0_ha_cfg */
#include "ha/link.h"          /* m0_ha_link_wait_delivery */
#include "lib/memory.h"       /* m0_alloc_aligned */
#include "lib/string.h"       /* m0_streq */
#include <libgen.h>           /* basename */
#include <getopt.h>           /* getopt_long */
#include <unistd.h>           /* isatty */

#define HAM_SERVER_EP_DEFAULT "0@lo:12345:63:100"
#define HAM_CLIENT_EP_DEFAULT "0@lo:12345:63:101"

enum ham_mode { HM_CONNECT, HM_LISTEN, HM_SELF_CHECK };

enum {
	HAM_ID = 72 /* = 'H', the constant used in m0ham fids */
};

static struct ham_params {
	enum ham_mode hp_mode;
	const char   *hp_ep_local;
	const char   *hp_ep_remote; /* connect mode only */
	bool          hp_verbose;
	const char   *hp_progname;
} g_params;

static struct m0_semaphore g_sem;
static struct m0_ha_msg   *g_msg;
static struct m0_uint128   g_self_req_id;

struct ham_rpc_ctx {
	struct m0_net_domain      mrc_net_dom;
	struct m0_net_buffer_pool mrc_buf_pool;
	struct m0_reqh            mrc_reqh;
	struct m0_rpc_machine     mrc_rpc_mach;
};

/**
 * Builds m0_xcode_obj from its string representation.
 *
 * @note If the call succeeds, the user is responsible for freeing the
 *       allocated memory with m0_xcode_free_obj().
 *
 * Example:
 * @code
 * {
 *         struct m0_ha_msg *msg;
 *         int               rc;
 *
 *         msg = xcode_read_as(m0_ha_msg_xc, xcode_str, &rc);
 *         if (rc == 0) {
 *                 // Use `msg'.
 *                 // ...
 *                 m0_xcode_free_obj(&M0_XCODE_OBJ(m0_ha_msg_xc, msg));
 *         }
 * }
 * @endcode
 *
 * @pre src != NULL && *src != '\0'
 * @post rc == NULL || (retval == NULL ? *rc < 0 : *rc == 0)
 *
 * @see m0_xcode_read()
 *
 * @todo XXX Move to xcode/xcode.[hc]. Use this function instead of
 * m0_confstr_parse().
 */
static void *
xcode_read_as(const struct m0_xcode_type *type, const char *str, int *rc)
{
	void *ptr;
	int   _rc;

	M0_ENTRY("type=%s", type->xct_name);
	M0_PRE(str != NULL && *str != '\0');

	if (rc == NULL)
		rc = &_rc;
	ptr = m0_alloc(type->xct_sizeof);
	if (ptr == NULL) {
		*rc = M0_ERR(-ENOMEM);
		return NULL;
	}
	*rc = m0_xcode_read(&M0_XCODE_OBJ(type, ptr), str);
	if (*rc != 0) {
		m0_xcode_free_obj(&M0_XCODE_OBJ(type, ptr)); /* frees `ptr' */
		ptr = NULL;
		M0_LOG(M0_ERROR, "Cannot read %s from string (rc=%d):\n%s\n",
		       type->xct_name, *rc, str);
	}
	M0_POST(ptr == NULL ? *rc < 0 : *rc == 0);
	M0_LEAVE("retval=%p", ptr);
	return ptr;
}

/** @todo Move to ha/msg.[hc]. */
static bool ha_msg_is_one_way(const struct m0_ha_msg *msg)
{
	switch (msg->hm_data.hed_type) {
	case M0_HA_MSG_NVEC:
		return msg->hm_data.u.hed_nvec.hmnv_type == M0_HA_NVEC_SET;
	/* XXX FUTURE: Add support for other types. */
	default:
		return true;
	}
}

static void ham_say(const char *format, ...)
{
	if (g_params.hp_verbose) {
		va_list ap;

		va_start(ap, format);
		fprintf(stderr, "%s: ", g_params.hp_progname);
		vfprintf(stderr, format, ap);
		fputs(".\n", stderr);
		va_end(ap);
	}
}

/*
 * XXX TODO: Define generic m0_xcode_obj_to_str().
 * Use it in ham_xcode_print() and m0_confx_to_string() implementations.
 */
static void ham_xcode_print(const struct m0_xcode_obj *x)
{
	m0_bcount_t size;
	char       *buf;
	int         rc;

	size = m0_xcode_print(x, NULL, 0) + 1;
	M0_ASSERT(size > 0);
	buf = m0_alloc(size);
	if (buf == NULL) {
		M0_LOG(M0_ERROR, "Memory allocation failed");
		return;
	}
	rc = m0_xcode_print(x, buf, size);
	if (rc < 0 || rc > size)
		M0_LOG(M0_ERROR, "m0_xcode_print() failed: rc=%d size=%"PRIu64,
		       rc, size);
	puts(buf);
	m0_free(buf);
}

/**
 * @returns the number of bytes read or negative error code.
 */
static int ham_fread_str(char *dest, size_t size, FILE *src)
{
	size_t n;

	if (isatty(fileno(src)))
		/*
		 * Don't let fread() block.
		 * XXX FUTURE: We may want to use select(), like ncat does.
		 */
		return 0;
	M0_PRE(size > 1);
	n = fread(dest, 1, size-1, src);
	if (!feof(src)) {
		if (ferror(src))
			return M0_ERR_INFO(-EIO, "IO error");
		M0_ASSERT(n == size-1);
		return M0_ERR_INFO(-E2BIG, "Input file is too big"
				   " (> %lu bytes)", (unsigned long)size-1);
	}
	dest[n] = '\0';
	return n;
}

/*
 * XXX Code duplication!
 * This function duplicates the code of m0_ut_rpc_machine_start() and
 * m0_ha_ut_rpc_ctx_init().
 */
static void ham_rpc_ctx_init(struct ham_rpc_ctx *ctx,
			     const char *local_endpoint,
			     const struct m0_fid *local_process)
{
	enum { NR_TMS = 1 };
	int rc;

	M0_PRE(local_endpoint != NULL && *local_endpoint != '\0');
	M0_PRE(m0_conf_fid_type(local_process) == &M0_CONF_PROCESS_TYPE);

	rc = m0_net_domain_init(&ctx->mrc_net_dom, &m0_net_lnet_xprt);
	M0_ASSERT(rc == 0);
	rc = m0_rpc_net_buffer_pool_setup(
		&ctx->mrc_net_dom,
		&ctx->mrc_buf_pool,
		m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, NR_TMS),
		NR_TMS);
	M0_ASSERT(rc == 0);
	rc = M0_REQH_INIT(&ctx->mrc_reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = local_process);
	M0_ASSERT(rc == 0);
	m0_reqh_start(&ctx->mrc_reqh);
	rc = m0_rpc_machine_init(&ctx->mrc_rpc_mach, &ctx->mrc_net_dom,
				 local_endpoint, &ctx->mrc_reqh,
				 &ctx->mrc_buf_pool, M0_BUFFER_ANY_COLOUR,
				 M0_RPC_DEF_MAX_RPC_MSG_SIZE,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_ASSERT(rc == 0);
}

/*
 * XXX Code duplication!
 * This function duplicates the code of m0_ut_rpc_machine_stop() and
 * m0_ha_ut_rpc_ctx_fini().
 */
static void ham_rpc_ctx_fini(struct ham_rpc_ctx *ctx)
{
	m0_reqh_shutdown_wait(&ctx->mrc_reqh);
	m0_rpc_machine_fini(&ctx->mrc_rpc_mach);
	m0_reqh_services_terminate(&ctx->mrc_reqh);
	m0_reqh_fini(&ctx->mrc_reqh);
	m0_rpc_net_buffer_pool_cleanup(&ctx->mrc_buf_pool);
	m0_net_domain_fini(&ctx->mrc_net_dom);
}

static const struct m0_ha_msg ham_self_check_msg_default = {
	.hm_data = { // m0_ha_msg_data
		.hed_type = M0_HA_MSG_NVEC,
		.u.hed_nvec = { // m0_ha_msg_nvec
			.hmnv_type              = M0_HA_NVEC_SET,
			.hmnv_id_of_get         = 0,
			.hmnv_ignore_same_state = false,
			.hmnv_nr                = 1,
			.hmnv_arr               = { // m0_ha_msg_nvec_array
				.hmna_arr = {
					{ // m0_ha_note
						.no_id = M0_FID_TINIT(
							'c', 1, 1),
						.no_state = M0_NC_FAILED
					}
				}
			}
		}
	}
};

static void
ham_send(struct m0_ha *ha, struct m0_ha_link *hl, const struct m0_ha_msg *msg)
{
	uint64_t tag;

	if (g_params.hp_mode == HM_SELF_CHECK && msg == NULL)
		msg = &ham_self_check_msg_default;
	if (msg != NULL) {
		m0_ha_send(ha, hl, msg, &tag);
		ham_say("Sent message #%"PRIu64, tag);
	}
}

/*
 * ---------------------------------------------------------------------
 * m0_ha_ops
 */

/**
 * Handles "entrypoint request received" event.
 * Is executed by entrypoint server.
 */
static void ham_entrypoint_request(struct m0_ha *ha,
				   const struct m0_ha_entrypoint_req *req,
				   const struct m0_uint128 *req_id)
{
	struct m0_ha_link          *hl;
	struct m0_ha_entrypoint_rep rep = {
		.hae_quorum        = 1,
		.hae_confd_fids    = {},
		.hae_confd_eps     = NULL,
		.hae_active_rm_fid = M0_FID_TINIT('s', HAM_ID, 1),
		.hae_active_rm_ep  = NULL,
		.hae_control       = M0_HA_ENTRYPOINT_CONSUME,
	};

	ham_say("Got entrypoint request. Replying");
	if (m0_streq(g_params.hp_ep_local, req->heq_rpc_endpoint)) {
		M0_ASSERT(g_params.hp_mode != HM_CONNECT);
		g_self_req_id = *req_id;
	}
	m0_ha_entrypoint_reply(ha, req_id, &rep, &hl);
}

/**
 * Handles "entrypoint reply received" event.
 * Is executed by entrypoint client.
 */
static void
ham_entrypoint_replied(struct m0_ha *ha, struct m0_ha_entrypoint_rep *rep)
{
	ham_say("Got entrypoint reply");
	/* XXX TODO: Show entrypoint fields: quorum, confds, rm. */
}

static void ham_msg_received(struct m0_ha *ha, struct m0_ha_link *hl,
			     struct m0_ha_msg *msg, uint64_t tag)
{
	ham_say("Got message #%"PRIu64, tag);
	if (g_params.hp_mode != HM_SELF_CHECK)
		ham_xcode_print(&M0_XCODE_OBJ(m0_ha_msg_xc, msg));
	m0_ha_delivered(ha, hl, msg);
	m0_semaphore_up(&g_sem);
}

static void
ham_msg_is_delivered(struct m0_ha *ha, struct m0_ha_link *hl, uint64_t tag)
{
	ham_say("Message #%"PRIu64" is delivered", tag);
}

static void
ham_msg_is_not_delivered(struct m0_ha *ha, struct m0_ha_link *hl, uint64_t tag)
{
	ham_say("Message #%"PRIu64" is NOT delivered", tag);
}

/** Is executed by entrypoint server. */
static void ham_link_connected(struct m0_ha *ha,
			       const struct m0_uint128 *req_id,
			       struct m0_ha_link *hl)
{
	ham_say("Connection established");
	M0_ASSERT(g_params.hp_mode != HM_CONNECT);
	if (!m0_uint128_eq(req_id, &g_self_req_id))
		ham_send(ha, hl, g_msg);
}

static void ham_link_reused(struct m0_ha *ha, const struct m0_uint128 *req_id,
			    struct m0_ha_link *hl)
{
	M0_IMPOSSIBLE("XXX Not implemented");
}

static void ham_link_absent(struct m0_ha *ha, const struct m0_uint128 *req_id)
{
	M0_IMPOSSIBLE("XXX Not implemented");
}

static void ham_link_is_disconnecting(struct m0_ha *ha, struct m0_ha_link *hl)
{
	m0_ha_disconnect_incoming(ha, hl);
}

static void ham_link_disconnected(struct m0_ha *ha, struct m0_ha_link *hl)
{
	ham_say("Disconnected");
#if 1
	/*
	 * .hao_link_disconnected is called from m0_ha_stop() only.
	 * XXX @max plans to fix this; see
	 * https://seagate.slack.com/archives/mero-kiev/p1475484199001743
	 *
	 * For now, if none of the parties sends m0_ha_msg, the only way
	 * to stop a listening process is to kill it with a signal.
	 */
#else /* XXX RESTOREME */
	if (g_params.hp_mode == HM_LISTEN)
		m0_semaphore_up(&g_sem);
#endif
}

static const struct m0_ha_ops ham_ha_ops = {
	.hao_entrypoint_request    = ham_entrypoint_request,
	.hao_entrypoint_replied    = ham_entrypoint_replied,
	.hao_msg_received          = ham_msg_received,
	.hao_msg_is_delivered      = ham_msg_is_delivered,
	.hao_msg_is_not_delivered  = ham_msg_is_not_delivered,
	.hao_link_connected        = ham_link_connected,
	.hao_link_reused           = ham_link_reused,
	.hao_link_absent           = ham_link_absent,
	.hao_link_is_disconnecting = ham_link_is_disconnecting,
	.hao_link_disconnected     = ham_link_disconnected
};

/*
 * ---------------------------------------------------------------------
 * signal handling
 */

static void ham_sighandler(int signum)
{
	ham_say("Interrupted by signal %d", signum);
	m0_semaphore_up(&g_sem);
	/* Restore default handlers. */
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

static int ham_sighandler_init(void)
{
	struct sigaction sa = { .sa_handler = ham_sighandler };
	int              rc;

	sigemptyset(&sa.sa_mask);
	/* Block these signals while the handler runs. */
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);

	rc = sigaction(SIGINT, &sa, NULL) ?: sigaction(SIGTERM, &sa, NULL);
	return rc == 0 ? 0 : M0_ERR(errno);
}

/*
 * ---------------------------------------------------------------------
 * CLI arguments
 */

static int ham_params_check(struct ham_params *params);

static void ham_help(FILE *stream, char *progname)
{
	fprintf(stream,
"Usage: %s [option] [endpoint]\n"
"Send/receive data over HA link.\n"
"\n"
"  -h, --help         Display this help screen\n"
"  -l, --listen       Listen for incoming connections\n"
"  -s, --source addr  Specify source address to use (doesn't affect -l);\n"
"                     defaults to "HAM_CLIENT_EP_DEFAULT"\n"
"  -v, --verbose      Explain what is being done\n",
		/*
		 * `--self-check' is intentionally left undocumented.
		 * XXX Do we need self-check mode at all?
		 */
		basename(progname));
}

/**
 * @retval 0      Success.
 * @retval 1      Help message printed. The program should be terminated.
 * @retval -Exxx  Error.
 */
static int
ham_args_parse(struct ham_params *params, int argc, char *const *argv)
{
	/*
	 * XXX FUTURE: We may want to add
	 *   -k, --keep-open (Accept multiple connections)
	 *   -w time, --wait time (Specify connect timeout)
	 * options in the future; see ncat(1).
	 */
	const struct option opts[] = {
		{ "self-check", no_argument, NULL, 'c' },
		{ "help",       no_argument, NULL, 'h' },
		{ "listen",     no_argument, NULL, 'l' },
		{ "source",     required_argument, NULL, 's' },
		{ "verbose",    no_argument, NULL, 'v' },
		{} /* terminator */
	};
	int c;

	*params = (struct ham_params){
		.hp_mode      = HM_CONNECT,
		.hp_ep_local  = NULL,
		.hp_ep_remote = NULL,
		.hp_verbose   = false,
		.hp_progname  = basename(argv[0])
	};
	while ((c = getopt_long(argc, argv, "hls:v", opts, NULL)) != -1) {
		switch (c) {
		case 'c':
			params->hp_mode = HM_SELF_CHECK;
			break;
		case 'h':
			ham_help(stdout, argv[0]);
			return 1;
		case 'l':
			params->hp_mode = HM_LISTEN;
			break;
		case 's':
			params->hp_ep_local = optarg;
			break;
		case 'v':
			params->hp_verbose = true;
			break;
		default:
			goto err;
		}
	}
	if (params->hp_ep_local != NULL && params->hp_mode != HM_CONNECT) {
		fprintf(stderr, "`-s' can only be used in connect mode\n");
		return -EINVAL; /* cannot use M0_ERR() */
	}
	if (optind == argc) {     /* no arguments */
		if (params->hp_mode == HM_CONNECT) {
			fprintf(stderr, "Remote endpoint is missing\n");
			goto err;
		}
		return ham_params_check(params);
	}
	if (optind + 1 == argc) { /* one argument */
		if (params->hp_mode == HM_LISTEN)
			params->hp_ep_local = argv[optind];
		else
			params->hp_ep_remote = argv[optind];
		return ham_params_check(params);
	}
	fprintf(stderr, "Too many arguments\n");
err:
	fprintf(stderr, "Type `%s --help' for usage\n", basename(argv[0]));
	return -EINVAL; /* cannot use M0_ERR() before m0_init() */
}

static void ham_maybe_set(const char **dest, const char *value)
{
	M0_PRE(value != NULL && *value != '\0');
	if (*dest == NULL)
		*dest = value;
}

static int ham_params_check(struct ham_params *params)
{
	switch (params->hp_mode) {
	case HM_CONNECT:
		ham_maybe_set(&params->hp_ep_local, HAM_CLIENT_EP_DEFAULT);
		M0_ASSERT(params->hp_ep_remote != NULL);
		if (m0_streq(params->hp_ep_local, params->hp_ep_remote)) {
			fprintf(stderr, "Remote and local endpoints must"
				" differ\n");
			return -EINVAL; /* cannot use M0_ERR() */
		}
		return 0;
	case HM_LISTEN:
	case HM_SELF_CHECK:
		ham_maybe_set(&params->hp_ep_local, HAM_SERVER_EP_DEFAULT);
		M0_ASSERT(params->hp_ep_remote == NULL);
		params->hp_ep_remote = params->hp_ep_local;
		return 0;
	default:
		M0_IMPOSSIBLE("");
	}
}

/*
 * ---------------------------------------------------------------------
 * main
 */

int main(int argc, char **argv)
{
	enum { BUFSIZE = 2 << 21 /* 4 MB */ };
	static char buf[BUFSIZE]; /* XXX TODO: expand the buffer dynamically */

	struct m0          inst = {};
	struct ham_rpc_ctx rpc_ctx;
	struct m0_ha       ha = {};
	struct m0_ha_link *hl;
	struct m0_ha_cfg   ha_cfg = {
		.hcf_ops         = ham_ha_ops,
		.hcf_rpc_machine = &rpc_ctx.mrc_rpc_mach,
		.hcf_reqh        = &rpc_ctx.mrc_reqh,
		.hcf_addr        = NULL,
		.hcf_process_fid = M0_FID_TINIT('r', HAM_ID, HAM_ID),
	};
	int rc;

	rc = ham_args_parse(&g_params, argc, argv);
	if (rc < 0)
		return -rc;
	if (rc == 1)
		return 0;

	rc = m0_semaphore_init(&g_sem, 0);
	if (rc != 0)
		return M0_ERR(errno);
	rc = ham_sighandler_init() ?: m0_init(&inst);
	if (rc != 0)
		goto sem_fini;

	rc = ham_fread_str(buf, sizeof buf, stdin);
	if (rc > 0) {
		g_msg = xcode_read_as(m0_ha_msg_xc, buf, &rc);
		if (g_msg == NULL) {
			M0_ASSERT(rc < 0);
			goto m0_fini;
		}
		g_msg->hm_tag = 0; /* Let `ha' layer set this value. */
	}
	if (rc < 0)
		goto m0_fini;

	ha_cfg.hcf_addr = g_params.hp_ep_remote;
	ha_cfg.hcf_process_fid =
		M0_FID_TINIT('r', HAM_ID, !!(g_params.hp_mode == HM_CONNECT));
	ham_rpc_ctx_init(&rpc_ctx, g_params.hp_ep_local,
			 &ha_cfg.hcf_process_fid);
	rc = m0_ha_init(&ha, &ha_cfg);
	if (rc != 0)
		goto rpc_fini;
	rc = m0_ha_start(&ha);
	if (rc != 0)
		goto ha_fini;
	if (g_params.hp_mode == HM_CONNECT)
		ham_say("Connecting to %s", g_params.hp_ep_remote);
	hl = m0_ha_connect(&ha);
	if (hl == NULL) {
		rc = M0_ERR(1);
		goto ha_stop;
	}
	M0_ASSERT(hl == ha.h_link);

	if (g_params.hp_mode == HM_LISTEN) {
		ham_say("Listening at %s", g_params.hp_ep_local);
		m0_semaphore_down(&g_sem);
		M0_ASSERT(m0_semaphore_value(&g_sem) == 0);
	} else {
		ham_send(&ha, hl, g_msg);
	}
	if (g_params.hp_mode == HM_CONNECT && g_msg != NULL &&
	    !ha_msg_is_one_way(g_msg)) {
		ham_say("Awaiting reply");
		m0_semaphore_down(&g_sem);
		M0_ASSERT(m0_semaphore_value(&g_sem) == 0);
	}
	ham_say("Finishing");
	m0_ha_flush(&ha, hl);
	m0_ha_disconnect(&ha);
ha_stop:
	m0_ha_stop(&ha);
ha_fini:
	m0_ha_fini(&ha);
rpc_fini:
	ham_rpc_ctx_fini(&rpc_ctx);
	m0_xcode_free_obj(&M0_XCODE_OBJ(m0_ha_msg_xc, g_msg));
m0_fini:
	m0_fini();
sem_fini:
	m0_semaphore_fini(&g_sem);
	return M0_RC(rc < 0 ? -rc : rc);
}

#undef M0_TRACE_SUBSYSTEM
/** @} m0ham */
