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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"

#include "stob/domain.h"

#include "lib/memory.h"		/* m0_alloc */
#include "lib/string.h"		/* m0_strdup */
#include "lib/errno.h"		/* EINVAL */

#include "stob/type.h"		/* m0_stob_type__dom_add */
#include "stob/cache.h"		/* m0_stob_cache */
#include "stob/stob_internal.h"	/* m0_stob__cache_evict */
#include "stob/stob.h"		/* m0_stob_fid_dom_id_get */

/**
 * @addtogroup stob
 *
 * @{
 */

enum {
	/**
	 * Maximum number of cached stobs that ain't held by any user and
	 * ain't finalised yet.
	 *
	 * @note 0x10 may be too small value.
	 * @todo make a parameter for stob domain.
	 */
	M0_STOB_CACHE_MAX_SIZE = 0x10,
};

static int stob_domain_type(const char *location,
			    struct m0_stob_type **type)
{
	char *colon;
	char *type_str;
	int   rc;

	M0_ENTRY("location=%s", location);

	rc = location == NULL ? -EINVAL : 0;
	if (location != NULL) {
		colon	 = strchr(location, ':');
		type_str = m0_strdup(location);
		rc	 = colon == NULL	  ? -EINVAL : 0;
		rc	 = rc ?: type_str == NULL ? -ENOMEM : 0;
		if (colon != NULL && type_str != NULL) {
			type_str[colon - location] = '\0';
			*type = m0_stob_type_by_name(type_str);
		}
		m0_free(type_str);
	}
	M0_POST(ergo(rc == 0, *type != NULL));
	return M0_RC(rc);
}

static char *stob_domain_location_data(const char *location)
{
	char *location_data = NULL;
	char *colon;

	if (location != NULL) {
		colon	      = strchr(location, ':');
		location_data = colon == NULL ? NULL : m0_strdup(colon + 1);
	}
	return location_data;
}

static void stob_domain_cache_evict_cb(struct m0_stob_cache *cache,
				       struct m0_stob *stob)
{
	m0_stob__cache_evict(stob);
}

static int stob_domain_create(struct m0_stob_type *type,
			      const char *location_data,
			      uint64_t dom_key,
			      const char *str_cfg_create)
{
	void *cfg_create = NULL;
	bool  cfg_parsed;
	int   rc;

	rc = type->st_ops->sto_domain_cfg_create_parse(str_cfg_create,
						       &cfg_create);
	cfg_parsed = rc == 0;
	rc = rc ?: type->st_ops->sto_domain_create(type, location_data,
						   dom_key, cfg_create);
	if (cfg_parsed)
		type->st_ops->sto_domain_cfg_create_free(cfg_create);
	return M0_RC(rc);
}

static int stob_domain_init(struct m0_stob_type *type,
			    const char *location_data,
			    const char *str_cfg_init,
			    struct m0_stob_domain **out)
{
	void *cfg_init = NULL;
	bool  cfg_parsed;
	int   rc;

	M0_ENTRY("location_data=%s str_cfg_init=%s",
		 location_data, str_cfg_init);
	rc = type->st_ops->sto_domain_cfg_init_parse(str_cfg_init, &cfg_init);
	cfg_parsed = rc == 0;
	rc = rc ?: type->st_ops->sto_domain_init(type, location_data,
						 cfg_init, out);
	if (cfg_parsed)
		type->st_ops->sto_domain_cfg_init_free(cfg_init);
	return M0_RC(rc);
}

static int stob_domain_init_create(const char *location,
				   const char *str_cfg_init,
				   uint64_t dom_key,
				   const char *str_cfg_create,
				   struct m0_stob_domain **out,
				   bool init)
{
	struct m0_stob_type   *type       = NULL;
	struct m0_stob_domain *dom;
	char                  *location_data;
	int                    rc;
	int                    rc1;

	M0_ENTRY();

	M0_LOG(M0_INFO, "location=%s str_cfg_init=%s dom_key=%"PRIu64" "
		 "str_cfg_create=%s init=%d",
		 location, str_cfg_init, dom_key, str_cfg_create, !!init);

	dom	      = m0_stob_domain_find_by_location(location);
	rc	      = dom != NULL ? -EEXIST : 0;
	rc	      = rc ?: stob_domain_type(location, &type);
	location_data = rc == 0 ? stob_domain_location_data(location) : NULL;
	rc	      = rc ?: location_data == NULL ? -ENOMEM : 0;
	rc = rc ?: init ? 0 : stob_domain_create(type, location_data, dom_key,
						 str_cfg_create);
	if (rc == 0) {
		rc = stob_domain_init(type, location_data, str_cfg_init, out);
		if (!init && rc != 0) {
			M0_LOG(M0_WARN,
			       "init() after create() failed: rc = %d, "
			       "location = %s", rc, location);
			rc1 = m0_stob_domain_destroy_location(location);
			if (rc1 != 0) {
				M0_LOG(M0_ERROR,
				       "destroy() failed: rc = %d, "
				       "location = %s", rc1, location);
			}
			/* rc1 is lost here */
		}
	}
	M0_ASSERT(ergo(rc == 0, *out != NULL));
	if (rc == 0) {
		dom		      = *out;
		dom->sd_location      = m0_strdup(location);
		dom->sd_location_data = location_data;
		dom->sd_type	      = type;
		m0_stob_cache_init(&dom->sd_cache, M0_STOB_CACHE_MAX_SIZE,
				   &stob_domain_cache_evict_cb);
		M0_ASSERT_EX(m0_stob_domain_find(m0_stob_domain_id_get(dom)) ==
			     NULL);
		m0_stob_type__dom_add(type, dom);
	} else {
		m0_free(location_data);
	}
	M0_POST(ergo(rc == 0, m0_stob_domain__invariant(*out)));
	return M0_RC(rc);
}

