/*
 * COPYRIGHT 2015 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#pragma once

#ifndef __S3_SERVER_CLOVIS_HELPERS_H__
#define __S3_SERVER_CLOVIS_HELPERS_H__

#ifdef __cplusplus
#define EXTERN_C_BLOCK_BEGIN extern "C" {
#define EXTERN_C_BLOCK_END }
#define EXTERN_C_FUNC extern "C"
#else
#define EXTERN_C_BLOCK_BEGIN
#define EXTERN_C_BLOCK_END
#define EXTERN_C_FUNC
#endif

EXTERN_C_BLOCK_BEGIN

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <sys/stat.h>

#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "helpers/helpers.h"

/* For htonl */
#include <arpa/inet.h>

EXTERN_C_BLOCK_END

int init_clovis(void);
void fini_clovis(void);
int create_new_instance_id(struct m0_uint128 *ufid);
void teardown_clovis_op(struct m0_clovis_op *op);

#endif
