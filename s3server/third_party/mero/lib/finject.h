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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 02/22/2012
 */

#pragma once

#ifndef __MERO_LIB_FINJECT_H__
#define __MERO_LIB_FINJECT_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"     /* ENABLE_FAULT_INJECTION */
#endif

#include "lib/types.h"
#include "lib/assert.h"   /* M0_PRE */

/**
 * @defgroup finject Fault Injection
 *
 * @brief Fault Injection API provides functions to set "fault points" inside
 * the code, and functions to enable/disable the failure of those points. It's
 * aimed at increasing code coverage by enabling execution of error-handling
 * code paths, which are not covered otherwise by unit tests.
 *
 * @{
 */

/**
 * Initializes fault injection subsystem.
 */
M0_INTERNAL int m0_fi_init(void);

/**
 * Finalizes fault injection subsystem.
 */
M0_INTERNAL void m0_fi_fini(void);

/**
 * Prints to stdout information about current state of fault points formatted as
 * table.
 */
M0_INTERNAL void m0_fi_print_info(void);

struct m0_fi_fpoint_state;

/**
 * Holds information about "fault point" (FP).
 */
struct m0_fi_fault_point {
	/** Subsystem/module name */
	const char                 *fp_module;
	/** File name */
	const char                 *fp_file;
	/** Line number */
	uint32_t                   fp_line_num;
	/** Function name */
	const char                 *fp_func;
	/**
	 * Tag - one or several words, separated by underscores, aimed to
	 * describe a purpose of the fault point and uniquely identify this FP
	 * within a current function
	 */
	const char                 *fp_tag;
	/**
	 * Reference to the "state" structure, which holds information about
	 * current state of fault point (e.g. enabled/disabled, triggering
	 * algorithm, FP data, etc.)
	 */
	struct m0_fi_fpoint_state  *fp_state;
};

/**
 * Fault point types, which determine FP behavior in enabled state.
 */
enum m0_fi_fpoint_type {
	/** Always triggers when enabled */
	M0_FI_ALWAYS,
	/** Triggers only on first hit, then becomes disabled automatically */
	M0_FI_ONESHOT,
	/** Triggers with a given probability */
	M0_FI_RANDOM,
	/**
	 * Doesn't trigger first N times, then triggers next M times, then
	 * repeats this cycle
	 */
	M0_FI_OFF_N_ON_M,
	/**
	 * Invokes a user-supplied callback of type m0_fi_fpoint_state_func_t
	 * to determine if FP should trigger or not
	 */
	M0_FI_FUNC,
	/* Not valid FP type (used by internal API) */
	M0_FI_INVALID_TYPE,
	/** Number of fault point types */
	/* this should be the last  */
	M0_FI_TYPES_NR
};

/**
 * A prototype of user-supplied callback for M0_FI_FUNC fault points, which is
 * used to to determine if FP should trigger or not.
 */
typedef bool (*m0_fi_fpoint_state_func_t)(void *data);

/**
 * Contains information, which controls fault point's behavior in enabled state,
 * depending on FP type.
 */
