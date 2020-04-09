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
#include "lib/finject.h"      /* Failure Injection */
#include "ioservice/fid_convert.h"

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
#include "clovis/io_req.c"

#include "layout/layout_internal.h" /* REMOVE ME */

struct m0_ut_suite        ut_suite_clovis_io_req;
static struct m0_clovis  *dummy_instance;

/**
 * Tests ioreq_sm_state().
 */
static void ut_clovis_test_ioreq_sm_state(void)
{
	struct m0_clovis_op_io  ioo;
	uint32_t                state;

	/* Base case. */
	ioo.ioo_sm.sm_state = 777;
	state = ioreq_sm_state(&ioo);
	M0_UT_ASSERT(state == 777);
}

/**
 * Tests ioreq_sm_state_set_locked().
 */
static void ut_clovis_test_ioreq_sm_state_set_locked(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;

	/* Init. */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);

	/* Base case. */
	m0_sm_group_lock(&instance->m0c_sm_group);

	ioreq_sm_state_set_locked(ioo, IRS_READING);

	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_READING);
	/* Extra transitions so we can fini. */
	m0_sm_state_set(&ioo->ioo_sm, IRS_READ_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests ioreq_sm_failed_locked().
 */
static void ut_clovis_test_ioreq_sm_failed_locked(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;

	/* Init. */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);

	/* Base case. */
	m0_sm_group_lock(&instance->m0c_sm_group);
	ioreq_sm_failed_locked(ioo, -777);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_FAILED);
	M0_UT_ASSERT(ioo->ioo_sm.sm_rc == -777);
	m0_sm_group_unlock(&instance->m0c_sm_group);
	ut_clovis_dummy_ioo_delete(ioo, instance);

}

static int
ut_clovis_mock_handle_launch_dispatch(struct nw_xfer_request *xfer)
{
	if(M0_FI_ENABLED("ut_mock_launch_dispatch_fails"))
		return -EINVAL;
	return 0;
}

static int
ut_clovis_mock_handle_executed_adc(struct m0_clovis_op_io *ioo,
					   	enum copy_direction dir,
					   	enum page_attr filter)
{
	if (M0_FI_ENABLED("ut_mock_handle_executed_adc_fails")) {
		ioo->ioo_rc = -EINVAL;
		return -EINVAL;
	}
	return 0;
}

static int
ut_clovis_mock_ioreq_parity_recalc(struct m0_clovis_op_io *ioo)
{
	if (M0_FI_ENABLED("ut_mock_ioreq_parity_recalc_fails"))
		return -EINVAL;
	return 0;
}

void ut_clovis_mock_handle_executed_complete(struct nw_xfer_request  *xfer,
					       bool rmw)
{
	/* Empty */
}

static void
ut_clovis_mock_ioreq_iosm_handle_executed(struct m0_sm_group *grp,
                                       struct m0_sm_ast *ast)
{
	/* Empty */
}
/**
 * Tests ioreq_iosm_handle_launch().
 */
