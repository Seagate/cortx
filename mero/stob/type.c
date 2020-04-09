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

#include "stob/type.h"

#include "lib/errno.h"		/* -ENOMEM */
#include "lib/memory.h"		/* M0_ALLOC_PTR */
#include "lib/tlist.h"		/* M0_TL_DESCR_DEFINE */
#include "mero/magic.h"		/* M0_STOB_DOMAINS_MAGIC */

#include "stob/domain.h"	/* m0_stob_domain */
#include "stob/module.h"	/* m0_stob_module */
#include "stob/null.h"		/* m0_stob_null_type */
#include "lib/string.h"		/* m0_streq */

#ifndef __KERNEL__
#include "stob/linux.h"		/* m0_stob_linux_type */
#include "stob/ad.h"		/* m0_stob_ad_type */
#include "stob/perf.h"		/* m0_stob_perf_type */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"

/**
 * @addtogroup stob
 *
 * @{
 */

static const struct m0_stob_type *stob_types_reg[] = {
	&m0_stob_null_type,
#ifndef __KERNEL__
	&m0_stob_linux_type,
	&m0_stob_ad_type,
	&m0_stob_perf_type,
#endif
};

M0_TL_DESCR_DEFINE(domains, "stob domains", static, struct m0_stob_domain,
		   sd_domain_linkage, sd_magic, M0_STOB_DOMAINS_MAGIC,
		   M0_STOB_DOMAINS_HEAD_MAGIC);
M0_TL_DEFINE(domains, static, struct m0_stob_domain);

M0_TL_DESCR_DEFINE(types, "stob types", static, struct m0_stob_type,
		   st_type_linkage, st_magic, M0_STOB_TYPES_MAGIC,
		   M0_STOB_TYPES_HEAD_MAGIC);
M0_TL_DEFINE(types, static, struct m0_stob_type);

static int stob_type_copy(const struct m0_stob_type *type,
			  struct m0_stob_type **copy)
{
	M0_ALLOC_PTR(*copy);
	return *copy == NULL ?
	       -ENOMEM : (memcpy(*copy, type, sizeof(*type)), 0);
}

static void stob_types_destroy_list(struct m0_stob_types *types)
{
	struct m0_stob_type *type;
	while (!types_tlist_is_empty(&types->sts_stypes)) {
		type = types_tlist_head(&types->sts_stypes);
		m0_stob_type_deregister(type);
		/* allocated by stob_type_copy() */
		m0_free(type);
	}
	types_tlist_fini(&types->sts_stypes);
}

static struct m0_stob_types *stob_types_get(void)
{
	return &m0_stob_module__get()->stm_types;
}

M0_INTERNAL int m0_stob_types_init(void)
{
	struct m0_stob_types *types = stob_types_get();
	struct m0_stob_type  *type;
	int		      i;
	int		      rc = 0;

	types_tlist_init(&types->sts_stypes);

	for (i = 0; i < ARRAY_SIZE(stob_types_reg); ++i) {
		rc = stob_type_copy(stob_types_reg[i], &type);
		if (rc != 0)
			break;
		m0_stob_type_register(type);
	}
	if (rc != 0)
		stob_types_destroy_list(types);

	return M0_RC(rc);
}

M0_INTERNAL void m0_stob_types_fini(void)
{
	struct m0_stob_types *types = stob_types_get();

	stob_types_destroy_list(types);
}

M0_INTERNAL struct m0_stob_type *m0_stob_type_by_dom_id(const struct m0_fid *id)
{
	const struct m0_fid_type *fidt = m0_fid_type_get(m0_fid_tget(id));

	/* XXX cast from const to non-const here */
	return fidt == NULL ? NULL :
	       container_of(fidt, struct m0_stob_type, st_fidt);
}

M0_INTERNAL struct m0_stob_type *m0_stob_type_by_name(const char *name)
{
	struct m0_stob_types *types = stob_types_get();

	return m0_tl_find(types, type, &types->sts_stypes,
			  m0_streq(m0_stob_type_name_get(type), name));
}

M0_INTERNAL uint8_t m0_stob_type_id_by_name(const char *name)
{
	return m0_stob_type_by_name(name)->st_fidt.ft_id;
}

M0_INTERNAL void m0_stob_type_register(struct m0_stob_type *type)
{
	struct m0_stob_types *types = stob_types_get();

	m0_mutex_init(&type->st_domains_lock);
	domains_tlist_init(&type->st_domains);
	m0_fid_type_register(&type->st_fidt);
	types_tlink_init_at_tail(type, &types->sts_stypes);
	type->st_ops->sto_register(type);
}

M0_INTERNAL void m0_stob_type_deregister(struct m0_stob_type *type)
{
	type->st_ops->sto_deregister(type);
	types_tlink_del_fini(type);
	m0_fid_type_unregister(&type->st_fidt);
	domains_tlist_fini(&type->st_domains);
	m0_mutex_fini(&type->st_domains_lock);
}

M0_INTERNAL uint8_t m0_stob_type_id_get(const struct m0_stob_type *type)
{
	return type->st_fidt.ft_id;
}

M0_INTERNAL const char *m0_stob_type_name_get(struct m0_stob_type *type)
{
	return type->st_fidt.ft_name;
}

M0_INTERNAL void m0_stob_type__dom_add(struct m0_stob_type *type,
				       struct m0_stob_domain *dom)
{
	m0_mutex_lock(&type->st_domains_lock);
	domains_tlink_init_at_tail(dom, &type->st_domains);
	m0_mutex_unlock(&type->st_domains_lock);
}

M0_INTERNAL void m0_stob_type__dom_del(struct m0_stob_type *type,
				       struct m0_stob_domain *dom)
{
	m0_mutex_lock(&type->st_domains_lock);
	domains_tlink_del_fini(dom);
	m0_mutex_unlock(&type->st_domains_lock);
}

M0_INTERNAL struct m0_stob_domain *
m0_stob_type__dom_find(struct m0_stob_type *type, const struct m0_fid *dom_id)
{
	struct m0_stob_domain *dom;

	m0_mutex_lock(&type->st_domains_lock);
	dom = m0_tl_find(domains, dom, &type->st_domains,
			 m0_fid_cmp(m0_stob_domain_id_get(dom), dom_id) == 0);
	m0_mutex_unlock(&type->st_domains_lock);

	return dom;
}

M0_INTERNAL struct m0_stob_domain *
m0_stob_type__dom_find_by_location(struct m0_stob_type *type,
				   const char *location)
{
	struct m0_stob_domain *dom;

	m0_mutex_lock(&type->st_domains_lock);
	dom = m0_tl_find(domains, dom, &type->st_domains,
			 m0_streq(m0_stob_domain_location_get(dom), location));
	m0_mutex_unlock(&type->st_domains_lock);
	return dom;
}

#undef M0_TRACE_SUBSYSTEM

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
