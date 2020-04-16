/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 09-Sep-2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"

#include "lib/errno.h"         /* EINVAL */
#include "lib/misc.h"          /* memcmp, strcmp */
#include "lib/string.h"        /* sscanf */
#include "lib/assert.h"        /* M0_PRE */
#include "lib/hash.h"          /* m0_hash */
#include "lib/arith.h"         /* m0_rnd */
#include "lib/uuid.h"          /* m0_uuid_generate */
#include "fid/fid_xc.h"
#include "fid/fid.h"
#include "lib/memory.h"        /* M0_ALLOC_ARR */

/**
   @addtogroup fid

   @{
 */

/* TODO move to m0 */
static const struct m0_fid_type *fid_types[256];

M0_INTERNAL void m0_fid_type_register(const struct m0_fid_type *fidt)
{
	uint8_t id = fidt->ft_id;

	M0_PRE(IS_IN_ARRAY(id, fid_types));
	M0_PRE(fid_types[id] == NULL);
	fid_types[id] = fidt;
}

M0_INTERNAL void m0_fid_type_unregister(const struct m0_fid_type *fidt)
{
	uint8_t id = fidt->ft_id;

	M0_PRE(IS_IN_ARRAY(id, fid_types));
	M0_PRE(fid_types[id] == fidt);
	fid_types[id] = NULL;
}

M0_INTERNAL const struct m0_fid_type *m0_fid_type_get(uint8_t id)
{
	M0_PRE(IS_IN_ARRAY(id, fid_types));
	return fid_types[id];
}

M0_INTERNAL const struct m0_fid_type *m0_fid_type_gethi(uint64_t id)
{
	return m0_fid_type_get(id >> (64 - 8));
}

M0_INTERNAL const struct m0_fid_type *
m0_fid_type_getfid(const struct m0_fid *fid)
{
	return m0_fid_type_gethi(fid->f_container);
}

M0_INTERNAL const struct m0_fid_type *m0_fid_type_getname(const char *name)
{
	size_t i;
	const struct m0_fid_type *fidt;

	for (i = 0; i < ARRAY_SIZE(fid_types); ++i) {
		fidt = fid_types[i];
		M0_ASSERT(ergo(fidt != NULL, fidt->ft_name != NULL));
		if (fidt != NULL && strcmp(name, fidt->ft_name) == 0)
			return fidt;
	}

	return NULL;
}

M0_INTERNAL bool m0_fid_is_valid(const struct m0_fid *fid)
{
	const struct m0_fid_type *ft = m0_fid_type_getfid(fid);

	return
		ft != NULL &&
		ergo(ft->ft_is_valid != NULL, ft->ft_is_valid(fid));
}
M0_EXPORTED(m0_fid_is_valid);

M0_INTERNAL bool m0_fid_is_set(const struct m0_fid *fid)
{
	static const struct m0_fid zero = {
		.f_container = 0,
		.f_key = 0
	};
	return !m0_fid_eq(fid, &zero);
}
M0_EXPORTED(m0_fid_is_set);

M0_INTERNAL void m0_fid_set(struct m0_fid *fid, uint64_t container,
			    uint64_t key)
{
	M0_PRE(fid != NULL);

	fid->f_container = container;
	fid->f_key = key;
}
M0_EXPORTED(m0_fid_set);

M0_INTERNAL void m0_fid_tset(struct m0_fid *fid,
			     uint8_t tid, uint64_t container, uint64_t key)
{
	m0_fid_set(fid, M0_FID_TCONTAINER(tid, container), key);
}
M0_EXPORTED(m0_fid_tset);

M0_INTERNAL uint8_t m0_fid_tget(const struct m0_fid *fid)
{
	return fid->f_container >> 56;
}
M0_EXPORTED(m0_fid_tget);

M0_INTERNAL void m0_fid_tchange(struct m0_fid *fid, uint8_t tid)
{
	M0_PRE(fid != NULL);
	fid->f_container = M0_FID_TCONTAINER(tid, fid->f_container);
	M0_POST(m0_fid_is_valid(fid));
}

M0_INTERNAL void m0_fid_tassume(struct m0_fid *fid,
				const struct m0_fid_type *ft)
{
	M0_PRE(fid != NULL);
	M0_PRE(ft != NULL);

	m0_fid_tchange(fid, ft->ft_id);
}

M0_INTERNAL void m0_fid_tgenerate(struct m0_fid *fid,
				  const uint8_t  tid)
{
	M0_PRE(fid != NULL);

	m0_uuid_generate((struct m0_uint128*)fid);
	m0_fid_tchange(fid, tid);
}

M0_INTERNAL bool m0_fid_eq(const struct m0_fid *fid0, const struct m0_fid *fid1)
{
	return memcmp(fid0, fid1, sizeof *fid0) == 0;
}
M0_EXPORTED(m0_fid_eq);

M0_INTERNAL int m0_fid_cmp(const struct m0_fid *fid0, const struct m0_fid *fid1)
{
	const struct m0_uint128 u0 = {
		.u_hi = fid0->f_container,
		.u_lo = fid0->f_key
	};

	const struct m0_uint128 u1 = {
		.u_hi = fid1->f_container,
		.u_lo = fid1->f_key
	};

	return m0_uint128_cmp(&u0, &u1);
}
M0_EXPORTED(m0_fid_cmp);

