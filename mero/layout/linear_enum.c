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
 * Original creation date: 11/16/2011
 */

/**
 * @addtogroup linear_enum
 *
 * @{
 */

#include "lib/errno.h"
#include "lib/tlist.h"  /* struct m0_tl */
#include "lib/vec.h"    /* m0_bufvec_cursor_step(), m0_bufvec_cursor_addr() */
#include "lib/memory.h" /* M0_ALLOC_PTR() */
#include "lib/misc.h"   /* M0_IN() */
#include "lib/bob.h"
#include "lib/finject.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

#include "mero/magic.h"
#include "fid/fid.h"    /* m0_fid_set(), m0_fid_is_valid() */
#include "layout/layout_internal.h"
#include "layout/linear_enum.h"
#include "ioservice/fid_convert.h" /* m0_fid_convert_gob2cob */

static const struct m0_bob_type linear_bob = {
	.bt_name         = "linear_enum",
	.bt_magix_offset = offsetof(struct m0_layout_linear_enum, lla_magic),
	.bt_magix        = M0_LAYOUT_LINEAR_ENUM_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(static, &linear_bob, m0_layout_linear_enum);

static bool linear_allocated_invariant(const struct m0_layout_linear_enum *le)
{
	return
		m0_layout_linear_enum_bob_check(le) &&
		le->lle_attr.lla_nr == 0 &&
		le->lle_attr.lla_B == 0;
}

/**
 * linear_invariant() can not be invoked until an enumeration object
 * is associated with some layout object. Hence this separation.
 */
static bool linear_invariant(const struct m0_layout_linear_enum *le)
{
	return
		m0_layout_linear_enum_bob_check(le) &&
		le->lle_attr.lla_nr != 0 &&
		le->lle_attr.lla_B != 0 &&
		m0_layout__enum_invariant(&le->lle_base);
}

static const struct m0_layout_enum_ops linear_enum_ops;

/** Implementation of leto_allocate for LINEAR enumeration type. */
static int linear_allocate(struct m0_layout_domain *dom,
			   struct m0_layout_enum **out)
{
	struct m0_layout_linear_enum *lin_enum;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(out != NULL);

	M0_ENTRY();

	if (M0_FI_ENABLED("mem_err")) { lin_enum = NULL; goto err1_injected; }
	M0_ALLOC_PTR(lin_enum);
err1_injected:
	if (lin_enum == NULL) {
		m0_layout__log("linear_allocate", "M0_ALLOC_PTR() failed",
			       LID_NONE, -ENOMEM);
		return M0_ERR(-ENOMEM);
	}
	m0_layout__enum_init(dom, &lin_enum->lle_base,
			     &m0_linear_enum_type, &linear_enum_ops);
	m0_layout_linear_enum_bob_init(lin_enum);
	M0_POST(linear_allocated_invariant(lin_enum));
	*out = &lin_enum->lle_base;
	M0_LEAVE("linear enum pointer %p", lin_enum);
	return 0;
}

static void linear_delete(struct m0_layout_enum *e)
{
	struct m0_layout_linear_enum *lin_enum;

	lin_enum = bob_of(e, struct m0_layout_linear_enum,
		          lle_base, &linear_bob);
	M0_PRE(linear_allocated_invariant(lin_enum));

	M0_ENTRY("enum_pointer %p", e);
	m0_layout_linear_enum_bob_fini(lin_enum);
	m0_layout__enum_fini(e);
	m0_free(lin_enum);
	M0_LEAVE();
}

static int linear_populate(struct m0_layout_linear_enum *lin_enum,
			   const struct m0_layout_linear_attr *attr)
{
	M0_PRE(linear_allocated_invariant(lin_enum));
	M0_PRE(attr != NULL);

	if (attr->lla_nr == 0 || attr->lla_B == 0) {
		M0_LOG(M0_ERROR,
			"lin_enum %p, attr %p,  Invalid attributes, rc %d",
		       lin_enum, attr, -EPROTO);
		return M0_ERR(-EPROTO);
	}
	lin_enum->lle_attr = *attr;
	M0_POST(linear_invariant(lin_enum));
	return 0;
}

M0_INTERNAL int m0_linear_enum_build(struct m0_layout_domain *dom,
				     const struct m0_layout_linear_attr *attr,
				     struct m0_layout_linear_enum **out)
{
	struct m0_layout_enum        *e;
	struct m0_layout_linear_enum *lin_enum;
	int                           rc;

	M0_PRE(out != NULL);
	M0_ENTRY("domain %p", dom);
	rc = linear_allocate(dom, &e);
	if (rc == 0) {
		lin_enum = bob_of(e, struct m0_layout_linear_enum,
			          lle_base, &linear_bob);
		rc = linear_populate(lin_enum, attr);
		if (rc == 0)
			*out = lin_enum;
		else
			linear_delete(e);
	}
	M0_POST(ergo(rc == 0, linear_invariant(lin_enum)));
	M0_LEAVE("domain %p, rc %d", dom, rc);
	return M0_RC(rc);
}

static struct m0_layout_linear_enum *
enum_to_linear_enum(const struct m0_layout_enum *e)
{
	struct m0_layout_linear_enum *lin_enum;

	lin_enum = bob_of(e, struct m0_layout_linear_enum,
			  lle_base, &linear_bob);
	M0_ASSERT(linear_invariant(lin_enum));
	return lin_enum;
}

/** Implementation of leo_fini for LINEAR enumeration type. */
static void linear_fini(struct m0_layout_enum *e)
{
	struct m0_layout_linear_enum *lin_enum;
	uint64_t                      lid;

	M0_PRE(m0_layout__enum_invariant(e));

	lid = e->le_sl_is_set ? e->le_sl->sl_base.l_id : 0;
	M0_ENTRY("lid %llu, enum_pointer %p", (unsigned long long)lid, e);
	lin_enum = enum_to_linear_enum(e);
	m0_layout_linear_enum_bob_fini(lin_enum);
	m0_layout__enum_fini(e);
	m0_free(lin_enum);
	M0_LEAVE();
}

/**
 * Implementation of leto_register for LINEAR enumeration type.
 * No table is required specifically for LINEAR enum type.
 */
static int linear_register(struct m0_layout_domain *dom,
			   const struct m0_layout_enum_type *et)
{
	return 0;
}

/** Implementation of leto_unregister for LINEAR enumeration type. */
static void linear_unregister(struct m0_layout_domain *dom,
			      const struct m0_layout_enum_type *et)
{
}

/** Implementation of leto_max_recsize() for linear enumeration type. */
static m0_bcount_t linear_max_recsize(void)
{
	return sizeof(struct m0_layout_linear_attr);
}

/** Implementation of leo_decode() for linear enumeration type. */
static int linear_decode(struct m0_layout_enum *e,
			 struct m0_bufvec_cursor *cur,
			 enum m0_layout_xcode_op op,
			 struct m0_be_tx *tx,
			 struct m0_striped_layout *stl)
{
	uint64_t                      lid;
	struct m0_layout_linear_enum *lin_enum;
	struct m0_layout_linear_attr *lin_attr;
	int                           rc;

	M0_PRE(e != NULL);
	M0_PRE(cur != NULL);
	M0_PRE(m0_bufvec_cursor_step(cur) >= sizeof *lin_attr);
	M0_PRE(M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op == M0_LXO_DB_LOOKUP, tx != NULL));
	M0_PRE(m0_layout__striped_allocated_invariant(stl));

	lid = stl->sl_base.l_id;
	M0_ENTRY("lid %llu", (unsigned long long)lid);
	lin_enum = bob_of(e, struct m0_layout_linear_enum,
			  lle_base, &linear_bob);
	M0_ASSERT(linear_allocated_invariant(lin_enum));

	lin_attr = m0_bufvec_cursor_addr(cur);
	m0_bufvec_cursor_move(cur, sizeof *lin_attr);

	if (M0_FI_ENABLED("attr_err")) { lin_attr->lla_nr = 0; }
	rc = linear_populate(lin_enum, lin_attr);
	if (rc != 0)
		M0_LOG(M0_ERROR, "linear_populate() failed");
	M0_POST(ergo(rc == 0, linear_invariant(lin_enum)));
	M0_POST(ergo(rc != 0, linear_allocated_invariant(lin_enum)));
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)lid, rc);
	return M0_RC(rc);
}

