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
 * Original creation date: 20-11-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#include "clovis/clovis.h"
#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"

struct m0_clovis_container clovis_st_example_container;
static struct m0_uint128 test_id;
static uint64_t layout_id;

/*
 * copy-cat of clovis_st_obj_delete_multiple just to show
 * how to write a more complicated test using new assert method.
 */
static void example_abitmorecomplicated(void)
{
	uint32_t                n_objs = 5;
	uint32_t                rounds = 20;
	uint32_t                max_ops = 4; /* max. ops in one launch() */
	struct m0_uint128      *ids = NULL;
	struct m0_clovis_obj   *objs = NULL;
	struct m0_clovis_op   **ops = NULL;
	bool                   *obj_exists = NULL;
	bool                   *already_chosen = NULL;
	uint32_t                i;
	uint32_t                j;
	uint32_t                idx;
	uint32_t                n_ops;
	int                     rc;


	M0_CLOVIS_THREAD_ENTER;

	/*
         * Don't worry about freeing memory, ST will do it for you
         * if you forget to do so.
         * Note: use clovis ST self-ownned MEM_XXX and mem_xxx rather
         * than mero stuff
         */
	MEM_ALLOC_ARR(ids, n_objs);
	CLOVIS_ST_ASSERT_FATAL(ids != NULL);
	MEM_ALLOC_ARR(objs, n_objs);
	CLOVIS_ST_ASSERT_FATAL(objs != NULL);
	MEM_ALLOC_ARR(obj_exists, n_objs);
	CLOVIS_ST_ASSERT_FATAL(obj_exists != NULL);
	MEM_ALLOC_ARR(already_chosen, n_objs);
	CLOVIS_ST_ASSERT_FATAL(already_chosen != NULL);

	MEM_ALLOC_ARR(ops, max_ops);
	CLOVIS_ST_ASSERT_FATAL(ops != NULL);
	for (i = 0; i < max_ops; i++)
		ops[i] = NULL;

	/*
	 * Initialise the objects. Closed set.
	 *
	 * Note: use clovis api wrapper (clovis_xxx) instead of
	 * m0_clovis_xxx directly
	 */
	for (i = 0; i < n_objs; ++i)
		clovis_oid_get(ids + i);

	/* Repeat several rounds. */
	for (i = 0; i < rounds; ++i) {
		/* Each round, a different number of ops in the same launch. */
		n_ops = generate_random(max_ops) + 1;
		memset(already_chosen, 0, n_objs*sizeof(already_chosen[0]));

		/* Set the ops. */
		for (j = 0; j < n_ops; ++j) {
			/* Pick n_ops objects. */
			do {
				//XXX this is broken in kernel mode
				idx = generate_random(n_objs);
			} while(already_chosen[idx]);
			already_chosen[idx] = true;

			M0_SET0(&objs[idx]);
			clovis_st_obj_init(&objs[idx],
				&clovis_st_example_container.co_realm,
				&ids[idx], layout_id);
			if (obj_exists[idx]) {
				clovis_st_entity_open(&objs[idx].ob_entity);
				rc = clovis_st_entity_delete(
					&objs[idx].ob_entity, &ops[j]);
				obj_exists[idx] = false;
				CLOVIS_ST_ASSERT_FATAL(ops[j] != NULL);
			} else {
				rc = clovis_st_entity_create(
					NULL, &objs[idx].ob_entity, &ops[j]);
				obj_exists[idx] = true;
				CLOVIS_ST_ASSERT_FATAL(ops[j] != NULL);
			}
			CLOVIS_ST_ASSERT_FATAL(rc == 0);
			CLOVIS_ST_ASSERT_FATAL(ops[j] != NULL);
			CLOVIS_ST_ASSERT_FATAL(ops[j]->op_sm.sm_rc == 0);
		}

		/* Launch and check. */
		clovis_st_op_launch(ops, n_ops);

		for (j = 0; j < n_ops; ++j) {
			rc = clovis_st_op_wait(ops[j],
					       M0_BITS(M0_CLOVIS_OS_FAILED,
						       M0_CLOVIS_OS_STABLE),
					               M0_TIME_NEVER);
			CLOVIS_ST_ASSERT_FATAL(rc == 0);
			CLOVIS_ST_ASSERT_FATAL(
			    ops[j]->op_sm.sm_state == M0_CLOVIS_OS_STABLE
			    || ops[j]->op_sm.sm_state == M0_CLOVIS_OS_FAILED);
			clovis_st_op_fini(ops[j]);
		}
	}

	/* No operations should be pending at this point. */
	for (i = 0; i < max_ops; ++i)
		clovis_st_op_free(ops[i]);

	for (i = 0; i < n_objs; ++i)
		clovis_st_entity_fini(&objs[i].ob_entity);

	/*
	 * Release all the allocated structs. If you want, you can
	 *` ask ST to do the dirty job for you by skipping this step.
	 */
	if (ids != NULL)
		mem_free(ids);
	if (objs != NULL)
		mem_free(objs);
	if (obj_exists != NULL)
		mem_free(obj_exists);
	if (already_chosen != NULL)
		mem_free(already_chosen);
	if (ops != NULL)
		mem_free(ops);
}

