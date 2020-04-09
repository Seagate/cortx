/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 26-Oct-2014
 */
#pragma once
#ifndef __MERO_UT_MODULE_H__
#define __MERO_UT_MODULE_H__

#include "module/module.h"
#include "lib/atomic.h"     /* m0_atomic64 */

/**
 * @addtogroup ut
 *
 * @{
 */

/** Represents a (module, level) pair, which a UT suite depends on. */
struct m0_ut_moddep {
	struct m0_module *ud_module; /* XXX FIXME: Use enum m0_module_id. */
	int               ud_level;
};

enum { M0_UT_SUITES_MAX = 1024 };

struct m0_ut_module {
	struct m0_module    ut_module;
	/**
	 * Specifies the list of tests to run (ut_exclude is false)
	 * or to be excluded from testing (ut_exclude is true).
	 *
	 * Format: suite[:test][,suite[:test]]
	 */
	const char         *ut_tests;
	/**
	 * Whether ->ut_tests should be excluded from testing.
	 * @pre ergo(m->ut_exclude, m->ut_tests != NULL)
	 */
	bool                ut_exclude;
	/** Name of UT sandbox directory. */
	const char         *ut_sandbox;
	/** Whether to keep sandbox directory after UT execution. */
	bool                ut_keep_sandbox;
	bool                ut_small_credits;
	struct m0_ut_suite *ut_suites[M0_UT_SUITES_MAX];
	unsigned            ut_suites_nr;
	struct m0_atomic64  ut_asserts;
};

/** Levels of m0_ut_module. */
enum {
	/** Creates sandbox directory. */
	M0_LEVEL_UT_PREPARE,
	/**
	 * XXX DELETEME
	 * Registers dummy service types that are used by some UTs.
	 */
	M0_LEVEL_UT_KLUDGE,
	/** Depends on M0_LEVEL_UT_SUITE_READY of the used test suites. */
	M0_LEVEL_UT_READY
};

/** Levels of m0_ut_suite module. */
enum { M0_LEVEL_UT_SUITE_READY };

/*
 *          m0_ut_suite                      m0_ut_module
 *         +-------------------------+ *  1 +---------------------+
 * [<-----]| M0_LEVEL_UT_SUITE_READY |<-----| M0_LEVEL_UT_READY   |
 *         +-------------------------+      +---------------------+
 *                                          | M0_LEVEL_UT_PREPARE |
 *                                          +---------------------+
 */
/* XXX DELETEME */
M0_INTERNAL void m0_ut_suite_module_setup(struct m0_ut_suite *ts,
					  struct m0 *instance);

/*
 *  m0_ut_module                 m0
 * +---------------------+      +--------------------------+
 * | M0_LEVEL_UT_READY   |<-----| M0_LEVEL_INST_READY      |
 * +---------------------+      +--------------------------+
 * | M0_LEVEL_UT_KLUDGE  |   ,--| M0_LEVEL_INST_SUBSYSTEMS |
 * +---------------------+   |  +--------------------------+
 * | M0_LEVEL_UT_PREPARE |<--'  | M0_LEVEL_INST_ONCE       |
 * +---------------------+      +--------------------------+
 *                              | M0_LEVEL_INST_PREPARE    |
 *                              +--------------------------+
 */
extern const struct m0_module_type m0_ut_module_type;

/** @} ut */
#endif /* __MERO_UT_MODULE_H__ */
