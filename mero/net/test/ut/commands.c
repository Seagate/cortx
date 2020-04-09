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
 * Original creation date: 06/06/2012
 */

#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/semaphore.h"	/* m0_semaphore */
#include "lib/memory.h"		/* m0_alloc */
#include "lib/time.h"		/* m0_time_seconds */
#include "lib/errno.h"          /* ENOENT */
#include "net/lnet/lnet.h"	/* m0_net_lnet_ifaces_get */

#include "net/test/commands.h"

/* NTC_ == NET_TEST_COMMANDS_ */
enum {
	NTC_TMID_START	      = 300,
	NTC_TMID_CONSOLE      = NTC_TMID_START,
	NTC_TMID_NODE	      = NTC_TMID_CONSOLE + 1,
	NTC_MULTIPLE_NODES    = 64,
	NTC_MULTIPLE_COMMANDS = 32,
	NTC_ADDR_LEN_MAX      = 0x100,
	NTC_TIMEOUT_MS	      = 1000,
	NTC_CMD_RECV_WAIT_NS  = 25000000,
};

static const char   NTC_ADDR[]	   = "0@lo:12345:42:%d";
static const size_t NTC_ADDR_LEN   = ARRAY_SIZE(NTC_ADDR);
static const char   NTC_DELIM      = ',';

struct net_test_cmd_node {
	struct m0_thread	   ntcn_thread;
	struct m0_net_test_cmd_ctx ntcn_ctx;
	/** index (range: [0..nr)) in nodes list */
	int			   ntcn_index;
	/** used for barriers with the main thread */
	struct m0_semaphore	   ntcn_signal;
	struct m0_semaphore	   ntcn_wait;
	/** number of failures */
	unsigned long		   ntcn_failures;
	/** flag: barriers are disabled for this node */
	bool			   ntcn_barriers_disabled;
	/** used when checking send/recv */
	bool			   ntcn_flag;
	/** transfer machine address */
	char			  *ntcn_addr;
};

static char addr_console[NTC_ADDR_LEN_MAX];
static char addr_node[NTC_ADDR_LEN_MAX * NTC_MULTIPLE_NODES];

static struct m0_net_test_slist   slist_node;
static struct m0_net_test_slist   slist_console;
static struct net_test_cmd_node	 *node;
static struct m0_net_test_cmd_ctx console;
static struct net_test_cmd_node	  nodes[NTC_MULTIPLE_NODES];

static m0_time_t timeout_get(void)
{
	return m0_time(NTC_TIMEOUT_MS / 1000, NTC_TIMEOUT_MS % 1000 * 1000000);
}

static m0_time_t timeout_get_abs(void)
{
	return m0_time_add(m0_time_now(), timeout_get());
}

static int make_addr(char *s, size_t s_len, int svc_id, bool add_comma)
{
	int rc = snprintf(s, s_len, NTC_ADDR, svc_id);

	M0_ASSERT(NTC_ADDR_LEN <= NTC_ADDR_LEN_MAX);
	M0_ASSERT(rc > 0);

	if (add_comma) {
		s[rc++] = NTC_DELIM;
		s[rc] = '\0';
	}
	return rc;
}

/** Fill addr_console and addr_node strings. */
static void fill_addr(uint32_t nr)
{
	char    *pos = addr_node;
	uint32_t i;
	int	 diff;

	/* console */
	make_addr(addr_console, NTC_ADDR_LEN_MAX, NTC_TMID_CONSOLE, false);
	/* nodes */
	for (i = 0; i < nr; ++i) {
		diff = make_addr(pos, NTC_ADDR_LEN_MAX,
				 NTC_TMID_NODE + i, i != nr - 1);
		M0_ASSERT(diff < NTC_ADDR_LEN_MAX);
		pos += diff;
	}
}

/**
   M0_UT_ASSERT() isn't thread-safe, so node->ntcn_failures is used for
   assertion failure tracking.
   Assuming that M0_UT_ASSERT() will abort program, otherwise
   will be a deadlock in the barrier_with_main()/barrier_with_nodes().
   Called from the node threads.
   @see command_ut_check(), commands_node_thread(), net_test_command_ut().
 */
