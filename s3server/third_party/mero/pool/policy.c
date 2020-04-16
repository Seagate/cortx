/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@seagate.com>
 * Original creation date: 07/06/2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_POOL
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/tlist.h"
#include "module/instance.h"
#include "module/module.h"
#include "mero/magic.h"
#include "conf/confc.h"           /* m0_confc_fini */
#include "conf/helpers.h"         /* m0_conf_pver_get */
#include "pool/pool.h"
#include "pool/policy.h"

/**
 * @addtogroup pool_policy
 *
 * @{
 */

M0_TL_DESCR_DEFINE(pver_policy_types, "pver policy type list", static,
		   struct m0_pver_policy_type, ppt_link, ppt_magic,
		   M0_PVER_POLICY_MAGIC, M0_PVER_POLICY_HEAD_MAGIC);
M0_TL_DEFINE(pver_policy_types, static, struct m0_pver_policy_type);

/*
 * Pool version selection policies.
 */

static int pver_first_available_create(struct m0_pver_policy **out);
static int pver_first_available_init(struct m0_pver_policy *policy);
static int pver_first_available_get(struct m0_pools_common  *pc,
				    const struct m0_pool    *pool,
				    struct m0_pool_version **pver);
static void pver_first_available_fini(struct m0_pver_policy *policy);

static const struct m0_pver_policy_type_ops
first_vailable_pver_policy_type_ops = {
	.ppto_create = pver_first_available_create
};

/** Default pver selection policy --- select first clean pver. */
struct m0_pver_policy_first_available {
	struct m0_pver_policy fcp_policy;
};

static struct m0_pver_policy_type first_available_pver_policy_type = {
	.ppt_name = "pver_policy_first_available",
	.ppt_code = M0_PVER_POLICY_FIRST_AVAILABLE,
	.ppt_ops  = &first_vailable_pver_policy_type_ops
};

static const struct m0_pver_policy_ops first_available_pver_policy_ops = {
	.ppo_init = pver_first_available_init,
	.ppo_fini = pver_first_available_fini,
	.ppo_get  = pver_first_available_get
};

static int pver_first_available_create(struct m0_pver_policy **out)
{
	struct m0_pver_policy_first_available *fa;

	M0_ALLOC_PTR(fa);
	if (fa == NULL)
		return M0_ERR(-ENOMEM);

	*out = &fa->fcp_policy;
	(*out)->pp_ops = &first_available_pver_policy_ops;
	return M0_RC(0);
}

static int pver_first_available_init(struct m0_pver_policy *policy)
{
	M0_ENTRY();
	return M0_RC(0);
}

static void pver_first_available_fini(struct m0_pver_policy *policy)
{
	M0_ENTRY();
	m0_free(container_of(policy, struct m0_pver_policy_first_available,
			     fcp_policy));
	M0_LEAVE();
}

static int pver_first_available_get(struct m0_pools_common  *pc,
				    const struct m0_pool    *pool,
				    struct m0_pool_version **pv)
{
	struct m0_conf_pver *pver;
	int                  rc;

	M0_ENTRY("pool="FID_F, FID_P(&pool->po_id));

	/* Check pool version cache */
	*pv = m0_pool_clean_pver_find(pool);
	if (*pv != NULL)
		return M0_RC(0);

	/* Derive new pver using formulae */
	rc = m0_conf_pver_get(pc->pc_confc, &pool->po_id, &pver);
	if (rc != 0)
		return M0_ERR(rc);

	/* Cache derived if not present */
	*pv = m0_pool_version_lookup(pc, &pver->pv_obj.co_id);
	if (*pv == NULL)
		rc = m0_pool_version_append(pc, pver, pv);

	m0_confc_close(&pver->pv_obj);
	return M0_RC(rc);
}

static struct m0_pver_policies *pver_policies(void)
{
	return m0_get()->i_moddata[M0_MODULE_POOL];
}

M0_INTERNAL struct m0_pver_policy_type *
m0_pver_policy_type_find(enum m0_pver_policy_code code)
{
	return m0_tl_find(pver_policy_types, pvpt, &pver_policies()->pp_types,
			  pvpt->ppt_code == code);
}

M0_INTERNAL int m0_pver_policy_types_nr(void)
{
	return pver_policy_types_tlist_length(&pver_policies()->pp_types);
}

M0_INTERNAL
int m0_pver_policy_type_register(struct m0_pver_policy_type *type)
{
	pver_policy_types_tlink_init_at_tail(type, &pver_policies()->pp_types);
	return M0_RC(0);
}

M0_INTERNAL
void m0_pver_policy_type_deregister(struct m0_pver_policy_type *type)
{
	pver_policy_types_tlink_del_fini(type);
}

M0_INTERNAL int m0_pver_policies_init(void)
{
	struct m0_pver_policies *policies;

	M0_ALLOC_PTR(policies);
	if (policies == NULL)
		return M0_ERR(-ENOMEM);

	m0_get()->i_moddata[M0_MODULE_POOL] = policies;
	pver_policy_types_tlist_init(&policies->pp_types);

	return M0_RC(m0_pver_policy_type_register(
			     &first_available_pver_policy_type));
}

M0_INTERNAL void m0_pver_policies_fini(void)
{
	m0_pver_policy_type_deregister(&first_available_pver_policy_type);
	pver_policy_types_tlist_fini(&pver_policies()->pp_types);
	m0_free(pver_policies());
	m0_get()->i_moddata[M0_MODULE_POOL] = NULL;
}

/** @} pool_policy */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
