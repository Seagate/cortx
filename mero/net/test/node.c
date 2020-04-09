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
 * Original creation date: 03/22/2012
 */

#include "lib/errno.h"		/* ETIMEDOUT */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/memory.h"		/* M0_ALLOC_PTR */
#include "lib/trace.h"		/* M0_LOG */

#include "net/test/node.h"	/* m0_net_test_node_ctx */
#include "net/test/node_stub.h"	/* m0_net_test_node_stub_ops */
#include "net/test/node_ping.h"	/* m0_net_test_node_ping_ops */
#include "net/test/node_bulk.h"	/* m0_net_test_node_bulk_ops */

#define NET_TEST_MODULE_NAME node
#include "net/test/debug.h"

/**
   @page net-test Mero Network Benchmark

   - @ref net-test-ovw
   - @ref net-test-def
   - @ref net-test-req
   - @ref net-test-depends
   - @ref net-test-highlights
   - @subpage net-test-fspec "Functional Specification"
   - @ref net-test-lspec
     - @ref net-test-lspec-comps
     - @ref net-test-lspec-state
     - @ref net-test-lspec-thread
     - @ref net-test-lspec-numa
   - @ref net-test-conformance
   - @ref net-test-ut
   - @ref net-test-st
   - @ref net-test-O
   - @ref net-test-lim
   - @ref net-test-issues
   - @ref net-test-ref


   <hr>
   @section net-test-ovw Overview

   Mero network benchmark is designed to test network subsystem of Mero
   and network connections between nodes that are running Mero.

   Mero Network Benchmark is implemented as a kernel module for test node
   and user space program for test console.
   Before testing kernel module must be copied to every test node.
   Then test console will perform test in this way:

   @msc
   console, node;
   console->node	[label = "Load kernel module"];
   node->console	[label = "Node is ready"];
   ---			[label = "waiting for all nodes"];
   console->node	[label = "Command to start test"];
   node rbox node	[label = "Executing test"];
   node->console	[label = "Statistics"];
   ---			[label = "waiting for all nodes"];
   console rbox console [label = "Print summary statistics"];
   console->node	[label = "Unload kernel module"];
   ---			[label = "waiting for all nodes"];
   @endmsc

   <hr>
   @section net-test-def Definitions

   Previously defined terms:
   @see @ref net-test-hld "Mero Network Benchmark HLD"

   New terms:
   - <b>Configuration variable</b> Variable with name. It can have some value.
   - @b Configuration Set of name-value pairs.

   <hr>
   @section net-test-req Requirements

   - @b R.m0.net.self-test.statistics statistics from the all nodes
     can be collected on the test console.
   - @b R.m0.net.self-test.statistics.live statistics from the all nodes
     can be collected on the test console at any time during the test.
   - @b R.m0.net.self-test.statistics.live pdsh is used to perform
     statistics collecting from the all nodes with some interval.
   - @b R.m0.net.self-test.test.ping latency is automatically measured for
     all messages.
   - @b R.m0.net.self-test.test.bulk used messages with additional data.
   - @b R.m0.net.self-test.test.bulk.integrity.no-check bulk messages
      additional data isn't checked.
   - @b R.m0.net.self-test.test.duration.simple end user should be able to
     specify how long a test should run, by loop.
   - @b R.m0.net.self-test.kernel test client/server is implemented as
     a kernel module.

   <hr>
   @section net-test-depends Dependencies

   - R.m0.net

   <hr>
   @section net-test-highlights Design Highlights

   - m0_net is used as network library.
   - To make latency measurement error as little as possible all
     heavy operations (such as buffer allocation) will be done before
     test message exchanging between test client and test server.

   <hr>
   @section net-test-lspec Logical Specification

   - @ref net-test-lspec-comps
     - @ref net-test-lspec-ping
     - @ref net-test-lspec-bulk
     - @ref net-test-lspec-algo-client-ping
     - @ref net-test-lspec-algo-client-bulk
     - @ref net-test-lspec-algo-server-ping
     - @ref net-test-lspec-algo-server-bulk
     - @ref net-test-lspec-console
     - @ref net-test-lspec-misc
   - @ref net-test-lspec-state
   - @ref net-test-lspec-thread
   - @ref net-test-lspec-numa

   @subsection net-test-lspec-comps Component Overview

   @dot
   digraph {
     node [style=box];
     label = "Network Benchmark Source File Relationship";
     network	[label="network.c"];
     service	[label="service.c"];
     stats	[label="stats.c"];
     node_ping	[label="node_ping.c"];
     node_bulk	[label="node_bulk.c"];
     node_help	[label="node_helper.c"];
     node_	[label="node.c"];
     node_u	[label="user_space/node_u.c"];
     node_k	[label="linux_kernel/node_k.c"];
     commands	[label="commands.c"];
     console	[label="console.c"];
     stats_u	[label="user_space/stats_u.c"];
     console_u	[label="user_space/console_u.c"];

     node_ping	-> network;
     node_ping	-> service;
     node_ping	-> stats;
     node_ping	-> node_help;
     node_bulk	-> network;
     node_bulk	-> service;
     node_bulk	-> stats;
     node_bulk	-> node_help;
     node_	-> node_ping;
     node_	-> node_bulk;
     node_	-> commands;
     node_	-> service;
     node_u	-> node_;
     node_k	-> node_;
     commands	-> network;
     console	-> commands;
     console_u	-> console;
     stats_u	-> stats;
     console_u	-> stats_u;
   }
   @enddot

   Test node can be run in such a way:

   @code
   int rc;
   struct m0_net_test_node_ctx node;
   struct m0_net_test_node_cfg cfg;
   int rc;
   // prepare config, m0_init() etc.
   rc = m0_net_test_node_init(&node, &cfg);
   if (rc == 0) {
	rc = m0_net_test_node_start(&node);
	if (rc == 0) {
		m0_semaphore_down(&node.ntnc_thread_finished_sem);
		m0_net_test_node_stop(&node);
	}
	m0_net_test_node_fini(&node);
   }
   @endcode

   @subsubsection net-test-lspec-ping Ping Test

   One test message travel:
   @msc
   c [label = "Test Client"],
   s [label = "Test Server"];

   |||;
   c rbox c [label = "Create test message for ping test with timestamp
		      and sequence number"];
   c=>s     [label = "Test message"];
   ...;
   s=>c     [label = "Test message"];
   c rbox c [label = "Check test message timestamp and sequence number,
		      add to statistics"];
   |||;
   @endmsc

   @subsubsection net-test-lspec-bulk Bulk Test

   RTT is measured as length of time interval
   [time of transition to transfer start state,
    time of transition to transfer finish state]. See
   @ref net-test-lspec-algo-client-bulk, @ref net-test-lspec-algo-server-bulk.

   One test message travel:
   @msc
   c [label = "Test Client"],
   s [label = "Test Server"];

   |||;
   c rbox c [label = "Allocate buffers for passive send/receive"];
   c rbox c [label = "Send buffer descriptors to the test server"];
   c=>s     [label = "Network buffer descriptors"];
   s rbox s [label = "Receive buffer descriptors from the test client"];
   c rbox c [label = "Start passive bulk sending"],
   s rbox s [label = "Start active bulk receiving"];
   c=>s	    [label = "Bulk data"];
   ...;
   |||;
   c rbox c [label = "Finish passive bulk sending"],
   s rbox s [label = "Finish active bulk receiving"];
   s rbox s [label = "Initialize bulk transfer as an active bulk sender"];
   c rbox c [label = "Start passive bulk receiving"],
   s rbox s [label = "Start active bulk sending"];
   s=>c     [label = "Bulk data"];
   ...;
   |||;
   c rbox c [label = "Finish passive bulk receiving"],
   s rbox s [label = "Finish active bulk sending"];
   |||;
   @endmsc

   @subsubsection net-test-lspec-algo-client-ping Ping Test Client Algorithm

   @todo Outdated and not used now.

   @dot
   digraph {
     S0 [label="entry point"];
     S1 [label="msg_left = msg_count;\l\
semaphore buf_free = concurrency;\l", shape=box];
     SD [label="", height=0, width=0, shape=plaintext];
     S2 [label="msg_left > 0?", shape=diamond];
     S3 [label="buf_free.down();", shape=box];
     S4 [label="stop.trydown()?", shape=diamond];
     S5 [label="msg_left--;", shape=box];
     S6 [label="test_type == ping?", shape=diamond];
     SA [label="add recv buf to recv queue\l\
add send buf to send queue", shape=box];
     SB [label="add bulk buf to PASSIVE_SEND queue\l\
add bulk buf to PASSIVE_RECV queue\l\
add 2 buf descriptors to MSG_SEND queue\l", shape=box];
     SC [label="", height=0, width=0, shape=plaintext];
     S7 [label="stop.down();\lbuf_free.up();\l", style=box];
     S8 [label="for (i = 0; i < concurrency; ++i)\l  buf_free.down();\l\
finished.up();\l", shape=box];
     S9 [label="exit point"];
     S0   -> S1;
     S1:s -> SD;
     SD   -> S2:n;
     S2:e -> S3   [label="yes"];
     S2:w -> S8   [label="no"];
     S3:s -> S4:n;
     S4:e -> S5   [label="no"];
     S4:w -> S7   [label="yes"];
     S5   -> S6;
     S6:e -> SA:n [label="yes"];
     S6:w -> SB:n [label="no"];
     SA:s -> SC;
     SB:s -> SC;
     SC   -> SD;
     S7   -> S8;
     S8   -> S9;
   }
   @enddot

   Callbacks for ping test
   - M0_NET_QT_MSG_SEND
     - update stats
   - M0_NET_QT_MSG_RECV
     - update stats
     - buf_free.up()

   @subsubsection net-test-lspec-algo-sm-legend State Machine Legend
   - Red solid arrow - state transition to "error" states.
   - Green bold arrow - state transition for "successful" operation.
   - Black dashed arrow - auto state transition (shouldn't be explicit).
   - State name in double oval - final state.

   @subsubsection net-test-lspec-algo-client-bulk Bulk Test Client Algorithm

   @dot
   digraph {
	label = "Bulk Test Client Buffer States";
	unused	    [label="UNUSED"];
	queued	    [label="QUEUED"];
	bd_sent	    [label="BD_SENT"];
	cb_left2    [label="CB_LEFT2"];
	cb_left1    [label="CB_LEFT1"];
	failed2	    [label="FAILED2", color="red"];
	failed1	    [label="FAILED1", color="red"];
	transferred [label="TRANSFERRED", peripheries=2];
	failed	    [label="FAILED", peripheries=2];

	unused	    -> queued	   [color="green", style="bold"];
	queued	    -> bd_sent	   [color="green", style="bold"];
	queued	    -> failed	   [color="red", style="solid"];
	queued	    -> failed1	   [color="red", style="solid"];
	queued	    -> failed2	   [color="red", style="solid"];
	bd_sent	    -> cb_left2	   [color="green", style="bold"];
	bd_sent	    -> failed2	   [color="red", style="solid"];
	cb_left2    -> cb_left1	   [color="green", style="bold"];
	cb_left2    -> failed1	   [color="red", style="solid"];
	cb_left1    -> transferred [color="green", style="bold"];
	cb_left1    -> failed	   [color="red", style="solid"];
	failed2	    -> failed1	   [color="red", style="solid"];
	failed1	    -> failed	   [color="red", style="solid"];
	transferred -> unused	   [color="black", style="dashed"];
	failed	    -> unused	   [color="black", style="dashed"];
   }
   @enddot

   @see @ref net-test-lspec-algo-sm-legend

   Bulk buffer pair states
   - UNUSED - buffer pair is not used in buffer operations now.
     It is initial state of buffer pair.
   - QUEUED - buffer pair is queued or almost added to network bulk queue.
     See @ref net-test-lspec-bulk-buf-states "RECEIVING state".
   - BD_SENT - buffer descriptors for bulk buffer pair are sent or almost sent
     to the test server.  See @ref net-test-lspec-bulk-buf-states "RECEIVING state".
   - CB_LEFT2(CB_LFET1) - there are 2 (or 1) network buffer callback(s) left
     for this buffer pair (including network buffer callback for the message
     with buffer descriptor).
   - TRANSFERRED - all buffer for this buffer pair and message with buffer
     descriptors was successfully executed.
   - FAILED2(FAILED1, FAILED) - some operation failed. Also 2 (1, 0) callbacks
     left for this buffer pair (like CB_LEFT2, CB_LEFT1).

   Initial state: UNUSED.

   Final states: TRANSFERRED, FAILED.

   Transfer start state: QUEUED.

   Transfer finish state: TRANSFERRED.

   Bulk buffer pair state transitions
   - UNUSED -> QUEUED
     - client_process_unused_bulk()
       - add bulk buffer to passive bulk send queue, then add
	 another bulk buffer in pair to passive recv queue
   - QUEUED -> BD_SENT
     - client_process_queued_bulk()
       - send msg buffer with bulk buffer descriptors
   - QUEUED -> FAILED
     - client_process_unused_bulk()
       - addition to passive send queue failed
   - QUEUED -> FAILED1
     - client_process_unused_bulk()
       - addition to passive recv queue failed
	 - remove from passive send queue already queued buffer
   - QUEUED -> FAILED2
     - client_process_queued_bulk()
       - bulk buffer network descriptors to ping buffer encoding failed
	 - dequeue already queued bulk buffers
       - addition msg with bulk buffer descriptors to network queue failed
	 - dequeue already queued bulk buffers
   - BD_SENT -> CB_LEFT2
   - CB_LEFT2 -> CB_LEFT1
   - CB_LEFT1 -> TRANSFERRED
     - network buffer callback
       - ev->nbe_status == 0
   - BD_SENT -> FAILED2
   - CB_LEFT2 -> FAILED1
   - CB_LEFT1 -> FAILED
     - network buffer callback
       - ev->nbe_status != 0
   - FAILED2 -> FAILED1
   - FAILED1 -> FAILED
     - network buffer callback
   - TRANSFERRED -> UNUSED
     - node_bulk_state_transition_auto_all()
       - stats: increase total number of number messages
   - FAILED -> UNUSED
     - node_bulk_state_transition_auto_all()
       - stats: increase total number of test messages and
	 number of failed messages

   @subsubsection net-test-lspec-algo-server-ping Ping Test Server Algorithm

   @todo Outdated and not used now.

   Test server allocates all necessary buffers and initializes transfer
   machine. Then it just works in transfer machine callbacks.

   Ping test callbacks
   - M0_NET_QT_MSG_RECV
     - add buffer to msg send queue
     - update stats
   - M0_NET_QT_MSG_SEND
     - add buffer to msg recv queue

   @subsubsection net-test-lspec-algo-server-bulk Bulk Test Server Algorithm

   - Every bulk buffer have its own state.
   - Bulk test server maintains unused bulk buffers queue - the bulk buffer
     for messages transfer will be taken from this queue when buffer
     descriptor arrives. If there are no buffers in queue -
     then buffer descriptor will be discarded, and number of failed and total
     test messages will be increased.

   @dot
   digraph {
	label = "Bulk Test Server Buffer States";
	unused	    [label="UNUSED"];
	bd_received [label="BD_RECEIVED"];
	receiving   [label="RECEIVING"];
	sending	    [label="SENDING"];
	transferred [label="TRANSFERRED", peripheries=2];
	failed	    [label="FAILED", peripheries=2];
	badmsg	    [label="BADMSG", peripheries=2];

	unused	    -> bd_received [color="green", style="bold"];
	bd_received -> badmsg	   [color="red", style="solid"];
	bd_received -> receiving   [color="green", style="bold"];
	receiving   -> sending	   [color="green", style="bold"];
	receiving   -> failed	   [color="red", style="solid"];
	sending	    -> transferred [color="green", style="bold"];
	sending	    -> failed	   [color="red", style="solid"];
	transferred -> unused	   [color="black", style="dashed"];
	failed	    -> unused	   [color="black", style="dashed"];
	badmsg	    -> unused	   [color="black", style="dashed"];
   }
   @enddot

   @see @ref net-test-lspec-algo-sm-legend

   @anchor net-test-lspec-bulk-buf-states
   Bulk buffer states
   - UNUSED - bulk buffer isn't currently used in network operations and
     can be used when passive bulk buffer decriptors arrive.
   - BD_RECEIVED - message with buffer descriptors was received from the test
     client.
   - RECEIVING - bulk buffer added to the active bulk receive queue. Bulk
     buffer enters this state just before adding to the network queue because
     network buffer callback may be executed before returning from
     'add to network buffer queue' function.
   - SENDING - bulk buffer added to the active bulk send queue. Bulk buffer
     enters this state as well as for the RECEIVING state.
   - TRANSFERRED - bulk buffer was successfully received and sent.
   - FAILED - some operation failed.
   - BADMSG - message with buffer descriptors contains invalid data.

   Initial state: UNUSED.

   Final states: TRANSFERRED, FAILED, BADMSG.

   Transfer start state: RECEIVING.

   Transfer finish state: TRANSFERRED.

   Bulk buffer state transitions

   - UNUSED -> BD_RECEIVED
     - M0_NET_QT_MSG_RECV callback
       - message with buffer decriptors was received from the test client
   - BD_RECEIVED -> BADMSG
     - M0_NET_QT_MSG_RECV callback
       - message with buffer decscriptors contains invalid data
   - BD_RECEIVED -> RECEIVING
     - M0_NET_QT_MSG_RECV callback
       - bulk buffer was successfully added to active bulk receive queue.
   - RECEIVING -> SENDING
     - M0_NET_QT_ACTIVE_BULK_RECV callback
       - bulk buffer was successfully received from the test client.
   - RECEIVING -> FAILED
     - M0_NET_QT_MSG_RECV callback
       - addition to the active bulk receive queue failed.
     - M0_NET_QT_ACTIVE_BULK_RECV callback
       - bulk buffer receiving failed.
   - SENDING -> TRANSFERRED
     - M0_NET_QT_ACTIVE_BULK_SEND callback
       - bulk buffer was successfully sent to the test client.
   - SENDING -> FAILED
     - M0_NET_QT_ACTIVE_BULK_RECV callback
       - addition to the active bulk send queue failed.
     - M0_NET_QT_ACTIVE_BULK_SEND callback
       - active bulk sending failed.
   - TRANSFERRED -> UNUSED
     - node_bulk_state_transition_auto_all()
       - stats: increase total number of number messages
   - FAILED -> UNUSED
     - node_bulk_state_transition_auto_all()
       - stats: increase total number of test messages and
	 number of failed messages
   - BADMSG -> UNUSED
     - node_bulk_state_transition_auto_all()
       - stats: increase total number of test messages and
	 number of bad messages

   @subsubsection net-test-lspec-console Test Console
   @msc
   console [label="Test Console"],
   clients [label="Test Clients"],
   servers [label="Test Servers"];

   |||;
   clients rbox clients	[label = "Listening for console commands"],
   servers rbox servers	[label = "Listening for console commands"];
   console => servers	[label = "INIT command"];
   servers => console	[label = "INIT DONE response"];
   ---			[label = "waiting for all servers"];
   console => clients	[label = "INIT command"];
   clients => console	[label = "INIT DONE response"];
   ---			[label = "waiting for all clients"];
   console => servers	[label = "START command"];
   servers => console	[label = "START ACK response"];
   ---			[label = "waiting for all servers"];
   console => clients	[label = "START command"];
   clients => console	[label = "START ACK response"];
   ---			[label = "waiting for all clients"];
   ---			[label = "running..."];
   console => clients	[label = "STATUS command"];
   clients => console	[label = "STATUS DATA response"];
   console => servers	[label = "STATUS command"];
   servers => console	[label = "STATUS DATA response"];
   ---			[label = "console wants to stop clients&servers"];
   console => clients	[label = "STOP command"];
   clients => console	[label = "STOP DONE response"];
   clients rbox clients	[label = "clients cleanup"];
   ---			[label = "waiting for all clients"];
   console => servers	[label = "STOP command"];
   servers => console	[label = "STOP DONE response"];
   servers rbox servers	[label = "servers cleanup"];
   ---			[label = "waiting for all servers"];
   @endmsc

   @subsubsection net-test-lspec-misc Misc
   - Typed variables are used to store configuration.
   - Configuration variables are set in m0_net_test_config_init(). They
   should be never changed in other place.
   - m0_net_test_stats is used for keeping some data for sample,
   based on which min/max/average/standard deviation can be calculated.
   - m0_net_test_network_(msg/bulk)_(send/recv)_* is a wrapper around m0_net.
   This functions use m0_net_test_ctx as containter for buffers, callbacks,
   endpoints and transfer machine. Buffer/endpoint index (int in range
   [0, NR), where NR is number of corresponding elements) is used for selecting
   buffer/endpoint structure from m0_net_test_ctx.
   - All buffers are allocated in m0_net_test_network_ctx_init().
   - Endpoints can be added after m0_net_test_network_ctx_init() using
   m0_net_test_network_ep_add().

   @subsection net-test-lspec-state State Specification

   @dot
   digraph {
     node [style=box];
     label = "Test Service States";
     S0 [label="Uninitialized"];
     S1 [label="Ready"];
     S2 [label="Finished"];
     S3 [label="Failed"];
     S0 -> S1 [label="successful m0_net_test_service_init()"];
     S1 -> S0 [label="m0_net_test_service_fini()"];
     S1 -> S2 [label="service state change: service was finished"];
     S1 -> S3 [label="service state change: service was failed"];
     S2 -> S0 [label="m0_net_test_service_fini()"];
     S3 -> S0 [label="m0_net_test_service_fini()"];
   }
   @enddot

   @subsection net-test-lspec-thread Threading and Concurrency Model

   - Configuration is not protected by any synchronization mechanism.
     Configuration is not intended to change after initialization,
     so no need to use synchronization mechanism for reading configuration.
   - struct m0_net_test_stats is not protected by any synchronization mechanism.
   - struct m0_net_test_ctx is not protected by any synchronization mechanism.

   @subsection net-test-lspec-numa NUMA optimizations

   - Configuration is not intended to change after initial initialization,
     so cache coherence overhead will not exists.
   - One m0_net_test_stats per locality can be used. Summary statistics can
     be collected from all localities using m0_net_test_stats_add_stats()
     only when it needed.
   - One m0_net_test_ctx per locality can be used.

   <hr>
   @section net-test-conformance Conformance

   - @b I.m0.net.self-test.statistics user-space LNet implementation is used
     to collect statistics from all nodes.
   - @b I.m0.net.self-test.statistics.live user-space LNet implementation
     is used to perform statistics collecting from the all nodes with
     some interval.
   - @b I.m0.net.self-test.test.ping latency is automatically measured for
     all messages.
   - @b I.m0.net.self-test.test.bulk used messages with additional data.
   - @b I.m0.net.self-test.test.bulk.integrity.no-check bulk messages
      additional data isn't checked.
   - @b I.m0.net.self-test.test.duration.simple end user is able to
     specify how long a test should run, by loop - see
     @ref net-test-fspec-cli-console.
   - @b I.m0.net.self-test.kernel test client/server is implemented as
     a kernel module.

   <hr>
   @section net-test-ut Unit Tests

   @test Ping message send/recv over loopback device.
   @test Concurrent ping messages send/recv over loopback device.
   @test Bulk active send/passive receive over loopback device.
   @test Bulk passive send/active receive over loopback device.
   @test Statistics for sample with one value.
   @test Statistics for sample with ten values.
   @test Merge two m0_net_test_stats structures with
	 m0_net_test_stats_add_stats()

   <hr>
   @section net-test-st System Tests

   @test Script for network benchmark ping/bulk self-testing over loopback
	 device on single node.
   @test Script for tool ping/bulk testing with two test nodes.

   <hr>
   @section net-test-O Analysis

   - all m0_net_test_stats_* functions have O(1) complexity;
   - one mutex lock/unlock per statistics update in test client/server/console;
   - one semaphore up/down per test message in test client;

   @see @ref net-test-hld "Mero Network Benchmark HLD"

   <hr>
   @section net-test-lim Current Limitations

   - test buffer for commands between test console and test node have
     size 16KiB now (see ::M0_NET_TEST_CMD_SIZE_MAX);
   @anchor net-test-sem-max-value
   - m0_net_test_cmd_ctx.ntcc_sem_send, m0_net_test_cmd_ctx.ntcc_sem_recv
     and node_ping_ctx.npc_buf_q_sem can exceed
     SEMVMX (see 'man 3 sem_post', 'man 2 semop') if large number of
     network buffers used in the corresponding structures;

   <hr>
   @section net-test-issues Know Issues

   - Test console returns different number of transfers for
     test clients and test servers if time limit reached.
     It is because test node can't answer to STATUS command
     after STOP command. Possible solution: split STOP command to
     2 different commands - "stop sending test messages" and
     "finalize test messages transfer machine" (now STOP command
     handler performs these actions) - in this case STATUS command
     can be sent after "stop sending test messages" command.
   - Bulk test worker thread (node_bulk_worker()): it is possible
     for the test server to have all work done in single
     m0_net_buffer_event_deliver_all(), especially if number of
     concurrent buffers is high.  STATUS command will give
     inconsistent results for test clients and test servers in this case.
     Workaround: use stats from the test client in bulk test.

   <hr>
   @section net-test-ref References

   @anchor net-test-hld
   - <a href="https://docs.google.com/a/seagate.com/document/view?id=11Evkryj4CR
nHfH1kkQ0uVTU6yKfWryF0lOg5wUN0Xuw">Mero Network Benchmark HLD</a>
   - <a href="http://wiki.lustre.org/manual/LustreManual20_HTML/
LNETSelfTest.html">LNET Self-Test manual</a>
   - <a href="http://reviewboard.clusterstor.com/r/773">DLD review request</a>

 */