/** Implementation of leo_encode() for linear enumeration type. */
static int linear_encode(const struct m0_layout_enum *e,
			 enum m0_layout_xcode_op op,
			 struct m0_be_tx *tx,
			 struct m0_bufvec_cursor *out)
{
	struct m0_layout_linear_enum *lin_enum;
	m0_bcount_t                   nbytes;
	uint64_t                      lid;

	M0_PRE(e != NULL);
	M0_PRE(M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_UPDATE,
			  M0_LXO_DB_DELETE, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op != M0_LXO_BUFFER_OP, tx != NULL));
	M0_PRE(out != NULL);
	M0_PRE(m0_bufvec_cursor_step(out) >= sizeof lin_enum->lle_attr);

	lin_enum = enum_to_linear_enum(e);
	lid = lin_enum->lle_base.le_sl->sl_base.l_id;
	M0_ENTRY("lid %llu", (unsigned long long)lid);
	nbytes = m0_bufvec_cursor_copyto(out, &lin_enum->lle_attr,
					 sizeof lin_enum->lle_attr);
	M0_ASSERT(nbytes == sizeof lin_enum->lle_attr);
	M0_LEAVE("lid %llu", (unsigned long long)lid);
	return 0;
}

/** Implementation of leo_nr for LINEAR enumeration. */
static uint32_t linear_nr(const struct m0_layout_enum *e)
{
	struct m0_layout_linear_enum *lin_enum;

	M0_PRE(e != NULL);

	M0_ENTRY("lid %llu, enum_pointer %p",
		 (unsigned long long)e->le_sl->sl_base.l_id, e);
	lin_enum = enum_to_linear_enum(e);
	M0_LEAVE("lid %llu, enum_pointer %p, nr %lu",
		 (unsigned long long)e->le_sl->sl_base.l_id, e,
		 (unsigned long)lin_enum->lle_attr.lla_nr);
	return lin_enum->lle_attr.lla_nr;
}