static bool commands_ut_assert(struct net_test_cmd_node *node, bool value)
{
	node->ntcn_failures += !value;
	M0_UT_ASSERT(value);
	return value;
}

/** Called from the main thread */
static void barrier_init(struct net_test_cmd_node *node)
{
	int rc = m0_semaphore_init(&node->ntcn_signal, 0);
	M0_UT_ASSERT(rc == 0);
	rc = m0_semaphore_init(&node->ntcn_wait, 0);
	M0_UT_ASSERT(rc == 0);
	node->ntcn_barriers_disabled = false;
}

/**
   Called from the main thread.
   @see net_test_command_ut().
 */
static void barrier_fini(struct net_test_cmd_node *node)
{
	m0_semaphore_fini(&node->ntcn_signal);
	m0_semaphore_fini(&node->ntcn_wait);
}

/**
   Called from the node threads.
   @see commands_node_thread().
 */
static void barrier_with_main(struct net_test_cmd_node *node)
{
	if (!node->ntcn_barriers_disabled) {
		m0_semaphore_up(&node->ntcn_signal);
		m0_semaphore_down(&node->ntcn_wait);
	}
}

/**
   Called from the main thread.
   Also checks for UT failures.
   @see net_test_command_ut().
 */
static void barrier_with_nodes(void)
{
	int i;

	for (i = 0; i < slist_node.ntsl_nr; ++i)
		if (!node[i].ntcn_barriers_disabled)
			m0_semaphore_down(&node[i].ntcn_signal);

	for (i = 0; i < slist_node.ntsl_nr; ++i)
		M0_UT_ASSERT(node[i].ntcn_failures == 0);

	for (i = 0; i < slist_node.ntsl_nr; ++i)
		if (!node[i].ntcn_barriers_disabled)
			m0_semaphore_up(&node[i].ntcn_wait);
}

/**
   Called from the node threads.
 */
static void barrier_disable(struct net_test_cmd_node *node)
{
	node->ntcn_barriers_disabled = true;
	m0_semaphore_up(&node->ntcn_signal);
}

static void flags_reset(size_t nr)
{
	size_t i;

	for (i = 0; i < nr; ++i)
		node[i].ntcn_flag = false;
}

static void flag_set(int index)
{
	node[index].ntcn_flag = true;
}

static bool is_flags_set(size_t nr, bool set)
{
	size_t i;

	for (i = 0; i < nr; ++i)
		if (node[i].ntcn_flag == !set)
			return false;
	return true;
}

static bool is_flags_set_odd(size_t nr)
{
	size_t i;

	for (i = 0; i < nr; ++i)
		if (node[i].ntcn_flag == (i + 1) % 2)
			return false;
	return true;
}

static void commands_ut_send(struct net_test_cmd_node *node,
			     struct m0_net_test_cmd_ctx *ctx)
{
	struct m0_net_test_cmd cmd;
	int		       rc;

	M0_SET0(&cmd);
	cmd.ntc_type = M0_NET_TEST_CMD_STOP_DONE;
	cmd.ntc_done.ntcd_errno = -node->ntcn_index;
	cmd.ntc_ep_index = 0;
	rc = m0_net_test_commands_send(ctx, &cmd);
	commands_ut_assert(node, rc == 0);
	m0_net_test_commands_send_wait_all(ctx);
}

static void commands_ut_recv(struct net_test_cmd_node *node,
			     struct m0_net_test_cmd_ctx *ctx,
			     m0_time_t deadline,
			     ssize_t *ep_index)
{
	struct m0_net_test_cmd cmd;
	int		       rc;

	M0_SET0(&cmd);
	rc = m0_net_test_commands_recv(ctx, &cmd, deadline);
	commands_ut_assert(node, rc == 0);
	if (rc != 0)
		return;
	if (ep_index == NULL) {
		rc = m0_net_test_commands_recv_enqueue(ctx, cmd.ntc_buf_index);
		commands_ut_assert(node, rc == 0);
	} else {
		*ep_index = cmd.ntc_buf_index;
	}
	commands_ut_assert(node, cmd.ntc_type == M0_NET_TEST_CMD_STOP);
	commands_ut_assert(node, cmd.ntc_ep_index == 0);
	m0_net_test_commands_received_free(&cmd);
	flag_set(node->ntcn_index);
}

