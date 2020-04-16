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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 12/06/2011
 */

#pragma once

#ifndef __MERO_UT_CS_SERVICE_H__
#define __MERO_UT_CS_SERVICE_H__

#include "reqh/reqh_service.h"

extern struct m0_reqh_service_type ds1_service_type;
extern struct m0_reqh_service_type ds2_service_type;

extern struct m0_reqh_service_type *m0_cs_default_stypes[];
extern const size_t m0_cs_default_stypes_nr;

int m0_cs_default_stypes_init(void);
void m0_cs_default_stypes_fini(void);

#endif /* __MERO_UT_CS_SERVICE_H__ */
