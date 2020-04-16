/* -*- C -*- */
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 12-Mar-2014
 */

#pragma once

#ifndef __MERO_STOB_TYPE_H__
#define __MERO_STOB_TYPE_H__

#include "lib/tlist.h"	/* m0_tl */
#include "lib/types.h"	/* uint64_t */
#include "lib/mutex.h"	/* m0_mutex */

#include "fid/fid.h"	/* m0_fid_type */

/**
 * @defgroup stob Storage object
 *
 * @{
 */

struct m0_stob_domain;
struct m0_stob_type_ops;

/** Registered stob types. */
struct m0_stob_types {
	struct m0_tl sts_stypes;
};

M0_INTERNAL int m0_stob_types_init(void);
M0_INTERNAL void m0_stob_types_fini(void);

/**
 * Stob type.
 *
 * Stob types are added dynamically during initialisation.
 *
 * Stob type implementation needs to provide instance of m0_stob_type that
 * should be registered via interface m0_stob_type_register().
 */
struct m0_stob_type {
	const struct m0_stob_type_ops *st_ops;
	struct m0_fid_type	       st_fidt;
	struct m0_tl		       st_domains;
	struct m0_mutex		       st_domains_lock;
	/** Linkage into m0_stob_types::sts_stypes list */
	struct m0_tlink		       st_type_linkage;
	uint64_t		       st_magic;
	void			      *st_private;
};

/** Stob type operations vector */
struct m0_stob_type_ops {
	/** @see m0_stob_type_register() */
	void (*sto_register)(struct m0_stob_type *type);
	/** @see m0_stob_type_deregister() */
	void (*sto_deregister)(struct m0_stob_type *type);
	/** Parses configuration for m0_stob_domain_init() */
	int (*sto_domain_cfg_init_parse)(const char *str_cfg_init,
					 void **cfg_init);
	/**
	 * Frees configuration allocated by successful
	 * m0_stob_type_ops::sto_domain_cfg_init_parse()
	 */
	void (*sto_domain_cfg_init_free)(void *cfg_init);
	/** Parses configuration for m0_stob_domain_create() */
	int (*sto_domain_cfg_create_parse)(const char *str_cfg_create,
					   void **cfg_create);
	/**
	 * Frees configuration allocated by successful
	 * m0_stob_type_ops::sto_domain_cfg_create_parse()
	 */
	void (*sto_domain_cfg_create_free)(void *cfg_create);
	/** @see m0_stob_domain_init() */
	int (*sto_domain_init)(struct m0_stob_type *type,
			       const char *location_data,
			       void *cfg_init,
			       struct m0_stob_domain **out);
	/**
	 * Creates stob domain.
	 *
	 * @note There is no m0_stob_domain parameter here, so this function
	 * shouldn't initialise stob domain. Initialisation will be done by
	 * domain generic code after successful create call.
	 * Currently m0_stob_domain uses only 64-bit key and 8-bit type from
	 * 128-bit fid identifier and it may be changed in the future.
	 *
	 * @see m0_stob_domain_create()
	 */
	int (*sto_domain_create)(struct m0_stob_type *type,
				 const char *location_data,
				 uint64_t dom_key,
				 void *cfg_create);
	/** @see m0_stob_domain_destroy_location()  */
	int (*sto_domain_destroy)(struct m0_stob_type *type,
				  const char *location_data);
};

/**
 * Registers new stob type.
 *
 * @pre type->st_fidt was not registered before for m0 instance
 */
M0_INTERNAL void m0_stob_type_register(struct m0_stob_type *type);
/** Deregisters stob type */
M0_INTERNAL void m0_stob_type_deregister(struct m0_stob_type *type);

/** Gets stob type by domain id. */
M0_INTERNAL struct m0_stob_type *
m0_stob_type_by_dom_id(const struct m0_fid *id);
/** Gets stob type by name. */
M0_INTERNAL struct m0_stob_type *m0_stob_type_by_name(const char *name);
/** Gets stob type id by name. */
M0_INTERNAL uint8_t m0_stob_type_id_by_name(const char *name);

/** Gets stob type id for the given stob type. */
M0_INTERNAL uint8_t m0_stob_type_id_get(const struct m0_stob_type *type);
/** Gets stob type name for the given stob type. */
M0_INTERNAL const char *m0_stob_type_name_get(struct m0_stob_type *type);

/**
 * Adds domain to the list of domains in the stob type.
 * @note This function is used by generic stob domain code.
 * @see m0_stob_type__dom_del(), m0_stob_type__dom_find().
 */
M0_INTERNAL void m0_stob_type__dom_add(struct m0_stob_type *type,
				       struct m0_stob_domain *dom);
/**
 * Removes previously added domain from the list of domains in the stob type.
 * @note This function is used by generic stob domain code.
 * @see m0_stob_type__dom_add(), m0_stob_type__dom_find().
 */
M0_INTERNAL void m0_stob_type__dom_del(struct m0_stob_type *type,
				       struct m0_stob_domain *dom);
/**
 * Finds previously added domain in the list of domains in the stob type.
 * @note This function is used by generic stob domain code.
 * @see m0_stob_type__dom_add(), m0_stob_type__dom_del(),
 * m0_stob_type__dom_find_by_location().
 */
M0_INTERNAL struct m0_stob_domain *
m0_stob_type__dom_find(struct m0_stob_type *type, const struct m0_fid *dom_id);

/**
 * The same as m0_stob_type__dom_find(), but it performs search by
 * domain location.
 * @see m0_stob_type__dom_find().
 */
M0_INTERNAL struct m0_stob_domain *
m0_stob_type__dom_find_by_location(struct m0_stob_type *type,
				   const char *location);

/** @} end of stob group */
#endif /* __MERO_STOB_TYPE_H__ */

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