/**
   @defgroup NetTestInternals Internals
   @ingroup NetTestDFS

   @see @ref net-test
 */

/**
   @defgroup NetTestNodeInternals Test Node Internals
   @ingroup NetTestInternals

   @see @ref net-test

   @{
 */

enum {
	NODE_WAIT_CMD_GRANULARITY_MS = 20,
};

static struct m0_net_test_service_ops *
service_ops_get(struct m0_net_test_cmd *cmd)
{
	M0_PRE(cmd->ntc_type == M0_NET_TEST_CMD_INIT);

	switch (cmd->ntc_init.ntci_type) {
	case M0_NET_TEST_TYPE_STUB:
		return &m0_net_test_node_stub_ops;
	case M0_NET_TEST_TYPE_PING:
		return &m0_net_test_node_ping_ops;
	case M0_NET_TEST_TYPE_BULK:
		return &m0_net_test_node_bulk_ops;
	default:
		return NULL;
	}
}

static int node_cmd_get(struct m0_net_test_cmd_ctx *cmd_ctx,
			struct m0_net_test_cmd *cmd,
			m0_time_t deadline)
{
	int rc = m0_net_test_commands_recv(cmd_ctx, cmd, deadline);
	if (rc == 0)
		rc = m0_net_test_commands_recv_enqueue(cmd_ctx,
						       cmd->ntc_buf_index);
	if (rc == 0) {
		LOGD("node_cmd_get: rc = %d", rc);
		LOGD("node_cmd_get: cmd->ntc_type = %d", cmd->ntc_type);
		LOGD("node_cmd_get: cmd->ntc_init.ntci_msg_nr = %lu",
		     (unsigned long) cmd->ntc_init.ntci_msg_nr);
	}
	return rc;
}

