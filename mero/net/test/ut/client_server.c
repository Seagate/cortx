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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 05/19/2012
 */

#include "ut/ut.h"			/* M0_UT_ASSERT */
#include "lib/memory.h"			/* m0_free */
#include "lib/thread.h"			/* M0_THREAD_INIT */
#include "lib/semaphore.h"		/* m0_semaphore_down */
#include "lib/misc.h"			/* M0_SET0 */
#include "lib/trace.h"			/* M0_LOG */

#include "net/lnet/lnet.h"		/* M0_NET_LNET_PID */

#include "net/test/node.h"		/* m0_net_test_node_ctx */
#include "net/test/console.h"		/* m0_net_test_console_ctx */

#define NET_TEST_MODULE_NAME ut_client_server
#include "net/test/debug.h"

enum {
	NTCS_PID		  = M0_NET_LNET_PID,
	NTCS_PORTAL		  = 42,
	NTCS_NODES_MAX		  = 128,
	NTCS_NODE_ADDR_MAX	  = 0x100,
	NTCS_TIMEOUT		  = 20,
	NTCS_TIMEOUT_GDB	  = 1200, /**< 20min for debugging in gdb */
	NTCS_TMID_CONSOLE4CLIENTS = 298,
	NTCS_TMID_CONSOLE4SERVERS = 299,
	NTCS_TMID_NODES	  = 300,
	NTCS_TMID_CMD_CLIENTS	  = NTCS_TMID_NODES,
	NTCS_TMID_DATA_CLIENTS    = NTCS_TMID_NODES + NTCS_NODES_MAX * 1,
	NTCS_TMID_CMD_SERVERS	  = NTCS_TMID_NODES + NTCS_NODES_MAX * 2,
	NTCS_TMID_DATA_SERVERS    = NTCS_TMID_NODES + NTCS_NODES_MAX * 3,
	NTCS_ACCEPTABLE_MSG_LOSS  = 20,	/**< 20% */
};

static struct m0_net_test_node_cfg node_cfg[NTCS_NODES_MAX * 2];
static struct m0_thread		   node_thread[NTCS_NODES_MAX * 2];
static struct m0_semaphore	   node_init_sem;

static char *addr_console4clients;
static char *addr_console4servers;
static char  clients[NTCS_NODES_MAX * NTCS_NODE_ADDR_MAX];
static char  servers[NTCS_NODES_MAX * NTCS_NODE_ADDR_MAX];
static char  clients_data[NTCS_NODES_MAX * NTCS_NODE_ADDR_MAX];
static char  servers_data[NTCS_NODES_MAX * NTCS_NODE_ADDR_MAX];

/* s/NTCS_TIMEOUT/NTCS_TIMEOUT_GDB/ while using gdb */
static m0_time_t timeout = M0_MKTIME(NTCS_TIMEOUT, 0);

static char *addr_get(const char *nid, int tmid)
{
	char  addr[NTCS_NODE_ADDR_MAX];
	char *result;
	int   rc;

	rc = snprintf(addr, NTCS_NODE_ADDR_MAX,
		     "%s:%d:%d:%d", nid, NTCS_PID, NTCS_PORTAL, tmid);
	M0_UT_ASSERT(rc < NTCS_NODE_ADDR_MAX);

	result = m0_alloc(rc + 1);
	M0_UT_ASSERT(result != NULL);
	return strncpy(result, addr, rc + 1);
}

static void addr_free(char *addr)
{
	m0_free(addr);
}

static void net_test_node(struct m0_net_test_node_cfg *node_cfg)
{
	struct m0_net_test_node_ctx *ctx;
	int			     rc;

	M0_PRE(node_cfg != NULL);

	M0_ALLOC_PTR(ctx);
	M0_ASSERT(ctx != NULL);
	rc = m0_net_test_node_init(ctx, node_cfg);
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_test_node_start(ctx);
	M0_UT_ASSERT(rc == 0);
	m0_semaphore_up(&node_init_sem);
	/* wait for the test node thread */
	m0_semaphore_down(&ctx->ntnc_thread_finished_sem);
	m0_net_test_node_stop(ctx);
	m0_net_test_node_fini(ctx);
	m0_free(ctx);
}

