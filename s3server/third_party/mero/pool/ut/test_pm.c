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
 * Original author: Huang Hua <Hua_Huang@xyratex.com>
 * Original creation date: 08/15/2012
 */

#include "ut/ut.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "pool/pool.h"
#include "cob/cob.h"
#include "ut/be.h"
#include "be/ut/helper.h"
#include "ha/note.h"         /* m0_ha_nvec */

#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_POOL
#include "lib/trace.h"      /* M0_LOG */
#include "lib/finject.h"    /* m0_fi_enable_off_n_on_m() m0_fi_disable() */

enum {
	PM_TEST_DEFAULT_DEVICE_NUMBER      = 10,
	PM_TEST_DEFAULT_NODE_NUMBER        = 1,
	PM_TEST_DEFAULT_MAX_DEVICE_FAILURE = 1,
	PM_TEST_DEFAULT_MAX_NODE_FAILURE   = 1
};

static struct m0_fid M0_POOL_ID = M0_FID_TINIT('o', 1, 23);
static struct m0_fid M0_PVER_ID = M0_FID_TINIT('v', 1, 24);
static struct m0_pool            pool;
static struct m0_pool_version    pver;

static int pool_pver_init(uint32_t N, uint32_t K)
{
	int rc;

	M0_SET0(&pool);
	m0_pool_init(&pool, &M0_POOL_ID, 0);
	M0_SET0(&pver);
	rc = m0_pool_version_init(&pver, &M0_PVER_ID, &pool,
				  PM_TEST_DEFAULT_DEVICE_NUMBER,
				  PM_TEST_DEFAULT_NODE_NUMBER, N, K);

	return rc;
}

static void pool_pver_fini(void)
{
	m0_fi_enable_once("m0_poolmach_fini", "poolmach_init_by_conf_skipped");
	m0_pool_version_fini(&pver);
	m0_pool_fini(&pool);
}

static void pm_test_init_fini(void)
{
	int rc;

	rc = pool_pver_init(8, PM_TEST_DEFAULT_MAX_DEVICE_FAILURE);
	M0_UT_ASSERT(rc == 0);
	pool_pver_fini();
}

