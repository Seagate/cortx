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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 *                  Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 10/31/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include <sys/stat.h>
#include <stdlib.h>

#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/getopts.h"
#include "lib/misc.h"         /* M0_IN */
#include "lib/memory.h"
#include "lib/time.h"
#include "lib/atomic.h"

#include "fop/fop.h"
#include "mero/init.h"
#include "module/instance.h"  /* m0 */

#include "rpc/rpclib.h"
#include "rpc/rpc_opcodes.h"

#include "sns/cm/cm.h"
#include "sns/cm/trigger_fop.h"
#include "dix/cm/cm.h"
#include "dix/cm/trigger_fop.h"
#include "cm/repreb/trigger_fop.h"
#include "cm/repreb/trigger_fop_xc.h"

#include "repair_cli.h"

struct m0_mutex                 repair_wait_mutex;
struct m0_chan                  repair_wait;
int32_t                         srv_cnt = 0;
struct m0_atomic64              srv_rep_cnt;
enum {
	MAX_FAILURES_NR = 10,
};
extern struct m0_rpc_client_ctx cl_ctx;

extern const char *cl_ep_addr;
extern const char *srv_ep_addr[MAX_SERVERS];

extern struct m0_fop_type m0_sns_repair_trigger_fopt;
extern struct m0_fop_type m0_sns_repair_quiesce_fopt;
extern struct m0_fop_type m0_sns_repair_status_fopt;
extern struct m0_fop_type m0_sns_rebalance_trigger_fopt;
extern struct m0_fop_type m0_sns_rebalance_quiesce_fopt;
extern struct m0_fop_type m0_sns_rebalance_status_fopt;

extern struct m0_fop_type m0_dix_repair_trigger_fopt;
extern struct m0_fop_type m0_dix_repair_quiesce_fopt;
extern struct m0_fop_type m0_dix_repair_status_fopt;
extern struct m0_fop_type m0_dix_rebalance_trigger_fopt;
extern struct m0_fop_type m0_dix_rebalance_quiesce_fopt;
extern struct m0_fop_type m0_dix_rebalance_status_fopt;

static void usage(void)
{
	fprintf(stdout,
"-O Operation: CM_OP_REPAIR = 2 or CM_OP_REBALANCE = 4\n"
"              CM_OP_REPAIR_QUIESCE = 8 or CM_OP_REBALANCE_QUIESCE = 16\n"
"              CM_OP_REPAIR_STATUS = 32 or CM_OP_REBALANCE_STATUS = 64\n"
"-C Client_end_point\n"
"-S Server_end_point [-S Server_end_point ]: max number is %d\n"
"-t Service type: \n"
"              SNS = 0 or\n"
"              DIX = 1\n",  MAX_SERVERS);
}

static void repair_reply_received(struct m0_rpc_item *item)
{
	uint32_t                  req_op;
	struct trigger_fop       *treq;
	struct m0_fop            *req_fop;
	struct m0_fop            *rep_fop;
	struct trigger_rep_fop   *trep;
	struct m0_status_rep_fop *srep;

	M0_ASSERT(m0_rpc_item_error(item) == 0);

	req_fop = m0_rpc_item_to_fop(item);
	treq = m0_fop_data(req_fop);
	req_op = treq->op;

	rep_fop = m0_rpc_item_to_fop(item->ri_reply);
	trep = m0_fop_data(rep_fop);
	printf("reply got from: %s: op=%d rc=%d",
		m0_rpc_item_remote_ep_addr(&rep_fop->f_item), req_op, trep->rc);
	if (req_op == CM_OP_REPAIR_STATUS || req_op == CM_OP_REBALANCE_STATUS) {
		srep = m0_fop_data(rep_fop);
		printf(" status=%d progress=%lu\n", srep->ssr_state,
		       srep->ssr_progress);
	} else {
		printf("\n");
	}
	if (m0_atomic64_dec_and_test(&srv_rep_cnt))
		m0_chan_signal_lock(&repair_wait);
}

static const struct m0_rpc_item_ops repair_item_ops = {
	.rio_replied = repair_reply_received,
};