static void node_cfg_fill(struct m0_net_test_node_cfg *ncfg,
			  char *addr_cmd,
			  char *addr_cmd_list,
			  char *addr_data,
			  char *addr_data_list,
			  char *addr_console,
			  bool last_node)
{
	*ncfg = (struct m0_net_test_node_cfg) {
		.ntnc_addr	   = addr_cmd,
		.ntnc_addr_console = addr_console,
		.ntnc_send_timeout = timeout,
	};

	strncat(addr_cmd_list, ncfg->ntnc_addr, NTCS_NODE_ADDR_MAX - 1);
	strncat(addr_cmd_list, last_node ? "" : ",", 1);
	strncat(addr_data_list, addr_data, NTCS_NODE_ADDR_MAX - 1);
	strncat(addr_data_list, last_node ? "" : ",", 1);

	addr_free(addr_data);
}

static void msg_nr_print(const char *prefix,
			 const struct m0_net_test_msg_nr *msg_nr)
{
	LOGD("%-21s total/failed/bad = %lu/%lu/%lu", prefix,
	     msg_nr->ntmn_total, msg_nr->ntmn_failed, msg_nr->ntmn_bad);
}

static bool msg_nr_in_range(size_t nr1, size_t nr2)
{
	size_t nr1_x100 = nr1 * 100;
	return nr1_x100 >= nr2 * (100 - NTCS_ACCEPTABLE_MSG_LOSS) &&
	       nr1_x100 <= nr2 * (100 + NTCS_ACCEPTABLE_MSG_LOSS);
}

#define nrchk(nr1, nr2)                                      \
{                                                            \
	size_t total1 = (nr1)->ntmn_total;                   \
	size_t total2 = (nr2)->ntmn_total;                   \
	M0_UT_ASSERT(msg_nr_in_range(total1, total2));       \
	M0_UT_ASSERT(msg_nr_in_range(total2, total1));       \
}

/*
 * Real situation - no explicit synchronization
 * between test console and test nodes.
 */