/** Implementation of leo_get for LINEAR enumeration. */
static void linear_get(const struct m0_layout_enum *e, uint32_t idx,
		       const struct m0_fid *gfid, struct m0_fid *out)
{
	struct m0_layout_linear_enum *lin_enum;

	M0_PRE(e != NULL);
	M0_PRE(gfid != NULL);
	M0_PRE(out != NULL);

	M0_ENTRY("lid %llu, enum_pointer %p",
		 (unsigned long long)e->le_sl->sl_base.l_id, e);
	lin_enum = enum_to_linear_enum(e);
	M0_ASSERT(idx < lin_enum->lle_attr.lla_nr);
	m0_fid_convert_gob2cob(gfid, out, lin_enum->lle_attr.lla_A +
					  idx * lin_enum->lle_attr.lla_B);

	M0_LEAVE("lid %llu, enum_pointer %p, fid_pointer %p",
		 (unsigned long long)e->le_sl->sl_base.l_id, e, out);
	M0_ASSERT(m0_fid_is_valid(out));
}

/** Implementation of leo_recsize() for linear enumeration type. */
static m0_bcount_t linear_recsize(struct m0_layout_enum *e)
{
	return sizeof(struct m0_layout_linear_attr);
}

static const struct m0_layout_enum_ops linear_enum_ops = {
	.leo_nr      = linear_nr,
	.leo_get     = linear_get,
	.leo_recsize = linear_recsize,
	.leo_fini    = linear_fini,
	.leo_delete  = linear_delete,
	.leo_decode  = linear_decode,
	.leo_encode  = linear_encode
};

static const struct m0_layout_enum_type_ops linear_type_ops = {
	.leto_register    = linear_register,
	.leto_unregister  = linear_unregister,
	.leto_max_recsize = linear_max_recsize,
	.leto_allocate    = linear_allocate
};

struct m0_layout_enum_type m0_linear_enum_type = {
	.let_name      = "linear",
	.let_id        = 1,
	.let_ref_count = 0,
	.let_ops       = &linear_type_ops
};

#undef M0_TRACE_SUBSYSTEM

/** @} end group linear_enum */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
