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
 * Original creation date: 09/03/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_CONSOLE_H__
#define __MERO_NET_TEST_CONSOLE_H__

#include "lib/time.h"			/* m0_time_t */
#include "lib/types.h"			/* m0_bcount_t */

#include "net/test/commands.h"		/* m0_net_test_cmd_ctx */
#include "net/test/node.h"		/* m0_net_test_role */
#include "net/test/slist.h"		/* m0_net_test_slist */


/**
   @defgroup NetTestConsoleDFS Test Console
   @ingroup NetTestDFS

   @see
   @ref net-test

   @{
 */

/**
 * Console configuration.
 * Set by console user before calling m0_net_test_init()
 */
struct m0_net_test_console_cfg {
	/** Console commands endpoint address for the test servers */
	char			*ntcc_addr_console4servers;
	/** Console commands endpoint address for the test clients */
	char			*ntcc_addr_console4clients;
	/**
	 * List of test server command endpoints.
	 * Test console will use this endpoints for sending/receiving
	 * commands to/from test servers.
	 */
	struct m0_net_test_slist ntcc_servers;
	/**
	 * List of test client command endpoints.
	 * @see m0_net_test_console_cfg.ntcc_servers
	 */
	struct m0_net_test_slist ntcc_clients;
	/**
	 * List of test server data endpoints.
	 * Every test server will create one transfer machine with endpoint
	 * from this list. Every test client will send/receive test messages
	 * to/from all test server data endpoints.
	 */
	struct m0_net_test_slist ntcc_data_servers;
	/**
	 * List of test client data endpoints.
	 * @see m0_net_test_console_cfg.ntcc_data_servers
	 */
	struct m0_net_test_slist ntcc_data_clients;
	/** Commands send timeout for the test nodes and test console */
	m0_time_t		 ntcc_cmd_send_timeout;
	/** Commands receive timeout for the test nodes and test console */
	m0_time_t		 ntcc_cmd_recv_timeout;
	/** Test messages send timeout for the test nodes */
	m0_time_t		 ntcc_buf_send_timeout;
	/** Test messages receive timeout for the test nodes */
	m0_time_t		 ntcc_buf_recv_timeout;
	/**
	 * Test messages receive timeout for the bulk transfers
	 * in bulk testing on the test nodes
	 */
	m0_time_t		 ntcc_buf_bulk_timeout;
	/** Test type */
	enum m0_net_test_type	 ntcc_test_type;
	/** Number of test messages for the test client */
	uint64_t		 ntcc_msg_nr;
	/**
	 * Test run time limit. The test will stop after
	 * ntcc_msg_nr test messages or when time limit reached.
	 */
	m0_time_t		 ntcc_test_time_limit;
	/** Test messages size */
	m0_bcount_t		 ntcc_msg_size;
	/**
	 * Number of buffers for bulk buffer network descriptors
	 * for the test server.
	 * @note Used in bulk testing only.
	 */
	uint64_t		 ntcc_bd_buf_nr_server;
	/**
	 * Number of buffers for bulk buffer network descriptors
	 * for the test client.
	 * @note Used in bulk testing only.
	 */
	uint64_t		 ntcc_bd_buf_nr_client;
	/**
	 * Size of buffer for bulk buffer network descriptors.
	 * @note Used in bulk testing only.
	 */
	m0_bcount_t		 ntcc_bd_buf_size;
	/**
	 * Maximum number of bulk buffer network descriptors in
	 * msg buffer.
	 * @see node_bulk_ctx.nbc_bd_nr_max
	 * @note Used in bulk testing only.
	 */
	uint64_t		 ntcc_bd_nr_max;
	/**
	 * Test server concurrency.
	 * @see m0_net_test_cmd_init.ntci_msg_concurrency
	 */
	uint64_t		 ntcc_concurrency_server;
	/**
	 * Test client concurrency.
	 * @see m0_net_test_cmd_init.ntci_msg_concurrency
	 */
	uint64_t		 ntcc_concurrency_client;
};

/** Test console context for the node role */
struct m0_net_test_console_role_ctx {
	/** Commands structure */
	struct m0_net_test_cmd_ctx	   *ntcrc_cmd;
	/** Accumulated status data */
	struct m0_net_test_cmd_status_data *ntcrc_sd;
	/** Number of nodes */
	size_t				    ntcrc_nr;
	/** -errno for the last function */
	int				   *ntcrc_errno;
	/** status of last received *_DONE command */
	int				   *ntcrc_status;
	/** number of m0_net_test_commands_recv() failures */
	size_t				    ntcrc_recv_errors;
	/** number of m0_net_test_commands_recv_enqueue() failures */
	size_t				    ntcrc_recv_enqueue_errors;
};

/** Test console context */
struct m0_net_test_console_ctx {
	/** Test console configuration */
	struct m0_net_test_console_cfg	   *ntcc_cfg;
	/** Test clients */
	struct m0_net_test_console_role_ctx ntcc_clients;
	/** Test servers */
	struct m0_net_test_console_role_ctx ntcc_servers;
};

int m0_net_test_console_init(struct m0_net_test_console_ctx *ctx,
			     struct m0_net_test_console_cfg *cfg);

void m0_net_test_console_fini(struct m0_net_test_console_ctx *ctx);

/**
   Send command from console to the set of test nodes and wait for reply.
   @param ctx Test console context.
   @param role Test node role. Test console will send commands to
	       nodes with this role only.
   @param cmd_type Command type. Test console will create and send command
		   with this type and wait for the corresponding reply
		   type (for example, if cmd_type == M0_NET_TEST_CMD_INIT,
		   then test console will wait for M0_NET_TEST_CMD_INIT_DONE
		   reply).
   @return number of successfully received replies for sent command.
 */
size_t m0_net_test_console_cmd(struct m0_net_test_console_ctx *ctx,
			       enum m0_net_test_role role,
			       enum m0_net_test_cmd_type cmd_type);

/**
   @} end of NetTestConsoleDFS group
 */

#endif /*  __MERO_NET_TEST_CONSOLE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