static void commands_ut_recv_type(struct net_test_cmd_node *node,
				  struct m0_net_test_cmd_ctx *ctx,
				  m0_time_t deadline,
				  enum m0_net_test_cmd_type type)
{
	struct m0_net_test_cmd_init *cmd_init;
	struct m0_net_test_cmd	     cmd;
	int			     rc;
	m0_time_t		     timeout;

	M0_SET0(&cmd);
	rc = m0_net_test_commands_recv(ctx, &cmd, deadline);
	commands_ut_assert(node, rc == 0);
	rc = m0_net_test_commands_recv_enqueue(ctx, cmd.ntc_buf_index);
	commands_ut_assert(node, rc == 0);
	commands_ut_assert(node, cmd.ntc_type == type);
	if (cmd.ntc_type == M0_NET_TEST_CMD_INIT) {
		cmd_init = &cmd.ntc_init;
		commands_ut_assert(node, cmd_init->ntci_role ==
					 M0_NET_TEST_ROLE_SERVER);
		commands_ut_assert(node, cmd_init->ntci_type ==
					 M0_NET_TEST_TYPE_BULK);
		commands_ut_assert(node, cmd_init->ntci_msg_nr	 == 0x10000);
		commands_ut_assert(node, cmd_init->ntci_msg_size == 0x100000);
		commands_ut_assert(node,
				   cmd_init->ntci_msg_concurrency == 0x100);
		timeout = cmd_init->ntci_buf_send_timeout;
		commands_ut_assert(node, m0_time_seconds(timeout) == 2);
		commands_ut_assert(node, m0_time_nanoseconds(timeout) == 3);
		commands_ut_assert(node, strncmp(cmd_init->ntci_tm_ep,
					         "0@lo:1:2:3", 64) == 0);
		/*
		 * m0_net_test_slist serialize/deserialize already checked
		 * in slist UT
		 */
		commands_ut_assert(node, cmd_init->ntci_ep.ntsl_nr == 3);
	}
	m0_net_test_commands_received_free(&cmd);
	flag_set(node->ntcn_index);
}

static void commands_node_thread(struct net_test_cmd_node *node)
{
	struct m0_net_test_cmd_ctx *ctx;
	int			    rc;
	ssize_t			    buf_index;

	if (node == NULL)
		return;
	ctx = &node->ntcn_ctx;

	node->ntcn_failures = 0;
	rc = m0_net_test_commands_init(ctx,
				       slist_node.ntsl_list[node->ntcn_index],
				       timeout_get(), NULL, &slist_console);
	if (!commands_ut_assert(node, rc == 0))
		return barrier_disable(node);

	barrier_with_main(node);	/* barrier #0 */
	commands_ut_recv(node, ctx, M0_TIME_NEVER, NULL);/* test #1 */
	barrier_with_main(node);	/* barrier #1.0 */
	/* main thread will check flags here */
	barrier_with_main(node);	/* barrier #1.1 */
	commands_ut_send(node, ctx);			/* test #2 */
	barrier_with_main(node);	/* barrier #2 */
	if (node->ntcn_index % 2 != 0)			/* test #3 */
		commands_ut_send(node, ctx);
	barrier_with_main(node);	/* barrier #3.0 */
	barrier_with_main(node);	/* barrier #4.0 */
	if (node->ntcn_index % 2 != 0)			/* test #4 */
		commands_ut_recv(node, ctx, timeout_get_abs(), &buf_index);
	barrier_with_main(node);	/* barrier #4.1 */
	if (node->ntcn_index % 2 != 0) {
		rc = m0_net_test_commands_recv_enqueue(ctx, buf_index);
	}
	barrier_with_main(node);	/* barrier #4.2 */
	/* main thread will check flags here */
	barrier_with_main(node);	/* barrier #4.3 */
	if (node->ntcn_index % 2 == 0) {
		commands_ut_recv(node, ctx, timeout_get_abs(), NULL);
	}
	commands_ut_send(node, ctx);			/* test #5 */
	commands_ut_send(node, ctx);
	barrier_with_main(node);	/* barrier #5.0 */
	/* main thread will start receiving here */
	barrier_with_main(node);	/* barrier #5.1 */
							/* test #6 */
	barrier_with_main(node);	/* barrier #6.0 */
	commands_ut_recv_type(node, ctx, M0_TIME_NEVER, M0_NET_TEST_CMD_INIT);
	barrier_with_main(node);	/* barrier #6.1 */
	barrier_with_main(node);	/* barrier #6.2 */
	commands_ut_recv_type(node, ctx, M0_TIME_NEVER, M0_NET_TEST_CMD_START);
	barrier_with_main(node);	/* barrier #6.3 */
	barrier_with_main(node);	/* barrier #6.4 */
	commands_ut_recv_type(node, ctx, M0_TIME_NEVER, M0_NET_TEST_CMD_STOP);
	barrier_with_main(node);	/* barrier #6.5 */
	barrier_with_main(node);	/* barrier #6.6 */
	commands_ut_recv_type(node, ctx, M0_TIME_NEVER, M0_NET_TEST_CMD_STATUS);
	barrier_with_main(node);	/* barrier #6.7 */
	barrier_with_main(node);	/* barrier #6.8 */
	m0_net_test_commands_fini(&node->ntcn_ctx);
}

