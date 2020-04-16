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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 09/25/2011
 */

#pragma once

#ifndef __MERO_CM_PUMP_H__
#define __MERO_CM_PUMP_H__

#include "fop/fom.h"

/**
   @addtogroup CM
 */

/* Import */
struct m0_cm;
struct m0_cm_type;

/**
 * Represents copy packet pump FOM. New copy packets are created in context
 * of cm_cp_pump::p_fom. The pump FOM (cm_cp_pump::p_fom) nicely resolves
 * the issues with creation of new copy packets and configuring them using
 * m0_cm_data_next(), which may block. The pump FOM is created when copy machine
 * operation starts and finalised when copy machine operation is complete.
 * The pump FOM goes to sleep when no more copy packets can be created (buffer
 * pool is exhausted). When a copy packet FOM terminates and frees its buffer
 * in the pool, it wakes up the pump FOM (using m0_cm_sw_fill()) to create more
 * copy packets.
 */
struct m0_cm_cp_pump {
	/** pump FOM. */
	struct m0_fom          p_fom;

	struct m0_fom_timeout  p_timeout;
	/**
	 * Every newly allocate Copy packet in CPP_ALLOC phase is saved for the
	 * further references, until the CPP_DATA_NEXT phase is completed for
	 * the copy packet. Pump FOM does not free this allocated copy packet,
	 * it is freed as part of copy packet FOM finalisation.
	 */
	struct m0_cm_cp       *p_cp;
	uint64_t               p_magix;
	struct m0_clink        p_complete;
	struct m0_sm_ast       p_wakeup;
};

M0_INTERNAL void m0_cm_cp_pump_init(struct m0_cm_type *cmtype);

M0_INTERNAL void m0_cm_cp_pump_prepare(struct m0_cm *cm);
/**
 * Initialises pump FOM and submits it to reqh for processing.
 * This is invoked from m0_cm_start()
 */
M0_INTERNAL void m0_cm_cp_pump_start(struct m0_cm *cm);
M0_INTERNAL void m0_cm_cp_pump_destroy(struct m0_cm *cm);
/**
 * Wakes up pump FOM to create more copy packets.
 */
M0_INTERNAL void m0_cm_cp_pump_wakeup(struct m0_cm *cm);

M0_INTERNAL bool m0_cm_cp_pump_is_complete(const struct m0_cm_cp_pump *cp_pump);
/** @} endgroup CM */

/* __MERO_CM_PUMP_H__ */

#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
