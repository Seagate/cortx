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
 * Original author: Anup Barve <Anup_Barve@xyratex.com>
 *		    Madhavrao Vemuri <madhav_vemuri@xyratex.com>
 * Original creation date: 06/27/2011
 */

#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/getopts.h"
#include "lib/memory.h"
#include "lib/misc.h"           /* M0_SET0 */
#include "lib/thread.h"
#include "lib/time.h"
#include "mero/init.h"
#include "reqh/reqh.h"          /* m0_reqh_rpc_mach_tl */
#include "net/net.h"
#include "net/lnet/lnet.h"
#include "fop/fop.h"            /* m0_fop_default_item_ops */
#include "rpc/rpc.h"
#include "rpc/rpclib.h"         /* m0_rpc_server_start, m0_rpc_client_start */
#include "rpc/item.h"           /* m0_rpc_item_error */
#include "rpc/it/ping_fop.h"
#include "rpc/it/ping_fop_xc.h"
#include "rpc/it/ping_fom.h"
#include "ut/cs_service.h"      /* m0_cs_default_stypes */

#ifdef __KERNEL__
#  include <linux/kernel.h>
#  include "rpc/it/linux_kernel/rpc_ping.h"
#  define printf printk
#else
#  include <stdlib.h>
#  include <stdio.h>
#  include <string.h>
#  include <unistd.h>		/* read */
#  ifdef HAVE_NETINET_IN_H
#    include <netinet/in.h>
#  endif
#  include <arpa/inet.h>
#  include <netdb.h>
#  include "module/instance.h"  /* m0 */
#endif

#define TRANSPORT_NAME       "lnet"
#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      TRANSPORT_NAME ":" SERVER_ENDPOINT_ADDR

#define SERVER_DB_FILE_NAME        "m0rpcping_server.db"
#define SERVER_STOB_FILE_NAME      "m0rpcping_server.stob"
#define SERVER_ADDB_STOB_FILE_NAME "linuxstob:m0rpcping_server_addb.stob"
#define SERVER_LOG_FILE_NAME       "m0rpcping_server.log"

enum ep_type { EP_SERVER, EP_CLIENT };

enum {
	BUF_LEN            = 128,
	M0_LNET_PORTAL     = 34,
	MAX_RPCS_IN_FLIGHT = 32,
	MAX_RETRIES        = 10
};

#ifndef __KERNEL__
static struct m0 instance;
static bool      server_mode = false;
#endif

static bool  verbose           = false;
static char *server_nid        = "0@lo";
static char *client_nid        = "0@lo";
static int   server_tmid       = 1;
static int   client_tmid       = 2;
static int   nr_client_threads = 1;
static int   nr_ping_bytes     = 8;
static int   nr_ping_item      = 1;
static int   tm_recv_queue_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
static int   max_rpc_msg_size  = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

static char client_endpoint[M0_NET_LNET_XEP_ADDR_LEN];
static char server_endpoint[M0_NET_LNET_XEP_ADDR_LEN];

static struct m0_net_xprt *xprt = &m0_net_lnet_xprt;

#ifdef __KERNEL__
/* Module parameters */
module_param(verbose, bool, S_IRUGO);
MODULE_PARM_DESC(verbose, "enable verbose output to kernel log");

module_param(client_nid, charp, S_IRUGO);
MODULE_PARM_DESC(client_nid, "client network identifier");

module_param(server_nid, charp, S_IRUGO);
MODULE_PARM_DESC(server_nid, "server network identifier");

module_param(server_tmid, int, S_IRUGO);
MODULE_PARM_DESC(server_tmid, "remote transfer machine identifier");

module_param(client_tmid, int, S_IRUGO);
MODULE_PARM_DESC(client_tmid, "local transfer machine identifier");

module_param(nr_client_threads, int, S_IRUGO);
MODULE_PARM_DESC(nr_client_threads, "number of client threads");

module_param(nr_ping_bytes, int, S_IRUGO);
MODULE_PARM_DESC(nr_ping_bytes, "number of ping fop bytes");

module_param(nr_ping_item, int, S_IRUGO);
MODULE_PARM_DESC(nr_ping_item, "number of ping fop items");

module_param(tm_recv_queue_len, int, S_IRUGO);
MODULE_PARM_DESC(tm_recv_queue_len, "minimum TM receive queue length");

