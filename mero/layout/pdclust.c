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
 *                  Trupti Patil <trupti_patil@xyratex.com>
 * Original creation date: 07/15/2010
 */

/**
 * @addtogroup pdclust
 *
 * <b>Implementation overview.</b>
 *
 * Parity de-clustering layout mapping function requires some amount of code
 * dealing with permutations, random sequences generations and conversions
 * between matrices of different shapes.
 *
 * First, as explained in the HLD, an efficient way to generate permutations
 * uniformly scattered across the set of all permutations of a given set is
 * necessary. To this end permute_column() uses a sequence of pseudo-random
 * numbers obtained from a PRNG (m0_rnd()). Few comments are in order:
 *
 * - to seed a PRNG, layout seed and tile number are hashed by a
 *   multiplicative cache (m0_hash());
 *
 * - system PRNG cannot be used, because reproducible sequences are needed.
 *   m0_rnd() is a very simple linear congruential generator straight from
 *   TAOCP. It takes care to return higher, more random, bits of result;
 *
 * - layout behavior is quite sensitive to the PRNG properties. For example,
 *   if m0_rnd() is changed to return lower bits (result % max), resulting
 *   distribution of spare and parity units is not uniform even for large number
 *   of units. Experiments with different PRNG's are indicated.
 *
 * Once permutation's Lehmer code is generated, it has to be applied to the set
 * of columns. permute() function applies a permutation, simultaneously building
 * an inverse permutation.
 *
 * Finally, layout mapping function is defined in terms of conversions between
 * matrices of different shapes. Let's call a matrix having M columns and an
 * arbitrary (probably infinite) number of rows an M-matrix. An element of an
 * M-matrix has (row, column) coordinates. Coordinate pairs can be ordered and
 * enumerated in the "row first" lexicographical order:
 *
 *         (0, 0) < (0, 1) < ... < (0, M - 1) < (1, 0) < ...
 *
 * Function m_enc() returns the number a (row, column) element of an M-matrix
 * has in this ordering. Conversely, function m_dec() returns coordinates of the
 * element having a given number in the ordering. With the help of these two
 * function an M-matrix can be re-arranged into an N-matrix in such a way the
 * element position in the ordering remains invariant.
 *
 * Layout mapping function m0_pdclust_instance_map() performs these
 * re-arrangements in the following places:
 *
 * - to convert a parity group number to a (tile number, group in tile)
 *   pair. This is a conversion of 1-matrix to C-matrix;
 *
 * - to convert a tile from C*(N + 2*K) to L*P form. This is a conversion of
 *   (N + 2*K)-matrix to P-matrix;
 *
 * - to convert a (tile number, frame in tile) pair to a target frame
 *   number. This is a conversion of L-matrix to 1-matrix.
 *
 * Inverse layout mapping function m0_pdclust_instance_inv() performs reverse
 * conversions.
 *
 * @{
 */

#include "lib/errno.h"
#include "lib/memory.h" /* M0_ALLOC_PTR(), M0_ALLOC_ARR(), m0_free() */
#include "lib/misc.h"   /* M0_IN() */
#include "lib/vec.h"    /* m0_bufvec_cursor_step(), m0_bufvec_cursor_addr() */
#include "lib/arith.h"  /* m0_rnd() */
#include "lib/misc.h"   /* m0_forall */
#include "lib/hash.h"   /* m0_hash */
#include "lib/bob.h"
#include "lib/finject.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

#include "mero/magic.h"
#include "layout/layout_internal.h"
#include "layout/pdclust.h"
#include "pool/pool.h"  /* m0_pool_version */
#include "fd/fd_internal.h"
#include "fd/fd.h"      /* m0_fd_perm_cache_init m0_fd_perm_cache_fini */