static void send_all(size_t nr, struct m0_net_test_cmd *cmd)
{
	int i;
	int rc;

	for (i = 0; i < nr; ++i) {
		cmd->ntc_ep_index = i;
		rc = m0_net_test_commands_send(&console, cmd);
		M0_UT_ASSERT(rc == 0);
	}
	m0_net_test_commands_send_wait_all(&console);
}

static void commands_ut_send_all(size_t nr)
{
	struct m0_net_test_cmd cmd;

	M0_SET0(&cmd);
	cmd.ntc_type = M0_NET_TEST_CMD_STOP;
	send_all(nr, &cmd);
}

static void commands_ut_send_all_type(size_t nr,
				      enum m0_net_test_cmd_type type)
{
	struct m0_net_test_cmd_init *cmd_init;
	struct m0_net_test_cmd	     cmd;

	M0_SET0(&cmd);
	cmd.ntc_type = type;
	if (type == M0_NET_TEST_CMD_INIT) {
		cmd_init			= &cmd.ntc_init;
		cmd_init->ntci_role		= M0_NET_TEST_ROLE_SERVER;
		cmd_init->ntci_type		= M0_NET_TEST_TYPE_BULK;
		cmd_init->ntci_msg_nr		= 0x10000;
		cmd_init->ntci_msg_size		= 0x100000;
		cmd_init->ntci_msg_concurrency  = 0x100;
		cmd_init->ntci_buf_send_timeout = M0_MKTIME(2, 3);
		cmd_init->ntci_tm_ep		= "0@lo:1:2:3";
		m0_net_test_slist_init(&cmd_init->ntci_ep, "1,2,3", ',');
	} else if (type != M0_NET_TEST_CMD_START &&
		   type != M0_NET_TEST_CMD_STOP &&
		   type != M0_NET_TEST_CMD_STATUS) {
		M0_IMPOSSIBLE("net-test commands UT: invalid command type");
	}
	send_all(nr, &cmd);
	if (type == M0_NET_TEST_CMD_INIT)
		m0_net_test_slist_fini(&cmd_init->ntci_ep);
}

static void commands_ut_recv_all(size_t nr, m0_time_t deadline)
{
	struct m0_net_test_cmd cmd;
	int		       i;
	int		       rc;

	for (i = 0; i < nr; ++i) {
		rc = m0_net_test_commands_recv(&console, &cmd, deadline);
		if (rc == -ETIMEDOUT)
			break;
		M0_UT_ASSERT(rc == 0);
		rc = m0_net_test_commands_recv_enqueue(&console,
						       cmd.ntc_buf_index);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(cmd.ntc_type == M0_NET_TEST_CMD_STOP_DONE);
		M0_UT_ASSERT(cmd.ntc_done.ntcd_errno == -cmd.ntc_ep_index);
		m0_net_test_commands_received_free(&cmd);
		flag_set(cmd.ntc_ep_index);
	}
}

