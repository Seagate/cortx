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

#ifndef __MERO_STOB_DOMAIN_H__
#define __MERO_STOB_DOMAIN_H__

#include "lib/types.h"		/* uint64_t */
#include "lib/tlist.h"		/* m0_tlink */
#include "fid/fid.h"            /* m0_fid */

#include "stob/cache.h"		/* m0_stob_cache */

/**
 * @defgroup stob Storage object
 *
 * @{
 */

struct m0_stob_type;
struct m0_stob_domain_ops;
struct m0_stob;
struct m0_stob_id;
struct m0_stob_io;
struct m0_be_tx_credit;
struct m0_fid;
struct m0_dtx;
struct m0_indexvec;

/**
 * Stob domain.
 *
 * <b>Description</b>.
 *
 * Stob domain is a collection of storage objects of the same type.
 * A stob type may have multiple domains, which are linked together to its
 * type by 'sd_domain_linkage'.
 *
 * A storage domain comes with an operation to find a storage object in the
 * domain. A domain might cache storage objects and use some kind of index to
 * speed up object lookup. This caching and indexing are not visible at the
 * generic level.
 *
 * Maintenance of stob domain lifetime is responsibility of user.
 *
 * Domain fid has the following structure:
 *
 *           8 bits                            120 bits
 *   +----------------------+----------------------------------------------+
 *   | stob domain type id  |               stob domain key                |
 *   +----------------------+----------------------------------------------+
 *
 * <b>Stob cache</b>.
 *
 * Stob domain doesn't contain list of initialised stobs. Instead, it has
 * a stob cache. This cache is maintained implicitly and there is no public
 * interface for it.
 *
 * A stob may be finalised and removed from stob cache at some time after
 * reference count is set to 0. This semantics is hidden.
 *
 * <b>Stob domain location</b>
 *
 * Location is a string that has the next format:
 *
 * @verbatim
 * location ::= <type name> ":" <implementation dependent location data>
 * @endverbatim
 *
 * Consequently, stob type name parameter is omitted in
 * m0_stob_domain_{init,create}().
 */
struct m0_stob_domain {
	const struct m0_stob_domain_ops *sd_ops;
	struct m0_stob_type		*sd_type;
	struct m0_fid                    sd_id;
	char				*sd_location;
	char				*sd_location_data;
	/** Linkage into m0_stob_type::st_domains list */
	struct m0_tlink			 sd_domain_linkage;
	/** Magic for sd_domain_linkage */
	uint64_t			 sd_magic;
	struct m0_stob_cache		 sd_cache;
	/** Private data of stob domain implementation */
	void                            *sd_private;
};

/** Stob domain operations vector. */
struct m0_stob_domain_ops {
	/** @see m0_stob_domain_fini(), m0_stob_domain_destroy() */
	void (*sdo_fini)(struct m0_stob_domain *dom);
	/**
	 * Allocates memory for m0_stob structure.
	 * @see m0_stob_find_by_key()
	 */
	struct m0_stob *(*sdo_stob_alloc)(struct m0_stob_domain *dom,
					  const struct m0_fid *stob_fid);
	/**
	 * Frees memory for m0_stob structure allocated by
	 * m0_stob_domain_ops::sdo_stob_alloc().
	 * @see m0_stob_find_by_key(), m0_stob_put().
	 */
	void (*sdo_stob_free)(struct m0_stob_domain *dom,
			      struct m0_stob *stob);
	/** Parses configuration for m0_stob_create() */
	int (*sdo_stob_cfg_parse)(const char *str_cfg_create,
				  void **cfg_create);
	/**
	 * Frees configuration allocated by successful
	 * m0_stob_domain_ops::sdo_stob_cfg_parse()
	 */
	void (*sdo_stob_cfg_free)(void *cfg_create);
	/** m0_stob_locate() type-specific implementation */
	int (*sdo_stob_init)(struct m0_stob *stob,
			     struct m0_stob_domain *dom,
			     const struct m0_fid *stob_fid);
	/** @see m0_stob_create_credit() */
	void (*sdo_stob_create_credit)(struct m0_stob_domain *dom,
				       struct m0_be_tx_credit *accum);
	/** @see m0_stob_create() */
	int (*sdo_stob_create)(struct m0_stob *stob,
			       struct m0_stob_domain *dom,
			       struct m0_dtx *dtx,
			       const struct m0_fid *stob_fid,
			       void *cfg);
	/** @see m0_stob_io_credit() */
	void (*sdo_stob_write_credit)(const struct m0_stob_domain *dom,
				      const struct m0_stob_io *io,
				      struct m0_be_tx_credit *accum);
};

/** Initialises existing domain. */
M0_INTERNAL int m0_stob_domain_init(const char *location,
				    const char *str_cfg_init,
				    struct m0_stob_domain **out);
