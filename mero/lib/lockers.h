/* -*- C -*- */
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
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 9-Jan-2013
 */


#pragma once

#ifndef __MERO_LIB_LOCKERS_H__
#define __MERO_LIB_LOCKERS_H__

#include "lib/types.h"

/**
 * @defgroup lockers
 *
 * Lockers module provides an interface to support storage of private pointers
 * in parent structures. This allows not only for efficient sharing of data
 * with others but also removes duplication of such interfaces.
 * To describe a typical usage pattern, consider one wants a locker for 32
 * objects in object of type foo.
 *
 * Following things have to be done:
 *
 * - Declare a locker type by, M0_LOCKERS_DECLARE(scope, foo, 32)
 *   This will generate a structure of type foo_lockers and declarations for
 *   functions to manipulate foo_lockers.
 *   It is required that a structure of type foo must be present in the source.
 *
 * - embed foo_lockers in struct foo
 *
 *   @code
 *   struct foo {
 *         ...
 *	   struct foo_lockers lockers;
 *	   ...
 *   };
 *   @endcode
 *
 * - Next, define a locker type by M0_LOCKERS_DEFINE(scope, foo, lockers)
 *
 * Now to use this locker following interfaces can be used:
 *
 * - foo_lockers_allot    - Returns new key to access stored data.
 * - foo_lockers_set      - Stores provided data against provided key.
 * - foo_lockers_get      - Locates and returns data corresponding
 *                          to the provided key.
 * - foo_lockers_clear    - Clears the data stored at given key.
 * - foo_lockers_is_empty - Checks whether a data is stored at given key.
 *
 * Lockers module does not provide any support against concurrency and
 * validation of data stored at a given key, it is the responsibility of the
 * invoker to take care of these aspects.
 *
 * Please refer to unit test for understanding the nitty-gritty of lockers.
 * @{
 */

struct m0_lockers_type {
	/* Maximum number of keys that can be alloted */
	uint32_t lot_max;
	/* Current number of alloted keys */
	bool    *lot_inuse;
};

struct m0_lockers {
	void *loc_slots[0];
};

#define M0_LOCKERS_DECLARE(scope, name, max) \
	M0_LOCKERS__DECLARE(scope, name, name, max)

#define M0_LOCKERS__DECLARE(scope, name, amb, max)			\
struct name;								\
									\
enum { M0_LOCKERS_ ## name ## _max = (max) };				\
									\
struct name ## _lockers {						\
	struct m0_lockers __base;					\
	void             *__slots[(max)];				\
};									\
									\
M0_BASSERT(offsetof(struct name ## _lockers, __slots[0]) ==		\
	   offsetof(struct m0_lockers, loc_slots[0]));			\
									\
scope void name ## _lockers_init(struct amb *par);			\
scope void name ## _lockers_fini(struct amb *par);			\
scope int name ## _lockers_allot(void);				\
scope void name ## _lockers_free(int key);				\
scope void name ## _lockers_set(struct amb *par, int key, void *data);	\
scope void * name ## _lockers_get(const struct amb *par, int key);	\
scope void name ## _lockers_clear(struct amb *par, int key);		\
scope bool name ## _lockers_is_empty(const struct amb *par, int key)

M0_INTERNAL void m0_lockers_init(const struct m0_lockers_type *lt,
				 struct m0_lockers            *lockers);


M0_INTERNAL void m0_lockers_fini(struct m0_lockers_type *lt,
				 struct m0_lockers      *lockers);

/**
 * Allots a new key of type lt
 * @pre lt->lot_count < lt->lot_max
 */
M0_INTERNAL int m0_lockers_allot(struct m0_lockers_type *lt);

/**
 * Frees a key of type lt
 * @pre lt->lot_count < lt->lot_max
 */
M0_INTERNAL void m0_lockers_free(struct m0_lockers_type *lt, int key);

/**
 * Stores a value in locker
 *
 * @post !m0_lockers_is_empty(locker, key) &&
 *        m0_lockers_get(locker, key) == data
 */
M0_INTERNAL void m0_lockers_set(const struct m0_lockers_type *lt,
				struct m0_lockers            *lockers,
				uint32_t                      key,
				void                         *data);
/**
 * Retrieves a value stored in locker
 *
 * @pre !m0_lockers_is_empty(locker, key)
 */
M0_INTERNAL void *m0_lockers_get(const struct m0_lockers_type *lt,
				 const struct m0_lockers      *lockers,
				 uint32_t                      key);

/**
 * Clears the value stored in a locker
 *
 * @post m0_lockers_is_empty(locker, key)
 */
M0_INTERNAL void m0_lockers_clear(const struct m0_lockers_type *lt,
				  struct m0_lockers            *lockers,
				  uint32_t                      key);

M0_INTERNAL bool m0_lockers_is_empty(const struct m0_lockers_type *lt,
				     const struct m0_lockers      *lockers,
				     uint32_t                      key);

#define M0_LOCKERS_DEFINE(scope, name, field) \
	M0_LOCKERS__DEFINE(scope, name, name, field)

#define M0_LOCKERS__DEFINE(scope, name, amb, field)			\
scope bool __ ## name ## _inuse [M0_LOCKERS_ ## name ## _max];		\
scope struct m0_lockers_type name ## _lockers_type = {			\
	.lot_max = ARRAY_SIZE(M0_FIELD_VALUE(struct name ## _lockers,	\
					     __slots)),		\
	.lot_inuse = __ ## name ## _inuse				\
};									\
									\
scope void name ## _lockers_init(struct amb *par)			\
{									\
	m0_lockers_init(&name ## _lockers_type, &par->field.__base);	\
}									\
									\
scope void name ## _lockers_fini(struct amb *par)			\
{									\
	m0_lockers_fini(&name ## _lockers_type, &par->field.__base);	\
}									\
									\
scope int name ## _lockers_allot(void)					\
{									\
	return m0_lockers_allot(&name ## _lockers_type);		\
}									\
									\
scope void name ## _lockers_free(int key)				\
{									\
	m0_lockers_free(&name ## _lockers_type, key);			\
}									\
									\
scope void name ## _lockers_set(struct amb *par, int key, void *data)	\
{									\
	m0_lockers_set(&name ## _lockers_type,				\
		       &par->field.__base, key, data);			\
}									\
									\
scope void * name ## _lockers_get(const struct amb *par, int key)	\
{									\
	return m0_lockers_get(&name ## _lockers_type,			\
			      &par->field.__base, key);		\
}									\
									\
scope void name ## _lockers_clear(struct amb *par, int key)		\
{									\
	m0_lockers_clear(&name ## _lockers_type, &par->field.__base, key); \
}									\
									\
scope bool name ## _lockers_is_empty(const struct amb *par, int key)	\
{									\
	return m0_lockers_is_empty(&name ## _lockers_type,		\
				   &par->field.__base, key);		\
}									\
									\
struct __ ## type ## _semicolon_catcher

/** @} end of lockers group */
#endif /* __MERO_LIB_LOCKERS_H__ */

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
