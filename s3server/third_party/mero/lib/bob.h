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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 21-Jan-2012
 */

#pragma once

#ifndef __MERO_LIB_BOB_H__
#define __MERO_LIB_BOB_H__

/**
 * @defgroup bob Branded objects
 *
 * Branded object (bob) module provides support for a simple run-time type
 * identification.
 *
 * A branded object is any memory structure containing magic field at a known
 * offset. A branded object type (m0_bob_type) specifies the offset and the
 * required magic value field, together with an optional check
 * function. m0_bob_check() function returns true iff the memory structure given
 * to it as a parameter has the required magic value and satisfies the optional
 * check.
 *
 * For flexibility branded objects are not represented by a special
 * data-type. Instead m0_bob_check() takes a void pointer.
 *
 * A couple of helper functions are provided to initialize m0_bob_type:
 *
 *     - m0_bob_type_tlist_init(): used when branded object is used as a typed
 *       list link, @see lib/tlist.h.
 *
 *     - m0_xcode_bob_type_init(): used in case where branded object type has an
 *       xcode representation, @see xcode/xcode.h. This function is defined in
 *       xcode.h to avoid introducing dependencies.
 *
 * A user is explicitly allowed to initialize m0_bob_type instance manually and
 * to set up the optional check function (m0_bob_type::bt_check()) either before
 * or after using these helpers.
 *
 * @{
 */

#include "lib/types.h"                  /* uint64_t */

/* import */
struct m0_tl_descr;

/* export */
struct m0_bob_type;

/**
 * Branded object type specifies how run-time identification is made.
 */
struct m0_bob_type {
	/** Human-readable name used in error messages. */
	const char *bt_name;
	/** Offset to the magic field. */
	int         bt_magix_offset;
	/** Magic value. Must be non zero. */
	uint64_t    bt_magix;
	/**
	 *  Optional check function. If provided, this function is called by
	 *  m0_bob_check().
	 */
	bool      (*bt_check)(const void *bob);
};

/**
 * Partially initializes a branded object type from a typed list descriptor.
 */
M0_INTERNAL void m0_bob_type_tlist_init(struct m0_bob_type *bt,
					const struct m0_tl_descr *td);

/**
 *  Initializes a branded object, by setting the magic field.
 */
M0_INTERNAL void m0_bob_init(const struct m0_bob_type *bt, void *bob);

/**
 *  Finalizes a branded object, by re-setting the magic field to 0.
 */
M0_INTERNAL void m0_bob_fini(const struct m0_bob_type *bt, void *bob);

/**
 * Returns true iff a branded object has the required magic value and check
 * function, if any, returns true.
 */
M0_INTERNAL bool m0_bob_check(const struct m0_bob_type *bt, const void *bob);

/**
 * Produces a type-safe versions of m0_bob_init(), m0_bob_fini() and
 * m0_bob_check(), taking branded object of a given type.
 */
#define M0_BOB_DEFINE(scope, bob_type, type)			\
scope void type ## _bob_init(struct type *bob)			\
{								\
	m0_bob_init(bob_type, bob);				\
}								\
								\
scope void type ## _bob_fini(struct type *bob)			\
{								\
	m0_bob_fini(bob_type, bob);				\
}								\
								\
scope M0_UNUSED bool type ## _bob_check(const struct type *bob)	\
{								\
	return m0_bob_check(bob_type, bob);			\
}								\
								\
struct __ ## type ## _semicolon_catcher

#define M0_BOB_DECLARE(scope, type)		        \
scope void type ## _bob_init(struct type *bob);		\
scope void type ## _bob_fini(struct type *bob);		\
scope bool type ## _bob_check(const struct type *bob)

/**
 * A safer version of container_of().
 *
 * Given a pointer (ptr) returns an ambient object of given type of which ptr is
 * a field. Ambient object has bob type "bt".
 */
#define bob_of(ptr, type, field, bt)					\
({									\
	void *__ptr = (void *)(ptr);					\
	type *__amb;							\
									\
	M0_ASSERT(__ptr != NULL);					\
	__amb = container_of(__ptr, type, field);			\
									\
	M0_ASSERT_INFO(m0_bob_check(bt, __amb),			\
		"%s.%s [%p->%p (%s)] got: %"PRIx64" want: %"PRIx64	\
		" check: %i.", (bt)->bt_name, #field, __ptr, __amb, #type, \
		*((uint64_t *)(((void *)__amb) + (bt)->bt_magix_offset)), \
		(bt)->bt_magix,					\
		(bt)->bt_check != NULL ? (bt)->bt_check(__amb) : -1);	\
	__amb;								\
})

/** @} end of bob group */

/* __MERO_LIB_BOB_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
