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
 * Original creation date: 08/29/2013
 */

#pragma once

#ifndef __MERO_SNS_CM_SERVICE_H__
#define __MERO_SNS_CM_SERVICE_H__

#include "cm/cm.h"
/**
  @defgroup SNSCMSVC SNS copy machine service
  @ingroup SNSCM

  @{
*/

/**
   State of current running repair or rebalance process. It is used for
   indicating. It is used to show the status of the operation to the Spiel
   %client.

   @dot
   digraph sns_service_states {
       node [shape=ellipse, fontsize=12];
       INIT [shape=point];
       IDLE [label="IDLE"];
       STARTED [label="STARTED"];
       PAUSED [label="PAUSED"];
       FAILED [label="FAILED"];
       INIT -> IDLE;
       IDLE -> STARTED [label="Start"];
       STARTED -> IDLE [label="Finish"];
       STARTED -> PAUSED [label="Pause"];
       PAUSED -> STARTED [label="Resume"];
       STARTED -> FAILED [label="Fail"];
       FAILED -> STARTED [label="Start again"];
   }
   @enddot

   @see m0_spiel_sns_repair_status
   @see m0_spiel_sns_rebalance_status
 */

/**
 * Allocates and initialises SNS copy machine.
 * This allocates struct m0_sns_cm and invokes m0_cm_init() to initialise
 * m0_sns_cm::rc_base.
 */
M0_INTERNAL int
m0_sns_cm_svc_allocate(struct m0_reqh_service **service,
		       const struct m0_reqh_service_type *stype,
		       const struct m0_reqh_service_ops *svc_ops,
		       const struct m0_cm_ops *cm_ops);

/**
 * Sets up copy machine corresponding to the given service.
 * Invokes m0_cm_setup().
 */
M0_INTERNAL int m0_sns_cm_svc_start(struct m0_reqh_service *service);

/**
 * Finalises copy machine corresponding to the given service.
 * Invokes m0_cm_fini().
 */
M0_INTERNAL void m0_sns_cm_svc_stop(struct m0_reqh_service *service);

/**
 * Destorys SNS copy machine (struct m0_sns_cm) correponding to the
 * given service.
 */
M0_INTERNAL void m0_sns_cm_svc_fini(struct m0_reqh_service *service);

/** @} SNSCMSVC */
#endif /* __MERO_SNS_CM_SERVICE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