static const struct m0_bob_type pdclust_bob = {
	.bt_name         = "pdclust",
	.bt_magix_offset = offsetof(struct m0_pdclust_layout, pl_magic),
	.bt_magix        = M0_LAYOUT_PDCLUST_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(static, &pdclust_bob, m0_pdclust_layout);

static const struct m0_bob_type pdclust_instance_bob = {
	.bt_name         = "pd_instance",
	.bt_magix_offset = offsetof(struct m0_pdclust_instance, pi_magic),
	.bt_magix        = M0_LAYOUT_PDCLUST_INSTANCE_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(static, &pdclust_instance_bob, m0_pdclust_instance);

M0_INTERNAL const struct m0_pdclust_src_addr M0_PDCLUST_SRC_NULL = {
	.sa_group = UINT64_MAX,
	.sa_unit  = UINT64_MAX,
};

static bool pdclust_allocated_invariant(const struct m0_pdclust_layout *pl)
{
	return
		pl != NULL &&
		m0_layout__striped_allocated_invariant(&pl->pl_base) &&
		m0_mutex_is_locked(&pl->pl_base.sl_base.l_lock);
}

static bool pdclust_invariant(const struct m0_pdclust_layout *pl)
{
	struct m0_pdclust_attr attr = pl->pl_attr;

	return
		m0_pdclust_layout_bob_check(pl) &&
		m0_layout__striped_invariant(&pl->pl_base) &&
		pl->pl_C * (attr.pa_N + 2 * attr.pa_K) ==
		pl->pl_L * attr.pa_P &&
		pl->pl_base.sl_enum->le_ops->leo_nr(pl->pl_base.sl_enum) ==
		attr.pa_P;
}

static bool pdclust_instance_invariant(const struct m0_pdclust_instance *pi)
{
	uint32_t                  P;
	struct m0_pdclust_layout *pl;
	const struct tile_cache  *tc;

	pl = bob_of(pi->pi_base.li_l, struct m0_pdclust_layout,
		    pl_base.sl_base, &pdclust_bob);
	P  = pl->pl_attr.pa_P;
	tc = &pi->pi_tile_cache;

	return
		m0_pdclust_instance_bob_check(pi) &&
		m0_layout__instance_invariant(&pi->pi_base) &&
		pdclust_invariant(pl) &&
		/*
		 * tc->tc_permute[] and tc->tc_inverse[] are mutually inverse
		 * bijections of {0, ..., P - 1}.
		 */
		m0_forall(i, P,
			  tc->tc_lcode[i] + i < P &&
			  (tc->tc_permute[i] < P && tc->tc_inverse[i] < P) &&
			  tc->tc_permute[tc->tc_inverse[i]] == i &&
			  tc->tc_inverse[tc->tc_permute[i]] == i);
}

/**
 * Implementation of lto_register for PDCLUST layout type.
 * No table is required specifically for PDCLUST layout type.
 */
static int pdclust_register(struct m0_layout_domain *dom,
			    const struct m0_layout_type *lt)
{
	return 0;
}

/** Implementation of lto_unregister for PDCLUST layout type. */
static void pdclust_unregister(struct m0_layout_domain *dom,
			       const struct m0_layout_type *lt)
{
}

/** Implementation of lo_fini for pdclust layout type. */
static void pdclust_fini(struct m0_ref *ref)
{
	struct m0_layout         *l;
	struct m0_pdclust_layout *pl;

	l = container_of(ref, struct m0_layout, l_ref);
	M0_PRE(m0_mutex_is_not_locked(&l->l_lock));

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	pl = m0_layout_to_pdl(l);
	m0_pdclust_layout_bob_fini(pl);
	m0_layout__striped_fini(&pl->pl_base);
	m0_free(pl);
	M0_LEAVE();
}

static const struct m0_layout_ops pdclust_ops;
/** Implementation of lto_allocate() for PDCLUST layout type. */
static int pdclust_allocate(struct m0_layout_domain *dom,
			    uint64_t lid,
			    struct m0_layout **out)
{
	struct m0_pdclust_layout *pl;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(lid > 0);
	M0_PRE(out != NULL);

	M0_ENTRY("lid %llu", (unsigned long long)lid);

	if (M0_FI_ENABLED("mem_err")) { pl = NULL; goto err1_injected; }
	M0_ALLOC_PTR(pl);
err1_injected:
	if (pl == NULL) {
		m0_layout__log("pdclust_allocate", "M0_ALLOC_PTR() failed",
			       lid, -ENOMEM);
		return M0_ERR(-ENOMEM);
	}

	m0_layout__striped_init(&pl->pl_base, dom, lid,
				&m0_pdclust_layout_type, &pdclust_ops);
	m0_pdclust_layout_bob_init(pl);
	m0_mutex_lock(&pl->pl_base.sl_base.l_lock);

	*out = &pl->pl_base.sl_base;
	M0_POST(pdclust_allocated_invariant(pl));
	M0_POST(m0_mutex_is_locked(&(*out)->l_lock));
	M0_LEAVE("lid %llu, pl pointer %p", (unsigned long long)lid, pl);
	return 0;
}

/** Implementation of lo_delete() for PDCLUST layout type. */
static void pdclust_delete(struct m0_layout *l)
{
	struct m0_pdclust_layout *pl;

	pl = bob_of(l, struct m0_pdclust_layout,
		    pl_base.sl_base, &pdclust_bob);
	M0_PRE(pdclust_allocated_invariant(pl));
	M0_PRE(m0_mutex_is_locked(&l->l_lock));

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	m0_mutex_unlock(&l->l_lock);
	m0_pdclust_layout_bob_fini(pl);
	m0_layout__striped_delete(&pl->pl_base);
	m0_free(pl);
	M0_LEAVE();
}

/** Populates pl using the arguments supplied. */
static int pdclust_populate(struct m0_pdclust_layout *pl,
			    const struct m0_pdclust_attr *attr,
			    struct m0_layout_enum *le,
			    uint32_t user_count)
{
	uint64_t lid;
	uint32_t B;
	uint32_t N;
	uint32_t K;
	uint32_t P;

	N = attr->pa_N;
	K = attr->pa_K;
	P = attr->pa_P;
	M0_PRE(pdclust_allocated_invariant(pl));
	M0_PRE(m0_mutex_is_locked(&pl->pl_base.sl_base.l_lock));
	M0_PRE(le != NULL);

	if (N + 2 * K > P) {
		M0_LOG(M0_ERROR, "pl %p, attr %p, Invalid attributes, rc %d",
		       pl, attr, -EPROTO);
		return M0_ERR(-EPROTO);
	}

	lid = pl->pl_base.sl_base.l_id;
	M0_ENTRY("lid %llu", (unsigned long long)lid);

	m0_layout__striped_populate(&pl->pl_base, le, user_count);
	pl->pl_attr = *attr;

	/* Select minimal possible B (least common multiple of P and N+2*K). */
	B = P*(N+2*K)/m0_gcd64(N+2*K, P);
	pl->pl_C = B/(N+2*K);
	pl->pl_L = B/P;

	M0_POST(pdclust_invariant(pl));
	M0_POST(m0_mutex_is_locked(&pl->pl_base.sl_base.l_lock));
	M0_LEAVE("lid %llu", (unsigned long long)lid);
	return 0;
}

M0_INTERNAL int m0_pdclust_build(struct m0_layout_domain *dom,
				 uint64_t lid,
				 const struct m0_pdclust_attr *attr,
				 struct m0_layout_enum *le,
				 struct m0_pdclust_layout **out)
{
	struct m0_layout         *l;
	struct m0_pdclust_layout *pl;
	int                       rc;

	M0_PRE(out != NULL);

	M0_ENTRY("domain %p, lid %llu", dom,(unsigned long long)lid);
	rc = pdclust_allocate(dom, lid, &l);
	if (rc == 0) {
		/* Here pdclust_allocate() has locked l->l_lock. */
		pl = bob_of(l, struct m0_pdclust_layout,
			    pl_base.sl_base, &pdclust_bob);
		M0_ASSERT(pdclust_allocated_invariant(pl));

		rc = pdclust_populate(pl, attr, le, 0);
		if (rc == 0) {
			*out = pl;
			m0_mutex_unlock(&l->l_lock);
		} else
			pdclust_delete(l);
	}

	M0_POST(ergo(rc == 0, pdclust_invariant(*out) &&
			      m0_mutex_is_not_locked(&l->l_lock)));
	M0_LEAVE("domain %p, lid %llu, pl %p, rc %d",
		 dom, (unsigned long long)lid, *out, rc);
	return M0_RC(rc);
}

M0_INTERNAL bool m0_pdclust_attr_check(const struct m0_pdclust_attr *attr)
{
	bool res = attr->pa_P >= attr->pa_N + 2 * attr->pa_K;
	if (!res)
		M0_LOG(M0_ERROR, "Bad pdclust attributes (P < N + 2K):"
		       " P=%"PRIu32" N=%"PRIu32" K=%"PRIu32,
		       attr->pa_P, attr->pa_N, attr->pa_K);
	return res;
}

M0_INTERNAL uint32_t m0_pdclust_N(const struct m0_pdclust_layout *pl)
{
	return pl->pl_attr.pa_N;
}

M0_INTERNAL uint32_t m0_pdclust_K(const struct m0_pdclust_layout *pl)
{
	return pl->pl_attr.pa_K;
}

M0_INTERNAL uint32_t m0_pdclust_P(const struct m0_pdclust_layout *pl)
{
	return pl->pl_attr.pa_P;
}

M0_INTERNAL uint32_t m0_pdclust_size(const struct m0_pdclust_layout *pl)
{
	return m0_pdclust_N(pl) + 2 * m0_pdclust_K(pl);
}

M0_INTERNAL uint64_t m0_pdclust_unit_size(const struct m0_pdclust_layout *pl)
{
	return pl->pl_attr.pa_unit_size;
}

M0_INTERNAL struct m0_pdclust_layout *m0_layout_to_pdl(const struct m0_layout
						       *l)
{
	struct m0_pdclust_layout *pl;

	pl = bob_of(l, struct m0_pdclust_layout,
		    pl_base.sl_base, &pdclust_bob);
	M0_ASSERT(pdclust_invariant(pl));
	return pl;
}

M0_INTERNAL struct m0_layout *m0_pdl_to_layout(struct m0_pdclust_layout *pl)
{
	M0_PRE(pdclust_invariant(pl));
	return &pl->pl_base.sl_base;
}

M0_INTERNAL struct m0_pdclust_instance *m0_layout_instance_to_pdi(const struct
							 m0_layout_instance *li)
{
	struct m0_pdclust_instance *pi;
	pi = bob_of(li, struct m0_pdclust_instance, pi_base,
		    &pdclust_instance_bob);
	M0_POST(pdclust_instance_invariant(pi));
	return pi;
}

static struct m0_pdclust_layout *pi_to_pl(struct m0_pdclust_instance *pi)
{
	return bob_of(pi->pi_base.li_l, struct m0_pdclust_layout,
		      pl_base.sl_base, &pdclust_bob);
}

/** Implementation of lio_to_enum() */
static struct m0_layout_enum *
pdclust_instance_to_enum(const struct m0_layout_instance *li)
{
	struct m0_pdclust_instance *pdi;

	pdi = m0_layout_instance_to_pdi(li);
	return m0_layout_to_enum(pdi->pi_base.li_l);
}

M0_INTERNAL enum m0_pdclust_unit_type
m0_pdclust_unit_classify(const struct m0_pdclust_layout *pl, int unit)
{
	if (unit < pl->pl_attr.pa_N)
		return M0_PUT_DATA;
	else if (unit < pl->pl_attr.pa_N + pl->pl_attr.pa_K)
		return M0_PUT_PARITY;
	else
		return M0_PUT_SPARE;
}

/** Implementation of lto_max_recsize() for pdclust layout type. */
static m0_bcount_t pdclust_max_recsize(struct m0_layout_domain *dom)
{
	M0_PRE(dom != NULL);

	return sizeof(struct m0_layout_pdclust_rec) +
		m0_layout__enum_max_recsize(dom);
}

/** Implementation of lo_decode() for pdclust layout type. */
static int pdclust_decode(struct m0_layout *l,
			  struct m0_bufvec_cursor *cur,
			  enum m0_layout_xcode_op op,
			  struct m0_be_tx *tx,
			  uint32_t user_count)
{
	struct m0_pdclust_layout     *pl;
	struct m0_layout_pdclust_rec *pl_rec;
	struct m0_layout_enum_type   *et;
	struct m0_layout_enum        *e;
	int                           rc;

	M0_PRE(m0_layout__allocated_invariant(l));
	M0_PRE(cur != NULL);
	M0_PRE(m0_bufvec_cursor_step(cur) >= sizeof *pl_rec);
	M0_PRE(M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op == M0_LXO_DB_LOOKUP, tx != NULL));

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	pl = bob_of(l, struct m0_pdclust_layout,
		    pl_base.sl_base, &pdclust_bob);
	M0_ASSERT(pdclust_allocated_invariant(pl));

	/* pl_rec can not be NULL since the buffer size is already verified. */
	pl_rec = m0_bufvec_cursor_addr(cur);
	m0_bufvec_cursor_move(cur, sizeof *pl_rec);

	if (M0_FI_ENABLED("attr_err1"))
		{ pl_rec->pr_let_id = M0_LAYOUT_ENUM_TYPE_MAX - 1; }
	if (M0_FI_ENABLED("attr_err2"))
		{ pl_rec->pr_let_id = M0_LAYOUT_ENUM_TYPE_MAX + 1; }
	et = l->l_dom->ld_enum[pl_rec->pr_let_id];
	if (!IS_IN_ARRAY(pl_rec->pr_let_id, l->l_dom->ld_enum) || et == NULL) {
		rc = -EPROTO;
		M0_LOG(M0_ERROR, "lid %llu, unregistered enum type, rc %d",
		       (unsigned long long)l->l_id, rc);
		goto out;
	}
	rc = et->let_ops->leto_allocate(l->l_dom, &e);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "lid %llu, leto_allocate() failed, rc %d",
		       (unsigned long long)l->l_id, rc);
		goto out;
	}
	rc = e->le_ops->leo_decode(e, cur, op, tx, &pl->pl_base);
	if (rc != 0) {
		/* Finalise the allocated enum object. */
		e->le_ops->leo_delete(e);
		M0_LOG(M0_ERROR, "lid %llu, leo_decode() failed, rc %d",
		       (unsigned long long)l->l_id, rc);
		goto out;
	}

	if (M0_FI_ENABLED("attr_err3")) { pl_rec->pr_attr.pa_P = 1; }
	rc = pdclust_populate(pl, &pl_rec->pr_attr, e, user_count);
	if (rc != 0) {
		/* Finalise the populated enum object. */
		e->le_ops->leo_fini(e);
		M0_LOG(M0_ERROR, "lid %llu, pdclust_populate() failed, rc %d",
		       (unsigned long long)l->l_id, rc);
	}
