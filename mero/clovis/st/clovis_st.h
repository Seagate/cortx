/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 20-Oct-2014
 */

#pragma once

#ifndef __MERO_CLOVIS_ST_H__
#define __MERO_CLOVIS_ST_H__

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "module/instance.h"

#ifdef __KERNEL__
#include <asm-generic/errno-base.h>
#else
#include <errno.h>
#endif

/**
 * @defgroup Clovis ST framework
 *
 * A simple system test framework for Clovis which supports:
 * (1) Test suites can be run as a whole or partially
 * (2) multiple test threads:
 *       - currently, only support same clovis instance for all threads
 *       - The thread main loop is controlled by either the number of test
 *         rounds or time (can be forever)
 *       - fixed number of threads only (configurable)
 * (3) speed control:
 *       - fixed test rate (useful for load test)
 *       - automatic load test mode: increase test rate till it reaches peak.
 * (4) Tests from multi suites can be mixed.
 *
 * Why not use existing mero ut?(1) to simulate a "real" Clovis app environment
 * in which only m0mero is available (2) to support those features above.
 *
 * @{
 * */

enum {
	CLOVIS_ST_MAX_WORKER_NUM = 32
};

enum {
	DEFAULT_PARGRP_UNIT_SIZE     = 4096,
	DEFAULT_PARGRP_DATA_UNIT_NUM = 3,
	DEFAULT_PARGRP_DATA_SIZE     = DEFAULT_PARGRP_UNIT_SIZE *
				       DEFAULT_PARGRP_DATA_UNIT_NUM
};

/*
 * structure to define test in test suite.
 */
struct clovis_st_test {
	/** name of the test, must be unique */
	const char *st_name;
	/** pointer to testing procedure */
	void      (*st_proc)(void);

	/* a test is selected in a CLOVIS SELECTED MODE */
	int	    st_enabled;
};

struct clovis_st_suite {
	/** name of a suite */
	const char           *ss_name;

	/** functions for suite */
	int                 (*ss_init)(void);
	int                 (*ss_fini)(void);

	/** tests in suite */
	struct clovis_st_test ss_tests[];
};

/**
 * Configuration parameters for Clovis ST
 */
enum clovis_st_mode {
	CLOVIS_ST_SEQ_MODE = 0,
	CLOVIS_ST_RAND_MODE,
	CLOVIS_ST_MIXED_MODE
};

struct clovis_st_cfg {
	/*number of test threads*/
	int                 sc_nr_threads;

	/*if this flag is set, only specified tests are run*/
	int                 sc_run_selected;
	const char         *sc_tests;

	/**
	 * number of test rounds and expected completion time. Test
	 * ends when it reaches either of them.
	 */
	int                 sc_nr_rounds;
	uint64_t            sc_deadline;

	/* test mode, see above*/
	enum clovis_st_mode sc_mode;

	/**
	 * control how fast we issue each single test
	 *   -  0: forward as fast as it can
	 *   - -n: automatically adjust the pace to find the saturation point
	 *   - +n: speed limit (tests/s)
	 */
	int                 sc_pace;
};

/*
 * Clovis related details
 */
struct clovis_instance {
	char		 *si_laddr;
	char		 *si_ha_addr;
	char		 *si_confd_addr;
	char		 *si_prof;

	struct m0_clovis *si_instance;
};

/**
 * Test statistics data (thread wise)
 */
struct clovis_st_worker_stat {
	int64_t sws_nr_asserts;
	int64_t sws_nr_failed_asserts;
};

struct clovis_st_ctx {
	struct clovis_st_cfg            sx_cfg;

	/* Clovis instance */
	struct clovis_instance          sx_instance;

	/* Test suites */
	int                             sx_nr_all;
	int                             sx_nr_selected;
	struct clovis_st_suite        **sx_all;
	struct clovis_st_suite        **sx_selected;

	/* Worker thread ID */
	pid_t                          *sx_worker_tids;

	/* Statistics data */
	struct clovis_st_worker_stat   *sx_worker_stats;

	/* Maximum length of test name (used for output)*/
	int                             sx_max_name_len;
};

enum {
	CLOVIS_ST_MAX_SUITE_NUM = 4096
};

/**
 * Return Mero instance.
 */
struct m0* clovis_st_get_mero(void);

/**
 * Setter and getter for Clovis instance
 */
void clovis_st_set_instance(struct m0_clovis *instance);
struct m0_clovis* clovis_st_get_instance(void);

/**
 * Start runing all Clovis test suites.
 *
 * @param test_list_str: By default, all tests are included unless a string
 * is provided to specify those selected testss.
 * @return 0 for success
 *
 */
