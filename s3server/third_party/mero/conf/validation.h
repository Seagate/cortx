/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 6-Jan-2016
 */
#pragma once
#ifndef __MERO_CONF_VALIDATION_H__
#define __MERO_CONF_VALIDATION_H__

#include "lib/types.h"  /* bool */

struct m0_conf_cache;
struct m0_conf_obj;
struct m0_fid;

/**
 * @defgroup conf_validation
 *
 * Mero subsystems that use confc API (m0t1fs, m0d, ioservice, &c.)
 * have certain expectations of the configuration objects they work with.
 * Subsystem developers specify these expectations in the form of "rules",
 * which valid configuration data should conform to.
 *
 * @{
 */

/**
 * Performs semantic validation of the DAG of configuration objects.
 *
 * If m0_conf_validation_error() finds a problem with configuration
 * data, it returns a pointer to a string that describes the problem.
 * This may be either a pointer to a string that the function stores
 * in `buf', or a pointer to some (imutable) static string (in which
 * case `buf' is unused).  If the function stores a string in `buf',
 * then at most `buflen' bytes are stored (the string may be truncated
 * if `buflen' is too small).  The string always includes a terminating
 * null byte ('\0').
 *
 * If no issues with configuration data are found, m0_conf_validation_error()
 * returns NULL.
 *
 * @pre  buf != NULL && buflen != 0
 */
char *m0_conf_validation_error(struct m0_conf_cache *cache,
			       char *buf, size_t buflen);

/**
 * Similar to m0_conf_validation_error(), but requires conf cache to be locked.
 *
 * @pre  buf != NULL && buflen != 0
 * @pre  m0_conf_cache_is_locked(cache)
 */
M0_INTERNAL char *m0_conf_validation_error_locked(
	const struct m0_conf_cache *cache, char *buf, size_t buflen);

/** Validation rule. */
struct m0_conf_rule {
	/*
	 * Use the name of the function that .cvr_error points at.
	 * This simplifies finding the rule that failed.
	 */
	const char *cvr_name;
	/**
	 * @see m0_conf_validation_error() for arguments' description.
	 *
	 * @pre  m0_conf_cache_is_locked(cache)
	 * (This precondition is enforced by m0_conf_validation_error().)
	 */
	char     *(*cvr_error)(const struct m0_conf_cache *cache,
			       char *buf, size_t buflen);
};

/** Maximal number of rules in a m0_conf_ruleset. */
enum { M0_CONF_RULES_MAX = 32 };

/** Named set of validation rules. */
struct m0_conf_ruleset {
	/*
	 * Use the name of m0_conf_ruleset variable. This simplifies
	 * finding the rule that failed.
	 */
	const char         *cv_name;
	/*
	 * This array must end with { NULL, NULL }.
	 */
	struct m0_conf_rule cv_rules[M0_CONF_RULES_MAX];
};

/** @} */
#endif /* __MERO_CONF_VALIDATION_H__ */

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
