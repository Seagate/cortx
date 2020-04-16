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
 * Original creation date: 15-Sep-2014
 */


#include "ut/ut.h"   /* M0_UT_ASSERT */
#include "clovis/ut/clovis.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"

/*
 * Including the c files so we can replace the M0_CLOVIS_PRE asserts
 * in order to test them.
 */
#include "clovis/clovis.c"
#include "clovis/clovis_init.c"

#include "lib/ub.h"
#include "lib/errno.h"    /* ETIMEDOUT */
#include "lib/timer.h"    /* m0_timer_init */
#include "lib/finject.h"  /* m0_fi_enable_once */
#include "conf/objs/common.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"          /* M0_LOG */

/** Counter for op_launch callback. */
static uint32_t ut_clovis_launch_cb_pass_count;
static uint32_t ut_clovis_launch_cb_fail_count;

/** Counter for op_fini callbacks. */
static uint32_t ut_clovis_op_fini_cb_count;

/** Counter for op_sm callbacks. */
static int ut_clovis_test_op_sm_callme_counter;

/** Counter for initlift get next floor callbacks. */
static int ut_clovis_test_initlift_gnf_counter;

/* Default Clovis configuration*/
struct m0_clovis_config clovis_default_config;

M0_INTERNAL void ut_clovis_realm_entity_setup(struct m0_clovis_realm  *realm,
						 struct m0_clovis_entity *ent,
						 struct m0_clovis        *cinst)
{
	struct m0_uint128 id;

	M0_UT_ASSERT(realm != NULL);
	M0_UT_ASSERT(ent != NULL);
	M0_UT_ASSERT(cinst != NULL);

	M0_SET0(realm);
	realm->re_instance = cinst;

	id = M0_CLOVIS_ID_APP;
	id.u_lo++;

	M0_SET0(ent);
	m0_clovis_entity_init(ent, realm, &id, M0_CLOVIS_ET_OBJ);
}

static bool ut_clovis_test_floor_tick_cb(struct m0_clink *cl)
{
	ut_clovis_test_initlift_gnf_counter++;
	return false;
}

/**
 * Make an arbitrary move between 'floors' of the clovis init state machine.
 * Makes various asserts about sane behaviour.
 *
 * @param instance The clovis instance we are working with.
 * @param start_state The state we believe we are in.
 * @param stop_state The state we would like to be in.
 */
static void ut_clovis_test_m0_clovis_init_floor_tick(struct m0_clovis *instance,
						     int start_state, int stop_state)
{
	struct m0_clink clink;

	M0_UT_ASSERT(stop_state >= CLOVIS_IL_UNINITIALISED);
	M0_UT_ASSERT(instance->m0c_initlift_sm.sm_state == start_state);

	ut_clovis_test_initlift_gnf_counter = 0;

	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_clink_init(&clink, ut_clovis_test_floor_tick_cb);
	m0_clink_add(&instance->m0c_initlift_sm.sm_chan, &clink);
	m0_sm_move(&instance->m0c_initlift_sm, 0, stop_state);
	m0_clink_del(&clink);
	m0_clink_fini(&clink);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	if (instance->m0c_initlift_sm.sm_state != CLOVIS_IL_UNINITIALISED
	    && instance->m0c_initlift_sm.sm_state != CLOVIS_IL_FAILED) {
		M0_UT_ASSERT(ut_clovis_test_initlift_gnf_counter != 0);
		M0_UT_ASSERT(instance->m0c_initlift_sm.sm_state == stop_state);
	}
	if (instance->m0c_initlift_direction == CLOVIS_STARTUP)
		M0_UT_ASSERT(instance->m0c_initlift_rc == 0);

	if(instance->m0c_initlift_sm.sm_state != CLOVIS_IL_FAILED)
		M0_UT_ASSERT(instance->m0c_initlift_sm.sm_rc == 0);
	else
		M0_UT_ASSERT(instance->m0c_initlift_sm.sm_rc ==
			     instance->m0c_initlift_rc);
}

/**
 * Move  the clovis init state machine from the CLOVIS_IL_UNINITIALISED state
 * to the specified state, then back again, optionally due to a failure.
 *
 * @param instance The clovis instance we are working with.
 * @param limit The maximum 'floor' to reach.
 * @param fail Whether a simulated failure causes us to return to
 * 	       CLOVIS_IL_UNINITIALISED
 */
static void ut_clovis_test_m0_clovis_init_floors(struct m0_clovis *instance,
						 int limit, bool fail)
{
	int           i;
	struct m0_sm *sm = &instance->m0c_initlift_sm;

	M0_UT_ASSERT(limit <= CLOVIS_IL_INITIALISED);
	M0_UT_ASSERT(limit > CLOVIS_IL_UNINITIALISED);

	M0_UT_ASSERT(instance->m0c_initlift_direction == CLOVIS_STARTUP);
	M0_UT_ASSERT(sm->sm_rc == 0);
	M0_UT_ASSERT(sm->sm_state == CLOVIS_IL_UNINITIALISED);

	for (i = CLOVIS_IL_UNINITIALISED + 1; i <= limit; i++)
		ut_clovis_test_m0_clovis_init_floor_tick(instance,
							 sm->sm_state, i);
	M0_UT_ASSERT(sm->sm_state == limit);

	if (fail) {
		/*
		 * Now Reverse direction and shut down
		 *(with a simulated error)
		 */
		clovis_initlift_fail(-EPROTO, instance);
		M0_UT_ASSERT(instance->m0c_initlift_direction
			     == CLOVIS_SHUTDOWN);
		M0_UT_ASSERT(sm->sm_rc == 0);
		M0_UT_ASSERT(instance->m0c_initlift_rc == -EPROTO);
	} else {
		/* Perform a clean shutdown */
		instance->m0c_initlift_direction = CLOVIS_SHUTDOWN;
		M0_UT_ASSERT(sm->sm_rc == 0);
		M0_UT_ASSERT(instance->m0c_initlift_rc == 0);
	}

	/*
	 * A failure will always happen in some state, which is expected to
	 * clean up after itself. In this case we triggered a failure from
	 * the outside, so we choose to fudge the current state.
	 *
	 * A clean shutdown would normally only happen from
	 * CLOVIS_IL_INITIALISED, we changed direction half way through, so
	 * need to fudge the current state so that we pass through it for
	 * startup and shutdown.
	 */
	sm->sm_state++;

	for (i = sm->sm_state - 1; i >= CLOVIS_IL_UNINITIALISED; i--)
		ut_clovis_test_m0_clovis_init_floor_tick(instance,
							 sm->sm_state, i);

	if (fail) {
		/*
		 * Check we ended up in CLOVIS_IL_FAILED, with sm_rc set
		 * to -EPROTO
		 */
		M0_UT_ASSERT(sm->sm_state == CLOVIS_IL_FAILED);
		M0_UT_ASSERT(sm->sm_rc == -EPROTO);
	} else {
		M0_UT_ASSERT(sm->sm_state == CLOVIS_IL_UNINITIALISED);
		M0_UT_ASSERT(sm->sm_rc == 0);
	}
}