module_param(max_rpc_msg_size, int, S_IRUGO);
MODULE_PARM_DESC(max_rpc_msg_size, "maximum RPC message size");
#endif

static int build_endpoint_addr(enum ep_type type,
			       char *out_buf,
			       size_t buf_size)
{
	char *ep_name;
	char *nid;
	int   tmid;

	M0_PRE(M0_IN(type, (EP_SERVER, EP_CLIENT)));

	if (type == EP_SERVER) {
		nid = server_nid;
		ep_name = "server";
		tmid = server_tmid;
	} else {
		nid = client_nid;
		ep_name = "client";
		tmid = client_tmid;
	}

	if (buf_size > M0_NET_LNET_XEP_ADDR_LEN)
		return -1;

	snprintf(out_buf, buf_size, "%s:%u:%u:%d", nid, M0_NET_LNET_PID,
		 M0_LNET_PORTAL, tmid);
	if (verbose)
		printf("%s endpoint: %s\n", ep_name, out_buf);

	return 0;
}

/* Get stats from rpc_machine and print them */
static void __print_stats(struct m0_rpc_machine *rpc_mach)
{
	struct m0_rpc_stats stats;
	printf("stats:\n");

	m0_rpc_machine_get_stats(rpc_mach, &stats, false);
	printf("\treceived_items: %llu\n",
	       (unsigned long long)stats.rs_nr_rcvd_items);
	printf("\tsent_items: %llu\n",
		(unsigned long long)stats.rs_nr_sent_items);
	printf("\tsent_items_uniq: %llu\n",
		(unsigned long long)stats.rs_nr_sent_items_uniq);
	printf("\tresent_items: %llu\n",
		(unsigned long long)stats.rs_nr_resent_items);
	printf("\tfailed_items: %llu\n",
		(unsigned long long)stats.rs_nr_failed_items);
	printf("\ttimedout_items: %llu\n",
		(unsigned long long)stats.rs_nr_timedout_items);
	printf("\tdropped_items: %llu\n",
		(unsigned long long)stats.rs_nr_dropped_items);
	printf("\tha_timedout_items: %llu\n",
		(unsigned long long)stats.rs_nr_ha_timedout_items);
	printf("\tha_noted_conns: %llu\n",
		(unsigned long long)stats.rs_nr_ha_noted_conns);

	printf("\treceived_packets: %llu\n",
	       (unsigned long long)stats.rs_nr_rcvd_packets);
	printf("\tsent_packets: %llu\n",
	       (unsigned long long)stats.rs_nr_sent_packets);
	printf("\tpackets_failed : %llu\n",
	       (unsigned long long)stats.rs_nr_failed_packets);

	printf("\tTotal_bytes_sent : %llu\n",
	       (unsigned long long)stats.rs_nr_sent_bytes);
	printf("\tTotal_bytes_rcvd : %llu\n",
	       (unsigned long long)stats.rs_nr_rcvd_bytes);
}

#ifndef __KERNEL__
/* Prints stats of all the rpc machines in the given request handler. */
static void print_stats(struct m0_reqh *reqh)
{
	struct m0_rpc_machine *rpcmach;

	M0_PRE(reqh != NULL);

	m0_rwlock_read_lock(&reqh->rh_rwlock);
	m0_tl_for(m0_reqh_rpc_mach, &reqh->rh_rpc_machines, rpcmach) {
		M0_ASSERT(m0_rpc_machine_bob_check(rpcmach));
		__print_stats(rpcmach);
	} m0_tl_endfor;
	m0_rwlock_read_unlock(&reqh->rh_rwlock);
}
#endif

static void ping_reply_received(struct m0_rpc_item *item)
{
	struct m0_fop_ping_rep *reply;
	struct m0_fop          *rfop;
	int                     rc;

	/* typical error checking performed in replied callback */
	rc = m0_rpc_item_error(item);
	if (rc == 0) {
		M0_ASSERT(item->ri_reply != NULL);
		rfop = container_of(item->ri_reply, struct m0_fop, f_item);
		reply = m0_fop_data(rfop);
		M0_ASSERT(reply != NULL);
		rc = reply->fpr_rc;
	}
	if (rc == 0) {
		/* operation is successful. */;
	} else {
		/* operation is failed */;
	}
}

const struct m0_rpc_item_ops ping_item_ops = {
	.rio_replied = ping_reply_received,
};

