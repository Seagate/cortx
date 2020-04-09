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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 11/21/2012
 */

#include "lib/memory.h"
#include "lib/misc.h"  /* SET0 */
#include "lib/errno.h"
#include "cob/ns_iter.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_COB
#include "lib/trace.h"
#
/**
 * @addtogroup cob_fid_ns_iter
 */

static bool ns_iter_invariant(const struct m0_cob_fid_ns_iter *iter)
{
	return iter != NULL &&
	       iter->cni_cdom != NULL && m0_fid_is_set(&iter->cni_last_fid);
}

M0_INTERNAL int m0_cob_ns_iter_init(struct m0_cob_fid_ns_iter *iter,
				    struct m0_fid *gfid,
				    struct m0_cob_domain *cdom)
{
	M0_PRE(iter != NULL);

	M0_SET0(iter);

	iter->cni_cdom     =  cdom;
	iter->cni_last_fid = *gfid;

	M0_POST(ns_iter_invariant(iter));

	return 0;
}

M0_INTERNAL int m0_cob_ns_rec_of(struct m0_be_btree *cob_namespace,
				 const struct m0_fid *key_gfid,
				 struct m0_fid *next_gfid,
				 struct m0_cob_nsrec **nsrec)
{
        struct m0_cob_nskey       *key = NULL;
        struct m0_buf              kbuf;
        struct m0_buf              nbuf;
        struct m0_be_btree_cursor  it;
	uint32_t                   cob_idx = 0;
        char                       nskey_bs[UINT32_STR_LEN];
        uint32_t                   nskey_bs_len;
	int                        rc;

        m0_be_btree_cursor_init(&it, cob_namespace);
	M0_SET0(&nskey_bs);
        snprintf(nskey_bs, UINT32_STR_LEN, "%u", cob_idx);
        nskey_bs_len = strlen(nskey_bs);

        rc = m0_cob_nskey_make(&key, key_gfid, nskey_bs,
			       nskey_bs_len);
        if (rc != 0)
                return M0_RC(rc);

	m0_buf_init(&kbuf, key, m0_cob_nskey_size(key) + UINT32_STR_LEN);
        rc = m0_be_btree_cursor_get_sync(&it, &kbuf, true);
	if (rc == 0) {
		/*
		 * Assign the fetched value to gfid, which is treated as
		 * iterator output.
		 */
		struct m0_cob_nskey* k;

		m0_be_btree_cursor_kv_get(&it, &kbuf, &nbuf);
		k = (struct m0_cob_nskey *)kbuf.b_addr;
		*nsrec = (struct m0_cob_nsrec *)nbuf.b_addr;
		*next_gfid = k->cnk_pfid;
	}
	m0_free(key);
        m0_be_btree_cursor_fini(&it);

	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_ns_iter_next(struct m0_cob_fid_ns_iter *iter,
				    struct m0_fid *gfid,
				    struct m0_cob_nsrec **nsrec)
{
	int           rc;
	struct m0_fid key_fid;

	M0_PRE(ns_iter_invariant(iter));
	M0_PRE(gfid != NULL);

	key_fid = iter->cni_last_fid;
	rc = m0_cob_ns_rec_of(&iter->cni_cdom->cd_namespace, &key_fid, gfid,
			      nsrec);
	if (rc == 0) {
		/* Container (f_container) value remains same, typically 0. */
		iter->cni_last_fid.f_container = gfid->f_container;
		/* Increment the f_key by 1, to exploit cursor_get() property.*/
		iter->cni_last_fid.f_key = gfid->f_key + 1;
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_cob_ns_iter_fini(struct m0_cob_fid_ns_iter *iter)
{
	M0_PRE(ns_iter_invariant(iter));
	M0_SET0(iter);
}

#undef M0_TRACE_SUBSYSTEM
/** @} end cob_fid_ns_iter */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