out:
	M0_POST(ergo(rc == 0, pdclust_invariant(pl)));
	M0_POST(ergo(rc != 0, pdclust_allocated_invariant(pl)));
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)l->l_id, rc);
	return M0_RC(rc);
}

/** Implementation of lo_encode() for pdclust layout type. */
static int pdclust_encode(struct m0_layout *l,
			  enum m0_layout_xcode_op op,
			  struct m0_be_tx *tx,
		          struct m0_bufvec_cursor *out)
{
	struct m0_pdclust_layout     *pl;
	struct m0_layout_pdclust_rec  pl_rec;
	struct m0_layout_enum        *e;
	m0_bcount_t                   nbytes;
	int                           rc;

	/*
	 * m0_layout__invariant() is part of pdclust_invariant(),
	 * to be invoked little later through m0_layout_to_pdl() below.
	 */
	M0_PRE(l != NULL);
	M0_PRE(M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_UPDATE,
		          M0_LXO_DB_DELETE, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op != M0_LXO_BUFFER_OP, tx != NULL));
	M0_PRE(out != NULL);
	M0_PRE(m0_bufvec_cursor_step(out) >= sizeof pl_rec);

	M0_ENTRY("%llu", (unsigned long long)l->l_id);
	pl = m0_layout_to_pdl(l);
	pl_rec.pr_let_id = pl->pl_base.sl_enum->le_type->let_id;
	pl_rec.pr_attr   = pl->pl_attr;

	nbytes = m0_bufvec_cursor_copyto(out, &pl_rec, sizeof pl_rec);
	M0_ASSERT(nbytes == sizeof pl_rec);

	e = pl->pl_base.sl_enum;
	rc = e->le_ops->leo_encode(e, op, tx, out);
	if (rc != 0)
		M0_LOG(M0_ERROR, "lid %llu, leo_encode() failed, rc %d",
		       (unsigned long long)l->l_id, rc);

	M0_LEAVE("lid %llu, rc %d", (unsigned long long)l->l_id, rc);
	return M0_RC(rc);
}