/* Create a ping fop and post it to rpc layer */
static void send_ping_fop(struct m0_rpc_session *session)
{
	int                 rc;
	struct m0_fop      *fop;
	struct m0_fop_ping *ping_fop;
	uint32_t            nr_arr_member;
	const size_t        sz = sizeof ping_fop->fp_arr.f_data[0];

	nr_arr_member = nr_ping_bytes / sz + !(nr_ping_bytes % sz);

	fop = m0_fop_alloc_at(session, &m0_fop_ping_fopt);
	M0_ASSERT(fop != NULL);

	ping_fop = m0_fop_data(fop);
	ping_fop->fp_arr.f_count = nr_arr_member;

	M0_ALLOC_ARR(ping_fop->fp_arr.f_data, nr_arr_member);
	M0_ASSERT(ping_fop->fp_arr.f_data != NULL);

	fop->f_item.ri_resend_interval = M0_TIME_ONE_MSEC * 50;
	rc = m0_rpc_post_sync(fop, session, &ping_item_ops,
			      m0_time_from_now(1, 0));
	M0_ASSERT(rc == 0);
	M0_ASSERT(fop->f_item.ri_error == 0);
	M0_ASSERT(fop->f_item.ri_reply != 0);
	m0_fop_put_lock(fop);
}

static void rpcping_thread(struct m0_rpc_session *session)
{
	int i;

	for (i = 0; i < nr_ping_item; ++i)
		send_ping_fop(session);
}

static int run_client(void)
{
	int               rc;
	int               i;
	struct m0_thread *client_thread;

	/*
	 * Declare these variables as static, to avoid on-stack allocation
	 * of big structures. This is important for kernel-space, where stack
	 * size is very small.
	 */
	static struct m0_net_domain     client_net_dom;
	static struct m0_rpc_client_ctx cctx;
	static struct m0_fid            process_fid = M0_FID_TINIT('r', 0, 1);

	m0_time_t start;
	m0_time_t delta;

	cctx.rcx_net_dom               = &client_net_dom;
	cctx.rcx_local_addr            = client_endpoint;
	cctx.rcx_remote_addr           = server_endpoint;
	cctx.rcx_max_rpcs_in_flight    = MAX_RPCS_IN_FLIGHT;
	cctx.rcx_recv_queue_min_length = tm_recv_queue_len;
	cctx.rcx_max_rpc_msg_size      = max_rpc_msg_size;
	cctx.rcx_fid                   = &process_fid;

	rc = build_endpoint_addr(EP_SERVER, server_endpoint,
				 sizeof server_endpoint);
	if (rc != 0)
		return rc;

	rc = build_endpoint_addr(EP_CLIENT, client_endpoint,
				 sizeof client_endpoint);
	if (rc != 0)
		return rc;

	m0_ping_fop_init();

	rc = m0_net_domain_init(&client_net_dom, xprt);
	if (rc != 0)
		goto fop_fini;

	rc = m0_rpc_client_start(&cctx);
	if (rc != 0) {
		printf("m0rpcping: client init failed \"%i\"\n", -rc);
		goto net_dom_fini;
	}
	M0_ALLOC_ARR(client_thread, nr_client_threads);

	start = m0_time_now();
	for (i = 0; i < nr_client_threads; i++) {
		M0_SET0(&client_thread[i]);

		rc = M0_THREAD_INIT(&client_thread[i], struct m0_rpc_session *,
				    NULL, &rpcping_thread,
				    &cctx.rcx_session, "client_%d", i);
		M0_ASSERT(rc == 0);
	}

	for (i = 0; i < nr_client_threads; i++) {
		m0_thread_join(&client_thread[i]);
		m0_thread_fini(&client_thread[i]);
	}

	delta = m0_time_sub(m0_time_now(), start);

	rc = m0_rpc_client_stop_stats(&cctx, &__print_stats);
	if (verbose)
		printf("Time: %lu.%2.2lu sec\n",
		       (unsigned long)m0_time_seconds(delta),
		       (unsigned long)m0_time_nanoseconds(delta) *
		       100 / M0_TIME_ONE_SECOND);
net_dom_fini:
	m0_net_domain_fini(&client_net_dom);
fop_fini:
	m0_ping_fop_fini();

	return rc;
}