static void net_test_command_ut(size_t nr)
{
	size_t i;
	int    rc;
	bool   rc_bool;

	M0_UT_ASSERT(nr > 0);

	/* prepare addresses */
	fill_addr(nr);
	rc = m0_net_test_slist_init(&slist_node, addr_node, NTC_DELIM);
	M0_UT_ASSERT(rc == 0);
	rc_bool = m0_net_test_slist_unique(&slist_node);
	M0_UT_ASSERT(rc_bool);
	rc = m0_net_test_slist_init(&slist_console, addr_console, NTC_DELIM);
	M0_UT_ASSERT(rc == 0);
	rc_bool = m0_net_test_slist_unique(&slist_console);
	M0_UT_ASSERT(rc_bool);
	/* init console */
	rc = m0_net_test_commands_init(&console, addr_console, timeout_get(),
				       /** @todo set callback */
				       NULL, &slist_node);
	M0_UT_ASSERT(rc == 0);
	/* alloc nodes */
	M0_ALLOC_ARR(node, nr);
	M0_UT_ASSERT(node != NULL);

	/*
	   start thread for every node because:
	   - some of m0_net_test_commands_*() functions have blocking interface;
	   - m0_net transfer machines parallel initialization is much faster
	     then serial.
	 */
	for (i = 0; i < nr; ++i) {
		barrier_init(&node[i]);
		node[i].ntcn_index = i;
		rc = M0_THREAD_INIT(&node[i].ntcn_thread,
				    struct net_test_cmd_node *,
				    NULL,
				    &commands_node_thread,
				    &node[i],
				    "node_thread_#%d",
				    (int) i);
		M0_UT_ASSERT(rc == 0);
	}

	barrier_with_nodes();				/* barrier #0 */
	/*
	   Test #1: console sends command to every node.
	 */
	flags_reset(nr);
	commands_ut_send_all(nr);
	barrier_with_nodes();				/* barrier #1.0 */
	M0_UT_ASSERT(is_flags_set(nr, true));
	barrier_with_nodes();				/* barrier #1.1 */
	/*
	   Test #2: every node sends command to console.
	 */
	flags_reset(nr);
	commands_ut_recv_all(nr, M0_TIME_NEVER);
	M0_UT_ASSERT(is_flags_set(nr, true));
	barrier_with_nodes();				/* barrier #2 */
	/*
	   Test #3: half of nodes (node #0, #2, #4, #6, ...) do not send
	   commands, but other half of nodes send.
	   Console receives commands from every node.
	 */
	flags_reset(nr);
	commands_ut_recv_all(nr, timeout_get_abs());
	M0_UT_ASSERT(is_flags_set_odd(nr));
	barrier_with_nodes();				/* barrier #3.0 */
	/*
	   Test #4: half of nodes (node #0, #2, #4, #6, ...) do not start
	   waiting for commands, but other half of nodes start.
	   Console sends commands to every node.
	 */
	barrier_with_nodes();				/* barrier #4.0 */
	flags_reset(nr);
	commands_ut_send_all(nr);
	barrier_with_nodes();				/* barrier #4.1 */
	barrier_with_nodes();				/* barrier #4.2 */
	M0_UT_ASSERT(is_flags_set_odd(nr));
	barrier_with_nodes();				/* barrier #4.3 */
	/*
	   Test #5: every node sends two commands, and only after that console
	   starts to receive.
	 */
	/* nodes will send two commands here */
	barrier_with_nodes();				/* barrier #5.0 */
	commands_ut_recv_all(nr, M0_TIME_NEVER);
	flags_reset(nr);
	commands_ut_recv_all(nr, timeout_get_abs());
	M0_UT_ASSERT(is_flags_set(nr, false));
	barrier_with_nodes();				/* barrier #5.1 */
	/*
	   Test #6: console sends all types of commands to every node.
	 */
	barrier_with_nodes();				/* barrier #6.0 */
	flags_reset(nr);
	commands_ut_send_all_type(nr, M0_NET_TEST_CMD_INIT);
	barrier_with_nodes();				/* barrier #6.1 */
	M0_UT_ASSERT(is_flags_set(nr, true));
	barrier_with_nodes();				/* barrier #6.2 */
	flags_reset(nr);
	commands_ut_send_all_type(nr, M0_NET_TEST_CMD_START);
	barrier_with_nodes();				/* barrier #6.3 */
	M0_UT_ASSERT(is_flags_set(nr, true));
	barrier_with_nodes();				/* barrier #6.4 */
	flags_reset(nr);
	commands_ut_send_all_type(nr, M0_NET_TEST_CMD_STOP);
	barrier_with_nodes();				/* barrier #6.5 */
	M0_UT_ASSERT(is_flags_set(nr, true));
	barrier_with_nodes();				/* barrier #6.6 */
	flags_reset(nr);
	commands_ut_send_all_type(nr, M0_NET_TEST_CMD_STATUS);
	barrier_with_nodes();				/* barrier #6.7 */
	M0_UT_ASSERT(is_flags_set(nr, true));
	barrier_with_nodes();				/* barrier #6.8 */
	/* stop all threads */
	for (i = 0; i < nr; ++i) {
		rc = m0_thread_join(&node[i].ntcn_thread);
		M0_UT_ASSERT(rc == 0);
		m0_thread_fini(&node[i].ntcn_thread);
		barrier_fini(&node[i]);
	}

	/* cleanup */
	m0_free(node);
	m0_net_test_commands_fini(&console);
	m0_net_test_slist_fini(&slist_console);
	m0_net_test_slist_fini(&slist_node);
}

