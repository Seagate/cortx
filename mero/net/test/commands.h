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
 * Original creation date: 05/05/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_COMMANDS_H__
#define __MERO_NET_TEST_COMMANDS_H__

#include "lib/semaphore.h"		/* m0_semaphore */

#include "net/test/slist.h"		/* m0_net_test_slist */
#include "net/test/ringbuf.h"		/* m0_net_test_ringbuf */
#include "net/test/stats.h"		/* m0_net_test_stats */
#include "net/test/network.h"		/* m0_net_test_network_ctx */

/**
   @defgroup NetTestCommandsDFS Commands
   @ingroup NetTestDFS

   @see
   @ref net-test

   @{
 */

enum {
	/**
	 * It is a size of command network buffer.
	 * Command will be serialized to this buffer, so it should
	 * be large enough to contain entire serialized command.
	 * @note There can be a problem with large number of test nodes,
	 * because M0_NET_TEST_CMD_INIT have list of node endpoints inside.
	 */
	M0_NET_TEST_CMD_SIZE_MAX = 16384,
};

/**
   Test node role - node can be test client or test server.
 */
enum m0_net_test_role {
	M0_NET_TEST_ROLE_CLIENT,
	M0_NET_TEST_ROLE_SERVER
};

/**
   Test type - can be ping test or bulk test.
   M0_NET_TEST_TYPE_STUB used in UTs.
 */
enum m0_net_test_type {
	M0_NET_TEST_TYPE_PING,
	M0_NET_TEST_TYPE_BULK,
	M0_NET_TEST_TYPE_STUB,
};

/**
   Command type.
   @see m0_net_test_cmd
 */
enum m0_net_test_cmd_type {
	M0_NET_TEST_CMD_INIT = 0,
	M0_NET_TEST_CMD_INIT_DONE,
	M0_NET_TEST_CMD_START,
	M0_NET_TEST_CMD_START_DONE,
	M0_NET_TEST_CMD_STATUS,
	M0_NET_TEST_CMD_STATUS_DATA,
	M0_NET_TEST_CMD_STOP,
	M0_NET_TEST_CMD_STOP_DONE,
	M0_NET_TEST_CMD_NR,
};

/**
   M0_NET_TEST_CMD_*_DONE.
   @see m0_net_test_cmd
 */
struct m0_net_test_cmd_done {
	int ntcd_errno;
};

/**
   M0_NET_TEST_CMD_INIT.
   @see m0_net_test_cmd
 */
struct m0_net_test_cmd_init {
	/** Node role */
	enum m0_net_test_role	 ntci_role;
	/** Node type */
	enum m0_net_test_type	 ntci_type;
	/**
	 * Number of test messages for the test client.
	 * It is number of ping messages for the ping test and
	 * number of bulk messages for the bulk test.
	 */
	uint64_t		 ntci_msg_nr;
	/**
	 * Test message size.
	 * It is size of ping buffer for the ping test and
	 * size for bulk buffer for the bulk test.
	 */
	m0_bcount_t		 ntci_msg_size;
	/**
	 * Number of message buffers for bulk buffer descriptors
	 * in the bulk test.
	 * @note Unused in ping test.
	 */
	uint64_t		 ntci_bd_buf_nr;
	/**
	 * Size of message buffer for bulk buffer descriptors
	 * in the bulk test.
	 * @note Unused in ping test.
	 */
	m0_bcount_t		 ntci_bd_buf_size;
	/**
	 * Maximum number of bulk message descriptors in
	 * the message buffer in the bulk test.
	 * @note Unused in ping test.
	 */
	uint64_t		 ntci_bd_nr_max;
	/**
	 * Test messages concurrency.
	 * Test server will allocate ntci_msg_concurrency buffers.
	 * Test client will allocate (ntci_msg_concurrency * 2) buffers (one for
	 * sending and another for receiving) for each test server
	 * from endpoints list ntci_ep.
	 */
	uint64_t		 ntci_msg_concurrency;
	/**
	 * Buffer send timeout for M0_NET_QT_MSG_SEND queue.
	 * Buffers in M0_NET_QT_MSG_RECV queue have M0_TIME_NEVER
	 * timeout because there is no way to send message to this queue
	 * to specific buffer (if number of buffers > 0).
	 */
	m0_time_t		 ntci_buf_send_timeout;
	/** Bulk buffers timeout for all bulk queues */
	m0_time_t		 ntci_buf_bulk_timeout;
	/** Transfer machine endpoint for data transfers */
	char			*ntci_tm_ep;
	/** Endpoints list */
	struct m0_net_test_slist ntci_ep;
};