#ifndef __KERNEL__
static void quit_dialog(void)
{
	int  rc __attribute__((unused));
	char ch;

	printf("\n########################################\n");
	printf("\n\nPress Enter to terminate\n\n");
	printf("\n########################################\n");
	rc = read(0, &ch, sizeof ch);
}

static int int2str(char *dest, size_t size, int src, int defval)
{
	int rc = snprintf(dest, size, "%d", src > 0 ? src : defval);
	return (rc < 0 || rc >= size) ? -EINVAL : 0;
}

static int run_server(void)
{
	enum { STRING_LEN = 16 };
	static char tm_len[STRING_LEN];
	static char rpc_size[STRING_LEN];
	int	    rc;
	char       *argv[] = {
		"rpclib_ut",
		"-e", server_endpoint,
		"-H", SERVER_ENDPOINT_ADDR,
		"-q", tm_len, "-m", rpc_size,
	};
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts            = &xprt,
		.rsx_xprts_nr         = 1,
		.rsx_argv             = argv,
		.rsx_argc             = ARRAY_SIZE(argv),
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
	};

	rc = int2str(tm_len, sizeof tm_len, tm_recv_queue_len,
		     M0_NET_TM_RECV_QUEUE_DEF_LEN) ?:
	     int2str(rpc_size, sizeof rpc_size, max_rpc_msg_size,
		     M0_RPC_DEF_MAX_RPC_MSG_SIZE);
	if (rc != 0)
		return rc;

	rc = m0_cs_default_stypes_init();
	if (rc != 0)
		goto m0_fini;

	m0_ping_fop_init();

	/*
	 * Prepend transport name to the beginning of endpoint,
	 * as required by mero-setup.
	 */
	strcpy(server_endpoint, TRANSPORT_NAME ":");

	rc = build_endpoint_addr(
		EP_SERVER, server_endpoint + strlen(server_endpoint),
		sizeof(server_endpoint) - strlen(server_endpoint));
	if (rc != 0)
		goto fop_fini;

	sctx.rsx_mero_ctx.cc_no_conf = true;
	sctx.rsx_mero_ctx.cc_no_storage = true;
	rc = m0_rpc_server_start(&sctx);
	if (rc != 0)
		goto fop_fini;

	quit_dialog();

	if (verbose) {
		printf("########### Server stats ###########\n");
		print_stats(m0_cs_reqh_get(&sctx.rsx_mero_ctx));
	}

	m0_rpc_server_stop(&sctx);
fop_fini:
	m0_ping_fop_fini();
	m0_cs_default_stypes_fini();
m0_fini:
	return rc;
}
#endif

#ifdef __KERNEL__
int m0_rpc_ping_init()
{
	return run_client();
}
#else
/* Main function for rpc ping */
int main(int argc, char *argv[])
{
	int rc;

	rc = m0_init(&instance);
	if (rc != 0)
		return -rc;
	rc = M0_GETOPTS("m0rpcping", argc, argv,
		M0_FLAGARG('s', "run server", &server_mode),
		M0_STRINGARG('C', "client nid",
			LAMBDA(void, (const char *str) { client_nid =
								(char*)str; })),
		M0_FORMATARG('p', "client tmid", "%i", &client_tmid),
		M0_STRINGARG('S', "server nid",
			LAMBDA(void, (const char *str) { server_nid =
								(char*)str; })),
		M0_FORMATARG('P', "server tmid", "%i", &server_tmid),
		M0_FORMATARG('b', "size in bytes", "%i", &nr_ping_bytes),
		M0_FORMATARG('t', "number of client threads", "%i",
						&nr_client_threads),
		M0_FORMATARG('n', "number of ping items", "%i", &nr_ping_item),
		M0_FORMATARG('q', "minimum TM receive queue length \n"
				   "Note: It's default value is 2. \n"
				   "If a large number of ping items having"
				   " bigger sizes are sent \nthen there may be"
				   " insufficient receive buffers.\nIn such"
				   "cases it should have higher values (~16).",
				   "%i", &tm_recv_queue_len),
		M0_FORMATARG('m', "maximum RPC msg size", "%i",
						&max_rpc_msg_size),
		M0_FLAGARG('v', "verbose", &verbose)
		);
	if (rc != 0)
		return -rc;

	if (server_mode)
		rc = run_server();
	else
		rc = run_client();
	m0_fini();
	return -rc;
}
#endif

M0_INTERNAL void m0_rpc_ping_fini(void)
{
}