struct m0_fi_fpoint_data {
	/**
	 * Fault point type, it determines which field of the following union
	 * are relevant and contains a meaningful data
	 */
	enum m0_fi_fpoint_type    fpd_type;
	union {
		struct {
			/**
			 * Used for M0_FI_OFF_N_ON_M fault points, means
			 * 'skip triggering N times in a row and then trigger M
			 * times in a row' (for M0_FI_OFF_N_ON_M type)
			 */
			uint32_t  fpd_n;
			/**
			 * Used for M0_FI_OFF_N_ON_M fault points, means how
			 * many times in a row to trigger FP, after skipping it
			 * N times before
			 */
			uint32_t  fpd_m;
			/**
			 * Internal counter for M0_FI_OFF_N_ON_M triggering
			 * algorithms, not intended to be accessed by user code
			 */
			uint32_t  fpd___n_cnt;
			/**
			 * Internal counter for M0_FI_OFF_N_ON_M and
			 * triggering algorithms, not intended to be accessed by
			 * user code
			 */
			uint32_t  fpd___m_cnt;
		} s1;
		struct {
			/** User-supplied triggering function */
			m0_fi_fpoint_state_func_t  fpd_trigger_func;
			/**
			 * Pointer to store user's private data, which can be
			 * accessed from user-supplied triggering function
			 */
			void                       *fpd_private;
		} s2;
		/**
		 * Probability with which FP is triggered (for M0_FI_RANDOM
		 * type), it's an integer number in range [1..100], which means
		 * a probability in percents, with which FP should be triggered
		 * on each hit
		 */
		uint32_t          fpd_p;
	} u;
	/**
	 * Counter of how many times (since last enable) fault point was
	 * checked/hit
	 */
	uint32_t                  fpd_hit_cnt;
	/**
	 * Counter of how many times (since last enable) fault point was
	 * triggered
	 */
	uint32_t                  fpd_trigger_cnt;
};

#ifdef ENABLE_FAULT_INJECTION

/**
 * Defines a fault point and checks if it's enabled.
 *
 * FP registration occurs only once, during first time when this macro is
 * "executed". m0_fi_register() is used to register FP in a global dynamic list,
 * which may introduce some delay if this list already contains large amount of
 * registered fault points.
 *
 * A typical use case for this macro is:
 *
 * @code
 *
 * void *m0_alloc(size_t n)
 * {
 *	...
 *	if (M0_FI_ENABLED("pretend_failure"))
 *		return NULL;
 *	...
 * }
 *
 * @endcode
 *
 * It creates a fault point with tag "pretend_failure" in function "m0_alloc",
 * which can be enabled/disabled from external code with something like the
 * following:
 *
 * @code
 *
 * m0_fi_enable_once("m0_alloc", "pretend_failure");
 *
 * @endcode
 *
 * @see m0_fi_enable_generic() for more details
 *
 * @param tag short descriptive name of fault point, usually separated by
 *            and uniquely identifies this FP within a current
 *            function
 *
 * @return    true, if FP is enabled
 * @return    false otherwise
 */
#define M0_FI_ENABLED(tag)				\
({							\
	static struct m0_fi_fault_point fp = {		\
		.fp_state    = NULL,			\
 /* TODO: add some macro to automatically get name of current module */ \
		.fp_module   = "UNKNOWN",		\
		.fp_file     = __FILE__,		\
		.fp_line_num = __LINE__,		\
		.fp_func     = __func__,		\
		.fp_tag      = (tag),			\
	};						\
							\
	if (unlikely(fp.fp_state == NULL)) {		\
		m0_fi_register(&fp);			\
		M0_ASSERT(fp.fp_state != NULL);		\
	}						\
							\
	m0_fi_enabled(fp.fp_state);			\
})

/**
 * Enables fault point, which identified by "func", "tag" pair.
 *
 * It's not intended to be used on it's own, a set of m0_fi_enable_xxx() wrapper
 * functions should be used instead.
 *
 * @param fp_func Name of function, which contains a target FP
 * @param fp_tag  FP tag, which was specified as a parameter to M0_FI_ENABLED()
 * @param fp_type Specifies a type of "triggering algorithm"
 *                (@see enum m0_fi_fpoint_type)
 * @param fp_data Parameters for "triggering algorithm", which controls FP
 *                behavior (@see struct m0_fi_fpoint_data)
 */
M0_INTERNAL void m0_fi_enable_generic(const char *fp_func, const char *fp_tag,
				      const struct m0_fi_fpoint_data *fp_data);

/**
 * Enables fault point, which identified by "func", "tag" pair, using
 * M0_FI_ALWAYS FP type.
 *
 * @param func Name of function, which contains the target FP
 * @param tag  FP tag, which was specified as a parameter to M0_FI_ENABLED()
 *
 * @see m0_fi_enable_generic() and m0_fi_fpoint_type for more details
 */