void m0_net_test_cmd_ut_single(void)
{
	net_test_command_ut(1);
}

void m0_net_test_cmd_ut_multiple(void)
{
	net_test_command_ut(NTC_MULTIPLE_NODES);
}

static void commands_node_loop(struct net_test_cmd_node *node,
			       struct m0_net_test_cmd *cmd)
{
	m0_time_t deadline;
	int	  cmd_rcvd = 0;
	int	  rc;

	/*
	 * Receive command from console and send it back.
	 * Repeat NTC_MULTIPLE_COMMANDS times.
	 */
	while (cmd_rcvd != NTC_MULTIPLE_COMMANDS) {
		deadline = m0_time_from_now(0, NTC_CMD_RECV_WAIT_NS);
		rc = m0_net_test_commands_recv(&node->ntcn_ctx, cmd, deadline);
		if (rc == -ETIMEDOUT)
			continue;
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(cmd->ntc_ep_index == 0);
		rc = m0_net_test_commands_recv_enqueue(&node->ntcn_ctx,
						       cmd->ntc_buf_index);
		M0_UT_ASSERT(rc == 0);
		m0_net_test_commands_send_wait_all(&node->ntcn_ctx);
		rc = m0_net_test_commands_send(&node->ntcn_ctx, cmd);
		M0_UT_ASSERT(rc == 0);
		m0_net_test_commands_received_free(cmd);
		++cmd_rcvd;
	}
}

static void commands_console_loop(struct net_test_cmd_node *node,
				  struct m0_net_test_cmd *cmd)
{
	m0_time_t deadline;
	int	  cmd_sent = 0;
	int	  cmd_rcvd;
	int	  i;
	int	  rc;

	/*
	 * Send command to every node and receive reply.
	 * Repeat NTC_MULTIPLE_COMMANDS times.
	 */
	while (cmd_sent != NTC_MULTIPLE_COMMANDS) {
		m0_net_test_commands_send_wait_all(&node->ntcn_ctx);
		for (i = 1; i < ARRAY_SIZE(nodes); ++i) {
			cmd->ntc_ep_index = i - 1;
			cmd->ntc_type = M0_NET_TEST_CMD_INIT_DONE;
			rc = m0_net_test_commands_send(&node->ntcn_ctx, cmd);
			M0_UT_ASSERT(rc == 0);
		}
		m0_net_test_commands_send_wait_all(&node->ntcn_ctx);
		cmd_rcvd = 0;
		deadline = timeout_get_abs();
		while (cmd_rcvd != ARRAY_SIZE(nodes) - 1 &&
		       m0_time_now() <= deadline) {
			rc = m0_net_test_commands_recv(&node->ntcn_ctx, cmd,
						       deadline);
			M0_UT_ASSERT(rc == 0);
			rc = m0_net_test_commands_recv_enqueue(&node->ntcn_ctx,
							cmd->ntc_buf_index);
			M0_UT_ASSERT(rc == 0);
			++cmd_rcvd;
			m0_net_test_commands_received_free(cmd);
		}
		M0_UT_ASSERT(cmd_rcvd == ARRAY_SIZE(nodes) - 1);
		++cmd_sent;
	}
}