/** Implementation of lo_recsize() for pdclust layout type. */
static m0_bcount_t pdclust_recsize(const struct m0_layout *l)
{
	struct m0_striped_layout *stl;
	struct m0_layout_enum    *e;
	m0_bcount_t               recsize;

	M0_PRE(l!= NULL);
	stl = m0_layout_to_striped(l);
	e = m0_striped_layout_to_enum(stl);
	recsize = sizeof(struct m0_layout_rec) +
		  sizeof(struct m0_layout_pdclust_rec) +
		  e->le_ops->leo_recsize(e);
	M0_POST(recsize <= m0_layout_max_recsize(l->l_dom));
	return recsize;
}

/**
 * "Encoding" function: returns the number that a (row, column) element of a
 * matrix with "width" columns has when elements are counted row by row. This
 * function is denoted e_{width} in the HLD.
 *
 * @see m_dec()
 */
static uint64_t m_enc(uint64_t width, uint64_t row, uint64_t column)
{
	M0_ASSERT(column < width);
	return row * width + column;
}

/**
 * "Decoding" function: returns (row, column) coordinates of a pos-th element in
 * a matrix with "width" column when elements are counted row by row. This
 * function is denoted d_{width} in the HLD.
 *
 * @see m_enc()
 */
static void m_dec(uint64_t width, uint64_t pos, uint64_t *row, uint64_t *column)
{
	*row    = pos / width;
	*column = pos % width;
}