int clovis_st_run(const char *test_list_str);

/**
 * Register all test suite
 */
void clovis_st_add_suites(void);

/**
 * List avaiable test suites and tests inside each suite.
 */
void clovis_st_list(bool);

/**
 * Initialise and finalise clovis ST
 */
int clovis_st_init(void);
void clovis_st_fini(void);

/**
 * Clovis ST assert to allow to collect test failure statistics
 */
bool clovis_st_assertimpl(bool c, const char *str_c, const char *file,
			  int lno, const char *func);

/**
 * Retrieve Clovis ST configuration information.
 */
struct clovis_st_cfg clovis_st_get_cfg(void);
struct clovis_st_worker_stat* clovis_st_get_worker_stat(int idx);

/**
 * Getter and setter for number of workers
 */
int  clovis_st_get_nr_workers(void);
void clovis_st_set_nr_workers(int nr);

/**
 * Getter and setter of selected tests
 */
void         clovis_st_set_tests(const char *);
const char*  clovis_st_get_tests(void);

/**
 * Getter and setter of test mode
 */
void                clovis_st_set_test_mode(enum clovis_st_mode);
enum clovis_st_mode clovis_st_get_test_mode(void);


/**
 * Set the tid of a worker thread
 */
int clovis_st_set_worker_tid(int idx, pid_t tid);
int clovis_st_get_worker_idx(pid_t tid);

/**
 * Start and stop worker threads for tests
 */
int clovis_st_start_workers(void);
int clovis_st_stop_workers(void);

/**
 * Object ID allocation and release
 */
int clovis_oid_get(struct m0_uint128 *oid);
void clovis_oid_put(struct m0_uint128 oid);
uint64_t clovis_oid_get_many(struct m0_uint128 *oids, uint64_t nr_oids);
void clovis_oid_put_many(struct m0_uint128 *oids, uint64_t nr_oids);

int clovis_oid_allocator_init(void);
int clovis_oid_allocator_fini(void);

/* Wrappers for Clovis APIs*/
void clovis_st_container_init(struct m0_clovis_container *con,
			      struct m0_clovis_realm     *parent,
			      const struct m0_uint128    *id,
			      struct m0_clovis           *instance);
void clovis_st_obj_init(struct m0_clovis_obj *obj,
			struct m0_clovis_realm  *parent,
			const struct m0_uint128 *id, uint64_t layout_id);
void clovis_st_obj_fini(struct m0_clovis_obj *obj);
int clovis_st_entity_create(struct m0_fid *pool,
			    struct m0_clovis_entity *entity,
			    struct m0_clovis_op **op);
int clovis_st_entity_delete(struct m0_clovis_entity *entity,
			 struct m0_clovis_op **op);
void clovis_st_entity_fini(struct m0_clovis_entity *entity);
void clovis_st_op_launch(struct m0_clovis_op **op, uint32_t nr);
int32_t clovis_st_op_wait(struct m0_clovis_op *op, uint64_t bits, m0_time_t to);
void clovis_st_op_fini(struct m0_clovis_op *op);
void clovis_st_op_free(struct m0_clovis_op *op);
void clovis_st_entity_open(struct m0_clovis_entity *entity);
void clovis_st_idx_open(struct m0_clovis_entity *entity);
void clovis_st_obj_op(struct m0_clovis_obj       *obj,
		      enum m0_clovis_obj_opcode   opcode,
		      struct m0_indexvec         *ext,
		      struct m0_bufvec           *data,
		      struct m0_bufvec           *attr,
		      uint64_t                    mask,
		      struct m0_clovis_op       **op);

void clovis_st_idx_init(struct m0_clovis_idx *idx,
		     struct m0_clovis_realm  *parent,
		     const struct m0_uint128 *id);
void clovis_st_idx_fini(struct m0_clovis_idx *idx);
int clovis_st_idx_op(struct m0_clovis_idx       *idx,
		     enum m0_clovis_idx_opcode   opcode,
		     struct m0_bufvec           *keys,
		     struct m0_bufvec           *vals,
		     int                        *rcs,
		     int                         flag,
		     struct m0_clovis_op       **op);

int clovis_st_layout_op(struct m0_clovis_obj *obj,
			enum m0_clovis_entity_opcode opcode,
			struct m0_clovis_layout *layout,
			struct m0_clovis_op **op);

/* Allocate aligned memory - user/kernel specific */
void clovis_st_alloc_aligned(void **ptr, size_t size, size_t alignment);
void clovis_st_free_aligned(void *ptr, size_t size, size_t alignment);

/**
 *	@} clovis ST end groud
 */

#endif /* __MERO_CLOVIS_ST_H__ */