/**
 * Parses fid string representation.
 *
 * Three formats are supported:
 *
 *     * CONT:KEY, where CONT and KEY are in sexadecimal.
 *     * <CONT:KEY>, where CONT and KEY are in sexadecimal.
 *
 *     * TYPE|CONT:KEY, where TYPE is a 1-character fid type
 *       (m0_fid_type::ft_id), CONT is the container sans type and KEY is the
 *       key (key and container are in %*i format: decimal by default,
 *       sexadecimal when start with 0x, octal when start with 0).
 */
static int fid_sscanf(const char *s, struct m0_fid *fid, int *nob)
{
	int rc;

	/* First check format with braces. */
	rc = sscanf(s, FID_SF" %n", FID_S(fid), nob);
	if (rc != 2) {
		/* If not found then check the without braces. */
		rc = sscanf(s, " %"SCNx64" : %"SCNx64" %n", FID_S(fid), nob);
		/* See a comment in m0_xcode_read() on the effects of %n. */
		if (rc != 2) {
			uint8_t ft;

			/* Check for other formats. */
			rc = sscanf(s, " %c | %"SCNi64" : %"SCNi64" %n",
				    &ft, FID_S(fid), nob);
			if (rc == 3 && m0_fid_tget(fid) == 0) {
				m0_fid_tchange(fid, ft);
				rc = 0;
			} else
				rc = -EINVAL; /* No M0_ERR() here. */
		} else
			rc = 0;
	} else
		rc = 0;
	return M0_RC(rc);
}

M0_INTERNAL int m0_fid_sscanf(const char *s, struct m0_fid *fid)
{
	int nob;
	return fid_sscanf(s, fid, &nob);
}

M0_INTERNAL int m0_fid_print(char *s, size_t s_len, const struct m0_fid *fid)
{
	int rc;

	M0_PRE(s != NULL);
	M0_PRE(s_len >=  M0_FID_STR_LEN);
	M0_PRE(fid != NULL);

	rc = snprintf(s, s_len, "%"PRIx64":%"PRIx64, FID_P(fid));
	if (rc < 0 || rc >= s_len)
		return M0_ERR(-EINVAL);

	return M0_RC(rc);
}

/**
 * Type of miscellaneous fids used in tests, etc.
 */
static const struct m0_fid_type misc = {
	.ft_id   = 0,
	.ft_name = "miscellaneous"
};

/**
 * m0_xcode_type_ops::xto_read() implementation for fids.
 *
 * Parses fids in xcode-readable strings.
 *
 * @see xt_ops, m0_xcode_read().
 */
static int xt_read(const struct m0_xcode_cursor *it,
		   struct m0_xcode_obj *obj, const char *str)
{
	int result;
	int nr;

	M0_ASSERT(obj->xo_type == m0_fid_xc);
	result = fid_sscanf(str, obj->xo_ptr, &nr);
	return result == 0 ? nr : M0_ERR(result);
}

/**
 * xcode operations for fids.
 */
static const struct m0_xcode_type_ops xt_ops = {
	.xto_read = &xt_read
};

M0_INTERNAL int m0_fid_init(void)
{
	m0_fid_type_register(&misc);
	m0_fid_xc->xct_ops = &xt_ops;
	return 0;
}
M0_EXPORTED(m0_fid_init);

M0_INTERNAL void m0_fid_fini(void)
{
	m0_fid_type_unregister(&misc);
}
M0_EXPORTED(m0_fid_fini);

M0_INTERNAL uint64_t m0_fid_hash(const struct m0_fid *fid)

{
	return m0_hash(M0_CIRCULAR_SHIFT_LEFT(fid->f_container, 3) ^
		       M0_CIRCULAR_SHIFT_LEFT(fid->f_key, 17));

}

M0_INTERNAL int m0_fid_arr_copy(struct m0_fid_arr *to,
				const struct m0_fid_arr *from)
{
	int i;

	M0_ALLOC_ARR(to->af_elems, from->af_count);
	if (to->af_elems == NULL)
		return M0_ERR(-ENOMEM);

	to->af_count = from->af_count;
	for (i = 0; i < to->af_count; ++i)
		to->af_elems[i] = from->af_elems[i];

	return M0_RC(0);
}

M0_INTERNAL bool m0_fid_arr_eq(const struct m0_fid_arr *a,
			       const struct m0_fid_arr *b)
{
	return b->af_count == a->af_count
	    && m0_forall(i, a->af_count, m0_fid_eq(&a->af_elems[i],
						   &b->af_elems[i]));
}

M0_INTERNAL bool m0_fid_arr_all_unique(const struct m0_fid_arr *a)
{
	int i;
	int j;

	for (i = 0; i < a->af_count; ++i) {
		for (j = i + 1; j < a->af_count; ++j)
			if (m0_fid_eq(&a->af_elems[i], &a->af_elems[j]))
				return false;
	}
	return true;
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of fid group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