/**
 * Apply a permutation given by its Lehmer code in k[] to a set s[] of n
 * elements and build inverse permutation in r[].
 *
 * @param n - number of elements in k[], s[] and r[]
 * @param k - Lehmer code of the permutation
 * @param s - an array to permute
 * @param r - an array to build inverse permutation in
 *
 * @pre  m0_forall(i, n, k[i] + i < n)
 * @pre  m0_forall(i, n, s[i] < n && ergo(s[i] == s[j], i == j))
 * @post m0_forall(i, n, s[i] < n && ergo(s[i] == s[j], i == j))
 * @post m0_forall(i, n, s[r[i]] == i && r[s[i]] == i)
 */
static void permute(uint32_t n, uint32_t *k, uint32_t *s, uint32_t *r)
{
	uint32_t i;
	uint32_t j;
	uint32_t t;
	uint32_t x;

	/*
	 * k[0] is an index of one of the n elements that permutation moves to
	 * the 0-th position in s[];
	 *
	 * k[1] is an index of one of the (n - 1) remaining elements that
	 * permutation moves to the 1-st position in s[], etc.
	 *
	 * To produce i-th element of s[], pick one of remaining elements, say
	 * s[t], as specified by k[i], shift elements s[i] ... s[t] to the right
	 * by one and place s[t] in s[i]. This guarantees that at beginning of
	 * the loop elements s[0] ... s[i - 1] are already selected and elements
	 * s[i] ... s[n - 1] are "remaining".
	 */

	for (i = 0; i < n - 1; ++i) {
		t = k[i] + i;
		M0_ASSERT(t < n);
		x = s[t];
		for (j = t; j > i; --j)
			s[j] = s[j - 1];
		s[i] = x;
		r[x] = i;
	}
	/*
	 * The loop above iterates n-1 times, because the last element finds its
	 * place automatically. Complete inverse permutation.
	 */
	r[s[n - 1]] = n - 1;
}

