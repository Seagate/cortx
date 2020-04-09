/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original author: Rajesh Bhalerao <rajesh_bhalerao@xyratex.com>
 * Original creation date: 05/12/2011
 */
#include "lib/finject.h"
#include "lib/mutex.h"
#include "lib/list.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/chan.h"
#include "lib/vec.h"
#include "fid/fid.h"      /* M0_FID_TINIT */
#include "xcode/xcode.h"
#include "ut/ut.h"

#include "rm/rm.h"
#include "rm/ut/rings.h"

struct m0_rm_resource_type rings_resource_type = {
	.rt_name = "Rings of Power",
	.rt_id   = RINGS_RESOURCE_TYPE_ID,
};

static void rings_policy(struct m0_rm_resource *resource,
			 struct m0_rm_incoming *in)
{
	struct m0_rm_credit *credit;
	struct m0_rm_pin    *pin;
	uint64_t             datum;
	int                  zeroes;

	/* Grant "real" ring if ANY_RING was requested */
	if (in->rin_want.cr_datum == ANY_RING) {
		/* One credit is always enough to satisfy request for ANY_RING.
		 * RM should not pin more than one credit in tests.
		 */
		M0_ASSERT(pi_tlist_length(&in->rin_pins) == 1);
		m0_tl_for (pi, &in->rin_pins, pin) {
			M0_ASSERT(pin->rp_flags == M0_RPF_PROTECT);
			credit = pin->rp_credit;
			datum = credit->cr_datum;
			zeroes = 0;
			M0_ASSERT(datum != 0);
			while ((datum & 0x01) == 0) {
				zeroes++;
				datum >>= 1;
			}
			in->rin_want.cr_datum =  1 << zeroes;
		} m0_tl_endfor;
	}

}

static void rings_credit_init(struct m0_rm_resource *resource,
			      struct m0_rm_credit   *credit)
{
	credit->cr_datum = 0;
	credit->cr_ops = &rings_credit_ops;
}

void rings_resource_free(struct m0_rm_resource *resource)
{
	struct m0_rings *rings;

	rings = container_of(resource, struct m0_rings, rs_resource);
	m0_free(rings);
}

const struct m0_rm_resource_ops rings_ops = {
	.rop_credit_decode  = NULL,
	.rop_policy         = rings_policy,
	.rop_credit_init    = rings_credit_init,
	.rop_resource_free  = rings_resource_free,
};

static bool rings_resources_are_equal(const struct m0_rm_resource *c0,
				      const struct m0_rm_resource *c1)
{
	return c0 == c1;
}

static bool rings_resource_is(const struct m0_rm_resource *res, uint64_t res_id)
{
	struct m0_rings *ring;

	ring = container_of(res, struct m0_rings, rs_resource);
	M0_ASSERT(ring != NULL);
	return res_id == ring->rs_id;
}

static m0_bcount_t rings_resource_len(const struct m0_rm_resource *resource)
{
	/* Resource type id + resource id */
	return (m0_bcount_t) sizeof(uint64_t) + sizeof(uint64_t);
}

static int rings_resource_encode(struct m0_bufvec_cursor     *cur,
				 const struct m0_rm_resource *resource)
{
	struct m0_rings *rings;

	rings = container_of(resource, struct m0_rings, rs_resource);
	M0_ASSERT(rings != NULL);

	m0_bufvec_cursor_copyto(cur, (void *)&rings->rs_id,
				sizeof rings->rs_id);
	return 0;
}

static int rings_resource_decode(struct m0_bufvec_cursor  *cur,
				 struct m0_rm_resource   **resource)
{
	static uint64_t  res_id;
	struct m0_rings *rings;

	m0_bufvec_cursor_copyfrom(cur, &res_id, sizeof res_id);

	M0_ALLOC_PTR(rings);
	if (rings == NULL)
		return -ENOMEM;

	rings->rs_id                      = res_id;
	rings->rs_resource.r_type         = &rings_resource_type;
	rings->rs_resource.r_type->rt_ops = &rings_rtype_ops;
	rings->rs_resource.r_ops          = &rings_ops;

