/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 20-Oct-2014
 */

#include "layout/layout.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"        /* M0_LOG */
#include "lib/uuid.h"         /* m0_uuid_generate */

#include "ut/ut.h"            /* M0_UT_ASSERT */
#include "clovis/ut/clovis.h"

/*
 * Including the c files so we can replace the M0_CLOVIS_PRE asserts
 * in order to test them.
 */
#if defined(round_down)
#undef round_down
#endif
#if defined(round_up)
#undef round_up
#endif
#include "clovis/io_req_fop.c"

struct m0_ut_suite        ut_suite_clovis_io_req_fop;
static struct m0_clovis  *dummy_instance;

/**
 * Tests ioreq_fop_invariant().
 */
static void ut_clovis_test_ioreq_fop_invariant(void)
{
	struct ioreq_fop *fop;
	bool              ret;

	fop = ut_clovis_dummy_ioreq_fop_create();

	/* Base case. */
	ret = ioreq_fop_invariant(fop);
	M0_UT_ASSERT(ret == true);

	ut_clovis_dummy_ioreq_fop_delete(fop);
}

static void ut_clovis_test_failure_vector_mismatch(void)
{
}

static void ut_clovis_test_clovis_io_bottom_half(void)
{
}

static uint32_t ut_clovis_sa_cb_executed;
static void ut_clovis_mock_rpc_item_sa_cb(struct m0_sm_group *grp,
					  struct m0_sm_ast *ast)
{
	++ut_clovis_sa_cb_executed;
}

static void ut_clovis_mock_rpc_item_release(struct m0_ref *ref)
{
	/* Do nothing. */
}

/**
 * Tests io_rpc_item_cb().
 */
static void ut_clovis_test_io_rpc_item_cb(void)
{
	struct m0_fop          *fop;
	struct m0_fop          *rep_fop;
	struct m0_rpc_item     *item;
	struct m0_clovis_op_io *ioo;
	struct ioreq_fop       *reqfop;
	struct m0_clovis       *instance = NULL;

	/* Init. */
	instance = dummy_instance;
	M0_ALLOC_PTR(reqfop);
	M0_ALLOC_PTR(rep_fop);

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	M0_ALLOC_PTR(reqfop->irf_tioreq);
	reqfop->irf_tioreq->ti_nwxfer = &ioo->ioo_nwxfer;
	ioreq_fop_bob_init(reqfop);
	fop = &reqfop->irf_iofop.if_fop;
	item = &fop->f_item;
	item->ri_reply = &rep_fop->f_item;
	m0_ref_init(&fop->f_ref, 777, ut_clovis_mock_rpc_item_release);
	m0_ref_init(&rep_fop->f_ref, 777, ut_clovis_mock_rpc_item_release);
	reqfop->irf_ast.sa_cb = ut_clovis_mock_rpc_item_sa_cb;

	ut_clovis_sa_cb_executed = 0;

	io_rpc_item_cb(item);
	M0_UT_ASSERT(m0_ref_read(&rep_fop->f_ref) == 778);
	M0_UT_ASSERT(ut_clovis_sa_cb_executed == 0);

	/* The CB should be enqueued by now. Force its execution. */
	m0_sm_group_lock(ioo->ioo_sm.sm_grp);
	m0_sm_group_unlock(ioo->ioo_sm.sm_grp);
	M0_UT_ASSERT(ut_clovis_sa_cb_executed == 1);

	ioreq_fop_bob_fini(reqfop);
	m0_free(reqfop->irf_tioreq);
	ut_clovis_dummy_ioo_delete(ioo, instance);

	/* Fini. */
	m0_free(rep_fop);
	m0_free(reqfop);
}

static void ut_clovis_test_client_passive_recv(void)
{
}

static void ut_clovis_test_ioreq_fop_async_submit(void)
{
}

/**
 * Tests ioreq_fop_release().
 */
static void ut_clovis_test_ioreq_fop_release(void)
{
	struct ioreq_fop       *fop;
	struct target_ioreq    *ti;
	struct m0_clovis_op_io *ioo;
	int                     rc;
	struct m0_clovis       *instance = NULL;

	/* initialise clovis */
	instance = dummy_instance;

	/* Base case. */
	M0_ALLOC_PTR(fop);
	ti = ut_clovis_dummy_target_ioreq_create();
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ti->ti_nwxfer = &ioo->ioo_nwxfer;
	m0_mutex_init(&ti->ti_nwxfer->nxr_lock);
	/* This is dirty, but avoids a chain of sm_move */
	ioo->ioo_sm.sm_state = IRS_READING;

	rc = ioreq_fop_init(fop, ti, PA_DATA);
	M0_UT_ASSERT(rc == 0);
	rpcbulk_tlist_init(&fop->irf_iofop.if_rbulk.rb_buflist);
	m0_mutex_init(&fop->irf_iofop.if_rbulk.rb_mutex);

	ioreq_fop_release(&fop->irf_iofop.if_fop.f_ref);

	ioo->ioo_sm.sm_state = IRS_READ_COMPLETE;
	ut_clovis_dummy_ioo_delete(ioo, instance);
	ut_clovis_dummy_target_ioreq_delete(ti);
}