/**
 * Returns column number that a column t has after a permutation for tile omega
 * is applied.
 */
static uint64_t permute_column(struct m0_pdclust_instance *pi,
			       uint64_t omega, uint64_t t)
{
	struct tile_cache        *tc;
	struct m0_pdclust_attr    attr;
	struct m0_pdclust_layout *pl;
	struct m0_fid            *gfid;

	pl   = pi_to_pl(pi);
	attr =  pl->pl_attr;
	gfid = &pi->pi_base.li_gfid;

	M0_ENTRY("t %lu, P %lu", (unsigned long)t, (unsigned long)attr.pa_P);
	M0_ASSERT(t < attr.pa_P);
	tc = &pi->pi_tile_cache;

	/* If cached values are for different tile, update the cache. */
	if (tc->tc_tile_no != omega) {
		uint32_t i;
		uint64_t rstate;

		/* Initialise columns array that will be permuted. */
		for (i = 0; i < attr.pa_P; ++i)
			tc->tc_permute[i] = i;

		/* Initialise PRNG. */
		rstate = m0_hash(attr.pa_seed.u_hi + gfid->f_key) ^
			 m0_hash(attr.pa_seed.u_lo + omega + gfid->f_container);

		/* Generate permutation number in lexicographic ordering. */
		for (i = 0; i < attr.pa_P - 1; ++i)
			tc->tc_lcode[i] = m0_rnd(attr.pa_P - i, &rstate);

		/* Apply the permutation. */
		permute(attr.pa_P, tc->tc_lcode,
			tc->tc_permute, tc->tc_inverse);
		tc->tc_tile_no = omega;
	}

	/**
	 * @todo Not sure if this should be replaced by an ADDB DP or a M0_LOG.
	 */

	M0_POST(tc->tc_permute[t] < attr.pa_P);
	M0_POST(tc->tc_inverse[tc->tc_permute[t]] == t);
	M0_POST(tc->tc_permute[tc->tc_inverse[t]] == t);
	return tc->tc_permute[t];
}

M0_INTERNAL void m0_pdclust_instance_map(struct m0_pdclust_instance *pi,
					 const struct m0_pdclust_src_addr *src,
					 struct m0_pdclust_tgt_addr *tgt)
{
	struct m0_pdclust_layout *pl;
	uint32_t                  N;
	uint32_t                  K;
	uint32_t                  P;
	uint32_t                  C;
	uint32_t                  L;
	uint64_t                  omega;
	uint64_t                  j;
	uint64_t                  r;
	uint64_t                  t;

	M0_PRE(pdclust_instance_invariant(pi));

	M0_ENTRY("pi %p", pi);

	pl = pi_to_pl(pi);
	N = pl->pl_attr.pa_N;
	K = pl->pl_attr.pa_K;
	P = pl->pl_attr.pa_P;
	C = pl->pl_C;
	L = pl->pl_L;

	/*
	 * First translate source address into a tile number and parity group
	 * number in the tile.
	 */
	m_dec(C, src->sa_group, &omega, &j);
	/*
	 * Then, convert from C*(N+2*K) coordinates to L*P coordinates within a
	 * tile.
	 */
	m_dec(P, m_enc(N + 2*K, j, src->sa_unit), &r, &t);
	/* Permute columns */
	tgt->ta_obj = permute_column(pi, omega, t);
	/* And translate back from tile to target address. */
	tgt->ta_frame = m_enc(L, omega, r);
	M0_LEAVE("pi %p", pi);
}