static void net_test_client_server(const char *nid,
				   enum m0_net_test_type type,
				   size_t clients_nr,
				   size_t servers_nr,
				   size_t concurrency_client,
				   size_t concurrency_server,
				   size_t msg_nr,
				   m0_bcount_t msg_size,
				   size_t bd_buf_nr_client,
				   size_t bd_buf_nr_server,
				   m0_bcount_t bd_buf_size,
				   size_t bd_nr_max)
{
	struct m0_net_test_cmd_status_data *sd_servers;
	struct m0_net_test_cmd_status_data *sd_clients;
	struct m0_net_test_console_cfg	   *console_cfg;
	struct m0_net_test_console_ctx	   *console;
	int				    rc;
	int				    i;

	M0_PRE(clients_nr <= NTCS_NODES_MAX);
	M0_PRE(servers_nr <= NTCS_NODES_MAX);

	M0_ALLOC_PTR(console_cfg);
	M0_ALLOC_PTR(console);
	if (console_cfg == NULL || console == NULL)
		goto done;
	/* prepare config for test clients and test servers */
	addr_console4clients = addr_get(nid, NTCS_TMID_CONSOLE4CLIENTS);
	addr_console4servers = addr_get(nid, NTCS_TMID_CONSOLE4SERVERS);
	clients[0] = '\0';
	clients_data[0] = '\0';
	for (i = 0; i < clients_nr; ++i) {
		node_cfg_fill(&node_cfg[i],
			      addr_get(nid, NTCS_TMID_CMD_CLIENTS + i), clients,
			      addr_get(nid, NTCS_TMID_DATA_CLIENTS + i),
			      clients_data, addr_console4clients,
			      i == clients_nr - 1);
	}
	servers[0] = '\0';
	servers_data[0] = '\0';
	for (i = 0; i < servers_nr; ++i) {
		node_cfg_fill(&node_cfg[clients_nr + i],
			      addr_get(nid, NTCS_TMID_CMD_SERVERS + i), servers,
			      addr_get(nid, NTCS_TMID_DATA_SERVERS + i),
			      servers_data, addr_console4servers,
			      i == servers_nr - 1);
	}
	/* spawn test clients and test servers */
	m0_semaphore_init(&node_init_sem, 0);
	for (i = 0; i < clients_nr + servers_nr; ++i) {
		rc = M0_THREAD_INIT(&node_thread[i],
				    struct m0_net_test_node_cfg *,
				    NULL, &net_test_node, &node_cfg[i],
				    "net_test node%d", i);
		M0_UT_ASSERT(rc == 0);
	}
	/* wait until test node started */
	for (i = 0; i < clients_nr + servers_nr; ++i)
		m0_semaphore_down(&node_init_sem);
	m0_semaphore_fini(&node_init_sem);
	/* prepare console config */
	*console_cfg = (struct m0_net_test_console_cfg) {
		.ntcc_addr_console4servers = addr_console4servers,
		.ntcc_addr_console4clients = addr_console4clients,
		.ntcc_cmd_send_timeout	   = timeout,
		.ntcc_cmd_recv_timeout	   = timeout,
		.ntcc_buf_send_timeout	   = timeout,
		.ntcc_buf_recv_timeout	   = timeout,
		.ntcc_buf_bulk_timeout	   = timeout,
		.ntcc_test_type		   = type,
		.ntcc_msg_nr		   = msg_nr,
		.ntcc_test_time_limit	   = M0_TIME_NEVER,
		.ntcc_msg_size		   = msg_size,
		.ntcc_bd_buf_nr_server	   = bd_buf_nr_server,
		.ntcc_bd_buf_nr_client	   = bd_buf_nr_client,
		.ntcc_bd_buf_size	   = bd_buf_size,
		.ntcc_bd_nr_max		   = bd_nr_max,
		.ntcc_concurrency_server   = concurrency_server,
		.ntcc_concurrency_client   = concurrency_client,
	};
	LOGD("addr_console4servers = %s", addr_console4servers);
	LOGD("addr_console4clients = %s", addr_console4clients);
	LOGD("clients      = %s", (char *) clients);
	LOGD("servers      = %s", (char *) servers);
	LOGD("clients_data = %s", (char *) clients_data);
	LOGD("servers_data = %s", (char *) servers_data);
	rc = m0_net_test_slist_init(&console_cfg->ntcc_clients, clients, ',');
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_test_slist_init(&console_cfg->ntcc_servers, servers, ',');
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_test_slist_init(&console_cfg->ntcc_data_clients,
				    clients_data, ',');
	M0_UT_ASSERT(rc == 0);
	rc = m0_net_test_slist_init(&console_cfg->ntcc_data_servers,
				    servers_data, ',');
	M0_UT_ASSERT(rc == 0);
	/* initialize console */
	rc = m0_net_test_console_init(console, console_cfg);
	M0_UT_ASSERT(rc == 0);
	/* send INIT to the test servers */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_SERVER,
				     M0_NET_TEST_CMD_INIT);
	M0_UT_ASSERT(rc == servers_nr);
	/* send INIT to the test clients */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_CLIENT,
				     M0_NET_TEST_CMD_INIT);
	M0_UT_ASSERT(rc == clients_nr);
	/* send START command to the test servers */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_SERVER,
				     M0_NET_TEST_CMD_START);
	M0_UT_ASSERT(rc == servers_nr);
	/* send START command to the test clients */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_CLIENT,
				     M0_NET_TEST_CMD_START);
	M0_UT_ASSERT(rc == clients_nr);
	/* send STATUS command to the test clients until it finishes. */
	do {
		rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_CLIENT,
					     M0_NET_TEST_CMD_STATUS);
		M0_UT_ASSERT(rc == clients_nr);
	} while (!console->ntcc_clients.ntcrc_sd->ntcsd_finished);
	/*
	 * Sleep a second to wait test update its status.
	 * TODO: Making a quiesce command (or is_quiesced query) for servers
	 * which will tell if all buffer callbacks are executed. After this is
	 * done and it’s called from the test, 1s delay will no longer be
	 * needed. So the bug is actually in UT which compares the values that
	 * can’t be compared. From @max.
	 */
	m0_nanosleep(m0_time(1,0), NULL);
	/* send STATUS command to the test clients */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_CLIENT,
				     M0_NET_TEST_CMD_STATUS);
	M0_UT_ASSERT(rc == clients_nr);
	/* send STATUS command to the test servers */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_SERVER,
				     M0_NET_TEST_CMD_STATUS);
	M0_UT_ASSERT(rc == servers_nr);
	msg_nr_print("client msg sent",
		     &console->ntcc_clients.ntcrc_sd->ntcsd_msg_nr_send);
	msg_nr_print("client msg received",
		     &console->ntcc_clients.ntcrc_sd->ntcsd_msg_nr_recv);
	msg_nr_print("client bulk sent",
		     &console->ntcc_clients.ntcrc_sd->ntcsd_bulk_nr_send);
	msg_nr_print("client bulk received",
		     &console->ntcc_clients.ntcrc_sd->ntcsd_bulk_nr_recv);
	msg_nr_print("client transfers",
		     &console->ntcc_clients.ntcrc_sd->ntcsd_transfers);
	msg_nr_print("server msg sent",
		     &console->ntcc_servers.ntcrc_sd->ntcsd_msg_nr_send);
	msg_nr_print("server msg received",
		     &console->ntcc_servers.ntcrc_sd->ntcsd_msg_nr_recv);
	msg_nr_print("server bulk sent",
		     &console->ntcc_servers.ntcrc_sd->ntcsd_bulk_nr_send);
	msg_nr_print("server bulk received",
		     &console->ntcc_servers.ntcrc_sd->ntcsd_bulk_nr_recv);
	msg_nr_print("server transfers",
		     &console->ntcc_servers.ntcrc_sd->ntcsd_transfers);
	/* send STOP command to the test clients */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_CLIENT,
				     M0_NET_TEST_CMD_STOP);
	M0_UT_ASSERT(rc == clients_nr);
	/* send STOP command to the test servers */
	rc = m0_net_test_console_cmd(console, M0_NET_TEST_ROLE_SERVER,
				     M0_NET_TEST_CMD_STOP);
	M0_UT_ASSERT(rc == servers_nr);
	sd_servers = console->ntcc_servers.ntcrc_sd;
	sd_clients = console->ntcc_clients.ntcrc_sd;
	/* check stats */
	nrchk(&sd_servers->ntcsd_msg_nr_send, &sd_clients->ntcsd_msg_nr_recv);
	nrchk(&sd_servers->ntcsd_msg_nr_recv, &sd_clients->ntcsd_msg_nr_send);
	nrchk(&sd_servers->ntcsd_bulk_nr_send, &sd_clients->ntcsd_bulk_nr_recv);
	nrchk(&sd_servers->ntcsd_bulk_nr_recv, &sd_clients->ntcsd_bulk_nr_send);
	if (type == M0_NET_TEST_TYPE_BULK) {
		/*
		 * number of transfers are not measured on the test server
		 * for ping test.
		 */
		nrchk(&sd_servers->ntcsd_transfers,
		      &sd_clients->ntcsd_transfers);
	}
	/* finalize console */
	m0_net_test_slist_fini(&console_cfg->ntcc_servers);
	m0_net_test_slist_fini(&console_cfg->ntcc_clients);
	m0_net_test_slist_fini(&console_cfg->ntcc_data_servers);
	m0_net_test_slist_fini(&console_cfg->ntcc_data_clients);
	m0_net_test_console_fini(console);
	/* finalize test clients and test servers */
	for (i = 0; i < clients_nr + servers_nr; ++i) {
		rc = m0_thread_join(&node_thread[i]);
		M0_UT_ASSERT(rc == 0);
		m0_thread_fini(&node_thread[i]);
		addr_free(node_cfg[i].ntnc_addr);
	}
	addr_free(addr_console4servers);
	addr_free(addr_console4clients);