static void ut_clovis_test_ioreq_iosm_handle_launch(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_sm_group      grp;
	struct m0_sm_group     *op_grp;
	struct m0_sm_group     *en_grp;
	struct m0_clovis       *instance = NULL;
	struct nw_xfer_ops     *nxr_ops;
	struct m0_clovis_realm  realm;
	struct m0_clovis_entity entity;
	struct m0_clovis_op_io_ops *ioo_ops;
	struct nw_xfer_request *xfer_req;

	/* Init. */
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &entity, instance);

	/* Base case. */
	m0_sm_group_init(&grp);

	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	op_grp = &ioo->ioo_oo.oo_oc.oc_op.op_sm_group;
	en_grp = &entity.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	M0_UT_ASSERT(ioo->ioo_iomap_nr == 1);
	/* ioo->ioo_iomap_nr == 1 so idx must be 1 */
	ioo->ioo_oo.oo_oc.oc_op.op_entity = &entity;
	ioo->ioo_map_idx = 1;
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	M0_ALLOC_PTR(nxr_ops);
	nxr_ops->nxo_dispatch = &ut_clovis_mock_handle_launch_dispatch;
	nxr_ops->nxo_complete = &ut_clovis_mock_handle_executed_complete;
	ioo->ioo_nwxfer.nxr_ops = nxr_ops;

	/* Change the ioo to not use the instance's group. */
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&grp);

	/* nxo_dispatch fails */
	m0_fi_enable_once("ut_clovis_mock_handle_launch_dispatch",
					"ut_mock_launch_dispatch_fails");
	m0_fi_enable_once("m0_clovis_op_failed", "skip_ongoing_io_ref");
	ioreq_iosm_handle_launch(&grp, &ioo->ioo_ast);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_FAILED);

	m0_sm_group_lock(op_grp);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);

	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(&grp);

	/* Set ioo for Reading */
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);

	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&grp);
	ioreq_iosm_handle_launch(&grp, &ioo->ioo_ast);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_READING);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_LAUNCHED );

	m0_sm_state_set(&ioo->ioo_sm, IRS_READ_COMPLETE);
	m0_sm_state_set(&ioo->ioo_sm, IRS_REQ_COMPLETE);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&ioo->ioo_oo.oo_oc.oc_op.op_sm, -777, M0_CLOVIS_OS_FAILED);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_unlock(&grp);

	/* Set ioo for writing */
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_WRITE;
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);

	/* Set some mock callbacks */
	M0_ALLOC_PTR(ioo_ops);
        ioo_ops->iro_application_data_copy =
                &ut_clovis_mock_handle_executed_adc;
	ioo_ops->iro_parity_recalc =
		&ut_clovis_mock_ioreq_parity_recalc;
	ioo->ioo_ops = ioo_ops;

	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&grp);
	ioreq_iosm_handle_launch(&grp, &ioo->ioo_ast);

	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_WRITING);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_LAUNCHED );

	m0_sm_state_set(&ioo->ioo_sm, IRS_WRITE_COMPLETE);
	m0_sm_state_set(&ioo->ioo_sm, IRS_REQ_COMPLETE);

	m0_sm_group_lock(op_grp);
	m0_sm_move(&ioo->ioo_oo.oo_oc.oc_op.op_sm, -777, M0_CLOVIS_OS_FAILED);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);

	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(&grp);

	/* ioo for Writing bu iro_application_data_copy fails */
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_WRITE;
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&grp);

	m0_fi_enable_once("ut_clovis_mock_handle_executed_adc",
					"ut_mock_handle_executed_adc_fails");
	m0_fi_enable_once("m0_clovis_op_failed", "skip_ongoing_io_ref");
	ioreq_iosm_handle_launch(&grp, &ioo->ioo_ast);

	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_FAILED );

	m0_sm_group_lock(op_grp);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_unlock(&grp);

	/* ioo for writing but iro_parity_recalc fails */
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_WRITE;
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&grp);


	m0_fi_enable_once("ut_clovis_mock_ioreq_parity_recalc",
					"ut_mock_ioreq_parity_recalc_fails");
	m0_fi_enable_once("m0_clovis_op_failed", "skip_ongoing_io_ref");
	ioreq_iosm_handle_launch(&grp, &ioo->ioo_ast);

	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_FAILED );

	m0_sm_group_lock(op_grp);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_group_unlock(&grp);

	/* ioo->ioo_iomap_nr != 1 ioo->ioo_map_idx */
	ioo->ioo_map_idx = 0;
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	xfer_req = ut_clovis_dummy_xfer_req_create();
	ioo->ioo_nwxfer = *xfer_req;
	tioreqht_htable_init(&ioo->ioo_nwxfer.nxr_tioreqs_hash, 1);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&grp);

	/* Set some callbacks */
	ioo_ops->iro_iosm_handle_executed = &ut_clovis_mock_ioreq_iosm_handle_executed;
	ioo->ioo_ops = ioo_ops;
	ioreq_iosm_handle_launch(&grp, &ioo->ioo_ast);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_READ_COMPLETE);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_LAUNCHED);

	tioreqht_htable_fini(&ioo->ioo_nwxfer.nxr_tioreqs_hash);
	ut_clovis_dummy_xfer_req_delete(xfer_req);
	m0_sm_group_lock(ioo->ioo_oo.oo_sm_grp);
	m0_sm_asts_run(ioo->ioo_oo.oo_sm_grp);
	m0_sm_group_unlock(ioo->ioo_oo.oo_sm_grp);
	m0_sm_group_lock(op_grp);
	m0_sm_move(&ioo->ioo_oo.oo_oc.oc_op.op_sm, -777, M0_CLOVIS_OS_FAILED);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_sm_state_set(&ioo->ioo_sm, IRS_REQ_COMPLETE);
	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(&grp);

	m0_free(nxr_ops);
	m0_free(ioo_ops);

	/* dummy_ioo_delete() won't crash when fining ioo_sm */
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED,
		   &instance->m0c_sm_group);

	/* Finalise op_sm. */
	m0_sm_group_lock(op_grp);
	m0_sm_move(&ioo->ioo_oo.oo_oc.oc_op.op_sm, -777, M0_CLOVIS_OS_FAILED);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);

	ut_clovis_dummy_ioo_delete(ioo, instance);
	m0_sm_group_fini(&grp);

	/* Fini. */
	m0_clovis_entity_fini(&entity);
}

static int ut_clovis_mock_ioreq_dgmode_read(struct m0_clovis_op_io *ioo,
					    bool rmw)
{
	if (M0_FI_ENABLED("ut_mock_dgmode_read_fails")) {
		ioo->ioo_rc = -EAGAIN;
		return -EAGAIN;
	}
	return 0;
}

static int ut_clovis_mock_ioreq_dgmode_write(struct m0_clovis_op_io *ioo,
					     bool rmw)
{
	if (M0_FI_ENABLED("ut_mock_dgmode_write_fails")) {
		ioo->ioo_rc = -EAGAIN;
		return -EAGAIN;
	}
	return 0;
}

static int ut_clovis_mock_ioreq_dgmode_recover(struct m0_clovis_op_io *ioo)
{
	return 0;
}

