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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 11-Feb-2013
 */


/**
 * @addtogroup rpc
 *
 * @{
 */

#include "ut/ut.h"
#include "lib/memory.h"
#include "lib/finject.h"
#include "lib/misc.h"              /* M0_BITS */
#include "lib/time.h"              /* m0_nanosleep */
#include "rpc/rpc.h"
#include "rpc/rpc_internal.h"
#include "rpc/ut/clnt_srv_ctx.c"
#include "rpc/ut/fops.h"           /* m0_rpc_arrow_fopt */
#include "ut/cs_fop.h"             /* cs_ds2_req_fop_fopt */
#include "ut/cs_fop_xc.h"          /* cs_ds2_req_fop */
#include "rpc/formation2.c"        /* frm_fill_packet_from_item_sources */

#include <stdio.h>

static struct m0_rpc_conn *conn;
static struct m0_rpc_item *item = NULL;
static int has_item_calls;
static int get_item_calls;
static bool conn_terminating_cb_called;

static int item_source_test_suite_init(void)
{
	m0_rpc_test_fops_init();
	start_rpc_client_and_server();
	conn = &cctx.rcx_connection;
	return 0;
}

static int item_source_test_suite_fini(void)
{
	/* rpc client-server will be stopped in conn_terminating_cb_test() */
	if (conn_terminating_cb_called == false)
		stop_rpc_client_and_server();
	m0_rpc_test_fops_fini();
	return 0;
}

static bool has_item(const struct m0_rpc_item_source *ris)
{
	M0_UT_ASSERT(m0_rpc_machine_is_locked(ris->ris_conn->c_rpc_machine));

	has_item_calls++;
	return M0_FI_ENABLED("yes");
}

static struct m0_rpc_item *get_item(struct m0_rpc_item_source *ris,
				    size_t max_payload_size)
{
	struct m0_fop         *fop;
	struct m0_rpc_machine *machine = ris->ris_conn->c_rpc_machine;

	M0_UT_ASSERT(m0_rpc_machine_is_locked(machine));
	get_item_calls++;
	fop  = m0_fop_alloc(&m0_rpc_arrow_fopt, NULL, machine);
	M0_UT_ASSERT(fop != NULL);
	item = &fop->f_item;
	/* without this "get", the item will be freed as soon as it is
	   sent/failed. The reference is required to protect item until
	   item_source_test() performs its checks on the item.
	 */
	m0_rpc_item_get(item);

	if (M0_FI_ENABLED("max"))
		item->ri_size = m0_rpc_session_get_max_item_payload_size(
					&cctx.rcx_session);

	if (M0_FI_ENABLED("not_multiple_of_8bytes"))
		item->ri_size = max_payload_size - 1;

	M0_UT_ASSERT(m0_rpc_item_is_oneway(item) &&
		     m0_rpc_item_size(item) <= max_payload_size);
	return item;
}

static void conn_terminating(struct m0_rpc_item_source *ris)
{
	M0_UT_ASSERT(!m0_rpc_item_source_is_registered(ris));

	conn_terminating_cb_called = true;
	m0_rpc_item_source_fini(ris);
	m0_free(ris);
}

static const struct m0_rpc_item_source_ops ris_ops = {
	.riso_has_item         = has_item,
	.riso_get_item         = get_item,
	.riso_conn_terminating = conn_terminating,
};

static void item_source_basic_test(void)
{
	struct m0_rpc_item_source ris;

	m0_rpc_item_source_init(&ris, "test-item-source", &ris_ops);
	M0_UT_ASSERT(ris.ris_ops == &ris_ops);
	m0_rpc_item_source_register(conn, &ris);
	m0_rpc_item_source_deregister(&ris);
	m0_rpc_item_source_fini(&ris);
}