static inline void m0_fi_enable(const char *func, const char *tag)
{
/*
 * Our headers are processed by gccxml, which is essentially a C++
 * compiler, and it doesn't support C99 compound literals and designated
 * initializers. So we want to hide this fancy code from gccxml to protect it
 * from brain damage :D It's safe, because we are interested only in type
 * definition information, produced by gccxml, and not a function
 * implementation.
 */
#if !defined(__cplusplus)
	m0_fi_enable_generic(func, tag, &(const struct m0_fi_fpoint_data){
						.fpd_type = M0_FI_ALWAYS });
#endif
}

/**
 * Enables fault point, which identified by "func", "tag" pair, using
 * M0_FI_ONESHOT FP type.
 *
 * @param func Name of function, which contains the target FP
 * @param tag  FP tag, which was specified as a parameter to M0_FI_ENABLED()
 *
 * @see m0_fi_enable_generic() and m0_fi_fpoint_type for more details
 */
static inline void m0_fi_enable_once(const char *func, const char *tag)
{
/* See the comment inside m0_fi_enable() about purpose of this C++ guard */
#if !defined(__cplusplus)
	m0_fi_enable_generic(func, tag, &(const struct m0_fi_fpoint_data){
						.fpd_type = M0_FI_ONESHOT });
#endif
}

/**
 * Enables fault point, which identified by "func", "tag" pair, using
 * M0_FI_RANDOM FP type.
 *
 * @param func Name of function, which contains the target FP
 * @param tag  FP tag, which was specified as a parameter to M0_FI_ENABLED()
 * @param p    Integer number in range [1..100], which means a probability in
 *             percents, with which FP should be triggered on each hit
 *
 * @see m0_fi_enable_generic() and m0_fi_fpoint_data for more details
 */
static inline void m0_fi_enable_random(const char *func, const char *tag,
				       uint32_t p)
{
/* See the comment inside m0_fi_enable() about purpose of this C++ guard */
#if !defined(__cplusplus)
	m0_fi_enable_generic(func, tag, &(const struct m0_fi_fpoint_data){
						.fpd_type = M0_FI_RANDOM,
						.u = { .fpd_p = p } });
#endif
}

/**
 * Enables fault point, which identified by "func", "tag" pair, using
 * M0_FI_OFF_N_ON_M FP type.
 *
 * @param func Name of function, which contains the target FP
 * @param tag  FP tag, which was specified as a parameter to M0_FI_ENABLED()
 * @param n    Integer values, used as initialized for fpd_n field of
 *             m0_fi_fpoint_data structure
 * @param m    Integer values, used as initialized for fpd_m field of
 *             m0_fi_fpoint_data structure
 *
 * @see m0_fi_enable_generic() and m0_fi_fpoint_data/m0_fi_fpoint_type for more
 * details
 */
static inline void m0_fi_enable_off_n_on_m(const char *func, const char *tag,
					   uint32_t n, uint32_t m)
{
/* See the comment inside m0_fi_enable() about purpose of this C++ guard */
#if !defined(__cplusplus)
	m0_fi_enable_generic(func, tag,
			&(const struct m0_fi_fpoint_data){
				.fpd_type = M0_FI_OFF_N_ON_M,
				.u = { .s1 = { .fpd_n = n, .fpd_m = m } } });
#endif
}

/**
 * A wrapper around m0_fi_enable_off_n_on_m() for a special case when N=n-1
 * and M=1, which simply means to trigger FP each n-th time.
 *
 * @param func Name of function, which contains the target FP
 * @param tag  FP tag, which was specified as a parameter to M0_FI_ENABLED()
 * @param n    A "frequency" with which FP is triggered
 */
static inline void m0_fi_enable_each_nth_time(const char *func, const char *tag,
					      uint32_t n)
{
	M0_PRE(n > 0);
	m0_fi_enable_off_n_on_m(func, tag, n - 1, 1);
}