static int ut_clovis_mock_ioreq_parity_verify(struct m0_clovis_op_io *ioo)
{
	if (M0_FI_ENABLED("ut_mock_parity_verify_fail")) {
		ioo->ioo_rc = -EINVAL;
                return -EINVAL;
	}

	return 0;
}



/**
 * Tests ioreq_iosm_handle_executed().
 */
static void ut_clovis_test_ioreq_iosm_handle_executed(void)
{
	struct m0_sm_group          grp;
	struct m0_sm_group         *op_grp;
	struct m0_clovis           *instance;
	struct m0_clovis_op_io     *ioo;
	struct m0_clovis_realm      realm;
	struct m0_clovis_entity     entity;
	struct m0_clovis_op_io_ops *ioo_ops;
	struct nw_xfer_ops         *nxr_ops;

	/* Initialise. */
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &entity, instance);
	m0_sm_group_init(&grp);

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_oo.oo_oc.oc_op.op_entity = &entity;
	op_grp = &ioo->ioo_oo.oo_oc.oc_op.op_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);

	M0_UT_ASSERT(ioo->ioo_iomap_nr == 1);
	ioo->ioo_map_idx = 1;
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	ioo->ioo_rc = 0;

	/* Set some mock callbacks */
	M0_ALLOC_PTR(ioo_ops);
	ioo_ops->iro_application_data_copy =
		&ut_clovis_mock_handle_executed_adc;
	ioo_ops->iro_dgmode_read = ut_clovis_mock_ioreq_dgmode_read;
	ioo_ops->iro_dgmode_write = ut_clovis_mock_ioreq_dgmode_write;
	ioo_ops->iro_dgmode_recover = ut_clovis_mock_ioreq_dgmode_recover;
	ioo_ops->iro_parity_verify = ut_clovis_mock_ioreq_parity_verify;
	ioo->ioo_ops = ioo_ops;
	M0_ALLOC_PTR(nxr_ops);
	nxr_ops->nxo_complete = ut_clovis_mock_handle_executed_complete;
	ioo->ioo_nwxfer.nxr_ops = nxr_ops;

	m0_sm_group_lock(op_grp);
	m0_sm_move(&ioo->ioo_oo.oo_oc.oc_op.op_sm, 0, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	/* Set the right state for ioo_sm. */
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READING);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READ_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	m0_sm_group_lock(&grp);
	ioreq_iosm_handle_executed(&grp, &ioo->ioo_ast);
	m0_sm_group_unlock(&grp);

	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_READ_COMPLETE);

	/* ioo->ioo_rc != 0 */
	/* Change the ioo to not use the instance's group. */
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_REQ_COMPLETE);
	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);

	m0_sm_group_lock(&grp);

	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READING);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READ_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	m0_sm_group_lock(op_grp);
	m0_sm_fini(&ioo->ioo_oo.oo_oc.oc_op.op_sm);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_state_set(&ioo->ioo_oo.oo_oc.oc_op.op_sm, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	ioo->ioo_rc = -1;
	ioreq_iosm_handle_executed(&grp, &ioo->ioo_ast);
	m0_sm_group_unlock(&grp);

	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_rc == -1);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);

	ioo->ioo_rc = 0;

	/* dgmode read fails */
	m0_sm_group_lock(&grp);

	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READING);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READ_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);


	m0_sm_group_lock(op_grp);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_state_set(&ioo->ioo_oo.oo_oc.oc_op.op_sm, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	/* Enable FI */
	m0_fi_enable_once("ut_clovis_mock_ioreq_dgmode_read", "ut_mock_dgmode_read_fails");
	ioreq_iosm_handle_executed(&grp, &ioo->ioo_ast);
	m0_sm_group_unlock(&grp);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_rc == -EAGAIN);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);

	/* Parity Verification failed */
	ioo->ioo_rc = 0;
	m0_sm_group_lock(&grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READING);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READ_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	m0_sm_group_lock(op_grp);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_state_set(&ioo->ioo_oo.oo_oc.oc_op.op_sm, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	/* Enable FI */
	m0_fi_enable_once("ut_clovis_mock_ioreq_parity_verify", "ut_mock_parity_verify_fail");
	ioreq_iosm_handle_executed(&grp, &ioo->ioo_ast);
	m0_sm_group_unlock(&grp);
	M0_UT_ASSERT(ioo->ioo_rc == -EINVAL);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);

	/* Application Data copy fails */
	ioo->ioo_rc = 0;
	m0_sm_group_lock(&grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READING);
	m0_sm_state_set(&ioo->ioo_sm, IRS_READ_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);


	m0_sm_group_lock(op_grp);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_state_set(&ioo->ioo_oo.oo_oc.oc_op.op_sm, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	/* Enable FI */
	m0_fi_enable_once("ut_clovis_mock_handle_executed_adc",
					"ut_mock_handle_executed_adc_fails");
	ioreq_iosm_handle_executed(&grp, &ioo->ioo_ast);
	m0_sm_group_unlock(&grp);
	M0_UT_ASSERT(ioo->ioo_rc == -EINVAL);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);

	/* Set ioo for writing */
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_WRITE;
	ioo->ioo_rc = 0;

	m0_sm_group_lock(&grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_WRITING);
	m0_sm_state_set(&ioo->ioo_sm, IRS_WRITE_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);


	m0_sm_group_lock(op_grp);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_state_set(&ioo->ioo_oo.oo_oc.oc_op.op_sm, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	ioreq_iosm_handle_executed(&grp, &ioo->ioo_ast);
	m0_sm_group_unlock(&grp);

	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_WRITE_COMPLETE);

	/* dgmodewrite fails */
	ioo->ioo_rc = 0;
	m0_sm_group_lock(&grp);

	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_state_set(&ioo->ioo_sm, IRS_WRITING);
	m0_sm_state_set(&ioo->ioo_sm, IRS_WRITE_COMPLETE);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	m0_sm_group_lock(op_grp);
	m0_sm_init(&ioo->ioo_oo.oo_oc.oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, op_grp);
	m0_sm_state_set(&ioo->ioo_oo.oo_oc.oc_op.op_sm, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(op_grp);

	 /*Enable FI*/
	m0_fi_enable_once("ut_clovis_mock_ioreq_dgmode_write",
					"ut_mock_dgmode_write_fails");
	ioreq_iosm_handle_executed(&grp, &ioo->ioo_ast);
	m0_sm_group_unlock(&grp);
	M0_UT_ASSERT(ioo->ioo_rc == -EAGAIN);
	M0_UT_ASSERT(ioo->ioo_oo.oo_oc.oc_op.op_sm.sm_state ==
		     M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ioo->ioo_sm.sm_state == IRS_REQ_COMPLETE);

	m0_free(nxr_ops);
	m0_free(ioo_ops);

	m0_clovis_entity_fini(ioo->ioo_oo.oo_oc.oc_op.op_entity);
	m0_sm_group_lock(&grp);
	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &grp);
	ut_clovis_dummy_ioo_delete(ioo, instance);
	m0_sm_group_unlock(&grp);

	/* Finalise clovis. */
	m0_sm_group_fini(&grp);
}