/**
   M0_NET_TEST_CMD_STATUS_DATA.
   @see m0_net_test_cmd, m0_net_test_msg_nr, m0_net_test_stats
 */
struct m0_net_test_cmd_status_data {
	/** number of sent messages (total/failed/bad) */
	struct m0_net_test_msg_nr ntcsd_msg_nr_send;
	/** number of received messages (total/failed/bad) */
	struct m0_net_test_msg_nr ntcsd_msg_nr_recv;
	/** number of sent bulk messages (total/failed/bad) */
	struct m0_net_test_msg_nr ntcsd_bulk_nr_send;
	/** number of received bulk messages (total/failed/bad) */
	struct m0_net_test_msg_nr ntcsd_bulk_nr_recv;
	/** number of transfers (in both directions) (total/failed/bad) */
	struct m0_net_test_msg_nr ntcsd_transfers;
	/** Test start time */
	m0_time_t		  ntcsd_time_start;
	/** Test finish time */
	m0_time_t		  ntcsd_time_finish;
	/** Current time on the test node */
	m0_time_t		  ntcsd_time_now;
	/** Test was finished */
	bool			  ntcsd_finished;
	/** 'send' messages per second statistics with 1s interval */
	struct m0_net_test_mps	  ntcsd_mps_send;
	/** 'receive' messages per second statistics with 1s interval */
	struct m0_net_test_mps	  ntcsd_mps_recv;
	/**
	 * RTT statistics (without lost messages) (test client only)
	 * @note Only RTT can be measured without very precise
	 * time synchronization between test client and test server.
	 */
	struct m0_net_test_stats  ntcsd_rtt;
};

/**
   Command structure to exchange between console and clients or servers.
   @b WARNING: be sure to change cmd_serialize() after changes to this
   structure.
   @note m0_net_test_cmd.ntc_ep_index and m0_net_test_cmd.ntc_buf_index
   will not be sent/received over the network.
 */
struct m0_net_test_cmd {
	/** command type */
	enum m0_net_test_cmd_type ntc_type;
	/** command structures */
	union {
		struct m0_net_test_cmd_done	   ntc_done;
		struct m0_net_test_cmd_init	   ntc_init;
		struct m0_net_test_cmd_status_data ntc_status_data;
	};
	/**
	   Endpoint index in commands context.
	   Set in m0_net_test_commands_recv().
	   Used in m0_net_test_commands_send().
	   Will be set to -1 in m0_net_test_commands_recv()
	   if endpoint isn't present in commands context endpoints list.
	 */
	ssize_t ntc_ep_index;
	/** buffer index. set in m0_net_test_commands_recv() */
	size_t  ntc_buf_index;
};

struct m0_net_test_cmd_buf_status {
	/**
	   m0_net_end_point_get() in message receive callback.
	   m0_net_end_point_put() in m0_net_test_commands_recv().
	 */
	struct m0_net_end_point *ntcbs_ep;
	/** buffer status, m0_net_buffer_event.nbe_status in buffer callback */
	int			 ntcbs_buf_status;
	/** buffer was added to the receive queue */
	bool			 ntcbs_in_recv_queue;
};

struct m0_net_test_cmd_ctx;

/** 'Command sent' callback. */
typedef void (*m0_net_test_commands_send_cb_t)(struct m0_net_test_cmd_ctx *ctx,
					       size_t ep_index,
					       int buf_status);

/**
   Commands context.
 */