/**
 * Partial Unit tests m0_clovis_init.
 *
 * Unit tests uses fault injections to make clovis_initlift_get_next_floor be
 * a no-op, so that they can manually move the 'initlift' between
 * floors to test cleanup/failures etc. In a real environment, the 'initlift'
 * would be moved through all the states to CLOVIS_IL_INITIALISED by
 * 'm0_clovis_init()'.
 *
 * It isn't possible to test connecting to services and confd etc from here.
 */
static void ut_clovis_test_m0_clovis_init(void)
{
	int                      i;
	int                      rc = 0; /* required */
	struct m0_clovis        *instance;

	/* Check -ENOMEM will be returned */
	m0_fi_enable_once("m0_alloc", "fail_allocation");
	instance = NULL;
	rc = CLOVIS_INIT(&instance);
	M0_UT_ASSERT(rc == -ENOMEM);
	M0_UT_ASSERT(instance == NULL);

	/* Check NULL or 0 for each argument */
	clovis_default_config.cc_local_addr = CLOVIS_DEFAULT_EP;
	clovis_default_config.cc_ha_addr = CLOVIS_DEFAULT_HA_ADDR;
	clovis_default_config.cc_profile = CLOVIS_DEFAULT_PROFILE;
	clovis_default_config.cc_tm_recv_queue_min_len = 0;
	clovis_default_config.cc_tm_recv_queue_min_len =
					M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_default_config.cc_max_rpc_msg_size = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	/*
	 * Cause the initlift to fail during init to check the error is
	 * propagated.
	 */
	m0_fi_enable_once("clovis_initlift_move_next_floor", "failure");
	rc = CLOVIS_INIT(&instance);
	M0_UT_ASSERT(rc == -EPROTO);
	M0_UT_ASSERT(instance == NULL);

	m0_fi_enable("clovis_initlift_get_next_floor", "clovis_ut");
	m0_fi_enable("clovis_initlift_move_next_floor", "immediate_ret");
	/*
	 * Trigger a failure at each 'floor', to check m0_clovis_init
	 * can finalise itself if an initialisation phase causes a failure.
	 */
	/*
	 * The floor after CLOVIS_IL_AST_THREAD attempts to contact confd,
	 * which we can't do from a unit-test environment (cyclic dependency
	 * with user-space:confd).
	 */
	for (i = CLOVIS_IL_UNINITIALISED + 1; i <= CLOVIS_IL_AST_THREAD; i++) {
		instance = NULL;
		rc = CLOVIS_INIT(&instance);
		M0_UT_ASSERT(rc == 0);
		ut_clovis_test_m0_clovis_init_floors(instance, i, true);
		m0_clovis_fini(instance, false);
	}

	m0_fi_disable("clovis_initlift_get_next_floor", "clovis_ut");
	m0_fi_disable("clovis_initlift_move_next_floor", "immediate_ret");
}

M0_INTERNAL int ut_m0_clovis_init(struct m0_clovis **instance)
{
	int                     i;
	int                     rc;
	struct m0_sm           *sm;
	struct m0_fid           id;
	struct m0_pool         *pool;
	struct m0_pool         *mdpool;
	struct m0_pool_version *pv;
	struct m0_pool_version *mdpv;

	m0_fi_enable_once("clovis_initlift_move_next_floor", "immediate_ret");
	rc = CLOVIS_INIT(instance);

	m0_fi_enable("clovis_initlift_get_next_floor", "clovis_ut");
	if (rc == 0) {
		sm = &(*instance)->m0c_initlift_sm;
		(*instance)->m0c_initlift_direction = CLOVIS_STARTUP;

		for (i = CLOVIS_IL_UNINITIALISED + 1;
		     i <= CLOVIS_IL_RPC;
		     //i <= CLOVIS_IL_AST_THREAD;
		     i++) {
			ut_clovis_test_m0_clovis_init_floor_tick(*instance,
								 sm->sm_state,
								 i);
		}

		/* Some dummy stuff for pools. */
		M0_ALLOC_PTR(pool);
		M0_UT_ASSERT(pool != NULL);
		M0_SET0(&id);
		m0_pool_init(pool, &id, 0);
		pools_tlist_init(&(*instance)->m0c_pools_common.pc_pools);
		pools_tlink_init_at_tail(pool, &((*instance)->m0c_pools_common.pc_pools));

		M0_ALLOC_PTR(mdpool);
		M0_UT_ASSERT(mdpool != NULL);
		id.f_key=1;
		m0_pool_init(mdpool, &id, 0);
		pools_tlink_init_at_tail(mdpool, &((*instance)->m0c_pools_common.pc_pools));

		M0_ALLOC_PTR(pv);
		M0_UT_ASSERT(pv != NULL);
		M0_SET0(pv);
		pv->pv_pool = pool;
		pool_version_tlink_init_at_tail(pv, &pool->po_vers);

		(*instance)->m0c_pools_common.pc_cur_pver = pv;
		(*instance)->m0c_pools_common.pc_confc = (struct m0_confc *)DUMMY_PTR;

		M0_ALLOC_PTR(mdpv);
		M0_UT_ASSERT(mdpv != NULL);
		M0_SET0(mdpv);
		mdpv->pv_pool = mdpool;
		pool_version_tlink_init_at_tail(mdpv, &mdpool->po_vers);
		(*instance)->m0c_pools_common.pc_md_pool = mdpool;

		(*instance)->m0c_process_fid = M0_FID_TINIT(M0_CONF__PROCESS_FT_ID, 0, 1);
	}
	m0_fi_disable("clovis_initlift_get_next_floor", "clovis_ut");

	return rc;
}

