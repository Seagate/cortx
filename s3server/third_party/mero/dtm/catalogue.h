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


#pragma once

#ifndef __MERO_DTM_CATALOGUE_H__
#define __MERO_DTM_CATALOGUE_H__


/**
 * @addtogroup dtm
 *
 * @{
 */
/* import */
#include "lib/tlist.h"
struct m0_uint128;
struct m0_dtm_history;
struct m0_dtm;
struct m0_uint128;

/* export */
struct m0_dtm_catalogue;

struct m0_dtm_catalogue {
	struct m0_tl ca_el;
};

M0_INTERNAL void m0_dtm_catalogue_init(struct m0_dtm_catalogue *cat);
M0_INTERNAL void m0_dtm_catalogue_fini(struct m0_dtm_catalogue *cat);
M0_INTERNAL int m0_dtm_catalogue_create(struct m0_dtm_catalogue *cat);
M0_INTERNAL int m0_dtm_catalogue_delete(struct m0_dtm_catalogue *cat);
M0_INTERNAL int m0_dtm_catalogue_lookup(struct m0_dtm_catalogue *cat,
					const struct m0_uint128 *id,
					struct m0_dtm_history **out);
M0_INTERNAL int m0_dtm_catalogue_add(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history);
M0_INTERNAL int m0_dtm_catalogue_del(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history);
typedef struct m0_dtm_history *
m0_dtm_catalogue_alloc_t(struct m0_dtm *, const struct m0_uint128 *, void *);

M0_INTERNAL int m0_dtm_catalogue_find(struct m0_dtm_catalogue *cat,
				      struct m0_dtm *dtm,
				      const struct m0_uint128 *id,
				      m0_dtm_catalogue_alloc_t *alloc,
				      void *datum,
				      struct m0_dtm_history **out);

/** @} end of dtm group */

#endif /* __MERO_DTM_CATALOGUE_H__ */


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