	*resource = &rings->rs_resource;
	return 0;
}

const struct m0_rm_resource_type_ops rings_rtype_ops = {
	.rto_eq     = rings_resources_are_equal,
	.rto_is     = rings_resource_is,
	.rto_len    = rings_resource_len,
	.rto_encode = rings_resource_encode,
	.rto_decode = rings_resource_decode
};

static bool rings_credit_intersects(const struct m0_rm_credit *c0,
				    const struct m0_rm_credit *c1)
{
	if (c0->cr_datum == ANY_RING || c1->cr_datum == ANY_RING)
		return true;
	return (c0->cr_datum & c1->cr_datum) != 0;
}

static int rings_credit_join(struct m0_rm_credit       *c0,
			     const struct m0_rm_credit *c1)
{
	M0_ASSERT(c0->cr_datum != ANY_RING && c1->cr_datum != ANY_RING);
	c0->cr_datum |= c1->cr_datum;
	return 0;
}

static int rings_credit_diff(struct m0_rm_credit       *c0,
			     const struct m0_rm_credit *c1)
{
	M0_ASSERT(ergo(c1->cr_datum == ANY_RING, c0->cr_datum == ANY_RING));

	if (c0->cr_datum == ANY_RING)
		c0->cr_datum = (c1->cr_datum != 0) ? 0 : c0->cr_datum;
	else
		c0->cr_datum &= ~c1->cr_datum;
	return 0;
}

static void rings_credit_free(struct m0_rm_credit *credit)
{
	credit->cr_datum = 0;
}

static int rings_credit_encdec(struct m0_rm_credit *credit,
			       struct m0_bufvec_cursor *cur,
			       enum m0_xcode_what what)
{
	return m0_xcode_encdec(&M0_XCODE_OBJ(&M0_XT_U64, &credit->cr_datum),
			       cur, what);
}

static int rings_credit_encode(struct m0_rm_credit     *credit,
			       struct m0_bufvec_cursor *cur)
{
	return rings_credit_encdec(credit, cur, M0_XCODE_ENCODE);
}

static int rings_credit_decode(struct m0_rm_credit *credit,
			       struct m0_bufvec_cursor *cur)
{
	return rings_credit_encdec(credit, cur, M0_XCODE_DECODE);
}

static int rings_credit_copy(struct m0_rm_credit       *dest,
			     const struct m0_rm_credit *src)
{
	if (M0_FI_ENABLED("fail_copy"))
		return -ENOMEM;

	dest->cr_datum = src->cr_datum;
	dest->cr_owner = src->cr_owner;
	dest->cr_ops = src->cr_ops;
	return 0;
}

static void rings_initial_capital(struct m0_rm_credit *self)
{
	self->cr_datum = ALLRINGS;
}


static m0_bcount_t rings_credit_len(const struct m0_rm_credit *credit)
{
	struct m0_xcode_obj datumobj;
	struct m0_xcode_ctx ctx;

	datumobj.xo_type = &M0_XT_U64;
	datumobj.xo_ptr = (void *)&credit->cr_datum;
	m0_xcode_ctx_init(&ctx, &datumobj);
	return m0_xcode_length(&ctx);
}

static bool rings_is_subset(const struct m0_rm_credit *src,
			    const struct m0_rm_credit *dest)
{
	M0_ASSERT(dest->cr_datum != ANY_RING);
	if (src->cr_datum == ANY_RING)
		return dest->cr_datum != 0;
	return (dest->cr_datum & src->cr_datum) == src->cr_datum;
}

static int rings_disjoin(struct m0_rm_credit       *src,
			 const struct m0_rm_credit *dest,
			 struct m0_rm_credit       *intersection)
{
	M0_ASSERT(src->cr_datum != ANY_RING && dest->cr_datum != ANY_RING);

	intersection->cr_datum = src->cr_datum & dest->cr_datum;
	src->cr_datum &= ~intersection->cr_datum;
	return 0;
}