static void pm_test_transit(void)
{
	struct m0_poolmach            *pm;
	enum m0_pool_nd_state          state;
	int                            rc;
	struct m0_poolmach_event       events[4];
	struct m0_poolmach_event       e_invalid;
	struct m0_poolmach_event       e_valid;
	struct m0_ha_nvec              nvec;

	M0_SET0(&nvec);
	rc = pool_pver_init(8, PM_TEST_DEFAULT_MAX_DEVICE_FAILURE);
	M0_UT_ASSERT(rc == 0);
	pm = &pver.pv_mach;
	m0_poolmach_failvec_apply(pm, &nvec);

	events[0].pe_type  = M0_POOL_DEVICE;
	events[0].pe_index = 1;
	events[0].pe_state = M0_PNDS_FAILED;
	rc = m0_poolmach_state_transit(pm, &events[0]);
	M0_UT_ASSERT(rc == 0);

	events[1].pe_type  = M0_POOL_DEVICE;
	events[1].pe_index = 3;
	events[1].pe_state = M0_PNDS_OFFLINE;
	rc = m0_poolmach_state_transit(pm, &events[1]);
	M0_UT_ASSERT(rc == 0);

	events[2].pe_type  = M0_POOL_DEVICE;
	events[2].pe_index = 3;
	events[2].pe_state = M0_PNDS_ONLINE;
	rc = m0_poolmach_state_transit(pm, &events[2]);
	M0_UT_ASSERT(rc == 0);

	events[3].pe_type  = M0_POOL_NODE;
	events[3].pe_index = 0;
	events[3].pe_state = M0_PNDS_OFFLINE;
	rc = m0_poolmach_state_transit(pm, &events[3]);
	M0_UT_ASSERT(rc == 0);

	/* invalid event. case 1: invalid type*/
	e_invalid.pe_type  = M0_POOL_NODE + 5;
	e_invalid.pe_index = 0;
	e_invalid.pe_state = M0_PNDS_OFFLINE;
	rc = m0_poolmach_state_transit(pm, &e_invalid);
	M0_UT_ASSERT(rc == -EINVAL);

	/* invalid event. case 2: invalid index */
	e_invalid.pe_type  = M0_POOL_NODE;
	e_invalid.pe_index = 100;
	e_invalid.pe_state = M0_PNDS_OFFLINE;
	rc = m0_poolmach_state_transit(pm, &e_invalid);
	M0_UT_ASSERT(rc == -EINVAL);

	/* invalid event. case 3: invalid state */
	e_invalid.pe_type  = M0_POOL_NODE;
	e_invalid.pe_index = 0;
	e_invalid.pe_state = M0_PNDS_SNS_REBALANCING + 1;
	rc = m0_poolmach_state_transit(pm, &e_invalid);
	M0_UT_ASSERT(rc == -EINVAL);

	/* invalid event. case 4: invalid state */
	e_invalid.pe_type  = M0_POOL_DEVICE;
	e_invalid.pe_index = 0;
	e_invalid.pe_state = M0_PNDS_NR;
	rc = m0_poolmach_state_transit(pm, &e_invalid);
	M0_UT_ASSERT(rc == -EINVAL);

	/*  Test transition from M0_PNDS_OFFLINE to M0_PNDS_FAILED. */
	rc = m0_poolmach_device_state(pm, 0, &state);
	M0_UT_ASSERT(rc == 0 && state == M0_PNDS_UNKNOWN);

	e_valid.pe_type  = M0_POOL_DEVICE;
	e_valid.pe_index = 0;
	e_valid.pe_state = M0_PNDS_OFFLINE;
	rc = m0_poolmach_state_transit(pm, &e_valid);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 0, &state);
	M0_UT_ASSERT(rc == 0 && state == M0_PNDS_OFFLINE);

	e_valid.pe_type  = M0_POOL_DEVICE;
	e_valid.pe_index = 0;
	e_valid.pe_state = M0_PNDS_FAILED;
	rc = m0_poolmach_state_transit(pm, &e_valid);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 0, &state);
	M0_UT_ASSERT(rc == 0 && state == M0_PNDS_FAILED);

	m0_fi_enable_off_n_on_m("m0_pooldev_clink_del",
			  "do_nothing_for_poolmach-ut", 0,
			  (PM_TEST_DEFAULT_DEVICE_NUMBER + 1));
	/* Destroy poolmach persistent storage. We will have some different
	 * poolmach parameters in next test case.
	 */
	m0_fi_disable("m0_pooldev_clink_del", "do_nothing_for_poolmach-ut");
	/* finally */
	pool_pver_fini();
}