/**
 * Tests ioreq_iomaps_destroy().
 */
static void ut_clovis_test_ioreq_iomaps_destroy(void)
{
	struct m0_clovis       *instance;
	struct m0_clovis_op_io *ioo;

	/* initialise clovis */
	instance = dummy_instance;

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);

	ioreq_iomaps_destroy(ioo);

	M0_UT_ASSERT(ioo->ioo_iomaps == NULL);
	M0_UT_ASSERT(ioo->ioo_iomap_nr == 0);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_ioreq_iomaps_prepare(void)
{
}

/**
 * Tests data_buf_copy().
 */
static void ut_clovis_test_data_buf_copy(void)
{
	int                     i;
	uint64_t                rc;
	struct m0_bufvec        app_data;
	struct m0_bufvec_cursor app_datacur;
	struct data_buf         clovis_data;

	/* With some fake buffers to read/write into */
	rc = m0_bufvec_alloc(&app_data, 1, UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(rc == 0);

	/* and its cursor... */
	m0_bufvec_cursor_init(&app_datacur, &app_data);

	/* Build a fake data buf */
	ut_clovis_dummy_data_buf_init(&clovis_data);

	/* base case */
	rc = data_buf_copy(&clovis_data, &app_datacur, CD_COPY_FROM_APP);
	M0_UT_ASSERT(rc == UT_DEFAULT_BLOCK_SIZE);

	/* Reset the cursor */
	m0_bufvec_cursor_init(&app_datacur, &app_data);

	/* Check data is copied */
	memset(app_data.ov_buf[0], 0, app_data.ov_vec.v_count[0]);
	memset(clovis_data.db_buf.b_addr, '!',
	       clovis_data.db_buf.b_nob);
	rc = data_buf_copy(&clovis_data, &app_datacur, CD_COPY_TO_APP);
	M0_UT_ASSERT(rc == UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(memcmp(app_data.ov_buf[0],
			    clovis_data.db_buf.b_addr,
			    app_data.ov_vec.v_count[0]) == 0);

	/* Reset the cursor */
	m0_bufvec_cursor_init(&app_datacur, &app_data);

	/* and the other way... */
	memset(clovis_data.db_buf.b_addr, 0, clovis_data.db_buf.b_nob);
	rc = data_buf_copy(&clovis_data, &app_datacur, CD_COPY_FROM_APP);
	M0_UT_ASSERT(rc == UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(memcmp(app_data.ov_buf[0],
			    clovis_data.db_buf.b_addr,
			    app_data.ov_vec.v_count[0]) == 0);

	m0_bufvec_free(&app_data);

	/* Use a fragmented application buffer */
	rc = m0_bufvec_alloc(&app_data, 8, UT_DEFAULT_BLOCK_SIZE/8);
	M0_UT_ASSERT(rc == 0);

	/* initialise the cursor... */
	m0_bufvec_cursor_init(&app_datacur, &app_data);

	memset(clovis_data.db_buf.b_addr, '?', clovis_data.db_buf.b_nob);
	rc = data_buf_copy(&clovis_data, &app_datacur,
			   CD_COPY_TO_APP);
	M0_UT_ASSERT(rc == UT_DEFAULT_BLOCK_SIZE);
	for (i = 0; i < app_data.ov_vec.v_nr; i++)
		M0_UT_ASSERT(memcmp(app_data.ov_buf[i],
				    clovis_data.db_buf.b_addr,
				    app_data.ov_vec.v_count[i]) == 0);

	/* Reset the cursor */
	m0_bufvec_cursor_init(&app_datacur, &app_data);

	/* base case */
	rc = data_buf_copy(&clovis_data, &app_datacur, CD_COPY_FROM_APP);
	M0_UT_ASSERT(rc == UT_DEFAULT_BLOCK_SIZE);

	m0_bufvec_free(&app_data);
	ut_clovis_dummy_data_buf_fini(&clovis_data);
}

/**
 * Tests clovis_application_data_copy().
 */
static void ut_clovis_test_clovis_application_data_copy(void)
{
	int                             rc;
	struct m0_clovis_obj           *obj;
	struct pargrp_iomap            *map;
	struct m0_clovis               *instance;
	struct m0_bufvec                data;
	struct m0_bufvec_cursor         datacur;

	/* With some fake buffers to read/write into */
	rc = m0_bufvec_alloc(&data, 1, UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(rc == 0);

	/* and its cursor... */
	m0_bufvec_cursor_init(&datacur, &data);

	/* init clovis */
	instance = dummy_instance;

	/* Generate a fake map/ioo etc */
	map = ut_clovis_dummy_pargrp_iomap_create(instance, 1);
	map->pi_ioo = ut_clovis_dummy_ioo_create(instance, 1);
	/* Allocate and initialise our fake object */
	obj = map->pi_ioo->ioo_obj;
	obj->ob_attr.oa_bshift = CLOVIS_DEFAULT_BUF_SHIFT;

	/* Initialise an index vector for the map */
	map->pi_ivec.iv_vec.v_count[0] = UT_DEFAULT_BLOCK_SIZE;
	map->pi_ivec.iv_index[0] = 0;

	/* base case */
	clovis_application_data_copy(map, obj, 0,
				     UT_DEFAULT_BLOCK_SIZE, &datacur,
				     CD_COPY_TO_APP, PA_DATA);

	/* Re-initialise veccursor */
	m0_bufvec_cursor_init(&datacur, &data);

	/* base case */
	clovis_application_data_copy(map, obj, 0,
				     UT_DEFAULT_BLOCK_SIZE, &datacur,
				     CD_COPY_FROM_APP, PA_DATA);
	/* Reset veccursor */
	m0_bufvec_cursor_init(&datacur, &data);

	/* XXX IN(filter, (...))? */
	/* XXX filter isn't used in the READ path, wait until WRITE is
	 * implemented */

	/* Check data is copied */
	memset(data.ov_buf[0], 0, data.ov_vec.v_count[0]);
	memset(map->pi_databufs[0][0]->db_buf.b_addr, '!',
	       map->pi_databufs[0][0]->db_buf.b_nob);

	clovis_application_data_copy(map, obj, 0,
				     UT_DEFAULT_BLOCK_SIZE, &datacur,
				     CD_COPY_TO_APP, PA_DATA);

	M0_UT_ASSERT(memcmp(data.ov_buf[0],
		     map->pi_databufs[0][0]->db_buf.b_addr,
		     data.ov_vec.v_count[0]) == 0);

	m0_bufvec_free(&data);

	/* Check no assert-failures for zero-copy */
	M0_ALLOC_ARR(data.ov_buf, 1);
	M0_UT_ASSERT(data.ov_buf != NULL);
	data.ov_buf[0] = map->pi_databufs[0][0]->db_buf.b_addr;
	data.ov_vec.v_nr = 1;
	M0_ALLOC_ARR(data.ov_vec.v_count, 1);
	M0_UT_ASSERT(data.ov_vec.v_count != NULL);
	data.ov_vec.v_count[0] = map->pi_databufs[0][0]->db_buf.b_nob;

	/* Re-initialise veccursor */
	m0_bufvec_cursor_init(&datacur, &data);

	clovis_application_data_copy(map, obj, 0,
				     UT_DEFAULT_BLOCK_SIZE, &datacur,
				     CD_COPY_TO_APP, PA_DATA);

	/* 'free' our fake buf-vec */
	data.ov_vec.v_nr = 0;
	m0_free(data.ov_vec.v_count);
	data.ov_vec.v_count = 0;
	m0_free(data.ov_buf);
	data.ov_buf = NULL;

	/* allocate a real one */
	rc = m0_bufvec_alloc(&data, 1, UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(rc == 0);

	/* Re-initialise veccursor */
	m0_bufvec_cursor_init(&datacur, &data);

	/* base case */
	clovis_application_data_copy(map, obj, 0,
				     UT_DEFAULT_BLOCK_SIZE, &datacur,
				     CD_COPY_TO_APP, PA_DATA);

	/* Re-initialise veccursor */
	m0_bufvec_cursor_init(&datacur, &data);

	/* base case */
	clovis_application_data_copy(map, obj, 0,
				     UT_DEFAULT_BLOCK_SIZE, &datacur,
				     CD_COPY_FROM_APP, PA_DATA);

	m0_bufvec_free(&data);
	ut_clovis_dummy_ioo_delete(map->pi_ioo, instance);
	ut_clovis_dummy_pargrp_iomap_delete(map, instance);
}

static void ut_clovis_test_ioreq_parity_recalc(void)
{
	int                     rc;
	struct pargrp_iomap    *map;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance;
	struct m0_clovis_realm  realm;

	/* Create a dummy ioo */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_obj->ob_entity.en_realm = &realm;
	realm.re_instance = instance;

	/* Set the dummy map. */
	map = ioo->ioo_iomaps[0];
	M0_UT_ASSERT(map != NULL);
	map->pi_ioo = ioo;
	map->pi_ops = &mock_iomap_ops;
	map->pi_grpid = 0;

	ut_clovis_dummy_paritybufs_create(map, true);

	m0_semaphore_init(&clovis_cpus_sem, 1);

	/*
 	 * Test 1. map->pi_rtype == PIR_NONE (normal case)
 	 */
	map->pi_ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_WRITE;
	map->pi_rtype = PIR_READREST;
	rc = ioreq_parity_recalc(ioo);
	M0_UT_ASSERT(rc == 0);

	/* Test 2. Read rest method */
	map->pi_rtype = PIR_READREST;
	rc = ioreq_parity_recalc(ioo);
	M0_UT_ASSERT(rc == 0);

	/* Test 3. Read old method */
	map->pi_rtype = PIR_READOLD;
	rc = ioreq_parity_recalc(ioo);
	M0_UT_ASSERT(rc == 0);

	/* free parity bufs*/
	ut_clovis_dummy_paritybufs_delete(map, true);

	/* Clean up */
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_ioreq_application_data_copy(void)
{
	int                     i;
	int                     j;
	int                     k;
	int                     rc;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis_obj   *obj;
	struct m0_clovis       *instance;
	struct m0_bufvec        stashed;

	/* init clovis */
	instance = dummy_instance;

	/* Generate a fake map/ioo etc */
	ioo = ut_clovis_dummy_ioo_create(instance, 6 / CLOVIS_M0T1FS_LAYOUT_N);
	for (i = 0; i < ioo->ioo_iomap_nr; i++) {
		ioo->ioo_iomaps[i]->pi_ivec.iv_index[0] =
				i * CLOVIS_M0T1FS_LAYOUT_N * UT_DEFAULT_BLOCK_SIZE;
	}

	/* Initialise our fake object */
	obj = ioo->ioo_obj;
	obj->ob_attr.oa_bshift = CLOVIS_DEFAULT_BUF_SHIFT;

	/* With some fake buffers to read/write into */
	stashed = ioo->ioo_data;
	rc = m0_bufvec_alloc(&ioo->ioo_data, 6, UT_DEFAULT_BLOCK_SIZE);
	M0_UT_ASSERT(rc == 0);

	/* extents and buffers must be the same size */
	ioo->ioo_ext.iv_index[0] = 0;
	ioo->ioo_ext.iv_vec.v_count[0] = 6 * UT_DEFAULT_BLOCK_SIZE;

	/* base case */
	rc = ioreq_application_data_copy(ioo,
					 CD_COPY_FROM_APP,
					 PA_NONE);
	M0_UT_ASSERT(rc == 0);

	ioo->ioo_iomaps[0]->pi_ioo = ioo;

	/* Check multiple blocks of data are copied */
	for (i = 0; i < ioo->ioo_data.ov_vec.v_nr; i++)
		memset(ioo->ioo_data.ov_buf[i], 0, ioo->ioo_data.ov_vec.v_count[i]);

	for (k = 0; k < ioo->ioo_iomap_nr; k++) {
		struct pargrp_iomap *map = ioo->ioo_iomaps[k];

		for (i = 0; i < map->pi_max_row; i++) {
			for (j = 0; j < map->pi_max_col; j++) {
				memset(map->pi_databufs[i][j]->db_buf.b_addr,
				       '!',
				       map->pi_databufs[i][j]->db_buf.b_nob);
			}
		}
	}

	rc = ioreq_application_data_copy(ioo,
					 CD_COPY_TO_APP,
				 	 PA_NONE);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < ioo->ioo_data.ov_vec.v_nr; i++) {
		struct pargrp_iomap *map = ioo->ioo_iomaps[0]; /* always the same */

		M0_UT_ASSERT(ioo->ioo_data.ov_vec.v_count[i] <=
			     map->pi_databufs[0][0]->db_buf.b_nob);
		M0_UT_ASSERT(memcmp(ioo->ioo_data.ov_buf[i],
				    map->pi_databufs[0][0]->db_buf.b_addr,
				    ioo->ioo_data.ov_vec.v_count[i]) == 0);
	}

	/* base case */
	rc = ioreq_application_data_copy(ioo,
					 CD_COPY_FROM_APP,
					 PA_NONE);
	M0_UT_ASSERT(rc == 0);

	m0_bufvec_free(&ioo->ioo_data);
	ioo->ioo_data = stashed;
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_device_check(void)
{
	int                     rc;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance;
	struct m0_clovis_realm  realm;
	struct nw_xfer_request *xfer;
	struct target_ioreq    *ti;
	struct m0_fid           fid;

	/* init */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ut_clovis_realm_entity_setup(&realm, ioo->ioo_oo.oo_oc.oc_op.op_entity,
				     instance);
	ut_clovis_dummy_poolmach_create(instance->m0c_pools_common.pc_cur_pver);
	ioo->ioo_sm.sm_state = IRS_READ_COMPLETE;

	/* Base cases. */
	xfer = &ioo->ioo_nwxfer;
	tioreqht_htable_init(&xfer->nxr_tioreqs_hash, 1);
	ti = ut_clovis_dummy_target_ioreq_create();
	m0_fid_gob_make(&fid, 0, 1);
	m0_fid_convert_gob2cob(&fid, &ti->ti_fid, 0);
	tioreqht_htable_add(&xfer->nxr_tioreqs_hash, ti);

	ut_clovis_set_device_state(
			&instance->m0c_pools_common.pc_cur_pver->pv_mach,
			0, M0_PNDS_FAILED);

	rc = device_check(ioo);
	M0_UT_ASSERT(rc == 1);

	tioreqht_htable_del(&xfer->nxr_tioreqs_hash, ti);
	ut_clovis_dummy_target_ioreq_delete(ti);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);

	/* fini */
	ut_clovis_dummy_ioo_delete(ioo, instance);
	ut_clovis_dummy_poolmach_delete(instance->m0c_pools_common.pc_cur_pver);
}

/**
 * Helper callback for ut_clovis_test_clovis_obj_io_cb_launch().
 */
static int
ut_clovis_mock_io_launch_distribute(struct nw_xfer_request  *xfer)
{
	return 0;
}

static void ut_clovis_test_ioreq_dgmode_read(void)
{
	int                     rc;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance;
	struct nw_xfer_ops     *nxr_ops;
	struct m0_clovis_realm  realm;
	struct nw_xfer_request *xfer;
	struct target_ioreq    *ti;
	struct m0_fid           fid;

	/* Init. */
	instance = dummy_instance;

	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_map_idx = 1;
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	ut_clovis_realm_entity_setup(&realm,
		ioo->ioo_oo.oo_oc.oc_op.op_entity, instance);
	ut_clovis_dummy_poolmach_create(instance->m0c_pools_common.pc_cur_pver);

	xfer = &ioo->ioo_nwxfer;
	tioreqht_htable_init(&xfer->nxr_tioreqs_hash, 1);
	ti = ut_clovis_dummy_target_ioreq_create();
	iofops_tlist_init(&ti->ti_iofops);
	m0_fid_gob_make(&fid, 0, 1);
	m0_fid_convert_gob2cob(&fid, &ti->ti_fid, 0);
	tioreqht_htable_add(&xfer->nxr_tioreqs_hash, ti);

	M0_ALLOC_PTR(nxr_ops);
	nxr_ops->nxo_complete = ut_clovis_mock_handle_executed_complete;
	nxr_ops->nxo_distribute = &ut_clovis_mock_io_launch_distribute;
	nxr_ops->nxo_dispatch = &ut_clovis_mock_handle_launch_dispatch;
	xfer->nxr_ops = nxr_ops;

	ut_clovis_set_device_state(
		&instance->m0c_pools_common.pc_cur_pver->pv_mach,
		0, M0_PNDS_FAILED);

	rc = ioreq_dgmode_read(ioo, 0);
	M0_UT_ASSERT(rc == 0);

	tioreqht_htable_del(&xfer->nxr_tioreqs_hash, ti);
	ut_clovis_dummy_target_ioreq_delete(ti);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);

	m0_free(nxr_ops);

	/* Fini. */
	ut_clovis_dummy_ioo_delete(ioo, instance);
	ut_clovis_dummy_poolmach_delete(instance->m0c_pools_common.pc_cur_pver);
}

static void ut_clovis_test_ioreq_dgmode_write(void)
{
	int                     rc;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance;
	struct nw_xfer_ops     *nxr_ops;
	struct m0_clovis_realm  realm;
	struct nw_xfer_request *xfer;
	struct target_ioreq    *ti;
	struct m0_fid           fid;

	/* Init. */
	instance = dummy_instance;

	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_map_idx = 1;
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	ut_clovis_realm_entity_setup(&realm,
		ioo->ioo_oo.oo_oc.oc_op.op_entity, instance);
	ut_clovis_dummy_poolmach_create(
		instance->m0c_pools_common.pc_cur_pver);

	xfer = &ioo->ioo_nwxfer;
	tioreqht_htable_init(&xfer->nxr_tioreqs_hash, 1);
	ti = ut_clovis_dummy_target_ioreq_create();
	iofops_tlist_init(&ti->ti_iofops);
	m0_fid_gob_make(&fid, 0, 1);
	m0_fid_convert_gob2cob(&fid, &ti->ti_fid, 0);
	tioreqht_htable_add(&xfer->nxr_tioreqs_hash, ti);

	M0_ALLOC_PTR(nxr_ops);
	nxr_ops->nxo_complete = ut_clovis_mock_handle_executed_complete;
	nxr_ops->nxo_distribute = &ut_clovis_mock_io_launch_distribute;
	nxr_ops->nxo_dispatch = &ut_clovis_mock_handle_launch_dispatch;
	xfer->nxr_ops = nxr_ops;

	ut_clovis_set_device_state(
		&instance->m0c_pools_common.pc_cur_pver->pv_mach,
		0, M0_PNDS_FAILED);

	rc = ioreq_dgmode_write(ioo, 0);
	M0_UT_ASSERT(rc == 0);

	tioreqht_htable_del(&xfer->nxr_tioreqs_hash, ti);
	ut_clovis_dummy_target_ioreq_delete(ti);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);

	m0_free(nxr_ops);

	/* Fini. */
	ut_clovis_dummy_ioo_delete(ioo, instance);
	ut_clovis_dummy_poolmach_delete(instance->m0c_pools_common.pc_cur_pver);
}

static void ut_clovis_test_ioreq_dgmode_recover(void)
{
	int                     i;
	int                     rc;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance;
	struct m0_clovis_realm  realm;

	/* Init. */
	instance = dummy_instance;

	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ioo->ioo_map_idx = 1;
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	ut_clovis_realm_entity_setup(&realm,
		ioo->ioo_oo.oo_oc.oc_op.op_entity, instance);
	ut_clovis_dummy_poolmach_create(
		instance->m0c_pools_common.pc_cur_pver);

	/* base case */
	ioo->ioo_sm.sm_state = IRS_READ_COMPLETE;
	for (i = 0; i < ioo->ioo_iomap_nr; i++)
		ioo->ioo_iomaps[i]->pi_ops = &mock_iomap_ops;

	rc = ioreq_dgmode_recover(ioo);
	M0_UT_ASSERT(rc == 0);

	/* Fini. */
	ut_clovis_dummy_ioo_delete(ioo, instance);
	ut_clovis_dummy_poolmach_delete(instance->m0c_pools_common.pc_cur_pver);
}

M0_INTERNAL int ut_clovis_io_req_init(void)
{
	int                       rc;
	struct m0_pdclust_layout *dummy_pdclust_layout;

#ifndef __KERNEL__
	ut_clovis_shuffle_test_order(&ut_suite_clovis_io_req);
#endif

	m0_fi_enable("m0_clovis_op_stable", "skip_ongoing_io_ref");

	m0_clovis_init_io_op();

	rc = ut_m0_clovis_init(&dummy_instance);
	M0_UT_ASSERT(rc == 0);

	ut_clovis_layout_domain_fill(dummy_instance);
	dummy_pdclust_layout =
		ut_clovis_dummy_pdclust_layout_create(dummy_instance);
	M0_UT_ASSERT(dummy_pdclust_layout != NULL);

	return 0;
}

M0_INTERNAL int ut_clovis_io_req_fini(void)
{
	//ut_clovis_dummy_pdclust_layout_delete(dummy_pdclust_layout,
	//				      dummy_instance);
	ut_clovis_layout_domain_empty(dummy_instance);
	ut_m0_clovis_fini(&dummy_instance);

	m0_fi_disable("m0_clovis_op_stable", "skip_ongoing_io_ref");

	return 0;
}

struct m0_ut_suite ut_suite_clovis_io_req = {
	.ts_name = "clovis-io-req-ut",
	.ts_init = ut_clovis_io_req_init,
	.ts_fini = ut_clovis_io_req_fini,
	.ts_tests = {
		{ "data_buf_copy",
				    &ut_clovis_test_data_buf_copy},
		{ "ioreq_sm_state",
				    &ut_clovis_test_ioreq_sm_state},
		{ "ioreq_sm_state_set_locked",
				    &ut_clovis_test_ioreq_sm_state_set_locked},
		{ "ioreq_sm_failed_locked",
				    &ut_clovis_test_ioreq_sm_failed_locked},
		{ "ioreq_iosm_handle_launch",
				    &ut_clovis_test_ioreq_iosm_handle_launch},
		{ "ioreq_iosm_handle_executed",
				    &ut_clovis_test_ioreq_iosm_handle_executed},
		{ "ioreq_iomaps_destroy",
				    &ut_clovis_test_ioreq_iomaps_destroy},
		{ "ioreq_iomaps_prepare",
				    &ut_clovis_test_ioreq_iomaps_prepare},
		{ "clovis_application_data_copy",
				    &ut_clovis_test_clovis_application_data_copy},
		{ "ioreq_application_data_copy",
				    &ut_clovis_test_ioreq_application_data_copy},
		{ "ioreq_parity_recalc",
				    &ut_clovis_test_ioreq_parity_recalc},
		{ "device_check",
				    &ut_clovis_test_device_check},
		{ "ioreq_dgmode_recover",
				    &ut_clovis_test_ioreq_dgmode_recover},
		{ "ioreq_dgmode_read",
				    &ut_clovis_test_ioreq_dgmode_read},
		{ "ioreq_dgmode_write",
				    &ut_clovis_test_ioreq_dgmode_write},
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