/**
 * Tests ioreq_fop_init().
 */
static void ut_clovis_test_ioreq_fop_init(void)
{
	struct ioreq_fop       *fop;
	struct target_ioreq    *ti;
	struct m0_clovis_op_io *ioo;
	int                     rc;
	struct m0_clovis       *instance = NULL;

	/* initialise clovis */
	instance = dummy_instance;

	/* Base case. */
	M0_ALLOC_PTR(fop);
	ti = ut_clovis_dummy_target_ioreq_create();
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ti->ti_nwxfer = &ioo->ioo_nwxfer;
	/* This is dirty, but avoids a chain of sm_move */
	ioo->ioo_sm.sm_state = IRS_READING;

	rc = ioreq_fop_init(fop, ti, PA_DATA);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(fop->irf_pattr == PA_DATA);
	M0_UT_ASSERT(fop->irf_tioreq == ti);
	M0_UT_ASSERT(fop->irf_reply_rc == 0);
	M0_UT_ASSERT(fop->irf_ast.sa_cb == clovis_io_bottom_half);
	M0_UT_ASSERT(fop->irf_iofop.if_fop.f_item.ri_ops == &clovis_item_ops);

	m0_free(fop->irf_iofop.if_fop.f_data.fd_data);
	ioo->ioo_sm.sm_state = IRS_INITIALIZED;

	ut_clovis_dummy_ioo_delete(ioo, instance);
	ut_clovis_dummy_target_ioreq_delete(ti);
	m0_free(fop);
}

/**
 * Tests ioreq_fop_fini().
 */
static void ut_clovis_test_ioreq_fop_fini(void)
{
	struct ioreq_fop *fop;

	/* Base case. */
	fop = ut_clovis_dummy_ioreq_fop_create();
	iofops_tlink_init(fop);
	ioreq_fop_fini(fop);
	M0_UT_ASSERT(fop->irf_tioreq == NULL);
	M0_UT_ASSERT(fop->irf_ast.sa_cb == NULL);
	M0_UT_ASSERT(fop->irf_ast.sa_mach == NULL);

	ut_clovis_dummy_ioreq_fop_delete(fop);
}

static void ut_clovis_test_ioreq_pgiomap_find(void)
{
	struct pargrp_iomap    *map;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_op_io *ioo;
	uint64_t                cursor = 0;

	/* init */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	cursor = 0;

	/* Base cases. */
	ioreq_pgiomap_find(ioo, 0, &cursor, &map);

	/* fini */
	ut_clovis_dummy_ioo_delete(ioo, instance);

}

static void ut_clovis_test_ioreq_fop_dgmode_read(void)
{
}

M0_INTERNAL int ut_clovis_io_req_fop_init(void)
{
	int                       rc;
	struct m0_pdclust_layout *dummy_pdclust_layout;

#ifndef __KERNEL__
	ut_clovis_shuffle_test_order(&ut_suite_clovis_io_req_fop);
#endif

	m0_clovis_init_io_op();

	rc = ut_m0_clovis_init(&dummy_instance);
	M0_UT_ASSERT(rc == 0);

	ut_clovis_layout_domain_fill(dummy_instance);
	dummy_pdclust_layout =
		ut_clovis_dummy_pdclust_layout_create(dummy_instance);
	M0_UT_ASSERT(dummy_pdclust_layout != NULL);

	return 0;
}

M0_INTERNAL int ut_clovis_io_req_fop_fini(void)
{
	ut_clovis_layout_domain_empty(dummy_instance);
	ut_m0_clovis_fini(&dummy_instance);

	return 0;
}

struct m0_ut_suite ut_suite_clovis_io_req_fop = {
	.ts_name = "clovis-io-req-fop-ut",
	.ts_init = ut_clovis_io_req_fop_init,
	.ts_fini = ut_clovis_io_req_fop_fini,
	.ts_tests = {

		{ "ioreq_fop_invariant",
				    &ut_clovis_test_ioreq_fop_invariant},
		{ "failure_vector_mismatch",
				    &ut_clovis_test_failure_vector_mismatch},
		{ "clovis_io_bottom_half",
				    &ut_clovis_test_clovis_io_bottom_half},
		{ "io_rpc_item_cb",
				    &ut_clovis_test_io_rpc_item_cb},
		{ "client_passive_recv",
				    &ut_clovis_test_client_passive_recv},
		{ "ioreq_fop_async_submit",
				    &ut_clovis_test_ioreq_fop_async_submit},
		{ "ioreq_fop_release",
				    &ut_clovis_test_ioreq_fop_release},
		{ "ioreq_fop_init",
				    &ut_clovis_test_ioreq_fop_init},
		{ "ioreq_fop_fini",
				    &ut_clovis_test_ioreq_fop_fini},
		{ "ioreq_pgiomap_find",
				    &ut_clovis_test_ioreq_pgiomap_find},
		{ "ioreq_fop_dgmode_read",
				    &ut_clovis_test_ioreq_fop_dgmode_read},
		{ NULL, NULL },
	}
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