static void pm_test_spare_slot(void)
{
	struct m0_poolmach       *pm;
	struct m0_ha_nvec         nvec;
	int                       rc = 0;
	struct m0_poolmach_event  event;
	enum m0_pool_nd_state     state_out;
	enum m0_pool_nd_state     target_state;
	enum m0_pool_nd_state     state;
	uint32_t                  spare_slot;

	rc = pool_pver_init(6, 2);
	M0_UT_ASSERT(rc == 0);
	pm = &pver.pv_mach;

	M0_SET0(&nvec);
	m0_poolmach_failvec_apply(pm, &nvec);
	event.pe_type  = M0_POOL_DEVICE;
	event.pe_index = 1;


	/* ONLINE */
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);
	/* FAILED */
	target_state = M0_PNDS_FAILED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);

	rc = m0_poolmach_device_state(pm, PM_TEST_DEFAULT_DEVICE_NUMBER - 1,
					&state_out);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, PM_TEST_DEFAULT_DEVICE_NUMBER,
					&state_out);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = m0_poolmach_device_state(pm, 100, &state_out);
	M0_UT_ASSERT(rc == -EINVAL);

	for (state = M0_PNDS_ONLINE; state < M0_PNDS_NR; state++) {
		if (state == M0_PNDS_SNS_REPAIRING || state == M0_PNDS_FAILED)
			continue;
		/*
		 * transition to other state other than the above two states is
		 * invalid
		 */
		event.pe_state = state;
		rc = m0_poolmach_state_transit(pm, &event);
		M0_UT_ASSERT(rc == -EINVAL);
	}

	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);


	/* transit to SNS_REPAIRING */
	target_state = M0_PNDS_SNS_REPAIRING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);
	/* the first spare slot is used by device 1 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 0);
	/* no spare slot is used by device 2 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 2, &spare_slot);
	M0_UT_ASSERT(rc == -ENOENT);
	for (state = M0_PNDS_ONLINE; state < M0_PNDS_NR; state++) {
		if (state == M0_PNDS_SNS_REPAIRED ||
		    state == M0_PNDS_FAILED ||
		    state == M0_PNDS_SNS_REPAIRING)
			continue;
		/* transit to other state other than the above one is invalid */
		event.pe_state = state;
		rc = m0_poolmach_state_transit(pm, &event);
		M0_UT_ASSERT(rc == -EINVAL);
	}


	/* transit to SNS_REPAIRED */
	target_state = M0_PNDS_SNS_REPAIRED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);
	/* the first spare slot is used by device 1 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 0);
	/* no spare slot is used by device 2 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 2, &spare_slot);
	M0_UT_ASSERT(rc == -ENOENT);
	for (state = M0_PNDS_ONLINE; state < M0_PNDS_NR; state++) {
		if (state == M0_PNDS_SNS_REBALANCING ||
		    state == M0_PNDS_SNS_REPAIRED)
			continue;
		/* transit to other state other than the above one is invalid */
		event.pe_state = state;
		rc = m0_poolmach_state_transit(pm, &event);
		M0_UT_ASSERT(rc == -EINVAL);
	}


	/* transit to SNS_REBALANCING */
	target_state = M0_PNDS_SNS_REBALANCING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);
	/* the first spare slot is used by device 1 */
	rc = m0_poolmach_sns_rebalance_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 0);
	for (state = M0_PNDS_ONLINE; state < M0_PNDS_NR; state++) {
		if (state == M0_PNDS_ONLINE ||
		    state == M0_PNDS_SNS_REBALANCING ||
		    state == M0_PNDS_SNS_REPAIRED ||
		    state == M0_PNDS_FAILED)
			continue;
		/* transit to other state other than the above one is invalid */
		event.pe_state = state;
		rc = m0_poolmach_state_transit(pm, &event);
		M0_UT_ASSERT(rc == -EINVAL);
	}

	/* transit to ONLINE */
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);
	/* the first spare slot is not used any more */
	rc = m0_poolmach_sns_repair_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == -ENOENT);

	m0_fi_enable_off_n_on_m("m0_pooldev_clink_del",
			  "do_nothing_for_poolmach-ut", 0,
			  (PM_TEST_DEFAULT_DEVICE_NUMBER + 1));
	/* Destroy poolmach persistent storage. We will have some different
	 * poolmach parameters in next test case.
	 */
	m0_fi_disable("m0_pooldev_clink_del", "do_nothing_for_poolmach-ut");
	/* finally */
	pool_pver_fini();
}