/** Unit tests m0_clovis_fini. */
M0_INTERNAL void ut_clovis_test_m0_clovis_fini(void)
{
	int               i;
	int               rc = 0; /* required */
	struct m0_clovis *instance = NULL;

	m0_fi_enable("clovis_initlift_get_next_floor", "clovis_ut");
	m0_fi_enable("clovis_initlift_move_next_floor", "immediate_ret");
	/*
	 * Shutdown from each 'floor' in turn, this catches any
	 * 'can't finalise it' as early as possible. The failure version of
	 * this test occurs in m0_clovis_init's tests.
	 */
	for (i = CLOVIS_IL_UNINITIALISED + 1; i <= CLOVIS_IL_AST_THREAD; i++) {
		instance = NULL;
		rc = CLOVIS_INIT(&instance);
		M0_UT_ASSERT(rc == 0);
		ut_clovis_test_m0_clovis_init_floors(instance, i, false);
		m0_clovis_fini(instance, false);
	}
	m0_fi_disable("clovis_initlift_get_next_floor", "clovis_ut");
	m0_fi_disable("clovis_initlift_move_next_floor", "immediate_ret");
}

M0_INTERNAL void ut_m0_clovis_fini(struct m0_clovis **instance)
{
	int           i;
	struct m0_sm *sm;

	m0_fi_enable("clovis_initlift_get_next_floor", "clovis_ut");
	if (*instance != NULL) {
		sm = &(*instance)->m0c_initlift_sm;

		(*instance)->m0c_initlift_direction = CLOVIS_SHUTDOWN;

		/*
		 * A clean shutdown would normally only happen from
		 * CLOVIS_IL_INITIALISED, we changed direction half way through, so
		 * need to fudge the current state so that we pass through it for
		 * startup and shutdown.
		 */
		sm->sm_state++;

		for (i = sm->sm_state - 1; i >= CLOVIS_IL_UNINITIALISED; i--) {
			ut_clovis_test_m0_clovis_init_floor_tick(*instance,
								 sm->sm_state,
								 i);
		}

	}
	m0_fi_disable("clovis_initlift_get_next_floor", "clovis_ut");

	m0_clovis_fini(*instance, false);
}

/* Don't use CLOVIS_INIT after this point */
#undef CLOVIS_INIT


static void ut_clovis_test_clovis_entity_invariant_locked(void)
{
	struct m0_clovis           *instance = NULL;
	struct m0_clovis_entity     entity;
	struct m0_uint128           id;
	struct m0_clovis_container  uber_realm;
	struct m0_sm_group         *en_grp;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);
	m0_clovis_container_init(&uber_realm, NULL,
				 &M0_CLOVIS_UBER_REALM, instance);
	ut_clovis_realm_entity_setup(&uber_realm.co_realm, &entity, instance);
	en_grp = &entity.en_sm_group;
	m0_sm_group_init(en_grp);

	/* we need a valid id */
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;

	/* base case: no error */
	m0_sm_group_lock(en_grp);
	M0_UT_ASSERT(clovis_entity_invariant_locked(&entity));
	m0_sm_group_unlock(en_grp);

	/* Check a bad type is caught, but not asserted */
	entity.en_type = 42;
	m0_sm_group_lock(en_grp);
	M0_UT_ASSERT(!clovis_entity_invariant_locked(&entity));
	m0_sm_group_unlock(en_grp);
	entity.en_type = M0_CLOVIS_ET_OBJ;

	m0_clovis_entity_fini(&entity);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * Tests the pre and post conditions of the m0_clovis_entity_init()
 * helper function.
 * The testee is seen as a black box that has to react as expected
 * to some specific input and generate some valid output.
 */
static void ut_clovis_test_m0_clovis_entity_init(void)
{
	struct m0_clovis_entity    entity;
	struct m0_uint128          id;
	struct m0_clovis          *instance = NULL;
	struct m0_clovis_container uber_realm;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);
	m0_clovis_container_init(&uber_realm, NULL,
				 &M0_CLOVIS_UBER_REALM,
				 instance);

	/* we need a valid id */
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;

	/* base case: no error */
	m0_clovis_entity_init(&entity, &uber_realm.co_realm,
			      &id, M0_CLOVIS_ET_OBJ);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * Tests the pre and post conditions of the m0_clovis_obj_init()
 * entry point. Also checks the object is correctly initialised.
 * The testee is seen as a black box that has to react as expected
 * to some specific input and generate some valid output.
 */
