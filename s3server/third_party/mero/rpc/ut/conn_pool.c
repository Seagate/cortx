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
 * Original author:
 * Original creation date:
 */

#include <unistd.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RPC
#include "lib/trace.h"

#include "ut/ut.h"
#include "rpc/conn_pool.h"
#include "rpc/conn_pool_internal.h"
#include "lib/finject.h"

#include "net/lnet/lnet.h"         /* m0_net_lnet_xprt */
#include "rpc/rpclib.h"
#include "rpc/ut/clnt_srv_ctx.c"   /* sctx, cctx. NOTE: This is .c file */
#include "ut/cs_service.h"         /* m0_cs_default_stypes */
#include "ut/misc.h"

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static void rpc_conn_pool(void)
{
	struct m0_rpc_conn_pool		 pool = {};
	struct m0_rpc_session		*session  = NULL;
	struct m0_rpc_session		*session2 = NULL;
	int 				 rc;
	struct m0_rpc_conn_pool_item    *pool_item;

	start_rpc_client_and_server();

	rc = m0_rpc_conn_pool_init(
			&pool, &cctx.rcx_rpc_machine, M0_TIME_NEVER, 1);
	M0_ASSERT(rc == 0);
	M0_UT_ASSERT(rpc_conn_pool_items_tlist_is_empty(&pool.cp_items));

	/* new connection */
	rc = m0_rpc_conn_pool_get_sync(&pool, SERVER_ENDPOINT_ADDR, &session);
	M0_UT_ASSERT(session != NULL);
	M0_UT_ASSERT(rc == 0);
	pool_item = rpc_conn_pool_items_tlist_head(&pool.cp_items);
	M0_UT_ASSERT(pool_item ==
		     rpc_conn_pool_items_tlist_tail(&pool.cp_items));
	M0_UT_ASSERT(m0_rpc_conn_pool_session_established(session));
	M0_UT_ASSERT(pool_item->cpi_users_nr == 1);

	while (!m0_rpc_conn_pool_session_established(session)) {
		/* Busy cycle to wait until session is established */
	}

	/* reuse connection */
	rc = m0_rpc_conn_pool_get_sync(&pool, SERVER_ENDPOINT_ADDR, &session2);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(session == session2);
	pool_item = rpc_conn_pool_items_tlist_head(&pool.cp_items);
	M0_UT_ASSERT(pool_item ==
		     rpc_conn_pool_items_tlist_tail(&pool.cp_items));
	M0_UT_ASSERT(m0_rpc_conn_pool_session_established(session2));
	M0_UT_ASSERT(pool_item->cpi_users_nr == 2);

	/* put back #1 */
	m0_rpc_conn_pool_put(&pool, session2);
	pool_item = rpc_conn_pool_items_tlist_head(&pool.cp_items);
	M0_UT_ASSERT(pool_item ==
		     rpc_conn_pool_items_tlist_tail(&pool.cp_items));
	M0_UT_ASSERT(m0_rpc_conn_pool_session_established(session));
	M0_UT_ASSERT(pool_item->cpi_users_nr == 1);

	/* put back #2 */
	m0_rpc_conn_pool_put(&pool, session2);
	pool_item = rpc_conn_pool_items_tlist_head(&pool.cp_items);
	M0_UT_ASSERT(pool_item ==
		     rpc_conn_pool_items_tlist_tail(&pool.cp_items));
	M0_UT_ASSERT(m0_rpc_conn_pool_session_established(session));
	M0_UT_ASSERT(pool_item->cpi_users_nr == 0);

	m0_rpc_conn_pool_fini(&pool);
	stop_rpc_client_and_server();
}

struct cp_pending_item {
	struct m0_rpc_session  *session;
	struct m0_clink	        clink;
	bool 		        cb_called;
};

static struct m0_cond connection_pending_cond;
static struct m0_mutex cond_mutex;

static bool pending_cp_clink_cb(struct m0_clink *clink)
{
	struct cp_pending_item		*cp_pending;

	m0_mutex_lock(&cond_mutex);
	cp_pending = container_of(clink, struct cp_pending_item, clink);
	cp_pending->cb_called = true;
	m0_cond_signal(&connection_pending_cond);
	m0_mutex_unlock(&cond_mutex);

	return true;
}

static void rpc_conn_pool_async(void) {
	struct m0_rpc_conn_pool		 pool = {};
	struct m0_rpc_session		*session  = NULL;
	int 				 rc;
	struct cp_pending_item		*cp_pending;
	struct m0_rpc_conn_pool_item    *pool_item;

	start_rpc_client_and_server();

	rc = m0_rpc_conn_pool_init(
			&pool, &cctx.rcx_rpc_machine, M0_TIME_NEVER, 1);
	M0_ASSERT(rc == 0);
	M0_UT_ASSERT(rpc_conn_pool_items_tlist_is_empty(&pool.cp_items));

	/* Initiate new connection */
	rc = m0_rpc_conn_pool_get_async(&pool, SERVER_ENDPOINT_ADDR, &session);
	M0_UT_ASSERT(rc == -EBUSY);
	pool_item = rpc_conn_pool_items_tlist_head(&pool.cp_items);
	M0_UT_ASSERT(pool_item ==
		     rpc_conn_pool_items_tlist_tail(&pool.cp_items));

	/* Call connect once again */
	rc = m0_rpc_conn_pool_get_async(&pool, SERVER_ENDPOINT_ADDR, &session);
	M0_UT_ASSERT(rc == -EBUSY);

	/* Configure callback */
	M0_ALLOC_PTR(cp_pending);

	m0_mutex_init(&cond_mutex);
	m0_cond_init(&connection_pending_cond, &cond_mutex);

	m0_clink_init(&cp_pending->clink, pending_cp_clink_cb);
	cp_pending->clink.cl_is_oneshot = true;
	cp_pending->session = session;
	cp_pending->cb_called = false;
	m0_clink_add_lock(m0_rpc_conn_pool_session_chan(session),
		&cp_pending->clink);

	m0_mutex_lock(&cond_mutex);
	m0_cond_wait(&connection_pending_cond);
	m0_mutex_unlock(&cond_mutex);

	M0_UT_ASSERT(cp_pending->cb_called);
	M0_UT_ASSERT(m0_rpc_conn_pool_session_established(
			cp_pending->session));

	m0_cond_fini(&connection_pending_cond);
	m0_mutex_fini(&cond_mutex);

	/* Connect once again */
	rc = m0_rpc_conn_pool_get_async(&pool, SERVER_ENDPOINT_ADDR, &session);
	M0_UT_ASSERT(rc == 0);

	m0_free(cp_pending);
	m0_rpc_conn_pool_fini(&pool);
	stop_rpc_client_and_server();
}

struct m0_ut_suite rpc_conn_pool_ut = {
	.ts_name = "rpc-conn-pool-ut",
	.ts_tests = {
		{ "rpc-conn-pool", rpc_conn_pool},
		{ "rpc-conn-pool-async", rpc_conn_pool_async},
		{ NULL, NULL },
	},
};

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
