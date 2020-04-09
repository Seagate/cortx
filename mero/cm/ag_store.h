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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 10/09/2015
 */

#pragma once

#ifndef __MERO_CM_AG_STORE_H__
#define __MERO_CM_AG_STORE_H__

#include "cm/ag.h"
#include "fop/fom.h"

/**
   @defgroup CMSTORE copy machine store
   @ingroup CM

   @{
 */

enum m0_cm_ag_store_status {
	S_ACTIVE,
	S_COMPLETE,
	S_FINI
};

struct m0_cm_ag_store_data {
	struct m0_cm_ag_id d_in;
	struct m0_cm_ag_id d_out;
	m0_time_t          d_cm_epoch;
};

struct m0_cm_ag_store {
	struct m0_fom              s_fom;
	struct m0_cm_ag_store_data s_data;
	enum m0_cm_ag_store_status s_status;
};

M0_INTERNAL void m0_cm_ag_store_init(struct m0_cm_type *cmtype);
M0_INTERNAL void m0_cm_ag_store_fom_start(struct m0_cm *cm);
M0_INTERNAL void m0_cm_ag_store_complete(struct m0_cm_ag_store *store);
M0_INTERNAL void m0_cm_ag_store_fini(struct m0_cm_ag_store *store);
M0_INTERNAL bool m0_cm_ag_store_is_complete(struct m0_cm_ag_store *store);

/** @} CMSTORE */

#endif /* __MERO_CM_AG_STORE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