static int node_cmd_wait(struct m0_net_test_node_ctx *ctx,
			 struct m0_net_test_cmd *cmd,
			 enum m0_net_test_cmd_type type)
{
	m0_time_t deadline;
	const int TIME_ONE_MS = M0_TIME_ONE_SECOND / 1000;
	int	  rc;

	M0_PRE(ctx != NULL);
	do {
		deadline = m0_time_from_now(0, NODE_WAIT_CMD_GRANULARITY_MS *
					       TIME_ONE_MS);
		rc = node_cmd_get(&ctx->ntnc_cmd, cmd, deadline);
		if (rc != 0 && rc != -ETIMEDOUT)
			return rc;	/** @todo add retry count */
		if (rc == 0 && cmd->ntc_type != type)
			m0_net_test_commands_received_free(cmd);
	} while (!(rc == 0 && cmd->ntc_type == type) && !ctx->ntnc_exit_flag);
	return 0;
}

static void node_thread(struct m0_net_test_node_ctx *ctx)
{
	struct m0_net_test_service	svc;
	struct m0_net_test_service_ops *svc_ops;
	enum m0_net_test_service_state	svc_state;
	struct m0_net_test_cmd	       *cmd;
	struct m0_net_test_cmd	       *reply;
	int				rc;
	bool				skip_cmd_get;

	M0_PRE(ctx != NULL);

	M0_ALLOC_PTR(cmd);
	M0_ALLOC_PTR(reply);
	if (cmd == NULL || reply == NULL) {
		rc = -ENOMEM;
		goto done;
	}

	/* wait for INIT command */
	ctx->ntnc_errno = node_cmd_wait(ctx, cmd, M0_NET_TEST_CMD_INIT);
	if (ctx->ntnc_exit_flag) {
		rc = 0;
		m0_net_test_commands_received_free(cmd);
		goto done;
	}
	if (ctx->ntnc_errno != 0) {
		rc = ctx->ntnc_errno;
		goto done;
	}
	/* we have configuration; initialize test service */
	svc_ops = service_ops_get(cmd);
	if (svc_ops == NULL) {
		rc = -ENODATA;
		m0_net_test_commands_received_free(cmd);
		goto done;
	}
	rc = m0_net_test_service_init(&svc, svc_ops);
	if (rc != 0) {
		m0_net_test_commands_received_free(cmd);
		goto done;
	}
	/* handle INIT command inside main loop */
	skip_cmd_get = true;
	/* test service is initialized. start main loop */
	do {
		/* get command */
		if (rc == 0 && !skip_cmd_get)
			rc = node_cmd_get(&ctx->ntnc_cmd, cmd,
					  m0_time_from_now(0, 25000000));
					  // m0_time_now());
		else
			skip_cmd_get = false;
		if (rc == 0)
			LOGD("node_thread: cmd_get, "
			     "rc = %d, cmd.ntc_ep_index = %lu",
			     rc, cmd->ntc_ep_index);
		if (rc == 0 && cmd->ntc_ep_index >= 0) {
			LOGD("node_thread: have command");
			/* we have command. handle it */
			rc = m0_net_test_service_cmd_handle(&svc, cmd, reply);
			LOGD("node_thread: cmd handle: rc = %d", rc);
			reply->ntc_ep_index = cmd->ntc_ep_index;
			m0_net_test_commands_received_free(cmd);
			/* send reply */
			LOGD("node_thread: reply.ntc_ep_index = %lu",
			     reply->ntc_ep_index);
			m0_net_test_commands_send_wait_all(&ctx->ntnc_cmd);
			rc = m0_net_test_commands_send(&ctx->ntnc_cmd, reply);
			LOGD("node_thread: send reply: rc = %d", rc);
			M0_SET0(cmd);
		} else if (rc == -ETIMEDOUT) {
			/* we haven't command. take a step. */
			rc = m0_net_test_service_step(&svc);
		} else {
			break;
		}
		svc_state = m0_net_test_service_state_get(&svc);
	} while (svc_state != M0_NET_TEST_SERVICE_FAILED &&
		 svc_state != M0_NET_TEST_SERVICE_FINISHED &&
		 !ctx->ntnc_exit_flag &&
		 rc == 0);

	/* finalize test service */
	m0_net_test_service_fini(&svc);

done:
	m0_free(cmd);
	m0_free(reply);
	LOGD("rc = %d", rc);
	ctx->ntnc_errno = rc;
	m0_semaphore_up(&ctx->ntnc_thread_finished_sem);
}

