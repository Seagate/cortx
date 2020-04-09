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
 * Original author: Trupti Patil <trupti_patil@xyratex.com>
 * Original creation date: 12/21/2011
 */

#include "ut/ut.h"
#include "lib/ub.h"
#include "lib/memory.h"
#include "lib/misc.h"                    /* M0_SET0 */
#include "lib/bitstring.h"
#include "lib/vec.h"
#include "lib/errno.h"     /* ENOENT */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"                   /* M0_LOG */

#ifdef __KERNEL__
# include "m0t1fs/linux_kernel/m0t1fs.h" /* m0t1fs_globals */
#endif

#include "lib/finject.h"

#include "pool/pool.h"                   /* m0_pool_init(), m0_pool_fini() */
#include "fid/fid.h"                     /* m0_fid_set() */
#include "layout/layout.h"
#include "layout/layout_internal.h"      /* LDB_MAX_INLINE_COB_ENTRIES, *_ERR */
#include "layout/pdclust.h"
//#include "layout/list_enum.h"
#include "layout/linear_enum.h"
#include "ioservice/fid_convert.h"       /* m0_fid_convert_gob2cob */

static struct m0_layout_domain domain;
static struct m0_pool          pool;
static int                     rc;

enum {
	DBFLAGS                  = 0,    /* Flag used for dbenv and tx init */
	LIST_ENUM_ID             = 0x4C495354, /* "LIST" */
	LINEAR_ENUM_ID           = 0x4C494E45, /* "LINE" */
	ADDITIONAL_BYTES_NONE    = 0,    /* For buffer initialisation */
	ADDITIONAL_BYTES_DEFAULT = 2048, /* For buffer initialisation */
	INLINE_NOT_APPLICABLE    = 0,    /* For list enumeration */
	LESS_THAN_INLINE         = 1,    /* For list enumeration */
	EXACT_INLINE             = 2,    /* For list enumeration */
	MORE_THAN_INLINE         = 3,    /* For list enumeration */
	EXISTING_TEST            = true, /* Add a layout to the DB */
	DUPLICATE_TEST           = true, /* Try to re-add a layout */
	FAILURE_TEST             = true, /* Failure injected */
	LAYOUT_DESTROY           = true, /* Delete layout object */
	UNIT_SIZE                = 4096  /* For pdclust layout type */
};

extern struct m0_layout_type m0_pdclust_layout_type;
//extern struct m0_layout_enum_type m0_list_enum_type;
extern struct m0_layout_enum_type m0_linear_enum_type;

static int test_init(void)
{
	/*
	 * Note: In test_init() and test_fini(), need to use M0_ASSERT()
	 * as against M0_UT_ASSERT().
	 */

	/* Initialise the domain. */
	rc = m0_layout_domain_init(&domain);
	M0_ASSERT(rc == 0);

	/* Register all the standard layout types and enum types. */
	rc = m0_layout_standard_types_register(&domain);
	M0_ASSERT(rc == 0);

	return rc;
}

static int test_fini(void)
{
	m0_layout_standard_types_unregister(&domain);
	m0_layout_domain_fini(&domain);

	return 0;
}

static void test_domain_init_fini(void)
{
	struct m0_layout_domain t_domain;

	M0_ENTRY();

	/* Initialise the domain. */
	rc = m0_layout_domain_init(&t_domain);
	M0_UT_ASSERT(rc == 0);

	/* Finalise the domain. */
	m0_layout_domain_fini(&t_domain);

	/* Should be able to initialise the domain again after finalising it. */
	rc = m0_layout_domain_init(&t_domain);
	M0_UT_ASSERT(rc == 0);

	/* Finalise the domain. */
	m0_layout_domain_fini(&t_domain);

	M0_LEAVE();
}

static void test_domain_init_fini_failure(void)
{
	struct m0_layout_domain t_domain;

	M0_ENTRY();

	m0_fi_enable_once("m0_layout_domain_init", "table_init_err");
	rc = m0_layout_domain_init(&t_domain);
	M0_UT_ASSERT(rc == L_TABLE_INIT_ERR);

	M0_LEAVE();
}

static int t_register(struct m0_layout_domain *dom,
		      const struct m0_layout_type *lt)
{
	return 0;
}

static void t_unregister(struct m0_layout_domain *dom,
			 const struct m0_layout_type *lt)
{
}

static m0_bcount_t t_max_recsize(struct m0_layout_domain *dom)
{
	return 0;
}

static const struct m0_layout_type_ops test_layout_type_ops = {
	.lto_register    = t_register,
	.lto_unregister  = t_unregister,
	.lto_max_recsize = t_max_recsize
};

struct m0_layout_type test_layout_type = {
	.lt_name     = "test",
	.lt_id       = 2,
	.lt_ops      = &test_layout_type_ops
};

static void test_type_reg_unreg(void)
{
	M0_ENTRY();

	/* Register a layout type. */
	rc = m0_layout_type_register(&domain, &test_layout_type);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(domain.ld_type[test_layout_type.lt_id] ==
		     &test_layout_type);

	/* Unregister it. */
	m0_layout_type_unregister(&domain, &test_layout_type);
	M0_UT_ASSERT(domain.ld_type[test_layout_type.lt_id] == NULL);

	M0_LEAVE();
}

static int t_enum_register(struct m0_layout_domain *dom,
			   const struct m0_layout_enum_type *et)
{
	return 0;
}

static void t_enum_unregister(struct m0_layout_domain *dom,
			      const struct m0_layout_enum_type *et)
{
}

static m0_bcount_t t_enum_max_recsize(void)
{
	return 0;
}

static const struct m0_layout_enum_type_ops test_enum_ops = {
	.leto_register    = t_enum_register,
	.leto_unregister  = t_enum_unregister,
	.leto_max_recsize = t_enum_max_recsize
};

struct m0_layout_enum_type test_enum_type = {
	.let_name = "test",
	.let_id   = 2,
	.let_ops  = &test_enum_ops
};

static void test_etype_reg_unreg(void)
{
	M0_ENTRY();

	/* Register a layout enum type. */
	rc = m0_layout_enum_type_register(&domain, &test_enum_type);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(domain.ld_enum[test_enum_type.let_id] ==
		     &test_enum_type);

	/* Unregister it. */
	m0_layout_enum_type_unregister(&domain, &test_enum_type);
	M0_UT_ASSERT(domain.ld_enum[test_enum_type.let_id] == NULL);

	M0_LEAVE();
}

static void test_reg_unreg(void)
{
	struct m0_layout_domain t_domain;

	M0_ENTRY();

	/*
	 * A layout type can be registered with only one domain at a time.
	 * Hence, unregister all the available layout types and enum types from
	 * the domain "domain", which are registered through test_init().
	 * This also covers the test of registering with one domain,
	 * unregistering from that domain and then registering with another
	 * domain.
	 */
	m0_layout_standard_types_unregister(&domain);

	/* Initialise the domain. */
	rc = m0_layout_domain_init(&t_domain);
	M0_UT_ASSERT(rc == 0);

	/* Register all the available layout types and enum types. */
	rc = m0_layout_standard_types_register(&t_domain);
	M0_UT_ASSERT(rc == 0);
/*	M0_UT_ASSERT(t_domain.ld_enum[m0_list_enum_type.let_id] ==
		     &m0_list_enum_type);*/
	M0_UT_ASSERT(t_domain.ld_enum[m0_linear_enum_type.let_id] ==
		     &m0_linear_enum_type);
	M0_UT_ASSERT(t_domain.ld_type[m0_pdclust_layout_type.lt_id] ==
		     &m0_pdclust_layout_type);

	/* Unregister all the registered layout and enum types. */
	m0_layout_standard_types_unregister(&t_domain);
	//M0_UT_ASSERT(t_domain.ld_enum[m0_list_enum_type.let_id] == NULL);
	M0_UT_ASSERT(t_domain.ld_enum[m0_linear_enum_type.let_id] == NULL);
	M0_UT_ASSERT(t_domain.ld_type[m0_pdclust_layout_type.lt_id] == NULL);

	/*
	 * Should be able to register all the available layout types and enum
	 * types, again after unregistering those.
	 */
	rc = m0_layout_standard_types_register(&t_domain);
	M0_UT_ASSERT(rc == 0);
	/*M0_UT_ASSERT(t_domain.ld_enum[m0_list_enum_type.let_id] ==
		     &m0_list_enum_type);*/
	M0_UT_ASSERT(t_domain.ld_enum[m0_linear_enum_type.let_id] ==
		     &m0_linear_enum_type);
	M0_UT_ASSERT(t_domain.ld_type[m0_pdclust_layout_type.lt_id] ==
		     &m0_pdclust_layout_type);

	/* Unregister all the registered layout and enum types. */
	m0_layout_standard_types_unregister(&t_domain);
	//M0_UT_ASSERT(t_domain.ld_enum[m0_list_enum_type.let_id] == NULL);
	M0_UT_ASSERT(t_domain.ld_enum[m0_linear_enum_type.let_id] == NULL);
	M0_UT_ASSERT(t_domain.ld_type[m0_pdclust_layout_type.lt_id] == NULL);

	/* Finalise the domain. */
	m0_layout_domain_fini(&t_domain);

	/*
	 * Register back all the available layout types and enum types with
	 * the domain "domain", to undo the change done at the beginning of
	 * this function.
	 */
	rc = m0_layout_standard_types_register(&domain);
	M0_ASSERT(rc == 0);

	M0_LEAVE();
}

static void test_reg_unreg_failure(void)
{
	struct m0_layout_domain t_domain;

	M0_ENTRY();

	/*
	 * A layout type can be registered with only one domain at a time.
	 * Hence, unregister all the available layout types and enum types from
	 * the domain "domain", which are registered through test_init().
	 * This also covers the test of registering with one domain,
	 * unregistering from that domain and then registering with another
	 * domain.
	 */
	m0_layout_standard_types_unregister(&domain);

	/* Initialise the domain. */
	rc = m0_layout_domain_init(&t_domain);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Try to register all the standard layout types and enum types by
	 * injecting errors.
	 */
	m0_fi_enable_once("m0_layout_type_register", "lto_reg_err");
	rc = m0_layout_type_register(&t_domain, &m0_pdclust_layout_type);
	M0_UT_ASSERT(rc == LTO_REG_ERR);

	/*m0_fi_enable_once("m0_layout_enum_type_register", "leto_reg_err");
	rc = m0_layout_enum_type_register(&t_domain, &m0_list_enum_type);
	M0_UT_ASSERT(rc == LETO_REG_ERR);*/

	m0_fi_enable_once("m0_layout_enum_type_register", "leto_reg_err");
	rc = m0_layout_enum_type_register(&t_domain, &m0_linear_enum_type);
	M0_UT_ASSERT(rc == LETO_REG_ERR);

	/*m0_fi_enable_once("list_register", "mem_err");
	rc = m0_layout_enum_type_register(&t_domain, &m0_list_enum_type);
	M0_UT_ASSERT(rc == -ENOMEM);*/

	/*m0_fi_enable_once("list_register", "table_init_err");
	rc = m0_layout_enum_type_register(&t_domain, &m0_list_enum_type);
	M0_UT_ASSERT(rc == -EEXIST);*/

	/*
	 * Now cover all the error cases from
	 * m0_layout_standard_types_register().
	 */
	m0_fi_enable_once("m0_layout_type_register", "lto_reg_err");
	rc = m0_layout_standard_types_register(&t_domain);
	M0_UT_ASSERT(rc == LTO_REG_ERR);

	m0_fi_enable_once("m0_layout_enum_type_register", "leto_reg_err");
	rc = m0_layout_standard_types_register(&t_domain);
	M0_UT_ASSERT(rc == LETO_REG_ERR);

	m0_fi_enable_once("m0_layout_enum_type_register", "leto_reg_err");
	rc = m0_layout_standard_types_register(&t_domain);
	M0_UT_ASSERT(rc == LETO_REG_ERR);
	m0_fi_disable("m0_layout_enum_type_register", "leto_reg_err");

	m0_layout_domain_fini(&t_domain);

	/*
	 * Register back all the available layout types and enum types with
	 * the domain "domain", to undo the change done at the beginning of
	 * this function.
	 */
	rc = m0_layout_standard_types_register(&domain);
	M0_ASSERT(rc == 0);

	M0_LEAVE();
}

static struct m0_layout *list_lookup(uint64_t lid)
{
	struct m0_layout *l;

	m0_mutex_lock(&domain.ld_lock);
	l = m0_layout__list_lookup(&domain, lid, false);
	m0_mutex_unlock(&domain.ld_lock);
	return l;
}

/*
 * Builds a layout object with PDCLUST layout type and using the provided
 * enumeration object.
 */
static int pdclust_l_build(uint64_t lid, uint32_t N, uint32_t K, uint32_t P,
			   struct m0_uint128 *seed,
			   struct m0_layout_enum *le,
			   struct m0_pdclust_layout **pl,
			   bool failure_test)
{
	struct m0_pdclust_attr  attr;

	M0_UT_ASSERT(le != NULL);
	M0_UT_ASSERT(pl != NULL);

	attr.pa_N         = N;
	attr.pa_K         = K;
	attr.pa_P         = P;
	attr.pa_unit_size = UNIT_SIZE;
	attr.pa_seed      = *seed;

	if (M0_FI_ENABLED("attr_err")) { attr.pa_P = 1; }
	rc = m0_pdclust_build(&domain, lid, &attr, le, pl);
	if (failure_test)
		M0_UT_ASSERT(rc == -ENOMEM || rc == -EPROTO);
	else {
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(list_lookup(lid) == &(*pl)->pl_base.sl_base);
		M0_UT_ASSERT(m0_ref_read(&(*pl)->pl_base.sl_base.l_ref) == 1);
		M0_UT_ASSERT((*pl)->pl_base.sl_base.l_user_count == 0);
	}

	return rc;
}

/*
 * Builds a layout object with PDCLUST layout type, by first building an
 * enumeration object with the specified enumeration type.
 */
static int pdclust_layout_build(uint32_t enum_id,
				uint64_t lid,
				uint32_t N, uint32_t K, uint32_t P,
				struct m0_uint128 *seed,
				uint32_t A, uint32_t B,
				struct m0_pdclust_layout **pl,
//				struct m0_layout_list_enum **list_enum,
				struct m0_layout_linear_enum **lin_enum,
				bool failure_test)
{
//	struct m0_fid                *cob_list = NULL; /* to keep gcc happy. */
//	int                           i;
	struct m0_layout_enum        *e;
	struct m0_layout_linear_attr  lin_attr;
	struct m0_layout             *l_from_pl;
	struct m0_layout_enum        *e_from_layout;

	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);
	M0_UT_ASSERT(pl != NULL);