M0_INTERNAL void m0_stob_domain_fini(struct m0_stob_domain *dom);
M0_INTERNAL bool m0_stob_domain__invariant(struct m0_stob_domain *dom);


/**
 * Creates a stob domain.
 *
 * This function has the functionality of m0_stob_domain_init() thereby
 * dom must be finalised after successful m0_stob_domain_create() call.
 *
 * @see m0_stob_domain_create_or_init(), m0_stob_domain_destroy(),
 * m0_stob_domain_init(), m0_stob_domain_fini().
 */
M0_INTERNAL int m0_stob_domain_create(const char *location,
				      const char *str_cfg_init,
				      uint64_t dom_key,
				      const char *str_cfg_create,
				      struct m0_stob_domain **out);
/**
 * Destroys a stob domain.
 *
 * This function has implicit m0_stob_domain_fini() functionality, so
 * domain is always finalised after call to this function.
 *
 * @see m0_stob_domain_destroy_location()
 */
M0_INTERNAL int m0_stob_domain_destroy(struct m0_stob_domain *dom);

/**
 * Destroys a stob domain.
 *
 * Stob domain shouldn't be in initialised state at the time of the call.
 *
 * Notes:
 * - it is possible to call this function again with the same location;
 * - if this function fails then stob domain is left in some intermediate state:
 *   m0_stob_domain_init() can't be called for this domain,
 *   and m0_stob_domain_create() will return error;
 * - the case when there is no such domain at specified @location
 *   is not considered as an error, 0 is returned.
 */
M0_INTERNAL int m0_stob_domain_destroy_location(const char *location);

/**
 * Initialises stob domain. Creates new domain if initialisation fails.
 *
 * @note dom_key and str_cfg_create parameters is completely ignored if stob
 * domain initialisation succeeds. m0_stob_domain_create() is not called
 * in this case.
 * @note Use this function if you don't care about reason of failure; otherwise
 * m0_stob_domain_init() and m0_stob_domain_create() should be used instead.
 */
M0_INTERNAL int m0_stob_domain_create_or_init(const char *location,
					      const char *str_cfg_init,
					      uint64_t dom_key,
					      const char *str_cfg_create,
					      struct m0_stob_domain **out);
/**
 * Searches domain with the given dom_id that was previously initialised with
 * m0_stob_domain_init() or m0_stob_domain_create().
 */
M0_INTERNAL struct m0_stob_domain *
m0_stob_domain_find(const struct m0_fid *dom_id);
/** Searches domain with the given location. */
M0_INTERNAL struct m0_stob_domain *
m0_stob_domain_find_by_location(const char *location);
/** Searches domain with the given dom_id, extracted from stob id. */
M0_INTERNAL struct m0_stob_domain *
m0_stob_domain_find_by_stob_id(const struct m0_stob_id *stob_id);

/** Returns stob domain id. */
M0_INTERNAL const struct m0_fid *
m0_stob_domain_id_get(const struct m0_stob_domain *dom);
/** Returns stob domain location. */
M0_INTERNAL const char *
m0_stob_domain_location_get(const struct m0_stob_domain *dom);
/** Determines if stob domain hosts the stobs of given type. */
M0_INTERNAL bool m0_stob_domain_is_of_type(const struct m0_stob_domain *dom,
					   const struct m0_stob_type *dt);
/**
 * Sets stob domain id.
 *
 * @note This function should be used from stob type implementation code only.
 */
M0_INTERNAL void m0_stob_domain__id_set(struct m0_stob_domain *dom,
					 struct m0_fid *dom_id);
M0_INTERNAL void m0_stob_domain__dom_id_make(struct m0_fid *dom_id,
					     uint8_t type_id,
					     uint64_t dom_container,
					     uint64_t dom_key);

/** Extracts stob type id from stob domain id. */
M0_INTERNAL uint8_t m0_stob_domain__type_id(const struct m0_fid *dom_id);
/** Extracts stob domain key from stob domain id. */
M0_INTERNAL uint64_t m0_stob_domain__dom_key(const struct m0_fid *dom_id);

/** Checks if domain key is valid. */
M0_INTERNAL bool m0_stob_domain__dom_key_is_valid(uint64_t dom_key);

M0_INTERNAL struct m0_stob_cache *
m0_stob_domain__cache(struct m0_stob_domain *dom);

M0_INTERNAL struct m0_stob *
m0_stob_domain__stob_alloc(struct m0_stob_domain *dom,
			   const struct m0_fid *stob_fid);
M0_INTERNAL void m0_stob_domain__stob_free(struct m0_stob_domain *dom,
					   struct m0_stob *stob);
/** @} end of stob group */
#endif /* __MERO_STOB_DOMAIN_H__ */

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