static int node_init_fini(struct m0_net_test_node_ctx *ctx,
			  struct m0_net_test_node_cfg *cfg)
{
	struct m0_net_test_slist ep_list;
	int			 rc;

	M0_PRE(ctx != NULL);
	if (cfg == NULL)
		goto fini;

	M0_SET0(ctx);

	rc = m0_net_test_slist_init(&ep_list, cfg->ntnc_addr_console, '`');
	if (rc != 0)
		goto failed;
	rc = m0_net_test_commands_init(&ctx->ntnc_cmd,
				       cfg->ntnc_addr,
				       cfg->ntnc_send_timeout,
				       NULL,
				       &ep_list);
	m0_net_test_slist_fini(&ep_list);
	if (rc != 0)
		goto failed;
	rc = m0_semaphore_init(&ctx->ntnc_thread_finished_sem, 0);
	if (rc != 0)
		goto commands_fini;

	return 0;
fini:
	rc = 0;
	m0_semaphore_fini(&ctx->ntnc_thread_finished_sem);
commands_fini:
	m0_net_test_commands_fini(&ctx->ntnc_cmd);
failed:
	return rc;
}

int m0_net_test_node_init(struct m0_net_test_node_ctx *ctx,
			  struct m0_net_test_node_cfg *cfg)
{
	return node_init_fini(ctx, cfg);
}