#if 0
	/* Build an enumeration object with the specified enum type. */
	if (enum_id == LIST_ENUM_ID) {
		M0_ALLOC_ARR(cob_list, P);
		M0_UT_ASSERT(cob_list != NULL);

		for (i = 0; i < P; ++i)
			m0_fid_set(&cob_list[i], i * 100 + 1, i + 1);

		if (M0_FI_ENABLED("list_attr_err")) { P = 0; }
		rc = m0_list_enum_build(&domain, cob_list, P, list_enum);
		M0_UT_ASSERT(rc == 0 || rc == -ENOMEM || rc == -EPROTO);

		e = &(*list_enum)->lle_base;

	} else { /* LINEAR_ENUM_ID */
#endif
		lin_attr.lla_nr = P;
		lin_attr.lla_A  = A;
		lin_attr.lla_B  = B;
		if (M0_FI_ENABLED("lin_attr_err")) { lin_attr.lla_nr = 0; }
		rc = m0_linear_enum_build(&domain, &lin_attr, lin_enum);
		M0_UT_ASSERT(rc == 0 || rc == -ENOMEM || rc == -EPROTO);

		e = &(*lin_enum)->lle_base;
#if 0
	}
#endif
	if (rc != 0) {
		M0_UT_ASSERT(failure_test);
#if 0
		if (enum_id == LIST_ENUM_ID)
			m0_free(cob_list);
#endif
		return rc;
	}

	/*
	 * Build a layout object with PDCLUST layout type and using the
	 * enumeration object built earlier here.
	 */
	rc = pdclust_l_build(lid, N, K, P, seed, e, pl, failure_test);
	if (failure_test) {
		M0_UT_ASSERT(rc == -ENOMEM || rc == -EPROTO);
		m0_layout_enum_fini(e);
		return rc;
	}
	else {
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(list_lookup(lid) == &(*pl)->pl_base.sl_base);
	}

	/* Verify m0_pdl_to_layout(). */
	l_from_pl = m0_pdl_to_layout(*pl);
	M0_UT_ASSERT(l_from_pl == &(*pl)->pl_base.sl_base);

	/* Verify m0_layout_to_enum(). */
	e_from_layout = m0_layout_to_enum(l_from_pl);
	M0_UT_ASSERT(e_from_layout == e);
	return rc;
}

/* Verifies generic part of the layout object. */
static void l_verify(struct m0_layout *l, uint64_t lid)
{
	M0_UT_ASSERT(l->l_id == lid);
	M0_UT_ASSERT(m0_ref_read(&l->l_ref) >= 0);
	M0_UT_ASSERT(l->l_ops != NULL);
}

/*
 * Verifies generic part of the layout object and the PDCLUST layout type
 * specific part of it.
 */
static void pdclust_l_verify(struct m0_pdclust_layout *pl,
			     uint64_t lid,
			     uint32_t N, uint32_t K, uint32_t P,
			     struct m0_uint128 *seed)
{
	/* Verify generic part of the layout object. */
	l_verify(&pl->pl_base.sl_base, lid);

	/* Verify PDCLUST layout type specific part of the layout object. */
	M0_UT_ASSERT(pl->pl_attr.pa_N == N);
	M0_UT_ASSERT(pl->pl_attr.pa_K == K);
	M0_UT_ASSERT(pl->pl_attr.pa_P == P);
	M0_UT_ASSERT(pl->pl_attr.pa_unit_size == UNIT_SIZE);
	M0_UT_ASSERT(m0_uint128_eq(&pl->pl_attr.pa_seed, seed));
}

/* Verifies the layout object against the various input arguments. */
static void pdclust_layout_verify(uint32_t enum_id,
				  struct m0_layout *l, uint64_t lid,
				  uint32_t N, uint32_t K, uint32_t P,
				  struct m0_uint128 *seed,
				  uint32_t A, uint32_t B)
{
	struct m0_pdclust_layout     *pl;
//	struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
//	int                           i;
//	struct m0_fid                 cob_id;

	M0_UT_ASSERT(l != NULL);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);
	M0_UT_ASSERT(l->l_type == &m0_pdclust_layout_type);

	pl = container_of(l, struct m0_pdclust_layout, pl_base.sl_base);

	/*
	 * Verify generic and PDCLUST layout type specific parts of the
	 * layout object.
	 */
	pdclust_l_verify(pl, lid, N, K, P, seed);

	/* Verify enum type specific part of the layout object. */
	M0_UT_ASSERT(pl->pl_base.sl_enum != NULL);

#if 0
	if (enum_id == LIST_ENUM_ID) {
		list_enum = container_of(pl->pl_base.sl_enum,
					 struct m0_layout_list_enum, lle_base);
		for(i = 0; i < list_enum->lle_nr; ++i) {
			m0_fid_set(&cob_id, i * 100 + 1, i + 1);
			M0_UT_ASSERT(m0_fid_eq(&cob_id,
					      &list_enum->lle_list_of_cobs[i]));
		}
		M0_UT_ASSERT(list_enum->lle_nr == P);
	} else {
#endif
		lin_enum = container_of(pl->pl_base.sl_enum,
					struct m0_layout_linear_enum, lle_base);
		M0_UT_ASSERT(lin_enum->lle_attr.lla_nr == P);
		M0_UT_ASSERT(lin_enum->lle_attr.lla_A == A);
		M0_UT_ASSERT(lin_enum->lle_attr.lla_B == B);
#if 0
	}
#endif
}

static void NKP_assign_and_pool_init(uint32_t enum_id,
				     uint32_t inline_test,
				     uint32_t list_nr_less,
				     uint32_t list_nr_more,
				     uint32_t linear_nr,
				     uint32_t *N, uint32_t *K, uint32_t *P)
{
	M0_UT_ASSERT(ergo(enum_id == LIST_ENUM_ID,
			  list_nr_less < LDB_MAX_INLINE_COB_ENTRIES &&
			  list_nr_more > LDB_MAX_INLINE_COB_ENTRIES));

	/**
	 * @todo This is a hack to be taken out along with the forthcoming
	 * patch for the layout module to use xcode and newer BE, with which
	 * the whole layout test suite will anyway be restructured.
	 */
	if (list_nr_more > 50)
		list_nr_more = 50;

#if 0
	if (enum_id == LIST_ENUM_ID) {
		switch (inline_test) {
		case LESS_THAN_INLINE:
			*P = list_nr_less;
			break;
		case EXACT_INLINE:
			*P = LDB_MAX_INLINE_COB_ENTRIES;
			break;
		case MORE_THAN_INLINE:
			*P = list_nr_more;
			break;
		default:
			M0_ASSERT(0);
		}
	} else {
#endif
		*P = linear_nr;
#if 0
	}
#endif

	if (*P <= 20)
		*K = 1;
	else if (*P <= 50)
		*K = 2;
	else if (*P <= 200)
		*K = 6;
	else if (*P <= 500)
		*K = 12;
	else if (*P <= 1000)
		*K = 100;
	else
		*K = 200;

	if (*P <= 20)
		*N = *P - (2 * (*K));
	else if (*P <= 100)
		*N = *P - (2 * (*K)) - 10;
	else if (*P <= 1000)
		*N = *P - (2 * (*K)) - 12;
	else
		*N = *P - (2 * (*K)) - 100;

	rc = m0_pool_init(&pool, &M0_FID_INIT(0, enum_id), 0);
	M0_ASSERT(rc == 0);
}

/*
 * Tests the APIs supported for enumeration object build, layout object build
 * and layout destruction that happens using m0_layout_put(). Verifies that the
 * newly built layout object is added to the list of layout objects maintained
 * in the domain object and that m0_layout_find() returns the same object.
 */
static int test_build_pdclust(uint32_t enum_id, uint64_t lid,
			      uint32_t inline_test,
			      bool failure_test)
{
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	struct m0_pdclust_layout     *pl;
//	struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	struct m0_layout             *l;

	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "buildpdclustlayo");

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 9, 109, 12000,
				 &N, &K, &P);

	rc = pdclust_layout_build(enum_id, lid,
				  N, K, P, &seed,
				  10, 20,
				  &pl, /*&list_enum, */&lin_enum,
				  failure_test);
	if (failure_test)
		M0_UT_ASSERT(rc == -ENOMEM || rc == -EPROTO);
	else {
		M0_UT_ASSERT(rc == 0);
		/*
		 * Verify that m0_layout_find() returns the same object by
		 * reading it from the memory.
		 */
		l = m0_layout_find(&domain, lid);
		M0_UT_ASSERT(l == &pl->pl_base.sl_base);

		/* Verify the layout object built earlier here. */
		pdclust_layout_verify(enum_id, &pl->pl_base.sl_base, lid,
				      N, K, P, &seed,
				      10, 20);
		/* Release the reference acquired by m0_layout_find(). */
		m0_layout_put(&pl->pl_base.sl_base);

		/* Delete the layout object by reducing the last reference. */
		m0_layout_put(&pl->pl_base.sl_base);
		M0_UT_ASSERT(list_lookup(lid) == NULL);
	}

	m0_pool_fini(&pool);
	return rc;
}

/*
 * Tests the APIs supported for enumeration object build, layout object build
 * and layout destruction that happens using m0_layout_put().
 */
static void test_build(void)
{
	uint64_t lid;

#if 0
	/*
	 * Build a layout object with PDCLUST layout type, LIST enum type
	 * with a few inline entries only and then destroy it.
	 */
	lid = 1001;
	rc = test_build_pdclust(LIST_ENUM_ID, lid, LESS_THAN_INLINE,
				!FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout object with PDCLUST layout type, LIST enum type
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES and then destroy it.
	 */
	lid = 1002;
	rc = test_build_pdclust(LIST_ENUM_ID, lid, EXACT_INLINE,
				!FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout object with PDCLUST layout type, LIST enum type
	 * including noninline entries and then destroy it.
	 */
	lid = 1003;
	rc = test_build_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				!FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Build a layout object with PDCLUST layout type and LINEAR enum
	 * type and then destroy it.
	 */
	lid = 1004;
	rc = test_build_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				!FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
}

static void test_build_failure(void)
{
	uint64_t lid;

#if 0
	/*
	 * Simulate memory allocation failure in pdclust_allocate() that is
	 * in the path of m0_pdclust_build().
	 */
	lid = 2001;
	m0_fi_enable_once("pdclust_allocate", "mem_err");
	rc = test_build_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);
#endif

	/*
	 * Simulate memory allocation failure in pdclust_allocate() that is
	 * in the path of m0_pdclust_build().
	 */
	lid = 2002;
	m0_fi_enable_once("pdclust_allocate", "mem_err");
	rc = test_build_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);

#if 0
	/*
	 * Simulate invalid attributes error in pdclust_populate() that is
	 * in the path of m0_pdclust_build().
	 */
	lid = 2003;
	m0_fi_enable_once("pdclust_l_build", "attr_err");
	rc = test_build_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);

	/*
	 * Simulate memory allocation failure in linear_allocate() that is
	 * in the path of m0_pdclust_build().
	 */
	lid = 2004;
	m0_fi_enable_once("list_allocate", "mem_err");
	rc = test_build_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);
#endif

	/*
	 * Simulate memory allocation failure in linear_allocate() that is
	 * in the path of m0_pdclust_build().
	 */
	lid = 2005;
	m0_fi_enable_once("linear_allocate", "mem_err");
	rc = test_build_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);

#if 0
	/* Simulate attributes invalid error in m0_list_enum_build(). */
	lid = 2006;
	m0_fi_enable_once("pdclust_layout_build", "list_attr_err");
	rc = test_build_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
#endif

	/* Simulate attributes invalid error in m0_linear_enum_build(). */
	lid = 2007;
	m0_fi_enable_once("pdclust_layout_build", "lin_attr_err");
	rc = test_build_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);

#if 0
	/* Simulate fid invalid error in m0_list_enum_build(). */
	lid = 2008;
	m0_fi_enable_once("m0_list_enum_build", "fid_invalid_err");
	rc = test_build_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
#endif
}


/* Builds part of the buffer representing generic part of the layout object. */
static void buf_build(uint32_t lt_id, struct m0_bufvec_cursor *dcur)
{
	struct m0_layout_rec rec;
	m0_bcount_t          nbytes_copied;

	rec.lr_lt_id      = lt_id;
	rec.lr_user_count = 0;

	nbytes_copied = m0_bufvec_cursor_copyto(dcur, &rec, sizeof rec);
	M0_UT_ASSERT(nbytes_copied == sizeof rec);
}

/*
 * Builds part of the buffer representing generic and PDCLUST layout type
 * specific parts of the layout object.
 */
static void pdclust_buf_build(uint32_t let_id, uint64_t lid,
			      uint32_t N, uint32_t K, uint32_t P,
			      struct m0_uint128 *seed,
			      struct m0_bufvec_cursor *dcur)
{
	struct m0_layout_pdclust_rec pl_rec;
	m0_bcount_t                  nbytes_copied;

	buf_build(m0_pdclust_layout_type.lt_id, dcur);

	pl_rec.pr_let_id            = let_id;
	pl_rec.pr_attr.pa_N         = N;
	pl_rec.pr_attr.pa_K         = K;
	pl_rec.pr_attr.pa_P         = P;
	pl_rec.pr_attr.pa_unit_size = UNIT_SIZE;
	pl_rec.pr_attr.pa_seed      = *seed;

	nbytes_copied = m0_bufvec_cursor_copyto(dcur, &pl_rec, sizeof pl_rec);
	M0_UT_ASSERT(nbytes_copied == sizeof pl_rec);
}

/* Builds a buffer containing serialised representation of a layout object. */
static int pdclust_layout_buf_build(uint32_t enum_id, uint64_t lid,
				    uint32_t N, uint32_t K, uint32_t P,
				    struct m0_uint128 *seed,
				    uint32_t A, uint32_t B,
				    struct m0_bufvec_cursor *dcur)
{
	uint32_t                     let_id;
	m0_bcount_t                  nbytes_copied;
//	struct cob_entries_header    ce_header;
/*	struct m0_fid                cob_id;
	uint32_t                     i;*/
	struct m0_layout_linear_attr lin_rec;

	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);
	M0_UT_ASSERT(dcur != NULL);

	/*
	 * Build part of the buffer representing generic and the PDCLUST layout
	 * type specific parts of the layout object.
	 */
	let_id = /*enum_id == LIST_ENUM_ID ? m0_list_enum_type.let_id :*/
					   m0_linear_enum_type.let_id;
	pdclust_buf_build(let_id, lid, N, K, P, seed, dcur);

