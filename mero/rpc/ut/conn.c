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
 * Original author: Madhav Vemuri<madhav_vemuri@xyratex.com>
 * Original creation date: 09/20/2012
 */

#include "ut/ut.h"
#include "lib/mutex.h"
#include "lib/finject.h"
#include "fop/fop.h"
#include "mero/magic.h"
#include "rpc/rpc_internal.h"

enum {
	SENDER_ID = 1001,
};

static struct m0_rpc_machine machine;
static struct m0_rpc_conn    conn;
static struct m0_fop	     est_fop;
static struct m0_fop	     term_fop;
static struct m0_fop	     est_fop_rep;
static struct m0_fop	     term_fop_rep;

static struct m0_rpc_fop_conn_establish_rep est_reply;
static struct m0_rpc_fop_conn_terminate_rep term_reply;

static struct m0_net_end_point ep;

M0_TL_DESCR_DEFINE(rpc_conn_ut, "rpc-conn", static, struct m0_rpc_conn,
		   c_link, c_magic, M0_RPC_CONN_MAGIC, M0_RPC_CONN_HEAD_MAGIC);
M0_TL_DEFINE(rpc_conn_ut, static, struct m0_rpc_conn);

static int conn_ut_init(void)
{
	ep.nep_addr = "dummy ep";

	est_fop.f_item.ri_reply  = &est_fop_rep.f_item;
	term_fop.f_item.ri_reply = &term_fop_rep.f_item;

	rpc_conn_ut_tlist_init(&machine.rm_incoming_conns);
	rpc_conn_ut_tlist_init(&machine.rm_outgoing_conns);
	rmach_watch_tlist_init(&machine.rm_watch);
	m0_sm_group_init(&machine.rm_sm_grp);

	m0_fi_enable("rpc_chan_get", "do_nothing");
	m0_fi_enable("rpc_chan_put", "do_nothing");
	m0_fi_enable("m0_rpc_frm_run_formation", "do_nothing");
	m0_fi_enable("m0_rpc__fop_post", "do_nothing");
	return 0;
}

static int conn_ut_fini(void)
{
	rmach_watch_tlist_fini(&machine.rm_watch);
	rpc_conn_ut_tlist_fini(&machine.rm_incoming_conns);
	rpc_conn_ut_tlist_fini(&machine.rm_outgoing_conns);
	m0_sm_group_fini(&machine.rm_sm_grp);
	m0_fi_disable("rpc_chan_get", "do_nothing");
	m0_fi_disable("rpc_chan_put", "do_nothing");
	m0_fi_disable("m0_rpc_frm_run_formation", "do_nothing");
	m0_fi_disable("m0_rpc__fop_post", "do_nothing");

	m0_fi_disable("rpc_chan_get", "fake_error");
	m0_fi_disable("m0_alloc", "fail_allocation");
	m0_fi_disable("m0_rpc__fop_post", "fake_error");
	return 0;
}

static void conn_init(void)
{
	int rc;
	rc = m0_rpc_conn_init(&conn, NULL, &ep, &machine, 1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_INITIALISED);
	M0_UT_ASSERT(conn.c_rpc_machine == &machine);
}

static void fop_set_session(struct m0_fop *fop)
{
	fop->f_item.ri_session = m0_rpc_conn_session0(&conn);
}

static void conn_init_fini_test(void)
{
	struct m0_uint128 uuid;
	int		  rc;

	/* Checks for RPC connection initialisation and finalisation. */
	conn_init();

	uuid = conn.c_uuid;

	m0_rpc_conn_fini(&conn);

	/* Check for Receive side conn init and fini */
	m0_rpc_machine_lock(&machine);
	rc = m0_rpc_rcv_conn_init(&conn, &ep, &machine, &uuid);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_INITIALISED);
	M0_UT_ASSERT(conn.c_rpc_machine == &machine);
	M0_UT_ASSERT(m0_uint128_cmp(&conn.c_uuid, &uuid) == 0);

	m0_rpc_conn_fini(&conn);
}

static void conn_init_and_establish(void)
{
	int rc;
	/* Checks for Conn M0_RPC_CONN_INITIALISED => M0_RPC_CONN_CONNECTING */
	conn_init();

	rc = m0_rpc_conn_establish(&conn, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_CONNECTING);

	fop_set_session(&est_fop);
	fop_set_session(&est_fop_rep);

	est_fop.f_item.ri_error    = 0;
	est_reply.rcer_sender_id   = SENDER_ID; /* sender_id_allocate() */
	est_reply.rcer_rc          = 0;
	est_fop_rep.f_data.fd_data = &est_reply;

}

static void conn_establish_reply(void)
{
	/* Checks for Conn M0_RPC_CONN_CONNECTING => M0_RPC_CONN_ACTIVE */
	m0_rpc_machine_lock(&machine);
	m0_rpc_conn_establish_reply_received(&est_fop.f_item);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_ACTIVE);
}

static void conn_terminate(void)
{
	int rc;
	/* Checks for Conn M0_RPC_CONN_ACTIVE => M0_RPC_CONN_TERMINATING */
	rc = m0_rpc_conn_terminate(&conn, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_TERMINATING);

	fop_set_session(&term_fop);
	fop_set_session(&term_fop_rep);

	term_fop.f_item.ri_error    = 0;
	term_reply.ctr_sender_id    = est_reply.rcer_sender_id;
	term_reply.ctr_rc           = 0;
	term_fop_rep.f_data.fd_data = &term_reply;

}