static void ut_clovis_test_m0_clovis_obj_init(void)
{
	struct m0_clovis_obj       obj;
	struct m0_uint128          id;
	struct m0_clovis          *instance = NULL;
	struct m0_clovis_container uber_realm;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);
	m0_clovis_container_init(&uber_realm, NULL,
				 &M0_CLOVIS_UBER_REALM,
				 instance);

	/* we need a valid id */
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;

	/* base case: no error */
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &uber_realm.co_realm, &id,
			   m0_clovis_layout_id(instance));

	/* check the initialisation */
	M0_UT_ASSERT(obj.ob_entity.en_type == M0_CLOVIS_ET_OBJ);
	M0_UT_ASSERT(m0_uint128_cmp(&obj.ob_entity.en_id, &id) == 0);
	M0_UT_ASSERT(obj.ob_entity.en_realm == &uber_realm.co_realm);
	M0_UT_ASSERT(obj.ob_attr.oa_bshift == CLOVIS_DEFAULT_BUF_SHIFT);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * Callback for fake-operation launch, increments a local counter, and
 * moves the state machine along.
 * Called with the m0_clovis_sm_group lock held.
 */
static void
ut_clovis_launch_cb_pass(struct m0_clovis_op_common *oc)
{
	M0_UT_ASSERT(oc != NULL);
	M0_UT_ASSERT(oc->oc_op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);

	ut_clovis_launch_cb_pass_count++;

	m0_sm_move(&oc->oc_op.op_sm, 0, M0_CLOVIS_OS_LAUNCHED);
}

/**
 * Callback for fake-operation launch, increments a local counter, and
 * moves the state machine along.
 * Called with the m0_clovis_sm_group lock held.
 */
static void
ut_clovis_launch_cb_fail(struct m0_clovis_op_common *oc)
{
	M0_UT_ASSERT(oc != NULL);
	M0_UT_ASSERT(oc->oc_op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);

	ut_clovis_launch_cb_fail_count++;

	//m0_sm_move(&oc->oc_op.op_sm, -EINVAL, M0_CLOVIS_OS_FAILED);
}

/** Initialises an operation for use as a fake-operation. */
static void
ut_clovis_init_fake_op(struct m0_clovis_op_common *cop,
		       struct m0_clovis_entity *ent,
		       void (*cb)(struct m0_clovis_op_common *oc))
{
	struct m0_sm_group *grp;

	M0_SET0(cop);

	grp = &cop->oc_op.op_sm_group;
	m0_sm_group_init(grp);
	m0_sm_init(&cop->oc_op.op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, grp);
	cop->oc_op.op_entity = ent;
	cop->oc_op.op_size = sizeof *cop;
	cop->oc_cb_launch = cb;

	m0_clovis_op_bob_init(&cop->oc_op);
	m0_clovis_op_common_bob_init(cop);
}

/**
 * Tests m0_clovis_op_launch().
 */