#if 0
	/*
	 * Build part of the buffer representing enum type specific part of
	 * the layout object.
	 */
	if (enum_id == LIST_ENUM_ID) {
		ce_header.ces_nr = P;
		nbytes_copied = m0_bufvec_cursor_copyto(dcur, &ce_header,
							sizeof ce_header);
		M0_UT_ASSERT(nbytes_copied == sizeof ce_header);

		for (i = 0; i < ce_header.ces_nr; ++i) {
			m0_fid_set(&cob_id, i * 100 + 1, i + 1);
			nbytes_copied = m0_bufvec_cursor_copyto(dcur, &cob_id,
								sizeof cob_id);
			M0_UT_ASSERT(nbytes_copied == sizeof cob_id);
		}
	} else {
#endif
		lin_rec.lla_nr = P;
		lin_rec.lla_A  = A;
		lin_rec.lla_B  = B;

		nbytes_copied = m0_bufvec_cursor_copyto(dcur, &lin_rec,
							sizeof lin_rec);
		M0_UT_ASSERT(nbytes_copied == sizeof lin_rec);
#if 0
	}
#endif
	return 0;
}

/*
 * Allocates area with size returned by m0_layout_max_recsize() and with
 * additional_bytes required if any.
 * For example, additional_bytes are required for LIST enumeration type, and
 * specifically when directly invoking 'm0_layout_encode() or
 * m0_layout_decode()' (and not while invoking Layout DB APIs like
 * m0_layout_add() etc).
 */
static void allocate_area(void **area,
			  m0_bcount_t additional_bytes,
			  m0_bcount_t *num_bytes)
{
	M0_UT_ASSERT(area != NULL);

	*num_bytes = m0_layout_max_recsize(&domain) + additional_bytes;

	*area = m0_alloc(*num_bytes);
	M0_UT_ASSERT(*area != NULL);
}

/* Tests the API m0_layout_decode() for PDCLUST layout type. */
static int test_decode_pdclust(uint32_t enum_id, uint64_t lid,
			       uint32_t inline_test,
			       bool failure_test)
{
	void                    *area;
	m0_bcount_t              num_bytes;
	struct m0_bufvec         bv;
	struct m0_bufvec_cursor  cur;
	struct m0_layout        *l;
	struct m0_uint128        seed;
	uint32_t                 N;
	uint32_t                 K;
	uint32_t                 P;
	struct m0_layout_type   *lt;

	M0_ENTRY();
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "decodepdclustlay");

	/* Build a layout buffer. */
	if (enum_id == LIST_ENUM_ID)
		allocate_area(&area, ADDITIONAL_BYTES_DEFAULT, &num_bytes);
	else
		allocate_area(&area, ADDITIONAL_BYTES_NONE, &num_bytes);

	bv = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&area, &num_bytes);
	m0_bufvec_cursor_init(&cur, &bv);

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 5, 125, 1500,
				 &N, &K, &P);

	rc = pdclust_layout_buf_build(enum_id, lid,
				      N, K, P, &seed,
				      777, 888, &cur);
	M0_UT_ASSERT(rc == 0);

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&cur, &bv);

	lt = &m0_pdclust_layout_type;
	rc = lt->lt_ops->lto_allocate(&domain, lid, &l);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_layout__allocated_invariant(l));

	/* Decode the layout buffer into a layout object. */
	rc = m0_layout_decode(l, &cur, M0_LXO_BUFFER_OP, NULL);
	if (failure_test)
		M0_UT_ASSERT(rc == -ENOMEM || rc == -EPROTO);
	else {
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(list_lookup(lid) == l);

		/* Verify the layout object built by m0_layout_decode(). */
		pdclust_layout_verify(enum_id, l, lid,
				      N, K, P, &seed,
				      777, 888);
	}

	/* Destroy the layout object. */
	if (failure_test)
		l->l_ops->lo_delete(l);
	else {
		/* Unlock the layout, locked by lto_allocate() */
		m0_mutex_unlock(&l->l_lock);
		m0_layout_put(l);
	}
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	m0_free(area);
	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the API m0_layout_decode(). */
static void test_decode(void)
{
	uint64_t lid;

#if 0
	/*
	 * Decode a layout object with PDCLUST layout type, LIST enum type
	 * with a few inline entries only.
	 */
	lid = 3001;
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, LESS_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Decode a layout object with PDCLUST layout type, LIST enum type
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES and then destroy it.
	 */
	lid = 3002;
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, EXACT_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Decode a layout object with PDCLUST layout type, LIST enum type
	 * including noninline entries and then destroy it.
	 */
	lid = 3003;
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Decode a layout object with PDCLUST layout type and LINEAR enum
	 * type.
	 */
	lid = 3004;
	rc = test_decode_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
}

static void test_decode_failure(void)
{
	uint64_t lid;

#if 0
	/* Simulate invalid attributes error in m0_layout_decode(). */
	lid = 4001;
	m0_fi_enable_once("m0_layout_decode", "attr_err");
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);

	/* Simulate invalid attributes error in pdclust_decode(). */
	lid = 4002;
	m0_fi_enable_once("pdclust_decode", "attr_err1");
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);

	/* Simulate invalid attributes error in pdclust_decode(). */
	lid = 4003;
	m0_fi_enable_once("pdclust_decode", "attr_err2");
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);

	/* Simulate invalid attributes error in list_populate(). */
	lid = 4004;
	m0_fi_enable_once("list_decode", "attr_err");
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
#endif

	/* Simulate invalid attributes error in linear_populate(). */
	lid = 4005;
	m0_fi_enable_once("linear_decode", "attr_err");
	rc = test_decode_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);

#if 0
	/* Simulate memory allocation failure in list_decode(). */
	lid = 4006;
	m0_fi_enable_once("list_decode", "mem_err");
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);

	/* Simulate fid invalid error in list_decode(). */
	lid = 4007;
	m0_fi_enable_once("list_decode", "fid_invalid_err");
	rc = test_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
#endif

	/* Simulate leto_allocate() failure in pdclust_decode(). */
	lid = 4008;
	m0_fi_enable_once("linear_allocate", "mem_err");
	rc = test_decode_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);

	/* Simulate pdclust_populate() failure in pdclust_decode(). */
	lid = 4009;
	m0_fi_enable_once("pdclust_decode", "attr_err3");
	rc = test_decode_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
}

/*
 * Verifies part of the layout buffer representing generic part of the layout
 * object.
 */
static void lbuf_verify(struct m0_bufvec_cursor *cur, uint32_t *lt_id)
{
	struct m0_layout_rec *rec;

	M0_UT_ASSERT(m0_bufvec_cursor_step(cur) >= sizeof *rec);

	rec = m0_bufvec_cursor_addr(cur);
	M0_UT_ASSERT(rec != NULL);

	*lt_id = rec->lr_lt_id;

	M0_UT_ASSERT(rec->lr_user_count == 0);

	m0_bufvec_cursor_move(cur, sizeof *rec);
}

/*
 * Verifies part of the layout buffer representing PDCLUST layout type specific
 * part of the layout object.
 */
static void pdclust_lbuf_verify(uint32_t N, uint32_t K, uint32_t P,
				struct m0_uint128 *seed,
				struct m0_bufvec_cursor *cur,
				uint32_t *let_id)
{
	struct m0_layout_pdclust_rec *pl_rec;

	M0_UT_ASSERT(m0_bufvec_cursor_step(cur) >= sizeof *pl_rec);

	pl_rec = m0_bufvec_cursor_addr(cur);

	M0_UT_ASSERT(pl_rec->pr_attr.pa_N == N);
	M0_UT_ASSERT(pl_rec->pr_attr.pa_K == K);
	M0_UT_ASSERT(pl_rec->pr_attr.pa_P == P);
	M0_UT_ASSERT(m0_uint128_eq(&pl_rec->pr_attr.pa_seed, seed));
	M0_UT_ASSERT(pl_rec->pr_attr.pa_unit_size == UNIT_SIZE);

	*let_id = pl_rec->pr_let_id;
	m0_bufvec_cursor_move(cur, sizeof *pl_rec);
}

/* Verifies layout buffer against the various input arguments. */
static void pdclust_layout_buf_verify(uint32_t enum_id, uint64_t lid,
				      uint32_t N, uint32_t K, uint32_t P,
				      struct m0_uint128 *seed,
				      uint32_t A, uint32_t B,
				      struct m0_bufvec_cursor *cur)
{
	uint32_t                      lt_id;
	uint32_t                      let_id;
/*	uint32_t                      i;
	struct cob_entries_header    *ce_header;
	struct m0_fid                *cob_id_from_layout;
	struct m0_fid                 cob_id_calculated;*/
	struct m0_layout_linear_attr *lin_attr;

	M0_UT_ASSERT(cur != NULL);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	/* Verify generic part of the layout buffer. */
	lbuf_verify(cur, &lt_id);
	M0_UT_ASSERT(lt_id == m0_pdclust_layout_type.lt_id);

	/* Verify PDCLUST layout type specific part of the layout buffer. */
	pdclust_lbuf_verify(N, K, P, seed, cur, &let_id);

	/* Verify enum type specific part of the layout buffer. */
#if 0
	if (enum_id == LIST_ENUM_ID) {
		M0_UT_ASSERT(let_id == m0_list_enum_type.let_id);

		M0_UT_ASSERT(m0_bufvec_cursor_step(cur) >= sizeof *ce_header);

		ce_header = m0_bufvec_cursor_addr(cur);
		M0_UT_ASSERT(ce_header != NULL);
		m0_bufvec_cursor_move(cur, sizeof *ce_header);

		M0_UT_ASSERT(ce_header->ces_nr == P);
		M0_UT_ASSERT(m0_bufvec_cursor_step(cur) >=
			     ce_header->ces_nr * sizeof *cob_id_from_layout);

		for (i = 0; i < ce_header->ces_nr; ++i) {
			cob_id_from_layout = m0_bufvec_cursor_addr(cur);
			M0_UT_ASSERT(cob_id_from_layout != NULL);

			m0_fid_set(&cob_id_calculated, i * 100 + 1, i + 1);
			M0_UT_ASSERT(m0_fid_eq(cob_id_from_layout,
					       &cob_id_calculated));

			m0_bufvec_cursor_move(cur, sizeof *cob_id_from_layout);
		}
	} else {
#endif
		M0_UT_ASSERT(let_id == m0_linear_enum_type.let_id);

		M0_UT_ASSERT(m0_bufvec_cursor_step(cur) >= sizeof *lin_attr);

		lin_attr = m0_bufvec_cursor_addr(cur);
		M0_UT_ASSERT(lin_attr->lla_nr == P);
		M0_UT_ASSERT(lin_attr->lla_A == A);
		M0_UT_ASSERT(lin_attr->lla_B == B);
#if 0
	}
#endif
}

/* Tests the API m0_layout_encode() for PDCLUST layout type. */
static int test_encode_pdclust(uint32_t enum_id, uint64_t lid,
			       uint32_t inline_test,
			       bool failure_test)
{
	struct m0_pdclust_layout     *pl;
	void                         *area;
	m0_bcount_t                   num_bytes;
	struct m0_bufvec              bv;
	struct m0_bufvec_cursor       cur;
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
//	struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "encodepdclustlay");

	/* Build a layout object. */
	if (enum_id == LIST_ENUM_ID)
		allocate_area(&area, ADDITIONAL_BYTES_DEFAULT, &num_bytes);
	else
		allocate_area(&area, ADDITIONAL_BYTES_NONE, &num_bytes);

	bv = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&area, &num_bytes);
	m0_bufvec_cursor_init(&cur, &bv);

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 10, 120, 120,
				 &N, &K, &P);

	rc = pdclust_layout_build(enum_id, lid,
				  N, K, P, &seed,
				  11, 21,
				  &pl, /*&list_enum, */&lin_enum,
				  !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/* Encode the layout object into a layout buffer. */
	m0_mutex_lock(&pl->pl_base.sl_base.l_lock);
	rc  = m0_layout_encode(&pl->pl_base.sl_base, M0_LXO_BUFFER_OP,
			       NULL, &cur);
	m0_mutex_unlock(&pl->pl_base.sl_base.l_lock);
	if (failure_test)
		M0_UT_ASSERT(rc == LO_ENCODE_ERR);
	else
		M0_UT_ASSERT(rc == 0);

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&cur, &bv);

	/* Verify the layout buffer produced by m0_layout_encode(). */
	if (!failure_test)
		pdclust_layout_buf_verify(enum_id, lid,
					  N, K, P, &seed,
					  11, 21, &cur);

	/* Delete the layout object. */
	m0_layout_put(&pl->pl_base.sl_base);
	M0_UT_ASSERT(list_lookup(lid) == NULL);
	m0_free(area);

	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the API m0_layout_encode(). */
static void test_encode(void)
{
	uint64_t lid;
	int      rc;

#if 0
	/*
	 * Encode for PDCLUST layout type and LIST enumeration type,
	 * with a few inline entries only.
	 */
	lid = 5001;
	rc = test_encode_pdclust(LIST_ENUM_ID, lid, LESS_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Encode for PDCLUST layout type and LIST enumeration type,
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES.
	 */
	lid = 5002;
	rc = test_encode_pdclust(LIST_ENUM_ID, lid, EXACT_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Encode for PDCLUST layout type and LIST enumeration type,
	 * including noninline entries and then destroy it.
	 */
	lid = 5003;
	rc = test_encode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
#endif

	/* Encode for PDCLUST layout type and LINEAR enumeration type. */
	lid = 5004;
	rc = test_encode_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
}

static void test_encode_failure(void)
{
	uint64_t lid;
	int      rc;

#if 0
	/* Simulate m0_layout_encode() failure. */
	lid = 6001;
	m0_fi_enable_once("m0_layout_encode", "lo_encode_err");
	rc = test_encode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == LO_ENCODE_ERR);
#endif

	/* Simulate m0_layout_encode() failure. */
	lid = 6002;
	m0_fi_enable_once("m0_layout_encode", "lo_encode_err");
	rc = test_encode_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == LO_ENCODE_ERR);
}


/* Compares generic part of the layout buffers. */
static void lbuf_compare(struct m0_bufvec_cursor *cur1,
			 struct m0_bufvec_cursor *cur2)
{
	struct m0_layout_rec *rec1;
	struct m0_layout_rec *rec2;

	M0_UT_ASSERT(m0_bufvec_cursor_step(cur1) >= sizeof *rec2);
	M0_UT_ASSERT(m0_bufvec_cursor_step(cur2) >= sizeof *rec2);

	rec1 = m0_bufvec_cursor_addr(cur1);
	rec2 = m0_bufvec_cursor_addr(cur2);

	M0_UT_ASSERT(rec1->lr_lt_id == rec2->lr_lt_id);
	M0_UT_ASSERT(rec1->lr_user_count == rec2->lr_user_count);

	m0_bufvec_cursor_move(cur1, sizeof *rec1);
	m0_bufvec_cursor_move(cur2, sizeof *rec2);
}

/* Compares PDCLUST layout type specific part of the layout buffers. */
static void pdclust_lbuf_compare(struct m0_bufvec_cursor *cur1,
				 struct m0_bufvec_cursor *cur2)
{
	struct m0_layout_pdclust_rec *pl_rec1;
	struct m0_layout_pdclust_rec *pl_rec2;

