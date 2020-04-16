/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "lib/memory.h"
#include "ut/ut.h"
struct m0_rpc_conn;
#include "rpc/rpc_machine_internal.h"  /* m0_rpc_machine_lock */
#include "rpc/session_internal.h"      /* m0_rpc_session_hold_busy */
#include "rpc/item_internal.h"         /* m0_rpc_item_sm_init */
#include "fdmi/fdmi.h"
#include "fdmi/service.h"              /* m0_reqh_fdmi_service */
#include "fdmi/source_dock_internal.h"
#include "fdmi/fops.h"                 /* m0_fop_fdmi_rec_release */

#include "fdmi/ut/sd_common.h"

static struct test_rpc_env    g_rpc_env;
static struct m0_rpc_packet  *g_sent_rpc_packet;

static int test_packet_ready(struct m0_rpc_packet *p);

static const struct m0_rpc_frm_ops test_frm_ops = {
	.fo_packet_ready = test_packet_ready,
};


static struct m0_semaphore     g_sem;
static struct m0_uint128 rec_id_to_release = M0_UINT128(0xDEAD, 0xBEEF);
static struct m0_fdmi_src_rec  g_src_rec;

static int test_fs_node_eval(
	        struct m0_fdmi_src_rec *src_rec,
		struct m0_fdmi_flt_var_node *value_desc,
		struct m0_fdmi_flt_operand *value)
{
	M0_UT_ASSERT(false);
	return 0;
}

static int test_fs_encode(struct m0_fdmi_src_rec *src_rec,
			   struct m0_buf          *buf)
{
	M0_UT_ASSERT(false);
	return 0;
}

static void test_fs_get(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(false);
}

static void test_fs_put(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(!m0_uint128_cmp(&src_rec->fsr_rec_id, &rec_id_to_release));
	/**
	 * Calling of this function is a sign that FDMI
	 * finished release FOP handling.
	 */
	m0_semaphore_up(&g_sem);
}

static struct m0_fdmi_src *src_alloc()
{
	struct m0_fdmi_src *src;
	int                 rc;

	rc = m0_fdmi_source_alloc(M0_FDMI_REC_TYPE_TEST, &src);
	M0_UT_ASSERT(rc == 0);

	src->fs_encode     = test_fs_encode;
	src->fs_get        = test_fs_get;
	src->fs_put        = test_fs_put;
	src->fs_node_eval  = test_fs_node_eval;
	return src;
}

int test_packet_ready(struct m0_rpc_packet *p)
{
	g_sent_rpc_packet = p;

	m0_semaphore_up(&g_sem);
	return 0;
}

int imitate_release_fop_recv(struct test_rpc_env *env)
{
	int                              rc;
	struct m0_fop                   *fop;
	struct m0_fop_fdmi_rec_release  *fop_data;
	struct m0_reqh                  *reqh;
	struct m0_rpc_item              *rpc_item;

	M0_ENTRY();

	reqh = &g_sd_ut.mero.cc_reqh_ctx.rc_reqh;

	fop = m0_fop_alloc(&m0_fop_fdmi_rec_release_fopt, NULL,
			   &env->tre_rpc_machine);
	M0_UT_ASSERT(fop != NULL);

	fop_data = m0_fop_data(fop);
	fop_data->frr_frid = rec_id_to_release;
	fop_data->frr_frt  = M0_FDMI_REC_TYPE_TEST;
	rpc_item = &fop->f_item;

	m0_fop_rpc_machine_set(fop, &env->tre_rpc_machine);
	m0_rpc_item_sm_init(rpc_item, M0_RPC_ITEM_INCOMING);

	rpc_item->ri_session = env->tre_session;

	m0_rpc_machine_lock(&env->tre_rpc_machine);
	m0_rpc_item_change_state(rpc_item, M0_RPC_ITEM_ACCEPTED);
	m0_rpc_machine_unlock(&env->tre_rpc_machine);

	m0_rpc_machine_lock(&env->tre_rpc_machine);
	m0_rpc_session_hold_busy(env->tre_session);
	m0_rpc_machine_unlock(&env->tre_rpc_machine);

	rc = m0_reqh_fop_handle(reqh, fop);

	M0_LEAVE();
	return rc;
}

void fdmi_sd_release_fom(void)
{
	struct m0_fdmi_src             *src = src_alloc();
	int                             rc;
	static struct m0_rpc_conn       rpc_conn;
	static struct m0_rpc_session    rpc_session;
	bool                            ok;

	M0_ENTRY();

	fdmi_serv_start_ut(&filterc_stub_ops);
	prepare_rpc_env(&g_rpc_env, &g_sd_ut.mero.cc_reqh_ctx.rc_reqh,
			&test_frm_ops, false, &rpc_conn, &rpc_session);
	m0_semaphore_init(&g_sem, 0);
	rc = m0_fdmi_source_register(src);
	M0_UT_ASSERT(rc == 0);
	g_src_rec.fsr_src = src;
	m0_fdmi__record_init(&g_src_rec);
	m0_fdmi__rec_id_gen(&g_src_rec);
	rec_id_to_release = g_src_rec.fsr_rec_id;
	rc = imitate_release_fop_recv(&g_rpc_env);
	M0_UT_ASSERT(rc == 0);

	/**
	 * Wait until record is processed and released.  Must happen within 10
	 * sec, otherwise we consider it a failure.
	 */
	ok = m0_semaphore_timeddown(&g_sem, m0_time_from_now(10, 0));
	M0_UT_ASSERT(ok);

	/**
	 * Wait until record is sent over RPC.  Must happen within 10 sec,
	 * otherwise we consider it a failure.
	 */
	ok = m0_semaphore_timeddown(&g_sem, m0_time_from_now(10, 0));
	M0_UT_ASSERT(ok);
	fdmi_ut_packet_send_failed(&g_rpc_env.tre_rpc_machine,
				   g_sent_rpc_packet);
	m0_fdmi__record_deinit(&g_src_rec);
	m0_fdmi_source_deregister(src);
	m0_fdmi_source_free(src);
	m0_semaphore_fini(&g_sem);
	unprepare_rpc_env(&g_rpc_env);
	fdmi_serv_stop_ut();
	M0_LEAVE();
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
