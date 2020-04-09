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
 * Original creation date: 19-Apr-2013
 */


#pragma once

#ifndef __MERO_DTM_FOL_H__
#define __MERO_DTM_FOL_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "dtm/history.h"
struct m0_dtm;
struct m0_dtm_remote;

/* export */
struct m0_dtm_fol;

struct m0_dtm_fol {
	struct m0_dtm_controlh fo_ch;
};

M0_INTERNAL void m0_dtm_fol_init(struct m0_dtm_fol *fol, struct m0_dtm *dtm);
M0_INTERNAL void m0_dtm_fol_fini(struct m0_dtm_fol *fol);
M0_INTERNAL void m0_dtm_fol_add(struct m0_dtm_fol *fol,
				struct m0_dtm_oper *oper);

M0_EXTERN const struct m0_dtm_history_type m0_dtm_fol_htype;

struct m0_dtm_fol_remote {
	struct m0_dtm_controlh rfo_ch;
};

M0_INTERNAL void m0_dtm_fol_remote_init(struct m0_dtm_fol_remote *frem,
					struct m0_dtm *dtm,
					struct m0_dtm_remote *remote);
M0_INTERNAL void m0_dtm_fol_remote_fini(struct m0_dtm_fol_remote *frem);
M0_INTERNAL void m0_dtm_fol_remote_add(struct m0_dtm_fol_remote *frem,
				       struct m0_dtm_oper *oper);

M0_EXTERN const struct m0_dtm_history_type m0_dtm_fol_remote_htype;

/** @} end of dtm group */

#endif /* __MERO_DTM_FOL_H__ */

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