struct m0_net_test_cmd_ctx {
	/**
	   Network context for this command context.
	   First ntcc_ep_nr ping buffers are used for commands sending,
	   other ntcc_ep_nr * 2 ping buffers are used for commands receiving.
	 */
	struct m0_net_test_network_ctx	   ntcc_net;
	/**
	   Ring buffer for receive queue.
	   m0_net_test_ringbuf_put() in message receive callback.
	   m0_net_test_ringbuf_get() in m0_net_test_commands_recv().
	 */
	struct m0_net_test_ringbuf	   ntcc_rb;
	/** number of commands in context */
	size_t				   ntcc_ep_nr;
	/**
	   m0_semaphore_up() in message send callback.
	   m0_semaphore_down() in m0_net_test_commands_send_wait_all().

	   @note Problem with semaphore max value can be here.
	   @see @ref net-test-sem-max-value "Problem Description"
	 */
	struct m0_semaphore		   ntcc_sem_send;
	/**
	   m0_semaphore_up() in message recv callback.
	   m0_semaphore_timeddown() in m0_net_test_commands_recv().

	   @note Problem with semaphore max value can be here.
	   @see @ref net-test-sem-max-value "Problem Description"
	 */
	struct m0_semaphore		   ntcc_sem_recv;
	/** Called from message send callback */
	m0_net_test_commands_send_cb_t	   ntcc_send_cb;
	/**
	   Number of sent commands.
	   Resets to 0 on every call to m0_net_test_commands_wait_all().
	 */
	size_t				   ntcc_send_nr;
	/** Mutex for ntcc_send_nr protection */
	struct m0_mutex			   ntcc_send_mutex;
	/**
	   Updated in message send/recv callbacks.
	 */
	struct m0_net_test_cmd_buf_status *ntcc_buf_status;
};

/**
   Initialize commands context.
   @param ctx commands context.
   @param cmd_ep endpoint for commands context.
   @param send_timeout timeout for message sending.
   @param send_cb 'Command sent' callback. Can be NULL.
   @param ep_list endpoints list. Commands will be sent to/will be
		  expected from endpoints from this list.
   @return 0 (success)
   @return -EEXIST ep_list contains two equal strings
   @return -errno (failure)
   @note
   - buffers for message sending/receiving will be allocated here,
     three buffers per endpoint: one for sending, two for receiving;
   - all buffers will have ::M0_NET_TEST_CMD_SIZE_MAX size;
   - all buffers for receiving commands will be added to receive queue here;
   - buffers will not be automatically added to receive queue after
     call to m0_net_test_commands_recv();
   - m0_net_test_commands_recv() can allocate resources while decoding
     command from buffer, so m0_net_test_received_free() must be called
     for command after successful m0_net_test_commads_recv().
 */
int m0_net_test_commands_init(struct m0_net_test_cmd_ctx *ctx,
			      const char *cmd_ep,
			      m0_time_t send_timeout,
			      m0_net_test_commands_send_cb_t send_cb,
			      struct m0_net_test_slist *ep_list);
void m0_net_test_commands_fini(struct m0_net_test_cmd_ctx *ctx);

/**
   Invariant for m0_net_test_cmd_ctx.
   Time complexity is O(1).
 */
bool m0_net_test_commands_invariant(struct m0_net_test_cmd_ctx *ctx);

/**
   Send command.
   @param ctx Commands context.
   @param cmd Command to send. cmd->ntc_ep_index should be set to valid
	      endpoint index in the commands context.
 */
int m0_net_test_commands_send(struct m0_net_test_cmd_ctx *ctx,
			      struct m0_net_test_cmd *cmd);
/**
   Wait until all 'command send' callbacks executed for every sent command.
 */
void m0_net_test_commands_send_wait_all(struct m0_net_test_cmd_ctx *ctx);

/**
   Receive command.
   @param ctx Commands context.
   @param cmd Received buffer will be deserialized to this structure.
	      m0_net_test_received_free() should be called for cmd to free
	      resources that can be allocated while decoding.
   @param deadline Functon will wait until deadline reached. Absolute time.
   @return -ETIMEDOUT command wasn't received before deadline

   @note cmd->ntc_buf_index will be set to buffer index with received command.
   This buffer will be removed from receive queue and should be added using
   m0_net_test_commands_recv_enqueue(). Buffer will not be removed from receive
   queue iff function returned -ETIMEDOUT.
   @see m0_net_test_commands_init().
 */
int m0_net_test_commands_recv(struct m0_net_test_cmd_ctx *ctx,
			      struct m0_net_test_cmd *cmd,
			      m0_time_t deadline);
/**
   Add commands context buffer to commands receive queue.
   @see m0_net_test_commands_recv().
 */
int m0_net_test_commands_recv_enqueue(struct m0_net_test_cmd_ctx *ctx,
				      size_t buf_index);
/**
   Free received command resources.
   @see m0_net_test_commands_recv().
 */
void m0_net_test_commands_received_free(struct m0_net_test_cmd *cmd);

/**
   @} end of NetTestCommandsDFS group
 */

#endif /*  __MERO_NET_TEST_COMMANDS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