void m0_net_test_node_fini(struct m0_net_test_node_ctx *ctx)
{
	int rc = node_init_fini(ctx, NULL);
	M0_ASSERT(rc == 0);
}

int m0_net_test_node_start(struct m0_net_test_node_ctx *ctx)
{
	M0_PRE(ctx != NULL);

	ctx->ntnc_exit_flag = false;
	ctx->ntnc_errno     = 0;

	return M0_THREAD_INIT(&ctx->ntnc_thread, struct m0_net_test_node_ctx *,
			      NULL, &node_thread, ctx, "net_test_node");
}

void m0_net_test_node_stop(struct m0_net_test_node_ctx *ctx)
{
	int rc;

	M0_PRE(ctx != NULL);

	ctx->ntnc_exit_flag = true;
	m0_net_test_commands_send_wait_all(&ctx->ntnc_cmd);
	rc = m0_thread_join(&ctx->ntnc_thread);
	/*
	 * In either case when rc != 0 there is an unmatched
	 * m0_net_test_node_start() and m0_net_test_node_stop()
	 * or deadlock. If non-zero rc is returned as result of this function,
	 * then m0_net_test_node_stop() leaves m0_net_test_node_ctx in
	 * inconsistent state (also possible resource leak).
	 */
	M0_ASSERT(rc == 0);
	m0_thread_fini(&ctx->ntnc_thread);
}