static void ut_clovis_test_m0_clovis_op_launch_one(void)
{
	struct m0_clovis_op_common oc;
	struct m0_clovis          *instance = NULL;
	struct m0_clovis_entity    ent;
	struct m0_clovis_realm     realm;

	/* init */
	ut_m0_clovis_init(&instance);
	ut_clovis_realm_entity_setup(&realm, &ent, instance);
	ut_clovis_init_fake_op(&oc, &ent, &ut_clovis_launch_cb_pass);

	/* base case */
	ut_clovis_launch_cb_pass_count = 0;
	m0_clovis_op_launch_one(&oc.oc_op);
	M0_UT_ASSERT(ut_clovis_launch_cb_pass_count == 1);

	m0_clovis_entity_fini(&ent);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/** Unit tests m0_clovis_op_launch. */
static void ut_clovis_test_m0_clovis_op_launch(void)
{
	struct m0_clovis_entity    ent;
	struct m0_clovis_op_common cops[2];
	struct m0_clovis_op       *p_ops[2];
	struct m0_clovis_realm     realm;
	struct m0_clovis          *instance = NULL;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);

	/* Give ops some sane state */
	ut_clovis_realm_entity_setup(&realm, &ent, instance);
	ut_clovis_init_fake_op(&cops[0], &ent,
			       &ut_clovis_launch_cb_pass);
	ut_clovis_init_fake_op(&cops[1], &ent,
			       &ut_clovis_launch_cb_pass);
	M0_SET_ARR0(p_ops);
	p_ops[0] = &cops[0].oc_op;
	p_ops[1] = &cops[1].oc_op;

	/* Check our op_launch has a firm foundation */
	ut_clovis_launch_cb_pass_count = 0;
	m0_clovis_op_launch(p_ops, ARRAY_SIZE(p_ops));
	M0_UT_ASSERT(ut_clovis_launch_cb_pass_count == 2);

	/* Check op_launch continues even if an operation fails */
	ut_clovis_init_fake_op(&cops[0], &ent,
					      &ut_clovis_launch_cb_pass);
	ut_clovis_init_fake_op(&cops[1], &ent,
					      &ut_clovis_launch_cb_fail);
	ut_clovis_launch_cb_pass_count = 0;
	ut_clovis_launch_cb_fail_count = 0;
	m0_clovis_op_launch(p_ops, ARRAY_SIZE(p_ops));
	M0_UT_ASSERT(ut_clovis_launch_cb_pass_count == 1);
	M0_UT_ASSERT(ut_clovis_launch_cb_fail_count == 1);

	m0_clovis_entity_fini(&ent);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * An ops callback that increments a counter, used to check the
 * callbacks were correctly called.
 */
static void ut_clovis_test_op_sm_callme(struct m0_clovis_op *op)
{
	M0_UT_ASSERT(op != NULL);
	ut_clovis_test_op_sm_callme_counter++;
}

/**
 * An ops callback that asserts immediatly. Don't call me - its a trap.
 */
static void ut_clovis_test_op_sm_dont_callme(struct m0_clovis_op *op)
{
	M0_UT_ASSERT(0);
}

/**
 * Helper method for setting many op_ops values in a one-liner.
 *
 *  @param ops the ops structure to pack.
 *  @param executed whether the execute callback should assert immediately.
 *  @param stable whether the stable callback should assert immediately.
 *  @param failed whether the failed callback should assert immediately.
 */
static void ut_clovis_test_op_sm_cbs_helper(struct m0_clovis_op_ops *cbs,
					    int executed, int stable,
					    int failed)
{
	M0_UT_ASSERT(cbs != NULL);

	if (executed)
		cbs->oop_executed = ut_clovis_test_op_sm_callme;
	else
		cbs->oop_executed = ut_clovis_test_op_sm_dont_callme;
	if (stable)
		cbs->oop_stable = ut_clovis_test_op_sm_callme;
	else
		cbs->oop_stable = ut_clovis_test_op_sm_dont_callme;
	if (failed)
		cbs->oop_failed = ut_clovis_test_op_sm_callme;
	else
		cbs->oop_failed = ut_clovis_test_op_sm_dont_callme;
}

/**
 * Tests the operation state machine by moving it through all valid
 * transitions. Also checks the optional callback mechanism works as expected.
 */
static void ut_clovis_test_op_sm(void)
{
	/*
	 * Create and move an op sm through all the permisable states.
	 * The sm code will panic if we try an invalid transition.
	 */
	struct m0_clovis_op     op;
	struct m0_clovis_op_ops cbs;
	struct m0_clovis       *instance = NULL;
	struct m0_sm_group     *grp;


	/* initialise clovis */
	ut_m0_clovis_init(&instance);

	M0_SET0(&op);
	op.op_size = sizeof(struct m0_clovis_op_common);
	op.op_cbs = &cbs;
	m0_clovis_op_bob_init(&op);

	grp = &op.op_sm_group;
	m0_sm_group_init(grp);
	m0_sm_group_lock(grp);

	/* INIT -> FAILED */
	ut_clovis_test_op_sm_cbs_helper(&cbs, 0, 0, 1);
	ut_clovis_test_op_sm_callme_counter = 0;
	m0_sm_init(&op.op_sm, &clovis_op_conf, M0_CLOVIS_OS_INITIALISED, grp);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	m0_sm_move(&op.op_sm, -EINVAL, M0_CLOVIS_OS_FAILED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_FAILED);
	M0_UT_ASSERT(ut_clovis_test_op_sm_callme_counter == 0);

	/* INIT -> LAUNCHED -> EXECUTED -> STABLE */
	ut_clovis_test_op_sm_cbs_helper(&cbs, 1, 1, 0);
	ut_clovis_test_op_sm_callme_counter = 0;
	m0_sm_init(&op.op_sm, &clovis_op_conf, M0_CLOVIS_OS_INITIALISED, grp);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_LAUNCHED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_LAUNCHED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_EXECUTED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(ut_clovis_test_op_sm_callme_counter == 0);

	/* INIT -> LAUNCHED -> EXECUTED -> FAILED */
	ut_clovis_test_op_sm_cbs_helper(&cbs, 1, 0, 1);
	ut_clovis_test_op_sm_callme_counter = 0;
	m0_sm_init(&op.op_sm, &clovis_op_conf, M0_CLOVIS_OS_INITIALISED, grp);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_LAUNCHED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_LAUNCHED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_EXECUTED);
	m0_sm_move(&op.op_sm, -EINVAL, M0_CLOVIS_OS_FAILED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_FAILED);
	M0_UT_ASSERT(ut_clovis_test_op_sm_callme_counter == 0);

	/* INIT -> LAUNCHED -> FAILED */
	ut_clovis_test_op_sm_cbs_helper(&cbs, 0, 0, 1);
	ut_clovis_test_op_sm_callme_counter = 0;
	m0_sm_init(&op.op_sm, &clovis_op_conf, M0_CLOVIS_OS_INITIALISED, grp);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_LAUNCHED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_LAUNCHED);
	m0_sm_move(&op.op_sm, -EINVAL, M0_CLOVIS_OS_FAILED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_FAILED);
	M0_UT_ASSERT(ut_clovis_test_op_sm_callme_counter == 0);

	/*
	 * Test a INIT -> LAUNCHEd -> EXECUTED -> STABLE transition
	 * with no callbacks enabled - to check they are optional
	 */
	M0_SET0(&cbs);
	m0_sm_init(&op.op_sm, &clovis_op_conf, M0_CLOVIS_OS_INITIALISED, grp);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_LAUNCHED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_LAUNCHED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_EXECUTED);
	m0_sm_move(&op.op_sm, 0, M0_CLOVIS_OS_STABLE);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_STABLE);

	m0_sm_group_unlock(grp);
	m0_sm_group_fini(grp);

	m0_clovis_op_bob_fini(&op);
	ut_m0_clovis_fini(&instance);
}

/**
 * Callback for the op_wait timer. Moves the provided operation state machine
 * into the EXECUTED state.
 *
 * @param data the operation to move, as an ugly integer.
 */
unsigned long ut_clovis_test_m0_clovis_op_wait_timer(unsigned long data)
{
	struct m0_clovis_op *op = (struct m0_clovis_op *)data;

	m0_sm_group_lock(&op->op_sm_group);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	m0_sm_group_unlock(&op->op_sm_group);

	return 0;
}