M0_INTERNAL void m0_pdclust_instance_inv(struct m0_pdclust_instance *pi,
					 const struct m0_pdclust_tgt_addr *tgt,
					 struct m0_pdclust_src_addr *src)
{
	struct m0_pdclust_layout *pl;
	uint32_t                  N;
	uint32_t                  K;
	uint32_t                  P;
	uint32_t                  C;
	uint32_t                  L;
	uint64_t                  omega;
	uint64_t                  j;
	uint64_t                  r;
	uint64_t                  t;

	pl = pi_to_pl(pi);
	N = pl->pl_attr.pa_N;
	K = pl->pl_attr.pa_K;
	P = pl->pl_attr.pa_P;
	C = pl->pl_C;
	L = pl->pl_L;

	r = tgt->ta_frame;
	t = tgt->ta_obj;

	M0_ASSERT(pdclust_instance_invariant(pi));

	/*
	 * Execute inverses of the steps of m0_pdclust_instance_map() in
	 * reverse order.
	 */
	m_dec(L, tgt->ta_frame, &omega, &r);
	permute_column(pi, omega, t); /* Force tile cache update */
	t = pi->pi_tile_cache.tc_inverse[t];
	m_dec(N + 2*K, m_enc(P, r, t), &j, &src->sa_unit);
	src->sa_group = m_enc(C, omega, j);
}

static const struct m0_layout_instance_ops pdclust_instance_ops;
M0_INTERNAL void pdclust_instance_fini(struct m0_layout_instance *li);

M0_INTERNAL void m0_pdclust_perm_cache_destroy(struct m0_layout *layout,
				               struct m0_pdclust_instance *pi)
{
	struct m0_pool_version *pool_ver;
	uint64_t                cache_cnt;
	uint64_t                i;

	pool_ver = layout->l_pver;
	M0_ASSERT(pool_ver != NULL);
	cache_cnt = layout->l_pver->pv_fd_tree.ft_cache_info.fci_nr;
	for (i = 0; i < cache_cnt; ++i)
		m0_fd_perm_cache_fini(&pi->pi_perm_cache[i]);
	m0_free(pi->pi_perm_cache);
	pi->pi_cache_nr = 0;
}

M0_INTERNAL int m0_pdclust_perm_cache_build(struct m0_layout *layout,
				            struct m0_pdclust_instance *pi)
{
	struct m0_fd_cache_info *cache_info;
	uint64_t                 i;
	int                      rc = 0;

	M0_PRE(layout != NULL && layout->l_pver != NULL);
	cache_info = &layout->l_pver->pv_fd_tree.ft_cache_info;
	M0_ALLOC_ARR(pi->pi_perm_cache, cache_info->fci_nr);
	if (pi->pi_perm_cache == NULL)
		return M0_ERR(-ENOMEM);
	for (i = 0; i < cache_info->fci_nr; ++i) {
		rc = m0_fd_perm_cache_init(&pi->pi_perm_cache[i],
				           cache_info->fci_info[i]);
		if (rc != 0)
			break;
	}
	if (rc != 0)
		m0_pdclust_perm_cache_destroy(layout, pi);
	pi->pi_cache_nr = cache_info->fci_nr;
	return M0_RC(rc);
}

M0_INTERNAL bool m0_pdclust_is_replicated(struct m0_pdclust_layout *play)
{
	return play->pl_attr.pa_N == 1;
}

/**
 * Implementation of lo_instance_build().
 *
 * Allocates and builds a parity de-clustered layout instance using the
 * supplied layout 'l' that is necessarily of the type pdclust. It acquires an
 * additional reference on that layout.
 * @pre pdclust_invariant(pl)
 * @post ergo(rc == 0, pdclust_instance_invariant(*out) &&
		       m0_ref_read(&l->l_ref) > 1))
 */
static int pdclust_instance_build(struct m0_layout           *l,
				  const struct m0_fid        *fid,
				  struct m0_layout_instance **out)
{
	struct m0_pdclust_layout   *pl = m0_layout_to_pdl(l);
	struct m0_pdclust_instance *pi;
	struct tile_cache          *tc = NULL; /* to keep gcc happy */
	uint32_t                    N;
	uint32_t                    K;
	uint32_t                    P;
	int                         rc;

	M0_PRE(pdclust_invariant(pl));
	M0_PRE(m0_fid_is_valid(fid));
	M0_PRE(out != NULL);