static struct m0_net_test_node_ctx *m0_net_test_node_module_ctx = NULL;

int m0_net_test_node_module_initfini(struct m0_net_test_node_cfg *cfg)
{
	int rc = 0;

	if (cfg == NULL)
		goto fini;
	if (cfg->ntnc_addr == NULL ||
	    cfg->ntnc_addr_console == NULL ||
	    cfg->ntnc_send_timeout == 0) {
		rc = -EINVAL;
		goto fail;
	}

	M0_ALLOC_PTR(m0_net_test_node_module_ctx);
	if (m0_net_test_node_module_ctx == NULL) {
		rc = -ENOMEM;
		goto fail;
	}
	rc = m0_net_test_node_init(m0_net_test_node_module_ctx, cfg);
	if (rc != 0)
		goto free_ctx;
	rc = m0_net_test_node_start(m0_net_test_node_module_ctx);
	if (rc != 0)
		goto fini_node;

	goto success;
fini:
	m0_net_test_node_stop(m0_net_test_node_module_ctx);
fini_node:
	m0_net_test_node_fini(m0_net_test_node_module_ctx);
free_ctx:
	m0_free(m0_net_test_node_module_ctx);
fail:
success:
	return rc;
}
M0_EXPORTED(m0_net_test_node_module_initfini);

#undef NET_TEST_MODULE_NAME

/**
   @} end of NetTestNodeInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
