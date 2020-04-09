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
 * Original creation date: 06/07/2013
 */

#pragma once

#ifndef __MERO_CM_REPREB_SW_ONWIRE_FOM_H__
#define __MERO_CM_REPREB_SW_ONWIRE_FOM_H__

/**
   @addtogroup XXX

   @{
 */

enum cm_repreb_sw_phases {
	SWOPH_START = M0_FOM_PHASE_INIT,
	SWOPH_FINI = M0_FOM_PHASE_FINISH,
};

M0_INTERNAL int m0_cm_repreb_sw_onwire_fom_create(struct m0_fop *fop,
						  struct m0_fop *r_fop,
						  struct m0_fom **out,
						  struct m0_reqh *reqh);

/** @} XXX */

#endif /* __MERO_CM_REPREB_SW_ONWIRE_FOM_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