	M0_ENTRY("lid %llu, gfid "FID_F,
		 (unsigned long long)l->l_id, FID_P(fid));
	N  = pl->pl_attr.pa_N;
	K  = pl->pl_attr.pa_K;
	P  = pl->pl_attr.pa_P;

	if (M0_FI_ENABLED("mem_err1")) { pi = NULL; goto err1_injected; }
	M0_ALLOC_PTR(pi);
err1_injected:
	if (pi != NULL) {
		rc = m0_pdclust_perm_cache_build(l, pi);
		if (rc != 0)
		return M0_RC(rc);

		tc = &pi->pi_tile_cache;

		if (M0_FI_ENABLED("mem_err2"))
			{ tc->tc_lcode = NULL; goto err2_injected; }
		M0_ALLOC_ARR(tc->tc_lcode, P);
		M0_ALLOC_ARR(tc->tc_permute, P);
		M0_ALLOC_ARR(tc->tc_inverse, P);
err2_injected:
		if (tc->tc_lcode != NULL &&
		    tc->tc_permute != NULL &&
		    tc->tc_inverse != NULL) {
			tc->tc_tile_no = 1;

			if (M0_FI_ENABLED("parity_math_err"))
				{ rc = -EPROTO; goto err3_injected; }
			if (K > 0 && N != 1)
				rc = m0_parity_math_init(&pi->pi_math, N, K);
err3_injected:
			if (rc == 0) {
				m0_layout__instance_init(&pi->pi_base, fid, l,
							&pdclust_instance_ops);
				m0_pdclust_instance_bob_init(pi);
				m0_mutex_init(&pi->pi_mutex);
				permute_column(pi, 0, 0);
			}
			else
				M0_LOG(M0_ERROR, "pi %p, m0_parity_math_init()"
						" failed, rc %d", pi, rc);
		} else
			rc = -ENOMEM;
	} else
		rc = -ENOMEM;

	if (rc == 0) {
		*out = &pi->pi_base;
		M0_POST(pdclust_instance_invariant(pi));
		M0_POST(m0_ref_read(&l->l_ref) > 1);
	} else {
		if (rc == -ENOMEM)
			m0_layout__log("pdclust_instance_build",
				       "M0_ALLOC() failed",
				       l->l_id, rc);
		if (pi != NULL) {
			m0_free(tc->tc_inverse);
			m0_free(tc->tc_permute);
			m0_free(tc->tc_lcode);
		}
		m0_free(pi);
	}

	M0_LEAVE("rc %d", rc);
	return M0_RC(rc);
}

/** Implementation of lio_fini(). */
M0_INTERNAL void pdclust_instance_fini(struct m0_layout_instance *li)
{
	struct m0_pdclust_instance *pi;
	struct m0_pdclust_layout   *pl;
	struct m0_layout           *layout;

	pi = m0_layout_instance_to_pdi(li);
	M0_ENTRY("pi %p", pi);
	layout = li->li_l;
	pl = m0_layout_to_pdl(layout);
	if (pl->pl_attr.pa_K > 0)
		m0_parity_math_fini(&pi->pi_math);
	m0_pdclust_perm_cache_destroy(layout, pi);
	m0_layout__instance_fini(&pi->pi_base);
	m0_mutex_fini(&pi->pi_mutex);
	m0_pdclust_instance_bob_fini(pi);
	m0_free(pi->pi_tile_cache.tc_inverse);
	m0_free(pi->pi_tile_cache.tc_permute);
	m0_free(pi->pi_tile_cache.tc_lcode);
	m0_free(pi);
	M0_LEAVE();
}

static const struct m0_layout_ops pdclust_ops = {
	.lo_fini           = pdclust_fini,
	.lo_delete         = pdclust_delete,
	.lo_recsize        = pdclust_recsize,
	.lo_instance_build = pdclust_instance_build,
	.lo_decode         = pdclust_decode,
	.lo_encode         = pdclust_encode
};

static const struct m0_layout_type_ops pdclust_type_ops = {
	.lto_register    = pdclust_register,
	.lto_unregister  = pdclust_unregister,
	.lto_max_recsize = pdclust_max_recsize,
	.lto_allocate    = pdclust_allocate,
};

struct m0_layout_type m0_pdclust_layout_type = {
	.lt_name      = "pdclust",
	.lt_id        = 0,
	.lt_ref_count = 0,
	.lt_ops       = &pdclust_type_ops
};

static const struct m0_layout_instance_ops pdclust_instance_ops = {
	.lio_fini    = pdclust_instance_fini,
	.lio_to_enum = pdclust_instance_to_enum
};

#undef M0_TRACE_SUBSYSTEM

/** @} end group pdclust */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
