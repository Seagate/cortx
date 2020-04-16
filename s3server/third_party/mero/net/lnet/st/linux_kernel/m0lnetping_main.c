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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>,
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 04/12/2011
 * Adapted for LNet: 04/11/2012
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/inet.h> /* in_aton */

#include "lib/memory.h"
#include "lib/misc.h" /* M0_SET0 */
#include "net/net.h"
#include "net/lnet/lnet.h"
#include "net/lnet/st/ping.h"

/**
   @addtogroup LNetDFS
   @{
 */

/* Module parameters */
static bool quiet = false;
module_param(quiet, bool, S_IRUGO);
MODULE_PARM_DESC(quiet, "quiet mode");

static int verbose = 0;
module_param(verbose, int, S_IRUGO);
MODULE_PARM_DESC(verbose, "verbosity level");

static bool client_only = false;
module_param(client_only, bool, S_IRUGO);
MODULE_PARM_DESC(verbose, "run client only");

static bool server_only = false;
module_param(server_only, bool, S_IRUGO);
MODULE_PARM_DESC(server_only, "run server only");

static bool async_events = false;
module_param(async_events, bool, S_IRUGO);
MODULE_PARM_DESC(async_events, "async event processing (old style)");

static uint nr_bufs = PING_DEF_BUFS;
module_param(nr_bufs, uint, S_IRUGO);
MODULE_PARM_DESC(nr_bufs, "total number of network buffers to allocate");

static uint nr_recv_bufs = 0;
module_param(nr_recv_bufs, uint, S_IRUGO);
MODULE_PARM_DESC(nr_recv_bufs, "number of receive buffers (server only)");

static char *bulk_size = NULL;
module_param(bulk_size, charp, S_IRUGO);
MODULE_PARM_DESC(bulk_size, "bulk data size");

static int active_bulk_delay = 0;
module_param(active_bulk_delay, int, S_IRUGO);
MODULE_PARM_DESC(active_bulk_delay, "Delay before sending active receive");

static int nr_clients = PING_DEF_CLIENT_THREADS;
module_param(nr_clients, int, S_IRUGO);
MODULE_PARM_DESC(nr_clients, "number of client threads");

static int loops = PING_DEF_LOOPS;
module_param(loops, int, S_IRUGO);
MODULE_PARM_DESC(loops, "loops to run");

static int bulk_timeout = PING_DEF_BULK_TIMEOUT;
module_param(bulk_timeout, int, S_IRUGO);
MODULE_PARM_DESC(bulk_timeout, "bulk timeout");

static int msg_timeout = PING_DEF_MSG_TIMEOUT;
module_param(msg_timeout, int, S_IRUGO);
MODULE_PARM_DESC(msg_timeout, "message timeout");

static char *client_network = NULL;
module_param(client_network, charp, S_IRUGO);
MODULE_PARM_DESC(client_network, "client network interface (ip@intf)");

static int client_portal = -1;
module_param(client_portal, int, S_IRUGO);
MODULE_PARM_DESC(client_portal, "client portal (optional)");

static int client_tmid = PING_CLIENT_DYNAMIC_TMID;
module_param(client_tmid, int, S_IRUGO);
MODULE_PARM_DESC(client_tmid, "client base TMID (optional)");

static char *server_network = NULL;
module_param(server_network, charp, S_IRUGO);
MODULE_PARM_DESC(server_network, "server network interface (ip@intf)");

static int server_portal = -1;
module_param(server_portal, int, S_IRUGO);
MODULE_PARM_DESC(server_portal, "server portal (optional)");

static int server_tmid = -1;
module_param(server_tmid, int, S_IRUGO);
MODULE_PARM_DESC(server_tmid, "server TMID (optional)");

static int server_min_recv_size = -1;
module_param(server_min_recv_size, int, S_IRUGO);
MODULE_PARM_DESC(server_min_recv_size, "server min receive size (optional)");

static int server_max_recv_msgs = -1;
module_param(server_max_recv_msgs, int, S_IRUGO);
MODULE_PARM_DESC(server_max_recv_msgs, "server max receive msgs (optional)");