	M0_UT_ASSERT(m0_bufvec_cursor_step(cur1) >= sizeof *pl_rec1);
	M0_UT_ASSERT(m0_bufvec_cursor_step(cur2) >= sizeof *pl_rec2);

	pl_rec1 = m0_bufvec_cursor_addr(cur1);
	pl_rec2 = m0_bufvec_cursor_addr(cur2);

	M0_UT_ASSERT(pl_rec1->pr_attr.pa_N == pl_rec2->pr_attr.pa_N);
	M0_UT_ASSERT(pl_rec1->pr_attr.pa_K == pl_rec2->pr_attr.pa_K);
	M0_UT_ASSERT(pl_rec1->pr_attr.pa_P == pl_rec2->pr_attr.pa_P);
	M0_UT_ASSERT(m0_uint128_eq(&pl_rec1->pr_attr.pa_seed,
				   &pl_rec2->pr_attr.pa_seed));
	M0_UT_ASSERT(pl_rec1->pr_attr.pa_unit_size ==
		     pl_rec2->pr_attr.pa_unit_size);

	m0_bufvec_cursor_move(cur1, sizeof *pl_rec1);
	m0_bufvec_cursor_move(cur2, sizeof *pl_rec2);
}

/* Compares two layout buffers provided as input arguments. */
static void pdclust_layout_buf_compare(uint32_t enum_id,
				       struct m0_bufvec_cursor *cur1,
				       struct m0_bufvec_cursor *cur2)
{
/*	struct cob_entries_header    *ce_header1;
	struct cob_entries_header    *ce_header2;
	struct m0_fid                *cob_id1;
	struct m0_fid                *cob_id2;*/
	struct m0_layout_linear_attr *lin_attr1;
	struct m0_layout_linear_attr *lin_attr2;
	//uint32_t                      i;

	M0_UT_ASSERT(cur1 != NULL);
	M0_UT_ASSERT(cur2 != NULL);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	/* Compare generic part of the layout buffers. */
	lbuf_compare(cur1, cur2);

	/* Compare PDCLUST layout type specific part of the layout buffers. */
	pdclust_lbuf_compare(cur1, cur2);

#if 0
	/* Compare enumeration type specific part of the layout buffers. */
	if (enum_id == LIST_ENUM_ID) {
		M0_UT_ASSERT(m0_bufvec_cursor_step(cur1) >= sizeof *ce_header1);
		M0_UT_ASSERT(m0_bufvec_cursor_step(cur2) >= sizeof *ce_header2);

		ce_header1 = m0_bufvec_cursor_addr(cur1);
		ce_header2 = m0_bufvec_cursor_addr(cur2);

		m0_bufvec_cursor_move(cur1, sizeof *ce_header1);
		m0_bufvec_cursor_move(cur2, sizeof *ce_header2);

		M0_UT_ASSERT(ce_header1->ces_nr == ce_header2->ces_nr);

		M0_UT_ASSERT(m0_bufvec_cursor_step(cur1) >=
			     ce_header1->ces_nr * sizeof *cob_id1);
		M0_UT_ASSERT(m0_bufvec_cursor_step(cur2) >=
			     ce_header2->ces_nr * sizeof *cob_id2);

		for (i = 0; i < ce_header1->ces_nr; ++i) {
			cob_id1 = m0_bufvec_cursor_addr(cur1);
			cob_id2 = m0_bufvec_cursor_addr(cur2);

			M0_UT_ASSERT(m0_fid_eq(cob_id1, cob_id2));

			m0_bufvec_cursor_move(cur1, sizeof *cob_id1);
			m0_bufvec_cursor_move(cur2, sizeof *cob_id2);
		}
	} else {
#endif
		M0_UT_ASSERT(m0_bufvec_cursor_step(cur1) >= sizeof *lin_attr1);
		M0_UT_ASSERT(m0_bufvec_cursor_step(cur2) >= sizeof *lin_attr2);

		lin_attr1 = m0_bufvec_cursor_addr(cur1);
		lin_attr2 = m0_bufvec_cursor_addr(cur2);

		M0_UT_ASSERT(lin_attr1->lla_nr == lin_attr2->lla_nr);
		M0_UT_ASSERT(lin_attr1->lla_A == lin_attr2->lla_A);
		M0_UT_ASSERT(lin_attr1->lla_B == lin_attr2->lla_B);
#if 0
	}
#endif
}

/*
 * Tests the API sequence m0_layout_decode() followed by m0_layout_encode(),
 * for the PDCLUST layout type.
 */
static int test_decode_encode_pdclust(uint32_t enum_id, uint64_t lid,
				      uint32_t inline_test)
{
	void                    *area1;
	struct m0_bufvec         bv1;
	struct m0_bufvec_cursor  cur1;
	void                    *area2;
	struct m0_bufvec         bv2;
	struct m0_bufvec_cursor  cur2;
	m0_bcount_t              num_bytes;
	uint32_t                 N;
	uint32_t                 K;
	uint32_t                 P;
	struct m0_uint128        seed;
	struct m0_layout        *l;
	struct m0_layout_type   *lt;

	M0_ENTRY();
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "decodeencodepdcl");

	/* Build a layout buffer. */
	if (enum_id == LIST_ENUM_ID)
		allocate_area(&area1, ADDITIONAL_BYTES_DEFAULT, &num_bytes);
	else
		allocate_area(&area1, ADDITIONAL_BYTES_NONE, &num_bytes);

	bv1 = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&area1, &num_bytes);
	m0_bufvec_cursor_init(&cur1, &bv1);

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 3, 103, 1510,
				 &N, &K, &P);

	rc = pdclust_layout_buf_build(LINEAR_ENUM_ID, lid,
				      N, K, P, &seed,
				      777, 888, &cur1);
	M0_UT_ASSERT(rc == 0);

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&cur1, &bv1);

	lt = &m0_pdclust_layout_type;
	rc = lt->lt_ops->lto_allocate(&domain, lid, &l);
	M0_ASSERT(m0_layout__allocated_invariant(l));

	/* Decode the layout buffer into a layout object. */
	rc = m0_layout_decode(l, &cur1, M0_LXO_BUFFER_OP, NULL);
	M0_UT_ASSERT(rc == 0);

	/* Unlock the layout, locked by lto_allocate() */
	m0_mutex_unlock(&l->l_lock);

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&cur1, &bv1);

	/*
	 * Encode the layout object produced by m0_layout_decode() into
	 * another layout buffer.
	 */
	if (enum_id == LIST_ENUM_ID)
		allocate_area(&area2, ADDITIONAL_BYTES_DEFAULT, &num_bytes);
	else
		allocate_area(&area2, ADDITIONAL_BYTES_NONE, &num_bytes);

	bv2 = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&area2, &num_bytes);
	m0_bufvec_cursor_init(&cur2, &bv2);

	m0_mutex_lock(&l->l_lock);
	rc = m0_layout_encode(l, M0_LXO_BUFFER_OP, NULL, &cur2);
	m0_mutex_unlock(&l->l_lock);
	M0_UT_ASSERT(rc == 0);

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&cur2, &bv2);

	/*
	 * Compare the two layout buffers - one created earlier here and
	 * the one that is produced by m0_layout_encode().
	 */
	pdclust_layout_buf_compare(enum_id, &cur1, &cur2);

	/* Destroy the layout. */
	m0_layout_put(l);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	m0_free(area1);
	m0_free(area2);
	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the API sequence m0_layout_decode() followed by m0_layout_encode(). */
static void test_decode_encode(void)
{
	uint64_t lid;
	int      rc;

#if 0
	/*
	 * Build a layout buffer representing a layout with PDCLUST layout type
	 * and LIST enum type, with a few inline entries only.
	 * Decode it into a layout object. Then encode that layout object again
	 * into another layout buffer.
	 * Then, compare the original layout buffer with the encoded layout
	 * buffer.
	 */
	lid = 7001;
	rc = test_decode_encode_pdclust(LIST_ENUM_ID, lid, LESS_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout buffer representing a layout with PDCLUST layout type
	 * and LIST enum type, with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES.
	 * Decode it into a layout object. Then encode that layout object again
	 * into another layout buffer.
	 * Then, compare the original layout buffer with the encoded layout
	 * buffer.
	 */
	lid = 7002;
	rc = test_decode_encode_pdclust(LIST_ENUM_ID, lid, EXACT_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout buffer representing a layout with PDCLUST layout type
	 * and LIST enum type including noninline entries.
	 * Decode it into a layout object. Then encode that layout object again
	 * into another layout buffer.
	 * Then, compare the original layout buffer with the encoded layout
	 * buffer.
	 */
	lid = 7003;
	rc = test_decode_encode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Build a layout buffer representing a layout with PDCLUST layout type
	 * and LINEAR enum type.
	 * Decode it into a layout object. Then encode that layout object again
	 * into another layout buffer.
	 * Then, compare the original layout buffer with the encoded layout
	 * buffer.
	 */
	lid = 7004;
	rc = test_decode_encode_pdclust(LINEAR_ENUM_ID, lid,
					INLINE_NOT_APPLICABLE);
	M0_UT_ASSERT(rc == 0);
}

/*
 * Compares two layout objects with PDCLUST layout type, provided as input
 * arguments.
 */
static void pdclust_layout_compare(uint32_t enum_id,
				   const struct m0_layout *l1,
				   const struct m0_layout *l2,
				   bool l2_ref_elevated)
{
	struct m0_pdclust_layout     *pl1;
	struct m0_pdclust_layout     *pl2;
/*	struct m0_layout_list_enum   *list_e1;
	struct m0_layout_list_enum   *list_e2;*/
	struct m0_layout_linear_enum *lin_e1;
	struct m0_layout_linear_enum *lin_e2;
//	uint32_t                      i;

	M0_UT_ASSERT(l1 != NULL && l2 != NULL);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	/* Compare generic part of the layout objects. */
	M0_UT_ASSERT(l1->l_id == l2->l_id);
	M0_UT_ASSERT(l1->l_type == l2->l_type);
	M0_UT_ASSERT(l1->l_dom == l2->l_dom);
	if (l2_ref_elevated)
		M0_UT_ASSERT(m0_ref_read(&l1->l_ref) ==
			     m0_ref_read(&l2->l_ref) - 1);
	else
		M0_UT_ASSERT(m0_ref_read(&l1->l_ref) ==
			     m0_ref_read(&l2->l_ref));
	M0_UT_ASSERT(l1->l_user_count == l2->l_user_count);
	M0_UT_ASSERT(l1->l_ops == l2->l_ops);

	/* Compare PDCLUST layout type specific part of the layout objects. */
	pl1 = container_of(l1, struct m0_pdclust_layout, pl_base.sl_base);
	pl2 = container_of(l2, struct m0_pdclust_layout, pl_base.sl_base);

	M0_UT_ASSERT(pl1->pl_attr.pa_N == pl2->pl_attr.pa_N);
	M0_UT_ASSERT(pl1->pl_attr.pa_K == pl2->pl_attr.pa_K);
	M0_UT_ASSERT(pl1->pl_attr.pa_P == pl2->pl_attr.pa_P);
	M0_UT_ASSERT(m0_uint128_eq(&pl1->pl_attr.pa_seed,
				   &pl2->pl_attr.pa_seed));

	/* Compare enumeration specific part of the layout objects. */
	M0_UT_ASSERT(pl1->pl_base.sl_enum->le_type ==
		     pl2->pl_base.sl_enum->le_type);
	M0_UT_ASSERT(pl1->pl_base.sl_enum->le_sl == &pl1->pl_base);
	M0_UT_ASSERT(pl1->pl_base.sl_enum->le_sl->sl_base.l_id ==
		     pl2->pl_base.sl_enum->le_sl->sl_base.l_id);
	M0_UT_ASSERT(pl1->pl_base.sl_enum->le_ops ==
		     pl2->pl_base.sl_enum->le_ops);

#if 0
	/* Compare enumeration type specific part of the layout objects. */
	if (enum_id == LIST_ENUM_ID) {
		list_e1 = container_of(pl1->pl_base.sl_enum,
				       struct m0_layout_list_enum, lle_base);
		list_e2 = container_of(pl2->pl_base.sl_enum,
				       struct m0_layout_list_enum, lle_base);

		M0_UT_ASSERT(list_e1->lle_nr == list_e2->lle_nr);

		for (i = 0; i < list_e1->lle_nr; ++i)
			M0_UT_ASSERT(m0_fid_eq(&list_e1->lle_list_of_cobs[i],
					       &list_e2->lle_list_of_cobs[i]));
	} else { /* LINEAR_ENUM_ID */
#endif
		lin_e1 = container_of(pl1->pl_base.sl_enum,
				      struct m0_layout_linear_enum, lle_base);
		lin_e2 = container_of(pl2->pl_base.sl_enum,
				      struct m0_layout_linear_enum, lle_base);

		M0_UT_ASSERT(lin_e1->lle_attr.lla_nr ==
			     lin_e2->lle_attr.lla_nr);
		M0_UT_ASSERT(lin_e1->lle_attr.lla_A == lin_e2->lle_attr.lla_A);
		M0_UT_ASSERT(lin_e1->lle_attr.lla_B == lin_e2->lle_attr.lla_B);
#if 0
	}
#endif
}

/* Copies contents of one layout object to the other. */
static void pdclust_layout_copy(uint32_t enum_id,
				const struct m0_layout *l_src,
				struct m0_layout **l_dest)
{
	struct m0_pdclust_layout     *pl_src;
	struct m0_pdclust_layout     *pl_dest;
//	struct m0_layout_list_enum   *list_src;
//	struct m0_layout_list_enum   *list_dest;
	struct m0_layout_linear_enum *lin_src;
	struct m0_layout_linear_enum *lin_dest;
//	uint32_t                      i;

	M0_UT_ASSERT(l_src != NULL && l_dest != NULL);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	pl_src = container_of(l_src, struct m0_pdclust_layout, pl_base.sl_base);
	pl_dest = m0_alloc(sizeof *pl_src);
	M0_UT_ASSERT(pl_dest != NULL);
	*l_dest = &pl_dest->pl_base.sl_base;

	/* Copy generic part of the layout object. */
	(*l_dest)->l_id         = l_src->l_id;
	(*l_dest)->l_type       = l_src->l_type;
	(*l_dest)->l_dom        = l_src->l_dom;
	(*l_dest)->l_user_count = l_src->l_user_count;
	(*l_dest)->l_ops        = l_src->l_ops;
	m0_ref_init(&(*l_dest)->l_ref, 1, l_src->l_ops->lo_fini);

	/* Copy PDCLUST layout type specific part of the layout objects. */
	pl_dest->pl_attr = pl_src->pl_attr;

#if 0
	/* Copy enumeration type specific part of the layout objects. */
	if (enum_id == LIST_ENUM_ID) {
		list_src = container_of(pl_src->pl_base.sl_enum,
					struct m0_layout_list_enum, lle_base);
		list_dest = m0_alloc(sizeof *list_src);
		M0_UT_ASSERT(list_src != NULL);

		list_dest->lle_nr = list_src->lle_nr;
		M0_ALLOC_ARR(list_dest->lle_list_of_cobs, list_dest->lle_nr);

		for (i = 0; i < list_src->lle_nr; ++i)
			list_dest->lle_list_of_cobs[i] =
					       list_src->lle_list_of_cobs[i];

		pl_dest->pl_base.sl_enum = &list_dest->lle_base;
	} else { /* LINEAR_ENUM_ID */
#endif
		lin_src = container_of(pl_src->pl_base.sl_enum,
				       struct m0_layout_linear_enum, lle_base);
		lin_dest = m0_alloc(sizeof *lin_src);
		M0_UT_ASSERT(lin_src != NULL);

		lin_dest->lle_attr = lin_src->lle_attr;
		pl_dest->pl_base.sl_enum = &lin_dest->lle_base;
#if 0
	}
#endif

	/* Copy enumeration specific part of the layout objects. */
	pl_dest->pl_base.sl_enum->le_type = pl_src->pl_base.sl_enum->le_type;
	pl_dest->pl_base.sl_enum->le_ops = pl_src->pl_base.sl_enum->le_ops;
	pl_dest->pl_base.sl_enum->le_sl = &pl_dest->pl_base;

	pdclust_layout_compare(enum_id, &pl_src->pl_base.sl_base,
			       &pl_dest->pl_base.sl_base, false);
}

static void pdclust_layout_copy_delete(uint32_t enum_id, struct m0_layout *l)
{
	struct m0_pdclust_layout     *pl;
//	struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;

	M0_UT_ASSERT(l != NULL);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	pl = container_of(l, struct m0_pdclust_layout, pl_base.sl_base);
#if 0
	if (enum_id == LIST_ENUM_ID) {
		list_enum = container_of(pl->pl_base.sl_enum,
					struct m0_layout_list_enum, lle_base);
		m0_free(list_enum->lle_list_of_cobs);
		m0_free(list_enum);
	} else { /* LINEAR_ENUM_ID */
#endif
		lin_enum = container_of(pl->pl_base.sl_enum,
				        struct m0_layout_linear_enum, lle_base);
		m0_free(lin_enum);
#if 0
	}
#endif
	m0_free(pl);
}

/*
 * Tests the API sequence m0_layout_encode() followed by m0_layout_decode(),
 * for the PDCLUST layout type.
 */
static int test_encode_decode_pdclust(uint32_t enum_id, uint64_t lid,
				      uint32_t inline_test)
{
	struct m0_pdclust_layout     *pl;
	void                         *area;
	m0_bcount_t                   num_bytes;
	struct m0_bufvec              bv;
	struct m0_bufvec_cursor       cur;
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	//struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	struct m0_layout             *l;
	struct m0_layout             *l_copy;
	struct m0_layout_type        *lt;

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "encodedecodepdcl");

	/* Build a layout object. */
	if (enum_id == LIST_ENUM_ID)
		allocate_area(&area, ADDITIONAL_BYTES_DEFAULT, &num_bytes);
	else
		allocate_area(&area, ADDITIONAL_BYTES_NONE, &num_bytes);

	bv = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&area, &num_bytes);
	m0_bufvec_cursor_init(&cur, &bv);

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 13, 113, 1130,
				 &N, &K, &P);

	rc = pdclust_layout_build(enum_id, lid,
				  N, K, P, &seed,
				  10, 20,
				  &pl, /*&list_enum, */&lin_enum,
				  !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	pdclust_layout_copy(enum_id, &pl->pl_base.sl_base, &l_copy);
	M0_UT_ASSERT(l_copy != NULL);

	/* Encode the layout object into a layout buffer. */
	m0_mutex_lock(&pl->pl_base.sl_base.l_lock);
	rc = m0_layout_encode(&pl->pl_base.sl_base, M0_LXO_BUFFER_OP,
			      NULL, &cur);
	m0_mutex_unlock(&pl->pl_base.sl_base.l_lock);
	M0_UT_ASSERT(rc == 0);

	/* Destroy the layout. */
	m0_layout_put(&pl->pl_base.sl_base);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&cur, &bv);

	lt = &m0_pdclust_layout_type;
	rc = lt->lt_ops->lto_allocate(&domain, lid, &l);
	M0_ASSERT(m0_layout__allocated_invariant(l));

	/*
	 * Decode the layout buffer produced by m0_layout_encode() into another
	 * layout object.
	 */
	rc = m0_layout_decode(l, &cur, M0_LXO_BUFFER_OP, NULL);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Comapre the two layout objects - one created earlier here and the
	 * one that is produced by m0_layout_decode().
	 */
	pdclust_layout_compare(enum_id, l_copy, l, false);
	pdclust_layout_copy_delete(enum_id, l_copy);

	/* Unlock the layout, locked by lto_allocate() */
	m0_mutex_unlock(&l->l_lock);

	/* Destroy the layout. */
	m0_layout_put(l);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	m0_free(area);
	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the API sequence m0_layout_encode() followed by m0_layout_decode(). */
