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
 * Original author: Subhash Arya <subhash_arya@xyratex.com>
 * Original creation date: 09/25/2012
 * Restructured by: Rohan Puri <Rohan_Puri@xyratex.com>
 * Restructured Date: 12/13/2012
 */

#pragma once
#ifndef __MERO_CM_UT_COMMON_SERVICE_H__
#define __MERO_CM_UT_COMMON_SERVICE_H__

#include "cm/cm.h"
#include "cm/cp.h"
#include "cm/ag.h"
#include "lib/misc.h"
#include "lib/finject.h"
#include "lib/memory.h"
#include "ut/ut.h"
#include "lib/chan.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "mero/setup.h"
#include "ut/ut_rpc_machine.h"

#define DUMMY_DBNAME      "dummy-db"
#define DUMMY_COB_ID      20
#define DUMMY_SERVER_ADDR "0@lo:12345:34:10"

extern struct m0_cm_cp            cm_ut_cp;
extern struct m0_reqh_service    *cm_ut_service;
extern struct m0_ut_rpc_mach_ctx  cmut_rmach_ctx;

enum {
	AG_ID_NR = 100,
	CM_UT_LOCAL_CP_NR = 4,
	MAX_CM_NR = 2
};

struct m0_ut_cm {
	uint64_t        ut_cm_id;
	struct m0_cm    ut_cm;
	struct m0_chan  ut_cm_wait;
	struct m0_mutex ut_cm_wait_mutex;
};

extern struct m0_reqh           cm_ut_reqh;
extern struct m0_cm_cp          cm_ut_cp;
extern struct m0_ut_cm          cm_ut[MAX_CM_NR];
extern struct m0_reqh_service  *cm_ut_service;
extern struct m0_mutex          cm_wait_mutex;
extern struct m0_chan           cm_wait;

extern struct m0_cm_type                 cm_ut_cmt;
extern const struct m0_cm_aggr_group_ops cm_ag_ut_ops;
extern uint64_t                          ut_cm_id;
extern bool                              test_ready_fop;

void cm_ut_service_alloc_init(struct m0_reqh *reqh);
void cm_ut_service_cleanup();

#endif /** __MERO_CM_UT_COMMON_SERVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