/** Unit tests m0_clovis_op_wait(). */
void ut_clovis_test_m0_clovis_op_wait(void)
{
	int                        rc = 0; /* required */
	struct m0_clovis_op_common cops[2];
	struct m0_clovis_entity    ent;
	struct m0_clovis_realm     realm;
	m0_time_t                  start;
	struct m0_clovis_op       *p_ops[2];
	struct m0_timer            tmr;
	struct m0_clovis          *instance = NULL;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);

	/* initialise our fake operation */
	ut_clovis_realm_entity_setup(&realm, &ent, instance);
	ut_clovis_init_fake_op(&cops[0], &ent,
					      &ut_clovis_launch_cb_pass);
	ut_clovis_init_fake_op(&cops[1], &ent,
					      &ut_clovis_launch_cb_pass);

	/* Test the timeout must be in the future */
	rc = m0_clovis_op_wait(&cops[0].oc_op,
			       M0_BITS(M0_CLOVIS_OS_LAUNCHED),
			       m0_time_now()-M0_TIME_ONE_SECOND);
	M0_UT_ASSERT(rc == -ETIMEDOUT);

	/* Test we time out, and don't wake up early */
	start = m0_time_now();
	rc = m0_clovis_op_wait(&cops[0].oc_op,
			       M0_BITS(M0_CLOVIS_OS_LAUNCHED),
			       m0_time_from_now(1,0));
	M0_UT_ASSERT(rc == -ETIMEDOUT);
	M0_UT_ASSERT(m0_time_now() - start >= M0_TIME_ONE_SECOND);

	/*
	 * Now launch the operation and check wait succeeds.
	 * Wait for operations in the reverse order to the order they
	 * were launched in.
	 */
	p_ops[0] = &cops[0].oc_op;
	p_ops[1] = &cops[1].oc_op;
	m0_clovis_op_launch(p_ops, ARRAY_SIZE(p_ops));
	rc = m0_clovis_op_wait(&cops[1].oc_op,
			       M0_BITS(M0_CLOVIS_OS_LAUNCHED),
			       m0_time_from_now(1,0));
	M0_UT_ASSERT(rc == 0);
	rc = m0_clovis_op_wait(&cops[0].oc_op,
			       M0_BITS(M0_CLOVIS_OS_LAUNCHED),
			       m0_time_from_now(1,0));
	M0_UT_ASSERT(rc == 0);

	/* Test repeated waits don't cause any problems */
	rc = m0_clovis_op_wait(&cops[0].oc_op,
			       M0_BITS(M0_CLOVIS_OS_LAUNCHED),
			       m0_time_from_now(1,0));
	M0_UT_ASSERT(rc == 0);
	rc = m0_clovis_op_wait(&cops[0].oc_op,
			       M0_BITS(M0_CLOVIS_OS_LAUNCHED),
			       m0_time_from_now(1,0));
	M0_UT_ASSERT(rc == 0);

	/* Use timers to check wait will wake up after a state transition */
	rc = m0_timer_init(&tmr, M0_TIMER_HARD, NULL,
			   &ut_clovis_test_m0_clovis_op_wait_timer,
			   (unsigned long)p_ops[0]);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(p_ops[0]->op_sm.sm_state == M0_CLOVIS_OS_LAUNCHED);
	m0_timer_start(&tmr, m0_time_from_now(1, 0));
	start = m0_time_now();
	M0_UT_ASSERT(p_ops[0]->op_sm.sm_state == M0_CLOVIS_OS_LAUNCHED);

	/* Wait for longer than the timer */
	/*
	 * N.B. the wait time has to be exagerated so that this test doesn't
	 * unnecesarily fail in gdb
	 */
	rc = m0_clovis_op_wait(p_ops[0],
			       M0_BITS(M0_CLOVIS_OS_EXECUTED),
			       m0_time_from_now(10,0));
	M0_UT_ASSERT(rc == 0);

	/* Check we did wake up early */
	M0_UT_ASSERT(m0_time_now() - start >= M0_TIME_ONE_SECOND);
	M0_UT_ASSERT(m0_time_now() - start < 2*M0_TIME_ONE_SECOND);

	if (m0_timer_is_started(&tmr))
		m0_timer_stop(&tmr);

	m0_timer_fini(&tmr);

	m0_clovis_entity_fini(&ent);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * Tests m0_clovis_op_alloc(), focusing on its pre-conditions. Also checks the
 * output is right when function succeeds.
 */
static void ut_clovis_test_m0_clovis_op_alloc(void)
{
	struct m0_clovis_op *op;
	size_t               oc_size = sizeof(struct m0_clovis_op_common);

	/* base case: it works! */
	op = (struct m0_clovis_op *)NULL;
	m0_clovis_op_alloc(&op, oc_size);
	M0_UT_ASSERT(op != NULL);
	M0_UT_ASSERT(op->op_size == oc_size);

	/* avoid leaks */
	m0_free(op);
}

/**
 * Tests  m0_clovis_op_init().
 */
static void ut_clovis_test_m0_clovis_op_init(void)
{
	struct m0_sm_conf       conf;
	struct m0_clovis_entity ent;
	struct m0_clovis_realm  realm;
	struct m0_clovis_op     op;
	struct m0_clovis_op    *op_p;
	struct m0_clovis       *instance = NULL;
	int                     rc = 0; /* required */

	/* Keep gcc quiet during debug build */
	M0_SET0(&conf);

	/* initialise clovis */
	ut_m0_clovis_init(&instance);

	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	/* base case: no asserts triggered */
	op_p = &op;
	op.op_code = M0_CLOVIS_EO_INVALID;
	rc = m0_clovis_op_init(op_p, &clovis_op_conf, &ent);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op.op_sm.sm_state == M0_CLOVIS_OS_INITIALISED);
	m0_sm_group_lock(&op_p->op_sm_group);
	M0_UT_ASSERT(m0_sm_invariant(&op.op_sm));
	m0_sm_group_unlock(&op_p->op_sm_group);

	m0_clovis_entity_fini(&ent);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * Tests m0_clovis_entity_fini().
 */
static void ut_clovis_test_m0_clovis_entity_fini(void)
{
	struct m0_uint128          id;
	struct m0_clovis_obj       obj;
	struct m0_clovis_entity   *ent;
	struct m0_clovis          *instance = NULL;
	struct m0_clovis_container uber_realm;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);
	m0_clovis_container_init(&uber_realm, NULL,
				 &M0_CLOVIS_UBER_REALM,
				 instance);

	/* Create an entity we can use */
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &uber_realm.co_realm, &id,
			   m0_clovis_layout_id(instance));
	ent = &obj.ob_entity;

	/* Base case: m0_clovis_entity_fini works */
	m0_clovis_entity_fini(ent);

	/* Re-initialise */
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &uber_realm.co_realm, &id,
			   m0_clovis_layout_id(instance));

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * Tests m0_clovis_obj_fini().
 */