static void test_encode_decode(void)
{
	uint64_t lid;
	int      rc;

#if 0
	/*
	 * Build a layout object with PDCLUST layout type and LIST enum type,
	 * with a few inline entries only.
	 * Encode it into a layout buffer. Then decode that layout buffer again
	 * into another layout object.
	 * Then, compare the original layout object with the decoded layout
	 * object.
	 */
	lid = 8001;
	rc = test_encode_decode_pdclust(LIST_ENUM_ID, lid, LESS_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout object with PDCLUST layout type and LIST enum type,
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES.
	 * Encode it into a layout buffer. Then decode that layout buffer again
	 * into another layout object.
	 * Then, compare the original layout object with the decoded layout
	 * object.
	 */
	lid = 8002;
	rc = test_encode_decode_pdclust(LIST_ENUM_ID, lid, EXACT_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout object with PDCLUST layout type and LIST enum type
	 * including noninline entries.
	 * Encode it into a layout buffer. Then decode that layout buffer again
	 * into another layout object.
	 * Then, compare the original layout object with the decoded layout
	 * object.
	 */
	lid = 8003;
	rc = test_encode_decode_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Build a layout object with PDCLUST layout type and LINEAR enum type.
	 * Encode it into a layout buffer. Then decode that layout buffer again
	 * into a another layout object.
	 * Now, compare the original layout object with the decoded layout
	 * object.
	 */
	lid = 8004;
	rc = test_encode_decode_pdclust(LINEAR_ENUM_ID, lid,
					INLINE_NOT_APPLICABLE);
	M0_UT_ASSERT(rc == 0);
}

/*
 * Tests the API m0_layout_get() and m0_layout_put(), for the PDCLUST layout
 * type.
 */
static int test_ref_get_put_pdclust(uint32_t enum_id, uint64_t lid)
{
	struct m0_pdclust_layout     *pl;
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	//struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	uint32_t                      i;

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	M0_UT_ASSERT(/*enum_id == LIST_ENUM_ID || */enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "refgetputpdclust");

	/* Build a layout object. */
	NKP_assign_and_pool_init(enum_id, MORE_THAN_INLINE,
				 10, 1212, 1212,
				 &N, &K, &P);

	rc = pdclust_layout_build(LIST_ENUM_ID, lid,
				  N, K, P, &seed,
				  10, 20,
				  &pl, /*&list_enum, */&lin_enum,
				  !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/* Verify that the ref count is set to 1. */
	M0_UT_ASSERT(m0_ref_read(&pl->pl_base.sl_base.l_ref) == 1);

	/* Add multiple references on the layout object. */
	for (i = 0; i < 123; ++i)
		m0_layout_get(&pl->pl_base.sl_base);
	M0_UT_ASSERT(m0_ref_read(&pl->pl_base.sl_base.l_ref) == 1 + 123);

	/* Release multiple references on the layout object. */
	for (i = 0; i < 123; ++i)
		m0_layout_put(&pl->pl_base.sl_base);
	M0_UT_ASSERT(m0_ref_read(&pl->pl_base.sl_base.l_ref) == 1);

	/* Release the last reference so as to delete the layout. */
	m0_layout_put(&pl->pl_base.sl_base);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	M0_LEAVE();
	return rc;
}

/* Tests the APIs m0_layout_get() and m0_layout_put(). */
static void test_ref_get_put(void)
{
	uint64_t lid;
	int      rc;

#if 0
	/*
	 * Reference get and put operations for PDCLUST layout type and LIST
	 * enumeration type.
	 */
	lid = 9001;
	rc = test_ref_get_put_pdclust(LIST_ENUM_ID, lid);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Reference get and put operations for PDCLUST layout type and LINEAR
	 * enumeration type.
	 */
	lid = 9002;
	rc = test_ref_get_put_pdclust(LINEAR_ENUM_ID, lid);
	M0_UT_ASSERT(rc == 0);
}

/* Verifies the enum operations pointed by leo_nr and leo_get. */
static void enum_op_verify(uint32_t enum_id, uint64_t lid,
			   uint32_t nr, struct m0_layout *l)
{
	struct m0_striped_layout     *stl;
	struct m0_layout_enum        *e;
	struct m0_layout_linear_enum *lin_enum;
	struct m0_fid                 fid_calculated;
	struct m0_fid                 fid_from_layout;
	struct m0_fid                 gfid;
	int                           i;

	M0_UT_ASSERT(l != NULL);

	stl = m0_layout_to_striped(l);
	e = m0_striped_layout_to_enum(stl);
	M0_UT_ASSERT(m0_layout_enum_nr(e) == nr);

	if (enum_id == LIST_ENUM_ID) {
		for(i = 0; i < nr; ++i) {
			m0_fid_set(&fid_calculated, i * 100 + 1, i + 1);
			m0_layout_enum_get(e, i, NULL, &fid_from_layout);
			M0_UT_ASSERT(m0_fid_eq(&fid_calculated,
					       &fid_from_layout));
		}
	} else {
		/* Set gfid to some dummy value. */
		m0_fid_gob_make(&gfid, 0, 999);
		lin_enum = container_of(e, struct m0_layout_linear_enum,
					lle_base);
		M0_UT_ASSERT(lin_enum != NULL);
		for(i = 0; i < nr; ++i) {
			m0_fid_convert_gob2cob(&gfid, &fid_calculated,
					       lin_enum->lle_attr.lla_A +
					       i * lin_enum->lle_attr.lla_B);
			m0_layout_enum_get(e, i, &gfid, &fid_from_layout);
			M0_UT_ASSERT(m0_fid_eq(&fid_calculated,
					       &fid_from_layout));
		}
	}
}

/*
 * Tests the enum operations pointed by leo_nr and leo_get, for the PDCLUST
 * layout type.
 */
static int test_enum_ops_pdclust(uint32_t enum_id, uint64_t lid,
				 uint32_t inline_test)
{
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	struct m0_pdclust_layout     *pl;
	//struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;

	M0_ENTRY();
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	/* Build a layout object. */
	m0_uint128_init(&seed, "enumopspdclustla");

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 14, 1014, 1014,
				 &N, &K, &P);

	rc = pdclust_layout_build(enum_id, lid,
				  N, K, P, &seed,
				  777, 888,
				  &pl, /*&list_enum, */&lin_enum,
				  !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/* Verify enum operations. */
	enum_op_verify(enum_id, lid, P, &pl->pl_base.sl_base);

	/* Destroy the layout object. */
	m0_layout_put(&pl->pl_base.sl_base);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the enum operations pointed by leo_nr and leo_get. */
static void test_enum_operations(void)
{
	uint64_t lid;

#if 0
	/*
	 * Decode a layout with PDCLUST layout type, LIST enum type and
	 * with a few inline entries only.
	 * And then verify its enum ops.
	 */
	lid = 10001;
	rc = test_enum_ops_pdclust(LIST_ENUM_ID, lid, LESS_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Decode a layout with PDCLUST layout type, LIST enum type and
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES.
	 * And then verify its enum ops.
	 */
	lid = 10002;
	rc = test_enum_ops_pdclust(LIST_ENUM_ID, lid, EXACT_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Decode a layout with PDCLUST layout type and LIST enum type
	 * including noninline entries.
	 * And then verify its enum ops.
	 */
	lid = 10003;
	rc = test_enum_ops_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Decode a layout with PDCLUST layout type and LINEAR enum type.
	 * And then verify its enum ops.
	 */
	lid = 10004;
	rc = test_enum_ops_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE);
	M0_UT_ASSERT(rc == 0);
}

/* Tests the API m0_layout_max_recsize(). */
static void test_max_recsize(void)
{
	struct m0_layout_domain t_domain;
	m0_bcount_t             max_size_from_api;
	m0_bcount_t             max_size_calculated;

	M0_ENTRY();

	/*
	 * A layout type can be registered with only one domain at a time.
	 * Hence, unregister all the available layout types and enum types from
	 * the domain "domain", which are registered through test_init().
	 */
	m0_layout_standard_types_unregister(&domain);

	/* Initialise the domain. */
	rc = m0_layout_domain_init(&t_domain);
	M0_UT_ASSERT(rc == 0);

	/* Register pdclust layout type and verify m0_layout_max_recsize(). */
	rc = m0_layout_type_register(&t_domain, &m0_pdclust_layout_type);
	M0_UT_ASSERT(rc == 0);

	max_size_from_api = m0_layout_max_recsize(&t_domain);

	max_size_calculated = sizeof(struct m0_layout_rec) +
			      sizeof(struct m0_layout_pdclust_rec);

	M0_UT_ASSERT(max_size_from_api == max_size_calculated);

	/* Register linear enum type and verify m0_layout_max_recsize(). */
	rc = m0_layout_enum_type_register(&t_domain, &m0_linear_enum_type);
	M0_UT_ASSERT(rc == 0);

	max_size_from_api = m0_layout_max_recsize(&t_domain);

	max_size_calculated = sizeof(struct m0_layout_rec) +
			      sizeof(struct m0_layout_pdclust_rec) +
			      sizeof(struct m0_layout_linear_attr);

	M0_UT_ASSERT(max_size_from_api == max_size_calculated);

#if 0
	/* Register list enum type and verify m0_layout_max_recsize(). */
	rc = m0_layout_enum_type_register(&t_domain, &m0_list_enum_type);
	M0_UT_ASSERT(rc == 0);

	max_size_from_api = m0_layout_max_recsize(&t_domain);

	max_size_calculated = sizeof(struct m0_layout_rec) +
			      sizeof(struct m0_layout_pdclust_rec) +
			      sizeof(struct cob_entries_header) +
			      LDB_MAX_INLINE_COB_ENTRIES *
			      sizeof(struct m0_fid);

	M0_UT_ASSERT(max_size_from_api == max_size_calculated);

	/* Unregister list enum type and verify m0_layout_max_recsize(). */
	m0_layout_enum_type_unregister(&t_domain, &m0_list_enum_type);

	max_size_from_api = m0_layout_max_recsize(&t_domain);

	max_size_calculated = sizeof(struct m0_layout_rec) +
			      sizeof(struct m0_layout_pdclust_rec) +
			      sizeof(struct m0_layout_linear_attr);

	M0_UT_ASSERT(max_size_from_api == max_size_calculated);
#endif

	/* Unregister linear enum type and verify m0_layout_max_recsize(). */
	m0_layout_enum_type_unregister(&t_domain, &m0_linear_enum_type);

	max_size_from_api = m0_layout_max_recsize(&t_domain);

	max_size_calculated = sizeof(struct m0_layout_rec) +
			      sizeof(struct m0_layout_pdclust_rec);

	M0_UT_ASSERT(max_size_from_api == max_size_calculated);

	/* Unregister pdclust layout type and verify m0_layout_max_recsize(). */
	m0_layout_type_unregister(&t_domain, &m0_pdclust_layout_type);

	max_size_from_api = m0_layout_max_recsize(&t_domain);

	max_size_calculated = sizeof(struct m0_layout_rec);

	M0_UT_ASSERT(max_size_from_api == max_size_calculated);

	/* Finalise the domain. */
	m0_layout_domain_fini(&t_domain);

	/*
	 * Register back all the available layout types and enum types with
	 * the domain "domain", to undo the change done at the beginning of
	 * this function.
	 */
	rc = m0_layout_standard_types_register(&domain);
	M0_ASSERT(rc == 0);

	M0_LEAVE();
}

/*
 * Calculates the recsize by considering the sizes of the internal data
 * structures and their values, as applicable. Then verifies that the recsize
 * provided as an argument matches the calcualted one.
 */
static void pdclust_recsize_verify(uint32_t enum_id,
				   struct m0_layout *l,
				   m0_bcount_t recsize_to_verify)
{
	//struct m0_pdclust_layout    *pl;
	//struct m0_layout_list_enum  *list_enum;
	m0_bcount_t                  recsize;

	M0_UT_ASSERT(l != NULL);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);
	M0_UT_ASSERT(l->l_type == &m0_pdclust_layout_type);

#if 0
	pl = container_of(l, struct m0_pdclust_layout, pl_base.sl_base);

	/* Account for the enum type specific recsize. */
	if (enum_id == LIST_ENUM_ID) {
		list_enum = container_of(pl->pl_base.sl_enum,
					 struct m0_layout_list_enum, lle_base);
		if (list_enum->lle_nr < LDB_MAX_INLINE_COB_ENTRIES)
			recsize = sizeof(struct cob_entries_header) +
				  list_enum->lle_nr * sizeof(struct m0_fid);
		else
			recsize = sizeof(struct cob_entries_header) +
				  LDB_MAX_INLINE_COB_ENTRIES *
				  sizeof(struct m0_fid);
	} else
#endif
		recsize = sizeof(struct m0_layout_linear_attr);

	/*
	 * Account for the recsize for the generic part of the layout object
	 * and for the PDCLUST layout type specific part of it.
	 */
	recsize = sizeof(struct m0_layout_rec) +
		  sizeof(struct m0_layout_pdclust_rec) + recsize;

	/* Compare the two sizes. */
	M0_UT_ASSERT(recsize == recsize_to_verify);
}

/* Tests the function lo_recsize(), for the PDCLUST layout type. */
static int test_recsize_pdclust(uint32_t enum_id, uint64_t lid,
				uint32_t inline_test)
{
	struct m0_pdclust_layout     *pl;
	struct m0_layout             *l;
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	//struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	m0_bcount_t                   recsize;

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "recsizepdclustla");

	/* Build a layout object. */
	NKP_assign_and_pool_init(enum_id,
				 inline_test, 1, 1200, 1111,
				 &N, &K, &P);

	rc = pdclust_layout_build(enum_id, lid,
				  N, K, P, &seed,
				  10, 20,
				  &pl, /*&list_enum, */&lin_enum,
				  !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/* Obtain the recsize by using the internal function lo_recsize(). */
	l = &pl->pl_base.sl_base;
	recsize = l->l_ops->lo_recsize(l);

	/* Verify the recsize returned by lo_recsize(). */
	pdclust_recsize_verify(enum_id, &pl->pl_base.sl_base, recsize);

	/* Destroy the layout object. */
	m0_layout_put(l);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the function lo_recsize(). */
static void test_recsize(void)
{
	uint64_t lid;
	int      rc;

#if 0
	/*
	 * lo_recsize() for PDCLUST layout type and LIST enumeration type,
	 * with a few inline entries only.
	 */
	lid = 11001;
	rc = test_recsize_pdclust(LIST_ENUM_ID, lid, LESS_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * lo_recsize() for PDCLUST layout type and LIST enumeration type,
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES
	 */
	lid = 11002;
	rc = test_recsize_pdclust(LIST_ENUM_ID, lid, EXACT_INLINE);
	M0_UT_ASSERT(rc == 0);

	/*
	 * lo_recsize() for PDCLUST layout type and LIST enumeration type
	 * including noninline entries.
	 */
	lid = 11003;
	rc = test_recsize_pdclust(LIST_ENUM_ID, lid, MORE_THAN_INLINE);
	M0_UT_ASSERT(rc == 0);
#endif

	/* lo_recsize() for PDCLUST layout type and LINEAR enumeration type. */
	lid = 11004;
	rc = test_recsize_pdclust(LINEAR_ENUM_ID, lid, INLINE_NOT_APPLICABLE);
	M0_UT_ASSERT(rc == 0);
}

static void ldemo(struct m0_pdclust_instance *pi,
		  const struct m0_pdclust_layout *pl)
{
	struct m0_pdclust_src_addr src;
	struct m0_pdclust_tgt_addr tgt;
	struct m0_pdclust_src_addr src1;
	struct m0_pdclust_attr     attr = pl->pl_attr;
	uint32_t                   W;
	uint32_t                   unit;

	W = attr.pa_N + 2 * attr.pa_K;
	src.sa_group = 0;
	for (unit = 0; unit < W; ++unit) {
		src.sa_unit = unit;
		m0_pdclust_instance_map(pi, &src, &tgt);
		m0_pdclust_instance_inv(pi, &tgt, &src1);
		M0_ASSERT(memcmp(&src, &src1, sizeof src) == 0);
	}
}

/* Tests the APIs supported for m0_pdclust_instance object. */
static int test_pdclust_instance_obj(uint32_t enum_id, uint64_t lid,
				     bool inline_test, bool failure_test)
{
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	uint32_t                      i;
	uint32_t                      cache_nr;
	uint64_t                     *cache_len;
	struct m0_layout             *l;
	struct m0_pdclust_layout     *pl;
	struct m0_pool_version        pool_ver;
	//struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	struct m0_pdclust_instance   *pi;
	struct m0_fid                 gfid;
	struct m0_layout_instance    *li;

	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	m0_uint128_init(&seed, "buildpdclustlayo");

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 14, 30, 30,
				 &N, &K, &P);

	rc = pdclust_layout_build(enum_id, lid,
				  N, K, P, &seed,
				  10, 20,
				  &pl, /*&list_enum, */&lin_enum,
				  !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/* Verify some pdclust APIs. */
	M0_UT_ASSERT(m0_pdclust_N(pl) == N);
	M0_UT_ASSERT(m0_pdclust_K(pl) == K);
	M0_UT_ASSERT(m0_pdclust_P(pl) == P);
	M0_UT_ASSERT(m0_pdclust_unit_size(pl) == UNIT_SIZE);

	M0_UT_ASSERT(m0_pdclust_unit_classify(pl, N - 1) == M0_PUT_DATA);
	M0_UT_ASSERT(m0_pdclust_unit_classify(pl, N) == M0_PUT_PARITY);
	M0_UT_ASSERT(m0_pdclust_unit_classify(pl, N + 2 * K ) == M0_PUT_SPARE);

	/* Build pdclust instance. */
	m0_fid_set(&gfid, 0, 999);
	l = m0_pdl_to_layout(pl);
	M0_UT_ASSERT(m0_ref_read(&l->l_ref) == 1);
	cache_nr = 4;
	pool_ver.pv_fd_tree.ft_cache_info.fci_nr = cache_nr;
	M0_ALLOC_ARR(cache_len, cache_nr);
	M0_UT_ASSERT(cache_len != NULL);
	pool_ver.pv_fd_tree.ft_cache_info.fci_info = cache_len;
	for (i = 0; i < cache_nr; ++i) {
		pool_ver.pv_fd_tree.ft_cache_info.fci_info[i] = i + 1;
	}
	l->l_pver = &pool_ver;
	rc = m0_layout_instance_build(l, &gfid, &li);
	if (failure_test) {
		M0_UT_ASSERT(rc == -ENOMEM || rc == -EPROTO);
	}
	else {
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(m0_ref_read(&l->l_ref) == 2);
		pi = m0_layout_instance_to_pdi(li);
		ldemo(pi, pl);

		/* Verify m0_layout_instance_to_pdi(). */
		li = &pi->pi_base;
		M0_UT_ASSERT(m0_layout_instance_to_pdi(li) == pi);

#if 0
		/* Verify m0_layout_instance_to_enum */
		if (enum_id == LIST_ENUM_ID)
			M0_UT_ASSERT(m0_layout_instance_to_enum(li) ==
				     &list_enum->lle_base);
		else
#endif
			M0_UT_ASSERT(m0_layout_instance_to_enum(li) ==
				     &lin_enum->lle_base);

		/* Delete the pdclust instance object. */
		m0_layout_instance_fini(&pi->pi_base);
		M0_UT_ASSERT(m0_ref_read(&l->l_ref) == 1);
	}

	/* Delete the layout object. */
	m0_layout_put(m0_pdl_to_layout(pl));
	M0_UT_ASSERT(list_lookup(lid) == NULL);
	m0_pool_fini(&pool);
	m0_free(cache_len);
	return rc;
}

/*
 * Tests the APIs supported for m0_pdclust_instance object, for various enum
 * types.
 */
static void test_pdclust_instance(void)
{
	uint64_t lid;

#if 0
	/*
	 * Build a layout object with PDCLUST layout type, LIST enum type
	 * with a few inline entries only and then destroy it.
	 */
	lid = 12001;
	rc = test_pdclust_instance_obj(LIST_ENUM_ID, lid, LESS_THAN_INLINE,
				       !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout object with PDCLUST layout type, LIST enum type
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES and then destroy it.
	 */
	lid = 12002;
	rc = test_pdclust_instance_obj(LIST_ENUM_ID, lid, EXACT_INLINE,
				       !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Build a layout object with PDCLUST layout type, LIST enum type
	 * including noninline entries and then destroy it.
	 */
	lid = 12003;
	rc = test_pdclust_instance_obj(LIST_ENUM_ID, lid, MORE_THAN_INLINE,
				       !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Build a layout object with PDCLUST layout type and LINEAR enum
	 * type and then destroy it.
	 */
	lid = 12004;
	rc = test_pdclust_instance_obj(LINEAR_ENUM_ID, lid,
				       INLINE_NOT_APPLICABLE,
				       !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
}

static void test_pdclust_instance_failure(void)
{
	uint64_t lid;

#if 0
	/* Simulate memory allocation error in m0_pdclust_instance_build(). */
	lid = 13001;
	m0_fi_enable_once("pdclust_instance_build", "mem_err1");
	rc = test_pdclust_instance_obj(LIST_ENUM_ID, lid, LESS_THAN_INLINE,
				       FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);
#endif

	/* Simulate memory allocation error in m0_pdclust_instance_build(). */
	lid = 13002;
	m0_fi_enable_once("pdclust_instance_build", "mem_err2");
	rc = test_pdclust_instance_obj(LINEAR_ENUM_ID, lid,
				      INLINE_NOT_APPLICABLE,
				       FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);

	/*
	 * Simulate m0_parity_math_init() error in
	 * m0_pdclust_instance_build().
	 */
	lid = 13003;
	m0_fi_enable_once("pdclust_instance_build", "parity_math_err");
	rc = test_pdclust_instance_obj(LINEAR_ENUM_ID, lid,
				      INLINE_NOT_APPLICABLE,
				       FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
}

#if 0
#ifndef __KERNEL__
/*
 * Sets (or resets) the pair using the area pointer and the layout id provided
 * as arguments.
 */
static void pair_set(struct m0_db_pair *pair, uint64_t *lid,
		       void *area, m0_bcount_t num_bytes)
{
	pair->dp_key.db_buf.b_addr = lid;
	pair->dp_key.db_buf.b_nob  = sizeof *lid;
	pair->dp_rec.db_buf.b_addr = area;
	pair->dp_rec.db_buf.b_nob  = num_bytes;
}

static int test_add_pdclust(uint32_t enum_id, uint64_t lid,
			    uint32_t inline_test,
			    bool layout_destroy, struct m0_layout **l_obj,
			    bool duplicate_test,
			    bool failure_test);

/* Tests the API m0_layout_lookup(), for the PDCLUST layout type. */
static int test_lookup_pdclust(uint32_t enum_id, uint64_t lid,
			       bool existing_test,
			       uint32_t inline_test,
			       bool failure_test)
{
	m0_bcount_t        num_bytes;
	void              *area;
	struct m0_layout  *l1;
	struct m0_layout  *l1_copy;
	struct m0_layout  *l2;
	struct m0_layout  *l3;
	struct m0_db_pair  pair;
	struct m0_db_tx    tx;
	int                rc_tmp;

	M0_ENTRY();
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);
	M0_UT_ASSERT(ergo(!existing_test, failure_test));

	/*
	 * If existing_test is true, then first add a layout object to the
	 * DB.
	 */
	if (existing_test) {
		rc = test_add_pdclust(enum_id, lid,
				      inline_test,
				      !LAYOUT_DESTROY, &l1,
				      !DUPLICATE_TEST,
				      !FAILURE_TEST);
		M0_UT_ASSERT(rc == 0);
		if (!failure_test)
			pdclust_layout_copy(enum_id, l1, &l1_copy);

		/*
		 * Lookup for the layout object to verify that the same object
		 * is returned from the memory, not requiring a lookup from the
		 * DB.
		 */
		rc = m0_layout_lookup(&domain, lid, &m0_pdclust_layout_type,
				      &tx, &pair, &l2);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(l2 == l1);

		/* Release the reference acquired by m0_layout_lookup(). */
		m0_layout_put(l1);

		/* Destroy the layout object. */
		m0_layout_put(l1);
	}

	M0_UT_ASSERT(list_lookup(lid) == NULL);
	M0_UT_ASSERT(m0_layout_find(&domain, lid) == NULL);

	/* Lookup for the layout object from the DB. */
	allocate_area(&area, ADDITIONAL_BYTES_NONE, &num_bytes);

	rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
	M0_UT_ASSERT(rc == 0);

	pair_set(&pair, &lid, area, num_bytes);

	rc = m0_layout_lookup(&domain, lid, &m0_pdclust_layout_type,
			      &tx, &pair, &l3);
	if (failure_test)
		M0_UT_ASSERT(rc == -ENOENT || rc == -ENOMEM || rc == -EPROTO ||
			     rc == LO_DECODE_ERR);
	else
		M0_UT_ASSERT(rc == 0);

	rc_tmp = m0_db_tx_commit(&tx);
	M0_UT_ASSERT(rc_tmp == 0);

	if (existing_test && !failure_test) {
		M0_UT_ASSERT(list_lookup(lid) == l3);
		pdclust_layout_compare(enum_id, l1_copy, l3, false);
		pdclust_layout_copy_delete(enum_id, l1_copy);

		/* Destroy the layout object. */
		m0_layout_put(l3);
		M0_UT_ASSERT(list_lookup(lid) == NULL);
	}
	m0_free(area);
	M0_LEAVE();
	return rc;
}

struct ghost_data {
	uint64_t                 lid;
	struct m0_layout        *l;
	struct m0_bufvec_cursor *cur;
};

static bool ghost_create(void *d)
{
	struct ghost_data     *data = d;
	struct m0_layout_type *lt;

	M0_ENTRY();
	M0_UT_ASSERT(list_lookup(data->lid) == NULL);

	lt = &m0_pdclust_layout_type;
	rc = lt->lt_ops->lto_allocate(&domain, data->lid, &data->l);
	M0_UT_ASSERT(rc == 0);

	/* Decode the layout buffer stored in the ghost_data. */
	rc = m0_layout_decode(data->l, data->cur, M0_LXO_BUFFER_OP, NULL);
	M0_UT_ASSERT(rc == 0);

	/* Unlock the layout, locked by lto_allocate() */
	m0_mutex_unlock(&data->l->l_lock);

	M0_LEAVE();
	return rc;
}

static int test_lookup_with_ghost_creation(uint32_t enum_id, uint64_t lid,
					   uint32_t inline_test)
{
	struct m0_layout        *l1;
	struct m0_layout        *l1_copy;
	struct ghost_data        g_data = { 0 };
	void                    *area_for_encode;
	m0_bcount_t              num_bytes_for_encode;
	struct m0_bufvec         bv_for_encode;
	struct m0_bufvec_cursor  cur_for_encode;
	struct m0_layout        *l_from_DB;
	m0_bcount_t              num_bytes_for_lookup;
	void                    *area_for_lookup;
	struct m0_db_pair        pair;
	struct m0_db_tx          tx;
	int                      rc_tmp;

	M0_ENTRY();
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	/* Create one layout object and add it to the LDB. */
	rc = test_add_pdclust(enum_id, lid,
			      inline_test,
			      !LAYOUT_DESTROY, &l1,
			      !DUPLICATE_TEST,
			      !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
	pdclust_layout_copy(enum_id, l1, &l1_copy);

        /*
	 * Encode the layout object and store its encoded representation into
	 * the ghost_data (g_data.cur) so that the ghost can be created at a
	 * later point.
	 */
	if (enum_id == LIST_ENUM_ID)
		allocate_area(&area_for_encode, ADDITIONAL_BYTES_DEFAULT,
			      &num_bytes_for_encode);
	else
		allocate_area(&area_for_encode, ADDITIONAL_BYTES_NONE,
			      &num_bytes_for_encode);
	bv_for_encode = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&area_for_encode,
						&num_bytes_for_encode);
	m0_bufvec_cursor_init(&cur_for_encode, &bv_for_encode);
	m0_mutex_lock(&l1->l_lock);
	rc = m0_layout_encode(l1, M0_LXO_BUFFER_OP, NULL, &cur_for_encode);
	m0_mutex_unlock(&l1->l_lock);
	M0_UT_ASSERT(rc == 0);
	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&cur_for_encode, &bv_for_encode);
	g_data.cur = &cur_for_encode;
	g_data.lid = lid;

	/*
	 * Destroy the layout object, so that the next m0_layout_lookup() does
	 * not return right away with the layout object read from memory and
	 * instead goes to the LDB to read it.
	 */
	m0_layout_put(l1);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	/*
	 * Lookup for the layout object from the LDB, using m0_layout_lookup().
	 * But while this m0_layout_lookup() is in progress, ghost_create()
	 * will create another in-memory layout object using m0_layout_decode()
	 * performed on the serialised representation of the same layout
	 * created above and stored in g_data.cur.
	 */
	if (enum_id == LIST_ENUM_ID)
		allocate_area(&area_for_lookup, ADDITIONAL_BYTES_DEFAULT,
			      &num_bytes_for_lookup);
	else
		allocate_area(&area_for_lookup, ADDITIONAL_BYTES_NONE,
			      &num_bytes_for_lookup);

	rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
	M0_UT_ASSERT(rc == 0);

	pair_set(&pair, &lid, area_for_lookup, num_bytes_for_lookup);
	m0_fi_enable_func("m0_layout_lookup", "ghost_creation",
			  ghost_create, &g_data);
	rc = m0_layout_lookup(&domain, lid, &m0_pdclust_layout_type,
			      &tx, &pair, &l_from_DB);
	M0_UT_ASSERT(rc == 0);
	m0_layout_put(l_from_DB);
	m0_fi_disable("m0_layout_lookup", "ghost_creation");
	rc_tmp = m0_db_tx_commit(&tx);
	M0_UT_ASSERT(rc_tmp == 0);

	M0_UT_ASSERT(l_from_DB == g_data.l);
	M0_UT_ASSERT(list_lookup(lid) == l_from_DB);
	pdclust_layout_compare(enum_id, l1_copy, l_from_DB, false);
	pdclust_layout_copy_delete(enum_id, l1_copy);

	/* Destroy the layout object. */
	m0_layout_put(l_from_DB);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	m0_free(area_for_encode);
	m0_free(area_for_lookup);
	M0_LEAVE();
	return rc;
}

/* Tests the API m0_layout_lookup(). */
static void test_lookup(void)
{
	uint64_t lid;

#if 0
	/*
	 * Lookup for a layout object with LIST enum type, that does not
	 * exist in the DB.
	 */
	lid = 14001;
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 !EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);

	/*
	 * Add a layout object with PDCLUST layout type, LIST enum type and
	 * with a few inline entries only. Then perform lookup for it.
	 */
	lid = 14002;
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 LESS_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Add a layout object with PDCLUST layout type, LIST enum type and
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES. Then perform lookup for it.
	 */
	lid = 14003;
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 EXACT_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Add a layout object with PDCLUST layout type, LIST enum type
	 * including noninline entries.
	 * Then perform lookup for it.
	 */
	lid = 14004;
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Now that a few entries are added into the DB, once again, lookup
	 * for a layout object that does not exist in the DB.
	 */
	lid = 14005;
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 !EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);
#endif

	/*
	 * Add a layout object with PDCLUST layout type and LINEAR enum type.
	 * Then perform lookup for it.
	 */
	lid = 14006;
	rc = test_lookup_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Simulate that another layout object with the same layout id is
	 * created while the first layout object is being allocated by
	 * m0_layout_lookup() with having the domain lock released.
	 */
	lid = 14007;
	rc = test_lookup_with_ghost_creation(LINEAR_ENUM_ID, lid,
					     INLINE_NOT_APPLICABLE);
	M0_UT_ASSERT(rc == 0);
}

/* Tests the API m0_layout_lookup(). */
static void test_lookup_failure(void)
{
	uint64_t           lid;
	struct m0_db_tx    tx;
	struct m0_db_pair  pair;
	struct m0_layout  *l;

	M0_ENTRY();

#if 0
	/*
	 * Lookup for a layout object with LIST enum type, that does not
	 * exist in the DB.
	 */
	lid = 15001;
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 !EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);
#endif

	/*
	 * Lookup for a layout object with LINEAR enum type, that does not
	 * exist in the DB.
	 */
	lid = 15002;
	rc = test_lookup_pdclust(LINEAR_ENUM_ID, lid,
				 !EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);

	/* Simulate pdclust_allocate() failure in m0_layout_lookup(). */
	lid = 15003;
	m0_fi_enable_off_n_on_m("pdclust_allocate", "mem_err", 1, 1);
	rc = test_lookup_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_disable("pdclust_allocate", "mem_err");

	/* Simulate m0_layout_decode() failure in m0_layout_lookup(). */
	lid = 15004;
	m0_fi_enable_once("m0_layout_decode", "lo_decode_err");
	rc = test_lookup_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == LO_DECODE_ERR);

	/* Furnish m0_layout_lookup() with unregistered layout type. */
	struct m0_layout_type test_layout_type = {
		.lt_name     = "test",
		.lt_id       = 1,
		.lt_ops      = NULL
	};
	lid = 15005;
	rc = m0_layout_lookup(&domain, lid, &test_layout_type, &tx, &pair, &l);
	M0_UT_ASSERT(rc == -EPROTO);

#if 0
	/*
	 * Simulate cursor init error in noninline_read() that is in the path
	 * of list_decode() that is in the path of m0_layout_decode().
	 */
	lid = 15006;
	m0_fi_enable_once("noninline_read", "cursor_init_err");
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);

	/*
	 * Simulate cursor get error in noninline_read() that is in the path
	 * of list_decode() that is in the path of m0_layout_decode().
	 */
	lid = 15007;
	m0_fi_enable_once("noninline_read", "cursor_get_err");
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);

	/*
	 * Simulate invalid fid error in noninline_read() that is in the path
	 * of list_decode() that is in the path of m0_layout_decode().
	 */
	lid = 15008;
	m0_fi_enable_once("noninline_read", "invalid_fid_err");
	rc = test_lookup_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
#endif

	M0_LEAVE();
}

/* Tests the API m0_layout_add(), for the PDCLUST layout type. */
static int test_add_pdclust(uint32_t enum_id, uint64_t lid,
			    uint32_t inline_test,
			    bool layout_destroy, struct m0_layout **l_obj,
			    bool duplicate_test,
			    bool failure_test)
{
	m0_bcount_t                   num_bytes;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	void                         *area;
	struct m0_pdclust_layout     *pl;
	struct m0_db_pair             pair;
	struct m0_db_tx               tx;
	struct m0_uint128             seed;
	struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	int                           rc_tmp;

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);
	M0_UT_ASSERT(ergo(layout_destroy, l_obj == NULL));
	M0_UT_ASSERT(ergo(!layout_destroy, l_obj != NULL));
	M0_UT_ASSERT(ergo(duplicate_test, !failure_test));

	m0_uint128_init(&seed, "addpdclustlayout");

	allocate_area(&area, ADDITIONAL_BYTES_NONE, &num_bytes);

	/* Build a layout object. */
	NKP_assign_and_pool_init(enum_id,
				 inline_test, 7, 1900, 1900,
				 &N, &K, &P);

	rc = pdclust_layout_build(enum_id, lid,
				  N, K, P, &seed,
				  100, 200,
				  &pl, &list_enum, &lin_enum,
				  !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/* Add the layout object to the DB. */
	rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
	M0_UT_ASSERT(rc == 0);

	pair_set(&pair, &lid, area, num_bytes);

	rc = m0_layout_add(&pl->pl_base.sl_base, &tx, &pair);
	if (failure_test)
		M0_UT_ASSERT(rc == -ENOENT || rc == LO_ENCODE_ERR);
	else
		M0_UT_ASSERT(rc == 0);

	rc_tmp = m0_db_tx_commit(&tx);
	M0_UT_ASSERT(rc_tmp == 0);

	M0_UT_ASSERT(list_lookup(lid) == &pl->pl_base.sl_base);

	/*
	 * If duplicate_test is true, again try to add the same layout object
	 * to the DB, to verify that it results into EEXIST error.
	 */
	if (duplicate_test) {
		rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
		M0_UT_ASSERT(rc == 0);

		pair_set(&pair, &lid, area, num_bytes);

		rc = m0_layout_add(&pl->pl_base.sl_base, &tx, &pair);
		M0_UT_ASSERT(rc == -EEXIST);

		rc_tmp = m0_db_tx_commit(&tx);
		M0_UT_ASSERT(rc_tmp == 0);
	}

	if (layout_destroy) {
		m0_layout_put(&pl->pl_base.sl_base);
		M0_UT_ASSERT(list_lookup(lid) == NULL);
	}
	else
		*l_obj = &pl->pl_base.sl_base;

	m0_free(area);
	m0_pool_fini(&pool);
	M0_LEAVE("lid %llu", (unsigned long long)lid);
	return rc;
}

/* Tests the API m0_layout_add(). */
static void test_add(void)
{
	uint64_t lid;

#if 0
	/*
	 * Add a layout object with PDCLUST layout type, LIST enum type and
	 * with a few inline entries only.
	 */
	lid = 16001;
	rc = test_add_pdclust(LIST_ENUM_ID, lid,
			      LESS_THAN_INLINE,
			      LAYOUT_DESTROY, NULL,
			      !DUPLICATE_TEST,
			      !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Add a layout object with PDCLUST layout type, LIST enum type and
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES.
	 */
	lid = 16002;
	rc = test_add_pdclust(LIST_ENUM_ID, lid,
			      EXACT_INLINE,
			      LAYOUT_DESTROY, NULL,
			      !DUPLICATE_TEST,
			      !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Add a layout object with PDCLUST layout type and LIST enum type
	 * including noninline entries.
	 */
	lid = 16003;
	rc = test_add_pdclust(LIST_ENUM_ID, lid,
			      MORE_THAN_INLINE,
			      LAYOUT_DESTROY, NULL,
			      !DUPLICATE_TEST,
			      !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
#endif

	/* Add a layout object with PDCLUST layout type and LINEAR enum type. */
	lid = 16004;
	rc = test_add_pdclust(LINEAR_ENUM_ID, lid,
			      INLINE_NOT_APPLICABLE,
			      LAYOUT_DESTROY, NULL,
			      !DUPLICATE_TEST,
			      !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
}

static void test_add_failure(void)
{
	uint64_t lid;

#if 0
	/* Simulate m0_layout_encode() failure in m0_layout_add(). */
	lid = 17001;
	m0_fi_enable_once("m0_layout_encode", "lo_encode_err");
	rc = test_add_pdclust(LIST_ENUM_ID, lid,
			      MORE_THAN_INLINE,
			      LAYOUT_DESTROY, NULL,
			      !DUPLICATE_TEST,
			      FAILURE_TEST);
	M0_UT_ASSERT(rc == LO_ENCODE_ERR);
#endif

	/*
	 * Simulate the error that entry already exists in the layout DB with
	 * the dame layout id.
	 */
	lid = 17002;
	rc = test_add_pdclust(LINEAR_ENUM_ID, lid,
			      INLINE_NOT_APPLICABLE,
			      LAYOUT_DESTROY, NULL,
			      DUPLICATE_TEST,
			      !FAILURE_TEST);
	M0_UT_ASSERT(rc == -EEXIST);

#if 0
	/*
	 * Simulate cursor init failure in noninline_write() that is in the
	 * path of list_encode() which is in the path of m0_layout_encode().
	 */
	lid = 17003;
	m0_fi_enable_once("noninline_write", "cursor_init_err");
	rc = test_add_pdclust(LIST_ENUM_ID, lid,
			      MORE_THAN_INLINE,
			      LAYOUT_DESTROY, NULL,
			      !DUPLICATE_TEST,
			      FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);

	/*
	 * Simulate cursor add failure in noninline_write() that is in the
	 * path of list_encode() which is in the path of m0_layout_encode().
	 */
	lid = 17004;
	m0_fi_enable_once("noninline_write", "cursor_add_err");
	rc = test_add_pdclust(LIST_ENUM_ID, lid,
			      MORE_THAN_INLINE,
			      LAYOUT_DESTROY, NULL,
			      !DUPLICATE_TEST,
			      FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);
#endif
}

/* Tests the API m0_layout_update(), for the PDCLUST layout type. */
static int test_update_pdclust(uint32_t enum_id, uint64_t lid,
			       bool existing_test,
			       uint32_t inline_test,
			       bool failure_test)
{
	m0_bcount_t                   num_bytes;
	void                         *area;
	struct m0_db_pair             pair;
	struct m0_db_tx               tx;
	struct m0_layout             *l1;
	struct m0_layout             *l1_copy;
	struct m0_layout             *l2;
	uint32_t                      i;
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	struct m0_pdclust_layout     *pl;
	struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	int                           rc_tmp;

	M0_ENTRY();
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	allocate_area(&area, ADDITIONAL_BYTES_NONE, &num_bytes);

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 13, 123, 1230,
				 &N, &K, &P);

	if (existing_test) {
		/* Add a layout object to the DB. */
		rc = test_add_pdclust(enum_id, lid,
				      inline_test,
				      !LAYOUT_DESTROY, &l1,
				      !DUPLICATE_TEST,
				      !FAILURE_TEST);
		M0_UT_ASSERT(rc == 0);
	} else {
		/* Build a layout object. */
		m0_uint128_init(&seed, "updatepdclustlay");

		rc = pdclust_layout_build(enum_id, lid,
					  N, K, P, &seed,
					  10, 20,
					  &pl, &list_enum, &lin_enum,
					  !FAILURE_TEST);
		M0_UT_ASSERT(rc == 0);
		l1 = &pl->pl_base.sl_base;
	}

	/* Verify the original user count is as expected. */
	M0_UT_ASSERT(l1->l_user_count == 0);

	/* Update the in-memory layout object - update its user count. */
	for (i = 0; i < 100; ++i)
		m0_layout_user_count_inc(l1);
	M0_UT_ASSERT(l1->l_user_count == 100);

	/* Update the layout object in the DB. */
	rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
	M0_UT_ASSERT(rc == 0);

	pair_set(&pair, &lid, area, num_bytes);

	rc = m0_layout_update(l1, &tx, &pair);
	if (failure_test)
		M0_UT_ASSERT(rc == LO_ENCODE_ERR || rc == L_TABLE_UPDATE_ERR);
	else
		M0_UT_ASSERT(rc == 0);
	/*
	 * Even a non-existing record can be written to the database using
	 * the database update operation.
	 */
	if (existing_test && !failure_test)
		pdclust_layout_copy(enum_id, l1, &l1_copy);

	rc_tmp = m0_db_tx_commit(&tx);
	M0_UT_ASSERT(rc_tmp == 0);

	/*
	 * Update the in-memory layout object - update its user count. This is
	 * to verify the functioning of m0_layout_user_count_dec().
	 */
	for (i = 0; i < 50; ++i)
		m0_layout_user_count_dec(l1);
	M0_UT_ASSERT(l1->l_user_count == 50);

	/* Delete the in-memory layout. */
	m0_layout_put(l1);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	if (existing_test && !failure_test) {
		/*
		 * Lookup for the layout object from the DB to verify that its
		 * user count is indeed updated.
		 */
		rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
		M0_UT_ASSERT(rc == 0);

		pair_set(&pair, &lid, area, num_bytes);

		rc = m0_layout_lookup(&domain, lid, &m0_pdclust_layout_type,
				      &tx, &pair, &l2);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(l2->l_user_count == 100);
		M0_UT_ASSERT(m0_ref_read(&l2->l_ref) == 1);

		rc = m0_db_tx_commit(&tx);
		M0_UT_ASSERT(rc == 0);

		/*
		 * Compare the two layouts - one created earlier here and the
		 * one that is looked up from the DB.
		 */
		pdclust_layout_compare(enum_id, l1_copy, l2, false);
		pdclust_layout_copy_delete(enum_id, l1_copy);

		/* Delete the in-memory layout. */
		m0_layout_put(l2);
		M0_UT_ASSERT(list_lookup(lid) == NULL);
	}

	m0_free(area);
	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the API m0_layout_update(). */
static void test_update(void)
{
	uint64_t lid;

#if 0
	/*
	 * Try to update a layout object with PDCLUST layout type and LIST enum
	 * type, that does not exist in the DB to verify that the operation
	 * fails with the error ENOENT.
	 */
	lid = 18001;
	rc = test_update_pdclust(LIST_ENUM_ID, lid,
				 !EXISTING_TEST,
				 MORE_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Update a layout object with PDCLUST layout type, LIST enum type and
	 * with a few inline entries only.
	 */
	lid = 18002;
	rc = test_update_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 LESS_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Update a layout object with PDCLUST layout type, LIST enum type and
	 * with number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES.
	 */
	lid = 18003;
	rc = test_update_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 EXACT_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Update a layout object with PDCLUST layout type and LIST enum
	 * type including noninline entries.
	 */
	lid = 18004;
	rc = test_update_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Update a layout object with PDCLUST layout type and LINEAR enum
	 * type.
	 */
	lid = 18005;
	rc = test_update_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
}

static void test_update_failure(void)
{
	uint64_t lid;

#if 0
	/* Simulate m0_layout_encode() failure in m0_layout_update(). */
	lid = 19001;
	m0_fi_enable_off_n_on_m("m0_layout_encode", "lo_encode_err", 1, 1);
	rc = test_update_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == LO_ENCODE_ERR);
	m0_fi_disable("m0_layout_encode", "lo_encode_err");
#endif

	/* Simulate m0_table_update() failure in m0_layout_update(). */
	lid = 19002;
	m0_fi_enable_once("m0_layout_update", "table_update_err");
	rc = test_update_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == L_TABLE_UPDATE_ERR);
}

/* Tests the API m0_layout_delete(), for the PDCLUST layout type. */
static int test_delete_pdclust(uint32_t enum_id, uint64_t lid,
			       bool existing_test,
			       uint32_t inline_test,
			       uint32_t failure_test)
{
	m0_bcount_t                   num_bytes;
	void                         *area;
	struct m0_db_pair             pair;
	struct m0_db_tx               tx;
	struct m0_layout             *l;
	struct m0_uint128             seed;
	uint32_t                      N;
	uint32_t                      K;
	uint32_t                      P;
	struct m0_pdclust_layout     *pl;
	struct m0_layout_list_enum   *list_enum;
	struct m0_layout_linear_enum *lin_enum;
	int                           rc_tmp;

	M0_ENTRY();
	M0_UT_ASSERT(enum_id == LIST_ENUM_ID || enum_id == LINEAR_ENUM_ID);

	allocate_area(&area, ADDITIONAL_BYTES_NONE, &num_bytes);

	NKP_assign_and_pool_init(enum_id,
				 inline_test, 12, 122, 1220,
				 &N, &K, &P);
	if (existing_test) {
		/* Add a layout object to the DB. */
		rc = test_add_pdclust(enum_id, lid,
				      inline_test,
				      !LAYOUT_DESTROY, &l,
				      !DUPLICATE_TEST,
				      !FAILURE_TEST);
		M0_UT_ASSERT(rc == 0);
	} else {
		/* Build a layout object. */
		m0_uint128_init(&seed, "deletepdclustlay");

		rc = pdclust_layout_build(enum_id, lid,
					  N, K, P, &seed,
					  10, 20,
					  &pl, &list_enum, &lin_enum,
					  !FAILURE_TEST);
		M0_UT_ASSERT(rc == 0);
		l = &pl->pl_base.sl_base;
	}

	if (M0_FI_ENABLED("nonzero_user_count_err"))
		m0_layout_user_count_inc(l);

	/* Delete the layout object from the DB. */
	pair_set(&pair, &lid, area, num_bytes);

	rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
	M0_UT_ASSERT(rc == 0);

	rc = m0_layout_delete(l, &tx, &pair);
	if (failure_test)
		M0_UT_ASSERT(rc == -ENOENT || rc == -ENOMEM ||
			     rc == -EPROTO || rc == LO_ENCODE_ERR);
	else
		M0_UT_ASSERT(rc == 0);

	rc_tmp = m0_db_tx_commit(&tx);
	M0_UT_ASSERT(rc_tmp == 0);

	/* Destroy the layout object. */
	m0_layout_put(l);
	M0_UT_ASSERT(list_lookup(lid) == NULL);

	if (!failure_test) {
		/*
		 * Lookup for the layout object from the DB, to verify that it
		 * does not exist there and that the lookup results into
		 * ENOENT error.
		 */
		rc = m0_db_tx_init(&tx, &dbenv, DBFLAGS);
		M0_UT_ASSERT(rc == 0);

		pair_set(&pair, &lid, area, num_bytes);

		rc_tmp = m0_layout_lookup(&domain, lid, &m0_pdclust_layout_type,
					  &tx, &pair, &l);
		M0_UT_ASSERT(rc_tmp == -ENOENT);

		rc_tmp = m0_db_tx_commit(&tx);
		M0_UT_ASSERT(rc_tmp == 0);
	}

	m0_free(area);
	m0_pool_fini(&pool);
	M0_LEAVE();
	return rc;
}

/* Tests the API m0_layout_delete(). */
static void test_delete(void)
{
	uint64_t lid;

#if 0
	/*
	 * Delete a layout object with PDCLUST layout type, LIST enum type and
	 * with a few inline entries only.
	 */
	lid = 20001;
	rc = test_delete_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 LESS_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Delete a layout object with PDCLUST layout type, LIST enum type and
	 * with a number of inline entries exactly equal to
	 * LDB_MAX_INLINE_COB_ENTRIES.
	 */
	lid = 20002;
	rc = test_delete_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 EXACT_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Delete a layout object with PDCLUST layout type and LIST enum
	 * type including noninline entries.
	 */
	lid = 20003;
	rc = test_delete_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
#endif

	/*
	 * Delete a layout object with PDCLUST layout type and LINEAR enum
	 * type.
	 */
	lid = 20004;
	rc = test_delete_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 !FAILURE_TEST);
	M0_UT_ASSERT(rc == 0);
}

static void test_delete_failure(void)
{
	uint64_t lid;

	/* Simulate m0_layout_encode() failure in m0_layout_delete(). */
	lid = 21001;
	m0_fi_enable_off_n_on_m("m0_layout_encode", "lo_encode_err", 1, 1);
	rc = test_delete_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == LO_ENCODE_ERR);
	m0_fi_disable("m0_layout_encode", "lo_encode_err");

	/*
	 * Try to delete a layout object with PDCLUST layout type and LINEAR
	 * enum type, that does not exist in the DB, to verify that it results
	 * into the error ENOENT.
	 */
	lid = 21002;
	rc = test_delete_pdclust(LINEAR_ENUM_ID, lid,
				 !EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);

#if 0
	/*
	 * Simulate cursor get failure in noninline_write() that is in the
	 * path of list_encode() which is in the path of m0_layout_encode().
	 */
	lid = 21003;
	m0_fi_enable_once("noninline_write", "cursor_get_err");
	rc = test_delete_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOENT);

	/*
	 * Simulate cursor delete failure in noninline_write() that is in the
	 * path of list_encode() which is in the path of m0_layout_encode().
	 */
	lid = 21004;
	m0_fi_enable_once("noninline_write", "cursor_del_err");
	rc = test_delete_pdclust(LIST_ENUM_ID, lid,
				 EXISTING_TEST,
				 MORE_THAN_INLINE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -ENOMEM);
#endif

	/*
	 * Try to delete a layout with PDCLUST layout type and LINEAR
	 * enum type, that has non-zero user count, to verify that it results
	 * into the error -EPROTO.
	 */
	lid = 21005;
	m0_fi_enable_once("test_delete_pdclust", "nonzero_user_count_err");
	rc = test_delete_pdclust(LINEAR_ENUM_ID, lid,
				 EXISTING_TEST,
				 INLINE_NOT_APPLICABLE,
				 FAILURE_TEST);
	M0_UT_ASSERT(rc == -EPROTO);
}

#endif /* __KERNEL__ */
#endif

struct m0_ut_suite layout_ut = {
	.ts_name  = "layout-ut",
	.ts_owners = "Trupti",
	.ts_init  = test_init,
	.ts_fini  = test_fini,
	.ts_tests = {
		{ "layout-domain-init-fini", test_domain_init_fini },
		{ "layout-domain-init-fini-failure",
				test_domain_init_fini_failure },
		{ "layout-type-register-unregister", test_type_reg_unreg },
		{ "layout-etype-register-unregister", test_etype_reg_unreg },
		{ "layout-register-unregister", test_reg_unreg },
		{ "layout-register-unregister-failure",
					test_reg_unreg_failure },
		{ "layout-build", test_build },
		{ "layout-build-failure", test_build_failure },
		{ "layout-decode", test_decode },
		{ "layout-decode-failure", test_decode_failure },
		{ "layout-encode", test_encode },
		{ "layout-encode-failure", test_encode_failure },
		{ "layout-decode-encode", test_decode_encode },
		{ "layout-encode-decode", test_encode_decode },
		{ "layout-ref-get-put", test_ref_get_put },
		{ "layout-enum-ops", test_enum_operations },
		{ "layout-max-recsize", test_max_recsize },
		{ "layout-recsize", test_recsize },
		{ "layout-pdclust-instance", test_pdclust_instance },
		{ "layout-pdclust-instance-failure",
					test_pdclust_instance_failure },
/*#ifndef __KERNEL__
		{ "layout-lookup", test_lookup },
		{ "layout-lookup-failure", test_lookup_failure },
		{ "layout-add", test_add },
		{ "layout-add-failure", test_add_failure },
		{ "layout-update", test_update },
		{ "layout-update-failure", test_update_failure },
		{ "layout-delete", test_delete },
		{ "layout-delete-failure", test_delete_failure },
#endif*/
		{ NULL, NULL }
	}
};
M0_EXPORTED(layout_ut);

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