int main(int argc, char *argv[])
{
	static struct m0 instance;

	struct trigger_fop    *treq;
	struct rpc_ctx        *ctxs;
	struct m0_rpc_session *session;
	struct m0_clink        repair_clink;
	m0_time_t              start;
	m0_time_t              delta;
	int                    rc;
	uint32_t               type;
	uint32_t               op;
	int                    i;

	rc = m0_init(&instance);
	if (rc != 0) {
		fprintf(stderr, "Cannot init Mero: %d\n", rc);
		return M0_ERR(rc);
	}

	if (argc <= 1) {
		usage();
		return M0_ERR(-EINVAL);
	}

	rc = M0_GETOPTS("repair", argc, argv,
			M0_FORMATARG('O',
				     "-O Operation: \n"
				     "\t\t\tCM_OP_REPAIR = 1 or\n"
				     "\t\t\tCM_OP_REBALANCE = 2 or\n"
				     "\t\t\tCM_OP_REPAIR_QUIESCE = 3 or\n"
				     "\t\t\tCM_OP_REBALANCE_QUIESCE = 4 or\n"
				     "\t\t\tCM_OP_REPAIR_RESUME = 5 or\n"
				     "\t\t\tCM_OP_REBALANCE_RESUME = 6 or\n"
				     "\t\t\tCM_OP_REPAIR_STATUS = 7 or\n"
				     "\t\t\tCM_OP_REBALANCE_STATUS = 8 or\n"
				     "\t\t\tCM_OP_REPAIR_ABORT = 9 or\n"
				     "\t\t\tCM_OP_REBALANCE_ABORT   = 10\n",
				     "%u", &op),
			M0_STRINGARG('C', "Client endpoint",
				LAMBDA(void, (const char *str){
						cl_ep_addr = str;
					})),
			M0_STRINGARG('S', "Server endpoint",
				LAMBDA(void, (const char *str){
					srv_ep_addr[srv_cnt] = str;
					++srv_cnt;
					})),
			M0_FORMATARG('t',
				     "Service type: \n"
				     "\t\t\tSNS = 0 or\n"
				     "\t\t\tDIX = 1\n",
				     "%u", &type),
			M0_HELPARG('h'),
			);

	if (rc != 0)
		return M0_ERR(rc);

	if (op < CM_OP_REPAIR || op > CM_OP_REBALANCE_ABORT ||
	    !M0_IN(type, (0, 1))) {
		usage();
		return M0_ERR(-EINVAL);
	}

	m0_atomic64_set(&srv_rep_cnt, srv_cnt);
	m0_sns_cm_repair_trigger_fop_init();
	m0_sns_cm_rebalance_trigger_fop_init();
	m0_dix_cm_repair_trigger_fop_init();
	m0_dix_cm_rebalance_trigger_fop_init();
	rc = repair_client_init();
	if (rc != 0)
		return M0_ERR(rc);

	m0_mutex_init(&repair_wait_mutex);
	m0_chan_init(&repair_wait, &repair_wait_mutex);
	m0_clink_init(&repair_clink, NULL);
	M0_ALLOC_ARR(ctxs, srv_cnt);
	M0_ASSERT(ctxs != NULL);
	for (i = 0; i < srv_cnt; ++i) {
		rc = repair_rpc_ctx_init(&ctxs[i], srv_ep_addr[i]);
		ctxs[i].ctx_rc = rc;
		if (rc != 0)
			printf("failed to connect to %s: %d\n",
				srv_ep_addr[i], rc);
	}
	m0_mutex_lock(&repair_wait_mutex);
	m0_clink_add(&repair_wait, &repair_clink);
	m0_mutex_unlock(&repair_wait_mutex);
	start = m0_time_now();
	for (i = 0; i < srv_cnt; ++i) {
		struct m0_rpc_machine *mach;
		struct m0_fop         *fop = NULL;
		struct m0_rpc_item    *item;

		if (ctxs[i].ctx_rc != 0) {
			m0_atomic64_dec(&srv_rep_cnt);
			continue;
		}
		session = &ctxs[i].ctx_session;
		mach = m0_fop_session_machine(session);
		if (type == 0)
			rc = m0_sns_cm_trigger_fop_alloc(mach, op, &fop);
		else
			rc = m0_dix_cm_trigger_fop_alloc(mach, op, &fop);
		if (rc != 0)
			return M0_ERR(rc);

		treq = m0_fop_data(fop);
		treq->op = op;

		item = m0_fop_to_rpc_item(fop);
		item->ri_ops = &repair_item_ops;
		item->ri_session = session;
		item->ri_prio  = M0_RPC_ITEM_PRIO_MID;
		item->ri_deadline = 0;
		rc = m0_rpc_post(item);
		m0_fop_put_lock(fop);
		if (rc != 0)
			return M0_ERR(rc);
		printf("trigger fop sent to %s\n", srv_ep_addr[i]);
	}

	m0_chan_wait(&repair_clink);

	delta = m0_time_sub(m0_time_now(), start);
	printf("Time: %lu.%2.2lu sec\n", (unsigned long)m0_time_seconds(delta),
			(unsigned long)(m0_time_nanoseconds(delta) * 100 /
			M0_TIME_ONE_SECOND));
	for (i = 0; i < srv_cnt; ++i)
		repair_rpc_ctx_fini(&ctxs[i]);
	repair_client_fini();
	m0_sns_cm_repair_trigger_fop_fini();
	m0_sns_cm_rebalance_trigger_fop_fini();
	m0_dix_cm_repair_trigger_fop_fini();
	m0_dix_cm_rebalance_trigger_fop_fini();
	m0_fini();

	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