M0_INTERNAL int m0_stob_domain_init(const char *location,
				    const char *str_cfg_init,
				    struct m0_stob_domain **out)
{
	M0_LOG(M0_DEBUG, "location=%s str_cfg_init=%s", location, str_cfg_init);
	return stob_domain_init_create(location, str_cfg_init, 0,
				       NULL, out, true);
}

M0_INTERNAL void m0_stob_domain_fini(struct m0_stob_domain *dom)
{
	const struct m0_fid *dom_id = m0_stob_domain_id_get(dom);
	struct m0_stob_type *type   = m0_stob_type_by_dom_id(dom_id);

	M0_ASSERT(type != NULL);
	m0_stob_type__dom_del(type, dom);
	m0_stob_cache_fini(&dom->sd_cache);
	m0_free(dom->sd_location_data);
	m0_free(dom->sd_location);
	dom->sd_ops->sdo_fini(dom);
}

M0_INTERNAL int m0_stob_domain_create(const char *location,
				      const char *str_cfg_init,
				      uint64_t dom_key,
				      const char *str_cfg_create,
				      struct m0_stob_domain **out)
{
	return stob_domain_init_create(location, str_cfg_init, dom_key,
				       str_cfg_create, out, false);
}

M0_INTERNAL int m0_stob_domain_destroy(struct m0_stob_domain *dom)
{
	const char *location_const = m0_stob_domain_location_get(dom);
	char	   *location;
	int	    rc;

	M0_ENTRY("location=%s", location_const);
	location = location_const == NULL ? NULL : m0_strdup(location_const);
	rc = location == NULL ? -ENOMEM : 0;
	m0_stob_domain_fini(dom);
	rc = rc ?: m0_stob_domain_destroy_location(location);
	m0_free(location);
	return M0_RC(rc);
}

M0_INTERNAL int m0_stob_domain_destroy_location(const char *location)
{
	struct m0_stob_type *type;
	char		    *location_data;
	int		     rc;

	M0_ENTRY("location=%s", location);

	rc = location == NULL ? -EINVAL : 0;
	rc = rc ?: stob_domain_type(location, &type);
	location_data = rc == 0 ? stob_domain_location_data(location) : NULL;
	rc = rc ?: location_data == NULL ? -ENOMEM : 0;
	rc = rc ?: type->st_ops->sto_domain_destroy(type, location_data);
	m0_free(location_data);

	if (!M0_IN(rc, (0, -ENOENT)))
		return M0_ERR(rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_stob_domain_create_or_init(const char *location,
					      const char *str_cfg_init,
					      uint64_t dom_key,
					      const char *str_cfg_create,
					      struct m0_stob_domain **out)
{
	int rc;

	rc = m0_stob_domain_init(location, str_cfg_init, out);
	if (rc != 0)
		rc = m0_stob_domain_create(location, str_cfg_init,
					   dom_key, str_cfg_create, out);
	return M0_RC(rc);
}

M0_INTERNAL struct m0_stob_domain *
m0_stob_domain_find(const struct m0_fid *dom_id)
{
	return m0_stob_type__dom_find(m0_stob_type_by_dom_id(dom_id), dom_id);
}

M0_INTERNAL struct m0_stob_domain *
m0_stob_domain_find_by_location(const char *location)
{
	struct m0_stob_type *type;
	int		     rc = stob_domain_type(location, &type);

	return rc != 0 ? NULL :
	       m0_stob_type__dom_find_by_location(type, location);
}

M0_INTERNAL struct m0_stob_domain *
m0_stob_domain_find_by_stob_id(const struct m0_stob_id *stob_id)
{
	return m0_stob_domain_find(&stob_id->si_domain_fid);
}

M0_INTERNAL const struct m0_fid *
m0_stob_domain_id_get(const struct m0_stob_domain *dom)
{
	return &dom->sd_id;
}

M0_INTERNAL const char *
m0_stob_domain_location_get(const struct m0_stob_domain *dom)
{
	return dom->sd_location;
}

M0_INTERNAL void m0_stob_domain__id_set(struct m0_stob_domain *dom,
					 struct m0_fid *dom_id)
{
	dom->sd_id = *dom_id;
}

M0_INTERNAL uint8_t m0_stob_domain__type_id(const struct m0_fid *dom_id)
{
	return m0_fid_tget(dom_id);
}

M0_INTERNAL uint64_t m0_stob_domain__dom_key(const struct m0_fid *dom_id)
{
	return dom_id->f_key;
}

M0_INTERNAL void m0_stob_domain__dom_id_make(struct m0_fid *dom_id,
					     uint8_t type_id,
					     uint64_t dom_container,
					     uint64_t dom_key)
{
	m0_fid_tset(dom_id, type_id, dom_container, dom_key);
}

M0_INTERNAL bool m0_stob_domain__invariant(struct m0_stob_domain *dom)
{
	const struct m0_fid *dom_id = m0_stob_domain_id_get(dom);
	struct m0_stob_type *type = dom->sd_type;

	return _0C(type != NULL) &&
	       _0C(m0_stob_domain__type_id(dom_id) == m0_stob_type_id_get(type));
}

M0_INTERNAL bool m0_stob_domain__dom_key_is_valid(uint64_t dom_key)
{
	return (dom_key & 0xFFULL << 56) == 0;
}

M0_INTERNAL bool m0_stob_domain_is_of_type(const struct m0_stob_domain *dom,
					   const struct m0_stob_type *dt)
{
	return m0_stob_type_id_get(dom->sd_type) == m0_stob_type_id_get(dt);
}
/** @} end of stob group */

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