static void ut_clovis_test_m0_clovis_obj_fini(void)
{
	struct m0_uint128          id;
	struct m0_clovis_obj       obj;
	struct m0_clovis          *instance = NULL;
	struct m0_clovis_container uber_realm;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);
	m0_clovis_container_init(&uber_realm, NULL,
				 &M0_CLOVIS_UBER_REALM,
				 instance);

	/* Create an entity we can use */
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &uber_realm.co_realm, &id,
			   m0_clovis_layout_id(instance));

	/* Base case: m0_clovis_obj_fini works */
	m0_clovis_obj_fini(&obj);

	/* Re-initialise */
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &uber_realm.co_realm, &id,
			   m0_clovis_layout_id(instance));

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/**
 * Callback for op_fini, used for unit tests to check the callback is occuring.
 *
 * @param oc the common operation that has been finished.
 */
static void
ut_clovis_op_fini_cb(struct m0_clovis_op_common *oc)
{
	M0_PRE(oc != NULL);
	ut_clovis_op_fini_cb_count++;
}

/**
 * Tests the pre and post conditions of the m0_clovis_op_fini()
 * entry point. Tests the optional callback is called if specified, and
 * the state machine is set to UNINITIALISED.
 */
static void ut_clovis_test_m0_clovis_op_fini(void)
{
	int                         rc = 0; /* required */
	struct m0_clovis_entity     ent;
	struct m0_clovis_realm      realm;
	struct m0_clovis_op        *op;
	struct m0_clovis_op_common *oc;
	struct m0_clovis           *instance = NULL;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);

	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	/* Allocate an operation to try and fini */
	op = NULL;
	rc = m0_clovis_op_alloc(&op, sizeof *oc);
	M0_UT_ASSERT(rc == 0);
	op->op_code = M0_CLOVIS_EO_INVALID;
	rc = m0_clovis_op_init(op, &clovis_op_conf, &ent);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_size >= sizeof *oc);
	oc = container_of(op, struct m0_clovis_op_common, oc_op);
	m0_clovis_op_common_bob_init(oc);
	m0_sm_group_init(&op->op_sm_group);

	/* test op_fini calls the provided callback */
	oc->oc_cb_fini = ut_clovis_op_fini_cb;
	ut_clovis_op_fini_cb_count = 0;
	m0_clovis_op_fini(op);
	M0_UT_ASSERT(ut_clovis_op_fini_cb_count > 0);
	M0_UT_ASSERT(op->op_sm.sm_state == M0_CLOVIS_OS_UNINITIALISED);

	/* fini */
	m0_clovis_op_free(op);
	m0_clovis_entity_fini(&ent);

	ut_m0_clovis_fini(&instance);
}

/**
 * Tests memory allocated by op_init/op_alloc can only be freed in the
 * appropriate circumstances.
 */
static void ut_clovis_test_m0_clovis_op_free(void)
{
	struct m0_clovis_op	   *op;
	struct m0_clovis_op_common *oc;
	struct m0_clovis_op_obj    *oo;

	/* base case: m0_clovis_op_common */
	op = m0_alloc(sizeof *oc);
	op->op_size = sizeof *oc;
	m0_clovis_op_free(op);

	/* base case: m0_clovis_op_common */
	oc = m0_alloc(sizeof *oc);
	oc->oc_op.op_size = sizeof *oc;
	m0_clovis_op_free(&oc->oc_op);

	/* base case: m0_clovis_op_obj */
	oo = m0_alloc(sizeof *oo);
	oo->oo_oc.oc_op.op_size = sizeof *oo;
	m0_clovis_op_free(&oo->oo_oc.oc_op);
}

/**
 * Tests setting up an operation.
 */
static void ut_clovis_test_m0_clovis_op_setup(void)
{
	m0_time_t               linger;
	struct m0_clovis_op     op;
	struct m0_clovis_op_ops cbs;
	struct m0_clovis       *instance = NULL;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);

	M0_SET0(&op);
	op.op_sm.sm_state = M0_CLOVIS_OS_INITIALISED;

	/* Base case: everything works */
	m0_clovis_op_setup(&op, NULL, 0);

	/* test ops and linger get set, overwriting manual versions */
	op.op_cbs = (void *)0xDEADBEEF;
	op.op_linger = 0x42;
	linger = m0_time_now() + 4ULL * M0_TIME_ONE_SECOND;
	m0_clovis_op_setup(&op, &cbs, linger);
	M0_UT_ASSERT(op.op_cbs == &cbs);
	M0_UT_ASSERT(op.op_linger == linger);

	/* finalise clovis */
	ut_m0_clovis_fini(&instance);
}

/** Tests an operation can be kicked without any side effects. */
static void ut_clovis_test_m0_clovis_op_kick(void)
{
	int                      rc;
	struct m0_clovis_op      op;
	struct m0_clovis_op     *op_p;
	struct m0_clovis_entity  ent;
	struct m0_clovis_realm   realm;
	struct m0_clovis        *instance = NULL;

	/* initialise clovis */
	ut_m0_clovis_init(&instance);

	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	op_p = &op;
	op.op_code = M0_CLOVIS_EO_INVALID;
	rc = m0_clovis_op_init(op_p, &clovis_op_conf, &ent);
	M0_UT_ASSERT(rc == 0);

	/* Base case - kick works */
	m0_clovis_op_kick(op_p);

	m0_clovis_entity_fini(&ent);
	ut_m0_clovis_fini(&instance);
}