static bool rings_conflicts(const struct m0_rm_credit *c0,
			    const struct m0_rm_credit *c1)
{
	M0_ASSERT(c0->cr_datum != ANY_RING || c1->cr_datum != ANY_RING);

	if (c0->cr_datum == ANY_RING || c1->cr_datum == ANY_RING)
		return false;
	if (c0->cr_datum == SHARED_RING && c1->cr_datum == SHARED_RING)
		return false;

	return c0->cr_datum & c1->cr_datum;
}

const struct m0_rm_credit_ops rings_credit_ops = {
	.cro_intersects      = rings_credit_intersects,
	.cro_join            = rings_credit_join,
	.cro_diff            = rings_credit_diff,
	.cro_copy            = rings_credit_copy,
	.cro_free            = rings_credit_free,
	.cro_encode          = rings_credit_encode,
	.cro_decode          = rings_credit_decode,
	.cro_len             = rings_credit_len,
	.cro_is_subset       = rings_is_subset,
	.cro_disjoin         = rings_disjoin,
	.cro_conflicts       = rings_conflicts,
	.cro_initial_capital = rings_initial_capital,
};

static void rings_incoming_complete(struct m0_rm_incoming *in, int32_t rc)
{
	M0_PRE(in != NULL);
}

static void rings_incoming_conflict(struct m0_rm_incoming *in)
{
}

const struct m0_rm_incoming_ops rings_incoming_ops = {
	.rio_complete = rings_incoming_complete,
	.rio_conflict = rings_incoming_conflict
};

static void rings_rtype_set(struct rm_ut_data *self)
{
	struct m0_rm_resource_type *rings_rtype;
	int                         rc;

	M0_ALLOC_PTR(rings_rtype);
	M0_ASSERT(rings_rtype != NULL);
	rings_rtype->rt_id = 0;
	rings_rtype->rt_ops = &rings_rtype_ops;
	rc = m0_rm_type_register(&self->rd_dom, rings_rtype);
	M0_ASSERT(rc == 0);
	self->rd_rt = rings_rtype;
}

static void rings_rtype_unset(struct rm_ut_data *self)
{
	m0_rm_type_deregister(self->rd_rt);
	m0_free0(&self->rd_rt);
}

static void rings_res_set(struct rm_ut_data *self)
{
	struct m0_rings *rings_res;

	M0_ALLOC_PTR(rings_res);
	M0_ASSERT(rings_res != NULL);
	rings_res->rs_resource.r_ops = &rings_ops;
	m0_rm_resource_add(self->rd_rt, &rings_res->rs_resource);
	self->rd_res = &rings_res->rs_resource;
}

static void rings_res_unset(struct rm_ut_data *self)
{
	struct m0_rings *rings_res;

	m0_rm_resource_del(self->rd_res);
	rings_res = container_of(self->rd_res, struct m0_rings, rs_resource);
	m0_free(rings_res);
	self->rd_res = NULL;
}

static void rings_owner_set(struct rm_ut_data *self)
{
	struct m0_rm_owner *owner;
	struct m0_fid       fid = M0_FID_TINIT(M0_RM_OWNER_FT, 1,
					       (uint64_t)self);

	M0_ALLOC_PTR(owner);
	M0_ASSERT(owner != NULL);
	m0_rm_owner_init(owner, &fid, &m0_rm_no_group, self->rd_res, NULL);
	self->rd_owner = owner;
}

static void rings_owner_unset(struct rm_ut_data *self)
{
	m0_rm_owner_fini(self->rd_owner);
	m0_free0(&self->rd_owner);
}

static void rings_datum_set(struct rm_ut_data *self)
{
	self->rd_credit.cr_datum = ALLRINGS;
}

const static struct rm_ut_data_ops rings_ut_data_ops = {
	.rtype_set = rings_rtype_set,
	.rtype_unset = rings_rtype_unset,
	.resource_set = rings_res_set,
	.resource_unset = rings_res_unset,
	.owner_set = rings_owner_set,
	.owner_unset = rings_owner_unset,
	.credit_datum_set = rings_datum_set
};

void rings_utdata_ops_set(struct rm_ut_data *data)
{
	data->rd_ops = &rings_ut_data_ops;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
