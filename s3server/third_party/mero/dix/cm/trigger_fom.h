/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 18-Aug-2016
 */

#pragma once

#ifndef __MERO_DIX_CM_TRIGGER_FOM_H__
#define __MERO_DIX_CM_TRIGGER_FOM_H__

/**
 * @defgroup DIXCM
 *
 * @{
 */

extern const struct m0_fom_type_ops m0_dix_trigger_fom_type_ops;

/** Finalises repair trigger fops. */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_fini(void);
/** Initialises repair trigger fops. */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_init(void);
/** Finalises re-balance trigger fops. */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_fini(void);
/** Initialises re-balance trigger fops. */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_init(void);

/** @} end of DIXCM group */
#endif /* __MERO_DIX_CM_TRIGGER_FOM_H__ */

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
