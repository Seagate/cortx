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
 * Original creation date: 01/12/2013
 */

#include "lib/misc.h"			/* M0_SET0 */
#include "lib/time.h"			/* m0_time_t */

#include "net/test/node_stub.h"


/**
   @defgroup NetTestStubNodeInternals Node Stub
   @ingroup NetTestInternals

   Used in UT to test node-console interaction.

   @{
 */

static void *node_stub_init(struct m0_net_test_service *svc)
{
	return svc;
}

static void node_stub_fini(void *ctx_)
{
}

static int node_stub_step(void *ctx_)
{
	return 0;
}

static int node_stub_cmd_init(void *ctx_,
			      const struct m0_net_test_cmd *cmd,
			      struct m0_net_test_cmd *reply)
{
	reply->ntc_type = M0_NET_TEST_CMD_INIT_DONE;
	reply->ntc_done.ntcd_errno = 0;
	return 0;
}

static int node_stub_cmd_start(void *ctx_,
			       const struct m0_net_test_cmd *cmd,
			       struct m0_net_test_cmd *reply)
{
	reply->ntc_type = M0_NET_TEST_CMD_START_DONE;
	reply->ntc_done.ntcd_errno = 0;
	return 0;
}

static int node_stub_cmd_stop(void *ctx_,
			      const struct m0_net_test_cmd *cmd,
			      struct m0_net_test_cmd *reply)
{
	m0_net_test_service_state_change(ctx_, M0_NET_TEST_SERVICE_FINISHED);
	reply->ntc_type = M0_NET_TEST_CMD_STOP_DONE;
	reply->ntc_done.ntcd_errno = 0;
	return 0;
}

static int node_stub_cmd_status(void *ctx_,
				const struct m0_net_test_cmd *cmd,
				struct m0_net_test_cmd *reply)
{
	struct m0_net_test_cmd_status_data *sd;

	reply->ntc_type = M0_NET_TEST_CMD_STATUS_DATA;
	sd = &reply->ntc_status_data;
	M0_SET0(sd);
	sd->ntcsd_finished = true;
	sd->ntcsd_time_start = m0_time_now();
	sd->ntcsd_time_finish = m0_time_add(sd->ntcsd_time_start,
					    M0_MKTIME(0, 1));
	return 0;
}

static struct m0_net_test_service_cmd_handler node_stub_cmd_handler[] = {
	{
		.ntsch_type    = M0_NET_TEST_CMD_INIT,
		.ntsch_handler = node_stub_cmd_init,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_START,
		.ntsch_handler = node_stub_cmd_start,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_STOP,
		.ntsch_handler = node_stub_cmd_stop,
	},
	{
		.ntsch_type    = M0_NET_TEST_CMD_STATUS,
		.ntsch_handler = node_stub_cmd_status,
	},
};

struct m0_net_test_service_ops m0_net_test_node_stub_ops = {
	.ntso_init	     = node_stub_init,
	.ntso_fini	     = node_stub_fini,
	.ntso_step	     = node_stub_step,
	.ntso_cmd_handler    = node_stub_cmd_handler,
	.ntso_cmd_handler_nr = ARRAY_SIZE(node_stub_cmd_handler),
};

/**
   @} end of NetTestStubNodeInternals group
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