static void item_source_limits_test(void)
{
	struct m0_rpc_item_source ris;
	struct m0_rpc_frm        *frm;
	struct m0_rpc_packet     *p;
	int                       cond;

	m0_rpc_item_source_init(&ris, "test-item-source", &ris_ops);
	M0_UT_ASSERT(ris.ris_ops == &ris_ops);
	m0_rpc_item_source_register(conn, &ris);
	frm = &conn->c_rpcchan->rc_frm;

	for (cond = 0; cond < 3; cond++) {
		M0_ALLOC_PTR(p);
		M0_UT_ASSERT(p != NULL);
		has_item_calls = get_item_calls = 0;
		m0_fi_enable_once("has_item", "yes");
		switch (cond) {
		case 0:
			/* For the minimum item size */
			break;
		case 1:
			m0_fi_enable_once("get_item", "max");
			break;
		case 2:
			m0_fi_enable_once("get_item", "not_multiple_of_8bytes");
			break;
		default:
			M0_IMPOSSIBLE("not supported");
		}

		m0_rpc_machine_lock(conn->c_rpc_machine);
		m0_rpc_packet_init(p, frm_rmachine(frm));
		frm_fill_packet_from_item_sources(frm, p);
		m0_rpc_machine_unlock(conn->c_rpc_machine);
		M0_UT_ASSERT(has_item_calls > get_item_calls &&
			     get_item_calls == 1);
		M0_UT_ASSERT(item->ri_sm.sm_state == M0_RPC_ITEM_SENDING);
		m0_rpc_machine_lock(conn->c_rpc_machine);
		m0_rpc_item_put(item);
		m0_rpc_item_change_state(item, M0_RPC_ITEM_SENT);
		m0_rpc_packet_discard(p);
		m0_rpc_item_put(item);
		m0_rpc_machine_unlock(conn->c_rpc_machine);
	}
	m0_rpc_item_source_deregister(&ris);
	m0_rpc_item_source_fini(&ris);
}

static void item_source_test(void)
{
	struct m0_rpc_item_source  *ris;
	int                         trigger;
	int                         rc;

	/*
	   Test:
	   - Confirm that formation correctly pulls items and sends them.
	   - Also verify that periodic item-source drain works.
	 */
	M0_ALLOC_PTR(ris);
	M0_UT_ASSERT(ris != NULL);
	m0_rpc_item_source_init(ris, "test-item-source", &ris_ops);
	m0_rpc_item_source_register(conn, ris);

	for (trigger = 0; trigger < 2; trigger++) {
		m0_fi_enable_once("has_item", "yes");
		m0_fi_enable("frm_is_ready", "ready");
		has_item_calls = get_item_calls = 0;
		switch (trigger) {
		case 0:
			m0_rpc_machine_lock(conn->c_rpc_machine);
			m0_rpc_frm_run_formation(&conn->c_rpcchan->rc_frm);
			m0_rpc_machine_unlock(conn->c_rpc_machine);
			break;
		case 1:
			m0_rpc_machine_lock(conn->c_rpc_machine);
			m0_rpc_machine_drain_item_sources(conn->c_rpc_machine,
							  128);
			m0_rpc_machine_unlock(conn->c_rpc_machine);
			break;
		default:
			M0_IMPOSSIBLE("only two triggers");
		}
		M0_UT_ASSERT(has_item_calls > get_item_calls &&
			     get_item_calls == 1);
		rc = m0_rpc_item_timedwait(item, M0_BITS(M0_RPC_ITEM_SENT,
							 M0_RPC_ITEM_FAILED),
					   m0_time_from_now(2, 0));
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(item->ri_sm.sm_state == M0_RPC_ITEM_SENT);

		/* riso_has_item() is set to return false.
		   Test that get_item does not get called when has_item
		   returns false
		 */
		has_item_calls = get_item_calls = 0;
		m0_rpc_machine_lock(conn->c_rpc_machine);
		m0_rpc_frm_run_formation(&conn->c_rpcchan->rc_frm);
		m0_rpc_machine_unlock(conn->c_rpc_machine);
		M0_UT_ASSERT(has_item_calls > get_item_calls &&
			     get_item_calls == 0);

		m0_fi_disable("frm_is_ready", "ready");
		m0_rpc_machine_lock(conn->c_rpc_machine);
		m0_rpc_item_put(item);
		m0_rpc_machine_unlock(conn->c_rpc_machine);
	}
	m0_rpc_item_source_deregister(ris);
	m0_rpc_item_source_fini(ris);
	m0_free(ris);
}

static void conn_terminating_cb_test(void)
{
	struct m0_rpc_item_source *ris;

	M0_ALLOC_PTR(ris);
	M0_UT_ASSERT(ris != NULL);
	m0_rpc_item_source_init(ris, "test-item-source", &ris_ops);
	m0_rpc_item_source_register(conn, ris);

	M0_UT_ASSERT(!conn_terminating_cb_called);
	stop_rpc_client_and_server();
	/* riso_conn_terminating() callback will be called on item-sources
	   which were still registered when rpc-conn was being terminated
	 */
	M0_UT_ASSERT(conn_terminating_cb_called);
}

struct m0_ut_suite item_source_ut = {
	.ts_name = "rpc-item-source-ut",
	.ts_init = item_source_test_suite_init,
	.ts_fini = item_source_test_suite_fini,
	.ts_tests = {
		{ "basic",                    item_source_basic_test   },
		{ "item_source_limits",       item_source_limits_test  },
		{ "item_pull",                item_source_test         },
		{ "conn_terminating_cb_test", conn_terminating_cb_test },
		{ NULL,                       NULL                     },
	}
};

/** @} end of rpc group */


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
