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

 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact

 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 07/20/2010
 */

#pragma once

#ifndef __MERO_LIB_UB_H__
#define __MERO_LIB_UB_H__

#include "lib/types.h"
#include "lib/time.h"
#include "lib/assert.h"

/**
   @defgroup ub Unit Benchmarking.

   @{
 */

#define M0_UB_ASSERT(cond)  M0_ASSERT(cond)

/**
 * Structure to define a unit benchmark.
 */
struct m0_ub_bench {
	/** Name of a benchmark, has to be unique in current unit set. */
	const char *ub_name;
	/** Number of iterations the benchmarks has to be run. */
	uint32_t    ub_iter;
	uint32_t    ub_block_size;
	uint32_t    ub_blocks_per_op;
	/** Pointer to the benchmark function. */
	void      (*ub_round)(int iter);
	/** Function to prepare benchmark. */
	void      (*ub_init)(void);
	/** Function to free benchmark. */
	void      (*ub_fini)(void);

	/* Fields used privately in the implementation: */

	/** Total seconds elapsed on all ->ub_round()s. */
	double      ub_total;
	m0_time_t   ub_total_etime;
	/** Total squared seconds. */
	double      ub_square;
	/** Minimum number of seconds spent in ->ub_round(). */
	double      ub_min;
	/** Maximum number of seconds spent in ->ub_round(). */
	double      ub_max;
};

enum { M0_UB_SET_BENCHMARKS_MAX = 32 };

/**
 * Structure to define a set of unit benchmarks.
 */
struct m0_ub_set {
	/** Name of a set, has to be unique. */
	const char        *us_name;

	/**
	 * Function to prepare tests in set.
	 *
	 * @param opts  Optional string with benchmark arguments,
	 *              specified via `-o' CLI option.
	 *
	 * @see ub_args_parse()
	 */
	int              (*us_init)(const char *opts);

	/** Function to free tests in set. */
	void             (*us_fini)(void);

	struct m0_ub_set  *us_prev;

	/** Benchmarks in the set. */
	struct m0_ub_bench us_run[M0_UB_SET_BENCHMARKS_MAX];
};

/**
 * Prints names of all available benchmarks.
 */
M0_INTERNAL void m0_ub_set_print(void);

/**
 * Selects one unit set from the unit benchmark to run.
 *
 * @param name Name of the unit set to run.
 *
 * @retun 0       Unit set has been selected successfully.
 * @retun -ENOENT Unit set with given name is not found.
 */
M0_INTERNAL int m0_ub_set_select(const char *name);

/**
 * Adds given unit set into the unit benchmark.
 */
M0_INTERNAL void m0_ub_set_add(struct m0_ub_set *set);

/**
 * Runs unit sets unit benchmark consists of.
 *
 * @param rounds Number of times every set in benchark has to be run.
 * @param opts   Optional string with benchmark arguments, specified
 *               via `-o' CLI option.
 *
 * @see ub_args_parse()
 */
M0_INTERNAL int m0_ub_run(uint32_t rounds, const char *opts);

/** @} end of ub group. */
#endif /* __MERO_LIB_UB_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