static void commands_node_thread2(struct net_test_cmd_node *node)
{
	struct m0_net_test_cmd	 *cmd;
	struct m0_net_test_slist  endpoints;
	static char		  buf[NTC_MULTIPLE_NODES * NTC_ADDR_LEN_MAX];
	int			  rc;
	bool			  console_thread = node == &nodes[0];
	int			  i;

	M0_ALLOC_PTR(cmd);
	M0_UT_ASSERT(cmd != NULL);

	if (console_thread) {
		buf[0] = '\0';
		for (i = 1; i < ARRAY_SIZE(nodes); ++i) {
			strncat(buf, nodes[i].ntcn_addr, NTC_ADDR_LEN_MAX - 1);
			if (i != ARRAY_SIZE(nodes) - 1)
				strncat(buf, ",", 1);
		}
		rc = m0_net_test_slist_init(&endpoints, buf, ',');
		M0_UT_ASSERT(rc == 0);
	} else {
		rc = m0_net_test_slist_init(&endpoints,
					    nodes[0].ntcn_addr, '`');
		M0_UT_ASSERT(rc == 0);
	}
	rc = m0_net_test_commands_init(&node->ntcn_ctx, node->ntcn_addr,
				       timeout_get(), NULL, &endpoints);
	M0_UT_ASSERT(rc == 0);

	barrier_with_main(node);

	if (console_thread) {
		commands_console_loop(node, cmd);
	} else {
		commands_node_loop(node, cmd);
	}

	m0_net_test_commands_fini(&node->ntcn_ctx);
	m0_net_test_slist_fini(&endpoints);
	m0_free(cmd);
}

/* main thread */
void m0_net_test_cmd_ut_multiple2(void)
{
	struct net_test_cmd_node *node;
	size_t			  i;
	int			  rc;

	/* console is node #0 */
	for (i = 0; i < ARRAY_SIZE(nodes); ++i) {
		node = &nodes[i];
		barrier_init(node);
		node->ntcn_addr = m0_alloc(NTC_ADDR_LEN_MAX);
		M0_UT_ASSERT(node->ntcn_addr != NULL);
		make_addr(node->ntcn_addr, NTC_ADDR_LEN_MAX,
			  NTC_TMID_START + i, false);
	}
	/* start threads */
	for (i = 0; i < ARRAY_SIZE(nodes); ++i) {
		node = &nodes[i];
		rc = M0_THREAD_INIT(&node->ntcn_thread,
				    struct net_test_cmd_node *, NULL,
				    &commands_node_thread2, node,
				    "cmd_ut #%d", (int)i);
		M0_UT_ASSERT(rc == 0);
	}
	/* barrier with node threads */
	for (i = 0; i < ARRAY_SIZE(nodes); ++i)
		m0_semaphore_down(&nodes[i].ntcn_signal);
	for (i = 0; i < ARRAY_SIZE(nodes); ++i)
		m0_semaphore_up(&nodes[i].ntcn_wait);
	/* stop threads */
	for (i = 0; i < ARRAY_SIZE(nodes); ++i) {
		node = &nodes[i];
		rc = m0_thread_join(&node->ntcn_thread);
		M0_UT_ASSERT(rc == 0);
		m0_thread_fini(&node->ntcn_thread);
	}
	/* fini nodes */
	for (i = 0; i < ARRAY_SIZE(nodes); ++i) {
		node = &nodes[i];
		m0_free(node->ntcn_addr);
		barrier_fini(node);
	}
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