done:
	m0_free(console);
	m0_free(console_cfg);
}

void m0_net_test_client_server_stub_ut(void)
{
	/* test console-node interaction with dummy node */
	net_test_client_server("0@lo", M0_NET_TEST_TYPE_STUB,
			       1, 1, 1, 1, 1, 1,
			       0, 0, 0, 0);
}

void m0_net_test_client_server_ping_ut(void)
{
	/*
	 * - 0@lo interface
	 * - 8 test clients, 8 test servers
	 * - 8 pairs of concurrent buffers on each test client
	 * - 128 concurrent buffers on each test server
	 * - 4k test messages x 4KiB per message
	 */
	net_test_client_server("0@lo", M0_NET_TEST_TYPE_PING,
			       8, 8, 8, 128, 0x1000, 0x1000,
			       /* 1, 1, 8, 128, 0x1000, 0x1000, */
			       0, 0, 0, 0);
}

void m0_net_test_client_server_bulk_ut(void)
{
	/**
	 * @todo investigate strange m0_net_tm_stop() time
	 * on the bulk test client.
	 */
	/*
	 * - 0@lo interface
	 * - 2 test clients, 2 test servers
	 * - 2 pairs of concurrent buffers on each test client
	 * - 8 concurrent buffers on each test server
	 * - 64 test messages x 1MiB per message
	 * - 8(16) network buffers for network buffer descriptors
	 *   on the test client(server) with 4KiB per buffer and
	 *   64k maximum buffer descriptors in buffer
	 */
	net_test_client_server("0@lo", M0_NET_TEST_TYPE_BULK,
			       2, 2, 2, 8,
			       64, 0x100000,
			       8, 16, 0x1000, 0x10000);
}

#undef NET_TEST_MODULE_NAME

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
