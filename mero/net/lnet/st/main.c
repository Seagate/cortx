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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mero/init.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/getopts.h"
#include "lib/memory.h"
#include "lib/thread.h"
#include "net/net.h"
#include "net/lnet/lnet.h"
#include "net/lnet/st/ping.h"

static struct nlx_ping_ctx sctx = {
	.pc_tm = {
		.ntm_state     = M0_NET_TM_UNDEFINED
	}
};

static int quiet_printf(const char *fmt, ...)
{
	return 0;
}

static struct nlx_ping_ops verbose_ops = {
	.pf  = printf,
	.pqs = nlx_ping_print_qstats_tm,
};

static struct nlx_ping_ops quiet_ops = {
	.pf  = quiet_printf,
	.pqs = nlx_ping_print_qstats_tm,
};

int main(int argc, char *argv[])
{
	int			 rc;
	bool			 client_only = false;
	bool			 server_only = false;
	bool			 quiet = false;
	int			 verbose = 0;
	bool                     async_events = false;
	int			 loops = PING_DEF_LOOPS;
	int			 nr_clients = PING_DEF_CLIENT_THREADS;
	int			 nr_bufs = PING_DEF_BUFS;
	unsigned		 nr_recv_bufs = 0;
	uint64_t                 bulk_size = 0;
	int                      bulk_timeout = PING_DEF_BULK_TIMEOUT;
	int                      msg_timeout = PING_DEF_MSG_TIMEOUT;
	int                      active_bulk_delay = 0;
	const char              *client_network = NULL;
	int32_t                  client_portal = -1;
	int32_t                  client_tmid = PING_CLIENT_DYNAMIC_TMID;
	const char              *server_network = NULL;
	int32_t                  server_portal = -1;
	int32_t                  server_tmid = -1;
	int                      client_debug = 0;
	int                      server_debug = 0;
	int                      server_min_recv_size = -1;
	int                      server_max_recv_msgs = -1;
	int                      send_msg_size = -1;
	struct m0_thread	 server_thread;

	rc = m0_init(NULL);
	M0_ASSERT(rc == 0);

	rc = M0_GETOPTS("m0lnetping", argc, argv,
			M0_FLAGARG('s', "run server only", &server_only),
			M0_FLAGARG('c', "run client only", &client_only),
			M0_FORMATARG('b', "number of buffers", "%i", &nr_bufs),
			M0_FORMATARG('B', "number of receive buffers "
				     "(server only)",
				     "%u", &nr_recv_bufs),
			M0_FORMATARG('l', "loops to run", "%i", &loops),
			M0_STRINGARG('d', "bulk data size",
				     LAMBDA(void, (const char *str) {
					     bulk_size =
						     nlx_ping_parse_uint64(str);
					     })),
			M0_FORMATARG('n', "number of client threads", "%i",
				     &nr_clients),
			M0_FORMATARG('D', "server active bulk delay",
				     "%i", &active_bulk_delay),
			M0_STRINGARG('i', "client network interface (ip@intf)",
				     LAMBDA(void, (const char *str) {
						     client_network = str; })),
			M0_FORMATARG('p', "client portal (optional)",
				     "%i", &client_portal),
			M0_FORMATARG('t', "client base TMID (optional)",
				     "%i", &client_tmid),
			M0_STRINGARG('I', "server network interface (ip@intf)",
				     LAMBDA(void, (const char *str) {
						     server_network = str; })),
			M0_FORMATARG('P', "server portal (optional)",
				     "%i", &server_portal),
			M0_FORMATARG('T', "server TMID (optional)",
				     "%i", &server_tmid),
			M0_FORMATARG('o', "message timeout in seconds",
				     "%i", &msg_timeout),
			M0_FORMATARG('O', "bulk timeout in seconds",
				     "%i", &bulk_timeout),
			M0_FORMATARG('x', "client debug",
				     "%i", &client_debug),
			M0_FORMATARG('X', "server debug",
				     "%i", &server_debug),
			M0_FLAGARG('A', "async event processing (old style)",
				   &async_events),
			M0_FORMATARG('R', "receive message max size "
				     "(server only)",
				     "%i", &server_min_recv_size),
			M0_FORMATARG('M', "max receive messages in a single "
				     "buffer (server only)",
				     "%i", &server_max_recv_msgs),
			M0_FORMATARG('m', "message size (client only)",
				     "%i", &send_msg_size),
			M0_FORMATARG('v', "verbosity level",
				     "%i", &verbose),
			M0_FLAGARG('q', "quiet", &quiet));
	if (rc != 0)
		return rc;

	if (nr_clients > PING_MAX_CLIENT_THREADS) {
		fprintf(stderr, "Max of %d client threads supported\n",
			PING_MAX_CLIENT_THREADS);
		return 1;
	}
	if (nr_bufs < PING_MIN_BUFS) {
		fprintf(stderr, "Minimum of %d buffers required\n",
			PING_MIN_BUFS);
		return 1;
	}
	if (bulk_size > PING_MAX_BUFFER_SIZE) {
		fprintf(stderr, "Max supported bulk data size: %d\n",
			PING_MAX_BUFFER_SIZE);
		return 1;
	}
	if (client_only && server_only)
		client_only = server_only = false;
	if (!server_only && client_only && server_network == NULL) {
		fprintf(stderr, "Server LNet interface address missing ("
			"e.g. 10.1.2.3@tcp0, 1.2.3.4@o2ib1)\n");
		return 1;
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

	nlx_ping_init();

	if (!client_only) {
		/* start server in background thread */
		m0_mutex_init(&sctx.pc_mutex);
		m0_cond_init(&sctx.pc_cond, &sctx.pc_mutex);
		if (!quiet)
			sctx.pc_ops = &verbose_ops;
		else
			sctx.pc_ops = &quiet_ops;
		sctx.pc_nr_bufs = nr_bufs;
		sctx.pc_nr_recv_bufs = nr_recv_bufs;
		sctx.pc_bulk_size = bulk_size;
		sctx.pc_bulk_timeout = bulk_timeout;
		sctx.pc_msg_timeout = msg_timeout;
		sctx.pc_server_bulk_delay = active_bulk_delay;
		sctx.pc_network = server_network;
		sctx.pc_pid = M0_NET_LNET_PID;
		sctx.pc_portal = server_portal;
		sctx.pc_tmid = server_tmid;
		sctx.pc_dom_debug = server_debug;
		sctx.pc_tm_debug = server_debug;
		sctx.pc_sync_events = !async_events;
		sctx.pc_min_recv_size = server_min_recv_size;
		sctx.pc_max_recv_msgs = server_max_recv_msgs;
		sctx.pc_verbose = verbose;
		nlx_ping_server_spawn(&server_thread, &sctx);

		if (!quiet)
			printf("Mero LNet System Test Server Initialized\n");
	}

	if (server_only) {
		char readbuf[BUFSIZ];

		printf("Type \"quit\" or ^D to cause server to terminate\n");
		while (fgets(readbuf, BUFSIZ, stdin)) {
			if (strcmp(readbuf, "quit\n") == 0)
				break;
			if (strcmp(readbuf, "\n") == 0)
				nlx_ping_print_qstats_tm(&sctx, false);
			if (strcmp(readbuf, "reset_stats\n") == 0)
				nlx_ping_print_qstats_tm(&sctx, true);
		}
	} else {
		int		      i;
		struct m0_thread     *client_thread;
		struct nlx_ping_client_params *params;
		int32_t               client_base_tmid = client_tmid;
		M0_ALLOC_ARR(client_thread, nr_clients);
		M0_ALLOC_ARR(params, nr_clients);

		/* start all the client threads */
		for (i = 0; i < nr_clients; ++i) {
			if (client_base_tmid != PING_CLIENT_DYNAMIC_TMID)
				client_tmid = client_base_tmid + i;
#define CPARAM_SET(f) params[i].f = f
			CPARAM_SET(loops);
			CPARAM_SET(nr_bufs);
			CPARAM_SET(bulk_size);
			CPARAM_SET(msg_timeout);
			CPARAM_SET(bulk_timeout);
			CPARAM_SET(client_network);
			CPARAM_SET(client_portal);
			CPARAM_SET(client_tmid);
			CPARAM_SET(server_network);
			CPARAM_SET(server_portal);
			CPARAM_SET(server_tmid);
			CPARAM_SET(send_msg_size);
			CPARAM_SET(verbose);
#undef CPARAM_SET
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
		if (!quiet)
			printf("Mero LNet System Test %d Client(s)"
			       " Initialized\n", nr_clients);

		/* ...and wait for them */
		for (i = 0; i < nr_clients; ++i) {
			m0_thread_join(&client_thread[i]);
			if (!quiet && verbose > 0) {
				printf("Client %d: joined\n",
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
		if (!quiet)
			nlx_ping_print_qstats_tm(&sctx, false);
		nlx_ping_server_should_stop(&sctx);
		m0_thread_join(&server_thread);
		m0_cond_fini(&sctx.pc_cond);
		m0_mutex_fini(&sctx.pc_mutex);
	}

	nlx_ping_fini();
	m0_fini();
	return 0;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
