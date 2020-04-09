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
 * Original creation date: 25-Nov-2014
 */
#pragma once
#ifndef __MERO_MODULE_PARAM_H__
#define __MERO_MODULE_PARAM_H__

#include "lib/tlist.h"  /* m0_tlink */

/**
 * @addtogroup module
 *
 * m0_param API is used to configure initialisation: Mero modules use
 * m0_param_get() to obtain external information.
 *
 * @{
 */

/**
 * Obtains the value of parameter associated with given key.
 * Returns NULL if no such value exists or an error occurs.
 */
M0_INTERNAL void *m0_param_get(const char *key);

/**
 * Source of parameters.
 *
 * Some (but not all) possible sources of parameters are:
 *
 * - "env": ->ps_param_get() uses getenv(3) to obtain the values of
 *   environment variables;
 *
 * - "kv pairs": ->ps_param_get() scans a set of KV pairs, which is
 *   accessible through an object ambient to m0_param_source;
 *
 * - "confc": ->ps_param_get() translates the key into confc path
 *   (a sequence of m0_fids) and accesses conf object and/or its field;
 *
 * - "argv": ->ps_param_get() looks for `-K key=val' in argv[]
 *   and returns the pointer to `val'.
 */
struct m0_param_source {
	void         *(*ps_param_get)(const struct m0_param_source *src,
				      const char *key);
	/** Linkage to m0::i_param_sources. */
	struct m0_tlink ps_link;
	uint64_t        ps_magic;
};

M0_TL_DECLARE(m0_param_sources, M0_INTERNAL, struct m0_param_source);

/** Appends new element to m0::i_param_sources. */
M0_INTERNAL void m0_param_source_add(struct m0_param_source *src);

/** Deletes `src' from m0::i_param_sources. */
M0_INTERNAL void m0_param_source_del(struct m0_param_source *src);

/** @} module */
#endif /* __MERO_MODULE_PARAM_H__ */
