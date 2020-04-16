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
 * Original creation date: 25/10/2011
 */

#pragma once

#ifndef __MERO_MERO_UT_CS_UT_FOP_FOMS_H__
#define __MERO_MERO_UT_CS_UT_FOP_FOMS_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

extern struct m0_fop_type cs_ds1_req_fop_fopt;
extern struct m0_fop_type cs_ds1_rep_fop_fopt;
extern struct m0_fop_type cs_ds2_req_fop_fopt;
extern struct m0_fop_type cs_ds2_rep_fop_fopt;

extern const struct m0_rpc_item_ops cs_ds_req_fop_rpc_item_ops;

/*
  Supported service types.
 */
enum {
        CS_UT_SERVICE1 = 1,
        CS_UT_SERVICE2,
};

/*
  Builds ds1 service fop types.
  Invoked from service specific stop function.
 */
int m0_cs_ut_ds1_fop_init(void);

/*
  Finalises ds1 service fop types.
  Invoked from service specific startup function.
 */
void m0_cs_ut_ds1_fop_fini(void);

/*
  Builds ds1 service fop types.
  Invoked from service specific stop function.
 */
int m0_cs_ut_ds2_fop_init(void);

/*
  Finalises ds1 service fop types.
  Invoked from service specific startup function.
 */
void m0_cs_ut_ds2_fop_fini(void);

struct m0_fom;

void m0_ut_fom_phase_set(struct m0_fom *fom, int phase);

/*
  Dummy fops to test mero setup
 */
struct cs_ds1_req_fop {
	uint64_t csr_value;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct cs_ds1_rep_fop {
	int32_t csr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct cs_ds2_req_fop {
	uint64_t csr_value;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct cs_ds2_rep_fop {
	int32_t csr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MERO_MERO_UT_CS_UT_FOP_FOMS_H__ */
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
