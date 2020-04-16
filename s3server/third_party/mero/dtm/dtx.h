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
 * Original creation date: 27-Jan-2013
 */


#pragma once

#ifndef __MERO_DTM_DTX_H__
#define __MERO_DTM_DTX_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "lib/types.h"                /* m0_uint128, uint32_t, uint64_t */
struct m0_dtm_dtx_party;
struct m0_dtm;

/* export */
struct m0_dtm_dtx;

struct m0_dtm_dtx {
	struct m0_dtm           *dt_dtm;
	struct m0_uint128        dt_id;
	uint32_t                 dt_nr;
	uint32_t                 dt_nr_max;
	uint32_t                 dt_nr_fixed;
	struct m0_dtm_dtx_party *dt_party;
};

struct m0_dtm_dtx_srv {
	struct m0_uint128     ds_id;
	struct m0_dtm_history ds_history;
};

M0_INTERNAL int m0_dtm_dtx_init(struct m0_dtm_dtx *dtx,
				const struct m0_uint128 *id,
				struct m0_dtm *dtm, uint32_t nr_max);
M0_INTERNAL void m0_dtm_dtx_fini(struct m0_dtm_dtx *dtx);

M0_INTERNAL void m0_dtm_dtx_add(struct m0_dtm_dtx *dtx,
				struct m0_dtm_oper *oper);
M0_INTERNAL void m0_dtm_dtx_close(struct m0_dtm_dtx *dtx);

M0_EXTERN const struct m0_dtm_history_type m0_dtm_dtx_htype;
M0_EXTERN const struct m0_dtm_history_type m0_dtm_dtx_srv_htype;

/** @} end of dtm group */

#endif /* __MERO_DTM_DTX_H__ */

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