/**
 * Enables fault point, which identified by "func", "tag" pair, using
 * M0_FI_FUNC FP type.
 *
 * @param func         Name of function, which contains the target FP
 * @param tag          FP tag, which was specified as a parameter to
 *                     M0_FI_ENABLED()
 * @param trigger_func Pointer to a user-supplied triggering function
 * @param data         Pointer to store user's private data, which can be
 *                     accessed from user-supplied triggering function
 *
 * @see m0_fi_enable_generic() and m0_fi_fpoint_data/m0_fi_fpoint_type for more
 * details
 */
static inline void m0_fi_enable_func(const char *func, const char *tag,
				     m0_fi_fpoint_state_func_t trigger_func,
				     void *data)
{
/* See the comment inside m0_fi_enable() about purpose of this C++ guard */
#if !defined(__cplusplus)
	m0_fi_enable_generic(func, tag,
			&(const struct m0_fi_fpoint_data){
				.fpd_type = M0_FI_FUNC,
				.u = { .s2 = {
					  .fpd_trigger_func = trigger_func,
					  .fpd_private = data
				       }
				 } });
#endif
}

/**
 * Disables fault point, which identified by "func", "tag" pair.
 *
 * @param fp_func Name of function, which contains a target FP
 * @param fp_tag  FP tag, which was specified as a parameter to M0_FI_ENABLED()
 */
M0_INTERNAL void m0_fi_disable(const char *fp_func, const char *fp_tag);

/**
 * Registers fault point in a global list.
 *
 * It's not intended to be used on it's own, instead it's used as part of
 * M0_FI_ENABLED() macro.
 *
 * @param fp A fault point descriptor
 *
 * @see M0_FI_ENABLED() for more information
 */
void m0_fi_register(struct m0_fi_fault_point *fp);

/**
 * Checks if fault point should "trigger" or not.
 *
 * It's not intended to be used on it's own, instead it's used as part of
 * M0_FI_ENABLED() macro.
 *
 * @param fps A pointer to fault point's state structure, which is linked with
 *            FP's "descriptor"
 *
 * @see M0_FI_ENABLED() for more information
 *
 * @return    true, if FP is enabled
 * @return    false otherwise
 */
bool m0_fi_enabled(struct m0_fi_fpoint_state *fps);

M0_INTERNAL enum m0_fi_fpoint_type m0_fi_fpoint_type_from_str(const char
							      *type_name);

#else /* ENABLE_FAULT_INJECTION */
#define M0_FI_ENABLED(tag)                               (false)

static inline void m0_fi_enable_generic(const char *fp_func, const char *fp_tag,
					const struct m0_fi_fpoint_data *fp_data)
{
}

static inline void m0_fi_enable(const char *func, const char *tag)
{
}

static inline void m0_fi_enable_once(const char *func, const char *tag)
{
}

static inline void m0_fi_enable_random(const char *func, const char *tag,
				       uint32_t p)
{
}

static inline void m0_fi_enable_each_nth_time(const char *func, const char *tag,
					      uint32_t n)
{
}

static inline void m0_fi_enable_off_n_on_m(const char *func, const char *tag,
					   uint32_t n, uint32_t m)
{
}

static inline void m0_fi_enable_func(const char *func, const char *tag,
				     m0_fi_fpoint_state_func_t trigger_func,
				     void *data)
{
}

static inline void m0_fi_disable(const char *fp_func, const char *fp_tag)
{
}

static inline void m0_fi_register(struct m0_fi_fault_point *fp)
{
}

static inline bool m0_fi_enabled(struct m0_fi_fpoint_state *fps)
{
	return false;
}

#endif /* ENABLE_FAULT_INJECTION */
M0_INTERNAL int m0_fi_enable_fault_point(const char *str);
M0_INTERNAL int m0_fi_enable_fault_points_from_file(const char *file_name);
/** @} end of finject group */
#endif /* __MERO_LIB_FINJECT_H__ */