#if 0 /* The op state isn't supported yet. */
static void ut_clovis_test_m0_clovis_op_transaction_committed(void)
{
	struct m0_clovis_op_common oc;
	struct m0_clovis          *instance = NULL;
	struct m0_clovis_entity    ent;
	struct m0_clovis_realm     realm;
	struct m0_sm_group *grp;

	/* init */
	ut_m0_clovis_init(&instance);
	ut_clovis_realm_entity_setup(&realm, &ent, instance);
	ut_clovis_init_fake_op(&oc, &ent, &ut_clovis_launch_cb_pass);

	/* Test m0_clovis_op_transaction_commited. */
	grp = &oc.oc_op.op_sm_group;
	m0_sm_group_lock(grp);
	oc.oc_op.op_sm.sm_state = M0_CLOVIS_OS_EXECUTED;
	m0_clovis_op_transaction_committed(&oc);
	m0_sm_group_unlock(grp);

	/* finalise clovis */
	m0_clovis_entity_fini(&ent);
	ut_m0_clovis_fini(&instance);
}

static void ut_clovis_test_m0_clovis_op_transaction_failed(void)
{
	struct m0_clovis_op_common oc;
	struct m0_clovis          *instance = NULL;
	struct m0_clovis_entity    ent;
	struct m0_clovis_realm     realm;
	struct m0_sm_group *grp;

	/* init */
	ut_m0_clovis_init(&instance);
	ut_clovis_realm_entity_setup(&realm, &ent, instance);
	ut_clovis_init_fake_op(&oc, &ent, &ut_clovis_launch_cb_pass);

	/* Test m0_clovis_op_transaction_commited. */
	grp = &oc.oc_op.op_sm_group;
	m0_sm_group_lock(grp);
	oc.oc_op.op_sm.sm_state = M0_CLOVIS_OS_EXECUTED;
	m0_clovis_op_transaction_failed(&oc);
	m0_sm_group_unlock(grp);

	/* finalise clovis */
	m0_clovis_entity_fini(&ent);
	ut_m0_clovis_fini(&instance);
}
#endif

#ifndef __KERNEL__
int rand(void);
struct m0_ut_suite ut_suite_clovis;


M0_INTERNAL void
ut_clovis_shuffle_test_order(struct m0_ut_suite *suite)
{
#ifdef CLVIS_UT_ENABLE_SHUFFLE

	int            i;
	int            num_tests;
	struct m0_ut **new_order;
	struct m0_ut  *real_order = (struct m0_ut *)suite->ts_tests;

	/* Count the tests */
	num_tests = 0;
	while (suite->ts_tests[num_tests].t_name != NULL)
		num_tests++;

	M0_ALLOC_ARR(new_order, num_tests + 1);
	M0_ASSERT(new_order != NULL);

	/* Produce a new order */
	for (i = 0; i < num_tests; i++) {
		int index = rand() % num_tests;

		while (new_order[index] != NULL)
			index = (index + 1) % num_tests;

		M0_ALLOC_PTR(new_order[index]);
		M0_ASSERT(new_order[index] != NULL);

		*new_order[index] = suite->ts_tests[i];
	}

	/* Re-write the test order */
	for (i = 0; i < num_tests; i++){
		real_order[i] = *new_order[i];
		m0_free(new_order[i]);
	}
	m0_free(new_order);
#endif
}
#endif

M0_INTERNAL int ut_clovis_init(void)
{

#ifndef __KERNEL__
	ut_clovis_shuffle_test_order(&ut_suite_clovis);
#endif

	m0_fi_enable("m0_clovis__obj_pool_version_get", "fake_pool_version");

	return 0;
}

M0_INTERNAL int ut_clovis_fini(void)
{
	m0_fi_disable("m0_clovis__obj_pool_version_get", "fake_pool_version");

	return 0;
}

struct m0_ut_suite ut_suite_clovis = {
	.ts_name = "clovis-ut",
	.ts_init = ut_clovis_init,
	.ts_fini = ut_clovis_fini,
	.ts_tests = {

		/* Initialising clovis. */
		{ "m0_clovis_init", &ut_clovis_test_m0_clovis_init},
		{ "m0_clovis_fini", &ut_clovis_test_m0_clovis_fini},
		{ "clovis op state machine",
			&ut_clovis_test_op_sm},

		/* Operations. */
		{ "m0_clovis_op_alloc",
			&ut_clovis_test_m0_clovis_op_alloc},
		{ "m0_clovis_op_init",
			&ut_clovis_test_m0_clovis_op_init},
		{ "m0_clovis_op_launch_one",
			&ut_clovis_test_m0_clovis_op_launch_one},
		{ "m0_clovis_op_launch",
			&ut_clovis_test_m0_clovis_op_launch},
		{ "m0_clovis_op_fini",
			&ut_clovis_test_m0_clovis_op_fini},
		{ "m0_clovis_op_free",
			&ut_clovis_test_m0_clovis_op_free},

		/* Entities */
		{ "m0_clovis_entity_init",
			&ut_clovis_test_m0_clovis_entity_init},
		{ "m0_clovis_obj_init",
			&ut_clovis_test_m0_clovis_obj_init},
		{ "m0_clovis_entity_fini",
			&ut_clovis_test_m0_clovis_entity_fini},
		{ "m0_clovis_obj_fini",
			&ut_clovis_test_m0_clovis_obj_fini},
		{ "clovis_entity_invariant_locked",
			&ut_clovis_test_clovis_entity_invariant_locked},
		{ "m0_clovis_op_setup",
			&ut_clovis_test_m0_clovis_op_setup},
		{ "m0_clovis_op_wait",
			&ut_clovis_test_m0_clovis_op_wait},
		{ "m0_clovis_op_kick",
			&ut_clovis_test_m0_clovis_op_kick},
#if 0
		{ "m0_clovis_op_transaction_committed",
			&ut_clovis_test_m0_clovis_op_transaction_committed},
		{ "m0_clovis_op_transaction_failed",
			&ut_clovis_test_m0_clovis_op_transaction_failed},
#endif
		{ NULL, NULL }
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
