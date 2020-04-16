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
 * Original author: Rajesh Bhalerao <Rajesh_Bhalerao@xyratex.com>
 * Original creation date: 07/07/2011
 */

#pragma once

#ifndef __MERO_RM_FOMS_H__
#define __MERO_RM_FOMS_H__

#include "lib/chan.h"
#include "fop/fop.h"
#include "rm/rm.h"
#include "rm/rm_internal.h"
#include "rm/rm_ha.h"        /* m0_rm_ha_subscriber */

/**
 * @addtogroup rm
 *
 * This file includes data structures used by RM:fop layer.
 *
 * @{
 *
 */
enum m0_rm_fom_phases {
	FOPH_RM_REQ_START = M0_FOM_PHASE_INIT,
	FOPH_RM_REQ_FINISH = M0_FOM_PHASE_FINISH,
	FOPH_RM_REQ_CREDIT_GET,
	FOPH_RM_REQ_WAIT,
	/*
	 * Custom step required for asynchronous subscription to debtor death.
	 * So far, only borrow request needs this while subscribing debtor
	 * object to HA notifications.
	 */
	FOPH_RM_REQ_DEBTOR_SUBSCRIBE,
};

struct rm_request_fom {
	/** Generic m0_fom object */
	struct m0_fom                rf_fom;
	/** Incoming request */
	struct m0_rm_remote_incoming rf_in;
	/**
	 * Subscriber for remote failure notifications.
	 * Used if new remote is created to handle borrow request.
	 */
	struct m0_rm_ha_subscriber   rf_sbscr;
};

/** @} */

/* __MERO_RM_FOMS_H__ */
#endif

/**
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
