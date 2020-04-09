/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Yuriy Umanets <yuriy.umanets@seagate.com>
 * Original creation date: 18-Jan-2015
 */


/**
 * @addtogroup layout_conf
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/tlist.h"  /* struct m0_tl */
#include "lib/vec.h"    /* m0_bufvec_cursor_step(), m0_bufvec_cursor_addr() */
#include "lib/memory.h" /* M0_ALLOC_PTR() */
#include "lib/misc.h"   /* M0_IN() */
#include "lib/bob.h"
#include "lib/finject.h"

#include "mero/magic.h"
#include "fid/fid.h"    /* m0_fid_set(), m0_fid_is_valid() */
#include "pool/pool.h"
#include "layout/linear_enum.h"
#include "layout/pdclust.h"
#include "layout/layout_internal.h" /* M0_PDCLUST_SEED */

static int layout_enum_build(struct m0_layout_domain *dom,
                             const uint32_t pool_width,
                             struct m0_layout_enum **lay_enum)
{
        struct m0_layout_linear_attr  lin_attr;
        struct m0_layout_linear_enum *lle;
        int                           rc;

        M0_ENTRY();
        M0_PRE(pool_width > 0 && lay_enum != NULL);
        /*
         * cob_fid = fid { B * idx + A, gob_fid.key }
         * where idx is in [0, pool_width)
         */
        lin_attr = (struct m0_layout_linear_attr){
                .lla_nr = pool_width,
                .lla_A  = 1,
                .lla_B  = 1
        };

        *lay_enum = NULL;
        rc = m0_linear_enum_build(dom, &lin_attr, &lle);
        if (rc == 0)
                *lay_enum = &lle->lle_base;
        return M0_RC(rc);
}


static int __layout_build(struct m0_layout_domain *dom,
                          const uint64_t layout_id,
                          struct m0_pool_version *pv,
                          struct m0_layout_enum *le,
                          struct m0_layout **layout)
{
        struct m0_pdclust_layout *pdlayout = NULL;
        int                       rc;

        M0_ENTRY();
        M0_PRE(pv->pv_attr.pa_P > 0);
        M0_PRE(le != NULL && layout != NULL);

        *layout = NULL;
        rc = m0_pdclust_build(dom, layout_id, &pv->pv_attr, le, &pdlayout);
        if (rc == 0) {
                *layout = m0_pdl_to_layout(pdlayout);
		(*layout)->l_pver = pv;
	}

        return M0_RC(rc);
}

int m0_lid_to_unit_map[] = {
       [ 0] =       -1, /* invalid */
       [ 1] =     4096,
       [ 2] =     8192,
       [ 3] =    16384,
       [ 4] =    32768,
       [ 5] =    65536,
       [ 6] =   131072,
       [ 7] =   262144,
       [ 8] =   524288,
       [ 9] =  1048576,
       [10] =  2097152,
       [11] =  4194304,
       [12] =  8388608,
       [13] = 16777216,
       [14] = 33554432,
};
const int m0_lid_to_unit_map_nr = ARRAY_SIZE(m0_lid_to_unit_map);

M0_INTERNAL int m0_layout_init_by_pver(struct m0_layout_domain *dom,
                                       struct m0_pool_version *pv,
                                       int *count)
{
        struct m0_pdclust_attr *pa = &pv->pv_attr;
        struct m0_layout_enum  *layout_enum;
        uint64_t 		layout_id;
        struct m0_layout       *layout;
        int 			rc;
        int 			i;

        M0_ENTRY();
	for (i = M0_DEFAULT_LAYOUT_ID; i < m0_lid_to_unit_map_nr; ++i) {
		/* Use current unit size. */
		pa->pa_unit_size = m0_lid_to_unit_map[i];
		m0_uint128_init(&pa->pa_seed, M0_PDCLUST_SEED);

		layout_id = m0_pool_version2layout_id(&pv->pv_id, i);

		rc = layout_enum_build(dom, pa->pa_P, &layout_enum);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "layout %"PRIu64" enum build failed: rc=%d",
			       layout_id, rc);
			return M0_RC(rc);
		}

		/**
		   At this point layout has also added to the list in domain.
		 */
		rc = __layout_build(dom, layout_id, pv, layout_enum, &layout);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "layout %"PRIu64" build failed: rc=%d",
			       layout_id, rc);
			m0_layout_enum_fini(layout_enum);
			return M0_RC(rc);
		}
		if (count != NULL)
			(*count)++;
	}
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of layout_conf group */

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