static int send_msg_size = -1;
module_param(send_msg_size, int, S_IRUGO);
MODULE_PARM_DESC(send_msg_size, "client message size (optional)");

static int server_debug = 0;
module_param(server_debug, int, S_IRUGO);
MODULE_PARM_DESC(server_debug, "server debug (optional)");

static int client_debug = 0;
module_param(client_debug, int, S_IRUGO);
MODULE_PARM_DESC(client_debug, "client debug (optional)");

static int quiet_printk(const char *fmt, ...)
{
	return 0;
}

static int verbose_printk(const char *fmt, ...)
{
	va_list varargs;
	char *fmtbuf;
	int rc;

	va_start(varargs, fmt);
	fmtbuf = m0_alloc(strlen(fmt) + sizeof KERN_INFO);
	if (fmtbuf != NULL) {
	    sprintf(fmtbuf, "%s%s", KERN_INFO, fmt);
	    fmt = fmtbuf;
	}
	/* call vprintk(KERN_INFO ...) */
	rc = vprintk(fmt, varargs);
	va_end(varargs);
	m0_free(fmtbuf);
	return rc;
}

static struct nlx_ping_ops verbose_ops = {
	.pf  = verbose_printk,
	.pqs = nlx_ping_print_qstats_tm,
};

static struct nlx_ping_ops quiet_ops = {
	.pf  = quiet_printk,
	.pqs = nlx_ping_print_qstats_tm,
};

static struct nlx_ping_ctx sctx = {
	.pc_tm = {
		.ntm_state     = M0_NET_TM_UNDEFINED
	}
};

static struct m0_thread server_thread;
static struct m0_thread *client_thread;
static struct nlx_ping_client_params *params;

