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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 11-Feb-2014
 */

#include <stdio.h>
#include "be/alloc.h"
#include "be/domain.h"
#include "be/op.h"
#include "be/seg0.h"
#include "be/seg.h"

#include "lib/mutex.h"
#include "lib/buf.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

static bool be_0type_invariant(const struct m0_be_0type *zt)
{
	return zt->b0_name != NULL &&
		zt->b0_init != NULL &&
		zt->b0_fini != NULL;
}

static bool dom_is_locked(const struct m0_be_domain *dom)
{
	return m0_be_domain_is_locked(dom);
}

static void keyname_format(const struct m0_be_0type *zt, const char *suffix,
			   char *keyname, size_t keyname_len)
{
	snprintf(keyname, keyname_len, "%s%s", zt->b0_name, suffix);
}

static struct m0_be_seg *be_0type_seg0_get(struct m0_be_domain *dom)
{
	struct m0_be_seg *seg;

	seg = m0_be_domain_seg0_get(dom);
	M0_ASSERT_INFO(seg != NULL,
		       "BE domain should be mkfs'ed first. "
		       "See m0_be_domain_cfg::bc_mkfs_mode.");
	return seg;
}

void m0_be_0type_register(struct m0_be_domain *dom, struct m0_be_0type *zt)
{
	M0_PRE(be_0type_invariant(zt));

	m0_be_domain__0type_register(dom, zt);
}

void m0_be_0type_unregister(struct m0_be_domain *dom, struct m0_be_0type *zt)
{
	M0_PRE(be_0type_invariant(zt));

	m0_be_domain__0type_unregister(dom, zt);
}

int m0_be_0type_add(struct m0_be_0type  *zt,
		    struct m0_be_domain *dom,
		    struct m0_be_tx     *tx,
		    const char          *suffix,
		    const struct m0_buf *data)
{
	struct m0_be_seg *seg;
	struct m0_buf    *opt;
	char              keyname[256] = {};
	int               rc;

	M0_PRE(dom_is_locked(dom));
	M0_PRE(be_0type_invariant(zt));
	M0_PRE(m0_be_tx__is_exclusive(tx));
	// add PRE 0type registered

	seg = be_0type_seg0_get(dom);
	keyname_format(zt, suffix, keyname, ARRAY_SIZE(keyname));

	M0_BE_ALLOC_PTR_SYNC(opt, seg, tx);
	opt->b_nob = data->b_nob;
	M0_BE_ALLOC_BUF_SYNC(opt, seg, tx);
	memcpy(opt->b_addr, data->b_addr, data->b_nob);
	M0_BE_TX_CAPTURE_PTR(seg, tx, opt);
	M0_BE_TX_CAPTURE_BUF(seg, tx, opt);
	rc = m0_be_seg_dict_insert(seg, tx, keyname, (void*)opt);
	if (rc != 0)
		return M0_RC(rc);

	/* XXX error handling is missing here: what if b0_init() fails? */
	return zt->b0_init(dom, suffix, opt);
}

int m0_be_0type_del(struct m0_be_0type  *zt,
		    struct m0_be_domain *dom,
		    struct m0_be_tx     *tx,
		    const char          *suffix)
{
	struct m0_be_seg *seg;
	struct m0_buf    *opt;
	char              keyname[256] = {};
	int               rc;

	M0_PRE(dom_is_locked(dom));
	M0_PRE(be_0type_invariant(zt));
	M0_PRE(m0_be_tx__is_exclusive(tx));

	seg = be_0type_seg0_get(dom);
	keyname_format(zt, suffix, keyname, ARRAY_SIZE(keyname));

	rc = m0_be_seg_dict_lookup(seg, keyname, (void**)&opt);
	if (rc != 0)
		return M0_RC(rc); /* keyname is not found -- nothing to delete */

	zt->b0_fini(dom, suffix, opt);
	M0_BE_FREE_PTR_SYNC(opt->b_addr, seg, tx);
	M0_BE_FREE_PTR_SYNC(opt, seg, tx);
	return m0_be_seg_dict_delete(seg, tx, keyname);
}

void m0_be_0type_add_credit(struct m0_be_domain       *dom,
			    const struct m0_be_0type  *zt,
			    const char                *suffix,
			    const struct m0_buf       *data,
			    struct m0_be_tx_credit    *credit)
{
	struct m0_be_seg *seg = be_0type_seg0_get(dom);
	char		  keyname[256] = {};

	keyname_format(zt, suffix, keyname, ARRAY_SIZE(keyname));
	M0_BE_ALLOC_CREDIT_PTR(data, seg, credit);
	M0_BE_ALLOC_CREDIT_BUF(data, seg, credit);
	m0_be_tx_credit_add(credit, &M0_BE_TX_CREDIT_PTR(data));
	m0_be_tx_credit_add(credit, &M0_BE_TX_CREDIT_BUF(data));
	m0_be_seg_dict_insert_credit(seg, keyname, credit);
}

void m0_be_0type_del_credit(struct m0_be_domain       *dom,
			    const struct m0_be_0type  *zt,
			    const char                *suffix,
			    struct m0_be_tx_credit    *credit)
{
	struct m0_be_seg *seg = be_0type_seg0_get(dom);
	char              keyname[256] = {};

	keyname_format(zt, suffix, keyname, ARRAY_SIZE(keyname));
	/* to free data */
	m0_be_allocator_credit(m0_be_seg_allocator(seg), M0_BAO_FREE,
			       0, 0, credit);
	/* to free m0_buf pointing to data */
	m0_be_allocator_credit(m0_be_seg_allocator(seg), M0_BAO_FREE,
			       0, 0, credit);
	m0_be_seg_dict_delete_credit(seg, keyname, credit);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
