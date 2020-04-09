/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * https://www.seagate.com/contacts
 *
 * Original author: Jean-Philippe Bernardy <jean-philippe.bernardy@tweag.io>
 * Original creation date: 15 Feb 2016
 * Subsequent modifications: Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
 * Date of modifications: 28 Nov 2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ISCS
#include "lib/trace.h"

#include "reqh/reqh_service.h"
#include "reqh/reqh.h"
#include "iscservice/isc_service.h"
#include "iscservice/isc_fops.h"
#include "lib/hash.h"
#include "module/instance.h"         /* m0_get() */
#include "lib/memory.h"
#include "fid/fid.h"
#include "mero/magic.h"
#include "iscservice/isc.h"

static int iscs_allocate(struct m0_reqh_service **service,
                         const struct m0_reqh_service_type *stype);
static void iscs_fini(struct m0_reqh_service *service);

static int iscs_start(struct m0_reqh_service *service);
static void iscs_stop(struct m0_reqh_service *service);

static bool comp_key_eq(const void *key1, const void *key2)
{
	return m0_fid_eq(key1, key2);
}

static uint64_t comp_hash_func(const struct m0_htable *htable, const void *k)
{
	return m0_fid_hash(k) % htable->h_bucket_nr;
}

M0_HT_DESCR_DEFINE(m0_isc, "Hash table for compute functions", M0_INTERNAL,
		   struct m0_isc_comp, ic_hlink, ic_magic,
		   M0_ISC_COMP_MAGIC, M0_ISC_TLIST_HEAD_MAGIC,
		   ic_fid, comp_hash_func, comp_key_eq);

M0_HT_DEFINE(m0_isc, M0_INTERNAL, struct m0_isc_comp, struct m0_fid);

enum {
	ISC_HT_BUCKET_NR = 100,
};

static const struct m0_reqh_service_type_ops iscs_type_ops = {
	.rsto_service_allocate = iscs_allocate
};

static const struct m0_reqh_service_ops iscs_ops = {
	.rso_start       = iscs_start,
	.rso_start_async = m0_reqh_service_async_start_simple,
	.rso_stop        = iscs_stop,
	.rso_fini        = iscs_fini
};

struct m0_reqh_service_type m0_iscs_type = {
	.rst_name = "M0_CST_ISCS",
	.rst_ops  = &iscs_type_ops,
	.rst_level = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_ISCS,
};

M0_INTERNAL struct m0_htable *m0_isc_htable_get(void)
{
	return m0_get()->i_moddata[M0_MODULE_ISC];
}

M0_INTERNAL int m0_isc_mod_init(void)
{
	struct m0_htable *isc_htable;

	M0_ALLOC_PTR(isc_htable);
	if (isc_htable == NULL)
		return M0_ERR(-ENOMEM);
	m0_get()->i_moddata[M0_MODULE_ISC] = isc_htable;
	return 0;
}

M0_INTERNAL void m0_isc_mod_fini(void)
{
	m0_free(m0_isc_htable_get());
}

M0_INTERNAL int m0_iscs_register(void)
{
	int rc;

	M0_ENTRY();
	rc = m0_reqh_service_type_register(&m0_iscs_type);
	M0_ASSERT(rc == 0);
	rc = m0_iscservice_fop_init();
	if (rc != 0)
		return M0_ERR_INFO(rc, "Fop initialization failed");
	m0_isc_fom_type_init();
	M0_LEAVE();
	return M0_RC(0);
}

M0_INTERNAL void m0_iscs_unregister(void)
{
	m0_reqh_service_type_unregister(&m0_iscs_type);
	m0_iscservice_fop_fini();
}

static int iscs_allocate(struct m0_reqh_service **service,
			 const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_isc_service *isc_svc;

	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(isc_svc);
	if (isc_svc == NULL)
		return M0_ERR(-ENOMEM);

	isc_svc->riscs_magic = M0_ISCS_REQH_SVC_MAGIC;

	*service = &isc_svc->riscs_gen;
	(*service)->rs_ops = &iscs_ops;

	return 0;
}

static int iscs_start(struct m0_reqh_service *service)
{
	int rc = 0;

	M0_ENTRY();
	rc = m0_isc_htable_init(m0_isc_htable_get(), ISC_HT_BUCKET_NR);
	if (rc != 0)
		return M0_ERR(rc);
	M0_LEAVE();
	return rc;
}

static void iscs_stop(struct m0_reqh_service *service)
{
	struct m0_isc_comp *isc_comp;

	M0_ENTRY();
	m0_htable_for(m0_isc, isc_comp, m0_isc_htable_get()) {
		M0_ASSERT(isc_comp->ic_ref_count == 0);
		m0_isc_comp_unregister(&isc_comp->ic_fid);
	} m0_htable_endfor;
	m0_isc_htable_fini(m0_isc_htable_get());
	M0_LEAVE();
}

static void iscs_fini(struct m0_reqh_service *service)
{
	struct m0_reqh_isc_service *isc_svc;

	M0_PRE(service != NULL);

	isc_svc = M0_AMB(isc_svc, service, riscs_gen);
	m0_free(isc_svc);
}

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