static int __init m0_netst_init_k(void)
{
	int rc;
	uint64_t buffer_size;
	M0_THREAD_ENTER;

	/* parse module options */
	if (nr_bufs < PING_MIN_BUFS) {
		printk(KERN_WARNING "Minimum of %d buffers required\n",
		       PING_MIN_BUFS);
		return -EINVAL;
	}
	buffer_size = nlx_ping_parse_uint64(bulk_size);
	if (buffer_size > PING_MAX_BUFFER_SIZE) {
		printk(KERN_WARNING "Max supported bulk data size: %d\n",
		       PING_MAX_BUFFER_SIZE);
		return -EINVAL;
	}
	if (nr_clients > PING_MAX_CLIENT_THREADS) {
		printk(KERN_WARNING "Max of %d client threads supported\n",
			PING_MAX_CLIENT_THREADS);
		return -EINVAL;
	}
	if (client_only && server_only)
		client_only = server_only = false;
	if (server_network == NULL) {
		printk(KERN_WARNING "Server LNet interface address missing ("
			"e.g. 10.1.2.3@tcp0, 1.2.3.4@o2ib1)\n");
		return -EINVAL;
	}
	if (!server_only && client_network == NULL) {
		printk(KERN_WARNING "Client LNet interface address missing ("
			"e.g. 10.1.2.3@tcp0, 1.2.3.4@o2ib1)\n");
		return -EINVAL;
	}
	if (server_portal < 0)
		server_portal = PING_SERVER_PORTAL;
	if (client_portal < 0)
		client_portal = PING_CLIENT_PORTAL;
	if (server_tmid < 0)
		server_tmid = PING_SERVER_TMID;
	if (client_tmid < 0)
		client_tmid = PING_CLIENT_DYNAMIC_TMID;
	if (verbose < 0)
		verbose = 0;

	/* set up sys fs entries? */

	nlx_ping_init();

	if (!client_only) {
		/* set up server context */
		m0_mutex_init(&sctx.pc_mutex);
		m0_cond_init(&sctx.pc_cond, &sctx.pc_mutex);
		if (!quiet)
			sctx.pc_ops = &verbose_ops;
		else
			sctx.pc_ops = &quiet_ops;

		sctx.pc_nr_bufs = nr_bufs;
		sctx.pc_nr_recv_bufs = nr_recv_bufs;
		sctx.pc_bulk_size = buffer_size;
		sctx.pc_msg_timeout = msg_timeout;
		sctx.pc_bulk_timeout = bulk_timeout;
		sctx.pc_server_bulk_delay = active_bulk_delay;
		sctx.pc_network = server_network;
		sctx.pc_portal = server_portal;
		sctx.pc_tmid = server_tmid;
		sctx.pc_dom_debug = server_debug;
		sctx.pc_tm_debug = server_debug;
		sctx.pc_sync_events = !async_events;
		sctx.pc_min_recv_size = server_min_recv_size;
		sctx.pc_max_recv_msgs = server_max_recv_msgs;
		sctx.pc_verbose = verbose;
		nlx_ping_server_spawn(&server_thread, &sctx);

		printk(KERN_INFO "Mero LNet System Test"
		       " Server Initialized\n");
	}

	if (!server_only) {
		int	i;
		int32_t client_base_tmid = client_tmid;
		M0_ALLOC_ARR(client_thread, nr_clients);
		M0_ALLOC_ARR(params, nr_clients);

		M0_ASSERT(client_thread != NULL);
		M0_ASSERT(params != NULL);

		/* start all the client threads */
		for (i = 0; i < nr_clients; ++i) {
			if (client_base_tmid != PING_CLIENT_DYNAMIC_TMID)
				client_tmid = client_base_tmid + i;
#define CPARAM_SET(f) params[i].f = f
			CPARAM_SET(loops);
			CPARAM_SET(nr_bufs);
			CPARAM_SET(bulk_timeout);
			CPARAM_SET(msg_timeout);
			CPARAM_SET(client_network);
			CPARAM_SET(client_portal);
			CPARAM_SET(client_tmid);
			CPARAM_SET(server_network);
			CPARAM_SET(server_portal);
			CPARAM_SET(server_tmid);
			CPARAM_SET(send_msg_size);
			CPARAM_SET(verbose);
#undef CPARAM_SET
			params[i].bulk_size = buffer_size;
			params[i].client_id = i + 1;
			params[i].client_pid = M0_NET_LNET_PID;
			params[i].server_pid = M0_NET_LNET_PID;
			params[i].debug = client_debug;
			if (!quiet)
				params[i].ops = &verbose_ops;
			else
				params[i].ops = &quiet_ops;

			rc = M0_THREAD_INIT(&client_thread[i],
					    struct nlx_ping_client_params *,
					    NULL, &nlx_ping_client, &params[i],
					    "client_%d", params[i].client_id);
			M0_ASSERT(rc == 0);
		}
		printk(KERN_INFO "Mero LNet System Test"
		       " %d Client(s) Initialized\n", nr_clients);
	}

	return 0;
}

static void __exit m0_netst_fini_k(void)
{
	M0_THREAD_ENTER;
	if (!server_only) {
		int i;
		for (i = 0; i < nr_clients; ++i) {
			m0_thread_join(&client_thread[i]);
			if (!quiet && verbose > 0) {
				printk(KERN_INFO "Client %d: joined\n",
				       params[i].client_id);
			}
		}
		if (!quiet)
			nlx_ping_print_qstats_total("Client total",
						    &verbose_ops);
		m0_free(client_thread);
		m0_free(params);
	}

	if (!client_only) {
		if (sctx.pc_ops->pqs != NULL)
			(*sctx.pc_ops->pqs)(&sctx, false);

		nlx_ping_server_should_stop(&sctx);
		m0_thread_join(&server_thread);
		m0_cond_fini(&sctx.pc_cond);
		m0_mutex_fini(&sctx.pc_mutex);
	}

	nlx_ping_fini();
	printk(KERN_INFO "Mero Kernel Messaging System Test removed\n");
}

module_init(m0_netst_init_k)
module_exit(m0_netst_fini_k)

MODULE_AUTHOR("Xyratex");
MODULE_DESCRIPTION("Mero Kernel Messaging System Test");
MODULE_LICENSE("proprietary");

/** @} */ /* LNetDFS */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