/* copy-cat of clovis_st_obj_create_multiple */
static void example_simple(void)
{
	enum { CREATE_MULTIPLE_N_OBJS = 20 };
	uint32_t                i;
	struct m0_clovis_op    *ops[CREATE_MULTIPLE_N_OBJS] = {NULL};
	struct m0_clovis_obj   **objs = NULL;
	struct m0_uint128       ids[CREATE_MULTIPLE_N_OBJS];
	int                     rc;

	M0_CLOVIS_THREAD_ENTER;

	MEM_ALLOC_ARR(objs, CREATE_MULTIPLE_N_OBJS);
	CLOVIS_ST_ASSERT_FATAL(objs != NULL);
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		MEM_ALLOC_PTR(objs[i]);
		CLOVIS_ST_ASSERT_FATAL(objs[i] != NULL);
		memset(objs[i], 0, sizeof *objs[i]);
	}

	clovis_oid_get_many(ids, CREATE_MULTIPLE_N_OBJS);

	/* Create different objects. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		clovis_st_obj_init(objs[i],
			&clovis_st_example_container.co_realm,
			&ids[i], layout_id);
		rc = clovis_st_entity_create(NULL,
					     &objs[i]->ob_entity, &ops[i]);
		CLOVIS_ST_ASSERT_FATAL(rc == 0);
		CLOVIS_ST_ASSERT_FATAL(ops[i] != NULL);
		CLOVIS_ST_ASSERT_FATAL(ops[i]->op_sm.sm_rc == 0);
	}

	clovis_st_op_launch(ops, ARRAY_SIZE(ops));

	/* We wait for each op. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		rc = clovis_st_op_wait(ops[i], M0_BITS(M0_CLOVIS_OS_FAILED,
						    M0_CLOVIS_OS_STABLE),
				    time_from_now(5,0));
		CLOVIS_ST_ASSERT_FATAL(rc == 0 || rc == -ETIMEDOUT);
	}

	/* All correctly created. */
	for (i = 0; i < CREATE_MULTIPLE_N_OBJS; ++i) {
		clovis_st_op_fini(ops[i]);
		clovis_st_op_free(ops[i]);

		clovis_st_entity_fini(&objs[i]->ob_entity);
		mem_free(objs[i]);
	}

	mem_free(objs);
}

static int clovis_st_example_init(void)
{
	int rc = 0;

	/*
	 * Retrieve the uber realm. We don't need to open this,
	 * as realms are not actually implemented yet
	 */
	clovis_st_container_init(&clovis_st_example_container, NULL,
			 &M0_CLOVIS_UBER_REALM,
			 clovis_st_get_instance());
	rc = clovis_st_example_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0)
		console_printf("Failed to open uber realm\n");

	test_id = M0_CLOVIS_ID_APP;
	layout_id = m0_clovis_layout_id(clovis_st_get_instance());
	return rc;
}

static int clovis_st_example_fini(void)
{
	return 0;
}

struct clovis_st_suite st_suite_clovis_example = {
	.ss_name = "clovis_example_st",
	.ss_init = clovis_st_example_init,
	.ss_fini = clovis_st_example_fini,
	.ss_tests = {
		{ "example_simple",
		  &example_simple},
		{ "example_abitmorecomplicated",
		  &example_abitmorecomplicated},
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