static void conn_terminate_reply_and_fini(void)
{
	/* Checks for Conn M0_RPC_CONN_TERMINATING => M0_RPC_CONN_TERMINATED */

	m0_rpc_machine_lock(&machine);
	m0_rpc_conn_terminate_reply_received(&term_fop.f_item);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_TERMINATED);

	/* Checks for Conn M0_RPC_CONN_TERMINATED => M0_RPC_CONN_FINALISED */
	m0_rpc_conn_fini(&conn);
}

static void conn_check(void)
{
	conn_init_and_establish();
	conn_establish_reply();
	conn_terminate();
	conn_terminate_reply_and_fini();
}

static void conn_init_fail_test(void)
{
	int rc;
	/* Checks for m0_rpc_conn_init() failure due to allocation failure */
	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_rpc_conn_init(&conn, NULL, &ep, &machine, 1);
	M0_UT_ASSERT(rc == -ENOMEM);

	/* Checks for failure due to error in rpc_chan_get() */
	m0_fi_enable_once("rpc_chan_get", "fake_error");
	rc = m0_rpc_conn_init(&conn, NULL, &ep, &machine, 1);
	M0_UT_ASSERT(rc == -ENOMEM);
}

static void conn_establish_fail_test(void)
{
	int rc;
	/* Checks for Conn M0_RPC_CONN_INITIALISED => M0_RPC_CONN_FAILED */
	conn_init();

	m0_fi_enable_once("m0_rpc__fop_post", "fake_error");
	rc = m0_rpc_conn_establish(&conn, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);

	/* Allocation failure */
	conn_init();

	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_rpc_conn_establish(&conn, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -ENOMEM);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);
}

static void conn_establish_reply_fail_test(void)
{
	/* Checks for Conn M0_RPC_CONN_CONNECTING => M0_RPC_CONN_FAILED */
	conn_init_and_establish();

	est_fop.f_item.ri_error = -EINVAL;
	m0_rpc_machine_lock(&machine);
	m0_rpc_conn_establish_reply_received(&est_fop.f_item);
	M0_UT_ASSERT(conn.c_sm.sm_rc == -EINVAL);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);
	est_fop.f_item.ri_error = 0;

	/* Due to invalid sender id. */
	conn_init_and_establish();

	est_reply.rcer_sender_id = SENDER_ID_INVALID;
	m0_rpc_machine_lock(&machine);
	m0_rpc_conn_establish_reply_received(&est_fop.f_item);
	M0_UT_ASSERT(conn.c_sm.sm_rc == -EPROTO);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);
	est_reply.rcer_sender_id = SENDER_ID; /* restore */

}

static void conn_terminate_fail_test(void)
{
	int rc;
	/* Checks for Conn M0_RPC_CONN_ACTIVE => M0_RPC_CONN_FAILED */
	conn_init_and_establish();
	conn_establish_reply();

	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_rpc_conn_terminate(&conn, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -ENOMEM);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);

	/* Due to m0_rpc__fop_post() failure. */
	conn_init_and_establish();
	conn_establish_reply();

	m0_fi_enable_once("m0_rpc__fop_post", "fake_error");
	rc = m0_rpc_conn_terminate(&conn, m0_time_from_now(2, 0));
	M0_UT_ASSERT(rc == -EINVAL);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);
}

static void conn_terminate_reply_fail_test(void)
{
	/* Checks for Conn M0_RPC_CONN_TERMINATING => M0_RPC_CONN_FAILED */
	conn_init_and_establish();
	conn_establish_reply();
	conn_terminate();

	term_fop.f_item.ri_error = -EINVAL;
	m0_rpc_machine_lock(&machine);
	m0_rpc_conn_terminate_reply_received(&term_fop.f_item);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(conn.c_sm.sm_rc == -EINVAL);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);
	term_fop.f_item.ri_error = 0;

	/* Due to non-matching sender id. */
	conn_init_and_establish();
	conn_establish_reply();
	conn_terminate();

	term_reply.ctr_sender_id = SENDER_ID + 1;
	m0_rpc_machine_lock(&machine);
	m0_rpc_conn_terminate_reply_received(&term_fop.f_item);
	m0_rpc_machine_unlock(&machine);
	M0_UT_ASSERT(conn.c_sm.sm_rc == -EPROTO);
	M0_UT_ASSERT(conn_state(&conn) == M0_RPC_CONN_FAILED);

	m0_rpc_conn_fini(&conn);
}

struct m0_ut_suite conn_ut = {
	.ts_name = "rpc-connection-ut",
	.ts_init = conn_ut_init,
	.ts_fini = conn_ut_fini,
	.ts_tests = {
		{ "conn-init-fini",            conn_init_fini_test           },
		{ "conn-check",                conn_check                    },
		{ "conn-init-fail",            conn_init_fail_test           },
		{ "conn-establish-fail",       conn_establish_fail_test      },
		{ "conn-terminate-fail",       conn_terminate_fail_test      },
		{ "conn-establish-reply-fail", conn_establish_reply_fail_test},
		{ "conn-terminate_reply-fail", conn_terminate_reply_fail_test},
		{ NULL, NULL}
	}
};
M0_EXPORTED(conn_ut);