static void pm_test_multi_fail(void)
{
	struct m0_poolmach       *pm;
	struct m0_poolmach_event  event;
	struct m0_ha_nvec         nvec;
	enum m0_pool_nd_state     state_out;
	enum m0_pool_nd_state     target_state;
	uint32_t                  spare_slot;
	int                       rc;

	rc = pool_pver_init(4, 3);
	M0_UT_ASSERT(rc == 0);
	pm = &pver.pv_mach;
	M0_SET0(&nvec);
	m0_poolmach_failvec_apply(pm, &nvec);

	event.pe_type  = M0_POOL_DEVICE;

	/* device 1 ONLINE */
	event.pe_index = 1;
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);

	/* device 1 FAILED */
	event.pe_index = 1;
	target_state = M0_PNDS_FAILED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 1, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);

	/* device 2 ONLINE */
	event.pe_index = 2;
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 2, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);

	/* device 2 FAILED */
	event.pe_index = 2;
	target_state = M0_PNDS_FAILED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_device_state(pm, 2, &state_out);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(state_out == target_state);

	/* transit device 1 to SNS_REPAIRING */
	event.pe_index = 1;
	target_state = M0_PNDS_SNS_REPAIRING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	/* the first spare slot is used by device 1 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 0);

	/* transit device 2 to SNS_REPAIRING */
	event.pe_index = 2;
	target_state = M0_PNDS_SNS_REPAIRING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	/* the second spare slot is used by device 2 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 2, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 1);


	/* transit device 1 to SNS_REPAIRED */
	event.pe_index = 1;
	target_state = M0_PNDS_SNS_REPAIRED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	/* the first spare slot is used by device 1 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 0);

	/* transit device 2 to SNS_REPAIRED */
	event.pe_index = 2;
	target_state = M0_PNDS_SNS_REPAIRED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	/* the second spare slot is used by device 2 */
	rc = m0_poolmach_sns_repair_spare_query(pm, 2, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 1);


	/* transit device 1 to SNS_REBALANCING */
	event.pe_index = 1;
	target_state = M0_PNDS_SNS_REBALANCING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	/* the first spare slot is used by device 1 */
	rc = m0_poolmach_sns_rebalance_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 0);

	/* transit device 2 to SNS_REBALANCING */
	event.pe_index = 2;
	target_state = M0_PNDS_SNS_REBALANCING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	/* the second spare slot is used by device 2 */
	rc = m0_poolmach_sns_rebalance_spare_query(pm, 2, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 1);


	/* transit device 2 to ONLINE */
	event.pe_index = 2;
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_sns_repair_spare_query(pm, 2, &spare_slot);
	M0_UT_ASSERT(rc == -ENOENT);

	/* transit device 3 to ONLINE */
	event.pe_index = 3;
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);

	/* transit device 3 to FAILED */
	event.pe_index = 3;
	target_state = M0_PNDS_FAILED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	target_state = M0_PNDS_SNS_REPAIRING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_sns_repair_spare_query(pm, 3, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 1);

	/* transit device 1 to ONLINE */
	event.pe_index = 1;
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_sns_repair_spare_query(pm, 1, &spare_slot);
	M0_UT_ASSERT(rc == -ENOENT);

	/* transit device 4 to ONLINE */
	event.pe_index = 4;
	target_state = M0_PNDS_ONLINE;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);

	/* transit device 4 to FAILED */
	event.pe_index = 4;
	target_state = M0_PNDS_FAILED;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	target_state = M0_PNDS_SNS_REPAIRING;
	event.pe_state = target_state;
	rc = m0_poolmach_state_transit(pm, &event);
	M0_UT_ASSERT(rc == 0);
	rc = m0_poolmach_sns_repair_spare_query(pm, 4, &spare_slot);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spare_slot == 0);

	/* We will keep the poolmach in persistent storage. It will be loaded
	 * in next test case.
	 */
	/* finally */
	pool_pver_fini();
}

struct m0_ut_suite poolmach_ut = {
	.ts_name = "poolmach-ut",
	.ts_tests = {
		{ "pm_test init & fini",   pm_test_init_fini                  },
		{ "pm_test state transit", pm_test_transit                    },
		{ "pm_test spare slot",    pm_test_spare_slot                 },
		{ "pm_test multi fail",    pm_test_multi_fail                 },
		{ NULL,                    NULL                               }
	}
};
M0_EXPORTED(poolmach_ut);
#undef M0_TRACE_SUBSYSTEM
