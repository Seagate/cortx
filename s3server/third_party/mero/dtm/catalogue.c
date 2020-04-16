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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 7-Apr-2013
 */


/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/errno.h"                 /* ENOENT */
#include "mero/magic.h"

#include "dtm/history.h"
#include "dtm/catalogue.h"

M0_TL_DESCR_DEFINE(cat, "catalogue", M0_INTERNAL,
		   struct m0_dtm_history, h_catlink,
		   h_hi.hi_ups.t_magic, M0_DTM_HI_MAGIX, M0_DTM_CAT_MAGIX);
M0_TL_DEFINE(cat, M0_INTERNAL, struct m0_dtm_history);

M0_INTERNAL void m0_dtm_catalogue_init(struct m0_dtm_catalogue *cat)
{
	cat_tlist_init(&cat->ca_el);
}

M0_INTERNAL void m0_dtm_catalogue_fini(struct m0_dtm_catalogue *cat)
{
	struct m0_dtm_history *history;

	m0_tl_for(cat, &cat->ca_el, history) {
		cat_tlist_del(history);
	} m0_tl_endfor;
	cat_tlist_fini(&cat->ca_el);
}

M0_INTERNAL int m0_dtm_catalogue_create(struct m0_dtm_catalogue *cat)
{
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_delete(struct m0_dtm_catalogue *cat)
{
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_lookup(struct m0_dtm_catalogue *cat,
					const struct m0_uint128 *id,
					struct m0_dtm_history **out)
{
	*out = m0_tl_find(cat, history, &cat->ca_el,
			  m0_uint128_eq(history->h_ops->hio_id(history), id));
	return *out != NULL ? 0 : -ENOENT;
}

M0_INTERNAL int m0_dtm_catalogue_add(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history)
{
	cat_tlist_add(&cat->ca_el, history);
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_del(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history)
{
	cat_tlist_del(history);
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_find(struct m0_dtm_catalogue *cat,
				      struct m0_dtm *dtm,
				      const struct m0_uint128 *id,
				      m0_dtm_catalogue_alloc_t *alloc,
				      void *datum,
				      struct m0_dtm_history **out)
{
	int result;

	result = m0_dtm_catalogue_lookup(cat, id, out);
	if (result == -ENOENT) {
		*out = alloc(dtm, id, datum);
		if (*out != NULL) {
			m0_dtm_catalogue_add(cat, *out);
			result = 0;
		} else
			result = M0_ERR(-ENOMEM);
	}
	return result;
}

/** @} end of dtm group */

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
